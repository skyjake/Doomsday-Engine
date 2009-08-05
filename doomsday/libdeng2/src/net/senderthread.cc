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

#include "de/SenderThread"
#include "de/Socket"
#include "de/Time"
#include "de/Message"

using namespace de;

SenderThread::SenderThread(Socket& socket, OutgoingBuffer& buffer) 
    : Thread(), socket_(socket), buffer_(buffer)
{}

SenderThread::~SenderThread()
{}

void SenderThread::run()
{
    while(!shouldStopNow())
    {
        try
        {
            // Wait for new outgoing messages.
            buffer_.wait(10);

            // There is a new outgoing message. We'll keep it in the FIFO
            // until it has been sent.
            const PacketType* data = buffer_.peek();
            if(data)
            {
                // Write this to the socket.
                socket_.mode.set(Socket::CHANNEL_1_BIT, data->channel() == 1);
                socket_ << *data;

                // The packet can be discarded.
                delete buffer_.get();
            }
        }
        catch(const Waitable::TimeOutError&)
        {
            // No need to react, let's try again.
            //std::cerr << "SenderThread: Outgoing buffer wait timeout.\n";
        }
        catch(const Waitable::WaitError&)
        {
            std::cerr << "SenderThread: Socket closed.\n";
            stop();
        }
    }           
}
