/** @file net_buf.cpp  Network Message Handling and Buffering.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "network/net_buf.h"
#include "network/net_event.h"
#include "world/p_players.h"
#ifdef __CLIENT__
#  include "network/sys_network.h"
#  include "network/serverlink.h"
#endif
#ifdef __SERVER__
#  include "serversystem.h"
#endif

#include <doomsday/network/masterserver.h>
#include <doomsday/net.h>
#include <de/c_wrapper.h>
#include <de/legacy/concurrency.h>
#include <de/legacy/memory.h>
#include <de/legacy/timer.h>
#include <de/byterefarray.h>
#include <de/loop.h>

using namespace de;

#define MSG_MUTEX_NAME  "MsgQueueMutex"

dd_bool allowSending;
netbuffer_t netBuffer;

// The message queue: list of incoming messages waiting for processing.
static netmessage_t *msgHead, *msgTail;
static dint entryCount;

// A mutex is used to protect the addition and removal of messages from
// the message queue.
static mutex_t msgMutex;

reader_s *Reader_NewWithNetworkBuffer()
{
    return Reader_NewWithBuffer((const byte *) netBuffer.msg.data, netBuffer.length);
}

void N_Init()
{
    // Create a mutex for the message queue.
    ::msgMutex = Sys_CreateMutex(MSG_MUTEX_NAME);

    ::allowSending = false;

    //N_SockInit();
    N_MasterInit();
}

void N_Shutdown()
{
    // Any queued messages will be destroyed.
    N_ClearMessages();

    N_MasterShutdown();

    ::allowSending = false;

    // Close the handle of the message queue mutex.
    Sys_DestroyMutex(msgMutex);
    msgMutex = 0;
}

/**
 * Acquire or release ownership of the message queue mutex.
 *
 * @return  @c true if successful.
 */
static dd_bool N_LockQueue(dd_bool doAcquire)
{
    if(doAcquire)
        Sys_Lock(msgMutex);
    else
        Sys_Unlock(msgMutex);
    return true;
}

void N_PostMessage(netmessage_t *msg)
{
    DE_ASSERT(msg);

    N_LockQueue(true);

    // This will be the latest message.
    msg->next = nullptr;

    // Set the timestamp for reception.
    msg->receivedAt = Timer_RealSeconds();

    if(::msgTail)
    {
        // There are previous messages.
        ::msgTail->next = msg;
    }

    // The tail pointer points to the last message.
    ::msgTail = msg;

    // If there is no head, this'll be the first message.
    if(::msgHead == nullptr)
        ::msgHead = msg;

    // One new message available.
    ::entryCount++;

    N_LockQueue(false);
}

/**
 * Extracts the next message from the queue of received messages.
 * The caller must release the message when it's no longer needed,
 * using N_ReleaseMessage().
 *
 * We use a mutex to synchronize access to the message queue. This is
 * called in the Doomsday thread.
 *
 * @return  @c nullptr if no message is found.
 */
static netmessage_t *N_GetMessage()
{
    // This is the message we'll return.
    netmessage_t *msg = nullptr;

    N_LockQueue(true);
    if(::msgHead != nullptr)
    {
        msg = ::msgHead;

        // Check for simulated latency.
        if(netState.simulatedLatencySeconds > 0 &&
           (Timer_RealSeconds() - msg->receivedAt < netState.simulatedLatencySeconds))
        {
            // This message has not been received yet.
            msg = nullptr;
        }
        else
        {
            // If there are no more messages, the tail pointer must be cleared, too.
            if(!::msgHead->next)
                ::msgTail = nullptr;

            // Advance the head pointer.
            ::msgHead = ::msgHead->next;

            if(msg)
            {
                // One less message available.
                ::entryCount--;
            }
        }
    }
    N_LockQueue(false);

    // Identify the sender.
    if(msg)
    {
        msg->player = N_IdentifyPlayer(msg->sender);
    }
    return msg;
}

