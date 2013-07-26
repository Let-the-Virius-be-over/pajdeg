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

#include "PDScanner.h"

#define fmatox(x, ato) \
    static inline x fast_mutative_##ato(char *str, PDInteger len) \
    { \
        char t = str[len]; \
        str[len] = 0; \
        x l = ato(str); \
        str[len] = t; \
        return l; \
    }

fmatox(long long, atoll)
fmatox(long, atol)

#define PDXOffset(pdx)      fast_mutative_atol(pdx.fields, 10)
#define PDXGenId(pdx)       fast_mutative_atol(&(pdx.fields)[11], 5)
#define PDXUsed(pdx)        (pdx.fields[17] == 'n')
#define PDXSetUsed(pdx,u)    pdx.fields[17] = (u ? 'n' : 'f')
#define PDXUndefined(pdx)   (pdx.fields[10] != ' ') // we use the space between ob and gen as indicator for whether the PDX was defined (everything is zeroed due to realloc)
#define PDXSetUndefined(pdx) pdx.fields[10] = 0;

static inline void PDXWrite(char *buf, PDInteger value, PDInteger digits)
{
    for (digits--; value > 0 && digits >= 0; digits--) {
        buf[digits] = '0' + (value % 10);
        value /= 10;
    }
    while (digits >= 0) 
        buf[digits--] = '0';
}

PDBool PDParserFetchXRefs(PDParserRef parser);

