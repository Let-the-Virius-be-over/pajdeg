//
// PDHashMap.h
//
// Copyright (c) 2014 Karl-Johan Alm (http://github.com/kallewoof)
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

/**
 @file PDHashMap.h Hash map object
 
 @ingroup PDHASHMAP
 
 @defgroup PDHASHMAP PDHashMap
 
 @brief A hash map construct.
 
 PDHashMap is a simple hash map implementation, which supports arbitrary key types (e.g. strings).
 
 @{
 */

#ifndef INCLUDED_PDHASHMAP_H
#define INCLUDED_PDHASHMAP_H

#include "PDDefines.h"

/**
 *  Create a hash map with C string keys using default number of buckets.
 *
 *  @return New hash map
 */
extern PDHashMapRef PDHashMapCreate();

/**
 *  Create a hash map with the given number of buckets.
 *  The hash map uses C string keys.
 *
 *  @param bucketCount Number of buckets to use in the hash map
 *
 *  @return New hash map
 */
extern PDHashMapRef PDHashMapCreateWithBucketCount(PDSize bucketCount);

/**
 *  Create a hash map with a complex object (a PDF dictionary stack).
 *
 *  @param stack PDF dictionary stack
 *
 *  @return PDHashMap containing the PDF dictionary content
 */
extern PDHashMapRef PDHashMapCreateWithComplex(pd_stack stack);

/**
 *  Add all entries from the given PDF dictionary stack to this hash map.
 *  Entries will replace pre-existing key/value pairs when a key in the 
 *  pd_stack matches the key in the hash map.
 *
 *  @param hm    Hash map to add entries to
 *  @param stack PDF dictionary stack
 */
extern void PDHashMapAddEntriesFromComplex(PDHashMapRef hm, pd_stack stack);

/**
 *  Set key to value. If value is NULL, an assertion is thrown. To delete
 *  keys, use PDHashMapDelete().
 *
 *  @param hm    The hash map
 *  @param key   The key
 *  @param value The value
 */
extern void PDHashMapSet(PDHashMapRef hm, const char *key, void *value);

/**
 *  Get the value of the given key.
 *
 *  @param hm  The hash map
 *  @param key The key
 */
extern void *PDHashMapGet(PDHashMapRef hm, const char *key);

/**
 *  Delete the given key from the hash map
 *
 *  @param hm  The hash map
 *  @param key The key to delete
 */
extern void PDHashMapDelete(PDHashMapRef hm, const char *key);

/**
 *  Get the number of items in the hash map currently.
 *
 *  @param hm The hash amp
 *
 *  @return The number of inserted key/value pairs
 */
extern PDSize PDHashMapGetCount(PDHashMapRef hm);

/**
 *  Iterate over the hash map entries using the given iterator.
 *
 *  @param hm The hash map whose key/value pairs should be iterated
 *  @param it The iterating function
 *  @param ui User information to pass to iterator, if any
 */
extern void PDHashMapIterate(PDHashMapRef hm, PDHashIterator it, void *ui);

/**
 *  Fill in the keys array with the keys in the dictionary. 
 *
 *  @note keys must have at minimum PDHashMapGetCount(hm) number of available slots.
 *
 *  @param hm   Hash map
 *  @param keys Pre-allocated array to populate
 */
extern void PDHashMapPopulateKeys(PDHashMapRef hm, char **keys);

/**
 *  Print the hash map content to stdout.
 *
 *  @param hm The hash map
 */
extern void PDHashMapPrint(PDHashMapRef hm);

/**
 *  Generate a C string formatted according to the PDF specification for this hash map.
 *
 *  @note The returned string must be freed.
 *
 *  @param hm The hash map
 *
 *  @return C string representation of hash map, as a PDF dictionary
 */
extern char *PDHashMapToString(PDHashMapRef hm);

extern PDInteger PDHashMapPrinter(void *inst, char **buf, PDInteger offs, PDInteger *cap);

#ifdef PD_SUPPORT_CRYPTO

/**
 *  Supply a crypto object to a hash map, and associate the hash map with a specific object. 
 *
 *  @param hm         The hash map
 *  @param crypto     The pd_crypt object
 *  @param objectID   The object ID of the owning object
 *  @param genNumber  Generation number of the owning object
 */
extern void PDHashMapAttachCrypto(PDHashMapRef hm, pd_crypto crypto, PDInteger objectID, PDInteger genNumber);

#endif // PD_SUPPORT_CRYPTO

#endif // INCLUDED_PDHASHMAP_H

/** @} */
