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
#include "PDSplayTree.h"
#include "pd_pdf_implementation.h"

static pd_stack arp = NULL;

// if you are having issues with a non-PDTypeRef being mistaken for a PDTypeRef, you can enable DEBUG_PDTYPES_BREAK to stop the assertion from happening and instead returning a NULL value (for the value-returning functions)
//#define DEBUG_PDTYPES_BREAK

#ifdef DEBUG_PD_RELEASES

PDSplayTreeRef _retrels = NULL;

void _PDDebugLogRetrelCall(const char *op, const char *file, int lineNo, void *ob)
{
    if (_retrels == NULL) {
        _retrels = PDSplayTreeCreate(PDDeallocatorNull, 0, 0x1fffffff, 10);
    }
    pd_stack entry = PDSplayTreeGet(_retrels, (PDInteger)ob);
    pd_stack_push_identifier(&entry, (PDID)lineNo);
    pd_stack_push_key(&entry, strdup(file));
    pd_stack_push_identifier(&entry, (PDID)op);
    PDSplayTreeInsert(_retrels, (PDInteger)ob, entry);
}

void _PDDebugLogDisplay(void *ob)
{
    if (_retrels == NULL) return;
    pd_stack entry = PDSplayTreeGet(_retrels, (PDInteger)ob);
    if (entry) {
        printf("Retain/release log for %p:\n", ob);
        printf("op:         line:  file:\n");
        while (entry) {
            char *op = (char *)entry->info ; entry = entry->prev;
            char *file = (char *)entry->info ; entry = entry->prev;
            int lineNo = (int)entry->info ; entry = entry->prev;
            printf("%11s %6d %s\n", op, lineNo, file);
        }
    } else {
        printf("(no log for %p!)\n", ob);
    }
}
#else
#define _PDDebugLogRetrelCall(...) 
#define _PDDebugLogDisplay(ob) 
#endif

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
        _PDDebugLogDisplay(pajdegObject); \
        PDTypeCheckFailed(err_ret); \
    }

#else
#define PDTypeCheck(cmd) 
#endif

void *PDAllocTyped(PDInstanceType it, PDSize size, void *dealloc, PDBool zeroed)
{
    PDTypeRef chunk = (zeroed ? calloc(1, sizeof(union PDType) + size) : malloc(sizeof(union PDType) + size));
#ifdef DEBUG_PDTYPES
    chunk->pdc = PDC;
#endif
    chunk->it = it;
    chunk->retainCount = 1;
    chunk->dealloc = dealloc;
    return chunk + 1;
}

void *PDAlloc(PDSize size, void *dealloc, PDBool zeroed)
{
    return PDAllocTyped(PDInstanceTypeUnset, size, dealloc, zeroed);
}

#ifdef DEBUG_PD_RELEASES
void PDReleaseFunc(void *pajdegObject) 
{
    _PDReleaseDebug("--", 0, pajdegObject);
}

void _PDReleaseDebug(const char *file, int lineNumber, void *pajdegObject)
#else
void PDRelease(void *pajdegObject)
#endif
{
    if (NULL == pajdegObject) return;
    _PDDebugLogRetrelCall("release", file, lineNumber, pajdegObject);
    PDTypeRef type = (PDTypeRef)pajdegObject - 1;
    PDTypeCheck("released", /* void */);
    type->retainCount--;
#ifdef DEBUG_PD_RELEASES
    // over-autorelease check
    pd_stack s;
    pd_stack_for_each(arp, s) {
        void *pdo = s->info;
        PDTypeRef t2 = (PDTypeRef)pdo - 1;
        PDAssert(t2->retainCount > 0); // crash = over-releasing autoreleased object
    }
#endif
    if (type->retainCount == 0) {
        (*type->dealloc)(pajdegObject);
        free(type);
    }
}

#ifdef DEBUG_PD_RELEASES
extern void *_PDRetainDebug(const char *file, int lineNumber, void *pajdegObject)
#else
void *PDRetain(void *pajdegObject)
#endif
{
    if (NULL == pajdegObject) return pajdegObject;
    _PDDebugLogRetrelCall("retain", file, lineNumber, pajdegObject);
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

#ifdef DEBUG_PD_RELEASES
void *_PDAutoreleaseDebug(const char *file, int lineNumber, void *pajdegObject)
#else
void *PDAutorelease(void *pajdegObject)
#endif
{
    if (NULL == pajdegObject) return NULL;
    _PDDebugLogRetrelCall("autorelease", file, lineNumber, pajdegObject);
    
#ifdef DEBUG_PDTYPES
    PDTypeRef type = (PDTypeRef)pajdegObject - 1;
    PDTypeCheck("autoreleased", NULL);
#endif
    pd_stack_push_identifier(&arp, pajdegObject);
    return pajdegObject;
}

PDInstanceType PDResolve(void *pajdegObject)
{
    if (NULL == pajdegObject) return PDInstanceTypeNull;
    
    PDTypeRef type = (PDTypeRef)pajdegObject - 1;
    PDTypeCheck("resolve", NULL);
    return type->it;
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
