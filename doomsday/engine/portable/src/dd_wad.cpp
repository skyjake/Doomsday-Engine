/**
 * @file dd_wad.cpp
 *
 * Wrapper API for accessing data stored in DOOM WAD files.
 *
 * @ingroup resource
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 1993-1996 by id Software, Inc.
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

#include <ctime>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "lumpindex.h"

using namespace de;

#if _DEBUG
#  define  W_Error              Con_Error
#else
#  define  W_Error              Con_Message
#endif

size_t W_LumpLength(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        return App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum).info().size;
    }
    catch(LumpIndex::NotFoundError const&)
    {
        W_Error("W_LumpLength: Invalid lumpnum %i.", absoluteLumpNum);
    }
    return 0;
}

char const* W_LumpName(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        return Str_Text(App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum).name());
    }
    catch(FS1::NotFoundError const&)
    {
        W_Error("W_LumpName: Invalid lumpnum %i.", absoluteLumpNum);
    }
    return "";
}

uint W_LumpLastModified(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        return App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum).info().lastModified;
    }
    catch(LumpIndex::NotFoundError const&)
    {
        W_Error("W_LumpLastModified: Invalid lumpnum %i.", absoluteLumpNum);
    }
    return 0;
}

char const* W_LumpSourceFile(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        de::File1 const& lump = App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum);
        return Str_Text(lump.container().composePath());
    }
    catch(LumpIndex::NotFoundError const&)
    {
        W_Error("W_LumpSourceFile: Invalid lumpnum %i.", absoluteLumpNum);
    }
    return "";
}

boolean W_LumpIsCustom(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        de::File1 const& lump = App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum);
        return lump.container().hasCustom();
    }
    catch(LumpIndex::NotFoundError const&)
    {
        W_Error("W_LumpIsCustom: Invalid lumpnum %i.", absoluteLumpNum);
    }
    return false;
}

lumpnum_t W_CheckLumpNumForName2(char const* name, boolean silent)
{
    lumpnum_t lumpNum;
    if(!name || !name[0])
    {
        if(!silent)
            VERBOSE2( Con_Message("Warning: W_CheckLumpNumForName: Empty name, returning invalid lumpnum.\n") )
        return -1;
    }
    lumpNum = App_FileSystem()->lumpNumForName(name);
    if(!silent && lumpNum < 0)
        VERBOSE2( Con_Message("Warning: W_CheckLumpNumForName: Lump \"%s\" not found.\n", name) )
    return lumpNum;
}

lumpnum_t W_CheckLumpNumForName(char const* name)
{
    return W_CheckLumpNumForName2(name, (verbose < 2));
}

lumpnum_t W_GetLumpNumForName(char const* name)
{
    lumpnum_t lumpNum = W_CheckLumpNumForName(name);
    if(lumpNum < 0)
    {
        W_Error("W_GetLumpNumForName: Lump \"%s\" not found.", name);
    }
    return lumpNum;
}

size_t W_ReadLump(lumpnum_t absoluteLumpNum, uint8_t* buffer)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        de::File1& lump = App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum);
        return lump.read(buffer, 0, lump.size());
    }
    catch(LumpIndex::NotFoundError const&)
    {
        W_Error("W_ReadLump: Invalid lumpnum %i.", absoluteLumpNum);
    }
    return 0;
}

size_t W_ReadLumpSection(lumpnum_t absoluteLumpNum, uint8_t* buffer, size_t startOffset, size_t length)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        de::File1& lump = App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum);
        return lump.read(buffer, startOffset, length);
    }
    catch(LumpIndex::NotFoundError const&)
    {
        W_Error("W_ReadLumpSection: Invalid lumpnum %i.", absoluteLumpNum);
    }
    return 0;
}

uint8_t const* W_CacheLump(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        return App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum).cache();
    }
    catch(LumpIndex::NotFoundError const&)
    {
        W_Error("W_CacheLump: Invalid lumpnum %i.", absoluteLumpNum);
    }
    return NULL;
}

void W_UnlockLump(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum).unlock();
    }
    catch(LumpIndex::NotFoundError const&)
    {
        W_Error("W_UnlockLump: Invalid lumpnum %i.", absoluteLumpNum);
    }
    return;
}
