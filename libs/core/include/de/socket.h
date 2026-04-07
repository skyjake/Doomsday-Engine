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

#ifndef LIBCORE_SOCKET_H
#define LIBCORE_SOCKET_H

#include "de/libcore.h"
#include "de/ibytearray.h"
#include "de/address.h"
#include "de/transmitter.h"
#include "de/observers.h"

#include <the_Foundation/socket.h>

/// Largest message sendable using the protocol.
#define DE_SOCKET_MAX_PAYLOAD_SIZE (1 << 22) // 4 MB

namespace de {

class Message;

/**
 * TCP/IP network socket.
 *
 * ListenSocket constructs Socket instances for incoming connections.
 *
 * Note that Socket instances must always be used in the same thread as ListenSocket.
 * Socket uses a background thread for compressing large messages before sending. This
 * means they may be sent out-of-order with regards to all other messages.
 *
 * @ingroup net
 */
class DE_PUBLIC Socket : public Transmitter
{
public:
    enum SocketState { AddressResolved, Disconnected, Connected };

    DE_AUDIENCE(StateChange, void socketStateChanged(Socket &, SocketState state))
    DE_AUDIENCE(Message,     void messagesIncoming(Socket &))
    DE_AUDIENCE(AllSent,     void allSent(Socket &))
    DE_AUDIENCE(Error,       void error(Socket &, const String &errorMessage))
    
    /// Creating the TCP/IP connection failed. @ingroup errors
    DE_ERROR(ConnectionError);

    /// Error subclass for all situations where the socket is left unusable. @ingroup errors
    DE_ERROR(BrokenError);

    /// The TCP/IP connection was disconnected. @ingroup errors
    DE_SUB_ERROR(BrokenError, DisconnectedError);

    /// Encountered a problem related to the messaging protocol. @ingroup errors
    DE_SUB_ERROR(BrokenError, ProtocolError);

    /// There is no peer connected. @ingroup errors
    DE_SUB_ERROR(BrokenError, PeerError);

    /// These flags are written to the sent headers.
    enum HeaderFlag {
        Huffman = 0x1,
        Channel1 = 0x2
    };
    using HeaderFlags = Flags;

public:
    Socket();

    /**
     * Opens a socket to @a address and waits (blocks) until the connection has
     * been formed. The socket is ready to be used after the constructor
     * returns. If the connection can be formed within the specified @a timeOut
     * threshold, an exception is thrown.
     *
     * @param address  Address to connect to.
     * @param timeOut  Maximum time to wait for connection.
     */
    Socket(const Address &address, TimeSpan timeOut);

    virtual ~Socket();

    /**
     * Opens a connection to @a address and returns immediately. The
     * connected() signal is emitted when the connection is ready to use.
     *
     * @param address  Address to connect to.
     */
    void open(const Address &address);

    /**
     * Opens a connection to a host and returns immediately. If the IP address
     * is not known, this must be called instead of connect() as Address does
     * not work with domain names.
     *
     * The connected() signal is emitted when the connection is ready to use. A
     * disconnected() signal is emitted if the domain name lookup fails.
     *
     * @param domainNameWithOptionalPort  Domain name or IP address, with
     * optional port appended (e.g., "myhost.com:13209").
     * @param defaultPort  Port number to use if not specified in the first argument.
     */
    void open(const String &domainNameWithOptionalPort, duint16 defaultPort = 0);

    /**
     * Returns the currently active channel.
     */
    duint channel() const;

    /**
     * Sets the active channel.
     *
     * @param number  Channel number.
     */
    void setChannel(duint number);

    /**
     * Specifies whether all sent messages need to be written out in the order they have
     * been sent. The default is to retain order.
     *
     * If the messaging protocol in use does not require messages to be ordered, setting
     * this to @c false allows Socket to compress large messages in a background thread
     * and send them when ready, instead of blocking.
     *
     * @param retainOrder  @c true to keep send order, @c false to allow order to be
     *                     unpredictable.
     */
    void setRetainOrder(bool retainOrder);

    // Implements Transmitter.
    /**
     * Sends the given data over the socket.  Copies the data into
     * a temporary buffer before sending. The data is sent on the current
     * sending channel.
     *
     * @param packet  Data to send.
     */
    void send(const IByteArray &packet);

    /**
     * Sends the given data over the socket.  Copies the data into
     * a temporary buffer before sending. The data is sent on the current
     * sending channel.
     *
     * @param data  Data to send.
     *
     * @return  Reference to this socket.
     */
    Socket &operator<<(const IByteArray &data);

    /**
     * Returns the next received message. If nothing has been received,
     * returns @c NULL.
     *
     * @return  Received bytes. Caller gets ownership of the message.
     */
    Message *receive();

    /**
     * Returns a pointer to the next received message, if one is available.
     *
     * @return  Message. Socket retains ownership.
     */
    Message *peek();

    /**
     * Determines the IP address and port of the remote end of a connected socket.
     *
     * @return  Address of the peer.
     */
    Address peerAddress() const;

    /**
     * Determines if the socket is open for communications.
     */
    bool isOpen() const;

    /**
     * Determines if the socket is on the local computer.
     */
    bool isLocal() const;

    /**
     * Determines whether there are any incoming messages waiting.
     */
    bool hasIncoming() const;

    /**
     * Determines the amount of data waiting to be sent out.
     */
    dsize bytesBuffered() const;

    /**
     * Blocks until all outgoing data has been written to the socket.
     */
    void flush();

    /**
     * Close the socket.
     */
    void close();

    /**
     * Allows or disallows the socket from outputting log output.
     *
     * @param noLogOutput  @c true to be quiet, @c false to allow output.
     */
    void setQuiet(bool noLogOutput);

    // Statistics:
    static void    resetCounters();
    static duint64 sentUncompressedBytes();
    static duint64 sentBytes();
    static double  outputBytesPerSecond();

protected:
    /// Create a Socket object for a previously opened socket.
    Socket(iSocket *existingSocket);

    void initialize();

    /**
     * Receives a specific number of bytes from the socket. Blocks
     * until all the data has been read correctly. An exception is
     * thrown if the connection fails.
     */
    void receiveBytes(duint count, dbyte *buffer);

    void send(const IByteArray &packet, duint channel);

private:
    DE_PRIVATE(d)

    /**
     * ListenSocket creates instances of Socket so it needs to use
     * the special private constructor that takes an existing
     * socket data pointer as a parameter.
     */
    friend class ListenSocket;
};

} // namespace de

#endif // LIBCORE_SOCKET_H
