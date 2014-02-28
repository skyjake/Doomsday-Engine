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
#include "gamestatereader.h"
#include "p_saveio.h"
#include "p_tick.h"          // mapTime
#include <de/Log>
#include <de/NativePath>
#include <cstring>

SessionMetadata::SessionMetadata()
    : sessionId(0)
    , magic    (0)
    , version  (0)
    , mapUri   (Uri_New())
#if !__JHEXEN__
    , mapTime  (0)
#endif
{
#if !__JHEXEN__
    de::zap(players);
#endif
}

SessionMetadata::SessionMetadata(SessionMetadata const &other)
    : userDescription(other.userDescription)
    , sessionId      (other.sessionId)
    , magic          (other.magic)
    , version        (other.version)
    , gameIdentityKey(other.gameIdentityKey)
    , mapUri         (Uri_Dup(other.mapUri))
    , gameRules      (other.gameRules)
#if !__JHEXEN__
    , mapTime        (other.mapTime)
#endif
{
#if !__JHEXEN__
    std::memcpy(&players, &other.players, sizeof(players));
#endif
}

SessionMetadata::~SessionMetadata()
{
    Uri_Delete(mapUri);
}

void SessionMetadata::write(Writer *writer) const
{
    Writer_WriteInt32(writer, magic);
    Writer_WriteInt32(writer, version);

    AutoStr *gameIdentityKeyStr = AutoStr_FromTextStd(gameIdentityKey.toUtf8().constData());
    Str_Write(gameIdentityKeyStr, writer);

    AutoStr *descriptionStr = AutoStr_FromTextStd(userDescription.toUtf8().constData());
    Str_Write(descriptionStr, writer);

    Uri_Write(mapUri, writer);
#if !__JHEXEN__
    Writer_WriteInt32(writer, mapTime);
#endif
    gameRules.write(writer);

#if !__JHEXEN__
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        Writer_WriteByte(writer, players[i]);
    }
#endif

    Writer_WriteInt32(writer, sessionId);
}

void SessionMetadata::read(Reader *reader)
{
#if __JHEXEN__
    // Read the magic byte to determine the high-level format.
    int magic = Reader_ReadInt32(reader);
    SV_HxSavePtr()->b -= 4; // Rewind the stream.

    if((!IS_NETWORK_CLIENT && magic != MY_SAVE_MAGIC) ||
       ( IS_NETWORK_CLIENT && magic != MY_CLIENT_SAVE_MAGIC))
    {
        // Assume the old v9 format.
        char descBuf[24];
        Reader_Read(reader, descBuf, 24);
        userDescription = de::String(descBuf, 24);

        magic           = MY_SAVE_MAGIC; // Lets pretend...

        char verText[16 + 1]; // "HXS Ver "
        Reader_Read(reader, &verText, 16); descBuf[16] = 0;
        version         = de::String(&verText[8]).toInt();

        /// @note Kludge: Assume the current game.
        gameIdentityKey = G_IdentityKey();
        /// Kludge end.

        /*Skip junk*/ SV_Seek(4);

        uint map = Reader_ReadByte(reader) - 1;
        Uri_Copy(mapUri, G_ComposeMapUri(0, map));

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
        return;
    }
#endif

    magic    = Reader_ReadInt32(reader);
    version  = Reader_ReadInt32(reader);
    if(version >= 14)
    {
        AutoStr *tmp = AutoStr_NewStd();
        Str_Read(tmp, reader);
        gameIdentityKey = Str_Text(tmp);
    }
    else
    {
        // Translate gamemode identifiers from older save versions.
        int oldGamemode = Reader_ReadInt32(reader);
        gameIdentityKey = G_IdentityKeyForLegacyGamemode(oldGamemode, version);
    }

    if(version >= 10)
    {
        AutoStr *tmp = AutoStr_NewStd();
        Str_Read(tmp, reader);
        userDescription = Str_Text(tmp);
    }
    else
    {
        // Description is a fixed 24 characters in length.
        char descBuf[24];
        Reader_Read(reader, descBuf, 24);
        userDescription = de::String(descBuf, 24);
    }

    if(version >= 14)
    {
        Uri_Read(mapUri, reader);
#if !__JHEXEN__
        mapTime = Reader_ReadInt32(reader);
#endif

        gameRules.read(reader);
    }
    else
    {
#if !__JHEXEN__
        if(version < 13)
        {
            // In DOOM the high bit of the skill mode byte is also used for the
            // "fast" game rule dd_bool. There is more confusion in that SM_NOTHINGS
            // will result in 0xff and thus always set the fast bit.
            //
            // Here we decipher this assuming that if the skill mode is invalid then
            // by default this means "spawn no things" and if so then the "fast" game
            // rule is meaningless so it is forced off.
            byte skillModePlusFastBit = Reader_ReadByte(reader);
            gameRules.skill = (skillmode_t) (skillModePlusFastBit & 0x7f);
            if(gameRules.skill < SM_BABY || gameRules.skill >= NUM_SKILL_MODES)
            {
                gameRules.skill = SM_NOTHINGS;
                gameRules.fast  = 0;
            }
            else
            {
                gameRules.fast  = (skillModePlusFastBit & 0x80) != 0;
            }
        }
        else
#endif
        {
            gameRules.skill = skillmode_t( Reader_ReadByte(reader) & 0x7f );
            // Interpret skill levels outside the normal range as "spawn no things".
            if(gameRules.skill < SM_BABY || gameRules.skill >= NUM_SKILL_MODES)
            {
                gameRules.skill = SM_NOTHINGS;
            }
        }

        uint episode = Reader_ReadByte(reader) - 1;
        uint map     = Reader_ReadByte(reader) - 1;
        Uri_Copy(mapUri, G_ComposeMapUri(episode, map));

        gameRules.deathmatch      = Reader_ReadByte(reader);
#if !__JHEXEN__
        if(version >= 13)
        {
            gameRules.fast        = Reader_ReadByte(reader);
        }
#endif
        gameRules.noMonsters      = Reader_ReadByte(reader);
#if __JHEXEN__
        gameRules.randomClasses   = Reader_ReadByte(reader);
#endif
#if !__JHEXEN__
        gameRules.respawnMonsters = Reader_ReadByte(reader);
#endif
#if !__JHEXEN__
        /*skip old junk*/ if(version < 10) SV_Seek(2);

        mapTime = Reader_ReadInt32(reader);
#endif
    }

#if !__JHEXEN__
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        players[i] = Reader_ReadByte(reader);
    }
