/** @file blockpacket.h  Packet that contains a Block.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBDENG2_BLOCKPACKET_H
#define LIBDENG2_BLOCKPACKET_H

#include "../Packet"
#include "../Block"

namespace de {

/**
 * Packet that contains a Block.
 *
 * @ingroup protocol
 */
class DENG2_PUBLIC BlockPacket : public Packet, public Block
{
public:
    BlockPacket();

    BlockPacket(Block const &block);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    /// Constructor for a Protocol.
    static Packet *fromBlock(Block const &block);
};

} // namespace de

#endif // LIBDENG2_BLOCKPACKET_H
