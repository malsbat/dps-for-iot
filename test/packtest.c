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

#include "test.h"
#include "topics.h"

static uint8_t packed[10000];

#define NUM_TESTS 13

static DPS_Status InitBitVector(DPS_BitVector* bf, size_t len, int testCase)
{
    DPS_Status ret = DPS_OK;
    size_t i;
    uint8_t* data;

    data = malloc(len);
    switch (testCase) {
    case 0:
        memset(data, 0, len);
        ret = DPS_BitVectorSet(bf, data, len);
        break;
    case 1:
        memset(data, 0xFF, len);
        ret = DPS_BitVectorSet(bf, data, len);
        break;
    case 2:
        memset(data, 0x55, len);
        ret = DPS_BitVectorSet(bf, data, len);
        break;
    case 3:
        memset(data, 0xAA, len);
        ret = DPS_BitVectorSet(bf, data, len);
        break;
    case 4:
        memset(data, 0xFF, len);
        data[len - 1] = 0x7F;
        ret = DPS_BitVectorSet(bf, data, len);
        break;
    case 5:
        memset(data, 0, len);
        data[len - 1] = 0x80;
        ret = DPS_BitVectorSet(bf, data, len);
        break;
    case 6:
        memset(data, 0, len);
        memset(data, 0x55, len / 2);
        ret = DPS_BitVectorSet(bf, data, len);
        break;
    case 7:
        memset(data, 0x55, len);
        memset(data, 0, len / 2);
        ret = DPS_BitVectorSet(bf, data, len);
        break;
    case 8:
        memset(data, 0xCC, len);
        ret = DPS_BitVectorSet(bf, data, len);
        break;
    case 9:
        for (i = 0; i < len; ++i) {
            data[i] = (uint8_t) i;
        }
        ret = DPS_BitVectorSet(bf, data, len);
        break;
    case 10:
        ret = DPS_AddTopic(bf, "a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v", ".", DPS_PubTopic);
        break;
    case 11:
        ret |= DPS_AddTopic(bf, "foo.bar.y", ".", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "red", "/", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "blue", "/", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "green", "/", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "foo", "/", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "foo/bar", "/", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "foo/baz", "/", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "foo/baz/gorn", "/", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "foo/baz/gorn.x", "/.", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "foo/baz/gorn.y", "/.", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "foo/baz/gorn.z", "/.", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "razz/baz/x=1", "/=", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "razz/baz/x=2", "/=", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "razz/baz/x=3", "/=", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "razz/baz/x=4", "/=", DPS_PubTopic);
        ret |= DPS_AddTopic(bf, "razz/baz/x=5", "/=", DPS_PubTopic);
        break;
    case 12:
        ret |= DPS_AddTopic(bf, "foo.bar.y", ".", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "red", "/", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "blue", "/", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "green", "/", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "foo", "/", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "foo/bar", "/", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "foo/baz", "/", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "foo/baz/gorn", "/", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "foo/baz/gorn.x", "/.", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "foo/baz/gorn.y", "/.", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "foo/baz/gorn.z", "/.", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "razz/baz/x=1", "/=", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "razz/baz/x=2", "/=", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "razz/baz/x=3", "/=", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "razz/baz/x=4", "/=", DPS_SubTopic);
        ret |= DPS_AddTopic(bf, "razz/baz/x=5", "/=", DPS_SubTopic);
        break;
    }
    ASSERT(ret == DPS_OK);
    free(data);
    DPS_BitVectorDump(bf, 1);
    return ret;
}


static void RunTests(DPS_BitVector* pubBf, size_t size)
{
    int i;
    int cmp;

    for (i = 0; i < NUM_TESTS; ++i) {
        DPS_Status ret;
        DPS_RxBuffer rxBuf;
        DPS_TxBuffer txBuf;
        DPS_BitVector* bf;

        DPS_TxBufferInit(&txBuf, packed, sizeof(packed));

        InitBitVector(pubBf, size, i);

        ret = DPS_BitVectorSerialize(pubBf, &txBuf);
        ASSERT(ret == DPS_OK);
        /*
         * Switch over from writing to reading
         */
        DPS_TxBufferToRx(&txBuf, &rxBuf);

        bf = DPS_BitVectorAlloc();
        ret = DPS_BitVectorDeserialize(bf, &rxBuf);
        ASSERT(ret == DPS_OK);

        DPS_BitVectorDump(bf, 1);

        cmp = DPS_BitVectorEquals(bf, pubBf);
        ASSERT(cmp == 1);

        DPS_BitVectorFree(bf);
        DPS_BitVectorClear(pubBf);
    }
}

int main(int argc, char** argv)
{
    DPS_Status ret;
    DPS_BitVector* bf;
    size_t filterBits = (argc > 1) ? atoi(argv[1]) : 256;
    size_t numHashes = (argc > 2) ? atoi(argv[2]) : 4;

    if (filterBits <= 0) {
        printf("Usage %s: <filter-bits> [<num-hashes>]\n", argv[0]);
        return EXIT_FAILURE;
    }

    ret = DPS_Configure(filterBits, numHashes);
    if (ret != DPS_OK) {
        DPS_ERRPRINT("Invalid configuration parameters\n");
        return EXIT_FAILURE;
    }

    bf = DPS_BitVectorAlloc();
    RunTests(bf, filterBits / 8);
    DPS_BitVectorFree(bf);
    return EXIT_SUCCESS;
}
