/** @file surface.h  World map surface.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_SURFACE_H
#define DENG_WORLD_SURFACE_H

#include "Material"

#include "MapElement"
#include "uri.hh"

#include <de/Error>
#include <de/Matrix>
#include <de/Observers>
#include <de/Vector>
#ifdef __CLIENT__
#  include <QList>
#endif

class BspLeaf;
class Decoration;

/**
 * Models a "boundless" but otherwise geometric map surface. Boundless in the
 * sense that a surface has no edges.
 *
 * @ingroup world
 */
class Surface : public de::MapElement
{
    DENG2_NO_COPY  (Surface)
    DENG2_NO_ASSIGN(Surface)

public:
    /// Required material is missing. @ingroup errors
    DENG2_ERROR(MissingMaterialError);

    /// Notified when the @em sharp material origin changes.
    DENG2_DEFINE_AUDIENCE(MaterialOriginChange, void surfaceMaterialOriginChanged(Surface &surface))

    /// Notified whenever the normal vector changes.
    DENG2_DEFINE_AUDIENCE(NormalChange, void surfaceNormalChanged(Surface &surface))

    /// Notified whenever the opacity changes.
    DENG2_DEFINE_AUDIENCE(OpacityChange, void surfaceOpacityChanged(Surface &surface))

    /// Notified whenever the tint color changes.
    DENG2_DEFINE_AUDIENCE(TintColorChange, void surfaceTintColorChanged(Surface &sector))

    /// Maximum speed for a smoothed material offset.
    static int const MAX_SMOOTH_MATERIAL_MOVE = 8;

#ifdef __CLIENT__
    typedef QList<Decoration *> Decorations;

public: /// @todo Does not belong at this level
    bool _needDecorationUpdate; ///< @c true= An update is needed.

#endif // __CLIENT__

public:
    /**
     * Construct a new surface.
     *
     * @param owner      Map element which will own the surface.
     * @param opacity    Default opacity strength (@c 1= fully opaque).
     * @param tintColor  Default tint color.
     */
    Surface(de::MapElement &owner,
            float opacity                 = 1,
            de::Vector3f const &tintColor = de::Vector3f(1, 1, 1));

    /**
     * Returns the normalized tangent space matrix for the surface.
     * (col0: tangent, col1: bitangent, col2: normal)
     */
    de::Matrix3f const &tangentMatrix() const;

    /**
     * Returns a copy of the normalized tangent vector for the surface.
     */
    inline de::Vector3f tangent() const { return tangentMatrix().column(0); }

    /**
     * Returns a copy of the normalized bitangent vector for the surface.
     */
    inline de::Vector3f bitangent() const { return tangentMatrix().column(1); }

    /**
     * Returns a copy of the normalized normal vector for the surface.
     */
    inline de::Vector3f normal() const { return tangentMatrix().column(2); }

    /**
     * Change the tangent space normal vector for the surface. If changed,
     * the tangent vectors will be recalculated next time they are needed.
     * The NormalChange audience is notified whenever the normal changes.
     *
     * @param newNormal  New normal vector (will be normalized if needed).
     */
    Surface &setNormal(de::Vector3f const &newNormal);

    /**
     * Returns a copy of the current @ref surfaceFlags of the surface.
     */
    int flags() const;

    /**
     * Change the @ref surfaceFlags of the surface.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    Surface &setFlags(int flagsToChange, de::FlagOp operation = de::SetFlags);

    /**
     * Returns @c true iff the surface is flagged @a flagsToTest.
     */
    inline bool isFlagged(int flagsToTest) const {
        return (flags() & flagsToTest) != 0;
    }

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
     * @return  @c true iff a sky-masked material is bound.
     */
    inline bool hasSkyMaskedMaterial() const {
        return hasMaterial() && material().isSkyMasked();
    }

    /**
     * Convenient helper method for determining whether a drawable, non @em fix
     * material is bound to the surface.
     *
     * @return  @c true iff drawable, non @em fix masked material is bound.
     *
     * @see hasMaterial(), hasFixMaterial(), Material::isDrawable()
     */
    inline bool hasDrawableNonFixMaterial() const {
        return hasMaterial() && !hasFixMaterial() && material().isDrawable();
    }

    /**
     * Returns the attributed material of the surface.
     *
     * @see hasMaterial(), hasFixMaterial()
     */
    Material &material() const;

