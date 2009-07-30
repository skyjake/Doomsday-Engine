/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_RECORDPACKET_H
#define LIBDENG2_RECORDPACKET_H

#include "../Packet"
#include "../Record"
#include "../String"

namespace de
{
    class Block;
    class Variable;

    /**
     * A packet that contains a Record. A label string can be attached.
     *
     * @ingroup protocol
     */
    class LIBDENG2_API RecordPacket : public Packet
    {
    public:
        RecordPacket(const String& label = "");
        virtual ~RecordPacket();

        /// Returns the caption of the packet.
        const String& label() const { return label_; }
        
        /// Sets the command of the packet.
        void setLabel(const String& s) { label_ = s; }

        /// Returns the arguments of the packet (non-modifiable).
        const Record& record() const { return *record_; }
        
        /// Returns the arguments of the packet.
        Record& record() { return *record_; }

        /**
         * Takes ownership of a previously created record.
         *
         * @param record  New record for the packet.
         */
        void take(Record* record);
        
        /**
         * Detaches the Record instance from the packet. The packet is
         * left with an empty record.
         *
         * @return  Caller gets ownership of the returned record.
         */
        Record* give();

        /**
         * Returns a variable in the packet's record.
         *
         * @param variableName  Name of variable.
         *
         * @return  Variable.
         */
        const Variable& operator [] (const String& variableName) const;

        /**
         * Convenience method that returns a variable's value as text from
         * the packet's record.
         *
         * @param variableName  Name of variable.
         *
         * @return  Variable's value as text.
         */
        String valueAsText(const String& variableName) const;

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);

    public:
        /// Constructor for the Protocol.
        static Packet* fromBlock(const Block& block);

    private:
        String label_;

        /// The record.
        Record* record_;
    };
}

#endif /* LIBDENG2_RECORDPACKET_H */
