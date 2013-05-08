/** @file render/rend_main.cpp Map Renderer.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <de/libdeng2.h>

#include <QtAlgorithms>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_ui.h"

#include "edit_bias.h" /// @todo remove me
#include "network/net_main.h" /// @todo remove me

#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "Texture"
#include "SectionEdge"

#include "map/blockmapvisual.h"
#include "map/gamemap.h"
#include "map/lineowner.h"
#include "map/p_objlink.h"
#include "map/p_players.h"
#include "map/r_world.h"

#include "render/sprite.h"
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

boolean usingFog = false; // Is the fog in use?
float fogColor[4];
float fieldOfView = 95.0f;
byte smoothTexAnim = true;
int useShinySurfaces = true;

int useDynLights = true;
float dynlightFactor = .5f;
float dynlightFogBright = .15f;

int useWallGlow = true;
float glowFactor = .5f;
float glowHeightFactor = 3; // Glow height as a multiplier.
int glowHeightMax = 100; // 100 is the default (0-1024).

int useShadows = true;
float shadowFactor = 1.2f;
int shadowMaxRadius = 80;
int shadowMaxDistance = 1000;

float detailFactor = .5f;
float detailScale = 4;

coord_t vOrigin[3];
float vang, vpitch;
float viewsidex, viewsidey;

byte freezeRLs = false;
int devRendSkyMode = false;
byte devRendSkyAlways = false;

// Ambient lighting, rAmbient is used within the renderer, ambientLight is
// used to store the value of the ambient light cvar.
// The value chosen for rAmbient occurs in Rend_CalcLightModRange
// for convenience (since we would have to recalculate the matrix anyway).
int rAmbient = 0, ambientLight = 0;

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
float lightRangeCompression = 0;
float lightModRange[255];
byte devLightModRange = 0;

float rendLightDistanceAttenuation = 1024;
int rendLightAttenuateFixedColormap = 1;

int extraLight; // Bumped light from gun blasts.
float extraLightDelta;

byte devMobjVLights = 0; // @c 1= Draw mobj vertex lighting vector.
int devMobjBBox = 0; // 1 = Draw mobj bounding boxes (for debug)
int devPolyobjBBox = 0; // 1 = Draw polyobj bounding boxes (for debug)
DGLuint dlBBox = 0; // Display list: active-textured bbox model.

byte devVertexIndices = 0; // @c 1= Draw world vertex indices (for debug).
byte devVertexBars = 0; // @c 1= Draw world vertex position bars.
byte devSoundOrigins = 0; ///< cvar @c 1= Draw sound origin debug display.
byte devSurfaceVectors = 0;
byte devNoTexFix = 0;

static void Rend_RenderBoundingBoxes();
static DGLuint constructBBox(DGLuint name, float br);
static uint buildLeafPlaneGeometry(BspLeaf const &leaf, bool antiClockwise,
    coord_t height, rvertex_t **verts, uint *vertsSize);

// Draw state:
static Vector2d eyeOrigin; // Viewer origin.
static BspLeaf *currentBspLeaf; // BSP leaf currently being drawn.
static Vector3f currentSectorLightColor;
static float currentSectorLightLevel;
static bool firstBspLeaf; // No range checking for the first one.

void Rend_Register()
{
    C_VAR_FLOAT ("rend-camera-fov",                 &fieldOfView,                   0, 1, 179);

    C_VAR_FLOAT ("rend-glow",                       &glowFactor,                    0, 0, 2);
    C_VAR_INT   ("rend-glow-height",                &glowHeightMax,                 0, 0, 1024);
    C_VAR_FLOAT ("rend-glow-scale",                 &glowHeightFactor,              0, 0.1f, 10);
    C_VAR_INT   ("rend-glow-wall",                  &useWallGlow,                   0, 0, 1);

    C_VAR_INT2  ("rend-light",                      &useDynLights,                  0, 0, 1, LO_UnlinkMobjLumobjs);
    C_VAR_INT2  ("rend-light-ambient",              &ambientLight,                  0, 0, 255, Rend_CalcLightModRange);
    C_VAR_FLOAT ("rend-light-attenuation",          &rendLightDistanceAttenuation, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT ("rend-light-bright",               &dynlightFactor,                0, 0, 1);
    C_VAR_FLOAT2("rend-light-compression",          &lightRangeCompression,         0, -1, 1, Rend_CalcLightModRange);
    C_VAR_FLOAT ("rend-light-fog-bright",           &dynlightFogBright,             0, 0, 1);
    C_VAR_FLOAT2("rend-light-sky",                  &rendSkyLight,                  0, 0, 1, LG_MarkAllForUpdate);
    C_VAR_BYTE2 ("rend-light-sky-auto",             &rendSkyLightAuto,              0, 0, 1, LG_MarkAllForUpdate);
    C_VAR_FLOAT ("rend-light-wall-angle",           &rendLightWallAngle,            CVF_NO_MAX, 0, 0);
    C_VAR_BYTE  ("rend-light-wall-angle-smooth",    &rendLightWallAngleSmooth,      0, 0, 1);

    C_VAR_BYTE  ("rend-map-material-precache",      &precacheMapMaterials,          0, 0, 1);

    C_VAR_INT   ("rend-shadow",                     &useShadows,                    0, 0, 1);
    C_VAR_FLOAT ("rend-shadow-darkness",            &shadowFactor,                  0, 0, 2);
    C_VAR_INT   ("rend-shadow-far",                 &shadowMaxDistance,             CVF_NO_MAX, 0, 0);
    C_VAR_INT   ("rend-shadow-radius-max",          &shadowMaxRadius,               CVF_NO_MAX, 0, 0);

    C_VAR_BYTE  ("rend-tex-anim-smooth",            &smoothTexAnim,                 0, 0, 1);
    C_VAR_INT   ("rend-tex-shiny",                  &useShinySurfaces,              0, 0, 1);

    C_VAR_INT   ("rend-dev-sky",                    &devRendSkyMode,                CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-sky-always",             &devRendSkyAlways,              CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-freeze",                 &freezeRLs,                     CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT   ("rend-dev-cull-leafs",             &devNoCulling,                  CVF_NO_ARCHIVE,0,1);
    C_VAR_INT   ("rend-dev-mobj-bbox",              &devMobjBBox,                   CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-mobj-show-vlights",      &devMobjVLights,                CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT   ("rend-dev-polyobj-bbox",           &devPolyobjBBox,                CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-light-mod",              &devLightModRange,              CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-tex-showfix",            &devNoTexFix,                   CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-blockmap-debug",         &bmapShowDebug,                 CVF_NO_ARCHIVE, 0, 4);
    C_VAR_FLOAT ("rend-dev-blockmap-debug-size",    &bmapDebugSize,                 CVF_NO_ARCHIVE, .1f, 100);
    C_VAR_BYTE  ("rend-dev-vertex-show-indices",    &devVertexIndices,              CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-vertex-show-bars",       &devVertexBars,                 CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-surface-show-vectors",   &devSurfaceVectors,             CVF_NO_ARCHIVE, 0, 7);
    C_VAR_BYTE  ("rend-dev-soundorigins",           &devSoundOrigins,               CVF_NO_ARCHIVE, 0, 7);

    RL_Register();
    LO_Register();
    Rend_DecorRegister();
    SB_Register();
    LG_Register();
    Sky_Register();
    Rend_ModelRegister();
    Rend_ParticleRegister();
    Rend_RadioRegister();
    Rend_SpriteRegister();
    Rend_ConsoleRegister();
    Vignette_Register();
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

static inline bool isNullLeaf(BspLeaf *leaf)
{
    return !leaf || !leaf->hasWorldVolume();
}

coord_t Rend_PointDist3D(coord_t const point[3])
{
    return M_ApproxDistance3(vOrigin[VX] - point[VX],
                             vOrigin[VZ] - point[VY],
                             1.2 * (vOrigin[VY] - point[VZ]));
}

void Rend_Init()
{
    C_Init();
    RL_Init();
    Sky_Init();
}

void Rend_Shutdown()
{
    RL_Shutdown();
}

/// World/map renderer reset.
void Rend_Reset()
{
    LO_Clear(); // Free lumobj stuff.
    if(dlBBox)
    {
        GL_DeleteLists(dlBBox, 1);
        dlBBox = 0;
    }
}

void Rend_ModelViewMatrix(boolean useAngles)
{
    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

    vOrigin[VX] = viewData->current.origin[VX];
    vOrigin[VY] = viewData->current.origin[VZ];
    vOrigin[VZ] = viewData->current.origin[VY];
    vang = viewData->current.angle / (float) ANGLE_MAX *360 - 90;
    vpitch = viewData->current.pitch * 85.0 / 110.0;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if(useAngles)
    {
        glRotatef(vpitch, 1, 0, 0);
        glRotatef(vang, 0, 1, 0);
    }
    glScalef(1, 1.2f, 1);      // This is the aspect correction.
    glTranslatef(-vOrigin[VX], -vOrigin[VY], -vOrigin[VZ]);
}

static inline double viewFacingDot(Vector2d const &v1, Vector2d const &v2)
{
    // The dot product.
    return (v1.y - v2.y) * (v1.x - vOrigin[VX]) + (v2.x - v1.x) * (v1.y - vOrigin[VZ]);
}

static void Rend_VertexColorsGlow(ColorRawf *colors, uint num, float glow)
{
    for(uint i = 0; i < num; ++i)
    {
        ColorRawf *c = &colors[i];
        c->rgba[CR] = c->rgba[CG] = c->rgba[CB] = glow;
    }
}

static void Rend_VertexColorsAlpha(ColorRawf *colors, uint num, float alpha)
{
    for(uint i = 0; i < num; ++i)
    {
        colors[i].rgba[CA] = alpha;
    }
}

void Rend_ApplyTorchLight(float color[3], float distance)
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
            color[CR] += d * torchColor[CR];
            color[CG] += d * torchColor[CG];
            color[CB] += d * torchColor[CB];
        }
        else
        {
            color[CR] += d * ((color[CR] * torchColor[CR]) - color[CR]);
            color[CG] += d * ((color[CG] * torchColor[CG]) - color[CG]);
            color[CB] += d * ((color[CB] * torchColor[CB]) - color[CB]);
        }
    }
}

float Rend_AttenuateLightLevel(float distToViewer, float lightLevel)
{
    if(distToViewer > 0 && rendLightDistanceAttenuation > 0)
    {
        float real = lightLevel -
            (distToViewer - 32) / rendLightDistanceAttenuation *
                (1 - lightLevel);

        float minimum = lightLevel * lightLevel + (lightLevel - .63f) * .5f;
        if(real < minimum)
            real = minimum; // Clamp it.

        return real;
    }

    return lightLevel;
}

float Rend_ExtraLightDelta()
{
    return extraLightDelta;
}

Vector3f const &Rend_SectorLightColor(Sector const &sector)
{
    static Vector3f skyLightColor;
    static Vector3f oldSkyAmbientColor(-1.f, -1.f, -1.f);
    static float oldRendSkyLight = -1;

    if(rendSkyLight > .001f && sector.hasSkyMaskedPlane())
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

            // When the sky light color changes we must update the lightgrid.
            LG_MarkAllForUpdate();
            oldSkyAmbientColor = Vector3f(ambientColor->rgb);
        }

        oldRendSkyLight = rendSkyLight;
        return skyLightColor;
    }

    // A non-skylight sector (i.e., everything else!)
    // Return the sector's ambient light color.
    return sector.lightColor();
}

static void lightVertex(ColorRawf &color, rvertex_t const &vtx, float lightLevel,
                        Vector3f const &ambientColor)
{
    float const dist = Rend_PointDist2D(vtx.pos);
    float lightVal = Rend_AttenuateLightLevel(dist, lightLevel);

    // Add extra light.
    lightVal += Rend_ExtraLightDelta();

    Rend_ApplyLightAdaptation(&lightVal);

    // Mix with the surface color.
    for(int i = 0; i < 3; ++i)
    {
        color.rgba[i] = lightVal * ambientColor[i];
    }
}

static void lightVertices(uint num, ColorRawf *colors, rvertex_t const *verts,
                          float lightLevel, Vector3f const &ambientColor)
{
    for(uint i = 0; i < num; ++i)
    {
        lightVertex(colors[i], verts[i], lightLevel, ambientColor);
    }
}

static void torchLightVertices(uint num, ColorRawf *colors, rvertex_t const *verts)
{
    for(uint i = 0; i < num; ++i)
    {
        Rend_ApplyTorchLight(colors[i].rgba, Rend_PointDist2D(verts[i].pos));
    }
}

int RIT_FirstDynlightIterator(dynlight_t const *dyn, void *parameters)
{
    dynlight_t const **ptr = (dynlight_t const **)parameters;
    *ptr = dyn;
    return 1; // Stop iteration.
}

static inline MaterialVariantSpec const &mapSurfaceMaterialSpec(int wrapS, int wrapT)
{
    return App_Materials().variantSpec(MapSurfaceContext, 0, 0, 0, 0, wrapS, wrapT,
                                       -1, -1, -1, true, true, false, false);
}

/**
 * This doesn't create a rendering primitive but a vissprite! The vissprite
 * represents the masked poly and will be rendered during the rendering
 * of sprites. This is necessary because all masked polygons must be
 * rendered back-to-front, or there will be alpha artifacts along edges.
 */
void Rend_AddMaskedPoly(rvertex_t const *rvertices, ColorRawf const *rcolors,
    coord_t wallLength, MaterialVariant *material, Vector2f const &materialOrigin,
    blendmode_t blendMode, uint lightListIdx, float glow)
{
    vissprite_t *vis = R_NewVisSprite();

    vis->type = VSPR_MASKED_WALL;
    vis->origin[VX] = (rvertices[0].pos[VX] + rvertices[3].pos[VX]) / 2;
    vis->origin[VY] = (rvertices[0].pos[VY] + rvertices[3].pos[VY]) / 2;
    vis->origin[VZ] = (rvertices[0].pos[VZ] + rvertices[3].pos[VZ]) / 2;
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
                (rvertices[3].pos[VZ] - rvertices[0].pos[VZ]) / ms.height();

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
        material = material->generalCase().chooseVariant(mapSurfaceMaterialSpec(wrapS, wrapT), true);
    }

    VS_WALL(vis)->material = material;
    VS_WALL(vis)->blendMode = blendMode;

    for(int i = 0; i < 4; ++i)
    {
        VS_WALL(vis)->vertices[i].pos[VX] = rvertices[i].pos[VX];
        VS_WALL(vis)->vertices[i].pos[VY] = rvertices[i].pos[VY];
        VS_WALL(vis)->vertices[i].pos[VZ] = rvertices[i].pos[VZ];

        for(int c = 0; c < 4; ++c)
        {
            /// @todo Do not clamp here.
            VS_WALL(vis)->vertices[i].color[c] = MINMAX_OF(0, rcolors[i].rgba[c], 1);
        }
    }

    /// @todo Semitransparent masked polys arn't lit atm
    if(glow < 1 && lightListIdx && numTexUnits > 1 && envModAdd &&
       !(rcolors[0].rgba[CA] < 1))
    {
        dynlight_t const *dyn = 0;

        /**
         * The dynlights will have already been sorted so that the brightest
         * and largest of them is first in the list. So grab that one.
         */
        LO_IterateProjections(lightListIdx, RIT_FirstDynlightIterator, (void *)&dyn);

        VS_WALL(vis)->modTex = dyn->texture;
        VS_WALL(vis)->modTexCoord[0][0] = dyn->s[0];
        VS_WALL(vis)->modTexCoord[0][1] = dyn->t[0];
        VS_WALL(vis)->modTexCoord[1][0] = dyn->s[1];
        VS_WALL(vis)->modTexCoord[1][1] = dyn->t[1];
        for(int c = 0; c < 4; ++c)
        {
            VS_WALL(vis)->modColor[c] = dyn->color.rgba[c];
        }
    }
    else
    {
        VS_WALL(vis)->modTex = 0;
    }
}

