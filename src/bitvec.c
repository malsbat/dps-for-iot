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

#include <assert.h>
#include <safe_lib.h>
#include <stdlib.h>
#include <dps/dbg.h>
#include <dps/private/cbor.h>
#include "bitvec.h"
#include "compat.h"
#include "sha2.h"

/*
 * Debug control for this module
 */
DPS_DEBUG_CONTROL(DPS_DEBUG_ON);

#if __BYTE_ORDER != __LITTLE_ENDIAN
   #define ENDIAN_SWAP
#endif

#ifndef DPS_CONFIG_BIT_LEN
#define DPS_CONFIG_BIT_LEN 8192
#endif

#if DPS_CONFIG_BIT_LEN & 63
#error "Default DPS_CONFIG_BIT_LEN must be a multiple of 64"
#endif

#ifndef DPS_CONFIG_HASHES
#define DPS_CONFIG_HASHES 3
#endif

#if (DPS_CONFIG_HASHES < 1 || DPS_CONFIG_HASHES > 16)
#error "Default DPS_CONFIG_HASHES must be in range 1..16"
#endif

/*
 * Flag that indicates if serialized bit vector was rle encode or sent raw
 */
#define FLAG_RLE_ENCODED     0x01

/*
 * Indicates the complement of the bit vector was serialized
 */
#define FLAG_RLE_COMPLEMENT  0x02

/*
 * Process bit vectors in 64 bit chunks
 */
typedef uint64_t chunk_t;

#define CHUNK_SIZE (8 * sizeof(chunk_t))

/*
 * CountVectors are entirely internal so can be sized according to scalability requirements
 */
#ifdef DPS_BIG_COUNTER
typedef uint32_t count_t;
#define CV_MAX UINT32_MAX
#else
typedef uint16_t count_t;
#define CV_MAX UINT16_MAX
#endif

typedef count_t counter_t[CHUNK_SIZE];

#define SET_BIT(a, b)  (a)[(b) >> 6] |= (1ull << ((b) & 0x3F))
#define TEST_BIT(a, b) ((a)[(b) >> 6] & (1ull << ((b) & 0x3F)))
#define ROTL64(n, r)   (((n) << r) | ((n) >> (64 - r)))

#if defined(__GNUC__) || defined(__MINGW64__)
#define POPCOUNT(n)    __builtin_popcountll((chunk_t)n)
#define COUNT_TZ(n)    __builtin_ctzll((chunk_t)n)
#elif defined(_WIN64)
#define POPCOUNT(n)    (uint32_t)(__popcnt64((chunk_t)n))
static inline uint32_t COUNT_TZ(uint64_t n)
{
    unsigned long index;
    if (_BitScanForward64(&index, n)) {
        return index;
    } else {
        return 0;
    }
}
#elif defined(_WIN32)
static inline uint32_t POPCOUNT(chunk_t n)
{
    return __popcnt((uint32_t)n) + __popcnt((uint32_t)(n >> 32));
}
static inline uint32_t COUNT_TZ(uint64_t n)
{
    unsigned long index;
    if (_BitScanForward(&index, (uint32_t)n)) {
        return index;
    } else if (_BitScanForward(&index, (uint32_t)(n >> 32))) {
        return index + 32;
    } else {
        return 0;
    }
}
#endif

#define UNKNOWN_POPCOUNT(bv)  ((bv)->popCount < 0)
#define INVALIDATE_POPCOUNT(bv)  ((bv)->popCount = -1)

struct _DPS_BitVector {
    int32_t popCount;
    size_t len;
    chunk_t bits[1];
};

struct _DPS_CountVector {
    size_t entries;
    size_t len;
    DPS_BitVector* bvUnion;
    counter_t counts[1];
};

typedef struct {
    size_t bitLen;
    uint8_t numHashes;
} Configuration;

/*
 * Compile time defaults for the configuration parameters
 */
static Configuration config = { DPS_CONFIG_BIT_LEN, (uint8_t)DPS_CONFIG_HASHES };

#define NUM_CHUNKS(bv)  ((bv)->len / CHUNK_SIZE)

#define FH_BITVECTOR_LEN  (4 * CHUNK_SIZE)

