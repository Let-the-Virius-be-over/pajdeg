//
// PDAnnot.c
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
#include "PDAnnot.h"
#include "pd_internal.h"

#include "PDAnnotsGroup.h"
#include "PDParser.h"

void PDAnnotDestroy(PDAnnotRef annot)
{
    PDRelease(annot->a);
    PDRelease(annot->uri);
    if (annot->subtype) free(annot->subtype);
}
/*
 PDAnnotsGroupRef      annots;        ///< Annots object containing the annotation.
 PDObjectRef      a;             ///< Action object, if any
 char            *subtype;       ///< Subtype, e.g. "Link"
 PDRect           rect;          ///< Rectangle
 */

PDAnnotRef PDAnnotCreateWithAnnots(PDAnnotsGroupRef annots)
{
    PDAnnotRef annot = PDAlloc(sizeof(struct PDAnnot), PDAnnotDestroy, true);
    annot->annots = annots;
    return annot;
}

void PDAnnotSetObject(PDAnnotRef annot, PDObjectRef object)
{
    PDAssert(! annot->object); // crash = trying to set the object for an annotation more than once
    
    PDInteger obid;
    const char *sref;
    pd_stack defs;
    
    PDParserRef parser = annot->annots->parser;
    annot->object = PDRetain(object);
    
    sref = PDObjectGetDictionaryEntry(object, "A");
    if (sref) {
        obid = PDIntegerFromString(sref);
        defs = PDParserLocateAndCreateDefinitionForObject(parser, obid, true);
        annot->a = PDObjectCreateFromDefinitionsStack(obid, defs);
        
        sref = PDObjectGetDictionaryEntry(annot->a, "URI");
        if (sref) {
            obid = PDIntegerFromString(sref);
            defs = PDParserLocateAndCreateDefinitionForObject(parser, obid, true);
            annot->uri = PDObjectCreateFromDefinitionsStack(obid, defs);
        }
    }
    
    sref = PDObjectGetDictionaryEntry(object, "Subtype");
    if (sref) {
        annot->subtype = strdup(sref);
    }
    
    sref = PDObjectGetDictionaryEntry(object, "Rect");
    if (sref) {
        PDRectReadFromArrayString(annot->rect, sref);
    }
}

void PDAnnotRemove(PDAnnotRef annot)
{
    if (annot->a) PDObjectDelete(annot->a);
    if (annot->uri) PDObjectDelete(annot->uri);
    PDAnnotsGroupDeleteAnnotation(annot->annots, annot);
}

void PDAnnotMakeRectLinkToURL(PDAnnotRef annot, PDRect rect, const char *urlString)
{
    PDAnnotSetRect(annot, rect);
    PDAnnotSetActionURLString(annot, urlString);
}

void PDAnnotSetRect(PDAnnotRef annot, PDRect rect)
{
    annot->rect = rect;
}

PDRect PDAnnotGetRect(PDAnnotRef annot)
{
    return annot->rect;
}

void PDAnnotSetActionURLString(PDAnnotRef annot, const char *urlString)
{
    PDParserRef parser = annot->annots->parser;
    PDObjectRef a = annot->a;
    if (a == NULL) {
        // we need a whole new actions object
        annot->a = a = PDParserCreateAppendedObject(parser);
    }
    
    PDObjectRef uri = annot->uri;
    if (uri == NULL) {
        // we need a URI object
        annot->uri = uri = PDParserCreateAppendedObject(parser);
    }
    
    // set up action
    PDObjectSetDictionaryEntry(a, "Type", "/Action");
    PDObjectSetDictionaryEntry(a, "S", "/URI");
    PDObjectSetDictionaryEntry(a, "URI", PDObjectGetReferenceString(uri));
    
    // set up URI object
    PDObjectSetValue(uri, urlString); // does this require "(" + urlString + ")" ???
}

const char *PDAnnotGetURLString(PDAnnotRef annot)
{
    return annot->uri ? PDObjectGetValue(annot->uri) : NULL;
}

