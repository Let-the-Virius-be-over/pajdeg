//
// PDPage.c
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

#include "Pajdeg.h"

#include "pd_internal.h"
#include "pd_stack.h"
#include "PDScanner.h"
#include "pd_pdf_implementation.h"
#include "pd_pdf_private.h"
#include "PDStreamFilter.h"
#include "PDObject.h"
#include "PDPage.h"
#include "PDParser.h"
#include "PDCatalog.h"
#include "pd_array.h"
#include "pd_dict.h"

void PDPageDestroy(PDPageRef page)
{
    PDRelease(page->parser);
    PDRelease(page->ob);
}

PDPageRef PDPageCreateForPageWithIndex(PDParserRef parser, PDInteger pageIndex)
{
    PDCatalogRef catalog = PDParserGetCatalog(parser);
    PDInteger obid = PDCatalogGetObjectIDForPage(catalog, pageIndex);
    PDObjectRef ob = PDParserLocateAndCreateObject(parser, obid, true);
    PDPageRef page = PDPageCreateWithObject(parser, ob);
    
//    const char *contentsRef = PDObjectGetDictionaryEntry(ob, "Contents");
//    assert(contentsRef);
//    PDObjectRef contentsOb = PDParserLocateAndCreateObject(parser, PDIntegerFromString(contentsRef), true);
//    char *stream = PDParserLocateAndFetchObjectStreamForObject(parser, contentsOb);
//    printf("page #%ld:\n===\n%s\n", (long)pageIndex, stream);
//    PDRelease(contentsOb);
    
    PDRelease(ob);
    return page;
}

PDPageRef PDPageCreateWithObject(PDParserRef parser, PDObjectRef object)
{
    PDPageRef page = PDAlloc(sizeof(struct PDPage), PDPageDestroy, false);
    page->parser = PDRetain(parser);
    page->ob = PDRetain(object);
    return page;
}

PDTaskResult PDPageInsertionTask(PDPipeRef pipe, PDTaskRef task, PDObjectRef object, void *info)
{
    char *buf = malloc(64);
    pd_stack s, t;
    
    PDObjectRef *userInfo = info;
    PDObjectRef neighbor = userInfo[0];
    PDObjectRef importedObject = userInfo[1];
    free(userInfo);
    
    pd_stack kids = pd_stack_get_dict_key(object->def, "Kids", false);

    if (NULL != neighbor) {
        sprintf(buf, "%ld", PDObjectGetObID(neighbor));
    }
    
    kids = kids->prev->prev->info;
    
    /*
     stack<0x14cdde90> {
     0x401394 ("array")
     0x401388 ("entries")
     stack<0x14cdeb90> {
     stack<0x14cdead0> {
     0x401398 ("ae")
     ...
     }
     }
     }
     */
    
    kids = kids->prev->prev->info;
    /*
     stack<0x14cdeb90> {
     stack<0x14cdead0> {
     0x401398 ("ae")
     ...
     }
     }
     */
    
    if (neighbor) {
    
        pd_stack_for_each(kids, s) {
            // we presume the array is valid and has an ae id at s
            t = as(pd_stack, as(pd_stack, s->info)->prev)->info;
            // we're now at another stack,
            // [PD_REF, ID, GEN]
            // which we also expect to be valid so we grab ID
            if (0 == strcmp(t->prev->info, buf)) {
                // found it; we now have to duplicate it and fix the id -- note that we fix the original, and leave the duplicate, to get the order right!
                pd_stack dup = pd_stack_copy(s->info);
                sprintf(buf, "%ld", PDObjectGetObID(importedObject));
                free(t->prev->info);
                t->prev->info = strdup(buf);
                //                free(as(pd_stack, dup->prev->info)->prev->info);
                //                as(pd_stack, dup->prev->info)->prev->info = strdup(buf);
                pd_stack_push_stack(&s->prev, dup);
                break;
            }
        }

    } else {
        
        // find last entry in stack
        for (s = kids; s && s->prev; s = s->prev) ;
        pd_stack dup = pd_stack_copy(s->info);
        sprintf(buf, "%ld", PDObjectGetObID(importedObject));
        free(as(pd_stack, dup->prev->info)->prev->info);
        as(pd_stack, dup->prev->info)->prev->info = strdup(buf);
        pd_stack_push_stack(&s->prev, dup);
        
    }
    
    
    
    
    
    
    
//    if (neighbor == NULL) {
//        // this means we're appending; to avoid the hassle of appending into a pd_stack array definition, we simply set up a quasi-object array and use that
//        PDObjectRef arrayOb = PDObjectCreateFromDefinitionsStack(-1, kids);
//        arrayOb->obclass = PDObjectClassCompressed;
//        PDObjectAddArrayElement(arrayOb, PDObjectGetReferenceString(importedObject));
//        PDObjectGenerateDefinition(arrayOb, &buf, 64);
//        PDObjectSetDictionaryEntry(object, "Kids", buf);
//    } else {
//        sprintf(buf, "%ld", PDObjectGetObID(neighbor));
//    
//        kids = kids->prev->prev->info;
//        
//    /*
//stack<0x14cdde90> {
//   0x401394 ("array")
//   0x401388 ("entries")
//    stack<0x14cdeb90> {
//        stack<0x14cdead0> {
//           0x401398 ("ae")
//           ...
//        }
//    }
//}
//     */
//    
//        kids = kids->prev->prev->info;
//    /*
//    stack<0x14cdeb90> {
//        stack<0x14cdead0> {
//           0x401398 ("ae")
//           ...
//        }
//    }
//     */
//    
//        pd_stack_for_each(kids, s) {
//            // we presume the array is valid and has an ae id at s
//            t = as(pd_stack, as(pd_stack, s->info)->prev)->info;
//            // we're now at another stack,
//            // [PD_REF, ID, GEN]
//            // which we also expect to be valid so we grab ID
//            if (0 == strcmp(t->prev->info, buf)) {
//                // found it; we now have to duplicate it and fix the id -- note that we fix the original, and leave the duplicate, to get the order right!
//                pd_stack dup = pd_stack_copy(s->info);
//                sprintf(buf, "%ld", PDObjectGetObID(importedObject));
//                free(t->prev->info);
//                t->prev->info = strdup(buf);
////                free(as(pd_stack, dup->prev->info)->prev->info);
////                as(pd_stack, dup->prev->info)->prev->info = strdup(buf);
//                pd_stack_push_stack(&s->prev, dup); // ??? does this work???
//                break;
//            }
//        }
//    }
    
    pd_stack countEntry = pd_stack_get_dict_key(object->def, "Count", false);
    PDInteger count = PDIntegerFromString(countEntry->prev->prev->info);
    count++;
    sprintf(buf, "%ld", count);
    countEntry->prev->prev->info = strdup(buf);
    
//    PDObjectSetDictionaryEntry(object, "Count", buf);
    
    free(buf);
    PDRelease(importedObject);
    if (neighbor) PDRelease(neighbor);
    
    // we can (must, in fact) unload this task as it only applies to a specific page
    return PDTaskUnload;
}

