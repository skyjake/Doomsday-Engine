/** @file texturevariantspec.h Texture Variant Specification.
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

#ifndef LIBDENG_RESOURCE_TEXTUREVARIANTSPEC_H
#define LIBDENG_RESOURCE_TEXTUREVARIANTSPEC_H

#ifndef __CLIENT__
#  error "resource/texturevariantspec.h only exists in the Client"
#endif

#include <de/String>
#include "dd_types.h"

typedef enum {
    TC_UNKNOWN = -1,
    TEXTUREVARIANTUSAGECONTEXT_FIRST = 0,
    TC_UI = TEXTUREVARIANTUSAGECONTEXT_FIRST,
    TC_MAPSURFACE_DIFFUSE,
    TC_MAPSURFACE_REFLECTION,
    TC_MAPSURFACE_REFLECTIONMASK,
    TC_MAPSURFACE_LIGHTMAP,
    TC_SPRITE_DIFFUSE,
    TC_MODELSKIN_DIFFUSE,
    TC_MODELSKIN_REFLECTION,
    TC_HALO_LUMINANCE,
    TC_PSPRITE_DIFFUSE,
    TC_SKYSPHERE_DIFFUSE,
    TEXTUREVARIANTUSAGECONTEXT_LAST = TC_SKYSPHERE_DIFFUSE
} texturevariantusagecontext_t;

#define TEXTUREVARIANTUSAGECONTEXT_COUNT (\
    TEXTUREVARIANTUSAGECONTEXT_LAST + 1 - TEXTUREVARIANTUSAGECONTEXT_FIRST )

#define VALID_TEXTUREVARIANTUSAGECONTEXT(tc) (\
    (tc) >= TEXTUREVARIANTUSAGECONTEXT_FIRST && (tc) <= TEXTUREVARIANTUSAGECONTEXT_LAST)

/**
 * @defgroup textureVariantSpecificationFlags  Texture Variant Specification Flags
 * @ingroup flags
 */
/*@{*/
#define TSF_ZEROMASK                0x1 // Set pixel alpha to fully opaque.
#define TSF_NO_COMPRESSION          0x2
#define TSF_UPSCALE_AND_SHARPEN     0x4
#define TSF_MONOCHROME              0x8

#define TSF_INTERNAL_MASK           0xff000000
#define TSF_HAS_COLORPALETTE_XLAT   0x80000000
/*@}*/

typedef enum {
    TEXTUREVARIANTSPECIFICATIONTYPE_FIRST = 0,
    TST_GENERAL = TEXTUREVARIANTSPECIFICATIONTYPE_FIRST,
    TST_DETAIL,
    TEXTUREVARIANTSPECIFICATIONTYPE_LAST = TST_DETAIL
} texturevariantspecificationtype_t;

#define TEXTUREVARIANTSPECIFICATIONTYPE_COUNT (\
    TEXTUREVARIANTSPECIFICATIONTYPE_LAST + 1 - TEXTUREVARIANTSPECIFICATIONTYPE_FIRST )

#define VALID_TEXTUREVARIANTSPECIFICATIONTYPE(t) (\
    (t) >= TEXTUREVARIANTSPECIFICATIONTYPE_FIRST && (t) <= TEXTUREVARIANTSPECIFICATIONTYPE_LAST)

typedef struct {
    int tClass, tMap;
} colorpalettetranslationspecification_t;

typedef struct {
    texturevariantusagecontext_t context;
    int flags; /// @ref textureVariantSpecificationFlags
    byte border; /// In pixels, added to all four edges of the texture.
    int wrapS, wrapT;
    boolean mipmapped, gammaCorrection, noStretch, toAlpha;

    /**
     * Minification filter modes. Specified using either a logical
     * texture class id (actual mode used is then determined by the
     * user's preference for that class) or a constant value.
     *
     * Texture class:
     * -1: No class
     *
     * Constant:
     * 0: Nearest or Nearest-Mipmap-Nearest (if mipmapping)
     * 1: Linear  or Linear-Mipmap-Nearest (if mipmapping)
     * 2: Nearest-Mipmap-Linear (mipmapping only)
     * 3: Linear-Mipmap-Linear (mipmapping only)
     */
    int minFilter;

    /**
     * Magnification filter modes. Specified using either a logical
     * texture class id (actual mode used is then determined by the
     * user's preference for that class) or a constant value.
     *
     * Texture class:
     * -3: UI class
     * -2: Sprite class
     * -1: No class
     *
     * Constant:
     * 0: Nearest (in Manhattan distance)
     * 1: Linear (weighted average)
     */
    int magFilter;

    /// -1: User preference else a logical DGL anisotropic filter level.
    int anisoFilter;

    /// Additional color palette translation spec.
    colorpalettetranslationspecification_t *translated;
} variantspecification_t;

/**
 * Detail textures are faded to gray depending on the contrast factor.
 * The texture is also progressively faded towards gray in each mipmap
 * level uploaded.
 *
 * Contrast is quantized in order to reduce the number of variants to
 * a more sensible/manageable number per texture.
 */
#define DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR  (10)

typedef struct {
    uint8_t contrast;
} detailvariantspecification_t;

#define TS_GENERAL(ts)      ((ts).data.variant)
#define TS_DETAIL(ts)       ((ts).data.detailvariant)

typedef struct texturevariantspecification_s {
    texturevariantspecificationtype_t type;
    union {
        variantspecification_t variant;
        detailvariantspecification_t detailvariant;
    } data; // type-specific data.

    /**
     * Returns a textual, human-readable representation of the specification.
     */
    de::String asText() const;
} texturevariantspecification_t;

/**
 * Compare the given TextureVariantSpecifications and determine whether they can
 * be considered equal (dependent on current engine state and the available features
 * of the GL implementation).
 */
int TextureVariantSpec_Compare(texturevariantspecification_t const *a,
    texturevariantspecification_t const *b);

#endif /* LIBDENG_RESOURCE_TEXTUREVARIANTSPEC_H */
