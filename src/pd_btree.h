//
// pd_btree.h
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
 @file pd_btree.h Binary tree header file.
 
 A simple binary tree implementation.
 
 @ingroup PDBTREE
 
 @defgroup PDBTREE pd_btree
 
 @brief Binary tree implementation, mostly working like a normal binary tree.
 
 @ingroup PDALGO

 @note This implementation currently does not balance the tree as this seemed beyond the scope of the needs.
 
 @note Keys are restricted to primitives, as this is sufficient for Pajdeg's needs. Inserting "foo" and then testing for "foo" will not be successful, unless the two strings point at the same memory address. 
 
 @{
 */

#ifndef INCLUDED_pd_btree_h
#define INCLUDED_pd_btree_h

#include "PDDefines.h"
#if 0
/**
 Inserts entry for key. 
 
 @param root  Pointer to tree into which insertion should be made.
 @param key   The key, as a long value.
 @param value The value.
 
 @return Whatever value the key had before the insertion.
 */
extern void *pd_btree_insert(pd_btree *root, long key, void *value);

/**
 Remove entry for key, returning its value (if any).
 
 @param root    Pointer to the tree from which removal should be made.
 @param key     The key to remove.
 */
extern void *pd_btree_remove(pd_btree *root, long key);

/**
 Fetch value for an entry.
 
 @param root    The tree in which the value exists.
 @param key     The key to locate.
 */
extern void *pd_btree_fetch(pd_btree root, long key);

/**
 Dump all keys (not values) into preallocated array. 
 @return Number of keys added.
 */
extern PDInteger pd_btree_populate_keys(pd_btree root, void **dest);

/**
 Destroy a tree, without touching value.
 */
extern void pd_btree_destroy(pd_btree root);

/**
 Destroy a tree, calling deallocator on the value for each element.
 */
extern void pd_btree_destroy_with_deallocator(pd_btree root, PDDeallocator deallocator);

#endif

/* @} */
#endif
