/** @file rend_main.cpp  World Map Renderer.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "render/rend_main.h"

#include "resource/materialvariantspec.h"
#include "resource/clienttexture.h"

#include "world/map.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "world/sky.h"
#include "world/surface.h"
#include "world/contact.h"
#include "world/convexsubspace.h"
#include "world/subsector.h"
#include "world/vertex.h"
#include "client/clskyplane.h"
#include "render/lightdecoration.h"
#include "render/lumobj.h"
#include "render/skyfixedge.h"
#include "render/trianglestripbuilder.h"
#include "render/walledge.h"

#include "gl/gl_main.h"
#include "gl/gl_tex.h"  // pointlight_analysis_t
#include "gl/gl_texmanager.h"
#include "gl/sys_opengl.h"

#include "api_fontrender.h"
#include "render/fx/bloom.h"
#include "render/fx/vignette.h"
#include "render/fx/lensflares.h"
#include "render/angleclipper.h"
#include "render/blockmapvisual.h"
#include "render/billboard.h"
#include "render/cameralensfx.h"
#include "render/modelrenderer.h"
#include "render/r_main.h"
#include "render/r_things.h"
#include "render/rendersystem.h"
#include "render/rend_fakeradio.h"
#include "render/rend_halo.h"
#include "render/rend_particle.h"
#include "render/rendpoly.h"
#include "render/skydrawable.h"
#include "render/store.h"
#include "render/viewports.h"
#include "render/vissprite.h"
#include "render/vr.h"

#include "ui/editors/rendererappearanceeditor.h"
#include "ui/editors/modelasseteditor.h"
#include "ui/ui_main.h"
#include "ui/postprocessing.h"
//#include "ui/editors/edit_bias.h"

#include "sys_system.h"
#include "dd_main.h"
#include "clientapp.h"
#include "network/net_main.h"

#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <doomsday/defs/sprite.h>
#include <doomsday/res/texturemanifest.h>
#include <doomsday/r_util.h>
#include <doomsday/mesh/face.h>
#include <doomsday/world/blockmap.h>
#include <doomsday/world/bspleaf.h>
#include <doomsday/world/lineowner.h>
#include <doomsday/world/materials.h>
#include <doomsday/world/thinkers.h>
#include <doomsday/world/bspnode.h>
#include <de/legacy/concurrency.h>
#include <de/legacy/timer.h>
#include <de/legacy/texgamma.h>
#include <de/legacy/vector1.h>
#include <de/glinfo.h>
#include <de/glstate.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace de;
using world::World;

/**
 * POD structure used to transport vertex data refs as a logical unit.
 * @note The geometric data itself is not owned!
 */
struct Geometry
{
    Vec3f *pos;
    Vec4f *color;
    Vec2f *tex;
    Vec2f *tex2;
};

/**
 * POD structure used to describe texture modulation data.
 */
struct TexModulationData
{
    DGLuint texture = 0;
    Vec3f color;
    Vec2f topLeft;
    Vec2f bottomRight;
};

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

void Rend_DrawBBox(const Vec3d &pos, coord_t w, coord_t l, coord_t h, float a,
    float const color[3], float alpha, float br, bool alignToBase = true);

void Rend_DrawArrow(const Vec3d &pos, float a, float s, float const color3f[3], float alpha);

D_CMD(OpenRendererAppearanceEditor);
D_CMD(LowRes);
D_CMD(MipMap);
D_CMD(TexReset);
D_CMD(CubeShot);

FogParams fogParams;
float fieldOfView = 95.0f;
dbyte smoothTexAnim = true;

int renderTextures = true;
#if defined (DE_OPENGL)
int renderWireframe;
#endif

int dynlightBlend;

Vec3f torchColor(1, 1, 1);

int useShinySurfaces = true;

int useDynLights = true;
float dynlightFactor = .7f;
float dynlightFogBright = .15f;

int useGlowOnWalls = true;
float glowFactor = .8f;
float glowHeightFactor = 3;  ///< Glow height as a multiplier.
int glowHeightMax = 100;     ///< 100 is the default (0-1024).

int useShadows = true;
float shadowFactor = 1.2f;
int shadowMaxRadius = 80;
int shadowMaxDistance = 1000;

dbyte useLightDecorations = true;  ///< cvar

float detailFactor = .5f;
float detailScale = 4;

int mipmapping = 5;
int filterUI   = 1;
int texQuality = TEXQ_BEST;

int ratioLimit;      ///< Zero if none.
dd_bool fillOutlines = true;
int useSmartFilter;  ///< Smart filter mode (cvar: 1=hq2x)
int filterSprites = true;
int texMagMode = 1;  ///< Linear.
int texAniso = -1;   ///< Use best.

//dd_bool noHighResTex;
//dd_bool noHighResPatches;
//dd_bool highResWithPWAD;
dbyte loadExtAlways;  ///< Always check for extres (cvar)

float texGamma;

GLenum glmode[6] = { // Extern; indexed by 'mipmapping'.
    GL_NEAREST,
    GL_LINEAR,
    GL_NEAREST_MIPMAP_NEAREST,
    GL_LINEAR_MIPMAP_NEAREST,
    GL_NEAREST_MIPMAP_LINEAR,
    GL_LINEAR_MIPMAP_LINEAR
};

Vec3d vOrigin;
float vang, vpitch;
float viewsidex, viewsidey;

// Helper for overriding the normal view matrices.
struct FixedView
{
    Mat4f projectionMatrix;
    Mat4f modelViewMatrix;
    float yaw;
    float pitch;
    float horizontalFov;
};
static std::unique_ptr<FixedView> fixedView;

dbyte freezeRLs;
int devNoCulling;  ///< @c 1= disabled (cvar).
int devRendSkyMode;
dbyte devRendSkyAlways;

// Ambient lighting, rAmbient is used within the renderer, ambientLight is
// used to store the value of the ambient light cvar.
// The value chosen for rAmbient occurs in Rend_UpdateLightModMatrix
// for convenience (since we would have to recalculate the matrix anyway).
int rAmbient, ambientLight;

//int viewpw, viewph;  ///< Viewport size, in pixels.
//int viewpx, viewpy;  ///< Viewpoint top left corner, in pixels.

float yfov;

int gameDrawHUD = 1;  ///< Set to zero when we advise that the HUD should not be drawn

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

float rendLightWallAngle = 1.2f; ///< Intensity of angle-based wall lighting.
dbyte rendLightWallAngleSmooth = true;

float rendSkyLight = .273f;     ///< Intensity factor.
dbyte rendSkyLightAuto = true;
int rendMaxLumobjs;             ///< Max lumobjs per viewer, per frame. @c 0= no maximum.

int extraLight;  ///< Bumped light from gun blasts.
float extraLightDelta;

//DGLuint dlBBox;  ///< Display list id for the active-textured bbox model.

/*
 * Debug/Development cvars:
 */

dbyte devMobjVLights;            ///< @c 1= Draw mobj vertex lighting vector.
int devMobjBBox;                ///< @c 1= Draw mobj bounding boxes.
int devPolyobjBBox;             ///< @c 1= Draw polyobj bounding boxes.

dbyte devVertexIndices;          ///< @c 1= Draw vertex indices.
dbyte devVertexBars;             ///< @c 1= Draw vertex position bars.

dbyte devDrawGenerators;         ///< @c 1= Draw active generators.
dbyte devSoundEmitters;          ///< @c 1= Draw sound emitters.
dbyte devSurfaceVectors;         ///< @c 1= Draw tangent space vectors for surfaces.
dbyte devNoTexFix;               ///< @c 1= Draw "missing" rather than fix materials.

dbyte devSectorIndices;          ///< @c 1= Draw sector indicies.
dbyte devThinkerIds;             ///< @c 1= Draw (mobj) thinker indicies.

dbyte rendInfoLums;              ///< @c 1= Print lumobj debug info to the console.
dbyte devDrawLums;               ///< @c 1= Draw lumobjs origins.

#if 0
dbyte devLightGrid;              ///< @c 1= Draw lightgrid debug visual.
float devLightGridSize = 1.5f;  ///< Lightgrid debug visual size factor.
#endif

//static void drawBiasEditingVisuals(world::Map &map);
//static void drawFakeRadioShadowPoints(world::Map &map);
static void drawGenerators(Map &map);
static void drawLumobjs(Map &map);
static void drawMobjBoundingBoxes(Map &map);
static void drawSectors(Map &map);
static void drawSoundEmitters(Map &map);
static void drawSurfaceTangentVectors(Map &map);
static void drawThinkers(Map &map);
static void drawVertexes(Map &map);

// Draw state:
static Vec3d eyeOrigin;               ///< Viewer origin.
static ConvexSubspace *curSubspace;   ///< Subspace currently being drawn.
static Vec3f curSectorLightColor;
static float curSectorLightLevel;
static bool firstSubspace;            ///< No range checking for the first one.

using MaterialAnimatorLookup = Hash<const Record *, MaterialAnimator *>;

// State lookup (for speed):
static const MaterialVariantSpec *lookupMapSurfaceMaterialSpec = nullptr;
static MaterialAnimatorLookup *s_lookupSpriteMaterialAnimators;

static inline MaterialAnimatorLookup &spriteAnimLookup()
{
    if (!s_lookupSpriteMaterialAnimators)
    {
        s_lookupSpriteMaterialAnimators = new MaterialAnimatorLookup;
    }
    return *s_lookupSpriteMaterialAnimators;
}

void Rend_ResetLookups()
{
    lookupMapSurfaceMaterialSpec = nullptr;
    spriteAnimLookup().clear();

    if (ClientApp::world().hasMap())
    {
        auto &map = ClientApp::world().map();

        map.forAllSectors([] (world::Sector &sector)
        {
            sector.forAllPlanes([] (world::Plane &plane)
            {
                plane.surface().resetLookups();
                return LoopContinue;
            });
            return LoopContinue;
        });

        map.forAllLines([] (world::Line &line)
        {
            auto resetFunc = [] (world::Surface &surface) {
                surface.resetLookups();
                return LoopContinue;
            };

            line.front().forAllSurfaces(resetFunc);
            line.back() .forAllSurfaces(resetFunc);
            return LoopContinue;
        });
    }
}

static void reportWallDrawn(world::Line &line)
{
    // Already been here?
    int playerNum = DoomsdayApp::players().indexOf(viewPlayer);
    if (line.isMappedByPlayer(playerNum)) return;

    // Mark as drawn.
    line.setMappedByPlayer(playerNum);

    // Send a status report.
    if (gx.HandleMapObjectStatusReport)
    {
        gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED, line.indexInMap(),
                                       DMU_LINE, &playerNum);
    }
}

