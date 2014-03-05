/** @file igamestatereader.h  Interface for a serialized game state reader.
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

#ifndef LIBDENG2_IGAMESTATEREADER_H
#define LIBDENG2_IGAMESTATEREADER_H

#include <de/Error>
#include <de/Path>
#include <de/Record>
#include <de/SavedSession>

namespace de {

/**
 * Interface for serialized game state (savegame) readers.
 */
class DENG2_PUBLIC IGameStateReader
{
public:
    /// An error occurred attempting to open the input file. @ingroup errors
    DENG2_ERROR(FileAccessError);

    /// Base class for read errors. @ingroup errors
    DENG2_ERROR(ReadError);

public:
    virtual ~IGameStateReader() {}

    /**
     * Attempt to load (read/interpret) the serialized game state.
     *
     * @paran stateFilePath     Path to the game state file to be read/interpreted.
     * @param mapStateFilePath  Path to the map state file to be read/interpreted.
     *
     * @param metadata  Deserialized save session metadata for the game state. At this point
     * the metadata has already been read once, so this is provided FYI.
     */
    virtual void read(Path const &stateFilePath, Path const &mapStateFilePath,
                      SavedSession::Metadata const &metadata) = 0;
};

} // namespace de

#endif // LIBDENG2_IGAMESTATEREADER_H
