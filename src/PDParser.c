//
// PDParser.c
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
#include "PDParser.h"

#include "PDOperator.h"
#include "PDObjectStream.h"
#include "pd_internal.h"
#include "PDState.h"
#include "pd_pdf_implementation.h"
#include "pd_stack.h"
#include "PDTwinStream.h"
#include "PDReference.h"
#include "PDBTree.h"
#include "PDStreamFilter.h"
#include "PDXTable.h"
#include "PDCatalog.h"
#include "pd_crypto.h"
#include "PDScanner.h"

void PDParserDestroy(PDParserRef parser)
{
    /*printf("xrefs:\n");
    printf("- mxt = %p: %ld\n", parser->mxt, ((PDTypeRef)parser->mxt - 1)->retainCount);
    printf("- cxt = %p: %ld\n", parser->cxt, ((PDTypeRef)parser->cxt - 1)->retainCount);
    for (pd_stack t = parser->xstack; t; t = t->prev)
        printf("- [-]: %ld\n", ((PDTypeRef)t->info - 1)->retainCount);*/
    
    PDRelease(parser->catalog);
    PDRelease(parser->construct);
    PDRelease(parser->root);
    PDRelease(parser->info);
    PDRelease(parser->encrypt);
    PDRelease(parser->rootRef);
    PDRelease(parser->infoRef);
    PDRelease(parser->encryptRef);
    PDRelease(parser->trailer);
    PDRelease(parser->skipT);
    pd_stack_destroy(parser->appends);
    
    PDRelease(parser->mxt);
    PDRelease(parser->cxt);
    pd_stack_destroy(parser->xstack);
    
    if (parser->crypto) pd_crypto_destroy(parser->crypto);
    
    pd_pdf_implementation_discard();
}

PDParserRef PDParserCreateWithStream(PDTwinStreamRef stream)
{
    pd_pdf_implementation_use();
    
    PDParserRef parser = PDAlloc(sizeof(struct PDParser), PDParserDestroy, true);
    parser->stream = stream;
    parser->state = PDParserStateBase;
    parser->success = true;
    
    if (! PDXTableFetchXRefs(parser)) {
        PDWarn("PDF is invalid or in an unsupported format.");
        //PDAssert(0); // the PDF is invalid or in a format that isn't supported
        free(parser);
        pd_pdf_implementation_discard();
        return NULL;
    }

    parser->skipT = PDBTreeCreate(PDDeallocatorNull, 1, parser->mxt->count, 3);
    
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
            pd_stack linearizedKey = pd_stack_get_dict_key(first->def, "Linearized", true);
            pd_stack_destroy(linearizedKey);
        }
    }
    
    PDTwinStreamAsserts(parser->stream);
    
    if (parser->encryptRef) {
        // set up crypto instance 
        if (! parser->encrypt) {
            pd_stack encDef = PDParserLocateAndCreateDefinitionForObject(parser, parser->encryptRef->obid, true);
            parser->encrypt = PDObjectCreate(parser->encryptRef->obid, parser->encryptRef->genid);
            parser->encrypt->def = encDef;
        }
        parser->crypto = pd_crypto_create(PDObjectGetDictionary(parser->trailer), PDObjectGetDictionary(parser->encrypt));
    }
    
    return parser;
}

