/**\file filelist.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "filelist.h"

struct filelist_s
{
    /// Current number of files.
    int _size;
    /// Current size of the reference table. Not necessarily equal to FileList::_size!
    int _tableSize;
    /// Reference pointer table.
    DFile** _table;
};

static void expandHandleTable(FileList* fl)
{
    assert(fl);
    if(!fl->_table || fl->_size > fl->_tableSize)
    {
        if(!fl->_tableSize)
            fl->_tableSize = 16;
        else
            fl->_tableSize *= 2;

        fl->_table = (DFile**)realloc(fl->_table, fl->_tableSize * sizeof *fl->_table);
        if(!fl->_table)
            Con_Error("FileList::expandHandleTable: Failed on (re)allocation of %lu bytes while "
                "enlarging reference table.", (unsigned long) (fl->_tableSize * sizeof *fl->_table));
    }
}

FileList* FileList_New(void)
{
    FileList* fl = (FileList*)calloc(1, sizeof *fl);
    if(!fl)
        Con_Error("FileList::New: Failed on allocation of %lu bytes for new FileList.",
            (unsigned long) sizeof *fl);
    return fl;
}

FileList* FileList_NewWithFiles(DFile** files, int count)
{
    FileList* fl = FileList_New();
    if(files && count)
    {
        int i;
        for(i = 0; i < count; ++i)
        {
            FileList_AddBack(fl, *(files++));
        }
    }
    return fl;
}

FileList* FileList_NewCopy(FileList* fl)
{
    assert(fl);
    {
    /// \todo Implement using a more performant algorithm.
    FileList* clone = FileList_New();
    int i;
    for(i = 0; i < fl->_size; ++i)
    {
        FileList_AddBack(clone, DFileBuilder_NewCopy(fl->_table[i]));
    }
    return clone;
    }
}

void FileList_Delete(FileList* fl)
{
    assert(fl);
    FileList_Clear(fl);
    if(fl->_table)
    {
        free(fl->_table);
    }
    free(fl);
}

void FileList_Clear(FileList* fl)
{
    if(!FileList_Empty(fl))
    {
        memset(fl->_table, 0, fl->_tableSize * sizeof *fl->_table);
        fl->_size = 0;
    }
}

DFile* FileList_Get(FileList* fl, int idx)
{
    assert(fl);
    {
    DFile* hndl = NULL;
    if(idx < 0) idx += fl->_size;
    if(idx >= 0 && idx < fl->_size)
    {
        hndl = fl->_table[idx];
    }
    return hndl;
    }
}

DFile* FileList_Front(FileList* fl)
{
    return FileList_Get(fl, 0);
}

DFile* FileList_Back(FileList* fl)
{
    return FileList_Get(fl, -1);
}

AbstractFile* FileList_GetFile(FileList* fl, int idx)
{
    DFile* hndl = FileList_Get(fl, idx);
    if(hndl) return DFile_File(hndl);
    return NULL;
}

AbstractFile* FileList_FrontFile(FileList* fl)
{
    DFile* hndl = FileList_Front(fl);
    if(hndl) return DFile_File(hndl);
    return NULL;
}

AbstractFile* FileList_BackFile(FileList* fl)
{
    DFile* hndl = FileList_Back(fl);
    if(hndl) return DFile_File(hndl);
    return NULL;
}

DFile* FileList_RemoveAt(FileList* fl, int idx)
{
    assert(fl);
    {
    DFile* file = NULL;
    if(idx < 0) idx += fl->_size;
    if(idx >= 0 && idx < fl->_size)
    {
        file = fl->_table[idx];
        if(idx < fl->_size-1)
            memmove(fl->_table + idx, fl->_table + idx + 1, (fl->_size - 1) * sizeof *fl->_table);
        --fl->_size;
    }
    return file;
    }
}

DFile* FileList_AddFront(FileList* fl, DFile* file)
{
    assert(fl && file);
    ++fl->_size;
    expandHandleTable(fl);
    memmove(fl->_table, fl->_table + 1, (fl->_size-1) * sizeof *fl->_table);
    DFile_SetList(file, fl);
    return (fl->_table[0] = file);
}

DFile* FileList_AddBack(FileList* fl, DFile* file)
{
    assert(fl && file);
    ++fl->_size;
    expandHandleTable(fl);
    DFile_SetList(file, fl);
    return (fl->_table[fl->_size-1] = file);
}

DFile* FileList_RemoveFront(FileList* fl)
{
    return FileList_RemoveAt(fl, 0);
}

DFile* FileList_RemoveBack(FileList* fl)
{
    return FileList_RemoveAt(fl, -1);
}

int FileList_Size(FileList* fl)
{
    assert(fl);
    return fl->_size;
}

boolean FileList_Empty(FileList* fl)
{
    return FileList_Size(fl) == 0;
}

AbstractFile** FileList_ToArray(FileList* fl, int* count)
{
    assert(fl);
    if(!FileList_Empty(fl))
    {
        AbstractFile** arr = (AbstractFile**)malloc(fl->_size * sizeof *arr);
        if(!arr)
            Con_Error("FileList::ToArray: Failed on allocation of %lu bytes for file list.",
                (unsigned long) (fl->_size * sizeof *arr));
        { int i = 0;
        for(i = 0; i < fl->_size; ++i)
        {
            arr[i] = DFile_File(fl->_table[i]);
        }}
        if(count) *count = fl->_size;
        return arr;
    }

    if(count) *count = 0;
    return NULL;
}

ddstring_t* FileList_ToString4(FileList* fl, int flags, const char* delimiter,
    boolean (*predicate)(DFile* hndl, void* paramaters), void* paramaters)
{
    assert(fl);
    {
    int maxLength, delimiterLength = (delimiter? (int)strlen(delimiter) : 0);
    int i, n, pLength, pathCount = 0;
    const ddstring_t* path;
    ddstring_t* str, buf;
    const char* p, *ext;

    // Do we need a working buffer to process this request?
    /// \todo We can do this without the buffer; implement cleverer algorithm.
    if(flags & PTSF_TRANSFORM_EXCLUDE_DIR)
        Str_Init(&buf);

    // Determine the maximum number of characters we'll need.
    maxLength = 0;
    for(i = 0; i < fl->_size; ++i)
    {
        if(predicate && !predicate(fl->_table[i], paramaters))
            continue; // Caller isn't interested in this...

        // One more path will be composited.
        ++pathCount;
        path = AbstractFile_Path(DFile_File_Const(fl->_table[i]));

        if(!(flags & (PTSF_TRANSFORM_EXCLUDE_DIR|PTSF_TRANSFORM_EXCLUDE_EXT)))
        {
            // Caller wants the whole path plus name and extension (if present).
            maxLength += Str_Length(path);
        }
        else
        {
            if(flags & PTSF_TRANSFORM_EXCLUDE_DIR)
            {
                // Caller does not want the directory hierarchy.
                F_FileNameAndExtension(&buf, Str_Text(path));
                p = Str_Text(&buf);
                pLength = Str_Length(&buf);
            }
            else
            {
                p = Str_Text(path);
                pLength = Str_Length(path);
            }

            if(flags & PTSF_TRANSFORM_EXCLUDE_EXT)
            {
                // Caller does not want the file extension.
                ext = F_FindFileExtension(p);
                if(ext) ext -= 1/*dot separator*/;
            }
            else
            {
                ext = NULL;
            }

            if(ext)
            {
                maxLength += pLength - (pLength - (ext - p));
            }
            else
            {
                maxLength += pLength;
            }
        }

        if(flags & PTSF_QUOTED)
            maxLength += sizeof(char);
    }
    maxLength += pathCount * delimiterLength;

    // Composite final string.
    str = Str_New();
    Str_Reserve(str, maxLength);
    n = 0;
    for(i = 0; i < fl->_size; ++i)
    {
        if(predicate && !predicate(fl->_table[i], paramaters))
            continue; // Caller isn't interested in this...

        path = AbstractFile_Path(DFile_File_Const(fl->_table[i]));

        if(flags & PTSF_QUOTED)
            Str_AppendChar(str, '"');

        if(!(flags & (PTSF_TRANSFORM_EXCLUDE_DIR|PTSF_TRANSFORM_EXCLUDE_EXT)))
        {
            // Caller wants the whole path plus name and extension (if present).
            Str_Append(str, Str_Text(path));
        }
        else
        {
            if(flags & PTSF_TRANSFORM_EXCLUDE_DIR)
            {
                // Caller does not want the directory hierarchy.
                F_FileNameAndExtension(&buf, Str_Text(path));
                p = Str_Text(&buf);
                pLength = Str_Length(&buf);
            }
            else
            {
                p = Str_Text(path);
                pLength = Str_Length(path);
            }

            if(flags & PTSF_TRANSFORM_EXCLUDE_EXT)
            {
                // Caller does not want the file extension.
                ext = F_FindFileExtension(p);
                if(ext) ext -= 1/*dot separator*/;
            }
            else
            {
                ext = NULL;
            }

            if(ext)
            {
                Str_PartAppend(str, p, 0, pLength - (pLength - (ext - p)));
                maxLength += pLength - (pLength - (ext - p));
            }
            else
            {
                Str_Append(str, p);
            }
        }

        if(flags & PTSF_QUOTED)
            Str_AppendChar(str, '"');

        ++n;
        if(n != pathCount)
            Str_Append(str, delimiter);
    }

    if(flags & PTSF_TRANSFORM_EXCLUDE_DIR)
        Str_Free(&buf);

    return str;
    }
}

ddstring_t* FileList_ToString3(FileList* fl, int flags, const char* delimiter)
{
    return FileList_ToString4(fl, flags, delimiter, NULL, NULL);
}

ddstring_t* FileList_ToString2(FileList* fl, int flags)
{
    return FileList_ToString3(fl, flags, " ");
}

ddstring_t* FileList_ToString(FileList* fl)
{
    return FileList_ToString2(fl, DEFAULT_PATHTOSTRINGFLAGS);
}

#if _DEBUG
void FileList_Print(FileList* fl)
{
    DFile* file;
    int i;
    for(i = 0; i < fl->_size; ++i)
    {
        file = fl->_table[i];
        Con_Printf(" %c%u: ", AbstractFile_HasStartup(DFile_File(file))? '*':' ', i);
        DFile_Print(file);
    }
}
#endif
