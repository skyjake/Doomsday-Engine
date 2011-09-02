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

abstractfile_t* F_NewFile(const char* absolutePath)
{
    abstractfile_t* file = (abstractfile_t*)malloc(sizeof(*file));
    if(!file) Con_Error("F_NewFile: Failed on allocation of %lu bytes for new abstractfile_t.", (unsigned long) sizeof(*file));
    AbstractFile_Init(file, FT_UNKNOWNFILE, NULL, absolutePath);
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

size_t F_Length(abstractfile_t* file)
{
    assert(NULL != file);
    {
    size_t currentPosition = F_Seek(file, 0, SEEK_END);
    size_t length = F_Tell(file);
    F_Seek(file, currentPosition, SEEK_SET);
    return length;
    }
}

unsigned int F_LastModified(abstractfile_t* file)
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

abstractfile_t* F_OpenStreamLump(abstractfile_t* file, abstractfile_t* container, int lumpIdx, boolean dontBuffer)
{
    assert(NULL != file && NULL != container);
    {
    const lumpinfo_t* info = F_LumpInfo(container, lumpIdx);
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
        F_ReadLumpSection(container, lumpIdx, (uint8_t*)file->data, 0, info->size);
    }
    return file;
    }
}

abstractfile_t* F_OpenStreamFile(abstractfile_t* file, FILE* hndl, const char* path)
{
    assert(file && hndl && path && path[0]);
    file->hndl = hndl;
    file->data = NULL;
    file->flags.open = true;
    file->lastModified = readLastModified(path);
    return file;
}

void F_Close(abstractfile_t* file)
{
    assert(NULL != file);

    if(!file->flags.open)
        return;

    if(file->hndl)
    {
        fclose(file->hndl);
        file->hndl = NULL;
    }
    else
    {   // Free the stored data.
        if(file->data)
        {
            free(file->data), file->data = NULL;
        }
    }
    file->pos = NULL;
    F_Release(file);
}

void F_Delete(abstractfile_t* file)
{
    assert(NULL != file);
    F_Close(file);
    free(file);
}

size_t F_Read(abstractfile_t* file, uint8_t* buffer, size_t count)
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

boolean F_AtEnd(abstractfile_t* file)
{
    assert(NULL != file);
    return deof(file);
}

unsigned char F_GetC(abstractfile_t* file)
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

size_t F_Tell(abstractfile_t* file)
{
    assert(NULL != file);
    if(!file->flags.open)
        return 0;
    if(file->hndl)
        return (size_t) ftell(file->hndl);
    return file->pos - file->data;
}

size_t F_Seek(abstractfile_t* file, size_t offset, int whence)
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

void F_Rewind(abstractfile_t* file)
{
    F_Seek(file, 0, SEEK_SET);
}
