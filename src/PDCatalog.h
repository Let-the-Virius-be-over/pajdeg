//
// PDCatalog.h
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
 @file PDCatalog.h PDF catalog header file.
 
 @ingroup PDCATALOG
 
 @defgroup PDCATALOG PDCatalog
 
 @brief PDF catalog, whose primary purpose is to map PDF objects to pages in a PDF document.
 
 @ingroup PDPIPE_CONCEPT

 The PDF catalog, usually derived from the "root" object of the PDF, consists of a media box (a rectangle defining the size of all pages) as well as an array of "kids", each kid being a page in the PDF. 
 
 The PDParser instantiates the catalog to the root object's Pages object on demand, e.g. when a task filter is set to some page, or simply when the developer requests it for the first time.
 
 @{
 */

#ifndef INCLUDED_PDCATALOG_h
#define INCLUDED_PDCATALOG_h

#include <sys/types.h>
#include "PDDefines.h"

/**
 Set up a catalog with a PDParser and a catalog object.
 
 The catalog will, via the parser, fetch the information needed to provide a complete representation of the pages supplied by the given catalog object. Normally, the catalog object is the root object of the PDF.
 
 @note It is recommended to use the parser's built-in PDParserGetCatalog() function for getting the root object catalog, if nothing else for sake of efficiency.
 
 @param parser  The PDParserRef instance.
 @param catalog The catalog object.
 @return The PDCatalog instance.
 */
extern PDCatalogRef PDCatalogCreateWithParserForObject(PDParserRef parser, PDObjectRef catalog);

/**
 Determine the object ID for the given page number, or throw an assertion if the page number is out of bounds.
 
 @warning Page numbers begin at 1, not 0.
 
 @param catalog The catalog object.
 @param pageNumber The page number whose object ID should be provided.
 @return The object ID.
 */
extern PDInteger PDCatalogGetObjectIDForPage(PDCatalogRef catalog, PDInteger pageNumber);

/**
 Determine the number of pages in this catalog.
 
 @warning Page numbers are valid in the range [1 .. pc], not [0 .. pc-1] where pc is the returned value.
 
 @param catalog The catalog object.
 @return The number of pages contained.
 */
extern PDInteger PDCatalogGetPageCount(PDCatalogRef catalog);

#endif

/** @} */