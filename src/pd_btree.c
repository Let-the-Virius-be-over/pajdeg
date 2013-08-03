//
//  pd_btree.c
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
#include "pd_internal.h"
#include "pd_btree.h"

void *pd_btree_insert(pd_btree *root, long key, void *value)
{
    pd_btree parent, ref;
    
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
    
    ref = malloc(sizeof(struct pd_btree));
    ref->branch[0] = ref->branch[1] = NULL;
    ref->key = key;
    ref->value = value;
    if (parent) 
        parent->branch[parent->key > key] = ref;
    else
        *root = ref;
    
    return NULL;
}

void *pd_btree_fetch(pd_btree root, long key)
{
    while (root && root->key != key)
        root = root->branch[root->key > key];
    return root ? root->value : NULL;
}

void *pd_btree_remove(pd_btree *root, long key)
{
    pd_btree orphan = NULL;
    pd_btree parent = NULL;
    pd_btree ref = *root;
    pd_btree rep;
    
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
        pd_btree_insert(&parent, orphan->key, orphan->value);
        free(orphan);
    }
    
    void *value = ref->value;
    free(ref);
    
    return value;
}

PDInteger pd_btree_populate_keys(pd_btree root, void **dest)
{
    pd_btree r;
    PDInteger i = 0;
    while (root) {
        dest[i++] = (void*)root->key;
        r = root->branch[1];
        if (root->branch[0]) i = pd_btree_populate_keys(root->branch[0], &dest[i]);
        root = r;
    }
    return i;
}

void pd_btree_destroy(pd_btree root)
{
    pd_btree_destroy_with_deallocator(root, NULL);
}

void _pd_btree_destroy(pd_btree root, PDDeallocator deallocator)
{
    if (root->branch[0]) pd_btree_destroy_with_deallocator(root->branch[0], deallocator);
    if (root->branch[1]) pd_btree_destroy_with_deallocator(root->branch[1], deallocator);
    if (deallocator) (*deallocator)(root->value);
    free(root);
}

void pd_btree_destroy_with_deallocator(pd_btree root, PDDeallocator deallocator)
{
    // avoid NULL crashes
    if (root) _pd_btree_destroy(root, deallocator);
}
