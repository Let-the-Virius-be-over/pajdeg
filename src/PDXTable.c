//
//  PDXTable.c
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/27/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

#include "PDInternal.h"
#include "PDParser.h"
#include "PDTwinStream.h"
#include "PDScanner.h"
#include "PDStack.h"
#include "PDPortableDocumentFormatState.h"

PDSize PDXTableFindStartXRef(PDParserRef parser)
{
    PDTwinStreamRef stream;
    PDStackRef stack;
    PDScannerRef xrefScanner;
    PDSize offs;
    
    stream = parser->stream;
    
    PDTWinStreamSetMethod(stream, PDTwinStreamReversed);
    
    xrefScanner = PDScannerCreateWithStateAndPopFunc(xrefSeeker, &PDScannerPopSymbolRev);
    
    PDScannerContextPush(stream, &PDTwinStreamGrowInputBufferReversed);
    
    // we expect a stack, because it should have skipped until it found startxref
    
    PDScannerSetLoopCap(100);
    if (! PDScannerPopStack(xrefScanner, &stack)) {
        PDScannerContextPop();
        return false;
    }
    
    // this stack should start out with "xref" indicating the ob type
    PDStackAssertExpectedKey(&stack, "startxref");
    // next is the offset 
    offs = PDStackPopSize(&stack);
    PDAssert(stack == NULL);
    PDScannerDestroy(xrefScanner);
    
    // we now start looping, each loop covering one xref entry, until we've jumped through all of them
    PDTWinStreamSetMethod(stream, PDTwinStreamRandomAccess);
    PDScannerContextPop();
    
    return offs;
}

