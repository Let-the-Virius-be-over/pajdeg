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

PDObjectRef PDObjectCreate(PDInteger obid, PDInteger genid)
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
        PDStackDestroy(object->mutations);
        if (object->type == PDObjectTypeString)
            free(object->def);
        else
            PDStackDestroy(object->def);
        free(object);
    }
}

PDInteger PDObjectGetObID(PDObjectRef object)
{
    return object->obid;
}

PDInteger PDObjectGetGenID(PDObjectRef object)
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

PDInteger PDObjectGetStreamLength(PDObjectRef object)
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
    // check mutations dict as well
    PDStackRef entry, prev;
    prev = NULL;
    for (entry = object->mutations; entry; entry = entry->prev->prev) {
        if (!strcmp((char*)entry->info, key)) {
            if (prev) {
                prev->prev->prev = entry->prev->prev;
                entry->prev->prev = NULL;
                PDStackDestroy(entry);
            } else {
                prev = object->mutations;
                object->mutations = entry->prev->prev;
                prev->prev->prev = NULL;
                PDStackDestroy(prev);
            }
            return;
        }
        prev = entry;
    }

    PDStackRef ks = PDStackGetDictKey(object->def, key, true);
    PDStackDestroy(ks);
}

#define PDArraySetEntries(ob,v)  ob->mutations->info = as(void*, v)
#define PDArrayGetEntries(ob)    as(PDStackRef *, ob->mutations->info)

#define PDArraySetCount(ob,v)    ob->mutations->prev->info = as(void*, v)
#define PDArrayGetCount(ob)      as(PDInteger, ob->mutations->prev->info)

#define PDArraySetCapacity(ob,v) ob->mutations->prev->prev->info = as(void*, v)
#define PDArrayGetCapacity(ob)   as(PDInteger, ob->mutations->prev->prev->info)

void PDObjectInstantiateArray(PDObjectRef object)
{
    /*
stack<0x10c1c0bb0> {
   0x10001d4f8 ("array")
   0x10001d4e0 ("entries")
    stack<0x10c1c0ca0> {
        stack<0x10c1c12a0> {
           0x10001d500 ("ae")
            stack<0x10c1c4bf0> {
               0x10001d4c0 ("name")
               ICCBased
            }
        }
        stack<0x10c1c1310> {
           0x10001d500 ("ae")
            stack<0x10c1c0b70> {
               0x10001d4d0 ("ref")
               1688
               0
            }
        }
    }
}
     */
    
    PDInteger count = 0;
    PDStackRef iter;
    PDStackRef ref = as(PDStackRef, as(PDStackRef, object->def)->prev->prev->info);
    for (iter = ref; iter; iter = iter->prev) 
        count++;
    PDStackRef *els = count < 1 ? NULL : malloc(count * sizeof(PDStackRef));
    
    count = 0;
    for (iter = ref; iter; iter = iter->prev) {
        els[count++] = as(PDStackRef, iter->info)->prev;
    }
        
    // third entry is the capacity of the array
    PDStackPushIdentifier(&object->mutations, (PDID)count);

    // second entry in the mutations is the length of the array
    PDStackPushIdentifier(&object->mutations, (PDID)count);
    
    // first entry is the array
    PDStackPushFreeable(&object->mutations, els);
}

PDInteger PDObjectGetArrayCount(PDObjectRef object)
{
    if (NULL == object->mutations)
        PDObjectInstantiateArray(object);
    
    return PDArrayGetCount(object);
}

const char *PDObjectGetArrayElementAtIndex(PDObjectRef object, PDInteger index)
{
    if (NULL == object->mutations) 
        PDObjectInstantiateArray(object);
    
    PDAssert(PDArrayGetCount(object) > index);  // crash = attempt to get element beyond count

    PDStackRef *array = PDArrayGetEntries(object);
    PDStackRef entry = array[index];
    if (entry->type != PDSTACK_STRING) {
        char *value;
        // to-string elements on demand
        PDStackSetGlobalPreserveFlag(true);
        entry = (PDStackRef)entry->info;
        value = PDStringFromComplex(&entry);
        PDStackSetGlobalPreserveFlag(false);
        PDStackReplaceInfoObject(entry, PDSTACK_STRING, value);
    }
    
    return as(const char *, entry->info);
}

void PDObjectAddArrayElement(PDObjectRef object, const char *value)
{
    if (NULL == object->mutations)
        PDObjectInstantiateArray(object);
    
    PDStackRef *els = PDArrayGetEntries(object);
    PDInteger count = PDArrayGetCount(object);
    PDInteger cap   = PDArrayGetCapacity(object);
    
    if (cap == count) {
        cap += 1 + cap;
        els = PDArraySetEntries(object, realloc(els, cap * sizeof(PDStackRef)));
        PDArraySetCapacity(object, cap);
    }

    // object's def holds onto the stack of keys, while mutations holds onto the array of pointers to the entries
    // therefore we must shift the new entry into the defs stack's entries, which is easiest done by putting a new
    // entry in at the end and setting prev for the last entry (els[count-1]) to the new entry
    els[count] = NULL;
    PDStackPushKey(&els[count], (char *)value);

    // we don't currently have a method for adding to an empty array; if an example PDF with an empty array is encountered, give me or patch code and give me!
    PDAssert(count > 0);
    els[count-1]->prev = els[count];
    
    PDArraySetCount(object, count + 1);
}

