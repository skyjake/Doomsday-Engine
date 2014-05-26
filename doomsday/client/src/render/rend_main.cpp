/** @file rend_main.cpp  World Map Renderer.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <de/GLState>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_resource.h"
#include "de_graphics.h"
#include "de_ui.h"
#include "clientapp.h"
#include "sys_system.h"
#include "ui/editors/rendererappearanceeditor.h"

#include "edit_bias.h" /// @todo remove me
#include "network/net_main.h" /// @todo remove me

#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "Texture"
#include "Face"
#include "world/map.h"
#include "world/lineowner.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "world/thinkers.h"
#include "BspLeaf"
#include "BspNode"
#include "Contact"
#include "ConvexSubspace"
#include "Hand"
#include "SectorCluster"
#include "Surface"
#include "BiasIllum"
#include "DrawLists"
#include "HueCircleVisual"
#include "LightDecoration"
//#include "Lumobj"
#include "Shard"
#include "SurfaceDecorator"
//#include "TriangleStripBuilder"
//#include "WallEdge"
#include "render/rend_main.h"
#include "render/blockmapvisual.h"
#include "render/billboard.h"
#include "render/vissprite.h"
#include "render/fx/bloom.h"
#include "render/fx/vignette.h"
#include "render/fx/lensflares.h"
#include "render/vr.h"
#include "gl/gl_texmanager.h"
#include "gl/sys_opengl.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <QtAlgorithms>
#include <QBitArray>

#include <de/vector1.h>
#include <de/libcore.h>
#include <de/concurrency.h>
#include <de/timer.h>

using namespace de;

// Surface (tangent-space) Vector Flags.
#define SVF_TANGENT             0x01
#define SVF_BITANGENT           0x02
#define SVF_NORMAL              0x04

/**
 * @defgroup soundOriginFlags  Sound Origin Flags
 * Flags for use with the sound origin debug display.
 * @ingroup flags
 */
///@{
#define SOF_SECTOR              0x01
#define SOF_PLANE               0x02
#define SOF_SIDE                0x04
///@}

void Rend_DrawBBox(Vector3d const &pos, coord_t w, coord_t l, coord_t h, float a,
    float const color[3], float alpha, float br, bool alignToBase = true);

void Rend_DrawArrow(Vector3d const &pos, float a, float s, float const color3f[3], float alpha);

D_CMD(OpenRendererAppearanceEditor);
D_CMD(LowRes);
D_CMD(MipMap);
D_CMD(TexReset);

int useBias; // Shadow Bias enabled? cvar

dd_bool usingFog; // Is the fog in use?
float fogColor[4];
float fieldOfView = 95.0f;
byte smoothTexAnim = true;

int renderTextures = true;
int renderWireframe = false;
int useMultiTexLights = true;
int useMultiTexDetails = true;

// Rendering paramaters for dynamic lights.
int dynlightBlend = 0;

Vector3f torchColor(1, 1, 1);
int torchAdditive = true;

int useShinySurfaces = true;

int useDynLights = true;
float dynlightFactor = .5f;
float dynlightFogBright = .15f;

int useGlowOnWalls = true;
float glowFactor = .8f;
float glowHeightFactor = 3; // Glow height as a multiplier.
int glowHeightMax = 100; // 100 is the default (0-1024).

int useShadows = true;
float shadowFactor = 1.2f;
int shadowMaxRadius = 80;
int shadowMaxDistance = 1000;

byte useLightDecorations = true; ///< cvar

float detailFactor = .5f;
float detailScale = 4;

int mipmapping = 5;
int filterUI   = 1;
int texQuality = TEXQ_BEST;

int ratioLimit = 0; // Zero if none.
dd_bool fillOutlines = true;
int useSmartFilter = 0; // Smart filter mode (cvar: 1=hq2x)
int filterSprites = true;
int texMagMode = 1; // Linear.
int texAniso = -1; // Use best.

dd_bool noHighResTex = false;
dd_bool noHighResPatches = false;
dd_bool highResWithPWAD = false;
byte loadExtAlways = false; // Always check for extres (cvar)

float texGamma = 0;

int glmode[6] = // Indexed by 'mipmapping'.
{
    GL_NEAREST,
    GL_LINEAR,
    GL_NEAREST_MIPMAP_NEAREST,
    GL_LINEAR_MIPMAP_NEAREST,
    GL_NEAREST_MIPMAP_LINEAR,
    GL_LINEAR_MIPMAP_LINEAR
};

Vector3d vOrigin;
float vang, vpitch;
float viewsidex, viewsidey;

byte freezeRLs;
int devRendSkyMode;
byte devRendSkyAlways;

// Ambient lighting, rAmbient is used within the renderer, ambientLight is
// used to store the value of the ambient light cvar.
// The value chosen for rAmbient occurs in Rend_UpdateLightModMatrix
// for convenience (since we would have to recalculate the matrix anyway).
int rAmbient, ambientLight;

int viewpw, viewph; // Viewport size, in pixels.
int viewpx, viewpy; // Viewpoint top left corner, in pixels.

float yfov;

int gameDrawHUD = 1; // Set to zero when we advise that the HUD should not be drawn

/**
 * Implements a pre-calculated LUT for light level limiting and range
 * compression offsets, arranged such that it may be indexed with a
 * light level value. Return value is an appropriate delta (considering
 * all applicable renderer properties) which has been pre-clamped such
 * that when summed with the original light value the result remains in
 * the normalized range [0..1].
 */
float lightRangeCompression;
float lightModRange[255];
byte devLightModRange;

float rendLightDistanceAttenuation = 924;
int rendLightAttenuateFixedColormap = 1;

float rendLightWallAngle = 1.2f; // Intensity of angle-based wall lighting.
byte rendLightWallAngleSmooth = true;

float rendSkyLight = .273f; // Intensity factor.
byte rendSkyLightAuto = true;

int rendMaxLumobjs; ///< Max lumobjs per viewer, per frame. @c 0= no maximum.

int extraLight; // Bumped light from gun blasts.
float extraLightDelta;

// Display list id for the active-textured bbox model.
DGLuint dlBBox;

/*
 * Debug/Development cvars:
 */

byte devMobjVLights;            ///< @c 1= Draw mobj vertex lighting vector.
int devMobjBBox;                ///< @c 1= Draw mobj bounding boxes.
int devPolyobjBBox;             ///< @c 1= Draw polyobj bounding boxes.

byte devVertexIndices;          ///< @c 1= Draw vertex indices.
byte devVertexBars;             ///< @c 1= Draw vertex position bars.

byte devDrawGenerators;         ///< @c 1= Draw active generators.
byte devSoundEmitters;          ///< @c 1= Draw sound emitters.
byte devSurfaceVectors;         ///< @c 1= Draw tangent space vectors for surfaces.
byte devNoTexFix;               ///< @c 1= Draw "missing" rather than fix materials.

byte devSectorIndices;          ///< @c 1= Draw sector indicies.
byte devThinkerIds;             ///< @c 1= Draw (mobj) thinker indicies.

byte rendInfoLums;              ///< @c 1= Print lumobj debug info to the console.
byte devDrawLums;               ///< @c 1= Draw lumobjs origins.

byte devLightGrid;              ///< @c 1= Draw lightgrid debug visual.
float devLightGridSize = 1.5f;  ///< Lightgrid debug visual size factor.

static void drawMobjBoundingBoxes(Map &map);
static void drawSoundEmitters(Map &map);
static void drawGenerators(Map &map);
static void drawSurfaceTangentVectors(Map &map);
static void drawBiasEditingVisuals(Map &map);
static void drawLumobjs(Map &map);
static void drawSectors(Map &map);
static void drawThinkers(Map &map);
static void drawVertexes(Map &map);

// Draw state:
static Vector3d eyeOrigin; // Viewer origin.
//static ConvexSubspace *curSubspace; // Subspace currently being drawn.
static bool firstSubspace; // No range checking for the first one.

static void scheduleFullLightGridUpdate()
{
    if(App_WorldSystem().hasMap())
    {
        Map &map = App_WorldSystem().map();
        if(map.hasLightGrid())
            map.lightGrid().scheduleFullUpdate();
    }
}

static int unlinkMobjLumobjWorker(thinker_t *th, void *)
{
    Mobj_UnlinkLumobjs(reinterpret_cast<mobj_t *>(th));
    return false; // Continue iteration.
}

static void unlinkMobjLumobjs()
{
    if(App_WorldSystem().hasMap())
    {
        Map &map = App_WorldSystem().map();
        map.thinkers().iterate(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1,
                               unlinkMobjLumobjWorker);
    }
}

static void fieldOfViewChanged()
{
    if(vrCfg().mode() == VRConfig::OculusRift)
    {
        if(Con_GetFloat("rend-vr-rift-fovx") != fieldOfView)
            Con_SetFloat("rend-vr-rift-fovx", fieldOfView);
    }
    else
    {
        if(Con_GetFloat("rend-vr-nonrift-fovx") != fieldOfView)
            Con_SetFloat("rend-vr-nonrift-fovx", fieldOfView);
    }
}

static void detailFactorChanged()
{
    App_ResourceSystem().releaseGLTexturesByScheme("Details");
}

static void loadExtAlwaysChanged()
{
    GL_TexReset();
}

static void useSmartFilterChanged()
{
    GL_TexReset();
}

static void texGammaChanged()
{
    R_BuildTexGammaLut();
    GL_TexReset();
    LOG_GL_MSG("Texture gamma correction set to %f") << texGamma;
}

static void mipmappingChanged()
{
    GL_TexReset();
}

static void texQualityChanged()
{
    GL_TexReset();
}

