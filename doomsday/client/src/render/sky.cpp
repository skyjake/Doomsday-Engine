/** @file sky.cpp Sky Sphere and 3D Models
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"

#include "def_data.h"
#include "client/cl_def.h"
#include "map/r_world.h"

#include <de/vector1.h>

#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "Texture"

#include "render/sky.h"

using namespace de;

/**
 * @defgroup skySphereRenderFlags  Sky Render Flags
 * @ingroup flags
 * @{
 */
#define SKYHEMI_UPPER           0x1
#define SKYHEMI_LOWER           0x2
#define SKYHEMI_JUST_CAP        0x4 ///< Just draw the top or bottom cap.
/**@}*/

/**
 * @defgroup skyLayerFlags  Sky Layer Flags
 * @ingroup flags
 * @{
 */
#define SLF_ACTIVE              0x1 ///< Layer is active and will be rendered.
#define SLF_MASKED              0x2 ///< Mask this layer's texture.
/**@}*/

/**
 * Logical sky "layer" (for parallax effects).
 */
struct skylayer_t
{
    int flags;
    Material *material;
    float offset;
    float fadeoutLimit;
};

#define VALID_SKY_LAYERID(val) ((val) > 0 && (val) <= MAX_SKY_LAYERS)

struct skyvertex_t
{
    float pos[3];
};

enum hemispherecap_t
{
    HC_NONE = 0,
    HC_TOP,
    HC_BOTTOM
};

struct skymodel_t
{
    ded_skymodel_t *def;
    modeldef_t *model;
    int frame;
    int timer;
    int maxTimer;
    float yaw;
};

struct renderhemispherestate_t
{
    bool fadeout, texXFlip;
    Size2Raw texSize;
    float texOffset;
    ColorRawf capColor;
};

static int skyDetail     = 6;
static int skyRows       = 3;
static float skyDistance = 1600;

// CVar callback function which marks the sphere as needing to be rebuilt.
static void updateSphere();

void Sky_Register()
{
    C_VAR_INT2("rend-sky-detail",    &skyDetail,   0,          3, 7, updateSphere);
    C_VAR_INT2("rend-sky-rows",      &skyRows,     0,          1, 8, updateSphere);
    C_VAR_FLOAT("rend-sky-distance", &skyDistance, CVF_NO_MAX, 1, 0);
}

static int skyColumns = 4*6;

static void constructHemisphere();
static void destroyHemisphere();

static void rebuildHemisphere();
static void setupSkyModels(ded_sky_t *def);

// @c true iff this module has been initialized.
static bool initedOk = false;

static bool alwaysDrawSphere;

static skylayer_t skyLayers[MAX_SKY_LAYERS];

// Hemisphere geometry used with the sky sphere.
static skyvertex_t *hemisphereVerts; // Crest is up.
static int numHemisphereVerts;
static bool needRebuildHemisphere = true;

static skymodel_t skyModels[MAX_SKY_MODELS];
static bool skyModelsInited;

// Sphere render state paramaters. Global for performance reasons.
static renderhemispherestate_t rs;

static int firstSkyLayer, activeSkyLayers;
static float horizonOffset;
static float height;

static bool skyAmbientColorDefined; /// @c true= pre-defined in a MapInfo def.
static bool needUpdateSkyAmbientColor; /// @c true= update if not pre-defined.
static ColorRawf skyAmbientColor;

static inline skylayer_t *skyLayerById(int id)
{
    if(!VALID_SKY_LAYERID(id)) return 0;
    return skyLayers + (id-1);
}

static void configureDefaultSky()
{
    skyAmbientColorDefined = false;
    needUpdateSkyAmbientColor = true;
    V3f_Set(skyAmbientColor.rgb, 1.0f, 1.0f, 1.0f);

    for(int i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        skylayer_t *layer = &skyLayers[i];
        layer->flags = (i == 0? SLF_ACTIVE : 0);
        layer->material = 0;
        try
        {
            layer->material = &App_Materials().find(de::Uri(DEFAULT_SKY_SPHERE_MATERIAL, RC_NULL)).material();
        }
        catch(MaterialManifest::MissingMaterialError const &)
        {} // Ignore this error.
        catch(Materials::NotFoundError const &)
        {} // Ignore this error.
        layer->offset = DEFAULT_SKY_SPHERE_XOFFSET;
        layer->fadeoutLimit = DEFAULT_SKY_SPHERE_FADEOUT_LIMIT;
    }

    height = DEFAULT_SKY_HEIGHT;
    horizonOffset = DEFAULT_SKY_HORIZON_OFFSET;
}

