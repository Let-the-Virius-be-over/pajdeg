//
//  PDParser.c
//
//  Copyright (c) 2013 Karl-Johan Alm (http://github.com/kallewoof)
// 
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
// 
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
// 
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//

#include "Pajdeg.h"
#include "PDParser.h"

#include "PDInternal.h"
#include "PDState.h"
#include "PDPortableDocumentFormatState.h"
#include "PDStack.h"
#include "PDTwinStream.h"
#include "PDReference.h"
#include "PDBTree.h"
#include "PDStreamFilter.h"
#include "PDXTable.h"

#include "PDScanner.h"

PDParserRef PDParserCreateWithStream(PDTwinStreamRef stream)
{
    PDPortableDocumentFormatStateRetain();
    
    PDParserRef parser = calloc(1, sizeof(struct PDParser));
    parser->stream = stream;
    parser->state = PDParserStateBase;
    parser->success = true;
    
    if (! PDXTableFetchXRefs(parser)) {
        PDWarn("PDF is invalid or in an unsupported format.");
        //PDAssert(0); // the PDF is invalid or in a format that isn't supported
        free(parser);
        PDPortableDocumentFormatStateRelease();
        return NULL;
    }
    
    PDTwinStreamAsserts(parser->stream);

    parser->scanner = PDTwinStreamSetupScannerWithState(stream, pdfRoot);

    PDTwinStreamAsserts(parser->stream);

    // we always grab the first object, for several reasons: (1) we need to iterate past the starting cruft (%PDF etc), and (2) if this is a linearized PDF, we need to indicate it NO LONGER IS a linearized PDF
    if (PDParserIterate(parser)) {
        // because we end up discarding old definitions, we want to first pass through eventual prefixes; in theory, nothing prevents a PDF from being riddled with comments in between every object, but in practice this tends to only be once, at the top
        PDTwinStreamPrune(parser->stream, parser->oboffset);
        
        // oboffset indicates the position in the PDF where the object begins; we need to pass that through
        PDObjectRef first = PDParserConstructObject(parser);
        if (first->type == PDObjectTypeDictionary) {
            PDStackRef linearizedKey = PDStackGetDictKey(first->def, "Linearized", true);
            PDStackDestroy(linearizedKey);
        }
    }
    
    PDTwinStreamAsserts(parser->stream);
    
    return parser;
}

void PDParserDestroy(PDParserRef parser)
{
    if (parser->construct) PDObjectRelease(parser->construct);
    if (parser->root) PDObjectRelease(parser->root);
    if (parser->encrypt) PDObjectRelease(parser->encrypt);
    if (parser->rootRef) PDReferenceDestroy(parser->rootRef);
    if (parser->infoRef) PDReferenceDestroy(parser->infoRef);
    if (parser->encryptRef) PDReferenceDestroy(parser->encryptRef);
    if (parser->trailer) PDObjectRelease(parser->trailer);
    if (parser->skipT) PDBTreeDestroy(parser->skipT);
    while (parser->appends) {
        PDObjectRef ob = (PDObjectRef)PDStackPopIdentifier(&parser->appends);
        PDObjectRelease(ob);
    }
    while (parser->cxt) {
        PDXTableDestroy(parser->cxt);
        parser->cxt = (PDXTableRef) PDStackPopIdentifier(&parser->xstack);
    }
    //free(parser->xrefs);
    free(parser);

    PDPortableDocumentFormatStateRelease();
}

PDStackRef PDParserLocateAndCreateDefinitionForObject(PDParserRef parser, PDInteger obid, PDInteger bufsize, PDBool master)
{
    char *tb;
    char *string;
    PDTwinStreamRef stream = parser->stream;
    PDStackRef stack;
    PDXTableRef xrefTable = (master ? parser->mxt : parser->cxt);
    PDTwinStreamFetchBranch(stream, PDXTableOffsetForID(xrefTable, obid), bufsize, &tb);
    PDScannerRef tmpscan = PDScannerCreateWithState(pdfRoot);
    PDScannerContextPush(stream, &PDTwinStreamDisallowGrowth);
    tmpscan->buf = tb;
    tmpscan->boffset = 0;
    tmpscan->bsize = bufsize;
    
    if (PDScannerPopStack(tmpscan, &stack)) {
        if (! stream->outgrown) {
            PDStackAssertExpectedKey(&stack, "obj");
            PDStackAssertExpectedInt(&stack, obid);
        }
        PDStackDestroy(stack);
    }
    
    stack = NULL;
    if (! PDScannerPopStack(tmpscan, &stack)) {
        if (PDScannerPopString(tmpscan, &string)) {
            PDStackPushKey(&stack, string);
        }
    }
    
    PDTwinStreamCutBranch(parser->stream, tb);
    PDScannerDestroy(tmpscan);
    PDScannerContextPop();
    
    return stack;
}

