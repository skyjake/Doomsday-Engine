/**
 * @file de/reader.h
 * Deserializer for reading values and data from a byte array.
 *
 * Reader1 instances assume that all values stored in the source array are in
 * little-endian (Intel) byte order. All read operations are checked against
 * the buffer boundaries; reading too much data from the buffer results in an
 * error.
 *
 * If @c DE_WRITER_TYPECHECK is defined, the type check codes preceding
 * the data values are checked for validity. The assumption is that the source
 * data buffer has been created using a Writer1.
 *
 * @see writer.h, Writer1
 *
 * @todo  This should be converted into a C wrapper for de::Reader.
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

#ifndef DE_READER_H
#define DE_READER_H

#include "../liblegacy.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup legacy
/// @{

struct reader_s; // The reader instance (opaque).

/**
 * Reader1 instance. Constructed with Reader_NewWithBuffer() or one of the other
 * constructors.
 */
typedef struct reader_s Reader1;

typedef char  (*Reader_Callback_ReadInt8) (Reader1 *);
typedef short (*Reader_Callback_ReadInt16)(Reader1 *);
typedef int   (*Reader_Callback_ReadInt32)(Reader1 *);
typedef float (*Reader_Callback_ReadFloat)(Reader1 *);
typedef void  (*Reader_Callback_ReadData) (Reader1 *, char *data, int len);

/**
 * Initializes the reader. The reader will use @a buffer as the reading buffer.
 * The buffer is expected to use network byte order. The reader has to be destroyed
 * with Reader_Delete() after it is not needed any more.
 *
 * @param buffer  Buffer to use for reading.
 * @param len     Length of the buffer.
 */
DE_PUBLIC Reader1 *Reader_NewWithBuffer(const byte *buffer, size_t len);

/**
 * Constructs a new reader that has no memory buffer of its own. Instead, all the
 * read operations will get routed to user-provided callbacks.
 */
DE_PUBLIC Reader1 *Reader_NewWithCallbacks(Reader_Callback_ReadInt8  readInt8,
                                             Reader_Callback_ReadInt16 readInt16,
                                             Reader_Callback_ReadInt32 readInt32,
                                             Reader_Callback_ReadFloat readFloat,
                                             Reader_Callback_ReadData  readData);

/**
 * Destroys the reader.
 */
DE_PUBLIC void Reader_Delete(Reader1 *reader);

/**
 * Returns the current position of the reader.
 */
DE_PUBLIC size_t Reader_Pos(const Reader1 *reader);

/**
 * Returns the size of the reading buffer.
 */
DE_PUBLIC size_t Reader_Size(const Reader1 *reader);

/**
 * Determines whether the reader is in the end of buffer, i.e., there is nothing
 * more to read.
 */
DE_PUBLIC dd_bool Reader_AtEnd(const Reader1 *reader);

/**
 * Sets the position of the reading cursor in the buffer.
 *
 * @param reader  Reader1.
 * @param newPos  New position.
 */
DE_PUBLIC void Reader_SetPos(Reader1 *reader, size_t newPos);

/**
 * Reads a value from the source buffer.
 */
///@{
DE_PUBLIC int8_t      Reader_ReadChar   (Reader1 *reader);
DE_PUBLIC byte        Reader_ReadByte   (Reader1 *reader);
DE_PUBLIC int16_t     Reader_ReadInt16  (Reader1 *reader);
DE_PUBLIC uint16_t    Reader_ReadUInt16 (Reader1 *reader);
DE_PUBLIC int32_t     Reader_ReadInt32  (Reader1 *reader);
DE_PUBLIC uint32_t    Reader_ReadUInt32 (Reader1 *reader);
DE_PUBLIC float       Reader_ReadFloat  (Reader1 *reader);
///@}

/**
 * Reads @a len bytes into @a buffer.
 */
DE_PUBLIC void Reader_Read(Reader1 *reader, void *buffer, size_t len);

/**
 * Only 15 bits can be used for the number because the high bit of the
 * lower byte is used to determine whether the upper byte follows or not.
 */
DE_PUBLIC uint16_t Reader_ReadPackedUInt16(Reader1 *reader);

DE_PUBLIC uint32_t Reader_ReadPackedUInt32(Reader1 *reader);

/// @}


/**
 * Reads x bits from the source stream and writes them to out.
 *
 * @warning Output buffer must be large enough to hold at least @a numBits!
 *
 * @param numBits  Number of bits to be read.
 * @param src      Current position in the source stream.
 * @param cb       Current byte. Used for tracking the current byte being read.
 * @param out      Read bits are ouput here.
 */
DE_PUBLIC void M_ReadBits(uint numBits, const uint8_t** src, uint8_t* cb, uint8_t* out);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_READER_H
