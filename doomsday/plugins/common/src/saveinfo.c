/**
 * @file saveinfo.c
 * Save state info.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include <string.h>

#include "common.h"
#include "p_tick.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include "saveinfo.h"

void SaveInfo_SetFilePath(saveinfo_t* info, ddstring_t* newFilePath)
{
    assert(info);
    Str_CopyOrClear(&info->filePath, newFilePath);
}

void SaveInfo_SetGameId(saveinfo_t* info, uint newGameId)
{
    assert(info);
    info->header.gameId = newGameId;
}

void SaveInfo_SetName(saveinfo_t* info, const char* newName)
{
    assert(info);
    dd_snprintf(info->header.name, SAVESTRINGSIZE, "%s", newName);
}

void SaveInfo_Configure(saveinfo_t* info)
{
    saveheader_t* hdr;
    assert(info);

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

static boolean saveInfoIsValidForCurrentGameSession(saveinfo_t* info)
{
    assert(info);

    /// @fixme What about original game saves, they will fail here presently.

    // Magic must match.
    if(info->header.magic != MY_SAVE_MAGIC) return false;

    /**
     * Check for unsupported versions.
     */
    // A future version?
    if(info->header.version > MY_SAVE_VERSION) return false;

#if __JHEXEN__
    // We are incompatible with v3 saves due to an invalid test used to determine
    // present sidedefs (ver3 format's sidedefs contain chunks of junk data).
    if(info->header.version == 3) return false;
#endif

    // Game Mode missmatch?
    if(info->header.gameMode != gameMode) return false;

    return true; // It's good!
}

static boolean recogniseAndReadHeader(saveinfo_t* info)
{
    if(SV_Recognise(info)) return true;

    // Perhaps an original game save?
#if __JDOOM__
    if(SV_v19_Recognise(info)) return true;
#endif
#if __JHERETIC__
    if(SV_v13_Recognise(info)) return true;
#endif

    return false;
}

void SaveInfo_Update(saveinfo_t* info)
{
    assert(info);

    if(Str_IsEmpty(&info->filePath))
    {
        // The save path cannot be accessed for some reason. Perhaps its a
        // network path? Clear the info for this slot.
        Str_Clear(&info->name);
        return;
    }

    if(!recogniseAndReadHeader(info))
    {
        // Not a loadable save.
        Str_Clear(&info->filePath);
        return;
    }

    // Ensure we have a valid name.
    if(Str_IsEmpty(&info->name))
    {
        Str_Set(&info->name, "UNNAMED");
    }

    if(!saveInfoIsValidForCurrentGameSession(info))
    {
        // Not a loadable save.
        Str_Clear(&info->filePath);
    }
}

void SaveInfo_Write(saveinfo_t* saveInfo, Writer* writer)
{
    saveheader_t* info;
    assert(saveInfo);

    info = &saveInfo->header;
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

void SaveInfo_Read(saveinfo_t* saveInfo, Reader* reader)
{
    saveheader_t* info;
    assert(saveInfo);

    info = &saveInfo->header;
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
void SaveInfo_Read_Hx_v9(saveinfo_t* saveInfo, Reader* reader)
{
# define HXS_VERSION_TEXT      "HXS Ver " // Do not change me!
# define HXS_VERSION_TEXT_LENGTH 16

    char verText[HXS_VERSION_TEXT_LENGTH];
    saveheader_t* info;
    assert(saveInfo);

    info = &saveInfo->header;
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
