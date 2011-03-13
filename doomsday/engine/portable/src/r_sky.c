/**\file r_sky.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

/**
 * Sky Management.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_console.h"

#include "cl_def.h"
#include "m_vector.h"
#include "rend_sky.h"
#include "gltexture.h"
#include "gltexturevariant.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(SkyDetail);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean alwaysDrawSphere = false;
int firstSkyLayer = 0, activeSkyLayers = 0;

skylayer_t skyLayers[MAXSKYLAYERS];

skymodel_t skyModels[NUM_SKY_MODELS];
boolean skyModelsInited = false;

float skyLightBalance = 1;
int skyDetail = 6, skySimple = false;
int skyColumns, skyRows = 3;
float skyDist = 1600;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static float skyAmbientColor[] = { 1.0f, 1.0f, 1.0f };

static boolean skyLightColorDefined = false;
static float skyLightColor[] = { 1.0f, 1.0f, 1.0f };

// CODE --------------------------------------------------------------------

void R_SkyRegister(void)
{
    // Cvars
    C_VAR_INT("rend-sky-detail", &skyDetail, CVF_PROTECTED, 3, 7);
    C_VAR_INT("rend-sky-rows", &skyRows, CVF_PROTECTED, 1, 8);
    C_VAR_FLOAT("rend-sky-distance", &skyDist, CVF_NO_MAX, 1, 0);
    C_VAR_INT("rend-sky-simple", &skySimple, 0, 0, 2);

    // Ccmds
    C_CMD_FLAGS("skydetail", "i", SkyDetail, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("skyrows", "i", SkyDetail, CMDF_NO_DEDICATED);
}

static void configureDefaultSky(void)
{
    // Configure the defaults.
    skyLightColorDefined = false;
    skyLightColor[CR] = skyLightColor[CG] = skyLightColor[CB] = 1.0f;
    skyLightBalance = 1;
    skyAmbientColor[CR] = skyAmbientColor[CG] = skyAmbientColor[CB] = 1.0f;

    { int i;
    for(i = 0; i < MAXSKYLAYERS; ++i)
    {
        skylayer_t* slayer = &skyLayers[i];

        slayer->flags = (i == 0? SLF_ENABLED : 0);
        slayer->material = Materials_NumForName(MATERIALS_TEXTURES_RESOURCE_NAMESPACE_NAME":SKY1");
        slayer->offset = 0;
        // Default the fadeout to black.
        slayer->fadeout.use = (i == 0);
        slayer->fadeout.limit = .3f;
        slayer->fadeout.rgb[CR] = slayer->fadeout.rgb[CG] = slayer->fadeout.rgb[CB] = 0;

        slayer->tex = 0;
        slayer->texWidth = slayer->texHeight = 0;
        slayer->texMagMode = 0;
    }}

    { float fval = .666667f;
    Rend_SkyParams(DD_SKY, DD_HEIGHT, &fval);
    }
    { int ival = 0;
    Rend_SkyParams(DD_SKY, DD_HORIZON, &ival);
    }
}

static void prepareSkySphere(void)
{
    boolean mustPrepare = false;
    float avgMaterialColor[3];
    int avgCount = 0;

    // First, lets check whether we need to do anything.
    { int i;
    skylayer_t* slayer;
    for(i = firstSkyLayer, slayer = &skyLayers[firstSkyLayer]; i < MAXSKYLAYERS; ++i, slayer++)
    {
        if(!(slayer->flags & SLF_ENABLED))
            continue;
        if(slayer->tex != 0) // Already prepared this layer.
            continue;
        mustPrepare = true;
        break;
    }}

    if(!mustPrepare)
        return; // We're done.

    /**
     * Looks like we have work to do.
     * \todo Re-implement the automatic sky light color calculation by
     * rendering the sky to a low-quality cubemap and use that to obtain
     * the lighting characteristics.
     */
    avgMaterialColor[CR] = avgMaterialColor[CG] = avgMaterialColor[CB] = 0;
    avgCount = 0;

    { int i;
    skylayer_t* slayer;
    for(i = firstSkyLayer, slayer = &skyLayers[firstSkyLayer]; i < MAXSKYLAYERS; ++i, slayer++)
    {
        material_load_params_t params;
        material_snapshot_t ms;
        material_t* material;

        if(!(slayer->flags & SLF_ENABLED) || !slayer->material)
            continue;

        material = Materials_ToMaterial(slayer->material);
        memset(&params, 0, sizeof(params));
        params.flags = MLF_LOAD_AS_SKY;
        params.tex.flags = GLTF_NO_COMPRESSION;
        if(slayer->flags & SLF_MASKED)
            params.tex.flags |= GLTF_ZEROMASK;

        Materials_Prepare(&ms, material, false, &params);

        slayer->tex = ms.units[MTU_PRIMARY].tex->glName;
        slayer->texWidth = GLTexture_GetWidth(ms.units[MTU_PRIMARY].tex->generalCase);
        slayer->texHeight = GLTexture_GetHeight(ms.units[MTU_PRIMARY].tex->generalCase);
        slayer->texMagMode = ms.units[MTU_PRIMARY].magMode;

        slayer->fadeout.rgb[CR] = ms.topColor[CR];
        slayer->fadeout.rgb[CG] = ms.topColor[CG];
        slayer->fadeout.rgb[CB] = ms.topColor[CB];

        // Is the fadeout in use?
        if(slayer->fadeout.rgb[CR] <= slayer->fadeout.limit ||
           slayer->fadeout.rgb[CG] <= slayer->fadeout.limit ||
           slayer->fadeout.rgb[CB] <= slayer->fadeout.limit)
        {
            slayer->fadeout.use = true;
        }

        if(!(skyModelsInited && !alwaysDrawSphere))
        {
            avgMaterialColor[CR] += ms.colorAmplified[CR];
            avgMaterialColor[CG] += ms.colorAmplified[CG];
            avgMaterialColor[CB] += ms.colorAmplified[CB];
            ++avgCount;
        }
    }}

    if(avgCount != 0)
    {
        skyAmbientColor[CR] = avgMaterialColor[CR];
        skyAmbientColor[CG] = avgMaterialColor[CG];
        skyAmbientColor[CB] = avgMaterialColor[CB];
        
        // The cap covers a large amount of the sky sphere, so factor it in too.
        if(skyLayers[firstSkyLayer].fadeout.use)
        {
            const fadeout_t* fadeout = &skyLayers[firstSkyLayer].fadeout;
            skyAmbientColor[CR] += fadeout->rgb[CR];
            skyAmbientColor[CG] += fadeout->rgb[CG];
            skyAmbientColor[CB] += fadeout->rgb[CB];
            ++avgCount;
        }

        if(avgCount != 1)
        {
            skyAmbientColor[CR] /= avgCount;
            skyAmbientColor[CG] /= avgCount;
            skyAmbientColor[CB] /= avgCount;
        }

        /**
         * Our automatically-calculated sky light color should not
         * be too prominent, lets be subtle.
         * \fixme Should be done in R_GetSectorLightColor (with cvar).
         */
        skyAmbientColor[CR] = 1.0f - (1.0f - avgMaterialColor[CR]) * .33f;
        skyAmbientColor[CG] = 1.0f - (1.0f - avgMaterialColor[CG]) * .33f;
        skyAmbientColor[CB] = 1.0f - (1.0f - avgMaterialColor[CB]) * .33f;
    }
    else
    {
        skyAmbientColor[CR] = skyAmbientColor[CG] = skyAmbientColor[CB] = 1.0f;
    }

    // Calculate a balancing factor so the light won't appear too bright.
    { const float* color = R_SkyAmbientColor();
    if(color[CR] > 0 || color[CG] > 0 || color[CB] > 0)
    {
        skyLightBalance = 0 + (color[CR]*2 + color[CG]*3 + color[CB]*2) / 7;
    }}

    // When the sky light color changes we must update the lightgrid.
    LG_MarkAllForUpdate(0);
}

