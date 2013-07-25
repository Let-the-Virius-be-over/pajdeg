//
//  PDObject.c
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

#include "PDInternal.h"
#include "PDStack.h"
#include "PDScanner.h"
#include "PDPortableDocumentFormatState.h"
#include "PDPDFPrivate.h"

PDObjectRef PDObjectCreate(int obid, int genid)
{
    PDObjectRef ob = calloc(1, sizeof(struct PDObject));
    ob->users = 1;
    ob->obid = obid;
    ob->genid = genid;
    ob->type = PDObjectTypeUnknown;
    return ob;
}

PDObjectRef PDObjectRetain(PDObjectRef object)
{
    object->users++;
    return object;
}

void PDObjectRelease(PDObjectRef object)
{
    object->users--;
    if (object->users == 0) {
        // todo: destroy def
        PDStackDestroy(object->mutations);
        if (object->type == PDObjectTypeDictionary)
            PDStackDestroy(object->def);
        free(object);
    }
}

int PDObjectGetObID(PDObjectRef object)
{
    return object->obid;
}

int PDObjectGetGenID(PDObjectRef object)
{
    return object->genid;
}

PDObjectType PDObjectGetType(PDObjectRef object)
{
    return object->type;
}

PDBool PDObjectHasStream(PDObjectRef object)
{
    return object->hasStream;
}

int PDObjectGetStreamLength(PDObjectRef object)
{
    return object->streamLen;
}

char *PDObjectGetValue(PDObjectRef object)
{
    return object->def;
}

PDStackRef lastKeyContainer;
const char *PDObjectGetDictionaryEntry(PDObjectRef object, const char *key)
{
    // check mutations dict if we have this one already
    PDStackRef entry, field;
    for (entry = object->mutations; entry; entry = entry->prev->prev) {
        if (!strcmp((char*)entry->info, key)) {
            lastKeyContainer = entry;
            return entry->prev->info;
        }
    }
    
    char *value;
    entry = PDStackGetDictKey(object->def, key, true);
    if (! entry) return NULL;
    
    // entry = e, ID, field
    PDStackAssertExpectedKey(&entry, PD_DE);
    PDStackAssertExpectedKey(&entry, key);
    // so we see if type is primitive or not
    if (entry->type == PDSTACK_STACK) {
        // it's not primitive, so we set the preserve flag and stringify
        field = PDStackPopStack(&entry);
        value = PDStringFromComplex(&field);
    } else {
        // it is primitive (we presume)
        PDAssert(entry->type == PDSTACK_STRING);
        value = PDStackPopKey(&entry);
    }
    
    // add this to mutations, as we keep everything simplified (and STRINGified) there
    PDStackPushKey(&object->mutations, value);
    PDStackPushKey(&object->mutations, strdup(key));
    lastKeyContainer = object->mutations;

    // destroy entry, as it's been detached since we removed it
    PDStackDestroy(entry);
    
    return value;
}

void PDObjectSetDictionaryEntry(PDObjectRef object, const char *key, const char *value)
{
    object->type = PDObjectTypeDictionary;
    
    // see if we have it (user is responsible for nulling lastKeyContainer)
    lastKeyContainer = NULL;
    PDObjectGetDictionaryEntry(object, key);
    if (lastKeyContainer) {
        free(lastKeyContainer->prev->info);
        lastKeyContainer->prev->info = strdup(value);
        return;
    }
    
#if 0
    PDStackRef entry;
    for (entry = object->mutations; entry; entry = entry->prev->prev) {
        if (!strcmp((char*)entry->info, key)) {
            // we do, so replace the value
            entry = entry->prev;
            free(entry->info);
            entry->info = strdup(value);
            return;
        }
    }
#endif
    
    // we didn't so we add
    PDStackPushKey(&object->mutations, strdup(value));
    PDStackPushKey(&object->mutations, strdup(key));

}

