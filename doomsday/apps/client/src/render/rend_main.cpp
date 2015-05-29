/** @file rend_main.cpp  World Map Renderer.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_base.h"
#include "render/rend_main.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <QtAlgorithms>
#include <QBitArray>
#include <de/concurrency.h>
#include <de/timer.h>
#include <de/vector1.h>
#include <de/GLState>
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>

#include "clientapp.h"
#include "sys_system.h"
#include "api_fontrender.h"

#include "edit_bias.h"         /// @todo remove me
#include "r_util.h"

#include "MaterialVariantSpec"
#include "Texture"

#include "Face"
#include "world/map.h"
#include "world/blockmap.h"
#include "world/lineowner.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "world/sky.h"
#include "world/thinkers.h"
#include "BspLeaf"
#include "BspNode"
#include "Contact"
#include "ConvexSubspace"
#include "Hand"
#include "SectorCluster"
#include "Surface"
#include "BiasIllum"
#include "HueCircleVisual"
#include "LightDecoration"
#include "Lumobj"
#include "Shard"
#include "SkyFixEdge"
#include "SurfaceDecorator"
#include "TriangleStripBuilder"
#include "WallEdge"

#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "gl/sys_opengl.h"

#include "render/fx/bloom.h"
#include "render/fx/vignette.h"
#include "render/fx/lensflares.h"
#include "render/angleclipper.h"
#include "render/blockmapvisual.h"
#include "render/billboard.h"
#include "render/cameralensfx.h"
#include "render/r_main.h"
#include "render/r_things.h"
#include "render/rend_fakeradio.h"
#include "render/rend_halo.h"
#include "render/rend_particle.h"
#include "render/skydrawable.h"
#include "render/viewports.h"
#include "render/vissprite.h"
#include "render/vr.h"

#include "ui/editors/rendererappearanceeditor.h"
#include "ui/ui_main.h"

using namespace de;

/**
 * POD structure used to transport vertex data refs as a logical unit.
 * @note The geometric data itself is not owned!
 */
struct Geometry
{
    Vector3f *pos;
    Vector4f *color;
    Vector2f *tex;
    Vector2f *tex2;
};

/**
 * POD structure used to describe texture modulation data.
 */
