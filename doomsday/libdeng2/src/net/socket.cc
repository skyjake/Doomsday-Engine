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
#include "de/String"
#include "de/ByteRefArray"
#include "de/Writer"
#include "de/Reader"

using namespace de;

/// Version of the block transfer protocol.
static const duint PROTOCOL_VERSION = 0;

/// Size of the message header in bytes.
static const duint HEADER_SIZE = 4;

Socket::Header::Header() : version(PROTOCOL_VERSION), huffman(false), channel(0), size(0) {}

Socket::Socket(const Address& address)
    : _receptionState(RECEIVING_HEADER),
      _bytesToBeWritten(0)
{
    _socket = new QTcpSocket(this);
    _socket->connectToHost(address.host(), address.port());
    initialize();
/*
    if(_socket == 0)
    {
        /// @throw ConnectionError Opening the socket to @a address failed.
        throw ConnectionError("Socket::Socket", "Failed to connect: " +
            String(SDLNet_GetError()));
    }
*/
}

Socket::Socket(QTcpSocket* existingSocket) : _socket(existingSocket)
{
    _socket->setParent(this);
    initialize();
}

Socket::~Socket()
{
    close();
}

void Socket::initialize()
{
    connect(_socket, SIGNAL(bytesWritten(qint64)), this, SLOT(bytesWereWritten(qint64)));
    connect(_socket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
    connect(_socket, SIGNAL(error(QAbstractSocket::SocketError)), this,
            SLOT(socketError(QAbstractSocket::SocketError)));
    connect(_socket, SIGNAL(readyRead()), this, SLOT(readIncomingBytes()));
}

void Socket::close()
{
    // Prevent emitting of any more signals.
    _socket->disconnect();

    _socket->close();
}

Socket& Socket::operator << (const IByteArray& packet)
{
    send(packet, mode & Channel1? 1 : 0);
    return *this;
}

duint32 Socket::packHeader(const Header& header)
{
    /**
     * Composese the 4-byte header to the beginning of the buffer.
     * - 3 bits for flags.
     * - 2 bits for the protocol version number.
     * - 16+11 bits for the packet length (max: 134 MB).
     */
     
    duint32 flags =
        (header.huffman? Header::HUFFMAN : 0) |
        (header.channel == 1? Header::CHANNEL_1 : 0);

    duint32 bits = ( (header.size & 0x7ffffff) |
                     ((header.version & 3) << 27) |
                     (flags << 29) );

    return bits;
}

void Socket::unpackHeader(duint32 headerBytes, Header& header)
{
    duint flags = (headerBytes >> 29) & 7;
    
    header.version = (headerBytes >> 27) & 3;
    header.huffman = (flags & HuffmanPayload) != 0;
    header.channel = (flags & Channel1? 1 : 0);
    header.size = headerBytes & 0x7ffffff;
}

void Socket::send(const IByteArray& packet, duint channel)
{
    if(!_socket) 
    {
        /// @throw DisconnectedError Sending is not possible because the socket has been closed.
        throw DisconnectedError("Socket::operator << ", "Socket is unavailable");
    }

    // Keep track of where we are with the traffic.
    _bytesToBeWritten += packet.size() + 4;

    // Write the packet header: packet length, version, possible flags.
    Header header;
    header.channel = channel;
    header.size = packet.size();

    Block dest;
    Writer(dest) << packHeader(header);
    _socket->write(dest);

    // Write the data itself.
    _socket->write(Block(packet));

/*    // Did the transmission fail?
    if(sentBytes != packetSize)
    {
        /// @throw DisconnectedError All the data was not sent successfully.
        throw DisconnectedError("Socket::operator << ", String(SDLNet_GetError()));
    }*/
}

/*
void Socket::checkValid()
{
    if(!_socket)
    {
        /// @throw DisconnectedError The socket has been closed.
        throw DisconnectedError("Socket::receive", "Socket was closed");
    }
}*/

/*
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
                static_cast<SDLNet_SocketSet>(_socketSet), LIBDENG2_SOCKET_RECV_TIMEOUT);

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
            int recvResult = SDLNet_TCP_Recv(static_cast<TCPsocket>(_socket), 
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
*/

void Socket::readIncomingBytes()
{
    forever
    {
        if(_receptionState == RECEIVING_HEADER)
        {
            if(_socket->bytesAvailable() >= HEADER_SIZE)
            {
                // Get the header.
                duint32 bits = 0;
                Reader(Block(_socket->read(HEADER_SIZE))) >> bits;
                unpackHeader(bits, _incomingHeader);

                _receptionState = RECEIVING_PAYLOAD;
            }
            else
            {
                // Let's wait until more is available.
                break;
            }
        }

        if(_receptionState == RECEIVING_PAYLOAD)
        {
            if(_socket->bytesAvailable() >= _incomingHeader.size)
            {
                _receivedMessages.append(new Message(Address(_socket->peerAddress(),
                                                             _socket->peerPort()),
                                                     _incomingHeader.channel,
                                                     Block(_socket->read(_incomingHeader.size))));

                // Get the next message.
                _receptionState = RECEIVING_HEADER;
            }
            else
            {
                // Let's wait until more is available.
                break;
            }
        }
    }

    // Notification about available messages.
    if(!_receivedMessages.isEmpty())
    {
        emit messagesReady();
    }
}

Message* Socket::receive()
{
    if(_receivedMessages.isEmpty())
    {
        return 0;
    }
    return _receivedMessages.takeFirst();

/*
    if(!_socket) 
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
    
    std::auto_ptr<Message> data(new Message(_peerAddress, incoming.channel, incoming.size)); 
    receiveBytes(incoming.size, const_cast<dbyte*>(data.get()->data()));
    return data.release();
    */
}

void Socket::flush()
{
    // Wait until data has been written.
    _socket->flush();
    _socket->waitForBytesWritten();
}

Address Socket::peerAddress() const
{
    if(isOpen())
    {
        return Address(_socket->peerAddress(), _socket->peerPort());
    }
    /// @throw PeerError Could not determine the TCP/IP address of the socket.
    throw PeerError("Socket::peerAddress", "Socket is unavailable");
}

bool Socket::isOpen() const
{
    return _socket && _socket->state() != QTcpSocket::UnconnectedState;
}

void Socket::socketDisconnected()
{
    emit disconnected();
}

void Socket::socketError(QAbstractSocket::SocketError socketError)
{
    LOG_AS("Socket::socketError");
    LOG_WARNING(_socket->errorString());

    emit error(socketError);
}

bool Socket::hasIncoming() const
{
    return !_receivedMessages.empty();
}

dsize Socket::bytesBuffered() const
{
    return _bytesToBeWritten;
}

void Socket::bytesWereWritten(qint64 bytes)
{
    _bytesToBeWritten -= bytes;
}