PDPageRef PDPageInsertIntoPipe(PDPageRef page, PDPipeRef pipe, PDInteger pageIndex)
{
    PDParserAttachmentRef attachment = PDPipeConnectForeignParser(pipe, page->parser);
    
    PDParserRef parser = PDPipeGetParser(pipe);
    PDAssert(parser != page->parser); // attempt to insert page into the same parser object; this is currently NOT supported even though it's radically simpler
    
    // we start by importing the object and its indirect companions over into the target parser
    PDObjectRef importedObject = PDParserAttachmentImportObject(attachment, page->ob, (const char *[]) {"Parent"}, 1);
    //PDParserImportObject(parser, page->parser, page->ob, (const char *[]) {"Parent"}, 1);
    
    // we now try to hook it up with a neighboring page's parent; what better neighbor than the page index itself, unless it exceeds the page count?
    PDCatalogRef cat = PDParserGetCatalog(parser);
    PDInteger pageCount = PDCatalogGetPageCount(cat);
    PDInteger neighborPI = pageIndex - (pageIndex > pageCount);
    PDAssert(neighborPI > 0 && neighborPI <= pageCount); // crash = attempt to insert a page outside of the bounds of the destination PDF (i.e. the page index is higher than page count + 1, which would result in a hole with no pages)
    
    PDInteger neighborObID = PDCatalogGetObjectIDForPage(cat, neighborPI);
    PDObjectRef neighbor = PDParserLocateAndCreateObject(parser, neighborObID, true);
    const char *parentRef = PDObjectGetDictionaryEntry(neighbor, "Parent");
    PDAssert(parentRef); // crash = ??? no such page? no parent? can pages NOT have parents?
    PDObjectSetDictionaryEntry(importedObject, "Parent", parentRef);
    PDInteger parentId = atol(parentRef);
    PDAssert(parentId); // crash = parentRef was not a <num> <num> R?
//    PDObjectRef parent = PDParserLocateAndCreateObject(parser, parentId, true);
    
    PDObjectRef *userInfo = malloc(sizeof(PDObjectRef) * 2);
    userInfo[0] = (neighborPI == pageIndex ? neighbor : NULL);
    userInfo[1] = PDRetain(importedObject);
    PDTaskRef task = PDTaskCreateMutatorForObject(parentId, PDPageInsertionTask);
    PDTaskSetInfo(task, userInfo);
    PDPipeAddTask(pipe, task);
    PDRelease(task);
    
    // update the catalog
    PDCatalogInsertPage(cat, pageIndex, PDObjectGetObID(importedObject));
    
    // finally, set up the new page and return it
    PDPageRef importedPage = PDPageCreateWithObject(parser, importedObject);
    return PDAutorelease(importedPage);
}
