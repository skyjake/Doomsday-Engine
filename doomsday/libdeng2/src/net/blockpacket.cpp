/** @file blockpacket.cpp  Packet that contains a block of data.
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

#include "de/BlockPacket"
#include "de/Writer"
#include "de/Reader"

namespace de {

static const char* BLOCK_PACKET_TYPE = "BLCK";

BlockPacket::BlockPacket() : Packet(BLOCK_PACKET_TYPE)
{}

BlockPacket::BlockPacket(Block const &block)
    : Packet(BLOCK_PACKET_TYPE), Block(block)
{}

void BlockPacket::operator >> (Writer &to) const
{
    Packet::operator >> (to);
    to << *static_cast<Block const *>(this);
}

void BlockPacket::operator << (Reader &from)
{
    Packet::operator << (from);
    from >> *static_cast<Block *>(this);
}

Packet *BlockPacket::fromBlock(Block const &block)
{
    // Attempts to deserialize from the data in block.
    return constructFromBlock<BlockPacket>(block, BLOCK_PACKET_TYPE);
}

} // namespace de
