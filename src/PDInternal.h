//
//  PDInternal.h
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

#ifndef INCLUDED_PDInternal_h
#define INCLUDED_PDInternal_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PDDefines.h"

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

////////////////////////////////////////
//
// construction
//

extern PDObjectRef PDObjectCreate(PDInteger obid, PDInteger genid);

////////////////////////////////////////
//
// destruction
//

extern void PDParserDestroy(PDParserRef parser);

////////////////////////////////////////
//
// structs that should remain private
//

//
// type
//

#define PDTYPE_PTR_LEN  2
union PDType {
    struct {
        PDInteger retainCount;
        PDDeallocator dealloc;
    };
    void *align[PDTYPE_PTR_LEN];
};

extern void *PDAlloc(PDSize size, void *dealloc, PDBool zeroed);
extern void  PDFlush(void);

//
// object
//

struct PDObject {
    PDInteger           obid;           // object id
    PDInteger           genid;          // generation id
    PDObjectClass       obclass;        // object class (regular, compressed, or trailer)
    PDObjectType        type;           // data structure of def below
    void               *def;            // the object content
    PDBool              hasStream;      // if set, object has a stream
    PDInteger           streamLen;      // length of stream (if one exists)
    PDInteger           extractedLen;   // length of extracted stream; -1 until stream has been fetched via the parser
    char               *streamBuf;      // the stream, if fetched via parser, otherwise an undefined value
    PDBool              skipStream;     // if set, even if an object has a stream, the stream (including keywords) is skipped when written to output
    PDBool              skipObject;     // if set, entire object is discarded
    PDStackRef          mutations;      // key/value stack of alterations to do when writing the object
    char               *ovrStream;      // stream override
    PDInteger           ovrStreamLen;   // length of ^
    char               *ovrDef;         // definition override
    PDInteger           ovrDefLen;      // take a wild guess
    PDBool              encryptedDoc;   // if set, the object is contained in an encrypted PDF; if false, PDObjectSetStreamEncrypted is NOP
    char               *refString;      // reference string, cached from calls to 
};

//
// object stream
//

typedef struct PDObjectStreamElement *PDObjectStreamElementRef;
struct PDObjectStreamElement {
    PDInteger obid;                     // object id of element
    PDInteger offset;                   // offset inside object stream
    PDInteger length;                   // length of the (stringified) definition; only valid during a commit
    PDObjectType type;                  // element object type
    void *def;                          // definition; NULL if a construct has been made for this element
};

struct PDObjectStream {
    PDObjectRef ob;                     // obstream object
    PDInteger n;                        // number of objects
    PDInteger first;                    // first object's offset
    PDStreamFilterRef filter;           // filter used to extract the initial raw content
    PDObjectStreamElementRef elements;  // n sized array of elements (non-pointered!)
    pd_btree constructs;                // instances of objects (i.e. constructs)
};

extern PDObjectStreamRef PDObjectStreamCreateWithObject(PDObjectRef object);
extern void PDObjectStreamParseRawObjectStream(PDObjectStreamRef obstm, char *rawBuf);
extern void PDObjectStreamParseExtractedObjectStream(PDObjectStreamRef obstm, char *rawBuf);
extern void PDObjectStreamCommit(PDObjectStreamRef obstm);

//
// task
//

//extern PDTaskFunc PDPipeAppendFilter;

//
//  environment
//

struct PDEnv {
    PDStateRef    state;
    PDStackRef    buildStack;
    PDStackRef    varStack;
    PDOperatorRef op;
    PDInteger     entryOffset;
};

//
//  b-tree
//

struct pd_btree {
    PDInteger key;
    void *value;
    PDInteger balance;
    pd_btree branch[2];
};

//
//  operator
//

struct PDOperator {
    PDOperatorType   type;
    union {
        PDStateRef   pushedState; // for "PushNewEnv", this is the environment being pushed
        char        *key;         // the argument to the operator, for PopVariable, Push/StoveComplex, PullBuildVariable
        PDID         identifier;  // identifier (constant string pointer pointer)
    };
    
    PDOperatorRef    next;        // the next operator, if any
};

//
//  parser
//

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

typedef struct PDXTable *PDXTableRef;

typedef enum {
    PDParserStateBase,              // parser is in between objects
    PDParserStateObjectDefinition,  // parser is right after "1 2 obj" and right before whatever the object consists of
    PDParserStateObjectAppendix,    // parser is right after the object's content, and expects to see "endobj" or "stream" next
    PDParserStateObjectPostStream,  // parser is right after the "endstream" keyword, at the "endobj" keyword
} PDParserState;

struct PDParser {
    PDTwinStreamRef stream;
    PDScannerRef scanner;
    PDParserState state;
    
    // xref related
    PDStackRef xstack;  // a stack of partial xref tables based on offset; see [1] below
    PDXTableRef mxt;    // master xref table, used for output
    PDXTableRef cxt;    // current input xref table
    PDBool done;        // parser has passed the last object in the input PDF
    size_t xrefnewiter; // iterator for locating unused id's for usage in master xref table
    
    // object related
    PDStackRef appends; // stack of objects that are meant to be appended at the end of the PDF
    PDObjectRef construct; // cannot be relied on to contain anything; is used to hold constructed objects until iteration (at which point they're released)
    size_t streamLen;
    size_t obid;
    size_t genid;
    size_t oboffset;
    
    // document-wide stuff
    size_t size;
    PDObjectRef trailer;
    PDObjectRef root;
    PDObjectRef encrypt;
    PDReferenceRef rootRef;
    PDReferenceRef infoRef;
    PDReferenceRef encryptRef;
    
