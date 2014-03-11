/** @file gamestatewriter.h  Saved game state writer.
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

#ifndef LIBCOMMON_GAMESTATEWRITER_H
#define LIBCOMMON_GAMESTATEWRITER_H

#include <de/Error>
#include <de/game/MapStateReader>
#include <de/Path>

/**
 * Native saved game state writer.
 *
 * @ingroup libcommon
 * @see GameStateReader
 */
class GameStateWriter
{
public:
    /// An error occurred attempting to open the output file. @ingroup errors
    DENG2_ERROR(FileAccessError);

public:
    GameStateWriter();

    void write(de::Path const &stateFilePath, de::Path const &mapStateFilePath,
               de::game::SessionMetadata const &metadata);

private:
    DENG2_PRIVATE(d)
};

#endif // LIBCOMMON_GAMESTATEWRITER_H
