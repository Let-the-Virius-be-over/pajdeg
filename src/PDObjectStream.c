//
//  PDObjectStream.c
//
//  Copyright (c) 2013 Karl-Johan Alm (http://github.com/kallewoof)
// 
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
// 
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
// 
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//

#include "Pajdeg.h"

#include "PDOperator.h"
#include "PDInternal.h"
#include "pd_stack.h"
#include "PDScanner.h"
#include "pd_pdf_implementation.h"
#include "pd_pdf_private.h"
#include "PDStreamFilter.h"
#include "PDObjectStream.h"
#include "pd_btree.h"

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
            pd_stack_destroy(elements[i].def);
    free(elements);
    PDRelease(obstm->filter);
    if (obstm->constructs) {
        pd_btree_destroy_with_deallocator(obstm->constructs, PDRelease);
    }
    PDRelease(obstm->ob);
}

PDObjectStreamRef PDObjectStreamCreateWithObject(PDObjectRef object)
{
    PDObjectStreamRef obstm = PDAlloc(sizeof(struct PDObjectStream), PDObjectStreamDestroy, false);
    obstm->ob = PDRetain(object);
    obstm->n = PDIntegerFromString(PDObjectGetDictionaryEntry(object, "N"));
    obstm->first = PDIntegerFromString(PDObjectGetDictionaryEntry(object, "First"));
    
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
    obstm->constructs = NULL;
    
    return obstm;
}

void PDObjectStreamParseRawObjectStream(PDObjectStreamRef obstm, char *rawBuf)
{
    char *extractedBuf;
    PDInteger len;

    extractedBuf = NULL;
    len = obstm->ob->streamLen;
    if (obstm->filter) {
        if (! PDStreamFilterApply(obstm->filter, (unsigned char *)rawBuf, (unsigned char **)&extractedBuf, len, &len)) {
            PDWarn("PDStreamFilterApply() failed.");
            PDAssert(0);
        }
        PDAssert(extractedBuf);
        rawBuf = extractedBuf;
        obstm->ob->streamBuf = extractedBuf;
        obstm->ob->extractedLen = len;
    }

    PDObjectStreamParseExtractedObjectStream(obstm, rawBuf);
}

void PDObjectStreamParseExtractedObjectStream(PDObjectStreamRef obstm, char *rawBuf)
{
    PDInteger i, n, len;
    n = obstm->n;
    
    PDObjectStreamElementRef el;
    PDObjectStreamElementRef elements = obstm->elements = malloc(sizeof(struct PDObjectStreamElement) * n);
    
    len = obstm->ob->extractedLen;
    
    PDScannerRef osScanner = PDScannerCreateWithState(arbStream);
    osScanner->buf = rawBuf;
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
    PDAssert(abs(obstm->first - osScanner->boffset) < 2);
    
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
    
    PDObjectRef ob = pd_btree_fetch(obstm->constructs, obid);
    if (ob) return ob;

    n = obstm->n;
    elements = obstm->elements;
    for (i = 0; i < n; i++) {
        if (elements[i].obid == obid) {
            ob = PDObjectCreate(obid, 0);
            ob->obclass = PDObjectClassCompressed;
            ob->def = elements[i].def;
            ob->type = elements[i].type;
            elements[i].def = NULL;
            pd_btree_insert(&obstm->constructs, obid, ob);
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
        ob->obclass = PDObjectClassCompressed;
        ob->def = elements[index].def;
        ob->type = elements[index].type;
        elements[index].def = NULL;
        pd_btree_insert(&obstm->constructs, elements[index].obid, ob);
        return ob;
    }
    
    return pd_btree_fetch(obstm->constructs, elements[index].obid);
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
            PDObjectRef ob = pd_btree_fetch(obstm->constructs, elements[i].obid);
            len = PDObjectGenerateDefinition(ob, (char**)&elements[i].def, 0);
            len--; // objects add \n after def; don't want two \n's
        } else {
            pd_stack def = elements[i].def;
            elements[i].def = PDStringFromComplex(&def);
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
    PDObjectSetDictionaryEntry(streamOb, "First", strdup(hbuf));
    
    // generate stream
    len = headerlen + offs;
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
        if (! PDStreamFilterApply(inversionFilter, (unsigned char *)content, (unsigned char **)&filteredBuf, len, &len)) {
            PDWarn("PDStreamFilterApply failed!\n");
            PDAssert(0);
        }
        PDRelease(inversionFilter);
        free(content);
        content = filteredBuf;
    }
    
    // update object stream
    PDObjectSetStream(streamOb, content, len, true);
}