void Rend_Register()
{
    C_VAR_INT   ("rend-bias",                       &useBias,                       0, 0, 1);
    C_VAR_FLOAT2("rend-camera-fov",                 &fieldOfView,                   0, 1, 179, fieldOfViewChanged);

    C_VAR_FLOAT ("rend-glow",                       &glowFactor,                    0, 0, 2);
    C_VAR_INT   ("rend-glow-height",                &glowHeightMax,                 0, 0, 1024);
    C_VAR_FLOAT ("rend-glow-scale",                 &glowHeightFactor,              0, 0.1f, 10);
    C_VAR_INT   ("rend-glow-wall",                  &useGlowOnWalls,                0, 0, 1);

    C_VAR_BYTE  ("rend-info-lums",                  &rendInfoLums,                  0, 0, 1);

    C_VAR_INT2  ("rend-light",                      &useDynLights,                  0, 0, 1, unlinkMobjLumobjs);
    C_VAR_INT2  ("rend-light-ambient",              &ambientLight,                  0, 0, 255, Rend_UpdateLightModMatrix);
    C_VAR_FLOAT ("rend-light-attenuation",          &rendLightDistanceAttenuation,  CVF_NO_MAX, 0, 0);
    C_VAR_INT   ("rend-light-blend",                &dynlightBlend,                 0, 0, 2);
    C_VAR_FLOAT ("rend-light-bright",               &dynlightFactor,                0, 0, 1);
    C_VAR_FLOAT2("rend-light-compression",          &lightRangeCompression,         0, -1, 1, Rend_UpdateLightModMatrix);
    C_VAR_BYTE  ("rend-light-decor",                &useLightDecorations,           0, 0, 1);
    C_VAR_FLOAT ("rend-light-fog-bright",           &dynlightFogBright,             0, 0, 1);
    C_VAR_INT   ("rend-light-multitex",             &useMultiTexLights,             0, 0, 1);
    C_VAR_INT   ("rend-light-num",                  &rendMaxLumobjs,                CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT2("rend-light-sky",                  &rendSkyLight,                  0, 0, 1, scheduleFullLightGridUpdate);
    C_VAR_BYTE2 ("rend-light-sky-auto",             &rendSkyLightAuto,              0, 0, 1, scheduleFullLightGridUpdate);
    C_VAR_FLOAT ("rend-light-wall-angle",           &rendLightWallAngle,            CVF_NO_MAX, 0, 0);
    C_VAR_BYTE  ("rend-light-wall-angle-smooth",    &rendLightWallAngleSmooth,      0, 0, 1);

    C_VAR_BYTE  ("rend-map-material-precache",      &precacheMapMaterials,          0, 0, 1);

    C_VAR_INT   ("rend-shadow",                     &useShadows,                    0, 0, 1);
    C_VAR_FLOAT ("rend-shadow-darkness",            &shadowFactor,                  0, 0, 2);
    C_VAR_INT   ("rend-shadow-far",                 &shadowMaxDistance,             CVF_NO_MAX, 0, 0);
    C_VAR_INT   ("rend-shadow-radius-max",          &shadowMaxRadius,               CVF_NO_MAX, 0, 0);

    C_VAR_INT   ("rend-tex",                        &renderTextures,                CVF_NO_ARCHIVE, 0, 2);
    C_VAR_BYTE  ("rend-tex-anim-smooth",            &smoothTexAnim,                 0, 0, 1);
    C_VAR_INT   ("rend-tex-detail",                 &r_detail,                      0, 0, 1);
    C_VAR_INT   ("rend-tex-detail-multitex",        &useMultiTexDetails,            0, 0, 1);
    C_VAR_FLOAT ("rend-tex-detail-scale",           &detailScale,                   CVF_NO_MIN | CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT2("rend-tex-detail-strength",        &detailFactor,                  0, 0, 5, detailFactorChanged);
    C_VAR_BYTE2 ("rend-tex-external-always",        &loadExtAlways,                 0, 0, 1, loadExtAlwaysChanged);
    C_VAR_INT   ("rend-tex-filter-anisotropic",     &texAniso,                      0, -1, 4);
    C_VAR_INT   ("rend-tex-filter-mag",             &texMagMode,                    0, 0, 1);
    C_VAR_INT2  ("rend-tex-filter-smart",           &useSmartFilter,                0, 0, 1, useSmartFilterChanged);
    C_VAR_INT   ("rend-tex-filter-sprite",          &filterSprites,                 0, 0, 1);
    C_VAR_INT   ("rend-tex-filter-ui",              &filterUI,                      0, 0, 1);
    C_VAR_FLOAT2("rend-tex-gamma",                  &texGamma,                      0, 0, 1, texGammaChanged);
    C_VAR_INT2  ("rend-tex-mipmap",                 &mipmapping,                    CVF_PROTECTED, 0, 5, mipmappingChanged);
    C_VAR_INT2  ("rend-tex-quality",                &texQuality,                    0, 0, 8, texQualityChanged);
    C_VAR_INT   ("rend-tex-shiny",                  &useShinySurfaces,              0, 0, 1);

    C_VAR_BYTE  ("rend-bias-grid-debug",            &devLightGrid,                  CVF_NO_ARCHIVE, 0, 1);
    C_VAR_FLOAT ("rend-bias-grid-debug-size",       &devLightGridSize,              0,              .1f, 100);
    C_VAR_BYTE  ("rend-dev-blockmap-debug",         &bmapShowDebug,                 CVF_NO_ARCHIVE, 0, 4);
    C_VAR_FLOAT ("rend-dev-blockmap-debug-size",    &bmapDebugSize,                 CVF_NO_ARCHIVE, .1f, 100);
    C_VAR_INT   ("rend-dev-cull-leafs",             &devNoCulling,                  CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-freeze",                 &freezeRLs,                     CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-generator-show-indices", &devDrawGenerators,             CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-light-mod",              &devLightModRange,              CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-lums",                   &devDrawLums,                   CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT   ("rend-dev-mobj-bbox",              &devMobjBBox,                   CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-mobj-show-vlights",      &devMobjVLights,                CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT   ("rend-dev-polyobj-bbox",           &devPolyobjBBox,                CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-sector-show-indices",    &devSectorIndices,              CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT   ("rend-dev-sky",                    &devRendSkyMode,                CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-sky-always",             &devRendSkyAlways,              CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-soundorigins",           &devSoundEmitters,              CVF_NO_ARCHIVE, 0, 7);
    C_VAR_BYTE  ("rend-dev-surface-show-vectors",   &devSurfaceVectors,             CVF_NO_ARCHIVE, 0, 7);
    C_VAR_BYTE  ("rend-dev-thinker-ids",            &devThinkerIds,                 CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-tex-showfix",            &devNoTexFix,                   CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-vertex-show-bars",       &devVertexBars,                 CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-vertex-show-indices",    &devVertexIndices,              CVF_NO_ARCHIVE, 0, 1);

    C_CMD       ("rendedit",    "",     OpenRendererAppearanceEditor);

    C_CMD_FLAGS ("lowres",      "",     LowRes, CMDF_NO_DEDICATED);
    C_CMD_FLAGS ("mipmap",      "i",    MipMap, CMDF_NO_DEDICATED);
    C_CMD_FLAGS ("texreset",    "",     TexReset, CMDF_NO_DEDICATED);
    C_CMD_FLAGS ("texreset",    "s",    TexReset, CMDF_NO_DEDICATED);

    BiasIllum::consoleRegister();
    LightDecoration::consoleRegister();
    LightGrid::consoleRegister();
    Lumobj::consoleRegister();
    Sky::consoleRegister();
    Rend_ModelRegister();
    Rend_ParticleRegister();
    Generator::consoleRegister();
    Rend_RadioRegister();
    Rend_SpriteRegister();
    LensFx_Register();
    fx::Bloom::consoleRegister();
    fx::Vignette::consoleRegister();
    fx::LensFlares::consoleRegister();
    BiasSurface::consoleRegister();
    VR_ConsoleRegister();
}

void Rend_ReportWallSectionDrawn(Line &line)
{
    // Already been here?
    int playerNum = viewPlayer - ddPlayers;
    if(line.isMappedByPlayer(playerNum)) return;

    // Mark as drawn.
    line.markMappedByPlayer(playerNum);

    // Send a status report.
    if(gx.HandleMapObjectStatusReport)
    {
        gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED, line.indexInMap(),
                                       DMU_LINE, &playerNum);
    }
}

void Rend_Init()
{
    C_Init();
}

void Rend_Shutdown()
{
    ClientApp::renderSystem().clearDrawLists();
}

/// World/map renderer reset.
void Rend_Reset()
{
    R_ClearViewData();
    if(App_WorldSystem().hasMap())
    {
        App_WorldSystem().map().removeAllLumobjs();
    }
    if(dlBBox)
    {
        GL_DeleteLists(dlBBox, 1);
        dlBBox = 0;
    }
}

bool Rend_IsMTexLights()
{
    return IS_MTEX_LIGHTS;
}

bool Rend_IsMTexDetails()
{
    return IS_MTEX_DETAILS;
}

float Rend_FieldOfView()
{
    if(vrCfg().mode() == VRConfig::OculusRift)
    {
        // fieldOfView = VR::riftFovX(); // Update for culling
        // return VR::riftFovX();
        return fieldOfView;
    }
    else
    {
        float widescreenCorrection = float(viewpw)/float(viewph) / (4.f / 3.f);
        widescreenCorrection = (1 + 2 * widescreenCorrection) / 3;
        return de::clamp(1.f, widescreenCorrection * fieldOfView, 179.f);
    }
}

Matrix4f Rend_GetModelViewMatrix(int consoleNum, bool useAngles)
{
    viewdata_t const *viewData = R_ViewData(consoleNum);

    vOrigin = viewData->current.origin.xzy();
    vang    = viewData->current.angle() / (float) ANGLE_MAX * 360 - 90; // head tracking included
    vpitch  = viewData->current.pitch * 85.0 / 110.0;

    Matrix4f modelView;

    if(useAngles)
    {
        float yaw   = vang;
        float pitch = vpitch;
        float roll  = 0;

        /// @todo Elevate roll angle use into viewer_t, and maybe all the way up into player
        /// model.

        /*
         * Pitch and yaw can be taken directly from the head tracker, as the game is aware of
         * these values and is syncing with them independently (however, game has more
         * latency).
         */
        if((vrCfg().mode() == VRConfig::OculusRift) && vrCfg().oculusRift().isReady())
        {
            Vector3f const pry = vrCfg().oculusRift().headOrientation();

            // Use angles directly from the Rift for best response.
            roll  = -radianToDegree(pry[1]);
            pitch =  radianToDegree(pry[0]);

        }

        modelView = Matrix4f::rotate(roll,  Vector3f(0, 0, 1)) *
                    Matrix4f::rotate(pitch, Vector3f(1, 0, 0)) *
                    Matrix4f::rotate(yaw,   Vector3f(0, 1, 0));
    }

    return (modelView *
            Matrix4f::scale(Vector3f(1.0f, 1.2f, 1.0f)) * // This is the aspect correction.
            Matrix4f::translate(-vOrigin));
}

void Rend_ModelViewMatrix(bool useAngles)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(Rend_GetModelViewMatrix(viewPlayer - ddPlayers, useAngles).values());
}

Vector3d const Rend_ViewerOrigin()
{
    return eyeOrigin;
}

static inline double viewFacingDot(Vector2d const &v1, Vector2d const &v2)
{
    // The dot product.
    return (v1.y - v2.y) * (v1.x - vOrigin.x) + (v2.x - v1.x) * (v1.y - vOrigin.z);
}

float Rend_ExtraLightDelta()
{
    return extraLightDelta;
}

void Rend_ApplyTorchLight(Vector4f &color, float distance)
{
    ddplayer_t *ddpl = &viewPlayer->shared;

    // Disabled?
    if(!ddpl->fixedColorMap) return;

    // Check for torch.
    if(!rendLightAttenuateFixedColormap || distance < 1024)
    {
        // Colormap 1 is the brightest. I'm guessing 16 would be
        // the darkest.
        float d = (16 - ddpl->fixedColorMap) / 15.0f;
        if(rendLightAttenuateFixedColormap)
        {
            d *= (1024 - distance) / 1024.0f;
        }

        if(torchAdditive)
        {
            color += torchColor * d;
        }
        else
        {
            color += ((color * torchColor) - color) * d;
        }
    }
}

void Rend_ApplyTorchLight(float *color3, float distance)
{
    Vector4f tmp(color3, 0);
    Rend_ApplyTorchLight(tmp, distance);
    for(int i = 0; i < 3; ++i)
    {
        color3[i] = tmp[i];
    }
}

float Rend_AttenuateLightLevel(float distToViewer, float lightLevel)
{
    if(distToViewer > 0 && rendLightDistanceAttenuation > 0)
    {
        float real = lightLevel -
            (distToViewer - 32) / rendLightDistanceAttenuation *
                (1 - lightLevel);

        float minimum = de::max(0.f, de::squared(lightLevel) + (lightLevel - .63f) * .5f);
        if(real < minimum)
            real = minimum; // Clamp it.

        return de::min(real, 1.f);
    }

    return lightLevel;
}

float Rend_ShadowAttenuationFactor(coord_t distance)
{
    if(shadowMaxDistance > 0 && distance > 3 * shadowMaxDistance / 4)
    {
        return (shadowMaxDistance - distance) / (shadowMaxDistance / 4);
    }
    return 1;
}

static Vector3f skyLightColor;
static Vector3f oldSkyAmbientColor(-1.f, -1.f, -1.f);
static float oldRendSkyLight = -1;

bool Rend_SkyLightIsEnabled()
{
    return rendSkyLight > .001f;
}

Vector3f Rend_SkyLightColor()
{
    if(Rend_SkyLightIsEnabled())
    {
        Vector3f const &ambientColor = theSky->ambientColor();

        if(rendSkyLight != oldRendSkyLight ||
           !INRANGE_OF(ambientColor.x, oldSkyAmbientColor.x, .001f) ||
           !INRANGE_OF(ambientColor.y, oldSkyAmbientColor.y, .001f) ||
           !INRANGE_OF(ambientColor.z, oldSkyAmbientColor.z, .001f))
        {
            skyLightColor = ambientColor;
            R_AmplifyColor(skyLightColor);

            // Apply the intensity factor cvar.
            for(int i = 0; i < 3; ++i)
            {
                skyLightColor[i] = skyLightColor[i] + (1 - rendSkyLight) * (1.f - skyLightColor[i]);
            }

            // When the sky light color changes we must update the light grid.
            scheduleFullLightGridUpdate();
            oldSkyAmbientColor = ambientColor;
        }

        oldRendSkyLight = rendSkyLight;
        return skyLightColor;
    }

    return Vector3f(1, 1, 1);
}

Vector4f Rend_AmbientLightColor(Sector const &sector)
{
    if(Rend_SkyLightIsEnabled() && sector.hasSkyMaskedPlane())
    {
        return Vector4f(Rend_SkyLightColor(), sector.lightLevel());
    }

    // A non-skylight sector (i.e., everything else!)
    // Return the sector's ambient light color.
    return Vector4f(sector.lightColor(), sector.lightLevel());
}

Vector3f Rend_LuminousColor(Vector3f const &color, float light)
{
    light = de::clamp(0.f, light, 1.f) * dynlightFactor;

    // In fog additive blending is used; the normal fog color is way too bright.
    if(usingFog) light *= dynlightFogBright;

    // Multiply light with (ambient) color.
    return color * light;
}

coord_t Rend_PlaneGlowHeight(float intensity)
{
    return de::clamp<double>(0, GLOW_HEIGHT_MAX * intensity * glowHeightFactor, glowHeightMax);
}

Material *Rend_ChooseMapSurfaceMaterial(Surface const &surface)
{
    switch(renderTextures)
    {
    case 0: // No texture mode.
    case 1: // Normal mode.
        if(devNoTexFix && surface.hasFixMaterial())
        {
            // Missing material debug mode -- use special "missing" material.
            return &ClientApp::resourceSystem().material(de::Uri("System", Path("missing")));
        }

        // Use the surface-bound material.
        return surface.materialPtr();

    case 2: // Lighting debug mode.
        if(surface.hasMaterial() && !(!devNoTexFix && surface.hasFixMaterial()))
        {
            if(!surface.hasSkyMaskedMaterial() || devRendSkyMode)
            {
                // Use the special "gray" material.
                return &ClientApp::resourceSystem().material(de::Uri("System", Path("gray")));
            }
        }
        break;

    default: break;
    }

    // No material, then.
    return 0;
}

void Rend_DivPosCoords(WorldVBuf &vbuf, WorldVBuf::Index *dst, Vector3f const *src,
    WallEdgeSection const &leftSection, WallEdgeSection const &rightSection)
{
    DENG2_ASSERT(dst != 0 && src != 0);

    int const numR = 3 + rightSection.divisionCount();
    int const numL = 3 + leftSection.divisionCount();

    if(numR + numL == 6) return; // Nothing to do.

    // Right fan:
    vbuf[dst[numL + 0]].pos = src[0];
    vbuf[dst[numL + 1]].pos = src[3];
    vbuf[dst[numL + numR - 1]].pos = src[2];

    for(int n = 0; n < rightSection.divisionCount(); ++n)
    {
        vbuf[dst[numL + 2 + n]].pos = rightSection[rightSection.lastDivision() - n].origin();
    }

    // Left fan:
    vbuf[dst[0]].pos = src[3];
    vbuf[dst[1]].pos = src[0];
    vbuf[dst[numL - 1]].pos = src[1];

    for(int n = 0; n < leftSection.divisionCount(); ++n)
    {
        vbuf[dst[2 + n]].pos = leftSection[leftSection.firstDivision() + n].origin();
    }
}

void Rend_DivTexCoords(WorldVBuf &vbuf, WorldVBuf::Index *dst, Vector2f const *src,
    WallEdgeSection const &leftSection, WallEdgeSection const &rightSection,
    WorldVBuf::TC tc)
{
    DENG2_ASSERT(dst != 0 && src != 0);

    int const numR = 3 + rightSection.divisionCount();
    int const numL = 3 + leftSection.divisionCount();

    if(numR + numL == 6) return; // Nothing to do.

    // Right fan:
    vbuf[dst[numL + 0]].texCoord[tc] = src[0];
    vbuf[dst[numL + 1]].texCoord[tc] = src[3];
    vbuf[dst[numL + numR-1]].texCoord[tc] = src[2];

    for(int n = 0; n < rightSection.divisionCount(); ++n)
    {
        WallEdgeSection::Event const &icpt = rightSection[rightSection.lastDivision() - n];
        vbuf[dst[numL + 2 + n]].texCoord[tc].x = src[3].x;
        vbuf[dst[numL + 2 + n]].texCoord[tc].y = src[2].y + (src[3].y - src[2].y) * icpt.distance();
    }

    // Left fan:
    vbuf[dst[0]].texCoord[tc] = src[3];
    vbuf[dst[1]].texCoord[tc] = src[0];
    vbuf[dst[numL - 1]].texCoord[tc] = src[1];

    for(int n = 0; n < leftSection.divisionCount(); ++n)
    {
        WallEdgeSection::Event const &icpt = leftSection[leftSection.firstDivision() + n];
        vbuf[dst[2 + n]].texCoord[tc].x = src[0].x;
        vbuf[dst[2 + n]].texCoord[tc].y = src[0].y + (src[1].y - src[0].y) * icpt.distance();
    }
}

void Rend_DivColorCoords(WorldVBuf &vbuf, WorldVBuf::Index *dst, Vector4f const *src,
    WallEdgeSection const &leftSection, WallEdgeSection const &rightSection)
{
    DENG2_ASSERT(dst != 0 && src != 0);

    int const numR = 3 + rightSection.divisionCount();
    int const numL = 3 + leftSection.divisionCount();

    if(numR + numL == 6) return; // Nothing to do.

    // Right fan:
    vbuf[dst[numL + 0]].rgba = src[0];
    vbuf[dst[numL + 1]].rgba = src[3];
    vbuf[dst[numL + numR-1]].rgba = src[2];

    for(int n = 0; n < rightSection.divisionCount(); ++n)
    {
        WallEdgeSection::Event const &icpt = rightSection[rightSection.lastDivision() - n];
        vbuf[dst[numL + 2 + n]].rgba = src[2] + (src[3] - src[2]) * icpt.distance();
    }

    // Left fan:
    vbuf[dst[0]].rgba = src[3];
    vbuf[dst[1]].rgba = src[0];
    vbuf[dst[numL - 1]].rgba = src[1];

    for(int n = 0; n < leftSection.divisionCount(); ++n)
    {
        WallEdgeSection::Event const &icpt = leftSection[leftSection.firstDivision() + n];
        vbuf[dst[2 + n]].rgba = src[0] + (src[1] - src[0]) * icpt.distance();
    }
}

int RIT_FirstDynlightIterator(TexProjection const *dyn, void *parameters)
{
    TexProjection const **ptr = (TexProjection const **)parameters;
    *ptr = dyn;
    return 1; // Stop iteration.
}

/**
 * This doesn't create a rendering primitive but a vissprite! The vissprite
 * represents the masked poly and will be rendered during the rendering
 * of sprites. This is necessary because all masked polygons must be
 * rendered back-to-front, or there will be alpha artifacts along edges.
 */
void Rend_AddMaskedPoly(Vector3f const *rvertices, Vector4f const *rcolors,
    coord_t wallLength, MaterialVariant *material, Vector2f const &materialOrigin,
    blendmode_t blendMode, uint lightListIdx, float glow)
{
    vissprite_t *vis = R_NewVisSprite(VSPR_MASKED_WALL);

    vis->origin   = (rvertices[0] + rvertices[3]) / 2;
    vis->distance = Rend_PointDist2D(vis->origin);

    VS_WALL(vis)->texOffset[0] = materialOrigin[VX];
    VS_WALL(vis)->texOffset[1] = materialOrigin[VY];

    // Masked walls are sometimes used for special effects like arcs, cobwebs
    // and bottoms of sails. In order for them to look right, we need to disable
    // texture wrapping on the horizontal axis (S).
    //
    // Most masked walls need wrapping, though. What we need to do is look at
    // the texture coordinates and see if they require texture wrapping.

    if(renderTextures)
    {
        MaterialSnapshot const &ms = material->prepare();
        int wrapS = GL_REPEAT, wrapT = GL_REPEAT;

        VS_WALL(vis)->texCoord[0][VX] = VS_WALL(vis)->texOffset[0] / ms.width();
        VS_WALL(vis)->texCoord[1][VX] = VS_WALL(vis)->texCoord[0][VX] + wallLength / ms.width();
        VS_WALL(vis)->texCoord[0][VY] = VS_WALL(vis)->texOffset[1] / ms.height();
        VS_WALL(vis)->texCoord[1][VY] = VS_WALL(vis)->texCoord[0][VY] +
                (rvertices[3].z - rvertices[0].z) / ms.height();

        if(!ms.isOpaque())
        {
            if(!(VS_WALL(vis)->texCoord[0][VX] < 0 || VS_WALL(vis)->texCoord[0][VX] > 1 ||
                 VS_WALL(vis)->texCoord[1][VX] < 0 || VS_WALL(vis)->texCoord[1][VX] > 1))
            {
                // Visible portion is within the actual [0..1] range.
                wrapS = GL_CLAMP_TO_EDGE;
            }

            // Clamp on the vertical axis if the coords are in the normal [0..1] range.
            if(!(VS_WALL(vis)->texCoord[0][VY] < 0 || VS_WALL(vis)->texCoord[0][VY] > 1 ||
                 VS_WALL(vis)->texCoord[1][VY] < 0 || VS_WALL(vis)->texCoord[1][VY] > 1))
            {
                wrapT = GL_CLAMP_TO_EDGE;
            }
        }

        // Choose a specific variant for use as a middle wall section.
        material = material->generalCase()
                       .chooseVariant(Rend_MapSurfaceMaterialSpec(wrapS, wrapT),
                                      true /*can create variant*/);
    }

    VS_WALL(vis)->material = material;
    VS_WALL(vis)->blendMode = blendMode;

    for(int i = 0; i < 4; ++i)
    {
        VS_WALL(vis)->vertices[i].pos[VX] = rvertices[i].x;
        VS_WALL(vis)->vertices[i].pos[VY] = rvertices[i].y;
        VS_WALL(vis)->vertices[i].pos[VZ] = rvertices[i].z;

        for(int c = 0; c < 4; ++c)
        {
            /// @todo Do not clamp here.
            VS_WALL(vis)->vertices[i].color[c] = de::clamp(0.f, rcolors[i][c], 1.f);
        }
    }

    /// @todo Semitransparent masked polys arn't lit atm
    if(glow < 1 && lightListIdx && numTexUnits > 1 && envModAdd &&
       !(rcolors[0].w < 1))
    {
        TexProjection const *dyn = 0;

        // The dynlights will have already been sorted so that the brightest
        // and largest of them is first in the list. So grab that one.
        Rend_IterateProjectionList(lightListIdx, RIT_FirstDynlightIterator, (void *)&dyn);

        VS_WALL(vis)->modTex = dyn->texture;
        VS_WALL(vis)->modTexCoord[0][0] = dyn->topLeft.x;
        VS_WALL(vis)->modTexCoord[0][1] = dyn->topLeft.y;
        VS_WALL(vis)->modTexCoord[1][0] = dyn->bottomRight.x;
        VS_WALL(vis)->modTexCoord[1][1] = dyn->bottomRight.y;
        for(int c = 0; c < 4; ++c)
        {
            VS_WALL(vis)->modColor[c] = dyn->color[c];
        }
    }
    else
    {
        VS_WALL(vis)->modTex = 0;
    }
}

bool Rend_MustDrawAsVissprite(bool forceOpaque, bool skyMasked, float opacity,
    blendmode_t blendmode, MaterialSnapshot const &ms)
{
    return (!forceOpaque && !skyMasked && (!ms.isOpaque() || opacity < 1 || blendmode > 0));
}

void Rend_PrepareWallSectionVissprite(ConvexSubspace &subspace,
    Vector4f const &ambientLight, Vector3f const &surfaceColor, float glowing,
    float opacity, blendmode_t blendmode,
    Vector2f const &matOrigin, MaterialSnapshot const &matSnapshot,
    uint lightListIdx,
    float surfaceLightLevelDL, float surfaceLightLevelDR, Matrix3f const &surfaceTangentMatrix,
    WallEdgeSection const *leftSection, WallEdgeSection const *rightSection,
    Vector3f const *surfaceColor2)
{
    // Only wall sections can presently be prepared as vissprites.
    DENG2_ASSERT(leftSection != 0 && rightSection != 0);

    DENG2_ASSERT(!leftSection->divisionCount() && !rightSection->divisionCount());
    DENG2_ASSERT(!matSnapshot.material().isSkyMasked());
    DENG2_ASSERT(!matSnapshot.isOpaque() || opacity < 1 || blendmode > 0);

    SectorCluster &cluster = subspace.cluster();

    Vector3f const posCoords[4] = {
        Vector3f( leftSection->bottom().origin()),
        Vector3f( leftSection->top   ().origin()),
        Vector3f(rightSection->bottom().origin()),
        Vector3f(rightSection->top   ().origin())
    };

    // Light this polygon.
    Vector4f colorCoords[4];
    if(levelFullBright || !(glowing < 1))
    {
        // Uniform color. Apply to all vertices.
        float ll = de::clamp(0.f, ambientLight.w + (levelFullBright? 1 : glowing), 1.f);
        Vector4f *colorIt = colorCoords;
        for(duint16 i = 0; i < 4; ++i, colorIt++)
        {
            colorIt->x = colorIt->y = colorIt->z = ll;
        }
    }
    else
    {
        // Non-uniform color.
        if(useBias)
        {
            MapElement &mapElement = leftSection->edge().hedge().mapElement();
            int const geomGroup    = leftSection->id() == WallEdge::WallMiddle? LineSide::Middle :
                                     leftSection->id() == WallEdge::WallBottom? LineSide::Bottom :
                                                                                LineSide::Top;

            Map &map = cluster.sector().map();
            BiasSurface &biasSurface = cluster.biasSurface(mapElement, geomGroup);

            // Apply the ambient light term from the grid (if available).
            if(map.hasLightGrid())
            {
                Vector3f const *posIt = posCoords;
                Vector4f *colorIt     = colorCoords;
                for(duint16 i = 0; i < 4; ++i, posIt++, colorIt++)
                {
                    *colorIt = map.lightGrid().evaluate(*posIt);
                }
            }

            // Apply bias light source contributions.
            biasSurface.light(posCoords, colorCoords, surfaceTangentMatrix,
                                       map.biasCurrentTime());

            // Apply surface glow.
            if(glowing > 0)
            {
                Vector4f const glow(glowing, glowing, glowing, 0);
                Vector4f *colorIt = colorCoords;
                for(duint16 i = 0; i < 4; ++i, colorIt++)
                {
                    *colorIt += glow;
                }
            }

            // Apply light range compression and clamp.
            Vector3f const *posIt = posCoords;
            Vector4f *colorIt     = colorCoords;
            for(duint16 i = 0; i < 4; ++i, posIt++, colorIt++)
            {
                for(int k = 0; k < 3; ++k)
                {
                    (*colorIt)[k] = de::clamp(0.f, (*colorIt)[k] + Rend_LightAdaptationDelta((*colorIt)[k]), 1.f);
                }
            }
        }
        else
        {
            // Apply the ambient light term.
            Vector4f const extraLight = Vector4f(0, 0, 0, Rend_ExtraLightDelta() + glowing);
            Vector4f const finalColor = ambientLight * Vector4f(surfaceColor, 1) + extraLight;
            colorCoords[1] = finalColor;
            colorCoords[3] = finalColor;
            if(!surfaceColor2)
            {
                colorCoords[0] = finalColor;
                colorCoords[2] = finalColor;
            }
            else
            {
                Vector4f const finalBottomColor = ambientLight * Vector4f(*surfaceColor2, 1) + extraLight;
                colorCoords[0] = finalBottomColor;
                colorCoords[2] = finalBottomColor;
            }

            // Apply the wall angle deltas to luminance.
            colorCoords[0].w += surfaceLightLevelDL;
            colorCoords[1].w += surfaceLightLevelDL;
            colorCoords[2].w += surfaceLightLevelDR;
            colorCoords[3].w += surfaceLightLevelDR;

            // Apply distance attenuation and compression to luminance.
            for(duint16 i = 0; i < 4; ++i)
            {
                float const dist = Rend_PointDist2D(posCoords[i]);
                colorCoords[i].w = Rend_AttenuateLightLevel(dist, colorCoords[i].w);

                // Apply range compression to the final luminance value.
                Rend_ApplyLightAdaptation(colorCoords[i].w);
            }

            // Multiply color with luminance and clamp (ignore alpha, we'll replace it soon).
            for(duint16 i = 0; i < 4; ++i)
            {
                float const luma = colorCoords[i].w;
                colorCoords[i].x = de::clamp(0.f, colorCoords[i].x * luma, 1.f);
                colorCoords[i].y = de::clamp(0.f, colorCoords[i].y * luma, 1.f);
                colorCoords[i].z = de::clamp(0.f, colorCoords[i].z * luma, 1.f);
            }
        }

        // Apply torch light?
        if(viewPlayer->shared.fixedColorMap)
        {
            Vector3f const *posIt = posCoords;
            Vector4f *colorIt     = colorCoords;
            for(duint16 i = 0; i < 4; ++i, colorIt++, posIt++)
            {
                Rend_ApplyTorchLight(*colorIt, Rend_PointDist2D(*posIt));
            }
        }
    }

    // Apply uniform alpha (overwritting luminance factors).
    Vector4f *colorIt = colorCoords;
    for(duint16 i = 0; i < 4; ++i, colorIt++)
    {
        colorIt->w = opacity;
    }

    coord_t const sectionWidth = de::abs(Vector2d(rightSection->edge().origin() - leftSection->edge().origin()).length());
    Rend_AddMaskedPoly(posCoords, colorCoords, sectionWidth,
                       &matSnapshot.materialVariant(), matOrigin,
                       blendmode, lightListIdx, glowing);
}

bool Rend_NearFadeOpacity(WallEdgeSection const &leftSection,
    WallEdgeSection const &rightSection, float &opacity)
{
    // Only middle wall section for a two-sided line is considered for near-fading.
    if(leftSection.id() != WallEdge::WallMiddle) return false;

    LineSide const &side = leftSection.edge().lineSide();
    if(side.considerOneSided()) return false;

    // Blocking lines only receive a near-fade when the viewplayer is a camera.
    if(side.line().isFlagged(DDLF_BLOCKING) &&
       !(viewPlayer->shared.flags & (DDPF_NOCLIP|DDPF_CAMERA)))
        return false;

    if(vOrigin.y < leftSection.bottom().z() || vOrigin.y > rightSection.top().z())
        return false;

    Line const &line         = side.line();
    mobj_t const *mo         = viewPlayer->shared.mo;
    coord_t linePoint[2]     = { line.fromOrigin().x, line.fromOrigin().y };
    coord_t lineDirection[2] = {  line.direction().x,  line.direction().y };
    vec2d_t result;
    double pos = V2d_ProjectOnLine(result, mo->origin, linePoint, lineDirection);

    if(!(pos > 0 && pos < 1))
        return false;

    coord_t const maxDistance = Mobj_Radius(*mo) * .8f;

    Vector2d delta = Vector2d(result) - Vector2d(mo->origin);
    coord_t distance = delta.length();

    if(de::abs(distance) > maxDistance)
        return false;

    if(distance > 0)
    {
        opacity = (opacity / maxDistance) * distance;
        opacity = de::clamp(0.f, opacity, 1.f);
    }

    return true;
}

static void markFrontFacingWalls(HEdge *hedge)
{
    if(!hedge || !hedge->hasMapElement()) return;
    LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
    // Which way is the line segment facing?
    seg.setFrontFacing(viewFacingDot(hedge->origin(), hedge->twin().origin()) >= 0);
}

static void markSubspaceFrontFacingWalls(ConvexSubspace &subspace)
{
    HEdge *base = subspace.poly().hedge();
    HEdge *hedge = base;
    do
    {
        markFrontFacingWalls(hedge);
    } while((hedge = &hedge->next()) != base);

    foreach(Mesh *mesh, subspace.extraMeshes())
    foreach(HEdge *hedge, mesh->hedges())
    {
        markFrontFacingWalls(hedge);
    }

    foreach(Polyobj *po, subspace.polyobjs())
    foreach(HEdge *hedge, po->mesh().hedges())
    {
        markFrontFacingWalls(hedge);
    }
}

static inline bool canOccludeEdgeBetweenPlanes(Plane &frontPlane, Plane const &backPlane)
{
    // Do not create an occlusion between two sky-masked planes.
    // Only because the open range does not account for the sky plane height? -ds
    return !(frontPlane.surface().hasSkyMaskedMaterial() &&
              backPlane.surface().hasSkyMaskedMaterial());
}

/**
 * Add angle clipper occlusion ranges for the edges of the current subspace.
 */
static void occludeSubspace(ConvexSubspace &subspace, bool frontFacing)
{
    SectorCluster &cluster = subspace.cluster();

    if(devNoCulling) return;
    if(P_IsInVoid(viewPlayer)) return;

    HEdge *base = subspace.poly().hedge();
    HEdge *hedge = base;
    do
    {
        // Edges without a line segment can never occlude.
        if(!hedge->hasMapElement())
            continue;

        LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();

        // Edges without line segment surface sections can never occlude.
        if(!seg.lineSide().hasSections())
            continue;

        // Only front-facing edges can occlude.
        if(frontFacing != seg.isFrontFacing())
            continue;

        // Occlusions should only happen where two sectors meet.
        if(!hedge->hasTwin() || !hedge->twin().hasFace() || !hedge->twin().face().hasMapElement())
            continue;

        SectorCluster &backCluster = hedge->twin().face().mapElementAs<ConvexSubspace>().cluster();

        // Determine the opening between the visual sector planes at this edge.
        coord_t openBottom;
        if(backCluster.visFloor().heightSmoothed() > cluster.visFloor().heightSmoothed())
        {
            openBottom = backCluster.visFloor().heightSmoothed();
        }
        else
        {
            openBottom = cluster.visFloor().heightSmoothed();
        }

        coord_t openTop;
        if(backCluster.visCeiling().heightSmoothed() < cluster.visCeiling().heightSmoothed())
        {
            openTop = backCluster.visCeiling().heightSmoothed();
        }
        else
        {
            openTop = cluster.visCeiling().heightSmoothed();
        }

        // Choose start and end vertexes so that it's facing forward.
        Vertex const &from = frontFacing? hedge->vertex() : hedge->twin().vertex();
        Vertex const &to   = frontFacing? hedge->twin().vertex() : hedge->vertex();

        // Does the floor create an occlusion?
        if(((openBottom > cluster.visFloor().heightSmoothed() && vOrigin.y <= openBottom)
            || (openBottom >  backCluster.visFloor().heightSmoothed() && vOrigin.y >= openBottom))
           && canOccludeEdgeBetweenPlanes(cluster.visFloor(), backCluster.visFloor()))
        {
            C_AddViewRelOcclusion(from.origin(), to.origin(), openBottom, false);
        }

        // Does the ceiling create an occlusion?
        if(((openTop < cluster.visCeiling().heightSmoothed() && vOrigin.y >= openTop)
            || (openTop <  backCluster.visCeiling().heightSmoothed() && vOrigin.y <= openTop))
           && canOccludeEdgeBetweenPlanes(cluster.visCeiling(), backCluster.visCeiling()))
        {
            C_AddViewRelOcclusion(from.origin(), to.origin(), openTop, true);
        }
    } while((hedge = &hedge->next()) != base);
}

/// If not front facing this is no-op.
static void clipFrontFacingWalls(HEdge *hedge)
{
    if(!hedge || !hedge->hasMapElement())
        return;

    LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
    if(seg.isFrontFacing())
    {
        if(!C_CheckRangeFromViewRelPoints(hedge->origin(), hedge->twin().origin()))
        {
            seg.setFrontFacing(false);
        }
    }
}

static void clipSubspaceFrontFacingWalls(ConvexSubspace &subspace)
{
    HEdge *base = subspace.poly().hedge();
    HEdge *hedge = base;
    do
    {
        clipFrontFacingWalls(hedge);
    } while((hedge = &hedge->next()) != base);

    foreach(Mesh *mesh, subspace.extraMeshes())
    foreach(HEdge *hedge, mesh->hedges())
    {
        clipFrontFacingWalls(hedge);
    }

    foreach(Polyobj *po, subspace.polyobjs())
    foreach(HEdge *hedge, po->mesh().hedges())
    {
        clipFrontFacingWalls(hedge);
    }
}

static int markGeneratorVisibleWorker(Generator *generator, void * /*context*/)
{
    R_ViewerGeneratorMarkVisible(*generator);
    return 0; // Continue iteration.
}

static int projectSpriteWorker(mobj_t &mo, void *context)
{
    ConvexSubspace *subspace = static_cast<ConvexSubspace *>(context);
    SectorCluster &cluster   = subspace->cluster();

    if(mo.addFrameCount != R_FrameCount())
    {
        mo.addFrameCount = R_FrameCount();

        R_ProjectSprite(&mo);

        // Hack: Sprites have a tendency to extend into the ceiling in
        // sky sectors. Here we will raise the skyfix dynamically, to make sure
        // that no sprites get clipped by the sky.

        if(cluster.visCeiling().surface().hasSkyMaskedMaterial())
        {
            if(Sprite *sprite = Mobj_Sprite(mo))
            {
                if(sprite->hasViewAngle(0))
                {
                    Material *material = sprite->viewAngle(0).material;
                    if(!(mo.dPlayer && (mo.dPlayer->flags & DDPF_CAMERA))
                       && mo.origin[VZ] <= cluster.visCeiling().heightSmoothed()
                       && mo.origin[VZ] >= cluster.visFloor().heightSmoothed())
                    {
                        Map::SkyPlane &skyCeiling = cluster.sector().map().skyCeiling();
                        coord_t visibleTop = mo.origin[VZ] + material->height();
                        if(visibleTop > skyCeiling.height())
                        {
                            // Raise skyfix ceiling.
                            skyCeiling.setHeight(visibleTop + 16/*leeway*/);
                        }
                    }
                }
            }
        }
    }

    return false; // Continue iteration.
}

static void drawSubspace(ConvexSubspace &subspace)
{
    DENG2_ASSERT(subspace.hasCluster());

    // Skip zero-volume subspaces. (Neighbors handle the angle clipper ranges).
    if(!subspace.cluster().hasWorldVolume())
        return;

    // Is this subspace visible?
    if(!firstSubspace && !C_IsPolyVisible(subspace.poly()))
        return;

    // Mark the leaf as visible for this frame.
    R_ViewerSubspaceMarkVisible(subspace);

    markSubspaceFrontFacingWalls(subspace);

    // Perform contact spreading for this map region.
    Sector &sector = subspace.sector();
    sector.map().spreadAllContacts(subspace.poly().aaBox());

    Rend_RadioSubspaceEdges(subspace);

    // Before clip testing lumobjs (for halos), range-occlude the back facing
    // edges. After testing, range-occlude the front facing edges. Done before
    // drawing wall sections so that opening occlusions cut out unnecessary
    // oranges.
    occludeSubspace(subspace, false /* back facing */);

    // Clip lumobjs in the subspace.
    foreach(Lumobj *lum, subspace.lumobjs()) R_ViewerClipLumobj(lum);

    occludeSubspace(subspace, true /* front facing */);

    clipSubspaceFrontFacingWalls(subspace);

    // In the situation where a subspace contains both lumobjs and a polyobj,
    // lumobjs must be clipped more carefully. Here we check if the line of sight
    // intersects any of the polyobj half-edges facing the viewer.
    if(subspace.polyobjCount())
    {
        foreach(Lumobj *lum, subspace.lumobjs()) R_ViewerClipLumobjBySight(lum, &subspace);
    }

    // Mark generators in the sector visible.
    if(useParticles)
    {
        sector.map().generatorListIterator(sector.indexInMap(), markGeneratorVisibleWorker);
    }

    // Sprites for this subspace have to be drawn.
    //
    // Must be done BEFORE the wall segments of this subspace are added to the
    // clipper. Otherwise the sprites would get clipped by them,and that wouldn't
    // be right.
    //
    // Must be done AFTER the lumobjs have been clipped as this affects the
    // projection of halos.
    R_SubspaceMobjContactIterator(subspace, projectSpriteWorker, &subspace);

    // Prepare shard geometries.
    subspace.cluster().prepareShards(subspace);

    // Draw all shard geometries.
    DrawLists &drawLists = ClientApp::renderSystem().drawLists();
    foreach(Shard const *shard, subspace.shards())
    {
        drawLists.find(shard->listSpec) << *shard;
    }

    // When the viewer is not in the void, we can angle-occlude the range defined by
    // the XY origins of the halfedge if the opening between floor and ceiling has
    // completely covered by opaque geometry.
    HEdge *base = subspace.poly().hedge();
    HEdge *hedge = base;
    do
    {
        if(hedge->hasMapElement())
        {
            LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
            if(seg.isFrontFacing() && seg.lineSide().hasSections())
            {
                Rend_ReportWallSectionDrawn(seg.line());
            }

            if(!P_IsInVoid(viewPlayer) && seg.isOpenRangeCovered())
            {
                C_AddRangeFromViewRelPoints(hedge->origin(), hedge->twin().origin());
            }
        }
    } while((hedge = &hedge->next()) != base);

    foreach(Mesh *mesh, subspace.extraMeshes())
    foreach(HEdge *hedge, mesh->hedges())
    {
        if(hedge->hasMapElement())
        {
            LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
            if(seg.isFrontFacing() && seg.lineSide().hasSections())
            {
                Rend_ReportWallSectionDrawn(seg.line());
            }

            if(!P_IsInVoid(viewPlayer) && seg.isOpenRangeCovered())
            {
                C_AddRangeFromViewRelPoints(hedge->origin(), hedge->twin().origin());
            }
        }
    }

    foreach(Polyobj *po, subspace.polyobjs())
    foreach(HEdge *hedge, po->mesh().hedges())
    {
        if(hedge->hasMapElement())
        {
            LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
            if(seg.isFrontFacing() && seg.lineSide().hasSections())
            {
                Rend_ReportWallSectionDrawn(seg.line());
            }

            if(!P_IsInVoid(viewPlayer) && hedge->mapElementAs<LineSideSegment>().isOpenRangeCovered())
            {
                C_AddRangeFromViewRelPoints(hedge->origin(), hedge->twin().origin());
            }
        }
    }
}

static void traverseBspTreeAndDrawSubspaces(Map::BspTree const *bspTree)
{
    DENG2_ASSERT(bspTree != 0);

    while(!bspTree->isLeaf())
    {
        // Descend deeper into the nodes.
        BspNode const &bspNode = bspTree->userData()->as<BspNode>();

        // On which side is the viewer?
        int eyeSide = bspNode.partition().pointOnSide(eyeOrigin) < 0;

        // Recursively divide front space.
        traverseBspTreeAndDrawSubspaces(bspTree->childPtr(Map::BspTree::ChildId(eyeSide)));

        // If the clipper is full we're pretty much done. This means no geometry
        // will be visible in the distance because every direction has already
        // been fully covered by geometry.
        if(!firstSubspace && C_IsFull())
            return;

        // ...and back space.
        bspTree = bspTree->childPtr(Map::BspTree::ChildId(eyeSide ^ 1));
    }
    // We've arrived at a leaf.

    // Only leafs with a subspace have drawable geometries.
    BspLeaf const &bspLeaf = bspTree->userData()->as<BspLeaf>();
    if(bspLeaf.hasSubspace())
    {
        drawSubspace(bspLeaf.subspace());
        firstSubspace = false; // No longer the first.
    }
}

/**
 * Project all the non-clipped decorations. They become regular vissprites.
 */
static void generateDecorationFlares(Map &map)
{
    Vector3d const viewPos = vOrigin.xzy();

    foreach(Lumobj *lum, map.lumobjs())
    {
        lum->generateFlare(viewPos, R_ViewerLumobjDistance(lum->indexInMap()));

        /// @todo mark these light sources visible for LensFx
    }
}

/**
 * Setup GL state for an entire rendering pass (compassing multiple lists).
 */
static void pushGLStateForPass(DrawMode mode, TexUnitMap &texUnitMap)
{
    static float const black[4] = { 0, 0, 0, 0 };

    zap(texUnitMap);

    switch(mode)
    {
    case DM_SKYMASK:
        GL_SelectTexUnits(0);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        break;

    case DM_BLENDED:
        GL_SelectTexUnits(2);

        // Intentional fall-through.

    case DM_ALL:
        // The first texture unit is used for the main texture.
        texUnitMap[0] = WorldVBuf::PrimaryTex + 1;
        texUnitMap[1] = WorldVBuf::InterTex + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Fog is allowed during this pass.
        if(usingFog)
        {
            glEnable(GL_FOG);
        }
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case DM_LIGHT_MOD_TEXTURE:
    case DM_TEXTURE_PLUS_LIGHT:
        // Modulate sector light, dynamic light and regular texture.
        GL_SelectTexUnits(2);
        if(mode == DM_LIGHT_MOD_TEXTURE)
        {
            texUnitMap[0] = WorldVBuf::ModTex + 1;
            texUnitMap[1] = WorldVBuf::PrimaryTex + 1;
            GL_ModulateTexture(4); // Light * texture.
        }
        else
        {
            texUnitMap[0] = WorldVBuf::PrimaryTex + 1;
            texUnitMap[1] = WorldVBuf::ModTex + 1;
            GL_ModulateTexture(5); // Texture + light.
        }
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Fog is allowed during this pass.
        if(usingFog)
        {
            glEnable(GL_FOG);
        }
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case DM_FIRST_LIGHT:
        // One light, no texture.
        GL_SelectTexUnits(1);
        texUnitMap[0] = WorldVBuf::ModTex + 1;
        GL_ModulateTexture(6);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case DM_BLENDED_FIRST_LIGHT:
        // One additive light, no texture.
        GL_SelectTexUnits(1);
        texUnitMap[0] = WorldVBuf::ModTex + 1;
        GL_ModulateTexture(7); // Add light, no color.
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 1 / 255.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        break;

    case DM_WITHOUT_TEXTURE:
        GL_SelectTexUnits(0);
        GL_ModulateTexture(1);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case DM_LIGHTS:
        GL_SelectTexUnits(1);
        texUnitMap[0] = WorldVBuf::PrimaryTex + 1;
        GL_ModulateTexture(1);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 1 / 255.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if(usingFog)
        {
            glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, black);
        }

        glEnable(GL_BLEND);
        GL_BlendMode(BM_ADD);
        break;

    case DM_MOD_TEXTURE:
    case DM_MOD_TEXTURE_MANY_LIGHTS:
    case DM_BLENDED_MOD_TEXTURE:
        // The first texture unit is used for the main texture.
        texUnitMap[0] = WorldVBuf::PrimaryTex + 1;
        texUnitMap[1] = WorldVBuf::InterTex + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;

    case DM_UNBLENDED_TEXTURE_AND_DETAIL:
        texUnitMap[0] = WorldVBuf::PrimaryTex + 1;
        texUnitMap[1] = WorldVBuf::PrimaryTex + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        // Fog is allowed.
        if(usingFog)
        {
            glEnable(GL_FOG);
        }
        break;

    case DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        texUnitMap[0] = WorldVBuf::PrimaryTex + 1;
        texUnitMap[1] = WorldVBuf::PrimaryTex + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;

    case DM_ALL_DETAILS:
        GL_SelectTexUnits(1);
        texUnitMap[0] = WorldVBuf::PrimaryTex + 1;
        GL_ModulateTexture(0);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
        // Use fog to fade the details, if fog is enabled.
        if(usingFog)
        {
            glEnable(GL_FOG);
            float const midGray[4] = { .5f, .5f, .5f, fogColor[3] }; // The alpha is probably meaningless?
            glFogfv(GL_FOG_COLOR, midGray);
        }
        break;

    case DM_BLENDED_DETAILS:
        GL_SelectTexUnits(2);
        texUnitMap[0] = WorldVBuf::PrimaryTex + 1;
        texUnitMap[1] = WorldVBuf::InterTex + 1;
        GL_ModulateTexture(3);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
        // Use fog to fade the details, if fog is enabled.
        if(usingFog)
        {
            glEnable(GL_FOG);
            float const midGray[4] = { .5f, .5f, .5f, fogColor[3] }; // The alpha is probably meaningless?
            glFogfv(GL_FOG_COLOR, midGray);
        }
        break;

    case DM_SHADOW:
        // A bit like 'negative lights'.
        GL_SelectTexUnits(1);
        texUnitMap[0] = WorldVBuf::PrimaryTex + 1;
        GL_ModulateTexture(1);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 1 / 255.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        // Set normal fog, if it's enabled.
        if(usingFog)
        {
            glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, fogColor);
        }
        glEnable(GL_BLEND);
        GL_BlendMode(BM_NORMAL);
        break;

    case DM_SHINY:
        GL_SelectTexUnits(1);
        texUnitMap[0] = WorldVBuf::PrimaryTex + 1;
        GL_ModulateTexture(1); // 8 for multitexture
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if(usingFog)
        {   // Fog makes the shininess diminish in the distance.
            glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, black);
        }
        glEnable(GL_BLEND);
        GL_BlendMode(BM_ADD); // Purely additive.
        break;

    case DM_MASKED_SHINY:
        GL_SelectTexUnits(2);
        texUnitMap[0] = WorldVBuf::PrimaryTex + 1;
        texUnitMap[1] = WorldVBuf::InterTex + 1; // the mask
        GL_ModulateTexture(8); // same as with details
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if(usingFog)
        {
            // Fog makes the shininess diminish in the distance.
            glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, black);
        }
        glEnable(GL_BLEND);
        GL_BlendMode(BM_ADD); // Purely additive.
        break;

    default: break;
    }
}

