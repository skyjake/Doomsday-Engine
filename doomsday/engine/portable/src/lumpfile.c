/**\file lumpfile.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"

#include "lumpdirectory.h"
#include "lumpfile.h"

lumpfile_t* LumpFile_Construct(DFILE* handle, const char* absolutePath,
    lumpdirectory_t* directory, lumpname_t lumpName, size_t lumpSize)
{
    assert(NULL != handle && NULL != directory);
    {
    lumpfile_t* file = (lumpfile_t*)malloc(sizeof(*file));
    if(NULL == file)
        Con_Error("LumpFile::Construct:: Failed on allocation of %lu bytes for new LumpFile.",
            (unsigned long) sizeof(*file));

    if(NULL != lumpName)
        memcpy(file->_info.name, lumpName, sizeof(file->_info.name));
    else
        memset(file->_info.name, 0, sizeof(file->_info.name));
    Str_Init(&file->_info.path);
    if(NULL != absolutePath)
        Str_Set(&file->_info.path, absolutePath);
    file->_info.size = file->_info.compressedSize = lumpSize;
    file->_info.baseOffset = 0;
    file->_info.lastModified = 0; /// \fixme Get real value.
    file->_cacheData = NULL;
    AbstractFile_Init((abstractfile_t*)file, FT_LUMPFILE, handle, absolutePath, directory);

    return file;
    }
}

void LumpFile_Destruct(lumpfile_t* file)
{
    assert(NULL != file);
    LumpFile_ClearLumpCache(file);
    Str_Free(&file->_base._absolutePath);
    free(file);
}

void LumpFile_ClearLumpCache(lumpfile_t* file)
{
    assert(NULL != file);
    if(file->_cacheData)
    {
        // If the block has a user, it must be explicitly freed.
        if(Z_GetTag(file->_cacheData) < PU_MAP)
            Z_ChangeTag(file->_cacheData, PU_MAP);
        // Mark the memory pointer in use, but unowned.
        Z_ChangeUser(file->_cacheData, (void*) 0x2);
    }
}

void LumpFile_ReadLumpSection2(lumpfile_t* file, lumpnum_t lumpNum, char* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = LumpDirectory_LumpInfo(file->_base._directory, lumpNum);
    size_t readBytes;

    assert(NULL != info);

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache && NULL != file->_cacheData)
    {
        boolean isCached = (NULL != file->_cacheData);
        void** cachePtr = (void**)&file->_cacheData;
        if(isCached)
        {
            memcpy(buffer, (char*)*cachePtr + startOffset, MIN_OF(info->size, length));
            return;
        }
    }

    F_Seek(file->_base._handle, info->baseOffset + startOffset, SEEK_SET);
    readBytes = F_Read(file->_base._handle, buffer, length);
    if(readBytes < length)
    {
        Con_Error("LumpFile::ReadLumpSection: Only read %lu of %lu bytes of lump #%i.",
                  (unsigned long) readBytes, (unsigned long) length, lumpNum);
    }
    }
}

void LumpFile_ReadLumpSection(lumpfile_t* file, lumpnum_t lumpNum, char* buffer,
    size_t startOffset, size_t length)
{
    LumpFile_ReadLumpSection2(file, lumpNum, buffer, startOffset, length, true);
}

void LumpFile_ReadLump2(lumpfile_t* file, lumpnum_t lumpNum, char* buffer, boolean tryCache)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = LumpDirectory_LumpInfo(file->_base._directory, lumpNum);
    assert(NULL != info);
    LumpFile_ReadLumpSection2(file, lumpNum, buffer, 0, info->size, tryCache);
    }
}

void LumpFile_ReadLump(lumpfile_t* file, lumpnum_t lumpNum, char* buffer)
{
    LumpFile_ReadLump2(file, lumpNum, buffer, true);
}

const char* LumpFile_CacheLump(lumpfile_t* file, lumpnum_t lumpNum, int tag)
{
    assert(NULL != file);
    {
    boolean isCached = (NULL != file->_cacheData);
    void** cachePtr = (void**)&file->_cacheData;

    if(!isCached)
    {
        const lumpinfo_t* info = LumpDirectory_LumpInfo(file->_base._directory, lumpNum);
        char* ptr;
        assert(NULL != info);
        ptr = (char*)Z_Malloc(info->size, tag, cachePtr);
        if(NULL == ptr)
            Con_Error("LumpFile::CacheLump: Failed on allocation of %lu bytes for "
                "cache copy of lump #%i.", (unsigned long) info->size, lumpNum);
        LumpFile_ReadLump2(file, lumpNum, ptr, false);
    }
    else
    {
        Z_ChangeTag(*cachePtr, tag);
    }

    return (char*)(*cachePtr);
    }
}

void LumpFile_ChangeLumpCacheTag(lumpfile_t* file, lumpnum_t lumpNum, int tag)
{
    assert(NULL != file);
    {
    boolean isCached = (NULL != file->_cacheData);
    void** cachePtr = (void**)&file->_cacheData;
    if(isCached)
    {
        Z_ChangeTag2(*cachePtr, tag);
    }
    }
}

void LumpFile_Close(lumpfile_t* file)
{
    assert(NULL != file);
    F_Close(file->_base._handle), file->_base._handle = NULL;
    F_ReleaseFileId(Str_Text(&file->_base._absolutePath));
}

int LumpFile_LumpCount(lumpfile_t* file)
{
    return 1; // Always.
}

boolean LumpFile_IsIWAD(lumpfile_t* file)
{
    return false; // Never.
}
