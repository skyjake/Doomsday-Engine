/**\file texture.c
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

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"

#include "texture.h"
#include "texturevariant.h"

struct texturevariant_s {
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
};

TextureVariant* TextureVariant_New(texture_t* generalCase,
    texturevariantspecification_t* spec)
{
    TextureVariant* tex;

    if(!generalCase)
        Con_Error("TextureVariant::New: Attempted with invalid generalCase reference (=NULL).");
    if(!spec)
        Con_Error("TextureVariant::New: Attempted with invalid spec reference (=NULL).");

    tex = (TextureVariant*) malloc(sizeof(*tex));
    if(!tex)
        Con_Error("TextureVariant::Construct: Failed on allocation of %lu bytes for "
            "new TextureVariant.", (unsigned long) sizeof(*tex));

    tex->_generalCase = generalCase;
    tex->_spec = spec;
    tex->_flags = 0;
    tex->_s = tex->_t = 0;
    tex->_glName = 0;
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
    return tex->_generalCase;
}

boolean TextureVariant_IsMasked(const TextureVariant* tex)
{
    assert(tex);
    return (tex->_flags & TVF_IS_MASKED) != 0;
}

void TextureVariant_FlagMasked(TextureVariant* tex, boolean yes)
{
    assert(tex);
    // if(yes) tex->_flags |= TVF_IS_MASKED; else tex->_flags &= ~TVF_IS_MASKED;
    tex->_flags ^= (-yes ^ tex->_flags) & TVF_IS_MASKED;
}

boolean TextureVariant_IsUploaded(const TextureVariant* tex)
{
    assert(tex);
    return (tex->_flags & TVF_IS_UPLOADED) != 0;
}

void TextureVariant_FlagUploaded(TextureVariant* tex, boolean yes)
{
    assert(tex);
    // if(yes) tex->_flags |= TVF_IS_UPLOADED; else tex->_flags &= ~TVF_IS_UPLOADED;
    tex->_flags ^= (-yes ^ tex->_flags) & TVF_IS_UPLOADED;
}

boolean TextureVariant_IsPrepared(const TextureVariant* tex)
{
    return TextureVariant_IsUploaded(tex) && 0 != TextureVariant_GLName(tex);
}

void TextureVariant_Coords(const TextureVariant* tex, float* s, float* t)
{
    assert(tex);
    if(s) *s = tex->_s;
    if(t) *t = tex->_t;
}

void TextureVariant_SetCoords(TextureVariant* tex, float s, float t)
{
    assert(tex);
    tex->_s = s;
    tex->_t = t;
}

texturevariantspecification_t* TextureVariant_Spec(const TextureVariant* tex)
{
    assert(tex);
    return tex->_spec;
}

DGLuint TextureVariant_GLName(const TextureVariant* tex)
{
    assert(tex);
    return tex->_glName;
}

void TextureVariant_SetGLName(TextureVariant* tex, DGLuint glName)
{
    assert(tex);
    tex->_glName = glName;
}
