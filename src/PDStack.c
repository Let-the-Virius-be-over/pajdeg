//
//  PDStack.c
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

#include "PDStack.h"
#include "PDInternal.h"
#include "PDEnv.h"
#include "PDState.h"
#include "PDPortableDocumentFormatState.h"

static PDInteger PDStackPreserveUsers = 0;
PDDeallocator PDStackDealloc = free;
void PDStackPreserve(void *ptr)
{}

void PDStackSetGlobalPreserveFlag(PDBool preserve)
{
    PDStackPreserveUsers += preserve ? 1 : -1;
    PDAssert(PDStackPreserveUsers >= 0);
    if (PDStackPreserveUsers - preserve < 1)
        PDStackDealloc = preserve ? &PDStackPreserve : &free;
}

void PDStackPushIdentifier(PDStackRef *stack, PDID identifier)
{
    PDStackRef s = malloc(sizeof(struct PDStack));
    s->prev = *stack;
    s->info = identifier;
    s->type = PDSTACK_ID;
    *stack = s;
}

void PDStackPushKey(PDStackRef *stack, char *key)
{
    PDStackRef s = malloc(sizeof(struct PDStack));
    s->prev = *stack;
    s->info = key;//strdup(key); free(key);
    s->type = PDSTACK_STRING;
    *stack = s;
}

void PDStackPushFreeable(PDStackRef *stack, void *freeable)
{
    PDStackRef s = malloc(sizeof(struct PDStack));
    s->prev = *stack;
    s->info = freeable;
    s->type = PDSTACK_FREEABL;
    *stack = s;
}

void PDStackPushStack(PDStackRef *stack, PDStackRef pstack)
{
    PDStackRef s = malloc(sizeof(struct PDStack));
    s->prev = *stack;
    s->info = pstack;
    s->type = PDSTACK_STACK;
    *stack = s;
}

void PDStackUnshiftStack(PDStackRef *stack, PDStackRef sstack) 
{
    PDStackRef vtail;
    if (*stack == NULL) {
        PDStackPushStack(stack, sstack);
        return;
    }
    
    for (vtail = *stack; vtail->prev; vtail = vtail->prev) ;

    PDStackRef s = malloc(sizeof(struct PDStack));
    s->prev = NULL;
    s->info = sstack;
    s->type = PDSTACK_STACK;
    vtail->prev = s;
}

void PDStackPushEnv(PDStackRef *stack, PDEnvRef env)
{
    PDStackRef s = malloc(sizeof(struct PDStack));
    s->prev = *stack;
    s->info = env;
    s->type = PDSTACK_ENV;
    *stack = s;
}

PDID PDStackPopIdentifier(PDStackRef *stack)
{
    if (*stack == NULL) return NULL;
    PDStackRef popped = *stack;
    PDAssert(popped->type == PDSTACK_ID);
    *stack = popped->prev;
    PDID identifier = popped->info;
    (*PDStackDealloc)(popped);
    return identifier;
}

void PDStackAssertExpectedKey(PDStackRef *stack, const char *key)
{
    PDAssert(*stack != NULL);
    
    PDStackRef popped = *stack;
    PDAssert(popped->type == PDSTACK_STRING || popped->type == PDSTACK_ID);

    char *got = popped->info;
    if (popped->type == PDSTACK_STRING) {
        PDAssert(got == key || !strcmp(got, key));
        (*PDStackDealloc)(got);
    } else {
        PDAssert(!strcmp(*(char**)got, key));
    }
    
    *stack = popped->prev;
    (*PDStackDealloc)(popped);
}

void PDStackAssertExpectedInt(PDStackRef *stack, PDInteger i)
{
    PDAssert(*stack != NULL);
    
    PDStackRef popped = *stack;
    PDAssert(popped->type == PDSTACK_STRING);
    
    char *got = popped->info;
    PDAssert(i == atoi(got));
    
    *stack = popped->prev;
    (*PDStackDealloc)(got);
    (*PDStackDealloc)(popped);
}

PDSize PDStackPopSize(PDStackRef *stack)
{
    if (*stack == NULL) return 0;
    PDStackRef popped = *stack;
    PDAssert(popped->type == PDSTACK_STRING);
    *stack = popped->prev;
    char *key = popped->info;
    PDSize st = atol(key);
    (*PDStackDealloc)(key);
    (*PDStackDealloc)(popped);
    return st;
}