/// World/map renderer reset.
void Rend_Reset()
{
    R_ClearViewData();
    if (auto *map = maybeAs<Map>(World::get().mapPtr()))
    {
        map->removeAllLumobjs();
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
    if (fixedView)
    {
        return fixedView->horizontalFov;
    }

    if (vrCfg().mode() == VRConfig::OculusRift)
    {
        // OVR tells us which FOV to use.
        return vrCfg().oculusRift().fovX();
    }
    else
    {
        return de::clamp(1.f, fieldOfView, 179.f);
    }
}

static Vec3d vEyeOrigin;

Vec3d Rend_EyeOrigin()
{
    return vEyeOrigin;
}

void Rend_SetFixedView(int consoleNum, float yaw, float pitch, float fov, Vec2f viewportSize)
{
    const viewdata_t *viewData = &DD_Player(consoleNum)->viewport();

    fixedView.reset(new FixedView);

    fixedView->yaw = yaw;
    fixedView->pitch = pitch;
    fixedView->horizontalFov = fov;
    fixedView->modelViewMatrix =
            Mat4f::rotate(pitch, Vec3f(1, 0, 0)) *
            Mat4f::rotate(yaw,   Vec3f(0, 1, 0)) *
            Mat4f::scale(Vec3f(1.0f, 1.2f, 1.0f)) * // This is the aspect correction.
            Mat4f::translate(-viewData->current.origin.xzy());

    const Rangef clip = GL_DepthClipRange();
    fixedView->projectionMatrix = BaseGuiApp::app().vr()
            .projectionMatrix(fov, viewportSize, clip.start, clip.end) *
            Mat4f::scale(Vec3f(1, 1, -1));
}

void Rend_UnsetFixedView()
{
    fixedView.reset();
}

Mat4f Rend_GetModelViewMatrix(int consoleNum, bool inWorldSpace, bool vgaAspect)
{
    const viewdata_t *viewData = &DD_Player(consoleNum)->viewport();

    /// @todo vOrigin et al. shouldn't be changed in a getter function. -jk

    vOrigin = viewData->current.origin.xzy();
    vEyeOrigin = vOrigin;

    if (fixedView)
    {
        vang   = fixedView->yaw;
        vpitch = fixedView->pitch;
        return fixedView->modelViewMatrix;
    }

    vang   = viewData->current.angle() / (float) ANGLE_MAX * 360 - 90;  // head tracking included
    vpitch = viewData->current.pitch * 85.0 / 110.0;

    float bodyAngle = viewData->current.angleWithoutHeadTracking() / (float) ANGLE_MAX * 360 - 90;

    OculusRift &ovr = vrCfg().oculusRift();
    const bool applyHead = (vrCfg().mode() == VRConfig::OculusRift && ovr.isReady());

    Mat4f modelView;
    Mat4f headOrientation;
    Mat4f headOffset;

    if (applyHead)
    {
        Vec3f headPos = swizzle(Mat4f::rotate(bodyAngle, Vec3f(0, 1, 0))
                                       * ovr.headPosition() * vrCfg().mapUnitsPerMeter()
                                   , AxisNegX, AxisNegY, AxisZ);
        headOffset = Mat4f::translate(headPos);

        vEyeOrigin -= headPos;
    }

    if (inWorldSpace)
    {
        float yaw   = vang;
        float pitch = vpitch;
        float roll  = 0;

        /// @todo Elevate roll angle use into viewer_t, and maybe all the way up into player
        /// model.

        // Pitch and yaw can be taken directly from the head tracker, as the game is aware of
        // these values and is syncing with them independently (however, game has more
        // latency).
        if (applyHead)
        {
            // Use angles directly from the Rift for best response.
            const Vec3f pry = ovr.headOrientation();
            roll  = -radianToDegree(pry[1]);
            pitch =  radianToDegree(pry[0]);
        }

        headOrientation = Mat4f::rotate(roll,  Vec3f(0, 0, 1))
                        * Mat4f::rotate(pitch, Vec3f(1, 0, 0))
                        * Mat4f::rotate(yaw,   Vec3f(0, 1, 0));

        modelView = headOrientation * headOffset;
    }

    if (applyHead)
    {
        // Apply the current eye offset to the eye origin.
        vEyeOrigin -= headOrientation.inverse() * (ovr.eyeOffset() * vrCfg().mapUnitsPerMeter());
    }

    return (modelView
            * Mat4f::scale(Vec3f(1.0f, vgaAspect ? 1.2f : 1.0f, 1.0f)) // This is the aspect correction.
            * Mat4f::translate(-vOrigin));
}

void Rend_ModelViewMatrix(bool inWorldSpace)
{
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_LoadMatrix(
        Rend_GetModelViewMatrix(DoomsdayApp::players().indexOf(viewPlayer), inWorldSpace).values());
}

Mat4f Rend_GetProjectionMatrix(float fixedFov, float clipRangeScale)
{
    if (fixedView)
    {
        return fixedView->projectionMatrix;
    }

    const Vec2f size = R_Console3DViewRect(displayPlayer).size();

    // IssueID #2379: adapt fixed FOV angle for a 4:3 aspect ratio
    if (fixedFov > 0)
    {
        const float ASPECT_DEFAULT = 16.0f / 9.0f;
        const float ASPECT_MIN     = 4.0f / 3.0f;
        const float aspect         = de::max(size.x / size.y, ASPECT_MIN);

        if (aspect < ASPECT_DEFAULT)
        {
            // The fixed FOV angle is specified for a 16:9 horizontal view.
            // Adjust for this aspect ratio.
            fixedFov *= lerp(0.825f, 1.0f, (aspect - ASPECT_MIN) / (ASPECT_DEFAULT - ASPECT_MIN));
        }
    }

    const float   fov  = (fixedFov > 0 ? fixedFov : Rend_FieldOfView());
    yfov                = vrCfg().verticalFieldOfView(fov, size);
    const Rangef clip   = GL_DepthClipRange();
    return vrCfg().projectionMatrix(
               fov, size, clip.start * clipRangeScale, clip.end * clipRangeScale) *
           Mat4f::scale(Vec3f(1, 1, -1));
}

static inline double viewFacingDot(const Vec2d &v1, const Vec2d &v2)
{
    // The dot product.
    return (v1.y - v2.y) * (v1.x - Rend_EyeOrigin().x) + (v2.x - v1.x) * (v1.y - Rend_EyeOrigin().z);
}

float Rend_ExtraLightDelta()
{
    return extraLightDelta;
}

void Rend_ApplyTorchLight(Vec4f &color, float distance)
{
    ddplayer_t *ddpl = &viewPlayer->publicData();

    // Disabled?
    if (!ddpl->fixedColorMap) return;

    // Check for torch.
    if (!rendLightAttenuateFixedColormap || distance < 1024)
    {
        // Colormap 1 is the brightest. I'm guessing 16 would be
        // the darkest.
        float d = (16 - ddpl->fixedColorMap) / 15.0f;
        if (rendLightAttenuateFixedColormap)
        {
            d *= (1024 - distance) / 1024.0f;
        }

        color = color.max(torchColor * d);
    }
}

void Rend_ApplyTorchLight(float *color3, float distance)
{
    Vec4f tmp(Vec3f(color3), 0);
    Rend_ApplyTorchLight(tmp, distance);
    for (int i = 0; i < 3; ++i)
    {
        color3[i] = tmp[i];
    }
}

float Rend_AttenuateLightLevel(float distToViewer, float lightLevel)
{
    if (distToViewer > 0 && rendLightDistanceAttenuation > 0)
    {
        float real = lightLevel -
            (distToViewer - 32) / rendLightDistanceAttenuation *
                (1 - lightLevel);

        float minimum = de::max(0.f, de::squared(lightLevel) + (lightLevel - .63f) * .5f);
        if (real < minimum)
            real = minimum; // Clamp it.

        return de::min(real, 1.f);
    }

    return lightLevel;
}

float Rend_ShadowAttenuationFactor(coord_t distance)
{
    if (shadowMaxDistance > 0 && distance > 3 * shadowMaxDistance / 4)
    {
        return (shadowMaxDistance - distance) / (shadowMaxDistance / 4);
    }
    return 1;
}

static Vec3f skyLightColor;
static Vec3f oldSkyAmbientColor(-1.f, -1.f, -1.f);
static float oldRendSkyLight = -1;

bool Rend_SkyLightIsEnabled()
{
    return rendSkyLight > .001f;
}

Vec3f Rend_SkyLightColor()
{
    if (Rend_SkyLightIsEnabled() && ClientApp::world().hasMap())
    {
        auto &sky = ClientApp::world().map().sky().as<Sky>();
        const Vec3f &ambientColor = sky.ambientColor();

        if (rendSkyLight != oldRendSkyLight
            || !INRANGE_OF(ambientColor.x, oldSkyAmbientColor.x, .001f)
            || !INRANGE_OF(ambientColor.y, oldSkyAmbientColor.y, .001f)
            || !INRANGE_OF(ambientColor.z, oldSkyAmbientColor.z, .001f))
        {
            skyLightColor = ambientColor;
            R_AmplifyColor(skyLightColor);

            // Apply the intensity factor cvar.
            for (int i = 0; i < 3; ++i)
            {
                skyLightColor[i] = skyLightColor[i] + (1 - rendSkyLight) * (1.f - skyLightColor[i]);
            }

#if 0
            // When the sky light color changes we must update the light grid.
            scheduleFullLightGridUpdate();
#endif
            oldSkyAmbientColor = ambientColor;
        }

        oldRendSkyLight = rendSkyLight;
        return skyLightColor;
    }

    return Vec3f(1);
}

/**
 * Determine the effective ambient light color for the given @a sector. Usually
 * one would obtain this info from Subsector, however in some situations the
 * correct light color is *not* that of the subsector (e.g., where map hacks use
 * mapped planes to reference another sector).
 */
static Vec3f Rend_AmbientLightColor(const world::Sector &sector)
{
    if (Rend_SkyLightIsEnabled() && sector.hasSkyMaskPlane())
    {
        return Rend_SkyLightColor();
    }

    // A non-skylight sector (i.e., everything else!)
    // Return the sector's ambient light color.
    return sector.lightColor();
}

Vec3f Rend_LuminousColor(const Vec3f &color, float light)
{
    light = de::clamp(0.f, light, 1.f) * dynlightFactor;

    // In fog additive blending is used; the normal fog color is way too bright.
    if (fogParams.usingFog) light *= dynlightFogBright;

    // Multiply light with (ambient) color.
    return color * light;
}

coord_t Rend_PlaneGlowHeight(float intensity)
{
    return de::clamp<double>(0, GLOW_HEIGHT_MAX * intensity * glowHeightFactor, glowHeightMax);
}

ClientMaterial *Rend_ChooseMapSurfaceMaterial(const Surface &surface)
{
    switch (renderTextures)
    {
    case 0:  // No texture mode.
    case 1:  // Normal mode.
        if (!(devNoTexFix && surface.hasFixMaterial()))
        {
            if (!surface.hasMaterial() && surface.parent().type() == DMU_SIDE)
            {
                // These are a few special cases to deal with DOOM render hacks:
                const LineSide &side = surface.parent().as<LineSide>();
                if (side.hasSector())
                {
                    if (auto *midAnim = side.middle().as<Surface>().materialAnimator())
                    {
                        DE_ASSERT(&surface != &side.middle());
                        if (!midAnim->isOpaque())
                        {
                            return &midAnim->material();
                        }
                    }

                    if (&surface != &side.top() && !side.hasAtLeastOneMaterial())
                    {
                        // Keep it blank.
                        return nullptr;
                    }

                    // Use the ceiling for missing top section, and floor for missing bottom section.
                    if (&surface == &side.bottom())
                    {
                        return static_cast<ClientMaterial *>(side.sector().floor().surface().materialPtr());
                    }
                    else if (&surface == &side.top())
                    {
                        /*
                        // TNT map31: missing upper texture, high ceiling in the room
                        if (side.back().hasSector())
                        {
                            const auto &backSector = side.back().sector();
                            //const auto backCeilZ = backSector.ceiling().heightSmoothed();
                            //const auto frontCeilZ = side.sector().ceiling().heightSmoothed();
                            if (fequal(backSector.ceiling().height(), backSector.floor().height()))
                    {
                                return nullptr;
                            }
                        }*/

                        return static_cast<ClientMaterial *>(side.sector().ceiling().surface().materialPtr());
                    }
                }
            }
            if (surface.hasMaterial() || surface.parent().type() != DMU_PLANE)
            {
                return static_cast<ClientMaterial *>(surface.materialPtr());
            }
        }

        // Use special "missing" material.
        return &ClientMaterial::find(res::Uri("System", Path("missing")));

    case 2:  // Lighting debug mode.
        if (surface.hasMaterial() && !(!devNoTexFix && surface.hasFixMaterial()))
        {
            if (!surface.hasSkyMaskedMaterial() || devRendSkyMode)
            {
                // Use the special "gray" material.
                return &ClientMaterial::find(res::Uri("System", Path("gray")));
            }
        }
        break;

    default: break;
    }

    // No material, then.
    return nullptr;
}

/**
 * This doesn't create a rendering primitive but a vissprite! The vissprite
 * represents the masked poly and will be rendered during the rendering
 * of sprites. This is necessary because all masked polygons must be
 * rendered back-to-front, or there will be alpha artifacts along edges.
 */
void Rend_AddMaskedPoly(const Vec3f *rvertices, const Vec4f *rcolors,
    coord_t wallLength, MaterialAnimator *matAnimator, const Vec2f &origin,
    blendmode_t blendMode, uint32_t lightListIdx, float glow)
{
    vissprite_t *vis = R_NewVisSprite(VSPR_MASKED_WALL);

    vis->pose.origin   = (rvertices[0] + rvertices[3]) / 2;
    vis->pose.distance = Rend_PointDist2D(vis->pose.origin);

    VS_WALL(vis)->texOffset[0] = origin[0];
    VS_WALL(vis)->texOffset[1] = origin[1];

    // Masked walls are sometimes used for special effects like arcs,
    // cobwebs and bottoms of sails. In order for them to look right,
    // we need to disable texture wrapping on the horizontal axis (S).
    // Most masked walls need wrapping, though. What we need to do is
    // look at the texture coordinates and see if they require texture
    // wrapping.
    if (renderTextures)
    {
        // Ensure we've up to date info about the material.
        matAnimator->prepare();

        const Vec2ui &matDimensions = matAnimator->dimensions();

        VS_WALL(vis)->texCoord[0][0] = VS_WALL(vis)->texOffset[0] / matDimensions.x;
        VS_WALL(vis)->texCoord[1][0] = VS_WALL(vis)->texCoord[0][0] + wallLength / matDimensions.x;
        VS_WALL(vis)->texCoord[0][1] = VS_WALL(vis)->texOffset[1] / matDimensions.y;
        VS_WALL(vis)->texCoord[1][1] = VS_WALL(vis)->texCoord[0][1] +
                (rvertices[3].z - rvertices[0].z) / matDimensions.y;

        GLenum wrapS = GL_REPEAT, wrapT = GL_REPEAT;
        if (!matAnimator->isOpaque())
        {
            if (!(VS_WALL(vis)->texCoord[0][0] < 0 || VS_WALL(vis)->texCoord[0][0] > 1 ||
                 VS_WALL(vis)->texCoord[1][0] < 0 || VS_WALL(vis)->texCoord[1][0] > 1))
            {
                // Visible portion is within the actual [0..1] range.
                wrapS = GL_CLAMP_TO_EDGE;
            }

            // Clamp on the vertical axis if the coords are in the normal [0..1] range.
            if (!(VS_WALL(vis)->texCoord[0][1] < 0 || VS_WALL(vis)->texCoord[0][1] > 1 ||
                 VS_WALL(vis)->texCoord[1][1] < 0 || VS_WALL(vis)->texCoord[1][1] > 1))
            {
                wrapT = GL_CLAMP_TO_EDGE;
            }
        }

        // Choose a specific variant for use as a middle wall section.
        matAnimator = &matAnimator->material().getAnimator(Rend_MapSurfaceMaterialSpec(wrapS, wrapT));
    }

    VS_WALL(vis)->animator  = matAnimator;
    VS_WALL(vis)->blendMode = blendMode;

    for (int i = 0; i < 4; ++i)
    {
        VS_WALL(vis)->vertices[i].pos[0] = rvertices[i].x;
        VS_WALL(vis)->vertices[i].pos[1] = rvertices[i].y;
        VS_WALL(vis)->vertices[i].pos[2] = rvertices[i].z;

        for (int c = 0; c < 4; ++c)
        {
            /// @todo Do not clamp here.
            VS_WALL(vis)->vertices[i].color[c] = de::clamp(0.f, rcolors[i][c], 1.f);
        }
    }

    /// @todo Semitransparent masked polys arn't lit atm
    if (glow < 1 && lightListIdx && !(rcolors[0].w < 1))
    {
        // The dynlights will have already been sorted so that the brightest
        // and largest of them is first in the list. So grab that one.
        ClientApp::render().forAllSurfaceProjections(lightListIdx, [&vis] (const ProjectedTextureData &tp)
        {
            VS_WALL(vis)->modTex = tp.texture;
            VS_WALL(vis)->modTexCoord[0][0] = tp.topLeft.x;
            VS_WALL(vis)->modTexCoord[0][1] = tp.topLeft.y;
            VS_WALL(vis)->modTexCoord[1][0] = tp.bottomRight.x;
            VS_WALL(vis)->modTexCoord[1][1] = tp.bottomRight.y;
            for (int c = 0; c < 4; ++c)
            {
                VS_WALL(vis)->modColor[c] = tp.color[c];
            }
            return LoopAbort;
        });
    }
    else
    {
        VS_WALL(vis)->modTex = 0;
    }
}

static void quadTexCoords(Vec2f *tc, const Vec3f *rverts, coord_t wallLength, const Vec3d &topLeft)
{
    DE_ASSERT(tc && rverts);
    tc[0].x = tc[1].x = rverts[0].x - topLeft.x;
    tc[3].y = tc[1].y = rverts[0].y - topLeft.y;
    tc[3].x = tc[2].x = tc[0].x + wallLength;
    tc[2].y = tc[3].y + (rverts[1].z - rverts[0].z);
    tc[0].y = tc[3].y + (rverts[3].z - rverts[2].z);
}

static void lightVertex(Vec4f &color, const Vec3f &vtx, float lightLevel, const Vec3f &ambientColor)
{
    const float dist = Rend_PointDist2D(vtx);

    // Apply distance attenuation.
    lightLevel = Rend_AttenuateLightLevel(dist, lightLevel);

    // Add extra light.
    lightLevel = de::clamp(0.f, lightLevel + Rend_ExtraLightDelta(), 1.f);

    Rend_ApplyLightAdaptation(lightLevel);

    color.x = lightLevel * ambientColor.x;
    color.y = lightLevel * ambientColor.y;
    color.z = lightLevel * ambientColor.z;
}

/**
 * Apply map-space lighting to the given geometry. All vertex lighting contributions affecting
 * map-space geometry are applied here.
 *
 * @param verts             Geometry to be illuminated.
 *
 * Surface geometry:
 * @param numVertices       Total number of map-space surface geometry vertices.
 * @param posCoords         Position coordinates for the map-space surface geometry.
 * @param mapElement        Source MapElement for the map-space surface geometry.
 * @param geomGroup         Source MapElement geometry-group selector, for the map-space surface geometry.
 * @param surfaceTangents   Tangent-space vectors for the map-space surface geometry.
 *
 * Surface lighting characteristics:
 * @param color             Tint color.
 * @param color2            Secondary tint color, for walls (if any).
 * @param glowing           Self-luminosity factor (normalized [0..1]).
 * @param luminosityDeltas  Edge luminosity deltas (for walls [left edge, right edge]).
 */
static void lightWallOrFlatGeometry(Geometry &verts, uint32_t numVertices, const Vec3f *posCoords,
    world::MapElement &mapElement, int /*geomGroup*/, const Mat3f &/*surfaceTangents*/,
    const Vec3f &color, const Vec3f *color2, float glowing, float const luminosityDeltas[2])
{
    const bool haveWall = is<LineSideSegment>(mapElement);
    //auto &subsec = ::curSubspace->subsector().as<Subsector>();

    // Uniform color?
    if (::levelFullBright || !(glowing < 1))
    {
        const float lum = de::clamp(0.f, ::curSectorLightLevel + (::levelFullBright? 1 : glowing), 1.f);
        Vec4f const uniformColor(lum, lum, lum, 0);
        for (uint32_t i = 0; i < numVertices; ++i)
        {
            verts.color[i] = uniformColor;
        }
        return;
    }

#if 0
    if (::useBias)  // Bias lighting model.
    {
        Map &map     = subsec.sector().map();
        Shard &shard = subsec.shard(mapElement, geomGroup);

        // Apply the ambient light term from the grid (if available).
        if (map.hasLightGrid())
        {
            for (uint32_t i = 0; i < numVertices; ++i)
            {
                verts.color[i] = map.lightGrid().evaluate(posCoords[i]);
            }
        }

        // Apply bias light source contributions.
        shard.lightWithBiasSources(posCoords, verts.color, surfaceTangents, map.biasCurrentTime());

        // Apply surface glow.
        if (glowing > 0)
        {
            Vec4f const glow(glowing, glowing, glowing, 0);
            for (uint32_t i = 0; i < numVertices; ++i)
            {
                verts.color[i] += glow;
            }
        }

        // Apply light range compression and clamp.
        for (uint32_t i = 0; i < numVertices; ++i)
        {
            Vec4f &color = verts.color[i];
            for (int k = 0; k < 3; ++k)
            {
                color[k] = de::clamp(0.f, color[k] + Rend_LightAdaptationDelta(color[k]), 1.f);
            }
        }
    }
    else  // Doom lighting model.
#endif
    {
        // Blend sector light color with the surface color tint.
        const Vec3f colorBlended = ::curSectorLightColor * color;
        const float lumLeft  = de::clamp(0.f, ::curSectorLightLevel + luminosityDeltas[0] + glowing, 1.f);
        const float lumRight = de::clamp(0.f, ::curSectorLightLevel + luminosityDeltas[1] + glowing, 1.f);

        if (haveWall && !de::fequal(lumLeft, lumRight))
        {
            lightVertex(verts.color[0], posCoords[0], lumLeft,  colorBlended);
            lightVertex(verts.color[1], posCoords[1], lumLeft,  colorBlended);
            lightVertex(verts.color[2], posCoords[2], lumRight, colorBlended);
            lightVertex(verts.color[3], posCoords[3], lumRight, colorBlended);
        }
        else
        {
            for (uint32_t i = 0; i < numVertices; ++i)
            {
                lightVertex(verts.color[i], posCoords[i], lumLeft, colorBlended);
            }
        }

        // Secondary color?
        if (haveWall && color2)
        {
            // Blend the secondary surface color tint with the sector light color.
            const Vec3f color2Blended = ::curSectorLightColor * (*color2);
            lightVertex(verts.color[0], posCoords[0], lumLeft,  color2Blended);
            lightVertex(verts.color[2], posCoords[2], lumRight, color2Blended);
        }
    }

    // Apply torch light?
    DE_ASSERT(::viewPlayer);
    if (::viewPlayer->publicData().fixedColorMap)
    {
        for (uint32_t i = 0; i < numVertices; ++i)
        {
            Rend_ApplyTorchLight(verts.color[i], Rend_PointDist2D(posCoords[i]));
        }
    }
}

static void makeFlatGeometry(Geometry &verts, uint32_t numVertices, const Vec3f *posCoords,
    const Vec3d &topLeft, const Vec3d & /*bottomRight*/, world::MapElement &mapElement, int geomGroup,
    const Mat3f &surfaceTangents, float uniformOpacity, const Vec3f &color, const Vec3f *color2,
    float glowing, float const luminosityDeltas[2], bool useVertexLighting = true)
{
    DE_ASSERT(posCoords);

    for (uint32_t i = 0; i < numVertices; ++i)
    {
        verts.pos[i] = posCoords[i];

        Vec3f const delta(posCoords[i] - topLeft);
        if (verts.tex)  // Primary.
        {
            verts.tex[i] = Vec2f(delta.x, -delta.y);
        }
        if (verts.tex2)  // Inter.
        {
            verts.tex2[i] = Vec2f(delta.x, -delta.y);
        }
    }

    // Light the geometry?
    if (useVertexLighting)
    {
        lightWallOrFlatGeometry(verts, numVertices, posCoords, mapElement, geomGroup, surfaceTangents,
                                color, color2, glowing, luminosityDeltas);

        // Apply uniform opacity (overwritting luminance factors).
        for (uint32_t i = 0; i < numVertices; ++i)
        {
            verts.color[i].w = uniformOpacity;
        }
    }
}

static void makeWallGeometry(Geometry &verts, uint32_t numVertices, const Vec3f *posCoords,
    const Vec3d &topLeft, const Vec3d & /*bottomRight*/, coord_t sectionWidth,
    world::MapElement &mapElement, int geomGroup, const Mat3f &surfaceTangents, float uniformOpacity,
    const Vec3f &color, const Vec3f *color2, float glowing, float const luminosityDeltas[2],
    bool useVertexLighting = true)
{
    DE_ASSERT(posCoords);

    for (uint32_t i = 0; i < numVertices; ++i)
    {
        verts.pos[i] = posCoords[i];
    }
    if (verts.tex)  // Primary.
    {
        quadTexCoords(verts.tex, posCoords, sectionWidth, topLeft);
    }
    if (verts.tex2)  // Inter.
    {
        quadTexCoords(verts.tex2, posCoords, sectionWidth, topLeft);
    }

    // Light the geometry?
    if (useVertexLighting)
    {
        lightWallOrFlatGeometry(verts, numVertices, posCoords, mapElement, geomGroup, surfaceTangents,
                                color, color2, glowing, luminosityDeltas);

        // Apply uniform opacity (overwritting luminance factors).
        for (uint32_t i = 0; i < numVertices; ++i)
        {
            verts.color[i].w = uniformOpacity;
        }
    }
}

static inline float shineVertical(float dy, float dx)
{
    return ((std::atan(dy/dx) / (PI/2)) + 1) / 2;
}

static void makeFlatShineGeometry(Geometry &verts, uint32_t numVertices, const Vec3f *posCoords,
    const Geometry &mainVerts, const Vec3f &shineColor, float shineOpacity)
{
    DE_ASSERT(posCoords);

    for (uint32_t i = 0; i < numVertices; ++i)
    {
        const Vec3f eye = Rend_EyeOrigin();

        // Determine distance to viewer.
        float distToEye = (eye.xz() - Vec2f(posCoords[i])).normalize().length();
        // Short distances produce an ugly 'crunch' below and above the viewpoint.
        if (distToEye < 10) distToEye = 10;

        // Offset from the normal view plane.
        float offset = ((eye.y - posCoords[i].y) * sin(.4f)/*viewFrontVec[0]*/ -
                         (eye.x - posCoords[i].x) * cos(.4f)/*viewFrontVec[2]*/);

        verts.tex[i] = Vec2f(((shineVertical(offset, distToEye) - .5f) * 2) + .5f,
                                shineVertical(eye.y - posCoords[i].z, distToEye));
    }

    for (uint32_t i = 0; i < numVertices; ++i)
    {
        verts.color[i] = Vec4f(Vec3f(mainVerts.color[i]).max(shineColor), shineOpacity);
    }
}

static void makeWallShineGeometry(Geometry &verts, uint32_t numVertices, const Vec3f *posCoords,
    const Geometry &mainVerts, coord_t sectionWidth, const Vec3f &shineColor, float shineOpactiy)
{
    DE_ASSERT(posCoords);

    const Vec3f &topLeft     = posCoords[1];
    const Vec3f &bottomRight = posCoords[2];

    // Quad surface vector.
    const Vec2f normal = Vec2f( (bottomRight.y - topLeft.y) / sectionWidth,
                                     -(bottomRight.x - topLeft.x) / sectionWidth);

    // Calculate coordinates based on viewpoint and surface normal.
    const Vec3f eye = Rend_EyeOrigin();
    float prevAngle = 0;
    for (uint32_t i = 0; i < 2; ++i)
    {
        const Vec2f eyeToVert = eye.xz() - (i == 0 ? Vec2f(topLeft) : Vec2f(bottomRight));
        const float distToEye = eyeToVert.length();
        const Vec2f view      = eyeToVert.normalize();

        Vec2f projected;
        const float div = normal.lengthSquared();
        if (div != 0)
        {
            projected = normal * view.dot(normal) / div;
        }

        const Vec2f reflected = view + (projected - view) * 2;
        float angle = std::acos(reflected.y) / PI;
        if (reflected.x < 0)
        {
            angle = 1 - angle;
        }

        if (i == 0)
        {
            prevAngle = angle;
        }
        else if (angle > prevAngle)
        {
            angle -= 1;
        }

        // Horizontal coordinates.
        verts.tex[ (i == 0 ? 1 : 2) ].x = angle + .3f /*std::acos(-dot)/PI*/;
        verts.tex[ (i == 0 ? 0 : 3) ].x = angle + .3f /*std::acos(-dot)/PI*/;

        // Vertical coordinates.
        verts.tex[ (i == 0 ? 0 : 2) ].y = shineVertical(eye.y - bottomRight.z, distToEye);
        verts.tex[ (i == 0 ? 1 : 3) ].y = shineVertical(eye.y - topLeft.z,     distToEye);
    }

    for (uint32_t i = 0; i < numVertices; ++i)
    {
        verts.color[i] = Vec4f(Vec3f(mainVerts.color[i]).max(shineColor), shineOpactiy);
    }
}

static void makeFlatShadowGeometry(Geometry &verts, const Vec3d &topLeft, const Vec3d &bottomRight,
    uint32_t numVertices, const Vec3f *posCoords,
    const ProjectedTextureData &tp)
{
    DE_ASSERT(posCoords);

    const Vec4f colorClamped = tp.color.min(Vec4f(1)).max(Vec4f());
    for (uint32_t i = 0; i < numVertices; ++i)
    {
        verts.color[i] = colorClamped;
    }

    const float width  = bottomRight.x - topLeft.x;
    const float height = bottomRight.y - topLeft.y;

    for (uint32_t i = 0; i < numVertices; ++i)
    {
        verts.tex[i].x = ((bottomRight.x - posCoords[i].x) / width * tp.topLeft.x) +
            ((posCoords[i].x - topLeft.x) / width * tp.bottomRight.x);

        verts.tex[i].y = ((bottomRight.y - posCoords[i].y) / height * tp.topLeft.y) +
            ((posCoords[i].y - topLeft.y) / height * tp.bottomRight.y);
    }

    std::memcpy(verts.pos, posCoords, sizeof(Vec3f) * numVertices);
}

static void makeWallShadowGeometry(Geometry &verts, const Vec3d &/*topLeft*/, const Vec3d &/*bottomRight*/,
    uint32_t numVertices, const Vec3f *posCoords, const WallEdge &leftEdge, const WallEdge &rightEdge,
    const ProjectedTextureData &tp)
{
    DE_ASSERT(posCoords);

    const Vec4f colorClamped = tp.color.min(Vec4f(1)).max(Vec4f());
    for (uint32_t i = 0; i < numVertices; ++i)
    {
        verts.color[i] = colorClamped;
    }

    verts.tex[1].x = verts.tex[0].x = tp.topLeft.x;
    verts.tex[1].y = verts.tex[3].y = tp.topLeft.y;
    verts.tex[3].x = verts.tex[2].x = tp.bottomRight.x;
    verts.tex[2].y = verts.tex[0].y = tp.bottomRight.y;

    // If either edge has divisions - make two trifans.
    if (leftEdge.divisionCount() || rightEdge.divisionCount())
    {
        // Need to swap indices around into fans set the position of the
        // division vertices, interpolate texcoords and color.

        Vec3f origPosCoords[4]; std::memcpy(origPosCoords, posCoords,   sizeof(Vec3f) * 4);
        Vec2f origTexCoords[4]; std::memcpy(origTexCoords, verts.tex,   sizeof(Vec2f) * 4);
        Vec4f origColors[4];    std::memcpy(origColors,    verts.color, sizeof(Vec4f) * 4);

        R_DivVerts     (verts.pos,   origPosCoords, leftEdge, rightEdge);
        R_DivTexCoords (verts.tex,   origTexCoords, leftEdge, rightEdge);
        R_DivVertColors(verts.color, origColors,    leftEdge, rightEdge);
    }
    else
    {
        std::memcpy(verts.pos, posCoords, sizeof(Vec3f) * numVertices);
    }
}

static void makeFlatLightGeometry(Geometry &verts, const Vec3d &topLeft, const Vec3d &bottomRight,
    uint32_t numVertices, const Vec3f *posCoords,
    const ProjectedTextureData &tp)
{
    DE_ASSERT(posCoords);

    const Vec4f colorClamped = tp.color.min(Vec4f(1)).max(Vec4f());
    for (uint32_t i = 0; i < numVertices; ++i)
    {
        verts.color[i] = colorClamped;
    }

    const float width  = bottomRight.x - topLeft.x;
    const float height = bottomRight.y - topLeft.y;
    for (uint32_t i = 0; i < numVertices; ++i)
    {
        verts.tex[i].x = ((bottomRight.x - posCoords[i].x) / width * tp.topLeft.x) +
            ((posCoords[i].x - topLeft.x) / width * tp.bottomRight.x);

        verts.tex[i].y = ((bottomRight.y - posCoords[i].y) / height * tp.topLeft.y) +
            ((posCoords[i].y - topLeft.y) / height * tp.bottomRight.y);
    }

    std::memcpy(verts.pos, posCoords, sizeof(Vec3f) * numVertices);
}

static void makeWallLightGeometry(Geometry &verts, const Vec3d &/*topLeft*/, const Vec3d &/*bottomRight*/,
    uint32_t numVertices, const Vec3f *posCoords, const WallEdge &leftEdge, const WallEdge &rightEdge,
    const ProjectedTextureData &tp)
{
    DE_ASSERT(posCoords);

    const Vec4f colorClamped = tp.color.min(Vec4f(1)).max(Vec4f());
    for (uint32_t i = 0; i < numVertices; ++i)
    {
        verts.color[i] = colorClamped;
    }

    verts.tex[1].x = verts.tex[0].x = tp.topLeft.x;
    verts.tex[1].y = verts.tex[3].y = tp.topLeft.y;
    verts.tex[3].x = verts.tex[2].x = tp.bottomRight.x;
    verts.tex[2].y = verts.tex[0].y = tp.bottomRight.y;

    // If either edge has divisions - make two trifans.
    if (leftEdge.divisionCount() || rightEdge.divisionCount())
    {
        // Need to swap indices around into fans set the position
        // of the division vertices, interpolate texcoords and color.

        Vec3f origPosCoords[4]; std::memcpy(origPosCoords, posCoords,   sizeof(Vec3f) * 4);
        Vec2f origTexCoords[4]; std::memcpy(origTexCoords, verts.tex,   sizeof(Vec2f) * 4);
        Vec4f origColors[4];    std::memcpy(origColors,    verts.color, sizeof(Vec4f) * 4);

        R_DivVerts     (verts.pos,   origPosCoords, leftEdge, rightEdge);
        R_DivTexCoords (verts.tex,   origTexCoords, leftEdge, rightEdge);
        R_DivVertColors(verts.color, origColors,    leftEdge, rightEdge);
    }
    else
    {
        std::memcpy(verts.pos, posCoords, sizeof(Vec3f) * numVertices);
    }
}

static float averageLuminosity(const Vec4f *rgbaValues, uint32_t count)
{
    DE_ASSERT(rgbaValues);
    float avg = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        const Vec4f &color = rgbaValues[i];
        avg += color.x + color.y + color.z;
    }
    return avg / (count * 3);
}

struct rendworldpoly_params_t
{
    bool         skyMasked;
    blendmode_t  blendMode;
    const Vec3d *topLeft;
    const Vec3d *bottomRight;
    const Vec2f *materialOrigin;
    const Vec2f *materialScale;
    float        alpha;
    float        surfaceLuminosityDeltas[2];
    const Vec3f *surfaceColor;
    const Mat3f *surfaceTangentMatrix;

    uint32_t           lightListIdx;  // List of lights that affect this poly.
    uint32_t           shadowListIdx; // List of shadows that affect this poly.
    float              glowing;
    bool               forceOpaque;
    world::MapElement *mapElement;
    int                geomGroup;

    bool isWall;

    // Wall only:
    struct {
        coord_t width;
        const Vec3f *surfaceColor2;  // Secondary color.
        const WallEdge *leftEdge;
        const WallEdge *rightEdge;
    } wall;
};