static void quadTexCoords(rtexcoord_t *tc, rvertex_t const *rverts,
    coord_t wallLength, Vector3d const &topLeft)
{
    tc[0].st[0] = tc[1].st[0] = rverts[0].pos[VX] - topLeft.x;
    tc[3].st[1] = tc[1].st[1] = rverts[0].pos[VY] - topLeft.y;
    tc[3].st[0] = tc[2].st[0] = tc[0].st[0] + wallLength;
    tc[2].st[1] = tc[3].st[1] + (rverts[1].pos[VZ] - rverts[0].pos[VZ]);
    tc[0].st[1] = tc[3].st[1] + (rverts[3].pos[VZ] - rverts[2].pos[VZ]);
}

static void quadLightCoords(rtexcoord_t *tc, float const s[2], float const t[2])
{
    tc[1].st[0] = tc[0].st[0] = s[0];
    tc[1].st[1] = tc[3].st[1] = t[0];
    tc[3].st[0] = tc[2].st[0] = s[1];
    tc[2].st[1] = tc[0].st[1] = t[1];
}

static float shinyVertical(float dy, float dx)
{
    return ((atan(dy/dx) / (PI/2)) + 1) / 2;
}

static void quadShinyTexCoords(rtexcoord_t *tc, rvertex_t const *topLeft,
    rvertex_t const *bottomRight, coord_t wallLength)
{
    vec2f_t surface, normal, projected, s, reflected, view;
    float distance, angle, prevAngle = 0;
    uint i;

    // Quad surface vector.
    V2f_Set(surface, (bottomRight->pos[VX] - topLeft->pos[VX]) / wallLength,
                     (bottomRight->pos[VY] - topLeft->pos[VY]) / wallLength);

    V2f_Set(normal, surface[VY], -surface[VX]);

    // Calculate coordinates based on viewpoint and surface normal.
    for(i = 0; i < 2; ++i)
    {
        // View vector.
        V2f_Set(view, vOrigin[VX] - (i == 0? topLeft->pos[VX] : bottomRight->pos[VX]),
                      vOrigin[VZ] - (i == 0? topLeft->pos[VY] : bottomRight->pos[VY]));

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
        tc[ (i == 0 ? 1 : 2) ].st[0] = tc[ (i == 0 ? 0 : 3) ].st[0] =
            angle + .3f; /*acos(-dot)/PI*/

        tc[ (i == 0 ? 0 : 2) ].st[1] =
            shinyVertical(vOrigin[VY] - bottomRight->pos[VZ], distance);

        // Vertical coordinates.
        tc[ (i == 0 ? 1 : 3) ].st[1] =
            shinyVertical(vOrigin[VY] - topLeft->pos[VZ], distance);
    }
}

static void flatShinyTexCoords(rtexcoord_t *tc, float const xyz[3])
{
    float distance, offset;
    vec2f_t view, start;

    // View vector.
    V2f_Set(view, vOrigin[VX] - xyz[VX], vOrigin[VZ] - xyz[VY]);

    distance = V2f_Normalize(view);
    if(distance < 10)
    {
        // Too small distances cause an ugly 'crunch' below and above
        // the viewpoint.
        distance = 10;
    }

    // Offset from the normal view plane.
    V2f_Set(start, vOrigin[VX], vOrigin[VZ]);

    offset = ((start[VY] - xyz[VY]) * sin(.4f)/*viewFrontVec[VX]*/ -
              (start[VX] - xyz[VX]) * cos(.4f)/*viewFrontVec[VZ]*/);

    tc->st[0] = ((shinyVertical(offset, distance) - .5f) * 2) + .5f;
    tc->st[1] = shinyVertical(vOrigin[VY] - xyz[VZ], distance);
}

struct rendworldpoly_params_t
{
    bool            isWall;
    int             flags; /// @ref rendpolyFlags
    blendmode_t     blendMode;
    Vector3d const *texTL;
    Vector3d const *texBR;
    Vector2f const *materialOrigin;
    Vector2f const *materialScale;
    Vector3f const *normal; // Surface normal.
    float           alpha;
    float           surfaceLightLevelDL;
    float           surfaceLightLevelDR;
    Vector3f const *surfaceColor;

    uint            lightListIdx; // List of lights that affect this poly.
    uint            shadowListIdx; // List of shadows that affect this poly.
    float           glowing;
    bool            forceOpaque;

// For bias:
    MapElement     *mapElement;
    int             elmIdx;
    biassurface_t  *bsuf;

// Wall only:
    struct {
        coord_t sectionWidth;
        Vector3f const *surfaceColor2; // Secondary color.
        SectionEdge const *leftEdge;
        SectionEdge const *rightEdge;
    } wall;
};

static bool renderWorldPoly(rvertex_t *rvertices, uint numVertices,
    rendworldpoly_params_t const &p, MaterialSnapshot const &ms)
{
    DENG_ASSERT(rvertices != 0);

    uint const realNumVertices = (p.isWall? 3 + p.wall.leftEdge->divisionCount() + 3 + p.wall.rightEdge->divisionCount() : numVertices);
    bool const mustSubdivide = (p.isWall && (p.wall.leftEdge->divisionCount() || p.wall.rightEdge->divisionCount()));

    bool const skyMaskedMaterial = ((p.flags & RPF_SKYMASK) || (ms.material().isSkyMasked()));
    bool const drawAsVisSprite   = (!p.forceOpaque && !(p.flags & RPF_SKYMASK) && (!ms.isOpaque() || p.alpha < 1 || p.blendMode > 0));

    bool useLights = false, useShadows = false, hasDynlights = false;

    // Map RTU configuration from prepared MaterialSnapshot(s).
    rtexmapunit_t const *primaryRTU       = (!(p.flags & RPF_SKYMASK))? &ms.unit(RTU_PRIMARY) : NULL;
    rtexmapunit_t const *primaryDetailRTU = (r_detail && !(p.flags & RPF_SKYMASK) && ms.unit(RTU_PRIMARY_DETAIL).hasTexture())? &ms.unit(RTU_PRIMARY_DETAIL) : NULL;
    rtexmapunit_t const *interRTU         = (!(p.flags & RPF_SKYMASK) && ms.unit(RTU_INTER).hasTexture())? &ms.unit(RTU_INTER) : NULL;
    rtexmapunit_t const *interDetailRTU   = (r_detail && !(p.flags & RPF_SKYMASK) && ms.unit(RTU_INTER_DETAIL).hasTexture())? &ms.unit(RTU_INTER_DETAIL) : NULL;
    rtexmapunit_t const *shinyRTU         = (useShinySurfaces && !(p.flags & RPF_SKYMASK) && ms.unit(RTU_REFLECTION).hasTexture())? &ms.unit(RTU_REFLECTION) : NULL;
    rtexmapunit_t const *shinyMaskRTU     = (useShinySurfaces && !(p.flags & RPF_SKYMASK) && ms.unit(RTU_REFLECTION).hasTexture() && ms.unit(RTU_REFLECTION_MASK).hasTexture())? &ms.unit(RTU_REFLECTION_MASK) : NULL;

    ColorRawf *rcolors          = !skyMaskedMaterial? R_AllocRendColors(realNumVertices) : 0;
    rtexcoord_t *primaryCoords  = R_AllocRendTexCoords(realNumVertices);
    rtexcoord_t *interCoords    = interRTU? R_AllocRendTexCoords(realNumVertices) : 0;

    ColorRawf *shinyColors      = 0;
    rtexcoord_t *shinyTexCoords = 0;
    rtexcoord_t *modCoords      = 0;

    DGLuint modTex = 0;
    float modTexSt[2][2] = {{ 0, 0 }, { 0, 0 }};
    ColorRawf modColor;

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
            if(useLights && RL_IsMTexLights())
            {
                dynlight_t *dyn = 0;
                LO_IterateProjections(p.lightListIdx, RIT_FirstDynlightIterator, (void *)&dyn);

                modCoords = R_AllocRendTexCoords(realNumVertices);

                modTex = dyn->texture;
                modColor.red   = dyn->color.red;
                modColor.green = dyn->color.green;
                modColor.blue  = dyn->color.blue;
                modTexSt[0][0] = dyn->s[0];
                modTexSt[0][1] = dyn->s[1];
                modTexSt[1][0] = dyn->t[0];
                modTexSt[1][1] = dyn->t[1];
            }
        }
    }

    if(p.isWall)
    {
        // Primary texture coordinates.
        quadTexCoords(primaryCoords, rvertices, p.wall.sectionWidth, *p.texTL);

        // Blend texture coordinates.
        if(interRTU && !drawAsVisSprite)
            quadTexCoords(interCoords, rvertices, p.wall.sectionWidth, *p.texTL);

        // Shiny texture coordinates.
        if(shinyRTU && !drawAsVisSprite)
            quadShinyTexCoords(shinyTexCoords, &rvertices[1], &rvertices[2], p.wall.sectionWidth);

        // First light texture coordinates.
        if(modTex && RL_IsMTexLights())
            quadLightCoords(modCoords, modTexSt[0], modTexSt[1]);
    }
    else
    {
        for(uint i = 0; i < numVertices; ++i)
        {
            rvertex_t const &vtx = rvertices[i];

            float const xyz[3] = {
                vtx.pos[VX] - float(p.texTL->x),
                vtx.pos[VY] - float(p.texTL->y),
                vtx.pos[VZ] - float(p.texTL->z)
            };

            // Primary texture coordinates.
            if(primaryRTU)
            {
                rtexcoord_t &tc = primaryCoords[i];
                tc.st[0] =  xyz[VX];
                tc.st[1] = -xyz[VY];
            }

            // Blend primary texture coordinates.
            if(interRTU)
            {
                rtexcoord_t &tc = interCoords[i];
                tc.st[0] =  xyz[VX];
                tc.st[1] = -xyz[VY];
            }

            // Shiny texture coordinates.
            if(shinyRTU)
            {
                flatShinyTexCoords(&shinyTexCoords[i], vtx.pos);
            }

            // First light texture coordinates.
            if(modTex && RL_IsMTexLights())
            {
                rtexcoord_t &tc = modCoords[i];
                float const width  = p.texBR->x - p.texTL->x;
                float const height = p.texBR->y - p.texTL->y;

                tc.st[0] = ((p.texBR->x - vtx.pos[VX]) / width  * modTexSt[0][0]) + (xyz[VX] / width  * modTexSt[0][1]);
                tc.st[1] = ((p.texBR->y - vtx.pos[VY]) / height * modTexSt[1][0]) + (xyz[VY] / height * modTexSt[1][1]);
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
            Rend_VertexColorsGlow(rcolors, numVertices, glowStrength);
        }
        else
        {
            // Non-uniform color.
            if(useBias && p.bsuf)
            {
                // Do BIAS lighting for this poly.
                SB_RendPoly(rcolors, p.bsuf, rvertices, numVertices, *p.normal,
                            currentSectorLightLevel, p.mapElement, p.elmIdx);

                if(p.glowing > 0)
                {
                    for(uint i = 0; i < numVertices; ++i)
                    {
                        rcolors[i].rgba[CR] = de::clamp(0.f, rcolors[i].rgba[CR] + p.glowing, 1.f);
                        rcolors[i].rgba[CG] = de::clamp(0.f, rcolors[i].rgba[CG] + p.glowing, 1.f);
                        rcolors[i].rgba[CB] = de::clamp(0.f, rcolors[i].rgba[CB] + p.glowing, 1.f);
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
                        lightVertex(rcolors[0], rvertices[0], llL, vColor);
                        lightVertex(rcolors[1], rvertices[1], llL, vColor);
                        lightVertex(rcolors[2], rvertices[2], llR, vColor);
                        lightVertex(rcolors[3], rvertices[3], llR, vColor);
                    }
                    else
                    {
                        lightVertices(numVertices, rcolors, rvertices, llL, vColor);
                    }
                }
                else
                {
                    // Use sector light+color only.
                    if(p.isWall && llL != llR)
                    {
                        lightVertex(rcolors[0], rvertices[0], llL, currentSectorLightColor);
                        lightVertex(rcolors[1], rvertices[1], llL, currentSectorLightColor);
                        lightVertex(rcolors[2], rvertices[2], llR, currentSectorLightColor);
                        lightVertex(rcolors[3], rvertices[3], llR, currentSectorLightColor);
                    }
                    else
                    {
                        lightVertices(numVertices, rcolors, rvertices, llL, currentSectorLightColor);
                    }
                }

                // Bottom color (if different from top)?
                if(p.isWall && p.wall.surfaceColor2)
                {
                    // Blend sector light+color+surfacecolor
                    Vector3f vColor = (*p.wall.surfaceColor2) * currentSectorLightColor;

                    lightVertex(rcolors[0], rvertices[0], llL, vColor);
                    lightVertex(rcolors[2], rvertices[2], llR, vColor);
                }
            }

            // Apply torch light?
            if(viewPlayer->shared.fixedColorMap)
            {
                torchLightVertices(numVertices, rcolors, rvertices);
            }
        }

        if(shinyRTU && !drawAsVisSprite)
        {
            // Strength of the shine.
            Vector3f const &minColor = ms.shineMinColor();
            for(uint i = 0; i < numVertices; ++i)
            {
                ColorRawf &color = shinyColors[i];
                color.rgba[CR] = de::max(rcolors[i].rgba[CR], minColor.x);
                color.rgba[CG] = de::max(rcolors[i].rgba[CG], minColor.y);
                color.rgba[CB] = de::max(rcolors[i].rgba[CB], minColor.z);
                color.rgba[CA] = shinyRTU->opacity;
            }
        }

        // Apply uniform alpha.
        Rend_VertexColorsAlpha(rcolors, numVertices, p.alpha);
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
            avgLightlevel += rcolors[i].rgba[CR];
            avgLightlevel += rcolors[i].rgba[CG];
            avgLightlevel += rcolors[i].rgba[CB];
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
        DENG_ASSERT(p.isWall);

        /**
         * Masked polys (walls) get a special treatment (=> vissprite).
         * This is needed because all masked polys must be sorted (sprites
         * are masked polys). Otherwise there will be artifacts.
         */
        Rend_AddMaskedPoly(rvertices, rcolors, p.wall.sectionWidth, &ms.materialVariant(),
                           *p.materialOrigin, p.blendMode, p.lightListIdx, p.glowing);

        R_FreeRendTexCoords(primaryCoords);
        R_FreeRendColors(rcolors);
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
        parm.rvertices       = rvertices;
        parm.numVertices     = numVertices;
        parm.realNumVertices = realNumVertices;
        parm.lastIdx         = 0;
        parm.texTL           = p.texTL;
        parm.texBR           = p.texBR;
        parm.isWall          = p.isWall;
        if(p.isWall)
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
        parm.rvertices       = rvertices;
        parm.numVertices     = numVertices;
        parm.realNumVertices = realNumVertices;
        parm.texTL           = p.texTL;
        parm.texBR           = p.texBR;
        parm.isWall          = p.isWall;
        if(p.isWall)
        {
            parm.wall.leftEdge  = p.wall.leftEdge;
            parm.wall.rightEdge = p.wall.rightEdge;
        }

        Rend_RenderShadowProjections(p.shadowListIdx, parm);
    }

    // Map RTU state from the prepared texture units in the MaterialSnapshot.
    RL_LoadDefaultRtus();
    RL_MapRtu(RTU_PRIMARY,         primaryRTU);
    RL_MapRtu(RTU_PRIMARY_DETAIL,  primaryDetailRTU);
    RL_MapRtu(RTU_INTER,           interRTU);
    RL_MapRtu(RTU_INTER_DETAIL,    interDetailRTU);
    RL_MapRtu(RTU_REFLECTION,      shinyRTU);
    RL_MapRtu(RTU_REFLECTION_MASK, shinyMaskRTU);

    if(primaryRTU)
    {
        if(p.materialOrigin) RL_Rtu_TranslateOffset(RTU_PRIMARY, *p.materialOrigin);
        if(p.materialScale)  RL_Rtu_ScaleST(RTU_PRIMARY, *p.materialScale);
    }

    if(primaryDetailRTU)
    {
        if(p.materialOrigin) RL_Rtu_TranslateOffset(RTU_PRIMARY_DETAIL, *p.materialOrigin);
    }

    if(interRTU)
    {
        if(p.materialOrigin) RL_Rtu_TranslateOffset(RTU_INTER, *p.materialOrigin);
        if(p.materialScale)  RL_Rtu_ScaleST(RTU_INTER, *p.materialScale);
    }

    if(interDetailRTU)
    {
        if(p.materialOrigin) RL_Rtu_TranslateOffset(RTU_INTER_DETAIL, *p.materialOrigin);
    }

    if(shinyMaskRTU)
    {
        if(p.materialOrigin) RL_Rtu_TranslateOffset(RTU_REFLECTION_MASK, *p.materialOrigin);
        if(p.materialScale)  RL_Rtu_ScaleST(RTU_REFLECTION_MASK, *p.materialScale);
    }

    // Write multiple polys depending on rend params.
    if(mustSubdivide)
    {
        SectionEdge const &leftEdge = *p.wall.leftEdge;
        SectionEdge const &rightEdge = *p.wall.rightEdge;

        /*
         * Need to swap indices around into fans set the position
         * of the division vertices, interpolate texcoords and
         * color.
         */

        rvertex_t origVerts[4];
        std::memcpy(origVerts, rvertices, sizeof(origVerts));

        rtexcoord_t origTexCoords[4];
        std::memcpy(origTexCoords, primaryCoords, sizeof(origTexCoords));

        ColorRawf origColors[4];
        if(rcolors || shinyColors)
        {
            std::memcpy(origColors, rcolors, sizeof(origColors));
        }

        float const bL = origVerts[0].pos[VZ];
        float const tL = origVerts[1].pos[VZ];
        float const bR = origVerts[2].pos[VZ];
        float const tR = origVerts[3].pos[VZ];

        R_DivVerts(rvertices, origVerts, leftEdge, rightEdge);
        R_DivTexCoords(primaryCoords, origTexCoords, leftEdge, rightEdge,
                       bL, tL, bR, tR);

        if(rcolors)
        {
            R_DivVertColors(rcolors, origColors, leftEdge, rightEdge,
                            bL, tL, bR, tR);
        }

        if(interCoords)
        {
            rtexcoord_t origTexCoords2[4];
            std::memcpy(origTexCoords2, interCoords, sizeof(origTexCoords2));
            R_DivTexCoords(interCoords, origTexCoords2, leftEdge, rightEdge,
                           bL, tL, bR, tR);
        }

        if(modCoords)
        {
            rtexcoord_t origTexCoords5[4];
            std::memcpy(origTexCoords5, modCoords, sizeof(origTexCoords5));
            R_DivTexCoords(modCoords, origTexCoords5, leftEdge, rightEdge,
                           bL, tL, bR, tR);
        }

        if(shinyTexCoords)
        {
            rtexcoord_t origShinyTexCoords[4];
            std::memcpy(origShinyTexCoords, shinyTexCoords, sizeof(origShinyTexCoords));
            R_DivTexCoords(shinyTexCoords, origShinyTexCoords, leftEdge, rightEdge,
                           bL, tL, bR, tR);
        }

        if(shinyColors)
        {
            ColorRawf origShinyColors[4];
            std::memcpy(origShinyColors, shinyColors, sizeof(origShinyColors));
            R_DivVertColors(shinyColors, origShinyColors, leftEdge, rightEdge,
                            bL, tL, bR, tR);
        }

        RL_AddPolyWithCoordsModulationReflection(PT_FAN, p.flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
                                                 3 + rightEdge.divisionCount(),
                                                 rvertices + 3 + leftEdge.divisionCount(),
                                                 rcolors? rcolors + 3 + leftEdge.divisionCount() : 0,
                                                 primaryCoords + 3 + leftEdge.divisionCount(),
                                                 interCoords? interCoords + 3 + leftEdge.divisionCount() : 0,
                                                 modTex, &modColor, modCoords? modCoords + 3 + leftEdge.divisionCount() : 0,
                                                 shinyColors + 3 + leftEdge.divisionCount(),
                                                 shinyTexCoords? shinyTexCoords + 3 + leftEdge.divisionCount() : 0,
                                                 shinyMaskRTU? primaryCoords + 3 + leftEdge.divisionCount() : 0);

        RL_AddPolyWithCoordsModulationReflection(PT_FAN, p.flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
                                                 3 + leftEdge.divisionCount(),
                                                 rvertices, rcolors, primaryCoords, interCoords,
                                                 modTex, &modColor, modCoords,
                                                 shinyColors, shinyTexCoords,
                                                 shinyMaskRTU? primaryCoords : 0);
    }
    else
    {
        RL_AddPolyWithCoordsModulationReflection(p.isWall? PT_TRIANGLE_STRIP : PT_FAN,
                                                 p.flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
                                                 numVertices, rvertices, rcolors,
                                                 primaryCoords, interCoords,
                                                 modTex, &modColor, modCoords,
                                                 shinyColors, shinyTexCoords, shinyMaskRTU? primaryCoords : 0);
    }

    R_FreeRendTexCoords(primaryCoords);
    R_FreeRendTexCoords(interCoords);
    R_FreeRendTexCoords(modCoords);
    R_FreeRendTexCoords(shinyTexCoords);
    R_FreeRendColors(rcolors);
    R_FreeRendColors(shinyColors);

    return (p.forceOpaque || skyMaskedMaterial ||
            !(p.alpha < 1 || !ms.isOpaque() || p.blendMode > 0));
}

