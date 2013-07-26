//
//  PDBTree.h
//
//  Copyright (c) 2013 Karl-Johan Alm (http://github.com/kallewoof)
// 
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
// 
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
// 
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//

/**
 @defgroup BTREE_GRP Binary Tree
 
 @brief The PDBTree mostly works like a normal binary tree. See notes. 
 
 @note This implementation currently does not balance the tree as this seemed beyond the scope of the needs.
 
 @note Keys are restricted to primitives, as this is sufficient for Pajdeg's needs. Inserting "foo" and then testing for "foo" will not be successful, unless the two strings point at the same memory address. 
 
 @{
 */

#ifndef INCLUDED_PDBTree_h
#define INCLUDED_PDBTree_h

#include "PDDefines.h"

/**
 Inserts entry for key. 
 
 @param root  Pointer to tree into which insertion should be made.
 @param key   The key, as a long value.
 @param value The value.
 
 @return Whatever value the key had before the insertion.
 */
extern void *PDBTreeInsert(PDBTreeRef *root, long key, void *value);

/**
 Remove entry for key, returning its value (if any).
 
 @param root    Pointer to the tree from which removal should be made.
 @param key     The key to remove.
 */
extern void *PDBTreeRemove(PDBTreeRef *root, long key);

/**
 Fetch value for an entry.
 
 @param root    The tree in which the value exists.
 @param key     The key to locate.
 */
extern void *PDBTreeFetch(PDBTreeRef root, long key);

/**
 Dump all keys (not values) into preallocated array. 
 @return Number of keys added.
 */
extern PDInteger PDBTreePopulateKeys(PDBTreeRef root, void **dest);

/**
 Destroy a tree, without touching value.
 */
extern void PDBTreeDestroy(PDBTreeRef root);

/**
 Destroy a tree, calling deallocator on the value for each element.
 */
extern void PDBTreeDestroyWithDeallocator(PDBTreeRef root, PDDeallocator deallocator);

#endif

/* @} */