static void popGLStateForPass(DrawMode mode)
{
    switch(mode)
    {
    default: break;

    case DM_SKYMASK:
        GL_SelectTexUnits(1);
        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        break;

    case DM_BLENDED:
        GL_SelectTexUnits(1);

        // Intentional fall-through.
    case DM_ALL:
        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        if(usingFog)
        {
            glDisable(GL_FOG);
        }
        glEnable(GL_BLEND);
        break;

    case DM_LIGHT_MOD_TEXTURE:
    case DM_TEXTURE_PLUS_LIGHT:
        GL_SelectTexUnits(1);
        GL_ModulateTexture(1);
        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        if(usingFog)
        {
            glDisable(GL_FOG);
        }
        glEnable(GL_BLEND);
        break;

    case DM_FIRST_LIGHT:
        GL_ModulateTexture(1);
        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        break;

    case DM_BLENDED_FIRST_LIGHT:
        GL_ModulateTexture(1);
        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;

    case DM_WITHOUT_TEXTURE:
        GL_SelectTexUnits(1);
        GL_ModulateTexture(1);
        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        break;

    case DM_LIGHTS:
        glDisable(GL_DEPTH_TEST);
        if(usingFog)
        {
            glDisable(GL_FOG);
        }
        GL_BlendMode(BM_NORMAL);
        break;

    case DM_MOD_TEXTURE:
    case DM_MOD_TEXTURE_MANY_LIGHTS:
    case DM_BLENDED_MOD_TEXTURE:
        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;

    case DM_UNBLENDED_TEXTURE_AND_DETAIL:
        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        if(usingFog)
        {
            glDisable(GL_FOG);
        }
        break;

    case DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;

    case DM_ALL_DETAILS:
        GL_ModulateTexture(1);
        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if(usingFog)
        {
            glDisable(GL_FOG);
        }
        break;

    case DM_BLENDED_DETAILS:
        GL_SelectTexUnits(1);
        GL_ModulateTexture(1);
        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if(usingFog)
        {
            glDisable(GL_FOG);
        }
        break;

    case DM_SHADOW:
        glDisable(GL_DEPTH_TEST);
        if(usingFog)
        {
            glDisable(GL_FOG);
        }
        break;

    case DM_SHINY:
        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        if(usingFog)
        {
            glDisable(GL_FOG);
        }
        GL_BlendMode(BM_NORMAL);
        break;

    case DM_MASKED_SHINY:
        GL_SelectTexUnits(1);
        GL_ModulateTexture(1);
        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        if(usingFog)
        {
            glDisable(GL_FOG);
        }
        GL_BlendMode(BM_NORMAL);
        break;
    }
}

