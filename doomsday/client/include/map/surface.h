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

#include "resource/r_data.h"
#include "map/p_dmu.h"
#include "map/bspleaf.h"
#include "Material"
#ifdef __CLIENT__
#  include "MaterialSnapshot"
#endif
#include <de/vector1.h>
#include <QSet>

/**
 * @defgroup surfaceInternalFlags Surface Internal Flags
 * @ingroup map
 */
///@{
#define SUIF_FIX_MISSING_MATERIAL   0x0001 ///< Current material is a fix replacement
                                           /// (not sent to clients, returned via DMU etc).
#define SUIF_NO_RADIO               0x0002 ///< No fakeradio for this surface.

#define SUIF_UPDATE_FLAG_MASK       0xff00
#define SUIF_UPDATE_DECORATIONS     0x8000
///@}

/**
 * @ingroup map
 */
class Surface : public de::MapElement
{
public:
    /// Required material is missing. @ingroup errors
    DENG2_ERROR(MissingMaterialError);

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
    /// Owning map element, either @c DMU_SIDEDEF, or @c DMU_PLANE.
    de::MapElement &_owner;

    /// Sound emitter.
    ddmobj_base_t _soundEmitter;

    /// @ref sufFlags
    int _flags;

    // Bound material.
    Material *_material;

    // Blending mode, for rendering.
    blendmode_t blendMode;

    // Tangent space vectors:
    vec3f_t tangent;
    vec3f_t bitangent;
    vec3f_t normal;

    /// [X, Y] Planar offset to surface material origin.
    vec2f_t offset;

    /// Old [X, Y] Planar material origin offset. For smoothing.
    vec2f_t _oldOffset[2];

    /// Smoothed [X, Y] Planar material origin offset.
    vec2f_t visOffset;

    /// Smoother [X, Y] Planar material origin offset delta.
    vec2f_t visOffsetDelta;

    /// Surface color tint.
    float rgba[4];

    /// @ref surfaceInternalFlags
    short inFlags;

    /// Old @ref surfaceInternalFlags, for tracking changes.
    short _oldInFlags;

    uint numDecorations;

    struct surfacedecorsource_s *decorations;

public:
    Surface(de::MapElement &owner);
    ~Surface();

    /// @todo Refactor away.
    Surface &operator = (Surface const &other);

    /// @return @c true= is drawable (i.e., a drawable Material is bound).
    inline bool isDrawable() const
    {
        return hasMaterial() && material().isDrawable();
    }

    /// @return @c true= is sky-masked (i.e., a sky-masked Material is bound).
    inline bool isSkyMasked() const
    {
        return hasMaterial() && material().isSkyMasked();
    }

    /// @return @c true= is owned by some element of the Map geometry.
    bool isAttachedToMap() const;

    /**
     * Returns the owning map element. Either @c DMU_SIDEDEF, or @c DMU_PLANE.
     */
    de::MapElement &owner() const;

    /**
     * Returns @c true iff a material is bound to the surface.
     */
    bool hasMaterial() const;

    /**
     * Returns the material bound to the surface.
     *
     * @see hasMaterial()
     */
    Material &material() const;

    /**
     * Returns a pointer to the material bound to the surface; otherwise @c 0.
     *
     * @see hasMaterial()
     */
    inline Material *materialPtr() const { return hasMaterial()? &material() : 0; }

    /**
     * Returns the @ref surfaceFlags of the surface.
     */
    int flags() const;

    /**
     * Returns the sound emitter for the surface.
     */
    ddmobj_base_t &soundEmitter();

    /// @copydoc soundEmitter()
    ddmobj_base_t const &soundEmitter() const;

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
     */
    void updateSoundEmitterOrigin();

    /**
     * Mark the surface as requiring a full update. To be called during an
     * engine reset.
     */
    void update();

    /**
     * Change Material bound to this surface.
     *
     * @param mat  New Material.
     */
    bool setMaterial(Material *material);

    /**
     * Change Material origin.
     *
     * @param x  New X origin in map space.
     * @param y  New Y origin in map space.
     */
    bool setMaterialOrigin(float x, float y);

    /**
     * Change Material origin X coordinate.
     *
     * @param x  New X origin in map space.
     */
    bool setMaterialOriginX(float x);

    /**
     * Change Material origin Y coordinate.
     *
     * @param y  New Y origin in map space.
     */
    bool setMaterialOriginY(float y);

    /**
     * Change surface color tint and alpha.
     *
     * @param red      Red color component [0..1].
     * @param green    Green color component [0..1].
     * @param blue     Blue color component [0..1].
     * @param alpha    Alpha component [0..1].
     */
    bool setColorAndAlpha(float red, float green, float blue, float alpha);

    /**
     * Change surfacecolor tint.
     *
     * @param red      Red color component [0..1].
     */
    bool setColorRed(float red);

    /**
     * Change surfacecolor tint.
     *
     * @param green    Green color component [0..1].
     */
    bool setColorGreen(float green);

    /**
     * Change surfacecolor tint.
     *
     * @param blue     Blue color component [0..1].
     */
    bool setColorBlue(float blue);

    /**
     * Change surface alpha.
     *
     * @param alpha    New alpha value [0..1].
     */
    bool setAlpha(float alpha);

    /**
     * Change blendmode.
     *
     * @param newBlendMode  New blendmode.
     */
    bool setBlendMode(blendmode_t newBlendMode);

    /**
     * Get a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int property(setargs_t &args) const;

    /**
     * Update a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int setProperty(setargs_t const &args);

#ifdef __CLIENT__
    /**
     * Create a new projected (light) decoration source for the surface.
     *
     * @return  Newly created decoration source.
     */
    DecorSource *newDecoration();

    /**
     * Clear all the projected (light) decoration sources for the surface.
     */
    void clearDecorations();

    /**
     * Returns the total number of decoration sources for the surface.
     */
    uint decorationCount() const;
#endif // __CLIENT__
};

struct surfacedecorsource_s;

/// Set of surfaces.
typedef QSet<Surface *> SurfaceSet;

#endif // LIBDENG_MAP_SURFACE
