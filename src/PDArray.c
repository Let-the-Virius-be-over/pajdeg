//
// PDArray.c
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
#include "PDArray.h"
#include "pd_stack.h"
#include "pd_crypto.h"
#include "pd_pdf_implementation.h"

#include "PDString.h"
#include "pd_internal.h"

void PDArrayDestroy(PDArrayRef array)
{
    for (PDInteger i = array->count-1; i >= 0; i--) {
        PDRelease(array->values[i]);
        pd_stack_destroy(&array->vstacks[i]);
    }
    
#ifdef PD_SUPPORT_CRYPTO
    PDRelease(array->ci);
#endif
    
    free(array->values);
    free(array->vstacks);
}

PDArrayRef PDArrayCreateWithCapacity(PDInteger capacity)
{
    PDArrayRef arr = PDAllocTyped(PDInstanceTypeArray, sizeof(struct PDArray), PDArrayDestroy, false);
    
    if (capacity < 1) capacity = 1;
    
#ifdef PD_SUPPORT_CRYPTO
    arr->ci = NULL;
#endif
    arr->count = 0;
    arr->capacity = capacity;
    arr->values = malloc(sizeof(void *) * capacity);
    arr->vstacks = malloc(sizeof(pd_stack) * capacity);
    return arr;
}

PDArrayRef PDArrayCreateWithStackList(pd_stack stack)
{
    if (stack == NULL) {
        // empty array
        return PDArrayCreateWithCapacity(1);
    }
    
    // determine size of stack
    PDInteger count;
    pd_stack s = stack;
//    pd_stack entry;
    
    for (count = 0; s; s = s->prev)
        count++;
    PDArrayRef arr = PDArrayCreateWithCapacity(count);
    arr->count = count;
    
    s = stack;
    pd_stack_set_global_preserve_flag(true);
    for (count = 0; s; s = s->prev) {
        if (s->type == PD_STACK_STRING) {
            arr->values[count] = PDStringCreate(strdup(s->info));
//            arr->values[count] =  // strdup(s->info);
            arr->vstacks[count] = NULL;
        } else {
            arr->vstacks[count] = /*entry =*/ pd_stack_copy(s->info);
            arr->values[count] = NULL; //NULL;//PDContainerCreateFromComplex(&entry);
        }
        count++;
    }
    pd_stack_set_global_preserve_flag(false);
    
    return arr;
}

PDArrayRef PDArrayCreateWithComplex(pd_stack stack)
{
    if (stack == NULL) {
        // empty array
        return PDArrayCreateWithCapacity(1);
    }
    
    /*
stack<0x14cdde90> {
   0x401394 ("array")
   0x401388 ("entries")
    stack<0x14cdeb90> {
        stack<0x14cdead0> {
           0x401398 ("ae")
           0
        }
    }
}
     */
    if (PDIdentifies(stack->info, PD_ARRAY)) {
        stack = stack->prev->prev->info;
    }
    /*
    stack<0x14cdeb90> {
        stack<0x14cdead0> {
           0x401398 ("ae")
           0
        }
    }
     */
    
    // determine size of stack
    PDInteger count;
    pd_stack s = stack;
    pd_stack entry;
    
    for (count = 0; s; s = s->prev)
        count++;
    PDArrayRef arr = PDArrayCreateWithCapacity(count);
    arr->count = count;
    
    s = stack;
    pd_stack_set_global_preserve_flag(true);
    for (count = 0; s; s = s->prev) {
        entry = as(pd_stack, s->info)->prev;
        if (entry->type == PD_STACK_STRING) {
            arr->values[count] = PDStringCreate(strdup(entry->info));
            arr->vstacks[count] = NULL;
        } else {
            arr->vstacks[count] = /*entry =*/ pd_stack_copy(entry->info);
            arr->values[count] = NULL; //PDStringFromComplex(&entry);
        }
        count++;
    }
    pd_stack_set_global_preserve_flag(false);
    
    return arr;
}

void PDArrayClear(PDArrayRef array)
{
    while (array->count > 0)
        PDArrayDeleteAtIndex(array, array->count-1);
}

PDInteger PDArrayGetCount(PDArrayRef array)
{
    return array->count;
}

void *PDArrayGetElement(PDArrayRef array, PDInteger index)
{
    void *ctr = array->values[index];
    if (NULL == ctr) {
        if (array->vstacks[index] != NULL) {
            pd_stack entry = array->vstacks[index];
            pd_stack_set_global_preserve_flag(true);
            array->values[index] = PDInstanceCreateFromComplex(&entry);
            pd_stack_set_global_preserve_flag(false);
#ifdef PD_SUPPORT_CRYPTO
            if (array->values[index] && array->ci) 
                (*PDInstanceCryptoExchanges[PDResolve(array->values[index])])(array->values[index], array->ci, true);
#endif
        }
    }
    
    return array->values[index];
}

void *PDArrayGetTypedElement(PDArrayRef array, PDInteger index, PDInstanceType type)
{
    void *ctr = PDArrayGetElement(array, index);
    return PDResolve(ctr) == type ? ctr : NULL;
}

