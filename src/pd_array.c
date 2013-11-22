//
// pd_array.c
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

#include "pd_array.h"
#include "pd_stack.h"
#include "pd_internal.h"
#include "pd_pdf_implementation.h"

pd_array pd_array_with_capacity(PDInteger capacity)
{
    pd_array arr = malloc(sizeof(struct pd_array));
    arr->count = 0;
    arr->capacity = capacity;
    arr->values = malloc(sizeof(char*) * capacity);
    return arr;
}

void pd_array_destroy(pd_array array)
{
    for (PDInteger i = array->count-1; i >= 0; i--)
        free(array->values[i]);
    free(array->values);
    free(array);
}

pd_array pd_array_from_stack(pd_stack stack)
{
    if (stack == NULL) {
        // empty array
        return pd_array_with_capacity(1);
    }

    // determine size of stack
    PDInteger count;
    pd_stack s = stack;
    pd_stack entry;
    
    for (count = 0; s; s = s->prev)
        count++;
    pd_array arr = pd_array_with_capacity(count);
    
    s = stack;
    pd_stack_set_global_preserve_flag(true);
    for (count = 0; s; s = s->prev) {
        if (s->type == PD_STACK_STRING) {
            arr->values[count] = strdup(s->info);
        } else {
            entry = s->info;
            arr->values[count] = PDStringFromComplex(&entry);
        }
    }
    pd_stack_set_global_preserve_flag(false);
    
    return arr;
}

pd_array pd_array_from_pdf_array_stack(pd_stack stack)
{
    if (stack == NULL) {
        // empty array
        return pd_array_with_capacity(1);
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
    pd_array arr = pd_array_with_capacity(count);
    arr->count = count;
    
    s = stack;
    pd_stack_set_global_preserve_flag(true);
    for (count = 0; s; s = s->prev) {
        entry = as(pd_stack, s->info)->prev;
        if (entry->type == PD_STACK_STRING) {
            arr->values[count] = strdup(entry->info);
        } else {
            entry = entry->info;
            arr->values[count] = PDStringFromComplex(&entry);
        }
        count++;
    }
    pd_stack_set_global_preserve_flag(false);
    
    return arr;
}

pd_stack pd_stack_from_array(pd_array array)
{
    pd_stack stack = NULL;
    for (PDInteger i = 0; i < array->count; i++) 
        pd_stack_push_key(&stack, array->values[i]);
    return stack;
}

PDInteger pd_array_get_count(pd_array array)
{
    return array->count;
}

const char *pd_array_get_at_index(pd_array array, PDInteger index)
{
    return array->values[index];
}

#define pd_array_prepare_insertion(array) \
    if (array->count == array->capacity) {\
        array->capacity += array->capacity + 1;\
        array->values = realloc(array->values, sizeof(char*) * array->capacity);\
    }

void pd_array_append(pd_array array, const char *value)
{
    pd_array_prepare_insertion(array);
    array->values[array->count++] = strdup(value);
}

void pd_array_insert_at_index(pd_array array, PDInteger index, const char *value)
{
    pd_array_prepare_insertion(array);
    for (PDInteger i = array->count; i > index; i--)
        array->values[i] = array->values[i-1];
    array->values[index] = strdup(value);
    array->count++;
}

void pd_array_remove_at_index(pd_array array, PDInteger index)
{
    free(array->values[index]);
    array->count--;
    for (PDInteger i = index; i < array->count; i++)
        array->values[i] = array->values[i+1];
}

void pd_array_replace_at_index(pd_array array, PDInteger index, const char *value)
{
    free(array->values[index]);
    array->values[index] = strdup(value);
}

char *pd_array_to_string(pd_array array)
{
    PDInteger len = 4; // [ ]\0
    for (PDInteger i = array->count-1; i >= 0; i--)
        len += 1 + strlen(array->values[i]);
    char *str = malloc(len);
    str[0] = '[';
    len = 1;
    for (PDInteger i = 0; i < array->count; i++) {
        str[len++] = ' ';
        strcpy(&str[len], array->values[i]);
        len += strlen(array->values[i]);
    }
    str[len++] = ' ';
    str[len++] = ']';
    str[len] = 0;
    return str;
}
