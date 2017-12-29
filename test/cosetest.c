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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "test.h"
#include <dps/dbg.h>
#include <dps/err.h>
#include <dps/private/dps.h>
#include "ccm.h"
#include "cose.h"
#include "ec.h"

/*
 * Debug control for this module
 */
DPS_DEBUG_CONTROL(DPS_DEBUG_ON);

static const uint8_t msg[] = {
   0x82,0x81,0x66,0x61,0x2f,0x62,0x2f,0x63,0x00,0x40
};

static const uint8_t aad[] = {
    0xa5,0x03,0x00,0x04,0x50,0xb8,0x5e,0x9a,0xdd,0xd5,0x55,0x88,0xc4,0x57,0xbd,0x01,
    0x19,0x77,0x71,0xa9,0x2a,0x05,0x01,0x06,0xf4,0x07,0x83,0x01,0x19,0x20,0x00,0x58,
    0x2d,0x00,0xbc,0x0d,0x88,0x02,0x09,0x00,0xd1,0x83,0x0a,0xa0,0x33,0x50,0x07,0x6c,
    0x00,0xc2,0x41,0x0d,0x46,0x00,0x19,0x01,0x39,0x58,0x00,0x5a,0x00,0xf0,0x12,0x6c,
    0x00,0x1f,0x01,0xc6,0x00,0x4a,0x00,0xd6,0x00,0x06,0x81,0x19,0x20,0x3d
};

static const uint8_t nonce[] = {
    0x01,0x00,0x00,0x00,0x38,0x5e,0x9a,0xdd,0xd5,0x55,0x88,0xc4,0x57
};

static const uint8_t key[] = {
    0x77,0x58,0x22,0xfc,0x3d,0xef,0x48,0x88,0x91,0x25,0x78,0xd0,0xe2,0x74,0x5c,0x10
};

static DPS_UUID keyId = {
    .val= { 0xed,0x54,0x14,0xa8,0x5c,0x4d,0x4d,0x15,0xb6,0x9f,0x0e,0x99,0x8a,0xb1,0x71,0xf2 }
};

static void Dump(const char* tag, const uint8_t* data, size_t len)
{
    size_t i;
    printf("%s:", tag);
    for (i = 0; i < len; ++i) {
        if ((i % 16) == 0)  {
            printf("\n");
        }
        printf("%02x", data[i]);
    }
    printf("\n");
}

uint8_t config[] = {
    AES_CCM_16_64_128,
    AES_CCM_16_128_128
};

static DPS_Status GetKey(void* ctx, const DPS_UUID* kid, int8_t alg, uint8_t* k)
{
    if (DPS_UUIDCompare(kid, &keyId) != 0) {
        ASSERT(0);
        return DPS_ERR_MISSING;
    } else {
        memcpy(k, key, sizeof(key));
        return DPS_OK;
    }
}

static void CCM_Raw()
{
    DPS_Status ret;
    DPS_TxBuffer cipherText;
    DPS_TxBuffer plainText;

    DPS_TxBufferInit(&cipherText, NULL, 512);
    DPS_TxBufferInit(&plainText, NULL, 512);

    ret = Encrypt_CCM(key, 16, 2, nonce, (uint8_t*)msg, sizeof(msg), aad, sizeof(aad), &cipherText);
    ASSERT(ret == DPS_OK);
    ret = Decrypt_CCM(key, 16, 2, nonce, cipherText.base, DPS_TxBufferUsed(&cipherText), aad, sizeof(aad), &plainText);
    ASSERT(ret == DPS_OK);

    ASSERT(DPS_TxBufferUsed(&plainText) == sizeof(msg));
    ASSERT(memcmp(plainText.base, msg, sizeof(msg)) == 0);

    DPS_TxBufferFree(&cipherText);
    DPS_TxBufferFree(&plainText);
}

