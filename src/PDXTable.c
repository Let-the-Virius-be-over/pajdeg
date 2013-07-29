//
//  PDXTable.c
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/27/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

#include <math.h>

#include "PDInternal.h"
#include "PDParser.h"
#include "PDTwinStream.h"
#include "PDScanner.h"
#include "PDStack.h"
#include "PDXTable.h"
#include "PDReference.h"
#include "PDObject.h"
#include "PDStreamFilter.h"
#include "PDPortableDocumentFormatState.h"

/**
 @todo Below functions swap endianness due to the fact numbers are big-endian in binary XRefs (and, I guess, in text-form XRefs as well); if the machine running Pajdeg happens to be big-endian as well, below methods need to be #ifdef-ified to handle this (by NOT swapping every byte around in the integral representation).
 */

PDXOffsetType PDXRefGetOffsetForArbitraryRepresentation(char *rep, PDInteger len)
{
    PDInteger i;
    unsigned char *o = (unsigned char *)rep;
    PDXOffsetType ot = 0;
    PDXOffsetType shift = 0;
    for (i = len-1; i >= 0; i--) {
        if (shift > 8*sizeof(PDXOffsetType)) {
            if (o[i] > 0) {
                PDWarn("XREF offset larger than largest value containable in the offset type. This is a bug, or the PDF is bigger than %.1f GBs.\n", pow(2.0, (8.0*sizeof(PDXOffsetType))-30.0));
            }
        } else {
            ot |= (o[i] << shift);
        }
        shift += 8;
    }
    return ot;
}

PDXOffsetType PDXRefGetOffsetForID(char *xrefs, PDInteger obid)
{
    unsigned char *o = (unsigned char *) &xrefs[PDXOffsAlign + obid * PDXWidth];
    // note: requires that offs size is 4
    return (PDXOffsetType)o[3] | (o[2]<<8) | (o[1]<<16) | (o[0]<<24);
}

void PDXRefSetOffsetForID(char *xrefs, PDInteger obid, PDXOffsetType offset)
{
    unsigned char *o = (unsigned char *) &xrefs[PDXOffsAlign + obid * PDXWidth];
    // note: requires that offs size is 4
    o[0] = (offset & 0xff000000) >> 24;
    o[1] = (offset & 0x00ff0000) >> 16;
    o[2] = (offset & 0x0000ff00) >> 8;
    o[3] = (offset & 0x000000ff);
    PDAssert(offset == ((PDXOffsetType)o[3] | (o[2]<<8) | (o[1]<<16) | (o[0]<<24)));
}

typedef struct PDXI *PDXI;
struct PDXI {
    PDInteger mtobid; // master trailer object id
    PDXTableRef pdx;
    PDParserRef parser;
    PDScannerRef scanner;
    PDStackRef queue;
    PDStackRef stack;
    PDTwinStreamRef stream;
    PDReferenceRef rootRef;
    PDReferenceRef infoRef;
    PDReferenceRef encryptRef;
    PDObjectRef trailer;
    PDInteger tables;
};

#define PDXIStart(parser) (struct PDXI) { \
    0,\
    NULL, \
    parser, \
    parser->scanner, \
    NULL, \
    NULL, \
    parser->stream, \
    NULL, \
    NULL, \
    NULL, \
    NULL, \
    0, \
}

void PDXTableDestroy(PDXTableRef xtable)
{
    free(xtable->xrefs);
    free(xtable);
}

PDBool PDXTableInsertXRef(PDParserRef parser)
{
#define twinstream_printf(fmt...) \
    len = sprintf(obuf, fmt); \
    PDTwinStreamInsertContent(stream, len, obuf)
#define twinstream_put(len, buf) \
    PDTwinStreamInsertContent(stream, len, (char*)buf);
    char *obuf = malloc(512);
    PDInteger len;
    PDInteger i;
    PDTwinStreamRef stream = parser->stream;
    PDXTableRef mxt = parser->mxt;
    
    // write xref header
    twinstream_printf("xref\n%d %llu\n", 0, mxt->count);
    
    // write xref table
    twinstream_put(20, "0000000000 65535 f \n");
    for (i = 1; i < mxt->count; i++) {
        twinstream_printf("%010u %05d %c \n", PDXTableOffsetForID(mxt, i), *PDXTableGenForID(mxt, i), PDXTableIsIDFree(mxt, i) ? 'f' : 'n');
    }
    
    PDObjectRef tob = parser->trailer;
    
    sprintf(obuf, "%zd", parser->mxt->count);
    PDObjectSetDictionaryEntry(tob, "Size", obuf);
    
    char *string = NULL;
    len = PDObjectGenerateDefinition(tob, &string, 0);
    // overwrite "0 0 obj" 
    //      with "trailer"
    memcpy(string, "trailer", 7);
    PDTwinStreamInsertContent(stream, len, string); 
    free(string);
    
    free(obuf);
    
    return true;
}

