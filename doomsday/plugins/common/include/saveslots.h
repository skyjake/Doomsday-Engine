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

#include "common.h"
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
        Slot(de::String id, bool userWritable, de::String const &fileName,
             int gameMenuWidgetId = 0);

        /**
         * Returns @c true iff a saved game session exists for the logical save slot.
         */
        bool isUsed() const;

        /**
         * Returns @c true iff the logical save slot is user-writable.
         */
        bool isUserWritable() const;

        /**
         * Returns the unique identifier/name for the logical save slot.
         */
        de::String const &id() const;

        /**
         * Returns the session file name bound to the logical save slot.
         */
        de::String const &fileName() const;

        /**
         * Change the session file name bound to the logical save slot.
         *
         * @param newName  New session file name to be bound.
         */
        void bindFileName(de::String newName);

        /**
         * Returns the saved session for the logical save slot.
         */
        de::game::SavedSession &savedSession() const;

        /**
         * Deletes the saved session package file for the logical save slot, from the repository.
         */
        void clear();

        /**
         * Replace the existing saved session with @a newSession.
         *
         * @param newSession  New SavedSession to replace with. Ownership is given.
         */
        void replaceSavedSession(de::game::SavedSession *newSession);

    private:
        DENG2_PRIVATE(d)
    };

public:
    SaveSlots();

    /**
     * Add a new logical save slot.
     *
     * @param id                Unique identifier for this slot.
     * @param userWritable      @c true= allow the user to write to this slot.
     * @param fileName          File name to bind to this slot.
     * @param gameMenuWidgetId  Unique identifier of the game menu widget to associate this slot with;
     *                          otherwise @c 0= none.
     */
    void addSlot(de::String id, bool userWritable, de::String fileName, int gameMenuWidgetId = 0);

    /**
     * Returns the total number of logical save slots.
     */
    int slotCount() const;

    /// @see slotCount()
    inline int size() const { return slotCount(); }

    /**
     * Returns @c true iff @a value is interpretable as a logical slot identifier.
     */
    bool hasSlot(de::String value) const;

    /// @see slot()
    inline Slot &operator [] (de::String slotId) {
        return slot(slotId);
    }

    /**
     * Returns the logical save slot associated with @a slotId.
     *
     * @see hasSlot()
     */
    Slot &slot(de::String slotId) const;

    /**
     * Returns the logical save slot associated with the given saved @a session.
     */
    Slot *slot(de::game::SavedSession const *session) const;

    /**
     * Copies the saved session file from one slot to another.
     *
     * @see hasSlot()
     */
    void copySlot(de::String sourceSlotId, de::String destSlotId);

    /**
     * Force an update of all the saved sessions. To be called (sparingly) at strategic points
     * when an update is necessary (e.g., the game-save paths have changed).
     *
     * @note It is not necessary to call this after a game-save is made.
     */
    void updateAll();

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
