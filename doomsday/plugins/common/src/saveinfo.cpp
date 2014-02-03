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
    : _gameId  (0)
    , _magic   (0)
    , _version (0)
    , _gameMode(NUM_GAME_MODES)
    , _episode (0)
    , _map     (0)
#if !__JHEXEN__
    , _mapTime (0)
#endif
{
    Str_InitStd(&_description);
#if !__JHEXEN__
    memset(&_players, 0, sizeof(_players));
#endif
    memset(&_gameRules, 0, sizeof(_gameRules));
}

SaveInfo::SaveInfo(SaveInfo const &other)
    : _gameId  (other._gameId)
    , _magic   (other._magic)
    , _version (other._version)
    , _gameMode(other._gameMode)
    , _episode (other._episode)
    , _map     (other._map)
#if !__JHEXEN__
    , _mapTime (other._mapTime)
#endif
{
    Str_Copy(Str_InitStd(&_description), &other._description);
#if !__JHEXEN__
    std::memcpy(&_players, &other._players, sizeof(_players));
#endif
    std::memcpy(&_gameRules, &other._gameRules, sizeof(_gameRules));
}

SaveInfo::~SaveInfo()
{
    Str_Free(&_description);
}

SaveInfo &SaveInfo::operator = (SaveInfo const &other)
{
    Str_Copy(&_description, &other._description);
    _gameId = other._gameId;
    _magic = other._magic;
    _version = other._version;
    _gameMode = other._gameMode;
    _episode = other._episode;
    _map = other._map;
#if !__JHEXEN__
    _mapTime = other._mapTime;
    std::memcpy(&_players, &other._players, sizeof(_players));
#endif
    std::memcpy(&_gameRules, &other._gameRules, sizeof(_gameRules));
    return *this;
}

int SaveInfo::version() const
{
    return _version;
}

int SaveInfo::magic() const
{
    return _magic;
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
    return _episode - 1;
}

uint SaveInfo::map() const
{
    return _map - 1;
}

#if !__JHEXEN__
int SaveInfo::mapTime() const
{
    return _mapTime;
}
#endif

gamerules_t const &SaveInfo::gameRules() const
{
    return _gameRules;
}

void SaveInfo::configure()
{
    _magic      = IS_NETWORK_CLIENT? MY_CLIENT_SAVE_MAGIC : MY_SAVE_MAGIC;
    _version    = MY_SAVE_VERSION;
    _gameMode   = gameMode;

    _map        = gameMap+1;
#if __JHEXEN__
    _episode    = 1;
#else
    _episode    = gameEpisode+1;
#endif

#if __JHEXEN__
    _gameRules.skill         = gameSkill;
    _gameRules.randomClasses = randomClassParm;
#else
    _gameRules.skill         = gameSkill;
    _gameRules.fast          = fastParm;
#endif
    _gameRules.deathmatch = deathmatch;
    _gameRules.noMonsters = noMonstersParm;

#if __JHEXEN__
    _gameRules.randomClasses = randomClassParm;
#else
    _gameRules.respawnMonsters = respawnMonsters;
#endif

#if !__JHEXEN__
    _mapTime    = ::mapTime;

    for(int i = 0; i < MAXPLAYERS; i++)
    {
        _players[i] = players[i].plr->inGame;
    }
#endif
}

bool SaveInfo::isLoadable()
{
    // Game Mode missmatch?
    if(_gameMode != gameMode) return false;

    /// @todo Validate loaded add-ons and checksum the definition database.

    return true; // It's good!
}