extern void PDParserPassthroughObject(PDParserRef parser);

PDBool PDXTableInsertXRefStream(PDParserRef parser)
{
    char *obuf = malloc(128);

    PDObjectRef trailer = parser->trailer;
    PDXTableRef mxt = parser->mxt;
    
    PDXRefSetOffsetForID(mxt->xrefs, trailer->obid, parser->oboffset);
    *PDXRefTypeForID(mxt->xrefs, trailer->obid) = PDXTypeUsed;
    
    sprintf(obuf, "%zd", mxt->count);
    PDObjectSetDictionaryEntry(trailer, "Size", obuf);
    PDObjectSetDictionaryEntry(trailer, "W", PDXWEntry);

    PDObjectRemoveDictionaryEntry(trailer, "Prev");
    PDObjectRemoveDictionaryEntry(trailer, "Index");

    // override filters/decode params always -- better than risk passing something on by mistake that makes the xref stream unreadable
    PDObjectSetFlateDecodedFlag(trailer, true);
    PDObjectSetPredictionStrategy(trailer, PDPredictorPNG_UP, 6);
    
    PDObjectSetStreamFiltered(trailer, (const char *)mxt->xrefs, PDXWidth * mxt->count);

    // now chuck this through via parser
    parser->state = PDParserStateBase;
    parser->obid = trailer->obid;
    parser->genid = trailer->genid = 0;
    parser->construct = PDObjectRetain(trailer);

    PDParserPassthroughObject(parser);
    
    free(obuf);
    
    return true;
}

PDBool PDXTableInsert(PDParserRef parser)
{
    if (parser->mxt->format == PDXTableFormatText) {
        return PDXTableInsertXRef(parser);
    } else {
        return PDXTableInsertXRefStream(parser);
    }
}

PDBool PDParserIterateXRefDomain(PDParserRef parser);

PDBool PDXTablePassoverXRefEntry(PDParserRef parser, PDStackRef stack, PDBool includeTrailer)
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
        
        running = PDScannerPopStack(scanner, &stack);
        if (running) PDStackAssertExpectedKey(&stack, "xref");
    } while (running);
    
    if (includeTrailer) {
        // read the trailer
        PDScannerAssertString(scanner, "trailer");
        
        // read the trailer dict
        PDScannerAssertStackType(scanner);
        
        // next is the startxref, except some PDF creators (Pages?) drop this nice burp:
        // trailer^M<</Size 10619/Root 10086 0 R>>^Mxref^M0 1^M0000000000 65535 f
        // trailer^M<</Size 10619/Root 10086 0 R/Info 10082 0 R/ID[<B426B7E075B899285BA9A41C8E8C22AC><AE4FA9CBEC3A42C1A878E076F8C838A9>]/Prev 4324067/XRefStm 72147>>^Mstartxref^M4536499^M%%EOF

        PDScannerPopStack(scanner, &stack);
        if (stack->info == &PD_XREF) {
            // this is an xref which means we're iterating the domain, and if we're supposed to continue reading (i.e. this was not the last entry), we loop
            if (PDParserIterateXRefDomain(parser)) {
                PDStackPopIdentifier(&stack);
                return PDXTablePassoverXRefEntry(parser, stack, true);
            }
            return false;
        }

        // read startxref
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
    
    return true;
}



