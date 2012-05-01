/**\file rend_main.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_edit.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"
#include "de_system.h"

#include "net_main.h"
#include "texturevariant.h"
#include "materialvariant.h"
#include "blockmapvisual.h"

// Surface (tangent-space) Vector Flags.
#define SVF_TANGENT             0x01
#define SVF_BITANGENT           0x02
#define SVF_NORMAL              0x04

/**
 * @defgroup soundOriginFlags  Sound Origin Flags
 * Flags for use with the sound origin debug display.
 */
///@{
#define SOF_SECTOR              0x01
#define SOF_PLANE               0x02
#define SOF_SIDEDEF             0x04
///@}

void Rend_DrawBBox(coord_t const pos[3], coord_t w, coord_t l, coord_t h, float a,
    float const color[3], float alpha, float br, boolean alignToBase);

void Rend_DrawArrow(coord_t const pos[3], float a, float s, float const color3f[3], float alpha);

static void Rend_RenderBoundingBoxes(void);
static DGLuint constructBBox(DGLuint name, float br);

boolean usingFog = false; // Is the fog in use?
float fogColor[4];
float fieldOfView = 95.0f;
byte smoothTexAnim = true;
int useShinySurfaces = true;

int useDynlights = true;
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

float rendLightDistanceAttentuation = 1024;

byte devMobjVLights = 0; // @c 1= Draw mobj vertex lighting vector.
int devMobjBBox = 0; // 1 = Draw mobj bounding boxes (for debug)
int devPolyobjBBox = 0; // 1 = Draw polyobj bounding boxes (for debug)
DGLuint dlBBox = 0; // Display list: active-textured bbox model.

byte devVertexIndices = 0; // @c 1= Draw world vertex indices (for debug).
byte devVertexBars = 0; // @c 1= Draw world vertex position bars.
byte devSoundOrigins = 0; ///< cvar @c 1= Draw sound origin debug display.
byte devSurfaceVectors = 0;
byte devNoTexFix = 0;

static boolean firstBspLeaf; // No range checking for the first one.

