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
#include "PDString.h"

#include "pd_internal.h"

PDNumberRef PDNullObject = NULL;

void PDNumberDestroy(PDNumberRef n)
{
    
}

PDNumberRef PDNumberCreateWithInteger(PDInteger i)
{
    PDNumberRef n = PDAllocTyped(PDInstanceTypeNumber, sizeof(struct PDNumber), PDNumberDestroy, false);
    n->type = PDObjectTypeInteger;
    n->i = i;
    return n;
}

PDNumberRef PDNumberCreateWithSize(PDSize s)
{
    PDNumberRef n = PDAllocTyped(PDInstanceTypeNumber, sizeof(struct PDNumber), PDNumberDestroy, false);
    n->type = PDObjectTypeSize;
    n->s = s;
    return n;
}

PDNumberRef PDNumberCreateWithReal(PDReal r)
{
    PDNumberRef n = PDAllocTyped(PDInstanceTypeNumber, sizeof(struct PDNumber), PDNumberDestroy, false);
    n->type = PDObjectTypeReal;
    n->r = r;
    return n;
}

PDNumberRef PDNumberCreateWithBool(PDBool b)
{
    PDNumberRef n = PDAllocTyped(PDInstanceTypeNumber, sizeof(struct PDNumber), PDNumberDestroy, false);
    n->type = PDObjectTypeBoolean;
    n->b = b;
    return n;
}

PDNumberRef PDNumberCreateWithCString(const char *cString)
{
    // constant check
    if (cString[0] == 't' || cString[0] == 'f') {
        if (0 == strcmp(cString, "true"))  return PDNumberCreateWithBool(true);
        if (0 == strcmp(cString, "false")) return PDNumberCreateWithBool(false);
        PDWarn("unknown constant value %s; passing back PDStringRef instead of PDNumberRef!", cString);
        return (void *) PDStringCreate(strdup(cString));
    }
    // integer or real; determined by existence of a dot
    PDBool realValue = false;
    PDInteger len = strlen(cString);
    for (PDInteger i = 0; i < len; i++) 
        if (cString[i] == '.') {
            realValue = true;
            break;
        }
    
    if (realValue) {
        return PDNumberCreateWithReal(PDRealFromString(cString));
    }
    return PDNumberCreateWithInteger(PDIntegerFromString(cString));
}

PDInteger PDNumberGetInteger(PDNumberRef n)
{
    if (n == NULL) return 0;
    if (PDInstanceTypeString == PDResolve(n)) return PDIntegerFromString(((PDStringRef)n)->data);
    
    switch (n->type) {
        case PDObjectTypeInteger:   return n->i;
        case PDObjectTypeSize:      return n->s;
        case PDObjectTypeBoolean:   return n->b;
        default:                    return (PDInteger)n->r;
    }
}

PDSize PDNumberGetSize(PDNumberRef n)
{
    if (n == NULL) return 0;
    if (PDInstanceTypeString == PDResolve(n)) return PDIntegerFromString(((PDStringRef)n)->data);
    
    switch (n->type) {
        case PDObjectTypeInteger:   return n->i;
        case PDObjectTypeSize:      return n->s;
        case PDObjectTypeBoolean:   return n->b;
        default:                    return (PDSize)n->r;
    }
}

PDReal PDNumberGetReal(PDNumberRef n)
{
    if (n == NULL) return 0;
    if (PDInstanceTypeString == PDResolve(n)) return PDRealFromString(((PDStringRef)n)->data);

    switch (n->type) {
        case PDObjectTypeInteger:   return (PDReal)n->i;
        case PDObjectTypeSize:      return (PDReal)n->s;
        case PDObjectTypeBoolean:   return (PDReal)n->b;
        default:                    return n->r;
    }
}

PDBool PDNumberGetBool(PDNumberRef n)
{
    if (n == NULL) return false;
    if (PDInstanceTypeString == PDResolve(n)) return PDStringEqualsCString((PDStringRef)n, "true") || PDIntegerFromString(((PDStringRef)n)->data) != 0;

    switch (n->type) {
        case PDObjectTypeInteger:   return 0 != n->i;
        case PDObjectTypeSize:      return 0 != n->s;
        case PDObjectTypeBoolean:   return n->b;
        default:                    return 0 != n->r;
    }
}

PDInteger PDNumberPrinter(void *inst, char **buf, PDInteger offs, PDInteger *cap)
{
    PDInstancePrinterInit(PDNumberRef, 0, 1);
    
    if (PDNullObject == i) {
        PDInstancePrinterRequire(5, 5);
        char *bv = *buf;
        strcpy(&bv[offs], "null");
        return offs + 4;
    }
    
    int len;
    char tmp[25];
    switch (i->type) {
        case PDObjectTypeInteger:
            len = sprintf(tmp, "%ld", i->i);
            break;
        case PDObjectTypeSize:
            len = sprintf(tmp, "%lu", i->s);
            break;
        case PDObjectTypeBoolean:
            len = sprintf(tmp, i->b ? "true" : "false");
            break;
        default:
            len = sprintf(tmp, "%g", i->r);
            
    }
    PDInstancePrinterRequire(len+1, len+5);
    char *bv = *buf;
    strcpy(&bv[offs], tmp);
    return offs + len;
}
