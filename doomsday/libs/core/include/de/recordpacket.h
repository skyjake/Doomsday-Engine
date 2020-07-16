/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBCORE_RECORDPACKET_H
#define LIBCORE_RECORDPACKET_H

#include "de/identifiedpacket.h"
#include "de/record.h"
#include "de/string.h"

namespace de {

class Block;
class Variable;

/**
 * An identified packet that contains a Record. The record itself can be
 * identified by a name.
 *
 * Record packets can be used as a generic packet format for multiple purposes.
 * It is easy to construct record packets in scripts.
 *
 * @ingroup protocol
 */
class DE_PUBLIC RecordPacket : public IdentifiedPacket
{
public:
    RecordPacket(const String &name = String(), Id id = 0);
    virtual ~RecordPacket();

    /// Returns the caption of the packet.
    const String &name() const { return _name; }

    /// Sets the command of the packet.
    void setName(const String &n) { _name = n; }

    /// Returns the arguments of the packet (non-modifiable).
    const Record& record() const { return *_record; }

    /// Returns the arguments of the packet.
    Record& record() { return *_record; }

    /**
     * Takes ownership of a previously created record.
     *
     * @param record  New record for the packet.
     */
    void take(Record *record);

    /**
     * Detaches the Record instance from the packet. The packet is
     * left with an empty record.
     *
     * @return  Caller gets ownership of the returned record.
     */
    Record *give();

    /**
     * Returns a variable in the packet's record.
     *
     * @param variableName  Name of variable.
     *
     * @return  Variable.
     */
    const Variable &operator [] (const String &variableName) const;

    /**
     * Convenience method that returns a variable's value as text from
     * the packet's record.
     *
     * @param variableName  Name of variable.
     *
     * @return  Variable's value as text.
     */
    String valueAsText(const String &variableName) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    /// Constructor for the Protocol.
    static Packet *fromBlock(const Block &block);

private:
    String _name;

    /// The record.
    Record *_record;
};

} // namespace de

#endif /* LIBCORE_RECORDPACKET_H */
