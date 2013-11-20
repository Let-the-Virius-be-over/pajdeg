//
// PDAnnot.h
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
 @file PDAnnot.h PDF annotation header file.
 
 @ingroup PDANNOT
 
 @defgroup PDANNOT PDAnnot
 
 @brief A PDF annotation, such as a link.
 
 @{
 */

#ifndef INCLUDED_PDANNOT_H
#define INCLUDED_PDANNOT_H

#include "PDDefines.h"

/**
 Create an empty annotation object.
 
 @param annots The annots instance which should contain this annot.
 */
extern PDAnnotRef PDAnnotCreateWithAnnots(PDAnnotsGroupRef annots);

/**
 Remove the annotation and its associated objects from the PDF.
 
 @param annot The annotation to exclude in the ouptut PDF.
 */
extern void PDAnnotRemove(PDAnnotRef annot);

/**
 Set the object for the annotation. This will configure the annotation according to the object's settings.
 
 @param annot The annotation.
 @param object The annotation object.
 */
extern void PDAnnotSetObject(PDAnnotRef annot, PDObjectRef object);

/**
 Modify annotation to be a link on the page, with a rectangular surface, pointing to a given URI.
 
 This is a convenience method for setting the rect, subtype, and action values to the given rect, /Link, and a URI action for the given URL.
 
 @param annot The annotation.
 @param rect The rect (as x1,y1, x2,y2 coordinates).
 @param urlString The URL.
 */
extern void PDAnnotMakeRectLinkToURL(PDAnnotRef annot, PDRect rect, const char *urlString);

/**
 Set the rect for the annotation.
 
 @param annot The annotation.
 @param rect The rect (as x1,y1, x2,y2 coordinates).
 */
extern void PDAnnotSetRect(PDAnnotRef annot, PDRect rect);

/**
 Get the rect for the annotation.
 
 @param annot The annotation.
 @return The rect.
 */
extern PDRect PDAnnotGetRect(PDAnnotRef annot);

/**
 Set the action for the annotation to "open URL" with the given string as the URL to open.
 
 @param annot The annotation.
 @param urlString The URL to open when the annotation action is triggered.
 */
extern void PDAnnotSetActionURLString(PDAnnotRef annot, const char *urlString);

/**
 Get the URL string for the annotation.
 
 @note If the annotation is not an action or is not an open-URL action, calling this method is undefined.
 
 @param annot The annotation.
 */
extern const char *PDAnnotGetURLString(PDAnnotRef annot);

#endif // INCLUDED_PDANNOT_H

/** @} */
