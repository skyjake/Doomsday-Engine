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
#ifdef __cplusplus

#include "common.h"
#include <de/Error>
#include <de/Path>

class SaveInfo;

/**
 * Maps saved games into a finite set of "save slots".
 *
 * @todo At present the associated SaveInfos are actually owned by this class. In the future
 * these should be referenced from another container which has none of the restrictions of
 * the slot-based mechanism.
 *
 * @ingroup libcommon
 */
class SaveSlots
{
public:
    /// An invalid slot was specified. @ingroup errors
    DENG2_ERROR(InvalidSlotError);

    /**
     * Logical save slot.
     */
    class Slot
    {
    public:
        /// Required SaveInfo is missing. @ingroup errors
        DENG2_ERROR(MissingInfoError);

    public:
        Slot(de::String const &fileName = "");

        /**
         * Returns the save game file name bound to the logical save slot.
         */
        de::String fileName() const;

        /**
         * Change the save game file name bound to the logical save slot.
         *
         * @param newName  New save game file name to be bound.
         */
        void bindFileName(de::String newName);

        /**
         * Returns @c true iff a saved game state exists for the logical save slot.
         */
        bool isUsed() const;

        /**
         * Returns @c true iff save info exists for the logical save slot.
         */
        bool hasSaveInfo() const;

        /**
         * Clear the save info for the logical save slot.
         *
         * @see hasSaveInfo()
         */
        void clearSaveInfo();

        /**
         * Returns the SaveInfo associated with the logical save slot.
         *
         * @see hasSaveInfo()
         */
        SaveInfo &saveInfo() const;

        /**
         * Replace the existing save info with @a newInfo.
         *
         * @param newInfo  New SaveInfo to replace with. Ownership is given.
         */
        void replaceSaveInfo(SaveInfo *newInfo);

    private:
        DENG2_PRIVATE(d)
    };

public:
    /**
     * @param numSlots  Number of logical slots.
     */
    SaveSlots(int numSlots);

    /**
     * Returns the total number of logical save slots.
     */
    int slotCount() const;

    /// @see slotCount()
    inline int size() const { return slotCount(); }

    /**
     * Returns @c true iff @a value is interpretable as logical slot number (in range).
     *
     * @see slotCount()
     */
    bool isKnownSlot(int value) const;

    /**
     * Composes the textual, symbolic identifier/name for save @a slotNumber.
     *
     * @see parseSlotIdentifier()
     */
    de::String slotIdentifier(int slotNumber) const;

    /**
     * Parse @a str and determine whether it references a logical game-save slot.
     *
     * @param str  String to be parsed. Parse is divided into three passes.
     *             Pass 1: Check for a known game-save name which matches this.
     *                 Search is in ascending logical slot order 0..N (where N is the number
     *                 of available save slots).
     *             Pass 2: Check for keyword identifiers.
     *                 <auto>  = The "auto save" slot.
     *                 <last>  = The last used slot.
     *                 <quick> = The currently nominated "quick save" slot.
     *             Pass 3: Check for a logical save slot number.
     *
     * @return  The parsed slot number if valid; otherwise @c -1
     *
     * @see slotIdentifier()
     */
    int parseSlotIdentifier(de::String str) const;

    /// @see slot()
    inline Slot &operator [] (int slotNumber) {
        return slot(slotNumber);
    }

    /**
     * Returns the logical save slot associated with @a slotNumber.
     *
     * @see isKnownSlot()
     */
    Slot &slot(int slotNumber) const;

    /**
     * Clears save info for all logical save slots.
     */
    void clearAll();

    /**
     * Force an update of the cached game-save info. To be called (sparingly) at strategic
     * points when an update is necessary (e.g., the game-save paths have changed).
     *
     * @note It is not necessary to call this after a game-save is made, this module will do
     * so automatically.
     */
    void updateAll();

    /**
     * Lookup a save slot by searching for a match on game-save description. The search is in
     * ascending logical slot order 0...N (where N is the number of available save slots).
     *
     * @param description  Description of the game-save to look for (not case sensitive).
     *
     * @return  Logical slot number of the found game-save else @c -1
     */
    int findSlotWithUserSaveDescription(de::String description) const;

    /**
     * Returns @c true iff save @a slotNumber is user-writable (i.e., not a special slot, such
     * as the @em auto and @em base slots).
     */
    bool slotIsUserWritable(int slotNumber) const;

    /**
     * Deletes all save game files associated with the specified save @a slotNumber.
     *
     * @see isKnownSlot()
     */
    void clearSlot(int slotNumber);

    /**
     * Copies all the save game files from one slot to another.
     */
    void copySlot(int sourceSlotNumber, int destSlotNumber);

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

#endif // __cplusplus
#endif // LIBCOMMON_SAVESLOTS_H