#ifdef DPS_DEBUG
/*
 * This is a compressed bit dump - it groups bits to keep
 * the total output readable. This is usually more useful
 * than dumping raw 8K long bit vectors.
 */
static void CompressedBitDump(const chunk_t* data, size_t bits)
{
    size_t stride = bits < 128 ? 1 : bits / 128;
    size_t i;
    for (i = 0; i < bits; i += stride) {
        int bit = 0;
        size_t j;
        for (j = 0; j < stride; ++j) {
            if (TEST_BIT(data, i + j)) {
                bit = 1;
                break;
            }
        }
        putc(bit ? '1' : '0', stderr);
    }
    putc('\n', stderr);
}
#endif

#define MIN_HASHES    1
#define MAX_HASHES    8

DPS_Status DPS_Configure(size_t bitLen, size_t numHashes)
{
    if (bitLen & 63) {
        DPS_ERRPRINT("Bit length must be a multiple of 64\n");
        return DPS_ERR_ARGS;
    }
    if (numHashes < MIN_HASHES || numHashes > MAX_HASHES) {
        DPS_ERRPRINT("Number of hashes must be in the range 1..16\n");
        return DPS_ERR_ARGS;
    }
    config.bitLen = bitLen;
    config.numHashes = (uint8_t)numHashes;
    return DPS_ERR_OK;
}

static DPS_BitVector* AllocBV(size_t sz)
{
    DPS_BitVector* bv;

    assert((sz % 64) == 0);
    bv = calloc(1, sizeof(DPS_BitVector) + ((sz / CHUNK_SIZE) - 1) * sizeof(chunk_t));
    if (bv) {
        bv->len = sz;
        INVALIDATE_POPCOUNT(bv);
    }
    return bv;
}

DPS_BitVector* DPS_BitVectorAlloc()
{
    return AllocBV(config.bitLen);
}

DPS_BitVector* DPS_BitVectorAllocFH()
{
    return AllocBV(FH_BITVECTOR_LEN);
}

int DPS_BitVectorIsClear(DPS_BitVector* bv)
{
    if (UNKNOWN_POPCOUNT(bv)) {
        size_t i;
        for (i = 0; i < NUM_CHUNKS(bv); ++i) {
            if (bv->bits[i]) {
                return DPS_FALSE;
            }
        }
        bv->popCount = 0;
        return DPS_TRUE;
    } else {
        return bv->popCount == 0;
    }
}

size_t DPS_BitVectorPopCount(DPS_BitVector* bv)
{
    if (UNKNOWN_POPCOUNT(bv)) {
        uint32_t popCount = 0;
        size_t i;
        for (i = 0; i < NUM_CHUNKS(bv); ++i) {
            popCount += POPCOUNT(bv->bits[i]);
        }
        bv->popCount = popCount;
    }
    return bv->popCount;
}

void DPS_BitVectorDup(DPS_BitVector* dst, DPS_BitVector* src)
{
    assert(dst->len == src->len);
    if (dst != src) {
        memcpy_s(dst->bits, src->len / 8, src->bits, src->len / 8);
        dst->popCount = src->popCount;
    }
}

DPS_BitVector* DPS_BitVectorClone(DPS_BitVector* bv)
{
    size_t sz = sizeof(DPS_BitVector) + ((bv->len / CHUNK_SIZE) - 1) * sizeof(chunk_t);
    DPS_BitVector* clone = malloc(sz);
    if (clone) {
        memcpy_s(clone, sz, bv, sz);
    }
    return clone;
}

void DPS_BitVectorFree(DPS_BitVector* bv)
{
    if (bv) {
        free(bv);
    }
}

void DPS_BitVectorBloomInsert(DPS_BitVector* bv, const uint8_t* data, size_t len)
{
    uint8_t h;
    uint32_t hashes[MAX_HASHES];
    uint32_t index;

    assert(sizeof(hashes) == DPS_SHA2_DIGEST_LEN);

    DPS_Sha2((uint8_t*)hashes, data, len);
#if 0
    DPS_PRINT("%.*s   (%zu)\n", (int)len, data, len);
#endif
    for (h = 0; h < config.numHashes; ++h) {
#ifdef ENDIAN_SWAP
        index = BSWAP_32(hashes[h]) % bv->len;
#else
        index = hashes[h] % bv->len;
#endif
        SET_BIT(bv->bits, index);
    }
    INVALIDATE_POPCOUNT(bv);
}