static inline PDBool PDXTableFindStartXRef(PDXI X)
{
    PDScannerRef xrefScanner;
    
    PDTWinStreamSetMethod(X->stream, PDTwinStreamReversed);
    
    xrefScanner = PDScannerCreateWithStateAndPopFunc(xrefSeeker, &PDScannerPopSymbolRev);
    
    PDScannerContextPush(X->stream, &PDTwinStreamGrowInputBufferReversed);
    
    // we expect a stack, because it should have skipped until it found startxref

    /// @todo If this is a corrupt PDF, or not a PDF at all, the scanner may end up scanning forever so we put a cap on # of loops -- 100 is overkill but who knows what crazy footers PDFs out there may have (the spec probably disallows that, though, so this should be investigated and truncated at some point)
    PDScannerSetLoopCap(100);
    if (! PDScannerPopStack(xrefScanner, &X->stack)) {
        PDScannerContextPop();
        return false;
    }
    
    // this stack should start out with "xref" indicating the ob type
    PDStackAssertExpectedKey(&X->stack, "startxref");
    // next is the offset 
    PDStackPushIdentifier(&X->queue, (PDID)PDStackPopSize(&X->stack));
    PDAssert(X->stack == NULL);
    PDScannerDestroy(xrefScanner);
    
    // we're now ready to skip to the first XRef
    PDTWinStreamSetMethod(X->stream, PDTwinStreamRandomAccess);
    PDScannerContextPop();
    
    return true;
}

// a 1.5 ob stream header; pull in the definition, skip past stream, then move on
static inline PDBool PDXTableReadXRefStreamHeader(PDXI X)
{
    PDStackPopIdentifier(&X->stack);
    X->mtobid = PDStackPopInt(&X->stack);
    PDStackDestroy(X->stack);
    PDScannerPopStack(X->scanner, &X->stack);
    PDScannerAssertString(X->scanner, "stream");
    PDInteger len = PDIntegerFromString(PDStackGetDictKey(X->stack, "Length", false)->prev->prev->info);
    PDScannerSkip(X->scanner, len);
    PDTwinStreamAdvance(X->stream, X->scanner->boffset);
    PDScannerAssertComplex(X->scanner, PD_ENDSTREAM);
    PDScannerAssertString(X->scanner, "endobj");
        
    return true;
}

