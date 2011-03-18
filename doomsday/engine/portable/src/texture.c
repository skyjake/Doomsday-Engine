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

texture_t* Texture_Construct(textureid_t id, const char rawName[9],
    gltexture_type_t glType, int index)
{
    assert(rawName && rawName[0] && VALID_GLTEXTURETYPE(glType));
    {
    texture_t* tex;

    if(NULL == (tex = (texture_t*) malloc(sizeof(*tex))))
        Con_Error("Texture::Construct: Failed on allocation of %lu bytes for "
                  "new Texture.", (unsigned long) sizeof(*tex));

    tex->_id = id;
    tex->_variants = NULL;
    tex->_index = index;
    tex->_glType = glType;

    // Prepare name for hashing.
    memset(tex->_name, 0, sizeof(tex->_name));
    { int i;
    for(i = 0; *rawName && i < 8; ++i, rawName++)
        tex->_name[i] = tolower(*rawName);
    }
    return tex;
    }
}

void Texture_Destruct(texture_t* tex)
{
    assert(tex);
    {
    texture_variantlist_node_t* node = tex->_variants;
    while(node)
    {
        texture_variantlist_node_t* next = node->next;
#if _DEBUG
        DGLuint glName;
        if(0 != (glName = TextureVariant_GLName(node->variant)))
        {
            Con_Printf("Warning:Texture::Destruct: GLName (%i) still set for "
                "a variant of \"%s\" (id:%i). Perhaps it wasn't released?\n",
                (unsigned int) glName, Texture_Name(tex), (int) Texture_Id(tex));
            GL_PrintTextureVariantSpecification(TextureVariant_Spec(node->variant));
        }
#endif
        TextureVariant_Destruct(node->variant);
        free(node);
        node = next;
    }
    free(tex);
    }
}

void Texture_AddVariant(texture_t* tex, texturevariant_t* variant)
{
    assert(tex);
    {
    texture_variantlist_node_t* node;

    if(NULL == variant)
    {
#if _DEBUG
        Con_Error("Texture::AddInstance: Warning, argument variant==NULL, ignoring.");
#endif
        return;
    }

    if(NULL == (node = (texture_variantlist_node_t*) malloc(sizeof(*node))))
        Con_Error("Texture::AddInstance: failed on allocation of %lu bytes for new node.",
                  (unsigned long) sizeof(*node));

    node->variant = variant;
    node->next = tex->_variants;
    tex->_variants = node;
    }
}

textureid_t Texture_Id(const texture_t* tex)
{
    assert(tex);
    return tex->_id;
}

const char* Texture_Name(const texture_t* tex)
{
    assert(tex);
    return tex->_name;
}

boolean Texture_IsFromIWAD(const texture_t* tex)
{
    assert(tex);
    switch(tex->_glType)
    {
    case GLT_FLAT:
        return !R_FlatTextureByIndex(tex->_index)->isCustom;

    case GLT_PATCHCOMPOSITE:
        return (R_PatchCompositeTextureByIndex(tex->_index)->flags & TXDF_IWAD) != 0;

    case GLT_SPRITE:
        return !R_SpriteTextureByIndex(tex->_index)->isCustom;

    case GLT_PATCH:
        return !R_PatchTextureByIndex(tex->_index)->isCustom;

    case GLT_DETAIL:
    case GLT_SHINY:
    case GLT_MASK:
    case GLT_SYSTEM:
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN:
    case GLT_LIGHTMAP:
    case GLT_FLARE:
        return false; // Its definitely not.

    default:
        Con_Error("Texture::IsFromIWAD: Internal Error, invalid type %i.",
                  (int) tex->_glType);
    }

    return false; // Unreachable.
}

int Texture_Width(const texture_t* tex)
{
    assert(tex);
    switch(tex->_glType)
    {
    case GLT_FLAT:
        return 64; /// \fixme not all flats are 64x64

    case GLT_PATCHCOMPOSITE:
        return R_PatchCompositeTextureByIndex(tex->_index)->width;

    case GLT_SPRITE:
        return R_SpriteTextureByIndex(tex->_index)->width;

    case GLT_PATCH:
        return R_PatchTextureByIndex(tex->_index)->width;

    case GLT_DETAIL:
        return 128;

    case GLT_SHINY:
        return 128; // Could be used for something useful.

    case GLT_MASK:
        return maskTextures[tex->_index]->width;

    case GLT_SYSTEM: /// \fixme Do not assume!
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN:
    case GLT_LIGHTMAP:
    case GLT_FLARE:
        return 64;

    default:
        Con_Error("Texture::Width: Internal error, invalid type %i.", (int) tex->_glType);
        return 0; // Unreachable.
    }
}

int Texture_Height(const texture_t* tex)
{
    assert(tex);
    switch(tex->_glType)
    {
    case GLT_FLAT:
        return 64; /// \fixme not all flats are 64x64

    case GLT_PATCHCOMPOSITE:
        return R_PatchCompositeTextureByIndex(tex->_index)->height;

    case GLT_SPRITE:
        return R_SpriteTextureByIndex(tex->_index)->height;

    case GLT_PATCH:
        return R_PatchTextureByIndex(tex->_index)->height;

    case GLT_DETAIL:
        return 128;

    case GLT_SHINY:
        return 128; // Could be used for something useful.

    case GLT_MASK:
        return maskTextures[tex->_index]->height;

    case GLT_SYSTEM: /// \fixme Do not assume!
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN:
    case GLT_LIGHTMAP:
    case GLT_FLARE:
        return 64;

    default:
        Con_Error("Texture::Height: Internal error, invalid type %i.", (int) tex->_glType);
        return 0; // Unreachable.
    }
}

void Texture_Dimensions(const texture_t* tex, int* width, int* height)
{
    if(width)   *width = Texture_Width(tex);
    if(height) *height = Texture_Height(tex);
}

int Texture_TypeIndex(const texture_t* tex)
{
    assert(tex);
    return tex->_index;
}

gltexture_type_t Texture_GLType(const texture_t* tex)
{
    assert(tex);
    return tex->_glType;
}

int Texture_IterateVariants(texture_t* tex,
    int (*callback)(texturevariant_t* instance, void* paramaters), void* paramaters)
{
    assert(tex);
    {
    int result = 0;
    if(callback)
    {
        texture_variantlist_node_t* node = tex->_variants;
        while(node)
        {
            texture_variantlist_node_t* next = node->next;
            if(0 != (result = callback(node->variant, paramaters)))
                break;
            node = next;
        }
    }
    return result;
    }
}