void PDParserFetchStreamLengthFromObjectDictionary(PDParserRef parser, PDStackRef entry)
{
    entry = entry->prev->prev;
    if (entry->type == PDSTACK_STACK) {
        PDInteger refid;
        PDStackRef stack;
        // this is a reference length
        // e, Length, { ref, 1, 2 }
        entry = entry->info;
        PDAssert(PDIdentifies(entry->info, PD_REF));
        refid = PDStackPeekInt(entry->prev);
        PDAssert(refid < parser->cxt->cap);
        
        stack = PDParserLocateAndCreateDefinitionForObject(parser, refid, 300, false);
        parser->streamLen = PDStackPopInt(&stack);
    } else {
        char *string;
        // e, Length, 116
        string = entry->info;
        parser->streamLen = atol(string);
        PDAssert(parser->streamLen > 0 || !strcmp("0", string));
    }
}

void PDParserUpdateObject(PDParserRef parser)
{
    if (parser->obid == 1927) {
        printf("");
    }
    char *string;
    PDInteger len;

    // old (input)              new (output)
    // <<<<<<<<<<<<<<<<<<<<     >>>>>>>>>>>>>>>>>>>>
    // 1 2 obj                  1 2 obj
    // << old definition >>     << new definition >>
    // stream                   stream
    // [*]old stream content    new stream content
    // endstream                endstream
    // endobj                   endobj
    // ([*] is scanner position)
    
    PDScannerRef scanner = parser->scanner;
    PDObjectRef ob = parser->construct;
    ob->skipStream |= ob->skipObject; // simplify by always killing entire object including stream, if object is skipped
    
    // we discard old definition first; if object has a stream but wants it nixed, we iterate beyond that before discarding; we may have passed beyond the appendix already, in which case we do nothing (we're already done)
    if (parser->state == PDParserStateObjectAppendix && ob->hasStream) {
        if (ob->skipStream) {
            PDAssert(parser->streamLen > 0);

            PDScannerSkip(scanner, parser->streamLen);
            PDTwinStreamDiscardContent(parser->stream);//, PDTwinStreamScannerCommitBytes(parser->stream));
            
            
            PDScannerAssertComplex(scanner, PD_ENDSTREAM);
            //PDScannerAssertString(scanner, "endstream");
        }

        // normalize trail
        ////scanner->btrail = scanner->boffset;
    }

    // discard up to this point (either up to 'endobj' or up to 'stream' dependingly)
    PDTwinStreamDiscardContent(parser->stream);//, PDTwinStreamScannerCommitBytes(parser->stream));
    
    // old (input)              new (output)
    //                          >>>>>>>>>>>>>>>>>>>>
    // 1 2 obj                  1 2 obj
    // << old definition >>     << new definition >>
    // stream                   stream
    // <<<<<<<<<<<<<<<<<<<<     
    // [*]old stream content    new stream content
    // endstream                endstream
    // [*]endobj                endobj
    // (two potential scanner locs; the latter one for 'skip stream' or 'no stream' case)

    // push object def, unless it should be skipped
    if (! ob->skipObject) {
        if (ob->ovrDef) {
            PDTwinStreamInsertContent(parser->stream, ob->ovrDefLen, ob->ovrDef);
        } else {
            string = NULL;
            len = PDObjectGenerateDefinition(ob, &string, 0);
            PDTwinStreamInsertContent(parser->stream, len, string);
            free(string);
        }

        // old (input)              new (output)
        // 1 2 obj                  1 2 obj
        // << old definition >>     << new definition >>
        //                          >>>>>>>>>>>>>>>>>>>>
        // stream                   stream
        // <<<<<<<<<<<<<<<<<<<<     
        // [*]old stream content    new stream content
        // endstream                endstream
        // [*]endobj                endobj

        // if we have a stream that shouldn't be skipped, we want to prep for that
        /*if (ob->hasStream && ! ob->skipStream) {
            PDTwinStreamInsertContent(parser->stream, 7, "stream\n");

            PDScannerPopString(scanner, &string);
            
            // now we only expect 'e'(ndobj)
            if (string[0] == 'e') {
                PDAssert(!strcmp(string, "endobj"));
                free(string);
            } else {
                fprintf(stderr, "unknown type: %s\n", string);
                assert(0);
            }
            
            // normalize trail
            scanner->btrail = scanner->boffset;
        }*/
        
        // if we have a stream, get it onto the heap or whip past it
        if (ob->hasStream) {
            PDScannerSkip(scanner, parser->streamLen);
            if (ob->skipStream || ob->ovrStream) {
                PDTwinStreamDiscardContent(parser->stream);
            } else {
                // we've just discarded "stream", so we have to put that in first of all
                PDTwinStreamInsertContent(parser->stream, 7, "stream\n");
                PDTWinStreamPassthroughContent(parser->stream);
            }
            PDScannerAssertComplex(scanner, PD_ENDSTREAM);
            //PDScannerAssertString(scanner, "endstream");
          
            // no matter what, we want to get past endobj keyword for this object
            PDScannerAssertString(scanner, "endobj");
        }
        
        ////scanner->btrail = scanner->boffset;

        // 1 2 obj                  1 2 obj
        // << old definition >>     << new definition >>
        //                          >>>>>>>>>>>>>>>>>>>>
        // stream                   stream
        // [*]old stream content    new stream content
        // endstream                endstream
        // [*]endobj                endobj
        // <<<<<<<<<<<<<<<<<<<<     
        
        // we may want a stream but did not have one -- hasStream defines original conditions, not our desire, hence 'hasStream' rather than 'wantsStream'
        if ((ob->hasStream && ! ob->skipStream) || ob->ovrStream) {
            if (ob->ovrStream) {
                // discard old and write new
                PDTwinStreamDiscardContent(parser->stream);//, PDTwinStreamScannerCommitBytes(parser->stream));
                //                                             012345 6
                PDTwinStreamInsertContent(parser->stream, 7, "stream\n");
                PDTwinStreamInsertContent(parser->stream, ob->ovrStreamLen, ob->ovrStream);
                //                                              0123456789 0123456 7
                PDTwinStreamInsertContent(parser->stream, 18, "\nendstream\nendobj\n");
            } else {
                // pass through endstream and endobj
                PDTWinStreamPassthroughContent(parser->stream);//, PDTwinStreamScannerCommitBytes(parser->stream));
            }

            // 1 2 obj                  1 2 obj
            // << old definition >>     << new definition >>
            // stream                   stream
            // [*]old stream content    new stream content
            // endstream                endstream
            // endobj                   endobj
            // <<<<<<<<<<<<<<<<<<<<     >>>>>>>>>>>>>>>>>>>>     
        } else {
            PDAssert(ob->hasStream == (parser->streamLen > 0));
            // discard and print out endobj; we do not pass through here, because we may be dealing with a brand new object that doesn't have anything for us to pass
            PDTwinStreamDiscardContent(parser->stream);//, PDTwinStreamScannerCommitBytes(parser->stream));
            PDTwinStreamInsertContent(parser->stream, 7, "endobj\n");
        }
    }
    
    PDObjectRelease(ob);
    parser->construct = NULL;
    
    parser->state = PDParserStateBase;
    parser->streamLen = 0;
}

