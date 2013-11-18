//
// PDReference.c
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

#include "PDReference.h"
#include "pd_internal.h"
#include "pd_stack.h"
#include "pd_pdf_implementation.h"

void PDReferenceDestroy(PDReferenceRef reference)
{
}

PDReferenceRef PDReferenceCreateFromStackDictEntry(pd_stack stack)
{
    PDReferenceRef ref = PDAlloc(sizeof(struct PDReference), PDReferenceDestroy, false);
    
    // ("de"), <key>, {ref, 789, 0}
    if (PDIdentifies(stack->info, PD_DE))
        stack = stack->prev->prev->info;
    
    // ref, 789, 0
    ref->obid = PDIntegerFromString(stack->prev->info);
    ref->genid = PDIntegerFromString(stack->prev->prev->info);
    return ref;
}

PDReferenceRef PDReferenceCreate(PDInteger obid, PDInteger genid)
{
    PDReferenceRef ref = PDAlloc(sizeof(struct PDReference), PDReferenceDestroy, false);
    ref->obid = obid;
    ref->genid = genid;
    return ref;
}