pd_stack PDParserLocateAndCreateDefinitionForObjectWithSize(PDParserRef parser, PDInteger obid, PDInteger bufsize, PDBool master, PDXOffsetType *outOffset)
{
    char *tb;
    char *string;
    PDTwinStreamRef stream;
    pd_stack stack;
    PDXTableRef xrefTable;
    
    stream = parser->stream;
    xrefTable = (master ? parser->mxt : parser->cxt);

    // if the object is in an object stream, we need to fetch its container, otherwise we can fetch the object itself
    if (PDXTypeComp == PDXTableGetTypeForID(xrefTable, obid)) {
        // grab container definition
        PDInteger len;
        PDObjectStreamRef obstm;
        PDXOffsetType containerOffset;
        PDInteger index = PDXGetGenForID(xrefTable->xrefs, obid);
        
        pd_stack containerDef = PDParserLocateAndCreateDefinitionForObjectWithSize(parser, PDXTableGetOffsetForID(xrefTable, obid), bufsize, master, &containerOffset);
        PDAssert(containerDef);
        PDObjectRef obstmObject = PDObjectCreate(0, 0);
        obstmObject->def = containerDef;
        obstmObject->crypto = parser->crypto;
        obstmObject->streamLen = len = pd_stack_peek_int(pd_stack_get_dict_key(containerDef, "Length", false)->prev->prev);
        
        PDTwinStreamFetchBranch(stream, containerOffset, len + 20, &tb);
        PDScannerRef streamrdr = PDScannerCreateWithState(pdfRoot);
        PDScannerContextPush(stream, &PDTwinStreamDisallowGrowth);
        streamrdr->buf = tb;
        streamrdr->boffset = 0;
        streamrdr->bsize = len + 20;

        char *rawBuf = malloc(len);
        PDScannerAssertString(streamrdr, "stream");
        PDScannerReadStream(streamrdr, len, rawBuf, len);
        PDRelease(streamrdr);
        PDTwinStreamCutBranch(stream, tb);
        PDScannerContextPop();
        
        obstm = PDObjectStreamCreateWithObject(obstmObject);
        PDRelease(obstmObject);

        PDObjectStreamParseRawObjectStream(obstm, rawBuf);
        if (obstm->elements[index].type == PDObjectTypeString) {
            stack = NULL;
            pd_stack_push_key(&stack, obstm->elements[index].def);
        } else {
            stack = obstm->elements[index].def;
            obstm->elements[index].def = NULL;
        }
        PDRelease(obstm);
        
        PDAssert(outOffset == NULL);
        
        return stack;
    } 
    
    PDXOffsetType offset = PDXTableGetOffsetForID(xrefTable, obid);
    if (outOffset) *outOffset = offset;
    PDTwinStreamFetchBranch(stream, offset, bufsize, &tb);
    
    PDScannerRef tmpscan = PDScannerCreateWithState(pdfRoot);
    PDScannerContextPush(stream, &PDTwinStreamDisallowGrowth);
    tmpscan->buf = tb;
    tmpscan->boffset = 0;
    tmpscan->bsize = bufsize;
    
    if (PDScannerPopStack(tmpscan, &stack)) {
        if (! stream->outgrown) {
            pd_stack_assert_expected_key(&stack, "obj");
            pd_stack_assert_expected_int(&stack, obid);
        }
        pd_stack_destroy(stack);
    }
    
    stack = NULL;
    if (! PDScannerPopStack(tmpscan, &stack)) {
        if (PDScannerPopString(tmpscan, &string)) {
            pd_stack_push_key(&stack, string);
        }
    }
    
    if (outOffset != NULL) *outOffset += tmpscan->boffset;
    
    PDTwinStreamCutBranch(parser->stream, tb);

    PDRelease(tmpscan);
    PDScannerContextPop();
    
    if (stream->outgrown) {
        // the object did not fit in our expected buffer, which means it's unusually big; we bump the buffer size to 6k if it's smaller, otherwise we consider this a failure
        pd_stack_destroy(stack);
        stack = NULL;
        if (bufsize < 9288)
            return PDParserLocateAndCreateDefinitionForObjectWithSize(parser, obid, 9288, master, outOffset);
    }
    
    return stack;
}

pd_stack PDParserLocateAndCreateDefinitionForObject(PDParserRef parser, PDInteger obid, PDBool master)
{
    return PDParserLocateAndCreateDefinitionForObjectWithSize(parser, obid, 4192, master, NULL);
}

void PDParserFetchStreamLengthFromObjectDictionary(PDParserRef parser, pd_stack entry)
{
    entry = entry->prev->prev;
    if (entry->type == pd_stack_STACK) {
        PDInteger refid;
        pd_stack stack;
        // this is a reference length
        // e, Length, { ref, 1, 2 }
        entry = entry->info;
        PDAssert(PDIdentifies(entry->info, PD_REF));
        refid = pd_stack_peek_int(entry->prev);
        PDAssert(refid < parser->cxt->cap);
        
        stack = PDParserLocateAndCreateDefinitionForObject(parser, refid, false);
        parser->streamLen = pd_stack_pop_int(&stack);
    } else {
        char *string;
        // e, Length, 116
        string = entry->info;
        parser->streamLen = atol(string);
        PDAssert(parser->streamLen > 0 || !strcmp("0", string));
    }
}

