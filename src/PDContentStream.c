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
#include "PDDictionary.h"

// Private declarations

PDArrayRef PDContentStreamPopArray(PDContentStreamRef cs, const char *stream, PDInteger len, PDInteger *iptr);
PDDictionaryRef PDContentStreamPopDictionary(PDContentStreamRef cs, const char *stream, PDInteger len, PDInteger *iptr);
void *PDContentStreamPopValue(PDContentStreamRef cs, const char *stream, PDInteger len, PDInteger *iptr);

// Public methods

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
    PDContentOperatorFunc op;
    PDContentStreamOperationRef operation;
    PDSize slen;
    void **arr;
    char *str;
    pd_stack inStack, outStack, argDests;
    PDArrayRef args;

    catchall = PDSplayTreeGet(cs->opertree, 0);
    argDests = NULL;
    args     = cs->args;
    
    const char *stream = PDObjectGetStream(cs->ob);
    PDInteger   len    = PDObjectGetExtractedStreamLength(cs->ob);

    pd_stack *opers = &cs->opers;
    pd_stack_destroy(opers);
    PDArrayClear(cs->args);

    PDInteger i = 0;
    while (i < len) {
//    for (PDInteger i = 0; i <= len;) {
        arg = PDContentStreamPopValue(cs, stream, len, &i);
        argValue = PDResolve(arg) != PDInstanceTypeString || PDStringTypeEscaped != PDStringGetType(arg) || PDStringIsWrapped(arg); // operators are distinguished by being PDStrings that are NOT wrapped; all other values are either not strings or are wrapped in one way or another
        if (argValue) {
            // add to args
            PDArrayAppend(args, arg);
        } else {
            // check operator
            str = ((PDStringRef)arg)->data;
            slen = ((PDStringRef)arg)->length;
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
            
            // here, we believe we've run into an operator, so we throw away accumulated arguments and start anew
            else {
                PDArrayClear(args);
            }
        }
        while (i < len && PDOperatorSymbolGlob[stream[i]] == PDOperatorSymbolGlobWhitespace) i++;
        PDRelease(arg);
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

// Private implementations

PDArrayRef PDContentStreamPopArray(PDContentStreamRef cs, const char *stream, PDInteger len, PDInteger *iptr)
{
    void *v;
    PDArrayRef res = PDArrayCreateWithCapacity(3);
    PDInteger mark = *iptr;
    
    // accept a leading '[', but don't mind if we were handed the char after
    mark += (stream[mark] == '[');
    while (mark < len) {
        while (mark < len && PDOperatorSymbolGlob[stream[mark]] == PDOperatorSymbolGlobWhitespace)
            mark++;
        if (mark >= len || stream[mark] == ']') break;
        v = PDContentStreamPopValue(cs, stream, len, &mark);
        if (v == NULL) {
            PDWarn("NULL value in content stream (pop value): using 'null' object");
            v = PDNullObject;
        }
        PDArrayAppend(res, v);
        PDRelease(v);
    }
    *iptr = mark + 1;
    return res;
}

PDDictionaryRef PDContentStreamPopDictionary(PDContentStreamRef cs, const char *stream, PDInteger len, PDInteger *iptr)
{
    void *v;
    PDStringRef key;
    PDDictionaryRef res = PDDictionaryCreateWithCapacity(3);
    PDInteger mark = *iptr;
    
    // accept leading '<<'
    mark += (stream[mark] == '<' && stream[mark+1] == '<') + (stream[mark] == '<' && stream[mark+1] == '<');
    while (mark < len) {
        while (mark < len && PDOperatorSymbolGlob[stream[mark]] == PDOperatorSymbolGlobWhitespace) mark++;
        if (mark + 1 >= len || (stream[mark] == '>' && stream[mark+1] == '>')) break;
        key = PDContentStreamPopValue(cs, stream, len, &mark);
        if (PDResolve(key) != PDInstanceTypeString || PDStringGetType(key) != PDStringTypeName) {
            PDWarn("invalid key instance type in content stream dictionary (%s): skipping", PDResolve(key) != PDInstanceTypeString ? "not a string" : "not a name type string");
        } else {
            while (mark < len && PDOperatorSymbolGlob[stream[mark]] == PDOperatorSymbolGlobWhitespace) mark++;
            v = PDContentStreamPopValue(cs, stream, len, &mark);
            if (v == NULL) {
                PDWarn("NULL value in content stream (pop value): using 'null' object");
                v = PDRetain(PDNullObject);
            }
            PDDictionarySetEntry(res, PDStringBinaryValue(key, NULL), v);
            PDRelease(key);
            PDRelease(v);
        }
    }
    *iptr = mark + 2;
    return res;
}

typedef void *(*creatorFunc)(char *);

void *PDContentStreamPopValue(PDContentStreamRef cs, const char *stream, PDInteger len, PDInteger *iptr)
{
    void *res;
    char *str;
    char chtype = 0;
    PDInteger i = *iptr;
    PDInteger mark = i;
    PDBool freeStr = false;
    PDBool escaped = false;
    PDInteger nestLevel = 1;
    
    char nestChar = 0;
    char termChar = 0;
    PDBool termWS = true; // terminate on whitespace
    PDBool termD  = true; // terminate on delimiter
    creatorFunc cf = NULL;
    
    // get hint about value type
    switch (stream[mark]) {
        case '(': 
            // PDString
            mark++;
            nestChar = '(';
            termChar = ')';
            termWS = false;
            termD = false;
            cf = (creatorFunc) PDStringCreate;
            break;
        case '/':
            // PDString (name)
            mark++;
            cf = (creatorFunc) PDStringCreateWithName;
            break;
        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': case '0': case '-':
            // PDNumber
            freeStr = true;
            cf = (creatorFunc) PDNumberCreateWithCString;
            break;
        case '<':
            // hex string or dictionary
            if (mark + 1 < len && stream[mark+1] == '<') {
                // dictionary
                return PDContentStreamPopDictionary(cs, stream, len, iptr);
            }
            // hex string
            termChar = '>';
            termWS = false;
            termD = false;
            cf = (creatorFunc) PDStringCreateWithHexString;
            break;
        case '[':
            return PDContentStreamPopArray(cs, stream, len, iptr);
        default:
            // operator
            cf = (creatorFunc) PDStringCreate;
            break;
    }
    
    while (mark < len) {
        if (escaped) {
            escaped = false;
        } else {
            escaped = (stream[mark] == '\\');
            
            if (stream[mark] == termChar) {
                mark++; // include the terminating character in the resulting string
                nestLevel--;
                if (nestLevel == 0)
                    break;
            }
            
            nestLevel += stream[mark] == nestChar;
            
            if (termWS || termD) {
                chtype = PDOperatorSymbolGlob[stream[mark]];
                if (termWS && chtype == PDOperatorSymbolGlobWhitespace) break;
                if (termD && nestLevel == 1 && chtype == PDOperatorSymbolGlobDelimiter) break;
            }
        }
        mark++;
    }
    
    str = strndup(&stream[i], mark - i);
    *iptr = mark + (termWS && chtype == PDOperatorSymbolGlobWhitespace);
    res = (*cf)(str);
    if (freeStr) free(str);
    
    return res;
}
