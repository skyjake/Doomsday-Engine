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
#include <de/NativePath>

#include <cstdio>
#include <cstring>

static dd_bool inited;
static LZFILE *savefile;
static de::Path savePath; // e.g., "savegame"
#if !__JHEXEN__
static de::Path clientSavePath; // e.g., "savegame/client"
#endif

#if __JHEXEN__
static saveptr_t saveptr;
static void *saveEndPtr;
#endif

#if __JHEXEN__
byte *saveBuffer;
#endif

static void errorIfNotInited(char const *callerName)
{
    if(inited) return;
    Con_Error("%s: Savegame I/O is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

/// @return  Possibly relative saved game directory.
static de::Path composeRootSaveDir()
{
    if(CommandLine_CheckWith("-savedir", 1))
    {
        return de::NativePath(CommandLine_NextAsPath()).withSeparators('/');
    }

    // Use the default path.
    GameInfo gameInfo;
    if(DD_GameInfo(&gameInfo))
    {
        return de::Path(SAVEGAME_DEFAULT_DIR) / Str_Text(gameInfo.identityKey);
    }

    Con_Error("composeSaveDir: Error, failed retrieving GameInfo.");
    exit(1); // Unreachable.
}

void SV_InitIO()
{
    savePath.clear();
#if !__JHEXEN__
    clientSavePath.clear();
#endif
    inited = true;
    savefile = 0;
}

void SV_ShutdownIO()
{
    if(!inited) return;

    SV_CloseFile();

    inited = false;
}

de::Path SV_SavePath()
{
    return savePath;
}

#if !__JHEXEN__
de::Path SV_ClientSavePath()
{
    return clientSavePath;
}
#endif

// Compose and create the saved game directories.
void SV_ConfigureSavePaths()
{
    DENG_ASSERT(inited);

    de::Path rootSaveDir = composeRootSaveDir();

    savePath = rootSaveDir;
#if !__JHEXEN__
    clientSavePath = rootSaveDir / "client";
#endif

    // Ensure that these paths exist.
    bool savePathExists = F_MakePath(de::NativePath(savePath).expand().toUtf8().constData());
#if !__JHEXEN__
    if(!F_MakePath(de::NativePath(clientSavePath).expand().toUtf8().constData()))
    {
        savePathExists = false;
    }
#endif

    if(!savePathExists)
    {
        App_Log(DE2_RES_ERROR, "SV_ConfigureSavePaths: Failed to locate \"%s\". Perhaps it could "
                               "not be created (insufficient permissions?). Saving will not be possible.",
                               de::NativePath(savePath).pretty().toLatin1().constData());
    }
}

LZFILE *SV_File()
{
    return savefile;
}

LZFILE *SV_OpenFile(de::Path filePath, de::String mode)
{
    DENG_ASSERT(savefile == 0);
    savefile = lzOpen(de::NativePath(filePath).expand().toUtf8().constData(), mode.toUtf8().constData());
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

bool SV_ExistingFile(de::Path filePath)
{
    if(FILE *fp = fopen(de::NativePath(filePath).expand().toUtf8().constData(), "rb"))
    {
        fclose(fp);
        return true;
    }
    return false;
}

int SV_RemoveFile(de::Path filePath)
{
    return remove(de::NativePath(filePath).expand().toUtf8().constData());
}

void SV_CopyFile(de::Path srcPath, de::Path destPath)
{
    if(!SV_ExistingFile(srcPath)) return;

    char *buffer;
    size_t length = M_ReadFile(de::NativePath(srcPath).expand().toUtf8().constData(), &buffer);
    if(!length)
    {
        App_Log(DE2_RES_ERROR, "SV_CopyFile: Failed opening \"%s\" for reading",
                               de::NativePath(srcPath).pretty().toLatin1().constData());
        return;
    }

    if(LZFILE *outf = lzOpen(de::NativePath(destPath).expand().toUtf8().constData(), "wp"))
    {
        lzWrite(buffer, length, outf);
        lzClose(outf);
    }

    Z_Free(buffer);
}

bool SV_OpenGameSaveFile(de::Path path, bool write)
{
    App_Log(DE2_DEV_RES_MSG, "SV_OpenGameSaveFile: Opening \"%s\"",
                             de::NativePath(path).pretty().toLatin1().constData());

#if __JHEXEN__
    if(!write)
    {
        size_t bufferSize = M_ReadFile(de::NativePath(path).expand().toUtf8().constData(), (char **)&saveBuffer);
        if(!bufferSize) return false;

        SV_HxSavePtr()->b = saveBuffer;
        SV_HxSetSaveEndPtr(saveBuffer + bufferSize);

        return true;
    }
    else
#endif
    {
        SV_OpenFile(path, write? "wp" : "rp");
    }

    return SV_File() != 0;
}

#if __JHEXEN__
bool SV_OpenMapSaveFile(de::Path path)
{
    App_Log(DE2_DEV_RES_MSG, "SV_OpenMapSaveFile: Opening \"%s\"",
                             de::NativePath(path).pretty().toLatin1().constData());

    // Load the file
    size_t bufferSize = M_ReadFile(de::NativePath(path).expand().toUtf8().constData(), (char **)&saveBuffer);
    if(!bufferSize) return false;

    SV_HxSavePtr()->b = saveBuffer;
    SV_HxSetSaveEndPtr(saveBuffer + bufferSize);

    return true;
}
#endif

#ifdef __JHEXEN__
saveptr_t *SV_HxSavePtr()
{
    return &saveptr;
}

void SV_HxSetSaveEndPtr(void *endPtr)
{
    saveEndPtr = endPtr;
}

size_t SV_HxBytesLeft()
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
