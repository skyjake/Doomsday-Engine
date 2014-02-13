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
#include "saveinfo.h"

#ifdef __cplusplus
#include <de/Error>

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

public:
    /**
     * @param numSlots  Number of logical slots.
     */
    SaveSlots(int numSlots);

    void clearAllSaveInfo();

    /**
     * Force an update of the cached game-save info. To be called (sparingly) at strategic
     * points when an update is necessary (e.g., the game-save paths have changed).
     *
     * @note It is not necessary to call this after a game-save is made, this module will do
     * so automatically.
     */
    void updateAllSaveInfo();

    /**
     * Returns the total number of logical save slots.
     */
    int slotCount() const;

    /**
     * Returns @c true iff @a slot is a valid logical slot number (in range).
     *
     * @see slotCount()
     */
    bool isValidSlot(int slot) const;

    /**
     * Composes the textual identifier/name for save @a slot.
     *
     * @see parseSlotIdentifier()
     */
    AutoStr *composeSlotIdentifier(int slot) const;

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
     * @see composeSlotIdentifier()
     */
    int parseSlotIdentifier(char const *str) const;

    /**
     * Lookup a save slot by searching for a match on game-save description. The search is in
     * ascending logical slot order 0...N (where N is the number of available save slots).
     *
     * @param description  Description of the game-save to look for (not case sensitive).
     *
     * @return  Logical slot number of the found game-save else @c -1
     */
    int findSlotWithSaveDescription(char const *description) const;

    /**
     * Returns @c true iff a saved game state exists for save @a slot.
     */
    bool slotInUse(int slot) const;

    /**
     * Returns @c true iff save @a slot is user-writable (i.e., not a special slot, such as
     * the @em auto and @em base slots).
     */
    bool slotIsUserWritable(int slot) const;

    /**
     * Returns the SaveInfo associated with the logical save @a slot.
     *
     * @see isValidSlot()
     */
    SaveInfo &saveInfo(int slot) const;

    inline SaveInfo *saveInfoPtr(int slot) const {
        return isValidSlot(slot)? &saveInfo(slot) : 0;
    }

    /**
     * Deletes all save game files associated with the specified save @a slot.
     *
     * @see isValidSlot()
     */
    void clearSlot(int slot);

    /**
     * @param slot     Slot to replace the info of.
     * @param newInfo  New SaveInfo to replace with. Ownership is given.
     */
    void replaceSaveInfo(int slot, SaveInfo *newInfo);

    /**
     * Copies all the save game files from one slot to another.
     */
    void copySlot(int sourceSlot, int destSlot);

    /**
     * Compose the (possibly relative) file path to the game state associated with save @a slot.
     *
     * @param slot  Slot to compose the identifier of.
     * @param map   If @c >= 0 include this logical map index in the composed path.
     *
     * @return  The composed path if reachable (else a zero-length string).
     */
    AutoStr *composeSavePathForSlot(int slot, int map = -1) const;

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
#endif // __cplusplus

// C wrapper API ---------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#else
typedef void *SaveSlots;
#endif

SaveSlots *SaveSlots_New(int slotCount);
void SaveSlots_Delete(SaveSlots *sslots);

void SaveSlots_ClearAllSaveInfo(SaveSlots *sslots);
void SaveSlots_UpdateAllSaveInfo(SaveSlots *sslots);
int SaveSlots_SlotCount(SaveSlots const *sslots);
dd_bool SaveSlots_IsValidSlot(SaveSlots const *sslots, int slot);
AutoStr *SaveSlots_ComposeSlotIdentifier(SaveSlots const *sslots, int slot);
int SaveSlots_ParseSlotIdentifier(SaveSlots const *sslots, char const *str);
int SaveSlots_FindSlotWithSaveDescription(SaveSlots const *sslots, char const *description);
dd_bool SaveSlots_SlotInUse(SaveSlots const *sslots, int slot);
dd_bool SaveSlots_SlotIsUserWritable(SaveSlots const *sslots, int slot);
SaveInfo *SaveSlots_SaveInfo(SaveSlots *sslots, int slot);
void SaveSlots_ReplaceSaveInfo(SaveSlots *sslots, int slot, SaveInfo *newInfo);
void SaveSlots_ClearSlot(SaveSlots *sslots, int slot);
void SaveSlots_CopySlot(SaveSlots *sslots, int sourceSlot, int destSlot);
AutoStr *SaveSlots_ComposeSavePathForSlot(SaveSlots const *sslots, int slot, int map);

void SaveSlots_ConsoleRegister();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_SAVESLOTS_H
