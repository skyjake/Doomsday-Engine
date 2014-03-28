/** @file saveslots.h  Map of logical game save slots.
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

#ifndef LIBCOMMON_SAVESLOTS_H
#define LIBCOMMON_SAVESLOTS_H

#include <de/Error>
#include <de/game/SavedSession>
#include <de/Path>
#include <de/String>

/**
 * Maps save-game session file names into a finite set of "save slots".
 *
 * @ingroup libcommon
 */
class SaveSlots
{
public:
    /// A missing/unknown slot was referenced. @ingroup errors
    DENG2_ERROR(MissingSlotError);

    /**
     * Logical save slot.
     */
    class Slot
    {
    public:
        /// No SavedSession exists for the logical save slot. @ingroup errors
        DENG2_ERROR(MissingSessionError);

        /// Logical saved session status:
        enum SessionStatus {
             Loadable,
             Incompatible,
             Unused
        };

    public:
        Slot(de::String id, bool userWritable, de::String savePath, int menuWidgetId = 0);

        /**
         * Returns the logical status of the saved session associated with the logical save slot.
         */
        SessionStatus sessionStatus() const;

        inline bool isLoadable() const     { return sessionStatus() == Loadable; }
        inline bool isIncompatible() const { return sessionStatus() == Incompatible; }
        inline bool isUnused() const       { return sessionStatus() == Unused; }

        /**
         * Returns @c true iff the logical save slot is user-writable.
         */
        bool isUserWritable() const;

        /**
         * Returns @c true iff a saved game session exists for the logical save slot.
         */
        bool hasSavedSession() const;

        /**
         * Convenient method for determining whether a loadable saved session exists for the
         * logical save slot.
         *
         * @see hasSavedSession(), isLoadable()
         */
        inline bool hasLoadableSavedSession() const {
            return hasSavedSession() && isLoadable();
        }

        /**
         * Returns the saved session for the logical save slot.
         */
        de::game::SavedSession &savedSession() const;

        /**
         * Change the saved session linked with the logical save slot. It is not usually
         * necessary to call this.
         *
         * @param newSession  New SavedSession to apply. Use @c 0 to clear.
         */
        void setSavedSession(de::game::SavedSession *newSession);

        /**
         * Copies the saved session from the @a source slot.
         */
        void copySavedSession(Slot const &source);

        /**
         * Returns the unique identifier/name for the logical save slot.
         */
        de::String const &id() const;

        /**
         * Returns the absolute path of the saved session, bound to the logical save slot.
         */
        de::String const &savePath() const;

        /**
         * Change the absolute path of the saved session, bound to the logical save slot.
         *
         * @param newPath  New absolute path of the saved session to bind to.
         */
        void bindSavePath(de::String newPath);

        /**
         * Deletes the saved session linked to the logical save slot (if any).
         */
        void clear();

    private:
        DENG2_PRIVATE(d)
    };

public:
    SaveSlots();

    /**
     * Add a new logical save slot.
     *
     * @param id              Unique identifier for this slot.
     * @param userWritable    @c true= allow the user to write to this slot.
     * @param repositoryPath  Relative path in the repository to bind to this slot.
     * @param menuWidgetId    Unique identifier of the game menu widget to associate this slot with.
     *                        Use @c 0 for none.
     */
    void add(de::String id, bool userWritable, de::String savePath, int menuWidgetId = 0);

    /**
     * Returns the total number of logical save slots.
     */
    int count() const;

    /// @copydoc count()
    inline int size() const { return count(); }

    /**
     * Returns @c true iff @a value is interpretable as a logical slot identifier.
     */
    bool has(de::String value) const;

    /// @see slot()
    inline Slot &operator [] (de::String slotId) {
        return slot(slotId);
    }

    /**
     * Returns the logical save slot associated with @a slotId.
     *
     * @see has()
     */
    Slot &slot(de::String slotId) const;

    /**
     * Returns the logical save slot associated with the given saved @a session.
     */
    Slot *slot(de::game::SavedSession const *session) const;

    /**
     * Register the console commands and variables of this module.
     *
     * - game-save-last-slot   Last-used slot. Can be @c -1 (not set).
     * - game-save-quick-slot  Nominated quick-save slot. Can be @c -1 (not set).
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

typedef SaveSlots::Slot SaveSlot;

#endif // LIBCOMMON_SAVESLOTS_H
