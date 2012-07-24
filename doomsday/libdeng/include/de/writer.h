/**
 * @file de/writer.h
 * Serializer for writing values and data into a byte array.
 *
 * @ingroup base
 *
 * Writer instances ensure that all values written into the array are stored in
 * little-endian (Intel) byte order. All write operations are also checked
 * against the buffer boundaries; writing too much data into the buffer results
 * in an error.
 *
 * If @c DENG_WRITER_TYPECHECK is defined, all written data is preceded by a type
 * check code, which is checked when the values are read by Reader. This
 * guarantees that data is interpreted as written.
 *
 * @see reader.h, Reader
 *
 * @todo  This should be converted into a C wrapper for the libdeng2 de::Writer.
 *
 * @authors Copyright &copy; 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_WRITER_H
#define LIBDENG_WRITER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

#ifdef DENG_WRITER_TYPECHECK
// Writer Type Check Codes.
enum
{
    WTCC_CHAR = 0x13,
    WTCC_BYTE = 0xf6,
    WTCC_INT16 = 0x55,
    WTCC_UINT16 = 0xab,
    WTCC_INT32 = 0x3f,
    WTCC_UINT32 = 0xbb,
    WTCC_FLOAT = 0x71,
    WTCC_BLOCK = 0x6e
};
#endif

struct writer_s; // The writer instance (opaque).

/**
 * Writer instance. Constructed with Writer_New() or one of the other constructors.
 */
typedef struct writer_s Writer;

typedef void (*Writer_Callback_WriteInt8) (Writer* w, char v);
typedef void (*Writer_Callback_WriteInt16)(Writer* w, short v);
typedef void (*Writer_Callback_WriteInt32)(Writer* w, int v);
typedef void (*Writer_Callback_WriteFloat)(Writer* w, float v);
typedef void (*Writer_Callback_WriteData) (Writer* w, const char* data, int len);

/**
 * Constructs a new writer. The writer will use @a buffer as the writing buffer.
 * The buffer will use small endian byte order. The writer has to be destroyed
 * with Writer_Delete() after it is not needed any more.
 *
 * @param buffer  Buffer to use for reading.
 * @param maxLen  Maximum length of the buffer.
 */
DENG_PUBLIC Writer* Writer_NewWithBuffer(byte* buffer, size_t maxLen);

/**
 * Constructs a new writer. The writer will allocate memory for the buffer
 * while more data gets written. The writer has to be destroyed with
 * Writer_Delete() after it is not needed any more.
 *
 * @param maxLen  Maximum size for the buffer. Use zero for unlimited size.
 */
DENG_PUBLIC Writer* Writer_NewWithDynamicBuffer(size_t maxLen);

/**
 * Constructs a new writer that has no memory buffer of its own. Instead, all
 * the write operations will get routed to user-provided callbacks. The writer
 * has to be destroyed with Writer_Delete() after it is not needed any more.
 */
DENG_PUBLIC Writer* Writer_NewWithCallbacks(Writer_Callback_WriteInt8  writeInt8,
                                            Writer_Callback_WriteInt16 writeInt16,
                                            Writer_Callback_WriteInt32 writeInt32,
                                            Writer_Callback_WriteFloat writeFloat,
                                            Writer_Callback_WriteData  writeData);

/**
 * Destroys the writer.
 */
DENG_PUBLIC void Writer_Delete(Writer* writer);

/**
 * Returns the current output size of the writer, i.e., how much has been written
 * so far.
 */
DENG_PUBLIC size_t Writer_Size(const Writer* writer);

/**
 * Returns a pointer to the beginning of the written data.
 * @see Writer_Size()
 */
DENG_PUBLIC const byte* Writer_Data(const Writer* writer);

/**
 * Returns the maximum size of the writing buffer.
 */
DENG_PUBLIC size_t Writer_TotalBufferSize(const Writer* writer);

/**
 * Returns the number of bytes left for writing.
 */
DENG_PUBLIC size_t Writer_BytesLeft(const Writer* writer);

/**
 * Sets the position of the writing cursor in the buffer.
 *
 * @param writer  Writer.
 * @param newPos  New position.
 */
DENG_PUBLIC void Writer_SetPos(Writer* writer, size_t newPos);

/**
 * Writes value @a v to the destination buffer using little-endian byte order.
 */
///@{
DENG_PUBLIC void Writer_WriteChar   (Writer* writer, char     v);
DENG_PUBLIC void Writer_WriteByte   (Writer* writer, byte     v);
DENG_PUBLIC void Writer_WriteInt16  (Writer* writer, int16_t  v);
DENG_PUBLIC void Writer_WriteUInt16 (Writer* writer, uint16_t v);
DENG_PUBLIC void Writer_WriteInt32  (Writer* writer, int32_t  v);
DENG_PUBLIC void Writer_WriteUInt32 (Writer* writer, uint32_t v);
DENG_PUBLIC void Writer_WriteFloat  (Writer* writer, float    v);
///@}

/**
 * Writes @a len bytes from @a buffer.
 */
DENG_PUBLIC void Writer_Write(Writer* writer, const void* buffer, size_t len);

/**
 * Only 15 bits can be used for the number because the high bit of the
 * lower byte is used to determine whether the upper byte follows or not.
 */
DENG_PUBLIC void Writer_WritePackedUInt16(Writer* writer, uint16_t v);

DENG_PUBLIC void Writer_WritePackedUInt32(Writer* writer, uint32_t v);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_WRITER_H
