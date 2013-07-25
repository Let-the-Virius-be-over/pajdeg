//
//  PDDefines.h
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
 @file PDDefines.h
 @brief Definitions for Pajdeg
 */

#ifndef INCLUDED_PDDefines_h
#define INCLUDED_PDDefines_h

// The DEBUG directive turns on all assertions and warnings. It is recommended when writing or testing but not for production code.
// #define DEBUG

// The PD_WARNINGS directive turns on printing of warnings to stderr. Enabled if DEBUG is defined.
//#define PD_WARNINGS

// The PD_ASSERTS directive turns on assertions. If something is misbehaving, turning this on is a good idea
// as it provides a crash point and/or clue as to what's not going right. Enabled if DEBUG is defined.
//#define PD_ASSERTS

// PD_DEBUG_TWINSTREAM_ASSERT_OBJECTS enables reassertions of every single object inserted into the output
// PDF, by seeking back to its supposed position (XREF-wise) and reading in the <num> <num> obj part.
//#define PD_DEBUG_TWINSTREAM_ASSERT_OBJECTS

// DEBUG_PARSER_PRINT_XREFS prints out the resulting XREF table to stdout on setup.
//#define DEBUG_PARSER_PRINT_XREFS

// DEBUG_PARSER_CHECK_XREFS checks every single object (that is 'in use') against the input PDF, by
// seeking to the specified offset, reading in a chunk of data, and comparing said data to the
// expected object. Needless to say, expensive, but excellent starting point to determine if a PDF
// is broken or not (XREF table tends to break "first").
//#define DEBUG_PARSER_CHECK_XREFS

/**
 @defgroup CORE_GRP Core
 @brief Internal type definitions.
 
 @{
 */

    /**
     PDF integer type in Pajdeg.
     */
    typedef long int             PDInteger;

    /**
     PDF real type in Pajdeg.
     */
    typedef float                PDReal;

    /**
     PDF object identifier type in Pajdeg. Together with a generation number, an object ID uniquely identifies an object in a PDF.
     */
    typedef PDInteger            PDObjectID;

    /**
     PDF boolean identifier.
     */
    #ifdef bool
    typedef bool                 PDBool;
    #else
    typedef unsigned char        PDBool;
    #endif

    /**
     Size type (unsigned).
     */
    typedef unsigned long long   PDSize;

    /**
     Offset type (signed).
     */
    typedef long long            PDOffset;

/** @} */

