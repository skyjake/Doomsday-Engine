/** @file mapstatereader.h  Abstract base for serialized game map state readers.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG2_MAPSTATEREADER_H
#define LIBDENG2_MAPSTATEREADER_H

#include <de/Error>
#include <de/game/SavedSession>
#include <de/String>

namespace de {
namespace game {

/**
 * Abstract base class for serialized, game map state readers (savegames).
 *
 * @ingroup game
 */
class DENG2_PUBLIC MapStateReader
{
public:
    /// Base class for read errors. @ingroup errors
    DENG2_ERROR(ReadError);

public:
    /**
     * Construct a new MapStateReader for the given saved @a session.
     *
     * @param session  The saved session being loaded.
     */
    MapStateReader(SavedSession const &session);
    virtual ~MapStateReader();

    /**
     * Returns the saved session being loaded.
     */
    SavedSession const &session() const;

    /**
     * Attempt to load (read/interpret) the serialized game map state.
     *
     * @param mapUriStr  Unique identifier of the map state to deserialize.
     */
    virtual void read(String const &mapUriStr) = 0;

private:
    DENG2_PRIVATE(d)
};

} // namespace game
} // namespace de

#endif // LIBDENG2_MAPSTATEREADER_H
