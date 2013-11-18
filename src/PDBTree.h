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

typedef struct bt_node *bt_node;

extern PDBTreeRef PDBTreeCreate(PDDeallocator deallocator, PDInteger expectedMinimum, PDInteger expectedMaximum, PDInteger intensity);

extern void *PDBTreeGet(PDBTreeRef btree, PDInteger key);

extern void PDBTreeInsert(PDBTreeRef btree, PDInteger key, void *value);

extern void PDBTreeDelete(PDBTreeRef btree, PDInteger key);

extern PDInteger PDBTreeGetCount(PDBTreeRef btree);

/**
 Dump all keys (not values) into preallocated array. 
 
 @warning If dest is not able to hold all entries, memory error will occur.

 @param tree The tree whose keys should be stored in the destination.
 @param dest The preallocated keys array.
 @return Number of keys added.
 */
extern PDInteger PDBTreePopulateKeys(PDBTreeRef btree, PDInteger *dest);

#endif
