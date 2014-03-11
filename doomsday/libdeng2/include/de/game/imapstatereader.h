/** @file imapstatereader.h  Interface for a serialized game map state reader.
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

#ifndef LIBDENG2_IMAPSTATEREADER_H
#define LIBDENG2_IMAPSTATEREADER_H

#include <de/Error>
#include <de/game/SavedSession>
#include <de/String>

namespace de {
namespace game {

/**
 * Interface for serialized game map state (savegame) readers.
 *
 * @ingroup game
 */
class DENG2_PUBLIC IMapStateReader
{
public:
    /// An error occurred attempting to open the input file. @ingroup errors
    DENG2_ERROR(FileAccessError);

    /// Base class for read errors. @ingroup errors
    DENG2_ERROR(ReadError);

public:
    virtual ~IMapStateReader() {}

    /**
     * Attempt to load (read/interpret) the serialized game map state.
     *
     * @param session    Saved session being loaded.
     * @param mapUriStr  Unique identifier of the map state to deserialize.
     */
    virtual void read(SavedSession const &session, String const &mapUriStr) = 0;
};

} // namespace game
} // namespace de

#endif // LIBDENG2_IMAPSTATEREADER_H
