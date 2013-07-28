//
//  PDStreamFilter.h
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
 @defgroup FILTER_GRP Stream filters
 
 @brief The stream filter interface.
 
 @{
 */

#ifndef INCLUDED_PDStreamFilter_h
#define INCLUDED_PDStreamFilter_h

#include "PDDefines.h"

/**
 Filter function signature.
 */
typedef PDInteger (*PDStreamFilterFunc)(PDStreamFilterRef filter);

/**
 Dual filter construction signature. 
 
 If inputEnd is set, the reader variant is returned, otherwise the writer variant is returned.
 */
typedef PDStreamFilterRef (*PDStreamDualFilterConstr)(PDBool inputEnd, PDStackRef options);

/**
 The stream filter struct.
 */
struct PDStreamFilter {
    PDBool initialized;                 ///< Determines if filter was set up or not. 
    PDStackRef options;                 ///< Filter options
    void *data;                         ///< User info object.
    unsigned char *bufIn;               ///< Input buffer.
    unsigned char *bufOut;              ///< Output buffer.
    PDInteger bufInAvailable;           ///< Available data.
    PDInteger bufOutCapacity;           ///< Output buffer capacity.
    PDStreamFilterFunc init;            ///< Initialization function. Called once before first use.
    PDStreamFilterFunc done;            ///< Deinitialization function. Called once after last use.
    PDStreamFilterFunc process;         ///< Processing function. Called any number of times, at most once per new input buffer.
    PDStreamFilterFunc proceed;         ///< Proceed function. Called any number of times to request more output from last process call.
    PDStreamFilterRef nextFilter;       ///< The next filter that should receive the output end of this filter as its input, or NULL if no such requirement exists.
};

/**
 Set up a stream filter with given callbacks.
 
 @param init The initializer. Returns 0 on failure, some other value on success.
 @param done The deinitializer. Returns 0 on failure, some other vaule on success.
 @param process The process function; called once when new data is put into bufIn. Returns # of bytes stored into output buffer.
 @param proceed The proceed function; called repeatedly after a process call was made, until the filter returns 0 lengths. Returns # of bytes stored into output buffer.
 */
extern PDStreamFilterRef PDStreamFilterCreate(PDStreamFilterFunc init, PDStreamFilterFunc done, PDStreamFilterFunc process, PDStreamFilterFunc proceed, PDStackRef options);

/**
 Destroy a stream filter.
 
 @note If initialized is true, the filter's done function will be called before deallocating.
 
 @param filter The filter.
 */
extern void PDStreamFilterDestroy(PDStreamFilterRef filter);

/**
 Register a dual filter with a given name.
 
 @param name The name of the filter
 @param filter The dual filter construction function.
 */
extern void PDStreamFilterRegisterDualFilter(const char *name, PDStreamDualFilterConstr constr);

/**
 Obtain a filter for given name and type, where the type is a boolean value for whether the filter should be a reader (i.e. decompress) or writer (i.e. compress).
 
 @param name The name of the filter.
 @param type true if filter should decompress, false if filter should compress
 @return A created PDStreamFilterRef or NULL if no match.
 */
extern PDStreamFilterRef PDStreamFilterObtain(const char *name, PDBool inputEnd, PDStackRef options);

/**
 Convert a PDScanner dictionary stack into a stream filter options stack.
 
 @note The original stack remains untouched.
 
 @param dictStack The dictionary stack to convert
 @return New allocated stack suited for stream filter options.
 */
extern PDStackRef PDStreamFilterCreateOptionsFromDictionaryStack(PDStackRef dictStack);

/**
 Convenience macro for init.
 */
#define PDStreamFilterInit(f) (*f->init)(f)

/**
 Convenience macro for done.
 */
#define PDStreamFilterDone(f) (*f->done)(f)

/**
 Convenience macro for process.
 */
#define PDStreamFilterProcess(f) (*f->process)(f)

/**
 Convenience macro for proceed.
 */
#define PDStreamFilterProceed(f) (*f->proceed)(f)

/**
 Convenience macro for setting buffers and sizes.
 */
#define PDStreamFilterPrepare(f, buf_in, len_in, buf_out, len_out) do { \
    f->bufIn = (unsigned char *)buf_in; \
    f->bufOut = (unsigned char *)buf_out; \
    f->bufInAvailable = len_in; \
    f->bufOutCapacity = len_out; \
} while (0)

#endif
