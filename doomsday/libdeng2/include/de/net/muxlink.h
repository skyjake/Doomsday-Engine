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

#ifndef LIBDENG2_MUXLINK_H
#define LIBDENG2_MUXLINK_H

#include "../deng.h"
#include "../Link"

namespace de
{
    /**
     * Multiplexes one Link so that multiple isolated communication channels
     * can operate over it.
     *
     * @ingroup net
     */
    class LIBDENG2_API MuxLink
    {
    public:
        enum ChannelId {
            BASE = 0,
            UPDATES = 1,
            NUM_CHANNELS
        };
        
        /**
         * The channel is a virtual transceiver working on top of a Link.
         *
         * @ingroup net
         */ 
        class LIBDENG2_API Channel : public Transceiver {
        public:
            Channel(MuxLink& mux, duint channel) : mux_(mux), channel_(channel) {}

            // Implements Transceiver.
            void send(const IByteArray& data);
            Message* receive();

            /// Checks if the channel has incoming data.
            bool hasIncoming();

        private:
            MuxLink& mux_;
            duint channel_;
        };
        
    public:
        /**
         * Constructs a new multiplex link. A new socket is created for the link.
         *
         * @param address  Address to connect to.
         */
        MuxLink(const Address& address);
        
        /**
         * Constructs a new multiplex linx. 
         *
         * @param socket  Socket to use for network traffic. MuxLink
         *                creates its own Link for this socket.
         */
        MuxLink(Socket* socket);
        
        virtual ~MuxLink();

        /**
         * Returns the link over which multiplexing is done.
         */
        Link& link() { return *link_; }

        /**
         * Returns the address of the remote end of the link.
         */
        Address peerAddress() const;

        /**
         * Returns a transceiver that operates on a particular channel in the
         * multiplex link.
         *
         * @param channel  Channel to operate on.
         *
         * @return  Transceiver for the channel.
         */
        Channel channel(ChannelId channel) {
            return Channel(*this, channel);
        }
        
        /**
         * Returns the base channel.
         */
        Channel base() { return channel(BASE); }

        /**
         * Returns the updates channel.
         */
        Channel updates() { return channel(UPDATES); }

        /**
         * When MuxLink is used as a Transceiver, it defaults to the base channel.
         */
        operator Transceiver& () {
            return defaultChannel_;
        }

    protected:
        /**
         * Gets all received packets and puts them into the channels' incoming buffers.
         */
        void demux();
        
    private:
        /// The link over which multiplexing is done.
        Link* link_;
        
        /// Each channel has its own incoming buffer.
        Link::IncomingBuffer buffers_[NUM_CHANNELS];
        
        Channel defaultChannel_;
    };
};

#endif /* LIBDENG2_MUXLINK_H */
