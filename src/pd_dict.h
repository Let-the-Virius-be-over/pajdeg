//
// pd_dict.h
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
 @file pd_dict.h Dictionary header file.
 
 @ingroup pd_dict
 
 @defgroup pd_dict pd_dict
 
 @brief Low-performance convenience dictionary implementation.
 
 @ingroup PDALGO
 
 The pd_dict object is a convenience wrapper for PDF dictionary stacks. It supports converting to and from PDF dictionary stack representations and has functionality normally found in dictionaries. 
 
 @{
 */


#ifndef INCLUDED_PD_DICT_H
#define INCLUDED_PD_DICT_H

#include <sys/types.h>
#include "PDDefines.h"

/**
 Create a new, empty dictionary with the given capacity.
 
 @param capacity The number of slots to allocate for added entries.
 @return Empty dictionary.
 */
extern pd_dict pd_dict_with_capacity(PDInteger capacity);

/**
 Free up a dictionary.
 
 @param dict The dict to delete. 
 */
extern void pd_dict_destroy(pd_dict dict);

/**
 Supply a crypto object to a dictionary, and associate the dict with a specific object. 
 
 The dict will swap out its g, s, r pointers to crypto-enabled ones. It will also instantiate a crypto instance for holding parameters used in crypto.
 
 @param dict The dict.
 @param crypto The pd_crypt object.
 @param objectID The object ID of the owning object.
 @param genNumber Generation number of the owning object.
 */
extern void pd_dict_set_crypto(pd_dict dict, pd_crypto crypto, PDInteger objectID, PDInteger genNumber);

/**
 Converts a PDF dict stack into a dictionary.
 
 @param stack The PDF dict stack.
 @return pd_dict representation of the stack.
 */
extern pd_dict pd_dict_from_pdf_dict_stack(pd_stack stack);

/**
 Returns number of entries in the dict.
 
 @param dict The dict whose entry count is to be returned.
 @return The number of entries.
 */
extern PDInteger pd_dict_get_count(pd_dict dict);

/**
 Return the value of the entry for the given key. O(n)!
 
 @param dict The dict.
 @param key The key.
 @return Value.
 */
extern const char *pd_dict_get(pd_dict dict, const char *key);

/**
 Delete entry for the given key. O(n)!
 
 @param dict The dict.
 @param key The key.
 */
extern void pd_dict_remove(pd_dict dict, const char *key);

/**
 Set entry for the given key to the given value. O(n)!
 
 @param dict The dict.
 @param key The key.
 @param value The value to set or replace the entry to/with.
 */
extern void pd_dict_set(pd_dict dict, const char *key, const char *value);

/**
 Get the keys of the dictionary as an array of strings.
 
 @param dict The dict.
 */
extern const char **pd_dict_keys(pd_dict dict);

/**
 Get the values of the dictionary as an array of strings.
 
 @param dict The dict.
 */
extern const char **pd_dict_values(pd_dict dict);

/**
 Convert the dict into a string value formatted as a PDF dict, including the dictionary symbols. The returned string must be freed.
 
 @param dict The dict.
 */
extern char *pd_dict_to_string(pd_dict dict);

#endif

/** @} */