static void drawLists(DrawLists::FoundLists const &lists, DrawMode mode)
{
    if(lists.isEmpty()) return;
    // If the first list is empty -- do nothing.
    if(lists.at(0)->isEmpty()) return;

    // Setup GL state that's common to all the lists in this mode.
    TexUnitMap texUnitMap;
    pushGLStateForPass(mode, texUnitMap);

    // Draw each given list.
    for(int i = 0; i < lists.count(); ++i)
    {
        lists.at(i)->draw(mode, texUnitMap);
    }

    popGLStateForPass(mode);
}

static void drawSky()
{
    DrawLists::FoundLists lists;
    ClientApp::renderSystem().drawLists().findAll(SkyMaskGeom, lists);
    if(!devRendSkyAlways && lists.isEmpty())
    {
        return;
    }

    // We do not want to update color and/or depth.
    glDisable(GL_DEPTH_TEST);
    GLState::push().setColorMask(gl::WriteNone).apply();

    // Mask out stencil buffer, setting the drawn areas to 1.
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glStencilFunc(GL_ALWAYS, 1, 0xffffffff);

    if(!devRendSkyAlways)
    {
        drawLists(lists, DM_SKYMASK);
    }
    else
    {
        glClearStencil(1);
        glClear(GL_STENCIL_BUFFER_BIT);
    }

    // Restore previous GL state.
    GLState::pop().apply();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    // Now, only render where the stencil is set to 1.
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 1, 0xffffffff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    theSky->draw();

    if(!devRendSkyAlways)
    {
        glClearStencil(0);
    }

    // Return GL state to normal.
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_DEPTH_TEST);
}

