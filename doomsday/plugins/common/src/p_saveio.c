/**\file p_saveio.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include "p_savedef.h"
#include "materialarchive.h"

static boolean inited;
static LZFILE* savefile;
static ddstring_t savePath; // e.g., "savegame/"
#if !__JHEXEN__
static ddstring_t clientSavePath; // e.g., "savegame/client/"
#endif

#if __JHEXEN__
static saveptr_t saveptr;
#endif

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: Savegame I/O is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

/// @return  Possibly relative saved game directory. Does not need to be free'd.
static AutoStr* composeSaveDir(void)
{
    AutoStr* dir = AutoStr_NewStd();

    if(CommandLine_CheckWith("-savedir", 1))
    {
        Str_Set(dir, CommandLine_Next());
        // Add a trailing backslash is necessary.
        if(Str_RAt(dir, 0) != '/')
            Str_AppendChar(dir, '/');
        return dir;
    }

    // Use the default path.
    { GameInfo gameInfo;
    if(DD_GameInfo(&gameInfo))
    {
        Str_Appendf(dir, SAVEGAME_DEFAULT_DIR "/%s/", gameInfo.identityKey);
        return dir;
    }}

    Con_Error("composeSaveDir: Error, failed retrieving GameInfo.");
    exit(1); // Unreachable.
}

void SV_InitIO(void)
{
    Str_Init(&savePath);
#if !__JHEXEN__
    Str_Init(&clientSavePath);
#endif
    inited = true;
    savefile = 0;
}

void SV_ShutdownIO(void)
{
    if(!inited) return;

    SV_CloseFile();

    Str_Free(&savePath);
#if !__JHEXEN__
    Str_Free(&clientSavePath);
#endif

    inited = false;
}

const char* SV_SavePath(void)
{
    return Str_Text(&savePath);
}

#if !__JHEXEN__
const char* SV_ClientSavePath(void)
{
    return Str_Text(&clientSavePath);
}
#endif

// Compose and create the saved game directories.
void SV_ConfigureSavePaths(void)
{
    assert(inited);
    {
    AutoStr* saveDir = composeSaveDir();
    boolean savePathExists;

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
        Con_Message("Warning:configureSavePaths: Failed to locate \"%s\"\nPerhaps it could "
                    "not be created (insufficent permissions?). Saving will not be possible.\n",
                    Str_Text(&savePath));
    }
}

LZFILE* SV_File(void)
{
    return savefile;
}

LZFILE* SV_OpenFile(const char* fileName, const char* mode)
{
    assert(savefile == 0);
    savefile = lzOpen((char*)fileName, (char*)mode);
    return savefile;
}

void SV_CloseFile(void)
{
    if(savefile)
    {
        lzClose(savefile);
        savefile = 0;
    }
}

boolean SV_ExistingFile(char* name)
{
    FILE* fp;
    if((fp = fopen(name, "rb")) != NULL)
    {
        fclose(fp);
        return true;
    }
    return false;
}

int SV_RemoveFile(const ddstring_t* path)
{
    if(!path) return 1;
    return remove(Str_Text(path));
}

void SV_CopyFile(const ddstring_t* srcPath, const ddstring_t* destPath)
{
    size_t length;
    char* buffer;
    LZFILE* outf;

    if(!srcPath || !destPath) return;
    if(!SV_ExistingFile(Str_Text(srcPath))) return;

    length = M_ReadFile(Str_Text(srcPath), &buffer);
    if(0 == length)
    {
        Con_Message("Warning: SV_CopyFile: Failed opening \"%s\" for reading.\n", Str_Text(srcPath));
        return;
    }

    outf = lzOpen((char*)Str_Text(destPath), "wp");
    if(outf)
    {
        lzWrite(buffer, length, outf);
        lzClose(outf);
    }
    Z_Free(buffer);
}

#ifdef __JHEXEN__
saveptr_t* SV_HxSavePtr(void)
{
    return &saveptr;
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

void SV_Write(const void* data, int len)
{
    errorIfNotInited("SV_Write");
    lzWrite((void*)data, len, savefile);
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
    int32_t temp = 0;
    assert(sizeof(val) == 4);
    errorIfNotInited("SV_WriteFloat");
    memcpy(&temp, &val, 4);
    lzPutL(temp, savefile);
}

void SV_Read(void *data, int len)
{
    errorIfNotInited("SV_Read");
#if __JHEXEN__
    memcpy(data, saveptr.b, len);
    saveptr.b += len;
#else
    lzRead(data, len, savefile);
#endif
}

byte SV_ReadByte(void)
{
    errorIfNotInited("SV_ReadByte");
#if __JHEXEN__
    return (*saveptr.b++);
#else
    return lzGetC(savefile);
#endif
}

short SV_ReadShort(void)
{
    errorIfNotInited("SV_ReadShort");
#if __JHEXEN__
    return (SHORT(*saveptr.w++));
#else
    return lzGetW(savefile);
#endif
}

long SV_ReadLong(void)
{
    errorIfNotInited("SV_ReadLong");
#if __JHEXEN__
    return (LONG(*saveptr.l++));
#else
    return lzGetL(savefile);
#endif
}

float SV_ReadFloat(void)
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
    assert(sizeof(float) == 4);
    memcpy(&returnValue, &val, 4);
    return returnValue;
#endif
}

static void swi8(Writer* w, char i)
{
    if(!w) return;
    SV_WriteByte(i);
}

static void swi16(Writer* w, short i)
{
    if(!w) return;
    SV_WriteShort(i);
}

static void swi32(Writer* w, int i)
{
    if(!w) return;
    SV_WriteLong(i);
}

static void swf(Writer* w, float i)
{
    if(!w) return;
    SV_WriteFloat(i);
}

static void swd(Writer* w, const char* data, int len)
{
    if(!w) return;
    SV_Write(data, len);
}

void SaveInfo_Write(saveheader_t* info, Writer* writer)
{
    assert(info);

    Writer_WriteInt32(writer, info->magic);
    Writer_WriteInt32(writer, info->version);
    Writer_WriteInt32(writer, info->gameMode);

    {
    ddstring_t name;
    Str_InitStatic(&name, info->name);
    Str_Write(&name, writer);
    }

    Writer_WriteByte(writer, info->skill);
    Writer_WriteByte(writer, info->episode);
    Writer_WriteByte(writer, info->map);
    Writer_WriteByte(writer, info->deathmatch);
    Writer_WriteByte(writer, info->noMonsters);
#if __JHEXEN__
    Writer_WriteByte(writer, info->randomClasses);
#else
    Writer_WriteByte(writer, info->respawnMonsters);
    Writer_WriteInt32(writer, info->mapTime);
    { int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        Writer_WriteByte(writer, info->players[i]);
    }}
#endif
    Writer_WriteInt32(writer, info->gameId);
}

void SV_SaveInfo_Write(saveheader_t* info)
{
    Writer* svWriter = Writer_NewWithCallbacks(swi8, swi16, swi32, swf, swd);
    SaveInfo_Write(info, svWriter);
    Writer_Delete(svWriter);
}

void SV_MaterialArchive_Write(MaterialArchive* arc)
{
    Writer* svWriter = Writer_NewWithCallbacks(swi8, swi16, swi32, swf, swd);
    MaterialArchive_Write(arc, svWriter);
    Writer_Delete(svWriter);
}

static char sri8(Reader* r)
{
    if(!r) return 0;
    return SV_ReadByte();
}

static short sri16(Reader* r)
{
    if(!r) return 0;
    return SV_ReadShort();
}

static int sri32(Reader* r)
{
    if(!r) return 0;
    return SV_ReadLong();
}

static float srf(Reader* r)
{
    if(!r) return 0;
    return SV_ReadFloat();
}

static void srd(Reader* r, char* data, int len)
{
    if(!r) return;
    SV_Read(data, len);
}

#if __JDOOM__ || __JHERETIC__
static void translateLegacyGameMode(gamemode_t* mode)
{
    static const gamemode_t oldGameModes[] = {
# if __JDOOM__
        doom_shareware,
        doom,
        doom2,
        doom_ultimate
# else // __JHERETIC__
        heretic_shareware,
        heretic,
        heretic_extended
# endif
    };

    if(!mode) return;

    *mode = oldGameModes[(int)(*mode)];

# if __JDOOM__
    /**
     * @note Kludge: Older versions did not differentiate between versions
     * of Doom2 (i.e., Plutonia and TNT are marked as Doom2). If we detect
     * that this save is from some version of Doom2, replace the marked
     * gamemode with the current gamemode.
     */
    if((*mode) == doom2 && (gameModeBits & GM_ANY_DOOM2))
    {
        (*mode) = gameMode;
    }
    /// kludge end.
