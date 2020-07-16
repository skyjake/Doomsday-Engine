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

#pragma once

#include "../uri.h"
#include "material.h"

#include <de/id.h>
#include <de/matrix.h>
#include <de/observers.h>
#include <de/string.h>
#include <de/vector.h>

namespace world {

/**
 * An infinite two-dimensional geometric plane.
 */
class LIBDOOMSDAY_PUBLIC Surface : public MapElement
{
    DE_NO_COPY  (Surface)
    DE_NO_ASSIGN(Surface)

public:
    /// Interface for surface decoration state.
    class IDecorationState
    {
    public:
        virtual ~IDecorationState();
    };

    /// Notified whenever the tint color changes.
    DE_AUDIENCE(ColorChange,   void surfaceColorChanged(Surface &sector))

    /// Notified whenever the normal vector changes.
    DE_AUDIENCE(NormalChange,  void surfaceNormalChanged(Surface &surface))

    /// Notified whenever the opacity changes.
    DE_AUDIENCE(OpacityChange, void surfaceOpacityChanged(Surface &surface))

    /// Notified whenever the @em sharp origin changes.
    DE_AUDIENCE(OriginChange,  void surfaceOriginChanged(Surface &surface))

public:
    /**
     * Construct a new surface.
     *
     * @param owner    Map element which will own the surface.
     * @param opacity  Opacity strength (@c 1= fully opaque).
     * @param color    Tint color.
     */
    Surface(world::MapElement &owner,
            float opacity        = 1,
            const de::Vec3f &color = de::Vec3f(1));

    /**
     * Composes a human-friendly, styled, textual description of the surface.
     */
    de::String description() const;

    /**
     * Returns the normalized tangent space matrix for the surface.
     * (col0: tangent, col1: bitangent, col2: normal)
     */
    const de::Mat3f &tangentMatrix() const;

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
    Surface &setNormal(const de::Vec3f &newNormal);

    /**
     * Returns the opacity of the surface. The OpacityChange audience is notified
     * whenever the opacity changes.
     *
     * @see setOpacity()
     */
    float opacity() const;
    Surface &setOpacity(float newOpacity);

    /**
     * Returns the material origin offset for the surface.
     */
    const de::Vec2f &origin() const;
    Surface &setOrigin(const de::Vec2f &newOrigin);

    /**
     * Returns the tint color of the surface. The ColorChange audience is
     * notified whenever the tint color changes.
     *
     * @see setColor()
     */
    const de::Vec3f &color() const;
    Surface &setColor(const de::Vec3f &newColor);

    /**
     * Returns the blendmode for the surface.
     */
    blendmode_t blendMode() const;
    Surface &setBlendMode(blendmode_t newBlendMode);

//- Material ----------------------------------------------------------------------------

    /// Thrown when a required material is missing. @ingroup errors
    DE_ERROR(MissingMaterialError);

    /// Notified when the material changes.
    DE_AUDIENCE(MaterialChange, void surfaceMaterialChanged(Surface &surface))

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
    res::Uri composeMaterialUri() const;

    void setDecorationState(IDecorationState *state);

    inline IDecorationState *decorationState() const {
        return _decorationState.get();
    }

    virtual void resetLookups();

protected:
    int property(world::DmuArgs &args) const;
    int setProperty(const world::DmuArgs &args);

private:
    std::unique_ptr<IDecorationState> _decorationState;

    DE_PRIVATE(d)
};

} // namespace world
