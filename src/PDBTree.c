//
// PDBTree.c
//
// Copyright (c) 2012 - 2014 Karl-Johan Alm (http://github.com/kallewoof)
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

#include <stdio.h>

#include "PDBTree.h"
#include "pd_internal.h"

typedef struct bt_node *bt_node;

struct PDBTree {
    PDDeallocator deallocator;
    bt_node       root;
    PDInteger     count;
};

struct bt_node {
    PDInteger key;
    PDBool active;
    void *value;
    bt_node brc[2];
};

#define bt_node_branch_for_key(node, k) node->brc[k > node->key]

static inline void bt_node_destroy(bt_node node, PDDeallocator deallocator)
{
    if (node->value) (*deallocator)(node->value);
    if (node->brc[0]) bt_node_destroy(node->brc[0], deallocator);
    if (node->brc[1]) bt_node_destroy(node->brc[1], deallocator);
    free(node);
}

static inline bt_node bt_node_create(PDInteger key, void *value, PDBool active)
{
    bt_node node = malloc(sizeof(struct bt_node));
    node->key = key;
    node->value = value;
    node->active = active;
    node->brc[0] = node->brc[1] = NULL;
    return node;
}

static inline bt_node bt_node_probe(bt_node subroot, PDInteger key, PDBool active)
{
    bt_node p = NULL;
    bt_node node = subroot;
    bt_node iter;
    for (iter = subroot; iter && iter->key != key; ) {
        p = iter;
        p->active |= active;
        iter = bt_node_branch_for_key(iter, key);
    }
    
    // exists
    if (iter) {
        iter->active |= active;
        return iter;
    }
    
    // insert
    if (p) {
        node = bt_node_create(key, NULL, active);
        bt_node_branch_for_key(p, key) = node;
    }
    
    return node;
}

static inline void *bt_node_insert(bt_node subroot, PDInteger key, void *value, PDBool active)
{
    bt_node node = bt_node_probe(subroot, key, active);
    void *old = node->value;
    node->value = value;
    return old;
}

void bt_node_null(void *v) {}

static inline void *bt_node_delete(bt_node *subroot, PDInteger key)
{
    bt_node orphan = NULL;
    bt_node parent = NULL;
    bt_node ref = *subroot;
    bt_node rep;
    
    while (ref && ref->active && ref->key != key) {
        parent = ref;
        ref = bt_node_branch_for_key(ref, key);
        //ref->brc[ref->key > key];
    }
    if (! ref || ! ref->value) {
        return NULL;
    }
    
    if (! ref->brc[0]) {
        rep = ref->brc[1];
    } else {
        rep = ref->brc[0];
        orphan = ref->brc[1];
    }
    
    if (parent) 
        bt_node_branch_for_key(parent, key) = rep;
    else 
        parent = *subroot = rep;
    
    if (orphan) {
        if (orphan->active) {
            bt_node rep = bt_node_probe(parent, orphan->key, true);
            rep->value = orphan->value;
            rep->brc[0] = orphan->brc[0];
            rep->brc[1] = orphan->brc[1];
            free(orphan);
        } else {
            bt_node_destroy(orphan, bt_node_null);
        }
    }
    
    void *value = ref->value;
    free(ref);
    
    return value;
}

static inline void *bt_node_get(bt_node subroot, PDInteger key)
{
    while (subroot && subroot->key != key && subroot->active) {
        subroot = bt_node_branch_for_key(subroot, key);
    }
    return subroot && subroot->active ? subroot->value : NULL;
}

static inline PDInteger bt_node_populate_keys(bt_node root, PDInteger *dest)
{
    PDInteger i = 0;
    while (root && root->active) {
        if (root->value) {
            dest[i++] = root->key;
        }
        if (root->brc[0]) i += bt_node_populate_keys(root->brc[0], &dest[i]);
        root = root->brc[1];
    }
    return i;
}

//
//
//

void PDBTreeDestroy(PDBTreeRef tree)
{
    bt_node node = tree->root;
    if (node) bt_node_destroy(node, tree->deallocator);
}

PDBTreeRef PDBTreeCreate(PDDeallocator deallocator, PDInteger expectedMinimum, PDInteger expectedMaximum, PDInteger intensity)
{
    if (deallocator == NULL) deallocator = PDNOP;
    PDBTreeRef tree = PDAlloc(sizeof(struct PDBTree), PDBTreeDestroy, true);
    tree->deallocator = deallocator;
    
    PDInteger step = expectedMaximum / 2;
    PDInteger first;

    tree->root = bt_node_create(expectedMinimum + step, NULL, false);
    
    while (intensity > 0 && step > 4) {
        step /= 2;
        for (first = expectedMinimum + step; first < expectedMaximum; first += step) {
            bt_node_insert(tree->root, first, NULL, false);
        }
        intensity--;
    }
    
    return tree;
}

void PDBTreeInsert(PDBTreeRef btree, PDInteger key, void *value)
{
    void *old = bt_node_insert(btree->root, key, value, true);
    if (value && NULL == old) btree->count++;
#ifdef DEBUG
    PDInteger *tmp = malloc(sizeof(PDInteger) * btree->count);
    PDInteger c = PDBTreePopulateKeys(btree, tmp);
    PDAssert(btree->count == c);
    free(tmp);
#endif
}

void PDBTreeDelete(PDBTreeRef btree, PDInteger key)
{
    if (bt_node_delete(&btree->root, key))
        btree->count--;
    
#ifdef DEBUG
    PDInteger *tmp = malloc(sizeof(PDInteger) * btree->count);
    PDInteger c = PDBTreePopulateKeys(btree, tmp);
    PDAssert(btree->count == c);
    free(tmp);
#endif
}

void *PDBTreeGet(PDBTreeRef btree, PDInteger key)
{
    return bt_node_get(btree->root, key);
}

PDInteger PDBTreeGetCount(PDBTreeRef btree)
{
    return btree->count;
}

PDInteger PDBTreePopulateKeys(PDBTreeRef tree, PDInteger *dest)
{
    return bt_node_populate_keys(tree->root, dest);
}

void PDBTreePrint(PDBTreeRef btree)
{
    PDInteger *dest = malloc(sizeof(PDInteger) * PDBTreeGetCount(btree));
    PDInteger count = bt_node_populate_keys(btree->root, dest);
    printf("[%ld items in tree]\n", count);
    for (PDInteger i = 0; i < count; i++) {
        printf("%ld = %p\n", dest[i], PDBTreeGet(btree, dest[i]));
    }
    free(dest);
}