void PDParserPassoverObject(PDParserRef parser);

void PDParserPassthroughObject(PDParserRef parser)
{
    char *string;
    PDStackRef stack, entry;
    PDScannerRef scanner;
    
    // update xref entry; we do this even if this ends up being an xref; if it's an old xref, it will be removed anyway, and if it's the master, it will have its offset set at the end anyway
    PDXTableSetOffset(parser->mxt, parser->obid, parser->oboffset);
    //PDXWrite((char*)&parser->mxt->fields[parser->obid], parser->oboffset, 10);
    
    // if we have a construct, we need to serialize that into the output stream
    if (parser->construct) {
        PDParserUpdateObject(parser);
        PDTwinStreamAsserts(parser->stream);
#ifdef PD_DEBUG_TWINSTREAM_ASSERT_OBJECTS
        char expect[100];
        PDInteger len = sprintf(expect, "%zd %zd obj", parser->obid, parser->genid);
        PDTwinStreamReassert(parser->stream, parser->oboffset, expect, len);
#endif
        parser->oboffset = PDTwinStreamGetOutputOffset(parser->stream);
        return;
    }
    
    scanner = parser->scanner;

    switch (parser->state) {
        case PDParserStateObjectDefinition:
            if (PDScannerPopStack(scanner, &stack)) {
                if (parser->encryptRef && parser->obid == parser->encryptRef->obid) {
                    // this is an encryption dictionary; those have a Length field that is not the length of the object stream
                } else {
                    entry = PDStackGetDictKey(stack, "Length", false);
                    if (entry) {
                        PDParserFetchStreamLengthFromObjectDictionary(parser, entry);
                    }
                }

                // this may be an xref object stream; we can those; if it is the master object, it will be appended to the end of the stream regardless
                entry = PDStackGetDictKey(stack, "Type", false);
                if (entry) {
                    entry = entry->prev->prev->info; // entry value is a stack (a name stack)
                    PDAssert(PDIdentifies(entry->info, PD_NAME));
                    if (!strcmp("XRef", entry->prev->info)) {
                        PDStackDestroy(stack);
                        *PDXRefTypeForID(parser->mxt->xrefs, parser->obid) = PDXTypeFreed;
                        parser->state = PDParserStateObjectAppendix;
                        PDParserPassoverObject(parser);
                        // we also have a startxref (apparently not always)
#if 0
                        PDScannerAssertComplex(scanner, PD_STARTXREF);
                        PDTwinStreamDiscardContent(parser->stream);
                        // we most likely also have a %%EOF
                        if (PDScannerPopStack(scanner, &stack)) {
                            if (PDIdentifies(stack->info, PD_META)) {
                                // yeh
                                PDTwinStreamDiscardContent(parser->stream);
                                PDStackDestroy(stack);
                            } else {
                                // whoops, no
                                PDStackPushStack(&scanner->resultStack, stack);
                            } 
                        }
#endif
                        return;
                    }
                }
                PDStackDestroy(stack);
            } else {
                PDScannerPopString(scanner, &string);
                free(string);
            }
            // <-- pass thru
        case PDParserStateObjectAppendix:
            PDTwinStreamAsserts(parser->stream);
            PDScannerPopString(scanner, &string);
            // we expect 'e'(ndobj) or 's'(tream)
            if (string[0] == 's')  {
                PDAssert(!strcmp(string, "stream"));
                free(string);
                PDAssert(parser->streamLen > 0);
                PDScannerSkip(scanner, parser->streamLen);
                PDTWinStreamPassthroughContent(parser->stream);//, PDTwinStreamScannerCommitBytes(parser->stream));
                PDScannerAssertComplex(scanner, PD_ENDSTREAM);
                //PDScannerAssertString(scanner, "endstream");
                PDScannerPopString(scanner, &string);
            }
            
            // now we only expect 'e'(ndobj)
            if (string[0] == 'e') {
                PDAssert(!strcmp(string, "endobj"));
                free(string);
            } else {
                PDWarn("unknown type: %s\n", string);
                PDAssert(0);
            }
        default:
            break;
    }
    
    // to make things easy, we nudge the trail so the entire object is passed through
    ////scanner->btrail = scanner->boffset;
    
    // pass through the object; scanner is the master scanner, and will be adjusted by the stream
    PDTWinStreamPassthroughContent(parser->stream);//, PDTwinStreamScannerCommitBytes(parser->stream));
    
#ifdef PD_DEBUG_TWINSTREAM_ASSERT_OBJECTS
    char expect[100];
    PDInteger len = sprintf(expect, "%zd %zd obj", parser->obid, parser->genid);
    PDTwinStreamReassert(parser->stream, parser->oboffset, expect, len);
#endif
    
    parser->state = PDParserStateBase;
    
    parser->oboffset = PDTwinStreamGetOutputOffset(parser->stream);
    PDTwinStreamAsserts(parser->stream);
}