static bool renderWorldPoly(const Vec3f *rvertices, uint32_t numVertices,
    const rendworldpoly_params_t &p, MaterialAnimator &matAnimator)
{
    using Parm = DrawList::PrimitiveParams;

    DE_ASSERT(rvertices);

    static DrawList::Indices indices;

    // Ensure we've up to date info about the material.
    matAnimator.prepare();

    // Sky-masked polys (flats and walls)
    const bool skyMaskedMaterial        = (p.skyMasked || (matAnimator.material().isSkyMasked()));

    // Masked polys (walls) get a special treatment (=> vissprite).
    const bool drawAsVisSprite          = (!p.forceOpaque && !p.skyMasked && (!matAnimator.isOpaque() || p.alpha < 1 || p.blendMode > 0));

    // Map RTU configuration.
    const GLTextureUnit *layer0RTU      = (!p.skyMasked)? &matAnimator.texUnit(MaterialAnimator::TU_LAYER0) : nullptr;
    const GLTextureUnit *layer0InterRTU = (!p.skyMasked && !drawAsVisSprite && matAnimator.texUnit(MaterialAnimator::TU_LAYER0_INTER).hasTexture())? &matAnimator.texUnit(MaterialAnimator::TU_LAYER0_INTER) : nullptr;
    const GLTextureUnit *detailRTU      = (r_detail && !p.skyMasked && matAnimator.texUnit(MaterialAnimator::TU_DETAIL).hasTexture())? &matAnimator.texUnit(MaterialAnimator::TU_DETAIL) : nullptr;
    const GLTextureUnit *detailInterRTU = (r_detail && !p.skyMasked && matAnimator.texUnit(MaterialAnimator::TU_DETAIL_INTER).hasTexture())? &matAnimator.texUnit(MaterialAnimator::TU_DETAIL_INTER) : nullptr;

    const GLTextureUnit *shineRTU       = (::useShinySurfaces && !skyMaskedMaterial && !drawAsVisSprite && matAnimator.texUnit(MaterialAnimator::TU_SHINE).hasTexture())? &matAnimator.texUnit(MaterialAnimator::TU_SHINE) : nullptr;
    const GLTextureUnit *shineMaskRTU   = (::useShinySurfaces && !skyMaskedMaterial && !drawAsVisSprite && matAnimator.texUnit(MaterialAnimator::TU_SHINE).hasTexture() && matAnimator.texUnit(MaterialAnimator::TU_SHINE_MASK).hasTexture())? &matAnimator.texUnit(MaterialAnimator::TU_SHINE_MASK) : nullptr;

    // Make surface geometry (position, primary texture, inter texture and color coords).
    Geometry verts;
    const bool mustSubdivide = (p.isWall && (p.wall.leftEdge->divisionCount() || p.wall.rightEdge->divisionCount()));
    const uint32_t numVerts     = (mustSubdivide && !drawAsVisSprite?   3 + p.wall.leftEdge ->divisionCount()
                                                                   + 3 + p.wall.rightEdge->divisionCount()
                                                                 : numVertices);
    // Allocate vertices from the pools.
    verts.pos   = R_AllocRendVertices(numVerts);
    verts.color = !skyMaskedMaterial? R_AllocRendColors   (numVerts) : nullptr;
    verts.tex   = layer0RTU         ? R_AllocRendTexCoords(numVerts) : nullptr;
    verts.tex2  = layer0InterRTU    ? R_AllocRendTexCoords(numVerts) : nullptr;
    if (p.isWall)
    {
        makeWallGeometry(verts, numVertices, rvertices, *p.topLeft, *p.bottomRight, p.wall.width,
                         *p.mapElement, p.geomGroup, *p.surfaceTangentMatrix,
                         p.alpha, *p.surfaceColor, p.wall.surfaceColor2, p.glowing, p.surfaceLuminosityDeltas,
                         !skyMaskedMaterial);
    }
    else
    {
        makeFlatGeometry(verts, numVertices, rvertices, *p.topLeft, *p.bottomRight,
                         *p.mapElement, p.geomGroup, *p.surfaceTangentMatrix,
                         p.alpha, *p.surfaceColor, p.wall.surfaceColor2, p.glowing, p.surfaceLuminosityDeltas,
                         !skyMaskedMaterial);
    }

    if (drawAsVisSprite)
    {
        DE_ASSERT(p.isWall);

        // This is needed because all masked polys must be sorted (sprites are masked polys).
        // Otherwise there will be artifacts.
        Rend_AddMaskedPoly(verts.pos, verts.color, p.wall.width, &matAnimator,
                           *p.materialOrigin, p.blendMode, p.lightListIdx, p.glowing);

        R_FreeRendVertices (verts.pos);
        R_FreeRendColors   (verts.color);
        R_FreeRendTexCoords(verts.tex);
        R_FreeRendTexCoords(verts.tex2);

        return false;  // We HAD to use a vissprite, so it MUST not be opaque.
    }

    // Skip drawing dynamic light/shadow on surfaces too bright/dark to benefit.
    bool useLights  = (p.lightListIdx  && !skyMaskedMaterial && !drawAsVisSprite && p.glowing < 1? true : false);
    bool useShadows = (p.shadowListIdx && !skyMaskedMaterial && !drawAsVisSprite && p.glowing < 1? true : false);
    if (useLights || useShadows)
    {
        const float avg = averageLuminosity(verts.color, numVertices);
        if (avg > 0.98f)  // Fully bright.
        {
            useLights = false;  // Skip lights.
        }
        if (avg < 0.02f)  // Fully dark.
        {
            useShadows = false;  // Skip shadows.
        }
    }

    // If multitexturing is enabled and there is at least one dynlight affecting the surface,
    // locate the projection data and use it for modulation.
    TexModulationData mod;
    Vec2f *modTexCoords = nullptr;
    if (useLights && Rend_IsMTexLights())
    {
        ClientApp::render().forAllSurfaceProjections(p.lightListIdx, [&mod] (const ProjectedTextureData &dyn)
        {
            mod.texture     = dyn.texture;
            mod.color       = dyn.color;
            mod.topLeft     = dyn.topLeft;
            mod.bottomRight = dyn.bottomRight;
            return LoopAbort;
        });

        if (mod.texture)
        {
            // Prepare modulation texture coordinates.
            modTexCoords = R_AllocRendTexCoords(numVerts);
            if (p.isWall)
            {
                modTexCoords[0] = Vec2f(mod.topLeft.x, mod.bottomRight.y);
                modTexCoords[1] = mod.topLeft;
                modTexCoords[2] = mod.bottomRight;
                modTexCoords[3] = Vec2f(mod.bottomRight.x, mod.topLeft.y);
            }
            else
            {
                for (uint32_t i = 0; i < numVertices; ++i)
                {
                    modTexCoords[i] = (( Vec2f(*p.bottomRight) - Vec2f(rvertices[i]) ) / ( Vec2f(*p.bottomRight) - Vec2f(*p.topLeft) ) * mod.topLeft    )
                                    + (( Vec2f(rvertices[i]  ) - Vec2f(*p.topLeft  ) ) / ( Vec2f(*p.bottomRight) - Vec2f(*p.topLeft) ) * mod.bottomRight);
                }
            }
        }
    }

    bool hasDynlights = false;
    if (useLights)
    {
        // Write projected lights.
        // Multitexturing can be used for the first light.
        const bool skipFirst = Rend_IsMTexLights();

        uint32_t numProcessed = 0;
        ClientApp::render().forAllSurfaceProjections(p.lightListIdx,
                                           [&p, &mustSubdivide, &rvertices, &numVertices
                                           , &skipFirst, &numProcessed] (const ProjectedTextureData &tp)
        {
            if (!(skipFirst && numProcessed == 0))
            {
                // Light texture determines the list to write to.
                DrawListSpec listSpec;
                listSpec.group = LightGeom;
                listSpec.texunits[TU_PRIMARY] =
                    GLTextureUnit(tp.texture, gfx::ClampToEdge, gfx::ClampToEdge);
                DrawList &lightList = ClientApp::render().drawLists().find(listSpec);

                // Make geometry.
                Geometry verts;
                const uint32_t numVerts = mustSubdivide ?   3 + p.wall.leftEdge ->divisionCount()
                                                       + 3 + p.wall.rightEdge->divisionCount()
                                                     : numVertices;
                // Allocate verts from the pools.
                verts.pos   = R_AllocRendVertices (numVerts);
                verts.color = R_AllocRendColors   (numVerts);
                verts.tex   = R_AllocRendTexCoords(numVerts);
                if (p.isWall)
                {
                    makeWallLightGeometry(verts, *p.topLeft, *p.bottomRight,
                                          numVertices, rvertices, *p.wall.leftEdge, *p.wall.rightEdge, tp);
                }
                else
                {
                    makeFlatLightGeometry(verts, *p.topLeft, *p.bottomRight,
                                          numVertices, rvertices, tp);
                }

                // Write geometry.
                // Walls with edge divisions mean two trifans.
                if (mustSubdivide)
                {
                    DE_ASSERT(p.isWall);
                    const uint32_t numLeftVerts  = 3 + p.wall.leftEdge ->divisionCount();
                    const uint32_t numRightVerts = 3 + p.wall.rightEdge->divisionCount();

                    Store &buffer = ClientApp::render().buffer();
                    {
                        uint32_t base = buffer.allocateVertices(numRightVerts);
                        DrawList::reserveSpace(indices, numRightVerts);
                        for (uint32_t i = 0; i < numRightVerts; ++i)
                        {
                            indices[i] = base + i;
                            buffer.posCoords   [indices[i]] = verts.pos[numLeftVerts + i];
                            buffer.colorCoords [indices[i]] = (verts.color[numLeftVerts + i] * 255).toVec4ub();
                            buffer.texCoords[0][indices[i]] = verts.tex[numLeftVerts + i];
                        }
                        lightList.write(buffer, indices.data(), numRightVerts, gfx::TriangleFan);
                    }
                    {
                        uint32_t base = buffer.allocateVertices(numLeftVerts);
                        DrawList::reserveSpace(indices, numLeftVerts);
                        for (uint32_t i = 0; i < numLeftVerts; ++i)
                        {
                            indices[i] = base + i;
                            buffer.posCoords   [indices[i]] = verts.pos[i];
                            buffer.colorCoords [indices[i]] = (verts.color[i] * 255).toVec4ub();
                            buffer.texCoords[0][indices[i]] = verts.tex[i];
                        }
                        lightList.write(buffer, indices.data(), numLeftVerts, gfx::TriangleFan);
                    }
                }
                else
                {
                    Store &buffer = ClientApp::render().buffer();
                    uint32_t base = buffer.allocateVertices(numVertices);
                    DrawList::reserveSpace(indices, numVertices);
                    for (uint32_t i = 0; i < numVertices; ++i)
                    {
                        indices[i] = base + i;
                        buffer.posCoords   [indices[i]] = verts.pos[i];
                        buffer.colorCoords [indices[i]] = (verts.color[i] * 255).toVec4ub();
                        buffer.texCoords[0][indices[i]] = verts.tex[i];
                    }
                    lightList.write(buffer, indices.data(), numVertices, p.isWall? gfx::TriangleStrip : gfx::TriangleFan);
                }

                // We're done with the geometry.
                R_FreeRendVertices (verts.pos);
                R_FreeRendColors   (verts.color);
                R_FreeRendTexCoords(verts.tex);
            }
            numProcessed += 1;
            return LoopContinue;
        });

        hasDynlights = (numProcessed > uint32_t(skipFirst? 1 : 0));
    }

    if (useShadows)
    {
        // Write projected shadows.
        // All shadows use the same texture (so use the same list).
        DrawListSpec listSpec;
        listSpec.group                = ShadowGeom;
        listSpec.texunits[TU_PRIMARY] = GLTextureUnit(
            GL_PrepareLSTexture(LST_DYNAMIC), gfx::ClampToEdge, gfx::ClampToEdge);
        DrawList &shadowList = ClientApp::render().drawLists().find(listSpec);

        ClientApp::render().forAllSurfaceProjections(p.shadowListIdx,
                                           [&p, &mustSubdivide, &rvertices, &numVertices, &shadowList]
                                           (const ProjectedTextureData &tp)
        {
            // Make geometry.
            Geometry verts;
            const uint32_t numVerts = mustSubdivide ?   3 + p.wall.leftEdge ->divisionCount()
                                                   + 3 + p.wall.rightEdge->divisionCount()
                                                 : numVertices;
            // Allocate verts from the pools.
            verts.pos   = R_AllocRendVertices (numVerts);
            verts.color = R_AllocRendColors   (numVerts);
            verts.tex   = R_AllocRendTexCoords(numVerts);
            if (p.isWall)
            {
                makeWallShadowGeometry(verts, *p.topLeft, *p.bottomRight,
                                       numVertices, rvertices, *p.wall.leftEdge, *p.wall.rightEdge, tp);
            }
            else
            {
                makeFlatShadowGeometry(verts, *p.topLeft, *p.bottomRight,
                                       numVertices, rvertices, tp);
            }

            // Write geometry.
            // Walls with edge divisions mean two trifans.
            if (mustSubdivide)
            {
                DE_ASSERT(p.isWall);
                const uint32_t numLeftVerts  = 3 + p.wall.leftEdge ->divisionCount();
                const uint32_t numRightVerts = 3 + p.wall.rightEdge->divisionCount();

                Store &buffer = ClientApp::render().buffer();
                {
                    uint32_t base = buffer.allocateVertices(numRightVerts);
                    DrawList::reserveSpace(indices, numRightVerts);
                    for (uint32_t i = 0; i < numRightVerts; ++i)
                    {
                        indices[i] = base + i;
                        buffer.posCoords   [indices[i]] = verts.pos[numLeftVerts + i];
                        buffer.colorCoords [indices[i]] = (verts.color[numLeftVerts + i] * 255).toVec4ub();
                        buffer.texCoords[0][indices[i]] = verts.tex[numLeftVerts + i];
                    }
                    shadowList.write(buffer, indices.data(), numRightVerts, gfx::TriangleFan);
                }
                {
                    uint32_t base = buffer.allocateVertices(numLeftVerts);
                    DrawList::reserveSpace(indices, numLeftVerts);
                    for (uint32_t i = 0; i < numLeftVerts; ++i)
                    {
                        indices[i] = base + i;
                        buffer.posCoords   [indices[i]] = verts.pos[i];
                        buffer.colorCoords [indices[i]] = (verts.color[i] * 255).toVec4ub();
                        buffer.texCoords[0][indices[i]] = verts.tex[i];
                    }
                    shadowList.write(buffer, indices.data(), numLeftVerts, gfx::TriangleFan);
                }
            }
            else
            {
                Store &buffer = ClientApp::render().buffer();
                uint32_t base = buffer.allocateVertices(numVerts);
                DrawList::reserveSpace(indices, numVerts);
                for (uint32_t i = 0; i < numVerts; ++i)
                {
                    indices[i] = base + i;
                    buffer.posCoords   [indices[i]] = verts.pos[i];
                    buffer.colorCoords [indices[i]] = (verts.color[i] * 255).toVec4ub();
                    buffer.texCoords[0][indices[i]] = verts.tex[i];
                }
                shadowList.write(buffer, indices.data(), numVerts, p.isWall ? gfx::TriangleStrip : gfx::TriangleFan);
            }

            // We're done with the geometry.
            R_FreeRendVertices (verts.pos);
            R_FreeRendColors   (verts.color);
            R_FreeRendTexCoords(verts.tex);

            return LoopContinue;
        });
    }

    // Write geometry.
    // Walls with edge divisions mean two trifans.
    if (mustSubdivide)
    {
        DE_ASSERT(p.isWall);
        const uint32_t numLeftVerts  = 3 + p.wall.leftEdge ->divisionCount();
        const uint32_t numRightVerts = 3 + p.wall.rightEdge->divisionCount();

        // Need to swap indices around into fans set the position of the division
        // vertices, interpolate texcoords and color.

        Vec3f origPos[4]; std::memcpy(origPos, verts.pos, sizeof(origPos));
        R_DivVerts(verts.pos, origPos, *p.wall.leftEdge, *p.wall.rightEdge);

        if (verts.color)
        {
            Vec4f orig[4]; std::memcpy(orig, verts.color, sizeof(orig));
            R_DivVertColors(verts.color, orig, *p.wall.leftEdge, *p.wall.rightEdge);
        }
        if (verts.tex)
        {
            Vec2f orig[4]; std::memcpy(orig, verts.tex, sizeof(orig));
            R_DivTexCoords(verts.tex, orig, *p.wall.leftEdge, *p.wall.rightEdge);
        }
        if (verts.tex2)
        {
            Vec2f orig[4]; std::memcpy(orig, verts.tex2, sizeof(orig));
            R_DivTexCoords(verts.tex2, orig, *p.wall.leftEdge, *p.wall.rightEdge);
        }
        if (modTexCoords)
        {
            Vec2f orig[4]; std::memcpy(orig, modTexCoords, sizeof(orig));
            R_DivTexCoords(modTexCoords, orig, *p.wall.leftEdge, *p.wall.rightEdge);
        }

        if (p.skyMasked)
        {
            DrawList &skyMaskList = ClientApp::render().drawLists().find(DrawListSpec(SkyMaskGeom));

            Store &buffer = ClientApp::render().buffer();
            {
                uint32_t base = buffer.allocateVertices(numRightVerts);
                DrawList::reserveSpace(indices, numRightVerts);
                for (uint32_t i = 0; i < numRightVerts; ++i)
                {
                    indices[i] = base + i;
                    buffer.posCoords[indices[i]] = verts.pos[numLeftVerts + i];
                }
                skyMaskList.write(buffer, indices.data(), numRightVerts, gfx::TriangleFan);
            }
            {
                uint32_t base = buffer.allocateVertices(numLeftVerts);
                DrawList::reserveSpace(indices, numLeftVerts);
                for (uint32_t i = 0; i < numLeftVerts; ++i)
                {
                    indices[i] = base + i;
                    buffer.posCoords[indices[i]] = verts.pos[i];
                }
                skyMaskList.write(buffer, indices.data(), numLeftVerts, gfx::TriangleFan);
            }
        }
        else
        {
            DrawListSpec listSpec((mod.texture || hasDynlights)? LitGeom : UnlitGeom);
            if (layer0RTU)
            {
                listSpec.texunits[TU_PRIMARY] = *layer0RTU;
                if (p.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY].offset += *p.materialOrigin;
                }
                if (p.materialScale)
                {
                    listSpec.texunits[TU_PRIMARY].scale  *= *p.materialScale;
                    listSpec.texunits[TU_PRIMARY].offset *= *p.materialScale;
                }
            }
            if (layer0InterRTU)
            {
                listSpec.texunits[TU_INTER] = *layer0InterRTU;
                if (p.materialOrigin)
                {
                    listSpec.texunits[TU_INTER].offset += *p.materialOrigin;
                }
                if (p.materialScale)
                {
                    listSpec.texunits[TU_INTER].scale  *= *p.materialScale;
                    listSpec.texunits[TU_INTER].offset *= *p.materialScale;
                }
            }
            if (detailRTU)
            {
                listSpec.texunits[TU_PRIMARY_DETAIL] = *detailRTU;
                if (p.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY_DETAIL].offset += *p.materialOrigin;
                }
            }
            if (detailInterRTU)
            {
                listSpec.texunits[TU_INTER_DETAIL] = *detailInterRTU;
                if (p.materialOrigin)
                {
                    listSpec.texunits[TU_INTER_DETAIL].offset += *p.materialOrigin;
                }
            }
            DrawList &drawList = ClientApp::render().drawLists().find(listSpec);
            // Is the geometry lit?
            Flags primFlags;
            //bool oneLight   = false;
            //bool manyLights = false;
            if (mod.texture && !hasDynlights)
            {
                primFlags |= Parm::OneLight; //oneLight = true;  // Using modulation.
            }
            else if (mod.texture || hasDynlights)
            {
                //manyLights = true;
                primFlags |= Parm::ManyLights;
            }

            Store &buffer = ClientApp::render().buffer();
            {
                uint32_t base = buffer.allocateVertices(numRightVerts);
                DrawList::reserveSpace(indices, numRightVerts);
                static Vec4ub const white(255, 255, 255, 255);
                for (uint32_t i = 0; i < numRightVerts; ++i)
                {
                    indices[i] = base + i;
                    buffer.posCoords[indices[i]] = verts.pos[numLeftVerts + i];
                    if (verts.color)
                    {
                        buffer.colorCoords[indices[i]] = (verts.color[numLeftVerts + i] * 255).toVec4ub();
                    }
                    else
                    {
                        buffer.colorCoords[indices[i]] = white;
                    }
                    if (verts.tex)
                    {
                        buffer.texCoords[0][indices[i]] = verts.tex[numLeftVerts + i];
                    }
                    if (verts.tex2)
                    {
                        buffer.texCoords[1][indices[i]] = verts.tex2[numLeftVerts + i];
                    }
                    if ((primFlags & (Parm::OneLight | Parm::ManyLights)) && Rend_IsMTexLights())
                    {
                        DE_ASSERT(modTexCoords);
                        buffer.modCoords[indices[i]] = modTexCoords[numLeftVerts + i];
                    }
                }
                drawList.write(buffer, indices.data(), numRightVerts,
                               Parm(gfx::TriangleFan,
                                    listSpec.unit(TU_PRIMARY       ).scale,
                                    listSpec.unit(TU_PRIMARY       ).offset,
                                    listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                    listSpec.unit(TU_PRIMARY_DETAIL).offset,
                                    primFlags, BM_NORMAL,
                                    mod.texture, mod.color));
            }
            {
                uint32_t base = buffer.allocateVertices(numLeftVerts);
                DrawList::reserveSpace(indices, numLeftVerts);
                static Vec4ub const white(255, 255, 255, 255);
                for (uint32_t i = 0; i < numLeftVerts; ++i)
                {
                    indices[i] = base + i;
                    buffer.posCoords[indices[i]] = verts.pos[i];
                    if (verts.color)
                    {
                        buffer.colorCoords[indices[i]] = (verts.color[i] * 255).toVec4ub();
                    }
                    else
                    {
                        buffer.colorCoords[indices[i]] = white;
                    }
                    if (verts.tex)
                    {
                        buffer.texCoords[0][indices[i]] = verts.tex[i];
                    }
                    if (verts.tex2)
                    {
                        buffer.texCoords[1][indices[i]] = verts.tex2[i];
                    }
                    if ((primFlags & (Parm::OneLight | Parm::ManyLights)) && Rend_IsMTexLights())
                    {
                        DE_ASSERT(modTexCoords);
                        buffer.modCoords[indices[i]] = modTexCoords[i];
                    }
                }
                drawList.write(buffer, indices.data(), numLeftVerts,
                               Parm(gfx::TriangleFan,
                                    listSpec.unit(TU_PRIMARY       ).scale,
                                    listSpec.unit(TU_PRIMARY       ).offset,
                                    listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                    listSpec.unit(TU_PRIMARY_DETAIL).offset,
                                    primFlags, BM_NORMAL,
                                    mod.texture, mod.color));
            }
        }
    }
    else
    {
        if (p.skyMasked)
        {
            Store &buffer = ClientApp::render().buffer();
            uint32_t base = buffer.allocateVertices(numVerts);
            DrawList::reserveSpace(indices, numVerts);
            for (uint32_t i = 0; i < numVerts; ++i)
            {
                indices[i] = base + i;
                buffer.posCoords[indices[i]] = verts.pos[i];
            }
            ClientApp::render()
                .drawLists().find(DrawListSpec(SkyMaskGeom))
                    .write(buffer, indices.data(), numVerts, p.isWall?  gfx::TriangleStrip :  gfx::TriangleFan);
        }
        else
        {
            DrawListSpec listSpec((mod.texture || hasDynlights) ? LitGeom : UnlitGeom);
            if (layer0RTU)
            {
                listSpec.texunits[TU_PRIMARY] = *layer0RTU;
                if (p.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY].offset += *p.materialOrigin;
                }
                if (p.materialScale)
                {
                    listSpec.texunits[TU_PRIMARY].scale  *= *p.materialScale;
                    listSpec.texunits[TU_PRIMARY].offset *= *p.materialScale;
                }
            }
            if (layer0InterRTU)
            {
                listSpec.texunits[TU_INTER] = *layer0InterRTU;
                if (p.materialOrigin)
                {
                    listSpec.texunits[TU_INTER].offset += *p.materialOrigin;
                }
                if (p.materialScale)
                {
                    listSpec.texunits[TU_INTER].scale  *= *p.materialScale;
                    listSpec.texunits[TU_INTER].offset *= *p.materialScale;
                }
            }
            if (detailRTU)
            {
                listSpec.texunits[TU_PRIMARY_DETAIL] = *detailRTU;
                if (p.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY_DETAIL].offset += *p.materialOrigin;
                }
            }
            if (detailInterRTU)
            {
                listSpec.texunits[TU_INTER_DETAIL] = *detailInterRTU;
                if (p.materialOrigin)
                {
                    listSpec.texunits[TU_INTER_DETAIL].offset += *p.materialOrigin;
                }
            }

            // Is the geometry lit?
            Flags primFlags;
            //bool oneLight   = false;
            //bool manyLights = false;
            if (mod.texture && !hasDynlights)
            {
                primFlags |= Parm::OneLight; // Using modulation.
            }
            else if (mod.texture || hasDynlights)
            {
                primFlags |= Parm::ManyLights; //manyLights = true;
            }

            Store &buffer = ClientApp::render().buffer();
            uint32_t base = buffer.allocateVertices(numVertices);
            DrawList::reserveSpace(indices, numVertices);
            static Vec4ub const white(255, 255, 255, 255);
            for (uint32_t i = 0; i < numVertices; ++i)
            {
                indices[i] = base + i;
                buffer.posCoords[indices[i]] = verts.pos[i];
                if (verts.color)
                {
                    buffer.colorCoords[indices[i]] = (verts.color[i] * 255).toVec4ub();
                }
                else
                {
                    buffer.colorCoords[indices[i]] = white;
                }
                if (verts.tex)
                {
                    buffer.texCoords[0][indices[i]] = verts.tex[i];
                }
                if (verts.tex2)
                {
                    buffer.texCoords[1][indices[i]] = verts.tex2[i];
                }
                if ((primFlags & (Parm::OneLight | Parm::ManyLights)) && Rend_IsMTexLights())
                {
                    DE_ASSERT(modTexCoords);
                    buffer.modCoords[indices[i]] = modTexCoords[i];
                }
            }
            ClientApp::render()
                .drawLists().find(listSpec)
                    .write(buffer, indices.data(), numVertices,
                           Parm(p.isWall?  gfx::TriangleStrip  :  gfx::TriangleFan,
                                listSpec.unit(TU_PRIMARY       ).scale,
                                listSpec.unit(TU_PRIMARY       ).offset,
                                listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                listSpec.unit(TU_PRIMARY_DETAIL).offset,
                                primFlags, BM_NORMAL,
                                mod.texture, mod.color));
        }
    }

    if (shineRTU)
    {
        // Make shine geometry.
        // Surface shine geometry (primary texture and color coords).
        // Use the surface geometry for position coords.
        // Use the surface texture coords with the mask.
        Geometry shineVerts = Geometry();
        {
            const Vec3f &shineColor = matAnimator.shineMinColor();  // Shine strength.
            const float shineOpacity  = shineRTU->opacity;

            // Allocate vertices from the pools.
            shineVerts.color = R_AllocRendColors(numVerts);
            shineVerts.tex   = R_AllocRendTexCoords(numVerts);

            if (p.isWall)
            {
                makeWallShineGeometry(shineVerts, numVertices, rvertices, verts, p.wall.width,
                                      shineColor, shineOpacity);
            }
            else
            {
                makeFlatShineGeometry(shineVerts, numVertices, rvertices, verts,
                                      shineColor, shineOpacity);
            }
        }

        // Write shine geometry.
        DrawListSpec listSpec(ShineGeom);
        listSpec.texunits[TU_PRIMARY] = *shineRTU;
        if (shineMaskRTU)
        {
            listSpec.texunits[TU_INTER] = *shineMaskRTU;
            if (p.materialOrigin)
            {
                listSpec.texunits[TU_INTER].offset += *p.materialOrigin;
            }
            if (p.materialScale)
            {
                listSpec.texunits[TU_INTER].scale  *= *p.materialScale;
                listSpec.texunits[TU_INTER].offset *= *p.materialScale;
            }
        }
        DrawList &shineList = ClientApp::render().drawLists().find(listSpec);

        Parm shineParams(gfx::TriangleFan,
                         listSpec.unit(TU_INTER).scale,
                         listSpec.unit(TU_INTER).offset,
                         Vec2f(1, 1),
                         Vec2f(0, 0),
                         Parm::Unlit,
                         matAnimator.shineBlendMode());

        // Walls with edge divisions mean two trifans.
        if (mustSubdivide)
        {
            DE_ASSERT(p.isWall);
            const uint32_t numLeftVerts  = 3 + p.wall.leftEdge ->divisionCount();
            const uint32_t numRightVerts = 3 + p.wall.rightEdge->divisionCount();

            {
                Vec2f orig[4]; std::memcpy(orig, shineVerts.tex, sizeof(orig));
                R_DivTexCoords(shineVerts.tex, orig, *p.wall.leftEdge, *p.wall.rightEdge);
            }
            {
                Vec4f orig[4]; std::memcpy(orig, shineVerts.color, sizeof(orig));
                R_DivVertColors(shineVerts.color, orig, *p.wall.leftEdge, *p.wall.rightEdge);
            }

            Store &buffer = ClientApp::render().buffer();
            {
                uint32_t base = buffer.allocateVertices(numRightVerts);
                DrawList::reserveSpace(indices, numRightVerts);
                for (uint32_t i = 0; i < numRightVerts; ++i)
                {
                    indices[i] = base + i;
                    buffer.posCoords   [indices[i]] = verts.pos[numLeftVerts + i];
                    buffer.colorCoords [indices[i]] = (shineVerts.color[numLeftVerts + i] * 255).toVec4ub();
                    buffer.texCoords[0][indices[i]] = shineVerts.tex[numLeftVerts + i];
                    if (shineMaskRTU)
                    {
                        buffer.texCoords[1][indices[i]] = verts.tex[numLeftVerts + i];
                    }
                }
                shineList.write(buffer, indices.data(), numRightVerts, shineParams);
            }
            {
                uint32_t base = buffer.allocateVertices(numLeftVerts);
                DrawList::reserveSpace(indices, numLeftVerts);
                for (uint32_t i = 0; i < numLeftVerts; ++i)
                {
                    indices[i] = base + i;
                    buffer.posCoords   [indices[i]] = verts.pos[i];
                    buffer.colorCoords [indices[i]] = (shineVerts.color[i] * 255).toVec4ub();
                    buffer.texCoords[0][indices[i]] = shineVerts.tex[i];
                    if (shineMaskRTU)
                    {
                        buffer.texCoords[1][indices[i]] = verts.tex[i];
                    }
                }
                shineList.write(buffer, indices.data(), numLeftVerts, shineParams);
            }
        }
        else
        {
            Store &buffer = ClientApp::render().buffer();
            uint32_t base = buffer.allocateVertices(numVertices);
            DrawList::reserveSpace(indices, numVertices);
            for (uint32_t i = 0; i < numVertices; ++i)
            {
                indices[i] = base + i;
                buffer.posCoords   [indices[i]] = verts.pos[i];
                buffer.colorCoords [indices[i]] = (shineVerts.color[i] * 255).toVec4ub();
                buffer.texCoords[0][indices[i]] = shineVerts.tex[i];
                if (shineMaskRTU)
                {
                    buffer.texCoords[1][indices[i]] = verts.tex[i];
                }
            }
            shineParams.type = p.isWall?  gfx::TriangleStrip :  gfx::TriangleFan;
            shineList.write(buffer, indices.data(), numVertices, shineParams);
        }

        // We're done with the shine geometry.
        R_FreeRendColors   (shineVerts.color);
        R_FreeRendTexCoords(shineVerts.tex);
    }

    // We're done with the geometry.
    R_FreeRendTexCoords(modTexCoords);

    R_FreeRendVertices (verts.pos);
    R_FreeRendColors   (verts.color);
    R_FreeRendTexCoords(verts.tex);
    R_FreeRendTexCoords(verts.tex2);

    return (p.forceOpaque || skyMaskedMaterial
            || !(p.alpha < 1 || !matAnimator.isOpaque() || p.blendMode > 0));
}

static Lumobj::LightmapSemantic lightmapForSurface(const Surface &surface)
{
    if (surface.parent().type() == DMU_SIDE) return Lumobj::Side;
    // Must be a plane then.
    const auto &plane = surface.parent().as<Plane>();
    return plane.isSectorFloor() ? Lumobj::Down : Lumobj::Up;
}

static DGLuint prepareLightmap(ClientTexture *tex = nullptr)
{
    if (tex)
    {
        if (TextureVariant *variant = tex->prepareVariant(Rend_MapSurfaceLightmapTextureSpec()))
        {
            return variant->glName();
        }
        // Dang...
    }
    // Prepare the default/fallback lightmap instead.
    return GL_PrepareLSTexture(LST_DYNAMIC);
}

