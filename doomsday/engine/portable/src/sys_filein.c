/**\file sys_filein.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2010 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#define deof(hndl)              ((hndl)->flags.eof != 0)

size_t F_Length(DFILE* hndl)
{
    assert(NULL != hndl);
    {
    size_t currentPosition = F_Seek(hndl, 0, SEEK_END);
    size_t length = F_Tell(hndl);
    F_Seek(hndl, currentPosition, SEEK_SET);
    return length;
    }
}

size_t F_Read(DFILE* hndl, uint8_t* buffer, size_t count)
{
    assert(NULL != hndl);
    {
    size_t bytesleft;

    if(!hndl->flags.open)
        return 0;

    if(hndl->hndl)
    {   // Normal file.
        count = fread(buffer, 1, count, hndl->hndl);
        if(feof(hndl->hndl))
            hndl->flags.eof = true;
        return count;
    }

    // Is there enough room in the file?
    bytesleft = hndl->size - (hndl->pos - hndl->data);
    if(count > bytesleft)
    {
        count = bytesleft;
        hndl->flags.eof = true;
    }

    if(count)
    {
        memcpy(buffer, hndl->pos, count);
        hndl->pos += count;
    }

    return count;
    }
}

boolean F_AtEnd(DFILE* hndl)
{
    assert(NULL != hndl);
    return deof(hndl);
}

unsigned char F_GetC(DFILE* hndl)
{
    assert(NULL != hndl);
    if(hndl->flags.open)
    {
        unsigned char ch = 0;
        F_Read(hndl, (uint8_t*)&ch, 1);
        return ch;
    }
    return 0;
}

size_t F_Tell(DFILE* hndl)
{
    assert(NULL != hndl);
    if(!hndl->flags.open)
        return 0;
    if(hndl->hndl)
        return (size_t) ftell(hndl->hndl);
    return hndl->pos - hndl->data;
}

size_t F_Seek(DFILE* hndl, size_t offset, int whence)
{
    assert(NULL != hndl);
    {
    size_t oldpos = F_Tell(hndl);

    if(!hndl->flags.open)
        return 0;

    hndl->flags.eof = false;
    if(hndl->hndl)
    {
        fseek(hndl->hndl, (long) offset, whence);
    }
    else
    {
        if(whence == SEEK_SET)
            hndl->pos = hndl->data + offset;
        else if(whence == SEEK_END)
            hndl->pos = hndl->data + (hndl->size + offset);
        else if(whence == SEEK_CUR)
            hndl->pos += offset;
    }

    return oldpos;
    }
}

void F_Rewind(DFILE* hndl)
{
    F_Seek(hndl, 0, SEEK_SET);
}
