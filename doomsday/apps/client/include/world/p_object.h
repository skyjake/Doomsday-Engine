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
#include <de/Record>
#include <de/Vector>
#include <doomsday/defs/ded.h>
#include <doomsday/world/thinker.h>
#include <doomsday/world/mobj.h>

#include "world/bspleaf.h"
#include "api_map.h"
#include "dd_def.h"
#ifdef __CLIENT__
#  include "resource/framemodeldef.h"
#endif

namespace world {
class Subsector;
}
class Plane;

#define MOBJ_SIZE               gx.GetInteger(DD_MOBJ_SIZE)

class MobjThinker : public ThinkerT<mobj_t>
{
public:
    MobjThinker(AllocMethod alloc = AllocateStandard) : ThinkerT(MOBJ_SIZE, alloc) {}
    MobjThinker(mobj_t const &existingToCopy) : ThinkerT(existingToCopy, MOBJ_SIZE) {}
    MobjThinker(mobj_t *existingToTake) : ThinkerT(existingToTake, MOBJ_SIZE) {}

    static void zap(mobj_t &mob) { ThinkerT::zap(mob, MOBJ_SIZE); }
};

#define DEFAULT_FRICTION        FIX2FLT(0xe800)
#define NOMOMENTUM_THRESHOLD    (0.0001)

#define IS_BLOCK_LINKED(mo)     ((mo)->bNext != 0)

DE_EXTERN_C de::dint useSRVO, useSRVOAngle;

void P_InitUnusedMobjList();

/**
 * To be called to register the commands and variables of this module.
 */
void Mobj_ConsoleRegister();

mobj_t *P_MobjCreate(thinkfunc_t function, de::Vec3d const &origin, angle_t angle,
    coord_t radius, coord_t height, de::dint ddflags);

void P_MobjRecycle(mobj_t *mob);

/**
 * Returns the map in which the map-object exists. Note that a map-object may exist in a
 * map while not being @em linked into data structures such as the blockmap and sectors.
 * To determine whether the map-object is linked, call @ref Mobj_IsLinked().
 *
 * @see Thinker_Map()
 */
world::Map &Mobj_Map(mobj_t const &mob);

/**
 * Returns @c true if the map-object has been linked into the map. The only time this is
 * not true is if @ref Mobj_SetOrigin() has not yet been called.
 *
 * @param mob  Map-object.
 *
 * @todo Automatically link all new mobjs into the map (making this redundant).
 */
bool Mobj_IsLinked(mobj_t const &mob);

/**
 * Returns a copy of the map-object's origin in map space.
 */
de::Vec3d Mobj_Origin(mobj_t const &mob);

/**
 * Returns the map-object's visual center (i.e., origin plus z-height offset).
 */
de::Vec3d Mobj_Center(mobj_t &mob);

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
 * Returns the map BSP leaf at the origin of the map-object. Note that the mobj must be
 * linked in the map (i.e., @ref Mobj_SetOrigin() has been called).
 *
 * @param mob  Map-object.
 *
 * @see Mobj_IsLinked(), Mobj_SetOrigin()
 */
world::BspLeaf &Mobj_BspLeafAtOrigin(mobj_t const &mob);

/**
 * Returns @c true if the BSP leaf at the map-object's origin is known (i.e., it has been
 * linked into the map by calling @ref Mobj_SetOrigin() and has a convex geometry).
 *
 * @param mob  Map-object.
 */
bool Mobj_HasSubsector(mobj_t const &mob);

/**
 * Returns the subsector in which the map-object currently resides.
 *
 * @param mob  Map-object.
 *
 * @see Mobj_HasSubsector()
 */
world::Subsector &Mobj_Subsector(mobj_t const &mob);

/**
 * Returns a pointer to subsector in which the mobj currently resides, or @c nullptr
 * if not linked or the BSP leaf at the origin has no convex geometry.
 *
 * @param mob  Map-object.
 *
 * @see Mobj_HasSubsector()
 */
world::Subsector *Mobj_SubsectorPtr(mobj_t const &mob);

/**
 * Creates a new map-object triggered particle generator based on the given definition.
 * The generator is added to the list of active ptcgens.
 */
void Mobj_SpawnParticleGen(mobj_t *source, ded_ptcgen_t const *def);

#ifdef __CLIENT__

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
de::dfloat Mobj_ShadowStrength(mobj_t const &mob);

/**
 * Determines which of the available sprites is in effect for the current map-object state
 * and frame. May return @c nullptr if the state and/or frame is not valid.
 */
de::Record const *Mobj_SpritePtr(mobj_t const &mob);

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
FrameModelDef *Mobj_ModelDef(mobj_t const &mob, FrameModelDef **nextModef = nullptr,
                        de::dfloat *interp = nullptr);

/**
 * Calculates the shadow radius of the map-object. Falls back to Mobj_VisualRadius().
 *
 * @param mob  Map-object.
 *
 * @return Radius for shadow.
 */
coord_t Mobj_ShadowRadius(mobj_t const &mob);

#endif // __CLIENT__

coord_t Mobj_ApproxPointDistance(mobj_t const *mob, coord_t const *point);

/**
 * Returns @c true if the map-object is physically inside (and @em presently linked to)
 * some Sector of the owning Map.
 */
bool Mobj_IsSectorLinked(mobj_t const &mob);

/**
 * Returns the current "float bob" offset (if enabled); otherwise @c 0.
 */
coord_t Mobj_BobOffset(mobj_t const &mob);

de::dfloat Mobj_Alpha(mobj_t const &mob);

/**
 * Returns the physical radius of the mobj.
 *
 * @param mob  Map-object.
 *
 * @see Mobj_VisualRadius()
 */
coord_t Mobj_Radius(mobj_t const &mob);

/**
 * Returns the radius of the mobj as it would visually appear to be, according
 * to the current visualization (either a sprite or a 3D model).
 *
 * @param mob  Map-object.
 *
 * @see Mobj_Radius()
 */
coord_t Mobj_VisualRadius(mobj_t const &mob);

/**
 * Returns an axis-aligned bounding box for the mobj in map space, centered
 * on the origin with dimensions equal to @code radius * 2 @endcode.
 *
 * @param mob  Map-object.
 *
 * @see Mobj_Radius()
 */
AABoxd Mobj_Bounds(mobj_t const &mob);

#endif  // WORLD_P_OBJECT_H