void PDParserPassoverObject(PDParserRef parser)
{
    char *string;
    PDStackRef stack, entry;
    PDScannerRef scanner;
    
    scanner = parser->scanner;
    
    switch (parser->state) {
        case PDParserStateObjectDefinition:
            if (PDScannerPopStack(scanner, &stack)) {
                if (parser->encryptRef && parser->obid == parser->encryptRef->obid) {
                    // this is an encryption dictionary; those have a Length field that is not the length of the object stream
                } else {
                    entry = PDStackGetDictKey(stack, "Length", false);
                    if (entry) {
                        PDParserFetchStreamLengthFromObjectDictionary(parser, entry);
                    }
                }
                PDStackDestroy(stack);
            } else {
                PDScannerPopString(scanner, &string);
                free(string);
            }
            // <-- pass thru
        case PDParserStateObjectAppendix:
            PDTwinStreamAsserts(parser->stream);
            PDScannerPopString(scanner, &string);
            // we expect 'e'(ndobj) or 's'(tream)
            if (string[0] == 's')  {
                PDAssert(!strcmp(string, "stream"));
                free(string);
                PDAssert(parser->streamLen > 0);
                PDScannerSkip(scanner, parser->streamLen);
                PDTwinStreamDiscardContent(parser->stream);
                PDScannerAssertComplex(scanner, PD_ENDSTREAM);
                //PDScannerAssertString(scanner, "endstream");
                PDScannerPopString(scanner, &string);
            }
            
            // now we only expect 'e'(ndobj)
            if (string[0] == 'e') {
                PDAssert(!strcmp(string, "endobj"));
                free(string);
            } else {
                PDWarn("unknown type: %s\n", string);
                PDAssert(0);
            }
        default:
            break;
    }
    
    // discard content
    PDTwinStreamDiscardContent(parser->stream);
    
    parser->state = PDParserStateBase;
    
    PDTwinStreamAsserts(parser->stream);
}

