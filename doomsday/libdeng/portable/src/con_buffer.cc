/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * con_buffer.c: Console history buffer.
 *
 * NOTE: With respect to threading, this code assumes that a cbuffer,
 *       mutex's lock/unlock state has been manipulated by the CALLER of
 *       private functions declared here. Therefore, public functions must
 *       lock before and unlock after calling a private function.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_misc.h"
#include "doomsday.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct cbnode_s {
    cbline_t*           data;
    struct cbnode_s*    next, *prev;
} cbnode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void insertNodeAtEnd(cbuffer_t* buf, cbnode_t* newnode);
static void removeNode(cbuffer_t* buf, cbnode_t* node);
static void moveNodeForReuse(cbuffer_t* buf, cbnode_t* node);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * NOTE: Also destroys the data object.
 */
static void destroyNode(cbnode_t* node)
{
    cbline_t*               line = node->data;

    if(line->text)
        M_Free(line->text);
    M_Free(line);
    M_Free(node);
}

static void insertNodeAfter(cbuffer_t* buf, cbnode_t* node, cbnode_t* newnode)
{
    newnode->prev = node;
    newnode->next = node->next;
    if(node->next == NULL)
        buf->tailptr = newnode;
    else
        node->next->prev = newnode;

    node->next = newnode;
}

static void insertNodeBefore(cbuffer_t* buf, cbnode_t* node, cbnode_t* newnode)
{
    newnode->prev = node->prev;
    newnode->next = node;
    if(node->prev == NULL)
        buf->headptr = newnode;
    else
        node->prev->next = newnode;

    node->prev = newnode;
}

static void insertNodeAtStart(cbuffer_t* buf, cbnode_t* newnode)
{
    if(buf->headptr == NULL)
    {
        buf->headptr = newnode;
        buf->tailptr = newnode;
        newnode->prev = NULL;
        newnode->next = NULL;
    }
    else
        insertNodeBefore(buf, (cbnode_t*) buf->headptr, newnode);
}

static void insertNodeAtEnd(cbuffer_t* buf, cbnode_t* newnode)
{
    if(buf->tailptr == NULL)
        insertNodeAtStart(buf, newnode);
    else
        insertNodeAfter(buf, (cbnode_t*) buf->tailptr, newnode);
}

static void moveNodeForReuse(cbuffer_t* buf, cbnode_t* node)
{
    cbline_t*               line;

    if(buf->unused != NULL)
        node->next = (cbnode_t*) buf->unused;
    else
        node->next = NULL;
    node->prev = NULL;
    buf->unused = node;

    line = node->data;
    line->flags = 0;

    if(line->text)
        memset(line->text, 0, line->len);
}

static void removeNode(cbuffer_t* buf, cbnode_t* node)
{
   if(node->prev == NULL)
       buf->headptr = node->next;
   else
       node->prev->next = node->next;

   if(node->next == NULL)
       buf->tailptr = node->prev;
   else
       node->next->prev = node->prev;

   moveNodeForReuse(buf, node);
}

/**
 * Creates a new console history buffer.
 *
 * @param maxNumLines       Maximum number of lines the buffer can hold,
 *                          @c 0, means unlimited.
 * @param maxLineLength     Maximum length of each line in the buffer.
 * @param flags             Console buffer flags (CBF_*).
 *
 * @return                  Ptr to the newly created console buffer.
 */
cbuffer_t* Con_NewBuffer(uint maxNumLines, uint maxLineLength, int flags)
{
    char                    name[32+1];
    cbuffer_t*              buf;

    if(maxLineLength < 1)
        Con_Error("Con_NewBuffer: Odd buffer params");

    buf = (cbuffer_t*) M_Malloc(sizeof(*buf));

    snprintf(name, 32, "CBufferMutex%p", buf);
    buf->mutex = Sys_CreateMutex(name);

    buf->flags = flags;
    buf->headptr = buf->tailptr = NULL;
    buf->numLines = 0;
    buf->maxLineLen = maxLineLength;
    buf->writebuf = (char*) M_Calloc(buf->maxLineLen + 1);
    buf->wbc = 0;
    buf->wbFlags = 0;
    buf->maxLines = maxNumLines;
    if(buf->maxLines != 0) // not unlimited.
    {   // Might as well allocate the index now.
        buf->index = (cbline_t**) M_Malloc(sizeof(cbline_t*) * buf->maxLines);
        buf->indexSize = buf->maxLines;
    }
    else
    {
        buf->index = NULL;
        buf->indexSize = 0;
    }

    buf->indexGood = true; // its empty so...
    buf->unused = NULL;

    return buf;
}

static void clearBuffer(cbuffer_t* buf, boolean destroy)
{
    cbnode_t*               n, *np;

    // Free the buffer contents.
    n = (cbnode_t*) buf->headptr;
    while(n != NULL)
    {
        np = n->next;
        if(destroy)
            destroyNode(n);
        else
            moveNodeForReuse(buf, n);
        n = np;
    }
    buf->headptr = buf->tailptr = NULL;
    buf->numLines = 0;

    memset(buf->writebuf, 0, buf->maxLineLen);
    buf->wbc = 0;
}

