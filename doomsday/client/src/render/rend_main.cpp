/** @file rend_main.cpp Map Renderer.
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

#include "de_base.h"
#include "de_console.h"
#include "de_edit.h"
#include "de_render.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"
#include "de_system.h"

#include "network/net_main.h"
#ifdef __CLIENT__
#  include "MaterialSnapshot"
#  include "MaterialVariantSpec"
#endif
#include "Texture"
#include "map/blockmapvisual.h"
#include "map/gamemap.h"
#include "map/lineowner.h"
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

byte devMobjVLights = 0; // @c 1= Draw mobj vertex lighting vector.
int devMobjBBox = 0; // 1 = Draw mobj bounding boxes (for debug)
int devPolyobjBBox = 0; // 1 = Draw polyobj bounding boxes (for debug)
DGLuint dlBBox = 0; // Display list: active-textured bbox model.

byte devVertexIndices = 0; // @c 1= Draw world vertex indices (for debug).
byte devVertexBars = 0; // @c 1= Draw world vertex position bars.
byte devSoundOrigins = 0; ///< cvar @c 1= Draw sound origin debug display.
byte devSurfaceVectors = 0;
byte devNoTexFix = 0;

#ifdef __CLIENT__
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

#endif // __CLIENT__

void Rend_Register()
{
#ifdef __CLIENT__

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

#endif
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

#ifdef __CLIENT__

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

static void lightVertex(ColorRawf &color, rvertex_t const &vtx, float lightLevel,
                        Vector3f const &ambientColor)
{
    float const dist = Rend_PointDist2D(vtx.pos);
    float lightVal = R_DistAttenuateLightLevel(dist, lightLevel);

    // Add extra light.
    lightVal += R_ExtraLightDelta();

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

/**
 * Determine which sections of @a line on @a backSide are potentially visible
 * according to the relative heights of the line's plane interfaces.
 *
 * @param line          Line to determine the potentially visible sections of.
 * @param backSide      If non-zero consider the back side, else the front.
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
       (fceil.visHeight() <= bceil.visHeight()))
    {
        sections &= ~Line::Side::TopFlag;
    }

    // Bottom?
    if((!devRendSkyMode && ffloor.surface().hasSkyMaskedMaterial() && bfloor.surface().hasSkyMaskedMaterial()) ||
       (ffloor.visHeight() >= bfloor.visHeight()))
    {
        sections &= ~Line::Side::BottomFlag;
    }

    return sections;
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

#ifdef __CLIENT__

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
        VS_WALL(vis)->modTexCoord[0][1] = dyn->s[1];
        VS_WALL(vis)->modTexCoord[1][0] = dyn->t[0];
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

#endif // __CLIENT__

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

#ifdef __CLIENT__

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
        coord_t segLength;
        Vector3f const *surfaceColor2; // Secondary color.
        struct {
            walldivnode_t *firstDiv;
            uint divCount;
        } left;
        struct {
            walldivnode_t *firstDiv;
            uint divCount;
        } right;
    } wall;
};

static bool renderWorldPoly(rvertex_t *rvertices, uint numVertices,
    rendworldpoly_params_t const &p, MaterialSnapshot const &ms)
{
    DENG_ASSERT(rvertices != 0);

    uint const realNumVertices = ((p.isWall && (p.wall.left.divCount || p.wall.right.divCount))? 3 + p.wall.left.divCount + 3 + p.wall.right.divCount : numVertices);

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
             * dynlight affecting this surface, grab the paramaters needed
             * to draw it.
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
        quadTexCoords(primaryCoords, rvertices, p.wall.segLength, *p.texTL);

        // Blend texture coordinates.
        if(interRTU && !drawAsVisSprite)
            quadTexCoords(interCoords, rvertices, p.wall.segLength, *p.texTL);

        // Shiny texture coordinates.
        if(shinyRTU && !drawAsVisSprite)
            quadShinyTexCoords(shinyTexCoords, &rvertices[1], &rvertices[2], p.wall.segLength);

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
        Rend_AddMaskedPoly(rvertices, rcolors, p.wall.segLength, &ms.materialVariant(),
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

        parm.rvertices       = rvertices;
        parm.numVertices     = numVertices;
        parm.realNumVertices = realNumVertices;
        parm.lastIdx         = 0;
        parm.texTL           = p.texTL;
        parm.texBR           = p.texBR;
        parm.isWall          = p.isWall;
        if(p.isWall)
        {
            parm.wall.left.firstDiv  = p.wall.left.firstDiv;
            parm.wall.left.divCount  = p.wall.left.divCount;
            parm.wall.right.firstDiv = p.wall.right.firstDiv;
            parm.wall.right.divCount = p.wall.right.divCount;
        }

        hasDynlights = (0 != Rend_RenderLightProjections(p.lightListIdx, parm));
    }

    if(useShadows)
    {
        // Render all shadows projected onto this surface.
        rendershadowprojectionparams_t parm;

        parm.rvertices       = rvertices;
        parm.numVertices     = numVertices;
        parm.realNumVertices = realNumVertices;
        parm.texTL           = p.texTL;
        parm.texBR           = p.texBR;
        parm.isWall          = p.isWall;
        if(p.isWall)
        {
            parm.wall.left.firstDiv  = p.wall.left.firstDiv;
            parm.wall.left.divCount  = p.wall.left.divCount;
            parm.wall.right.firstDiv = p.wall.right.firstDiv;
            parm.wall.right.divCount = p.wall.right.divCount;
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
    if(p.isWall && (p.wall.left.divCount || p.wall.right.divCount))
    {
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

        R_DivVerts(rvertices, origVerts, p.wall.left.firstDiv, p.wall.left.divCount, p.wall.right.firstDiv, p.wall.right.divCount);
        R_DivTexCoords(primaryCoords, origTexCoords, p.wall.left.firstDiv, p.wall.left.divCount, p.wall.right.firstDiv, p.wall.right.divCount, bL, tL, bR, tR);

        if(rcolors)
        {
            R_DivVertColors(rcolors, origColors, p.wall.left.firstDiv, p.wall.left.divCount, p.wall.right.firstDiv, p.wall.right.divCount, bL, tL, bR, tR);
        }

        if(interCoords)
        {
            rtexcoord_t origTexCoords2[4];
            std::memcpy(origTexCoords2, interCoords, sizeof(origTexCoords2));
            R_DivTexCoords(interCoords, origTexCoords2, p.wall.left.firstDiv, p.wall.left.divCount, p.wall.right.firstDiv, p.wall.right.divCount, bL, tL, bR, tR);
        }

        if(modCoords)
        {
            rtexcoord_t origTexCoords5[4];
            std::memcpy(origTexCoords5, modCoords, sizeof(origTexCoords5));
            R_DivTexCoords(modCoords, origTexCoords5, p.wall.left.firstDiv, p.wall.left.divCount, p.wall.right.firstDiv, p.wall.right.divCount, bL, tL, bR, tR);
        }

        if(shinyTexCoords)
        {
            rtexcoord_t origShinyTexCoords[4];
            std::memcpy(origShinyTexCoords, shinyTexCoords, sizeof(origShinyTexCoords));
            R_DivTexCoords(shinyTexCoords, origShinyTexCoords, p.wall.left.firstDiv, p.wall.left.divCount, p.wall.right.firstDiv, p.wall.right.divCount, bL, tL, bR, tR);
        }

        if(shinyColors)
        {
            ColorRawf origShinyColors[4];
            std::memcpy(origShinyColors, shinyColors, sizeof(origShinyColors));
            R_DivVertColors(shinyColors, origShinyColors, p.wall.left.firstDiv, p.wall.left.divCount, p.wall.right.firstDiv, p.wall.right.divCount, bL, tL, bR, tR);
        }

        RL_AddPolyWithCoordsModulationReflection(PT_FAN, p.flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
            3 + p.wall.right.divCount, rvertices + 3 + p.wall.left.divCount, rcolors? rcolors + 3 + p.wall.left.divCount : NULL,
            primaryCoords + 3 + p.wall.left.divCount, interCoords? interCoords + 3 + p.wall.left.divCount : NULL,
            modTex, &modColor, modCoords? modCoords + 3 + p.wall.left.divCount : NULL,
            shinyColors + 3 + p.wall.left.divCount, shinyTexCoords? shinyTexCoords + 3 + p.wall.left.divCount : NULL,
            shinyMaskRTU? primaryCoords + 3 + p.wall.left.divCount : NULL);

        RL_AddPolyWithCoordsModulationReflection(PT_FAN, p.flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
            3 + p.wall.left.divCount, rvertices, rcolors,
            primaryCoords, interCoords,
            modTex, &modColor, modCoords,
            shinyColors, shinyTexCoords, shinyMaskRTU? primaryCoords : NULL);
    }
    else
    {
        RL_AddPolyWithCoordsModulationReflection(p.isWall? PT_TRIANGLE_STRIP : PT_FAN, p.flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
            numVertices, rvertices, rcolors,
            primaryCoords, interCoords,
            modTex, &modColor, modCoords,
            shinyColors, shinyTexCoords, shinyMaskRTU? primaryCoords : NULL);
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

static bool writeWallSection2(HEdge &hedge, Vector3f const &normal,
    float alpha, float lightLevelDL, float lightLevelDR,
    uint lightListIdx, uint shadowListIdx,
    walldivs_t *leftWallDivs, walldivs_t *rightWallDivs,
    bool skyMask, bool addFakeRadio, Vector3d const &texTL, Vector3d const &texBR,
    Vector2f const &materialOrigin, Vector2f const &materialScale,
    blendmode_t blendMode, Vector3f const &color, Vector3f const &color2,
    biassurface_t *bsuf, int elmIdx /*tmp*/,
    MaterialSnapshot const &ms,
    bool isTwosidedMiddle)
{
    rendworldpoly_params_t parm;
    std::memset(&parm, 0, sizeof(parm));

    parm.flags = RPF_DEFAULT | (skyMask? RPF_SKYMASK : 0);
    parm.isWall = true;
    parm.wall.segLength = hedge.length();
    parm.forceOpaque = (alpha < 0? true : false);
    parm.alpha = (alpha < 0? 1 : alpha);
    parm.mapElement = &hedge;
    parm.elmIdx = elmIdx;
    parm.bsuf = bsuf;
    parm.normal = &normal;
    parm.texTL = &texTL;
    parm.texBR = &texBR;
    parm.surfaceLightLevelDL = lightLevelDL;
    parm.surfaceLightLevelDR = lightLevelDR;
    parm.surfaceColor = &color;
    parm.wall.surfaceColor2 = &color2;
    if(glowFactor > .0001f)
        parm.glowing = ms.glowStrength() * glowFactor; // Global scale factor.
    parm.blendMode = blendMode;
    parm.materialOrigin = &materialOrigin;
    parm.materialScale = &materialScale;
    parm.lightListIdx = lightListIdx;
    parm.shadowListIdx = shadowListIdx;
    parm.wall.left.firstDiv  = WallDivNode_Next(WallDivs_First(leftWallDivs)); // Step over first node.
    parm.wall.left.divCount  = WallDivs_Size(leftWallDivs)-2;
    parm.wall.right.firstDiv = WallDivNode_Prev(WallDivs_Last(rightWallDivs)); // Step over last node.
    parm.wall.right.divCount = WallDivs_Size(rightWallDivs)-2;

    rvertex_t *rvertices;
    // Allocate enough vertices for the divisions too.
    if(WallDivs_Size(leftWallDivs) > 2 || WallDivs_Size(rightWallDivs) > 2)
    {
        // Use two fans.
        rvertices = R_AllocRendVertices(1 + WallDivs_Size(leftWallDivs) + 1 + WallDivs_Size(rightWallDivs));
    }
    else
    {
        // Use a quad.
        rvertices = R_AllocRendVertices(4);
    }

    // Vertex coords.
    // Bottom Left.
    V3f_Set(rvertices[0].pos, hedge.fromOrigin().x,
                              hedge.fromOrigin().y,
                              WallDivNode_Height(WallDivs_First(leftWallDivs)));
    // Top Left.
    V3f_Set(rvertices[1].pos, hedge.fromOrigin().x,
                              hedge.fromOrigin().y,
                              WallDivNode_Height(WallDivs_Last(leftWallDivs)));
    // Bottom Right.
    V3f_Set(rvertices[2].pos, hedge.toOrigin().x,
                              hedge.toOrigin().y,
                              WallDivNode_Height(WallDivs_First(rightWallDivs)));
    // Top Right.
    V3f_Set(rvertices[3].pos, hedge.toOrigin().x,
                              hedge.toOrigin().y,
                              WallDivNode_Height(WallDivs_Last(rightWallDivs)));

    // Draw this section.
    bool opaque = renderWorldPoly(rvertices, 4, parm, ms);
    if(opaque)
    {
        // Render FakeRadio for this section?
        if(!(parm.flags & RPF_SKYMASK) && addFakeRadio)
        {
            RendRadioWallSectionParms radioParms;
            std::memset(&radioParms, 0, sizeof(radioParms));

            radioParms.line      = &hedge.line();

            LineSideRadioData &frData = Rend_RadioDataForLineSide(hedge.lineSide());
            radioParms.botCn     = frData.bottomCorners;
            radioParms.topCn     = frData.topCorners;
            radioParms.sideCn    = frData.sideCorners;
            radioParms.spans     = frData.spans;

            radioParms.segOffset = hedge.lineOffset();
            radioParms.segLength = hedge.length();
            radioParms.frontSec  = hedge.bspLeafSectorPtr();

            radioParms.wall.left.firstDiv  = parm.wall.left.firstDiv;
            radioParms.wall.left.divCount  = parm.wall.left.divCount;
            radioParms.wall.right.firstDiv = parm.wall.right.firstDiv;
            radioParms.wall.right.divCount = parm.wall.right.divCount;

            if(!isTwosidedMiddle && !(hedge.hasTwin() && !(hedge.twin().hasLineSide() && hedge.twin().lineSide().hasSections())))
            {
                if(hedge.hasTwin())
                    radioParms.backSec = hedge.twin().bspLeafSectorPtr();
            }

            /// @todo kludge: Revert the vertex coords as they may have been changed
            ///               due to height divisions.

            // Bottom Left.
            V3f_Set(rvertices[0].pos, hedge.fromOrigin().x,
                                      hedge.fromOrigin().y,
                                      WallDivNode_Height(WallDivs_First(leftWallDivs)));
            // Top Left.
            V3f_Set(rvertices[1].pos, hedge.fromOrigin().x,
                                      hedge.fromOrigin().y,
                                      WallDivNode_Height(WallDivs_Last(leftWallDivs)));
            // Bottom Right.
            V3f_Set(rvertices[2].pos, hedge.toOrigin().x,
                                      hedge.toOrigin().y,
                                      WallDivNode_Height(WallDivs_First(rightWallDivs)));
            // Top Right.
            V3f_Set(rvertices[3].pos, hedge.toOrigin().x,
                                      hedge.toOrigin().y,
                                      WallDivNode_Height(WallDivs_Last(rightWallDivs)));

            // kludge end.

            float ll = currentSectorLightLevel;
            Rend_ApplyLightAdaptation(&ll);
            if(ll > 0)
            {
                // Determine the shadow properties.
                /// @todo Make cvars out of constants.
                radioParms.shadowSize = 2 * (8 + 16 - ll * 16);
                radioParms.shadowDark = Rend_RadioCalcShadowDarkness(ll);

                if(radioParms.shadowSize > 0)
                {
                    // Shadows are black.
                    radioParms.shadowRGB[CR] =
                            radioParms.shadowRGB[CG] =
                                radioParms.shadowRGB[CB] = 0;

                    Rend_RadioWallSection(rvertices, radioParms);
                }
            }
        }
    }

    R_FreeRendVertices(rvertices);
    return opaque; // Clip with this if opaque.
}

