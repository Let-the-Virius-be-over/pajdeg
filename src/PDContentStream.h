//
// PDContentStream.h
//
// Copyright (c) 2014 Karl-Johan Alm (http://github.com/kallewoof)
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
 @file PDContentStream.h PDF content stream header file.
 
 @ingroup PDCONTENTSTREAM
 
 @defgroup PDCONTENTSTREAM PDContentStream
 
 @brief A content stream, i.e. a stream containing Adobe commands, such as for writing text and similar.
 
 @ingroup PDUSER
 
 Content streams may have a wide variety of purposes. One such purpose is the drawing of content on the page. 
 The content stream module contains support for working with the state machine to process the content in a
 variety of ways.
 
 The mode of operation goes as follows: 
 
 - there are two types of entries: arguments and operators
 - there is a stack of (space/line separated) arguments
 - there is a stack of current operators
 - when an argument is encountered, it's pushed onto the stack
 - when an operator is encountered, a defined number of arguments (based on operator name) are popped off the stack
 - some operators push onto or pop off of the current operators stack (BT pushes, ET pops, for example)
 
 At this point, no known example exists where the above complexity (in reference to arguments) is necessary. Instead, the following approximation is done:
 
 - put arguments into a list until the next operator is encountered
 - operator & list = the next operation
 
 Current operators stack is done exactly as defined.
 
 @{
 */

#ifndef INCLUDED_PDContentStream_h
#define INCLUDED_PDContentStream_h

#include <stdio.h>

#include "PDDefines.h"

/**
 *  A PDF content stream operation.
 *
 *  Content stream operations are simple pairs of operator names and their corresponding state stack, if any.
 *
 *  @ingroup PDCONTENTSTREAM
 */
struct PDContentStreamOperation {
    char    *name;  ///< name of operator
    pd_stack state; ///< state of operator; usually preserved values from its args
};

typedef struct PDContentStreamOperation *PDContentStreamOperationRef;

///! Basic operations

/**
 *  Set up a content stream based on an existing object and its (existing) stream.
 *
 *  @param object The object containing a stream.
 *
 *  @return The content stream object
 */
extern PDContentStreamRef PDContentStreamCreateWithObject(PDObjectRef object);

/**
 *  Attach an operator function to a given operator (replacing the current operator, if any).
 *
 *  @param cs       The content stream
 *  @param opname   The operator (e.g. "BT")
 *  @param op       The callback, which abides by the PDContentOperatorFunc signature
 *  @param userInfo User info value passed to the operator when called
 */
extern void PDContentStreamAttachOperator(PDContentStreamRef cs, const char *opname, PDContentOperatorFunc op, void *userInfo);

/**
 *  Attach a variable number of operator function pairs (opname, func, ...), each sharing the given user info object.
 *
 *  Pairs are provided using the PDDef() macro. The following code
 @code
    PDContentStreamAttachOperatorPairs(cs, ui, PDDef(
        "q",  myGfxStatePush,
        "Q",  myGfxStatePop,
        "BT", myBeginTextFunc, 
        "ET", myEndTextFunc
    ));
 @endcode
 *  is equivalent to
 @code
     PDContentStreamAttachOperator(cs, "q",  myGfxStatePush,  ui);
     PDContentStreamAttachOperator(cs, "Q",  myGfxStatePop,   ui);
     PDContentStreamAttachOperator(cs, "BT", myBeginTextFunc, ui);
     PDContentStreamAttachOperator(cs, "ET", myEndTextFunc,   ui);
 @endcode
 *
 *  @param cs       The content stream
 *  @param userInfo The shared user info object
 *  @param pairs    Pairs of operator name + operator callback
 */
extern void PDContentStreamAttachOperatorPairs(PDContentStreamRef cs, void *userInfo, const void **pairs);

/**
 *  Get the operator tree for the content stream. The operator tree is the representation of the operators
 *  in effect in the content stream. It is mutable, and updates to it, or to the content stream, will affect
 *  the original content stream.
 *
 *  @param cs The content stream
 *
 *  @return The operator tree
 */
extern PDBTreeRef PDContentStreamGetOperatorTree(PDContentStreamRef cs);

/**
 *  Replace the content stream's operator tree with the new tree (which may not be NULL). The content stream
 *  will use the new tree internally, thus making changes to and be affected by changes to the object.
 *
 *  @param cs           The content stream
 *  @param operatorTree The new operator tree
 */
extern void PDContentStreamSetOperatorTree(PDContentStreamRef cs, PDBTreeRef operatorTree);

/**
 *  Execute the content stream, i.e. parse the stream and call the operators as appropriate.
 *
 *  @param cs The content stream
 */
extern void PDContentStreamExecute(PDContentStreamRef cs);

/**
 *  Get the current operator stack from the content stream. 
 *
 *  The operator stack is a stack of PDContentStreamOperationRef objects; the values in the object can be obtained usign ob->name and ob->state.
 *
 *  @see PDContentStreamOperation
 *
 *  @param cs The content stream
 *
 *  @return Operator stack
 */
extern const pd_stack PDContentStreamGetOperators(PDContentStreamRef cs);

///! Advanced operations

/**
 *  Create a content stream configured to perform text search. 
 *
 *  @note Creating *one* text search content stream, and then using PDContentStreamGetOperatorTree and PDContentStreamSetOperatorTree to configure additional content streams is more performance efficient than creating a text search content stream for every content stream, when searching across multiple streams.
 *
 *  @param object       Object in which search should be performed
 *  @param searchString String to search for
 *  @param callback     Callback for matches
 *
 *  @return A pre-configured content stream
 */
extern PDContentStreamRef PDContentStreamCreateTextSearch(PDObjectRef object, const char *searchString, PDTextSearchOperatorFunc callback);

/**
 *  Create a content stream configured to print out every operation to the given file stream
 *
 *  This is purely for debugging Pajdeg and/or odd PDF content streams, or to learn what the various operators do and how they affect things.
 *
 *  @param object Object whose content stream should be printed
 *  @param stream The file stream to which printing should be made
 *
 *  @return A pre-configured content stream
 */
extern PDContentStreamRef PDContentStreamCreateStreamPrinter(PDObjectRef object, FILE *stream);

#endif

/** @} */

/** @} */
