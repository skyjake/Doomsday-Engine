/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * net_buf.c: Network Message Handling and Buffering
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_network.h"
#include "de_console.h"
#include "de_misc.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

#define MSG_MUTEX_NAME  "MsgQueueMutex"

// Flags for the sent message store (for to-be-confirmed messages):
//#define SMSF_ORDERED  0x1     // Block other ordered messages until confirmed
//#define SMSF_QUEUED       0x2     // Ordered message waiting to be sent
//#define SMSF_CONFIRMED    0x4     // Delivery has been confirmed! (OK to remove)

// Length of the received message ID history.
//#define STORE_HISTORY_SIZE 100

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean allowSending;
netbuffer_t netBuffer;

// The Sent Message Store: list of sent or queued messages waiting to be
// confirmed.
//static store_t stores[DDMAXPLAYERS];

// The message queue: list of incoming messages waiting for processing.
//static netmessage_t *msgHead, *msgTail;
//static int msgCount;

// A mutex is used to protect the addition and removal of messages from
// the message queue.
//static mutex_t msgMutex;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Number of bytes of outgoing data transmitted.
//static size_t numOutBytes;

// Number of bytes sent over the network (compressed).
//static size_t numSentBytes;

// CODE --------------------------------------------------------------------

/**
 * Initialize the low-level network subsystem. This is called always
 * during startup (via Sys_Init()).
 */
void N_Init(void)
{
#if 0
    // Create a mutex for the message queue.
    msgMutex = Sys_CreateMutex(MSG_MUTEX_NAME);

    allowSending = false;

    N_SockInit();
    N_MasterInit();
    N_SystemInit();             // Platform dependent stuff.
#endif
}

/**
 * Shut down the low-level network interface. Called during engine
 * shutdown (not before).
 */
void N_Shutdown(void)
{
#if 0
    N_SystemShutdown();
    N_MasterShutdown();
    N_SockShutdown();

    allowSending = false;

    // Close the handle of the message queue mutex.
    Sys_DestroyMutex(msgMutex);
    msgMutex = 0;

    if(ArgExists("-huffavg"))
    {
        Con_Execute(CMDS_DDAY, "huffman", false, false);
    }
#endif
}

/**
 * Acquire or release ownership of the message queue mutex.
 *
 * @return          @c true, if successful.
 */
boolean N_LockQueue(boolean doAcquire)
{
#if 0
    if(doAcquire)
        Sys_Lock(msgMutex);
    else
        Sys_Unlock(msgMutex);
#endif
    return true;
}

/**
 * Adds the given netmessage_s to the queue of received messages.
 * Before calling this, allocate the message using malloc().  We use a
 * mutex to synchronize access to the message queue.  This is called
 * in the network receiver thread.
 */
void N_PostMessage(netmessage_t *msg)
{
#if 0
    N_LockQueue(true);

    // This will be the latest message.
    msg->next = NULL;

    if(msgTail)
    {
        // There are previous messages.
        msgTail->next = msg;
    }

    // The tail pointer points to the last message.
    msgTail = msg;

    // If there is no head, this'll be the first message.
    if(msgHead == NULL)
        msgHead = msg;

    // One new message available.
    msgCount++;

    N_LockQueue(false);
#endif
}

/**
 * Extracts the next message from the queue of received messages.
 * The caller must release the message when it's no longer needed,
 * using N_ReleaseMessage().
 *
 * We use a mutex to synchronize access to the message queue. This is
 * called in the Doomsday thread.
 *
 * @return              @c NULL, if no message is found;
 */
netmessage_t *N_GetMessage(void)
{
    // This is the message we'll return.
    netmessage_t *msg = NULL;
#if 0

    N_LockQueue(true);
    if(msgHead != NULL)
    {
        msg = msgHead;

        // If there are no more messages, the tail pointer must be
        // cleared, too.
        if(!msgHead->next)
            msgTail = NULL;

        // Advance the head pointer.
        msgHead = msgHead->next;

        if(msg)
        {
            // One less message available.
            msgCount--;
        }
    }
    N_LockQueue(false);

    // Identify the sender.
    if(msg)
    {
        msg->player = N_IdentifyPlayer(msg->sender);
    }
#endif
    return msg;
}

/**
 * Frees the message.
 */
void N_ReleaseMessage(netmessage_t *msg)
{
#if 0
    if(msg->handle)
    {
        N_ReturnBuffer(msg->handle);
    }
    M_Free(msg);
#endif
}

/**
 * Empties the message buffers.
 */
void N_ClearMessages(void)
{
#if 0
    netmessage_t *msg;

    while((msg = N_GetMessage()) != NULL)
        N_ReleaseMessage(msg);

    // The queue is now empty.
    msgHead = msgTail = NULL;
    msgCount = 0;
#endif
}

/**
 * Send the data in the netbuffer. The message is sent using an
 * unreliable, nonsequential (i.e. fast) method.
 *
 * Handles broadcasts using recursion.
 * Clients can only send stuff to the server.
 */