static Material *chooseSurfaceMaterialForTexturingMode(Surface const &surface)
{
    switch(renderTextures)
    {
    case 0: // No texture mode.
    case 1: // Normal mode.
        if(devNoTexFix && surface.hasFixMaterial())
        {
            // Missing material debug mode -- use special "missing" material.
            return &App_Materials().find(de::Uri("System", Path("missing"))).material();
        }

        // Use the surface-bound material.
        return surface.materialPtr();

    case 2: // Lighting debug mode.
        if(surface.hasMaterial() && !(!devNoTexFix && surface.hasFixMaterial()))
        {
            if(!surface.hasSkyMaskedMaterial() || devRendSkyMode)
            {
                // Use the special "gray" material.
                return &App_Materials().find(de::Uri("System", Path("gray"))).material();
            }
        }
        break;

    default: break;
    }

    // No material, then.
    return 0;
}

static float calcLightLevelDelta(Vector3f const &normal)
{
    return (1.0f / 255) * (normal.x * 18) * rendLightWallAngle;
}

static Line::Side *findSideBlendNeighbor(Line::Side const &side, int edge, binangle_t *diff = 0)
{
    LineOwner const *farVertOwner = side.line().vertexOwner(side.lineSideId() ^ edge);

    if(diff) *diff = 0;

    Line *neighbor;
    if(R_SideBackClosed(side))
    {
        neighbor = R_FindSolidLineNeighbor(side.sectorPtr(), &side.line(), farVertOwner, edge, diff);
    }
    else
    {
        neighbor = R_FindLineNeighbor(side.sectorPtr(), &side.line(), farVertOwner, edge, diff);
    }

    // No suitable line neighbor?
    if(!neighbor) return 0;

    // Choose the correct side of the neighbor (determined by which vertex is shared).
    Line::Side *otherSide;
    if(&neighbor->vertex(edge ^ 1) == &side.vertex(edge))
        otherSide = &neighbor->front();
    else
        otherSide = &neighbor->back();

    // We can only blend neighbors with surface sections.
    return otherSide->hasSections()? otherSide : 0;
}

/**
 * Determines whether light level delta smoothing should be performed for
 * the given pair of map surfaces (which are assumed to share an edge).
 *
 * Yes if the angle between the two lines is less than 45 degrees.
 * @todo Should be user customizable with a Material property. -ds
 *
 * @param sufA       The "left"  map surface which shares an edge with @a sufB.
 * @param sufB       The "right" map surface which shares an edge with @a sufA.
 * @param angleDiff  Angle difference (i.e., normal delta) between the two surfaces.
 */
static bool shouldSmoothLightLevels(Surface &sufA, Surface &sufB, binangle_t angleDiff)
{
    DENG_UNUSED(sufA);
    DENG_UNUSED(sufB);
    return INRANGE_OF(angleDiff, BANG_180, BANG_45);
}

/**
 * The DOOM lighting model applies a sector light level delta when drawing
 * walls based on their 2D world angle.
 *
 * @todo: Use the half-edge rings instead of LineOwners.
 *
 * @param side     Line side to calculate light level deltas for.
 * @param section  Section of the side for which deltas are to be calculated.
 * @param edge     Edge of the side for which deltas are to be calculated.
 *
 * @return  Light delta for the specified wall section edge.
 */
static float wallSectionEdgeLightLevelDelta(Line::Side &side, int section, int edge)
{
    // Are light level deltas disabled?
    if(rendLightWallAngle <= 0) return 0;

    // ...always if the surface's material was chosen as a HOM fix (lighting
    // must be consistent with that applied to the relative back sector plane).
    if(side.hasSections() && side.back().hasSections() &&
       side.surface(section).hasFixMaterial())
        return 0;

    float delta = calcLightLevelDelta(side.surface(section).normal());

    // Is delta smoothing disabled?
    if(!rendLightWallAngleSmooth) return delta;
    // ...always for polyobj lines (no owner rings).
    if(side.line().isFromPolyobj()) return delta;

    /**
     * Smoothing is enabled, so find the neighbor side for the specified edge
     * which we will use to calculate the averaged lightlevel delta for each.
     *
     * @todo Do not assume the neighbor is the middle section of @var otherSide.
     */
    binangle_t angleDiff;
    if(Line::Side *otherSide = findSideBlendNeighbor(side, edge, &angleDiff))
    {
        Surface &sufA = side.surface(section);
        Surface &sufB = otherSide->surface(section);

        if(shouldSmoothLightLevels(sufA, sufB, angleDiff))
        {
            // Average normals.
            Vector3f avgNormal = Vector3f(sufA.normal() + sufB.normal()) / 2;
            return calcLightLevelDelta(avgNormal);
        }
    }

    return delta;
}

/**
 * Calculate the light level deltas for this wall section.
 * @todo Cache these values somewhere. -ds
 */
static void wallSectionLightLevelDeltas(SectionEdge const &leftEdge, SectionEdge const &rightEdge,
    float &leftDelta, float &rightDelta)
{
    leftDelta  = wallSectionEdgeLightLevelDelta(leftEdge.lineSide(), leftEdge.section(), HEdge::From);
    rightDelta = wallSectionEdgeLightLevelDelta(rightEdge.lineSide(), rightEdge.section(), HEdge::To);

    if(!de::fequal(leftDelta, rightDelta))
    {
        // Linearly interpolate to find the light level delta values for the
        // vertical edges of this wall section.
        coord_t const lineLength = leftEdge.lineSide().line().length();
        coord_t const sectionOffset = leftEdge.lineOffset();
        coord_t const sectionWidth = de::abs(Vector2d(rightEdge.origin() - leftEdge.origin()).length());

        float deltaDiff = rightDelta - leftDelta;
        rightDelta = leftDelta + ((sectionOffset + sectionWidth) / lineLength) * deltaDiff;
        leftDelta += (sectionOffset / lineLength) * deltaDiff;
    }
}

/**
 * Project lights for the given map @a surface.
 *
 * @pre currentBspLeaf is set to the BSP leaf which "contains" the surface.
 *
 * @param surface          Map surface to project lights onto.
 * @param glowStrength     Surface glow strength (glow scale factor(s) are assumed to
 *                         have already been applied).
 * @param topLeft          Top left coordinates for the conceptual quad used for projection.
 * @param bottomRight      Bottom right coordinates for the conceptual quad used for projection.
 * @param sortProjections  @c true= instruct the projection algorithm to sort the resultant
 *                         projections by descending luminosity. Default = @c false.
 *
 * @return  Identifier for the resultant light projection list; otherwise @c 0.
 *
 * @see LO_ProjectToSurface()
 */
static uint projectSurfaceLights(Surface &surface, float glowStrength,
    Vector3d const &topLeft, Vector3d const &bottomRight, bool sortProjections = false)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    // Is light projection disabled?
    if(glowStrength >= 1) return 0;
    if(!useDynLights && !useWallGlow) return 0;

    return LO_ProjectToSurface(0 | (sortProjections? PLF_SORT_LUMINOSITY_DESC : 0), leaf,
                               1, topLeft, bottomRight,
                               surface.tangent(), surface.bitangent(), surface.normal());
}

