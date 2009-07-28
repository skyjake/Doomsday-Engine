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

#include "de/ReceiverThread"
#include "de/Socket"

using namespace de;

ReceiverThread::ReceiverThread(Socket& socket, IncomingBuffer& buffer) 
    : Thread(), socket_(socket), buffer_(buffer)
{}

ReceiverThread::~ReceiverThread()
{}

void ReceiverThread::run()
{
    try
    {
        while(!shouldStopNow())
        {
            buffer_.put(socket_.receive());
        }
    }
    catch(const Socket::DisconnectedError&)
    {
        // No need to react. When the socket is closed, this is the exception
        // that is thrown.
    }
    catch(const Error& err)
    {
        std::cerr << "Fatal exception in ReceiverThread::run(): " << err.what() << "\n";
    }
}
