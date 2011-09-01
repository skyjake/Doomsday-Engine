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

#define deof(file)          ((file)->flags.eof != 0)

struct dfile_s
{
    struct DFILE_flags_s {
        unsigned char open:1;
        unsigned char eof:1;
    } flags;
    size_t size;
    FILE* hndl;
    uint8_t* data;
    uint8_t* pos;
    unsigned int lastModified;
};

DFILE* F_NewFile(void)
{
    DFILE* file = (DFILE*)calloc(1, sizeof(*file));
    if(!file) Con_Error("F_NewFile: Failed on allocation of %lu bytes for new DFILE.", (unsigned long) sizeof(*file));
    return file;
}

int F_MatchFileName(const char* string, const char* pattern)
{
    const char* in = string, *st = pattern;

    while(*in)
    {
        if(*st == '*')
        {
            st++;
            continue;
        }

        if(*st != '?' && (tolower((unsigned char) *st) != tolower((unsigned char) *in)))
        {
            // A mismatch. Hmm. Go back to a previous '*'.
            while(st >= pattern && *st != '*')
                st--;
            if(st < pattern)
                return false; // No match!
            // The asterisk lets us continue.
        }

        // This character of the pattern is OK.
        st++;
        in++;
    }

    // Match is good if the end of the pattern was reached.
    while(*st == '*')
        st++; // Skip remaining asterisks.

    return *st == 0;
}

int F_Access(const char* path)
{
    // Open for reading, but don't buffer anything.
    DFILE* file = F_Open(path, "rx");
    if(file)
    {
        F_Close(file);
        return true;
    }
    return false;
}

size_t F_Length(DFILE* file)
{
    assert(NULL != file);
    {
    size_t currentPosition = F_Seek(file, 0, SEEK_END);
    size_t length = F_Tell(file);
    F_Seek(file, currentPosition, SEEK_SET);
    return length;
    }
}

unsigned int F_LastModified(DFILE* file)
{
    assert(NULL != file);
    return file->lastModified;
}

/// \note This only works on real files.
static unsigned int readLastModified(const char* path)
{
#ifdef UNIX
    struct stat s;
    stat(path, &s);
    return s.st_mtime;
#endif

#ifdef WIN32
    struct _stat s;
    _stat(path, &s);
    return s.st_mtime;
#endif
}

DFILE* F_OpenStreamLump(DFILE* file, abstractfile_t* fsObject, int lumpIdx, boolean dontBuffer)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = F_LumpInfo(fsObject, lumpIdx);
    assert(info);

    // Init and load in the lump data.
    file->flags.open = true;
    file->hndl = NULL;
    file->lastModified = info->lastModified;
    if(!dontBuffer)
    {
        file->size = info->size;
        file->pos = file->data = (uint8_t*)malloc(file->size);
        if(NULL == file->data)
            Con_Error("F_OpenStreamLump: Failed on allocation of %lu bytes for buffered data.",
                (unsigned long) file->size);
#if _DEBUG
        VERBOSE2( Con_Printf("Next FILE read from F_OpenStreamLump.\n") )
#endif
        F_ReadLumpSection(fsObject, lumpIdx, (uint8_t*)file->data, 0, info->size);
    }
    return file;
    }
}

DFILE* F_OpenStreamFile(DFILE* file, FILE* hndl, const char* path)
{
    assert(file && hndl && path && path[0]);
    file->hndl = hndl;
    file->data = NULL;
    file->flags.open = true;
    file->lastModified = readLastModified(path);
    return file;
}

void F_Close(DFILE* file)
{
    assert(NULL != file);

    if(!file->flags.open)
        return;

    if(file->hndl)
    {
        fclose(file->hndl);
    }
    else
    {   // Free the stored data.
        if(file->data)
            free(file->data);
    }
    memset(file, 0, sizeof(*file));

    F_Release(file);
}

size_t F_Read(DFILE* file, uint8_t* buffer, size_t count)
{
    assert(NULL != file);
    {
    size_t bytesleft;

    if(!file->flags.open)
        return 0;

    if(file->hndl)
    {   // Normal file.
        count = fread(buffer, 1, count, file->hndl);
        if(feof(file->hndl))
            file->flags.eof = true;
        return count;
    }

    // Is there enough room in the file?
    bytesleft = file->size - (file->pos - file->data);
    if(count > bytesleft)
    {
        count = bytesleft;
        file->flags.eof = true;
    }

    if(count)
    {
        memcpy(buffer, file->pos, count);
        file->pos += count;
    }

    return count;
    }
}

boolean F_AtEnd(DFILE* file)
{
    assert(NULL != file);
    return deof(file);
}

unsigned char F_GetC(DFILE* file)
{
    assert(NULL != file);
    if(file->flags.open)
    {
        unsigned char ch = 0;
        F_Read(file, (uint8_t*)&ch, 1);
        return ch;
    }
    return 0;
}

size_t F_Tell(DFILE* file)
{
    assert(NULL != file);
    if(!file->flags.open)
        return 0;
    if(file->hndl)
        return (size_t) ftell(file->hndl);
    return file->pos - file->data;
}

size_t F_Seek(DFILE* file, size_t offset, int whence)
{
    assert(NULL != file);
    {
    size_t oldpos = F_Tell(file);

    if(!file->flags.open)
        return 0;

    file->flags.eof = false;
    if(file->hndl)
    {
        fseek(file->hndl, (long) offset, whence);
    }
    else
    {
        if(whence == SEEK_SET)
            file->pos = file->data + offset;
        else if(whence == SEEK_END)
            file->pos = file->data + (file->size + offset);
        else if(whence == SEEK_CUR)
            file->pos += offset;
    }

    return oldpos;
    }
}

void F_Rewind(DFILE* file)
{
    F_Seek(file, 0, SEEK_SET);
}
