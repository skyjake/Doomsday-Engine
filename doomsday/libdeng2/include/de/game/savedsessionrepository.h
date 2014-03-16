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

#include "../Error"
#include "../Observers"
#include "../String"
#include <map>

namespace de {
namespace game {

class SavedSession;

/**
 * Centralized saved session repository.
 *
 * @ingroup game
 */
class DENG2_PUBLIC SavedSessionRepository
{
public:
    /// Required/referenced session is missing. @ingroup errors
    DENG2_ERROR(MissingSessionError);

    /// Notified whenever a saved session is added/removed from the repository.
    DENG2_DEFINE_AUDIENCE2(AvailabilityUpdate, void repositoryAvailabilityUpdate(SavedSessionRepository const &repository))

    typedef std::map<String, SavedSession *> All;

public:
    SavedSessionRepository();

    void clear();

    /**
     * Add/replace a saved session in the repository. If a session already exists, it is
     * replaced by the new one.
     *
     * @param path        Relative path of the associated game state file package.
     * @param newSession  New saved session to replace with. Ownership is given.
     */
    void add(String path, SavedSession *find);

    /**
     * Determines whether a saved session exists for @a path.
     *
     * @see find()
     */
    bool has(String path) const;

    /**
     * Lookup the SavedSession for @a path.
     *
     * @see has()
     */
    SavedSession &find(String path) const;

    inline SavedSession *findPtr(String path) const {
        return has(path)? &find(path) : 0;
    }

    /**
     * Lookup the saved session with a matching user description. The search is in ascending
     * saved session file name order.
     *
     * @param description  User description of the saved session to look for (not case sensitive).
     *
     * @return  The found SavedSession; otherwise @c 0.
     *
     * @see find()
     */
    SavedSession *findByUserDescription(String description) const;

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
