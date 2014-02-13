/** @file p_saveio.cpp  Game save file IO.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "p_saveio.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_saveg.h"
#include "p_savedef.h"
#include "saveinfo.h"
#include "api_materialarchive.h"

#include <cstdio>
#include <cstring>

static dd_bool inited;
static LZFILE *savefile;
static ddstring_t savePath; // e.g., "savegame/"
#if !__JHEXEN__
static ddstring_t clientSavePath; // e.g., "savegame/client/"
#endif

#if __JHEXEN__
static saveptr_t saveptr;
static void *saveEndPtr;
#endif

static void errorIfNotInited(char const *callerName)
{
    if(inited) return;
    Con_Error("%s: Savegame I/O is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

/// @return  Possibly relative saved game directory. Does not need to be free'd.
static AutoStr *composeSaveDir()
{
    AutoStr *dir = AutoStr_NewStd();

    if(CommandLine_CheckWith("-savedir", 1))
    {
        Str_Set(dir, CommandLine_Next());
        // Add a trailing backslash is necessary.
        if(Str_RAt(dir, 0) != '/')
            Str_AppendChar(dir, '/');
        return dir;
    }

    // Use the default path.
    GameInfo gameInfo;
    if(DD_GameInfo(&gameInfo))
    {
        Str_Appendf(dir, SAVEGAME_DEFAULT_DIR "/%s/", Str_Text(gameInfo.identityKey));
        return dir;
    }

    Con_Error("composeSaveDir: Error, failed retrieving GameInfo.");
    exit(1); // Unreachable.
}

void SV_InitIO()
{
    Str_Init(&savePath);
#if !__JHEXEN__
    Str_Init(&clientSavePath);
#endif
    inited = true;
    savefile = 0;
}

void SV_ShutdownIO()
{
    if(!inited) return;

    SV_CloseFile();

    Str_Free(&savePath);
#if !__JHEXEN__
    Str_Free(&clientSavePath);
#endif

    inited = false;
}

char const *SV_SavePath()
{
    return Str_Text(&savePath);
}

#if !__JHEXEN__
char const *SV_ClientSavePath()
{
    return Str_Text(&clientSavePath);
}
#endif

// Compose and create the saved game directories.
void SV_ConfigureSavePaths()
{
    DENG_ASSERT(inited);

    AutoStr *saveDir = composeSaveDir();
    dd_bool savePathExists;

    Str_Set(&savePath, Str_Text(saveDir));
#if !__JHEXEN__
    Str_Clear(&clientSavePath); Str_Appendf(&clientSavePath, "%sclient/", Str_Text(saveDir));
#endif

    // Ensure that these paths exist.
    savePathExists = F_MakePath(Str_Text(&savePath));
#if !__JHEXEN__
    if(!F_MakePath(Str_Text(&clientSavePath)))
        savePathExists = false;
#endif

    if(!savePathExists)
    {
        App_Log(DE2_RES_ERROR, "SV_ConfigureSavePaths: Failed to locate \"%s\". Perhaps it could "
                "not be created (insufficent permissions?). Saving will not be possible.",
                Str_Text(&savePath));
    }
}

LZFILE *SV_File()
{
    return savefile;
}

LZFILE *SV_OpenFile(Str const *filePath, char const *mode)
{
    DENG_ASSERT(savefile == 0);
    savefile = lzOpen(Str_Text(filePath), (char *)mode);
    return savefile;
}

void SV_CloseFile()
{
    if(savefile)
    {
        lzClose(savefile);
        savefile = 0;
    }
}

dd_bool SV_ExistingFile(Str const *filePath)
{
    if(FILE *fp = fopen(Str_Text(filePath), "rb"))
    {
        fclose(fp);
        return true;
    }
    return false;
}

int SV_RemoveFile(Str const *filePath)
{
    if(!filePath) return 1;
    return remove(Str_Text(filePath));
}

void SV_CopyFile(Str const *srcPath, Str const *destPath)
{
    if(!srcPath || !destPath) return;

    if(!SV_ExistingFile(srcPath)) return;

    char *buffer;
    size_t length = M_ReadFile(Str_Text(srcPath), &buffer);
    if(!length)
    {
        App_Log(DE2_RES_ERROR, "SV_CopyFile: Failed opening \"%s\" for reading", Str_Text(srcPath));
        return;
    }

    if(LZFILE *outf = lzOpen((char*)Str_Text(destPath), "wp"))
    {
        lzWrite(buffer, length, outf);
        lzClose(outf);
    }

    Z_Free(buffer);
}

#ifdef __JHEXEN__
saveptr_t *SV_HxSavePtr()
{
    return &saveptr;
}

void SV_HxSetSaveEndPtr(void *endPtr)
{
    saveEndPtr = endPtr;
}

dd_bool SV_HxBytesLeft()
{
    return (byte *) saveEndPtr - saveptr.b;
}
#endif

void SV_Seek(uint offset)
{
    errorIfNotInited("SV_SetPos");
#if __JHEXEN__
    saveptr.b += offset;
#else
    lzSeek(savefile, offset);
#endif
}

void SV_Write(void const *data, int len)
{
    errorIfNotInited("SV_Write");
    lzWrite((void *)data, len, savefile);
}

void SV_WriteByte(byte val)
{
    errorIfNotInited("SV_WriteByte");
    lzPutC(val, savefile);
}

#if __JHEXEN__
void SV_WriteShort(unsigned short val)
#else
void SV_WriteShort(short val)
#endif
{
    errorIfNotInited("SV_WriteShort");
    lzPutW(val, savefile);
}

#if __JHEXEN__
void SV_WriteLong(unsigned int val)
#else
void SV_WriteLong(long val)
#endif
{
    errorIfNotInited("SV_WriteLong");
    lzPutL(val, savefile);
}

void SV_WriteFloat(float val)
{
    DENG_ASSERT(sizeof(val) == 4);

    int32_t temp = 0;
    errorIfNotInited("SV_WriteFloat");
    std::memcpy(&temp, &val, 4);
    lzPutL(temp, savefile);
}

void SV_Read(void *data, int len)
{
    errorIfNotInited("SV_Read");
#if __JHEXEN__
    std::memcpy(data, saveptr.b, len);
    saveptr.b += len;
#else
    lzRead(data, len, savefile);
#endif
}

byte SV_ReadByte()
{
    errorIfNotInited("SV_ReadByte");
#if __JHEXEN__
    DENG_ASSERT((saveptr.b + 1) <= (byte *) saveEndPtr);
    return (*saveptr.b++);
#else
    return lzGetC(savefile);
#endif
}

short SV_ReadShort()
{
    errorIfNotInited("SV_ReadShort");
#if __JHEXEN__
    DENG_ASSERT((saveptr.w + 1) <= (short *) saveEndPtr);
    return (SHORT(*saveptr.w++));
#else
    return lzGetW(savefile);
#endif
}

long SV_ReadLong()
{
    errorIfNotInited("SV_ReadLong");
#if __JHEXEN__
    DENG_ASSERT((saveptr.l + 1) <= (int *) saveEndPtr);
    return (LONG(*saveptr.l++));
#else
    return lzGetL(savefile);
#endif
}

float SV_ReadFloat()
{
#if !__JHEXEN__
    float returnValue = 0;
    int32_t val;
#endif
    errorIfNotInited("SV_ReadFloat");
#if __JHEXEN__
    return (FLOAT(*saveptr.f++));
#else
    val = lzGetL(savefile);
    returnValue = 0;
    DENG_ASSERT(sizeof(float) == 4);
    std::memcpy(&returnValue, &val, 4);
    return returnValue;
#endif
}

void SV_AssertSegment(int segmentId)
{
    errorIfNotInited("SV_AssertSegment");
#if __JHEXEN__
    if(segmentId == ASEG_END && SV_HxBytesLeft() < 4)
    {
        App_Log(DE2_LOG_WARNING, "Savegame lacks ASEG_END marker (unexpected end-of-file)\n");
        return;
    }
    if(SV_ReadLong() != segmentId)
    {
        Con_Error("Corrupt save game: Segment [%d] failed alignment check", segmentId);
    }
#else
    DENG_UNUSED(segmentId);
#endif
}

void SV_BeginSegment(int segType)
{
    errorIfNotInited("SV_BeginSegment");
#if __JHEXEN__
    SV_WriteLong(segType);
#else
    DENG_UNUSED(segType);
#endif
}

void SV_EndSegment()
{
    SV_BeginSegment(ASEG_END);
}

void SV_WriteConsistencyBytes()
{
#if !__JHEXEN__
    SV_WriteByte(CONSISTENCY);
#endif
}

void SV_ReadConsistencyBytes()
{
#if !__JHEXEN__
    if(SV_ReadByte() != CONSISTENCY)
    {
        Con_Error("Corrupt save game: Consistency test failed.");
    }
#endif
}

static void swi8(Writer *w, char i)
{
    if(!w) return;
    SV_WriteByte(i);
}

static void swi16(Writer *w, short i)
{
    if(!w) return;
    SV_WriteShort(i);
}

static void swi32(Writer *w, int i)
{
    if(!w) return;
    SV_WriteLong(i);
}

static void swf(Writer *w, float i)
{
    if(!w) return;
    SV_WriteFloat(i);
}

static void swd(Writer *w, char const *data, int len)
{
    if(!w) return;
    SV_Write(data, len);
}

Writer *SV_NewWriter()
{
    return Writer_NewWithCallbacks(swi8, swi16, swi32, swf, swd);
}

static char sri8(Reader *r)
{
    if(!r) return 0;
    return SV_ReadByte();
}

static short sri16(Reader *r)
{
    if(!r) return 0;
    return SV_ReadShort();
}

static int sri32(Reader *r)
{
    if(!r) return 0;
    return SV_ReadLong();
}

static float srf(Reader *r)
{
    if(!r) return 0;
    return SV_ReadFloat();
}

static void srd(Reader *r, char *data, int len)
{
    if(!r) return;
    SV_Read(data, len);
}

Reader *SV_NewReader()
{
    return Reader_NewWithCallbacks(sri8, sri16, sri32, srf, srd);
}