# endif
}
#endif

void SaveInfo_Read(saveheader_t* info, Reader* reader)
{
    assert(info);

    info->magic = Reader_ReadInt32(reader);
    info->version = Reader_ReadInt32(reader);
    info->gameMode = (gamemode_t)Reader_ReadInt32(reader);

    if(info->version >= 10)
    {
        ddstring_t buf;
        Str_InitStd(&buf);
        Str_Read(&buf, reader);
        memcpy(info->name, Str_Text(&buf), SAVESTRINGSIZE);
        info->name[SAVESTRINGSIZE] = '\0';
        Str_Free(&buf);
    }
    else
    {
        // Older formats use a fixed-length name (24 characters).
        Reader_Read(reader, info->name, SAVESTRINGSIZE);
    }
    info->skill = Reader_ReadByte(reader);
    info->episode = Reader_ReadByte(reader);
    info->map = Reader_ReadByte(reader);
    info->deathmatch = Reader_ReadByte(reader);
    info->noMonsters = Reader_ReadByte(reader);
#if __JHEXEN__
    info->randomClasses = Reader_ReadByte(reader);
#endif

#if !__JHEXEN__
    info->respawnMonsters = Reader_ReadByte(reader);

    // Older formats serialize the unpacked saveheader_t struct; skip the junk values (alignment).
    if(info->version < 10) SV_Seek(2);

    info->mapTime = Reader_ReadInt32(reader);
    { int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        info->players[i] = Reader_ReadByte(reader);
    }}
