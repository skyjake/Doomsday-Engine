/** @file rend_main.cpp World Map Renderer.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <QtAlgorithms>
#include <QBitArray>

#include <de/vector1.h>
#include <de/libdeng2.h>
#include <de/GLState>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_resource.h"
#include "de_graphics.h"
#include "de_ui.h"
#include "clientapp.h"

#include "ui/editors/rendererappearanceeditor.h"

#include "edit_bias.h" /// @todo remove me
#include "network/net_main.h" /// @todo remove me

#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "Texture"

#include "Face"

#include "BspLeaf"
#include "BspNode"
#include "Hand"
#include "Surface"
#include "world/map.h"
#include "world/generators.h"
#include "world/lineowner.h"
#include "world/p_object.h"
#include "Contact"
#include "world/p_players.h"
#include "world/thinkers.h"

#include "BiasIllum"
#include "BiasSurface"
#include "HueCircleVisual"
#include "LightDecoration"
#include "Lumobj"
#include "SkyFixEdge"
#include "SurfaceDecorator"
#include "TriangleStripBuilder"
#include "WallEdge"
#include "render/blockmapvisual.h"
#include "render/billboard.h"
#include "render/vissprite.h"
#include "render/fx/vignette.h"
#include "render/fx/lensflares.h"
#include "render/vr.h"

#include "gl/gl_texmanager.h"
#include "gl/sys_opengl.h"

#include "render/rend_main.h"

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

boolean usingFog; // Is the fog in use?
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
boolean fillOutlines = true;
int useSmartFilter = 0; // Smart filter mode (cvar: 1=hq2x)
int filterSprites = true;
int texMagMode = 1; // Linear.
int texAniso = -1; // Use best.

boolean noHighResTex = false;
boolean noHighResPatches = false;
boolean highResWithPWAD = false;
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

coord_t vOrigin[3];
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

byte devMobjVLights;    ///< @c 1= Draw mobj vertex lighting vector.
int devMobjBBox;        ///< @c 1= Draw mobj bounding boxes.
int devPolyobjBBox;     ///< @c 1= Draw polyobj bounding boxes.

byte devVertexIndices;  ///< @c 1= Draw vertex indices.
byte devVertexBars;     ///< @c 1= Draw vertex position bars.

byte devDrawGenerators; ///< @c 1= Draw active generators.
byte devSoundEmitters;  ///< @c 1= Draw sound emitters.
byte devSurfaceVectors; ///< @c 1= Draw tangent space vectors for surfaces.
byte devNoTexFix;       ///< @c 1= Draw "missing" rather than fix materials.

byte devSectorIndices;  ///< @c 1= Draw sector indicies.
byte devThinkerIds;     ///< @c 1= Draw (mobj) thinker indicies.

byte rendInfoLums;      ///< @c 1= Print lumobj debug info to the console.
byte devDrawLums;       ///< @c 1= Draw lumobjs origins.

static void drawMobjBoundingBoxes(Map &map);
static void drawSoundEmitters(Map &map);
static void drawGenerators(Map &map);
static void drawAllSurfaceTangentVectors(Map &map);
static void drawBiasEditingVisuals(Map &map);
static void drawLumobjs(Map &map);
static void drawSectors(Map &map);
static void drawThinkers(Map &map);
static void drawVertexes(Map &map);

// Draw state:
static Vector3d eyeOrigin; // Viewer origin.
static BspLeaf *currentBspLeaf; // BSP leaf currently being drawn.
static Vector3f currentSectorLightColor;
static float currentSectorLightLevel;
static bool firstBspLeaf; // No range checking for the first one.

static void markLightGridForFullUpdate()
{
    if(App_World().hasMap())
    {
        Map &map = App_World().map();
        if(map.hasLightGrid())
            map.lightGrid().markAllForUpdate();
    }
}

static int unlinkMobjLumobjWorker(thinker_t *th, void *)
{
    Mobj_UnlinkLumobjs(reinterpret_cast<mobj_t *>(th));
    return false; // Continue iteration.
}

static void unlinkMobjLumobjs()
{
    if(App_World().hasMap())
    {
        Map &map = App_World().map();
        map.thinkers().iterate(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1,
                               unlinkMobjLumobjWorker);
    }
}

static void fieldOfViewChanged() {
    if (VR::mode() == VR::MODE_OCULUS_RIFT) {
        if (Con_GetFloat("rend-vr-rift-fovx") != fieldOfView)
            Con_SetFloat("rend-vr-rift-fovx", fieldOfView);
    }
    else
    {
        if (Con_GetFloat("rend-vr-nonrift-fovx") != fieldOfView)
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
    //LOG_MSG("Gamma correction set to %f.") << texGamma;
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
    C_VAR_FLOAT2("rend-light-sky",                  &rendSkyLight,                  0, 0, 1, markLightGridForFullUpdate);
    C_VAR_BYTE2 ("rend-light-sky-auto",             &rendSkyLightAuto,              0, 0, 1, markLightGridForFullUpdate);
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
    C_VAR_BYTE  ("rend-dev-soundorigins",           &devSoundEmitters,               CVF_NO_ARCHIVE, 0, 7);
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
    BiasSurface::consoleRegister();
    LightDecoration::consoleRegister();
    LightGrid::consoleRegister();
    Lumobj::consoleRegister();
    Sky_Register();
    Rend_ModelRegister();
    Rend_ParticleRegister();
    Rend_RadioRegister();
    Rend_SpriteRegister();
    LensFx_Register();
    fx::Vignette::consoleRegister();
    fx::LensFlares::consoleRegister();
    VR::consoleRegister();
}

static void reportWallSectionDrawn(Line &line)
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
    Sky_Init();
}

void Rend_Shutdown()
{
    ClientApp::renderSystem().clearDrawLists();
}

/// World/map renderer reset.
void Rend_Reset()
{
    R_ClearViewData();
    if(App_World().hasMap())
    {
        App_World().map().removeAllLumobjs();
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
    if (VR::mode() == VR::MODE_OCULUS_RIFT)
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

    vOrigin[VX] = viewData->current.origin.x;
    vOrigin[VY] = viewData->current.origin.z;
    vOrigin[VZ] = viewData->current.origin.y;
    vang = viewData->current.angle / (float) ANGLE_MAX *360 - 90;
    vpitch = viewData->current.pitch * 85.0 / 110.0;

    Matrix4f modelView;

    if(useAngles)
    {
        bool scheduledLate = false;
        float pitch, roll, yaw;
        static float storedPitch, storedRoll, storedYaw;

        if (VR::viewPositionHeld()) {
            roll = storedRoll;
            pitch = storedPitch;
            yaw = storedYaw;
        }
        else {
            /// Hard code Oculus Rift roll angle directly into OpenGL ModelView matrix
            /// @todo Elevate roll angle use into viewer_t, and maybe all the way up into player model.
            if ( (VR::mode() == VR::MODE_OCULUS_RIFT) && (VR::hasHeadOrientation()) )
            {
                std::vector<float> pry = VR::getHeadOrientation();
                if (pry.size() == 3)
                {
                    static float previousRiftYaw = 0;
                    static float previousVang2 = 0;

                    // Desired view angle without interpolation?
                    float vang2 = viewData->latest.angle / (float) ANGLE_MAX *360 - 90;

                    // Late-scheduled update
                    scheduledLate = true;
                    roll = -radianToDegree(pry[1]);
                    pitch = radianToDegree(pry[0]);

                    // Yaw might have a contribution from mouse/keyboard.
                    // So only late schedule if it seems to be head tracking only.
                    yaw = vang2; // default no late schedule
                    float currentRiftYaw = radianToDegree(pry[2]);
                    float dRiftYaw = currentRiftYaw - previousRiftYaw;
                    while (dRiftYaw > 180) dRiftYaw -= 360;
                    while (dRiftYaw < -180) dRiftYaw += 360;
                    float dVang = vang2 - previousVang2;
                    while (dVang > 180) dVang -= 360;
                    while (dVang < -180) dVang += 360;
                    if (abs(dVang) < 2.0 * abs(dRiftYaw)) // Mostly head motion
                    {
                        yaw = storedYaw + dRiftYaw;
                        float dy = vang2 - yaw;
                        while (dy > 180) dy -= 360;
                        while (dy < -180) dy += 360;
                        yaw += 0.05 * dy; // ease slowly toward target angle
                    }
                    else
                    {
                        yaw = vang2; // No interpolation if not from head tracker
                    }

                    previousRiftYaw = currentRiftYaw;
                    previousVang2 = vang2;
                }
            }
            if (! scheduledLate)
            {
                // Vanilla angle update
                roll = 0;
                pitch = vpitch;
                yaw = vang;
            }
        }

        modelView = Matrix4f::rotate(roll,  Vector3f(0, 0, 1)) *
                    Matrix4f::rotate(pitch, Vector3f(1, 0, 0)) *
                    Matrix4f::rotate(yaw,   Vector3f(0, 1, 0));

        storedRoll = roll;
        storedPitch = pitch;
        storedYaw = yaw;
    }

    return (modelView *
            Matrix4f::scale(Vector3f(1.0f, 1.2f, 1.0f)) * // This is the aspect correction.
            Matrix4f::translate(-Vector3f(vOrigin[VX], vOrigin[VY], vOrigin[VZ])));
}

void Rend_ModelViewMatrix(bool useAngles)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(Rend_GetModelViewMatrix(viewPlayer - ddPlayers, useAngles).values());
}

static inline double viewFacingDot(Vector2d const &v1, Vector2d const &v2)
{
    // The dot product.
    return (v1.y - v2.y) * (v1.x - vOrigin[VX]) + (v2.x - v1.x) * (v1.y - vOrigin[VZ]);
}

static void Rend_VertexColorsGlow(Vector4f *colors, uint num, float glow)
{
    for(uint i = 0; i < num; ++i)
    {
        colors[i].x = colors[i].y = colors[i].z = glow;
    }
}

static void Rend_VertexColorsAlpha(Vector4f *colors, uint num, float alpha)
{
    for(uint i = 0; i < num; ++i)
    {
        colors[i].w = alpha;
    }
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

Vector3f const &Rend_SectorLightColor(SectorCluster const &cluster)
{
    if(rendSkyLight > .001f && cluster.hasSkyMaskedPlane())
    {
        ColorRawf const *ambientColor = Sky_AmbientColor();

        if(rendSkyLight != oldRendSkyLight ||
           !INRANGE_OF(ambientColor->red,   oldSkyAmbientColor.x, .001f) ||
           !INRANGE_OF(ambientColor->green, oldSkyAmbientColor.y, .001f) ||
           !INRANGE_OF(ambientColor->blue,  oldSkyAmbientColor.z, .001f))
        {
            skyLightColor = Vector3f(ambientColor->rgb);
            R_AmplifyColor(skyLightColor);

            // Apply the intensity factor cvar.
            for(int i = 0; i < 3; ++i)
            {
                skyLightColor[i] = skyLightColor[i] + (1 - rendSkyLight) * (1.f - skyLightColor[i]);
            }

            // When the sky light color changes we must update the light grid.
            markLightGridForFullUpdate();
            oldSkyAmbientColor = Vector3f(ambientColor->rgb);
        }

        oldRendSkyLight = rendSkyLight;
        return skyLightColor;
    }

    // A non-skylight sector (i.e., everything else!)
    // Return the sector's ambient light color.
    return cluster.sector().lightColor();
}

static bool sectorHasSkyMaskedPlane(Sector const &sector)
{
    foreach(Plane *plane, sector.planes())
    {
        if(plane->surface().hasSkyMaskedMaterial())
            return true;
    }
    return false;
}

/// @todo refactor away.
Vector3f const &Rend_SectorLightColor(Sector const &sector)
{
    if(rendSkyLight > .001f && sectorHasSkyMaskedPlane(sector))
    {
        ColorRawf const *ambientColor = Sky_AmbientColor();

        if(rendSkyLight != oldRendSkyLight ||
           !INRANGE_OF(ambientColor->red,   oldSkyAmbientColor.x, .001f) ||
           !INRANGE_OF(ambientColor->green, oldSkyAmbientColor.y, .001f) ||
           !INRANGE_OF(ambientColor->blue,  oldSkyAmbientColor.z, .001f))
        {
            skyLightColor = Vector3f(ambientColor->rgb);
            R_AmplifyColor(skyLightColor);

            // Apply the intensity factor cvar.
            for(int i = 0; i < 3; ++i)
            {
                skyLightColor[i] = skyLightColor[i] + (1 - rendSkyLight) * (1.f - skyLightColor[i]);
            }

            // When the sky light color changes we must update the light grid.
            markLightGridForFullUpdate();
            oldSkyAmbientColor = Vector3f(ambientColor->rgb);
        }

        oldRendSkyLight = rendSkyLight;
        return skyLightColor;
    }

    // A non-skylight sector (i.e., everything else!)
    // Return the sector's ambient light color.
    return sector.lightColor();
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

static void lightVertex(Vector4f &color, Vector3f const &vtx, float lightLevel,
                        Vector3f const &ambientColor)
{
    float const dist = Rend_PointDist2D(vtx);

    // Apply distance attenuation.
    lightLevel = Rend_AttenuateLightLevel(dist, lightLevel);

    // Add extra light.
    lightLevel = de::clamp(0.f, lightLevel + Rend_ExtraLightDelta(), 1.f);

    Rend_ApplyLightAdaptation(lightLevel);

    // Mix with the surface color.
    for(int i = 0; i < 3; ++i)
    {
        color[i] = lightLevel * ambientColor[i];
    }
}

static void lightVertices(uint num, Vector4f *colors, Vector3f const *verts,
                          float lightLevel, Vector3f const &ambientColor)
{
    for(uint i = 0; i < num; ++i)
    {
        lightVertex(colors[i], verts[i], lightLevel, ambientColor);
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

    // Masked walls are sometimes used for special effects like arcs,
    // cobwebs and bottoms of sails. In order for them to look right,
    // we need to disable texture wrapping on the horizontal axis (S).
    // Most masked walls need wrapping, though. What we need to do is
    // look at the texture coordinates and see if they require texture
    // wrapping.
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

        /**
         * The dynlights will have already been sorted so that the brightest
         * and largest of them is first in the list. So grab that one.
         */
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

