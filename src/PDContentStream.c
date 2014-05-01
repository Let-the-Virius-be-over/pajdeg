//
// PDContentStream.c
//
// Copyright (c) 2014 Karl-Johan Alm (http://github.com/kallewoof)
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

#include "pd_internal.h"
#include "PDObject.h"
#include "PDBTree.h"
#include "PDOperator.h"
#include "pd_array.h"
#include "pd_stack.h"

void PDContentStreamDestroy(PDContentStreamRef cs)
{
    PDRelease(cs->ob);
    PDRelease(cs->opertree);
    pd_stack_destroy(&cs->opers);
    pd_array_destroy(cs->args);
    
    PDOperatorSymbolGlobClear();
}

PDContentStreamRef PDContentStreamCreateWithObject(PDObjectRef object)
{
    PDOperatorSymbolGlobSetup();
    
    PDContentStreamRef cs = PDAlloc(sizeof(struct PDContentStream), PDContentStreamDestroy, false);
    cs->ob = PDRetain(object);
    cs->opertree = PDBTreeCreate(NULL, 0, 10000000, 4);
    cs->opers = NULL;
    cs->args = pd_array_with_capacity(8);
    return cs;
}

void PDContentStreamAttachOperator(PDContentStreamRef cs, const char *opname, PDContentOperatorFunc op)
{
    int oplen = strlen(opname);
    PDBTreeInsert(cs->opertree, PDBT_KEY_STR(opname, oplen), op);
}

void PDContentStreamExecute(PDContentStreamRef cs)
{
    PDInteger strlen;
    char *str;

    const char *stream = PDObjectGetStream(cs->ob);
    PDInteger   len    = PDObjectGetStreamLength(cs->ob);
    PDInteger   mark   = 0;

    pd_stack *opers = &cs->opers;
    pd_stack_destroy(opers);
    pd_array_clear(cs->args);

    for (PDInteger i = 0; i <= len; i++) {
        if (i == len || PDOperatorSymbolGlob[stream[i]] == PDOperatorSymbolGlobWhitespace) {
            if (i > mark) {
                str = strndup(&stream[mark], i - mark);
                strlen = i - mark;
                PDContentOperatorFunc op = PDBTreeGet(cs->opertree, PDBT_KEY_STR(str, strlen));
                
                // have we matched a string to an operator?
                if (op) {
                    PDInteger argc = pd_array_get_count(cs->args);
                    const char **args = pd_array_create_args(cs->args);
                    
                    PDOperatorState state = (*op)(cs, args, argc);
                    
                    free(args);
                    pd_array_clear(cs->args);
                    
                    if (state == PDOperatorStatePush) {
                        pd_stack_push_key(opers, str);
                    } else if (state == PDOperatorStatePop) {
                        free(pd_stack_pop_key(opers));
                    }
                } 
                
                // we conditionally stuff arguments for numeric values and '/' somethings only; we do this to prevent a function from getting a ton of un-handled operators as arguments
                else if ((str[0] >= '0' && str[0] <= '9') || str[0] == '/' || str[0] == '0' || str[0] == '.') {
                    pd_array_append(cs->args, str);
                } 
                
                // here, we believe we've run into an operator, so we throw away accumulated arguments and start anew
                else {
                    // unhandled operator
                    pd_array_clear(cs->args);
                }
                
                free(str);
            }
            
            mark = i + 1;
        }
    }
}
