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
#include <cstdlib>
#include <cstring>

DENG2_PIMPL_NOREF(SaveInfo)
{
    Str description;
    uint sessionId;
    int magic;
    int version;
    Str gameIdentityKey;
    Uri *mapUri;
#if !__JHEXEN__
    int mapTime;
    Players players;
#endif
    GameRuleset gameRules;

    Instance()
        : sessionId(0)
        , magic    (0)
        , version  (0)
        , mapUri   (Uri_New())
#if !__JHEXEN__
        , mapTime  (0)
#endif
    {
        Str_InitStd(&description);
        Str_InitStd(&gameIdentityKey);
#if !__JHEXEN__
        de::zap(players);
#endif
        de::zap(gameRules);
    }

    Instance(Instance const &other)
        : IPrivate()
        , sessionId(other.sessionId)
        , magic    (other.magic)
        , version  (other.version)
        , mapUri   (Uri_Dup(other.mapUri))
#if !__JHEXEN__
        , mapTime  (other.mapTime)
#endif
    {
        Str_Copy(Str_InitStd(&description), &other.description);
        Str_Copy(Str_InitStd(&gameIdentityKey), &other.gameIdentityKey);
#if !__JHEXEN__
        std::memcpy(&players, &other.players, sizeof(players));
#endif
        std::memcpy(&gameRules, &other.gameRules, sizeof(gameRules));
    }

    ~Instance()
    {
        Str_Free(&description);
        Str_Free(&gameIdentityKey);
        Uri_Delete(mapUri);
    }

#if __JHEXEN__
    /**
     * Deserialize the legacy Hexen-specific v.9 info.
     */
    void read_Hx_v9(Reader *reader)
    {
        char descBuf[24 + 1];
        Reader_Read(reader, descBuf, 24); descBuf[24] = 0;
        Str_Set(&description, descBuf);

        magic     = MY_SAVE_MAGIC; // Lets pretend...

        char verText[16 + 1]; // "HXS Ver "
        Reader_Read(reader, &verText, 16); descBuf[16] = 0;
        version   = atoi(&verText[8]);

        /// @note Kludge: Assume the current game mode.
        GameInfo gameInfo;
        DD_GameInfo(&gameInfo);
        Str_Copy(&gameIdentityKey, gameInfo.identityKey);
        /// Kludge end.

        /*Skip junk*/ SV_Seek(4);

        uint episode = 0;
        uint map     = Reader_ReadByte(reader) - 1;
        Uri_Copy(mapUri, G_ComposeMapUri(episode, map));

        gameRules.skill         = (skillmode_t) (Reader_ReadByte(reader) & 0x7f);
        // Interpret skill modes outside the normal range as "spawn no things".
        if(gameRules.skill < SM_BABY || gameRules.skill >= NUM_SKILL_MODES)
        {
            gameRules.skill     = SM_NOTHINGS;
        }

        gameRules.deathmatch    = Reader_ReadByte(reader);
        gameRules.noMonsters    = Reader_ReadByte(reader);
        gameRules.randomClasses = Reader_ReadByte(reader);

        sessionId = 0; // None.
    }
#endif
};

SaveInfo::SaveInfo() : d(new Instance)
{}

SaveInfo::SaveInfo(SaveInfo const &other) : d(new Instance(*other.d))
{}

SaveInfo *SaveInfo::newWithCurrentSessionMetadata(Str const *description) // static
{
    SaveInfo *info = new SaveInfo;
    info->setDescription(description);
    info->applyCurrentSessionMetadata();
    info->setSessionId(G_GenerateSessionId());
    return info;
}

SaveInfo *SaveInfo::fromReader(Reader *reader) // static
{
    SaveInfo *info = new SaveInfo;
    info->read(reader);
    return info;
}

SaveInfo &SaveInfo::operator = (SaveInfo const &other)
{
    d.reset(new Instance(*other.d));
    return *this;
}

Str const *SaveInfo::gameIdentityKey() const
{
    return &d->gameIdentityKey;
}

void SaveInfo::setGameIdentityKey(Str const *newGameIdentityKey)
{
    Str_CopyOrClear(&d->gameIdentityKey, newGameIdentityKey);
}

int SaveInfo::version() const
{
    return d->version;
}

void SaveInfo::setVersion(int newVersion)
{
    d->version = newVersion;
}

Str const *SaveInfo::description() const
{
    return &d->description;
}

void SaveInfo::setDescription(Str const *newDescription)
{
    Str_CopyOrClear(&d->description, newDescription);
}

uint SaveInfo::sessionId() const
{
    return d->sessionId;
}

void SaveInfo::setSessionId(uint newSessionId)
{
    d->sessionId = newSessionId;
}

Uri const *SaveInfo::mapUri() const
{
    return d->mapUri;
}

