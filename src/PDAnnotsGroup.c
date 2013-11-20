//
// PDAnnotsGroup.c
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

#include "Pajdeg.h"
#include "pd_internal.h"

#include "PDAnnotsGroup.h"
#include "PDAnnot.h"
#include "PDObject.h"
#include "PDParser.h"

void PDAnnotsGroupDestroy(PDAnnotsGroupRef annots)
{
    PDRelease(annots->object);
    for (int i = 0; i < annots->count; i++) 
        PDRelease(annots->annots[i]);
}
/*
 struct PDAnnotsGroup {
    PDParserRef parser;             ///< The parser owning the annotations array.
    PDObjectRef object;             ///< The object associated with the annotations.
    PDInteger   count;              ///< Number of annotations.
    PDAnnotRef *annots;             ///< Array of annotations.
 };
 */

PDAnnotsGroupRef PDAnnotsGroupCreateWithParserForObject(PDParserRef parser, PDObjectRef annotObject)
{
    PDAnnotsGroupRef agrp = PDAlloc(sizeof(struct PDAnnot), PDAnnotsGroupDestroy, false);
    agrp->parser = parser;
    agrp->object = PDRetain(annotObject);
    agrp->count = agrp->capacity = PDObjectGetArrayCount(annotObject);
    agrp->annots = malloc(sizeof(PDAnnotRef) * agrp->count);

    PDInteger obid;
    const char *sref;
    pd_stack defs;
    PDAnnotRef annot;
    for (int i = 0; i < agrp->count; i++) {
        sref = PDObjectGetArrayElementAtIndex(annotObject, i);
        annot = agrp->annots[i] = PDAnnotCreateWithAnnots(agrp);
        annot->annots = agrp;

        obid = PDIntegerFromString(sref);
        defs = PDParserLocateAndCreateDefinitionForObject(parser, obid, true);
        PDAnnotSetObject(annot, PDObjectCreateFromDefinitionsStack(obid, defs));
    }
    
    return agrp;
}

void PDAnnotsGroupSynchronize(PDAnnotsGroupRef annots)
{
    // first off, we set the array
}

PDInteger PDAnnotsGroupGetAnnotationCount(PDAnnotsGroupRef annots)
{
    return annots->count;
}

PDAnnotRef PDAnnotsGroupGetAnnotationAtIndex(PDAnnotsGroupRef annots, PDInteger index)
{
    return annots->annots[index];
}

PDInteger PDAnnotsGroupGetIndexOfAnnotation(PDAnnotsGroupRef annots, PDAnnotRef annot)
{
    for (int i = 0; i < annots->count; i++) 
        if (annot == annots->annots[i]) return i;
    return -1;
}

PDBool PDAnnotsGroupDeleteAnnotationAtIndex(PDAnnotsGroupRef annots, PDInteger index)
{
    if (index < 0 || index >= annots->count) return false;
    PDRelease(annots->annots[index]);
    
    for (int i = index + 1; i < annots->count; i++) 
        annots->annots[i-1] = annots->annots[i];
    annots->count--;
    
    return true;
}

PDBool PDAnnotsGroupDeleteAnnotation(PDAnnotsGroupRef annots, PDAnnotRef annot)
{
    return PDAnnotsGroupDeleteAnnotationAtIndex(annots, 
                                                PDAnnotsGroupGetIndexOfAnnotation(annots, annot));
}

PDAnnotRef* PDAnnotsGroupGetAnnotationArray(PDAnnotsGroupRef annots)
{
    return annots->annots;
}

PDAnnotRef PDAnnotsGroupCreateAnnotation(PDAnnotsGroupRef annots)
{
    if (annots->capacity == annots->count) {
        annots->capacity++;
        annots->annots = realloc(annots->annots, sizeof(PDAnnotRef) * annots->capacity);
    }
    return (annots->annots[annots->count++] = PDAnnotCreateWithAnnots(annots)); // assignment
}
