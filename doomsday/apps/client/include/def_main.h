/** @file def_main.h  Definition subsystem.
 * @ingroup defs
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef DEFINITIONS_MAIN_H
#define DEFINITIONS_MAIN_H

#ifndef __cplusplus
#  error "def_main.h requires C++"
#endif

#include <vector>
#include <de/String>
#include <doomsday/defs/ded.h>  // ded_t
#include <doomsday/defs/dedtypes.h>
#include <doomsday/uri.h>

template <typename PODType>
struct Array : public std::vector<PODType>
{
    Array() : _elements(0) {}
    bool isEmpty() const {
        return !size();
    }
    int size() const {
        return std::vector<PODType>::size();
    }
    void clear() {
        _elements = nullptr;
        std::vector<PODType>::clear();
    }
    PODType *append(int count = 1) {
        DENG2_ASSERT(count >= 0);
        for(int i = 0; i < count; ++i) {
            std::vector<PODType>::push_back(PODType());
        }
        if(!isEmpty()) {
            _elements = &(*this)[0];
            return &_elements[size() - count];
        }
        return nullptr;
    }
    /// Determine the index of element @a elem. Performance is O(1).
    int indexOf(PODType const *elem) const {
        if(!elem) return 0;
        int index = elem - elements();
        DENG_ASSERT(index >= 0 && index < size());
        //if(index < 0 || index >= size()) return 0;
        return index;
    }
    PODType *elements() {
        return _elements;
    }
    PODType const *elements() const {
        return _elements;
    }
    PODType **elementsPtr() {
        return &_elements;
    }
private:
    PODType *_elements;
};

extern ded_t defs;  ///< Main definitions database (internal).
struct xgclass_s;   ///< @note The actual classes are on game side.

struct sfxinfo_t
{
    void *data;           ///< Pointer to sound data.
    lumpnum_t lumpNum;
    char lumpName[9];     ///< Actual lump name of the sound (full name).
    char id[32];          ///< Identifier name (from the def).
    char name[32];        ///< Long name.
    sfxinfo_t *link;      ///< Link to another sound.
    int linkPitch;
    int linkVolume;
    int priority;
    int channels;         ///< Max. channels for the sound to occupy.
    int usefulness;       ///< Used to determine when to cache out.
    int flags;
    int group;
    ddstring_t external;  ///< Path to external file.
};

struct stateinfo_t
{
    mobjinfo_t *owner;
    ded_light_t *light;
    ded_ptcgen_t *ptcGens;
};

/**
 * Definitions that have been preprocessed for runtime use. Some of these are
 * visible to the games via the InternalData API.
 */
struct RuntimeDefs
{
    Array<mobjinfo_t>  mobjInfo;   ///< Map object info database.
    Array<state_t>     states;     ///< State list.
    Array<stateinfo_t> stateInfo;
    Array<sfxinfo_t>   sounds;     ///< Sound effect list.
    Array<ddtext_t>    texts;      ///< Text string list.

    void clear();
};

extern RuntimeDefs runtimeDefs;

/**
 * Register the console commands and/or variables of this module.
 */
void Def_ConsoleRegister();

/**
 * Initializes the definition databases.
 */
void Def_Init();

/**
 * Destroy databases.
 */
void Def_Destroy();

/**
 * Finish definition database initialization. Initialization is split into two
 * phases either side of the texture manager, this being the post-phase.
 */
void Def_PostInit();

/**
 * Reads the specified definition files, and creates the sprite name,
 * state, mobjinfo, sound, music, text and mapinfo databases accordingly.
 */
void Def_Read();

de::String Def_GetStateName(state_t const *state);

/**
 * Can we reach 'snew' if we start searching from 'sold'?
 * Take a maximum of 16 steps.
 */
bool Def_SameStateSequence(state_t *snew, state_t *sold);

ded_compositefont_t *Def_GetCompositeFont(char const *uri);
ded_ptcgen_t *Def_GetGenerator(struct uri_s const *uri);
ded_ptcgen_t *Def_GetGenerator(de::Uri const &uri);
ded_ptcgen_t *Def_GetDamageGenerator(int mobjType);
ded_light_t *Def_GetLightDef(int spr, int frame);

state_t *Def_GetState(int num);

/**
 * Gets information about a defined sound. Linked sounds are resolved.
 *
 * @param soundId  ID number of the sound.
 * @param freq     Defined frequency for the sound is returned here. May be @c nullptr.
 * @param volume   Defined volume for the sound is returned here. May be @c nullptr.
 *
 * @return  Sound info (from definitions).
 */
sfxinfo_t *Def_GetSoundInfo(int soundId, float *freq, float *volume);

/**
 * Returns @c true if the given @a soundId is defined as a repeating sound.
 */
bool Def_SoundIsRepeating(int soundId);

/**
 * @return  @c true= the definition was found.
 */
int Def_Get(int type, char const *id, void *out);

/**
 * This is supposed to be the main interface for outside parties to
 * modify definitions (unless they want to do it manually with dedfile.h).
 */
int Def_Set(int type, int index, int value, void const *ptr);

#endif  // DEFINITIONS_MAIN_H