static bool projectDynlight(const Vec3d &topLeft, const Vec3d &bottomRight,
    const Lumobj &lum, const Surface &surface, float blendFactor,
    ProjectedTextureData &projected)
{
    if (blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return false;

    // Has this already been occluded?
    if (R_ViewerLumobjIsHidden(lum.indexInMap()))
        return false;

    // No lightmap texture?
    DGLuint tex = prepareLightmap(lum.lightmap(lightmapForSurface(surface)));
    if (!tex) return false;

    Vec3d lumCenter = lum.origin();
    lumCenter.z += lum.zOffset();

    // On the right side?
    Vec3d topLeftToLum = topLeft - lumCenter;
    if (topLeftToLum.dot(surface.tangentMatrix().column(2)) > 0.f)
        return false;

    // Calculate 3D distance between surface and lumobj.
    Vec3d pointOnPlane = R_ClosestPointOnPlane(surface.tangentMatrix().column(2)/*normal*/,
                                                  topLeft, lumCenter);

    coord_t distToLum = (lumCenter - pointOnPlane).length();
    if (distToLum <= 0 || distToLum > lum.radius())
        return false;

    // Calculate the final surface light attribution factor.
    float luma = 1.5f - 1.5f * distToLum / lum.radius();

    // Fade out as distance from viewer increases.
    luma *= lum.attenuation(R_ViewerLumobjDistance(lum.indexInMap()));

    // Would this be seen?
    if (luma * blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return false;

    // Project, counteracting aspect correction slightly.
    Vec2f s, t;
    const float scale = 1.0f / ((2.f * lum.radius()) - distToLum);
    if (!R_GenerateTexCoords(s, t, pointOnPlane, scale, scale * 1.08f,
                            topLeft, bottomRight, surface.tangentMatrix()))
        return false;

    projected = {};
    projected.texture     = tex;
    projected.topLeft     = Vec2f(s[0], t[0]);
    projected.bottomRight = Vec2f(s[1], t[1]);
    projected.color       = Vec4f(Rend_LuminousColor(lum.color(), luma), blendFactor);

    return true;
}

static bool projectPlaneGlow(const Vec3d &topLeft, const Vec3d &bottomRight,
    const Plane &plane, const Vec3d &pointOnPlane, float blendFactor,
    ProjectedTextureData &projected)
{
    if (blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return false;

    Vec3f color;
    float intensity = plane.surface().glow(color);

    // Is the material glowing at this moment?
    if (intensity < .05f)
        return false;

    coord_t glowHeight = Rend_PlaneGlowHeight(intensity);
    if (glowHeight < 2) return false;  // Not too small!

    // Calculate coords.
    float bottom, top;
    if (plane.surface().normal().z < 0)
    {
        // Cast downward.
              bottom = (pointOnPlane.z - topLeft.z) / glowHeight;
        top = bottom + (topLeft.z - bottomRight.z) / glowHeight;
    }
    else
    {
        // Cast upward.
                 top = (bottomRight.z - pointOnPlane.z) / glowHeight;
        bottom = top + (topLeft.z - bottomRight.z) / glowHeight;
    }

    // Within range on the Z axis?
    if (!(bottom <= 1 || top >= 0)) return false;

    de::zap(projected);
    projected.texture     = GL_PrepareLSTexture(LST_GRADIENT);
    projected.topLeft     = Vec2f(0, bottom);
    projected.bottomRight = Vec2f(1, top);
    projected.color       = Vec4f(Rend_LuminousColor(color, intensity), blendFactor);
    return true;
}

static bool projectShadow(const Vec3d &topLeft, const Vec3d &bottomRight,
    const mobj_t &mob, const Surface &surface, float blendFactor,
    ProjectedTextureData &projected)
{
    static Vec3f const black;  // shadows are black

    coord_t mobOrigin[3];
    Mobj_OriginSmoothed(const_cast<mobj_t *>(&mob), mobOrigin);

    // Is this too far?
    coord_t distanceFromViewer = 0;
    if (shadowMaxDistance > 0)
    {
        distanceFromViewer = Rend_PointDist2D(mobOrigin);
        if (distanceFromViewer > shadowMaxDistance)
            return false;
    }

    float shadowStrength = Mobj_ShadowStrength(mob) * ::shadowFactor;
    if (fogParams.usingFog) shadowStrength /= 2;
    if (shadowStrength <= 0) return false;

    coord_t shadowRadius = Mobj_ShadowRadius(mob);
    if (shadowRadius > ::shadowMaxRadius)
        shadowRadius = ::shadowMaxRadius;
    if (shadowRadius <= 0) return false;

    mobOrigin[2] -= mob.floorClip;
    if (mob.ddFlags & DDMF_BOB)
        mobOrigin[2] -= Mobj_BobOffset(mob);

    coord_t mobHeight = mob.height;
    if (!mobHeight) mobHeight = 1;

    // If this were a light this is where we would check whether the origin is on
    // the right side of the surface. However this is a shadow and light is moving
    // in the opposite direction (inward toward the mobj's origin), therefore this
    // has "volume/depth".

    // Calculate 3D distance between surface and mobj.
    Vec3d point = R_ClosestPointOnPlane(surface.tangentMatrix().column(2)/*normal*/,
                                        topLeft, Vec3d(mobOrigin));
    coord_t distFromSurface = (Vec3d(mobOrigin) - Vec3d(point)).length();

    // Too far above or below the shadowed surface?
    if (distFromSurface > mob.height)
        return false;
    if (mobOrigin[2] + mob.height < point.z)
        return false;
    if (distFromSurface > shadowRadius)
        return false;

    // Calculate the final strength of the shadow's attribution to the surface.
    shadowStrength *= 1.5f - 1.5f * distFromSurface / shadowRadius;

    // Fade at half mobj height for smooth fade out when embedded in the surface.
    coord_t halfMobjHeight = mobHeight / 2;
    if (distFromSurface > halfMobjHeight)
    {
        shadowStrength *= 1 - (distFromSurface - halfMobjHeight) / (mobHeight - halfMobjHeight);
    }

    // Fade when nearing the maximum distance?
    shadowStrength *= Rend_ShadowAttenuationFactor(distanceFromViewer);
    shadowStrength *= blendFactor;

    // Would this shadow be seen?
    if (shadowStrength < SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return false;

    // Project, counteracting aspect correction slightly.
    Vec2f s, t;
    const float scale = 1.0f / ((2.f * shadowRadius) - distFromSurface);
    if (!R_GenerateTexCoords(s, t, point, scale, scale * 1.08f,
                            topLeft, bottomRight, surface.tangentMatrix()))
        return false;

    de::zap(projected);
    projected.texture     = GL_PrepareLSTexture(LST_DYNAMIC);
    projected.topLeft     = Vec2f(s[0], t[0]);
    projected.bottomRight = Vec2f(s[1], t[1]);
    projected.color       = Vec4f(black, shadowStrength);
    return true;
}

/**
 * @pre The coordinates of the given quad must be contained wholly within the subspoce
 * specified. This is due to an optimization within the lumobj management which separates
 * them by subspace.
 */
static void projectDynamics(const Surface &surface, float glowStrength,
    const Vec3d &topLeft, const Vec3d &bottomRight,
    bool noLights, bool noShadows, bool sortLights,
    uint32_t &lightListIdx, uint32_t &shadowListIdx)
{
    DE_ASSERT(curSubspace);

    if (levelFullBright) return;
    if (glowStrength >= 1) return;

    // lights?
    if (!noLights)
    {
        const float blendFactor = 1;

        if (::useDynLights)
        {
            // Project all lumobjs affecting the given quad (world space), calculate
            // coordinates (in texture space) then store into a new list of projections.
            R_ForAllSubspaceLumContacts(*curSubspace, [&topLeft, &bottomRight, &surface
                                                      , &blendFactor, &sortLights
                                                      , &lightListIdx] (Lumobj &lum)
            {
                ProjectedTextureData projected;
                if (projectDynlight(topLeft, bottomRight, lum, surface, blendFactor,
                                   projected))
                {
                    ClientApp::render().findSurfaceProjectionList(&lightListIdx, sortLights)
                                << projected;  // a copy is made.
                }
                return LoopContinue;
            });
        }

        if (::useGlowOnWalls && surface.parent().type() == DMU_SIDE && bottomRight.z < topLeft.z)
        {
            // Project all plane glows affecting the given quad (world space), calculate
            // coordinates (in texture space) then store into a new list of projections.
            const auto &subsec = curSubspace->subsector().as<Subsector>();
            for (int i = 0; i < subsec.visPlaneCount(); ++i)
            {
                const Plane &plane = subsec.visPlane(i);
                Vec3d const pointOnPlane(subsec.center(), plane.heightSmoothed());

                ProjectedTextureData projected;
                if (projectPlaneGlow(topLeft, bottomRight, plane, pointOnPlane, blendFactor,
                                    projected))
                {
                    ClientApp::render().findSurfaceProjectionList(&lightListIdx, sortLights)
                                << projected;  // a copy is made.
                }
            }
        }
    }

    // Shadows?
    if (!noShadows && ::useShadows)
    {
        // Glow inversely diminishes shadow strength.
        float blendFactor = 1 - glowStrength;
        if (blendFactor >= SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        {
            blendFactor = de::clamp(0.f, blendFactor, 1.f);

            // Project all mobj shadows affecting the given quad (world space), calculate
            // coordinates (in texture space) then store into a new list of projections.
            R_ForAllSubspaceMobContacts(*curSubspace, [&topLeft, &bottomRight, &surface
                                                      , &blendFactor, &shadowListIdx] (mobj_t &mob)
            {
                ProjectedTextureData projected;
                if (projectShadow(topLeft, bottomRight, mob, surface, blendFactor,
                                 projected))
                {
                    ClientApp::render().findSurfaceProjectionList(&shadowListIdx)
                                << projected;  // a copy is made.
                }
                return LoopContinue;
            });
        }
    }
}

/**
 * World light can both light and shade. Normal objects get more shade than light
 * (preventing them from ending up too bright compared to the ambient light).
 */
static bool lightWithWorldLight(const Vec3d & /*point*/, const Vec3f &ambientColor,
    bool starkLight, VectorLightData &vlight)
{
    static Vec3f const worldLight(-.400891f, -.200445f, .601336f);

    vlight = {};
    vlight.direction         = worldLight;
    vlight.color             = ambientColor;
    vlight.affectedByAmbient = false;
    vlight.approxDist        = 0;
    if (starkLight)
    {
        vlight.lightSide = .35f;
        vlight.darkSide  = .5f;
        vlight.offset    = 0;
    }
    else
    {
        vlight.lightSide = .2f;
        vlight.darkSide  = .8f;
        vlight.offset    = .3f;
    }
    vlight.sourceMobj = nullptr;
    return true;
}

static bool lightWithLumobj(const Vec3d &point, const Lumobj &lum, VectorLightData &vlight)
{
    Vec3d const lumCenter(lum.x(), lum.y(), lum.z() + lum.zOffset());

    // Is this light close enough?
    const double dist = M_ApproxDistance(M_ApproxDistance(lumCenter.x - point.x,
                                                           lumCenter.y - point.y),
                                          point.z - lumCenter.z);
    float intensity = 0;
    if (dist < Lumobj::radiusMax())
    {
        intensity = de::clamp(0.f, float(1 - dist / lum.radius()) * 2, 1.f);
    }
    if (intensity < .05f) return false;

    vlight = {};
    vlight.direction         = (lumCenter - point) / dist;
    vlight.color             = lum.color() * intensity;
    vlight.affectedByAmbient = true;
    vlight.approxDist        = dist;
    vlight.lightSide         = 1;
    vlight.darkSide          = 0;
    vlight.offset            = 0;
    vlight.sourceMobj        = lum.sourceMobj();
    return true;
}

static bool lightWithPlaneGlow(const Vec3d &point, const Subsector &subsec,
    int visPlaneIndex, VectorLightData &vlight)
{
    const Plane &plane     = subsec.as<Subsector>().visPlane(visPlaneIndex);
    const Surface &surface = plane.surface();

    // Glowing at this moment?
    Vec3f glowColor;
    float intensity = surface.glow(glowColor);
    if (intensity < .05f) return false;

    const coord_t glowHeight = Rend_PlaneGlowHeight(intensity);
    if (glowHeight < 2) return false;  // Not too small!

    // In front of the plane?
    Vec3d const pointOnPlane(subsec.center(), plane.heightSmoothed());
    const double dist = (point - pointOnPlane).dot(surface.normal());
    if (dist < 0) return false;

    intensity *= 1 - dist / glowHeight;
    if (intensity < .05f) return false;

    const Vec3f color = Rend_LuminousColor(glowColor, intensity);
    if (color == Vec3f()) return false;

    vlight = {};
    vlight.direction         = Vec3f(surface.normal().x, surface.normal().y, -surface.normal().z);
    vlight.color             = color;
    vlight.affectedByAmbient = true;
    vlight.approxDist        = dist;
    vlight.lightSide         = 1;
    vlight.darkSide          = 0;
    vlight.offset            = 0.3f;
    vlight.sourceMobj        = nullptr;
    return true;
}

uint32_t Rend_CollectAffectingLights(const Vec3d &point, const Vec3f &ambientColor,
    world::ConvexSubspace *subspace, bool starkLight)
{
    uint32_t lightListIdx = 0;

    // Always apply an ambient world light.
    {
        VectorLightData vlight;
        if (lightWithWorldLight(point, ambientColor, starkLight, vlight))
        {
            ClientApp::render().findVectorLightList(&lightListIdx)
                    << vlight;  // a copy is made.
        }
    }

    // Add extra light by interpreting nearby sources.
    if (subspace)
    {
        // Interpret lighting from luminous-objects near the origin and which
        // are in contact the specified subspace and add them to the identified list.
        R_ForAllSubspaceLumContacts(subspace->as<ConvexSubspace>(), [&point, &lightListIdx] (Lumobj &lum)
        {
            VectorLightData vlight;
            if (lightWithLumobj(point, lum, vlight))
            {
                ClientApp::render().findVectorLightList(&lightListIdx)
                        << vlight;  // a copy is made.
            }
            return LoopContinue;
        });

        // Interpret vlights from glowing planes at the origin in the specfified
        // subspace and add them to the identified list.
        const auto &subsec = subspace->subsector().as<Subsector>();
        for (int i = 0; i < subsec.sector().planeCount(); ++i)
        {
            VectorLightData vlight;
            if (lightWithPlaneGlow(point, subsec, i, vlight))
            {
                ClientApp::render().findVectorLightList(&lightListIdx)
                        << vlight;  // a copy is made.
            }
        }
    }

    return lightListIdx;
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
static bool applyNearFadeOpacity(const WallEdge &leftEdge, const WallEdge &rightEdge,
    float &opacity)
{
    if (!leftEdge.spec().flags.testFlag(WallSpec::NearFade))
        return false;

    if (Rend_EyeOrigin().y < leftEdge.bottom().z() || Rend_EyeOrigin().y > rightEdge.top().z())
        return false;

    const mobj_t *mo         = viewPlayer->publicData().mo;
    const Line &line         = leftEdge.lineSide().line().as<Line>();

    coord_t linePoint[2]     = { line.from().x(), line.from().y() };
    coord_t lineDirection[2] = {  line.direction().x,  line.direction().y };
    vec2d_t result;
    double pos = V2d_ProjectOnLine(result, mo->origin, linePoint, lineDirection);

    if (!(pos > 0 && pos < 1))
        return false;

    const coord_t maxDistance = Mobj_Radius(*mo) * .8f;

    auto delta       = Vec2d(result) - Vec2d(mo->origin);
    coord_t distance = delta.length();

    if (de::abs(distance) > maxDistance)
        return false;

    if (distance > 0)
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
static float wallLuminosityDeltaFromNormal(const Vec3f &normal)
{
    return (1.0f / 255) * (normal.x * 18) * ::rendLightWallAngle;
}

static void wallLuminosityDeltas(const WallEdge &leftEdge, const WallEdge &rightEdge,
    float luminosityDeltas[2])
{
    float &leftDelta  = luminosityDeltas[0];
    float &rightDelta = luminosityDeltas[1];

    if (leftEdge.spec().flags.testFlag(WallSpec::NoLightDeltas))
    {
        leftDelta = rightDelta = 0;
        return;
    }

    leftDelta = wallLuminosityDeltaFromNormal(leftEdge.normal());

    if (leftEdge.normal() == rightEdge.normal())
    {
        rightDelta = leftDelta;
    }
    else
    {
        rightDelta = wallLuminosityDeltaFromNormal(rightEdge.normal());

        // Linearly interpolate to find the light level delta values for the
        // vertical edges of this wall section.
        const coord_t lineLength    = leftEdge.lineSide().line().length();
        const coord_t sectionOffset = leftEdge.lineSideOffset();
        const coord_t sectionWidth  = de::abs(Vec2d(rightEdge.origin() - leftEdge.origin()).length());

        float deltaDiff = rightDelta - leftDelta;
        rightDelta = leftDelta + ((sectionOffset + sectionWidth) / lineLength) * deltaDiff;
        leftDelta += (sectionOffset / lineLength) * deltaDiff;
    }
}

static void writeWall(const WallEdge &leftEdge, const WallEdge &rightEdge,
    bool *retWroteOpaque = nullptr, coord_t *retBottomZ = nullptr, coord_t *retTopZ = nullptr)
{
    DE_ASSERT(leftEdge.lineSideSegment().isFrontFacing() && leftEdge.lineSide().hasSections());

    if (retWroteOpaque) *retWroteOpaque = false;
    if (retBottomZ)     *retBottomZ     = 0;
    if (retTopZ)        *retTopZ        = 0;

    auto &subsec = curSubspace->subsector().as<Subsector>();
    Surface &surface = leftEdge.lineSide().surface(leftEdge.spec().section).as<Surface>();

    // Skip nearly transparent surfaces.
    float opacity = surface.opacity();
    if (opacity < .001f)
        return;

    // Determine which Material to use (a drawable material is required).
    ClientMaterial *material = Rend_ChooseMapSurfaceMaterial(surface);
    if (!material || !material->isDrawable())
        return;

    // Do the edge geometries describe a valid polygon?
    if (!leftEdge.isValid() || !rightEdge.isValid()
        || de::fequal(leftEdge.bottom().z(), rightEdge.top().z()))
        return;

    const WallSpec &wallSpec      = leftEdge.spec();
    const bool didNearFade        = applyNearFadeOpacity(leftEdge, rightEdge, opacity);
    const bool skyMasked          = material->isSkyMasked() && !::devRendSkyMode;
    const bool twoSidedMiddle     = (wallSpec.section == LineSide::Middle && !leftEdge.lineSide().considerOneSided());

    MaterialAnimator &matAnimator = material->getAnimator(Rend_MapSurfaceMaterialSpec());
    const Vec2f materialScale  = surface.materialScale();
    const Vec3f materialOrigin = leftEdge.materialOrigin();
    const Vec3d topLeft        = leftEdge .top   ().origin();
    const Vec3d bottomRight    = rightEdge.bottom().origin();

    rendworldpoly_params_t parm; de::zap(parm);
    parm.skyMasked            = skyMasked;
    parm.mapElement           = &leftEdge.lineSideSegment();
    parm.geomGroup            = wallSpec.section;
    parm.topLeft              = &topLeft;
    parm.bottomRight          = &bottomRight;
    parm.forceOpaque          = wallSpec.flags.testFlag(WallSpec::ForceOpaque);
    parm.alpha                = parm.forceOpaque? 1 : opacity;
    parm.surfaceTangentMatrix = &surface.tangentMatrix();
    parm.blendMode            = BM_NORMAL;
    parm.materialOrigin       = &materialOrigin;
    parm.materialScale        = &materialScale;

    parm.isWall               = true;
    parm.wall.width           = de::abs(Vec2d(rightEdge.origin() - leftEdge.origin()).length());
    parm.wall.leftEdge        = &leftEdge;
    parm.wall.rightEdge       = &rightEdge;
    // Calculate the angle-based luminosity deltas.
    wallLuminosityDeltas(leftEdge, rightEdge, parm.surfaceLuminosityDeltas);

    LineSide &side = leftEdge.lineSide();
    if (!parm.skyMasked)
    {
        if (glowFactor > .0001f)
        {
//            if (material == surface.materialPtr())
//            {
                parm.glowing = matAnimator.glowStrength();
//            }
//            else
//            {
//                auto *actualMaterial =
//                    surface.hasMaterial() ? static_cast<ClientMaterial *>(surface.materialPtr())
//                                          : &ClientMaterial::find(de::Uri("System", Path("missing")));

//                parm.glowing = actualMaterial->getAnimator(Rend_MapSurfaceMaterialSpec()).glowStrength();
//            }

            parm.glowing *= glowFactor;
        }

        projectDynamics(surface, parm.glowing, *parm.topLeft, *parm.bottomRight,
                        wallSpec.flags.testFlag(WallSpec::NoDynLights),
                        wallSpec.flags.testFlag(WallSpec::NoDynShadows),
                        wallSpec.flags.testFlag(WallSpec::SortDynLights),
                        parm.lightListIdx, parm.shadowListIdx);

        if (twoSidedMiddle)
        {
            parm.blendMode = surface.blendMode();
            if (parm.blendMode == BM_NORMAL && noSpriteTrans)
                parm.blendMode = BM_ZEROALPHA;  // "no translucency" mode
        }

        side.chooseSurfaceColors(wallSpec.section, &parm.surfaceColor, &parm.wall.surfaceColor2);
    }

    //
    // Geometry write/drawing begins.
    //

    if (twoSidedMiddle && side.sectorPtr() != &subsec.sector())
    {
        // Temporarily modify the draw state.
        curSectorLightColor = Rend_AmbientLightColor(side.sector());
        curSectorLightLevel = side.sector().lightLevel();
    }

    Vec3f const posCoords[] = {
        leftEdge .bottom().origin(),
        leftEdge .top   ().origin(),
        rightEdge.bottom().origin(),
        rightEdge.top   ().origin()
    };

    // Draw this wall.
    const bool wroteOpaque = renderWorldPoly(posCoords, 4, parm, matAnimator);

    // Draw FakeRadio for this wall?
    if (wroteOpaque && !skyMasked && !(parm.glowing > 0))
    {
        Rend_DrawWallRadio(leftEdge, rightEdge, ::curSectorLightLevel);
    }

    if (twoSidedMiddle && side.sectorPtr() != &subsec.sector())
    {
        // Undo temporary draw state changes.
        const Vec4f color = subsec.lightSourceColorfIntensity();
        curSectorLightColor = color.toVec3f();
        curSectorLightLevel = color.w;
    }

    if (retWroteOpaque) *retWroteOpaque = wroteOpaque && !didNearFade;
    if (retBottomZ)     *retBottomZ     = leftEdge .bottom().z();
    if (retTopZ)        *retTopZ        = rightEdge.top   ().z();
}

/**
 * Prepare a trifan geometry according to the edges of the current subspace.
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
static uint32_t buildSubspacePlaneGeometry(ClockDirection direction, coord_t height,
    Vec3f **verts)
{
    DE_ASSERT(verts);

    const auto &poly = curSubspace->poly();
    auto *fanBase    = curSubspace->fanBase();
    const uint32_t totalVerts = poly.hedgeCount() + (!fanBase? 2 : 0);

    *verts = R_AllocRendVertices(totalVerts);

    uint32_t n = 0;
    if (!fanBase)
    {
        (*verts)[n] = Vec3f(poly.center(), height);
        n++;
    }

    // Add the vertices for each hedge.
    auto *baseNode = fanBase? fanBase : poly.hedge();
    auto *node = baseNode;
    do
    {
        (*verts)[n] = Vec3f(node->origin(), height);
        n++;
    } while ((node = &node->neighbor(direction)) != baseNode);

    // The last vertex is always equal to the first.
    if (!fanBase)
    {
        (*verts)[n] = Vec3f(poly.hedge()->origin(), height);
    }

    return totalVerts;
}

static void writeSubspacePlane(Plane &plane)
{
    const mesh::Face &poly = curSubspace->poly();
    const Surface &surface = plane.surface();

    // Skip nearly transparent surfaces.
    const float opacity = surface.opacity();
    if (opacity < .001f) return;

    // Determine which Material to use (a drawable material is required).
    ClientMaterial *material = Rend_ChooseMapSurfaceMaterial(surface);
    if (!material || !material->isDrawable())
        return;

    // Skip planes with a sky-masked material (drawn with the mask geometry)?
    if (!::devRendSkyMode && surface.hasSkyMaskedMaterial() && plane.indexInSector() <= world::Sector::Ceiling)
        return;

    MaterialAnimator &matAnimator = material->getAnimator(Rend_MapSurfaceMaterialSpec());

    Vec2f materialOrigin = curSubspace->worldGridOffset() // Align to the worldwide grid.
                            + surface.originSmoothed();
    // Add the Y offset to orient the Y flipped material.
    /// @todo fixme: What is this meant to do? -ds
    if (plane.isSectorCeiling())
    {
        materialOrigin.y -= poly.bounds().maxY - poly.bounds().minY;
    }
    materialOrigin.y = -materialOrigin.y;

    const Vec2f materialScale = surface.materialScale();

    // Set the texture origin, Y is flipped for the ceiling.
    Vec3d topLeft(poly.bounds().minX,
                     poly.bounds().arvec2[plane.isSectorFloor()? 1 : 0][1],
                     plane.heightSmoothed());
    Vec3d bottomRight(poly.bounds().maxX,
                         poly.bounds().arvec2[plane.isSectorFloor()? 0 : 1][1],
                         plane.heightSmoothed());

    rendworldpoly_params_t parm; de::zap(parm);
    parm.mapElement           = curSubspace;
    parm.geomGroup            = plane.indexInSector();
    parm.topLeft              = &topLeft;
    parm.bottomRight          = &bottomRight;
    parm.materialOrigin       = &materialOrigin;
    parm.materialScale        = &materialScale;
    parm.surfaceColor         = &surface.color();
    parm.surfaceTangentMatrix = &surface.tangentMatrix();

    if (material->isSkyMasked())
    {
        // In devRendSkyMode mode we render all polys destined for the
        // skymask as regular world polys (with a few obvious properties).
        if (devRendSkyMode)
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
    else if (plane.indexInSector() <= world::Sector::Ceiling)
    {
        parm.blendMode   = BM_NORMAL;
        parm.forceOpaque = true;
    }
    else
    {
        parm.blendMode = surface.blendMode();
        if (parm.blendMode == BM_NORMAL && noSpriteTrans)
        {
            parm.blendMode = BM_ZEROALPHA;  // "no translucency" mode
        }

        parm.alpha = surface.opacity();
    }

    if (!parm.skyMasked)
    {
        if (glowFactor > .0001f)
        {
            if (material == surface.materialPtr())
            {
                parm.glowing = matAnimator.glowStrength();
            }
            else
            {
                world::Material *actualMaterial =
                    surface.hasMaterial()? surface.materialPtr()
                                         : &world::Materials::get().material(res::Uri("System", Path("missing")));

                parm.glowing = actualMaterial->as<ClientMaterial>().getAnimator(Rend_MapSurfaceMaterialSpec()).glowStrength();
            }

            parm.glowing *= ::glowFactor;
        }

        projectDynamics(surface, parm.glowing, *parm.topLeft, *parm.bottomRight,
                        false /*do light*/, false /*do shadow*/, false /*don't sort*/,
                        parm.lightListIdx, parm.shadowListIdx);
    }

    //
    // Geometry write/drawing begins.
    //

    if (&plane.sector() != &curSubspace->subsector().sector())
    {
        // Temporarily modify the draw state.
        curSectorLightColor = Rend_AmbientLightColor(plane.sector());
        curSectorLightLevel = plane.sector().lightLevel();
    }

    // Allocate position coordinates.
    Vec3f *posCoords;
    uint32_t vertCount = buildSubspacePlaneGeometry((plane.isSectorCeiling())? CounterClockwise : Clockwise,
                                                 plane.heightSmoothed(), &posCoords);

    // Draw this section.
    renderWorldPoly(posCoords, vertCount, parm, matAnimator);

    if (&plane.sector() != &curSubspace->subsector().sector())
    {
        // Undo temporary draw state changes.
        const Vec4f color = curSubspace->subsector().as<Subsector>().lightSourceColorfIntensity();
        curSectorLightColor = color.toVec3f();
        curSectorLightLevel = color.w;
    }

    R_FreeRendVertices(posCoords);
}

static void writeSkyMaskStrip(int vertCount, const Vec3f *posCoords, const Vec2f *texCoords,
    world::Material *material)
{
    DE_ASSERT(posCoords);

    static DrawList::Indices indices;

    if (!devRendSkyMode)
    {
        Store &buffer = ClientApp::render().buffer();
        uint32_t base = buffer.allocateVertices(vertCount);
        DrawList::reserveSpace(indices, vertCount);
        for (int i = 0; i < vertCount; ++i)
        {
            indices[i] = base + i;
            buffer.posCoords[indices[i]] = posCoords[i];
        }
        ClientApp::render().drawLists().find(DrawListSpec(SkyMaskGeom))
                      .write(buffer, indices.data(), vertCount, gfx::TriangleStrip);
    }
    else
    {
        DE_ASSERT(texCoords);

        DrawListSpec listSpec;
        listSpec.group = UnlitGeom;
        if (renderTextures != 2)
        {
            DE_ASSERT(material);
            MaterialAnimator &matAnimator = material->as<ClientMaterial>().getAnimator(Rend_MapSurfaceMaterialSpec());

            // Ensure we've up to date info about the material.
            matAnimator.prepare();

            // Map RTU configuration from the sky surface material.
            listSpec.texunits[TU_PRIMARY]        = matAnimator.texUnit(MaterialAnimator::TU_LAYER0);
            listSpec.texunits[TU_PRIMARY_DETAIL] = matAnimator.texUnit(MaterialAnimator::TU_DETAIL);
            listSpec.texunits[TU_INTER]          = matAnimator.texUnit(MaterialAnimator::TU_LAYER0_INTER);
            listSpec.texunits[TU_INTER_DETAIL]   = matAnimator.texUnit(MaterialAnimator::TU_DETAIL_INTER);
        }

        Store &buffer = ClientApp::render().buffer();
        uint32_t base = buffer.allocateVertices(vertCount);
        DrawList::reserveSpace(indices, vertCount);
        for (int i = 0; i < vertCount; ++i)
        {
            indices[i] = base + i;
            buffer.posCoords   [indices[i]] = posCoords[i];
            buffer.texCoords[0][indices[i]] = texCoords[i];
            buffer.colorCoords [indices[i]] = Vec4ub(255, 255, 255, 255);
        }

        ClientApp::render().drawLists().find(listSpec)
                      .write(buffer, indices.data(), vertCount,
                             DrawList::PrimitiveParams(gfx::TriangleStrip,
                                                       listSpec.unit(TU_PRIMARY       ).scale,
                                                       listSpec.unit(TU_PRIMARY       ).offset,
                                                       listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                                       listSpec.unit(TU_PRIMARY_DETAIL).offset));
    }
}

static void writeSubspaceSkyMaskStrips(SkyFixEdge::FixType fixType)
{
    // Determine strip generation behavior.
    const ClockDirection direction   = Clockwise;
    const bool splitOnMaterialChange = (::devRendSkyMode && ::renderTextures != 2);

    // Determine the relative sky plane (for monitoring material changes).
    const int relPlane = (fixType == SkyFixEdge::Upper ? world::Sector::Ceiling : world::Sector::Floor);

    // Configure the strip builder wrt vertex attributes.
    TriangleStripBuilder stripBuilder(CPP_BOOL(::devRendSkyMode));

    // Configure the strip build state (we'll most likely need to break edge loop
    // into multiple strips).
    world::Material *scanMaterial       = nullptr;
    float            scanMaterialOffset = 0;
    mesh::HEdge *    scanNode           = nullptr;
    coord_t          scanZBottom        = 0;
    coord_t          scanZTop           = 0;

    // Begin generating geometry.
    auto *base  = curSubspace->poly().hedge();
    auto *hedge = base;
    for (;;)
    {
        // Are we monitoring material changes?
        world::Material *skyMaterial = nullptr;
        if (splitOnMaterialChange)
        {
            skyMaterial = hedge->face().mapElementAs<ConvexSubspace>()
                              .subsector().as<Subsector>()
                              .visPlane(relPlane).surface().materialPtr();
        }

        // Add a first (left) edge to the current strip?
        if (!scanNode && hedge->hasMapElement())
        {
            scanMaterialOffset = hedge->mapElementAs<LineSideSegment>().lineSideOffset();

            // Prepare the edge geometry
            SkyFixEdge skyEdge(*hedge, fixType, (direction == CounterClockwise ? Line::To : Line::From),
                               scanMaterialOffset);

            if (skyEdge.isValid() && skyEdge.bottom().z() < skyEdge.top().z())
            {
                // A new strip begins.
                stripBuilder.begin(direction);
                stripBuilder << skyEdge;

                // Update the strip build state.
                scanNode     = hedge;
                scanZBottom  = skyEdge.bottom().z();
                scanZTop     = skyEdge.top   ().z();
                scanMaterial = skyMaterial;
            }
        }

        bool beginNewStrip = false;

        // Add the i'th (right) edge to the current strip?
        if (scanNode)
        {
            // Stop if we've reached a "null" edge.
            bool endStrip = false;
            if (hedge->hasMapElement())
            {
                scanMaterialOffset += hedge->mapElementAs<LineSideSegment>().length() *
                                      (direction == CounterClockwise ? -1 : 1);

                // Prepare the edge geometry
                SkyFixEdge skyEdge(*hedge, fixType, (direction == CounterClockwise)? Line::From : Line::To,
                                   scanMaterialOffset);

                if (!(skyEdge.isValid() && skyEdge.bottom().z() < skyEdge.top().z()))
                {
                    endStrip = true;
                }
                // Must we split the strip here?
                else if (hedge != scanNode &&
                        (   !de::fequal(skyEdge.bottom().z(), scanZBottom)
                         || !de::fequal(skyEdge.top   ().z(), scanZTop)
                         || (splitOnMaterialChange && skyMaterial != scanMaterial)))
                {
                    endStrip      = true;
                    beginNewStrip = true;  // We'll continue from here.
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

            if (endStrip || &hedge->neighbor(direction) == base)
            {
                // End the current strip.
                scanNode = nullptr;

                // Take ownership of the built geometry.
                PositionBuffer *positions = nullptr;
                TexCoordBuffer *texcoords = nullptr;
                const int numVerts = stripBuilder.take(&positions, &texcoords);

                // Write the strip geometry to the render lists.
                writeSkyMaskStrip(numVerts, positions->data(),
                                  (texcoords ? texcoords->data() : nullptr),
                                  scanMaterial);

                delete positions;
                delete texcoords;
            }
        }

        // Start a new strip from the current node?
        if (beginNewStrip) continue;

        // On to the next node.
        hedge = &hedge->neighbor(direction);

        // Are we done?
        if (hedge == base) break;
    }
}

/**
 * @defgroup skyCapFlags  Sky Cap Flags
 * @ingroup flags
 */
///@{
#define SKYCAP_LOWER        0x1
#define SKYCAP_UPPER        0x2
///@}

static uint makeFlatSkyMaskGeometry(DrawList::Indices &indices, Store &verts,  gfx::Primitive &primitive,
    const ConvexSubspace &subspace, coord_t worldZPosition = 0, ClockDirection direction = Clockwise)
{
    const auto &poly = subspace.poly();
    auto *fanBase   = subspace.fanBase();

    // Assign indices.
    const uint32_t vertCount = poly.hedgeCount() + (!fanBase? 2 : 0);
    const uint32_t base      = verts.allocateVertices(vertCount);
    DrawList::reserveSpace(indices, vertCount);
    for (uint32_t i = 0; i < vertCount; ++i)
    {
        indices[i] = base + i;
    }

    //
    // Build geometry.
    //
    primitive  = gfx::TriangleFan;
    uint32_t n = 0;
    if (!fanBase)
    {
        verts.posCoords[indices[n++]] = Vec3f(poly.center(), worldZPosition);
    }
    auto *baseNode = fanBase? fanBase : poly.hedge();
    auto *node = baseNode;
    do
    {
        verts.posCoords[indices[n++]] = Vec3f(node->origin(), worldZPosition);
    } while ((node = &node->neighbor(direction)) != baseNode);
    if (!fanBase)
    {
        verts.posCoords[indices[n  ]] = Vec3f(node->origin(), worldZPosition);
    }

    return vertCount;
}

/// @param skyCap  @ref skyCapFlags
static void writeSubspaceSkyMask(int skyCap = SKYCAP_LOWER | SKYCAP_UPPER)
{
    DE_ASSERT(::curSubspace);

    // No work to do?
    if (!skyCap) return;

    auto &subsec = curSubspace->subsector().as<Subsector>();
    Map &map = subsec.sector().map().as<Map>();

    DrawList &dlist = ClientApp::render().drawLists().find(DrawListSpec(SkyMaskGeom));
    static DrawList::Indices indices;

    // Lower?
    if ((skyCap & SKYCAP_LOWER) && subsec.hasSkyFloor())
    {
        ClSkyPlane &skyFloor = map.skyFloor();

        writeSubspaceSkyMaskStrips(SkyFixEdge::Lower);

        // Draw a cap? (handled as a regular plane in sky-debug mode).
        if (!::devRendSkyMode)
        {
            const double height =
                P_IsInVoid(::viewPlayer) ? subsec.visFloor().heightSmoothed() : skyFloor.height();

            // Make geometry.
            Store &verts = ClientApp::render().buffer();
             gfx::Primitive primitive;
            uint vertCount = makeFlatSkyMaskGeometry(indices, verts, primitive, *curSubspace, height, Clockwise);

            // Write geometry.
            dlist.write(verts, indices.data(), vertCount, primitive);
        }
    }

    // Upper?
    if ((skyCap & SKYCAP_UPPER) && subsec.hasSkyCeiling())
    {
        ClSkyPlane &skyCeiling = map.skyCeiling();

        writeSubspaceSkyMaskStrips(SkyFixEdge::Upper);

        // Draw a cap? (handled as a regular plane in sky-debug mode).
        if (!::devRendSkyMode)
        {
            const double height =
                P_IsInVoid(::viewPlayer) ? subsec.visCeiling().heightSmoothed() : skyCeiling.height();

            // Make geometry.
            Store &verts = ClientApp::render().buffer();
             gfx::Primitive primitive;
            uint vertCount = makeFlatSkyMaskGeometry(indices, verts, primitive, *curSubspace, height, CounterClockwise);

            // Write geometry.
            dlist.write(verts, indices.data(), vertCount, primitive);
        }
    }
}

static bool coveredOpenRange(mesh::HEdge &hedge, coord_t middleBottomZ, coord_t middleTopZ,
    bool wroteOpaqueMiddle)
{
    const LineSide &front = hedge.mapElementAs<LineSideSegment>().lineSide().as<LineSide>();

    // TNT map09 transparent window: blank line
    if (!front.hasAtLeastOneMaterial())
    {
        return false;
    }

    // TNT map02 window grille: transparent masked wall
    if (auto *anim = front.middle().as<Surface>().materialAnimator())
    {
        if (!anim->isOpaque())
        {
            return false;
        }
    }

    if (front.considerOneSided())
    {
        return wroteOpaqueMiddle;
    }

    /// @todo fixme: This additional test should not be necessary. For the obove
    /// test to fail and this to pass means that the geometry produced by the BSP
    /// builder is not correct (see: eternall.wad MAP10; note mapping errors).
    if (!hedge.twin().hasFace())
    {
        return wroteOpaqueMiddle;
    }

    const auto &subsec     = hedge.face().mapElementAs<ConvexSubspace>().subsector().as<Subsector>();
    const auto &backSubsec = hedge.twin().face().mapElementAs<ConvexSubspace>().subsector().as<Subsector>();

    const double ffloor   = subsec.visFloor().heightSmoothed();
    const double fceil    = subsec.visCeiling().heightSmoothed();

    const double bfloor   = backSubsec.visFloor().heightSmoothed();
    const double bceil    = backSubsec.visCeiling().heightSmoothed();

    bool middleCoversOpening = false;
    if (wroteOpaqueMiddle)
    {
        double xbottom = de::max(bfloor, ffloor);
        double xtop    = de::min(bceil,  fceil);

        xbottom += front.middle().as<Surface>().originSmoothed().y;
        xtop    += front.middle().as<Surface>().originSmoothed().y;

        middleCoversOpening = (middleTopZ >= xtop && middleBottomZ <= xbottom);
    }

    if (wroteOpaqueMiddle && middleCoversOpening)
        return true;

    if (  (bceil  <= ffloor && (front.top   ().hasMaterial() || front.middle().hasMaterial()))
       || (bfloor >= fceil  && (front.bottom().hasMaterial() || front.middle().hasMaterial())))
    {
        const Surface &ffloorSurface = subsec.visFloor  ().surface();
        const Surface &fceilSurface  = subsec.visCeiling().surface();
        const Surface &bfloorSurface = backSubsec.visFloor  ().surface();
        const Surface &bceilSurface  = backSubsec.visCeiling().surface();

        // A closed gap?
        if (de::fequal(fceil, bfloor))
        {
            return (bceil <= bfloor)
                   || !(   fceilSurface.hasSkyMaskedMaterial()
                        && bceilSurface.hasSkyMaskedMaterial());
        }

        if (de::fequal(ffloor, bceil))
        {
            return (bfloor >= bceil)
                   || !(   ffloorSurface.hasSkyMaskedMaterial()
                        && bfloorSurface.hasSkyMaskedMaterial());
        }

        return true;
    }

    /// @todo Is this still necessary?
    if (bceil <= bfloor
        || (!(bceil - bfloor > 0) && bfloor > ffloor && bceil < fceil
            && front.top().hasMaterial() && front.bottom().hasMaterial()))
    {
        // A zero height back segment
        return true;
    }

    return false;
}

static void writeAllWalls(mesh::HEdge &hedge)
{
    // Edges without a map line segment implicitly have no surfaces.
    if (!hedge.hasMapElement())
        return;

    // We are only interested in front facing segments with sections.
    auto &seg = hedge.mapElementAs<LineSideSegment>();
    if (!seg.isFrontFacing() || !seg.lineSide().hasSections())
        return;

    // Done here because of the logic of doom.exe wrt the automap.
    reportWallDrawn(seg.line());

    bool wroteOpaqueMiddle = false;
    coord_t middleBottomZ  = 0;
    coord_t middleTopZ     = 0;

    writeWall(WallEdge(WallSpec::fromMapSide(seg.lineSide().as<LineSide>(), LineSide::Bottom), hedge, Line::From),
              WallEdge(WallSpec::fromMapSide(seg.lineSide().as<LineSide>(), LineSide::Bottom), hedge, Line::To  ));
    writeWall(WallEdge(WallSpec::fromMapSide(seg.lineSide().as<LineSide>(), LineSide::Top),    hedge, Line::From),
              WallEdge(WallSpec::fromMapSide(seg.lineSide().as<LineSide>(), LineSide::Top),    hedge, Line::To  ));
    writeWall(WallEdge(WallSpec::fromMapSide(seg.lineSide().as<LineSide>(), LineSide::Middle), hedge, Line::From),
              WallEdge(WallSpec::fromMapSide(seg.lineSide().as<LineSide>(), LineSide::Middle), hedge, Line::To  ),
              &wroteOpaqueMiddle, &middleBottomZ, &middleTopZ);

    // We can occlude the angle range defined by the X|Y origins of the
    // line segment if the open range has been covered (when the viewer
    // is not in the void).
    if (!P_IsInVoid(viewPlayer) && coveredOpenRange(hedge, middleBottomZ, middleTopZ, wroteOpaqueMiddle))
    {
        if (hedge.hasMapElement())
        {
            // IssueID #2306: Black segments appear in the sky due to polyobj walls being marked
            // as occluding angle ranges. As a workaround, don't consider these walls occluding.
            if (hedge.mapElementAs<LineSideSegment>().line().definesPolyobj())
            {
                const Polyobj &poly = hedge.mapElementAs<LineSideSegment>().line().polyobj();
                if (poly.sector().ceiling().surface().hasSkyMaskedMaterial())
                {
                    return;
                }
            }
        }
        ClientApp::render().angleClipper()
            .addRangeFromViewRelPoints(hedge.origin(), hedge.twin().origin());
    }
}

static void writeSubspaceWalls()
{
    DE_ASSERT(::curSubspace);
    auto *base  = ::curSubspace->poly().hedge();
    DE_ASSERT(base);
    auto *hedge = base;
    do
    {
        writeAllWalls(*hedge);
    } while ((hedge = &hedge->next()) != base);

    ::curSubspace->forAllExtraMeshes([] (mesh::Mesh &mesh)
    {
        for (auto *hedge : mesh.hedges())
        {
            writeAllWalls(*hedge);
        }
        return LoopContinue;
    });

    ::curSubspace->forAllPolyobjs([] (Polyobj &pob)
    {
        for (auto *hedge : pob.mesh().hedges())
        {
            writeAllWalls(*hedge);
        }
        return LoopContinue;
    });
}

static void writeSubspaceFlats()
{
    DE_ASSERT(::curSubspace);
    auto &subsec = ::curSubspace->subsector().as<Subsector>();
    for (int i = 0; i < subsec.visPlaneCount(); ++i)
    {
        // Skip planes facing away from the viewer.
        Plane &plane = subsec.visPlane(i);
        Vec3d const pointOnPlane(subsec.center(), plane.heightSmoothed());
        if ((eyeOrigin - pointOnPlane).dot(plane.surface().normal()) < 0)
            continue;

        writeSubspacePlane(plane);
    }
}

static void markFrontFacingWalls(mesh::HEdge &hedge)
{
    if (!hedge.hasMapElement()) return;
    // Which way is the line segment facing?
    hedge.mapElementAs<LineSideSegment>()
              .setFrontFacing(viewFacingDot(hedge.origin(), hedge.twin().origin()) >= 0);
}

static void markSubspaceFrontFacingWalls()
{
    DE_ASSERT(::curSubspace);

    mesh::HEdge *base = ::curSubspace->poly().hedge();
    DE_ASSERT(base);
    mesh::HEdge *hedge = base;
    do
    {
        markFrontFacingWalls(*hedge);
    } while ((hedge = &hedge->next()) != base);

    ::curSubspace->forAllExtraMeshes([] (mesh::Mesh &mesh)
    {
        for (auto *hedge : mesh.hedges())
        {
            markFrontFacingWalls(*hedge);
        }
        return LoopContinue;
    });

    ::curSubspace->forAllPolyobjs([] (Polyobj &pob)
    {
        for (auto *hedge : pob.mesh().hedges())
        {
            markFrontFacingWalls(*hedge);
        }
        return LoopContinue;
    });
}

static inline bool canOccludeEdgeBetweenPlanes(const Plane &frontPlane, const Plane &backPlane)
{
    // Do not create an occlusion between two sky-masked planes.
    // Only because the open range does not account for the sky plane height? -ds
    return !(frontPlane.surface().hasSkyMaskedMaterial() && backPlane .surface().hasSkyMaskedMaterial());
}

/**
 * Add angle clipper occlusion ranges for the edges of the current subspace.
 */
static void occludeSubspace(bool frontFacing)
{
    if (devNoCulling) return;
    if (P_IsInVoid(viewPlayer)) return;

    AngleClipper &clipper = ClientApp::render().angleClipper();

    const auto &subsec = ::curSubspace->subsector().as<Subsector>();
    const auto *base   = ::curSubspace->poly().hedge();
    DE_ASSERT(base);
    const auto *hedge = base;
    do
    {
        // Edges without a line segment can never occlude.
        if (!hedge->hasMapElement())
            continue;

        auto &seg = hedge->mapElementAs<LineSideSegment>();

        // Only front-facing edges can occlude.
        if (frontFacing != seg.isFrontFacing())
            continue;

        // Edges without line segment surface sections can never occlude.
        if (!seg.lineSide().hasSections())
            continue;

        // Occlusions should only happen where two sectors meet.
        if (!hedge->hasTwin() || !hedge->twin().hasFace()
            || !hedge->twin().face().hasMapElement())
            continue;

        const auto &backSubspace = hedge->twin().face().mapElementAs<ConvexSubspace>();
        const auto &backSubsec   = backSubspace.subsector().as<Subsector>();

        // Determine the opening between plane heights at this edge.
        double openBottom = de::max(backSubsec.visFloor().heightSmoothed(),
                                     subsec    .visFloor().heightSmoothed());

        double openTop = de::min(backSubsec.visCeiling().heightSmoothed(),
                                  subsec    .visCeiling().heightSmoothed());

        // Choose start and end vertexes so that it's facing forward.
        const auto &from = frontFacing ? hedge->vertex() : hedge->twin().vertex();
        const auto &to   = frontFacing ? hedge->twin().vertex() : hedge->vertex();

        // Does the floor create an occlusion?
        if ( (   (openBottom > subsec    .visFloor().heightSmoothed() && Rend_EyeOrigin().y <= openBottom)
              || (openBottom > backSubsec.visFloor().heightSmoothed() && Rend_EyeOrigin().y >= openBottom))
            && canOccludeEdgeBetweenPlanes(subsec.visFloor(), backSubsec.visFloor()))
        {
            clipper.addViewRelOcclusion(from.origin(), to.origin(), openBottom, false);
        }

        // Does the ceiling create an occlusion?
        if ( (   (openTop < subsec    .visCeiling().heightSmoothed() && Rend_EyeOrigin().y >= openTop)
              || (openTop < backSubsec.visCeiling().heightSmoothed() && Rend_EyeOrigin().y <= openTop))
            && canOccludeEdgeBetweenPlanes(subsec.visCeiling(), backSubsec.visCeiling()))
        {
            clipper.addViewRelOcclusion(from.origin(), to.origin(), openTop, true);
        }
    } while ((hedge = &hedge->next()) != base);
}

static void clipSubspaceLumobjs()
{
    DE_ASSERT(::curSubspace);
    ::curSubspace->forAllLumobjs([] (Lumobj &lob)
    {
        R_ViewerClipLumobj(&lob);
        return LoopContinue;
    });
}

/**
 * In the situation where a subspace contains both lumobjs and a polyobj, lumobjs
 * must be clipped more carefully. Here we check if the line of sight intersects
 * any of the polyobj half-edges facing the viewer.
 */
static void clipSubspaceLumobjsBySight()
{
    DE_ASSERT(::curSubspace);

    // Any work to do?
    if (!::curSubspace->polyobjCount())
        return;

    ::curSubspace->forAllLumobjs([] (Lumobj &lob)
    {
        R_ViewerClipLumobjBySight(&lob, ::curSubspace);
        return LoopContinue;
    });
}

/// If not front facing this is no-op.
static void clipFrontFacingWalls(mesh::HEdge &hedge)
{
    if (!hedge.hasMapElement())
        return;

    auto &seg = hedge.mapElementAs<LineSideSegment>();
    if (seg.isFrontFacing())
    {
        if (!ClientApp::render().angleClipper().checkRangeFromViewRelPoints(hedge.origin(), hedge.twin().origin()))
        {
            seg.setFrontFacing(false);
        }
    }
}

static void clipSubspaceFrontFacingWalls()
{
    DE_ASSERT(::curSubspace);

    auto *base  = ::curSubspace->poly().hedge();
    DE_ASSERT(base);
    auto *hedge = base;
    do
    {
        clipFrontFacingWalls(*hedge);
    } while ((hedge = &hedge->next()) != base);

    ::curSubspace->forAllExtraMeshes([] (mesh::Mesh &mesh)
    {
        for (auto *hedge : mesh.hedges())
        {
            clipFrontFacingWalls(*hedge);
        }
        return LoopContinue;
    });

    ::curSubspace->forAllPolyobjs([] (Polyobj &pob)
    {
        for (auto *hedge : pob.mesh().hedges())
        {
            clipFrontFacingWalls(*hedge);
        }
        return LoopContinue;
    });
}

static void projectSubspaceSprites()
{
    DE_ASSERT(::curSubspace);

    // Do not use validCount because other parts of the renderer may change it.
    if (::curSubspace->lastSpriteProjectFrame() == R_FrameCount())
        return;  // Already added.

    R_ForAllSubspaceMobContacts(*::curSubspace, [] (mobj_t &mob)
    {
        const auto &subsec = ::curSubspace->subsector().as<Subsector>();
        if (mob.addFrameCount != R_FrameCount())
        {
            mob.addFrameCount = R_FrameCount();

            R_ProjectSprite(mob);

            // Kludge: Map-objects have a tendency to extend into the ceiling in
            // sky sectors. Here we will raise the skyfix dynamically, to make
            // sure they don't get clipped by the sky.
            if (subsec.visCeiling().surface().hasSkyMaskedMaterial())
            {
                /// @todo fixme: Consider 3D models, also. -ds
                if (const Record *spriteRec = Mobj_SpritePtr(mob))
                {
                    defn::Sprite const sprite(*spriteRec);
                    const res::Uri &viewMaterial = sprite.viewMaterial(0);
                    if (!viewMaterial.isEmpty())
                    {
                        if (world::Material *material = world::Materials::get().materialPtr(viewMaterial))
                        {
                            if (!(mob.dPlayer && (mob.dPlayer->flags & DDPF_CAMERA))
                               && mob.origin[2] <= subsec.visCeiling().heightSmoothed()
                               && mob.origin[2] >= subsec.visFloor  ().heightSmoothed())
                            {
                                ClSkyPlane &skyCeiling = subsec.sector().map().as<Map>().skyCeiling();
                                double visibleTop = mob.origin[2] + material->height();
                                if (visibleTop > skyCeiling.height())
                                {
                                    // Raise the ceiling!
                                    skyCeiling.setHeight(visibleTop + 16/*leeway*/);
                                }
                            }
                        }
                    }
                }
            }
        }
        return LoopContinue;
    });

    ::curSubspace->setLastSpriteProjectFrame(R_FrameCount());
}

/**
 * @pre Assumes the subspace is at least partially visible.
 */
static void drawCurrentSubspace()
{
    DE_ASSERT(curSubspace);

    auto &sector = ::curSubspace->sector();

    // Mark the leaf as visible for this frame.
    R_ViewerSubspaceMarkVisible(*::curSubspace);

    markSubspaceFrontFacingWalls();

    // Perform contact spreading for this map region.
    sector.map().as<Map>().spreadAllContacts(::curSubspace->poly().bounds());

    Rend_DrawFlatRadio(*::curSubspace);

    // Before clip testing lumobjs (for halos), range-occlude the back facing edges.
    // After testing, range-occlude the front facing edges. Done before drawing wall
    // sections so that opening occlusions cut out unnecessary oranges.

    occludeSubspace(false /* back facing */);
    clipSubspaceLumobjs();
    occludeSubspace(true /* front facing */);

    clipSubspaceFrontFacingWalls();
    clipSubspaceLumobjsBySight();

    // Mark generators in the sector visible.
    if (::useParticles)
    {
        sector.map().as<Map>().forAllGeneratorsInSector(sector, [] (Generator &gen)
        {
            R_ViewerGeneratorMarkVisible(gen);
            return LoopContinue;
        });
    }

    // Sprites for this subspace have to be drawn.
    //
    // Must be done BEFORE the wall segments of this subspace are added to the
    // clipper. Otherwise the sprites would get clipped by them, and that wouldn't
    // be right.
    //
    // Must be done AFTER the lumobjs have been clipped as this affects the projection
    // of halos.
    projectSubspaceSprites();

    writeSubspaceSkyMask();
    writeSubspaceWalls();
    writeSubspaceFlats();
}

/**
 * Change the current subspace (updating any relevant draw state properties
 * accordingly).
 *
 * @param subspace  The new subspace to make current.
 */
static void makeCurrent(ConvexSubspace &subspace)
{
    const bool subsecChanged = (!::curSubspace || ::curSubspace->subsectorPtr() != subspace.subsectorPtr());

    ::curSubspace = &subspace;

    // Update draw state.
    if (subsecChanged)
    {
        const Vec4f color = subspace.subsector().as<Subsector>().lightSourceColorfIntensity();
        ::curSectorLightColor = color.toVec3f();
        ::curSectorLightLevel = color.w;
    }
}

static void traverseBspTreeAndDrawSubspaces(const world::BspTree *bspTree)
{
    DE_ASSERT(bspTree);
    const AngleClipper &clipper = ClientApp::render().angleClipper();

    while (!bspTree->isLeaf())
    {
        // Descend deeper into the nodes.
        const auto &bspNode = bspTree->userData()->as<world::BspNode>();
        // Decide which side the view point is on.
        const int eyeSide  = bspNode.pointOnSide(eyeOrigin) < 0;

        // Recursively divide front space.
        traverseBspTreeAndDrawSubspaces(bspTree->childPtr(world::BspTree::ChildId(eyeSide)));

        // If the clipper is full we're pretty much done. This means no geometry
        // will be visible in the distance because every direction has already
        // been fully covered by geometry.
        if (!::firstSubspace && clipper.isFull())
            return;

        // ...and back space.
        bspTree = bspTree->childPtr(world::BspTree::ChildId(eyeSide ^ 1));
    }
    // We've arrived at a leaf.

    // Only leafs with a convex subspace geometry contain surfaces to draw.
    if (auto *subspace = bspTree->userData()->as<world::BspLeaf>().subspacePtr())
    {
        DE_ASSERT(subspace->hasSubsector());
        
        // Skip zero-volume subspaces.
        // (Neighbors handle the angle clipper ranges.)
        if (!subspace->subsector().as<Subsector>().hasWorldVolume())
            return;
        
        // Is this subspace visible?
        if (!::firstSubspace && !clipper.isPolyVisible(subspace->poly()))
            return;

        // This is now the current subspace.
        makeCurrent(subspace->as<ConvexSubspace>());

        drawCurrentSubspace();

        // This is no longer the first subspace.
        ::firstSubspace = false;
    }
}

/**
 * Project all the non-clipped decorations. They become regular vissprites.
 */
static void generateDecorationFlares(Map &map)
{
    const Vec3d viewPos = Rend_EyeOrigin().xzy();
    map.forAllLumobjs([&viewPos] (Lumobj &lob)
    {
        lob.generateFlare(viewPos, R_ViewerLumobjDistance(lob.indexInMap()));

        /// @todo mark these light sources visible for LensFx
        return LoopContinue;
    });
}

MaterialAnimator *Rend_SpriteMaterialAnimator(const Record &spriteDef)
{
    MaterialAnimator *matAnimator = nullptr;

    // Check the cache first.
    auto found = spriteAnimLookup().find(&spriteDef);
    if (found != spriteAnimLookup().end())
    {
        matAnimator = found->second;
    }
    else
    {
        // Look it up...
        defn::Sprite const sprite(spriteDef);
        const res::Uri &viewMaterial = sprite.viewMaterial(0);
        if (!viewMaterial.isEmpty())
        {
            if (world::Material *mat = world::Materials::get().materialPtr(viewMaterial))
            {
                matAnimator = &mat->as<ClientMaterial>().getAnimator(Rend_SpriteMaterialSpec());
            }
        }
        spriteAnimLookup().insert(&spriteDef, matAnimator);
    }
    return matAnimator;
}

double Rend_VisualRadius(const Record &spriteDef)
{
    if (auto *anim = Rend_SpriteMaterialAnimator(spriteDef))
    {
        anim->prepare();  // Ensure we've up to date info.
        return anim->dimensions().x / 2;
    }
    return 0;
}

Lumobj *Rend_MakeLumobj(const Record &spriteDef)
{
    LOG_AS("Rend_MakeLumobj");

    //defn::Sprite const sprite(spriteRec);
    //const res::Uri &viewMaterial = sprite.viewMaterial(0);

    // Always use the front view.
    /// @todo We could do better here...
    //if (viewMaterial.isEmpty()) return nullptr;

    //world::Material *mat = world::Materials::get().materialPtr(viewMaterial);
    //if (!mat) return nullptr;

    MaterialAnimator *matAnimator = Rend_SpriteMaterialAnimator(spriteDef); //mat->as<ClientMaterial>().getAnimator(Rend_SpriteMaterialSpec());
    if (!matAnimator) return nullptr;

    matAnimator->prepare();  // Ensure we have up-to-date info.

    TextureVariant *texture = matAnimator->texUnit(MaterialAnimator::TU_LAYER0).texture;
    if (!texture) return nullptr;  // Unloadable texture?

    const auto *pl = (const pointlight_analysis_t *)
            texture->base().analysisDataPointer(res::Texture::BrightPointAnalysis);
    if (!pl)
    {
        LOGDEV_RES_WARNING("Texture \"%s\" has no BrightPointAnalysis")
                << texture->base().manifest().composeUri();
        return nullptr;
    }

    // Apply the auto-calculated color.
    return &(new Lumobj(Vec3d(), pl->brightMul, Vec3f(pl->color.rgb)))
            ->setZOffset(-texture->base().origin().y - pl->originY * matAnimator->dimensions().y);
}

/**
 * Setup GL state for an entire rendering pass (compassing multiple lists).
 */
static void pushGLStateForPass(DrawMode mode, TexUnitMap &texUnitMap)
{
    static float const black[] = { 0, 0, 0, 0 };

    texUnitMap.fill(-1);

    switch (mode)
    {
    case DM_SKYMASK:
        GL_SelectTexUnits(0);
        DGL_Disable(DGL_ALPHA_TEST);
        DGL_Enable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LESS);
        break;

    case DM_BLENDED:
        GL_SelectTexUnits(2);

        // Intentional fall-through.

    case DM_ALL:
        // The first texture unit is used for the main texture.
        texUnitMap[0] = AttributeSpec::TexCoord0;
        texUnitMap[1] = AttributeSpec::TexCoord1;
        DGL_Disable(DGL_ALPHA_TEST);
        DGL_Enable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LESS);

        // Fog is allowed during this pass.
        if (fogParams.usingFog)
        {
            DGL_Enable(DGL_FOG);
        }
        // All of the surfaces are opaque.        
        DGL_Disable(DGL_BLEND);
        break;

    case DM_LIGHT_MOD_TEXTURE:
    case DM_TEXTURE_PLUS_LIGHT:
        // Modulate sector light, dynamic light and regular texture.
        GL_SelectTexUnits(2);
        if (mode == DM_LIGHT_MOD_TEXTURE)
        {
            texUnitMap[0] = AttributeSpec::ModTexCoord;
            texUnitMap[1] = AttributeSpec::TexCoord0;
            DGL_ModulateTexture(4);  // Light * texture.
        }
        else
        {
            texUnitMap[0] = AttributeSpec::TexCoord0;
            texUnitMap[1] = AttributeSpec::ModTexCoord;
            DGL_ModulateTexture(5);  // Texture + light.
        }
        DGL_Disable(DGL_ALPHA_TEST);
        DGL_Enable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LESS);

        // Fog is allowed during this pass.
        if (fogParams.usingFog)
        {
            DGL_Enable(DGL_FOG);
        }
        // All of the surfaces are opaque.
        DGL_Disable(DGL_BLEND);
        break;

    case DM_FIRST_LIGHT:
        // One light, no texture.
        GL_SelectTexUnits(1);
        texUnitMap[0] = AttributeSpec::ModTexCoord;
        DGL_ModulateTexture(6);
        DGL_Disable(DGL_ALPHA_TEST);
        DGL_Enable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LESS);
        // All of the surfaces are opaque.
        DGL_Disable(DGL_BLEND);
        break;

    case DM_BLENDED_FIRST_LIGHT:
        // One additive light, no texture.
        GL_SelectTexUnits(1);
        texUnitMap[0] = AttributeSpec::ModTexCoord;
        DGL_ModulateTexture(7);  // Add light, no color.
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_SetFloat(DGL_ALPHA_LIMIT, 1 / 255.0f);
        DGL_Disable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LEQUAL);
        // All of the surfaces are opaque.
        DGL_Enable(DGL_BLEND);
        DGL_BlendFunc(DGL_ONE, DGL_ONE);
        break;

    case DM_WITHOUT_TEXTURE:
        GL_SelectTexUnits(0);
        DGL_ModulateTexture(1);
        DGL_Disable(DGL_ALPHA_TEST);
        DGL_Enable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LESS);
        // All of the surfaces are opaque.
        DGL_Disable(DGL_BLEND);
        break;

    case DM_LIGHTS:
        GL_SelectTexUnits(1);
        texUnitMap[0] = AttributeSpec::TexCoord0;
        DGL_ModulateTexture(1);
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_SetFloat(DGL_ALPHA_LIMIT, 1 / 255.0f);
        DGL_Disable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LEQUAL);

        if (fogParams.usingFog)
        {
            DGL_Enable(DGL_FOG);
            DGL_Fogfv(DGL_FOG_COLOR, black);
        }

        DGL_Enable(DGL_BLEND);
        GL_BlendMode(BM_ADD);
        break;

    case DM_MOD_TEXTURE:
    case DM_MOD_TEXTURE_MANY_LIGHTS:
    case DM_BLENDED_MOD_TEXTURE:
        // The first texture unit is used for the main texture.
        texUnitMap[0] = AttributeSpec::TexCoord0;
        texUnitMap[1] = AttributeSpec::TexCoord1;
        DGL_Disable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LEQUAL);

        // All of the surfaces are opaque.
        DGL_Enable(DGL_BLEND);
        DGL_BlendFunc(DGL_DST_COLOR, DGL_ZERO);
        break;

    case DM_UNBLENDED_TEXTURE_AND_DETAIL:
        texUnitMap[0] = AttributeSpec::TexCoord0;
        texUnitMap[1] = AttributeSpec::TexCoord0;
        DGL_Disable(DGL_ALPHA_TEST);
        DGL_Enable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LESS);

        // All of the surfaces are opaque.
        DGL_Disable(DGL_BLEND);
        // Fog is allowed.
        if (fogParams.usingFog)
        {
            DGL_Enable(DGL_FOG);
        }
        break;

    case DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        texUnitMap[0] = AttributeSpec::TexCoord0;
        texUnitMap[1] = AttributeSpec::TexCoord0;
        DGL_Disable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LEQUAL);

        // All of the surfaces are opaque.
        DGL_Enable(DGL_BLEND);
        DGL_BlendFunc(DGL_DST_COLOR, DGL_ZERO);
        break;

    case DM_ALL_DETAILS:
        GL_SelectTexUnits(1);
        texUnitMap[0] = AttributeSpec::TexCoord0;
        DGL_ModulateTexture(0);
        DGL_Disable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LEQUAL);

        // All of the surfaces are opaque.
        DGL_Enable(DGL_BLEND);
        DGL_BlendFunc(DGL_DST_COLOR, DGL_SRC_COLOR);
        // Use fog to fade the details, if fog is enabled.
        if (fogParams.usingFog)
        {
            DGL_Enable(DGL_FOG);
            float const midGray[] = { .5f, .5f, .5f, fogParams.fogColor[3] };  // The alpha is probably meaningless?
            DGL_Fogfv(DGL_FOG_COLOR, midGray);
        }
        break;

    case DM_BLENDED_DETAILS:
        GL_SelectTexUnits(2);
        texUnitMap[0] = AttributeSpec::TexCoord0;
        texUnitMap[1] = AttributeSpec::TexCoord1;
        DGL_ModulateTexture(3);
        DGL_Disable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LEQUAL);

        // All of the surfaces are opaque.
        DGL_Enable(DGL_BLEND);
        DGL_BlendFunc(DGL_DST_COLOR, DGL_SRC_COLOR);
        // Use fog to fade the details, if fog is enabled.
        if (fogParams.usingFog)
        {
            DGL_Enable(DGL_FOG);
            float const midGray[] = { .5f, .5f, .5f, fogParams.fogColor[3] };  // The alpha is probably meaningless?
            DGL_Fogfv(DGL_FOG_COLOR, midGray);
        }
        break;

    case DM_SHADOW:
        // A bit like 'negative lights'.
        GL_SelectTexUnits(1);
        texUnitMap[0] = AttributeSpec::TexCoord0;
        DGL_ModulateTexture(1);
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_SetFloat(DGL_ALPHA_LIMIT, 1 / 255.0f);
        DGL_Disable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LEQUAL);
        // Set normal fog, if it's enabled.
        if (fogParams.usingFog)
        {
            DGL_Enable(DGL_FOG);
            DGL_Fogfv(DGL_FOG_COLOR, fogParams.fogColor);
        }
        DGL_Enable(DGL_BLEND);
        GL_BlendMode(BM_NORMAL);
        break;

    case DM_SHINY:
        GL_SelectTexUnits(1);
        texUnitMap[0] = AttributeSpec::TexCoord0;
        DGL_ModulateTexture(1);  // 8 for multitexture
        DGL_Disable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LEQUAL);

        if (fogParams.usingFog)
        {
            // Fog makes the shininess diminish in the distance.
            DGL_Enable(DGL_FOG);
            DGL_Fogfv(DGL_FOG_COLOR, black);
        }
        DGL_Enable(DGL_BLEND);
        GL_BlendMode(BM_ADD);  // Purely additive.
        break;

    case DM_MASKED_SHINY:
        GL_SelectTexUnits(2);
        texUnitMap[0] = AttributeSpec::TexCoord0;
        texUnitMap[1] = AttributeSpec::TexCoord1;  // the mask
        DGL_ModulateTexture(8);  // same as with details
        DGL_Disable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_DepthFunc(DGL_LEQUAL);

        if (fogParams.usingFog)
        {
            // Fog makes the shininess diminish in the distance.
            DGL_Enable(DGL_FOG);
            DGL_Fogfv(DGL_FOG_COLOR, black);
        }
        DGL_Enable(DGL_BLEND);
        GL_BlendMode(BM_ADD);  // Purely additive.
        break;

    default: break;
    }
}