static void quadTexCoords(Vector2f *tc, Vector3f const *rverts,
    coord_t wallLength, Vector3d const &topLeft)
{
    tc[0].x = tc[1].x = rverts[0].x - topLeft.x;
    tc[3].y = tc[1].y = rverts[0].y - topLeft.y;
    tc[3].x = tc[2].x = tc[0].x + wallLength;
    tc[2].y = tc[3].y + (rverts[1].z - rverts[0].z);
    tc[0].y = tc[3].y + (rverts[3].z - rverts[2].z);
}

static void quadLightCoords(Vector2f *tc, Vector2f const &topLeft, Vector2f const &bottomRight)
{
    tc[1].x = tc[0].x = topLeft.x;
    tc[1].y = tc[3].y = topLeft.y;
    tc[3].x = tc[2].x = bottomRight.x;
    tc[2].y = tc[0].y = bottomRight.y;
}

static float shinyVertical(float dy, float dx)
{
    return ((atan(dy/dx) / (PI/2)) + 1) / 2;
}

static void quadShinyTexCoords(Vector2f *tc, Vector3f const *topLeft,
    Vector3f const *bottomRight, coord_t wallLength)
{
    vec2f_t surface, normal, projected, s, reflected, view;
    float distance, angle, prevAngle = 0;
    uint i;

    // Quad surface vector.
    V2f_Set(surface, (bottomRight->x - topLeft->x) / wallLength,
                     (bottomRight->y - topLeft->y) / wallLength);

    V2f_Set(normal, surface[VY], -surface[VX]);

    // Calculate coordinates based on viewpoint and surface normal.
    for(i = 0; i < 2; ++i)
    {
        // View vector.
        V2f_Set(view, vOrigin[VX] - (i == 0? topLeft->x : bottomRight->x),
                      vOrigin[VZ] - (i == 0? topLeft->y : bottomRight->y));

        distance = V2f_Normalize(view);

        V2f_Project(projected, view, normal);
        V2f_Subtract(s, projected, view);
        V2f_Scale(s, 2);
        V2f_Sum(reflected, view, s);

        angle = acos(reflected[VY]) / PI;
        if(reflected[VX] < 0)
        {
            angle = 1 - angle;
        }

        if(i == 0)
        {
            prevAngle = angle;
        }
        else
        {
            if(angle > prevAngle)
                angle -= 1;
        }

        // Horizontal coordinates.
        tc[ (i == 0 ? 1 : 2) ].x =
            tc[ (i == 0 ? 0 : 3) ].x = angle + .3f; /*acos(-dot)/PI*/

        // Vertical coordinates.
        tc[ (i == 0 ? 0 : 2) ].y = shinyVertical(vOrigin[VY] - bottomRight->z, distance);
        tc[ (i == 0 ? 1 : 3) ].y = shinyVertical(vOrigin[VY] - topLeft->z, distance);
    }
}

static void flatShinyTexCoords(Vector2f *tc, Vector3f const &point)
{
    // Determine distance to viewer.
    float distToEye = Vector2f(vOrigin[VX] - point.x, vOrigin[VZ] - point.y)
                          .normalize().length();
    if(distToEye < 10)
    {
        // Too small distances cause an ugly 'crunch' below and above
        // the viewpoint.
        distToEye = 10;
    }

    // Offset from the normal view plane.
    Vector2f start(vOrigin[VX], vOrigin[VZ]);

    float offset = ((start.y - point.y) * sin(.4f)/*viewFrontVec[VX]*/ -
                    (start.x - point.x) * cos(.4f)/*viewFrontVec[VZ]*/);

    tc->x = ((shinyVertical(offset, distToEye) - .5f) * 2) + .5f;
    tc->y = shinyVertical(vOrigin[VY] - point.z, distToEye);
}

struct rendworldpoly_params_t
{
    //int             flags; /// @ref rendpolyFlags
    bool            skyMasked;
    blendmode_t     blendMode;
    Vector3d const *topLeft;
    Vector3d const *bottomRight;
    Vector2f const *materialOrigin;
    Vector2f const *materialScale;
    float           alpha;
    float           surfaceLightLevelDL;
    float           surfaceLightLevelDR;
    Vector3f const *surfaceColor;

    uint            lightListIdx; // List of lights that affect this poly.
    uint            shadowListIdx; // List of shadows that affect this poly.
    float           glowing;
    bool            forceOpaque;
    BiasSurface    *bsuf;
    int             geomGroup;

    bool            isWall;
// Wall only:
    struct {
        coord_t sectionWidth;
        Vector3f const *surfaceColor2; // Secondary color.
        WallEdge const *leftEdge;
        WallEdge const *rightEdge;
    } wall;
};