static inline PDBool PDXTableReadXRefStreamContent(PDXI X)
{
    PDSize size;
    char *xrefs;
    char *buf;
    char *bufi;
    PDBool aligned;
    PDInteger len;
    PDStackRef byteWidths;
    PDStackRef index;
    PDStackRef filterOpts;
    PDInteger startob;
    PDInteger obcount;
    PDStackRef filterDef;
    PDInteger j;
    PDInteger i;
    PDInteger sizeT;
    PDInteger sizeO;
    PDInteger sizeI;
    PDInteger padT;
    PDInteger padO;
    PDInteger padI;
    PDInteger shrT;
    PDInteger shrO;
    PDInteger shrI;
    PDInteger capT;
    PDInteger capO;
    PDInteger capI;
    PDXTableRef pdx;
    
    pdx = X->pdx;
    pdx->format = PDXTableFormatBinary;
    
    // pull in defs stack and get ready to read stream
    PDStackDestroy(X->stack);
    PDScannerPopStack(X->scanner, &X->stack);
    PDScannerAssertString(X->scanner, "stream");
    len = PDIntegerFromString(PDStackGetDictKey(X->stack, "Length", false)->prev->prev->info);
    byteWidths = PDStackGetDictKey(X->stack, "W", false);
    index = PDStackGetDictKey(X->stack, "Index", false);
    filterDef = PDStackGetDictKey(X->stack, "Filter", false);
    size = PDIntegerFromString(PDStackGetDictKey(X->stack, "Size", false)->prev->prev->info);
    
    PDStreamFilterRef filter = NULL;
    if (filterDef) {
        // ("name"), "filter name"
        filterDef = as(PDStackRef, filterDef->prev->prev->info)->prev;
        filterOpts = PDStackGetDictKey(X->stack, "DecodeParms", false);
        if (filterOpts) 
            filterOpts = PDStreamFilterCreateOptionsFromDictionaryStack(filterOpts->prev->prev->info);
        filter = PDStreamFilterObtain(filterDef->info, true, filterOpts);
    }
    
    if (size > pdx->count) {
        pdx->count = size;
        if (size > pdx->cap) {
            /// @todo this size is known beforehand, or can be known beforehand, in pass 1; xrefs should never have to be reallocated, except for the initial setup
            pdx->cap = size;
            pdx->xrefs = realloc(pdx->xrefs, PDXWidth * size);
        }
    }
        
    if (filter) {
        /// @todo We know from 'size' exactly how many bytes we expect out of this thing, so we can set buffer to this value instead of basing it off len (compressed stream length)
        
        PDInteger got = 0;
        PDInteger cap = len < 1024 ? 1024 : len * 4;
        buf = malloc(cap);
        
        PDScannerAttachFilter(X->scanner, filter);
        PDInteger bytes = PDScannerReadStream(X->scanner, len, buf, cap);
        while (bytes > 0) {
            got += bytes;
            if (! filter->finished && filter->bufOutCapacity < 512) {
                cap *= (cap < 8192 ? 4 : 2); // we don't want to hit caps in filters more than once or twice, but we don't want retardo-huge buffers either so we can the rapid growth to 8k and then double after that
                buf = realloc(buf, cap);
            }
            bytes = PDScannerReadStreamNext(X->scanner, &buf[got]  , cap - got);
        }

        PDScannerDetachFilter(X->scanner);
    } else {
        buf = malloc(len);
        PDScannerReadStream(X->scanner, len, buf, len);
    }
    
    // process buf
    
    byteWidths = as(PDStackRef, byteWidths->prev->prev->info)->prev->prev->info;
    sizeT = PDIntegerFromString(as(PDStackRef, byteWidths->info)->prev->info);
    sizeO = PDIntegerFromString(as(PDStackRef, byteWidths->prev->info)->prev->info);
    sizeI = PDIntegerFromString(as(PDStackRef, byteWidths->prev->prev->info)->prev->info);
    
    // this layout may be optimized for Pajdeg; if it is, we can inject the buffer straight into the data structure
    aligned = sizeT == PDXTypeSize && sizeO == PDXOffsSize && sizeI == PDXGenSize;
    
    if (! aligned) {
        // not aligned, so need pad
#define setup_align(suf, our_size) \
        if (our_size > size##suf) { \
            cap##suf = size##suf; \
            pad##suf = our_size - size##suf; \
            shr##suf = 0; \
        } else { \
            cap##suf = our_size; \
            pad##suf = 0; \
            shr##suf = size##suf - our_size; \
        }

        setup_align(T, PDXTypeSize);
        setup_align(O, PDXOffsSize);
        setup_align(I, PDXGenSize);
        
#undef setup_align
#define transfer_pc(dst, src, pad, shr, cap, i) \
            for (i = 0; i < pad; i++) \
                dst[i] = 0; \
            memcpy(&dst[pad], src, cap); \
            dst += pad + cap; \
            src += shr + cap
#define transfer_pcs(dst, src, i, suf) transfer_pc(dst, src, pad##suf, shr##suf, cap##suf, i)
    }
    
    // index, which is optional, can fine tune startob/obcount; it defaults to [0 Size]

#define index_pop() \
    startob = PDIntegerFromString(as(PDStackRef, index->info)->prev->info); \
    obcount = PDIntegerFromString(as(PDStackRef, index->prev->info)->prev->info); \
    index = index->prev->prev
    
    startob = 0;
    obcount = size;

    if (index) {
        // move beyond header
        index = as(PDStackRef, index->prev->prev->info)->prev->prev->info;
        // some crazy PDF creators out there may think it's a lovely idea to put an empty index in; if they do, we will presume they meant the default [0 Size]
        if (index) {
            index_pop();
        }
    }
    
    xrefs = pdx->xrefs;
    bufi = buf;
    
    do {
        PDAssert(startob + obcount <= size);
        
        if (aligned) {
            memcpy(&pdx->xrefs[startob * PDXWidth], bufi, obcount * PDXWidth);
            bufi += obcount * PDXWidth;
        } else {
            char *dst = &pdx->xrefs[startob * PDXWidth];
            
            for (i = 0; i < obcount; i++) {
                // transfer 
                transfer_pcs(dst, bufi, j, T);
#ifdef DEBUG
                PDXOffsetType mark = PDXRefGetOffsetForArbitraryRepresentation(bufi, sizeO);
#endif
                transfer_pcs(dst, bufi, j, O);
                transfer_pcs(dst, bufi, j, I);
                PDAssert(((dst - pdx->xrefs) % PDXWidth) == 0);
#ifdef DEBUG
                printf("force-aligned XREF entry: #%ld: %u (%d)\n", i+startob, PDXTableOffsetForID(pdx, startob+i), *PDXTableGenForID(pdx, startob+i));
                PDAssert(mark == PDXRefGetOffsetForID(pdx->xrefs, startob+i)); // crash = transfer failure
#endif
            }
        }
        
        obcount = 0;
        if (index) {
            index_pop();
            PDAssert(obcount > 0);
        }
    } while (obcount > 0);
    
#undef transfer_pcs
#undef transfer_pc
#undef index_pop
    
    free(buf);
    
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
    
    PDScannerAssertComplex(X->scanner, PD_ENDSTREAM);
    PDScannerAssertString(X->scanner, "endobj");
    
    return true;
}