#endif

    info->gameId = Reader_ReadInt32(reader);

    // Translate gameMode identifiers from older save versions.
#if __JDOOM__ || __JHERETIC__
# if __JDOOM__
    if(info->version < 9)
# else // __JHERETIC__
    if(info->version < 8)
# endif
    {
        translateLegacyGameMode(&info->gameMode);
    }
#endif
}

#if __JHEXEN__
void SaveInfo_Read_Hx_v9(saveheader_t* info, Reader* reader)
{
# define HXS_VERSION_TEXT      "HXS Ver " // Do not change me!
# define HXS_VERSION_TEXT_LENGTH 16

    char verText[HXS_VERSION_TEXT_LENGTH];

    assert(info);

    Reader_Read(reader, &info->name, SAVESTRINGSIZE);
    Reader_Read(reader, &verText, HXS_VERSION_TEXT_LENGTH);
    info->version = atoi(&verText[8]);

    SV_Seek(4); // Junk.

    info->episode = 1;
    info->map = Reader_ReadByte(reader);
    info->skill = Reader_ReadByte(reader);
    info->deathmatch = Reader_ReadByte(reader);
    info->noMonsters = Reader_ReadByte(reader);
    info->randomClasses = Reader_ReadByte(reader);

    info->magic = MY_SAVE_MAGIC; // Lets pretend...

    /// @note Older formats do not contain all needed values:
    info->gameMode = gameMode; // Assume the current mode.
    info->gameId  = 0; // None.

# undef HXS_VERSION_TEXT_LENGTH
# undef HXS_VERSION_TEXT
}
#endif

void SV_SaveInfo_Read(saveheader_t* info)
{
    Reader* svReader = Reader_NewWithCallbacks(sri8, sri16, sri32, srf, srd);
#if __JHEXEN__
    // Read the magic byte to determine the high-level format.
    int magic = Reader_ReadInt32(svReader);
    saveptr.b -= 4; // Rewind the stream.

    if(magic != MY_SAVE_MAGIC)
    {
        // Perhaps the old v9 format?
        SaveInfo_Read_Hx_v9(info, svReader);
    }
    else
#endif
    {
        SaveInfo_Read(info, svReader);
    }
    Reader_Delete(svReader);
}

void SV_MaterialArchive_Read(MaterialArchive* arc, int version)
{
    Reader* svReader = Reader_NewWithCallbacks(sri8, sri16, sri32, srf, srd);
    MaterialArchive_Read(arc, version, svReader);
    Reader_Delete(svReader);
}
