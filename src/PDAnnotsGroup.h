//
// PDAnnotsGroup.h
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
 @file PDAnnotsGroup.h PDF annotations header file.
 
 @ingroup PDANNOTS
 
 @defgroup PDANNOTS PDAnnotsGroup
 
 @brief PDF annotations, such as links
 
 @{
 */

#ifndef INCLUDED_PDANNOTS_H
#define INCLUDED_PDANNOTS_H

#include "PDDefines.h"

/**
 Set up an annotations object with the given parser for the given object.
 
 @param parser The parser instance.
 @param annots The annots object (not a page object).
 @return Annots instance for the given annotations.
 */
extern PDAnnotsGroupRef PDAnnotsGroupCreateWithParserForObject(PDParserRef parser, PDObjectRef annotObject);

/**
 Synchronize annotations with the annots object.
 
 Changes made to annotations will not be reflected in the final PDF unless synchronization is done after all changes are made.
 
 Synchronization can be done multiple times.
 
 @warning If the parser iterates beyond any of the affected objects, the synchronization will fail. If synchronization is not done, despite changes being made, there will be zombie objects (objects that take up space but are not referenced by anything) in the final PDF.
 
 @param annots The annotations group.
 */
extern void PDAnnotsGroupSynchronize(PDAnnotsGroupRef annots);

/**
 Get the number of annotations in the annots instance.
 
 @param annots Annots instance.
 */
extern PDInteger PDAnnotsGroupGetAnnotationCount(PDAnnotsGroupRef annots);

/**
 Get the annotation at the given index.
 
 @param annots Annots instance.
 @param index The index of the annotation that should be returned.
 */
extern PDAnnotRef PDAnnotsGroupGetAnnotationAtIndex(PDAnnotsGroupRef annots, PDInteger index);

/**
 Get the index for the given annotation.
 
 @param annots Annots instance.
 @param annot PDAnnot instance.
 @return Index of annot, or -1 if not found.
 */
extern PDInteger PDAnnotsGroupGetIndexOfAnnotation(PDAnnotsGroupRef annots, PDAnnotRef annot);

/**
 Delete annotation at index.
 
 @param annots Annots instance.
 @param index The index of the annotation that should be deleted.
 @return true if the annotation was removed, false if it was not found.
 */
extern PDBool PDAnnotsGroupDeleteAnnotationAtIndex(PDAnnotsGroupRef annots, PDInteger index);

/**
 Delete annotation.
 
 @param annots Annots instance.
 @param annot The annotation.
 @return true if the annotation was removed, false if it was not found.
 */
extern PDBool PDAnnotsGroupDeleteAnnotation(PDAnnotsGroupRef annots, PDAnnotRef annot);

/**
 Get all annot objects as an array.
 
 The objects in the array may be modified and rearranged, but the size of the array is restricted. 
 
 Calls to PDAnnotsGroupDeleteAnnotationAtIndex() and PDAnnotsGroupCreateAnnotation() will affect the size of the array. The latter will sometimes invalidate the array completely. Thus, the array should only be used as long as creation is not done, but deletion is allowed.
 
 @param annots Annots instance.
 @return Array of PDAnnot objects. Its size can be determined via PDAnnotsGroupGetAnnotsCount().
 */
extern PDAnnotRef* PDAnnotsGroupGetAnnotationArray(PDAnnotsGroupRef annots);

/**
 Create a new annotation.
 
 @param annots Annots instance.
 @return An empty PDAnnot instance, which has been added to the annots array.
 */
extern PDAnnotRef PDAnnotsGroupCreateAnnotation(PDAnnotsGroupRef annots);

#endif // INCLUDED_PDANNOTS_H

/** @} */