static void writePlane2(BspLeaf &bspLeaf, Plane::Type type, coord_t height,
    Vector3f const &tangent, Vector3f const &bitangent, Vector3f const &normal,
    Material *inMat, Vector3f const &sufColor, float sufAlpha, blendmode_t blendMode,
    Vector3d const &texTL, Vector3d const &texBR,
    Vector2f const &materialOrigin, Vector2f const &materialScale,
    bool skyMasked, bool addDLights, bool addMobjShadows,
    biassurface_t *bsuf, int elmIdx /*tmp*/)
{
    Sector *sec = bspLeaf.sectorPtr();

    rendworldpoly_params_t parms;
    std::memset(&parms, 0, sizeof(parms));

    parms.flags = RPF_DEFAULT;
    parms.isWall = false;
    parms.mapElement = &bspLeaf;
    parms.elmIdx = elmIdx;
    parms.bsuf = bsuf;
    parms.normal = &normal;
    parms.texTL = &texTL;
    parms.texBR = &texBR;
    parms.surfaceLightLevelDL = parms.surfaceLightLevelDR = 0;
    parms.surfaceColor = &sufColor;
    parms.materialOrigin = &materialOrigin;
    parms.materialScale = &materialScale;

    Material *mat = 0;
    if(skyMasked)
    {
        // In devRendSkyMode mode we render all polys destined for the
        // skymask as regular world polys (with a few obvious properties).
        if(devRendSkyMode)
        {
            parms.blendMode = BM_NORMAL;
            parms.glowing = 1;
            parms.forceOpaque = true;
            mat = inMat;
        }
        else
        {   // We'll mask this.
            parms.flags |= RPF_SKYMASK;
        }
    }
    else
    {
        mat = inMat;

        if(type != Plane::Middle)
        {
            parms.blendMode = BM_NORMAL;
            parms.alpha = 1;
            parms.forceOpaque = true;
        }
        else
        {
            if(blendMode == BM_NORMAL && noSpriteTrans)
                parms.blendMode = BM_ZEROALPHA; // "no translucency" mode
            else
                parms.blendMode = blendMode;
            parms.alpha = sufAlpha;
        }
    }

    uint numVertices;
    rvertex_t *rvertices;
    buildLeafPlaneGeometry(bspLeaf, (type == Plane::Ceiling), height,
                           &rvertices, &numVertices);

    MaterialSnapshot const &ms = mat->prepare(mapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT));

    if(!(parms.flags & RPF_SKYMASK))
    {
        if(glowFactor > .0001f)
        {
            Surface const &suf = sec->planeSurface(elmIdx);
            if(mat == suf.materialPtr())
            {
                parms.glowing = ms.glowStrength();
            }
            else
            {
                Material *material = suf.hasMaterial()? suf.materialPtr()
                                                      : &App_Materials().find(de::Uri("System", Path("missing"))).material();

                MaterialSnapshot const &ms = material->prepare(Rend_MapSurfaceMaterialSpec());
                parms.glowing = ms.glowStrength();
            }

            parms.glowing *= glowFactor; // Global scale factor.
        }

        // Dynamic lights?
        if(addDLights && parms.glowing < 1 && !(!useDynLights && !useWallGlow))
        {
            /// @ref projectLightFlags
            int plFlags = (PLF_NO_PLANE | (type == Plane::Floor? PLF_TEX_FLOOR : PLF_TEX_CEILING));

            parms.lightListIdx =
                LO_ProjectToSurface(plFlags, &bspLeaf, 1, *parms.texTL, *parms.texBR,
                                    tangent, bitangent, normal);
        }

        // Mobj shadows?
        if(addMobjShadows && parms.glowing < 1 && Rend_MobjShadowsEnabled())
        {
            // Glowing planes inversely diminish shadow strength.
            float blendFactor = 1 - parms.glowing;

            parms.shadowListIdx =
                R_ProjectShadowsToSurface(&bspLeaf, blendFactor, *parms.texTL, *parms.texBR,
                                          tangent, bitangent, normal);
        }
    }

    renderWorldPoly(rvertices, numVertices, parms, ms);

    R_FreeRendVertices(rvertices);
}

