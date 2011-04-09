/**\file material.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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
#include "material.h"

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
        MaterialVariant_Ticker(node->variant, time);
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

boolean Material_HasGlow(material_t* mat)
{
    assert(mat);
    {
    material_snapshot_t ms;
    /// \fixme We should not need to prepare to determine this.
    Materials_Prepare(&ms, mat, true,
        Materials_VariantSpecificationForContext(MC_MAPSURFACE, 0, 0, 0, 0,
            GL_REPEAT, GL_REPEAT, -1, true, true, false, false));
    return (ms.glowing > .0001f);
    }
}

boolean Material_HasDecorations(material_t* mat)
{
    assert(mat);
    /// \fixme We should not need to prepare to determine this.
    return NULL != Materials_Decoration(Materials_ToMaterialNum(mat));
}

int Material_LayerCount(const material_t* mat)
{
    assert(mat);
    return 1;
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