PDInteger PDStackPopInt(PDStackRef *stack)
{
    if (*stack == NULL) return 0;
    PDStackRef popped = *stack;
    PDAssert(popped->type == PDSTACK_STRING);
    *stack = popped->prev;
    char *key = popped->info;
    PDInteger st = atol(key);
    (*PDStackDealloc)(key);
    (*PDStackDealloc)(popped);
    return st;
}

PDInteger PDStackPeekInt(PDStackRef popped)
{
    if (popped == NULL) return 0;
    PDAssert(popped->type == PDSTACK_STRING);
    return PDIntegerFromString(popped->info);
}

char *PDStackPopKey(PDStackRef *stack)
{
    if (*stack == NULL) return NULL;
    PDStackRef popped = *stack;
    PDAssert(popped->type == PDSTACK_STRING);
    *stack = popped->prev;
    char *key = popped->info;
    (*PDStackDealloc)(popped);
    return key;
}

PDStackRef PDStackPopStack(PDStackRef *stack)
{
    if (*stack == NULL) return NULL;
    PDStackRef popped = *stack;
    PDAssert(popped->type == PDSTACK_STACK);
    *stack = popped->prev;
    PDStackRef pstack = popped->info;
    (*PDStackDealloc)(popped);
    return pstack;
}

PDEnvRef PDStackPopEnv(PDStackRef *stack)
{
    if (*stack == NULL) return NULL;
    PDStackRef popped = *stack;
    PDAssert(popped->type == PDSTACK_ENV);
    *stack = popped->prev;
    PDEnvRef env = popped->info;
    (*PDStackDealloc)(popped);
    return env;
}

void *PDStackPopFreeable(PDStackRef *stack)
{
    if (*stack == NULL) return NULL;
    PDStackRef popped = *stack;
    PDAssert(popped->type == PDSTACK_FREEABL);
    *stack = popped->prev;
    void *key = popped->info;
    (*PDStackDealloc)(popped);
    return key;
}

void PDStackPopInto(PDStackRef *dest, PDStackRef *source)
{
    if (*source == NULL) {
        PDAssert(*source != NULL);  // must never pop into from a null stack
        return;
    }
    
    PDStackRef popped = *source;
    *source = popped->prev;
    popped->prev = *dest;
    *dest = popped;
}

static inline void PDStackFreeInfo(PDStackRef stack)
{
    switch (stack->type) {
        case PDSTACK_STRING:
        case PDSTACK_FREEABL:
            free(stack->info);
            break;
        case PDSTACK_STACK:
            PDStackDestroy(stack->info);
            break;
        case PDSTACK_ENV:
            PDEnvDestroy(stack->info);
            break;
    }
}

void PDStackReplaceInfoObject(PDStackRef stack, char type, void *info)
{
    PDStackFreeInfo(stack);
    stack->type = type;
    stack->info = info;
}

void PDStackDestroy(PDStackRef stack)
{
    PDStackRef p;
    while (stack) {
        //printf("-stack %p\n", stack);
        p = stack->prev;
        PDStackFreeInfo(stack);
        free(stack);
        stack = p;
    }
}

PDStackRef PDStackGetDictKey(PDStackRef dictStack, const char *key, PDBool remove)
{
    // dicts are set up (reversedly) as
    // "dict"
    // "entries"
    // [stack]
    if (dictStack == NULL || ! PDIdentifies(dictStack->info, PD_DICT)) 
        return NULL;
    
    PDStackRef prev = dictStack->prev->prev;
    PDStackRef stack = dictStack->prev->prev->info;
    PDStackRef entry;
    while (stack) {
        // entries are stacks, with
        // e
        // ID
        // entry
        entry = stack->info;
        if (! strcmp((char*)entry->prev->info, key)) {
            if (remove) {
                if (prev == dictStack->prev->prev) {
                    // first entry; container stack must reref
                    prev->info = stack->prev;
                } else {
                    // not first entry; simply reref prev
                    prev->prev = stack->prev;
                }
                // disconnect stack from its siblings and from prev (or prev is destroyed), then destroy stack and we can return prev
                stack->info = NULL;
                stack->prev = NULL;
                PDStackDestroy(stack);
                return entry;
            }
            return entry;
        }
        prev = stack;
        stack = stack->prev;
    }
    return NULL;
}

