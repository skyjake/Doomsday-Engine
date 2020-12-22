/** @file blockpacket.h  Packet that contains a Block.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_BLOCKPACKET_H
#define LIBCORE_BLOCKPACKET_H

#include "de/packet.h"
#include "de/block.h"

namespace de {

/**
 * Packet that contains a Block.
 *
 * @ingroup protocol
 */
class DE_PUBLIC BlockPacket : public Packet, private Block
{
public:
    BlockPacket();

    BlockPacket(const Block &block);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    Block &block();
    const Block &block() const;

public:
    /// Constructor for a Protocol.
    static Packet *fromBlock(const Block &block);
};

} // namespace de

#endif // LIBCORE_BLOCKPACKET_H
