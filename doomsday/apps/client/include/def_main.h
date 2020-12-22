/** @file def_main.h  Definition subsystem.
 * @ingroup defs
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/string.h>
#include <doomsday/defs/ded.h>  // ded_t
#include <doomsday/defs/dedtypes.h>
#include <doomsday/uri.h>

struct xgclass_s;   ///< @note The actual classes are on game side.

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

de::String Def_GetStateName(const state_t *state);

/**
 * Can we reach 'snew' if we start searching from 'sold'?
 * Take a maximum of 16 steps.
 */
bool Def_SameStateSequence(state_t *snew, state_t *sold);

ded_compositefont_t *Def_GetCompositeFont(const char *uri);
ded_ptcgen_t *Def_GetGenerator(const struct uri_s *uri);
ded_ptcgen_t *Def_GetGenerator(const res::Uri &uri);
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
int Def_Get(int type, const char *id, void *out);

/**
 * This is supposed to be the main interface for outside parties to
 * modify definitions (unless they want to do it manually with dedfile.h).
 */
int Def_Set(int type, int index, int value, const void *ptr);

#endif  // DEFINITIONS_MAIN_H