static bool renderWorldPoly(Vector3f *posCoords, uint numVertices,
    rendworldpoly_params_t const &p, MaterialSnapshot const &ms)
{
    BspLeaf *leaf = currentBspLeaf;

    DENG_ASSERT(posCoords != 0);

    uint const realNumVertices   = (p.isWall? 3 + p.wall.leftEdge->divisionCount() + 3 + p.wall.rightEdge->divisionCount() : numVertices);
    bool const mustSubdivide     = (p.isWall && (p.wall.leftEdge->divisionCount() || p.wall.rightEdge->divisionCount()));

    bool const skyMaskedMaterial = (p.skyMasked || (ms.material().isSkyMasked()));
    bool const drawAsVisSprite   = (!p.forceOpaque && !p.skyMasked && (!ms.isOpaque() || p.alpha < 1 || p.blendMode > 0));

    bool useLights = false, useShadows = false, hasDynlights = false;

    // Map RTU configuration from prepared MaterialSnapshot(s).
    GLTextureUnit const *primaryRTU       = (!p.skyMasked)? &ms.unit(RTU_PRIMARY) : NULL;
    GLTextureUnit const *primaryDetailRTU = (r_detail && !p.skyMasked && ms.unit(RTU_PRIMARY_DETAIL).hasTexture())? &ms.unit(RTU_PRIMARY_DETAIL) : NULL;
    GLTextureUnit const *interRTU         = (!p.skyMasked && ms.unit(RTU_INTER).hasTexture())? &ms.unit(RTU_INTER) : NULL;
    GLTextureUnit const *interDetailRTU   = (r_detail && !p.skyMasked && ms.unit(RTU_INTER_DETAIL).hasTexture())? &ms.unit(RTU_INTER_DETAIL) : NULL;
    GLTextureUnit const *shinyRTU         = (useShinySurfaces && !p.skyMasked && ms.unit(RTU_REFLECTION).hasTexture())? &ms.unit(RTU_REFLECTION) : NULL;
    GLTextureUnit const *shinyMaskRTU     = (useShinySurfaces && !p.skyMasked && ms.unit(RTU_REFLECTION).hasTexture() && ms.unit(RTU_REFLECTION_MASK).hasTexture())? &ms.unit(RTU_REFLECTION_MASK) : NULL;

    Vector4f *colorCoords    = !skyMaskedMaterial? R_AllocRendColors(realNumVertices) : 0;
    Vector2f *primaryCoords  = R_AllocRendTexCoords(realNumVertices);
    Vector2f *interCoords    = interRTU? R_AllocRendTexCoords(realNumVertices) : 0;

    Vector4f *shinyColors    = 0;
    Vector2f *shinyTexCoords = 0;
    Vector2f *modCoords      = 0;

    DGLuint modTex = 0;
    Vector2f modTexSt[2]; // [topLeft, bottomRight]
    Vector3f modColor;

    if(!skyMaskedMaterial)
    {
        // ShinySurface?
        if(shinyRTU && !drawAsVisSprite)
        {
            // We'll reuse the same verts but we need new colors.
            shinyColors = R_AllocRendColors(realNumVertices);
            // The normal texcoords are used with the mask.
            // New texcoords are required for shiny texture.
            shinyTexCoords = R_AllocRendTexCoords(realNumVertices);
        }

        if(p.glowing < 1)
        {
            useLights  = (p.lightListIdx ? true : false);
            useShadows = (p.shadowListIdx? true : false);

            /**
             * If multitexturing is enabled and there is at least one
             * dynlight affecting this surface, grab the parameters
             * needed to draw it.
             */
            if(useLights && Rend_IsMTexLights())
            {
                TexProjection *dyn = 0;
                Rend_IterateProjectionList(p.lightListIdx, RIT_FirstDynlightIterator, (void *)&dyn);

                modTex      = dyn->texture;
                modCoords   = R_AllocRendTexCoords(realNumVertices);
                modColor    = dyn->color;
                modTexSt[0] = dyn->topLeft;
                modTexSt[1] = dyn->bottomRight;
            }
        }
    }

    if(p.isWall)
    {
        // Primary texture coordinates.
        quadTexCoords(primaryCoords, posCoords, p.wall.sectionWidth, *p.topLeft);

        // Blend texture coordinates.
        if(interRTU && !drawAsVisSprite)
            quadTexCoords(interCoords, posCoords, p.wall.sectionWidth, *p.topLeft);

        // Shiny texture coordinates.
        if(shinyRTU && !drawAsVisSprite)
            quadShinyTexCoords(shinyTexCoords, &posCoords[1], &posCoords[2], p.wall.sectionWidth);

        // First light texture coordinates.
        if(modTex && Rend_IsMTexLights())
            quadLightCoords(modCoords, modTexSt[0], modTexSt[1]);
    }
    else
    {
        for(uint i = 0; i < numVertices; ++i)
        {
            Vector3f const &vtx = posCoords[i];
            Vector3f const delta(vtx - *p.topLeft);

            // Primary texture coordinates.
            if(primaryRTU)
            {
                primaryCoords[i] = Vector2f(delta.x, -delta.y);
            }

            // Blend primary texture coordinates.
            if(interRTU)
            {
                interCoords[i] = Vector2f(delta.x, -delta.y);
            }

            // Shiny texture coordinates.
            if(shinyRTU)
            {
                flatShinyTexCoords(&shinyTexCoords[i], vtx);
            }

            // First light texture coordinates.
            if(modTex && Rend_IsMTexLights())
            {
                float const width  = p.bottomRight->x - p.topLeft->x;
                float const height = p.bottomRight->y - p.topLeft->y;

                modCoords[i] = Vector2f(((p.bottomRight->x - vtx.x) / width  * modTexSt[0].x) + (delta.x / width  * modTexSt[1].x),
                                        ((p.bottomRight->y - vtx.y) / height * modTexSt[0].y) + (delta.y / height * modTexSt[1].y));
            }
        }
    }

    // Light this polygon.
    if(!skyMaskedMaterial)
    {
        if(levelFullBright || !(p.glowing < 1))
        {
            // Uniform color. Apply to all vertices.
            float glowStrength = currentSectorLightLevel + (levelFullBright? 1 : p.glowing);
            Rend_VertexColorsGlow(colorCoords, numVertices, glowStrength);
        }
        else
        {
            // Non-uniform color.
            if(useBias)
            {
                // Apply the ambient light term from the grid (if available).
                if(leaf->map().hasLightGrid())
                {
                    Vector3f const *posIt = posCoords;
                    Vector4f *colorIt = colorCoords;
                    for(uint i = 0; i < numVertices; ++i, posIt++, colorIt++)
                    {
                        *colorIt = leaf->map().lightGrid().evaluate(*posIt);
                    }
                }

                // Apply shadow bias contributions.
                p.bsuf->lightBiasPoly(p.geomGroup, posCoords, colorCoords);

                if(p.glowing > 0)
                {
                    static const Vector3f saturated(1, 1, 1);
                    float const glow = p.glowing;
                    Vector4f *colorIt = colorCoords;
                    for(uint i = 0; i < numVertices; ++i, colorIt++)
                    {
                        *colorIt = (Vector3f(*colorIt) + Vector3f(glow, glow, glow))
                                   .min(saturated); // clamp
                    }
                }
            }
            else
            {
                float llL = de::clamp(0.f, currentSectorLightLevel + p.surfaceLightLevelDL + p.glowing, 1.f);
                float llR = de::clamp(0.f, currentSectorLightLevel + p.surfaceLightLevelDR + p.glowing, 1.f);

                // Calculate the color for each vertex, blended with plane color?
                if(p.surfaceColor->x < 1 || p.surfaceColor->y < 1 || p.surfaceColor->z < 1)
                {
                    // Blend sector light+color+surfacecolor
                    Vector3f vColor = (*p.surfaceColor) * currentSectorLightColor;

                    if(p.isWall && llL != llR)
                    {
                        lightVertex(colorCoords[0], posCoords[0], llL, vColor);
                        lightVertex(colorCoords[1], posCoords[1], llL, vColor);
                        lightVertex(colorCoords[2], posCoords[2], llR, vColor);
                        lightVertex(colorCoords[3], posCoords[3], llR, vColor);
                    }
                    else
                    {
                        lightVertices(numVertices, colorCoords, posCoords, llL, vColor);
                    }
                }
                else
                {
                    // Use sector light+color only.
                    if(p.isWall && llL != llR)
                    {
                        lightVertex(colorCoords[0], posCoords[0], llL, currentSectorLightColor);
                        lightVertex(colorCoords[1], posCoords[1], llL, currentSectorLightColor);
                        lightVertex(colorCoords[2], posCoords[2], llR, currentSectorLightColor);
                        lightVertex(colorCoords[3], posCoords[3], llR, currentSectorLightColor);
                    }
                    else
                    {
                        lightVertices(numVertices, colorCoords, posCoords, llL, currentSectorLightColor);
                    }
                }

                // Bottom color (if different from top)?
                if(p.isWall && p.wall.surfaceColor2)
                {
                    // Blend sector light+color+surfacecolor
                    Vector3f vColor = (*p.wall.surfaceColor2) * currentSectorLightColor;

                    lightVertex(colorCoords[0], posCoords[0], llL, vColor);
                    lightVertex(colorCoords[2], posCoords[2], llR, vColor);
                }
            }

            // Apply torch light?
            if(viewPlayer->shared.fixedColorMap)
            {
                Vector3f const *posIt = posCoords;
                Vector4f *colorIt = colorCoords;
                for(uint i = 0; i < numVertices; ++i, colorIt++, posIt++)
                {
                    Rend_ApplyTorchLight(*colorIt, Rend_PointDist2D(*posIt));
                }
            }
        }

        if(shinyRTU && !drawAsVisSprite)
        {
            // Strength of the shine.
            Vector3f const &minColor = ms.shineMinColor();
            for(uint i = 0; i < numVertices; ++i)
            {
                Vector4f &color = shinyColors[i];
                color = Vector3f(colorCoords[i]).max(minColor);
                color.w = shinyRTU->opacity;
            }
        }

        // Apply uniform alpha.
        Rend_VertexColorsAlpha(colorCoords, numVertices, p.alpha);
    }

    if(useLights || useShadows)
    {
        /*
         * Surfaces lit by dynamic lights may need to be rendered differently
         * than non-lit surfaces. Determine the average light level of this rend
         * poly, if too bright; do not bother with lights.
         */
        float avgLightlevel = 0;
        for(uint i = 0; i < numVertices; ++i)
        {
            avgLightlevel += colorCoords[i].x;
            avgLightlevel += colorCoords[i].y;
            avgLightlevel += colorCoords[i].z;
        }
        avgLightlevel /= numVertices * 3;

        if(avgLightlevel > 0.98f)
        {
            useLights = false;
        }
        if(avgLightlevel < 0.02f)
        {
            useShadows = false;
        }
    }

    if(drawAsVisSprite)
    {
        DENG2_ASSERT(p.isWall);

        /*
         * Masked polys (walls) get a special treatment (=> vissprite). This is
         * needed because all masked polys must be sorted (sprites are masked
         * polys). Otherwise there will be artifacts.
         */
        Rend_AddMaskedPoly(posCoords, colorCoords, p.wall.sectionWidth, &ms.materialVariant(),
                           *p.materialOrigin, p.blendMode, p.lightListIdx, p.glowing);

        R_FreeRendTexCoords(primaryCoords);
        R_FreeRendColors(colorCoords);
        R_FreeRendTexCoords(interCoords);
        R_FreeRendTexCoords(modCoords);
        R_FreeRendTexCoords(shinyTexCoords);
        R_FreeRendColors(shinyColors);

        return false; // We HAD to use a vissprite, so it MUST not be opaque.
    }

    if(useLights)
    {
        // Render all lights projected onto this surface.
        renderlightprojectionparams_t parm;

        zap(parm);
        parm.rvertices       = posCoords;
        parm.numVertices     = numVertices;
        parm.realNumVertices = realNumVertices;
        parm.lastIdx         = 0;
        parm.topLeft         = p.topLeft;
        parm.bottomRight     = p.bottomRight;
        parm.isWall          = p.isWall;
        if(parm.isWall)
        {
            parm.wall.leftEdge  = p.wall.leftEdge;
            parm.wall.rightEdge = p.wall.rightEdge;
        }

        hasDynlights = (0 != Rend_RenderLightProjections(p.lightListIdx, parm));
    }

    if(useShadows)
    {
        // Render all shadows projected onto this surface.
        rendershadowprojectionparams_t parm;

        zap(parm);
        parm.rvertices       = posCoords;
        parm.numVertices     = numVertices;
        parm.realNumVertices = realNumVertices;
        parm.topLeft         = p.topLeft;
        parm.bottomRight     = p.bottomRight;
        parm.isWall          = p.isWall;
        if(parm.isWall)
        {
            parm.wall.leftEdge  = p.wall.leftEdge;
            parm.wall.rightEdge = p.wall.rightEdge;
        }

        Rend_RenderShadowProjections(p.shadowListIdx, parm);
    }

    // Write multiple polys depending on rend params.
    if(mustSubdivide)
    {
        WallEdge const &leftEdge = *p.wall.leftEdge;
        WallEdge const &rightEdge = *p.wall.rightEdge;

        /*
         * Need to swap indices around into fans set the position of the division
         * vertices, interpolate texcoords and color.
         */

        Vector3f origVerts[4];
        std::memcpy(origVerts, posCoords, sizeof(origVerts));

        Vector2f origTexCoords[4];
        std::memcpy(origTexCoords, primaryCoords, sizeof(origTexCoords));

        Vector4f origColors[4];
        if(colorCoords || shinyColors)
        {
            std::memcpy(origColors, colorCoords, sizeof(origColors));
        }

        R_DivVerts(posCoords, origVerts, leftEdge, rightEdge);
        R_DivTexCoords(primaryCoords, origTexCoords, leftEdge, rightEdge);

        if(colorCoords)
        {
            R_DivVertColors(colorCoords, origColors, leftEdge, rightEdge);
        }

        if(interCoords)
        {
            Vector2f origTexCoords2[4];
            std::memcpy(origTexCoords2, interCoords, sizeof(origTexCoords2));
            R_DivTexCoords(interCoords, origTexCoords2, leftEdge, rightEdge);
        }

        if(modCoords)
        {
            Vector2f origTexCoords5[4];
            std::memcpy(origTexCoords5, modCoords, sizeof(origTexCoords5));
            R_DivTexCoords(modCoords, origTexCoords5, leftEdge, rightEdge);
        }

        if(shinyTexCoords)
        {
            Vector2f origShinyTexCoords[4];
            std::memcpy(origShinyTexCoords, shinyTexCoords, sizeof(origShinyTexCoords));
            R_DivTexCoords(shinyTexCoords, origShinyTexCoords, leftEdge, rightEdge);
        }

        if(shinyColors)
        {
            Vector4f origShinyColors[4];
            std::memcpy(origShinyColors, shinyColors, sizeof(origShinyColors));
            R_DivVertColors(shinyColors, origShinyColors, leftEdge, rightEdge);
        }

        if(p.skyMasked)
        {
            ClientApp::renderSystem().drawLists()
                      .find(DrawListSpec(SkyMaskGeom))
                          .write(gl::TriangleFan,
                                 BM_NORMAL, Vector2f(1, 1), Vector2f(0, 0),
                                 Vector2f(1, 1), Vector2f(0, 0),
                                 0, 3 + rightEdge.divisionCount(),
                                 posCoords + 3 + leftEdge.divisionCount())
                          .write(gl::TriangleFan,
                                 BM_NORMAL, Vector2f(1, 1), Vector2f(0, 0),
                                 Vector2f(1, 1), Vector2f(0, 0),
                                 0, 3 + leftEdge.divisionCount(),
                                 posCoords);
        }
        else
        {
            DrawListSpec listSpec((modTex || hasDynlights)? LitGeom : UnlitGeom);

            if(primaryRTU)
            {
                listSpec.texunits[TU_PRIMARY] = *primaryRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY].offset += *p.materialOrigin;
                }
                if(p.materialScale)
                {
                    listSpec.texunits[TU_PRIMARY].scale  *= *p.materialScale;
                    listSpec.texunits[TU_PRIMARY].offset *= *p.materialScale;
                }
            }

            if(primaryDetailRTU)
            {
                listSpec.texunits[TU_PRIMARY_DETAIL] = *primaryDetailRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY_DETAIL].offset += *p.materialOrigin;
                }
            }

            if(interRTU)
            {
                listSpec.texunits[TU_INTER] = *interRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_INTER].offset += *p.materialOrigin;
                }
                if(p.materialScale)
                {
                    listSpec.texunits[TU_INTER].scale  *= *p.materialScale;
                    listSpec.texunits[TU_INTER].offset *= *p.materialScale;
                }
            }

            if(interDetailRTU)
            {
                listSpec.texunits[TU_INTER_DETAIL] = *interDetailRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_INTER_DETAIL].offset += *p.materialOrigin;
                }
            }

            ClientApp::renderSystem().drawLists()
                      .find(listSpec)
                          .write(gl::TriangleFan, BM_NORMAL,
                                 listSpec.unit(TU_PRIMARY       ).scale,
                                 listSpec.unit(TU_PRIMARY       ).offset,
                                 listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                 listSpec.unit(TU_PRIMARY_DETAIL).offset,
                                 hasDynlights, 3 + rightEdge.divisionCount(),
                                 posCoords + 3 + leftEdge.divisionCount(),
                                 colorCoords? colorCoords + 3 + leftEdge.divisionCount() : 0,
                                 primaryCoords + 3 + leftEdge.divisionCount(),
                                 interCoords? interCoords + 3 + leftEdge.divisionCount() : 0,
                                 modTex, &modColor, modCoords? modCoords + 3 + leftEdge.divisionCount() : 0)
                          .write(gl::TriangleFan, BM_NORMAL,
                                 listSpec.unit(TU_PRIMARY       ).scale,
                                 listSpec.unit(TU_PRIMARY       ).offset,
                                 listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                 listSpec.unit(TU_PRIMARY_DETAIL).offset,
                                 hasDynlights, 3 + leftEdge.divisionCount(),
                                 posCoords, colorCoords, primaryCoords,
                                 interCoords, modTex, &modColor, modCoords);

            if(shinyRTU)
            {
                DrawListSpec listSpec(ShineGeom);

                listSpec.texunits[TU_PRIMARY] = *shinyRTU;

                if(shinyMaskRTU)
                {
                    listSpec.texunits[TU_INTER] = *shinyMaskRTU;
                    if(p.materialOrigin)
                    {
                        listSpec.texunits[TU_INTER].offset += *p.materialOrigin;
                    }
                    if(p.materialScale)
                    {
                        listSpec.texunits[TU_INTER].scale  *= *p.materialScale;
                        listSpec.texunits[TU_INTER].offset *= *p.materialScale;
                    }
                }

                ClientApp::renderSystem().drawLists()
                          .find(listSpec)
                              .write(gl::TriangleFan, ms.shineBlendMode(),
                                     listSpec.unit(TU_INTER  ).scale,
                                     listSpec.unit(TU_INTER  ).offset,
                                     Vector2f(1, 1), Vector2f(0, 0),
                                     hasDynlights, 3 + rightEdge.divisionCount(),
                                     posCoords + 3 + leftEdge.divisionCount(),
                                     shinyColors + 3 + leftEdge.divisionCount(),
                                     shinyTexCoords? shinyTexCoords + 3 + leftEdge.divisionCount() : 0,
                                     shinyMaskRTU? primaryCoords + 3 + leftEdge.divisionCount() : 0)
                              .write(gl::TriangleFan, ms.shineBlendMode(),
                                     listSpec.unit(TU_INTER  ).scale,
                                     listSpec.unit(TU_INTER  ).offset,
                                     Vector2f(1, 1), Vector2f(0, 0),
                                     hasDynlights, 3 + leftEdge.divisionCount(),
                                     posCoords, shinyColors, shinyTexCoords,
                                     shinyMaskRTU? primaryCoords : 0);
            }
        }
    }
    else
    {
        if(p.skyMasked)
        {
            ClientApp::renderSystem().drawLists()
                      .find(DrawListSpec(SkyMaskGeom))
                          .write(p.isWall? gl::TriangleStrip : gl::TriangleFan,
                                 BM_NORMAL, Vector2f(1, 1), Vector2f(0, 0),
                                 Vector2f(1, 1), Vector2f(0, 0),
                                 0, numVertices,
                                 posCoords);
        }
        else
        {
            DrawListSpec listSpec((modTex || hasDynlights)? LitGeom : UnlitGeom);

            if(primaryRTU)
            {
                listSpec.texunits[TU_PRIMARY] = *primaryRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY].offset += *p.materialOrigin;
                }
                if(p.materialScale)
                {
                    listSpec.texunits[TU_PRIMARY].scale  *= *p.materialScale;
                    listSpec.texunits[TU_PRIMARY].offset *= *p.materialScale;
                }
            }

            if(primaryDetailRTU)
            {
                listSpec.texunits[TU_PRIMARY_DETAIL] = *primaryDetailRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY_DETAIL].offset += *p.materialOrigin;
                }
            }

            if(interRTU)
            {
                listSpec.texunits[TU_INTER] = *interRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_INTER].offset += *p.materialOrigin;
                }
                if(p.materialScale)
                {
                    listSpec.texunits[TU_INTER].scale  *= *p.materialScale;
                    listSpec.texunits[TU_INTER].offset *= *p.materialScale;
                }
            }

            if(interDetailRTU)
            {
                listSpec.texunits[TU_INTER_DETAIL] = *interDetailRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_INTER_DETAIL].offset += *p.materialOrigin;
                }
            }

            ClientApp::renderSystem().drawLists()
                      .find(listSpec)
                          .write(p.isWall? gl::TriangleStrip : gl::TriangleFan,
                                 BM_NORMAL,
                                 listSpec.unit(TU_PRIMARY       ).scale,
                                 listSpec.unit(TU_PRIMARY       ).offset,
                                 listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                 listSpec.unit(TU_PRIMARY_DETAIL).offset,
                                 hasDynlights, numVertices,
                                 posCoords, colorCoords, primaryCoords, interCoords,
                                 modTex, &modColor, modCoords);

            if(shinyRTU)
            {
                DrawListSpec listSpec(ShineGeom);

                listSpec.texunits[TU_PRIMARY] = *shinyRTU;

                if(shinyMaskRTU)
                {
                    listSpec.texunits[TU_INTER] = *shinyMaskRTU;
                    if(p.materialOrigin)
                    {
                        listSpec.texunits[TU_INTER].offset += *p.materialOrigin;
                    }
                    if(p.materialScale)
                    {
                        listSpec.texunits[TU_INTER].scale  *= *p.materialScale;
                        listSpec.texunits[TU_INTER].offset *= *p.materialScale;
                    }
                }

                ClientApp::renderSystem().drawLists()
                          .find(listSpec)
                              .write(p.isWall? gl::TriangleStrip : gl::TriangleFan,
                                     ms.shineBlendMode(),
                                     listSpec.unit(TU_INTER         ).scale,
                                     listSpec.unit(TU_INTER         ).offset,
                                     listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                     listSpec.unit(TU_PRIMARY_DETAIL).offset,
                                     hasDynlights, numVertices,
                                     posCoords, shinyColors, shinyTexCoords, shinyMaskRTU? primaryCoords : 0);
            }
        }
    }

    R_FreeRendTexCoords(primaryCoords);
    R_FreeRendTexCoords(interCoords);
    R_FreeRendTexCoords(modCoords);
    R_FreeRendTexCoords(shinyTexCoords);
    R_FreeRendColors(colorCoords);
    R_FreeRendColors(shinyColors);

    return (p.forceOpaque || skyMaskedMaterial ||
            !(p.alpha < 1 || !ms.isOpaque() || p.blendMode > 0));
}

