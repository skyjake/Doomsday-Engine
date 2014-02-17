/** @file gamestatereader.cpp  Saved game state reader.
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
#include "gamestatereader.h"

#include "d_net.h"          // NetSv_LoadGame
#include "g_common.h"
#include "mapstatereader.h"
#include "p_mapsetup.h"     // P_SpawnAllMaterialOriginScrollers
#include "p_saveio.h"
#include "p_saveg.h"        /// @todo remove me
#include "p_tick.h"         // mapTime
#include "r_common.h"       // R_UpdateConsoleView
#include <de/NativePath>
#include <de/String>

DENG2_PIMPL(GameStateReader)
{
    SaveInfo *saveInfo; ///< Info for the save state to be read. Not owned.
    Reader *reader;
    dd_bool loaded[MAXPLAYERS];
    dd_bool infile[MAXPLAYERS];

    Instance(Public *i)
        : Base(i)
        , saveInfo(0)
        , reader(0)
    {
        de::zap(loaded);
        de::zap(infile);
    }

    ~Instance()
    {
        Reader_Delete(reader);
    }

    /// Assumes the reader is currently positioned at the start of the stream.
    void seekToGameState()
    {
        // Read the header again.
        /// @todo seek straight to the game state.
        delete SaveInfo::fromReader(reader);
    }

    void beginSegment(int segId)
    {
#if __JHEXEN__
        if(segId == ASEG_END && SV_HxBytesLeft() < 4)
        {
            App_Log(DE2_LOG_WARNING, "Savegame lacks ASEG_END marker (unexpected end-of-file)");
            return;
        }
        if(Reader_ReadInt32(reader) != segId)
        {
            /// @throw ReadError Failed alignment check.
            throw ReadError("GameStateReader", "Corrupt save game, segment #" + de::String::number(segId) + " failed alignment check");
        }
#else
        DENG_UNUSED(segId);
#endif
    }

    void endSegment()
    {
        beginSegment(ASEG_END);
    }

    void readConsistencyBytes()
    {
#if !__JHEXEN__
        if(Reader_ReadByte(reader) != CONSISTENCY)
        {
            /// @throw ReadError Failed alignment check.
            throw ReadError("GameStateReader", "Corrupt save game, failed consistency check");
        }
#endif
    }

    void readWorldACScriptData()
    {
#if __JHEXEN__
        if(saveInfo->version() >= 7)
        {
            beginSegment(ASEG_WORLDSCRIPTDATA);
        }
        Game_ACScriptInterpreter().readWorldScriptData(reader, saveInfo->version());
#endif
    }

    void readMap(int thingArchiveSize)
    {
#if __JHEXEN__ // The map state is actually read from a separate file.
        // Close the game state file.
        Z_Free(saveBuffer);

        // Open the map state file.
        de::Path path = saveSlots->mapSavePathForSlot(BASE_SLOT, gameMap);
        if(!SV_OpenMapSaveFile(path))
        {
            throw FileAccessError("GameStateReader", "Failed opening \"" + de::NativePath(path).pretty() + "\"");
        }
#endif

        MapStateReader(saveInfo->version(), thingArchiveSize).read(reader);

        readConsistencyBytes();
#if __JHEXEN__
        Z_Free(saveBuffer);
#else
        SV_CloseFile();
#endif
#if __JHEXEN__
        SV_ClearTargetPlayers();
#endif
    }

    // We don't have the right to say which players are in the game. The players that already are
    // will continue to be. If the data for a given player is not in the savegame file, he will
    // be notified. The data for players who were saved but are not currently in the game will be
    // discarded.
    void readPlayers()
    {
#if __JHEXEN__
        if(saveInfo->version() >= 4)
#else
        if(saveInfo->version() >= 5)
#endif
        {
            beginSegment(ASEG_PLAYER_HEADER);
        }
        playerheader_t plrHdr;
        plrHdr.read(reader, saveInfo->version());

        // Setup the dummy.
        ddplayer_t dummyDDPlayer;
        player_t dummyPlayer;
        dummyPlayer.plr = &dummyDDPlayer;

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            loaded[i] = 0;
#if !__JHEXEN__
            infile[i] = saveInfo->players()[i];
#endif
        }

        beginSegment(ASEG_PLAYERS);
        {
#if __JHEXEN__
            for(int i = 0; i < MAXPLAYERS; ++i)
            {
                infile[i] = Reader_ReadByte(reader);
            }
#endif

            // Load the players.
            for(int i = 0; i < MAXPLAYERS; ++i)
            {
                // By default a saved player translates to nothing.
                saveToRealPlayerNum[i] = -1;

                if(!infile[i]) continue;

                // The ID number will determine which player this actually is.
                int pid = Reader_ReadInt32(reader);
                player_t *player = 0;
                for(int k = 0; k < MAXPLAYERS; ++k)
                {
                    if((IS_NETGAME && int(Net_GetPlayerID(k)) == pid) ||
                       (!IS_NETGAME && k == 0))
                    {
                        // This is our guy.
                        player = players + k;
                        loaded[k] = true;
                        // Later references to the player number 'i' must be translated!
                        saveToRealPlayerNum[i] = k;
                        App_Log(DE2_DEV_MAP_MSG, "readPlayers: saved %i is now %i\n", i, k);
                        break;
                    }
                }

                if(!player)
                {
                    // We have a missing player. Use a dummy to load the data.
                    player = &dummyPlayer;
                }

                // Read the data.
                player->read(reader, plrHdr);
            }
        }
        endSegment();
    }

    void kickMissingPlayers()
    {
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            bool notLoaded = false;

#if __JHEXEN__
            if(players[i].plr->inGame)
            {
                // Try to find a saved player that corresponds this one.
                int k;
                for(k = 0; k < MAXPLAYERS; ++k)
                {
                    if(saveToRealPlayerNum[k] == i)
                    {
                        break;
                    }
                }
                if(k < MAXPLAYERS)
                    continue; // Found; don't bother this player.

                players[i].playerState = PST_REBORN;

                if(!i)
                {
                    // If the CONSOLEPLAYER isn't in the save, it must be some
                    // other player's file?
                    P_SetMessage(players, LMF_NO_HIDE, GET_TXT(TXT_LOADMISSING));
                }
                else
                {
                    NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));
                    notLoaded = true;
                }
            }
#else
            if(!loaded[i] && players[i].plr->inGame)
            {
                if(!i)
                {
                    P_SetMessage(players, LMF_NO_HIDE, GET_TXT(TXT_LOADMISSING));
                }
                else
                {
                    NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));
                }
                notLoaded = true;
            }
#endif

            if(notLoaded)
            {
                // Kick this player out, he doesn't belong here.
                DD_Executef(false, "kick %i", i);
            }
        }
    }
};

GameStateReader::GameStateReader() : d(new Instance(this))
{}

GameStateReader::~GameStateReader()
{}

bool GameStateReader::recognize(SaveInfo &info, de::Path path) // static
{
    if(!SV_ExistingFile(path)) return false;
    if(!SV_OpenGameSaveFile(path, false/*for reading*/)) return false;

    Reader *reader = SV_NewReader();
    info.read(reader);
    Reader_Delete(reader);