MaterialVariantSpec const &Sky_SphereMaterialSpec(bool masked)
{
    return App_Materials().variantSpec(SkySphereContext, TSF_NO_COMPRESSION | (masked? TSF_ZEROMASK : 0),
                                       0, 0, 0, GL_REPEAT, GL_CLAMP_TO_EDGE, 0, -1, -1, false, true, false, false);
}

static void calculateSkyAmbientColor()
{
    ColorRawf avgMaterialColor;
    ColorRawf bottomCapColor;
    ColorRawf topCapColor;
    skylayer_t *slayer;
    int i, avgCount;

    if(!needUpdateSkyAmbientColor) return;
    needUpdateSkyAmbientColor = false;

    V3f_Set(skyAmbientColor.rgb, 1.0f, 1.0f, 1.0f);
    if(skyModelsInited && !alwaysDrawSphere) return;

    /**
     * @todo Re-implement me by rendering the sky to a low-quality cubemap
     * and use that to obtain the lighting characteristics.
     */
    avgCount = 0;
    for(i = 0, slayer = &skyLayers[firstSkyLayer]; i < MAX_SKY_LAYERS; ++i, slayer++)
    {
        if(!(slayer->flags & SLF_ACTIVE) || !slayer->material) continue;

        MaterialSnapshot const &ms =
            slayer->material->prepare(Sky_SphereMaterialSpec(!!(slayer->flags & SLF_MASKED)));

        if(ms.hasTexture(MTU_PRIMARY))
        {
            Texture const &tex = ms.texture(MTU_PRIMARY).generalCase();
            averagecolor_analysis_t const *avgColor = reinterpret_cast<averagecolor_analysis_t const *>(tex.analysisDataPointer(Texture::AverageColorAnalysis));
            if(!avgColor) throw Error("calculateSkyAmbientColor", QString("Texture \"%1\" has no AverageColorAnalysis").arg(ms.texture(MTU_PRIMARY).generalCase().manifest().composeUri()));

            if(i == firstSkyLayer)
            {
                averagecolor_analysis_t const *avgLineColor = reinterpret_cast<averagecolor_analysis_t const *>(tex.analysisDataPointer(Texture::AverageTopColorAnalysis));
                if(!avgLineColor) throw Error("calculateSkyAmbientColor", QString("Texture \"%1\" has no AverageTopColorAnalysis").arg(tex.manifest().composeUri()));

                V3f_Copy(topCapColor.rgb, avgLineColor->color.rgb);

                avgLineColor = reinterpret_cast<averagecolor_analysis_t const *>(tex.analysisDataPointer(Texture::AverageBottomColorAnalysis));
                if(!avgLineColor) throw Error("calculateSkyAmbientColor", QString("Texture \"%1\" has no AverageBottomColorAnalysis").arg(tex.manifest().composeUri()));

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

void Sky_Init()
{
    if(novideo || isDedicated || initedOk) return;
    initedOk = true;

    firstSkyLayer = 0;
    activeSkyLayers = 0;
    skyModelsInited = false;
    alwaysDrawSphere = false;
    needUpdateSkyAmbientColor = true;
}

void Sky_Shutdown()
{
    if(novideo || isDedicated || !initedOk) return;
    destroyHemisphere();
    initedOk = false;
}

void Sky_Configure(ded_sky_t *def)
{
    configureDefaultSky();
    if(!def) return; // Go with the defaults.

    height  = def->height;
    horizonOffset = def->horizonOffset;

    for(int i = 1; i <= MAX_SKY_LAYERS; ++i)
    {
        ded_skylayer_t *sl = &def->layers[i-1];

        if(!(sl->flags & SLF_ACTIVE))
        {
            Sky_LayerSetActive(i, false);
            continue;
        }

        Sky_LayerSetActive(i, true);
        Sky_LayerSetMasked(i, (sl->flags & SLF_MASKED) != 0);
        if(sl->material)
        {
            try
            {
                Material *mat = &App_Materials().find(*reinterpret_cast<de::Uri *>(sl->material)).material();
                Sky_LayerSetMaterial(i, mat);
            }
            catch(Materials::NotFoundError const &er)
            {
                // Log but otherwise ignore this error.
                LOG_WARNING(er.asText() + ". Unknown material \"%s\" in sky def %i, using default.")
                    << *reinterpret_cast<de::Uri *>(sl->material) << i;
            }
        }
        Sky_LayerSetOffset(i, sl->offset);
        Sky_LayerSetFadeoutLimit(i, sl->colorLimit);
    }

    if(def->color[CR] > 0 || def->color[CG] > 0 || def->color[CB] > 0)
    {
        skyAmbientColorDefined = true;
        V3f_Set(skyAmbientColor.rgb, def->color[CR], def->color[CG], def->color[CB]);
    }

    // Any sky models to setup? Models will override the normal sphere by default.
    setupSkyModels(def);
}

/**
 * The sky models are set up using the data in the definition.
 */
static void setupSkyModels(ded_sky_t *def)
{
    // Clear the whole sky models data.
    memset(skyModels, 0, sizeof(skyModels));

    // Normally the sky sphere is not drawn if models are in use.
    alwaysDrawSphere = (def->flags & SIF_DRAW_SPHERE) != 0;

    // The normal sphere is used if no models will be set up.
    skyModelsInited = false;

    ded_skymodel_t *modef = def->models;
    skymodel_t *sm = skyModels;
    for(int i = 0; i < MAX_SKY_MODELS; ++i, modef++, sm++)
    {
        // Is the model ID set?
        sm->model = Models_Definition(modef->id);
        if(!sm->model) continue;

        // There is a model here.
        skyModelsInited = true;

        sm->def = modef;
        sm->maxTimer = (int) (TICSPERSEC * modef->frameInterval);
        sm->yaw = modef->yaw;
        sm->frame = sm->model->sub[0].frame;
    }
}

void Sky_Cache()
{
    needUpdateSkyAmbientColor = true;
    calculateSkyAmbientColor();

    if(skyModelsInited)
    {
        skymodel_t* sky = skyModels;
        for(int i = 0; i < MAX_SKY_MODELS; ++i, sky++)
        {
            if(!sky->def) continue;
            Models_Cache(sky->model);
        }
    }
}

void Sky_Ticker()
{
    if(clientPaused || !skyModelsInited) return;

    skymodel_t *sky = skyModels;
    for(int i = 0; i < MAX_SKY_MODELS; ++i, sky++)
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

int Sky_FirstActiveLayer()
{
    return firstSkyLayer+1; //1-based index.
}

ColorRawf const *Sky_AmbientColor()
{
    static ColorRawf const white(1.0f, 1.0f, 1.0f, 0);

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

float Sky_HorizonOffset()
{
    return horizonOffset;
}

float Sky_Height()
{
    return height;
}

void Sky_LayerSetActive(int id, boolean active)
{
    skylayer_t *layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning: Sky_LayerSetActive: Invalid layer id #%i, ignoring.", id);
#endif
        return;
    }

    if(active) layer->flags |= SLF_ACTIVE;
    else       layer->flags &= ~SLF_ACTIVE;

    needUpdateSkyAmbientColor = true;
}

boolean Sky_LayerActive(int id)
{
    skylayer_t* layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning: Sky_LayerActive: Invalid layer id #%i, returning false.", id);
#endif
        return false;
    }
    return !!(layer->flags & SLF_ACTIVE);
}

void Sky_LayerSetMasked(int id, boolean masked)
{
    skylayer_t *layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning: Sky_LayerSetMasked: Invalid layer id #%i, ignoring.", id);
#endif
        return;
    }
    if(masked) layer->flags |= SLF_MASKED;
    else       layer->flags &= ~SLF_MASKED;

    needUpdateSkyAmbientColor = true;
}

boolean Sky_LayerMasked(int id)
{
    skylayer_t *layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning: Sky_LayerMasked: Invalid layer id #%i, returning false.", id);
#endif
        return false;
    }
    return !!(layer->flags & SLF_MASKED);
}

Material *Sky_LayerMaterial(int id)
{
    skylayer_t *layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning: Sky_LayerMaterial: Invalid layer id #%i, returning NULL.", id);
#endif
        return 0;
    }
    return layer->material;
}

void Sky_LayerSetMaterial(int id, Material *mat)
{
    skylayer_t *layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning: Sky_LayerSetMaterial: Invalid layer id #%i, ignoring.", id);
#endif
        return;
    }
    if(layer->material == mat) return;

    layer->material = mat;
    needUpdateSkyAmbientColor = true;
}

