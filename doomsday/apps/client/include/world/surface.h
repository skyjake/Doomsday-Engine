/** @file surface.h  Map surface.
 * @ingroup world
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#include <functional>
#include <de/Id>
#include <de/Matrix>
#include <de/Observers>
#include <de/String>
#include <de/Vector>
#include <doomsday/uri.h>
#include <doomsday/world/Material>

#ifdef __CLIENT__
class Decoration;
class MaterialAnimator;
#endif

/**
 * MapElement for modelling a "boundless" two-dimensional geometric plane (i.e., no edges).
 */
class Surface : public world::MapElement
{
    DENG2_NO_COPY  (Surface)
    DENG2_NO_ASSIGN(Surface)

public:
    /// Interface for surface decoration state.
    class IDecorationState
    {
    public:
        virtual ~IDecorationState();
    };

    /// Notified whenever the tint color changes.
    DENG2_DEFINE_AUDIENCE2(ColorChange,   void surfaceColorChanged(Surface &sector))

    /// Notified whenever the normal vector changes.
    DENG2_DEFINE_AUDIENCE2(NormalChange,  void surfaceNormalChanged(Surface &surface))

    /// Notified whenever the opacity changes.
    DENG2_DEFINE_AUDIENCE2(OpacityChange, void surfaceOpacityChanged(Surface &surface))

    /// Notified whenever the @em sharp origin changes.
    DENG2_DEFINE_AUDIENCE2(OriginChange,  void surfaceOriginChanged(Surface &surface))

    /**
     * Construct a new surface.
     *
     * @param owner    Map element which will own the surface.
     * @param opacity  Opacity strength (@c 1= fully opaque).
     * @param color    Tint color.
     */
    Surface(world::MapElement &owner,
            de::dfloat opacity        = 1,
            de::Vec3f const &color = de::Vec3f(1, 1, 1));

    /**
     * Composes a human-friendly, styled, textual description of the surface.
     */
    de::String description() const;

    /**
     * Returns the normalized tangent space matrix for the surface.
     * (col0: tangent, col1: bitangent, col2: normal)
     */
    de::Mat3f const &tangentMatrix() const;

    inline de::Vec3f tangent()   const { return tangentMatrix().column(0); }
    inline de::Vec3f bitangent() const { return tangentMatrix().column(1); }
    inline de::Vec3f normal()    const { return tangentMatrix().column(2); }

    /**
     * Change the tangent space normal vector for the surface. If changed, the
     * tangent vectors will be recalculated next time they are needed. The
     * NormalChange audience is notified whenever the normal changes.
     *
     * @param newNormal  New normal vector (will be normalized if needed).
     */
    Surface &setNormal(de::Vec3f const &newNormal);

    /**
     * Returns the opacity of the surface. The OpacityChange audience is notified
     * whenever the opacity changes.
     *
     * @see setOpacity()
     */
    de::dfloat opacity() const;
    Surface &setOpacity(de::dfloat newOpacity);

    /**
     * Returns the material origin offset for the surface.
     */
    de::Vec2f const &origin() const;
    Surface &setOrigin(de::Vec2f const &newOrigin);

    /**
     * Returns the tint color of the surface. The ColorChange audience is
     * notified whenever the tint color changes.
     *
     * @see setColor()
     */
    de::Vec3f const &color() const;
    Surface &setColor(de::Vec3f const &newColor);

    /**
     * Returns the blendmode for the surface.
     */
    blendmode_t blendMode() const;
    Surface &setBlendMode(blendmode_t newBlendMode);

//- Material ----------------------------------------------------------------------------

    /// Thrown when a required material is missing. @ingroup errors
    DENG2_ERROR(MissingMaterialError);

    /// Notified when the material changes.
    DENG2_DEFINE_AUDIENCE2(MaterialChange, void surfaceMaterialChanged(Surface &surface))

    /**
     * Returns @c true iff a material is bound to the surface.
     */
    bool hasMaterial() const;

    /**
     * Returns @c true iff a @em fix material is bound to the surface, which was
     * chosen automatically where one was missing. Clients should not be notified
     * when a fix material is bound to the surface (as they should perform their
     * fixing, locally). However, if the fix material is later replaced with a
     * "normally-bound" material, clients should be notified as per usual.
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
    world::Material &material() const;
    world::Material *materialPtr() const;

    /**
     * Change the material attributed to the surface.
     *
     * @param newMaterial   New material to apply. Use @c nullptr to clear.
     * @param isMissingFix  @c true= this is a fix for a "missing" material.
     */
    Surface &setMaterial(world::Material *newMaterial, bool isMissingFix = false);

    /**
     * Returns @c true if the surface material is mirrored on the X axis.
     */
    bool materialMirrorX() const;

    /**
     * Returns @c true if the surface material is mirrored on the Y axis.
     */
    bool materialMirrorY() const;

    /**
     * Returns the material scale factors for the surface.
     */
    de::Vec2f materialScale() const;

    /**
     * Compose a URI for the surface's material. If no material is bound then a
     * default (i.e., empty) URI is returned.
     *
     * @see hasMaterial(), MaterialManifest::composeUri()
     */
    de::Uri composeMaterialUri() const;

    void setDecorationState(IDecorationState *state);

    inline IDecorationState *decorationState() const {
        return _decorationState.get();
    }

#ifdef __CLIENT__

    MaterialAnimator *materialAnimator() const;

    /**
     * Resets all lookups that are used for accelerating common operations.
     */
    void resetLookups();

//- Origin smoothing --------------------------------------------------------------------

    /// Notified when the @em sharp material origin changes.
    DENG2_DEFINE_AUDIENCE2(OriginSmoothedChange, void surfaceOriginSmoothedChanged(Surface &surface))

    /// Maximum speed for a smoothed material offset.
    static de::dint const MAX_SMOOTH_MATERIAL_MOVE = 8;

    /**
     * Returns the current smoothed (interpolated) material origin for the
     * surface in the map coordinate space.
     *
     * @see setOrigin()
     */
    de::Vec2f const &originSmoothed() const;

    /**
     * Returns the delta between current and the smoothed material origin for
     * the surface in the map coordinate space.
     *
     * @see setOrigin(), smoothOrigin()
     */
    de::Vec2f const &originSmoothedAsDelta() const;

    /**
     * Perform smoothed material origin interpolation.
     *
     * @see originSmoothed()
     */
    void lerpSmoothedOrigin();

    /**
     * Reset the surface's material origin tracking.
     *
     * @see originSmoothed()
     */
    void resetSmoothedOrigin();

    /**
     * Roll the surface's material origin tracking buffer.
     */
    void updateOriginTracking();

//---------------------------------------------------------------------------------------

    /**
     * Determine the glow properties of the surface, which, are derived from the
     * bound material (averaged color).
     *
     * @param color  Amplified glow color is written here.
     *
     * @return  Glow strength/intensity or @c 0 if not presently glowing.
     */
    de::dfloat glow(de::Vec3f &color) const;

#endif // __CLIENT__

protected:
    de::dint property(world::DmuArgs &args) const;
    de::dint setProperty(world::DmuArgs const &args);

private:
    std::unique_ptr<IDecorationState> _decorationState;

    DENG2_PRIVATE(d)
};

#endif  // DENG_WORLD_SURFACE_H
