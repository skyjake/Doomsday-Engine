/** @file p_object.h  World map objects.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_P_OBJECT_H
#define DENG_WORLD_P_OBJECT_H

#if defined(__JDOOM__) || defined(__JHERETIC__) || defined(__JHEXEN__)
#  error Attempted to include internal Doomsday p_object.h from a game
#endif

#include "api_map.h"
#include "def_data.h"
#ifdef __CLIENT__
#  include "ModelDef"
#  include "Sprite"
#endif
#include <de/Vector>
#include <de/aabox.h>

class BspLeaf;
class Plane;
class SectorCluster;

// This macro can be used to calculate a mobj-specific 'random' number.
#define MOBJ_TO_ID(mo)          ( (long)(mo)->thinker.id * 48 + ((unsigned long)(mo)/1000) )

// We'll use the base mobj template directly as our mobj.
typedef struct mobj_s
{
    DD_BASE_MOBJ_ELEMENTS()
} mobj_t;

#define MOBJ_SIZE               gx.mobjSize

#define DEFAULT_FRICTION        FIX2FLT(0xe800)
#define NOMOMENTUM_THRESHOLD    (0.0001)

#define IS_BLOCK_LINKED(mo)     ((mo)->bNext != 0)

DENG_EXTERN_C int useSRVO, useSRVOAngle;

void P_InitUnusedMobjList();

/**
 * To be called to register the commands and variables of this module.
 */
void Mobj_ConsoleRegister();

mobj_t *P_MobjCreate(thinkfunc_t function, de::Vector3d const &origin, angle_t angle,
    coord_t radius, coord_t height, int ddflags);

void P_MobjRecycle(mobj_t *mobj);

/**
 * Returns the map in which the mobj exists. Note that a mobj may exist in a map
 * while not being @em linked into data structures such as the blockmap and sectors.
 * To determine whether the mobj is linked, call @ref Mobj_IsLinked().
 *
 * @see Thinker_Map()
 */
de::Map &Mobj_Map(mobj_t const &mobj);

/**
 * Returns @c true iff the mobj has been linked into the map. The only time this
 * is not true is if @ref Mobj_SetOrigin() has not yet been called.
 *
 * @param mobj  Mobj instance.
 *
 * @todo Automatically link all new mobjs into the map (making this redundant).
 */
bool Mobj_IsLinked(mobj_t const &mobj);

/**
 * Returns a copy of the mobj's map space origin.
 */
de::Vector3d Mobj_Origin(mobj_t const &mobj);

/**
 * Returns the mobj's visual center (i.e., origin plus z-height offset).
 */
de::Vector3d Mobj_Center(mobj_t &mobj);

/**
 * Sets a mobj's position.
 *
 * @return  @c true if successful, @c false otherwise. The object's position is
 *          not changed if the move fails.
 *
 * @note Internal to the engine.
 */
dd_bool Mobj_SetOrigin(mobj_t *mobj, coord_t x, coord_t y, coord_t z);

/**
 * Returns the map BSP leaf at the origin of the mobj. Note that the mobj must
 * be linked in the map (i.e., @ref Mobj_SetOrigin() has been called).
 *
 * @param mobj  Mobj instance.
 *
 * @see Mobj_IsLinked(), Mobj_SetOrigin()
 */
BspLeaf &Mobj_BspLeafAtOrigin(mobj_t const &mobj);

/**
 * Returns @c true iff the BSP leaf at the mobj's origin is known (i.e.,
 * it has been linked into the map by calling @ref Mobj_SetOrigin() and has a
 * convex geometry).
 *
 * @param mobj  Mobj instance.
 */
bool Mobj_HasSubspace(mobj_t const &mobj);

/**
 * Returns the sector cluster in which the mobj currently resides.
 *
 * @param mobj  Mobj instance.
 *
 * @see Mobj_HasSubspace()
 */
SectorCluster &Mobj_Cluster(mobj_t const &mobj);

/**
 * Returns a pointer to sector cluster in which the mobj currently resides, or
 * @c 0 if not linked or the BSP leaf at the origin has no convex geometry.
 *
 * @param mobj  Mobj instance.
 *
 * @see Mobj_HasCluster()
 */
SectorCluster *Mobj_ClusterPtr(mobj_t const &mobj);

/**
 * Creates a new mobj-triggered particle generator based on the given
 * definition. The generator is added to the list of active ptcgens.
 */
void Mobj_SpawnParticleGen(mobj_t *source, ded_ptcgen_t const *def);

#ifdef __CLIENT__

/**
 * Determines whether the Z origin of the mobj lies above the visual ceiling,
 * or below the visual floor plane of the BSP leaf at the origin. This can be
 * used to determine whether this origin should be adjusted with respect to
 * smoothed plane movement.
 */
dd_bool Mobj_OriginBehindVisPlane(mobj_t *mobj);

/**
 * To be called when lumobjs are disabled to perform necessary bookkeeping.
 */
void Mobj_UnlinkLumobjs(mobj_t *mobj);

/**
 * Generates lumobjs for the mobj.
 * @note: This is called each frame for each luminous object!
 */
void Mobj_GenerateLumobjs(mobj_t *mobj);

/**
 * Calculate the strength of the shadow this mobj should cast.
 *
 * @note Implemented using a greatly simplified version of the lighting equation;
 *       no light diminishing or light range compression.
 */
float Mobj_ShadowStrength(mobj_t *mobj);

/**
 * Determines which of the available sprites is in effect for the current mobj
 * state and frame. May return @c 0 if the state and/or frame is not valid.
 */
Sprite *Mobj_Sprite(mobj_t const &mobj);

/**
 * Determines which of the available model definitions (if any), are in effect
 * for the current mobj state and frame. (Interlinks are resolved).
 *
 * @param nextModef  If non-zero the model definition for the @em next frame is
 *                   written here.
 * @param interp     If non-zero and both model definitions are found the current
 *                   interpolation point between the two is written here.
 *
 * @return  Active model definition for the current frame (if any).
 */
ModelDef *Mobj_ModelDef(mobj_t const &mobj, ModelDef **nextModef = 0,
                        float *interp = 0);

#endif // __CLIENT__

coord_t Mobj_ApproxPointDistance(mobj_t *start, coord_t const *point);

dd_bool Mobj_IsSectorLinked(mobj_t *mobj);

/**
 * @return  The current floatbob offset for the mobj, if the mobj is flagged
 *          for bobbing; otherwise @c 0.
 */
coord_t Mobj_BobOffset(mobj_t *mobj);

float Mobj_Alpha(mobj_t *mobj);

/**
 * Returns the physical radius of the mobj.
 *
 * @param mobj  Mobj instance.
 *
 * @see Mobj_VisualRadius()
 */
coord_t Mobj_Radius(mobj_t const &mobj);

/**
 * Returns the radius of the mobj as it would visually appear to be, according
 * to the current visualization (either a sprite or a 3D model).
 *
 * @param mobj  Mobj instance.
 *
 * @see Mobj_Radius()
 */
coord_t Mobj_VisualRadius(mobj_t const &mobj);

/**
 * Returns an axis-aligned bounding box for the mobj in map space, centered
 * on the origin with dimensions equal to @code radius * 2 @endcode.
 *
 * @param mobj  Mobj instance.
 *
 * @see Mobj_Radius()
 */
AABoxd Mobj_AABox(mobj_t const &mobj);

#endif // DENG_WORLD_P_OBJECT_H
