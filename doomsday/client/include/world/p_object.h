/** @file p_object.h World map objects.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

//#include <de/libdeng1.h>

#include <de/Vector>

#include "Sector"

class BspLeaf;
class Plane;

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

mobj_t *P_MobjCreate(thinkfunc_t function, coord_t const post[3], angle_t angle,
    coord_t radius, coord_t height, int ddflags);

void P_MobjRecycle(mobj_t *mobj);

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
de::Vector3d Mobj_Origin(mobj_t &mobj);

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
boolean Mobj_SetOrigin(mobj_t *mobj, coord_t x, coord_t y, coord_t z);

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
 * Returns @c true iff the sector cluster at the mobj's origin is known (i.e.,
 * it has been linked into the map by calling @ref Mobj_SetOrigin() and the BSP
 * leaf at the origin has a convex geometry (not degenerate)).
 *
 * @param mobj  Mobj instance.
 */
bool Mobj_HasCluster(mobj_t const &mobj);

/**
 * Returns the sector cluster in which the mobj currently resides.
 *
 * @param mobj  Mobj instance.
 *
 * @see Mobj_HasCluster()
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

#ifdef __CLIENT__

/**
 * Determines whether the Z origin of the mobj lies above the visual ceiling,
 * or below the visual floor plane of the BSP leaf at the origin. This can be
 * used to determine whether this origin should be adjusted with respect to
 * smoothed plane movement.
 */
boolean Mobj_OriginBehindVisPlane(mobj_t *mobj);

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

#endif // __CLIENT__

coord_t Mobj_ApproxPointDistance(mobj_t *start, coord_t const *point);

boolean Mobj_IsSectorLinked(mobj_t *mobj);

/**
 * @return  The current floatbob offset for the mobj, if the mobj is flagged
 *          for bobbing; otherwise @c 0.
 */
coord_t Mobj_BobOffset(mobj_t *mobj);

float Mobj_Alpha(mobj_t *mobj);

/// @return  Radius of the mobj as it would visually appear to be.
coord_t Mobj_VisualRadius(mobj_t *mobj);

#endif // DENG_WORLD_P_OBJECT_H
