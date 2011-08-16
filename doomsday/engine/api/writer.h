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

#ifndef __DOOMSDAY_WRITER_H__
#define __DOOMSDAY_WRITER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "dd_types.h"

struct writer_s; // The writer instance (opaque).
typedef struct writer_s Writer;

/**
 * Constructs a new writer. The writer will use the engine's netBuffer
 * as the writing buffer. The writer has to be destroyed with Writer_Destruct()
 * after it is not needed any more.
 */
Writer* Writer_New(void);

/**
 * Initializes the writer. The writer will use @a buffer as the writing buffer.
 * The buffer will use network byte order. The writer has to be destroyed
 * with Writer_Destruct() after it is not needed any more.
 *
 * @param buffer  Buffer to use for reading.
 * @param maxLen  Maximum length of the buffer.
 */
Writer* Writer_NewWithBuffer(byte* buffer, size_t maxLen);

/**
 * Destroys the writer.
 */
void Writer_Destruct(Writer* writer);

/**
 * Returns the current output size of the writer, i.e., how much has been written
 * so far.
 */
size_t Writer_Size(const Writer* writer);

/**
 * Returns the maximum size of the writing buffer.
 */
size_t Writer_TotalBufferSize(const Writer* writer);

/**
 * Returns the number of bytes left for writing.
 */
size_t Writer_BytesLeft(const Writer* writer);

/**
 * Sets the position of the writing cursor in the buffer.
 *
 * @param writer  Writer.
 * @param newPos  New position.
 */
void Writer_SetPos(Writer* writer, size_t newPos);

void Writer_WriteChar(Writer* writer, char v);
void Writer_WriteByte(Writer* writer, byte v);
void Writer_WriteInt16(Writer* writer, int16_t v);
void Writer_WriteUInt16(Writer* writer, uint16_t v);
void Writer_WriteInt32(Writer* writer, int32_t v);
void Writer_WriteUInt32(Writer* writer, uint32_t v);
void Writer_WriteFloat(Writer* writer, float v);

/**
 * Writes @a len bytes from @a buffer.
 */
void Writer_Write(Writer* writer, const void* buffer, size_t len);

/**
 * Only 15 bits can be used for the number because the high bit of the
 * lower byte is used to determine whether the upper byte follows or not.
 */
void Writer_WritePackedUInt16(Writer* writer, uint16_t v);

void Writer_WritePackedUInt32(Writer* writer, uint32_t v);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __DOOMSDAY_WRITER_H__
