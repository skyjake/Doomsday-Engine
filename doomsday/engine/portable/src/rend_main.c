/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * rend_main.c: Rendering Subsystem
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_dgl.h"
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

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Rend_DrawBBox(const float pos3f[3], float w, float l, float h,
                   const float color3f[3], float alpha, float br,
                   boolean alignToBase);
void Rend_DrawArrow(const float pos3f[3], angle_t a, float s,
                    const float color3f[3], float alpha);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void Rend_RenderBoundingBoxes(void);
static DGLuint constructBBox(DGLuint name, float br);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int useDynLights, translucentIceCorpse;
extern int skyhemispheres;
extern int loMaxRadius;
extern int devNoCulling;
extern boolean firstFrameAfterLoad;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean usingFog = false; // Is the fog in use?
float fogColor[4];
float fieldOfView = 95.0f;
byte smoothTexAnim = true;
int useShinySurfaces = true;

float vx, vy, vz, vang, vpitch;
float viewsidex, viewsidey;

byte freezeRLs = false;
int devSkyMode = false;

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

int devMobjBBox = 0; // 1 = Draw mobj bounding boxes (for debug)
DGLuint dlBBox = 0; // Display list: active-textured bbox model.

byte devVertexIndices = 0; // @c 1= Draw world vertex indices (for debug).
byte devVertexBars = 0; // @c 1= Draw world vertex position bars.
byte devSurfaceNormals = 0; // @c 1= Draw world surface normal tails.
byte devNoTexFix = 0;

// Current sector light color.
const float* sLightColor;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstsubsector; // No range checking for the first one.

// CODE --------------------------------------------------------------------

