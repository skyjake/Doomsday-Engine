/** @file protocol.cpp Implementation of the network protocol. 
 * @ingroup network
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

#include <de/memory.h>
#include <de/memoryzone.h>
#include <de/c_wrapper.h>

#include "de_platform.h"
#include "de_console.h"
#include "de_misc.h"
#include "de_network.h"

boolean Protocol_Receive(nodeid_t from)
{
    int size = 0;
    int sock = N_GetNodeSocket(from);
    byte* packet = 0;

    packet = LegacyNetwork_Receive(sock, &size);
    if(!packet)
    {
        // Failed to receive anything.
        return false;
    }

    // Post the received message.
    {
        netmessage_t *msg = (netmessage_t *) M_Calloc(sizeof(netmessage_t));

        msg->sender = from;
        msg->data = packet;
        msg->size = size;
        msg->handle = packet; // needs to be freed

        // The message queue will handle the message from now on.
        N_PostMessage(msg);
    }
    return true;
}

void Protocol_FreeBuffer(void *handle)
{
    if(handle)
    {
        LegacyNetwork_FreeBuffer((byte *) handle);
    }
}

void Protocol_Send(void *data, size_t size, nodeid_t destination)
{
    if(size == 0 || !N_GetNodeSocket(destination) || !N_HasNodeJoined(destination))
        return;

    if(size > DDMAXINT)
    {
        Con_Error("Protocol_Send: Trying to send an oversized data buffer.\n"
                  "  Attempted size is %u bytes.\n", (unsigned long) size);
    }

#ifdef _DEBUG
    Monitor_Add((uint8_t const *) data, size);
#endif

    LegacyNetwork_Send(N_GetNodeSocket(destination), data, size);
}
