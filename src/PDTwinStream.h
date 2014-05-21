//
// PDTwinStream.h
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
/**
 @file PDTwinStream.h
 
 @ingroup PDTWINSTREAM

 @defgroup PDTWINSTREAM PDTwinStream

 @ingroup PDINTERNAL
 
 @brief The twin stream.
 
 The twin stream is similar to how a *NIX pipe works, except it provides functionality for jumping back and forth in the input stream while simultaneously streaming data through the scanner, parser, and into the output stream.
 
 Generally, this is done in the following steps, iterated over and over until the input ends:
 
 1. Request to grow some buffer comes in.
 2. Pull data from input stream onto heap and drop buffer inside heap.
 3. Request to discard/pass through content comes in from parser, at which point the master scanner position is adjusted. 
 
 Beyond this, there are also requests to insert content.
 
 @{
 */

#ifndef INCLUDED_PDTwinStream_h
#define INCLUDED_PDTwinStream_h

#include <stdio.h>
#include "PDDefines.h"

/// @name Construction

/**
 Create a new stream with the given file handlers.
 
 @param fi Input file handler.
 @param fo Output file handler.
 */
extern PDTwinStreamRef PDTwinStreamCreate(FILE *fi, FILE *fo);

/// @name Configuring / querying

/**
 Set the stream method for the given stream.
 
 @param ts The stream.
 @param method The PDTwinStreamMethod.
 
 @see PDTwinStreamMethod
 */
extern void PDTWinStreamSetMethod(PDTwinStreamRef ts, PDTwinStreamMethod method);

/**
 Get twin stream scanner.
 
 This is NULL until set up with a state.
 
 @param ts The stream.
 */
extern PDScannerRef PDTwinStreamGetScanner(PDTwinStreamRef ts);

/**
 Set up twin stream scanner with given state.
 
 It can be set multiple times. Previous instances are destroyed
 
 @param ts The stream.
 @param state The state.
 */
extern PDScannerRef PDTwinStreamSetupScannerWithState(PDTwinStreamRef ts, PDStateRef state);

/**
 Get the absolute input offset for the given stream.
 
 @param str Stream.
 */
#define PDTwinStreamGetInputOffset(str)  (str->offsi + str->cursor)

/**
 Get the absolute output offset for the given stream.
 
 @param str Stream.
 */
#define PDTwinStreamGetOutputOffset(str) (str->offso)

/// @name Reading 

/**
 Grow buffer (reallocating if necessary) to where it holds the requested amount.
 
 Fitted for PDScannerBufFunc

 Initial buffers are obtained by passing NULL as buffer with 0 as size

 @warning Passing a NULL buffer with non-zero size or vice versa is undefined
 @warning Behavior is undefined if PDTwinStreamReversed has been set
 
 @param ts PDTwinStreamRef instance
 @param scanner The scanner making the request.
 @param buf Pointer to buffer that should be grown.
 @param size Pointer to size variable that contains current size, and should be updated with new size after growth.
 @param req The requested amount of data. If 0, an optimal amount in terms of performance is selected.
 */
extern void PDTwinStreamGrowInputBuffer(void *ts, PDScannerRef scanner, char **buf, PDInteger *size, PDInteger req);

/**
 Grow buffer backwards, i.e. by prepending the buffer with previous content.

 @warning Behavior is undefined unless PDTwinStreamReversed has been set
 
 @param ts PDTwinStreamRef instance
 @param scanner The scanner making the request.
 @param buf Pointer to buffer that should be grown.
 @param size Pointer to size variable that contains current size, and should be updated with new size after growth.
 @param req The requested amount of data. If 0, an optimal amount in terms of performance is selected.
 */
extern void PDTwinStreamGrowInputBufferReversed(void *ts, PDScannerRef scanner, char **buf, PDInteger *size, PDInteger req);