static Lumobj::LightmapSemantic lightmapForSurface(Surface const &surface)
{
    if(surface.parent().type() == DMU_SIDE) return Lumobj::Side;
    // Must be a plane then.
    Plane const &plane = surface.parent().as<Plane>();
    return plane.isSectorFloor()? Lumobj::Down : Lumobj::Up;
}

static void projectDynamics(Surface const &surface, float glowStrength,
    Vector3d const &topLeft, Vector3d const &bottomRight,
    bool noLights, bool noShadows, bool sortLights,
    uint &lightListIdx, uint &shadowListIdx)
{
    BspLeaf *leaf = currentBspLeaf;

    if(glowStrength >= 1 || levelFullBright)
        return;

    // lights?
    if(!noLights)
    {
        float const blendFactor = 1;

        if(useDynLights)
        {
            Rend_ProjectLumobjs(leaf, topLeft, bottomRight, surface.tangentMatrix(),
                                blendFactor, lightmapForSurface(surface), sortLights,
                                lightListIdx);
        }

        if(useGlowOnWalls && surface.parent().type() == DMU_SIDE)
        {
            Rend_ProjectPlaneGlows(leaf, topLeft, bottomRight, surface.tangentMatrix(),
                                   blendFactor, sortLights,
                                   lightListIdx);
        }
    }

    // Shadows?
    if(!noShadows && useShadows)
    {
        // Glow inversely diminishes shadow strength.
        float const blendFactor = 1 - glowStrength;

        Rend_ProjectMobjShadows(leaf, topLeft, bottomRight, surface.tangentMatrix(),
                                blendFactor, shadowListIdx);
    }
}

/**
 * Fade the specified @a opacity value to fully transparent the closer the view
 * player is to the geometry.
 *
 * @note When the viewer is close enough we should NOT try to occlude with this
 * section in the angle clipper, otherwise HOM would occur when directly on top
 * of the wall (e.g., passing through an opaque waterfall).
 *
 * @return  @c true= fading was applied (see above note), otherwise @c false.
 */
