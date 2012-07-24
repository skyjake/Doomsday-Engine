/**
 * @file net_buf.h
 * Network message handling and buffering. @ingroup network
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_NETWORK_BUFFER_H
#define LIBDENG_NETWORK_BUFFER_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Send Packet flags:
#define SPF_REBOUND     0x00020000 // Write only to local loopback
#define SPF_DONT_SEND   0x00040000 // Don't really send out anything

#define NETBUFFER_MAXSIZE    0x7ffff  // 512 KB

// Each network node is identified by a number.
typedef unsigned int nodeid_t;

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
    int             player;         // Recipient or sender.
    size_t          length;         // Number of bytes in the data buffer.
    size_t          headerLength;   // 1 byte at the moment.

    netdata_t       msg;            // The data buffer for sending and
                                    // receiving packets.
} netbuffer_t;

/**
 * Globally accessible data.
 */
extern netbuffer_t netBuffer;
extern boolean  allowSending;

/**
 * Constructs a new reader. The reader will use the engine's netBuffer
 * as the reading buffer. The reader has to be destroyed with Reader_Delete()
 * after it is not needed any more.
 */
Reader* Reader_NewWithNetworkBuffer(void);

/**
 * Constructs a new writer. The writer will use the engine's netBuffer
 * as the writing buffer. The writer has to be destroyed with Writer_Delete()
 * after it is not needed any more.
 */
Writer* Writer_NewWithNetworkBuffer(void);

/**
 * Functions.
 */
void            N_Init(void);
void            N_Shutdown(void);
void            N_ClearMessages(void);
void            N_SendPacket(int flags);
boolean         N_GetPacket(void);
int             N_IdentifyPlayer(nodeid_t id);
void            N_PrintBufferInfo(void);
void            N_PrintTransmissionStats(void);
void            N_PostMessage(netmessage_t *msg);
void            N_AddSentBytes(size_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* LIBDENG_NETWORK_BUFFER_H */