/**
 Do not grow buffer. A call to this method will flag the outgrown flag.
 
 @param ts PDTwinStreamRef instance
 @param scanner The scanner making the request.
 @param buf Pointer to buffer that should be grown.
 @param size Pointer to size variable that contains current size, and should be updated with new size after growth.
 @param req The requested amount of data. If 0, an optimal amount in terms of performance is selected.
 */
extern void PDTwinStreamDisallowGrowth(void *ts, PDScannerRef scanner, char **buf, PDInteger *size, PDInteger req);

/**
 Jump to byte position in input file

 @warning behavior undefined unless PDTwinStreamRandomAccess has been set
 
 @param ts The stream.
 @param position The absolute position to seek to.
 */
extern void PDTwinStreamSeek(PDTwinStreamRef ts, PDSize position);

/**
 Iterate forward `bytes' bytes in input file
 @warning Behavior undefined unless PDTwinStreamRandomAccess has been set.

 @param ts The stream.
 @param bytes Number of bytes to advance in input stream.
 */
extern void PDTwinStreamAdvance(PDTwinStreamRef ts, PDSize bytes);

/**
 Temporarily jump to and read given amount from given offset in input, then immediately jump back to original position.
 
 @note Uses existing heap if position + size is within bounds. Extends heap if appropriate, otherwise seeks to and reads the content into buf directly.
 
 @note buf may become invalidated as soon as any of the other functions are used, but should be discarded using PDTwinStreamCutBranch() when no longer needed.
 
 @param ts The stream.
 @param position The absolute position to seek to.
 @param bytes Number of bytes to read
 @param buf Pointer to string that should be updated. The string must not be freed. Instead, PDTwinStreamCutBranch() should be used, or nothing done at all.
 @return The actual amount read (which may be lower, e.g. if EOF is hit).
 */
extern PDSize PDTwinStreamFetchBranch(PDTwinStreamRef ts, PDSize position, PDInteger bytes, char **buf);

/**
 Deallocate (if necessary) a fetched branch buffer.

 @param ts The stream.
 @param buf The buffer.
 */
extern void PDTwinStreamCutBranch(PDTwinStreamRef ts, char *buf);

/// @name Committing

// all commit operations are subject to heap realignment; any scanners except the master scanner (stream->scanner) must be discarded 
// method must be ReadWrite; this is seldom checked; behavior is undefined if this is not the case, and the resulting PDF will most certainly not be OK

/**
 Pass through content based on the master scanner's position in the stream.
 
 @param ts The stream.
 */
extern void PDTWinStreamPassthroughContent(PDTwinStreamRef ts);

/**
 Discard content by not writing it to output.
 
 @param ts The stream.
 */
extern void PDTwinStreamDiscardContent(PDTwinStreamRef ts);

/**
 Insert new content into output.

 @param ts The stream.
 @param bytes Number of bytes to write.
 @param content The content to write.
 */
extern void PDTwinStreamInsertContent(PDTwinStreamRef ts, PDSize bytes, const char *content);

/**
 Prune the stream.
 
 This is basically only used when updating objects, to preserve prefix content in scanner iteration.
 
 @param ts The stream.
 @param mark Comparison mark.
 */
extern void PDTwinStreamPrune(PDTwinStreamRef ts, PDOffset mark);

/**
 *  Create a new scanner for use in this stream.
 *
 *  @param ts    The stream
 *  @param state Root state
 *
 *  @return The new scanner, with is context set up to grow from this stream
 */
extern PDScannerRef PDTwinStreamCreateScanner(PDTwinStreamRef ts, PDStateRef state);

/// @name Debugging

#ifdef PD_DEBUG_TWINSTREAM_ASSERT_OBJECTS
/**
 Reassert data exists at given position in output file.
 
 @param ts The stream.
 @param offset The absolute offset.
 @param expect The expected string.
 @param len The length of the expected string.
 */
extern void PDTwinStreamReassert(PDTwinStreamRef ts, PDOffset offset, char *expect, PDInteger len);
#endif

#endif

/** @} */

/** @} */