static bool nearFadeOpacity(WallEdge const &leftEdge, WallEdge const &rightEdge,
                            float &opacity)
{
    if(vOrigin[VY] < leftEdge.bottom().z() || vOrigin[VY] > rightEdge.top().z())
        return false;

    mobj_t const *mo         = viewPlayer->shared.mo;
    Line const &line         = leftEdge.mapLineSide().line();

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

/**
 * The DOOM lighting model applies a sector light level delta when drawing
 * walls based on their 2D world angle.
 *
 * @todo WallEdge should encapsulate.
 */
static float calcLightLevelDelta(Vector3f const &normal)
{
    return (1.0f / 255) * (normal.x * 18) * rendLightWallAngle;
}

static void wallSectionLightLevelDeltas(WallEdge const &leftEdge, WallEdge const &rightEdge,
    float &leftDelta, float &rightDelta)
{
    leftDelta = calcLightLevelDelta(leftEdge.normal());

    if(leftEdge.normal() == rightEdge.normal())
    {
        rightDelta = leftDelta;
    }
    else
    {
        rightDelta = calcLightLevelDelta(rightEdge.normal());

        // Linearly interpolate to find the light level delta values for the
        // vertical edges of this wall section.
        coord_t const lineLength    = leftEdge.mapLineSide().line().length();
        coord_t const sectionOffset = leftEdge.mapLineSideOffset();
        coord_t const sectionWidth  = de::abs(Vector2d(rightEdge.origin() - leftEdge.origin()).length());

        float deltaDiff = rightDelta - leftDelta;
        rightDelta = leftDelta + ((sectionOffset + sectionWidth) / lineLength) * deltaDiff;
        leftDelta += (sectionOffset / lineLength) * deltaDiff;
    }
}

static void writeWallSection(HEdge &hedge, int section,
    bool *retWroteOpaque = 0, coord_t *retBottomZ = 0, coord_t *retTopZ = 0)
{
    BspLeaf *leaf = currentBspLeaf;

    LineSideSegment &segment = hedge.mapElementAs<LineSideSegment>();
    DENG_ASSERT(segment.isFrontFacing() && segment.lineSide().hasSections());

    if(retWroteOpaque) *retWroteOpaque = false;
    if(retBottomZ)     *retBottomZ     = 0;
    if(retTopZ)        *retTopZ        = 0;

    LineSide &side    = segment.lineSide();
    Surface &surface  = side.surface(section);

    // Skip nearly transparent surfaces.
    float opacity = surface.opacity();
    if(opacity < .001f)
        return;

    // Determine which Material to use.
    Material *material = Rend_ChooseMapSurfaceMaterial(surface);

    // A drawable material is required.
    if(!material || !material->isDrawable())
        return;

    // Generate edge geometries.
    WallSpec const wallSpec = WallSpec::fromMapSide(side, section);

    WallEdge leftEdge(wallSpec, hedge, Line::From);
    WallEdge rightEdge(wallSpec, hedge, Line::To);

    // Do the edge geometries describe a valid polygon?
    if(!leftEdge.isValid() || !rightEdge.isValid() ||
       de::fequal(leftEdge.bottom().z(), rightEdge.top().z()))
        return;

    // Apply a fade out when the viewer is near to this geometry?
    bool didNearFade = false;
    if(wallSpec.flags.testFlag(WallSpec::NearFade))
    {
        didNearFade = nearFadeOpacity(leftEdge, rightEdge, opacity);
    }

    bool const skyMasked       = material->isSkyMasked() && !devRendSkyMode;
    bool const twoSidedMiddle  = (wallSpec.section == LineSide::Middle && !side.considerOneSided());

    MaterialSnapshot const &ms = material->prepare(Rend_MapSurfaceMaterialSpec());

    Vector2f const materialScale((surface.flags() & DDSUF_MATERIAL_FLIPH)? -1 : 1,
                                 (surface.flags() & DDSUF_MATERIAL_FLIPV)? -1 : 1);

    rendworldpoly_params_t parm; zap(parm);

    Vector3f materialOrigin = leftEdge.materialOrigin();
    Vector3d topLeft        = leftEdge.top().origin();
    Vector3d bottomRight    = rightEdge.bottom().origin();

    parm.skyMasked           = skyMasked;
    parm.bsuf                = &segment;
    parm.geomGroup           = wallSpec.section;
    parm.topLeft             = &topLeft;
    parm.bottomRight         = &bottomRight;
    parm.forceOpaque         = wallSpec.flags.testFlag(WallSpec::ForceOpaque);
    parm.alpha               = parm.forceOpaque? 1 : opacity;

    // Calculate the light level deltas for this wall section?
    if(!wallSpec.flags.testFlag(WallSpec::NoLightDeltas))
    {
        wallSectionLightLevelDeltas(leftEdge, rightEdge,
                                    parm.surfaceLightLevelDL, parm.surfaceLightLevelDR);
    }

    parm.blendMode           = BM_NORMAL;
    parm.materialOrigin      = &materialOrigin;
    parm.materialScale       = &materialScale;

    parm.isWall              = true;
    parm.wall.sectionWidth   = de::abs(Vector2d(rightEdge.origin() - leftEdge.origin()).length());
    parm.wall.leftEdge       = &leftEdge;
    parm.wall.rightEdge      = &rightEdge;

    if(!parm.skyMasked)
    {
        if(glowFactor > .0001f)
        {
            if(material == surface.materialPtr())
            {
                parm.glowing = ms.glowStrength();
            }
            else
            {
                Material *actualMaterial =
                    surface.hasMaterial()? surface.materialPtr()
                                         : &ClientApp::resourceSystem().material(de::Uri("System", Path("missing")));

                MaterialSnapshot const &ms = actualMaterial->prepare(Rend_MapSurfaceMaterialSpec());
                parm.glowing = ms.glowStrength();
            }

            parm.glowing *= glowFactor; // Global scale factor.
        }

        projectDynamics(surface, parm.glowing, *parm.topLeft, *parm.bottomRight,
                        wallSpec.flags.testFlag(WallSpec::NoDynLights),
                        wallSpec.flags.testFlag(WallSpec::NoDynShadows),
                        wallSpec.flags.testFlag(WallSpec::SortDynLights),
                        parm.lightListIdx, parm.shadowListIdx);

        if(twoSidedMiddle)
        {
            parm.blendMode = surface.blendMode();
            if(parm.blendMode == BM_NORMAL && noSpriteTrans)
                parm.blendMode = BM_ZEROALPHA; // "no translucency" mode
        }

        side.chooseSurfaceTintColors(wallSpec.section, &parm.surfaceColor, &parm.wall.surfaceColor2);
    }

    /*
     * Geometry write/drawing begins.
     */

    if(twoSidedMiddle && side.sectorPtr() != leaf->sectorPtr())
    {
        // Temporarily modify the draw state.
        currentSectorLightColor = Rend_SectorLightColor(side.sector());
        currentSectorLightLevel = side.sector().lightLevel();
    }

    // Allocate position coordinates.
    Vector3f *posCoords;
    if(leftEdge.divisionCount() || rightEdge.divisionCount())
    {
        // Two fans plus edge divisions.
        posCoords = R_AllocRendVertices(3 + leftEdge.divisionCount() +
                                        3 + rightEdge.divisionCount());
    }
    else
    {
        // One quad.
        posCoords = R_AllocRendVertices(4);
    }

    posCoords[0] =  leftEdge.bottom().origin();
    posCoords[1] =     leftEdge.top().origin();
    posCoords[2] = rightEdge.bottom().origin();
    posCoords[3] =    rightEdge.top().origin();

    // Draw this section.
    bool wroteOpaque = renderWorldPoly(posCoords, 4, parm, ms);
    if(wroteOpaque)
    {
        // Render FakeRadio for this section?
        if(!wallSpec.flags.testFlag(WallSpec::NoFakeRadio) && !skyMasked &&
           !(parm.glowing > 0) && currentSectorLightLevel > 0)
        {
            Rend_RadioUpdateForLineSide(side);

            // Determine the shadow properties.
            /// @todo Make cvars out of constants.
            float shadowSize = 2 * (8 + 16 - currentSectorLightLevel * 16);
            float shadowDark = Rend_RadioCalcShadowDarkness(currentSectorLightLevel);

            Rend_RadioWallSection(leftEdge, rightEdge, shadowDark, shadowSize);
        }
    }

    if(twoSidedMiddle && side.sectorPtr() != leaf->sectorPtr())
    {
        // Undo temporary draw state changes.
        currentSectorLightColor = Rend_SectorLightColor(leaf->cluster());
        currentSectorLightLevel = leaf->sector().lightLevel();
    }

    R_FreeRendVertices(posCoords);

    if(retWroteOpaque) *retWroteOpaque = wroteOpaque && !didNearFade;
    if(retBottomZ)     *retBottomZ     = leftEdge.bottom().z();
    if(retTopZ)        *retTopZ        = rightEdge.top().z();
}

/**
 * Prepare a trifan geometry according to the edges of the current BSP leaf.
 * If a fan base HEdge has been chosen it will be used as the center of the
 * trifan, else the mid point of this leaf will be used instead.
 *
 * @param direction  Vertex winding direction.
 * @param height     Z map space height coordinate to be set for each vertex.
 * @param verts      Built position coordinates are written here. It is the
 *                   responsibility of the caller to release this storage with
 *                   @ref R_FreeRendVertices() when done.
 *
 * @return  Number of built vertices.
 */
static uint buildLeafPlaneGeometry(ClockDirection direction, coord_t height,
    Vector3f **verts)
{
    DENG2_ASSERT(verts != 0);

    BspLeaf const *leaf = currentBspLeaf;
    Face const &face    = leaf->poly();

    HEdge *fanBase  = leaf->fanBase();
    uint totalVerts = face.hedgeCount() + (!fanBase? 2 : 0);

    *verts = R_AllocRendVertices(totalVerts);

    uint n = 0;
    if(!fanBase)
    {
        (*verts)[n] = Vector3f(face.center(), height);
        n++;
    }

    // Add the vertices for each hedge.
    HEdge *baseNode = fanBase? fanBase : face.hedge();
    HEdge *node = baseNode;
    do
    {
        (*verts)[n] = Vector3f(node->origin(), height);
        n++;
    } while((node = &node->neighbor(direction)) != baseNode);

    // The last vertex is always equal to the first.
    if(!fanBase)
    {
        (*verts)[n] = Vector3f(face.hedge()->origin(), height);
    }

    return totalVerts;
}

static void writeLeafPlane(Plane &plane)
{
    BspLeaf *leaf          = currentBspLeaf;
    Face const &poly       = leaf->poly();
    Surface const &surface = plane.surface();

    // Skip nearly transparent surfaces.
    float const opacity = surface.opacity();
    if(opacity < .001f) return;

    // Determine which Material to use.
    Material *material = Rend_ChooseMapSurfaceMaterial(surface);

    // A drawable material is required.
    if(!material) return;
    if(!material->isDrawable()) return;

    // Skip planes with a sky-masked material?
    if(!devRendSkyMode)
    {
        if(surface.hasSkyMaskedMaterial() && plane.indexInSector() <= Sector::Ceiling)
        {
            return; // Not handled here (drawn with the mask geometry).
        }
    }

    MaterialSnapshot const &ms = material->prepare(Rend_MapSurfaceMaterialSpec());

    Vector2f materialOrigin = leaf->worldGridOffset() // Align to the worldwide grid.
                            + surface.materialOriginSmoothed();

    // Add the Y offset to orient the Y flipped material.
    /// @todo fixme: What is this meant to do? -ds
    if(plane.isSectorCeiling())
    {
        materialOrigin.y -= poly.aaBox().maxY - poly.aaBox().minY;
    }
    materialOrigin.y = -materialOrigin.y;

    Vector2f const materialScale((surface.flags() & DDSUF_MATERIAL_FLIPH)? -1 : 1,
                                 (surface.flags() & DDSUF_MATERIAL_FLIPV)? -1 : 1);

    // Set the texture origin, Y is flipped for the ceiling.
    Vector3d topLeft(poly.aaBox().minX,
                     poly.aaBox().arvec2[plane.isSectorFloor()? 1 : 0][VY],
                     plane.heightSmoothed());
    Vector3d bottomRight(poly.aaBox().maxX,
                         poly.aaBox().arvec2[plane.isSectorFloor()? 0 : 1][VY],
                         plane.heightSmoothed());

    rendworldpoly_params_t parm; zap(parm);

    parm.bsuf                = leaf;
    parm.geomGroup           = plane.indexInSector();
    parm.topLeft             = &topLeft;
    parm.bottomRight         = &bottomRight;
    parm.materialOrigin      = &materialOrigin;
    parm.materialScale       = &materialScale;
    parm.surfaceLightLevelDL = parm.surfaceLightLevelDR = 0;
    parm.surfaceColor        = &surface.tintColor();

    if(material->isSkyMasked())
    {
        // In devRendSkyMode mode we render all polys destined for the
        // skymask as regular world polys (with a few obvious properties).
        if(devRendSkyMode)
        {
            parm.blendMode   = BM_NORMAL;
            parm.forceOpaque = true;
        }
        else
        {
            // We'll mask this.
            parm.skyMasked = true;
        }
    }
    else if(plane.indexInSector() <= Sector::Ceiling)
    {
        parm.blendMode   = BM_NORMAL;
        parm.forceOpaque = true;
    }
    else
    {
        parm.blendMode = surface.blendMode();
        if(parm.blendMode == BM_NORMAL && noSpriteTrans)
        {
            parm.blendMode = BM_ZEROALPHA; // "no translucency" mode
        }

        parm.alpha = surface.opacity();
    }

    if(!parm.skyMasked)
    {
        if(glowFactor > .0001f)
        {
            if(material == surface.materialPtr())
            {
                parm.glowing = ms.glowStrength();
            }
            else
            {
                Material *actualMaterial =
                    surface.hasMaterial()? surface.materialPtr()
                                         : &ClientApp::resourceSystem().material(de::Uri("System", Path("missing")));

                MaterialSnapshot const &ms = actualMaterial->prepare(Rend_MapSurfaceMaterialSpec());
                parm.glowing = ms.glowStrength();
            }

            parm.glowing *= glowFactor; // Global scale factor.
        }

        projectDynamics(surface, parm.glowing, *parm.topLeft, *parm.bottomRight,
                        false /*do light*/, false /*do shadow*/, false /*don't sort*/,
                        parm.lightListIdx, parm.shadowListIdx);
    }

    /*
     * Geometry write/drawing begins.
     */

    if(&plane.sector() != leaf->sectorPtr())
    {
        // Temporarily modify the draw state.
        currentSectorLightColor = Rend_SectorLightColor(plane.sector());
        currentSectorLightLevel = plane.sector().lightLevel();
    }

    // Allocate position coordinates.
    Vector3f *posCoords;
    uint vertCount = buildLeafPlaneGeometry((plane.isSectorCeiling())? Anticlockwise : Clockwise,
                                            plane.heightSmoothed(), &posCoords);

    // Draw this section.
    renderWorldPoly(posCoords, vertCount, parm, ms);

    if(&plane.sector() != leaf->sectorPtr())
    {
        // Undo temporary draw state changes.
        currentSectorLightColor = Rend_SectorLightColor(leaf->cluster());
        currentSectorLightLevel = leaf->sector().lightLevel();
    }

    R_FreeRendVertices(posCoords);
}

static void writeSkyMaskStrip(int vertCount, Vector3f const *posCoords,
    Vector2f const *texCoords, Material *material)
{
    DENG2_ASSERT(posCoords != 0);

    if(!devRendSkyMode)
    {
        ClientApp::renderSystem().drawLists()
                  .find(DrawListSpec(SkyMaskGeom))
                      .write(gl::TriangleStrip,
                             BM_NORMAL, Vector2f(1, 1), Vector2f(0, 0),
                             Vector2f(1, 1), Vector2f(0, 0),
                             0, vertCount,
                             posCoords);
    }
    else
    {
        DENG2_ASSERT(texCoords != 0);

        DrawListSpec listSpec;
        listSpec.group = UnlitGeom;
        if(renderTextures != 2)
        {
            DENG2_ASSERT(material != 0);

            // Map RTU configuration from the sky surface material.
            MaterialSnapshot const &ms = material->prepare(Rend_MapSurfaceMaterialSpec());
            listSpec.texunits[TU_PRIMARY]        = ms.unit(RTU_PRIMARY);
            listSpec.texunits[TU_PRIMARY_DETAIL] = ms.unit(RTU_PRIMARY_DETAIL);
            listSpec.texunits[TU_INTER]          = ms.unit(RTU_INTER);
            listSpec.texunits[TU_INTER_DETAIL]   = ms.unit(RTU_INTER_DETAIL);
        }

        ClientApp::renderSystem().drawLists()
                  .find(listSpec)
                      .write(gl::TriangleStrip, BM_NORMAL,
                             listSpec.unit(TU_PRIMARY       ).scale,
                             listSpec.unit(TU_PRIMARY       ).offset,
                             listSpec.unit(TU_PRIMARY_DETAIL).scale,
                             listSpec.unit(TU_PRIMARY_DETAIL).offset,
                             0, vertCount,
                             posCoords, 0, texCoords);
    }
}

static void writeLeafSkyMaskStrips(SkyFixEdge::FixType fixType)
{
    BspLeaf *leaf = currentBspLeaf;

    // Determine strip generation behavior.
    ClockDirection const direction   = Clockwise;
    bool const buildTexCoords        = CPP_BOOL(devRendSkyMode);
    bool const splitOnMaterialChange = (devRendSkyMode && renderTextures != 2);

    // Configure the strip builder wrt vertex attributes.
    TriangleStripBuilder stripBuilder(buildTexCoords);

    // Configure the strip build state (we'll most likely need to break
    // edge loop into multiple strips).
    HEdge *startNode          = 0;
    coord_t startZBottom      = 0;
    coord_t startZTop         = 0;
    Material *startMaterial   = 0;
    float startMaterialOffset = 0;

    // Determine the relative sky plane (for monitoring material changes).
    int relPlane = fixType == SkyFixEdge::Upper? Sector::Ceiling : Sector::Floor;

    // Begin generating geometry.
    HEdge *base  = leaf->poly().hedge();
    HEdge *hedge = base;
    forever
    {
        // Are we monitoring material changes?
        Material *skyMaterial = 0;
        if(splitOnMaterialChange)
        {
            skyMaterial = hedge->face().mapElementAs<BspLeaf>()
                              .cluster().visPlane(relPlane).surface().materialPtr();
        }

        // Add a first (left) edge to the current strip?
        if(startNode == 0 && hedge->hasMapElement())
        {
            startMaterialOffset = hedge->mapElementAs<LineSideSegment>().lineSideOffset();

            // Prepare the edge geometry
            SkyFixEdge skyEdge(*hedge, fixType, (direction == Anticlockwise)? Line::To : Line::From,
                               startMaterialOffset);

            if(skyEdge.isValid() && skyEdge.bottom().z() < skyEdge.top().z())
            {
                // A new strip begins.
                stripBuilder.begin(direction);
                stripBuilder << skyEdge;

                // Update the strip build state.
                startNode     = hedge;
                startZBottom  = skyEdge.bottom().z();
                startZTop     = skyEdge.top().z();
                startMaterial = skyMaterial;
            }
        }

        bool beginNewStrip = false;

        // Add the i'th (right) edge to the current strip?
        if(startNode != 0)
        {
            // Stop if we've reached a "null" edge.
            bool endStrip = false;
            if(hedge->hasMapElement())
            {
                startMaterialOffset += hedge->mapElementAs<LineSideSegment>().length()
                                     * (direction == Anticlockwise? -1 : 1);

                // Prepare the edge geometry
                SkyFixEdge skyEdge(*hedge, fixType, (direction == Anticlockwise)? Line::From : Line::To,
                                   startMaterialOffset);

                if(!(skyEdge.isValid() && skyEdge.bottom().z() < skyEdge.top().z()))
                {
                    endStrip = true;
                }
                // Must we split the strip here?
                else if(hedge != startNode &&
                        (!de::fequal(skyEdge.bottom().z(), startZBottom) ||
                         !de::fequal(skyEdge.top().z(), startZTop) ||
                         (splitOnMaterialChange && skyMaterial != startMaterial)))
                {
                    endStrip = true;
                    beginNewStrip = true; // We'll continue from here.
                }
                else
                {
                    // Extend the strip geometry.
                    stripBuilder << skyEdge;
                }
            }
            else
            {
                endStrip = true;
            }

            if(endStrip || &hedge->neighbor(direction) == base)
            {
                // End the current strip.
                startNode = 0;

                // Take ownership of the built geometry.
                PositionBuffer *positions = 0;
                TexCoordBuffer *texcoords = 0;
                int numVerts = stripBuilder.take(&positions, &texcoords);

                // Write the strip geometry to the render lists.
                writeSkyMaskStrip(numVerts, positions->constData(),
                                  texcoords? texcoords->constData() : 0, startMaterial);

                delete positions;
                delete texcoords;
            }
        }

        // Start a new strip from the current node?
        if(beginNewStrip) continue;

        // On to the next node.
        hedge = &hedge->neighbor(direction);

        // Are we done?
        if(hedge == base) break;
    }
}

/**
 * @defgroup skyCapFlags  Sky Cap Flags
 * @ingroup flags
 */
///@{
#define SKYCAP_LOWER                0x1
#define SKYCAP_UPPER                0x2
///@}

static coord_t skyPlaneZ(int skyCap)
{
    BspLeaf *leaf = currentBspLeaf;

    int const relPlane = (skyCap & SKYCAP_UPPER)? Sector::Ceiling : Sector::Floor;
    if(!P_IsInVoid(viewPlayer))
    {
        return leaf->map().skyFix(relPlane == Sector::Ceiling);
    }

    return leaf->cluster().visPlane(relPlane).heightSmoothed();
}

/// @param skyCap  @ref skyCapFlags.
static void writeLeafSkyMaskCap(int skyCap)
{
    // Caps are unnecessary in sky debug mode (will be drawn as regular planes).
    if(devRendSkyMode) return;
    if(!skyCap) return;

    Vector3f *posCoords;
    uint vertCount = buildLeafPlaneGeometry((skyCap & SKYCAP_UPPER)? Anticlockwise : Clockwise,
                                            skyPlaneZ(skyCap), &posCoords);

    ClientApp::renderSystem().drawLists()
              .find(DrawListSpec(SkyMaskGeom))
                  .write(gl::TriangleFan,
                         BM_NORMAL, Vector2f(1, 1), Vector2f(0, 0),
                         Vector2f(1, 1), Vector2f(0, 0), 0,
                         vertCount, posCoords);

    R_FreeRendVertices(posCoords);
}

/// @param skyCap  @ref skyCapFlags
static void writeLeafSkyMask(int skyCap = SKYCAP_LOWER|SKYCAP_UPPER)
{
    BspLeaf *leaf = currentBspLeaf;
    SectorCluster &cluster = leaf->cluster();

    // Any work to do?
    // Sky caps are only necessary in sectors with sky-masked planes.
    if((skyCap & SKYCAP_LOWER) &&
       !cluster.visFloor().surface().hasSkyMaskedMaterial())
    {
        skyCap &= ~SKYCAP_LOWER;
    }
    if((skyCap & SKYCAP_UPPER) &&
       !cluster.visCeiling().surface().hasSkyMaskedMaterial())
    {
        skyCap &= ~SKYCAP_UPPER;
    }

    if(!skyCap) return;

    // Lower?
    if(skyCap & SKYCAP_LOWER)
    {
        writeLeafSkyMaskStrips(SkyFixEdge::Lower);
        writeLeafSkyMaskCap(SKYCAP_LOWER);
    }

    // Upper?
    if(skyCap & SKYCAP_UPPER)
    {
        writeLeafSkyMaskStrips(SkyFixEdge::Upper);
        writeLeafSkyMaskCap(SKYCAP_UPPER);
    }
}

static bool coveredOpenRange(HEdge &hedge, coord_t middleBottomZ, coord_t middleTopZ,
    bool wroteOpaqueMiddle)
{
    LineSide const &front = hedge.mapElementAs<LineSideSegment>().lineSide();

    if(front.considerOneSided())
    {
        return wroteOpaqueMiddle;
    }

    /// @todo fixme: This additional test should not be necessary. For the obove
    /// test to fail and this to pass means that the geometry produced by the BSP
    /// builder is not correct (see: eternall.wad MAP10; note mapping errors).
    if(!hedge.twin().hasFace())
    {
        return wroteOpaqueMiddle;
    }

    SectorCluster const &cluster     = hedge.face().mapElementAs<BspLeaf>().cluster();
    SectorCluster const &backCluster = hedge.twin().face().mapElementAs<BspLeaf>().cluster();

    coord_t const ffloor   = cluster.visFloor().heightSmoothed();
    coord_t const fceil    = cluster.visCeiling().heightSmoothed();
    coord_t const bfloor   = backCluster.visFloor().heightSmoothed();
    coord_t const bceil    = backCluster.visCeiling().heightSmoothed();

    bool middleCoversOpening = false;
    if(wroteOpaqueMiddle)
    {
        coord_t xbottom = de::max(bfloor, ffloor);
        coord_t xtop    = de::min(bceil,  fceil);

        Surface const &middle = front.middle();
        xbottom += middle.materialOriginSmoothed().y;
        xtop    += middle.materialOriginSmoothed().y;

        middleCoversOpening = (middleTopZ >= xtop &&
                               middleBottomZ <= xbottom);
    }

    if(wroteOpaqueMiddle && middleCoversOpening)
        return true;

    if(   (bceil <= ffloor &&
               (front.top().hasMaterial()    || front.middle().hasMaterial()))
       || (bfloor >= fceil &&
               (front.bottom().hasMaterial() || front.middle().hasMaterial())))
    {
        Surface const &ffloorSurface = cluster.visFloor().surface();
        Surface const &fceilSurface  = cluster.visCeiling().surface();
        Surface const &bfloorSurface = backCluster.visFloor().surface();
        Surface const &bceilSurface  = backCluster.visCeiling().surface();

        // A closed gap?
        if(de::fequal(fceil, bfloor))
        {
            return (bceil <= bfloor) ||
                   !(fceilSurface.hasSkyMaskedMaterial() &&
                     bceilSurface.hasSkyMaskedMaterial());
        }

        if(de::fequal(ffloor, bceil))
        {
            return (bfloor >= bceil) ||
                   !(ffloorSurface.hasSkyMaskedMaterial() &&
                     bfloorSurface.hasSkyMaskedMaterial());
        }

        return true;
    }

    /// @todo Is this still necessary?
    if(bceil <= bfloor ||
       (!(bceil - bfloor > 0) && bfloor > ffloor && bceil < fceil &&
        front.top().hasMaterial() && front.bottom().hasMaterial()))
    {
        // A zero height back segment
        return true;
    }

    return false;
}

static void writeAllWallSections(HEdge *hedge)
{
    // Edges without a map line segment implicitly have no surfaces.
    if(!hedge || !hedge->hasMapElement())
        return;

    // We are only interested in front facing segments with sections.
    LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
    if(!seg.isFrontFacing() || !seg.lineSide().hasSections())
        return;

    // Done here because of the logic of doom.exe wrt the automap.
    reportWallSectionDrawn(seg.line());

    bool wroteOpaqueMiddle = false;
    coord_t middleBottomZ = 0, middleTopZ = 0;

    writeWallSection(*hedge, LineSide::Bottom);
    writeWallSection(*hedge, LineSide::Top);
    writeWallSection(*hedge, LineSide::Middle,
                     &wroteOpaqueMiddle, &middleBottomZ, &middleTopZ);

    // We can occlude the angle range defined by the X|Y origins of the
    // line segment if the open range has been covered (when the viewer
    // is not in the void).
    if(!P_IsInVoid(viewPlayer) &&
       coveredOpenRange(*hedge, middleBottomZ, middleTopZ, wroteOpaqueMiddle))
    {
        C_AddRangeFromViewRelPoints(hedge->origin(), hedge->twin().origin());
    }
}

static void writeLeafWallSections()
{
    BspLeaf *leaf = currentBspLeaf;

    HEdge *base = leaf->poly().hedge();
    HEdge *hedge = base;
    do
    {
        writeAllWallSections(hedge);
    } while((hedge = &hedge->next()) != base);

    foreach(Mesh *mesh, leaf->extraMeshes())
    foreach(HEdge *hedge, mesh->hedges())
    {
        writeAllWallSections(hedge);
    }

    foreach(Polyobj *po, leaf->polyobjs())
    foreach(HEdge *hedge, po->mesh().hedges())
    {
        writeAllWallSections(hedge);
    }
}

static void writeLeafPlanes()
{
    BspLeaf *leaf = currentBspLeaf;
    SectorCluster &cluster = leaf->cluster();

    for(int i = 0; i < cluster.visPlaneCount(); ++i)
    {
        Plane &plane = cluster.visPlane(i);

        // Skip planes facing away from the viewer.
        Vector3d const pointOnPlane(cluster.center(), plane.heightSmoothed());
        if((eyeOrigin - pointOnPlane).dot(plane.surface().normal()) < 0)
            continue;

        writeLeafPlane(plane);
    }
}

static void markFrontFacingWalls(HEdge *hedge)
{
    if(!hedge || !hedge->hasMapElement()) return;
    LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
    // Which way is the line segment facing?
    seg.setFrontFacing(viewFacingDot(hedge->origin(), hedge->twin().origin()) >= 0);
}

static void markLeafFrontFacingWalls()
{
    BspLeaf *leaf = currentBspLeaf;

    HEdge *base = leaf->poly().hedge();
    HEdge *hedge = base;
    do
    {
        markFrontFacingWalls(hedge);
    } while((hedge = &hedge->next()) != base);

    foreach(Mesh *mesh, leaf->extraMeshes())
    foreach(HEdge *hedge, mesh->hedges())
    {
        markFrontFacingWalls(hedge);
    }

    foreach(Polyobj *po, leaf->polyobjs())
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
 * Add angle clipper occlusion ranges for the edges of the current leaf.
 */
static void occludeLeaf(bool frontFacing)
{
    BspLeaf *leaf = currentBspLeaf;
    SectorCluster &cluster = leaf->cluster();

    if(devNoCulling) return;
    if(P_IsInVoid(viewPlayer)) return;

    HEdge *base = leaf->poly().hedge();
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
        if(!hedge->hasTwin() || !hedge->twin().hasFace())
            continue;

        BspLeaf &backLeaf = hedge->twin().face().mapElementAs<BspLeaf>();
        if(!backLeaf.hasCluster())
            continue;
        SectorCluster &backCluster = backLeaf.cluster();

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
        if(((openBottom > cluster.visFloor().heightSmoothed() && vOrigin[VY] <= openBottom)
            || (openBottom >  backCluster.visFloor().heightSmoothed() && vOrigin[VY] >= openBottom))
           && canOccludeEdgeBetweenPlanes(cluster.visFloor(), backCluster.visFloor()))
        {
            C_AddViewRelOcclusion(from.origin(), to.origin(), openBottom, false);
        }

        // Does the ceiling create an occlusion?
        if(((openTop < cluster.visCeiling().heightSmoothed() && vOrigin[VY] >= openTop)
            || (openTop <  backCluster.visCeiling().heightSmoothed() && vOrigin[VY] <= openTop))
           && canOccludeEdgeBetweenPlanes(cluster.visCeiling(), backCluster.visCeiling()))
        {
            C_AddViewRelOcclusion(from.origin(), to.origin(), openTop, true);
        }
    } while((hedge = &hedge->next()) != base);
}

static void clipLeafLumobjs()
{
    BspLeaf *leaf = currentBspLeaf;

    foreach(Lumobj *lum, leaf->lumobjs())
    {
        R_ViewerClipLumobj(lum);
    }
}

/**
 * In the situation where a BSP leaf contains both lumobjs and a polyobj, lumobjs
 * must be clipped more carefully. Here we check if the line of sight intersects any
 * of the polyobj half-edges facing the viewer.
 */
static void clipLeafLumobjsBySight()
{
    BspLeaf *leaf = currentBspLeaf;

    foreach(Lumobj *lum, leaf->lumobjs())
    {
        R_ViewerClipLumobjBySight(lum, leaf);
    }
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

static void clipLeafFrontFacingWalls()
{
    BspLeaf *leaf = currentBspLeaf;

    HEdge *base = leaf->poly().hedge();
    HEdge *hedge = base;
    do
    {
        clipFrontFacingWalls(hedge);
    } while((hedge = &hedge->next()) != base);

    foreach(Mesh *mesh, leaf->extraMeshes())
    foreach(HEdge *hedge, mesh->hedges())
    {
        clipFrontFacingWalls(hedge);
    }

    foreach(Polyobj *po, leaf->polyobjs())
    foreach(HEdge *hedge, po->mesh().hedges())
    {
        clipFrontFacingWalls(hedge);
    }
}

static int projectSpriteWorker(mobj_t &mo, void * /*context*/)
{
    BspLeaf const *leaf = currentBspLeaf;
    SectorCluster &cluster = leaf->cluster();

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
                        coord_t visibleTop = mo.origin[VZ] + material->height();
                        if(visibleTop > cluster.sector().map().skyFixCeiling())
                        {
                            // Raise skyfix ceiling.
                            cluster.sector().map().setSkyFixCeiling(visibleTop + 16/*leeway*/);
                        }
                    }
                }
            }
        }
    }

    return false; // Continue iteration.
}