/**
 * Project shadows for the given map @a surface.
 *
 * @pre currentBspLeaf is set to the BSP leaf which "contains" the surface.
 *
 * @param surface       Map surface to project shadows onto.
 * @param glowStrength  Surface glow strength (glow scale factor(s) are assumed to
 *                      have already been applied).
 * @param topLeft       Top left coordinates for the conceptual quad used for projection.
 * @param bottomRight   Bottom right coordinates for the conceptual quad used for projection.
 *
 * @return  Identifier for the resultant shadow projection list; otherwise @c 0.
 *
 * @see R_ProjectShadowsToSurface()
 */
static uint projectSurfaceShadows(Surface &surface, float glowStrength,
    Vector3d const &topLeft, Vector3d const &bottomRight)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    // Is shadow projection disabled?
    if(glowStrength >= 1) return 0;
    if(!Rend_MobjShadowsEnabled()) return 0;

    // Glow inversely diminishes shadow strength.
    float const shadowStrength = 1 - glowStrength;

    return R_ProjectShadowsToSurface(leaf, shadowStrength, topLeft, bottomRight,
                                     surface.tangent(), surface.bitangent(), surface.normal());
}

/**
 * @defgroup writeWallSectionFlags Write Wall Section Flags
 * Flags for writeWallSection()
 * @ingroup flags
 */
///@{
#define WSF_ADD_DYNLIGHTS       0x01 ///< Write geometry for dynamic lights.
#define WSF_ADD_DYNSHADOWS      0x02 ///< Write geometry for dynamic (mobj) shadows.
#define WSF_ADD_RADIO           0x04 ///< Write geometry for faked radiosity.
#define WSF_FORCE_OPAQUE        0x08 ///< Force the geometry to be opaque.

#define ALL_WRITE_WALL_SECTION_FLAGS        WSF_ADD_DYNLIGHTS | WSF_ADD_DYNSHADOWS | WSF_ADD_RADIO | WSF_FORCE_OPAQUE

#define DEFAULT_WRITE_WALL_SECTION_FLAGS    ALL_WRITE_WALL_SECTION_FLAGS
///@}

/**
 * @param flags  @ref writeWallSectionFlags
 */
static bool writeWallSection(SectionEdge const &leftEdge, SectionEdge const &rightEdge,
    MapElement &mapElement, biassurface_t &biasSurface,
    int flags = DEFAULT_WRITE_WALL_SECTION_FLAGS)
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));
    DENG_ASSERT(rightEdge.top().distance() > leftEdge.bottom().distance());

    Line::Side &side  = leftEdge.lineSide();
    Line &line        = side.line();
    int const section = leftEdge.section();
    Surface &surface  = leftEdge.surface();

    bool const isTwoSidedMiddle = (section == Line::Side::Middle &&
                                   side.hasSections() && side.back().hasSections());

    float opacity = surface.opacity();

    // Apply a fade out when the viewer is near to this geometry?
    bool didNearFade = false;
    if(isTwoSidedMiddle &&
       ((viewPlayer->shared.flags & (DDPF_NOCLIP|DDPF_CAMERA)) ||
            !(line.isFlagged(DDLF_BLOCKING))) &&
       (vOrigin[VY] > leftEdge.bottom().distance() &&
            vOrigin[VY] < rightEdge.top().distance()))
    {
        mobj_t const *mo = viewPlayer->shared.mo;

        coord_t linePoint[2]     = { line.fromOrigin().x, line.fromOrigin().y };
        coord_t lineDirection[2] = {  line.direction().x,  line.direction().y };
        vec2d_t result;
        double pos = V2d_ProjectOnLine(result, mo->origin, linePoint, lineDirection);

        if(pos > 0 && pos < 1)
        {
            /**
             * Fade out the closer the viewPlayer gets and clamp.
             *
             * @note When the viewer is close enough we should NOT try to
             * occlude with this section in the angle clipper, otherwise HOM
             * would occur when directly on top of the wall (e.g., passing
             * through an opaque waterfall).
             */
            coord_t const minDistance = mo->radius * .8f;

            Vector2d delta = Vector2d(result) - Vector2d(mo->origin);
            coord_t distance = delta.length();

            if(de::abs(distance) < minDistance)
            {
                if(distance > 0)
                {
                    opacity = (opacity / minDistance) * distance;
                    opacity = de::clamp(0.f, opacity, 1.f);
                }
                didNearFade = true;
            }
        }
    }

    if(opacity <= 0) return false;

    // Determine which Material to use.
    Material *material = chooseSurfaceMaterialForTexturingMode(surface);

    // Surfaces without a drawable material are never rendered.
    if(!material || !material->isDrawable())
        return false;

    Vector2f materialScale((surface.flags() & DDSUF_MATERIAL_FLIPH)? -1 : 1,
                           (surface.flags() & DDSUF_MATERIAL_FLIPV)? -1 : 1);

    Vector3d texTL(leftEdge.top().origin());
    Vector3d texBR(rightEdge.bottom().origin());

    // Calculate the light level deltas for this wall section.
    float leftLightLevelDelta, rightLightLevelDelta;
    wallSectionLightLevelDeltas(leftEdge, rightEdge, leftLightLevelDelta, rightLightLevelDelta);

    rendworldpoly_params_t parm; zap(parm);

    parm.flags               = RPF_DEFAULT;
    parm.forceOpaque         = (flags & WSF_FORCE_OPAQUE)? true : false;
    parm.alpha               = (flags & WSF_FORCE_OPAQUE)? 1 : opacity;
    parm.mapElement          = &mapElement;
    parm.elmIdx              = section;
    parm.bsuf                = &biasSurface;
    parm.normal              = &surface.normal();
    parm.texTL               = &texTL;
    parm.texBR               = &texBR;
    parm.surfaceLightLevelDL = leftLightLevelDelta;
    parm.surfaceLightLevelDR = rightLightLevelDelta;
    parm.blendMode           = BM_NORMAL;
    parm.materialOrigin      = &leftEdge.materialOrigin();
    parm.materialScale       = &materialScale;

    parm.isWall = true;
    parm.wall.sectionWidth   = de::abs(Vector2d(rightEdge.origin() - leftEdge.origin()).length());
    parm.wall.leftEdge       = &leftEdge;
    parm.wall.rightEdge      = &rightEdge;

    MaterialSnapshot const &ms = material->prepare(mapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT));

    // Fill in the remaining params data.
    if(material->isSkyMasked() && !devRendSkyMode)
    {
        parm.flags |= RPF_SKYMASK;
    }
    else
    {
        if(isTwoSidedMiddle)
        {
            parm.blendMode = surface.blendMode();
            if(parm.blendMode == BM_NORMAL && noSpriteTrans)
                parm.blendMode = BM_ZEROALPHA; // "no translucency" mode
        }

        if(glowFactor > .0001f)
        {
            if(material == surface.materialPtr())
            {
                parm.glowing = ms.glowStrength();
            }
            else
            {
                Material *actualMaterial = surface.hasMaterial()? surface.materialPtr()
                                                                : &App_Materials().find(de::Uri("System", Path("missing"))).material();

                MaterialSnapshot const &ms = actualMaterial->prepare(Rend_MapSurfaceMaterialSpec());
                parm.glowing = ms.glowStrength();
            }

            parm.glowing *= glowFactor; // Global scale factor.
        }

        side.chooseSurfaceTintColors(section, &parm.surfaceColor, &parm.wall.surfaceColor2);
    }

    // Project dynamic Lights?
    if((flags & WSF_ADD_DYNLIGHTS) && !(parm.flags & RPF_SKYMASK))
    {
        parm.lightListIdx = projectSurfaceLights(surface, parm.glowing, texTL, texBR,
                                                 isTwoSidedMiddle);
    }

    // Project dynamic shadows?
    if((flags & WSF_ADD_DYNSHADOWS) && !(parm.flags & RPF_SKYMASK))
    {
        parm.shadowListIdx = projectSurfaceShadows(surface, parm.glowing, texTL, texBR);
    }

    /*
     * Geometry write/drawing begins.
     */

    if(isTwoSidedMiddle && side.sectorPtr() != currentBspLeaf->sectorPtr())
    {
        // Temporarily modify the draw state.
        currentSectorLightColor = Rend_SectorLightColor(side.sector());
        currentSectorLightLevel = side.sector().lightLevel();
    }

    rvertex_t *rvertices;
    // Allocate enough vertices for the divisions too.
    if(leftEdge.divisionCount() || rightEdge.divisionCount())
    {
        // Use two fans.
        rvertices = R_AllocRendVertices(3 + leftEdge.divisionCount() +
                                        3 + rightEdge.divisionCount());
    }
    else
    {
        // Use a quad.
        rvertices = R_AllocRendVertices(4);
    }

    // Vertex coords.
    // Bottom Left.
    V3f_Set(rvertices[0].pos, leftEdge.bottom().origin().x,
                              leftEdge.bottom().origin().y,
                              leftEdge.bottom().origin().z);
    // Top Left.
    V3f_Set(rvertices[1].pos, leftEdge.top().origin().x,
                              leftEdge.top().origin().y,
                              leftEdge.top().origin().z);
    // Bottom Right.
    V3f_Set(rvertices[2].pos, rightEdge.bottom().origin().x,
                              rightEdge.bottom().origin().y,
                              rightEdge.bottom().origin().z);
    // Top Right.
    V3f_Set(rvertices[3].pos, rightEdge.top().origin().x,
                              rightEdge.top().origin().y,
                              rightEdge.top().origin().z);

    // Draw this section.
    bool opaque = renderWorldPoly(rvertices, 4, parm, ms);
    if(opaque)
    {
        // Render FakeRadio for this section?
        if((flags & WSF_ADD_RADIO) && !(parm.flags & RPF_SKYMASK) &&
           !(parm.glowing > 0) && currentSectorLightLevel > 0 &&
           !line.isFromPolyobj())
        {
            Rend_RadioUpdateForLineSide(side);

            // Determine the shadow properties.
            /// @todo Make cvars out of constants.
            float shadowSize = 2 * (8 + 16 - currentSectorLightLevel * 16);
            float shadowDark = Rend_RadioCalcShadowDarkness(currentSectorLightLevel);

            Sector *frontSec = 0, *backSec = 0;
            {
                HEdge *hedge = mapElement.castTo<HEdge>();
                frontSec = hedge->wallSectionSector();
                if(hedge->hasTwin() && !isTwoSidedMiddle)
                {
                    if(hedge->twin().hasLineSide() && hedge->twin().lineSide().hasSections())
                    {
                        backSec = hedge->twin().bspLeaf().sectorPtr();
                    }
                }
            }

            Rend_RadioWallSection(leftEdge, rightEdge, shadowDark, shadowSize,
                                  frontSec, backSec);
        }
    }

    if(isTwoSidedMiddle && side.sectorPtr() != currentBspLeaf->sectorPtr())
    {
        // Undo temporary draw state changes.
        currentSectorLightColor = Rend_SectorLightColor(currentBspLeaf->sector());
        currentSectorLightLevel = currentBspLeaf->sector().lightLevel();
    }

    R_FreeRendVertices(rvertices);

    return opaque && !didNearFade;
}

static void writeLeafPlane(Plane &plane)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    Surface const &surface = plane.surface();
    Vector3f eyeToSurface(vOrigin[VX] - leaf->center().x,
                          vOrigin[VZ] - leaf->center().y,
                          vOrigin[VY] - plane.visHeight());

    // Skip planes facing away from the viewer.
    if(eyeToSurface.dot(surface.normal()) < 0)
        return;

    Material *material = chooseSurfaceMaterialForTexturingMode(surface);
    // We must have a drawable material.
    if(!material || !material->isDrawable()) return;

    // Skip planes with a sky-masked material?
    if(!devRendSkyMode)
    {
        if(surface.hasSkyMaskedMaterial() && plane.type() != Plane::Middle)
            return; // Not handled here (drawn with the mask geometry).
    }

    Vector2f materialOrigin = leaf->worldGridOffset() // Align to the worldwide grid.
                            + surface.visMaterialOrigin();

    // Add the Y offset to orient the Y flipped material.
    /// @todo fixme: What is this meant to do? -ds
    if(plane.type() == Plane::Ceiling)
        materialOrigin.y -= leaf->aaBox().maxY - leaf->aaBox().minY;
    materialOrigin.y = -materialOrigin.y;

    Vector2f materialScale((surface.flags() & DDSUF_MATERIAL_FLIPH)? -1 : 1,
                           (surface.flags() & DDSUF_MATERIAL_FLIPV)? -1 : 1);

    // Set the texture origin, Y is flipped for the ceiling.
    Vector3d texTL(leaf->aaBox().minX,
                   leaf->aaBox().arvec2[plane.type() == Plane::Floor? 1 : 0][VY],
                   plane.visHeight());
    Vector3d texBR(leaf->aaBox().maxX,
                   leaf->aaBox().arvec2[plane.type() == Plane::Floor? 0 : 1][VY],
                   plane.visHeight());

    rendworldpoly_params_t parm; zap(parm);

    parm.flags               = RPF_DEFAULT;
    parm.isWall              = false;
    parm.mapElement          = leaf;
    parm.elmIdx              = plane.inSectorIndex();
    parm.bsuf                = &leaf->biasSurfaceForGeometryGroup(plane.inSectorIndex());
    parm.normal              = &surface.normal();
    parm.texTL               = &texTL;
    parm.texBR               = &texBR;
    parm.surfaceLightLevelDL = parm.surfaceLightLevelDR = 0;
    parm.surfaceColor        = &surface.tintColor();
    parm.materialOrigin      = &materialOrigin;
    parm.materialScale       = &materialScale;

    if(material->isSkyMasked())
    {
        // In devRendSkyMode mode we render all polys destined for the
        // skymask as regular world polys (with a few obvious properties).
        if(devRendSkyMode)
        {
            parm.blendMode = BM_NORMAL;
            parm.forceOpaque = true;
        }
        else
        {   // We'll mask this.
            parm.flags |= RPF_SKYMASK;
        }
    }
    else if(plane.type() != Plane::Middle)
    {
        parm.blendMode = BM_NORMAL;
        parm.forceOpaque = true;
    }
    else
    {
        parm.blendMode = surface.blendMode();
        if(parm.blendMode == BM_NORMAL && noSpriteTrans)
            parm.blendMode = BM_ZEROALPHA; // "no translucency" mode

        parm.alpha = surface.opacity();
    }

    uint numVertices;
    rvertex_t *rvertices;
    buildLeafPlaneGeometry(*leaf, (plane.type() == Plane::Ceiling), plane.visHeight(),
                           &rvertices, &numVertices);

    MaterialSnapshot const &ms = material->prepare(mapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT));

    if(!(parm.flags & RPF_SKYMASK))
    {
        if(glowFactor > .0001f)
        {
            if(material == surface.materialPtr())
            {
                parm.glowing = ms.glowStrength();
            }
            else
            {
                Material *actualMaterial = surface.hasMaterial()? surface.materialPtr()
                                                                : &App_Materials().find(de::Uri("System", Path("missing"))).material();

                MaterialSnapshot const &ms = actualMaterial->prepare(Rend_MapSurfaceMaterialSpec());
                parm.glowing = ms.glowStrength();
            }

            parm.glowing *= glowFactor; // Global scale factor.
        }

        // Dynamic lights?
        if(!devRendSkyMode && parm.glowing < 1 && !(!useDynLights && !useWallGlow))
        {
            /// @ref projectLightFlags
            int plFlags = (PLF_NO_PLANE | (plane.type() == Plane::Floor? PLF_TEX_FLOOR : PLF_TEX_CEILING));

            parm.lightListIdx =
                LO_ProjectToSurface(plFlags, leaf, 1, *parm.texTL, *parm.texBR,
                                    surface.tangent(), surface.bitangent(), surface.normal());
        }

        // Mobj shadows?
        if(plane.type() == Plane::Floor && parm.glowing < 1 && Rend_MobjShadowsEnabled())
        {
            // Glowing planes inversely diminish shadow strength.
            float blendFactor = 1 - parm.glowing;

            parm.shadowListIdx =
                R_ProjectShadowsToSurface(leaf, blendFactor, *parm.texTL, *parm.texBR,
                                          surface.tangent(), surface.bitangent(), surface.normal());
        }
    }

    renderWorldPoly(rvertices, numVertices, parm, ms);

    R_FreeRendVertices(rvertices);
}

