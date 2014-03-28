/** @file savedsessionrepository.h  Saved (game) session repository.
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

#ifndef LIBDENG2_SAVEDSESSIONREPOSITORY_H
#define LIBDENG2_SAVEDSESSIONREPOSITORY_H

#include "../Folder"
#include "../Observers"
#include "../String"
#include <QMap>

namespace de {
namespace game {

class SavedSession;

/**
 * Index of available SavedSessions.
 *
 * @ingroup game
 */
class DENG2_PUBLIC SavedSessionRepository
{
public:
    /// Notified whenever a saved session is added/removed from the repository.
    DENG2_DEFINE_AUDIENCE2(AvailabilityUpdate, void repositoryAvailabilityUpdate(SavedSessionRepository const &repository))

    typedef QMap<String, SavedSession *> All;

public:
    SavedSessionRepository();

    /**
     * Clear the SavedSession index.
     */
    void clear();

    /**
     * Add a saved session in the index. If an entry for the session already exists,
     * it is replaced by the new one.
     *
     * @param session  SavedSession to add to the index.
     */
    void add(SavedSession &session);

    /**
     * Remove a saved session from the index (if present).
     *
     * @param path  Absolute path of the associated .save package.
     */
    void remove(String path);

    /**
     * Lookup a SavedSession in the index by absolute path.
     */
    SavedSession *find(String path) const;

    /**
     * Provides access to the saved session dataset, for efficient traversal.
     */
    All const &all() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace game
} // namespace de

#endif // LIBDENG2_SAVEDSESSIONREPOSITORY_H