static void popGLStateForPass(DrawMode mode)
{
    switch (mode)
    {
    default: break;

    case DM_SKYMASK:
        GL_SelectTexUnits(1);
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_TEST);
        break;

    case DM_BLENDED:
        GL_SelectTexUnits(1);
        DE_FALLTHROUGH;
    case DM_ALL:
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_TEST);
        if (fogParams.usingFog)
        {
            DGL_Disable(DGL_FOG);
        }
        DGL_Enable(DGL_BLEND);
        break;

    case DM_LIGHT_MOD_TEXTURE:
    case DM_TEXTURE_PLUS_LIGHT:
        GL_SelectTexUnits(1);
        DGL_ModulateTexture(1);
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_TEST);
        if (fogParams.usingFog)
        {
            DGL_Disable(DGL_FOG);
        }
        DGL_Enable(DGL_BLEND);
        break;

    case DM_FIRST_LIGHT:
        DGL_ModulateTexture(1);
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_TEST);
        DGL_Enable(DGL_BLEND);
        break;

    case DM_BLENDED_FIRST_LIGHT:
        DGL_ModulateTexture(1);
        DGL_Disable(DGL_DEPTH_TEST);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
        break;

    case DM_WITHOUT_TEXTURE:
        GL_SelectTexUnits(1);
        DGL_ModulateTexture(1);
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_TEST);
        DGL_Enable(DGL_BLEND);
        break;

    case DM_LIGHTS:
        DGL_Disable(DGL_DEPTH_TEST);
        if (fogParams.usingFog)
        {
            DGL_Disable(DGL_FOG);
        }
        GL_BlendMode(BM_NORMAL);
        break;

    case DM_MOD_TEXTURE:
    case DM_MOD_TEXTURE_MANY_LIGHTS:
    case DM_BLENDED_MOD_TEXTURE:
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_TEST);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
        break;

    case DM_UNBLENDED_TEXTURE_AND_DETAIL:
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_TEST);
        DGL_Enable(DGL_BLEND);
        if (fogParams.usingFog)
        {
            DGL_Disable(DGL_FOG);
        }
        break;

    case DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_TEST);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
        break;

    case DM_ALL_DETAILS:
        DGL_ModulateTexture(1);
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_TEST);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
        if (fogParams.usingFog)
        {
            DGL_Disable(DGL_FOG);
        }
        break;

    case DM_BLENDED_DETAILS:
        GL_SelectTexUnits(1);
        DGL_ModulateTexture(1);
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_TEST);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
        if (fogParams.usingFog)
        {
            DGL_Disable(DGL_FOG);
        }
        break;

    case DM_SHADOW:
        DGL_Disable(DGL_DEPTH_TEST);
        if (fogParams.usingFog)
        {
            DGL_Disable(DGL_FOG);
        }
        break;

    case DM_SHINY:
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_TEST);
        if (fogParams.usingFog)
        {
            DGL_Disable(DGL_FOG);
        }
        GL_BlendMode(BM_NORMAL);
        break;

    case DM_MASKED_SHINY:
        GL_SelectTexUnits(1);
        DGL_ModulateTexture(1);
        DGL_Enable(DGL_ALPHA_TEST);
        DGL_Disable(DGL_DEPTH_TEST);
        if (fogParams.usingFog)
        {
            DGL_Disable(DGL_FOG);
        }
        GL_BlendMode(BM_NORMAL);
        break;
    }
}

