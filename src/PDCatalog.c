//
// PDCatalog.c
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

#include "Pajdeg.h"
#include "PDCatalog.h"

#include "PDParser.h"

#include "pd_internal.h"
#include "pd_stack.h"
#include "pd_dict.h"

void PDPageReferenceDestroy(PDPageReference * page)
{
    if (page->collection) {
        for (long i = page->count - 1; i >= 0; i--)
            PDPageReferenceDestroy(&page->kids[i]);
        free(page->kids);
    }
}

void PDCatalogDestroy(PDCatalogRef catalog)
{
    PDPageReferenceDestroy(&catalog->pages);
    PDRelease(catalog->object);
    free(catalog->kids);
}

void PDCatalogAppendPages(PDCatalogRef catalog, PDPageReference *pages, pd_stack defs)
{
    pages->collection = true;
    PDParserRef parser = catalog->parser;
    
    PDInteger count = PDIntegerFromString(pd_stack_get_dict_key(defs, "Count", false)->prev->prev->info);
    PDAssert(count > 0);
    if (count + catalog->count >= catalog->capacity) {
        catalog->capacity = catalog->count + count;
        catalog->kids = realloc(catalog->kids, sizeof(PDInteger) * catalog->capacity);
    }
    
    pd_stack kidsStack = pd_stack_get_dict_key(defs, "Kids", true);
    pd_stack_destroy(&defs);
    pd_stack kidsArr = pd_stack_get_arr(kidsStack);
    
    PDInteger lcount = pages->count = pd_stack_get_count(kidsArr); //PDObjectGetArrayCount(kidsArray);
    
    PDInteger *ckids = catalog->kids;
    PDPageReference *kids = pages->kids = malloc(sizeof(PDPageReference) * lcount);
    
    /*
     stack<0x113c5c10> {
         0x3f99a0 ("ae")
         stack<0x113532b0> {
             0x3f9988 ("ref")
             557
             0
         }
     }
     stack<0x113d49b0> {
         0x3f99a0 ("ae")
         stack<0x113efa50> {
             0x3f9988 ("ref")
             558
             0
         }
     }
     */
    pd_stack stack, iter;
    PDInteger i = 0;
    PDInteger oid;
    pd_stack_for_each(kidsArr, iter) {
        stack = as(pd_stack, as(pd_stack, iter->info)->prev->info)->prev;
        oid = PDIntegerFromString(stack->info);
        defs = PDParserLocateAndCreateDefinitionForObject(parser, oid, true);
        PDAssert(defs); // crash = above function is failing; it may start failing if an object is "weird", or if the code to fetch objects is broken (e.g. PDScanner, PDTwinStream, or even PDParser)
        char *type = as(pd_stack, pd_stack_get_dict_key(defs, "Type", false)->prev->prev->info)->prev->info;
        if (0 == strcmp(type, "Pages")) {
            PDCatalogAppendPages(catalog, &kids[i], defs);
        } else {
            ckids[catalog->count++] = oid;
            kids[i].collection = false;
            kids[i].obid = oid;
            kids[i].genid = PDIntegerFromString(stack->prev->info);
            pd_stack_destroy(&defs);
        }
        i++;
    }
    pd_stack_destroy(&kidsStack);
    /*for (PDInteger i = 0; i < count; i++) {
     //sref = PDObjectGetArrayElementAtIndex(kidsArray, i); // 2 0 R
     stack = pd_stack_get_arr_element(kidsStack, i);
     kids[i] = PDIntegerFromString(sref);
     }
     
     PDRelease(kidsArray);*/
}

PDCatalogRef PDCatalogCreateWithParserForObject(PDParserRef parser, PDObjectRef catalogObject)
{
    const char *sref;
    
    PDCatalogRef catalog = PDAlloc(sizeof(struct PDCatalog), PDCatalogDestroy, false);
    catalog->parser = parser;
    catalog->object = PDRetain(catalogObject);
    catalog->count = 0;
    catalog->capacity = 0;
    catalog->kids = NULL;
    
    // catalogObject has the form
    //  << /Type /Catalog /Pages 3 0 R >>
    // so the first thing we do is fetch that /Pages object
    sref = PDObjectGetDictionaryEntry(catalogObject, "Pages");
    PDAssert(sref != NULL); // catalogObject is most likely not a valid catalog

    // the Pages object looks like:
    //  << /Type /Pages /MediaBox [0 0 1024 768] /Count 1 /Kids [ 2 0 R ] >>
    // note that MediaBox is currently ignored
    
    pd_stack defs = PDParserLocateAndCreateDefinitionForObject(parser, PDIntegerFromString(sref), true);
    if (! defs) {
        PDRelease(catalog);
        return NULL;
    }
    PDCatalogAppendPages(catalog, &catalog->pages, defs);
    
    return catalog;
}

PDInteger PDCatalogGetObjectIDForPage(PDCatalogRef catalog, PDInteger pageNumber)
{
    PDAssert(pageNumber > 0 && pageNumber <= catalog->count);
    return catalog->kids[pageNumber-1];
}

PDInteger PDCatalogGetPageCount(PDCatalogRef catalog)
{
    return catalog->count;
}

void PDCatalogInsertPage(PDCatalogRef catalog, PDInteger pageNumber, PDObjectRef pageObject)
{
    if (catalog->count == catalog->capacity) {
        catalog->capacity += 5;
        catalog->kids = realloc(catalog->kids, sizeof(PDInteger) * catalog->capacity);
    }
    PDInteger pageObjectID = PDObjectGetObID(pageObject);
    PDInteger t;
    for (PDInteger i = pageNumber - 1; i <= catalog->count; i++) {
        t = catalog->kids[i];
        catalog->kids[i] = pageObjectID;
        pageObjectID = t;
    }
    catalog->count++;
}
