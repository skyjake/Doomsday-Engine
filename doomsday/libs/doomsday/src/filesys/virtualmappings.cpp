/** @file virtualmappings.cpp  Maps WAD lumps and native files to virtual FS1 files.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/fs_util.h"
#include "doomsday/filesys/lumpindex.h"
#include "doomsday/filesys/virtualmappings.h"
#include "doomsday/doomsdayapp.h"

#include <de/legacy/memory.h>
#include <de/c_wrapper.h>

#ifdef WIN32
#  define strupr _strupr
#endif

using namespace de;
using namespace res;

void FS_InitVirtualPathMappings()
{
    App_FileSystem().clearPathMappings();

    if (DoomsdayApp::app().isShuttingDown()) return;

    // Create virtual directory mappings by processing all -vdmap options.
    dint argC = CommandLine_Count();
    for (dint i = 0; i < argC; ++i)
    {
        if (iCmpStrNCase("-vdmap", CommandLine_At(i), 6))
        {
            continue;
        }

        if (i < argC - 1 && !CommandLine_IsOption(i + 1) && !CommandLine_IsOption(i + 2))
        {
            String source      = NativePath(CommandLine_PathAt(i + 1)).expand().withSeparators('/');
            String destination = NativePath(CommandLine_PathAt(i + 2)).expand().withSeparators('/');
            App_FileSystem().addPathMapping(source, destination);
            i += 2;
        }
    }
}

/// Skip all whitespace except newlines.
static inline const char *skipSpace(const char *ptr)
{
    DE_ASSERT(ptr != 0);
    while (*ptr && *ptr != '\n' && isspace(*ptr))
    { ptr++; }
    return ptr;
}

static bool parsePathLumpMapping(char lumpName[9/*LUMPNAME_T_MAXLEN*/], ddstring_t *path, const char *buffer)
{
    DE_ASSERT(lumpName != 0 && path != 0);

    // Find the start of the lump name.
    const char *ptr = skipSpace(buffer);

    // Just whitespace?
    if (!*ptr || *ptr == '\n') return false;

    // Find the end of the lump name.
    const char *end = M_FindWhite(ptr);
    if (!*end || *end == '\n') return false;

    size_t len = end - ptr;
    // Invalid lump name?
    if (len > 8) return false;

    memset(lumpName, 0, 9/*LUMPNAME_T_MAXLEN*/);
    strncpy(lumpName, ptr, len);
    strupr(lumpName);

    // Find the start of the file path.
    ptr = skipSpace(end);
    if (!*ptr || *ptr == '\n') return false; // Missing file path.

    // We're at the file path.
    Str_Set(path, ptr);
    // Get rid of any extra whitespace on the end.
    Str_StripRight(path);
    F_FixSlashes(path, path);
    return true;
}

/**
 * <pre> LUMPNAM0 \\Path\\In\\The\\Base.ext
 * LUMPNAM1 Path\\In\\The\\RuntimeDir.ext
 *  :</pre>
 */
static bool parsePathLumpMappings(const char *buffer)
{
    DE_ASSERT(buffer != 0);

    bool successful = false;
    ddstring_t path; Str_Init(&path);
    ddstring_t line; Str_Init(&line);

    const char *ch = buffer;
    char lumpName[9/*LUMPNAME_T_MAXLEN*/];
    do
    {
        ch = Str_GetLine(&line, ch);
        if (!parsePathLumpMapping(lumpName, &path, Str_Text(&line)))
        {
            // Failure parsing the mapping.
            // Ignore errors in individual mappings and continue parsing.
            //goto parseEnded;
        }
        else
        {
            String destination = NativePath(Str_Text(&path)).expand().withSeparators('/');
            App_FileSystem().addPathLumpMapping(lumpName, destination);
        }
    } while (*ch);

    // Success.
    successful = true;

//parseEnded:
    Str_Free(&line);
    Str_Free(&path);
    return successful;
}

void FS_InitPathLumpMappings()
{
    // Free old paths, if any.
    App_FileSystem().clearPathLumpMappings();

    if (DoomsdayApp::app().isShuttingDown()) return;

    size_t bufSize = 0;
    uint8_t *buf = 0;

    // Add the contents of all DD_DIREC lumps.
    /// @todo fixme: Enforce scope to the containing package!
    const LumpIndex &lumpIndex = App_FileSystem().nameIndex();
    LumpIndex::FoundIndices foundDirecs;
    lumpIndex.findAll("DD_DIREC.lmp", foundDirecs);
    DE_FOR_EACH_CONST(LumpIndex::FoundIndices, i, foundDirecs) // in load order
    {
        File1 &lump = lumpIndex[*i];
        const FileInfo &lumpInfo = lump.info();

        // Make a copy of it so we can ensure it ends in a null.
        if (!buf || bufSize < lumpInfo.size + 1)
        {
            bufSize = lumpInfo.size + 1;
            buf = (uint8_t *) M_Realloc(buf, bufSize);
        }

        lump.read(buf, 0, lumpInfo.size);
        buf[lumpInfo.size] = 0;
        parsePathLumpMappings(reinterpret_cast<const char *>(buf));
    }

    M_Free(buf);
}
