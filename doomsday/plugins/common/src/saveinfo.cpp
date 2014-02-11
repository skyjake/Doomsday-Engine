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

#include "g_common.h"
#include "p_tick.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include <de/memory.h>
#include <cstdlib>
#include <cstring>

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

SaveInfo::SaveInfo()
    : _sessionId(0)
    , _magic    (0)
    , _version  (0)
    , _gameMode (NUM_GAME_MODES)
    , _mapUri   (Uri_New())
#if !__JHEXEN__
    , _mapTime  (0)
#endif
{
    Str_InitStd(&_description);
#if !__JHEXEN__
    de::zap(_players);
#endif
    de::zap(_gameRules);
}

SaveInfo::SaveInfo(SaveInfo const &other)
    : _sessionId(other._sessionId)
    , _magic    (other._magic)
    , _version  (other._version)
    , _gameMode (other._gameMode)
#if !__JHEXEN__
    , _mapTime  (other._mapTime)
#endif
{
    Str_Copy(Str_InitStd(&_description), &other._description);
    Uri_Copy(_mapUri, other._mapUri);
#if !__JHEXEN__
    std::memcpy(&_players, &other._players, sizeof(_players));
#endif
    std::memcpy(&_gameRules, &other._gameRules, sizeof(_gameRules));
}

SaveInfo::~SaveInfo()
{
    Str_Free(&_description);
    Uri_Delete(_mapUri);
}

SaveInfo *SaveInfo::newWithCurrentSessionMetadata(Str const *description) // static
{
    SaveInfo *info = new SaveInfo;
    info->setDescription(description);
    info->applyCurrentSessionMetadata();
    info->setSessionId(G_GenerateSessionId());
    return info;
}

SaveInfo &SaveInfo::operator = (SaveInfo const &other)
{
    Str_Copy(&_description, &other._description);
    _sessionId = other._sessionId;
    _magic = other._magic;
    _version = other._version;
    _gameMode = other._gameMode;
    Uri_Copy(_mapUri, other._mapUri);
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

uint SaveInfo::sessionId() const
{
    return _sessionId;
}

void SaveInfo::setSessionId(uint newGameId)
{
    _sessionId = newGameId;
}

Uri const *SaveInfo::mapUri() const
{
    return _mapUri;
}

#if !__JHEXEN__
int SaveInfo::mapTime() const
{
    return _mapTime;
}
#endif

GameRuleset const &SaveInfo::gameRules() const
{
    return _gameRules;
}

void SaveInfo::applyCurrentSessionMetadata()
{
    _magic    = IS_NETWORK_CLIENT? MY_CLIENT_SAVE_MAGIC : MY_SAVE_MAGIC;
    _version  = MY_SAVE_VERSION;
    _gameMode = gameMode;
    Uri_Copy(_mapUri, gameMapUri);
#if !__JHEXEN__
    _mapTime  = ::mapTime;
#endif

    // Make a copy of the current game rules.
    _gameRules = ::gameRules;

#if !__JHEXEN__
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

    Uri_Write(_mapUri, writer);
#if !__JHEXEN__
    Writer_WriteInt32(writer, _mapTime);
#endif
    GameRuleset_Write(&_gameRules, writer);

#if !__JHEXEN__
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        Writer_WriteByte(writer, _players[i]);
    }
#endif

    Writer_WriteInt32(writer, _sessionId);
}

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
        // Description is a fixed 24 characters in length.
        char buf[24 + 1];
        Reader_Read(reader, buf, 24); buf[24] = 0;
        Str_Set(&_description, buf);
    }

    if(_version >= 14)
    {
        Uri_Read(_mapUri, reader);
#if !__JHEXEN__
        _mapTime = Reader_ReadInt32(reader);
#endif

        GameRuleset_Read(&_gameRules, reader);
    }
    else
    {
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
                _gameRules.skill = SM_NOTHINGS;
        }

        uint episode = Reader_ReadByte(reader) - 1;
        uint map     = Reader_ReadByte(reader) - 1;
        Uri_Copy(_mapUri, G_ComposeMapUri(episode, map));

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
#endif
#if !__JHEXEN__
        // Older formats serialize the unpacked saveheader_t struct; skip the junk values (alignment).
        if(_version < 10) SV_Seek(2);

        _mapTime = Reader_ReadInt32(reader);
#endif
    }

#if !__JHEXEN__
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        _players[i] = Reader_ReadByte(reader);
    }
#endif

    _sessionId = Reader_ReadInt32(reader);

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

    _magic    = MY_SAVE_MAGIC; // Lets pretend...
    _gameMode = gameMode; // Assume the current mode.

    uint episode = 0;
    uint map     = Reader_ReadByte(reader) - 1;
    Uri_Copy(_mapUri, G_ComposeMapUri(episode, map));

    _gameRules.skill         = (skillmode_t) (Reader_ReadByte(reader) & 0x7f);
    // Interpret skill modes outside the normal range as "spawn no things".
    if(_gameRules.skill < SM_BABY || _gameRules.skill >= NUM_SKILL_MODES)
    {
        _gameRules.skill = SM_NOTHINGS;
    }

    _gameRules.deathmatch    = Reader_ReadByte(reader);
    _gameRules.noMonsters    = Reader_ReadByte(reader);
    _gameRules.randomClasses = Reader_ReadByte(reader);

    _sessionId  = 0; // None.

# undef HXS_NAME_LENGTH
# undef HXS_VERSION_TEXT_LENGTH
# undef HXS_VERSION_TEXT
}
#endif

// C wrapper API ---------------------------------------------------------------

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
    return info->sessionId();
}

void SaveInfo_SetGameId(SaveInfo *info, uint newGameId)
{
    DENG_ASSERT(info != 0);
    info->setSessionId(newGameId);
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
