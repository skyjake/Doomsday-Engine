/** @file readfile.cpp  Legacy file reading utility routines.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/filesys/readfile.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#if defined(WIN32)
#  include <direct.h>
#  include <io.h>
#  include <conio.h>
#endif

#if defined(UNIX)
#  include <unistd.h>
#  include <string.h>
#endif

#ifndef O_BINARY
#  define O_BINARY 0
#endif

#include "doomsday/filesys/fs_main.h"
#include "lzss.h"

#include <de/app.h>
#include <de/legacy/str.h>
#include <de/legacy/memoryzone.h>
#include <cstdlib>
#include <cctype>

#if defined(WIN32)
// Must be defined after includes.
#  define open _open
#endif

using namespace de;
using namespace res;

static size_t FileReader(char const* name, char** buffer);

size_t M_ReadFile(const char* name, char** buffer)
{
    return FileReader(name, buffer);
}

AutoStr *M_ReadFileIntoString(const ddstring_t *path, dd_bool *isCustom)
{
    if (isCustom) *isCustom = false;

    if (Str_StartsWith(path, "LumpIndex:"))
    {
        bool isNumber;
        const lumpnum_t lumpNum    = String(Str_Text(path) + 10).toInt(&isNumber);
        const LumpIndex &lumpIndex = App_FileSystem().nameIndex();
        if (isNumber && lumpIndex.hasLump(lumpNum))
        {
            File1 &lump = lumpIndex.lump(lumpNum);
            if (isCustom)
            {
                /// @todo Custom status for contained files is not inherited from the container?
                *isCustom = (lump.isContained()? lump.container().hasCustom() : lump.hasCustom());
            }

            // Ignore zero-length lumps.
            if (!lump.size()) return 0;

            // Ensure the resulting string is terminated.
            AutoStr *string = Str_PartAppend(AutoStr_NewStd(), (const char *)lump.cache(), 0, lump.size());
            lump.unlock();

            if (Str_IsEmpty(string))
                return 0;

            return string;
        }

        return 0;
    }

    if (Str_StartsWith(path, "Lumps:"))
    {
        const char *lumpName       = Str_Text(path) + 6;
        const LumpIndex &lumpIndex = App_FileSystem().nameIndex();
        if (!lumpIndex.contains(String(lumpName) + ".lmp"))
            return 0;

        File1 &lump = lumpIndex[lumpIndex.findLast(String(lumpName) + ".lmp")];
        if (isCustom)
        {
            /// @todo Custom status for contained files is not inherited from the container?
            *isCustom = (lump.isContained()? lump.container().hasCustom() : lump.hasCustom());
        }

        // Ignore zero-length lumps.
        if (!lump.size()) return 0;

        // Ensure the resulting string is terminated.
        AutoStr *string = Str_PartAppend(AutoStr_NewStd(), (const char *)lump.cache(), 0, lump.size());
        lump.unlock();

        if (Str_IsEmpty(string))
            return 0;

        return string;
    }

    // Try the virtual file system.
    try
    {
        std::unique_ptr<FileHandle> hndl(&App_FileSystem().openFile(Str_Text(path), "rb"));

        if (isCustom)
        {
            /// @todo Custom status for contained files is not inherited from the container?
            File1 &file = hndl->file();
            *isCustom = (file.isContained()? file.container().hasCustom() : file.hasCustom());
        }

        // Ignore zero-length lumps.
        AutoStr *string = nullptr;
        if (size_t lumpLength = hndl->length())
        {
            // Read in the whole thing and ensure the resulting string is terminated.
            Block buffer;
            buffer.resize(lumpLength);
            hndl->read((uint8_t *)buffer.data(), lumpLength);
            string = Str_PartAppend(AutoStr_NewStd(), buffer.c_str(), 0, lumpLength);
        }

        App_FileSystem().releaseFile(hndl->file());

        if (!string || Str_IsEmpty(string))
            return 0;

        return string;
    }
    catch (const FS1::NotFoundError &)
    {} // Ignore this error.


    // Perhaps a local file known to the native file system?
    char *readBuf = 0;
    if (size_t bytesRead = M_ReadFile(Str_Text(path), &readBuf))
    {
        // Ensure the resulting string is terminated.
        AutoStr *string = Str_PartAppend(AutoStr_New(), readBuf, 0, int(bytesRead));
        Z_Free(readBuf);

        // Ignore zero-length files.
        if (Str_IsEmpty(string))
            return 0;

        return string;
    }

    return 0;
}

#if defined(WIN32)
#  define close _close
#  define read  _read
#  define write _write
#endif

static size_t FileReader(const char* name, char** buffer)
{
    struct stat fileinfo;
    char* buf = NULL;
    size_t length = 0;
    int handle;

    LOG_AS("FileReader");

    // First try with LZSS.
    LZFILE *file = lzOpen((char*) name, "rp");

    if (NULL != file)
    {
#define BSIZE 1024

        char readBuf[BSIZE];

        // Read 1kb pieces until file ends.
        while (!lzEOF(file))
        {
            size_t bytesRead = lzRead(readBuf, BSIZE, file);
            char* newBuf;

            // Allocate more memory.
            newBuf = (char*) Z_Malloc(length + bytesRead, PU_APPSTATIC, 0);
            if (buf != NULL)
            {
                memcpy(newBuf, buf, length);
                Z_Free(buf);
            }
            buf = newBuf;

            // Copy new data to buffer.
            memcpy(buf + length, readBuf, bytesRead);
            length += bytesRead;
        }

        lzClose(file);
        *buffer = (char*)buf;
        return length;

#undef BSIZE
    }

    handle = open(name, O_RDONLY | O_BINARY, 0666);
    if (handle == -1)
    {
        LOG_RES_WARNING("Failed opening \"%s\" for reading") << name;
        return length;
    }

    if (-1 == fstat(handle, &fileinfo))
    {
        LOG_RES_ERROR("Couldn't read file \"%s\"") << name;
        return 0;
    }

    length = fileinfo.st_size;
    if (!length)
    {
        *buffer = 0;
        return 0;
    }

    buf = (char *) Z_Malloc(length, PU_APPSTATIC, 0);
    DE_ASSERT(buf != 0);

    size_t bytesRead = read(handle, buf, length);
    close(handle);
    if (bytesRead < length)
    {
        LOG_RES_ERROR("Couldn't read file \"%s\"") << name;
    }
    *buffer = buf;

    return length;
}

#if defined(WIN32)
#  undef close
#  undef read
#  undef write
#  undef open
#endif
