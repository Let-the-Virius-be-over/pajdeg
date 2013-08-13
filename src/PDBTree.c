//
//  btree.c
//  Infinite PDF
//
//  Created by Karl-Johan Alm on 8/11/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

#include <stdio.h>

#include "PDBTree.h"
#include "pd_internal.h"

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
    node = bt_node_create(key, NULL, active);
    bt_node_branch_for_key(p, key) = node;
    
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
    PDAssert(deallocator);
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
    PDInteger *tmp = malloc(sizeof(void*) * btree->count);
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
    PDInteger *tmp = malloc(sizeof(void*) * btree->count);
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
