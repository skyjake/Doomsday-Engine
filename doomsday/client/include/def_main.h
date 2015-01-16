/** @file def_main.h  Definition subsystem.
 *
 * @ingroup defs
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_DEFINITIONS_MAIN_H
#define LIBDENG_DEFINITIONS_MAIN_H

#include <vector>
#include <doomsday/defs/ded.h>
#include <doomsday/defs/dedtypes.h>
#include <de/stringarray.h>
#include "Material"

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

#ifdef __cplusplus
extern "C" {
#endif

// the actual classes are game-side
struct xgclass_s;

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

extern ded_t defs;  ///< Main definitions database (internal).

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
    Array<sprname_t>   sprNames;   ///< Sprite name list.
    Array<mobjinfo_t>  mobjInfo;   ///< Map object info database.
    Array<state_t>     states;     ///< State list.
    Array<stateinfo_t> stateInfo;
    Array<sfxinfo_t>   sounds;     ///< Sound effect list.
    Array<ddtext_t>    texts;      ///< Text string list.

    void clear();
};

extern RuntimeDefs runtimeDefs;

/**
 * Initializes the definition databases.
 */
void Def_Init(void);

/**
 * Retrieves the XG Class list from the Game.
 * XGFunc links are provided by the Game, who owns the actual
 * XG classes and their functions.
 */
int Def_GetGameClasses(void);

/**
 * Finish definition database initialization. Initialization is split into two
 * phases either side of the texture manager, this being the post-phase.
 */
void Def_PostInit(void);

/**
 * Destroy databases.
 */
void Def_Destroy(void);

/**
 * Reads the specified definition files, and creates the sprite name,
 * state, mobjinfo, sound, music, text and mapinfo databases accordingly.
 */
void Def_Read(void);

int Def_GetMobjNum(char const *id);
int Def_GetMobjNumForName(char const *name);
char const *Def_GetMobjName(int num);

state_t *Def_GetState(int num);
int Def_GetStateNum(char const *id);
char const *Def_GetStateName(state_t *state);

int Def_GetActionNum(char const *id);

/**
 * Returns the unique sprite number associated with the specified sprite @a name;
 * otherwise @c -1 if not found.
 */
spritenum_t Def_GetSpriteNum(char const *name);

int Def_GetModelNum(char const *id);
int Def_GetMusicNum(char const *id);
int Def_GetSoundNum(char const *id);
ded_value_t *Def_GetValueById(char const *id);
ded_value_t *Def_GetValueByUri(Uri const *uri);
ded_material_t *Def_GetMaterial(char const *uri);
ded_compositefont_t *Def_GetCompositeFont(char const *uri);
ded_light_t *Def_GetLightDef(int spr, int frame);

#ifdef __cplusplus
} // extern "C"
#endif

spritenum_t Def_GetSpriteNum(de::String const &name);

ded_ptcgen_t *Def_GetGenerator(Uri const *uri);
ded_ptcgen_t *Def_GetGenerator(de::Uri const &uri);

#ifdef __cplusplus
extern "C" {
#endif

ded_ptcgen_t *Def_GetDamageGenerator(int mobjType);

int Def_EvalFlags(char const *string);

/**
 * @return  @c true= the definition was found.
 */
int Def_Get(int type, char const *id, void *out);

/**
 * This is supposed to be the main interface for outside parties to
 * modify definitions (unless they want to do it manually with dedfile.h).
 */
int Def_Set(int type, int index, int value, void const *ptr);

/**
 * Can we reach 'snew' if we start searching from 'sold'?
 * Take a maximum of 16 steps.
 */
dd_bool Def_SameStateSequence(state_t *snew, state_t *sold);

/**
 * Compiles a list of all the defined mobj types. Indices in this list
 * match those in the @c mobjInfo array.
 *
 * @return StringArray instance. Caller gets ownership.
 */
StringArray *Def_ListMobjTypeIDs(void);

/**
 * Compiles a list of all the defined mobj states. Indices in this list
 * match those in the @c states array.
 *
 * @return StringArray instance. Caller gets ownership.
 */
StringArray *Def_ListStateIDs(void);

/**
 * Returns @c true iff @a def is compatible with the specified context.
 */
bool Def_IsAllowedDecoration(ded_decoration_t const *def, /*bool hasExternal,*/ bool isCustom);

/**
 * Returns @c true iff @a def is compatible with the specified context.
 */
bool Def_IsAllowedReflection(ded_reflection_t const *def, /*bool hasExternal,*/ bool isCustom);

/**
 * Returns @c true iff @a def is compatible with the specified context.
 */
bool Def_IsAllowedDetailTex(ded_detailtexture_t const *def, /*bool hasExternal,*/ bool isCustom);

D_CMD(ListMobjs);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // LIBDENG_DEFINITIONS_MAIN_H
