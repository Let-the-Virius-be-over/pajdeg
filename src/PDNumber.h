//
// PDNumber.h
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

/**
 @file PDNumber.h Number wrapper
 
 @ingroup PDNUMBER
 
 @defgroup PDNUMBER PDNumber
 
 @brief A wrapper around PDF numbers.
 
 PDNumber objects exist to provide a unified way to maintain and convert between different number types. It also serves as a retainable wrapper around numeric values.
 
 @{
 */

#ifndef INCLUDED_PDNUMBER_H
#define INCLUDED_PDNUMBER_H

#include "PDDefines.h"

// retained variants
extern PDNumberRef PDNumberCreateWithInteger(PDInteger i);
extern PDNumberRef PDNumberCreateWithSize(PDSize s);
extern PDNumberRef PDNumberCreateWithReal(PDReal r);
extern PDNumberRef PDNumberCreateWithBool(PDBool b);
extern PDNumberRef PDNumberCreateWithCString(const char *cString);

// autoreleased variants
#define PDNumberWithInteger(i)  PDAutorelease(PDNumberCreateWithInteger(i))
#define PDNumberWithSize(s)     PDAutorelease(PDNumberCreateWithSize(s))
#define PDNumberWithReal(r)     PDAutorelease(PDNumberCreateWithReal(r))
#define PDNumberWithBool(b)     PDAutorelease(PDNumberCreateWithBool(b))

// NULL n safe
extern PDInteger PDNumberGetInteger(PDNumberRef n);
extern PDSize PDNumberGetSize(PDNumberRef n);
extern PDReal PDNumberGetReal(PDNumberRef n);
extern PDBool PDNumberGetBool(PDNumberRef n);

extern PDInteger PDNumberPrinter(void *inst, char **buf, PDInteger offs, PDInteger *cap);

#endif // INCLUDED_PDNUMBER_H

/** @} */
