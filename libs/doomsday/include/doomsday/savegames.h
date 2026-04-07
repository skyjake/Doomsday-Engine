/** @file savegames.h  Saved games.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_SAVEGAMES_H
#define LIBDOOMSDAY_SAVEGAMES_H

#include "libdoomsday.h"
#include <de/string.h>
#include <de/fileindex.h>

class Games;

class LIBDOOMSDAY_PUBLIC SaveGames
{
public:
    static SaveGames &get();

public:
    SaveGames();

    void setGames(Games &games);

    void initialize();

    const de::FileIndex &saveIndex() const;

    /**
     * Utility for scheduling legacy savegame conversion(s) (delegated to background Tasks).
     *
     * @param gameId      Identifier of the game and corresponding subfolder name within
     *                    save repository to output the converted savegame to. Also used for
     *                    resolving ambiguous savegame formats.
     * @param sourcePath  If a zero-length string then @em all legacy savegames located for
     *                    this game will be considered. Otherwise use the path of a single
     *                    legacy savegame file to schedule a single conversion.
     *
     * @return  @c true if one or more conversion tasks were scheduled.
     */
    bool convertLegacySavegames(const de::String &gameId, const de::String &sourcePath = "");

    static void consoleRegister();
    static de::String savePath();

private:
    DE_PRIVATE(d)
};

#endif // LIBDOOMSDAY_SAVEGAMES_H
