//
// PDObject.h
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
 @file PDObject.h PDF object header file.
 
 @ingroup PDOBJECT

 @defgroup PDOBJECT PDObject
 
 @brief A PDF object.
 
 @ingroup PDUSER

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
    PDInteger asyncDone = 0;
    do_asynchronous_thing(object, &asyncDone);
    while (! asyncDone) sleep(1);
    return PDTaskDone;
 }
 
 void do_asynchronous_thing(PDObjectRef object, PDInteger *asyncDone)
 {
    // start whatever asynchronous thing needs doing
 }
 
 void finish_asynchronous_thing(PDObjectRef object, PDInteger *asyncDone)
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

/// @name Examining

/**
 Get object ID for object.
 
 @param object The object.
 */
extern PDInteger PDObjectGetObID(PDObjectRef object);

/**
 Get generation ID for an object.
 
 @param object The object.
 */
extern PDInteger PDObjectGetGenID(PDObjectRef object);

/**
 Get reference string for this object.
 
 @param object The object.
 */
extern const char *PDObjectGetReferenceString(PDObjectRef object);

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
 Determine the raw (unextracted) length of the object stream.
 
 This can be compared to the size of a file.txt.gz.
 
 @param object The object.
 */
extern PDInteger PDObjectGetStreamLength(PDObjectRef object);

/**
 Determine the extracted length of the previously fetched object stream. 
 
 This can be compared to the size of a file.txt after decompressing a file.txt.gz.
 
 @warning Assertion thrown if the object stream has not been fetched before this call.
 
 @param object The object.
 */
extern PDInteger PDObjectGetExtractedStreamLength(PDObjectRef object);

/**
 Get the object's stream. Assertion thrown if the stream has not been fetched via PDParserFetchCurrentObjectStream() first.
 
 @param object The object.
 */
extern char *PDObjectGetStream(PDObjectRef object);

/**
 Fetch the value of the given object.
 
 @note If object is non-primitive (e.g. dictionary), the returned value is not a proper string.
 
 @param object The object.
 @return The value of the primitive (string, integer, real, ...) object. 
 */
extern char *PDObjectGetValue(PDObjectRef object);

/// @name Dictionary objects

/**
 Fetch the dictionary entry for the given key.
 
 @warning Crashes if the object is not a dictionary.
 
 @param object The object.
 @param key The dictionary key. Note that keys in a PDF dictionary are names, i.e. /Something, but the corresponding key in PDObjectRef is the string without the forward slash, i.e. "Something" in this case.
 */
extern const char *PDObjectGetDictionaryEntry(PDObjectRef object, const char *key);

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

/// @name Array objects

/**
 Get the element count of the array object.
 
 @warning Crashes if the object is not an array.
 
 @param object The object.
 */
extern PDInteger PDObjectGetArrayCount(PDObjectRef object);

/**
 Fetch the array element at the given index.
 
 @warning Crashes if the object is not an array.
 
 @param object The object.
 @param index The array index.
 */
extern const char *PDObjectGetArrayElementAtIndex(PDObjectRef object, PDInteger index);

/**
 Add an element to the array object.
 
 @note Expects object to be an array.
 @note Value must be a null terminated string.
 
 @param object  The object.
 @param value   The string value, null terminated.
 */
extern void PDObjectAddArrayElement(PDObjectRef object, const char *value);

/**
 Delete the array element at the given index.
 
 @param object The object.
 @param index The array index.
 */
extern void PDObjectRemoveArrayElementAtIndex(PDObjectRef object, PDInteger index);

/**
 Replace the value of the array element at the given index with a new value.
 
 @param object The object.
 @param index The array index.
 @param value The replacement value.
 */
extern void PDObjectSetArrayElement(PDObjectRef object, PDInteger index, const char *value);

/// @name Miscellaneous

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
 right after the stream length (extraneous whitespace is allowed between the content's last byte and the 'endstream' keyword's beginning).
 
 Also note that filters and encodings are often used, but not required.
 
 @param object The object.
 @param str The replacement string.
 @param len The length of the replacement string.
 */
extern void PDObjectReplaceWithString(PDObjectRef object, char *str, PDInteger len);

/// @name PDF stream support

/**
 Removes the stream from the object.
 
 The stream will be skipped when written. This has no effect if the object had no stream to begin with.
 
 @param object The object.
 */
extern void PDObjectSkipStream(PDObjectRef object);

/**
 Replaces the stream with given data.
 
 @note The stream is inserted as is, with no filtering applied to it whatsoever. To insert a filtered stream, e.g. FlateDecoded, use PDObjectSetStreamFiltered() instead.
 
 @param object The object.
 @param str The stream data.
 @param len The length of the stream data.
 @param includeLength Whether the object's /Length entry should be updated to reflect the new stream content length.
 @param allocated Whether str should be free()d after the object is done using it.
 */
extern void PDObjectSetStream(PDObjectRef object, char *str, PDInteger len, PDBool includeLength, PDBool allocated);

/**
 Replaces the stream with given data, filtered according to the object's /Filter and /DecodeParams settings.
 
 @note Pajdeg only supports a limited number of filters. If the object's filter settings are not supported, the operation is aborted.
 
 @see PDObjectSetStreamFiltered
 @see PDObjectSetFlateDecodedFlag
 @see PDObjectSetPredictionStrategy
 
 @warning str is not freed.
 
 @param object The object.
 @param str The stream data.
 @param len The length of the stream data.
 @return Success value. If false is returned, the stream remains unset.
 */
extern PDBool PDObjectSetStreamFiltered(PDObjectRef object, char *str, PDInteger len);

/**
 Enable or disable compression (FlateDecode) filter flag for the object stream.
 
 @note Passing false to the state will remove the Filter and DecodeParms dictionary entries from the object.
 
 @param object The object.
 @param state Boolean value of whether the stream is compressed or not.
 */
extern void PDObjectSetFlateDecodedFlag(PDObjectRef object, PDBool state);

/**
 Define prediction strategy for the stream.
 
 @warning Pajdeg currently only supports PDPredictorNone and PDPredictorPNG_UP. Updating an existing stream (e.g. fixing its predictor values) is possible, however, but replacing the stream or requiring Pajdeg to predict the content in some other way will cause an assertion.
 
 @param object The object.
 @param strategy The PDPredictorType value.
 @param columns Columns value.
 
 @see PDPredictorType
 @see PDStreamFilterPrediction.h
 */
extern void PDObjectSetPredictionStrategy(PDObjectRef object, PDPredictorType strategy, PDInteger columns);

/**
 Sets the encrypted flag for the object's stream.
 
 @warning If the document is encrypted, and the stream is not, this must be set to false or the stream will not function properly
 
 @param object The object.
 @param encrypted Whether or not the stream is encrypted.
 */
extern void PDObjectSetStreamEncrypted(PDObjectRef object, PDBool encrypted);

/// @name Conversion

/**
 Generates an object definition up to and excluding the stream definition, from "<obid> <genid> obj" to right before "endobj" or "stream" depending on whether a stream exists or not.
 
 The results are written into dstBuf, reallocating it if necessary (i.e. it must be a valid allocation and not a point inside a heap).
 
 @note This method ignores definition replacements via PDObjectReplaceWithString().
 
 @param object The object.
 @param dstBuf Pointer to buffer into which definition should be written. Must be a proper allocation.
 @param capacity The number of bytes allocated into *dstBuf already.
 @return Bytes written. 
 */
extern PDInteger PDObjectGenerateDefinition(PDObjectRef object, char **dstBuf, PDInteger capacity);

#endif

/** @} */

/** @} */ // unbalanced, but doxygen complains for some reason

