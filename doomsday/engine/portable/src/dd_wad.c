/**\file dd_wad.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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

#include <time.h>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#if _DEBUG
#  define  W_Error              Con_Error
#else
#  define  W_Error              Con_Message
#endif

size_t W_LumpLength(lumpnum_t absoluteLumpNum)
{
    const LumpInfo* info = F_FindInfoForLumpNum(absoluteLumpNum);
    if(!info)
    {
        W_Error("W_LumpLength: Invalid lumpnum %i.", absoluteLumpNum);
        return 0;
    }
    return info->size;
}

const char* W_LumpName(lumpnum_t absoluteLumpNum)
{
    const char* lumpName = F_LumpName(absoluteLumpNum);
    if(!lumpName[0])
    {
        W_Error("W_LumpName: Invalid lumpnum %i.", absoluteLumpNum);
        return NULL;
    }
    return lumpName;
}

uint W_LumpLastModified(lumpnum_t absoluteLumpNum)
{
    const LumpInfo* info = F_FindInfoForLumpNum(absoluteLumpNum);
    if(!info)
    {
        W_Error("W_LumpLastModified: Invalid lumpnum %i.", absoluteLumpNum);
        return (uint)time(NULL);
    }
    return info->lastModified;
}

const char* W_LumpSourceFile(lumpnum_t absoluteLumpNum)
{
    abstractfile_t* fsObject = F_FindFileForLumpNum(absoluteLumpNum);
    if(!fsObject)
    {
        W_Error("W_LumpSourceFile: Invalid lumpnum %i.", absoluteLumpNum);
        return "";
    }
    return Str_Text(AbstractFile_Path(fsObject));
}

boolean W_LumpIsCustom(lumpnum_t absoluteLumpNum)
{
    if(!F_IsValidLumpNum(absoluteLumpNum))
    {
        W_Error("W_LumpIsCustom: Invalid lumpnum %i.", absoluteLumpNum);
        return false;
    }
    return F_LumpIsCustom(absoluteLumpNum);
}

lumpnum_t W_CheckLumpNumForName2(const char* name, boolean silent)
{
    lumpnum_t lumpNum;
    if(!name || !name[0])
    {
        if(!silent)
            VERBOSE2( Con_Message("Warning:W_CheckLumpNumForName: Empty name, returning invalid lumpnum.\n") )
        return -1;
    }
    lumpNum = F_CheckLumpNumForName2(name, true);
    if(!silent && lumpNum < 0)
        VERBOSE2( Con_Message("Warning:W_CheckLumpNumForName: Lump \"%s\" not found.\n", name) )
    return lumpNum;
}

lumpnum_t W_CheckLumpNumForName(const char* name)
{
    return W_CheckLumpNumForName2(name, (verbose < 2));
}

lumpnum_t W_GetLumpNumForName(const char* name)
{
    lumpnum_t lumpNum = W_CheckLumpNumForName(name);
    if(lumpNum < 0)
    {
        W_Error("W_GetLumpNumForName: Lump \"%s\" not found.", name);
    }
    return lumpNum;
}

size_t W_ReadLumpSection(lumpnum_t absoluteLumpNum, uint8_t* buffer, size_t startOffset, size_t length)
{
    int lumpIdx;
    abstractfile_t* fsObject = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
    if(!fsObject)
    {
        W_Error("W_ReadLumpSection: Invalid lumpnum %i.", absoluteLumpNum);
        return 0;
    }
    return F_ReadLumpSection(fsObject, lumpIdx, buffer, startOffset, length);
}

size_t W_ReadLump(lumpnum_t absoluteLumpNum, uint8_t* buffer)
{
    int lumpIdx;
    abstractfile_t* fsObject = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
    if(!fsObject)
    {
        W_Error("W_ReadLump: Invalid lumpnum %i.", absoluteLumpNum);
        return 0;
    }
    return F_ReadLumpSection(fsObject, lumpIdx, buffer, 0, F_LumpLength(absoluteLumpNum));
}

const uint8_t* W_CacheLump(lumpnum_t absoluteLumpNum, int tag)
{
    int lumpIdx;
    abstractfile_t* fsObject = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
    if(!fsObject)
    {
        W_Error("W_CacheLump: Invalid lumpnum %i.", absoluteLumpNum);
        return NULL;
    }
    return F_CacheLump(fsObject, lumpIdx, tag);
}

void W_CacheChangeTag(lumpnum_t absoluteLumpNum, int tag)
{
    int lumpIdx;
    abstractfile_t* fsObject = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
    if(!fsObject)
    {
        W_Error("W_ChacheChangeTag: Invalid lumpnum %i.", absoluteLumpNum);
        return;
    }
    F_CacheChangeTag(fsObject, lumpIdx, tag);
}
