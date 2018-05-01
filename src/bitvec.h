/**
 * @file
 * Bit vector and Bloom Filter operations
 */

/*
 *******************************************************************
 *
 * Copyright 2016 Intel Corporation All rights reserved.
 *
 *-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 */

#ifndef _BITVEC_H
#define _BITVEC_H

#include <stdint.h>
#include <stddef.h>
#include <dps/private/dps.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque type for bit vector and Bloom Filter operations
 */
typedef struct _DPS_BitVector DPS_BitVector;

/**
 * Opaque type for supporting add/remove operations on bit vectors
 */
typedef struct _DPS_CountVector DPS_CountVector;

/**
 * Global configuration for this module. Overrides the default value
 * for various global parameters. These are system wide parameters
 * that must be the same for all nodes participating in a single DPS
 * network.
 *
 * @param  bitLen        The size of the bit vectors in bits.  The size must be a multiple of 64.
 * @param  numHashes     The number of hashes for Bloom filter operations - must be in the range 1..16.
 *
 * @return
 * - DPS_OK if the parameters are ok.
 * -DPS_ERR_ARGS if the setting values are not permitted.
 */
DPS_Status DPS_Configure(size_t bitLen, size_t numHashes);

/**
 * Bloom Filter insertion operation.
 *
 * @param bv        An initialized bit vector
 * @param data      Data for the item to add
 * @param len       Length of the data to add
 */
void DPS_BitVectorBloomInsert(DPS_BitVector* bv, const uint8_t* data, size_t len);

/**
 * Bloom Filter existence check operation.
 *
 * @param bv    An initialized bit vector
 * @param data  Data for the item to check for
 * @param len   Length of the data to check
 *
 * @return 1 if the item is present, 0 if not.
 */
int DPS_BitVectorBloomTest(const DPS_BitVector* bv, const uint8_t* data, size_t len);

/**
 * Allocates a bit vector using the default values set by DPS_Configure()
 *
 * @return  An initialized bit vector bit vector or NULL if the allocation failed.
 */
DPS_BitVector* DPS_BitVectorAlloc();

/**
 * Allocates a bit vector sized for use as a fuzzy hash.
 *
 * @return  An initialized bit vector or NULL if the allocation failed.
 */
DPS_BitVector* DPS_BitVectorAllocFH();

/**
 * Clone a bit vector
 *
 * @param bv An initialized bit vector
 *
 * @return  An initialized context structure or NULL if the allocation failed.
 */
DPS_BitVector* DPS_BitVectorClone(DPS_BitVector* bv);

/**
 * Duplicate a bit vector
 *
 * @param dst Destination bit vector
 * @param src Source bit vector
 */
void DPS_BitVectorDup(DPS_BitVector* dst, DPS_BitVector* src);

/**
 * Free resources for a bit vector
 *
 * @param bv   An initialized bit vector
 */
void DPS_BitVectorFree(DPS_BitVector* bv);

/**
 * Compute the load factor of the bit vector. The value returned is in
 * the range 0.0..100.0 and is the percentage of bits set in the bit
 * vector.
 *
 * @param bv   An initialized bit vector
 *
 * @return the load factor
 */
float DPS_BitVectorLoadFactor(DPS_BitVector* bv);

/**
 * Compute the population count (number of bits set) of the bit vector.
 *
 * @param bv   An initialized bit vector
 *
 * @return the population count
 */
size_t DPS_BitVectorPopCount(DPS_BitVector* bv);

/**
 * Generate a "fuzzy hash" (also called a "similarity preserving
 * hash") of a bit vector. The hash has the additional strong property
 * that given two bit vectors A and B where A is a superset of B,
 * FH(A) will be a superset of FH(B).
 *
 * To have the correct size the hash bit vector must has been
 * allocated by calling DPS_BitVectorAllocFH().
 *
 * @param hash  Returns the fuzzy hash of the input bit vector
 * @param bv    An initialized bit vector
 *
 * @return  The population count (number of bits set) of the bit vector.
 */
DPS_Status DPS_BitVectorFuzzyHash(DPS_BitVector* hash, DPS_BitVector* bv);

/**
 * Check if one bit vector includes all bit of another. The two bit
 * vectors must be the same size.  Returns DPS_FALSE is bv1 has no
 * bits set.
 *
 * @param bv1   An initialized bit vector
 * @param bv2   The bit vector to test for inclusion
 *
 * @return
 * - DPS_TRUE  if the bv2 is included in bv1.
 * - DPS_FALSE if the bv2 is not included in bv1 or if the two bit
 *             vectors cannot be compared.
 */
int DPS_BitVectorIncludes(const DPS_BitVector* bv1, const DPS_BitVector* bv2);

/**
 * Check if two bit vectors are identical.
 *
 * @param bv1   An initialized bit vector
 * @param bv2   An initialized bit vector
 *
 * @return
 * - DPS_TRUE  if the two bit vectors are identical
 * - DPS_FALSE if the two bit vectors are different or if the two bit
 *             vectors cannot be compared.
 */
int DPS_BitVectorEquals(const DPS_BitVector* bv1, const DPS_BitVector* bv2);

/**
 * Returns the intersection of two bit vectors. The bit vectors must
 * be the same size.
 *
 * @param bvOut   The result of the intersection (can be same as bv1 or bv2)
 * @param bv1     A bit vector
 * @param bv2     A bit vector
 *
 * @return DPS_OK if computing the intersection is successful, an error
 *         otherwise
 */
DPS_Status DPS_BitVectorIntersection(DPS_BitVector* bvOut, DPS_BitVector* bv1, DPS_BitVector* bv2);

