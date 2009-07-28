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

#ifndef LIBDENG2_RECEIVERTHREAD_H
#define LIBDENG2_RECEIVERTHREAD_H

#include "../Thread"
#include "../FIFO"
#include "../Message"

namespace de
{
    class Socket;

    /**
     * The receiver thread is responsible for reading the owner's socket
     * for incoming data and storing the received packets to the incoming
     * buffer.
     *
     * @ingroup net
     */
    class ReceiverThread : public Thread
    {
    public:
        typedef Message PacketType;
        typedef FIFO<PacketType> IncomingBuffer;        
        
    public:
        ReceiverThread(Socket& socket, IncomingBuffer& buffer);
        ~ReceiverThread();

        /// Read from owner's socket, store packets into the incoming
        /// buffer.
        void run();
        
    private:
        Socket& socket_;
        IncomingBuffer& buffer_;
    };
}

#endif /* LIBDENG2_RECEIVERTHREAD_H */
