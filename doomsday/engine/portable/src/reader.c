/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_misc.h"
#include "de_console.h"
#include "net_buf.h"
#include "reader.h"

struct reader_s
{
    const byte* data;       // The data buffer.
    size_t size;            // Size of the data buffer.
    size_t pos;             // Current position in the buffer.
};

static boolean Reader_Check(const Reader* reader, size_t len)
{
    if(!reader || !reader->data) return false;
    if(reader->pos > reader->size - len)
    {
        Con_Error("Reader_Check: Position %lu[+%lu] out of bounds, size=%lu.\n",
                  (unsigned long) reader->pos,
                  (unsigned long) len,
                  (unsigned long) reader->size);
    }
    return true;
}

Reader* Reader_New(void)
{
    Reader* rd = M_Calloc(sizeof(Reader));
    rd->size = netBuffer.length;
    rd->data = (const byte*) netBuffer.msg.data;
    return rd;
}

Reader* Reader_NewWithBuffer(const byte* buffer, size_t len)
{
    Reader* rd = M_Calloc(sizeof(Reader));
    rd->size = len;
    rd->data = buffer;
    return rd;
}

void Reader_Destruct(Reader* reader)
{
    M_Free(reader);
}

size_t Reader_Pos(const Reader* reader)
{
    if(!reader) return 0;
    return reader->pos;
}

size_t Reader_Size(const Reader* reader)
{
    if(!reader) return 0;
    return reader->size;
}

void Reader_SetPos(Reader* reader, size_t newPos)
{
    if(!reader) return;
    reader->pos = newPos;
    Reader_Check(reader, 0);
}

int8_t Reader_ReadChar(Reader* reader)
{
    if(!Reader_Check(reader, 1)) return 0;
    return ((int8_t*)reader->data)[reader->pos++];
}

byte Reader_ReadByte(Reader* reader)
{
    if(!Reader_Check(reader, 1)) return 0;
    return reader->data[reader->pos++];
}

int16_t Reader_ReadInt16(Reader* reader)
{
    int16_t result = 0;
    if(Reader_Check(reader, 2))
    {
        result = SHORT( *(int16_t*) &reader->data[reader->pos] );
        reader->pos += 2;
    }
    return result;
}

uint16_t Reader_ReadUInt16(Reader* reader)
{
    uint16_t result = 0;
    if(Reader_Check(reader, 2))
    {
        result = USHORT( *(uint16_t*) &reader->data[reader->pos] );
        reader->pos += 2;
    }
    return result;
}

int32_t Reader_ReadInt32(Reader* reader)
{
    int32_t result = 0;
    if(Reader_Check(reader, 4))
    {
        result = LONG( *(int32_t*) &reader->data[reader->pos] );
        reader->pos += 4;
    }
    return result;
}

uint32_t Reader_ReadUInt32(Reader* reader)
{
    uint32_t result = 0;
    if(Reader_Check(reader, 4))
    {
        result = ULONG( *(uint32_t*) &reader->data[reader->pos] );
        reader->pos += 4;
    }
    return result;
}

float Reader_ReadFloat(Reader* reader)
{
    uint32_t v = Reader_ReadUInt32(reader);
    return *(float*) &v;
}

void Reader_Read(Reader* reader, void* buffer, size_t len)
{
    if(Reader_Check(reader, len))
    {
        memcpy(buffer, reader->data + reader->pos, len);
        reader->pos += len;
    }
}

uint16_t Reader_ReadPackedUInt16(Reader* reader)
{
    ushort pack = Reader_ReadByte(reader);
    if(pack & 0x80)
    {
        pack &= ~0x80;
        pack |= Reader_ReadByte(reader) << 7;
    }
    return pack;
}

uint32_t Reader_ReadPackedUInt32(Reader* reader)
{
    byte pack = 0;
    int pos = 0;
    uint value = 0;

    while(Reader_Check(reader, 1))
    {
        pack = Reader_ReadByte(reader);
        value |= ((pack & 0x7f) << pos);
        pos += 7;
    } while(pack & 0x80);

    return value;
}
