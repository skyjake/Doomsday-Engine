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
#include <de/NativePath>
#include <cstring>

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

    static de::String currentGameIdentityKey()
    {
        GameInfo gameInfo;
        DD_GameInfo(&gameInfo);
        return Str_Text(gameInfo.identityKey);
    }
}

using namespace de;
using namespace internal;

DENG2_PIMPL_NOREF(SaveInfo)
{
    String fileName; ///< Name of the game session file.

    // Metadata (the session header):
    String userDescription;
    uint sessionId;
    int magic;
    int version;
    String gameIdentityKey;
    Uri *mapUri;
    GameRuleset gameRules;
#if !__JHEXEN__
    int mapTime;
    Players players;
#endif

    Instance()
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

    Instance(Instance const &other)
        : IPrivate()
        , fileName       (other.fileName)
        , userDescription(other.userDescription)
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

    ~Instance()
    {
        Uri_Delete(mapUri);
    }

#if __JHEXEN__
    /**
     * Deserialize the legacy Hexen-specific v.9 info.
     */
    void read_Hx_v9(reader_s *reader)
    {
        char descBuf[24];
        Reader_Read(reader, descBuf, 24);
        userDescription = String(descBuf, 24);

        magic           = MY_SAVE_MAGIC; // Lets pretend...

        char verText[16 + 1]; // "HXS Ver "
        Reader_Read(reader, &verText, 16); descBuf[16] = 0;
        version         = String(&verText[8]).toInt();

        /// @note Kludge: Assume the current game.
        gameIdentityKey = currentGameIdentityKey();
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
    }
#endif
};

SaveInfo::SaveInfo(String const &fileName) : d(new Instance)
{
    d->fileName = fileName;
}

SaveInfo::SaveInfo(SaveInfo const &other) : d(new Instance(*other.d))
{}

SaveInfo *SaveInfo::newWithCurrentSessionMetadata(String const &fileName,
    String const &userDescription) // static
{
    SaveInfo *info = new SaveInfo(fileName);
    info->setUserDescription(userDescription);
    info->applyCurrentSessionMetadata();
    info->setSessionId(G_GenerateSessionId());
    return info;
}

SaveInfo &SaveInfo::operator = (SaveInfo const &other)
{
    d.reset(new Instance(*other.d));
    return *this;
}

SaveInfo::SessionStatus SaveInfo::status() const
{
    if(!haveGameSession())       return Unused;
    if(!gameSessionIsLoadable()) return Incompatible;
    return Loadable;
}

String SaveInfo::fileName() const
{
    return d->fileName + String("." SAVEGAMEEXTENSION);
}

void SaveInfo::setFileName(String newName)
{
    d->fileName = newName;
}

String SaveInfo::fileNameForMap(Uri const *mapUri) const
{
    if(!mapUri) mapUri = gameMapUri;

    uint map = G_MapNumberFor(mapUri);
    return d->fileName + String("%1." SAVEGAMEEXTENSION)
                             .arg(int(map + 1), 2, 10, QChar('0'));
}

String const &SaveInfo::gameIdentityKey() const
{
    return d->gameIdentityKey;
}

void SaveInfo::setGameIdentityKey(String newGameIdentityKey)
{
    d->gameIdentityKey = newGameIdentityKey;
}

int SaveInfo::version() const
{
    return d->version;
}

void SaveInfo::setVersion(int newVersion)
{
    d->version = newVersion;
}

String const &SaveInfo::userDescription() const
{
    return d->userDescription;
}

void SaveInfo::setUserDescription(String newUserDescription)
{
    d->userDescription = newUserDescription;
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
    DENG2_ASSERT(newMapUri != 0);
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
    d->gameRules = newRules; // Make a copy
}

void SaveInfo::applyCurrentSessionMetadata()
{
    d->magic           = IS_NETWORK_CLIENT? MY_CLIENT_SAVE_MAGIC : MY_SAVE_MAGIC;
    d->version         = MY_SAVE_VERSION;
    d->gameIdentityKey = currentGameIdentityKey();
    Uri_Copy(d->mapUri, gameMapUri);
#if !__JHEXEN__
    d->mapTime         = ::mapTime;
#endif
    d->gameRules       = G_Rules(); // Make a copy.

#if !__JHEXEN__
    for(int i = 0; i < MAXPLAYERS; i++)
    {
        d->players[i]  = (::players[i]).plr->inGame;
    }
#endif
}

bool SaveInfo::haveGameSession() const
{
    return SV_ExistingFile(SV_SavePath() / fileName());
}