static inline PDBool PDXTableReadXRefHeader(PDXI X)
{
    // we keep popping stacks until we fail, which means we've encountered the trailer (which is a string, not a stack)
    do {
        // this stack = xref, startobid, <startobid>, count, <count>
        PDStackAssertExpectedKey(&X->stack, "xref");
        PDStackPopInt(&X->stack);
        PDInteger count = PDStackPopInt(&X->stack);
        
        // we now have a stream (technically speaking) of xrefs
        count *= 20;
        PDScannerSkip(X->scanner, count);
        PDTwinStreamAdvance(X->stream, X->scanner->boffset);
    } while (PDScannerPopStack(X->scanner, &X->stack));
    
    // we now get the trailer
    PDScannerAssertString(X->scanner, "trailer");
    
    // and the trailer dictionary
    PDScannerPopStack(X->scanner, &X->stack);

    return true;
}

static inline PDBool PDXTableReadXRefContent(PDXI X)
{
    PDSize size;
    PDInteger i;
    char *buf;
    char *src;
    char *dst;
    PDXGenType *freeLink;

    PDXTableRef pdx = X->pdx;
    pdx->format = PDXTableFormatText;
    
    do {
        // this stack = xref, startobid, <startobid>, count, <count>
        PDStackAssertExpectedKey(&X->stack, "xref");
        PDInteger startobid = PDStackPopInt(&X->stack);
        PDInteger count = PDStackPopInt(&X->stack);
        
        //printf("[%d .. %d]\n", startobid, startobid + count - 1);
        
        size = startobid + count;
        
        if (size > pdx->count) {
            pdx->count = size;
            if (size > pdx->cap) {
                // we must realloc xref as it can't contain all the xrefs
                pdx->cap = size;
                pdx->xrefs = realloc(pdx->xrefs, PDXWidth * size);
            }
        }
        
        // we now have a stream (technically speaking) of xrefs
        PDInteger bytes = count * 20;
        buf = malloc(bytes);
        if (bytes != PDScannerReadStream(X->scanner, bytes, buf, bytes)) {
            free(buf);
            return false;
        }
        
        // convert into internal xref table
        src = buf;
        dst = &pdx->xrefs[PDXWidth * startobid];
        freeLink = NULL;
        PDBool used;
        PDXOffsetType offset;
        for (i = 0; i < count; i++) {
#define PDXOffset(pdx)      fast_mutative_atol(pdx, 10)
#define PDXGenId(pdx)       fast_mutative_atol(&pdx[11], 5)
#define PDXUsed(pdx)        (pdx[17] == 'n')
            
            offset = PDXOffset(src);
            
            // some PDF creators (determine who this is so they can contacted) incorrectly think setting generation number to 65536 is the same as setting the used character to 'f' (free) -- in order to not confuse Pajdeg, we address that here
            // other PDF creators think dumping 000000000 00000 n (i.e. this object can be found at offset 0, and it's in use) means "this object is unused"; we address that as well
#ifdef DEBUG
            if (PDXUsed(src) && (PDXGenId(src) == 65536 || offset == 0)) {
                PDWarn("warning: marking object #%ld as unused, because its generation id is 65536 or its offset is 0\n", i);
                
            }
#endif
            used = PDXUsed(src) && (PDXGenId(src) != 65536) && (offset != 0);
            
            PDXRefSetOffsetForID(dst, i, offset);

            if (used) {
                *PDXRefTypeForID(dst, i) = PDXTypeUsed;
                *PDXRefGenForID(dst, i) = PDXGenId(src);
            } else {
                // freed objects link to each other in obstreams
                *PDXRefTypeForID(dst, i) = PDXTypeFreed;
                if (freeLink) 
                    *freeLink =  startobid + i;
                freeLink = PDXRefGenForID(dst, i);
                *freeLink = 0;
            }
            
            src += 20;
        }
    } while (PDScannerPopStack(X->scanner, &X->stack));
    
    return true;
}