float Sky_LayerFadeoutLimit(int id)
{
    skylayer_t *layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning: Sky_LayerFadeoutLimit: Invalid layer id #%i, returning default.", id);
#endif
        return DEFAULT_SKY_SPHERE_FADEOUT_LIMIT;
    }
    return layer->fadeoutLimit;
}

void Sky_LayerSetFadeoutLimit(int id, float limit)
{
    skylayer_t *layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning: Sky_LayerSetFadeoutLimit: Invalid layer id #%i, ignoring.", id);
#endif
        return;
    }
    if(layer->fadeoutLimit == limit) return;

    layer->fadeoutLimit = limit;
    needUpdateSkyAmbientColor = true;
}

float Sky_LayerOffset(int id)
{
    skylayer_t *layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning: Sky_LayerOffset: Invalid layer id #%i, returning default.", id);
#endif
        return DEFAULT_SKY_SPHERE_XOFFSET;
    }
    return layer->offset;
}

void Sky_LayerSetOffset(int id, float offset)
{
    skylayer_t *layer = skyLayerById(id);
    if(!layer)
    {
#if _DEBUG
        Con_Message("Warning: Sky_LayerOffset: Invalid layer id #%i, ignoring.", id);
#endif
        return;
    }
    if(layer->offset == offset) return;
    layer->offset = offset;
}

