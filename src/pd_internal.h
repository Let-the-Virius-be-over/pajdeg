//
//  pd_internal.h
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
 @file pd_internal.h
 @brief Internal definitions for Pajdeg.
 
 @ingroup PDDEV
 
 @defgroup PDDEV Pajdeg Development
 
 Only interesting to people intending to improve or expand Pajdeg.
 */

#ifndef INCLUDED_pd_internal_h
#define INCLUDED_pd_internal_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PDDefines.h"

/**
 @def true 
 The truth value. 
 
 @def false
 The false value.
 */
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

/// @name Construction

/**
 Create PDObjectRef with given ID and generation number. 
 
 @note Normally this method is not called directly, but instead PDParserRef's PDParserCreateNewObject() is used.
 
 @param obid Object ID
 @param genid Generation number
 @return A detached object.
 */
extern PDObjectRef PDObjectCreate(PDInteger obid, PDInteger genid);

/// @name Private structs

/**
 The length of PDType, in pointers.
 */
#ifdef DEBUG_PDTYPES
#define PDTYPE_PTR_LEN  3
#else
#define PDTYPE_PTR_LEN  2
#endif

/*
 @cond IGNORE
 
 The PDType union. 
 
 It is force-aligned to the given size using PDTYPE_PTR_LEN.
 
 @todo Determine: Is the align done correctly? Is it even necessary?
 
 (Note: this is purposefully not doxygenified as Doxygen bugs out over the layout.)
 */
union PDType {
    struct {
#ifdef DEBUG_PDTYPES
        char *pdc;                  // Pajdeg signature
#endif
        PDInteger retainCount;      // Retain count. If the retain count of an object hits zero, the object is disposed of.
        PDDeallocator dealloc;      // Deallocation method.
    };
    void *align[PDTYPE_PTR_LEN];    // Force-align. 
};

/** @endcond // IGNORE */

/**
 Allocate a new PDType object, with given size and deallocator.
 
 @param size Size of object.
 @param dealloc Dealloc method as a (*)(void*).
 @param zeroed Whether the allocated block should be zeroed or not. If true, calloc is used to allocate the memory.
 */
extern void *PDAlloc(PDSize size, void *dealloc, PDBool zeroed);

/**
 Flush autoreleased pool.
 */
extern void  PDFlush(void);

/**
 Identifier for PD type checking.
 */
extern char *PDC;

/**
 A macro for asserting that an object is a proper PDType.
 */
#ifdef DEBUG_PDTYPES
#define PDTYPE_ASSERT(ob) PDAssert(((PDTypeRef)ob - 1)->pdc == PDC)
#else
#define PDTYPE_ASSERT(ob) 
#endif

/**
 Object internal structure
 
 @ingroup PDOBJECT
 */

struct PDObject {
    PDInteger           obid;           ///< object id
    PDInteger           genid;          ///< generation id
    PDObjectClass       obclass;        ///< object class (regular, compressed, or trailer)
    PDObjectType        type;           ///< data structure of def below
    void               *def;            ///< the object content
    PDBool              hasStream;      ///< if set, object has a stream
    PDInteger           streamLen;      ///< length of stream (if one exists)
    PDInteger           extractedLen;   ///< length of extracted stream; -1 until stream has been fetched via the parser
    char               *streamBuf;      ///< the stream, if fetched via parser, otherwise an undefined value
    PDBool              skipStream;     ///< if set, even if an object has a stream, the stream (including keywords) is skipped when written to output
    PDBool              skipObject;     ///< if set, entire object is discarded
    pd_stack            mutations;      ///< key/value stack of alterations to do when writing the object
    char               *ovrStream;      ///< stream override
    PDInteger           ovrStreamLen;   ///< length of ^
    PDBool              ovrStreamAlloc; ///< if set, ovrStream will be free()d by the object after use
    char               *ovrDef;         ///< definition override
    PDInteger           ovrDefLen;      ///< take a wild guess
    PDBool              encryptedDoc;   ///< if set, the object is contained in an encrypted PDF; if false, PDObjectSetStreamEncrypted is NOP
    char               *refString;      ///< reference string, cached from calls to 
};

//
// object stream
//

typedef struct PDObjectStreamElement *PDObjectStreamElementRef;

/**
 @addtogroup PDOBJECTSTREAM

 @{ 
 */

/**
 Object stream element structure
 
 This is a wrapper around an object inside of an object stream.
 */
struct PDObjectStreamElement {
    PDInteger obid;                     ///< object id of element
    PDInteger offset;                   ///< offset inside object stream
    PDInteger length;                   ///< length of the (stringified) definition; only valid during a commit
    PDObjectType type;                  ///< element object type
    void *def;                          ///< definition; NULL if a construct has been made for this element
};

