/**
 * @file texturevariant.c
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

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"

#include "texture.h"
#include "texturevariant.h"

struct texturevariant_s {
    /// Superior Texture of which this is a derivative.
    struct texture_s* generalCase;

    /// Source of this texture.
    TexSource source;

    /// @see textureVariantFlags
    int flags;

    /// Name of the associated GL texture object.
    DGLuint glName;

    /// Prepared coordinates for the bottom right of the texture minus border.
    float s, t;

    /// Specification used to derive this variant.
    texturevariantspecification_t* spec;
};

TextureVariant* TextureVariant_New(Texture* generalCase, TexSource source,
    texturevariantspecification_t* spec)
{
    TextureVariant* tex;

    if(!generalCase)
        Con_Error("TextureVariant::New: Attempted with invalid generalCase reference (=NULL).");
    if(!spec)
        Con_Error("TextureVariant::New: Attempted with invalid spec reference (=NULL).");

    tex = (TextureVariant*) malloc(sizeof(*tex));
    if(!tex)
        Con_Error("TextureVariant::New: Failed on allocation of %lu bytes for new TextureVariant.",
                  (unsigned long) sizeof(*tex));

    tex->generalCase = generalCase;
    tex->source = source;
    tex->spec = spec;
    tex->flags = 0;
    tex->s = tex->t = 0;
    tex->glName = 0;
    return tex;
}

void TextureVariant_Delete(TextureVariant* tex)
{
    assert(tex);
    free(tex);
}

struct texture_s* TextureVariant_GeneralCase(const TextureVariant* tex)
{
    assert(tex);
    return tex->generalCase;
}

TexSource TextureVariant_Source(const TextureVariant* tex)
{
    assert(tex);
    return tex->source;
}

void TextureVariant_SetSource(TextureVariant* tex, TexSource source)
{
    assert(tex);
    tex->source = source;
}

boolean TextureVariant_IsMasked(const TextureVariant* tex)
{
    assert(tex);
    return (tex->flags & TVF_IS_MASKED) != 0;
}

void TextureVariant_FlagMasked(TextureVariant* tex, boolean yes)
{
    assert(tex);
    // if(yes) tex->flags |= TVF_IS_MASKED; else tex->flags &= ~TVF_IS_MASKED;
    tex->flags ^= (-yes ^ tex->flags) & TVF_IS_MASKED;
}

boolean TextureVariant_IsUploaded(const TextureVariant* tex)
{
    assert(tex);
    return (tex->flags & TVF_IS_UPLOADED) != 0;
}

void TextureVariant_FlagUploaded(TextureVariant* tex, boolean yes)
{
    assert(tex);
    // if(yes) tex->flags |= TVF_IS_UPLOADED; else tex->flags &= ~TVF_IS_UPLOADED;
    tex->flags ^= (-yes ^ tex->flags) & TVF_IS_UPLOADED;
}

boolean TextureVariant_IsPrepared(const TextureVariant* tex)
{
    return TextureVariant_IsUploaded(tex) && 0 != TextureVariant_GLName(tex);
}

void TextureVariant_Coords(const TextureVariant* tex, float* s, float* t)
{
    assert(tex);
    if(s) *s = tex->s;
    if(t) *t = tex->t;
}

void TextureVariant_SetCoords(TextureVariant* tex, float s, float t)
{
    assert(tex);
    tex->s = s;
    tex->t = t;
}

texturevariantspecification_t* TextureVariant_Spec(const TextureVariant* tex)
{
    assert(tex);
    return tex->spec;
}

DGLuint TextureVariant_GLName(const TextureVariant* tex)
{
    assert(tex);
    return tex->glName;
}

void TextureVariant_SetGLName(TextureVariant* tex, DGLuint glName)
{
    assert(tex);
    tex->glName = glName;
}