static void drawLists(const DrawLists::FoundLists &lists, DrawMode mode)
{
    if (lists.isEmpty()) return;
    // If the first list is empty -- do nothing.
    if (lists.at(0)->isEmpty()) return;

    // Setup GL state that's common to all the lists in this mode.
    TexUnitMap texUnitMap;
    pushGLStateForPass(mode, texUnitMap);

    // Draw each given list.
    for (int i = 0; i < lists.count(); ++i)
    {
        lists.at(i)->draw(mode, texUnitMap);
    }

    popGLStateForPass(mode);
}

static void drawSky()
{
    DrawLists::FoundLists lists;
    ClientApp::render().drawLists().findAll(SkyMaskGeom, lists);
    if (!devRendSkyAlways && lists.isEmpty())
    {
        return;
    }

    // We do not want to update color and/or depth.
    DGL_PushState();        
    DGL_Disable(DGL_DEPTH_TEST);
    DGL_Disable(DGL_DEPTH_WRITE);
    GLState::current()
            .setColorMask(gfx::WriteNone)
            .setStencilOp(gfx::StencilOp::Replace, gfx::StencilOp::Replace, gfx::StencilOp::Replace)
            .setStencilFunc(gfx::Always, 1, 0xff)
            .setStencilTest(true);
    
    // Mask out stencil buffer, setting the drawn areas to 1.
    if (!devRendSkyAlways)
    {
        drawLists(lists, DM_SKYMASK);
    }
    else
    {
        glClearStencil(1);
        glClear(GL_STENCIL_BUFFER_BIT);
    }

    // Restore previous GL state.
    DGL_PopState();
    
    // Now, only render where the stencil is set to 1.
    GLState::push()
            .setStencilTest(true)
            .setStencilFunc(gfx::Equal, 1, 0xff)
            .setStencilOp(gfx::StencilOp::Keep,  gfx::StencilOp::Keep,  gfx::StencilOp::Keep);
    
    ClientApp::render().sky().draw(&World::get().map().as<Map>().skyAnimator());

    if (!devRendSkyAlways)
    {
        glClearStencil(0);
    }

    // Return GL state to normal.
    DGL_PopState();
}

