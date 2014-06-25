//
// PDContentStreamTextExtractor.h
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
 @file PDContentStreamTextExtractor.h PDF content stream text extractor header file.
 
 @ingroup PDCONTENTSTREAM
 
 Extracts text from a PDF as a string.
 
 @{
 */

#ifndef INCLUDED_PDContentStreamTextExtractor_h
#define INCLUDED_PDContentStreamTextExtractor_h

#include "PDContentStream.h"

/**
 *  Create a content stream configured to write all string values of the stream into a string, allocated to fit any amount of content, then pointing *result to the string.
 *
 *  @param object Object whose content stream should have its text extracted
 *  @param result Pointer to char * into which results are to be written
 *
 *  @return A pre-configured content stream
 */
extern PDContentStreamRef PDContentStreamCreateTextExtractor(PDObjectRef object, char **result);

#endif

/** @} */

/** @} */
