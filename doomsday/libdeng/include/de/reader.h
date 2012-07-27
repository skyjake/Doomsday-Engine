/**
 * @file de/reader.h
 * Deserializer for reading values and data from a byte array. @ingroup base
 *
 * Reader instances assume that all values stored in the source array are in
 * little-endian (Intel) byte order. All read operations are checked against
 * the buffer boundaries; reading too much data from the buffer results in an
 * error.
 *
 * If @c DENG_WRITER_TYPECHECK is defined, the type check codes preceding
 * the data values are checked for validity. The assumption is that the source
 * data buffer has been created using a Writer.
 *
 * @see writer.h, Writer
 *
 * @todo  This should be converted into a C wrapper for the libdeng2 de::Reader.
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

#ifndef LIBDENG_READER_H
#define LIBDENG_READER_H

#include "libdeng.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct reader_s; // The reader instance (opaque).

/**
 * Reader instance. Constructed with Reader_NewWithBuffer() or one of the other
 * constructors.
 */
typedef struct reader_s Reader;

typedef char  (*Reader_Callback_ReadInt8) (Reader*);
typedef short (*Reader_Callback_ReadInt16)(Reader*);
typedef int   (*Reader_Callback_ReadInt32)(Reader*);
typedef float (*Reader_Callback_ReadFloat)(Reader*);
typedef void  (*Reader_Callback_ReadData) (Reader*, char* data, int len);

/**
 * Initializes the reader. The reader will use @a buffer as the reading buffer.
 * The buffer is expected to use network byte order. The reader has to be destroyed
 * with Reader_Delete() after it is not needed any more.
 *
 * @param buffer  Buffer to use for reading.
 * @param len     Length of the buffer.
 */
DENG_PUBLIC Reader* Reader_NewWithBuffer(const byte* buffer, size_t len);

/**
 * Constructs a new reader that has no memory buffer of its own. Instead, all the
 * read operations will get routed to user-provided callbacks.
 */
DENG_PUBLIC Reader* Reader_NewWithCallbacks(Reader_Callback_ReadInt8  readInt8,
                                            Reader_Callback_ReadInt16 readInt16,
                                            Reader_Callback_ReadInt32 readInt32,
                                            Reader_Callback_ReadFloat readFloat,
                                            Reader_Callback_ReadData  readData);

/**
 * Destroys the reader.
 */
DENG_PUBLIC void Reader_Delete(Reader* reader);

/**
 * Returns the current position of the reader.
 */
DENG_PUBLIC size_t Reader_Pos(const Reader* reader);

/**
 * Returns the size of the reading buffer.
 */
DENG_PUBLIC size_t Reader_Size(const Reader* reader);

/**
 * Determines whether the reader is in the end of buffer, i.e., there is nothing
 * more to read.
 */
DENG_PUBLIC boolean Reader_AtEnd(const Reader* reader);

/**
 * Sets the position of the reading cursor in the buffer.
 *
 * @param reader  Reader.
 * @param newPos  New position.
 */
DENG_PUBLIC void Reader_SetPos(Reader* reader, size_t newPos);

/**
 * Reads a value from the source buffer.
 */
///@{
DENG_PUBLIC int8_t      Reader_ReadChar   (Reader* reader);
DENG_PUBLIC byte        Reader_ReadByte   (Reader* reader);
DENG_PUBLIC int16_t     Reader_ReadInt16  (Reader* reader);
DENG_PUBLIC uint16_t    Reader_ReadUInt16 (Reader* reader);
DENG_PUBLIC int32_t     Reader_ReadInt32  (Reader* reader);
DENG_PUBLIC uint32_t    Reader_ReadUInt32 (Reader* reader);
DENG_PUBLIC float       Reader_ReadFloat  (Reader* reader);
///@}

/**
 * Reads @a len bytes into @a buffer.
 */
DENG_PUBLIC void Reader_Read(Reader* reader, void* buffer, size_t len);

/**
 * Only 15 bits can be used for the number because the high bit of the
 * lower byte is used to determine whether the upper byte follows or not.
 */
DENG_PUBLIC uint16_t Reader_ReadPackedUInt16(Reader* reader);

DENG_PUBLIC uint32_t Reader_ReadPackedUInt32(Reader* reader);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_READER_H
