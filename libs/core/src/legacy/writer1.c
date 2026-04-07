/**
 * @file de/writer1.c
 * Serializer for writing values and data into a byte array.
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

#include "de/legacy/writer.h"
#include "de/legacy/memory.h"
#include <string.h>
#include <de/c_wrapper.h>

#ifdef DE_WRITER_TYPECHECK
#  define Writer_TypeCheck(w, code)  w->data[w->pos++] = code;
#else
#  define Writer_TypeCheck(w, code)
#endif

typedef struct writerfuncs_s {
    Writer_Callback_WriteInt8  writeInt8;
    Writer_Callback_WriteInt16 writeInt16;
    Writer_Callback_WriteInt32 writeInt32;
    Writer_Callback_WriteFloat writeFloat;
    Writer_Callback_WriteData  writeData;
} writerfuncs_t;

struct writer_s
{
    byte *data;             // The data buffer.
    size_t size;            // Size of the data buffer.
    size_t pos;             // Current position in the buffer.
    dd_bool isDynamic;      // The buffer will be reallocated when needed.
    size_t maxDynamicSize;  // Zero for unlimited.

    dd_bool useCustomFuncs; // Validity checks are skipped (callbacks' responsibility).
    writerfuncs_t func;     // Callbacks for write operations.
};

static dd_bool Writer_Check(Writer1 const *writer, size_t len)
{
#ifdef DE_WRITER_TYPECHECK
    // One extra byte for the check code.
    if (len) len++;
#endif

    DE_ASSERT(writer);
    DE_ASSERT(writer->data || writer->useCustomFuncs);

    if (!writer || (!writer->data && !writer->useCustomFuncs))
        return false;

    if (writer->useCustomFuncs)
    {
        // Not our responsibility.
        return true;
    }
    if ((int)writer->pos > (int)writer->size - (int)len)
    {
        // Dynamic buffers will expand.
        if (writer->isDynamic && len)
        {
            Writer1 *modWriter = (Writer1 *) writer;
            while ((int)modWriter->size < (int)writer->pos + (int)len)
            {
                modWriter->size *= 2;
            }
            if (writer->maxDynamicSize)
            {
                modWriter->size = MIN_OF(writer->maxDynamicSize, writer->size);
            }
            modWriter->data = M_Realloc(writer->data, writer->size);

            // OK now?
            if ((int)writer->pos <= (int)writer->size - (int)len)
                return true;
        }
        App_Log(DE2_LOG_ERROR,
            "Writer_Check: Position %lu[+%lu] out of bounds, size=%lu, dynamic=%i.",
                (unsigned long) writer->pos,
                (unsigned long) len,
                (unsigned long) writer->size,
                writer->isDynamic);
        App_FatalError("Writer1 bounds check failed.");
    }
    return true;
}

static void Writer_WriteRaw(Writer1 *writer, void const *buffer, size_t len)
{
    memcpy(writer->data + writer->pos, buffer, len);
    writer->pos += len;
}

Writer1 *Writer_NewWithBuffer(byte *buffer, size_t maxLen)
{
    Writer1 *w = M_Calloc(sizeof(Writer1));
    w->size = maxLen;
    w->data = buffer;
    return w;
}

Writer1 *Writer_NewWithDynamicBuffer(size_t maxLen)
{
    Writer1 *w = M_Calloc(sizeof(Writer1));
    w->isDynamic = true;
    w->maxDynamicSize = maxLen;
    w->size = 256;
    w->data = M_Calloc(w->size);
    return w;
}

Writer1 *Writer_NewWithCallbacks(Writer_Callback_WriteInt8  writeInt8,
                                 Writer_Callback_WriteInt16 writeInt16,
                                 Writer_Callback_WriteInt32 writeInt32,
                                 Writer_Callback_WriteFloat writeFloat,
                                 Writer_Callback_WriteData  writeData)
{
    Writer1 *w = M_Calloc(sizeof(Writer1));
    w->useCustomFuncs = true;
    w->func.writeInt8 = writeInt8;
    w->func.writeInt16 = writeInt16;
    w->func.writeInt32 = writeInt32;
    w->func.writeFloat = writeFloat;
    w->func.writeData = writeData;
    return w;
}

void Writer_Delete(Writer1 *writer)
{
    if (!writer) return;
    if (writer->isDynamic)
    {
        // The buffer was allocated by us.
        M_Free(writer->data);
    }
    M_Free(writer);
}

size_t Writer_Size(Writer1 const *writer)
{
    if (!writer) return 0;
    return writer->pos;
}

size_t Writer_TotalBufferSize(Writer1 const *writer)
{
    if (!writer) return 0;
    return writer->size;
}

size_t Writer_BytesLeft(Writer1 const *writer)
{
    return Writer_TotalBufferSize(writer) - Writer_Size(writer);
}

byte const *Writer_Data(Writer1 const *writer)
{
    if (Writer_Check(writer, 0))
    {
        return writer->data;
    }
    return 0;
}

void Writer_SetPos(Writer1 *writer, size_t newPos)
{
    if (!writer || writer->useCustomFuncs) return;
    writer->pos = newPos;
    Writer_Check(writer, 0);
}

void Writer_WriteChar(Writer1 *writer, char v)
{
    if (!Writer_Check(writer, 1)) return;
    if (!writer->useCustomFuncs)
    {
        Writer_TypeCheck(writer, WTCC_CHAR);
        ((int8_t *)writer->data)[writer->pos++] = v;
    }
    else
    {
        assert(writer->func.writeInt8);
        writer->func.writeInt8(writer, v);
    }
}

void Writer_WriteByte(Writer1 *writer, byte v)
{
    if (!Writer_Check(writer, 1)) return;
    if (!writer->useCustomFuncs)
    {
        Writer_TypeCheck(writer, WTCC_BYTE);
        writer->data[writer->pos++] = v;
    }
    else
    {
        assert(writer->func.writeInt8);
        writer->func.writeInt8(writer, v);
    }
}

void Writer_WriteInt16(Writer1 *writer, int16_t v)
{
    if (Writer_Check(writer, 2))
    {
        if (!writer->useCustomFuncs)
        {
            int16_t foreign = LittleEndianByteOrder_ToForeignInt16(v);
            Writer_TypeCheck(writer, WTCC_INT16);
            Writer_WriteRaw(writer, &foreign, 2);
        }
        else
        {
            assert(writer->func.writeInt16);
            writer->func.writeInt16(writer, v);
        }
    }
}

void Writer_WriteUInt16(Writer1 *writer, uint16_t v)
{
    if (Writer_Check(writer, 2))
    {
        if (!writer->useCustomFuncs)
        {
            uint16_t foreign = LittleEndianByteOrder_ToForeignUInt16(v);
            Writer_TypeCheck(writer, WTCC_UINT16);
            Writer_WriteRaw(writer, &foreign, 2);
        }
        else
        {
            assert(writer->func.writeInt16);
            writer->func.writeInt16(writer, v);
        }
    }
}

void Writer_WriteInt32(Writer1 *writer, int32_t v)
{
    if (Writer_Check(writer, 4))
    {
        if (!writer->useCustomFuncs)
        {
            int32_t foreign = LittleEndianByteOrder_ToForeignInt32(v);
            Writer_TypeCheck(writer, WTCC_INT32);
            Writer_WriteRaw(writer, &foreign, 4);
        }
        else
        {
            assert(writer->func.writeInt32);
            writer->func.writeInt32(writer, v);
        }
    }
}

void Writer_WriteUInt32(Writer1 *writer, uint32_t v)
{
    if (Writer_Check(writer, 4))
    {
        if (!writer->useCustomFuncs)
        {
            uint32_t foreign = LittleEndianByteOrder_ToForeignUInt32(v);
            Writer_TypeCheck(writer, WTCC_UINT32);
            Writer_WriteRaw(writer, &foreign, 4);
        }
        else
        {
            assert(writer->func.writeInt32);
            writer->func.writeInt32(writer, v);
        }
    }
}

void Writer_WriteFloat(Writer1 *writer, float v)
{
    if (Writer_Check(writer, 4))
    {
        if (!writer->useCustomFuncs)
        {
            float foreign = LittleEndianByteOrder_ToForeignFloat(v);
            Writer_TypeCheck(writer, WTCC_FLOAT);
            Writer_WriteRaw(writer, &foreign, 4);
        }
        else
        {
            assert(writer->func.writeFloat);
            writer->func.writeFloat(writer, v);
        }
    }
}

void Writer_Write(Writer1 *writer, void const *buffer, size_t len)
{
    if (!len) return;

    if (Writer_Check(writer, len))
    {
        if (!writer->useCustomFuncs)
        {
            Writer_TypeCheck(writer, WTCC_BLOCK);
            Writer_WriteRaw(writer, buffer, len);
        }
        else
        {
            assert(writer->func.writeData);
            writer->func.writeData(writer, buffer, len);
        }
    }
}

void Writer_WritePackedUInt16(Writer1 *writer, uint16_t v)
{
    DE_ASSERT(!(v & 0x8000));
    if (v & 0x8000)
    {
        App_Log(DE2_LOG_ERROR,
            "Writer_WritePackedUInt16: Cannot write %i (%x).", v, v);
        return;
    }

    // Can the number be represented with 7 bits?
    if (v < 0x80)
    {
        Writer_WriteByte(writer, (dbyte) v);
    }
    else
    {
        Writer_WriteByte(writer, 0x80 | (v & 0x7f));
        Writer_WriteByte(writer, v >> 7);  // Highest bit is lost.
    }
}

void Writer_WritePackedUInt32(Writer1 *writer, uint32_t l)
{
    while (l >= 0x80)
    {
        // Write the lowest 7 bits, and set the high bit to indicate that
        // at least one more byte will follow.
        Writer_WriteByte(writer, 0x80 | (l & 0x7f));

        l >>= 7;
    }
    // Write the last byte, with the high bit clear.
    Writer_WriteByte(writer, l);
}
