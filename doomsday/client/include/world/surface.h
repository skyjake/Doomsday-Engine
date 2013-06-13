/** @file surface.h World map surface.
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

#include <de/Error>
#include <de/Observers>
#include <de/Vector>

#include "MapElement"
#include "Material"
#ifdef __CLIENT__
#  include "MaterialSnapshot"
#endif

class BspLeaf;

/**
 * World map surface.
 *
 * @ingroup world
 */
class Surface : public de::MapElement
{
public:
    /// Required material is missing. @ingroup errors
    DENG2_ERROR(MissingMaterialError);

    /*
     * Observers to be notified when the normal vector changes.
     */
    DENG2_DEFINE_AUDIENCE(NormalChange,
        void normalChanged(Surface &surface, de::Vector3f oldNormal,
                           int changedAxes /*bit-field (0x1=X, 0x2=Y, 0x4=Z)*/))
    /*
     * Observers to be notified when the @em sharp material origin changes.
     */
    DENG2_DEFINE_AUDIENCE(MaterialOriginChange,
        void materialOriginChanged(Surface &surface, de::Vector2f oldMaterialOrigin,
                                   int changedAxes /*bit-field (0x1=X, 0x2=Y)*/))
    /*
     * Observers to be notified when the opacity changes.
     */
    DENG2_DEFINE_AUDIENCE(OpacityChange,
        void opacityChanged(Surface &surface, float oldOpacity))

    /*
     * Observers to be notified when the tint color changes.
     */
    DENG2_DEFINE_AUDIENCE(TintColorChange,
        void tintColorChanged(Surface &sector, de::Vector3f const &oldTintColor,
                              int changedComponents /*bit-field (0x1=Red, 0x2=Green, 0x4=Blue)*/))

    // Constants:
    static int const MAX_SMOOTH_MATERIAL_MOVE = 8; ///< Maximum speed for a smoothed material offset.

#ifdef __CLIENT__
    struct DecorSource
    {
        de::Vector3d origin; ///< World coordinates of the decoration.
        BspLeaf *bspLeaf;
        /// @todo $revise-texture-animation reference by index.
        de::MaterialSnapshot::Decoration const *decor;
    };
#endif // __CLIENT__

public: /// @todo make private:
#ifdef __CLIENT__
    struct DecorationData
    {
        /// @c true= An update is needed.
        bool needsUpdate;

        /// Plotted decoration sources [numSources size].
        DecorSource *sources;
        int numSources;
    } _decorationData;
#endif

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
     * Returns the owning map element. Either @c DMU_SIDE, or @c DMU_PLANE.
     */
    de::MapElement &owner() const;

    /**
     * Returns the normalized tangent vector for the surface.
     */
    de::Vector3f const &tangent() const;

    /**
     * Returns the normalized bitangent vector for the surface.
     */
    de::Vector3f const &bitangent() const;

    /**
     * Returns the normalized normal vector for the surface.
     */
    de::Vector3f const &normal() const;

    /**
     * Change the tangent space normal vector for the surface. If changed,
     * the tangent vectors will be recalculated next time they are needed.
     * The NormalChange audience is notified whenever the normal changes.
     *
     * @param newNormal  New normal vector (will be normalized if needed).
     */
    void setNormal(de::Vector3f const &newNormal);

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
    void setFlags(int flagsToChange, de::FlagOp operation = de::SetFlags);

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
    bool setMaterial(Material *newMaterial, bool isMissingFix = false);

    /**
     * Returns the material origin offset for the surface.
     */
    de::Vector2f const &materialOrigin() const;

    /**
     * Change the material origin offset for the surface.
     *
     * @param newOrigin  New origin offset in map coordinate space units.
     */
    void setMaterialOrigin(de::Vector2f const &newOrigin);

    /**
     * Change the specified @a component of the material origin for the surface.
     * The MaterialOriginChange audience is notified whenever the material origin
     * changes.
     *
     * @param component    Index of the component axis (0=X, 1=Y).
     * @param newPosition  New position for the origin component axis.
     *
     * @see setMaterialorigin(), setMaterialOriginX(), setMaterialOriginY()
     */
    void setMaterialOriginComponent(int component, float newPosition);

    /**
     * Change the position of the X axis component of the material origin for the
     * surface. The MaterialOriginChange audience is notified whenever the material
     * origin changes.
     *
     * @param newPosition  New X axis position for the material origin.
     *
     * @see setMaterialOriginComponent(), setMaterialOriginY()
     */
    inline void setMaterialOriginX(float newPosition) { setMaterialOriginComponent(0, newPosition); }

