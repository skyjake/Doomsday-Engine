/** @file doomv9gamestatereader.h  Doom ver 1.9 saved game state reader.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#ifndef LIBDOOM_DOOMV9_GAMESTATEREADER
#define LIBDOOM_DOOMV9_GAMESTATEREADER

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include <de/game/IGameStateReader>

/**
 * Doom ver 1.9 saved game state reader.
 *
 * @ingroup libdoom
 */
class DoomV9GameStateReader : public de::IGameStateReader
{
public:
    DoomV9GameStateReader();
    ~DoomV9GameStateReader();

    static de::IGameStateReader *make();
    static bool recognize(de::Path const &stateFilePath, de::SessionMetadata &metadata);

    void read(de::Path const &stateFilePath, de::Path const &mapStateFilePath,
              de::SessionMetadata const &metadata);

private:
    DENG2_PRIVATE(d)
};

#endif // LIBDOOM_DOOMV9_GAMESTATEREADER