void Rend_Register(void)
{
    C_VAR_INT("rend-dev-sky", &devSkyMode, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-freeze", &freezeRLs, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-cull-subsectors", &devNoCulling,CVF_NO_ARCHIVE,0,1);
    C_VAR_INT("rend-dev-mobj-bbox", &devMobjBBox, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_FLOAT("rend-camera-fov", &fieldOfView, 0, 1, 179);
    C_VAR_BYTE("rend-tex-anim-smooth", &smoothTexAnim, 0, 0, 1);
    C_VAR_INT("rend-tex-shiny", &useShinySurfaces, 0, 0, 1);
    C_VAR_FLOAT2("rend-light-compression", &lightRangeCompression, 0, -1, 1,
                 Rend_CalcLightModRange);
    C_VAR_INT2("rend-light-ambient", &ambientLight, 0, 0, 255,
               Rend_CalcLightModRange);
    C_VAR_INT2("rend-light-sky", &rendSkyLight, 0, 0, 1,
               LG_MarkAllForUpdate);
    C_VAR_FLOAT("rend-light-wall-angle", &rendLightWallAngle, CVF_NO_MAX,
                0, 0);
    C_VAR_FLOAT("rend-light-attenuation", &rendLightDistanceAttentuation,
                CVF_NO_MAX, 0, 0);
    C_VAR_BYTE("rend-dev-light-mod", &devLightModRange, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-tex-showfix", &devNoTexFix, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-blockmap-debug", &bmapShowDebug, CVF_NO_ARCHIVE, 0, 2);
    C_VAR_FLOAT("rend-dev-blockmap-debug-size", &bmapDebugSize, CVF_NO_ARCHIVE, .1f, 100);
    C_VAR_BYTE("rend-dev-vertex-show-indices", &devVertexIndices, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-vertex-show-bars", &devVertexBars, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-surface-normals", &devSurfaceNormals, CVF_NO_ARCHIVE, 0, 1);

    RL_Register();
    DL_Register();
    SB_Register();
    LG_Register();
    Rend_ModelRegister();
    Rend_ParticleRegister();
    Rend_RadioRegister();
    Rend_ShadowRegister();
    Rend_SkyRegister();
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
    C_Init();                   // Clipper.
    RL_Init();                  // Rendering lists.
    Rend_InitSky();             // The sky.
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
        DGL_DeleteLists(dlBBox, 1);
    dlBBox = 0;
}

void Rend_ModelViewMatrix(boolean useAngles)
{
    vx = viewX;
    vy = viewZ;
    vz = viewY;
    vang = viewAngle / (float) ANGLE_MAX *360 - 90;
    vpitch = viewPitch * 85.0 / 110.0;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_LoadIdentity();
    if(useAngles)
    {
        DGL_Rotatef(vpitch, 1, 0, 0);
        DGL_Rotatef(vang, 0, 1, 0);
    }
    DGL_Scalef(1, 1.2f, 1);      // This is the aspect correction.
    DGL_Translatef(-vx, -vy, -vz);
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

void Rend_VertexColorsGlow(rcolor_t* colors, size_t num)
{
    size_t              i;

    for(i = 0; i < num; ++i)
    {
        rcolor_t*           c = &colors[i];
        c->rgba[CR] = c->rgba[CG] = c->rgba[CB] = 1;
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

static void lightVertex(rcolor_t* color, const rvertex_t* vtx,
                        float lightLevel, const float* ambientColor)
{
    float               lightVal, dist;

    dist = Rend_PointDist2D(vtx->pos);

    // Apply distance attenuation.
    lightVal = R_DistAttenuateLightLevel(dist, lightLevel);

    // Add extra light.
    lightVal += R_ExtraLightDelta();

    Rend_ApplyLightAdaptation(&lightVal);

    // Mix with the surface color.
    color->rgba[CR] = lightVal * ambientColor[CR];
    color->rgba[CG] = lightVal * ambientColor[CG];
    color->rgba[CB] = lightVal * ambientColor[CB];
}

void Rend_VertexColorsApplyTorchLight(rcolor_t* colors,
                                      const rvertex_t* vertices,
                                      size_t numVertices)
{
    size_t              i;
    ddplayer_t*         ddpl = &viewPlayer->shared;

    if(!ddpl->fixedColorMap)
        return; // No need, its disabled.

    for(i = 0; i < numVertices; ++i)
    {
        const rvertex_t*    vtx = &vertices[i];
        rcolor_t*           c = &colors[i];

        Rend_ApplyTorchLight(c->rgba, Rend_PointDist2D(vtx->pos));
    }
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
boolean Rend_DoesMidTextureFillGap(linedef_t *line, int backside)
{
    // Check for unmasked midtextures on twosided lines that completely
    // fill the gap between floor and ceiling (we don't want to give away
    // the location of any secret areas (false walls)).
    if(line->L_backside)
    {
        sector_t*           front = line->L_sector(backside);
        sector_t*           back  = line->L_sector(backside^1);
        sidedef_t*          side  = line->L_side(backside);

        if(side->SW_middlematerial)
        {
            material_t*         mat = side->SW_middlematerial;
            materialtexinst_t*  texInst;

            // Ensure we have up to date info on the materialtex.
            texInst = R_MaterialPrepare(mat->current, 0, NULL, NULL, NULL);

            if(texInst && !texInst->masked &&
               !side->SW_middleblendmode && side->SW_middlergba[3] >= 1)
            {
                float               openTop[2], matTop[2];
                float               openBottom[2], matBottom[2];

                if(side->flags & SDF_MIDDLE_STRETCH)
                    return true;

                openTop[0] = openTop[1] = matTop[0] = matTop[1] =
                    MIN_OF(back->SP_ceilvisheight, front->SP_ceilvisheight);
                openBottom[0] = openBottom[1] = matBottom[0] = matBottom[1] =
                    MAX_OF(back->SP_floorvisheight, front->SP_floorvisheight);

                // Could the mid texture fill enough of this gap for us
                // to consider it completely closed?
                if(mat->height >= (openTop[0] - openBottom[0]) &&
                   mat->height >= (openTop[1] - openBottom[1]))
                {
                    // Possibly. Check the placement of the mid texture.
                    if(Rend_MidMaterialPos
                       (&matBottom[0], &matBottom[1], &matTop[0], &matTop[1],
                        NULL, side->SW_middlevisoffset[VY], mat->height,
                        0 != (line->flags & DDLF_DONTPEGBOTTOM)))
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

static void Rend_MarkSegSectionsPVisible(seg_t *seg)
{
    uint                i, j;
    sidedef_t          *side;
    linedef_t          *line;

    if(!seg || !seg->lineDef)
        return; // huh?

    line = seg->lineDef;
    for(i = 0; i < 2; ++i)
    {
        // Missing side?
        if(!line->L_side(i))
            continue;

        side = line->L_side(i);
        for(j = 0; j < 3; ++j)
            side->sections[j].inFlags |= SUIF_PVIS;

        // Top
        if(!line->L_backside)
        {
            side->sections[SEG_TOP].inFlags &= ~SUIF_PVIS;
            side->sections[SEG_BOTTOM].inFlags &= ~SUIF_PVIS;
        }
        else
        {
            // Check middle texture
            if((!side->SW_middlematerial ||
                (side->SW_middlematerial->flags & MATF_NO_DRAW)) ||
                side->SW_middlergba[3] <= 0) // Check alpha
                side->sections[SEG_MIDDLE].inFlags &= ~SUIF_PVIS;

            if(R_IsSkySurface(&line->L_backsector->SP_ceilsurface) &&
               R_IsSkySurface(&line->L_frontsector->SP_ceilsurface))
               side->sections[SEG_TOP].inFlags &= ~SUIF_PVIS;
            else
            {
                if(i != 0)
                {
                    if(line->L_backsector->SP_ceilvisheight <=
                       line->L_frontsector->SP_ceilvisheight)
                        side->sections[SEG_TOP].inFlags &= ~SUIF_PVIS;
                }
                else
                {
                    if(line->L_frontsector->SP_ceilvisheight <=
                       line->L_backsector->SP_ceilvisheight)
                        side->sections[SEG_TOP].inFlags &= ~SUIF_PVIS;
                }
            }

            // Bottom
            if(R_IsSkySurface(&line->L_backsector->SP_floorsurface) &&
               R_IsSkySurface(&line->L_frontsector->SP_floorsurface))
               side->sections[SEG_BOTTOM].inFlags &= ~SUIF_PVIS;
            else
            {
                if(i != 0)
                {
                    if(line->L_backsector->SP_floorvisheight >=
                       line->L_frontsector->SP_floorvisheight)
                        side->sections[SEG_BOTTOM].inFlags &= ~SUIF_PVIS;
                }
                else
                {
                    if(line->L_frontsector->SP_floorvisheight >=
                       line->L_backsector->SP_floorvisheight)
                        side->sections[SEG_BOTTOM].inFlags &= ~SUIF_PVIS;
                }
            }
        }
    }
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
                       float tcyoff, float texHeight, boolean lowerUnpeg)
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

        // We don't allow vertical tiling.
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
        if(*(side? bottomright : bottomleft) < openingBottom)
        {
            *(side? bottomright : bottomleft) = openingBottom;
        }

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
                        uint lightListIdx, boolean glow, boolean masked,
                        const rtexmapunit_t pTU[NUM_TEXMAP_UNITS])
{
    vissprite_t*        vis = R_NewVisSprite();
    int                 i, c;
    float               midpoint[3];

    midpoint[VX] = (rvertices[0].pos[VX] + rvertices[3].pos[VX]) / 2;
    midpoint[VY] = (rvertices[0].pos[VY] + rvertices[3].pos[VY]) / 2;
    midpoint[VZ] = (rvertices[0].pos[VZ] + rvertices[3].pos[VZ]) / 2;

    vis->type = VSPR_MASKED_WALL;
    vis->lumIdx = 0;
    vis->center[VX] = midpoint[VX];
    vis->center[VY] = midpoint[VY];
    vis->center[VZ] = midpoint[VZ];
    vis->isDecoration = false;
    vis->distance = Rend_PointDist2D(midpoint);
    vis->data.wall.tex = pTU[TU_PRIMARY].tex;
    vis->data.wall.magMode = pTU[TU_PRIMARY].magMode;
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
    vis->data.wall.texCoord[0][VX] = texOffset[VX] / texWidth;
    vis->data.wall.texCoord[1][VX] =
        vis->data.wall.texCoord[0][VX] + wallLength / texWidth;
    vis->data.wall.texCoord[0][VY] = texOffset[VY] / texHeight;
    vis->data.wall.texCoord[1][VY] =
        vis->data.wall.texCoord[0][VY] +
            (rvertices[3].pos[VZ] - rvertices[0].pos[VZ]) / texHeight;
    vis->data.wall.blendMode = blendMode;

    //// \fixme Semitransparent masked polys arn't lit atm
    if(!glow && lightListIdx && numTexUnits > 1 && envModAdd &&
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
    }
    else
    {
        vis->data.wall.modTex = 0;
    }
}

static void quadTexCoords(rtexcoord_t* tc, const rvertex_t* rverts,
                          float wallLength, const float texOrigin[2][3])
{
    tc[0].st[0] = tc[1].st[0] =
        rverts[0].pos[VX] - texOrigin[0][VX];
    tc[3].st[1] = tc[1].st[1] =
        rverts[0].pos[VY] - texOrigin[0][VY];
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

static void quadShinyMaskTexCoords(rtexcoord_t* tc, const rvertex_t* rverts,
                                   float wallLength, float texWidth,
                                   float texHeight, const float texOrigin[2][3],
                                   const float texOffset[2])
{
    tc[0].st[0] = tc[1].st[0] =
        rverts[0].pos[VX] - texOrigin[0][VX] + texOffset[VX] / texWidth;
    tc[3].st[1] = tc[1].st[1] =
        rverts[0].pos[VY] - texOrigin[0][VY] + texOffset[VY] / texHeight;
    tc[3].st[0] = tc[2].st[0] = tc[0].st[0] + wallLength / texWidth;
    tc[2].st[1] = tc[3].st[1] + (rverts[1].pos[VZ] - rverts[0].pos[VZ]) / texHeight;
    tc[0].st[1] = tc[3].st[1] + (rverts[3].pos[VZ] - rverts[2].pos[VZ]) / texHeight;
}

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
    float           texOrigin[2][3];
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
        rtexmapunit_t       pTU[NUM_TEXMAP_UNITS];

        memset(pTU, 0, sizeof(pTU));

        // Allocate enough for the divisions too.
        rvertices = R_AllocRendVertices(params->realNumVertices);
        rtexcoords = R_AllocRendTexCoords(params->realNumVertices);
        rcolors = R_AllocRendColors(params->realNumVertices);

        pTU[TU_PRIMARY].tex = dyn->texture;
        pTU[TU_PRIMARY].magMode = DGL_LINEAR;

        pTU[TU_PRIMARY_DETAIL].tex = 0;
        pTU[TU_INTER].tex = 0;
        pTU[TU_INTER_DETAIL].tex = 0;

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

            width = params->texOrigin[1][VX] - params->texOrigin[0][VX];
            height = params->texOrigin[1][VY] - params->texOrigin[0][VY];

            for(i = 0; i < params->numVertices; ++i)
            {
                rtexcoords[i].st[0] = ((params->texOrigin[1][VX] - params->rvertices[i].pos[VX]) / width * dyn->s[0]) +
                    ((params->rvertices[i].pos[VX] - params->texOrigin[0][VX]) / width * dyn->s[1]);

                rtexcoords[i].st[1] = ((params->texOrigin[1][VY] - params->rvertices[i].pos[VY]) / height * dyn->t[0]) +
                    ((params->rvertices[i].pos[VY] - params->texOrigin[0][VY]) / height * dyn->t[1]);
            }

            memcpy(rvertices, params->rvertices, sizeof(rvertex_t) * params->numVertices);
        }

        if(params->isWall && params->divs)
        {
            RL_AddPoly(PT_FAN, RPT_LIGHT, rvertices + 3 + params->divs[0].num,
                       rtexcoords + 3 + params->divs[0].num, NULL, NULL,
                       NULL, NULL,
                       rcolors + 3 + params->divs[0].num, NULL,
                       3 + params->divs[1].num, 0,
                       0, NULL, pTU);
            RL_AddPoly(PT_FAN, RPT_LIGHT, rvertices, rtexcoords, NULL, NULL,
                       NULL, NULL, rcolors, NULL,
                       3 + params->divs[0].num, 0,
                       0, NULL, pTU);
        }
        else
        {
            RL_AddPoly(params->isWall? PT_TRIANGLE_STRIP : PT_FAN,
                       RPT_LIGHT,
                       rvertices, rtexcoords, NULL, NULL,
                       NULL, NULL, rcolors, NULL,
                       params->numVertices, 0,
                       0, NULL, pTU);
        }

        R_FreeRendVertices(rvertices);
        R_FreeRendTexCoords(rtexcoords);
        R_FreeRendColors(rcolors);
    }
    params->lastIdx++;

    return true; // Continue iteration.
}

typedef struct {
    boolean         isWall;
    rendpolytype_t  type;
    float           texOrigin[2][3];
    float           texOffset[2]; // Texture coordinates for left/top
                                  // (in real texcoords).
    boolean         texFlipH;
    boolean         texFlipV;
    const float*    normal; // Surface normal.
    float           alpha;
    const float*    sectorLightLevel;
    const float*    sectorLightColor;
    const float*    surfaceColor;
    material_t*     mat; // Material to take the primary texture from.
    material_t*     blendMat; // For smooth texture animation.
    material_t*     detailMat; // Material to take detail info from.
    material_t*     blendDetailMat; // For smooth texture animation.
    material_t*     shinyMat; // Material to take the shiny info from.

    uint            lightListIdx; // List of lights that affect this poly.
    blendmode_t     blendMode; // Primitive-specific blending mode.
    boolean         forceOpaque; // Used when we never want blending.
    boolean         glowing;

// For bias:
    void*           mapObject;
    uint            elmIdx;
    biasaffection_t* affection;
    biastracker_t*  tracker;

// Wall only (todo).
    const float*    segLength;
    float           wallAngleLightLevelDelta;
    const float*    surfaceColor2; // Secondary color.
} rendworldpoly_params_t;

static boolean renderWorldPoly(rvertex_t* rvertices, uint numVertices,
                               const walldiv_t* divs,
                               const rendworldpoly_params_t* p)
{
    rtexmapunit_t       pTU[NUM_TEXMAP_UNITS];
    rcolor_t*           rcolors;
    rtexcoord_t*        rtexcoords = NULL, *rtexcoords2 = NULL,
                       *rtexcoords5 = NULL;
    boolean             drawAsVisSprite = false;
    uint                realNumVertices =
        p->isWall && divs? 3 + divs[0].num + 3 + divs[1].num : numVertices;
    rcolor_t*           shinyColors = NULL;
    rtexcoord_t*        shinyTexCoords = NULL;
    boolean             useLights = false;
    uint                numLights = 0;
    float               interPos = 0;
    DGLuint             modTex = 0;
    float               modTexTC[2][2];
    float               modColor[3];
    boolean             isGlowing = p->glowing;
    gltexture_t         tex, detailTex, shinyTex, shinyMaskTex;
    gltexture_t         interTex, interDetailTex;

    memset(modTexTC, 0, sizeof(modTexTC));
    memset(modColor, 0, sizeof(modColor));

    memset(&tex, 0, sizeof(tex));
    memset(&detailTex, 0, sizeof(detailTex));
    memset(&shinyTex, 0, sizeof(shinyTex));
    memset(&shinyMaskTex, 0, sizeof(shinyMaskTex));
    memset(&interTex, 0, sizeof(interTex));
    memset(&interDetailTex, 0, sizeof(interDetailTex));

    // ShinySurface?
    if(p->shinyMat && p->type != RPT_SKY_MASK)
    {
        // We'll reuse the same verts but we need new colors.
        shinyColors = R_AllocRendColors(realNumVertices);
        // The normal texcoords are used with the mask.
        // New texcoords are required for shiny texture.
        shinyTexCoords = R_AllocRendTexCoords(realNumVertices);

        shinyTex.id = p->shinyMat->shiny.tex;
        shinyTex.magMode = DGL_LINEAR;
        shinyTex.width = p->shinyMat->width;
        shinyTex.height = p->shinyMat->height;
        shinyTex.flags = 0;

        if(p->shinyMat->shiny.maskTex)
        {
            shinyMaskTex.id = p->shinyMat->shiny.maskTex;
            shinyMaskTex.magMode = glmode[texMagMode];
            shinyMaskTex.width = p->shinyMat->width * p->shinyMat->shiny.maskWidth;
            shinyMaskTex.height = p->shinyMat->height * p->shinyMat->shiny.maskHeight;
            shinyMaskTex.flags = 0;
        }
    }

    if(p->type != RPT_SKY_MASK)
    {
        // Prepare the primary texture.
        R_MaterialPrepare(p->mat->current, 0, &tex, NULL, NULL);
        curTex = tex.id;
        if(p->forceOpaque)
            tex.flags = 0;

        // Prepare the detail texture.
        if(p->detailMat && p->detailMat->current->detail &&
           !(tex.flags & GLTXF_MASKED))
            R_MaterialPrepare(p->detailMat->current, 0, NULL, &detailTex, NULL);

        // If alpha, masked or blended we'll render as a vissprite.
        if(p->alpha < 1 || (tex.flags & GLTXF_MASKED) || p->blendMode > 0)
            drawAsVisSprite = true;

        // Smooth Texture Animation?
        if(p->blendDetailMat)
        {
            // If fog is active, inter=0 is accepted as well. Otherwise flickering
            // may occur if the rendering passes don't match for blended and
            // unblended surfaces.
            if(smoothTexAnim && numTexUnits > 1 &&
               p->blendDetailMat->current != p->blendDetailMat->next &&
               !(!usingFog && p->blendDetailMat->inter < 0))
            {
                interPos = p->blendDetailMat->inter;

                // Prepare the inter texture.
                R_MaterialPrepare(p->blendMat->next, 0, &interTex, NULL, NULL);

                // Prepare the inter detail texture.
                R_MaterialPrepare(p->blendDetailMat->next, 0, NULL,
                                  &interDetailTex, NULL);
            }
        }
    }

    // Dynamic lights?
    if(p->type != RPT_SKY_MASK)
    {
        // In multiplicative mode, glowing surfaces are fullbright.
        // Rendering lights on them would be pointless.
        if(!isGlowing)
            useLights = (p->lightListIdx? true : false);
    }

    rcolors = R_AllocRendColors(realNumVertices);
    rtexcoords = R_AllocRendTexCoords(realNumVertices);
    if(interTex.id)
        rtexcoords2 = R_AllocRendTexCoords(realNumVertices);

    // If multitexturing is enabled and there is at least one dynlight
    // affecting this surface, grab the paramaters needed to draw it.
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

    if(p->isWall)
    {
        // Primary texture coordinates.
        quadTexCoords(rtexcoords, rvertices, *p->segLength, p->texOrigin);

        // Blend texture coordinates.
        if(interTex.id)
            quadTexCoords(rtexcoords2, rvertices, *p->segLength, p->texOrigin);

        // Shiny texture coordinates.
        if(shinyTex.id)
            quadShinyTexCoords(shinyTexCoords, &rvertices[1], &rvertices[2],
                               *p->segLength);

        // First light texture coordinates.
        if(numLights > 0 && RL_IsMTexLights())
            quadLightCoords(rtexcoords5, modTexTC[0], modTexTC[1]);
    }
    else
    {
        uint                i;

        for(i = 0; i < numVertices; ++i)
        {
            const rvertex_t*    vtx = &rvertices[i];
            float               xyz[3];

            xyz[VX] = vtx->pos[VX] - p->texOrigin[0][VX];
            xyz[VY] = vtx->pos[VY] - p->texOrigin[0][VY];
            xyz[VZ] = vtx->pos[VZ] - p->texOrigin[0][VZ];

            // Primary texture coordinates.
            if(tex.id)
            {
                rtexcoord_t*        tc = &rtexcoords[i];

                tc->st[0] =  xyz[VX];
                tc->st[1] = -xyz[VY];
            }

            // Blend primary texture coordinates.
            if(interTex.id)
            {
                rtexcoord_t*        tc = &rtexcoords2[i];

                tc->st[0] =  xyz[VX];
                tc->st[1] = -xyz[VY];
            }

            // Shiny texture coordinates.
            if(shinyTex.id)
            {
                flatShinyTexCoords(&shinyTexCoords[i], vtx->pos);
            }

            // First light texture coordinates.
            if(numLights > 0 && RL_IsMTexLights())
            {
                rtexcoord_t*        tc = &rtexcoords5[i];
                float               width, height;

                width  = p->texOrigin[1][VX] - p->texOrigin[0][VX];
                height = p->texOrigin[1][VY] - p->texOrigin[0][VY];

                tc->st[0] = ((p->texOrigin[1][VX] - vtx->pos[VX]) / width * modTexTC[0][0]) +
                    (xyz[VX] / width * modTexTC[0][1]);
                tc->st[1] = ((p->texOrigin[1][VY] - vtx->pos[VY]) / height * modTexTC[1][0]) +
                    (xyz[VY] / height * modTexTC[1][1]);
            }
        }
    }

    // Light this polygon.
    if(p->type != RPT_SKY_MASK)
    {
        if(isGlowing || levelFullBright)
        {   // Uniform colour. Apply to all vertices.
            Rend_VertexColorsGlow(rcolors, numVertices);
        }
        else
        {   // Non-uniform color.
            if(useBias)
            {
                // Do BIAS lighting for this poly.
                SB_RendPoly(rvertices, rcolors, numVertices,
                            p->normal, *p->sectorLightLevel,
                            p->tracker, p->affection,
                            p->mapObject, p->elmIdx, p->isWall);
            }
            else
            {
                uint                i;
                float               ll = *p->sectorLightLevel;

                if(p->isWall)
                    ll += p->wallAngleLightLevelDelta;

                // Calculate the color for each vertex, blended with plane color?
                if(p->surfaceColor[0] < 1 || p->surfaceColor[1] < 1 ||
                   p->surfaceColor[2] < 1)
                {
                    float               vColor[4];

                    // Blend sector light+color+surfacecolor
                    vColor[CR] = p->surfaceColor[CR] * p->sectorLightColor[CR];
                    vColor[CG] = p->surfaceColor[CG] * p->sectorLightColor[CG];
                    vColor[CB] = p->surfaceColor[CB] * p->sectorLightColor[CB];
                    vColor[CA] = 1;

                    for(i = 0; i < numVertices; ++i)
                        lightVertex(&rcolors[i], &rvertices[i], ll, vColor);
                }
                else
                {
                    // Use sector light+color only
                    for(i = 0; i < numVertices; ++i)
                        lightVertex(&rcolors[i], &rvertices[i], ll,
                                    p->sectorLightColor);
                }

                // Bottom color (if different from top)?
                if(p->isWall && p->surfaceColor2)
                {
                    float               vColor[4];

                    // Blend sector light+color+surfacecolor
                    vColor[CR] = p->surfaceColor2[CR] * p->sectorLightColor[CR];
                    vColor[CG] = p->surfaceColor2[CG] * p->sectorLightColor[CG];
                    vColor[CB] = p->surfaceColor2[CB] * p->sectorLightColor[CB];
                    vColor[CA] = 1;

                    lightVertex(&rcolors[0], &rvertices[0], ll, vColor);
                    lightVertex(&rcolors[2], &rvertices[2], ll, vColor);
                }
            }

            Rend_VertexColorsApplyTorchLight(rcolors, rvertices, numVertices);
        }

        if(p->shinyMat)
        {
            uint                i;

            // Strength of the shine.
            for(i = 0; i < numVertices; ++i)
            {
                shinyColors[i].rgba[CR] =
                    MAX_OF(rcolors[i].rgba[CR], p->shinyMat->shiny.minColor[CR]);
                shinyColors[i].rgba[CG] =
                    MAX_OF(rcolors[i].rgba[CG], p->shinyMat->shiny.minColor[CG]);
                shinyColors[i].rgba[CB] =
                    MAX_OF(rcolors[i].rgba[CB], p->shinyMat->shiny.minColor[CB]);
                shinyColors[i].rgba[CA] = p->shinyMat->shiny.shininess;
            }
        }

        // Apply uniform alpha.
        Rend_VertexColorsAlpha(rcolors, numVertices, p->alpha);
    }
    else
    {
        memset(rcolors, 0, sizeof(*rcolors) * numVertices);
    }

    if(IS_MUL && useLights)
    {
        // Surfaces lit by dynamic lights may need to be rendered
        // differently than non-lit surfaces.
        uint                i;
        float               avglightlevel = 0;

        // Determine the average light level of this rend poly,
        // if too bright; do not bother with lights.
        for(i = 0; i < numVertices; ++i)
        {
            avglightlevel += rcolors[i].rgba[CR];
            avglightlevel += rcolors[i].rgba[CG];
            avglightlevel += rcolors[i].rgba[CB];
        }
        avglightlevel /= (float) numVertices * 3;

        if(avglightlevel > 0.98f)
        {
            useLights = false;
        }
    }

    // Setup the texturemap units.
    memset(pTU, 0, sizeof(pTU));
    if(p->type != RPT_SKY_MASK)
    {
        pTU[TU_PRIMARY].tex = tex.id;
        pTU[TU_PRIMARY].magMode = tex.magMode;
        pTU[TU_PRIMARY].scale[0] = (p->texFlipH? -1.f : 1.f) / p->mat->width;
        pTU[TU_PRIMARY].scale[1] = (p->texFlipV? -1.f : 1.f) / p->mat->height;
        pTU[TU_PRIMARY].offset[0] = p->texOffset[0] / p->mat->width;
        pTU[TU_PRIMARY].offset[1] =
            (p->isWall? p->texOffset[1] : -p->texOffset[1]) / p->mat->height;
        pTU[TU_PRIMARY].blend = 1;

        pTU[TU_PRIMARY_DETAIL].tex = detailTex.id;
        pTU[TU_PRIMARY_DETAIL].magMode = detailTex.magMode;
        pTU[TU_PRIMARY_DETAIL].blend = 1;
        if(detailTex.id)
        {
            float               scale = 1.f / detailTex.width *
                (detailTex.scale * detailScale);

            pTU[TU_PRIMARY_DETAIL].scale[0] =
                pTU[TU_PRIMARY_DETAIL].scale[1] = scale;

            pTU[TU_PRIMARY_DETAIL].offset[0] = p->texOffset[0] * scale;
            pTU[TU_PRIMARY_DETAIL].offset[1] =
                (p->isWall? p->texOffset[1] : -p->texOffset[1]) * scale;
        }

        pTU[TU_INTER].tex = interTex.id;
        pTU[TU_INTER].magMode = interTex.magMode;
        pTU[TU_INTER].blend = interPos;
        if(interTex.id)
        {
            pTU[TU_INTER].scale[0] = (p->texFlipH? -1.f : 1.f) / p->blendMat->width;
            pTU[TU_INTER].scale[1] = (p->texFlipV? -1.f : 1.f) / p->blendMat->height;
            pTU[TU_INTER].offset[0] = p->texOffset[0] / p->blendMat->width;
            pTU[TU_INTER].offset[1] =
                (p->isWall? p->texOffset[1] : -p->texOffset[1]) / p->blendMat->height;
        }

        pTU[TU_INTER_DETAIL].tex = interDetailTex.id;
        pTU[TU_INTER_DETAIL].magMode = interDetailTex.magMode;
        pTU[TU_INTER_DETAIL].blend = interPos;
        if(interDetailTex.id)
        {
            float               scale = 1.f / interDetailTex.width *
                interDetailTex.scale * detailScale;

            pTU[TU_INTER_DETAIL].scale[0] =
                pTU[TU_INTER_DETAIL].scale[1] = scale;

            pTU[TU_INTER_DETAIL].offset[0] = p->texOffset[0] * scale;
            pTU[TU_INTER_DETAIL].offset[1] =
                (p->isWall? p->texOffset[1] : -p->texOffset[1]) * scale;
        }

        pTU[TU_SHINY].tex = shinyTex.id;
        pTU[TU_SHINY].magMode = shinyTex.magMode;
        pTU[TU_SHINY].blend = 1;
        if(shinyTex.id)
        {
            pTU[TU_SHINY].blendMode = p->shinyMat->shiny.blendMode;

            pTU[TU_SHINY_MASK].tex = shinyMaskTex.id;
            pTU[TU_SHINY_MASK].magMode = shinyMaskTex.magMode;
            pTU[TU_SHINY_MASK].scale[0] = p->texFlipH? -1 : 1;
            pTU[TU_SHINY_MASK].scale[1] = p->texFlipV? -1 : 1;
            pTU[TU_SHINY_MASK].blend = 1;
        }
    }

    if(drawAsVisSprite)
    {
        assert(p->isWall);

        // Masked polys (walls) get a special treatment (=> vissprite).
        // This is needed because all masked polys must be sorted (sprites
        // are masked polys). Otherwise there will be artifacts.
        Rend_AddMaskedPoly(rvertices, rcolors, *p->segLength,
                           tex.width, tex.height, p->texOffset,
                           p->blendMode, p->lightListIdx, isGlowing,
                           (tex.flags & GLTXF_MASKED)? true : false, pTU);
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

        return false;
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
        dlparams.texOrigin[0][VX] = p->texOrigin[0][VX];
        dlparams.texOrigin[0][VY] = p->texOrigin[0][VY];
        dlparams.texOrigin[0][VZ] = p->texOrigin[0][VZ];
        dlparams.texOrigin[1][VX] = p->texOrigin[1][VX];
        dlparams.texOrigin[1][VY] = p->texOrigin[1][VY];
        dlparams.texOrigin[1][VZ] = p->texOrigin[1][VZ];

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
                   shinyTexCoords? shinyTexCoords + 3 + divs[0].num : NULL,
                   rtexcoords + 3 + divs[0].num,
                   rcolors + 3 + divs[0].num,
                   shinyColors + 3 + divs[0].num,
                   3 + divs[1].num,
                   numLights,
                   modTex, modColor, pTU);
        RL_AddPoly(PT_FAN, p->type, rvertices, rtexcoords, rtexcoords2, rtexcoords5,
                   shinyTexCoords, rtexcoords,
                   rcolors, shinyColors,
                   3 + divs[0].num,
                   numLights,
                   modTex, modColor, pTU);
    }
    else
    {
        RL_AddPoly(p->isWall? PT_TRIANGLE_STRIP : PT_FAN, p->type, rvertices,
                   rtexcoords, rtexcoords2, rtexcoords5,
                   shinyTexCoords, rtexcoords,
                   rcolors, shinyColors,
                   numVertices,
                   numLights,
                   modTex, modColor, pTU);
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

    return !(p->alpha < 1 || (tex.flags & GLTXF_MASKED) || p->blendMode > 0);
}

#define RPF2_SHINY  0x0001
#define RPF2_GLOW   0x0002
#define RPF2_BLEND  0x0004

static boolean doRenderSeg(subsector_t* ssec, seg_t* seg, segsection_t section,
                           surface_t* surface, float bottom, float top,
                           float alpha, float texXOffset, float texYOffset,
                           sector_t* frontsec, boolean skyMask,
                           short sideFlags)
{
    int                 surfaceFlags = 0, surfaceInFlags = 0;
    rendworldpoly_params_t params;
    walldiv_t           divs[2];
    short               tempflags = 0;
    boolean             isTwoSided = (seg->lineDef &&
        seg->lineDef->L_frontside && seg->lineDef->L_backside)? true:false;
    boolean             hasDivisions = false;

    // Init the params.
    memset(&params, 0, sizeof(params));

    params.isWall = true;
    params.segLength = &seg->length;
    params.alpha = alpha;
    params.mapObject = seg;
    params.elmIdx = (uint) section;
    params.tracker = &seg->tracker[section];
    params.affection = seg->affected;

    params.texOffset[VX] = texXOffset;
    params.texOffset[VY] = texYOffset;

    // Fill in the remaining params data.
    if(skyMask || R_IsSkySurface(surface))
    {
        // In devSkyMode mode we render all polys destined for the skymask
        // as regular world polys (with a few obvious properties).
        if(devSkyMode)
        {
            params.mat = surface->material;
            params.type = RPT_NORMAL;
            params.forceOpaque = true;
            tempflags |= RPF2_GLOW;
        }
        else
        {   // We'll mask this.
            params.mat = NULL;
            params.type = RPT_SKY_MASK;
        }
    }
    else
    {
        int                 texMode = 0;
        material_t*         mat;

        // Determine which texture to use.
        if(renderTextures == 2)
            texMode = 2;
        else if(!surface->material ||
                ((surface->inFlags & SUIF_MATERIAL_FIX) && devNoTexFix))
            texMode = 1;
        else
            texMode = 0;

        if(texMode == 0)
            mat = surface->material;
        else if(texMode == 1)
            // For debug, render the "missing" texture instead of the texture
            // chosen for surfaces to fix the HOMs.
            mat = R_GetMaterial(DDT_MISSING, MG_DDTEXTURES);
        else // texMode == 2
            // For lighting debug, render all solid surfaces using the gray
            // texture.
            mat = R_GetMaterial(DDT_GRAY, MG_DDTEXTURES);

        params.mat = mat;
        params.detailMat =
            (surface->material? surface->material : NULL);

        // Make any necessary adjustments to the surface flags to suit the
        // current texture mode.
        surfaceFlags = surface->flags;
        surfaceInFlags = surface->inFlags;
        if(texMode == 0)
        {
            tempflags |= RPF2_BLEND;
        }
        else if(texMode == 1)
        {
            tempflags |= RPF2_GLOW; // Make it stand out
        }
        else // texMode == 2
        {
            tempflags |= RPF2_BLEND;
            surfaceInFlags &= ~(SUIF_MATERIAL_FIX);
        }

        // Smooth Texture Animation?
        if(tempflags & RPF2_BLEND)
        {
            params.blendMat = mat;
            params.blendDetailMat =
                (surface->material? surface->material : NULL);
        }

        if(section != SEG_MIDDLE || (section == SEG_MIDDLE && !isTwoSided))
        {
            params.alpha = 1;
            params.forceOpaque = true;
            params.blendMode = BM_NORMAL;
        }
        else
        {
            params.forceOpaque = false;

            if(surface->blendMode == BM_NORMAL && noSpriteTrans)
                params.blendMode = BM_ZEROALPHA; // "no translucency" mode
            else
                params.blendMode = surface->blendMode;
        }

        if(mat)
            tempflags |= RPF2_SHINY;
        if(glowingTextures &&
           ((surfaceFlags & DDSUF_GLOW) ||
           (surface->material && (surface->material->flags & MATF_GLOW))))
            tempflags |= RPF2_GLOW;
    }

    /**
     * If this poly is destined for the skymask, we don't need to
     * do any further processing.
     */
    if(params.type != RPT_SKY_MASK)
    {
        params.normal = surface->normal;
        params.sectorLightLevel = &frontsec->lightLevel;
        params.sectorLightColor = R_GetSectorLightColor(frontsec);
        params.wallAngleLightLevelDelta =
            R_WallAngleLightLevelDelta(seg->lineDef, seg->side);

        if(!(surfaceInFlags & SUIF_NO_RADIO))
            Rend_RadioUpdateLinedef(seg->lineDef, seg->side);

        selectSurfaceColors(&params.surfaceColor, &params.surfaceColor2,
                            SEG_SIDEDEF(seg), section);

        // Dynamic lights.
        if(!(tempflags & RPF2_GLOW))
            params.lightListIdx =
                DL_ProcessSegSection(seg, ssec, bottom, top,
                                     (section == SEG_MIDDLE && isTwoSided));

        // Check for neighborhood division?
        if(!(section == SEG_MIDDLE && isTwoSided) &&
           !(seg->lineDef->inFlags & LF_POLYOBJ))
        {
            applyWallHeightDivision(divs, seg, frontsec, bottom, top);

            if(divs[0].num > 0 || divs[1].num > 0)
                hasDivisions = true;
        }

        // Render Shiny polys for this seg?
        if(useShinySurfaces && (tempflags & RPF2_SHINY) &&
           surface->material && surface->material->current->shiny.tex)
        {
            params.shinyMat = surface->material->current;
        }
    }

    // Make it fullbright?
    if((tempflags & RPF2_GLOW))
        params.glowing = true;

    {
    rvertex_t*          rvertices;

    if(hasDivisions)
    {   // Allocate enough vertices for the divisions too.
        rvertices = R_AllocRendVertices(3 + divs[0].num + 3 + divs[1].num);
    }
    else
    {
        rvertices = R_AllocRendVertices(4);
    }

    // Bottom Left.
    rvertices[0].pos[VX] = seg->SG_v1pos[VX];
    rvertices[0].pos[VY] = seg->SG_v1pos[VY];
    rvertices[0].pos[VZ] = bottom;

    // Top Left.
    rvertices[1].pos[VX] = seg->SG_v1pos[VX];
    rvertices[1].pos[VY] = seg->SG_v1pos[VY];
    rvertices[1].pos[VZ] = top;

    // Bottom Right.
    rvertices[2].pos[VX] = seg->SG_v2pos[VX];
    rvertices[2].pos[VY] = seg->SG_v2pos[VY];
    rvertices[2].pos[VZ] = bottom;

    // Top Right.
    rvertices[3].pos[VX] = seg->SG_v2pos[VX];
    rvertices[3].pos[VY] = seg->SG_v2pos[VY];
    rvertices[3].pos[VZ] = top;

    // Top left.
    params.texOrigin[0][VX] = seg->SG_v1pos[VX];
    params.texOrigin[0][VY] = seg->SG_v1pos[VY];
    params.texOrigin[0][VZ] = top;

    // Bottom right.
    params.texOrigin[1][VX] = seg->SG_v2pos[VX];
    params.texOrigin[1][VY] = seg->SG_v2pos[VY];
    params.texOrigin[1][VZ] = bottom;

    params.texFlipH = (surface->flags & DDSUF_MATERIAL_FLIPH)? true : false;
    params.texFlipV = (surface->flags & DDSUF_MATERIAL_FLIPV)? true : false;

    // Draw this seg.
    if(renderWorldPoly(rvertices, 4, hasDivisions? divs : NULL, &params))
    {   // An entirely opaque seg was drawn.
        // Render Fakeradio polys for this seg?
        if(params.type != RPT_SKY_MASK && !(tempflags & RPF2_GLOW) &&
           !(surfaceInFlags & SUIF_NO_RADIO))
        {
            rendsegradio_params_t radioParams;

            radioParams.sectorLightLevel = &frontsec->lightLevel;
            radioParams.linedefLength = &seg->lineDef->length;
            radioParams.botCn = SEG_SIDEDEF(seg)->bottomCorners;
            radioParams.topCn = SEG_SIDEDEF(seg)->topCorners;
            radioParams.sideCn = SEG_SIDEDEF(seg)->sideCorners;
            radioParams.spans = SEG_SIDEDEF(seg)->spans;
            radioParams.segOffset = &seg->offset;
            radioParams.segLength = &seg->length;
            radioParams.frontSec = seg->SG_frontsector;
            radioParams.backSec = seg->SG_backsector;

            /**
             * \kludge Revert the vertex coords as they may have been changed
             * due to height divisions.
             */

            // Bottom Left.
            rvertices[0].pos[VX] = seg->SG_v1pos[VX];
            rvertices[0].pos[VY] = seg->SG_v1pos[VY];
            rvertices[0].pos[VZ] = bottom;

            // Top Left.
            rvertices[1].pos[VX] = seg->SG_v1pos[VX];
            rvertices[1].pos[VY] = seg->SG_v1pos[VY];
            rvertices[1].pos[VZ] = top;

            // Bottom Right.
            rvertices[2].pos[VX] = seg->SG_v2pos[VX];
            rvertices[2].pos[VY] = seg->SG_v2pos[VY];
            rvertices[2].pos[VZ] = bottom;

            // Top Right.
            rvertices[3].pos[VX] = seg->SG_v2pos[VX];
            rvertices[3].pos[VY] = seg->SG_v2pos[VY];
            rvertices[3].pos[VZ] = top;

            Rend_RadioSegSection(rvertices, (hasDivisions? divs:NULL),
                                 &radioParams);
        }

        R_FreeRendVertices(rvertices);
        return true; // Clip with this solid seg.
    }

    R_FreeRendVertices(rvertices);
    }

    return false; // Do not clip with this.
}

static void Rend_RenderPlane(subsector_t* subsector, uint planeID)
{
    sector_t*           sector = subsector->sector;
    float               height;
    surface_t*          surface;
    sector_t*           polySector;
    float               vec[3];
    plane_t*            plane;

    polySector = subsector->sector;
    surface = &polySector->planes[planeID]->surface;

    // Must have a visible surface.
    if(surface->material && (surface->material->flags & MATF_NO_DRAW))
        return;

    plane = polySector->planes[planeID];

    // We don't render planes for unclosed sectors when the polys would
    // be added to the skymask (a DOOM.EXE renderer hack).
    if((sector->flags & SECF_UNCLOSED) && R_IsSkySurface(surface))
        return;

    // Determine plane height.
    height = polySector->SP_planevisheight(planeID);
    // Add the skyfix.
    if(plane->type != PLN_MID)
        height += polySector->S_skyfix(plane->type).offset;

    vec[VX] = vx - subsector->midPoint.pos[VX];
    vec[VY] = vz - subsector->midPoint.pos[VY];
    vec[VZ] = vy - height;

    // Don't bother with planes facing away from the camera.
    if(!(M_DotProduct(vec, surface->normal) < 0))
    {
        short               tempflags = 0;
        rendworldpoly_params_t params;
        uint                numVertices = subsector->numVertices;
        rvertex_t*          rvertices;

        memset(&params, 0, sizeof(params));

        params.isWall = false;
        params.blendMode = BM_NORMAL;
        params.mapObject = subsector;
        params.elmIdx = plane->planeID;

        // Set the texture origin.
        params.texOrigin[0][VX] = subsector->bBox[0].pos[VX];
        params.texOrigin[1][VX] = subsector->bBox[1].pos[VX];
        if(plane->type == PLN_FLOOR)
        {
            params.texOrigin[0][VY] = subsector->bBox[1].pos[VY];
            params.texOrigin[1][VY] = subsector->bBox[0].pos[VY];
        }
        else
        {   // Y is flipped for the ceiling.
            params.texOrigin[0][VY] = subsector->bBox[0].pos[VY];
            params.texOrigin[1][VY] = subsector->bBox[1].pos[VY];
        }
        params.texOrigin[0][VZ] = params.texOrigin[1][VZ] = height;

        params.texOffset[VX] =
            sector->SP_plane(plane->planeID)->PS_visoffset[VX];
        params.texOffset[VY] =
            sector->SP_plane(plane->planeID)->PS_visoffset[VY];

        // Add the Y offset to orient the Y flipped texture.
        if(plane->type == PLN_CEILING)
            params.texOffset[VY] -= subsector->bBox[1].pos[VY] -
               subsector->bBox[0].pos[VY];

        // Add the additional offset to align with the worldwide grid.
        params.texOffset[VX] += subsector->worldGridOffset[VX];
        params.texOffset[VY] += subsector->worldGridOffset[VY];

        params.texFlipH = (surface->flags & DDSUF_MATERIAL_FLIPH)? true : false;
        params.texFlipV = (surface->flags & DDSUF_MATERIAL_FLIPV)? true : false;

        // Check for sky.
        if(R_IsSkySurface(surface))
        {
            skyhemispheres |=
                (plane->type == PLN_FLOOR? SKYHEMI_LOWER : SKYHEMI_UPPER);

            // In devSkyMode mode we render all polys destined for the
            // skymask as regular world polys (with a few obvious properties).
            if(devSkyMode)
            {
                params.mat = surface->material;
                params.type = RPT_NORMAL;
                params.forceOpaque = true;
                tempflags |= RPF2_GLOW;
            }
            else
            {   // We'll mask this.
                params.mat = NULL;
                params.type = RPT_SKY_MASK;
            }
        }
        else
        {
            int                 surfaceFlags, surfaceInFlags = 0, texMode;
            material_t*         mat;

            params.type = RPT_NORMAL;

            if(renderTextures == 2)
                texMode = 2;
            else if(!surface->material ||
                    ((surface->inFlags & SUIF_MATERIAL_FIX) && devNoTexFix))
                texMode = 1;
            else
                texMode = 0;

            if(texMode == 0)
                mat = surface->material;
            else if(texMode == 1)
                // For debug, render the "missing" texture instead of the texture
                // chosen for surfaces to fix the HOMs.
                mat = R_GetMaterial(DDT_MISSING, MG_DDTEXTURES);
            else
                // For lighting debug, render all solid surfaces using the gray texture.
                mat = R_GetMaterial(DDT_GRAY, MG_DDTEXTURES);

            params.mat = mat;
            params.detailMat =
                (surface->material? surface->material : NULL);

            // Make any necessary adjustments to the surface flags to suit the
            // current texture mode.
            surfaceFlags = surface->flags;
            surfaceInFlags = surface->inFlags;
            if(texMode == 0)
            {
                tempflags |= RPF2_BLEND;
            }
            else if(texMode == 1)
            {
                tempflags |= RPF2_GLOW; // Make it stand out
            }
            else // texMode == 2
            {
                tempflags |= RPF2_BLEND;
                surfaceInFlags &= ~(SUIF_MATERIAL_FIX);
            }

            // Smooth Texture Animation?
            if(tempflags & RPF2_BLEND)
            {
                params.blendMat = mat;
                params.blendDetailMat =
                    (surface->material? surface->material : NULL);
            }

            if(plane->type == PLN_FLOOR || plane->type == PLN_CEILING)
            {
                params.alpha = 1;
                params.forceOpaque = true;
                params.blendMode = BM_NORMAL;
            }
            else
            {
                params.forceOpaque = false;

                if(surface->blendMode == BM_NORMAL && noSpriteTrans)
                    params.blendMode = BM_ZEROALPHA; // "no translucency" mode
                else
                    params.blendMode = surface->blendMode;
            }

            if(mat)
                tempflags |= RPF2_SHINY;
            if(glowingTextures &&
               ((surfaceFlags & DDSUF_GLOW) ||
                 (surface->material && (surface->material->flags & MATF_GLOW))))
                tempflags |= RPF2_GLOW;
        }

        /**
         * If this poly is destined for the skymask, we don't need to
         * do any further processing.
         */
        if(params.type != RPT_SKY_MASK)
        {
            // Dynamic lights.
            if(!(tempflags & RPF2_GLOW))
                params.lightListIdx =
                    DL_ProcessSubSectorPlane(subsector, plane->planeID);

            // Render Shiny polys?
            if(useShinySurfaces && (tempflags & RPF2_SHINY) &&
               surface->material && surface->material->current->shiny.tex)
            {
                params.shinyMat = surface->material->current;
            }
        }

        // Make it fullbright?
        if((tempflags & RPF2_GLOW))
            params.glowing = true;

        params.normal = surface->normal;
        params.sectorLightLevel = &polySector->lightLevel;
        params.alpha = 1;/*surface->rgba[CA];*/

        {
        subplaneinfo_t*     subPln =
            &plane->subPlanes[subsector->inSectorID];

        params.tracker = &subPln->tracker;
        params.affection = subPln->affected;
        }

        params.sectorLightColor = R_GetSectorLightColor(polySector);
        params.surfaceColor = surface->rgba;

        rvertices = R_AllocRendVertices(numVertices);
        Rend_PreparePlane(rvertices, numVertices, height, subsector,
                          !(surface->normal[VZ] > 0));

        renderWorldPoly(rvertices, numVertices, NULL, &params);

        R_FreeRendVertices(rvertices);
    }
}

static boolean renderSegSection(subsector_t* ssec, seg_t* seg,
                                segsection_t section, surface_t* surface,
                                float bottom, float top,
                                float texXOffset, float texYOffset,
                                sector_t* frontsec, boolean softSurface,
                                boolean canMask, short sideFlags)
{
    boolean             visible = false, skyMask = false;
    boolean             solidSeg = true;

    if(bottom >= top)
        return true;

    if(!(surface->material && (surface->material->flags & MATF_NO_DRAW)))
    {
        visible = true;
    }
    else if(canMask)
    {   // Perhaps add this section to the sky mask?
        if(R_IsSkySurface(&frontsec->SP_ceilsurface) &&
           R_IsSkySurface(&frontsec->SP_floorsurface))
        {
           visible = skyMask = true;
        }
    }
    else
    {
        solidSeg = false;
    }

    // Is there a visible surface?
    if(visible)
    {
        float               alpha;

        alpha = (section == SEG_MIDDLE? surface->rgba[3] : 1.0f);

        if(section == SEG_MIDDLE && softSurface)
        {
            mobj_t*             mo = viewPlayer->shared.mo;

            /**
             * Can the player walk through this surface?
             * If the player is close enough we should NOT add a
             * solid seg otherwise they'd get HOM when they are
             * directly on top of the line (eg passing through an
             * opaque waterfall).
             */

            if(viewZ > bottom && viewZ < top)
            {
                float               delta[2], pos, result[2];
                linedef_t*          lineDef = seg->lineDef;

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
            solidSeg = doRenderSeg(ssec, seg, section, surface, bottom, top,
                                   alpha, texXOffset, texYOffset, frontsec,
                                   skyMask, sideFlags);
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

    solidSeg = true;
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
        float               offset[2];
        surface_t*          surface = &side->SW_middlesurface;

        offset[0] = surface->visOffset[0] + seg->offset;
        offset[1] = surface->visOffset[1];

        if(ldef->flags & DDLF_DONTPEGBOTTOM)
            offset[1] += -(fceil - ffloor);

        renderSegSection(ssec, seg, SEG_MIDDLE, &side->SW_middlesurface,
                         ffloor, fceil, offset[0], offset[1],
                         /*temp >*/ frontsec, /*< temp*/
                         false, false, side->flags);
    }

    if(P_IsInVoid(viewPlayer))
        solidSeg = false;

    return solidSeg;
}

/**
 * Renders wall sections for given two-sided seg.
 */
static boolean Rend_RenderWallSeg(subsector_t* ssec, seg_t* seg)
{
    int                 solidSeg = false;
    sector_t*           backsec;
    sidedef_t*          backsid, *side;
    linedef_t*          ldef;
    float               ffloor, fceil, bfloor, bceil;
    boolean             backSide;
    sector_t*           frontsec;
    int                 pid = viewPlayer - ddPlayers;

    backsid = SEG_SIDEDEF(seg->backSeg);
    side = SEG_SIDEDEF(seg);
    frontsec = side->sector;
    backsec = backsid->sector;
    backSide = seg->side;
    ldef = seg->lineDef;

    if(!ldef->mapped[pid])
    {
        ldef->mapped[pid] = true; // This line is now seen in the map.

        // Send a status report.
        if(gx.HandleMapObjectStatusReport)
            gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED,
                                           GET_LINE_IDX(ldef),
                                           DMU_LINEDEF, &pid);
    }

    if(backsec == frontsec &&
       !side->SW_topmaterial && !side->SW_bottommaterial &&
       !side->SW_middlematerial)
       return false; // Ugh... an obvious wall seg hack. Best take no chances...

    ffloor = ssec->sector->SP_floorvisheight;
    fceil = ssec->sector->SP_ceilvisheight;

    bceil = backsec->SP_ceilvisheight;
    bfloor = backsec->SP_floorvisheight;

    // Create the wall sections.

    // We may need multiple wall sections.
    // Determine which parts of the segment are visible.

    // Middle section.
    if(side->SW_middleinflags & SUIF_PVIS)
    {
        surface_t*          surface = &side->SW_middlesurface;
        float               texOffset[2];
        float               top, bottom, vL_ZBottom, vR_ZBottom, vL_ZTop, vR_ZTop;
        boolean             softSurface = false;
        material_t*         mat;

        if(ldef->L_backsector == ldef->L_frontsector)
        {
            vL_ZBottom = vR_ZBottom = bottom = MIN_OF(bfloor, ffloor);
            vL_ZTop    = vR_ZTop    = top    = MAX_OF(bceil, fceil);
        }
        else
        {
            vL_ZBottom = vR_ZBottom = bottom = MAX_OF(bfloor, ffloor);
            vL_ZTop    = vR_ZTop    = top    = MIN_OF(bceil, fceil);
        }

        mat = surface->material->current;
        if((side->flags & SDF_MIDDLE_STRETCH) ||
           Rend_MidMaterialPos
           (&vL_ZBottom, &vR_ZBottom, &vL_ZTop, &vR_ZTop,
           &texOffset[VY], surface->visOffset[VY], mat->height,
            (ldef->flags & DDLF_DONTPEGBOTTOM)? true : false))
        {
            // Can we make this a soft surface?
            if((viewPlayer->shared.flags & (DDPF_NOCLIP|DDPF_CAMERA)) ||
               !(ldef->flags & DDLF_BLOCKING))
            {
                softSurface = true;
            }

            texOffset[VX] = surface->visOffset[VX] + seg->offset;
            texOffset[VY] = surface->visOffset[VY];

            solidSeg = renderSegSection(ssec, seg, SEG_MIDDLE, surface,
                                        vL_ZBottom, vL_ZTop, texOffset[VX],
                                        texOffset[VY],
                                        /*temp >*/ frontsec, /*< temp*/
                                        softSurface, false, side->flags);

            // Can we make this a solid segment?
            if(!(vL_ZTop >= top && vL_ZBottom <= bottom &&
                 vR_ZTop >= top && vR_ZBottom <= bottom))
                 solidSeg = false;
        }
    }

    // Upper section.
    if(side->SW_topinflags & SUIF_PVIS)
    {
        float               bottom = bceil;
        float               texOffset[2];
        surface_t*          surface = &side->SW_topsurface;

        if(bceil < ffloor)
        {   // Can't go over front ceiling, would induce polygon flaws.
            bottom = ffloor;
        }

        texOffset[VX] = surface->visOffset[VX] + seg->offset;
        texOffset[VY] = surface->visOffset[VY];

        if(!(ldef->flags & DDLF_DONTPEGTOP))
            texOffset[VY] += -(fceil - bceil);  // Align with normal middle texture.

        renderSegSection(ssec, seg, SEG_TOP, &side->SW_topsurface, bottom, fceil,
                         texOffset[VX], texOffset[VY], frontsec, false,
                         false, side->flags);
    }

    // Lower section.
    if(side->SW_bottominflags & SUIF_PVIS)
    {
        float               top = bfloor;
        float               texOffset[2];
        surface_t*          surface = &side->SW_bottomsurface;

        texOffset[VX] = surface->visOffset[VX] + seg->offset;
        texOffset[VY] = surface->visOffset[VY];

        if(bfloor > bceil)
        {   // Can't go over the back ceiling, would induce polygon flaws.
            texOffset[VY] += bfloor - bceil;
            top = bceil;
        }

        if(top > fceil)
        {   // Can't go over front ceiling, would induce polygon flaws.
            texOffset[VY] += top - fceil;
            top = fceil;
        }

        if(ldef->flags & DDLF_DONTPEGBOTTOM)
            texOffset[VY] += (fceil - bfloor); // Align with normal middle texture.

        renderSegSection(ssec, seg, SEG_BOTTOM, &side->SW_bottomsurface,
                         ffloor, top, texOffset[VX], texOffset[VY], frontsec,
                         false, false, side->flags);
    }

    // Can we make this a solid segment in the clipper?
    if(solidSeg == -1)
        return false; // NEVER (we have a hole we couldn't fix).

    if(ldef->L_frontside && ldef->L_backside &&
       ldef->L_frontsector == ldef->L_backsector)
       return false;

    if(!solidSeg) // We'll have to determine whether we can...
    {
        if(backsec == frontsec)
        {
            // An obvious hack, what to do though??
        }
        else if((bceil <= ffloor &&
                    ((side->SW_topmaterial /* && !(side->flags & SDF_MIDTEXUPPER)*/) ||
                     (side->SW_middlematerial))) ||
                (bfloor >= fceil &&
                    (side->SW_bottommaterial || side->SW_middlematerial)))
        {
            // A closed gap.
            solidSeg = true;
        }
        else if((seg->frameFlags & SEGINF_BACKSECSKYFIX) ||
                (!(bceil - bfloor > 0) && bfloor > ffloor && bceil < fceil &&
                (side->SW_topmaterial /*&& !(side->flags & SDF_MIDTEXUPPER)*/) &&
                (side->SW_bottommaterial)))
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
        *ptr++;
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

static void Rend_SSectSkyFixes(subsector_t *ssec)
{
    float               ffloor, fceil, bfloor, bceil, bsh;
    rvertex_t           rvertices[4];
    rcolor_t            rcolors[4];
    rtexcoord_t         rtexcoords[4];
    rtexmapunit_t       pTU[NUM_TEXMAP_UNITS];
    float*              vBL, *vBR, *vTL, *vTR;
    sector_t*           frontsec, *backsec;
    uint                j, num;
    seg_t*              seg, **list;
    sidedef_t*          side;

    // Init the poly.
    memset(pTU, 0, sizeof(pTU));

    if(!devSkyMode)
    {
        pTU[TU_PRIMARY].tex = 0;
        pTU[TU_PRIMARY_DETAIL].tex = 0;
        pTU[TU_INTER].tex = 0;
        pTU[TU_INTER_DETAIL].tex = 0;
    }
    else
    {
        uint                i;

        rtexcoords[0].st[0] = 0;
        rtexcoords[0].st[1] = 1;
        rtexcoords[1].st[0] = 0;
        rtexcoords[1].st[1] = 0;
        rtexcoords[2].st[0] = 1;
        rtexcoords[2].st[1] = 1;
        rtexcoords[3].st[0] = 1;
        rtexcoords[3].st[1] = 0;

        for(i = 0; i < 4; ++i)
            rcolors[i].rgba[CR] = rcolors[i].rgba[CG] =
                rcolors[i].rgba[CB] = rcolors[i].rgba[CA] = 1;
    }

    vBL = rvertices[0].pos;
    vBR = rvertices[2].pos;
    vTL = rvertices[1].pos;
    vTR = rvertices[3].pos;

    num  = ssec->segCount;
    list = ssec->segs;

    for(j = 0; j < num; ++j)
    {
        seg = list[j];

        if(!seg->lineDef) // "minisegs" have no linedefs.
            continue;

        // Let's first check which way this seg is facing.
        if(!(seg->frameFlags & SEGINF_FACINGFRONT))
            continue;

        side = SEG_SIDEDEF(seg);
        if(!side)
            continue;

        backsec = seg->SG_backsector;
        frontsec = seg->SG_frontsector;

        if(backsec == frontsec &&
           !side->SW_topmaterial && !side->SW_bottommaterial &&
           !side->SW_middlematerial)
           continue; // Ugh... an obvious wall seg hack. Best take no chances...

        ffloor = frontsec->SP_floorvisheight;
        fceil = frontsec->SP_ceilvisheight;

        if(backsec)
        {
            bceil = backsec->SP_ceilvisheight;
            bfloor = backsec->SP_floorvisheight;
            bsh = bceil - bfloor;
        }
        else
            bsh = bceil = bfloor = 0;

        // Get the start and end vertices, left then right. Top and bottom.
        vBL[VX] = vTL[VX] = seg->SG_v1pos[VX];
        vBL[VY] = vTL[VY] = seg->SG_v1pos[VY];
        vBR[VX] = vTR[VX] = seg->SG_v2pos[VX];
        vBR[VY] = vTR[VY] = seg->SG_v2pos[VY];

        // Upper/lower normal skyfixes.
        // Floor.
        if(frontsec->skyFix[PLN_FLOOR].offset < 0)
        {
            if(!backsec ||
               (backsec && backsec != seg->SG_frontsector &&
                (bfloor + backsec->skyFix[PLN_FLOOR].offset >
                 ffloor + frontsec->skyFix[PLN_FLOOR].offset)))
            {
                if(devSkyMode)
                {
                    gltexture_t         tex, detailTex;

                    // In devSkyMode mode we render all polys destined for the skymask as
                    // regular world polys (with a few obvious properties).

                    R_MaterialPrepare(frontsec->SP_floormaterial->current, 0, &tex, &detailTex, NULL);
                    pTU[TU_PRIMARY].tex = curTex = tex.id;
                    pTU[TU_PRIMARY].magMode = tex.magMode;

                    pTU[TU_PRIMARY_DETAIL].tex = detailTex.id;
                    pTU[TU_PRIMARY_DETAIL].tex = detailTex.magMode;

                    pTU[TU_INTER].tex = 0;
                    pTU[TU_INTER_DETAIL].tex = 0;
                }

                vTL[VZ] = vTR[VZ] = ffloor;
                vBL[VZ] = vBR[VZ] =
                    ffloor + frontsec->skyFix[PLN_FLOOR].offset;

                RL_AddPoly(PT_TRIANGLE_STRIP,
                           (devSkyMode? RPT_NORMAL : RPT_SKY_MASK),
                           rvertices, rtexcoords, NULL,
                           NULL, NULL, NULL, rcolors, NULL, 4,
                           0, 0, NULL, pTU);
            }
        }

        // Ceiling.
        if(frontsec->skyFix[PLN_CEILING].offset > 0)
        {
            if(!backsec ||
               (backsec && backsec != seg->SG_frontsector &&
                (bceil + backsec->skyFix[PLN_CEILING].offset <
                 fceil + frontsec->skyFix[PLN_CEILING].offset)))
            {
                if(devSkyMode)
                {
                    gltexture_t         tex, detailTex;

                    // In devSkyMode mode we render all polys destined for the skymask as
                    // regular world polys (with a few obvious properties).

                    R_MaterialPrepare(frontsec->SP_ceilmaterial->current, 0, &tex, &detailTex, NULL);
                    pTU[TU_PRIMARY].tex = curTex = tex.id;
                    pTU[TU_PRIMARY].magMode = tex.magMode;

                    pTU[TU_PRIMARY_DETAIL].tex = detailTex.id;
                    pTU[TU_PRIMARY_DETAIL].tex = detailTex.magMode;

                    pTU[TU_INTER].tex = 0;
                    pTU[TU_INTER_DETAIL].tex = 0;
                }

                vTL[VZ] = vTR[VZ] =
                    fceil + frontsec->skyFix[PLN_CEILING].offset;
                vBL[VZ] = vBR[VZ] = fceil;

                RL_AddPoly(PT_TRIANGLE_STRIP,
                           (devSkyMode? RPT_NORMAL : RPT_SKY_MASK),
                           rvertices, rtexcoords, NULL,
                           NULL, NULL, NULL, rcolors, NULL, 4,
                           0, 0, NULL, pTU);
            }
        }

        // Upper/lower zero height backsec skyfixes.
        if(backsec && bsh <= 0)
        {
            // Floor.
            if(R_IsSkySurface(&frontsec->SP_floorsurface) &&
               R_IsSkySurface(&backsec->SP_floorsurface))
            {
                if(backsec->skyFix[PLN_FLOOR].offset < 0)
                {
                    if(devSkyMode)
                    {
                        gltexture_t         tex, detailTex;

                        // In devSkyMode mode we render all polys destined for the skymask as
                        // regular world polys (with a few obvious properties).

                        R_MaterialPrepare(frontsec->SP_floormaterial->current, 0, &tex, &detailTex, NULL);
                        pTU[TU_PRIMARY].tex = curTex = tex.id;
                        pTU[TU_PRIMARY].magMode = tex.magMode;

                        pTU[TU_PRIMARY_DETAIL].tex = detailTex.id;
                        pTU[TU_PRIMARY_DETAIL].tex = detailTex.magMode;

                        pTU[TU_INTER].tex = 0;
                        pTU[TU_INTER_DETAIL].tex = 0;
                    }

                    vTL[VZ] = vTR[VZ] = bfloor;
                    vBL[VZ] = vBR[VZ] =
                        bfloor + backsec->skyFix[PLN_FLOOR].offset;
                    RL_AddPoly(PT_TRIANGLE_STRIP,
                               (devSkyMode? RPT_NORMAL : RPT_SKY_MASK),
                               rvertices, rtexcoords,
                               NULL, NULL, NULL, NULL, rcolors, NULL,
                               4, 0,
                               0, NULL, pTU);
                }
                // Ensure we add a solid view seg.
                seg->frameFlags |= SEGINF_BACKSECSKYFIX;
            }

            // Ceiling.
            if(R_IsSkySurface(&frontsec->SP_ceilsurface) &&
               R_IsSkySurface(&backsec->SP_ceilsurface))
            {
                if(backsec->skyFix[PLN_CEILING].offset > 0)
                {
                    if(devSkyMode)
                    {
                        gltexture_t         tex, detailTex;

                        // In devSkyMode mode we render all polys destined for the skymask as
                        // regular world polys (with a few obvious properties).

                        R_MaterialPrepare(frontsec->SP_ceilmaterial->current, 0, &tex, &detailTex, NULL);
                        pTU[TU_PRIMARY].tex = curTex = tex.id;
                        pTU[TU_PRIMARY].magMode = tex.magMode;

                        pTU[TU_PRIMARY_DETAIL].tex = detailTex.id;
                        pTU[TU_PRIMARY_DETAIL].tex = detailTex.magMode;

                        pTU[TU_INTER].tex = 0;
                        pTU[TU_INTER_DETAIL].tex = 0;
                    }

                    vTL[VZ] = vTR[VZ] =
                        bceil + backsec->skyFix[PLN_CEILING].offset;
                    vBL[VZ] = vBR[VZ] = bceil;
                    RL_AddPoly(PT_TRIANGLE_STRIP,
                               (devSkyMode? RPT_NORMAL : RPT_SKY_MASK),
                               rvertices, rtexcoords,
                               NULL, NULL, NULL, NULL, rcolors, NULL, 4,
                               0,
                               0, NULL, pTU);
                }
                // Ensure we add a solid view seg.
                seg->frameFlags |= SEGINF_BACKSECSKYFIX;
            }
        }
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
    float               fronth[2], backh[2];
    float*              startv, *endv;
    sector_t*           front = sub->sector, *back;
    seg_t*              seg, **ptr;

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

        *ptr++;
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

    // Retrieve the sector light color.
    sLightColor = R_GetSectorLightColor(sect);

    Rend_MarkSegsFacingFront(ssec);

    R_InitForSubsector(ssec);

    Rend_RadioSubsectorEdges(ssec);

    occludeSubsector(ssec, false);
    LO_ClipInSubsector(ssecidx);
    occludeSubsector(ssec, true);

    if(ssec->polyObj)
    {
        // Polyobjs don't obstruct, do clip lights with another algorithm.
        LO_ClipInSubsectorBySight(ssecidx);
    }

    // Mark the particle generators in the sector visible.
    PG_SectorIsVisible(sect);

    // Sprites for this subsector have to be drawn. This must be done before
    // the segments of this subsector are added to the clipper. Otherwise
    // the sprites would get clipped by them, and that wouldn't be right.
    R_AddSprites(ssec);

    // Draw the various skyfixes for all front facing segs in this ssec
    // (includes polyobject segs).
    if(ssec->sector->planeCount > 0)
    {
        boolean             doSkyFixes;

        doSkyFixes = false;
        i = 0;
        do
        {
            if(R_IsSkySurface(&ssec->sector->SP_planesurface(i)))
                doSkyFixes = true;
            else
                i++;
        } while(!doSkyFixes && i < ssec->sector->planeCount);

        if(doSkyFixes)
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
            boolean             solid;

            if(!seg->SG_backsector || !seg->SG_frontsector)
                solid = Rend_RenderSSWallSeg(ssec, seg);
            else
                solid = Rend_RenderWallSeg(ssec, seg);

            if(solid)
            {
                C_AddViewRelSeg(seg->SG_v1pos[VX], seg->SG_v1pos[VY],
                                seg->SG_v2pos[VX], seg->SG_v2pos[VY]);
            }
        }

        *ptr++;
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
                boolean             solid = Rend_RenderSSWallSeg(ssec, seg);

                if(solid)
                {
                    C_AddViewRelSeg(seg->SG_v1pos[VX], seg->SG_v1pos[VY],
                                    seg->SG_v2pos[VX], seg->SG_v2pos[VY]);
                }
            }
        }
    }

    // Render all planes of this sector.
    for(i = 0; i < sect->planeCount; ++i)
        Rend_RenderPlane(ssec, i);
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
        node_t             *bsp;
        byte                side;

        // Descend deeper into the nodes.
        bsp = NODE_PTR(bspnum);

        // Decide which side the view point is on.
        side = R_PointOnSide(viewX, viewY, &bsp->partition);

        Rend_RenderNode(bsp->children[side]);   // Recursively divide front space.
        Rend_RenderNode(bsp->children[side ^ 1]);   // ...and back space.
    }
}

static void drawNormal(vec3_t origin, vec3_t normal, float scalar)
{
    float               black[4] = { 0, 0, 0, 0 };
    float               red[4] = { 1, 0, 0, 1 };

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(origin[VX], origin[VZ], origin[VY]);

    DGL_Begin(DGL_LINES);
    {
        DGL_Color4fv(black);
        DGL_Vertex3f(scalar * normal[VX],
                     scalar * normal[VZ],
                     scalar * normal[VY]);
        DGL_Color4fv(red);
        DGL_Vertex3f(0, 0, 0);

    }
    DGL_End();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
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

    DGL_Disable(DGL_TEXTURING);
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
            if(seg->SG_backsector->SP_ceilvisheight <
               seg->SG_frontsector->SP_ceilvisheight)
            {
                bottom = seg->SG_backsector->SP_ceilvisheight;
                top = seg->SG_frontsector->SP_ceilvisheight;
                suf = &side->SW_topsurface;

                V3_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawNormal(origin, suf->normal, scale);
            }

            if(side->SW_middlesurface.material)
            {
                top = seg->SG_frontsector->SP_ceilvisheight;
                bottom = seg->SG_frontsector->SP_floorvisheight;
                suf = &side->SW_middlesurface;

                V3_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawNormal(origin, suf->normal, scale);
            }

            if(seg->SG_backsector->SP_floorvisheight >
               seg->SG_frontsector->SP_floorvisheight)
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
            if(pln->type == PLN_FLOOR)
                origin[VZ] += ssec->sector->skyFix[0].offset;
            else if(pln->type == PLN_CEILING)
                origin[VZ] += ssec->sector->skyFix[1].offset;

            drawNormal(origin, pln->PS_normal, scale);
        }
    }

    glEnable(GL_CULL_FACE);
    DGL_Enable(DGL_TEXTURING);

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
    DGL_Begin(DGL_POINTS);
        DGL_Color4f(.7f, .7f, .2f, alpha * 2);
        DGL_Vertex3f(vtx->V_pos[VX], z, vtx->V_pos[VY]);
    DGL_End();
}

static void drawVertexBar(const vertex_t* vtx, float bottom, float top,
                          float alpha)
{
#define EXTEND_DIST     64

    static const float  black[4] = { 0, 0, 0, 0 };

    DGL_Begin(DGL_LINES);
        DGL_Color4fv(black);
        DGL_Vertex3f(vtx->V_pos[VX], bottom - EXTEND_DIST, vtx->V_pos[VY]);
        DGL_Color4f(1, 1, 1, alpha);
        DGL_Vertex3f(vtx->V_pos[VX], bottom, vtx->V_pos[VY]);
        DGL_Vertex3f(vtx->V_pos[VX], bottom, vtx->V_pos[VY]);
        DGL_Vertex3f(vtx->V_pos[VX], top, vtx->V_pos[VY]);
        DGL_Vertex3f(vtx->V_pos[VX], top, vtx->V_pos[VY]);
        DGL_Color4fv(black);
        DGL_Vertex3f(vtx->V_pos[VX], top + EXTEND_DIST, vtx->V_pos[VY]);
    DGL_End();

#undef EXTEND_DIST
}

static void drawVertexIndex(const vertex_t* vtx, float z, float scale,
                            float alpha)
{
    char                buf[80];

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(vtx->V_pos[VX], z, vtx->V_pos[VY]);
    DGL_Rotatef(-vang + 180, 0, 1, 0);
    DGL_Rotatef(vpitch, 1, 0, 0);
    DGL_Scalef(-scale, -scale, 1);

    sprintf(buf, "%i", vtx - vertexes);
    UI_TextOutEx(buf, 2, 2, false, false, UI_Color(UIC_TITLE), alpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
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

            DGL_Disable(DGL_TEXTURING);

            if(devVertexBars)
                drawVertexBar(vtx, bottom, top, MIN_OF(alpha, .15f));

            drawVertexPoint(vtx, bottom, alpha * 2);

            DGL_Enable(DGL_TEXTURING);
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
        DGL_Enable(DGL_LINE_SMOOTH);
        oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
        DGL_SetFloat(DGL_LINE_WIDTH, 2);
        DGL_Disable(DGL_TEXTURING);

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

        DGL_Enable(DGL_TEXTURING);
    }

    // Always draw the vertex point nodes.
    DGL_Enable(DGL_POINT_SMOOTH);
    oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);
    DGL_SetFloat(DGL_POINT_SIZE, 6);
    DGL_Disable(DGL_TEXTURING);

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

    DGL_Enable(DGL_TEXTURING);

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
        DGL_Disable(DGL_LINE_SMOOTH);
    }
    DGL_SetFloat(DGL_POINT_SIZE, oldPointSize);
    DGL_Disable(DGL_POINT_SMOOTH);
    glEnable(GL_DEPTH_TEST);
}

void Rend_RenderMap(void)
{
    binangle_t          viewside;
    boolean             doLums =
        (useDynLights || haloMode || spriteLight || useDecorations);

    // Set to true if dynlights are inited for this frame.
    loInited = false;

    DGL_Enable(DGL_MULTISAMPLE);

    // This is all the clearing we'll do.
    if(firstFrameAfterLoad || freezeRLs || P_IsInVoid(viewPlayer))
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    else
        glClear(GL_DEPTH_BUFFER_BIT);

    // Setup the modelview matrix.
    Rend_ModelViewMatrix(true);

    if(!freezeRLs)
    {
        // Prepare for rendering.
        RL_ClearLists(); // Clear the lists for new quads.
        C_ClearRanges(); // Clear the clipper.
        LO_BeginFrame();

        // Make vissprites of all the visible decorations.
        Rend_ProjectDecorations();

        // Recycle the vlight lists. Currently done here as the lists are
        // not shared by all viewports.
        VL_InitForNewFrame();

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
            float   a = fabs(vpitch) / (90 - yfov / 2);
            binangle_t startAngle =
                (binangle_t) (BANG_45 * fieldOfView / 90) * (1 + a);
            binangle_t angLen = BANG_180 - startAngle;

            viewside = (viewAngle >> (32 - BAMS_BITS)) + startAngle;
            C_SafeAddRange(viewside, viewside + angLen);
            C_SafeAddRange(viewside + angLen, viewside + 2 * angLen);
        }

        // The viewside line for the depth cue.
        viewsidex = -viewSin;
        viewsidey = viewCos;

        // We don't want subsector clipchecking for the first subsector.
        firstsubsector = true;
        Rend_RenderNode(numNodes - 1);

        Rend_RenderShadows();
    }
    RL_RenderAllLists();

    Rend_RenderNormals();

/*#if _DEBUG
LO_DrawLumobjs();
#endif*/

    // Draw the mobj bounding boxes.
    Rend_RenderBoundingBoxes();

    // Draw the vertex position/indice debug aids.
    Rend_Vertexes();

    // Draw the Source Bias Editor's draw that identifies the current light.
    SBE_DrawCursor();

    DGL_Disable(DGL_MULTISAMPLE);
}

/**
 * Updates the lightModRange which is used to applify sector light to help
 * compensate for the differences between the OpenGL lighting equation,
 * the software Doom lighting model and the light grid (ambient lighting).
 *
 * The offsets in the lightRangeModTables are added to the sector->lightLevel
 * during rendering (both positive and negative).
 */
void Rend_CalcLightModRange(cvar_t *unused)
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

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, theWindow->width, theWindow->height, -1, 1);

    DGL_Translatef(BORDER, BORDER, 0);

    color.red = 0.2f;
    color.green = 0;
    color.blue = 0.6f;

    DGL_Disable(DGL_TEXTURING);

    // Draw an outside border.
    DGL_Color4f(1, 1, 0, 1);
    DGL_Begin(DGL_LINES);
    DGL_Vertex2f(-1, -1);
    DGL_Vertex2f(255 + 1, -1);
    DGL_Vertex2f(255 + 1,  -1);
    DGL_Vertex2f(255 + 1,  bHeight + 1);
    DGL_Vertex2f(255 + 1,  bHeight + 1);
    DGL_Vertex2f(-1,  bHeight + 1);
    DGL_Vertex2f(-1,  bHeight + 1);
    DGL_Vertex2f(-1, -1);
    DGL_End();

    DGL_Begin(DGL_QUADS);
    for(i = 0, c = 0; i < 255; ++i, c += (1.0f/255.0f))
    {
        // Get the result of the source light level + offset.
        off = lightModRange[i];

        DGL_Color4f(c + off, c + off, c + off, 1);
        DGL_Vertex2f(i * bWidth, 0);
        DGL_Vertex2f(i * bWidth + bWidth, 0);
        DGL_Vertex2f(i * bWidth + bWidth,  bHeight);
        DGL_Vertex2f(i * bWidth, bHeight);
    }
    DGL_End();

    DGL_Enable(DGL_TEXTURING);

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

#undef bWidth
#undef bHeight
#undef BORDER
}