/**
 * Clear the contents of a console history buffer.
 *
 * @param buf               Ptr to the buffer to be cleared.
 */
void Con_BufferClear(cbuffer_t* buf)
{
    if(!buf)
        return;

    Sys_Lock(buf->mutex);
    clearBuffer(buf, false);
    Sys_Unlock(buf->mutex);
}

/**
 * Destroy an existing console history buffer.
 *
 * @param buf               Ptr to the buffer to be destroyed.
 */
void Con_DestroyBuffer(cbuffer_t* buf)
{
    if(buf)
    {
        cbnode_t*               n, *np;

        Sys_Lock(buf->mutex);

        clearBuffer(buf, true);
        M_Free(buf->writebuf);
        if(buf->index)
            M_Free(buf->index);

        // Free any unused nodes.
        n = (cbnode_t*) buf->unused;
        while(n)
        {
            np = n->next;
            destroyNode(n);
            n = np;
        }

        Sys_Unlock(buf->mutex);
        Sys_DestroyMutex(buf->mutex);
        buf->mutex = 0;
        M_Free(buf);
    }
}

/**
 * Change the maximum line length for the given console history buffer.
 * Existing lines are unaffected, the change only impacts new lines.
 *
 * @param buf               Ptr to the buffer to be changed.
 * @param length            Length to set the max line length to.
 */
void Con_BufferSetMaxLineLength(cbuffer_t* buf, uint length)
{
    if(!buf)
        return;

    Sys_Lock(buf->mutex);
    buf->maxLineLen = length;

    // The write buffer will be trimmed if resizing smaller.
    buf->writebuf = (char*) M_Realloc(buf->writebuf, buf->maxLineLen + 1);
    if(buf->wbc > buf->maxLineLen)
        buf->wbc = buf->maxLineLen;
    Sys_Unlock(buf->mutex);
}

/**
 * @param buf           Ptr to the buffer to be queried.
 *
 * @return              Number of lines currently in the buffer.
 */
uint Con_BufferNumLines(cbuffer_t* buf)
{
    uint                num;
    if(!buf)
        return 0;

    Sys_Lock(buf->mutex);
    num = buf->numLines; // index + 1
    Sys_Unlock(buf->mutex);

    return num;
}

static cbline_t const* bufferGetLine(cbuffer_t* buf, uint idx)
{
    const cbline_t*     ptr = NULL;

    if(!(buf->numLines == 0 || idx >= buf->numLines))
    {
        if(!buf->indexGood)
        {   // Rebuild the index.
            uint                i;
            cbnode_t*           node;

            // Do we need to enlarge the index?
            if(buf->indexSize < buf->numLines)
            {
                buf->index = (cbline_t**) M_Realloc(buf->index,
                    sizeof(cbline_t*) * buf->numLines);
                buf->indexSize = buf->numLines;
            }

            i = 0;
            node = (cbnode_t*) buf->headptr;
            while(node != NULL)
            {
                buf->index[i++] = node->data;
                node = node->next;
            }

            buf->indexGood = true;
        }
        ptr = (const cbline_t*) buf->index[idx];
    }

    return ptr;
}

static uint bufferGetLines(cbuffer_t* buf, uint reqCount, int firstIdx,
                           cbline_t const** list)
{
    if((long) firstIdx <= (long) buf->numLines)
    {
        uint                i, n, idx, count = reqCount;

        if(firstIdx < 0)
        {
            long                other = -((long) firstIdx);

            if(other > (long) buf->numLines)
                firstIdx = 0;
            else
                firstIdx = buf->numLines - other;
        }

        idx = firstIdx;
        if(reqCount == 0 || idx + count > buf->numLines)
            count = buf->numLines - idx;

        // Collect the ptrs.
        for(i = 0, n = idx; i < count; ++i)
            list[i] = bufferGetLine(buf, n++);

        // Terminate.
        list[i] = NULL;

        return count; // index + 1
    }

    if(*list)
        list[0] = NULL;
    return 0;
}

/**
 * Retrive an array of un-mutable ptrs to console buffer lines from the
 * given cbuffer.
 *
 * NOTE: The array must be free'd with M_Free().
 *
 * @param buf           Ptr to the buffer to retrieve the lines from.
 * @param reqCount      Number of lines requested from the buffer, zero means
 *                      use the current number of lines as the limit.
 * @param firstIdx      Line index of the first line to be retrieved. If
 *                      negative, the index is from the end of list.
 * @param list          Ptr to an array of console buffer ptrs which we'll
 *                      write to and terminate with @c NULL,
 *
 * @return              The number of elements written back to the buffer.
 */
