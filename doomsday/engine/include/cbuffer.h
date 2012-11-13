/**\file cbuffer.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_CONSOLE_BUFFER_H
#define LIBDENG_CONSOLE_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup consoleBufferLineFlags Console Buffer Line Flags
 * @ingroup flags
 *
 * These correspond to the good old text mode VGA colors.
 * @{
 */
#define CBLF_BLACK          0x00000001
#define CBLF_BLUE           0x00000002
#define CBLF_GREEN          0x00000004
#define CBLF_CYAN           0x00000008
#define CBLF_RED            0x00000010
#define CBLF_MAGENTA        0x00000020
#define CBLF_YELLOW         0x00000040
#define CBLF_WHITE          0x00000080
#define CBLF_LIGHT          0x00000100
#define CBLF_RULER          0x00000200
#define CBLF_CENTER         0x00000400
/*@}*/

typedef struct cbline_s {
    uint len; /// Length of the line in characters (no terminator).
    char* text; /// Text line.
    int flags; /// @see consoleBufferLineFlags
} cbline_t;

/**
 * @defgroup consoleBufferFlags Console Buffer Flags
 * @ingroup flags
 * @{
 */
#define CBF_ALWAYSFLUSH  0x00000001 // don't leave data in the write buffer.
/**@}*/

/**
 * CBuffer. Console text buffer.
 * @ingroup console
 */
struct cbuffer_s; // The cbuffer instance (opaque).
typedef struct cbuffer_s CBuffer;

/**
 * Construct a new (empty) console buffer.
 *
 * @param maxNumLines  Maximum number of lines the buffer can hold.
 * @param maxLineLength  Maximum length of a text line in characters.
 * @param flags  @see consoleBufferFlags
 */
CBuffer* CBuffer_New(uint maxNumLines, uint maxLineLength, int flags);

void CBuffer_Delete(CBuffer* cb);

/**
 * Write the given text string (plus optional flags) to the buffer.
 *
 * @param cb   Console buffer.
 * @param flags  @see consoleBufferLineFlags
 * @param txt  Ptr to the text string to be written.
 */
void CBuffer_Write(CBuffer* cb, int flags, const char* txt);

/// Flush the content of the write buffer.
void CBuffer_Flush(CBuffer* cb);

/// Clear the text content of the buffer.
void CBuffer_Clear(CBuffer* cb);

/// @return  Current maximum line length in characters.
uint CBuffer_MaxLineLength(CBuffer* cb);

/**
 * Change the maximum line length. @note The existing lines are unaffected, the
 * change only impacts new lines.
 *
 * @param cb      Console buffer.
 * @param length  New max line length, in characters.
 */
void CBuffer_SetMaxLineLength(CBuffer* cb, uint length);

/// @return  Number of lines present in the buffer.
uint CBuffer_NumLines(CBuffer* cb);

/**
 * Retrieve an immutable ptr to the text line at index @a idx.
 *
 * @param cb   Console buffer.
 * @param idx  Index of the line to retrieve.
 *
 * @return  Text line at index @a idx, or @c NULL if invalid index.
 */
const cbline_t* CBuffer_GetLine(CBuffer* cb, uint idx);

/**
 * @defgroup bufferLineFlags Buffer Line Flags
 * @ingroup flags
 * @{
 */
#define BLF_OMIT_RULER      0x1 /// Ignore rulers.
#define BLF_OMIT_EMPTYLINE  0x2 /// Ignore empty lines.
/// @}

/**
 * Collate an array of ptrs to the immutable @c cbline_t objects owned by
 * the cbuffer. Caller retains ownership of @a list.
 *
 * @param cb            CBuffer instance.
 * @param reqCount      Number of lines requested from the buffer, zero means
 *                      use the current number of lines as the limit.
 * @param firstIdx      Line index of the first line to be retrieved. If
 *                      negative, the index is from the end of list.
 * @param list          Ptr to an array of console buffer ptrs which we'll
 *                      write to and terminate with @c NULL.
 * @param blflags       @see bufferLineFlags
 *
 * @return              The number of elements written back to the buffer.
 */
uint CBuffer_GetLines2(CBuffer* cb, uint reqCount, int firstIdx, cbline_t const** list, int blflags);
uint CBuffer_GetLines(CBuffer* cb, uint reqCount, int firstIdx, cbline_t const** list); /* blflags = 0 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_CONSOLE_BUFFER_H */
