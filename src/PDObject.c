//
// PDObject.c
//
// Copyright (c) 2013 Karl-Johan Alm (http://github.com/kallewoof)
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

#include "pd_internal.h"
#include "pd_stack.h"
#include "PDScanner.h"
#include "pd_pdf_implementation.h"
#include "pd_pdf_private.h"
#include "PDStreamFilter.h"
#include "PDObject.h"
#include "pd_array.h"
#include "pd_dict.h"

void PDObjectDestroy(PDObjectRef object)
{
    if (object->array) pd_array_destroy(object->array);
    if (object->dict) pd_dict_destroy(object->dict);
    pd_stack_destroy(object->mutations);
    
    if (object->type == PDObjectTypeString)
        free(object->def);
    else
        pd_stack_destroy(object->def);
    
    if (object->ovrDef) free(object->ovrDef);
    if (object->ovrStream && object->ovrStreamAlloc)
        free(object->ovrStream);
    if (object->refString) free(object->refString);
    if (object->extractedLen != -1) free(object->streamBuf);
}

PDObjectRef PDObjectCreate(PDInteger obid, PDInteger genid)
{
    PDObjectRef ob = PDAlloc(sizeof(struct PDObject), PDObjectDestroy, true);
    //PDObjectRef ob = calloc(1, sizeof(struct PDObject));
    ob->obid = obid;
    ob->genid = genid;
    ob->type = PDObjectTypeUnknown;
    ob->obclass = PDObjectClassRegular;
    ob->extractedLen = -1;
    return ob;
}

PDObjectRef PDObjectCreateFromDefinitionsStack(PDInteger obid, pd_stack defs)
{
    PDObjectRef ob = PDObjectCreate(obid, 0);
    ob->def = defs;
    return ob;
}

void PDObjectSetSynchronizationCallback(PDObjectRef object, PDSynchronizer callback, const void *syncInfo)
{
    object->synchronizer = callback;
    object->syncInfo = syncInfo;
}

