//
//  PDType.c
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/31/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

#include "PDDefines.h"
#include "PDInternal.h"
#include "PDType.h"
#include "PDStack.h"

static PDStackRef arp = NULL;

void *PDAlloc(PDSize size, void *dealloc, PDBool zeroed)
{
    PDTypeRef chunk = (zeroed ? calloc(1, sizeof(union PDType) + size) : malloc(sizeof(union PDType) + size));
    chunk->retainCount = 1;
    chunk->dealloc = dealloc;
    return chunk + 1;
}

void PDRelease(void *pajdegObject)
{
    PDTypeRef type = (PDTypeRef)pajdegObject - 1;
    type->retainCount--;
    if (type->retainCount == 0) {
        (*type->dealloc)(pajdegObject);
        free(type);
    }
}

void *PDRetain(void *pajdegObject)
{
    PDTypeRef type = (PDTypeRef)pajdegObject - 1;
    type->retainCount++;
    return pajdegObject;
}

void *PDAutorelease(void *pajdegObject)
{
    PDStackPushIdentifier(&arp, pajdegObject);
    return pajdegObject;
}

void PDFlush(void)
{
    void *obj;
    while ((obj = PDStackPopIdentifier(&arp))) {
        PDRelease(obj);
    }
}