/**
 Object stream internal structure
 
 The object stream is an object in a PDF which itself contains a stream of objects in a specially formatted form.
 */
struct PDObjectStream {
    PDObjectRef ob;                     ///< obstream object
    PDInteger n;                        ///< number of objects
    PDInteger first;                    ///< first object's offset
    PDStreamFilterRef filter;           ///< filter used to extract the initial raw content
    PDObjectStreamElementRef elements;  ///< n sized array of elements (non-pointered!)
    pd_btree constructs;                ///< instances of objects (i.e. constructs)
};

/**
 Createa an object stream with the given object.
 
 @param object Object whose stream is an object stream.
 */
extern PDObjectStreamRef PDObjectStreamCreateWithObject(PDObjectRef object);

/**
 Parse the raw object stream rawBuf and set up the object stream structure.
 
 If the object has a defined filter, the object stream decodes the content before parsing it.
 
 @param obstm The object stream.
 @param rawBuf The raw buffer.
 */
extern void PDObjectStreamParseRawObjectStream(PDObjectStreamRef obstm, char *rawBuf);

/**
 Parse the extracted object stream and set up the object stream structure.
 
 This is identical to PDObjectStreamParseRawObjectStream except that this method presumes that decoding has been done, if necessary.
 
 @param obstm The object stream.
 @param buf The buffer.
 */
extern void PDObjectStreamParseExtractedObjectStream(PDObjectStreamRef obstm, char *buf);

/**
 Commit an object stream to its associated object. 
 
 If changes to an object in an object stream are made, they are not automatically reflected. 
 */
extern void PDObjectStreamCommit(PDObjectStreamRef obstm);

/** @} */

/// @name Environment

/**
 PDState wrapping structure
 */
struct pd_env {
    PDStateRef    state;            ///< The wrapped state.
    pd_stack      buildStack;       ///< Build stack (for sub-components)
    pd_stack      varStack;         ///< Variable stack (for incomplete components)
    //PDInteger     entryOffset;     
};

/**
 Destroy an environment
 
 @param env The environment
 */
extern void pd_env_destroy(pd_env env);

/// @name Binary tree

/**
 Binary tree structure
 */
struct pd_btree {
    PDInteger key;                  ///< The (primitive) key.
    void *value;                    ///< The value.
    //PDInteger balance;              
    pd_btree branch[2];             ///< The left and right branches of the tree.
};

/// @name Operator

/**
 The PDperator internal structure
 */
struct PDOperator {
    PDOperatorType   type;          ///< The operator type

    union {
        PDStateRef   pushedState;   ///< for "PushNewEnv", this is the environment being pushed
        char        *key;           ///< the argument to the operator, for PopVariable, Push/StoveComplex, PullBuildVariable
        PDID         identifier;    ///< identifier (constant string pointer pointer)
    };
    
    PDOperatorRef    next;          ///< the next operator, if any
};

/**
 PDXTable
 
 @ingroup PDXTABLE
 */
typedef struct PDXTable *PDXTableRef;

/**
 The state of a PDParser instance. 
 */
typedef enum {
    PDParserStateBase,              ///< parser is in between objects
    PDParserStateObjectDefinition,  ///< parser is right after 1 2 obj and right before whatever the object consists of
    PDParserStateObjectAppendix,    ///< parser is right after the object's content, and expects to see endobj or stream next
    PDParserStateObjectPostStream,  ///< parser is right after the endstream keyword, at the endobj keyword
} PDParserState;

/**
 The PDParser internal structure.
 */
struct PDParser {
    PDTwinStreamRef stream;         ///< The I/O stream from the pipe
    PDScannerRef scanner;           ///< The main scanner
    PDParserState state;            ///< The parser state
    
    // xref related
    pd_stack xstack;                ///< A stack of partial xref tables based on offset; see [1] below
    PDXTableRef mxt;                ///< master xref table, used for output
    PDXTableRef cxt;                ///< current input xref table
    PDBool done;                    ///< parser has passed the last object in the input PDF
    size_t xrefnewiter;             ///< iterator for locating unused id's for usage in master xref table
    
    // object related
    pd_stack appends;               ///< stack of objects that are meant to be appended at the end of the PDF
    PDObjectRef construct;          ///< cannot be relied on to contain anything; is used to hold constructed objects until iteration (at which point they're released)
    size_t streamLen;               ///< stream length of the current object
    size_t obid;                    ///< object ID of the current object
    size_t genid;                   ///< generation number of the current object
    size_t oboffset;                ///< offset of the current object
    