#endif

    sessionId = Reader_ReadInt32(reader);
}

namespace internal
{
    static bool usingSeparateMapSessionFiles()
    {
#ifdef __JHEXEN__
        return true;
#else
        return false;
#endif
    }
}

using namespace de;
using namespace internal;

DENG2_PIMPL(SaveInfo)
{
    String fileName; ///< Name of the game session file.

    SessionMetadata meta;
    SessionStatus status;
    bool needUpdateStatus;

    Instance(Public *i)
        : Base(i)
        , status          (Unused)
        , needUpdateStatus(true)
    {}

    Instance(Public *i, Instance const &other)
        : Base(i)
        , fileName        (other.fileName)
        , meta            (other.meta)
        , status          (other.status)
        , needUpdateStatus(other.needUpdateStatus)
    {}

    void updateStatusIfNeeded()
    {
        if(!needUpdateStatus) return;

        needUpdateStatus = false;
        LOGDEV_XVERBOSE("Updating SaveInfo %p status") << thisPublic;

        SessionStatus const oldStatus = status;

        status = Unused;
        if(self.haveGameSession())
        {
            status = Incompatible;
            // Game identity key missmatch?
            if(!meta.gameIdentityKey.compareWithoutCase(G_IdentityKey()))
            {
                /// @todo Validate loaded add-ons and checksum the definition database.
                status = Loadable; // It's good!
            }
        }

        if(status != oldStatus)
        {
            DENG2_FOR_PUBLIC_AUDIENCE(SessionStatusChange, i)
            {
                i->saveInfoSessionStatusChanged(self);
            }
        }
    }
};

SaveInfo::SaveInfo(String const &fileName) : d(new Instance(this))
{
    d->fileName = fileName;
}

SaveInfo::SaveInfo(SaveInfo const &other) : d(new Instance(this, *other.d))
{}

SaveInfo *SaveInfo::newWithCurrentSessionMeta(String const &fileName,
    String const &userDescription) // static
{
    LOG_AS("SaveInfo");
    SaveInfo *info = new SaveInfo(fileName);

    info->d->meta.userDescription  = userDescription;
    info->d->meta.magic            = IS_NETWORK_CLIENT? MY_CLIENT_SAVE_MAGIC : MY_SAVE_MAGIC;
    info->d->meta.version          = MY_SAVE_VERSION;
    info->d->meta.gameIdentityKey  = G_IdentityKey();
    Uri_Copy(info->d->meta.mapUri, gameMapUri);
#if !__JHEXEN__
    info->d->meta.mapTime          = ::mapTime;
#endif
    info->d->meta.gameRules        = G_Rules(); // Make a copy.

#if !__JHEXEN__
    for(int i = 0; i < MAXPLAYERS; i++)
    {
        info->d->meta.players[i]   = (::players[i]).plr->inGame;
    }
#endif
    info->d->meta.sessionId        = G_GenerateSessionId();

    info->d->needUpdateStatus = true;
    return info;
}

SaveInfo &SaveInfo::operator = (SaveInfo const &other)
{
    d.reset(new Instance(this, *other.d));
    return *this;
}

SaveInfo::SessionStatus SaveInfo::status() const
{
    d->updateStatusIfNeeded();
    return d->status;
}

String SaveInfo::fileName() const
{
    return d->fileName + String("." SAVEGAMEEXTENSION);
}

void SaveInfo::setFileName(String newName)
{
    if(d->fileName != newName)
    {
        d->fileName         = newName;
        d->needUpdateStatus = true;
    }
}

