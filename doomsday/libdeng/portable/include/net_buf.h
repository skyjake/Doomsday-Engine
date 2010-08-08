/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * net_buf.h: Network Message Handling and Buffering
 */

#ifndef __DOOMSDAY_NETWORK_BUFFER_H__
#define __DOOMSDAY_NETWORK_BUFFER_H__

#include "con_decl.h"

// Send Packet flags:
#define SPF_REBOUND     0x00020000 // Write only to local loopback
#define SPF_DONT_SEND   0x00040000 // Don't really send out anything
#define SPF_CONFIRM     0x40000000 // Make sure it's received
#define SPF_ORDERED     0x80000000 // Send in order & confirm

#define NETBUFFER_MAXDATA   32768

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
} netmessage_t;

typedef unsigned short msgid_t;

#pragma pack(1)
typedef struct {
    msgid_t         id;             // Unused.
    byte            type;           // Type of the message.
    byte            data[NETBUFFER_MAXDATA];
} netdata_t;
#pragma pack()

typedef struct netbuffer_s {
    int             player;         // Recipient or sender.
    size_t          length;         // Number of bytes in the data buffer.
    size_t          headerLength;   // 1 byte at the moment.

    byte           *cursor;         // Points into the data buffer.

    netdata_t       msg;            // The data buffer for sending and
                                    // receiving packets.
} netbuffer_t;

/**
 * Globally accessible data.
 */
//extern netbuffer_t netBuffer;
//extern boolean  allowSending;

/**
 * Functions.
 */
void            N_Init(void);
void            N_Shutdown(void);
void            N_ClearMessages(void);
void            N_SendPacket(int flags);
boolean         N_GetPacket(void);
uint            N_IdentifyPlayer(nodeid_t id);
void            N_PrintBufferInfo(void);
void            N_PrintHuffmanStats(void);
void            N_PostMessage(netmessage_t *msg);

#endif
