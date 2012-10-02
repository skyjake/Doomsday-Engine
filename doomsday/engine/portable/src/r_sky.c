/**\file r_sky.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
#include "de_graphics.h"
#include "de_refresh.h"

#include "cl_def.h"
#include "m_vector.h"
#include "texture.h"
#include "texturevariant.h"
#include "materialvariant.h"

/**
 * @defgroup skyLayerFlags  Sky Layer Flags
 * @{
 */
#define SLF_ACTIVE              0x1 /// Layer is active and will be rendered.
#define SLF_MASKED              0x2 /// Mask this layer's texture.
/**@}*/

typedef struct {
    int flags;
    material_t* material;
    float offset;
    float fadeoutLimit;
} skylayer_t;

#define VALID_SKY_LAYERID(val) ((val) > 0 && (val) <= MAX_SKY_LAYERS)

static void R_SetupSkyModels(ded_sky_t* def);

boolean alwaysDrawSphere;

skylayer_t skyLayers[MAX_SKY_LAYERS];

skymodel_t skyModels[MAX_SKY_MODELS];
boolean skyModelsInited;

static int firstSkyLayer, activeSkyLayers;
static float horizonOffset;
static float height;

static boolean skyAmbientColorDefined; /// @c true= pre-defined in a MapInfo def.
static boolean needUpdateSkyAmbientColor; /// @c true= update if not pre-defined.
static ColorRawf skyAmbientColor;

static __inline skylayer_t* skyLayerById(int id)
{
    if(!VALID_SKY_LAYERID(id)) return NULL;
    return skyLayers + (id-1);
}

static void configureDefaultSky(void)
{
    int i;

    skyAmbientColorDefined = false;
    needUpdateSkyAmbientColor = true;
    V3f_Set(skyAmbientColor.rgb, 1.0f, 1.0f, 1.0f);

    for(i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        skylayer_t* layer = &skyLayers[i];
        layer->flags = (i == 0? SLF_ACTIVE : 0);
        layer->material = Materials_ToMaterial(Materials_ResolveUriCString(DEFAULT_SKY_SPHERE_MATERIAL));
        layer->offset = DEFAULT_SKY_SPHERE_XOFFSET;
        layer->fadeoutLimit = DEFAULT_SKY_SPHERE_FADEOUT_LIMIT;
    }

    height = DEFAULT_SKY_HEIGHT;
    horizonOffset = DEFAULT_SKY_HORIZON_OFFSET;
}