static bool generateHaloForVisSprite(vissprite_t const *spr, bool primary = false)
{
    float occlusionFactor;

    if(primary && (spr->data.flare.flags & RFF_NO_PRIMARY))
    {
        return false;
    }

    if(spr->data.flare.isDecoration)
    {
        /*
         * Surface decorations do not yet persist over frames, so we do
         * not smoothly occlude their flares. Instead, we will have to
         * put up with them instantly appearing/disappearing.
         */
        occlusionFactor = R_ViewerLumobjIsClipped(spr->data.flare.lumIdx)? 0 : 1;
    }
    else
    {
        occlusionFactor = (spr->data.flare.factor & 0x7f) / 127.0f;
    }

    return H_RenderHalo(spr->origin,
                        spr->data.flare.size,
                        spr->data.flare.tex,
                        spr->data.flare.color,
                        spr->distance,
                        occlusionFactor, spr->data.flare.mul,
                        spr->data.flare.xOff, primary,
                        (spr->data.flare.flags & RFF_NO_TURN) == 0);
}

/**
 * Render sprites, 3D models, masked wall segments and halos, ordered back to
 * front. Halos are rendered with Z-buffer tests and writes disabled, so they
 * don't go into walls or interfere with real objects. It means that halos can
 * be partly occluded by objects that are closer to the viewpoint, but that's
 * the price to pay for not having access to the actual Z-buffer per-pixel depth
 * information. The other option would be for halos to shine through masked walls,
 * sprites and models, which looks even worse. (Plus, they are *halos*, not real
 * lens flares...)
 */
static void drawMasked()
{
    if(devNoSprites) return;

    R_SortVisSprites();

    if(visSpriteP > visSprites)
    {
        bool primaryHaloDrawn = false;

        // Draw all vissprites back to front.
        // Sprites look better with Z buffer writes turned off.
        for(vissprite_t *spr = visSprSortedHead.next; spr != &visSprSortedHead; spr = spr->next)
        {
            switch(spr->type)
            {
            default: break;

            case VSPR_MASKED_WALL:
                // A masked wall is a specialized sprite.
                Rend_DrawMaskedWall(spr->data.wall);
                break;

            case VSPR_SPRITE:
                // Render an old fashioned sprite, ah the nostalgia...
                Rend_DrawSprite(spr->data.sprite);
                break;

            case VSPR_MODEL:
                Rend_DrawModel(spr->data.model);
                break;

            case VSPR_FLARE:
                if(generateHaloForVisSprite(spr, true))
                {
                    primaryHaloDrawn = true;
                }
                break;
            }
        }

        // Draw secondary halos?
        if(primaryHaloDrawn && haloMode > 1)
        {
            // Now we can setup the state only once.
            H_SetupState(true);

            for(vissprite_t *spr = visSprSortedHead.next; spr != &visSprSortedHead; spr = spr->next)
            {
                if(spr->type == VSPR_FLARE)
                {
                    generateHaloForVisSprite(spr);
                }
            }

            // And we're done...
            H_SetupState(false);
        }
    }
}

/*
 * We have several different paths to accommodate both multitextured details and
 * dynamic lights. Details take precedence (they always cover entire primitives
 * and usually *all* of the surfaces in a scene).
 */
static void drawAllLists(Map &map)
{
    DENG_ASSERT(!Sys_GLCheckError());
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    drawSky();

    // Render the real surfaces of the visible world.

    /*
     * Pass: Unlit geometries (all normal lists).
     */

    DrawLists::FoundLists lists;
    ClientApp::renderSystem().drawLists().findAll(UnlitGeom, lists);
    if(IS_MTEX_DETAILS)
    {
        // Draw details for unblended surfaces in this pass.
        drawLists(lists, DM_UNBLENDED_TEXTURE_AND_DETAIL);

        // Blended surfaces.
        drawLists(lists, DM_BLENDED);
    }
    else
    {
        // Blending is done during this pass.
        drawLists(lists, DM_ALL);
    }

    /*
     * Pass: Lit geometries.
     */

    ClientApp::renderSystem().drawLists().findAll(LitGeom, lists);

    // If multitexturing is available, we'll use it to our advantage when
    // rendering lights.
    if(IS_MTEX_LIGHTS && dynlightBlend != 2)
    {
        if(IS_MUL)
        {
            // All (unblended) surfaces with exactly one light can be
            // rendered in a single pass.
            drawLists(lists, DM_LIGHT_MOD_TEXTURE);

            // Render surfaces with many lights without a texture, just
            // with the first light.
            drawLists(lists, DM_FIRST_LIGHT);
        }
        else // Additive ('foggy') lights.
        {
            drawLists(lists, DM_TEXTURE_PLUS_LIGHT);

            // Render surfaces with blending.
            drawLists(lists, DM_BLENDED);

            // Render the first light for surfaces with blending.
            // (Not optimal but shouldn't matter; texture is changed for
            // each primitive.)
            drawLists(lists, DM_BLENDED_FIRST_LIGHT);
        }
    }
    else // Multitexturing is not available for lights.
    {
        if(IS_MUL)
        {
            // Render all lit surfaces without a texture.
            drawLists(lists, DM_WITHOUT_TEXTURE);
        }
        else
        {
            if(IS_MTEX_DETAILS) // Draw detail textures using multitexturing.
            {
                // Unblended surfaces with a detail.
                drawLists(lists, DM_UNBLENDED_TEXTURE_AND_DETAIL);

                // Blended surfaces without details.
                drawLists(lists, DM_BLENDED);

                // Details for blended surfaces.
                drawLists(lists, DM_BLENDED_DETAILS);
            }
            else
            {
                drawLists(lists, DM_ALL);
            }
        }
    }

    /*
     * Pass: All light geometries (always additive).
     */
    if(dynlightBlend != 2)
    {
        ClientApp::renderSystem().drawLists().findAll(LightGeom, lists);
        drawLists(lists, DM_LIGHTS);
    }

    /*
     * Pass: Geometries with texture modulation.
     */
    if(IS_MUL)
    {
        // Finish the lit surfaces that didn't yet get a texture.
        ClientApp::renderSystem().drawLists().findAll(LitGeom, lists);
        if(IS_MTEX_DETAILS)
        {
            drawLists(lists, DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL);
            drawLists(lists, DM_BLENDED_MOD_TEXTURE);
            drawLists(lists, DM_BLENDED_DETAILS);
        }
        else
        {
            if(IS_MTEX_LIGHTS && dynlightBlend != 2)
            {
                drawLists(lists, DM_MOD_TEXTURE_MANY_LIGHTS);
            }
            else
            {
                drawLists(lists, DM_MOD_TEXTURE);
            }
        }
    }

    /*
     * Pass: Geometries with details & modulation.
     *
     * If multitexturing is not available for details, we need to apply them as
     * an extra pass over all the detailed surfaces.
     */
    if(r_detail)
    {
        // Render detail textures for all surfaces that need them.
        ClientApp::renderSystem().drawLists().findAll(UnlitGeom, lists);
        if(IS_MTEX_DETAILS)
        {
            // Blended detail textures.
            drawLists(lists, DM_BLENDED_DETAILS);
        }
        else
        {
            drawLists(lists, DM_ALL_DETAILS);

            ClientApp::renderSystem().drawLists().findAll(LitGeom, lists);
            drawLists(lists, DM_ALL_DETAILS);
        }
    }

    /*
     * Pass: Shiny geometries.
     *
     * If we have two texture units, the shiny masks will be enabled. Otherwise
     * the masks are ignored. The shine is basically specular environmental
     * additive light, multiplied by the mask so that black texels from the mask
     * produce areas without shine.
     */

    ClientApp::renderSystem().drawLists().findAll(ShineGeom, lists);
    if(numTexUnits > 1)
    {
        // Render masked shiny surfaces in a separate pass.
        drawLists(lists, DM_SHINY);
        drawLists(lists, DM_MASKED_SHINY);
    }
    else
    {
        drawLists(lists, DM_ALL_SHINY);
    }

    /*
     * Pass: Shadow geometries (objects and Fake Radio).
     */
    int const oldRenderTextures = renderTextures;

    renderTextures = true;

    ClientApp::renderSystem().drawLists().findAll(ShadowGeom, lists);
    drawLists(lists, DM_SHADOW);

    renderTextures = oldRenderTextures;

    glDisable(GL_TEXTURE_2D);

    // The draw lists do not modify these states -ds
    glEnable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0);
    if(usingFog)
    {
        glEnable(GL_FOG);
        glFogfv(GL_FOG_COLOR, fogColor);
    }

    // Draw masked walls, sprites and models.
    drawMasked();

    // Draw particles.
    Rend_RenderParticles(map);

    if(usingFog)
    {
        glDisable(GL_FOG);
    }

    DENG2_ASSERT(!Sys_GLCheckError());
}

