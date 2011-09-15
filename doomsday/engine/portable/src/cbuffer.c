/**\file cbuffer.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * \note With respect to threading, this code assumes that a cbuffer,
 * mutex's lock/unlock state has been manipulated by the CALLER of
 * private functions declared here. Therefore, public functions must
 * lock before and unlock after calling a private function.
 */

#include <ctype.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_misc.h"

#include "cbuffer.h"

typedef struct cbnode_s {
    cbline_t* data;
    struct cbnode_s* next, *prev;
} cbnode_t;

static void insertNodeAtEnd(cbuffer_t* cb, cbnode_t* newnode);
static void removeNode(cbuffer_t* cb, cbnode_t* node);
static void moveNodeForReuse(cbuffer_t* cb, cbnode_t* node);

static void lock(cbuffer_t* cb, boolean yes)
{
    assert(cb);
    if(yes)
    {
        Sys_Lock(cb->mutex);
        return;
    }
    Sys_Unlock(cb->mutex);
}

/// \notes Also destroys the data object.
static void destroyNode(cbnode_t* node)
{
    assert(node);
    {
    cbline_t* line = node->data;
    if(line->text)
        Z_Free(line->text);
    free(line);
    free(node);
    }
}

static void insertNodeAfter(cbuffer_t* cb, cbnode_t* node, cbnode_t* newnode)
{
    assert(node && newnode);
    newnode->prev = node;
    newnode->next = node->next;
    if(!node->next)
    {
        cb->tail = newnode;
    }
    else
    {
        node->next->prev = newnode;
    }
    node->next = newnode;

    cb->indexGood = false; // it will be updated when needed.
}

static void insertNodeBefore(cbuffer_t* cb, cbnode_t* node, cbnode_t* newnode)
{
    assert(node && newnode);
    newnode->prev = node->prev;
    newnode->next = node;
    if(!node->prev)
    {
        cb->head = newnode;
    }
    else
    {
        node->prev->next = newnode;
    }
    node->prev = newnode;

    cb->indexGood = false; // it will be updated when needed.
}

static void insertNodeAtStart(cbuffer_t* cb, cbnode_t* newnode)
{
    assert(cb && newnode);
    if(!cb->head)
    {
        cb->head = newnode;
        cb->tail = newnode;
        newnode->prev = NULL;
        newnode->next = NULL;

        cb->indexGood = false; // it will be updated when needed.
        return;
    }
    insertNodeBefore(cb, cb->head, newnode);
}

static void insertNodeAtEnd(cbuffer_t* cb, cbnode_t* newnode)
{
    assert(newnode);
    if(!cb->tail)
    {
        insertNodeAtStart(cb, newnode);
        return;
    }
    insertNodeAfter(cb, cb->tail, newnode);
}

static void moveNodeForReuse(cbuffer_t* cb, cbnode_t* node)
{
    cbline_t* line;
    assert(cb && node);

    if(cb->used)
    {
        node->next = cb->used;
    }
    else
    {
        node->next = NULL;
    }
    node->prev = NULL;
    cb->used = node;

    line = node->data;
    line->flags = 0;

    if(line->text)
        memset(line->text, 0, line->len);
}

static void removeNode(cbuffer_t* cb, cbnode_t* node)
{
    assert(cb && node);
    if(!node->prev)
    {
        cb->head = node->next;
    }
    else
    {
        node->prev->next = node->next;
    }

    if(!node->next)
    {
        cb->tail = node->prev;
    }
    else
    {
        node->next->prev = node->prev;
    }

    moveNodeForReuse(cb, node);
    cb->indexGood = false; // it will be updated when needed.
}

cbuffer_t* CBuffer_New(uint maxNumLines, uint maxLineLength, int flags)
{
    char name[32+1];
    cbuffer_t* cb;

    if(maxNumLines < 1 || maxLineLength < 1)
        Con_Error("CBuffer::New: Odd buffer params");

    cb = (cbuffer_t*)malloc(sizeof *cb);
    if(!cb) Con_Error("CBuffer::New: Failed on allocation of %lu bytes for new CBuffer.", (unsigned long) sizeof *cb);

    dd_snprintf(name, 33, "CBufferMutex%p", cb);
    cb->mutex = Sys_CreateMutex(name);

    cb->flags = flags;
    cb->head = cb->tail = NULL;
    cb->used = NULL;

    cb->numLines = 0;
    cb->maxLineLen = maxLineLength;
    cb->writebuf = (char*)calloc(1, cb->maxLineLen+1);
    if(!cb->writebuf) Con_Error("CBuffer::New: Failed on allocation of %lu bytes for write buffer.", (unsigned long) (cb->maxLineLen+1));
    cb->wbc = 0;
    cb->wbFlags = 0;
    cb->maxLines = maxNumLines;

    // Allocate the index now.
    cb->index = (cbnode_t**)malloc(cb->maxLines * sizeof *cb->index);
    if(!cb->index) Con_Error("CBuffer::New: Failed on allocation of %lu bytes for line index.", (unsigned long) (cb->maxLines * sizeof *cb->index));
    cb->indexSize = cb->maxLines;
    cb->indexGood = true; // its empty so...

    return cb;
}