static void projectLeafSprites()
{
    BspLeaf *leaf = currentBspLeaf;

    // Do not use validCount because other parts of the renderer may change it.
    if(leaf->lastSpriteProjectFrame() == R_FrameCount())
        return; // Already added.

    R_BspLeafMobjContactIterator(*leaf, projectSpriteWorker);

    leaf->setLastSpriteProjectFrame(R_FrameCount());
}

/**
 * @pre Assumes the leaf is at least partially visible.
 */
static void drawCurrentLeaf()
{
    BspLeaf *leaf = currentBspLeaf;

    // Mark the leaf as visible for this frame.
    R_ViewerBspLeafMarkVisible(*leaf);

    markLeafFrontFacingWalls();

    // Perform contact spreading for this map region.
    leaf->map().spreadAllContacts(leaf->poly().aaBox());

    Rend_RadioBspLeafEdges(*leaf);

    /*
     * Before clip testing lumobjs (for halos), range-occlude the back facing edges.
     * After testing, range-occlude the front facing edges. Done before drawing wall
     * sections so that opening occlusions cut out unnecessary oranges.
     */

    occludeLeaf(false /* back facing */);
    clipLeafLumobjs();
    occludeLeaf(true /* front facing */);

    clipLeafFrontFacingWalls();

    if(leaf->polyobjCount())
    {
        // Polyobjs don't obstruct - clip lights with another algorithm.
        clipLeafLumobjsBySight();
    }

    // Mark particle generators in the sector visible.
    Rend_ParticleMarkInSectorVisible(leaf->sectorPtr());

    /*
     * Sprites for this BSP leaf have to be drawn.
     *
     * Must be done BEFORE the segments of this BspLeaf are added to the clipper.
     * Otherwise the sprites would get clipped by them, and that wouldn't be right.
     *
     * Must be done AFTER the lumobjs have been clipped as this affects the projection
     * of halos.
     */
    projectLeafSprites();

    writeLeafSkyMask();
    writeLeafWallSections();
    writeLeafPlanes();
}