bool SaveInfo::gameSessionIsLoadable() const
{
    if(!haveGameSession()) return false;

    // Game identity key missmatch?
    if(d->gameIdentityKey.compareWithoutCase(currentGameIdentityKey()))
    {
        return false;
    }

    /// @todo Validate loaded add-ons and checksum the definition database.

    return true; // It's good!
}

bool SaveInfo::haveMapSession(Uri const *mapUri) const
{
    if(usingSeparateMapSessionFiles())
    {
        return SV_ExistingFile(SV_SavePath() / fileNameForMap(mapUri));
    }
    return haveGameSession();
}

void SaveInfo::updateFromFile()
{
    if(SV_SavePath().isEmpty())
    {
        // The save path cannot be accessed for some reason. Perhaps its a network path?
        setUserDescription("");
        setSessionId(0);
        return;
    }

    // Is this a recognized game state?
    if(!G_GameStateReaderFactory().recognize(*this))
    {
        // Clear the info.
        setUserDescription("");
        setSessionId(0);
        return;
    }

    // Ensure we have a valid description.
    if(d->userDescription.isEmpty())
    {
        setUserDescription("UNNAMED");
    }
}

void SaveInfo::write(writer_s *writer) const
{
    Writer_WriteInt32(writer, d->magic);
    Writer_WriteInt32(writer, d->version);

    AutoStr *gameIdentityKeyStr = AutoStr_FromTextStd(d->gameIdentityKey.toUtf8().constData());
    Str_Write(gameIdentityKeyStr, writer);

    AutoStr *descriptionStr = AutoStr_FromTextStd(d->userDescription.toUtf8().constData());
    Str_Write(descriptionStr, writer);

    Uri_Write(d->mapUri, writer);
#if !__JHEXEN__
    Writer_WriteInt32(writer, d->mapTime);
#endif
    d->gameRules.write(writer);

#if !__JHEXEN__
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        Writer_WriteByte(writer, d->players[i]);
    }
#endif

    Writer_WriteInt32(writer, d->sessionId);
}

void SaveInfo::read(reader_s *reader)
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
        AutoStr *tmp = AutoStr_NewStd();
        Str_Read(tmp, reader);
        d->gameIdentityKey = Str_Text(tmp);
    }
    else
    {
        // Translate gamemode identifiers from older save versions.
        int oldGamemode = Reader_ReadInt32(reader);
        d->gameIdentityKey = Str_Text(G_IdentityKeyForLegacyGamemode(oldGamemode, d->version));
    }

    if(d->version >= 10)
    {
        AutoStr *tmp = AutoStr_NewStd();
        Str_Read(tmp, reader);
        d->userDescription = Str_Text(tmp);
    }
    else
    {
        // Description is a fixed 24 characters in length.
        char descBuf[24];
        Reader_Read(reader, descBuf, 24);
        d->userDescription = String(descBuf, 24);
    }

    if(d->version >= 14)
    {
        Uri_Read(d->mapUri, reader);
#if !__JHEXEN__
        d->mapTime = Reader_ReadInt32(reader);
#endif

        d->gameRules.read(reader);
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
    AutoStr *currentMapUriAsText = Uri_ToString(mapUri());

    return String(_E(b) "%1\n" _E(.)
                  _E(l) "IdentityKey: " _E(.)_E(i) "%2 "  _E(.)
                  _E(l) "Current map: " _E(.)_E(i) "%3\n" _E(.)
                  _E(l) "Source file: " _E(.)_E(i) "\"%4\"\n" _E(.)
                  _E(l) "Version: "     _E(.)_E(i) "%5 "  _E(.)
                  _E(l) "Session id: "  _E(.)_E(i) "%6\n" _E(.)
                  _E(D) "Game rules:\n" _E(.) "  %7\n"
                  _E(D) "Status: " _E(.) "%8")
             .arg(userDescription())
             .arg(gameIdentityKey())
             .arg(Str_Text(currentMapUriAsText))
             .arg(NativePath(SV_SavePath() / fileName()).pretty())
             .arg(version())
             .arg(sessionId())
             .arg(gameRules().asText())
             .arg(statusAsText());
}

int SaveInfo::magic() const
{
    return d->magic;
}

void SaveInfo::setMagic(int newMagic)
{
    d->magic = newMagic;
}

SaveInfo *SaveInfo::fromReader(reader_s *reader) // static
{
    SaveInfo *info = new SaveInfo;
    info->read(reader);
    return info;
}