static void writePlane(Plane::Type type, coord_t height,
    Vector3f const &tangent, Vector3f const &bitangent, Vector3f const &normal,
    Material *material, Vector3f const &tintColor, float opacity, blendmode_t blendMode,
    Vector2f const &materialOrigin, Vector2f const &materialScale,
    bool skyMasked, bool addDLights, bool addMobjShadows,
    biassurface_t *bsuf, int elmIdx /*tmp*/,
    bool clipBackFacing = false /*why not?*/)
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));
    DENG_ASSERT(material != 0 && material->isDrawable()); // Must have a drawable material.

    Vector3f eyeToSurface(vOrigin[VX] - bspLeaf->center().x,
                          vOrigin[VZ] - bspLeaf->center().y,
                          vOrigin[VY] - height);

    // Don't bother with planes facing away from the camera.
    if(!(clipBackFacing && !(eyeToSurface.dot(normal) < 0)))
    {
        // Set the texture origin, Y is flipped for the ceiling.
        Vector3d texTL(bspLeaf->aaBox().minX,
                       bspLeaf->aaBox().arvec2[type == Plane::Floor? 1 : 0][VY],
                       height);
        Vector3d texBR(bspLeaf->aaBox().maxX,
                       bspLeaf->aaBox().arvec2[type == Plane::Floor? 0 : 1][VY],
                       height);

        writePlane2(*bspLeaf, type, height, tangent, bitangent, normal, material,
                    tintColor, opacity, blendMode, texTL, texBR, materialOrigin, materialScale,
                    skyMasked, addDLights, addMobjShadows, bsuf, elmIdx);
    }
}