uint Con_BufferGetLines(cbuffer_t* buf, uint reqCount, int firstIdx,
                        cbline_t const** list)
{
    if(buf)
    {
        uint                result;

        Sys_Lock(buf->mutex);
        result = bufferGetLines(buf, reqCount, firstIdx, list);
        Sys_Unlock(buf->mutex);

        return result;
    }

    if(*list)
        list[0] = NULL;

    return 0;
}

/**
 * Retrieve an un-mutable ptr to the line with the given index from the
 * history buffer.
 *
 * @param buf               Ptr to the buffer to use.
 * @param idx               Index of the line to retrieve.
 *
 * @return                  Ptr to the cbline_t with the requested index or
 *                          @c NULL, if the index was invalid.
 */
const cbline_t* Con_BufferGetLine(cbuffer_t* buf, uint idx)
{
    if(buf)
    {
        const cbline_t*         ptr;

        Sys_Lock(buf->mutex);
        ptr = bufferGetLine(buf, idx);
        Sys_Unlock(buf->mutex);
        return ptr;
    }

    return NULL;
}

/**
 * Create a new buffer line and link it into the history buffer.
 *
 * @param buf               Ptr to the buffer to use.
 *
 * @return                  Ptr to the cbline_t with the requested index or
 *                          @c NULL, if the index was invalid.
 */
static cbline_t* bufferNewLine(cbuffer_t* buf)
{
    cbnode_t*               node;
    cbline_t*               line;

    if(!buf)
        return NULL; // This is unacceptable!

    // Do we have any unused nodes we can reuse?
    if(buf->unused != NULL)
    {
        node = (cbnode_t*) buf->unused;
        buf->unused = node->next;
        line = node->data;
    }
    else
    {
        // Allocate another line.
        line = (cbline_t*) M_Malloc(sizeof(cbline_t));
        node = (cbnode_t*) M_Malloc(sizeof(cbnode_t));
        node->data = line;

        line->text = NULL;
        line->len = 0;
    }
    assert(node->data);

    buf->numLines++;

    line->flags = 0;

    // Link it in.
    insertNodeAtEnd(buf, node);

    // Check if there are too many lines.
    if(buf->maxLines != 0 && buf->numLines > buf->maxLines)
    {
        // Drop the earliest.
        removeNode(buf, (cbnode_t*) buf->headptr);
        buf->numLines--;
    }

    buf->indexGood = false; // it will be updated when needed.

    return line;
}

static void bufferFlush(cbuffer_t* buf)
{
    uint                    len;
    cbline_t*               line;

    // Is there anything to flush?
    if(buf->wbc < 1)
        return;

    line = bufferNewLine(buf);

    //
    // Flush the write buffer.
    //
    len = buf->wbc;
    if(line->text != NULL)
    {   // We are re-using an existing line so we may not need to
        // reallocate at all.
        if(line->len <= len)
        {
            line->len = len + 1;
            line->text = (char*) M_Realloc(line->text, line->len);
        }
    }
    else
    {
        line->len = len + 1;
        line->text = (char*) M_Malloc(line->len);
    }

    memcpy(line->text, buf->writebuf, len);
    line->text[len] = 0;

    line->flags = buf->wbFlags;

    // Clear the write buffer.
    memset(buf->writebuf, 0, buf->maxLineLen);
    buf->wbc = 0;
    buf->wbFlags = 0;
}

/**
 * Flushes the contents of the write buffer to the history buffer.
 *
 * @param buf               Ptr to the history buffer to flush.
 */
void Con_BufferFlush(cbuffer_t* buf)
{
    if(!buf)
        return;

    Sys_Lock(buf->mutex);
    bufferFlush(buf);
    Sys_Unlock(buf->mutex);
}

/**
 * Write the given text string (plus optional flags) to the buffer.
 *
 * @param buf               Ptr to the buffer to write to.
 * @param flags             CBLF_* flags in use for this write.
 * @param txt               Ptr to the text string to be written.
 */
void Con_BufferWrite(cbuffer_t* buf, int flags, const char* txt)
{
    if(!buf)
        return;

    Sys_Lock(buf->mutex);

    // Check for special write actions first.
    if(flags & CBLF_RULER)
    {
        bufferFlush(buf);

        bufferNewLine(buf)->flags |= CBLF_RULER;
        flags &= ~CBLF_RULER;
    }

    if(!(!txt || !strcmp(txt, "")))
    {
        size_t                  i, len = strlen(txt);

        // Copy the text into the write buffer and flush to the history
        // buffer when as necessary/required.
        for(i = 0; i < len; ++i)
        {
            buf->wbFlags = flags;
            if(txt[i] == '\n' || buf->wbc >= buf->maxLineLen) // A new line?
            {
                bufferFlush(buf);

                // Newlines won't get in the buffer at all.
                if(txt[i] == '\n')
                    continue;
            }
            // Copy the next character to the write buffer.
            buf->writebuf[buf->wbc++] = txt[i];
        }

        if(buf->flags & CBF_ALWAYSFLUSH)
        {   // Don't leave data in the write buffer.
            bufferFlush(buf);
        }
    }
    Sys_Unlock(buf->mutex);
}
