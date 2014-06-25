//
// PDContentStream.c
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

#include "Pajdeg.h"

#include "PDContentStream.h"
#include "pd_internal.h"
#include "PDObject.h"
#include "PDSplayTree.h"
#include "PDOperator.h"
#include "PDArray.h"
#include "pd_stack.h"
#include "PDString.h"
#include "PDNumber.h"

void PDContentStreamOperationDestroy(PDContentStreamOperationRef op)
{
    free(op->name);
    pd_stack_destroy(&op->state);
}

PDContentStreamOperationRef PDContentStreamOperationCreate(char *name, pd_stack state)
{
    PDContentStreamOperationRef op = PDAlloc(sizeof(struct PDContentStreamOperation), PDContentStreamOperationDestroy, false);
    op->name = name;
    op->state = state;
    return op;
}

//----

void PDContentStreamDestroy(PDContentStreamRef cs)
{
    while (cs->deallocators) {
        PDDeallocator deallocator = (PDDeallocator) pd_stack_pop_identifier(&cs->deallocators);
        void *userInfo = (void *) pd_stack_pop_identifier(&cs->deallocators);
        (*deallocator)(userInfo);
    }
    pd_stack_destroy(&cs->resetters);
    PDRelease(cs->ob);
    PDRelease(cs->opertree);
    pd_stack_destroy(&cs->opers);
    PDRelease(cs->args);
//    pd_array_destroy(cs->args);
    
//    PDOperatorSymbolGlobClear();
}

PDContentStreamRef PDContentStreamCreateWithObject(PDObjectRef object)
{
    PDOperatorSymbolGlobSetup();
    
    PDContentStreamRef cs = PDAlloc(sizeof(struct PDContentStream), PDContentStreamDestroy, false);
    cs->ob = PDRetain(object);
    cs->opertree = PDSplayTreeCreateWithDeallocator(free);
    cs->opers = NULL;
    cs->deallocators = NULL;
    cs->resetters = NULL;
    cs->args = PDArrayCreateWithCapacity(8);//pd_array_with_capacity(8);
    return cs;
}

void PDContentStreamAttachOperator(PDContentStreamRef cs, const char *opname, PDContentOperatorFunc op, void *userInfo)
{
    void **arr = malloc(sizeof(void*) << 1);
    arr[0] = op;
    arr[1] = userInfo;
    if (opname) {
        unsigned long oplen = strlen(opname);
        
        PDSplayTreeInsert(cs->opertree, PDST_KEY_STR(opname, oplen), arr);
    } else {
        // catch all
        PDSplayTreeInsert(cs->opertree, 0, arr);
    }
}

void PDContentStreamAttachDeallocator(PDContentStreamRef cs, PDDeallocator deallocator, void *userInfo)
{
    pd_stack_push_identifier(&cs->deallocators, (PDID)userInfo);
    pd_stack_push_identifier(&cs->deallocators, (PDID)deallocator);
}

void PDContentStreamAttachResetter(PDContentStreamRef cs, PDDeallocator resetter, void *userInfo)
{
    pd_stack_push_identifier(&cs->resetters, (PDID)userInfo);
    pd_stack_push_identifier(&cs->resetters, (PDID)resetter);
}

void PDContentStreamAttachOperatorPairs(PDContentStreamRef cs, void *userInfo, const void **pairs)
{
    for (PDInteger i = 0; pairs[i]; i += 2) {
        PDContentStreamAttachOperator(cs, pairs[i], pairs[i+1], userInfo);
    }
}

PDSplayTreeRef PDContentStreamGetOperatorTree(PDContentStreamRef cs)
{
    return cs->opertree;
}

void PDContentStreamSetOperatorTree(PDContentStreamRef cs, PDSplayTreeRef operatorTree)
{
    PDAssert(operatorTree); // crash = null operatorTree which is not allowed
    PDRetain(operatorTree);
    PDRelease(cs->opertree);
    cs->opertree = operatorTree;
}

void PDContentStreamInheritContentStream(PDContentStreamRef dest, PDContentStreamRef source)
{
    PDContentStreamSetOperatorTree(dest, PDContentStreamGetOperatorTree(source));
    PDAssert(dest->resetters == NULL); // crash = attempt to inherit from a non-clean content stream; inheriting is limited to the act of copying a content stream into multiple other content streams, and is not meant to be used to supplement a pre-configured stream (e.g. by inheriting multiple content streams) -- configure the stream *after* inheriting, if in the case of only one inherit
    dest->resetters = pd_stack_copy(source->resetters);
    // note that we do not copy the deallocators from dest; dest is the "master", and it is the responsibility of the caller to ensure that master CS'es remain alive until the spawned CS'es are all done
}