/**
 * Change the current BspLeaf (updating any relevant draw state properties
 * accordingly).
 *
 * @param bspLeaf  The new BSP leaf to make current.
 */
static void makeCurrent(BspLeaf &bspLeaf)
{
    bool sectorChanged = (!currentBspLeaf || currentBspLeaf->sectorPtr() != bspLeaf.sectorPtr());

    currentBspLeaf = &bspLeaf;

    // Update draw state.
    if(sectorChanged)
    {
        currentSectorLightColor = Rend_SectorLightColor(bspLeaf.cluster());
        currentSectorLightLevel = bspLeaf.sector().lightLevel();
    }
}

static void traverseBspAndDrawLeafs(MapElement *bspElement)
{
    DENG_ASSERT(bspElement != 0);

    while(bspElement->type() != DMU_BSPLEAF)
    {
        // Descend deeper into the nodes.
        BspNode const &bspNode = bspElement->as<BspNode>();

        // Decide which side the view point is on.
        int eyeSide = bspNode.partition().pointOnSide(eyeOrigin) < 0;

        // Recursively divide front space.
        traverseBspAndDrawLeafs(bspNode.childPtr(eyeSide));

        // If the clipper is full we're pretty much done. This means no geometry
        // will be visible in the distance because every direction has already
        // been fully covered by geometry.
        if(!firstBspLeaf && C_IsFull())
            return;

        // ...and back space.
        bspElement = bspNode.childPtr(eyeSide ^ 1);
    }

    // We've arrived at a leaf.
    BspLeaf &bspLeaf = bspElement->as<BspLeaf>();

    // Skip null leafs (those with zero volume). Neighbors handle adding the
    // angle clipper ranges.
    if(!bspLeaf.hasCluster() || !bspLeaf.cluster().hasWorldVolume())
        return;

    // Is this leaf visible?
    if(!firstBspLeaf && !C_IsPolyVisible(bspLeaf.poly()))
        return;

    // This is now the current leaf.
    makeCurrent(bspLeaf);

    drawCurrentLeaf();

    // This is no longer the first leaf.
    firstBspLeaf = false;
}