static Material *chooseSurfaceMaterialForTexturingMode(Surface const &surface)
{
    if(renderTextures == 2)
    {
        // Lighting debug mode -- use the special "gray" material.
        return &App_Materials().find(de::Uri("System", Path("gray"))).material();
    }

    if(!surface.hasMaterial() || (devNoTexFix && surface.hasFixMaterial()))
    {
        // Missing material debug mode -- use special "missing" material.
        return &App_Materials().find(de::Uri("System", Path("missing"))).material();
    }

    // Normal mode -- use the surface-bound material.
    return surface.materialPtr();
}

static float calcLightLevelDelta(Vector3f const &normal)
{
    return (1.0f / 255) * (normal.x * 18) * rendLightWallAngle;
}

static Line::Side *findSideBlendNeighbor(Line::Side const &side, bool right, binangle_t *diff = 0)
{
    LineOwner const *farVertOwner = side.line().vertexOwner(side.lineSideId() ^ (int)right);

    if(diff) *diff = 0;

    Line *neighbor;
    if(R_SideBackClosed(side))
    {
        neighbor = R_FindSolidLineNeighbor(side.sectorPtr(), &side.line(), farVertOwner, right, diff);
    }
    else
    {
        neighbor = R_FindLineNeighbor(side.sectorPtr(), &side.line(), farVertOwner, right, diff);
    }

    // No suitable line neighbor?
    if(!neighbor) return 0;

    // Return the relative side of the neighbor.
    if(&neighbor->vertex(right ^ 1) == &side.vertex(right))
        return &neighbor->front();

    return &neighbor->back();
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
 * @param side     Line side to calculate light level deltas for.
 * @param section  Section of the side for which deltas are to be calculated.
 *
 * Return values:
 * @param deltaL   Light delta for the left edge written here.
 * @param deltaR   Light delta for the right edge written here.
 *
 * @todo: Use the half-edge rings instead of LineOwners.
 */
static void wallSectionLightLevelDeltas(Line::Side &side, int section, float &deltaL, float &deltaR)
{
    deltaL = 0;
    deltaR = 0;

    // Are light level deltas disabled?
    if(rendLightWallAngle <= 0) return;
    // ...always if the surface's material was chosen as a HOM fix (lighting
    // must be consistent with that applied to the relative back sector plane).
    if(side.hasSections() && side.back().hasSections() &&
       side.surface(section).hasFixMaterial())
        return;

    deltaL = deltaR = calcLightLevelDelta(side.surface(section).normal());

    // Is delta smoothing disabled?
    if(!rendLightWallAngleSmooth) return;
    // ...always for polyobj lines (no owner rings).
    if(side.line().isFromPolyobj()) return;

    /**
     * Smoothing is enabled, so find the neighbor sides for each edge which
     * we will use to calculate the averaged lightlevel delta for each.
     *
     * @todo Do not assume the neighbor is the middle section of @a otherSide.
     */
    binangle_t angleDiff;

    if(Line::Side *otherSide = findSideBlendNeighbor(side, false /*left neighbor*/, &angleDiff))
    {
        Surface &sufA = side.surface(section);
        Surface &sufB = otherSide->surface(section);

        if(shouldSmoothLightLevels(sufA, sufB, angleDiff))
        {
            // Average normals.
            Vector3f avgNormal = Vector3f(sufA.normal() + sufB.normal()) / 2;
            deltaL = calcLightLevelDelta(avgNormal);
        }
    }

    // Do the same for the right edge but with the right neighbor side.
    if(Line::Side *otherSide = findSideBlendNeighbor(side, true /*right neighbor*/, &angleDiff))
    {
        Surface &sufA = side.surface(section);
        Surface &sufB = otherSide->surface(section);

        if(shouldSmoothLightLevels(sufA, sufB, angleDiff))
        {
            // Average normals.
            Vector3f avgNormal = Vector3f(sufA.normal() + sufB.normal()) / 2;
            deltaR = calcLightLevelDelta(avgNormal);
        }
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
#define WSF_VIEWER_NEAR_BLEND   0x08 ///< Alpha-blend geometry when viewer is near.
#define WSF_FORCE_OPAQUE        0x10 ///< Force the geometry to be opaque.
///@}

/**
 * @param flags  @ref writeWallSectionFlags
 */
static bool writeWallSection(HEdge &hedge, int section,
    int flags, walldivs_t *leftWallDivs, walldivs_t *rightWallDivs,
    Vector2f const &materialOrigin)
{
    Surface &surface = hedge.lineSide().surface(section);

    // Surfaces without a drawable material are never rendered.
    if(!(surface.hasMaterial() && surface.material().isDrawable()))
        return false;

    if(WallDivNode_Height(WallDivs_First(leftWallDivs)) >=
       WallDivNode_Height(WallDivs_Last(rightWallDivs))) return true;

    bool opaque = true;
    float opacity = (section == Line::Side::Middle? surface.opacity() : 1.0f);

    if(section == Line::Side::Middle && (flags & WSF_VIEWER_NEAR_BLEND))
    {
        mobj_t *mo = viewPlayer->shared.mo;
        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

        /*
         * Can the player walk through this surface?
         * If the player is close enough we should NOT add a solid hedge otherwise HOM
         * would occur when they are directly on top of the line (e.g., passing through
         * an opaque waterfall).
         */

        if(viewData->current.origin[VZ] >  WallDivNode_Height(WallDivs_First(leftWallDivs)) &&
           viewData->current.origin[VZ] < WallDivNode_Height(WallDivs_Last(rightWallDivs)))
        {
            Line const &line = hedge.line();

            coord_t linePoint[2]     = { line.fromOrigin().x, line.fromOrigin().y };
            coord_t lineDirection[2] = { line.direction().x, line.direction().y };

            vec2d_t result;
            double pos = V2d_ProjectOnLine(result, mo->origin, linePoint, lineDirection);

            if(pos > 0 && pos < 1)
            {
                coord_t const minDistance = mo->radius * .8f;

                coord_t delta[2]; V2d_Subtract(delta, mo->origin, result);
                coord_t distance = M_ApproxDistance(delta[VX], delta[VY]);

                if(distance < minDistance)
                {
                    // Fade it out the closer the viewPlayer gets and clamp.
                    opacity = (opacity / minDistance) * distance;
                    opacity = de::clamp(0.f, opacity, 1.f);
                }

                if(opacity < 1)
                    opaque = false;
            }
        }
    }

    if(opacity > 0)
    {
        bool const isTwoSided = (hedge.hasLineSide() && hedge.line().hasFrontSections() && hedge.line().hasBackSections())? true:false;

        Vector2f materialScale((surface.flags() & DDSUF_MATERIAL_FLIPH)? -1 : 1,
                               (surface.flags() & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        Vector3d texTL(hedge.fromOrigin(),
                       WallDivNode_Height(WallDivs_Last(leftWallDivs)));

        Vector3d texBR(hedge.toOrigin(),
                       WallDivNode_Height(WallDivs_First(rightWallDivs)));

        // Determine which Material to use.
        Material *material = 0;
        int rpFlags = RPF_DEFAULT; /// @ref rendPolyFlags
        /// @todo Geometry tests here should use the same sectors as used when
        /// deciding the z-heights for the wall divs. At least in the case of
        /// selfreferencing lines, this logic is probably incorrect. -ds
        if(devRendSkyMode && hedge.hasTwin() &&
           ((section == Line::Side::Bottom && hedge.bspLeafSector().floorSurface().hasSkyMaskedMaterial() &&
                                              hedge.twin().bspLeafSector().floorSurface().hasSkyMaskedMaterial()) ||
            (section == Line::Side::Top    && hedge.bspLeafSector().ceilingSurface().hasSkyMaskedMaterial() &&
                                              hedge.twin().bspLeafSector().ceilingSurface().hasSkyMaskedMaterial())))
        {
            // Geometry not normally rendered however we do so in dev sky mode.
            material = hedge.bspLeafSector().planeSurface(section == Line::Side::Top? Plane::Ceiling : Plane::Floor).materialPtr();
        }
        else
        {
            material = chooseSurfaceMaterialForTexturingMode(surface);

            if(material->isSkyMasked())
            {
                if(!devRendSkyMode)
                {
                    // We'll mask this.
                    rpFlags |= RPF_SKYMASK;
                }
                else
                {
                    // In dev sky mode we render all would-be skymask geometry
                    // as if it were non-skymask.
                    flags |= WSF_FORCE_OPAQUE;
                }
            }
        }

        MaterialSnapshot const &ms = material->prepare(mapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT));

        // Fill in the remaining params data.
        Vector3f const *topColor = 0, *bottomColor = 0;
        blendmode_t blendMode = BM_NORMAL;
        uint lightListIdx = 0, shadowListIdx = 0;
        if(!(rpFlags & RPF_SKYMASK))
        {
            // Make any necessary adjustments to the draw flags to suit the
            // current texture mode.
            if(section != Line::Side::Middle || (section == Line::Side::Middle && !isTwoSided))
            {
                flags |= WSF_FORCE_OPAQUE;
                blendMode = BM_NORMAL;
            }
            else
            {
                if(surface.blendMode() == BM_NORMAL && noSpriteTrans)
                    blendMode = BM_ZEROALPHA; // "no translucency" mode
                else
                    blendMode = surface.blendMode();
            }

            // Polyobj surfaces never shadow.
            if(hedge.line().isFromPolyobj())
                flags &= ~WSF_ADD_RADIO;

            float glowStrength = 0;
            if(glowFactor > .0001f)
                glowStrength = ms.glowStrength() * glowFactor; // Global scale factor.

            // Dynamic Lights?
            if(flags & WSF_ADD_DYNLIGHTS)
            {
                lightListIdx = projectSurfaceLights(hedge.lineSide().middle(), glowStrength, texTL, texBR,
                                                    (section == Line::Side::Middle && isTwoSided));
            }

            // Dynamic shadows?
            if(flags & WSF_ADD_DYNSHADOWS)
            {
                shadowListIdx = projectSurfaceShadows(hedge.lineSide().middle(), glowStrength, texTL, texBR);
            }

            if(glowStrength > 0)
                flags &= ~WSF_ADD_RADIO;

            hedge.lineSide().chooseSurfaceTintColors(section, &topColor, &bottomColor);
        }

        float deltaL, deltaR;
        wallSectionLightLevelDeltas(hedge.lineSide(), section, deltaL, deltaR);

        if(!de::fequal(deltaL, deltaR))
        {
            // Linearly interpolate to find the light level delta values for the
            // vertical edges of this wall section.
            coord_t const lineLength = hedge.line().length();
            coord_t const sectionLength = hedge.length();
            coord_t const sectionOffset = hedge.lineOffset();

            float deltaDiff = deltaR - deltaL;
            deltaR = deltaL + ((sectionOffset + sectionLength) / lineLength) * deltaDiff;
            deltaL += (sectionOffset / lineLength) * deltaDiff;
        }

        if(section == Line::Side::Middle && isTwoSided)
        {
            // Temporarily modify the draw state.
            currentSectorLightColor = R_GetSectorLightColor(hedge.lineSide().sector());
            currentSectorLightLevel = hedge.lineSide().sector().lightLevel();
        }

        opaque = writeWallSection2(hedge,
                                   surface.normal(), ((flags & WSF_FORCE_OPAQUE)? -1 : opacity),
                                   deltaL, deltaR,
                                   lightListIdx, shadowListIdx,
                                   leftWallDivs, rightWallDivs,
                                   (rpFlags & RPF_SKYMASK) != 0, (flags & WSF_ADD_RADIO) != 0,
                                   texTL, texBR, materialOrigin, materialScale, blendMode,
                                   *topColor, *bottomColor,
                                   &hedge.biasSurfaceForGeometryGroup(section), section,
                                   ms,
                                   (section == Line::Side::Middle && isTwoSided));

        if(section == Line::Side::Middle && isTwoSided)
        {
            // Undo temporary draw state changes.
            currentSectorLightColor = R_GetSectorLightColor(currentBspLeaf->sector());
            currentSectorLightLevel = currentBspLeaf->sector().lightLevel();
        }
    }

    return opaque;
}

static void reportLineDrawn(Line &line)
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

/**
 * Prepare and write wall sections for a "one-sided" line to the render lists.
 *
 * @param hedge  HEdge to draw wall sections for.
 * @param sections  @ref sideSectionFlags
 */
static bool writeWallSections2(HEdge &hedge, int sections)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    DENG_ASSERT(hedge.hasLineSide());
    DENG_ASSERT(hedge.lineSide().hasSections());
    DENG_ASSERT(hedge._frameFlags & HEDGEINF_FACINGFRONT);

    // Only a "middle" section.
    if(!(sections & Line::Side::MiddleFlag)) return false;

    walldivs_t leftWallDivs, rightWallDivs;
    Vector2f materialOrigin;
    bool opaque = false;

    if(hedge.prepareWallDivs(Line::Side::Middle, &leftWallDivs, &rightWallDivs, &materialOrigin))
    {
        Rend_RadioUpdateForLineSide(hedge.lineSide());

        int wsFlags = WSF_ADD_DYNLIGHTS|WSF_ADD_DYNSHADOWS|WSF_ADD_RADIO;

        opaque = writeWallSection(hedge, Line::Side::Middle, wsFlags,
                                  &leftWallDivs, &rightWallDivs, materialOrigin);
    }

    reportLineDrawn(hedge.line());
    return opaque;
}

static bool prepareWallDivsPolyobj(Line::Side const &side, int section,
    walldivs_t *leftWallDivs, walldivs_t *rightWallDivs, Vector2f *materialOrigin = 0)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    coord_t bottom, top;
    bool visible = R_SideSectionCoords(side, section, leaf->sectorPtr(), 0,
                                       &bottom, &top, materialOrigin);

    if(!visible) return false;

    WallDivs_Append(leftWallDivs, bottom); // First node.
    WallDivs_Append(leftWallDivs, top); // Last node.

    WallDivs_Append(rightWallDivs, bottom); // First node.
    WallDivs_Append(rightWallDivs, top); // Last node.

    return true;
}

static bool writeWallSections2Polyobj(HEdge &hedge, int sections)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    DENG_ASSERT(hedge.hasLineSide() && hedge.lineSide().hasSections());
    DENG_ASSERT(hedge.line().isFromPolyobj());
    DENG_ASSERT(hedge._frameFlags & HEDGEINF_FACINGFRONT);

    // Only a "middle" section.
    if(!(sections & Line::Side::MiddleFlag)) return false;

    walldivs_t leftWallDivs, rightWallDivs;
    Vector2f materialOrigin;
    bool opaque = false;

    if(prepareWallDivsPolyobj(hedge.lineSide(), Line::Side::Middle,
                              &leftWallDivs, &rightWallDivs, &materialOrigin))
    {
        Rend_RadioUpdateForLineSide(hedge.lineSide());

        int wsFlags = WSF_ADD_DYNLIGHTS|WSF_ADD_DYNSHADOWS|WSF_ADD_RADIO;

        opaque = writeWallSection(hedge, Line::Side::Middle, wsFlags,
                                  &leftWallDivs, &rightWallDivs, materialOrigin);
    }

    reportLineDrawn(hedge.line());
    return opaque;
}

/**
 * Prepare and write wall sections for a "two-sided" line to the render lists.
 *
 * @param hedge  HEdge to draw wall sections for.
 * @param sections  @ref sideSectionFlags
 */
static bool writeWallSections2Twosided(HEdge &hedge, int sections)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    DENG_ASSERT(hedge.hasLineSide() && hedge.hasTwin());
    DENG_ASSERT(hedge.lineSide().hasSections());
    DENG_ASSERT(hedge._frameFlags & HEDGEINF_FACINGFRONT);

    Line &line = hedge.line();

    reportLineDrawn(line);

    Line::Side &front  = hedge.lineSide();
    Line::Side &back   = hedge.twin().lineSide();

    /// @todo Is this now redundant? -ds
    if(back.sectorPtr() == front.sectorPtr() &&
       !front.top().hasMaterial() && !front.bottom().hasMaterial() &&
       !front.middle().hasMaterial())
       return false; // Ugh... an obvious wall hedge hack. Best take no chances...

    Plane &ffloor = leaf->sector().floor();
    Plane &fceil  = leaf->sector().ceiling();
    Plane &bfloor = back.sector().floor();
    Plane &bceil  = back.sector().ceiling();

    bool opaque = false;

    // Middle section?
    if(sections & Line::Side::MiddleFlag)
    {
        walldivs_t leftWallDivs, rightWallDivs;
        Vector2f materialOrigin;

        if(hedge.prepareWallDivs(Line::Side::Middle, &leftWallDivs, &rightWallDivs, &materialOrigin))
        {
            int rhFlags = WSF_ADD_DYNLIGHTS|WSF_ADD_DYNSHADOWS|WSF_ADD_RADIO;

            if((viewPlayer->shared.flags & (DDPF_NOCLIP|DDPF_CAMERA)) ||
               !(line.isFlagged(DDLF_BLOCKING)))
                rhFlags |= WSF_VIEWER_NEAR_BLEND;

            Rend_RadioUpdateForLineSide(hedge.lineSide());

            opaque = writeWallSection(hedge, Line::Side::Middle, rhFlags,
                                      &leftWallDivs, &rightWallDivs, materialOrigin);
            if(opaque)
            {
                Surface &surface = front.middle();
                coord_t xbottom, xtop;

                if(line.isSelfReferencing())
                {
                    xbottom = de::min(bfloor.visHeight(), ffloor.visHeight());
                    xtop    = de::max(bceil.visHeight(),  fceil.visHeight());
                }
                else
                {
                    xbottom = de::max(bfloor.visHeight(), ffloor.visHeight());
                    xtop    = de::min(bceil.visHeight(),  fceil.visHeight());
                }

                xbottom += surface.visMaterialOrigin()[VY];
                xtop    += surface.visMaterialOrigin()[VY];

                // Can we make this a solid segment?
                if(!(WallDivNode_Height(WallDivs_Last(&rightWallDivs)) >= xtop &&
                     WallDivNode_Height(WallDivs_First(&leftWallDivs)) <= xbottom))
                     opaque = false;
            }
        }
    }

    // Upper section?
    if(sections & Line::Side::TopFlag)
    {
        walldivs_t leftWallDivs, rightWallDivs;
        Vector2f materialOrigin;

        if(hedge.prepareWallDivs(Line::Side::Top, &leftWallDivs, &rightWallDivs, &materialOrigin))
        {
            Rend_RadioUpdateForLineSide(hedge.lineSide());

            writeWallSection(hedge, Line::Side::Top, WSF_ADD_DYNLIGHTS|WSF_ADD_DYNSHADOWS|WSF_ADD_RADIO,
                             &leftWallDivs, &rightWallDivs, materialOrigin);
        }
    }

    // Lower section?
    if(sections & Line::Side::BottomFlag)
    {
        walldivs_t leftWallDivs, rightWallDivs;
        Vector2f materialOrigin;

        if(hedge.prepareWallDivs(Line::Side::Bottom, &leftWallDivs, &rightWallDivs, &materialOrigin))
        {
            Rend_RadioUpdateForLineSide(hedge.lineSide());

            writeWallSection(hedge, Line::Side::Bottom, WSF_ADD_DYNLIGHTS|WSF_ADD_DYNSHADOWS|WSF_ADD_RADIO,
                             &leftWallDivs, &rightWallDivs, materialOrigin);
        }
    }

    if(line.isSelfReferencing())
       return false;

    if(!opaque) // We'll have to determine whether we can...
    {
        if(   (bceil.visHeight() <= ffloor.visHeight() &&
                   (front.top().hasMaterial()    || front.middle().hasMaterial()))
           || (bfloor.visHeight() >= fceil.visHeight() &&
                   (front.bottom().hasMaterial() || front.middle().hasMaterial())))
        {
            // A closed gap?
            if(de::fequal(fceil.visHeight(), bfloor.visHeight()))
            {
                opaque = (bceil.visHeight() <= bfloor.visHeight()) ||
                           !(fceil.surface().hasSkyMaskedMaterial() &&
                             bceil.surface().hasSkyMaskedMaterial());
            }
            else if(de::fequal(ffloor.visHeight(), bceil.visHeight()))
            {
                opaque = (bfloor.visHeight() >= bceil.visHeight()) ||
                           !(ffloor.surface().hasSkyMaskedMaterial() &&
                             bfloor.surface().hasSkyMaskedMaterial());
            }
            else
            {
                opaque = true;
            }
        }
        /// @todo Is this still necessary?
        else if(bceil.visHeight() <= bfloor.visHeight() ||
                (!(bceil.visHeight() - bfloor.visHeight() > 0) && bfloor.visHeight() > ffloor.visHeight() && bceil.visHeight() < fceil.visHeight() &&
                front.top().hasMaterial() && front.bottom().hasMaterial()))
        {
            // A zero height back segment
            opaque = true;
        }
    }

    return opaque;
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
                // Which way should it be facing?
                if(!(viewFacingDot(hedge->fromOrigin(), hedge->toOrigin()) < 0))
                    hedge->_frameFlags |= HEDGEINF_FACINGFRONT;
                else
                    hedge->_frameFlags &= ~HEDGEINF_FACINGFRONT;
            }
        } while((hedge = &hedge->next()) != base);
    }

    if(Polyobj *po = bspLeaf->firstPolyobj())
    {
        foreach(Line *line, po->lines())
        {
            HEdge *hedge = line->front().leftHEdge();

            // Which way should it be facing?
            if(!(viewFacingDot(hedge->fromOrigin(), hedge->toOrigin()) < 0))
                hedge->_frameFlags |= HEDGEINF_FACINGFRONT;
            else
                hedge->_frameFlags &= ~HEDGEINF_FACINGFRONT;
        }
    }
}

