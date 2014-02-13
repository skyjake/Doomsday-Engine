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

    void writeSessionHeader()
    {
        saveInfo->write(writer);
    }

    void writeWorldACScriptData()
    {
#if __JHEXEN__
        SV_BeginSegment(ASEG_WORLDSCRIPTDATA);
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

        MapStateWriter(thingArchiveExcludePlayers).write(writer);

        SV_WriteConsistencyBytes(); // To be absolutely sure...
        SV_CloseFile();

#if !__JHEXEN___
        SV_ClearThingArchive();
#endif
    }

    void writePlayers()
    {
        SV_BeginSegment(ASEG_PLAYERS);
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
        SV_EndSegment();
    }
};

GameStateWriter::GameStateWriter() : d(new Instance(this))
{}

void GameStateWriter::write(SaveInfo *saveInfo, Str const *path)
{
    DENG_ASSERT(saveInfo != 0 && path != 0);
    d->saveInfo = saveInfo;

    playerHeaderOK = false; // Uninitialized.

    if(!SV_OpenGameSaveFile(path, true))
    {
        throw FileAccessError("GameStateWriter", "Failed opening \"" + de::String(Str_Text(path)) + "\"");
    }

    d->writer = SV_NewWriter();

    d->writeSessionHeader();
    d->writeWorldACScriptData();

    // Set the mobj archive numbers.
    SV_InitThingArchiveForSave(false/*do not exclude players*/);
#if !__JHEXEN__
    Writer_WriteInt32(d->writer, thingArchiveSize);
#endif

    SV_WritePlayerHeader(d->writer);

    d->writePlayers();
    d->writeMap();

    Writer_Delete(d->writer); d->writer = 0;
}
