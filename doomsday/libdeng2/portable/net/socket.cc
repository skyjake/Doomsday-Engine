/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/Socket"
#include "de/Message"
#include "../internal.h"
#include "../sdl.h"

#include <string>

using namespace de;

/// Version of the block transfer protocol.
static const duint PROTOCOL_VERSION = 0;

Socket::Header::Header() : version(PROTOCOL_VERSION), huffman(false), channel(0), size(0) {}

Socket::Socket(const Address& address) : socket_(0), socketSet_(0)
{
    IPaddress ip;

    internal::convertAddress(address, &ip);
    socket_ = SDLNet_TCP_Open(&ip);
    if(socket_ == 0)
    {
        /// @throw ConnectionError Opening the socket to @a address failed.
        throw ConnectionError("Socket::Socket", "Failed to connect: " +
            std::string(SDLNet_GetError()));
    }

    initialize();
}

Socket::Socket(void* existingSocket) : socket_(existingSocket), socketSet_(0)
{
    initialize();
}

Socket::~Socket()
{
    close();

    if(socketSet_)
    {
        SDLNet_FreeSocketSet(static_cast<SDLNet_SocketSet>(socketSet_));
        socketSet_ = 0;
    }
}

void Socket::initialize()
{
    // Allocate a socket set for waiting on.
    socketSet_ = SDLNet_AllocSocketSet(1);

    SDLNet_AddSocket(static_cast<SDLNet_SocketSet>(socketSet_),
        static_cast<SDLNet_GenericSocket>(socket_));

    // Incoming packets will be marked with this address.
    peerAddress_ = peerAddress();
}

void Socket::close()
{
    // Close the socket.
    if(socket_)
    {
        lock();
        TCPsocket s = static_cast<TCPsocket>(socket_);
        socket_ = 0;
        SDLNet_TCP_Close(s);
        unlock();
    }
}

Socket& Socket::operator << (const IByteArray& packet)
{
    send(packet, mode[CHANNEL_1_BIT]? 1 : 0);
    return *this;
}

void Socket::writeHeader(const Header& header, IByteArray::Byte* buffer)
{
    /**
     * Writes the 4-byte header to the beginning of the buffer.
     * - 3 bits for flags.
     * - 2 bits for the protocol version number.
     * - 16+11 bits for the packet length (max: 134 MB).
     */
     
    duint flags = 
        (header.huffman? Header::HUFFMAN : 0) |
        (header.channel == 1? Header::CHANNEL_1 : 0);

    duint bits = ( (header.size & 0x7ffffff) |
                   ((header.version & 3) << 27) |
                   (flags << 29) );

    SDLNet_Write32(bits, buffer);
}

void Socket::readHeader(duint headerBytes, Header& header)
{
    duint hostHeader = SDLNet_Read32(&headerBytes);
    duint flags = (hostHeader >> 29) & 7;
    
    header.version = (hostHeader >> 27) & 3;
    header.huffman = (flags & HUFFMAN) != 0;
    header.channel = (flags & CHANNEL_1? 1 : 0);
    header.size = hostHeader & 0x7ffffff;
}

void Socket::send(const IByteArray& packet, duint channel)
{
    if(!socket_) 
    {
        /// @throw DisconnectedError Sending is not possible because the socket has been closed.
        throw DisconnectedError("Socket::operator << ", "Socket closed");
    }

    duint packetSize = packet.size() + 4;
    IByteArray::Byte* buffer = new IByteArray::Byte[packetSize];

    // Write the packet header: packet length, version, possible flags.
    Header header;
    header.channel = channel;
    header.size = packet.size();
    writeHeader(header, buffer);
    
    packet.get(0, buffer + 4, packet.size());
    unsigned int sentBytes = SDLNet_TCP_Send(
        static_cast<TCPsocket>(socket_), buffer, packetSize);
    delete [] buffer;

    // Did the transmission fail?
    if(sentBytes != packetSize)
    {
        /// @throw DisconnectedError All the data was not sent successfully.
        throw DisconnectedError("Socket::operator << ", std::string(SDLNet_GetError()));
    }
}

void Socket::checkValid()
{
    if(!socket_ || !socketSet_)
    {
        /// @throw DisconnectedError The socket has been closed.
        throw DisconnectedError("Socket::receive", "Socket was closed");
    }
}

void Socket::receiveBytes(duint count, dbyte* buffer)
{
    duint received = 0;

    try
    {
        // We make sure that the socket is valid while we're locked.
        lock();
        checkValid();
        
        // Wait indefinitely until there is something to receive.
        while(received < count)
        {
            unlock();

            int result = SDLNet_CheckSockets(
                static_cast<SDLNet_SocketSet>(socketSet_), LIBDENG2_SOCKET_RECV_TIMEOUT);

            lock();
            checkValid();

            if(result < 0)
            {
                /// @throw DisconnectedError There was an error in the socket while waiting 
                /// for incoming data.
                throw DisconnectedError("Socket::receive", "Socket broken while waiting");
            }
            else if(!result)
            {
                // Nothing yet.
                continue;
            }

            // There is something to receive.
            int recvResult = SDLNet_TCP_Recv(static_cast<TCPsocket>(socket_), 
                buffer + received, count - received);

            if(recvResult <= 0)
            {
                /// @throw DisconnectedError An error occurred in the socket while receiving
                /// data. Only part of the data was received (or none of it).
                throw DisconnectedError("Socket::receive", "Socket broken while receiving data");
            }
        
            received += recvResult;
        }

        unlock();
    }
    catch(const DisconnectedError& err)
    {
        // We leave here unlocked.
        unlock();
        throw err;
    }
    catch(const Error& err)
    {
        // We leave here unlocked.
        unlock();
        throw err;
    }
}

Message* Socket::receive()
{
    if(!socket_) 
    {
        /// @throw DisconnectedError Receiving data is not possible because the socket is closed.
        throw DisconnectedError("Socket::receive", "Socket is closed");
    }

    // First read the header.
    duint headerBytes = 0;
    receiveBytes(4, reinterpret_cast<dbyte*>(&headerBytes));

    Header incoming;
    readHeader(headerBytes, incoming);
    
    // Check for valid protocols.
    if(incoming.version != PROTOCOL_VERSION)
    {
        /// @throw UnknownProtocolError The received data block's protocol version number
        /// was not recognized. This is probably because the remote end is not a libdeng2
        /// application.
        throw UnknownProtocolError("Socket::receive", "Incoming packet has unknown protocol");
    }
    
    std::auto_ptr<Message> data(new Message(peerAddress_, incoming.channel, incoming.size)); 
    receiveBytes(incoming.size, const_cast<dbyte*>(data.get()->data()));
    return data.release();
}

Address Socket::peerAddress() const
{
    if(socket_)
    {
        IPaddress* ip = SDLNet_TCP_GetPeerAddress(static_cast<TCPsocket>(socket_));
        if(ip)
        {
            return internal::convertAddress(ip);
        }                
    }
    /// @throw PeerError Could not determine the TCP/IP address of the socket.
    throw PeerError("Socket::peerAddress", SDLNet_GetError());
}
