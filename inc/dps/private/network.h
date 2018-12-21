/**
 * @file
 * Network layer macros and functions
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

#ifndef _NETWORK_H
#define _NETWORK_H

#include <stdint.h>
#include <uv.h>
#include <dps/private/dps.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque data structure for network-specific state
 */
typedef struct _DPS_NetContext DPS_NetContext;

/**
 * Opaque type for managing connection state for connection-oriented transports
 */
typedef struct _DPS_NetConnection DPS_NetConnection;

/**
 * Type for a remote network endpoint. This provides an abstraction connectionless and
 * connection-oriented network layers.
 */
typedef struct _DPS_NetEndpoint {
    DPS_NodeAddress addr;   /**< The endpoint address */
    DPS_NetConnection* cn;  /**< The connection state or NULL for connectionless network layers */
} DPS_NetEndpoint;

/**
 * Opaque type for multicast receiver
 */
typedef struct _DPS_MulticastReceiver DPS_MulticastReceiver;

/**
 * Free uv buffer resources
 *
 * @param bufs     The buffers to free
 * @param numBufs  The number of buffers to free
 */
void DPS_NetFreeBufs(uv_buf_t* bufs, size_t numBufs);

/**
 * Function prototype for handler to be called on receiving data from a remote node
 *
 * @param node      The node that received the data
 * @param endpoint  The endpoint that received the data
 * @param status    Indicates if the receive was successful or there was a network layer error
 * @param data      The raw data
 * @param len       Length of the raw data
 *
 * @return
 * - DPS_OK if the message was correctly parsed
 * - An error code indicating the data received was invalid
 */
typedef DPS_Status (*DPS_OnReceive)(DPS_Node* node, DPS_NetEndpoint* endpoint, DPS_Status status, const uint8_t* data, size_t len);

/**
 * Set the port number on a network endpoint.
 *
 * This is only applied to connection-less endpoints since the sending
 * port may be ephemeral.  For connection endpoints, the sending port
 * forms part of the connection tuple and is untouched by this
 * function.
 *
 * @param endpoint  The endpoint to set
 * @param port      The port number to set on the endpoint
 */
void DPS_EndpointSetPort(DPS_NetEndpoint* endpoint, uint16_t port);

/**
 * Send a message locally, short-circuiting the transport layer.
 *
 * @param node The node sending and receiving the message
 * @param bufs Data buffers to send
 * @param numBufs Number of buffers to send
 *
 * @return DPS_OK if the send is successful, an error otherwise
 */
DPS_Status DPS_LoopbackSend(DPS_Node* node, uv_buf_t* bufs, size_t numBufs);

/**
 * Start receiving multicast data
 *
 * @param node     Opaque pointer to the DPS node
 * @param cb       Function prototype for handler to be called on receiving data from a remote node
 *
 * @return   An opaque pointer to the multicast receiver
 */
DPS_MulticastReceiver* DPS_MulticastStartReceive(DPS_Node* node, DPS_OnReceive cb);

/**
 * Stop receiving multicast data
 *
 * @param receiver   An opaque pointer to the multicast receiver
 */
void DPS_MulticastStopReceive(DPS_MulticastReceiver* receiver);

/**
 * Opaque type for multicast sender
 */
typedef struct _DPS_MulticastSender DPS_MulticastSender;

/**
 * Setup to enable sending multicast data
 *
 * @param node     Opaque pointer to the DPS node
 *
 * @return   An opaque pointer to a struct holding the state of the multicast sender.
 */
DPS_MulticastSender* DPS_MulticastStartSend(DPS_Node* node);

/**
 * Free resources used for sending multicast data
 *
 * @param sender   An opaque pointer to a struct holding the state of the multicast sender.
 *                 This will be free after this call and the pointer will no longer be valid.
 */
void DPS_MulticastStopSend(DPS_MulticastSender* sender);

/**
 * Prototype for function called when a send completes.
 *
 * @param sender   Opaque pointer to the struct holding the state of the multicast sender
 * @param appCtx   Application context pointer that was passed into DPS_MulticastSend()
 * @param bufs     Array holding pointers to the buffers passed in the send API call. The data in these buffers
 *                 can now be freed.
 * @param numBufs  The length of the bufs array
 * @param status   Indicates if the send was successful or not
 */
typedef void (*DPS_MulticastSendComplete)(DPS_MulticastSender* sender, void* appCtx, uv_buf_t* bufs, size_t numBufs, DPS_Status status);

