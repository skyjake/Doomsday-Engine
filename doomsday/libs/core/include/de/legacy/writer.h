/**
 * @file de/writer.h
 * Serializer for writing values and data into a byte array.
 *
 * Writer1 instances ensure that all values written into the array are stored in
 * little-endian (Intel) byte order. All write operations are also checked
 * against the buffer boundaries; writing too much data into the buffer results
 * in an error.
 *
 * If @c DE_WRITER_TYPECHECK is defined, all written data is preceded by a type
 * check code, which is checked when the values are read by Reader1. This
 * guarantees that data is interpreted as written.
 *
 * @see reader.h, Reader1
 *
 * @todo  This should be converted into a C wrapper for de::Writer.
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

#ifndef DE_WRITER_H
#define DE_WRITER_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup legacy
/// @{

#ifdef DE_WRITER_TYPECHECK
// Writer1 Type Check Codes.
enum
{
    WTCC_CHAR   = 0x13,
    WTCC_BYTE   = 0xf6,
    WTCC_INT16  = 0x55,
    WTCC_UINT16 = 0xab,
    WTCC_INT32  = 0x3f,
    WTCC_UINT32 = 0xbb,
    WTCC_FLOAT  = 0x71,
    WTCC_BLOCK  = 0x6e
};
#endif

struct writer_s; // The writer instance (opaque).

/**
 * Writer1 instance. Constructed with Writer_NewWithBuffer() or one of the other
 * constructors.
 */
typedef struct writer_s Writer1;

typedef void (*Writer_Callback_WriteInt8) (Writer1 *w, char v);
typedef void (*Writer_Callback_WriteInt16)(Writer1 *w, short v);
typedef void (*Writer_Callback_WriteInt32)(Writer1 *w, int v);
typedef void (*Writer_Callback_WriteFloat)(Writer1 *w, float v);
typedef void (*Writer_Callback_WriteData) (Writer1 *w, const char *data, int len);

/**
 * Constructs a new writer. The writer will use @a buffer as the writing buffer.
 * The buffer will use small endian byte order. The writer has to be destroyed
 * with Writer_Delete() after it is not needed any more.
 *
 * @param buffer  Buffer to use for reading.
 * @param maxLen  Maximum length of the buffer.
 */
DE_PUBLIC Writer1 *Writer_NewWithBuffer(byte *buffer, size_t maxLen);

/**
 * Constructs a new writer. The writer will allocate memory for the buffer
 * while more data gets written. The writer has to be destroyed with
 * Writer_Delete() after it is not needed any more.
 *
 * @param maxLen  Maximum size for the buffer. Use zero for unlimited size.
 */
DE_PUBLIC Writer1 *Writer_NewWithDynamicBuffer(size_t maxLen);

/**
 * Constructs a new writer that has no memory buffer of its own. Instead, all
 * the write operations will get routed to user-provided callbacks. The writer
 * has to be destroyed with Writer_Delete() after it is not needed any more.
 */
DE_PUBLIC Writer1 *Writer_NewWithCallbacks(Writer_Callback_WriteInt8  writeInt8,
                                             Writer_Callback_WriteInt16 writeInt16,
                                             Writer_Callback_WriteInt32 writeInt32,
                                             Writer_Callback_WriteFloat writeFloat,
                                             Writer_Callback_WriteData  writeData);

/**
 * Destroys the writer.
 */
DE_PUBLIC void Writer_Delete(Writer1 *writer);

/**
 * Returns the current output size of the writer, i.e., how much has been written
 * so far.
 */
DE_PUBLIC size_t Writer_Size(const Writer1 *writer);

/**
 * Returns a pointer to the beginning of the written data.
 * @see Writer_Size()
 */
DE_PUBLIC const byte *Writer_Data(const Writer1 *writer);

/**
 * Returns the maximum size of the writing buffer.
 */
DE_PUBLIC size_t Writer_TotalBufferSize(const Writer1 *writer);

/**
 * Returns the number of bytes left for writing.
 */
DE_PUBLIC size_t Writer_BytesLeft(const Writer1 *writer);

/**
 * Sets the position of the writing cursor in the buffer.
 *
 * @param writer  Writer1.
 * @param newPos  New position.
 */
DE_PUBLIC void Writer_SetPos(Writer1 *writer, size_t newPos);

/**
 * Writes value @a v to the destination buffer using little-endian byte order.
 */
///@{
DE_PUBLIC void Writer_WriteChar   (Writer1 *writer, char     v);
DE_PUBLIC void Writer_WriteByte   (Writer1 *writer, byte     v);
DE_PUBLIC void Writer_WriteInt16  (Writer1 *writer, int16_t  v);
DE_PUBLIC void Writer_WriteUInt16 (Writer1 *writer, uint16_t v);
DE_PUBLIC void Writer_WriteInt32  (Writer1 *writer, int32_t  v);
DE_PUBLIC void Writer_WriteUInt32 (Writer1 *writer, uint32_t v);
DE_PUBLIC void Writer_WriteFloat  (Writer1 *writer, float    v);
///@}

/**
 * Writes @a len bytes from @a buffer.
 */
DE_PUBLIC void Writer_Write(Writer1 *writer, const void *buffer, size_t len);

/**
 * Only 15 bits can be used for the number because the high bit of the
 * lower byte is used to determine whether the upper byte follows or not.
 */
DE_PUBLIC void Writer_WritePackedUInt16(Writer1 *writer, uint16_t v);

DE_PUBLIC void Writer_WritePackedUInt32(Writer1 *writer, uint32_t v);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_WRITER_H