static void clear(cbuffer_t* cb, boolean recycle)
{
    cbnode_t* n, *np;
    assert(cb);

    n = cb->head;
    while(n)
    {
        np = n->next;
        if(!recycle)
        {
            destroyNode(n);
        }
        else
        {
            moveNodeForReuse(cb, n);
        }
        n = np;
    }
    cb->head = cb->tail = NULL;
    cb->numLines = 0;
}

static void clearWriteBuffer(cbuffer_t* cb)
{
    assert(cb);
    memset(cb->writebuf, 0, cb->maxLineLen);
    cb->wbc = 0;
}

static void clearIndex(cbuffer_t* cb)
{
    assert(cb);
    if(!cb->index) return;
    free(cb->index);
}

static void clearUsedNodes(cbuffer_t* cb)
{
    assert(cb);
    { cbnode_t* np, *n = cb->used;
    while(n)
    {
        np = n->next;
        destroyNode(n);
        n = np;
    }}
}

void CBuffer_Clear(cbuffer_t* cb)
{
    lock(cb, true);
    clear(cb, true);
    lock(cb, false);
}

void CBuffer_Delete(cbuffer_t* cb)
{
    assert(cb);

    lock(cb, true);

    clear(cb, false);
    clearWriteBuffer(cb);
    clearIndex(cb);
    clearUsedNodes(cb);

    lock(cb, false);
    Sys_DestroyMutex(cb->mutex);
    cb->mutex = 0;
    free(cb);
}

uint CBuffer_MaxLineLength(cbuffer_t* cb)
{
    assert(cb);
    {
    uint max;
    lock(cb, true);
    max = cb->maxLineLen;
    lock(cb, false);
    return max;
    }
}

void CBuffer_SetMaxLineLength(cbuffer_t* cb, uint length)
{
    assert(cb);

    lock(cb, true);
    cb->maxLineLen = length;

    // The write buffer will be trimmed if resizing smaller.
    cb->writebuf = (char*)realloc(cb->writebuf, cb->maxLineLen+1);
    if(!cb->writebuf) Con_Error("CBuffer::SetMaxLineLength: Failed on reallocation of %lu bytes for new write buffer.", (unsigned long) (cb->maxLineLen+1));
    if(cb->wbc > cb->maxLineLen)
        cb->wbc = cb->maxLineLen;
    lock(cb, false);
}

uint CBuffer_NumLines(cbuffer_t* cb)
{
    uint num;
    assert(cb);
    lock(cb, true);
    num = cb->numLines; // 1-based index.
    lock(cb, false);
    return num;
}

static void expandIndex(cbuffer_t* cb)
{
    assert(cb);
    if(cb->indexSize < cb->numLines)
    {
        cb->index = (cbnode_t**)realloc(cb->index, cb->numLines * sizeof *cb->index);
        if(!cb->index) Con_Error("CBuffer::expandIndex: Failed on (re)allocation of %lu bytes for line index.", cb->numLines * sizeof *cb->index);
        cb->indexSize = cb->numLines;
    }
}

static void rebuildIndex(cbuffer_t* cb)
{
    assert(cb);

    if(cb->indexGood) return;

    expandIndex(cb);

    { uint i = 0;
    cbnode_t* node;
    for(node = cb->head; node; node = node->next)
    {
        cb->index[i++] = node;
    }}

    cb->indexGood = true;
}

static cbnode_t* bufferGetLine(cbuffer_t* cb, uint idx)
{
    cbnode_t* node = NULL;
    assert(cb);
    if(!(cb->numLines == 0 || idx >= cb->numLines))
    {
        rebuildIndex(cb);
        node = cb->index[idx];
    }
    return node;
}

static uint bufferGetLines(cbuffer_t* cb, uint reqCount, int firstIdx,
    cbline_t const** list, int blflags)
{
    assert(cb && list);
    if((long) firstIdx <= (long) cb->numLines)
    {
        uint n, idx, count = reqCount;

        if(firstIdx < 0)
        {
            long other = -((long) firstIdx);
            if(other > (long) cb->numLines)
                firstIdx = 0;
            else
                firstIdx = cb->numLines - other;
        }

        idx = firstIdx;
        if(reqCount == 0 || idx + count > cb->numLines)
            count = cb->numLines - idx;

        // Collect the ptrs.
        n = 0;
        { uint i;
        for(i = 0; i < count; ++i)
        {
            cbnode_t* node = bufferGetLine(cb, idx++);
            const cbline_t* line = node->data;
            if((blflags & BLF_OMIT_RULER) && (line->flags & CBLF_RULER))
                continue;
            if((blflags & BLF_OMIT_EMPTYLINE) && !(line->flags & CBLF_RULER) &&
               (!line->text || line->len == 0))
                continue;
            list[n++] = line;
        }}
        // Terminate.
        list[n] = 0;
        return n;
    }

    if(*list)
        list[0] = 0;
    return 0;
}

