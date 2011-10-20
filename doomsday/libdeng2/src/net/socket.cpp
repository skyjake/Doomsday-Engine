/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/Writer"
#include "de/Reader"

#include <QThread>

using namespace de;

/// Version of the block transfer protocol.
static const duint PROTOCOL_VERSION = 0;

/// Size of the message header in bytes.
static const duint HEADER_SIZE = 4;

/// Maximum number of channels.
static const duint MAX_CHANNELS = 2;

struct Socket::Instance
{
    enum ReceptionState {
        ReceivingHeader,
        ReceivingPayload
    };
    ReceptionState receptionState;

    /// Apply Huffman encoding to payload data.
    bool useHuffman;

    /// Number of the active channel.
    duint activeChannel;

    Header incomingHeader;

    /// Pointer to the internal socket data.
    QTcpSocket* socket;

    /// Buffer for incoming received messages.
    QList<Message*> receivedMessages;

    /// Number of bytes waiting to be written to the socket.
    dint64 bytesToBeWritten;

    Instance() :
        receptionState(ReceivingHeader),
        useHuffman(false),
        activeChannel(0),
        socket(0),
        bytesToBeWritten(0) {}

    ~Instance() {
        // Delete received messages left in the buffer.
        foreach(Message* msg, receivedMessages) delete msg;
    }
};

Socket::Header::Header() : version(PROTOCOL_VERSION), huffman(false), channel(0), size(0) {}

Socket::Socket(const Address& address)   
{
    d = new Instance;
    d->socket = new QTcpSocket;
    initialize();

    // Now that the signals have been set...
    d->socket->connectToHost(address.host(), address.port());
    if(!d->socket->waitForConnected(5000))
    {
        delete d->socket;
        delete d;
        d = 0;

        // Timed out!
        /// @throw ConnectionError Connection did not open in time.
        throw ConnectionError("Socket: Opening the connection to " + address.asText() + " timed out.");
    }
}

Socket::Socket(QTcpSocket* existingSocket)
{
    d = new Instance;
    d->socket = existingSocket;
    initialize();
}

Socket::~Socket()
{
    close();
    delete d->socket;
    delete d;
}

void Socket::initialize()
{
    // Options.
    d->socket->setSocketOption(QTcpSocket::LowDelayOption, 1);

    connect(d->socket, SIGNAL(bytesWritten(qint64)), this, SLOT(bytesWereWritten(qint64)));
    connect(d->socket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
    connect(d->socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)), Qt::DirectConnection);
    connect(d->socket, SIGNAL(readyRead()), this, SLOT(readIncomingBytes()));
}

void Socket::close()
{
    flush();

    // Prevent emitting of any more signals.
    d->socket->disconnect();

    d->socket->close();
}

duint Socket::channel() const
{
    return d->activeChannel;
}

void Socket::setChannel(duint number)
{
    DENG2_ASSERT(number < MAX_CHANNELS);
    d->activeChannel = number;
}

void Socket::send(const IByteArray &packet)
{
    send(packet, d->activeChannel);
}

Socket& Socket::operator << (const IByteArray& packet)
{
    send(packet, d->activeChannel);
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
        (header.huffman? Huffman : 0) |
        (header.channel == 1? Channel1 : 0);

    duint32 bits = ( (header.size & 0x7ffffff) |
                     ((header.version & 3) << 27) |
                     (flags << 29) );

    return bits;
}

void Socket::unpackHeader(duint32 headerBytes, Header& header)
{
    duint flags = (headerBytes >> 29) & 7;
    
    header.version = (headerBytes >> 27) & 3;
    header.huffman = ((flags & Huffman) != 0);
    header.channel = (flags & Channel1? 1 : 0);
    header.size = headerBytes & 0x7ffffff;
}

void Socket::send(const IByteArray& packet, duint channel)
{
    if(!d->socket)
    {
        /// @throw DisconnectedError Sending is not possible because the socket has been closed.
        throw DisconnectedError("Socket::operator << ", "Socket is unavailable");
    }

    // Keep track of where we are with the traffic.
    // A header is also included.
    d->bytesToBeWritten += packet.size() + 4;

    // Write the packet header: packet length, version, possible flags.
    Header header;
    header.channel = channel;
    header.size = packet.size();

    Block dest;
    Writer(dest) << packHeader(header);
    d->socket->write(dest);

    // Write the data itself.
    d->socket->write(Block(packet));
}

void Socket::readIncomingBytes()
{
    forever
    {
        if(d->receptionState == Instance::ReceivingHeader)
        {
            if(d->socket->bytesAvailable() >= HEADER_SIZE)
            {
                // Get the header.
                duint32 bits = 0;
                Reader(Block(d->socket->read(HEADER_SIZE))) >> bits;
                unpackHeader(bits, d->incomingHeader);

                // Check for valid protocols.
                if(d->incomingHeader.version > PROTOCOL_VERSION)
                {
                    /// @throw UnknownProtocolError The received data block's protocol version number
                    /// was not recognized. This is probably because the remote end is not a libdeng2
                    /// application.
                    throw UnknownProtocolError("Socket::readIncomingBytes", "Incoming packet has unknown protocol");
                }

                d->receptionState = Instance::ReceivingPayload;
            }
            else
            {
                // Let's wait until more is available.
                break;
            }
        }

        if(d->receptionState == Instance::ReceivingPayload)
        {
            if(d->socket->bytesAvailable() >= d->incomingHeader.size)
            {
                d->receivedMessages.append(new Message(Address(d->socket->peerAddress(),
                                                               d->socket->peerPort()),
                                                       d->incomingHeader.channel,
                                                       Block(d->socket->read(d->incomingHeader.size))));

                // Get the next message.
                d->receptionState = Instance::ReceivingHeader;
            }
            else
            {
                // Let's wait until more is available.
                break;
            }
        }
    }

    // Notification about available messages.
    if(!d->receivedMessages.isEmpty())
    {
        emit messagesReady();
    }
}

Message* Socket::receive()
{
    if(d->receivedMessages.isEmpty())
    {
        return 0;
    }
    return d->receivedMessages.takeFirst();
}

Message* Socket::peek()
{
    if(d->receivedMessages.isEmpty())
    {
        return 0;
    }
    return d->receivedMessages.first();
}

void Socket::flush()
{
    // Wait until data has been written.
    d->socket->flush();
    d->socket->waitForBytesWritten();
}

Address Socket::peerAddress() const
{
    if(isOpen())
    {
        return Address(d->socket->peerAddress(), d->socket->peerPort());
    }
    /// @throw PeerError Could not determine the TCP/IP address of the socket.
    throw PeerError("Socket::peerAddress", "Socket is unavailable");
}

bool Socket::isOpen() const
{
    return d->socket && d->socket->state() != QTcpSocket::UnconnectedState;
}

void Socket::socketDisconnected()
{
    emit disconnected();
}

void Socket::socketError(QAbstractSocket::SocketError /*socketError*/)
{
    LOG_AS("Socket");
    LOG_WARNING(d->socket->errorString());

    emit disconnected(); //error(socketError);
}

bool Socket::hasIncoming() const
{
    return !d->receivedMessages.empty();
}

dsize Socket::bytesBuffered() const
{
    return d->bytesToBeWritten;
}

void Socket::bytesWereWritten(qint64 bytes)
{
    d->bytesToBeWritten -= bytes;
    DENG2_ASSERT(d->bytesToBeWritten >= 0);
}