void Rend_RenderMap(Map &map)
{
    GL_SetMultisample(true);

    // Setup the modelview matrix.
    Rend_ModelViewMatrix();

    if(!freezeRLs)
    {
        // Discard all Shards generated for the previous frame only.
        foreach(SectorCluster *cluster, map.clusters()) cluster->clearAllShards();

        // Prepare for rendering.
        ClientApp::renderSystem().resetDrawLists(); // Clear the lists for new geometry.
        C_ClearRanges(); // Clear the clipper.

        // Recycle the vlight lists. Currently done here as the lists are
        // not shared by all viewports.
        VL_InitForNewFrame();

        R_BeginFrame();

        // Make vissprites of all the visible decorations.
        generateDecorationFlares(map);

        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
        eyeOrigin = viewData->current.origin;

        // Add the backside clipping range (if vpitch allows).
        if(vpitch <= 90 - yfov / 2 && vpitch >= -90 + yfov / 2)
        {
            float a = de::abs(vpitch) / (90 - yfov / 2);
            binangle_t startAngle = binangle_t(BANG_45 * Rend_FieldOfView() / 90) * (1 + a);
            binangle_t angLen = BANG_180 - startAngle;

            binangle_t viewside = (viewData->current.angle() >> (32 - BAMS_BITS)) + startAngle;
            C_SafeAddRange(viewside, viewside + angLen);
            C_SafeAddRange(viewside + angLen, viewside + 2 * angLen);
        }

        // The viewside line for the depth cue.
        viewsidex = -viewData->viewSin;
        viewsidey = viewData->viewCos;

        // We don't want BSP clip checking for the first subspace.
        firstSubspace = true;

        // Draw the world!
        traverseBspTreeAndDrawSubspaces(&map.bspTree());
    }
    drawAllLists(map);

    // Draw various debugging displays:
    drawSurfaceTangentVectors(map);
    drawLumobjs(map);
    drawMobjBoundingBoxes(map);
    drawSectors(map);
    drawVertexes(map);
    drawThinkers(map);
    drawSoundEmitters(map);
    drawGenerators(map);
    drawBiasEditingVisuals(map);

    GL_SetMultisample(false);
}

static void drawStar(Vector3d const &origin, float size, Vector4f const &color)
{
    float const black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(origin.x - size, origin.z, origin.y);
        glColor4f(color.x, color.y, color.z, color.w);
        glVertex3f(origin.x, origin.z, origin.y);
        glVertex3f(origin.x, origin.z, origin.y);
        glColor4fv(black);
        glVertex3f(origin.x + size, origin.z, origin.y);

        glVertex3f(origin.x, origin.z - size, origin.y);
        glColor4f(color.x, color.y, color.z, color.w);
        glVertex3f(origin.x, origin.z, origin.y);
        glVertex3f(origin.x, origin.z, origin.y);
        glColor4fv(black);
        glVertex3f(origin.x, origin.z + size, origin.y);

        glVertex3f(origin.x, origin.z, origin.y - size);
        glColor4f(color.x, color.y, color.z, color.w);
        glVertex3f(origin.x, origin.z, origin.y);
        glVertex3f(origin.x, origin.z, origin.y);
        glColor4fv(black);
        glVertex3f(origin.x, origin.z, origin.y + size);
    glEnd();
}

static void drawLabel(Vector3d const &origin, String const &label, float scale, float alpha)
{
    if(label.isEmpty()) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(origin.x, origin.z, origin.y);
    glRotatef(-vang + 180, 0, 1, 0);
    glRotatef(vpitch, 1, 0, 0);
    glScalef(-scale, -scale, 1);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    Point2Raw offset(2, 2);
    UI_TextOutEx(label.toUtf8().constData(), &offset, UI_Color(UIC_TITLE), alpha);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
}

static void drawLabel(Vector3d const &origin, String const &label)
{
    ddouble distToEye = (vOrigin.xzy() - origin).length();
    drawLabel(origin, label, distToEye / (DENG_GAMEVIEW_WIDTH / 2), 1 - distToEye / 2000);
}

/*
 * Visuals for Shadow Bias editing:
 */

static String labelForSource(BiasSource *s)
{
    if(!s || !editShowIndices) return String();
    /// @todo Don't assume the current map.
    return String::number(App_WorldSystem().map().toIndex(*s));
}

static void drawSource(BiasSource *s)
{
    if(!s) return;

    ddouble distToEye = (s->origin() - eyeOrigin).length();

    drawStar(s->origin(), 25 + s->evaluateIntensity() / 20,
             Vector4f(s->color(), 1.0f / de::max(float((distToEye - 100) / 1000), 1.f)));
    drawLabel(s->origin(), labelForSource(s));
}

static void drawLock(Vector3d const &origin, double unit, double t)
{
    glColor4f(1, 1, 1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glTranslatef(origin.x, origin.z, origin.y);

    glRotatef(t / 2,  0, 0, 1);
    glRotatef(t,      1, 0, 0);
    glRotatef(t * 15, 0, 1, 0);

    glBegin(GL_LINES);
        glVertex3f(-unit, 0, -unit);
        glVertex3f(+unit, 0, -unit);

        glVertex3f(+unit, 0, -unit);
        glVertex3f(+unit, 0, +unit);

        glVertex3f(+unit, 0, +unit);
        glVertex3f(-unit, 0, +unit);

        glVertex3f(-unit, 0, +unit);
        glVertex3f(-unit, 0, -unit);
    glEnd();

    glPopMatrix();
}

static void drawBiasEditingVisuals(Map &map)
{
    if(freezeRLs) return;
    if(!SBE_Active() || editHidden) return;

    if(!map.biasSourceCount())
        return;

    double const t = Timer_RealMilliseconds() / 100.0f;

    if(HueCircle *hueCircle = SBE_HueCircle())
    {
        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(vOrigin.x, vOrigin.y, vOrigin.z);
        glScalef(1, 1.0f/1.2f, 1);
        glTranslatef(-vOrigin.x, -vOrigin.y, -vOrigin.z);

        HueCircleVisual::draw(*hueCircle, vOrigin, viewData->frontVec);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    coord_t handDistance;
    Hand &hand = App_WorldSystem().hand(&handDistance);

    // Grabbed sources blink yellow.
    Vector4f grabbedColor;
    if(!editBlink || map.biasCurrentTime() & 0x80)
        grabbedColor = Vector4f(1, 1, .8f, .5f);
    else
        grabbedColor = Vector4f(.7f, .7f, .5f, .4f);

    BiasSource *nearSource = map.biasSourceNear(hand.origin());
    DENG_ASSERT(nearSource != 0);

    if((hand.origin() - nearSource->origin()).length() > 2 * handDistance)
    {
        // Show where it is.
        glDisable(GL_DEPTH_TEST);
    }

    // The nearest cursor phases blue.
    drawStar(nearSource->origin(), 10000,
             nearSource->isGrabbed()? grabbedColor :
             Vector4f(.0f + sin(t) * .2f,
                      .2f + sin(t) * .15f,
                      .9f + sin(t) * .3f,
                      .8f - sin(t) * .2f));

    glDisable(GL_DEPTH_TEST);

    drawLabel(nearSource->origin(), labelForSource(nearSource));
    if(nearSource->isLocked())
        drawLock(nearSource->origin(), 2 + (nearSource->origin() - eyeOrigin).length() / 100, t);

    foreach(Grabbable *grabbable, hand.grabbed())
    {
        if(de::internal::cannotCastGrabbableTo<BiasSource>(grabbable)) continue;
        BiasSource *s = &grabbable->as<BiasSource>();

        if(s == nearSource)
            continue;

        drawStar(s->origin(), 10000, grabbedColor);
        drawLabel(s->origin(), labelForSource(s));

        if(s->isLocked())
            drawLock(s->origin(), 2 + (s->origin() - eyeOrigin).length() / 100, t);
    }

    /*BiasSource *s = hand.nearestBiasSource();
    if(s && !hand.hasGrabbed(*s))
    {
        glDisable(GL_DEPTH_TEST);
        drawLabel(s->origin(), labelForSource(s));
    }*/

    // Show all sources?
    if(editShowAll)
    {
        foreach(BiasSource *source, map.biasSources())
        {
            if(source == nearSource) continue;
            if(source->isGrabbed()) continue;

            drawSource(source);
        }
    }

    glEnable(GL_DEPTH_TEST);
}

void Rend_UpdateLightModMatrix()
{
    if(novideo) return;

    zap(lightModRange);

    if(!App_WorldSystem().hasMap())
    {
        rAmbient = 0;
        return;
    }

    int mapAmbient = App_WorldSystem().map().ambientLightLevel();
    if(mapAmbient > ambientLight)
    {
        rAmbient = mapAmbient;
    }
    else
    {
        rAmbient = ambientLight;
    }

    for(int i = 0; i < 255; ++i)
    {
        // Adjust the white point/dark point?
        float lightlevel = 0;
        if(lightRangeCompression != 0)
        {
            if(lightRangeCompression >= 0)
            {
                // Brighten dark areas.
                lightlevel = float(255 - i) * lightRangeCompression;
            }
            else
            {
                // Darken bright areas.
                lightlevel = float(-i) * -lightRangeCompression;
            }
        }

        // Lower than the ambient limit?
        if(rAmbient != 0 && i+lightlevel <= rAmbient)
        {
            lightlevel = rAmbient - i;
        }

        // Clamp the result as a modifier to the light value (j).
        if((i + lightlevel) >= 255)
        {
            lightlevel = 255 - i;
        }
        else if((i + lightlevel) <= 0)
        {
            lightlevel = -i;
        }

        // Insert it into the matrix.
        lightModRange[i] = lightlevel / 255.0f;

        // Ensure the resultant value never exceeds the expected [0..1] range.
        DENG2_ASSERT(INRANGE_OF(i / 255.0f + lightModRange[i], 0.f, 1.f));
    }
}

float Rend_LightAdaptationDelta(float val)
{
    int clampedVal = de::clamp(0, ROUND(255.0f * val), 254);
    return lightModRange[clampedVal];
}

void Rend_ApplyLightAdaptation(float &val)
{
    val += Rend_LightAdaptationDelta(val);
}

void Rend_DrawLightModMatrix()
{
#define BLOCK_WIDTH             (1.0f)
#define BLOCK_HEIGHT            (BLOCK_WIDTH * 255.0f)
#define BORDER                  (20)

    // Disabled?
    if(!devLightModRange) return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);

    glTranslatef(BORDER, BORDER, 0);

    /*
    color.red = 0.2f;
    color.green = 0;
    color.blue = 0.6f;
    */

    // Draw an outside border.
    glColor4f(1, 1, 0, 1);
    glBegin(GL_LINES);
        glVertex2f(-1, -1);
        glVertex2f(255 + 1, -1);
        glVertex2f(255 + 1, -1);
        glVertex2f(255 + 1, BLOCK_HEIGHT + 1);
        glVertex2f(255 + 1, BLOCK_HEIGHT + 1);
        glVertex2f(-1, BLOCK_HEIGHT + 1);
        glVertex2f(-1, BLOCK_HEIGHT + 1);
        glVertex2f(-1, -1);
    glEnd();

    glBegin(GL_QUADS);
    float c = 0;
    for(int i = 0; i < 255; ++i, c += (1.0f/255.0f))
    {
        // Get the result of the source light level + offset.
        float off = lightModRange[i];

        glColor4f(c + off, c + off, c + off, 1);
        glVertex2f(i * BLOCK_WIDTH, 0);
        glVertex2f(i * BLOCK_WIDTH + BLOCK_WIDTH, 0);
        glVertex2f(i * BLOCK_WIDTH + BLOCK_WIDTH, BLOCK_HEIGHT);
        glVertex2f(i * BLOCK_WIDTH, BLOCK_HEIGHT);
    }
    glEnd();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

#undef BORDER
#undef BLOCK_HEIGHT
#undef BLOCK_WIDTH
}

static DGLuint constructBBox(DGLuint name, float br)
{
    if(GL_NewList(name, GL_COMPILE))
    {
        glBegin(GL_QUADS);
        {
            // Top
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br, 1.0f,-1.0f-br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br, 1.0f,-1.0f-br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br, 1.0f, 1.0f+br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br, 1.0f, 1.0f+br);  // BR
            // Bottom
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br,-1.0f, 1.0f+br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br,-1.0f, 1.0f+br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br,-1.0f,-1.0f-br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br,-1.0f,-1.0f-br);  // BR
            // Front
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br, 1.0f+br, 1.0f);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br, 1.0f+br, 1.0f);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br,-1.0f-br, 1.0f);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br,-1.0f-br, 1.0f);  // BR
            // Back
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br,-1.0f-br,-1.0f);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br,-1.0f-br,-1.0f);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br, 1.0f+br,-1.0f);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br, 1.0f+br,-1.0f);  // BR
            // Left
            glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, 1.0f+br, 1.0f+br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, 1.0f+br,-1.0f-br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,-1.0f-br,-1.0f-br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f,-1.0f-br, 1.0f+br);  // BR
            // Right
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f, 1.0f+br,-1.0f-br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, 1.0f+br, 1.0f+br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f,-1.0f-br, 1.0f+br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,-1.0f-br,-1.0f-br);  // BR
        }
        glEnd();
        return GL_EndList();
    }
    return 0;
}

/**
 * Draws a textured cube using the currently bound gl texture.
 * Used to draw mobj bounding boxes.
 *
 * @param pos           Coordinates of the center of the box (in map space units).
 * @param w             Width of the box.
 * @param l             Length of the box.
 * @param h             Height of the box.
 * @param a             Angle of the box.
 * @param color         Color to make the box (uniform vertex color).
 * @param alpha         Alpha to make the box (uniform vertex color).
 * @param br            Border amount to overlap box faces.
 * @param alignToBase   If @c true, align the base of the box
 *                      to the Z coordinate.
 */
void Rend_DrawBBox(Vector3d const &pos, coord_t w, coord_t l, coord_t h,
    float a, float const color[3], float alpha, float br, bool alignToBase)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    if(alignToBase)
        // The Z coordinate is to the bottom of the object.
        glTranslated(pos.x, pos.z + h, pos.y);
    else
        glTranslated(pos.x, pos.z, pos.y);

    glRotatef(0, 0, 0, 1);
    glRotatef(0, 1, 0, 0);
    glRotatef(a, 0, 1, 0);

    glScaled(w - br - br, h - br - br, l - br - br);
    glColor4f(color[CR], color[CG], color[CB], alpha);

    GL_CallList(dlBBox);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