void Rend_Register(void)
{
    C_VAR_FLOAT("rend-camera-fov", &fieldOfView, 0, 1, 179);

    C_VAR_FLOAT("rend-glow", &glowFactor, 0, 0, 2);
    C_VAR_INT("rend-glow-height", &glowHeightMax, 0, 0, 1024);
    C_VAR_FLOAT("rend-glow-scale", &glowHeightFactor, 0, 0.1f, 10);
    C_VAR_INT("rend-glow-wall", &useWallGlow, 0, 0, 1);

    C_VAR_INT2("rend-light", &useDynlights, 0, 0, 1, LO_UnlinkMobjLumobjs);
    C_VAR_INT2("rend-light-ambient", &ambientLight, 0, 0, 255, Rend_CalcLightModRange);
    C_VAR_FLOAT("rend-light-attenuation", &rendLightDistanceAttentuation, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-light-bright", &dynlightFactor, 0, 0, 1);
    C_VAR_FLOAT2("rend-light-compression", &lightRangeCompression, 0, -1, 1, Rend_CalcLightModRange);
    C_VAR_FLOAT("rend-light-fog-bright", &dynlightFogBright, 0, 0, 1);
    C_VAR_FLOAT2("rend-light-sky", &rendSkyLight, 0, 0, 1, LG_MarkAllForUpdate);
    C_VAR_BYTE2("rend-light-sky-auto", &rendSkyLightAuto, 0, 0, 1, LG_MarkAllForUpdate);
    C_VAR_FLOAT("rend-light-wall-angle", &rendLightWallAngle, CVF_NO_MAX, 0, 0);
    C_VAR_BYTE("rend-light-wall-angle-smooth", &rendLightWallAngleSmooth, 0, 0, 1);

    C_VAR_BYTE("rend-map-material-precache", &precacheMapMaterials, 0, 0, 1);

    C_VAR_INT("rend-shadow", &useShadows, 0, 0, 1);
    C_VAR_FLOAT("rend-shadow-darkness", &shadowFactor, 0, 0, 2);
    C_VAR_INT("rend-shadow-far", &shadowMaxDistance, CVF_NO_MAX, 0, 0);
    C_VAR_INT("rend-shadow-radius-max", &shadowMaxRadius, CVF_NO_MAX, 0, 0);

    C_VAR_BYTE("rend-tex-anim-smooth", &smoothTexAnim, 0, 0, 1);
    C_VAR_INT("rend-tex-shiny", &useShinySurfaces, 0, 0, 1);

    C_VAR_INT("rend-dev-sky", &devRendSkyMode, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-sky-always", &devRendSkyAlways, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-freeze", &freezeRLs, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-cull-leafs", &devNoCulling, CVF_NO_ARCHIVE,0,1);
    C_VAR_INT("rend-dev-mobj-bbox", &devMobjBBox, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-mobj-show-vlights", &devMobjVLights, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-polyobj-bbox", &devPolyobjBBox, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-light-mod", &devLightModRange, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-tex-showfix", &devNoTexFix, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-blockmap-debug", &bmapShowDebug, CVF_NO_ARCHIVE, 0, 4);
    C_VAR_FLOAT("rend-dev-blockmap-debug-size", &bmapDebugSize, CVF_NO_ARCHIVE, .1f, 100);
    C_VAR_BYTE("rend-dev-vertex-show-indices", &devVertexIndices, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-vertex-show-bars", &devVertexBars, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-surface-show-vectors", &devSurfaceVectors, CVF_NO_ARCHIVE, 0, 7);
    C_VAR_BYTE("rend-dev-soundorigins", &devSoundOrigins, CVF_NO_ARCHIVE, 0, 7);

    RL_Register();
    LO_Register();
    Rend_DecorRegister();
    SB_Register();
    LG_Register();
    Rend_SkyRegister();
    Rend_ModelRegister();
    Rend_ParticleRegister();
    Rend_RadioRegister();
    Rend_SpriteRegister();
    Rend_ConsoleRegister();
}

/**
 * Approximated! The Z axis aspect ratio is corrected.
 */
coord_t Rend_PointDist3D(coord_t const point[3])
{
    return M_ApproxDistance3(vOrigin[VX] - point[VX], vOrigin[VZ] - point[VY], 1.2 * (vOrigin[VY] - point[VZ]));
}

void Rend_Init(void)
{
    C_Init();
    RL_Init();
    Rend_SkyInit();
}

void Rend_Shutdown(void)
{
    RL_Shutdown();
}

/// World/map renderer reset.
void Rend_Reset(void)
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
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

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

static __inline double viewFacingDot(coord_t v1[2], coord_t v2[2])
{
    // The dot product.
    return (v1[VY] - v2[VY]) * (v1[VX] - vOrigin[VX]) + (v2[VX] - v1[VX]) * (v1[VY] - vOrigin[VZ]);
}

static int C_DECL DivSortAscend(const void* e1, const void* e2)
{
    coord_t f1 = *(coord_t*) e1, f2 = *(coord_t*) e2;
    if(f1 > f2) return 1;
    if(f2 > f1) return -1;
    return 0;
}

static int C_DECL DivSortDescend(const void* e1, const void* e2)
{
    coord_t f1 = *(coord_t*) e1, f2 = *(coord_t*) e2;
    if(f1 > f2) return -1;
    if(f2 > f1) return 1;
    return 0;
}

void Rend_VertexColorsGlow(ColorRawf* colors, size_t num, float glow)
{
    size_t i;
    for(i = 0; i < num; ++i)
    {
        ColorRawf* c = &colors[i];
        c->rgba[CR] = c->rgba[CG] = c->rgba[CB] = glow;
    }
}

void Rend_VertexColorsAlpha(ColorRawf* colors, size_t num, float alpha)
{
    size_t i;
    for(i = 0; i < num; ++i)
    {
        colors[i].rgba[CA] = alpha;
    }
}

void Rend_ApplyTorchLight(float* color, float distance)
{
    ddplayer_t* ddpl = &viewPlayer->shared;

    // Disabled?
    if(!ddpl->fixedColorMap) return;

    // Check for torch.
    if(distance < 1024)
    {
        // Colormap 1 is the brightest. I'm guessing 16 would be
        // the darkest.
        int ll = 16 - ddpl->fixedColorMap;
        float d = (1024 - distance) / 1024.0f * ll / 15.0f;

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

static void lightVertex(ColorRawf* color, const rvertex_t* vtx, float lightLevel,
    const float* ambientColor)
{
    float dist = Rend_PointDist2D(vtx->pos);
    float lightVal = R_DistAttenuateLightLevel(dist, lightLevel);

    // Add extra light.
    lightVal += R_ExtraLightDelta();

    Rend_ApplyLightAdaptation(&lightVal);

    // Mix with the surface color.
    color->rgba[CR] = lightVal * ambientColor[CR];
    color->rgba[CG] = lightVal * ambientColor[CG];
    color->rgba[CB] = lightVal * ambientColor[CB];
}

static void lightVertices(size_t num, ColorRawf* colors, const rvertex_t* verts,
    float lightLevel, const float* ambientColor)
{
    size_t i;
    for(i = 0; i < num; ++i)
    {
        lightVertex(colors+i, verts+i, lightLevel, ambientColor);
    }
}

void Rend_VertexColorsApplyTorchLight(ColorRawf* colors, const rvertex_t* vertices,
    size_t numVertices)
{
    ddplayer_t* ddpl = &viewPlayer->shared;
    size_t i;

    // Disabled?
    if(!ddpl->fixedColorMap) return;

    for(i = 0; i < numVertices; ++i)
    {
        const rvertex_t* vtx = &vertices[i];
        ColorRawf* c = &colors[i];
        Rend_ApplyTorchLight(c->rgba, Rend_PointDist2D(vtx->pos));
    }
}

static void markSideDefSectionsPVisible(HEdge* hedge)
{
    const Plane* fceil, *bceil, *ffloor, *bfloor;
    SideDef* side;
    uint i;

    if(!hedge->lineDef || !hedge->lineDef->L_side(hedge->side)) return;
    side = hedge->lineDef->L_side(hedge->side);

    for(i = 0; i < 3; ++i)
    {
        side->sections[i].inFlags |= SUIF_PVIS;
    }

    if(!hedge->lineDef->L_backside)
    {
        side->SW_topsurface   .inFlags &= ~SUIF_PVIS;
        side->SW_bottomsurface.inFlags &= ~SUIF_PVIS;
        return;
    }

    fceil  = hedge->lineDef->L_sector(hedge->side  )->SP_plane(PLN_CEILING);
    ffloor = hedge->lineDef->L_sector(hedge->side  )->SP_plane(PLN_FLOOR);
    bceil  = hedge->lineDef->L_sector(hedge->side^1)->SP_plane(PLN_CEILING);
    bfloor = hedge->lineDef->L_sector(hedge->side^1)->SP_plane(PLN_FLOOR);

    // Middle.
    if(!side->SW_middlematerial || !Material_IsDrawable(side->SW_middlematerial) || side->SW_middlergba[3] <= 0)
        side->SW_middlesurface.inFlags &= ~SUIF_PVIS;

    // Top.
    if((!devRendSkyMode && Surface_IsSkyMasked(&fceil->surface) && Surface_IsSkyMasked(&bceil->surface)) ||
       //(!devRendSkyMode && Surface_IsSkyMasked(&bceil->surface) && (side->SW_topsurface.inFlags & SUIF_FIX_MISSING_MATERIAL)) ||
       (fceil->visHeight <= bceil->visHeight))
        side->SW_topsurface   .inFlags &= ~SUIF_PVIS;

    // Bottom.
    if((!devRendSkyMode && Surface_IsSkyMasked(&ffloor->surface) && Surface_IsSkyMasked(&bfloor->surface)) ||
       //(!devRendSkyMode && Surface_IsSkyMasked(&bfloor->surface) && (side->SW_bottomsurface.inFlags & SUIF_FIX_MISSING_MATERIAL)) ||
       (ffloor->visHeight >= bfloor->visHeight))
        side->SW_bottomsurface.inFlags &= ~SUIF_PVIS;
}

static void Rend_MarkSideDefSectionsPVisible(HEdge* hedge)
{
    assert(hedge);
    /// @todo Do we really need to do this for both sides? Surely not.
    markSideDefSectionsPVisible(hedge);
    if(hedge->twin)
        markSideDefSectionsPVisible(hedge->twin);
}

/**
 * @return  @true if there is a division at the specified height.
 */
static int checkDiv(walldiv_t* div, float height)
{
    uint i;
    for(i = 0; i < div->num; ++i)
    {
        if(div->pos[i] == height)
            return true;
    }
    return false;
}

static void doCalcSegDivisions(walldiv_t* div, const LineDef* line,
    boolean backSide, const Sector* frontSec, coord_t bottomZ, coord_t topZ,
    boolean doRight)
{
    uint i, j;
    LineDef* iter;
    Sector* scanSec;
    lineowner_t* base, *own;
    boolean clockwise = !doRight;
    boolean stopScan = false;

    if(bottomZ >= topZ)
        return; // Obviously no division.

    // Retrieve the start owner node.
    base = own = R_GetVtxLineOwner(line->L_v(backSide^doRight), line);
    do
    {
        own = own->link[clockwise];

        if(own == base)
        {
            stopScan = true;
        }
        else
        {
            iter = own->lineDef;

            if(LINE_SELFREF(iter))
                continue;

            i = 0;
            do
            {   // First front, then back.
                scanSec = NULL;
                if(!i && iter->L_frontside && iter->L_frontsector != frontSec)
                    scanSec = iter->L_frontsector;
                else if(i && iter->L_backside && iter->L_backsector != frontSec)
                    scanSec = iter->L_backsector;

                if(scanSec)
                {
                    if(scanSec->SP_ceilvisheight - scanSec->SP_floorvisheight > 0)
                    {
                        for(j = 0; j < scanSec->planeCount && !stopScan; ++j)
                        {
                            Plane* pln = scanSec->SP_plane(j);

                            if(pln->visHeight > bottomZ && pln->visHeight < topZ)
                            {
                                if(!checkDiv(div, pln->visHeight))
                                {
                                    div->pos[div->num++] = pln->visHeight;

                                    // Have we reached the div limit?
                                    if(div->num == RL_MAX_DIVS)
                                        stopScan = true;
                                }
                            }

                            if(!stopScan)
                            {
                                // Clip a range bound to this height?
                                if(pln->type == PLN_FLOOR && pln->visHeight > bottomZ)
                                    bottomZ = pln->visHeight;
                                else if(pln->type == PLN_CEILING && pln->visHeight < topZ)
                                    topZ = pln->visHeight;

                                // All clipped away?
                                if(bottomZ >= topZ)
                                    stopScan = true;
                            }
                        }
                    }
                    else
                    {
                        /**
                         * A zero height sector is a special case. In this
                         * instance, the potential division is at the height
                         * of the back ceiling. This is because elsewhere
                         * we automatically fix the case of a floor above a
                         * ceiling by lowering the floor.
                         */
                        coord_t z = scanSec->SP_ceilvisheight;

                        if(z > bottomZ && z < topZ)
                        {
                            if(!checkDiv(div, z))
                            {
                                div->pos[div->num++] = z;

                                // Have we reached the div limit?
                                if(div->num == RL_MAX_DIVS)
                                    stopScan = true;
                            }
                        }
                    }
                }
            } while(!stopScan && ++i < 2);

            // Stop the scan when a single sided line is reached.
            if(!iter->L_frontside || !iter->L_backside)
                stopScan = true;
        }
    } while(!stopScan);
}

static void calcSegDivisions(walldiv_t* div, const HEdge* hedge,
    const Sector* frontSec, coord_t bottomZ, coord_t topZ, boolean doRight)
{
    SideDef* side;

    div->num = 0;

    // Polyobj hedges are never split.
    if(hedge->lineDef && (hedge->lineDef->inFlags & LF_POLYOBJ)) return;

    // Only hedges at sidedef ends can/should be split.
    side = HEDGE_SIDEDEF(hedge);
    if(!((hedge == side->hedgeLeft  && !doRight) ||
         (hedge == side->hedgeRight &&  doRight)))
        return;

    doCalcSegDivisions(div, hedge->lineDef, hedge->side, frontSec, bottomZ, topZ, doRight);
}

/**
 * Division will only happen if it must be done.
 */
static void applyWallHeightDivision(walldiv_t* divs, const HEdge* hedge,
    const Sector* frontsec, coord_t low, coord_t hi)
{
    walldiv_t* div;
    uint i;

    // Mini-segs are never drawn.
    if(!hedge->lineDef) return;

    for(i = 0; i < 2; ++i)
    {
        div = &divs[i];
        calcSegDivisions(div, hedge, frontsec, low, hi, i);

        // We need to sort the divisions for the renderer.
        if(div->num > 1)
        {
            // Sorting is required. This shouldn't take too long...
            // There seldom are more than one or two divisions.
            qsort(div->pos, div->num, sizeof(*div->pos),
                  i!=0 ? DivSortDescend : DivSortAscend);
        }

#ifdef RANGECHECK
        { uint k;
        for(k = 0; k < div->num; ++k)
        {
            if(div->pos[k] > hi || div->pos[k] < low)
            {
                Con_Error("DivQuad: i=%i, pos (%f), hi (%f), low (%f), num=%i\n",
                          i, div->pos[k], hi, low, div->num);
            }
        }}
#endif
    }
}

static void selectSurfaceColors(const float** topColor,
    const float** bottomColor, SideDef* side, SideDefSection section)
{
    switch(section)
    {
    case SS_MIDDLE:
        if(side->flags & SDF_BLENDMIDTOTOP)
        {
            *topColor = side->SW_toprgba;
            *bottomColor = side->SW_middlergba;
        }
        else if(side->flags & SDF_BLENDMIDTOBOTTOM)
        {
            *topColor = side->SW_middlergba;
            *bottomColor = side->SW_bottomrgba;
        }
        else
        {
            *topColor = side->SW_middlergba;
            *bottomColor = NULL;
        }
        break;

    case SS_TOP:
        if(side->flags & SDF_BLENDTOPTOMID)
        {
            *topColor = side->SW_toprgba;
            *bottomColor = side->SW_middlergba;
        }
        else
        {
            *topColor = side->SW_toprgba;
            *bottomColor = NULL;
        }
        break;

    case SS_BOTTOM:
        if(side->flags & SDF_BLENDBOTTOMTOMID)
        {
            *topColor = side->SW_middlergba;
            *bottomColor = side->SW_bottomrgba;
        }
        else
        {
            *topColor = side->SW_bottomrgba;
            *bottomColor = NULL;
        }
        break;

    default:
        break;
    }
}

int RIT_FirstDynlightIterator(const dynlight_t* dyn, void* paramaters)
{
    const dynlight_t** ptr = (const dynlight_t**)paramaters;
    *ptr = dyn;
    return 1; // Stop iteration.
}

static __inline const materialvariantspecification_t*
mapSurfaceMaterialSpec(int wrapS, int wrapT)
{
    return Materials_VariantSpecificationForContext(MC_MAPSURFACE, 0, 0, 0, 0,
                                                    wrapS, wrapT, -1, -1, -1,
                                                    true, true, false, false);
}

/**
 * This doesn't create a rendering primitive but a vissprite! The vissprite
 * represents the masked poly and will be rendered during the rendering
 * of sprites. This is necessary because all masked polygons must be
 * rendered back-to-front, or there will be alpha artifacts along edges.
 */
void Rend_AddMaskedPoly(const rvertex_t* rvertices, const ColorRawf* rcolors,
    coord_t wallLength, materialvariant_t* material, float const texOffset[2],
    blendmode_t blendMode, uint lightListIdx, float glow)
{
    vissprite_t* vis = R_NewVisSprite();
    int i, c;

    vis->type = VSPR_MASKED_WALL;
    vis->origin[VX] = (rvertices[0].pos[VX] + rvertices[3].pos[VX]) / 2;
    vis->origin[VY] = (rvertices[0].pos[VY] + rvertices[3].pos[VY]) / 2;
    vis->origin[VZ] = (rvertices[0].pos[VZ] + rvertices[3].pos[VZ]) / 2;
    vis->distance = Rend_PointDist2D(vis->origin);

    if(texOffset)
    {
        VS_WALL(vis)->texOffset.x = texOffset[VX];
        VS_WALL(vis)->texOffset.y = texOffset[VY];
    }

    // Masked walls are sometimes used for special effects like arcs,
    // cobwebs and bottoms of sails. In order for them to look right,
    // we need to disable texture wrapping on the horizontal axis (S).
    // Most masked walls need wrapping, though. What we need to do is
    // look at the texture coordinates and see if they require texture
    // wrapping.
    if(renderTextures)
    {
        const materialsnapshot_t* ms = Materials_PrepareVariant(material);
        int wrapS = GL_REPEAT, wrapT = GL_REPEAT;

        VS_WALL(vis)->texCoord[0][VX] = VS_WALL(vis)->texOffset.x / ms->size.width;
        VS_WALL(vis)->texCoord[1][VX] = VS_WALL(vis)->texCoord[0][VX] + wallLength / ms->size.width;
        VS_WALL(vis)->texCoord[0][VY] = VS_WALL(vis)->texOffset.y / ms->size.height;
        VS_WALL(vis)->texCoord[1][VY] = VS_WALL(vis)->texCoord[0][VY] +
                (rvertices[3].pos[VZ] - rvertices[0].pos[VZ]) / ms->size.height;

        if(ms && !ms->isOpaque)
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

        // Choose an appropriate variant.
        /// @fixme Can result in multiple variants being prepared.
        ///        This decision should be made earlier (in rendHEdgeSection()).
        material = Materials_ChooseVariant(MaterialVariant_GeneralCase(material),
                                           mapSurfaceMaterialSpec(wrapS, wrapT), true, true);
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

    /// @fixme Semitransparent masked polys arn't lit atm
    if(glow < 1 && lightListIdx && numTexUnits > 1 && envModAdd &&
       !(rcolors[0].rgba[CA] < 1))
    {
        const dynlight_t* dyn = NULL;

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
            VS_WALL(vis)->modColor[c] = dyn->color.rgba[c];
    }
    else
    {
        VS_WALL(vis)->modTex = 0;
    }
}

static void quadTexCoords(rtexcoord_t* tc, const rvertex_t* rverts,
    coord_t wallLength, coord_t const topLeft[3])
{
    tc[0].st[0] = tc[1].st[0] =
        rverts[0].pos[VX] - topLeft[VX];
    tc[3].st[1] = tc[1].st[1] =
        rverts[0].pos[VY] - topLeft[VY];
    tc[3].st[0] = tc[2].st[0] = tc[0].st[0] + wallLength;
    tc[2].st[1] = tc[3].st[1] + (rverts[1].pos[VZ] - rverts[0].pos[VZ]);
    tc[0].st[1] = tc[3].st[1] + (rverts[3].pos[VZ] - rverts[2].pos[VZ]);
}

static void quadLightCoords(rtexcoord_t* tc, float const s[2], float const t[2])
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

static void quadShinyTexCoords(rtexcoord_t* tc, const rvertex_t* topLeft,
    const rvertex_t* bottomRight, coord_t wallLength)
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

static void flatShinyTexCoords(rtexcoord_t* tc, const float xyz[3])
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

static float getSnapshots(const materialsnapshot_t** msA,
    const materialsnapshot_t** msB, material_t* mat)
{
    const materialvariantspecification_t* spec = mapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT);
    float interPos = 0;
    assert(msA);

    *msA = Materials_Prepare(mat, spec, true);

    // Smooth Texture Animation?
    if(msB)
    {
        materialvariant_t* variant = Materials_ChooseVariant(mat, spec, false, false);
        if(MaterialVariant_TranslationCurrent(variant) != MaterialVariant_TranslationNext(variant))
        {
            materialvariant_t* matB = MaterialVariant_TranslationNext(variant);

            // Prepare the inter texture.
            *msB = Materials_PrepareVariant(matB);

            // If fog is active, inter=0 is accepted as well. Otherwise
            // flickering may occur if the rendering passes don't match for
            // blended and unblended surfaces.
            if(!(!usingFog && MaterialVariant_TranslationPoint(matB) < 0))
            {
                interPos = MaterialVariant_TranslationPoint(matB);
            }
        }
        else
        {
            *msB = NULL;
        }
    }

    return interPos;
}

typedef struct {
    boolean         isWall;
    int             flags; /// @see rendpolyFlags
    blendmode_t     blendMode;
    pvec3d_t        texTL, texBR;
    const float*    texOffset;
    const float*    texScale;
    const float*    normal; // Surface normal.
    float           alpha;
    const float*    sectorLightLevel;
    float           surfaceLightLevelDL;
    float           surfaceLightLevelDR;
    const float*    sectorLightColor;
    const float*    surfaceColor;

    uint            lightListIdx; // List of lights that affect this poly.
    uint            shadowListIdx; // List of shadows that affect this poly.
    float           glowing;
    boolean         forceOpaque;

// For bias:
    void*           mapObject;
    uint            elmIdx;
    biassurface_t*  bsuf;

// Wall only (todo).
    const coord_t*  segLength;
    const float*    surfaceColor2; // Secondary color.
} rendworldpoly_params_t;

static boolean renderWorldPoly(rvertex_t* rvertices, uint numVertices,
                               const walldiv_t* divs,
                               const rendworldpoly_params_t* p,
                               const materialsnapshot_t* msA, float inter,
                               const materialsnapshot_t* msB)
{
    boolean useLights = false, useShadows = false, hasDynlights = false;
    rtexcoord_t* primaryCoords = NULL, *interCoords = NULL, *modCoords = NULL;
    uint realNumVertices = p->isWall && divs? 3 + divs[0].num + 3 + divs[1].num : numVertices;
    ColorRawf* rcolors;
    ColorRawf* shinyColors = NULL;
    rtexcoord_t* shinyTexCoords = NULL;
    float modTexTC[2][2] = {{ 0, 0 }, { 0, 0 }};
    ColorRawf modColor = { 0, 0, 0, 0 };
    DGLuint modTex = 0;
    float glowing = p->glowing;
    boolean drawAsVisSprite = false;

    // Map RTU configuration from prepared MaterialSnapshot(s).
    const rtexmapunit_t* primaryRTU       = (!(p->flags & RPF_SKYMASK))? &MSU(msA, MTU_PRIMARY) : NULL;
    const rtexmapunit_t* primaryDetailRTU = (r_detail && !(p->flags & RPF_SKYMASK) && Rtu_HasTexture(&MSU(msA, MTU_DETAIL)))? &MSU(msA, MTU_DETAIL) : NULL;
    const rtexmapunit_t* interRTU         = (!(p->flags & RPF_SKYMASK) && msB && Rtu_HasTexture(&MSU(msB, MTU_PRIMARY)))? &MSU(msB, MTU_PRIMARY) : NULL;
    const rtexmapunit_t* interDetailRTU   = (r_detail && !(p->flags & RPF_SKYMASK) && msB && Rtu_HasTexture(&MSU(msB, MTU_DETAIL)))? &MSU(msB, MTU_DETAIL) : NULL;
    const rtexmapunit_t* shinyRTU         = (useShinySurfaces && !(p->flags & RPF_SKYMASK) && Rtu_HasTexture(&MSU(msA, MTU_REFLECTION)))? &MSU(msA, MTU_REFLECTION) : NULL;
    const rtexmapunit_t* shinyMaskRTU     = (useShinySurfaces && !(p->flags & RPF_SKYMASK) && Rtu_HasTexture(&MSU(msA, MTU_REFLECTION)) && Rtu_HasTexture(&MSU(msA, MTU_REFLECTION_MASK)))? &MSU(msA, MTU_REFLECTION_MASK) : NULL;

    if(!p->forceOpaque && !(p->flags & RPF_SKYMASK) &&
       (!msA->isOpaque || p->alpha < 1 || p->blendMode > 0))
        drawAsVisSprite = true;

    rcolors = R_AllocRendColors(realNumVertices);
    primaryCoords = R_AllocRendTexCoords(realNumVertices);
    if(interRTU)
        interCoords = R_AllocRendTexCoords(realNumVertices);

    if(!(p->flags & RPF_SKYMASK))
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

        if(glowing < 1)
        {
            useLights  = (p->lightListIdx ? true : false);
            useShadows = (p->shadowListIdx? true : false);

            /**
             * If multitexturing is enabled and there is at least one
             * dynlight affecting this surface, grab the paramaters needed
             * to draw it.
             */
            if(useLights && RL_IsMTexLights())
            {
                dynlight_t* dyn = NULL;

                LO_IterateProjections2(p->lightListIdx, RIT_FirstDynlightIterator, (void*)&dyn);

                modCoords = R_AllocRendTexCoords(realNumVertices);

                modTex = dyn->texture;
                modColor.red   = dyn->color.red;
                modColor.green = dyn->color.green;
                modColor.blue  = dyn->color.blue;
                modTexTC[0][0] = dyn->s[0];
                modTexTC[0][1] = dyn->s[1];
                modTexTC[1][0] = dyn->t[0];
                modTexTC[1][1] = dyn->t[1];
            }
        }
    }

    if(p->isWall)
    {
        // Primary texture coordinates.
        quadTexCoords(primaryCoords, rvertices, *p->segLength, p->texTL);

        // Blend texture coordinates.
        if(interRTU && !drawAsVisSprite)
            quadTexCoords(interCoords, rvertices, *p->segLength, p->texTL);

        // Shiny texture coordinates.
        if(shinyRTU && !drawAsVisSprite)
            quadShinyTexCoords(shinyTexCoords, &rvertices[1], &rvertices[2], *p->segLength);

        // First light texture coordinates.
        if(modTex && RL_IsMTexLights())
            quadLightCoords(modCoords, modTexTC[0], modTexTC[1]);
    }
    else
    {
        uint i;

        for(i = 0; i < numVertices; ++i)
        {
            const rvertex_t* vtx = &rvertices[i];
            float xyz[3];

            xyz[VX] = vtx->pos[VX] - p->texTL[VX];
            xyz[VY] = vtx->pos[VY] - p->texTL[VY];
            xyz[VZ] = vtx->pos[VZ] - p->texTL[VZ];

            // Primary texture coordinates.
            if(primaryRTU)
            {
                rtexcoord_t* tc = &primaryCoords[i];

                tc->st[0] =  xyz[VX];
                tc->st[1] = -xyz[VY];
            }

            // Blend primary texture coordinates.
            if(interRTU)
            {
                rtexcoord_t* tc = &interCoords[i];

                tc->st[0] =  xyz[VX];
                tc->st[1] = -xyz[VY];
            }

            // Shiny texture coordinates.
            if(shinyRTU)
            {
                flatShinyTexCoords(&shinyTexCoords[i], vtx->pos);
            }

            // First light texture coordinates.
            if(modTex && RL_IsMTexLights())
            {
                rtexcoord_t* tc = &modCoords[i];
                float width, height;

                width  = p->texBR[VX] - p->texTL[VX];
                height = p->texBR[VY] - p->texTL[VY];

                tc->st[0] = ((p->texBR[VX] - vtx->pos[VX]) / width  * modTexTC[0][0]) + (xyz[VX] / width  * modTexTC[0][1]);
                tc->st[1] = ((p->texBR[VY] - vtx->pos[VY]) / height * modTexTC[1][0]) + (xyz[VY] / height * modTexTC[1][1]);
            }
        }
    }

    // Light this polygon.
    if(!(p->flags & RPF_SKYMASK))
    {
        if(levelFullBright || !(glowing < 1))
        {   // Uniform colour. Apply to all vertices.
            Rend_VertexColorsGlow(rcolors, numVertices, *p->sectorLightLevel + (levelFullBright? 1 : glowing));
        }
        else
        {   // Non-uniform color.
            if(useBias && p->bsuf)
            {   // Do BIAS lighting for this poly.
                SB_RendPoly(rcolors, p->bsuf, rvertices, numVertices, p->normal, *p->sectorLightLevel, p->mapObject, p->elmIdx, p->isWall);
                if(glowing > 0)
                {
                    uint i;
                    for(i = 0; i < numVertices; ++i)
                    {
                        rcolors[i].rgba[CR] = MINMAX_OF(0, rcolors[i].rgba[CR] + glowing, 1);
                        rcolors[i].rgba[CG] = MINMAX_OF(0, rcolors[i].rgba[CG] + glowing, 1);
                        rcolors[i].rgba[CB] = MINMAX_OF(0, rcolors[i].rgba[CB] + glowing, 1);
                    }
                }
            }
            else
            {
                float llL = MINMAX_OF(0, *p->sectorLightLevel + p->surfaceLightLevelDL + glowing, 1);
                float llR = MINMAX_OF(0, *p->sectorLightLevel + p->surfaceLightLevelDR + glowing, 1);

                // Calculate the color for each vertex, blended with plane color?
                if(p->surfaceColor[0] < 1 || p->surfaceColor[1] < 1 || p->surfaceColor[2] < 1)
                {
                    float vColor[4];

                    // Blend sector light+color+surfacecolor
                    vColor[CR] = p->surfaceColor[CR] * p->sectorLightColor[CR];
                    vColor[CG] = p->surfaceColor[CG] * p->sectorLightColor[CG];
                    vColor[CB] = p->surfaceColor[CB] * p->sectorLightColor[CB];
                    vColor[CA] = 1;

                    if(p->isWall && llL != llR)
                    {
                        lightVertex(&rcolors[0], &rvertices[0], llL, vColor);
                        lightVertex(&rcolors[1], &rvertices[1], llL, vColor);
                        lightVertex(&rcolors[2], &rvertices[2], llR, vColor);
                        lightVertex(&rcolors[3], &rvertices[3], llR, vColor);
                    }
                    else
                    {
                        lightVertices(numVertices, rcolors, rvertices, llL, vColor);
                    }
                }
                else
                {   // Use sector light+color only.
                    if(p->isWall && llL != llR)
                    {
                        lightVertex(&rcolors[0], &rvertices[0], llL, p->sectorLightColor);
                        lightVertex(&rcolors[1], &rvertices[1], llL, p->sectorLightColor);
                        lightVertex(&rcolors[2], &rvertices[2], llR, p->sectorLightColor);
                        lightVertex(&rcolors[3], &rvertices[3], llR, p->sectorLightColor);
                    }
                    else
                    {
                        lightVertices(numVertices, rcolors, rvertices, llL, p->sectorLightColor);
                    }
                }

                // Bottom color (if different from top)?
                if(p->isWall && p->surfaceColor2)
                {
                    float vColor[4];

                    // Blend sector light+color+surfacecolor
                    vColor[CR] = p->surfaceColor2[CR] * p->sectorLightColor[CR];
                    vColor[CG] = p->surfaceColor2[CG] * p->sectorLightColor[CG];
                    vColor[CB] = p->surfaceColor2[CB] * p->sectorLightColor[CB];
                    vColor[CA] = 1;

                    lightVertex(&rcolors[0], &rvertices[0], llL, vColor);
                    lightVertex(&rcolors[2], &rvertices[2], llR, vColor);
                }
            }

            Rend_VertexColorsApplyTorchLight(rcolors, rvertices, numVertices);
        }

        if(shinyRTU && !drawAsVisSprite)
        {
            uint i;

            // Strength of the shine.
            for(i = 0; i < numVertices; ++i)
            {
                shinyColors[i].rgba[CR] = MAX_OF(rcolors[i].rgba[CR], msA->shinyMinColor[CR]);
                shinyColors[i].rgba[CG] = MAX_OF(rcolors[i].rgba[CG], msA->shinyMinColor[CG]);
                shinyColors[i].rgba[CB] = MAX_OF(rcolors[i].rgba[CB], msA->shinyMinColor[CB]);
                shinyColors[i].rgba[CA] = shinyRTU->opacity;
            }
        }

        // Apply uniform alpha.
        Rend_VertexColorsAlpha(rcolors, numVertices, p->alpha);
    }

    if(useLights || useShadows)
    {
        // Surfaces lit by dynamic lights may need to be rendered
        // differently than non-lit surfaces.
        float avgLightlevel = 0;

        // Determine the average light level of this rend poly,
        // if too bright; do not bother with lights.
        {uint i;
        for(i = 0; i < numVertices; ++i)
        {
            avgLightlevel += rcolors[i].rgba[CR];
            avgLightlevel += rcolors[i].rgba[CG];
            avgLightlevel += rcolors[i].rgba[CB];
        }}
        avgLightlevel /= (float) numVertices * 3;

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
        assert(p->isWall);

        /**
         * Masked polys (walls) get a special treatment (=> vissprite).
         * This is needed because all masked polys must be sorted (sprites
         * are masked polys). Otherwise there will be artifacts.
         */
        Rend_AddMaskedPoly(rvertices, rcolors, *p->segLength, msA->material,
                           p->texOffset, p->blendMode, p->lightListIdx, glowing);

        R_FreeRendTexCoords(primaryCoords);
        R_FreeRendColors(rcolors);

        if(interCoords) R_FreeRendTexCoords(interCoords);
        if(modCoords) R_FreeRendTexCoords(modCoords);
        if(shinyTexCoords) R_FreeRendTexCoords(shinyTexCoords);
        if(shinyColors) R_FreeRendColors(shinyColors);

        return false; // We HAD to use a vissprite, so it MUST not be opaque.
    }

    if(!(p->flags & RPF_SKYMASK) && useLights)
    {
        // Render all lights projected onto this surface.
        renderlightprojectionparams_t params;

        params.rvertices = rvertices;
        params.numVertices = numVertices;
        params.realNumVertices = realNumVertices;
        params.lastIdx = 0;
        params.isWall = p->isWall;
        params.divs = divs;
        params.texTL = p->texTL;
        params.texBR = p->texBR;

        hasDynlights = (0 != Rend_RenderLightProjections(p->lightListIdx, &params));
    }

    if(!(p->flags & RPF_SKYMASK) && useShadows)
    {
        // Render all shadows projected onto this surface.
        rendershadowprojectionparams_t params;

        params.rvertices = rvertices;
        params.numVertices = numVertices;
        params.realNumVertices = realNumVertices;
        params.isWall = p->isWall;
        params.texTL = p->texTL;
        params.texBR = p->texBR;
        params.divs = divs;

        Rend_RenderShadowProjections(p->shadowListIdx, &params);
    }

    // Map RTU state from the prepared texture units in the MaterialSnapshot(s).
    RL_LoadDefaultRtus();
    RL_MapRtu(RTU_PRIMARY, primaryRTU);
    RL_MapRtu(RTU_PRIMARY_DETAIL, primaryDetailRTU);
    RL_MapRtu(RTU_INTER, interRTU);
    RL_MapRtu(RTU_INTER_DETAIL, interDetailRTU);
    RL_MapRtu(RTU_REFLECTION, shinyRTU);
    RL_MapRtu(RTU_REFLECTION_MASK, shinyMaskRTU);

    /// @todo Avoid modifying the RTU write state for the purposes of primitive
    ///       specific translations by implementing these as arguments to the
    ///       RL_Add* family of functions.
    if(primaryRTU)
    {
        if(p->texOffset) RL_Rtu_TranslateOffsetv(RTU_PRIMARY, p->texOffset);
        if(p->texScale) RL_Rtu_ScaleST(RTU_PRIMARY, p->texScale);
    }

    if(primaryDetailRTU)
    {
        if(p->texOffset) RL_Rtu_TranslateOffsetv(RTU_PRIMARY_DETAIL, p->texOffset);
    }

    if(interRTU)
    {
        rtexmapunit_t rtu;
        memcpy(&rtu, interRTU, sizeof rtu);
        // Blend between the primary and inter textures.
        rtu.opacity = inter;
        if(p->texOffset) Rtu_TranslateOffsetv(&rtu, p->texOffset);
        if(p->texScale) Rtu_ScaleST(&rtu, p->texScale);
        RL_CopyRtu(RTU_INTER, &rtu);

        if(interDetailRTU)
        {
            memcpy(&rtu, interDetailRTU, sizeof rtu);
            // Blend between the primary and inter textures.
            rtu.opacity = inter;
            if(p->texOffset) Rtu_TranslateOffsetv(&rtu, p->texOffset);
            RL_CopyRtu(RTU_INTER_DETAIL, &rtu);
        }
    }

    if(shinyMaskRTU)
    {
        if(p->texOffset) RL_Rtu_TranslateOffsetv(RTU_REFLECTION_MASK, p->texOffset);
        if(p->texScale) RL_Rtu_ScaleST(RTU_REFLECTION_MASK, p->texScale);
    }

    // Write multiple polys depending on rend params.
    if(p->isWall && divs)
    {
        float               bL, tL, bR, tR;
        rvertex_t           origVerts[4];
        ColorRawf            origColors[4];
        rtexcoord_t         origTexCoords[4];

        /**
         * Need to swap indices around into fans set the position
         * of the division vertices, interpolate texcoords and
         * color.
         */

        memcpy(origVerts, rvertices, sizeof(rvertex_t) * 4);
        memcpy(origTexCoords, primaryCoords, sizeof(rtexcoord_t) * 4);
        memcpy(origColors, rcolors, sizeof(ColorRawf) * 4);

        bL = origVerts[0].pos[VZ];
        tL = origVerts[1].pos[VZ];
        bR = origVerts[2].pos[VZ];
        tR = origVerts[3].pos[VZ];

        R_DivVerts(rvertices, origVerts, divs);
        R_DivTexCoords(primaryCoords, origTexCoords, divs, bL, tL, bR, tR);
        R_DivVertColors(rcolors, origColors, divs, bL, tL, bR, tR);

        if(interCoords)
        {
            rtexcoord_t origTexCoords2[4];

            memcpy(origTexCoords2, interCoords, sizeof(rtexcoord_t) * 4);
            R_DivTexCoords(interCoords, origTexCoords2, divs, bL, tL, bR, tR);
        }

        if(modCoords)
        {
            rtexcoord_t origTexCoords5[4];

            memcpy(origTexCoords5, modCoords, sizeof(rtexcoord_t) * 4);
            R_DivTexCoords(modCoords, origTexCoords5, divs, bL, tL, bR, tR);
        }

        if(shinyTexCoords)
        {
            rtexcoord_t origShinyTexCoords[4];

            memcpy(origShinyTexCoords, shinyTexCoords, sizeof(rtexcoord_t) * 4);
            R_DivTexCoords(shinyTexCoords, origShinyTexCoords, divs, bL, tL, bR, tR);
        }

        if(shinyColors)
        {
            ColorRawf origShinyColors[4];
            memcpy(origShinyColors, shinyColors, sizeof(ColorRawf) * 4);
            R_DivVertColors(shinyColors, origShinyColors, divs, bL, tL, bR, tR);
        }

        RL_AddPolyWithCoordsModulationReflection(PT_FAN, p->flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
            3 + divs[1].num, rvertices + 3 + divs[0].num, rcolors + 3 + divs[0].num,
            primaryCoords + 3 + divs[0].num, interCoords? interCoords + 3 + divs[0].num : NULL,
            modTex, &modColor, modCoords? modCoords + 3 + divs[0].num : NULL,
            shinyColors + 3 + divs[0].num, shinyTexCoords? shinyTexCoords + 3 + divs[0].num : NULL,
            shinyMaskRTU? primaryCoords + 3 + divs[0].num : NULL);

        RL_AddPolyWithCoordsModulationReflection(PT_FAN, p->flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
            3 + divs[0].num, rvertices, rcolors,
            primaryCoords, interCoords,
            modTex, &modColor, modCoords,
            shinyColors, shinyTexCoords, shinyMaskRTU? primaryCoords : NULL);
    }
    else
    {
        RL_AddPolyWithCoordsModulationReflection(p->isWall? PT_TRIANGLE_STRIP : PT_FAN, p->flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
            numVertices, rvertices, rcolors,
            primaryCoords, interCoords,
            modTex, &modColor, modCoords,
            shinyColors, shinyTexCoords, shinyMaskRTU? primaryCoords : NULL);
    }

    R_FreeRendTexCoords(primaryCoords);
    if(interCoords)
        R_FreeRendTexCoords(interCoords);
    if(modCoords)
        R_FreeRendTexCoords(modCoords);
    if(shinyTexCoords)
        R_FreeRendTexCoords(shinyTexCoords);
    R_FreeRendColors(rcolors);
    if(shinyColors)
        R_FreeRendColors(shinyColors);

    return (p->forceOpaque || (p->flags & RPF_SKYMASK) ||
        !(p->alpha < 1 || !msA->isOpaque || p->blendMode > 0));
}

static boolean doRenderSeg(HEdge* hedge,
                           coord_t const from[2], coord_t const to[2],
                           coord_t bottom, coord_t top, const pvec3f_t normal,
                           float alpha,
                           const float* lightLevel, float lightLevelDL,
                           float lightLevelDR,
                           const float* lightColor,
                           uint lightListIdx,
                           uint shadowListIdx,
                           const walldiv_t* divs,
                           boolean skyMask,
                           boolean addFakeRadio,
                           vec3d_t texTL, vec3d_t texBR,
                           float const texOffset[2],
                           float const texScale[2],
                           blendmode_t blendMode,
                           const float* color, const float* color2,
                           biassurface_t* bsuf, uint elmIdx /*tmp*/,
                           const materialsnapshot_t* msA, float inter,
                           const materialsnapshot_t* msB,
                           boolean isTwosidedMiddle)
{
    rendworldpoly_params_t params;
    SideDef*            side = (hedge->lineDef? HEDGE_SIDEDEF(hedge) : NULL);
    rvertex_t*          rvertices;

    // Init the params.
    memset(&params, 0, sizeof(params));

    params.flags = RPF_DEFAULT | (skyMask? RPF_SKYMASK : 0);
    params.isWall = true;
    params.segLength = &hedge->length;
    params.forceOpaque = (alpha < 0? true : false);
    params.alpha = (alpha < 0? 1 : alpha);
    params.mapObject = hedge;
    params.elmIdx = elmIdx;
    params.bsuf = bsuf;
    params.normal = normal;
    params.texTL = texTL;
    params.texBR = texBR;
    params.sectorLightLevel = lightLevel;
    params.surfaceLightLevelDL = lightLevelDL;
    params.surfaceLightLevelDR = lightLevelDR;
    params.sectorLightColor = lightColor;
    params.surfaceColor = color;
    params.surfaceColor2 = color2;
    params.glowing = msA? msA->glowing : 0;
    params.blendMode = blendMode;
    params.texOffset = texOffset;
    params.texScale = texScale;
    params.lightListIdx = lightListIdx;
    params.shadowListIdx = shadowListIdx;

    if(divs)
    {
        // Allocate enough vertices for the divisions too.
        rvertices = R_AllocRendVertices(3 + divs[0].num + 3 + divs[1].num);
    }
    else
    {
        rvertices = R_AllocRendVertices(4);
    }

    // Vertex coords.
    // Bottom Left.
    V3f_Set(rvertices[0].pos, from[VX], from[VY], bottom);

    // Top Left.
    V3f_Set(rvertices[1].pos, from[VX], from[VY], top);

    // Bottom Right.
    V3f_Set(rvertices[2].pos, to[VX], to[VY], bottom);

    // Top Right.
    V3f_Set(rvertices[3].pos, to[VX], to[VY], top);

    // Draw this hedge.
    if(renderWorldPoly(rvertices, 4, divs, &params, msA, inter, msB))
    {   // Drawn poly was opaque.
        // Render Fakeradio polys for this hedge?
        if(!(params.flags & RPF_SKYMASK) && addFakeRadio)
        {
            rendsegradio_params_t radioParams;
            float ll;

            radioParams.linedefLength = &hedge->lineDef->length;
            radioParams.botCn = side->bottomCorners;
            radioParams.topCn = side->topCorners;
            radioParams.sideCn = side->sideCorners;
            radioParams.spans = side->spans;
            radioParams.segOffset = &hedge->offset;
            radioParams.segLength = &hedge->length;
            radioParams.frontSec = hedge->sector;

            if(!isTwosidedMiddle && !(hedge->twin && !HEDGE_SIDEDEF(hedge->twin)))
            {
                radioParams.backSec = HEDGE_BACK_SECTOR(hedge);
            }
            else
            {
                radioParams.backSec = NULL;
            }

            /// @kludge Revert the vertex coords as they may have been changed
            ///         due to height divisions.

            // Bottom Left.
            V3f_Set(rvertices[0].pos, from[VX], from[VY], bottom);

            // Top Left.
            V3f_Set(rvertices[1].pos, from[VX], from[VY], top);

            // Bottom Right.
            V3f_Set(rvertices[2].pos, to[VX], to[VY], bottom);

            // Top Right.
            V3f_Set(rvertices[3].pos, to[VX], to[VY], top);

            // kludge end.

            ll = *lightLevel;
            Rend_ApplyLightAdaptation(&ll);
            if(ll > 0)
            {
                // Determine the shadow properties.
                /// @fixme Make cvars out of constants.
                radioParams.shadowSize = 2 * (8 + 16 - ll * 16);
                radioParams.shadowDark = Rend_RadioCalcShadowDarkness(ll);

                if(radioParams.shadowSize > 0)
                {
                    // Shadows are black.
                    radioParams.shadowRGB[CR] = radioParams.shadowRGB[CG] = radioParams.shadowRGB[CB] = 0;
                    Rend_RadioSegSection(rvertices, divs, &radioParams);
                }
            }
        }

        R_FreeRendVertices(rvertices);
        return true; // Clip with this solid hedge.
    }

    R_FreeRendVertices(rvertices);

    return false; // Do not clip with this.
}

static void renderPlane(BspLeaf* bspLeaf, planetype_t type, coord_t height,
    vec3f_t tangent, vec3f_t bitangent, vec3f_t normal,
    material_t* inMat, int sufFlags, short sufInFlags,
    const float sufColor[4], blendmode_t blendMode,
    vec3d_t texTL, vec3d_t texBR,
    float const texOffset[2], float const texScale[2],
    boolean skyMasked,
    boolean addDLights, boolean addMobjShadows,
    biassurface_t* bsuf, uint elmIdx /*tmp*/,
    int texMode /*tmp*/, boolean flipSurfaceNormal)
{
    float               inter = 0;
    rendworldpoly_params_t params;
    uint                numVertices = BspLeaf_NumFanVertices(bspLeaf);
    rvertex_t*          rvertices;
    boolean             blended = false;
    Sector*             sec = bspLeaf->sector;
    material_t*         mat = NULL;
    const materialsnapshot_t* msA = NULL, *msB = NULL;

    memset(&params, 0, sizeof(params));

    params.flags = RPF_DEFAULT;
    params.isWall = false;
    params.mapObject = bspLeaf;
    params.elmIdx = elmIdx;
    params.bsuf = bsuf;
    params.normal = normal;
    params.texTL = texTL;
    params.texBR = texBR;
    params.sectorLightLevel = &sec->lightLevel;
    params.sectorLightColor = R_GetSectorLightColor(sec);
    params.surfaceLightLevelDL = params.surfaceLightLevelDR = 0;
    params.surfaceColor = sufColor;
    params.texOffset = texOffset;
    params.texScale = texScale;

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

        if(type != PLN_MID)
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
            params.alpha = sufColor[CA];
        }
    }

    rvertices = R_AllocRendVertices(numVertices);
    BspLeaf_PrepareFan(bspLeaf, !(normal[VZ] > 0) ^ flipSurfaceNormal, height, rvertices, numVertices);

    if(!(params.flags & RPF_SKYMASK))
    {
        // Smooth Texture Animation?
        if(smoothTexAnim)
            blended = true;

        inter = getSnapshots(&msA, blended? &msB : NULL, mat);

        if(texMode != 2)
        {
            params.glowing = msA->glowing;
        }
        else
        {
            Surface* suf = &bspLeaf->sector->planes[elmIdx]->surface;
            const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
                MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
            const materialsnapshot_t* ms = Materials_Prepare(suf->material, spec, true);
            params.glowing = ms->glowing;
        }

        // Dynamic lights?
        if(addDLights && params.glowing < 1 && !(!useDynlights && !useWallGlow))
        {
            params.lightListIdx = LO_ProjectToSurface((PLF_NO_PLANE | (type == PLN_FLOOR? PLF_TEX_FLOOR : PLF_TEX_CEILING)), bspLeaf, 1,
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

    renderWorldPoly(rvertices, numVertices, NULL, &params, msA, inter, blended? msB : NULL);

    R_FreeRendVertices(rvertices);
}

static void Rend_RenderPlane(BspLeaf* bspLeaf, planetype_t type, coord_t height,
    const_pvec3f_t _tangent, const_pvec3f_t _bitangent, const_pvec3f_t _normal,
    material_t* inMat, int sufFlags, short sufInFlags,
    float const sufColor[4], blendmode_t blendMode,
    float const texOffset[2], float const texScale[2],
    boolean skyMasked, boolean addDLights, boolean addMobjShadows,
    biassurface_t* bsuf, uint elmIdx /*tmp*/,
    int texMode /*tmp*/, boolean flipNormal, boolean clipBackFacing)
{
    vec3f_t vec, tangent, bitangent, normal;

    // Must have a visible surface.
    if(!inMat || !Material_IsDrawable(inMat)) return;

    V3f_Set(vec, vOrigin[VX] - bspLeaf->midPoint[VX], vOrigin[VZ] - bspLeaf->midPoint[VY], vOrigin[VY] - height);

    /**
     * Flip surface tangent space vectors according to the z positon of the viewer relative
     * to the plane height so that the plane is always visible.
     */
    V3f_Copy(tangent, _tangent);
    V3f_Copy(bitangent, _bitangent);
    V3f_Copy(normal, _normal);
    if(flipNormal)
    {
        /// @fixme This is obviously wrong, do this correctly!
        tangent[VZ] *= -1;
        bitangent[VZ] *= -1;
        normal[VZ] *= -1;
    }

    // Don't bother with planes facing away from the camera.
    if(!(clipBackFacing && !(V3f_DotProduct(vec, normal) < 0)))
    {
        coord_t texTL[3], texBR[3];

        // Set the texture origin, Y is flipped for the ceiling.
        V3d_Set(texTL, bspLeaf->aaBox.minX,
                       bspLeaf->aaBox.arvec2[type == PLN_FLOOR? 1 : 0][VY], height);
        V3d_Set(texBR, bspLeaf->aaBox.maxX,
                       bspLeaf->aaBox.arvec2[type == PLN_FLOOR? 0 : 1][VY], height);

        renderPlane(bspLeaf, type, height, tangent, bitangent, normal, inMat, sufFlags, sufInFlags,
                    sufColor, blendMode, texTL, texBR, texOffset, texScale,
                    skyMasked, addDLights, addMobjShadows, bsuf, elmIdx, texMode, flipNormal);
    }
}

static boolean rendHEdgeSection(HEdge* hedge, BspLeaf* bspLeaf, SideDefSection section,
    Surface* surface, coord_t const from[2], coord_t const to[2], coord_t bottom, coord_t top,
    float const texOffset[2], Sector* frontsec, boolean softSurface, boolean addDLights,
    boolean addMobjShadows, short sideFlags)
{
    boolean solidSeg = true;
    float alpha;

    if(!Surface_IsDrawable(surface)) return false;
    if(bottom >= top) return true;

    alpha = (section == SS_MIDDLE? surface->rgba[3] : 1.0f);

    if(section == SS_MIDDLE && softSurface)
    {
        mobj_t* mo = viewPlayer->shared.mo;
        const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

        /**
         * Can the player walk through this surface?
         * If the player is close enough we should NOT add a solid hedge otherwise HOM
         * would occur when they are directly on top of the line (e.g., passing through
         * an opaque waterfall).
         */

        if(viewData->current.origin[VZ] > bottom &&
           viewData->current.origin[VZ] < top)
        {
            LineDef* lineDef = hedge->lineDef;
            vec2d_t result;
            double pos = V2d_ProjectOnLine(result, mo->origin, lineDef->L_v1origin, lineDef->direction);

            if(pos > 0 && pos < 1)
            {
                coord_t distance, minDistance = mo->radius * .8f;
                coord_t delta[2];

                V2d_Subtract(delta, mo->origin, result);
                distance = M_ApproxDistance(delta[VX], delta[VY]);

                if(distance < minDistance)
                {
                    // Fade it out the closer the viewPlayer gets and clamp.
                    alpha = (alpha / minDistance) * distance;
                    alpha = MINMAX_OF(0, alpha, 1);
                }

                if(alpha < 1)
                    solidSeg = false;
            }
        }
    }

    if(alpha > 0)
    {
        uint lightListIdx = 0, shadowListIdx = 0;
        vec3d_t texTL, texBR;
        float texScale[2], inter = 0;
        walldiv_t           divs[2];
        boolean             forceOpaque = false;
        material_t*         mat = NULL;
        int                 rpFlags = RPF_DEFAULT;
        boolean             isTwoSided = (hedge->lineDef &&
            hedge->lineDef->L_frontside && hedge->lineDef->L_backside)? true:false;
        blendmode_t         blendMode = BM_NORMAL;
        boolean             addFakeRadio = false;
        const float*        color = NULL, *color2 = NULL;
        const materialsnapshot_t* msA = NULL, *msB = NULL;

        texScale[0] = ((surface->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
        texScale[1] = ((surface->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        V3d_Set(texTL, from[VX], from[VY], top);
        V3d_Set(texBR, to  [VX], to  [VY], bottom);

        // Determine which Material to use.
        if(devRendSkyMode && HEDGE_BACK_SECTOR(hedge) &&
           ((section == SS_BOTTOM && Surface_IsSkyMasked(&hedge->sector->SP_floorsurface) &&
                                     Surface_IsSkyMasked(&HEDGE_BACK_SECTOR(hedge)->SP_floorsurface)) ||
            (section == SS_TOP    && Surface_IsSkyMasked(&hedge->sector->SP_ceilsurface) &&
                                     Surface_IsSkyMasked(&HEDGE_BACK_SECTOR(hedge)->SP_ceilsurface))))
        {
            // Geometry not normally rendered however we do so in dev sky mode.
            mat = hedge->sector->SP_planematerial(section == SS_TOP? PLN_CEILING : PLN_FLOOR);
        }
        else
        {
            if(renderTextures == 2)
            {
                // Lighting debug mode; render using System:gray.
                mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":gray"));
            }
            else if(!surface->material ||
                    ((surface->inFlags & SUIF_FIX_MISSING_MATERIAL) && devNoTexFix))
            {
                // Missing material debug mode; render using System:missing.
                mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":missing"));
            }
            else
            {
                // Normal mode.
                mat = surface->material;
            }

            if(Material_IsSkyMasked(mat))
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
                    forceOpaque = true;
                }
            }
        }

        // Fill in the remaining params data.
        if(!(rpFlags & RPF_SKYMASK))
        {
            int surfaceFlags, surfaceInFlags;

            // Make any necessary adjustments to the surface flags to suit the
            // current texture mode.
            surfaceFlags = surface->flags;
            surfaceInFlags = surface->inFlags;
            if(renderTextures == 2)
            {
                surfaceInFlags &= ~(SUIF_FIX_MISSING_MATERIAL);
            }

            if(section != SS_MIDDLE || (section == SS_MIDDLE && !isTwoSided))
            {
                forceOpaque = true;
                blendMode = BM_NORMAL;
            }
            else
            {
                if(surface->blendMode == BM_NORMAL && noSpriteTrans)
                    blendMode = BM_ZEROALPHA; // "no translucency" mode
                else
                    blendMode = surface->blendMode;
            }

            addFakeRadio = !(surfaceInFlags & SUIF_NO_RADIO);
        }

        if(!(rpFlags & RPF_SKYMASK) && mat)
        {
            // Smooth Texture Animation?
            inter = getSnapshots(&msA, smoothTexAnim? &msB : NULL, mat);

            // Dynamic Lights?
            if(addDLights && msA->glowing < 1 && !(!useDynlights && !useWallGlow))
            {
                lightListIdx = LO_ProjectToSurface(((section == SS_MIDDLE && isTwoSided)? PLF_SORT_LUMINOSITY_DESC : 0), bspLeaf, 1,
                    texTL, texBR, HEDGE_SIDEDEF(hedge)->SW_middletangent, HEDGE_SIDEDEF(hedge)->SW_middlebitangent, HEDGE_SIDEDEF(hedge)->SW_middlenormal);
            }

            // Mobj shadows?
            if(addMobjShadows && msA->glowing < 1 && Rend_MobjShadowsEnabled())
            {
                // Glowing planes inversely diminish shadow strength.
                shadowListIdx = R_ProjectShadowsToSurface(bspLeaf, 1 - msA->glowing, texTL, texBR,
                    HEDGE_SIDEDEF(hedge)->SW_middletangent, HEDGE_SIDEDEF(hedge)->SW_middlebitangent, HEDGE_SIDEDEF(hedge)->SW_middlenormal);
            }

            addFakeRadio = ((addFakeRadio && msA->glowing == 0)? true : false);

            selectSurfaceColors(&color, &color2, HEDGE_SIDEDEF(hedge), section);
        }

        // Check for neighborhood division?
        divs[0].num = divs[1].num = 0;
        if(!(section == SS_MIDDLE && isTwoSided) &&
           !(hedge->lineDef->inFlags & LF_POLYOBJ))
        {
            applyWallHeightDivision(divs, hedge, frontsec, bottom, top);
        }

        {
        float deltaL, deltaR, diff;

        LineDef_LightLevelDelta(hedge->lineDef, hedge->side, &deltaL, &deltaR);

        // Linear interpolation of the linedef light deltas to the edges of the hedge.
        diff = deltaR - deltaL;
        deltaR = deltaL + ((hedge->offset + hedge->length) / hedge->lineDef->length) * diff;
        deltaL += (hedge->offset / hedge->lineDef->length) * diff;

        solidSeg = doRenderSeg(hedge,
                               from, to, bottom, top,
                               surface->normal, (forceOpaque? -1 : alpha),
                               &frontsec->lightLevel,
                               deltaL, deltaR,
                               R_GetSectorLightColor(frontsec),
                               lightListIdx, shadowListIdx,
                               (divs[0].num > 0 || divs[1].num > 0)? divs : NULL,
                               !!(rpFlags & RPF_SKYMASK),
                               addFakeRadio,
                               texTL, texBR, texOffset, texScale, blendMode,
                               color, color2,
                               hedge->bsuf[section], (uint) section,
                               msA, inter, msB,
                               (section == SS_MIDDLE && isTwoSided));
        }
    }

    return solidSeg;
}

static void reportLineDefDrawn(LineDef* line)
{
    int pid;
    if(!line) return;

    // Already been here?
    pid = viewPlayer - ddPlayers;
    if(line->mapped[pid]) return;

    // Mark as drawn.
    line->mapped[pid] = true;

    // Send a status report.
    if(gx.HandleMapObjectStatusReport)
    {
        gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED, GET_LINE_IDX(line), DMU_LINEDEF, &pid);
    }
}

/**
 * @param hedge  HEdge to draw wall surfaces for.
 * @param bspLeaf  BSP leaf from which to derive plane heights. May not be the
 *                 same as the leaf linked to the half-edge itself (in the case
 *                 of polyobject half-edges).
 */
static boolean Rend_RenderHEdge(HEdge* hedge, BspLeaf* bspLeaf)
{
    SideDef* side = HEDGE_SIDEDEF(hedge);
    coord_t ffloor, fceil;
    Surface* surface;
    float texOffset[2];
    boolean solid;

    assert(side);
    if(!(side->SW_middleinflags & SUIF_PVIS)) return false;

    // Only a "middle" section.
    assert(bspLeaf);
    ffloor = bspLeaf->sector->SP_floorvisheight;
    fceil = bspLeaf->sector->SP_ceilvisheight;
    surface = &side->SW_middlesurface;

    texOffset[0] = surface->visOffset[0] + (float)(hedge->offset);
    texOffset[1] = surface->visOffset[1];
    if(hedge->lineDef->flags & DDLF_DONTPEGBOTTOM)
        texOffset[1] += -(fceil - ffloor);

    Rend_RadioUpdateLinedef(hedge->lineDef, hedge->side);

    solid = rendHEdgeSection(hedge, bspLeaf, SS_MIDDLE, &side->SW_middlesurface,
                             hedge->HE_v1origin, hedge->HE_v2origin, ffloor, fceil,
                             texOffset,
                             /*temp >*/ bspLeaf->sector, /*< temp*/
                             false, true, true, side->flags);

    reportLineDefDrawn(hedge->lineDef);

    return solid;
}

boolean R_FindBottomTop(LineDef* lineDef, int side, SideDefSection section,
    coord_t matOffsetX, coord_t matOffsetY,
    const Plane* ffloor, const Plane* fceil,
    const Plane* bfloor, const Plane* bceil,
    boolean unpegBottom, boolean unpegTop,
    boolean stretchMiddle, boolean isSelfRef,
    coord_t* bottom, coord_t* top, float texOffset[2])
{
    switch(section)
    {
    case SS_TOP:
        *top = fceil->visHeight;
        // Can't go over front ceiling, would induce polygon flaws.
        if(bceil->visHeight < ffloor->visHeight)
            *bottom = ffloor->visHeight;
        else
            *bottom = bceil->visHeight;
        if(*top > *bottom)
        {
            texOffset[VX] = matOffsetX;
            texOffset[VY] = matOffsetY;

            // Align with normal middle texture?
            if(!unpegTop)
                texOffset[VY] += -(fceil->visHeight - bceil->visHeight);

            return true;
        }
        break;

    case SS_BOTTOM: {
        const boolean raiseToBackFloor = (Surface_IsSkyMasked(&fceil->surface) && Surface_IsSkyMasked(&bceil->surface) && fceil->visHeight < bceil->visHeight);
        coord_t t = bfloor->visHeight;

        *bottom = ffloor->visHeight;
        // Can't go over the back ceiling, would induce polygon flaws.
        if(bfloor->visHeight > bceil->visHeight)
            t = bceil->visHeight;

        // Can't go over front ceiling, would induce polygon flaws.
        // In the special case of a sky masked upper we must extend the bottom
        // section up to the height of the back floor.
        if(t > fceil->visHeight && !raiseToBackFloor)
            t = fceil->visHeight;
        *top = t;

        if(*top > *bottom)
        {
            texOffset[VX] = matOffsetX;
            texOffset[VY] = matOffsetY;

            if(bfloor->visHeight > fceil->visHeight)
                texOffset[VY] += -((raiseToBackFloor? t : fceil->visHeight) - bfloor->visHeight);

            // Align with normal middle texture?
            if(unpegBottom)
                texOffset[VY] += (raiseToBackFloor? t : fceil->visHeight) - bfloor->visHeight;

            return true;
        }
        break;
      }
    case SS_MIDDLE: {
        coord_t ftop, fbottom, vR_ZBottom, vR_ZTop;

        if(isSelfRef)
        {
            fbottom = MIN_OF(bfloor->visHeight, ffloor->visHeight);
            ftop    = MAX_OF(bceil->visHeight, fceil->visHeight);
        }
        else
        {
            fbottom = MAX_OF(bfloor->visHeight, ffloor->visHeight);
            ftop    = MIN_OF(bceil->visHeight, fceil->visHeight);
        }

        *bottom = vR_ZBottom = fbottom;
        *top    = vR_ZTop    = ftop;

        if(stretchMiddle)
        {
            if(*top > *bottom)
            {
                texOffset[VX] = matOffsetX;
                texOffset[VY] = 0;

                return true;
            }
        }
        else
        {
            boolean clipBottom = true, clipTop = true;

            if(!P_IsInVoid(viewPlayer))
            {
                if(Surface_IsSkyMasked(&ffloor->surface) && Surface_IsSkyMasked(&bfloor->surface))
                    clipBottom = false;
                if(Surface_IsSkyMasked(&fceil->surface)  && Surface_IsSkyMasked(&bceil->surface))
                    clipTop = false;
            }

            if(LineDef_MiddleMaterialCoords(lineDef, side, bottom, &vR_ZBottom, top,
                                            &vR_ZTop, &texOffset[VY], unpegBottom,
                                            clipTop, clipBottom))
            {
                texOffset[VX] = matOffsetX;
                if(!clipTop) texOffset[VY] = 0;

                return true;
            }
        }
        }
        break;
    }

    return false;
}

/**
 * Render wall sections for a HEdge belonging to a two-sided LineDef.
 */
static boolean Rend_RenderHEdgeTwosided(HEdge* hedge, BspLeaf* bspLeaf)
{
    coord_t bottom, top;
    float texOffset[2];
    Sector* frontSec, *backSec;
    SideDef* frontSide, *backSide;
    Plane* ffloor, *fceil, *bfloor, *bceil;
    LineDef* line;
    int solidSeg = false;

    frontSide = HEDGE_SIDEDEF(hedge);
    backSide = HEDGE_SIDEDEF(hedge->twin);
    frontSec = frontSide->sector;
    backSec = backSide->sector;
    line = hedge->lineDef;

    reportLineDefDrawn(line);

    if(backSec == frontSec &&
       !frontSide->SW_topmaterial && !frontSide->SW_bottommaterial &&
       !frontSide->SW_middlematerial)
       return false; // Ugh... an obvious wall hedge hack. Best take no chances...

    ffloor = bspLeaf->sector->SP_plane(PLN_FLOOR);
    fceil  = bspLeaf->sector->SP_plane(PLN_CEILING);
    bfloor = backSec->SP_plane(PLN_FLOOR);
    bceil  = backSec->SP_plane(PLN_CEILING);

    if((frontSide->SW_middleinflags & SUIF_PVIS) ||
       (frontSide->SW_topinflags & SUIF_PVIS) ||
       (frontSide->SW_bottominflags & SUIF_PVIS))
        Rend_RadioUpdateLinedef(hedge->lineDef, hedge->side);

    /**
     * Create the wall sections.
     *
     * We may need multiple wall sections.
     * Determine which parts of the segment are really visible.
     */

    // Middle section.
    if(frontSide->SW_middleinflags & SUIF_PVIS)
    {
        Surface* suf = &frontSide->SW_middlesurface;

        if(R_FindBottomTop(hedge->lineDef, hedge->side, SS_MIDDLE,
                           hedge->offset + suf->visOffset[VX], suf->visOffset[VY],
                           ffloor, fceil, bfloor, bceil,
                           (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
                           (line->flags & DDLF_DONTPEGTOP)? true : false,
                           (frontSide->flags & SDF_MIDDLE_STRETCH)? true : false,
                           LINE_SELFREF(line)? true : false,
                           &bottom, &top, texOffset))
        {
            solidSeg = rendHEdgeSection(hedge, bspLeaf, SS_MIDDLE, suf,
                                        hedge->HE_v1origin, hedge->HE_v2origin, bottom, top, texOffset,
                                        frontSec,
                                        (((viewPlayer->shared.flags & (DDPF_NOCLIP|DDPF_CAMERA)) ||
                                         !(line->flags & DDLF_BLOCKING))? true : false),
                                        true, false, frontSide->flags);
            if(solidSeg)
            {
                coord_t xbottom, xtop;
                Surface* suf = &frontSide->SW_middlesurface;

                if(LINE_SELFREF(line))
                {
                    xbottom = MIN_OF(bfloor->visHeight, ffloor->visHeight);
                    xtop    = MAX_OF(bceil->visHeight, fceil->visHeight);
                }
                else
                {
                    xbottom = MAX_OF(bfloor->visHeight, ffloor->visHeight);
                    xtop    = MIN_OF(bceil->visHeight, fceil->visHeight);
                }

                xbottom += suf->visOffset[VY];
                xtop    += suf->visOffset[VY];

                // Can we make this a solid segment?
                if(!(top >= xtop && bottom <= xbottom))
                     solidSeg = false;
            }
        }
    }

    // Upper section.
    if(frontSide->SW_topinflags & SUIF_PVIS)
    {
        Surface* suf = &frontSide->SW_topsurface;

        if(R_FindBottomTop(hedge->lineDef, hedge->side, SS_TOP,
                           hedge->offset + suf->visOffset[VX], suf->visOffset[VY],
                           ffloor, fceil, bfloor, bceil,
                           (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
                           (line->flags & DDLF_DONTPEGTOP)? true : false,
                           (frontSide->flags & SDF_MIDDLE_STRETCH)? true : false,
                           LINE_SELFREF(line)? true : false,
                           &bottom, &top, texOffset))
        {
            rendHEdgeSection(hedge, bspLeaf, SS_TOP, suf,
                             hedge->HE_v1origin, hedge->HE_v2origin, bottom, top, texOffset,
                             frontSec, false, true, true, frontSide->flags);
        }
    }

    // Lower section.
    if(frontSide->SW_bottominflags & SUIF_PVIS)
    {
        Surface* suf = &frontSide->SW_bottomsurface;

        if(R_FindBottomTop(hedge->lineDef, hedge->side, SS_BOTTOM,
                           hedge->offset + suf->visOffset[VX], suf->visOffset[VY],
                           ffloor, fceil, bfloor, bceil,
                           (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
                           (line->flags & DDLF_DONTPEGTOP)? true : false,
                           (frontSide->flags & SDF_MIDDLE_STRETCH)? true : false,
                           LINE_SELFREF(line)? true : false,
                           &bottom, &top, texOffset))
        {
            rendHEdgeSection(hedge, bspLeaf, SS_BOTTOM, suf,
                             hedge->HE_v1origin, hedge->HE_v2origin, bottom, top, texOffset,
                             frontSec, false, true, true, frontSide->flags);
        }
    }

    // Can we make this a solid segment in the clipper?
    if(solidSeg == -1)
        return false; // NEVER (we have a hole we couldn't fix).

    if(line->L_frontside && line->L_backside &&
       line->L_frontsector == line->L_backsector)
       return false;

    /// @fixme Most of these cases are now irrelevant and can be removed.

    if(!solidSeg) // We'll have to determine whether we can...
    {
        if(backSec == frontSec)
        {
            // An obvious hack, what to do though??
        }
        else if((bceil->visHeight <= ffloor->visHeight &&
                    ((frontSide->SW_topmaterial /* && !(frontSide->flags & SDF_MIDTEXUPPER)*/) ||
                     (frontSide->SW_middlematerial))) ||
                (bfloor->visHeight >= fceil->visHeight &&
                    (frontSide->SW_bottommaterial || frontSide->SW_middlematerial)))
        {
            // A closed gap.
            solidSeg = true;
        }
        else if(bceil->visHeight <= bfloor->visHeight ||
                (!(bceil->visHeight - bfloor->visHeight > 0) && bfloor->visHeight > ffloor->visHeight && bceil->visHeight < fceil->visHeight &&
                (frontSide->SW_topmaterial /*&& !(frontSide->flags & SDF_MIDTEXUPPER)*/) &&
                (frontSide->SW_bottommaterial)))
        {
            // A zero height back segment
            solidSeg = true;
        }
    }

    if(solidSeg && !P_IsInVoid(viewPlayer))
        return true;

    return false;
}

static void Rend_MarkSegsFacingFront(BspLeaf* leaf)
{
    if(leaf->hedge)
    {
        HEdge* hedge = leaf->hedge;
        do
        {
            // Occlusions can only happen where two sectors contact.
            if(hedge->lineDef)
            {
                // Which way should it be facing?
                if(!(viewFacingDot(hedge->HE_v1origin, hedge->HE_v2origin) < 0))
                    hedge->frameFlags |= HEDGEINF_FACINGFRONT;
                else
                    hedge->frameFlags &= ~HEDGEINF_FACINGFRONT;

                Rend_MarkSideDefSectionsPVisible(hedge);
            }
        } while((hedge = hedge->next) != leaf->hedge);
    }

    if(leaf->polyObj)
    {
        LineDef* line;
        HEdge* hedge;
        uint i;
        for(i = 0; i < leaf->polyObj->lineCount; ++i)
        {
            line = leaf->polyObj->lines[i];
            hedge = line->L_frontside->hedgeLeft;

            // Which way should it be facing?
            if(!(viewFacingDot(hedge->HE_v1origin, hedge->HE_v2origin) < 0))
                hedge->frameFlags |= HEDGEINF_FACINGFRONT;
            else
                hedge->frameFlags &= ~HEDGEINF_FACINGFRONT;

            Rend_MarkSideDefSectionsPVisible(hedge);
        }
    }
}

static void occludeFrontFacingSegsInBspLeaf(const BspLeaf* bspLeaf)
{
    if(bspLeaf->hedge)
    {
        HEdge* hedge = bspLeaf->hedge;
        do
        {
            if(!hedge->lineDef || !(hedge->frameFlags & HEDGEINF_FACINGFRONT)) continue;

            if(!C_CheckRangeFromViewRelPoints(hedge->HE_v1origin, hedge->HE_v2origin))
            {
                hedge->frameFlags &= ~HEDGEINF_FACINGFRONT;
            }
        } while((hedge = hedge->next) != bspLeaf->hedge);
    }

    if(bspLeaf->polyObj)
    {
        LineDef* line;
        HEdge* hedge;
        uint i;

        for(i = 0; i < bspLeaf->polyObj->lineCount; ++i)
        {
            line = bspLeaf->polyObj->lines[i];
            hedge = line->L_frontside->hedgeLeft;

            if(!(hedge->frameFlags & HEDGEINF_FACINGFRONT)) continue;

            if(!C_CheckRangeFromViewRelPoints(hedge->HE_v1origin, hedge->HE_v2origin))
            {
                hedge->frameFlags &= ~HEDGEINF_FACINGFRONT;
            }
        }
    }
}

/**
 * @defgroup skyCapFlags  Sky Cap Flags
 */
///@{
#define SKYCAP_LOWER                0x1
#define SKYCAP_UPPER                0x2
///@}

static __inline coord_t skyCapZ(BspLeaf* bspLeaf, int skyCap)
{
    const planetype_t plane = (skyCap & SKYCAP_UPPER)? PLN_CEILING : PLN_FLOOR;
    if(!bspLeaf) Con_Error("skyCapZ: Invalid bspLeaf argument (=NULL).");
    if(!bspLeaf->sector || !P_IsInVoid(viewPlayer)) return GameMap_SkyFix(theMap, plane == PLN_CEILING);
    return bspLeaf->sector->SP_planevisheight(plane);
}

static __inline coord_t skyFixFloorZ(const Plane* frontFloor, const Plane* backFloor)
{
    if(P_IsInVoid(viewPlayer))
        return frontFloor->visHeight;
    return GameMap_SkyFixFloor(theMap);
}

static __inline coord_t skyFixCeilZ(const Plane* frontCeil, const Plane* backCeil)
{
    if(P_IsInVoid(viewPlayer))
        return frontCeil->visHeight;
    return GameMap_SkyFixCeiling(theMap);
}

/**
 * Determine which sky fixes are necessary for the specified @a hedge.
 * @param hedge  HEdge to be evaluated.
 * @return  @see skyCapFlags
 */
static int chooseHEdgeSkyFixes(HEdge* hedge)
{
    int fixes = 0;
    if(hedge && hedge->lineDef) // "minisegs" have no linedefs.
    {
        const Sector* frontSec = hedge->sector;
        const Sector* backSec  = HEDGE_BACK_SECTOR(hedge);

        if(!backSec || backSec != hedge->sector)
        {
            const boolean hasSkyFloor   = Surface_IsSkyMasked(&frontSec->SP_floorsurface);
            const boolean hasSkyCeiling = Surface_IsSkyMasked(&frontSec->SP_ceilsurface);

            if(hasSkyFloor || hasSkyCeiling)
            {
                const boolean hasClosedBack = LineDef_BackClosed(hedge->lineDef, hedge->side, true/*ignore opacity*/);

                // Lower fix?
                if(hasSkyFloor)
                {
                    const Plane* ffloor = frontSec->SP_plane(PLN_FLOOR);
                    const Plane* bfloor = backSec? backSec->SP_plane(PLN_FLOOR) : NULL;
                    const coord_t skyZ = skyFixFloorZ(ffloor, bfloor);

                    if(hasClosedBack || (!Surface_IsSkyMasked(&bfloor->surface) || P_IsInVoid(viewPlayer)))
                    {
                        const Plane* floor = (bfloor && Surface_IsSkyMasked(&bfloor->surface) && ffloor->visHeight < bfloor->visHeight? bfloor : ffloor);
                        if(floor->visHeight > skyZ)
                            fixes |= SKYCAP_LOWER;
                    }
                }

                // Upper fix?
                if(hasSkyCeiling)
                {
                    const Plane* fceil = frontSec->SP_plane(PLN_CEILING);
                    const Plane* bceil = backSec? backSec->SP_plane(PLN_CEILING) : NULL;
                    const coord_t skyZ = skyFixCeilZ(fceil, bceil);

                    if(hasClosedBack || (!Surface_IsSkyMasked(&bceil->surface) || P_IsInVoid(viewPlayer)))
                    {
                        const Plane* ceil = (bceil && Surface_IsSkyMasked(&bceil->surface) && fceil->visHeight > bceil->visHeight? bceil : fceil);
                        if(ceil->visHeight < skyZ)
                            fixes |= SKYCAP_UPPER;
                    }
                }
            }
        }
    }
    return fixes;
}

/**
 * @param hedge  HEdge from which to determine sky fix coordinates.
 * @param skyCap  Either SKYCAP_LOWER or SKYCAP_UPPER (SKYCAP_UPPER has precendence).
 * @param bottom  Z map space coordinate for the bottom of the skyfix written here.
 * @param top  Z map space coordinate for the top of the skyfix written here.
 */
static void skyFixZCoords(HEdge* hedge, int skyCap, coord_t* bottom, coord_t* top)
{
    const Sector* frontSec = hedge->sector;
    const Sector* backSec  = HEDGE_BACK_SECTOR(hedge);
    const Plane* ffloor = frontSec->SP_plane(PLN_FLOOR);
    const Plane* fceil  = frontSec->SP_plane(PLN_CEILING);
    const Plane* bceil  = backSec? backSec->SP_plane(PLN_CEILING) : NULL;
    const Plane* bfloor = backSec? backSec->SP_plane(PLN_FLOOR)   : NULL;

    if(!bottom && !top) return;
    if(bottom) *bottom = 0;
    if(top)    *top    = 0;

    if(skyCap & SKYCAP_UPPER)
    {
        if(top)    *top    = skyFixCeilZ(fceil, bceil);
        if(bottom) *bottom = MAX_OF((backSec && Surface_IsSkyMasked(&bceil->surface) )? bceil->visHeight  : fceil->visHeight,  ffloor->visHeight);
    }
    else
    {
        if(top)    *top    = MIN_OF((backSec && Surface_IsSkyMasked(&bfloor->surface))? bfloor->visHeight : ffloor->visHeight, fceil->visHeight);
        if(bottom) *bottom = skyFixFloorZ(ffloor, bfloor);
    }
}

static void writeSkyFixGeometry(BspLeaf* bspLeaf, int skyCap, int rendPolyFlags)
{
    coord_t zBottom, zTop;
    int segSkyCapFlags;
    rvertex_t verts[4];
    HEdge* hedge;

    if(!bspLeaf || !bspLeaf->hedgeCount || !bspLeaf->sector) return;
    if(!(skyCap & SKYCAP_LOWER|SKYCAP_UPPER)) return;

    hedge = bspLeaf->hedge;
    do
    {
        // Is a fix or two necessary for this hedge?
        segSkyCapFlags = chooseHEdgeSkyFixes(hedge);
        if(!(segSkyCapFlags & (SKYCAP_LOWER|SKYCAP_UPPER))) continue;

        /**
         * Vertex layout:
         *   1--3
         *   |  |
         *   0--2
         */
        V2f_Copyd(verts[0].pos, hedge->HE_v1origin);
        V2f_Copyd(verts[1].pos, hedge->HE_v1origin);
        V2f_Copyd(verts[2].pos, hedge->HE_v2origin);
        V2f_Copyd(verts[3].pos, hedge->HE_v2origin);

        // First, lower fix:
        if(skyCap & segSkyCapFlags & SKYCAP_LOWER)
        {
            skyFixZCoords(hedge, SKYCAP_LOWER, &zBottom, &zTop);
            if(zBottom < zTop)
            {
                verts[0].pos[VZ] = verts[2].pos[VZ] = zBottom;
                verts[1].pos[VZ] = verts[3].pos[VZ] = zTop;
                RL_AddPoly(PT_TRIANGLE_STRIP, rendPolyFlags, 4, verts, NULL);
            }
        }

        // Upper fix:
        if(skyCap & segSkyCapFlags & SKYCAP_UPPER)
        {
            skyFixZCoords(hedge, SKYCAP_UPPER, &zBottom, &zTop);
            if(zBottom < zTop)
            {
                verts[0].pos[VZ] = verts[2].pos[VZ] = zBottom;
                verts[1].pos[VZ] = verts[3].pos[VZ] = zTop;
                RL_AddPoly(PT_TRIANGLE_STRIP, rendPolyFlags, 4, verts, NULL);
            }
        }
    } while((hedge = hedge->next) != bspLeaf->hedge);
}

/**
 * Render the lower and/or upper sky cap geometry for @a bspLeaf.
 * @param bspLeaf  BspLeaf to render geometry for.
 * @param skyCap  @see skyCapFlags
 */
static void rendBspLeafSky(BspLeaf* bspLeaf, int skyCap)
{
    const int rendPolyFlags = RPF_DEFAULT | RPF_SKYMASK;

    if(!bspLeaf || !bspLeaf->hedgeCount || !bspLeaf->sector) return;

    // Sky caps are only necessary in sectors with skymasked planes.
    if((skyCap & SKYCAP_LOWER) && !Surface_IsSkyMasked(&bspLeaf->sector->SP_floorsurface))
        skyCap &= ~SKYCAP_LOWER;
    if((skyCap & SKYCAP_UPPER) && !Surface_IsSkyMasked(&bspLeaf->sector->SP_ceilsurface))
        skyCap &= ~SKYCAP_UPPER;

    // Sky fixes:
    writeSkyFixGeometry(bspLeaf, skyCap, rendPolyFlags);

    // Lower cap:
    if(skyCap & SKYCAP_LOWER)
    {
        const float height = skyCapZ(bspLeaf, SKYCAP_LOWER);
        const int numVerts = BspLeaf_NumFanVertices(bspLeaf);
        rvertex_t* verts = R_AllocRendVertices(numVerts);

        BspLeaf_PrepareFan(bspLeaf, false/*clockwise*/, height, verts, numVerts);
        RL_AddPoly(PT_FAN, rendPolyFlags, numVerts, verts, NULL);
        R_FreeRendVertices(verts);
    }

    // Upper cap:
    if(skyCap & SKYCAP_UPPER)
    {
        const float height = skyCapZ(bspLeaf, SKYCAP_UPPER);
        const int numVerts = BspLeaf_NumFanVertices(bspLeaf);
        rvertex_t* verts = R_AllocRendVertices(numVerts);

        BspLeaf_PrepareFan(bspLeaf, true/*anticlockwise*/, height, verts, numVerts);
        RL_AddPoly(PT_FAN, rendPolyFlags, numVerts, verts, NULL);
        R_FreeRendVertices(verts);
    }
}

static boolean skymaskSegIsVisible(HEdge* hedge, boolean clipBackFacing)
{
    LineDef* lineDef;
    SideDef* sideDef;
    Sector* backSec;
    Sector* frontSec;

    // "minisegs" have no linedefs.
    if(!hedge->lineDef) return false;
    lineDef = hedge->lineDef;

    // No SideDef on this side?
    sideDef = HEDGE_SIDEDEF(hedge);
    if(!sideDef) return false;

    // Let's first check which way this hedge is facing.
    if(clipBackFacing && !(hedge->frameFlags & HEDGEINF_FACINGFRONT)) return false;

    frontSec = hedge->sector;
    backSec  = HEDGE_BACK_SECTOR(hedge);

    // Avoid obvious hack (best take no chances...).
    return !(backSec == frontSec && !sideDef->SW_topmaterial && !sideDef->SW_bottommaterial &&
            !sideDef->SW_middlematerial);
}

static void Rend_RenderBspLeafSky(BspLeaf* bspLeaf)
{
    // Any work to do?
    if(devRendSkyMode || !bspLeaf || !bspLeaf->sector || !R_SectorContainsSkySurfaces(bspLeaf->sector)) return;

    // All geometry uses the same (default) RTU write state.
    RL_LoadDefaultRtus();

    // Write geometry.
    rendBspLeafSky(bspLeaf, SKYCAP_LOWER|SKYCAP_UPPER);
}

/**
 * Creates new occlusion planes from the BspLeaf's edges.
 * Before testing, occlude the BspLeaf's backfaces. After testing occlude
 * the remaining faces, i.e. the forward facing edges. This is done before
 * rendering edges, so solid segments cut out all unnecessary oranges.
 */
static void occludeBspLeaf(const BspLeaf* bspLeaf, boolean forwardFacing)
{
    coord_t fFloor, fCeil, bFloor, bCeil;
    const coord_t* startv, *endv;
    Sector* front, *back;
    HEdge* hedge;

    if(devNoCulling || !bspLeaf || !bspLeaf->hedge || P_IsInVoid(viewPlayer)) return;

    front = bspLeaf->sector;
    fFloor = front->SP_floorheight;
    fCeil = front->SP_ceilheight;

    hedge = bspLeaf->hedge;
    do
    {
        // Occlusions can only happen where two sectors contact.
        if(hedge->lineDef && HEDGE_BACK_SECTOR(hedge) &&
           (forwardFacing == ((hedge->frameFlags & HEDGEINF_FACINGFRONT)? true : false)))
        {
            back = HEDGE_BACK_SECTOR(hedge);
            bFloor = back->SP_floorheight;
            bCeil = back->SP_ceilheight;

            // Choose start and end vertices so that it's facing forward.
            if(forwardFacing)
            {
                startv = hedge->HE_v1origin;
                endv   = hedge->HE_v2origin;
            }
            else
            {
                startv = hedge->HE_v2origin;
                endv   = hedge->HE_v1origin;
            }

            // Do not create an occlusion for sky floors.
            if(!Surface_IsSkyMasked(&back->SP_floorsurface) ||
               !Surface_IsSkyMasked(&front->SP_floorsurface))
            {
                // Do the floors create an occlusion?
                if((bFloor > fFloor && vOrigin[VY] <= bFloor) ||
                   (bFloor < fFloor && vOrigin[VY] >= fFloor))
                {
                    // Occlude down.
                    C_AddViewRelOcclusion(startv, endv, MAX_OF(fFloor, bFloor), false);
                }
            }

            // Do not create an occlusion for sky ceilings.
            if(!Surface_IsSkyMasked(&back->SP_ceilsurface) ||
               !Surface_IsSkyMasked(&front->SP_ceilsurface))
            {
                // Do the ceilings create an occlusion?
                if((bCeil < fCeil && vOrigin[VY] >= bCeil) ||
                   (bCeil > fCeil && vOrigin[VY] <= fCeil))
                {
                    // Occlude up.
                    C_AddViewRelOcclusion(startv, endv, MIN_OF(fCeil, bCeil), true);
                }
            }
        }
    } while((hedge = hedge->next) != bspLeaf->hedge);
}

static void Rend_RenderBspLeaf(BspLeaf* bspLeaf)
{
    coord_t sceil, sfloor;
    uint i, bspLeafIdx;
    Sector* sect;

    if(!bspLeaf->sector) return; // An orphan BSP leaf?

    sect = bspLeaf->sector;
    sceil = sect->SP_ceilvisheight;
    sfloor = sect->SP_floorvisheight;

    if(sceil - sfloor <= 0 || bspLeaf->hedgeCount < 3)
    {
        // Skip this, it has no volume.
        // Neighbors handle adding the solid clipper segments.
        return;
    }

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
    sect->frameFlags |= SIF_VISIBLE;

    Rend_MarkSegsFacingFront(bspLeaf);

    R_InitForBspLeaf(bspLeaf);
    Rend_RadioBspLeafEdges(bspLeaf);

    bspLeafIdx = GET_BSPLEAF_IDX(bspLeaf);
    occludeBspLeaf(bspLeaf, false);
    LO_ClipInBspLeaf(bspLeafIdx);
    occludeBspLeaf(bspLeaf, true);

    occludeFrontFacingSegsInBspLeaf(bspLeaf);

    if(bspLeaf->polyObj)
    {
        // Polyobjs don't obstruct, do clip lights with another algorithm.
        LO_ClipInBspLeafBySight(bspLeafIdx);
    }

    // Mark the particle generators in the sector visible.
    Rend_ParticleMarkInSectorVisible(sect);

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

    // Draw the sky cap geometry (and various skymask fixes).
    Rend_RenderBspLeafSky(bspLeaf);

    // Draw the walls.
    if(bspLeaf->hedge)
    {
        HEdge* hedge = bspLeaf->hedge;
        do
        {
            if(hedge->lineDef && // "minisegs" have no linedefs.
               HEDGE_SIDEDEF(hedge) && // "windows" have no sidedef.
               (hedge->frameFlags & HEDGEINF_FACINGFRONT))
            {
                boolean solid;

                if(!hedge->sector || !HEDGE_BACK_SECTOR(hedge) ||
                   (hedge->twin && !HEDGE_SIDEDEF(hedge->twin)) /* front side of a "window" */)
                    solid = Rend_RenderHEdge(hedge, bspLeaf);
                else
                    solid = Rend_RenderHEdgeTwosided(hedge, bspLeaf);

                // When the viewer is in the void no wall is "solid".
                if(solid && !P_IsInVoid(viewPlayer))
                {
                    C_AddRangeFromViewRelPoints(hedge->HE_v1origin, hedge->HE_v2origin);
                }
            }
        } while((hedge = hedge->next) != bspLeaf->hedge);
    }

    // Is there a polyobj on board?
    if(bspLeaf->polyObj)
    {
        LineDef* line;
        HEdge* hedge;
        for(i = 0; i < bspLeaf->polyObj->lineCount; ++i)
        {
            line = bspLeaf->polyObj->lines[i];
            hedge = line->L_frontside->hedgeLeft;

            // Let's first check which way this hedge is facing.
            if(hedge->frameFlags & HEDGEINF_FACINGFRONT)
            {
                boolean solid = Rend_RenderHEdge(hedge, bspLeaf);
                if(solid)
                {
                    C_AddRangeFromViewRelPoints(hedge->HE_v1origin, hedge->HE_v2origin);
                }
            }
        }
    }

    // Render all planes of this sector.
    for(i = 0; i < sect->planeCount; ++i)
    {
        const Plane* plane = sect->planes[i];
        const Surface* suf = &plane->surface;
        boolean isSkyMasked = false;
        boolean addDLights = true;
        boolean flipSurfaceNormal = false;
        boolean clipBackFacing = false;
        float texOffset[2];
        float texScale[2];
        material_t* mat;
        int texMode;

        isSkyMasked = Surface_IsSkyMasked(suf);
        if(isSkyMasked && plane->type != PLN_MID)
        {
            if(!devRendSkyMode) continue; // Not handled here.
            isSkyMasked = false;
        }

        if(renderTextures == 2)
            texMode = 2;
        else if(!suf->material || (devNoTexFix && (suf->inFlags & SUIF_FIX_MISSING_MATERIAL)))
            texMode = 1;
        else
            texMode = 0;

        if(texMode == 0)
            mat = suf->material;
        else if(texMode == 1)
            // For debug, render the "missing" texture instead of the texture
            // chosen for surfaces to fix the HOMs.
            mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":missing"));
        else
            // For lighting debug, render all solid surfaces using the gray texture.
            mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":gray"));

        V2f_Copy(texOffset, suf->visOffset);
        // Add the Y offset to orient the Y flipped texture.
        if(plane->type == PLN_CEILING)
            texOffset[VY] -= bspLeaf->aaBox.maxY - bspLeaf->aaBox.minY;
        // Add the additional offset to align with the worldwide grid.
        texOffset[VX] += (float)bspLeaf->worldGridOffset[VX];
        texOffset[VY] += (float)bspLeaf->worldGridOffset[VY];
        // Inverted.
        texOffset[VY] = -texOffset[VY];

        texScale[VX] = (((suf->flags & DDSUF_MATERIAL_FLIPH) || (texMode == 1 && (i == PLN_CEILING)))? -1 : 1);
        texScale[VY] = ((suf->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        Rend_RenderPlane(bspLeaf, plane->type, plane->visHeight, suf->tangent, suf->bitangent, suf->normal,
            mat, suf->flags, suf->inFlags, suf->rgba, suf->blendMode, texOffset, texScale,
            isSkyMasked, addDLights, (i == PLN_FLOOR), bspLeaf->bsuf[plane->planeID], plane->planeID,
            texMode, flipSurfaceNormal, clipBackFacing);
    }
}

static void Rend_RenderNode(runtime_mapdata_header_t* bspPtr)
{
    // If the clipper is full we're pretty much done. This means no geometry
    // will be visible in the distance because every direction has already been
    // fully covered by geometry.
    if(C_IsFull()) return;

    if(bspPtr->type == DMU_BSPLEAF)
    {
        // We've arrived at a leaf. Render it.
        Rend_RenderBspLeaf((BspLeaf*)bspPtr);
    }
    else
    {
        // Descend deeper into the nodes.
        const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
        BspNode* node = (BspNode*)bspPtr;
        byte side;

        // Decide which side the view point is on.
        side = Partition_PointOnSide(&node->partition, viewData->current.origin);

        Rend_RenderNode(node->children[side]); // Recursively divide front space.
        Rend_RenderNode(node->children[side ^ 1]); // ...and back space.
    }
}

static void drawVector(const_pvec3f_t origin, const_pvec3f_t normal, float scalar, const float color[3])
{
    static const float black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(scalar * normal[VX], scalar * normal[VZ], scalar * normal[VY]);
        glColor3fv(color);
        glVertex3f(0, 0, 0);
    glEnd();
}

static void drawSurfaceTangentSpaceVectors(Surface* suf, const_pvec3f_t origin)
{
#define VISUAL_LENGTH       (20)

    static const float red[3]   = { 1, 0, 0 };
    static const float green[3] = { 0, 1, 0 };
    static const float blue[3]  = { 0, 0, 1 };

    assert(origin && suf);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(origin[VX], origin[VZ], origin[VY]);

    if(devSurfaceVectors & SVF_TANGENT)   drawVector(origin, suf->tangent,   VISUAL_LENGTH, red);
    if(devSurfaceVectors & SVF_BITANGENT) drawVector(origin, suf->bitangent, VISUAL_LENGTH, green);
    if(devSurfaceVectors & SVF_NORMAL)    drawVector(origin, suf->normal,    VISUAL_LENGTH, blue);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

#undef VISUAL_LENGTH
}

/**
 * Draw the surface normals, primarily for debug.
 */
void Rend_RenderSurfaceVectors(void)
{
    uint i;

    if(devSurfaceVectors == 0 || !theMap) return;

    glDisable(GL_CULL_FACE);

    for(i = 0; i < NUM_HEDGES; ++i)
    {
        HEdge* hedge = GameMap_HEdge(theMap, i);
        float x, y, bottom, top;
        Sector* backSec;
        SideDef* side;
        Surface* suf;
        vec3f_t origin;

        if(!hedge->lineDef || !hedge->sector ||
           (hedge->lineDef->inFlags & LF_POLYOBJ))
            continue;

        side = HEDGE_SIDEDEF(hedge);
        x = hedge->HE_v1origin[VX] + (hedge->HE_v2origin[VX] - hedge->HE_v1origin[VX]) / 2;
        y = hedge->HE_v1origin[VY] + (hedge->HE_v2origin[VY] - hedge->HE_v1origin[VY]) / 2;

        backSec = HEDGE_BACK_SECTOR(hedge);
        if(!backSec)
        {
            bottom = side->sector->SP_floorvisheight;
            top = side->sector->SP_ceilvisheight;
            suf = &side->SW_middlesurface;

            V3f_Set(origin, x, y, bottom + (top - bottom) / 2);
            drawSurfaceTangentSpaceVectors(suf, origin);
        }
        else
        {
            if(side->SW_middlesurface.material)
            {
                top = hedge->sector->SP_ceilvisheight;
                bottom = hedge->sector->SP_floorvisheight;
                suf = &side->SW_middlesurface;

                V3f_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }

            if(backSec->SP_ceilvisheight <
               hedge->sector->SP_ceilvisheight &&
               !(Surface_IsSkyMasked(&hedge->sector->SP_ceilsurface) &&
                 Surface_IsSkyMasked(&backSec->SP_ceilsurface)))
            {
                bottom = backSec->SP_ceilvisheight;
                top = hedge->sector->SP_ceilvisheight;
                suf = &side->SW_topsurface;

                V3f_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }

            if(backSec->SP_floorvisheight >
               hedge->sector->SP_floorvisheight &&
               !(Surface_IsSkyMasked(&hedge->sector->SP_floorsurface) &&
                 Surface_IsSkyMasked(&backSec->SP_floorsurface)))
            {
                bottom = hedge->sector->SP_floorvisheight;
                top = backSec->SP_floorvisheight;
                suf = &side->SW_bottomsurface;

                V3f_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }
        }
    }

    for(i = 0; i < NUM_BSPLEAFS; ++i)
    {
        BspLeaf* bspLeaf = bspLeafs[i];
        uint j;

        if(!bspLeaf->sector) continue;

        for(j = 0; j < bspLeaf->sector->planeCount; ++j)
        {
            Plane* pln = bspLeaf->sector->SP_plane(j);
            vec3f_t origin;

            V3f_Set(origin, bspLeaf->midPoint[VX], bspLeaf->midPoint[VY], pln->visHeight);
            if(pln->type != PLN_MID && Surface_IsSkyMasked(&pln->surface))
                origin[VZ] = GameMap_SkyFix(theMap, pln->type == PLN_CEILING);

            drawSurfaceTangentSpaceVectors(&pln->surface, origin);
        }
    }

    for(i = 0; i < NUM_POLYOBJS; ++i)
    {
        const Polyobj* po = polyObjs[i];
        const Sector* sec = po->bspLeaf->sector;
        float zPos = sec->SP_floorheight + (sec->SP_ceilheight - sec->SP_floorheight)/2;
        vec3f_t origin;
        uint j;

        for(j = 0; j < po->lineCount; ++j)
        {
            LineDef* line = po->lines[j];

            V3f_Set(origin, (line->L_v2origin[VX] + line->L_v1origin[VX])/2,
                            (line->L_v2origin[VY] + line->L_v1origin[VY])/2, zPos);
            drawSurfaceTangentSpaceVectors(&line->L_frontside->SW_middlesurface, origin);
        }
    }

    glEnable(GL_CULL_FACE);
}

static void drawSoundOrigin(coord_t const origin[3], const char* label, coord_t const eye[3])
{
#define MAX_SOUNDORIGIN_DIST    384 ///< Maximum distance from origin to eye in map coordinates.

    const Point2Raw labelOrigin = { 2, 2 };
    coord_t dist;
    float alpha;

    if(!origin || !label || !eye) return;

    dist = V3d_Distance(origin, eye);
    alpha = 1.f - MIN_OF(dist, MAX_SOUNDORIGIN_DIST) / MAX_SOUNDORIGIN_DIST;

    if(alpha > 0)
    {
        float scale = dist / (Window_Width(theWindow) / 2);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(origin[VX], origin[VZ], origin[VY]);
        glRotatef(-vang + 180, 0, 1, 0);
        glRotatef(vpitch, 1, 0, 0);
        glScalef(-scale, -scale, 1);

        UI_TextOutEx(label, &labelOrigin, UI_Color(UIC_TITLE), alpha);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

#undef MAX_SOUNDORIGIN_DIST
}

static int drawSideDefSoundOrigins(SideDef* side, void* parameters)
{
    uint idx = GameMap_SideDefIndex(theMap, side); /// @fixme Do not assume current map.
    char buf[80];

    dd_snprintf(buf, 80, "Side #%i (middle)", idx);
    drawSoundOrigin(side->SW_middlesurface.base.origin, buf, parameters);

    dd_snprintf(buf, 80, "Side #%i (bottom)", idx);
    drawSoundOrigin(side->SW_bottomsurface.base.origin, buf, parameters);

    dd_snprintf(buf, 80, "Side #%i (top)", idx);
    drawSoundOrigin(side->SW_topsurface.base.origin, buf, parameters);
    return false; // Continue iteration.
}

static int drawSectorSoundOrigins(Sector* sec, void* parameters)
{
    uint idx = GameMap_SectorIndex(theMap, sec); /// @fixme Do not assume current map.
    char buf[80];

    if(devSoundOrigins & SOF_PLANE)
    {
        uint i;
        for(i = 0; i < sec->planeCount; ++i)
        {
            Plane* pln = sec->SP_plane(i);
            dd_snprintf(buf, 80, "Sector #%i (pln:%i)", idx, i);
            drawSoundOrigin(pln->PS_base.origin, buf, parameters);
        }
    }

    if(devSoundOrigins & SOF_SECTOR)
    {
        dd_snprintf(buf, 80, "Sector #%i", idx);
        drawSoundOrigin(sec->base.origin, buf, parameters);
    }

    return false; // Continue iteration.
}

/**
 * Debugging aid for visualizing sound origins.
 */
void Rend_RenderSoundOrigins(void)
{
    coord_t eye[3];

    if(!devSoundOrigins || !theMap) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    V3d_Set(eye, vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

    if(devSoundOrigins & SOF_SIDEDEF)
    {
        GameMap_SideDefIterator(theMap, drawSideDefSoundOrigins, eye);
    }

    if(devSoundOrigins & (SOF_SECTOR|SOF_PLANE))
    {
        GameMap_SectorIterator(theMap, drawSectorSoundOrigins, eye);
    }

    // Restore previous state.
    glEnable(GL_DEPTH_TEST);
}

static void getVertexPlaneMinMax(const Vertex* vtx, coord_t* min, coord_t* max)
{
    lineowner_t* vo, *base;

    if(!vtx || (!min && !max))
        return; // Wha?

    vo = base = vtx->lineOwners;
    do
    {
        LineDef* li = vo->lineDef;

        if(li->L_frontside)
        {
            if(min && li->L_frontsector->SP_floorvisheight < *min)
                *min = li->L_frontsector->SP_floorvisheight;

            if(max && li->L_frontsector->SP_ceilvisheight > *max)
                *max = li->L_frontsector->SP_ceilvisheight;
        }

        if(li->L_backside)
        {
            if(min && li->L_backsector->SP_floorvisheight < *min)
                *min = li->L_backsector->SP_floorvisheight;

            if(max && li->L_backsector->SP_ceilvisheight > *max)
                *max = li->L_backsector->SP_ceilvisheight;
        }

        vo = vo->LO_next;
    } while(vo != base);
}

static void drawVertexPoint(const Vertex* vtx, coord_t z, float alpha)
{
    glBegin(GL_POINTS);
        glColor4f(.7f, .7f, .2f, alpha * 2);
        glVertex3f(vtx->origin[VX], z, vtx->origin[VY]);
    glEnd();
}

static void drawVertexBar(const Vertex* vtx, coord_t bottom, coord_t top, float alpha)
{
#define EXTEND_DIST     64

    static const float black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(vtx->origin[VX], bottom - EXTEND_DIST, vtx->origin[VY]);
        glColor4f(1, 1, 1, alpha);
        glVertex3f(vtx->origin[VX], bottom, vtx->origin[VY]);
        glVertex3f(vtx->origin[VX], bottom, vtx->origin[VY]);
        glVertex3f(vtx->origin[VX], top, vtx->origin[VY]);
        glVertex3f(vtx->origin[VX], top, vtx->origin[VY]);
        glColor4fv(black);
        glVertex3f(vtx->origin[VX], top + EXTEND_DIST, vtx->origin[VY]);
    glEnd();

#undef EXTEND_DIST
}

static void drawVertexIndex(const Vertex* vtx, coord_t z, float scale, float alpha)
{
    const Point2Raw origin = { 2, 2 };
    char buf[80];

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    sprintf(buf, "%lu", (unsigned long) (vtx - vertexes));

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(vtx->origin[VX], z, vtx->origin[VY]);
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

static int drawVertex1(LineDef* li, void* context)
{
    Vertex* vtx = li->L_v1;
    Polyobj* po = context;
    coord_t dist2D = M_ApproxDistance(vOrigin[VX] - vtx->origin[VX], vOrigin[VZ] - vtx->origin[VY]);

    if(dist2D < MAX_VERTEX_POINT_DIST)
    {
        float alpha = 1 - dist2D / MAX_VERTEX_POINT_DIST;

        if(alpha > 0)
        {
            coord_t bottom = po->bspLeaf->sector->SP_floorvisheight;
            coord_t top = po->bspLeaf->sector->SP_ceilvisheight;

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

        pos[VX] = vtx->origin[VX];
        pos[VY] = vtx->origin[VY];
        pos[VZ] = po->bspLeaf->sector->SP_floorvisheight;

        dist3D = V3d_Distance(pos, eye);

        if(dist3D < MAX_VERTEX_POINT_DIST)
        {
            drawVertexIndex(vtx, pos[VZ], dist3D / (Window_Width(theWindow) / 2),
                            1 - dist3D / MAX_VERTEX_POINT_DIST);
        }
    }

    return false; // Continue iteration.
}

int drawPolyObjVertexes(Polyobj* po, void* context)
{
    return Polyobj_LineIterator(po, drawVertex1, po);
}

/**
 * Draw the various vertex debug aids.
 */
void Rend_Vertexes(void)
{
    float oldPointSize, oldLineWidth = 1;
    uint i;
    AABoxd box;

    if(!devVertexBars && !devVertexIndices) return;

    glDisable(GL_DEPTH_TEST);

    if(devVertexBars)
    {
        glEnable(GL_LINE_SMOOTH);
        oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
        DGL_SetFloat(DGL_LINE_WIDTH, 2);

        for(i = 0; i < NUM_VERTEXES; ++i)
        {
            Vertex* vtx = &vertexes[i];
            float alpha;

            if(!vtx->lineOwners)
                continue; // Not a linedef vertex.
            if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ)
                continue; // A polyobj linedef vertex.

            alpha = 1 - M_ApproxDistance(vOrigin[VX] - vtx->origin[VX], vOrigin[VZ] - vtx->origin[VY]) / MAX_VERTEX_POINT_DIST;
            alpha = MIN_OF(alpha, .15f);

            if(alpha > 0)
            {
                coord_t bottom, top;

                bottom = DDMAXFLOAT;
                top = DDMINFLOAT;
                getVertexPlaneMinMax(vtx, &bottom, &top);

                drawVertexBar(vtx, bottom, top, alpha);
            }
        }
    }

    // Always draw the vertex point nodes.
    glEnable(GL_POINT_SMOOTH);
    oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);
    DGL_SetFloat(DGL_POINT_SIZE, 6);

    for(i = 0; i < NUM_VERTEXES; ++i)
    {
        Vertex* vtx = &vertexes[i];
        coord_t dist;

        if(!vtx->lineOwners)
            continue; // Not a linedef vertex.
        if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ)
            continue; // A polyobj linedef vertex.

        dist = M_ApproxDistance(vOrigin[VX] - vtx->origin[VX], vOrigin[VZ] - vtx->origin[VY]);

        if(dist < MAX_VERTEX_POINT_DIST)
        {
            coord_t bottom;

            bottom = DDMAXFLOAT;
            getVertexPlaneMinMax(vtx, &bottom, NULL);

            drawVertexPoint(vtx, bottom, (1 - dist / MAX_VERTEX_POINT_DIST) * 2);
        }
    }


    if(devVertexIndices)
    {
        coord_t eye[3];

        eye[VX] = vOrigin[VX];
        eye[VY] = vOrigin[VZ];
        eye[VZ] = vOrigin[VY];

        for(i = 0; i < NUM_VERTEXES; ++i)
        {
            Vertex* vtx = &vertexes[i];
            coord_t pos[3], dist;

            if(!vtx->lineOwners) continue; // Not a linedef vertex.
            if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ) continue; // A polyobj linedef vertex.

            pos[VX] = vtx->origin[VX];
            pos[VY] = vtx->origin[VY];
            pos[VZ] = DDMAXFLOAT;
            getVertexPlaneMinMax(vtx, &pos[VZ], NULL);

            dist = V3d_Distance(pos, eye);

            if(dist < MAX_VERTEX_POINT_DIST)
            {
                float alpha, scale;

                alpha = 1 - dist / MAX_VERTEX_POINT_DIST;
                scale = dist / (Window_Width(theWindow) / 2);

                drawVertexIndex(vtx, pos[VZ], scale, alpha);
            }
        }
    }

    // Next, the vertexes of all nearby polyobjs.
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

void Rend_RenderMap(void)
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
        Rend_ProjectDecorations();

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
        firstBspLeaf = true;
        if(theMap->bsp->type == DMU_BSPNODE)
        {
            Rend_RenderNode(theMap->bsp);
        }
        else
        {
            // A single leaf is a special case.
            Rend_RenderBspLeaf((BspLeaf*)theMap->bsp);
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

/**
 * Updates the lightModRange which is used to applify sector light to help
 * compensate for the differences between the OpenGL lighting equation,
 * the software Doom lighting model and the light grid (ambient lighting).
 *
 * The offsets in the lightRangeModTables are added to the sector->lightLevel
 * during rendering (both positive and negative).
 */
void Rend_CalcLightModRange(void)
{
    GameMap* map = theMap;
    int j, mapAmbient;
    float f;

    if(novideo) return;

    memset(lightModRange, 0, sizeof(float) * 255);

    if(!map)
    {
        rAmbient = 0;
        return;
    }

    mapAmbient = GameMap_AmbientLightLevel(map);
    if(mapAmbient > ambientLight)
        rAmbient = mapAmbient;
    else
        rAmbient = ambientLight;

    for(j = 0; j < 255; ++j)
    {
        // Adjust the white point/dark point?
        f = 0;
        if(lightRangeCompression != 0)
        {
            if(lightRangeCompression >= 0)
            {
                // Brighten dark areas.
                f = (float) (255 - j) * lightRangeCompression;
            }
            else
            {
                // Darken bright areas.
                f = (float) -j * -lightRangeCompression;
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
void R_DrawLightRange(void)
{
#define BLOCK_WIDTH             (1.0f)
#define BLOCK_HEIGHT            (BLOCK_WIDTH * 255.0f)
#define BORDER                  (20)

    ui_color_t color;
    float c, off;
    int i;

    // Disabled?
    if(!devLightModRange) return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Window_Width(theWindow), Window_Height(theWindow), 0, -1, 1);

    glTranslatef(BORDER, BORDER, 0);

    color.red = 0.2f;
    color.green = 0;
    color.blue = 0.6f;

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
 * @param pos3f         Coordinates of the center of the base of the
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

static int drawMobjBBox(thinker_t* th, void* context)
{
    static const float  red[3] = { 1, 0.2f, 0.2f}; // non-solid objects
    static const float  green[3] = { 0.2f, 1, 0.2f}; // solid objects
    static const float  yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles

    mobj_t* mo = (mobj_t*) th;
    coord_t eye[3], size;
    float alpha;

    // We don't want the console player.
    if(mo == ddPlayers[consolePlayer].shared.mo)
        return false; // Continue iteration.
    // Is it vissible?
    if(!(mo->bspLeaf && mo->bspLeaf->sector->frameFlags & SIF_VISIBLE))
        return false; // Continue iteration.

    V3d_Set(eye, vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

    alpha = 1 - ((V3d_Distance(mo->origin, eye) / (Window_Width(theWindow)/2)) / 4);
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
static void Rend_RenderBoundingBoxes(void)
{
    //static const float red[3] = { 1, 0.2f, 0.2f}; // non-solid objects
    static const float green[3] = { 0.2f, 1, 0.2f}; // solid objects
    static const float yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles

    const materialvariantspecification_t* spec;
    const materialsnapshot_t* ms;
    material_t* mat;
    coord_t eye[3];
    uint i;

    if(!devMobjBBox && !devPolyobjBBox)
        return;

#ifndef _DEBUG
    // Bounding boxes are not allowed in non-debug netgames.
    if(netGame) return;
#endif

    if(!dlBBox)
        dlBBox = constructBBox(0, .08f);

    eye[VX] = vOrigin[VX];
    eye[VY] = vOrigin[VZ];
    eye[VZ] = vOrigin[VY];

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":bbox"));
    spec = Materials_VariantSpecificationForContext(MC_SPRITE, 0, 0, 0, 0,
        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 1, -2, -1, true, true, true, false);
    ms = Materials_Prepare(mat, spec, true);

    GL_BindTexture(MST(ms, MTU_PRIMARY));
    GL_BlendMode(BM_ADD);

    if(devMobjBBox)
        GameMap_IterateThinkers(theMap, gx.MobjThinker, 0x1, drawMobjBBox, NULL);

    if(devPolyobjBBox)
    for(i = 0; i < NUM_POLYOBJS; ++i)
    {
        const Polyobj* po = polyObjs[i];
        const Sector* sec = po->bspLeaf->sector;
        coord_t width  = (po->aaBox.maxX - po->aaBox.minX)/2;
        coord_t length = (po->aaBox.maxY - po->aaBox.minY)/2;
        coord_t height = (sec->SP_ceilheight - sec->SP_floorheight)/2;
        coord_t pos[3];
        float alpha;

        pos[VX] = po->aaBox.minX + width;
        pos[VY] = po->aaBox.minY + length;
        pos[VZ] = sec->SP_floorheight;

        alpha = 1 - ((V3d_Distance(pos, eye) / (Window_Width(theWindow)/2)) / 4);
        if(alpha < .25f)
            alpha = .25f; // Don't make them totally invisible.

        Rend_DrawBBox(pos, width, length, height, 0, yellow, alpha, .08f, true);

        {uint j;
        for(j = 0; j < po->lineCount; ++j)
        {
            LineDef* line = po->lines[j];
            coord_t width  = (line->aaBox.maxX - line->aaBox.minX)/2;
            coord_t length = (line->aaBox.maxY - line->aaBox.minY)/2;
            coord_t pos[3];

            /** Draw a bounding box for the lineDef.
            pos[VX] = line->aaBox.minX + width;
            pos[VY] = line->aaBox.minY + length;
            pos[VZ] = sec->SP_floorheight;
            Rend_DrawBBox(pos, width, length, height, 0, red, alpha, .08f, true);
            */

            pos[VX] = (line->L_v2origin[VX] + line->L_v1origin[VX])/2;
            pos[VY] = (line->L_v2origin[VY] + line->L_v1origin[VY])/2;
            pos[VZ] = sec->SP_floorheight;
            width = 0;
            length = line->length/2;

            Rend_DrawBBox(pos, width, length, height, BANG2DEG(BANG_90 - line->angle), green, alpha, 0, true);
        }}
    }

    GL_BlendMode(BM_NORMAL);

    glEnable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}
