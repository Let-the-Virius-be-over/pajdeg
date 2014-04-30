//
// PDContentStream.h
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

/**
 @file PDContentStream.h PDF content stream header file.
 
 @ingroup PDCONTENTSTREAM
 
 @defgroup PDCONTENTSTREAM PDContentStream
 
 @brief A content stream, i.e. a stream containing Adobe commands, such as for writing text and similar.
 
 @ingroup PDUSER
 
 Content streams may have a wide variety of purposes. One such purpose is the drawing of content on the page. 
 The content stream module contains support for working with the state machine to process the content in a
 variety of ways. 
 
 @{
 */

#ifndef ICViewer_PDContentStream_h
#define ICViewer_PDContentStream_h

#include "PDDefines.h"

/**
 *  Set up a content stream based on an existing object and its (existing) stream.
 *
 *  @param object The object containing a stream.
 *
 *  @return The content stream object.
 */
extern PDContentStreamRef PDContentStreamCreateWithObject(PDObjectRef object);



#endif

/** @} */

/** @} */
