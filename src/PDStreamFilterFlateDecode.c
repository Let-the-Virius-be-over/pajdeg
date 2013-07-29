//
//  PDStreamFilterFlateDecode.c
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/26/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

#include "PDInternal.h"

#ifdef PD_SUPPORT_ZLIB

#include "PDStack.h"
#include "PDStreamFilterFlateDecode.h"
#include "zlib.h"

PDInteger fd_compress_init(PDStreamFilterRef filter)
{
    if (filter->initialized)
        return true;
    
    if (filter->options) {
        PDStackRef iter = filter->options;
        while (iter) {
            if (!strcmp(iter->info, "Predictor")) {
                // we need a predictor as well
                PDStreamFilterRef predictor = PDStreamFilterObtain(iter->info, false, filter->options);
                if (predictor) {
                    // untie this from ourselves or we will destroy it twice
                    filter->options = NULL;
                    // because prediction has to come before compression we do a swap-a-roo here
                    PDStreamFilterRef newSelf = malloc(sizeof(struct PDStreamFilter)); // new field for us
                    memcpy(newSelf, filter, sizeof(struct PDStreamFilter));     // move us there
                    memcpy(filter, predictor, sizeof(struct PDStreamFilter));   // replace old us with predictor
                    filter->nextFilter = newSelf;                               // set new us as predictor's next
                    predictor->options = NULL;                                  // clear OLD predictor's options or destroyed twice
                    PDStreamFilterDestroy(predictor);
                    return (*filter->init)(filter);                             // init predictor, not us
                }
                break;
            }
            iter = iter->prev->prev;
        }
    }
    
    z_stream *stream = filter->data = calloc(1, sizeof(z_stream));
    
    stream->zalloc = Z_NULL;
    stream->zfree = Z_NULL;
    stream->opaque = Z_NULL;
    
    if (Z_OK != deflateInit(stream, 5)) {
        free(stream);
        return false;
    }
    
    filter->initialized = true;
    
    return true;
}

PDInteger fd_decompress_init(PDStreamFilterRef filter)
{
    if (filter->initialized)
        return true;
    
    if (filter->options) {
        PDStackRef iter = filter->options;
        while (iter) {
            if (!strcmp(iter->info, "Predictor")) {
                // we need a predictor as well
                filter->nextFilter = PDStreamFilterObtain(iter->info, true, filter->options);
                if (filter->nextFilter) {
                    // untie this from ourselves or we will destroy it twice
                    filter->options = NULL;
                }
                break;
            }
            iter = iter->prev->prev;
        }
    }
    
    z_stream *stream = filter->data = calloc(1, sizeof(z_stream));
    
    stream->zalloc = Z_NULL;
    stream->zfree = Z_NULL;
    stream->opaque = Z_NULL;
    stream->avail_in = 0;
    stream->next_in = Z_NULL;
    
    if (Z_OK != inflateInit(stream)) {
        free(stream);
        return false;
    }
    
    filter->initialized = true;
    
    return true;
}

PDInteger fd_compress_done(PDStreamFilterRef filter)
{
    PDAssert(filter->initialized);
    
    z_stream *stream = filter->data;
    
    deflateEnd(stream);
    free(stream);
    
    filter->initialized = false;
    
    return true;
}

PDInteger fd_decompress_done(PDStreamFilterRef filter)
{
    PDAssert(filter->initialized);
    
    z_stream *stream = filter->data;
    
    inflateEnd(stream);
    free(stream);
    
    filter->initialized = false;
    
    return true;
}

PDInteger fd_compress_proceed(PDStreamFilterRef filter)
{
    PDInteger outputLength;
    int ret;

    z_stream *stream = filter->data;

    stream->avail_out = filter->bufOutCapacity;
    stream->next_out = filter->bufOut;
    
    // we flush stream, if we have no more input
    int flush = filter->hasInput ? Z_NO_FLUSH : Z_FINISH;
    ret = deflate(stream, flush);
    if (ret < 0) { PDWarn("deflate error: %s\n", stream->msg); }
    filter->finished = ret == Z_STREAM_END;
    assert (ret != Z_STREAM_ERROR); // crash = screwed up setup
    assert (ret != Z_BUF_ERROR);    // crash = buffer was trashed
    filter->failing = ret < 0;
    
    filter->bufInAvailable = stream->avail_in;
    outputLength = filter->bufOutCapacity - stream->avail_out;
    filter->bufOut += outputLength;
    
    filter->needsInput = 0 == stream->avail_in;
    filter->bufOutCapacity = stream->avail_out;
    
    return outputLength;
}

PDInteger fd_decompress_proceed(PDStreamFilterRef filter)
{
    PDInteger outputLength;
    int ret;
    
    z_stream *stream = filter->data;
    
    stream->avail_out = filter->bufOutCapacity;
    stream->next_out = filter->bufOut;
    
    ret = inflate(stream, Z_NO_FLUSH);
    if (ret < 0) { PDWarn("inflate error: %s\n", stream->msg); }
    filter->finished = ret == Z_STREAM_END;
    assert (ret != Z_STREAM_ERROR); // crash = screwed up setup
    assert (ret != Z_BUF_ERROR);    // crash = buffer was trashed
    switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            inflateEnd(stream);
            filter->failing = true;
            return 0;
    }
    
    filter->bufInAvailable = stream->avail_in;
    outputLength = filter->bufOutCapacity - stream->avail_out;
    filter->bufOut += outputLength;
    
    filter->needsInput = 0 == stream->avail_in;
    filter->bufOutCapacity = stream->avail_out;

    return outputLength;
}

PDInteger fd_compress_process(PDStreamFilterRef filter)
{
    z_stream *stream = filter->data;
    
    stream->avail_in = filter->bufInAvailable;
    stream->next_in = filter->bufIn;
    
    return fd_compress_proceed(filter);
}

PDInteger fd_decompress_process(PDStreamFilterRef filter)
{
    z_stream *stream = filter->data;
    
    stream->avail_in = filter->bufInAvailable;
    stream->next_in = filter->bufIn;

    return fd_decompress_proceed(filter);
}

PDStreamFilterRef fd_compress_invert(PDStreamFilterRef filter)
{
    return PDStreamFilterFlateDecodeDecompressCreate(NULL);
}

PDStreamFilterRef fd_decompress_invert(PDStreamFilterRef filter)
{
    return PDStreamFilterFlateDecodeCompressCreate(NULL);
}

PDStreamFilterRef PDStreamFilterFlateDecodeCompressCreate(PDStackRef options)
{
    return PDStreamFilterCreate(fd_compress_init, fd_compress_done, fd_compress_process, fd_compress_proceed, fd_compress_invert, options);
}

PDStreamFilterRef PDStreamFilterFlateDecodeDecompressCreate(PDStackRef options)
{
    return PDStreamFilterCreate(fd_decompress_init, fd_decompress_done, fd_decompress_process, fd_decompress_proceed, fd_decompress_invert, options);
}

PDStreamFilterRef PDStreamFilterFlateDecodeConstructor(PDBool inputEnd, PDStackRef options)
{
    return (inputEnd
            ? PDStreamFilterFlateDecodeDecompressCreate(options)
            : PDStreamFilterFlateDecodeCompressCreate(options));
}

#endif // PD_SUPPORT_ZLIB
