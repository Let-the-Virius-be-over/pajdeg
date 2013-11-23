//
// pd_dict.c
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

#include "pd_dict.h"
#include "pd_internal.h"
#include "pd_stack.h"
#include "pd_pdf_implementation.h"

pd_dict pd_dict_with_capacity(PDInteger capacity)
{
    pd_dict dict = malloc(sizeof(struct pd_dict));
    dict->capacity = capacity;
    dict->count = 0;
    dict->keys = malloc(sizeof(char*) * capacity);
    dict->values = malloc(sizeof(char*) * capacity);
    return dict;
}

void pd_dict_destroy(pd_dict dict)
{
    for (PDInteger i = dict->count-1; i >= 0; i--) {
        free(dict->keys[i]);
        free(dict->values[i]);
    }
    free(dict->keys);
    free(dict->values);
    free(dict);
}

pd_dict pd_dict_from_pdf_dict_stack(pd_stack stack)
{
    if (stack == NULL) {
        // empty dictionary
        return pd_dict_with_capacity(1);
    }
    
    // we may have the DICT identifier
    if (PDIdentifies(stack->info, PD_DICT)) {
        stack = stack->prev->prev->info;
    }
    
    /*
 stack<0x1136ba20> {
   0x3f9998 ("de")
   Kids
    stack<0x11368f20> {
       0x3f999c ("array")
       0x3f9990 ("entries")
        stack<0x113ea650> {
            stack<0x113c5c10> {
               0x3f99a0 ("ae")
                stack<0x113532b0> {
                   0x3f9988 ("ref")
                   557
                   0
                }
            }
            stack<0x113d49b0> {
               0x3f99a0 ("ae")
                stack<0x113efa50> {
                   0x3f9988 ("ref")
                   558
                   0
                }
            }
            stack<0x113f3c40> {
               0x3f99a0 ("ae")
                stack<0x1136df80> {
                   0x3f9988 ("ref")
                   559
                   0
                }
            }
            stack<0x113585b0> {
               0x3f99a0 ("ae")
                stack<0x11368e30> {
                   0x3f9988 ("ref")
                   560
                   0
                }
            }
            stack<0x1135df20> {
               0x3f99a0 ("ae")
                stack<0x113f3470> {
                   0x3f9988 ("ref")
                   1670
                   0
                }
            }
        }
    }
}
     */
    
    // determine size of stack
    PDInteger count;
    pd_stack s = stack;
    pd_stack entry;
    
    for (count = 0; s; s = s->prev)
        count++;
    pd_dict dict = pd_dict_with_capacity(count);
    dict->count = count;
    
    s = stack;
    pd_stack_set_global_preserve_flag(true);
    for (count = 0; s; s = s->prev) {
        entry = as(pd_stack, s->info)->prev;
        // entry must be a string; it's the key value
        dict->keys[count] = strdup(entry->info);
        entry = entry->prev;
        if (entry->type == PD_STACK_STRING) {
            dict->values[count] = strdup(entry->info);
        } else {
            entry = entry->info;
            dict->values[count] = PDStringFromComplex(&entry);
        }
        count++;
    }
    pd_stack_set_global_preserve_flag(false);
    
    return dict;
}

PDInteger pd_dict_get_count(pd_dict dict)
{
    return dict->count;
}

#define pd_dict_fetch_i(dict, key) \
    PDInteger i; \
    for (i = 0; i < dict->count; i++) \
        if (0 == strcmp(key, dict->keys[i])) \
            break

const char *pd_dict_get(pd_dict dict, const char *key)
{
    pd_dict_fetch_i(dict, key);
    return (i == dict->count ? NULL : dict->values[i]);
}

void pd_dict_remove(pd_dict dict, const char *key)
{
    pd_dict_fetch_i(dict, key);
    if (i == dict->count) return;
    
    free(dict->values[i]);
    free(dict->keys[i]);
    
    dict->count--;
    for (PDInteger j = i; j < dict->count; j++) {
        dict->keys[j] = dict->keys[j+1];
        dict->values[j] = dict->values[j+1];
    }
}

void pd_dict_set(pd_dict dict, const char *key, const char *value)
{
    pd_dict_fetch_i(dict, key);
    if (i == dict->count) {
        if (dict->count == dict->capacity) {
            dict->capacity += dict->capacity + 1;
            dict->values = realloc(dict->values, sizeof(char*) * dict->capacity);
            dict->keys = realloc(dict->keys, sizeof(char*) * dict->capacity);
        }
        dict->keys[dict->count] = strdup(key);
        dict->values[dict->count++] = strdup(value);
    } else {
        free(dict->values[i]);
        dict->values[i] = strdup(value);
    }
}

const char **pd_dict_keys(pd_dict dict)
{
    return (const char **) dict->keys;
}

const char **pd_dict_values(pd_dict dict)
{
    return (const char **) dict->values;
}

char *pd_dict_to_string(pd_dict dict)
{
    PDInteger len = 6; // << >>\0
    for (PDInteger i = dict->count-1; i >= 0; i--)
        len += 3 + strlen(dict->values[i]) + strlen(dict->keys[i]);
    char *str = malloc(len);
    str[0] = str[1] = '<';
    len = 2;
    for (PDInteger i = 0; i < dict->count; i++) {
        str[len++] = ' ';
        str[len++] = '/';
        strcpy(&str[len], dict->keys[i]);
        len += strlen(dict->keys[i]);
        str[len++] = ' ';
        strcpy(&str[len], dict->values[i]);
        len += strlen(dict->values[i]);
    }
    str[len++] = ' ';
    str[len++] = '>';
    str[len++] = '>';
    str[len] = 0;
    return str;
}
