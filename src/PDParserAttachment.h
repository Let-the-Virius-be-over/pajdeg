//
// PDParserAttachment.h
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
 @file PDParserAttachment.h PDF parser attachment header file.
 
 @ingroup PDPARSER
 
 @brief An attachment between two parsers
 
 @ingroup PDUSER
 
 @{
 */

#ifndef INCLUDED_PDParserAttachment_h
#define INCLUDED_PDParserAttachment_h

#include "PDDefines.h"

/**
 *  Create a foreign parser attachment, so that importing objects into parser from foreignParser becomes possible.
 *
 *  @param parser        The native parser, which will receive imported objects
 *  @param foreignParser The foreign parser, which will provide existing objects in import operations
 *
 *  @return A retained PDParserAttachment instance
 */
extern PDParserAttachmentRef PDParserAttachmentCreate(PDParserRef parser, PDParserRef foreignParser);

/**
 *  Iterate over the given foreign object recursively, creating appended objects for every indirect reference encountered. The resulting (new) indirect references, along with any direct references and associated stream (if any) are copied into a new object which is returned to the caller autoreleased.
 *
 *  A set of keys which should not be imported can be set. This set only applies to the object itself, and is not recursive. Thus, if an excluded key is encountered in one of the indirectly referenced objects, it will be included.
 *
 *  @param parser           The parser which should import the foreign object
 *  @param attachment       The attachment to a foreign parser
 *  @param foreignObject    The foreign object
 *  @param excludeKeys      Array of strings that should not be imported, if any
 *  @param excludeKeysCount Size of excludeKeys array
 *
 *  @return The new, native object based on the foreign object
 */
extern PDObjectRef PDParserAttachmentImportObject(PDParserAttachmentRef attachment, PDObjectRef foreignObject, const char **excludeKeys, PDInteger excludeKeysCount);

#endif