#if __JHEXEN__
    Z_Free(saveBuffer);
#else
    SV_CloseFile();
#endif

    // Magic must match.
    if(info.magic() != MY_SAVE_MAGIC && info.magic() != MY_CLIENT_SAVE_MAGIC)
    {
        return false;
    }

    /*
     * Check for unsupported versions.
     */
    if(info.version() > MY_SAVE_VERSION) // Future version?
    {
        return false;
    }

#if __JHEXEN__
    // We are incompatible with v3 saves due to an invalid test used to determine
    // present sides (ver3 format's sides contain chunks of junk data).
    if(info.version() == 3)
    {
        return false;
    }
#endif

    return true;
}

IGameStateReader *GameStateReader::make() // static
{
    return new GameStateReader;
}

void GameStateReader::read(SaveInfo &info, de::Path path)
{
    d->saveInfo = &info;

    if(!SV_OpenGameSaveFile(path, false/*for reading*/))
    {
        throw FileAccessError("GameStateReader", "Failed opening \"" + de::NativePath(path).pretty() + "\"");
    }

    d->reader = SV_NewReader();

    d->seekToGameState();

    d->readWorldACScriptData();

    /*
     * Load the map and configure some game settings.
     */
    briefDisabled = true;
    G_NewGame(d->saveInfo->mapUri(), 0/*not saved??*/, &d->saveInfo->gameRules());
    G_SetGameAction(GA_NONE); /// @todo Necessary?

#if !__JHEXEN__
    mapTime = d->saveInfo->mapTime();
#endif

    int thingArchiveSize = 0;
#if !__JHEXEN__
    thingArchiveSize = (d->saveInfo->version() >= 5? Reader_ReadInt32(d->reader) : 1024);
#endif

    d->readPlayers();
    d->readMap(thingArchiveSize);

    // Notify the players that weren't in the savegame.
    d->kickMissingPlayers();

#if !__JHEXEN__
    // In netgames, the server tells the clients about this.
    NetSv_LoadGame(d->saveInfo->sessionId());
#endif

    Reader_Delete(d->reader); d->reader = 0;

    // Material scrollers must be spawned for older savegame versions.
    if(d->saveInfo->version() <= 10)
    {
        P_SpawnAllMaterialOriginScrollers();
    }

    // Let the engine know where the local players are now.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        R_UpdateConsoleView(i);
    }

    // Inform the engine that map setup must be performed once more.
    R_SetupMap(0, 0);
}