int DPS_BitVectorBloomTest(const DPS_BitVector* bv, const uint8_t* data, size_t len)
{
    uint8_t h;
    uint32_t hashes[MAX_HASHES];
    uint32_t index;

    DPS_Sha2((uint8_t*)hashes, data, len);
    for (h = 0; h < config.numHashes; ++h) {
#ifdef ENDIAN_SWAP
        index = BSWAP_32(hashes[h]) % bv->len;
#else
        index = hashes[h] % bv->len;
#endif
        if (!TEST_BIT(bv->bits, index)) {
            return DPS_FALSE;
        }
    }
    return DPS_TRUE;
}

float DPS_BitVectorLoadFactor(DPS_BitVector* bv)
{
    return (float)((100.0 * DPS_BitVectorPopCount(bv) + 1.0) / bv->len);
}

int DPS_BitVectorEquals(const DPS_BitVector* bv1, const DPS_BitVector* bv2)
{
    size_t i;
    const chunk_t* b1;
    const chunk_t* b2;

    if (!bv1 || !bv2) {
        return DPS_FALSE;
    }
    if (bv1->len != bv2->len) {
        return DPS_FALSE;
    }
    b1 = bv1->bits;
    b2 = bv2->bits;
    for (i = 0; i < NUM_CHUNKS(bv1); ++i, ++b1, ++b2) {
        if (*b1 != *b2) {
            return DPS_FALSE;
        }
    }
    return DPS_TRUE;
}

int DPS_BitVectorIncludes(const DPS_BitVector* bv1, const DPS_BitVector* bv2)
{
    size_t i;
    const chunk_t* b1;
    const chunk_t* b2;
    chunk_t b1un = 0;

    if (!bv1 || !bv2) {
        return DPS_FALSE;
    }
    assert(bv1->len == bv2->len);
    if (bv1->popCount == 0) {
        return DPS_FALSE;
    }
    b1 = bv1->bits;
    b2 = bv2->bits;
    for (i = 0; i < NUM_CHUNKS(bv1); ++i, ++b1, ++b2) {
        if ((*b1 & *b2) != *b2) {
            return DPS_FALSE;
        }
        b1un |= *b1;
    }
    return b1un != 0;
}

DPS_Status DPS_BitVectorFuzzyHash(DPS_BitVector* hash, DPS_BitVector* bv)
{
    size_t i;
    chunk_t s = 0;
    chunk_t p;
    uint32_t popCount = 0;

    if (!hash || !bv) {
        return DPS_ERR_NULL;
    }
    assert(hash->len == FH_BITVECTOR_LEN);
    if (bv->popCount != 0) {
        /*
         * Squash the bit vector into 64 bits
         */
        for (i = 0; i < NUM_CHUNKS(bv); ++i) {
            chunk_t n = bv->bits[i];
            popCount += POPCOUNT(n);
            s |= n;
        }
        bv->popCount = popCount;
    }
    if (popCount == 0) {
        DPS_BitVectorClear(hash);
        return DPS_OK;
    }
    p = s;
    p |= ROTL64(p, 7);
    p |= ROTL64(p, 31);
    hash->bits[0] = p;
    p = s;
    p |= ROTL64(p, 11);
    p |= ROTL64(p, 29);
    p |= ROTL64(p, 37);
    hash->bits[1] = p;
    p = s;
    p |= ROTL64(p, 13);
    p |= ROTL64(p, 17);
    p |= ROTL64(p, 19);
    p |= ROTL64(p, 41);
    hash->bits[2] = p;
    if (popCount > 62) {
        hash->bits[3] = ~0ull;
    } else {
        hash->bits[3] = (1ull << popCount) - 1;
    }
    INVALIDATE_POPCOUNT(hash);
    return DPS_OK;
}

