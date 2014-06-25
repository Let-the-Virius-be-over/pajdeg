//
// PDContentStreamPrinter.h
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
 @file PDContentStreamPrinter.h PDF content stream printer header file.
 
 @ingroup PDCONTENTSTREAM
 
 Prints the commands of a content stream to e.g. stdout.
 
 @{
 */

#ifndef INCLUDED_PDContentStreamPrinter_h
#define INCLUDED_PDContentStreamPrinter_h

#include "PDContentStream.h"

/**
 *  Create a content stream configured to print out every operation to the given file stream
 *
 *  This is purely for debugging Pajdeg and/or odd PDF content streams, or to learn what the various operators do and how they affect things.
 *
 *  @param object Object whose content stream should be printed
 *  @param stream The file stream to which printing should be made
 *
 *  @return A pre-configured content stream
 */
extern PDContentStreamRef PDContentStreamCreateStreamPrinter(PDObjectRef object, FILE *stream);

#endif

/** @} */

/** @} */
