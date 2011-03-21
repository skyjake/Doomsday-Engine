/**\file p_material.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2008-2011 Daniel Swanson <danij@dengine.net>
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
#include "de_play.h"
#include "de_refresh.h"
#include "m_misc.h"

#include "texture.h"
#include "materialvariant.h"
#include "p_material.h"

typedef struct material_variantlist_node_s {
    struct material_variantlist_node_s* next;
    materialvariant_t* variant;
} material_variantlist_node_t;

static void destroyVariants(material_t* mat)
{
    assert(mat);
    while(mat->_variants)
    {
        material_variantlist_node_t* next = mat->_variants->next;
        MaterialVariant_Destruct(mat->_variants->variant);
        free(mat->_variants);
        mat->_variants = next;
    }
}

static int variantTicker(materialvariant_t* variant, timespan_t time)
{
    assert(variant);
    {
    const material_t* mat = MaterialVariant_GeneralCase(variant);
    const ded_material_t* def = Material_Definition(mat);
    uint i;

    if(!def) // Its a system generated material.
        return 0; // Continue iteration.

    // Update layers
    for(i = 0; i < mat->numLayers; ++i)
    {
        const ded_material_layer_t* lDef = &def->layers[i];
        const ded_material_layer_stage_t* lsDef, *lsDefNext;
        materialvariant_layer_t* layer = &variant->layers[i];
        float inter;

        if(!(lDef->stageCount.num > 1))
            continue;

        if(layer->tics-- <= 0)
        {
            // Advance to next stage.
            if(++layer->stage == lDef->stageCount.num)
            {
                // Loop back to the beginning.
                layer->stage = 0;
            }

            lsDef = &lDef->stages[layer->stage];
            if(lsDef->variance != 0)
                layer->tics = lsDef->tics * (1 - lsDef->variance * RNG_RandFloat());
            else
                layer->tics = lsDef->tics;

            inter = 0;
        }
        else
        {
            lsDef = &lDef->stages[layer->stage];
            inter = 1.0f - (layer->tics - frameTimePos) / (float) lsDef->tics;
        }

        {const texture_t* glTex;
        if((glTex = GL_TextureByUri(lsDef->texture)))
        {
            layer->tex = Texture_Id(glTex);
            variant->inter = inter;
        }
        else
        {
            /// @fixme Should reset this to the non-stage animated texture here.
            //layer->tex = 0;
            //mat->inter = 0;
        }}

        if(inter == 0)
        {
            layer->glow = lsDef->glow;
            layer->texOrigin[0] = lsDef->texOrigin[0];
            layer->texOrigin[1] = lsDef->texOrigin[1];
            continue;
        }
        lsDefNext = &lDef->stages[(layer->stage+1) % lDef->stageCount.num];

        layer->glow = lsDefNext->glow * inter + lsDef->glow * (1 - inter);

        /// @todo Implement a more useful method of interpolation (but what? what do we want/need here?).
        layer->texOrigin[0] = lsDefNext->texOrigin[0] * inter + lsDef->texOrigin[0] * (1 - inter);
        layer->texOrigin[1] = lsDefNext->texOrigin[1] * inter + lsDef->texOrigin[1] * (1 - inter);
    }

    return 0; // Continue iteration.
    }
}

void Material_Initialize(material_t* mat)
{
    assert(mat);
    memset(mat, 0, sizeof(material_t));
    mat->header.type = DMU_MATERIAL;
    mat->_envClass = MEC_UNKNOWN;
}

void Material_Ticker(material_t* mat, timespan_t time)
{
    assert(mat);
    {material_variantlist_node_t* node;
    for(node = mat->_variants; node; node = node->next)
    {
        variantTicker(node->variant, time);
    }}
}

ded_material_t* Material_Definition(const material_t* mat)
{
    assert(mat);
    return mat->_def;
}

void Material_Dimensions(const material_t* mat, int* width, int* height)
{
    assert(mat);
    if(width) *width = mat->_width;
    if(height) *height = mat->_height;
}

int Material_Width(const material_t* mat)
{
    assert(mat);
    return mat->_width;
}

int Material_Height(const material_t* mat)
{
    assert(mat);
    return mat->_height;
}

short Material_Flags(const material_t* mat)
{
    assert(mat);
    return mat->_flags;
}

boolean Material_IsCustom(const material_t* mat)
{
    assert(mat);
    return mat->_isCustom;
}

boolean Material_IsGroupAnimated(const material_t* mat)
{
    assert(mat);
    return mat->_inAnimGroup;
}

boolean Material_IsSkyMasked(const material_t* mat)
{
    assert(mat);
    return 0 != (mat->_flags & MATF_SKYMASK);
}

boolean Material_IsDrawable(const material_t* mat)
{
    assert(mat);
    return 0 == (mat->_flags & MATF_NO_DRAW);
}

int Material_LayerCount(const material_t* mat)
{
    assert(mat);
    return mat->numLayers;
}

void Material_SetGroupAnimated(material_t* mat, boolean yes)
{
    assert(mat);
    mat->_inAnimGroup = yes;
}

uint Material_BindId(const material_t* mat)
{
    assert(mat);
    return mat->_bindId;
}

void Material_SetBindId(material_t* mat, uint bindId)
{
    assert(mat);
    mat->_bindId = bindId;
}

material_env_class_t Material_EnvClass(const material_t* mat)
{
    assert(mat);
    if(!Material_IsDrawable(mat))
        return MEC_UNKNOWN;
    return mat->_envClass;
}

void Material_SetEnvClass(material_t* mat, material_env_class_t envClass)
{
    assert(mat);
    mat->_envClass = envClass;
}

materialvariant_t* Material_AddVariant(material_t* mat, materialvariant_t* variant)
{
    assert(mat);
    {
    material_variantlist_node_t* node;

    if(NULL == variant)
    {
#if _DEBUG
        Con_Error("Material::AddVariant: Warning, argument variant==NULL, ignoring.");
#endif
        return variant;
    }

    if(NULL == (node = (material_variantlist_node_t*) malloc(sizeof(*node))))
        Con_Error("Material::AddVariant: Failed on allocation of %lu bytes for new node.",
                  (unsigned long) sizeof(*node));

    node->variant = variant;
    node->next = mat->_variants;
    mat->_variants = node;
    return variant;
    }
}

int Material_IterateVariants(material_t* mat,
    int (*callback)(materialvariant_t* variant, void* paramaters), void* paramaters)
{
    assert(mat);
    {
    int result = 0;
    if(callback)
    {
        material_variantlist_node_t* node = mat->_variants;
        while(node)
        {
            material_variantlist_node_t* next = node->next;
            if(0 != (result = callback(node->variant, paramaters)))
                break;
            node = next;
        }
    }
    return result;
    }
}

void Material_DestroyVariants(material_t* mat)
{
    destroyVariants(mat);
}
