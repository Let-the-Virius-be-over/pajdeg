//
// PDObjectStream.c
//
// Copyright (c) 2012 - 2014 Karl-Johan Alm (http://github.com/kallewoof)
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "Pajdeg.h"

#include "PDOperator.h"
#include "pd_internal.h"
#include "pd_stack.h"
#include "PDScanner.h"
#include "pd_pdf_implementation.h"
#include "pd_pdf_private.h"
#include "PDStreamFilter.h"
#include "PDObjectStream.h"
#include "PDBTree.h"

#include "pd_crypto.h" // temporary!

/*struct PDObjectStream {
    PDObjectRef ob;                     // obstream object
    PDInteger n;                        // number of objects
    PDInteger first;                    // first object's offset
    PDStreamFilterRef filter;           // filter used to extract the initial raw content
    PDObjectStreamElementRef elements;  // n sized array of elements (non-pointered!)
};*/

void PDObjectStreamDestroy(PDObjectStreamRef obstm)
{
    PDInteger i;
    PDObjectStreamElementRef elements = obstm->elements;
    for (i = 0; i < obstm->n; i++)
        if (elements[i].type == PDObjectTypeString)
            free(elements[i].def);
        else
            pd_stack_destroy((pd_stack *)&elements[i].def);
    free(elements);
    PDRelease(obstm->filter);
    if (obstm->constructs) {
        PDRelease(obstm->constructs);
        //pd_btree_destroy_with_deallocator(obstm->constructs, PDRelease);
    }
    PDRelease(obstm->ob);
}

PDObjectStreamRef PDObjectStreamCreateWithObject(PDObjectRef object)
{
    PDObjectStreamRef obstm = PDAlloc(sizeof(struct PDObjectStream), PDObjectStreamDestroy, false);
    obstm->ob = PDRetain(object);
    obstm->n = PDIntegerFromString(PDObjectGetDictionaryEntry(object, "N"));
    obstm->first = PDIntegerFromString(PDObjectGetDictionaryEntry(object, "First"));
    obstm->constructs = PDBTreeCreate(PDRelease, obstm->first, obstm->first + obstm->n, obstm->n/3);
    
    const char *filterName = PDObjectGetDictionaryEntry(object, "Filter");
    if (filterName) {
        filterName = &filterName[1]; // get rid of name slash
        pd_stack decodeParms = pd_stack_get_dict_key(object->def, "DecodeParms", false);
        if (decodeParms) 
            decodeParms = PDStreamFilterGenerateOptionsFromDictionaryStack(decodeParms);
        obstm->filter = PDStreamFilterObtain(filterName, true, decodeParms);
    } else {
        obstm->filter = NULL;
    }
    
    obstm->elements = NULL;
    
    return obstm;
}

PDBool PDObjectStreamParseRawObjectStream(PDObjectStreamRef obstm, char *rawBuf)
{
    char *extractedBuf;
    PDInteger len;

    extractedBuf = NULL;
    len = obstm->ob->streamLen;
    if (obstm->filter) {
        if (! PDStreamFilterApply(obstm->filter, (unsigned char *)rawBuf, (unsigned char **)&extractedBuf, len, &len, NULL)) {
            PDError("PDStreamFilterApply() failed.");
            obstm->ob->streamBuf = NULL;
            obstm->ob->extractedLen = 0;
            obstm->n = 0;
            return false;
        } 
        
        PDAssert(extractedBuf);
        rawBuf = extractedBuf;
        obstm->ob->streamBuf = extractedBuf;
        obstm->ob->extractedLen = len;
    }

    PDObjectStreamParseExtractedObjectStream(obstm, rawBuf);
    return true;
}

void PDObjectStreamParseExtractedObjectStream(PDObjectStreamRef obstm, char *buf)
{
    PDInteger i, n, len;
    n = obstm->n;
    
    PDObjectStreamElementRef el;
    PDObjectStreamElementRef elements = obstm->elements = malloc(sizeof(struct PDObjectStreamElement) * n);
    
    len = obstm->ob->extractedLen;
    
    PDScannerRef osScanner = PDScannerCreateWithState(arbStream);
    osScanner->buf = buf;
    osScanner->fixedBuf = true;
    osScanner->boffset = 0;
    osScanner->bsize = len;
    
    // header (obid offset * n)
    for (i = 0; i < n; i++) {
        el = &elements[i];
        el->obid = PDIntegerFromString(&osScanner->buf[osScanner->boffset]);
        PDScannerPassSymbolCharacterType(osScanner, PDOperatorSymbolGlobWhitespace);
        el->offset = PDIntegerFromString(&osScanner->buf[osScanner->boffset]);
        PDScannerPassSymbolCharacterType(osScanner, PDOperatorSymbolGlobWhitespace);
    }
    
    // we should now be at the first object's definition, but we can't presume whitespace will be exact so we += 1 byte
    PDAssert(labs(obstm->first - osScanner->boffset) < 2);
    
    // read definitions 
    for (i = 0; i < n; i++) {
        if (PDScannerPopStack(osScanner, (pd_stack *)&elements[i].def)) {
            elements[i].type = PDObjectTypeFromIdentifier(as(pd_stack, elements[i].def)->info);
        } else {
            elements[i].type = PDObjectTypeString;
            PDScannerPopString(osScanner, (char **)&elements[i].def);
        }
    }
    
    PDRelease(osScanner);
}

