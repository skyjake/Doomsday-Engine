/** @file texture.h Logical Texture.
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_TEXTURE_H
#define LIBDENG_RESOURCE_TEXTURE_H

#include "texturevariant.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup resource
///@{
typedef enum {
    TEXTURE_ANALYSIS_FIRST = 0,
    TA_COLORPALETTE = TEXTURE_ANALYSIS_FIRST,
    TA_SPRITE_AUTOLIGHT,
    TA_COLOR,                  ///< Average.
    TA_COLOR_AMPLIFIED,        ///< Average amplified (max component ==1).
    TA_ALPHA,                  ///< Average.
    TA_LINE_TOP_COLOR,         ///< Average.
    TA_LINE_BOTTOM_COLOR,      ///< Average.
    TEXTURE_ANALYSIS_COUNT
} texture_analysisid_t;

#define VALID_TEXTURE_ANALYSISID(id) (\
    (id) >= TEXTURE_ANALYSIS_FIRST && (id) < TEXTURE_ANALYSIS_COUNT)
///@}

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <list>
#include <QFlag>
#include <QPoint>
#include <QSize>

namespace de {

    class TextureManifest;

    /**
     * Logical texture object.
     * @ingroup resource
     */
    class Texture
    {
    public:
        /**
         * Classification/processing flags
         */
        enum Flag
        {
            /// Texture is not to be drawn.
            NoDraw              = 0x1,

            /// Texture is "custom" (i.e., not an original game resource).
            Custom              = 0x2,

            /// Apply the monochrome filter to the processed image.
            Monochrome          = 0x4,

            /// Apply the upscaleAndSharpen filter to the processed image.
            UpscaleAndSharpen   = 0x8
        };
        Q_DECLARE_FLAGS(Flags, Flag)

        typedef std::list<TextureVariant *> Variants;

    public:
        /**
         * @param manifest  Manifest derived to yield the texture.
         * @param userData  User data to associate with the resultant texture.
         */
        Texture(TextureManifest &manifest, void *userData = 0);

        ~Texture();

        /// @return  Provides access to the classification/processing flags.
        Flags const &flags() const;

        /// @return  Provides access to the classification/processing flags.
        Flags &flags();

        /**
         * Returns the TextureManifest derived to yield the texture.
         */
        TextureManifest &manifest() const;

        /**
         * Retrieve the value of the associated user data pointer.
         * @return  Associated data pointer value.
         */
        void *userDataPointer() const;

        /**
         * Set the user data pointer value. Ownership of the data is not given to
         * this instance.
         *
         * @note If already set the old value will be replaced (so if it points
         *       to some dynamically constructed data/resource it is the caller's
         *       responsibility to release it beforehand).
         *
         * @param userData  User data pointer value.
         */
        void setUserDataPointer(void *userData);

        /**
         * Add a new prepared variant to the list of resources for this Texture.
         * Texture takes ownership of the variant.
         *
         * @param variant  Variant instance to add to the resource list.
         */
        TextureVariant &addVariant(TextureVariant &variant);

        /// @return  Number of variants for the texture.
        uint variantCount() const;

        /// Destroy all analyses for the texture.
        void clearAnalyses();

        /// Destroy all prepared variants for the texture.
        void clearVariants();

        /**
         * Retrieve the value of an identified @a analysis data pointer.
         * @return  Associated data pointer value.
         **/
        void *analysisDataPointer(texture_analysisid_t analysis) const;

        /**
         * Set the value of an identified @a analysis data pointer. Ownership of
         * the data is not given to this instance.
         *
         * @note If already set the old value will be replaced (so if it points
         *       to some dynamically constructed data/resource it is the caller's
         *       responsibility to release it beforehand).
         *
         * @param analysis  Identifier of the data being attached.
         * @param data  Data to be attached.
         */
        void setAnalysisDataPointer(texture_analysisid_t analysis, void *data);

        /**
         * Returns the world width of the texture in map coordinate space units.
         */
        int width() const;

        /**
         * Returns the world height of the texture in map coordinate space units.
         */
        int height() const;

        /**
         * Returns the world dimensions [width, height] of the texture in map
         * coordinate space units.
         */
        QSize const &dimensions() const;

        /**
         * Change the world width of the texture.
         * @param newWidth  New width in map coordinate space units.
         */
        void setWidth(int newWidth);

        /**
         * Change the world height of the texture.
         * @param newHeight  New height in map coordinate space units.
         */
        void setHeight(int newHeight);

        /**
         * Change the world dimensions of the texture.
         * @param newDimensions  New dimensions [width, height] in map coordinate space units.
         */
        void setDimensions(QSize const &newDimensions);

        /**
         * Returns the world origin offset of texture in map coordinate space units.
         */
        QPoint const &origin() const;

        /**
         * Change the world origin offset of the texture.
         * @param newOrigin  New origin in map coordinate space units.
         */
        void setOrigin(QPoint const &newOrigin);

        /**
         * Provides access to the list of variant textures for efficent traversals.
         */
        Variants const &variantList() const;

    private:
        struct Instance;
        Instance *d;
    };

} // namespace de

#endif // __cplusplus

struct texture_s; // The texture instance (opaque).

#endif /// LIBDENG_RESOURCE_TEXTURE_H