static bool generateHaloForVisSprite(const vissprite_t *spr, bool primary = false)
{
    DE_ASSERT(spr);

    if (primary && (spr->data.flare.flags & RFF_NO_PRIMARY))
    {
        return false;
    }

    float occlusionFactor;
    if (spr->data.flare.isDecoration)
    {
        // Surface decorations do not yet persist over frames, so we do
        // not smoothly occlude their flares. Instead, we will have to
        // put up with them instantly appearing/disappearing.
        occlusionFactor = R_ViewerLumobjIsClipped(spr->data.flare.lumIdx)? 0 : 1;
    }
    else
    {
        occlusionFactor = (spr->data.flare.factor & 0x7f) / 127.0f;
    }

    return H_RenderHalo(Vec3d(spr->pose.origin),
                        spr->data.flare.size,
                        spr->data.flare.tex,
                        Vec3f(spr->data.flare.color),
                        spr->pose.distance,
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
    if (::devNoSprites) return;

    R_SortVisSprites();

    if (::visSpriteP && ::visSpriteP > ::visSprites)
    {
        bool primaryHaloDrawn = false;

        // Draw all vissprites back to front.
        // Sprites look better with Z buffer writes turned off.
        for (vissprite_t *spr = ::visSprSortedHead.next; spr != &::visSprSortedHead; spr = spr->next)
        {
            switch (spr->type)
            {
            default: break;

            case VSPR_MASKED_WALL:
                // A masked wall is a specialized sprite.
                Rend_DrawMaskedWall(spr->data.wall);
                break;

            case VSPR_SPRITE:
                // Render an old fashioned sprite, ah the nostalgia...
                Rend_DrawSprite(*spr);
                break;

            case VSPR_MODEL:
                Rend_DrawModel(*spr);
                break;

            case VSPR_MODELDRAWABLE:
                DGL_Flush();
                ClientApp::render().modelRenderer().render(*spr);
                break;

            case VSPR_FLARE:
                if (generateHaloForVisSprite(spr, true))
                {
                    primaryHaloDrawn = true;
                }
                break;
            }
        }

        // Draw secondary halos?
        if (primaryHaloDrawn && ::haloMode > 1)
        {
            // Now we can setup the state only once.
            H_SetupState(true);

            for (vissprite_t *spr = ::visSprSortedHead.next; spr != &::visSprSortedHead; spr = spr->next)
            {
                if (spr->type == VSPR_FLARE)
                {
                    generateHaloForVisSprite(spr);
                }
            }

            // And we're done...
            H_SetupState(false);
        }
    }
}

/**
 * We have several different paths to accommodate both multitextured details and
 * dynamic lights. Details take precedence (they always cover entire primitives
 * and usually *all* of the surfaces in a scene).
 */
static void drawAllLists(Map &map)
{
    DE_ASSERT(!Sys_GLCheckError());
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    drawSky();

    // Render the real surfaces of the visible world.

    //
    // Pass: Unlit geometries (all normal lists).
    //
    DrawLists::FoundLists lists;
    ClientApp::render().drawLists().findAll(UnlitGeom, lists);
    if (IS_MTEX_DETAILS)
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

    //
    // Pass: Lit geometries.
    //
    ClientApp::render().drawLists().findAll(LitGeom, lists);

    // If multitexturing is available, we'll use it to our advantage when
    // rendering lights.
    if (IS_MTEX_LIGHTS && dynlightBlend != 2)
    {
        if (IS_MUL)
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
        if (IS_MUL)
        {
            // Render all lit surfaces without a texture.
            drawLists(lists, DM_WITHOUT_TEXTURE);
        }
        else
        {
            if (IS_MTEX_DETAILS) // Draw detail textures using multitexturing.
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

    //
    // Pass: All light geometries (always additive).
    //
    if (dynlightBlend != 2)
    {
        ClientApp::render().drawLists().findAll(LightGeom, lists);
        drawLists(lists, DM_LIGHTS);
    }

    //
    // Pass: Geometries with texture modulation.
    //
    if (IS_MUL)
    {
        // Finish the lit surfaces that didn't yet get a texture.
        ClientApp::render().drawLists().findAll(LitGeom, lists);
        if (IS_MTEX_DETAILS)
        {
            drawLists(lists, DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL);
            drawLists(lists, DM_BLENDED_MOD_TEXTURE);
            drawLists(lists, DM_BLENDED_DETAILS);
        }
        else
        {
            if (IS_MTEX_LIGHTS && dynlightBlend != 2)
            {
                drawLists(lists, DM_MOD_TEXTURE_MANY_LIGHTS);
            }
            else
            {
                drawLists(lists, DM_MOD_TEXTURE);
            }
        }
    }

    //
    // Pass: Geometries with details & modulation.
    //
    // If multitexturing is not available for details, we need to apply them as
    // an extra pass over all the detailed surfaces.
    //
    if (r_detail)
    {
        // Render detail textures for all surfaces that need them.
        ClientApp::render().drawLists().findAll(UnlitGeom, lists);
        if (IS_MTEX_DETAILS)
        {
            // Blended detail textures.
            drawLists(lists, DM_BLENDED_DETAILS);
        }
        else
        {
            drawLists(lists, DM_ALL_DETAILS);

            ClientApp::render().drawLists().findAll(LitGeom, lists);
            drawLists(lists, DM_ALL_DETAILS);
        }
    }

    //
    // Pass: Shiny geometries.
    //
    // If we have two texture units, the shiny masks will be enabled. Otherwise
    // the masks are ignored. The shine is basically specular environmental
    // additive light, multiplied by the mask so that black texels from the mask
    // produce areas without shine.
    //

    ClientApp::render().drawLists().findAll(ShineGeom, lists);

    // Render masked shiny surfaces in a separate pass.
    drawLists(lists, DM_SHINY);
    drawLists(lists, DM_MASKED_SHINY);

    //
    // Pass: Shadow geometries (objects and Fake Radio).
    //
    const int oldRenderTextures = renderTextures;

    renderTextures = true;

    ClientApp::render().drawLists().findAll(ShadowGeom, lists);
    drawLists(lists, DM_SHADOW);

    renderTextures = oldRenderTextures;

    DGL_Disable(DGL_TEXTURE_2D);

    // The draw lists do not modify these states -ds
    DGL_Enable(DGL_BLEND);
    DGL_Enable(DGL_DEPTH_WRITE);
    DGL_Enable(DGL_DEPTH_TEST);
    DGL_DepthFunc(DGL_LESS);
    DGL_Enable(DGL_ALPHA_TEST);
    DGL_SetFloat(DGL_ALPHA_LIMIT, 0);
    if (fogParams.usingFog)
    {
        DGL_Enable(DGL_FOG);
        DGL_Fogfv(DGL_FOG_COLOR, fogParams.fogColor);
    }

    // Draw masked walls, sprites and models.
    drawMasked();

    // Draw particles.
    Rend_RenderParticles(map);

    if (fogParams.usingFog)
    {
        DGL_Disable(DGL_FOG);
    }

    DE_ASSERT(!Sys_GLCheckError());
}

void Rend_RenderMap(Map &map)
{
    //GL_SetMultisample(true);

    // Setup the modelview matrix.
    Rend_ModelViewMatrix();

    if (!freezeRLs)
    {
        // Prepare for rendering.
        ClientApp::render().beginFrame();

        // Make vissprites of all the visible decorations.
        generateDecorationFlares(map);

        const viewdata_t *viewData = &viewPlayer->viewport();
        eyeOrigin = viewData->current.origin;

        // Add the backside clipping range (if vpitch allows).
        if (vpitch <= 90 - yfov / 2 && vpitch >= -90 + yfov / 2)
        {
            AngleClipper &clipper = ClientApp::render().angleClipper();

            float a = de::abs(vpitch) / (90 - yfov / 2);
            binangle_t startAngle = binangle_t(BANG_45 * Rend_FieldOfView() / 90) * (1 + a);
            binangle_t angLen = BANG_180 - startAngle;

            binangle_t viewside = (viewData->current.angle() >> (32 - BAMS_BITS)) + startAngle;
            clipper.safeAddRange(viewside, viewside + angLen);
            clipper.safeAddRange(viewside + angLen, viewside + 2 * angLen);
        }

        // The viewside line for the depth cue.
        viewsidex = -viewData->viewSin;
        viewsidey = viewData->viewCos;

        // We don't want BSP clip checking for the first subspace.
        firstSubspace = true;

        // No current subspace as of yet.
        curSubspace = nullptr;

        // Draw the world!
        traverseBspTreeAndDrawSubspaces(&map.bspTree());
    }
    drawAllLists(map);

    // Draw various debugging displays:
    //drawFakeRadioShadowPoints(map);
    drawSurfaceTangentVectors(map);
    drawLumobjs(map);
    drawMobjBoundingBoxes(map);
    drawSectors(map);
    drawVertexes(map);
    drawThinkers(map);
    drawSoundEmitters(map);
    drawGenerators(map);
    //drawBiasEditingVisuals(map);

    //GL_SetMultisample(false);

    DGL_Flush();
}

#if 0
static void drawStar(const Vec3d &origin, float size, const Vec4f &color)
{
    static float const black[] = { 0, 0, 0, 0 };

    DGL_Begin(DGL_LINES);
        DGL_Color4fv(black);
        DGL_Vertex3f(origin.x - size, origin.z, origin.y);
        DGL_Color4f(color.x, color.y, color.z, color.w);
        DGL_Vertex3f(origin.x, origin.z, origin.y);
        DGL_Vertex3f(origin.x, origin.z, origin.y);
        DGL_Color4fv(black);
        DGL_Vertex3f(origin.x + size, origin.z, origin.y);

        DGL_Vertex3f(origin.x, origin.z - size, origin.y);
        DGL_Color4f(color.x, color.y, color.z, color.w);
        DGL_Vertex3f(origin.x, origin.z, origin.y);
        DGL_Vertex3f(origin.x, origin.z, origin.y);
        DGL_Color4fv(black);
        DGL_Vertex3f(origin.x, origin.z + size, origin.y);

        DGL_Vertex3f(origin.x, origin.z, origin.y - size);
        DGL_Color4f(color.x, color.y, color.z, color.w);
        DGL_Vertex3f(origin.x, origin.z, origin.y);
        DGL_Vertex3f(origin.x, origin.z, origin.y);
        DGL_Color4fv(black);
        DGL_Vertex3f(origin.x, origin.z, origin.y + size);
    DGL_End();
}
#endif

static void drawLabel(const String &label, const Vec3d &origin, float scale, float opacity)
{
    if (label.isEmpty()) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(origin.x, origin.z, origin.y);
    DGL_Rotatef(-vang + 180, 0, 1, 0);
    DGL_Rotatef(vpitch, 1, 0, 0);
    DGL_Scalef(-scale, -scale, 1);

    Point2Raw offset = {{{2, 2}}};
    UI_TextOutEx(label, &offset, UI_Color(UIC_TITLE), opacity);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

static void drawLabel(const String &label, const Vec3d &origin, double maxDistance = 2000)
{
    const double distToEye = (Rend_EyeOrigin().xzy() - origin).length();
    if (distToEye < maxDistance)
    {
        drawLabel(label, origin, distToEye / (DE_GAMEVIEW_WIDTH / 2), 1 - distToEye / maxDistance);
    }
}

void Rend_UpdateLightModMatrix()
{
    if (novideo) return;

    de::zap(lightModRange);

    if (!world::World::get().hasMap())
    {
        rAmbient = 0;
        return;
    }

    int mapAmbient = App_World().map().ambientLightLevel();
    if (mapAmbient > ambientLight)
    {
        rAmbient = mapAmbient;
    }
    else
    {
        rAmbient = ambientLight;
    }

    for (int i = 0; i < 255; ++i)
    {
        // Adjust the white point/dark point?
        float lightlevel = 0;
        if (lightRangeCompression != 0)
        {
            if (lightRangeCompression >= 0)
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
        if (rAmbient != 0 && i+lightlevel <= rAmbient)
        {
            lightlevel = rAmbient - i;
        }

        // Clamp the result as a modifier to the light value (j).
        if ((i + lightlevel) >= 255)
        {
            lightlevel = 255 - i;
        }
        else if ((i + lightlevel) <= 0)
        {
            lightlevel = -i;
        }

        // Insert it into the matrix.
        lightModRange[i] = lightlevel / 255.0f;

        // Ensure the resultant value never exceeds the expected [0..1] range.
        DE_ASSERT(INRANGE_OF(i / 255.0f + lightModRange[i], 0.f, 1.f));
    }
}

float Rend_LightAdaptationDelta(float val)
{
    int clampedVal = de::clamp(0, de::roundi(255.0f * val), 254);
    return lightModRange[clampedVal];
}

void Rend_ApplyLightAdaptation(float &val)
{
    val += Rend_LightAdaptationDelta(val);
}

void Rend_DrawLightModMatrix()
{
#define BLOCK_WIDTH             ( 1.0f )
#define BLOCK_HEIGHT            ( BLOCK_WIDTH * 255.0f )
#define BORDER                  ( 20 )

    // Disabled?
    if (!devLightModRange) return;

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, DE_GAMEVIEW_WIDTH, DE_GAMEVIEW_HEIGHT, -1, 1);

    DGL_Translatef(BORDER, BORDER, 0);

    // Draw an outside border.
    DGL_Color4f(1, 1, 0, 1);
    DGL_Begin(DGL_LINES);
        DGL_Vertex2f(-1, -1);
        DGL_Vertex2f(255 + 1, -1);
        DGL_Vertex2f(255 + 1, -1);
        DGL_Vertex2f(255 + 1, BLOCK_HEIGHT + 1);
        DGL_Vertex2f(255 + 1, BLOCK_HEIGHT + 1);
        DGL_Vertex2f(-1, BLOCK_HEIGHT + 1);
        DGL_Vertex2f(-1, BLOCK_HEIGHT + 1);
        DGL_Vertex2f(-1, -1);
    DGL_End();

    DGL_Begin(DGL_QUADS);
    float c = 0;
    for (int i = 0; i < 255; ++i, c += (1.0f/255.0f))
    {
        // Get the result of the source light level + offset.
        float off = lightModRange[i];

        DGL_Color4f(c + off, c + off, c + off, 1);
        DGL_Vertex2f(i * BLOCK_WIDTH, 0);
        DGL_Vertex2f(i * BLOCK_WIDTH + BLOCK_WIDTH, 0);
        DGL_Vertex2f(i * BLOCK_WIDTH + BLOCK_WIDTH, BLOCK_HEIGHT);
        DGL_Vertex2f(i * BLOCK_WIDTH, BLOCK_HEIGHT);
    }
    DGL_End();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

#undef BORDER
#undef BLOCK_HEIGHT
#undef BLOCK_WIDTH
}

static void drawBBox(float br)
{
//    if (GL_NewList(name, GL_COMPILE))
//    {
    DGL_Begin(DGL_QUADS);
    {
        // Top
        DGL_TexCoord2f(0, 1.0f, 1.0f); DGL_Vertex3f( 1.0f+br, 1.0f,-1.0f-br);  // TR
        DGL_TexCoord2f(0, 0.0f, 1.0f); DGL_Vertex3f(-1.0f-br, 1.0f,-1.0f-br);  // TL
        DGL_TexCoord2f(0, 0.0f, 0.0f); DGL_Vertex3f(-1.0f-br, 1.0f, 1.0f+br);  // BL
        DGL_TexCoord2f(0, 1.0f, 0.0f); DGL_Vertex3f( 1.0f+br, 1.0f, 1.0f+br);  // BR
        // Bottom
        DGL_TexCoord2f(0, 1.0f, 1.0f); DGL_Vertex3f( 1.0f+br,-1.0f, 1.0f+br);  // TR
        DGL_TexCoord2f(0, 0.0f, 1.0f); DGL_Vertex3f(-1.0f-br,-1.0f, 1.0f+br);  // TL
        DGL_TexCoord2f(0, 0.0f, 0.0f); DGL_Vertex3f(-1.0f-br,-1.0f,-1.0f-br);  // BL
        DGL_TexCoord2f(0, 1.0f, 0.0f); DGL_Vertex3f( 1.0f+br,-1.0f,-1.0f-br);  // BR
        // Front
        DGL_TexCoord2f(0, 1.0f, 1.0f); DGL_Vertex3f( 1.0f+br, 1.0f+br, 1.0f);  // TR
        DGL_TexCoord2f(0, 0.0f, 1.0f); DGL_Vertex3f(-1.0f-br, 1.0f+br, 1.0f);  // TL
        DGL_TexCoord2f(0, 0.0f, 0.0f); DGL_Vertex3f(-1.0f-br,-1.0f-br, 1.0f);  // BL
        DGL_TexCoord2f(0, 1.0f, 0.0f); DGL_Vertex3f( 1.0f+br,-1.0f-br, 1.0f);  // BR
        // Back
        DGL_TexCoord2f(0, 1.0f, 1.0f); DGL_Vertex3f( 1.0f+br,-1.0f-br,-1.0f);  // TR
        DGL_TexCoord2f(0, 0.0f, 1.0f); DGL_Vertex3f(-1.0f-br,-1.0f-br,-1.0f);  // TL
        DGL_TexCoord2f(0, 0.0f, 0.0f); DGL_Vertex3f(-1.0f-br, 1.0f+br,-1.0f);  // BL
        DGL_TexCoord2f(0, 1.0f, 0.0f); DGL_Vertex3f( 1.0f+br, 1.0f+br,-1.0f);  // BR
        // Left
        DGL_TexCoord2f(0, 1.0f, 1.0f); DGL_Vertex3f(-1.0f, 1.0f+br, 1.0f+br);  // TR
        DGL_TexCoord2f(0, 0.0f, 1.0f); DGL_Vertex3f(-1.0f, 1.0f+br,-1.0f-br);  // TL
        DGL_TexCoord2f(0, 0.0f, 0.0f); DGL_Vertex3f(-1.0f,-1.0f-br,-1.0f-br);  // BL
        DGL_TexCoord2f(0, 1.0f, 0.0f); DGL_Vertex3f(-1.0f,-1.0f-br, 1.0f+br);  // BR
        // Right
        DGL_TexCoord2f(0, 1.0f, 1.0f); DGL_Vertex3f( 1.0f, 1.0f+br,-1.0f-br);  // TR
        DGL_TexCoord2f(0, 0.0f, 1.0f); DGL_Vertex3f( 1.0f, 1.0f+br, 1.0f+br);  // TL
        DGL_TexCoord2f(0, 0.0f, 0.0f); DGL_Vertex3f( 1.0f,-1.0f-br, 1.0f+br);  // BL
        DGL_TexCoord2f(0, 1.0f, 0.0f); DGL_Vertex3f( 1.0f,-1.0f-br,-1.0f-br);  // BR
    }
    DGL_End();
    //    return GL_EndList();
    //}
    //return 0;
}

/**
 * Draws a textured cube using the currently bound gl texture.
 * Used to draw mobj bounding boxes.
 *
 * @param pos          Coordinates of the center of the box (in map space units).
 * @param w            Width of the box.
 * @param l            Length of the box.
 * @param h            Height of the box.
 * @param a            Angle of the box.
 * @param color        Color to make the box (uniform vertex color).
 * @param alpha        Alpha to make the box (uniform vertex color).
 * @param br           Border amount to overlap box faces.
 * @param alignToBase  @c true= align the base of the box to the Z coordinate.
 */
void Rend_DrawBBox(const Vec3d &pos, coord_t w, coord_t l, coord_t h,
    float a, float const color[3], float alpha, float br, bool alignToBase)
{
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    if (alignToBase)
        // The Z coordinate is to the bottom of the object.
        DGL_Translatef(pos.x, pos.z + h, pos.y);
    else
        DGL_Translatef(pos.x, pos.z, pos.y);

    DGL_Rotatef(0, 0, 0, 1);
    DGL_Rotatef(0, 1, 0, 0);
    DGL_Rotatef(a, 0, 1, 0);

    DGL_Scalef(w - br - br, h - br - br, l - br - br);
    DGL_Color4f(color[0], color[1], color[2], alpha);

    //GL_CallList(dlBBox);
    drawBBox(.08f);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

/**
 * Draws a textured triangle using the currently bound gl texture.
 * Used to draw mobj angle direction arrow.
 *
 * @param pos    World space coordinates of the center of the base of the triangle.
 * @param a      Angle to point the triangle in.
 * @param s      Scale of the triangle.
 * @param color  Color to make the box (uniform vertex color).
 * @param alpha  Alpha to make the box (uniform vertex color).
 */
void Rend_DrawArrow(const Vec3d &pos, float a, float s, float const color[3],
                    float alpha)
{
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(pos.x, pos.z, pos.y);

    DGL_Rotatef(0, 0, 0, 1);
    DGL_Rotatef(0, 1, 0, 0);
    DGL_Rotatef(a, 0, 1, 0);

    DGL_Scalef(s, 0, s);

    DGL_Begin(DGL_TRIANGLES);
    {
        DGL_Color4f(0.0f, 0.0f, 0.0f, 0.5f);
        DGL_TexCoord2f(0, 1.0f, 1.0f);
        DGL_Vertex3f( 1.0f, 1.0f,-1.0f);  // L

        DGL_Color4f(color[0], color[1], color[2], alpha);
        DGL_TexCoord2f(0, 0.0f, 1.0f);
        DGL_Vertex3f(-1.0f, 1.0f,-1.0f);  // Point

        DGL_Color4f(0.0f, 0.0f, 0.0f, 0.5f);
        DGL_TexCoord2f(0, 0.0f, 0.0f);
        DGL_Vertex3f(-1.0f, 1.0f, 1.0f);  // R
    }
    DGL_End();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

static void drawMobjBBox(mobj_t &mob)
{
    static float const red   [] = { 1,    0.2f, 0.2f };  // non-solid objects
    static float const green [] = { 0.2f, 1,    0.2f };  // solid objects
    static float const yellow[] = { 0.7f, 0.7f, 0.2f };  // missiles

    // We don't want the console player.
    if (&mob == DD_Player(consolePlayer)->publicData().mo)
        return;

    // Is it vissible?
    if (!Mobj_IsLinked(mob)) return;

    const auto &bspLeaf = Mobj_BspLeafAtOrigin(mob);
    if (!bspLeaf.hasSubspace() || !R_ViewerSubspaceIsVisible(bspLeaf.subspace().as<ConvexSubspace>()))
        return;

    const double distToEye = (eyeOrigin - Mobj_Origin(mob)).length();
    float alpha = 1 - ((distToEye / (DE_GAMEVIEW_WIDTH/2)) / 4);
    if (alpha < .25f)
        alpha = .25f; // Don't make them totally invisible.

    // Draw a bounding box in an appropriate color.
    coord_t size = Mobj_Radius(mob);
    Rend_DrawBBox(Vec3d(mob.origin), size, size, mob.height/2, 0,
                  (mob.ddFlags & DDMF_MISSILE)? yellow :
                  (mob.ddFlags & DDMF_SOLID)? green : red,
                  alpha, .08f);

    Rend_DrawArrow(Vec3d(mob.origin), ((mob.angle + ANG45 + ANG90) / (float) ANGLE_MAX *-360), size*1.25,
                   (mob.ddFlags & DDMF_MISSILE)? yellow :
                   (mob.ddFlags & DDMF_SOLID)? green : red, alpha);
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
    //static float const red   [] = { 1,    0.2f, 0.2f };  // non-solid objects
    static float const green [] = { 0.2f, 1,    0.2f };  // solid objects
    static float const yellow[] = { 0.7f, 0.7f, 0.2f };  // missiles

    if (!devMobjBBox && !devPolyobjBBox) return;

#ifndef _DEBUG
    // Bounding boxes are not allowed in non-debug netgames.
    if (netGame) return;
#endif

//    if (!dlBBox)
//        dlBBox = constructBBox(0, .08f);

    DGL_Disable(DGL_DEPTH_TEST);
    DGL_Enable(DGL_TEXTURE_2D);
    //glDisable(GL_CULL_FACE);
    DGL_CullFace(DGL_NONE);

    MaterialAnimator &matAnimator = ClientMaterial::find(res::Uri("System", Path("bbox")))
            .getAnimator(Rend_SpriteMaterialSpec());

    // Ensure we've up to date info about the material.
    matAnimator.prepare();

    GL_BindTexture(matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture);
    GL_BlendMode(BM_ADD);

    if (devMobjBBox)
    {
        map.thinkers().forAll(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1, [] (thinker_t *th)
        {
            drawMobjBBox(*reinterpret_cast<mobj_t *>(th));
            return LoopContinue;
        });
    }

    if (devPolyobjBBox)
    {
        map.forAllPolyobjs([] (Polyobj &pob)
        {
            const auto &sec = pob.sector();

            coord_t width  = (pob.bounds.maxX - pob.bounds.minX)/2;
            coord_t length = (pob.bounds.maxY - pob.bounds.minY)/2;
            coord_t height = (sec.ceiling().height() - sec.floor().height())/2;

            Vec3d pos(pob.bounds.minX + width,
                         pob.bounds.minY + length,
                         sec.floor().height());

            const double distToEye = (eyeOrigin - pos).length();
            float alpha = 1 - ((distToEye / (DE_GAMEVIEW_WIDTH/2)) / 4);
            if (alpha < .25f)
                alpha = .25f; // Don't make them totally invisible.

            Rend_DrawBBox(pos, width, length, height, 0, yellow, alpha, .08f);

            for (auto *line : pob.lines())
            {
                Vec3d pos(line->center(), sec.floor().height());

                Rend_DrawBBox(pos, 0, line->length() / 2, height,
                              BANG2DEG(BANG_90 - line->angle()),
                              green, alpha, 0);
            }

            return LoopContinue;
        });
    }

    GL_BlendMode(BM_NORMAL);

    DGL_PopState();
    DGL_Disable(DGL_TEXTURE_2D);
    DGL_Enable(DGL_DEPTH_TEST);
}

static void drawPoint(const Vec3d &origin, const Vec4f &color = Vec4f(1))
{
    DGL_Begin(DGL_POINTS);
        DGL_Color4f(color.x, color.y, color.z, color.w);
        DGL_Vertex3f(origin.x, origin.z, origin.y);
    DGL_End();
}

static void drawVector(const Vec3f &vector, float scalar, const Vec4f &color = Vec4f(1))
{
    static float const black[] = { 0, 0, 0, 0 };

    DGL_Begin(DGL_LINES);
        DGL_Color4fv(black);
        DGL_Vertex3f(scalar * vector.x, scalar * vector.z, scalar * vector.y);
        DGL_Color4f(color.x, color.y, color.z, color.w);
        DGL_Vertex3f(0, 0, 0);
    DGL_End();
}

static void drawTangentVectorsForSurface(const Surface &suf, const Vec3d &origin)
{
    static const int VISUAL_LENGTH = 20;
    static Vec4f const red  ( 1, 0, 0, 1);
    static Vec4f const green( 0, 1, 0, 1);
    static Vec4f const blue ( 0, 0, 1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(origin.x, origin.z, origin.y);

    if (::devSurfaceVectors & SVF_TANGENT)   drawVector(suf.tangent(),   VISUAL_LENGTH, red);
    if (::devSurfaceVectors & SVF_BITANGENT) drawVector(suf.bitangent(), VISUAL_LENGTH, green);
    if (::devSurfaceVectors & SVF_NORMAL)    drawVector(suf.normal(),    VISUAL_LENGTH, blue);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

/**
 * @todo Determine Z-axis origin from a WallEdge.
 */
static void drawTangentVectorsForWalls(const mesh::HEdge *hedge)
{
    if (!hedge || !hedge->hasMapElement())
        return;

    const LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
    const LineSide &lineSide   = seg.lineSide().as<LineSide>();
    const Line &line           = lineSide.line().as<Line>();
    const Vec2d center         = (hedge->twin().origin() + hedge->origin()) / 2;

    if (lineSide.considerOneSided())
    {
        auto &subsec =
            (line.definesPolyobj() ? line.polyobj().bspLeaf().subspace()
                                   : hedge->face().mapElementAs<ConvexSubspace>())
                .subsector().as<Subsector>();

        const double bottom = subsec.  visFloor().heightSmoothed();
        const double top    = subsec.visCeiling().heightSmoothed();

        drawTangentVectorsForSurface(lineSide.middle().as<Surface>(),
                                     Vec3d(center, bottom + (top - bottom) / 2));
    }
    else
    {
        auto &subsec =
            (line.definesPolyobj() ? line.polyobj().bspLeaf().subspace()
                                   : hedge->face().mapElementAs<ConvexSubspace>())
                .subsector().as<Subsector>();

        auto &backSubsec =
            (line.definesPolyobj() ? line.polyobj().bspLeaf().subspace()
                                   : hedge->twin().face().mapElementAs<ConvexSubspace>())
                .subsector().as<Subsector>();

        if (lineSide.middle().hasMaterial())
        {
            const double bottom = subsec.  visFloor().heightSmoothed();
            const double top    = subsec.visCeiling().heightSmoothed();

            drawTangentVectorsForSurface(lineSide.middle().as<Surface>(),
                                         Vec3d(center, bottom + (top - bottom) / 2));
        }

        if (backSubsec.visCeiling().heightSmoothed() < subsec.visCeiling().heightSmoothed() &&
           !(subsec.    visCeiling().surface().hasSkyMaskedMaterial() &&
             backSubsec.visCeiling().surface().hasSkyMaskedMaterial()))
        {
            const coord_t bottom = backSubsec.visCeiling().heightSmoothed();
            const coord_t top    = subsec.    visCeiling().heightSmoothed();

            drawTangentVectorsForSurface(lineSide.top().as<Surface>(),
                                         Vec3d(center, bottom + (top - bottom) / 2));
        }

        if (backSubsec.visFloor().heightSmoothed() > subsec.visFloor().heightSmoothed() &&
           !(subsec.    visFloor().surface().hasSkyMaskedMaterial() &&
             backSubsec.visFloor().surface().hasSkyMaskedMaterial()))
        {
            const coord_t bottom = subsec.    visFloor().heightSmoothed();
            const coord_t top    = backSubsec.visFloor().heightSmoothed();

            drawTangentVectorsForSurface(lineSide.bottom().as<Surface>(),
                                         Vec3d(center, bottom + (top - bottom) / 2));
        }
    }
}

/**
 * @todo Use drawTangentVectorsForWalls() for polyobjs too.
 */
static void drawSurfaceTangentVectors(Subsector &subsec)
{
    subsec.forAllSubspaces([] (world::ConvexSubspace &space)
    {
        const auto *base  = space.poly().hedge();
        const auto *hedge = base;
        do
        {
            drawTangentVectorsForWalls(hedge);
        } while ((hedge = &hedge->next()) != base);

        space.forAllExtraMeshes([] (mesh::Mesh &mesh)
        {
            for (auto *hedge : mesh.hedges())
            {
                drawTangentVectorsForWalls(hedge);
            }
            return LoopContinue;
        });

        space.forAllPolyobjs([] (Polyobj &pob)
        {
            for (auto *hedge : pob.mesh().hedges())
            {
                drawTangentVectorsForWalls(hedge);
            }
            return LoopContinue;
        });

        return LoopContinue;
    });

    const int planeCount = subsec.sector().planeCount();
    for (int i = 0; i < planeCount; ++i)
    {
        const Plane &plane = subsec.as<Subsector>().visPlane(i);
        double height     = 0;
        if (plane.surface().hasSkyMaskedMaterial()
            && (plane.isSectorFloor() || plane.isSectorCeiling()))
        {
            height = plane.map().skyPlane(plane.isSectorCeiling()).height();
        }
        else
        {
            height = plane.heightSmoothed();
        }

        drawTangentVectorsForSurface(plane.surface(), Vec3d(subsec.center(), height));
    }
}

/**
 * Draw the surface tangent space vectors, primarily for debug.
 */
static void drawSurfaceTangentVectors(Map &map)
{
    if (!::devSurfaceVectors) return;

    DGL_CullFace(DGL_NONE);

    map.forAllSectors([] (world::Sector &sec)
    {
        return sec.forAllSubsectors([] (world::Subsector &subsec)
        {
            drawSurfaceTangentVectors(subsec.as<Subsector>());
            return LoopContinue;
        });
    });

    //glEnable(GL_CULL_FACE);
    DGL_PopState();
}

static void drawLumobjs(Map &map)
{
    static float const black[] = { 0, 0, 0, 0 };

    if (!devDrawLums) return;

    DGL_PushState();
    DGL_Disable(DGL_DEPTH_TEST);
    DGL_CullFace(DGL_NONE);

    map.forAllLumobjs([] (Lumobj &lob)
    {
        if (rendMaxLumobjs > 0 && R_ViewerLumobjIsHidden(lob.indexInMap()))
            return LoopContinue;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();

        DGL_Translatef(lob.origin().x, lob.origin().z + lob.zOffset(), lob.origin().y);

        DGL_Begin(DGL_LINES);
        {
            DGL_Color4fv(black);
            DGL_Vertex3f(-lob.radius(), 0, 0);
            DGL_Color4f(lob.color().x, lob.color().y, lob.color().z, 1);
            DGL_Vertex3f(0, 0, 0);
            DGL_Vertex3f(0, 0, 0);
            DGL_Color4fv(black);
            DGL_Vertex3f(lob.radius(), 0, 0);

            DGL_Vertex3f(0, -lob.radius(), 0);
            DGL_Color4f(lob.color().x, lob.color().y, lob.color().z, 1);
            DGL_Vertex3f(0, 0, 0);
            DGL_Vertex3f(0, 0, 0);
            DGL_Color4fv(black);
            DGL_Vertex3f(0, lob.radius(), 0);

            DGL_Vertex3f(0, 0, -lob.radius());
            DGL_Color4f(lob.color().x, lob.color().y, lob.color().z, 1);
            DGL_Vertex3f(0, 0, 0);
            DGL_Vertex3f(0, 0, 0);
            DGL_Color4fv(black);
            DGL_Vertex3f(0, 0, lob.radius());
        }
        DGL_End();

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
        return LoopContinue;
    });

    DGL_PopState();
}

static String labelForLineSideSection(world::LineSide &side, int sectionId)
{
    return Stringf("Line #%i (%s, %s)",
                          side.line().indexInMap(),
                          Line::sideIdAsText(side.sideId()).upperFirstChar().c_str(),
                          LineSide::sectionIdAsText(sectionId).c_str());
}

static String labelForSector(world::Sector &sector)
{
    return Stringf("Sector #%i", sector.indexInMap());
}

static String labelForSectorPlane(world::Plane &plane)
{
    return Stringf("Sector #%i (%s)",
                          plane.sector().indexInMap(),
                          world::Sector::planeIdAsText(plane.indexInSector()).upperFirstChar().c_str());
}

/**
 * Debugging aid for visualizing sound origins.
 */
static void drawSoundEmitters(Map &map)
{
    static const double MAX_DISTANCE = 384;

    if (!devSoundEmitters) return;

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    DGL_Disable(DGL_DEPTH_TEST);
    DGL_Enable(DGL_TEXTURE_2D);

    if (devSoundEmitters & SOF_SIDE)
    {
        map.forAllLines([] (world::Line &line)
        {
            for (int i = 0; i < 2; ++i)
            {
                auto &side = line.side(i);
                if (!side.hasSections()) continue;

                drawLabel(labelForLineSideSection(side, LineSide::Middle)
                          , Vec3d(side.middleSoundEmitter().origin), MAX_DISTANCE);

                drawLabel(labelForLineSideSection(side, LineSide::Bottom)
                          , Vec3d(side.bottomSoundEmitter().origin), MAX_DISTANCE);

                drawLabel(labelForLineSideSection(side, LineSide::Top)
                          , Vec3d(side.topSoundEmitter   ().origin), MAX_DISTANCE);
            }
            return LoopContinue;
        });
    }

    if (devSoundEmitters & (SOF_SECTOR | SOF_PLANE))
    {
        map.forAllSectors([] (world::Sector &sector)
        {
            if (devSoundEmitters & SOF_PLANE)
            {
                sector.forAllPlanes([] (world::Plane &plane)
                {
                    drawLabel(labelForSectorPlane(plane)
                              , Vec3d(plane.soundEmitter().origin), MAX_DISTANCE);
                    return LoopContinue;
                });
            }

            if (devSoundEmitters & SOF_SECTOR)
            {
                drawLabel(labelForSector(sector)
                          , Vec3d(sector.soundEmitter().origin), MAX_DISTANCE);
            }
            return LoopContinue;
        });
    }

    DGL_Enable(DGL_DEPTH_TEST);
    DGL_Disable(DGL_TEXTURE_2D);
}

void Rend_DrawVectorLight(const VectorLightData &vlight, float alpha)
{
    if (alpha < .0001f) return;

    const float unitLength = 100;

    DGL_Begin(DGL_LINES);
        DGL_Color4f(vlight.color.x, vlight.color.y, vlight.color.z, alpha);
        DGL_Vertex3f(unitLength * vlight.direction.x, unitLength * vlight.direction.z, unitLength * vlight.direction.y);
        DGL_Color4f(vlight.color.x, vlight.color.y, vlight.color.z, 0);
        DGL_Vertex3f(0, 0, 0);
    DGL_End();
}

static String labelForGenerator(const Generator &gen)
{
    return String::asText(gen.id());
}

static void drawGenerator(const Generator &gen)
{
    static const int MAX_GENERATOR_DIST = 2048;

    if (gen.source || gen.isUntriggered())
    {
        const Vec3d origin   = gen.origin();
        const double distToEye = (eyeOrigin - origin).length();
        if (distToEye < MAX_GENERATOR_DIST)
        {
            drawLabel(labelForGenerator(gen), origin, distToEye / (DE_GAMEVIEW_WIDTH / 2)
                      , 1 - distToEye / MAX_GENERATOR_DIST);
        }
    }
}

/**
 * Debugging aid; Draw all active generators.
 */
static void drawGenerators(Map &map)
{
    if (!devDrawGenerators) return;

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    DGL_Disable(DGL_DEPTH_TEST);
    DGL_Enable(DGL_TEXTURE_2D);

    map.forAllGenerators([] (Generator &gen)
    {
        drawGenerator(gen);
        return LoopContinue;
    });

    DGL_Enable(DGL_DEPTH_TEST);
    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawBar(const Vec3d &origin, coord_t height, float opacity)
{
    static const int EXTEND_DIST = 64;
    static float const black[] = { 0, 0, 0, 0 };

    DGL_Begin(DGL_LINES);
        DGL_Color4fv(black);
        DGL_Vertex3f(origin.x, origin.z - EXTEND_DIST, origin.y);
        DGL_Color4f(1, 1, 1, opacity);
        DGL_Vertex3f(origin.x, origin.z, origin.y);
        DGL_Vertex3f(origin.x, origin.z, origin.y);
        DGL_Vertex3f(origin.x, origin.z + height, origin.y);
        DGL_Vertex3f(origin.x, origin.z + height, origin.y);
        DGL_Color4fv(black);
        DGL_Vertex3f(origin.x, origin.z + height + EXTEND_DIST, origin.y);
    DGL_End();
}

static String labelForVertex(const world::Vertex *vtx)
{
    DE_ASSERT(vtx);
    return String::asText(vtx->indexInMap());
}

struct drawvertexvisual_parameters_t
{
    int maxDistance;
    bool drawOrigin;
    bool drawBar;
    bool drawLabel;
    BitArray *drawnVerts;
};

static void drawVertexVisual(const world::Vertex &vertex, double minHeight, double maxHeight,
    drawvertexvisual_parameters_t &parms)
{
    if (!parms.drawOrigin && !parms.drawBar && !parms.drawLabel)
        return;

    // Skip vertexes produced by the space partitioner.
    if (vertex.indexInArchive() == world::MapElement::NoIndex)
        return;

    // Skip already processed verts?
    if (parms.drawnVerts)
    {
        if (parms.drawnVerts->testBit(vertex.indexInArchive()))
            return;
        parms.drawnVerts->setBit(vertex.indexInArchive(), true);
    }

    // Distance in 2D determines visibility/opacity.
    double distToEye = (Vec2d(eyeOrigin.x, eyeOrigin.y) - vertex.origin()).length();
    if (distToEye >= parms.maxDistance)
        return;

    Vec3d const origin(vertex.origin(), minHeight);
    const float opacity = 1 - distToEye / parms.maxDistance;

    if (parms.drawBar)
    {
        drawBar(origin, maxHeight - minHeight, opacity);
    }
    if (parms.drawOrigin)
    {
        drawPoint(origin, Vec4f(.7f, .7f, .2f, opacity * 4));
    }
    if (parms.drawLabel)
    {
        DGL_Disable(DGL_DEPTH_TEST);
        DGL_Enable(DGL_TEXTURE_2D);

        drawLabel(labelForVertex(&vertex), origin, distToEye / (DE_GAMEVIEW_WIDTH / 2), opacity);

        DGL_Enable(DGL_DEPTH_TEST);
        DGL_Disable(DGL_TEXTURE_2D);
    }
}

/**
 * Find the relative next minmal and/or maximal visual height(s) of all sector
 * planes which "interface" at the half-edge, edge vertex.
 *
 * @param base  Base half-edge to find heights for.
 * @param edge  Edge of the half-edge.
 * @param min   Current minimal height to use as a base (will be overwritten).
 *              Use DDMAXFLOAT if the base is unknown.
 * @param min   Current maximal height to use as a base (will be overwritten).
 *              Use DDMINFLOAT if the base is unknown.
 *
 * @todo Don't stop when a zero-volume back neighbor is found; process all of
 * the neighbors at the specified vertex (the half-edge geometry will need to
 * be linked such that "outside" edges are neighbor-linked similarly to those
 * with a face).
 */
static void findMinMaxPlaneHeightsAtVertex(mesh::HEdge *base, int edge,
    double &min, double &max)
{
    if (!base || !base->hasFace())
        return;

    if (!base->face().hasMapElement() || !base->face().mapElementAs<ConvexSubspace>().hasSubsector())
        return;

    // Process neighbors?
    if (!Subsector::isInternalEdge(base))
    {
        const ClockDirection direction = edge? Clockwise : CounterClockwise;
        auto *hedge = base;
        while ((hedge = &world::SubsectorCirculator::findBackNeighbor(*hedge, direction)) != base)
        {
            // Stop if there is no back space.
            const auto *backSpace = hedge->hasFace() ? &hedge->face().mapElementAs<ConvexSubspace>() : nullptr;
            if (!backSpace) break;

            const auto &subsec = backSpace->subsector().as<Subsector>();
            if (subsec.visFloor().heightSmoothed() < min)
            {
                min = subsec.visFloor().heightSmoothed();
            }

            if (subsec.visCeiling().heightSmoothed() > max)
            {
                max = subsec.visCeiling().heightSmoothed();
            }
        }
    }
}

static void drawSubspaceVertices(ConvexSubspace &sub, drawvertexvisual_parameters_t &parms)
{
    auto &subsec = sub.subsector().as<Subsector>();
    const double min = subsec.  visFloor().heightSmoothed();
    const double max = subsec.visCeiling().heightSmoothed();

    auto *base  = sub.poly().hedge();
    auto *hedge = base;
    do
    {
        double edgeMin = min;
        double edgeMax = max;
        findMinMaxPlaneHeightsAtVertex(hedge, 0 /*left edge*/, edgeMin, edgeMax);

        drawVertexVisual(hedge->vertex(), min, max, parms);

    } while ((hedge = &hedge->next()) != base);

    sub.forAllExtraMeshes([&min, &max, &parms] (mesh::Mesh &mesh)
    {
        for (auto *hedge : mesh.hedges())
        {
            drawVertexVisual(hedge->vertex(), min, max, parms);
            drawVertexVisual(hedge->twin().vertex(), min, max, parms);
        }
        return LoopContinue;
    });

    sub.forAllPolyobjs([&min, &max, &parms] (Polyobj &pob)
    {
        for (auto *line : pob.lines())
        {
            drawVertexVisual(line->from(), min, max, parms);
            drawVertexVisual(line->to(), min, max, parms);
        }
        return LoopContinue;
    });
}

/**
 * Draw the various vertex debug aids.
 */
static void drawVertexes(Map &map)
{
#define MAX_DISTANCE            1280  ///< From the viewer.

    float oldLineWidth = -1;

    if (!devVertexBars && !devVertexIndices)
        return;

    AABoxd box(eyeOrigin.x - MAX_DISTANCE, eyeOrigin.y - MAX_DISTANCE,
               eyeOrigin.x + MAX_DISTANCE, eyeOrigin.y + MAX_DISTANCE);

    BitArray drawnVerts(map.vertexCount());
    drawvertexvisual_parameters_t parms;
    parms.maxDistance = MAX_DISTANCE;
    parms.drawnVerts  = &drawnVerts;

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    if (devVertexBars)
    {
        DGL_Disable(DGL_DEPTH_TEST);

#if defined (DE_OPENGL)
        glEnable(GL_LINE_SMOOTH);
#endif
        oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
        DGL_SetFloat(DGL_LINE_WIDTH, 2);

        parms.drawBar   = true;
        parms.drawLabel = parms.drawOrigin = false;

        map.subspaceBlockmap().forAllInBox(box, [&box, &parms] (void *object)
        {
            // Check the bounds.
            auto &subspace = *(ConvexSubspace *)object;
            const AABoxd &polyBounds = subspace.poly().bounds();
            if (!(   polyBounds.maxX < box.minX
                 || polyBounds.minX > box.maxX
                 || polyBounds.minY > box.maxY
                 || polyBounds.maxY < box.minY))
            {
                drawSubspaceVertices(subspace, parms);
            }
            return LoopContinue;
        });

        DGL_Enable(DGL_DEPTH_TEST);
    }

    // Draw the vertex origins.
    const float oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);

//#if defined (DE_OPENGL)
//    glEnable(GL_POINT_SMOOTH);
//#endif
    DGL_SetFloat(DGL_POINT_SIZE, 6);

    DGL_Disable(DGL_DEPTH_TEST);

    parms.drawnVerts->fill(false);  // Process all again.
    parms.drawOrigin = true;
    parms.drawBar    = parms.drawLabel = false;

    map.subspaceBlockmap().forAllInBox(box, [&box, &parms] (void *object)
    {
        // Check the bounds.
        auto &subspace = *(ConvexSubspace *)object;
        const AABoxd &polyBounds = subspace.poly().bounds();
        if (!(   polyBounds.maxX < box.minX
              || polyBounds.minX > box.maxX
              || polyBounds.minY > box.maxY
              || polyBounds.maxY < box.minY))
        {
            drawSubspaceVertices(subspace, parms);
        }
        return LoopContinue;
    });

    DGL_Enable(DGL_DEPTH_TEST);

    if (devVertexIndices)
    {
        parms.drawnVerts->fill(false);  // Process all again.
        parms.drawLabel = true;
        parms.drawBar   = parms.drawOrigin = false;

        map.subspaceBlockmap().forAllInBox(box, [&box, &parms] (void *object)
        {
            auto &subspace = *(ConvexSubspace *)object;
            // Check the bounds.
            const AABoxd &polyBounds = subspace.poly().bounds();
            if (!(   polyBounds.maxX < box.minX
                  || polyBounds.minX > box.maxX
                  || polyBounds.minY > box.maxY
                  || polyBounds.maxY < box.minY))
            {
                drawSubspaceVertices(subspace, parms);
            }
            return LoopContinue;
        });
    }

    // Restore previous state.
    if (devVertexBars)
    {
        DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);
#if defined (DE_OPENGL)
        glDisable(GL_LINE_SMOOTH);
#endif
    }
    DGL_SetFloat(DGL_POINT_SIZE, oldPointSize);
//#if defined (DE_OPENGL)
//    glDisable(GL_POINT_SMOOTH);
//#endif

#undef MAX_VERTEX_POINT_DIST
}

static String labelForSubsector(const world::Subsector &subsec)
{
    return String::asText(subsec.sector().indexInMap());
}

/**
 * Draw the sector debugging aids.
 */
static void drawSectors(Map &map)
{
    static const double MAX_LABEL_DIST = 1280;

    if (!devSectorIndices) return;

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    DGL_Disable(DGL_DEPTH_TEST);
    DGL_Enable(DGL_TEXTURE_2D);

    // Draw a sector label at the center of each subsector:
    map.forAllSectors([] (world::Sector &sec)
    {
        return sec.forAllSubsectors([] (world::Subsector &subsec)
        {
            Vec3d const origin(subsec.center(), subsec.as<Subsector>().visFloor().heightSmoothed());
            const double distToEye = (eyeOrigin - origin).length();
            if (distToEye < MAX_LABEL_DIST)
            {
                drawLabel(labelForSubsector(subsec), origin, distToEye / (DE_GAMEVIEW_WIDTH / 2)
                          , 1 - distToEye / MAX_LABEL_DIST);
            }
            return LoopContinue;
        });
    });

    DGL_Enable(DGL_DEPTH_TEST);
    DGL_Disable(DGL_TEXTURE_2D);
}

static String labelForThinker(thinker_t *thinker)
{
    DE_ASSERT(thinker);
    return String::asText(thinker->id);
}

/**
 * Debugging aid for visualizing thinker IDs.
 */
static void drawThinkers(Map &map)
{
    static const double MAX_THINKER_DIST = 2048;

    if (!devThinkerIds) return;

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    DGL_Disable(DGL_DEPTH_TEST);
    DGL_Enable(DGL_TEXTURE_2D);

    map.thinkers().forAll(0x1 | 0x2, [] (thinker_t *th)
    {
        // Ignore non-mobjs.
        if (Thinker_IsMobj(th))
        {
            const Vec3d origin   = Mobj_Center(*(mobj_t *)th);
            const double distToEye = (eyeOrigin - origin).length();
            if (distToEye < MAX_THINKER_DIST)
            {
                drawLabel(labelForThinker(th), origin,  distToEye / (DE_GAMEVIEW_WIDTH / 2)
                          , 1 - distToEye / MAX_THINKER_DIST);
            }
        }
        return LoopContinue;
    });

    DGL_Enable(DGL_DEPTH_TEST);
    DGL_Disable(DGL_TEXTURE_2D);
}

#if 0
void Rend_LightGridVisual(LightGrid &lg)
{
    static Vec3f const red(1, 0, 0);
    static int blink = 0;

    // Disabled?
    if (!devLightGrid) return;

    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    // Determine the grid reference of the view player.
    LightGrid::Index viewerGridIndex = 0;
    if (viewPlayer)
    {
        blink++;
        viewerGridIndex = lg.toIndex(lg.toRef(viewPlayer->publicData().mo->origin));
    }

    // Go into screen projection mode.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, DE_GAMEVIEW_WIDTH, DE_GAMEVIEW_HEIGHT, -1, 1);

    for (int y = 0; y < lg.dimensions().y; ++y)
    {
        DGL_Begin(DGL_QUADS);
        for (int x = 0; x < lg.dimensions().x; ++x)
        {
            LightGrid::Index gridIndex = lg.toIndex(x, lg.dimensions().y - 1 - y);
            const bool isViewerIndex   = (viewPlayer && viewerGridIndex == gridIndex);

            const Vec3f *color = 0;
            if (isViewerIndex && (blink & 16))
            {
                color = &red;
            }
            else if (lg.primarySource(gridIndex))
            {
                color = &lg.rawColorRef(gridIndex);
            }

            if (!color) continue;

            glColor3f(color->x, color->y, color->z);

            DGL_Vertex2f(x * devLightGridSize, y * devLightGridSize);
            DGL_Vertex2f(x * devLightGridSize + devLightGridSize, y * devLightGridSize);
            DGL_Vertex2f(x * devLightGridSize + devLightGridSize,
                       y * devLightGridSize + devLightGridSize);
            DGL_Vertex2f(x * devLightGridSize, y * devLightGridSize + devLightGridSize);
        }
        DGL_End();
    }

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
}
#endif

const MaterialVariantSpec &Rend_MapSurfaceMaterialSpec(GLenum wrapS, GLenum wrapT)
{
    return ClientApp::resources().materialSpec(
        MapSurfaceContext, 0, 0, 0, 0, wrapS, wrapT, -1, -1, -1, true, true, false, false);
}

const MaterialVariantSpec &Rend_MapSurfaceMaterialSpec()
{
    if (!lookupMapSurfaceMaterialSpec)
    {
        lookupMapSurfaceMaterialSpec = &Rend_MapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT);
    }
    return *lookupMapSurfaceMaterialSpec;
}

/// Returns the texture variant specification for lightmaps.
const TextureVariantSpec &Rend_MapSurfaceLightmapTextureSpec()
{
    return ClientApp::resources().textureSpec(TC_MAPSURFACE_LIGHTMAP, 0, 0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                1, -1, -1, false, false, false, true);
}

const TextureVariantSpec &Rend_MapSurfaceShinyTextureSpec()
{
    return ClientApp::resources().textureSpec(TC_MAPSURFACE_REFLECTION, TSF_NO_COMPRESSION, 0, 0, 0,
                                GL_REPEAT, GL_REPEAT, 1, 1, -1, false, false, false, false);
}

const TextureVariantSpec &Rend_MapSurfaceShinyMaskTextureSpec()
{
    return ClientApp::resources().textureSpec(TC_MAPSURFACE_REFLECTIONMASK, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                                -1, -1, -1, true, false, false, false);
}

D_CMD(OpenRendererAppearanceEditor)
{
    DE_UNUSED(src, argc, argv);

    if (!App_GameLoaded())
    {
        LOG_ERROR("A game must be loaded before the Renderer Appearance editor can be opened");
        return false;
    }

    if (!ClientWindow::main().hasSidebar())
    {
        // The editor sidebar will give its ownership automatically
        // to the window.
        RendererAppearanceEditor *editor = new RendererAppearanceEditor;
        editor->open();
    }
    return true;
}

D_CMD(OpenModelAssetEditor)
{
    DE_UNUSED(src, argc, argv);

    if (!App_GameLoaded())
    {
        LOG_ERROR("A game must be loaded before the Model Asset editor can be opened");
        return false;
    }

    if (!ClientWindow::main().hasSidebar())
    {
        ModelAssetEditor *editor = new ModelAssetEditor;
        editor->open();
    }
    return true;
}

D_CMD(LowRes)
{
    DE_UNUSED(src, argv, argc);

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

D_CMD(MipMap)
{
    DE_UNUSED(src, argc);

    int newMipMode = String(argv[1]).toInt();
    if (newMipMode < 0 || newMipMode > 5)
    {
        LOG_SCR_ERROR("Invalid mipmapping mode %i; the valid range is 0...5") << newMipMode;
        return false;
    }

    mipmapping = newMipMode;
    //GL_SetAllTexturesMinFilter(glmode[mipmapping]);
    return true;
}

D_CMD(TexReset)
{
    DE_UNUSED(src);

    if (argc == 2 && !String(argv[1]).compareWithoutCase("raw"))
    {
        // Reset just raw images.
        GL_ReleaseTexturesForRawImages();
    }
    else
    {
        // Reset everything.
        GL_TexReset();
    }
    if (World::get().hasMap())
    {
        // Texture IDs are cached in Lumobjs, so have to redo all of them.
        World::get().map().as<Map>().redecorate();
    }
    return true;
}

static void detailFactorChanged()
{
    App_Resources().releaseGLTexturesByScheme("Details");
}

/*
static void fieldOfViewChanged()
{
    if (vrCfg().mode() == VRConfig::OculusRift)
    {
        if (Con_GetFloat("rend-vr-rift-fovx") != fieldOfView)
            Con_SetFloat("rend-vr-rift-fovx", fieldOfView);
    }
    else
    {
        if (Con_GetFloat("rend-vr-nonrift-fovx") != fieldOfView)
            Con_SetFloat("rend-vr-nonrift-fovx", fieldOfView);
    }
}*/

static void loadExtAlwaysChanged()
{
    GL_TexReset();
}

static void mipmappingChanged()
{
    GL_TexReset();
}

static void texGammaChanged()
{
    R_BuildTexGammaLut(texGamma);
    GL_TexReset();
    LOG_GL_MSG("Texture gamma correction set to %f") << texGamma;
}

static void texQualityChanged()
{
    GL_TexReset();
}

static void useDynlightsChanged()
{
    if (!ClientApp::world().hasMap()) return;

    // Unlink luminous objects.
    ClientApp::world().map().thinkers()
        .forAll(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1, [](thinker_t *th)
    {
        Mobj_UnlinkLumobjs(reinterpret_cast<mobj_t *>(th));
        return LoopContinue;
    });
}

#if 0
static void useSkylightChanged()
{
    scheduleFullLightGridUpdate();
}
#endif

static void useSmartFilterChanged()
{
    GL_TexReset();
}

void Rend_Register()
{
    //C_VAR_INT("rend-bias", &useBias, 0, 0, 1);
    C_VAR_FLOAT("rend-camera-fov", &fieldOfView, 0, 1, 179);

    C_VAR_FLOAT("rend-glow", &glowFactor, 0, 0, 2);
    C_VAR_INT("rend-glow-height", &glowHeightMax, 0, 0, 1024);
    C_VAR_FLOAT("rend-glow-scale", &glowHeightFactor, 0, 0.1f, 10);
    C_VAR_INT("rend-glow-wall", &useGlowOnWalls, 0, 0, 1);

    C_VAR_BYTE("rend-info-lums", &rendInfoLums, 0, 0, 1);

    C_VAR_INT2("rend-light", &useDynLights, 0, 0, 1, useDynlightsChanged);
    C_VAR_INT2("rend-light-ambient", &ambientLight, 0, 0, 255, Rend_UpdateLightModMatrix);
    C_VAR_FLOAT("rend-light-attenuation", &rendLightDistanceAttenuation, CVF_NO_MAX, 0, 0);
    C_VAR_INT("rend-light-blend", &dynlightBlend, 0, 0, 2);
    C_VAR_FLOAT("rend-light-bright", &dynlightFactor, 0, 0, 1);
    C_VAR_FLOAT2("rend-light-compression", &lightRangeCompression, 0, -1, 1, Rend_UpdateLightModMatrix);
    C_VAR_BYTE("rend-light-decor", &useLightDecorations, 0, 0, 1);
    C_VAR_FLOAT("rend-light-fog-bright", &dynlightFogBright, 0, 0, 1);
    //C_VAR_INT("rend-light-multitex", &useMultiTexLights, 0, 0, 1);
    C_VAR_INT("rend-light-num", &rendMaxLumobjs, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-light-sky", &rendSkyLight, 0, 0, 1/*, useSkylightChanged*/);
    C_VAR_BYTE("rend-light-sky-auto", &rendSkyLightAuto, 0, 0, 1/*, useSkylightChanged*/);
    C_VAR_FLOAT("rend-light-wall-angle", &rendLightWallAngle, CVF_NO_MAX, 0, 0);
    C_VAR_BYTE("rend-light-wall-angle-smooth", &rendLightWallAngleSmooth, 0, 0, 1);

    C_VAR_BYTE("rend-map-material-precache", &precacheMapMaterials, 0, 0, 1);

    C_VAR_INT("rend-shadow", &useShadows, 0, 0, 1);
    C_VAR_FLOAT("rend-shadow-darkness", &shadowFactor, 0, 0, 2);
    C_VAR_INT("rend-shadow-far", &shadowMaxDistance, CVF_NO_MAX, 0, 0);
    C_VAR_INT("rend-shadow-radius-max", &shadowMaxRadius, CVF_NO_MAX, 0, 0);

    C_VAR_INT("rend-tex", &renderTextures, CVF_NO_ARCHIVE, 0, 2);
    C_VAR_BYTE("rend-tex-anim-smooth", &smoothTexAnim, 0, 0, 1);
    C_VAR_INT("rend-tex-detail", &r_detail, 0, 0, 1);
    //C_VAR_INT("rend-tex-detail-multitex", &useMultiTexDetails, 0, 0, 1);
    C_VAR_FLOAT("rend-tex-detail-scale", &detailScale, CVF_NO_MIN | CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT2("rend-tex-detail-strength", &detailFactor, 0, 0, 5, detailFactorChanged);
    C_VAR_BYTE2("rend-tex-external-always", &loadExtAlways, 0, 0, 1, loadExtAlwaysChanged);
    C_VAR_INT("rend-tex-filter-anisotropic", &texAniso, 0, -1, 4);
    C_VAR_INT("rend-tex-filter-mag", &texMagMode, 0, 0, 1);
    C_VAR_INT2("rend-tex-filter-smart", &useSmartFilter, 0, 0, 1, useSmartFilterChanged);
    C_VAR_INT("rend-tex-filter-sprite", &filterSprites, 0, 0, 1);
    C_VAR_INT("rend-tex-filter-ui", &filterUI, 0, 0, 1);
    C_VAR_FLOAT2("rend-tex-gamma", &texGamma, 0, 0, 1, texGammaChanged);
    C_VAR_INT2("rend-tex-mipmap", &mipmapping, CVF_PROTECTED, 0, 5, mipmappingChanged);
    C_VAR_INT2("rend-tex-quality", &texQuality, 0, 0, 8, texQualityChanged);
    C_VAR_INT("rend-tex-shiny", &useShinySurfaces, 0, 0, 1);

    //C_VAR_BYTE("rend-bias-grid-debug", &devLightGrid, CVF_NO_ARCHIVE, 0, 1);
    //C_VAR_FLOAT("rend-bias-grid-debug-size", &devLightGridSize, 0, .1f, 100);
    C_VAR_BYTE("rend-dev-blockmap-debug", &bmapShowDebug, CVF_NO_ARCHIVE, 0, 4);
    C_VAR_FLOAT("rend-dev-blockmap-debug-size", &bmapDebugSize, CVF_NO_ARCHIVE, .1f, 100);
    C_VAR_INT("rend-dev-cull-leafs", &devNoCulling, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-freeze", &freezeRLs, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-generator-show-indices", &devDrawGenerators, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-light-mod", &devLightModRange, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-lums", &devDrawLums, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-mobj-bbox", &devMobjBBox, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-mobj-show-vlights", &devMobjVLights, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-polyobj-bbox", &devPolyobjBBox, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-sector-show-indices", &devSectorIndices, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-sky", &devRendSkyMode, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-sky-always", &devRendSkyAlways, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-soundorigins", &devSoundEmitters, CVF_NO_ARCHIVE, 0, 7);
    C_VAR_BYTE("rend-dev-surface-show-vectors", &devSurfaceVectors, CVF_NO_ARCHIVE, 0, 7);
    C_VAR_BYTE("rend-dev-thinker-ids", &devThinkerIds, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-tex-showfix", &devNoTexFix, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-vertex-show-bars", &devVertexBars, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-vertex-show-indices", &devVertexIndices, CVF_NO_ARCHIVE, 0, 1);

    C_CMD("rendedit", "", OpenRendererAppearanceEditor);
    C_CMD("modeledit", "", OpenModelAssetEditor);
    C_CMD("cubeshot", "i", CubeShot);

    C_CMD_FLAGS("lowres", "", LowRes, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("mipmap", "i", MipMap, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("texreset", "", TexReset, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("texreset", "s", TexReset, CMDF_NO_DEDICATED);

    LightDecoration::consoleRegister();
    Lumobj::consoleRegister();
    SkyDrawable::consoleRegister();
    Rend_ModelRegister();
    Rend_ParticleRegister();
    Generator::consoleRegister();
    Rend_RadioRegister();
    Rend_SpriteRegister();
    PostProcessing::consoleRegister();
    fx::Bloom::consoleRegister();
    fx::Vignette::consoleRegister();
    fx::LensFlares::consoleRegister();
    VR_ConsoleRegister();
}

ResourceConfigVars &R_Config()
{
    static ResourceConfigVars vars { nullptr, nullptr, nullptr};
    if (!vars.noHighResTex)
    {
        vars.noHighResTex     = &App::config("resource.noHighResTex");
        vars.noHighResPatches = &App::config("resource.noHighResPatches");
        vars.highResWithPWAD  = &App::config("resource.highResWithPWAD");
    }
    return vars;
}
