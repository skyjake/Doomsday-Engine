/**\file con_buffer.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

/**
 * Console history buffer.
 */

#ifndef LIBDENG_CONSOLE_BUFFER_H
#define LIBDENG_CONSOLE_BUFFER_H

#include "sys_system.h"

/**
 * @defgroup consoleBufferLineFlags Console Buffer Line Flags.
 */
/*@{*/
// These correspond the good old text mode VGA colors.
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

// A console buffer line.
typedef struct cbline_s {
    uint            len; // This is the length of the line (no term).
    char*           text; // This is the text.
    int             flags;
} cbline_t;

#define CBF_ALWAYSFLUSH  0x00000001 // don't leave data in the write buffer.

// A console buffer.
typedef struct {
    mutex_t         mutex;
    int             flags; // CBF_* flags.
    uint            numLines; // How many lines are there in the buffer?
    uint            maxLines; // Maximum number of lines for the buffer.
    uint            maxLineLen; // Maximum length of a line.

    void*           headptr;
    void*           tailptr;
    void*           unused;

    cbline_t**      index; // Used when indexing the buffer for read.
    uint            indexSize;
    boolean         indexGood; // If the index needs updating.

    char*           writebuf; // write buffer.
    uint            wbc; // write buffer cursor.
    int             wbFlags; // write buffer line flags.
} cbuffer_t;

cbuffer_t* Con_NewBuffer(uint maxNumLines, uint maxLineLength, int cbflags);
void Con_DestroyBuffer(cbuffer_t* buf);

/**
 * Write the given text string (plus optional flags) to the buffer.
 *
 * @param buf  Ptr to the buffer to write to.
 * @param flags  @see consoleBufferLineFlags
 * @param txt  Ptr to the text string to be written.
 */
void Con_BufferWrite(cbuffer_t* buf, int flags, const char* txt);

void Con_BufferFlush(cbuffer_t* buf);
void Con_BufferClear(cbuffer_t* buf);

/// @return  Current maximum line length in characters.
uint Con_BufferMaxLineLength(cbuffer_t* buf);

/**
 * Change the maximum line length for the given console history buffer.
 * Existing lines are unaffected, the change only impacts new lines.
 *
 * @param buf  Ptr to the buffer to be changed.
 * @param length  Length to set the max line length to.
 */
void Con_BufferSetMaxLineLength(cbuffer_t* buf, uint length);

uint Con_BufferNumLines(cbuffer_t* buf);

const cbline_t* Con_BufferGetLine(cbuffer_t* buf, uint idx);

/**
 * @defgroup bufferLineFlags Buffer Line Flags.
 */ 
/*@{*/
#define BLF_OMIT_RULER      0x1 // Ignore rulers.
#define BLF_OMIT_EMPTYLINE  0x2 // Ignore empty lines.
/*@}*/

/**
 * Collate an array of ptrs to the immutable @c cbline_t objects owned by
 * the cbuffer. Caller retains ownership of @a list.
 *
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
uint Con_BufferGetLines2(cbuffer_t* buf, uint reqCount, int firstIdx, cbline_t const** list, int blflags);
uint Con_BufferGetLines(cbuffer_t* buf, uint reqCount, int firstIdx, cbline_t const** list);
#endif /* LIBDENG_CONSOLE_BUFFER_H */