/**
 * Compute the xor of two bit vectors. The bit vectors must be the
 * same size.
 *
 * @param bvOut   The bit vector to receive the difference (can be same as bv1 or bv2)
 * @param bv1     A bit vector
 * @param bv2     A bit vector
 * @param equal   Returns non-zero if the two bit input vectors are identical in which case
 *                the output vector will be cleared. Can be NULL.
 *
 * @return DPS_OK if computing the xor is successful, an error
 *         otherwise
 */
DPS_Status DPS_BitVectorXor(DPS_BitVector* bvOut, DPS_BitVector* bv1, DPS_BitVector* bv2, int* equal);

/**
 * Forms the union of two bit vectors.
 *
 * @param bvOut  The bit vector to form a union with
 * @param bv     A bit vector
 *
 * @return DPS_OK if computing the union is successful, an error
 *         otherwise
 */
DPS_Status DPS_BitVectorUnion(DPS_BitVector* bvOut, DPS_BitVector* bv);

/**
 * Compress and serialize a bit vector into a buffer
 *
 * @param bv      The bit vector to serialize
 * @param buffer  The buffer to serialize the bit vector into
 *
 * @return  The success or failure of the operation
 */
DPS_Status DPS_BitVectorSerialize(DPS_BitVector* bv, DPS_TxBuffer* buffer);

/**
 * Maximum buffer space needed to serialize a bit vector.
 *
 * @param bv  The bit vector to check
 *
 * @return  The maximum space needed to serialize a bit vector.
 */
size_t DPS_BitVectorSerializeMaxSize(DPS_BitVector* bv);

/**
 * Deserialize an decompress a bit vector from a buffer
 *
 * @param bv      Allocated bit vector to deserialize into
 * @param buffer  The buffer containing a serialized bit vector
 *
 * @return  an initialized bit vector or null if the deserialization failed
 */
DPS_Status DPS_BitVectorDeserialize(DPS_BitVector* bv, DPS_RxBuffer* buffer);

/**
 * Clear all bits in an existing bit vector.
 *
 * @param bv  The bit vector to clear.
 */
void DPS_BitVectorClear(DPS_BitVector* bv);

/**
 * Set all bits in an existing bit vector.
 *
 * @param bv  The bit vector to set.
 */
void DPS_BitVectorFill(DPS_BitVector* bv);

/**
 * Test if bit vector has no bits set.
 *
 * @param bv  The bit vector to test.
 *
 * @return DPS_TRUE if the bit vector has no bits set, DPS_FALSE
 *         otherwise.
 */
int DPS_BitVectorIsClear(DPS_BitVector* bv);

/**
 * Bitwise complement changes all 1's to 0's and 0's to 1's
 *
 * @param bv  The bit vector to complement.
 */
void DPS_BitVectorComplement(DPS_BitVector* bv);

/**
 * Set the bits in a bit vector. This is primarily for unit testing.
 *
 * @param bv      The bit vector to set
 * @param data    The data to set in the bit vector
 * @param len     The length of the data to set. This must match the bit size of the bit vector.
 *
 * @return DPS_OK if the set is successful, an error otherwise
 */
DPS_Status DPS_BitVectorSet(DPS_BitVector* bv, uint8_t* data, size_t len);

/**
 * Dump information about a bit vector
 *
 * @param bv    The bit vector to dump
 * @param bits  If non-zero dump out the bit array
 */
void DPS_BitVectorDump(DPS_BitVector* bv, int bits);

/**
 * Allocates a count vector using the default values set by DPS_Configure()
 *
 * @return  An initialized count vector bit vector or NULL if the allocation failed.
 */
DPS_CountVector* DPS_CountVectorAlloc();

/**
 * Allocates a count vector sized for use as a fuzzy hash.
 *
 * @return  An initialized count vector or NULL if the allocation failed.
 */
DPS_CountVector* DPS_CountVectorAllocFH();

/**
 * Free resources for a count vector
 *
 * @param cv   An initialized count vector
 */
void DPS_CountVectorFree(DPS_CountVector* cv);

/**
 * Adds a bit vector to a count vector.
 *
 * @param cv An initialized count vector
 * @param bv An initialized bit vector
 *
 * @return
 * - DPS_OK
 * - DPS_ERR_OVERFLOW if the counter vector is full
 */
DPS_Status DPS_CountVectorAdd(DPS_CountVector* cv, DPS_BitVector* bv);

/**
 * Deletes a bit vector from a count vector. The bit vector should be the
 * same as one that was previously added or the results are unpredictable.
 *
 * @param cv An initialized count vector
 * @param bv An initialized bit vector
 *
 * @return DPS_OK if the delete is successful, an error otherwise
 */
DPS_Status DPS_CountVectorDel(DPS_CountVector* cv, DPS_BitVector* bv);

/**
 * Allocates and returns a bit vector that represents the union of the
 * bit vectors added to the count vector.
 *
 * @param cv An initialized count vector
 *
 * @return  A bit vector or NULL if the resource could not be allocated
 */
DPS_BitVector* DPS_CountVectorToUnion(DPS_CountVector* cv);

/**
 * Allocates and returns a bit vector that represents the intersection of the
 * bit vectors added to the count vector.
 *
 * @param cv An initialized count vector
 *
 * @return  A bit vector or NULL if the resource could not be allocated
 */
DPS_BitVector* DPS_CountVectorToIntersection(DPS_CountVector* cv);

/**
 * Print a count vector.
 *
 * @param cv An initialized count vector
 */
void DPS_CountVectorDump(DPS_CountVector* cv);

#ifdef __cplusplus
}
#endif

#endif