static void calculateSkyAmbientColor(void)
{
    ColorRawf avgMaterialColor = { 0, 0, 0, 0 };
    ColorRawf bottomCapColor = { 0, 0, 0, 0 };
    ColorRawf topCapColor = { 0, 0, 0, 0 };
    skylayer_t* slayer;
    int i, avgCount;

    if(!needUpdateSkyAmbientColor) return;
    needUpdateSkyAmbientColor = false;

    V3f_Set(skyAmbientColor.rgb, 1.0f, 1.0f, 1.0f);
    if(skyModelsInited && !alwaysDrawSphere) return;

    /**
     * \todo Re-implement me by rendering the sky to a low-quality cubemap
     * and use that to obtain the lighting characteristics.
     */
    avgCount = 0;
    for(i = 0, slayer = &skyLayers[firstSkyLayer]; i < MAX_SKY_LAYERS; ++i, slayer++)
    {
        const materialvariantspecification_t* spec;
        const materialsnapshot_t* ms;

        if(!(slayer->flags & SLF_ACTIVE) || !slayer->material) continue;

        /**
         * \note Ensure that this specification matches that used when
         * preparing the sky for render (see ./engine/portable/src/rend_sky.c
         * configureRenderHemisphereStateForLayer) else an unnecessary
         * GL texture will be uploaded as a consequence of this call.
         */
        spec = Materials_VariantSpecificationForContext(MC_SKYSPHERE,
            TSF_NO_COMPRESSION | ((slayer->flags & SLF_MASKED)? TSF_ZEROMASK : 0),
            0, 0, 0, GL_REPEAT, GL_CLAMP_TO_EDGE, 0, -1, -1, false, true, false, false);
        ms = Materials_Prepare(slayer->material, spec, false);

        if(MSU_texture(ms, MTU_PRIMARY))
        {
            const averagecolor_analysis_t* avgColor = (const averagecolor_analysis_t*)
                Texture_AnalysisDataPointer(MSU_texture(ms, MTU_PRIMARY), TA_COLOR);
            if(!avgColor)
                Con_Error("calculateSkyAmbientColor: Texture id:%u has no TA_COLOR analysis.", Textures_Id(MSU_texture(ms, MTU_PRIMARY)));

            if(i == firstSkyLayer)
            {
                const Texture* tex = MSU_texture(ms, MTU_PRIMARY);
                const averagecolor_analysis_t* avgLineColor = (const averagecolor_analysis_t*)
                    Texture_AnalysisDataPointer(tex, TA_LINE_TOP_COLOR);
                if(!avgLineColor)
                    Con_Error("calculateSkyAmbientColor: Texture id:%u has no TA_LINE_TOP_COLOR analysis.", Textures_Id(MSU_texture(ms, MTU_PRIMARY)));
                V3f_Copy(topCapColor.rgb, avgLineColor->color.rgb);

                avgLineColor = (const averagecolor_analysis_t*)
                    Texture_AnalysisDataPointer(tex, TA_LINE_BOTTOM_COLOR);
                if(!avgLineColor)
                    Con_Error("calculateSkyAmbientColor: Texture id:%u has no TA_LINE_BOTTOM_COLOR analysis.", Textures_Id(MSU_texture(ms, MTU_PRIMARY)));
                V3f_Copy(bottomCapColor.rgb, avgLineColor->color.rgb);
            }

            V3f_Sum(avgMaterialColor.rgb, avgMaterialColor.rgb, avgColor->color.rgb);
            ++avgCount;
        }
    }

    if(avgCount != 0)
    {
        // The caps cover a large amount of the sky sphere, so factor it in too.
        vec3f_t capSum;
        V3f_Sum(capSum, topCapColor.rgb, bottomCapColor.rgb);
        V3f_Sum(skyAmbientColor.rgb, avgMaterialColor.rgb, capSum);
        avgCount += 2; // Each cap is another unit.
        V3f_Scale(skyAmbientColor.rgb, 1.f / avgCount);
    }
}

void R_SkyInit(void)
{
    firstSkyLayer = 0;
    activeSkyLayers = 0;
    skyModelsInited = false;
    alwaysDrawSphere = false;
    needUpdateSkyAmbientColor = true;
}

void R_SetupSky(ded_sky_t* def)
{
    int i;

    configureDefaultSky();
    if(!def) return; // Go with the defaults.

    height  = def->height;
    horizonOffset = def->horizonOffset;

    for(i = 1; i <= MAX_SKY_LAYERS; ++i)
    {
        ded_skylayer_t* sl = &def->layers[i-1];

        if(!(sl->flags & SLF_ACTIVE))
        {
            R_SkyLayerSetActive(i, false);
            continue;
        }

        R_SkyLayerSetActive(i, true);
        R_SkyLayerSetMasked(i, (sl->flags & SLF_MASKED) != 0);
        if(sl->material)
        {
            material_t* mat = Materials_ToMaterial(
                Materials_ResolveUri2(sl->material, true/*quiet please*/));
            if(mat)
            {
                R_SkyLayerSetMaterial(i, mat);
            }
            else
            {
                AutoStr* path = Uri_ToString(sl->material);
                Con_Message("Warning: Unknown material \"%s\" in sky def %i, using default.\n", Str_Text(path), i);
            }
        }
        R_SkyLayerSetOffset(i, sl->offset);
        R_SkyLayerSetFadeoutLimit(i, sl->colorLimit);
    }

    if(def->color[CR] > 0 || def->color[CG] > 0 || def->color[CB] > 0)
    {
        skyAmbientColorDefined = true;
        V3f_Set(skyAmbientColor.rgb, def->color[CR], def->color[CG], def->color[CB]);
    }

    // Any sky models to setup? Models will override the normal sphere by default.
    R_SetupSkyModels(def);
}

