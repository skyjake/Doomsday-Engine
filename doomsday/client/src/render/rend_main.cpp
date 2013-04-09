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
#include "render/sprite.h"
#include "gl/sys_opengl.h"

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
#define SOF_SIDEDEF             0x04
///@}

void Rend_DrawBBox(coord_t const pos[3], coord_t w, coord_t l, coord_t h, float a,
    float const color[3], float alpha, float br, boolean alignToBase);

void Rend_DrawArrow(coord_t const pos[3], float a, float s, float const color3f[3], float alpha);

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
static uint Rend_BuildBspLeafPlaneGeometry(BspLeaf *leaf, boolean antiClockwise,
                                           coord_t height, rvertex_t **verts, uint *vertsSize);

static BspLeaf *currentBspLeaf; // BSP leaf currently being drawn.
static boolean firstBspLeaf; // No range checking for the first one.
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

/**
 * Approximated! The Z axis aspect ratio is corrected.
 */
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

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

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

static inline double viewFacingDot(const_pvec2d_t v1, const_pvec2d_t v2)
{
    // The dot product.
    return (v1[VY] - v2[VY]) * (v1[VX] - vOrigin[VX]) + (v2[VX] - v1[VX]) * (v1[VY] - vOrigin[VZ]);
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
static byte pvisibleLineSections(LineDef *line, int backSide)
{
    byte sections = 0;

    if(!line || !line->hasSideDef(backSide)) return 0;

    if(!line->hasSector(backSide^1) /*$degenleaf*/ || !line->hasBackSideDef())
    {
        // Only a middle.
        sections |= SSF_MIDDLE;
    }
    else
    {
        SideDef const &sideDef = line->sideDef(backSide);
        Sector const *fsec  = line->sectorPtr(backSide);
        Sector const *bsec  = line->sectorPtr(backSide^1);
        Plane const *fceil  = &fsec->ceiling();
        Plane const *ffloor = &fsec->floor();
        Plane const *bceil  = &bsec->ceiling();
        Plane const *bfloor = &bsec->floor();

        sections |= SSF_MIDDLE | SSF_BOTTOM | SSF_TOP;

        // Middle?
        if(!sideDef.middle().hasMaterial() || !sideDef.middle().material().isDrawable() ||
            sideDef.middle().opacity() <= 0)
            sections &= ~SSF_MIDDLE;

        // Top?
        if((!devRendSkyMode && fceil->surface().hasSkyMaskedMaterial() && bceil->surface().hasSkyMaskedMaterial()) ||
           //(!devRendSkyMode && bceil->surface().isSkyMasked() && (sideDef.top().hasFixMaterial())) ||
           (fceil->visHeight() <= bceil->visHeight()))
            sections &= ~SSF_TOP;

        // Bottom?
        if((!devRendSkyMode && ffloor->surface().hasSkyMaskedMaterial() && bfloor->surface().hasSkyMaskedMaterial()) ||
           //(!devRendSkyMode && bfloor->surface().isSkyMasked() && (sideDef.bottom().hasFixMaterial())) ||
           (ffloor->visHeight() >= bfloor->visHeight()))
            sections &= ~SSF_BOTTOM;
    }

    return sections;
}

static void selectSurfaceColors(Vector3f const **topColor,
    Vector3f const **bottomColor, SideDef *sideDef, SideDefSection section)
{
    switch(section)
    {
    case SS_MIDDLE:
        if(sideDef->flags() & SDF_BLENDMIDTOTOP)
        {
            *topColor    = &sideDef->top().tintColor();
            *bottomColor = &sideDef->middle().tintColor();
        }
        else if(sideDef->flags() & SDF_BLENDMIDTOBOTTOM)
        {
            *topColor    = &sideDef->middle().tintColor();
            *bottomColor = &sideDef->bottom().tintColor();
        }
        else
        {
            *topColor    = &sideDef->middle().tintColor();
            *bottomColor = 0;
        }
        break;

    case SS_TOP:
        if(sideDef->flags() & SDF_BLENDTOPTOMID)
        {
            *topColor    = &sideDef->top().tintColor();
            *bottomColor = &sideDef->middle().tintColor();
        }
        else
        {
            *topColor    = &sideDef->top().tintColor();
            *bottomColor = 0;
        }
        break;

    case SS_BOTTOM:
        if(sideDef->flags() & SDF_BLENDBOTTOMTOMID)
        {
            *topColor    = &sideDef->middle().tintColor();
            *bottomColor = &sideDef->bottom().tintColor();
        }
        else
        {
            *topColor    = &sideDef->bottom().tintColor();
            *bottomColor = 0;
        }
        break;

    default: break;
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

#ifdef __CLIENT__

/**
 * This doesn't create a rendering primitive but a vissprite! The vissprite
 * represents the masked poly and will be rendered during the rendering
 * of sprites. This is necessary because all masked polygons must be
 * rendered back-to-front, or there will be alpha artifacts along edges.
 */
void Rend_AddMaskedPoly(rvertex_t const *rvertices, ColorRawf const *rcolors,
    coord_t wallLength, MaterialVariant *material, float const texOffset[2],
    blendmode_t blendMode, uint lightListIdx, float glow)
{
    vissprite_t *vis = R_NewVisSprite();
    int i, c;

    vis->type = VSPR_MASKED_WALL;
    vis->origin[VX] = (rvertices[0].pos[VX] + rvertices[3].pos[VX]) / 2;
    vis->origin[VY] = (rvertices[0].pos[VY] + rvertices[3].pos[VY]) / 2;
    vis->origin[VZ] = (rvertices[0].pos[VZ] + rvertices[3].pos[VZ]) / 2;
    vis->distance = Rend_PointDist2D(vis->origin);

    if(texOffset)
    {
        VS_WALL(vis)->texOffset[0] = texOffset[VX];
        VS_WALL(vis)->texOffset[1] = texOffset[VY];
    }

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

    for(i = 0; i < 4; ++i)
    {
        VS_WALL(vis)->vertices[i].pos[VX] = rvertices[i].pos[VX];
        VS_WALL(vis)->vertices[i].pos[VY] = rvertices[i].pos[VY];
        VS_WALL(vis)->vertices[i].pos[VZ] = rvertices[i].pos[VZ];

        for(c = 0; c < 4; ++c)
        {
            /// @todo Do not clamp here.
            VS_WALL(vis)->vertices[i].color[c] = MINMAX_OF(0, rcolors[i].rgba[c], 1);
        }
    }

    /// @todo Semitransparent masked polys arn't lit atm
    if(glow < 1 && lightListIdx && numTexUnits > 1 && envModAdd &&
       !(rcolors[0].rgba[CA] < 1))
    {
        dynlight_t const *dyn = NULL;

        /**
         * The dynlights will have already been sorted so that the brightest
         * and largest of them is first in the list. So grab that one.
         */
        LO_IterateProjections2(lightListIdx, RIT_FirstDynlightIterator, (void*)&dyn);

        VS_WALL(vis)->modTex = dyn->texture;
        VS_WALL(vis)->modTexCoord[0][0] = dyn->s[0];
        VS_WALL(vis)->modTexCoord[0][1] = dyn->s[1];
        VS_WALL(vis)->modTexCoord[1][0] = dyn->t[0];
        VS_WALL(vis)->modTexCoord[1][1] = dyn->t[1];
        for(c = 0; c < 4; ++c)
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
    coord_t wallLength, coord_t const topLeft[3])
{
    tc[0].st[0] = tc[1].st[0] = rverts[0].pos[VX] - topLeft[VX];
    tc[3].st[1] = tc[1].st[1] = rverts[0].pos[VY] - topLeft[VY];
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
    boolean         isWall;
    int             flags; /// @ref rendpolyFlags
    blendmode_t     blendMode;
    pvec3d_t        texTL, texBR;
    float const    *texOffset;
    float const    *texScale;
    Vector3f const *normal; // Surface normal.
    float           alpha;
    float           sectorLightLevel;
    float           surfaceLightLevelDL;
    float           surfaceLightLevelDR;
    Vector3f const *sectorLightColor;
    Vector3f const *surfaceColor;

    uint            lightListIdx; // List of lights that affect this poly.
    uint            shadowListIdx; // List of shadows that affect this poly.
    float           glowing;
    boolean         forceOpaque;

// For bias:
    de::MapElement *mapElement;
    uint            elmIdx;
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
    DENG_ASSERT(rvertices);

    uint const realNumVertices = ((p.isWall && (p.wall.left.divCount || p.wall.right.divCount))? 3 + p.wall.left.divCount + 3 + p.wall.right.divCount : numVertices);

    bool const skyMaskedMaterial = ((p.flags & RPF_SKYMASK) || (ms.material().isSkyMasked()));
    bool const drawAsVisSprite   = (!p.forceOpaque && !(p.flags & RPF_SKYMASK) && (!ms.isOpaque() || p.alpha < 1 || p.blendMode > 0));

    boolean useLights = false, useShadows = false, hasDynlights = false;

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
                LO_IterateProjections2(p.lightListIdx, RIT_FirstDynlightIterator, (void *)&dyn);

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
        quadTexCoords(primaryCoords, rvertices, p.wall.segLength, p.texTL);

        // Blend texture coordinates.
        if(interRTU && !drawAsVisSprite)
            quadTexCoords(interCoords, rvertices, p.wall.segLength, p.texTL);

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
                vtx.pos[VX] - float(p.texTL[VX]),
                vtx.pos[VY] - float(p.texTL[VY]),
                vtx.pos[VZ] - float(p.texTL[VZ])
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
                float const width  = p.texBR[VX] - p.texTL[VX];
                float const height = p.texBR[VY] - p.texTL[VY];

                tc.st[0] = ((p.texBR[VX] - vtx.pos[VX]) / width  * modTexSt[0][0]) + (xyz[VX] / width  * modTexSt[0][1]);
                tc.st[1] = ((p.texBR[VY] - vtx.pos[VY]) / height * modTexSt[1][0]) + (xyz[VY] / height * modTexSt[1][1]);
            }
        }
    }

    // Light this polygon.
    if(!skyMaskedMaterial)
    {
        if(levelFullBright || !(p.glowing < 1))
        {
            // Uniform color. Apply to all vertices.
            float glowStrength = p.sectorLightLevel + (levelFullBright? 1 : p.glowing);
            Rend_VertexColorsGlow(rcolors, numVertices, glowStrength);
        }
        else
        {
            // Non-uniform color.
            if(useBias && p.bsuf)
            {
                // Do BIAS lighting for this poly.
                vec3f_t v1Normal; V3f_Set(v1Normal, p.normal->x, p.normal->y, p.normal->z);
                SB_RendPoly(rcolors, p.bsuf, rvertices, numVertices, v1Normal,
                            p.sectorLightLevel, p.mapElement, p.elmIdx);

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
                float llL = de::clamp(0.f, p.sectorLightLevel + p.surfaceLightLevelDL + p.glowing, 1.f);
                float llR = de::clamp(0.f, p.sectorLightLevel + p.surfaceLightLevelDR + p.glowing, 1.f);

                // Calculate the color for each vertex, blended with plane color?
                if(p.surfaceColor->x < 1 || p.surfaceColor->y < 1 || p.surfaceColor->z < 1)
                {
                    // Blend sector light+color+surfacecolor
                    Vector3f vColor;

                    for(int c = 0; c < 3; ++c)
                        vColor[c] = (*p.surfaceColor)[c] * (*p.sectorLightColor)[c];

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
                        lightVertex(rcolors[0], rvertices[0], llL, *p.sectorLightColor);
                        lightVertex(rcolors[1], rvertices[1], llL, *p.sectorLightColor);
                        lightVertex(rcolors[2], rvertices[2], llR, *p.sectorLightColor);
                        lightVertex(rcolors[3], rvertices[3], llR, *p.sectorLightColor);
                    }
                    else
                    {
                        lightVertices(numVertices, rcolors, rvertices, llL, *p.sectorLightColor);
                    }
                }

                // Bottom color (if different from top)?
                if(p.isWall && p.wall.surfaceColor2)
                {
                    // Blend sector light+color+surfacecolor
                    Vector3f vColor;

                    for(int c = 0; c < 3; ++c)
                        vColor[c] = (*p.wall.surfaceColor2)[c] * (*p.sectorLightColor)[c];

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
                color.rgba[CR] = MAX_OF(rcolors[i].rgba[CR], minColor.x);
                color.rgba[CG] = MAX_OF(rcolors[i].rgba[CG], minColor.y);
                color.rgba[CB] = MAX_OF(rcolors[i].rgba[CB], minColor.z);
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
                           p.texOffset, p.blendMode, p.lightListIdx, p.glowing);

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
        renderlightprojectionparams_t params;

        params.rvertices = rvertices;
        params.numVertices = numVertices;
        params.realNumVertices = realNumVertices;
        params.lastIdx = 0;
        params.texTL = p.texTL;
        params.texBR = p.texBR;
        params.isWall = p.isWall;
        if(p.isWall)
        {
            params.wall.left.firstDiv = p.wall.left.firstDiv;
            params.wall.left.divCount = p.wall.left.divCount;
            params.wall.right.firstDiv = p.wall.right.firstDiv;
            params.wall.right.divCount = p.wall.right.divCount;
        }

        hasDynlights = (0 != Rend_RenderLightProjections(p.lightListIdx, &params));
    }

    if(useShadows)
    {
        // Render all shadows projected onto this surface.
        rendershadowprojectionparams_t params;

        params.rvertices = rvertices;
        params.numVertices = numVertices;
        params.realNumVertices = realNumVertices;
        params.texTL = p.texTL;
        params.texBR = p.texBR;
        params.isWall = p.isWall;
        if(p.isWall)
        {
            params.wall.left.firstDiv = p.wall.left.firstDiv;
            params.wall.left.divCount = p.wall.left.divCount;
            params.wall.right.firstDiv = p.wall.right.firstDiv;
            params.wall.right.divCount = p.wall.right.divCount;
        }

        Rend_RenderShadowProjections(p.shadowListIdx, &params);
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
        if(p.texOffset) RL_Rtu_TranslateOffsetv(RTU_PRIMARY, p.texOffset);
        if(p.texScale)  RL_Rtu_ScaleST(RTU_PRIMARY, p.texScale);
    }

    if(primaryDetailRTU)
    {
        if(p.texOffset) RL_Rtu_TranslateOffsetv(RTU_PRIMARY_DETAIL, p.texOffset);
    }

    if(interRTU)
    {
        if(p.texOffset) RL_Rtu_TranslateOffsetv(RTU_INTER, p.texOffset);
        if(p.texScale)  RL_Rtu_ScaleST(RTU_INTER, p.texScale);
    }

    if(interDetailRTU)
    {
        if(p.texOffset) RL_Rtu_TranslateOffsetv(RTU_INTER_DETAIL, p.texOffset);
    }

    if(shinyMaskRTU)
    {
        if(p.texOffset) RL_Rtu_TranslateOffsetv(RTU_REFLECTION_MASK, p.texOffset);
        if(p.texScale)  RL_Rtu_ScaleST(RTU_REFLECTION_MASK, p.texScale);
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

static boolean doRenderHEdge(HEdge *hedge, Vector3f const &normal,
    float alpha, float lightLevel, float lightLevelDL, float lightLevelDR,
    Vector3f const *lightColor, uint lightListIdx, uint shadowListIdx,
    walldivs_t *leftWallDivs, walldivs_t *rightWallDivs,
    boolean skyMask, boolean addFakeRadio, vec3d_t texTL, vec3d_t texBR,
    float const texOffset[2], float const texScale[2],
    blendmode_t blendMode, Vector3f const &color, Vector3f const &color2,
    biassurface_t *bsuf, uint elmIdx /*tmp*/,
    MaterialSnapshot const &ms,
    boolean isTwosidedMiddle)
{
    SideDef *sideDef = hedge->hasLine()? hedge->lineSide().sideDefPtr() : 0;
    rvertex_t *rvertices;

    // Init the params.
    rendworldpoly_params_t params;
    std::memset(&params, 0, sizeof(params));

    params.flags = RPF_DEFAULT | (skyMask? RPF_SKYMASK : 0);
    params.isWall = true;
    params.wall.segLength = hedge->length();
    params.forceOpaque = (alpha < 0? true : false);
    params.alpha = (alpha < 0? 1 : alpha);
    params.mapElement = hedge;
    params.elmIdx = elmIdx;
    params.bsuf = bsuf;
    params.normal = &normal;
    params.texTL = texTL;
    params.texBR = texBR;
    params.sectorLightLevel = lightLevel;
    params.surfaceLightLevelDL = lightLevelDL;
    params.surfaceLightLevelDR = lightLevelDR;
    params.sectorLightColor = lightColor;
    params.surfaceColor = &color;
    params.wall.surfaceColor2 = &color2;
    if(glowFactor > .0001f)
        params.glowing = ms.glowStrength() * glowFactor; // Global scale factor.
    params.blendMode = blendMode;
    params.texOffset = texOffset;
    params.texScale = texScale;
    params.lightListIdx = lightListIdx;
    params.shadowListIdx = shadowListIdx;
    params.wall.left.firstDiv = WallDivNode_Next(WallDivs_First(leftWallDivs)); // Step over first node.
    params.wall.left.divCount = WallDivs_Size(leftWallDivs)-2;
    params.wall.right.firstDiv = WallDivNode_Prev(WallDivs_Last(rightWallDivs)); // Step over last node.
    params.wall.right.divCount = WallDivs_Size(rightWallDivs)-2;

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
    V2f_Copyd(rvertices[0].pos, hedge->v1Origin());
    rvertices[0].pos[VZ] = (float)WallDivNode_Height(WallDivs_First(leftWallDivs));

    // Top Left.
    V2f_Copyd(rvertices[1].pos, hedge->v1Origin());
    rvertices[1].pos[VZ] = (float)WallDivNode_Height(WallDivs_Last(leftWallDivs));

    // Bottom Right.
    V2f_Copyd(rvertices[2].pos, hedge->v2Origin());
    rvertices[2].pos[VZ] = (float)WallDivNode_Height(WallDivs_First(rightWallDivs));

    // Top Right.
    V2f_Copyd(rvertices[3].pos, hedge->v2Origin());
    rvertices[3].pos[VZ] = (float)WallDivNode_Height(WallDivs_Last(rightWallDivs));

    // Draw this hedge.
    if(renderWorldPoly(rvertices, 4, params, ms))
    {
        // Drawn poly was opaque.
        // Render Fakeradio polys for this hedge?
        if(!(params.flags & RPF_SKYMASK) && addFakeRadio)
        {
            RendRadioWallSectionParms radioParams;

            radioParams.line      = hedge->linePtr();

            SideDef::FakeRadioData &frData = sideDef->fakeRadioData();
            radioParams.botCn     = frData.bottomCorners;
            radioParams.topCn     = frData.topCorners;
            radioParams.sideCn    = frData.sideCorners;
            radioParams.spans     = frData.spans;

            radioParams.segOffset = hedge->lineOffset();
            radioParams.segLength = hedge->length();
            radioParams.frontSec  = hedge->sectorPtr();
            radioParams.wall.left.firstDiv  = params.wall.left.firstDiv;
            radioParams.wall.left.divCount  = params.wall.left.divCount;
            radioParams.wall.right.firstDiv = params.wall.right.firstDiv;
            radioParams.wall.right.divCount = params.wall.right.divCount;

            if(!isTwosidedMiddle && !(hedge->hasTwin() && !hedge->twin().hasLineSideDef()))
            {
                radioParams.backSec = hedge->hasTwin()? hedge->twin().sectorPtr() : 0;
            }
            else
            {
                radioParams.backSec = NULL;
            }

            /// @todo kludge: Revert the vertex coords as they may have been changed
            ///               due to height divisions.

            // Bottom Left.
            V2f_Copyd(rvertices[0].pos, hedge->v1Origin());
            rvertices[0].pos[VZ] = (float)WallDivNode_Height(WallDivs_First(leftWallDivs));

            // Top Left.
            V2f_Copyd(rvertices[1].pos, hedge->v1Origin());
            rvertices[1].pos[VZ] = (float)WallDivNode_Height(WallDivs_Last(leftWallDivs));

            // Bottom Right.
            V2f_Copyd(rvertices[2].pos, hedge->v2Origin());
            rvertices[2].pos[VZ] = (float)WallDivNode_Height(WallDivs_First(rightWallDivs));

            // Top Right.
            V2f_Copyd(rvertices[3].pos, hedge->v2Origin());
            rvertices[3].pos[VZ] = (float)WallDivNode_Height(WallDivs_Last(rightWallDivs));

            // kludge end.

            float ll = lightLevel;
            Rend_ApplyLightAdaptation(&ll);
            if(ll > 0)
            {
                // Determine the shadow properties.
                /// @todo Make cvars out of constants.
                radioParams.shadowSize = 2 * (8 + 16 - ll * 16);
                radioParams.shadowDark = Rend_RadioCalcShadowDarkness(ll);

                if(radioParams.shadowSize > 0)
                {
                    // Shadows are black.
                    radioParams.shadowRGB[CR] =
                            radioParams.shadowRGB[CG] =
                                radioParams.shadowRGB[CB] = 0;

                    Rend_RadioWallSection(rvertices, radioParams);
                }
            }
        }

        R_FreeRendVertices(rvertices);
        return true; // Clip with this solid hedge.
    }

    R_FreeRendVertices(rvertices);

    return false; // Do not clip with this.
}

static void renderPlane(BspLeaf *bspLeaf, Plane::Type type, coord_t height,
    Vector3f const &tangent, Vector3f const &bitangent, Vector3f const &normal,
    Material *inMat, Vector3f const &sufColor, float sufAlpha, blendmode_t blendMode,
    vec3d_t texTL, vec3d_t texBR,
    float const texOffset[2], float const texScale[2],
    boolean skyMasked,
    boolean addDLights, boolean addMobjShadows,
    biassurface_t *bsuf, uint elmIdx /*tmp*/,
    int texMode /*tmp*/)
{
    Sector *sec = bspLeaf->sectorPtr();

    rendworldpoly_params_t params;
    std::memset(&params, 0, sizeof(params));

    params.flags = RPF_DEFAULT;
    params.isWall = false;
    params.mapElement = bspLeaf;
    params.elmIdx = elmIdx;
    params.bsuf = bsuf;
    params.normal = &normal;
    params.texTL = texTL;
    params.texBR = texBR;
    params.sectorLightLevel = sec->lightLevel();
    params.sectorLightColor = &R_GetSectorLightColor(*sec);
    params.surfaceLightLevelDL = params.surfaceLightLevelDR = 0;
    params.surfaceColor = &sufColor;
    params.texOffset = texOffset;
    params.texScale = texScale;

    Material *mat = 0;
    if(skyMasked)
    {
        // In devRendSkyMode mode we render all polys destined for the
        // skymask as regular world polys (with a few obvious properties).
        if(devRendSkyMode)
        {
            params.blendMode = BM_NORMAL;
            params.glowing = 1;
            params.forceOpaque = true;
            mat = inMat;
        }
        else
        {   // We'll mask this.
            params.flags |= RPF_SKYMASK;
        }
    }
    else
    {
        mat = inMat;

        if(type != Plane::Middle)
        {
            params.blendMode = BM_NORMAL;
            params.alpha = 1;
            params.forceOpaque = true;
        }
        else
        {
            if(blendMode == BM_NORMAL && noSpriteTrans)
                params.blendMode = BM_ZEROALPHA; // "no translucency" mode
            else
                params.blendMode = blendMode;
            params.alpha = sufAlpha;
        }
    }

    uint numVertices;
    rvertex_t *rvertices;
    Rend_BuildBspLeafPlaneGeometry(bspLeaf, (type == Plane::Ceiling), height,
                                   &rvertices, &numVertices);

    MaterialSnapshot const &ms = mat->prepare(mapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT));

    if(!(params.flags & RPF_SKYMASK))
    {
        if(glowFactor > .0001f)
        {
            if(texMode != 2)
            {
                params.glowing = ms.glowStrength();
            }
            else
            {
                Surface const &suf = sec->planeSurface(elmIdx);
                Material &material = suf.hasMaterial()? suf.material()
                                                      : App_Materials().find(de::Uri("System", Path("missing"))).material();

                MaterialSnapshot const &ms = material.prepare(Rend_MapSurfaceMaterialSpec());
                params.glowing = ms.glowStrength();
            }

            params.glowing *= glowFactor; // Global scale factor.
        }

        // Dynamic lights?
        if(addDLights && params.glowing < 1 && !(!useDynLights && !useWallGlow))
        {
            params.lightListIdx = LO_ProjectToSurface((PLF_NO_PLANE | (type == Plane::Floor? PLF_TEX_FLOOR : PLF_TEX_CEILING)), bspLeaf, 1,
                params.texTL, params.texBR, tangent, bitangent, normal);
        }

        // Mobj shadows?
        if(addMobjShadows && params.glowing < 1 && Rend_MobjShadowsEnabled())
        {
            // Glowing planes inversely diminish shadow strength.
            params.shadowListIdx = R_ProjectShadowsToSurface(bspLeaf, 1 - params.glowing,
                params.texTL, params.texBR, tangent, bitangent, normal);
        }
    }

    renderWorldPoly(rvertices, numVertices, params, ms);

    R_FreeRendVertices(rvertices);
}

static void Rend_RenderPlane(Plane::Type type, coord_t height,
    Vector3f const &tangent, Vector3f const &bitangent, Vector3f const &normal,
    Material* inMat, Vector3f const &sufColor, float sufAlpha, blendmode_t blendMode,
    float const texOffset[2], float const texScale[2],
    boolean skyMasked, boolean addDLights, boolean addMobjShadows,
    biassurface_t* bsuf, uint elmIdx /*tmp*/,
    int texMode /*tmp*/, boolean clipBackFacing)
{
    BspLeaf *bspLeaf = currentBspLeaf;

    // Must have a visible surface.
    if(!inMat || !inMat->isDrawable()) return;

    Vector3f eyeToSurface(vOrigin[VX] - bspLeaf->center()[VX],
                          vOrigin[VZ] - bspLeaf->center()[VY],
                          vOrigin[VY] - height);

    // Don't bother with planes facing away from the camera.
    if(!(clipBackFacing && !(eyeToSurface.dot(normal) < 0)))
    {
        coord_t texTL[3], texBR[3];

        // Set the texture origin, Y is flipped for the ceiling.
        V3d_Set(texTL, bspLeaf->aaBox().minX,
                       bspLeaf->aaBox().arvec2[type == Plane::Floor? 1 : 0][VY], height);
        V3d_Set(texBR, bspLeaf->aaBox().maxX,
                       bspLeaf->aaBox().arvec2[type == Plane::Floor? 0 : 1][VY], height);

        renderPlane(bspLeaf, type, height, tangent, bitangent, normal, inMat,
                    sufColor, sufAlpha, blendMode, texTL, texBR, texOffset, texScale,
                    skyMasked, addDLights, addMobjShadows, bsuf, elmIdx, texMode);
    }
}

/**
 * @defgroup rendHEdgeFlags Rend Half-edge Flags
 * Flags for rendHEdgeSection()
 * @ingroup flags
 */
///@{
#define RHF_ADD_DYNLIGHTS       0x01 ///< Write geometry for dynamic lights.
#define RHF_ADD_DYNSHADOWS      0x02 ///< Write geometry for dynamic (mobj) shadows.
#define RHF_ADD_RADIO           0x04 ///< Write geometry for faked radiosity.
#define RHF_VIEWER_NEAR_BLEND   0x08 ///< Alpha-blend geometry when viewer is near.
#define RHF_FORCE_OPAQUE        0x10 ///< Force the geometry to be opaque.
///@}

static boolean rendHEdgeSection(HEdge *hedge, SideDefSection section,
    int flags, float lightLevel, Vector3f const &lightColor,
    walldivs_t *leftWallDivs, walldivs_t *rightWallDivs,
    float const matOffset[2])
{
    SideDef *frontSide = hedge->hasLine()? hedge->lineSide().sideDefPtr() : 0;
    Surface *surface = &frontSide->surface(section);
    boolean opaque = true;
    float alpha;

    // Surfaces without a drawable material are never rendered.
    if(!(surface->hasMaterial() && surface->material().isDrawable()))
        return false;

    if(WallDivNode_Height(WallDivs_First(leftWallDivs)) >=
       WallDivNode_Height(WallDivs_Last(rightWallDivs))) return true;

    alpha = (section == SS_MIDDLE? surface->opacity() : 1.0f);

    if(section == SS_MIDDLE && (flags & RHF_VIEWER_NEAR_BLEND))
    {
        mobj_t *mo = viewPlayer->shared.mo;
        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

        /**
         * Can the player walk through this surface?
         * If the player is close enough we should NOT add a solid hedge otherwise HOM
         * would occur when they are directly on top of the line (e.g., passing through
         * an opaque waterfall).
         */

        if(viewData->current.origin[VZ] >  WallDivNode_Height(WallDivs_First(leftWallDivs)) &&
           viewData->current.origin[VZ] < WallDivNode_Height(WallDivs_Last(rightWallDivs)))
        {
            LineDef const &line = hedge->line();
            vec2d_t result;
            double pos = V2d_ProjectOnLine(result, mo->origin, line.v1Origin(),
                                                   line.direction());

            if(pos > 0 && pos < 1)
            {
                coord_t const minDistance = mo->radius * .8f;

                coord_t delta[2]; V2d_Subtract(delta, mo->origin, result);
                coord_t distance = M_ApproxDistance(delta[VX], delta[VY]);

                if(distance < minDistance)
                {
                    // Fade it out the closer the viewPlayer gets and clamp.
                    alpha = (alpha / minDistance) * distance;
                    alpha = de::clamp(0.f, alpha, 1.f);
                }

                if(alpha < 1)
                    opaque = false;
            }
        }
    }

    if(alpha > 0)
    {
        uint lightListIdx = 0, shadowListIdx = 0;
        vec3d_t texTL, texBR;
        float texScale[2];
        Material *mat = NULL;
        int rpFlags = RPF_DEFAULT;
        boolean isTwoSided = (hedge->hasLine() && hedge->line().hasFrontSideDef() && hedge->line().hasBackSideDef())? true:false;
        blendmode_t blendMode = BM_NORMAL;
        Vector3f const *color = 0, *color2 = 0;

        texScale[0] = ((surface->flags() & DDSUF_MATERIAL_FLIPH)? -1 : 1);
        texScale[1] = ((surface->flags() & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        V2d_Copy(texTL,  hedge->v1Origin());
        texTL[VZ] =  WallDivNode_Height(WallDivs_Last(leftWallDivs));

        V2d_Copy(texBR, hedge->v2Origin());
        texBR[VZ] = WallDivNode_Height(WallDivs_First(rightWallDivs));

        // Determine which Material to use.
        if(devRendSkyMode && hedge->hasTwin() &&
           ((section == SS_BOTTOM && hedge->sector().floorSurface().hasSkyMaskedMaterial() &&
                                     hedge->twin().sector().floorSurface().hasSkyMaskedMaterial()) ||
            (section == SS_TOP    && hedge->sector().ceilingSurface().hasSkyMaskedMaterial() &&
                                     hedge->twin().sector().ceilingSurface().hasSkyMaskedMaterial())))
        {
            // Geometry not normally rendered however we do so in dev sky mode.
            mat = hedge->sector().planeSurface(section == SS_TOP? Plane::Ceiling : Plane::Floor).materialPtr();
        }
        else
        {
            if(renderTextures == 2)
            {
                // Lighting debug mode; render using System:gray.
                mat = &App_Materials().find(de::Uri("System", Path("gray"))).material();
            }
            else if(!surface->hasMaterial() || (surface->hasFixMaterial() && devNoTexFix))
            {
                // Missing material debug mode; render using System:missing.
                mat = &App_Materials().find(de::Uri("System", Path("missing"))).material();
            }
            else
            {
                // Normal mode.
                mat = surface->materialPtr();
            }

            if(mat->isSkyMasked())
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
                    flags |= RHF_FORCE_OPAQUE;
                }
            }
        }

        MaterialSnapshot const &ms = mat->prepare(mapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT));

        // Fill in the remaining params data.
        if(!(rpFlags & RPF_SKYMASK))
        {
            // Make any necessary adjustments to the draw flags to suit the
            // current texture mode.
            if(section != SS_MIDDLE || (section == SS_MIDDLE && !isTwoSided))
            {
                flags |= RHF_FORCE_OPAQUE;
                blendMode = BM_NORMAL;
            }
            else
            {
                if(surface->blendMode() == BM_NORMAL && noSpriteTrans)
                    blendMode = BM_ZEROALPHA; // "no translucency" mode
                else
                    blendMode = surface->blendMode();
            }

            if(Surface::isFromPolyobj(*surface))
                flags &= ~RHF_ADD_RADIO;

            float glowStrength = 0;
            if(glowFactor > .0001f)
                glowStrength = ms.glowStrength() * glowFactor; // Global scale factor.

            // Dynamic Lights?
            if((flags & RHF_ADD_DYNLIGHTS) &&
               glowStrength < 1 && !(!useDynLights && !useWallGlow))
            {
                Surface const &middleSurface = hedge->lineSideDef().middle();
                int plFlags = ((section == SS_MIDDLE && isTwoSided)? PLF_SORT_LUMINOSITY_DESC : 0);

                lightListIdx = LO_ProjectToSurface(plFlags, currentBspLeaf, 1, texTL, texBR,
                                                   middleSurface.tangent(),
                                                   middleSurface.bitangent(),
                                                   middleSurface.normal());
            }

            // Dynamic shadows?
            if((flags & RHF_ADD_DYNSHADOWS) &&
               glowStrength < 1 && Rend_MobjShadowsEnabled())
            {
                Surface const &middleSurface = hedge->lineSideDef().middle();

                // Glowing planes inversely diminish shadow strength.
                float const shadowStrength = 1 - glowStrength;

                shadowListIdx = R_ProjectShadowsToSurface(currentBspLeaf, shadowStrength, texTL, texBR,
                                                          middleSurface.tangent(),
                                                          middleSurface.bitangent(),
                                                          middleSurface.normal());
            }

            if(glowStrength > 0)
                flags &= ~RHF_ADD_RADIO;

            selectSurfaceColors(&color, &color2, &hedge->lineSideDef(), section);
        }

        float deltaL, deltaR;

        // Do not apply an angle based lighting delta if this surface's material
        // has been chosen as a HOM fix (we must remain consistent with the lighting
        // applied to the back plane (on this half-edge's back side)).
        if(hedge->hasLineSideDef() && isTwoSided && section != SS_MIDDLE && surface->hasFixMaterial())
        {
            deltaL = deltaR = 0;
        }
        else
        {
            LineDef const &line = hedge->line();
            line.lightLevelDelta(hedge->lineSideId(), &deltaL, &deltaR);

            // Linear interpolation of the linedef light deltas to the edges of the hedge.
            float diff = deltaR - deltaL;
            deltaR = deltaL + ((hedge->lineOffset() + hedge->length()) / line.length()) * diff;
            deltaL += (hedge->lineOffset() / line.length()) * diff;
        }

        opaque = doRenderHEdge(hedge,
                               surface->normal(), ((flags & RHF_FORCE_OPAQUE)? -1 : alpha),
                               lightLevel, deltaL, deltaR, &lightColor,
                               lightListIdx, shadowListIdx,
                               leftWallDivs, rightWallDivs,
                               !!(rpFlags & RPF_SKYMASK), !!(flags & RHF_ADD_RADIO),
                               texTL, texBR, matOffset, texScale, blendMode,
                               *color, *color2,
                               &hedge->biasSurfaceForGeometryGroup(section), (uint) section,
                               ms,
                               (section == SS_MIDDLE && isTwoSided));
    }

    return opaque;
}

static void reportLineDrawn(LineDef &line)
{
    // Already been here?
    int playerNum = viewPlayer - ddPlayers;
    if(line.mappedByPlayer(playerNum)) return;

    // Mark as drawn.
    line._mapped[playerNum] = true;

    // Send a status report.
    if(gx.HandleMapObjectStatusReport)
    {
        gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED, theMap->lineIndex(&line),
                                       DMU_LINEDEF, &playerNum);
    }
}

/**
 * @param hedge  HEdge to draw wall surfaces for.
 * @param sections  @ref sideSectionFlags
 */
static boolean Rend_RenderHEdge(HEdge *hedge, byte sections)
{
    if(!hedge->hasLine() || !hedge->lineSide().hasSideDef()) return false;

    // Only a "middle" section.
    if(sections & SSF_MIDDLE)
    {
        BspLeaf *leaf    = currentBspLeaf;
        Sector *frontSec = leaf->sectorPtr();
        Sector *backSec  = hedge->hasTwin()? hedge->twin().sectorPtr() : 0;

        walldivs_t leftWallDivs, rightWallDivs;
        float matOffset[2];
        boolean opaque = false;

        if(hedge->prepareWallDivs(SS_MIDDLE, frontSec, backSec,
                                  &leftWallDivs, &rightWallDivs, matOffset))
        {
            Rend_RadioUpdateLine(hedge->line(), hedge->lineSideId());
            opaque = rendHEdgeSection(hedge, SS_MIDDLE, RHF_ADD_DYNLIGHTS|RHF_ADD_DYNSHADOWS|RHF_ADD_RADIO,
                                      frontSec->lightLevel(), R_GetSectorLightColor(*frontSec),
                                      &leftWallDivs, &rightWallDivs, matOffset);
        }

        reportLineDrawn(hedge->line());
        return opaque;
    }

    return false;
}

/**
 * Render wall sections for a HEdge belonging to a two-sided LineDef.
 */
static boolean Rend_RenderHEdgeTwosided(HEdge *hedge, byte sections)
{
    DENG_ASSERT(hedge);

    BspLeaf *leaf = currentBspLeaf;
    int solidSeg = false;

    if(!hedge->hasLine()) return false;

    LineDef &line = hedge->line();
    LineDef::Side *front = &hedge->lineSide();
    LineDef::Side *back  = hedge->hasTwin()? &hedge->twin().lineSide() : 0;
    Sector *backSector   = hedge->hasTwin()? hedge->twin().sectorPtr() : 0;

    reportLineDrawn(line);

    if(back->sectorPtr() == front->sectorPtr() &&
       !front->sideDef().top().hasMaterial() && !front->sideDef().bottom().hasMaterial() &&
       !front->sideDef().middle().hasMaterial())
       return false; // Ugh... an obvious wall hedge hack. Best take no chances...

    Plane *ffloor = &leaf->sector().floor();
    Plane *fceil  = &leaf->sector().ceiling();
    Plane *bfloor = &back->sector().floor();
    Plane *bceil  = &back->sector().ceiling();

    /**
     * Create the wall sections.
     *
     * We may need multiple wall sections.
     * Determine which parts of the segment are really visible.
     */

    // Middle section?
    if(sections & SSF_MIDDLE)
    {
        walldivs_t leftWallDivs, rightWallDivs;
        float matOffset[2];

        if(hedge->prepareWallDivs(SS_MIDDLE, leaf->sectorPtr(), backSector,
                                  &leftWallDivs, &rightWallDivs, matOffset))
        {
            int rhFlags = RHF_ADD_DYNLIGHTS|RHF_ADD_DYNSHADOWS|RHF_ADD_RADIO;

            if((viewPlayer->shared.flags & (DDPF_NOCLIP|DDPF_CAMERA)) ||
               !(line.isFlagged(DDLF_BLOCKING)))
                rhFlags |= RHF_VIEWER_NEAR_BLEND;

            Rend_RadioUpdateLine(line, hedge->lineSideId());
            solidSeg = rendHEdgeSection(hedge, SS_MIDDLE, rhFlags,
                                        front->sector().lightLevel(), R_GetSectorLightColor(front->sector()),
                                        &leftWallDivs, &rightWallDivs, matOffset);
            if(solidSeg)
            {
                Surface &surface = front->sideDef().middle();
                coord_t xbottom, xtop;

                if(line.isSelfReferencing())
                {
                    xbottom = de::min(bfloor->visHeight(), ffloor->visHeight());
                    xtop    = de::max(bceil->visHeight(),  fceil->visHeight());
                }
                else
                {
                    xbottom = de::max(bfloor->visHeight(), ffloor->visHeight());
                    xtop    = de::min(bceil->visHeight(),  fceil->visHeight());
                }

                xbottom += surface.visMaterialOrigin()[VY];
                xtop    += surface.visMaterialOrigin()[VY];

                // Can we make this a solid segment?
                if(!(WallDivNode_Height(WallDivs_Last(&rightWallDivs)) >= xtop &&
                     WallDivNode_Height(WallDivs_First(&leftWallDivs)) <= xbottom))
                     solidSeg = false;
            }
        }
    }

    // Upper section?
    if(sections & SSF_TOP)
    {
        walldivs_t leftWallDivs, rightWallDivs;
        float matOffset[2];

        if(hedge->prepareWallDivs(SS_TOP, leaf->sectorPtr(), backSector,
                                  &leftWallDivs, &rightWallDivs, matOffset))
        {
            Rend_RadioUpdateLine(line, hedge->lineSideId());
            rendHEdgeSection(hedge, SS_TOP, RHF_ADD_DYNLIGHTS|RHF_ADD_DYNSHADOWS|RHF_ADD_RADIO,
                             front->sector().lightLevel(), R_GetSectorLightColor(front->sector()),
                             &leftWallDivs, &rightWallDivs, matOffset);
        }
    }

    // Lower section?
    if(sections & SSF_BOTTOM)
    {
        walldivs_t leftWallDivs, rightWallDivs;
        float matOffset[2];

        if(hedge->prepareWallDivs(SS_BOTTOM, leaf->sectorPtr(), backSector,
                                  &leftWallDivs, &rightWallDivs, matOffset))
        {
            Rend_RadioUpdateLine(line, hedge->lineSideId());
            rendHEdgeSection(hedge, SS_BOTTOM, RHF_ADD_DYNLIGHTS|RHF_ADD_DYNSHADOWS|RHF_ADD_RADIO,
                             front->sector().lightLevel(), R_GetSectorLightColor(front->sector()),
                             &leftWallDivs, &rightWallDivs, matOffset);
        }
    }

    // Can we make this a solid segment in the clipper?
    if(solidSeg == -1)
        return false; // NEVER (we have a hole we couldn't fix).

    if(line.isSelfReferencing())
       return false;

    if(!solidSeg) // We'll have to determine whether we can...
    {
        /*if(back->sector == front->sector)
        {
            // An obvious hack, what to do though??
        }
        else*/
             if(   (bceil->visHeight() <= ffloor->visHeight() &&
                        (front->sideDef().top().hasMaterial()    || front->sideDef().middle().hasMaterial()))
                || (bfloor->visHeight() >= fceil->visHeight() &&
                        (front->sideDef().bottom().hasMaterial() || front->sideDef().middle().hasMaterial())))
        {
            // A closed gap?
            if(FEQUAL(fceil->visHeight(), bfloor->visHeight()))
            {
                solidSeg = (bceil->visHeight() <= bfloor->visHeight()) ||
                           !(fceil->surface().hasSkyMaskedMaterial() &&
                             bceil->surface().hasSkyMaskedMaterial());
            }
            else if(FEQUAL(ffloor->visHeight(), bceil->visHeight()))
            {
                solidSeg = (bfloor->visHeight() >= bceil->visHeight()) ||
                           !(ffloor->surface().hasSkyMaskedMaterial() &&
                             bfloor->surface().hasSkyMaskedMaterial());
            }
            else
            {
                solidSeg = true;
            }
        }
        /// @todo Is this still necessary?
        else if(bceil->visHeight() <= bfloor->visHeight() ||
                (!(bceil->visHeight() - bfloor->visHeight() > 0) && bfloor->visHeight() > ffloor->visHeight() && bceil->visHeight() < fceil->visHeight() &&
                front->sideDef().top().hasMaterial() && front->sideDef().bottom().hasMaterial()))
        {
            // A zero height back segment
            solidSeg = true;
        }
    }

    if(solidSeg && !P_IsInVoid(viewPlayer))
        return true;

    return false;
}

static void Rend_MarkSegsFacingFront(BspLeaf *leaf)
{
    if(HEdge *base = leaf->firstHEdge())
    {
        HEdge *hedge = base;
        do
        {
            // Occlusions can only happen where two sectors contact.
            if(hedge->hasLine())
            {
                // Which way should it be facing?
                if(!(viewFacingDot(hedge->v1Origin(), hedge->v2Origin()) < 0))
                    hedge->_frameFlags |= HEDGEINF_FACINGFRONT;
                else
                    hedge->_frameFlags &= ~HEDGEINF_FACINGFRONT;
            }
        } while((hedge = &hedge->next()) != base);
    }

    if(Polyobj *po = leaf->firstPolyobj())
    {
        foreach(LineDef *line, po->lines())
        {
            HEdge &hedge = line->front().leftHEdge();

            // Which way should it be facing?
            if(!(viewFacingDot(hedge.v1Origin(), hedge.v2Origin()) < 0))
                hedge._frameFlags |= HEDGEINF_FACINGFRONT;
            else
                hedge._frameFlags &= ~HEDGEINF_FACINGFRONT;
        }
    }
}

static void occludeFrontFacingSegsInBspLeaf(BspLeaf const *bspLeaf)
{
    if(HEdge *base = bspLeaf->firstHEdge())
    {
        HEdge *hedge = base;
        do
        {
            if(!hedge->hasLine() || !(hedge->_frameFlags & HEDGEINF_FACINGFRONT)) continue;

            if(!C_CheckRangeFromViewRelPoints(hedge->v1Origin(), hedge->v2Origin()))
            {
                hedge->_frameFlags &= ~HEDGEINF_FACINGFRONT;
            }
        } while((hedge = &hedge->next()) != base);
    }

    if(Polyobj *po = bspLeaf->firstPolyobj())
    {
        foreach(LineDef *line, po->lines())
        {
            HEdge &hedge = line->front().leftHEdge();

            if(!(hedge._frameFlags & HEDGEINF_FACINGFRONT)) continue;

            if(!C_CheckRangeFromViewRelPoints(hedge.v1Origin(), hedge.v2Origin()))
            {
                hedge._frameFlags &= ~HEDGEINF_FACINGFRONT;
            }
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
    Sector const *frontSec = hedge->sectorPtr();
    Sector const *backSec  = hedge->hasTwin()? hedge->twin().sectorPtr() : 0;
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
 * @return  @c true if this half-edge is considered "closed" (i.e.,
 *     there is no opening through which the back Sector can be seen).
 *     Tests consider all Planes which interface with this and the "middle"
 *     Material used on the relative front side (if any).
 */
static bool hedgeBackClosedForSkyFix(HEdge const &hedge)
{
    LineDef const &line     = hedge.line();
    Sector const *frontSec  = line.sectorPtr(hedge.lineSideId());
    Sector const *backSec   = line.sectorPtr(hedge.lineSideId()^1);
    SideDef const *frontDef = line.sideDefPtr(hedge.lineSideId());
    SideDef const *backDef  = line.sideDefPtr(hedge.lineSideId()^1);

    if(!frontDef) return false;
    if(!backDef) return true;

    if(frontSec == backSec) return false; // Never.

    if(frontSec && backSec)
    {
        if(backSec->floor().visHeight()   >= backSec->ceiling().visHeight())  return true;
        if(backSec->ceiling().visHeight() <= frontSec->floor().visHeight())   return true;
        if(backSec->floor().visHeight()   >= frontSec->ceiling().visHeight()) return true;
    }

    return R_MiddleMaterialCoversOpening(line.flags(), frontSec, backSec, frontDef, backDef,
                                         false/*don't ignore opacity*/);
}

/**
 * Determine which sky fixes are necessary for the specified @a hedge.
 * @param hedge  HEdge to be evaluated.
 * @param skyCap  The fixes we are interested in. @ref skyCapFlags.
 * @return
 */
static int chooseHEdgeSkyFixes(HEdge *hedge, int skyCap)
{
    int fixes = 0;
    if(hedge && hedge->hasLine() && // "minisegs" have no lines.
       hedge->hasSector()) // $degenleaf
    {
        Sector const *frontSec = hedge->sectorPtr();
        Sector const *backSec  = hedge->hasTwin()? hedge->twin().sectorPtr() : 0;

        if(!backSec || backSec != frontSec)
        {
            bool const hasSkyFloor   = frontSec->floorSurface().hasSkyMaskedMaterial();
            bool const hasSkyCeiling = frontSec->ceilingSurface().hasSkyMaskedMaterial();

            if(hasSkyFloor || hasSkyCeiling)
            {
                bool const hasClosedBack = hedgeBackClosedForSkyFix(*hedge);

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
    }
    return fixes;
}

static inline void Rend_BuildBspLeafSkyFixStripEdge(coord_t const vXY[2],
    coord_t v1Z, coord_t v2Z, float texS,
    rvertex_t* v1, rvertex_t* v2, rtexcoord_t* t1, rtexcoord_t* t2)
{
    if(v1)
    {
        DENG_ASSERT(vXY);
        V2f_Copyd(v1->pos, vXY);
        v1->pos[VZ] = v1Z;
    }
    if(v2)
    {
        DENG_ASSERT(vXY);
        V2f_Copyd(v2->pos, vXY);
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
static void Rend_BuildBspLeafSkyFixStripGeometry(BspLeaf *leaf, HEdge *startNode,
    HEdge *endNode, bool _antiClockwise, int skyCap,
    rvertex_t **verts, uint *vertsSize, rtexcoord_t **coords)
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
    float texS = float( node->hasLine()? node->lineOffset() : 0 );
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

            Rend_BuildBspLeafSkyFixStripEdge(node->v1Origin(), zBottom, zTop, texS,
                                             v1, v2, t1, t2);

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

            Rend_BuildBspLeafSkyFixStripEdge((antiClockwise? &node->prev() : &node->next())->v1Origin(),
                                             zBottom, zTop, texS,
                                             v1, v2, t1, t2);

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

    Rend_BuildBspLeafSkyFixStripGeometry(leaf, startNode, endNode, antiClockwise, skyFix,
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
 * @param leaf      BspLeaf to write geometry for.
 * @param skyFix    @ref skyCapFlags
 */
static void Rend_WriteBspLeafSkyFixGeometry(BspLeaf *leaf, int skyFix)
{
    bool const antiClockwise = false;

    if(!leaf || !leaf->hedgeCount() || !leaf->hasSector()) return;
    if(!(skyFix & (SKYCAP_LOWER|SKYCAP_UPPER))) return;

    // We may need to break the loop into multiple strips.
    HEdge *startNode = 0;
    coord_t startZBottom = 0;
    coord_t startZTop    = 0;
    Material *startMaterial = 0;

    HEdge *base = leaf->firstHEdge();
    HEdge *node = base;
    forever
    {
        HEdge *hedge = (antiClockwise? &node->prev() : node);
        bool endStrip = false;
        bool beginNewStrip = false;

        // Is a fix or two necessary for this hedge?
        if(chooseHEdgeSkyFixes(hedge, skyFix))
        {
            coord_t zBottom, zTop;
            Material *skyMaterial = 0;

            skyFixZCoords(hedge, skyFix, &zBottom, &zTop);

            if(devRendSkyMode)
            {
                skyMaterial = hedge->sector().planeSurface(skyFix == SKYCAP_UPPER? Plane::Ceiling : Plane::Floor).materialPtr();
            }

            if(zBottom >= zTop)
            {
                // End the current strip.
                endStrip = true;
            }
            else if(startNode && (!FEQUAL(zBottom, startZBottom) || !FEQUAL(zTop, startZTop) ||
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
            Rend_WriteBspLeafSkyFixStripGeometry(leaf, startNode, node, antiClockwise,
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
        Rend_WriteBspLeafSkyFixStripGeometry(leaf, startNode, base, antiClockwise,
                                             skyFix, startMaterial);
    }
}

/**
 * Determine the HEdge from @a bspleaf whose vertex is suitable for use as the
 * center point of a trifan primitive.
 *
 * Note that we do not want any overlapping or zero-area (degenerate) triangles.
 *
 * We are assured by the node build process that BspLeaf->hedges has been ordered
 * by angle, clockwise starting from the smallest angle.
 *
 * @par Algorithm
 * <pre>For each vertex
 *    For each triangle
 *        if area is not greater than minimum bound, move to next vertex
 *    Vertex is suitable</pre>
 *
 * If a vertex exists which results in no zero-area triangles it is suitable for
 * use as the center of our trifan. If a suitable vertex is not found then the
 * center of BSP leaf should be selected instead (it will always be valid as
 * BSP leafs are convex).
 *
 * @return  The chosen node. Can be @a NULL in which case there was no suitable node.
 */
static HEdge *Rend_ChooseBspLeafFanBase(BspLeaf *leaf)
{
#define MIN_TRIANGLE_EPSILON  (0.1) ///< Area

    if(!leaf) return NULL;

    if(leaf->flags() & BLF_UPDATE_FANBASE)
    {
        HEdge *firstNode = leaf->firstHEdge();
        HEdge *fanBase = firstNode;

        if(leaf->hedgeCount() > 3)
        {
            // Splines with higher vertex counts demand checking.
            Vertex const *base, *a, *b;

            // Search for a good base.
            do
            {
                HEdge *other = firstNode;

                base = &fanBase->v1();
                do
                {
                    // Test this triangle?
                    if(!(fanBase != firstNode && (other == fanBase || other == &fanBase->prev())))
                    {
                        a = &other->from();
                        b = &other->next().from();

                        if(M_TriangleArea(base->origin(), a->origin(), b->origin()) <= MIN_TRIANGLE_EPSILON)
                        {
                            // No good. We'll move on to the next vertex.
                            base = 0;
                        }
                    }

                    // On to the next triangle.
                } while(base && (other = &other->next()) != firstNode);

                if(!base)
                {
                    // No good. Select the next vertex and start over.
                    fanBase = &fanBase->next();
                }
            } while(!base && fanBase != firstNode);

            // Did we find something suitable?
            if(!base) // No.
            {
                fanBase = NULL;
            }
        }
        //else Implicitly suitable (or completely degenerate...).

        leaf->_fanBase = fanBase;
        leaf->_flags &= ~BLF_UPDATE_FANBASE;
    }

    return leaf->fanBase();

#undef MIN_TRIANGLE_EPSILON
}

uint Rend_NumFanVerticesForBspLeaf(BspLeaf *leaf)
{
    if(!leaf) return 0;
    // Are we using a hedge vertex as the fan base?
    Rend_ChooseBspLeafFanBase(leaf);
    return leaf->hedgeCount() + (leaf->fanBase()? 0 : 2);
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
static uint Rend_BuildBspLeafPlaneGeometry(BspLeaf *leaf, boolean antiClockwise,
    coord_t height, rvertex_t **verts, uint *vertsSize)
{
    if(!leaf || !verts) return 0;

    HEdge *fanBase = Rend_ChooseBspLeafFanBase(leaf);
    HEdge *baseNode = fanBase? fanBase : leaf->firstHEdge();

    uint totalVerts = leaf->hedgeCount() + (!fanBase? 2 : 0);
    *verts = R_AllocRendVertices(totalVerts);

    uint n = 0;
    if(!fanBase)
    {
        V2f_Copyd((*verts)[n].pos, leaf->center());
        (*verts)[n].pos[VZ] = float( height );
        n++;
    }

    // Add the vertices for each hedge.
    HEdge *node = baseNode;
    do
    {
        V2f_Copyd((*verts)[n].pos, node->v1Origin());
        (*verts)[n].pos[VZ] = float( height );
        n++;
    } while((node = antiClockwise? &node->prev() : &node->next()) != baseNode);

    // The last vertex is always equal to the first.
    if(!fanBase)
    {
        V2f_Copyd((*verts)[n].pos, leaf->firstHEdge()->v1Origin());
        (*verts)[n].pos[VZ] = float( height );
    }

    if(vertsSize) *vertsSize = totalVerts;
    return totalVerts;
}

/// @param skyFix  @ref skyCapFlags.
static void Rend_RenderSkyFix(int skyFix)
{
    BspLeaf *leaf = currentBspLeaf;
    if(!leaf || !skyFix) return;

    Rend_WriteBspLeafSkyFixGeometry(leaf, skyFix/*, &verts, &numVerts*/);
}

/// @param skyCap  @ref skyCapFlags.
static void Rend_RenderSkyCap(int skyCap)
{
    // Caps are unnecessary in sky debug mode (will be drawn as regular planes).
    if(devRendSkyMode) return;

    BspLeaf *leaf = currentBspLeaf;
    if(!leaf || !skyCap) return;

    rvertex_t *verts;
    uint numVerts;
    Rend_BuildBspLeafPlaneGeometry(leaf, !!(skyCap & SKYCAP_UPPER), R_SkyCapZ(leaf, skyCap),
                                   &verts, &numVerts);

    RL_AddPoly(PT_FAN, RPF_DEFAULT | RPF_SKYMASK, numVerts, verts, NULL);
    R_FreeRendVertices(verts);
}

/// @param skyCap  @ref skyCapFlags
static void Rend_RenderSkySurfaces(int skyCap)
{
    BspLeaf *leaf = currentBspLeaf;

    // Any work to do?
    if(!leaf || !leaf->hedgeCount()) return;
    if(!leaf->hasSector() || !R_SectorContainsSkySurfaces(leaf->sectorPtr())) return;

    // Sky caps are only necessary in sectors with sky-masked planes.
    if((skyCap & SKYCAP_LOWER) && !leaf->sector().floorSurface().hasSkyMaskedMaterial())
        skyCap &= ~SKYCAP_LOWER;
    if((skyCap & SKYCAP_UPPER) && !leaf->sector().ceilingSurface().hasSkyMaskedMaterial())
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
        Rend_RenderSkyFix(SKYCAP_LOWER);
        Rend_RenderSkyCap(SKYCAP_LOWER);
    }

    // Upper?
    if(skyCap & SKYCAP_UPPER)
    {
        Rend_RenderSkyFix(SKYCAP_UPPER);
        Rend_RenderSkyCap(SKYCAP_UPPER);
    }
}

static void Rend_RenderWalls()
{
    BspLeaf *leaf = currentBspLeaf;
    if(!leaf || !leaf->firstHEdge()) return;

    HEdge *base = leaf->firstHEdge();
    HEdge *hedge = base;
    do
    {
        if((hedge->_frameFlags & HEDGEINF_FACINGFRONT) &&
           /* "mini-hedges" have no lines and "windows" have no sidedef */
           hedge->hasLineSideDef())
        {
            Sector *frontSec = hedge->sectorPtr();
            Sector *backSec  = hedge->hasTwin()? hedge->twin().sectorPtr() : 0;
            byte sections = pvisibleLineSections(hedge->linePtr(), hedge->lineSideId());
            boolean opaque;

            if(!frontSec || !backSec ||
               (hedge->hasTwin() && !hedge->twin().hasLineSideDef()) /* front side of a "window" */)
            {
                opaque = Rend_RenderHEdge(hedge, sections);
            }
            else
            {
                opaque = Rend_RenderHEdgeTwosided(hedge, sections);
            }

            // When the viewer is in the void do not range-occlude.
            if(opaque && !P_IsInVoid(viewPlayer))
            {
                C_AddRangeFromViewRelPoints(hedge->v1Origin(), hedge->v2Origin());
            }
        }
    } while((hedge = &hedge->next()) != base);
}

static void Rend_RenderPolyobjs()
{
    BspLeaf *leaf = currentBspLeaf;
    if(!leaf) return;

    Polyobj *po = leaf->firstPolyobj();
    if(!po) return;

    foreach(LineDef *line, po->lines())
    {
        HEdge &hedge = line->front().leftHEdge();

        // Let's first check which way this hedge is facing.
        if(hedge._frameFlags & HEDGEINF_FACINGFRONT)
        {
            byte sections  = pvisibleLineSections(hedge.linePtr(), hedge.lineSideId());
            boolean opaque = Rend_RenderHEdge(&hedge, sections);

            // When the viewer is in the void do not range-occlude.
            if(opaque && !P_IsInVoid(viewPlayer))
            {
                C_AddRangeFromViewRelPoints(hedge.v1Origin(), hedge.v2Origin());
            }
        }
    }
}

static void Rend_RenderPlanes()
{
    BspLeaf *leaf = currentBspLeaf;

    if(!leaf || !leaf->hasSector()) return; // An orphan BSP leaf?
    Sector &sect = leaf->sector();

    // Render all planes of this sector.
    foreach(Plane *plane, sect.planes())
    {
        Surface const *suf = &plane->surface();
        bool isSkyMasked = false;
        bool addDynLights = !devRendSkyMode;
        bool clipBackFacing = false;
        float matOrigin[2];
        float matScale[2];
        Material *mat;
        int texMode;

        isSkyMasked = suf->hasSkyMaskedMaterial();
        if(isSkyMasked && plane->type() != Plane::Middle)
        {
            if(!devRendSkyMode) continue; // Not handled here.
            isSkyMasked = false;
        }

        if(renderTextures == 2)
            texMode = 2;
        else if(!suf->hasMaterial() || (devNoTexFix && suf->hasFixMaterial()))
            texMode = 1;
        else
            texMode = 0;

        if(texMode == 0)
            mat = suf->materialPtr();
        else if(texMode == 1)
            // For debug, render the "missing" texture instead of the texture
            // chosen for surfaces to fix the HOMs.
            mat = &App_Materials().find(de::Uri("System", Path("missing"))).material();
        else
            // For lighting debug, render all solid surfaces using the gray texture.
            mat = &App_Materials().find(de::Uri("System", Path("gray"))).material();

        V2f_Copy(matOrigin, suf->visMaterialOrigin());
        // Add the Y offset to orient the Y flipped texture.
        if(plane->type() == Plane::Ceiling)
            matOrigin[VY] -= leaf->aaBox().maxY - leaf->aaBox().minY;
        // Add the additional offset to align with the worldwide grid.
        matOrigin[VX] += float( leaf->worldGridOffset()[VX] );
        matOrigin[VY] += float( leaf->worldGridOffset()[VY] );
        // Inverted.
        matOrigin[VY] = -matOrigin[VY];

        matScale[VX] = ((suf->flags() & DDSUF_MATERIAL_FLIPH)? -1 : 1);
        matScale[VY] = ((suf->flags() & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        Rend_RenderPlane(plane->type(), plane->visHeight(),
                         suf->tangent(), suf->bitangent(), suf->normal(),
                         mat, suf->tintColor(), suf->opacity(), suf->blendMode(), matOrigin, matScale,
                         isSkyMasked, addDynLights, (!devRendSkyMode && plane->type() == Plane::Floor),
                         &leaf->biasSurfaceForGeometryGroup(plane->inSectorIndex()),
                         plane->inSectorIndex(), texMode, clipBackFacing);
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
        if(hedge->hasLine() && hedge->hasTwin() && hedge->twin().hasSector() &&
           (forwardFacing == ((hedge->_frameFlags & HEDGEINF_FACINGFRONT)? true : false)))
        {
            Sector *backSec      = hedge->twin().sectorPtr();
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

static inline boolean isNullLeaf(BspLeaf *leaf)
{
    if(!leaf || !leaf->hasSector()) return true;

    Sector &sec = leaf->sector();
    if(sec.ceiling().visHeight() - sec.floor().visHeight() <= 0) return true;
    if(leaf->hedgeCount() < 3) return true;
    return false;
}

static void Rend_RenderBspLeaf(BspLeaf *bspLeaf)
{
    DENG_ASSERT(bspLeaf);

    if(isNullLeaf(bspLeaf))
    {
        // Skip this, it has no volume.
        // Neighbors handle adding the solid clipper segments.
        return;
    }

    // This is now the current leaf.
    currentBspLeaf = bspLeaf;

    if(!firstBspLeaf)
    {
        if(!C_CheckBspLeaf(bspLeaf))
            return; // This isn't visible.
    }
    else
    {
        firstBspLeaf = false;
    }

    // Mark the sector visible for this frame.
    Sector &sector = bspLeaf->sector();
    sector._frameFlags |= SIF_VISIBLE;

    Rend_MarkSegsFacingFront(bspLeaf);

    R_InitForBspLeaf(bspLeaf);
    Rend_RadioBspLeafEdges(*bspLeaf);

    uint bspLeafIdx = theMap->bspLeafIndex(bspLeaf);
    occludeBspLeaf(bspLeaf, false);
    LO_ClipInBspLeaf(bspLeafIdx);
    occludeBspLeaf(bspLeaf, true);

    occludeFrontFacingSegsInBspLeaf(bspLeaf);

    if(bspLeaf->hasPolyobj())
    {
        // Polyobjs don't obstruct, do clip lights with another algorithm.
        LO_ClipInBspLeafBySight(bspLeafIdx);
    }

    // Mark the particle generators in the sector visible.
    Rend_ParticleMarkInSectorVisible(&sector);

    /**
     * Sprites for this BSP leaf have to be drawn.
     * @note
     * Must be done BEFORE the segments of this BspLeaf are added to the
     * clipper. Otherwise the sprites would get clipped by them, and that
     * wouldn't be right.
     * Must be done AFTER the lumobjs have been clipped as this affects the
     * projection of flares.
     */
    R_AddSprites(bspLeaf);

    // Write sky-surface geometry.
    Rend_RenderSkySurfaces(SKYCAP_LOWER|SKYCAP_UPPER);

    // Write wall geometry.
    Rend_RenderWalls();

    // Write polyobj geometry.
    Rend_RenderPolyobjs();

    // Write plane geometry.
    Rend_RenderPlanes();
}

static void Rend_RenderNode(MapElement *bspPtr)
{
    // If the clipper is full we're pretty much done. This means no geometry
    // will be visible in the distance because every direction has already been
    // fully covered by geometry.
    if(C_IsFull()) return;

    if(bspPtr->type() == DMU_BSPLEAF)
    {
        // We've arrived at a leaf. Render it.
        Rend_RenderBspLeaf(bspPtr->castTo<BspLeaf>());
    }
    else
    {
        // Descend deeper into the nodes.
        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
        BspNode const &node = *bspPtr->castTo<BspNode>();

        // Decide which side the view point is on.
        byte side = node.partition().pointOnSide(viewData->current.origin);

        Rend_RenderNode(node.childPtr(side)); // Recursively divide front space.
        Rend_RenderNode(node.childPtr(side ^ 1)); // ...and back space.
    }
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

static void drawSurfaceTangentSpaceVectors(Surface *suf, const_pvec3f_t origin)
{
    int const VISUAL_LENGTH = 20;

    static float const red[3]   = { 1, 0, 0 };
    static float const green[3] = { 0, 1, 0 };
    static float const blue[3]  = { 0, 0, 1 };

    DENG_ASSERT(origin && suf);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(origin[VX], origin[VZ], origin[VY]);

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

    vec3f_t origin;
    foreach(HEdge *hedge, theMap->hedges())
    {
        if(!hedge->hasLine() || hedge->line().isFromPolyobj())
            continue;

        if(!hedge->hasSector())
            continue;

        float x = hedge->v1Origin()[VX] + (hedge->v2Origin()[VX] - hedge->v1Origin()[VX]) / 2;
        float y = hedge->v1Origin()[VY] + (hedge->v2Origin()[VY] - hedge->v1Origin()[VY]) / 2;

        if(!(hedge->hasTwin() && hedge->twin().hasSector()))
        {
            coord_t const bottom = hedge->sector().floor().visHeight();
            coord_t const top    = hedge->sector().ceiling().visHeight();
            Surface *suf = &hedge->lineSideDef().middle();

            V3f_Set(origin, x, y, bottom + (top - bottom) / 2);
            drawSurfaceTangentSpaceVectors(suf, origin);
        }
        else
        {
            Sector *backSec  = hedge->twin().sectorPtr();
            SideDef *sideDef = &hedge->lineSideDef();

            if(sideDef->middle().hasMaterial())
            {
                coord_t const bottom = hedge->sector().floor().visHeight();
                coord_t const top    = hedge->sector().ceiling().visHeight();
                Surface *suf = &sideDef->middle();

                V3f_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }

            if(backSec->ceiling().visHeight() <
               hedge->sector().ceiling().visHeight() &&
               !(hedge->sector().ceilingSurface().hasSkyMaskedMaterial() &&
                 backSec->ceilingSurface().hasSkyMaskedMaterial()))
            {
                coord_t const bottom = backSec->ceiling().visHeight();
                coord_t const top    = hedge->sector().ceiling().visHeight();
                Surface *suf = &sideDef->top();

                V3f_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }

            if(backSec->floor().visHeight() >
               hedge->sector().floor().visHeight() &&
               !(hedge->sector().floorSurface().hasSkyMaskedMaterial() &&
                 backSec->floorSurface().hasSkyMaskedMaterial()))
            {
                coord_t const bottom = hedge->sector().floor().visHeight();
                coord_t const top    = backSec->floor().visHeight();
                Surface *suf = &sideDef->bottom();

                V3f_Set(origin, x, y, bottom + (top - bottom) / 2);
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
            V3f_Set(origin, bspLeaf->center()[VX],
                            bspLeaf->center()[VY],
                            plane->visHeight());

            if(plane->type() != Plane::Middle && plane->surface().hasSkyMaskedMaterial())
                origin[VZ] = theMap->skyFix(plane->type() == Plane::Ceiling);

            drawSurfaceTangentSpaceVectors(&plane->surface(), origin);
        }
    }

    foreach(Polyobj *polyobj, theMap->polyobjs())
    {
        Sector const &sector = polyobj->bspLeaf->sector();
        float zPos = sector.floor().height() + (sector.ceiling().height() - sector.floor().height())/2;

        foreach(LineDef *line, polyobj->lines())
        {
            V3f_Set(origin, (line->v2Origin()[VX] + line->v1Origin()[VX])/2,
                            (line->v2Origin()[VY] + line->v1Origin()[VY])/2, zPos);
            drawSurfaceTangentSpaceVectors(&line->frontSideDef().middle(), origin);
        }
    }

    glEnable(GL_CULL_FACE);
}

static void drawSoundOrigin(coord_t const origin[3], const char* label, coord_t const eye[3])
{
    int const MAX_SOUNDORIGIN_DIST = 384; ///< Maximum distance from origin to eye in map coordinates.

    if(!origin || !label || !eye) return;

    coord_t dist = V3d_Distance(origin, eye);
    float alpha = 1.f - MIN_OF(dist, MAX_SOUNDORIGIN_DIST) / MAX_SOUNDORIGIN_DIST;

    if(alpha > 0)
    {
        float scale = dist / (DENG_WINDOW->width() / 2);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(origin[VX], origin[VZ], origin[VY]);
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
    coord_t eye[3];

    if(!devSoundOrigins || !theMap) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    V3d_Set(eye, vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

    if(devSoundOrigins & SOF_SIDEDEF)
    {
        /// @todo Do not assume current map.
        foreach(LineDef *line, theMap->lines())
        for(int i = 0; i < 2; ++i)
        {
            if(!line->hasSideDef(i))
                continue;

            SideDef &sideDef = line->sideDef(i);
            uint idx = theMap->sideDefIndex(&sideDef);
            char buf[80];

            dd_snprintf(buf, 80, "Side #%i (middle)", idx);
            drawSoundOrigin(sideDef.middle().soundEmitter().origin, buf, eye);

            dd_snprintf(buf, 80, "Side #%i (bottom)", idx);
            drawSoundOrigin(sideDef.bottom().soundEmitter().origin, buf, eye);

            dd_snprintf(buf, 80, "Side #%i (top)", idx);
            drawSoundOrigin(sideDef.top().soundEmitter().origin, buf, eye);
        }
    }

    if(devSoundOrigins & (SOF_SECTOR|SOF_PLANE))
    {
        /// @todo Do not assume current map.
        foreach(Sector *sec, theMap->sectors())
        {
            uint sectorIndex = theMap->sectorIndex(sec);
            char buf[80];

            if(devSoundOrigins & SOF_PLANE)
            {
                for(uint i = 0; i < sec->planeCount(); ++i)
                {
                    Plane &plane = sec->plane(i);
                    dd_snprintf(buf, 80, "Sector #%i (pln:%i)", sectorIndex, i);
                    drawSoundOrigin(plane.surface().soundEmitter().origin, buf, eye);
                }
            }

            if(devSoundOrigins & SOF_SECTOR)
            {
                dd_snprintf(buf, 80, "Sector #%i", sectorIndex);
                drawSoundOrigin(sec->soundEmitter().origin, buf, eye);
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
        LineDef *li = &own->line();

        if(li->hasFrontSideDef())
        {
            if(min && li->frontSector().floor().visHeight() < *min)
                *min = li->frontSector().floor().visHeight();

            if(max && li->frontSector().ceiling().visHeight() > *max)
                *max = li->frontSector().ceiling().visHeight();
        }

        if(li->hasBackSideDef())
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
        glVertex3f(vtx->origin()[VX], z, vtx->origin()[VY]);
    glEnd();
}

static void drawVertexBar(Vertex const *vtx, coord_t bottom, coord_t top, float alpha)
{
    int const EXTEND_DIST = 64;

    static const float black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(vtx->origin()[VX], bottom - EXTEND_DIST, vtx->origin()[VY]);
        glColor4f(1, 1, 1, alpha);
        glVertex3f(vtx->origin()[VX], bottom, vtx->origin()[VY]);
        glVertex3f(vtx->origin()[VX], bottom, vtx->origin()[VY]);
        glVertex3f(vtx->origin()[VX], top, vtx->origin()[VY]);
        glVertex3f(vtx->origin()[VX], top, vtx->origin()[VY]);
        glColor4fv(black);
        glVertex3f(vtx->origin()[VX], top + EXTEND_DIST, vtx->origin()[VY]);
    glEnd();
}

static void drawVertexIndex(Vertex const *vtx, coord_t z, float scale, float alpha)
{
    Point2Raw const origin(2, 2);
    char buf[80];

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    sprintf(buf, "%i", theMap->vertexIndex(static_cast<Vertex const *>(vtx)));

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(vtx->origin()[VX], z, vtx->origin()[VY]);
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

static int drawVertex1(LineDef *li, void *context)
{
    Vertex *vtx = &li->v1();
    Polyobj *po = (Polyobj *) context;
    coord_t dist2D = M_ApproxDistance(vOrigin[VX] - vtx->origin()[VX], vOrigin[VZ] - vtx->origin()[VY]);

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

        pos[VX] = vtx->origin()[VX];
        pos[VY] = vtx->origin()[VY];
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
    foreach(LineDef *line, po->lines())
    {
        if(line->validCount() == validCount)
            continue;

        line->_validCount = validCount;
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
            // Not a linedef vertex?
            LineOwner const *own = vertex->firstLineOwner();
            if(!own) continue;

            // Ignore polyobj vertexes.
            if(own->line().isFromPolyobj()) continue;

            float alpha = 1 - M_ApproxDistance(vOrigin[VX] - vertex->origin()[VX],
                                               vOrigin[VZ] - vertex->origin()[VY]) / MAX_VERTEX_POINT_DIST;
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
        // Not a linedef vertex?
        LineOwner const *own = vertex->firstLineOwner();
        if(!own) continue;

        // Ignore polyobj vertexes.
        if(own->line().isFromPolyobj()) continue;

        coord_t dist = M_ApproxDistance(vOrigin[VX] - vertex->origin()[VX],
                                       vOrigin[VZ] - vertex->origin()[VY]);

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

            // Not a linedef vertex?
            LineOwner const *own = vertex->firstLineOwner();
            if(!own) continue;

            // Ignore polyobj vertexes.
            if(own->line().isFromPolyobj()) continue;

            pos[VX] = vertex->origin()[VX];
            pos[VY] = vertex->origin()[VY];
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
        const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

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

        // Add the backside clipping range (if vpitch allows).
        if(vpitch <= 90 - yfov / 2 && vpitch >= -90 + yfov / 2)
        {

            float a = fabs(vpitch) / (90 - yfov / 2);
            binangle_t startAngle =
                (binangle_t) (BANG_45 * fieldOfView / 90) * (1 + a);
            binangle_t angLen = BANG_180 - startAngle;

            viewside = (viewData->current.angle >> (32 - BAMS_BITS)) + startAngle;
            C_SafeAddRange(viewside, viewside + angLen);
            C_SafeAddRange(viewside + angLen, viewside + 2 * angLen);
        }

        // The viewside line for the depth cue.
        viewsidex = -viewData->viewSin;
        viewsidey = viewData->viewCos;

        // We don't want BSP clip checking for the first BSP leaf.
        MapElement *bspRootElement = theMap->bspRoot();
        firstBspLeaf = true;
        if(bspRootElement->type() == DMU_BSPNODE)
        {
            Rend_RenderNode(bspRootElement);
        }
        else
        {
            // A single leaf is a special case.
            Rend_RenderBspLeaf(bspRootElement->castTo<BspLeaf>());
        }

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
 * @param pos           Coordinates of the center of the box (in "world"
 *                      coordinates [VX, VY, VZ]).
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
void Rend_DrawBBox(coord_t const pos[3], coord_t w, coord_t l, coord_t h,
    float a, float const color[3], float alpha, float br, boolean alignToBase)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    if(alignToBase)
        // The Z coordinate is to the bottom of the object.
        glTranslated(pos[VX], pos[VZ] + h, pos[VY]);
    else
        glTranslated(pos[VX], pos[VZ], pos[VY]);

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
void Rend_DrawArrow(coord_t const pos[3], float a, float s, float const color[3],
    float alpha)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glTranslated(pos[VX], pos[VZ], pos[VY]);

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

    mobj_t* mo = (mobj_t *) th;
    coord_t eye[3], size;
    float alpha;

    // We don't want the console player.
    if(mo == ddPlayers[consolePlayer].shared.mo)
        return false; // Continue iteration.
    // Is it vissible?
    if(!(mo->bspLeaf && mo->bspLeaf->sector().frameFlags() & SIF_VISIBLE))
        return false; // Continue iteration.

    V3d_Set(eye, vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

    alpha = 1 - ((V3d_Distance(mo->origin, eye) / (DENG_WINDOW->width()/2)) / 4);
    if(alpha < .25f)
        alpha = .25f; // Don't make them totally invisible.

    // Draw a bounding box in an appropriate color.
    size = mo->radius;
    Rend_DrawBBox(mo->origin, size, size, mo->height/2, 0,
                  (mo->ddFlags & DDMF_MISSILE)? yellow :
                  (mo->ddFlags & DDMF_SOLID)? green : red,
                  alpha, .08f, true);

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

    coord_t eye[3];
    eye[VX] = vOrigin[VX];
    eye[VY] = vOrigin[VZ];
    eye[VZ] = vOrigin[VY];

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

            coord_t pos[3] = { polyobj->aaBox.minX + width,
                               polyobj->aaBox.minY + length,
                               sec.floor().height() };

            float alpha = 1 - ((V3d_Distance(pos, eye) / (DENG_WINDOW->width()/2)) / 4);
            if(alpha < .25f)
                alpha = .25f; // Don't make them totally invisible.

            Rend_DrawBBox(pos, width, length, height, 0, yellow, alpha, .08f, true);

            foreach(LineDef *line, polyobj->lines())
            {
                coord_t pos[3] = { (line->v2Origin()[VX] + line->v1Origin()[VX])/2,
                                   (line->v2Origin()[VY] + line->v1Origin()[VY])/2,
                                   sec.floor().height() };

                Rend_DrawBBox(pos, 0, line->length() / 2, height,
                              BANG2DEG(BANG_90 - line->angle()),
                              green, alpha, 0, true);
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
