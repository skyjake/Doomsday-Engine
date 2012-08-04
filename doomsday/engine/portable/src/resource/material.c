/**
 * @file material.c
 * Logical material. @ingroup resource
 *
 * @authors Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
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
#include "de_play.h"
#include "de_refresh.h"
#include "m_misc.h"

#include "texture.h"
#include "s_environ.h"
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
        MaterialVariant_Delete(mat->_variants->variant);
        free(mat->_variants);
        mat->_variants = next;
    }
    mat->_prepared = 0;
}

void Material_Initialize(material_t* mat)
{
    assert(mat);
    memset(mat, 0, sizeof *mat);
    mat->header.type = DMU_MATERIAL;
    mat->_envClass = MEC_UNKNOWN;
    mat->_size = Size2_New();
}

void Material_Destroy(material_t* mat)
{
    assert(mat);
    Material_DestroyVariants(mat);
    Size2_Delete(mat->_size);
    mat->_size = NULL;
}

void Material_Ticker(material_t* mat, timespan_t time)
{
    material_variantlist_node_t* node;
    assert(mat);

    for(node = mat->_variants; node; node = node->next)
    {
        MaterialVariant_Ticker(node->variant, time);
    }
}

ded_material_t* Material_Definition(const material_t* mat)
{
    assert(mat);
    return mat->_def;
}

void Material_SetDefinition(material_t* mat, struct ded_material_s* def)
{
    Size2Raw size;
    assert(mat);

    if(mat->_def != def)
    {
        mat->_def = def;

        // Textures are updated automatically at prepare-time, so just clear them.
        Material_SetDetailTexture(mat, NULL);
        Material_SetShinyTexture(mat, NULL);
        Material_SetShinyMaskTexture(mat, NULL);
    }

    if(!mat->_def) return;

    mat->_flags = mat->_def->flags;

    size.width  = def->width;
    size.height = def->height;
    Material_SetSize(mat, &size);

    Material_SetEnvironmentClass(mat, S_MaterialEnvClassForUri(def->uri));

    // Update custom status.
    /// @todo This should take into account the whole definition, not just whether
    ///       the primary layer's first texture is custom or not.
    mat->_isCustom = false;
    if(def->layers[0].stageCount.num > 0 && def->layers[0].stages[0].texture)
    {
        textureid_t texId = Textures_ResolveUri2(def->layers[0].stages[0].texture, true/*quiet please*/);
        if(texId != NOTEXTUREID)
        {
            mat->_isCustom = Texture_IsCustom(Textures_ToTexture(texId));
        }
    }
}

const Size2* Material_Size(const material_t* mat)
{
    assert(mat);
    return mat->_size;
}

void Material_SetSize(material_t* mat, const Size2Raw* newSize)
{
    Size2* size;
    assert(mat);
    if(!newSize) return;

    size = Size2_NewFromRaw(newSize);
    if(!Size2_Equality(mat->_size, size))
    {
        Size2_SetWidthHeight(mat->_size, newSize->width, newSize->height);
        R_UpdateMapSurfacesOnMaterialChange(mat);
    }
    Size2_Delete(size);
}

int Material_Width(const material_t* mat)
{
    assert(mat);
    return Size2_Width(mat->_size);
}

void Material_SetWidth(material_t* mat, int width)
{
    assert(mat);
    if(Size2_Width(mat->_size) == width) return;
    Size2_SetWidth(mat->_size, width);
    R_UpdateMapSurfacesOnMaterialChange(mat);
}

int Material_Height(const material_t* mat)
{
    assert(mat);
    return Size2_Height(mat->_size);
}

void Material_SetHeight(material_t* mat, int height)
{
    assert(mat);
    if(Size2_Height(mat->_size) == height) return;
    Size2_SetHeight(mat->_size, height);
    R_UpdateMapSurfacesOnMaterialChange(mat);
}

short Material_Flags(const material_t* mat)
{
    assert(mat);
    return mat->_flags;
}

void Material_SetFlags(material_t* mat, short flags)
{
    assert(mat);
    mat->_flags = flags;
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
    if(novideo) return false;

    {
    /// @todo We should not need to prepare to determine this.
    const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
        MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
    const materialsnapshot_t* ms = Materials_Prepare(mat, spec, true);

    return (ms->glowing > .0001f);
    }
}

