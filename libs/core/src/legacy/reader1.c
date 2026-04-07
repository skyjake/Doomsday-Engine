/**
 * @file src/reader1.c
 * Deserializer for reading values and data from a byte array.
 *
 * @authors Copyright &copy; 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de/legacy/reader.h"
#include "de/legacy/memory.h"
#include <de/c_wrapper.h>

#ifdef DE_WRITER_TYPECHECK
#  include "de/writer.h"
#  define Reader_TypeCheck(r, code)   if (r->data[r->pos++] != code) DE_ASSERT(!code);
#else
#  define Reader_TypeCheck(r, code)
#endif

#include <string.h>

#define Reader_16(reader)        ( (uint16_t) (Reader_8(reader,  0) | Reader_8(reader,  8)) )
#define Reader_32(reader)        ( Reader_8(reader,  0) | Reader_8(reader,  8) | \
                                   Reader_8(reader, 16) | Reader_8(reader, 24) )

typedef struct readerfuncs_s {
    Reader_Callback_ReadInt8  readInt8;
    Reader_Callback_ReadInt16 readInt16;
    Reader_Callback_ReadInt32 readInt32;
    Reader_Callback_ReadFloat readFloat;
    Reader_Callback_ReadData  readData;
} readerfuncs_t;

struct reader_s
{
    byte const *data;       // The data buffer.
    size_t size;            // Size of the data buffer.
    size_t pos;             // Current position in the buffer.

    dd_bool useCustomFuncs;
    readerfuncs_t func;
};

static uint32_t Reader_8(Reader1 *reader, int shift)
{
    return reader->data[reader->pos++] << shift;
}

static dd_bool Reader_Check(Reader1 const *reader, size_t len)
{
#ifdef DE_WRITER_TYPECHECK
    // One byte for the code.
    if (len) len++;
#endif

    DE_ASSERT(reader);
    DE_ASSERT(reader->data || reader->useCustomFuncs);

    if (!reader || (!reader->data && !reader->useCustomFuncs))
        return false;

    if (reader->useCustomFuncs)
    {
        // Not our responsibility.
        return true;
    }
    if (reader->pos > reader->size - len)
    {
        App_Log(DE2_LOG_ERROR,
            "Reader_Check: Position %lu[+%lu] out of bounds, size=%lu.",
                (unsigned long) reader->pos,
                (unsigned long) len,
                (unsigned long) reader->size);
        App_FatalError("Reader1 bounds check failed.");
    }
    return true;
}

Reader1 *Reader_NewWithBuffer(byte const *buffer, size_t len)
{
    Reader1 *rd = M_Calloc(sizeof(Reader1));
    rd->size = len;
    rd->data = buffer;
    return rd;
}

Reader1 *Reader_NewWithCallbacks(Reader_Callback_ReadInt8  readInt8,
                                 Reader_Callback_ReadInt16 readInt16,
                                 Reader_Callback_ReadInt32 readInt32,
                                 Reader_Callback_ReadFloat readFloat,
                                 Reader_Callback_ReadData  readData)
{
    Reader1 *rd = M_Calloc(sizeof(Reader1));
    rd->useCustomFuncs = true;
    rd->func.readInt8 = readInt8;
    rd->func.readInt16 = readInt16;
    rd->func.readInt32 = readInt32;
    rd->func.readFloat = readFloat;
    rd->func.readData = readData;
    return rd;
}

void Reader_Delete(Reader1 *reader)
{
    M_Free(reader);
}

size_t Reader_Pos(Reader1 const *reader)
{
    if (!reader) return 0;
    return reader->pos;
}

size_t Reader_Size(Reader1 const *reader)
{
    if (!reader) return 0;
    return reader->size;
}

void Reader_SetPos(Reader1 *reader, size_t newPos)
{
    if (!reader) return;
    if (reader->useCustomFuncs) return;
    reader->pos = newPos;
    Reader_Check(reader, 0);
}

dd_bool Reader_AtEnd(Reader1 const *reader)
{
    Reader_Check(reader, 0);
    if (reader->useCustomFuncs) return false;
    return reader->pos == reader->size;
}

int8_t Reader_ReadChar(Reader1 *reader)
{
    int8_t result = 0;
    if (Reader_Check(reader, 1))
    {
        if (!reader->useCustomFuncs)
        {
            Reader_TypeCheck(reader, WTCC_CHAR);
            result = ((int8_t *)reader->data)[reader->pos++];
        }
        else
        {
            assert(reader->func.readInt8);
            result = reader->func.readInt8(reader);
        }
    }
    return result;
}

byte Reader_ReadByte(Reader1 *reader)
{
    byte result = 0;
    if (Reader_Check(reader, 1))
    {
        if (!reader->useCustomFuncs)
        {
            Reader_TypeCheck(reader, WTCC_BYTE);
            result = reader->data[reader->pos++];
        }
        else
        {
            assert(reader->func.readInt8);
            result = reader->func.readInt8(reader);
        }
    }
    return result;
}

int16_t Reader_ReadInt16(Reader1 *reader)
{
    int16_t result = 0;
    if (Reader_Check(reader, 2))
    {
        if (!reader->useCustomFuncs)
        {
            Reader_TypeCheck(reader, WTCC_INT16);
            result = LittleEndianByteOrder_ToNativeInt16(Reader_16(reader));
        }
        else
        {
            assert(reader->func.readInt16);
            result = reader->func.readInt16(reader);
        }
    }
    return result;
}

uint16_t Reader_ReadUInt16(Reader1 *reader)
{
    uint16_t result = 0;
    if (Reader_Check(reader, 2))
    {
        if (!reader->useCustomFuncs)
        {
            Reader_TypeCheck(reader, WTCC_UINT16);
            result = LittleEndianByteOrder_ToNativeUInt16(Reader_16(reader));
        }
        else
        {
            assert(reader->func.readInt16);
            result = reader->func.readInt16(reader);
        }
    }
    return result;
}

int32_t Reader_ReadInt32(Reader1 *reader)
{
    int32_t result = 0;
    if (Reader_Check(reader, 4))
    {
        if (!reader->useCustomFuncs)
        {
            Reader_TypeCheck(reader, WTCC_INT32);
            result = LittleEndianByteOrder_ToNativeInt32(Reader_32(reader));
        }
        else
        {
            assert(reader->func.readInt32);
            result = reader->func.readInt32(reader);
        }
    }
    return result;
}

uint32_t Reader_ReadUInt32(Reader1 *reader)
{
    uint32_t result = 0;
    if (Reader_Check(reader, 4))
    {
        if (!reader->useCustomFuncs)
        {
            Reader_TypeCheck(reader, WTCC_UINT32);
            result = LittleEndianByteOrder_ToNativeUInt32(Reader_32(reader));
        }
        else
        {
            assert(reader->func.readInt32);
            result = reader->func.readInt32(reader);
        }
    }
    return result;
}

float Reader_ReadFloat(Reader1 *reader)
{
    float result = 0;
    if (Reader_Check(reader, 4))
    {
        if (!reader->useCustomFuncs)
        {
            float foreign;
            Reader_TypeCheck(reader, WTCC_FLOAT);
            memcpy(&foreign, reader->data + reader->pos, 4);
            reader->pos += 4;
            result = LittleEndianByteOrder_ToNativeFloat(foreign);
        }
        else
        {
            assert(reader->func.readFloat);
            result = reader->func.readFloat(reader);
        }
    }
    return result;
}

void Reader_Read(Reader1 *reader, void *buffer, size_t len)
{
    if (!len || !buffer) return;

    if (Reader_Check(reader, len))
    {
        if (!reader->useCustomFuncs)
        {
            Reader_TypeCheck(reader, WTCC_BLOCK);
            memcpy(buffer, reader->data + reader->pos, len);
            reader->pos += len;
        }
        else
        {
            assert(reader->func.readData);
            reader->func.readData(reader, buffer, len);
        }
    }
}

uint16_t Reader_ReadPackedUInt16(Reader1 *reader)
{
    ushort pack = Reader_ReadByte(reader);
    if (pack & 0x80)
    {
        pack &= ~0x80;
        pack |= Reader_ReadByte(reader) << 7;
    }
    return pack;
}

uint32_t Reader_ReadPackedUInt32(Reader1 *reader)
{
    byte pack = 0;
    int pos = 0;
    uint value = 0;

    do {
        if (!Reader_Check(reader, 1)) return 0;
        pack = Reader_ReadByte(reader);
        value |= ((pack & 0x7f) << pos);
        pos += 7;
    } while (pack & 0x80);

    return value;
}

void M_ReadBits(uint numBits, const uint8_t** src, uint8_t* cb, uint8_t* out)
{
    int offset = 0, unread = numBits;

    DE_ASSERT(src && cb && out);

    // Read full bytes.
    if (unread >= 8)
    {
        do
        {
            out[offset++] = **src, (*src)++;
        }
        while ((unread -= 8) >= 8);
    }

    if (unread != 0)
    {
        // Read remaining bits.
        uint8_t fb = 8 - unread;
        if ((*cb) == 0) (*cb) = 8;

        do
        {
            (*cb)--;
            out[offset] <<= 1;
            out[offset] |= ((**src >> (*cb)) & 0x01);
        }
        while (--unread > 0);

        out[offset] <<= fb;
        if ((*cb) == 0) (*src)++;
    }
}