static void chooseFirstLayer()
{
    // -1 denotes 'no active layers'.
    firstSkyLayer = -1;
    activeSkyLayers = 0;

    for(int i = 1; i <= MAX_SKY_LAYERS; ++i)
    {
        if(!Sky_LayerActive(i)) continue;

        ++activeSkyLayers;
        if(firstSkyLayer == -1)
        {
            firstSkyLayer = i-1;
        }
    }
}

static void internalSkyParams(int layer, int param, void *data)
{
    switch(param)
    {
    case DD_ENABLE:
        Sky_LayerSetActive(layer, true);
        chooseFirstLayer();
        break;

    case DD_DISABLE:
        Sky_LayerSetActive(layer, false);
        chooseFirstLayer();
        break;

    case DD_MASK:
        Sky_LayerSetMasked(layer, *((int *)data) == DD_YES);
        break;

    case DD_MATERIAL:
        Sky_LayerSetMaterial(layer, (Material *)data);
        break;

    case DD_OFFSET:
        Sky_LayerSetOffset(layer, *((float *)data));
        break;

    case DD_COLOR_LIMIT:
        Sky_LayerSetFadeoutLimit(layer, *((float *)data));
        break;

    default:
        Con_Error("R_SkyParams: Bad parameter (%d).\n", param);
    }
}

