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

#ifndef __DOOMSDAY_READER_H__
#define __DOOMSDAY_READER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "dd_types.h"

struct reader_s; // The reader instance (opaque).
typedef struct reader_s Reader;

/**
 * Constructs a new reader. The reader will use the engine's netBuffer
 * as the reading buffer. The reader has to be destroyed with Reader_Destruct()
 * after it is not needed any more.
 */
Reader* Reader_New(void);

/**
 * Initializes the reader. The reader will use @a buffer as the reading buffer.
 * The buffer is expected to use network byte order. The reader has to be destroyed
 * with Reader_Destruct() after it is not needed any more.
 *
 * @param buffer  Buffer to use for reading.
 * @param len     Length of the buffer.
 */
Reader* Reader_NewWithBuffer(const byte* buffer, size_t len);

/**
 * Destroys the reader.
 */
void Reader_Delete(Reader* reader);

/**
 * Returns the current position of the reader.
 */
size_t Reader_Pos(const Reader* reader);

/**
 * Returns the size of the reading buffer.
 */
size_t Reader_Size(const Reader* reader);

/**
 * Determines whether the reader is in the end of buffer, i.e., there is nothing
 * more to read.
 */
boolean Reader_AtEnd(const Reader* reader);

/**
 * Sets the position of the reading cursor in the buffer.
 *
 * @param reader  Reader.
 * @param newPos  New position.
 */
void Reader_SetPos(Reader* reader, size_t newPos);

int8_t      Reader_ReadChar(Reader* reader);
byte        Reader_ReadByte(Reader* reader);
int16_t     Reader_ReadInt16(Reader* reader);
uint16_t    Reader_ReadUInt16(Reader* reader);
int32_t     Reader_ReadInt32(Reader* reader);
uint32_t    Reader_ReadUInt32(Reader* reader);
float       Reader_ReadFloat(Reader* reader);

/**
 * Reads @a len bytes into @a buffer.
 */
void Reader_Read(Reader* reader, void* buffer, size_t len);

/**
 * Only 15 bits can be used for the number because the high bit of the
 * lower byte is used to determine whether the upper byte follows or not.
 */
uint16_t Reader_ReadPackedUInt16(Reader* reader);

uint32_t Reader_ReadPackedUInt32(Reader* reader);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __DOOMSDAY_READER_H__