void R_SkyInit(void)
{
    firstSkyLayer = 0;
    // Calculate sky vertices.
    Rend_CreateSkySphere(skyDetail, skyRows);
}

void R_SetupSky(ded_sky_t* sky)
{
    configureDefaultSky();

    if(!sky)
        return; // Go with the defaults.

    Rend_SkyParams(DD_SKY, DD_HEIGHT, &sky->height);
    Rend_SkyParams(DD_SKY, DD_HORIZON, &sky->horizonOffset);
    { int i;
    for(i = 1; i <= MAXSKYLAYERS; ++i)
    {
        ded_skylayer_t* def = &sky->layers[i-1];

        if(def->flags & SLF_ENABLED)
        {
            R_SkyLayerEnable(i, true);
            R_SkyLayerMasked(i, (def->flags & SLF_MASKED) != 0);
            { materialnum_t material;
            if(def->material && (material = Materials_NumForName2(def->material)) == 0)
            {
                ddstring_t* path = Uri_ToString(def->material);
                Con_Message("Warning, unknown material \"%s\" in sky def %i, using default.\n", Str_Text(path), i);
                Str_Delete(path);
            }
            else
            {
                R_SkyLayerSetMaterial(i, material);
            }}
            R_SkyLayerSetOffset(i, def->offset);
            R_SkyLayerSetFadeoutLimit(i, def->colorLimit);
        }
        else
        {
            R_SkyLayerEnable(i, false);
        }
    }}

    if(sky->color[CR] > 0 || sky->color[CG] > 0 || sky->color[CB] > 0)
    {
        skyLightColorDefined = true;
        skyLightColor[CR] = sky->color[CR];
        skyLightColor[CG] = sky->color[CG];
        skyLightColor[CB] = sky->color[CB];
    }

    // Any sky models to setup? Models will override the normal sphere by default.
    R_SetupSkyModels(sky);
}

