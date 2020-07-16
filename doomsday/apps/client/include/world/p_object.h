/** @file p_object.h  World map objects.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef WORLD_P_OBJECT_H
#define WORLD_P_OBJECT_H

#if defined(__JDOOM__) || defined(__JHERETIC__) || defined(__JHEXEN__)
#  error Attempted to include internal Doomsday p_object.h from a game
#endif

#include <de/legacy/aabox.h>
#include <de/record.h>
#include <de/vector.h>
#include <doomsday/defs/ded.h>
#include <doomsday/world/thinker.h>
#include <doomsday/world/mobj.h>
#include <doomsday/world/bspleaf.h>
#include <doomsday/api_map.h>
#include "dd_def.h"
#ifdef __CLIENT__
#  include "resource/framemodeldef.h"
#endif

namespace world
{
    class Subsector;
    class Plane;
}

#define DEFAULT_FRICTION        FIX2FLT(0xe800)
#define NOMOMENTUM_THRESHOLD    (0.0001)

#define IS_BLOCK_LINKED(mo)     ((mo)->bNext != 0)

DE_EXTERN_C int useSRVO, useSRVOAngle;

/**
 * To be called to register the commands and variables of this module.
 */
void Mobj_ConsoleRegister();

/**
 * Set the origin of the map-object in map space.
 *
 * @return  @c true if successful, @c false otherwise. The object's position is not changed
 *          if the move fails.
 *
 * @note Internal to the engine.
 */
dd_bool Mobj_SetOrigin(mobj_t *mob, coord_t x, coord_t y, coord_t z);

/**
 * Returns @c true if the BSP leaf at the map-object's origin is known (i.e., it has been
 * linked into the map by calling @ref Mobj_SetOrigin() and has a convex geometry).
 *
 * @param mob  Map-object.
 */
bool Mobj_HasSubsector(const mobj_t &mob);

/**
 * Returns the subsector in which the map-object currently resides.
 *
 * @param mob  Map-object.
 *
 * @see Mobj_HasSubsector()
 */
world_Subsector &Mobj_Subsector(const mobj_t &mob);

/**
 * Returns a pointer to subsector in which the mobj currently resides, or @c nullptr
 * if not linked or the BSP leaf at the origin has no convex geometry.
 *
 * @param mob  Map-object.
 *
 * @see Mobj_HasSubsector()
 */
world_Subsector *Mobj_SubsectorPtr(const mobj_t &mob);

/**
 * Creates a new map-object triggered particle generator based on the given definition.
 * The generator is added to the list of active ptcgens.
 */
void Mobj_SpawnParticleGen(mobj_t *source, const ded_ptcgen_t *def);

#ifdef __CLIENT__

/**
 * Calculate the visible @a origin of @a mob in world space, including
 * any short range offset.
 */
void Mobj_OriginSmoothed(const mobj_t *mob, coord_t origin[3]);

angle_t Mobj_AngleSmoothed(const mobj_t *mob);

/**
 * Determines whether the Z origin of the mobj lies above the visual ceiling, or below the
 * visual floor plane of the BSP leaf at the origin. This can be used to determine whether
 * this origin should be adjusted with respect to smoothed plane movement.
 */
dd_bool Mobj_OriginBehindVisPlane(mobj_t *mob);

/**
 * To be called when Lumobjs are disabled to perform necessary bookkeeping.
 */
void Mobj_UnlinkLumobjs(mobj_t *mob);

/**
 * Generates Lumobjs for the map-object.
 * @note: This is called each frame for each luminous object!
 */
void Mobj_GenerateLumobjs(mobj_t *mob);

void Mobj_AnimateHaloOcclussion(mobj_t &mob);

/**
 * Calculate the strength of the shadow this map-object should cast.
 *
 * @note Implemented using a greatly simplified version of the lighting equation;
 *       no light diminishing or light range compression.
 */
float Mobj_ShadowStrength(const mobj_t &mob);

/**
 * Determines which of the available sprites is in effect for the current map-object state
 * and frame. May return @c nullptr if the state and/or frame is not valid.
 */
const de::Record *Mobj_SpritePtr(const mobj_t &mob);

/**
 * Determines which of the available model definitions (if any), are in effect for the
 * current map-object state and frame. (Interlinks are resolved).
 *
 * @param nextModef  If non-zero the model definition for the @em next frame is written here.
 * @param interp     If non-zero and both model definitions are found the current interpolation
 *                   point between the two is written here.
 *
 * @return  Active model definition for the current frame (if any).
 */
FrameModelDef *Mobj_ModelDef(const mobj_t &mob, FrameModelDef **nextModef = nullptr,
                        float *interp = nullptr);

/**
 * Calculates the shadow radius of the map-object. Falls back to Mobj_VisualRadius().
 *
 * @param mob  Map-object.
 *
 * @return Radius for shadow.
 */
coord_t Mobj_ShadowRadius(const mobj_t &mob);

void Mobj_SpawnDamageParticleGen(const mobj_t *mob, const mobj_t *inflictor, int amount);

#endif // __CLIENT__

coord_t Mobj_ApproxPointDistance(const mobj_t *mob, const coord_t *point);

/**
 * Returns the current "float bob" offset (if enabled); otherwise @c 0.
 */
coord_t Mobj_BobOffset(const mobj_t &mob);

float Mobj_Alpha(const mobj_t &mob);

/**
 * Returns the radius of the mobj as it would visually appear to be, according
 * to the current visualization (either a sprite or a 3D model).
 *
 * @param mob  Map-object.
 *
 * @see Mobj_Radius()
 */
coord_t Mobj_VisualRadius(const mobj_t &mob);

#endif  // WORLD_P_OBJECT_H