/**
 * Multicast some data.
 *
 * @param sender          Opaque pointer to the multicast sender
 * @param appCtx          An application context to be passed to the send complete callback
 * @param bufs            Data buffers to send
 * @param numBufs         Number of buffers to send
 * @param sendCompleteCB  Function called when the send is complete so the content of the data buffers can be freed.
 *
 * @return
 * - DPS_OK if send is successful,
 * - DPS_ERR_NO_ROUTE if no interfaces are usable for multicast,
 * - an error otherwise
 */
DPS_Status DPS_MulticastSend(DPS_MulticastSender* sender, void* appCtx, uv_buf_t* bufs, size_t numBufs, DPS_MulticastSendComplete sendCompleteCB);

/**
 * Start listening and receiving data
 *
 * @param node  Opaque pointer to the DPS node
 * @param port  If non-zero the port number to listen on, if zero use an ephemeral port
 * @param cb    Function to call when data is received
 *
 * @return   Returns a pointer to an opaque data structure that holds the state of the netCtx.
 */
DPS_NetContext* DPS_NetStart(DPS_Node* node, uint16_t port, DPS_OnReceive cb);

/**
 * Get the port the netCtx is listening on
 *
 * @param netCtx  Pointer to an opaque data structure that holds the state of the netCtx.
 *
 * @return the port
 */
uint16_t DPS_NetGetListenerPort(DPS_NetContext* netCtx);

/**
 * Stop listening for data
 *
 * @param netCtx  Pointer to an opaque data structure that holds the network state.
 *                The netCtx will be freed and this pointer will be invalid after this call.
 */
void DPS_NetStop(DPS_NetContext* netCtx);

/**
 * Prototype for function called when a send completes.
 *
 * @param node     Opaque pointer to the DPS node
 * @param appCtx   Application context pointer that was passed into DPS_NetSend()
 * @param endpoint The endpoint for which the send was complete
 * @param bufs     Array holding pointers to the buffers passed in the send API call. The data in these buffers
 *                 can now be freed.
 * @param numBufs  The length of the bufs array
 * @param status   Indicates if the send was successful or not
 */
typedef void (*DPS_NetSendComplete)(DPS_Node* node, void* appCtx, DPS_NetEndpoint* endpoint, uv_buf_t* bufs, size_t numBufs, DPS_Status status);

/**
 * Send data to a specific endpoint.
 *
 * @param node            Pointer to the DPS node
 * @param appCtx          An application context to be passed to the send complete callback
 * @param endpoint        The endpoint to send to - note this may be updated with connection state
 *                        information.
 * @param bufs            Data buffers to send, the data in the buffers must be live until the send completes.
 * @param numBufs         Number of buffers to send
 * @param sendCompleteCB  Function called when the send is complete so the content of the data buffers can be freed.
 *
 * @return DPS_OK if the send is successful, an error otherwise
 */
DPS_Status DPS_NetSend(DPS_Node* node, void* appCtx, DPS_NetEndpoint* endpoint, uv_buf_t* bufs, size_t numBufs, DPS_NetSendComplete sendCompleteCB);

/**
 * Increment the reference count to potentially keeping a underlying connection alive. This is only
 * meaningful for connection-oriented transports.
 *
 * @param cn    Connection to be add'refd
 */
void DPS_NetConnectionAddRef(DPS_NetConnection* cn);

/**
 * Decrement the reference count on a connection potentially allowing an underlying connection to be
 * dropped. This is only meaningful for connection-oriented transports.
 *
 * @param cn    Connection to be dec'refd
 */
void DPS_NetConnectionDecRef(DPS_NetConnection* cn);

/**
 * Compare two addresses. This comparison handles the case of ipv6 mapped ipv4 address
 *
 * @param addr1  The address to compare against
 * @param addr2  The address to compare with addr1
 *
 * @return 0 if the addresses are different non-0 if they are the same
 */
int DPS_SameAddr(const DPS_NodeAddress* addr1, const DPS_NodeAddress* addr2);

/**
 * Generates text for an address
 *
 * @note This function uses a static string internally so is not thread-safe
 *
 * @param addr  The address to stringify.
 *
 * @return The text
 */
const char* DPS_NetAddrText(const struct sockaddr* addr);

/**
 * Maps the supplied address to a v6 address if needed.
 *
 * This is necessary when using dual-stack sockets.
 *
 * @param addr  The address
 */
void DPS_MapAddrToV6(struct sockaddr* addr);

#ifdef __cplusplus
}
#endif

#endif
