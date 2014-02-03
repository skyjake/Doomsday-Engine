/** @file saveinfo.cpp  Saved game session info.
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
#include "saveinfo.h"

#include "p_tick.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include <de/memory.h>
#include <cstdlib>
#include <cstring>

SaveInfo::SaveInfo()
    : _gameId(0)
{
    Str_InitStd(&_description);
    memset(&_header, 0, sizeof(_header));
}

SaveInfo::SaveInfo(SaveInfo const &other)
    : _gameId(other._gameId)
{
    Str_Copy(Str_InitStd(&_description), &other._description);
    std::memcpy(&_header, &other._header, sizeof(_header));
}

SaveInfo::~SaveInfo()
{
    Str_Free(&_description);
}

SaveInfo &SaveInfo::operator = (SaveInfo const &other)
{
    Str_Copy(&_description, &other._description);
    _gameId = other._gameId;
    std::memcpy(&_header, &other._header, sizeof(_header));
    return *this;
}

int SaveInfo::version() const
{
    return _header.version;
}

int SaveInfo::magic() const
{
    return _header.magic;
}

Str const *SaveInfo::description() const
{
    return &_description;
}

void SaveInfo::setDescription(Str const *newName)
{
    Str_CopyOrClear(&_description, newName);
}

uint SaveInfo::gameId() const
{
    return _gameId;
}

void SaveInfo::setGameId(uint newGameId)
{
    _gameId = newGameId;
}

uint SaveInfo::episode() const
{
    return _header.episode - 1;
}

uint SaveInfo::map() const
{
    return _header.map - 1;
}

#if !__JHEXEN__
int SaveInfo::mapTime() const
{
    return _header.mapTime;
}
#endif

gamerules_t const &SaveInfo::gameRules() const
{
    return _header.gameRules;
}

void SaveInfo::configure()
{
    saveheader_t *hdr = &_header;

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
    hdr->gameRules.skill         = gameSkill;
    hdr->gameRules.randomClasses = randomClassParm;
#else
    hdr->gameRules.skill         = gameSkill;
    hdr->gameRules.fast          = fastParm;
#endif
    hdr->gameRules.deathmatch = deathmatch;
    hdr->gameRules.noMonsters = noMonstersParm;

#if __JHEXEN__
    hdr->gameRules.randomClasses = randomClassParm;
#else
    hdr->gameRules.respawnMonsters = respawnMonsters;
#endif

#if !__JHEXEN__
    hdr->mapTime    = ::mapTime;

    for(int i = 0; i < MAXPLAYERS; i++)
    {
        hdr->players[i] = players[i].plr->inGame;
    }
#endif
}

bool SaveInfo::isLoadable()
{
    // Game Mode missmatch?
    if(_header.gameMode != gameMode) return false;

    /// @todo Validate loaded add-ons and checksum the definition database.

    return true; // It's good!
}

void SaveInfo::write(Writer *writer) const
{
    saveheader_t const *hdr = &_header;
    Writer_WriteInt32(writer, hdr->magic);
    Writer_WriteInt32(writer, hdr->version);
    Writer_WriteInt32(writer, hdr->gameMode);
    Str_Write(&_description, writer);

    Writer_WriteByte(writer, hdr->gameRules.skill & 0x7f);
    Writer_WriteByte(writer, hdr->episode);
    Writer_WriteByte(writer, hdr->map);
    Writer_WriteByte(writer, hdr->gameRules.deathmatch);
#if !__JHEXEN__
    Writer_WriteByte(writer, hdr->gameRules.fast);
#endif
    Writer_WriteByte(writer, hdr->gameRules.noMonsters);
#if __JHEXEN__
    Writer_WriteByte(writer, hdr->gameRules.randomClasses);
#else
    Writer_WriteByte(writer, hdr->gameRules.respawnMonsters);
#endif

#if !__JHEXEN__
    Writer_WriteInt32(writer, hdr->mapTime);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        Writer_WriteByte(writer, hdr->players[i]);
    }
#endif
    Writer_WriteInt32(writer, _gameId);
}

#if __JDOOM__ || __JHERETIC__
static void translateLegacyGameMode(gamemode_t *mode, int saveVersion)
{
    DENG_ASSERT(mode != 0);

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

void SaveInfo::read(Reader *reader)
{
    saveheader_t *hdr = &_header;

    hdr->magic    = Reader_ReadInt32(reader);
    hdr->version  = Reader_ReadInt32(reader);
    hdr->gameMode = (gamemode_t)Reader_ReadInt32(reader);

    if(hdr->version >= 10)
    {
        Str_Read(&_description, reader);
    }
    else
    {
        // Older formats use a fixed-length name (24 characters).
#define OLD_NAME_LENGTH 24

        char buf[OLD_NAME_LENGTH];
        Reader_Read(reader, buf, OLD_NAME_LENGTH);
        Str_Set(&_description, buf);

#undef OLD_NAME_LENGTH
    }

#if !__JHEXEN__
    if(hdr->version < 13)
    {
        // In DOOM the high bit of the skill mode byte is also used for the
        // "fast" game rule dd_bool. There is more confusion in that SM_NOTHINGS
        // will result in 0xff and thus always set the fast bit.
        //
        // Here we decipher this assuming that if the skill mode is invalid then
        // by default this means "spawn no things" and if so then the "fast" game
        // rule is meaningless so it is forced off.
        byte skillModePlusFastBit = Reader_ReadByte(reader);
        hdr->gameRules.skill = (skillmode_t) (skillModePlusFastBit & 0x7f);
        if(hdr->gameRules.skill < SM_BABY || hdr->gameRules.skill >= NUM_SKILL_MODES)
        {
            hdr->gameRules.skill = SM_NOTHINGS;
            hdr->gameRules.fast  = 0;
        }
        else
        {
            hdr->gameRules.fast  = (skillModePlusFastBit & 0x80) != 0;
        }
    }
    else
#endif
    {
        hdr->gameRules.skill = skillmode_t( Reader_ReadByte(reader) & 0x7f );

        // Interpret skill levels outside the normal range as "spawn no things".
        if(hdr->gameRules.skill < SM_BABY || hdr->gameRules.skill >= NUM_SKILL_MODES)
        {
            hdr->gameRules.skill = SM_NOTHINGS;
        }
    }

    hdr->episode        = Reader_ReadByte(reader);
    hdr->map            = Reader_ReadByte(reader);

    hdr->gameRules.deathmatch      = Reader_ReadByte(reader);
#if !__JHEXEN__
    if(hdr->version >= 13)
        hdr->gameRules.fast        = Reader_ReadByte(reader);
#endif
    hdr->gameRules.noMonsters      = Reader_ReadByte(reader);
#if __JHEXEN__
    hdr->gameRules.randomClasses   = Reader_ReadByte(reader);
#endif

#if !__JHEXEN__
    hdr->gameRules.respawnMonsters = Reader_ReadByte(reader);

    // Older formats serialize the unpacked saveheader_t struct; skip the junk values (alignment).
    if(hdr->version < 10) SV_Seek(2);

    hdr->mapTime        = Reader_ReadInt32(reader);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        hdr->players[i] = Reader_ReadByte(reader);
    }
#endif

    _gameId = Reader_ReadInt32(reader);

#if __JDOOM__ || __JHERETIC__
    // Translate gameMode identifiers from older save versions.
    translateLegacyGameMode(&hdr->gameMode, hdr->version);
#endif
}

#if __JHEXEN__
void SaveInfo::read_Hx_v9(Reader *reader)
{
# define HXS_VERSION_TEXT           "HXS Ver " // Do not change me!
# define HXS_VERSION_TEXT_LENGTH    16
# define HXS_NAME_LENGTH            24

    char verText[HXS_VERSION_TEXT_LENGTH];
    char nameBuffer[HXS_NAME_LENGTH];

    saveheader_t *hdr = &_header;

    Reader_Read(reader, nameBuffer, HXS_NAME_LENGTH);
    Str_Set(&_description, nameBuffer);

    Reader_Read(reader, &verText, HXS_VERSION_TEXT_LENGTH);
    hdr->version  = atoi(&verText[8]);

    /*Skip junk*/ SV_Seek(4);

    hdr->episode  = 1;
    hdr->map      = Reader_ReadByte(reader);
    hdr->magic    = MY_SAVE_MAGIC; // Lets pretend...
    hdr->gameMode = gameMode; // Assume the current mode.

    hdr->gameRules.skill         = (skillmode_t) (Reader_ReadByte(reader) & 0x7f);

    // Interpret skill modes outside the normal range as "spawn no things".
    if(hdr->gameRules.skill < SM_BABY || hdr->gameRules.skill >= NUM_SKILL_MODES)
        hdr->gameRules.skill = SM_NOTHINGS;

    hdr->gameRules.deathmatch    = Reader_ReadByte(reader);
    hdr->gameRules.noMonsters    = Reader_ReadByte(reader);
    hdr->gameRules.randomClasses = Reader_ReadByte(reader);

    _gameId  = 0; // None.