/**
 Delete the array element at the given index.
 
 @param object The object.
 @param index The array index.
 */
void PDObjectRemoveArrayElementAtIndex(PDObjectRef object, PDInteger index)
{
    if (NULL == object->mutations) 
        PDObjectInstantiateArray(object);
        
    PDStackRef *array = PDArrayGetEntries(object);
    PDInteger count = PDArrayGetCount(object);
    
    PDAssert(count > index);  // crash = attempt to get element beyond count

    PDStackRef entry = array[index];
    
    // we don't support EMPTYING arrays
    PDAssert(count > 0);
    
    for (index++; index < count; index++) 
        array[index-1] = array[index];
    
    array[index]->prev = entry->prev;
    entry->prev = NULL;
    
    PDStackDestroy(entry);
    PDArraySetCount(object, count-1);
}

/**
 Replace the value of the array element at the given index with a new value.
 
 @param object The object.
 @param index The array index.
 @param value The replacement value.
 */
void PDObjectSetArrayElement(PDObjectRef object, PDInteger index, const char *value)
{
    if (NULL == object->mutations) 
        PDObjectInstantiateArray(object);
    
    PDAssert(PDArrayGetCount(object) > index);  // crash = attempt to get element beyond count
    
    PDStackRef *array = PDArrayGetEntries(object);
    PDStackRef entry = array[index];
    
    if (entry->type != PDSTACK_STRING) {
        PDStackReplaceInfoObject(entry, PDSTACK_STRING, strdup(value));
    } else {
        free(entry->info);
        entry->info = strdup(value);
    }
}

void PDObjectReplaceWithString(PDObjectRef object, char *str, PDInteger len)
{
    object->ovrDef = str;
    object->ovrDefLen = len;
}

void PDObjectSkipStream(PDObjectRef object)
{
    object->skipStream = true;
}

void PDObjectSetStream(PDObjectRef object, const char *str, PDInteger len, PDBool includeLength)
{
    object->ovrStream = str;
    object->ovrStreamLen = len;
    if (includeLength)  {
        char *lenstr = malloc(30);
        sprintf(lenstr, "%ld", len);
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

PDInteger PDObjectGenerateDefinition(PDObjectRef object, char **dstBuf, PDInteger capacity)
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
    PDStackRef *array;
    PDInteger i, count;
    char *key, *val;
    PDInteger sz;
    putfmt("%ld %ld obj\n", object->obid, object->genid);
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
                PDStringGrow(4 + strlen(key) + strlen(val));
                putfmt("/%s %s ", key, val);
                free(val);
            }
            PDStringGrow(10);
            putstr(">>\n", 2);
            break;
            
        case PDObjectTypeArray:
            if (NULL == object->mutations) {
                // no mutations means we can just to-string it right away
                PDStackSetGlobalPreserveFlag(true);
                stack = object->def;
                val = PDStringFromComplex(&stack);
                PDStackSetGlobalPreserveFlag(false);
                PDStringGrow(1 + strlen(val));
                putfmt("%s", val);
                break;
            }

            PDStringGrow(10);
            currchi = '[';
            PDObjectInstantiateArray(object);
                
            array = PDArrayGetEntries(object);
            count = PDArrayGetCount(object);
            PDStackSetGlobalPreserveFlag(true);
            for (i = 0; i < count; i++) {
                if (array[i]->type == PDSTACK_STRING) {
                    val = array[i]->info;
                } else {
                    stack = (PDStackRef)array[i]->info;
                    val = PDStringFromComplex(&stack);
                }
                PDStringGrow(2 + strlen(val));
                putfmt(" %s", val);
                
                if (array[i]->type != PDSTACK_STRING) 
                    free(val);
            }
            PDStackSetGlobalPreserveFlag(false);
            
            PDStringGrow(3);
            currchi = ' ';
            currchi = ']';
            break;
            
        case PDObjectTypeString:
            putfmt("%s\n", (char*)object->def);
            break;
            
        default:
            PDStackSetGlobalPreserveFlag(true);
            stack = object->def;
            val = PDStringFromComplex(&stack);
            PDStackSetGlobalPreserveFlag(false);
            PDStringGrow(1 + strlen(val));
            putfmt("%s", val);
            break;
            
    }
    
    PDStringGrow(2);
    currchi = '\n';
    currch = 0;
    
    *dstBuf = scv->allocBuf;

    return scv->offs;
}