/**
 * @defgroup skyCapFlags  Sky Cap Flags
 * @ingroup flags
 */
///@{
#define SKYCAP_LOWER                0x1
#define SKYCAP_UPPER                0x2
///@}

static coord_t skyCapZ(BspLeaf *bspLeaf, int skyCap)
{
    DENG_ASSERT(bspLeaf);
    Plane::Type const plane = (skyCap & SKYCAP_UPPER)? Plane::Ceiling : Plane::Floor;
    if(!bspLeaf->hasSector() || !P_IsInVoid(viewPlayer))
        return theMap->skyFix(plane == Plane::Ceiling);
    return bspLeaf->sector().plane(plane).visHeight();
}

static coord_t skyFixFloorZ(Plane const *frontFloor, Plane const *backFloor)
{
    DENG_UNUSED(backFloor);
    if(devRendSkyMode || P_IsInVoid(viewPlayer))
        return frontFloor->visHeight();
    return theMap->skyFixFloor();
}

static coord_t skyFixCeilZ(Plane const *frontCeil, Plane const *backCeil)
{
    DENG_UNUSED(backCeil);
    if(devRendSkyMode || P_IsInVoid(viewPlayer))
        return frontCeil->visHeight();
    return theMap->skyFixCeiling();
}

/**
 * @param hedge  HEdge from which to determine sky fix coordinates.
 * @param skyCap  Either SKYCAP_LOWER or SKYCAP_UPPER (SKYCAP_UPPER has precendence).
 * @param bottom  Z map space coordinate for the bottom of the skyfix written here.
 * @param top  Z map space coordinate for the top of the skyfix written here.
 */
static void skyFixZCoords(HEdge *hedge, int skyCap, coord_t *bottom, coord_t *top)
{
    Sector const *frontSec = hedge->bspLeafSectorPtr();
    Sector const *backSec  = hedge->hasTwin()? hedge->twin().bspLeafSectorPtr() : 0;
    Plane const *ffloor = &frontSec->floor();
    Plane const *fceil  = &frontSec->ceiling();
    Plane const *bceil  = backSec? &backSec->ceiling() : 0;
    Plane const *bfloor = backSec? &backSec->floor()   : 0;

    if(!bottom && !top) return;
    if(bottom) *bottom = 0;
    if(top)    *top    = 0;

    if(skyCap & SKYCAP_UPPER)
    {
        if(top)    *top    = skyFixCeilZ(fceil, bceil);
        if(bottom) *bottom = de::max((backSec && bceil->surface().hasSkyMaskedMaterial() )? bceil->visHeight()  : fceil->visHeight(),  ffloor->visHeight());
    }
    else
    {
        if(top)    *top    = de::min((backSec && bfloor->surface().hasSkyMaskedMaterial())? bfloor->visHeight() : ffloor->visHeight(), fceil->visHeight());
        if(bottom) *bottom = skyFixFloorZ(ffloor, bfloor);
    }
}

/**
 * Determine which sky fixes are necessary for the specified @a hedge.
 * @param hedge  HEdge to be evaluated.
 * @param skyCap  The fixes we are interested in. @ref skyCapFlags.
 * @return
 */
static int chooseHEdgeSkyFixes(HEdge *hedge, int skyCap)
{
    if(!hedge || !hedge->hasBspLeaf())
        return 0; // Wha?

    // "minisegs" have no lines
    if(!hedge->hasLineSide()) return 0;
    if(!hedge->bspLeaf().hasSector()) return 0; // $degenleaf

    int fixes = 0;
    Sector const *frontSec = hedge->bspLeafSectorPtr();
    Sector const *backSec  = hedge->hasTwin()? hedge->twin().bspLeafSectorPtr() : 0;

    if(!backSec || backSec != frontSec)
    {
        bool const hasSkyFloor   = frontSec->floorSurface().hasSkyMaskedMaterial();
        bool const hasSkyCeiling = frontSec->ceilingSurface().hasSkyMaskedMaterial();

        if(hasSkyFloor || hasSkyCeiling)
        {
            bool const hasClosedBack = R_SideBackClosed(hedge->lineSide());

            // Lower fix?
            if(hasSkyFloor && (skyCap & SKYCAP_LOWER))
            {
                Plane const *ffloor = &frontSec->floor();
                Plane const *bfloor = backSec? &backSec->floor() : NULL;
                coord_t const skyZ = skyFixFloorZ(ffloor, bfloor);

                if(hasClosedBack ||
                        (!bfloor->surface().hasSkyMaskedMaterial() ||
                         devRendSkyMode || P_IsInVoid(viewPlayer)))
                {
                    Plane const *floor = (bfloor && bfloor->surface().hasSkyMaskedMaterial() && ffloor->visHeight() < bfloor->visHeight()? bfloor : ffloor);
                    if(floor->visHeight() > skyZ)
                        fixes |= SKYCAP_LOWER;
                }
            }

            // Upper fix?
            if(hasSkyCeiling && (skyCap & SKYCAP_UPPER))
            {
                Plane const *fceil = &frontSec->ceiling();
                Plane const *bceil = backSec? &backSec->ceiling() : NULL;
                coord_t const skyZ = skyFixCeilZ(fceil, bceil);

                if(hasClosedBack ||
                        (!bceil->surface().hasSkyMaskedMaterial() ||
                         devRendSkyMode || P_IsInVoid(viewPlayer)))
                {
                    Plane const *ceil = (bceil && bceil->surface().hasSkyMaskedMaterial() && fceil->visHeight() > bceil->visHeight()? bceil : fceil);
                    if(ceil->visHeight() < skyZ)
                        fixes |= SKYCAP_UPPER;
                }
            }
        }
    }
    return fixes;
}

static inline void buildStripEdge(Vector2d const &vXY,
    coord_t v1Z, coord_t v2Z, float texS,
    rvertex_t *v1, rvertex_t *v2, rtexcoord_t *t1, rtexcoord_t *t2)
{
    if(v1)
    {
        V2f_Set(v1->pos, vXY.x, vXY.y);
        v1->pos[VZ] = v1Z;
    }
    if(v2)
    {
        V2f_Set(v2->pos, vXY.x, vXY.y);
        v2->pos[VZ] = v2Z;
    }
    if(t1)
    {
        t1->st[0] = texS;
        t1->st[1] = v2Z - v1Z;
    }
    if(t2)
    {
        t2->st[0] = texS;
        t2->st[1] = 0;
    }
}

/**
 * Vertex layout:
 *   1--3    2--0
 *   |  | or |  | if antiClockwise
 *   0--2    3--1
 */
static void buildSkyFixStrip(BspLeaf *leaf, HEdge *startNode, HEdge *endNode,
    bool _antiClockwise, int skyCap, rvertex_t **verts, uint *vertsSize, rtexcoord_t **coords)
{
    int const antiClockwise = _antiClockwise? 1:0;
    *vertsSize = 0;
    *verts = 0;

    if(!startNode || !endNode || !skyCap) return;

    // Count verts.
    if(startNode == endNode)
    {
        // Special case: the whole edge loop.
        *vertsSize += 2 * (leaf->hedgeCount() + 1);
    }
    else
    {
        HEdge *afterEndNode = antiClockwise? &endNode->prev() : &endNode->next();
        HEdge *node = startNode;
        do
        {
            *vertsSize += 2;
        } while((node = antiClockwise? &node->prev() : &node->next()) != afterEndNode);
    }

    // Build geometry.
    *verts = R_AllocRendVertices(*vertsSize);
    if(coords)
    {
        *coords = R_AllocRendTexCoords(*vertsSize);
    }

    HEdge *node = startNode;
    float texS = float( node->hasLineSide()? node->lineOffset() : 0 );
    uint n = 0;
    do
    {
        HEdge *hedge = (antiClockwise? &node->prev() : node);
        coord_t zBottom, zTop;

        skyFixZCoords(hedge, skyCap, &zBottom, &zTop);
        DENG_ASSERT(zBottom < zTop);

        if(n == 0)
        {
            // Add the first edge.
            rvertex_t *v1 = &(*verts)[n + antiClockwise];
            rvertex_t *v2 = &(*verts)[n + (antiClockwise^1)];
            rtexcoord_t *t1 = coords? &(*coords)[n + antiClockwise] : NULL;
            rtexcoord_t *t2 = coords? &(*coords)[n + (antiClockwise^1)] : NULL;

            buildStripEdge(node->fromOrigin(), zBottom, zTop, texS, v1, v2, t1, t2);

            if(coords)
            {
                texS += antiClockwise? -node->prev().length() : hedge->length();
            }

            n += 2;
        }

        // Add the next edge.
        {
            rvertex_t *v1 = &(*verts)[n + antiClockwise];
            rvertex_t *v2 = &(*verts)[n + (antiClockwise^1)];
            rtexcoord_t *t1 = coords? &(*coords)[n + antiClockwise] : NULL;
            rtexcoord_t *t2 = coords? &(*coords)[n + (antiClockwise^1)] : NULL;

            buildStripEdge((antiClockwise? &node->prev() : &node->next())->fromOrigin(),
                           zBottom, zTop, texS, v1, v2, t1, t2);

            if(coords)
            {
                texS += antiClockwise? -hedge->length() : hedge->next().length();
            }

            n += 2;
        }
    } while((node = antiClockwise? &node->prev() : &node->next()) != endNode);
}

static void Rend_WriteBspLeafSkyFixStripGeometry(BspLeaf *leaf, HEdge *startNode,
    HEdge *endNode, bool antiClockwise, int skyFix, Material *material)
{
    int const rendPolyFlags = RPF_DEFAULT | (!devRendSkyMode? RPF_SKYMASK : 0);
    rtexcoord_t *coords = 0;
    rvertex_t *verts;
    uint vertsSize;

    buildSkyFixStrip(leaf, startNode, endNode, antiClockwise, skyFix,
                     &verts, &vertsSize, devRendSkyMode? &coords : NULL);

    if(!devRendSkyMode)
    {
        RL_AddPoly(PT_TRIANGLE_STRIP, rendPolyFlags, vertsSize, verts, NULL);
    }
    else
    {
        if(renderTextures != 2)
        {
            // Map RTU configuration from the sky surface material.
            MaterialSnapshot const &ms = material->prepare(mapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT));

            RL_LoadDefaultRtus();
            RL_MapRtu(RTU_PRIMARY, &ms.unit(RTU_PRIMARY));
        }
        RL_AddPolyWithCoords(PT_TRIANGLE_STRIP, rendPolyFlags, vertsSize, verts, NULL, coords, NULL);
    }

    R_FreeRendVertices(verts);
    R_FreeRendTexCoords(coords);
}

/**
 * Prepare the trifan rvertex_t buffer specified according to the edges of this
 * BSP leaf. If a fan base HEdge has been chosen it will be used as the center of
 * the trifan, else the mid point of this leaf will be used instead.
 *
 * @param leaf  BspLeaf instance.
 * @param antiClockwise  @c true= wind vertices in anticlockwise order (else clockwise).
 * @param height  Z map space height coordinate to be set for each vertex.
 * @param verts  Built vertices are written here.
 * @param vertsSize  Number of built vertices is written here. Can be @c NULL.
 *
 * @return  Number of built vertices (same as written to @a vertsSize).
 */
static uint buildLeafPlaneGeometry(BspLeaf const &leaf, bool antiClockwise,
    coord_t height, rvertex_t **verts, uint *vertsSize)
{
    DENG_ASSERT(verts != 0);

    HEdge *fanBase = leaf.fanBase();
    HEdge *baseNode = fanBase? fanBase : leaf.firstHEdge();

    uint totalVerts = leaf.hedgeCount() + (!fanBase? 2 : 0);
    *verts = R_AllocRendVertices(totalVerts);

    uint n = 0;
    if(!fanBase)
    {
        V3f_Set((*verts)[n].pos, leaf.center().x, leaf.center().y, height);
        n++;
    }

    // Add the vertices for each hedge.
    HEdge *node = baseNode;
    do
    {
        V3f_Set((*verts)[n].pos, node->fromOrigin().x, node->fromOrigin().y, height);
        n++;
    } while((node = antiClockwise? &node->prev() : &node->next()) != baseNode);

    // The last vertex is always equal to the first.
    if(!fanBase)
    {
        V3f_Set((*verts)[n].pos, leaf.firstHEdge()->fromOrigin().x,
                                 leaf.firstHEdge()->fromOrigin().y,
                                 height);
    }

    if(vertsSize) *vertsSize = totalVerts;
    return totalVerts;
}