void SaveInfo::setMapUri(Uri const *newMapUri)
{
    DENG_ASSERT(newMapUri != 0);
    Uri_Copy(d->mapUri, newMapUri);
}

#if !__JHEXEN__
int SaveInfo::mapTime() const
{
    return d->mapTime;
}

void SaveInfo::setMapTime(int newMapTime)
{
    d->mapTime = newMapTime;
}

SaveInfo::Players const &SaveInfo::players() const
{
    return d->players;
}

void SaveInfo::setPlayers(Players const &newPlayers)
{
    std::memcpy(d->players, newPlayers, sizeof(d->players));
}

#endif // !__JHEXEN__

GameRuleset const &SaveInfo::gameRules() const
{
    return d->gameRules;
}

void SaveInfo::setGameRules(GameRuleset const &newRules)
{
    d->gameRules = newRules; // make a copy
}

void SaveInfo::applyCurrentSessionMetadata()
{
    d->magic    = IS_NETWORK_CLIENT? MY_CLIENT_SAVE_MAGIC : MY_SAVE_MAGIC;
    d->version  = MY_SAVE_VERSION;
    GameInfo gameInfo;
    DD_GameInfo(&gameInfo);
    Str_Copy(&d->gameIdentityKey, gameInfo.identityKey);
    Uri_Copy(d->mapUri, gameMapUri);
#if !__JHEXEN__
    d->mapTime  = ::mapTime;
#endif

    // Make a copy of the current game rules.
    d->gameRules = ::gameRules;

#if !__JHEXEN__
    for(int i = 0; i < MAXPLAYERS; i++)
    {
        d->players[i] = (::players[i]).plr->inGame;
    }
#endif
}

bool SaveInfo::isLoadable()
{
    // Game identity key missmatch?
    GameInfo gameInfo;
    DD_GameInfo(&gameInfo);
    if(Str_Compare(gameInfo.identityKey, Str_Text(&d->gameIdentityKey)))
    {
        return false;
    }

    /// @todo Validate loaded add-ons and checksum the definition database.

    return true; // It's good!
}

void SaveInfo::updateFromFile(Str const *path)
{
    if(!path || Str_IsEmpty(path))
    {
        // The save path cannot be accessed for some reason. Perhaps its a network path?
        setDescription(0);
        setSessionId(0);
        return;
    }

    // Is this a recognized game state?
    if(!SV_RecognizeGameState(*this, path))
    {
        // Clear the info for this slot.
        setDescription(0);
        setSessionId(0);
        return;
    }

    // Ensure we have a valid description.
    if(Str_IsEmpty(description()))
    {
        setDescription(AutoStr_FromText("UNNAMED"));
    }
}

void SaveInfo::write(Writer *writer) const
{
    Writer_WriteInt32(writer, d->magic);
    Writer_WriteInt32(writer, d->version);
    Str_Write(&d->gameIdentityKey, writer);
    Str_Write(&d->description, writer);

    Uri_Write(d->mapUri, writer);
#if !__JHEXEN__
    Writer_WriteInt32(writer, d->mapTime);
#endif
    GameRuleset_Write(&d->gameRules, writer);

#if !__JHEXEN__
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        Writer_WriteByte(writer, d->players[i]);
    }
#endif

    Writer_WriteInt32(writer, d->sessionId);
}

