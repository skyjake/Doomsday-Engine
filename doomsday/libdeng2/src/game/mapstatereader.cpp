/** @file mapstatereader.cpp  Abstract base for serialized game map state readers.
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

#include "de/game/MapStateReader"

namespace de {
namespace game {

DENG2_PIMPL_NOREF(MapStateReader)
{
    SavedSession const *session; ///< Saved session being loaded. Not Owned.
};

MapStateReader::MapStateReader(SavedSession const &session) : d(new Instance)
{
    d->session = &session;
}

MapStateReader::~MapStateReader()
{}

SavedSession const &MapStateReader::session() const
{
    DENG2_ASSERT(d->session != 0);
    return *d->session;
}

} // namespace game
} // namespace de
