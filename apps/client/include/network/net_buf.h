/**
 * @file net_buf.h
 * Network message handling and buffering. @ingroup network
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DE_NETWORK_BUFFER_H
#define DE_NETWORK_BUFFER_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Send Packet flags:
#define SPF_REBOUND     0x00020000 // Write only to local loopback
#define SPF_DONT_SEND   0x00040000 // Don't really send out anything

#define NETBUFFER_MAXSIZE    0x7ffff  // 512 KB

// Incoming messages are stored in netmessage_s structs.
typedef struct netmessage_s {
    struct netmessage_s *next;
    nodeid_t        sender;
    uint            player;        // Set in N_GetMessage().
    size_t          size;
    byte           *data;
    void           *handle;
    double          receivedAt;     // Time when received (seconds).
} netmessage_t;

#pragma pack(1)
typedef struct {
    byte            type;           // Type of the message.
    byte            data[NETBUFFER_MAXSIZE];
} netdata_t;
#pragma pack()

typedef struct netbuffer_s {
    int             player;         // Recipient or sender (can be NSP_BROADCAST).
    size_t          length;         // Number of bytes in the data buffer.
    size_t          headerLength;   // 1 byte at the moment.

    netdata_t       msg;            // The data buffer for sending and
                                    // receiving packets.
} netbuffer_t;

/**
 * Globally accessible data.
 */
extern netbuffer_t netBuffer;
extern dd_bool  allowSending;

/**
 * Constructs a new reader. The reader will use the engine's netBuffer
 * as the reading buffer. The reader has to be destroyed with Reader_Delete()
 * after it is not needed any more.
 */
Reader1* Reader_NewWithNetworkBuffer(void);

/*
 * Functions:
 */

/**
 * Initialize the low-level network subsystem. This is called always during startup (via
 * Sys_Init()).
 */
void N_Init(void);

/**
 * Shut down the low-level network interface. Called during engine shutdown (not before).
 */
void N_Shutdown(void);

/**
 * Empties the message buffers.
 */
void N_ClearMessages(void);

/**
 * Send the data in the netbuffer. The message is sent over a reliable and ordered
 * connection.
 *
 * Handles broadcasts using recursion.
 * Clients can only send packets to the server.
 */
void N_SendPacket(void);

/**
 * An attempt is made to extract a message from the message queue.
 *
 * @return  @c true if a message successful.
 */
dd_bool N_GetPacket(void);

/**
 * @return The player number that corresponds network node @a id.
 */
int N_IdentifyPlayer(nodeid_t id);

/**
 * Print low-level information about the network buffer.
 */
void N_PrintBufferInfo(void);

/**
 * Print status information about the workings of data compression in the network buffer.
 */
void N_PrintTransmissionStats(void);

/**
 * Adds the given netmessage_s to the queue of received messages. Uses a mutex to
 * synchronize access to the message queue.
 *
 * @note This is called in the network receiver thread.
 */
void N_PostMessage(netmessage_t *msg);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DE_NETWORK_BUFFER_H */
