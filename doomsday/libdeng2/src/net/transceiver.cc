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

#include "de/Transceiver"
#include "de/App"
#include "de/Block"
#include "de/Writer"
#include "de/Reader"
#include "de/Message"
#include "de/Protocol"
#include "de/Packet"

using namespace de;

Transceiver::~Transceiver()
{}

Transceiver& Transceiver::operator << (const IByteArray& data)
{
    send(data);
    return *this;
}

Transceiver& Transceiver::operator << (const Packet& packet)
{
    sendPacket(packet);
    return *this;
}

void Transceiver::sendPacket(const Packet& packet)
{
    Block data;
    Writer(data) << packet;
    send(data);
}

Packet* Transceiver::receivePacket(const Time::Delta& timeOut)
{
    /*
    Time startedAt;
    while(startedAt.since() <= timeOut)
    {
        std::auto_ptr<Message> message(receive());
        if(!message.get())
        {
            // Wait for a bit.
            Time::sleep(.05);
            continue;
        }
        Packet* packet = App::protocol().interpret(*message.get());
        if(!packet)
        {
            /// @throw UnexpectedError Something that was not recognized as a Packet was received.
            throw UnexpectedError("Transceiver::receivePacket", 
                "Expected a packet, but something else was received");
        }
        return packet;
    }
    */
    /// @throw TimeOutError Timeout expired before anything was received.
    throw TimeOutError("Transceiver::receivePacket", "NOT IMPLEMENTED!");
    //throw TimeOutError("Transceiver::receivePacket", "Timeout expired before anything was received");
}
