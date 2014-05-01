//
// PDBTree.h
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

#ifndef Infinite_PDF_btree_h
#define Infinite_PDF_btree_h

#include <sys/types.h>

#include "PDDefines.h"

/**
 *  Create a new binary tree.
 *
 *  @param deallocator     The deallocator for tree entries.
 *  @param expectedMinimum Expected minimum value.
 *  @param expectedMaximum Expected maximum value.
 *  @param intensity       Intensity (or density) of the tree.
 *
 *  @return A new tree with given parameters.
 */
extern PDBTreeRef PDBTreeCreate(PDDeallocator deallocator, PDInteger expectedMinimum, PDInteger expectedMaximum, PDInteger intensity);

/**
 *  Get entry from binary tree for given key.
 *
 *  @param btree The binary tree
 *  @param key   The key
 */
extern void *PDBTreeGet(PDBTreeRef btree, PDInteger key);

/**
 *  Insert value into binary tree for given key, replacing old value if already existing
 *
 *  @param btree The binary tree
 *  @param key   The key
 *  @param value The value
 */
extern void PDBTreeInsert(PDBTreeRef btree, PDInteger key, void *value);

/**
 *  Delete value for given key from binary tree
 *
 *  @param btree The binary tree
 *  @param key   The key whose value should be removed
 */
extern void PDBTreeDelete(PDBTreeRef btree, PDInteger key);

/**
 *  Get the number of entries in the binary tree
 *
 *  @param btree The binary tree
 *
 *  @return The number of entries in the tree
 */
extern PDInteger PDBTreeGetCount(PDBTreeRef btree);

/**
 *  Dump all keys (not values) into preallocated array. 
 * 
 *  @warning If dest is not able to hold all entries, memory error will occur.
 * 
 *  @param tree The tree whose keys should be stored in the destination.
 *  @param dest The preallocated keys array.
 *  @return Number of keys added.
 */
extern PDInteger PDBTreePopulateKeys(PDBTreeRef btree, PDInteger *dest);

/**
 *  Print tree to stdout.
 */
extern void PDBTreePrint(PDBTreeRef btree);

/**
 *  Convert a (short) string into a key using simple arithmetics.
 *
 *  @warning Keys with a length greater than 4 characters are effectively truncated down to 4 characters.
 *
 *  @param str String to convert
 *  @param len Length of string
 *  @return Binary tree integer value for string
 */
#define PDBT_KEY_STR(str, len) \
    (PDInteger) (\
        str[0]\
        + ((str[1*(len>0)]*(len>0)) << 8)\
        + ((str[2*(len>1)]*(len>1)) << 16)\
        + ((str[3*(len>2)]*(len>2)) << 24)\
    )

#endif