DPS_Status DPS_BitVectorUnion(DPS_BitVector* bvOut, DPS_BitVector* bv)
{
    size_t i;
    if (!bvOut || !bv) {
        return DPS_ERR_NULL;
    }
    assert(bvOut->len == bv->len);
    for (i = 0; i < NUM_CHUNKS(bv); ++i) {
        bvOut->bits[i] |= bv->bits[i];
    }
    INVALIDATE_POPCOUNT(bvOut);
    return DPS_OK;
}

DPS_Status DPS_BitVectorIntersection(DPS_BitVector* bvOut, DPS_BitVector* bv1, DPS_BitVector* bv2)
{
    if (!bvOut || !bv1 || !bv2) {
        return DPS_ERR_NULL;
    }
    assert(bvOut->len == bv1->len && bvOut->len == bv2->len);
    if ((bv1->popCount && bv2->popCount)) {
        size_t i;
        int nz = 0;
        assert(bvOut->len == bv1->len && bvOut->len == bv2->len);
        for (i = 0; i < NUM_CHUNKS(bv1); ++i) {
            nz |= ((bvOut->bits[i] = bv1->bits[i] & bv2->bits[i]) != 0);
        }
        if (nz) {
            INVALIDATE_POPCOUNT(bvOut);
        } else {
            bvOut->popCount = 0;
        }
    } else {
        DPS_BitVectorClear(bvOut);
    }
    return DPS_OK;
}

DPS_Status DPS_BitVectorXor(DPS_BitVector* bvOut, DPS_BitVector* bv1, DPS_BitVector* bv2, int* equal)
{
    if (!bvOut || !bv1 || !bv2) {
        return DPS_ERR_NULL;
    }
    assert(bvOut->len == bv1->len && bvOut->len == bv2->len);

    if (bv1->popCount == 0) {
        if (equal && DPS_BitVectorPopCount(bv2) == 0) {
            *equal = DPS_TRUE;
        }
        DPS_BitVectorDup(bvOut, bv2);
    } else if (bv2->popCount == 0) {
        if (equal && DPS_BitVectorPopCount(bv1) == 0) {
            *equal = DPS_TRUE;
        }
        DPS_BitVectorDup(bvOut, bv1);
    } else {
        size_t i;
        int diff = 0;

        for (i = 0; i < NUM_CHUNKS(bv1); ++i) {
            diff |= ((bvOut->bits[i] = bv1->bits[i] ^ bv2->bits[i]) != 0ull);
        }
        if (equal) {
            *equal = !diff;
        }
        INVALIDATE_POPCOUNT(bvOut);
    }
    return DPS_OK;
}

#define SET_BIT8(a, b)   (a)[(b) >> 3] |= (1 << ((b) & 0x7))
#define TEST_BIT8(a, b) ((a)[(b) >> 3] &  (1 << ((b) & 0x7)))

/****************************************

  Run-length Encoding algorithm.

  String of 1's encode unchanged

  Strings with leading zeroes are encoded as follows:

  Count the leading zeroes.
  Compute number of bits C required to encode the count
  Write C zeroes followed by a 1
  Write out the range adusted count bits
  The trailing 1 is assumed and does not need to be encoded

  prefix        count width    range encoded
  --------------------------------------------
  01               1 bit           1 ..    2
  001              2 bit           3 ..    6
  0001             3 bit           7 ..   14
  00001            4 bit          15 ..   30
  000001           5 bit          31 ..   62
  0000001          6 bit          63 ..  126
  00000001         7 bit         127 ..  254
  000000001        8 bit         255 ..  510
  0000000001       9 bit         511 .. 1022
  00000000001     10 bit        1023 .. 2046
  000000000001    11 bit        2047 .. 4094
  0000000000001   12 bit        4095 .. 8190
  etc.

Examples:

1         ->        1        =       1
01        ->       01    0   =     010
001       ->       01    1   =     011
0001      ->      001   00   =   00100
00001     ->      001   01   =   00101
000001    ->      001   10   =   00110
0000001   ->      001   11   =   00111
00000001  ->     0001  000   =  001000
000000001 ->     0001  001   =  001001

 *****************************************/

/*
 * Compute ceil(log2(n))
 */
