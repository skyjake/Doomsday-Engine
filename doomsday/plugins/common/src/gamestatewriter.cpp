/** @file gamestatewriter.cpp  Saved game state writer.
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
#include "gamestatewriter.h"

#include "d_net.h"          // NetSv_SaveGame
#include "mapstatewriter.h"
#include "p_saveio.h"
#include "p_saveg.h" /// @todo remove me
#include <de/String>

DENG2_PIMPL(GameStateWriter)
{
    SaveInfo *saveInfo; ///< Info for the save state to be written.
    Writer *writer;

    Instance(Public *i)
        : Base(i)
        , saveInfo(0)
        , writer(0)
    {}

    void beginSegment(int segmentId)
    {
        SV_BeginSegment(segmentId);
    }

    void endSegment()
    {
        SV_EndSegment();
    }

    void writeSessionHeader()
    {
        saveInfo->write(writer);
    }

    void writeWorldACScriptData()
    {
#if __JHEXEN__
        beginSegment(ASEG_WORLDSCRIPTDATA);
        Game_ACScriptInterpreter().writeWorldScriptData(writer);
#endif
    }

    void writeMap()
    {
#if __JHEXEN__ // The map state is actually written to a separate file.
        // Close the game state file.
        SV_CloseFile();

        // Open the map state file.
        SV_OpenFile(saveSlots->composeSavePathForSlot(BASE_SLOT, gameMap + 1), "wp");
#endif

        MapStateWriter(theThingArchive).write(writer);

        SV_WriteConsistencyBytes(); // To be absolutely sure...
        SV_CloseFile();

#if !__JHEXEN___
        theThingArchive.clear();
#endif
    }

    void writePlayers()
    {
        beginSegment(ASEG_PLAYER_HEADER);
        SV_GetPlayerHeader()->write(writer);

        beginSegment(ASEG_PLAYERS);
        {
#if __JHEXEN__
            for(int i = 0; i < MAXPLAYERS; ++i)
            {
                Writer_WriteByte(writer, players[i].plr->inGame);
            }
#endif

            for(int i = 0; i < MAXPLAYERS; ++i)
            {
                player_t *plr = players + i;
                if(!plr->plr->inGame)
                    continue;

                Writer_WriteInt32(writer, Net_GetPlayerID(i));
                plr->write(writer);
            }
        }
        endSegment();
    }
};

GameStateWriter::GameStateWriter() : d(new Instance(this))
{}

void GameStateWriter::write(SaveInfo *saveInfo, Str const *path)
{
    DENG_ASSERT(saveInfo != 0 && path != 0);
    d->saveInfo = saveInfo;

    // In networked games the server tells the clients to save their games.
#if !__JHEXEN__
    NetSv_SaveGame(d->saveInfo->sessionId());
#endif

    playerHeaderOK = false; // Uninitialized.

    if(!SV_OpenGameSaveFile(path, true/*for writing*/))
    {
        throw FileAccessError("GameStateWriter", "Failed opening \"" + de::String(Str_Text(path)) + "\"");
    }

    d->writer = SV_NewWriter();

    d->writeSessionHeader();
    d->writeWorldACScriptData();

    // Set the mobj archive numbers.
    theThingArchive.initForSave(false/*do not exclude players*/);
#if !__JHEXEN__
    Writer_WriteInt32(d->writer, theThingArchive.size());
#endif

    d->writePlayers();
    d->writeMap();

    Writer_Delete(d->writer); d->writer = 0;
}
