/**\file rend_main.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

/**
 * Rendering Subsystem.
 */

// HEADER FILES ------------------------------------------------------------

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

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// Surface (tangent-space) Vector Flags.
#define SVF_TANGENT             0x01
#define SVF_BITANGENT           0x02
#define SVF_NORMAL              0x04

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Rend_DrawBBox(const float pos3f[3], float w, float l, float h, float a,
                   const float color3f[3], float alpha, float br,
                   boolean alignToBase);
void Rend_DrawArrow(const float pos3f[3], float a, float s,
                    const float color3f[3], float alpha);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void Rend_RenderBoundingBoxes(void);
static DGLuint constructBBox(DGLuint name, float br);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int translucentIceCorpse;
extern int loMaxRadius;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean usingFog = false; // Is the fog in use?
float fogColor[4];
float fieldOfView = 95.0f;
byte smoothTexAnim = true;
int useShinySurfaces = true;

int useDynlights = true;
float dynlightFactor = .7f;
float dynlightFogBright = .15f;

int useWallGlow = true;
float glowFactor = 1.0f;
float glowHeightFactor = 3; // Glow height as a multiplier.
int glowHeightMax = 100; // 100 is the default (0-1024).

int useShadows = true;
float shadowFactor = 1.2f;
int shadowMaxRadius = 80;
int shadowMaxDistance = 1000;

float vx, vy, vz, vang, vpitch;
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
byte devSurfaceVectors = 0;
byte devNoTexFix = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstsubsector; // No range checking for the first one.

// CODE --------------------------------------------------------------------

