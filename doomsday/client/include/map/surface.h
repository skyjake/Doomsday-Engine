/** @file surface.h Map surface.
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
 * @ingroup map
 */
class Surface : public de::MapElement
{
public:
    /// Required material is missing. @ingroup errors
    DENG2_ERROR(MissingMaterialError);

    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

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

    /// Bound material.
    Material *_material;

    /// @c true= Bound material is a "missing material fix".
    bool _materialIsMissingFix;

    /// Blending mode.
    blendmode_t _blendMode;

    /// Tangent space vectors:
    vec3f_t _tangent;
    vec3f_t _bitangent;
    vec3f_t _normal;

    /// [X, Y] Planar offset to surface material origin.
    vec2f_t _offset;

    /// Old [X, Y] Planar material origin offset. For smoothing.
    vec2f_t _oldOffset[2];

    /// Smoothed [X, Y] Planar material origin offset.
    vec2f_t _visOffset;

    /// Smoother [X, Y] Planar material origin offset delta.
    vec2f_t _visOffsetDelta;

    /// Surface color tint and alpha.
    vec4f_t _colorAndAlpha;

#ifdef __CLIENT__
    /// @todo Does not belong here - move to the map renderer.
    struct DecorationData
    {
        /// @c true= An update is needed.
        bool needsUpdate;

        /// Plotted decoration sources [numSources size].
        struct surfacedecorsource_s *sources;
        uint numSources;
    } _decorationData;
#endif

public:
    Surface(de::MapElement &owner);
    ~Surface();

    /// @todo Refactor away.
    Surface &operator = (Surface const &other);

    /**
     * Returns the owning map element. Either @c DMU_SIDEDEF, or @c DMU_PLANE.
     */
    de::MapElement &owner() const;

    /**
     * Returns the normalized tangent vector for the surface.
     */
    const_pvec3f_t &tangent() const;

    /**
     * Returns the normalized bitangent vector for the surface.
     */
    const_pvec3f_t &bitangent() const;

    /**
     * Returns the normalized normal vector for the surface.
     */
    const_pvec3f_t &normal() const;

    /**
     * Returns the @ref surfaceFlags of the surface.
     */
    int flags() const;

    /**
     * Returns @c true iff a material is bound to the surface.
     */
    bool hasMaterial() const;

    /**
     * Returns @c true iff a @em fix material is bound to the surface, which
     * was chosen automatically where one was missing. Clients should not be
     * notified when a fix material is bound to the surface (as they should
     * perform their fixing, locally). However, if the fix material is later
     * replaced with a "normally-bound" material, clients should be notified
     * as per usual.
     */
    bool hasFixMaterial() const;

    /**
     * Convenient helper method for determining whether a sky-masked material
     * is bound to the surface.
     *
     * @return  @c true iff a sky-masked material is bound; otherwise @c 0.
     */
    inline bool hasSkyMaskedMaterial() const {
        return hasMaterial() && material().isSkyMasked();
    }

    /**
     * Returns the material bound to the surface.
     *
     * @see hasMaterial(), hasFixMaterial()
     */
    Material &material() const;

    /**
     * Returns a pointer to the material bound to the surface; otherwise @c 0.
     *
     * @see hasMaterial(), hasFixMaterial()
     */
    inline Material *materialPtr() const { return hasMaterial()? &material() : 0; }

    /**
     * Change Material bound to the surface.
     *
     * @param newMaterial   New material to be bound.
     * @param isMissingFix  The new material is a fix for a "missing" material.
     */
    bool setMaterial(Material *material, bool isMissingFix = false);

    /**
     * Returns the material origin offset for the surface.
     */
    const_pvec2f_t &materialOrigin() const;

    /**
     * Change the material origin offset for the surface.
     *
     * @param newOrigin  New origin offset in map coordinate space units.
     */
    bool setMaterialOrigin(const_pvec2f_t newOrigin);

    /**
     * @copydoc setMaterialOrigin()
     *
     * @param x  New X origin offset in map coordinate space units.
     * @param y  New Y origin offset in map coordinate space units.
     */
    inline bool setMaterialOrigin(float x, float y)
    {
        float newOrigin[2] = { x, y};
        return setMaterialOrigin(newOrigin);
    }

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
     * Returns the current interpolated visual material origin of the surface
     * in the map coordinate space.
     *
     * @see setMaterialOrigin()
     */
    const_pvec2f_t &visMaterialOrigin() const;

    /**
     * Returns the delta between current material origin and the interpolated
     * visual origin of the material in the map coordinate space.
     *
     * @see setMaterialOrigin(), visMaterialOrigin()
     */
    const_pvec2f_t &visMaterialOriginDelta() const;

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
     * Returns the surface color and alpha for the surface.
     */
    const_pvec4f_t &colorAndAlpha() const;

    /**
     * Change surface color tint and alpha.
     *
     * @param newColorAndAlpha  [red, green, blue, alpha] All components [0..1].
     */
    bool setColorAndAlpha(const_pvec4f_t newColorAndAlpha);

    /**
     * @copydoc setColorAndAlpha()
     *
     * @param red      Red color component [0..1].
     * @param green    Green color component [0..1].
     * @param blue     Blue color component [0..1].
     * @param alpha    Alpha component [0..1].
     */
    inline bool setColorAndAlpha(float red, float green, float blue, float alpha)
    {
        float newColorAndAlpha[4] = { red, green, blue, alpha };
        return setColorAndAlpha(newColorAndAlpha);
    }

    /**
     * Change surface color tint.
     *
     * @param red      Red color component [0..1].
     */
    bool setColorRed(float red);

    /**
     * Change surface color tint.
     *
     * @param green    Green color component [0..1].
     */
    bool setColorGreen(float green);

    /**
     * Change surface color tint.
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
     * Returns the blendmode for the surface.
     */
    blendmode_t blendMode() const;

    /**
     * Change blendmode.
     *
     * @param newBlendMode  New blendmode.
     */
    bool setBlendMode(blendmode_t newBlendMode);

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

    /**
     * Mark the surface as needing a decoration source update.
     *
     * @todo This data should not be owned by Surface.
     */
    void markAsNeedingDecorationUpdate();
#endif // __CLIENT__

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

    /// @return @c true= is owned by some element of the Map geometry.
    /// @deprecated Unnecessary; refactor away.
    bool isAttachedToMap() const;

    /**
     * Helper function for determining whether the surface is owned by a
     * Line which is itself owned by a Polyobj.
     */
    static bool isFromPolyobj(Surface const &surface);
};

struct surfacedecorsource_s;

/// Set of surfaces.
typedef QSet<Surface *> SurfaceSet;

#endif // LIBDENG_MAP_SURFACE