boolean Material_HasTranslation(const material_t* mat)
{
    assert(mat);
    /// \todo Separate meanings.
    return Material_IsGroupAnimated(mat);
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

byte Material_Prepared(const material_t* mat)
{
    assert(mat);
    return mat->_prepared;
}

void Material_SetPrepared(material_t* mat, byte state)
{
    assert(mat && state <= 2);
    mat->_prepared = state;
}

materialid_t Material_PrimaryBind(const material_t* mat)
{
    assert(mat);
    return mat->_primaryBind;
}

void Material_SetPrimaryBind(material_t* mat, materialid_t bindId)
{
    assert(mat);
    mat->_primaryBind = bindId;
}

material_env_class_t Material_EnvironmentClass(const material_t* mat)
{
    assert(mat);
    if(!Material_IsDrawable(mat))
        return MEC_UNKNOWN;
    return mat->_envClass;
}

void Material_SetEnvironmentClass(material_t* mat, material_env_class_t envClass)
{
    assert(mat);
    mat->_envClass = envClass;
}

Texture* Material_DetailTexture(material_t* mat)
{
    assert(mat);
    return mat->_detailTex;
}

void Material_SetDetailTexture(material_t* mat, Texture* tex)
{
    assert(mat);
    mat->_detailTex = tex;
}

float Material_DetailStrength(material_t* mat)
{
    assert(mat);
    return mat->_detailStrength;
}

void Material_SetDetailStrength(material_t* mat, float strength)
{
    assert(mat);
    mat->_detailStrength = MINMAX_OF(0, strength, 1);
}

float Material_DetailScale(material_t* mat)
{
    assert(mat);
    return mat->_detailScale;
}

void Material_SetDetailScale(material_t* mat, float scale)
{
    assert(mat);
    mat->_detailScale = MINMAX_OF(0, scale, 1);
}

Texture* Material_ShinyTexture(material_t* mat)
{
    assert(mat);
    return mat->_shinyTex;
}

void Material_SetShinyTexture(material_t* mat, Texture* tex)
{
    assert(mat);
    mat->_shinyTex = tex;
}

blendmode_t Material_ShinyBlendmode(material_t* mat)
{
    assert(mat);
    return mat->_shinyBlendmode;
}

void Material_SetShinyBlendmode(material_t* mat, blendmode_t blendmode)
{
    assert(mat && VALID_BLENDMODE(blendmode));
    mat->_shinyBlendmode = blendmode;
}

const float* Material_ShinyMinColor(material_t* mat)
{
    assert(mat);
    return mat->_shinyMinColor;
}

void Material_SetShinyMinColor(material_t* mat, const float colorRGB[3])
{
    assert(mat && colorRGB);
    mat->_shinyMinColor[CR] = MINMAX_OF(0, colorRGB[CR], 1);
    mat->_shinyMinColor[CG] = MINMAX_OF(0, colorRGB[CG], 1);
    mat->_shinyMinColor[CB] = MINMAX_OF(0, colorRGB[CB], 1);
}

float Material_ShinyStrength(material_t* mat)
{
    assert(mat);
    return mat->_shinyStrength;
}

void Material_SetShinyStrength(material_t* mat, float strength)
{
    assert(mat);
    mat->_shinyStrength = MINMAX_OF(0, strength, 1);
}

Texture* Material_ShinyMaskTexture(material_t* mat)
{
    assert(mat);
    return mat->_shinyMaskTex;
}

void Material_SetShinyMaskTexture(material_t* mat, Texture* tex)
{
    assert(mat);
    mat->_shinyMaskTex = tex;
}

materialvariant_t* Material_AddVariant(material_t* mat, materialvariant_t* variant)
{
    material_variantlist_node_t* node;
    assert(mat);

    if(!variant)
    {
#if _DEBUG
        Con_Error("Material::AddVariant: Argument variant==NULL, ignoring.");
#endif
        return variant;
    }

    node = (material_variantlist_node_t*) malloc(sizeof *node);
    if(!node)
        Con_Error("Material::AddVariant: Failed on allocation of %lu bytes for new node.", (unsigned long) sizeof *node);

    node->variant = variant;
    node->next = mat->_variants;
    mat->_variants = node;
    return variant;
}

int Material_IterateVariants(material_t* mat,
    int (*callback)(materialvariant_t* variant, void* paramaters), void* paramaters)
{
    int result = 0;
    assert(mat);

    if(callback)
    {
        material_variantlist_node_t* node = mat->_variants;
        while(node)
        {
            material_variantlist_node_t* next = node->next;
            result = callback(node->variant, paramaters);
            if(result) break;
            node = next;
        }
    }
    return result;
}

void Material_DestroyVariants(material_t* mat)
{
    destroyVariants(mat);
}
