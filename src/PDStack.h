//
//  PDStack.h
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
 @defgroup STACK_GRP Stack
 
 @brief Simple stack implementation tailored for Pajdeg's purposes. 
 
 The PDStackRef works like any other stack, except it has some amount of awareness about certain object types, such as PDEnvRef instances. 
 
 @{
 */

#ifndef INCLUDED_PDStack_h
#define INCLUDED_PDStack_h

#include <sys/types.h>
#include "PDDefines.h"

/**
 The global deallocator for stacks. Defaults to the built-in free() function, but is overridden when global preserve flag is set.
 
 @see PDStackSetGlobalPreserveFlag
 */
extern PDDeallocator PDStackDealloc;

/**
 Deallocate something using stack deallocator.
 */
#define PDDeallocateViaStackDealloc(ob) (*PDStackDealloc)(ob)

/**
 Globally turn on or off destructive operations in stacks
 
 @param preserve Whether preserve should be enabled or not.
 
 @note Nests truths.
 */
extern void PDStackSetGlobalPreserveFlag(PDBool preserve);

/// @name Pushing onto stack

/**
 Push a key (a string value) onto a stack.
 
 @param stack The stack.
 @param key The key. It is copied.
 */
extern void PDStackPushKey(PDStackRef *stack, char *key);

/**
 Push an identifier onto a stack.
 
 @param stack The stack.
 @param identifier The identifier. Can be anything. Never touched.
 */
extern void PDStackPushIdentifier(PDStackRef *stack, const char **identifier);

/**
 Push a freeable, arbitrary object. 
 
 @param stack The stack.
 @param info The object. If the stack is destroyed, the object is freed.
 */
extern void PDStackPushFreeable(PDStackRef *stack, void *info);

/**
 Push a stack onto the stack. 
 
 @note This is not an append operation; the pushed stack becomes a single entry that is a stack.
 
 @param stack The stack.
 @param pstack The stack to push.
 */
extern void PDStackPushStack(PDStackRef *stack, PDStackRef pstack);

/**
 Push a PDEnvRef object.
 
 @param stack The stack.
 @param env The env.
 */
extern void PDStackPushEnv(PDStackRef *stack, PDEnvRef env);

/**
 Unshift (put in from the start) a stack onto a stack.
 
 The difference between this and pushing a stack is demonstrated by the following:

 1. @code push    [a,b,c] onto [1,2,3] -> [1,2,3,[a,b,c]] @endcode
 2. @code unshift [a,b,c] onto [1,2,3] -> [[a,b,c],1,2,3] @endcode
 
 @note This is only here to deal with reversed dictionaries/arrays; support for unshifting was never intended.

 @param stack The stack.
 @param sstack The stack to unshift.
 */
extern void PDStackUnshiftStack(PDStackRef *stack, PDStackRef sstack);



/// @name Popping from stack



/**
 Destroy a stack (getting rid of every item according to its type).
 
 @param stack The stack.
 */
extern void PDStackDestroy(PDStackRef stack);

/**
 Pop a key off of the stack. Throws assertion if the next item is not a key.
 
 @param stack The stack.
 */
extern char *PDStackPopKey(PDStackRef *stack);

/**
 Pop an identifier off of the stack. Throws assertion if the next item is not an identifier.
 
 @param stack The stack.
 */
extern char **PDStackPopIdentifier(PDStackRef *stack);

/**
 Pop a stack off of the stack. Throws assertion if the next item is not a stack.
 
 @param stack The stack.
 */
extern PDStackRef PDStackPopStack(PDStackRef *stack);

/**
 Pop a PDEnvRef off of the stack. Throws assertion if the next item is not an environment.
 
 @param stack The stack.
 */
extern PDEnvRef PDStackPopEnv(PDStackRef *stack);

/**
 Pop a freeable off of the stack. Throws assertion if the next item is not a freeable.
 
 @param stack The stack.
 */
extern void *PDStackPopFreeable(PDStackRef *stack);

/**
 Pop and convert key into a size_t value. Throws assertion if the next item is not a key.
 
 @param stack The stack.
 */
extern size_t PDStackPopSizeT(PDStackRef *stack);

/**
 Pop and convert key into an int value. Throws assertion if the next item is not a key.
 
 @param stack The stack.
 */
extern int    PDStackPopInt(PDStackRef *stack);

/**
 Pop the next key, verify that it is equal to the given key, and then discard it. 
 
 Throws assertion if any of this is not the case.
 
 @param stack The stack.
 @param key Expected key.
 */
extern void   PDStackAssertExpectedKey(PDStackRef *stack, const char *key);

/**
 Pop the next key, verify that its integer value is equal to the given integer, and then discard it. 
 
 Throws assertion if any of this is not the case.
 
 @param stack The stack.
 @param i Expected integer value.
 */
extern void   PDStackAssertExpectedInt(PDStackRef *stack, int i);

/**
 Look at the next int on the stack without popping it. Throws assertion if the next item is not a key.
 
 @param stack The stack.
 */
extern int    PDStackPeekInt(PDStackRef stack);

/// @name Convenience features

/**
 Pop a value off of source and push it onto dest.
 
 @param dest The destination stack.
 @param source The source stack. Must not be NULL.
 */
extern void PDStackPopInto(PDStackRef *dest, PDStackRef *source);

// 
/**
 Non-destructive dictionary get function.
 
 @param dictStack The dictionary stack.
 @param key The key.
 @param remove If true, the entry for the key is removed from the dictionary stack.
 */
extern PDStackRef PDStackGetDictKey(PDStackRef dictStack, const char *key, PDBool remove);

/**
 Non-destructive stack iteration.
 
 Converts the value of the dictionary entry into a string representation.
 
 @warning In order to use this function, a supplementary PDStackRef must be set to the dictionary stack, and then used as the iterStack argument. Passing the master dictionary will result in the (memory / information) loss of the stack.
 
 @note Value must be freed, key must not.
 
 @param iterStack The iteration stack.
 @param key Pointer to a C string which should be pointed at the next key in the dictionary. Must not be pre-allocated. Must not be freed.
 @param value Pointer to the value string. Must not be pre-allocated. Must be freed.
 @return true if key and value were set, false if not.
 */
extern PDBool PDStackGetNextDictKey(PDStackRef *iterStack, char **key, char **value);

/// @name Debugging

/**
 Print out a stack (including pointer data).
 
 @param stack The stack.
 */
extern void PDStackPrint(PDStackRef stack);
/**
 Show a stack (like printing, but in a more overviewable format).
 
 @param stack The stack.
 */
extern void PDStackShow(PDStackRef stack);

#endif

/** @} */

/** @} */
