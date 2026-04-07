/** @file blockpacket.cpp  Packet that contains a block of data.
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

#include "de/blockpacket.h"
#include "de/writer.h"
#include "de/reader.h"

namespace de {

static const Packet::Type BLOCK_PACKET_TYPE = Packet::typeFromString("BLCK");

BlockPacket::BlockPacket() : Packet(BLOCK_PACKET_TYPE)
{}

BlockPacket::BlockPacket(const Block &block)
    : Packet(BLOCK_PACKET_TYPE), Block(block)
{}

void BlockPacket::operator >> (Writer &to) const
{
    Packet::operator >> (to);
    to << *static_cast<const Block *>(this);
}

void BlockPacket::operator << (Reader &from)
{
    Packet::operator << (from);
    from >> *static_cast<Block *>(this);
}

Block &BlockPacket::block()
{
    return *this;
}

const Block &BlockPacket::block() const
{
    return *this;
}

Packet *BlockPacket::fromBlock(const Block &block)
{
    // Attempts to deserialize from the data in block.
    return constructFromBlock<BlockPacket>(block, BLOCK_PACKET_TYPE);
}

} // namespace de
