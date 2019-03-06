/*
 *******************************************************************
 *
 * Copyright 2018 Intel Corporation All rights reserved.
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
#include <ctype.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <dps/dbg.h>
#include <dps/dps.h>
#include <dps/event.h>
#include <dps/synchronous.h>

#define MAX_LINKS  64

static const char* topic = "dps/roundtrip";
static DPS_Event* nodeDestroyed;
static DPS_Event* ackReceived;

static void OnNodeDestroyed(DPS_Node* node, void* data)
{
    DPS_SignalEvent(nodeDestroyed, DPS_OK);
}

#ifdef _WIN32
static int ElapsedMicroseconds(void)
{
    static LARGE_INTEGER freq = { 0 };
    static LARGE_INTEGER prev;
    LARGE_INTEGER now;
    LONGLONG elapsed;

    if (!freq.QuadPart) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&now);
    elapsed = now.QuadPart - prev.QuadPart;
    prev.QuadPart = now.QuadPart;
    return (int)(elapsed * 1000000 / freq.QuadPart);
}
#else
static int ElapsedMicroseconds(void)
{
    static struct timespec prev;
    uint64_t elapsed;
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsed = (now.tv_sec - prev.tv_sec) * 1000000000ull + (now.tv_nsec - prev.tv_nsec);
    prev = now;
    return (int)(elapsed / 1000);
}
#endif

static int rtTime = 0;

static void OnAck(DPS_Publication* pub, uint8_t* data, size_t len)
{
    rtTime = ElapsedMicroseconds();
    DPS_SignalEvent(ackReceived, DPS_OK);
}

static int IntArg(char* opt, char*** argp, int* argcp, int* val, int min, int max)
{
    char* p;
    char** arg = *argp;
    int argc = *argcp;

    if (strcmp(*arg++, opt) != 0) {
        return 0;
    }
    if (!--argc) {
        return 0;
    }
    *val = strtol(*arg++, &p, 10);
    if (*p) {
        return 0;
    }
    if (*val < min || *val > max) {
        DPS_PRINT("Value for option %s must be in range %d..%d\n", opt, min, max);
        return 0;
    }
    *argp = arg;
    *argcp = argc;
    return 1;
}

int main(int argc, char** argv)
{
    DPS_Status ret;
    DPS_Node* node;
    char** arg = argv + 1;
    const char* host = NULL;
    const char* linkPort[MAX_LINKS] = { NULL };
    const char* linkHosts[MAX_LINKS];
    int numLinks = 0;
    int numPubs = 1000; /* default */
    int i;
    int missingAcks = 0;
    uint8_t* payload = NULL;
    int payloadSize = 0;
    int mcast = DPS_MCAST_PUB_ENABLE_SEND;
    int listenPort = 0;
    DPS_NodeAddress* listenAddr = NULL;
    struct sockaddr_in6 saddr;
    DPS_NodeAddress* addr = NULL;
    DPS_Publication* pub = NULL;
    int rtMin = INT_MAX;
    int rtMax = 0;
    int64_t rtSum = 0;

    DPS_Debug = DPS_FALSE;
    while (--argc) {
        if (strcmp(*arg, "-d") == 0) {
            ++arg;
            DPS_Debug = DPS_TRUE;
            continue;
        }
        if (strcmp(*arg, "-p") == 0) {
            ++arg;
            if (!--argc) {
                goto Usage;
            }
            linkPort[numLinks] = *arg++;
            linkHosts[numLinks] = host;
            ++numLinks;
            continue;
        }
        if (strcmp(*arg, "-h") == 0) {
            ++arg;
            if (!--argc) {
                goto Usage;
            }
            host = *arg++;
            continue;
        }
        if (IntArg("-s", &arg, &argc, &payloadSize, 0,  UINT16_MAX)) {
            continue;
        }
        if (IntArg("-n", &arg, &argc, &numPubs, 1,  1000000)) {
            continue;
        }
    }
    /*
     * Disable multicast publications if we have an explicit destination
     */
    if (numLinks) {
        mcast = DPS_MCAST_PUB_DISABLED;
        addr = DPS_CreateAddress();
    }

    node = DPS_CreateNode("/", NULL, NULL);
    listenAddr = DPS_CreateAddress();
    if (!listenAddr) {
        DPS_ERRPRINT("DPS_CreateAddress failed: %s\n", DPS_ErrTxt(DPS_ERR_RESOURCES));
        return 1;
    }
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    saddr.sin6_port = htons(listenPort);
    memcpy(&saddr.sin6_addr, &in6addr_any, sizeof(saddr.sin6_addr));
    DPS_SetAddress(listenAddr, (const struct sockaddr*)&saddr);
    ret = DPS_StartNode(node, mcast, listenAddr);
    if (ret != DPS_OK) {
        DPS_ERRPRINT("DPS_StartNode failed: %s\n", DPS_ErrTxt(ret));
        return 1;
    }

    for (i = 0; i < numLinks; ++i) {
        ret = DPS_ResolveAddressSyn(node, linkHosts[i], linkPort[i], addr);
        if (ret != DPS_OK) {
            DPS_ERRPRINT("DPS_ResolveAddress %s:%s returned %s\n", linkHosts[i], linkPort[i], DPS_ErrTxt(ret));
            return 1;
        }
        ret = DPS_LinkTo(node, addr);
        if (ret != DPS_OK) {
            DPS_ERRPRINT("DPS_LinkTo %d returned %s\n", linkPort[i], DPS_ErrTxt(ret));
            return 1;
        }
    }

    nodeDestroyed = DPS_CreateEvent();
    ackReceived = DPS_CreateEvent();

    pub = DPS_CreatePublication(node);

    ret = DPS_InitPublication(pub, &topic, 1, DPS_FALSE, NULL, OnAck);
    if (ret != DPS_OK) {
        DPS_ERRPRINT("Failed to create publication - error=%s\n", DPS_ErrTxt(ret));
        return 1;
    }
    if (payloadSize) {
        payload = malloc(payloadSize);
    }
    for (i = 0; i <= numPubs; ++i) {
        // Initialize the round trip timer
        ElapsedMicroseconds();
        ret = DPS_Publish(pub, payload, payloadSize, 0);
        if (ret != DPS_OK) {
            DPS_ERRPRINT("Failed to publish topic - error=%s\n", DPS_ErrTxt(ret));
        }
        ret = DPS_TimedWaitForEvent(ackReceived, 1000);
        if (ret == DPS_OK) {
            if (i) {
                if (rtTime > rtMax) {
                    rtMax = rtTime;
                }
                if (rtTime < rtMin) {
                    rtMin = rtTime;
                }
                rtSum += rtTime;
            }
        } else {
            if (ret != DPS_ERR_TIMEOUT) {
                break;
            }
            DPS_ERRPRINT("Timeout waiting for ACK\n");
            ++missingAcks;
        }
    }
    printf("Total pub sent = %d, missing ACK's = %d, payload size %d\n", numPubs, missingAcks, payloadSize);
    printf("Min RT = %duS, Max RT = %duS, Avg RT %dus\n", rtMin, rtMax, (int)(rtSum / (numPubs - missingAcks)));

    DPS_DestroyNode(node, OnNodeDestroyed, NULL);

    DPS_WaitForEvent(nodeDestroyed);
    DPS_DestroyEvent(nodeDestroyed);
    DPS_DestroyAddress(listenAddr);
    return 0;

Usage:
    DPS_PRINT("Usage %s [-d] [-n <count>] [-p <portnum>] [-s <size>]\n", argv[0]);
    DPS_PRINT("       -d: Enable debug ouput if built for debug.\n");
    DPS_PRINT("       -n: Number of publications to send.\n");
    DPS_PRINT("       -p: port to link.\n");
    DPS_PRINT("       -s: Size of PUB payload.\n");
    return 1;
}


