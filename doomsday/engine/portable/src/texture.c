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
#include "pathdirectory.h"

#include "texture.h"

typedef struct texture_variantlist_node_s {
    struct texture_variantlist_node_s* next;
    texturevariant_t* variant;
} texture_variantlist_node_t;

texture_t* Texture_New(textureid_t id, struct pathdirectory_node_s* directoryNode, int index)
{
    assert(directoryNode);
    {
    texture_t* tex;
    if(NULL == (tex = (texture_t*) malloc(sizeof(*tex))))
        Con_Error("Texture::Construct: Failed on allocation of %lu bytes for "
            "new Texture.", (unsigned long) sizeof(*tex));

    tex->_id = id;
    tex->_width = tex->_height = 0;
    tex->_variants = NULL;
    tex->_index = index;
    tex->_directoryNode = directoryNode;
    memset(tex->_analyses, 0, sizeof(tex->_analyses));

    return tex;
    }
}

texture_t* Texture_NewWithDimensions(textureid_t id, struct pathdirectory_node_s* directoryNode,
    int index, int width, int height)
{
    texture_t* tex = Texture_New(id, directoryNode, index);
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
            Uri* uri = Texture_ComposeUri(tex);
            ddstring_t* path = Uri_ToString(uri);
            Con_Printf("Warning:Texture::Destruct: GLName (%i) still set for "
                "a variant of \"%s\" (id:%i). Perhaps it wasn't released?\n",
                (unsigned int) glName, Str_Text(path), (int) Texture_Id(tex));
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
    assert(tex);
    { int i;
    for(i = 0; i < TEXTURE_ANALYSIS_COUNT; ++i)
        if(tex->_analyses[i])
            free(tex->_analyses[i]);
    }
}

void Texture_Delete(texture_t* tex)
{
    assert(tex);
    destroyVariants(tex);
    destroyAnalyses(tex);
    free(tex);
}

void Texture_ClearVariants(texture_t* tex)
{
    assert(tex);
    destroyVariants(tex);
}

texturevariant_t* Texture_AddVariant(texture_t* tex, texturevariant_t* variant)
{
    assert(tex);
    {
    texture_variantlist_node_t* node;

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
}

textureid_t Texture_Id(const texture_t* tex)
{
    assert(tex);
    return tex->_id;
}

boolean Texture_IsFromIWAD(const texture_t* tex)
{
    texturenamespaceid_t texNamespace = Textures_NamespaceId(tex);
    switch(texNamespace)
    {
    case TN_FLATS:
        return !R_FlatTextureByIndex(tex->_index)->isCustom;

    case TN_TEXTURES:
        return (R_PatchCompositeTextureByIndex(tex->_index)->flags & TXDF_IWAD) != 0;

    case TN_SPRITES:
        return !R_SpriteTextureByIndex(tex->_index)->isCustom;

    case TN_PATCHES:
        return !R_PatchTextureByIndex(tex->_index)->isCustom;

    case TN_DETAILS:
    case TN_REFLECTIONS:
    case TN_MASKS:
    case TN_SYSTEM:
    case TN_MODELSKINS:
    case TN_MODELREFLECTIONSKINS:
    case TN_LIGHTMAPS:
    case TN_FLAREMAPS:
        return false; // Its definitely not.

    default:
        Con_Error("Texture::IsFromIWAD: Internal Error, invalid type %i.", (int) texNamespace);
        exit(1); // Unreachable.
    }
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
}

int Texture_TypeIndex(const texture_t* tex)
{
    assert(tex);
    return tex->_index;
}

struct pathdirectory_node_s* Texture_DirectoryNode(const texture_t* tex)
{
    assert(tex);
    return tex->_directoryNode;
}

ddstring_t* Texture_ComposePath(const texture_t* tex)
{
    struct pathdirectory_node_s* node = tex->_directoryNode;
    return PathDirectory_ComposePath(PathDirectoryNode_Directory(node), node, Str_New(), NULL, TEXTUREDIRECTORY_DELIMITER);
}

Uri* Texture_ComposeUri(const texture_t* tex)
{
    ddstring_t* path = Texture_ComposePath(tex);
    Uri* uri = Uri_NewWithPath2(Str_Text(path), RC_NULL);
    Uri_SetScheme(uri, Str_Text(DD_TextureNamespaceNameForId(Textures_NamespaceId(tex))));
    Str_Delete(path);
    return uri;
}

int Texture_IterateVariants(texture_t* tex,
    int (*callback)(texturevariant_t* variant, void* paramaters), void* paramaters)
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

void* Texture_Analysis(const texture_t* tex, texture_analysisid_t analysis)
{
    assert(tex && VALID_TEXTURE_ANALYSISID(analysis));
    return tex->_analyses[analysis];
}

void Texture_AttachAnalysis(texture_t* tex, texture_analysisid_t analysis,
    void* data)
{
    assert(tex && VALID_TEXTURE_ANALYSISID(analysis));
    if(tex->_analyses[analysis])
    {
        Uri* uri = Texture_ComposeUri(tex);
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning, image analysis #%i already present for \"%s\", will replace.\n",
            (int) analysis, Str_Text(path));
        Str_Delete(path);
        Uri_Delete(uri);
    }
    tex->_analyses[analysis] = data;
}

void* Texture_DetachAnalysis(texture_t* tex, texture_analysisid_t analysis)
{
    assert(tex && VALID_TEXTURE_ANALYSISID(analysis));
    {
    void* data = tex->_analyses[analysis];
    tex->_analyses[analysis] = NULL;
    return data;
    }
}
