/**\file texture.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"

#include "gl_texmanager.h"
#include "texturevariant.h"

#include "texture.h"

typedef struct texture_variantlist_node_s {
    struct texture_variantlist_node_s* next;
    texturevariant_t* variant;
} texture_variantlist_node_t;

texture_t* Texture_New(int flags, textureid_t bindId, void* userData)
{
    texture_t* tex = (texture_t*)malloc(sizeof *tex);
    if(!tex)
        Con_Error("Texture::Construct: Failed on allocation of %lu bytes for new Texture.", (unsigned long) sizeof *tex);

    tex->_flags = flags;
    tex->_width = tex->_height = 0;
    tex->_variants = NULL;
    tex->_primaryBind = bindId;
    tex->_userData = userData;
    memset(tex->_analyses, 0, sizeof(tex->_analyses));

    return tex;
}

texture_t* Texture_NewWithDimensions(int flags, textureid_t bindId,
    int width, int height, void* userData)
{
    texture_t* tex = Texture_New(flags, bindId, userData);
    Texture_SetDimensions(tex, width, height);
    return tex;
}

static void destroyVariants(texture_t* tex)
{
    assert(tex);
    while(tex->_variants)
    {
        texturevariant_t* variant = tex->_variants->variant;
        texture_variantlist_node_t* next = tex->_variants->next;
#if _DEBUG
        DGLuint glName = TextureVariant_GLName(variant);
        if(glName)
        {
            Uri* uri = Textures_ComposeUri(Textures_Id(tex));
            ddstring_t* path = Uri_ToString(uri);
            Con_Printf("Warning:Texture::Destruct: GLName (%i) still set for "
                "a variant of \"%s\" (id:%i). Perhaps it wasn't released?\n",
                (unsigned int) glName, Str_Text(path), (int) Textures_Id(tex));
            GL_PrintTextureVariantSpecification(TextureVariant_Spec(variant));
            Str_Delete(path);
            Uri_Delete(uri);
        }
#endif
        TextureVariant_Delete(variant);
        free(tex->_variants);
        tex->_variants = next;
    }
}

static void destroyAnalyses(texture_t* tex)
{
    int i;
    assert(tex);
    for(i = 0; i < TEXTURE_ANALYSIS_COUNT; ++i)
    {
        if(tex->_analyses[i]) free(tex->_analyses[i]);
    }
}

void Texture_Delete(texture_t* tex)
{
    assert(tex);
    destroyVariants(tex);
    destroyAnalyses(tex);
    free(tex);
}

textureid_t Texture_PrimaryBind(const texture_t* tex)
{
    assert(tex);
    return tex->_primaryBind;
}

void Texture_SetPrimaryBind(texture_t* tex, textureid_t bindId)
{
    assert(tex);
    tex->_primaryBind = bindId;
}

void Texture_AttachUserData(texture_t* tex, void* userData)
{
    assert(tex);
#if _DEBUG
    if(tex->_userData)
    {
        Con_Message("Warning:Texture::AttachUserData: User data is already present for [%p id:%i], it will be replaced.\n", (void*)tex, Textures_Id(tex));
    }
#endif
    tex->_userData = userData;
}

void* Texture_DetachUserData(texture_t* tex)
{
    void* data;
    assert(tex);
    data = tex->_userData;
    tex->_userData = NULL;
    return data;
}

void* Texture_UserData(const texture_t* tex)
{
    assert(tex);
    return tex->_userData;
}

void Texture_ClearVariants(texture_t* tex)
{
    assert(tex);
    destroyVariants(tex);
}

texturevariant_t* Texture_AddVariant(texture_t* tex, texturevariant_t* variant)
{
    texture_variantlist_node_t* node;
    assert(tex);

    if(!variant)
    {
#if _DEBUG
        Con_Message("Warning:Texture::AddVariant: Argument variant==NULL, ignoring.");
#endif
        return variant;
    }

    node = (texture_variantlist_node_t*) malloc(sizeof *node);
    if(!node)
        Con_Error("Texture::AddVariant: Failed on allocation of %lu bytes for new node.", (unsigned long) sizeof *node);

    node->variant = variant;
    node->next = tex->_variants;
    tex->_variants = node;
    return variant;
}

boolean Texture_IsCustom(const texture_t* tex)
{
    assert(tex);
    return (tex->_flags & TXF_CUSTOM) != 0;
}

int Texture_Flags(const texture_t* tex)
{
    assert(tex);
    return tex->_flags;
}

void Texture_SetFlags(texture_t* tex, int flags)
{
    assert(tex);
    tex->_flags = flags;
    /// \fixme Update any Materials (and thus Surfaces) which reference this.
}

int Texture_Width(const texture_t* tex)
{
    assert(tex);
    return tex->_width;
}

void Texture_SetWidth(texture_t* tex, int width)
{
    assert(tex);
    tex->_width = width;
    /// \fixme Update any Materials (and thus Surfaces) which reference this.
}

int Texture_Height(const texture_t* tex)
{
    assert(tex);
    return tex->_height;
}

void Texture_SetHeight(texture_t* tex, int height)
{
    assert(tex);
    tex->_height = height;
    /// \fixme Update any Materials (and thus Surfaces) which reference this.
}

void Texture_Dimensions(const texture_t* tex, int* width, int* height)
{
    if(width)   *width = Texture_Width(tex);
    if(height) *height = Texture_Height(tex);
}

void Texture_SetDimensions(texture_t* tex, int width, int height)
{
    assert(tex);
    tex->_width  = width;
    tex->_height = height;
    /// \fixme Update any Materials (and thus Surfaces) which reference this.
}

int Texture_IterateVariants(texture_t* tex,
    int (*callback)(texturevariant_t* variant, void* paramaters), void* paramaters)
{
    int result = 0;
    assert(tex);

    if(callback)
    {
        texture_variantlist_node_t* node = tex->_variants;
        while(node)
        {
            texture_variantlist_node_t* next = node->next;
            result = callback(node->variant, paramaters);
            if(result) break;
            node = next;
        }
    }
    return result;
}

void* Texture_Analysis(const texture_t* tex, texture_analysisid_t analysis)
{
    assert(tex && VALID_TEXTURE_ANALYSISID(analysis));
    return tex->_analyses[analysis];
}

void Texture_AttachAnalysis(texture_t* tex, texture_analysisid_t analysis,
    void* data)
{
    assert(tex && VALID_TEXTURE_ANALYSISID(analysis));
#if _DEBUG
    if(tex->_analyses[analysis])
    {
        Uri* uri = Textures_ComposeUri(Textures_Id(tex));
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning: Image analysis #%i already present for \"%s\", will replace.\n",
            (int) analysis, Str_Text(path));
        Str_Delete(path);
        Uri_Delete(uri);
    }
#endif
    tex->_analyses[analysis] = data;
}

void* Texture_DetachAnalysis(texture_t* tex, texture_analysisid_t analysis)
{
    void* data;
    assert(tex && VALID_TEXTURE_ANALYSISID(analysis));

    data = tex->_analyses[analysis];
    tex->_analyses[analysis] = NULL;
    return data;
}