/// @param skyFix  @ref skyCapFlags.
static void writeLeafSkyMaskStrips(int skyFix)
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));

    if(!(skyFix & (SKYCAP_LOWER|SKYCAP_UPPER)))
        return;

    bool const antiClockwise = false;
    bool const splitForSkyMaterials = (devRendSkyMode && renderTextures != 2);

    // We may need to break the half-edge loop into multiple strips.
    HEdge *startNode        = 0;
    coord_t startZBottom    = 0;
    coord_t startZTop       = 0;
    Material *startMaterial = 0;

    HEdge *base = bspLeaf->firstHEdge();
    HEdge *node = base;
    forever
    {
        HEdge *hedge = (antiClockwise? &node->prev() : node);
        bool endStrip = false, beginNewStrip = false;

        // Is a fix or two necessary for this edge?
        if(chooseHEdgeSkyFixes(hedge, skyFix))
        {
            coord_t zBottom, zTop;
            skyFixZCoords(hedge, skyFix, &zBottom, &zTop);

            Material *skyMaterial = 0;
            if(splitForSkyMaterials)
            {
                int relPlane = skyFix == SKYCAP_UPPER? Plane::Ceiling : Plane::Floor;
                skyMaterial = hedge->bspLeafSector().planeSurface(relPlane).materialPtr();
            }

            if(zBottom >= zTop)
            {
                // End the current strip.
                endStrip = true;
            }
            else if(startNode && (!de::fequal(zBottom, startZBottom) || !de::fequal(zTop, startZTop) ||
                                  (splitForSkyMaterials && skyMaterial != startMaterial)))
            {
                // End the current strip and start another.
                endStrip = true;
                beginNewStrip = true;
            }
            else if(!startNode)
            {
                // A new strip begins.
                startNode = node;
                startZBottom = zBottom;
                startZTop = zTop;
                startMaterial = skyMaterial;
            }
        }
        else
        {
            // End the current strip.
            endStrip = true;
        }

        if(endStrip && startNode)
        {
            // We have complete strip; build and write it.
            Rend_WriteBspLeafSkyFixStripGeometry(bspLeaf, startNode, node, antiClockwise,
                                                 skyFix, startMaterial);

            // End the current strip.
            startNode = 0;
        }

        // Start a new strip from this node?
        if(beginNewStrip) continue;

        // On to the next node.
        node = antiClockwise? &node->prev() : &node->next();

        // Are we done?
        if(node == base) break;
    }

    // Have we an unwritten strip? - build it.
    if(startNode)
    {
        Rend_WriteBspLeafSkyFixStripGeometry(bspLeaf, startNode, base, antiClockwise,
                                             skyFix, startMaterial);
    }
}

/// @param skyCap  @ref skyCapFlags.
static void writeLeafSkyMaskCap(int skyCap)
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));

    // Caps are unnecessary in sky debug mode (will be drawn as regular planes).
    if(devRendSkyMode) return;
    if(!skyCap) return;

    rvertex_t *verts;
    uint numVerts;
    buildLeafPlaneGeometry(*bspLeaf, (skyCap & SKYCAP_UPPER) != 0, skyCapZ(bspLeaf, skyCap),
                           &verts, &numVerts);

    RL_AddPoly(PT_FAN, RPF_DEFAULT | RPF_SKYMASK, numVerts, verts, NULL);
    R_FreeRendVertices(verts);
}

/// @param skyCap  @ref skyCapFlags
static void writeLeafSkyMask(int skyCap = SKYCAP_LOWER|SKYCAP_UPPER)
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));

    // Any work to do?
    if(!bspLeaf->sector().hasSkyMaskedPlane())
        return;

    // Sky caps are only necessary in sectors with sky-masked planes.
    if((skyCap & SKYCAP_LOWER) && !bspLeaf->sector().floorSurface().hasSkyMaskedMaterial())
        skyCap &= ~SKYCAP_LOWER;
    if((skyCap & SKYCAP_UPPER) && !bspLeaf->sector().ceilingSurface().hasSkyMaskedMaterial())
        skyCap &= ~SKYCAP_UPPER;

    if(!skyCap) return;

    if(!devRendSkyMode || renderTextures == 2)
    {
        // All geometry uses the same RTU write state.
        RL_LoadDefaultRtus();
        if(renderTextures == 2)
        {
            // Map RTU configuration from the special "gray" material.
            MaterialSnapshot const &ms =
                App_Materials().find(de::Uri("System", Path("gray")))
                    .material().prepare(mapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT));
            RL_MapRtu(RTU_PRIMARY, &ms.unit(RTU_PRIMARY));
        }
    }

    // Lower?
    if(skyCap & SKYCAP_LOWER)
    {
        writeLeafSkyMaskStrips(SKYCAP_LOWER);
        writeLeafSkyMaskCap(SKYCAP_LOWER);
    }

    // Upper?
    if(skyCap & SKYCAP_UPPER)
    {
        writeLeafSkyMaskStrips(SKYCAP_UPPER);
        writeLeafSkyMaskCap(SKYCAP_UPPER);
    }
}

/**
 * Prepare edges and write the specified wall @a section to the render lists.
 *
 * @pre currentBspLeaf is set to the BSP leaf which "contains" the half-edge.
 * @pre @a hedge has an attributed map line side with sections.
 * @pre @a hedge is front facing.
 *
 * @param hedge    HEdge to write wall a section for.
 * @param section  Identifier of the wall section to write.
 *
 * Return values:
 * @param opaque   @c true= an opaque polygon was written to the lists. Can be @c 0.
 *
 * @return  @c true iff one or more polygons are actually written to the lists.
 */
static bool prepareEdgesAndWriteWallSection(HEdge &hedge, int section, bool *opaque = 0)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    DENG_ASSERT(hedge.hasLineSide());
    DENG_ASSERT(hedge.lineSide().hasSections());
    DENG_ASSERT(hedge.isFlagged(HEdge::FacingFront));

    // Done here because of the logic of doom.exe wrt the automap.
    reportWallSectionDrawn(hedge.line());

    SectionEdge leftEdge(hedge, section, HEdge::From);
    SectionEdge rightEdge(hedge, section, HEdge::To);

    leftEdge.prepare();
    rightEdge.prepare();

    if(leftEdge.isValid() && rightEdge.isValid() &&
       rightEdge.top().distance() > leftEdge.bottom().distance())
    {
        bool wroteOpaquePoly =
            writeWallSection(leftEdge, rightEdge,
                             hedge, hedge.biasSurfaceForGeometryGroup(section));

        if(opaque) *opaque = wroteOpaquePoly;
        return wroteOpaquePoly;
    }

    if(opaque) *opaque = false;
    return false;
}

static bool shouldAddSolidSegmentForTwosidedEdge(HEdge &hedge,
    bool wroteOpaqueMiddle, bool middleCoversOpening)
{
    if(hedge.line().isSelfReferencing())
       return false; /// @todo Why? -ds

    if(wroteOpaqueMiddle && middleCoversOpening)
        return true;

    // We'll have to determine whether we can...
    Sector *frontSec = hedge.wallSectionSector();
    Sector *backSec  = hedge.wallSectionSector(HEdge::Back);

    Line::Side const &front = hedge.lineSide();

    coord_t const ffloor = frontSec->floor().visHeight();
    coord_t const fceil  = frontSec->ceiling().visHeight();
    coord_t const bfloor = backSec->floor().visHeight();
    coord_t const bceil  = backSec->ceiling().visHeight();

    if(   (bceil <= ffloor &&
               (front.top().hasMaterial()    || front.middle().hasMaterial()))
       || (bfloor >= fceil &&
               (front.bottom().hasMaterial() || front.middle().hasMaterial())))
    {
        Surface const &ffloorSurface = frontSec->floor().surface();
        Surface const &fceilSurface  = frontSec->ceiling().surface();
        Surface const &bfloorSurface = backSec->floor().surface();
        Surface const &bceilSurface  = backSec->ceiling().surface();

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

/**
 * Determine which sections of the map line on @a side are potentially visible
 * according to the relative heights of the interfacing planes.
 *
 * @param side  Map line side to determine the potentially visible sections of.
 *
 * @return @ref sideSectionFlags denoting which sections are potentially visible.
 */
static int pvisibleWallSections(Line::Side &side)
{
    if(!side.hasSections()) return 0; // None.

    // One-sided? (therefore only the middle)
    if(!side.back().hasSector() /*$degenleaf*/ || !side.back().hasSections())
    {
        return Line::Side::MiddleFlag;
    }

    // Selfreferencing lines only ever get a middle.
    if(side.line().isSelfReferencing())
    {
        DENG_ASSERT(!side.middle().hasFixMaterial());
        return side.middle().hasMaterial()? Line::Side::MiddleFlag : 0;
    }

    Sector const &frontSec = side.sector();
    Sector const &backSec  = side.back().sector();
    Plane const &fceil  = frontSec.ceiling();
    Plane const &ffloor = frontSec.floor();
    Plane const &bceil  = backSec.ceiling();
    Plane const &bfloor = backSec.floor();

    // Default to all pvisible.
    int sections = Line::Side::AllSectionFlags;

    // Middle?
    if(!side.middle().hasMaterial() ||
       !side.middle().material().isDrawable() ||
        side.middle().opacity() <= 0)
    {
        sections &= ~Line::Side::MiddleFlag;
    }

    // Top?
    if((!devRendSkyMode && fceil.surface().hasSkyMaskedMaterial() && bceil.surface().hasSkyMaskedMaterial()) ||
       fceil.visHeight() <= bceil.visHeight())
    {
        sections &= ~Line::Side::TopFlag;
    }

    // Bottom?
    if((!devRendSkyMode && ffloor.surface().hasSkyMaskedMaterial() && bfloor.surface().hasSkyMaskedMaterial()) ||
       ffloor.visHeight() >= bfloor.visHeight())
    {
        sections &= ~Line::Side::BottomFlag;
    }

    return sections;
}

static void writeWallSections(HEdge &hedge)
{
    DENG_ASSERT(hedge.hasLineSide());
    DENG_ASSERT(hedge.lineSide().hasSections());
    DENG_ASSERT(hedge.isFlagged(HEdge::FacingFront));

    int pvisSections = pvisibleWallSections(hedge.lineSide()); /// @ref sideSectionFlags
    bool opaque = false;

    if(!hedge.hasTwin() || !hedge.twin().bspLeaf().hasSector() ||
       /* solid side of a "one-way window"? */
       !(hedge.twin().hasLineSide() && hedge.twin().lineSide().hasSections()))
    {
        // One-sided. Only a middle section.
        if(pvisSections & Line::Side::MiddleFlag)
            prepareEdgesAndWriteWallSection(hedge, Line::Side::Middle, &opaque);
    }
    else
    {
        // Two-sided.
        if(pvisSections & Line::Side::BottomFlag)
            prepareEdgesAndWriteWallSection(hedge, Line::Side::Bottom);

        if(pvisSections & Line::Side::TopFlag)
            prepareEdgesAndWriteWallSection(hedge, Line::Side::Top);

        bool wroteOpaqueMiddle = false;
        bool middleCoversOpening = false;

        if(pvisSections & Line::Side::MiddleFlag)
        {
            reportWallSectionDrawn(hedge.line());

            SectionEdge leftEdge(hedge, Line::Side::Middle, HEdge::From);
            SectionEdge rightEdge(hedge, Line::Side::Middle, HEdge::To);

            leftEdge.prepare();
            rightEdge.prepare();

            if(leftEdge.isValid() && rightEdge.isValid() &&
               rightEdge.top().distance() > leftEdge.bottom().distance())
            {
                wroteOpaqueMiddle =
                    writeWallSection(leftEdge, rightEdge,
                                     hedge, hedge.biasSurfaceForGeometryGroup(Line::Side::Middle),
                                     DEFAULT_WRITE_WALL_SECTION_FLAGS & ~WSF_FORCE_OPAQUE);

                if(wroteOpaqueMiddle)
                {
                    // Did we completely cover the open range?
                    Sector *frontSec = hedge.wallSectionSector();
                    Sector *backSec  = hedge.wallSectionSector(HEdge::Back);

                    coord_t const ffloor = frontSec->floor().visHeight();
                    coord_t const fceil  = frontSec->ceiling().visHeight();
                    coord_t const bfloor = backSec->floor().visHeight();
                    coord_t const bceil  = backSec->ceiling().visHeight();

                    coord_t xbottom, xtop;
                    if(hedge.line().isSelfReferencing())
                    {
                        xbottom = de::min(bfloor, ffloor);
                        xtop    = de::max(bceil,  fceil);
                    }
                    else
                    {
                        xbottom = de::max(bfloor, ffloor);
                        xtop    = de::min(bceil,  fceil);
                    }

                    Surface const &middle = hedge.lineSide().middle();
                    xbottom += middle.visMaterialOrigin().y;
                    xtop    += middle.visMaterialOrigin().y;

                    middleCoversOpening = (rightEdge.top().distance() >= xtop &&
                                           leftEdge.bottom().distance() <= xbottom);
                }
            }
        }

        opaque = shouldAddSolidSegmentForTwosidedEdge(hedge, wroteOpaqueMiddle, middleCoversOpening);
    }

    // We can occlude the angle range defined by the wall section
    // if the opening has been covered (when the viewer is not in the void).
    if(opaque && !P_IsInVoid(viewPlayer))
    {
        C_AddRangeFromViewRelPoints(hedge.fromOrigin(), hedge.toOrigin());
    }
}

static void writeLeafWallSections()
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    HEdge *base = leaf->firstHEdge();
    HEdge *hedge = base;
    do
    {
        // Ignore back facing walls.
        if(hedge->isFlagged(HEdge::FacingFront))
        if(hedge->hasLineSide() && hedge->lineSide().hasSections()) // "mini-hedges" have no lines and "windows" have no sections.
        {
            writeWallSections(*hedge);
        }
    } while((hedge = &hedge->next()) != base);
}

static void writeLeafPolyobjs()
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    Polyobj *po = leaf->firstPolyobj();
    if(!po) return;

    foreach(Line *line, po->lines())
    {
        HEdge *hedge = line->front().leftHEdge();

        // Ignore back facing walls.
        if(hedge->isFlagged(HEdge::FacingFront))
        {
            bool opaque = false;
            prepareEdgesAndWriteWallSection(*hedge, Line::Side::Middle, &opaque);

            // We can occlude the wall range if the opening is filled (when the viewer is not in the void).
            if(opaque && !P_IsInVoid(viewPlayer))
            {
                C_AddRangeFromViewRelPoints(hedge->fromOrigin(), hedge->toOrigin());
            }
        }
    }
}

static void writeLeafPlanes()
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    foreach(Plane *plane, leaf->sector().planes())
    {
        writeLeafPlane(*plane);
    }
}

static void markFrontFacingHEdges()
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));

    if(HEdge *base = bspLeaf->firstHEdge())
    {
        HEdge *hedge = base;
        do
        {
            // Occlusions can only happen where two sectors contact.
            if(hedge->hasLineSide())
            {
                // Which way is it facing?
                double dot = viewFacingDot(hedge->fromOrigin(), hedge->toOrigin());
                hedge->setFlags(HEdge::FacingFront, dot < 0? UnsetFlags : SetFlags);
            }
        } while((hedge = &hedge->next()) != base);
    }

    if(Polyobj *po = bspLeaf->firstPolyobj())
    {
        foreach(Line *line, po->lines())
        {
            HEdge *hedge = line->front().leftHEdge();

            // Which way is it facing?
            double dot = viewFacingDot(hedge->fromOrigin(), hedge->toOrigin());
            hedge->setFlags(HEdge::FacingFront, dot < 0? UnsetFlags : SetFlags);
        }
    }
}

