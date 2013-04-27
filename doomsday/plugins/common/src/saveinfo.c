/** @file common/saveinfo.c Save state info.
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

#include <stdlib.h>
#include <string.h>

#include "common.h"

#include <de/memory.h>

#include "p_tick.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include "saveinfo.h"

SaveInfo *SaveInfo_New(void)
{
    SaveInfo *info = (SaveInfo *)M_Malloc(sizeof *info);

    Str_Init(&info->name);
    info->gameId = 0;
    memset(&info->header, 0, sizeof(info->header));

    return info;
}

SaveInfo *SaveInfo_NewCopy(SaveInfo const *other)
{
    return SaveInfo_Copy(SaveInfo_New(), other);
}

void SaveInfo_Delete(SaveInfo *info)
{
    DENG_ASSERT(info != 0);
    Str_Free(&info->name);
    M_Free(info);
}

SaveInfo *SaveInfo_Copy(SaveInfo *info, SaveInfo const *other)
{
    DENG_ASSERT(info != 0 && other != 0);

    Str_Copy(&info->name, SaveInfo_Name(other));
    info->gameId = SaveInfo_GameId(other);
    memcpy(&info->header, SaveInfo_Header(other), sizeof(info->header));

    return info;
}

uint SaveInfo_GameId(SaveInfo const *info)
{
    DENG_ASSERT(info != 0);
    return info->gameId;
}

saveheader_t const *SaveInfo_Header(SaveInfo const *info)
{
    DENG_ASSERT(info != 0);
    return &info->header;
}

Str const *SaveInfo_Name(SaveInfo const *info)
{
    DENG_ASSERT(info != 0);
    return &info->name;
}

void SaveInfo_SetGameId(SaveInfo *info, uint newGameId)
{
    DENG_ASSERT(info != 0);
    info->gameId = newGameId;
}

void SaveInfo_SetName(SaveInfo *info, Str const *newName)
{
    DENG_ASSERT(info != 0);
    Str_CopyOrClear(&info->name, newName);
}

void SaveInfo_Configure(SaveInfo *info)
{
    saveheader_t *hdr;
    DENG_ASSERT(info != 0);

    hdr = &info->header;

    hdr->magic      = IS_NETWORK_CLIENT? MY_CLIENT_SAVE_MAGIC : MY_SAVE_MAGIC;
    hdr->version    = MY_SAVE_VERSION;
    hdr->gameMode   = gameMode;

    hdr->map        = gameMap+1;
#if __JHEXEN__
    hdr->episode    = 1;
#else
    hdr->episode    = gameEpisode+1;
#endif
#if __JHEXEN__
    hdr->skill      = gameSkill;
    hdr->randomClasses = randomClassParm;
#else
    hdr->skill      = gameSkill;
    if(fastParm) hdr->skill |= 0x80; // Set high byte.
#endif
    hdr->deathmatch = deathmatch;
    hdr->noMonsters = noMonstersParm;

#if __JHEXEN__
    hdr->randomClasses = randomClassParm;
#else
    hdr->respawnMonsters = respawnMonsters;
    hdr->mapTime    = mapTime;

    { int i;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        hdr->players[i] = players[i].plr->inGame;
    }}
#endif
}

boolean SaveInfo_IsLoadable(SaveInfo *info)
{
    DENG_ASSERT(info != 0);

    // Game Mode missmatch?
    if(info->header.gameMode != gameMode) return false;

    /// @todo Validate loaded add-ons and checksum the definition database.

    return true; // It's good!
}

void SaveInfo_Write(SaveInfo *info, Writer *writer)
{
    saveheader_t *hdr;
    DENG_ASSERT(info != 0);

    hdr = &info->header;
    Writer_WriteInt32(writer, hdr->magic);
    Writer_WriteInt32(writer, hdr->version);
    Writer_WriteInt32(writer, hdr->gameMode);
    Str_Write(&info->name, writer);

    Writer_WriteByte(writer, hdr->skill);
    Writer_WriteByte(writer, hdr->episode);
    Writer_WriteByte(writer, hdr->map);
    Writer_WriteByte(writer, hdr->deathmatch);
    Writer_WriteByte(writer, hdr->noMonsters);
#if __JHEXEN__
    Writer_WriteByte(writer, hdr->randomClasses);
#else
    Writer_WriteByte(writer, hdr->respawnMonsters);
    Writer_WriteInt32(writer, hdr->mapTime);

    { int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        Writer_WriteByte(writer, hdr->players[i]);
    }}
#endif
    Writer_WriteInt32(writer, info->gameId);
}

#if __JDOOM__ || __JHERETIC__
static void translateLegacyGameMode(gamemode_t *mode, int saveVersion)
{
    static gamemode_t const oldGameModes[] = {
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

    DENG_ASSERT(mode != 0);

    // Is translation unnecessary?
#if __JDOOM__
    if(saveVersion >= 9) return;
#elif __JHERETIC__
    if(saveVersion >= 8) return;
#endif

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

void SaveInfo_Read(SaveInfo *info, Reader *reader)
{
    saveheader_t *hdr;
    DENG_ASSERT(info != 0);

    hdr = &info->header;
    hdr->magic = Reader_ReadInt32(reader);
    hdr->version = Reader_ReadInt32(reader);
    hdr->gameMode = (gamemode_t)Reader_ReadInt32(reader);

    if(hdr->version >= 10)
    {
        Str_Read(&info->name, reader);
    }
    else
    {
        // Older formats use a fixed-length name (24 characters).
#define OLD_NAME_LENGTH         24
        char buf[OLD_NAME_LENGTH];
        Reader_Read(reader, buf, OLD_NAME_LENGTH);
        Str_Set(&info->name, buf);
#undef OLD_NAME_LENGTH
    }

    hdr->skill          = Reader_ReadByte(reader);
    hdr->episode        = Reader_ReadByte(reader);
    hdr->map            = Reader_ReadByte(reader);
    hdr->deathmatch     = Reader_ReadByte(reader);
    hdr->noMonsters     = Reader_ReadByte(reader);
#if __JHEXEN__
    hdr->randomClasses  = Reader_ReadByte(reader);
#endif

#if !__JHEXEN__
    hdr->respawnMonsters = Reader_ReadByte(reader);

    // Older formats serialize the unpacked saveheader_t struct; skip the junk values (alignment).
    if(hdr->version < 10) SV_Seek(2);

    hdr->mapTime        = Reader_ReadInt32(reader);

    { int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hdr->players[i] = Reader_ReadByte(reader);
    }}
#endif

    info->gameId        = Reader_ReadInt32(reader);

#if __JDOOM__ || __JHERETIC__
    // Translate gameMode identifiers from older save versions.
    translateLegacyGameMode(&hdr->gameMode, hdr->version);
#endif
}

#if __JHEXEN__
void SaveInfo_Read_Hx_v9(SaveInfo *info, Reader *reader)
{
# define HXS_VERSION_TEXT           "HXS Ver " // Do not change me!
# define HXS_VERSION_TEXT_LENGTH    16
# define HXS_NAME_LENGTH            24

    char verText[HXS_VERSION_TEXT_LENGTH], nameBuffer[HXS_NAME_LENGTH];
    saveheader_t *hdr;

    DENG_ASSERT(info != 0);

    hdr = &info->header;

    Reader_Read(reader, nameBuffer, HXS_NAME_LENGTH);
    Str_Set(&info->name, nameBuffer);

    Reader_Read(reader, &verText, HXS_VERSION_TEXT_LENGTH);
    hdr->version = atoi(&verText[8]);

    /*Skip junk*/ SV_Seek(4);

    hdr->episode        = 1;
    hdr->map            = Reader_ReadByte(reader);
    hdr->skill          = Reader_ReadByte(reader);
    hdr->deathmatch     = Reader_ReadByte(reader);
    hdr->noMonsters     = Reader_ReadByte(reader);
    hdr->randomClasses  = Reader_ReadByte(reader);

    hdr->magic          = MY_SAVE_MAGIC; // Lets pretend...

    /// @note Older formats do not contain all needed values:
    hdr->gameMode       = gameMode; // Assume the current mode.

    info->gameId  = 0; // None.

# undef HXS_NAME_LENGTH
# undef HXS_VERSION_TEXT_LENGTH
# undef HXS_VERSION_TEXT
}
#endif