void SaveInfo::write(Writer *writer) const
{
    Writer_WriteInt32(writer, _magic);
    Writer_WriteInt32(writer, _version);
    Writer_WriteInt32(writer, _gameMode);
    Str_Write(&_description, writer);

    Writer_WriteByte(writer, _gameRules.skill & 0x7f);
    Writer_WriteByte(writer, _episode);
    Writer_WriteByte(writer, _map);
    Writer_WriteByte(writer, _gameRules.deathmatch);
#if !__JHEXEN__
    Writer_WriteByte(writer, _gameRules.fast);
#endif
    Writer_WriteByte(writer, _gameRules.noMonsters);
#if __JHEXEN__
    Writer_WriteByte(writer, _gameRules.randomClasses);
#else
    Writer_WriteByte(writer, _gameRules.respawnMonsters);
#endif

#if !__JHEXEN__
    Writer_WriteInt32(writer, _mapTime);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        Writer_WriteByte(writer, _players[i]);
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
    _magic    = Reader_ReadInt32(reader);
    _version  = Reader_ReadInt32(reader);
    _gameMode = (gamemode_t)Reader_ReadInt32(reader);

    if(_version >= 10)
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
    if(_version < 13)
    {
        // In DOOM the high bit of the skill mode byte is also used for the
        // "fast" game rule dd_bool. There is more confusion in that SM_NOTHINGS
        // will result in 0xff and thus always set the fast bit.
        //
        // Here we decipher this assuming that if the skill mode is invalid then
        // by default this means "spawn no things" and if so then the "fast" game
        // rule is meaningless so it is forced off.
        byte skillModePlusFastBit = Reader_ReadByte(reader);
        _gameRules.skill = (skillmode_t) (skillModePlusFastBit & 0x7f);
        if(_gameRules.skill < SM_BABY || _gameRules.skill >= NUM_SKILL_MODES)
        {
            _gameRules.skill = SM_NOTHINGS;
            _gameRules.fast  = 0;
        }
        else
        {
            _gameRules.fast  = (skillModePlusFastBit & 0x80) != 0;
        }
    }
    else
#endif
    {
        _gameRules.skill = skillmode_t( Reader_ReadByte(reader) & 0x7f );

        // Interpret skill levels outside the normal range as "spawn no things".
        if(_gameRules.skill < SM_BABY || _gameRules.skill >= NUM_SKILL_MODES)
        {
            _gameRules.skill = SM_NOTHINGS;
        }
    }

    _episode        = Reader_ReadByte(reader);
    _map            = Reader_ReadByte(reader);

    _gameRules.deathmatch      = Reader_ReadByte(reader);
#if !__JHEXEN__
    if(_version >= 13)
        _gameRules.fast        = Reader_ReadByte(reader);
#endif
    _gameRules.noMonsters      = Reader_ReadByte(reader);
#if __JHEXEN__
    _gameRules.randomClasses   = Reader_ReadByte(reader);
#endif

#if !__JHEXEN__
    _gameRules.respawnMonsters = Reader_ReadByte(reader);

    // Older formats serialize the unpacked saveheader_t struct; skip the junk values (alignment).
    if(_version < 10) SV_Seek(2);

    _mapTime        = Reader_ReadInt32(reader);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        _players[i] = Reader_ReadByte(reader);
    }
#endif

    _gameId = Reader_ReadInt32(reader);

#if __JDOOM__ || __JHERETIC__
    // Translate gameMode identifiers from older save versions.
    translateLegacyGameMode(&_gameMode, _version);
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

    Reader_Read(reader, nameBuffer, HXS_NAME_LENGTH);
    Str_Set(&_description, nameBuffer);

    Reader_Read(reader, &verText, HXS_VERSION_TEXT_LENGTH);
    _version  = atoi(&verText[8]);

    /*Skip junk*/ SV_Seek(4);

    _episode  = 1;
    _map      = Reader_ReadByte(reader);
    _magic    = MY_SAVE_MAGIC; // Lets pretend...
    _gameMode = gameMode; // Assume the current mode.

    _gameRules.skill         = (skillmode_t) (Reader_ReadByte(reader) & 0x7f);

    // Interpret skill modes outside the normal range as "spawn no things".
    if(_gameRules.skill < SM_BABY || _gameRules.skill >= NUM_SKILL_MODES)
        _gameRules.skill = SM_NOTHINGS;

    _gameRules.deathmatch    = Reader_ReadByte(reader);
    _gameRules.noMonsters    = Reader_ReadByte(reader);
    _gameRules.randomClasses = Reader_ReadByte(reader);

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
