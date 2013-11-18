//
// PDOperator.c
//
// Copyright (c) 2013 Karl-Johan Alm (http://github.com/kallewoof)
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
#include "PDOperator.h"
#include "PDState.h"

#include "pd_internal.h"

char *PDOperatorSymbolsWhitespace = "\x00\x09\x0A\x0C\x0D ";    // 0, 9, 10, 12, 13, 32 (character codes)
char *PDOperatorSymbolsDelimiters = "()<>[]{}/%";               // (, ), <, >, [, ], {, }, /, % (characters)
//char *PDOperatorSymbolsNumeric = "0123456789";                  // 0-9 (character range)
//char *PDOperatorSymbolsAlpha = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";        // a-zA-Z 
//char *PDOperatorSymbolsAlphanumeric = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; // a-zA-Z0-9

char *PDOperatorSymbolGlob = NULL;

void PDOperatorSymbolGlobSetup()
{
    PDInteger i;

    if (PDOperatorSymbolGlob != NULL) return;
    
    PDOperatorSymbolGlob = calloc(256, sizeof(char));
    for (i = strlen(&PDOperatorSymbolsWhitespace[1]); i >= 0; i--)
        PDOperatorSymbolGlob[(unsigned char)PDOperatorSymbolsWhitespace[i]] = PDOperatorSymbolGlobWhitespace;
    for (i = strlen(PDOperatorSymbolsDelimiters)-1; i >= 0; i--) 
        PDOperatorSymbolGlob[(unsigned char)PDOperatorSymbolsDelimiters[i]] = PDOperatorSymbolGlobDelimiter;
}

void PDOperatorSymbolGlobClear()
{
    if (PDOperatorSymbolGlob == NULL) return;
    free(PDOperatorSymbolGlob);
    PDOperatorSymbolGlob = NULL;
}

char PDOperatorSymbolGlobDefine(char *str)
{
    if (PDOperatorSymbolGlob[(unsigned char)str[0]] == PDOperatorSymbolGlobDelimiter) 
        return PDOperatorSymbolGlobDelimiter;

    char c;
    PDBool numeric, real;
    numeric = true;
    real = false;
    PDSymbolDetermineNumeric(numeric, real, c, str, strlen(str));

    return (numeric
            ? PDOperatorSymbolExtNumeric 
            : PDOperatorSymbolGlobRegular);
}

void PDOperatorDestroy(PDOperatorRef op)
{
    switch (op->type) {
        case PDOperatorPopVariable:
            free(op->key);
            break;
        case PDOperatorPushState:
            PDRelease(op->pushedState);
        default:
            break;
    }
    if (op->next) {
        PDRelease(op->next);
    }
}

PDOperatorRef PDOperatorCreate(PDOperatorType type)
{
    PDOperatorRef op = PDAlloc(sizeof(struct PDOperator), PDOperatorDestroy, false);
    op->type = type;
    op->next = NULL;
    op->key = NULL;
    return op;
}

PDOperatorRef PDOperatorCreateWithKey(PDOperatorType type, const char *key)
{
    PDOperatorRef op = PDOperatorCreate(type);
    op->key = strdup(key);
    return op;
}

PDOperatorRef PDOperatorCreateWithPushedState(PDStateRef pushedState)
{
    PDOperatorRef op = PDOperatorCreate(PDOperatorPushState);
    op->pushedState = PDRetain(pushedState);
    return op;
}

void PDOperatorAppendOperator(PDOperatorRef op, PDOperatorRef next)
{
    PDAssert(op != next);
    while (op->next) {
        op = op->next;
        PDAssert(op != next);
    }
    op->next = PDRetain(next);
}

PDOperatorRef PDOperatorCreateFromDefinition(const void **defs)
{
    PDInteger i = 0;
    PDOperatorRef result, op, op2;
    result = op = NULL;
    PDOperatorType t;
    while (defs[i]) {
        t = (PDOperatorType)defs[i++];
        switch (t) {
            case PDOperatorPopVariable:
                op2 = PDOperatorCreateWithKey(t, defs[i++]);
                break;
            case PDOperatorPullBuildVariable:
            case PDOperatorStoveComplex:
            case PDOperatorPushComplex:
                op2 = PDOperatorCreate(t);
                op2->identifier = (PDID)defs[i++];
                break;
            case PDOperatorPushState:
            case PDOperatorPushWeakState:
                op2 = PDOperatorCreateWithPushedState((PDStateRef)defs[i++]);
                if (t == PDOperatorPushWeakState) {
                    op2->type = PDOperatorPushWeakState;
                    PDRelease(op2->pushedState);
                }
                break;
            default:
                op2 = PDOperatorCreate(t);
                break;
        }
        if (op) {
            /// @todo CLANG doesn't like homemade retaining
            PDOperatorAppendOperator(op, op2);
            PDRelease(op2);
            op = op2;
        } else {
            result = op = op2;
        }
    }
    return result;
}

void PDOperatorCompileStates(PDOperatorRef op)
{
    while (op) {
        if (op->type == PDOperatorPushState || op->type == PDOperatorPushWeakState) 
            PDStateCompile(op->pushedState);
        op = op->next;
    }
}
