//
// PDStaticHash.h
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
 @file PDStaticHash.h Static hash header file.
 
 @ingroup PDSTATICHASH
 
 @defgroup PDSTATICHASH PDStaticHash
 
 @brief A (very) simple hash table implementation.
 
 @ingroup PDALGO
 
 Limited to predefined set of primitive keys on creation. \f$O(1)\f$ but triggers false positives for undefined keys.
 
 This is used as a pre-filter in the PDPipe implementation to reduce the misses for the more cumbersome pd_btree containing tasks. It is also used in the PDF spec implementation's PDStringFromComplex() function. 
 
 The static hash generates a hash table of \f$2^n\f$ size, where \f$n\f$ is the lowest possible number where the primitive value of the range of keys produces all unique indices based on the following formula, where \f$K\f$, \f$s\f$, and \f$m\f$ are the key, shift and mask parameters
 
    \f$(K \oplus (K \gg s)) \land m\f$
 
  The shifting is necessary because the range of keys tend to align, and in bad cases, the alignment happens in the low bits, resulting in redundancy. Unfortunately, determining the mask and shift values for a given instance is expensive and fairly unoptimized at the moment, with the excuse that creation does not occur repeatedly.
 
 @see PDPIPE
 @see PDPDF_GRP
 
 @{
 */

#ifndef INCLUDED_PDStaticHash_h
#define INCLUDED_PDStaticHash_h

#include "PDDefines.h"

/**
 Generate a hash for the given key based on provided mask and shift.
 
 @note Does not rely on a PDStaticHash instance.
 
 @param mask The mask.
 @param shift The shift.
 @param key The key.
 */
#define static_hash_idx(mask, shift, key)    (((long)key ^ ((long)key >> shift)) & mask)

/**
 Generate a hash for the given key in the given static hash.
 
 @param stha The PDStaticHashRef instance.
 @param key The key.
 */
#define PDStaticHashIdx(stha, key)     static_hash_idx(stha->mask, stha->shift, key)

/**
 Obtain value for given hash.
 
 @param stha The PDStaticHashRef instance.
 @param hash The hash.
 */
#define PDStaticHashValueForHash(stha, hash) stha->table[hash]

/**
 Obtain value for given key.
 
 @param stha The PDStaticHashRef instance.
 @param key The key.
 */
#define PDStaticHashValueForKey(stha, key)   stha->table[PDStaticHashIdx(stha, key)]

/**
 Obtain typecast value for hash.

 @param stha The PDStaticHashRef instance.
 @param hash The hash.
 @param type The type.
 */
#define PDStaticHashValueForHashAs(stha, hash, type) as(type, PDStaticHashValueForHash(stha, hash))

/**
 Obtain typecast value for key.
 
 @param stha The PDStaticHashRef instance.
 @param key The key.
 @param type The type.
 */
#define PDStaticHashValueForKeyAs(stha, key, type)   as(type, PDStaticHashValueForKey(stha, key))

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

/** @} */

#endif
