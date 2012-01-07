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
#include "writer.h"

#ifdef DENG_WRITER_TYPECHECK
#  define Writer_TypeCheck(w, code)  w->data[w->pos++] = code;
#else
#  define Writer_TypeCheck(w, code)
#endif

struct writer_s
{
    byte* data;             // The data buffer.
    size_t size;            // Size of the data buffer.
    size_t pos;             // Current position in the buffer.
    boolean isDynamic;         // The buffer will be reallocated when needed.
    size_t maxDynamicSize;  // Zero for unlimited.
};

static boolean Writer_Check(const Writer* writer, size_t len)
{
#ifdef DENG_WRITER_TYPECHECK
    // One extra byte for the check code.
    if(len) len++;
#endif

    if(!writer || !writer->data)
    {
        Con_Message("Writer_Check: Invalid Writer!\n");
        return false;
    }
    if((int)writer->pos > (int)writer->size - (int)len)
    {
        // Dynamic buffers will expand.
        if(writer->isDynamic && len)
        {
            Writer* modWriter = (Writer*) writer;
            while((int)modWriter->size < (int)writer->pos + (int)len)
            {
                modWriter->size *= 2;
            }
            if(writer->maxDynamicSize)
            {
                modWriter->size = MIN_OF(writer->maxDynamicSize, writer->size);
            }
            modWriter->data = M_Realloc(writer->data, writer->size);

            // OK now?
            if((int)writer->pos <= (int)writer->size - (int)len)
                return true;
        }
        Con_Error("Writer_Check: Position %lu[+%lu] out of bounds, size=%lu, dynamic=%i.\n",
                  (unsigned long) writer->pos,
                  (unsigned long) len,
                  (unsigned long) writer->size,
                  writer->isDynamic);
    }
    return true;
}

Writer* Writer_New(void)
{
    Writer* w = M_Calloc(sizeof(Writer));
    w->size = NETBUFFER_ACTUALSIZE;
    w->data = netBuffer.msg.data;
    return w;
}

Writer* Writer_NewWithBuffer(byte* buffer, size_t maxLen)
{
    Writer* w = M_Calloc(sizeof(Writer));
    w->size = maxLen;
    w->data = buffer;
    return w;
}

Writer* Writer_NewWithDynamicBuffer(size_t maxLen)
{
    Writer* w = M_Calloc(sizeof(Writer));
    w->isDynamic = true;
    w->maxDynamicSize = maxLen;
    w->size = 256;
    w->data = M_Calloc(w->size);
    return w;
}

void Writer_Delete(Writer* writer)
{
    if(writer->isDynamic)
    {
        // The buffer was allocated by us.
        M_Free(writer->data);
    }
    M_Free(writer);
}

size_t Writer_Size(const Writer* writer)
{
    if(!writer) return 0;
    return writer->pos;
}

size_t Writer_TotalBufferSize(const Writer* writer)
{
    if(!writer) return 0;
    return writer->size;
}

size_t Writer_BytesLeft(const Writer* writer)
{
    return Writer_TotalBufferSize(writer) - Writer_Size(writer);
}

const byte* Writer_Data(const Writer* writer)
{
    if(Writer_Check(writer, 0))
    {
        return writer->data;
    }
    return 0;
}

void Writer_SetPos(Writer* writer, size_t newPos)
{
    if(!writer) return;
    writer->pos = newPos;
    Writer_Check(writer, 0);
}

void Writer_WriteChar(Writer* writer, char v)
{
    if(!Writer_Check(writer, 1)) return;
    Writer_TypeCheck(writer, WTCC_CHAR);
    ((int8_t*)writer->data)[writer->pos++] = v;
}

void Writer_WriteByte(Writer* writer, byte v)
{
    if(!Writer_Check(writer, 1)) return;
    Writer_TypeCheck(writer, WTCC_BYTE);
    writer->data[writer->pos++] = v;
}

void Writer_WriteInt16(Writer* writer, int16_t v)
{
    if(Writer_Check(writer, 2))
    {
        Writer_TypeCheck(writer, WTCC_INT16);
        *(int16_t*) (writer->data + writer->pos) = SHORT(v);
        writer->pos += 2;
    }
}

void Writer_WriteUInt16(Writer* writer, uint16_t v)
{
    if(Writer_Check(writer, 2))
    {
        Writer_TypeCheck(writer, WTCC_UINT16);
        *(uint16_t*) (writer->data + writer->pos) = USHORT(v);
        writer->pos += 2;
    }
}

void Writer_WriteInt32(Writer* writer, int32_t v)
{
    if(Writer_Check(writer, 4))
    {
        Writer_TypeCheck(writer, WTCC_INT32);
        *(int32_t*) (writer->data + writer->pos) = LONG(v);
        writer->pos += 4;
    }
}

void Writer_WriteUInt32(Writer* writer, uint32_t v)
{
    if(Writer_Check(writer, 4))
    {
        Writer_TypeCheck(writer, WTCC_UINT32);
        *(uint32_t*) (writer->data + writer->pos) = ULONG(v);
        writer->pos += 4;
    }
}

void Writer_WriteFloat(Writer* writer, float v)
{
    if(Writer_Check(writer, 4))
    {
        Writer_TypeCheck(writer, WTCC_FLOAT);
        *(float*) (writer->data + writer->pos) = FLOAT(v);
        writer->pos += 4;
    }
}

void Writer_Write(Writer* writer, const void* buffer, size_t len)
{
    if(!len) return;

    if(Writer_Check(writer, len))
    {
        Writer_TypeCheck(writer, WTCC_BLOCK);
        memcpy(writer->data + writer->pos, buffer, len);
        writer->pos += len;
    }
}

void Writer_WritePackedUInt16(Writer* writer, uint16_t v)
{
    if(v & 0x8000)
    {
        Con_Error("Writer_WritePackedUInt16: Cannot write %i (%x).\n", v, v);
    }

    // Can the number be represented with 7 bits?
    if(v < 0x80)
    {
        Writer_WriteByte(writer, v);
    }
    else
    {
        Writer_WriteByte(writer, 0x80 | (v & 0x7f));
        Writer_WriteByte(writer, v >> 7);  // Highest bit is lost.
    }
}

void Writer_WritePackedUInt32(Writer* writer, uint32_t l)
{
    while(l >= 0x80)
    {
        // Write the lowest 7 bits, and set the high bit to indicate that
        // at least one more byte will follow.
        Writer_WriteByte(writer, 0x80 | (l & 0x7f));

        l >>= 7;
    }
    // Write the last byte, with the high bit clear.
    Writer_WriteByte(writer, l);
}
