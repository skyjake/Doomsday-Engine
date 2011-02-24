/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * con_buffer.h: Console history buffer.
 */

#ifndef __DOOMSDAY_CONSOLE_BUFFER_H__
#define __DOOMSDAY_CONSOLE_BUFFER_H__

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

cbuffer_t*      Con_NewBuffer(uint maxNumLines, uint maxLineLength,
                              int flags);
void            Con_DestroyBuffer(cbuffer_t* buf);

void            Con_BufferWrite(cbuffer_t* buf, int flags, const char* txt);
void            Con_BufferFlush(cbuffer_t* buf);
void            Con_BufferClear(cbuffer_t* buf);
void            Con_BufferSetMaxLineLength(cbuffer_t* buf, uint length);

const cbline_t* Con_BufferGetLine(cbuffer_t* buf, uint idx);
uint            Con_BufferGetLines(cbuffer_t* buf, uint reqCount,
                                   int firstIdx, cbline_t const** list);
uint            Con_BufferNumLines(cbuffer_t* buf);
#endif