/**
 * Draws a textured triangle using the currently bound gl texture.
 * Used to draw mobj angle direction arrow.
 *
 * @param pos           Coordinates of the center of the base of the
 *                      triangle (in "world" coordinates [VX, VY, VZ]).
 * @param a             Angle to point the triangle in.
 * @param s             Scale of the triangle.
 * @param color         Color to make the box (uniform vertex color).
 * @param alpha         Alpha to make the box (uniform vertex color).
 */
void Rend_DrawArrow(Vector3d const &pos, float a, float s, float const color[3],
    float alpha)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glTranslated(pos.x, pos.z, pos.y);

    glRotatef(0, 0, 0, 1);
    glRotatef(0, 1, 0, 0);
    glRotatef(a, 0, 1, 0);

    glScalef(s, 0, s);

    glBegin(GL_TRIANGLES);
    {
        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f( 1.0f, 1.0f,-1.0f);  // L

        glColor4f(color[0], color[1], color[2], alpha);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-1.0f, 1.0f,-1.0f);  // Point

        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-1.0f, 1.0f, 1.0f);  // R
    }
    glEnd();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

static int drawMobjBBox(thinker_t *th, void * /*context*/)
{
    static float const red[3]    = { 1, 0.2f, 0.2f}; // non-solid objects
    static float const green[3]  = { 0.2f, 1, 0.2f}; // solid objects
    static float const yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles

    mobj_t *mo = (mobj_t *) th;

    // We don't want the console player.
    if(mo == ddPlayers[consolePlayer].shared.mo)
        return false;

    // Is it vissible?
    if(!Mobj_IsLinked(*mo))
        return false;
    BspLeaf const &bspLeaf = Mobj_BspLeafAtOrigin(*mo);
    if(!bspLeaf.hasSubspace() || !R_ViewerSubspaceIsVisible(bspLeaf.subspace()))
        return false;

    ddouble const distToEye = (eyeOrigin - Mobj_Origin(*mo)).length();
    float alpha = 1 - ((distToEye / (DENG_GAMEVIEW_WIDTH/2)) / 4);
    if(alpha < .25f)
        alpha = .25f; // Don't make them totally invisible.

    // Draw a bounding box in an appropriate color.
    coord_t size = Mobj_Radius(*mo);
    Rend_DrawBBox(mo->origin, size, size, mo->height/2, 0,
                  (mo->ddFlags & DDMF_MISSILE)? yellow :
                  (mo->ddFlags & DDMF_SOLID)? green : red,
                  alpha, .08f);

    Rend_DrawArrow(mo->origin, ((mo->angle + ANG45 + ANG90) / (float) ANGLE_MAX *-360), size*1.25,
                   (mo->ddFlags & DDMF_MISSILE)? yellow :
                   (mo->ddFlags & DDMF_SOLID)? green : red, alpha);
    return false;
}

/**
 * Renders bounding boxes for all mobj's (linked in sec->mobjList, except
 * the console player) in all sectors that are currently marked as vissible.
 *
 * Depth test is disabled to show all mobjs that are being rendered, regardless
 * if they are actually vissible (hidden by previously drawn map geometry).
 */
static void drawMobjBoundingBoxes(Map &map)
{
    //static float const red[3]   = { 1, 0.2f, 0.2f}; // non-solid objects
    static float const green[3]  = { 0.2f, 1, 0.2f}; // solid objects
    static float const yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles

    if(!devMobjBBox && !devPolyobjBBox) return;

#ifndef _DEBUG
    // Bounding boxes are not allowed in non-debug netgames.
    if(netGame) return;
#endif

    if(!dlBBox)
        dlBBox = constructBBox(0, .08f);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    MaterialSnapshot const &ms =
        ClientApp::resourceSystem().material(de::Uri("System", Path("bbox")))
                  .prepare(Rend_SpriteMaterialSpec());

    GL_BindTexture(&ms.texture(MTU_PRIMARY));
    GL_BlendMode(BM_ADD);

    if(devMobjBBox)
    {
        map.thinkers().iterate(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1,
                               drawMobjBBox);
    }

    if(devPolyobjBBox)
    {
        foreach(Polyobj const *polyobj, map.polyobjs())
        {
            Sector const &sec = polyobj->sector();
            coord_t width  = (polyobj->aaBox.maxX - polyobj->aaBox.minX)/2;
            coord_t length = (polyobj->aaBox.maxY - polyobj->aaBox.minY)/2;
            coord_t height = (sec.ceiling().height() - sec.floor().height())/2;

            Vector3d pos(polyobj->aaBox.minX + width,
                         polyobj->aaBox.minY + length,
                         sec.floor().height());

            ddouble const distToEye = (eyeOrigin - pos).length();
            float alpha = 1 - ((distToEye / (DENG_GAMEVIEW_WIDTH/2)) / 4);
            if(alpha < .25f)
                alpha = .25f; // Don't make them totally invisible.

            Rend_DrawBBox(pos, width, length, height, 0, yellow, alpha, .08f);

            foreach(Line *line, polyobj->lines())
            {
                Vector3d pos(line->center(), sec.floor().height());

                Rend_DrawBBox(pos, 0, line->length() / 2, height,
                              BANG2DEG(BANG_90 - line->angle()),
                              green, alpha, 0);
            }
        }
    }

    GL_BlendMode(BM_NORMAL);

    glEnable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}

static void drawVector(Vector3f const &vector, float scalar, float const color[3])
{
    static float const black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(scalar * vector.x, scalar * vector.z, scalar * vector.y);
        glColor3fv(color);
        glVertex3f(0, 0, 0);
    glEnd();
}

