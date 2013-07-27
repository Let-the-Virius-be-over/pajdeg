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

static PDStackRef filterRegistry = NULL;

void PDStreamFilterRegisterDualFilter(const char *name, PDStreamDualFilterConstr constr)
{
    PDStackPushIdentifier(&filterRegistry, (PDID)constr);
    PDStackPushIdentifier(&filterRegistry, (PDID)name);
}

PDStreamFilterRef PDStreamFilterObtain(const char *name, PDBool inputEnd)
{
    PDStackRef iter = filterRegistry;
    while (iter && !strcmp(iter->info, name)) iter = iter->prev->prev;
    if (iter) {
        PDStreamDualFilterConstr constr = iter->prev->info;
        return (*constr)(inputEnd);
    }
    
    return NULL;
}

PDStreamFilterRef PDStreamFilterCreate(PDStreamFilterFunc init, PDStreamFilterFunc done, PDStreamFilterFunc process, PDStreamFilterFunc proceed)
{
    PDStreamFilterRef filter = calloc(1, sizeof(struct PDStreamFilter));
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
    free(filter);
}
