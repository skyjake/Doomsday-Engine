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

#ifndef LIBDENG2_TRANSCEIVER_H
#define LIBDENG2_TRANSCEIVER_H

#include "../Time"

namespace de
{
    class Message;
    class IByteArray;
    class Packet;
    
    /**
     * Abstract base class for objects that can send and receive data.
     *
     * @ingroup net
     */
    class LIBDENG2_API Transceiver
    {
    public:
        /// A packet of specific type was expected but something else was received 
        /// instead. @ingroup errors
        DEFINE_ERROR(UnexpectedError);
        
        /// Specified timeout elapsed. @ingroup errors
        DEFINE_ERROR(TimeOutError);
        
    public:
        virtual ~Transceiver() {}

        /**
         * Sends an array of data.
         *
         * @param data  Data to send.
         */
        virtual void send(const IByteArray& data) = 0;
 
        /**
         * Receives an array of data. 
         *
         * @return  Received data array, or @c NULL if nothing has been received.
         *          Caller gets ownership of the returned object.
         */
        virtual Message* receive() = 0;
 
        /**
         * Sends a packet. The packet is first serialized and then sent.
         *
         * @param packet  Packet.
         */
        virtual void sendPacket(const Packet& packet);
        
        /**
         * Receives a packet. Will not return until the packet has been received,
         * or the timeout has expired.
         *
         * @param timeOut  Maximum period of time to wait.
         *
         * @return  The received packet. Never returns NULL. Caller gets ownership
          *         of the packet.
         */
        virtual Packet* receivePacket(const Time::Delta& timeOut = 10);

        /**
         * Receives a packet of specific type. Will not return until the packet has 
         * been received, or the timeout has expired.
         *
         * @param timeOut  Maximum period of time to wait.
         *
         * @return  The received packet. Never returns NULL. Caller gets ownership
         *          of the packet.
         */
        template <typename Type>
        Type* receivePacket(const Time::Delta& timeOut = 10) {
            Type* packet = dynamic_cast<Type*>(receivePacket(timeOut));
            if(!packet)
            {
                /// @throw UnexpectedError A packet was received but it's type was not Type.
                throw UnexpectedError("Transceiver::receive", "Received wrong type of packet");
            }
            return packet;
        }
 
        /**
         * Sends an array of data.
         *
         * @param data  Data to send.
         */
        virtual Transceiver& operator << (const IByteArray& data);
        
        /**
         * Sends a packet. The packet is first serialized and then sent.
         *
         * @param packet  Packet.
         */
        virtual Transceiver& operator << (const Packet& packet);
    };
}

#endif /* LIBDENG2_TRANSCEIVER_H */