char *PDParserFetchCurrentObjectStream(PDParserRef parser, PDInteger obid)
{
    PDObjectRef ob = parser->construct;
    
    PDAssert(obid == parser->obid);
    PDAssert(ob);
    PDAssert(ob->obid == obid);
    PDAssert(ob->hasStream);
    
    if (ob->extractedLen != -1) return ob->streamBuf;
    
    PDAssert(parser->state == PDParserStateObjectAppendix);
    
    PDInteger len = parser->streamLen;
    const char *filterName = PDObjectGetDictionaryEntry(parser->construct, "Filter");

    //PDScannerAssertString(parser->scanner, "stream");

    char *rawBuf = malloc(len);
    PDScannerReadStream(parser->scanner, len, rawBuf, len);
    
    if (filterName) {
        filterName = &filterName[1];
        pd_stack filterOpts = pd_stack_get_dict_key(ob->def, "DecodeParms", false);
        if (filterOpts) 
            filterOpts = PDStreamFilterGenerateOptionsFromDictionaryStack(filterOpts->prev->prev->info);
        PDStreamFilterRef filter = PDStreamFilterObtain(filterName, true, filterOpts);
        char *extractedBuf;
        PDStreamFilterApply(filter, (unsigned char *)rawBuf, (unsigned char **)&extractedBuf, len, &len);
        free(rawBuf);
        rawBuf = extractedBuf;
        PDRelease(filter);
    }
    
    ob->extractedLen = len;
    ob->streamBuf = rawBuf;
    
    parser->state = PDParserStateObjectPostStream;
    
    return rawBuf;
}

void PDParserUpdateObject(PDParserRef parser)
{
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
    
    if (ob->synchronizer) (*ob->synchronizer)(parser, ob, ob->syncInfo);
    
    if (ob->deleteObject) {
        ob->skipObject = true;
        PDXTableSetTypeForID(parser->mxt, ob->obid, PDXTypeFreed);
    }
    
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
    //                         >>>>>>>>>>>>>>>>>>>>
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
        //                         >>>>>>>>>>>>>>>>>>>>
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
            if (parser->state != PDParserStateObjectPostStream) {
                PDScannerSkip(scanner, parser->streamLen);
            }
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
        //                         >>>>>>>>>>>>>>>>>>>>
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
                //                                            012345 6
                PDTwinStreamInsertContent(parser->stream, 7, "stream\n");
                PDTwinStreamInsertContent(parser->stream, ob->ovrStreamLen, ob->ovrStream);
                //                                             0123456789 0123456 7
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
    
    PDRelease(ob);
    parser->construct = NULL;
    
    parser->state = PDParserStateBase;
    parser->streamLen = 0;
}

void PDParserPassoverObject(PDParserRef parser);

void PDParserPassthroughObject(PDParserRef parser)
{
    char *string;
    pd_stack stack, entry;
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
        parser->oboffset = (PDSize)PDTwinStreamGetOutputOffset(parser->stream);
        return;
    }
    
    scanner = parser->scanner;
    
    switch (parser->state) {
        case PDParserStateObjectDefinition:
            if (PDScannerPopStack(scanner, &stack)) {
                if (parser->encryptRef && parser->obid == parser->encryptRef->obid) {
                    // this is an encryption dictionary; those have a Length field that is not the length of the object stream
                } else {
                    entry = pd_stack_get_dict_key(stack, "Length", false);
                    if (entry) {
                        PDParserFetchStreamLengthFromObjectDictionary(parser, entry);
                    }
                }

                // this may be an xref object stream; we can those; if it is the master object, it will be appended to the end of the stream regardless
                entry = pd_stack_get_dict_key(stack, "Type", false);
                if (entry) {
                    entry = entry->prev->prev->info; // entry value is a stack (a name stack)
                    PDAssert(PDIdentifies(entry->info, PD_NAME));
                    if (!strcmp("XRef", entry->prev->info)) {
                        pd_stack_destroy(stack);
                        PDXSetTypeForID(parser->mxt->xrefs, parser->obid, PDXTypeFreed);
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
                                pd_stack_destroy(stack);
                            } else {
                                // whoops, no
                                pd_stack_push_stack(&scanner->resultStack, stack);
                            } 
                        }
#endif
                        return;
                    }
                }
                pd_stack_destroy(stack);
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
    
    parser->oboffset = (PDSize)PDTwinStreamGetOutputOffset(parser->stream);
    PDTwinStreamAsserts(parser->stream);
}

