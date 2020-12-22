/** @file lumobj.h Luminous object.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_CLIENT_RENDER_LUMOBJ_H
#define DE_CLIENT_RENDER_LUMOBJ_H

#include <de/legacy/aabox.h>

#include <de/vector.h>

#include "world/mapobject.h"
#include "resource/clienttexture.h"

/**
 * Luminous object. @ingroup render
 *
 * @todo This should implement ILightSource and be owned by ClientMobjThinkerData (or
 * Plane or anything else that acts as a light source). There is no need to keep
 * recreating light source information on each frame, when it can be kept in the
 * thinker/map data for the lifetime of the owner. -jk
 */
class Lumobj : public world::MapObject
{
public:
    /// Identifiers for attributing lightmaps (used during projection).
    enum LightmapSemantic {
        Side,
        Down,
        Up
    };

    /**
     * Base for any class wishing to act as the source of the luminous object.
     */
    class Source
    {
    public:
        virtual ~Source() {}

        /**
         * Calculate an occlusion factor for the light. The implementation should
         * return a value in the range [0..1], where @c 0 is fully occluded and
         * @c 1 is fully visible.
         *
         * The default implementation assumes the source is always visible.
         *
         * @param eye  Position of the eye in map space.
         */
        virtual float occlusion(const de::Vec3d &eye) const;
    };

public:
    /**
     * Construct a new luminous object.
     *
     * @param origin       Origin in map space.
     * @param radius       Radius in map space units.
     * @param color        Color/intensity.
     */
    Lumobj(const de::Vec3d &origin = de::Vec3d(),
           double radius           = 256,
           const de::Vec3f &color  = de::Vec3f(1));

    /// Construct a new luminious object by copying @a other.
    Lumobj(const Lumobj &other);

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

    /**
     * Returns the current radius scale factor (user configurable).
     */
    static float radiusFactor();

    /**
     * Returns the current radius maximum (user configurable).
     */
    static int radiusMax();

    /**
     * Change the attributed source of the lumobj.
     *
     * @param newSource  New source to attribute. Use @c 0 to clear.
     */
    void setSource(const Source *newSource);

    /**
     * Sets the mobj that is responsible for casting this light.
     *
     * @param mo  Thing.
     */
    void setSourceMobj(const struct mobj_s *mo);

    const struct mobj_s *sourceMobj() const;

    /**
     * Returns the light color/intensity of the lumobj.
     *
     * @see setColor()
     */
    const de::Vec3f &color() const;

    /**
     * Change the light color/intensity of the lumobj.
     *
     * @param newColor  New color to apply.
     *
     * @see color()
     */
    Lumobj &setColor(const de::Vec3f &newColor);

    /**
     * Returns the radius of the lumobj in map space units.
     *
     * @see setRadius()
     */
    double radius() const;

    /**
     * Change the radius of the lumobj in map space units.
     *
     * @param newRadius  New radius to apply.
     *
     * @see radius()
     */
    Lumobj &setRadius(double newRadius);

    /**
     * Returns an axis-aligned bounding box for the lumobj in map space, centered
     * on the origin with dimensions equal to @code radius * 2 @endcode.
     *
     * @see radius()
     */
    AABoxd bounds() const;

    /**
     * Returns the z-offset of the lumobj.
     *
     * @see setZOffset()
     */
    double zOffset() const;

    /**
     * Change the z-offset of the lumobj.
     *
     * @param newZOffset  New z-offset to apply.
     *
     * @see zOffset()
     */
    Lumobj &setZOffset(double newZOffset);

    /**
     * Returns the maximum distance at which the lumobj will be drawn. If no
     * maximum is configured then @c 0 is returned (default).
     *
     * @see setMaxDistance()
     */
    double maxDistance() const;

    /**
     * Change the maximum distance at which the lumobj will drawn. For use with
     * surface decorations, which, should only de visible within a fairly small
     * radius around the viewer).
     *
     * @param newMaxDistance  New maximum distance to apply.
     *
     * @see maxDistance()
     */
    Lumobj &setMaxDistance(double newMaxDistance);

    /**
     * Returns the identified custom lightmap (if any).
     *
     * @param semantic  Identifier of the lightmap.
     *
     * @see setLightmap()
     */
    ClientTexture *lightmap(LightmapSemantic semantic) const;

    /**
     * Change an attributed lightmap to the texture specified.
     *
     * @param semantic    Identifier of the lightmap to change.
     * @param newTexture  Lightmap texture to apply. Use @c 0 to clear.
     *
     * @see lightmap()
     */
    Lumobj &setLightmap(LightmapSemantic semantic, ClientTexture *newTexture);

    /**
     * Returns the current flare size of the lumobj.
     */
    float flareSize() const;

    /**
     * Change the flare size of the lumobj.
     *
     * @param newFlareSize  New flare size factor.
     */
    Lumobj &setFlareSize(float newFlareSize);

    /**
     * Returns the current flare texture of the lumobj.
     */
    DGLuint flareTexture() const;

    /**
     * Change the flare texture of the lumobj.
     *
     * @param newTexture  New flare texture.
     */
    Lumobj &setFlareTexture(DGLuint newTexture);

    /**
     * Calculate a distance attentuation factor for the lumobj.
     *
     * @param distFromEye  Distance between the lumobj and the viewer.
     *
     * @return  Attentuation factor [0..1].
     */
    float attenuation(double distFromEye) const;

    /**
     * Generates a VSPR_FLARE vissprite for the lumobj.
     *
     * @param eye          Position of the viewer in map space.
     * @param distFromEye  Distance between the lumobj and the viewer.
     */
    void generateFlare(const de::Vec3d &eye, double distFromEye);

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_RENDER_LUMOBJ_H
