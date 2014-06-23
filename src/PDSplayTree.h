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

/**
 *  Dump all keys (not values) into preallocated array. 
 * 
 *  @warning If dest is not able to hold all entries, memory error will occur.
 * 
 *  @param tree The tree whose keys should be stored in the destination.
 *  @param dest The preallocated keys array.
 *  @return Number of keys added.
 */
extern PDInteger PDSplayTreePopulateKeys(PDSplayTreeRef tree, PDInteger *dest);

/**
 *  Convert a (short) string into a key using simple arithmetics.
 *
 *  @warning Keys with a length greater than 4 characters are effectively truncated down to 4 characters.
 *
 *  @param str String to convert
 *  @param len Length of string
 *  @return Binary tree integer value for string
 */
#define PDST_KEY_STR(str, len) \
    (PDInteger) (\
        str[0]\
        + ((str[1*(len>0)]*(len>0)) << 8)\
        + ((str[2*(len>1)]*(len>1)) << 16)\
        + ((str[3*(len>2)]*(len>2)) << 24)\
    )

#endif // INCLUDED_PDSPLAYTREE_H