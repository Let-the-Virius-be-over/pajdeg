//
// pd_array.h
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

/**
 @file pd_array.h Array header file.
 
 @ingroup pd_array
 
 @defgroup pd_array pd_array
 
 @brief Low-performance convenience array implementation with optional crypto support.
 
 This is a crude implementation of the array structure. If a crypto object is supplied, the array will transparently supply decrypted values for entries on demand, and will encrypt entries supplied.
 
 Currently, entries are only encrypted if they are found to be strings, i.e. if they are "(wrapped within parentheses)".
 
 @ingroup PDALGO
 
 The pd_array object is a convenience wrapper for stacks. It supports converting to and from stack representations and has functionality normally found in arrays. 
 
 @{
 */


#ifndef INCLUDED_PD_ARRAY_H
#define INCLUDED_PD_ARRAY_H

#include <sys/types.h>
#include "PDDefines.h"

/**
 Create a new, empty array with the given capacity.
 
 @param capacity The number of slots to allocate for added elements.
 @return Empty array.
 */
extern pd_array pd_array_with_capacity(PDInteger capacity);

/**
 *  Convert a pd_array into a pd_stack object
 *
 *  @param array The array to convert
 *
 *  @return A pd_stack fitting as input to e.g. pd_array_from_stack()
 */
extern pd_stack pd_array_to_stack(pd_array array);

/**
 Clear the given array, removing all entries.
 
 @param arr The array.
 */
extern void pd_array_clear(pd_array arr);

/**
 Free up an array.
 
 @param array The array to delete. 
 */
extern void pd_array_destroy(pd_array array);

/**
 Supply a crypto object to an array, and associate the array with a specific object. 
 
 The array will swap out its g, s, pi pointers to crypto-enabled ones. It will also instantiate a crypto instance for holding parameters used in crypto.
 
 @param array The array.
 @param crypto The pd_crypt object.
 @param objectID The object ID of the owning object.
 @param genNumber Generation number of the owning object.
 */
extern void pd_array_set_crypto(pd_array array, pd_crypto crypto, PDInteger objectID, PDInteger genNumber);

/**
 Converts a stack into an array.
 
 This simply takes each element as is. To convert a PDF array stack representation into a pd_array, use pd_array_from_pdf_array_stack().
 
 @param stack The stack.
 @return pd_array representation of the stack.
 */
extern pd_array pd_array_from_stack(pd_stack stack);

/**
 Converts a PDF array stack into an array.
 
 @param stack The PDF array stack.
 @return pd_array representation of the stack.
 */
extern pd_array pd_array_from_pdf_array_stack(pd_stack stack);

/**
 Converts a pd_array into a stack.
 
 @param array The array to convert.
 @return A pd_stack representation of the array.
 */
extern pd_stack pd_stack_from_array(pd_array array);

/**
 Returns number of elements in the array.
 
 @param array The array whose element count is to be returned.
 @return The number of elements.
 */
extern PDInteger pd_array_get_count(pd_array array);

/**
 Return the value of the element at the given index.
 
 @param array The array.
 @param index The index of the element.
 @return Value.
 */
extern const char *pd_array_get_at_index(pd_array array, PDInteger index);

/**
 Return the raw value of the element at the given index.
 
 @note The raw value is NULL if it's a native string.
 
 @param array The array.
 @param index The index of the element.
 @return Raw value as pd_stack.
 */
extern pd_stack pd_array_get_raw_at_index(pd_array array, PDInteger index);

/**
 *  Get the type of the given element.
 *
 *  @param array The array
 *  @param index Element index
 *
 *  @return PDObjectType enum value.
 */
extern PDObjectType pd_array_get_type_at_index(pd_array array, PDInteger index);

/**
 *  Obtain a copy of the element at the given index.
 *
 *  The type of the returned object can be determined using pd_dict_get_type, and maps to the following types:
 *  - PDObjectTypeNull       -> NULL
 *  - PDObjectTypeUnknown    -> (not applicable)
 *  - PDObjectTypeBoolean    -> PDBool [1]
 *  - PDObjectTypeInteger    -> PDInteger [1] 
 *  - PDObjectTypeReal       -> PDReal [1]
 *  - PDObjectTypeName       -> const char *
 *  - PDObjectTypeString     -> const char *
 *  - PDObjectTypeArray      -> pd_array
 *  - PDObjectTypeDictionary -> pd_dict
 *  - PDObjectTypeStream     -> (not applicable)
 *  - PDObjectTypeReference  -> PDReference
 *
 *  [1] These all map to PDObjectTypeString, in the current implementation.
 *
 *  @note Making changes to the copy naturally does not affect the dictionary.
 *  @note The returned value must be released or freed accordingly.
 *
 *  @param array The array
 *  @param index Element index
 *
 *  @return A copy of the appropriate object
 */
extern void *pd_array_get_copy_at_index(pd_array array, PDInteger index);

/**
 Return entire array as a newly allocated char* array which must be free()'d. Its elements, however, must NOT be freed.
 
 @warning The returned array should be freed, but its elements shouldn't be as they're still used internally in the array.
 
 @param array The array whose entries should be returned.
 */
extern const char **pd_array_create_args(pd_array array);

/**
 Append an element to the end of the array.
 
 @param array The array.
 @param value The value to append.
 */
extern void pd_array_append(pd_array array, const char *value);

/**
 Insert an element at the given index.
 
 @param array The array.
 @param index The index at which the element should be inserted.
 @param value The element value.
 */
extern void pd_array_insert_at_index(pd_array array, PDInteger index, const char *value);

/**
 *  Get the index of the given string value, or -1 if the value is not in the array at all.
 *
 *  @param array Array to search through
 *  @param value Value to search for
 *
 *  @return -1 if not found, otherwise the index of the first encounter of the given value
 */
extern PDInteger pd_array_get_index_of_value(pd_array array, const char *value);

/**
 Delete element at the given index.
 
 @param array The array.
 @param index The index of the element to delete.
 */
extern void pd_array_remove_at_index(pd_array array, PDInteger index);

/**
 Replace element at the given index with another element.
 
 @param array The array.
 @param index The index of the element to replace.
 @param value The value to replace the element with.
 */
extern void pd_array_replace_at_index(pd_array array, PDInteger index, const char *value);

/**
 Convert the array into a string value formatted as a PDF array, including the brackets. The returned string must be freed.
 
 @param array The array.
 */
extern char *pd_array_to_string(pd_array array);

extern void pd_array_print(pd_array array);

#define encryptable(str) (strlen(str) > 0 && str[0] == '(' && str[strlen(str)-1] == ')')

#endif

/** @} */