static void N_ReleaseMessage(netmessage_t *msg)
{
    DE_ASSERT(msg);
    if(msg->handle)
    {
        delete [] reinterpret_cast<byte *>(msg->handle);
        msg->handle = nullptr;
    }
    M_Free(msg);
}

void N_ClearMessages()
{
    if(!msgMutex) return;  // Not initialized yet.

    const dfloat oldSim = netState.simulatedLatencySeconds;

    // No simulated latency now.
    netState.simulatedLatencySeconds = 0;

    netmessage_t *msg;
    while((msg = N_GetMessage()) != nullptr)
    {
        N_ReleaseMessage(msg);
    }

    netState.simulatedLatencySeconds = oldSim;

    // The queue is now empty.
    ::msgHead = ::msgTail = nullptr;
    ::entryCount = 0;
}

void N_SendPacket(void)
{
    try
    {
        if (allowSending)
        {
            DoomsdayApp::net().sendDataToPlayer(
                netBuffer.player,
                ByteRefArray(&netBuffer.msg, netBuffer.headerLength + netBuffer.length));
        }
    }
    catch(const Error &er)
    {
        LOGDEV_NET_WARNING("N_SendPacket failed: ") << er.asText();
    }
}

dint N_IdentifyPlayer(nodeid_t id)
{
#ifdef __SERVER__
    // What is the corresponding player number? Only the server keeps
    // a list of all the IDs.
    for(dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(DD_Player(i)->remoteUserId == id)
            return i;
    }
    return -1;
#else
    DE_UNUSED(id);
#endif

    // Clients receive messages only from the server.
    return 0;
}

/**
 * Retrieves the next incoming message.
 *
 * @return  The next message waiting in the incoming message queue. When the message
 * is no longer needed you must call N_ReleaseMessage() to delete it.
 */
static netmessage_t *N_GetNextMessage()
{
    netmessage_t *msg;
    while((msg = N_GetMessage()) != nullptr)
    {
        return msg;
    }
    return nullptr;  // There are no more messages.
}

dd_bool N_GetPacket()
{
    // If there are net events pending, let's not return any packets
    // yet. The net events may need to be processed before the
    // packets.
    if(N_NEPending())
        return false;

    ::netBuffer.player = -1;
    ::netBuffer.length = 0;

    netmessage_t *msg = N_GetNextMessage();
    if(!msg) return false;  // No messages at this time.

    // There was a packet!
    ::netBuffer.player = msg->player;
    ::netBuffer.length = msg->size - ::netBuffer.headerLength;

    if(sizeof(::netBuffer.msg) >= msg->size)
    {
        std::memcpy(&::netBuffer.msg, msg->data, msg->size);
    }
    else
    {
        LOG_NET_ERROR("Received an oversized packet with %i bytes") << msg->size;

        N_ReleaseMessage(msg);
        return false;
    }

    // The message can now be freed.
    N_ReleaseMessage(msg);

    // We have no idea who sent this (on serverside).
    if(::netBuffer.player == -1)
        return false;

    return true;
}

void N_PrintBufferInfo()
{
    N_PrintTransmissionStats();

    const double loopRate = Loop::get().rate();
    if (loopRate > 0)
    {
        LOG_NET_MSG("Event loop frequency: up to %.1f Hz") << loopRate;
    }
    else
    {
        LOG_NET_MSG("Event loop frequency: unlimited");
    }
}

void N_PrintTransmissionStats()
{
    const auto dataBytes = Socket::sentUncompressedBytes();
    const auto outBytes  = Socket::sentBytes();
    const auto outRate   = Socket::outputBytesPerSecond();

    if (outBytes == 0)
    {
        LOG_NET_MSG("Nothing has been sent yet over the network");
    }
    else
    {
        LOG_NET_MSG("Average compression: %.3f%% (data: %.1f KB, out: %.1f KB)\n"
                    "Current output: %.1f KB/s")
            << 100 * (1.0 - double(outBytes) / double(dataBytes))
            << dataBytes/1000.0
            << outBytes/1000.0
            << outRate/1000.0;
    }
}
