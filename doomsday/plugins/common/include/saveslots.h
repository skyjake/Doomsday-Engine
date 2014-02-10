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
#include "p_savedef.h" /// @todo remove me

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
    SaveSlots();

    void clearSaveInfo();

    /// Re-build game-save info by re-scanning the save paths and populating the list.
    void buildSaveInfo();

    /**
     * Force an update of the cached game-save info. To be called (sparingly) at strategic
     * points when an update is necessary (e.g., the game-save paths have changed).
     *
     * @note It is not necessary to call this after a game-save is made, this module will do
     * so automatically.
     */
    void updateAllSaveInfo();

    /**
     * Composes the textual identifier/name for save @a slot.
     */
    AutoStr *composeSlotIdentifier(int slot);

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
     * @return  Save slot identifier of the slot else @c -1
     */
    int parseSlotIdentifier(char const *str);

    /**
     * Lookup a save slot by searching for a match on game-save name. Search is in ascending
     * logical slot order 0...N (where N is the number of available save slots).
     *
     * @param name  Name of the game-save to look for. Case insensitive.
     *
     * @return  Logical slot number of the found game-save else @c -1
     */
    int slotForSaveName(char const *description);

    /**
     * Returns @c true iff a game-save is present for logical save @a slot.
     */
    bool slotInUse(int slot);

    /**
     * Returns @c true iff @a slot is a valid logical save slot.
     */
    bool isValidSlot(int slot);

    /**
     * Returns @c true iff @a slot is user-writable save slot (i.e., its not one of the special
     * slots such as @em auto).
     */
    bool isUserWritableSlot(int slot);

    /**
     * Returns the save info for save @a slot. Always returns SaveInfo even if supplied with an
     * invalid or unused slot identifer (a null object).
     */
    SaveInfo *findSaveInfoForSlot(int slot);

    void replaceSaveInfo(int slot, SaveInfo *newInfo);

    /**
     * Deletes all save game files associated with a slot number.
     */
    void clearSlot(int slot);

    /**
     * Copies all the save game files from one slot to another.
     */
    void copySlot(int sourceSlot, int destSlot);

    /**
     * Compose the (possibly relative) path to save state associated with the logical save @a slot.
     *
     * @param slot  Logical save slot identifier.
     * @param map   If @c >= 0 include this logical map index in the composed path.
     *
     * @return  The composed path if reachable (else a zero-length string).
     */
    AutoStr *composeGameSavePathForSlot(int slot, int map = -1);

private:
    DENG2_PRIVATE(d)
};

#endif // LIBCOMMON_MAPSTATEREADER_H
