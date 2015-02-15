/** @file session.h  Logical game session base class.
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

#ifndef LIBDENG2_SESSION_H
#define LIBDENG2_SESSION_H

#include "../Error"
#include "../Observers"
#include "../String"
#include <QMap>

namespace de {
namespace game {

class SavedSession;

/**
 * Base class for a logical game session. Derived classes implement the high level logic for
 * the manipulation and configuration of the game session.
 *
 * The game session exists at the same conceptual level as the logical game state. The primary
 * job of the derived class is to ensure that the current game state remains valid and provide
 * a mechanism for saving player progress.
 */
class DENG2_PUBLIC Session
{
public:
    /// Current in-progress state does not match that expected. @ingroup errors
    DENG2_ERROR(InProgressError);

public:
    virtual ~Session() {}

    /**
     * Configuration profile.
     */
    struct Profile
    {
        // Unique identifier of the game this profile is used with.
        String gameId;

        // List of resource files (specified via the command line or in a cfg, or found using
        // the default search algorithm (e.g., /auto and DOOMWADDIR)).
        QStringList resourceFiles;
    };

    /**
     * Returns the current configuration profile for the game session.
     */
    static Profile &profile();

    /// Convenient method of looking up the game identity key from the game session profile.
    static inline String gameId()   { return profile().gameId; }

    /// Compose the absolute path of the @em user saved session folder for the game session.
    static inline String savePath() { return String("/home/savegames") / profile().gameId; }

    /**
     * Determines whether the currently configured game session is in progress. Usually this
     * will not be the case during title sequences (for example).
     */
    virtual bool hasBegun() = 0;

    /**
     * Determines whether the game state currently allows the session to be saved.
     */
    virtual bool savingPossible() = 0;

    /**
     * Determines whether the game state currently allows a saved session to be loaded.
     */
    virtual bool loadingPossible() = 0;

    /**
     * Save the current game state to a new @em user saved session.
     *
     * @param saveName         Name of the new saved session.
     * @param userDescription  Textual description of the current game state provided either
     *                         by the user or possibly generated automatically.
     */
    virtual void save(String const &saveName, String const &userDescription) = 0;

    /**
     * Load the game state from the @em user saved session specified.
     *
     * @param saveName  Name of the saved session to be loaded.
     */
    virtual void load(String const &saveName) = 0;

protected: // Saved session management -------------------------------------------------------

    /**
     * Makes a copy of the saved session specified.
     *
     * @param destPath    Path for the new/replaced saved session.
     * @param sourcePath  Path for the saved session to be copied.
     */
    static void copySaved(String const &destPath, String const &sourcePath);

    /**
     * Removes the saved session at @a path.
     */
    static void removeSaved(String const &path);

public: // Saved session index ---------------------------------------------------------------

    /// @todo Integrate this functionality into the filesystem.
    class DENG2_PUBLIC SavedIndex
    {
    public:
        /// Notified whenever a saved session is added/removed from the index.
        DENG2_DEFINE_AUDIENCE2(AvailabilityUpdate, void savedIndexAvailabilityUpdate(SavedIndex const &index))

        typedef QMap<String, SavedSession *> All;

    public:
        SavedIndex();

        /// Lookup a SavedSession by absolute @a path.
        SavedSession *find(String path) const;

        /// Add an entry for the saved @a session, replacing any existing one.
        void add(SavedSession &session);

        /// Remove the entry for the saved session with absolute @a path (if present).
        void remove(String path);

        void clear();

        /// Provides access to the entry dataset, for efficient traversal.
        All const &all() const;

    private:
        DENG2_PRIVATE(d)
    };

    /**
     * Provides access to the (shared) saved session index.
     */
    static SavedIndex &savedIndex();
};

} // namespace game
} // namespace de

#endif // LIBDENG2_SESSION_H