static uint32_t Ceil_Log2(uint16_t n)
{
    uint32_t b = 0;
    if (n & 0xFF00) {
        n >>= 8;  b += 8;
    }
    if (n & 0x00F0) {
        n >>= 4;  b += 4;
    }
    if (n & 0x000C) {
        n >>= 2;  b += 2;
    }
    if (n & 0x0002) {
        n >>= 1;  b += 1;
    }
    return b;
}


static DPS_Status RunLengthEncode(DPS_BitVector* bv, DPS_TxBuffer* buffer, uint8_t flags)
{
    size_t i;
    size_t rleSize = 0;
    size_t sz;
    uint32_t num0 = 0;
    uint8_t* packed = buffer->txPos;
    chunk_t complement = flags & FLAG_RLE_COMPLEMENT ? ~0 : 0;

    /*
     * Nothing to enode for empty bit vectors
     */
    if (bv->popCount == 0) {
        return DPS_OK;
    }
    /*
     * We don't allow RLE to expand the bit vector
     */
    if (DPS_TxBufferSpace(buffer) < (bv->len / 8)) {
        return DPS_ERR_OVERFLOW;
    }
    /*
     * We only need to set the 1's so clear the buffer
     */
    memzero_s(packed, bv->len / 8);

    for (i = 0; i < NUM_CHUNKS(bv); ++i) {
        uint32_t rem0;
        chunk_t chunk = bv->bits[i] ^ complement;
        if (!chunk) {
            num0 += CHUNK_SIZE;
            continue;
        }
        rem0 = CHUNK_SIZE;
        while (chunk) {
            uint32_t val;
            int tz = COUNT_TZ(chunk);
            chunk >>= tz;
            rem0 -= tz + 1;
            num0 += tz;
            /*
             * Size of the length field
             */
            sz = Ceil_Log2(num0 + 1);
            /*
             * Adjusted length value to write
             */
            val = num0 - ((1 << sz) - 1);
            /*
             * Skip zeroes
             */
            rleSize += sz;
            SET_BIT8(packed, rleSize);
            ++rleSize;
            if ((rleSize + sz) > bv->len) {
                return DPS_ERR_OVERFLOW;
            }
            /*
             * Write length of the zero run - little endian
             */
            while (sz--) {
                if (val & 1) {
                    SET_BIT8(packed, rleSize);
                }
                val >>= 1;
                ++rleSize;
            }
            chunk >>= 1;
            num0 = 0;
        }
        num0 = rem0;
    }
    if (rleSize > bv->len) {
        return DPS_ERR_OVERFLOW;
    }
    buffer->txPos += (rleSize + 7) / 8;
    return DPS_OK;
}

#define TOP_UP_THRESHOLD 56

static DPS_Status RunLengthDecode(uint8_t* packed, size_t packedSize, chunk_t* bits, size_t len)
{
    uint64_t bitPos = 0;
    uint64_t current;
    size_t currentBits = 0;

    memzero_s(bits, len / 8);

    if (packedSize) {
        currentBits = 8;
        current = *packed++;
        --packedSize;
    }
    while (currentBits) {
        /*
         * Keep the current bits above the threshold where we are guaranteed
         * contiguous bits to decode the lengths below.
         */
        while (packedSize && (currentBits <= TOP_UP_THRESHOLD)) {
            current |= (((uint64_t)*packed++) << currentBits);
            currentBits += 8;
            --packedSize;
        }
        if (!current) {
            assert(packedSize == 0);
            break;
        }
        if (current & 1) {
            current >>= 1;
            --currentBits;
        } else {
            uint64_t val;
            uint64_t num0;
            int tz = COUNT_TZ(current);

            current >>= (tz + 1);
            /*
             * We can extract the length with a mask
             */
            val = current & (((uint64_t)1 << tz) - 1);
            /*
             * The value is little-endian so we may need to do an endian swap
             */
#ifdef ENDIAN_SWAP
            val = BSWAP_64(val);
#endif
            num0 = val + (((uint64_t)1 << tz) - 1);
            bitPos += num0;
            currentBits -= (1 + tz * 2);
            current >>= tz;
        }
        if (bitPos >= len) {
            return DPS_ERR_INVALID;
        }
        SET_BIT(bits, bitPos);
        ++bitPos;
    }
    return DPS_OK;
}

