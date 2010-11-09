/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_LINK_H
#define LIBDENG2_LINK_H

#include "../Transceiver"
#include "../SenderThread"
#include "../ReceiverThread"
#include "../Observers"
#include "../Flag"

namespace de
{   
    class Address;
    class Packet;
    class Socket;
    
    /**
     * Network communications link.
     *
     * @ingroup net
     */
    class LIBDENG2_API Link : public Transceiver
    {
    public:
        /// The remote end has closed the link. @ingroup errors
        DEFINE_ERROR(DisconnectedError);
        
        /// Sending on channel 1 instead of the default 0.
        DEFINE_FINAL_FLAG(CHANNEL_1, 0, Mode);
        
        typedef SenderThread::OutgoingBuffer OutgoingBuffer;
        typedef ReceiverThread::IncomingBuffer IncomingBuffer;
        
    public:
        /**
         * Constructs a new communications link. A new socket is created for the link.
         *
         * @param address  Address to connect to.
         */
        Link(const Address& address);
        
        /**
         * Constructs a new communications link.
         *
         * @param socket  Socket for the link. Link gets ownership.
         */
        Link(Socket* socket);
        
        virtual ~Link();

        /**
         * Checks if any incoming data has been received.
         */
        bool hasIncoming() const;
        
        /**
         * Wait until all data has been sent.
         */
        void flush();

        /**
         * Returns the socket over which the Link communicates.
         *
         * @return  Socket.
         */ 
        Socket& socket() { return *_socket; }
        
        /**
         * Returns the address of the remote end of the link.
         */ 
        Address peerAddress() const;
        
        // Implements Transceiver.
        void send(const IByteArray& data);
        Message* receive();
      
    protected:
        void initialize();

    public:
        DEFINE_AUDIENCE(Deletion, void linkBeingDeleted(Link& link));

        /// Mode flags.
        Mode mode;
    
    private:
        /// Socket over which the link communicates.
        Socket* _socket; 

        /// Address of the remote end.
        Address _peerAddress;
        
        /// Thread that writes outgoing data to the socket.
        SenderThread* _sender;
        
        /// Thread that reads incoming data from the socket.
        ReceiverThread* _receiver;
        
        OutgoingBuffer _outgoing;
        IncomingBuffer _incoming;
    };
}

#endif /* LIBDENG2_LINK_H */
