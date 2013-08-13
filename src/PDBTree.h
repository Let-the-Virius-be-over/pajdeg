//
//  PDBTree.h
//  Infinite PDF
//
//  Created by Karl-Johan Alm on 8/11/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
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
