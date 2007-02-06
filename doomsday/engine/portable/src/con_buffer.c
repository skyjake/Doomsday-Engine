/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * con_buffer.c: Console history buffer.
 *
 * TODO: A buffer must be protected for multithreaded usage with a mutex so 
 *       that the busy drawer can read the console history contents. In
 *       other words, add a mutex_t to cbuffer_t and make sure it's locked
 *       whenever the buffer is accessed. Also mains that the buffer struct
 *       mustn't be accessed directly, just using functions.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct cbnode_s {
    cbline_t *data;
    struct cbnode_s *next, *prev;
} cbnode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void insertNodeAtEnd(cbuffer_t *buf, cbnode_t *newnode);
static void removeNode(cbuffer_t *buf, cbnode_t *node);
static void moveNodeForReuse(cbuffer_t *buf, cbnode_t *node);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Create a new buffer line and link it into the history buffer.
 *
 * @param buf               Ptr to the buffer to use.
 *
 * @return                  Ptr to the cbline_t with the requested index or
 *                          <code>NULL</code> if the index was invalid.
 */
static cbline_t *bufferNewLine(cbuffer_t *buf)
{
    cbnode_t   *node;
    cbline_t   *line;

    if(!buf)
        return NULL; // This is unacceptable!

    // Do we have any unused nodes we can reuse?
    if(buf->unused != NULL)
    {
        node = buf->unused;
        buf->unused = node->next;
        line = node->data;
    }
    else
    {
        // Allocate another line.
        line = M_Malloc(sizeof(cbline_t));
        node = M_Malloc(sizeof(cbnode_t));
        node->data = line;

        line->text = NULL;
    }
    buf->numLines++;

    line->len = 0;
    line->flags = 0;

    // Link it in.
    insertNodeAtEnd(buf, node);

    // Check if there are too many lines.
    if(buf->maxLines != 0 && buf->numLines > buf->maxLines)
    {
        // Drop the earliest.
        removeNode(buf, buf->headptr);
        buf->numLines--;
    }

    buf->indexGood = false; // it will be updated when needed.

    return line;
}

/**
 * NOTE: Also destroys the data object.
 */
static void destroyNode(cbnode_t *node)
{
    cbline_t   *line = node->data;

    if(line->text)
        M_Free(line->text);
    M_Free(line);
    M_Free(node);
}

static void insertNodeAfter(cbuffer_t *buf, cbnode_t *node, cbnode_t *newnode)
{
    newnode->prev = node;
    newnode->next = node->next;
    if(node->next == NULL)
        buf->tailptr = newnode;
    else
        node->next->prev = newnode;

    node->next = newnode;
}

static void insertNodeBefore(cbuffer_t *buf, cbnode_t *node, cbnode_t *newnode)
{
    newnode->prev = node->prev;
    newnode->next = node;
    if(node->prev == NULL)
        buf->headptr = newnode;
    else
        node->prev->next = newnode;

    node->prev = newnode;
}

static void insertNodeAtStart(cbuffer_t *buf, cbnode_t *newnode)
{
    if(buf->headptr == NULL)
    {
        buf->headptr = newnode;
        buf->tailptr = newnode;
        newnode->prev = NULL;
        newnode->next = NULL;
    }
    else
        insertNodeBefore(buf, buf->headptr, newnode);
}

static void insertNodeAtEnd(cbuffer_t *buf, cbnode_t *newnode)
{
    if(buf->tailptr == NULL)
        insertNodeAtStart(buf, newnode);
    else
        insertNodeAfter(buf, buf->tailptr, newnode);
}

static void moveNodeForReuse(cbuffer_t *buf, cbnode_t *node)
{
    if(buf->unused != NULL)
        node->next = buf->unused;
    else
        node->next = NULL;
    node->prev = NULL;
    buf->unused = node;
}

static void removeNode(cbuffer_t *buf, cbnode_t *node)
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
 *                          <code>0</code> means unlimited.
 * @param maxLineLength     Maximum length of each line in the buffer.
 * @param flags             Console buffer flags (CBF_*).
 *
 * @return                  Ptr to the newly created console buffer.
 */
