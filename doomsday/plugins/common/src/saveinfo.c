/**
 * @file saveinfo.c
 * Save state info.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "p_tick.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include "saveinfo.h"

SaveInfo* SaveInfo_New(void)
{
    SaveInfo* info = (SaveInfo*)malloc(sizeof *info);
    if(!info) Con_Error("SaveInfo_New: Failed on allocation of %lu bytes for new SaveInfo.", (unsigned long) sizeof *info);

    Str_Init(&info->name);
    info->gameId = 0;
    memset(&info->header, 0, sizeof(info->header));

    return info;
}

SaveInfo* SaveInfo_NewCopy(const SaveInfo* other)
{
    return SaveInfo_Copy(SaveInfo_New(), other);
}

void SaveInfo_Delete(SaveInfo* info)
{
    DENG_ASSERT(info);
    Str_Free(&info->name);
    free(info);
}

SaveInfo* SaveInfo_Copy(SaveInfo* self, const SaveInfo* other)
{
    DENG_ASSERT(self);
    if(!other) return self;
    Str_Copy(&self->name, SaveInfo_Name(other));
    self->gameId = SaveInfo_GameId(other);
    memcpy(&self->header, SaveInfo_Header(other), sizeof(self->header));
    return self;
}

uint SaveInfo_GameId(const SaveInfo* info)
{
    DENG_ASSERT(info);
    return info->gameId;
}

const saveheader_t* SaveInfo_Header(const SaveInfo* info)
{
    DENG_ASSERT(info);
    return &info->header;
}

const ddstring_t* SaveInfo_Name(const SaveInfo* info)
{
    DENG_ASSERT(info);
    return &info->name;
}

void SaveInfo_SetGameId(SaveInfo* info, uint newGameId)
{
    DENG_ASSERT(info);
    info->gameId = newGameId;
}

void SaveInfo_SetName(SaveInfo* info, const ddstring_t* newName)
{
    DENG_ASSERT(info);
    Str_CopyOrClear(&info->name, newName);
}

void SaveInfo_Configure(SaveInfo* info)
{
    saveheader_t* hdr;
    DENG_ASSERT(info);

    hdr = &info->header;
    hdr->magic    = IS_NETWORK_CLIENT? MY_CLIENT_SAVE_MAGIC : MY_SAVE_MAGIC;
    hdr->version  = MY_SAVE_VERSION;
    hdr->gameMode = gameMode;

    hdr->map = gameMap+1;
#if __JHEXEN__
    hdr->episode = 1;
#else
    hdr->episode = gameEpisode+1;
#endif
#if __JHEXEN__
    hdr->skill = gameSkill;
    hdr->randomClasses = randomClassParm;
#else
    hdr->skill = gameSkill;
    if(fastParm) hdr->skill |= 0x80; // Set high byte.
#endif
    hdr->deathmatch = deathmatch;
    hdr->noMonsters = noMonstersParm;

#if __JHEXEN__
    hdr->randomClasses = randomClassParm;
#else
    hdr->respawnMonsters = respawnMonsters;
    hdr->mapTime = mapTime;
    { int i;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        hdr->players[i] = players[i].plr->inGame;
    }}
#endif
}

boolean SaveInfo_IsLoadable(SaveInfo* info)
{
    DENG_ASSERT(info);

    // Game Mode missmatch?
    if(info->header.gameMode != gameMode) return false;

    /// @todo Validate loaded add-ons and checksum the definition database.

    return true; // It's good!
}

void SaveInfo_Write(SaveInfo* saveInfo, Writer* writer)
{
    saveheader_t* info;
    DENG_ASSERT(saveInfo);

    info = &saveInfo->header;
    Writer_WriteInt32(writer, info->magic);
    Writer_WriteInt32(writer, info->version);
    Writer_WriteInt32(writer, info->gameMode);
    Str_Write(&saveInfo->name, writer);

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
    Writer_WriteInt32(writer, saveInfo->gameId);
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

void SaveInfo_Read(SaveInfo* saveInfo, Reader* reader)
{
    saveheader_t* info;
    DENG_ASSERT(saveInfo);

    info = &saveInfo->header;
    info->magic = Reader_ReadInt32(reader);
    info->version = Reader_ReadInt32(reader);
    info->gameMode = (gamemode_t)Reader_ReadInt32(reader);

    if(info->version >= 10)
    {
        Str_Read(&saveInfo->name, reader);
    }
    else
    {
        // Older formats use a fixed-length name (24 characters).
#define OLD_NAME_LENGTH         24
        char buf[OLD_NAME_LENGTH];
        Reader_Read(reader, buf, OLD_NAME_LENGTH);
        Str_Set(&saveInfo->name, buf);
#undef OLD_NAME_LENGTH
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

    saveInfo->gameId = Reader_ReadInt32(reader);

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
void SaveInfo_Read_Hx_v9(SaveInfo* saveInfo, Reader* reader)
{
# define HXS_VERSION_TEXT       "HXS Ver " // Do not change me!
# define HXS_VERSION_TEXT_LENGTH 16
# define HXS_NAME_LENGTH        24

    char verText[HXS_VERSION_TEXT_LENGTH], nameBuffer[HXS_NAME_LENGTH];
    saveheader_t* info;
    DENG_ASSERT(saveInfo);

    info = &saveInfo->header;
    Reader_Read(reader, nameBuffer, HXS_NAME_LENGTH);
    Str_Set(&saveInfo->name, nameBuffer);
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
    saveInfo->gameId  = 0; // None.

# undef HXS_NAME_LENGTH
# undef HXS_VERSION_TEXT_LENGTH
# undef HXS_VERSION_TEXT
}
#endif