// append objects
void PDParserAppendObjects(PDParserRef parser)
{
    PDObjectRef obj;
    
    if (parser->state != PDParserStateBase)
        PDParserPassthroughObject(parser);
    
    while (parser->appends) {
        obj = parser->construct = (PDObjectRef)PDStackPopIdentifier(&parser->appends);
        parser->obid = obj->obid;
        parser->genid = obj->genid;
        parser->streamLen = obj->streamLen;
        PDParserPassthroughObject(parser);
    }
}

// advance to the next XRef domain
PDBool PDParserIterateXRefDomain(PDParserRef parser)
{
    // linearized XREF = ignore extraneous XREF records
    if (parser->cxt->linearized && parser->cxt->pos > PDTwinStreamGetInputOffset(parser->stream)) 
        return true;
    
    if (parser->xstack == NULL) {
        parser->done = true;
        
        PDParserAppendObjects(parser);
        PDAssert(NULL == parser->xstack); // crash = we missed an xref table in the PDF; that's very not good
        PDAssert(NULL == parser->skipT); // crash = we lost objects
        parser->success &= NULL == parser->xstack && NULL == parser->skipT;
        return false;
    }
    
    PDXTableRef next = (PDXTableRef) PDStackPopIdentifier(&parser->xstack);
    
    PDBool retval = true;
    if (parser->cxt != parser->mxt) {
        PDXTableDestroy(parser->cxt);
        parser->cxt = next;

        if (parser->cxt->pos < PDTwinStreamGetInputOffset(parser->stream)) 
            retval = PDParserIterateXRefDomain(parser);
    } else {
        if (next->pos < PDTwinStreamGetInputOffset(parser->stream)) 
            retval = PDParserIterateXRefDomain(parser);
        PDXTableDestroy(next);
    }
    
    return retval;
}