DPS_Status DPS_BitVectorSerializeFH(DPS_BitVector* bv, DPS_TxBuffer* buffer)
{
    assert(bv->len == FH_BITVECTOR_LEN);
#ifdef ENDIAN_SWAP
#error(TODO bit vector endian swapping not implemented)
#else
    return CBOR_EncodeBytes(buffer, (const uint8_t*)bv->bits, bv->len / 8);
#endif
}

DPS_Status DPS_BitVectorSerialize(DPS_BitVector* bv, DPS_TxBuffer* buffer)
{
    DPS_Status ret;
    uint8_t flags;
    float load = DPS_BitVectorLoadFactor(bv);

    /*
     * Bit vector is encoded as an array of 3 items
     * [
     *    flags (uint),
     *    bit vector length (uint)
     *    compressed bit vector (bstr)
     * ]
     */
    ret = CBOR_EncodeArray(buffer, 3);
    if (ret != DPS_OK) {
        return ret;
    }
    /*
     * The load factor will tell us if it is worth trying run length encoding and if
     * the bit complement will result in a more compact encoding.
     */
    if (load < 30.0) {
        flags = FLAG_RLE_ENCODED;
    } else if (load > 70.0) {
        flags = FLAG_RLE_ENCODED | FLAG_RLE_COMPLEMENT;
    } else{
        flags = 0;
    }
    while (1) {
        uint8_t* resetPos = buffer->txPos;
        ret = CBOR_EncodeUint(buffer, flags);
        if (ret != DPS_OK) {
            return ret;
        }
        ret = CBOR_EncodeUint(buffer, bv->len);
        if (ret != DPS_OK) {
            return ret;
        }
        if (flags & FLAG_RLE_ENCODED) {
            uint8_t* wrapPos;
            /*
             * Reserve space in the buffer
             */
            ret = CBOR_StartWrapBytes(buffer, bv->len / 8, &wrapPos);
            if (ret == DPS_OK) {
                ret = RunLengthEncode(bv, buffer, flags);
                if (ret == DPS_OK) {
                    ret = CBOR_EndWrapBytes(buffer, wrapPos);
                }
            } else if (ret == DPS_ERR_OVERFLOW) {
                /*
                 * Reset buffer and use raw encoding
                 */
                flags = 0;
                buffer->txPos = resetPos;
                continue;
            }
        } else {
#ifdef ENDIAN_SWAP
#error(TODO bit vector endian swapping not implemented)
#else
            ret = CBOR_EncodeBytes(buffer, (const uint8_t*)bv->bits, bv->len / 8);
#endif
        }
        break;
    }
    return ret;
}

size_t DPS_BitVectorSerializeMaxSize(DPS_BitVector* bv)
{
    return CBOR_SIZEOF_ARRAY(3) + CBOR_SIZEOF(uint8_t) + CBOR_SIZEOF(uint32_t) + CBOR_SIZEOF_BYTES(bv->len / 8);
}

size_t DPS_BitVectorSerializeFHSize(void)
{
    return CBOR_SIZEOF_BYTES(FH_BITVECTOR_LEN / 8);
}

DPS_Status DPS_BitVectorDeserializeFH(DPS_BitVector* bv, DPS_RxBuffer* buffer)
{
    uint8_t* data;
    size_t size;
    DPS_Status ret;

    assert(bv->len == FH_BITVECTOR_LEN);
    ret = CBOR_DecodeBytes(buffer, &data, &size);
    if (ret == DPS_OK) {
        if (size == bv->len / 8) {
            memcpy_s(bv->bits, size, data, size);
        } else {
            DPS_ERRPRINT("Deserialized fuzzy hash bit vector has wrong length\n");
            ret = DPS_ERR_INVALID;
        }
    }
    return ret;
}

