/** @file materialsnapshot.h Logical material state snapshot.
 *
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_MATERIALSNAPSHOT_H
#define LIBDENG_RESOURCE_MATERIALSNAPSHOT_H

// Material texture unit idents:
enum {
    MTU_PRIMARY,
    MTU_DETAIL,
    MTU_REFLECTION,
    MTU_REFLECTION_MASK,
    NUM_MATERIAL_TEXTURE_UNITS
};

#include "Material"
#include "Texture"
#ifdef __CLIENT__
#  include "render/rendpoly.h"
#endif
#include <de/Error>
#include <de/Vector>
#include <QSize>

namespace de {

/**
 * Logical material state snapshot.
 *
 * @ingroup resource
 */
class MaterialSnapshot
{
public:
#ifdef __CLIENT__
    /// Interpolated (light) decoration properties.
    struct Decoration
    {
        float pos[2]; // Coordinates in material space.
        float elevation; // Distance from the surface.
        float color[3]; // Light color.
        float radius; // Dynamic light radius (-1 = no light).
        float haloRadius; // Halo radius (zero = no halo).
        float lightLevels[2]; // Fade by sector lightlevel.

        DGLuint tex, ceilTex, floorTex;
        DGLuint flareTex;
    };
#endif

public:
    /// The referenced (texture) unit does not exist. @ingroup errors
    DENG2_ERROR(UnknownUnitError);

#ifdef __CLIENT__
    /// The referenced decoration does not exist. @ingroup errors
    DENG2_ERROR(UnknownDecorationError);
#endif

public:
    /**
     * Construct a new material snapshot instance.
     * @param materialVariant  Material variant to capture to produce the snapshot.
     */
    MaterialSnapshot(MaterialVariant &materialVariant);

    ~MaterialSnapshot();

    /**
     * Returns the material variant for the snapshot.
     */
    MaterialVariant &materialVariant() const;

    /**
     * Returns the material for the snapshot, for convenience.
     */
    inline Material &material() const { return materialVariant().generalCase(); }

    /**
     * Returns the dimensions in the world coordinate space for the material snapshot.
     */
    QSize const &dimensions() const;

    /**
     * Returns @c true if the material snapshot is completely opaque.
     */
    bool isOpaque() const;

    /**
     * Returns the interpolated, glow strength multiplier for the material snapshot.
     */
    float glowStrength() const;

    /**
     * Returns the interpolated, shine effect minimum ambient light color for the material snapshot.
     */
    Vector3f const &shineMinColor() const;

    /**
     * Returns @c true if a texture with @a index is set for the material snapshot.
     */
    bool hasTexture(int index) const;

    /**
     * Lookup a material snapshot texture by logical material texture unit index.
     *
     * @param index  Index of the texture to lookup.
     * @return  The texture associated with the logical material texture unit.
     */
    Texture::Variant &texture(int index) const;

#ifdef __CLIENT__
    /**
     * Lookup a material snapshot prepared texture unit by id.
     *
     * @param id  Identifier of the texture unit to lookup.
     * @return  The associated prepared texture unit.
     */
    rtexmapunit_t const &unit(rtexmapunitid_t id) const;

    /**
     * Lookup a material snapshot decoration by index.
     *
     * @param index  Index of the decoration to lookup.
     * @return  The associated decoration data.
     */
    Decoration &decoration(int index) const;
#endif

    /**
     * Prepare all textures and update property values.
     */
    void update();

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif /* LIBDENG_RESOURCE_MATERIALSNAPSHOT_H */