void PDContentStreamExecute(PDContentStreamRef cs)
{
    void **catchall;
    PDBool argValue;
    void *arg;
    PDBool termed, escaped;
    char termChar;
    PDContentOperatorFunc op;
    PDContentStreamOperationRef operation;
    PDInteger slen;
    void **arr;
    char *str;
    char ch;
    pd_stack inStack, outStack, argDests;
    PDArrayRef args;

    catchall = PDSplayTreeGet(cs->opertree, 0);
    termChar = 0;
    termed   = false;
    escaped  = false;
    argDests = NULL;
    args     = cs->args;
    
    const char *stream = PDObjectGetStream(cs->ob);
    PDInteger   len    = PDObjectGetExtractedStreamLength(cs->ob);
    PDInteger   mark   = 0;

    pd_stack *opers = &cs->opers;
    pd_stack_destroy(opers);
    PDArrayClear(cs->args);

    for (PDInteger i = 0; i <= len; i++) {
        if (escaped) {
            escaped = false;
        } else {
            escaped = stream[i] == '\\';
            if (termChar) {
                termed = (i + 1 >= len || stream[i] == termChar);
                if (termed) termChar = 0;
            }
            
            if (termChar == 0 && i < len && (stream[i] == '[' || stream[i] == ']' || stream[i] == '(')) {
                if (stream[i] == '(') {
                    termChar = ')';
                } else if (stream[i] == '[') {
                    // push embedded array
                    PDArrayRef eArgs = PDArrayCreateWithCapacity(3);
                    PDArrayAppend(args, eArgs);
                    pd_stack_push_object(&argDests, args);
                    args = eArgs;
                    PDRelease(eArgs);
                } else {
                    // pop out of embedded array
                    PDAssert(argDests); // crash = unexpected ']' was encountered; investigation necessary
                    args = pd_stack_pop_object(&argDests);
                    mark = i;
                }
            }
            
            if (termChar == 0 && (termed || i == len || PDOperatorSymbolGlob[stream[i]] == PDOperatorSymbolGlobDelimiter || PDOperatorSymbolGlob[stream[i]] == PDOperatorSymbolGlobWhitespace)) {
                if (termed + i > mark) {
                    ch = stream[mark];
                    argValue = args != cs->args || ((ch >= '0' && ch <= '9') || ch == '<' || ch == '/' || ch == '.' || ch == '[' || ch == '(' || ch == '-' || ch == ']');
                    if (argValue && PDOperatorSymbolGlob[stream[i]] == PDOperatorSymbolGlobDelimiter) 
                        continue;
                    
                    slen = termed + i - mark;
                    slen -= PDOperatorSymbolGlob[stream[mark+slen-1]] == PDOperatorSymbolGlobWhitespace;
                    str = strndup(&stream[mark], slen);
                    arr = PDSplayTreeGet(cs->opertree, PDST_KEY_STR(str, slen));
                    
                    // if we did not get an operator, switch to catchall
                    if (arr == NULL && ! argValue) arr = catchall;
                    
                    // have we matched a string to an operator?
                    if (arr) {
                        outStack = NULL;
                        inStack  = NULL;
                        
                        if (cs->opers) {
                            operation = cs->opers->info;
                            inStack = operation->state;
                        }
                        
                        cs->lastOperator = str;
                        op = arr[0];
                        PDOperatorState state = (*op)(cs, arr[1], args, inStack, &outStack);
                        
                        PDAssert(NULL == argDests); // crash = ending ] was not encountered for embedded array
                        PDArrayClear(args);
                        
                        if (state == PDOperatorStatePush) {
                            operation = PDContentStreamOperationCreate(strdup(str), outStack);
                            pd_stack_push_object(opers, operation);
                        } else if (state == PDOperatorStatePop) {
                            PDRelease(pd_stack_pop_object(opers));
                        }
                    }
                    
                    // we conditionally stuff arguments for numeric values and '/' somethings only; we do this to prevent a function from getting a ton of un-handled operators as arguments
                    else if (argValue) {
                        switch (str[0]) {
                            case '<':
                                arg = PDStringCreateWithHexString(str);
                                break;
                            case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':case '-':
                                arg = PDNumberCreateWithCString(str);
                                free(str);
                                break;
                            default:
                                arg = PDStringCreate(str);
                        }
                        
                        PDArrayAppend(args, arg);
                        PDRelease(arg);
                        str = NULL;
                    } 
                    
                    // here, we believe we've run into an operator, so we throw away accumulated arguments and start anew
                    else {
                        PDArrayClear(args);
                    }
                    
                    if (str) {
                        free(str);
                    }
                }
                
                // skip over white space, but do not skip over delimiters; these are parts of arguments
                mark = i + (termed || stream[i] == '[' || stream[i] == ']' || PDOperatorSymbolGlob[stream[i]] == PDOperatorSymbolGlobWhitespace);
                
                // we also rewind i if this was a term char; ideally we would rewind for all delimiters before above and just do i + 1 always, but that would result in endless loops for un-termchar delims
                i -= (stream[i] == '(');// || stream[i] == '[');
                termed = false;
            }
        }
    }

    for (pd_stack iter = cs->resetters; iter; iter = iter->prev->prev) {
        PDDeallocator resetter = (PDDeallocator) iter->info;
        void *userInfo = (void *) iter->prev->info;
        (*resetter)(userInfo);
    }
}

const pd_stack PDContentStreamGetOperators(PDContentStreamRef cs)
{
    return cs->opers;
}
