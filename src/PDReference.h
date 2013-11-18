//
// PDReference.h
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

/**
 @file PDReference.h Object reference header file.
 
 @ingroup PDREFERENCE
 
 @defgroup PDREFERENCE PDReference
 
 @brief A PDF object reference.
 
 @ingroup PDUSER
 
 @{
 */

#ifndef INCLUDED_PDReference_h
#define INCLUDED_PDReference_h

#include "PDDefines.h"

/**
 Create a reference based on a stack. 
 
 The stack may be a dictionary entry containing a ref stack, or just a ref stack on its own.
 
 @param stack The dictionary or reference stack.
 */
extern PDReferenceRef PDReferenceCreateFromStackDictEntry(pd_stack stack);

/**
 Create a reference for the given object ID and generation number.
 
 @param obid Object ID.
 @param genid Generation number.
 */
extern PDReferenceRef PDReferenceCreate(PDInteger obid, PDInteger genid);

/**
 Get the object ID for the reference.
 
 @param reference The reference.
 */
extern PDInteger PDReferenceGetObjectID(PDReferenceRef reference);
/**
 Get the generation ID for the reference.
 
 @param reference The reference.
 */
extern PDInteger PDReferenceGetGenerationID(PDReferenceRef reference);

#endif

/** @} */