void N_SendPacket(int flags)
{
#if 0
    uint                i, dest = 0;
    void               *data;
    size_t              size;

    // Is the network available?
    if(!allowSending || !N_IsAvailable())
        return;

    // Figure out the destination DPNID.
    if(netServerMode)
    {
        player_t           *plr = &ddPlayers[netBuffer.player];
        ddplayer_t         *ddpl = &plr->shared;

        if(netBuffer.player >= 0 && netBuffer.player < DDMAXPLAYERS)
        {
            if((ddpl->flags & DDPF_LOCAL) ||
               !clients[netBuffer.player].connected)
            {
                // Do not send anything to local or disconnected players.
                return;
            }

            dest = clients[netBuffer.player].nodeID;
        }
        else
        {
            // Broadcast to all non-local players, using recursive calls.
            for(i = 0; i < DDMAXPLAYERS; ++i)
            {
                netBuffer.player = i;
                N_SendPacket(flags);
            }

            // Reset back to -1 to notify of the broadcast.
            netBuffer.player = NSP_BROADCAST;
            return;
        }
    }

    // Message IDs are currently not used.
    netBuffer.msg.id = 0;

    // This is what will be sent.
    numOutBytes += netBuffer.headerLength + netBuffer.length;

    // Compress using Huffman codes.
    data = Huff_Encode((byte *) &netBuffer.msg,
                       netBuffer.headerLength + netBuffer.length, &size);

    // This many bytes are actually sent.
    numSentBytes += size;

    if(flags & (SPF_CONFIRM | SPF_ORDERED))
    {
        // Ordered and confirmed messages are send over a TCP connection.
        N_SendDataBufferReliably(data, size, dest);
#if _DEBUG
VERBOSE2(
Con_Message("N_SendPacket: Sending %ul bytes reliably to %i.\n", size, dest));
#endif
    }
    else
    {
        // Other messages are sent via UDP, so that there is as little latency
        // as possible.
        N_SendDataBuffer(data, size, dest);
    }
#endif
}

/**
 * @return              The player number that corresponds the DPNID.
 */
uint N_IdentifyPlayer(nodeid_t id)
{
#if 0
    uint                i;
    boolean             found;

    if(netServerMode)
    {
        // What is the corresponding player number? Only the server keeps
        // a list of all the IDs.
        i = 0;
        found = false;
        while(i < DDMAXPLAYERS && !found)
            if(clients[i].nodeID == id)
                found = true;
            else
                i++;

        if(found)
            return i;
        else
            return -1; // Bogus?
    }

    // Clients receive messages only from the server.
    return 0;
#endif
}

/**
 * Confirmations are handled here.
 *
 * \note Skips all messages from unknown nodeids!
 *
 * @return              The next message waiting in the incoming message
 *                      queue.
 */
netmessage_t *N_GetNextMessage(void)
{
#if 0
    netmessage_t       *msg;

    while((msg = N_GetMessage()) != NULL)
    {
        //// \fixme When can player IDs be unknown?
        /* if(msg->player < 0)
        {
            // From an unknown ID?
            N_ReleaseMessage(msg);
        }
        else */
        {
            // Decode the Huffman codes. The returned buffer is static, so
            // it doesn't need to be freed (not thread-safe, though).
            msg->data = Huff_Decode(msg->data, msg->size, &msg->size);

            // The original packet buffer can be freed.
            N_ReturnBuffer(msg->handle);
            msg->handle = NULL;

            return msg;
        }
    }
#endif
    return NULL; // There are no more messages.
}

/**
 * An attempt is made to extract a message from the message queue.
 *
 * @return          @c true, if a message successfull.
 */
boolean N_GetPacket(void)
{
#if 0
    netmessage_t *msg;

    // If there are net events pending, let's not return any packets
    // yet. The net events may need to be processed before the
    // packets.
    if(!N_IsAvailable() || N_NEPending())
        return false;

    netBuffer.player = -1;
    netBuffer.length = 0;

    /*{extern byte monitorMsgQueue;
    if(monitorMsgQueue)
        Con_Message("N_GetPacket: %i messages queued.\n", msgCount);
    }*/

    if((msg = N_GetNextMessage()) == NULL)
    {
        // No messages at this time.
        return false;
    }

    // There was a packet!
/*
#if _DEBUG
   Con_Message("N_GetPacket: from=%x, len=%i\n", msg->sender, msg->size);
#endif
*/
    netBuffer.player = msg->player;
    netBuffer.length = msg->size - netBuffer.headerLength;
    memcpy(&netBuffer.msg, msg->data,
           MIN_OF(sizeof(netBuffer.msg), msg->size));

    // The message can now be freed.
    N_ReleaseMessage(msg);

    // We have no idea who sent this (on serverside).
    if(netBuffer.player == -1)
        return false;

#endif
    return true;
}

/**
 * Print low-level information about the network buffer.
 */
void N_PrintBufferInfo(void)
{
    N_PrintHuffmanStats();
}

/**
 * Print status information about the workings of Huffman compression
 * in the network buffer.
 */
void N_PrintHuffmanStats(void)
{
#if 0
    if(numOutBytes == 0)
    {
        Con_Printf("Huffman efficiency: Nothing has been sent yet.\n");
    }
    else
    {
        Con_Printf("Huffman efficiency: %.3f%% (data: %ul bytes, sent: %ul "
                   "bytes)\n", 100 - (100.0f * numSentBytes) / numOutBytes,
                   numOutBytes, numSentBytes);
    }
#endif
}

/**
 * Console command for printing the Huffman efficiency.
 */
D_CMD(HuffmanStats)
{
    N_PrintHuffmanStats();
    return true;
}
