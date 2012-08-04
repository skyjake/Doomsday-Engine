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

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
namespace de {

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
    TextureVariant(struct texture_s& generalCase, texturevariantspecification_t& spec,
                   TexSource source=TEXS_NONE);

    /// @return  Superior Texture of which this is a derivative.
    struct texture_s* generalCase() const { return texture; }

    /// @return  Source of this variant.
    TexSource source() const { return texSource; }

    /**
     * Change the source of this variant.
     * @param newSource  New TextureSource.
     */
    void setSource(TexSource newSource);

    /// @return  TextureVariantSpecification used to derive this variant.
    texturevariantspecification_t* spec() const { return varSpec; }

    bool isMasked() const { return !!(flags & Masked); }

    void flagMasked(bool yes);

    bool isUploaded() const { return !!(flags & Uploaded); }

    void flagUploaded(bool yes);

    bool isPrepared() const;

    void coords(float* s, float* t) const;
    void setCoords(float s, float t);

    DGLuint glName() const { return glTexName; }

    void setGLName(unsigned int glName);

private:
    /// Superior Texture of which this is a derivative.
    struct texture_s* texture;

    /// Source of this texture.
    TexSource texSource;

    Flags flags;

    /// Name of the associated GL texture object.
    unsigned int glTexName;

    /// Prepared coordinates for the bottom right of the texture minus border.
    float s, t;

    /// Specification used to derive this variant.
    texturevariantspecification_t* varSpec;
};

} // namespace de

extern "C" {
#endif

/**
 * C wrapper API:
 */

struct texturevariant_s; // The texturevariant instance (opaque).
typedef struct texturevariant_s TextureVariant;

TextureVariant* TextureVariant_New(struct texture_s* generalCase,
    texturevariantspecification_t* spec, TexSource source);

void TextureVariant_Delete(TextureVariant* tex);

struct texture_s* TextureVariant_GeneralCase(const TextureVariant* tex);

TexSource TextureVariant_Source(const TextureVariant* tex);

void TextureVariant_SetSource(TextureVariant* tex, TexSource source);

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
