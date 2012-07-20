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

#ifndef LIBDENG2_SOCKET_H
#define LIBDENG2_SOCKET_H

#include "../libdeng2.h"
#include "../IByteArray"
#include "../Address"
#include "../Transmitter"

#include <QTcpSocket>
#include <QList>
#include <QFlags>

/// Largest message sendable using the protocol.
#define DENG2_SOCKET_MAX_PAYLOAD_SIZE (1 << 22) // 4 MB

namespace de {

class Message;

/**
 * TCP/IP network socket.
 *
 * ListenSocket constructs Socket instances for incoming connections.
 *
 * @ingroup net
 */
class DENG2_PUBLIC Socket : public QObject, public Transmitter
{
    Q_OBJECT

public:
    /// Creating the TCP/IP connection failed. @ingroup errors
    DENG2_ERROR(ConnectionError)

    /// Error subclass for all situations where the socket is left unusable. @ingroup errors
    DENG2_ERROR(BrokenError)

    /// The TCP/IP connection was disconnected. @ingroup errors
    DENG2_SUB_ERROR(BrokenError, DisconnectedError)

    /// Encountered a problem related to the messaging protocol. @ingroup errors
    DENG2_SUB_ERROR(BrokenError, ProtocolError)

    /// There is no peer connected. @ingroup errors
    DENG2_SUB_ERROR(BrokenError, PeerError)

    /// These flags are written to the sent headers.
    enum HeaderFlag {
        Huffman = 0x1,
        Channel1 = 0x2
    };
    Q_DECLARE_FLAGS(HeaderFlags, HeaderFlag)

public:
    /**
     * Opens a socket to @a address and waits (blocks) until the connection has
     * been formed. The socket is ready to be used after the constructor
     * returns. If the connection can be formed within the specified @a timeOut
     * threshold, an exception is thrown.
     *
     * @param address  Address to connect to.
     * @param timeOut  Maximum time to wait for connection.
     */
    Socket(const Address& address, const Time::Delta& timeOut = 5);

    virtual ~Socket();

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
    Socket& operator << (const IByteArray& data);

    /**
     * Returns the next received message. If nothing has been received,
     * returns @c NULL.
     *
     * @return  Received bytes. Caller gets ownership of the message.
     */
    Message* receive();

    /**
     * Returns a pointer to the next received message, if one is available.
     *
     * @return  Message. Socket retains ownership.
     */
    Message* peek();

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

signals:
    void messagesReady();
    void connectionFailure();
    void disconnected();

public slots:
    void socketDisconnected();
    void socketError(QAbstractSocket::SocketError socketError);
    void readIncomingBytes();

private slots:
    void bytesWereWritten(qint64 bytes);
    void socketDestroyed();

protected:
    /// Create a Socket object for a previously opened socket.
    Socket(QTcpSocket* existingSocket);

    void initialize();

    /**
     * Receives a specific number of bytes from the socket. Blocks
     * until all the data has been read correctly. An exception is
     * thrown if the connection fails.
     */
    void receiveBytes(duint count, dbyte* buffer);

    void send(const IByteArray& packet, duint channel);

private:
    struct Instance;
    Instance* d;

    /**
     * ListenSocket creates instances of Socket so it needs to use
     * the special private constructor that takes an existing
     * socket data pointer as a parameter.
     */
    friend class ListenSocket;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Socket::HeaderFlags)

} // namespace de

#endif // LIBDENG2_SOCKET_H