static inline void PDXTableParseTrailer(PDXI X)
{
    // if we have no Root or Info yet, grab them if found
    PDStackRef dictStack;
    if (X->rootRef == NULL && (dictStack = PDStackGetDictKey(X->stack, "Root", false))) {
        X->rootRef = PDReferenceCreateFromStackDictEntry(dictStack->prev->prev->info);
    }
    if (X->infoRef == NULL && (dictStack = PDStackGetDictKey(X->stack, "Info", false))) {
        X->infoRef = PDReferenceCreateFromStackDictEntry(dictStack->prev->prev->info);
    }
    if (X->encryptRef == NULL && (dictStack = PDStackGetDictKey(X->stack, "Encrypt", false))) {
        X->encryptRef = PDReferenceCreateFromStackDictEntry(dictStack->prev->prev->info);
    }
    
    // a Prev key may or may not exist, in which case we want to hit it
    PDStackRef prev = PDStackGetDictKey(X->stack, "Prev", false);
    if (prev) {
        // e, Prev, 116
        char *s = prev->prev->prev->info;
        PDStackPushIdentifier(&X->queue, (PDID)PDSizeFromString(s));
    } 
    
    // For 1.5+ PDF:s, an XRefStm may exist; it takes precedence over Prev, but does not override Prev
    if ((dictStack = PDStackGetDictKey(X->stack, "XRefStm", false))) {
        /// @todo FIND A CASE WHERE XRefStm EXISTS!
        char *s = dictStack->prev->prev->info;
        PDSize xrefStreamOffs = PDSizeFromString(s);
        // X->queue is a queue of objects newer-to-older, which means pushing the XRefStm entry after pushing the Prev entry will do the right thing (XRefStm takes precedence, Prev is not skipped)
        PDStackPushIdentifier(&X->queue, (PDID)xrefStreamOffs);
    }
    
    // update the trailer object in case additional info is included
    // TODO: determine if spec requires this or if the last trailer is the whole truth
    if (X->trailer->def == NULL) {
        X->trailer->obid = X->mtobid;
        X->trailer->def = X->stack;
    } else {
        /// @todo Disabled this part; the master trailer has to contain vital stuff or Pajdeg loses it; this will e.g. put a bunch of stream related stuff into the regular trailer if a PDF has mixed XRef streams and plaintext XRefs
#if 0
        char *key;
        char *value;
        PDStackRef iter = X->stack; 
        while (PDStackGetNextDictKey(&iter, &key, &value)) {
            if (NULL == PDObjectGetDictionaryEntry(X->trailer, key)) {
                PDObjectSetDictionaryEntry(X->trailer, key, value);
            }
            free(value);
        }
#endif
        PDStackDestroy(X->stack);
    }
}

PDBool PDXTableFetchHeaders(PDXI X)
{
    PDBool running;
    PDStackRef osstack;
    
    X->tables = 0;
    X->mtobid = 0;
    X->trailer = X->parser->trailer = PDObjectCreate(0, 0);
    
    osstack = NULL;
    running = true;
    
    do {
        // pull next offset out of queue into the offsets stack and jump there
        X->tables++;
        PDStackPopInto(&osstack, &X->queue);
        
        // jump to xref
        printf("offset = %lld\n", (PDSize)osstack->info);
        PDTwinStreamSeek(X->stream, (PDSize)osstack->info);
        
        // set up scanner
        X->scanner = PDScannerCreateWithState(pdfRoot);
        
        // if this is a v1.5 PDF, we may run into an object definition here; the object is the replacement for the trailer, and has a (usually compressed) stream of the XREF table
        if (PDScannerPopStack(X->scanner, &X->stack)) {
            // we determine this by checking the identifier for the popped stack
            if (PDIdentifies(X->stack->info, PD_OBJ)) {
                if (! PDXTableReadXRefStreamHeader(X)) {
                    PDWarn("Failed to read XRef stream header.");
                    return false;
                }
            } else {
                // this is a regular old xref table with a trailer at the end
                if (! PDXTableReadXRefHeader(X)) {
                    PDWarn("Failed to read XRef header.");
                    return false;
                }
            }
        }
        
        PDXTableParseTrailer(X);
        
        PDScannerDestroy(X->scanner);
    } while (X->queue);
    
    X->stack = osstack;
    
    return true;
}