DPS_Status DPS_BitVectorDeserialize(DPS_BitVector* bv, DPS_RxBuffer* buffer)
{
    DPS_Status ret;
    uint64_t flags;
    uint64_t len;
    size_t size;
    uint8_t* data;

    ret = CBOR_DecodeArray(buffer, &size);
    if (ret != DPS_OK) {
        return ret;
    }
    if (size != 3) {
        return DPS_ERR_INVALID;
    }
    ret = CBOR_DecodeUint(buffer, &flags);
    if (ret != DPS_OK) {
        return ret;
    }
    ret = CBOR_DecodeUint(buffer, &len);
    if (ret != DPS_OK) {
        return ret;
    }
    if (len != bv->len) {
        DPS_ERRPRINT("Deserialized bloom filter has wrong size\n");
        return DPS_ERR_INVALID;
    }
    ret = CBOR_DecodeBytes(buffer, &data, &size);
    if (ret != DPS_OK) {
        return ret;
    }
    if (flags & FLAG_RLE_ENCODED) {
        ret = RunLengthDecode(data, size, bv->bits, bv->len);
        if ((ret == DPS_OK) && (flags & FLAG_RLE_COMPLEMENT)) {
            DPS_BitVectorComplement(bv);
        }
    } else if (size == bv->len / 8) {
        memcpy_s(bv->bits, size, data, size);
    } else {
        DPS_ERRPRINT("Deserialized bloom filter has wrong length\n");
        ret = DPS_ERR_INVALID;
    }
    return ret;
}

void DPS_BitVectorFill(DPS_BitVector* bv)
{
    if (bv) {
        memset_s(bv->bits, bv->len / 8, 0xFF);
        bv->popCount = (uint32_t)bv->len;
    }
}

void DPS_BitVectorClear(DPS_BitVector* bv)
{
    if (bv->popCount != 0) {
        memzero_s(bv->bits, bv->len / 8);
        bv->popCount = 0;
    }
}

void DPS_BitVectorComplement(DPS_BitVector* bv)
{
    size_t i;
    for (i = 0; i < NUM_CHUNKS(bv); ++i) {
        bv->bits[i] = ~bv->bits[i];
    }
    if (bv->popCount) {
        bv->popCount = (uint32_t)bv->len - bv->popCount;
    }
}

static size_t RLE_Size(DPS_BitVector* bv)
{
    size_t i;
    size_t rleSize = 0;
    uint32_t num0 = 0;
    chunk_t complement = 0;
    float load = DPS_BitVectorLoadFactor(bv);

    if (load >= 30.0 && load <= 70.0) {
        return bv->len;
    }

    if (load > 70.0) {
        complement = ~complement;
    }

    for (i = 0; i < NUM_CHUNKS(bv); ++i) {
        uint32_t rem0;
        chunk_t chunk = bv->bits[i] ^ complement;
        if (!chunk) {
            num0 += CHUNK_SIZE;
            continue;
        }
        rem0 = CHUNK_SIZE;
        while (chunk) {
            size_t sz;
            int tz = COUNT_TZ(chunk);
            chunk >>= tz;
            rem0 -= tz + 1;
            num0 += tz;
            sz = Ceil_Log2(num0 + 1);
            rleSize += 1 + sz * 2;
            chunk >>= 1;
            num0 = 0;
        }
        num0 = rem0;
    }
    return rleSize;
}

DPS_Status DPS_BitVectorSet(DPS_BitVector* bv, uint8_t* data, size_t len)
{
    if (len != (bv->len / 8)) {
        return DPS_ERR_ARGS;
    } else {
        memcpy_s(bv->bits, len, data, len);
        INVALIDATE_POPCOUNT(bv);
        return DPS_OK;
    }
}

void DPS_BitVectorDump(DPS_BitVector* bv, int dumpBits)
{
    if (DPS_DEBUG_ENABLED()) {
        DPS_PRINT("Bit len = %zu, ", bv->len);
        DPS_PRINT("Pop = %zu, ", DPS_BitVectorPopCount((DPS_BitVector*)bv));
        DPS_PRINT("RLE bits = %zu, ", RLE_Size(bv));
        DPS_PRINT("Loading = %.2f%%\n", DPS_BitVectorLoadFactor((DPS_BitVector*)bv));
#ifdef DPS_DEBUG
        if (dumpBits) {
            CompressedBitDump(bv->bits, bv->len);
        }
#endif
    }
}

static DPS_CountVector* AllocCV(size_t sz)
{
    DPS_CountVector* cv;

    assert((sz % 64) == 0);
    cv = calloc(1, sizeof(DPS_CountVector) + ((sz / CHUNK_SIZE) - 1) * sizeof(counter_t));
    if (cv) {
        cv->len = sz;
    }
    return cv;
}

