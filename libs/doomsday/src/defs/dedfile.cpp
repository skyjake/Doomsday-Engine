/** @file dedfile.cpp
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "doomsday/defs/dedfile.h"

#include <de/app.h>
#include <de/folder.h>
#include <de/logbuffer.h>
#include "doomsday/defs/dedparser.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/fs_util.h"

#include <de/c_wrapper.h>

using namespace de;
using namespace res;

static char dedReadError[512];

void DED_SetError(const String &message)
{
    String msg = "Error: " + message + ".";
    strncpy(dedReadError, msg, sizeof(dedReadError));
}

void Def_ReadProcessDED(ded_t *defs, const String& sourcePath)
{
     LOG_AS("Def_ReadProcessDED");

     if (sourcePath.isEmpty()) return;

     // Try FS2 first.
     try
     {
         Block text;
         App::rootFolder().locate<File const>(sourcePath) >> text;
         if (!DED_ReadData(defs, String(text), sourcePath, true/*consider it custom; there is no way to check...*/))
         {
             App_FatalError("Def_ReadProcessDED: %s\n", dedReadError);
         }
        return; // Done!
    }
    catch (...)
    {
        // Try FS1 as fallback.
    }

    res::Uri const uri(sourcePath, RC_NULL);
    if (!App_FileSystem().accessFile(uri))
    {
        LOG_RES_WARNING("\"%s\" not found!") << NativePath(uri.asText()).pretty();
        return;
    }

    // We use the File Ids to prevent loading the same files multiple times.
    if (!App_FileSystem().checkFileId(uri))
    {
        // Already handled.
        LOG_RES_XVERBOSE("\"%s\" has already been read", NativePath(uri.asText()).pretty());
        return;
    }

    if (!DED_Read(defs, sourcePath))
    {
        App_FatalError("Def_ReadProcessDED: %s\n", dedReadError);
    }
}

int DED_ReadLump(ded_t *ded, lumpnum_t lumpNum)
{
    try
    {
        File1 &lump = App_FileSystem().lump(lumpNum);
        if (lump.size() > 0)
        {
            const uint8_t *data = lump.cache();
            String sourcePath = lump.container().composePath();
            bool custom       = (lump.isContained()? lump.container().hasCustom() : lump.hasCustom());
            const int result  = DED_ReadData(ded, (const char *)data, sourcePath, custom);
            lump.unlock();
            return result;
        }
        return true;
    }
    catch (LumpIndex::NotFoundError const&)
    {} // Ignore error.
    DED_SetError("Bad lump number");
    return 0;
}

int DED_Read(ded_t *ded, const String& path)
{
    // Attempt to open a definition file on this path.
    try
    {
        // Relative paths are relative to the native working directory.
        String fullPath = (NativePath::workPath() / NativePath(path).expand()).withSeparators('/');
        std::unique_ptr<FileHandle> hndl(&App_FileSystem().openFile(fullPath, "rb"));

        // We will buffer a local copy of the file. How large a buffer do we need?
        hndl->seek(0, SeekEnd);
        size_t bufferedDefSize = hndl->tell();
        hndl->rewind();
        char *bufferedDef = (char *) M_Calloc(bufferedDefSize + 1);

        File1 &file = hndl->file();
        /// @todo Custom status for contained files is not inherited from the container?
        const bool isCustom = (file.isContained()? file.container().hasCustom() : file.hasCustom());

        // Copy the file into the local buffer and parse definitions.
        hndl->read((uint8_t *)bufferedDef, bufferedDefSize);
        int result = DED_ReadData(ded, bufferedDef, path, isCustom);
        App_FileSystem().releaseFile(file);

        // Done. Release temporary storage and return the result.
        M_Free(bufferedDef);
        return result;
    }
    catch (const FS1::NotFoundError &)
    {} // Ignore.

    DED_SetError("File could not be opened for reading");
    return false;
}

int DED_ReadData(ded_t *ded, const char *buffer, String sourceFile, bool sourceIsCustom)
{
    return DEDParser(ded).parse(buffer, sourceFile, sourceIsCustom);
}

const char *DED_Error()
{
    return dedReadError;
}