void PDArrayPushIndex(PDArrayRef array, PDInteger index) 
{
    if (array->count == array->capacity) {
        array->capacity += array->capacity + 1;
        array->values = realloc(array->values, sizeof(void *) * array->capacity);
        array->vstacks = realloc(array->vstacks, sizeof(pd_stack) * array->capacity);
    }
    
    for (PDInteger i = array->count; i > index; i--) {
        array->values[i] = array->values[i-1];
        array->vstacks[i] = array->vstacks[i-1];
    }
    
    array->values[index] = NULL;
    array->vstacks[index] = NULL;
    
    array->count++;
}

void PDArrayAppend(PDArrayRef array, void *value)
{
    PDArrayInsertAtIndex(array, array->count, value);
}

void PDArrayInsertAtIndex(PDArrayRef array, PDInteger index, void *value)
{
#ifdef PD_SUPPORT_CRYPTO
    if (array->ci) (*PDInstanceCryptoExchanges[PDResolve(value)])(value, array->ci, false);
#endif
    PDArrayPushIndex(array, index);
    array->values[index] = PDRetain(value);
}

PDInteger PDArrayGetIndex(PDArrayRef array, void *value)
{
    PDInteger obid, genid;
    PDInstanceType it = PDResolve(value);
    if (it == PDInstanceTypeObj) { 
        PDObjectRef ob = value;
        obid = ob->obid;
        genid = ob->genid;
    } else if (it == PDInstanceTypeRef) {
        PDReferenceRef ref = value;
        obid = ref->obid;
        genid = ref->genid;
    } else {
        for (PDInteger i = 0; i < array->count; i++) {
            if (value == PDArrayGetElement(array, i)) return i;
        }
        
        return -1;
    }
    
    void *v;
    PDReferenceRef ref2;
    PDObjectRef ob;
    for (PDInteger i = 0; i < array->count; i++) {
        v = PDArrayGetElement(array, i);
        ref2 = v;
        ob = v;
        
        it = PDResolve(v);
        if ((v == value) || 
            (it == PDInstanceTypeRef && ref2->obid == obid && ref2->genid == genid) ||
            (it == PDInstanceTypeObj && ob->obid == obid && ob->genid == genid))
            return i;
    }
    
    return -1;
}

void PDArrayDeleteAtIndex(PDArrayRef array, PDInteger index)
{
    PDRelease(array->values[index]);
    pd_stack_destroy(&array->vstacks[index]);
    array->count--;
    for (PDInteger i = index; i < array->count; i++) {
        array->values[i] = array->values[i+1];
        array->vstacks[i] = array->vstacks[i+1];
    }
}

void PDArrayReplaceAtIndex(PDArrayRef array, PDInteger index, void *value)
{
    PDRelease(array->values[index]);
    
    if (array->vstacks[index]) {
        pd_stack_destroy(&array->vstacks[index]);
        array->vstacks[index] = NULL;
    }
    
    array->values[index] = PDRetain(value);
#ifdef PD_SUPPORT_CRYPTO
    if (array->ci) (*PDInstanceCryptoExchanges[PDResolve(value)])(value, array->ci, false);
#endif
}

char *PDArrayToString(PDArrayRef array)
{
    PDInteger len = 4 + 10 * array->count;
    char *str = malloc(len);
    PDArrayPrinter(array, &str, 0, &len);
    return str;
}

void PDArrayPrint(PDArrayRef array)
{
    char *str = PDArrayToString(array);
    puts(str);
    free(str);
}

extern PDInteger PDArrayPrinter(void *inst, char **buf, PDInteger offs, PDInteger *cap)
{
    PDInstancePrinterInit(PDArrayRef, 0, 1);
    PDInteger len = 4 + 10 * i->count;
    PDInstancePrinterRequire(len, len);
    char *bv = *buf;
    bv[offs++] = '[';
    bv[offs++] = ' ';
    for (PDInteger j = 0; j < i->count; j++) {
        if (! i->values[j] && i->vstacks[j]) {
            PDArrayGetElement(i, j);
        }
        if (i->values[j]) {
            offs = (*PDInstancePrinters[PDResolve(i->values[j])])(i->values[j], buf, offs, cap);
        }
        PDInstancePrinterRequire(3, 3);
        bv = *buf;
        bv[offs++] = ' ';
    }
    bv[offs++] = ']';
    bv[offs] = 0;
    return offs;
}

#define encryptable(str) (strlen(str) > 0 && str[0] == '(' && str[strlen(str)-1] == ')')

#ifdef PD_SUPPORT_CRYPTO

void PDArrayAttachCrypto(PDArrayRef array, pd_crypto crypto, PDInteger objectID, PDInteger genNumber)
{
    array->ci = PDCryptoInstanceCreate(crypto, objectID, genNumber, NULL);
    for (PDInteger i = 0; i < array->count; i++) {
        if (array->values[i]) 
            (*PDInstanceCryptoExchanges[PDResolve(array->values[i])])(array->values[i], array->ci, false);
    }
}

void PDArrayAttachCryptoInstance(PDArrayRef array, PDCryptoInstanceRef ci, PDBool encrypted)
{
    array->ci = PDRetain(ci);
    for (PDInteger i = 0; i < array->count; i++) {
        if (array->values[i]) 
            (*PDInstanceCryptoExchanges[PDResolve(array->values[i])])(array->values[i], array->ci, false);
    }
}

#endif // PD_SUPPORT_CRYPTO