    /**
     * Change the position of the Y axis component of the material origin for the
     * surface. The MaterialOriginChange audience is notified whenever the material
     * origin changes.
     *
     * @param newPosition  New Y axis position for the material origin.
     *
     * @see setMaterialOriginComponent(), setMaterialOriginX()
     */
    inline void setMaterialOriginY(float newPosition) { setMaterialOriginComponent(1, newPosition); }

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
    void setOpacity(float newOpacity);

    /**
     * Returns the tint color of the surface. The TintColorChange audience is notified
     * whenever the tint color changes.
     *
     * @see setTintColor(), tintColorComponent(), tintRed(), tintGreen(), tintBlue()
     */
    de::Vector3f const &tintColor() const;

    /**
     * Returns the strength of the specified @a component of the tint color for the
     * surface. The TintColorChange audience is notified whenever the tint color changes.
     *
     * @param component    RGB index of the color component (0=Red, 1=Green, 2=Blue).
     *
     * @see tintColor(), tintRed(), tintGreen(), tintBlue()
     */
    inline float tintColorComponent(int component) const { return tintColor()[component]; }

    /**
     * Returns the strength of the @em red component of the tint color for the surface
     * The TintColorChange audience is notified whenever the tint color changes.
     *
     * @see tintColorComponent(), tintGreen(), tintBlue()
     */
    inline float tintRed() const   { return tintColorComponent(0); }

    /**
     * Returns the strength of the @em green component of the tint color for the
     * surface. The TintColorChange audience is notified whenever the tint color changes.
     *
     * @see tintColorComponent(), tintRed(), tintBlue()
     */
    inline float tintGreen() const { return tintColorComponent(1); }

    /**
     * Returns the strength of the @em blue component of the tint color for the
     * surface. The TintColorChange audience is notified whenever the tint color changes.
     *
     * @see tintColorComponent(), tintRed(), tintGreen()
     */
    inline float tintBlue() const  { return tintColorComponent(2); }

    /**
     * Change the tint color for the surface. The TintColorChange audience is notified
     * whenever the tint color changes.
     *
     * @param newTintColor  New tint color.
     *
     * @see tintColor(), setTintColorComponent(), setTintRed(), setTintGreen(), setTintBlue()
     */
    void setTintColor(de::Vector3f const &newTintColor);

    /**
     * Change the strength of the specified @a component of the tint color for the
     * surface. The TintColorChange audience is notified whenever the tint color changes.
     *
     * @param component    RGB index of the color component (0=Red, 1=Green, 2=Blue).
     * @param newStrength  New strength factor for the color component.
     *
     * @see setTintColor(), setTintRed(), setTintGreen(), setTintBlue()
     */
    void setTintColorComponent(int component, float newStrength);

    /**
     * Change the strength of the red component of the tint color for the surface.
     * The TintColorChange audience is notified whenever the tint color changes.
     *
     * @param newStrength  New red strength for the tint color.
     *
     * @see setTintColorComponent(), setTintGreen(), setTintBlue()
     */
    inline void setTintRed(float newStrength)  { setTintColorComponent(0, newStrength); }

    /**
     * Change the strength of the green component of the tint color for the surface.
     * The TintColorChange audience is notified whenever the tint color changes.
     *
     * @param newStrength  New green strength for the tint color.
     *
     * @see setTintColorComponent(), setTintRed(), setTintBlue()
     */
    inline void setTintGreen(float newStrength) { setTintColorComponent(1, newStrength); }

    /**
     * Change the strength of the blue component of the tint color for the surface.
     * The TintColorChange audience is notified whenever the tint color changes.
     *
     * @param newStrength  New blue strength for the tint color.
     *
     * @see setTintColorComponent(), setTintRed(), setTintGreen()
     */
    inline void setTintBlue(float newStrength)  { setTintColorComponent(2, newStrength); }

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
     * Returns the current interpolated visual material origin of the surface
     * in the map coordinate space.
     *
     * @see setMaterialOrigin()
     */
    de::Vector2f const &visMaterialOrigin() const;

    /**
     * Returns the delta between current material origin and the interpolated
     * visual origin of the material in the map coordinate space.
     *
     * @see setMaterialOrigin(), visMaterialOrigin()
     */
    de::Vector2f const &visMaterialOriginDelta() const;

    /**
     * Interpolate the visible material origin.
     *
     * @see visMaterialOrigin()
     */
    void lerpVisMaterialOrigin();

    /**
     * Reset the surface's material origin tracking.
     *
     * @see visMaterialOrigin()
     */
    void resetVisMaterialOrigin();

    /**
     * Roll the surface's material origin tracking buffer.
     */
    void updateMaterialOriginTracking();

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
    int decorationCount() const;

    /**
     * Mark the surface as needing a decoration source update.
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
