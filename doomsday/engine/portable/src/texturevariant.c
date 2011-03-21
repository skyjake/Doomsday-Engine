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

texturevariant_t* TextureVariant_Construct(texture_t* generalCase,
    texturevariantspecification_t* spec)
{
    assert(generalCase && spec);
    {
    texturevariant_t* tex = tex = (texturevariant_t*) malloc(sizeof(*tex));
    if(NULL == tex)
        Con_Error("TextureVariant::Construct: Failed on allocation of %lu bytes for "
                  "new TextureVariant.", sizeof(*tex));
    tex->_generalCase = generalCase;
    tex->_spec = spec;
    tex->_isMasked = false;
    tex->_glName = 0;
    tex->_s = tex->_t = 0;
    memset(tex->_analyses, 0, sizeof(tex->_analyses));
    return tex;
    }
}

void TextureVariant_Destruct(texturevariant_t* tex)
{
    assert(tex);
    { int i;
    for(i = 0; i < TEXTUREVARIANT_ANALYSIS_COUNT; ++i)
        if(tex->_analyses[i])
            free(tex->_analyses[i]);
    }
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
    return tex->_isMasked;
}

void TextureVariant_SetMasked(texturevariant_t* tex, boolean yes)
{
    assert(tex);
    tex->_isMasked = yes;
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

void* TextureVariant_Analysis(const texturevariant_t* tex,
    texturevariant_analysisid_t analysis)
{
    assert(tex && VALID_TEXTUREVARIANT_ANALYSISID(analysis));
    return tex->_analyses[analysis];
}

void TextureVariant_AddAnalysis(texturevariant_t* tex, texturevariant_analysisid_t analysis,
    void* data)
{
    assert(tex && VALID_TEXTUREVARIANT_ANALYSISID(analysis));
    if(NULL != tex->_analyses[analysis])
        Con_Message("Warning, image analysis #%i already present for \"%s\" (%i), replacing.\n",
                    (int) analysis, Texture_Name(TextureVariant_GeneralCase(tex)), tex->_glName);
    tex->_analyses[analysis] = data;
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