PDObjectRef PDObjectStreamGetObjectByID(PDObjectStreamRef obstm, PDInteger obid)
{
    PDInteger i, n;
    PDObjectStreamElementRef elements;
    
    PDObjectRef ob = PDBTreeGet(obstm->constructs, obid);
    //pd_btree_fetch(obstm->constructs, obid);
    if (ob) return ob;

    n = obstm->n;
    elements = obstm->elements;
    for (i = 0; i < n; i++) {
        if (elements[i].obid == obid) {
            ob = PDObjectCreate(obid, 0);
            ob->crypto = obstm->ob->crypto;
            ob->obclass = PDObjectClassCompressed;
            ob->def = elements[i].def;
            ob->type = elements[i].type;
            elements[i].def = NULL;
            PDBTreeInsert(obstm->constructs, obid, ob);
            return ob;
        }
    }
    
    return NULL;
}

PDObjectRef PDObjectStreamGetObjectAtIndex(PDObjectStreamRef obstm, PDInteger index)
{
    PDObjectStreamElementRef elements;
    
    elements = obstm->elements;
    PDAssert(obstm->n > index);
    PDAssert(index > -1);
    
    if (elements[index].def) {
        PDObjectRef ob = PDObjectCreate(elements[index].obid, 0);
        ob->crypto = obstm->ob->crypto;
        ob->obclass = PDObjectClassCompressed;
        ob->def = elements[index].def;
        ob->type = elements[index].type;
        elements[index].def = NULL;
        PDBTreeInsert(obstm->constructs, elements[index].obid, ob);
        //pd_btree_insert(&obstm->constructs, elements[index].obid, ob);
        return ob;
    }
    
    return PDBTreeGet(obstm->constructs, elements[index].obid);
    //pd_btree_fetch(obstm->constructs, elements[index].obid);
}

void PDObjectStreamCommit(PDObjectStreamRef obstm)
{
    PDInteger i;
    PDInteger n;
    PDInteger len;
    PDInteger headerlen;
    PDInteger offs;
    PDObjectRef streamOb;
    PDObjectStreamElementRef elements;
    char hbuf[64];
    char *content;
    
    if (obstm->constructs == NULL) return;
    
    streamOb = obstm->ob;
    elements = obstm->elements;
    n = obstm->n;
    offs = 0;
    headerlen = 0;
    
    // stringify and update offsets
    for (i = 0; i < n; i++) {
        if (elements[i].def == NULL) {
            PDObjectRef ob = PDBTreeGet(obstm->constructs, elements[i].obid);
            //pd_btree_fetch(obstm->constructs, elements[i].obid);
            len = PDObjectGenerateDefinition(ob, (char**)&elements[i].def, 0);
            len--; // objects add \n after def; don't want two \n's
        } else {
            if (PDObjectTypeString != elements[i].type) {
                pd_stack def = elements[i].def;
                elements[i].def = PDStringFromComplex(&def);
            }
            len = strlen(elements[i].def);
        }
        len++; // add a \n after every def
        elements[i].offset = offs;
        elements[i].length = len;
        headerlen += sprintf(hbuf, "%ld %ld ", elements[i].obid, offs);
        offs += len;
    }
    
    // update keys
    sprintf(hbuf, "%ld", headerlen);
    PDObjectSetDictionaryEntry(streamOb, "First", hbuf);
    
    // generate stream
    len = headerlen + offs;
    if (len == 0) return; // this will never happen (in theory), but this line gets rid of CLANG warnings
    content = malloc(len);
    
    // header
    offs = 0;
    for (i = 0; i < n; i++) {
        offs += sprintf(&content[offs], "%ld %ld ", elements[i].obid, elements[i].offset);
    }
    
    // change final space to a newline to be cleanly
    PDAssert(offs > 0);
    content[offs-1] = '\n';
    
    // content
    for (i = 0; i < n; i++) {
        memcpy(&content[offs], elements[i].def, elements[i].length);
        offs += elements[i].length;
        content[offs-1] = '\n';
        PDAssert(offs <= len);
    }
    
    PDAssert(offs == len);
    
    // filter (if necessary)
    if (obstm->filter) {
        char *filteredBuf;
        PDStreamFilterRef inversionFilter = PDStreamFilterCreateInversionForFilter(obstm->filter);
        if (! PDStreamFilterApply(inversionFilter, (unsigned char *)content, (unsigned char **)&filteredBuf, len, &len, NULL)) {
            PDWarn("PDStreamFilterApply failed!\n");
            PDAssert(0);
        }
        PDRelease(inversionFilter);
        free(content);
        content = filteredBuf;
    }
    
    // update object stream
    PDObjectSetStream(streamOb, content, len, true, true);
}
