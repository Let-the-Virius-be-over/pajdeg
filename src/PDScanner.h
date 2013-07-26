//
//  PDScanner.h
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

/**
 @defgroup SCANNER_GRP Scanners
 
 The Pajdeg scanner takes a PDStateRef state and optionally a PDScannerPopFunc and allows the interpretation of symbols as defined by the state and its sub-states.
 
 The most public functions of the scanner are PDScannerPopString and PDScannerPopStack. The former attempts to retrieve a string from the input stream, and the other a PDStack. If the next value scanned is not the requested type, the function keeps the value around and returns falsity. It is not uncommon to attempt to pop a stack, and upon failure, to pop a string and behave accordingly.

 @{
 */

#ifndef INCLUDED_PDScanner_h
#define INCLUDED_PDScanner_h

#include <sys/types.h>
#include "PDDefines.h"

/// @name Creating / deleting scanners

/**
 Create a scanner using the default pop function.
 
 @param state The root state to use in the scanner.
 */
extern PDScannerRef PDScannerCreateWithState(PDStateRef state);

/**
 Create a scanner using the provided pop function.
 
 @param state The root state to use in the scanner.
 @param popFunc The pop function to use.
 */
extern PDScannerRef PDScannerCreateWithStateAndPopFunc(PDStateRef state, PDScannerPopFunc popFunc);

/**
 Destroy an existing scanner.
 
 @param scanner The scanner.
 */
extern void PDScannerDestroy(PDScannerRef scanner);

/// @name Using

/**
 Pop the next string. 
 
 @param scanner The scanner.
 @param value Pointer to string variable. Must be freed.
 @return true if the next value was a string.
 */
extern PDBool PDScannerPopString(PDScannerRef scanner, char **value);

/**
 Pop the next stack.
 
 @param scanner The scanner.
 @param value Pointer to stack ref. Must be freed.
 @return true if the next value was a stack.
 */
extern PDBool PDScannerPopStack(PDScannerRef scanner, PDStackRef *value);

/**
 Skip over a chunk of data internally.
 
 @note The stream is not iterated, only the scanner's internal buffer offset is.
 
 @param scanner The scanner.
 @param bytes The amount of bytes to skip.
 */
extern void PDScannerSkip(PDScannerRef scanner, PDSize bytes);

/**
 Require that the next result is a string, and that it is equal to the given value, or throw assertion.

 @param scanner The scanner.
 @param value Expected string.
 */
extern void PDScannerAssertString(PDScannerRef scanner, char *value);

// 
/**
 Require that the next result is a stack (the stack is discarded), or throw assertion.

 @param scanner The scanner.
 */
extern void PDScannerAssertStackType(PDScannerRef scanner);

/**
 Require that the next result is a complex of the given type, or throw assertion.

 @param scanner The scanner.
 @param identifier The identifier.
 */
extern void PDScannerAssertComplex(PDScannerRef scanner, const char *identifier);

// 
/**
 Read parts or entire stream at current position.
 
 Iterates scanner and stream (contrary to PDScannerSkip above, which only iterates scanner).
 
 @note dest must be capable of holding `bytes' bytes
 
 @param scanner The scanner.
 @param dest The destination buffer. Must be allocated, and be able to hold at least `bytes` bytes.
 @param bytes The number of bytes to read.
 */
extern PDInteger PDScannerReadStream(PDScannerRef scanner, char *dest, PDInteger bytes);

/// @name Adjusting scanner / source

/**
 Push a global scanner context.
 
 Global scanner contexts, consisting of an info pointer and a buffer function; can be pushed to provide a temporary context e.g. for off-reading.
 
 @note These are global. All instances of PDScannerRef will switch to using the pushed context and switch back when popped.
 
 @param ctxInfo The info object. Can be anything, but in Pajdeg it is always a PDTwinStreamRef. It is passed on to the buffer function when called.
 @param ctxBufFunc The buffer function.
 */
extern void PDScannerContextPush(void *ctxInfo, PDScannerBufFunc ctxBufFunc);

/**
 Pop global scanner context.
 
 @see PDScannerContextPush
 */
extern void PDScannerContextPop(void);

/**
 Set a cap on # of loops scanners make before considering a pop a failure.
 
 This is used when reading a PDF for the first time to not scan through the entire thing backwards looking for the startxref entry.
 
 The loop cap is reset after every successful pop.
 
 @param cap The cap.
 */
extern void PDScannerSetLoopCap(PDInteger cap);

/**
 Pop a symbol as normal, via forward reading of buffer.
 
 @param scanner The scanner.
 
 @see PDScannerCreateWithStateAndPopFunc
 */
extern void PDScannerPopSymbol(PDScannerRef scanner);

/**
 Pop a symbol reversedly, by iterating backward.

 @param scanner The scanner.
 
 @see PDScannerCreateWithStateAndPopFunc
 */
extern void PDScannerPopSymbolRev(PDScannerRef scanner);

/// @name Aligning, resetting, trimming scanner
//

/**
 Align buffer along with pointers with given offset (often negative).

 @param scanner The scanner.
 @param offset The offset.
 */
extern void PDScannerAlign(PDScannerRef scanner, PDOffset offset); 

/**
 Trim off of head from buffer (only used if buffer is a non-allocated pointer into a heap).
 
 @param scanner The scanner.
 @param bytes Bytes to trim off.
 */
extern void PDScannerTrim(PDScannerRef scanner, PDOffset bytes); 

/**
 Reset scanner buffer including size, offset, trail, etc., as well as discarding symbols and results.
 
 @param scanner The scanner.
 */
extern void PDScannerReset(PDScannerRef scanner);


/// @name Debugging

/**
 Print a trace of states to stdout for the scanner. 
 
 @param scanner The scanner.
 */
extern void PDScannerPrintStateTrace(PDScannerRef scanner);

#endif

/** @} */

/** @} */