/**
 @defgroup PAJDEG_GRP Pajdeg objects
 
 @{
 */

    /**
     @defgroup ALG_GRP Algorithm Based
     
     @{
     */

        /**
         @defgroup STACK_GRP Stack
         
         @{
         */

        /**
         @file PDStack.h
         */

        /**
         A simple stack implementation.
         
         The PDStack is tailored to handle some common types used in Pajdeg.
         */
        typedef struct PDStack      *PDStackRef;

        /** @} */

        /**
         @defgroup STATICHASH_GRP Static Hash
         
         @{
         */

        /**
         @file PDStaticHash.h
         */

        /**
         A (very) simple hash table implementation.
         
         Limited to predefined set of primitive keys on creation. O(1) but triggers false positives in some instances.
         */
        typedef struct PDStaticHash *PDStaticHashRef;

        /** @} */

        /**
         @defgroup BTREE_GRP Binary Tree
         
         @{
         */

        /**
         @file PDBTree.h
         @brief A simple binary tree implementation.
         */

        /**
         A binary tree implementation.
         */
        typedef struct PDBTree      *PDBTreeRef;

        /** @} */

        /**
         Deallocation signature. 
         
         When encountered, the default is usually the built-in free() method, unless otherwise stated.
         */
        typedef void (*PDDeallocator)(void *ob);

    /** @} */

    /**
     @defgroup OBJECT_GRP Objects
     
     @{
     */


    /**
     @file PDObject.h
     
     PDF object.
     */

    /**
     A PDF object.
     */
    typedef struct PDObject     *PDObjectRef;

    /**
     The type of object.
     
     @note This enum is matched with CGPDFObject's type enum (Core Graphics)
     @warning At the moment, PDObjectTypeString and PDObjectTypeDictionary are the only supported values.
     */
    typedef enum {
        PDObjectTypeUnknown     = 1,    ///< The type of the object has not (yet) been determined
        PDObjectTypeBoolean,            ///< A boolean.
        PDObjectTypeInteger,            ///< An integer.
        PDObjectTypeReal,               ///< A real (internally represented as a float).
        PDObjectTypeName,               ///< A name. Names in PDFs are things that begin with a slash, e.g. /Info.
        PDObjectTypeString,             ///< A string.
        PDObjectTypeArray,              ///< An array.
        PDObjectTypeDictionary,         ///< A dictionary. Most objects are considered dictionaries.
        PDObjectTypeStream,             ///< A stream.
    } PDObjectType;

    /** @} */

    /**
     @defgroup REF_GRP References
     
     @{
     */

    /**
     @file PDReference.h
     
     PDF object reference.
     */

    /**
     A reference to a PDF object.
     */
    typedef struct PDReference  *PDReferenceRef;

    /** @} */

    /**
     @defgroup PIPE_GRP Pipes
     
     @{
     */


        /**
         @file PDPipe.h
         @brief A Pajdeg pipe, for mutating PDF files.
         */

        /**
         A pipe.
         */
        typedef struct PDPipe       *PDPipeRef;

        /**
         @defgroup TASK_GRP Tasks
         
         @{
         */

        /**
         @file PDTask.h
         */

        /**
         A task.
         */
        typedef struct PDTask       *PDTaskRef;

        /**
         Task filter property type.
         
         @todo Implement PDPropertyPage support.
         @todo Resolve issue related to unfiltered tasks.
         */
        typedef enum {
            PDPropertyObjectId      = 1,///< value = object ID; only called for the live object, even if multiple copies exist
            PDPropertyRootObject,       ///< triggered when root object is encountered
            PDPropertyInfoObject,       ///< triggered when info object is encountered
            
            // *** planned ***
            //PDPropertyPage,             ///< value = number; if 0, triggers for every page
        } PDPropertyType;

        /**
         Task result type. 
         
         Tasks are chained together. When triggered, the first task in the list of tasks for the specific state is executed. Before moving on to the next task in the list, each task has a number of options specified in the task result.
         
         - A task has the option of canceling the entire PDF creation process due to some serious issue, via PDTaskFailure. 
         - It may also "filter" out its child tasks dynamically by returning PDTaskSkipRest for given conditions. This will stop the iterator, and any tasks remaining in the queue will be skipped. 
         
         Returning PDTaskDone means the task ended normally, and that the next task, if any, may execute. This is usually the preferred return value.
         */
        typedef enum {
            PDTaskFailure   = -1,       ///< the entire pipe operation is terminated and the destination file is left unfinished
            PDTaskDone      =  0,       ///< the task ended as normal
            PDTaskSkipRest  =  1,       ///< the task ended, and requires that remaining tasks in its pipeline are skipped
        } PDTaskResult;

        /**
         Function signature for task callbacks. 
         */
        typedef PDTaskResult (*PDTaskFunc)(PDPipeRef pipe, PDTaskRef task, PDObjectRef object);

        /** @} */

        /**
         @defgroup PARSER_GRP Parsers
         
         @{
         */

        /**
         @file PDParser.h
         
         The Pajdeg parser.
         */

        /**
         A parser.
         */
        typedef struct PDParser     *PDParserRef;

        /** @} */

        /**
         @defgroup STREAM_GRP Streams
         
         @{
         */

        /**
         @file PDTwinStream.h
         */

        /**
         A double-edged stream.
         */
        typedef struct PDTwinStream *PDTwinStreamRef;

        /**
         The twin stream has three methods available for reading data: read/write, random access, and reversed. 
         
         - Read/write is the default method, and is the only method allowed for writing to the output stream.
         - Random access turns the stream temporarily into a regular file reader, permitting random access to the input file. This mode is used when collecting XREF tables on parser initialization.
         - Reverse turns the stream inside out, in the sense that the heap is filled from the bottom, and buffer fills begin at the end and iterate back toward the beginning.
         */
        typedef enum {
            PDTwinStreamReadWrite,      ///< reads and writes from start to end of input into output, skipping/replacing/inserting as appropriate
            PDTwinStreamRandomAccess,   ///< random access, jumping to positions in the input, not writing anything to output
            PDTwinStreamReversed,       ///< reads from the end of the input file, filling the heap from the end up towards the beginning
        } PDTwinStreamMethod;

        /** @} */

    /** @} */

    /**
     @defgroup SCANNER_GRP Scanners
     
     @{
     */

        /**
         @defgroup STATE_GRP States
         
         @{
         */

        /**
         @file PDState.h
         */

        /**
         A state. 
         
         A state in Pajdeg is a definition of a given set of conditions.
         
         @see PDPortableDocumentFormatState.h
         */
        typedef struct PDState      *PDStateRef;

        /** @} */

        /**
         @defgroup ENV_GRP Environments
         
         @{
         */

        /**
         @file PDEnv.h
         
         State instance.
         */

        /**
         An environment. 
         
         Environments are instances of states. 
         */
        typedef struct PDEnv        *PDEnvRef;

        /** @} */

        /**
         @defgroup OPERATOR_GRP Operators
         
         @{
         */

        /**
         @file PDOperator.h
         
         Operator.
         */

        /**
         An operator. 
         */
        typedef struct PDOperator   *PDOperatorRef;

        /**
         Operator type.
         
         Pajdeg's internal parser is configured using a set of states and operators. Operators are defined using a PDOperatorType, and an appropriate unioned argument.
         
         @see PDPortableDocumentFormatState.h
         */
        typedef enum {
            PDOperatorPushState = 1,    ///< pushes another state; e.g. "<" pushes dict_hex, which on seeing "<" pushes "dict"
            PDOperatorPushWeakState,    ///< identical to above, but does not 'retain' the target state, to prevent retain cycles (e.g. arb -> array -> arb -> ...)
            PDOperatorPopState,         ///< pops back to previous state
            PDOperatorPopVariable,      ///< pops entry off of results stack and stores as an attribute of a future complex result
            PDOperatorPopValue,         ///< pops entry off of results stack and stores in variable stack, without including a variable name
            PDOperatorPushResult,       ///< pushes current symbol onto results stack
            PDOperatorAppendResult,     ///< appends current symbol to last result on stack
            PDOperatorPushContent,      ///< pushes everything from the point where the state was entered to current offset to results
            PDOperatorPushComplex,      ///< pushes object of type description `key' with variables (if any) as attributes onto results stack
            PDOperatorStoveComplex,     ///< pushes a complex onto build stack rather than results stack (for popping as a chunk of objects later)
            PDOperatorPullBuildVariable,///< takes build stack as is, and stores it as if it was a popped variable
            PDOperatorPushbackSymbol,   ///< pushes the scanned symbol back onto the symbols stack, so that it is re-read the next time a symbol is popped from the scanner
            PDOperatorPushbackValue,    ///< pushes the top value on the stack onto the symbols stack as if it were never read
            PDOperatorPopLine,          ///< read to end of line
            PDOperatorReadToDelimiter,  ///< read over symbols and whitespace until a delimiter is encountered
            PDOperatorNOP,              ///< do nothing
            // debugging
            PDOperatorBreak,            ///< break (presuming breakpoint is properly placed)
        } PDOperatorType;

        /** @} */

        /**
         @file PDScanner.h
         */

        /**
         A scanner.
         */
        typedef struct PDScanner    *PDScannerRef;

        /**
         Called whenever the scanner needs more data.
         
         The scanner's buffer function requires that the buffer is intact in the sense that the content in the range 0..*size (on call) remains intact and in the same position relative to *buf; req may be set if the scanner has an idea of how much data it needs, but is most often 0.
         */
        typedef void(*PDScannerBufFunc)(void *info, PDScannerRef scanner, char **buf, int *size, int req);

        /**
         Pop function signature. 
         
         The pop function is used to lex symbols out of the buffer, potentially requesting more data via the supplied PDScannerBufFunc.
         
         It defaults to a simple read-forward but can be swapped for special purpose readers on demand. It is internally swapped for the reverse lexer when the initial "startxref" value is located.
         */
        typedef void(*PDScannerPopFunc)(PDScannerRef scanner);

    /** @} */

/** @} */

#endif