void PDObjectDelete(PDObjectRef object)
{
    if (object->obclass != PDObjectClassCompressed) {
        object->skipObject = object->deleteObject = true;
    } else {
        fprintf(stderr, "*** Pajdeg notice *** objects inside of object streams cannot be deleted");
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

PDBool PDObjectGetObStreamFlag(PDObjectRef object)
{
    return object->obclass == PDObjectClassCompressed;
}

PDObjectType PDObjectGetType(PDObjectRef object)
{
    return object->type;
}

PDObjectType PDObjectDetermineType(PDObjectRef object)
{
    pd_stack st = object->def;
    if (st == NULL) return object->type;
    
    PDID stid = st->info;
    if (PDIdentifies(stid, PD_ARRAY))
        object->type = PDObjectTypeArray;
    else if (PDIdentifies(stid, PD_DICT))
        object->type = PDObjectTypeDictionary;
    else if (PDIdentifies(stid, PD_NAME))
        object->type = PDObjectTypeString; // todo: make use of more types
    return object->type;
}

const char *PDObjectGetReferenceString(PDObjectRef object)
{
    if (object->refString)
        return object->refString;
    
    char refbuf[64];
    sprintf(refbuf, "%ld %ld R", object->obid, object->genid);
    object->refString = strdup(refbuf);
    return object->refString;
}

PDBool PDObjectHasStream(PDObjectRef object)
{
    return object->hasStream;
}

PDInteger PDObjectGetStreamLength(PDObjectRef object)
{
    return object->streamLen;
}

PDInteger PDObjectGetExtractedStreamLength(PDObjectRef object)
{
    PDAssert(object->extractedLen != -1);
    return object->extractedLen;
}

char *PDObjectGetStream(PDObjectRef object)
{
    PDAssert(object->extractedLen != -1);
    return object->streamBuf;
}

char *PDObjectGetValue(PDObjectRef object)
{
    return object->def;
}

void PDObjectSetValue(PDObjectRef object, const char *value)
{
    PDAssert(object->type == PDObjectTypeString);
    if (object->def) 
        free(object->def);
    object->def = strdup(value);
}

//pd_stack lastKeyContainer;
const char *PDObjectGetDictionaryEntry(PDObjectRef object, const char *key)
{
    if (NULL == object->dict) {
        object->dict = pd_dict_from_pdf_dict_stack(object->def);
        object->type = PDObjectTypeDictionary;
    }
    
    return pd_dict_get(object->dict, key);
#if 0
    // check mutations dict if we have this one already
    pd_stack entry, field;
    for (entry = object->mutations; entry; entry = entry->prev->prev) {
        if (!strcmp((char*)entry->info, key)) {
            lastKeyContainer = entry;
            return entry->prev->info;
        }
    }
    
    char *value;
    entry = pd_stack_get_dict_key(object->def, key, true);
    if (! entry) return NULL;
    
    // entry = e, ID, field
    pd_stack_assert_expected_key(&entry, PD_DE);
    pd_stack_assert_expected_key(&entry, key);
    // so we see if type is primitive or not
    if (entry->type == pd_stack_STACK) {
        // it's not primitive, so we stringify
        field = pd_stack_pop_stack(&entry);
        value = PDStringFromComplex(&field);
    } else {
        // it is primitive (we presume)
        PDAssert(entry->type == PD_STACK_STRING);
        value = pd_stack_pop_key(&entry);
    }
    
    // add this to mutations, as we keep everything simplified (and STRINGified) there
    pd_stack_push_key(&object->mutations, value);
    pd_stack_push_key(&object->mutations, strdup(key));
    lastKeyContainer = object->mutations;

    // destroy entry, as it's been detached since we removed it
    pd_stack_destroy(entry);
    
    return value;
#endif
}

void PDObjectSetDictionaryEntry(PDObjectRef object, const char *key, const char *value)
{
    if (NULL == object->dict) {
        object->dict = pd_dict_from_pdf_dict_stack(object->def);
        object->type = PDObjectTypeDictionary;
    }
    
    pd_dict_set(object->dict, key, value);
#if 0
    object->type = PDObjectTypeDictionary;
    
    // see if we have it (user is responsible for nulling lastKeyContainer)
    lastKeyContainer = NULL;
    PDObjectGetDictionaryEntry(object, key);
    if (lastKeyContainer) {
        free(lastKeyContainer->prev->info);
        lastKeyContainer->prev->info = strdup(value);
        return;
    }
    
    // we didn't so we add
    pd_stack_push_key(&object->mutations, strdup(value));
    pd_stack_push_key(&object->mutations, strdup(key));
#endif
}

void PDObjectRemoveDictionaryEntry(PDObjectRef object, const char *key)
{
    if (NULL == object->dict) {
        object->dict = pd_dict_from_pdf_dict_stack(object->def);
        object->type = PDObjectTypeDictionary;
    }
    
    pd_dict_remove(object->dict, key);
#if 0
    // check mutations dict as well
    pd_stack entry, prev;
    prev = NULL;
    for (entry = object->mutations; entry; entry = entry->prev->prev) {
        if (!strcmp((char*)entry->info, key)) {
            if (prev) {
                prev->prev->prev = entry->prev->prev;
                entry->prev->prev = NULL;
                pd_stack_destroy(entry);
            } else {
                prev = object->mutations;
                object->mutations = entry->prev->prev;
                prev->prev->prev = NULL;
                pd_stack_destroy(prev);
            }
            return;
        }
        prev = entry;
    }

    pd_stack ks = pd_stack_get_dict_key(object->def, key, true);
    pd_stack_destroy(ks);
#endif
}

/*#define PDArraySetEntries(ob,v)  ob->mutations->info = as(void*, v)
#define PDArrayGetEntries(ob)    as(pd_stack *, ob->mutations->info)

#define PDArraySetCount(ob,v)    ob->mutations->prev->info = as(void*, v)
#define PDArrayGetCount(ob)      as(PDInteger, ob->mutations->prev->info)

#define PDArraySetCapacity(ob,v) ob->mutations->prev->prev->info = as(void*, v)
#define PDArrayGetCapacity(ob)   as(PDInteger, ob->mutations->prev->prev->info)*/

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
    if (object->array != NULL) {
        pd_array_destroy(object->array);
    }
    object->array = pd_array_from_pdf_array_stack(object->def);
    
#if 0
    PDInteger count = 0;
    pd_stack iter;
    pd_stack ref = as(pd_stack, as(pd_stack, object->def)->prev->prev->info);
    for (iter = ref; iter; iter = iter->prev) 
        count++;
    pd_stack *els = count < 1 ? NULL : malloc(count * sizeof(pd_stack));
    
    count = 0;
    for (iter = ref; iter; iter = iter->prev) {
        els[count++] = as(pd_stack, iter->info)->prev;
    }
        
    // third entry is the capacity of the array
    pd_stack_push_identifier(&object->mutations, (PDID)count);

    // second entry in the mutations is the length of the array
    pd_stack_push_identifier(&object->mutations, (PDID)count);
    
    // first entry is the array
    pd_stack_push_freeable(&object->mutations, els);
#endif

    // set the object type as we're now defined to be an array
    object->type = PDObjectTypeArray;
}

PDInteger PDObjectGetDictionaryCount(PDObjectRef object)
{
    if (NULL == object->dict) {
        object->dict = pd_dict_from_pdf_dict_stack(object->def);
        object->type = PDObjectTypeDictionary;
    }

    return pd_dict_get_count(object->dict);
}

PDInteger PDObjectGetArrayCount(PDObjectRef object)
{
    if (NULL == object->array)
        PDObjectInstantiateArray(object);
    
    return pd_array_get_count(object->array);
    
    //return PDArrayGetCount(object);
}

const char *PDObjectGetArrayElementAtIndex(PDObjectRef object, PDInteger index)
{
    if (NULL == object->array) 
        PDObjectInstantiateArray(object);
    
    return pd_array_get_at_index(object->array, index);
#if 0
    PDAssert(PDArrayGetCount(object) > index);  // crash = attempt to get element beyond count

    pd_stack *array = PDArrayGetEntries(object);
    pd_stack entry = array[index];
    if (entry->type != PD_STACK_STRING) {
        char *value;
        // to-string elements on demand
        pd_stack_set_global_preserve_flag(true);
        entry = (pd_stack)entry->info;
        value = PDStringFromComplex(&entry);
        entry = array[index];
        pd_stack_set_global_preserve_flag(false);
        pd_stack_replace_info_object(entry, PD_STACK_STRING, value);
    }
    
    return as(const char *, entry->info);
#endif
}

void PDObjectAddArrayElement(PDObjectRef object, const char *value)
{
    if (NULL == object->array)
        PDObjectInstantiateArray(object);
    
    pd_array_append(object->array, value);
#if 0
    pd_stack *els = PDArrayGetEntries(object);
    PDInteger count = PDArrayGetCount(object);
    PDInteger cap   = PDArrayGetCapacity(object);
    
    if (cap == count) {
        cap += 1 + cap;
        els = PDArraySetEntries(object, realloc(els, cap * sizeof(pd_stack)));
        PDArraySetCapacity(object, cap);
    }

    // object's def holds onto the stack of keys, while mutations holds onto the array of pointers to the entries
    // therefore we must shift the new entry into the defs stack's entries, which is easiest done by putting a new
    // entry in at the end and setting prev for the last entry (els[count-1]) to the new entry
    els[count] = NULL;
    pd_stack_push_key(&els[count], strdup(value));

    // we don't currently have a method for adding to an empty array; if an example PDF with an empty array is encountered, give me or patch code and give me!
    PDAssert(count > 0);
    els[count-1]->prev = els[count];
    
    PDArraySetCount(object, count + 1);
#endif
}

/**
 Delete the array element at the given index.
 
 @param object The object.
 @param index The array index.
 */
void PDObjectRemoveArrayElementAtIndex(PDObjectRef object, PDInteger index)
{
    if (NULL == object->array) 
        PDObjectInstantiateArray(object);
    
    pd_array_remove_at_index(object->array, index);
#if 0
    pd_stack *array = PDArrayGetEntries(object);
    PDInteger count = PDArrayGetCount(object);
    
    PDAssert(count > index);  // crash = attempt to get element beyond count

    pd_stack entry = array[index];
    
    // we don't support EMPTYING arrays
    PDAssert(count > 0);
    
    for (index++; index < count; index++) 
        array[index-1] = array[index];
    
    //array[index]->prev = entry->prev;
    entry->prev = NULL;
    
    pd_stack_destroy(entry);
    PDArraySetCount(object, count-1);
#endif
}

/**
 Replace the value of the array element at the given index with a new value.
 
 @param object The object.
 @param index The array index.
 @param value The replacement value.
 */
void PDObjectSetArrayElement(PDObjectRef object, PDInteger index, const char *value)
{
    if (NULL == object->array) 
        PDObjectInstantiateArray(object);
    
    pd_array_replace_at_index(object->array, index, value);
#if 0
    PDAssert(PDArrayGetCount(object) > index);  // crash = attempt to get element beyond count
    
    pd_stack *array = PDArrayGetEntries(object);
    pd_stack entry = array[index];
    
    if (entry->type != PD_STACK_STRING) {
        pd_stack_replace_info_object(entry, PD_STACK_STRING, strdup(value));
    } else {
        free(entry->info);
        entry->info = strdup(value);
    }
#endif
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

void PDObjectSetStream(PDObjectRef object, char *str, PDInteger len, PDBool includeLength, PDBool allocated)
{
    object->ovrStream = str;
    object->ovrStreamLen = len;
    object->ovrStreamAlloc = allocated;
    if (includeLength)  {
        char *lenstr = malloc(30);
        sprintf(lenstr, "%ld", len);
        PDObjectSetDictionaryEntry(object, "Length", lenstr);
        free(lenstr);
    }
}

PDBool PDObjectSetStreamFiltered(PDObjectRef object, char *str, PDInteger len)
{
    // Need to get /Filter and /DecodeParms
    const char *filter = PDObjectGetDictionaryEntry(object, "Filter");
    
    if (NULL == filter) {
        // no filter
        return false;
    } else filter = &filter[1]; // get rid of name slash

    const char *decodeParms = PDObjectGetDictionaryEntry(object, "DecodeParms");
    
    pd_stack options = NULL;
    if (NULL != decodeParms) {
        /// @todo At some point this (all values as strings) should not be as roundabout as it is but it will do for now.
        
        PDScannerRef scanner = PDScannerCreateWithState(pdfRoot);
        PDScannerAttachFixedSizeBuffer(scanner, (char*)decodeParms, strlen(decodeParms));
        pd_stack optsDict;
        if (! PDScannerPopStack(scanner, &optsDict)) {
            PDWarn("Unable to pop dictionary stack from decode parameters: %s", decodeParms);
            PDAssert(0);
            optsDict = NULL;
        }
        if (optsDict) {
            options = PDStreamFilterGenerateOptionsFromDictionaryStack(optsDict);
            pd_stack_destroy(optsDict);
        }
        PDRelease(scanner);
    }
    
    PDBool success = true;
    PDStreamFilterRef sf = PDStreamFilterObtain(filter, false, options);
    if (NULL == sf) {
        // we don't support this filter, at all
        pd_stack_destroy(options);
        success = false;
    } 
    
    if (success) success = PDStreamFilterInit(sf);
    // if !success, filter did not initialize properly

    if (success) success = (sf->compatible);
    // if !success, filter was not compatible with options

    char *filtered;
    PDInteger flen;
    if (success) success = PDStreamFilterApply(sf, (unsigned char *)str, (unsigned char **)&filtered, len, &flen);
    // if !success, filter did not apply to input data successfully

    PDRelease(sf);
    
    if (success) PDObjectSetStream(object, filtered, flen, true, true);
    
    return success;
}

void PDObjectSetFlateDecodedFlag(PDObjectRef object, PDBool state)
{
    if (state) {
        PDObjectSetDictionaryEntry(object, "Filter", "/FlateDecode");
    } else {
        PDObjectRemoveDictionaryEntry(object, "Filter");
        PDObjectRemoveDictionaryEntry(object, "DecodeParms");
    }
}

void PDObjectSetPredictionStrategy(PDObjectRef object, PDPredictorType strategy, PDInteger columns)
{
    char buf[52];
    sprintf(buf, "<</Predictor %d /Columns %ld>>", strategy, columns);
    PDObjectSetDictionaryEntry(object, "DecodeParms", buf);
}

void PDObjectSetStreamEncrypted(PDObjectRef object, PDBool encrypted)
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
    
    pd_stack stack;
    //pd_stack *array;
    PDInteger i;//, count;
    char *key, *val;
    PDInteger sz;
    switch (object->obclass) {
        case PDObjectClassRegular:
            putfmt("%ld %ld obj\n", object->obid, object->genid);
            break;
        case PDObjectClassCompressed:
            break;
        case PDObjectClassTrailer:
            putstr("trailer\n", 8);
        default:
            PDAssert(0); // crash = undefined class which at a 99.9% accuracy means memory was trashed because that should never ever happen
    }
    switch (object->type) {
        case PDObjectTypeDictionary:
            if (NULL == object->dict) {
                PDStringGrow(20);
                putstr("<<", 2);
                // write out mutations first (deprecated)
                for (stack = object->mutations; stack; stack = stack->prev->prev) {
                    PDStringGrow(13 + strlen(stack->info) + strlen(stack->prev->info));
                    putfmt("/%s %s ", (char*)stack->info, (char*)stack->prev->info);
                }
                // then write out unmodified entries
                stack = object->def;
                while (pd_stack_get_next_dict_key(&stack, &key, &val)) { // <-- sploff upon generating << /Marked true >>
                    PDStringGrow(4 + strlen(key) + strlen(val));
                    putfmt("/%s %s ", key, val);
                    free(val);
                }
                PDStringGrow(10);
                putstr(">>\n", 2);
            } else {
                char *dictstr = pd_dict_to_string(object->dict);
                i = strlen(dictstr);
                PDStringGrow(i + i);
                putstr(dictstr, i);
            }
            break;
            
        case PDObjectTypeArray:
            if (NULL == object->array) {
                // no array means we can just to-string it right away
                pd_stack_set_global_preserve_flag(true);
                stack = object->def;
                val = PDStringFromComplex(&stack);
                pd_stack_set_global_preserve_flag(false);
                PDStringGrow(1 + strlen(val));
                putfmt("%s", val);
                break;
            }
            
            char *arrstr = pd_array_to_string(object->array);
            i = strlen(arrstr);
            PDStringGrow(1 + i);
            putstr(arrstr, i);
#if 0
            PDStringGrow(10);
            currchi = '[';
            //PDObjectInstantiateArray(object);
            
            array = PDArrayGetEntries(object);
            count = PDArrayGetCount(object);
            pd_stack_set_global_preserve_flag(true);
            for (i = 0; i < count; i++) {
                if (array[i]->type == PD_STACK_STRING) {
                    val = array[i]->info;
                } else {
                    stack = (pd_stack)array[i]->info;
                    val = PDStringFromComplex(&stack);
                }
                PDStringGrow(2 + strlen(val));
                putfmt(" %s", val);
                
                if (array[i]->type != PD_STACK_STRING) 
                    free(val);
            }
            pd_stack_set_global_preserve_flag(false);
            
            PDStringGrow(3);
            currchi = ' ';
            currchi = ']';
#endif
            break;
            
        case PDObjectTypeString:
            PDStringGrow(1 + strlen((char*)object->def));
            putfmt("%s\n", (char*)object->def);
            break;
            
        default:
            pd_stack_set_global_preserve_flag(true);
            stack = object->def;
            PDAssert(stack); // crash = an object was appended or created, but it was not set to any value; this is not legal in PDFs; objects must contain something
            val = PDStringFromComplex(&stack);
            pd_stack_set_global_preserve_flag(false);
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