void PDObjectRemoveDictionaryEntry(PDObjectRef object, const char *key)
{
    PDStackRef ks = PDStackGetDictKey(object->def, key, true);
    PDStackDestroy(ks);
}

void PDObjectReplaceWithString(PDObjectRef object, char *str, int len)
{
    object->ovrDef = str;
    object->ovrDefLen = len;
}

void PDObjectSkipStream(PDObjectRef object)
{
    object->skipStream = true;
}

void PDObjectSetStream(PDObjectRef object, const char *str, int len, PDBool includeLength)
{
    object->ovrStream = str;
    object->ovrStreamLen = len;
    if (includeLength)  {
        char *lenstr = malloc(30);
        sprintf(lenstr, "%d", len);
        PDObjectSetDictionaryEntry(object, "Length", lenstr);
        free(lenstr);
    }
}

void PDObjectSetEncryptedStreamFlag(PDObjectRef object, PDBool encrypted)
{
    if (object->encryptedDoc) {
        if (encrypted) {
            // we don't support encryption except in that we undo our no-encryption stuff below
            PDObjectRemoveDictionaryEntry(object, "Filter");
            PDObjectRemoveDictionaryEntry(object, "DecodeParms");
        } else {
            PDObjectSetDictionaryEntry(object, "Filter", "[/Crypt]");
            PDObjectSetDictionaryEntry(object, "DecodeParms", "<</Type /CryptFilterDecodeParms /Name /Identity>>");
        }
    }
}

// PDPortableDocumentFormatState.c

extern void PDStringFromMeta(PDStackRef *s, PDStringConvRef scv);
extern void PDStringFromObj(PDStackRef *s, PDStringConvRef scv);
extern void PDStringFromRef(PDStackRef *s, PDStringConvRef scv);
extern void PDStringFromHexString(PDStackRef *s, PDStringConvRef scv);
extern void PDStringFromDict(PDStackRef *s, PDStringConvRef scv);
extern void PDStringFromDictEntry(PDStackRef *s, PDStringConvRef scv);
extern void PDStringFromArray(PDStackRef *s, PDStringConvRef scv);
extern void PDStringFromArrayEntry(PDStackRef *s, PDStringConvRef scv);
extern void PDStringFromArbitrary(PDStackRef *s, PDStringConvRef scv);

int PDObjectGenerateDefinition(PDObjectRef object, char **dstBuf, int capacity)
{
    struct PDStringConv sscv = (struct PDStringConv) {*dstBuf, 0, capacity};
    PDStringConvRef scv = &sscv;
    
    // we don't even want to start off with < 50 b
    PDStringGrow(50);
    
    // generate
    //PDStringFromArbitrary(complex, &scv);
    
    // null terminate
    //if (scv.left < 1) scv.allocBuf = realloc(scv.allocBuf, scv.offs + 1);
    //scv.allocBuf[scv.offs] = 0;
    
    PDStackRef stack;
    char *key, *val;
    int sz;
    putfmt("%d %d obj\n", object->obid, object->genid);
    switch (object->type) {
        case PDObjectTypeDictionary:
            PDStringGrow(20);
            putstr("<<", 2);
            // write out mutations first
            for (stack = object->mutations; stack; stack = stack->prev->prev) {
                PDStringGrow(13 + strlen(stack->info) + strlen(stack->prev->info));
                putfmt("/%s %s ", (char*)stack->info, (char*)stack->prev->info);
            }
            // then write out unmodified entries
            stack = object->def;
            while (PDStackGetNextDictKey(&stack, &key, &val)) {
                PDStringGrow(3 + strlen(key) + strlen(val));
                putfmt("/%s %s ", key, val);
                free(val);
            }
            PDStringGrow(10);
            putstr(">>\n", 2);
            break;
            
        default:
            putfmt("%s\n", (char*)object->def);
            break;
    }
    
    PDStringGrow(2);
    currchi = '\n';
    currch = 0;
    
    *dstBuf = scv->allocBuf;

    return scv->offs;
}



