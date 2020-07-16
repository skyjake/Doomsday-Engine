/**
 * @file info.h
 * Original game value LUTs and accessor mechanisms. @ingroup dehread
 *
 * @author Copyright &copy; 2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDEHREAD_INFO_H
#define LIBDEHREAD_INFO_H

#include <de/string.h>
#include "importdeh.h"

struct FinaleBackgroundMapping
{
    const de::String text;
    const de::String mnemonic;
};

/**
 * Lookup a record in the FinaleBackgrounMapping table by the flat name that
 * was originally used for this background.
 *
 * @param text          Deh label/name (identifier) of the sound to search for.
 * @param mapping       If not @c NULL the address of the FinaleBackgroundMapping
 *                      record will be written back here. Caller should not derive
 *                      meaning from the returned address itself.
 *
 * @return Index of the found finale-background-mapping in the mapping table (0-index).
 *         Returns a negative value if not found.
 */
int findFinaleBackgroundMappingForText(const de::String& text, const FinaleBackgroundMapping** mapping);

struct FlagMapping
{
    ushort bit;
    int group;
    const de::String dehLabel;
};

/**
 * Lookup a record in the mobjtype FlagMapping table by it's Dehacked name.
 *
 * @param dehLabel      Deh label/name (identifier) of the flag to search for.
 * @param mapping       If not @c NULL the address of the FlagMapping record
 *                      will be written back here. Caller should not derive
 *                      meaning from the returned address itself.
 *
 * @return Index of the found flag-mapping in the mapping table (0-index).
 *         Returns a negative value if not found.
 */
int findMobjTypeFlagMappingByDehLabel(const de::String& dehLabel, const FlagMapping** mapping=NULL);

struct SoundMapping
{
    const de::String dehLabel;
    int id;
    const de::String name;
};

/**
 * Lookup a record in the SoundMapping table by it's Dehacked name.
 *
 * @param dehLabel      Deh label/name (identifier) of the sound to search for.
 * @param mapping       If not @c NULL the address of the SoundMapping record
 *                      will be written back here. Caller should not derive
 *                      meaning from the returned address itself.
 *
 * @return Index of the found sound-mapping in the mapping table (0-index).
 *         Returns a negative value if not found.
 */
int findSoundMappingByDehLabel(const de::String& dehLabel, const SoundMapping** mapping);

struct StateMapping
{
    const de::String dehLabel;
    statename_t id;
    const de::String name;
};

/**
 * Lookup a record in the StateMapping table by it's Dehacked name.
 *
 * @param dehLabel      Deh label/name (identifier) of the state to search for.
 * @param mapping       If not @c NULL the address of the StateMapping record
 *                      will be written back here. Caller should not derive
 *                      meaning from the returned address itself.
 *
 * @return Index of the found state-mapping in the mapping table (0-index).
 *         Returns a negative value if not found.
 */
int findStateMappingByDehLabel(const de::String& dehLabel, const StateMapping** mapping=NULL);

/// @todo Should be defined by the engine.
typedef enum {
    WSN_UP,
    WSN_DOWN,
    WSN_READY,
    WSN_ATTACK,
    WSN_FLASH,
    NUM_WEAPON_STATE_NAMES
} weaponstatename_t;

struct WeaponStateMapping
{
    const de::String dehLabel;
    weaponstatename_t id;
    const de::String name;
};

int findWeaponStateMappingByDehLabel(const de::String& dehLabel, const WeaponStateMapping** mapping=NULL);

struct TextMapping
{
    const de::String name; ///< Unique name/identifier for the text.
    const de::String text; ///< Original text string associated with the name.
};

/**
 * Lookup a record in the TextMapping table using the original text blob.
 *
 * @param origText      Unique name/identifier of the text to search for.
 * @param mapping       If not @c NULL the address of the TextMapping record
 *                      will be written back here. Caller should not derive
 *                      meaning from the returned address itself.
 *
 * @return Index of the found Text in the mapping table (0-index).
 *         Returns a negative value if not found.
 */
int textMappingForBlob(const de::String& origText, const TextMapping** mapping=NULL);

struct ValueMapping
{
    const de::String dehLabel;
    const de::String valuePath;
};

/**
 * Lookup a record in the StateMapping table by it's Dehacked name.
 *
 * @param dehLabel      Deh label/name (identifier) of the state to search for.
 * @param mapping       If not @c NULL the address of the StateMapping record
 *                      will be written back here. Caller should not derive
 *                      meaning from the returned address itself.
 *
 * @return Index of the found state-mapping in the mapping table (0-index).
 *         Returns a negative value if not found.
 */
int findValueMappingForDehLabel(const de::String& dehLabel, const ValueMapping** mapping=NULL);

/**
 * Lookup a record in the music lump name table.
 *
 * @param name          Unique name of the music to lookup.
 * @return Index of the found music name in the mapping table (0-index).
 *         Returns a negative value if not found.
 */
int findMusicLumpNameInMap(const de::String& name);

/**
 * Lookup a record in the sound lump name table.
 *
 * @param name          Unique name of the sound to lookup.
 * @return Index of the found sound name in the mapping table (0-index).
 *         Returns a negative value if not found.
 */
int findSoundLumpNameInMap(const de::String& name);

/**
 * Lookup a record in the sprite name table.
 *
 * @param name          Unique name of the sprite to lookup.
 * @return Index of the found sprite name in the mapping table (0-index).
 *         Returns a negative value if not found.
 */
int findSpriteNameInMap(const de::String& name);

/**
 * Given an @a offset to a state's mobj action code pointer in the original
 * executable, lookup the associated state index.
 *
 * @return Index of the associated state or a negative value if not found/OOR
 */
int stateIndexForActionOffset(int offset);

int originalHeightForMobjType(int type);

#endif // LIBDEHREAD_INFO_H
