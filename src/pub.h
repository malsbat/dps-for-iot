/**
 * @file
 * Send and receive publication messages
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

#ifndef _PUB_H
#define _PUB_H

#include <stdint.h>
#include <stddef.h>
#include <dps/private/dps.h>
#include "node.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PUB_FLAG_PUBLISH   (0x01) /**< The publication should be published */
#define PUB_FLAG_LOCAL     (0x02) /**< The publication is local to this node */
#define PUB_FLAG_RETAINED  (0x04) /**< The publication had a non-zero TTL */
#define PUB_FLAG_EXPIRED   (0x10) /**< The publication had a negative TTL */
#define PUB_FLAG_WAS_FREED (0x20) /**< The publication has been freed but has a non-zero ref count */
#define PUB_FLAG_IS_COPY   (0x80) /**< This publication is a copy and can only be used for acknowledgements */

/**
 * Shared fields between members of a publication data series
 */
typedef struct _PublicationShared {
    void* userData;                 /**< Application provided user data */
    uint8_t ackRequested;           /**< TRUE if an ack was requested by the publisher */
    DPS_AcknowledgementHandler handler; /**< Called when an acknowledgement is received from a subscriber */
    DPS_UUID pubId;                 /**< Publication identifier */
    COSE_Entity* recipients;        /**< Publication recipient IDs */
    size_t recipientsCount;         /**< Number of valid elements in recipients array */
    size_t recipientsCap;           /**< Capacity of recipients array */
    DPS_Node* node;                 /**< Node for this publication */
    DPS_BitVector* bf;              /**< The Bloom filter bit vector for the topics for this publication */
    DPS_TxBuffer bfBuf;             /**< Pre-serialized bloom filter */
    char** topics;                  /**< Publication topics - pointers into topicsBuf */
    size_t numTopics;               /**< Number of publication topics */
    DPS_TxBuffer topicsBuf;         /**< Pre-serialized topic strings */
    uint32_t refCount;              /**< Ref count to prevent shared fields from being freed while in use */

    COSE_Entity sender;             /**< Publication sender ID */
    DPS_NodeAddress senderAddr;     /**< For retained messages - the sender address */
    COSE_Entity ack;                /**< For ack messages - the ack sender ID */
} PublicationShared;

/**
 * Notes on the use of the DPS_Publication fields:
 *
 * The pubId identifies a publication that replaces an earlier
 * retained instance of the same publication.
 *
 * The ttl starts when a publication is first published. It may expire
 * before the publication is ever sent.  If a publication received by
 * a subscriber has a non-zero ttl is will be retained for later
 * publication until the ttl expires or it is explicitly expired.
 */
typedef struct _DPS_Publication {
    PublicationShared* shared;      /**< Shared fields between members of a publication data series */
    uint8_t flags;                  /**< Internal state flags */
    uint8_t checkToSend;            /**< TRUE if this publication should be checked to send */
    uint8_t numSend;                /**< Number of pending network sends */
    uint32_t refCount;              /**< Ref count to prevent publication from being free while a send is in progress */
    uint32_t sequenceNum;           /**< Sequence number for this publication */
    uint64_t expires;               /**< Time (in milliseconds) that this publication expires */

    DPS_TxBuffer protectedBuf;      /**< Authenticated fields */
    DPS_TxBuffer encryptedBuf;      /**< Encrypted fields */
    DPS_Publication* history;       /**< History of data in this series */
    size_t historyCount;            /**< Number of data in history */
    size_t historyCap;              /**< Maximum number of data in history */
    DPS_Publication* next;          /**< Next publication in list */
} DPS_Publication;

/**
 * Time-to-live in seconds of a publication
 */
#define PUB_TTL(node, pub)  (int16_t)((pub->expires + 999 - uv_now((node)->loop)) / 1000)

/**
 * Run checks of one or more publications against the current subscriptions
 *
 * @param node       The local node
 * @param pub        The publication, may be NULL
 */
void DPS_UpdatePubs(DPS_Node* node, DPS_Publication* pub);

/**
 * Decode and process a received publication
 *
 * @param node       The local node
 * @param ep         The endpoint the publication was received on
 * @param buffer     The encoded publication
 * @param multicast  DPS_TRUE if publication was multicast, DPS_FALSE if unicast
 *
 * @return DPS_OK if decoding and processing is successful, an error otherwise
 */
DPS_Status DPS_DecodePublication(DPS_Node* node, DPS_NetEndpoint* ep, DPS_RxBuffer* buffer, int multicast);

/**
 * Multicast a publication or send it directly to a remote subscriber node
 *
 * @param node      The local node
 * @param pub       The publication to send
 * @param remote    The remote node to send the publication to, NULL for multicast
 * @param loopback  DPS_TRUE if publication should be looped back
 *
 * @return DPS_OK if sending is successful, an error otherwise
 */
DPS_Status DPS_SendPublication(DPS_Node* node, DPS_Publication* pub, RemoteNode* remote, int loopback);

/**
 * When a ttl expires retained publications are freed, local
 * publications are disabled by clearing the PUBLISH flag.
 *
 * @param node The node
 * @param pub The publication
 */
void DPS_ExpirePub(DPS_Node* node, DPS_Publication* pub);

/**
 * Serialize the body and payload sections of a publication
 *
 * The topic strings and bloom filter have already been serialized into buffers in
 * the publication structure,
 *
 * @param node The node
 * @param pub The publication
 * @param data The payload
 * @param dataLen The number of payload bytes
 * @param ttl The time-to-live of the publication
 *
 * @return DPS_OK if the serialization is successful, an error otherwise
 */
DPS_Status DPS_SerializePub(DPS_Node* node, DPS_Publication* pub, const uint8_t* data, size_t dataLen, int16_t ttl);

/**
 * Free publications of node
 *
 * @param node The node
 */
void DPS_FreePublications(DPS_Node* node);

/**
 * Increase a publication's refcount to prevent it from being freed
 * from inside a callback function
 *
 * @param pub The publication
 */
void DPS_PublicationIncRef(DPS_Publication* pub);

/**
 * Decrease a publication's refcount to allow it to be freed after
 * returning from a callback function
 *
 * @param pub The publication
 */
void DPS_PublicationDecRef(DPS_Publication* pub);

/**
 * Print publications of node
 *
 * @param node The node
 */
#ifdef DPS_DEBUG
void DPS_DumpPubs(DPS_Node* node);
#else
#define DPS_DumpPubs(node)
#endif

#ifdef __cplusplus
}
#endif

#endif
