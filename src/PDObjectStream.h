//
// PDObjectStream.h
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
 @file PDObjectStream.h PDF object stream header file.
 
 @ingroup PDOBJECTSTREAM
 
 @defgroup PDOBJECTSTREAM PDObjectStream
 
 @brief A PDF object stream, i.e. a stream of PDF objects inside a stream.
 
 @ingroup PDUSER
 
 Normally, objects are located directly inside of the PDF, but an alternative way is to keep objects as so called object streams (Chapter 3.4.6 of PDF specification v 1.7, p. 100). 
 
 When a filtering task is made for an object that is determined to be located inside of an object stream, supplementary tasks are automatically set up to generate the object stream instance of the container object and to present the given object as a regular, mutable instance to the requesting task. Upon completion (that is, when returning from the task callback), the object stream is "committed" as stream content to the actual containing object, which in turn is written to the output as normal.
 
 @{
 */

#ifndef ICViewer_PDObjectStream_h
#define ICViewer_PDObjectStream_h

#include "PDDefines.h"

/**
 Get the object with the given ID out of the object stream. 
 
 Object is mutable with regular object mutability conditions applied. 
 
 @param obstm The object stream.
 @param obid The id of the object to fetch.
 @return The object, or NULL if not found.
 */
extern PDObjectRef PDObjectStreamGetObjectByID(PDObjectStreamRef obstm, PDInteger obid);

/**
 Get the object at the given index out of the object stream.
 
 Assertion is thrown if index is out of bounds.
 
 The index of an object stream object can be determined through the /N dictionary entry in the object itself.
 
 @param obstm The object stream.
 @param index Object stream index.
 @return The object.
 */
extern PDObjectRef PDObjectStreamGetObjectAtIndex(PDObjectStreamRef obstm, PDInteger index);

#endif

/** @} */

/** @} */
