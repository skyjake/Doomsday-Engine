/** @file sessionmetadata.cpp  Saved game session metadata.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
#include "sessionmetadata.h"

#include "g_common.h"
#include "p_savedef.h"  // MY_SAVE_MAGIC
#include "p_saveio.h"
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

SessionMetadata *SessionMetadata::fromReader(reader_s *reader) // static
{
    SessionMetadata *meta = new SessionMetadata;
    meta->read(reader);
    return meta;
}

de::String SessionMetadata::asText() const
{
    AutoStr *currentMapUriAsText = Uri_ToString(mapUri);

    return de::String(_E(b) "%1\n" _E(.)
                      _E(l) "IdentityKey: " _E(.)_E(i) "%2 "  _E(.)
                      _E(l) "Current map: " _E(.)_E(i) "%3\n" _E(.)
                      _E(l) "Version: "     _E(.)_E(i) "%4 "  _E(.)
                      _E(l) "Session id: "  _E(.)_E(i) "%5\n" _E(.)
                      _E(D) "Game rules:\n" _E(.) "  %6")
                 .arg(userDescription)
                 .arg(gameIdentityKey)
                 .arg(Str_Text(currentMapUriAsText))
                 .arg(version)
                 .arg(sessionId)
                 .arg(gameRules.asText());
}
