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

size_t F_Length(streamfile_t* sf)
{
    assert(NULL != sf);
    {
    size_t currentPosition = F_Seek(sf, 0, SEEK_END);
    size_t length = F_Tell(sf);
    F_Seek(sf, currentPosition, SEEK_SET);
    return length;
    }
}

size_t F_Read(streamfile_t* sf, uint8_t* buffer, size_t count)
{
    assert(NULL != sf);
    {
    size_t bytesleft;

    if(sf->hndl)
    {   // Normal file.
        count = fread(buffer, 1, count, sf->hndl);
        if(feof(sf->hndl))
            sf->eof = true;
        return count;
    }

    // Is there enough room in the file?
    bytesleft = sf->size - (sf->pos - sf->data);
    if(count > bytesleft)
    {
        count = bytesleft;
        sf->eof = true;
    }

    if(count)
    {
        memcpy(buffer, sf->pos, count);
        sf->pos += count;
    }

    return count;
    }
}

boolean F_AtEnd(streamfile_t* sf)
{
    assert(NULL != sf);
    return sf->eof;
}

unsigned char F_GetC(streamfile_t* sf)
{
    assert(NULL != sf);
    {
    unsigned char ch = 0;
    F_Read(sf, (uint8_t*)&ch, 1);
    return ch;
    }
}

size_t F_Tell(streamfile_t* sf)
{
    assert(NULL != sf);
    if(sf->hndl)
        return (size_t) ftell(sf->hndl);
    return sf->pos - sf->data;
}

size_t F_Seek(streamfile_t* sf, size_t offset, int whence)
{
    assert(NULL != sf);
    {
    size_t oldpos = F_Tell(sf);

    sf->eof = false;
    if(sf->hndl)
    {
        fseek(sf->hndl, (long) offset, whence);
    }
    else
    {
        if(whence == SEEK_SET)
            sf->pos = sf->data + offset;
        else if(whence == SEEK_END)
            sf->pos = sf->data + (sf->size + offset);
        else if(whence == SEEK_CUR)
            sf->pos += offset;
    }

    return oldpos;
    }
}

void F_Rewind(streamfile_t* sf)
{
    F_Seek(sf, 0, SEEK_SET);
}
