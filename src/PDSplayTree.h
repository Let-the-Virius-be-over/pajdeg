//
// PDSplayTree.h
//
// Based on the Wikipedia article on splay trees.
//

#ifndef INCLUDED_PDSPLAYTREE_H
#define INCLUDED_PDSPLAYTREE_H

#include <sys/types.h>

#include "PDDefines.h"

extern PDSplayTreeRef PDSplayTreeCreate(void);
extern PDSplayTreeRef PDSplayTreeCreateWithDeallocator(PDDeallocator deallocator);
extern void PDSplayTreeInsert(PDSplayTreeRef tree, PDInteger key, void *value);
extern void *PDSplayTreeGet(PDSplayTreeRef tree, PDInteger key);
extern void PDSplayTreeDelete(PDSplayTreeRef tree, PDInteger key);
extern PDInteger PDSplayTreeGetCount(PDSplayTreeRef tree);

#endif // INCLUDED_PDSPLAYTREE_H
