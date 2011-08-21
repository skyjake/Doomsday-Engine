/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * net_msg.c: Network Messaging
 *
 * Buffer overflow checks *ARE NOT* made ifndef _DEBUG
 * The caller must know what it's doing.
 * The data is stored using little-endian ordering.
 *
 * Note that negative values are not good for the packed write/read routines,
 * as they always have the high bits set.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_network.h"
#include "de_console.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Msg_Begin(int type)
{
    netBuffer.cursor = netBuffer.msg.data;
    netBuffer.length = 0;
    netBuffer.msg.type = type;
}

void Msg_WriteByte(byte b)
{
    *netBuffer.cursor++ = b;
}

void Msg_WriteShort(short w)
{
    *(short *) netBuffer.cursor = SHORT(w);
    netBuffer.cursor += 2;
}

/**
 * Only 15 bits can be used for the number because the high bit of the
 * lower byte is used to determine whether the upper byte follows or not.
 */
void Msg_WritePackedShort(short w)
{
    if(w < 0)
    {
        Con_Error("Msg_WritePackedShort: Cannot write %i.\n", w);
    }

    // Can the number be represented with 7 bits?
    if(w < 0x80)
    {
        Msg_WriteByte(w);
    }
    else
    {
        Msg_WriteByte(0x80 | (w & 0x7f));
        Msg_WriteByte(w >> 7);  // Highest bit is lost.
    }
}

void Msg_WriteLong(int l)
{
    *(int *) netBuffer.cursor = LONG(l);
    netBuffer.cursor += 4;
}

void Msg_WritePackedLong(uint l)
{
    while(l >= 0x80)
    {
        // Write the lowest 7 bits, and set the high bit to indicate that
        // at least one more byte will follow.
        Msg_WriteByte(0x80 | (l & 0x7f));

        l >>= 7;
    }
    // Write the last byte, with the high bit clear.
    Msg_WriteByte(l);
}

void Msg_Write(const void *src, size_t len)
{
    memcpy(netBuffer.cursor, src, len);
    netBuffer.cursor += len;
}

byte Msg_ReadByte(void)
{
#ifdef _DEBUG
    if(Msg_Offset() >= netBuffer.length)
        Con_Error("Msg_ReadByte: Packet read overflow!\n");
#endif
    return *netBuffer.cursor++;
}

short Msg_ReadShort(void)
{
#ifdef _DEBUG
    if(Msg_Offset() >= netBuffer.length)
        Con_Error("Msg_ReadShort: Packet read overflow!\n");
#endif
    netBuffer.cursor += 2;
    return SHORT( *(short *) (netBuffer.cursor - 2) );
}

/**
 * Only 15 bits can be used for the number because the high bit of the
 * lower byte is used to determine whether the upper byte follows or not.
 */
short Msg_ReadPackedShort(void)
{
    ushort          pack = *netBuffer.cursor++;

    if(pack & 0x80)
    {
        pack &= ~0x80;
        pack |= (*netBuffer.cursor++) << 7;
    }
    return pack;
}

int Msg_ReadLong(void)
{
#ifdef _DEBUG
    if(Msg_Offset() >= netBuffer.length)
        Con_Error("Msg_ReadLong: Packet read overflow!\n");
#endif
    netBuffer.cursor += 4;
    return LONG( *(int *) (netBuffer.cursor - 4) );
}

uint Msg_ReadPackedLong(void)
{
    byte            pack = 0;
    int             pos = 0;
    uint            value = 0;

    do
    {
#ifdef _DEBUG
        if(Msg_Offset() >= netBuffer.length)
            Con_Error("Msg_ReadPackedLong: Packet read overflow!\n");
#endif

        pack = *netBuffer.cursor++;
        value |= ((pack & 0x7f) << pos);
        pos += 7;
    } while(pack & 0x80);

    return value;
}

void Msg_Read(void *dest, size_t len)
{
#ifdef _DEBUG
    if(Msg_Offset() >= netBuffer.length)
        Con_Error("Msg_Read: Packet read overflow!\n");
#endif
    memcpy(dest, netBuffer.cursor, len);
    netBuffer.cursor += len;
}

size_t Msg_Offset(void)
{
    return netBuffer.cursor - netBuffer.msg.data;
}

void Msg_SetOffset(size_t offset)
{
    netBuffer.cursor = netBuffer.msg.data + offset;
}

size_t Msg_MemoryLeft(void)
{
    return NETBUFFER_MAXDATA - (netBuffer.cursor - netBuffer.msg.data);
}

boolean Msg_End(void)
{
    if((size_t) (netBuffer.cursor - netBuffer.msg.data) >= netBuffer.length)
        return true;

    return false;
}
