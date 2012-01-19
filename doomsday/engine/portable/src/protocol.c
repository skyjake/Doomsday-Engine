/**
 * @file protocol.c
 * Implementation of the network protocol. @ingroup network
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

/**
 * @page sysNetwork Low-Level Networking
 *
 * On server-side connected clients can be either in "unjoined" mode or
 * "joined" mode. The former is for querying information about the server's
 * status, while the latter one is for clients participating in the on-going
 * game.
 *
 * Unjoined TCP sockets are periodically polled for activity
 * (N_ListenUnjoinedNodes()). Joined TCP sockets are handled in a separate
 * receiver thread (N_JoinedListenerThread()).
 *
 * @section netProtocol Network Protocol
 *
 * In joined mode, the network protocol works as follows. All messages are sent
 * over a TCP socket. Every message consists of a header and the message payload.
 * The content of these depends on the compressed message size.
 *
 * @par 1&ndash;127 bytes
 * Very small messages, such as the position updates that a client streams
 * to the server, are encoded with Huffman codes (see huffman.h). If
 * the Huffman coded payload happens to exceed 127 bytes, the message is
 * switched to the medium format (see below). Message structure:
 * - 1 byte: payload size
 * - @em n bytes: payload contents (Huffman)
 *
 * @par 128&ndash;4095 bytes
 * Medium-sized messages are compressed either using a fast zlib deflate level,
 * or Huffman codes if it yields better compression.
 * If the deflated message size exceeds 4095 bytes, the message is switched to
 * the large format (see below). Message structure:
 * - 1 byte: 0x80 | (payload size & 0x7f)
 * - 1 byte: payload size >> 7 | (0x40 for Huffman, otherwise deflated)
 * - @em n bytes: payload contents (as produced by ZipFile_CompressAtLevel()).
 *
 * @par >= 4096 bytes (up to 4MB)
 * Large messages are compressed using the best zlib deflate level.
 * Message structure:
 * - 1 byte: 0x80 | (payload size & 0x7f)
 * - 1 byte: 0x80 | (payload size >> 7) & 0x7f
 * - 1 byte: payload size >> 14
 * - @em n bytes: payload contents (as produced by ZipFile_CompressAtLevel()).
 *
 * Messages larger than or equal to 2^22 bytes (about 4MB) must be broken into
 * smaller pieces before sending.
 *
 * @see Protocol_Send()
 * @see Protocol_Receive()
 */

#ifdef DENG_SDLNET_DUMMY
#  include "sdlnet_dummy.h"
#else
#  include <SDL_net.h>
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_misc.h"
#include "protocol.h"

#include "sys_network.h"
#include "huffman.h"
#include "zipfile.h"

#define MAX_SIZE_SMALL          127 // bytes
#define MAX_SIZE_MEDIUM         4095 // bytes
#define MAX_SIZE_LARGE          PROTOCOL_MAX_DATAGRAM_SIZE

#define DEFAULT_TRANSMISSION_SIZE   4096

static byte* transmissionBuffer;
static size_t transmissionBufferSize;

void Protocol_Init(void)
{
    // Allocate the transmission buffer.
    transmissionBufferSize = DEFAULT_TRANSMISSION_SIZE;
    transmissionBuffer = M_Malloc(transmissionBufferSize);
}

void Protocol_Shutdown(void)
{
    M_Free(transmissionBuffer);
    transmissionBuffer = NULL;
    transmissionBufferSize = 0;
}

static boolean getBytesBlocking(TCPsocket sock, byte* buffer, size_t size)
{
    int bytes = 0;
    while(bytes < (int)size)
    {
        int result = SDLNet_TCP_Recv(sock, buffer + bytes, size - bytes);
        if(result == -1)
            return false; // Socket error.

        bytes += result;
        assert(bytes <= (int)size);
    }
    return true;
}

static boolean getNextByte(TCPsocket sock, byte* b)
{
    return getBytesBlocking(sock, b, 1);
}

