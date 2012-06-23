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

#define MAX_HUB_MAPS 99

static boolean inited;
static LZFILE* savefile;
static ddstring_t savePath; // e.g., "savegame/"
#if !__JHEXEN__
static ddstring_t clientSavePath; // e.g., "savegame/client/"
#endif
static saveinfo_t* saveInfo;
static saveinfo_t autoSaveInfo;
#if __JHEXEN__
static saveinfo_t baseSaveInfo;
#endif

#if __JHEXEN__
static saveptr_t saveptr;
#endif

static boolean readGameSaveHeader(saveinfo_t* info);

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: Savegame I/O is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static void initSaveInfo(saveinfo_t* info)
{
    if(!info) return;
    Str_Init(&info->filePath);
    Str_Init(&info->name);
}

static void updateSaveInfo(saveinfo_t* info, ddstring_t* savePath)
{
    if(!info) return;

    Str_CopyOrClear(&info->filePath, savePath);
    if(Str_IsEmpty(&info->filePath))
    {
        // The save path cannot be accessed for some reason. Perhaps its a
        // network path? Clear the info for this slot.
        Str_Clear(&info->name);
        return;
    }

    if(!readGameSaveHeader(info))
    {
        // Not a valid save file.
        Str_Clear(&info->filePath);
    }
}

static void clearSaveInfo(saveinfo_t* info)
{
    if(!info) return;
    Str_Free(&info->filePath);
    Str_Free(&info->name);
}

static boolean existingFile(char* name)
{
    FILE* fp;
    if((fp = fopen(name, "rb")) != NULL)
    {
        fclose(fp);
        return true;
    }
    return false;
}

static int removeFile(const ddstring_t* path)
{
    if(!path) return 1;
    return remove(Str_Text(path));
}