/**
 * Project all the non-clipped decorations. They become regular vissprites.
 */
static void generateDecorationFlares(Map &map)
{
    Vector3d const viewPos(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

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
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_BLEND + 1;
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
            texUnitMap[0] = Store::TCA_LIGHT + 1;
            texUnitMap[1] = Store::TCA_MAIN + 1;
            GL_ModulateTexture(4); // Light * texture.
        }
        else
        {
            texUnitMap[0] = Store::TCA_MAIN + 1;
            texUnitMap[1] = Store::TCA_LIGHT + 1;
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
        texUnitMap[0] = Store::TCA_LIGHT + 1;
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
        texUnitMap[0] = Store::TCA_LIGHT + 1;
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
        texUnitMap[0] = Store::TCA_MAIN + 1;
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
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_BLEND + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;

    case DM_UNBLENDED_TEXTURE_AND_DETAIL:
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_MAIN + 1;
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
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_MAIN + 1;
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
        texUnitMap[0] = Store::TCA_MAIN + 1;
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
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_BLEND + 1;
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
        texUnitMap[0] = Store::TCA_MAIN + 1;
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
        texUnitMap[0] = Store::TCA_MAIN + 1;
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
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_BLEND + 1; // the mask
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

    Sky_Render();

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
static void drawAllLists()
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
    Rend_RenderParticles();

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
        // Prepare for rendering.
        ClientApp::renderSystem().resetDrawLists(); // Clear the lists for new geometry.
        C_ClearRanges(); // Clear the clipper.

        // Recycle the vlight lists. Currently done here as the lists are
        // not shared by all viewports.
        VL_InitForNewFrame();

        R_BeginFrame();

        // Make vissprites of all the visible decorations.
        generateDecorationFlares(map);

        // Clear particle generator visibilty info.
        Rend_ParticleInitForNewFrame();

        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
        eyeOrigin = viewData->current.origin;

        // Add the backside clipping range (if vpitch allows).
        if(vpitch <= 90 - yfov / 2 && vpitch >= -90 + yfov / 2)
        {
            float a = de::abs(vpitch) / (90 - yfov / 2);
            binangle_t startAngle = binangle_t(BANG_45 * Rend_FieldOfView() / 90) * (1 + a);
            binangle_t angLen = BANG_180 - startAngle;

            binangle_t viewside = (viewData->current.angle >> (32 - BAMS_BITS)) + startAngle;
            C_SafeAddRange(viewside, viewside + angLen);
            C_SafeAddRange(viewside + angLen, viewside + 2 * angLen);
        }

        // The viewside line for the depth cue.
        viewsidex = -viewData->viewSin;
        viewsidey = viewData->viewCos;

        // We don't want BSP clip checking for the first BSP leaf.
        firstBspLeaf = true;

        // No current BSP leaf as of yet.
        currentBspLeaf = 0;

        // Draw the world!
        traverseBspAndDrawLeafs(&map.bspRoot());
    }
    drawAllLists();

    // Draw various debugging displays:
    drawAllSurfaceTangentVectors(map);
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
    ddouble distToEye = (Vector3d(vOrigin[VX], vOrigin[VZ], vOrigin[VY]) - origin).length();
    drawLabel(origin, label, distToEye / (DENG_GAMEVIEW_WIDTH / 2), 1 - distToEye / 2000);
}

/*
 * Visuals for Shadow Bias editing:
 */

static String labelForSource(BiasSource *s)
{
    if(!s || !editShowIndices) return String();
    /// @todo Don't assume the current map.
    return String::number(App_World().map().toIndex(*s));
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

        glTranslatef(vOrigin[VX], vOrigin[VY], vOrigin[VZ]);
        glScalef(1, 1.0f/1.2f, 1);
        glTranslatef(-vOrigin[VX], -vOrigin[VY], -vOrigin[VZ]);

        HueCircleVisual::draw(*hueCircle, vOrigin, viewData->frontVec);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    coord_t handDistance;
    Hand &hand = App_World().hand(&handDistance);

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

    if(!App_World().hasMap())
    {
        rAmbient = 0;
        return;
    }

    int mapAmbient = App_World().map().ambientLightLevel();
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
        return false; // Continue iteration.
    // Is it vissible?
    if(!Mobj_IsLinked(*mo) || !R_ViewerBspLeafIsVisible(Mobj_BspLeafAtOrigin(*mo)))
        return false; // Continue iteration.

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
    return false; // Continue iteration.
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

static void drawVector(Vector3f const &vector, float scalar, const float color[3])
{
    static const float black[4] = { 0, 0, 0, 0 };

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
            (line.definesPolyobj()? line.polyobj().bspLeaf()
                                  : hedge->face().mapElementAs<BspLeaf>()).cluster();

        coord_t const bottom = cluster.  visFloor().heightSmoothed();
        coord_t const top    = cluster.visCeiling().heightSmoothed();

        drawTangentVectorsForSurface(lineSide.middle(),
                                     Vector3d(center, bottom + (top - bottom) / 2));
    }
    else
    {
        SectorCluster &cluster =
            (line.definesPolyobj()? line.polyobj().bspLeaf()
                                  : hedge->face().mapElementAs<BspLeaf>()).cluster();
        SectorCluster &backCluster =
            (line.definesPolyobj()? line.polyobj().bspLeaf()
                                  : hedge->twin().face().mapElementAs<BspLeaf>()).cluster();

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

/**
 * @todo Use drawTangentVectorsForWallSections() for polyobjs too.
 */
static void drawSurfaceTangentVectors(SectorCluster *cluster)
{
    if(!cluster) return;

    foreach(BspLeaf *bspLeaf, cluster->bspLeafs())
    {
        HEdge const *base  = bspLeaf->poly().hedge();
        HEdge const *hedge = base;
        do
        {
            drawTangentVectorsForWallSections(hedge);
        } while((hedge = &hedge->next()) != base);

        foreach(Mesh *mesh, bspLeaf->extraMeshes())
        foreach(HEdge *hedge, mesh->hedges())
        {
            drawTangentVectorsForWallSections(hedge);
        }

        foreach(Polyobj *polyobj, bspLeaf->polyobjs())
        foreach(HEdge *hedge, polyobj->mesh().hedges())
        {
            drawTangentVectorsForWallSections(hedge);
        }
    }

    int const planeCount = cluster->sector().planeCount();
    for(int i = 0; i < planeCount; ++i)
    {
        Plane const &plane = cluster->visPlane(i);
        coord_t height     = 0;

        if(plane.surface().hasSkyMaskedMaterial() &&
           (plane.isSectorFloor() || plane.isSectorCeiling()))
        {
            height = plane.map().skyFix(plane.isSectorCeiling());
        }
        else
        {
            height = plane.heightSmoothed();
        }

        drawTangentVectorsForSurface(plane.surface(),
                                     Vector3d(cluster->center(), height));
    }
}

/**
 * Draw the surface tangent space vectors, primarily for debug.
 */
static void drawAllSurfaceTangentVectors(Map &map)
{
    if(!devSurfaceVectors) return;

    glDisable(GL_CULL_FACE);

    foreach(Sector *sector, map.sectors())
    foreach(SectorCluster *cluster, sector->clusters())
    {
        drawSurfaceTangentVectors(cluster);
    }

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

// Currently active Generators collection.
static Generators *gens;

static String labelForGenerator(ptcgen_t const *gen)
{
    DENG_ASSERT(gen != 0);
    return String("%1").arg(gens->generatorId(gen));
}

static int drawGenerator(ptcgen_t *gen, void *context)
{
    DENG2_UNUSED(context);

#define MAX_GENERATOR_DIST  2048

    if(gen->source || (gen->flags & PGF_UNTRIGGERED))
    {
        Vector3d const origin   = Generator_Origin(*gen);
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

    gens = &map.generators();
    gens->iterate(drawGenerator);
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

    BspLeaf &bspLeaf = base->face().mapElementAs<BspLeaf>();
    if(!bspLeaf.hasCluster())
        return;

    // Process neighbors?
    if(!SectorCluster::isInternalEdge(base))
    {
        ClockDirection const direction = edge? Clockwise : Anticlockwise;
        HEdge *hedge = base;
        while((hedge = &SectorClusterCirculator::findBackNeighbor(*hedge, direction)) != base)
        {
            // Stop if there is no back cluster.
            BspLeaf *backLeaf = hedge->hasFace()? &hedge->face().mapElementAs<BspLeaf>() : 0;
            if(!backLeaf || !backLeaf->hasCluster())
                break;

            if(backLeaf->cluster().visFloor().heightSmoothed() < min)
                min = backLeaf->cluster().visFloor().heightSmoothed();

            if(backLeaf->cluster().visCeiling().heightSmoothed() > max)
                max = backLeaf->cluster().visCeiling().heightSmoothed();
        }
    }
}

static int drawBspLeafVertexWorker(BspLeaf *bspLeaf, void *context)
{
    drawVertexVisual_params_t &parms = *static_cast<drawVertexVisual_params_t *>(context);

    if(!bspLeaf->hasCluster())
        return false; // Continue iteration.

    ddouble min = bspLeaf->cluster().  visFloor().heightSmoothed();
    ddouble max = bspLeaf->cluster().visCeiling().heightSmoothed();

    HEdge *base  = bspLeaf->poly().hedge();
    HEdge *hedge = base;
    do
    {
        ddouble edgeMin = min;
        ddouble edgeMax = max;
        findMinMaxPlaneHeightsAtVertex(hedge, 0 /*left edge*/, edgeMin, edgeMax);

        drawVertexVisual(hedge->vertex(), min, max, parms);

    } while((hedge = &hedge->next()) != base);

    foreach(Mesh *mesh, bspLeaf->extraMeshes())
    foreach(HEdge *hedge, mesh->hedges())
    {
        drawVertexVisual(hedge->vertex(), min, max, parms);
        drawVertexVisual(hedge->twin().vertex(), min, max, parms);
    }

    foreach(Polyobj *polyobj, bspLeaf->polyobjs())
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
        map.bspLeafBoxIterator(box, drawBspLeafVertexWorker, &parms);

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
    map.bspLeafBoxIterator(box, drawBspLeafVertexWorker, &parms);

    glEnable(GL_DEPTH_TEST);

    if(devVertexIndices)
    {
        parms.drawnVerts->fill(false); // Process all again.
        parms.drawLabel = true;
        parms.drawBar = parms.drawOrigin = false;
        map.bspLeafBoxIterator(box, drawBspLeafVertexWorker, &parms);
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

    foreach(Sector *sector, map.sectors())
    foreach(SectorCluster *cluster, sector->clusters())
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
        Con_Message("Invalid mipmapping mode %i specified. Valid range is [0..5).", newMipMode);
        return false;
    }

    mipmapping = newMipMode;
    //GL_SetAllTexturesMinFilter(glmode[mipmapping]);
    return true;
}