// iterate to the next (non-deprecated) object
PDBool PDParserIterate(PDParserRef parser)
{
    PDBool running;
    PDBool skipObject;
    PDStackRef stack;
    PDID typeid;
    PDScannerRef scanner = parser->scanner;
    char *mxrefs = parser->mxt->xrefs;
    size_t nextobid, nextgenid;

    PDTwinStreamAsserts(parser->stream);
    
    // parser may be done
    if (parser->done) 
        return false;
    
    // we may have passed beyond the current binary XREF table
    if (parser->cxt->format == PDXTableFormatBinary && PDTwinStreamGetInputOffset(parser->stream) >= parser->cxt->pos) {
        if (! PDParserIterateXRefDomain(parser)) 
            // we've reached the end
            return false;
    }
    
    // move past half-read objects
    if (PDParserStateBase != parser->state || NULL != parser->construct) {
        PDParserPassthroughObject(parser);
    }

    PDTwinStreamAsserts(parser->stream);
    
    while (true) {
        // discard up to this point
        if (scanner->boffset > 0) {
            PDTwinStreamDiscardContent(parser->stream);
        }
        
        PDTwinStreamAsserts(parser->stream);
        
        if (PDScannerPopStack(scanner, &stack)) {
            // mark output position
            parser->oboffset = scanner->bresoffset + PDTwinStreamGetOutputOffset(parser->stream);
            
            PDTwinStreamAsserts(parser->stream);
            
            // first is the type, returned as an identifier
            
            typeid = PDStackPopIdentifier(&stack);

            // we expect PD_XREF, PD_STARTXREF, or PD_OBJ
            
            if (typeid == &PD_XREF) {
                // xref entry; note that we use running as the 'should consume trailer' argument to passover as the two states coincide
                running = PDParserIterateXRefDomain(parser);
                PDXTablePassoverXRefEntry(parser, stack, running);

                if (! running) return false;
                
                continue;
            }
            
            if (typeid == &PD_OBJ) {
                // object definition; this is what we're after, unless this object is deprecated
                parser->obid = nextobid = PDStackPopInt(&stack);
                PDAssert(nextobid < parser->mxt->cap);
                parser->genid = nextgenid = PDStackPopInt(&stack);
                PDStackDestroy(stack);
                
                skipObject = false;
                parser->state = PDParserStateObjectDefinition;
                
                //printf("object %zd (genid = %zd)\n", nextobid, nextgenid);
                
                if (nextgenid != *PDXRefGenForID(mxrefs, nextobid)) {
                    // this is the wrong object
                    skipObject = true;
                } else {
                    long long offset = scanner->bresoffset + PDTwinStreamGetInputOffset(parser->stream) - PDXRefGetOffsetForID(mxrefs, nextobid); // PDXOffset(mxrefs[nextobid]);
                    //printf("offset = %lld\n", offset);
                    if (offset == 0) {
                        PDBTreeRemove(&parser->skipT, nextobid);
                    } else {
                        //printf("offset mismatch for object %zd\n", nextobid);
                        PDBTreeInsert(&parser->skipT, nextobid, (void*)nextobid);
                        // this is an old version of the object (in reality, PDF:s should not have the same object defined twice; in practice, Adobe InDesign CS5.5 (7.5) happily spews out 2 (+?) copies of the same object with different versions in the same PDF (admittedly the obs are separated by %%EOF and in separate appendings, but regardless)
                        skipObject = true;
                    }
                }
                
                if (skipObject) {
                    // move past object
                    PDParserPassoverObject(parser);
                    continue;
                } 
                
                return true;
            }
            
            if (typeid == &PD_STARTXREF) {
                // a trailing startxref entry
                PDStackDestroy(stack);
                PDScannerAssertComplex(scanner, PD_META);
                // snip
                PDTwinStreamDiscardContent(parser->stream);
                continue;
            }

            PDWarn("unknown type: %s\n", *typeid);
            PDAssert(0);
        } else {
            // we failed to get a stack which is very odd
            PDWarn("failed to pop stack from PDF stream; the unexpected string is \"%s\"\n", (char*)scanner->resultStack->info);
            PDAssert(0);
            return false;
        }
    }
    
    return false;
}

PDObjectRef PDParserCreateNewObject(PDParserRef parser)
{
    size_t newiter, count, cap;
    char *xrefs;
    
    // we cannot be inside another object when we do this
    if (parser->state != PDParserStateBase || parser->construct) 
        PDParserPassthroughObject(parser);
    
    newiter = parser->xrefnewiter;
    count = parser->mxt->count;
    cap = parser->mxt->cap;
    xrefs = parser->mxt->xrefs;
    
    while (newiter < count && PDXTypeFreed != *PDXRefTypeForID(xrefs, newiter)) //PDXUsed(xrefs[newiter]))
        newiter++;
    if (newiter == cap) {
        // we must realloc xref as it can't contain all the xrefs
        parser->mxt->cap = parser->mxt->count = cap = cap + 1;
        xrefs = parser->mxt->xrefs = realloc(parser->mxt->xrefs, PDXWidth * cap);
    }
    *PDXRefTypeForID(xrefs, newiter) = PDXTypeUsed;
    *PDXRefGenForID(xrefs, newiter) = 0;

    parser->xrefnewiter = newiter;
    
    parser->obid = newiter;
    parser->genid = 0;
    parser->streamLen = 0;
    
    PDObjectRef object = parser->construct = PDObjectCreate(parser->obid, parser->genid);
    object->encryptedDoc = PDParserGetEncryptionState(parser);
    
    // we need to retain the object twice; once because this returns a retained object, and once because we retain it ourselves, and release it in "UpdateObject"
    return PDObjectRetain(object);
}