static void ECDSA_VerifyCurve(int crv, uint8_t* x, uint8_t* y, uint8_t* d, uint8_t* data, size_t dataLen)
{
    DPS_Status ret;
    DPS_TxBuffer buf;

    DPS_TxBufferInit(&buf, NULL, 512);
    ret = Sign_ECDSA(crv, d, data, sizeof(data), &buf);
    ASSERT(ret == DPS_OK);

    ret = Verify_ECDSA(crv, x, y, data, sizeof(data), buf.base, DPS_TxBufferUsed(&buf));
    ASSERT(ret == DPS_OK);

    DPS_TxBufferFree(&buf);
}

static void ECDSA_Raw()
{
    DPS_Status ret;

    uint8_t data[] = {
        0x85, 0x70, 0x43, 0x6F, 0x75, 0x6E, 0x74, 0x65, 0x72, 0x53, 0x69, 0x67, 0x6E, 0x61, 0x74, 0x75,
        0x72, 0x65, 0x43, 0xA1, 0x01, 0x01, 0x44, 0xA1, 0x01, 0x38, 0x23, 0x40, 0x58, 0x24, 0x7A, 0xDB,
        0xE2, 0x70, 0x9C, 0xA8, 0x18, 0xFB, 0x41, 0x5F, 0x1E, 0x5D, 0xF6, 0x6F, 0x4E, 0x1A, 0x51, 0x05,
        0x3B, 0xA6, 0xD6, 0x5A, 0x1A, 0x0C, 0x52, 0xA3, 0x57, 0xDA, 0x7A, 0x64, 0x4B, 0x80, 0x70, 0xA1,
        0x51, 0xB0
    };

    {
        int crv = EC_CURVE_P256;
        uint8_t x[] = {
            0xba, 0xc5, 0xb1, 0x1c, 0xad, 0x8f, 0x99, 0xf9, 0xc7, 0x2b, 0x05, 0xcf, 0x4b, 0x9e, 0x26, 0xd2,
            0x44, 0xdc, 0x18, 0x9f, 0x74, 0x52, 0x28, 0x25, 0x5a, 0x21, 0x9a, 0x86, 0xd6, 0xa0, 0x9e, 0xff
        };
        uint8_t y[] = {
            0x20, 0x13, 0x8b, 0xf8, 0x2d, 0xc1, 0xb6, 0xd5, 0x62, 0xbe, 0x0f, 0xa5, 0x4a, 0xb7, 0x80, 0x4a,
            0x3a, 0x64, 0xb6, 0xd7, 0x2c, 0xcf, 0xed, 0x6b, 0x6f, 0xb6, 0xed, 0x28, 0xbb, 0xfc, 0x11, 0x7e
        };
        uint8_t d[] = {
            0x57, 0xc9, 0x20, 0x77, 0x66, 0x41, 0x46, 0xe8, 0x76, 0x76, 0x0c, 0x95, 0x20, 0xd0, 0x54, 0xaa,
            0x93, 0xc3, 0xaf, 0xb0, 0x4e, 0x30, 0x67, 0x05, 0xdb, 0x60, 0x90, 0x30, 0x85, 0x07, 0xb4, 0xd3
        };
        ECDSA_VerifyCurve(crv, x, y, d, data, sizeof(data));
    }
    {
        int crv = EC_CURVE_P384;
        uint8_t x[] = {
            0x91, 0x32, 0x72, 0x3f, 0x62, 0x92, 0xb0, 0x10, 0x61, 0x9d, 0xbe, 0x24, 0x8d, 0x69, 0x8c, 0x17,
            0xb5, 0x87, 0x56, 0xc6, 0x39, 0xe7, 0x15, 0x0f, 0x81, 0xbe, 0xe4, 0xeb, 0x8a, 0xc3, 0x72, 0x36,
            0xad, 0x0a, 0x1a, 0x19, 0xd6, 0x7b, 0xe3, 0x2a, 0x66, 0x26, 0x3e, 0x1e, 0x52, 0x4d, 0x12, 0x9c
        };
        uint8_t y[] = {
            0x98, 0xcd, 0x30, 0x78, 0xc5, 0x54, 0xd8, 0x32, 0xac, 0x60, 0x3c, 0x43, 0x26, 0x41, 0x0f, 0xf6,
            0x16, 0x62, 0x45, 0x9b, 0x41, 0xf1, 0xf3, 0xdf, 0x5d, 0xbc, 0xc8, 0x35, 0x98, 0xff, 0x7c, 0x5e,
            0xd8, 0x41, 0x1c, 0xa7, 0x35, 0x67, 0x9d, 0x1c, 0x4c, 0xb3, 0x00, 0x93, 0x97, 0xd9, 0xef, 0x2c
        };
        uint8_t d[] = {
            0xa2, 0x4d, 0xcd, 0xab, 0xde, 0xc0, 0x5e, 0x5a, 0x44, 0xba, 0xc3, 0xbb, 0x8c, 0x8c, 0xb5, 0x15,
            0x90, 0x13, 0x94, 0x13, 0xfd, 0x3c, 0xd4, 0x5e, 0x31, 0x4e, 0xc3, 0x59, 0xb9, 0x0b, 0x43, 0x97,
            0x54, 0xf7, 0x4b, 0x27, 0x1e, 0xeb, 0x87, 0x54, 0x38, 0xc4, 0x3e, 0x6b, 0x55, 0xd1, 0xf4, 0xe8
        };
        ECDSA_VerifyCurve(crv, x, y, d, data, sizeof(data));
    }
    {
        int crv = EC_CURVE_P521;
        uint8_t x[] = {
            0x00, 0x72, 0x99, 0x2c, 0xb3, 0xac, 0x08, 0xec, 0xf3, 0xe5, 0xc6, 0x3d, 0xed, 0xec, 0x0d, 0x51,
            0xa8, 0xc1, 0xf7, 0x9e, 0xf2, 0xf8, 0x2f, 0x94, 0xf3, 0xc7, 0x37, 0xbf, 0x5d, 0xe7, 0x98, 0x66,
            0x71, 0xea, 0xc6, 0x25, 0xfe, 0x82, 0x57, 0xbb, 0xd0, 0x39, 0x46, 0x44, 0xca, 0xaa, 0x3a, 0xaf,
            0x8f, 0x27, 0xa4, 0x58, 0x5f, 0xbb, 0xca, 0xd0, 0xf2, 0x45, 0x76, 0x20, 0x08, 0x5e, 0x5c, 0x8f,
            0x42, 0xad
        };
        uint8_t y[] = {
            0x01, 0xdc, 0xa6, 0x94, 0x7b, 0xce, 0x88, 0xbc, 0x57, 0x90, 0x48, 0x5a, 0xc9, 0x74, 0x27, 0x34,
            0x2b, 0xc3, 0x5f, 0x88, 0x7d, 0x86, 0xd6, 0x5a, 0x08, 0x93, 0x77, 0xe2, 0x47, 0xe6, 0x0b, 0xaa,
            0x55, 0xe4, 0xe8, 0x50, 0x1e, 0x2a, 0xda, 0x57, 0x24, 0xac, 0x51, 0xd6, 0x90, 0x90, 0x08, 0x03,
            0x3e, 0xbc, 0x10, 0xac, 0x99, 0x9b, 0x9d, 0x7f, 0x5c, 0xc2, 0x51, 0x9f, 0x3f, 0xe1, 0xea, 0x1d,
            0x94, 0x75
        };
        uint8_t d[] = {
            0x00, 0x08, 0x51, 0x38, 0xdd, 0xab, 0xf5, 0xca, 0x97, 0x5f, 0x58, 0x60, 0xf9, 0x1a, 0x08, 0xe9,
            0x1d, 0x6d, 0x5f, 0x9a, 0x76, 0xad, 0x40, 0x18, 0x76, 0x6a, 0x47, 0x66, 0x80, 0xb5, 0x5c, 0xd3,
            0x39, 0xe8, 0xab, 0x6c, 0x72, 0xb5, 0xfa, 0xcd, 0xb2, 0xa2, 0xa5, 0x0a, 0xc2, 0x5b, 0xd0, 0x86,
            0x64, 0x7d, 0xd3, 0xe2, 0xe6, 0xe9, 0x9e, 0x84, 0xca, 0x2c, 0x36, 0x09, 0xfd, 0xf1, 0x77, 0xfe,
            0xb2, 0x6d
        };
        uint8_t sig[] = {
            0x00, 0x92, 0x96, 0x63, 0xc8, 0x78, 0x9b, 0xb2, 0x81, 0x77, 0xae, 0x28, 0x46, 0x7e, 0x66, 0x37,
            0x7d, 0xa1, 0x23, 0x02, 0xd7, 0xf9, 0x59, 0x4d, 0x29, 0x99, 0xaf, 0xa5, 0xdf, 0xa5, 0x31, 0x29,
            0x4f, 0x88, 0x96, 0xf2, 0xb6, 0xcd, 0xf1, 0x74, 0x00, 0x14, 0xf4, 0xc7, 0xf1, 0xa3, 0x58, 0xe3,
            0xa6, 0xcf, 0x57, 0xf4, 0xed, 0x6f, 0xb0, 0x2f, 0xcf, 0x8f, 0x7a, 0xa9, 0x89, 0xf5, 0xdf, 0xd0,
            0x7f, 0x07, 0x00, 0xa3, 0xa7, 0xd8, 0xf3, 0xc6, 0x04, 0xba, 0x70, 0xfa, 0x94, 0x11, 0xbd, 0x10,
            0xc2, 0x59, 0x1b, 0x48, 0x3e, 0x1d, 0x2c, 0x31, 0xde, 0x00, 0x31, 0x83, 0xe4, 0x34, 0xd8, 0xfb,
            0xa1, 0x8f, 0x17, 0xa4, 0xc7, 0xe3, 0xdf, 0xa0, 0x03, 0xac, 0x1c, 0xf3, 0xd3, 0x0d, 0x44, 0xd2,
            0x53, 0x3c, 0x49, 0x89, 0xd3, 0xac, 0x38, 0xc3, 0x8b, 0x71, 0x48, 0x1c, 0xc3, 0x43, 0x0c, 0x9d,
            0x65, 0xe7, 0xdd, 0xff
        };
        ret = Verify_ECDSA(crv, x, y, data, sizeof(data), sig, sizeof(sig));
        ASSERT(ret == DPS_OK);
        ECDSA_VerifyCurve(crv, x, y, d, data, sizeof(data));
    }
}

