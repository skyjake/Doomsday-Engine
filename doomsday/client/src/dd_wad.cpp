/** @file dd_wad.cpp
 *
 * Wrapper API for accessing data stored in DOOM WAD files.
 *
 * @ingroup resource
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 1993-1996 by id Software, Inc.
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

#define DENG_NO_API_MACROS_WAD

#include <ctime>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "api_wad.h"

using namespace de;

#undef W_LumpLength
size_t W_LumpLength(lumpnum_t lumpNum)
{
    try
    {
        return App_FileSystem().lump(lumpNum).info().size;
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_LumpLength");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return 0;
}

#undef W_LumpName
AutoStr *W_LumpName(lumpnum_t lumpNum)
{
    try
    {
        return AutoStr_FromTextStd(App_FileSystem().lump(lumpNum).name().toUtf8().constData());
    }
    catch(FS1::NotFoundError const &er)
    {
        LOG_AS("W_LumpName");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return AutoStr_NewStd();
}

#undef W_LumpLastModified
uint W_LumpLastModified(lumpnum_t lumpNum)
{
    try
    {
        return App_FileSystem().lump(lumpNum).info().lastModified;
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_LumpLastModified");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return 0;
}

#undef W_LumpSourceFile
AutoStr *W_LumpSourceFile(lumpnum_t lumpNum)
{
    try
    {
        de::File1 const &container = App_FileSystem().lump(lumpNum).container();
        return AutoStr_FromText(container.composePath().toUtf8().constData());
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_LumpSourceFile");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return AutoStr_NewStd();
}

#undef W_LumpIsCustom
dd_bool W_LumpIsCustom(lumpnum_t lumpNum)
{
    try
    {
        return App_FileSystem().lump(lumpNum).container().hasCustom();
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_LumpIsCustom");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return false;
}

#undef W_CheckLumpNumForName
lumpnum_t W_CheckLumpNumForName(char const *name)
{
    if(!name || !name[0])
    {
        LOGDEV_RES_WARNING("W_CheckLumpNumForName: Empty lump name, returning invalid lumpnum");
        return -1;
    }
    return App_FileSystem().lumpNumForName(name);
}

#undef W_GetLumpNumForName
lumpnum_t W_GetLumpNumForName(char const *name)
{
    lumpnum_t lumpNum = W_CheckLumpNumForName(name);
    if(lumpNum < 0)
    {
        LOG_RES_ERROR("Lump \"%s\" not found") << name;
    }
    return lumpNum;
}

#undef W_ReadLump
size_t W_ReadLump(lumpnum_t lumpNum, uint8_t *buffer)
{
    try
    {
        de::File1 &lump = App_FileSystem().lump(lumpNum);
        return lump.read(buffer, 0, lump.size());
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_ReadLump");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return 0;
}

#undef W_ReadLumpSection
size_t W_ReadLumpSection(lumpnum_t lumpNum, uint8_t *buffer, size_t startOffset, size_t length)
{
    try
    {
        return App_FileSystem().lump(lumpNum).read(buffer, startOffset, length);
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_ReadLumpSection");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return 0;
}

#undef W_CacheLump
uint8_t const *W_CacheLump(lumpnum_t lumpNum)
{
    try
    {
        return App_FileSystem().lump(lumpNum).cache();
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_CacheLump");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return NULL;
}

#undef W_UnlockLump
void W_UnlockLump(lumpnum_t lumpNum)
{
    try
    {
        App_FileSystem().lump(lumpNum).unlock();
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_UnlockLump");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return;
}

// Public API:
DENG_DECLARE_API(W) =
{
    { DE_API_WAD },
    W_LumpLength,
    W_LumpName,
    W_LumpLastModified,
    W_LumpSourceFile,
    W_LumpIsCustom,
    W_CheckLumpNumForName,
    W_GetLumpNumForName,
    W_ReadLump,
    W_ReadLumpSection,
    W_CacheLump,
    W_UnlockLump
};
