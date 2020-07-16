/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * @page sysNetwork Low-Level Networking
 *
 * On server-side connected clients can be either in "unjoined" mode or
 * "joined" mode. The former is for querying information about the server's
 * status, while the latter one is for clients participating in the on-going
 * game.
 *
 * In unjoined mode, messages are not compressed.
 * @todo Document uncompressed message format.
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
 * - 1 byte: (payload size >> 7) | (0x40 for deflated, otherwise Huffman)
 * - @em n bytes: payload contents (as produced by ZipFile::compressAtLevel()).
 *
 * @par >= 4096 bytes (up to 4MB)
 * Large messages are compressed using the best zlib deflate level.
 * Message structure:
 * - 1 byte: 0x80 | (payload size & 0x7f)
 * - 1 byte: 0x80 | (payload size >> 7) & 0x7f
 * - 1 byte: payload size >> 14
 * - @em n bytes: payload contents (as produced by ZipFile::compressAtLevel()).
 *
 * Messages larger than or equal to 2^22 bytes (about 4MB) must be broken into
 * smaller pieces before sending.
 *
 * @see Protocol_Send()
 * @see Protocol_Receive()
 */

#include "de/socket.h"

#include "de/loop.h"
#include "de/message.h"
#include "de/reader.h"
#include "de/taskpool.h"
#include "de/waitable.h"
#include "de/writer.h"
#include "de/huffman.h"

#include <the_Foundation/object.h>

namespace de {

struct Counters
{
    duint64 sentUncompressedBytes = 0;
    duint64 sentBytes = 0;
    duint64 sentPeriodBytes = 0;
    double outputBytesPerSecond = 0;
    Time periodStartedAt;
};
static LockableT<Counters> counters;
static constexpr TimeSpan sendPeriodDuration = 5.0_s;

/// Maximum number of channels.
static const duint MAX_CHANNELS = 2;

static const int MAX_SIZE_SMALL  = 127; // bytes
static const int MAX_SIZE_MEDIUM = 4095; // bytes
static const int MAX_SIZE_BIG    = 10*MAX_SIZE_MEDIUM;
static const int MAX_SIZE_LARGE  = DE_SOCKET_MAX_PAYLOAD_SIZE;

/// Threshold for input data size: messages smaller than this are first compressed
/// with Doomsday's Huffman codes. If the result is smaller than the deflated data,
/// the Huffman coded payload is used (unless it doesn't fit in a medium-sized packet).
static const int MAX_HUFFMAN_INPUT_SIZE = 4096; // bytes

#define TRMF_CONTINUE           0x80
#define TRMF_DEFLATED           0x40
#define TRMF_SIZE_MASK          0x7f
#define TRMF_SIZE_MASK_MEDIUM   0x3f
#define TRMF_SIZE_SHIFT         7

namespace internal {

/**
 * Network message header.
 */
struct MessageHeader : public ISerializable
{
    dsize size;
    bool  isHuffmanCoded;
    bool  isDeflated;
    duint channel; /// @todo include in the written header

    MessageHeader() : size(0), isHuffmanCoded(false), isDeflated(false), channel(0)
    {}

    void operator>>(Writer &writer) const
    {
        if (size <= MAX_SIZE_SMALL && !isDeflated)
        {
            writer << dbyte(size);
        }
        else if (size <= MAX_SIZE_MEDIUM)
        {
            writer << dbyte(TRMF_CONTINUE | (size & TRMF_SIZE_MASK));
            writer << dbyte((isDeflated? TRMF_DEFLATED : 0) | (size >> TRMF_SIZE_SHIFT));
        }
        else if (size <= MAX_SIZE_LARGE)
        {
            DE_ASSERT(isDeflated);

            writer << dbyte(TRMF_CONTINUE | (size & TRMF_SIZE_MASK));
            writer << dbyte(TRMF_CONTINUE | ((size >> TRMF_SIZE_SHIFT) & TRMF_SIZE_MASK));
            writer << dbyte(size >> (2*TRMF_SIZE_SHIFT));
        }
        else
        {
            // Not supported.
            DE_ASSERT_FAIL("[MessageHeader::operator>>] Message is too long");
        }
    }