struct TexModulationData
{
    DGLuint texture = 0;
    Vector3f color;
    Vector2f topLeft;
    Vector2f bottomRight;
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

void Rend_DrawBBox(Vector3d const &pos, coord_t w, coord_t l, coord_t h, dfloat a,
    dfloat const color[3], dfloat alpha, dfloat br, bool alignToBase = true);

void Rend_DrawArrow(Vector3d const &pos, dfloat a, dfloat s, dfloat const color3f[3], dfloat alpha);

D_CMD(OpenRendererAppearanceEditor);
D_CMD(LowRes);
D_CMD(MipMap);
D_CMD(TexReset);

dint useBias;  ///< Shadow Bias enabled? cvar

dd_bool usingFog;  ///< Is the fog in use?
dfloat fogColor[4];
dfloat fieldOfView = 95.0f;
dbyte smoothTexAnim = true;

dint renderTextures = true;
dint renderWireframe;
dint useMultiTexLights = true;
dint useMultiTexDetails = true;

dint dynlightBlend;

Vector3f torchColor(1, 1, 1);
dint torchAdditive = true;

dint useShinySurfaces = true;

dint useDynLights = true;
dfloat dynlightFactor = .7f;
dfloat dynlightFogBright = .15f;

dint useGlowOnWalls = true;
dfloat glowFactor = .8f;
dfloat glowHeightFactor = 3;  ///< Glow height as a multiplier.
dint glowHeightMax = 100;     ///< 100 is the default (0-1024).

dint useShadows = true;
dfloat shadowFactor = 1.2f;
dint shadowMaxRadius = 80;
dint shadowMaxDistance = 1000;

dbyte useLightDecorations = true;  ///< cvar

dfloat detailFactor = .5f;
dfloat detailScale = 4;

dint mipmapping = 5;
dint filterUI   = 1;
dint texQuality = TEXQ_BEST;

dint ratioLimit;      ///< Zero if none.
dd_bool fillOutlines = true;
dint useSmartFilter;  ///< Smart filter mode (cvar: 1=hq2x)
dint filterSprites = true;
dint texMagMode = 1;  ///< Linear.
dint texAniso = -1;   ///< Use best.

dd_bool noHighResTex;
dd_bool noHighResPatches;
dd_bool highResWithPWAD;
dbyte loadExtAlways;  ///< Always check for extres (cvar)

dfloat texGamma;

dint glmode[6] = // Indexed by 'mipmapping'.
{
    GL_NEAREST,
    GL_LINEAR,
    GL_NEAREST_MIPMAP_NEAREST,
    GL_LINEAR_MIPMAP_NEAREST,
    GL_NEAREST_MIPMAP_LINEAR,
    GL_LINEAR_MIPMAP_LINEAR
};

Vector3d vOrigin;
dfloat vang, vpitch;
dfloat viewsidex, viewsidey;

dbyte freezeRLs;
dint devNoCulling;  ///< @c 1= disabled (cvar).
dint devRendSkyMode;
dbyte devRendSkyAlways;

// Ambient lighting, rAmbient is used within the renderer, ambientLight is
// used to store the value of the ambient light cvar.
// The value chosen for rAmbient occurs in Rend_UpdateLightModMatrix
// for convenience (since we would have to recalculate the matrix anyway).
dint rAmbient, ambientLight;

dint viewpw, viewph;  ///< Viewport size, in pixels.
dint viewpx, viewpy;  ///< Viewpoint top left corner, in pixels.

dfloat yfov;

dint gameDrawHUD = 1;  ///< Set to zero when we advise that the HUD should not be drawn

/**
 * Implements a pre-calculated LUT for light level limiting and range
 * compression offsets, arranged such that it may be indexed with a
 * light level value. Return value is an appropriate delta (considering
 * all applicable renderer properties) which has been pre-clamped such
 * that when summed with the original light value the result remains in
 * the normalized range [0..1].
 */
dfloat lightRangeCompression;
dfloat lightModRange[255];
byte devLightModRange;

dfloat rendLightDistanceAttenuation = 924;
dint rendLightAttenuateFixedColormap = 1;

dfloat rendLightWallAngle = 1.2f; ///< Intensity of angle-based wall lighting.
dbyte rendLightWallAngleSmooth = true;

dfloat rendSkyLight = .273f;     ///< Intensity factor.
dbyte rendSkyLightAuto = true;
dint rendMaxLumobjs;             ///< Max lumobjs per viewer, per frame. @c 0= no maximum.

dint extraLight;  ///< Bumped light from gun blasts.
dfloat extraLightDelta;

DGLuint dlBBox;  ///< Display list id for the active-textured bbox model.

/*
 * Debug/Development cvars:
 */

dbyte devMobjVLights;            ///< @c 1= Draw mobj vertex lighting vector.
dint devMobjBBox;                ///< @c 1= Draw mobj bounding boxes.
dint devPolyobjBBox;             ///< @c 1= Draw polyobj bounding boxes.

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

dbyte devLightGrid;              ///< @c 1= Draw lightgrid debug visual.
dfloat devLightGridSize = 1.5f;  ///< Lightgrid debug visual size factor.

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
static Vector3d eyeOrigin;            ///< Viewer origin.
static ConvexSubspace *curSubspace;   ///< Subspace currently being drawn.
static Vector3f curSectorLightColor;
static dfloat curSectorLightLevel;
static bool firstSubspace;            ///< No range checking for the first one.

static inline RenderSystem &rendSys()
{
    return ClientApp::renderSystem();
}

static inline ResourceSystem &resSys()
{
    return ClientApp::resourceSystem();
}

static inline WorldSystem &worldSys()
{
    return ClientApp::worldSystem();
}

static void reportWallSectionDrawn(Line &line)
{
    // Already been here?
    dint playerNum = viewPlayer - ddPlayers;
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

static void scheduleFullLightGridUpdate()
{
    if(!worldSys().hasMap()) return;

    // Schedule a LightGrid update.
    if(worldSys().map().hasLightGrid())
    {
        worldSys().map().lightGrid().scheduleFullUpdate();
    }
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

dfloat Rend_FieldOfView()
{
    if(vrCfg().mode() == VRConfig::OculusRift)
    {
        // OVR tells us which FOV to use.
        return vrCfg().oculusRift().fovX();
    }
    else
    {
        // Correction is applied for wide screens so that when the FOV is kept
        // at a certain value (e.g., the default FOV), a 16:9 view has a wider angle
        // than a 4:3, but not just scaled linearly since that would go too far
        // into the fish eye territory.
        dfloat widescreenCorrection = dfloat(viewpw) / dfloat(viewph) / (4.f / 3.f);
        if(widescreenCorrection < 1.5)  // up to ~16:9
        {
            widescreenCorrection = (1 + 2 * widescreenCorrection) / 3;
            return de::clamp(1.f, widescreenCorrection * fieldOfView, 179.f);
        }
        // This is an unusually wide (perhaps multimonitor) setup, so just use the
        // configured FOV as is.
        return de::clamp(1.f, fieldOfView, 179.f);
    }
}

static Vector3d vEyeOrigin;

Vector3d Rend_EyeOrigin()
{
    return vEyeOrigin;
}

Matrix4f Rend_GetModelViewMatrix(dint consoleNum, bool inWorldSpace)
{
    viewdata_t const *viewData = R_ViewData(consoleNum);

    dfloat bodyAngle = viewData->current.angleWithoutHeadTracking() / (dfloat) ANGLE_MAX * 360 - 90;

    /// @todo vOrigin et al. shouldn't be changed in a getter function. -jk

    vOrigin = viewData->current.origin.xzy();
    vang    = viewData->current.angle() / (dfloat) ANGLE_MAX * 360 - 90;  // head tracking included
    vpitch  = viewData->current.pitch * 85.0 / 110.0;

    vEyeOrigin = vOrigin;

    OculusRift &ovr = vrCfg().oculusRift();
    bool const applyHead = (vrCfg().mode() == VRConfig::OculusRift && ovr.isReady());

    Matrix4f modelView;
    Matrix4f headOrientation;
    Matrix4f headOffset;

    if(applyHead)
    {
        Vector3f headPos = swizzle(Matrix4f::rotate(bodyAngle, Vector3f(0, 1, 0)) *
                                   ovr.headPosition() * vrCfg().mapUnitsPerMeter(),
                                   AxisNegX, AxisNegY, AxisZ);
        headOffset = Matrix4f::translate(headPos);

        vEyeOrigin -= headPos;
    }

    if(inWorldSpace)
    {
        dfloat yaw   = vang;
        dfloat pitch = vpitch;
        dfloat roll  = 0;

        /// @todo Elevate roll angle use into viewer_t, and maybe all the way up into player
        /// model.

        // Pitch and yaw can be taken directly from the head tracker, as the game is aware of
        // these values and is syncing with them independently (however, game has more
        // latency).
        if(applyHead)
        {
            // Use angles directly from the Rift for best response.
            Vector3f const pry = ovr.headOrientation();
            roll  = -radianToDegree(pry[1]);
            pitch =  radianToDegree(pry[0]);
        }

        headOrientation = Matrix4f::rotate(roll,  Vector3f(0, 0, 1)) *
                          Matrix4f::rotate(pitch, Vector3f(1, 0, 0)) *
                          Matrix4f::rotate(yaw,   Vector3f(0, 1, 0));

        modelView = headOrientation * headOffset;
    }

    if(applyHead)
    {
        // Apply the current eye offset to the eye origin.
        vEyeOrigin -= headOrientation.inverse() * (ovr.eyeOffset() * vrCfg().mapUnitsPerMeter());
    }

    return (modelView *
            Matrix4f::scale(Vector3f(1.0f, 1.2f, 1.0f)) * // This is the aspect correction.
            Matrix4f::translate(-vOrigin));
}

void Rend_ModelViewMatrix(bool inWorldSpace)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(Rend_GetModelViewMatrix(viewPlayer - ddPlayers, inWorldSpace).values());
}

static inline ddouble viewFacingDot(Vector2d const &v1, Vector2d const &v2)
{
    // The dot product.
    return (v1.y - v2.y) * (v1.x - Rend_EyeOrigin().x) + (v2.x - v1.x) * (v1.y - Rend_EyeOrigin().z);
}

dfloat Rend_ExtraLightDelta()
{
    return extraLightDelta;
}

void Rend_ApplyTorchLight(Vector4f &color, dfloat distance)
{
    ddplayer_t *ddpl = &viewPlayer->shared;

    // Disabled?
    if(!ddpl->fixedColorMap) return;

    // Check for torch.
    if(!rendLightAttenuateFixedColormap || distance < 1024)
    {
        // Colormap 1 is the brightest. I'm guessing 16 would be
        // the darkest.
        dfloat d = (16 - ddpl->fixedColorMap) / 15.0f;
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

void Rend_ApplyTorchLight(dfloat *color3, dfloat distance)
{
    Vector4f tmp(color3, 0);
    Rend_ApplyTorchLight(tmp, distance);
    for(dint i = 0; i < 3; ++i)
    {
        color3[i] = tmp[i];
    }
}

dfloat Rend_AttenuateLightLevel(dfloat distToViewer, dfloat lightLevel)
{
    if(distToViewer > 0 && rendLightDistanceAttenuation > 0)
    {
        dfloat real = lightLevel -
            (distToViewer - 32) / rendLightDistanceAttenuation *
                (1 - lightLevel);

        dfloat minimum = de::max(0.f, de::squared(lightLevel) + (lightLevel - .63f) * .5f);
        if(real < minimum)
            real = minimum; // Clamp it.

        return de::min(real, 1.f);
    }

    return lightLevel;
}

dfloat Rend_ShadowAttenuationFactor(coord_t distance)
{
    if(shadowMaxDistance > 0 && distance > 3 * shadowMaxDistance / 4)
    {
        return (shadowMaxDistance - distance) / (shadowMaxDistance / 4);
    }
    return 1;
}

static Vector3f skyLightColor;
static Vector3f oldSkyAmbientColor(-1.f, -1.f, -1.f);
static dfloat oldRendSkyLight = -1;

bool Rend_SkyLightIsEnabled()
{
    return rendSkyLight > .001f;
}

Vector3f Rend_SkyLightColor()
{
    if(Rend_SkyLightIsEnabled() && ClientApp::worldSystem().hasMap())
    {
        Sky &sky = ClientApp::worldSystem().map().sky();
        Vector3f const &ambientColor = sky.ambientColor();

        if(rendSkyLight != oldRendSkyLight ||
           !INRANGE_OF(ambientColor.x, oldSkyAmbientColor.x, .001f) ||
           !INRANGE_OF(ambientColor.y, oldSkyAmbientColor.y, .001f) ||
           !INRANGE_OF(ambientColor.z, oldSkyAmbientColor.z, .001f))
        {
            skyLightColor = ambientColor;
            R_AmplifyColor(skyLightColor);

            // Apply the intensity factor cvar.
            for(dint i = 0; i < 3; ++i)
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

/**
 * Determine the effective ambient light color for the given @a sector. Usually
 * one would obtain this info from SectorCluster, however in some situations the
 * correct light color is *not* that of the cluster (e.g., where map hacks use
 * mapped planes to reference another sector).
 */
static Vector3f Rend_AmbientLightColor(Sector const &sector)
{
    if(Rend_SkyLightIsEnabled() && sector.hasSkyMaskedPlane())
    {
        return Rend_SkyLightColor();
    }

    // A non-skylight sector (i.e., everything else!)
    // Return the sector's ambient light color.
    return sector.lightColor();
}

Vector3f Rend_LuminousColor(Vector3f const &color, dfloat light)
{
    light = de::clamp(0.f, light, 1.f) * dynlightFactor;

    // In fog additive blending is used; the normal fog color is way too bright.
    if(usingFog) light *= dynlightFogBright;

    // Multiply light with (ambient) color.
    return color * light;
}

coord_t Rend_PlaneGlowHeight(dfloat intensity)
{
    return de::clamp<ddouble>(0, GLOW_HEIGHT_MAX * intensity * glowHeightFactor, glowHeightMax);
}

Material *Rend_ChooseMapSurfaceMaterial(Surface const &surface)
{
    switch(renderTextures)
    {
    case 0:  // No texture mode.
    case 1:  // Normal mode.
        if(!(devNoTexFix && surface.hasFixMaterial()))
        {
            if(surface.hasMaterial() || surface.parent().type() != DMU_PLANE)
                return surface.materialPtr();
        }

        // Use special "missing" material.
        return &resSys().material(de::Uri("System", Path("missing")));

    case 2:  // Lighting debug mode.
        if(surface.hasMaterial() && !(!devNoTexFix && surface.hasFixMaterial()))
        {
            if(!surface.hasSkyMaskedMaterial() || devRendSkyMode)
            {
                // Use the special "gray" material.
                return &resSys().material(de::Uri("System", Path("gray")));
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
void Rend_AddMaskedPoly(Vector3f const *rvertices, Vector4f const *rcolors,
    coord_t wallLength, MaterialAnimator *matAnimator, Vector2f const &materialOrigin,
    blendmode_t blendMode, duint lightListIdx, dfloat glow)
{
    vissprite_t *vis = R_NewVisSprite(VSPR_MASKED_WALL);

    vis->pose.origin   = (rvertices[0] + rvertices[3]) / 2;
    vis->pose.distance = Rend_PointDist2D(vis->pose.origin);

    VS_WALL(vis)->texOffset[0] = materialOrigin[0];
    VS_WALL(vis)->texOffset[1] = materialOrigin[1];

    // Masked walls are sometimes used for special effects like arcs,
    // cobwebs and bottoms of sails. In order for them to look right,
    // we need to disable texture wrapping on the horizontal axis (S).
    // Most masked walls need wrapping, though. What we need to do is
    // look at the texture coordinates and see if they require texture
    // wrapping.
    if(renderTextures)
    {
        // Ensure we've up to date info about the material.
        matAnimator->prepare();

        Vector2i const &matDimensions = matAnimator->dimensions();

        VS_WALL(vis)->texCoord[0][0] = VS_WALL(vis)->texOffset[0] / matDimensions.x;
        VS_WALL(vis)->texCoord[1][0] = VS_WALL(vis)->texCoord[0][0] + wallLength / matDimensions.x;
        VS_WALL(vis)->texCoord[0][1] = VS_WALL(vis)->texOffset[1] / matDimensions.y;
        VS_WALL(vis)->texCoord[1][1] = VS_WALL(vis)->texCoord[0][1] +
                (rvertices[3].z - rvertices[0].z) / matDimensions.y;

        dint wrapS = GL_REPEAT, wrapT = GL_REPEAT;
        if(!matAnimator->isOpaque())
        {
            if(!(VS_WALL(vis)->texCoord[0][0] < 0 || VS_WALL(vis)->texCoord[0][0] > 1 ||
                 VS_WALL(vis)->texCoord[1][0] < 0 || VS_WALL(vis)->texCoord[1][0] > 1))
            {
                // Visible portion is within the actual [0..1] range.
                wrapS = GL_CLAMP_TO_EDGE;
            }

            // Clamp on the vertical axis if the coords are in the normal [0..1] range.
            if(!(VS_WALL(vis)->texCoord[0][1] < 0 || VS_WALL(vis)->texCoord[0][1] > 1 ||
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

    for(dint i = 0; i < 4; ++i)
    {
        VS_WALL(vis)->vertices[i].pos[0] = rvertices[i].x;
        VS_WALL(vis)->vertices[i].pos[1] = rvertices[i].y;
        VS_WALL(vis)->vertices[i].pos[2] = rvertices[i].z;

        for(dint c = 0; c < 4; ++c)
        {
            /// @todo Do not clamp here.
            VS_WALL(vis)->vertices[i].color[c] = de::clamp(0.f, rcolors[i][c], 1.f);
        }
    }

    /// @todo Semitransparent masked polys arn't lit atm
    if(glow < 1 && lightListIdx && numTexUnits > 1 && envModAdd &&
       !(rcolors[0].w < 1))
    {
        // The dynlights will have already been sorted so that the brightest
        // and largest of them is first in the list. So grab that one.
        rendSys().forAllSurfaceProjections(lightListIdx, [&vis] (ProjectedTextureData const &tp)
        {
            VS_WALL(vis)->modTex = tp.texture;
            VS_WALL(vis)->modTexCoord[0][0] = tp.topLeft.x;
            VS_WALL(vis)->modTexCoord[0][1] = tp.topLeft.y;
            VS_WALL(vis)->modTexCoord[1][0] = tp.bottomRight.x;
            VS_WALL(vis)->modTexCoord[1][1] = tp.bottomRight.y;
            for(dint c = 0; c < 4; ++c)
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

static void quadTexCoords(Vector2f *tc, Vector3f const *rverts, coord_t wallLength, Vector3d const &topLeft)
{
    DENG2_ASSERT(tc && rverts);
    tc[0].x = tc[1].x = rverts[0].x - topLeft.x;
    tc[3].y = tc[1].y = rverts[0].y - topLeft.y;
    tc[3].x = tc[2].x = tc[0].x + wallLength;
    tc[2].y = tc[3].y + (rverts[1].z - rverts[0].z);
    tc[0].y = tc[3].y + (rverts[3].z - rverts[2].z);
}

static void lightVertex(Vector4f &color, Vector3f const &vtx, dfloat lightLevel, Vector3f const &ambientColor)
{
    dfloat const dist = Rend_PointDist2D(vtx);

    // Apply distance attenuation.
    lightLevel = Rend_AttenuateLightLevel(dist, lightLevel);

    // Add extra light.
    lightLevel = de::clamp(0.f, lightLevel + Rend_ExtraLightDelta(), 1.f);

    Rend_ApplyLightAdaptation(lightLevel);

    for(dint i = 0; i < 3; ++i)
    {
        color[i] = lightLevel * ambientColor[i];
    }
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
static void lightWallOrFlatGeometry(Geometry &verts, duint numVertices, Vector3f const *posCoords,
    MapElement &mapElement, dint geomGroup, Matrix3f const &surfaceTangents,
    Vector3f const &color, Vector3f const *color2, dfloat glowing, dfloat const luminosityDeltas[2])
{
    bool const haveWall    = mapElement.is<LineSideSegment>();
    SectorCluster &cluster = ::curSubspace->cluster();

    // Uniform color?
    if(::levelFullBright || !(glowing < 1))
    {
        dfloat const lum = de::clamp(0.f, ::curSectorLightLevel + (::levelFullBright? 1 : glowing), 1.f);
        Vector4f const uniformColor(lum, lum, lum, 0);
        for(duint i = 0; i < numVertices; ++i)
        {
            verts.color[i] = uniformColor;
        }
        return;
    }

    if(::useBias)  // Bias lighting model.
    {
        Map &map     = cluster.sector().map();
        Shard &shard = cluster.shard(mapElement, geomGroup);

        // Apply the ambient light term from the grid (if available).
        if(map.hasLightGrid())
        {
            for(duint i = 0; i < numVertices; ++i)
            {
                verts.color[i] = map.lightGrid().evaluate(posCoords[i]);
            }
        }

        // Apply bias light source contributions.
        shard.lightWithBiasSources(posCoords, verts.color, surfaceTangents, map.biasCurrentTime());

        // Apply surface glow.
        if(glowing > 0)
        {
            Vector4f const glow(glowing, glowing, glowing, 0);
            for(duint i = 0; i < numVertices; ++i)
            {
                verts.color[i] += glow;
            }
        }

        // Apply light range compression and clamp.
        for(duint i = 0; i < numVertices; ++i)
        {
            Vector4f &color = verts.color[i];
            for(dint k = 0; k < 3; ++k)
            {
                color[k] = de::clamp(0.f, color[k] + Rend_LightAdaptationDelta(color[k]), 1.f);
            }
        }
    }
    else  // Doom lighting model.
    {
        // Blend sector light color with the surface color tint.
        Vector3f const colorBlended = ::curSectorLightColor * color;
        dfloat const lumLeft  = de::clamp(0.f, ::curSectorLightLevel + luminosityDeltas[0] + glowing, 1.f);
        dfloat const lumRight = de::clamp(0.f, ::curSectorLightLevel + luminosityDeltas[1] + glowing, 1.f);

        if(haveWall && !de::fequal(lumLeft, lumRight))
        {
            lightVertex(verts.color[0], posCoords[0], lumLeft,  colorBlended);
            lightVertex(verts.color[1], posCoords[1], lumLeft,  colorBlended);
            lightVertex(verts.color[2], posCoords[2], lumRight, colorBlended);
            lightVertex(verts.color[3], posCoords[3], lumRight, colorBlended);
        }
        else
        {
            for(duint i = 0; i < numVertices; ++i)
            {
                lightVertex(verts.color[i], posCoords[i], lumLeft, colorBlended);
            }
        }

        // Secondary color?
        if(haveWall && color2)
        {
            // Blend the secondary surface color tint with the sector light color.
            Vector3f const color2Blended = ::curSectorLightColor * (*color2);
            lightVertex(verts.color[0], posCoords[0], lumLeft,  color2Blended);
            lightVertex(verts.color[2], posCoords[2], lumRight, color2Blended);
        }
    }

    // Apply torch light?
    DENG2_ASSERT(::viewPlayer);
    if(::viewPlayer->shared.fixedColorMap)
    {
        for(duint i = 0; i < numVertices; ++i)
        {
            Rend_ApplyTorchLight(verts.color[i], Rend_PointDist2D(posCoords[i]));
        }
    }
}

static void makeFlatGeometry(Geometry &verts, duint numVertices, Vector3f const *posCoords,
    Vector3d const &topLeft, Vector3d const & /*bottomRight*/, MapElement &mapElement, dint geomGroup,
    Matrix3f const &surfaceTangents, dfloat uniformOpacity, Vector3f const &color, Vector3f const *color2,
    dfloat glowing, dfloat const luminosityDeltas[2], bool useVertexLighting = true)
{
    DENG2_ASSERT(posCoords);

    for(duint i = 0; i < numVertices; ++i)
    {
        verts.pos[i] = posCoords[i];

        Vector3f const delta(posCoords[i] - topLeft);
        if(verts.tex)  // Primary.
        {
            verts.tex[i] = Vector2f(delta.x, -delta.y);
        }
        if(verts.tex2)  // Inter.
        {
            verts.tex2[i] = Vector2f(delta.x, -delta.y);
        }
    }

    // Light the geometry?
    if(useVertexLighting)
    {
        lightWallOrFlatGeometry(verts, numVertices, posCoords, mapElement, geomGroup, surfaceTangents,
                                color, color2, glowing, luminosityDeltas);

        // Apply uniform opacity (overwritting luminance factors).
        for(duint i = 0; i < numVertices; ++i)
        {
            verts.color[i].w = uniformOpacity;
        }
    }
}

static void makeWallGeometry(Geometry &verts, duint numVertices, Vector3f const *posCoords,
    Vector3d const &topLeft, Vector3d const & /*bottomRight*/, coord_t sectionWidth,
    MapElement &mapElement, dint geomGroup, Matrix3f const &surfaceTangents, dfloat uniformOpacity,
    Vector3f const &color, Vector3f const *color2, dfloat glowing, dfloat const luminosityDeltas[2],
    bool useVertexLighting = true)
{
    DENG2_ASSERT(posCoords);

    for(duint i = 0; i < numVertices; ++i)
    {
        verts.pos[i] = posCoords[i];
    }
    if(verts.tex)  // Primary.
    {
        quadTexCoords(verts.tex, posCoords, sectionWidth, topLeft);
    }
    if(verts.tex2)  // Inter.
    {
        quadTexCoords(verts.tex2, posCoords, sectionWidth, topLeft);
    }

    // Light the geometry?
    if(useVertexLighting)
    {
        lightWallOrFlatGeometry(verts, numVertices, posCoords, mapElement, geomGroup, surfaceTangents,
                                color, color2, glowing, luminosityDeltas);

        // Apply uniform opacity (overwritting luminance factors).
        for(duint i = 0; i < numVertices; ++i)
        {
            verts.color[i].w = uniformOpacity;
        }
    }
}

static inline dfloat shineVertical(dfloat dy, dfloat dx)
{
    return ((std::atan(dy/dx) / (PI/2)) + 1) / 2;
}

static void makeFlatShineGeometry(Geometry &verts, duint numVertices, Vector3f const *posCoords,
    Geometry const &mainVerts, Vector3f const &shineColor, dfloat shineOpacity)
{
    for(duint i = 0; i < numVertices; ++i)
    {
        Vector3f const eye = Rend_EyeOrigin();

        // Determine distance to viewer.
        dfloat distToEye = (eye.xz() - Vector2f(posCoords[i])).normalize().length();
        // Short distances produce an ugly 'crunch' below and above the viewpoint.
        if(distToEye < 10) distToEye = 10;

        // Offset from the normal view plane.
        dfloat offset = ((eye.y - posCoords[i].y) * sin(.4f)/*viewFrontVec[0]*/ -
                         (eye.x - posCoords[i].x) * cos(.4f)/*viewFrontVec[2]*/);

        verts.tex[i] = Vector2f(((shineVertical(offset, distToEye) - .5f) * 2) + .5f,
                                shineVertical(eye.y - posCoords[i].z, distToEye));
    }

    for(duint i = 0; i < numVertices; ++i)
    {
        verts.color[i] = Vector4f(Vector3f(mainVerts.color[i]).max(shineColor), shineOpacity);
    }
}

static void makeWallShineGeometry(Geometry &verts, duint numVertices, Vector3f const *posCoords,
    Geometry const &mainVerts, coord_t sectionWidth, Vector3f const &shineColor, dfloat shineOpactiy)
{
    Vector3f const &topLeft     = posCoords[1];
    Vector3f const &bottomRight = posCoords[2];

    // Quad surface vector.
    Vector2f const normal = Vector2f( (bottomRight.y - topLeft.y) / sectionWidth,
                                     -(bottomRight.x - topLeft.x) / sectionWidth);

    // Calculate coordinates based on viewpoint and surface normal.
    Vector3f const eye = Rend_EyeOrigin();
    dfloat prevAngle = 0;
    for(duint i = 0; i < 2; ++i)
    {
        Vector2f const eyeToVert = eye.xz() - (i == 0? Vector2f(topLeft) : Vector2f(bottomRight));
        dfloat const distToEye   = eyeToVert.length();
        Vector2f const view      = eyeToVert.normalize();

        Vector2f projected;
        dfloat const div = normal.lengthSquared();
        if(div != 0)
        {
            projected = normal * view.dot(normal) / div;
        }

        Vector2f const reflected = view + (projected - view) * 2;
        dfloat angle = std::acos(reflected.y) / PI;
        if(reflected.x < 0)
        {
            angle = 1 - angle;
        }

        if(i == 0)
        {
            prevAngle = angle;
        }
        else if(angle > prevAngle)
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

    for(duint i = 0; i < numVertices; ++i)
    {
        verts.color[i] = Vector4f(Vector3f(mainVerts.color[i]).max(shineColor), shineOpactiy);
    }
}

static void makeFlatShadowGeometry(Geometry &verts, Vector3d const &topLeft, Vector3d const &bottomRight,
    Vector3f const *rvertices, duint numVertices,
    ProjectedTextureData const &tp)
{
    for(duint i = 0; i < numVertices; ++i)
    {
        verts.color[i] = tp.color;
    }

    dfloat const width  = bottomRight.x - topLeft.x;
    dfloat const height = bottomRight.y - topLeft.y;

    for(duint i = 0; i < numVertices; ++i)
    {
        verts.tex[i].x = ((bottomRight.x - rvertices[i].x) / width * tp.topLeft.x) +
            ((rvertices[i].x - topLeft.x) / width * tp.bottomRight.x);

        verts.tex[i].y = ((bottomRight.y - rvertices[i].y) / height * tp.topLeft.y) +
            ((rvertices[i].y - topLeft.y) / height * tp.bottomRight.y);
    }

    std::memcpy(verts.pos, rvertices, sizeof(Vector3f) * numVertices);
}

static void makeWallShadowGeometry(Geometry &verts, Vector3d const &topLeft, Vector3d const &bottomRight,
    Vector3f const *rvertices, duint numVertices, WallEdge const &leftEdge, WallEdge const &rightEdge,
    ProjectedTextureData const &tp)
{
    for(duint i = 0; i < numVertices; ++i)
    {
        verts.color[i] = tp.color;
    }

    verts.tex[1].x = verts.tex[0].x = tp.topLeft.x;
    verts.tex[1].y = verts.tex[3].y = tp.topLeft.y;
    verts.tex[3].x = verts.tex[2].x = tp.bottomRight.x;
    verts.tex[2].y = verts.tex[0].y = tp.bottomRight.y;

    // If either edge has divisions - make two trifans.
    if(leftEdge.divisionCount() || rightEdge.divisionCount())
    {
        // Need to swap indices around into fans set the position of the
        // division vertices, interpolate texcoords and color.

        Vector3f origposCoords[4]; std::memcpy(origposCoords, rvertices,   sizeof(Vector3f) * 4);
        Vector2f origTexCoords[4]; std::memcpy(origTexCoords, verts.tex,   sizeof(Vector2f) * 4);
        Vector4f origColors[4];    std::memcpy(origColors,    verts.color, sizeof(Vector4f) * 4);

        R_DivVerts     (verts.pos,   origposCoords, leftEdge, rightEdge);
        R_DivTexCoords (verts.tex,   origTexCoords, leftEdge, rightEdge);
        R_DivVertColors(verts.color, origColors,    leftEdge, rightEdge);
    }
    else
    {
        std::memcpy(verts.pos, rvertices, sizeof(Vector3f) * numVertices);
    }
}

static void makeFlatLightGeometry(Geometry &verts, Vector3d const &topLeft, Vector3d const &bottomRight,
    Vector3f const *rvertices, duint numVertices,
    ProjectedTextureData const &tp)
{
    for(duint i = 0; i < numVertices; ++i)
    {
        verts.color[i] = tp.color;
    }

    dfloat const width  = bottomRight.x - topLeft.x;
    dfloat const height = bottomRight.y - topLeft.y;
    for(duint i = 0; i < numVertices; ++i)
    {
        verts.tex[i].x = ((bottomRight.x - rvertices[i].x) / width * tp.topLeft.x) +
            ((rvertices[i].x - topLeft.x) / width * tp.bottomRight.x);

        verts.tex[i].y = ((bottomRight.y - rvertices[i].y) / height * tp.topLeft.y) +
            ((rvertices[i].y - topLeft.y) / height * tp.bottomRight.y);
    }

    std::memcpy(verts.pos, rvertices, sizeof(Vector3f) * numVertices);
}

static void makeWallLightGeometry(Geometry &verts, Vector3d const &topLeft, Vector3d const &bottomRight,
    Vector3f const *rvertices, duint numVertices, WallEdge const &leftEdge, WallEdge const &rightEdge,
    ProjectedTextureData const &tp)
{
    for(duint i = 0; i < numVertices; ++i)
    {
        verts.color[i] = tp.color;
    }

    verts.tex[1].x = verts.tex[0].x = tp.topLeft.x;
    verts.tex[1].y = verts.tex[3].y = tp.topLeft.y;
    verts.tex[3].x = verts.tex[2].x = tp.bottomRight.x;
    verts.tex[2].y = verts.tex[0].y = tp.bottomRight.y;

    // If either edge has divisions - make two trifans.
    if(leftEdge.divisionCount() || rightEdge.divisionCount())
    {
        // Need to swap indices around into fans set the position
        // of the division vertices, interpolate texcoords and color.

        Vector3f origPosCoords[4]; std::memcpy(origPosCoords, rvertices,   sizeof(Vector3f) * 4);
        Vector2f origTexCoords[4]; std::memcpy(origTexCoords, verts.tex,   sizeof(Vector2f) * 4);
        Vector4f origColors[4];    std::memcpy(origColors,    verts.color, sizeof(Vector4f) * 4);

        R_DivVerts     (verts.pos,   origPosCoords, leftEdge, rightEdge);
        R_DivTexCoords (verts.tex,   origTexCoords, leftEdge, rightEdge);
        R_DivVertColors(verts.color, origColors,    leftEdge, rightEdge);
    }
    else
    {
        std::memcpy(verts.pos, rvertices, sizeof(Vector3f) * numVertices);
    }
}

static dfloat averageLuminosity(Vector4f const *rgbaValues, duint count)
{
    DENG2_ASSERT(rgbaValues);
    dfloat avg = 0;
    for(duint i = 0; i < count; ++i)
    {
        Vector4f const &color = rgbaValues[i];
        avg += color.x + color.y + color.z;
    }
    return avg / (count * 3);
}

struct rendworldpoly_params_t
{
    bool            skyMasked;
    blendmode_t     blendMode;
    Vector3d const *topLeft;
    Vector3d const *bottomRight;
    Vector2f const *materialOrigin;
    Vector2f const *materialScale;
    dfloat          alpha;
    dfloat          surfaceLuminosityDeltas[2];
    Vector3f const *surfaceColor;
    Matrix3f const *surfaceTangentMatrix;

    duint           lightListIdx;   ///< List of lights that affect this poly.
    duint           shadowListIdx;  ///< List of shadows that affect this poly.
    dfloat          glowing;
    bool            forceOpaque;
    MapElement     *mapElement;
    dint            geomGroup;

    bool            isWall;
// Wall only:
    struct {
        coord_t sectionWidth;
        Vector3f const *surfaceColor2;  ///< Secondary color.
        WallEdge const *leftEdge;
        WallEdge const *rightEdge;
    } wall;
};

static bool renderWorldPoly(Vector3f const *rvertices, duint numVertices,
    rendworldpoly_params_t const &p, MaterialAnimator &matAnimator)
{
    DENG2_ASSERT(rvertices);

    SectorCluster &cluster = curSubspace->cluster();

    // Ensure we've up to date info about the material.
    matAnimator.prepare();

    bool const skyMaskedMaterial        = (p.skyMasked || (matAnimator.material().isSkyMasked()));
    bool const drawAsVisSprite          = (!p.forceOpaque && !p.skyMasked && (!matAnimator.isOpaque() || p.alpha < 1 || p.blendMode > 0));

    // Map RTU configuration.
    GLTextureUnit const *layer0RTU      = (!p.skyMasked)? &matAnimator.texUnit(MaterialAnimator::TU_LAYER0) : nullptr;
    GLTextureUnit const *layer0InterRTU = (!p.skyMasked && !drawAsVisSprite && matAnimator.texUnit(MaterialAnimator::TU_LAYER0_INTER).hasTexture())? &matAnimator.texUnit(MaterialAnimator::TU_LAYER0_INTER) : nullptr;
    GLTextureUnit const *detailRTU      = (r_detail && !p.skyMasked && matAnimator.texUnit(MaterialAnimator::TU_DETAIL).hasTexture())? &matAnimator.texUnit(MaterialAnimator::TU_DETAIL) : nullptr;
    GLTextureUnit const *detailInterRTU = (r_detail && !p.skyMasked && matAnimator.texUnit(MaterialAnimator::TU_DETAIL_INTER).hasTexture())? &matAnimator.texUnit(MaterialAnimator::TU_DETAIL_INTER) : nullptr;

    GLTextureUnit const *shineRTU       = (::useShinySurfaces && !skyMaskedMaterial && !drawAsVisSprite && matAnimator.texUnit(MaterialAnimator::TU_SHINE).hasTexture())? &matAnimator.texUnit(MaterialAnimator::TU_SHINE) : nullptr;
    GLTextureUnit const *shineMaskRTU   = (::useShinySurfaces && !skyMaskedMaterial && !drawAsVisSprite && matAnimator.texUnit(MaterialAnimator::TU_SHINE).hasTexture() && matAnimator.texUnit(MaterialAnimator::TU_SHINE_MASK).hasTexture())? &matAnimator.texUnit(MaterialAnimator::TU_SHINE_MASK) : nullptr;

    // Make surface geometry (position, primary texture, inter texture and color coords).
    Geometry verts;
    duint const numVerts = (p.isWall ? 3 + p.wall.leftEdge->divisionCount() + 3 + p.wall.rightEdge->divisionCount() : numVertices);
    // Allocate vertices from the pools.
    verts.pos   = R_AllocRendVertices(numVerts);
    verts.color = !skyMaskedMaterial? R_AllocRendColors   (numVerts) : nullptr;
    verts.tex   = layer0RTU         ? R_AllocRendTexCoords(numVerts) : nullptr;
    verts.tex2  = layer0InterRTU    ? R_AllocRendTexCoords(numVerts) : nullptr;
    if(p.isWall)
    {
        makeWallGeometry(verts, numVertices, rvertices, *p.topLeft, *p.bottomRight, p.wall.sectionWidth,
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

    if(drawAsVisSprite)
    {
        DENG2_ASSERT(p.isWall);

        // Masked polys (walls) get a special treatment (=> vissprite). This is
        // needed because all masked polys must be sorted (sprites are masked
        // polys). Otherwise there will be artifacts.
        Rend_AddMaskedPoly(verts.pos, verts.color, p.wall.sectionWidth, &matAnimator,
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
    if(useLights || useShadows)
    {
        dfloat const avg = averageLuminosity(verts.color, numVertices);
        if(avg > 0.98f)  // Fully bright.
        {
            useLights = false;  // Skip lights.
        }
        if(avg < 0.02f)  // Fully dark.
        {
            useShadows = false;  // Skip shadows.
        }
    }

    // If multitexturing is enabled and there is at least one dynlight affecting the surface,
    // locate the projection data and use it for modulation.
    TexModulationData mod;
    Vector2f *modTexCoords = nullptr;
    if(useLights && Rend_IsMTexLights())
    {
        rendSys().forAllSurfaceProjections(p.lightListIdx, [&mod] (ProjectedTextureData const &dyn)
        {
            mod.texture     = dyn.texture;
            mod.color       = dyn.color;
            mod.topLeft     = dyn.topLeft;
            mod.bottomRight = dyn.bottomRight;
            return LoopAbort;
        });

        if(mod.texture)
        {
            modTexCoords = R_AllocRendTexCoords(numVerts);
                
            // Modulation texture coordinates.
            if(p.isWall)
            {
                modTexCoords[0] = Vector2f(mod.topLeft.x, mod.bottomRight.y);
                modTexCoords[1] = mod.topLeft;
                modTexCoords[2] = mod.bottomRight;
                modTexCoords[3] = Vector2f(mod.bottomRight.x, mod.topLeft.y);
            }
            else
            {
                for(duint i = 0; i < numVertices; ++i)
                {
                    modTexCoords[i] = (( Vector2f(*p.bottomRight) - Vector2f(rvertices[i]) ) / ( Vector2f(*p.bottomRight) - Vector2f(*p.topLeft) ) * mod.topLeft    )
                                    + (( Vector2f(rvertices[i]  ) - Vector2f(*p.topLeft  ) ) / ( Vector2f(*p.bottomRight) - Vector2f(*p.topLeft) ) * mod.bottomRight);
                }
            }
        }
    }

    bool hasDynlights = false;
    if(useLights)
    {
        // Write projected lights.
        // Multitexturing can be used for the first light.
        bool const skipFirst = Rend_IsMTexLights();

        duint numProcessed = 0;
        rendSys().forAllSurfaceProjections(p.lightListIdx,
                                           [&p, &rvertices, &numVertices, &skipFirst, &numProcessed]
                                           (ProjectedTextureData const &tp)
        {
            if(!(skipFirst && numProcessed == 0))
            {
                // Light texture determines the list to write to.
                DrawListSpec listSpec;
                listSpec.group = LightGeom;
                listSpec.texunits[TU_PRIMARY] = GLTextureUnit(tp.texture, gl::ClampToEdge, gl::ClampToEdge);

                // Make geometry.
                Geometry verts;
                duint const numVerts = p.isWall ? 3 + p.wall.leftEdge->divisionCount() + 3 + p.wall.rightEdge->divisionCount()
                                                : numVertices;
                // Allocate verts from the pools.
                verts.pos   = R_AllocRendVertices (numVerts);
                verts.color = R_AllocRendColors   (numVerts);
                verts.tex   = R_AllocRendTexCoords(numVerts);
                if(p.isWall)
                {
                    makeWallLightGeometry(verts, *p.topLeft, *p.bottomRight,
                                          rvertices, numVertices, *p.wall.leftEdge, *p.wall.rightEdge, tp);
                }
                else
                {
                    makeFlatLightGeometry(verts, *p.topLeft, *p.bottomRight,
                                          rvertices, numVertices, tp);
                }

                // Write geometry.
                // Walls with edge divisions mean two trifans.
                if(p.isWall && (p.wall.leftEdge->divisionCount() || p.wall.rightEdge->divisionCount()))
                {
                    duint const numLeftVerts  = 3 + p.wall.leftEdge->divisionCount();
                    duint const numRightVerts = 3 + p.wall.rightEdge->divisionCount();

                    rendSys().drawLists().find(listSpec)
                                 .write(gl::TriangleFan, BM_NORMAL,
                                        Vector2f(1, 1), Vector2f(0, 0),
                                        Vector2f(1, 1), Vector2f(0, 0), false /*not lit*/,
                                        numRightVerts,
                                        verts.pos   + numLeftVerts,
                                        verts.color + numLeftVerts,
                                        verts.tex   + numLeftVerts)
                                 .write(gl::TriangleFan, BM_NORMAL,
                                        Vector2f(1, 1), Vector2f(0, 0),
                                        Vector2f(1, 1), Vector2f(0, 0), false /*not lit*/,
                                        numLeftVerts,
                                        verts.pos, verts.color, verts.tex);
                }
                else
                {
                    rendSys().drawLists().find(listSpec)
                                 .write(p.isWall ? gl::TriangleStrip : gl::TriangleFan, BM_NORMAL,
                                        Vector2f(1, 1), Vector2f(0, 0),
                                        Vector2f(1, 1), Vector2f(0, 0), false /*not lit*/,
                                        numVertices,
                                        verts.pos, verts.color, verts.tex);
                }

                // We're done with the geometry.
                R_FreeRendVertices (verts.pos);
                R_FreeRendColors   (verts.color);
                R_FreeRendTexCoords(verts.tex);
            }
            numProcessed += 1;
            return LoopContinue;
        });

        hasDynlights = (numProcessed > duint(skipFirst? 1 : 0));
    }

    if(useShadows)
    {
        // Write projected shadows.
        // All shadows use the same texture (so use the same list).
        DrawListSpec listSpec;
        listSpec.group = ShadowGeom;
        listSpec.texunits[TU_PRIMARY] = GLTextureUnit(GL_PrepareLSTexture(LST_DYNAMIC), gl::ClampToEdge, gl::ClampToEdge);
        DrawList &shadowList = rendSys().drawLists().find(listSpec);

        rendSys().forAllSurfaceProjections(p.shadowListIdx,
                                           [&p, &rvertices, &numVertices, &shadowList]
                                           (ProjectedTextureData const &tp)
        {
            // Make geometry.
            Geometry verts;
            duint const numVerts = p.isWall ? 3 + p.wall.leftEdge->divisionCount() + 3 + p.wall.rightEdge->divisionCount()
                                            : numVertices;
            // Allocate verts from the pools.
            verts.pos   = R_AllocRendVertices (numVerts);
            verts.color = R_AllocRendColors   (numVerts);
            verts.tex   = R_AllocRendTexCoords(numVerts);
            if(p.isWall)
            {
                makeWallShadowGeometry(verts, *p.topLeft, *p.bottomRight,
                                       rvertices, numVertices, *p.wall.leftEdge, *p.wall.rightEdge, tp);
            }
            else
            {
                makeFlatShadowGeometry(verts, *p.topLeft, *p.bottomRight,
                                       rvertices, numVertices, tp);
            }

            // Write geometry.
            // Walls with edge divisions mean two trifans.
            if(p.isWall && (p.wall.leftEdge->divisionCount() || p.wall.rightEdge->divisionCount()))
            {
                duint const numLeftVerts  = 3 + p.wall.leftEdge->divisionCount();
                duint const numRightVerts = 3 + p.wall.rightEdge->divisionCount();

                shadowList.write(gl::TriangleFan, BM_NORMAL,
                                 Vector2f(1, 1), Vector2f(0, 0),
                                 Vector2f(1, 1), Vector2f(0, 0), false /*not lit*/,
                                 numRightVerts,
                                 verts.pos   + numLeftVerts,
                                 verts.color + numLeftVerts,
                                 verts.tex   + numLeftVerts)
                          .write(gl::TriangleFan, BM_NORMAL,
                                 Vector2f(1, 1), Vector2f(0, 0),
                                 Vector2f(1, 1), Vector2f(0, 0), false /*not lit*/,
                                 numLeftVerts,
                                 verts.pos, verts.color, verts.tex);
            }
            else
            {
                shadowList.write(p.isWall ? gl::TriangleStrip : gl::TriangleFan, BM_NORMAL,
                                 Vector2f(1, 1), Vector2f(0, 0),
                                 Vector2f(1, 1), Vector2f(0, 0), false /*not lit*/,
                                 numVerts,
                                 verts.pos, verts.color, verts.tex);
            }

            // We're done with the geometry.
            R_FreeRendVertices (verts.pos);
            R_FreeRendColors   (verts.color);
            R_FreeRendTexCoords(verts.tex);

            return LoopContinue;
        });
    }

    // Make shine geometry.
    // Surface shine geometry (primary texture and color coords).
    // Use the surface geometry for position coords.
    // Use the surface texture coords with the mask.
    Geometry shineVerts = Geometry();
    if(shineRTU)
    {
        Vector3f const &shineColor = matAnimator.shineMinColor();  // Shine strength.
        dfloat const shineOpacity  = shineRTU->opacity;

        // Allocate vertices from the pools.
        shineVerts.color = R_AllocRendColors(numVerts);
        shineVerts.tex   = R_AllocRendTexCoords(numVerts);

        if(p.isWall)
        {
            makeWallShineGeometry(shineVerts, numVertices, rvertices, verts, p.wall.sectionWidth,
                                  shineColor, shineOpacity);
        }
        else
        {
            makeFlatShineGeometry(shineVerts, numVertices, rvertices, verts,
                                  shineColor, shineOpacity);
        }
    }

    // Write geometry.
    // Walls with edge divisions mean two trifans.
    if(p.isWall && (p.wall.leftEdge->divisionCount() || p.wall.rightEdge->divisionCount()))
    {
        WallEdge const &leftEdge  = *p.wall.leftEdge;
        WallEdge const &rightEdge = *p.wall.rightEdge;

        // Need to swap indices around into fans set the position of the division
        // vertices, interpolate texcoords and color.

        Vector3f origPos[4]; std::memcpy(origPos, verts.pos, sizeof(origPos));
        R_DivVerts(verts.pos, origPos, leftEdge, rightEdge);

        if(verts.color)
        {
            Vector4f orig[4]; std::memcpy(orig, verts.color, sizeof(orig));
            R_DivVertColors(verts.color, orig, leftEdge, rightEdge);
        }

        if(verts.tex)
        {
            Vector2f orig[4]; std::memcpy(orig, verts.tex, sizeof(orig));
            R_DivTexCoords(verts.tex, orig, leftEdge, rightEdge);
        }
        if(verts.tex2)
        {
            Vector2f orig[4]; std::memcpy(orig, verts.tex2, sizeof(orig));
            R_DivTexCoords(verts.tex2, orig, leftEdge, rightEdge);
        }

        if(modTexCoords)
        {
            Vector2f orig[4]; std::memcpy(orig, modTexCoords, sizeof(orig));
            R_DivTexCoords(modTexCoords, orig, leftEdge, rightEdge);
        }

        if(p.skyMasked)
        {
            rendSys().drawLists().find(DrawListSpec(SkyMaskGeom))
                          .write(gl::TriangleFan, BM_NORMAL,
                                 Vector2f(1, 1), Vector2f(0, 0),
                                 Vector2f(1, 1), Vector2f(0, 0), false /*not lit*/,
                                 3 + rightEdge.divisionCount(),
                                 verts.pos + 3 + leftEdge.divisionCount())
                          .write(gl::TriangleFan, BM_NORMAL,
                                 Vector2f(1, 1), Vector2f(0, 0),
                                 Vector2f(1, 1), Vector2f(0, 0), false /*not lit*/,
                                 3 + leftEdge.divisionCount(),
                                 verts.pos);
        }
        else
        {
            DrawListSpec listSpec((mod.texture || hasDynlights)? LitGeom : UnlitGeom);
            if(layer0RTU)
            {
                listSpec.texunits[TU_PRIMARY] = *layer0RTU;
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
            if(layer0InterRTU)
            {
                listSpec.texunits[TU_INTER] = *layer0InterRTU;
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
            if(detailRTU)
            {
                listSpec.texunits[TU_PRIMARY_DETAIL] = *detailRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY_DETAIL].offset += *p.materialOrigin;
                }
            }
            if(detailInterRTU)
            {
                listSpec.texunits[TU_INTER_DETAIL] = *detailInterRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_INTER_DETAIL].offset += *p.materialOrigin;
                }
            }

            rendSys().drawLists().find(listSpec)
                          .write(gl::TriangleFan, BM_NORMAL,
                                 listSpec.unit(TU_PRIMARY       ).scale,
                                 listSpec.unit(TU_PRIMARY       ).offset,
                                 listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                 listSpec.unit(TU_PRIMARY_DETAIL).offset, hasDynlights,
                                 3 + rightEdge.divisionCount(),
                                 verts.pos + 3 + leftEdge.divisionCount(),
                                 verts.color? verts.color + 3 + leftEdge.divisionCount() : 0,
                                 verts.tex  ? verts.tex   + 3 + leftEdge.divisionCount() : 0,
                                 verts.tex2 ? verts.tex2  + 3 + leftEdge.divisionCount() : 0,
                                 mod.texture, &mod.color, modTexCoords? modTexCoords + 3 + leftEdge.divisionCount() : 0)
                          .write(gl::TriangleFan, BM_NORMAL,
                                 listSpec.unit(TU_PRIMARY       ).scale,
                                 listSpec.unit(TU_PRIMARY       ).offset,
                                 listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                 listSpec.unit(TU_PRIMARY_DETAIL).offset, hasDynlights,
                                 3 + leftEdge.divisionCount(),
                                 verts.pos, verts.color, verts.tex,
                                 verts.tex2, mod.texture, &mod.color, modTexCoords);
        }
    }
    else
    {
        if(p.skyMasked)
        {
            rendSys().drawLists().find(DrawListSpec(SkyMaskGeom))
                          .write(p.isWall? gl::TriangleStrip : gl::TriangleFan, BM_NORMAL,
                                 Vector2f(1, 1), Vector2f(0, 0),
                                 Vector2f(1, 1), Vector2f(0, 0), false /*not lit*/,
                                 numVertices, verts.pos);
        }
        else
        {
            DrawListSpec listSpec((mod.texture || hasDynlights)? LitGeom : UnlitGeom);
            if(layer0RTU)
            {
                listSpec.texunits[TU_PRIMARY] = *layer0RTU;
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
            if(layer0InterRTU)
            {
                listSpec.texunits[TU_INTER] = *layer0InterRTU;
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
            if(detailRTU)
            {
                listSpec.texunits[TU_PRIMARY_DETAIL] = *detailRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY_DETAIL].offset += *p.materialOrigin;
                }
            }
            if(detailInterRTU)
            {
                listSpec.texunits[TU_INTER_DETAIL] = *detailInterRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_INTER_DETAIL].offset += *p.materialOrigin;
                }
            }

            rendSys().drawLists().find(listSpec)
                          .write(p.isWall? gl::TriangleStrip : gl::TriangleFan, BM_NORMAL,
                                 listSpec.unit(TU_PRIMARY       ).scale,
                                 listSpec.unit(TU_PRIMARY       ).offset,
                                 listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                 listSpec.unit(TU_PRIMARY_DETAIL).offset, hasDynlights,
                                 numVertices, verts.pos, verts.color, verts.tex, verts.tex2,
                                 mod.texture, &mod.color, modTexCoords);
        }
    }

    if(shineRTU)
    {
        // Write shine geometry.
        DrawListSpec listSpec(ShineGeom);
        listSpec.texunits[TU_PRIMARY] = *shineRTU;
        if(shineMaskRTU)
        {
            listSpec.texunits[TU_INTER] = *shineMaskRTU;
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

        // Walls with edge divisions mean two trifans.
        if(p.isWall && (p.wall.leftEdge->divisionCount() || p.wall.rightEdge->divisionCount()))
        {
            WallEdge const &leftEdge  = *p.wall.leftEdge;
            WallEdge const &rightEdge = *p.wall.rightEdge;

            if(shineVerts.tex)
            {
                Vector2f orig[4]; std::memcpy(orig, shineVerts.tex, sizeof(orig));
                R_DivTexCoords(shineVerts.tex, orig, leftEdge, rightEdge);
            }
            if(shineVerts.color)
            {
                Vector4f orig[4]; std::memcpy(orig, shineVerts.color, sizeof(orig));
                R_DivVertColors(shineVerts.color, orig, leftEdge, rightEdge);
            }

            rendSys().drawLists().find(listSpec)
                            .write(gl::TriangleFan, matAnimator.shineBlendMode(),
                                   listSpec.unit(TU_INTER).scale,
                                   listSpec.unit(TU_INTER).offset,
                                   Vector2f(1, 1), Vector2f(0, 0), false /*not lit*/,
                                   3 + rightEdge.divisionCount(),
                                   verts.pos + 3 + leftEdge.divisionCount(),
                                   shineVerts.color + 3 + leftEdge.divisionCount(),
                                   shineVerts.tex ? shineVerts.tex + 3 + leftEdge.divisionCount() : 0,
                                   shineMaskRTU? verts.tex + 3 + leftEdge.divisionCount() : 0)
                            .write(gl::TriangleFan, matAnimator.shineBlendMode(),
                                   listSpec.unit(TU_INTER).scale,
                                   listSpec.unit(TU_INTER).offset,
                                   Vector2f(1, 1), Vector2f(0, 0), false /*not lit*/,
                                   3 + leftEdge.divisionCount(),
                                   verts.pos, shineVerts.color, shineVerts.tex,
                                   shineMaskRTU? verts.tex : 0);
        }
        else
        {
            rendSys().drawLists().find(listSpec)
                            .write(p.isWall? gl::TriangleStrip : gl::TriangleFan, matAnimator.shineBlendMode(),
                                   listSpec.unit(TU_INTER).scale,
                                   listSpec.unit(TU_INTER).offset,
                                   Vector2f(1, 1), Vector2f(0, 0), false /*not lit*/,
                                   numVertices,
                                   verts.pos, shineVerts.color, shineVerts.tex,
                                   shineMaskRTU? verts.tex : 0);
        }
    }

    // We're done with the geometry.
    R_FreeRendTexCoords(modTexCoords);

    R_FreeRendVertices (verts.pos);
    R_FreeRendColors   (verts.color);
    R_FreeRendTexCoords(verts.tex);
    R_FreeRendTexCoords(verts.tex2);

    R_FreeRendColors   (shineVerts.color);
    R_FreeRendTexCoords(shineVerts.tex);

    return (p.forceOpaque || skyMaskedMaterial ||
            !(p.alpha < 1 || !matAnimator.isOpaque() || p.blendMode > 0));
}

static Lumobj::LightmapSemantic lightmapForSurface(Surface const &surface)
{
    if(surface.parent().type() == DMU_SIDE) return Lumobj::Side;
    // Must be a plane then.
    auto const &plane = surface.parent().as<Plane>();
    return plane.isSectorFloor()? Lumobj::Down : Lumobj::Up;
}

static DGLuint prepareLightmap(Texture *tex = nullptr)
{
    if(tex)
    {
        if(TextureVariant *variant = tex->prepareVariant(Rend_MapSurfaceLightmapTextureSpec()))
        {
            return variant->glName();
        }
        // Dang...
    }
    // Prepare the default/fallback lightmap instead.
    return GL_PrepareLSTexture(LST_DYNAMIC);
}

static bool projectDynlight(Vector3d const &topLeft, Vector3d const &bottomRight,
    Lumobj const &lum, Surface const &surface, dfloat blendFactor,
    ProjectedTextureData &projected)
{
    if(blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return false;

    // Has this already been occluded?
    if(R_ViewerLumobjIsHidden(lum.indexInMap()))
        return false;

    // No lightmap texture?
    DGLuint tex = prepareLightmap(lum.lightmap(lightmapForSurface(surface)));
    if(!tex) return false;

    Vector3d lumCenter = lum.origin();
    lumCenter.z += lum.zOffset();

    // On the right side?
    Vector3d topLeftToLum = topLeft - lumCenter;
    if(topLeftToLum.dot(surface.tangentMatrix().column(2)) > 0.f)
        return false;

    // Calculate 3D distance between surface and lumobj.
    Vector3d pointOnPlane = R_ClosestPointOnPlane(surface.tangentMatrix().column(2)/*normal*/,
                                                  topLeft, lumCenter);

    coord_t distToLum = (lumCenter - pointOnPlane).length();
    if(distToLum <= 0 || distToLum > lum.radius())
        return false;

    // Calculate the final surface light attribution factor.
    dfloat luma = 1.5f - 1.5f * distToLum / lum.radius();

    // Fade out as distance from viewer increases.
    luma *= lum.attenuation(R_ViewerLumobjDistance(lum.indexInMap()));

    // Would this be seen?
    if(luma * blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return false;

    // Project, counteracting aspect correction slightly.
    Vector2f s, t;
    dfloat const scale = 1.0f / ((2.f * lum.radius()) - distToLum);
    if(!R_GenerateTexCoords(s, t, pointOnPlane, scale, scale * 1.08f,
                            topLeft, bottomRight, surface.tangentMatrix()))
        return false;

    de::zap(projected);
    projected.texture     = tex;
    projected.topLeft     = Vector2f(s[0], t[0]);
    projected.bottomRight = Vector2f(s[1], t[1]);
    projected.color       = Vector4f(Rend_LuminousColor(lum.color(), luma), blendFactor);

    return true;
}

static bool projectPlaneGlow(Vector3d const &topLeft, Vector3d const &bottomRight,
    Plane const &plane, Vector3d const &pointOnPlane, dfloat blendFactor,
    ProjectedTextureData &projected)
{
    if(blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return false;

    Surface const &surface = plane.surface();
    Vector3f color;
    dfloat intensity = surface.glow(color);

    // Is the material glowing at this moment?
    if(intensity < .05f)
        return false;

    coord_t glowHeight = Rend_PlaneGlowHeight(intensity);
    if(glowHeight < 2) return false;  // Not too small!

    // Calculate coords.
    dfloat bottom, top;
    if(surface.normal().z < 0)
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
    if(!(bottom <= 1 || top >= 0)) return false;

    de::zap(projected);
    projected.texture     = GL_PrepareLSTexture(LST_GRADIENT);
    projected.topLeft     = Vector2f(0, bottom);
    projected.bottomRight = Vector2f(1, top);
    projected.color       = Vector4f(Rend_LuminousColor(color, intensity), blendFactor);
    return true;
}

static bool projectShadow(Vector3d const &topLeft, Vector3d const &bottomRight,
    mobj_t const &mob, Surface const &surface, dfloat blendFactor,
    ProjectedTextureData &projected)
{
    static Vector3f const black;  // shadows are black

    coord_t mobOrigin[3];
    Mobj_OriginSmoothed(const_cast<mobj_t *>(&mob), mobOrigin);

    // Is this too far?
    coord_t distanceFromViewer = 0;
    if(shadowMaxDistance > 0)
    {
        distanceFromViewer = Rend_PointDist2D(mobOrigin);
        if(distanceFromViewer > shadowMaxDistance)
            return LoopContinue;
    }

    // Should this mobj even have a shadow?
    dfloat shadowStrength = Mobj_ShadowStrength(mob) * ::shadowFactor;
    if(::usingFog) shadowStrength /= 2;
    if(shadowStrength <= 0) return LoopContinue;

    coord_t shadowRadius = Mobj_ShadowRadius(mob);
    if(shadowRadius > ::shadowMaxRadius)
        shadowRadius = ::shadowMaxRadius;
    if(shadowRadius <= 0) return LoopContinue;

    mobOrigin[2] -= mob.floorClip;
    if(mob.ddFlags & DDMF_BOB)
        mobOrigin[2] -= Mobj_BobOffset(mob);

    coord_t mobHeight = mob.height;
    if(!mobHeight) mobHeight = 1;

    // If this were a light this is where we would check whether the origin is on
    // the right side of the surface. However this is a shadow and light is moving
    // in the opposite direction (inward toward the mobj's origin), therefore this
    // has "volume/depth".

    // Calculate 3D distance between surface and mobj.
    Vector3d point = R_ClosestPointOnPlane(surface.tangentMatrix().column(2)/*normal*/,
                                           topLeft, mobOrigin);
    coord_t distFromSurface = (Vector3d(mobOrigin) - Vector3d(point)).length();

    // Too far above or below the shadowed surface?
    if(distFromSurface > mob.height)
        return false;
    if(mobOrigin[2] + mob.height < point.z)
        return false;
    if(distFromSurface > shadowRadius)
        return false;

    // Calculate the final strength of the shadow's attribution to the surface.
    shadowStrength *= 1.5f - 1.5f * distFromSurface / shadowRadius;

    // Fade at half mobj height for smooth fade out when embedded in the surface.
    coord_t halfMobjHeight = mobHeight / 2;
    if(distFromSurface > halfMobjHeight)
    {
        shadowStrength *= 1 - (distFromSurface - halfMobjHeight) / (mobHeight - halfMobjHeight);
    }

    // Fade when nearing the maximum distance?
    shadowStrength *= Rend_ShadowAttenuationFactor(distanceFromViewer);
    shadowStrength *= blendFactor;

    // Would this shadow be seen?
    if(shadowStrength < SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return false;

    // Project, counteracting aspect correction slightly.
    Vector2f s, t;
    dfloat const scale = 1.0f / ((2.f * shadowRadius) - distFromSurface);
    if(!R_GenerateTexCoords(s, t, point, scale, scale * 1.08f,
                            topLeft, bottomRight, surface.tangentMatrix()))
        return false;

    de::zap(projected);
    projected.texture     = GL_PrepareLSTexture(LST_DYNAMIC);
    projected.topLeft     = Vector2f(s[0], t[0]);
    projected.bottomRight = Vector2f(s[1], t[1]);
    projected.color       = Vector4f(black, shadowStrength);
    return true;
}

/**
 * @pre The coordinates of the given quad must be contained wholly within the subspoce
 * specified. This is due to an optimization within the lumobj management which separates
 * them by subspace.
 */
static void projectDynamics(Surface const &surface, dfloat glowStrength,
    Vector3d const &topLeft, Vector3d const &bottomRight,
    bool noLights, bool noShadows, bool sortLights,
    duint &lightListIdx, duint &shadowListIdx)
{
    DENG2_ASSERT(curSubspace);

    if(levelFullBright) return;
    if(glowStrength >= 1) return;

    // lights?
    if(!noLights)
    {
        dfloat const blendFactor = 1;

        if(::useDynLights)
        {
            // Project all lumobjs affecting the given quad (world space), calculate
            // coordinates (in texture space) then store into a new list of projections.
            R_ForAllSubspaceLumContacts(*curSubspace, [&topLeft, &bottomRight, &surface
                                                      , &blendFactor, &sortLights
                                                      , &lightListIdx] (Lumobj &lum)
            {
                ProjectedTextureData projected;
                if(projectDynlight(topLeft, bottomRight, lum, surface, blendFactor,
                                   projected))
                {
                    rendSys().findSurfaceProjectionList(&lightListIdx, sortLights)
                                << projected;  // a copy is made
                }
                return LoopContinue;
            });
        }

        if(::useGlowOnWalls && surface.parent().type() == DMU_SIDE && bottomRight.z < topLeft.z)
        {
            // Project all plane glows affecting the given quad (world space), calculate
            // coordinates (in texture space) then store into a new list of projections.
            SectorCluster const &cluster = curSubspace->cluster();
            for(dint i = 0; i < cluster.visPlaneCount(); ++i)
            {
                Plane const &plane = cluster.visPlane(i);
                Vector3d const pointOnPlane(cluster.center(), plane.heightSmoothed());

                ProjectedTextureData projected;
                if(projectPlaneGlow(topLeft, bottomRight, plane, pointOnPlane, blendFactor,
                                    projected))
                {
                    rendSys().findSurfaceProjectionList(&lightListIdx, sortLights)
                                << projected;  // a copy is made.
                }
            }
        }
    }

    // Shadows?
    if(!noShadows && ::useShadows)
    {
        // Glow inversely diminishes shadow strength.
        dfloat blendFactor = 1 - glowStrength;
        if(blendFactor >= SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        {
            blendFactor = de::clamp(0.f, blendFactor, 1.f);

            // Project all mobj shadows affecting the given quad (world space), calculate
            // coordinates (in texture space) then store into a new list of projections.
            R_ForAllSubspaceMobContacts(*curSubspace, [&topLeft, &bottomRight, &surface
                                                      , &blendFactor, &shadowListIdx] (mobj_t &mob)
            {
                ProjectedTextureData projected;
                if(projectShadow(topLeft, bottomRight, mob, surface, blendFactor,
                                 projected))
                {
                    rendSys().findSurfaceProjectionList(&shadowListIdx)
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
static bool lightWithWorldLight(Vector3d const & /*point*/, Vector3f const &ambientColor,
    bool starkLight, VectorLightData &vlight)
{
    static Vector3f const worldLight(-.400891f, -.200445f, .601336f);

    de::zap(vlight);
    vlight.direction         = worldLight;
    vlight.color             = ambientColor;
    vlight.affectedByAmbient = false;
    vlight.approxDist        = 0;
    if(starkLight)
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
    return true;
}

static bool lightWithLumobj(Vector3d const &point, Lumobj const &lum, VectorLightData &vlight)
{
    Vector3d const lumCenter(lum.x(), lum.y(), lum.z() + lum.zOffset());

    // Is this light close enough?
    ddouble const dist = M_ApproxDistance(M_ApproxDistance(lumCenter.x - point.x,
                                                           lumCenter.y - point.y),
                                          point.z - lumCenter.z);
    dfloat intensity = 0;
    if(dist < Lumobj::radiusMax())
    {
        intensity = de::clamp(0.f, dfloat(1 - dist / lum.radius()) * 2, 1.f);
    }
    if(intensity < .05f) return false;

    de::zap(vlight);
    vlight.direction         = (lumCenter - point) / dist;
    vlight.color             = lum.color() * intensity;
    vlight.affectedByAmbient = true;
    vlight.approxDist        = dist;
    vlight.lightSide         = 1;
    vlight.darkSide          = 0;
    vlight.offset            = 0;
    return true;
}

static bool lightWithPlaneGlow(Vector3d const &point, SectorCluster const &cluster,
    dint visPlaneIndex, VectorLightData &vlight)
{
    Plane const &plane     = cluster.visPlane(visPlaneIndex);
    Surface const &surface = plane.surface();

    // Glowing at this moment?
    Vector3f glowColor;
    dfloat intensity = surface.glow(glowColor);
    if(intensity < .05f) return false;

    coord_t const glowHeight = Rend_PlaneGlowHeight(intensity);
    if(glowHeight < 2) return false;  // Not too small!

    // In front of the plane?
    Vector3d const pointOnPlane(cluster.center(), plane.heightSmoothed());
    ddouble const dist = (point - pointOnPlane).dot(surface.normal());
    if(dist < 0) return false;

    intensity *= 1 - dist / glowHeight;
    if(intensity < .05f) return false;

    Vector3f const color = Rend_LuminousColor(glowColor, intensity);
    if(color == Vector3f()) return false;

    de::zap(vlight);
    vlight.direction         = Vector3f(surface.normal().x, surface.normal().y, -surface.normal().z);
    vlight.color             = color;
    vlight.affectedByAmbient = true;
    vlight.approxDist        = dist;
    vlight.lightSide         = 1;
    vlight.darkSide          = 0;
    vlight.offset            = 0.3f;
    return true;
}

duint Rend_CollectAffectingLights(Vector3d const &point, Vector3f const &ambientColor,
    ConvexSubspace *subspace, bool starkLight)
{
    duint lightListIdx = 0;

    // Always apply an ambient world light.
    {
        VectorLightData vlight;
        if(lightWithWorldLight(point, ambientColor, starkLight, vlight))
        {
            rendSys().findVectorLightList(&lightListIdx)
                    << vlight;  // a copy is made.
        }
    }

    // Add extra light by interpreting nearby sources.
    if(subspace)
    {
        // Interpret lighting from luminous-objects near the origin and which
        // are in contact the specified subspace and add them to the identified list.
        R_ForAllSubspaceLumContacts(*subspace, [&point, &lightListIdx] (Lumobj &lum)
        {
            VectorLightData vlight;
            if(lightWithLumobj(point, lum, vlight))
            {
                rendSys().findVectorLightList(&lightListIdx)
                        << vlight;  // a copy is made.
            }
            return LoopContinue;
        });

        // Interpret vlights from glowing planes at the origin in the specfified
        // subspace and add them to the identified list.
        SectorCluster const &cluster = subspace->cluster();
        for(dint i = 0; i < cluster.sector().planeCount(); ++i)
        {
            VectorLightData vlight;
            if(lightWithPlaneGlow(point, cluster, i, vlight))
            {
                rendSys().findVectorLightList(&lightListIdx)
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
static bool nearFadeOpacity(WallEdge const &leftEdge, WallEdge const &rightEdge,
                            dfloat &opacity)
{
    if(Rend_EyeOrigin().y < leftEdge.bottom().z() || Rend_EyeOrigin().y > rightEdge.top().z())
        return false;

    mobj_t const *mo         = viewPlayer->shared.mo;
    Line const &line         = leftEdge.mapLineSide().line();

    coord_t linePoint[2]     = { line.fromOrigin().x, line.fromOrigin().y };
    coord_t lineDirection[2] = {  line.direction().x,  line.direction().y };
    vec2d_t result;
    ddouble pos = V2d_ProjectOnLine(result, mo->origin, linePoint, lineDirection);

    if(!(pos > 0 && pos < 1))
        return false;

    coord_t const maxDistance = Mobj_Radius(*mo) * .8f;

    auto delta       = Vector2d(result) - Vector2d(mo->origin);
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
static dfloat calcLightLevelDelta(Vector3f const &normal)
{
    return (1.0f / 255) * (normal.x * 18) * ::rendLightWallAngle;
}

static void wallSectionLightLevelDeltas(WallEdge const &leftEdge, WallEdge const &rightEdge,
    dfloat surfaceLuminosityDeltas[2])
{
    dfloat &leftDelta  = surfaceLuminosityDeltas[0];
    dfloat &rightDelta = surfaceLuminosityDeltas[1];

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

        dfloat deltaDiff = rightDelta - leftDelta;
        rightDelta = leftDelta + ((sectionOffset + sectionWidth) / lineLength) * deltaDiff;
        leftDelta += (sectionOffset / lineLength) * deltaDiff;
    }
}

static void writeWallSection(HEdge &hedge, dint section,
    bool *retWroteOpaque = nullptr, coord_t *retBottomZ = nullptr, coord_t *retTopZ = nullptr)
{
    SectorCluster &cluster = curSubspace->cluster();

    auto &segment = hedge.mapElementAs<LineSideSegment>();
    DENG2_ASSERT(segment.isFrontFacing() && segment.lineSide().hasSections());

    if(retWroteOpaque) *retWroteOpaque = false;
    if(retBottomZ)     *retBottomZ     = 0;
    if(retTopZ)        *retTopZ        = 0;

    LineSide &side    = segment.lineSide();
    Surface &surface  = side.surface(section);

    // Skip nearly transparent surfaces.
    dfloat opacity = surface.opacity();
    if(opacity < .001f)
        return;

    // Determine which Material to use.
    Material *material = Rend_ChooseMapSurfaceMaterial(surface);

    // A drawable material is required.
    if(!material || !material->isDrawable())
        return;

    // Generate edge geometries.
    auto const wallSpec = WallSpec::fromMapSide(side, section);

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

    MaterialAnimator &matAnimator = material->getAnimator(Rend_MapSurfaceMaterialSpec());
    Vector2f const materialScale  = surface.materialScale();

    rendworldpoly_params_t parm; zap(parm);

    Vector3f materialOrigin   = leftEdge.materialOrigin();
    Vector3d topLeft          = leftEdge.top().origin();
    Vector3d bottomRight      = rightEdge.bottom().origin();

    parm.skyMasked            = skyMasked;
    parm.mapElement           = &segment;
    parm.geomGroup            = wallSpec.section;
    parm.topLeft              = &topLeft;
    parm.bottomRight          = &bottomRight;
    parm.forceOpaque          = wallSpec.flags.testFlag(WallSpec::ForceOpaque);
    parm.alpha                = parm.forceOpaque? 1 : opacity;
    parm.surfaceTangentMatrix = &surface.tangentMatrix();

    // Calculate the light level deltas for this wall section?
    if(!wallSpec.flags.testFlag(WallSpec::NoLightDeltas))
    {
        wallSectionLightLevelDeltas(leftEdge, rightEdge, parm.surfaceLuminosityDeltas);
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
                parm.glowing = matAnimator.glowStrength();
            }
            else
            {
                Material *actualMaterial =
                    surface.hasMaterial()? surface.materialPtr()
                                         : &resSys().material(de::Uri("System", Path("missing")));

                parm.glowing = actualMaterial->getAnimator(Rend_MapSurfaceMaterialSpec()).glowStrength();
            }

            parm.glowing *= ::glowFactor;
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
                parm.blendMode = BM_ZEROALPHA;  // "no translucency" mode
        }

        side.chooseSurfaceTintColors(wallSpec.section, &parm.surfaceColor, &parm.wall.surfaceColor2);
    }

    //
    // Geometry write/drawing begins.
    //

    if(twoSidedMiddle && side.sectorPtr() != &cluster.sector())
    {
        // Temporarily modify the draw state.
        curSectorLightColor = Rend_AmbientLightColor(side.sector());
        curSectorLightLevel = side.sector().lightLevel();
    }

    Vector3f posCoords[4];
    posCoords[0] =  leftEdge.bottom().origin();
    posCoords[1] =     leftEdge.top().origin();
    posCoords[2] = rightEdge.bottom().origin();
    posCoords[3] =    rightEdge.top().origin();

    // Draw this section.
    bool wroteOpaque = renderWorldPoly(posCoords, 4, parm, matAnimator);
    if(wroteOpaque)
    {
        // Render FakeRadio for this section?
        if(!wallSpec.flags.testFlag(WallSpec::NoFakeRadio) && !skyMasked &&
           !(parm.glowing > 0) && curSectorLightLevel > 0)
        {
            Rend_RadioUpdateForLineSide(side);

            // Determine the shadow properties.
            /// @todo Make cvars out of constants.
            dfloat shadowSize = 2 * (8 + 16 - curSectorLightLevel * 16);
            dfloat shadowDark = Rend_RadioCalcShadowDarkness(curSectorLightLevel);

            Rend_RadioWallSection(leftEdge, rightEdge, shadowDark, shadowSize);
        }
    }

    if(twoSidedMiddle && side.sectorPtr() != &cluster.sector())
    {
        // Undo temporary draw state changes.
        Vector4f const color = cluster.lightSourceColorfIntensity();
        curSectorLightColor = color.toVector3f();
        curSectorLightLevel = color.w;
    }

    if(retWroteOpaque) *retWroteOpaque = wroteOpaque && !didNearFade;
    if(retBottomZ)     *retBottomZ     = leftEdge.bottom().z();
    if(retTopZ)        *retTopZ        = rightEdge.top().z();
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
static duint buildSubspacePlaneGeometry(ClockDirection direction, coord_t height,
    Vector3f **verts)
{
    DENG2_ASSERT(verts);

    Face const &poly       = curSubspace->poly();
    HEdge *fanBase         = curSubspace->fanBase();
    duint const totalVerts = poly.hedgeCount() + (!fanBase? 2 : 0);

    *verts = R_AllocRendVertices(totalVerts);

    duint n = 0;
    if(!fanBase)
    {
        (*verts)[n] = Vector3f(poly.center(), height);
        n++;
    }

    // Add the vertices for each hedge.
    HEdge *baseNode = fanBase? fanBase : poly.hedge();
    HEdge *node = baseNode;
    do
    {
        (*verts)[n] = Vector3f(node->origin(), height);
        n++;
    } while((node = &node->neighbor(direction)) != baseNode);

    // The last vertex is always equal to the first.
    if(!fanBase)
    {
        (*verts)[n] = Vector3f(poly.hedge()->origin(), height);
    }

    return totalVerts;
}

static void writeSubspacePlane(Plane &plane)
{
    Face const &poly       = curSubspace->poly();
    Surface const &surface = plane.surface();

    // Skip nearly transparent surfaces.
    dfloat const opacity = surface.opacity();
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

    MaterialAnimator &matAnimator = material->getAnimator(Rend_MapSurfaceMaterialSpec());

    Vector2f materialOrigin = curSubspace->worldGridOffset() // Align to the worldwide grid.
                            + surface.materialOriginSmoothed();

    // Add the Y offset to orient the Y flipped material.
    /// @todo fixme: What is this meant to do? -ds
    if(plane.isSectorCeiling())
    {
        materialOrigin.y -= poly.aaBox().maxY - poly.aaBox().minY;
    }
    materialOrigin.y = -materialOrigin.y;

    Vector2f const materialScale = surface.materialScale();

    // Set the texture origin, Y is flipped for the ceiling.
    Vector3d topLeft(poly.aaBox().minX,
                     poly.aaBox().arvec2[plane.isSectorFloor()? 1 : 0][1],
                     plane.heightSmoothed());
    Vector3d bottomRight(poly.aaBox().maxX,
                         poly.aaBox().arvec2[plane.isSectorFloor()? 0 : 1][1],
                         plane.heightSmoothed());

    rendworldpoly_params_t parm; de::zap(parm);
    parm.mapElement           = curSubspace;
    parm.geomGroup            = plane.indexInSector();
    parm.topLeft              = &topLeft;
    parm.bottomRight          = &bottomRight;
    parm.materialOrigin       = &materialOrigin;
    parm.materialScale        = &materialScale;
    parm.surfaceColor         = &surface.tintColor();
    parm.surfaceTangentMatrix = &surface.tangentMatrix();

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
            parm.blendMode = BM_ZEROALPHA;  // "no translucency" mode
        }

        parm.alpha = surface.opacity();
    }

    if(!parm.skyMasked)
    {
        if(glowFactor > .0001f)
        {
            if(material == surface.materialPtr())
            {
                parm.glowing = matAnimator.glowStrength();
            }
            else
            {
                Material *actualMaterial =
                    surface.hasMaterial()? surface.materialPtr()
                                         : &resSys().material(de::Uri("System", Path("missing")));

                parm.glowing = actualMaterial->getAnimator(Rend_MapSurfaceMaterialSpec()).glowStrength();
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

    if(&plane.sector() != &curSubspace->sector())
    {
        // Temporarily modify the draw state.
        curSectorLightColor = Rend_AmbientLightColor(plane.sector());
        curSectorLightLevel = plane.sector().lightLevel();
    }

    // Allocate position coordinates.
    Vector3f *posCoords;
    duint vertCount = buildSubspacePlaneGeometry((plane.isSectorCeiling())? Anticlockwise : Clockwise,
                                                 plane.heightSmoothed(), &posCoords);

    // Draw this section.
    renderWorldPoly(posCoords, vertCount, parm, matAnimator);

    if(&plane.sector() != &curSubspace->sector())
    {
        // Undo temporary draw state changes.
        Vector4f const color = curSubspace->cluster().lightSourceColorfIntensity();
        curSectorLightColor = color.toVector3f();
        curSectorLightLevel = color.w;
    }

    R_FreeRendVertices(posCoords);
}

static void writeSkyMaskStrip(dint vertCount, Vector3f const *posCoords,
    Vector2f const *texCoords, Material *material)
{
    DENG2_ASSERT(posCoords);

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
        DENG2_ASSERT(texCoords);

        DrawListSpec listSpec;
        listSpec.group = UnlitGeom;
        if(renderTextures != 2)
        {
            DENG2_ASSERT(material);
            MaterialAnimator &matAnimator = material->getAnimator(Rend_MapSurfaceMaterialSpec());

            // Ensure we've up to date info about the material.
            matAnimator.prepare();

            // Map RTU configuration from the sky surface material.
            listSpec.texunits[TU_PRIMARY]        = matAnimator.texUnit(MaterialAnimator::TU_LAYER0);
            listSpec.texunits[TU_PRIMARY_DETAIL] = matAnimator.texUnit(MaterialAnimator::TU_DETAIL);
            listSpec.texunits[TU_INTER]          = matAnimator.texUnit(MaterialAnimator::TU_LAYER0_INTER);
            listSpec.texunits[TU_INTER_DETAIL]   = matAnimator.texUnit(MaterialAnimator::TU_DETAIL_INTER);
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

static void writeSubspaceSkyMaskStrips(SkyFixEdge::FixType fixType)
{
    // Determine strip generation behavior.
    ClockDirection const direction   = Clockwise;
    bool const buildTexCoords        = CPP_BOOL(devRendSkyMode);
    bool const splitOnMaterialChange = (devRendSkyMode && renderTextures != 2);

    // Configure the strip builder wrt vertex attributes.
    TriangleStripBuilder stripBuilder(buildTexCoords);

    // Configure the strip build state (we'll most likely need to break
    // edge loop into multiple strips).
    HEdge *startNode           = nullptr;
    coord_t startZBottom       = 0;
    coord_t startZTop          = 0;
    Material *startMaterial    = nullptr;
    dfloat startMaterialOffset = 0;

    // Determine the relative sky plane (for monitoring material changes).
    dint relPlane = fixType == SkyFixEdge::Upper? Sector::Ceiling : Sector::Floor;

    // Begin generating geometry.
    HEdge *base  = curSubspace->poly().hedge();
    HEdge *hedge = base;
    forever
    {
        // Are we monitoring material changes?
        Material *skyMaterial = nullptr;
        if(splitOnMaterialChange)
        {
            skyMaterial = hedge->face().mapElementAs<ConvexSubspace>()
                              .cluster().visPlane(relPlane).surface().materialPtr();
        }

        // Add a first (left) edge to the current strip?
        if(!startNode && hedge->hasMapElement())
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
        if(startNode)
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
                startNode = nullptr;

                // Take ownership of the built geometry.
                PositionBuffer *positions = nullptr;
                TexCoordBuffer *texcoords = nullptr;
                dint numVerts = stripBuilder.take(&positions, &texcoords);

                // Write the strip geometry to the render lists.
                writeSkyMaskStrip(numVerts, positions->constData(),
                                  texcoords? texcoords->constData() : nullptr,
                                  startMaterial);

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
#define SKYCAP_LOWER        0x1
#define SKYCAP_UPPER        0x2
///@}

static coord_t skyPlaneZ(dint skyCap)
{
    SectorCluster &cluster = curSubspace->cluster();

    dint const relPlane = (skyCap & SKYCAP_UPPER)? Sector::Ceiling : Sector::Floor;
    if(!P_IsInVoid(viewPlayer))
    {
        return cluster.sector().map().skyFix(relPlane == Sector::Ceiling);
    }

    return cluster.visPlane(relPlane).heightSmoothed();
}

/// @param skyCap  @ref skyCapFlags.
static void writeSubspaceSkyMaskCap(dint skyCap)
{
    // Caps are unnecessary in sky debug mode (will be drawn as regular planes).
    if(devRendSkyMode) return;
    if(!skyCap) return;

    Vector3f *posCoords;
    duint vertCount = buildSubspacePlaneGeometry((skyCap & SKYCAP_UPPER)? Anticlockwise : Clockwise,
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
static void writeSubspaceSkyMask(dint skyCap = SKYCAP_LOWER | SKYCAP_UPPER)
{
    SectorCluster &cluster = curSubspace->cluster();

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
        writeSubspaceSkyMaskStrips(SkyFixEdge::Lower);
        writeSubspaceSkyMaskCap(SKYCAP_LOWER);
    }

    // Upper?
    if(skyCap & SKYCAP_UPPER)
    {
        writeSubspaceSkyMaskStrips(SkyFixEdge::Upper);
        writeSubspaceSkyMaskCap(SKYCAP_UPPER);
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

    SectorCluster const &cluster     = hedge.face().mapElementAs<ConvexSubspace>().cluster();
    SectorCluster const &backCluster = hedge.twin().face().mapElementAs<ConvexSubspace>().cluster();

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
        rendSys().angleClipper().addRangeFromViewRelPoints(hedge->origin(), hedge->twin().origin());
    }
}

static void writeSubspaceWallSections()
{
    HEdge *base  = curSubspace->poly().hedge();
    HEdge *hedge = base;
    do
    {
        writeAllWallSections(hedge);
    } while((hedge = &hedge->next()) != base);

    curSubspace->forAllExtraMeshes([] (Mesh &mesh)
    {
        for(HEdge *hedge : mesh.hedges())
        {
            writeAllWallSections(hedge);
        }
        return LoopContinue;
    });

    curSubspace->forAllPolyobjs([] (Polyobj &pob)
    {
        for(HEdge *hedge : pob.mesh().hedges())
        {
            writeAllWallSections(hedge);
        }
        return LoopContinue;
    });
}

static void writeSubspacePlanes()
{
    SectorCluster &cluster = curSubspace->cluster();

    for(dint i = 0; i < cluster.visPlaneCount(); ++i)
    {
        Plane &plane = cluster.visPlane(i);

        // Skip planes facing away from the viewer.
        Vector3d const pointOnPlane(cluster.center(), plane.heightSmoothed());
        if((eyeOrigin - pointOnPlane).dot(plane.surface().normal()) < 0)
            continue;

        writeSubspacePlane(plane);
    }
}

static void markFrontFacingWalls(HEdge *hedge)
{
    if(!hedge || !hedge->hasMapElement()) return;
    auto &seg = hedge->mapElementAs<LineSideSegment>();
    // Which way is the line segment facing?
    seg.setFrontFacing(viewFacingDot(hedge->origin(), hedge->twin().origin()) >= 0);
}

static void markSubspaceFrontFacingWalls()
{
    HEdge *base = curSubspace->poly().hedge();
    HEdge *hedge = base;
    do
    {
        markFrontFacingWalls(hedge);
    } while((hedge = &hedge->next()) != base);

    curSubspace->forAllExtraMeshes([] (Mesh &mesh)
    {
        for(HEdge *hedge : mesh.hedges())
        {
            markFrontFacingWalls(hedge);
        }
        return LoopContinue;
    });

    curSubspace->forAllPolyobjs([] (Polyobj &pob)
    {
        for(HEdge *hedge : pob.mesh().hedges())
        {
            markFrontFacingWalls(hedge);
        }
        return LoopContinue;
    });
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
static void occludeSubspace(bool frontFacing)
{
    if(devNoCulling) return;
    if(P_IsInVoid(viewPlayer)) return;

    AngleClipper &clipper  = rendSys().angleClipper();

    SectorCluster &cluster = curSubspace->cluster();

    HEdge *base  = curSubspace->poly().hedge();
    HEdge *hedge = base;
    do
    {
        // Edges without a line segment can never occlude.
        if(!hedge->hasMapElement())
            continue;

        auto &seg = hedge->mapElementAs<LineSideSegment>();

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
        if(((openBottom > cluster.visFloor().heightSmoothed() && Rend_EyeOrigin().y <= openBottom)
            || (openBottom >  backCluster.visFloor().heightSmoothed() && Rend_EyeOrigin().y >= openBottom))
           && canOccludeEdgeBetweenPlanes(cluster.visFloor(), backCluster.visFloor()))
        {
            clipper.addViewRelOcclusion(from.origin(), to.origin(), openBottom, false);
        }

        // Does the ceiling create an occlusion?
        if(((openTop < cluster.visCeiling().heightSmoothed() && Rend_EyeOrigin().y >= openTop)
            || (openTop <  backCluster.visCeiling().heightSmoothed() && Rend_EyeOrigin().y <= openTop))
           && canOccludeEdgeBetweenPlanes(cluster.visCeiling(), backCluster.visCeiling()))
        {
            clipper.addViewRelOcclusion(from.origin(), to.origin(), openTop, true);
        }
    } while((hedge = &hedge->next()) != base);
}

static void clipSubspaceLumobjs()
{
    DENG2_ASSERT(curSubspace);
    curSubspace->forAllLumobjs([] (Lumobj &lob)
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
    // Any work to do?
    DENG2_ASSERT(curSubspace);
    if(!curSubspace->polyobjCount()) return;

    curSubspace->forAllLumobjs([] (Lumobj &lob)
    {
        R_ViewerClipLumobjBySight(&lob, curSubspace);
        return LoopContinue;
    });
}

/// If not front facing this is no-op.
static void clipFrontFacingWalls(HEdge *hedge)
{
    if(!hedge || !hedge->hasMapElement())
        return;

    LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
    if(seg.isFrontFacing())
    {
        if(!rendSys().angleClipper().checkRangeFromViewRelPoints(hedge->origin(), hedge->twin().origin()))
        {
            seg.setFrontFacing(false);
        }
    }
}

static void clipSubspaceFrontFacingWalls()
{
    HEdge *base = curSubspace->poly().hedge();
    HEdge *hedge = base;
    do
    {
        clipFrontFacingWalls(hedge);
    } while((hedge = &hedge->next()) != base);

    curSubspace->forAllExtraMeshes([] (Mesh &mesh)
    {
        for(HEdge *hedge : mesh.hedges())
        {
            clipFrontFacingWalls(hedge);
        }
        return LoopContinue;
    });

    curSubspace->forAllPolyobjs([] (Polyobj &pob)
    {
        for(HEdge *hedge : pob.mesh().hedges())
        {
            clipFrontFacingWalls(hedge);
        }
        return LoopContinue;
    });
}

static void projectSubspaceSprites()
{
    // Do not use validCount because other parts of the renderer may change it.
    if(curSubspace->lastSpriteProjectFrame() == R_FrameCount())
        return;  // Already added.

    R_ForAllSubspaceMobContacts(*curSubspace, [] (mobj_t &mob)
    {
        SectorCluster &cluster = curSubspace->cluster();
        if(mob.addFrameCount != R_FrameCount())
        {
            mob.addFrameCount = R_FrameCount();

            R_ProjectSprite(mob);

            // Hack: Sprites have a tendency to extend into the ceiling in
            // sky sectors. Here we will raise the skyfix dynamically, to make sure
            // that no sprites get clipped by the sky.

            if(cluster.visCeiling().surface().hasSkyMaskedMaterial())
            {
                if(Sprite *sprite = Mobj_Sprite(mob))
                {
                    if(sprite->hasViewAngle(0))
                    {
                        Material *material = sprite->viewAngle(0).material;
                        if(!(mob.dPlayer && (mob.dPlayer->flags & DDPF_CAMERA))
                           && mob.origin[2] <= cluster.visCeiling().heightSmoothed()
                           && mob.origin[2] >= cluster.visFloor().heightSmoothed())
                        {
                            coord_t visibleTop = mob.origin[2] + material->height();
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
        return LoopContinue;
    });

    curSubspace->setLastSpriteProjectFrame(R_FrameCount());
}

/**
 * @pre Assumes the subspace is at least partially visible.
 */
static void drawCurrentSubspace()
{
    Sector &sector = curSubspace->sector();

    // Mark the leaf as visible for this frame.
    R_ViewerSubspaceMarkVisible(*curSubspace);

    markSubspaceFrontFacingWalls();

    // Perform contact spreading for this map region.
    sector.map().spreadAllContacts(curSubspace->poly().aaBox());

    Rend_RadioSubspaceEdges(*curSubspace);

    // Before clip testing lumobjs (for halos), range-occlude the back facing edges.
    // After testing, range-occlude the front facing edges. Done before drawing wall
    // sections so that opening occlusions cut out unnecessary oranges.

    occludeSubspace(false /* back facing */);
    clipSubspaceLumobjs();
    occludeSubspace(true /* front facing */);

    clipSubspaceFrontFacingWalls();
    clipSubspaceLumobjsBySight();

    // Mark generators in the sector visible.
    if(useParticles)
    {
        sector.map().forAllGeneratorsInSector(sector, [] (Generator &gen)
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
    writeSubspaceWallSections();
    writeSubspacePlanes();
}

/**
 * Change the current subspace (updating any relevant draw state properties
 * accordingly).
 *
 * @param subspace  The new subspace to make current.
 */
static void makeCurrent(ConvexSubspace &subspace)
{
    bool clusterChanged = (!curSubspace || curSubspace->clusterPtr() != subspace.clusterPtr());

    curSubspace = &subspace;

    // Update draw state.
    if(clusterChanged)
    {
        Vector4f const color = subspace.cluster().lightSourceColorfIntensity();
        curSectorLightColor = color.toVector3f();
        curSectorLightLevel = color.w;
    }
}

static void traverseBspTreeAndDrawSubspaces(Map::BspTree const *bspTree)
{
    DENG2_ASSERT(bspTree);
    AngleClipper const &clipper = rendSys().angleClipper();

    while(!bspTree->isLeaf())
    {
        // Descend deeper into the nodes.
        BspNode &bspNode = bspTree->userData()->as<BspNode>();

        // Decide which side the view point is on.
        dint eyeSide = bspNode.partition().pointOnSide(eyeOrigin) < 0;

        // Recursively divide front space.
        traverseBspTreeAndDrawSubspaces(bspTree->childPtr(Map::BspTree::ChildId(eyeSide)));

        // If the clipper is full we're pretty much done. This means no geometry
        // will be visible in the distance because every direction has already
        // been fully covered by geometry.
        if(!firstSubspace && clipper.isFull())
            return;

        // ...and back space.
        bspTree = bspTree->childPtr(Map::BspTree::ChildId(eyeSide ^ 1));
    }
    // We've arrived at a leaf.

    // Only leafs with a convex subspace geometry contain surfaces to draw.
    if(ConvexSubspace *subspace = bspTree->userData()->as<BspLeaf>().subspacePtr())
    {
        DENG2_ASSERT(subspace->hasCluster());

        // Skip zero-volume subspaces.
        // (Neighbors handle the angle clipper ranges.)
        if(!subspace->cluster().hasWorldVolume())
            return;

        // Is this subspace visible?
        if(!firstSubspace && !clipper.isPolyVisible(subspace->poly()))
            return;

        // This is now the current subspace.
        makeCurrent(*subspace);

        drawCurrentSubspace();

        // This is no longer the first subspace.
        firstSubspace = false;
    }
}

/**
 * Project all the non-clipped decorations. They become regular vissprites.
 */
static void generateDecorationFlares(Map &map)
{
    Vector3d const viewPos = Rend_EyeOrigin().xzy();
    map.forAllLumobjs([&viewPos] (Lumobj &lob)
    {
        lob.generateFlare(viewPos, R_ViewerLumobjDistance(lob.indexInMap()));

        /// @todo mark these light sources visible for LensFx
        return LoopContinue;
    });
}

/**
 * Setup GL state for an entire rendering pass (compassing multiple lists).
 */
static void pushGLStateForPass(DrawMode mode, TexUnitMap &texUnitMap)
{
    static dfloat const black[] = { 0, 0, 0, 0 };

    de::zap(texUnitMap);

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
            GL_ModulateTexture(4);  // Light * texture.
        }
        else
        {
            texUnitMap[0] = Store::TCA_MAIN + 1;
            texUnitMap[1] = Store::TCA_LIGHT + 1;
            GL_ModulateTexture(5);  // Texture + light.
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
        GL_ModulateTexture(7);  // Add light, no color.
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
            dfloat const midGray[] = { .5f, .5f, .5f, fogColor[3] };  // The alpha is probably meaningless?
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
            dfloat const midGray[] = { .5f, .5f, .5f, fogColor[3] };  // The alpha is probably meaningless?
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
        GL_ModulateTexture(1);  // 8 for multitexture
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
        GL_BlendMode(BM_ADD);  // Purely additive.
        break;

    case DM_MASKED_SHINY:
        GL_SelectTexUnits(2);
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_BLEND + 1;  // the mask
        GL_ModulateTexture(8);  // same as with details
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
        GL_BlendMode(BM_ADD);  // Purely additive.
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
    for(dint i = 0; i < lists.count(); ++i)
    {
        lists.at(i)->draw(mode, texUnitMap);
    }

    popGLStateForPass(mode);
}

static void drawSky()
{
    DrawLists::FoundLists lists;
    rendSys().drawLists().findAll(SkyMaskGeom, lists);
    if(!devRendSkyAlways && lists.isEmpty())
    {
        return;
    }

    // We do not want to update color and/or depth.
    GLState::push()
            .setDepthTest(false)
            .setDepthWrite(false)
            .setColorMask(gl::WriteNone)
            .apply();

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
    glDisable(GL_STENCIL_TEST);

    // Now, only render where the stencil is set to 1.
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 1, 0xffffffff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    rendSys().sky().draw(&worldSys().skyAnimator());

    if(!devRendSkyAlways)
    {
        glClearStencil(0);
    }

    // Return GL state to normal.
    glDisable(GL_STENCIL_TEST);
}

static bool generateHaloForVisSprite(vissprite_t const *spr, bool primary = false)
{
    dfloat occlusionFactor;

    if(primary && (spr->data.flare.flags & RFF_NO_PRIMARY))
    {
        return false;
    }

    if(spr->data.flare.isDecoration)
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

    return H_RenderHalo(spr->pose.origin,
                        spr->data.flare.size,
                        spr->data.flare.tex,
                        spr->data.flare.color,
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
    if(devNoSprites) return;

    R_SortVisSprites();

    if(visSpriteP && visSpriteP > visSprites)
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
                Rend_DrawSprite(*spr);
                break;

            case VSPR_MODEL:
                Rend_DrawModel(*spr);
                break;

            case VSPR_MODEL_GL2:
                Rend_DrawModel2(*spr);
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

/**
 * We have several different paths to accommodate both multitextured details and
 * dynamic lights. Details take precedence (they always cover entire primitives
 * and usually *all* of the surfaces in a scene).
 */
static void drawAllLists(Map &map)
{
    DENG2_ASSERT(!Sys_GLCheckError());
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    drawSky();

    // Render the real surfaces of the visible world.

    //
    // Pass: Unlit geometries (all normal lists).
    //
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

    //
    // Pass: Lit geometries.
    //
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

    //
    // Pass: All light geometries (always additive).
    //
    if(dynlightBlend != 2)
    {
        ClientApp::renderSystem().drawLists().findAll(LightGeom, lists);
        drawLists(lists, DM_LIGHTS);
    }

    //
    // Pass: Geometries with texture modulation.
    //
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

    //
    // Pass: Geometries with details & modulation.
    //
    // If multitexturing is not available for details, we need to apply them as
    // an extra pass over all the detailed surfaces.
    //
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

    //
    // Pass: Shiny geometries.
    //
    // If we have two texture units, the shiny masks will be enabled. Otherwise
    // the masks are ignored. The shine is basically specular environmental
    // additive light, multiplied by the mask so that black texels from the mask
    // produce areas without shine.
    //

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

    //
    // Pass: Shadow geometries (objects and Fake Radio).
    //
    dint const oldRenderTextures = renderTextures;

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
        // Prepare for rendering.
        rendSys().beginFrame();

        // Make vissprites of all the visible decorations.
        generateDecorationFlares(map);

        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
        eyeOrigin = viewData->current.origin;

        // Add the backside clipping range (if vpitch allows).
        if(vpitch <= 90 - yfov / 2 && vpitch >= -90 + yfov / 2)
        {
            AngleClipper &clipper = rendSys().angleClipper();

            dfloat a = de::abs(vpitch) / (90 - yfov / 2);
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

static void drawStar(Vector3d const &origin, dfloat size, Vector4f const &color)
{
    static dfloat const black[] = { 0, 0, 0, 0 };

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

static void drawLabel(Vector3d const &origin, String const &label, dfloat scale, dfloat alpha)
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
    ddouble distToEye = (Rend_EyeOrigin().xzy() - origin).length();
    drawLabel(origin, label, distToEye / (DENG_GAMEVIEW_WIDTH / 2), 1 - distToEye / 2000);
}

/*
 * Visuals for Shadow Bias editing:
 */

static String labelForSource(BiasSource *s)
{
    if(!s || !editShowIndices) return String();
    /// @todo Don't assume the current map.
    return String::number(App_WorldSystem().map().indexOf(*s));
}

static void drawSource(BiasSource *s)
{
    if(!s) return;

    ddouble distToEye = (s->origin() - eyeOrigin).length();

    drawStar(s->origin(), 25 + s->evaluateIntensity() / 20,
             Vector4f(s->color(), 1.0f / de::max(float((distToEye - 100) / 1000), 1.f)));
    drawLabel(s->origin(), labelForSource(s));
}

static void drawLock(Vector3d const &origin, ddouble unit, ddouble t)
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

    ddouble const t = Timer_RealMilliseconds() / 100.0f;

    if(HueCircle *hueCircle = SBE_HueCircle())
    {
        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(Rend_EyeOrigin().x, Rend_EyeOrigin().y, Rend_EyeOrigin().z);
        glScalef(1, 1.0f/1.2f, 1);
        glTranslatef(-Rend_EyeOrigin().x, -Rend_EyeOrigin().y, -Rend_EyeOrigin().z);

        HueCircleVisual::draw(*hueCircle, Rend_EyeOrigin(), viewData->frontVec);

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
    DENG2_ASSERT(nearSource);

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

    for(Grabbable *grabbable : hand.grabbed())
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
        map.forAllBiasSources([&nearSource] (BiasSource &source)
        {
            if(&source != nearSource && !source.isGrabbed())
            {
                drawSource(&source);
            }
            return LoopContinue;
        });
    }

    glEnable(GL_DEPTH_TEST);
}

void Rend_UpdateLightModMatrix()
{
    if(novideo) return;

    de::zap(lightModRange);

    if(!App_WorldSystem().hasMap())
    {
        rAmbient = 0;
        return;
    }

    dint mapAmbient = App_WorldSystem().map().ambientLightLevel();
    if(mapAmbient > ambientLight)
    {
        rAmbient = mapAmbient;
    }
    else
    {
        rAmbient = ambientLight;
    }

    for(dint i = 0; i < 255; ++i)
    {
        // Adjust the white point/dark point?
        dfloat lightlevel = 0;
        if(lightRangeCompression != 0)
        {
            if(lightRangeCompression >= 0)
            {
                // Brighten dark areas.
                lightlevel = dfloat(255 - i) * lightRangeCompression;
            }
            else
            {
                // Darken bright areas.
                lightlevel = dfloat(-i) * -lightRangeCompression;
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

dfloat Rend_LightAdaptationDelta(dfloat val)
{
    dint clampedVal = de::clamp(0, de::roundi(255.0f * val), 254);
    return lightModRange[clampedVal];
}

void Rend_ApplyLightAdaptation(dfloat &val)
{
    val += Rend_LightAdaptationDelta(val);
}

void Rend_DrawLightModMatrix()
{
#define BLOCK_WIDTH             ( 1.0f )
#define BLOCK_HEIGHT            ( BLOCK_WIDTH * 255.0f )
#define BORDER                  ( 20 )

    // Disabled?
    if(!devLightModRange) return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);

    glTranslatef(BORDER, BORDER, 0);

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
    dfloat c = 0;
    for(dint i = 0; i < 255; ++i, c += (1.0f/255.0f))
    {
        // Get the result of the source light level + offset.
        dfloat off = lightModRange[i];

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

static DGLuint constructBBox(DGLuint name, dfloat br)
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
void Rend_DrawBBox(Vector3d const &pos, coord_t w, coord_t l, coord_t h,
    dfloat a, dfloat const color[3], dfloat alpha, dfloat br, bool alignToBase)
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
    glColor4f(color[0], color[1], color[2], alpha);

    GL_CallList(dlBBox);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
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
void Rend_DrawArrow(Vector3d const &pos, dfloat a, dfloat s, dfloat const color[3],
    dfloat alpha)
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

static void drawMobjBBox(mobj_t &mob)
{
    static dfloat const red   [] = { 1,    0.2f, 0.2f };  // non-solid objects
    static dfloat const green [] = { 0.2f, 1,    0.2f };  // solid objects
    static dfloat const yellow[] = { 0.7f, 0.7f, 0.2f };  // missiles

    // We don't want the console player.
    if(&mob == ddPlayers[consolePlayer].shared.mo)
        return;

    // Is it vissible?
    if(!Mobj_IsLinked(mob)) return;

    BspLeaf const &bspLeaf = Mobj_BspLeafAtOrigin(mob);
    if(!bspLeaf.hasSubspace() || !R_ViewerSubspaceIsVisible(bspLeaf.subspace()))
        return;

    ddouble const distToEye = (eyeOrigin - Mobj_Origin(mob)).length();
    dfloat alpha = 1 - ((distToEye / (DENG_GAMEVIEW_WIDTH/2)) / 4);
    if(alpha < .25f)
        alpha = .25f; // Don't make them totally invisible.

    // Draw a bounding box in an appropriate color.
    coord_t size = Mobj_Radius(mob);
    Rend_DrawBBox(mob.origin, size, size, mob.height/2, 0,
                  (mob.ddFlags & DDMF_MISSILE)? yellow :
                  (mob.ddFlags & DDMF_SOLID)? green : red,
                  alpha, .08f);

    Rend_DrawArrow(mob.origin, ((mob.angle + ANG45 + ANG90) / (dfloat) ANGLE_MAX *-360), size*1.25,
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
    //static dfloat const red   [] = { 1,    0.2f, 0.2f };  // non-solid objects
    static dfloat const green [] = { 0.2f, 1,    0.2f };  // solid objects
    static dfloat const yellow[] = { 0.7f, 0.7f, 0.2f };  // missiles

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

    MaterialAnimator &matAnimator = resSys().material(de::Uri("System", Path("bbox")))
                                                .getAnimator(Rend_SpriteMaterialSpec());

    // Ensure we've up to date info about the material.
    matAnimator.prepare();

    GL_BindTexture(matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture);
    GL_BlendMode(BM_ADD);

    if(devMobjBBox)
    {
        map.thinkers().forAll(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1, [] (thinker_t *th)
        {
            drawMobjBBox(*reinterpret_cast<mobj_t *>(th));
            return LoopContinue;
        });
    }

    if(devPolyobjBBox)
    {
        map.forAllPolyobjs([] (Polyobj &pob)
        {
            Sector const &sec = pob.sector();

            coord_t width  = (pob.aaBox.maxX - pob.aaBox.minX)/2;
            coord_t length = (pob.aaBox.maxY - pob.aaBox.minY)/2;
            coord_t height = (sec.ceiling().height() - sec.floor().height())/2;

            Vector3d pos(pob.aaBox.minX + width,
                         pob.aaBox.minY + length,
                         sec.floor().height());

            ddouble const distToEye = (eyeOrigin - pos).length();
            dfloat alpha = 1 - ((distToEye / (DENG_GAMEVIEW_WIDTH/2)) / 4);
            if(alpha < .25f)
                alpha = .25f; // Don't make them totally invisible.

            Rend_DrawBBox(pos, width, length, height, 0, yellow, alpha, .08f);

            for(Line *line : pob.lines())
            {
                Vector3d pos(line->center(), sec.floor().height());

                Rend_DrawBBox(pos, 0, line->length() / 2, height,
                              BANG2DEG(BANG_90 - line->angle()),
                              green, alpha, 0);
            }

            return LoopContinue;
        });
    }

    GL_BlendMode(BM_NORMAL);

    glEnable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}

static void drawVector(Vector3f const &vector, dfloat scalar, dfloat const color[3])
{
    static dfloat const black[] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(scalar * vector.x, scalar * vector.z, scalar * vector.y);
        glColor3fv(color);
        glVertex3f(0, 0, 0);
    glEnd();
}

static void drawTangentVectorsForSurface(Surface const &suf, Vector3d const &origin)
{
    static dint const VISUAL_LENGTH = 20;
    static dfloat const red  [] = { 1, 0, 0 };
    static dfloat const green[] = { 0, 1, 0 };
    static dfloat const blue [] = { 0, 0, 1 };

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

/**
 * @todo Use drawTangentVectorsForWallSections() for polyobjs too.
 */
static void drawSurfaceTangentVectors(SectorCluster &cluster)
{
    for(ConvexSubspace *subspace : cluster.subspaces())
    {
        HEdge const *base  = subspace->poly().hedge();
        HEdge const *hedge = base;
        do
        {
            drawTangentVectorsForWallSections(hedge);
        } while((hedge = &hedge->next()) != base);

        subspace->forAllExtraMeshes([] (Mesh &mesh)
        {
            for(HEdge *hedge : mesh.hedges())
            {
                drawTangentVectorsForWallSections(hedge);
            }
            return LoopContinue;
        });

        subspace->forAllPolyobjs([] (Polyobj &pob)
        {
            for(HEdge *hedge : pob.mesh().hedges())
            {
                drawTangentVectorsForWallSections(hedge);
            }
            return LoopContinue;
        });
    }

    dint const planeCount = cluster.sector().planeCount();
    for(dint i = 0; i < planeCount; ++i)
    {
        Plane const &plane = cluster.visPlane(i);
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

        drawTangentVectorsForSurface(plane.surface(), Vector3d(cluster.center(), height));
    }
}

/**
 * Draw the surface tangent space vectors, primarily for debug.
 */
static void drawAllSurfaceTangentVectors(Map &map)
{
    if(!devSurfaceVectors) return;

    glDisable(GL_CULL_FACE);

    map.forAllClusters([] (SectorCluster &cluster)
    {
        drawSurfaceTangentVectors(cluster);
        return LoopContinue;
    });

    glEnable(GL_CULL_FACE);
}

static void drawLumobjs(Map &map)
{
    static dfloat const black[] = { 0, 0, 0, 0 };

    if(!devDrawLums) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    map.forAllLumobjs([] (Lumobj &lob)
    {
        if(rendMaxLumobjs > 0 && R_ViewerLumobjIsHidden(lob.indexInMap()))
            return LoopContinue;

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslated(lob.origin().x, lob.origin().z + lob.zOffset(), lob.origin().y);

        glBegin(GL_LINES);
        {
            glColor4fv(black);
            glVertex3f(-lob.radius(), 0, 0);
            glColor4f(lob.color().x, lob.color().y, lob.color().z, 1);
            glVertex3f(0, 0, 0);
            glVertex3f(0, 0, 0);
            glColor4fv(black);
            glVertex3f(lob.radius(), 0, 0);

            glVertex3f(0, -lob.radius(), 0);
            glColor4f(lob.color().x, lob.color().y, lob.color().z, 1);
            glVertex3f(0, 0, 0);
            glVertex3f(0, 0, 0);
            glColor4fv(black);
            glVertex3f(0, lob.radius(), 0);

            glVertex3f(0, 0, -lob.radius());
            glColor4f(lob.color().x, lob.color().y, lob.color().z, 1);
            glVertex3f(0, 0, 0);
            glVertex3f(0, 0, 0);
            glColor4fv(black);
            glVertex3f(0, 0, lob.radius());
        }
        glEnd();

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        return LoopContinue;
    });

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

static void drawSoundEmitter(SoundEmitter &emitter, String const &label)
{
#define MAX_SOUNDORIGIN_DIST  384

    Vector3d const &origin(emitter.origin);
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
        map.forAllLines([] (Line &line)
        {
            for(dint i = 0; i < 2; ++i)
            {
                LineSide &side = line.side(i);
                if(!side.hasSections()) continue;

                drawSoundEmitter(side.middleSoundEmitter(),
                                 String("Line #%1 (%2, middle)")
                                     .arg(line.indexInMap())
                                     .arg(i? "back" : "front"));

                drawSoundEmitter(side.bottomSoundEmitter(),
                                 String("Line #%1 (%2, bottom)")
                                     .arg(line.indexInMap())
                                     .arg(i? "back" : "front"));

                drawSoundEmitter(side.topSoundEmitter(),
                                 String("Line #%1 (%2, top)")
                                     .arg(line.indexInMap())
                                     .arg(i? "back" : "front"));
            }
            return LoopContinue;
        });
    }

    if(devSoundEmitters & (SOF_SECTOR | SOF_PLANE))
    {
        map.forAllSectors([] (Sector &sector)
        {
            if(devSoundEmitters & SOF_PLANE)
            {
                sector.forAllPlanes([] (Plane &plane)
                {
                    drawSoundEmitter(plane.soundEmitter(),
                                     String("Sector #%1 (pln:%2)")
                                         .arg(plane.sector().indexInMap())
                                         .arg(plane.indexInSector()));
                    return LoopContinue;
                });
            }

            if(devSoundEmitters & SOF_SECTOR)
            {
                drawSoundEmitter(sector.soundEmitter(),
                                 String("Sector #%1").arg(sector.indexInMap()));
            }
            return LoopContinue;
        });
    }
}

void Rend_DrawVectorLight(VectorLightData const &vlight, dfloat alpha)
{
    if(alpha < .0001f) return;

    dfloat const unitLength = 100;
    glBegin(GL_LINES);
        glColor4f(vlight.color.x, vlight.color.y, vlight.color.z, alpha);
        glVertex3f(unitLength * vlight.direction.x, unitLength * vlight.direction.z, unitLength * vlight.direction.y);
        glColor4f(vlight.color.x, vlight.color.y, vlight.color.z, 0);
        glVertex3f(0, 0, 0);
    glEnd();
}

static String labelForGenerator(Generator const &gen)
{
    return String("%1").arg(gen.id());
}

static void drawGenerator(Generator const &gen)
{
    static dint const MAX_GENERATOR_DIST = 2048;

    if(gen.source || gen.isUntriggered())
    {
        Vector3d const origin   = gen.origin();
        ddouble const distToEye = (eyeOrigin - origin).length();
        if(distToEye < MAX_GENERATOR_DIST)
        {
            drawLabel(origin, labelForGenerator(gen),
                      distToEye / (DENG_GAMEVIEW_WIDTH / 2),
                      1 - distToEye / MAX_GENERATOR_DIST);
        }
    }
}

/**
 * Debugging aid; Draw all active generators.
 */
static void drawGenerators(Map &map)
{
    if(!devDrawGenerators) return;

    map.forAllGenerators([] (Generator &gen)
    {
        drawGenerator(gen);
        return LoopContinue;
    });
}

static void drawPoint(Vector3d const &origin, dfloat opacity)
{
    glBegin(GL_POINTS);
        glColor4f(.7f, .7f, .2f, opacity * 2);
        glVertex3f(origin.x, origin.z, origin.y);
    glEnd();
}

static void drawBar(Vector3d const &origin, coord_t height, dfloat opacity)
{
    static dint const EXTEND_DIST = 64;
    static dfloat const black[] = { 0, 0, 0, 0 };

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
    DENG2_ASSERT(vtx);
    return String("%1").arg(vtx->indexInMap());
}

struct drawvertexvisual_parameters_t
{
    dint maxDistance;
    bool drawOrigin;
    bool drawBar;
    bool drawLabel;
    QBitArray *drawnVerts;
};

static void drawVertexVisual(Vertex const &vertex, ddouble minHeight, ddouble maxHeight,
    drawvertexvisual_parameters_t &parms)
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
    dfloat const opacity = 1 - distToEye / parms.maxDistance;

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
 *              Use DDMAXFLOAT if the base is unknown.
 * @param min   Current maximal height to use as a base (will be overwritten).
 *              Use DDMINFLOAT if the base is unknown.
 *
 * @todo Don't stop when a zero-volume back neighbor is found; process all of
 * the neighbors at the specified vertex (the half-edge geometry will need to
 * be linked such that "outside" edges are neighbor-linked similarly to those
 * with a face).
 */
static void findMinMaxPlaneHeightsAtVertex(HEdge *base, dint edge,
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
            ConvexSubspace *subspace = hedge->hasFace()? &hedge->face().mapElementAs<ConvexSubspace>() : nullptr;
            if(!subspace) break;

            if(subspace->cluster().visFloor().heightSmoothed() < min)
                min = subspace->cluster().visFloor().heightSmoothed();

            if(subspace->cluster().visCeiling().heightSmoothed() > max)
                max = subspace->cluster().visCeiling().heightSmoothed();
        }
    }
}

static void drawSubspaceVertexs(ConvexSubspace &sub, drawvertexvisual_parameters_t &parms)
{
    SectorCluster &cluster = sub.cluster();
    ddouble const min      = cluster.  visFloor().heightSmoothed();
    ddouble const max      = cluster.visCeiling().heightSmoothed();

    HEdge *base  = sub.poly().hedge();
    HEdge *hedge = base;
    do
    {
        ddouble edgeMin = min;
        ddouble edgeMax = max;
        findMinMaxPlaneHeightsAtVertex(hedge, 0 /*left edge*/, edgeMin, edgeMax);

        drawVertexVisual(hedge->vertex(), min, max, parms);

    } while((hedge = &hedge->next()) != base);

    sub.forAllExtraMeshes([&min, &max, &parms] (Mesh &mesh)
    {
        for(HEdge *hedge : mesh.hedges())
        {
            drawVertexVisual(hedge->vertex(), min, max, parms);
            drawVertexVisual(hedge->twin().vertex(), min, max, parms);
        }
        return LoopContinue;
    });

    sub.forAllPolyobjs([&min, &max, &parms] (Polyobj &pob)
    {
        for(Line *line : pob.lines())
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

    dfloat oldLineWidth = -1;

    if(!devVertexBars && !devVertexIndices)
        return;

    AABoxd box(eyeOrigin.x - MAX_DISTANCE, eyeOrigin.y - MAX_DISTANCE,
               eyeOrigin.x + MAX_DISTANCE, eyeOrigin.y + MAX_DISTANCE);

    QBitArray drawnVerts(map.vertexCount());
    drawvertexvisual_parameters_t parms;
    parms.maxDistance = MAX_DISTANCE;
    parms.drawnVerts  = &drawnVerts;

    if(devVertexBars)
    {
        glDisable(GL_DEPTH_TEST);

        glEnable(GL_LINE_SMOOTH);
        oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
        DGL_SetFloat(DGL_LINE_WIDTH, 2);

        parms.drawBar   = true;
        parms.drawLabel = parms.drawOrigin = false;

        map.subspaceBlockmap().forAllInBox(box, [&box, &parms] (void *object)
        {
            // Check the bounds.
            auto &subspace = *(ConvexSubspace *)object;
            AABoxd const &polyBox = subspace.poly().aaBox();
            if(!(polyBox.maxX < box.minX ||
                 polyBox.minX > box.maxX ||
                 polyBox.minY > box.maxY ||
                 polyBox.maxY < box.minY))
            {
                drawSubspaceVertexs(subspace, parms);
            }
            return LoopContinue;
        });

        glEnable(GL_DEPTH_TEST);
    }

    // Draw the vertex origins.
    dfloat const oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);

    glEnable(GL_POINT_SMOOTH);
    DGL_SetFloat(DGL_POINT_SIZE, 6);

    glDisable(GL_DEPTH_TEST);

    parms.drawnVerts->fill(false);  // Process all again.
    parms.drawOrigin = true;
    parms.drawBar    = parms.drawLabel = false;
    map.subspaceBlockmap().forAllInBox(box, [&box, &parms] (void *object)
    {
        // Check the bounds.
        auto &subspace = *(ConvexSubspace *)object;
        AABoxd const &polyBox = subspace.poly().aaBox();
        if(!(polyBox.maxX < box.minX ||
             polyBox.minX > box.maxX ||
             polyBox.minY > box.maxY ||
             polyBox.maxY < box.minY))
        {
            drawSubspaceVertexs(subspace, parms);
        }
        return LoopContinue;
    });

    glEnable(GL_DEPTH_TEST);

    if(devVertexIndices)
    {
        parms.drawnVerts->fill(false);  // Process all again.
        parms.drawLabel = true;
        parms.drawBar   = parms.drawOrigin = false;
        map.subspaceBlockmap().forAllInBox(box, [&box, &parms] (void *object)
        {
            auto &subspace = *(ConvexSubspace *)object;
            // Check the bounds.
            AABoxd const &polyBox = subspace.poly().aaBox();
            if(!(polyBox.maxX < box.minX ||
                 polyBox.minX > box.maxX ||
                 polyBox.minY > box.maxY ||
                 polyBox.maxY < box.minY))
            {
                drawSubspaceVertexs(subspace, parms);
            }
            return LoopContinue;
        });
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

static String labelForCluster(SectorCluster const &cluster)
{
    return String::number(cluster.sector().indexInMap());
}

/**
 * Draw the sector cluster debugging aids.
 */
static void drawSectors(Map &map)
{
    static coord_t const MAX_LABEL_DIST = 1280;

    if(!devSectorIndices) return;

    // Draw per-cluster sector labels:
    map.forAllClusters([] (SectorCluster &cluster)
    {
        Vector3d const origin(cluster.center(), cluster.visPlane(Sector::Floor).heightSmoothed());
        ddouble const distToEye = (eyeOrigin - origin).length();
        if(distToEye < MAX_LABEL_DIST)
        {
            drawLabel(origin, labelForCluster(cluster),
                      distToEye / (DENG_GAMEVIEW_WIDTH / 2),
                      1 - distToEye / MAX_LABEL_DIST);
        }
        return LoopContinue;
    });
}

static String labelForThinker(thinker_t *thinker)
{
    DENG2_ASSERT(thinker);
    return String("%1").arg(thinker->id);
}

/**
 * Debugging aid for visualizing thinker IDs.
 */
static void drawThinkers(Map &map)
{
    static coord_t const MAX_THINKER_DIST = 2048;

    if(!devThinkerIds) return;

    map.thinkers().forAll(0x1 | 0x2, [] (thinker_t *th)
    {
        // Ignore non-mobjs.
        if(Thinker_IsMobjFunc(th->function))
        {
            Vector3d const origin   = Mobj_Center(*(mobj_t *)th);
            ddouble const distToEye = (eyeOrigin - origin).length();
            if(distToEye < MAX_THINKER_DIST)
            {
                drawLabel(origin, labelForThinker(th),
                          distToEye / (DENG_GAMEVIEW_WIDTH / 2),
                          1 - distToEye / MAX_THINKER_DIST);
            }
        }
        return LoopContinue;
    });
}

void Rend_LightGridVisual(LightGrid &lg)
{
    static Vector3f const red(1, 0, 0);
    static dint blink = 0;

    // Disabled?
    if(!devLightGrid) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Determine the grid reference of the view player.
    LightGrid::Index viewerGridIndex = 0;
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

    for(dint y = 0; y < lg.dimensions().y; ++y)
    {
        glBegin(GL_QUADS);
        for(dint x = 0; x < lg.dimensions().x; ++x)
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

MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec(dint wrapS, dint wrapT)
{
    return resSys().materialSpec(MapSurfaceContext, 0, 0, 0, 0, wrapS, wrapT,
                                 -1, -1, -1, true, true, false, false);
}

MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec()
{
    return Rend_MapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT);
}

/// Returns the texture variant specification for lightmaps.
TextureVariantSpec const &Rend_MapSurfaceLightmapTextureSpec()
{
    return resSys().textureSpec(TC_MAPSURFACE_LIGHTMAP, 0, 0, 0, 0, GL_CLAMP, GL_CLAMP,
                                1, -1, -1, false, false, false, true);
}

TextureVariantSpec const &Rend_MapSurfaceShinyTextureSpec()
{
    return resSys().textureSpec(TC_MAPSURFACE_REFLECTION, TSF_NO_COMPRESSION, 0, 0, 0,
                                GL_REPEAT, GL_REPEAT, 1, 1, -1, false, false, false, false);
}

TextureVariantSpec const &Rend_MapSurfaceShinyMaskTextureSpec()
{
    return resSys().textureSpec(TC_MAPSURFACE_REFLECTIONMASK, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                                -1, -1, -1, true, false, false, false);
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

D_CMD(MipMap)
{
    DENG2_UNUSED2(src, argc);

    dint newMipMode = String(argv[1]).toInt();
    if(newMipMode < 0 || newMipMode > 5)
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

static void detailFactorChanged()
{
    App_ResourceSystem().releaseGLTexturesByScheme("Details");
}

/*
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
    R_BuildTexGammaLut();
    GL_TexReset();
    LOG_GL_MSG("Texture gamma correction set to %f") << texGamma;
}

static void texQualityChanged()
{
    GL_TexReset();
}

static void useDynlightsChanged()
{
    if(!worldSys().hasMap()) return;

    // Unlink luminous objects.
    worldSys().map().thinkers()
        .forAll(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1, [](thinker_t *th)
    {
        Mobj_UnlinkLumobjs(reinterpret_cast<mobj_t *>(th));
        return LoopContinue;
    });
}

static void useSkylightChanged()
{
    scheduleFullLightGridUpdate();
}

static void useSmartFilterChanged()
{
    GL_TexReset();
}

void Rend_Register()
{
    C_VAR_INT("rend-bias", &useBias, 0, 0, 1);
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
    C_VAR_INT("rend-light-multitex", &useMultiTexLights, 0, 0, 1);
    C_VAR_INT("rend-light-num", &rendMaxLumobjs, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT2("rend-light-sky", &rendSkyLight, 0, 0, 1, useSkylightChanged);
    C_VAR_BYTE2("rend-light-sky-auto", &rendSkyLightAuto, 0, 0, 1, useSkylightChanged);
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
    C_VAR_INT("rend-tex-detail-multitex", &useMultiTexDetails, 0, 0, 1);
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

    C_VAR_BYTE("rend-bias-grid-debug", &devLightGrid, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_FLOAT("rend-bias-grid-debug-size", &devLightGridSize, 0, .1f, 100);
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

    C_CMD_FLAGS("lowres", "", LowRes, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("mipmap", "i", MipMap, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("texreset", "", TexReset, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("texreset", "s", TexReset, CMDF_NO_DEDICATED);

    BiasIllum::consoleRegister();
    LightDecoration::consoleRegister();
    LightGrid::consoleRegister();
    Lumobj::consoleRegister();
    SkyDrawable::consoleRegister();
    Rend_ModelRegister();
    Rend_ParticleRegister();
    Generator::consoleRegister();
    Rend_RadioRegister();
    Rend_SpriteRegister();
    LensFx_Register();
    fx::Bloom::consoleRegister();
    fx::Vignette::consoleRegister();
    fx::LensFlares::consoleRegister();
    Shard::consoleRegister();
    VR_ConsoleRegister();
}