PDParserRef PDParserCreateWithStream(PDTwinStreamRef stream)
{
    PDPortableDocumentFormatStateRetain();
    
    PDParserRef parser = calloc(1, sizeof(struct PDParser));
    parser->stream = stream;
    parser->state = PDParserStateBase;
    parser->success = true;
    
    if (! PDParserFetchXRefs(parser)) {
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
    if (parser->skipT) PDBTreeDestroy(parser->skipT);
    while (parser->appends) {
        PDObjectRef ob = (PDObjectRef)PDStackPopIdentifier(&parser->appends);
        PDObjectRelease(ob);
    }
    while (parser->cxt) {
        free(parser->cxt->fields);
        free(parser->cxt);
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
    PDTwinStreamFetchBranch(stream, PDXOffset(xrefTable->fields[obid]), bufsize, &tb);
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
            
            //PDScannerReadStream(scanner, NULL, parser->streamLen);
            
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

void PDParserPassthroughObject(PDParserRef parser)
{
    char *string;
    PDStackRef stack, entry;
    PDScannerRef scanner;
    
    // update xref entry
    PDXWrite((char*)&parser->mxt->fields[parser->obid], parser->oboffset, 10);
    
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
                //PDScannerReadStream(scanner, NULL, parser->streamLen);
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

void PDParserPassoverXRef(PDParserRef parser, PDStackRef stack, PDBool includeTrailer)
{
    PDInteger count;
    PDBool running = true;
    PDScannerRef scanner = parser->scanner;
    
    do {
        // this stack = (xref,) startobid, count
        
        free(PDStackPopKey(&stack));
        count = PDStackPopInt(&stack);
        
        // we know the # of bytes to skip over, so we discard right away
        ////scanner->btrail = scanner->boffset;
        
        PDScannerSkip(scanner, count * 20);
        PDTwinStreamDiscardContent(parser->stream);//, PDTwinStreamScannerCommitBytes(parser->stream) + count * 20);
        //bytes = count * 20;
        //if (bytes != PDScannerReadStream(scanner, NULL, bytes)) {
        //    PDAssert(0);
        //}
        
        running = PDScannerPopStack(scanner, &stack);
        if (running) PDStackAssertExpectedKey(&stack, "xref");
    } while (running);
    
    if (includeTrailer) {
        // read the trailer
        PDScannerAssertString(scanner, "trailer");
        
        // read the trailer dict
        PDScannerAssertStackType(scanner);
        
        // read startxref
        PDScannerPopStack(scanner, &stack);
        PDStackAssertExpectedKey(&stack, "startxref");
        PDStackDestroy(stack);
        
        // next is EOF meta
        PDScannerPopStack(scanner, &stack);
        PDStackAssertExpectedKey(&stack, "meta");
        PDStackAssertExpectedKey(&stack, "EOF");
        
        // set trail and discard the content
        ////scanner->btrail = scanner->boffset;
        
        PDTwinStreamDiscardContent(parser->stream);//, PDTwinStreamScannerCommitBytes(parser->stream));
    }
}

// append objects
void PDParserAppendObjects(PDParserRef parser)
{
    PDObjectRef obj;
    while (parser->appends) {
        obj = parser->construct = (PDObjectRef)PDStackPopIdentifier(&parser->appends);
        parser->obid = obj->obid;
        parser->genid = obj->genid;
        parser->streamLen = obj->streamLen;
        PDParserPassthroughObject(parser);
    }
}

// iterate to the next (non-deprecated) object
PDBool PDParserIterate(PDParserRef parser)
{
    PDBool skipObject;
    PDStackRef stack;
    char * string;
    PDScannerRef scanner = parser->scanner;
    PDXRef mxrefs = parser->mxt->fields;
    size_t nextobid, nextgenid;

    PDTwinStreamAsserts(parser->stream);
    
    // we may have passed beyond the last object already
    if (PDTwinStreamGetInputOffset(parser->stream) >= parser->mxt->pos) {
        return false;
    }
    
    if (PDParserStateBase != parser->state || NULL != parser->construct) {
        PDParserPassthroughObject(parser);
    }

    PDTwinStreamAsserts(parser->stream);
    
    PDBool running = true;
    while (running) {
        // discard scanner trail up to this point
        if (scanner->boffset > 0) {
            PDTwinStreamDiscardContent(parser->stream);//, PDTwinStreamScannerCommitBytes(parser->stream));
        }
        
        PDTwinStreamAsserts(parser->stream);
        
        if (PDScannerPopStack(scanner, &stack)) {
            // mark output position
            parser->oboffset = scanner->bresoffset + PDTwinStreamGetOutputOffset(parser->stream);
            
            PDTwinStreamAsserts(parser->stream);
            
            // first string is the type
            string = (char *)*PDStackPopIdentifier(&stack);
            // we expect 'x'(ref), 'o'(bj)
            switch (string[0]) {
                case 'x':
                    // xref entry
                    
                    if (PDTwinStreamGetInputOffset(parser->stream) >= parser->mxt->pos) {
                        // iteration phase is ended; we now have the primary xref table in front of us, then the trailer; we want to pass over the xref part as always, but stop at the trailer; we also may have objects to append
                        PDParserAppendObjects(parser);
                        PDAssert(NULL == parser->xstack); // crash = we missed an xref table in the PDF; that's very not good
                        PDAssert(NULL == parser->skipT); // crash = we lost objects
                        parser->success &= NULL == parser->xstack && NULL == parser->skipT;
                        PDParserPassoverXRef(parser, stack, false);
                        return false;
                    }
                    
                    // not the final xref table; we iterate to the next one internally and pass over the definition
                    
                    PDAssert(PDTwinStreamGetInputOffset(parser->stream) >= parser->cxt->pos);
                    
                    PDXTableRef next = (PDXTableRef) PDStackPopIdentifier(&parser->xstack);
                    PDAssert(next);
                    
                    free(parser->cxt->fields);
                    free(parser->cxt);
                    parser->cxt = next;
                    
                    PDParserPassoverXRef(parser, stack, true);
                    break;
                    
                case 'o':
                    // object definition; this is what we're after, unless this object is deprecated
                    parser->obid = nextobid = PDStackPopInt(&stack);
                    PDAssert(nextobid < parser->mxt->cap);
                    parser->genid = nextgenid = PDStackPopInt(&stack);
                    PDStackDestroy(stack);

                    skipObject = false;
                    parser->state = PDParserStateObjectDefinition;
                    
                    //printf("object %zd (genid = %zd)\n", nextobid, nextgenid);
                    
                    if (nextgenid != PDXGenId(mxrefs[nextobid])) {
                        // this is the wrong object
                        skipObject = true;
                    } else {
                        long long offset = scanner->bresoffset + PDTwinStreamGetInputOffset(parser->stream) - PDXOffset(mxrefs[nextobid]);
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
                        break;
                    } 
                      
                    return true;
                default:
                    PDWarn("unknown type: %s\n", string);
                    PDAssert(0);
            }
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
    PDXRef xrefs;
    
    // we cannot be inside another object when we do this
    if (parser->state != PDParserStateBase || parser->construct) 
        PDParserPassthroughObject(parser);
    
    newiter = parser->xrefnewiter;
    count = parser->mxt->count;
    cap = parser->mxt->cap;
    xrefs = parser->mxt->fields;
    
    while (newiter < count && PDXUsed(xrefs[newiter]))
        newiter++;
    if (newiter == cap) {
        // we must realloc xref as it can't contain all the xrefs
        parser->mxt->cap = cap = cap + 1;
        xrefs = parser->mxt->fields = realloc(parser->mxt->fields, 20 * cap);
        //                       0123456789112345678 9
        memcpy(&xrefs[newiter], "0000000000 00000 n \n", 20);
    } else {
        PDInteger i;
        PDXSetUsed(xrefs[newiter], true);
        for (i = 11; i < 16; i++) xrefs[newiter].fields[i] = '0';
    }
    parser->xrefnewiter = newiter;
    
    parser->mxt->count++;
    
    parser->obid = newiter;
    parser->genid = 0;
    //parser->oboffset = PDTwinStreamGetOutputOffset(parser->stream);
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

PDBool PDParserFetchXRefs(PDParserRef parser)
{
    PDSize highob;
    PDScannerRef scanner;
    char *s;
    PDBool running;
    PDXTableRef *tables;
    PDSize      *offsets;
    PDInteger offscount;
    PDInteger j;
    PDInteger i;

    PDTwinStreamRef stream = parser->stream;
    PDXRef xrefs;// = parser->xrefs;

    PDTWinStreamSetMethod(stream, PDTwinStreamReversed);
    
    PDScannerRef xrefScanner = PDScannerCreateWithStateAndPopFunc(xrefSeeker, &PDScannerPopSymbolRev);
    PDScannerContextPush(stream, &PDTwinStreamGrowInputBufferReversed);
    
    // we expect a stack, because it should have skipped until it found startxref
    
    PDStackRef stack;
    PDScannerSetLoopCap(100);
    if (! PDScannerPopStack(xrefScanner, &stack)) {
        return false;
    }
    
    //PDStackShow(stack);
    
    // this stack should start out with "xref" indicating the ob type
    PDStackAssertExpectedKey(&stack, "startxref");
    // next is the offset 
    PDSize offs = PDStackPopSize(&stack);
    PDAssert(stack == NULL);
    PDScannerDestroy(xrefScanner);
    
    // we set up a stack of startxref pointers, and loop until we've got them in chronological order
    
    PDStackRef osstack = NULL;
    
    // we now start looping, each loop covering one xref entry, until we've jumped through all of them
    PDTWinStreamSetMethod(stream, PDTwinStreamRandomAccess);
    PDScannerContextPop();
    
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
        
        // we're positioned right, so pop the next stack (which should be the xref header)
        // we keep popping stacks until we fail, which means we've encountered the trailer (which is a string, not a stack)
        
        while (PDScannerPopStack(scanner, &stack)) {
            
            // this stack = xref, startobid, <startobid>, count, <count>
            PDStackAssertExpectedKey(&stack, "xref");
            PDStackPopInt(&stack);
            PDInteger count = PDStackPopInt(&stack);
            
            // we now have a stream (technically speaking) of xrefs
            count *= 20;
            PDScannerSkip(scanner, count);
            PDTwinStreamAdvance(stream, scanner->boffset);

        }
        
        // we now get the trailer
        PDScannerAssertString(scanner, "trailer");
        
        // and the trailer dictionary
        PDScannerPopStack(scanner, &stack);
        
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
        
        while (PDScannerPopStack(scanner, &stack)) {
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
            if (bytes != PDScannerReadStream(scanner, (char*)&xrefs[startobid], bytes)) {
                PDAssert(0);
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

void PDParserDone(PDParserRef parser)
{
    PDAssert(parser->success);

#define twinstream_printf(fmt...) \
        len = sprintf(obuf, fmt); \
        PDTwinStreamInsertContent(stream, len, obuf)
#define twinstream_put(len, buf) \
        PDTwinStreamInsertContent(stream, len, (char*)buf);
    char *obuf = malloc(2048);
    PDInteger len;
    PDTwinStreamRef stream = parser->stream;
    PDScannerRef scanner = parser->scanner;
    
    // iterate past all remaining objects, if any
    while (PDParserIterate(parser));
    
    // we should now be right at the trailer
    PDScannerAssertString(scanner, "trailer");
    
    // we already have the trailer defined so we don't have to read any more from the file
    
    // read the trailer dict
    //PDScannerPopStack(scanner, &trailer);
    
    // the output offset is our new startxref entry
    PDSize startxref = PDTwinStreamGetOutputOffset(parser->stream);
    
    // write xref header
    twinstream_printf("xref\n%d %llu\n", 0, parser->mxt->count);
    
    // also write xref table
    twinstream_put(20 * parser->mxt->count, parser->mxt->fields);
    
    // now we write the trailer using our stack
    //twinstream_put(10, "trailer\n");
    
    PDObjectRef tob = parser->trailer;
    //tob->def = trailer;
    
    sprintf(obuf, "%zd", parser->mxt->count);
    PDObjectSetDictionaryEntry(tob, "Size", obuf);
    /*if (parser->rootRef) {
        sprintf(obuf, "%d %d R", parser->rootRef->obid, parser->rootRef->genid);
        PDObjectSetDictionaryEntry(tob, "Root", obuf);
    }
    if (parser->infoRef) {
        sprintf(obuf, "%d %d R", parser->infoRef->obid, parser->infoRef->genid);
        PDObjectSetDictionaryEntry(tob, "Info", obuf);
    }*/

    char *string = NULL;
    len = PDObjectGenerateDefinition(tob, &string, 0);
    // overwrite "0 0 obj" 
    //      with "trailer"
    memcpy(string, "trailer", 7);
    PDTwinStreamInsertContent(stream, len, string); 
    free(string);
    
    PDObjectRelease(tob);
    
    // write startxref entry
    twinstream_printf("startxref\n%llu\n%%%%EOF\n", startxref);
    
    free(obuf);
}

PDBool PDParserIsObjectStillMutable(PDParserRef parser, PDInteger obid)
{
    return (PDTwinStreamGetOutputOffset(parser->stream) < PDXOffset(parser->mxt->fields[obid]));
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
