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
 
#ifndef LIBDENG2_COMMANDPACKET_H
#define LIBDENG2_COMMANDPACKET_H

#include "../RecordPacket"
#include "../String"

#include <list>

namespace de
{
    /**
     * Command packet. Used for controlling a libdeng2 based application.
     *
     * @ingroup protocol
     */
    class LIBDENG2_API CommandPacket : public RecordPacket
    {    
    public:
        CommandPacket(const String& cmd = "");
        ~CommandPacket();
        
        /// Returns the command of the packet.
        const String& command() const { return label(); }
        
        /// Sets the command of the packet.
        void setCommand(const String& c) { setLabel(c); }

        /// Returns the arguments of the packet (non-modifiable).
        const Record& arguments() const { return record(); }
        
        /// Returns the arguments of the packet.
        Record& arguments() { return record(); }

    public:
        /// Constructor for the Protocol.
        static Packet* fromBlock(const Block& block);
    };
} 

#endif /* LIBDENG2_COMMANDPACKET_H */
 