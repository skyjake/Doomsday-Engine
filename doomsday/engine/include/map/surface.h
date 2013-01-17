/** @file surface.h Logical map surface.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_SURFACE
#define LIBDENG_MAP_SURFACE

#ifndef __cplusplus
#  error "map/surface.h requires C++"
#endif

#include <QSet>
#include "resource/r_data.h"
#include "resource/material.h"
#include "map/p_dmu.h"
#include "map/bspleaf.h"

#ifdef __cplusplus
#include "resource/materialsnapshot.h"
#endif

// Internal surface flags:
#define SUIF_FIX_MISSING_MATERIAL   0x0001 ///< Current material is a fix replacement
                                           /// (not sent to clients, returned via DMU etc).
#define SUIF_NO_RADIO               0x0002 ///< No fakeradio for this surface.

#define SUIF_UPDATE_FLAG_MASK 0xff00
#define SUIF_UPDATE_DECORATIONS 0x8000


class Surface : public de::MapElement
{
public:
#ifdef __CLIENT__
    struct DecorSource
    {
        coord_t origin[3]; ///< World coordinates of the decoration.
        BspLeaf *bspLeaf;
        /// @todo $revise-texture-animation reference by index.
        de::MaterialSnapshot::Decoration const *decor;
    };
#endif // __CLIENT__

public:
    ddmobj_base_t  base;
    de::MapElement *owner;        ///< Either @c DMU_SIDEDEF, or @c DMU_PLANE
    int            flags;         ///< SUF_ flags
    int            oldFlags;
    material_t*    material;
    blendmode_t    blendMode;
    float          tangent[3];
    float          bitangent[3];
    float          normal[3];
    float          offset[2];     ///< [X, Y] Planar offset to surface material origin.
    float          oldOffset[2][2];
    float          visOffset[2];
    float          visOffsetDelta[2];
    float          rgba[4];       ///< Surface color tint
    short          inFlags;       ///< SUIF_* flags
    unsigned int   numDecorations;
    struct surfacedecorsource_s *decorations;

public:
    Surface();
    ~Surface();
};
struct surfacedecorsource_s;

typedef QSet<Surface *> SurfaceSet;

/**
 * Mark the surface as requiring a full update. To be called
 * during engine reset.
 */
void Surface_Update(Surface* surface);

/**
 * Update the Surface's map space base origin according to relevant points in
 * the owning object.
 *
 * If this surface is owned by a SideDef then the origin is updated using the
 * points defined by the associated LineDef's vertices and the plane heights of
 * the Sector on that SideDef's side.
 *
 * If this surface is owned by a Plane then the origin is updated using the
 * points defined the center of the Plane's Sector (on the XY plane) and the Z
 * height of the plane.
 *
 * If no owner is presently associated this is a no-op.
 *
 * @param surface  Surface instance.
 */
void Surface_UpdateBaseOrigin(Surface* surface);

/// @return @c true= is drawable (i.e., a drawable Material is bound).
boolean Surface_IsDrawable(Surface* surface);

/// @return @c true= is sky-masked (i.e., a sky-masked Material is bound).
boolean Surface_IsSkyMasked(const Surface* suf);

/// @return @c true= is owned by some element of the Map geometry.
boolean Surface_AttachedToMap(Surface* surface);

/**
 * Change Material bound to this surface.
 *
 * @param surface  Surface instance.
 * @param mat  New Material.
 */
boolean Surface_SetMaterial(Surface* surface, material_t* material);

/**
 * Change Material origin.
 *
 * @param surface  Surface instance.
 * @param x  New X origin in map space.
 * @param y  New Y origin in map space.
 */
boolean Surface_SetMaterialOrigin(Surface* surface, float x, float y);

/**
 * Change Material origin X coordinate.
 *
 * @param surface  Surface instance.
 * @param x  New X origin in map space.
 */
boolean Surface_SetMaterialOriginX(Surface* surface, float x);

/**
 * Change Material origin Y coordinate.
 *
 * @param surface  Surface instance.
 * @param y  New Y origin in map space.
 */
boolean Surface_SetMaterialOriginY(Surface* surface, float y);

/**
 * Change surface color tint and alpha.
 *
 * @param surface  Surface instance.
 * @param red      Red color component (0...1).
 * @param green    Green color component (0...1).
 * @param blue     Blue color component (0...1).
 * @param alpha    Alpha component (0...1).
 */
boolean Surface_SetColorAndAlpha(Surface* surface, float red, float green, float blue, float alpha);

/**
 * Change surfacecolor tint.
 *
 * @param surface  Surface instance.
 * @param red      Red color component (0...1).
 */
boolean Surface_SetColorRed(Surface* surface, float red);

/**
 * Change surfacecolor tint.
 *
 * @param surface  Surface instance.
 * @param green    Green color component (0...1).
 */
boolean Surface_SetColorGreen(Surface* surface, float green);

/**
 * Change surfacecolor tint.
 *
 * @param surface  Surface instance.
 * @param blue     Blue color component (0...1).
 */
boolean Surface_SetColorBlue(Surface* surface, float blue);

/**
 * Change surface alpha.
 *
 * @param surface  Surface instance.
 * @param alpha    New alpha value (0...1).
 */
boolean Surface_SetAlpha(Surface* surface, float alpha);

/**
 * Change blendmode.
 *
 * @param surface  Surface instance.
 * @param blendMode  New blendmode.
 */
boolean Surface_SetBlendMode(Surface* surface, blendmode_t blendMode);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param surface  Surface instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Surface_GetProperty(const Surface* surface, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param surface  Surface instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Surface_SetProperty(Surface* surface, const setargs_t* args);

#ifdef __CLIENT__
/**
 * Create a new projected (light) decoration source for the surface.
 *
 * @param surface  Surface instance.
 * @return  Newly created decoration source.
 */
Surface::DecorSource *Surface_NewDecoration(Surface *surface);

/**
 * Clear all the projected (light) decoration sources for the surface.
 * @param surface  Surface instance.
 */
void Surface_ClearDecorations(Surface *surface);
#endif __CLIENT__

#endif /// LIBDENG_MAP_SURFACE