PDBool PDXTableFetchXRefsForParser(PDParserRef parser)
{
    PDTwinStreamRef stream = parser->stream;
    PDSize highob;
    PDScannerRef scanner;
    char *s;
    PDBool running;
    PDXTableRef *tables;
    PDSize      *offsets;
    PDInteger offscount;
    PDInteger j;
    PDInteger i;
    PDXRef xrefs;
    
    running = true;
    PDObjectRef trailer = parser->trailer = PDObjectCreate(0, 0);
    PDReferenceRef rootRef = NULL;
    PDReferenceRef infoRef = NULL;
    PDReferenceRef encryptRef = NULL;
    
    offscount = 0;
    do {
        // add this offset to stack
        offscount++;
        PDStackPushIdentifier(&osstack, (PDID)offs);
        
        // jump to xref
        PDTwinStreamSeek(stream, offs);
        
        // set up scanner
        scanner = PDScannerCreateWithState(pdfRoot);
        
        // if this is a v1.5 PDF, we may run into an object definition here; the object is the replacement for the trailer, and has a (usually compressed) stream of the XREF table
        if (PDScannerPopStack(scanner, &stack)) {
            // we determine this by checking the identifier for the popped stack
            if (PDIdentifies(stack->info, PD_OBJ)) {
                // this is indeed a 1.5 ob stream; pull in the definition, skip past stream, then move on
                PDStackDestroy(stack);
                PDScannerPopStack(scanner, &stack);
                PDScannerAssertString(scanner, "stream");
                PDInteger len = PDIntegerFromString(PDStackGetDictKey(stack, "Length", false)->prev->prev->info);
                PDScannerSkip(scanner, len);
                PDTwinStreamAdvance(stream, scanner->boffset);
                PDScannerAssertComplex(scanner, PD_ENDSTREAM);
                PDScannerAssertString(scanner, "endobj");
                
                /*
                 stack<0x17d74410> {
                 0x46d0ac ("dict")
                 0x46d0a8 ("entries")
                 stack<0x172ab3a0> {
                 stack<0x172ac3f0> {
                 0x46d0b0 ("de")
                 DecodeParms
                 stack<0x172adcd0> {
                 0x46d0ac ("dict")
                 0x46d0a8 ("entries")
                 stack<0x172ad090> {
                 stack<0x172ad080> {
                 0x46d0b0 ("de")
                 Columns
                 6
                 }
                 stack<0x172adca0> {
                 0x46d0b0 ("de")
                 Predictor
                 12
                 }
                 }
                 }
                 }
                 stack<0x172adcf0> {
                 0x46d0b0 ("de")
                 Filter
                 stack<0x172add80> {
                 0x46d098 ("name")
                 FlateDecode
                 }
                 }
                 stack<0x172add70> {
                 0x46d0b0 ("de")
                 ID
                 stack<0x172aded0> {
                 0x46d0b4 ("array")
                 0x46d0a8 ("entries")
                 stack<0x172addd0> {
                 stack<0x172adde0> {
                 0x46d0b8 ("ae")
                 stack<0x172adeb0> {
                 0x46d0a4 ("hexstr")
                 7A1018F6F6840A3ACF5AE3FE390B8CAF
                 }
                 }
                 stack<0x172ade00> {
                 0x46d0b8 ("ae")
                 stack<0x172adf00> {
                 0x46d0a4 ("hexstr")
                 31CF30870E074234BF633D6118DFF806
                 }
                 }
                 }
                 }
                 }
                 stack<0xb648ae0> {
                 0x46d0b0 ("de")
                 Index
                 stack<0x172aef60> {
                 0x46d0b4 ("array")
                 0x46d0a8 ("entries")
                 stack<0x172adff0> {
                 stack<0x172adfe0> {
                 0x46d0b8 ("ae")
                 1636
                 }
                 stack<0x172ae000> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172ae070> {
                 0x46d0b8 ("ae")
                 1660
                 }
                 stack<0x172ae0c0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172ae160> {
                 0x46d0b8 ("ae")
                 1663
                 }
                 stack<0x172ae1c0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172ae260> {
                 0x46d0b8 ("ae")
                 1679
                 }
                 stack<0x172ae2c0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172ae360> {
                 0x46d0b8 ("ae")
                 1681
                 }
                 stack<0x172ae3c0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172ae460> {
                 0x46d0b8 ("ae")
                 1683
                 }
                 stack<0x172ae4c0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172ae560> {
                 0x46d0b8 ("ae")
                 1685
                 }
                 stack<0x172ae5c0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172ae660> {
                 0x46d0b8 ("ae")
                 1687
                 }
                 stack<0x172ae6c0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172ae760> {
                 0x46d0b8 ("ae")
                 1689
                 }
                 stack<0x172ae7c0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172ae860> {
                 0x46d0b8 ("ae")
                 1691
                 }
                 stack<0x172ae8c0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172ae960> {
                 0x46d0b8 ("ae")
                 1693
                 }
                 stack<0x172ae9c0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172aea60> {
                 0x46d0b8 ("ae")
                 1695
                 }
                 stack<0x172aeac0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172aeb60> {
                 0x46d0b8 ("ae")
                 1697
                 }
                 stack<0x172aebc0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172aec60> {
                 0x46d0b8 ("ae")
                 1699
                 }
                 stack<0x172aecc0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172aed60> {
                 0x46d0b8 ("ae")
                 1747
                 }
                 stack<0x172aedc0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172aee60> {
                 0x46d0b8 ("ae")
                 2248
                 }
                 stack<0x172aeec0> {
                 0x46d0b8 ("ae")
                 211
                 }
                 }
                 }
                 }
                 stack<0x172addb0> {
                 0x46d0b0 ("de")
                 Info
                 stack<0x172af0e0> {
                 0x46d0a0 ("ref")
                 1663
                 0
                 }
                 }
                 stack<0x172aefc0> {
                 0x46d0b0 ("de")
                 Length
                 324
                 }
                 stack<0x17d703b0> {
                 0x46d0b0 ("de")
                 Prev
                 19337293
                 }
                 stack<0x17d70190> {
                 0x46d0b0 ("de")
                 Root
                 stack<0x17d71140> {
                 0x46d0a0 ("ref")
                 1665
                 0
                 }
                 }
                 stack<0x17d710e0> {
                 0x46d0b0 ("de")
                 Size
                 2459
                 }
                 stack<0x17d71ae0> {
                 0x46d0b0 ("de")
                 Type
                 stack<0x17d722b0> {
                 0x46d098 ("name")
                 XRef
                 }
                 }
                 stack<0x17d72180> {
                 0x46d0b0 ("de")
                 W
                 stack<0x17d73dc0> {
                 0x46d0b4 ("array")
                 0x46d0a8 ("entries")
                 stack<0x17d72c60> {
                 stack<0x17d72930> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x17d72e90> {
                 0x46d0b8 ("ae")
                 4
                 }
                 stack<0x17d736d0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 }
                 }
                 }
                 }
                 }
                 */
                
            } else {
                // this is a regular old xref table with a trailer at the end
                
                // we keep popping stacks until we fail, which means we've encountered the trailer (which is a string, not a stack)
                do {
                    // this stack = xref, startobid, <startobid>, count, <count>
                    PDStackAssertExpectedKey(&stack, "xref");
                    PDStackPopInt(&stack);
                    PDInteger count = PDStackPopInt(&stack);
                    
                    // we now have a stream (technically speaking) of xrefs
                    count *= 20;
                    PDScannerSkip(scanner, count);
                    PDTwinStreamAdvance(stream, scanner->boffset);
                } while (PDScannerPopStack(scanner, &stack));
                
                // we now get the trailer
                PDScannerAssertString(scanner, "trailer");
                
                // and the trailer dictionary
                PDScannerPopStack(scanner, &stack);
            }
        }
        
        // if we have no Root or Info yet, grab them if found
        PDStackRef dictStack;
        if (rootRef == NULL && (dictStack = PDStackGetDictKey(stack, "Root", false))) {
            rootRef = PDReferenceCreateFromStackDictEntry(dictStack->prev->prev->info);
        }
        if (infoRef == NULL && (dictStack = PDStackGetDictKey(stack, "Info", false))) {
            infoRef = PDReferenceCreateFromStackDictEntry(dictStack->prev->prev->info);
        }
        if (encryptRef == NULL && (dictStack = PDStackGetDictKey(stack, "Encrypt", false))) {
            encryptRef = PDReferenceCreateFromStackDictEntry(dictStack->prev->prev->info);
        }
        
        // a Prev key may or may not exist, in which case we want to hit it
        PDStackRef prev = PDStackGetDictKey(stack, "Prev", false);
        if (prev) {
            // e, Prev, 116
            s = prev->prev->prev->info;
            offs = PDSizeFromString(s);
            PDAssert(offs > 0 || !strcmp("0", s));
        } else running = false;
        
        // update the trailer object in case additional info is included
        // TODO: determine if spec requires this or if the last trailer is the whole truth
        if (trailer->def == NULL) {
            trailer->def = stack;
        } else {
            char *key;
            char *value;
            PDStackRef iter = stack; 
            while (PDStackGetNextDictKey(&iter, &key, &value)) {
                if (NULL == PDObjectGetDictionaryEntry(trailer, key)) {
                    PDObjectSetDictionaryEntry(trailer, key, value);
                }
                free(value);
            }
            PDStackDestroy(stack);
        }
        
        PDScannerDestroy(scanner);
    } while (running);
    
    // we now have a stack in versioned order, so we start setting up xrefs
    offsets = malloc(offscount * sizeof(PDSize));
    tables = malloc(offscount * sizeof(PDXTableRef));
    offscount = 0;
    
    PDXTableRef pdx = NULL;
    while (0 != (offs = (size_t)PDStackPopIdentifier(&osstack))) {
        if (pdx) {
            /// @todo CLANG doesn't like this (pdx is stored in tables, put into ctx stack, and released on PDParserDestroy)
            pdx = memcpy(malloc(sizeof(struct PDXTable)), pdx, sizeof(struct PDXTable));
            pdx->fields = memcpy(malloc(pdx->cap * 20), pdx->fields, pdx->cap * 20);
        } else {
            pdx = malloc(sizeof(struct PDXTable));
            pdx->cap = 0;
            pdx->count = 0;
            pdx->fields = NULL;
        }
        
        // put offset in (sorted)
        for (i = 0; i < offscount && offsets[i] < offs; i++) ;
        for (j = i + 1; j <= offscount; j++) {
            offsets[j] = offsets[j-1];
            tables[j] = tables[j-1];
        }
        offsets[i] = offs;
        tables[i] = pdx;
        offscount++;
        
        pdx->pos = offs;
        xrefs = pdx->fields;
        
        // jump to xref
        PDTwinStreamSeek(stream, offs);
        
        // set up scanner
        scanner = PDScannerCreateWithState(pdfRoot);
        
        // if this is a v1.5 PDF, we may run into an object definition here; the object is the replacement for the trailer, and has a (usually compressed) stream of the XREF table
        if (PDScannerPopStack(scanner, &stack)) {
            // we determine this by checking the identifier for the popped stack
            if (PDIdentifies(stack->info, PD_OBJ)) {
                // this is indeed a 1.5 ob stream; pull in defs stack and get ready to read stream
                PDStackDestroy(stack);
                PDScannerPopStack(scanner, &stack);
                PDScannerAssertString(scanner, "stream");
                PDInteger len = PDIntegerFromString(PDStackGetDictKey(stack, "Length", false)->prev->prev->info);
                PDStackRef byteWidths = PDStackGetDictKey(stack, "W", false);
                PDStackRef index = PDStackGetDictKey(stack, "Index", false);
                PDStackRef filterDef = PDStackGetDictKey(stack, "Filter", false);
                highob = PDIntegerFromString(PDStackGetDictKey(stack, "Size", false)->prev->prev->info);
                
                PDStreamFilterRef filter = NULL;
                if (filterDef) {
                    // ("name"), "filter name"
                    filterDef = as(PDStackRef, filterDef->prev->prev->info)->prev;
                    filter = PDStreamFilterObtain(filterDef->info, true);
                }
                
                if (highob > pdx->count) {
                    pdx->count = highob;
                    if (highob > pdx->cap) {
                        pdx->cap = highob;
                        xrefs = pdx->fields = realloc(pdx->fields, 20 * highob);
                    }
                }
                
                char *buf;
                
                if (filter) {
                    PDInteger got = 0;
                    PDInteger cap = len < 1024 ? 1024 : len * 4;
                    buf = malloc(cap);
                    
                    PDScannerAttachFilter(scanner, filter);
                    PDInteger bytes = PDScannerReadStream(scanner, len, buf, cap);
                    while (bytes > 0) {
                        got += bytes;
                        if (cap - got < 512) {
                            cap *= 2;
                            buf = realloc(buf, cap);
                        }
                        bytes = PDScannerReadStreamNext(scanner, &buf[got]  , cap - got);
                    }
                    PDScannerDetachFilter(scanner);
                } else {
                    buf = malloc(len);
                    PDScannerReadStream(scanner, len, buf, len);
                }
                
                // process buf
                
                byteWidths = byteWidths->prev->prev->info;
                PDInteger sizeT = PDIntegerFromString(as(PDStackRef, byteWidths->info)->prev->info);
                PDInteger sizeO = PDIntegerFromString(as(PDStackRef, byteWidths->prev->info)->prev->info);
                PDInteger sizeI = PDIntegerFromString(as(PDStackRef, byteWidths->prev->prev->info)->prev->info);
                
                
                
                // 01 0E8A 0    % entry for object 2 (0x0E8A = 3722)
                // 02 0002 00   % entry for object 3 (in object stream 2, index 0)
                
                // Index defines what we've got 
                /*
                 stack<0xb648ae0> {
                 0x46d0b0 ("de")
                 Index
                 stack<0x172aef60> {
                 0x46d0b4 ("array")
                 0x46d0a8 ("entries")
                 stack<0x172adff0> {
                 stack<0x172adfe0> {
                 0x46d0b8 ("ae")
                 1636
                 }
                 stack<0x172ae000> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172ae070> {
                 0x46d0b8 ("ae")
                 1660
                 }
                 stack<0x172ae0c0> {
                 0x46d0b8 ("ae")
                 1
                 }
                 stack<0x172ae160> {
                 0x46d0b8 ("ae")
                 1663
                 }
                 */
                
                PDScannerAssertComplex(scanner, PD_ENDSTREAM);
                PDScannerAssertString(scanner, "endobj");
            } else {
                do {
                    // this stack = xref, startobid, <startobid>, count, <count>
                    PDStackAssertExpectedKey(&stack, "xref");
                    PDInteger startobid = PDStackPopInt(&stack);
                    PDInteger count = PDStackPopInt(&stack);
                    
                    //printf("[%d .. %d]\n", startobid, startobid + count - 1);
                    
                    highob = startobid + count;
                    
                    if (highob > pdx->count) {
                        pdx->count = highob;
                        if (highob > pdx->cap) {
                            // we must realloc xref as it can't contain all the xrefs
                            pdx->cap = highob;
                            xrefs = pdx->fields = realloc(pdx->fields, 20 * highob);
                        }
                    }
                    
                    // we now have a stream (technically speaking) of xrefs
                    PDInteger bytes = count * 20;
                    if (bytes != PDScannerReadStream(scanner, bytes, (char*)&xrefs[startobid], bytes)) {
                        PDAssert(0);
                    }
                } while (PDScannerPopStack(scanner, &stack));
            }
        }
        
        PDScannerDestroy(scanner);
        PDStackDestroy(stack);
    }
    
    // pdx is now the complete input xref table with all offsets correct, so we use it as is for the master table
    parser->mxt = pdx;
    
    // we now set up the xstack from the (byte-ordered) list of xref tables
    parser->xstack = NULL;
    for (i = offscount - 1; i >= 0; i--) 
        PDStackPushIdentifier(&parser->xstack, (PDID)tables[i]);
    free(offsets);
    free(tables);
    // xstackr is now the (reversed) xstack, so we set that up
    //parser->xstack = NULL;
    //while (xstackr) 
    //    PDStackPopInto(&parser->xstack, &xstackr);
    
    // and finally pull out the current table
    parser->cxt = (PDXTableRef) PDStackPopIdentifier(&parser->xstack);
    
    parser->xrefnewiter = 1;
    
    parser->rootRef = rootRef;
    parser->infoRef = infoRef;
    parser->encryptRef = encryptRef;
    
    // we've got all the xrefs so we can switch back to the readwritable method
    PDTWinStreamSetMethod(stream, PDTwinStreamReadWrite);
    
    //#define DEBUG_PARSER_PRINT_XREFS
#ifdef DEBUG_PARSER_PRINT_XREFS
    printf("\n"
           "       XREFS     \n"
           "  OFFSET    GEN  U\n"
           "---------- ----- -\n"
           "%s", (char*)xrefs);
#endif
    
    //#define DEBUG_PARSER_CHECK_XREFS
#ifdef DEBUG_PARSER_CHECK_XREFS
    printf("* * * * *\nCHECKING XREFS\n* * * * * *\n");
    xrefs = parser->mxt->fields;
    char *buf;
    char obdef[50];
    PDInteger bufl;
    PDInteger  obdefl;
    for (i = 0; i < parser->mxt->count; i++) {
        long offs = PDXOffset(xrefs[i]);
        printf("object #%3d: %10ld (%s)\n", i, offs, PDXUsed(xrefs[i]) ? "in use" : "free");
        if (PDXUsed(xrefs[i])) {
            bufl = PDTwinStreamFetchBranch(stream, offs, 200, &buf);
            obdefl = sprintf(obdef, "%d %ld obj", i, PDXGenId(xrefs[i]));
            if (bufl < obdefl || strncmp(obdef, buf, obdefl)) {
                printf("ERROR: object definition did not start at %ld: instead, this was encountered: ", offs);
                for (j = 0; j < 20 && j < bufl; j++) 
                    putchar(buf[j] < '0' || buf[j] > 'z' ? '.' : buf[j]);
                printf("\n");
            }
        }
    }
#endif
    
    return true;
}