void Rend_Register(void)
{
    C_VAR_FLOAT("rend-camera-fov", &fieldOfView, 0, 1, 179);

    C_VAR_FLOAT("rend-glow", &glowFactor, 0, 0, 1);
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
    C_VAR_INT("rend-dev-cull-subsectors", &devNoCulling,CVF_NO_ARCHIVE,0,1);
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

#if 0 // unused atm
float Rend_SignedPointDist2D(float c[2])
{
    /*          (YA-YC)(XB-XA)-(XA-XC)(YB-YA)
       s = -----------------------------
       L**2
       Luckily, L**2 is one. dist = s*L. Even more luckily, L is also one.
     */
    return (vz - c[VY]) * viewsidex - (vx - c[VX]) * viewsidey;
}
#endif

/**
 * Approximated! The Z axis aspect ratio is corrected.
 */
float Rend_PointDist3D(const float c[3])
{
    return M_ApproxDistance3f(vx - c[VX], vz - c[VY], 1.2f * (vy - c[VZ]));
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
        GL_DeleteLists(dlBBox, 1);
    dlBBox = 0;
}

void Rend_ModelViewMatrix(boolean useAngles)
{
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

    vx = viewData->current.pos[VX];
    vy = viewData->current.pos[VZ];
    vz = viewData->current.pos[VY];
    vang = viewData->current.angle / (float) ANGLE_MAX *360 - 90;
    vpitch = viewData->current.pitch * 85.0 / 110.0;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if(useAngles)
    {
        glRotatef(vpitch, 1, 0, 0);
        glRotatef(vang, 0, 1, 0);
    }
    glScalef(1, 1.2f, 1);      // This is the aspect correction.
    glTranslatef(-vx, -vy, -vz);
}

static __inline float segFacingViewerDot(float v1[2], float v2[2])
{
    // The dot product.
    return (v1[VY] - v2[VY]) * (v1[VX] - vx) + (v2[VX] - v1[VX]) * (v1[VY] - vz);
}

#if 0 // unused atm
static int Rend_FixedSegFacingDir(const seg_t *seg)
{
    // The dot product. (1 if facing front.)
    return((seg->fv1.pos[VY] - seg->fv2.pos[VY]) * (seg->fv1.pos[VX] - viewX) +
           (seg->fv2.pos[VX] - seg->fv1.pos[VX]) * (seg->fv1.pos[VY] - viewY)) > 0;
}
#endif

#if 0 // unused atm
int Rend_SegFacingPoint(float v1[2], float v2[2], float pnt[2])
{
    float   nx = v1[VY] - v2[VY], ny = v2[VX] - v1[VX];
    float   vvx = v1[VX] - pnt[VX], vvy = v1[VY] - pnt[VY];

    // The dot product.
    if(nx * vvx + ny * vvy > 0)
        return 1;               // Facing front.
    return 0;                   // Facing away.
}
#endif

static int C_DECL DivSortAscend(const void *e1, const void *e2)
{
    float   f1 = *(float *) e1, f2 = *(float *) e2;

    if(f1 > f2)
        return 1;
    if(f2 > f1)
        return -1;
    return 0;
}

static int C_DECL DivSortDescend(const void *e1, const void *e2)
{
    float   f1 = *(float *) e1, f2 = *(float *) e2;

    if(f1 > f2)
        return -1;
    if(f2 > f1)
        return 1;
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
    size_t               i;

    for(i = 0; i < num; ++i)
    {
        colors[i].rgba[CA] = alpha;
    }
}

void Rend_ApplyTorchLight(float* color, float distance)
{
    ddplayer_t*         ddpl = &viewPlayer->shared;

    if(!ddpl->fixedColorMap)
        return;

    // Check for torch.
    if(distance < 1024)
    {
        // Colormap 1 is the brightest. I'm guessing 16 would be
        // the darkest.
        int                 ll = 16 - ddpl->fixedColorMap;
        float               d = (1024 - distance) / 1024.0f * ll / 15.0f;

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

    if(!ddpl->fixedColorMap)
        return; // No need, its disabled.

    { size_t i;
    for(i = 0; i < numVertices; ++i)
    {
        const rvertex_t* vtx = &vertices[i];
        ColorRawf* c = &colors[i];
        Rend_ApplyTorchLight(c->rgba, Rend_PointDist2D(vtx->pos));
    }}
}

void Rend_PreparePlane(rvertex_t* rvertices, size_t numVertices,
                       float height, const subsector_t* subsector,
                       boolean antiClockwise)
{
    size_t              i, vid;

    // First vertex is always #0.
    rvertices[0].pos[VX] = subsector->vertices[0]->pos[VX];
    rvertices[0].pos[VY] = subsector->vertices[0]->pos[VY];
    rvertices[0].pos[VZ] = height;

    // Copy the vertices in reverse order for ceilings (flip faces).
    if(antiClockwise)
        vid = numVertices - 1;
    else
        vid = 1;

    for(i = 1; i < numVertices; ++i)
    {
        rvertices[i].pos[VX] = subsector->vertices[vid]->pos[VX];
        rvertices[i].pos[VY] = subsector->vertices[vid]->pos[VY];
        rvertices[i].pos[VZ] = height;

        (antiClockwise? vid-- : vid++);
    }
}

static void markSegSectionsPVisible(seg_t* seg)
{
    const plane_t* fceil, *bceil, *ffloor, *bfloor;
    sidedef_t* side;

    if(!seg->lineDef || !seg->lineDef->L_side(seg->side))
        return;
    side = seg->lineDef->L_side(seg->side);

    { uint i;
    for(i = 0; i < 3; ++i)
        side->sections[i].inFlags |= SUIF_PVIS;
    }

    if(!seg->lineDef->L_backside)
    {
        side->SW_topsurface   .inFlags &= ~SUIF_PVIS;
        side->SW_bottomsurface.inFlags &= ~SUIF_PVIS;
        return;
    }

    fceil  = seg->lineDef->L_sector(seg->side  )->SP_plane(PLN_CEILING);
    ffloor = seg->lineDef->L_sector(seg->side  )->SP_plane(PLN_FLOOR);
    bceil  = seg->lineDef->L_sector(seg->side^1)->SP_plane(PLN_CEILING);
    bfloor = seg->lineDef->L_sector(seg->side^1)->SP_plane(PLN_FLOOR);

    // Middle.
    if(!side->SW_middlematerial || !Material_IsDrawable(side->SW_middlematerial) || side->SW_middlergba[3] <= 0)
        side->SW_middlesurface.inFlags &= ~SUIF_PVIS;

    // Top.
    if((!devRendSkyMode && R_IsSkySurface(&fceil->surface) && R_IsSkySurface(&bceil->surface)) ||
       (!devRendSkyMode && R_IsSkySurface(&bceil->surface) && (side->SW_topsurface.inFlags & SUIF_FIX_MISSING_MATERIAL)) ||
       (fceil->visHeight <= bceil->visHeight))
        side->SW_topsurface   .inFlags &= ~SUIF_PVIS;

    // Bottom.
    if((!devRendSkyMode && R_IsSkySurface(&ffloor->surface) && R_IsSkySurface(&bfloor->surface)) ||
       (!devRendSkyMode && R_IsSkySurface(&bfloor->surface) && (side->SW_bottomsurface.inFlags & SUIF_FIX_MISSING_MATERIAL)) ||
       (ffloor->visHeight >= bfloor->visHeight))
        side->SW_bottomsurface.inFlags &= ~SUIF_PVIS;
}

static void Rend_MarkSegSectionsPVisible(seg_t* seg)
{
    assert(seg);
    // danij: Do we really need to do this for both sides? Surely not.
    markSegSectionsPVisible(seg);
    if(seg->backSeg)
        markSegSectionsPVisible(seg->backSeg);
}

/**
 * @return              @true, if there is a division at the specified height.
 */
static int checkDiv(walldiv_t *div, float height)
{
    uint                i;

    for(i = 0; i < div->num; ++i)
        if(div->pos[i] == height)
            return true;

    return false;
}

static void doCalcSegDivisions(walldiv_t* div, const linedef_t* line,
                               boolean backSide, const sector_t* frontSec,
                               float bottomZ, float topZ, boolean doRight)
{
    uint                i, j;
    linedef_t          *iter;
    sector_t           *scanSec;
    lineowner_t        *base, *own;
    boolean             clockwise = !doRight;
    boolean             stopScan = false;

    if(bottomZ >= topZ)
        return; // Obviously no division.

    // Retrieve the start owner node.
    base = own = R_GetVtxLineOwner(line->L_v(backSide^doRight), line);
    do
    {
        own = own->link[clockwise];

        if(own == base)
            stopScan = true;
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
                            plane_t            *pln = scanSec->SP_plane(j);

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
                            {   // Clip a range bound to this height?
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
                        float               z = scanSec->SP_ceilvisheight;

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

static void calcSegDivisions(walldiv_t* div, const seg_t* seg,
                             const sector_t* frontSec, float bottomZ,
                             float topZ, boolean doRight)
{
    sidedef_t*          side;

    div->num = 0;

    side = SEG_SIDEDEF(seg);

    if(seg->flags & SEGF_POLYOBJ)
        return; // Polyobj segs are never split.

    // Only segs at sidedef ends can/should be split.
    if(!((seg == side->segs[0] && !doRight) ||
         (seg == side->segs[side->segCount -1] && doRight)))
        return;

    doCalcSegDivisions(div, seg->lineDef, seg->side, frontSec, bottomZ,
                       topZ, doRight);
}

/**
 * Division will only happen if it must be done.
 */
static void applyWallHeightDivision(walldiv_t* divs, const seg_t* seg,
                                    const sector_t* frontsec, float low,
                                    float hi)
{
    uint                i;
    walldiv_t*          div;

    if(!seg->lineDef)
        return; // Mini-segs arn't drawn.

    for(i = 0; i < 2; ++i)
    {
        div = &divs[i];
        calcSegDivisions(div, seg, frontsec, low, hi, i);

        // We need to sort the divisions for the renderer.
        if(div->num > 1)
        {
            // Sorting is required. This shouldn't take too long...
            // There seldom are more than one or two divisions.
            qsort(div->pos, div->num, sizeof(float),
                  i!=0 ? DivSortDescend : DivSortAscend);
        }

#ifdef RANGECHECK
{
uint        k;
for(k = 0; k < div->num; ++k)
    if(div->pos[k] > hi || div->pos[k] < low)
    {
        Con_Error("DivQuad: i=%i, pos (%f), hi (%f), low (%f), num=%i\n",
                  i, div->pos[k], hi, low, div->num);
    }
}
#endif
    }
}

static void selectSurfaceColors(const float** topColor,
                                const float** bottomColor, sidedef_t* side,
                                segsection_t section)
{
    // Select the colors for this surface.
    switch(section)
    {
    case SEG_MIDDLE:
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

    case SEG_TOP:
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

    case SEG_BOTTOM:
        // Select the correct colors for this surface.
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

/**
 * This doesn't create a rendering primitive but a vissprite! The vissprite
 * represents the masked poly and will be rendered during the rendering
 * of sprites. This is necessary because all masked polygons must be
 * rendered back-to-front, or there will be alpha artifacts along edges.
 */
void Rend_AddMaskedPoly(const rvertex_t* rvertices,
                        const ColorRawf* rcolors, float wallLength,
                        DGLuint tex, int magMode, float texWidth, float texHeight,
                        const float texOffset[2], blendmode_t blendMode,
                        uint lightListIdx, float glow, boolean masked)
{
    vissprite_t*        vis = R_NewVisSprite();
    int                 i, c;
    float               midpoint[3];

    midpoint[VX] = (rvertices[0].pos[VX] + rvertices[3].pos[VX]) / 2;
    midpoint[VY] = (rvertices[0].pos[VY] + rvertices[3].pos[VY]) / 2;
    midpoint[VZ] = (rvertices[0].pos[VZ] + rvertices[3].pos[VZ]) / 2;

    vis->type = VSPR_MASKED_WALL;
    vis->center[VX] = midpoint[VX];
    vis->center[VY] = midpoint[VY];
    vis->center[VZ] = midpoint[VZ];
    vis->distance = Rend_PointDist2D(midpoint);
    vis->data.wall.tex = tex;
    vis->data.wall.magMode = magMode;
    vis->data.wall.masked = masked;
    for(i = 0; i < 4; ++i)
    {
        vis->data.wall.vertices[i].pos[VX] = rvertices[i].pos[VX];
        vis->data.wall.vertices[i].pos[VY] = rvertices[i].pos[VY];
        vis->data.wall.vertices[i].pos[VZ] = rvertices[i].pos[VZ];

        for(c = 0; c < 4; ++c)
        {
            vis->data.wall.vertices[i].color[c] =
                MINMAX_OF(0, rcolors[i].rgba[c], 1);
        }
    }

    vis->data.wall.texCoord[0][VX] = (texOffset? texOffset[VX] / texWidth : 0);
    vis->data.wall.texCoord[1][VX] =
        vis->data.wall.texCoord[0][VX] + wallLength / texWidth;
    vis->data.wall.texCoord[0][VY] = (texOffset? texOffset[VY] / texHeight : 0);
    vis->data.wall.texCoord[1][VY] =
        vis->data.wall.texCoord[0][VY] +
            (rvertices[3].pos[VZ] - rvertices[0].pos[VZ]) / texHeight;
    vis->data.wall.blendMode = blendMode;

    //// \fixme Semitransparent masked polys arn't lit atm
    if(glow < 1 && lightListIdx && numTexUnits > 1 && envModAdd &&
       !(rcolors[0].rgba[CA] < 1))
    {
        const dynlight_t* dyn = NULL;

        /**
         * The dynlights will have already been sorted so that the brightest
         * and largest of them is first in the list. So grab that one.
         */
        LO_IterateProjections2(lightListIdx, RIT_FirstDynlightIterator, (void*)&dyn);

        vis->data.wall.modTex = dyn->texture;
        vis->data.wall.modTexCoord[0][0] = dyn->s[0];
        vis->data.wall.modTexCoord[0][1] = dyn->s[1];
        vis->data.wall.modTexCoord[1][0] = dyn->t[0];
        vis->data.wall.modTexCoord[1][1] = dyn->t[1];
        for(c = 0; c < 4; ++c)
            vis->data.wall.modColor[c] = dyn->color.rgba[c];
    }
    else
    {
        vis->data.wall.modTex = 0;
    }
}

static void quadTexCoords(rtexcoord_t* tc, const rvertex_t* rverts,
                          float wallLength, const vectorcomp_t topLeft[3])
{
    tc[0].st[0] = tc[1].st[0] =
        rverts[0].pos[VX] - topLeft[VX];
    tc[3].st[1] = tc[1].st[1] =
        rverts[0].pos[VY] - topLeft[VY];
    tc[3].st[0] = tc[2].st[0] = tc[0].st[0] + wallLength;
    tc[2].st[1] = tc[3].st[1] + (rverts[1].pos[VZ] - rverts[0].pos[VZ]);
    tc[0].st[1] = tc[3].st[1] + (rverts[3].pos[VZ] - rverts[2].pos[VZ]);
}

static void quadLightCoords(rtexcoord_t* tc, const float s[2],
                            const float t[2])
{
    tc[1].st[0] = tc[0].st[0] = s[0];
    tc[1].st[1] = tc[3].st[1] = t[0];
    tc[3].st[0] = tc[2].st[0] = s[1];
    tc[2].st[1] = tc[0].st[1] = t[1];
}

#if 0
static void quadShinyMaskTexCoords(rtexcoord_t* tc, const rvertex_t* rverts,
                                   float wallLength, float texWidth,
                                   float texHeight, const pvec2_t texOrigin[2],
                                   const pvec2_t texOffset)
{
    tc[0].st[0] = tc[1].st[0] =
        rverts[0].pos[VX] - texOrigin[0][VX] + texOffset[VX] / texWidth;
    tc[3].st[1] = tc[1].st[1] =
        rverts[0].pos[VY] - texOrigin[0][VY] + texOffset[VY] / texHeight;
    tc[3].st[0] = tc[2].st[0] = tc[0].st[0] + wallLength / texWidth;
    tc[2].st[1] = tc[3].st[1] + (rverts[1].pos[VZ] - rverts[0].pos[VZ]) / texHeight;
    tc[0].st[1] = tc[3].st[1] + (rverts[3].pos[VZ] - rverts[2].pos[VZ]) / texHeight;
}
#endif

static float shinyVertical(float dy, float dx)
{
    return ( (atan(dy/dx) / (PI/2)) + 1 ) / 2;
}

static void quadShinyTexCoords(rtexcoord_t* tc, const rvertex_t* topLeft,
                               const rvertex_t* bottomRight, float wallLength)
{
    uint                i;
    vec2_t              surface, normal, projected, s, reflected, view;
    float               distance, angle, prevAngle = 0;

    // Quad surface vector.
    V2_Set(surface,
           (bottomRight->pos[VX] - topLeft->pos[VX]) / wallLength,
           (bottomRight->pos[VY] - topLeft->pos[VY]) / wallLength);

    V2_Set(normal, surface[VY], -surface[VX]);

    // Calculate coordinates based on viewpoint and surface normal.
    for(i = 0; i < 2; ++i)
    {
        // View vector.
        V2_Set(view, vx - (i == 0? topLeft->pos[VX] : bottomRight->pos[VX]),
                vz - (i == 0? topLeft->pos[VY] : bottomRight->pos[VY]));

        distance = V2_Normalize(view);

        V2_Project(projected, view, normal);
        V2_Subtract(s, projected, view);
        V2_Scale(s, 2);
        V2_Sum(reflected, view, s);

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
            angle + .3f;     /*acos(-dot)/PI*/

        tc[ (i == 0 ? 0 : 2) ].st[1] =
            shinyVertical(vy - bottomRight->pos[VZ], distance);

        // Vertical coordinates.
        tc[ (i == 0 ? 1 : 3) ].st[1] =
            shinyVertical(vy - topLeft->pos[VZ], distance);
    }
}

static void flatShinyTexCoords(rtexcoord_t* tc, const float xyz[3])
{
    vec2_t              view, start;
    float               distance;
    float               offset;

    // View vector.
    V2_Set(view, vx - xyz[VX], vz - xyz[VY]);

    distance = V2_Normalize(view);
    if(distance < 10)
    {
        // Too small distances cause an ugly 'crunch' below and above
        // the viewpoint.
        distance = 10;
    }

    // Offset from the normal view plane.
    V2_Set(start, vx, vz);

    offset = ((start[VY] - xyz[VY]) * sin(.4f)/*viewFrontVec[VX]*/ -
              (start[VX] - xyz[VX]) * cos(.4f)/*viewFrontVec[VZ]*/);

    tc->st[0] = ((shinyVertical(offset, distance) - .5f) * 2) + .5f;
    tc->st[1] = shinyVertical(vy - xyz[VZ], distance);
}

static float getSnapshots(const materialsnapshot_t** msA, const materialsnapshot_t** msB,
    material_t* mat)
{
    assert(msA);
    {
    const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
        MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
    float interPos = 0;

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
}

typedef struct {
    boolean         isWall;
    int             flags; /// @see rendpolyFlags
    blendmode_t     blendMode;
    pvec3_t         texTL, texBR;
    const float*    texOffset, *texScale;
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
    const float*    segLength;
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
    const rtexmapunit_t* primaryDetailRTU = (r_detail && !(p->flags & RPF_SKYMASK) && MSU(msA, MTU_DETAIL).tex)? &MSU(msA, MTU_DETAIL) : NULL;
    const rtexmapunit_t* interRTU         = (!(p->flags & RPF_SKYMASK) && msB && MSU(msB, MTU_PRIMARY).tex)? &MSU(msB, MTU_PRIMARY) : NULL;
    const rtexmapunit_t* interDetailRTU   = (r_detail && !(p->flags & RPF_SKYMASK) && msB && MSU(msB, MTU_DETAIL).tex)? &MSU(msB, MTU_DETAIL) : NULL;
    const rtexmapunit_t* shinyRTU         = (useShinySurfaces && !(p->flags & RPF_SKYMASK) && MSU(msA, MTU_REFLECTION).tex)? &MSU(msA, MTU_REFLECTION) : NULL;
    const rtexmapunit_t* shinyMaskRTU     = (useShinySurfaces && !(p->flags & RPF_SKYMASK) && MSU(msA, MTU_REFLECTION).tex && MSU(msA, MTU_REFLECTION_MASK).tex)? &MSU(msA, MTU_REFLECTION_MASK) : NULL;

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
        Rend_AddMaskedPoly(rvertices, rcolors, *p->segLength,
                           primaryRTU->tex, primaryRTU->magMode, msA->size.width, msA->size.height,
                           p->texOffset,
                           p->blendMode, p->lightListIdx, glowing,
                           !msA->isOpaque);
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

    /// \todo Avoid modifying the RTU write state for the purposes of primitive
    /// specific translations by implementing these as arguments to the RL_Add*
    /// family of functions.
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

    return (p->forceOpaque ||
        !(p->alpha < 1 || !msA->isOpaque || p->blendMode > 0));
}

static boolean doRenderSeg(seg_t* seg,
                           const fvertex_t* from, const fvertex_t* to,
                           float bottom, float top, const pvec3_t normal,
                           float alpha,
                           const float* lightLevel, float lightLevelDL,
                           float lightLevelDR,
                           const float* lightColor,
                           uint lightListIdx,
                           uint shadowListIdx,
                           const walldiv_t* divs,
                           boolean skyMask,
                           boolean addFakeRadio,
                           vec3_t texTL, vec3_t texBR,
                           const float texOffset[2],
                           const float texScale[2],
                           blendmode_t blendMode,
                           const float* color, const float* color2,
                           biassurface_t* bsuf, uint elmIdx /*tmp*/,
                           const materialsnapshot_t* msA, float inter,
                           const materialsnapshot_t* msB,
                           boolean isTwosidedMiddle)
{
    rendworldpoly_params_t params;
    sidedef_t*          side = (seg->lineDef? SEG_SIDEDEF(seg) : NULL);
    rvertex_t*          rvertices;

    // Init the params.
    memset(&params, 0, sizeof(params));

    params.flags = RPF_DEFAULT | (skyMask? RPF_SKYMASK : 0);
    params.isWall = true;
    params.segLength = &seg->length;
    params.forceOpaque = (alpha < 0? true : false);
    params.alpha = (alpha < 0? 1 : alpha);
    params.mapObject = seg;
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
    V3_Set(rvertices[0].pos, from->pos[VX], from->pos[VY], bottom);

    // Top Left.
    V3_Set(rvertices[1].pos, from->pos[VX], from->pos[VY], top);

    // Bottom Right.
    V3_Set(rvertices[2].pos, to->pos[VX], to->pos[VY], bottom);

    // Top Right.
    V3_Set(rvertices[3].pos, to->pos[VX], to->pos[VY], top);

    // Draw this seg.
    if(renderWorldPoly(rvertices, 4, divs, &params, msA, inter, msB))
    {   // Drawn poly was opaque.
        // Render Fakeradio polys for this seg?
        if(!(params.flags & RPF_SKYMASK) && addFakeRadio)
        {
            rendsegradio_params_t radioParams;
            float ll;

            radioParams.linedefLength = &seg->lineDef->length;
            radioParams.botCn = side->bottomCorners;
            radioParams.topCn = side->topCorners;
            radioParams.sideCn = side->sideCorners;
            radioParams.spans = side->spans;
            radioParams.segOffset = &seg->offset;
            radioParams.segLength = &seg->length;
            radioParams.frontSec = seg->SG_frontsector;
            radioParams.backSec = (!isTwosidedMiddle? seg->SG_backsector : NULL);

            /**
             * \kludge Revert the vertex coords as they may have been changed
             * due to height divisions.
             */

            // Bottom Left.
            V3_Set(rvertices[0].pos, from->pos[VX], from->pos[VY], bottom);

            // Top Left.
            V3_Set(rvertices[1].pos, from->pos[VX], from->pos[VY], top);

            // Bottom Right.
            V3_Set(rvertices[2].pos, to->pos[VX], to->pos[VY], bottom);

            // Top Right.
            V3_Set(rvertices[3].pos, to->pos[VX], to->pos[VY], top);

            ll = *lightLevel;
            Rend_ApplyLightAdaptation(&ll);
            if(ll > 0)
            {
                // Determine the shadow properties.
                // \fixme Make cvars out of constants.
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
        return true; // Clip with this solid seg.
    }

    R_FreeRendVertices(rvertices);

    return false; // Do not clip with this.
}

static void renderPlane(subsector_t* ssec, planetype_t type, float height,
    vec3_t tangent, vec3_t bitangent, vec3_t normal,
    material_t* inMat, int sufFlags, short sufInFlags,
    const float sufColor[4], blendmode_t blendMode,
    vec3_t texTL, vec3_t texBR,
    const float texOffset[2], const float texScale[2],
    boolean skyMasked,
    boolean addDLights, boolean addMobjShadows,
    biassurface_t* bsuf, uint elmIdx /*tmp*/,
    int texMode /*tmp*/, boolean flipSurfaceNormal)
{
    float               inter = 0;
    rendworldpoly_params_t params;
    uint                numVertices = ssec->numVertices;
    rvertex_t*          rvertices;
    boolean             blended = false;
    sector_t*           sec = ssec->sector;
    material_t*         mat = NULL;
    materialsnapshot_t* msA = NULL, *msB = NULL;

    memset(&params, 0, sizeof(params));

    params.flags = RPF_DEFAULT;
    params.isWall = false;
    params.mapObject = ssec;
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
    Rend_PreparePlane(rvertices, numVertices, height, ssec, !(normal[VZ] > 0) ^ flipSurfaceNormal);

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
            surface_t* suf = &ssec->sector->planes[elmIdx]->surface;
            const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
                MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
            const materialsnapshot_t* ms = Materials_Prepare(suf->material, spec, true);
            params.glowing = ms->glowing;
        }

        // Dynamic lights?
        if(addDLights && params.glowing < 1 && !(!useDynlights && !useWallGlow))
        {
            params.lightListIdx = LO_ProjectToSurface((PLF_NO_PLANE | (type == PLN_FLOOR? PLF_TEX_FLOOR : PLF_TEX_CEILING)), ssec, 1,
                params.texTL, params.texBR, tangent, bitangent, normal);
        }

        // Mobj shadows?
        if(addMobjShadows && params.glowing < 1 && Rend_MobjShadowsEnabled())
        {
            // Glowing planes inversely diminish shadow strength.
            params.shadowListIdx = R_ProjectShadowsToSurface(ssec, 1 - params.glowing,
                params.texTL, params.texBR, tangent, bitangent, normal);
        }
    }

    renderWorldPoly(rvertices, numVertices, NULL, &params, msA, inter, blended? msB : NULL);

    R_FreeRendVertices(rvertices);
}

static void Rend_RenderPlane(subsector_t* ssec, planetype_t type, float height,
    const_pvec3_t _tangent, const_pvec3_t _bitangent, const_pvec3_t _normal,
    material_t* inMat, int sufFlags, short sufInFlags,
    const float sufColor[4], blendmode_t blendMode,
    const float texOffset[2], const float texScale[2],
    boolean skyMasked, boolean addDLights, boolean addMobjShadows,
    biassurface_t* bsuf, uint elmIdx /*tmp*/,
    int texMode /*tmp*/, boolean flipNormal, boolean clipBackFacing)
{
    vec3_t vec, tangent, bitangent, normal;

    // Must have a visible surface.
    if(!inMat || !Material_IsDrawable(inMat))
        return;

    V3_Set(vec, vx - ssec->midPoint.pos[VX], vz - ssec->midPoint.pos[VY], vy - height);

    /**
     * Flip surface tangent space vectors according to the z positon of the viewer relative
     * to the plane height so that the plane is always visible.
     */
    V3_Copy(tangent, _tangent);
    V3_Copy(bitangent, _bitangent);
    V3_Copy(normal, _normal);
    if(flipNormal)
    {
        /// \fixme This is obviously wrong, do this correctly!
        tangent[VZ] *= -1;
        bitangent[VZ] *= -1;
        normal[VZ] *= -1;
    }

    // Don't bother with planes facing away from the camera.
    if(!(clipBackFacing && !(V3_DotProduct(vec, normal) < 0)))
    {
        float texTL[3], texBR[3];

        // Set the texture origin, Y is flipped for the ceiling.
        V3_Set(texTL, ssec->aaBox.minX,
               ssec->aaBox.arvec2[type == PLN_FLOOR? 1 : 0][VY], height);
        V3_Set(texBR, ssec->aaBox.maxX,
               ssec->aaBox.arvec2[type == PLN_FLOOR? 0 : 1][VY], height);

        renderPlane(ssec, type, height, tangent, bitangent, normal, inMat, sufFlags, sufInFlags,
                    sufColor, blendMode, texTL, texBR, texOffset, texScale,
                    skyMasked, addDLights, addMobjShadows, bsuf, elmIdx, texMode, flipNormal);
    }
}

static boolean isVisible(surface_t* surface, sector_t* frontsec)
{
    return (!(surface->material && !Material_IsDrawable(surface->material)));
}

static boolean rendSegSection(subsector_t* ssec, seg_t* seg,
                              segsection_t section, surface_t* surface,
                              const fvertex_t* from, const fvertex_t* to,
                              float bottom, float top,
                              const float texOffset[2],
                              sector_t* frontsec, boolean softSurface,
                              boolean addDLights, boolean addMobjShadows, short sideFlags)
{
    boolean solidSeg = true;
    float alpha;

    if(!isVisible(surface, frontsec)) return false;
    if(bottom >= top) return true;

    alpha = (section == SEG_MIDDLE? surface->rgba[3] : 1.0f);

    if(section == SEG_MIDDLE && softSurface)
    {
        mobj_t* mo = viewPlayer->shared.mo;
        const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

        /**
         * Can the player walk through this surface?
         * If the player is close enough we should NOT add a
         * solid seg otherwise they'd get HOM when they are
         * directly on top of the line (eg passing through an
         * opaque waterfall).
         */

        if(viewData->current.pos[VZ] > bottom &&
           viewData->current.pos[VZ] < top)
        {
            float delta[2], pos, result[2];
            linedef_t* lineDef = seg->lineDef;

            delta[0] = lineDef->dX;
            delta[1] = lineDef->dY;

            pos = M_ProjectPointOnLine(mo->pos, lineDef->L_v1pos, delta, 0,
                                       result);
            if(pos > 0 && pos < 1)
            {
                float               distance;
                float               minDistance = mo->radius * .8f;

                delta[VX] = mo->pos[VX] - result[VX];
                delta[VY] = mo->pos[VY] - result[VY];

                distance = M_ApproxDistancef(delta[VX], delta[VY]);

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
        uint                lightListIdx = 0, shadowListIdx = 0;
        float               texTL[3], texBR[3], texScale[2],
                            inter = 0;
        walldiv_t           divs[2];
        boolean             forceOpaque = false;
        material_t*         mat = NULL;
        int                 rpFlags = RPF_DEFAULT;
        boolean             isTwoSided = (seg->lineDef &&
            seg->lineDef->L_frontside && seg->lineDef->L_backside)? true:false;
        blendmode_t         blendMode = BM_NORMAL;
        boolean             addFakeRadio = false;
        const float*        color = NULL, *color2 = NULL;
        materialsnapshot_t* msA = NULL, *msB = NULL;

        texScale[0] = ((surface->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
        texScale[1] = ((surface->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        V3_Set(texTL, from->pos[VX], from->pos[VY], top);
        V3_Set(texBR, to->pos  [VX], to->pos  [VY], bottom);

        // Determine which Material to use.
        if(devRendSkyMode && seg->SG_backsector &&
           ((section == SEG_BOTTOM && R_IsSkySurface(&seg->SG_frontsector->SP_floorsurface) &&
                                      R_IsSkySurface(&seg->SG_backsector->SP_floorsurface)) ||
            (section == SEG_TOP    && R_IsSkySurface(&seg->SG_frontsector->SP_ceilsurface) &&
                                      R_IsSkySurface(&seg->SG_backsector->SP_ceilsurface))))
        {
            // Geometry not normally rendered however we do so in dev sky mode.
            mat = seg->SG_frontsector->SP_planematerial(section == SEG_TOP? PLN_CEILING : PLN_FLOOR);
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

            if(section != SEG_MIDDLE || (section == SEG_MIDDLE && !isTwoSided))
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
                lightListIdx = LO_ProjectToSurface(((section == SEG_MIDDLE && isTwoSided)? PLF_SORT_LUMINOSITY_DESC : 0), ssec, 1,
                    texTL, texBR, SEG_SIDEDEF(seg)->SW_middletangent, SEG_SIDEDEF(seg)->SW_middlebitangent, SEG_SIDEDEF(seg)->SW_middlenormal);
            }

            // Mobj shadows?
            if(addMobjShadows && msA->glowing < 1 && Rend_MobjShadowsEnabled())
            {
                // Glowing planes inversely diminish shadow strength.
                shadowListIdx = R_ProjectShadowsToSurface(ssec, 1 - msA->glowing, texTL, texBR,
                    SEG_SIDEDEF(seg)->SW_middletangent, SEG_SIDEDEF(seg)->SW_middlebitangent, SEG_SIDEDEF(seg)->SW_middlenormal);
            }
        
            addFakeRadio = ((addFakeRadio && msA->glowing == 0)? true : false);

            selectSurfaceColors(&color, &color2, SEG_SIDEDEF(seg), section);
        }

        // Check for neighborhood division?
        divs[0].num = divs[1].num = 0;
        if(!(section == SEG_MIDDLE && isTwoSided) &&
           !(seg->lineDef->inFlags & LF_POLYOBJ))
        {
            applyWallHeightDivision(divs, seg, frontsec, bottom, top);
        }

        {
        float deltaL, deltaR, diff;

        LineDef_LightLevelDelta(seg->lineDef, seg->side, &deltaL, &deltaR);

        // Linear interpolation of the linedef light deltas to the edges of the seg.
        diff = deltaR - deltaL;
        deltaR = deltaL + ((seg->offset + seg->length) / seg->lineDef->length) * diff;
        deltaL += (seg->offset / seg->lineDef->length) * diff;

        solidSeg = doRenderSeg(seg,
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
                               seg->bsuf[section], (uint) section,
                               msA, inter, msB,
                               (section == SEG_MIDDLE && isTwoSided));
        }
    }

    return solidSeg;
}

/**
 * Renders the given single-sided seg into the world.
 */
static boolean Rend_RenderSeg(subsector_t* ssec, seg_t* seg)
{
    boolean             solidSeg = true;
    sidedef_t*          side;
    linedef_t*          ldef;
    float               ffloor, fceil;
    boolean             backSide;
    sector_t*           frontsec;
    int                 pid;

    side = SEG_SIDEDEF(seg);
    if(!side)
    {   // A one-way window.
        return false;
    }

    frontsec = ssec->sector;
    backSide = seg->side;
    ldef = seg->lineDef;

    pid = viewPlayer - ddPlayers;
    if(!ldef->mapped[pid])
    {
        ldef->mapped[pid] = true; // This line is now seen in the map.

        // Send a status report.
        if(gx.HandleMapObjectStatusReport)
            gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED,
                                           GET_LINE_IDX(ldef),
                                           DMU_LINEDEF, &pid);
    }

    ffloor = ssec->sector->SP_floorvisheight;
    fceil = ssec->sector->SP_ceilvisheight;

    // Create the wall sections.

    // Middle section.
    if(side->SW_middleinflags & SUIF_PVIS)
    {
        float               texOffset[2];
        surface_t*          surface = &side->SW_middlesurface;

        texOffset[0] = surface->visOffset[0] + seg->offset;
        texOffset[1] = surface->visOffset[1];

        if(ldef->flags & DDLF_DONTPEGBOTTOM)
            texOffset[1] += -(fceil - ffloor);

        Rend_RadioUpdateLinedef(seg->lineDef, seg->side);

        solidSeg = rendSegSection(ssec, seg, SEG_MIDDLE, &side->SW_middlesurface,
                           &seg->SG_v1->v, &seg->SG_v2->v, ffloor, fceil,
                           texOffset,
                           /*temp >*/ frontsec, /*< temp*/
                           false, true, true, side->flags);
    }

    if(P_IsInVoid(viewPlayer))
        solidSeg = false;

    return solidSeg;
}

boolean R_FindBottomTop(linedef_t* lineDef, int side, segsection_t section,
    float matOffsetX, float matOffsetY,
    const plane_t* ffloor, const plane_t* fceil,
    const plane_t* bfloor, const plane_t* bceil,
    boolean unpegBottom, boolean unpegTop,
    boolean stretchMiddle, boolean isSelfRef,
    float* bottom, float* top, float texOffset[2])
{
    switch(section)
    {
    case SEG_TOP:
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

    case SEG_BOTTOM: {
        float t = bfloor->visHeight;

        *bottom = ffloor->visHeight;
        // Can't go over the back ceiling, would induce polygon flaws.
        if(bfloor->visHeight > bceil->visHeight)
            t = bceil->visHeight;

        // Can't go over front ceiling, would induce polygon flaws.
        if(t > fceil->visHeight)
            t = fceil->visHeight;
        *top = t;

        if(*top > *bottom)
        {
            texOffset[VX] = matOffsetX;
            texOffset[VY] = matOffsetY;

            if(bfloor->visHeight > fceil->visHeight)
                texOffset[VY] += -(fceil->visHeight - bfloor->visHeight);

            // Align with normal middle texture?
            if(unpegBottom)
                texOffset[VY] += fceil->visHeight - bfloor->visHeight;

            return true;
        }
        break;
      }
    case SEG_MIDDLE: {
        float ftop, fbottom, vR_ZBottom, vR_ZTop;

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
                if(R_IsSkySurface(&ffloor->surface) && R_IsSkySurface(&bfloor->surface))
                    clipBottom = false;
                if(R_IsSkySurface(&fceil->surface)  && R_IsSkySurface(&bceil->surface))
                    clipTop = false;
            }

            if(LineDef_MiddleMaterialCoords(lineDef, side, bottom, &vR_ZBottom, top,
                   &vR_ZTop, &texOffset[VY], unpegBottom, clipTop, clipBottom))
            {
                texOffset[VX] = matOffsetX;
                //texOffset[VY] = 0;

                return true;
            }
        }
        }
        break;
    }

    return false;
}

/**
 * Render wall sections for a Seg belonging to a two-sided LineDef.
 */
static boolean Rend_RenderSegTwosided(subsector_t* ssec, seg_t* seg)
{
    int                 pid = viewPlayer - ddPlayers;
    float               bottom, top, texOffset[2];
    sector_t*           frontSec, *backSec;
    sidedef_t*          frontSide, *backSide;
    plane_t*            ffloor, *fceil, *bfloor, *bceil;
    linedef_t*          line;
    int                 solidSeg = false;

    frontSide = SEG_SIDEDEF(seg);
    backSide = SEG_SIDEDEF(seg->backSeg);
    frontSec = frontSide->sector;
    backSec = backSide->sector;
    line = seg->lineDef;

    if(!line->mapped[pid])
    {
        line->mapped[pid] = true; // This line is now seen in the map.

        // Send a status report.
        if(gx.HandleMapObjectStatusReport)
            gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED,
                                           GET_LINE_IDX(line),
                                           DMU_LINEDEF, &pid);
    }

    if(backSec == frontSec &&
       !frontSide->SW_topmaterial && !frontSide->SW_bottommaterial &&
       !frontSide->SW_middlematerial)
       return false; // Ugh... an obvious wall seg hack. Best take no chances...

    ffloor = ssec->sector->SP_plane(PLN_FLOOR);
    fceil  = ssec->sector->SP_plane(PLN_CEILING);
    bfloor = backSec->SP_plane(PLN_FLOOR);
    bceil  = backSec->SP_plane(PLN_CEILING);

    if((frontSide->SW_middleinflags & SUIF_PVIS) ||
       (frontSide->SW_topinflags & SUIF_PVIS) ||
       (frontSide->SW_bottominflags & SUIF_PVIS))
        Rend_RadioUpdateLinedef(seg->lineDef, seg->side);

    /**
     * Create the wall sections.
     *
     * We may need multiple wall sections.
     * Determine which parts of the segment are really visible.
     */

    // Middle section.
    if(frontSide->SW_middleinflags & SUIF_PVIS)
    {
        surface_t*          suf = &frontSide->SW_middlesurface;

        if(R_FindBottomTop(seg->lineDef, seg->side, SEG_MIDDLE,
               seg->offset + suf->visOffset[VX], suf->visOffset[VY],
               ffloor, fceil, bfloor, bceil,
               (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
               (line->flags & DDLF_DONTPEGTOP)? true : false,
               (frontSide->flags & SDF_MIDDLE_STRETCH)? true : false,
               LINE_SELFREF(line)? true : false,
               &bottom, &top, texOffset))
        {
            solidSeg = rendSegSection(ssec, seg, SEG_MIDDLE, suf,
                                      &seg->SG_v1->v, &seg->SG_v2->v, bottom, top, texOffset,
                                      frontSec,
                                      (((viewPlayer->shared.flags & (DDPF_NOCLIP|DDPF_CAMERA)) ||
                                        !(line->flags & DDLF_BLOCKING))? true : false),
                                      true, false, frontSide->flags);
            if(solidSeg)
            {
                float               xbottom, xtop;
                surface_t*          suf = &frontSide->SW_middlesurface;

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
        surface_t*      suf = &frontSide->SW_topsurface;

        if(R_FindBottomTop(seg->lineDef, seg->side, SEG_TOP,
               seg->offset + suf->visOffset[VX], suf->visOffset[VY],
               ffloor, fceil, bfloor, bceil,
               (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
               (line->flags & DDLF_DONTPEGTOP)? true : false,
               (frontSide->flags & SDF_MIDDLE_STRETCH)? true : false,
               LINE_SELFREF(line)? true : false,
               &bottom, &top, texOffset))
        {
            rendSegSection(ssec, seg, SEG_TOP, suf,
                           &seg->SG_v1->v, &seg->SG_v2->v, bottom, top, texOffset,
                           frontSec, false, true, true, frontSide->flags);
        }
    }

    // Lower section.
    if(frontSide->SW_bottominflags & SUIF_PVIS)
    {
        surface_t*          suf = &frontSide->SW_bottomsurface;

        if(R_FindBottomTop(seg->lineDef, seg->side, SEG_BOTTOM,
               seg->offset + suf->visOffset[VX], suf->visOffset[VY],
               ffloor, fceil, bfloor, bceil,
               (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
               (line->flags & DDLF_DONTPEGTOP)? true : false,
               (frontSide->flags & SDF_MIDDLE_STRETCH)? true : false,
               LINE_SELFREF(line)? true : false,
               &bottom, &top, texOffset))
        {
            rendSegSection(ssec, seg, SEG_BOTTOM, suf,
                           &seg->SG_v1->v, &seg->SG_v2->v, bottom, top, texOffset,
                           frontSec, false, true, true, frontSide->flags);
        }
    }

    // Can we make this a solid segment in the clipper?
    if(solidSeg == -1)
        return false; // NEVER (we have a hole we couldn't fix).

    if(line->L_frontside && line->L_backside &&
       line->L_frontsector == line->L_backsector)
       return false;

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
        else if((!(bceil->visHeight - bfloor->visHeight > 0) && bfloor->visHeight > ffloor->visHeight && bceil->visHeight < fceil->visHeight &&
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

static void Rend_MarkSegsFacingFront(subsector_t *sub)
{
    uint                i;
    seg_t              *seg, **ptr;

    ptr = sub->segs;
    while(*ptr)
    {
        seg = *ptr;

        // Occlusions can only happen where two sectors contact.
        if(seg->lineDef && !(seg->flags & SEGF_POLYOBJ))
        {
            // Which way should it be facing?
            if(!(segFacingViewerDot(seg->SG_v1pos, seg->SG_v2pos) < 0))
                seg->frameFlags |= SEGINF_FACINGFRONT;
            else
                seg->frameFlags &= ~SEGINF_FACINGFRONT;

            Rend_MarkSegSectionsPVisible(seg);
        }
        ptr++;
     }

    if(sub->polyObj)
    {
        for(i = 0; i < sub->polyObj->numSegs; ++i)
        {
            seg = sub->polyObj->segs[i];

            // Which way should it be facing?
            if(!(segFacingViewerDot(seg->SG_v1pos, seg->SG_v2pos) < 0))
                seg->frameFlags |= SEGINF_FACINGFRONT;
            else
                seg->frameFlags &= ~SEGINF_FACINGFRONT;

            Rend_MarkSegSectionsPVisible(seg);
        }
    }
}

static void occludeFrontFacingSegsInSubsector(const subsector_t* ssec)
{
    seg_t** ptr;

    ptr = ssec->segs;
    while(*ptr)
    {
        seg_t* seg = *ptr;

        if((seg->frameFlags & SEGINF_FACINGFRONT) && seg->lineDef &&
           !(seg->flags & SEGF_POLYOBJ))
        {
            if(!C_CheckViewRelSeg(seg->SG_v1pos[VX], seg->SG_v1pos[VY], seg->SG_v2pos[VX], seg->SG_v2pos[VY]))
                seg->frameFlags &= ~SEGINF_FACINGFRONT;
        }
        ptr++;
    }

    if(ssec->polyObj)
    {
        uint i;
        for(i = 0; i < ssec->polyObj->numSegs; ++i)
        {
            seg_t* seg = ssec->polyObj->segs[i];
            if(seg->frameFlags & SEGINF_FACINGFRONT)
            {
                if(!C_CheckViewRelSeg(seg->SG_v1pos[VX], seg->SG_v1pos[VY], seg->SG_v2pos[VX], seg->SG_v2pos[VY]))
                    seg->frameFlags &= ~SEGINF_FACINGFRONT;
            }
        }
    }
}

/**
 * @defgroup skyCapFlags  Sky Cap Flags
 * @{
 */
#define SKYCAP_LOWER                0x1
#define SKYCAP_UPPER                0x2
/**@}*/

static __inline float skyCapZ(subsector_t* ssec, int skyCap)
{
    const planetype_t plane = (skyCap & SKYCAP_UPPER)? PLN_CEILING : PLN_FLOOR;
    if(!ssec) Con_Error("skyCapZ: Invalid ssec argument (=NULL).");
    if(!ssec->sector || !P_IsInVoid(viewPlayer)) return skyFix[plane].height;
    return ssec->sector->SP_planevisheight(plane);
}

static __inline float skyFixFloorZ(const plane_t* frontFloor, const plane_t* backFloor)
{
    if(P_IsInVoid(viewPlayer))
        return frontFloor->visHeight;
    return skyFix[PLN_FLOOR].height;
}

static __inline float skyFixCeilZ(const plane_t* frontCeil, const plane_t* backCeil)
{
    if(P_IsInVoid(viewPlayer))
        return frontCeil->visHeight;
    return skyFix[PLN_CEILING].height;
}

/**
 * Determine which sky fixes are necessary for the specified @a seg.
 * @param seg  Seg to be evaluated.
 * @return  @see skyCapFlags
 */
static int segSkyFixes(seg_t* seg)
{
    int fixes = 0;
    if(seg && seg->lineDef) // "minisegs" have no linedefs.
    {
        const sector_t* frontSec = seg->SG_frontsector;
        const sector_t* backSec  = seg->SG_backsector;
        if(!backSec || backSec != seg->SG_frontsector)
        {
            const boolean hasSkyFloor   = R_IsSkySurface(&frontSec->SP_floorsurface);
            const boolean hasSkyCeiling = R_IsSkySurface(&frontSec->SP_ceilsurface);

            if(hasSkyFloor || hasSkyCeiling)
            {
                const boolean hasClosedBack = LineDef_BackClosed(seg->lineDef, seg->side, true/*ignore opacity*/);

                // Lower fix?
                if(hasSkyFloor)
                {
                    const plane_t* ffloor = frontSec->SP_plane(PLN_FLOOR);
                    const plane_t* bfloor = backSec? backSec->SP_plane(PLN_FLOOR) : NULL;
                    const float skyZ = skyFixFloorZ(ffloor, bfloor);

                    if(hasClosedBack || (!R_IsSkySurface(&bfloor->surface) || P_IsInVoid(viewPlayer)))
                    {
                        const plane_t* floor = (bfloor && R_IsSkySurface(&bfloor->surface)? bfloor : ffloor);
                        if(floor->visHeight > skyZ)
                            fixes |= SKYCAP_LOWER;
                    }
                }

                // Upper fix?
                if(hasSkyCeiling)
                {
                    const plane_t* fceil = frontSec->SP_plane(PLN_CEILING);
                    const plane_t* bceil = backSec? backSec->SP_plane(PLN_CEILING) : NULL;
                    const float skyZ = skyFixCeilZ(fceil, bceil);

                    if(hasClosedBack || (!R_IsSkySurface(&bceil->surface) || P_IsInVoid(viewPlayer)))
                    {
                        const plane_t* ceil = (bceil && R_IsSkySurface(&bceil->surface)? bceil : fceil);
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
 * @param seg  Seg from which to determine sky fix coordinates.
 * @param skyCap  Either SKYCAP_LOWER or SKYCAP_UPPER (SKYCAP_UPPER has precendence).
 * @param bottom  Z map space coordinate for the bottom of the skyfix written here.
 * @param top  Z map space coordinate for the top of the skyfix written here.
 */
static void skyFixZCoords(seg_t* seg, int skyCap, float* bottom, float* top)
{
    const sector_t* frontSec = seg->SG_frontsector;
    const sector_t* backSec  = seg->SG_backsector;
    const plane_t* ffloor = frontSec->SP_plane(PLN_FLOOR);
    const plane_t* fceil  = frontSec->SP_plane(PLN_CEILING);
    const plane_t* bceil  = backSec? backSec->SP_plane(PLN_CEILING) : NULL;
    const plane_t* bfloor = backSec? backSec->SP_plane(PLN_FLOOR)   : NULL;

    if(!bottom && !top) return;
    if(bottom) *bottom = 0;
    if(top)    *top    = 0;

    if(skyCap & SKYCAP_UPPER)
    {
        if(top)    *top    = skyFixCeilZ(fceil, bceil);
        if(bottom) *bottom = MAX_OF((backSec && R_IsSkySurface(&bceil->surface))? bceil->visHeight : fceil->visHeight, ffloor->visHeight);
    }
    else
    {
        if(top)    *top    = MIN_OF(fceil->visHeight, (backSec? bfloor->visHeight : ffloor->visHeight));
        if(bottom) *bottom = (backSec? ffloor->visHeight : skyFixFloorZ(ffloor, bfloor));
    }
}

static void writeSkyFixGeometry(subsector_t* ssec, int skyCap, int rendPolyFlags)
{
    float zBottom, zTop;
    int segSkyCapFlags;
    rvertex_t verts[4];
    seg_t** segPtr;

    if(!ssec || !ssec->segCount || !ssec->sector) return;
    if(!(skyCap & SKYCAP_LOWER|SKYCAP_UPPER)) return;

    for(segPtr = ssec->segs; *segPtr; segPtr++)
    {
        seg_t* seg = *segPtr;

        // Is a fix or two necessary for this seg?
        segSkyCapFlags = segSkyFixes(seg);
        if(!(segSkyCapFlags & (SKYCAP_LOWER|SKYCAP_UPPER))) continue;

        /**
         * Vertex layout:
         *   1--3
         *   |  |
         *   0--2
         */
        V2_Copy(verts[0].pos, seg->SG_v1pos);
        V2_Copy(verts[1].pos, seg->SG_v1pos);
        V2_Copy(verts[2].pos, seg->SG_v2pos);
        V2_Copy(verts[3].pos, seg->SG_v2pos);

        // First, lower fix:
        if(skyCap & segSkyCapFlags & SKYCAP_LOWER)
        {
            skyFixZCoords(seg, SKYCAP_LOWER, &zBottom, &zTop);
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
            skyFixZCoords(seg, SKYCAP_UPPER, &zBottom, &zTop);
            if(zBottom < zTop)
            {
                verts[0].pos[VZ] = verts[2].pos[VZ] = zBottom;
                verts[1].pos[VZ] = verts[3].pos[VZ] = zTop;
                RL_AddPoly(PT_TRIANGLE_STRIP, rendPolyFlags, 4, verts, NULL);
            }
        }
    }
}

/**
 * Render the lower and/or upper sky cap geometry for @a ssec.
 * @param ssec  Subsector to render geometry for.
 * @param skyCap  @see skyCapFlags
 */
static void rendSubsectorSky(subsector_t* ssec, int skyCap)
{
    const int rendPolyFlags = RPF_DEFAULT | RPF_SKYMASK;

    if(!ssec || !ssec->segCount || !ssec->sector) return;

    // Sky caps are only necessary in sectors with skymasked planes.
    if((skyCap & SKYCAP_LOWER) && !R_IsSkySurface(&ssec->sector->SP_floorsurface))
        skyCap &= ~SKYCAP_LOWER;
    if((skyCap & SKYCAP_UPPER) && !R_IsSkySurface(&ssec->sector->SP_ceilsurface))
        skyCap &= ~SKYCAP_UPPER;

    // Sky fixes:
    writeSkyFixGeometry(ssec, skyCap, rendPolyFlags);

    // Lower cap:
    if(skyCap & SKYCAP_LOWER)
    {
        const float height = skyCapZ(ssec, SKYCAP_LOWER);
        const int numVerts = ssec->numVertices;
        rvertex_t* verts = R_AllocRendVertices(numVerts);

        Rend_PreparePlane(verts, numVerts, height, ssec, false/*clockwise*/);
        RL_AddPoly(PT_FAN, rendPolyFlags, numVerts, verts, NULL);
        R_FreeRendVertices(verts);
    }

    // Upper cap:
    if(skyCap & SKYCAP_UPPER)
    {
        const float height = skyCapZ(ssec, SKYCAP_UPPER);
        const int numVerts = ssec->numVertices;
        rvertex_t* verts = R_AllocRendVertices(numVerts);

        Rend_PreparePlane(verts, numVerts, height, ssec, true/*anticlockwise*/);
        RL_AddPoly(PT_FAN, rendPolyFlags, numVerts, verts, NULL);
        R_FreeRendVertices(verts);
    }
}

static boolean skymaskSegIsVisible(seg_t* seg, boolean clipBackFacing)
{
    linedef_t* lineDef;
    sidedef_t* sideDef;
    sector_t* backSec;
    sector_t* frontSec;

    // "minisegs" have no linedefs.
    if(!seg->lineDef) return false;
    lineDef = seg->lineDef;

    // No SideDef on this side?
    sideDef = SEG_SIDEDEF(seg);
    if(!sideDef) return false;

    // Let's first check which way this seg is facing.
    if(clipBackFacing && !(seg->frameFlags & SEGINF_FACINGFRONT)) return false;

    backSec  = seg->SG_backsector;
    frontSec = seg->SG_frontsector;

    // Avoid obvious hack (best take no chances...).
    return !(backSec == frontSec && !sideDef->SW_topmaterial && !sideDef->SW_bottommaterial &&
            !sideDef->SW_middlematerial);
}

static void Rend_RenderSubsectorSky(subsector_t* ssec)
{
    // Any work to do?
    if(devRendSkyMode || !ssec || !ssec->sector || !R_SectorContainsSkySurfaces(ssec->sector)) return;

    // All geometry uses the same (default) RTU write state.
    RL_LoadDefaultRtus();

    // Write geometry.
    rendSubsectorSky(ssec, SKYCAP_LOWER|SKYCAP_UPPER);
}

/**
 * Creates new occlusion planes from the subsector's sides.
 * Before testing, occlude subsector's backfaces. After testing occlude
 * the remaining faces, i.e. the forward facing segs. This is done before
 * rendering segs, so solid segments cut out all unnecessary oranges.
 */
static void occludeSubsector(const subsector_t* sub, boolean forwardFacing)
{
    float fronth[2], backh[2];
    float* startv, *endv;
    sector_t* front = sub->sector, *back;
    seg_t* seg, **ptr;

    if(devNoCulling || P_IsInVoid(viewPlayer))
        return;

    fronth[0] = front->SP_floorheight;
    fronth[1] = front->SP_ceilheight;

    ptr = sub->segs;
    while(*ptr)
    {
        seg = *ptr;

        // Occlusions can only happen where two sectors contact.
        if(seg->lineDef &&
           seg->SG_backsector && !(seg->flags & SEGF_POLYOBJ) && // Polyobjects don't occlude.
           (forwardFacing == ((seg->frameFlags & SEGINF_FACINGFRONT)? true : false)))
        {
            back = seg->SG_backsector;
            backh[0] = back->SP_floorheight;
            backh[1] = back->SP_ceilheight;
            // Choose start and end vertices so that it's facing forward.
            if(forwardFacing)
            {
                startv = seg->SG_v1pos;
                endv   = seg->SG_v2pos;
            }
            else
            {
                startv = seg->SG_v2pos;
                endv   = seg->SG_v1pos;
            }

            // Do not create an occlusion for sky floors.
            if(!R_IsSkySurface(&back->SP_floorsurface) ||
               !R_IsSkySurface(&front->SP_floorsurface))
            {
                // Do the floors create an occlusion?
                if((backh[0] > fronth[0] && vy <= backh[0]) ||
                   (backh[0] < fronth[0] && vy >= fronth[0]))
                {
                    // Occlude down.
                    C_AddViewRelOcclusion(startv, endv, MAX_OF(fronth[0], backh[0]),
                                          false);
                }
            }

            // Do not create an occlusion for sky ceilings.
            if(!R_IsSkySurface(&back->SP_ceilsurface) ||
               !R_IsSkySurface(&front->SP_ceilsurface))
            {
                // Do the ceilings create an occlusion?
                if((backh[1] < fronth[1] && vy >= backh[1]) ||
                   (backh[1] > fronth[1] && vy <= fronth[1]))
                {
                    // Occlude up.
                    C_AddViewRelOcclusion(startv, endv, MIN_OF(fronth[1], backh[1]),
                                          true);
                }
            }
        }

        ptr++;
    }
}

static void Rend_RenderSubsector(uint ssecidx)
{
    uint                i;
    subsector_t*        ssec = SUBSECTOR_PTR(ssecidx);
    seg_t*              seg, **ptr;
    sector_t*           sect;
    float               sceil, sfloor;

    if(!ssec->sector)
    {   // An orphan subsector.
        return;
    }

    sect = ssec->sector;
    sceil = sect->SP_ceilvisheight;
    sfloor = sect->SP_floorvisheight;

    if(sceil - sfloor <= 0 || ssec->segCount < 3)
    {
        // Skip this, it has no volume.
        // Neighbors handle adding the solid clipper segments.
        return;
    }

    if(!firstsubsector)
    {
        if(!C_CheckSubsector(ssec))
            return; // This isn't visible.
    }
    else
    {
        firstsubsector = false;
    }

    // Mark the sector visible for this frame.
    sect->frameFlags |= SIF_VISIBLE;

    Rend_MarkSegsFacingFront(ssec);

    R_InitForSubsector(ssec);
    Rend_RadioSubsectorEdges(ssec);

    occludeSubsector(ssec, false);
    LO_ClipInSubsector(ssecidx);
    occludeSubsector(ssec, true);

    occludeFrontFacingSegsInSubsector(ssec);

    if(ssec->polyObj)
    {
        // Polyobjs don't obstruct, do clip lights with another algorithm.
        LO_ClipInSubsectorBySight(ssecidx);
    }

    // Mark the particle generators in the sector visible.
    Rend_ParticleMarkInSectorVisible(sect);

    /**
     * Sprites for this subsector have to be drawn.
     * @note
     * Must be done BEFORE the segments of this subsector are added to the
     * clipper. Otherwise the sprites would get clipped by them, and that
     * wouldn't be right.
     * Must be done AFTER the lumobjs have been clipped as this affects the
     * projection of flares.
     */
    R_AddSprites(ssec);

    // Draw the sky cap geometry (and various skymask fixes).
    Rend_RenderSubsectorSky(ssec);

    // Draw the walls.
    ptr = ssec->segs;
    while(*ptr)
    {
        seg = *ptr;
        if(!(seg->flags & SEGF_POLYOBJ)  &&// Not handled here.
           seg->lineDef && // "minisegs" have no linedefs.
           (seg->frameFlags & SEGINF_FACINGFRONT))
        {
            boolean solid;

            if(!seg->SG_backsector || !seg->SG_frontsector)
                solid = Rend_RenderSeg(ssec, seg);
            else
                solid = Rend_RenderSegTwosided(ssec, seg);

            if(solid)
            {
                C_AddViewRelSeg(seg->SG_v1pos[VX], seg->SG_v1pos[VY], seg->SG_v2pos[VX], seg->SG_v2pos[VY]);
            }
        }

        ptr++;
    }

    // Is there a polyobj on board?
    if(ssec->polyObj)
    {
        for(i = 0; i < ssec->polyObj->numSegs; ++i)
        {
            seg = ssec->polyObj->segs[i];
            // Let's first check which way this seg is facing.
            if(seg->frameFlags & SEGINF_FACINGFRONT)
            {
                boolean solid = Rend_RenderSeg(ssec, seg);
                if(solid)
                {
                    C_AddViewRelSeg(seg->SG_v1pos[VX], seg->SG_v1pos[VY], seg->SG_v2pos[VX], seg->SG_v2pos[VY]);
                }
            }
        }
    }

    // Render all planes of this sector.
    for(i = 0; i < sect->planeCount; ++i)
    {
        const plane_t* plane = sect->planes[i];
        const surface_t* suf = &plane->surface;
        boolean isSkyMasked = false;
        boolean addDLights = true;
        boolean flipSurfaceNormal = false;
        boolean clipBackFacing = false;
        float texOffset[2], texScale[2];
        material_t* mat;
        int texMode;

        isSkyMasked = R_IsSkySurface(suf);
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

        V2_Copy(texOffset, suf->visOffset);
        // Add the Y offset to orient the Y flipped texture.
        if(plane->type == PLN_CEILING)
            texOffset[VY] -= ssec->aaBox.maxY - ssec->aaBox.minY;
        // Add the additional offset to align with the worldwide grid.
        texOffset[VX] += ssec->worldGridOffset[VX];
        texOffset[VY] += ssec->worldGridOffset[VY];
        // Inverted.
        texOffset[VY] = -texOffset[VY];

        texScale[VX] = ((suf->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
        texScale[VY] = ((suf->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        Rend_RenderPlane(ssec, plane->type, plane->visHeight, suf->tangent, suf->bitangent, suf->normal,
            mat, suf->flags, suf->inFlags, suf->rgba, suf->blendMode, texOffset, texScale,
            isSkyMasked, addDLights, (i == PLN_FLOOR), ssec->bsuf[plane->planeID], plane->planeID,
            texMode, flipSurfaceNormal, clipBackFacing);
    }
}

static void Rend_RenderNode(uint bspnum)
{
    // If the clipper is full we're pretty much done. This means no geometry
    // will be visible in the distance because every direction has already been
    // fully covered by geometry.
    if(C_IsFull())
        return;

    if(bspnum & NF_SUBSECTOR)
    {
        // We've arrived at a subsector. Render it.
        Rend_RenderSubsector(bspnum & ~NF_SUBSECTOR);
    }
    else
    {
        const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
        node_t* bsp;
        byte side;

        // Descend deeper into the nodes.
        bsp = NODE_PTR(bspnum);

        // Decide which side the view point is on.
        side = R_PointOnSide(viewData->current.pos[VX], viewData->current.pos[VY],
                             &bsp->partition);

        Rend_RenderNode(bsp->children[side]); // Recursively divide front space.
        Rend_RenderNode(bsp->children[side ^ 1]); // ...and back space.
    }
}

static void drawVector(const_pvec3_t origin, const_pvec3_t normal, float scalar, const float color[3])
{
    static const float black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(scalar * normal[VX], scalar * normal[VZ], scalar * normal[VY]);
        glColor4f(color[CR], color[CG], color[CB], 1);
        glVertex3f(0, 0, 0);
    glEnd();
}

static void drawSurfaceTangentSpaceVectors(surface_t* suf, const_pvec3_t origin)
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

    if(devSurfaceVectors == 0) return;

    glDisable(GL_CULL_FACE);

    for(i = 0; i < numSegs; ++i)
    {
        seg_t* seg = &segs[i];
        float x, y, bottom, top;
        sidedef_t* side;
        surface_t* suf;
        vec3_t origin;

        if(!seg->lineDef || !seg->SG_frontsector ||
           (seg->lineDef->inFlags & LF_POLYOBJ))
            continue;

        side = SEG_SIDEDEF(seg);
        x = seg->SG_v1pos[VX] + (seg->SG_v2pos[VX] - seg->SG_v1pos[VX]) / 2;
        y = seg->SG_v1pos[VY] + (seg->SG_v2pos[VY] - seg->SG_v1pos[VY]) / 2;

        if(!seg->SG_backsector)
        {
            bottom = side->sector->SP_floorvisheight;
            top = side->sector->SP_ceilvisheight;
            suf = &side->SW_middlesurface;

            V3_Set(origin, x, y, bottom + (top - bottom) / 2);
            drawSurfaceTangentSpaceVectors(suf, origin);
        }
        else
        {
            if(side->SW_middlesurface.material)
            {
                top = seg->SG_frontsector->SP_ceilvisheight;
                bottom = seg->SG_frontsector->SP_floorvisheight;
                suf = &side->SW_middlesurface;

                V3_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }

            if(seg->SG_backsector->SP_ceilvisheight <
               seg->SG_frontsector->SP_ceilvisheight &&
               !(R_IsSkySurface(&seg->SG_frontsector->SP_ceilsurface) &&
                 R_IsSkySurface(&seg->SG_backsector->SP_ceilsurface)))
            {
                bottom = seg->SG_backsector->SP_ceilvisheight;
                top = seg->SG_frontsector->SP_ceilvisheight;
                suf = &side->SW_topsurface;

                V3_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }

            if(seg->SG_backsector->SP_floorvisheight >
               seg->SG_frontsector->SP_floorvisheight &&
               !(R_IsSkySurface(&seg->SG_frontsector->SP_floorsurface) &&
                 R_IsSkySurface(&seg->SG_backsector->SP_floorsurface)))
            {
                bottom = seg->SG_frontsector->SP_floorvisheight;
                top = seg->SG_backsector->SP_floorvisheight;
                suf = &side->SW_bottomsurface;

                V3_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }
        }
    }

    for(i = 0; i < numSSectors; ++i)
    {
        subsector_t* ssec = &ssectors[i];
        uint j;

        if(!ssec->sector) continue;

        for(j = 0; j < ssec->sector->planeCount; ++j)
        {
            plane_t* pln = ssec->sector->SP_plane(j);
            vec3_t origin;

            V3_Set(origin, ssec->midPoint.pos[VX], ssec->midPoint.pos[VY], pln->visHeight);
            if(pln->type != PLN_MID && R_IsSkySurface(&pln->surface))
                origin[VZ] = skyFix[pln->type].height;

            drawSurfaceTangentSpaceVectors(&pln->surface, origin);
        }
    }

    for(i = 0; i < numPolyObjs; ++i)
    {
        const polyobj_t* po = polyObjs[i];
        const sector_t* sec = po->subsector->sector;
        float zPos = sec->SP_floorheight + (sec->SP_ceilheight - sec->SP_floorheight)/2;
        vec3_t origin;
        uint j;

        for(j = 0; j < po->numSegs; ++j)
        {
            seg_t* seg = po->segs[j];
            linedef_t* lineDef = seg->lineDef;

            V3_Set(origin, (lineDef->L_v2pos[VX]+lineDef->L_v1pos[VX])/2,
                           (lineDef->L_v2pos[VY]+lineDef->L_v1pos[VY])/2, zPos);
            drawSurfaceTangentSpaceVectors(&SEG_SIDEDEF(seg)->SW_middlesurface, origin);
        }
    }

    glEnable(GL_CULL_FACE);
}

static void getVertexPlaneMinMax(const vertex_t* vtx, float* min, float* max)
{
    lineowner_t*        vo, *base;

    if(!vtx || (!min && !max))
        return; // Wha?

    vo = base = vtx->lineOwners;

    do
    {
        linedef_t*          li = vo->lineDef;

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

static void drawVertexPoint(const vertex_t* vtx, float z, float alpha)
{
    glBegin(GL_POINTS);
        glColor4f(.7f, .7f, .2f, alpha * 2);
        glVertex3f(vtx->V_pos[VX], z, vtx->V_pos[VY]);
    glEnd();
}

static void drawVertexBar(const vertex_t* vtx, float bottom, float top,
                          float alpha)
{
#define EXTEND_DIST     64

    static const float  black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(vtx->V_pos[VX], bottom - EXTEND_DIST, vtx->V_pos[VY]);
        glColor4f(1, 1, 1, alpha);
        glVertex3f(vtx->V_pos[VX], bottom, vtx->V_pos[VY]);
        glVertex3f(vtx->V_pos[VX], bottom, vtx->V_pos[VY]);
        glVertex3f(vtx->V_pos[VX], top, vtx->V_pos[VY]);
        glVertex3f(vtx->V_pos[VX], top, vtx->V_pos[VY]);
        glColor4fv(black);
        glVertex3f(vtx->V_pos[VX], top + EXTEND_DIST, vtx->V_pos[VY]);
    glEnd();

#undef EXTEND_DIST
}

static void drawVertexIndex(const vertex_t* vtx, float z, float scale, float alpha)
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
    glTranslatef(vtx->V_pos[VX], z, vtx->V_pos[VY]);
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

static int drawVertex1(linedef_t* li, void* context)
{
    vertex_t*           vtx = li->L_v1;
    polyobj_t*          po = context;
    float               dist2D =
        M_ApproxDistancef(vx - vtx->V_pos[VX], vz - vtx->V_pos[VY]);

    if(dist2D < MAX_VERTEX_POINT_DIST)
    {
        float               alpha = 1 - dist2D / MAX_VERTEX_POINT_DIST;

        if(alpha > 0)
        {
            float               bottom = po->subsector->sector->SP_floorvisheight;
            float               top = po->subsector->sector->SP_ceilvisheight;

            if(devVertexBars)
                drawVertexBar(vtx, bottom, top, MIN_OF(alpha, .15f));

            drawVertexPoint(vtx, bottom, alpha * 2);
        }
    }

    if(devVertexIndices)
    {
        float               eye[3], pos[3], dist3D;

        eye[VX] = vx;
        eye[VY] = vz;
        eye[VZ] = vy;

        pos[VX] = vtx->V_pos[VX];
        pos[VY] = vtx->V_pos[VY];
        pos[VZ] = po->subsector->sector->SP_floorvisheight;

        dist3D = M_Distance(pos, eye);

        if(dist3D < MAX_VERTEX_POINT_DIST)
        {
            drawVertexIndex(vtx, pos[VZ], dist3D / (theWindow->geometry.size.width / 2),
                            1 - dist3D / MAX_VERTEX_POINT_DIST);
        }
    }

    return false; // Continue iteration.
}

int drawPolyObjVertexes(polyobj_t* po, void* context)
{
    return P_PolyobjLinesIterator(po, drawVertex1, po);
}

/**
 * Draw the various vertex debug aids.
 */
void Rend_Vertexes(void)
{
    uint                i;
    float               oldPointSize, oldLineWidth = 1;
    AABoxf box;

    if(!devVertexBars && !devVertexIndices)
        return;

    glDisable(GL_DEPTH_TEST);

    if(devVertexBars)
    {
        glEnable(GL_LINE_SMOOTH);
        oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
        DGL_SetFloat(DGL_LINE_WIDTH, 2);

        for(i = 0; i < numVertexes; ++i)
        {
            vertex_t*           vtx = &vertexes[i];
            float               alpha;

            if(!vtx->lineOwners)
                continue; // Not a linedef vertex.
            if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ)
                continue; // A polyobj linedef vertex.

            alpha = 1 - M_ApproxDistancef(vx - vtx->V_pos[VX],
                                          vz - vtx->V_pos[VY]) / MAX_VERTEX_POINT_DIST;
            alpha = MIN_OF(alpha, .15f);

            if(alpha > 0)
            {
                float               bottom, top;

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

    for(i = 0; i < numVertexes; ++i)
    {
        vertex_t*           vtx = &vertexes[i];
        float               dist;

        if(!vtx->lineOwners)
            continue; // Not a linedef vertex.
        if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ)
            continue; // A polyobj linedef vertex.

        dist = M_ApproxDistancef(vx - vtx->V_pos[VX], vz - vtx->V_pos[VY]);

        if(dist < MAX_VERTEX_POINT_DIST)
        {
            float               bottom;

            bottom = DDMAXFLOAT;
            getVertexPlaneMinMax(vtx, &bottom, NULL);

            drawVertexPoint(vtx, bottom, (1 - dist / MAX_VERTEX_POINT_DIST) * 2);
        }
    }


    if(devVertexIndices)
    {
        float               eye[3];

        eye[VX] = vx;
        eye[VY] = vz;
        eye[VZ] = vy;

        for(i = 0; i < numVertexes; ++i)
        {
            vertex_t* vtx = &vertexes[i];
            float pos[3], dist;

            if(!vtx->lineOwners) continue; // Not a linedef vertex.
            if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ) continue; // A polyobj linedef vertex.

            pos[VX] = vtx->V_pos[VX];
            pos[VY] = vtx->V_pos[VY];
            pos[VZ] = DDMAXFLOAT;
            getVertexPlaneMinMax(vtx, &pos[VZ], NULL);

            dist = M_Distance(pos, eye);

            if(dist < MAX_VERTEX_POINT_DIST)
            {
                float alpha, scale;

                alpha = 1 - dist / MAX_VERTEX_POINT_DIST;
                scale = dist / (theWindow->geometry.size.width / 2);

                drawVertexIndex(vtx, pos[VZ], scale, alpha);
            }
        }
    }

    // Next, the vertexes of all nearby polyobjs.
    box.minX = vx - MAX_VERTEX_POINT_DIST;
    box.minY = vy - MAX_VERTEX_POINT_DIST;
    box.maxX = vx + MAX_VERTEX_POINT_DIST;
    box.maxY = vy + MAX_VERTEX_POINT_DIST;
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

        // We don't want subsector clipchecking for the first subsector.
        firstsubsector = true;
        if(numNodes != 0)
        {
            Rend_RenderNode(numNodes - 1);
        }
        else
        {
            Rend_RenderSubsector(0);
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
    Rend_Vertexes(); // World vertex positions/indices.
    Rend_RenderGenerators(); // Particle generator origins.

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
    int                 j;
    int                 mapAmbient;
    float               f;
    gamemap_t          *map = P_GetCurrentMap();

    memset(lightModRange, 0, sizeof(float) * 255);

    mapAmbient = P_GetMapAmbientLightLevel(map);
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
            if(lightRangeCompression >= 0) // Brighten dark areas.
                f = (float) (255 - j) * lightRangeCompression;
            else // Darken bright areas.
                f = (float) -j * -lightRangeCompression;
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

/**
 * Applies the offset from the lightModRangeto the given light value.
 *
 * The lightModRange is used to implement (precalculated) ambient light
 * limit, light range compression and light range shift.
 *
 * \note There is no need to clamp the result. Since the offset values in
 *       the lightModRange have already been clamped so that the resultant
 *       lightvalue is NEVER outside the range 0-254 when the original
 *       lightvalue is used as the index.
 *
 * @param lightvar      Ptr to the value to apply the adaptation to.
 */
void Rend_ApplyLightAdaptation(float *lightvar)
{
    int                 lightval;

    if(lightvar == NULL)
        return; // Can't apply adaptation with a NULL val ptr...

    lightval = ROUND(255.0f * *lightvar);
    if(lightval > 254)
        lightval = 254;
    else if(lightval < 0)
        lightval = 0;

    *lightvar += lightModRange[lightval];
}

/**
 * Same as above but instead of applying light adaptation to the var directly
 * it returns the light adaptation value for the passed light value.
 *
 * @param lightvalue    Light value to look up the adaptation value of.
 * @return int          Adaptation value for the passed light value.
 */
float Rend_GetLightAdaptVal(float lightvalue)
{
    int                 lightval;

    lightval = ROUND(255.0f * lightvalue);
    if(lightval > 254)
        lightval = 254;
    else if(lightval < 0)
        lightval = 0;

    return lightModRange[lightval];
}

/**
 * Draws the lightModRange (for debug)
 */
void R_DrawLightRange(void)
{
#define bWidth          1.0f
#define bHeight         (bWidth * 255.0f)
#define BORDER          20

    int                 i;
    float               c, off;
    ui_color_t          color;

    if(!devLightModRange)
        return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->geometry.size.width, theWindow->geometry.size.height, 0, -1, 1);

    glTranslatef(BORDER, BORDER, 0);

    color.red = 0.2f;
    color.green = 0;
    color.blue = 0.6f;

    // Draw an outside border.
    glColor4f(1, 1, 0, 1);
    glBegin(GL_LINES);
        glVertex2f(-1, -1);
        glVertex2f(255 + 1, -1);
        glVertex2f(255 + 1,  -1);
        glVertex2f(255 + 1,  bHeight + 1);
        glVertex2f(255 + 1,  bHeight + 1);
        glVertex2f(-1,  bHeight + 1);
        glVertex2f(-1,  bHeight + 1);
        glVertex2f(-1, -1);
    glEnd();

    glBegin(GL_QUADS);
    for(i = 0, c = 0; i < 255; ++i, c += (1.0f/255.0f))
    {
        // Get the result of the source light level + offset.
        off = lightModRange[i];

        glColor4f(c + off, c + off, c + off, 1);
        glVertex2f(i * bWidth, 0);
        glVertex2f(i * bWidth + bWidth, 0);
        glVertex2f(i * bWidth + bWidth,  bHeight);
        glVertex2f(i * bWidth, bHeight);
    }
    glEnd();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

#undef bWidth
#undef bHeight
#undef BORDER
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
 * @param pos3f         Coordinates of the center of the box (in "world"
 *                      coordinates [VX, VY, VZ]).
 * @param w             Width of the box.
 * @param l             Length of the box.
 * @param h             Height of the box.
 * @param color3f       Color to make the box (uniform vertex color).
 * @param alpha         Alpha to make the box (uniform vertex color).
 * @param br            Border amount to overlap box faces.
 * @param alignToBase   If @c true, align the base of the box
 *                      to the Z coordinate.
 */
void Rend_DrawBBox(const float pos3f[3], float w, float l, float h, float a,
                   const float color3f[3], float alpha, float br,
                   boolean alignToBase)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    if(alignToBase)
        // The Z coordinate is to the bottom of the object.
        glTranslatef(pos3f[VX], pos3f[VZ] + h, pos3f[VY]);
    else
        glTranslatef(pos3f[VX], pos3f[VZ], pos3f[VY]);

    glRotatef(0, 0, 0, 1);
    glRotatef(0, 1, 0, 0);
    glRotatef(a, 0, 1, 0);

    glScalef(w - br - br, h - br - br, l - br - br);
    glColor4f(color3f[0], color3f[1], color3f[2], alpha);

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
 * @param color3f       Color to make the box (uniform vertex color).
 * @param alpha         Alpha to make the box (uniform vertex color).
 */
void Rend_DrawArrow(const float pos3f[3], float a, float s,
                    const float color3f[3], float alpha)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glTranslatef(pos3f[VX], pos3f[VZ], pos3f[VY]);

    glRotatef(0, 0, 0, 1);
    glRotatef(0, 1, 0, 0);
    glRotatef(a, 0, 1, 0);

    glScalef(s, 0, s);

    glBegin(GL_TRIANGLES);
    {
        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f( 1.0f, 1.0f,-1.0f);  // L

        glColor4f(color3f[0], color3f[1], color3f[2], alpha);
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
    float size, alpha, eye[3];

    // We don't want the console player.
    if(mo == ddPlayers[consolePlayer].shared.mo)
        return false; // Continue iteration.
    // Is it vissible?
    if(!(mo->subsector && mo->subsector->sector->frameFlags & SIF_VISIBLE))
        return false; // Continue iteration.

    eye[VX] = vx;
    eye[VY] = vz;
    eye[VZ] = vy;

    alpha = 1 - ((M_Distance(mo->pos, eye)/(theWindow->geometry.size.width/2))/4);
    if(alpha < .25f)
        alpha = .25f; // Don't make them totally invisible.

    // Draw a bounding box in an appropriate color.
    size = mo->radius;
    Rend_DrawBBox(mo->pos, size, size, mo->height/2, 0,
                  (mo->ddFlags & DDMF_MISSILE)? yellow :
                  (mo->ddFlags & DDMF_SOLID)? green : red,
                  alpha, .08f, true);

    Rend_DrawArrow(mo->pos, ((mo->angle + ANG45 + ANG90) / (float) ANGLE_MAX *-360), size*1.25,
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
    float eye[3];
    uint i;

    if(!devMobjBBox && !devPolyobjBBox)
        return;

#ifndef _DEBUG
    // Bounding boxes are not allowed in non-debug netgames.
    if(netGame) return;
#endif

    if(!dlBBox)
        dlBBox = constructBBox(0, .08f);

    eye[VX] = vx;
    eye[VY] = vz;
    eye[VZ] = vy;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":bbox"));
    spec = Materials_VariantSpecificationForContext(MC_SPRITE, 0, 0, 0, 0,
        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 1, -2, -1, true, true, true, false);
    ms = Materials_Prepare(mat, spec, true);

    GL_BindTexture(MSU_gltexture(ms, MTU_PRIMARY), MSU(ms, MTU_PRIMARY).magMode);
    GL_BlendMode(BM_ADD);

    if(devMobjBBox)
        P_IterateThinkers(gx.MobjThinker, 0x1, drawMobjBBox, NULL);

    if(devPolyobjBBox)
    for(i = 0; i < numPolyObjs; ++i)
    {
        const polyobj_t* po = polyObjs[i];
        const sector_t* sec = po->subsector->sector;
        float width  = (po->aaBox.maxX - po->aaBox.minX)/2;
        float length = (po->aaBox.maxY - po->aaBox.minY)/2;
        float height = (sec->SP_ceilheight - sec->SP_floorheight)/2;
        float pos[3], alpha;

        pos[VX] = po->aaBox.minX + width;
        pos[VY] = po->aaBox.minY + length;
        pos[VZ] = sec->SP_floorheight;

        alpha = 1 - ((M_Distance(pos, eye)/(theWindow->geometry.size.width/2))/4);
        if(alpha < .25f)
            alpha = .25f; // Don't make them totally invisible.

        Rend_DrawBBox(pos, width, length, height, 0, yellow, alpha, .08f, true);

        {uint j;
        for(j = 0; j < po->numSegs; ++j)
        {
            seg_t* seg = po->segs[j];
            linedef_t* lineDef = seg->lineDef;
            float width  = (lineDef->aaBox.maxX - lineDef->aaBox.minX)/2;
            float length = (lineDef->aaBox.maxY - lineDef->aaBox.minY)/2;
            float pos[3];

            /** Draw a bounding box for the lineDef.
            pos[VX] = lineDef->aaBox.minX+width;
            pos[VY] = lineDef->aaBox.minY+length;
            pos[VZ] = sec->SP_floorheight;
            Rend_DrawBBox(pos, width, length, height, 0, red, alpha, .08f, true);
            */

            pos[VX] = (lineDef->L_v2pos[VX]+lineDef->L_v1pos[VX])/2;
            pos[VY] = (lineDef->L_v2pos[VY]+lineDef->L_v1pos[VY])/2;
            pos[VZ] = sec->SP_floorheight;
            width = 0;
            length = lineDef->length/2;

            Rend_DrawBBox(pos, width, length, height, BANG2DEG(BANG_90-lineDef->angle), green, alpha, 0, true);
        }}
    }

    GL_BlendMode(BM_NORMAL);

    glEnable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}
