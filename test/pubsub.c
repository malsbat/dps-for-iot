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
#include "bitvec.h"
#include "topics.h"

/*
 * A published topic has the form "A<sep>B<sep>C" where <sep> is any of a specified set of separators and
 * A, B, C are arbitrary strings or a standalone wild-card character "*"
 */
static void AddTopic(DPS_BitVector* filter, const char* topic)
{
    printf("AddTopic %s\n", topic);
    DPS_AddTopic(filter, topic, "/.", DPS_PubTopic);
    DPS_BitVectorDump(filter, DPS_TRUE);
}

#define NOT_EXPECT            0
#define EXPECT                1
#define EXPECT_FALSE_POSITIVE 2

static void SubscriptionCheck(DPS_BitVector* pubFilter, const char* subscription, int expect)
{
    if (DPS_MatchTopic(pubFilter, subscription, "/.")) {
        if (expect == EXPECT) {
            printf("Matched expected topic %s: PASS\n", subscription);
        } else if (expect == EXPECT_FALSE_POSITIVE) {
            printf("Matched expected (false positive) topic %s: PASS\n", subscription);
        } else {
            printf("Matched unexpected topic %s: FAIL\n", subscription);
            exit(EXIT_FAILURE);
        }
    } else {
        if (expect == EXPECT) {
            printf("No match for expected topic %s: FAIL\n", subscription);
            exit(EXIT_FAILURE);
        } else if (expect == EXPECT_FALSE_POSITIVE) {
            printf("No match for expected (false positive) topic %s: FAIL\n", subscription);
        } else {
            printf("No match for topic %s: PASS\n", subscription);
        }
    }
}

int main(int argc, char** argv)
{
    DPS_Status ret;
    char** arg = argv + 1;
    DPS_BitVector* pubFilter;
    size_t filterBits = 1024;
    size_t numHashes = 4;


    DPS_Debug = DPS_FALSE;
    while (--argc) {
        char* p;
        if (strcmp(*arg, "-b") == 0) {
            ++arg;
            if (!--argc) {
                goto Usage;
            }
            filterBits = strtol(*arg++, &p, 10);
            if (*p) {
                goto Usage;
            }
            continue;
        }
        if (strcmp(*arg, "-n") == 0) {
            ++arg;
            if (!--argc) {
                goto Usage;
            }
            numHashes = strtol(*arg++, &p, 10);
            if (*p) {
                goto Usage;
            }
            continue;
        }
        if (strcmp(*arg, "-d") == 0) {
            ++arg;
            DPS_Debug = DPS_TRUE;
            continue;
        }
        goto Usage;
    }

    ret = DPS_Configure(filterBits, numHashes);
    if (ret != DPS_OK) {
        DPS_ERRPRINT("Invalid configuration parameters\n");
        goto Usage;
    }

    pubFilter = DPS_BitVectorAlloc();
    AddTopic(pubFilter, "1");
    AddTopic(pubFilter, "x/y");
    AddTopic(pubFilter, "red");
    AddTopic(pubFilter, "blue");
    AddTopic(pubFilter, "foo");
    AddTopic(pubFilter, "foo/bar");
    AddTopic(pubFilter, "foo/baz");
    AddTopic(pubFilter, "foo/baz/gorn");
    AddTopic(pubFilter, "foo/baz/gorn.x");
    AddTopic(pubFilter, "foo/baz/gorn.y");
    AddTopic(pubFilter, "foo/baz/gorn.z");
    AddTopic(pubFilter, "goo/bar");
    AddTopic(pubFilter, "goo/bonzo/gronk");
    AddTopic(pubFilter, "1.0");
    AddTopic(pubFilter, "1.1");
    AddTopic(pubFilter, "1.2");
    AddTopic(pubFilter, "2.0");
    AddTopic(pubFilter, "a.b.c.1");
    AddTopic(pubFilter, "a.b.c.2");
    AddTopic(pubFilter, "a.b.c.3");
    AddTopic(pubFilter, "x.y.c.4");
    AddTopic(pubFilter, "x/y/z");
    AddTopic(pubFilter, "a/b/z");

    DPS_BitVectorDump(pubFilter, 1);

    SubscriptionCheck(pubFilter, "+", EXPECT);
    SubscriptionCheck(pubFilter, "#", EXPECT);
    SubscriptionCheck(pubFilter, "+/+", EXPECT);
    SubscriptionCheck(pubFilter, "foo/+/+.#", EXPECT);
    SubscriptionCheck(pubFilter, "foo/+/+/+/#", NOT_EXPECT);
    SubscriptionCheck(pubFilter, "+/baz", EXPECT);
    SubscriptionCheck(pubFilter, "+/+/gorn", EXPECT);
    SubscriptionCheck(pubFilter, "+/baz/gorn", EXPECT);
    SubscriptionCheck(pubFilter, "+/+/gorn.x", EXPECT);
    SubscriptionCheck(pubFilter, "red", EXPECT);
    SubscriptionCheck(pubFilter, "foo", EXPECT);
    SubscriptionCheck(pubFilter, "foo/bar", EXPECT);
    SubscriptionCheck(pubFilter, "foo/bar/*", NOT_EXPECT);
    SubscriptionCheck(pubFilter, "+/+/+.z", EXPECT);
    SubscriptionCheck(pubFilter, "foo/#", EXPECT);
    SubscriptionCheck(pubFilter, "+/gorn.blah", NOT_EXPECT);
    SubscriptionCheck(pubFilter, "goo/baz", NOT_EXPECT);
    SubscriptionCheck(pubFilter, "foo/+/gorn", EXPECT);
    SubscriptionCheck(pubFilter, "foo/+/+.x", EXPECT);
    SubscriptionCheck(pubFilter, "foo/baz/gorn.z/1", NOT_EXPECT);
    SubscriptionCheck(pubFilter, "goo/baz/gorn.z", NOT_EXPECT);
    SubscriptionCheck(pubFilter, "goo/+/gorn", EXPECT_FALSE_POSITIVE);
    SubscriptionCheck(pubFilter, "goo/+/+.x", EXPECT_FALSE_POSITIVE);
    SubscriptionCheck(pubFilter, "1.#", EXPECT);
    SubscriptionCheck(pubFilter, "2.#", EXPECT);
    SubscriptionCheck(pubFilter, "+.0", EXPECT);
    SubscriptionCheck(pubFilter, "+.1", EXPECT);
    SubscriptionCheck(pubFilter, "+.2", EXPECT);
    SubscriptionCheck(pubFilter, "2.1", NOT_EXPECT);
    SubscriptionCheck(pubFilter, "2.2", NOT_EXPECT);
    SubscriptionCheck(pubFilter, "x.y.c.1", NOT_EXPECT);
    SubscriptionCheck(pubFilter, "a.b.c.4", NOT_EXPECT);
    SubscriptionCheck(pubFilter, "x/b/#", NOT_EXPECT);
    SubscriptionCheck(pubFilter, "+.+.c.5", NOT_EXPECT);
    SubscriptionCheck(pubFilter, "1", EXPECT);
    SubscriptionCheck(pubFilter, "2", NOT_EXPECT);

    DPS_BitVectorFree(pubFilter);
    return EXIT_SUCCESS;

Usage:
    DPS_PRINT("Usage %s: [-r] [-b <filter-bits>] [-n <num-hashes>]\n", argv[0]);
    return EXIT_FAILURE;
}