PDObjectRef PDParserCreateAppendedObject(PDParserRef parser)
{
    PDObjectRef object = PDParserCreateNewObject(parser);
    PDStackPushIdentifier(&parser->appends, (PDID)object);
    parser->construct = NULL;
    return object;
}

// construct a PDObjectRef for the current object
PDObjectRef PDParserConstructObject(PDParserRef parser)
{
    // we may have an object already; this is the case if someone constructs twice, or if someone constructs the very first object, which is always constructed
    if (parser->construct && parser->construct->obid == parser->obid)
        return parser->construct;
    
    PDAssert(parser->state == PDParserStateObjectDefinition);
    PDAssert(parser->construct == NULL);
    PDObjectRef object = parser->construct = PDObjectCreate(parser->obid, parser->genid);
    object->encryptedDoc = PDParserGetEncryptionState(parser);

    char *string;
    PDStackRef stack;
    
    PDScannerRef scanner = parser->scanner;
    
    if (PDScannerPopStack(scanner, &stack)) {
        object->def = stack;
        object->type = PDObjectTypeFromIdentifier(stack->info);

        if (parser->encryptRef && parser->obid == parser->encryptRef->obid) {
            // this is an encryption dictionary; those have a Length field that is not the length of the object stream
            parser->streamLen = 0;
        } else {
            if ((stack = PDStackGetDictKey(stack, "Length", false))) {
                PDParserFetchStreamLengthFromObjectDictionary(parser, stack);
                object->streamLen = parser->streamLen;
            } else {
                parser->streamLen = 0;
            }
        }
    } else {
        PDScannerPopString(scanner, &string);
        object->def = string;
        object->type = PDObjectTypeString;
    }
    
    PDScannerPopString(scanner, &string);
    // we expect 'e'(ndobj) or 's'(tream)
    if (string[0] == 's') {
        PDAssert(!strcmp(string, "stream"));
        free(string);
        object->hasStream = true;
    
        parser->state = PDParserStateObjectAppendix;
    } else if (string[0] == 'e') {
        PDAssert(!strcmp(string, "endobj"));
        free(string);

        // align trail as if we passed over this object (because we essentially did, even though we still may submit a modified version of it)
        ////scanner->btrail = scanner->boffset;
        
        parser->state = PDParserStateBase;
    }
    
    return object;
}

PDBool PDParserGetEncryptionState(PDParserRef parser)
{
    return NULL != parser->encryptRef;
}

void PDParserDone(PDParserRef parser)
{
    PDAssert(parser->success);

#define twinstream_printf(fmt...) \
        len = sprintf(obuf, fmt); \
        PDTwinStreamInsertContent(stream, len, obuf)
#define twinstream_put(len, buf) \
        PDTwinStreamInsertContent(stream, len, (char*)buf);
    char *obuf = malloc(512);
    PDInteger len;
    PDTwinStreamRef stream = parser->stream;
    
    // iterate past all remaining objects, if any
    while (PDParserIterate(parser));
    
    // the output offset is our new startxref entry
    PDSize startxref = PDTwinStreamGetOutputOffset(parser->stream);
    
    // write XREF table and trailer
    PDXTableInsert(parser);
    
    // write startxref entry
    twinstream_printf("startxref\n%llu\n%%%%EOF\n", startxref);
    
    free(obuf);
}

PDBool PDParserIsObjectStillMutable(PDParserRef parser, PDInteger obid)
{
    return (PDTwinStreamGetOutputOffset(parser->stream) < PDXTableOffsetForID(parser->mxt, obid)); // PDXOffset(parser->mxt->fields[obid]));
}

PDObjectRef PDParserGetRootObject(PDParserRef parser)
{
    if (! parser->root) {
        PDStackRef rootDef = PDParserLocateAndCreateDefinitionForObject(parser, parser->rootRef->obid, 500, true);
        parser->root = PDObjectCreate(parser->rootRef->obid, parser->rootRef->genid);
        parser->root->def = rootDef;
    }
    return parser->root;
}

PDInteger PDParserGetTotalObjectCount(PDParserRef parser)
{
    return parser->mxt->count;
}