    // miscellaneous
    PDBool success;
    pd_btree skipT;   // whenever an object is ignored due to offset discrepancy, its ID is put on the skip tree; when the last object has been parsed, if the skip tree is non-empty, the parser aborts, as it means objects were lost
};

//
//  Scanner
//

typedef struct PDScannerSymbol *PDScannerSymbolRef;
struct PDScannerSymbol {
    char         *sstart;       // symbol start
    short         shash;        // symbol hash (not normalized)
    PDInteger     slen;         // symbol length
    PDInteger     stype;        // symbol type
};

struct PDScanner {
    PDEnvRef   env;
    
    PDStackRef envStack;        // environment stack; e.g. root -> arb -> array -> arb -> ...
    PDStackRef resultStack;     // results stack
    PDStackRef symbolStack;     // symbols stack; used to "rewind" when misinterpretations occur (e.g. for "number_or_obref" when one or two numbers)
    PDStackRef garbageStack;    // temporary allocations; only used in operator function when a symbol is regenerated from a malloc()'d string
    
    PDStreamFilterRef filter;   // filter, if any
    
    char         *buf;          // buffer
    PDInteger     bresoffset;   // previously popped result's offset relative to buf
    PDInteger     bsize;        // buffer capacity
    PDInteger     boffset;      // buffer offset (we are at position &buf[boffset]
    PDInteger     bmark;        // buffer mark
    PDScannerSymbolRef sym;     // the latest symbol
    PDScannerPopFunc popFunc;   // the symbol pop function
    PDBool        fixedBuf;     // if set, the buffer is fixed (i.e. buffering function should not be called)
    PDBool        failed;       // if set, the scanner aborted due to a failure
};

//
//  Stack
//

#define PDSTACK_STRING  0
#define PDSTACK_ID      1
#define PDSTACK_STACK   2
#define PDSTACK_PDOB    3
#define PDSTACK_FREEABL 4

struct PDStack {
    PDStackRef prev;
    char       type;
    void      *info;
};

//
//  State
//

struct PDState {
    PDBool         iterates;    // if true, scanner will stop while in this state, after reading one entry
    char          *name;        // name of the state
    char         **symbol;      // symbol strings
    PDInteger      symbols;     // # of symbols in total
    
    PDInteger     *symindex;    // symbol indices (for hash)
    short          symindices;  // # of index slots in total (not = `symbols', often bigger)
    
    PDOperatorRef *symbolOp;    // symbol operators
    PDOperatorRef  numberOp;    // number operator
    PDOperatorRef  delimiterOp; // delimiter operator
    PDOperatorRef  fallbackOp;  // fallback operator
};

//
// SH
//

struct PDStaticHash {
    PDInteger entries;
    PDInteger mask;
    PDInteger shift;
    PDBool leaveKeys;     // if set, the keys are not deallocated on destruction; default = false (i.e. dealloc keys)
    PDBool leaveValues;   // -'-
    void **keys;
    void **values;
    void **table;
};

//
//  Tasks
//

struct PDTask {
    PDBool          isFilter;
    PDPropertyType  propertyType;
    PDInteger       value;
    PDTaskFunc      func;
    PDTaskRef       child;
    PDDeallocator   deallocator;
    void           *info;
};

//
//  Stream
//

struct PDTwinStream {
    PDTwinStreamMethod method;
    PDScannerRef scanner;   // the master scanner
    
    FILE    *fi;            // reader
    FILE    *fo;            // writer
    fpos_t   offsi;         // absolute offset in input for heap
    fpos_t   offso;         // ao in output
    
    char    *heap;          // heap in which buffer is located
    PDSize   size;          // size of heap
    PDSize   holds;         // bytes in heap
    PDSize   cursor;        // position in heap (bytes 0..cursor have been written (unless discarded) to output)
    
    char    *sidebuf;       // temporary buffer (e.g. for Fetch)
    
    PDBool   outgrown;      // if true, a buffer with growth disallowed attempted to grow and failed
};

//
//  Pipe
//

struct PDPipe {
    PDBool          opened;
    PDBool          dynamicFiltering;
    char           *pi;
    char           *po;
    FILE           *fi;
    FILE           *fo;
    PDInteger       filterCount;
    PDTwinStreamRef stream;
    PDParserRef     parser;
    pd_btree      filter;
    PDStackRef      unfilteredTasks;
};

//
//  Reference
//

struct PDReference {
    PDInteger obid;
    PDInteger genid;
};




//
// conversion (PDF specification)
//

typedef struct PDStringConv *PDStringConvRef;
struct PDStringConv {
    char *allocBuf;
    PDInteger offs;
    PDInteger left;
};

////////////////////////////////////////
//
// macros / convenience
//

/**
 Pajdeg definition list.
 */
#define PDDEF const void*[]

/**
 Wrapper for null terminated definitions.
 */
#define PDDef(defs...) (PDDEF){(void*)defs, NULL}

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

#define as(type, expr...) ((type)(expr))

#define _pd_cyclic_exec(type, initial, next, exec_expr) \
    type t = initial; \
    while (initial) { \
        t = initial->next; \
        exec_expr; \
        initial = t; \
    }

#define _pd_cyclic_release(type, initial, next, free_expr) \
    _pd_cyclic_exec(type, initial, next, \
        free_expr; \
        free(initial)\
    )

extern void PDTwinStreamAsserts(PDTwinStreamRef ts);

#endif