void SaveInfo::read(Reader *reader)
{
#if __JHEXEN__
    // Read the magic byte to determine the high-level format.
    int magic = Reader_ReadInt32(reader);
    SV_HxSavePtr()->b -= 4; // Rewind the stream.

    if((!IS_NETWORK_CLIENT && magic != MY_SAVE_MAGIC) ||
       ( IS_NETWORK_CLIENT && magic != MY_CLIENT_SAVE_MAGIC))
    {
        // Perhaps the old v9 format?
        d->read_Hx_v9(reader);
        return;
    }
#endif

    d->magic    = Reader_ReadInt32(reader);
    d->version  = Reader_ReadInt32(reader);
    if(d->version >= 14)
    {
        Str_Read(&d->gameIdentityKey, reader);
    }
    else
    {
        // Translate gamemode identifiers from older save versions.
        int oldGamemode = Reader_ReadInt32(reader);
        Str_Copy(&d->gameIdentityKey, G_IdentityKeyForLegacyGamemode(oldGamemode, d->version));
    }

    if(d->version >= 10)
    {
        Str_Read(&d->description, reader);
    }
    else
    {
        // Description is a fixed 24 characters in length.
        char buf[24 + 1];
        Reader_Read(reader, buf, 24); buf[24] = 0;
        Str_Set(&d->description, buf);
    }

    if(d->version >= 14)
    {
        Uri_Read(d->mapUri, reader);
#if !__JHEXEN__
        d->mapTime = Reader_ReadInt32(reader);
#endif

        GameRuleset_Read(&d->gameRules, reader);
    }
    else
    {
#if !__JHEXEN__
        if(d->version < 13)
        {
            // In DOOM the high bit of the skill mode byte is also used for the
            // "fast" game rule dd_bool. There is more confusion in that SM_NOTHINGS
            // will result in 0xff and thus always set the fast bit.
            //
            // Here we decipher this assuming that if the skill mode is invalid then
            // by default this means "spawn no things" and if so then the "fast" game
            // rule is meaningless so it is forced off.
            byte skillModePlusFastBit = Reader_ReadByte(reader);
            d->gameRules.skill = (skillmode_t) (skillModePlusFastBit & 0x7f);
            if(d->gameRules.skill < SM_BABY || d->gameRules.skill >= NUM_SKILL_MODES)
            {
                d->gameRules.skill = SM_NOTHINGS;
                d->gameRules.fast  = 0;
            }
            else
            {
                d->gameRules.fast  = (skillModePlusFastBit & 0x80) != 0;
            }
        }
        else
#endif
        {
            d->gameRules.skill = skillmode_t( Reader_ReadByte(reader) & 0x7f );
            // Interpret skill levels outside the normal range as "spawn no things".
            if(d->gameRules.skill < SM_BABY || d->gameRules.skill >= NUM_SKILL_MODES)
            {
                d->gameRules.skill = SM_NOTHINGS;
            }
        }

        uint episode = Reader_ReadByte(reader) - 1;
        uint map     = Reader_ReadByte(reader) - 1;
        Uri_Copy(d->mapUri, G_ComposeMapUri(episode, map));

        d->gameRules.deathmatch      = Reader_ReadByte(reader);
#if !__JHEXEN__
        if(d->version >= 13)
        {
            d->gameRules.fast        = Reader_ReadByte(reader);
        }
#endif
        d->gameRules.noMonsters      = Reader_ReadByte(reader);
#if __JHEXEN__
        d->gameRules.randomClasses   = Reader_ReadByte(reader);
#endif
#if !__JHEXEN__
        d->gameRules.respawnMonsters = Reader_ReadByte(reader);
#endif
#if !__JHEXEN__
        /*skip old junk*/ if(d->version < 10) SV_Seek(2);

        d->mapTime = Reader_ReadInt32(reader);
#endif
    }

#if !__JHEXEN__
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        d->players[i] = Reader_ReadByte(reader);
    }
#endif

    d->sessionId = Reader_ReadInt32(reader);
}

int SaveInfo::magic() const
{
    return d->magic;
}

void SaveInfo::setMagic(int newMagic)
{
    d->magic = newMagic;
}

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

uint SaveInfo_SessionId(SaveInfo const *info)
{
    DENG_ASSERT(info != 0);
    return info->sessionId();
}

void SaveInfo_SetSessionId(SaveInfo *info, uint newSessionId)
{
    DENG_ASSERT(info != 0);
    info->setSessionId(newSessionId);
}

Str const *SaveInfo_GameIdentityKey(SaveInfo const *info)
{
    DENG_ASSERT(info != 0);
    return info->gameIdentityKey();
}

void SaveInfo_SetGameIdentityKey(SaveInfo *info, Str const *newGameIdentityKey)
{
    DENG_ASSERT(info != 0);
    info->setGameIdentityKey(newGameIdentityKey);
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

int SaveInfo_Version(SaveInfo const *info)
{
    DENG_ASSERT(info != 0);
    return info->version();
}

void SaveInfo_SetVersion(SaveInfo *info, int newVersion)
{
    DENG_ASSERT(info != 0);
    info->setVersion(newVersion);
}

Uri const *SaveInfo_MapUri(SaveInfo const *info)
{
    DENG_ASSERT(info != 0);
    return info->mapUri();
}

void SaveInfo_SetMapUri(SaveInfo *info, Uri const *newMapUri)
{
    DENG_ASSERT(info != 0);
    info->setMapUri(newMapUri);
}

#if !__JHEXEN__
int SaveInfo_MapTime(SaveInfo const *info)
{
    DENG_ASSERT(info != 0);
    return info->mapTime();
}

void SaveInfo_SetMapTime(SaveInfo *info, int newMapTime)
{
    DENG_ASSERT(info != 0);
    info->setMapTime(newMapTime);
}
#endif

GameRuleset const *SaveInfo_GameRules(SaveInfo const *info)
{
    DENG_ASSERT(info != 0);
    return &info->gameRules();
}

void SaveInfo_SetGameRules(SaveInfo *info, GameRuleset const *newRules)
{
    DENG_ASSERT(info != 0 && newRules != 0);
    info->setGameRules(*newRules);
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
