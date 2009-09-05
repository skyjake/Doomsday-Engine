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

#ifndef LIBDENG2_SENDERTHREAD_H
#define LIBDENG2_SENDERTHREAD_H

#include "../Thread"
#include "../Block"
#include "../WaitableFIFO"

namespace de
{
    class Message;
    class Socket;

    /**
     * The sender thread is responsible for retrieving outgoing messages
     * from the FIFO and writing them to a socket.
     *
     * As the data gets sent, the objects received from the outgoing buffer
     * are deleted.
     *
     * @ingroup net
     */
    class SenderThread : public Thread
    {
    public:
        typedef Message PacketType;
        typedef WaitableFIFO<PacketType> OutgoingBuffer;
        
    public:
        /**
         * The thread does not get ownership of the socket nor the buffer.
         */
        SenderThread(Socket& socket, OutgoingBuffer& buffer);
        ~SenderThread();

        /// Retrieve messages, write them to the owner's socket.
        void run();
    
    private:
        Socket& _socket;
        OutgoingBuffer& _buffer;
    };
}

#endif /* LIBDENG2_SENDERTHREAD_H */
