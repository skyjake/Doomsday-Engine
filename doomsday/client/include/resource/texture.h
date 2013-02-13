/** @file texture.h Logical Texture.
 *
 * @authors Copyright © 2003-2013 Jaakko Ker√§nen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "TextureVariantSpec"
#include <de/Error>
#include <QFlag>
#include <QList>
#include <QPoint>
#include <QSize>

namespace de {

class TextureManifest;

/**
 * Logical texture resource.
 *
 * @ingroup resource
 */
class Texture
{
    struct Instance; // Needs to be friended by Variant.

public:
    /**
     * Classification/processing flags.
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

    /**
     * Image analysis identifiers.
     */
    enum AnalysisId
    {
        /// Color palette info.
        ColorPaletteAnalysis,

        /// Brightest point for automatic light sources.
        BrightPointAnalysis,

        /// Average color.
        AverageColorAnalysis,

        /// Average color amplified (max component ==1).
        AverageColorAmplifiedAnalysis,

        /// Average alpha.
        AverageAlphaAnalysis,

        /// Average top line color.
        AverageTopColorAnalysis,

        /// Average bottom line color.
        AverageBottomColorAnalysis
    };

    /**
     * Context-specialized variant. Encapsulates all context variant values
     * and logics pertaining to a specialized version of the @em superior
     * Texture instance.
     *
     * @see texturevariantspecification_t
     */
    class Variant
    {
    public:
        enum Flag
        {
            /// Texture contains alpha.
            Masked = 0x1,

            /// Texture has been uploaded to GL.
            Uploaded = 0x2
        };
        Q_DECLARE_FLAGS(Flags, Flag)

    private:
        /**
         * @param generalCase   Texture from which this variant is derived.
         * @param spec          Specification used to derive this variant.
         *                      Ownership is NOT given to the Variant.
         */
        Variant(Texture &generalCase, texturevariantspecification_t const &spec);
        ~Variant();

    public:
        /**
         * Retrieve the general case for this variant. Allows for a variant
         * reference to be used in place of a texture (implicit indirection).
         *
         * @see generalCase()
         */
        inline operator Texture &() const {
            return generalCase();
        }

        /// @return  Superior texture of which the variant is a derivative.
        Texture &generalCase() const;

        /// @return  Texture variant specification for the variant.
        texturevariantspecification_t const &spec() const;

        /// @return  Source of the variant.
        TexSource source() const;

        /**
         * Change the source of the variant.
         * @param newSource  New TextureSource.
         */
        void setSource(TexSource newSource);

        void coords(float *s, float *t) const;

        void setCoords(float s, float t);

        /// Returns @c true if the variant is flagged as "masked".
        inline bool isMasked() const { return flags().testFlag(Masked); }

        /// Returns @c true if the variant is flagged as "uploaded".
        inline bool isUploaded() const { return flags().testFlag(Uploaded); }

        /// Returns @c true if the variant is "prepared".
        inline bool isPrepared() const { return isUploaded() && glName() != 0; }

        /**
         * Returns the flags for the variant.
         */
        Flags flags() const;

        /**
         * Change the variant's flags.
         *
         * @param flagsToChange  Flags to change the value of.
         * @param set  @c true to set, @c false to clear.
         */
        void setFlags(Flags flagsToChange, bool set = true);

        uint glName() const;

        void setGLName(uint glName);

        friend class Texture;
        friend struct Texture::Instance;

    private:
        struct Instance;
        Instance *d;
    };

    /// A list of variants.
    typedef QList<Variant *> Variants;

    /**
     * Logics for selecting a texture variant instance from the candidates.
     *
     * @see chooseVariant()
     */
    enum ChooseVariantMethod
    {
        /// The variant specification of the candidate must match exactly.
        MatchSpec,

        /// The variant specification of the candidate must match however
        /// certain properties may vary (e.g., quality arguments) if it means
        /// we can avoid creating a new variant.
        FuzzyMatchSpec
    };

public:
    /**
     * @param manifest  Manifest derived to yield the texture.
     */
    Texture(TextureManifest &manifest);
    ~Texture();

    /**
     * Returns the TextureManifest derived to yield the texture.
     */
    TextureManifest &manifest() const;

    /// Returns the dimensions of the texture in map coordinate space units.
    QSize const &dimensions() const;

    /// Returns the world width of the texture in map coordinate space units.
    inline int width() const { return dimensions().width(); }

    /// Returns the world height of the texture in map coordinate space units.
    inline int height() const { return dimensions().height(); }

    /**
     * Change the world dimensions of the texture.
     * @param newDimensions  New dimensions in map coordinate space units.
     *
     * @todo Update any Materials (and thus Surfaces) which reference this.
     */
    void setDimensions(QSize const &newDimensions);

    /**
     * Change the world width of the texture.
     * @param newWidth  New width in map coordinate space units.
     *
     * @todo Update any Materials (and thus Surfaces) which reference this.
     */
    void setWidth(int newWidth);

    /**
     * Change the world height of the texture.
     * @param newHeight  New height in map coordinate space units.
     *
     * @todo Update any Materials (and thus Surfaces) which reference this.
     */
    void setHeight(int newHeight);

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
     * Returns the flags for the texture.
     */
    Flags flags() const;

    /**
     * Change the texture's flags.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param set  @c true to set, @c false to clear.
     */
    void setFlags(Flags flagsToChange, bool set = true);

    /**
     * Destroys all derived variants for the texture.
     */
    void clearVariants();

    /**
     * Choose/create a variant of the texture which fulfills @a spec.
     *
     * @param method    Method of selection.
     * @param spec      Texture specialization specification.
     * @param canCreate @c true= Create a new variant if no suitable one exists.
     *
     * @return  Chosen variant; otherwise @c NULL if none suitable and not creating.
     */
    Variant *chooseVariant(ChooseVariantMethod method,
        texturevariantspecification_t const &spec, bool canCreate = false);

    /**
     * Provides access to the list of variant instances for efficent traversal.
     */
    Variants const &variants() const;

    /**
     * Returns the number of variants for the texture.
     */
    uint variantCount() const;

    /**
     * Destroys all analyses for the texture.
     */
    void clearAnalyses();

    /**
     * Retrieve the value of an identified @a analysisId data pointer.
     * @return  Associated data pointer value.
     */
    void *analysisDataPointer(AnalysisId analysisId) const;

    /**
     * Set the value of an identified @a analysisId data pointer. Ownership of
     * the data is not given to this instance.
     *
     * @note If already set the old value will be replaced (so if it points
     *       to some dynamically constructed data/resource it is the caller's
     *       responsibility to release it beforehand).
     *
     * @param analysisId  Identifier of the data being attached.
     * @param data  Data to be attached.
     */
    void setAnalysisDataPointer(AnalysisId analysisId, void *data);

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

private:
    Instance *d;
};

} // namespace de

struct texture_s; // The texture instance (opaque).

#endif /// LIBDENG_RESOURCE_TEXTURE_H
