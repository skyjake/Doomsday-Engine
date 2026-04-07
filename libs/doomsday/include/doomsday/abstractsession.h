/** @file abstractsession.h  Logical game session base class.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_SESSION_H
#define LIBDOOMSDAY_SESSION_H

#include "libdoomsday.h"
#include "uri.h"
#include "world/ithinkermapping.h"
#include <de/error.h>
#include <de/observers.h>
#include <de/string.h>
#include <de/keymap.h>

class GameStateFolder;

/**
 * Base class for a logical game session. Derived classes implement the high level logic for
 * the manipulation and configuration of the game session.
 *
 * The game session exists at the same conceptual level as the logical game state. The primary
 * job of the derived class is to ensure that the current game state remains valid and provide
 * a mechanism for saving player progress.
 */
class LIBDOOMSDAY_PUBLIC AbstractSession
{
public:
    /// Current in-progress state does not match that expected. @ingroup errors
    DE_ERROR(InProgressError);

public:
    AbstractSession();

    virtual ~AbstractSession();

    /**
     * Determines whether the currently configured game session is in progress. Usually this
     * will not be the case during title sequences (for example).
     */
    bool hasBegun() const;

    /**
     * Returns the current map URI for the game session in progress. If the session has not
     * yet begun then an empty URI is returned.
     */
    res::Uri mapUri() const;

    /**
     * Determines whether the game state currently allows the session to be saved.
     */
    virtual bool isSavingPossible() = 0;

    /**
     * Determines whether the game state currently allows a saved session to be loaded.
     */
    virtual bool isLoadingPossible() = 0;

    /**
     * Save the current game state to a new @em user saved session.
     *
     * @param saveName         Name of the new saved session.
     * @param userDescription  Textual description of the current game state provided either
     *                         by the user or possibly generated automatically.
     */
    virtual void save(const de::String &saveName, const de::String &userDescription) = 0;

    /**
     * Load the game state from the @em user saved session specified.
     *
     * @param saveName  Name of the saved session to be loaded.
     */
    virtual void load(const de::String &saveName) = 0;

    const world::IThinkerMapping *thinkerMapping() const;

    /**
     * Sets the currently used serialization thinker mapping object.
     * @param mapping  Thinker mapping. Set to @c nullptr when the mapping is not
     *                 available. Caller retains ownership.
     */
    void setThinkerMapping(world::IThinkerMapping *mapping);

protected:
    void setMapUri(const res::Uri &uri);
    void setInProgress(bool inProgress);

//- Saved session management ------------------------------------------------------------

    /**
     * Makes a copy of the saved session specified.
     *
     * @param destPath    Path for the new/replaced saved session.
     * @param sourcePath  Path for the saved session to be copied.
     */
    static void copySaved(const de::String &destPath, const de::String &sourcePath);

    /**
     * Removes the saved session at @a path.
     */
    static void removeSaved(const de::String &path);

private:
    DE_PRIVATE(d)
};

#endif // LIBDOOMSDAY_SESSION_H
