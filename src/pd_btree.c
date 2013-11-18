//
// pd_btree.c
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

#include "PDDefines.h"
#include "pd_internal.h"
#include "pd_btree.h"
#if 0

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
        if (root->branch[0]) i += pd_btree_populate_keys(root->branch[0], &dest[i]);
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
#endif