/**
 * The sky models are set up using the data in the definition.
 */
static void R_SetupSkyModels(ded_sky_t* def)
{
    ded_skymodel_t* modef;
    skymodel_t* sm;
    int i;

    // Clear the whole sky models data.
    memset(skyModels, 0, sizeof(skyModels));

    // Normally the sky sphere is not drawn if models are in use.
    alwaysDrawSphere = (def->flags & SIF_DRAW_SPHERE) != 0;

    // The normal sphere is used if no models will be set up.
    skyModelsInited = false;

    for(i = 0, modef = def->models, sm = skyModels; i < MAX_SKY_MODELS;
        ++i, modef++, sm++)
    {
        // Is the model ID set?
        sm->model = R_CheckIDModelFor(modef->id);
        if(!sm->model) continue;

        // There is a model here.
        skyModelsInited = true;

        sm->def = modef;
        sm->maxTimer = (int) (TICSPERSEC * modef->frameInterval);
        sm->yaw = modef->yaw;
        sm->frame = sm->model->sub[0].frame;
    }
}

/**
 * Prepare all sky model skins.
 */
void R_SkyPrecache(void)
{
    needUpdateSkyAmbientColor = true;
    calculateSkyAmbientColor();

    if(skyModelsInited)
    {
        int i;
        skymodel_t* sky;
        for(i = 0, sky = skyModels; i < MAX_SKY_MODELS; ++i, sky++)
        {
            if(!sky->def) continue;
            R_PrecacheModel(sky->model);
        }
    }
}

/**
 * Animate sky models.
 */
void R_SkyTicker(void)
{
    skymodel_t* sky;
    int i;

    if(clientPaused || !skyModelsInited) return;

    for(i = 0, sky = skyModels; i < MAX_SKY_MODELS; ++i, sky++)
    {
        if(!sky->def) continue;

        // Rotate the model.
        sky->yaw += sky->def->yawSpeed / TICSPERSEC;

        // Is it time to advance to the next frame?
        if(sky->maxTimer > 0 && ++sky->timer >= sky->maxTimer)
        {
            sky->timer = 0;
            sky->frame++;

            // Execute a console command?
            if(sky->def->execute)
                Con_Execute(CMDS_SCRIPT, sky->def->execute, true, false);
        }
    }
}

int R_SkyFirstActiveLayer(void)
{
    return firstSkyLayer+1; //1-based index.
}

const ColorRawf* R_SkyAmbientColor(void)
{
    static const ColorRawf white = { 1.0f, 1.0f, 1.0f, 0 };
    if(skyAmbientColorDefined || rendSkyLightAuto)
    {
        if(!skyAmbientColorDefined)
        {
            calculateSkyAmbientColor();
        }
        return &skyAmbientColor;
    }
    return &white;
}

float R_SkyHorizonOffset(void)
{
    return horizonOffset;
}

float R_SkyHeight(void)
{
    return height;
}

void R_SkyLayerSetActive(int id, boolean active)
{
    skylayer_t* layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning:R_SkyLayerSetActive: Invalid layer id #%i, ignoring.\n", id);
#endif
        return;
    }

    if(active) layer->flags |= SLF_ACTIVE;
    else       layer->flags &= ~SLF_ACTIVE;

    needUpdateSkyAmbientColor = true;
}

boolean R_SkyLayerActive(int id)
{
    skylayer_t* layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning:R_SkyLayerActive: Invalid layer id #%i, returning false.\n", id);
#endif
        return false;
    }
    return !!(layer->flags & SLF_ACTIVE);
}

void R_SkyLayerSetMasked(int id, boolean masked)
{
    skylayer_t* layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning:R_SkyLayerSetMasked: Invalid layer id #%i, ignoring.\n", id);
#endif
        return;
    }
    if(masked) layer->flags |= SLF_MASKED;
    else       layer->flags &= ~SLF_MASKED;

    needUpdateSkyAmbientColor = true;
}

boolean R_SkyLayerMasked(int id)
{
    skylayer_t* layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning:R_SkyLayerMasked: Invalid layer id #%i, returning false.\n", id);
#endif
        return false;
    }
    return !!(layer->flags & SLF_MASKED);
}

