/**
 * @file texture.h
 * Logical texture variant. @ingroup resource
 *
 * @authors Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
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
extern "C" {
#endif

struct texture_s;

/**
 * @defgroup textureVariantFlags  Texture Variant Flags
 * @{
 */
#define TVF_IS_MASKED           0x1 /// Texture contains alpha.
#define TVF_IS_UPLOADED         0x2 /// Texture has been uploaded to GL.
/**@}*/

struct texturevariant_s; // The texturevariant instance (opaque).
typedef struct texturevariant_s TextureVariant;

/**
 * @param generalCase  Texture from which this variant is derived.
 * @param spec  Specification used to derive this variant. Ownership
 *              is NOT given to the resultant TextureVariant
 */
TextureVariant* TextureVariant_New(struct texture_s* generalCase,
    TexSource source, texturevariantspecification_t* spec);

void TextureVariant_Delete(TextureVariant* tex);

/// @return  Superior Texture of which this is a derivative.
struct texture_s* TextureVariant_GeneralCase(const TextureVariant* tex);

/// @return  Source of this variant.
TexSource TextureVariant_Source(const TextureVariant* tex);

/**
 * Change the source of this variant.
 * @param source  New TextureSource.
 */
void TextureVariant_SetSource(TextureVariant* tex, TexSource source);

/// @return  TextureVariantSpecification used to derive this variant.
texturevariantspecification_t* TextureVariant_Spec(const TextureVariant* tex);

boolean TextureVariant_IsMasked(const TextureVariant* tex);
void TextureVariant_FlagMasked(TextureVariant* tex, boolean yes);

boolean TextureVariant_IsUploaded(const TextureVariant* tex);
void TextureVariant_FlagUploaded(TextureVariant* tex, boolean yes);

boolean TextureVariant_IsPrepared(const TextureVariant* tex);

void TextureVariant_Coords(const TextureVariant* tex, float* s, float* t);
void TextureVariant_SetCoords(TextureVariant* tex, float s, float t);

DGLuint TextureVariant_GLName(const TextureVariant* tex);
void TextureVariant_SetGLName(TextureVariant* tex, DGLuint glName);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_TEXTUREVARIANT_H */
