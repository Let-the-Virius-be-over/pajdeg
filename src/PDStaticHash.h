//
//  PDStaticHash.h
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
 @defgroup STATICHASH_GRP Static Hash
 
 @brief A (very) simple hash table implementation.
 
 Limited to predefined set of primitive keys on creation. O(1) but triggers false positives in some instances.

 @{
 */

#ifndef INCLUDED_PDStaticHash_h
#define INCLUDED_PDStaticHash_h

#include "PDDefines.h"

/**
 Generate a hash for the given key based on provided mask and shift.
 
 @param mask The mask.
 @param shift The shift.
 @param key The key.
 */
#define static_hash_idx(mask, shift, key)    (((long)key ^ ((long)key >> shift)) & mask)

/**
 Generate a hash for the given key in the given static hash.
 
 @param staticHash The PDStaticHashRef instance.
 @param key The key.
 */
#define PDStaticHashIdx(staticHash, key)     static_hash_idx(staticHash->mask, staticHash->shift, key)

/**
 Obtain value for given hash.
 
 @param href The PDStaticHashRef instance.
 @param hash The hash.
 */
#define PDStaticHashValueForHash(href, hash) href->table[hash]

/**
 Obtain value for given key.
 
 @param href The PDStaticHashRef instance.
 @param key The key.
 */
#define PDStaticHashValueForKey(href, key)   href->table[PDStaticHashIdx(href, key)]

/**
 Obtain typecast value for hash.

 @param href The PDStaticHashRef instance.
 @param hash The hash.
 @param type The type.
 */
#define PDStaticHashValueForHashAs(href, hash, type) as(type, PDStaticHashValueForHash(href, hash))

/**
 Obtain typecast value for key.
 
 @param href The PDStaticHashRef instance.
 @param key The key.
 @param type The type.
 */
#define PDStaticHashValueForKeyAs(href, key, type)   as(type, PDStaticHashValueForKey(href, key))

/**
 Create a static hash with keys and values.
 
 Sets up a static hash with given entries, where given keys are hashed as longs into indices inside the internal table, with guaranteed non-collision and O(1) mapping of keys to values.
 
 @note Setting up is expensive.
 @warning Triggers false-positives for non-complete input.
 
 @param entries The number of entries.
 @param keys The array of keys. Note that keys are primitives.
 @param values The array of values corresponding to the array of keys.
 */
extern PDStaticHashRef PDStaticHashCreate(PDInteger entries, void **keys, void **values);

/**
 Indicate that keys and/or values are not owned by the static hash and should not be freed on destruction; enabled flags are not disabled even if false is passed in (once disowned, always disowned)
 
 @param sh The static hash
 @param disownKeys Whether keys should be disowned.
 @param disownValues Whether values should be disowned.
 */
extern void PDStaticHashDisownKeysValues(PDStaticHashRef sh, PDBool disownKeys, PDBool disownValues);

// add key/value entry to a static hash; often expensive
//extern void PDStaticHashAdd(void *key, void *value);

/**
 Retain static hash.
 
 @param sh The static hash
 */
extern PDStaticHashRef PDStaticHashRetain(PDStaticHashRef sh);

/**
 Retain static hash.

 @param sh The static hash
 */
extern void PDStaticHashRelease(PDStaticHashRef sh);

#endif

/** @} */