static void copyFile(const ddstring_t* srcPath, const ddstring_t* destPath)
{
    size_t length;
    char* buffer;
    LZFILE* outf;

    if(!srcPath || !destPath) return;
    if(!existingFile(Str_Text(srcPath))) return;

    length = M_ReadFile(Str_Text(srcPath), &buffer);
    if(0 == length)
    {
        Con_Message("Warning: copyFile: Failed opening \"%s\" for reading.\n", Str_Text(srcPath));
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

/**
 * Compose the (possibly relative) path to the game-save associated
 * with the logical save @a slot.
 *
 * @param slot  Logical save slot identifier.
 * @param map   If @c >= 0 include this logical map index in the composed path.
 * @return  The composed path if reachable (else @c NULL). Does not need to be free'd.
 */
static AutoStr* composeGameSavePathForSlot2(int slot, int map)
{
    AutoStr* path;
    assert(inited);

    // A valid slot?
    if(!SV_IsValidSlot(slot)) return NULL;

    // Do we have a valid path?
    if(!F_MakePath(SV_SavePath())) return NULL;

    // Compose the full game-save path and filename.
    path = AutoStr_NewStd();
    if(map >= 0)
    {
        Str_Appendf(path, "%s" SAVEGAMENAME "%i%02i." SAVEGAMEEXTENSION, SV_SavePath(), slot, map);
    }
    else
    {
        Str_Appendf(path, "%s" SAVEGAMENAME "%i." SAVEGAMEEXTENSION, SV_SavePath(), slot);
    }
    F_TranslatePath(path, path);
    return path;
}

static AutoStr* composeGameSavePathForSlot(int slot)
{
    return composeGameSavePathForSlot2(slot, -1);
}

void SV_InitIO(void)
{
    Str_Init(&savePath);
#if !__JHEXEN__
    Str_Init(&clientSavePath);
#endif
    saveInfo = NULL;
    inited = true;
    savefile = 0;
}

void SV_ShutdownIO(void)
{
    inited = false;

    SV_CloseFile();

    if(saveInfo)
    {
        int i;
        for(i = 0; i < NUMSAVESLOTS; ++i)
        {
            saveinfo_t* info = &saveInfo[i];
            clearSaveInfo(info);
        }
        free(saveInfo); saveInfo = NULL;

        clearSaveInfo(&autoSaveInfo);
#if __JHEXEN__
        clearSaveInfo(&baseSaveInfo);
#endif
    }

    Str_Free(&savePath);
#if !__JHEXEN__
    Str_Free(&clientSavePath);
#endif
}

const char* SV_SavePath(void)
{
    return Str_Text(&savePath);
}

#ifdef __JHEXEN__
saveptr_t* SV_HxSavePtr(void)
{
    return &saveptr;
}
#endif

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

void SV_ClearSlot(int slot)
{
    AutoStr* path;

    errorIfNotInited("SV_ClearSlot");
    if(!SV_IsValidSlot(slot)) return;

    { int i;
    for(i = 0; i < MAX_HUB_MAPS; ++i)
    {
        path = composeGameSavePathForSlot2(slot, i);
        removeFile(path);
    }}

    path = composeGameSavePathForSlot(slot);
    removeFile(path);
}

boolean SV_IsValidSlot(int slot)
{
    if(slot == AUTO_SLOT) return true;
#if __JHEXEN__
    if(slot == BASE_SLOT) return true;
#endif
    return (slot >= 0  && slot < NUMSAVESLOTS);
}

boolean SV_IsUserWritableSlot(int slot)
{
    if(slot == AUTO_SLOT) return false;
#if __JHEXEN__
    if(slot == BASE_SLOT) return false;
#endif
    return SV_IsValidSlot(slot);
}

static boolean readGameSaveHeader(saveinfo_t* info)
{
    boolean found = false;
#if __JHEXEN__
    byte* saveBuffer;
#endif
    assert(inited && info);

    if(Str_IsEmpty(&info->filePath)) return false;

#if __JHEXEN__
    /// @todo Do not buffer the whole file.
    if(M_ReadFile(Str_Text(&info->filePath), (char**)&saveBuffer))
#else
    if(SV_OpenFile(Str_Text(&info->filePath), "rp"))
#endif
    {
        saveheader_t* hdr = SV_SaveHeader();

#if __JHEXEN__
        // Set the save pointer.
        SV_HxSavePtr()->b = saveBuffer;
#endif

        SV_SaveInfo_Read(hdr);

#if __JHEXEN__
        Z_Free(saveBuffer);
#else
        SV_CloseFile();
#endif

        if(MY_SAVE_MAGIC == hdr->magic)
        {
            Str_Set(&info->name, hdr->name);
            found = true;
        }
    }

    // If not found or not recognized try other supported formats.
#if __JDOOM__ || __JHERETIC__
    if(!found)
    {
        // Perhaps a DOOM(2).EXE v19 saved game?
        if(SV_OpenFile(Str_Text(&info->filePath), "r"))
        {
            char nameBuffer[SAVESTRINGSIZE];
            lzRead(nameBuffer, SAVESTRINGSIZE, SV_File());
            nameBuffer[SAVESTRINGSIZE - 1] = 0;
            Str_Set(&info->name, nameBuffer);
            SV_CloseFile();
            found = true;
        }
    }
#endif

    // Ensure we have a non-empty name.
    if(found && Str_IsEmpty(&info->name))
    {
        Str_Set(&info->name, "UNNAMED");
    }

    return found;
}

/// Re-build game-save info by re-scanning the save paths and populating the list.
static void buildSaveInfo(void)
{
    int i;
    assert(inited);

    if(!saveInfo)
    {
        // Not yet been here. We need to allocate and initialize the game-save info list.
        saveInfo = (saveinfo_t*) malloc(NUMSAVESLOTS * sizeof(*saveInfo));
        if(!saveInfo)
            Con_Error("buildSaveInfo: Failed on allocation of %lu bytes for game-save info list.",
                      (unsigned long) (NUMSAVESLOTS * sizeof(*saveInfo)));

        // Initialize.
        for(i = 0; i < NUMSAVESLOTS; ++i)
        {
            saveinfo_t* info = &saveInfo[i];
            initSaveInfo(info);
        }
        initSaveInfo(&autoSaveInfo);
#if __JHEXEN__
        initSaveInfo(&baseSaveInfo);
#endif
    }

    /// Scan the save paths and populate the list.
    /// \todo We should look at all files on the save path and not just those
    /// which match the default game-save file naming convention.
    for(i = 0; i < NUMSAVESLOTS; ++i)
    {
        saveinfo_t* info = &saveInfo[i];
        updateSaveInfo(info, composeGameSavePathForSlot(i));
    }
    updateSaveInfo(&autoSaveInfo, composeGameSavePathForSlot(AUTO_SLOT));
#if __JHEXEN__
    updateSaveInfo(&baseSaveInfo, composeGameSavePathForSlot(BASE_SLOT));
#endif
}

/// Given a logical save slot identifier retrieve the assciated game-save info.
static saveinfo_t* findSaveInfoForSlot(int slot)
{
    static saveinfo_t invalidInfo = { { "" }, { "" } };
    assert(inited);

    if(!SV_IsValidSlot(slot)) return &invalidInfo;

    // On first call - automatically build and populate game-save info.
    if(!saveInfo)
    {
        buildSaveInfo();
    }

    // Retrieve the info for this slot.
    if(slot == AUTO_SLOT) return &autoSaveInfo;
#if __JHEXEN__
    if(slot == BASE_SLOT) return &baseSaveInfo;
#endif
    return &saveInfo[slot];
}

const saveinfo_t* SV_SaveInfoForSlot(int slot)
{
    errorIfNotInited("SV_SaveInfoForSlot");
    return findSaveInfoForSlot(slot);
}

void SV_UpdateAllSaveInfo(void)
{
    errorIfNotInited("SV_UpdateAllSaveInfo");
    buildSaveInfo();
}

int SV_ParseSlotIdentifier(const char* str)
{
    int slot;

    // Try game-save name match.
    slot = SV_SlotForSaveName(str);
    if(slot >= 0)
    {
        return slot;
    }

    // Try keyword identifiers.
    if(!stricmp(str, "last") || !stricmp(str, "<last>"))
    {
        return Con_GetInteger("game-save-last-slot");
    }
    if(!stricmp(str, "quick") || !stricmp(str, "<quick>"))
    {
        return Con_GetInteger("game-save-quick-slot");
    }
    if(!stricmp(str, "auto") || !stricmp(str, "<auto>"))
    {
        return AUTO_SLOT;
    }

    // Try logical slot identifier.
    if(M_IsStringValidInt(str))
    {
        return atoi(str);
    }

    // Unknown/not found.
    return -1;
}

int SV_SlotForSaveName(const char* name)
{
    int saveSlot = -1;

    errorIfNotInited("SV_SlotForSaveName");

    if(name && name[0])
    {
        int i = 0;
        // On first call - automatically build and populate game-save info.
        if(!saveInfo)
        {
            buildSaveInfo();
        }

        do
        {
            const saveinfo_t* info = &saveInfo[i];
            if(!Str_CompareIgnoreCase(&info->name, name))
            {
                // This is the one!
                saveSlot = i;
            }
        } while(-1 == saveSlot && ++i < NUMSAVESLOTS);
    }
    return saveSlot;
}

boolean SV_ComposeSavePathForSlot(int slot, ddstring_t* path)
{
    errorIfNotInited("SV_ComposeSavePathForSlot");
    if(!path) return false;
    Str_CopyOrClear(path, composeGameSavePathForSlot(slot));
    return !Str_IsEmpty(path);
}

#if __JHEXEN__
boolean SV_ComposeSavePathForMapSlot(uint map, int slot, ddstring_t* path)
{
    errorIfNotInited("SV_ComposeSavePathForMapSlot");
    if(!path) return false;
    Str_CopyOrClear(path, composeGameSavePathForSlot2(slot, (int)map));
    return !Str_IsEmpty(path);
}
#else
/**
 * Compose the (possibly relative) path to the game-save associated
 * with @a gameId. If the game-save path is unreachable then @a path
 * will be made empty.
 *
 * @param gameId  Unique game identifier.
 * @param path  String buffer to populate with the game save path.
 * @return  @c true if @a path was set.
 */
static boolean composeClientGameSavePathForGameId(uint gameId, ddstring_t* path)
{
    assert(inited && NULL != path);
    // Do we have a valid path?
    if(!F_MakePath(SV_ClientSavePath())) return false;
    // Compose the full game-save path and filename.
    Str_Clear(path);
    Str_Appendf(path, "%s" CLIENTSAVEGAMENAME "%08X." SAVEGAMEEXTENSION, SV_ClientSavePath(), gameId);
    F_TranslatePath(path, path);
    return true;
}

boolean SV_ComposeSavePathForClientGameId(uint gameId, ddstring_t* path)
{
    errorIfNotInited("SV_ComposeSavePathForSlot");
    if(!path) return false;
    Str_Clear(path);
    return composeClientGameSavePathForGameId(gameId, path);
}
#endif

boolean SV_IsSlotUsed(int slot)
{
    const saveinfo_t* info;
    errorIfNotInited("SV_IsSlotUsed");

    info = SV_SaveInfoForSlot(slot);
    return !Str_IsEmpty(&info->filePath);
}

#if __JHEXEN__
boolean SV_HxHaveMapSaveForSlot(int slot, uint map)
{
    AutoStr* path = composeGameSavePathForSlot2(slot, (int)map);
    if(!path || Str_IsEmpty(path)) return false;
    return existingFile(Str_Text(path));
}
#endif

void SV_CopySlot(int sourceSlot, int destSlot)
{
    AutoStr* src, *dst;

    errorIfNotInited("SV_CopySlot");

    if(!SV_IsValidSlot(sourceSlot))
    {
#if _DEBUG
        Con_Message("Warning: SV_CopySlot: Source slot %i invalid, save game not copied.\n", sourceSlot);
#endif
        return;
    }

    if(!SV_IsValidSlot(destSlot))
    {
#if _DEBUG
        Con_Message("Warning: SV_CopySlot: Dest slot %i invalid, save game not copied.\n", destSlot);
#endif
        return;
    }

    { int i;
    for(i = 0; i < MAX_HUB_MAPS; ++i)
    {
        src = composeGameSavePathForSlot2(sourceSlot, i);
        dst = composeGameSavePathForSlot2(destSlot, i);
        copyFile(src, dst);
    }}

    src = composeGameSavePathForSlot(sourceSlot);
    dst = composeGameSavePathForSlot(destSlot);
    copyFile(src, dst);
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

void SV_Seek(uint offset)
{
    errorIfNotInited("SV_SetPos");
#if __JHEXEN__
    saveptr.b += offset;
#else
    lzSeek(savefile, offset);
#endif
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