static inline bool canOccludeSectorPairBoundary(Sector const &frontSec,
    Sector const &backSec, bool upward)
{
    Plane const &frontPlane = frontSec.plane(upward? Plane::Ceiling : Plane::Floor);
    Plane const &backPlane  =  backSec.plane(upward? Plane::Ceiling : Plane::Floor);

    // Do not create an occlusion between two sky-masked planes.
    // Only because the open range does not account for the sky plane height? -ds
    return !(frontPlane.surface().hasSkyMaskedMaterial() &&
              backPlane.surface().hasSkyMaskedMaterial());
}

static void occludeHEdge(HEdge const &hedge, bool frontFacing)
{
    DENG_ASSERT(hedge.hasBspLeaf() && hedge.bspLeaf().hasSector());

    if(frontFacing != hedge.isFlagged(HEdge::FacingFront)) return;

    // Edges without map line sections can never occlude.
    if(!hedge.hasLineSide() || !hedge.lineSide().hasSections()) return;

    // Occlusions should only happen where two sectors meet.
    if(!hedge.hasTwin() || !hedge.twin().bspLeafSectorPtr()) return;

    Sector const &frontSec = hedge.bspLeaf().sector();
    Sector const &backSec  = hedge.twin().bspLeaf().sector();

    // Choose start and end vertexes so that it's facing forward.
    Vertex const &from = hedge.vertex(frontFacing^1);
    Vertex const &to   = hedge.vertex(frontFacing  );

    coord_t openBottom, openTop;
    R_VisOpenRange(hedge.lineSide(), &frontSec, &backSec, &openBottom, &openTop);

    // Does the floor create an occlusion?
    if(((openBottom > frontSec.floor().visHeight() && vOrigin[VY] <= openBottom) ||
        (openBottom >  backSec.floor().visHeight() && vOrigin[VY] >= openBottom))
       && canOccludeSectorPairBoundary(frontSec, backSec, false))
    {
        C_AddViewRelOcclusion(from.origin(), to.origin(), openBottom, false);
    }

    // Does the ceiling create an occlusion?
    if(((openTop < frontSec.ceiling().visHeight() && vOrigin[VY] >= openTop) ||
        (openTop <  backSec.ceiling().visHeight() && vOrigin[VY] <= openTop))
       && canOccludeSectorPairBoundary(frontSec, backSec, true))
    {
        C_AddViewRelOcclusion(from.origin(), to.origin(), openTop, true);
    }
}

/**
 * Add angle clipper occlusion ranges for the edges of the current leaf.
 */
static void occludeLeaf(bool frontFacing)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    if(devNoCulling) return;
    if(P_IsInVoid(viewPlayer)) return;

    HEdge *base = leaf->firstHEdge();
    HEdge *hedge = base;
    do
    {
        occludeHEdge(*hedge, frontFacing);
    } while((hedge = &hedge->next()) != base);
}

/// If @a hedge is @em not front facing this is no-op.
static inline void clipFrontFacingHEdge(HEdge &hedge)
{
    if(hedge.isFlagged(HEdge::FacingFront))
    {
        if(!C_CheckRangeFromViewRelPoints(hedge.fromOrigin(), hedge.toOrigin()))
        {
            hedge.setFlags(HEdge::FacingFront, UnsetFlags);
        }
    }
}

static void clipFrontFacingHEdges()
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));

    if(HEdge *base = bspLeaf->firstHEdge())
    {
        HEdge *hedge = base;
        do
        {
            if(hedge->hasLineSide())
            {
                clipFrontFacingHEdge(*hedge);
            }
        } while((hedge = &hedge->next()) != base);
    }

    if(Polyobj *po = bspLeaf->firstPolyobj())
    {
        foreach(Line *line, po->lines())
        {
            clipFrontFacingHEdge(*line->front().leftHEdge());
        }
    }
}

/**
 * @pre Assumes the leaf is at least partially visible.
 */
static void drawCurrentLeaf()
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    // Mark the sector visible for this frame.
    leaf->sector()._frameFlags |= SIF_VISIBLE;

    markFrontFacingHEdges();
    R_InitForBspLeaf(leaf);
    Rend_RadioBspLeafEdges(*leaf);

    /*
     * Before clip testing lumobjs (for halos), range-occlude the back facing edges.
     * After testing, range-occlude the front facing edges. Done before drawing wall
     * sections so that opening occlusions cut out unnecessary oranges.
     */

    occludeLeaf(false /* back facing */);
    LO_ClipInBspLeaf(leaf->indexInMap());
    occludeLeaf(true /* front facing */);

    clipFrontFacingHEdges();

    if(leaf->hasPolyobj())
    {
        // Polyobjs don't obstruct - clip lights with another algorithm.
        LO_ClipInBspLeafBySight(leaf->indexInMap());
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
    R_AddSprites(leaf);

    writeLeafSkyMask();
    writeLeafWallSections();
    writeLeafPolyobjs();
    writeLeafPlanes();
}

/**
 * Change the current BspLeaf (updating any relevant draw state properties
 * accordingly).
 *
 * @param bspLeaf  The new BSP leaf to make current.
 */
static void makeCurrent(BspLeaf *bspLeaf)
{
    DENG_ASSERT(bspLeaf != 0);
    bool sectorChanged = (!currentBspLeaf || currentBspLeaf->sectorPtr() != bspLeaf->sectorPtr());

    currentBspLeaf = bspLeaf;

    // Update draw state.
    if(sectorChanged)
    {
        currentSectorLightColor = Rend_SectorLightColor(bspLeaf->sector());
        currentSectorLightLevel = bspLeaf->sector().lightLevel();
    }
}

