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

extern int useDynLights, translucentIceCorpse;
extern int loMaxRadius;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean usingFog = false; // Is the fog in use?
float fogColor[4];
float fieldOfView = 95.0f;
byte smoothTexAnim = true;
int useShinySurfaces = true;
// Glowing textures are always rendered fullbright.
float glowingTextures = 1.0f;

float vx, vy, vz, vang, vpitch;
float viewsidex, viewsidey;

byte freezeRLs = false;
int devRendSkyMode = false;
byte devRendSkyAlways = false;

int missileBlend = 1;
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
byte devSurfaceNormals = 0; // @c 1= Draw world surface normal tails.
byte devNoTexFix = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstsubsector; // No range checking for the first one.

// CODE --------------------------------------------------------------------

void Rend_Register(void)
{
    C_VAR_FLOAT("rend-camera-fov", &fieldOfView, 0, 1, 179);
    C_VAR_BYTE("rend-tex-anim-smooth", &smoothTexAnim, 0, 0, 1);
    C_VAR_INT("rend-tex-shiny", &useShinySurfaces, 0, 0, 1);
    C_VAR_FLOAT2("rend-light-compression", &lightRangeCompression, 0, -1, 1, Rend_CalcLightModRange);
    C_VAR_INT2("rend-light-ambient", &ambientLight, 0, 0, 255, Rend_CalcLightModRange);
    C_VAR_BYTE2("rend-light-sky", &rendSkyLight, 0, 0, 1, LG_MarkAllForUpdate);
    C_VAR_BYTE2("rend-light-sky-auto", &rendSkyLightAuto, 0, 0, 1, LG_MarkAllForUpdate);
    C_VAR_BYTE2("rend-light-sky-balance", &rendSkyLightBalance, 0, 0, 1, LG_MarkAllForUpdate);
    C_VAR_FLOAT("rend-light-wall-angle", &rendLightWallAngle, CVF_NO_MAX, 0, 0);
    C_VAR_BYTE("rend-light-wall-angle-smooth", &rendLightWallAngleSmooth, 0, 0, 1);
    C_VAR_FLOAT("rend-light-attenuation", &rendLightDistanceAttentuation, CVF_NO_MAX, 0, 0);

    C_VAR_INT("rend-dev-sky", &devRendSkyMode, CVF_NO_ARCHIVE, 0, 2);
    C_VAR_BYTE("rend-dev-sky-always", &devRendSkyAlways, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-freeze", &freezeRLs, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-cull-subsectors", &devNoCulling,CVF_NO_ARCHIVE,0,1);
    C_VAR_INT("rend-dev-mobj-bbox", &devMobjBBox, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-mobj-show-vlights", &devMobjVLights, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-polyobj-bbox", &devPolyobjBBox, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-light-mod", &devLightModRange, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-tex-showfix", &devNoTexFix, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-blockmap", &bmapShowDebug, CVF_NO_ARCHIVE, 0, 3);
    C_VAR_FLOAT("rend-dev-blockmap-debug", &bmapDebugSize, CVF_NO_ARCHIVE, .1f, 100);
    C_VAR_BYTE("rend-dev-vertex-show-indices", &devVertexIndices, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-vertex-show-bars", &devVertexBars, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-surface-show-normals", &devSurfaceNormals, CVF_NO_ARCHIVE, 0, 1);

    RL_Register();
    DL_Register();
    SB_Register();
    LG_Register();
    R_SkyRegister();
    Rend_ModelRegister();
    Rend_ParticleRegister();
    Rend_RadioRegister();
    Rend_ShadowRegister();
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
    C_Init(); // Clipper.
    RL_Init(); // Rendering lists.
}

/**
 * Used to be called before starting a new map.
 */
void Rend_Reset(void)
{
    // Textures are deleted (at least skies need this???).
    GL_ClearRuntimeTextures();
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

void Rend_VertexColorsGlow(rcolor_t* colors, size_t num, float glow)
{
    size_t i;
    for(i = 0; i < num; ++i)
    {
        rcolor_t* c = &colors[i];
        c->rgba[CR] = c->rgba[CG] = c->rgba[CB] = glow;
    }
}

void Rend_VertexColorsAlpha(rcolor_t* colors, size_t num, float alpha)
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

static void lightVertex(rcolor_t* color, const rvertex_t* vtx, float lightLevel,
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

static void lightVertices(size_t num, rcolor_t* colors, const rvertex_t* verts,
    float lightLevel, const float* ambientColor)
{
    size_t i;
    for(i = 0; i < num; ++i)
    {
        lightVertex(colors+i, verts+i, lightLevel, ambientColor);
    }
}

void Rend_VertexColorsApplyTorchLight(rcolor_t* colors, const rvertex_t* vertices,
    size_t numVertices)
{
    ddplayer_t* ddpl = &viewPlayer->shared;

    if(!ddpl->fixedColorMap)
        return; // No need, its disabled.

    { size_t i;
    for(i = 0; i < numVertices; ++i)
    {
        const rvertex_t* vtx = &vertices[i];
        rcolor_t* c = &colors[i];
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

/**
 * \fixme No need to do this each frame. Set a flag in sidedef_t->flags to
 * denote this. Is sensitive to plane heights, surface properties
 * (e.g. alpha) and surface texture properties.
 */
boolean Rend_DoesMidTextureFillGap(linedef_t *line, int backside, boolean ignoreAlpha)
{
    // Check for unmasked midtextures on twosided lines that completely
    // fill the gap between floor and ceiling (we don't want to give away
    // the location of any secret areas (false walls)).
    if(line->L_backside)
    {
        sector_t* front = line->L_sector(backside);
        sector_t* back  = line->L_sector(backside^1);
        sidedef_t* side  = line->L_side(backside);

        if(side->SW_middlematerial)
        {
            material_t* mat = side->SW_middlematerial;
            material_snapshot_t ms;

            // Ensure we have up to date info.
            Materials_Prepare(&ms, mat, true,
                Materials_VariantSpecificationForContext(MC_MAPSURFACE,
                    0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, true, true, false, false));

            if(ignoreAlpha || (ms.isOpaque && !side->SW_middleblendmode && side->SW_middlergba[3] >= 1))
            {
                float openTop[2], matTop[2];
                float openBottom[2], matBottom[2];

                if(side->flags & SDF_MIDDLE_STRETCH)
                    return true;

                openTop[0] = openTop[1] = matTop[0] = matTop[1] =
                    MIN_OF(back->SP_ceilvisheight, front->SP_ceilvisheight);
                openBottom[0] = openBottom[1] = matBottom[0] = matBottom[1] =
                    MAX_OF(back->SP_floorvisheight, front->SP_floorvisheight);

                // Could the mid texture fill enough of this gap for us
                // to consider it completely closed?
                if(ms.height >= (openTop[0] - openBottom[0]) &&
                   ms.height >= (openTop[1] - openBottom[1]))
                {
                    // Possibly. Check the placement of the mid texture.
                    if(Rend_MidMaterialPos
                       (&matBottom[0], &matBottom[1], &matTop[0], &matTop[1],
                        NULL, side->SW_middlevisoffset[VY], ms.height,
                        0 != (line->flags & DDLF_DONTPEGBOTTOM),
                        !(R_IsSkySurface(&front->SP_ceilsurface) &&
                          R_IsSkySurface(&back->SP_ceilsurface)),
                        !(R_IsSkySurface(&front->SP_floorsurface) &&
                          R_IsSkySurface(&back->SP_floorsurface))))
                    {
                        if(matTop[0] >= openTop[0] &&
                           matTop[1] >= openTop[1] &&
                           matBottom[0] <= openBottom[0] &&
                           matBottom[1] <= openBottom[1])
                            return true;
                    }
                }
            }
        }
    }

    return false;
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
    if((R_IsSkySurface(&fceil->surface) && R_IsSkySurface(&bceil->surface)) ||
       (R_IsSkySurface(&bceil->surface) && (side->SW_topsurface.inFlags & SUIF_FIX_MISSING_MATERIAL)) ||
       (fceil->visHeight <= bceil->visHeight))
        side->SW_topsurface   .inFlags &= ~SUIF_PVIS;

    // Bottom.
    if((R_IsSkySurface(&ffloor->surface) && R_IsSkySurface(&bfloor->surface)) ||
       (R_IsSkySurface(&bfloor->surface) && (side->SW_bottomsurface.inFlags & SUIF_FIX_MISSING_MATERIAL)) ||
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

/**
 * Calculates the placement for a middle texture (top, bottom, offset).
 * texoffy may be NULL.
 * Returns false if the middle texture isn't visible (in the opening).
 */
int Rend_MidMaterialPos(float* bottomleft, float* bottomright,
                        float* topleft, float* topright, float* texoffy,
                        float tcyoff, float texHeight, boolean lowerUnpeg,
                        boolean clipTop, boolean clipBottom)
{
    int                 side;
    float               openingTop, openingBottom;
    boolean             visible[2] = {false, false};

    for(side = 0; side < 2; ++side)
    {
        openingTop = *(side? topright : topleft);
        openingBottom = *(side? bottomright : bottomleft);

        if(openingTop <= openingBottom)
            continue;

        // Else the mid texture is visible on this side.
        visible[side] = true;

        if(side == 0 && texoffy)
            *texoffy = 0;

        if(lowerUnpeg)
        {
            *(side? bottomright : bottomleft) += tcyoff;
            *(side? topright : topleft) =
                *(side? bottomright : bottomleft) + texHeight;
        }
        else
        {
            *(side? topright : topleft) += tcyoff;
            *(side? bottomright : bottomleft) =
                *(side? topright : topleft) - texHeight;
        }

        // Clip it.
        if(clipBottom)
            if(*(side? bottomright : bottomleft) < openingBottom)
            {
                *(side? bottomright : bottomleft) = openingBottom;
            }

        if(clipTop)
            if(*(side? topright : topleft) > openingTop)
            {
                if(side == 0 && texoffy)
                    *texoffy += *(side? topright : topleft) - openingTop;
                *(side? topright : topleft) = openingTop;
            }
    }

    return (visible[0] || visible[1]);
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

boolean RLIT_DynGetFirst(const dynlight_t* dyn, void* data)
{
    dynlight_t**        ptr = data;

    *ptr = (dynlight_t*) dyn;
    return false; // Stop iteration.
}

/**
 * This doesn't create a rendering primitive but a vissprite! The vissprite
 * represents the masked poly and will be rendered during the rendering
 * of sprites. This is necessary because all masked polygons must be
 * rendered back-to-front, or there will be alpha artifacts along edges.
 */
void Rend_AddMaskedPoly(const rvertex_t* rvertices,
                        const rcolor_t* rcolors, float wallLength,
                        float texWidth, float texHeight,
                        const float texOffset[2], blendmode_t blendMode,
                        uint lightListIdx, float glow, boolean masked,
                        const rtexmapunit_t rTU[NUM_TEXMAP_UNITS])
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
    vis->data.wall.tex = rTU[TU_PRIMARY].tex;
    vis->data.wall.magMode = rTU[TU_PRIMARY].magMode;
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
        dynlight_t*         dyn = NULL;

        /**
         * The dynlights will have already been sorted so that the brightest
         * and largest of them is first in the list. So grab that one.
         */
        DL_ListIterator(lightListIdx, &dyn, RLIT_DynGetFirst);

        vis->data.wall.modTex = dyn->texture;
        vis->data.wall.modTexCoord[0][0] = dyn->s[0];
        vis->data.wall.modTexCoord[0][1] = dyn->s[1];
        vis->data.wall.modTexCoord[1][0] = dyn->t[0];
        vis->data.wall.modTexCoord[1][1] = dyn->t[1];
        for(c = 0; c < 3; ++c)
            vis->data.wall.modColor[c] = dyn->color[c];
        vis->data.wall.modColor[3] = 1;
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

typedef struct {
    uint            lastIdx;
    const rvertex_t* rvertices;
    uint            numVertices, realNumVertices;
    const float*    texTL, *texBR;
    boolean         isWall;
    const walldiv_t* divs;
} dynlightiterparams_t;

boolean RLIT_DynLightWrite(const dynlight_t* dyn, void* data)
{
    dynlightiterparams_t* params = data;

    // If multitexturing is in use, we skip the first light.
    if(!(RL_IsMTexLights() && params->lastIdx == 0))
    {
        uint                i;
        rvertex_t*          rvertices;
        rtexcoord_t*        rtexcoords;
        rcolor_t*           rcolors;
        rtexmapunit_t       rTU[NUM_TEXMAP_UNITS];

        memset(rTU, 0, sizeof(rTU));

        // Allocate enough for the divisions too.
        rvertices = R_AllocRendVertices(params->realNumVertices);
        rtexcoords = R_AllocRendTexCoords(params->realNumVertices);
        rcolors = R_AllocRendColors(params->realNumVertices);

        rTU[TU_PRIMARY].tex = dyn->texture;
        rTU[TU_PRIMARY].magMode = GL_LINEAR;

        rTU[TU_PRIMARY_DETAIL].tex = 0;
        rTU[TU_INTER].tex = 0;
        rTU[TU_INTER_DETAIL].tex = 0;

        for(i = 0; i < params->numVertices; ++i)
        {
            uint                c;
            rcolor_t*           col = &rcolors[i];

            // Each vertex uses the light's color.
            for(c = 0; c < 3; ++c)
                col->rgba[c] = dyn->color[c];
            col->rgba[3] = 1;
        }

        if(params->isWall)
        {
            rtexcoords[1].st[0] = rtexcoords[0].st[0] = dyn->s[0];
            rtexcoords[1].st[1] = rtexcoords[3].st[1] = dyn->t[0];
            rtexcoords[3].st[0] = rtexcoords[2].st[0] = dyn->s[1];
            rtexcoords[2].st[1] = rtexcoords[0].st[1] = dyn->t[1];

            if(params->divs)
            {   // We need to subdivide the dynamic light quad.
                float               bL, tL, bR, tR;
                rvertex_t           origVerts[4];
                rcolor_t            origColors[4];
                rtexcoord_t         origTexCoords[4];

                /**
                 * Need to swap indices around into fans set the position
                 * of the division vertices, interpolate texcoords and
                 * color.
                 */

                memcpy(origVerts, params->rvertices, sizeof(rvertex_t) * 4);
                memcpy(origTexCoords, rtexcoords, sizeof(rtexcoord_t) * 4);
                memcpy(origColors, rcolors, sizeof(rcolor_t) * 4);

                bL = params->rvertices[0].pos[VZ];
                tL = params->rvertices[1].pos[VZ];
                bR = params->rvertices[2].pos[VZ];
                tR = params->rvertices[3].pos[VZ];

                R_DivVerts(rvertices, origVerts, params->divs);
                R_DivTexCoords(rtexcoords, origTexCoords, params->divs, bL, tL, bR, tR);
                R_DivVertColors(rcolors, origColors, params->divs, bL, tL, bR, tR);
            }
            else
            {
                memcpy(rvertices, params->rvertices, sizeof(rvertex_t) * params->numVertices);
            }
        }
        else
        {   // It's a flat.
            uint                i;
            float               width, height;

            width  = params->texBR[VX] - params->texTL[VX];
            height = params->texBR[VY] - params->texTL[VY];

            for(i = 0; i < params->numVertices; ++i)
            {
                rtexcoords[i].st[0] = ((params->texBR[VX] - params->rvertices[i].pos[VX]) / width * dyn->s[0]) +
                    ((params->rvertices[i].pos[VX] - params->texTL[VX]) / width * dyn->s[1]);

                rtexcoords[i].st[1] = ((params->texBR[VY] - params->rvertices[i].pos[VY]) / height * dyn->t[0]) +
                    ((params->rvertices[i].pos[VY] - params->texTL[VY]) / height * dyn->t[1]);
            }

            memcpy(rvertices, params->rvertices, sizeof(rvertex_t) * params->numVertices);
        }

        if(params->isWall && params->divs)
        {
            RL_AddPoly(PT_FAN, RPT_LIGHT, rvertices + 3 + params->divs[0].num,
                       rtexcoords + 3 + params->divs[0].num, NULL, NULL,
                       rcolors + 3 + params->divs[0].num,
                       3 + params->divs[1].num, 0,
                       0, NULL, rTU);
            RL_AddPoly(PT_FAN, RPT_LIGHT, rvertices, rtexcoords, NULL, NULL,
                       rcolors, 3 + params->divs[0].num, 0,
                       0, NULL, rTU);
        }
        else
        {
            RL_AddPoly(params->isWall? PT_TRIANGLE_STRIP : PT_FAN,
                       RPT_LIGHT,
                       rvertices, rtexcoords, NULL, NULL,
                       rcolors, params->numVertices, 0,
                       0, NULL, rTU);
        }

        R_FreeRendVertices(rvertices);
        R_FreeRendTexCoords(rtexcoords);
        R_FreeRendColors(rcolors);
    }
    params->lastIdx++;

    return true; // Continue iteration.
}

static float getSnapshots(material_snapshot_t* msA, material_snapshot_t* msB,
    material_t* mat)
{
    materialvariantspecification_t* spec =
        Materials_VariantSpecificationForContext(MC_MAPSURFACE, 0, 0, 0, 0,
            GL_REPEAT, GL_REPEAT, -1, true, true, false, false);
    float interPos = 0;

    Materials_Prepare(msA, mat, true, spec);

    // Smooth Texture Animation?
    if(msB)
    {
        materialvariant_t* variant = Materials_ChooseVariant(mat, spec);
        assert(variant);
        // If fog is active, inter=0 is accepted as well. Otherwise
        // flickering may occur if the rendering passes don't match for
        // blended and unblended surfaces.
        if(MaterialVariant_TranslationCurrent(variant) != MaterialVariant_TranslationNext(variant) &&
           !(!usingFog && MaterialVariant_TranslationPoint(variant) < 0))
        {
            // Prepare the inter texture.
            Materials_Prepare(msB, MaterialVariant_GeneralCase(MaterialVariant_TranslationNext(variant)),
                false, spec);
            interPos = MaterialVariant_TranslationPoint(variant);
        }
    }

    return interPos;
}

static void setupRTU(rtexmapunit_t main[NUM_TEXMAP_UNITS], rtexmapunit_t reflection[NUM_TEXMAP_UNITS],
    const material_snapshot_t* msA, float inter, const material_snapshot_t* msB)
{
    RTU(main, TU_PRIMARY).tex       = TextureVariant_GLName(MSU(msA, MTU_PRIMARY).tex);
    RTU(main, TU_PRIMARY).magMode   = MSU(msA, MTU_PRIMARY).magMode;
    RTU(main, TU_PRIMARY).scale[0]  = MSU(msA, MTU_PRIMARY).scale[0];
    RTU(main, TU_PRIMARY).scale[1]  = MSU(msA, MTU_PRIMARY).scale[1];
    RTU(main, TU_PRIMARY).offset[0] = MSU(msA, MTU_PRIMARY).offset[0] * RTU(main, TU_PRIMARY).scale[0];
    RTU(main, TU_PRIMARY).offset[1] = MSU(msA, MTU_PRIMARY).offset[1] * RTU(main, TU_PRIMARY).scale[1];
    RTU(main, TU_PRIMARY).blendMode = MSU(msA, MTU_PRIMARY).blendMode;
    RTU(main, TU_PRIMARY).blend     = MSU(msA, MTU_PRIMARY).alpha;

    if(MSU(msA, MTU_DETAIL).tex)
    {
        RTU(main, TU_PRIMARY_DETAIL).tex       = TextureVariant_GLName(MSU(msA, MTU_DETAIL).tex);
        RTU(main, TU_PRIMARY_DETAIL).magMode   = MSU(msA, MTU_DETAIL).magMode;
        RTU(main, TU_PRIMARY_DETAIL).scale[0]  = MSU(msA, MTU_DETAIL).scale[0];
        RTU(main, TU_PRIMARY_DETAIL).scale[1]  = MSU(msA, MTU_DETAIL).scale[1];
        RTU(main, TU_PRIMARY_DETAIL).offset[0] = MSU(msA, MTU_DETAIL).offset[0] * RTU(main, TU_PRIMARY_DETAIL).scale[0];
        RTU(main, TU_PRIMARY_DETAIL).offset[1] = MSU(msA, MTU_DETAIL).offset[1] * RTU(main, TU_PRIMARY_DETAIL).scale[1];
        RTU(main, TU_PRIMARY_DETAIL).blendMode = MSU(msA, MTU_DETAIL).blendMode;
        RTU(main, TU_PRIMARY_DETAIL).blend     = MSU(msA, MTU_DETAIL).alpha;
    }

    if(msB && MSU(msB, MTU_PRIMARY).tex)
    {
        RTU(main, TU_INTER).tex       = TextureVariant_GLName(MSU(msB, MTU_PRIMARY).tex);
        RTU(main, TU_INTER).magMode   = MSU(msB, MTU_PRIMARY).magMode;
        RTU(main, TU_INTER).scale[0]  = MSU(msB, MTU_PRIMARY).scale[0];
        RTU(main, TU_INTER).scale[1]  = MSU(msB, MTU_PRIMARY).scale[1];
        RTU(main, TU_INTER).offset[0] = MSU(msB, MTU_PRIMARY).offset[0] * RTU(main, TU_INTER).scale[0];
        RTU(main, TU_INTER).offset[1] = MSU(msB, MTU_PRIMARY).offset[1] * RTU(main, TU_INTER).scale[1];
        RTU(main, TU_INTER).blendMode = MSU(msB, MTU_PRIMARY).blendMode;
        RTU(main, TU_INTER).blend     = MSU(msB, MTU_PRIMARY).alpha;

        // Blend between the primary and inter textures.
        RTU(main, TU_INTER).blend = inter;
    }

    if(msB && MSU(msB, MTU_DETAIL).tex)
    {
        RTU(main, TU_INTER_DETAIL).tex       = TextureVariant_GLName(MSU(msB, MTU_DETAIL).tex);
        RTU(main, TU_INTER_DETAIL).magMode   = MSU(msB, MTU_DETAIL).magMode;
        RTU(main, TU_INTER_DETAIL).scale[0]  = MSU(msB, MTU_DETAIL).scale[0];
        RTU(main, TU_INTER_DETAIL).scale[1]  = MSU(msB, MTU_DETAIL).scale[1];
        RTU(main, TU_INTER_DETAIL).offset[0] = MSU(msB, MTU_DETAIL).offset[0] * RTU(main, TU_INTER_DETAIL).scale[0];
        RTU(main, TU_INTER_DETAIL).offset[1] = MSU(msB, MTU_DETAIL).offset[1] * RTU(main, TU_INTER_DETAIL).scale[1];
        RTU(main, TU_INTER_DETAIL).blendMode = MSU(msB, MTU_DETAIL).blendMode;
        RTU(main, TU_INTER_DETAIL).blend     = MSU(msB, MTU_DETAIL).alpha;

        // Blend between the primary and inter detail textures.
        RTU(main, TU_INTER_DETAIL).blend     = inter;
    }

    if(MSU(msA, MTU_REFLECTION).tex)
    {
        RTU(reflection, TU_PRIMARY).tex       = TextureVariant_GLName(MSU(msA, MTU_REFLECTION).tex);
        RTU(reflection, TU_PRIMARY).magMode   = MSU(msA, MTU_REFLECTION).magMode;
        RTU(reflection, TU_PRIMARY).scale[0]  = MSU(msA, MTU_REFLECTION).scale[0];
        RTU(reflection, TU_PRIMARY).scale[1]  = MSU(msA, MTU_REFLECTION).scale[1];
        RTU(reflection, TU_PRIMARY).offset[0] = MSU(msA, MTU_REFLECTION).offset[0] * RTU(reflection, TU_PRIMARY).scale[0];
        RTU(reflection, TU_PRIMARY).offset[1] = MSU(msA, MTU_REFLECTION).offset[1] * RTU(reflection, TU_PRIMARY).scale[1];
        RTU(reflection, TU_PRIMARY).blendMode = MSU(msA, MTU_REFLECTION).blendMode;
        RTU(reflection, TU_PRIMARY).blend     = MSU(msA, MTU_REFLECTION).alpha;

        if(MSU(msA, MTU_REFLECTION_MASK).tex)
        {
            RTU(reflection, TU_INTER).tex       = TextureVariant_GLName(MSU(msA, MTU_REFLECTION_MASK).tex);
            RTU(reflection, TU_INTER).magMode   = MSU(msA, MTU_REFLECTION_MASK).magMode;
            RTU(reflection, TU_INTER).scale[0]  = MSU(msA, MTU_REFLECTION_MASK).scale[0];
            RTU(reflection, TU_INTER).scale[1]  = MSU(msA, MTU_REFLECTION_MASK).scale[1];
            RTU(reflection, TU_INTER).offset[0] = MSU(msA, MTU_REFLECTION_MASK).offset[0] * RTU(reflection, TU_INTER).scale[0];
            RTU(reflection, TU_INTER).offset[1] = MSU(msA, MTU_REFLECTION_MASK).offset[1] * RTU(reflection, TU_INTER).scale[1];
            RTU(reflection, TU_INTER).blendMode = MSU(msA, MTU_REFLECTION_MASK).blendMode;
            RTU(reflection, TU_INTER).blend     = MSU(msA, MTU_REFLECTION_MASK).alpha;
        }
    }
}

/**
 * Apply primitive-specific manipulations.
 */
static void setupRTU2(rtexmapunit_t rTU[NUM_TEXMAP_UNITS],
                      rtexmapunit_t rTUs[NUM_TEXMAP_UNITS],
                      boolean isWall, const float texOffset[2],
                      const float texScale[2],
                      const material_snapshot_t* msA,
                      const material_snapshot_t* msB)
{
    if(texScale)
    {
        rTU[TU_PRIMARY].scale[0] *= texScale[0];
        rTU[TU_PRIMARY].scale[1] *= texScale[1];
    }
    if(texOffset)
    {
        rTU[TU_PRIMARY].offset[0] += texOffset[0] / msA->width;
        rTU[TU_PRIMARY].offset[1] +=
            (isWall? texOffset[1] : -texOffset[1]) / msA->height;
    }

    if(msA->units[MTU_DETAIL].tex && texOffset)
    {
        rTU[TU_PRIMARY_DETAIL].offset[0] +=
            texOffset[0] * rTU[TU_PRIMARY_DETAIL].scale[0];
        rTU[TU_PRIMARY_DETAIL].offset[1] +=
            (isWall? texOffset[1] : -texOffset[1]) *
                rTU[TU_PRIMARY_DETAIL].scale[1];
    }

    if(msB && msB->units[MTU_PRIMARY].tex)
    {
        if(texScale)
        {
            rTU[TU_INTER].scale[0] *= texScale[0];
            rTU[TU_INTER].scale[1] *= texScale[1];
        }
        if(texOffset)
        {
            rTU[TU_INTER].offset[0] += texOffset[0] / msB->width;
            rTU[TU_INTER].offset[1] +=
                (isWall? texOffset[1] : -texOffset[1]) / msB->height;
        }
    }

    if(msB && msB->units[MTU_DETAIL].tex && texOffset)
    {
        rTU[TU_INTER_DETAIL].offset[0] +=
            texOffset[0] * rTU[TU_INTER_DETAIL].scale[0];
        rTU[TU_INTER_DETAIL].offset[1] +=
            (isWall? texOffset[1] : -texOffset[1]) *
                rTU[TU_INTER_DETAIL].scale[1];
    }

    if(msA->units[MTU_REFLECTION].tex)
    {
        if(texScale)
        {
            rTUs[TU_INTER].scale[0] *= texScale[0];
            rTUs[TU_INTER].scale[1] *= texScale[1];
        }
        if(texOffset)
        {
            rTUs[TU_INTER].offset[0] += texOffset[0] / msA->width;
            rTUs[TU_INTER].offset[1] += (isWall? texOffset[1] : -texOffset[1]) /
                msA->height;
        }
    }
}

typedef struct {
    boolean         isWall;
    rendpolytype_t  type;
    blendmode_t     blendMode;
    const float*    texTL, *texBR;
    const float*    texOffset, *texScale;
    const float*    normal; // Surface normal.
    float           alpha;
    const float*    sectorLightLevel;
    float           surfaceLightLevelDL;
    float           surfaceLightLevelDR;
    const float*    sectorLightColor;
    const float*    surfaceColor;

    uint            lightListIdx; // List of lights that affect this poly.
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
                               const material_snapshot_t* msA, float inter,
                               const material_snapshot_t* msB)
{
    rcolor_t*           rcolors;
    rtexcoord_t*        rtexcoords = NULL, *rtexcoords2 = NULL,
                       *rtexcoords5 = NULL;
    uint                realNumVertices =
        p->isWall && divs? 3 + divs[0].num + 3 + divs[1].num : numVertices;
    rcolor_t*           shinyColors = NULL;
    rtexcoord_t*        shinyTexCoords = NULL;
    boolean             useLights = false;
    uint                numLights = 0;
    DGLuint             modTex = 0;
    float               modTexTC[2][2];
    float               modColor[3];
    float               glowing = p->glowing;
    boolean             drawAsVisSprite = false;
    rtexmapunit_t       rTU[NUM_TEXMAP_UNITS], rTUs[NUM_TEXMAP_UNITS];

    if(!p->forceOpaque && p->type != RPT_SKY_MASK &&
       (!msA->isOpaque || p->alpha < 1 || p->blendMode > 0))
        drawAsVisSprite = true;

    memset(rTU, 0, sizeof(rtexmapunit_t) * NUM_TEXMAP_UNITS);
    memset(rTUs, 0, sizeof(rtexmapunit_t) * NUM_TEXMAP_UNITS);

    if(p->type != RPT_SKY_MASK)
    {
        setupRTU(rTU, rTUs, msA, inter, msB);
        setupRTU2(rTU, rTUs, p->isWall, p->texOffset, p->texScale, msA, msB);
    }

    memset(modTexTC, 0, sizeof(modTexTC));
    memset(modColor, 0, sizeof(modColor));

    rcolors = R_AllocRendColors(realNumVertices);
    rtexcoords = R_AllocRendTexCoords(realNumVertices);
    if(rTU[TU_INTER].tex)
        rtexcoords2 = R_AllocRendTexCoords(realNumVertices);

    if(p->type != RPT_SKY_MASK)
    {
        // ShinySurface?
        if(useShinySurfaces && rTUs[TU_PRIMARY].tex && !drawAsVisSprite)
        {
            // We'll reuse the same verts but we need new colors.
            shinyColors = R_AllocRendColors(realNumVertices);
            // The normal texcoords are used with the mask.
            // New texcoords are required for shiny texture.
            shinyTexCoords = R_AllocRendTexCoords(realNumVertices);
        }

        /**
         * Dynamic lights?
         * In multiplicative mode, glowing surfaces are fullbright.
         * Rendering lights on them would be pointless.
         */
        if(glowing < 1)
        {
            useLights = (p->lightListIdx? true : false);

            /**
             * If multitexturing is enabled and there is at least one
             * dynlight affecting this surface, grab the paramaters needed
             * to draw it.
             */
            if(useLights && RL_IsMTexLights())
            {
                dynlight_t*         dyn = NULL;

                DL_ListIterator(p->lightListIdx, &dyn, RLIT_DynGetFirst);

                rtexcoords5 = R_AllocRendTexCoords(realNumVertices);

                modTex = dyn->texture;
                modColor[CR] = dyn->color[CR];
                modColor[CG] = dyn->color[CG];
                modColor[CB] = dyn->color[CB];
                modTexTC[0][0] = dyn->s[0];
                modTexTC[0][1] = dyn->s[1];
                modTexTC[1][0] = dyn->t[0];
                modTexTC[1][1] = dyn->t[1];

                numLights = 1;
            }
        }
    }

    if(p->isWall)
    {
        // Primary texture coordinates.
        quadTexCoords(rtexcoords, rvertices, *p->segLength, p->texTL);

        // Blend texture coordinates.
        if(rTU[TU_INTER].tex && !drawAsVisSprite)
            quadTexCoords(rtexcoords2, rvertices, *p->segLength, p->texTL);

        // Shiny texture coordinates.
        if(useShinySurfaces && rTUs[TU_PRIMARY].tex && !drawAsVisSprite)
            quadShinyTexCoords(shinyTexCoords, &rvertices[1], &rvertices[2], *p->segLength);

        // First light texture coordinates.
        if(numLights > 0 && RL_IsMTexLights())
            quadLightCoords(rtexcoords5, modTexTC[0], modTexTC[1]);
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
            if(rTU[TU_PRIMARY].tex)
            {
                rtexcoord_t* tc = &rtexcoords[i];

                tc->st[0] =  xyz[VX];
                tc->st[1] = -xyz[VY];
            }

            // Blend primary texture coordinates.
            if(rTU[TU_INTER].tex)
            {
                rtexcoord_t*        tc = &rtexcoords2[i];

                tc->st[0] =  xyz[VX];
                tc->st[1] = -xyz[VY];
            }

            // Shiny texture coordinates.
            if(useShinySurfaces && rTUs[TU_PRIMARY].tex)
            {
                flatShinyTexCoords(&shinyTexCoords[i], vtx->pos);
            }

            // First light texture coordinates.
            if(numLights > 0 && RL_IsMTexLights())
            {
                rtexcoord_t* tc = &rtexcoords5[i];
                float width, height;

                width  = p->texBR[VX] - p->texTL[VX];
                height = p->texBR[VY] - p->texTL[VY];

                tc->st[0] = ((p->texBR[VX] - vtx->pos[VX]) / width  * modTexTC[0][0]) + (xyz[VX] / width  * modTexTC[0][1]);
                tc->st[1] = ((p->texBR[VY] - vtx->pos[VY]) / height * modTexTC[1][0]) + (xyz[VY] / height * modTexTC[1][1]);
            }
        }
    }

    // Light this polygon.
    if(p->type != RPT_SKY_MASK)
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

        if(useShinySurfaces && rTUs[TU_PRIMARY].tex && !drawAsVisSprite)
        {
            uint i;

            // Strength of the shine.
            for(i = 0; i < numVertices; ++i)
            {
                shinyColors[i].rgba[CR] = MAX_OF(rcolors[i].rgba[CR], msA->shiny.minColor[CR]);
                shinyColors[i].rgba[CG] = MAX_OF(rcolors[i].rgba[CG], msA->shiny.minColor[CG]);
                shinyColors[i].rgba[CB] = MAX_OF(rcolors[i].rgba[CB], msA->shiny.minColor[CB]);
                shinyColors[i].rgba[CA] = rTUs[TU_PRIMARY].blend;
            }
        }

        // Apply uniform alpha.
        Rend_VertexColorsAlpha(rcolors, numVertices, p->alpha);
    }

    if(IS_MUL && useLights)
    {
        // Surfaces lit by dynamic lights may need to be rendered
        // differently than non-lit surfaces.
        float avglightlevel = 0;

        // Determine the average light level of this rend poly,
        // if too bright; do not bother with lights.
        {uint i;
        for(i = 0; i < numVertices; ++i)
        {
            avglightlevel += rcolors[i].rgba[CR];
            avglightlevel += rcolors[i].rgba[CG];
            avglightlevel += rcolors[i].rgba[CB];
        }}
        avglightlevel /= (float) numVertices * 3;

        if(avglightlevel > 0.98f)
        {
            useLights = false;
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
                           msA->width, msA->height, p->texOffset,
                           p->blendMode, p->lightListIdx, glowing,
                           !msA->isOpaque, rTU);
        R_FreeRendTexCoords(rtexcoords);
        if(rtexcoords2)
            R_FreeRendTexCoords(rtexcoords2);
        if(rtexcoords5)
            R_FreeRendTexCoords(rtexcoords5);
        if(shinyTexCoords)
            R_FreeRendTexCoords(shinyTexCoords);
        R_FreeRendColors(rcolors);
        if(shinyColors)
            R_FreeRendColors(shinyColors);

        return false; // We HAD to use a vissprite, so it MUST not be opaque.
    }

    if(p->type != RPT_SKY_MASK && useLights)
    {
        /**
         * Generate a dynlight primitive for each of the lights affecting
         * the surface. Multitexturing may be used for the first light, so
         * it's skipped.
         */
        dynlightiterparams_t dlparams;

        dlparams.rvertices = rvertices;
        dlparams.numVertices = numVertices;
        dlparams.realNumVertices = realNumVertices;
        dlparams.lastIdx = 0;
        dlparams.isWall = p->isWall;
        dlparams.divs = divs;
        dlparams.texTL = p->texTL;
        dlparams.texBR = p->texBR;

        DL_ListIterator(p->lightListIdx, &dlparams, RLIT_DynLightWrite);
        numLights += dlparams.lastIdx;
        if(RL_IsMTexLights())
            numLights -= 1;
    }

    // Write multiple polys depending on rend params.
    if(p->isWall && divs)
    {
        float               bL, tL, bR, tR;
        rvertex_t           origVerts[4];
        rcolor_t            origColors[4];
        rtexcoord_t         origTexCoords[4];

        /**
         * Need to swap indices around into fans set the position
         * of the division vertices, interpolate texcoords and
         * color.
         */

        memcpy(origVerts, rvertices, sizeof(rvertex_t) * 4);
        memcpy(origTexCoords, rtexcoords, sizeof(rtexcoord_t) * 4);
        memcpy(origColors, rcolors, sizeof(rcolor_t) * 4);

        bL = origVerts[0].pos[VZ];
        tL = origVerts[1].pos[VZ];
        bR = origVerts[2].pos[VZ];
        tR = origVerts[3].pos[VZ];

        R_DivVerts(rvertices, origVerts, divs);
        R_DivTexCoords(rtexcoords, origTexCoords, divs, bL, tL, bR, tR);
        R_DivVertColors(rcolors, origColors, divs, bL, tL, bR, tR);

        if(rtexcoords2)
        {
            rtexcoord_t         origTexCoords2[4];

            memcpy(origTexCoords2, rtexcoords2, sizeof(rtexcoord_t) * 4);
            R_DivTexCoords(rtexcoords2, origTexCoords2, divs, bL, tL, bR, tR);
        }

        if(rtexcoords5)
        {
            rtexcoord_t         origTexCoords5[4];

            memcpy(origTexCoords5, rtexcoords5, sizeof(rtexcoord_t) * 4);
            R_DivTexCoords(rtexcoords5, origTexCoords5, divs, bL, tL, bR, tR);
        }

        if(shinyTexCoords)
        {
            rtexcoord_t         origShinyTexCoords[4];

            memcpy(origShinyTexCoords, shinyTexCoords, sizeof(rtexcoord_t) * 4);
            R_DivTexCoords(shinyTexCoords, origShinyTexCoords, divs, bL, tL, bR, tR);
        }

        if(shinyColors)
        {
            rcolor_t            origShinyColors[4];
            memcpy(origShinyColors, shinyColors, sizeof(rcolor_t) * 4);
            R_DivVertColors(shinyColors, origShinyColors, divs, bL, tL, bR, tR);
        }

        RL_AddPoly(PT_FAN, p->type, rvertices + 3 + divs[0].num,
                   rtexcoords + 3 + divs[0].num,
                   rtexcoords2? rtexcoords2 + 3 + divs[0].num : NULL,
                   rtexcoords5? rtexcoords5 + 3 + divs[0].num : NULL,
                   rcolors + 3 + divs[0].num, 3 + divs[1].num,
                   numLights, modTex, modColor, rTU);
        RL_AddPoly(PT_FAN, p->type, rvertices, rtexcoords, rtexcoords2,

                   rtexcoords5, rcolors, 3 + divs[0].num,
                   numLights, modTex, modColor, rTU);
        if(useShinySurfaces && rTUs[TU_PRIMARY].tex)
        {
            RL_AddPoly(PT_FAN, RPT_SHINY, rvertices + 3 + divs[0].num,
                       shinyTexCoords? shinyTexCoords + 3 + divs[0].num : NULL,
                       rTUs[TU_INTER].tex? rtexcoords + 3 + divs[0].num : NULL,
                       NULL,
                       shinyColors + 3 + divs[0].num,
                       3 + divs[1].num, 0, 0, NULL, rTUs);
            RL_AddPoly(PT_FAN, RPT_SHINY, rvertices,
                       shinyTexCoords, rTUs[TU_INTER].tex? rtexcoords : NULL,
                       NULL, shinyColors, 3 + divs[0].num, 0, 0, NULL, rTUs);
        }
    }
    else
    {
        RL_AddPoly(p->isWall? PT_TRIANGLE_STRIP : PT_FAN, p->type, rvertices,
                   rtexcoords, rtexcoords2, rtexcoords5, rcolors,

                   numVertices, numLights, modTex, modColor, rTU);
        if(useShinySurfaces && rTUs[TU_PRIMARY].tex)
            RL_AddPoly(p->isWall? PT_TRIANGLE_STRIP : PT_FAN, RPT_SHINY,
                       rvertices, shinyTexCoords,
                       rTUs[TU_INTER].tex? rtexcoords : NULL,
                       NULL, shinyColors, numVertices, 0, 0, NULL, rTUs);
    }

    R_FreeRendTexCoords(rtexcoords);
    if(rtexcoords2)
        R_FreeRendTexCoords(rtexcoords2);
    if(rtexcoords5)
        R_FreeRendTexCoords(rtexcoords5);
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
                           const walldiv_t* divs,
                           boolean skyMask,
                           boolean addFakeRadio,
                           float glowing,
                           const float texTL[3], const float texBR[3],
                           const float texOffset[2],
                           const float texScale[2],
                           blendmode_t blendMode,
                           const float* color, const float* color2,
                           biassurface_t* bsuf, uint elmIdx /*tmp*/,
                           const material_snapshot_t* msA, float inter,
                           const material_snapshot_t* msB,
                           boolean isTwosidedMiddle)
{
    rendworldpoly_params_t params;
    sidedef_t*          side = (seg->lineDef? SEG_SIDEDEF(seg) : NULL);
    rvertex_t*          rvertices;

    // Init the params.
    memset(&params, 0, sizeof(params));

    params.type = (skyMask? RPT_SKY_MASK : RPT_NORMAL);
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
    params.glowing = glowing;
    params.blendMode = blendMode;
    params.texOffset = texOffset;
    params.texScale = texScale;
    params.lightListIdx = lightListIdx;

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
        if(params.type != RPT_SKY_MASK && addFakeRadio)
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

static void renderPlane(subsector_t* ssec, planetype_t type,
                        float height, const vectorcomp_t normal[3],
                        material_t* inMat, int sufFlags, short sufInFlags,
                        const float sufColor[4], blendmode_t blendMode,
                        const float texTL[3], const float texBR[3],
                        const float texOffset[2], const float texScale[2],
                        boolean skyMasked,
                        boolean addDLights,
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
    material_snapshot_t msA, msB;

    memset(&msA, 0, sizeof(msA));
    memset(&msB, 0, sizeof(msB));

    memset(&params, 0, sizeof(params));

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
            params.type = RPT_NORMAL;
            params.blendMode = BM_NORMAL;
            params.glowing = 1;
            params.forceOpaque = true;
            mat = inMat;
        }
        else
        {   // We'll mask this.
            params.type = RPT_SKY_MASK;
        }
    }
    else
    {
        params.type = RPT_NORMAL;
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

    if(params.type != RPT_SKY_MASK)
    {
        // Smooth Texture Animation?
        if(smoothTexAnim)
            blended = true;

        inter = getSnapshots(&msA, blended? &msB : NULL, mat);

        if(texMode != 2)
        {
            params.glowing = msA.glowing;
        }
        else
        {
            material_snapshot_t ms;
            surface_t* suf = &ssec->sector->planes[elmIdx]->surface;
            Materials_Prepare(&ms, suf->material, true,
                Materials_VariantSpecificationForContext(MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, true, true, false, false));
            params.glowing = ms.glowing;
        }

        // Dynamic lights.
        if(addDLights && params.glowing < 1)
        {
            params.lightListIdx = DL_ProjectOnSurface(ssec, params.texTL, params.texBR, normal,
                (DLF_NO_PLANAR | (type == PLN_FLOOR? DLF_TEX_FLOOR : DLF_TEX_CEILING)));
        }
    }

    renderWorldPoly(rvertices, numVertices, NULL, &params, &msA, inter, blended? &msB : NULL);

    R_FreeRendVertices(rvertices);
}

static void Rend_RenderPlane(subsector_t* ssec, planetype_t type,
                             float height, const vectorcomp_t _normal[3],
                             material_t* inMat, int sufFlags, short sufInFlags,
                             const float sufColor[4], blendmode_t blendMode,
                             const float texOffset[2], const float texScale[2],
                             boolean skyMasked,
                             boolean addDLights,
                             biassurface_t* bsuf, uint elmIdx /*tmp*/,
                             int texMode /*tmp*/, boolean flipNormal, boolean clipBackFacing)
{
    vec3_t vec, normal;

    // Must have a visible surface.
    if(!inMat || !Material_IsDrawable(inMat))
        return;

    V3_Set(vec, vx - ssec->midPoint.pos[VX], vz - ssec->midPoint.pos[VY], vy - height);

        /**
         * Flip the plane normal according to the z positon of the
         * viewer relative to the plane height so that the plane is
         * always visible.
         */
    V3_Copy(normal, _normal);
    if(flipNormal)
        normal[VZ] *= -1;

    // Don't bother with planes facing away from the camera.
    if(!(clipBackFacing && !(V3_DotProduct(vec, normal) < 0)))
    {
        float texTL[3], texBR[3];

        // Set the texture origin, Y is flipped for the ceiling.
        V3_Set(texTL, ssec->bBox[0].pos[VX], ssec->bBox[type == PLN_FLOOR? 1 : 0].pos[VY], height);
        V3_Set(texBR, ssec->bBox[1].pos[VX], ssec->bBox[type == PLN_FLOOR? 0 : 1].pos[VY], height);

        renderPlane(ssec, type, height, normal, inMat, sufFlags, sufInFlags,
                    sufColor, blendMode, texTL, texBR, texOffset, texScale,
                    skyMasked, addDLights, bsuf, elmIdx, texMode, flipNormal);
    }
}

static boolean isVisible(surface_t* surface, sector_t* frontsec,
                         boolean canMask, boolean* skyMask)
{
    if(!(surface->material && !Material_IsDrawable(surface->material)))
    {
        *skyMask = false;
        return true;
    }
    else if(canMask)
    {   // Perhaps add this section to the sky mask?
        if(R_IsSkySurface(&frontsec->SP_ceilsurface) &&
           R_IsSkySurface(&frontsec->SP_floorsurface))
        {
           *skyMask = true;
           return true;
        }
    }

    return false;
}

static boolean rendSegSection(subsector_t* ssec, seg_t* seg,
                              segsection_t section, surface_t* surface,
                              const fvertex_t* from, const fvertex_t* to,
                              float bottom, float top,
                              const float texOffset[2],
                              sector_t* frontsec, boolean softSurface,
                              boolean addDLights, short sideFlags)
{
    float               alpha;
    boolean             skyMask;
    boolean             solidSeg = true;

    if(!isVisible(surface, frontsec, false, &skyMask))
        return false;

    if(bottom >= top)
        return true;

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
        int                 texMode = 0;
        uint                lightListIdx = 0;
        float               texTL[3], texBR[3], texScale[2],
                            inter = 0, glowing = 0;
        walldiv_t           divs[2];
        boolean             forceOpaque = false;
        material_t*         mat = NULL;
        rendpolytype_t      type = RPT_NORMAL;
        boolean             isTwoSided = (seg->lineDef &&
            seg->lineDef->L_frontside && seg->lineDef->L_backside)? true:false;
        blendmode_t         blendMode = BM_NORMAL;
        boolean             addFakeRadio = false, blended = false;
        const float*        color = NULL, *color2 = NULL;
        material_snapshot_t msA, msB;

        memset(&msA, 0, sizeof(msA));
        memset(&msB, 0, sizeof(msB));

        texScale[0] = ((surface->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
        texScale[1] = ((surface->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        V3_Set(texTL, from->pos[VX], from->pos[VY], top);
        V3_Set(texBR, to->pos  [VX], to->pos  [VY], bottom);

        // Fill in the remaining params data.
        if(skyMask || R_IsSkySurface(surface))
        {
            // In devRendSkyMode mode we render all polys destined for the skymask
            // as regular world polys (with a few obvious properties).
            if(devRendSkyMode)
            {
                mat = surface->material;
                // Lets make it stand out.
                forceOpaque = true;
                glowing = 1.0;
            }
            else
            {   // We'll mask this.
                type = RPT_SKY_MASK;
            }
        }
        else
        {
            int surfaceFlags, surfaceInFlags;

            // Determine which texture to use.
            if(renderTextures == 2)
                texMode = 2;
            else if(!surface->material ||
                    ((surface->inFlags & SUIF_FIX_MISSING_MATERIAL) && devNoTexFix))
                texMode = 1;
            else
                texMode = 0;

            if(texMode == 0)
                mat = surface->material;
            else if(texMode == 1)
                // For debug, render the "missing" texture instead of the texture
                // chosen for surfaces to fix the HOMs.
                mat = Materials_ToMaterial(Materials_IndexForName(MN_SYSTEM_NAME":missing"));
            else // texMode == 2
                // For lighting debug, render all solid surfaces using the gray
                // texture.
                mat = Materials_ToMaterial(Materials_IndexForName(MN_SYSTEM_NAME":gray"));

            // Make any necessary adjustments to the surface flags to suit the
            // current texture mode.
            surfaceFlags = surface->flags;
            surfaceInFlags = surface->inFlags;
            if(texMode == 2)
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

        if(type != RPT_SKY_MASK)
        {
            // Smooth Texture Animation?
            if(smoothTexAnim)
                blended = true;

            inter = getSnapshots(&msA, blended? &msB : NULL, mat);

            if(texMode != 2)
            {
                glowing = msA.glowing;
            }
            else
            {
                material_snapshot_t ms;
                Materials_Prepare(&ms, surface->material, true,
                    Materials_VariantSpecificationForContext(MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, true, true, false, false));
                glowing = ms.glowing;
            }

            if(addDLights && msA.glowing < 1)
                lightListIdx = DL_ProjectOnSurface(ssec, texTL, texBR, SEG_SIDEDEF(seg)->SW_middlenormal, ((section == SEG_MIDDLE && isTwoSided)? DLF_SORT_LUMADSC : 0));

            addFakeRadio = ((addFakeRadio && glowing == 0)? true : false);

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

        Linedef_LightLevelDelta(seg->lineDef, seg->side, &deltaL, &deltaR);

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
                               lightListIdx,
                               (divs[0].num > 0 || divs[1].num > 0)? divs : NULL,
                               skyMask,
                               addFakeRadio,
                               glowing,
                               texTL, texBR, texOffset, texScale, blendMode,
                               color, color2,
                               seg->bsuf[section], (uint) section,
                               &msA, inter, blended? &msB : NULL,
                               (section == SEG_MIDDLE && isTwoSided));
        }
    }

    return solidSeg;
}

/**
 * Renders the given single-sided seg into the world.
 */
static boolean Rend_RenderSSWallSeg(subsector_t* ssec, seg_t* seg)
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
                           false, true, side->flags);
    }

    if(P_IsInVoid(viewPlayer))
        solidSeg = false;

    return solidSeg;
}

boolean R_FindBottomTop(segsection_t section, float segOffset,
                             const surface_t* suf,
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
            texOffset[VX] = suf->visOffset[VX] + segOffset;
            texOffset[VY] = suf->visOffset[VY];

            // Align with normal middle texture?
            if(!unpegTop)
                texOffset[VY] += -(fceil->visHeight - bceil->visHeight);

            return true;
        }
        break;

    case SEG_BOTTOM:
        {
        float               t = bfloor->visHeight;

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
            texOffset[VX] = suf->visOffset[VX] + segOffset;
            texOffset[VY] = suf->visOffset[VY];

            if(bfloor->visHeight > fceil->visHeight)
                texOffset[VY] += -(fceil->visHeight - bfloor->visHeight);

            // Align with normal middle texture?
            if(unpegBottom)
                texOffset[VY] += fceil->visHeight - bfloor->visHeight;

            return true;
        }
        break;
        }
    case SEG_MIDDLE:
        {
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

        //fbottom += suf->visOffset[VY];
        //ftop    += suf->visOffset[VY];

        *bottom = vR_ZBottom = fbottom;
        *top    = vR_ZTop    = ftop;

        if(stretchMiddle)
        {
            if(*top > *bottom)
            {
                texOffset[VX] = suf->visOffset[VX] + segOffset;
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

            if(Rend_MidMaterialPos(bottom, &vR_ZBottom, top, &vR_ZTop, &texOffset[VY],
               suf->visOffset[VY], Material_Height(suf->material),
                    unpegBottom, clipTop, clipBottom))
            {
                texOffset[VX] = suf->visOffset[VX] + segOffset;
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
 * Renders wall sections for given two-sided seg.
 */
static boolean Rend_RenderWallSeg(subsector_t* ssec, seg_t* seg)
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

        if(R_FindBottomTop(SEG_MIDDLE, seg->offset, suf,
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
                                      true, frontSide->flags);
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

        if(R_FindBottomTop(SEG_TOP, seg->offset, suf,
                         ffloor, fceil, bfloor, bceil,
                         (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
                         (line->flags & DDLF_DONTPEGTOP)? true : false,
                         (frontSide->flags & SDF_MIDDLE_STRETCH)? true : false,
                         LINE_SELFREF(line)? true : false,
                         &bottom, &top, texOffset))
        {
            rendSegSection(ssec, seg, SEG_TOP, suf,
                           &seg->SG_v1->v, &seg->SG_v2->v, bottom, top, texOffset,
                           frontSec, false, true, frontSide->flags);
        }
    }

    // Lower section.
    if(frontSide->SW_bottominflags & SUIF_PVIS)
    {
        surface_t*          suf = &frontSide->SW_bottomsurface;

        if(R_FindBottomTop(SEG_BOTTOM, seg->offset, suf,
                         ffloor, fceil, bfloor, bceil,
                         (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
                         (line->flags & DDLF_DONTPEGTOP)? true : false,
                         (frontSide->flags & SDF_MIDDLE_STRETCH)? true : false,
                         LINE_SELFREF(line)? true : false,
                         &bottom, &top, texOffset))
        {
            rendSegSection(ssec, seg, SEG_BOTTOM, suf,
                           &seg->SG_v1->v, &seg->SG_v2->v, bottom, top, texOffset,
                           frontSec, false, true, frontSide->flags);
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
        else if((seg->frameFlags & SEGINF_BACKSECSKYFIX) ||
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

float Rend_SectorLight(sector_t *sec)
{
    if(levelFullBright)
        return 1.0f;

    return sec->lightLevel;
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
            seg->frameFlags &= ~SEGINF_BACKSECSKYFIX;

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

            seg->frameFlags &= ~SEGINF_BACKSECSKYFIX;

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

static boolean skymaskSegIsVisible(seg_t* seg, boolean clipBackFacing)
{
    linedef_t* lineDef;
    sidedef_t* sideDef;

    if(!seg->lineDef) // "minisegs" have no linedefs.
        return false;
    lineDef = seg->lineDef;

    // Let's first check which way this seg is facing.
    if(clipBackFacing && !(seg->frameFlags & SEGINF_FACINGFRONT))
        return false;

    sideDef = SEG_SIDEDEF(seg);
    if(!sideDef)
        return false;

    { sector_t* backsec = seg->SG_backsector;
    sector_t* frontsec = seg->SG_frontsector;
    if(backsec == frontsec && !sideDef->SW_topmaterial && !sideDef->SW_bottommaterial && !sideDef->SW_middlematerial)
       return false; /* Ugh... an obvious wall seg hack. Best take no chances... */ }
    return true;
}

void lightGeometry(rendpolytype_t type, size_t count, rvertex_t* rvertices, rcolor_t* rcolors,
    rcolor_t* rcolorsShiny, rtexmapunit_t* rTU, rtexmapunit_t* rTUs, material_snapshot_t* msA,
    const float* surfaceNormal, const float* surfaceColor, float surfaceAlpha, const float* surfaceColor2,
    const float* ambientLightColor, float ambientLightLevel, float lightLevelDeltaLeft, float lightLevelDeltaRight, float lightLevelDeltaBottom, float lightLevelDeltaTop,
    biassurface_t* bsuf, void* mapObject, uint elmIdx, float glowing, boolean isWall, boolean drawAsVisSprite)
{
    assert(rvertices && rcolors && ambientLightColor);
    {
    // Light this polygon.
    if(rcolors && type != RPT_SKY_MASK)
    {
        if(levelFullBright || !(glowing < 1))
        {   // Uniform colour. Apply to all vertices.
            Rend_VertexColorsGlow(rcolors, count, ambientLightLevel + (levelFullBright? 1 : glowing));
        }
        else
        {   // Non-uniform color.
            if(useBias && bsuf)
            {   // Do BIAS lighting for this poly.
                SB_RendPoly(rcolors, bsuf, rvertices, count, surfaceNormal, ambientLightLevel, mapObject, elmIdx, isWall);
                if(glowing > 0)
                {
                    size_t i;
                    for(i = 0; i < count; ++i)
                    {
                        rcolors[i].rgba[CR] = MINMAX_OF(0, rcolors[i].rgba[CR] + glowing, 1);
                        rcolors[i].rgba[CG] = MINMAX_OF(0, rcolors[i].rgba[CG] + glowing, 1);
                        rcolors[i].rgba[CB] = MINMAX_OF(0, rcolors[i].rgba[CB] + glowing, 1);
                    }
                }
            }
            else
            {
                float llB = MINMAX_OF(0, ambientLightLevel + lightLevelDeltaBottom + glowing, 1);
                float llT = MINMAX_OF(0, ambientLightLevel + lightLevelDeltaTop    + glowing, 1);
                float llL = MINMAX_OF(0, ambientLightLevel + lightLevelDeltaLeft   + glowing, 1);
                float llR = MINMAX_OF(0, ambientLightLevel + lightLevelDeltaRight  + glowing, 1);

                // Calculate the color for each vertex, blended with plane color?
                if(surfaceColor && (surfaceColor[0] < 1 || surfaceColor[1] < 1 || surfaceColor[2] < 1))
                {
                    float vColor[4];

                    // Blend sector light+color+surfacecolor
                    vColor[CR] = surfaceColor[CR] * ambientLightColor[CR];
                    vColor[CG] = surfaceColor[CG] * ambientLightColor[CG];
                    vColor[CB] = surfaceColor[CB] * ambientLightColor[CB];
                    vColor[CA] = 1;

                    if(!(isWall && (llL != llR || llB != llT)))
                    {
                        lightVertices(count, rcolors, rvertices, llL, vColor);
                    }
                    else
                    {
                        float lightLevels[4];
                        lightLevels[0] = MINMAX_OF(0, ambientLightLevel + lightLevelDeltaBottom + lightLevelDeltaLeft  + glowing, 1);
                        lightLevels[1] = MINMAX_OF(0, ambientLightLevel + lightLevelDeltaTop    + lightLevelDeltaLeft  + glowing, 1);
                        lightLevels[2] = MINMAX_OF(0, ambientLightLevel + lightLevelDeltaBottom + lightLevelDeltaRight + glowing, 1);
                        lightLevels[3] = MINMAX_OF(0, ambientLightLevel + lightLevelDeltaTop    + lightLevelDeltaRight + glowing, 1);
                        lightVertex(&rcolors[0], &rvertices[0], lightLevels[0], vColor);
                        lightVertex(&rcolors[1], &rvertices[1], lightLevels[1], vColor);
                        lightVertex(&rcolors[2], &rvertices[2], lightLevels[2], vColor);
                        lightVertex(&rcolors[3], &rvertices[3], lightLevels[3], vColor);
                    }
                }
                else
                {   // Use sector light+color only.
                    if(!(isWall && (llL != llR || llB != llT)))
                    {
                        lightVertices(count, rcolors, rvertices, llL, ambientLightColor);
                    }
                    else
                    {
                        float lightLevels[4];
                        lightLevels[0] = MINMAX_OF(0, ambientLightLevel + lightLevelDeltaBottom + lightLevelDeltaLeft  + glowing, 1);
                        lightLevels[1] = MINMAX_OF(0, ambientLightLevel + lightLevelDeltaTop    + lightLevelDeltaLeft  + glowing, 1);
                        lightLevels[2] = MINMAX_OF(0, ambientLightLevel + lightLevelDeltaBottom + lightLevelDeltaRight + glowing, 1);
                        lightLevels[3] = MINMAX_OF(0, ambientLightLevel + lightLevelDeltaTop    + lightLevelDeltaRight + glowing, 1);
                        lightVertex(&rcolors[0], &rvertices[0], lightLevels[0], ambientLightColor);
                        lightVertex(&rcolors[1], &rvertices[1], lightLevels[1], ambientLightColor);
                        lightVertex(&rcolors[2], &rvertices[2], lightLevels[2], ambientLightColor);
                        lightVertex(&rcolors[3], &rvertices[3], lightLevels[3], ambientLightColor);
                    }
                }

                // Bottom color (if different from top)?
                if(isWall && surfaceColor2)
                {
                    float vColor[4];

                    // Blend sector light+color+surfacecolor
                    vColor[CR] = surfaceColor2[CR] * ambientLightColor[CR];
                    vColor[CG] = surfaceColor2[CG] * ambientLightColor[CG];
                    vColor[CB] = surfaceColor2[CB] * ambientLightColor[CB];
                    vColor[CA] = 1;

                    lightVertex(&rcolors[0], &rvertices[0], llL * llB, vColor);
                    lightVertex(&rcolors[2], &rvertices[2], llR * llB, vColor);
                }
            }

            Rend_VertexColorsApplyTorchLight(rcolors, rvertices, count);
        }

        if(useShinySurfaces && rcolorsShiny && rTUs && rTUs[TU_PRIMARY].tex && !drawAsVisSprite)
        {
            // Strength of the shine.
            size_t i;
            for(i = 0; i < count; ++i)
            {
                rcolorsShiny[i].rgba[CR] = MAX_OF(rcolors[i].rgba[CR], msA->shiny.minColor[CR]);
                rcolorsShiny[i].rgba[CG] = MAX_OF(rcolors[i].rgba[CG], msA->shiny.minColor[CG]);
                rcolorsShiny[i].rgba[CB] = MAX_OF(rcolors[i].rgba[CB], msA->shiny.minColor[CB]);
                rcolorsShiny[i].rgba[CA] = rTUs[TU_PRIMARY].blend;
            }
        }

        // Apply uniform alpha.
        Rend_VertexColorsAlpha(rcolors, count, surfaceAlpha);
    }
    }
}

static void prepareSkyMaskSurface(rendpolytype_t polyType, size_t count, rvertex_t* rvertices,
    rtexcoord_t* rtexcoords, rcolor_t* rcolors, rcolor_t* rcolorsShiny, rtexmapunit_t* rTU, rtexmapunit_t* rTUs,
    const float* surfaceNormal, const float* surfaceColor, float surfaceAlpha, const float* surfaceColor2,
    const float* ambientLightColor, float lightLevel, float lightLevelDeltaLeft, float lightLevelDeltaRight,
    float lightLevelDeltaBottom, float lightLevelDeltaTop,
    biassurface_t* bsuf, void* mapObject, uint elmIdx, float glowing, float wallLength, boolean isWall,
    material_t* mat, boolean drawAsVisSprite)
{
    material_snapshot_t ms;

    // In devRendSkyMode mode we render all polys destined for the skymask as
    // regular world polys (with a few obvious properties).

    Materials_Prepare(&ms, mat, true,
        Materials_VariantSpecificationForContext(MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, true, true, false, false));

    rTU[TU_PRIMARY].tex = TextureVariant_GLName(ms.units[MTU_PRIMARY].tex);
    rTU[TU_PRIMARY].magMode = ms.units[MTU_PRIMARY].magMode;
    rTU[TU_PRIMARY].scale[0] = ms.units[MTU_PRIMARY].scale[0];
    rTU[TU_PRIMARY].scale[1] = ms.units[MTU_PRIMARY].scale[1];
    rTU[TU_PRIMARY].offset[0] = ms.units[MTU_PRIMARY].offset[0] * rTU[TU_PRIMARY].scale[0];
    rTU[TU_PRIMARY].offset[1] = ms.units[MTU_PRIMARY].offset[1] * rTU[TU_PRIMARY].scale[1];
    rTU[TU_PRIMARY].blend = ms.units[MTU_PRIMARY].alpha;
    rTU[TU_PRIMARY].blendMode = ms.units[MTU_PRIMARY].blendMode;

    if(rtexcoords)
    {
        vec3_t texOrigin[2];
        // Top left.
        V3_Set(texOrigin[0], rvertices[1].pos[VX], rvertices[1].pos[VY], rvertices[1].pos[VZ]);
        // Bottom right.
        V3_Set(texOrigin[1], rvertices[2].pos[VX], rvertices[2].pos[VY], rvertices[2].pos[VZ]);
        quadTexCoords(rtexcoords, rvertices, wallLength, texOrigin[0]);
    }

    if(rcolors)
    {
        lightGeometry(polyType, count, rvertices, rcolors, rcolorsShiny, rTU, rTUs, &ms,
                      surfaceNormal, surfaceColor, surfaceAlpha, surfaceColor2, ambientLightColor,
                      lightLevel, lightLevelDeltaLeft, lightLevelDeltaRight, lightLevelDeltaBottom, lightLevelDeltaTop,
                      bsuf, mapObject, elmIdx, glowing, isWall, drawAsVisSprite);
    }
}

static int buildSkymaskQuad(rendpolytype_t polyType, rvertex_t* rvertices, rtexcoord_t* rtexcoords)
{
    V3_Set(rvertices[0].pos, 0, 0, 0);
    V3_Set(rvertices[1].pos, 0, 0, 0);
    V3_Set(rvertices[2].pos, 0, 0, 0);
    V3_Set(rvertices[3].pos, 0, 0, 0);

    if(rtexcoords)
    {
        rtexcoords[0].st[0] = 0;
        rtexcoords[0].st[1] = 1;
        rtexcoords[1].st[0] = 0;
        rtexcoords[1].st[1] = 0;
        rtexcoords[2].st[0] = 1;
        rtexcoords[2].st[1] = 1;
        rtexcoords[3].st[0] = 1;
        rtexcoords[3].st[1] = 0;
    }

    return 4;
}

/*static __inline*/ void translateGeometryAxis(rendpolytype_t polyType, byte axis, float delta,
    rvertex_t* rvertices, rtexcoord_t* rtexcoord, rcolor_t* rcolor, rcolor_t* shinyColor)
{
    rvertices[0].pos[axis] += delta;
}

/*static __inline*/ void translateGeometryX(rendpolytype_t polyType, vec3_t* deltas,
    size_t count, rvertex_t* rvertices, rtexcoord_t* rtexcoords, rcolor_t* rcolors,
    rcolor_t* rcolorsShiny)
{
    size_t i;
    for(i = 0; i < count; ++i)
    {
        translateGeometryAxis(polyType, VX, deltas[i][VX], rvertices+i, rtexcoords? rtexcoords+i : 0, rcolors? rcolors+i : 0, rcolorsShiny? rcolorsShiny+i : 0);
    }
}

/*static __inline*/ void translateGeometryY(rendpolytype_t polyType, vec3_t* deltas,
    size_t count, rvertex_t* rvertices, rtexcoord_t* rtexcoords, rcolor_t* rcolors,
    rcolor_t* rcolorsShiny)
{
    size_t i;
    for(i = 0; i < count; ++i)
    {
        translateGeometryAxis(polyType, VY, deltas[i][VY], rvertices+i, rtexcoords? rtexcoords+i : 0, rcolors? rcolors+i : 0, rcolorsShiny? rcolorsShiny+i : 0);
    }
}

/*static __inline*/ void translateGeometryZ(rendpolytype_t polyType, vec3_t* deltas,
    size_t count, rvertex_t* rvertices, rtexcoord_t* rtexcoords, rcolor_t* rcolors,
    rcolor_t* rcolorsShiny)
{
    size_t i;
    for(i = 0; i < count; ++i)
    {
        translateGeometryAxis(polyType, VZ, deltas[i][VZ], rvertices+i, rtexcoords? rtexcoords+i : 0, rcolors? rcolors+i : 0, rcolorsShiny? rcolorsShiny+i : 0);
    }
}

/*static __inline*/ void translateGeometryXY(rendpolytype_t polyType, vec3_t* deltas,
    size_t count, rvertex_t* rvertices, rtexcoord_t* rtexcoords, rcolor_t* rcolors,
    rcolor_t* rcolorsShiny)
{
    translateGeometryX(polyType, deltas, count, rvertices, rtexcoords, rcolors, rcolorsShiny);
    translateGeometryY(polyType, deltas, count, rvertices, rtexcoords, rcolors, rcolorsShiny);
}

/*static __inline*/ void translateGeometryXYZ(rendpolytype_t polyType, vec3_t* deltas,
    size_t count, rvertex_t* rvertices, rtexcoord_t* rtexcoords, rcolor_t* rcolors,
    rcolor_t* rcolorsShiny)
{
    translateGeometryXY(polyType, deltas, count, rvertices, rtexcoords, rcolors, rcolorsShiny);
    translateGeometryZ (polyType, deltas, count, rvertices, rtexcoords, rcolors, rcolorsShiny);
}

/*static __inline*/ void setGeometryAxis(rendpolytype_t polyType, byte axis, float delta,
    rvertex_t* rvertices, rtexcoord_t* rtexcoords, rcolor_t* rcolors, rcolor_t* rcolorsShiny)
{
    rvertices[0].pos[axis] = 0;
    translateGeometryAxis(polyType, axis, delta, rvertices, rtexcoords, rcolors, rcolorsShiny);
}

/*static __inline*/ void setGeometryX(rendpolytype_t polyType, vec3_t* destPoints,
    size_t count, rvertex_t* rvertices, rtexcoord_t* rtexcoords, rcolor_t* rcolors,
    rcolor_t* rcolorsShiny)
{
    size_t i;
    for(i = 0; i < count; ++i)
    {
        setGeometryAxis(polyType, VX, destPoints[i][VX], rvertices+i, rtexcoords? rtexcoords+i : 0, rcolors? rcolors+i : 0, rcolorsShiny? rcolorsShiny+i : 0);
    }
}

/*static __inline*/ void setGeometryY(rendpolytype_t polyType, vec3_t* destPoints,
    size_t count, rvertex_t* rvertices, rtexcoord_t* rtexcoords, rcolor_t* rcolors,
    rcolor_t* rcolorsShiny)
{
    size_t i;
    for(i = 0; i < count; ++i)
    {
        setGeometryAxis(polyType, VY, destPoints[i][VY], rvertices+i, rtexcoords? rtexcoords+i : 0, rcolors? rcolors+i : 0, rcolorsShiny? rcolorsShiny+i : 0);
    }
}

/*static __inline*/ void setGeometryZ(rendpolytype_t polyType, vec3_t* destPoints,
    size_t count, rvertex_t* rvertices, rtexcoord_t* rtexcoords, rcolor_t* rcolors,
    rcolor_t* rcolorsShiny)
{
    size_t i;
    for(i = 0; i < count; ++i)
    {
        setGeometryAxis(polyType, VZ, destPoints[i][VZ], rvertices+i, rtexcoords? rtexcoords+i : 0, rcolors? rcolors+i : 0, rcolorsShiny? rcolorsShiny+i : 0);
    }
}

/*static __inline*/ void setGeometryXY(rendpolytype_t polyType, vec3_t* destPoints,
    size_t count, rvertex_t* rvertices, rtexcoord_t* rtexcoords, rcolor_t* rcolors,
    rcolor_t* rcolorsShiny)
{
    size_t i;
    for(i = 0; i < count; ++i)
    {
        setGeometryX(polyType, &destPoints[i], 1, rvertices+i, rtexcoords? rtexcoords+i : 0, rcolors? rcolors+i : 0, rcolorsShiny? rcolorsShiny+i : 0);
        setGeometryY(polyType, &destPoints[i], 1, rvertices+i, rtexcoords? rtexcoords+i : 0, rcolors? rcolors+i : 0, rcolorsShiny? rcolorsShiny+i : 0);
    }
}

/*static __inline*/ void setGeometryXYZ(rendpolytype_t polyType, vec3_t* destPoints,
    size_t count, rvertex_t* rvertices, rtexcoord_t* rtexcoords, rcolor_t* rcolors,
    rcolor_t* rcolorsShiny)
{
    size_t i;
    for(i = 0; i < count; ++i)
    {
        setGeometryX(polyType, &destPoints[i], 1, rvertices+i, rtexcoords? rtexcoords+i : 0, rcolors? rcolors+i : 0, rcolorsShiny? rcolorsShiny+i : 0);
        setGeometryY(polyType, &destPoints[i], 1, rvertices+i, rtexcoords? rtexcoords+i : 0, rcolors? rcolors+i : 0, rcolorsShiny? rcolorsShiny+i : 0);
        setGeometryZ(polyType, &destPoints[i], 1, rvertices+i, rtexcoords? rtexcoords+i : 0, rcolors? rcolors+i : 0, rcolorsShiny? rcolorsShiny+i : 0);
    }
}

#if 0
/*static __inline*/ float calculateZBiasInter(float factor, float _height, float _skyHeight)
{
    // Influenced by the source bias.
    if(_skyHeight != 0)
    {
        float height    = fabs(_height);
        float skyHeight = fabs(_skyHeight);
        float zDelta    = skyHeight - height;
        return MINMAX_OF(-0x80, zDelta * (1 - factor) + zDelta * factor, 0x7f);
    }
    return 0;
}

/*static __inline*/ float getZBiasHeightDelta(plane_t* ffloor, plane_t* fceil, float bias, float height)
{
    assert(ffloor && fceil);
    {
    float diff = 0;
    if(bias < 0)
    {   // Calculate Z difference to the ceiling.
        diff = fceil->visHeight - height;
    }
    else if(bias > 0)
    {
        // Calculate Z difference to the floor.
        diff = height - ffloor->visHeight;
    }
    return MAX_OF(diff, 0);
    }
}

/*static __inline*/ float calculateZBiasDimming(float diffZ, float bias)
{
    float dimming = 0;
    if(diffZ && bias)
    {
        if(bias < 0)
            dimming = 1 - (diffZ * -bias) / 35000.0f;
        else
            dimming = 1 - (diffZ *  bias) / 35000.0f;

        if(dimming < .5f)
            dimming = .5f;
    }
    return dimming;
}
#endif

/*static __inline*/ void getSurfaceLightLevelDeltas(seg_t* seg, plane_t* ffloor, plane_t* fceil,
    plane_t* bfloor, plane_t* bceil, float bottom, float top, float skyFloor, float skyCeil,
    float* deltaL, float* deltaR, float* deltaB, float* deltaT)
{
#define ZBIAS_FACTOR                (1.0)

    assert(deltaL && deltaR && deltaB && deltaT);
    Linedef_LightLevelDelta(seg->lineDef, seg->side, deltaL, deltaR);
    /// Linear interpolation of the linedef light deltas to the edges of the seg.
    { float diffX, diffY;
    diffX = *deltaR - *deltaL;
    diffY = *deltaT - *deltaB;
    *deltaR = *deltaL + ((seg->offset + seg->length) / seg->lineDef->length) * diffX;
    *deltaL += (seg->offset / seg->lineDef->length) * diffX;

    *deltaB = 0;
    *deltaT = 0;
    }

#undef ZBIAS_FACTOR
}

/*static __inline*/ void setGeometryDeltasZ(size_t count, vec3_t* deltas, float height)
{
    assert(count != 0 && deltas);
    { size_t i;
    for(i = 0; i < count; ++i)
    {
        deltas[i][VZ] = height;
    }}
}

/*static __inline*/ void translateGeometryDeltasZ(size_t count, vec3_t* edgeDeltas,
    float* deltasZ, rvertex_t* rvertices)
{
    assert(count != 0 && edgeDeltas && deltasZ && rvertices);
    { size_t i;
    for(i = 0; i < count; ++i)
    {
        setGeometryDeltasZ(1, edgeDeltas+i,   deltasZ[i]);
    }}
}

/*static __inline*/ void getSurfaceNormal(float* normal, linedef_t* lineDef, byte side)
{
    assert(normal && lineDef && lineDef->L_side(side));
    { sidedef_t* sideDef = lineDef->L_side(side);
    V3_Copy(normal, sideDef->SW_middlenormal);
    if(side)
    {
        normal[VZ] *= -1;
    }
    }
}

/*static*/ void getSkymaskBottomTop(plane_t* ffloor, plane_t* fceil, plane_t* bfloor, plane_t* bceil,
    float skyFloor, float skyCeil,
    seg_t* seg, sector_t* frontsec, sector_t* backsec,
    float* bottom, float* top)
{
    assert(ffloor && fceil && bottom && top);
    *bottom = 0;
    *top = 0;
    if(R_IsSkySurface(&frontsec->SP_floorsurface))
        if(!P_IsInVoid(viewPlayer))
        {
            if(!(backsec && R_IsSkySurface(&bfloor->surface)) && ffloor->visHeight > skyFloor)
            {
                *top    = MIN_OF(fceil->visHeight, ffloor->visHeight);
                *bottom = skyFloor;
            }
        }
        else if(!backsec || (R_IsSkySurface(&bfloor->surface) && bfloor->visHeight > skyFloor))
        {
            if(!(backsec && R_IsSkySurface(&bfloor->surface) && bfloor->visHeight <= ffloor->visHeight))
            {
                *top    = MIN_OF(fceil->visHeight, (backsec? bfloor->visHeight : ffloor->visHeight));
                *bottom = (backsec? ffloor->visHeight : skyFloor);
            }
        }
}

/*static*/ void getSkymaskBottomTop2(plane_t* ffloor, plane_t* fceil, plane_t* bfloor, plane_t* bceil,
    float skyFloor, float skyCeil,
    seg_t* seg, sector_t* frontsec, sector_t* backsec,
    float* bottom, float* top)
{
    assert(ffloor && fceil && bottom && top);
    *bottom = 0;
    *top = 0;
    if(R_IsSkySurface(&fceil->surface))
        if(!P_IsInVoid(viewPlayer))
        {
            if(!(backsec && R_IsSkySurface(&bceil->surface)) && fceil->visHeight < skyCeil)
            {
                *top    = skyCeil;
                *bottom = MAX_OF(fceil->visHeight, ffloor->visHeight);
            }
        }
        else if(!backsec || (R_IsSkySurface(&bceil->surface) && bceil->visHeight < skyCeil))
        {
            if(!(backsec && R_IsSkySurface(&bceil->surface) && bceil->visHeight >= fceil->visHeight))
            {
                *top    = (backsec? fceil->visHeight : skyCeil);
                *bottom = MAX_OF(ffloor->visHeight, (backsec? bceil->visHeight : fceil->visHeight));
            }
        }
}

/*static*/ void getSkymaskBottomTop3(plane_t* ffloor, plane_t* fceil, plane_t* bfloor, plane_t* bceil,
    float skyFloor, float skyCeil,
    seg_t* seg, sector_t* frontsec , sector_t* backsec,
    float* bottom, float* top)
{
    assert(ffloor && fceil && bfloor && bceil && bottom && top);
    {
    float texOffset[2];
    linedef_t* lineDef = seg->lineDef;

    *bottom = 0;
    *top = 0;
    if(!(bceil->visHeight <= ffloor->visHeight || bfloor->visHeight >= fceil->visHeight))
    {
        if(lineDef)
        {
           sidedef_t* sideDef = SEG_SIDEDEF(seg);
           if(sideDef && (sideDef->SW_middleinflags & SUIF_PVIS))
               R_FindBottomTop(SEG_MIDDLE, seg->offset, &sideDef->SW_middlesurface,
                               ffloor, fceil, bfloor, bceil,
                               (lineDef->flags & DDLF_DONTPEGBOTTOM)? true : false,
                               (lineDef->flags & DDLF_DONTPEGTOP)? true : false,
                               (sideDef->flags & SDF_MIDDLE_STRETCH)? true : false,
                               LINE_SELFREF(lineDef)? true : false,
                               bottom, top, texOffset);
        }
    }
    else
    {
        *bottom = skyFloor;
        *top = fceil->visHeight;
        return;
    }

    if(*top > *bottom)
    {
        if(R_IsSkySurface(&frontsec->SP_floorsurface) && R_IsSkySurface(&backsec->SP_floorsurface) &&
            bfloor->visHeight > skyFloor && !(*bottom > ffloor->visHeight))
        {
            *bottom = skyFloor;
            *top = MIN_OF(bfloor->visHeight, *bottom);
        }
    }
    else
    {
        *bottom = 0;
        *top = 0;
    }
    }
}

/*static*/ void getSkymaskBottomTop4(plane_t* ffloor, plane_t* fceil, plane_t* bfloor, plane_t* bceil,
    float skyFloor, float skyCeil,
    seg_t* seg, sector_t* frontsec, sector_t* backsec,
    float* bottom, float* top)
{
    assert(ffloor && fceil && bfloor && bceil && bottom && top);
    {
    float texOffset[2];
    linedef_t* lineDef = seg->lineDef;

    *bottom = 0;
    *top = 0;
    if(!(bceil->visHeight <= ffloor->visHeight || bfloor->visHeight >= fceil->visHeight))
    {
        if(lineDef)
        {
            sidedef_t* sideDef = SEG_SIDEDEF(seg);
            if(sideDef && (sideDef->SW_middleinflags & SUIF_PVIS))
               R_FindBottomTop(SEG_MIDDLE, seg->offset, &sideDef->SW_middlesurface,
                               ffloor, fceil, bfloor, bceil,
                               (lineDef->flags & DDLF_DONTPEGBOTTOM)? true : false,
                               (lineDef->flags & DDLF_DONTPEGTOP)? true : false,
                               (sideDef->flags & SDF_MIDDLE_STRETCH)? true : false,
                               LINE_SELFREF(lineDef)? true : false,
                               bottom, top, texOffset);
        }
    }
    else
    {
        *bottom = ffloor->visHeight;
        *top = skyCeil;
        return;
    }

    if(*top > *bottom)
    {
        if(R_IsSkySurface(&fceil->surface) && R_IsSkySurface(&bceil->surface) &&
           bceil->visHeight < skyCeil && !(*top < fceil->visHeight))
        {
            *bottom = MAX_OF(bceil->visHeight, *top);
            *top = skyCeil;
        }
    }
    else
    {
        *bottom = 0;
        *top = 0;
    }
    }
}

/*static*/ void getSkymaskBottomTop5(plane_t* ffloor, plane_t* fceil, plane_t* bfloor, plane_t* bceil,
    float skyFloor, float skyCeil, boolean bottomHasMaterialFix,
    seg_t* seg, sector_t* frontsec, sector_t* backsec,
    float* bottom, float* top, boolean* addSolidViewSeg)
{
    assert(ffloor && fceil && bfloor && bceil && bottom && top);
    {
    *bottom = 0;
    *top = 0;
    *addSolidViewSeg = false;
    if((R_IsSkySurface(&ffloor->surface) && R_IsSkySurface(&bfloor->surface)) ||
       (R_IsSkySurface(&bfloor->surface) && bottomHasMaterialFix))
    {
        if(bfloor->visHeight > skyFloor)
        {
           *bottom = skyFloor;
           *top = bfloor->visHeight;
        }
        // Ensure we add a solid view seg.
        *addSolidViewSeg = true;
    }
    }
}

/*static*/ void getSkymaskBottomTop6(plane_t* ffloor, plane_t* fceil, plane_t* bfloor, plane_t* bceil,
    float skyFloor, float skyCeil, boolean topHasMaterialFix,
    seg_t* seg, sector_t* frontsec, sector_t* backsec,
    float* bottom, float* top, boolean* addSolidViewSeg)
{
    assert(ffloor && fceil && bfloor && bceil && bottom && top);
    {
    *bottom = 0;
    *top = 0;
    *addSolidViewSeg = false;
    if((R_IsSkySurface(&fceil->surface) && R_IsSkySurface(&bceil->surface)) ||
       (R_IsSkySurface(&bceil->surface) && topHasMaterialFix))
    {
        if(bceil->visHeight < skyCeil)
        {
            *top = skyCeil;
            *bottom = bceil->visHeight;
        }
        // Ensure we add a solid view seg.
        *addSolidViewSeg = true;
    }
    }
}

static __inline float getSkyFloor(plane_t* ffloor, plane_t* fceil, plane_t* bfloor, plane_t* bceil)
{
    return ((P_IsInVoid(viewPlayer) && !(R_IsSkySurface(&ffloor->surface) && bfloor && R_IsSkySurface(&bfloor->surface)))? ffloor->visHeight : (skyFix[PLN_FLOOR].height   < DDMAXFLOAT? skyFix[PLN_FLOOR  ].height : 0));
}

static __inline float getSkyCeiling(plane_t* ffloor, plane_t* fceil, plane_t* bfloor, plane_t* bceil)
{
    return ((P_IsInVoid(viewPlayer) && !(R_IsSkySurface(&fceil-> surface) && bceil && R_IsSkySurface(&bceil-> surface)))? fceil-> visHeight : (skyFix[PLN_CEILING].height > DDMINFLOAT? skyFix[PLN_CEILING].height : 0));
}

/*static*/ void getSkymaskBottomOffsets(seg_t* seg, sector_t* frontsec, sector_t* backsec,
    plane_t* ffloor, plane_t* fceil, plane_t* bfloor, plane_t* bceil, float skyFloor, float skyCeil,
    float* offsets, float* bottom, float* top, boolean* addSolidViewSeg)
{
    assert(offsets && bottom && top && addSolidViewSeg);
    *bottom = 0;
    *top    = 0;
    *addSolidViewSeg = false;
    if(!backsec || backsec != seg->SG_frontsector)
    {
       getSkymaskBottomTop(ffloor, fceil, bfloor, bceil, skyFloor, skyCeil, seg, frontsec, backsec, bottom, top);
    }
    if(*top > *bottom)
    {
        offsets[0] = *bottom;
        offsets[1] = *top;
        offsets[2] = *bottom;
        offsets[3] = *top;
    }
}

/*static*/ void drawSSectSkyFixBottom(rendpolytype_t polyType, subsector_t* ssec, boolean clipBackFacing, boolean flipSurfaceNormals)
{
    rtexmapunit_t rTU[NUM_TEXMAP_UNITS];
    rtexcoord_t rtexcoords[4];
    rvertex_t rvertices[4];
    rcolor_t rcolors[4], rcolorsShiny[4];
    size_t numVerts;
    seg_t** segPtr;

    numVerts = buildSkymaskQuad(polyType, rvertices, (polyType == RPT_NORMAL? rtexcoords : 0));

    segPtr = ssec->segs;
    while(*segPtr)
    {
        vec3_t edgeDeltasXY[4];
        seg_t* seg = *segPtr;
        linedef_t* lineDef = seg->lineDef;
        sidedef_t* sideDef = lineDef? lineDef->L_side(seg->side) : 0;
        sector_t* frontsec = seg->SG_frontsector;
        sector_t* backsec  = seg->SG_backsector;
        vertex_t* from = seg->SG_v(0);
        vertex_t* to   = seg->SG_v(1);
        plane_t* ffloor, *fceil, *bceil, *bfloor;
        float skyFloor, skyCeil, bottom, top;
        vec3_t surfaceNormal, edgeDeltasZ[4];
        boolean addSolidViewSeg;
        float offsets[4];

        if(!skymaskSegIsVisible(seg, clipBackFacing))
        {
            segPtr++;
            continue;
        }

        // Get the start and end vertices, left then right, bottom and top.
        V3_Set(edgeDeltasXY[0], from->V_pos[VX], from->V_pos[VY], 0);
        V3_Set(edgeDeltasXY[1], from->V_pos[VX], from->V_pos[VY], 0);

        V3_Set(edgeDeltasXY[2], to->  V_pos[VX], to->  V_pos[VY], 0);
        V3_Set(edgeDeltasXY[3], to->  V_pos[VX], to->  V_pos[VY], 0);

        setGeometryXY(polyType, edgeDeltasXY, numVerts, rvertices, (polyType == RPT_NORMAL? rtexcoords : 0), (polyType == RPT_NORMAL? rcolors : 0), (polyType == RPT_NORMAL? rcolorsShiny : 0));

        ffloor = frontsec->SP_plane(PLN_FLOOR);
        fceil  = frontsec->SP_plane(PLN_CEILING);

        bceil  = backsec ? backsec->SP_plane(PLN_CEILING) : 0;
        bfloor = backsec ? backsec->SP_plane(PLN_FLOOR)   : 0;

        skyFloor = getSkyFloor(ffloor, fceil, bfloor, bceil);
        skyCeil  = getSkyCeiling(ffloor, fceil, bfloor, bceil);

        getSurfaceNormal(surfaceNormal, lineDef, flipSurfaceNormals);

        addSolidViewSeg = false;
        getSkymaskBottomOffsets(seg, frontsec, backsec, ffloor, fceil, bfloor, bceil, skyFloor, skyCeil, offsets, &bottom, &top, &addSolidViewSeg);

        if(top > bottom)
        {
            float lightLevelDeltaLeft = 0, lightLevelDeltaRight = 0, lightLevelDeltaBottom = 0, lightLevelDeltaTop = 0;
            const float* ambientLightColor = R_GetSectorLightColor(frontsec);
            float ambientLightLevel = frontsec->lightLevel;

            getSurfaceLightLevelDeltas(seg, ffloor, fceil, bfloor, bceil, bottom, top, skyFloor, skyCeil, &lightLevelDeltaLeft, &lightLevelDeltaRight, &lightLevelDeltaBottom, &lightLevelDeltaTop);
            translateGeometryDeltasZ(numVerts, edgeDeltasZ, offsets, rvertices);

            memset(rTU, 0, sizeof(rTU));
            setGeometryZ(polyType, edgeDeltasZ, numVerts, rvertices, (polyType == RPT_NORMAL? rtexcoords : 0), (polyType == RPT_NORMAL? rcolors : 0), (polyType == RPT_NORMAL? rcolorsShiny : 0));
            prepareSkyMaskSurface(polyType,     numVerts, rvertices, (polyType == RPT_NORMAL? rtexcoords : 0), (polyType == RPT_NORMAL? rcolors : 0), (polyType == RPT_NORMAL? rcolorsShiny : 0), rTU, 0, surfaceNormal, 0, 1, 0, ambientLightColor, ambientLightLevel, lightLevelDeltaLeft, lightLevelDeltaRight, lightLevelDeltaBottom, lightLevelDeltaTop, 0, 0, 0, 0, seg->length, true, renderTextures!=2?ffloor->PS_material:Materials_ToMaterial(Materials_IndexForName(MN_SYSTEM_NAME":gray")), false);
            RL_AddPoly(PT_TRIANGLE_STRIP, polyType,       rvertices, (polyType == RPT_NORMAL? rtexcoords : 0), 0, 0, (polyType == RPT_NORMAL? rcolors : 0), numVerts, 0, 0, 0, rTU);
        }
        segPtr++;
    }
}

/*static*/ void getSkymaskTopOffsets(seg_t* seg, sector_t* frontsec, sector_t* backsec,
    plane_t* ffloor, plane_t* fceil, plane_t* bfloor, plane_t* bceil, float skyFloor, float skyCeil,
    float* offsets, float* bottom, float* top, boolean* addSolidViewSeg)
{
    assert(offsets && bottom && top && addSolidViewSeg);
    *bottom = 0;
    *top    = 0;
    *addSolidViewSeg = false;
    if(!backsec || backsec != seg->SG_frontsector)
    {
        getSkymaskBottomTop2(ffloor, fceil, bfloor, bceil, skyFloor, skyCeil, seg, frontsec, backsec, bottom, top);
    }
    if(*top > *bottom)
    {
        offsets[0] = *bottom;
        offsets[1] = *top;
        offsets[2] = *bottom;
        offsets[3] = *top;
    }
}

/*static*/ void drawSSectSkyFixTop(rendpolytype_t polyType, subsector_t* ssec, boolean clipBackFacing, boolean flipSurfaceNormals)
{
    rtexmapunit_t rTU[NUM_TEXMAP_UNITS];
    rtexcoord_t rtexcoords[4];
    rvertex_t rvertices[4];
    rcolor_t rcolors[4], rcolorsShiny[4];
    size_t numVerts;
    seg_t** segPtr;

    numVerts = buildSkymaskQuad(polyType, rvertices, (polyType == RPT_NORMAL? rtexcoords : 0));

    segPtr = ssec->segs;
    while(*segPtr)
    {
        vec3_t edgeDeltasXY[4];
        seg_t* seg = *segPtr;
        linedef_t* lineDef = seg->lineDef;
        sidedef_t* sideDef = lineDef? lineDef->L_side(seg->side) : 0;
        sector_t* frontsec = seg->SG_frontsector;
        sector_t* backsec  = seg->SG_backsector;
        vertex_t* from = seg->SG_v(0);
        vertex_t* to   = seg->SG_v(1);
        plane_t* ffloor, *fceil, *bceil, *bfloor;
        float skyFloor, skyCeil, bottom, top;
        vec3_t surfaceNormal, edgeDeltasZ[4];
        boolean addSolidViewSeg;
        float offsets[4];

        if(!skymaskSegIsVisible(seg, clipBackFacing))
        {
            segPtr++;
            continue;
        }

        // Get the start and end vertices, left then right, bottom and top.
        V3_Set(edgeDeltasXY[0], from->V_pos[VX], from->V_pos[VY], 0);
        V3_Set(edgeDeltasXY[1], from->V_pos[VX], from->V_pos[VY], 0);

        V3_Set(edgeDeltasXY[2], to->  V_pos[VX], to->  V_pos[VY], 0);
        V3_Set(edgeDeltasXY[3], to->  V_pos[VX], to->  V_pos[VY], 0);

        setGeometryXY(polyType, edgeDeltasXY, numVerts, rvertices, (polyType == RPT_NORMAL? rtexcoords : 0), (polyType == RPT_NORMAL? rcolors : 0), (polyType == RPT_NORMAL? rcolorsShiny : 0));

        ffloor = frontsec->SP_plane(PLN_FLOOR);
        fceil  = frontsec->SP_plane(PLN_CEILING);

        bceil  = backsec ? backsec->SP_plane(PLN_CEILING) : 0;
        bfloor = backsec ? backsec->SP_plane(PLN_FLOOR)   : 0;

        skyFloor = getSkyFloor(ffloor, fceil, bfloor, bceil);
        skyCeil  = getSkyCeiling(ffloor, fceil, bfloor, bceil);

        getSurfaceNormal(surfaceNormal, lineDef, flipSurfaceNormals);

        addSolidViewSeg = false;
        getSkymaskTopOffsets(seg, frontsec, backsec, ffloor, fceil, bfloor, bceil, skyFloor, skyCeil, offsets, &bottom, &top, &addSolidViewSeg);

        if(top > bottom)
        {
            float lightLevelDeltaLeft = 0, lightLevelDeltaRight = 0, lightLevelDeltaBottom = 0, lightLevelDeltaTop = 0;
            const float* ambientLightColor = R_GetSectorLightColor(frontsec);
            float ambientLightLevel = frontsec->lightLevel;

            getSurfaceLightLevelDeltas(seg, ffloor, fceil, bfloor, bceil, bottom, top, skyFloor, skyCeil, &lightLevelDeltaLeft, &lightLevelDeltaRight, &lightLevelDeltaBottom, &lightLevelDeltaTop);
            translateGeometryDeltasZ(numVerts, edgeDeltasZ, offsets, rvertices);

            memset(rTU, 0, sizeof(rTU));
            setGeometryZ(polyType, edgeDeltasZ, numVerts, rvertices, (polyType == RPT_NORMAL? rtexcoords : 0), (polyType == RPT_NORMAL? rcolors : 0), (polyType == RPT_NORMAL? rcolorsShiny : 0));
            prepareSkyMaskSurface(polyType,     numVerts, rvertices, (polyType == RPT_NORMAL? rtexcoords : 0), (polyType == RPT_NORMAL? rcolors : 0), (polyType == RPT_NORMAL? rcolorsShiny : 0), rTU, 0, surfaceNormal, 0, 1, 0, ambientLightColor, ambientLightLevel, lightLevelDeltaLeft, lightLevelDeltaRight, lightLevelDeltaBottom, lightLevelDeltaTop, 0, 0, 0, 0, seg->length, true, renderTextures!=2?fceil->PS_material:Materials_ToMaterial(Materials_IndexForName(MN_SYSTEM_NAME":gray")), false);
            RL_AddPoly(PT_TRIANGLE_STRIP, polyType,       rvertices, (polyType == RPT_NORMAL? rtexcoords : 0), 0, 0, (polyType == RPT_NORMAL? rcolors : 0), numVerts, 0, 0, 0, rTU);
        }
        segPtr++;
    }
}

/*static*/ void getSkymaskClosedOffsets(seg_t* seg, sector_t* frontsec, sector_t* backsec,
    plane_t* ffloor, plane_t* fceil, plane_t* bfloor, plane_t* bceil, float skyFloor, float skyCeil, boolean topHasMaterialFix,
    float* offsets, float* bottom, float* top, boolean* addSolidViewSeg)
{
    assert(offsets && bottom && top && addSolidViewSeg);
    *bottom = 0;
    *top    = 0;
    *addSolidViewSeg = false;
    if(!P_IsInVoid(viewPlayer) && backsec && !(seg->lineDef && LINE_SELFREF(seg->lineDef)) && bceil && (bceil->visHeight <= ffloor->visHeight||bfloor->visHeight>= bceil->visHeight))
    {
        if((backsec && bfloor->visHeight < bceil->visHeight))
            getSkymaskBottomTop4(ffloor, fceil, bfloor, bceil, skyFloor, skyCeil, seg, frontsec, backsec, bottom, top);
        else
            getSkymaskBottomTop6(ffloor, fceil, bfloor, bceil, skyFloor, skyCeil, topHasMaterialFix, seg, frontsec, backsec, bottom, top, addSolidViewSeg);
    }
    if(*top > *bottom)
    {
        offsets[0] = *bottom;
        offsets[1] = *top;
        offsets[2] = *bottom;
        offsets[3] = *top;
    }
}

/**
 * danij: Still needed?
 */
/*static*/ void getSkymaskClosedOffsets2(seg_t* seg, sector_t* frontsec, sector_t* backsec,
    plane_t* ffloor, plane_t* fceil, plane_t* bfloor, plane_t* bceil, float skyFloor, float skyCeil,
    boolean bottomHasMaterialFix,
    float* offsets, float* bottom, float* top, boolean* addSolidViewSeg)
{
    assert(offsets && bottom && top && addSolidViewSeg);
    *bottom = 0;
    *top    = 0;
    *addSolidViewSeg = false;
    if(!P_IsInVoid(viewPlayer) && backsec && !(seg->lineDef && LINE_SELFREF(seg->lineDef)) && bceil && (bceil->visHeight <= ffloor->visHeight||bfloor->visHeight>= bceil->visHeight))
    {
        if((backsec && bfloor->visHeight < bceil->visHeight))
            getSkymaskBottomTop3(ffloor, fceil, bfloor, bceil, skyFloor, skyCeil, seg, frontsec, backsec, bottom, top);
        else
            getSkymaskBottomTop5(ffloor, fceil, bfloor, bceil, skyFloor, skyCeil, bottomHasMaterialFix, seg, frontsec, backsec, bottom, top, addSolidViewSeg);
    }
    if(*top > *bottom)
    {
        offsets[0] = *bottom;
        offsets[1] = *top;
        offsets[2] = *bottom;
        offsets[3] = *top;
    }
}

/*static*/ void drawSSectSkyFixClosed(rendpolytype_t polyType, subsector_t* ssec, boolean clipBackFacing, boolean flipSurfaceNormals)
{
    rtexmapunit_t rTU[NUM_TEXMAP_UNITS];
    rtexcoord_t rtexcoords[4];
    rvertex_t rvertices[4];
    rcolor_t rcolors[4], rcolorsShiny[4];
    size_t numVerts;
    seg_t** segPtr;

    numVerts = buildSkymaskQuad(polyType, rvertices, (polyType == RPT_NORMAL? rtexcoords : 0));

    segPtr = ssec->segs;
    while(*segPtr)
    {
        vec3_t edgeDeltasXY[4];
        seg_t* seg = *segPtr;
        linedef_t* lineDef = seg->lineDef;
        sidedef_t* sideDef = lineDef? lineDef->L_side(seg->side) : 0;
        sector_t* frontsec = seg->SG_frontsector;
        sector_t* backsec  = seg->SG_backsector;
        vertex_t* from = seg->SG_v(0);
        vertex_t* to   = seg->SG_v(1);
        plane_t* ffloor, *fceil, *bceil, *bfloor;
        float skyFloor, skyCeil, bottom, top;
        vec3_t surfaceNormal, edgeDeltasZ[4];
        boolean addSolidViewSeg = false;
        float offsets[4];

        if(!skymaskSegIsVisible(seg, clipBackFacing))
        {
            segPtr++;
            continue;
        }

        // Get the start and end vertices, left then right, bottom and top.
        V3_Set(edgeDeltasXY[0], from->V_pos[VX], from->V_pos[VY], 0);
        V3_Set(edgeDeltasXY[1], from->V_pos[VX], from->V_pos[VY], 0);

        V3_Set(edgeDeltasXY[2], to->  V_pos[VX], to->  V_pos[VY], 0);
        V3_Set(edgeDeltasXY[3], to->  V_pos[VX], to->  V_pos[VY], 0);

        setGeometryXY(polyType, edgeDeltasXY, numVerts, rvertices, (polyType == RPT_NORMAL? rtexcoords : 0), (polyType == RPT_NORMAL? rcolors : 0), (polyType == RPT_NORMAL? rcolorsShiny : 0));

        ffloor = frontsec->SP_plane(PLN_FLOOR);
        fceil  = frontsec->SP_plane(PLN_CEILING);

        bceil  = backsec ? backsec->SP_plane(PLN_CEILING) : 0;
        bfloor = backsec ? backsec->SP_plane(PLN_FLOOR)   : 0;

        skyFloor = getSkyFloor(ffloor, fceil, bfloor, bceil);
        skyCeil  = getSkyCeiling(ffloor, fceil, bfloor, bceil);

        getSurfaceNormal(surfaceNormal, lineDef, flipSurfaceNormals);

        addSolidViewSeg = false;
        getSkymaskClosedOffsets(seg, frontsec, backsec, ffloor, fceil, bfloor, bceil, skyFloor, skyCeil, (sideDef->SW_topsurface.inFlags & SUIF_FIX_MISSING_MATERIAL) != 0, offsets, &bottom, &top, &addSolidViewSeg);

        if(top > bottom)
        {
            const float* ambientLightColor = R_GetSectorLightColor(frontsec);
            float ambientLightLevel = frontsec->lightLevel;
            float lightLevelDeltaLeft = 0, lightLevelDeltaRight = 0, lightLevelDeltaBottom = 0, lightLevelDeltaTop = 0;

            getSurfaceLightLevelDeltas(seg, ffloor, fceil, bfloor, bceil, bottom, top, skyFloor, skyCeil, &lightLevelDeltaLeft, &lightLevelDeltaRight, &lightLevelDeltaBottom, &lightLevelDeltaTop);
            translateGeometryDeltasZ(numVerts, edgeDeltasZ, offsets, rvertices);

            memset(rTU, 0, sizeof(rTU));
            setGeometryZ(polyType, edgeDeltasZ, numVerts, rvertices, (polyType == RPT_NORMAL? rtexcoords : 0), (polyType == RPT_NORMAL? rcolors : 0), (polyType == RPT_NORMAL? rcolorsShiny : 0));
            prepareSkyMaskSurface(polyType,     numVerts, rvertices, (polyType == RPT_NORMAL? rtexcoords : 0), (polyType == RPT_NORMAL? rcolors : 0), (polyType == RPT_NORMAL? rcolorsShiny : 0), rTU, 0, surfaceNormal, 0, 1, 0, ambientLightColor, ambientLightLevel, lightLevelDeltaLeft, lightLevelDeltaRight, lightLevelDeltaBottom, lightLevelDeltaTop, 0, 0, 0, 0, seg->length, true, renderTextures!=2?fceil->PS_material:Materials_ToMaterial(Materials_IndexForName(MN_SYSTEM_NAME":gray")), false);
            RL_AddPoly(PT_TRIANGLE_STRIP, polyType,       rvertices, (polyType == RPT_NORMAL? rtexcoords : 0), 0, 0, (polyType == RPT_NORMAL? rcolors : 0), numVerts, 0, 0, 0, rTU);
        }

        if(!P_IsInVoid(viewPlayer) && backsec && !LINE_SELFREF(lineDef))
        {
            if(addSolidViewSeg)
                seg->frameFlags |= SEGINF_BACKSECSKYFIX;
        }
        segPtr++;
    }
}

static void Rend_SSectSkyFixes(subsector_t* ssec)
{
    assert(ssec && ssec->sector && (devRendSkyMode == 2 || R_SectorContainsSkySurfaces(ssec->sector)));
    {
    rendpolytype_t polyType = (devRendSkyMode? RPT_NORMAL : RPT_SKY_MASK);
    boolean clipBackFacing  = (devRendSkyMode == 2? false : polyType == RPT_NORMAL);
    boolean flipSurfaceNormals = false;

    /// Setup GL state for skymask geometry construction.
    /**
     if(!clipBackFacing)
        glDisable(GL_CULL_FACE);
     */

    drawSSectSkyFixBottom(polyType, ssec, clipBackFacing, flipSurfaceNormals);
    drawSSectSkyFixTop   (polyType, ssec, clipBackFacing, flipSurfaceNormals);
    drawSSectSkyFixClosed(polyType, ssec, clipBackFacing, flipSurfaceNormals);

    /// Restore original GL state.
    /*
     glEnable(GL_CULL_FACE);
     */
    }
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

    // Draw the various skyfixes for all front facing segs in this ssec
    // (includes polyobject segs).
    if(R_SectorContainsSkySurfaces(ssec->sector))
    {
        Rend_SSectSkyFixes(ssec);
    }

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
                solid = Rend_RenderSSWallSeg(ssec, seg);
            else
                solid = Rend_RenderWallSeg(ssec, seg);

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
                boolean solid = Rend_RenderSSWallSeg(ssec, seg);

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
        int texMode;
        float height, texOffset[2], texScale[2];
        const plane_t* plane;
        const surface_t* suf;
        material_t* mat;
        boolean isSkyMasked, addDLights, flipSurfaceNormal, clipBackFacing;

        isSkyMasked = false;
        addDLights = true;
        flipSurfaceNormal = false;
        clipBackFacing = false;

        plane = sect->planes[i];
        suf = &plane->surface;

        // Determine plane height.
        if(!R_IsSkySurface(suf) || (devRendSkyMode != 2 && P_IsInVoid(viewPlayer)))
        {
            height = plane->visHeight;
        }
        else
        {
            if(plane->type != PLN_MID)
                height = skyFix[plane->type].height;
            else
                height = plane->visHeight;
        }

        if(renderTextures == 2)
            texMode = 2;
        else if(!suf->material ||
                (devNoTexFix && (suf->inFlags & SUIF_FIX_MISSING_MATERIAL)))
            texMode = 1;
        else
            texMode = 0;

        if(texMode == 0)
            mat = suf->material;
        else if(texMode == 1)
            // For debug, render the "missing" texture instead of the texture
            // chosen for surfaces to fix the HOMs.
            mat = Materials_ToMaterial(Materials_IndexForName(MN_SYSTEM_NAME":missing"));
        else
            // For lighting debug, render all solid surfaces using the gray texture.
            mat = Materials_ToMaterial(Materials_IndexForName(MN_SYSTEM_NAME":gray"));

        V2_Copy(texOffset, suf->visOffset);

        // Add the Y offset to orient the Y flipped texture.
        if(plane->type == PLN_CEILING)
            texOffset[VY] -= ssec->bBox[1].pos[VY] - ssec->bBox[0].pos[VY];

        // Add the additional offset to align with the worldwide grid.
        texOffset[VX] += ssec->worldGridOffset[VX];
        texOffset[VY] += ssec->worldGridOffset[VY];

        texScale[VX] = ((suf->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
        texScale[VY] = ((suf->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        if(!devRendSkyMode)
        {
            isSkyMasked = R_IsSkySurface(suf);
            clipBackFacing = false;
        }
        else
        {
            //clipBackFacing = R_IsSkySurface(suf)? false : true;
            if(R_IsSkySurface(suf) && devRendSkyMode == 2)
            {
                flipSurfaceNormal = (i == 0? vy < height : vy > height);
            }
        }

        Rend_RenderPlane(ssec, plane->type, height, suf->normal, mat,
                         suf->flags, suf->inFlags, suf->rgba,
                         suf->blendMode, texOffset, texScale,
                         isSkyMasked, addDLights,
                         ssec->bsuf[plane->planeID], plane->planeID,
                         texMode, flipSurfaceNormal, clipBackFacing);
    }

    if(devRendSkyMode == 2)
    {
        /**
         * In mode 2, we draw additional geometry, showing the real "physical" height
         * of any sky masked planes that are drawn at a different height due to the skyFix.
         */
        if(R_IsSkySurface(&sect->SP_floorsurface))
        {
            const plane_t* plane = sect->SP_plane(PLN_FLOOR);
            const surface_t* suf = &plane->surface;
            vec3_t normal;
            V3_Copy(normal, plane->PS_normal);
            Rend_RenderPlane(ssec, PLN_MID, plane->visHeight, normal,
                             renderTextures!=2? sect->SP_floormaterial : Materials_ToMaterial(Materials_IndexForName(MN_SYSTEM_NAME":gray")),
                             suf->flags, suf->inFlags, suf->rgba,
                             BM_NORMAL, NULL, NULL, false,
                             devRendSkyMode != 0,
                             0, plane->planeID, 2, (vy < plane->visHeight), false);
        }

        if(R_IsSkySurface(&sect->SP_ceilsurface))
        {
            const plane_t* plane = sect->SP_plane(PLN_CEILING);
            const surface_t* suf = &plane->surface;
            vec3_t normal;
            V3_Copy(normal, plane->PS_normal);
            Rend_RenderPlane(ssec, PLN_MID, plane->visHeight, normal,
                             renderTextures!=2? sect->SP_ceilmaterial : Materials_ToMaterial(Materials_IndexForName(MN_SYSTEM_NAME":gray")),
                             suf->flags, suf->inFlags, suf->rgba,
                             BM_NORMAL, NULL, NULL, false,
                             devRendSkyMode != 0,
                             0, plane->planeID, 2, (vy > plane->visHeight), false);
        }
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

static void drawNormal(vec3_t origin, vec3_t normal, float scalar)
{
    float               black[4] = { 0, 0, 0, 0 };
    float               red[4] = { 1, 0, 0, 1 };

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glTranslatef(origin[VX], origin[VZ], origin[VY]);

    glBegin(GL_LINES);
    {
        glColor4fv(black);
        glVertex3f(scalar * normal[VX],
                     scalar * normal[VZ],
                     scalar * normal[VY]);
        glColor4fv(red);
        glVertex3f(0, 0, 0);

    }
    glEnd();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

/**
 * Draw the surface normals, primarily for debug.
 */
void Rend_RenderNormals(void)
{
#define NORM_TAIL_LENGTH    (20)

    uint                i;

    if(!devSurfaceNormals)
        return;

    glDisable(GL_CULL_FACE);

    for(i = 0; i < numSegs; ++i)
    {
        seg_t*              seg = &segs[i];
        sidedef_t*          side;
        surface_t*          suf;
        vec3_t              origin;
        float               x, y, bottom, top;
        float               scale = NORM_TAIL_LENGTH;

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
            drawNormal(origin, suf->normal, scale);
        }
        else
        {
            if(side->SW_middlesurface.material)
            {
                top = seg->SG_frontsector->SP_ceilvisheight;
                bottom = seg->SG_frontsector->SP_floorvisheight;
                suf = &side->SW_middlesurface;

                V3_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawNormal(origin, suf->normal, scale);
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
                drawNormal(origin, suf->normal, scale);
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
                drawNormal(origin, suf->normal, scale);
            }
        }
    }

    for(i = 0; i < numSSectors; ++i)
    {
        uint                j;
        subsector_t*        ssec = &ssectors[i];

        for(j = 0; j < ssec->sector->planeCount; ++j)
        {
            vec3_t              origin;
            plane_t*            pln = ssec->sector->SP_plane(j);
            float               scale = NORM_TAIL_LENGTH;

            V3_Set(origin, ssec->midPoint.pos[VX], ssec->midPoint.pos[VY],
                   pln->visHeight);
            if(pln->type != PLN_MID && R_IsSkySurface(&pln->surface))
                origin[VZ] = skyFix[pln->type].height;

            drawNormal(origin, pln->PS_normal, scale);
        }
    }

    glEnable(GL_CULL_FACE);

#undef NORM_TAIL_LENGTH
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
    char buf[80];

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(vtx->V_pos[VX], z, vtx->V_pos[VY]);
    glRotatef(-vang + 180, 0, 1, 0);
    glRotatef(vpitch, 1, 0, 0);
    glScalef(-scale, -scale, 1);

    glEnable(GL_TEXTURE_2D);

    sprintf(buf, "%lu", (unsigned long) (vtx - vertexes));
    FR_SetFont(glFontFixed);
    UI_TextOutEx(buf, 2, 2, UI_Color(UIC_TITLE), alpha);

    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

#define MAX_VERTEX_POINT_DIST 1280

static boolean drawVertex1(linedef_t* li, void* context)
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
            drawVertexIndex(vtx, pos[VZ], dist3D / (theWindow->width / 2),
                            1 - dist3D / MAX_VERTEX_POINT_DIST);
        }
    }

    return true; // Continue iteration.
}

boolean drawPolyObjVertexes(polyobj_t* po, void* context)
{
    return P_PolyobjLinesIterator(po, drawVertex1, po);
}

/**
 * Draw the various vertex debug aids.
 */
void Rend_Vertexes(void)
{
    uint                i;
    float               oldPointSize, oldLineWidth = 1, bbox[4];

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
            vertex_t*           vtx = &vertexes[i];
            float               pos[3], dist;

            if(!vtx->lineOwners)
                continue; // Not a linedef vertex.
            if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ)
                continue; // A polyobj linedef vertex.

            pos[VX] = vtx->V_pos[VX];
            pos[VY] = vtx->V_pos[VY];
            pos[VZ] = DDMAXFLOAT;
            getVertexPlaneMinMax(vtx, &pos[VZ], NULL);

            dist = M_Distance(pos, eye);

            if(dist < MAX_VERTEX_POINT_DIST)
            {
                float               alpha, scale;

                alpha = 1 - dist / MAX_VERTEX_POINT_DIST;
                scale = dist / (theWindow->width / 2);

                drawVertexIndex(vtx, pos[VZ], scale, alpha);
            }
        }
    }

    // Next, the vertexes of all nearby polyobjs.
    bbox[BOXLEFT]   = vx - MAX_VERTEX_POINT_DIST;
    bbox[BOXRIGHT]  = vx + MAX_VERTEX_POINT_DIST;
    bbox[BOXBOTTOM] = vy - MAX_VERTEX_POINT_DIST;
    bbox[BOXTOP]    = vy + MAX_VERTEX_POINT_DIST;
    P_PolyobjsBoxIterator(bbox, drawPolyObjVertexes, NULL);

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
    boolean doLums = (useDynLights || haloMode || spriteLight || useDecorations[DT_LIGHT]);

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

        if(doLums)
        {
            /**
             * Clear the projected dynlight lists. This is done here as
             * the projections are sensitive to distance from the viewer
             * (e.g. some may fade out when far away).
             */
            DL_InitForNewFrame();
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

        Rend_RenderShadows();
    }
    RL_RenderAllLists();

    // Draw various debugging displays:
    Rend_RenderNormals(); // World surface normals.
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
void Rend_CalcLightModRange(const cvar_t* unused)
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
        return; // Can't apply adaptation to a NULL val ptr...

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
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

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

static boolean drawMobjBBox(thinker_t* th, void* context)
{
    static const float  red[3] = { 1, 0.2f, 0.2f}; // non-solid objects
    static const float  green[3] = { 0.2f, 1, 0.2f}; // solid objects
    static const float  yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles

    mobj_t* mo = (mobj_t*) th;
    float size, alpha, eye[3];

    // We don't want the console player.
    if(mo == ddPlayers[consolePlayer].shared.mo)
        return true; // Continue iteration.
    // Is it vissible?
    if(!(mo->subsector && mo->subsector->sector->frameFlags & SIF_VISIBLE))
        return true; // Continue iteration.

    eye[VX] = vx;
    eye[VY] = vz;
    eye[VZ] = vy;

    alpha = 1 - ((M_Distance(mo->pos, eye)/(theWindow->width/2))/4);
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
    return true; // Continue iteration.
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
    static const float  red[3] = { 1, 0.2f, 0.2f}; // non-solid objects
    static const float  green[3] = { 0.2f, 1, 0.2f}; // solid objects
    static const float  yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles

    uint                i;
    float               eye[3];
    material_t* mat;
    material_snapshot_t ms;

    if((!devMobjBBox && !devPolyobjBBox) || netGame)
        return;

    if(!dlBBox)
        dlBBox = constructBBox(0, .08f);

    eye[VX] = vx;
    eye[VY] = vz;
    eye[VZ] = vy;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    mat = Materials_ToMaterial(Materials_IndexForName(MN_SYSTEM_NAME":bbox"));
    Materials_Prepare(&ms, mat, true,
        Materials_VariantSpecificationForContext(MC_SPRITE, 0, 0, 0, 0,
            GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, -1, true, true, true, false));

    GL_BindTexture(TextureVariant_GLName(ms.units[MTU_PRIMARY].tex), ms.units[MTU_PRIMARY].magMode);
    GL_BlendMode(BM_ADD);

    if(devMobjBBox)
        P_IterateThinkers(gx.MobjThinker, 0x1, drawMobjBBox, NULL);

    if(devPolyobjBBox)
    for(i = 0; i < numPolyObjs; ++i)
    {
        const polyobj_t* po = polyObjs[i];
        const sector_t* sec = po->subsector->sector;
        float width  = (po->box[1][0] - po->box[0][0])/2;
        float length = (po->box[1][1] - po->box[0][1])/2;
        float height = (sec->SP_ceilheight - sec->SP_floorheight)/2;
        float pos[3], alpha;

        pos[VX] = po->box[0][0]+width;
        pos[VY] = po->box[0][1]+length;
        pos[VZ] = sec->SP_floorheight;

        alpha = 1 - ((M_Distance(pos, eye)/(theWindow->width/2))/4);
        if(alpha < .25f)
            alpha = .25f; // Don't make them totally invisible.

        Rend_DrawBBox(pos, width, length, height, 0, yellow, alpha, .08f, true);

        {uint j;
        for(j = 0; j < po->numSegs; ++j)
        {
            seg_t* seg = po->segs[j];
            linedef_t* lineDef = seg->lineDef;
            float width  = (lineDef->bBox[BOXRIGHT] - lineDef->bBox[BOXLEFT])/2;
            float length = (lineDef->bBox[BOXTOP] - lineDef->bBox[BOXBOTTOM])/2;
            float pos[3];

            /** Draw a bounding box for the lineDef.
            pos[VX] = lineDef->bBox[BOXLEFT]+width;
            pos[VY] = lineDef->bBox[BOXBOTTOM]+length;
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