void PDXTablePrint(PDXTableRef pdx)
{
    char *xrefs = pdx->xrefs;
    PDInteger i;
    
    char *types[] = {"free", "used", "compressed"};
    
    printf("XREF with %lld objects @ %lld:\n", pdx->count, pdx->pos);

    for (i = 0; i < pdx->count; i++) {
        PDOffset offs = PDXRefGetOffsetForID(xrefs, i);
        printf("#%03ld: %010lld (%s)\n", i, offs, types[*PDXRefTypeForID(xrefs, i)]);
    }
}

PDBool PDXTableFetchContent(PDXI X)
{
    PDXTableRef *tables;
    PDSize      *offsets;
    PDSize       offs;
    PDInteger offscount;
    PDInteger j;
    PDInteger i;
    PDStackRef osstack;
    PDXTableRef prev;
    PDXTableRef pdx;
    
    // fetch headers puts offset stack into X as stack so we get that out
    osstack = X->stack;
    X->stack = NULL;
    
    // we now have a stack in versioned order, so we start setting up xrefs
    offsets = malloc(X->tables * sizeof(PDSize));
    tables = malloc(X->tables * sizeof(PDXTableRef));
    offscount = 0;
    
    pdx = NULL;
    while (0 != (offs = (PDSize)PDStackPopIdentifier(&osstack))) {
        prev = pdx;
        if (pdx) {
            /// @todo CLANG doesn't like this (pdx is stored in tables, put into ctx stack, and released on PDParserDestroy)
            pdx = memcpy(malloc(sizeof(struct PDXTable)), pdx, sizeof(struct PDXTable));
            pdx->xrefs = memcpy(malloc(pdx->cap * PDXWidth), pdx->xrefs, pdx->cap * PDXWidth);
            prev->next = pdx;
        } else {
            pdx = malloc(sizeof(struct PDXTable));
            pdx->linearized = false;
            pdx->cap = 0;
            pdx->count = 0;
            pdx->next = NULL;
            pdx->xrefs = NULL;
        }
        
        pdx->prev = prev;
        X->pdx = pdx;
        
        // put offset in (sorted)
        for (i = 0; i < offscount && offsets[i] < offs; i++) ;
        for (j = offscount; j > i; j--) {
            offsets[j] = offsets[j-1];
            tables[j] = tables[j-1];
        }
        offsets[i] = offs;
        tables[i] = pdx;
        offscount++;
        
        pdx->pos = offs;
        
        // jump to xref
        PDTwinStreamSeek(X->stream, offs);
        
        // set up scanner
        X->scanner = PDScannerCreateWithState(pdfRoot);
        
        // if this is a v1.5 PDF, we may run into an object definition here; the object is the replacement for the trailer, and has a (usually compressed) stream of the XREF table
        if (PDScannerPopStack(X->scanner, &X->stack)) {
            // we determine this by checking the identifier for the popped stack
            if (PDIdentifies(X->stack->info, PD_OBJ)) {
                if (! PDXTableReadXRefStreamContent(X)) {
                    PDWarn("Failed to read XRef stream header.");
                    return false;
                }
            } else {
                // this is a regular old xref table with a trailer at the end
                if (! PDXTableReadXRefContent(X)) {
                    PDWarn("Failed to read XRef header.");
                    return false;
                }
            }
        }
        
        PDScannerDestroy(X->scanner);
        PDStackDestroy(X->stack);
    }
    
    // pdx is now the complete input xref table with all offsets correct, so we use it as is for the master table
    X->parser->mxt = pdx;
    
    // we now set up the xstack from the (byte-ordered) list of xref tables; if the PDF is or appears to be linearized, however, we flatten the stack into one entry
    X->parser->xstack = NULL;
    if (X->tables == 2 && pdx->pos < pdx->prev->pos) {
        // master is before its precdecessor byte-wise, and the PDF has two XRef entries => linearized; flatten
        pdx->linearized = true;
        pdx->pos = pdx->prev->pos;
        PDStackPushIdentifier(&X->parser->xstack, (PDID)pdx);
    } else {
        // otherwise ensure that the master xref table is last; if it isn't, Pajdeg will end parsing prematurely
        for (i = X->tables - 1; i >= 0; i--) {
            // if the XREF comes after the master, the PDF is broken (?), and we bump the master XREF position and skip over the XREF entirely -- this is perfectly non-destructive in terms of data; the master XREF contains the complete set of changes applied in revision order; in theory, all XREF tables could be completely ignored with no side effects aside from safety harness of the parser seeing what it expects to be seeing; the one potential problem with dropping a revision is that indirect object referenced stream lengths for deprecated objects may receive the wrong length; I'm fine with Pajdeg failing at that point
            if (tables[i]->pos > pdx->pos) {
                /// @todo this is not the case when XRefStm objects are included, as that puts table count > 2
                PDWarn("Master XREF position adjusted due to bytewise successing predecessor -- this PDF is badly formed; may result in corrupted output\n");
                pdx->pos = tables[i]->pos;
                pdx->linearized = true;
            } else {
                PDStackPushIdentifier(&X->parser->xstack, (PDID)tables[i]);
            }
        }
    }
    free(offsets);
    free(tables);
    
    return true;
}

