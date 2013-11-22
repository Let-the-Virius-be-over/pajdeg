//
// pd_array.h
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

/**
 @file pd_array.h Array header file.
 
 @ingroup pd_array
 
 @defgroup pd_array pd_array
 
 @brief Low-performance convenience array implementation.
 
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
 Free up an array.
 
 @param array The array to delete. 
 */
extern void pd_array_destroy(pd_array array);

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

#endif

/** @} */