void PDParserPassoverObject(PDParserRef parser)
{
    char *string;
    pd_stack stack, entry;
    PDScannerRef scanner;
    
    scanner = parser->scanner;
    
    switch (parser->state) {
        case PDParserStateObjectDefinition:
            if (PDScannerPopStack(scanner, &stack)) {
                if (parser->encryptRef && parser->obid == parser->encryptRef->obid) {
                    // this is an encryption dictionary; those have a Length field that is not the length of the object stream
                } else {
                    entry = pd_stack_get_dict_key(stack, "Length", false);
                    if (entry) {
                        PDParserFetchStreamLengthFromObjectDictionary(parser, entry);
                    }
                }
                pd_stack_destroy(stack);
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
        obj = parser->construct = pd_stack_pop_object(&parser->appends);
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
        PDAssert(0 == PDBTreeGetCount(parser->skipT)); // crash = we lost objects
        parser->success &= NULL == parser->xstack && 0 == PDBTreeGetCount(parser->skipT);
        return false;
    }
    
    PDXTableRef next = pd_stack_pop_object(&parser->xstack);
    
    PDBool retval = true;
    if (parser->cxt != parser->mxt) {
        PDRelease(parser->cxt);
        parser->cxt = next;

        if (parser->cxt->pos < PDTwinStreamGetInputOffset(parser->stream)) 
            retval = PDParserIterateXRefDomain(parser);
    } else {
        if (next->pos < PDTwinStreamGetInputOffset(parser->stream)) 
            retval = PDParserIterateXRefDomain(parser);
        PDRelease(next);
    }
    
    return retval;
}

// iterate to the next (non-deprecated) object
PDBool PDParserIterate(PDParserRef parser)
{
    PDBool running;
    PDBool skipObject;
    pd_stack stack;
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
            parser->oboffset = scanner->bresoffset + (PDSize)PDTwinStreamGetOutputOffset(parser->stream);
            
            PDTwinStreamAsserts(parser->stream);
            
            // first is the type, returned as an identifier
            
            typeid = pd_stack_pop_identifier(&stack);

            // we expect PD_XREF, PD_STARTXREF, or PD_OBJ
            
            if (typeid == &PD_XREF) {
                // xref entry; note that we use running as the 'should consume trailer' argument to passover as the two states coincide; we also and result to the result of passover, as it may result in an XREF iteration in some special cases (in which case it may result in PDF end)
                running = PDParserIterateXRefDomain(parser);
                running &= PDXTablePassoverXRefEntry(parser, stack, running); // if !running, will all compilers enter PDXTablePassover...()? if not, this is bad

                if (! running) return false;
                
                continue;
            }
            
            if (typeid == &PD_OBJ) {
                // object definition; this is what we're after, unless this object is deprecated
                parser->obid = nextobid = pd_stack_pop_int(&stack);
                PDAssert(nextobid < parser->mxt->cap);
                parser->genid = nextgenid = pd_stack_pop_int(&stack);
                pd_stack_destroy(stack);
                
                skipObject = false;
                parser->state = PDParserStateObjectDefinition;
                
                //printf("object %zd (genid = %zd)\n", nextobid, nextgenid);
                
                if (nextgenid != PDXGetGenForID(mxrefs, nextobid)) {
                    // this is the wrong object
                    skipObject = true;
                } else {
                    long long offset = scanner->bresoffset + PDTwinStreamGetInputOffset(parser->stream) - PDXGetOffsetForID(mxrefs, nextobid); // PDXOffset(mxrefs[nextobid]);
                    if (offset != 0) {
                        // okay, getting a slight bit out of hand here, but when we run into a bad offset, it sometimes is because someone screwed up and gave us:
                        // \r     \r\n1753 0 obj\r<</DecodeParm....... and claimed that the starting offset for 1753 0 was at the first \r, which it of course is not, it's 7 bytes later
                        PDInteger wsi = 0;
                        while (offset < 0 && PDOperatorSymbolGlob[(unsigned char)parser->stream->heap[parser->stream->cursor+(wsi++)]] == PDOperatorSymbolGlobWhitespace) 
                            offset++;
                        /*wsi = 1;
                        while (offset > 0 && PDOperatorSymbolGlob[parser->stream->heap[parser->stream->cursor-(wsi++)]] == PDOperatorSymbolGlobWhitespace) 
                            offset--;*/
                    }
                    //printf("offset = %lld\n", offset);
                    if (offset < 2 && offset > -2) {
                        PDBTreeDelete(parser->skipT, nextobid);
                        //pd_btree_remove(&parser->skipT, nextobid);
                    } else {
                        //printf("offset mismatch for object %zd\n", nextobid);
                        PDBTreeInsert(parser->skipT, nextobid, (void*)nextobid);
                        //pd_btree_insert(&parser->skipT, nextobid, (void*)nextobid);
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
                pd_stack_destroy(stack);
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
    
    while (newiter < count && PDXTypeFreed != PDXGetTypeForID(xrefs, newiter)) //PDXUsed(xrefs[newiter]))
        newiter++;
    if (newiter == cap) {
        // we must realloc xref as it can't contain all the xrefs
        parser->mxt->cap = parser->mxt->count = cap = cap + 1;
        xrefs = parser->mxt->xrefs = realloc(parser->mxt->xrefs, PDXWidth * cap);
    }
    PDXSetTypeForID(xrefs, newiter, PDXTypeUsed);
    PDXSetGenForID(xrefs, newiter, 0);

    parser->xrefnewiter = newiter;
    
    parser->obid = newiter;
    parser->genid = 0;
    parser->streamLen = 0;
    
    PDObjectRef object = parser->construct = PDObjectCreate(parser->obid, parser->genid);
    object->encryptedDoc = PDParserGetEncryptionState(parser);
    object->crypto = parser->crypto;
    
    // we need to retain the object twice; once because this returns a retained object, and once because we retain it ourselves, and release it in "UpdateObject"
    return PDRetain(object);
}

PDObjectRef PDParserCreateAppendedObject(PDParserRef parser)
{
    PDObjectRef object = PDParserCreateNewObject(parser);
    pd_stack_push_object(&parser->appends, object);

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
    object->crypto = parser->crypto;
    object->encryptedDoc = PDParserGetEncryptionState(parser);

    char *string;
    pd_stack stack;
    
    PDScannerRef scanner = parser->scanner;
    
    if (PDScannerPopStack(scanner, &stack)) {
        object->def = stack;
        object->type = PDObjectTypeFromIdentifier(stack->info);

        if (parser->encryptRef && parser->obid == parser->encryptRef->obid) {
            // this is an encryption dictionary; those have a Length field that is not the length of the object stream
            parser->streamLen = 0;
        } else {
            if ((stack = pd_stack_get_dict_key(stack, "Length", false))) {
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
    PDSize startxref = (PDSize)PDTwinStreamGetOutputOffset(parser->stream);
    
    // write XREF table and trailer
    PDXTableInsert(parser);
    
    // write startxref entry
    twinstream_printf("startxref\n%zu\n%%%%EOF\n", startxref);
    
    free(obuf);
}

PDInteger PDParserGetContainerObjectIDForObject(PDParserRef parser, PDInteger obid)
{
    if (PDXTypeComp != PDXTableGetTypeForID(parser->mxt, obid)) 
        return -1;
    
    return PDXTableGetOffsetForID(parser->mxt, obid);
}

PDBool PDParserIsObjectStillMutable(PDParserRef parser, PDInteger obid)
{
    return (PDTwinStreamGetInputOffset(parser->stream) <= PDXTableGetOffsetForID(parser->mxt, obid)); // PDXOffset(parser->mxt->fields[obid]));
}

PDObjectRef PDParserGetRootObject(PDParserRef parser)
{
    if (! parser->root) {
        pd_stack rootDef = PDParserLocateAndCreateDefinitionForObject(parser, parser->rootRef->obid, true);
        parser->root = PDObjectCreate(parser->rootRef->obid, parser->rootRef->genid);
        parser->root->def = rootDef;
        parser->root->crypto = parser->crypto;
    }
    return parser->root;
}

PDObjectRef PDParserGetInfoObject(PDParserRef parser)
{
    if (! parser->info) {
        pd_stack infoDef = PDParserLocateAndCreateDefinitionForObject(parser, parser->infoRef->obid, true);
        parser->info = PDObjectCreate(parser->infoRef->obid, parser->infoRef->genid);
        parser->info->def = infoDef;
        parser->info->crypto = parser->crypto;
    }
    return parser->info;
}

PDCatalogRef PDParserGetCatalog(PDParserRef parser)
{
    if (! parser->catalog) {
        PDObjectRef root = PDParserGetRootObject(parser);
        parser->catalog = PDCatalogCreateWithParserForObject(parser, root);
    }
    return parser->catalog;
}

PDInteger PDParserGetTotalObjectCount(PDParserRef parser)
{
    return parser->mxt->count;
}

