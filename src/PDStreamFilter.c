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

PDStreamFilterRef PDStreamFilterCreate(PDStreamFilterFunc init, PDStreamFilterFunc done, PDStreamFilterFunc begin, PDStreamFilterFunc proceed, PDStreamFilterPrcs inversion, PDStackRef options)
{
    PDStreamFilterRef filter = calloc(1, sizeof(struct PDStreamFilter));
    filter->growthHint = 1.f;
    filter->compatible = true;
    filter->options = options;
    filter->init = init;
    filter->done = done;
    filter->begin = begin;
    filter->proceed = proceed;
    filter->inversion = inversion;
    return filter;
}

void PDStreamFilterDestroy(PDStreamFilterRef filter)
{
    if (filter->initialized) 
        (*filter->done)(filter);

    PDStackDestroy(filter->options);
    filter->options = NULL;
    
    if (filter->bufOutOwned)
        free(filter->bufOutOwned);
    
    if (filter->nextFilter)
        PDStreamFilterDestroy(filter->nextFilter);
    
    free(filter);
}

PDBool PDStreamFilterApply(PDStreamFilterRef filter, unsigned char *src, unsigned char **dstPtr, PDInteger len, PDInteger *newlenPtr)
{
    if (! filter->initialized) {
        if (! PDStreamFilterInit(filter))
            return false;
    }
    
    unsigned char *resbuf;
    PDInteger dstcap = len * filter->growthHint;
    if (dstcap < 64) dstcap = 64;
    if (dstcap > 64 * 1024) dstcap = 64 * 1024;
    
    filter->bufIn = src;
    filter->bufInAvailable = len;
    resbuf = filter->bufOut = malloc(dstcap);
    filter->bufOutCapacity = dstcap;
    
    PDInteger bytes = PDStreamFilterBegin(filter);
    PDInteger got = 0;
    while (bytes > 0) {
        got += bytes;
        if (! filter->finished) {
            dstcap *= 3;
            resbuf = realloc(resbuf, dstcap);
        }
        filter->bufOut = &resbuf[got];
        filter->bufOutCapacity = dstcap - got;
        bytes = PDStreamFilterProceed(filter);
    }
    
    *dstPtr = resbuf;
    *newlenPtr = got;

    return true;
}

PDBool PDStreamFilterInit(PDStreamFilterRef filter)
{
    if (! (*filter->init)(filter)) 
        return false;

    if (filter->nextFilter) {
        if (! PDStreamFilterInit(filter->nextFilter))
            return false;
        filter->compatible &= filter->nextFilter->compatible;
    }
    return true;
}

PDBool PDStreamFilterDone(PDStreamFilterRef filter)
{
    while (filter) {
        if (! (*filter->done)(filter)) 
            return false;
        filter = filter->nextFilter;
    }
    return true;
}

PDInteger PDStreamFilterBegin(PDStreamFilterRef filter)
{
    // we set this up so that the first and last filters in the chain swap filters on entry/exit (for proceed); that will give the caller a consistent filter state which they can read from / update as they need
    PDStreamFilterRef curr, next;
    
    if (filter->nextFilter == NULL) 
        return (*filter->begin)(filter);
    
    // we pull out filter's output values
    unsigned char *bufOut = filter->bufOut;
    PDInteger bufOutCapacity = filter->bufOutCapacity;
    PDInteger cap;
    
    // set up new buffers for each of the filters in between
    //prev = NULL;
    for (curr = filter; curr->nextFilter; curr = next) {
        next = curr->nextFilter;

        cap = curr->bufInAvailable * curr->growthHint;
        if (cap < bufOutCapacity) cap = bufOutCapacity;
        if (cap > bufOutCapacity * 10) cap = bufOutCapacity * 10;
        curr->bufOutCapacity = curr->bufOutOwnedCapacity = cap;
        next->bufIn = curr->bufOutOwned = curr->bufOut = malloc(curr->bufOutCapacity);
        next->bufInAvailable = 0; //(*curr->begin)(curr);
        next->needsInput = true;
        next->hasInput = true;
    }
    
    filter->finished = false;
    filter->needsInput = true;
    filter->hasInput = false;
    filter->bufOut = bufOut;
    filter->bufOutCapacity = bufOutCapacity;
    
    // we can now call proceed as normal
    return PDStreamFilterProceed(filter);
}