    // document-wide stuff
    PDObjectRef trailer;            ///< the trailer object
    PDObjectRef root;               ///< the root object, if instantiated
    PDObjectRef encrypt;            ///< the encrypt object, if instantiated
    PDReferenceRef rootRef;         ///< reference to the root object
    PDReferenceRef infoRef;         ///< reference to the info object
    PDReferenceRef encryptRef;      ///< reference to the encrypt object
    
    // miscellaneous
    PDBool success;                 ///< if true, the parser has so far succeeded at parsing the input file
    pd_btree skipT;                 ///< whenever an object is ignored due to offset discrepancy, its ID is put on the skip tree; when the last object has been parsed, if the skip tree is non-empty, the parser aborts, as it means objects were lost
};

/// @name Scanner

typedef struct PDScannerSymbol *PDScannerSymbolRef;

/**
 A scanner symbol.
 */
struct PDScannerSymbol {
    char         *sstart;       ///< symbol start
    short         shash;        ///< symbol hash (not normalized)
    PDInteger     slen;         ///< symbol length
    PDInteger     stype;        ///< symbol type
};

/**
 The internal scanner structure.
 */
struct PDScanner {
    pd_env   env;               ///< the current environment
    
    pd_stack envStack;          ///< environment stack; e.g. root -> arb -> array -> arb -> ...
    pd_stack resultStack;       ///< results stack
    pd_stack symbolStack;       ///< symbols stack; used to "rewind" when misinterpretations occur (e.g. for "number_or_obref" when one or two numbers)
    pd_stack garbageStack;      ///< temporary allocations; only used in operator function when a symbol is regenerated from a malloc()'d string
    
    PDStreamFilterRef filter;   ///< filter, if any
    
    char         *buf;          ///< buffer
    PDInteger     bresoffset;   ///< previously popped result's offset relative to buf
    PDInteger     bsize;        ///< buffer capacity
    PDInteger     boffset;      ///< buffer offset (we are at position &buf[boffset]
    PDInteger     bmark;        ///< buffer mark
    PDScannerSymbolRef sym;     ///< the latest symbol
    PDScannerPopFunc popFunc;   ///< the symbol pop function
    PDBool        fixedBuf;     ///< if set, the buffer is fixed (i.e. buffering function should not be called)
    PDBool        failed;       ///< if set, the scanner aborted due to a failure
};

/// @name Stack

#define pd_stack_STRING  0      ///< Stack string type
#define pd_stack_ID      1      ///< Stack identifier type
#define pd_stack_STACK   2      ///< Stack stack type
#define pd_stack_PDOB    3      ///< Stack object (PDTypeRef managed) type
#define pd_stack_FREEABL 4      ///< Stack freeable type

/**
 The internal stack structure
 */
struct pd_stack {
    pd_stack   prev;            ///< Previous object in stack
    char       type;            ///< Stack type
    void      *info;            ///< The stack content, based on its type
};

/// @name State

/**
 The internal PDState structure
 */
struct PDState {
    PDBool         iterates;    ///< if true, scanner will stop while in this state, after reading one entry
    char          *name;        ///< name of the state
    char         **symbol;      ///< symbol strings
    PDInteger      symbols;     ///< number of symbols in total
    
    PDInteger     *symindex;    ///< symbol indices (for hash)
    short          symindices;  ///< number of index slots in total (not = `symbols`, often bigger)
    
    PDOperatorRef *symbolOp;    ///< symbol operators
    PDOperatorRef  numberOp;    ///< number operator
    PDOperatorRef  delimiterOp; ///< delimiter operator
    PDOperatorRef  fallbackOp;  ///< fallback operator
};

/// @name Static Hash

/**
 The internal static hash structure
 */
struct PDStaticHash {
    PDInteger entries;          ///< Number of entries in static hash
    PDInteger mask;             ///< The mask
    PDInteger shift;            ///< The shift
    PDBool    leaveKeys;        ///< if set, the keys are not deallocated on destruction; default = false (i.e. dealloc keys)
    PDBool    leaveValues;      ///< if set, the values are not deallocated on destruction; default = false (i.e. dealloc values)
    void    **keys;             ///< Keys array
    void    **values;           ///< Values array
    void    **table;            ///< The static hash table
};

/// @name Tasks

/**
 The internal task structure
 */
struct PDTask {
    PDBool          isFilter;       ///< Whether task is a filter or not. Internally, a task is only a filter if it is assigned to a specific object ID or IDs.
    PDPropertyType  propertyType;   ///< The filter property type
    PDInteger       value;          ///< The filter value, if any
    PDTaskFunc      func;           ///< The function callback, if the task is not a filter
    PDTaskRef       child;          ///< The task's child task; child tasks are called in order.
    PDDeallocator   deallocator;    ///< The deallocator for the task.
    void           *info;           ///< The (user) info object.
};