DPS_CountVector* DPS_CountVectorAlloc()
{
    DPS_CountVector* cv = AllocCV(config.bitLen);
    if (cv) {
        cv->bvUnion = AllocBV(config.bitLen);
        if (!cv->bvUnion) {
            free(cv);
            cv = NULL;
        }
    }
    return cv;
}

DPS_CountVector* DPS_CountVectorAllocFH()
{
    return AllocCV(FH_BITVECTOR_LEN);
}

void DPS_CountVectorFree(DPS_CountVector* cv)
{
    if (cv) {
        if (cv->bvUnion) {
            free(cv->bvUnion);
        }
        free(cv);
    }
}

DPS_Status DPS_CountVectorAdd(DPS_CountVector* cv, DPS_BitVector* bv)
{
    size_t i;

    if (!cv || !bv) {
        return DPS_ERR_NULL;
    }
    if (cv->entries == CV_MAX) {
        return DPS_ERR_RESOURCES;
    }
    if (bv->popCount != 0) {
        for (i = 0; i < NUM_CHUNKS(bv); ++i) {
            chunk_t chunk = bv->bits[i];
            if (chunk) {
                count_t* count = cv->counts[i];
                if (cv->bvUnion) {
                    cv->bvUnion->bits[i] |= chunk;
                }
                do {
                    if (chunk & 1) {
                        ++(*count);
                    }
                    chunk >>= 1;
                    ++count;
                } while (chunk);
            }
        }
        if (cv->bvUnion) {
            INVALIDATE_POPCOUNT(cv->bvUnion);
        }
    }
    ++cv->entries;
    return DPS_OK;
}

DPS_Status DPS_CountVectorDel(DPS_CountVector* cv, DPS_BitVector* bv)
{
    size_t i;

    if (!cv || !bv) {
        return DPS_ERR_NULL;
    }
    if (cv->entries == 0) {
        return DPS_ERR_ARGS;
    }
    if (bv->popCount != 0) {
        for (i = 0; i < NUM_CHUNKS(bv); ++i) {
            chunk_t chunk = bv->bits[i];
            if (chunk) {
                count_t* count = cv->counts[i];
                chunk_t bit = 1;
                chunk_t clear = 0;
                do {
                    if (chunk & 1) {
                        if (--(*count) == 0) {
                            clear |= bit;
                        }
                    }
                    bit <<= 1;
                    chunk >>= 1;
                    ++count;
                } while (chunk);
                if (cv->bvUnion) {
                    cv->bvUnion->bits[i] ^= clear;
                }
            }
        }
        if (cv->bvUnion) {
            INVALIDATE_POPCOUNT(cv->bvUnion);
        }
    }
    --cv->entries;
    return DPS_OK;
}

DPS_BitVector* DPS_CountVectorToUnion(DPS_CountVector* cv)
{
    if (!cv || !cv->bvUnion) {
        return NULL;
    } else {
        return DPS_BitVectorClone(cv->bvUnion);
    }
}

DPS_BitVector* DPS_CountVectorToIntersection(DPS_CountVector* cv)
{
    DPS_BitVector* bv = AllocBV(cv->len);
    if (bv && cv->entries) {
        size_t i;
        for (i = 0; i < NUM_CHUNKS(bv); ++i) {
            if (!cv->bvUnion || cv->bvUnion->bits[i]) {
                chunk_t b = 1;
                count_t* count = cv->counts[i];
                chunk_t chunk = 0;
                while (b) {
                    if (*count++ == cv->entries) {
                        chunk |= b;
                    }
                    b <<= 1;
                }
                bv->bits[i] = chunk;
            }
        }
    }
    return bv;
}

void DPS_CountVectorDump(DPS_CountVector* cv)
{
    size_t i;
    DPS_PRINT("Entries %zu\n", cv->entries);
    for (i = 0; i < NUM_CHUNKS(cv); ++i) {
        size_t j;
        for (j = 0; j < CHUNK_SIZE; ++j) {
            DPS_PRINT("%d ", cv->counts[i][j]);
        }
        DPS_PRINT("\n");
    }
}