    /**
     * Returns a pointer to the attributed material of the surface; otherwise @c 0.
     *
     * @see hasMaterial(), hasFixMaterial()
     */
    inline Material *materialPtr() const { return hasMaterial()? &material() : 0; }

    /**
     * Change the attributed material of the surface. On client side, any existing
     * decorations are cleared whenever the material changes and the surface is
     * marked for redecoration.
     *
     * @param newMaterial   New material to apply. Use @c 0 to clear.
     * @param isMissingFix  @c true= this is a fix for a "missing" material.
     */
    Surface &setMaterial(Material *newMaterial, bool isMissingFix = false);

    /**
     * Returns the material origin offset of the surface.
     */
    de::Vector2f const &materialOrigin() const;

    /**
     * Change the material origin offset of the surface.
     *
     * @param newOrigin  New origin offset in map coordinate space units.
     */
    Surface &setMaterialOrigin(de::Vector2f const &newOrigin);

    /**
     * Compose a URI for the surface's material. If no material is bound then a
     * default (i.e., empty) URI is returned.
     *
     * @see hasMaterial(), MaterialManifest::composeUri()
     */
    de::Uri composeMaterialUri() const;

    /**
     * Returns the opacity of the surface. The OpacityChange audience is notified
     * whenever the opacity changes.
     *
     * @see setOpacity()
     */
    float opacity() const;

    /**
     * Change the opacity of the surface. The OpacityChange audience is notified
     * whenever the opacity changes.
     *
     * @param newOpacity  New opacity strength.
     *
     * @see opacity()
     */
    Surface &setOpacity(float newOpacity);

    /**
     * Returns the tint color of the surface. The TintColorChange audience is notified
     * whenever the tint color changes.
     *
     * @see setTintColor()
     */
    de::Vector3f const &tintColor() const;

    /**
     * Change the tint color for the surface. The TintColorChange audience is notified
     * whenever the tint color changes.
     *
     * @param newTintColor  New tint color.
     *
     * @see tintColor()
     */
    Surface &setTintColor(de::Vector3f const &newTintColor);

    /**
     * Returns the blendmode for the surface.
     */
    blendmode_t blendMode() const;

    /**
     * Change blendmode.
     *
     * @param newBlendMode  New blendmode.
     */
    Surface &setBlendMode(blendmode_t newBlendMode);

#ifdef __CLIENT__

    /**
     * Returns the current smoothed (interpolated) material origin for the
     * surface in the map coordinate space.
     *
     * @see setMaterialOrigin()
     */
    de::Vector2f const &materialOriginSmoothed() const;

    /**
     * Returns the delta between current and the smoothed material origin for
     * the surface in the map coordinate space.
     *
     * @see setMaterialOrigin(), smoothMaterialOrigin()
     */
    de::Vector2f const &materialOriginSmoothedAsDelta() const;

    /**
     * Perform smoothed material origin interpolation.
     *
     * @see materialOriginSmoothed()
     */
    void lerpSmoothedMaterialOrigin();

    /**
     * Reset the surface's material origin tracking.
     *
     * @see materialOriginSmoothed()
     */
    void resetSmoothedMaterialOrigin();

    /**
     * Roll the surface's material origin tracking buffer.
     */
    void updateMaterialOriginTracking();

    /**
     * Determine the glow properties of the surface, which, are derived from the
     * bound material (averaged color).
     *
     * Return values:
     * @param color  Amplified glow color is written here.
     *
     * @return  Glow strength/intensity or @c 0 if not presently glowing.
     */
    float glow(de::Vector3f &color) const;

    /**
     * Add the specified decoration to the surface.
     *
     * @param decoration  Decoration to add. Ownership is given to the surface.
     */
    void addDecoration(Decoration *decoration);

    /**
     * Clear all surface decorations.
     */
    void clearDecorations();

    /**
     * Provides access to the surface decorations for efficient traversal.
     */
    Decorations const &decorations() const;

    /**
     * Returns the total number of surface decorations.
     */
    int decorationCount() const;

    /**
     * Mark the surface as needing a decoration update.
     */
    void markAsNeedingDecorationUpdate();

#endif // __CLIENT__

protected:
    int property(DmuArgs &args) const;
    int setProperty(DmuArgs const &args);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_SURFACE_H