PDInteger PDStreamFilterProceed(PDStreamFilterRef filter)
{
    PDStreamFilterRef curr, prev, next;
    
    // don't waste time
    if (filter->finished) return 0;
    
    // or energy
    if (filter->nextFilter == NULL) return (*filter->proceed)(filter);
    
    // we pull out filter's output values
    PDInteger result = 0;
    unsigned char *bufOut = filter->bufOut;
    PDInteger bufOutCapacity = filter->bufOutCapacity;
    
    // iterate over each filter
    prev = NULL;
    for (curr = filter; curr; curr = next) {
        next = curr->nextFilter;
        
        if (next) {
            
            if (! next->needsInput || next->bufInAvailable >= curr->bufOutOwnedCapacity / 2) {
                // the next filter is still processing data so we have to pass
                prev = curr;
                continue;
            }
            
            // filters sometimes need more data to process (e.g. predictor), so we can't blindly throw out and replace the buffers each time; the next->bufInAvailable declares how many bytes are still unprocessed
            if (next->bufInAvailable > 0) {
                // we can always memcpy() as no overlap will occur (or we would've skipped this filter)
                //assert(curr->bufOutOwned + curr->bufOutOwnedCapacity - next->bufInAvailable == next->bufIn);
                memcpy(curr->bufOutOwned, next->bufIn, next->bufInAvailable);
                next->bufIn = curr->bufOutOwned;
                curr->bufOut = curr->bufOutOwned + next->bufInAvailable;
                curr->bufOutCapacity = curr->bufOutOwnedCapacity - next->bufInAvailable;
            } else {
                // next filter ate all its breakfast, so we do this the easy way
                curr->bufOut = next->bufIn = curr->bufOutOwned;
                curr->bufOutCapacity = curr->bufOutOwnedCapacity;
            }
            
            next->bufInAvailable += (curr->needsInput ? (*curr->begin)(curr) : (*curr->proceed)(curr));
            next->hasInput = curr->bufInAvailable > 0;
            
        } else {
            
            // last filter, which means we plug it into the "real" output buffer
            curr->bufOut = bufOut;
            curr->bufOutCapacity = bufOutCapacity;
            result += curr->needsInput ? (*curr->begin)(curr) : (*curr->proceed)(curr);
            bufOut = curr->bufOut;
            bufOutCapacity = curr->bufOutCapacity;
        
        }

        if (curr->failing) {
            filter->failing = true;
            return 0;
        }
        
        // we may be in a situation where we consumed a lot of content and produced little; to prevent bottlenecks we will return to previous filters and reapply in these cases
        if (prev && prev->bufInAvailable > 0 && curr->bufInAvailable < 64 && curr->bufOutCapacity > 64) {
            // we have a prev, its input buffer has stuff, our input buffer has very little or no stuff, and our output buffer is capable of taking more stuff
            // this is a good sign that we may need to grow an inbetween buffer to not bounce around too much
            if (prev->bufOutOwnedCapacity < result + bufOutCapacity) {
                PDInteger offs = curr->bufIn - prev->bufOutOwned;
                prev->bufOutOwned = realloc(prev->bufOutOwned, result + bufOutCapacity);
                curr->bufIn = prev->bufOutOwned + offs;
                prev->bufOutOwnedCapacity = result + bufOutCapacity;
            }
            next = prev;
            curr = NULL;
        } 
        
        prev = curr;
    }
    
    // now restore filter's output buffers from prev (which is the last non-null curr)
    filter->bufOut = prev->bufOut;
    filter->bufOutCapacity = prev->bufOutCapacity;
    
    // we actually go through once more to set 'finished' as it may flip back and forth as the filters rewind
    PDBool finished = true;
    for (curr = filter; curr; curr = curr->nextFilter)
        finished &= curr->finished;
    
    filter->finished = finished;

    return result;
}

PDStreamFilterRef PDStreamFilterCreateInversionForFilter(PDStreamFilterRef filter)
{
    PDStreamFilterRef inversionParent = NULL;
    
    if (! filter->initialized) (*filter->init)(filter);
    
    if (filter->inversion == NULL) return NULL;
    
    if (filter->nextFilter) {
        if (filter->nextFilter->inversion) 
            inversionParent = (*filter->nextFilter->inversion)(filter);
        if (inversionParent == NULL) 
            return NULL;
    }
    
    PDStreamFilterRef inversion = (*filter->inversion)(filter);
    if (! inversionParent) {
        return inversion;
    }
    
    // we went from  [1] ->  [2] ->  [3] to 
    //                       [2] ->  [3] to
    //                              ~[3] to
    //                      ~[3] -> ~[2]
    // and we keep getting the "super parent" back each time, so we have to step down
    // to the last child and put us below that, in order to get
    //              ~[3] -> ~[2] -> ~[1]
    // which is the only proper inversion (~ is not the proper sign for inversion, but ^-1 looks ugly)
    
    PDStreamFilterRef result = inversionParent;
    while (inversionParent->nextFilter) inversionParent = inversionParent->nextFilter;
    inversionParent->nextFilter = inversion;
    
    return result;
}

PDStackRef PDStreamFilterCreateOptionsFromDictionaryStack(PDStackRef dictStack)
{
    if (dictStack == NULL) return NULL;
    
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