/// @name Twin streams

/**
 The internal twin stream structure
 */
struct PDTwinStream {
    PDTwinStreamMethod method;      ///< The current method
    PDScannerRef scanner;           ///< the master scanner
    
    FILE    *fi;                    ///< Reader
    FILE    *fo;                    ///< writer
    fpos_t   offsi;                 ///< absolute offset in input for heap
    fpos_t   offso;                 ///< absolute offset in output for file pointer
    
    char    *heap;                  ///< heap in which buffer is located
    PDSize   size;                  ///< size of heap
    PDSize   holds;                 ///< bytes in heap
    PDSize   cursor;                ///< position in heap (bytes 0..cursor have been written (unless discarded) to output)
    
    char    *sidebuf;               ///< temporary buffer (e.g. for Fetch)
    
    PDBool   outgrown;              ///< if true, a buffer with growth disallowed attempted to grow and failed
};

/**
 Internal structure.
 
 @ingroup PDPIPE
 */
struct PDPipe {
    PDBool          opened;             ///< Whether pipe has been opened or not.
    PDBool          dynamicFiltering;   ///< Whether dynamic filtering is necessary; if set, the static hash filtering of filters is skipped and filters are checked for all objects.
    char           *pi;                 ///< The path of the input file.
    char           *po;                 ///< The path of the output file.
    FILE           *fi;                 ///< Reader
    FILE           *fo;                 ///< Writer
    PDInteger       filterCount;        ///< Number of filters in the pipe
    PDTwinStreamRef stream;             ///< The pipe stream
    PDParserRef     parser;             ///< The parser
    pd_btree        filter;             ///< The filters, in a tree with the object ID as key
    pd_stack        unfilteredTasks;    ///< Tasks which run on every single object iterated (i.e. unfiltered).
};

/// @name Reference

/**
 Internal reference structure
 
 @ingroup PDREFERENCE
 */
struct PDReference {
    PDInteger obid;         ///< The object ID
    PDInteger genid;        ///< The generation number
};

/// @name Conversion (PDF specification)

typedef struct PDStringConv *PDStringConvRef;

/**
 Internal string conversion structure
 */
struct PDStringConv {
    char *allocBuf;         ///< The allocated buffer
    PDInteger offs;         ///< The current offset inside the buffer
    PDInteger left;         ///< The current bytes left (remaining) in the buffer
};

/// @name Macros / convenience

/**
 Pajdeg definition list.
 */
#define PDDEF const void*[]

/**
 Wrapper for null terminated definitions.
 */
#define PDDef(defs...) (PDDEF){(void*)defs, NULL}


/**
 @def PDWarn
 Print a warning to stderr, if user has turned on PD_WARNINGS.
 
 @param args Formatted variable argument list.
 */
#ifndef PDWarn
#   if defined(DEBUG) || defined(PD_WARNINGS)
#       define PD_WARNINGS
#       define PDWarn(args...) do { \
fprintf(stderr, "%s:%d - ", __FILE__,__LINE__); \
fprintf(stderr, args); \
} while (0)
#   else
#       define PDWarn(args...) 
#   endif
#endif

/**
 @def PDAssert
 Assert that expression is non-false. 
 
 If PD_WARNINGS is set, prints out the expression to stderr along with "assertion failure", then re-asserts expression using stdlib's assert()
 
 If PD_WARNINGS is unset, simply re-asserts expression using stdlib's assert().
 
 @param args Expression which must resolve to non-false (i.e. not 0, not nil, not NULL, etc).
 */
#ifndef PDAssert
#   if defined(DEBUG) || defined(PD_ASSERTS)
#       include <assert.h>
#       if defined(PD_WARNINGS)
#           define PDAssert(args...) \
if (!(args)) { \
PDWarn("assertion failure : %s", #args); \
assert(args); \
}
#       else
#           define PDAssert(args...) assert(args)
#       endif
#   else
#       define PDAssert(args...) 
#   endif
#endif

/**
 Macro for making casting of types a bit less of an eyesore. 
 
 as(PDInteger, stack->info)->prev is the same as ((PDInteger)(stack->info))->prev
 
 @param type The cast-to type
 @param expr The expression that should be cast
 */
#define as(type, expr...) ((type)(expr))

/**
 Perform assertions related to the twin stream's internal state.
 
 @param ts The twin stream.
 */
extern void PDTwinStreamAsserts(PDTwinStreamRef ts);

/**
 @def fmatox(x, ato)
 
 Fast mutative atoXXX inline function generation macro.
 
 @param x Function return type.
 @param ato Method
 */
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

#endif
