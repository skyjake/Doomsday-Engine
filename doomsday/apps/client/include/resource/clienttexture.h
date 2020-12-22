/** @file texture.h  Logical texture resource.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_RESOURCE_CLIENTTEXTURE_H
#define DE_RESOURCE_CLIENTTEXTURE_H

#include "image.h" // res::Source
#include "texturevariantspec.h"

#include <doomsday/res/texture.h>

/**
 * Logical texture resource.
 *
 * @ingroup resource
 */
class ClientTexture : public res::Texture
{
public:
     /**
     * Context-specialized variant. Encapsulates all context variant values
     * and logics pertaining to a specialized version of the @em superior
     * Texture instance.
     *
     * @see TextureVariantSpec
     */
    class Variant
    {
    public:
        enum Flag
        {
            /// Texture contains alpha.
            /// @todo Does not belong here (is actually a source image analysis).
            Masked = 0x1
        };

        /**
         * @param texture  Base Texture from which the draw-context variant is derived.
         * @param spec     Draw-context variant specification.
         */
        Variant(ClientTexture &texture, const TextureVariantSpec &spec);

    public:
        /**
         * Returns the base Texture for the draw-context variant.
         */
        ClientTexture &base() const;

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
         * @return  GL-name of the uploaded texture.
         */
        uint prepare();

        /**
         * Release any uploaded GL-texture and clear the associated GL-name
         * for the variant.
         */
        void release();

        /**
         * Returns the specification used to derive the variant.
         */
        const TextureVariantSpec &spec() const;

        /**
         * Returns the source of the image used to prepare the uploaded GL-texture
         * for the variant.
         */
        res::Source source() const;

        /**
         * Returns a textual description of the source of the variant.
         *
         * @return Human-friendly description of the source of the variant.
         */
        de::String sourceDescription() const;

        /**
         * Returns the flags for the variant.
         */
        de::Flags flags() const;

        /**
         * Returns @c true if the variant is flagged @a flagsToTest.
         */
        inline bool isFlagged(de::Flags flagsToTest) const
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

    private:
        DE_PRIVATE(d)
    };

    /// A list of variants.
    typedef de::List<Variant *> Variants;

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
        //FuzzyMatchSpec
    };

public:
    ClientTexture(res::TextureManifest &manifest);

    void release() override;
    de::String description() const override;

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
                           const TextureVariantSpec &spec,
                           bool canCreate = false);

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
     *
     * @return  The prepared texture variant if successful; otherwise @c 0.
     *
     * @see chooseVariant()
     */
    Variant *prepareVariant(const TextureVariantSpec &spec);

    /**
     * Provides access to the list of variant instances for efficent traversal.
     */
    const Variants &variants() const;

    /**
     * Returns the number of variants for the texture.
     */
    uint variantCount() const;

private:
    DE_PRIVATE(d)
};

typedef ClientTexture::Variant TextureVariant;

#endif // DE_RESOURCE_CLIENTTEXTURE_H