/// If @a hedge is @em not front facing this is no-op.
static inline void occludeFrontFacingHEdge(HEdge &hedge)
{
    if(hedge._frameFlags & HEDGEINF_FACINGFRONT)
    {
        if(!C_CheckRangeFromViewRelPoints(hedge.fromOrigin(), hedge.toOrigin()))
        {
            hedge._frameFlags &= ~HEDGEINF_FACINGFRONT;
        }
    }
}

static void occludeFrontFacingHEdges()
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
                occludeFrontFacingHEdge(*hedge);
            }
        } while((hedge = &hedge->next()) != base);
    }

    if(Polyobj *po = bspLeaf->firstPolyobj())
    {
        foreach(Line *line, po->lines())
        {
            occludeFrontFacingHEdge(*line->front().leftHEdge());
        }
    }
}

#endif // __CLIENT__

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
        // Map RTU configuration from the prepared MaterialSnapshot.
        MaterialSnapshot const &ms = material->prepare(mapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT));

        RL_LoadDefaultRtus();
        RL_MapRtu(RTU_PRIMARY, &ms.unit(RTU_PRIMARY));
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
            if(devRendSkyMode)
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
                                  (devRendSkyMode && skyMaterial != startMaterial)))
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
    buildLeafPlaneGeometry(*bspLeaf, (skyCap & SKYCAP_UPPER) != 0, R_SkyCapZ(bspLeaf, skyCap),
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
    if(!R_SectorContainsSkySurfaces(bspLeaf->sectorPtr()))
        return;

    // Sky caps are only necessary in sectors with sky-masked planes.
    if((skyCap & SKYCAP_LOWER) && !bspLeaf->sector().floorSurface().hasSkyMaskedMaterial())
        skyCap &= ~SKYCAP_LOWER;
    if((skyCap & SKYCAP_UPPER) && !bspLeaf->sector().ceilingSurface().hasSkyMaskedMaterial())
        skyCap &= ~SKYCAP_UPPER;

    if(!skyCap) return;

    if(!devRendSkyMode)
    {
        // All geometry uses the same RTU write state.
        RL_LoadDefaultRtus();
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

static void writeWallSections(HEdge &hedge)
{
    DENG_ASSERT(hedge.hasLineSide());
    DENG_ASSERT(hedge.lineSide().hasSections());
    DENG_ASSERT(hedge._frameFlags & HEDGEINF_FACINGFRONT);

    int const pvisSections = pvisibleWallSections(hedge.lineSide()); /// @ref sideSectionFlags
    bool opaque;

    if(!hedge.hasTwin() || !hedge.twin().bspLeafSectorPtr() ||
       /* solid side of a "one-way window"? */
       !(hedge.twin().hasLineSide() && hedge.twin().lineSide().hasSections()))
    {
        // One-sided.
        opaque = writeWallSections2(hedge, pvisSections);
    }
    else
    {
        // Two-sided.
        opaque = writeWallSections2Twosided(hedge, pvisSections);
    }

    // We can occlude the wall range if the opening is filled (when the viewer is not in the void).
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
        if(hedge->_frameFlags & HEDGEINF_FACINGFRONT)
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
        if(hedge->_frameFlags & HEDGEINF_FACINGFRONT)
        {
            bool opaque = writeWallSections2Polyobj(*hedge, Line::Side::MiddleFlag);

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
        Surface const *surface = &plane->surface();

        bool isSkyMasked = surface->hasSkyMaskedMaterial();
        if(isSkyMasked && plane->type() != Plane::Middle)
        {
            if(!devRendSkyMode) continue; // Not handled here.
            isSkyMasked = false;
        }

        Material *mat = chooseSurfaceMaterialForTexturingMode(*surface);

        Vector2f materialOrigin = surface->visMaterialOrigin();
        // Add the Y offset to orient the Y flipped texture.
        if(plane->type() == Plane::Ceiling)
            materialOrigin.y -= leaf->aaBox().maxY - leaf->aaBox().minY;
        // Add the additional offset to align with the worldwide grid.
        materialOrigin += leaf->worldGridOffset();
        // Inverted.
        materialOrigin.y = -materialOrigin.y;

        Vector2f materialScale((surface->flags() & DDSUF_MATERIAL_FLIPH)? -1 : 1,
                               (surface->flags() & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        if(mat && mat->isDrawable())
        {
            writePlane(plane->type(), plane->visHeight(),
                       surface->tangent(), surface->bitangent(), surface->normal(),
                       mat, surface->tintColor(), surface->opacity(), surface->blendMode(),
                       materialOrigin, materialScale,
                       isSkyMasked, !devRendSkyMode, (!devRendSkyMode && plane->type() == Plane::Floor),
                       &leaf->biasSurfaceForGeometryGroup(plane->inSectorIndex()),
                       plane->inSectorIndex());
        }
    }
}

/**
 * Creates new occlusion planes from the BspLeaf's edges.
 * Before testing, occlude the BspLeaf's backfaces. After testing occlude
 * the remaining faces, i.e. the forward facing edges. This is done before
 * rendering edges, so solid segments cut out all unnecessary oranges.
 */
static void occludeBspLeaf(BspLeaf const *bspLeaf, bool forwardFacing)
{
    if(devNoCulling) return;

    if(!bspLeaf || !bspLeaf->firstHEdge()) return;

    if(P_IsInVoid(viewPlayer)) return;

    Sector *frontSec     = bspLeaf->sectorPtr();
    coord_t const fFloor = frontSec->floor().height();
    coord_t const fCeil  = frontSec->ceiling().height();

    HEdge *base = bspLeaf->firstHEdge();
    HEdge *hedge = base;
    do
    {
        // Occlusions can only happen where two sectors contact.
        if(hedge->hasLineSide() && hedge->hasTwin() && hedge->twin().bspLeaf().hasSector() &&
           (forwardFacing == ((hedge->_frameFlags & HEDGEINF_FACINGFRONT)? true : false)))
        {
            Sector *backSec      = hedge->twin().bspLeafSectorPtr();
            coord_t const bFloor = backSec->floor().height();
            coord_t const bCeil  = backSec->ceiling().height();

            // Choose start and end vertices so that it's facing forward.
            Vertex const &startv = hedge->vertex(forwardFacing^1);
            Vertex const &endv   = hedge->vertex(forwardFacing  );

            // Do not create an occlusion for sky floors.
            if(!backSec->floorSurface().hasSkyMaskedMaterial() ||
               !frontSec->floorSurface().hasSkyMaskedMaterial())
            {
                // Do the floors create an occlusion?
                if((bFloor > fFloor && vOrigin[VY] <= bFloor) ||
                   (bFloor < fFloor && vOrigin[VY] >= fFloor))
                {
                    // Occlude down.
                    C_AddViewRelOcclusion(startv.origin(), endv.origin(),
                                          de::max(fFloor, bFloor), false);
                }
            }

            // Do not create an occlusion for sky ceilings.
            if(!backSec->ceilingSurface().hasSkyMaskedMaterial() ||
               !frontSec->ceilingSurface().hasSkyMaskedMaterial())
            {
                // Do the ceilings create an occlusion?
                if((bCeil < fCeil && vOrigin[VY] >= bCeil) ||
                   (bCeil > fCeil && vOrigin[VY] <= fCeil))
                {
                    // Occlude up.
                    C_AddViewRelOcclusion(startv.origin(), endv.origin(),
                                          de::min(fCeil, bCeil), true);
                }
            }
        }
    } while((hedge = &hedge->next()) != base);
}

/**
 * @pre Assumes the leaf is at least partially visible.
 */
static void drawCurrentBspLeaf()
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));

    // Mark the sector visible for this frame.
    bspLeaf->sector()._frameFlags |= SIF_VISIBLE;

    markFrontFacingHEdges();
    R_InitForBspLeaf(bspLeaf);
    Rend_RadioBspLeafEdges(*bspLeaf);

    occludeBspLeaf(bspLeaf, false);
    LO_ClipInBspLeaf(bspLeaf->indexInMap());
    occludeBspLeaf(bspLeaf, true);

    occludeFrontFacingHEdges();

    if(bspLeaf->hasPolyobj())
    {
        // Polyobjs don't obstruct - clip lights with another algorithm.
        LO_ClipInBspLeafBySight(bspLeaf->indexInMap());
    }

    // Mark particle generators in the sector visible.
    Rend_ParticleMarkInSectorVisible(bspLeaf->sectorPtr());

    /*
     * Sprites for this BSP leaf have to be drawn.
     *
     * Must be done BEFORE the segments of this BspLeaf are added to the clipper.
     * Otherwise the sprites would get clipped by them, and that wouldn't be right.
     *
     * Must be done AFTER the lumobjs have been clipped as this affects the projection
     * of halos.
     */
    R_AddSprites(bspLeaf);

    writeLeafSkyMask();
    writeLeafWallSections();
    writeLeafPolyobjs();
    writeLeafPlanes();
}

/**
 * Change the current BspLeaf (updating any relevant draw state properties
 * accordingly).
 *
 * @param bspLeaf  The new BSP leaf to make current. Cannot be @c 0.
 */
static void makeCurrent(BspLeaf *bspLeaf)
{
    DENG_ASSERT(bspLeaf != 0);
    bool sectorChanged = (!currentBspLeaf || currentBspLeaf->sectorPtr() != bspLeaf->sectorPtr());

    currentBspLeaf = bspLeaf;

    // Update draw state.
    if(sectorChanged)
    {
        currentSectorLightColor = R_GetSectorLightColor(bspLeaf->sector());
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

    drawCurrentBspLeaf();

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

#endif // __CLIENT__