String SaveInfo::fileNameForMap(Uri const *mapUri) const
{
    if(!mapUri) mapUri = gameMapUri;

    uint map = G_MapNumberFor(mapUri);
    return d->fileName + String("%1." SAVEGAMEEXTENSION)
                             .arg(int(map + 1), 2, 10, QChar('0'));
}

void SaveInfo::setGameIdentityKey(String newGameIdentityKey)
{
    if(d->meta.gameIdentityKey != newGameIdentityKey)
    {
        d->meta.gameIdentityKey  = newGameIdentityKey;
        d->needUpdateStatus = true;
    }
}

void SaveInfo::setMagic(int newMagic)
{
    if(d->meta.magic != newMagic)
    {
        d->meta.magic = newMagic;
        d->needUpdateStatus = true;
    }
}

void SaveInfo::setVersion(int newVersion)
{
    if(d->meta.version != newVersion)
    {
        d->meta.version = newVersion;
        d->needUpdateStatus = true;
    }
}

void SaveInfo::setUserDescription(String newUserDescription)
{
    if(d->meta.userDescription != newUserDescription)
    {
        d->meta.userDescription = newUserDescription;
        DENG2_FOR_AUDIENCE(UserDescriptionChange, i)
        {
            i->saveInfoUserDescriptionChanged(*this);
        }
    }
}

void SaveInfo::setSessionId(uint newSessionId)
{
    if(d->meta.sessionId != newSessionId)
    {
        d->meta.sessionId = newSessionId;
        d->needUpdateStatus = true;
    }
}

void SaveInfo::setMapUri(Uri const *newMapUri)
{
    DENG2_ASSERT(newMapUri != 0);
    Uri_Copy(d->meta.mapUri, newMapUri);
}

#if !__JHEXEN__
void SaveInfo::setMapTime(int newMapTime)
{
    d->meta.mapTime = newMapTime;
}

void SaveInfo::setPlayers(SessionMetadata::Players const &newPlayers)
{
    std::memcpy(d->meta.players, newPlayers, sizeof(d->meta.players));
}
#endif // !__JHEXEN__

void SaveInfo::setGameRules(GameRuleset const &newRules)
{
    LOG_AS("SaveInfo");
    d->meta.gameRules = newRules; // Make a copy
    d->needUpdateStatus = true;
}

bool SaveInfo::haveGameSession() const
{
    LOG_AS("SaveInfo");
    return SV_ExistingFile(SV_SavePath() / fileName());
}

bool SaveInfo::haveMapSession(Uri const *mapUri) const
{
    LOG_AS("SaveInfo");
    if(usingSeparateMapSessionFiles())
    {
        return SV_ExistingFile(SV_SavePath() / fileNameForMap(mapUri));
    }
    return haveGameSession();
}

void SaveInfo::updateFromFile()
{
    LOGDEV_VERBOSE("Updating SaveInfo %p from source file") << this;
    LOG_AS("SaveInfo");

    // Is this a recognized game state?
    if(G_GameStateReaderFactory().recognize(*this))
    {
        // Ensure we have a valid description.
        if(d->meta.userDescription.isEmpty())
        {
            setUserDescription("UNNAMED");
        }
    }
    else
    {
        // Unrecognized or the file could not be accessed (perhaps its a network path?).
        // Clear the info.
        setUserDescription("");
        setSessionId(0);
    }

    d->updateStatusIfNeeded();
}

SessionMetadata const &SaveInfo::meta() const
{
    return d->meta;
}

void SaveInfo::readMeta(reader_s *reader)
{
    LOG_AS("SaveInfo");
    d->meta.read(reader);
    d->needUpdateStatus = true;
}

String SaveInfo::statusAsText() const
{
    static String const statusTexts[] = {
        "Loadable",
        "Incompatible",
        "Unused",
    };
    return statusTexts[int(status())];
}

String SaveInfo::description() const
{
    AutoStr *currentMapUriAsText = Uri_ToString(meta().mapUri);

    return String(_E(b) "%1\n" _E(.)
                  _E(l) "IdentityKey: " _E(.)_E(i) "%2 "  _E(.)
                  _E(l) "Current map: " _E(.)_E(i) "%3\n" _E(.)
                  _E(l) "Source file: " _E(.)_E(i) "\"%4\"\n" _E(.)
                  _E(l) "Version: "     _E(.)_E(i) "%5 "  _E(.)
                  _E(l) "Session id: "  _E(.)_E(i) "%6\n" _E(.)
                  _E(D) "Game rules:\n" _E(.) "  %7\n"
                  _E(D) "Status: " _E(.) "%8")
             .arg(meta().userDescription)
             .arg(meta().gameIdentityKey)
             .arg(Str_Text(currentMapUriAsText))
             .arg(NativePath(SV_SavePath() / fileName()).pretty())
             .arg(meta().version)
             .arg(meta().sessionId)
             .arg(meta().gameRules.asText())
             .arg(statusAsText());
}

SaveInfo *SaveInfo::fromReader(reader_s *reader) // static
{
    SaveInfo *info = new SaveInfo;
    info->readMeta(reader);
    return info;
}