static void traverseBspAndDrawLeafs(MapElement *bspElement)
{
    DENG_ASSERT(bspElement != 0);

    while(bspElement->type() != DMU_BSPLEAF)
    {
        // Descend deeper into the nodes.
        BspNode const *bspNode = bspElement->castTo<BspNode>();

        // Decide which side the view point is on.
        int eyeSide = bspNode->partition().pointOnSide(eyeOrigin) < 0;

        // Recursively divide front space.
        traverseBspAndDrawLeafs(bspNode->childPtr(eyeSide));

        // If the clipper is full we're pretty much done. This means no geometry
        // will be visible in the distance because every direction has already
        // been fully covered by geometry.
        if(!firstBspLeaf && C_IsFull())
            return;

        // ...and back space.
        bspElement = bspNode->childPtr(eyeSide ^ 1);
    }

    // We've arrived at a leaf.
    BspLeaf *bspLeaf = bspElement->castTo<BspLeaf>();

    // Skip null leafs (those with zero volume). Neighbors handle adding the
    // solid clipper segments.
    if(isNullLeaf(bspLeaf))
        return;

    // Is this leaf visible?
    if(!firstBspLeaf && !C_CheckBspLeaf(bspLeaf))
        return;

    // This is now the current leaf.
    makeCurrent(bspLeaf);

    drawCurrentLeaf();

    // This is no longer the first leaf.
    firstBspLeaf = false;
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

static void drawSurfaceTangentSpaceVectors(Surface *suf, Vector3f const &origin)
{
    int const VISUAL_LENGTH = 20;

    static float const red[3]   = { 1, 0, 0 };
    static float const green[3] = { 0, 1, 0 };
    static float const blue[3]  = { 0, 0, 1 };

    DENG_ASSERT(suf != 0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(origin.x, origin.z, origin.y);

    if(devSurfaceVectors & SVF_TANGENT)   drawVector(suf->tangent(),   VISUAL_LENGTH, red);
    if(devSurfaceVectors & SVF_BITANGENT) drawVector(suf->bitangent(), VISUAL_LENGTH, green);
    if(devSurfaceVectors & SVF_NORMAL)    drawVector(suf->normal(),    VISUAL_LENGTH, blue);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

/**
 * Draw the surface tangent space vectors, primarily for debug.
 */
void Rend_RenderSurfaceVectors()
{
    if(devSurfaceVectors == 0 || !theMap) return;

    glDisable(GL_CULL_FACE);

    Vector3f origin;
    foreach(HEdge *hedge, theMap->hedges())
    {
        if(!hedge->hasLineSide() || hedge->line().isFromPolyobj())
            continue;

        if(!hedge->hasBspLeaf() || !hedge->bspLeaf().hasSector())
            continue;

        if(!(hedge->hasTwin() && hedge->twin().hasBspLeaf() && hedge->twin().bspLeaf().hasSector()))
        {
            coord_t const bottom = hedge->bspLeafSector().floor().visHeight();
            coord_t const top    = hedge->bspLeafSector().ceiling().visHeight();
            Surface *suf = &hedge->lineSide().middle();

            origin = Vector3f(hedge->center(), bottom + (top - bottom) / 2);
            drawSurfaceTangentSpaceVectors(suf, origin);
        }
        else
        {
            Sector *backSec  = hedge->twin().bspLeafSectorPtr();
            Line::Side &side = hedge->lineSide();

            if(side.middle().hasMaterial())
            {
                coord_t const bottom = hedge->bspLeafSector().floor().visHeight();
                coord_t const top    = hedge->bspLeafSector().ceiling().visHeight();
                Surface *suf = &side.middle();

                origin = Vector3f(hedge->center(), bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }

            if(backSec->ceiling().visHeight() <
               hedge->bspLeafSector().ceiling().visHeight() &&
               !(hedge->bspLeafSector().ceilingSurface().hasSkyMaskedMaterial() &&
                 backSec->ceilingSurface().hasSkyMaskedMaterial()))
            {
                coord_t const bottom = backSec->ceiling().visHeight();
                coord_t const top    = hedge->bspLeafSector().ceiling().visHeight();
                Surface *suf = &side.top();

                origin = Vector3f(hedge->center(), bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }

            if(backSec->floor().visHeight() >
               hedge->bspLeafSector().floor().visHeight() &&
               !(hedge->bspLeafSector().floorSurface().hasSkyMaskedMaterial() &&
                 backSec->floorSurface().hasSkyMaskedMaterial()))
            {
                coord_t const bottom = hedge->bspLeafSector().floor().visHeight();
                coord_t const top    = backSec->floor().visHeight();
                Surface *suf = &side.bottom();

                origin = Vector3f(hedge->center(), bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }
        }
    }

    foreach(BspLeaf *bspLeaf, theMap->bspLeafs())
    {
        if(!bspLeaf->hasSector()) continue;
        Sector &sector = bspLeaf->sector();

        foreach(Plane *plane, sector.planes())
        {
            origin = Vector3f(bspLeaf->center(), plane->visHeight());

            if(plane->type() != Plane::Middle && plane->surface().hasSkyMaskedMaterial())
                origin.z = theMap->skyFix(plane->type() == Plane::Ceiling);

            drawSurfaceTangentSpaceVectors(&plane->surface(), origin);
        }
    }

    foreach(Polyobj *polyobj, theMap->polyobjs())
    {
        Sector const &sector = polyobj->bspLeaf->sector();
        float zPos = sector.floor().height() + (sector.ceiling().height() - sector.floor().height())/2;

        foreach(Line *line, polyobj->lines())
        {
            origin = Vector3f(line->center(), zPos);
            drawSurfaceTangentSpaceVectors(&line->front().middle(), origin);
        }
    }

    glEnable(GL_CULL_FACE);
}

static void drawSoundOrigin(Vector3d const &origin, char const *label, Vector3d const &eye)
{
    coord_t const MAX_SOUNDORIGIN_DIST = 384; ///< Maximum distance from origin to eye in map coordinates.

    if(!label) return;

    coord_t dist = Vector3d(eye - origin).length();
    float alpha = 1.f - de::min(dist, MAX_SOUNDORIGIN_DIST) / MAX_SOUNDORIGIN_DIST;

    if(alpha > 0)
    {
        float scale = dist / (DENG_WINDOW->width() / 2);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(origin.x, origin.z, origin.y);
        glRotatef(-vang + 180, 0, 1, 0);
        glRotatef(vpitch, 1, 0, 0);
        glScalef(-scale, -scale, 1);

        Point2Raw const labelOrigin(2, 2);
        UI_TextOutEx(label, &labelOrigin, UI_Color(UIC_TITLE), alpha);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
}

/**
 * Debugging aid for visualizing sound origins.
 */
void Rend_RenderSoundOrigins()
{
    if(!devSoundOrigins || !theMap) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    Vector3d eye(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

    if(devSoundOrigins & SOF_SIDE)
    {
        /// @todo Do not assume current map.
        foreach(Line *line, theMap->lines())
        for(int i = 0; i < 2; ++i)
        {
            if(!line->hasSections(i))
                continue;

            Line::Side &side = line->side(i);
            char buf[80];

            dd_snprintf(buf, 80, "Line #%d (%s, middle)", line->indexInMap(), (i? "back" : "front"));
            drawSoundOrigin(Vector3d(side.middleSoundEmitter().origin), buf, eye);

            dd_snprintf(buf, 80, "Line #%d (%s, bottom)", line->indexInMap(), (i? "back" : "front"));
            drawSoundOrigin(Vector3d(side.bottomSoundEmitter().origin), buf, eye);

            dd_snprintf(buf, 80, "Line #%d (%s, top)", line->indexInMap(), (i? "back" : "front"));
            drawSoundOrigin(Vector3d(side.topSoundEmitter().origin), buf, eye);
        }
    }

    if(devSoundOrigins & (SOF_SECTOR|SOF_PLANE))
    {
        /// @todo Do not assume current map.
        foreach(Sector *sec, theMap->sectors())
        {
            char buf[80];

            if(devSoundOrigins & SOF_PLANE)
            {
                for(int i = 0; i < sec->planeCount(); ++i)
                {
                    Plane &plane = sec->plane(i);
                    dd_snprintf(buf, 80, "Sector #%i (pln:%i)", sec->indexInMap(), i);
                    drawSoundOrigin(Vector3d(plane.soundEmitter().origin), buf, eye);
                }
            }

            if(devSoundOrigins & SOF_SECTOR)
            {
                dd_snprintf(buf, 80, "Sector #%i", sec->indexInMap());
                drawSoundOrigin(Vector3d(sec->soundEmitter().origin), buf, eye);
            }
        }
    }

    // Restore previous state.
    glEnable(GL_DEPTH_TEST);
}

static void getVertexPlaneMinMax(Vertex const *vtx, coord_t *min, coord_t *max)
{
    if(!vtx || (!min && !max)) return;

    LineOwner const *base = vtx->firstLineOwner();
    LineOwner const *own  = base;
    do
    {
        Line *li = &own->line();

        if(li->hasFrontSections())
        {
            if(min && li->frontSector().floor().visHeight() < *min)
                *min = li->frontSector().floor().visHeight();

            if(max && li->frontSector().ceiling().visHeight() > *max)
                *max = li->frontSector().ceiling().visHeight();
        }

        if(li->hasBackSections())
        {
            if(min && li->backSector().floor().visHeight() < *min)
                *min = li->backSector().floor().visHeight();

            if(max && li->backSector().ceiling().visHeight() > *max)
                *max = li->backSector().ceiling().visHeight();
        }

        own = &own->next();
    } while(own != base);
}

static void drawVertexPoint(const Vertex* vtx, coord_t z, float alpha)
{
    glBegin(GL_POINTS);
        glColor4f(.7f, .7f, .2f, alpha * 2);
        glVertex3f(vtx->origin().x, z, vtx->origin().y);
    glEnd();
}

static void drawVertexBar(Vertex const *vtx, coord_t bottom, coord_t top, float alpha)
{
    int const EXTEND_DIST = 64;

    static const float black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(vtx->origin().x, bottom - EXTEND_DIST, vtx->origin().y);
        glColor4f(1, 1, 1, alpha);
        glVertex3f(vtx->origin().x, bottom, vtx->origin().y);
        glVertex3f(vtx->origin().x, bottom, vtx->origin().y);
        glVertex3f(vtx->origin().x, top, vtx->origin().y);
        glVertex3f(vtx->origin().x, top, vtx->origin().y);
        glColor4fv(black);
        glVertex3f(vtx->origin().x, top + EXTEND_DIST, vtx->origin().y);
    glEnd();
}

static void drawVertexIndex(Vertex const *vtx, coord_t z, float scale, float alpha)
{
    DENG_ASSERT(vtx != 0);
    Point2Raw const origin(2, 2);
    char buf[80];

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    sprintf(buf, "%i", vtx->indexInMap());

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(vtx->origin().x, z, vtx->origin().y);
    glRotatef(-vang + 180, 0, 1, 0);
    glRotatef(vpitch, 1, 0, 0);
    glScalef(-scale, -scale, 1);

    glEnable(GL_TEXTURE_2D);

    UI_TextOutEx(buf, &origin, UI_Color(UIC_TITLE), alpha);

    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

#define MAX_VERTEX_POINT_DIST 1280

static int drawVertex1(Line *li, void *context)
{
    Vertex *vtx = &li->from();
    Polyobj *po = (Polyobj *) context;
    coord_t dist2D = M_ApproxDistance(vOrigin[VX] - vtx->origin().x, vOrigin[VZ] - vtx->origin().y);

    if(dist2D < MAX_VERTEX_POINT_DIST)
    {
        float alpha = 1 - dist2D / MAX_VERTEX_POINT_DIST;

        if(alpha > 0)
        {
            coord_t bottom = po->bspLeaf->sector().floor().visHeight();
            coord_t top    = po->bspLeaf->sector().ceiling().visHeight();

            if(devVertexBars)
                drawVertexBar(vtx, bottom, top, MIN_OF(alpha, .15f));

            drawVertexPoint(vtx, bottom, alpha * 2);
        }
    }

    if(devVertexIndices)
    {
        coord_t eye[3], pos[3], dist3D;

        eye[VX] = vOrigin[VX];
        eye[VY] = vOrigin[VZ];
        eye[VZ] = vOrigin[VY];

        pos[VX] = vtx->origin().x;
        pos[VY] = vtx->origin().y;
        pos[VZ] = po->bspLeaf->sector().floor().visHeight();

        dist3D = V3d_Distance(pos, eye);

        if(dist3D < MAX_VERTEX_POINT_DIST)
        {
            drawVertexIndex(vtx, pos[VZ], dist3D / (DENG_WINDOW->width() / 2),
                            1 - dist3D / MAX_VERTEX_POINT_DIST);
        }
    }

    return false; // Continue iteration.
}

static int drawPolyObjVertexes(Polyobj *po, void * /*context*/)
{
    DENG_ASSERT(po != 0);
    foreach(Line *line, po->lines())
    {
        if(line->validCount() == validCount)
            continue;

        line->setValidCount(validCount);
        int result = drawVertex1(line, po);
        if(result) return result;
    }
    return false; // Continue iteration.
}

/**
 * Draw the various vertex debug aids.
 */
void Rend_Vertexes()
{
    float oldLineWidth = -1;

    if(!devVertexBars && !devVertexIndices) return;

    DENG_ASSERT(theMap);

    glDisable(GL_DEPTH_TEST);

    if(devVertexBars)
    {
        glEnable(GL_LINE_SMOOTH);
        oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
        DGL_SetFloat(DGL_LINE_WIDTH, 2);

        foreach(Vertex *vertex, theMap->vertexes())
        {
            // Not a line vertex?
            LineOwner const *own = vertex->firstLineOwner();
            if(!own) continue;

            // Ignore polyobj vertexes.
            if(own->line().isFromPolyobj()) continue;

            float alpha = 1 - M_ApproxDistance(vOrigin[VX] - vertex->origin().x,
                                               vOrigin[VZ] - vertex->origin().y) / MAX_VERTEX_POINT_DIST;
            alpha = de::min(alpha, .15f);

            if(alpha > 0)
            {
                coord_t bottom = DDMAXFLOAT;
                coord_t top    = DDMINFLOAT;
                getVertexPlaneMinMax(vertex, &bottom, &top);

                drawVertexBar(vertex, bottom, top, alpha);
            }
        }
    }

    // Draw the vertex point nodes.
    float const oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);

    glEnable(GL_POINT_SMOOTH);
    DGL_SetFloat(DGL_POINT_SIZE, 6);

    foreach(Vertex *vertex, theMap->vertexes())
    {
        // Not a line vertex?
        LineOwner const *own = vertex->firstLineOwner();
        if(!own) continue;

        // Ignore polyobj vertexes.
        if(own->line().isFromPolyobj()) continue;

        coord_t dist = M_ApproxDistance(vOrigin[VX] - vertex->origin().x,
                                       vOrigin[VZ] - vertex->origin().y);

        if(dist < MAX_VERTEX_POINT_DIST)
        {
            coord_t bottom = DDMAXFLOAT;
            getVertexPlaneMinMax(vertex, &bottom, NULL);

            drawVertexPoint(vertex, bottom, (1 - dist / MAX_VERTEX_POINT_DIST) * 2);
        }
    }

    if(devVertexIndices)
    {
        coord_t eye[3];

        eye[VX] = vOrigin[VX];
        eye[VY] = vOrigin[VZ];
        eye[VZ] = vOrigin[VY];

        foreach(Vertex *vertex, theMap->vertexes())
        {
            coord_t pos[3], dist;

            // Not a line vertex?
            LineOwner const *own = vertex->firstLineOwner();
            if(!own) continue;

            // Ignore polyobj vertexes.
            if(own->line().isFromPolyobj()) continue;

            pos[VX] = vertex->origin().x;
            pos[VY] = vertex->origin().y;
            pos[VZ] = DDMAXFLOAT;
            getVertexPlaneMinMax(vertex, &pos[VZ], NULL);

            dist = V3d_Distance(pos, eye);

            if(dist < MAX_VERTEX_POINT_DIST)
            {
                float const alpha = 1 - dist / MAX_VERTEX_POINT_DIST;
                float const scale = dist / (DENG_WINDOW->width() / 2);

                drawVertexIndex(vertex, pos[VZ], scale, alpha);
            }
        }
    }

    // Next, the vertexes of all nearby polyobjs.
    AABoxd box;
    box.minX = vOrigin[VX] - MAX_VERTEX_POINT_DIST;
    box.minY = vOrigin[VY] - MAX_VERTEX_POINT_DIST;
    box.maxX = vOrigin[VX] + MAX_VERTEX_POINT_DIST;
    box.maxY = vOrigin[VY] + MAX_VERTEX_POINT_DIST;
    P_PolyobjsBoxIterator(&box, drawPolyObjVertexes, NULL);

    // Restore previous state.
    if(devVertexBars)
    {
        DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);
        glDisable(GL_LINE_SMOOTH);
    }
    DGL_SetFloat(DGL_POINT_SIZE, oldPointSize);
    glDisable(GL_POINT_SMOOTH);
    glEnable(GL_DEPTH_TEST);
}

void Rend_RenderMap()
{
    binangle_t viewside;

    if(!theMap) return;

    // Set to true if dynlights are inited for this frame.
    loInited = false;

    GL_SetMultisample(true);

    // Setup the modelview matrix.
    Rend_ModelViewMatrix(true);

    if(!freezeRLs)
    {
        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

        // Prepare for rendering.
        RL_ClearLists(); // Clear the lists for new quads.
        C_ClearRanges(); // Clear the clipper.

        // Recycle the vlight lists. Currently done here as the lists are
        // not shared by all viewports.
        VL_InitForNewFrame();

        // Make vissprites of all the visible decorations.
        Rend_DecorProject();

        LO_BeginFrame();

        // Clear particle generator visibilty info.
        Rend_ParticleInitForNewFrame();

        if(Rend_MobjShadowsEnabled())
        {
            R_InitShadowProjectionListsForNewFrame();
        }

        eyeOrigin = Vector2d(viewData->current.origin);

        // Add the backside clipping range (if vpitch allows).
        if(vpitch <= 90 - yfov / 2 && vpitch >= -90 + yfov / 2)
        {
            float a = fabs(vpitch) / (90 - yfov / 2);
            binangle_t startAngle = binangle_t(BANG_45 * fieldOfView / 90) * (1 + a);
            binangle_t angLen = BANG_180 - startAngle;

            viewside = (viewData->current.angle >> (32 - BAMS_BITS)) + startAngle;
            C_SafeAddRange(viewside, viewside + angLen);
            C_SafeAddRange(viewside + angLen, viewside + 2 * angLen);
        }

        // The viewside line for the depth cue.
        viewsidex = -viewData->viewSin;
        viewsidey = viewData->viewCos;

        // We don't want BSP clip checking for the first BSP leaf.
        firstBspLeaf = true;

        // Draw the world!
        traverseBspAndDrawLeafs(theMap->bspRoot());

        if(Rend_MobjShadowsEnabled())
        {
            Rend_RenderMobjShadows();
        }
    }
    RL_RenderAllLists();

    // Draw various debugging displays:
    Rend_RenderSurfaceVectors();
    LO_DrawLumobjs(); // Lumobjs.
    Rend_RenderBoundingBoxes(); // Mobj bounding boxes.
    Rend_Vertexes();
    Rend_RenderSoundOrigins();
    Rend_RenderGenerators();

    // Draw the Source Bias Editor's draw that identifies the current light.
    SBE_DrawCursor();

    GL_SetMultisample(false);
}

void Rend_CalcLightModRange()
{
    if(novideo) return;

    std::memset(lightModRange, 0, sizeof(float) * 255);

    if(!theMap)
    {
        rAmbient = 0;
        return;
    }

    int mapAmbient = theMap->ambientLightLevel();
    if(mapAmbient > ambientLight)
        rAmbient = mapAmbient;
    else
        rAmbient = ambientLight;

    for(int j = 0; j < 255; ++j)
    {
        // Adjust the white point/dark point?
        float f = 0;
        if(lightRangeCompression != 0)
        {
            if(lightRangeCompression >= 0)
            {
                // Brighten dark areas.
                f = float(255 - j) * lightRangeCompression;
            }
            else
            {
                // Darken bright areas.
                f = float(-j) * -lightRangeCompression;
            }
        }

        // Lower than the ambient limit?
        if(rAmbient != 0 && j+f <= rAmbient)
            f = rAmbient - j;

        // Clamp the result as a modifier to the light value (j).
        if((j+f) >= 255)
            f = 255 - j;
        else if((j+f) <= 0)
            f = -j;

        // Insert it into the matrix
        lightModRange[j] = f / 255.0f;
    }
}

float Rend_LightAdaptationDelta(float val)
{
    int clampedVal;

    clampedVal = ROUND(255.0f * val);
    if(clampedVal > 254)
        clampedVal = 254;
    else if(clampedVal < 0)
        clampedVal = 0;

    return lightModRange[clampedVal];
}

void Rend_ApplyLightAdaptation(float* val)
{
    if(!val) return;
    *val += Rend_LightAdaptationDelta(*val);
}

/**
 * Draws the lightModRange (for debug)
 */
void R_DrawLightRange()
{
#define BLOCK_WIDTH             (1.0f)
#define BLOCK_HEIGHT            (BLOCK_WIDTH * 255.0f)
#define BORDER                  (20)

    //ui_color_t color;
    float c, off;
    int i;

    // Disabled?
    if(!devLightModRange) return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_WINDOW->width(), DENG_WINDOW->height(), 0, -1, 1);

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
    for(i = 0, c = 0; i < 255; ++i, c += (1.0f/255.0f))
    {
        // Get the result of the source light level + offset.
        off = lightModRange[i];

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
    if(!(mo->bspLeaf && mo->bspLeaf->sector().frameFlags() & SIF_VISIBLE))
        return false; // Continue iteration.

    Vector3d eye(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

    float alpha = 1 - ((Vector3d(eye - Vector3d(mo->origin)).length() / (DENG_WINDOW->width()/2)) / 4);
    if(alpha < .25f)
        alpha = .25f; // Don't make them totally invisible.

    // Draw a bounding box in an appropriate color.
    coord_t size = mo->radius;
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
static void Rend_RenderBoundingBoxes()
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

    Vector3d eye(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    MaterialSnapshot const &ms = App_Materials().
            find(de::Uri("System", Path("bbox"))).material().prepare(Rend_SpriteMaterialSpec());

    GL_BindTexture(&ms.texture(MTU_PRIMARY));
    GL_BlendMode(BM_ADD);

    if(devMobjBBox)
    {
        GameMap_IterateThinkers(theMap, reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                                0x1, drawMobjBBox, NULL);
    }

    if(devPolyobjBBox)
    {
        foreach(Polyobj const *polyobj, theMap->polyobjs())
        {
            Sector const &sec = polyobj->bspLeaf->sector();
            coord_t width  = (polyobj->aaBox.maxX - polyobj->aaBox.minX)/2;
            coord_t length = (polyobj->aaBox.maxY - polyobj->aaBox.minY)/2;
            coord_t height = (sec.ceiling().height() - sec.floor().height())/2;

            Vector3d pos(polyobj->aaBox.minX + width,
                         polyobj->aaBox.minY + length,
                         sec.floor().height());

            float alpha = 1 - ((Vector3d(eye - pos).length() / (DENG_WINDOW->width()/2)) / 4);
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

MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec()
{
    return App_Materials().variantSpec(MapSurfaceContext, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                                       -1, -1, -1, true, true, false, false);
}

texturevariantspecification_t &Rend_MapSurfaceShinyTextureSpec()
{
    return GL_TextureVariantSpec(TC_MAPSURFACE_REFLECTION, TSF_NO_COMPRESSION,
                                 0, 0, 0, GL_REPEAT, GL_REPEAT, 1, 1, -1,
                                 false, false, false, false);
}

texturevariantspecification_t &Rend_MapSurfaceShinyMaskTextureSpec()
{
    return GL_TextureVariantSpec(TC_MAPSURFACE_REFLECTIONMASK, 0,
                                 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1,
                                 true, false, false, false);
}