uint CBuffer_GetLines2(cbuffer_t* cb, uint reqCount, int firstIdx,
    cbline_t const** list, int blflags)
{
    uint result;
    lock(cb, true);
    result = bufferGetLines(cb, reqCount, firstIdx, list, blflags);
    lock(cb, false);
    return result;
}

uint CBuffer_GetLines(cbuffer_t* cb, uint reqCount, int firstIdx,
    cbline_t const** list)
{
    return CBuffer_GetLines2(cb, reqCount, firstIdx, list, 0);
}

const cbline_t* CBuffer_GetLine(cbuffer_t* cb, uint idx)
{
    const cbline_t* line;
    cbnode_t* node;
    lock(cb, true);
    node = bufferGetLine(cb, idx);
    if(node) line = node->data;
    lock(cb, false);
    return line;
}

static cbline_t* bufferNewLine(cbuffer_t* cb)
{
    cbnode_t* node;
    cbline_t* line;

    assert(cb);

    // Do we have any used nodes we can reuse?
    if(cb->used)
    {
        node = cb->used;
        cb->used = node->next;
        line = node->data;
    }
    else
    {
        // Allocate another line.
        line = (cbline_t*)malloc(sizeof *line);
        if(!line) Con_Error("CBuffer::NewLine: Failed on allocation of %lu bytes for new cbline.", (unsigned long) sizeof *line);
        node = (cbnode_t*)malloc(sizeof *node);
        if(!node) Con_Error("CBuffer::NewLine: Failed on allocation of %lu bytes for new cbline node", (unsigned long) sizeof *node);
        node->data = line;

        line->text = NULL;
        line->len = 0;
    }
    assert(node->data);

    cb->numLines++;

    line->flags = 0;

    // Link it in.
    insertNodeAtEnd(cb, node);

    // Check if there are too many lines.
    if(cb->numLines > cb->maxLines)
    {
        // Drop the earliest.
        removeNode(cb, cb->head);
        cb->numLines--;
    }

    return line;
}

static void bufferFlush(cbuffer_t* cb)
{
    uint len;
    cbline_t* line;

    assert(cb);

    // Is there anything to flush?
    if(cb->wbc < 1)
        return;

    line = bufferNewLine(cb);

    //
    // Flush the write buffer.
    //
    len = cb->wbc;
    if(line->text)
    {
        if(line->len <= len)
        {
            line->len = len + 1;
            line->text = (char*)Z_Realloc(line->text, line->len, PU_APPSTATIC);
            if(!line->text) Con_Error("CBuffer::flush: Failed on reallocation of %lu bytes for text line.", (unsigned long) line->len);
        }
    }
    else
    {
        line->len = len + 1;
        line->text = (char*)Z_Malloc(line->len, PU_APPSTATIC, 0);
        if(!line->text) Con_Error("CBuffer::flush: Failed on allocation of %lu bytes for text line.", (unsigned long) line->len);
    }

    memcpy(line->text, cb->writebuf, len);
    line->text[len] = 0;

    line->flags = cb->wbFlags;

    // Clear the write buffer.
    memset(cb->writebuf, 0, cb->maxLineLen);
    cb->wbc = 0;
    cb->wbFlags = 0;
}

void CBuffer_Flush(cbuffer_t* cb)
{
    lock(cb, true);
    bufferFlush(cb);
    lock(cb, false);
}

void CBuffer_Write(cbuffer_t* cb, int flags, const char* txt)
{
    assert(cb);
    lock(cb, true);

    // Check for special write actions first.
    if(flags & CBLF_RULER)
    {
        bufferFlush(cb);
        bufferNewLine(cb)->flags |= CBLF_RULER;
        flags &= ~CBLF_RULER;
    }

    if(!(!txt || !strcmp(txt, "")))
    {
        size_t i, len = strlen(txt);

        // Copy the text into the write buffer and flush to the history
        // buffer when as necessary/required.
        for(i = 0; i < len; ++i)
        {
            cb->wbFlags = flags;
            if(txt[i] == '\n' || cb->wbc >= cb->maxLineLen) // A new line?
            {
                bufferFlush(cb);

                // Newlines won't get in the buffer at all.
                if(txt[i] == '\n')
                    continue;
            }
            // Copy the next character to the write buffer.
            cb->writebuf[cb->wbc++] = txt[i];
        }

        if(cb->flags & CBF_ALWAYSFLUSH)
        {   // Don't leave data in the write buffer.
            bufferFlush(cb);
        }
    }
    lock(cb, false);
}
