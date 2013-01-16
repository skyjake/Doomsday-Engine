/** @file material.cpp Logical Material.
 *
 * @author Copyright &copy; 2009-2013 Daniel Swanson <danij@dengine.net>
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

#include <cstring>

#include "de_base.h"
#include "de_defs.h"
#include "de_resource.h"
#include "de_render.h"

#include "api_map.h"
#include "audio/s_environ.h"
#include "gl/sys_opengl.h" // TODO: get rid of this
#include "map/r_world.h"
#include "sys_system.h" // var: novideo

#include "resource/material.h"

using namespace de;

struct material_s
{
    /// DMU object header.
    runtime_mapdata_header_t header;

    /// Definition from which this material was derived (if any).
    ded_material_t *def;

    /// Set of use-case/context variant instances.
    Material::Variants variants;

    /// Environmental sound class.
    material_env_class_t envClass;

    /// Unique identifier of the MaterialManifest associated with this Material or @c NULL if not bound.
    materialid_t manifestId;

    /// World dimensions in map coordinate space units.
    Size2 *dimensions;

    /// @see materialFlags
    short flags;

    /// Detail texture layer & properties.
    Texture *detailTex;
    float detailScale;
    float detailStrength;

    /// Shiny texture layer & properties.
    Texture *shinyTex;
    blendmode_t shinyBlendmode;
    float shinyMinColor[3];
    float shinyStrength;
    Texture *shinyMaskTex;

    /// Layers.
    Material::Layers layers;

    /// Decorations (will be projected into the map relative to a surface).
    Material::Decorations decorations;

    /// Current prepared state.
    byte prepared;

    material_s(short _flags, ded_material_t &_def,
               Size2Raw &_dimensions, material_env_class_t _envClass)
        : def(&_def), envClass(_envClass), manifestId(0),
          dimensions(Size2_NewFromRaw(&_dimensions)), flags(_flags),
          detailTex(0), detailScale(0), detailStrength(0),
          shinyTex(0), shinyBlendmode(BM_ADD), shinyStrength(0), shinyMaskTex(0),
          prepared(0)
    {
        header.type = DMU_MATERIAL;
        std::memset(shinyMinColor, 0, sizeof(shinyMinColor));

        for(int i = 0; i < DED_MAX_MATERIAL_LAYERS; ++i)
        {
            Material::Layer *layer = Material::Layer::fromDef(_def.layers[i]);
            layers.push_back(layer);
        }
    }

    ~material_s()
    {
        clearDecorations();
        clearLayers();
        clearVariants();
        Size2_Delete(dimensions);
    }

    void clearVariants()
    {
        while(!variants.isEmpty())
        {
             delete variants.takeFirst();
        }
        prepared = 0;
    }

    void clearLayers()
    {
        while(!layers.isEmpty())
        {
            delete layers.takeFirst();
        }
    }

    void clearDecorations()
    {
        while(!decorations.isEmpty())
        {
            delete decorations.takeFirst();
        }
    }
};

material_t *Material_New(short flags, ded_material_t *def,
    Size2Raw *dimensions, material_env_class_t envClass)
{
    DENG_ASSERT(def && dimensions);
    return new material_s(flags, *def, *dimensions, envClass);
}

void Material_Delete(material_t *mat)
{
    if(mat)
    {
        delete (material_s *) mat;
    }
}

int Material_DecorationCount(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->decorations.count();
}

boolean Material_IsValid(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return !!Material_Definition(mat);
}

void Material_Ticker(material_t *mat, timespan_t time)
{
    DENG2_ASSERT(mat);
    DENG2_FOR_EACH(Material::Variants, i, mat->variants)
    {
        (*i)->ticker(time);
    }
}

ded_material_t *Material_Definition(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->def;
}

void Material_SetDefinition(material_t *mat, struct ded_material_s *def)
{
    DENG2_ASSERT(mat);
    if(mat->def != def)
    {
        mat->def = def;

        // Textures are updated automatically at prepare-time, so just clear them.
        Material_SetDetailTexture(mat, NULL);
        Material_SetShinyTexture(mat, NULL);
        Material_SetShinyMaskTexture(mat, NULL);
    }

    if(!mat->def) return;

    MaterialManifest &manifest = Material_Manifest(mat);

    mat->flags = mat->def->flags;
    Size2Raw size(def->width, def->height);
    Material_SetDimensions(mat, &size);
    Material_SetEnvironmentClass(mat, S_MaterialEnvClassForUri(def->uri));

    // Update custom status.
    /// @todo This should take into account the whole definition, not just whether
    ///       the primary layer's first texture is custom or not.
    manifest.setCustom(false);
    if(mat->layers[0]->stageCount() > 0 && mat->layers[0]->stages()[0]->texture)
    {
        try
        {
            de::Uri *texUri = reinterpret_cast<de::Uri *>(mat->layers[0]->stages()[0]->texture);
            if(Texture *tex = App_Textures()->find(*texUri).texture())
            {
                manifest.setCustom(tex->flags().testFlag(Texture::Custom));
            }
        }
        catch(Textures::NotFoundError const &)
        {} // Ignore this error.
    }
}

Size2 const *Material_Dimensions(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->dimensions;
}

void Material_SetDimensions(material_t* mat, const Size2Raw* newSize)
{
    DENG2_ASSERT(mat);
    if(!newSize) return;

    Size2 *size = Size2_NewFromRaw(newSize);
    if(!Size2_Equality(mat->dimensions, size))
    {
        Size2_SetWidthHeight(mat->dimensions, newSize->width, newSize->height);
        R_UpdateMapSurfacesOnMaterialChange(mat);
    }
    Size2_Delete(size);
}

int Material_Width(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return Size2_Width(mat->dimensions);
}

void Material_SetWidth(material_t *mat, int width)
{
    DENG2_ASSERT(mat);
    if(Size2_Width(mat->dimensions) == width) return;
    Size2_SetWidth(mat->dimensions, width);
    R_UpdateMapSurfacesOnMaterialChange(mat);
}

int Material_Height(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return Size2_Height(mat->dimensions);
}

void Material_SetHeight(material_t *mat, int height)
{
    DENG2_ASSERT(mat);
    if(Size2_Height(mat->dimensions) == height) return;
    Size2_SetHeight(mat->dimensions, height);
    R_UpdateMapSurfacesOnMaterialChange(mat);
}

short Material_Flags(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->flags;
}

void Material_SetFlags(material_t *mat, short flags)
{
    DENG2_ASSERT(mat);
    mat->flags = flags;
}

boolean Material_IsAnimated(material_t const *mat)
{
    // Materials cease animation once they are no longer valid.
    if(!Material_IsValid(mat)) return false;

    DENG2_FOR_EACH_CONST(Material::Layers, i, mat->layers)
    {
        if((*i)->stageCount() > 1) return true;
    }
    return false; // Not at all.
}

boolean Material_IsSkyMasked(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return 0 != (mat->flags & MATF_SKYMASK);
}

boolean Material_IsDrawable(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return 0 == (mat->flags & MATF_NO_DRAW);
}

boolean Material_HasGlow(material_t const *mat)
{
    DENG_ASSERT(mat);
    if(mat->def)
    {
        DENG2_FOR_EACH_CONST(Material::Layers, i, mat->layers)
        {
            Material::Layer::Stages const &stages = (*i)->stages();
            DENG2_FOR_EACH_CONST(Material::Layer::Stages, k, stages)
            {
                ded_material_layer_stage_t const &stage = **k;
                if(stage.glowStrength > .0001f) return true;
            }
        }
    }
    return false;
}

int Material_LayerCount(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->layers.count();
}

byte Material_Prepared(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->prepared;
}

void Material_SetPrepared(material_t *mat, byte state)
{
    DENG2_ASSERT(mat && state <= 2);
    mat->prepared = state;
}

MaterialManifest &Material_Manifest(material_t const *mat)
{
    DENG2_ASSERT(mat);
    /// @todo Material should store a link to the manifest.
    return *App_Materials()->toMaterialManifest(mat->manifestId);
}

materialid_t Material_ManifestId(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->manifestId;
}

void Material_SetManifestId(material_t *mat, materialid_t id)
{
    DENG2_ASSERT(mat);
    mat->manifestId = id;
}

material_env_class_t Material_EnvironmentClass(material_t const *mat)
{
    DENG2_ASSERT(mat);
    if(!Material_IsDrawable(mat))
        return MEC_UNKNOWN;
    return mat->envClass;
}

void Material_SetEnvironmentClass(material_t *mat, material_env_class_t envClass)
{
    DENG2_ASSERT(mat);
    mat->envClass = envClass;
}

struct texture_s *Material_DetailTexture(material_t *mat)
{
    DENG2_ASSERT(mat);
    return reinterpret_cast<struct texture_s *>(mat->detailTex);
}

void Material_SetDetailTexture(material_t *mat, struct texture_s *tex)
{
    DENG2_ASSERT(mat);
    mat->detailTex = reinterpret_cast<de::Texture *>(tex);
}

float Material_DetailStrength(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->detailStrength;
}

void Material_SetDetailStrength(material_t *mat, float strength)
{
    DENG2_ASSERT(mat);
    mat->detailStrength = MINMAX_OF(0, strength, 1);
}

float Material_DetailScale(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->detailScale;
}

void Material_SetDetailScale(material_t *mat, float scale)
{
    DENG2_ASSERT(mat);
    mat->detailScale = MINMAX_OF(0, scale, 1);
}

struct texture_s *Material_ShinyTexture(material_t *mat)
{
    DENG2_ASSERT(mat);
    return reinterpret_cast<struct texture_s *>(mat->shinyTex);
}

void Material_SetShinyTexture(material_t *mat, struct texture_s *tex)
{
    DENG2_ASSERT(mat);
    mat->shinyTex = reinterpret_cast<de::Texture *>(tex);
}

blendmode_t Material_ShinyBlendmode(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->shinyBlendmode;
}

void Material_SetShinyBlendmode(material_t *mat, blendmode_t blendmode)
{
    DENG2_ASSERT(mat && VALID_BLENDMODE(blendmode));
    mat->shinyBlendmode = blendmode;
}

float const *Material_ShinyMinColor(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->shinyMinColor;
}

void Material_SetShinyMinColor(material_t *mat, float const colorRGB[3])
{
    DENG2_ASSERT(mat && colorRGB);
    mat->shinyMinColor[CR] = MINMAX_OF(0, colorRGB[CR], 1);
    mat->shinyMinColor[CG] = MINMAX_OF(0, colorRGB[CG], 1);
    mat->shinyMinColor[CB] = MINMAX_OF(0, colorRGB[CB], 1);
}

float Material_ShinyStrength(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->shinyStrength;
}

void Material_SetShinyStrength(material_t *mat, float strength)
{
    DENG2_ASSERT(mat);
    mat->shinyStrength = MINMAX_OF(0, strength, 1);
}

struct texture_s *Material_ShinyMaskTexture(material_t *mat)
{
    DENG2_ASSERT(mat);
    return reinterpret_cast<struct texture_s *>(mat->shinyMaskTex);
}

void Material_SetShinyMaskTexture(material_t *mat, struct texture_s *tex)
{
    DENG2_ASSERT(mat);
    mat->shinyMaskTex = reinterpret_cast<de::Texture *>(tex);
}

Material::Layers const &Material_Layers(material_t const *mat)
{
    DENG_ASSERT(mat);
    return mat->layers;
}

void Material_AddDecoration(material_t *mat, de::Material::Decoration &decor)
{
    DENG_ASSERT(mat);
    mat->decorations.push_back(&decor);
}

Material::Decorations const &Material_Decorations(material_t const *mat)
{
    DENG_ASSERT(mat);
    return mat->decorations;
}

Material::Variants const &Material_Variants(material_t const *mat)
{
    DENG_ASSERT(mat);
    return mat->variants;
}

MaterialVariant *Material_ChooseVariant(material_t *mat, MaterialVariantSpec const &spec,
                                        bool canCreate)
{
    DENG_ASSERT(mat);

    MaterialVariant *variant = 0;
    DENG2_FOR_EACH_CONST(Material::Variants, i, mat->variants)
    {
        MaterialVariantSpec const &cand = (*i)->spec();
        if(cand.compare(spec))
        {
            // This will do fine.
            variant = *i;
            break;
        }
    }

    if(!variant)
    {
        if(!canCreate) return 0;

        variant = new MaterialVariant(*mat, spec);
        mat->variants.push_back(variant);
    }

    return variant;
}

int Material_VariantCount(material_t const *mat)
{
    DENG_ASSERT(mat);
    return mat->variants.count();
}

void Material_ClearVariants(material_t *mat)
{
    DENG_ASSERT(mat);
    mat->clearVariants();
}

boolean Material_HasDecorations(material_t const *mat)
{
    DENG_ASSERT(mat);
    return mat->decorations.count() != 0;
}

int Material_GetProperty(material_t const *mat, setargs_t *args)
{
    DENG_ASSERT(mat && args);
    switch(args->prop)
    {
    case DMU_FLAGS: {
        short flags = Material_Flags(mat);
        DMU_GetValue(DMT_MATERIAL_FLAGS, &flags, args, 0);
        break; }

    case DMU_WIDTH: {
        int width = Material_Width(mat);
        DMU_GetValue(DMT_MATERIAL_WIDTH, &width, args, 0);
        break; }

    case DMU_HEIGHT: {
        int height = Material_Height(mat);
        DMU_GetValue(DMT_MATERIAL_HEIGHT, &height, args, 0);
        break; }

    default: {
        QByteArray msg = String("Material_GetProperty: No property %1.").arg(DMU_Str(args->prop)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }
    return false; // Continue iteration.
}

int Material_SetProperty(material_t *mat, setargs_t const *args)
{
    DENG_UNUSED(mat);
    QByteArray msg = String("Material_SetProperty: Property '%1' is not writable.").arg(DMU_Str(args->prop)).toUtf8();
    LegacyCore_FatalError(msg.constData());
    return 0; // Unreachable.
}
