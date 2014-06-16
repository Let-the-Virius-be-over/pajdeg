//
// PDNumber.c
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
#include "PDNumber.h"
#include "pd_stack.h"
#include "pd_crypto.h"

#include "pd_internal.h"

void PDNumberDestroy(PDNumberRef n)
{
    
}

PDNumberRef PDNumberCreateWithInteger(PDInteger i)
{
    PDNumberRef n = PDAlloc(sizeof(struct PDNumber), PDNumberDestroy, false);
    n->type = PDObjectTypeInteger;
    n->i = i;
    return n;
}

PDNumberRef PDNumberCreateWithReal(PDReal r)
{
    PDNumberRef n = PDAlloc(sizeof(struct PDNumber), PDNumberDestroy, false);
    n->type = PDObjectTypeInteger;
    n->r = r;
    return n;
}

PDNumberRef PDNumberCreateWithBool(PDBool b)
{
    PDNumberRef n = PDAlloc(sizeof(struct PDNumber), PDNumberDestroy, false);
    n->type = PDObjectTypeInteger;
    n->b = b;
    return n;
}

PDInteger PDNumberGetInteger(PDNumberRef n)
{
    if (n == NULL) return 0;
    switch (n->type) {
        case PDObjectTypeInteger:
            return n->i;
        case PDObjectTypeBoolean:
            return n->b;
        default:
            return (PDInteger)n->r;
    }
}

PDReal PDNumberGetReal(PDNumberRef n)
{
    if (n == NULL) return 0;
    switch (n->type) {
        case PDObjectTypeInteger:
            return (PDReal)n->i;
        case PDObjectTypeBoolean:
            return (PDReal)n->b;
        default:
            return n->r;
    }
}

PDBool PDNumberGetBool(PDNumberRef n)
{
    if (n == NULL) return false;
    switch (n->type) {
        case PDObjectTypeInteger:
            return 0 != n->i;
        case PDObjectTypeBoolean:
            return n->b;
        default:
            return 0 != n->r;
    }
}

PDInteger PDNumberPrinter(void *inst, char **buf, PDInteger offs, PDInteger *cap)
{
    PDInstancePrinterInit(PDNumberRef, 0, 1);
    
    int len;
    char tmp[25];
    switch (i->type) {
        case PDObjectTypeInteger:
            len = sprintf(tmp, "%ld", i->i);
        case PDObjectTypeBoolean:
            len = sprintf(tmp, "%d", i->b);
        default:
            len = sprintf(tmp, "%f", i->r);
    }
    PDInstancePrinterRequire(len+1, len+5);
    char *bv = *buf;
    strcpy(&bv[offs], tmp);
    return offs + len;
}