PDBool PDXTableFetchXRefs(PDParserRef parser)
{
    struct PDXI X = PDXIStart(parser);
    
    // find starting XRef position in PDF
    if (! PDXTableFindStartXRef(&X)) {
        return false;
    }
    
    // pass over XRefs once, to get offsets in the right order (we want oldest first)
    if (! PDXTableFetchHeaders(&X)) {
        return false;
    }
    
    // pass over XRefs again, in right order, and parse through content this time
    if (! PDXTableFetchContent(&X)) {
        return false;
    }
    
    // and finally pull out the current table
    parser->cxt = (PDXTableRef) PDStackPopIdentifier(&parser->xstack);
    
    parser->xrefnewiter = 1;
    
    parser->rootRef = X.rootRef;
    parser->infoRef = X.infoRef;
    parser->encryptRef = X.encryptRef;
    
    // we've got all the xrefs so we can switch back to the readwritable method
    PDTWinStreamSetMethod(X.stream, PDTwinStreamReadWrite);
    
    //#define DEBUG_PARSER_PRINT_XREFS
#ifdef DEBUG_PARSER_PRINT_XREFS
    printf("\n"
           "       XREFS     \n"
           "  OFFSET    GEN  U\n"
           "---------- ----- -\n"
           "%s", (char*)xrefs);
#endif
    
#define DEBUG_PARSER_CHECK_XREFS
#ifdef DEBUG_PARSER_CHECK_XREFS
    printf("* * * * *\nCHECKING XREFS\n* * * * * *\n");
    {
        char *xrefs = parser->mxt->xrefs;
        char *buf;
        char obdef[50];
        PDInteger bufl,obdefl,i,j;
        
        char *types[] = {"free", "used", "compressed"};
        
        for (i = 0; i < parser->mxt->count; i++) {
            PDOffset offs = PDXRefGetOffsetForID(xrefs, i);
            printf("object #%3ld: %10lld (%s)\n", i, offs, types[*PDXRefTypeForID(xrefs, i)]);
            if (PDXTypeUsed == *PDXRefTypeForID(xrefs, i)) {
                bufl = PDTwinStreamFetchBranch(X.stream, offs, 200, &buf);
                obdefl = sprintf(obdef, "%ld %d obj", i, *PDXRefGenForID(xrefs, i));//PDXGenId(xrefs[i]));
                if (bufl < obdefl || strncmp(obdef, buf, obdefl)) {
                    printf("ERROR: object definition did not start at %lld: instead, this was encountered: ", offs);
                    for (j = 0; j < 20 && j < bufl; j++) 
                        putchar(buf[j] < '0' || buf[j] > 'z' ? '.' : buf[j]);
                    printf("\n");
                }
            }
        }
    }
#endif
    
    return true;
}