cbuffer_t *Con_NewBuffer(uint maxNumLines, uint maxLineLength, int flags)
{
    cbuffer_t  *buf;

    if(maxLineLength < 1)
        Con_Error("Con_NewBuffer: Odd buffer params");

    buf = M_Malloc(sizeof(*buf));

    buf->flags = flags;
    buf->headptr = buf->tailptr = NULL;
    buf->numLines = 0;
    buf->maxLineLen = maxLineLength;
    buf->writebuf = M_Calloc(buf->maxLineLen + 1);
    buf->wbc = 0;
    buf->wbFlags = 0;
    buf->maxLines = maxNumLines;
    if(buf->maxLines != 0) // not unlimited.
    {   // Might as well allocate the index now.
        buf->index = M_Malloc(sizeof(cbline_t*) * buf->maxLines);
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

static void clearBuffer(cbuffer_t *buf, boolean destroy)
{
    cbnode_t   *n, *np;

    // Free the buffer contents.
    n = buf->headptr;
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

    memset(buf->writebuf, 0, buf->maxLineLen + 1);
    buf->wbc = 0;
}

/**
 * Clear the contents of a console history buffer.
 *
 * @param buf               Ptr to the buffer to be cleared.
 */
void Con_BufferClear(cbuffer_t *buf)
{
    if(!buf)
        return;

    clearBuffer(buf, false);
}

/**
 * Destroy an existing console history buffer.
 *
 * @param buf               Ptr to the buffer to be destroyed.
 */
void Con_DestroyBuffer(cbuffer_t *buf)
{
    if(buf)
    {
        cbnode_t *n, *np;

        clearBuffer(buf, true);
        M_Free(buf->writebuf);
        if(buf->index)
            M_Free(buf->index);

        // Free any unused nodes.
        n = buf->unused;
        while(n)
        {
            np = n->next;
            destroyNode(n);
            n = np;
        }

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
void Con_BufferSetMaxLineLength(cbuffer_t *buf, uint length)
{
    if(!buf)
        return;

    buf->maxLineLen = length;

    // The write buffer will be trimmed if resizing smaller.
    buf->writebuf = M_Realloc(buf->writebuf, buf->maxLineLen + 1);
    if(buf->wbc > buf->maxLineLen)
        buf->wbc = buf->maxLineLen;
}

/**
 * @param buf               Ptr to the buffer to be queried.
 *
 * @return                  Number of lines currently in the buffer.
 */
uint Con_BufferNumLines(cbuffer_t *buf)
{
    if(!buf)
        return 0;

    return buf->numLines; // index + 1
}

/**
 * Retrieve the line with the given index from the history buffer.
 *
 * @param buf               Ptr to the buffer to use.
 * @param idx               Index of the line to retrieve.
 *
 * @return                  Ptr to the cbline_t with the requested index or
 *                          <code>NULL</code> if the index was invalid.
 */
cbline_t *Con_BufferGetLine(cbuffer_t *buf, uint idx)
{
    if(!buf)
        return NULL;

    if(buf->numLines == 0 || idx >= buf->numLines)
        return NULL;

    if(!buf->indexGood)
    {   // Rebuild the index.
        uint        i;
        cbnode_t   *node;

        // Do we need to enlarge the index?
        if(buf->indexSize < buf->numLines)
        {
            buf->index =
                M_Realloc(buf->index, sizeof(cbline_t*) * buf->numLines);
            buf->indexSize = buf->numLines;
        }

        i = 0;
        node = buf->headptr;
        while(node != NULL)
        {
            buf->index[i++] = node->data;
            node = node->next;
        }

        buf->indexGood = true;
    }

    return buf->index[idx];
}

/**
 * Flushes the contents of the write buffer to the history buffer.
 *
 * @param buf               Ptr to the history buffer to flush.
 */
void Con_BufferFlush(cbuffer_t *buf)
{
    uint        len;
    cbline_t   *line;

    if(!buf)
        return;

    // Is there anything to flush?
    if(buf->wbc < 1)
        return;

    line = bufferNewLine(buf);

    //
    // Flush the write buffer.
    //
    len = strlen(buf->writebuf);
    if(line->text != NULL)
    {   // We are re-using an existing line so we may not need to
        // reallocate at all.
        uint    exlen = strlen(line->text);

        if(exlen < len)
            line->text = M_Realloc(line->text, len + 1);
        else
            memset(line->text, 0, exlen);
    }
    else
    {
        line->text = M_Malloc(len + 1);
    }

    strcpy(line->text, buf->writebuf);
    line->text[len] = 0;
    line->len = len;
    line->flags = buf->wbFlags;

    // Clear the write buffer.
    memset(buf->writebuf, 0, buf->maxLineLen + 1);
    buf->wbc = 0;
    buf->wbFlags = 0;
}

/**
 * Write the given text string (plus optional flags) to the buffer.
 *
 * @param buf               Ptr to the buffer to write to.
 * @param flags             CBLF_* flags in use for this write.
 * @param txt               Ptr to the text string to be written.
 */
void Con_BufferWrite(cbuffer_t *buf, int flags, char *txt)
{
    uint        i;

    if(!buf)
        return;

    // Check for special write actions first.
    if(flags & CBLF_RULER)
    {
        Con_BufferFlush(buf);

        bufferNewLine(buf)->flags |= CBLF_RULER;
        flags &= ~CBLF_RULER;
    }

    if(!txt || !strcmp(txt, ""))
        return; // Nothing else we can do.

    // Copy the text into the write buffer and flush to the history
    // buffer when as necessary/required.
    for(i = 0; i < strlen(txt); ++i)
    {
        buf->wbFlags = flags;
        if(txt[i] == '\n' || buf->wbc >= buf->maxLineLen)  // A new line?
        {
            Con_BufferFlush(buf);

            // Newlines won't get in the buffer at all.
            if(txt[i] == '\n')
                continue;
        }
        // Copy the next character to the write buffer.
        buf->writebuf[buf->wbc++] = txt[i];
    }

    if(buf->flags & CBF_ALWAYSFLUSH)
    {   // Don't leave data in the write buffer.
        Con_BufferFlush(buf);
    }
}
