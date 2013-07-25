//
//  PDObject.h
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
 @defgroup OBJECT_GRP Objects
 
 @brief A PDF object.

 Objects in PDFs range from simple numbers indicating the length of some stream somewhere, to streams, images, and so on. In fact, the only things other than objects in a PDF, at the root level, are XREF (cross reference) tables, trailers, and the "startxref" marker.
 
 ### Pajdeg objects are momentarily mutable. 
 
 @warning Pajdeg object mutability expires. If you are attempting to modify a PDObjectRef instance and it's not reflected in the resulting PDF, you may be updating the object *too late*.
 
 What this means is that the objects can, with a few exceptions (see below paragraph), be modified at the moment of creation, and the modifications will be reflected in the resulting PDF document. An object can also be kept around indefinitely (by retaining it), but will at a certain point silently become immutable (changes made to the object instance will update the object itself, but the resulting PDF will not have the changes).
 
 Objects that are always immutable are:
 
 1. The PDParserRef's root object. To modify the root object, check its object ID and add a filter task.
 2. Any object fetched via PDParserLocateAndCreateDefinitionForObject(). Same deal; add a filter and mutator.
 
 ### Pinpointing mutability expiration
 
 A mutable object is mutable for as long as it has not been written to the output file. This happens as soon as the parser iterates to the next object via PDParserIterate(), and this will happen as soon as all the tasks triggered for the object finish executing.
 
 In other words, an object can be kept mutable forever by simply having a task do
 
 @code
    while (1) sleep(1);
 @endcode
 
 or, an arguably more useful example, an asynchronous operation can be triggered by simply keeping the task waiting for some flag, e.g.
 
 @code
 PDTaskResult asyncWait(PDPipeRef pipe, PDTaskRef task, PDObjectRef object)
 {
    int asyncDone = 0;
    do_asynchronous_thing(object, &asyncDone);
    while (! asyncDone) sleep(1);
    return PDTaskDone;
 }
 
 void do_asynchronous_thing(PDObjectRef object, int *asyncDone)
 {
    // start whatever asynchronous thing needs doing
 }
 
 void finish_asynchronous_thing(PDObjectRef object, int *asyncDone)
 {
    PDObjectSetDictionaryEntry(object, "Foo", "bar");
    *asyncDone = 1;
 }
 @endcode

 @{
 */

#ifndef INCLUDED_PDObject_h
#define INCLUDED_PDObject_h

#include "PDDefines.h"

/// @name Holding

/**
 Retain an object.
 
 @param object The object.
 @return The object.
 */
extern PDObjectRef PDObjectRetain(PDObjectRef object);

/**
 Release an object
 
 @param object The object.
 */
extern void PDObjectRelease(PDObjectRef object);

/// @name Examining

/**
 Get object ID for object.
 
 @param object The object.
 */
extern int PDObjectGetObID(PDObjectRef object);

/**
 Get generation ID for an object.
 
 @param object The object.
 */
extern int PDObjectGetGenID(PDObjectRef object);

/**
 Get type of an object.
 
 @param object The object.
 @return The PDObjectType of the object.
 
 @note Types are restricted to PDObjectTypeUnknown, PDObjectTypeDictionary, and PDObjectTypeString in the current implementation.
 */
extern PDObjectType PDObjectGetType(PDObjectRef object);

/**
 Determine if the object has a stream or not.
 
 @param object The object.
 */
extern PDBool PDObjectHasStream(PDObjectRef object);

/**
 Determine the length of the object stream.
 
 @param object The object.
 */
extern int PDObjectGetStreamLength(PDObjectRef object);

/**
 Fetch the value of the given object.
 
 @note If object is non-primitive (e.g. dictionary), the returned value is not a proper string.
 
 @param object The object.
 @return The value of the primitive (string, integer, real, ...) object. 
 */
extern char *PDObjectGetValue(PDObjectRef object);

/**
 Fetch the dictionary entry for the given key.
 
 @warning Crashes if the object is not a dictionary.
 
 @param object The object.
 @param key The dictionary key. Note that keys in a PDF dictionary are names, i.e. /Something, but the corresponding key in PDObjectRef is the string without the forward slash, i.e. "Something" in this case.
 */
extern const char *PDObjectGetDictionaryEntry(PDObjectRef object, const char *key);

/// @name Mutation

/**
 Set a dictionary key to a new value.
 
 @note Expects object to be a dictionary.
 @note Value must be a null terminated string.
 
 @param object  The object.
 @param key     The dictionary key.
 @param value   The string value, null terminated.
 */
extern void PDObjectSetDictionaryEntry(PDObjectRef object, const char *key, const char *value);

/**
 Delete a dictionary key. 
 
 Does nothing if the key does not exist
 
 @param object The object.
 @param key The key to delete.
 */
extern void PDObjectRemoveDictionaryEntry(PDObjectRef object, const char *key);

/**
 Replaces the entire object's definition with the given string of the given length; does not replace the stream and the caller is responsible for asserting that the /Length key is preserved; if the stream was turned off, this may include a stream element by abiding by the PDF specification, which requires that
 
 1. the object is a dictionary, and has a /Length key with the exact length of the stream (excluding the keywords and newlines wrapping it), 
 2. the keyword
 @code
    stream
 @endcode
 is directly below the object dictionary on its own line followed by the stream content, and 
 3. followed by
 @code
    endstream
 @endcode
 right after the stream length (extraneous whitespace is allowed between the content's last byte and the 'endstream' keyword's beginning) also note that filters and encodings are often used, but not required.
 
 @param object The object.
 @param str The replacement string.
 @param len The length of the replacement string.
 */
extern void PDObjectReplaceWithString(PDObjectRef object, char *str, int len);

/**
 Removes the stream from the object.
 
 The stream will be skipped when written. This has no effect if the object had no stream to begin with.
 
 @param object The object.
 */
extern void PDObjectSkipStream(PDObjectRef object);

/**
 Replaces the stream with given data.
 
 @warning Caller is responsible for asserting that e.g. presence of /Filter /FlateDecode and usage of gzip compression match up. 
 
 @param object The object.
 @param str The stream data.
 @param len The length of the stream data.
 @param includeLength Whether the object's /Length entry should be updated to reflect the new stream content length.
 */
extern void PDObjectSetStream(PDObjectRef object, const char *str, int len, PDBool includeLength);

/**
 Sets the encrypted flag for the object's stream.
 
 @warning If the document is encrypted, and the stream is not, this must be set to false or the stream will not function properly
 
 @param object The object.
 @param encrypted Whether or not the stream is encrypted.
 */
extern void PDObjectSetEncryptedStreamFlag(PDObjectRef object, PDBool encrypted);

/// @name Conversion

/**
 Generates an object definition up to and excluding the stream definition, from "<obid> <genid> obj" to right before "endobj" or "stream" depending on whether a stream exists or not.
 
 The results are written into dstBuf, reallocating it if necessary (i.e. it must be a valid allocation and not a point inside a heap).
 
 @note This method ignores definition replacements via PDObjectReplaceWithString().
 
 @return Bytes written. 
 @param object The object.
 @param dstBuf Pointer to buffer into which definition should be written. Must be a proper allocation.
 @param capacity The number of bytes allocated into *dstBuf already.
 */
extern int PDObjectGenerateDefinition(PDObjectRef object, char **dstBuf, int capacity);

#endif

/** @} */

/** @} */ // unbalanced, but doxygen complains for some reason

