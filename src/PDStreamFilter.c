//
//  PDStreamFilter.c
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/26/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

#include "PDInternal.h"
#include "PDStreamFilter.h"
#include "PDStack.h"
#include "PDPortableDocumentFormatState.h"

static PDStackRef filterRegistry = NULL;

void PDStreamFilterRegisterDualFilter(const char *name, PDStreamDualFilterConstr constr)
{
    PDStackPushIdentifier(&filterRegistry, (PDID)constr);
    PDStackPushIdentifier(&filterRegistry, (PDID)name);
}

PDStreamFilterRef PDStreamFilterObtain(const char *name, PDBool inputEnd, PDStackRef options)
{
    PDStackRef iter = filterRegistry;
    while (iter && strcmp(iter->info, name)) iter = iter->prev->prev;
    if (iter) {
        PDStreamDualFilterConstr constr = iter->prev->info;
        return (*constr)(inputEnd, options);
    }
    
    return NULL;
}

PDStreamFilterRef PDStreamFilterCreate(PDStreamFilterFunc init, PDStreamFilterFunc done, PDStreamFilterFunc process, PDStreamFilterFunc proceed, PDStackRef options)
{
    PDStreamFilterRef filter = calloc(1, sizeof(struct PDStreamFilter));
    filter->options = options;
    filter->init = init;
    filter->done = done;
    filter->process = process;
    filter->proceed = proceed;
    return filter;
}

void PDStreamFilterDestroy(PDStreamFilterRef filter)
{
    if (filter->initialized) 
        (*filter->done)(filter);
    PDStackDestroy(filter->options);
    filter->options = NULL;
    free(filter);
}

PDStackRef PDStreamFilterCreateOptionsFromDictionaryStack(PDStackRef dictStack)
{
    PDStackRef stack = NULL;
    PDStackRef iter = dictStack->prev->prev->info;
    PDStackRef entry;
    PDStackSetGlobalPreserveFlag(true);
    while (iter) {
        entry = iter->info;
        char *value = (entry->prev->prev->type == PDSTACK_STACK 
                       ? PDStringFromComplex(entry->prev->prev->info)
                       : strdup(entry->prev->prev->info));
        PDStackPushKey(&stack, value);
        PDStackPushKey(&stack, strdup(entry->prev->info));
        iter = iter->prev;
    }
    PDStackSetGlobalPreserveFlag(false);
    return stack;
}
/*
     stack<0x172adcd0> {
         0x46d0ac ("dict")
         0x46d0a8 ("entries")
         stack<0x172ad090> {
             stack<0x172ad080> {
                 0x46d0b0 ("de")
                 Columns
                 6
             }
             stack<0x172adca0> {
                 0x46d0b0 ("de")
                 Predictor
                 12
             }
         }
     }
*/