material_t* R_SkyLayerMaterial(int id)
{
    skylayer_t* layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning:R_SkyLayerMaterial: Invalid layer id #%i, returning NULL.\n", id);
#endif
        return NULL;
    }
    return layer->material;
}

void R_SkyLayerSetMaterial(int id, material_t* mat)
{
    skylayer_t* layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning:R_SkyLayerSetMaterial: Invalid layer id #%i, ignoring.\n", id);
#endif
        return;
    }
    if(layer->material == mat) return;

    layer->material = mat;
    needUpdateSkyAmbientColor = true;
}

float R_SkyLayerFadeoutLimit(int id)
{
    skylayer_t* layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning:R_SkyLayerFadeoutLimit: Invalid layer id #%i, returning default.\n", id);
#endif
        return DEFAULT_SKY_SPHERE_FADEOUT_LIMIT;
    }
    return layer->fadeoutLimit;
}

void R_SkyLayerSetFadeoutLimit(int id, float limit)
{
    skylayer_t* layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning:R_SkyLayerSetFadeoutLimit: Invalid layer id #%i, ignoring.\n", id);
#endif
        return;
    }
    if(layer->fadeoutLimit == limit) return;

    layer->fadeoutLimit = limit;
    needUpdateSkyAmbientColor = true;
}

float R_SkyLayerOffset(int id)
{
    skylayer_t* layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning:R_SkyLayerOffset: Invalid layer id #%i, returning default.\n", id);
#endif
        return DEFAULT_SKY_SPHERE_XOFFSET;
    }
    return layer->offset;
}

void R_SkyLayerSetOffset(int id, float offset)
{
    skylayer_t* layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning:R_SkyLayerOffset: Invalid layer id #%i, ignoring.\n", id);
#endif
        return;
    }
    if(layer->offset == offset) return;
    layer->offset = offset;
}

static void chooseFirstLayer(void)
{
    int i;
    // -1 denotes 'no active layers'.
    firstSkyLayer = -1;
    activeSkyLayers = 0;

    for(i = 1; i <= MAX_SKY_LAYERS; ++i)
    {
        if(!R_SkyLayerActive(i)) continue;

        ++activeSkyLayers;
        if(firstSkyLayer == -1)
        {
            firstSkyLayer = i-1;
        }
    }
}

static void internalSkyParams(int layer, int param, void* data)
{
    switch(param)
    {
    case DD_ENABLE:
        R_SkyLayerSetActive(layer, true);
        chooseFirstLayer();
        break;

    case DD_DISABLE:
        R_SkyLayerSetActive(layer, false);
        chooseFirstLayer();
        break;

    case DD_MASK:
        R_SkyLayerSetMasked(layer, *((int*)data) == DD_YES);
        break;

    case DD_MATERIAL: {
        materialid_t materialId = *((materialid_t*) data);
        R_SkyLayerSetMaterial(layer, Materials_ToMaterial(materialId));
        break;
      }
    case DD_OFFSET:
        R_SkyLayerSetOffset(layer, *((float*) data));
        break;

    case DD_COLOR_LIMIT:
        R_SkyLayerSetFadeoutLimit(layer, *((float*) data));
        break;

    default:
        Con_Error("R_SkyParams: Bad parameter (%d).\n", param);
        break;
    }
}

void R_SkyParams(int layer, int param, void* data)
{
    if(layer == DD_SKY) // The whole sky?
    {
        switch(param)
        {
        case DD_HEIGHT:
            height = *((float*) data);
            break;

        case DD_HORIZON: // Horizon offset angle
            horizonOffset = *((float*) data);
            break;

        default: { // Operate on all layers.
            int i;
            for(i = 1; i <= MAX_SKY_LAYERS; ++i)
            {
                internalSkyParams(i, param, data);
            }
            break;
          }
        }
        return;
    }

    // A specific layer?
    if(layer >= 0 && layer < MAX_SKY_LAYERS)
    {
        internalSkyParams(layer+1, param, data);
    }
}