    /**
    * Throws an exception if the header is malformed/incomplete.
    */
    void operator<<(Reader &reader)
    {
        // Start reading the header.
        dbyte b;
        reader >> b;

        size = b & TRMF_SIZE_MASK;

        isDeflated = false;
        isHuffmanCoded = true;

        if (b & TRMF_CONTINUE) // More follows...
        {
            reader >> b;

            if (b & TRMF_CONTINUE) // Yet more to come...
            {
                // Large header.
                isDeflated = true;
                isHuffmanCoded = false;
                size |= ((b & TRMF_SIZE_MASK) << TRMF_SIZE_SHIFT);

                reader >> b;

                size |= (b << (2*TRMF_SIZE_SHIFT));
            }
            else
            {
                // Medium header.
                if (b & TRMF_DEFLATED)
                {
                    isDeflated = true;
                    isHuffmanCoded = false;
                }
                size |= ((b & TRMF_SIZE_MASK_MEDIUM) << TRMF_SIZE_SHIFT);
            }
        }
    }
};

} // namespace internal

using namespace internal;

DE_PIMPL_NOREF(Socket)
{
    Waitable connecting;
    Address  peer;
    bool     quiet       = false;
    bool     retainOrder = true;

    enum ReceptionState { ReceivingHeader, ReceivingPayload };
    ReceptionState receptionState = ReceivingHeader;
    Block          receivedBytes;
    MessageHeader  incomingHeader;

    /// Number of the active channel.
    /// @todo Channel is not used at the moment.
    duint activeChannel = 0;

    /// Pointer to the internal socket data.
    tF::ref<iSocket> socket;

    /// Buffer for incoming received messages.
    List<Message *> receivedMessages;

    /// Number of bytes waiting to be written to the socket.
    //    dint64 bytesToBeWritten = 0;

    /// Number of bytes written to the socket so far.
    dint64 totalBytesWritten = 0;

    TaskPool tasks;
    Dispatch dispatch;

    ~Impl()
    {
        deleteAll(receivedMessages);
    }

    void serializeMessage(MessageHeader &header, Block &payload)
    {
        Block huffData;

        // Let's find the appropriate compression method of the payload. First see
        // if the encoded contents are under 128 bytes as Huffman codes.
        if (payload.size() <= MAX_HUFFMAN_INPUT_SIZE) // Potentially short enough.
        {
            huffData = codec::huffmanEncode(payload);
            if (int(huffData.size()) <= MAX_SIZE_SMALL)
            {
                // We'll use this.
                header.isHuffmanCoded = true;
                header.size = huffData.size();
                payload = huffData;
            }
            // Even if that didn't seem suitable, we'll keep it to compare against
            // the deflated payload.
        }

        /// @todo Messages broadcasted to multiple recipients are separately
        /// compressed for each TCP send -- should do only one compression per
        /// message.

        if (!header.size) // Try deflate.
        {
            const int level = 1; //(payload.size() < MAX_SIZE_BIG? 1 /*fast*/ : 9 /*best*/);
            const Block deflated = payload.compressed(level);

            if (!deflated.size())
            {
                throw ProtocolError("Socket::send:", "Failed to deflate message payload");
            }
            if (deflated.size() > MAX_SIZE_LARGE)
            {
                throw ProtocolError("Socket::send",
                                    stringf("Compressed payload is too large (%zu bytes)", deflated.size()));
            }

            // Choose the smallest compression.
            if (huffData.size() && huffData.size() <= deflated.size() && int(huffData.size()) <= MAX_SIZE_MEDIUM)
            {
                // Huffman yielded smaller payload.
                header.isHuffmanCoded = true;
                header.size = huffData.size();
                payload = huffData;
            }
            else
            {
                // Use the deflated payload.
                header.isDeflated = true;
                header.size = deflated.size();
                payload = deflated;
            }
        }
    }

    void sendMessage(const MessageHeader &header, const Block &payload)
    {
        DE_ASSERT(socket);

        // Write the message header.
        Block dest;
        Writer(dest) << header;
        write_Socket(socket, dest);

        // Update totals (for statistics).
        const dsize total = dest.size() + payload.size();
//        bytesToBeWritten  += total;
        totalBytesWritten += total;

        write_Socket(socket, payload);

        // Update total counters, too.
        {
            DE_GUARD(counters);
            counters.value.sentPeriodBytes += total;
            counters.value.sentBytes       += total;
            // Update Bps counter.
            if (!counters.value.periodStartedAt.isValid()
                || counters.value.periodStartedAt.since() > sendPeriodDuration)
            {
                counters.value.outputBytesPerSecond = double(counters.value.sentPeriodBytes)
                                                    / sendPeriodDuration;
                counters.value.sentPeriodBytes = 0;
                counters.value.periodStartedAt = Time::currentHighPerformanceTime();
            }
        }
    }

    void serializeAndSendMessage(const IByteArray &packet)
    {
        Block payload = packet;
        {
            DE_GUARD(counters);
            counters.value.sentUncompressedBytes += payload.size();
        }

        if (!retainOrder && packet.size() >= MAX_SIZE_BIG)
        {
            struct WorkData : public Deletable {
                MessageHeader header;
                Block         payload;
            };

            // Prepare for sending in a background thread, since it may take a moment.
            tasks.async(
                [this, payload]() {
                    WorkData data;
                    data.payload = payload;
                    serializeMessage(data.header, data.payload);
                    return data;
                },
                [this](const Variant &var) {
                    if (socket)
                    {
                        const auto &data = var.value<WorkData>();
                        // Write to socket in main thread.
                        sendMessage(data.header, data.payload);
                    }
                });
        }
        else
        {
            MessageHeader header;
            serializeMessage(header, payload);
            sendMessage(header, payload);
        }
    }

    /**
     * Checks the incoming bytes and sees if any messages can be formed.
     */
    void deserializeMessages()
    {
        for (;;)
        {
            if (receptionState == ReceivingHeader)
            {
                if (receivedBytes.size() < 2)
                {
                    // A message must be at least two bytes long (header + payload).
                    return;
                }
                try
                {
                    Reader reader(receivedBytes);
                    reader >> incomingHeader;
                    receptionState = ReceivingPayload;

                    // Remove the read bytes from the buffer.
                    receivedBytes.remove(0, reader.offset());
                }
                catch (const Error &)
                {
                    // It seems we don't have a full header yet.
                    return;
                }
            }

            if (receptionState == ReceivingPayload)
            {
                if (receivedBytes.size() >= incomingHeader.size)
                {
                    // Extract the payload from the incoming buffer.
                    Block payload = receivedBytes.left(incomingHeader.size);
                    receivedBytes.remove(0, incomingHeader.size);

                    // We have the full payload, but it still may need to uncompressed.
                    if (incomingHeader.isHuffmanCoded)
                    {
                        payload = codec::huffmanDecode(payload);
                        if (!payload.size())
                        {
                            throw ProtocolError("Socket::Impl::deserializeMessages",
                                                "Huffman decoding failed");
                        }
                    }
                    else if (incomingHeader.isDeflated)
                    {
                        payload = payload.decompressed(); //qUncompress(payload);
                        if (!payload.size())
                        {
                            throw ProtocolError("Socket::Impl::deserializeMessages",
                                                "Deflate failed");
                        }
                    }

                    receivedMessages << new Message(
                        Address(address_Socket(socket)), incomingHeader.channel, payload);

                    // We can proceed to the next message.
                    receptionState = ReceivingHeader;
                    incomingHeader = MessageHeader();
                }
                else
                {
                    // Let's wait until more is available.
                    return;
                }
            }
        }
    }

    static void handleAddressLookedUp(iAny *, const iAddress *addr)
    {
        Loop::mainCall([addr]() {
            Socket &self = *static_cast<Socket *>(userData_Object(addr));
            try
            {
                DE_FOR_OBSERVERS(i, self.audienceForStateChange())
                {
                    i->socketStateChanged(self, AddressResolved);
                }
                // Proceed with opening the connection.
                self.open(Address(addr));
            }
            catch (const Error &er)
            {
                DE_FOR_OBSERVERS(i, self.audienceForError())
                {
                    i->error(self, "Failed to look up address: " + er.asText());
                }
            }
        });
    }

    static void handleError(iAny *, iSocket *sock, int error, const char *msg)
    {
        Loop::mainCall([=]() {
            Socket &self = *static_cast<Socket *>(userData_Object(sock));
            if (!self.d->quiet)
            {
                LOG_NET_WARNING("%s") << msg;
            }
            DE_FOR_OBSERVERS(i, self.audienceForError())
            {
                i->error(self, Stringf("Socket error %i: %s", error, msg));
            }
        });
    }

    static void handleConnected(iAny *, iSocket *sock)
    {
        Loop::mainCall([sock]() {
            Socket &self = *static_cast<Socket *>(userData_Object(sock));
            self.d->connecting.post();
            DE_FOR_OBSERVERS(i, self.audienceForStateChange())
            {
                i->socketStateChanged(self, Connected);
            }
        });
    }

    static void handleDisconnected(iAny *, iSocket *sock)
    {
        Loop::mainCall([sock]() {
            Socket &self = *static_cast<Socket *>(userData_Object(sock));
            DE_FOR_OBSERVERS(i, self.audienceForStateChange())
            {
                i->socketStateChanged(self, Disconnected);
            }
        });
    }

    static void handleReadyRead(iAny *, iSocket *sock)
    {
        Loop::mainCall([sock]() {
            Socket &self = *static_cast<Socket *>(userData_Object(sock));
            Impl *d = self.d;
            d->receivedBytes += Block::take(readAll_Socket(sock));
            d->deserializeMessages();
            if (d->receivedMessages)
            {
                DE_FOR_OBSERVERS(i, self.audienceForMessage())
                {
                    i->messagesIncoming(self);
                }
            }
        });
    }

    static void handleWriteFinished(iAny *, iSocket *sock)
    {
        Socket *self = static_cast<Socket *>(userData_Object(sock));
        self->d->dispatch += [self]() {
            DE_FOR_OBSERVERS(i, self->audienceForAllSent())
            {
                i->allSent(*self);
            }
        };
    }

    DE_PIMPL_AUDIENCE(StateChange)
    DE_PIMPL_AUDIENCE(Message)
    DE_PIMPL_AUDIENCE(AllSent)
    DE_PIMPL_AUDIENCE(Error)
};

DE_AUDIENCE_METHOD(Socket, StateChange)
DE_AUDIENCE_METHOD(Socket, Message)
DE_AUDIENCE_METHOD(Socket, AllSent)
DE_AUDIENCE_METHOD(Socket, Error)

Socket::Socket()
    : d(new Impl)
{
//    initialize();
//    /QObject::connect(d->socket, SIGNAL(connected()), this, SIGNAL(connected()));
}

Socket::Socket(const Address &address, TimeSpan timeOut)
    : d(new Impl)
{
    LOG_AS("Socket");
    try
    {
        open(address);
        // This semaphore is made available when the connection is open.
        if (!d->connecting.tryWait(timeOut))
        {
            /// @throw ConnectionError  Connection timed out.
            throw ConnectionError("Socket", "Timeout trying to connect to " + address.asText());
        }
    }
    catch (...)
    {
        d.reset();
        throw;
    }

    //LOG_NET_NOTE("Connection opened to %s") << address.asText();

//    d->peer = address;

//    DE_ASSERT(d->socket->isOpen() && d->socket->isWritable() &&
//                 d->socket->state() == QAbstractSocket::ConnectedState);
}

void Socket::open(const Address &address) // non-blocking
{
//    DE_ASSERT(d->socket);
//    DE_ASSERT(d->socket->state() == QAbstractSocket::UnconnectedState);

    LOG_AS("Socket");
    if (!d->quiet) LOG_NET_MSG("Opening connection to %s") << address.asText();

    d->peer = address;
    d->socket = tF::make_ref(newAddress_Socket(address));
    initialize();

    // Now that the signals have been set...
//    d->socket->connectToHost(address.host(), address.port());
    //if (!d->socket->waitForConnected(int(timeOut.asMilliSeconds())))
    if (!open_Socket(d->socket))
    {
        /// @throw ConnectionError  Error when setting up the socket.
        throw ConnectionError("Socket", "Failed to open connection to " + address.asText());
    }

    //d->socket->connectToHost(address.host(), address.port());
//    d->peer = address;
//    d->socket.reset(newAddress_Socket(address));
//    initialize();
//    open_Socket(d->socket);
}

void Socket::open(const String &domainNameWithOptionalPort,
                  duint16       defaultPort) // non-blocking
{
    String  host = domainNameWithOptionalPort;
    duint16 port = defaultPort;

    if (host.contains(':'))
    {
        auto pos = host.lastIndexOf(':');
        port = duint16(host.substr(pos + 1).toInt());
        if (!port) port = defaultPort;
        host.truncate(pos);
    }
//    if (host == "localhost")
//    {
//        //open(Address(str.toLatin1(), port));
//        return;
//    }

//    QHostAddress host(str);
//    if (!host.isNull())
//    {
//        // Looks like a regular IP address.
//        open(Address(str.toLatin1(), port));
//        return;
//    }

//    d->peer.setPort(port);

    iAddress *addr = new_Address();
    setUserData_Object(addr, this);
    iConnect(Address, addr, lookupFinished, addr, Impl::handleAddressLookedUp);
    lookupTcpCStr_Address(addr, host, port);
    d->peer = Address::take(addr);



    // Looks like we will need to look this up.
//    QHostInfo::lookupHost(str, this, SLOT(hostResolved(QHostInfo)));
}

//void Socket::reconnect()
//{
//    DE_ASSERT(!isOpen());

//    open(d->peer);
//}

Socket::Socket(iSocket *existingSocket) : d(new Impl)
{
    d->socket.reset(existingSocket);
    initialize();

    // Maybe we missed an earlier signal, since we are only now getting ownership.
    Impl::handleReadyRead(nullptr, d->socket);
}

Socket::~Socket()
{
    close();
//    delete d->socket;
}

//void Socket::socketDestroyed()
//{
//    // The socket is gone...
//    d->socket.reset();
//}

void Socket::initialize()
{
    // Options.
    //d->socket->setSocketOption(QTcpSocket::LowDelayOption, 1); // prefer short buffering

    setUserData_Object(d->socket, this);

    iConnect(Socket, d->socket, writeFinished, d->socket, Impl::handleWriteFinished);
    iConnect(Socket, d->socket, error,         d->socket, Impl::handleError);
    iConnect(Socket, d->socket, connected,     d->socket, Impl::handleConnected);
    iConnect(Socket, d->socket, disconnected,  d->socket, Impl::handleDisconnected);
    iConnect(Socket, d->socket, readyRead,     d->socket, Impl::handleReadyRead);

//    QObject::connect(d->socket, SIGNAL(bytesWritten(qint64)), this, SLOT(bytesWereWritten(qint64)));
//    QObject::connect(d->socket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
//    QObject::connect(d->socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)), Qt::DirectConnection);
//    QObject::connect(d->socket, SIGNAL(readyRead()), this, SLOT(readIncomingBytes()));
//    QObject::connect(d->socket, SIGNAL(destroyed()), this, SLOT(socketDestroyed()));

    /*QObject::connect(d->socket, &QAbstractSocket::stateChanged, [] (QAbstractSocket::SocketState state)
    {
        qDebug() << "socket state:" << state;
    });*/
}

void Socket::close()
{
    if (!d->socket) return;

//    if (status_Socket(d->socket) == connected_SocketStatus) //d->socket->state() == QAbstractSocket::ConnectedState)
//    {
        // All pending data will be written to the socket before closing.
//        d->socket->disconnectFromHost();
    close_Socket(d->socket);
//    }
//    else
//    {
//        d->socket->abort();
//    }

//    if (d->socket->state() != QAbstractSocket::UnconnectedState)
//    {
//        // Make sure the socket is disconnected before the return.
//        d->socket->waitForDisconnected();
//    }

//    d->socket->close();
}

void Socket::setQuiet(bool noLogOutput)
{
    d->quiet = noLogOutput;
}

void Socket::resetCounters()
{
    DE_GUARD(counters);
    counters.value = Counters();
}

duint64 Socket::sentUncompressedBytes()
{
    DE_GUARD(counters);
    return counters.value.sentUncompressedBytes;
}

duint64 Socket::sentBytes()
{
    DE_GUARD(counters);
    return counters.value.sentBytes;
}

double Socket::outputBytesPerSecond()
{
    DE_GUARD(counters);
    return counters.value.outputBytesPerSecond;
}

duint Socket::channel() const
{
    return d->activeChannel;
}

void Socket::setChannel(duint number)
{
    DE_ASSERT(number < MAX_CHANNELS);
    d->activeChannel = min(number, MAX_CHANNELS - 1);
}

void Socket::setRetainOrder(bool retainOrder)
{
    d->retainOrder = retainOrder;
}

void Socket::send(const IByteArray &packet)
{
    send(packet, d->activeChannel);
}

Socket &Socket::operator<<(const IByteArray &packet)
{
    send(packet, d->activeChannel);
    return *this;
}

void Socket::send(const IByteArray &packet, duint /*channel*/)
{
    if (!d->socket)
    {
        /// @throw DisconnectedError Sending is not possible because the socket has been closed.
        throw DisconnectedError("Socket::send", "Socket is unavailable");
    }

    // Sockets must be used only in their own thread.
//    DE_ASSERT(thread() == QThread::currentThread());

    d->serializeAndSendMessage(packet);
}

/*
void Socket::hostResolved(const QHostInfo &info)
{
    if (info.error() != QHostInfo::NoError || info.addresses().isEmpty())
    {
        LOG_NET_ERROR("Could not resolve host: ") << info.errorString();
        emit disconnected();
    }
    else
    {
        // Now we know where to connect.
        open(Address(info.addresses().first(), d->peer.port()));

        emit addressResolved();
    }
}
*/

Message *Socket::receive()
{
    if (d->receivedMessages.isEmpty())
    {
        return nullptr;
    }
    return d->receivedMessages.takeFirst();
}

Message *Socket::peek()
{
    if (d->receivedMessages.isEmpty())
    {
        return nullptr;
    }
    return d->receivedMessages.first();
}

void Socket::flush()
{
    if (d->socket)
    {
        // Wait until data has been written.
        flush_Socket(d->socket);
    }
}

Address Socket::peerAddress() const
{
    if (isOpen() && d->socket && status_Socket(d->socket) == connected_SocketStatus)
    {
        return Address(address_Socket(d->socket));
    }
    return d->peer;
}

bool Socket::isOpen() const
{
    if (!d->socket)
    {
        return !d->peer.isNull();
    }
    const auto st = status_Socket(d->socket);
    return d->socket && (st == connecting_SocketStatus || st == connected_SocketStatus);
}

bool Socket::isLocal() const
{
    return peerAddress().isLocal();
}

//void Socket::socketDisconnected()
//{
//    emit disconnected();
//}

//void Socket::socketError(QAbstractSocket::SocketError socketError)
//{
//    if (socketError != QAbstractSocket::SocketTimeoutError)
//    {
//        LOG_AS("Socket");
//        if (!d->quiet) LOG_NET_WARNING(d->socket->errorString());

//        emit error(d->socket->errorString());
//        emit disconnected();
//    }
//}

bool Socket::hasIncoming() const
{
    return !d->receivedMessages.empty();
}

dsize Socket::bytesBuffered() const
{
    return bytesToSend_Socket(d->socket);
}

//void Socket::bytesWereWritten(qint64 bytes)
//{
//    d->bytesToBeWritten -= bytes;
//    DE_ASSERT(d->bytesToBeWritten >= 0);

//    if (d->bytesToBeWritten == 0)
//    {
//        emit allSent();
//    }
//}

} // namespace de