#undef R_SkyParams
DENG_EXTERN_C void R_SkyParams(int layer, int param, void *data)
{
    if(layer == DD_SKY) // The whole sky?
    {
        switch(param)
        {
        case DD_HEIGHT:
            height = *((float *)data);
            break;

        case DD_HORIZON: // Horizon offset angle
            horizonOffset = *((float *)data);
            break;

        default: // Operate on all layers.
            for(int i = 1; i <= MAX_SKY_LAYERS; ++i)
            {
                internalSkyParams(i, param, data);
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

static void renderSkyModels()
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Setup basic translation.
    glTranslatef(vOrigin[VX], vOrigin[VY], vOrigin[VZ]);

    rendmodelparams_t parm;
    skymodel_t* sky = skyModels;
    for(int i = 0; i < NUM_SKY_MODELS; ++i, sky++)
    {
        if(!sky->def) continue;

        if(!Sky_LayerActive(sky->def->layer+1))
        {
            // The model has been assigned to a layer, but the layer is
            // not visible.
            continue;
        }

        float inter = (sky->maxTimer > 0 ? sky->timer / float(sky->maxTimer) : 0);

        memset(&parm, 0, sizeof(parm));

        // Calculate the coordinates for the model.
        parm.origin[VX] = vOrigin[VX] * -sky->def->coordFactor[VX];
        parm.origin[VY] = vOrigin[VZ] * -sky->def->coordFactor[VZ];
        parm.origin[VZ] = vOrigin[VY] * -sky->def->coordFactor[VY];
        parm.gzt = parm.origin[VZ];
        parm.distance = 1;

        parm.extraYawAngle   = parm.yawAngleOffset   = sky->def->rotate[0];
        parm.extraPitchAngle = parm.pitchAngleOffset = sky->def->rotate[1];
        parm.inter = inter;
        parm.mf = sky->model;
        parm.alwaysInterpolate = true;
        Rend_ModelSetFrame(sky->model, sky->frame);
        parm.yaw = sky->yaw;
        for(int c = 0; c < 4; ++c)
        {
            parm.ambientColor[c] = sky->def->color[c];
        }
        parm.vLightListIdx = 0;
        parm.shineTranslateWithViewerPos = true;

        Rend_RenderModel(&parm);
    }

    // We don't want that anything interferes with what was drawn.
    //glClear(GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// Look up the precalculated vertex.
static inline skyvertex_t *skyVertex(int r, int c)
{
    return hemisphereVerts + (r * skyColumns + c % skyColumns);
}

static void renderHemisphereCap()
{
    // Use the appropriate color.
    glColor3fv(rs.capColor.rgb);

    // Draw the cap.
    glBegin(GL_TRIANGLE_FAN);
    for(int c = 0; c < skyColumns; ++c)
    {
        glVertex3fv((GLfloat const *)skyVertex(0, c)->pos);
    }
    glEnd();

    // Are we doing a colored fadeout?
    if(!rs.fadeout) return;

    // We must fill the background for the top row since it'll be
    // partially translucent.
    glBegin(GL_TRIANGLE_STRIP);
    glVertex3fv((GLfloat const *)skyVertex(0, 0)->pos);
    int c = 0;
    for(; c < skyColumns; ++c)
    {
        // One step down.
        glVertex3fv((GLfloat const *)skyVertex(1, c)->pos);
        // And one step right.
        glVertex3fv((GLfloat const *)skyVertex(0, c + 1)->pos);
    }
    glVertex3fv((GLfloat const *)skyVertex(1, c)->pos);
    glEnd();
}

static void renderHemisphere()
{
#define WRITESKYVERTEX(r_, c_) { \
    svtx = skyVertex(r_, c_); \
    if(rs.texSize.width != 0) \
       glTexCoord2f((c_) / float(skyColumns), (r_) / float(skyRows)); \
    if(rs.fadeout) \
    { \
        if((r_) == 0) glColor4f(1, 1, 1, 0); \
        else          glColor3f(1, 1, 1); \
    } \
    else \
    { \
        if((r_) == 0) glColor3f(0, 0, 0); \
        else          glColor3f(1, 1, 1); \
    } \
    glVertex3fv((GLfloat const *)svtx->pos); \
}

    skyvertex_t const *svtx;
    for(int r = 0; r < skyRows; ++r)
    {
        glBegin(GL_TRIANGLE_STRIP);
        WRITESKYVERTEX(r, 0);
        WRITESKYVERTEX(r + 1, 0);
        for(int c = 1; c <= skyColumns; ++c)
        {
            WRITESKYVERTEX(r, c);
            WRITESKYVERTEX(r + 1, c);
        }
        glEnd();
    }
}

static void configureRenderHemisphereStateForLayer(int layer, hemispherecap_t setupCap)
{
    // Default state is no texture and no fadeout.
    rs.texSize.width = rs.texSize.height = 0;
    if(setupCap != HC_NONE)
        rs.fadeout = false;
    rs.texXFlip = true;

    if(renderTextures != 0)
    {
        Material *mat;

        if(renderTextures == 2)
        {
            mat = &App_Materials().find(de::Uri("System", Path("gray"))).material();
        }
        else
        {
            mat = Sky_LayerMaterial(layer);
            if(!mat)
            {
                mat = &App_Materials().find(de::Uri("System", Path("missing"))).material();
                rs.texXFlip = false;
            }
        }
        DENG_ASSERT(mat);

        MaterialSnapshot const &ms =
            mat->prepare(Sky_SphereMaterialSpec(Sky_LayerMasked(layer)));

        rs.texSize.width  = ms.texture(MTU_PRIMARY).generalCase().width();
        rs.texSize.height = ms.texture(MTU_PRIMARY).generalCase().height();
        if(rs.texSize.width && rs.texSize.height)
        {
            rs.texOffset = Sky_LayerOffset(layer);
            GL_BindTexture(&ms.texture(MTU_PRIMARY));
        }
        else
        {
            // Disable texturing.
            rs.texSize.width = rs.texSize.height = 0;
            GL_SetNoTexture();
        }

        if(setupCap != HC_NONE)
        {
            averagecolor_analysis_t const *avgLineColor = reinterpret_cast<averagecolor_analysis_t const *>
                    (ms.texture(MTU_PRIMARY).generalCase().analysisDataPointer((setupCap == HC_TOP? Texture::AverageTopColorAnalysis : Texture::AverageBottomColorAnalysis)));
            float const fadeoutLimit = Sky_LayerFadeoutLimit(layer);
            if(!avgLineColor) throw Error("configureRenderHemisphereStateForLayer", QString("Texture \"%1\" has no %2").arg(ms.texture(MTU_PRIMARY).generalCase().manifest().composeUri()).arg(setupCap == HC_TOP? "AverageTopColorAnalysis" : "AverageBottomColorAnalysis"));

            V3f_Copy(rs.capColor.rgb, avgLineColor->color.rgb);
            // Is the colored fadeout in use?
            rs.fadeout = (rs.capColor.red   >= fadeoutLimit ||
                          rs.capColor.green >= fadeoutLimit ||
                          rs.capColor.blue  >= fadeoutLimit);
        }
    }
    else
    {
        GL_SetNoTexture();
    }

    if(setupCap != HC_NONE && !rs.fadeout)
    {
        // Default color is black.
        V3f_Set(rs.capColor.rgb, 0, 0, 0);
    }
}

/// @param flags  @ref skySphereRenderFlags
static void renderHemisphere(int flags)
{
    int firstSkyLayer = Sky_FirstActiveLayer();
    bool const yflip = !!(flags & SKYHEMI_LOWER);
    hemispherecap_t cap = !!(flags & SKYHEMI_LOWER)? HC_BOTTOM : HC_TOP;

    // Rebuild the hemisphere model if necessary.
    rebuildHemisphere();

    if(yflip)
    {
        // The lower hemisphere must be flipped.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glScalef(1.0f, -1.0f, 1.0f);
    }

    // First render the cap and the background for fadeouts, if needed.
    configureRenderHemisphereStateForLayer(firstSkyLayer, cap);
    renderHemisphereCap();

    if(!(flags & SKYHEMI_JUST_CAP))
    {
        for(int i = firstSkyLayer; i <= MAX_SKY_LAYERS; ++i)
        {
            if(!Sky_LayerActive(i)) continue;

            if(i != firstSkyLayer)
            {
                configureRenderHemisphereStateForLayer(i, HC_NONE);
            }

            if(rs.texSize.width != 0)
            {
                glEnable(GL_TEXTURE_2D);
                glMatrixMode(GL_TEXTURE);
                glPushMatrix();
                glLoadIdentity();
                glTranslatef(rs.texOffset / rs.texSize.width, 0, 0);
                glScalef(1024.f / rs.texSize.width * (rs.texXFlip? 1 : -1), yflip? -1 : 1, 1);
                if(yflip) glTranslatef(0, -1, 0);
            }

            renderHemisphere();

            if(rs.texSize.width != 0)
            {
                glMatrixMode(GL_TEXTURE);
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);
            }
        }
    }

    if(yflip)
    {
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
}

static void destroyHemisphere()
{
    if(hemisphereVerts)
    {
        M_Free(hemisphereVerts);
        hemisphereVerts = 0;
    }
    numHemisphereVerts = 0;
}

/**
 * The top row (row 0) is the one that's faded out.
 * There must be at least 4 columns. The preferable number is 4n, where
 * n is 1, 2, 3... There should be at least two rows because the first
 * one is always faded.
 *
 * The total number of triangles per hemisphere can be calculated thus:
 *
 * Sum: rows * columns * 2 + (hemisphere)
 *      rows * 2 + (fadeout)
 *      rows - 2 (cap)
 */
static void constructHemisphere()
{
    float const maxSideAngle  = (float) PI / 2 * Sky_Height();
    float const horizonOffset = (float) PI / 2 * Sky_HorizonOffset();
    float const scale = 1;
    float realRadius, topAngle, sideAngle;
    int c, r;

    if(skyDetail < 1) skyDetail = 1;
    if(skyRows < 1) skyRows = 1;

    skyColumns = 4 * skyDetail;

    numHemisphereVerts = skyColumns * (skyRows + 1);
    hemisphereVerts = (skyvertex_t*) M_Realloc(hemisphereVerts, sizeof(*hemisphereVerts) * numHemisphereVerts);
    if(!hemisphereVerts) Con_Error("constructHemisphere: Failed (re)allocation of %lu bytes for hemisphere vertices.", (unsigned long) sizeof(*hemisphereVerts) * numHemisphereVerts);

    // Calculate the vertices.
    for(r = 0; r < skyRows + 1; ++r)
    {
        for(c = 0; c < skyColumns; ++c)
        {
            skyvertex_t *svtx = skyVertex(r, c);

            topAngle = ((c / float(skyColumns)) *2) * PI;
            sideAngle = horizonOffset + maxSideAngle * (skyRows - r) / float(skyRows);
            realRadius = scale * cos(sideAngle);
            svtx->pos[VX] = realRadius * cos(topAngle);
            svtx->pos[VY] = scale * sin(sideAngle); // The height.
            svtx->pos[VZ] = realRadius * sin(topAngle);
        }
    }
}

static void rebuildHemisphere()
{
    static bool firstBuild = true;
    static float oldHorizonOffset;
    static float oldHeight;

    // Rebuild our model if any paramaters have changed.
    if(firstBuild || Sky_HorizonOffset() != oldHorizonOffset)
    {
        oldHorizonOffset = Sky_HorizonOffset();
        needRebuildHemisphere = true;
    }
    if(firstBuild || Sky_Height() != oldHeight)
    {
        oldHeight = Sky_Height();
        needRebuildHemisphere = true;
    }
    firstBuild = false;

    if(!needRebuildHemisphere) return;

    // We have work to do...
    constructHemisphere();
    needRebuildHemisphere = false;
}

void Sky_Render()
{
    if(novideo || isDedicated || !initedOk) return;

    // Is there a sky to be rendered?
    if(!Sky_FirstActiveLayer()) return;

    if(usingFog) glEnable(GL_FOG);

    // If sky models have been inited, they will be used.
    if(!skyModelsInited || alwaysDrawSphere)
    {
        // We don't want anything written in the depth buffer.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        // Disable culling, all triangles face the viewer.
        glDisable(GL_CULL_FACE);

        // Setup a proper matrix.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(vOrigin[VX], vOrigin[VY], vOrigin[VZ]);
        glScalef(skyDistance, skyDistance, skyDistance);

        // Always draw both hemispheres.
        renderHemisphere(SKYHEMI_LOWER);
        renderHemisphere(SKYHEMI_UPPER);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        // Restore assumed default GL state.
        glEnable(GL_CULL_FACE);

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    // How about some 3D models?
    if(skyModelsInited)
    {
        // We don't want anything written in the depth buffer.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        renderSkyModels();

        // Restore assumed default GL state.
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    if(usingFog) glDisable(GL_FOG);
}

/// @note A CVar callback.
static void updateSphere()
{
    // Defer this task until render time, when we can be sure we are in correct thread.
    needRebuildHemisphere = true;
}
