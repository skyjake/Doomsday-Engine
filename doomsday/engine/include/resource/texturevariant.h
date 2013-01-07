/** @file texturevariant.h Logical Texture Variant.
 *
 * @authors Copyright &copy; 2011-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_TEXTUREVARIANT_H
#define LIBDENG_RESOURCE_TEXTUREVARIANT_H

#include "texturevariantspecification.h"

#ifdef __cplusplus

#include <QFlag>

namespace de {

    class Texture;

    /**
     * @ingroup resource
     */
    class TextureVariant
    {
    private:
        enum Flag
        {
            /// Texture contains alpha.
            Masked = 0x1,

            /// Texture has been uploaded to GL.
            Uploaded = 0x2
        };
        Q_DECLARE_FLAGS(Flags, Flag)

    public:
        /**
         * @param generalCase   Texture from which this variant is derived.
         * @param spec          Specification used to derive this variant.
         *                      Ownership is NOT given to the resultant TextureVariant
         * @param source        Source of this variant.
         */
        TextureVariant(Texture &generalCase, texturevariantspecification_t &spec,
                       TexSource source = TEXS_NONE);

        /// @return  Superior Texture of which this is a derivative.
        Texture &generalCase() const { return texture; }

        /// @return  Source of this variant.
        TexSource source() const { return texSource; }

        /**
         * Change the source of this variant.
         * @param newSource  New TextureSource.
         */
        void setSource(TexSource newSource);

        /// @return  TextureVariantSpecification used to derive this variant.
        texturevariantspecification_t *spec() const { return varSpec; }

        bool isMasked() const { return !!(flags & Masked); }

        void flagMasked(bool yes);

        bool isUploaded() const { return !!(flags & Uploaded); }

        void flagUploaded(bool yes);

        bool isPrepared() const;

        void coords(float *s, float *t) const;
        void setCoords(float s, float t);

        uint glName() const { return glTexName; }

        void setGLName(uint glName);

    private:
        /// Superior Texture of which this is a derivative.
        Texture &texture;

        /// Source of this texture.
        TexSource texSource;

        Flags flags;

        /// Name of the associated GL texture object.
        uint glTexName;

        /// Prepared coordinates for the bottom right of the texture minus border.
        float s, t;

        /// Specification used to derive this variant.
        texturevariantspecification_t *varSpec;
    };

} // namespace de

#endif // __cplusplus

struct texturevariant_s; // The texture variant instance (opaque).

#endif /* LIBDENG_RESOURCE_TEXTUREVARIANT_H */