boolean Protocol_Receive(nodeid_t from)
{
    TCPsocket sock = (TCPsocket) N_GetNodeSocket(from);
    char* packet = 0;
    size_t size = 0;
    boolean needInflate = false;
    byte b;

    // Read the header.
    if(!getNextByte(sock, &b)) return false;

    size = b & 0x7f;

    if(b & 0x80)
    {
        needInflate = true;
        if(!getNextByte(sock, &b)) return false;

        size |= ((b & 0x7f) << 7);

        if(b & 0x80)
        {
            if(!getNextByte(sock, &b)) return false;

            size |= (b << 14);
        }
    }

    // Allocate memory for the packet. This will be freed once the message
    // has been handled.
    packet = M_Malloc(size);

    if(!getBytesBlocking(sock, packet, size))
    {
        // An error with the socket (closed?).
        M_Free(packet);
        return false;
    }

    // Uncompress the payload.
    {
        size_t uncompSize = 0;
        byte* uncompData = 0;
        if(needInflate)
        {
            uncompData = ZipFile_Uncompress(packet, size, &uncompSize);
        }
        else
        {
            uncompData = Huffman_Decode(packet, size, &uncompSize);
        }
        if(!uncompData)
        {
            M_Free(packet);
            return false;
        }
        // Replace with the uncompressed version.
        M_Free(packet);
        packet = uncompData;
        size = uncompSize;
    }

    // Post the received message.
    {
        netmessage_t *msg = M_Calloc(sizeof(netmessage_t));
        msg->sender = from;
        msg->data = (byte*) packet;
        msg->size = size;
        msg->handle = packet;

        // The message queue will handle the message from now on.
        N_PostMessage(msg);
    }
    return true;
}

void Protocol_FreeBuffer(void *handle)
{
    if(handle)
    {
        M_Free(handle);
    }
}

static void checkTransmissionBufferSize(size_t atLeastBytes)
{
    while(transmissionBufferSize < atLeastBytes)
    {
        transmissionBufferSize *= 2;
        transmissionBuffer = M_Realloc(transmissionBuffer, transmissionBufferSize);
    }
}

/**
 * Copies the message payload @a data to the transmission buffer.
 */
static size_t prepareTransmission(void* data, size_t size)
{
    Writer* msg = 0;
    size_t msgSize = 0;

    // The header is at most 3 bytes.
    checkTransmissionBufferSize(size + 3);

    // Compose the header and payload into the transmission buffer.
    msg = Writer_NewWithBuffer(transmissionBuffer, transmissionBufferSize);

    if(size <= MAX_SIZE_SMALL)
    {
        Writer_WriteByte(msg, size);
    }
    else if(size <= MAX_SIZE_MEDIUM)
    {
        Writer_WriteByte(msg, 0x80 | (size & 0x7f));
        Writer_WriteByte(msg, size >> 7);
    }
    else if(size <= MAX_SIZE_LARGE)
    {
        Writer_WriteByte(msg, 0x80 | (size & 0x7f));
        Writer_WriteByte(msg, 0x80 | ((size >> 7) & 0x7f));
        Writer_WriteByte(msg, size >> 14);
    }
    else
    {
        // Not supported.
        assert(false);
    }

    // The payload.
    Writer_Write(msg, data, size);

    msgSize = Writer_Size(msg);
    Writer_Delete(msg);
    return msgSize;
}

/**
 * Send the data buffer over a TCP connection.
 * The data may be compressed with zlib.
 */
void Protocol_Send(void *data, size_t size, nodeid_t destination)
{
    int result = 0;
    size_t transmissionSize = 0;

    if(size == 0 || !N_GetNodeSocket(destination) || !N_HasNodeJoined(destination))
        return;

    if(size > DDMAXINT)
    {
        Con_Error("Protocol_Send: Trying to send an oversized data buffer.\n"
                  "  Attempted size is %u bytes.\n", (unsigned long) size);
    }

#ifdef _DEBUG
    Monitor_Add(data, size);
#endif

    // Let's first see if the encoded contents are under 128 bytes
    // as Huffman codes.
    if(size <= MAX_SIZE_SMALL*4) // Potentially short enough.
    {
        size_t compSize = 0;
        uint8_t* compData = Huffman_Encode(data, size, &compSize);
        if(compSize <= MAX_SIZE_SMALL)
        {
            // We can use this.
            transmissionSize = prepareTransmission(compData, compSize);
        }
        M_Free(compData);
    }

    if(!transmissionSize) // Let's deflate, then.
    {
        /// @todo Messages broadcasted to multiple recipients are separately
        /// compressed for each TCP send -- should do only one compression
        /// per message.

        size_t compSize = 0;
        uint8_t* compData = ZipFile_CompressAtLevel(data, size, &compSize,
            size < 2*MAX_SIZE_MEDIUM? 6 /*default*/ : 9 /*best*/);
        if(!compData)
        {
            Con_Error("Protocol_Send: Failed to compress transmission.\n");
        }
        if(compSize > MAX_SIZE_LARGE)
        {
            M_Free(compData);
            Con_Error("Protocol_Send: Compressed payload is too large (%u bytes).\n", compSize);
        }
        transmissionSize = prepareTransmission(compData, compSize);
        M_Free(compData);
    }

    // Send the data over the socket.
    result = SDLNet_TCP_Send((TCPsocket)N_GetNodeSocket(destination),
                             transmissionBuffer, transmissionSize);
    if(result < 0 || (size_t) result != transmissionSize)
    {
        perror("Socket error");
    }
    // Statistics.
    N_AddSentBytes(transmissionSize);
}
