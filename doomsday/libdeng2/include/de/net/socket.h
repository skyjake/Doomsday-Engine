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

#ifndef LIBDENG2_SOCKET_H
#define LIBDENG2_SOCKET_H

#include "../deng.h"
#include "../IByteArray"
#include "../Address"
#include "../Flag"

#include <QTcpSocket>
#include <QList>

/**
 * @defgroup net Network
 *
 * Classes responsible for network communications.
 */

namespace de
{
    class Message;

    /**
     * TCP/IP network socket.
     *
     * ListenSocket constructs Socket instances for incoming connections.
     *
     * @ingroup net
     */
    class LIBDENG2_API Socket : public QObject
    {
        Q_OBJECT

    public:
        /// Payload is in Huffman code.
        DEFINE_FLAG(HUFFMAN, 0); 

        /// Payload belongs to channel 1 instead of the default channel 0.
        DEFINE_FINAL_FLAG(CHANNEL_1, 1, Mode);
        
        /// Creating the TCP/IP connection failed. @ingroup errors
        DEFINE_ERROR(ConnectionError);

        /// Error subclass for all situations where the socket is left unusable. @ingroup errors
        DEFINE_ERROR(BrokenError);
        
        /// The TCP/IP connection was disconnected. @ingroup errors
        DEFINE_SUB_ERROR(BrokenError, DisconnectedError);
        
        /// Incoming packet has an unknown block protocol. @ingroup errors
        DEFINE_SUB_ERROR(BrokenError, UnknownProtocolError);

        /// There is no peer connected. @ingroup errors
        DEFINE_SUB_ERROR(BrokenError, PeerError);

    public:
        Socket(const Address& address);
        virtual ~Socket();

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

        void close();

    signals:
        void messagesReady();
        void disconnected();
        void error(QAbstractSocket::SocketError error);

    public slots:
        void socketDisconnected();
        void socketError(QAbstractSocket::SocketError socketError);
        void readIncomingBytes();

    protected slots:
        void bytesWereWritten(qint64 bytes);

    protected:
        /// Values for the transmission block header.
        struct Header {
            /// These flags are written to the sent headers.
            enum Flags {
                HUFFMAN = 0x1,
                CHANNEL_1 = 0x2
            };
            duint version;
            bool huffman;
            duint channel;
            duint size;
            Header();
        };
        
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
        
        duint32 packHeader(const Header& header);

        void unpackHeader(duint32 headerBytes, Header& header);
        
        //inline void checkValid();

    public:
        /// Operating mode.
        Mode mode;
    
    private:
        enum ReceptionState
        {
            RECEIVING_HEADER,
            RECEIVING_PAYLOAD
        };

        ReceptionState _receptionState;

        Header _incomingHeader;

        /// Pointer to the internal socket data.
        QTcpSocket* _socket;

        /// Buffer for incoming received messages.
        QList<Message*> _receivedMessages;

        /// Number of bytes waiting to be written to the socket.
        dsize _bytesToBeWritten;

        /** 
         * ListenSocket creates instances of Socket so it needs to use
         * the special private constructor that takes an existing
         * socket data pointer as a parameter.
         */
        friend class ListenSocket;
    };
}

#endif /* LIBDENG2_SOCKET_H */