int main(int argc, char** argv)
{
    DPS_Status ret;
    int i;

    DPS_Debug = 1;

    CCM_Raw();
    ECDSA_Raw();

    for (i = 0; i < sizeof(config); ++i) {
        uint8_t alg = config[i];
        DPS_UUID kid;
        DPS_RxBuffer aadBuf;
        DPS_RxBuffer msgBuf;
        DPS_TxBuffer cipherText;
        DPS_TxBuffer plainText;
        DPS_RxBuffer input;

        DPS_RxBufferInit(&aadBuf, (uint8_t*)aad, sizeof(aad));
        DPS_RxBufferInit(&msgBuf, (uint8_t*)msg, sizeof(msg));

        ret = COSE_Encrypt(alg, &keyId, nonce, &aadBuf, &msgBuf, GetKey, NULL, &cipherText);
        if (ret != DPS_OK) {
            DPS_ERRPRINT("COSE_Encrypt failed: %s\n", DPS_ErrTxt(ret));
            return EXIT_FAILURE;
        }
        Dump("CipherText", cipherText.base, DPS_TxBufferUsed(&cipherText));
        /*
         * Turn output buffers into input buffers
         */
        DPS_TxBufferToRx(&cipherText, &input);

        DPS_RxBufferInit(&aadBuf, (uint8_t*)aad, sizeof(aad));

        ret = COSE_Decrypt(nonce, &kid, &aadBuf, &input, GetKey, NULL, &plainText);
        if (ret != DPS_OK) {
            DPS_ERRPRINT("COSE_Decrypt failed: %s\n", DPS_ErrTxt(ret));
            return EXIT_FAILURE;
        }

        ASSERT(DPS_TxBufferUsed(&plainText) == sizeof(msg));
        ASSERT(memcmp(plainText.base, msg, sizeof(msg)) == 0);

        DPS_TxBufferFree(&cipherText);
        DPS_TxBufferFree(&plainText);
    }

    DPS_PRINT("Passed\n");
    return EXIT_SUCCESS;
}
