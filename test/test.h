/*
 *******************************************************************
 *
 * Copyright 2017 Intel Corporation All rights reserved.
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

#ifndef _TEST_H
#define _TEST_H

#include <safe_lib.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dps/dbg.h>
#include <dps/dps.h>
#include <dps/err.h>
#include <dps/event.h>
#include <dps/synchronous.h>
#include <dps/uuid.h>

#include <dps/private/cbor.h>
#include <dps/private/dps.h>

#ifdef _WIN32
#define SLEEP(t) Sleep(t)
#else
#include <unistd.h>
#define SLEEP(t) usleep((t) * 1000)
#endif

#define ASSERT(cond) do { assert(cond); if (!(cond)) exit(EXIT_FAILURE); } while (0)

int IntArg(char* opt, char*** argp, int* argcp, int* val, int min, int max);
int AddressArg(char* opt, char*** argp, int* argcp, DPS_NodeAddress** addr);

#endif
