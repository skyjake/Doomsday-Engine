/**
 * @file s_environ.h
 * Sound environment. @ingroup audio
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_SOUND_ENVIRON_H
#define LIBDENG_SOUND_ENVIRON_H

#include "p_mapdata.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Re-calculate the reverb properties of the given sector. Should be called
 * whenever any of the properties governing reverb properties have changed
 * (i.e. hedge/plane texture or plane height changes).
 *
 * PRE: BspLeaf attributors must have been determined first.
 *
 * @param sec  Ptr to the sector to calculate reverb properties of.
 */
void S_CalcSectorReverb(Sector* sec);

/**
 * Called during map init to determine which BSP leafs affect the reverb
 * properties of each sector. Given that BSP leafs do not change shape (in
 * two dimensions at least), they do not move and are not created/destroyed
 * once the map has been loaded; this step can be pre-processed.
 */
void S_DetermineBspLeafsAffectingSectorReverb(GameMap* map);

/**
 * Recalculate reverb properties in the sectors where update has been requested.
 */
void S_UpdateReverb(void);

/**
 * Must be called when the map changes.
 */
void S_ResetReverb(void);

/**
 * @return  Environment class name for identifier @a mclass.
 */
const char* S_MaterialEnvClassName(material_env_class_t mclass);

/**
 * @return  Environment class associated with material @a uri else @c MEC_UNKNOWN.
 */
material_env_class_t S_MaterialEnvClassForUri(const Uri* uri);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_SOUND_ENVIRON_H */

