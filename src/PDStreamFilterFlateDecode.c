//
//  PDStreamFilterFlateDecode.c
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/26/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

#include "PDInternal.h"

#ifdef PD_SUPPORT_ZLIB

#include "PDStreamFilterFlateDecode.h"
#include "zlib.h"

PDInteger fd_compress_init(PDStreamFilterRef filter)
{
    PDAssert(! filter->initialized);
    
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
    PDAssert(! filter->initialized);
    
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
    
    // we always flush stream as we expect to get entire chunks passed always; this may change
    ret = deflate(stream, Z_FINISH);
    assert (ret != Z_STREAM_ERROR); // crash = screwed up setup
    
    outputLength = filter->bufOutCapacity - stream->avail_out;
    
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
    assert (ret != Z_STREAM_ERROR); // crash = screwed up setup
    switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            inflateEnd(stream);
            return -1;
    }
    
    outputLength = filter->bufOutCapacity - stream->avail_out;
    
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

PDStreamFilterRef PDStreamFilterFlateDecodeCompressCreate(void)
{
    return PDStreamFilterCreate(fd_compress_init, fd_compress_done, fd_compress_process, fd_compress_proceed);
}

PDStreamFilterRef PDStreamFilterFlateDecodeDecompressCreate(void)
{
    return PDStreamFilterCreate(fd_decompress_init, fd_decompress_done, fd_decompress_process, fd_decompress_proceed);
}

PDStreamFilterRef PDStreamFilterFlateDecodeConstructor(PDBool inputEnd)
{
    return (inputEnd
            ? PDStreamFilterFlateDecodeDecompressCreate()
            : PDStreamFilterFlateDecodeCompressCreate());
}

#endif // PD_SUPPORT_ZLIB
