/** @file materialsnapshot.h Material Snapshot.
 *
 * @author Copyright &copy; 2011-2012 Daniel Swanson <danij@dengine.net>
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

#ifdef __cplusplus

#include <QSize>
#include <de/Error>
#include "de/vector1.h"
#include "resource/materialvariant.h"
#include "resource/texturevariant.h"
#include "render/rendpoly.h"

namespace de {

    /**
     * @ingroup resource
     */
    class MaterialSnapshot
    {
    public:
        /// Invalid texture unit referenced. @ingroup errors
        DENG2_ERROR(InvalidUnitError);

    public:
        /**
         * Construct a new material snapshot instance.
         * @param material  Material to capture to produce the snapshot.
         */
        MaterialSnapshot(MaterialVariant &material);

        ~MaterialSnapshot();

        /**
         * Returns the material variant of the material snapshot.
         */
        MaterialVariant &material() const;

        /**
         * Returns the dimensions in the world coordinate space for the material snapshot.
         */
        QSize const &dimensions() const;

        /**
         * Returns @c true if the material snapshot is completely opaque.
         */
        bool isOpaque() const;

        /**
         * Returns the glow strength multiplier for the material snapshot.
         */
        float glowStrength() const;

        /**
         * Returns the minimum ambient light color for the material snapshot.
         */
        vec3f_t const &reflectionMinColor() const;

        /**
         * Returns @c true if a texture with @a index is set for the material snapshot.
         */
        bool hasTexture(int index) const;

        /**
         * Lookup a material snapshot texture unit texture by index.
         *
         * @param index  Index of the texture unit to lookup.
         * @return  The texture associated with the texture unit.
         */
        TextureVariant &texture(int index) const;

        /**
         * Lookup a material snapshot texture unit by index.
         *
         * @param index  Index of the texture unit to lookup.
         * @return  The associated texture unit.
         */
        rtexmapunit_t const &unit(int index) const;

        /**
         * Prepare all textures and update property values.
         */
        void update();

    private:
        struct Instance;
        Instance *d;
    };

} // namespace de

struct materialsnapshot_s; // The material snapshot instance (opaque).

#endif

#endif /* LIBDENG_RESOURCE_MATERIALSNAPSHOT_H */