# undef HXS_NAME_LENGTH
# undef HXS_VERSION_TEXT_LENGTH
# undef HXS_VERSION_TEXT
}
#endif

SaveInfo *SaveInfo_New()
{
    return new SaveInfo;
}

SaveInfo *SaveInfo_Dup(SaveInfo const *other)
{
    DENG_ASSERT(other != 0);
    return new SaveInfo(*other);
}

void SaveInfo_Delete(SaveInfo *info)
{
    if(info) delete info;
}

SaveInfo *SaveInfo_Copy(SaveInfo *info, SaveInfo const *other)
{
    DENG_ASSERT(info != 0 && other != 0);
    *info = *other;
    return info;
}

uint SaveInfo_GameId(SaveInfo const *info)
{
    DENG_ASSERT(info != 0);
    return info->gameId();
}

void SaveInfo_SetGameId(SaveInfo *info, uint newGameId)
{
    DENG_ASSERT(info != 0);
    info->setGameId(newGameId);
}

Str const *SaveInfo_Description(SaveInfo const *info)
{
    DENG_ASSERT(info != 0);
    return info->description();
}

void SaveInfo_SetDescription(SaveInfo *info, Str const *newName)
{
    DENG_ASSERT(info != 0);
    info->setDescription(newName);
}

dd_bool SaveInfo_IsLoadable(SaveInfo *info)
{
    DENG_ASSERT(info != 0);
    return info->isLoadable();
}

void SaveInfo_Write(SaveInfo *info, Writer *writer)
{
    DENG_ASSERT(info != 0);
    info->write(writer);
}

void SaveInfo_Read(SaveInfo *info, Reader *reader)
{
    DENG_ASSERT(info != 0);
    info->read(reader);
}

#if __JHEXEN__
void SaveInfo_Read_Hx_v9(SaveInfo *info, Reader *reader)
{
    DENG_ASSERT(info != 0);
    info->read_Hx_v9(reader);
}
#endif