static void drawTangentVectorsForSurface(Surface const &suf, Vector3d const &origin)
{
    int const VISUAL_LENGTH = 20;

    static float const red[3]   = { 1, 0, 0 };
    static float const green[3] = { 0, 1, 0 };
    static float const blue[3]  = { 0, 0, 1 };

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(origin.x, origin.z, origin.y);

    if(devSurfaceVectors & SVF_TANGENT)   drawVector(suf.tangent(),   VISUAL_LENGTH, red);
    if(devSurfaceVectors & SVF_BITANGENT) drawVector(suf.bitangent(), VISUAL_LENGTH, green);
    if(devSurfaceVectors & SVF_NORMAL)    drawVector(suf.normal(),    VISUAL_LENGTH, blue);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

/**
 * @todo Determine Z-axis origin from a WallEdge.
 */
static void drawTangentVectorsForWallSections(HEdge const *hedge)
{
    if(!hedge || !hedge->hasMapElement())
        return;

    LineSideSegment const &seg = hedge->mapElementAs<LineSideSegment>();
    LineSide const &lineSide   = seg.lineSide();
    Line const &line           = lineSide.line();
    Vector2d const center      = (hedge->twin().origin() + hedge->origin()) / 2;

    if(lineSide.considerOneSided())
    {
        SectorCluster &cluster =
            (line.definesPolyobj()? line.polyobj().bspLeaf().subspace()
                                  : hedge->face().mapElementAs<ConvexSubspace>()).cluster();

        coord_t const bottom = cluster.  visFloor().heightSmoothed();
        coord_t const top    = cluster.visCeiling().heightSmoothed();

        drawTangentVectorsForSurface(lineSide.middle(),
                                     Vector3d(center, bottom + (top - bottom) / 2));
    }
    else
    {
        SectorCluster &cluster =
            (line.definesPolyobj()? line.polyobj().bspLeaf().subspace()
                                  : hedge->face().mapElementAs<ConvexSubspace>()).cluster();
        SectorCluster &backCluster =
            (line.definesPolyobj()? line.polyobj().bspLeaf().subspace()
                                  : hedge->twin().face().mapElementAs<ConvexSubspace>()).cluster();

        if(lineSide.middle().hasMaterial())
        {
            coord_t const bottom = cluster.  visFloor().heightSmoothed();
            coord_t const top    = cluster.visCeiling().heightSmoothed();

            drawTangentVectorsForSurface(lineSide.middle(),
                                         Vector3d(center, bottom + (top - bottom) / 2));
        }

        if(backCluster.visCeiling().heightSmoothed() < cluster.visCeiling().heightSmoothed() &&
           !(cluster.    visCeiling().surface().hasSkyMaskedMaterial() &&
             backCluster.visCeiling().surface().hasSkyMaskedMaterial()))
        {
            coord_t const bottom = backCluster.visCeiling().heightSmoothed();
            coord_t const top    = cluster.    visCeiling().heightSmoothed();

            drawTangentVectorsForSurface(lineSide.top(),
                                         Vector3d(center, bottom + (top - bottom) / 2));
        }

        if(backCluster.visFloor().heightSmoothed() > cluster.visFloor().heightSmoothed() &&
           !(cluster.    visFloor().surface().hasSkyMaskedMaterial() &&
             backCluster.visFloor().surface().hasSkyMaskedMaterial()))
        {
            coord_t const bottom = cluster.    visFloor().heightSmoothed();
            coord_t const top    = backCluster.visFloor().heightSmoothed();

            drawTangentVectorsForSurface(lineSide.bottom(),
                                         Vector3d(center, bottom + (top - bottom) / 2));
        }
    }
}

static int drawSurfaceTangentVectorsWorker(ConvexSubspace *subspace, void *)
{
    DENG2_ASSERT(subspace->hasCluster());

    // Have we yet to process this subspace?
    if(subspace->validCount() != validCount)
    {
        subspace->setValidCount(validCount); // Don't do so again.

        HEdge const *base  = subspace->poly().hedge();
        HEdge const *hedge = base;
        do
        {
            drawTangentVectorsForWallSections(hedge);
        } while((hedge = &hedge->next()) != base);

        foreach(Mesh *mesh, subspace->extraMeshes())
        foreach(HEdge *hedge, mesh->hedges())
        {
            drawTangentVectorsForWallSections(hedge);
        }

        foreach(Polyobj *polyobj, subspace->polyobjs())
        foreach(HEdge *hedge, polyobj->mesh().hedges())
        {
            drawTangentVectorsForWallSections(hedge);
        }

        int const planeCount = subspace->cluster().visPlaneCount();
        for(int i = 0; i < planeCount; ++i)
        {
            Plane const &plane = subspace->cluster().visPlane(i);
            coord_t height     = 0;

            if(plane.surface().hasSkyMaskedMaterial() &&
               (plane.isSectorFloor() || plane.isSectorCeiling()))
            {
                height = plane.map().skyPlane(plane.isSectorCeiling()? Map::SkyCeiling : Map::SkyFloor).height();
            }
            else
            {
                height = plane.heightSmoothed();
            }

            drawTangentVectorsForSurface(plane.surface(), Vector3d(subspace->poly().center(), height));
        }
    }

    return 0; // Continue iteration.
}

/**
 * Draw the surface tangent space vectors, primarily for debug.
 */
static void drawSurfaceTangentVectors(Map &map)
{
    coord_t const MAX_DISTANCE = 1280; ///< From the viewer.

    if(!devSurfaceVectors) return;

    glDisable(GL_CULL_FACE);

    validCount++;
    AABoxd box(eyeOrigin.x - MAX_DISTANCE, eyeOrigin.y - MAX_DISTANCE,
               eyeOrigin.x + MAX_DISTANCE, eyeOrigin.y + MAX_DISTANCE);
    map.subspaceBoxIterator(box, drawSurfaceTangentVectorsWorker);

    glEnable(GL_CULL_FACE);
}

static void drawLumobjs(Map &map)
{
    DENG_UNUSED(map);

    static float const black[4] = { 0, 0, 0, 0 };

    if(!devDrawLums) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    for(int i = 0; i < map.lumobjCount(); ++i)
    {
        Lumobj *lum = map.lumobj(i);

        if(rendMaxLumobjs > 0 && R_ViewerLumobjIsHidden(i))
            continue;

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslated(lum->origin().x, lum->origin().z + lum->zOffset(), lum->origin().y);

        glBegin(GL_LINES);
        {
            glColor4fv(black);
            glVertex3f(-lum->radius(), 0, 0);
            glColor4f(lum->color().x, lum->color().y, lum->color().z, 1);
            glVertex3f(0, 0, 0);
            glVertex3f(0, 0, 0);
            glColor4fv(black);
            glVertex3f(lum->radius(), 0, 0);

            glVertex3f(0, -lum->radius(), 0);
            glColor4f(lum->color().x, lum->color().y, lum->color().z, 1);
            glVertex3f(0, 0, 0);
            glVertex3f(0, 0, 0);
            glColor4fv(black);
            glVertex3f(0, lum->radius(), 0);

            glVertex3f(0, 0, -lum->radius());
            glColor4f(lum->color().x, lum->color().y, lum->color().z, 1);
            glVertex3f(0, 0, 0);
            glVertex3f(0, 0, 0);
            glColor4fv(black);
            glVertex3f(0, 0, lum->radius());
        }
        glEnd();

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

static void drawSoundEmitter(SoundEmitter &emitter, String const &label)
{
#define MAX_SOUNDORIGIN_DIST 384

    Vector3d const &origin  = Vector3d(emitter.origin);
    ddouble const distToEye = (eyeOrigin - origin).length();
    if(distToEye < MAX_SOUNDORIGIN_DIST)
    {
        drawLabel(origin, label,
                  distToEye / (DENG_GAMEVIEW_WIDTH / 2),
                  1 - distToEye / MAX_SOUNDORIGIN_DIST);
    }

#undef MAX_SOUNDORIGIN_DIST
}

/**
 * Debugging aid for visualizing sound origins.
 */
static void drawSoundEmitters(Map &map)
{
    if(!devSoundEmitters) return;

    if(devSoundEmitters & SOF_SIDE)
    {
        foreach(Line *line, map.lines())
        for(int i = 0; i < 2; ++i)
        {
            LineSide &side = line->side(i);
            if(!side.hasSections()) continue;

            drawSoundEmitter(side.middleSoundEmitter(),
                             String("Line #%1 (%2, middle)")
                                 .arg(line->indexInMap())
                                 .arg(i? "back" : "front"));

            drawSoundEmitter(side.bottomSoundEmitter(),
                             String("Line #%1 (%2, bottom)")
                                 .arg(line->indexInMap())
                                 .arg(i? "back" : "front"));

            drawSoundEmitter(side.topSoundEmitter(),
                             String("Line #%1 (%2, top)")
                                 .arg(line->indexInMap())
                                 .arg(i? "back" : "front"));
        }
    }

    if(devSoundEmitters & (SOF_SECTOR|SOF_PLANE))
    {
        foreach(Sector *sec, map.sectors())
        {
            if(devSoundEmitters & SOF_PLANE)
            {
                foreach(Plane *plane, sec->planes())
                {
                    drawSoundEmitter(plane->soundEmitter(),
                                     String("Sector #%1 (pln:%2)")
                                         .arg(sec->indexInMap())
                                         .arg(plane->indexInSector()));
                }
            }

            if(devSoundEmitters & SOF_SECTOR)
            {
                drawSoundEmitter(sec->soundEmitter(),
                                 String("Sector #%1").arg(sec->indexInMap()));
            }
        }
    }
}

static String labelForGenerator(Generator const *gen)
{
    DENG2_ASSERT(gen != 0);
    return String("%1").arg(gen->id());
}

static int drawGenerator(Generator *gen, void * /*context*/)
{
#define MAX_GENERATOR_DIST  2048

    if(gen->source || gen->isUntriggered())
    {
        Vector3d const origin   = gen->origin();
        ddouble const distToEye = (eyeOrigin - origin).length();
        if(distToEye < MAX_GENERATOR_DIST)
        {
            drawLabel(origin, labelForGenerator(gen),
                      distToEye / (DENG_GAMEVIEW_WIDTH / 2),
                      1 - distToEye / MAX_GENERATOR_DIST);
        }
    }

    return false; // Continue iteration.

#undef MAX_GENERATOR_DIST
}

/**
 * Debugging aid; Draw all active generators.
 */
static void drawGenerators(Map &map)
{
    if(!devDrawGenerators) return;
    map.generatorIterator(drawGenerator);
}

static void drawPoint(Vector3d const &origin, float opacity)
{
    glBegin(GL_POINTS);
        glColor4f(.7f, .7f, .2f, opacity * 2);
        glVertex3f(origin.x, origin.z, origin.y);
    glEnd();
}

static void drawBar(Vector3d const &origin, coord_t height, float opacity)
{
    int const EXTEND_DIST = 64;

    static float const black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(origin.x, origin.z - EXTEND_DIST, origin.y);
        glColor4f(1, 1, 1, opacity);
        glVertex3f(origin.x, origin.z, origin.y);
        glVertex3f(origin.x, origin.z, origin.y);
        glVertex3f(origin.x, origin.z + height, origin.y);
        glVertex3f(origin.x, origin.z + height, origin.y);
        glColor4fv(black);
        glVertex3f(origin.x, origin.z + height + EXTEND_DIST, origin.y);
    glEnd();
}

static String labelForVertex(Vertex const *vtx)
{
    DENG_ASSERT(vtx != 0);
    return String("%1").arg(vtx->indexInMap());
}

struct drawVertexVisual_params_t
{
    int maxDistance;
    bool drawOrigin;
    bool drawBar;
    bool drawLabel;
    QBitArray *drawnVerts;
};

static void drawVertexVisual(Vertex const &vertex, ddouble minHeight, ddouble maxHeight,
    drawVertexVisual_params_t &parms)
{
    if(!parms.drawOrigin && !parms.drawBar && !parms.drawLabel)
        return;

    // Skip vertexes produced by the space partitioner.
    if(vertex.indexInArchive() == MapElement::NoIndex)
        return;

    // Skip already processed verts?
    if(parms.drawnVerts)
    {
        if(parms.drawnVerts->testBit(vertex.indexInArchive()))
            return;
        parms.drawnVerts->setBit(vertex.indexInArchive());
    }

    // Distance in 2D determines visibility/opacity.
    ddouble distToEye = (Vector2d(eyeOrigin.x, eyeOrigin.y) - vertex.origin()).length();
    if(distToEye >= parms.maxDistance)
        return;

    Vector3d const origin(vertex.origin(), minHeight);
    float const opacity = 1 - distToEye / parms.maxDistance;

    if(parms.drawBar)
    {
        drawBar(origin, maxHeight - minHeight, opacity);
    }
    if(parms.drawOrigin)
    {
        drawPoint(origin, opacity * 2);
    }
    if(parms.drawLabel)
    {
        drawLabel(origin, labelForVertex(&vertex),
                  distToEye / (DENG_GAMEVIEW_WIDTH / 2), opacity);
    }
}

/**
 * Find the relative next minmal and/or maximal visual height(s) of all sector
 * planes which "interface" at the half-edge, edge vertex.
 *
 * @param base  Base half-edge to find heights for.
 * @param edge  Edge of the half-edge.
 * @param min   Current minimal height to use as a base (will be overwritten).
 *              Use DDMAXFLOAT if the base is unknown. Can be @c 0.
 * @param min   Current maximal height to use as a base (will be overwritten).
 *              Use DDMINFLOAT if the base is unknown. Can be @c 0.
 *
 * @todo Don't stop when a zero-volume back neighbor is found; process all of
 * the neighbors at the specified vertex (the half-edge geometry will need to
 * be linked such that "outside" edges are neighbor-linked similarly to those
 * with a face).
 */
static void findMinMaxPlaneHeightsAtVertex(HEdge *base, int edge,
    ddouble &min, ddouble &max)
{
    if(!base) return;
    if(!base->hasFace() || !base->face().hasMapElement())
        return;

    if(!base->face().mapElementAs<ConvexSubspace>().hasCluster())
        return;

    // Process neighbors?
    if(!SectorCluster::isInternalEdge(base))
    {
        ClockDirection const direction = edge? Clockwise : Anticlockwise;
        HEdge *hedge = base;
        while((hedge = &SectorClusterCirculator::findBackNeighbor(*hedge, direction)) != base)
        {
            // Stop if there is no back subspace.
            ConvexSubspace *subspace = hedge->hasFace()? &hedge->face().mapElementAs<ConvexSubspace>() : 0;
            if(!subspace) break;

            if(subspace->cluster().visFloor().heightSmoothed() < min)
                min = subspace->cluster().visFloor().heightSmoothed();

            if(subspace->cluster().visCeiling().heightSmoothed() > max)
                max = subspace->cluster().visCeiling().heightSmoothed();
        }
    }
}

static int drawSubspaceVertexWorker(ConvexSubspace *subspace, void *context)
{
    drawVertexVisual_params_t &parms = *static_cast<drawVertexVisual_params_t *>(context);

    SectorCluster &cluster = subspace->cluster();

    ddouble min = cluster.  visFloor().heightSmoothed();
    ddouble max = cluster.visCeiling().heightSmoothed();

    HEdge *base  = subspace->poly().hedge();
    HEdge *hedge = base;
    do
    {
        ddouble edgeMin = min;
        ddouble edgeMax = max;
        findMinMaxPlaneHeightsAtVertex(hedge, 0 /*left edge*/, edgeMin, edgeMax);

        drawVertexVisual(hedge->vertex(), min, max, parms);

    } while((hedge = &hedge->next()) != base);

    foreach(Mesh *mesh, subspace->extraMeshes())
    foreach(HEdge *hedge, mesh->hedges())
    {
        drawVertexVisual(hedge->vertex(), min, max, parms);
        drawVertexVisual(hedge->twin().vertex(), min, max, parms);
    }

    foreach(Polyobj *polyobj, subspace->polyobjs())
    foreach(Line *line, polyobj->lines())
    {
        drawVertexVisual(line->from(), min, max, parms);
        drawVertexVisual(line->to(), min, max, parms);
    }

    return false; // Continue iteration.
}

/**
 * Draw the various vertex debug aids.
 */
static void drawVertexes(Map &map)
{
#define MAX_DISTANCE            1280 ///< From the viewer.

    float oldLineWidth = -1;

    if(!devVertexBars && !devVertexIndices)
        return;

    AABoxd box(eyeOrigin.x - MAX_DISTANCE, eyeOrigin.y - MAX_DISTANCE,
               eyeOrigin.x + MAX_DISTANCE, eyeOrigin.y + MAX_DISTANCE);

    QBitArray drawnVerts(map.vertexCount());
    drawVertexVisual_params_t parms;
    parms.maxDistance = MAX_DISTANCE;
    parms.drawnVerts  = &drawnVerts;

    if(devVertexBars)
    {
        glDisable(GL_DEPTH_TEST);

        glEnable(GL_LINE_SMOOTH);
        oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
        DGL_SetFloat(DGL_LINE_WIDTH, 2);

        parms.drawBar = true;
        parms.drawLabel = parms.drawOrigin = false;
        map.subspaceBoxIterator(box, drawSubspaceVertexWorker, &parms);

        glEnable(GL_DEPTH_TEST);
    }

    // Draw the vertex origins.
    float const oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);

    glEnable(GL_POINT_SMOOTH);
    DGL_SetFloat(DGL_POINT_SIZE, 6);

    glDisable(GL_DEPTH_TEST);

    parms.drawnVerts->fill(false); // Process all again.
    parms.drawOrigin = true;
    parms.drawBar = parms.drawLabel = false;
    map.subspaceBoxIterator(box, drawSubspaceVertexWorker, &parms);

    glEnable(GL_DEPTH_TEST);

    if(devVertexIndices)
    {
        parms.drawnVerts->fill(false); // Process all again.
        parms.drawLabel = true;
        parms.drawBar = parms.drawOrigin = false;
        map.subspaceBoxIterator(box, drawSubspaceVertexWorker, &parms);
    }

    // Restore previous state.
    if(devVertexBars)
    {
        DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);
        glDisable(GL_LINE_SMOOTH);
    }
    DGL_SetFloat(DGL_POINT_SIZE, oldPointSize);
    glDisable(GL_POINT_SMOOTH);

#undef MAX_VERTEX_POINT_DIST
}

static String labelForCluster(SectorCluster const *cluster)
{
    DENG_ASSERT(cluster != 0);
    return String("%1").arg(cluster->sector().indexInMap());
}

/**
 * Draw the sector cluster debugging aids.
 */
static void drawSectors(Map &map)
{
#define MAX_LABEL_DIST 1280

    if(!devSectorIndices) return;

    // Draw per-cluster sector labels:

    foreach(SectorCluster *cluster, map.clusters())
    {
        Vector3d const origin(cluster->center(), cluster->visPlane(Sector::Floor).heightSmoothed());
        ddouble const distToEye = (eyeOrigin - origin).length();
        if(distToEye < MAX_LABEL_DIST)
        {
            drawLabel(origin, labelForCluster(cluster),
                      distToEye / (DENG_GAMEVIEW_WIDTH / 2),
                      1 - distToEye / MAX_LABEL_DIST);
        }
    }

#undef MAX_LABEL_DIST
}

static String labelForThinker(thinker_t *thinker)
{
    DENG_ASSERT(thinker != 0);
    return String("%1").arg(thinker->id);
}

static int drawThinkersWorker(thinker_t *thinker, void *context)
{
    DENG2_UNUSED(context);

#define MAX_THINKER_DIST 2048

    // Skip non-mobjs.
    if(!Thinker_IsMobjFunc(thinker->function))
        return false;

    Vector3d const origin = Mobj_Center(*(mobj_t *)thinker);
    ddouble const distToEye = (eyeOrigin - origin).length();
    if(distToEye < MAX_THINKER_DIST)
    {
        drawLabel(origin, labelForThinker(thinker),
                  distToEye / (DENG_GAMEVIEW_WIDTH / 2),
                  1 - distToEye / MAX_THINKER_DIST);
    }

    return false; // Continue iteration.

#undef MAX_THINKER_DIST
}

/**
 * Debugging aid for visualizing thinker IDs.
 */
static void drawThinkers(Map &map)
{
    if(!devThinkerIds) return;
    map.thinkers().iterate(NULL, 0x1 | 0x2, drawThinkersWorker);
}

void Rend_LightGridVisual(LightGrid &lg)
{
    static Vector3f const red(1, 0, 0);
    static int blink = 0;

    // Disabled?
    if(!devLightGrid) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Determine the grid reference of the view player.
    LightGrid::Index viewerGridIndex;
    if(viewPlayer)
    {
        blink++;
        viewerGridIndex = lg.toIndex(lg.toRef(viewPlayer->shared.mo->origin));
    }

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);

    for(int y = 0; y < lg.dimensions().y; ++y)
    {
        glBegin(GL_QUADS);
        for(int x = 0; x < lg.dimensions().x; ++x)
        {
            LightGrid::Index gridIndex = lg.toIndex(x, lg.dimensions().y - 1 - y);
            bool const isViewerIndex   = (viewPlayer && viewerGridIndex == gridIndex);

            Vector3f const *color = 0;
            if(isViewerIndex && (blink & 16))
            {
                color = &red;
            }
            else if(lg.primarySource(gridIndex))
            {
                color = &lg.rawColorRef(gridIndex);
            }

            if(!color) continue;

            glColor3f(color->x, color->y, color->z);

            glVertex2f(x * devLightGridSize, y * devLightGridSize);
            glVertex2f(x * devLightGridSize + devLightGridSize, y * devLightGridSize);
            glVertex2f(x * devLightGridSize + devLightGridSize,
                       y * devLightGridSize + devLightGridSize);
            glVertex2f(x * devLightGridSize, y * devLightGridSize + devLightGridSize);
        }
        glEnd();
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec(int wrapS, int wrapT)
{
    return ClientApp::resourceSystem().materialSpec(MapSurfaceContext, 0, 0, 0, 0,
                                                    wrapS, wrapT, -1, -1, -1, true,
                                                    true, false, false);
}

MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec()
{
    return Rend_MapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT);
}

TextureVariantSpec const &Rend_MapSurfaceShinyTextureSpec()
{
    return ClientApp::resourceSystem().textureSpec(TC_MAPSURFACE_REFLECTION,
        TSF_NO_COMPRESSION, 0, 0, 0, GL_REPEAT, GL_REPEAT, 1, 1, -1, false,
        false, false, false);
}

TextureVariantSpec const &Rend_MapSurfaceShinyMaskTextureSpec()
{
    return ClientApp::resourceSystem().textureSpec(TC_MAPSURFACE_REFLECTIONMASK,
        0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, false, false, false);
}

D_CMD(OpenRendererAppearanceEditor)
{
    DENG2_UNUSED3(src, argc, argv);

    if(!App_GameLoaded())
    {
        LOG_ERROR("A game must be loaded before the Renderer Appearance editor can be opened");
        return false;
    }

    if(!ClientWindow::main().hasSidebar())
    {
        // The editor sidebar will give its ownership automatically
        // to the window.
        RendererAppearanceEditor *editor = new RendererAppearanceEditor;
        editor->open();
    }
    return true;
}

D_CMD(LowRes)
{
    DENG2_UNUSED3(src, argv, argc);

    // Set everything as low as they go.
    filterSprites = 0;
    filterUI      = 0;
    texMagMode    = 0;

    //GL_SetAllTexturesMinFilter(GL_NEAREST);
    GL_SetRawTexturesMinFilter(GL_NEAREST);

    // And do a texreset so everything is updated.
    GL_TexReset();
    return true;
}

D_CMD(TexReset)
{
    DENG2_UNUSED(src);

    if(argc == 2 && !String(argv[1]).compareWithoutCase("raw"))
    {
        // Reset just raw images.
        GL_ReleaseTexturesForRawImages();
    }
    else
    {
        // Reset everything.
        GL_TexReset();
    }
    return true;
}

D_CMD(MipMap)
{
    DENG2_UNUSED2(src, argc);

    int newMipMode = String(argv[1]).toInt();
    if(newMipMode < 0 || newMipMode > 5)
    {
        LOG_SCR_ERROR("Invalid mipmapping mode %i; the valid range is 0...5") << newMipMode;
        return false;
    }

    mipmapping = newMipMode;
    //GL_SetAllTexturesMinFilter(glmode[mipmapping]);
    return true;
}
