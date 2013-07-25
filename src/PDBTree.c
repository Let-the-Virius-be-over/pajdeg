//
//  PDBTree.c
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

#include "PDDefines.h"
#include "PDInternal.h"
#include "PDBTree.h"

void *PDBTreeInsert(PDBTreeRef *root, long key, void *value)
{
    PDBTreeRef parent, ref;
    
    parent = NULL;
    ref = *root;
    
    while (ref && ref->key != key) {
        parent = ref;
        ref = ref->branch[ref->key > key];
    }
    if (ref) {
        void *replaced = ref->value;
        ref->value = value;
        return replaced;
    }
    
    ref = malloc(sizeof(struct PDBTree));
    ref->branch[0] = ref->branch[1] = NULL;
    ref->key = key;
    ref->value = value;
    if (parent) 
        parent->branch[parent->key > key] = ref;
    else
        *root = ref;
    
    return NULL;
}

void *PDBTreeFetch(PDBTreeRef root, long key)
{
    while (root && root->key != key)
        root = root->branch[root->key > key];
    return root ? root->value : NULL;
}

void *PDBTreeRemove(PDBTreeRef *root, long key)
{
    PDBTreeRef orphan = NULL;
    PDBTreeRef parent = NULL;
    PDBTreeRef ref = *root;
    PDBTreeRef rep;
    
    while (ref && ref->key != key) {
        parent = ref;
        ref = ref->branch[ref->key > key];
    }
    if (! ref) return NULL;
    
    if (! ref->branch[0]) {
        rep = ref->branch[1];
    } else {
        rep = ref->branch[0];
        orphan = ref->branch[1];
    }
    
    if (parent) 
        parent->branch[parent->key > key] = rep;
    else 
        parent = *root = rep;
    
    if (orphan) {
        PDBTreeInsert(&parent, orphan->key, orphan->value);
        free(orphan);
    }
    
    void *value = ref->value;
    free(ref);
    
    return value;
}

int PDBTreePopulateKeys(PDBTreeRef root, void **dest)
{
    PDBTreeRef r;
    int i = 0;
    while (root) {
        dest[i++] = (void*)root->key;
        r = root->branch[1];
        if (root->branch[0]) i = PDBTreePopulateKeys(root->branch[0], &dest[i]);
        root = r;
    }
    return i;
}

void PDBTreeDestroy(PDBTreeRef root)
{
    PDBTreeDestroyWithDeallocator(root, NULL);
}

void _PDBTreeDestroy(PDBTreeRef root, PDDeallocator deallocator)
{
    if (root->branch[0]) PDBTreeDestroyWithDeallocator(root->branch[0], deallocator);
    if (root->branch[1]) PDBTreeDestroyWithDeallocator(root->branch[1], deallocator);
    if (deallocator) (*deallocator)(root->value);
    free(root);
}

void PDBTreeDestroyWithDeallocator(PDBTreeRef root, PDDeallocator deallocator)
{
    // avoid NULL crashes
    if (root) _PDBTreeDestroy(root, deallocator);
}
