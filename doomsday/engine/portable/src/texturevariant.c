/**\file texture.c
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

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"

#include "texture.h"
#include "texturevariant.h"

texturevariant_t* TextureVariant_New(texture_t* generalCase,
    DGLuint glName, texturevariantspecification_t* spec)
{
    assert(generalCase && spec);
    {
    texturevariant_t* tex = tex = (texturevariant_t*) malloc(sizeof(*tex));
    if(NULL == tex)
        Con_Error("TextureVariant::Construct: Failed on allocation of %lu bytes for "
            "new TextureVariant.", (unsigned long) sizeof(*tex));
    tex->_generalCase = generalCase;
    tex->_spec = spec;
    tex->_flags = 0;
    tex->_s = tex->_t = 0;
    tex->_glName = glName;
    return tex;
    }
}

void TextureVariant_Delete(texturevariant_t* tex)
{
    assert(tex);
    free(tex);
}

struct texture_s* TextureVariant_GeneralCase(const texturevariant_t* tex)
{
    assert(tex);
    return tex->_generalCase;
}

boolean TextureVariant_IsMasked(const texturevariant_t* tex)
{
    assert(tex);
    return (tex->_flags & TVF_IS_MASKED) != 0;
}

void TextureVariant_FlagMasked(texturevariant_t* tex, boolean yes)
{
    assert(tex);
    // if(yes) tex->_flags |= TVF_IS_MASKED; else tex->_flags &= ~TVF_IS_MASKED;
    tex->_flags ^= (-yes ^ tex->_flags) & TVF_IS_MASKED;
}

boolean TextureVariant_IsUploaded(const texturevariant_t* tex)
{
    assert(tex);
    return (tex->_flags & TVF_IS_UPLOADED) != 0;
}

void TextureVariant_FlagUploaded(texturevariant_t* tex, boolean yes)
{
    assert(tex);
    // if(yes) tex->_flags |= TVF_IS_UPLOADED; else tex->_flags &= ~TVF_IS_UPLOADED;
    tex->_flags ^= (-yes ^ tex->_flags) & TVF_IS_UPLOADED;
}

boolean TextureVariant_IsPrepared(const texturevariant_t* tex)
{
    return TextureVariant_IsUploaded(tex) && 0 != TextureVariant_GLName(tex);
}

void TextureVariant_Coords(const texturevariant_t* tex, float* s, float* t)
{
    assert(tex);
    if(s) *s = tex->_s;
    if(t) *t = tex->_t;
}

void TextureVariant_SetCoords(texturevariant_t* tex, float s, float t)
{
    assert(tex);
    tex->_s = s;
    tex->_t = t;
}

texturevariantspecification_t* TextureVariant_Spec(const texturevariant_t* tex)
{
    assert(tex);
    return tex->_spec;
}

DGLuint TextureVariant_GLName(const texturevariant_t* tex)
{
    assert(tex);
    return tex->_glName;
}

void TextureVariant_SetGLName(texturevariant_t* tex, DGLuint glName)
{
    assert(tex);
    tex->_glName = glName;
}