PDBool PDStackGetNextDictKey(PDStackRef *iterStack, char **key, char **value)
{
    // dicts are set up (reversedly) as
    // "dict"
    // "entries"
    // [stack]
    PDStackRef stack = *iterStack;
    PDStackRef entry;
    
    // two instances where we hit falsity; stack is indeed a dictionary, but it's empty: we will initially have a stack but it will be void after below if case (hence, stack truthy is included), otherwise it is the last element, in which case it's NULL on entry
    if (stack && PDIdentifies(stack->info, PD_DICT)) {
        *iterStack = stack = stack->prev->prev->info;
    }
    if (! stack) return false;
    
    // entries are stacks, with
    // de
    // ID
    // entry
    entry = stack->info;
    *key = (char*)entry->prev->info;
    entry = (PDStackRef)entry->prev->prev;
    
    // entry is now iterated past e, ID and is now at
    // entry
    // so we see if type is primitive or not
    if (entry->type == PDSTACK_STACK) {
        // it's not primitive, so we set the preserve flag and stringify
        PDStackSetGlobalPreserveFlag(true);
        entry = (PDStackRef)entry->info;
        *value = PDStringFromComplex(&entry);
        PDStackSetGlobalPreserveFlag(false);
    } else {
        // it is primitive (we presume)
        PDAssert(entry->type == PDSTACK_STRING);
        *value = strdup((char*)entry->info);
    }
    
    *iterStack = stack->prev;
    
    return true;
}

PDStackRef PDStackCreateFromDefinition(const void **defs)
{
    PDInteger i;
    PDStackRef stack = NULL;
    
    for (i = 0; defs[i]; i++) {
        PDStackPushIdentifier(&stack, (const char **)defs[i]);
    }
    
    return stack;
}

//
// debugging
//


static char *sind = NULL;
static PDInteger cind = 0;
void PDStackPrint_(PDStackRef stack, PDInteger indent)
{
    PDInteger res = cind;
    sind[cind] = ' ';
    sind[indent] = 0;
    printf("%sstack<%p> {\n", sind, stack);
    sind[indent] = ' ';
    cind = indent + 2;
    sind[cind] = 0;
    while (stack) {
        switch (stack->type) {
            case PDSTACK_ID:
                printf("%s %p (\"%s\")\n", sind, stack->info, *(char **)stack->info);
                break;
            case PDSTACK_STRING:
                printf("%s %s\n", sind, (char*)stack->info);
                break;
            case PDSTACK_FREEABL:
                printf("%s %p\n", sind, stack->info);
                break;
            case PDSTACK_STACK:
                PDStackPrint_(stack->info, cind + 2);
                break;
            case PDSTACK_ENV:
                printf("%s env %s (%p)", sind, ((PDEnvRef)stack->info)->state->name, stack->info);
                break;
            default:
                printf("%s ?????? %p", sind, stack->info);
                break;
        }
        stack = stack->prev;
    }
    sind[cind] = ' ';
    cind -= 2;
    sind[indent] = 0;
    printf("%s}\n", sind);
    cind = res;
    sind[indent] = ' ';
    sind[cind] = 0;
}

void PDStackPrint(PDStackRef stack)
{
    if (sind == NULL) sind = strdup("                                                                                                                                                                                                                                                       ");
    PDStackPrint_(stack, 0);
}

// 
// the "pretty" version (above is debuggy)
//

void PDStackShow_(PDStackRef stack)
{
    PDBool stackLumping = false;
    while (stack) {
        stackLumping &= (stack->type == PDSTACK_STACK);
        if (stackLumping) putchar('\t');
        
        switch (stack->type) {
            case PDSTACK_ID:
                printf("@%s", *(char **)stack->info);
                break;
            case PDSTACK_STRING:
                printf("\"%s\"", (char*)stack->info);
                break;
            case PDSTACK_FREEABL:
                printf("%p", stack->info);
                break;
            case PDSTACK_STACK:
                if (! stackLumping && (stackLumping |= stack->prev && stack->prev->type == PDSTACK_STACK)) 
                    putchar('\n');
                //stackLumping |= stack->prev && stack->prev->type == PDSTACK_STACK;
                if (stackLumping) {
                    printf("\t{ ");
                    PDStackShow_(stack->info);
                    printf(" }");
                } else {
                    printf("{ ");
                    PDStackShow_(stack->info);
                    printf(" }");
                }
                break;
            case PDSTACK_ENV:
                printf("<%s>", ((PDEnvRef)stack->info)->state->name);
                break;
            default:
                printf("??? %d %p ???", stack->type, stack->info);
                break;
        }
        stack = stack->prev;
        if (stack) printf(stackLumping ? ",\n" : ", ");
    }
}

void PDStackShow(PDStackRef stack)
{
    printf("{ ");
    PDStackShow_(stack);
    printf(" }\n");
}


