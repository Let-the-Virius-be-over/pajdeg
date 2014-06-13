//
// PDType.c
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

#include "PDDefines.h"
#include "pd_internal.h"
#include "PDType.h"
#include "pd_stack.h"

static pd_stack arp = NULL;

// if you are having issues with a non-PDTypeRef being mistaken for a PDTypeRef, you can enable DEBUG_PDTYPES_BREAK to stop the assertion from happening and instead returning a NULL value (for the value-returning functions)
//#define DEBUG_PDTYPES_BREAK

#ifdef DEBUG_PDTYPES
char *PDC = "PAJDEG";

#ifdef DEBUG_PDTYPES_BREAK

void breakHere()
{
    printf("");
}

#define PDTypeCheckFailed(err_ret) breakHere(); return err_ret

#else

#define PDTypeCheckFailed(err_ret) PDAssert(0)

#endif

#define PDTypeCheck(cmd, err_ret) \
    if (type->pdc != PDC) { \
        fprintf(stderr, "*** error : object being " cmd " is not a valid PDType instance : %p ***\n", pajdegObject); \
        PDTypeCheckFailed(err_ret); \
    }

#else
#define PDTypeCheck(cmd) 
#endif

void *PDAlloc(PDSize size, void *dealloc, PDBool zeroed)
{
    PDTypeRef chunk = (zeroed ? calloc(1, sizeof(union PDType) + size) : malloc(sizeof(union PDType) + size));
    chunk->pdc = PDC;
    chunk->retainCount = 1;
    chunk->dealloc = dealloc;
    return chunk + 1;
}

void PDRelease(void *pajdegObject)
{
    if (NULL == pajdegObject) return;
    PDTypeRef type = (PDTypeRef)pajdegObject - 1;
    PDTypeCheck("released", /* void */);
    type->retainCount--;
    if (type->retainCount == 0) {
        (*type->dealloc)(pajdegObject);
        free(type);
    }
}

void *PDRetain(void *pajdegObject)
{
    if (NULL == pajdegObject) return pajdegObject;
    PDTypeRef type = (PDTypeRef)pajdegObject - 1;
    PDTypeCheck("retained", NULL);
    
    // if the most recent autoreleased object matches, we remove it from the autorelease pool rather than retain the object
    if (arp != NULL && pajdegObject == arp->info) {
        pd_stack_pop_identifier(&arp);
    } else {
        type->retainCount++;
    }
    
    return pajdegObject;
}

void *PDAutorelease(void *pajdegObject)
{
#ifdef DEBUG_PDTYPES
    PDTypeRef type = (PDTypeRef)pajdegObject - 1;
    PDTypeCheck("autoreleased", NULL);
#endif
    pd_stack_push_identifier(&arp, pajdegObject);
    return pajdegObject;
}

PDInteger PDGetRetainCount(void *pajdegObject)
{
    PDTypeRef type = (PDTypeRef)pajdegObject - 1;
    PDTypeCheck("GetRetainCounted", -1);
    return type->retainCount;
}

void PDFlush(void)
{
    void *obj;
    while ((obj = pd_stack_pop_identifier(&arp))) {
        PDRelease(obj);
    }
}

void PDNOP(void *val) {}

#ifdef PD_WARNINGS
void _PDBreak()
{
    /*
     * The sole purpose of this method is to be a gathering spot for break points triggered by calls to the PDError() macro.
     * If you are not a Pajdeg developer, you shouldn't worry about this.
     */
    return;
}
#endif
