//
//  PDType.c
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
    PDTypeRef type = (PDTypeRef)pajdegObject - 1;
    PDTypeCheck("retained", NULL);
    type->retainCount++;
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

void PDFlush(void)
{
    void *obj;
    while ((obj = pd_stack_pop_identifier(&arp))) {
        PDRelease(obj);
    }
}
