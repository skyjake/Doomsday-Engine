/**\file texturevariant.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG_REFRESH_TEXTUREVARIANT_H
#define LIBDENG_REFRESH_TEXTUREVARIANT_H

#include "texturevariantspecification.h"

struct texture_s;

/**
 * @defgroup textureVariantFlags  Texture Variant Flags
 * @{
 */
#define TVF_IS_MASKED           0x1 /// Texture contains alpha.
#define TVF_IS_UPLOADED         0x2 /// Texture has been uploaded to GL.
/**@}*/

struct texturevariant_s; // The texturevariant instance (opaque).
typedef struct texturevariant_s texturevariant_t;

/**
 * @param generalCase  Texture from which this variant is derived.
 * @param spec  Specification used to derive this variant. Ownership of
 *      specifcation is NOT given to the resultant TextureVariant
 */
texturevariant_t* TextureVariant_New(struct texture_s* generalCase,
    texturevariantspecification_t* spec);

void TextureVariant_Delete(texturevariant_t* tex);

/// @return  Superior Texture of which this is a derivative.
struct texture_s* TextureVariant_GeneralCase(const texturevariant_t* tex);

/// @return  TextureVariantSpecification used to derive this variant.
texturevariantspecification_t* TextureVariant_Spec(const texturevariant_t* tex);

boolean TextureVariant_IsMasked(const texturevariant_t* tex);
void TextureVariant_FlagMasked(texturevariant_t* tex, boolean yes);

boolean TextureVariant_IsUploaded(const texturevariant_t* tex);
void TextureVariant_FlagUploaded(texturevariant_t* tex, boolean yes);

boolean TextureVariant_IsPrepared(const texturevariant_t* tex);

void TextureVariant_Coords(const texturevariant_t* tex, float* s, float* t);
void TextureVariant_SetCoords(texturevariant_t* tex, float s, float t);

DGLuint TextureVariant_GLName(const texturevariant_t* tex);
void TextureVariant_SetGLName(texturevariant_t* tex, DGLuint glName);

#endif /* LIBDENG_REFRESH_TEXTUREVARIANT_H */