/**
 * The sky models are set up using the data in the definition.
 */
void R_SetupSkyModels(ded_sky_t* def)
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

    for(i = 0, modef = def->models, sm = skyModels; i < NUM_SKY_MODELS;
        ++i, modef++, sm++)
    {
        // Is the model ID set?
        if((sm->model = R_CheckIDModelFor(modef->id)) == NULL)
            continue;

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
    prepareSkySphere();

    if(skyModelsInited)
    {
        int i;
        skymodel_t* sky;
        for(i = 0, sky = skyModels; i < NUM_SKY_MODELS; ++i, sky++)
        {
            if(!sky->def)
                continue;
            R_PrecacheModelSkins(sky->model);
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

    if(clientPaused || !skyModelsInited)
        return;

    for(i = 0, sky = skyModels; i < NUM_SKY_MODELS; ++i, sky++)
    {
        if(!sky->def)
            continue;

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

void R_SkyUpdate(void)
{
    if(novideo || isDedicated)
        return;
    { int i;
    for(i = 0; i < MAXSKYLAYERS; ++i)
    {
        skylayer_t* slayer = &skyLayers[i];
        if(slayer->tex)
            glDeleteTextures(1, &slayer->tex);
        slayer->tex = 0;
    }}
}

const float* R_SkyAmbientColor(void)
{
    static const float white[3] = { 1.0f, 1.0f, 1.0f };
    if(skyLightColorDefined)
        return skyLightColor;
    if(rendSkyLightAuto)
        return skyAmbientColor;
    return white;
}

const fadeout_t* R_SkyFadeout(void)
{
    // The current fadeout is the first layer's fadeout.
    return &skyLayers[firstSkyLayer].fadeout;
}

void R_SkyLayerEnable(int layer, boolean enable)
{
    skylayer_t* slayer;
    if(!VALID_SKY_LAYERID(layer))
        return;
    slayer = &skyLayers[layer-1];
    if(enable)
    {
        slayer->flags |= SLF_ENABLED;
        return;
    }
    slayer->flags &= ~SLF_ENABLED;
}

boolean R_SkyLayerIsEnabled(int layer)
{
    if(VALID_SKY_LAYERID(layer))
        return (skyLayers[layer-1].flags & SLF_ENABLED) != 0;
    return false;
}

void R_SkyLayerMasked(int layer, boolean masked)
{
    skylayer_t* slayer;
    if(!VALID_SKY_LAYERID(layer))
        return;
    slayer = &skyLayers[layer-1];
    if(masked)
        slayer->flags |= SLF_MASKED;
    else
        slayer->flags &= ~SLF_MASKED;
    // Invalidate the loaded texture, if necessary.
    if(slayer->material && masked != (slayer->flags & SLF_MASKED))
        slayer->tex = 0;
}

boolean R_SkyLayerIsMasked(int layer)
{
    if(VALID_SKY_LAYERID(layer))
        return (skyLayers[layer-1].flags & SLF_MASKED) != 0;
    return false;
}

void R_SkyLayerSetMaterial(int layer, materialnum_t material)
{
    skylayer_t* slayer;
    if(!VALID_SKY_LAYERID(layer))
        return;
    slayer = &skyLayers[layer-1];
    if(slayer->material == material)
        return;
    slayer->material = material;
    slayer->tex = 0; // Invalidate the currently prepared layer (if any).
}

void R_SkyLayerSetFadeoutLimit(int layer, float limit)
{
    skylayer_t* slayer;
    if(!VALID_SKY_LAYERID(layer))
        return;
    slayer = &skyLayers[layer-1];
    if(slayer->fadeout.limit == limit)
        return;
    slayer->fadeout.limit = limit;
    slayer->tex = 0; // Invalidate the currently prepared layer (if any).
}

void R_SkyLayerSetOffset(int layer, float offset)
{
    if(VALID_SKY_LAYERID(layer))
        skyLayers[layer-1].offset = offset;
}

void R_SetupSkySphereParamsForSkyLayer(rendskysphereparams_t* params, int layer)
{
    assert(params);
    {
    skylayer_t* slayer;

    if(!VALID_SKY_LAYERID(layer))
        Con_Error("R_SetupSkySphereParamsForSkyLayer: Invalid layer %i.", layer);

    prepareSkySphere();

    memset(params, 0, sizeof(*params));

    slayer = &skyLayers[layer-1];
    params->offset = slayer->offset;
    params->tex = slayer->tex;
    params->texWidth = slayer->texWidth;
    params->texHeight = slayer->texHeight;
    params->texMagMode = slayer->texMagMode;
    }
}

D_CMD(SkyDetail)
{
    if(!stricmp(argv[0], "skydetail"))
    {
        Rend_CreateSkySphere(strtol(argv[1], NULL, 0), skyRows);
    }
    else if(!stricmp(argv[0], "skyrows"))
    {
        Rend_CreateSkySphere(skyDetail, strtol(argv[1], NULL, 0));
    }
    return true;
}
