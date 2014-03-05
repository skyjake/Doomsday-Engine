/** @file gamestatereader.h  Saved game state reader.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_GAMESTATEREADER_H
#define LIBCOMMON_GAMESTATEREADER_H

#include <de/game/IGameStateReader>

/**
 * Native saved game state reader.
 *
 * @ingroup libcommon
 * @see GameStateWriter
 */
class GameStateReader : public de::IGameStateReader
{
public:
    GameStateReader();
    ~GameStateReader();

    static de::IGameStateReader *make();
    static bool recognize(de::Path const &stateFilePath, de::SessionMetadata &metadata);

    void read(de::Path const &stateFilePath, de::Path const &mapStateFilePath,
              de::SessionMetadata const &metadata);

private:
    DENG2_PRIVATE(d)
};

#endif // LIBCOMMON_GAMESTATEREADER_H
