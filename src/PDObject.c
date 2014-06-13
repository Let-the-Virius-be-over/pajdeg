//
// PDObject.c
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
    pd_stack_destroy(&object->mutations);
    
    if (object->type == PDObjectTypeString)
        free(object->def);
    else
        pd_stack_destroy((pd_stack *)&object->def);
    
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
        PDError("Objects inside of object streams cannot be deleted");
    }
}

void PDObjectUndelete(PDObjectRef object)
{
    if (object->obclass != PDObjectClassCompressed) {
        object->skipObject = object->deleteObject = false;
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

void PDObjectSetType(PDObjectRef object, PDObjectType type)
{
    object->type = type;
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
    else if (st->prev == NULL) {
        // this is *probably* a string; let's see if we get something sensible out of it
        char *string = strdup(st->info);
        if (string) {
            object->type = PDObjectTypeString;
            object->def = string;
            pd_stack_destroy(&st);
            /*
            if (string[0] == '(') {
                // yeah, it's a string
                object->type = PDObjectTypeString;
            } else if (string[0] >= '0' && string[0] <= '9') {
                // it's a number; we can find out by looking for a period if it's real or integral
                while (string[0] && string[0] != '.') string = &string[1];
                if (string[0]) { 
                    // it's real
                    object->type = PDObjectTypeReal;
                } else {
                    // it's integral
                    object->type = PDObjectTypeInteger;
                }
            }
            */
        }
    }
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

PDBool PDObjectHasTextStream(PDObjectRef object)
{
    PDAssert(object->extractedLen != -1);
    if (object->extractedLen == 0) return false;
    
    PDInteger ix = object->extractedLen > 10 ? 10 : object->extractedLen - 1;
    PDInteger matches = 0;
    PDInteger thresh = ix * 0.8f;
    char ch;
//    if (object->streamBuf[object->extractedLen-1] != 0) 
//        return false;
    for (PDInteger i = 0; matches < thresh && i < ix; i++) {
        ch = object->streamBuf[i];
        matches += ((ch >= 'a' && ch <= 'z') || 
                    (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    (ch >= 32 && ch <= 126) ||
                    ch == '\n' || ch == '\r');
    }
    return matches >= thresh;
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

void PDObjectInstantiateDictionary(PDObjectRef object)
{
    object->dict = pd_dict_from_pdf_dict_stack(object->def);
    object->type = PDObjectTypeDictionary;
#ifdef PD_SUPPORT_CRYPTO
    if (object->crypto) {
        pd_dict_set_crypto(object->dict, object->crypto, object->obid, object->genid);
    }
#endif
}

void PDObjectInstantiateArray(PDObjectRef object)
{
    object->array = pd_array_from_pdf_array_stack(object->def);
    object->type = PDObjectTypeArray;
#ifdef PD_SUPPORT_CRYPTO
    if (object->crypto) {
        pd_array_set_crypto(object->array, object->crypto, object->obid, object->genid);
    }
#endif
}

const char *PDObjectGetDictionaryEntry(PDObjectRef object, const char *key)
{
    if (NULL == object->dict) {
        PDObjectInstantiateDictionary(object);
    }
    
    return pd_dict_get(object->dict, key);
}

const pd_stack PDObjectGetDictionaryEntryRaw(PDObjectRef object, const char *key)
{
    if (NULL == object->dict) {
        PDObjectInstantiateDictionary(object);
    }
    
    return pd_dict_get_raw(object->dict, key);
}

PDObjectType PDObjectGetDictionaryEntryType(PDObjectRef object, const char *key)
{
    if (NULL == object->dict) {
        PDObjectInstantiateDictionary(object);
    }
    
    return pd_dict_get_type(object->dict, key);
}

void *PDObjectCopyDictionaryEntry(PDObjectRef object, const char *key)
{
    if (NULL == object->dict) {
        PDObjectInstantiateDictionary(object);
    }
    
    return pd_dict_get_copy(object->dict, key);
}


void PDObjectSetDictionaryEntry(PDObjectRef object, const char *key, const char *value)
{
    if (NULL == object->dict) {
        PDObjectInstantiateDictionary(object);
    }
    
    pd_dict_set(object->dict, key, value);
}

void PDObjectRemoveDictionaryEntry(PDObjectRef object, const char *key)
{
    if (NULL == object->dict) {
        PDObjectInstantiateDictionary(object);
    }
    
    pd_dict_remove(object->dict, key);
}

pd_dict PDObjectGetDictionary(PDObjectRef object)
{
    if (NULL == object->dict) {
        PDObjectInstantiateDictionary(object);
    }
    return object->dict;
}

pd_array PDObjectGetArray(PDObjectRef object)
{
    if (NULL == object->array) {
        PDObjectInstantiateArray(object);
    }
    return object->array;
}

PDInteger PDObjectGetDictionaryCount(PDObjectRef object)
{
    if (NULL == object->dict) {
        PDObjectInstantiateDictionary(object);
    }

    return pd_dict_get_count(object->dict);
}

PDInteger PDObjectGetArrayCount(PDObjectRef object)
{
    if (NULL == object->array)
        PDObjectInstantiateArray(object);
    
    return pd_array_get_count(object->array);
}

const char *PDObjectGetArrayElementAtIndex(PDObjectRef object, PDInteger index)
{
    if (NULL == object->array) 
        PDObjectInstantiateArray(object);
    
    return pd_array_get_at_index(object->array, index);
}

const pd_stack PDObjectGetArrayElementRawAtIndex(PDObjectRef object, PDInteger index)
{
    if (NULL == object->array)
        PDObjectInstantiateArray(object);
    
    return pd_array_get_raw_at_index(object->array, index);
}

PDObjectType PDObjectGetArrayElementTypeAtIndex(PDObjectRef object, PDInteger index)
{
    if (NULL == object->array)
        PDObjectInstantiateArray(object);
    
    return pd_array_get_type_at_index(object->array, index);
}

void *PDObjectCopyArrayElementAtIndex(PDObjectRef object, PDInteger index)
{
    if (NULL == object->array)
        PDObjectInstantiateArray(object);
    
    return pd_array_get_copy_at_index(object->array, index);
}

void PDObjectAddArrayElement(PDObjectRef object, const char *value)
{
    if (NULL == object->array)
        PDObjectInstantiateArray(object);
    
    pd_array_append(object->array, value);
}

void PDObjectRemoveArrayElementAtIndex(PDObjectRef object, PDInteger index)
{
    if (NULL == object->array) 
        PDObjectInstantiateArray(object);
    
    pd_array_remove_at_index(object->array, index);
}

void PDObjectSetArrayElement(PDObjectRef object, PDInteger index, const char *value)
{
    if (NULL == object->array) 
        PDObjectInstantiateArray(object);
    
    pd_array_replace_at_index(object->array, index, value);
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
        PDObjectSetStream(object, str, len, true, false);
        return true;
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
            pd_stack_destroy(&optsDict);
        }
        PDRelease(scanner);
    }
    
    PDBool success = true;
    PDStreamFilterRef sf = PDStreamFilterObtain(filter, false, options);
    if (NULL == sf) {
        // we don't support this filter, at all
        pd_stack_destroy(&options);
        success = false;
    } 
    
    if (success) success = PDStreamFilterInit(sf);
    // if !success, filter did not initialize properly

    if (success) success = (sf->compatible);
    // if !success, filter was not compatible with options

    char *filtered = NULL;
    PDInteger flen = 0;
    if (success) success = PDStreamFilterApply(sf, (unsigned char *)str, (unsigned char **)&filtered, len, &flen, NULL);
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
    
    pd_stack stack;
    PDInteger i;
    char *val;
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
                // no dict probably means we can to-string but this needs to be tested; for now we instantiate dict and use that
                object->dict = pd_dict_from_pdf_dict_stack(object->def);
            } 
            
            val = pd_dict_to_string(object->dict);
            i = strlen(val);
            PDStringGrow(i + i);
            putstr(val, i);
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
            
            val = pd_array_to_string(object->array);
            i = strlen(val);
            PDStringGrow(1 + i);
            putstr(val, i);
            break;
            
        case PDObjectTypeString:
            PDStringGrow(2 + strlen((char*)object->def));
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

PDInteger PDObjectPrinter(void *inst, char **buf, PDInteger offs, PDInteger *cap)
{
    PDInstancePrinterInit(PDObjectRef, 12, 50);
    char *bv = *buf;
    return offs + sprintf(&bv[offs], "%ld %ld R", i->obid, i->genid);;
}
