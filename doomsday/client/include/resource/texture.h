/** @file texture.h Logical Texture.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifdef __CLIENT__
#  include "TextureVariantSpec"
#endif
#include <de/Error>
#include <de/Observers>
#include <de/Vector>
#include <QFlag>
#include <QList>

/**
 * Texture (content) Source.
 */
typedef enum {
    TEXS_NONE,                    /// Not a valid source.
    TEXS_ORIGINAL,                /// An "original".
    TEXS_EXTERNAL                 /// An "external" replacement.
} TexSource;

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
    DENG2_DEFINE_AUDIENCE(Deletion, void textureBeingDeleted(Texture const &texture))
    DENG2_DEFINE_AUDIENCE(DimensionsChange, void textureDimensionsChanged(Texture const &texture))

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

#ifdef __CLIENT__

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
        /// Logical prepare() result.
        enum PrepareResult
        {
            NotFound,           ///< Failed. No suitable variant could be found/prepared.
            Found,              ///< Success. Reusing a cached resource.
            UploadedOriginal,   ///< Success. Prepared and cached using an original-game resource.
            UploadedExternal    ///< Success. Prepared and cached using an external-replacement resource.
        };

        enum Flag
        {
            /// Texture contains alpha.
            /// @todo Does not belong here (is actually a source image analysis).
            Masked = 0x1
        };
        Q_DECLARE_FLAGS(Flags, Flag)

    private:
        /**
         * @param generalCase   Texture from which this variant is derived.
         * @param spec          Specification used to derive this variant.
         *                      Ownership is NOT given to the Variant.
         */
        Variant(Texture &generalCase, texturevariantspecification_t const &spec);

    public:
        /// @return  Superior texture of which the variant is a derivative.
        Texture &generalCase() const;

        /// Returns @c true if the variant is "prepared".
        inline bool isPrepared() const { return glName() != 0; }

        /// Returns @c true if the variant is flagged as "masked".
        inline bool isMasked() const { return isFlagged(Masked); }

        /**
         * Prepare the texture variant for render.
         *
         * @note If a cache miss occurs texture content data may need to be
         * (re-)uploaded to GL. However, the actual upload will be deferred
         * if possible. This has the side effect that although the variant
         * is considered "prepared", attempts to render using the associated
         * GL texture will result in "uninitialized" white texels being used
         * instead.
         *
         * @param result  If not @c 0 the logical result of this operation
         *                is written here.
         *
         * @return  GL-name of the uploaded texture.
         */
        uint prepare(PrepareResult *result = 0);

        /**
         * Release any uploaded GL-texture and clear the associated GL-name
         * for the variant.
         */
        void release();

        /**
         * Returns the specification used to derive the variant.
         */
        texturevariantspecification_t const &spec() const;

        /**
         * Returns the source of the image used to prepare the uploaded GL-texture
         * for the variant.
         */
        TexSource source() const;

        /**
         * Returns a textual description of the source of the variant.
         *
         * @return Human-friendly description of the source of the variant.
         */
        String sourceDescription() const;

        /**
         * Returns the flags for the variant.
         */
        Flags flags() const;

        /**
         * Returns @c true if the variant is flagged @a flagsToTest.
         */
        inline bool isFlagged(Flags flagsToTest) const
        {
            return (flags() & flagsToTest) != 0;
        }

        /**
         * Returns the GL-name of the uploaded texture content for the variant;
         * otherwise @c 0 (not uploaded).
         */
        uint glName() const;

        /**
         * Returns the prepared GL-texture coordinates for the variant.
         *
         * @param s  S axis coordinate.
         * @param t  T axis coordinate.
         */
        void glCoords(float *s, float *t) const;

        friend class Texture;
        friend struct Texture::Instance;

    private:
        DENG2_PRIVATE(d)
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

#endif // __CLIENT__

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

    /**
     * Returns a brief textual description/overview of the texture.
     *
     * @return Human-friendly description/overview of the texture.
     */
    String description() const;

    /**
     * Returns the world dimensions of the texture, in map coordinate space
     * units. The DimensionsChange audience is notified whenever dimensions
     * are changed.
     */
    Vector2i const &dimensions() const;

    /**
     * Convenient accessor method for returning the X axis size (width) of
     * the world dimensions for the texture, in map coordinate space units.
     *
     * @see dimensions()
     */
    inline int width() const { return dimensions().x; }

    /**
     * Convenient accessor method for returning the X axis size (height) of
     * the world dimensions for the texture, in map coordinate space units.
     *
     * @see dimensions()
     */
    inline int height() const { return dimensions().y; }

    /**
     * Change the world dimensions of the texture.
     * @param newDimensions  New dimensions in map coordinate space units.
     *
     * @todo Update any Materials (and thus Surfaces) which reference this.
     */
    void setDimensions(Vector2i const &newDimensions);

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
    Vector2i const &origin() const;

    /**
     * Change the world origin offset of the texture.
     * @param newOrigin  New origin in map coordinate space units.
     */
    void setOrigin(Vector2i const &newOrigin);

    /**
     * Returns @c true if the texture is flagged @a flagsToTest.
     */
    inline bool isFlagged(Flags flagsToTest) const { return !!(flags() & flagsToTest); }

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

#ifdef __CLIENT__

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
     * Choose/create a variant of the texture which fulfills @a spec and then
     * immediately prepare it for render.
     *
     * @note A convenient shorthand of the call tree:
     * <pre>
     *    chooseVariant(MatchSpec, @a spec, true)->prepareVariant();
     * </pre>
     *
     * @param spec      Specification for the derivation of the texture.
     * @param returnOutcome  If not @c 0 the logical result is written back here.
     *
     * @return  The prepared texture variant if successful; otherwise @c 0.
     *
     * @see chooseVariant()
     */
    Variant *prepareVariant(texturevariantspecification_t const &spec,
        Variant::PrepareResult *returnOutcome = 0);

    /**
     * Provides access to the list of variant instances for efficent traversal.
     */
    Variants const &variants() const;

    /**
     * Returns the number of variants for the texture.
     */
    uint variantCount() const;

#endif // __CLIENT__

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

#ifdef __CLIENT__
// Alias.
typedef Texture::Variant TextureVariant;
#endif

} // namespace de

struct texture_s; // The texture instance (opaque).

#endif // LIBDENG_RESOURCE_TEXTURE_H