static DGLuint constructBBox(DGLuint name, float br)
{
    if(DGL_NewList(name, DGL_COMPILE))
    {
        DGL_Begin(DGL_QUADS);
        {
            // Top
            DGL_TexCoord2f(1.0f, 1.0f); DGL_Vertex3f( 1.0f+br, 1.0f,-1.0f-br);  // TR
            DGL_TexCoord2f(0.0f, 1.0f); DGL_Vertex3f(-1.0f-br, 1.0f,-1.0f-br);  // TL
            DGL_TexCoord2f(0.0f, 0.0f); DGL_Vertex3f(-1.0f-br, 1.0f, 1.0f+br);  // BL
            DGL_TexCoord2f(1.0f, 0.0f); DGL_Vertex3f( 1.0f+br, 1.0f, 1.0f+br);  // BR
            // Bottom
            DGL_TexCoord2f(1.0f, 1.0f); DGL_Vertex3f( 1.0f+br,-1.0f, 1.0f+br);  // TR
            DGL_TexCoord2f(0.0f, 1.0f); DGL_Vertex3f(-1.0f-br,-1.0f, 1.0f+br);  // TL
            DGL_TexCoord2f(0.0f, 0.0f); DGL_Vertex3f(-1.0f-br,-1.0f,-1.0f-br);  // BL
            DGL_TexCoord2f(1.0f, 0.0f); DGL_Vertex3f( 1.0f+br,-1.0f,-1.0f-br);  // BR
            // Front
            DGL_TexCoord2f(1.0f, 1.0f); DGL_Vertex3f( 1.0f+br, 1.0f+br, 1.0f);  // TR
            DGL_TexCoord2f(0.0f, 1.0f); DGL_Vertex3f(-1.0f-br, 1.0f+br, 1.0f);  // TL
            DGL_TexCoord2f(0.0f, 0.0f); DGL_Vertex3f(-1.0f-br,-1.0f-br, 1.0f);  // BL
            DGL_TexCoord2f(1.0f, 0.0f); DGL_Vertex3f( 1.0f+br,-1.0f-br, 1.0f);  // BR
            // Back
            DGL_TexCoord2f(1.0f, 1.0f); DGL_Vertex3f( 1.0f+br,-1.0f-br,-1.0f);  // TR
            DGL_TexCoord2f(0.0f, 1.0f); DGL_Vertex3f(-1.0f-br,-1.0f-br,-1.0f);  // TL
            DGL_TexCoord2f(0.0f, 0.0f); DGL_Vertex3f(-1.0f-br, 1.0f+br,-1.0f);  // BL
            DGL_TexCoord2f(1.0f, 0.0f); DGL_Vertex3f( 1.0f+br, 1.0f+br,-1.0f);  // BR
            // Left
            DGL_TexCoord2f(1.0f, 1.0f); DGL_Vertex3f(-1.0f, 1.0f+br, 1.0f+br);  // TR
            DGL_TexCoord2f(0.0f, 1.0f); DGL_Vertex3f(-1.0f, 1.0f+br,-1.0f-br);  // TL
            DGL_TexCoord2f(0.0f, 0.0f); DGL_Vertex3f(-1.0f,-1.0f-br,-1.0f-br);  // BL
            DGL_TexCoord2f(1.0f, 0.0f); DGL_Vertex3f(-1.0f,-1.0f-br, 1.0f+br);  // BR
            // Right
            DGL_TexCoord2f(1.0f, 1.0f); DGL_Vertex3f( 1.0f, 1.0f+br,-1.0f-br);  // TR
            DGL_TexCoord2f(0.0f, 1.0f); DGL_Vertex3f( 1.0f, 1.0f+br, 1.0f+br);  // TL
            DGL_TexCoord2f(0.0f, 0.0f); DGL_Vertex3f( 1.0f,-1.0f-br, 1.0f+br);  // BL
            DGL_TexCoord2f(1.0f, 0.0f); DGL_Vertex3f( 1.0f,-1.0f-br,-1.0f-br);  // BR
        }
        DGL_End();

        return DGL_EndList();
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
void Rend_DrawBBox(const float pos3f[3], float w, float l, float h,
                   const float color3f[3], float alpha, float br,
                   boolean alignToBase)
{
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    if(alignToBase)
        // The Z coordinate is to the bottom of the object.
        DGL_Translatef(pos3f[VX], pos3f[VZ] + h, pos3f[VY]);
    else
        DGL_Translatef(pos3f[VX], pos3f[VZ], pos3f[VY]);

    DGL_Scalef(w - br - br, h - br - br, l - br - br);
    DGL_Color4f(color3f[0], color3f[1], color3f[2], alpha);

    DGL_CallList(dlBBox);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
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
void Rend_DrawArrow(const float pos3f[3], angle_t a, float s,
                    const float color3f[3], float alpha)
{
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(pos3f[VX], pos3f[VZ], pos3f[VY]);

    DGL_Rotatef(0, 0, 0, 1);
    DGL_Rotatef(0, 1, 0, 0);
    DGL_Rotatef((a / (float) ANGLE_MAX *-360), 0, 1, 0);

    DGL_Scalef(s, 0, s);

    DGL_Begin(DGL_TRIANGLES);
    {
        DGL_Color4f(0.0f, 0.0f, 0.0f, 0.5f);
        DGL_TexCoord2f(1.0f, 1.0f); DGL_Vertex3f( 1.0f, 1.0f,-1.0f);  // L

        DGL_Color4f(color3f[0], color3f[1], color3f[2], alpha);
        DGL_TexCoord2f(0.0f, 1.0f); DGL_Vertex3f(-1.0f, 1.0f,-1.0f);  // Point

        DGL_Color4f(0.0f, 0.0f, 0.0f, 0.5f);
        DGL_TexCoord2f(0.0f, 0.0f); DGL_Vertex3f(-1.0f, 1.0f, 1.0f);  // R
    }
    DGL_End();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
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
    float               size, alpha, eye[3];
    mobj_t*             mo;
    sector_t*           sec;
    material_t*         mat;
    gltexture_t         glTex;

    if(!devMobjBBox || netGame)
        return;

    if(!dlBBox)
        dlBBox = constructBBox(0, .08f);

    eye[VX] = vx;
    eye[VY] = vz;
    eye[VZ] = vy;

    if(usingFog)
        DGL_Disable(DGL_FOG);

    glDisable(GL_DEPTH_TEST);
    DGL_Enable(DGL_TEXTURING);
    glDisable(GL_CULL_FACE);

    mat = R_GetMaterial(DDT_BBOX, MG_DDTEXTURES);
    R_MaterialPrepare(mat->current, 0, &glTex, NULL, NULL);
    GL_BindTexture(glTex.id, glTex.magMode);

    GL_BlendMode(BM_ADD);

    // For every sector
    for(i = 0; i < numSectors; ++i)
    {
        sec = SECTOR_PTR(i);

        // Is it vissible?
        if(!(sec->frameFlags & SIF_VISIBLE))
            continue;

        // For every mobj in the sector's mobjList
        for(mo = sec->mobjList; mo; mo = mo->sNext)
        {
            if(mo == ddPlayers[consolePlayer].shared.mo)
                continue; // We don't want the console player.

            alpha = 1 - ((M_Distance(mo->pos, eye)/(theWindow->width/2))/4);

            if(alpha < .25f)
                alpha = .25f; // Don't make them totally invisible.

            // Draw a bounding box in an appropriate color.
            size = mo->radius;
            Rend_DrawBBox(mo->pos, size, size, mo->height/2,
                          (mo->ddFlags & DDMF_MISSILE)? yellow :
                          (mo->ddFlags & DDMF_SOLID)? green : red,
                          alpha, .08f, true);

            Rend_DrawArrow(mo->pos, mo->angle + ANG45 + ANG90 , size*1.25,
                           (mo->ddFlags & DDMF_MISSILE)? yellow :
                           (mo->ddFlags & DDMF_SOLID)? green : red, alpha);
        }
    }

    GL_BlendMode(BM_NORMAL);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    if(usingFog)
        DGL_Enable(DGL_FOG);
}
