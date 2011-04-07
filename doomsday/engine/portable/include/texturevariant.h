/**\file texturevariant.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_GL_TEXTUREVARIANT_H
#define LIBDENG_GL_TEXTUREVARIANT_H

#include "texturevariantspecification.h"

struct texture_s;

typedef enum {
    TEXTUREVARIANT_ANALYSIS_FIRST = 0,
    TA_SPRITE_AUTOLIGHT = TEXTUREVARIANT_ANALYSIS_FIRST,
    TA_MAP_AMBIENTLIGHT,
    TA_SKY_SPHEREFADECOLOR,
    TEXTUREVARIANT_ANALYSIS_COUNT
} texturevariant_analysisid_t;

#define VALID_TEXTUREVARIANT_ANALYSISID(id) (\
    (id) >= TEXTUREVARIANT_ANALYSIS_FIRST && (id) < TEXTUREVARIANT_ANALYSIS_COUNT)

/**
 * @defgroup textureVariantFlags  Texture Variant Flags
 * @{
 */
#define TVF_IS_MASKED           0x1 /// Texture contains alpha.
#define TVF_IS_UPLOADED         0x2 /// Texture has been uploaded to GL.
/**@}*/

typedef struct texturevariant_s {
    /// Table of analyses object ptrs, used for various purposes depending
    /// on the variant specification.
    void* _analyses[TEXTUREVARIANT_ANALYSIS_COUNT];

    /// Superior Texture of which this is a derivative.
    struct texture_s* _generalCase;

    /// @see textureVariantFlags
    int _flags;

    /// Name of the associated GL texture object.
    DGLuint _glName;

    /// Prepared coordinates for the bottom right of the texture minus border.
    float _s, _t;

    /// Specification used to derive this variant.
    texturevariantspecification_t* _spec;
} texturevariant_t;

/**
 * @param generalCase  Texture from which this variant is derived.
 * @param glName  GL-name of the associated texture object.
 * @param spec  Specification used to derive this variant. Ownership of
 *      specifcation is NOT given to the resultant TextureVariant
 */
texturevariant_t* TextureVariant_Construct(struct texture_s* generalCase,
    DGLuint glName, texturevariantspecification_t* spec);

void TextureVariant_Destruct(texturevariant_t* tex);

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

/// @return  Associated data for the specified analysis identifier.
void* TextureVariant_Analysis(const texturevariant_t* tex,
    texturevariant_analysisid_t analysis);

/**
 * Attach new analysis data to the variant. If data is already present
 * it will be replaced.
 *
 * @param analysis  Identifier of the data being attached.
 * @param data  Data to be attached. Ownership is given to the variant.
 */
void TextureVariant_AddAnalysis(texturevariant_t* tex,
    texturevariant_analysisid_t analysis, void* data);

#endif /* LIBDENG_GL_TEXTUREVARIANT_H */
