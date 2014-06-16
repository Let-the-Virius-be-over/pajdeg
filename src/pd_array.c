////
//// pd_array.c
////
//// Copyright (c) 2012 - 2014 Karl-Johan Alm (http://github.com/kallewoof)
//// 
//// This program is free software: you can redistribute it and/or modify
//// it under the terms of the GNU General Public License as published by
//// the Free Software Foundation, either version 3 of the License, or
//// (at your option) any later version.
//// 
//// This program is distributed in the hope that it will be useful,
//// but WITHOUT ANY WARRANTY; without even the implied warranty of
//// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//// GNU General Public License for more details.
//// 
//// You should have received a copy of the GNU General Public License
//// along with this program.  If not, see <http://www.gnu.org/licenses/>.
////
//
//#include "pd_array.h"
//#include "pd_stack.h"
//#include "pd_crypto.h"
//#include "pd_internal.h"
//#include "pd_pdf_implementation.h"
//#include "pd_container.h"
//
/////
///// Getters/setters
/////
//
//const char *pd_array_getter(void *arr, const void *k)
//{
//    return as(pd_array, arr)->values[as(PDInteger, k)];
//}
//
//const pd_stack pd_array_getter_raw(void *arr, const void *k)
//{
//    return as(pd_array, arr)->vstacks[as(PDInteger, k)];
//}
//
//#ifdef PD_SUPPORT_CRYPTO
//const char *pd_array_crypto_getter(void *arr, const void *k)
//{
//    pd_array array = arr;
//    PDCryptoInstanceRef ci = array->info;
//    char **plain = ci->info;
//    PDInteger index = as(PDInteger, k);
//
//    if (NULL == plain[index] && NULL != array->values[index]) {
//        char *encrypted = plain[index] = strdup(array->values[index]);
//        if (encryptable(encrypted)) {
//            pd_crypto_decrypt(ci->crypto, ci->obid, ci->genid, encrypted);
//        }
//    }
//    return plain[index];
//}
//#endif
//
/////
//
//void pd_array_remover(void *arr, const void *k)
//{
//    pd_array array = as(pd_array, arr);
//    PDInteger index = as(PDInteger, k);
//    
//    free(array->values[index]);
//    pd_stack_destroy(&array->vstacks[index]);
//    array->count--;
//    for (PDInteger i = index; i < array->count; i++) {
//        array->values[i] = array->values[i+1];
//        array->vstacks[i] = array->vstacks[i+1];
//    }
//}
//
//#ifdef PD_SUPPORT_CRYPTO
//void pd_array_crypto_remover(void *arr, const void *k)
//{
//    pd_array array = as(pd_array, arr);
//    PDCryptoInstanceRef ci = array->info;
//    PDInteger index = as(PDInteger, k);
//    char **plain = ci->info;
//    
//    free(array->values[index]);
//    pd_stack_destroy(&array->vstacks[index]);
//    free(plain[index]);
//    array->count--;
//    for (PDInteger i = index; i < array->count; i++) {
//        array->values[i] = array->values[i+1];
//        array->vstacks[i] = array->vstacks[i+1];
//        plain[i] = plain[i+1];
//    }
//}
//#endif
//
/////
//
//PDInteger pd_array_setter(void *arr, const void *k, const char *value)
//{
//    pd_array array = as(pd_array, arr);
//    PDInteger index = as(PDInteger, k);
//    
//    free(array->values[index]);
//    pd_stack_destroy(&array->vstacks[index]);
//    array->values[index] = strdup(value);
//    
//    return index;
//}
//
//#ifdef PD_SUPPORT_CRYPTO
//PDInteger pd_array_crypto_setter(void *arr, const void *k, const char *value)
//{
//    pd_array array = as(pd_array, arr);
//    PDCryptoInstanceRef ci = array->info;
//    PDInteger index = as(PDInteger, k);
//    char **plain = ci->info;
//    
//    free(array->values[index]);
//    pd_stack_destroy(&array->vstacks[index]);
//    free(plain[index]);
//
//    plain[index] = strdup(value);
//    char *decrypted = array->values[index] = strdup(value);
//    if (encryptable(decrypted)) {
//        pd_crypto_encrypt(ci->crypto, ci->obid, ci->genid, &array->values[index], decrypted, strlen(decrypted));
//        free(decrypted);
//    }
//    return index;
//}
//#endif
//
/////
//
//void pd_array_push_index(void *arr, PDInteger index)
//{
//    pd_array array = as(pd_array, arr);
//    
//    if (array->count == array->capacity) {
//        array->capacity += array->capacity + 1;
//        array->values = realloc(array->values, sizeof(char*) * array->capacity);
//        array->vstacks = realloc(array->vstacks, sizeof(pd_stack) * array->capacity);
//    }
//    
//    for (PDInteger i = array->count; i > index; i--) {
//        array->values[i] = array->values[i-1];
//        array->vstacks[i] = array->vstacks[i-1];
//    }
//    
//    array->values[index] = NULL;
//    array->vstacks[index] = NULL;
//    
//    array->count++;
//}
//
//#ifdef PD_SUPPORT_CRYPTO
//void pd_array_crypto_push_index(void *arr, PDInteger index)
//{
//    pd_array array = as(pd_array, arr);
//    PDCryptoInstanceRef ci = array->info;
//    char **plain = ci->info;
//    
//    if (array->count == array->capacity) {
//        array->capacity += array->capacity + 1;
//        array->values = realloc(array->values, sizeof(char*) * array->capacity);
//        array->vstacks = realloc(array->vstacks, sizeof(pd_stack) * array->capacity);
//        ci->info = plain = realloc(plain, sizeof(char*) * array->capacity);
//    }
//    
//    for (PDInteger i = array->count; i > index; i--) {
//        array->values[i] = array->values[i-1];
//        array->vstacks[i] = array->vstacks[i-1];
//        plain[i] = plain[i-1];
//    }
//
//    plain[index] = array->values[index] = NULL;
//    array->vstacks[index] = NULL;
//    
//    array->count++;
//}
//#endif
//
/////
///// Array code
/////
//
//pd_array pd_array_with_capacity(PDInteger capacity)
//{
//    if (capacity < 1) capacity = 1;
//    
//    pd_array arr = malloc(sizeof(struct pd_array));
//    arr->g = pd_array_getter;
//    arr->rg = pd_array_getter_raw;
//    arr->s = pd_array_setter;
//    arr->r = pd_array_remover;
//    arr->pi = pd_array_push_index;
//    arr->info = NULL;
//    arr->count = 0;
//    arr->capacity = capacity;
//    arr->values = malloc(sizeof(char*) * capacity);
//    arr->vstacks = malloc(sizeof(pd_stack) * capacity);
//    return arr;
//}
//
//void pd_array_clear(pd_array arr)
//{
//    while (arr->count > 0)
//        (*arr->r)(arr, (const void *)(arr->count-1));
//}
//
//void pd_array_destroy(pd_array array)
//{
//    for (PDInteger i = array->count-1; i >= 0; i--) {
//        free(array->values[i]);
//        free(array->vstacks[i]);
//    }
//    
//#ifdef PD_SUPPORT_CRYPTO
//    if (array->info) {
//        // we don't bother with signature juggling for this as it presumably happens relatively seldom (destruction of arrays, that is), compared to getting/setting
//        PDCryptoInstanceRef ci = array->info;
//        free(ci->info);
//        free(ci);
//    }
//#endif
//    
//    free(array->values);
//    free(array);
//}
//
//void pd_array_set_crypto(pd_array array, pd_crypto crypto, PDInteger objectID, PDInteger genNumber)
//{
//#ifdef PD_SUPPORT_CRYPTO
//    PDCryptoInstanceRef ci = PDCryptoInstanceCreate(crypto, objectID, genNumber, calloc(array->capacity, sizeof(char *)));
//    array->info = ci;
//    array->g = pd_array_crypto_getter;
//    array->s = pd_array_crypto_setter;
//    array->r = pd_array_crypto_remover;
//    array->pi = pd_array_crypto_push_index;
//#endif
//}
//
//pd_array pd_array_from_stack(pd_stack stack)
//{
//    if (stack == NULL) {
//        // empty array
//        return pd_array_with_capacity(1);
//    }
//
//    // determine size of stack
//    PDInteger count;
//    pd_stack s = stack;
//    pd_stack entry;
//    
//    for (count = 0; s; s = s->prev)
//        count++;
//    pd_array arr = pd_array_with_capacity(count);
//    arr->count = count;
//    
//    s = stack;
//    pd_stack_set_global_preserve_flag(true);
//    for (count = 0; s; s = s->prev) {
//        if (s->type == PD_STACK_STRING) {
//            arr->values[count] = strdup(s->info);
//            arr->vstacks[count] = NULL;
//        } else {
//            arr->vstacks[count] = entry = pd_stack_copy(s->info);
//            arr->values[count] = PDStringFromComplex(&entry);
//        }
//        count++;
//    }
//    pd_stack_set_global_preserve_flag(false);
//    
//    return arr;
//}
//
//pd_stack pd_array_to_stack(pd_array array)
//{
//    pd_stack result = NULL;
//    for (PDInteger i = array->count-1; i >= 0; i--) {
//        if (array->vstacks[i])
//            pd_stack_push_stack(&result, pd_stack_copy(array->vstacks[i]));
//        else
//            pd_stack_push_key(&result, strdup((*array->g)(array, (const void *)i)));
//    }
//    return result;
//}
//
//pd_array pd_array_from_pdf_array_stack(pd_stack stack)
//{
//    if (stack == NULL) {
//        // empty array
//        return pd_array_with_capacity(1);
//    }
//    
//    /*
//stack<0x14cdde90> {
//   0x401394 ("array")
//   0x401388 ("entries")
//    stack<0x14cdeb90> {
//        stack<0x14cdead0> {
//           0x401398 ("ae")
//           0
//        }
//    }
//}
//     */
//    if (PDIdentifies(stack->info, PD_ARRAY)) {
//        stack = stack->prev->prev->info;
//    }
//    /*
//    stack<0x14cdeb90> {
//        stack<0x14cdead0> {
//           0x401398 ("ae")
//           0
//        }
//    }
//     */
//    
//    // determine size of stack
//    PDInteger count;
//    pd_stack s = stack;
//    pd_stack entry;
//    
//    for (count = 0; s; s = s->prev)
//        count++;
//    pd_array arr = pd_array_with_capacity(count);
//    arr->count = count;
//    
//    s = stack;
//    pd_stack_set_global_preserve_flag(true);
//    for (count = 0; s; s = s->prev) {
//        entry = as(pd_stack, s->info)->prev;
//        if (entry->type == PD_STACK_STRING) {
//            arr->values[count] = strdup(entry->info);
//            arr->vstacks[count] = NULL;
//        } else {
//            arr->vstacks[count] = entry = pd_stack_copy(entry->info);
//            arr->values[count] = PDStringFromComplex(&entry);
//        }
//        count++;
//    }
//    pd_stack_set_global_preserve_flag(false);
//    
//    return arr;
//}
//
//pd_stack pd_stack_from_array(pd_array array)
//{
//    pd_stack stack = NULL;
//    for (PDInteger i = 0; i < array->count; i++) 
//        pd_stack_push_key(&stack, array->values[i]);
//    return stack;
//}
//
//PDInteger pd_array_get_count(pd_array array)
//{
//    return array->count;
//}
//
//const char *pd_array_get_at_index(pd_array array, PDInteger index)
//{
//    return (*array->g)(array, (const void*)index);
//}
//
//PDInteger pd_array_get_index_of_value(pd_array array, const char *value)
//{
//    for (PDInteger i = 0; i < array->count; i++)
//        if (0 == strcmp(value, (*array->g)(array, (const void*)i))) 
//            return i;
//    return -1;
//}
//
//pd_stack pd_array_get_raw_at_index(pd_array array, PDInteger index)
//{
//    return (*array->rg)(array, (const void*)index);
//}
//
//PDObjectType pd_array_get_type_at_index(pd_array array, PDInteger index)
//{
//    pd_stack stack = pd_array_get_raw_at_index(array, index);
//    if (NULL == stack) {
//        const char *str = pd_array_get_at_index(array, index);
//        return str ? PDObjectTypeString : PDObjectTypeNull;
//    }
//    return pd_container_determine_type(stack);
//}
//
//void *pd_array_get_copy_at_index(pd_array array, PDInteger index)
//{
//    pd_stack stack = pd_array_get_raw_at_index(array, index);
//    if (NULL == stack) 
//        return strdup(pd_array_get_at_index(array, index));
//    return pd_container_spawn(stack);
//}
//
//const char **pd_array_create_args(pd_array array)
//{
//    if (array->count == 0) return NULL;
//    const char **result = malloc(sizeof(char*) * array->count);
//    for (PDInteger i = 0; i < array->count; i++) 
//        result[i] = (*array->g)(array, (const void*)i);
//    return result;
//}
//
//void pd_array_insert_at_index(pd_array array, PDInteger index, const char *value)
//{
//    // make room first
//    (*array->pi)(array, index);
//    // then insert
//    (*array->s)(array, (const void *)index, value);
//}
//
//void pd_array_append(pd_array array, const char *value)
//{
//    PDInteger index = array->count;
//    // make room first
//    (*array->pi)(array, index);
//    // then insert
//    (*array->s)(array, (const void *)index, value);
//}
//
//void pd_array_remove_at_index(pd_array array, PDInteger index)
//{
//    (*array->r)(array, (const void *)index);
//}
//
//void pd_array_replace_at_index(pd_array array, PDInteger index, const char *value)
//{
//    (*array->s)(array, (const void *)index, value);
//}
//
//char *pd_array_to_string(pd_array array)
//{
//    PDInteger len = 4; // [ ]\0
//    for (PDInteger i = array->count-1; i >= 0; i--)
//        len += 1 + strlen(array->values[i]);
//    char *str = malloc(len);
//    str[0] = '[';
//    len = 1;
//    for (PDInteger i = 0; i < array->count; i++) {
//        str[len++] = ' ';
//        strcpy(&str[len], array->values[i]);
//        len += strlen(array->values[i]);
//    }
//    str[len++] = ' ';
//    str[len++] = ']';
//    str[len] = 0;
//    return str;
//}
//
//void pd_array_print(pd_array array)
//{
//    printf("(array %p; count = %ld) {\n", array, (long)array->count);
//    for (PDInteger i = 0; i < array->count; i++) {
//        printf("\t#%ld: %s\n", (long)i, pd_array_get_at_index(array, i));
//    }
//    printf("}\n");
//}
