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

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int useDynLights, translucentIceCorpse;
extern int skyhemispheres;
extern material_t* skyMaskMaterial;
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

boolean willRenderSprites = true;
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
int devLightModRange = 0;

int devMobjBBox = 0; // 1 = Draw mobj bounding boxes (for debug)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstsubsector; // No range checking for the first one.

// Current sector light color.
const float *sLightColor;

int devNoTexFix = 0;
byte devNoLinkedSurfaces = 0;

// CODE --------------------------------------------------------------------

void Rend_Register(void)
{
    C_VAR_INT("rend-dev-sky", &devSkyMode, 0, 0, 1);
    C_VAR_BYTE("rend-dev-freeze", &freezeRLs, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-cull-subsectors", &devNoCulling,CVF_NO_ARCHIVE,0,1);
    C_VAR_INT("rend-dev-mobj-bbox", &devMobjBBox, 0, 0, 1);
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
    C_VAR_INT("rend-dev-light-mod", &devLightModRange, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-tex-showfix", &devNoTexFix, 0, 0, 1);
    C_VAR_BYTE("rend-dev-surface-linked", &devNoLinkedSurfaces, 0, 0, 1);

    C_VAR_BYTE("rend-dev-blockmap-debug", &bmapShowDebug, 0, 0, 2);
    C_VAR_FLOAT("rend-dev-blockmap-debug-size", &bmapDebugSize, 0, .1f, 100);

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
 * Used to be called before starting a new level.
 */
void Rend_Reset(void)
{
    // Textures are deleted (at least skies need this???).
    GL_ClearRuntimeTextures();
    LO_Clear(); // Free lumobj stuff.
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

typedef struct {
    rendpolytype_t      type;
    short               flags;
    float               width, height;
    float               texOffset[2];
    const float*        segLength;
    const ded_reflection_t* ref;
} rendshinypoly_params_t;

/**
 * \pre As we modify param poly quite a bit it is the responsibility of
 *      the caller to ensure this is OK (ie if not it should pass us a
 *      copy instead (eg wall segs)).
 *
 * @param vertices      The vertices to be used for creating the shiny poly.
 * @param numVertices   Number of vertices in the poly.
 * @param texture       The texture/flat id of the texture on the poly.
 * @param isFlat        @c true = param texture is a flat.
 */
static void Rend_AddShinyPoly(const rvertex_t* rvertices,
                              const rcolor_t* rcolors,
                              uint numVertices, const walldiv_t* divs,
                              const rendshinypoly_params_t *p)
{
    uint                i;
    rendpoly_params_t   params;
    rendpoly_wall_t     wall;
    rcolor_t*           shinyColors;

    // Make it a shiny polygon.
    params.type = p->type;
    params.lightListIdx = 0;
    params.flags = p->flags | RPF_SHINY;
    params.blendMode = p->ref->blendMode;
    params.tex.id = p->ref->useShiny->shinyTex;
    params.tex.magMode = glmode[texMagMode];
    params.tex.detail = NULL;
    params.tex.masked = false;
    params.texOffset[VX] = p->texOffset[VX];
    params.texOffset[VY] = p->texOffset[VY];
    params.interTex.detail = NULL;
    params.interTex.masked = false;
    params.interPos = 0;
    params.wall = &wall;
    if(divs)
        memcpy(params.wall->divs, divs, sizeof(params.wall->divs));
    else
        memset(params.wall->divs, 0, sizeof(params.wall->divs));
    params.wall->length = *p->segLength;

    // We'll reuse the same verts but we need new colors.
    shinyColors = R_AllocRendColors(numVertices);

    // Strength of the shine.
    for(i = 0; i < numVertices; ++i)
    {
        shinyColors[i].rgba[CR] =
            MAX_OF(rcolors[i].rgba[CR], p->ref->minColor[CR]);
        shinyColors[i].rgba[CG] =
            MAX_OF(rcolors[i].rgba[CG], p->ref->minColor[CG]);
        shinyColors[i].rgba[CB] =
            MAX_OF(rcolors[i].rgba[CB], p->ref->minColor[CB]);
        shinyColors[i].rgba[CA] = p->ref->shininess;
    }

    // The mask texture is stored in the intertex.
    if(p->ref->useMask && p->ref->useMask->maskTex)
    {
        params.interTex.id = p->ref->useMask->maskTex;
        params.interTex.magMode = glmode[texMagMode];
        params.tex.width = params.interTex.width =
            p->width * p->ref->maskWidth;
        params.tex.height = params.interTex.height =
            p->height * p->ref->maskHeight;
    }
    else
    {
        // No mask.
        params.interTex.id = 0;
        params.tex.width = p->width;
        params.tex.height = p->height;
    }

    RL_AddPoly(rvertices, shinyColors, numVertices, &params);
    R_FreeRendColors(shinyColors);
}

static float polyTexBlend(gltexture_t* interTex, material_t* mat)
{
    if(!mat)
        return 0;

    // If fog is active, inter=0 is accepted as well. Otherwise flickering
    // may occur if the rendering passes don't match for blended and
    // unblended surfaces.
    if(!smoothTexAnim || numTexUnits < 2 || mat->current == mat->next ||
       (!usingFog && mat->inter < 0) || renderTextures == 2)
    {
        // No blending for you, my friend.
        return 0;
    }

    // Get info of the blend target.
    interTex->id = GL_PrepareMaterial2(mat->next);
    interTex->magMode = glmode[texMagMode];

    interTex->width = mat->width;
    interTex->height = mat->height;
    interTex->detail = (r_detail && mat->detail.tex? &mat->detail : 0);
    interTex->masked = mat->dgl.masked;

    return mat->inter;
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

void Rend_VertexColorsAlpha(rcolor_t* colors, size_t num, short flags,
                            float alpha)
{
    size_t               i;

    // Check for special case exceptions.
    if(flags & (RPF_SKY_MASK|RPF_LIGHT|RPF_SHADOW))
    {
        return;
    }

    for(i = 0; i < num; ++i)
    {
        colors[i].rgba[CA] = alpha;
    }
}

void Rend_ApplyTorchLight(float* color, float distance)
{
    ddplayer_t         *ddpl = &viewPlayer->shared;

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
                                      size_t numVertices, short flags)
{
    size_t              i;
    ddplayer_t         *ddpl = &viewPlayer->shared;

    // Check for special case exceptions.
    if(flags & (RPF_SKY_MASK|RPF_LIGHT|RPF_SHADOW|RPF_GLOW))
    {
        return; // Don't receive light from torches.
    }

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
        sector_t           *front = line->L_sector(backside);
        sector_t           *back  = line->L_sector(backside^1);
        sidedef_t          *side  = line->L_side(backside);

        if(side->SW_middlematerial)
        {
            material_t*         mat = side->SW_middlematerial->current;

            if(!side->SW_middleblendmode && side->SW_middlergba[3] >= 1 &&
               !mat->dgl.masked)
            {
                float               openTop[2], gapTop[2];
                float               openBottom[2], gapBottom[2];

                openTop[0] = openTop[1] = gapTop[0] = gapTop[1] =
                    MIN_OF(back->SP_ceilvisheight, front->SP_ceilvisheight);
                openBottom[0] = openBottom[1] = gapBottom[0] = gapBottom[1] =
                    MAX_OF(back->SP_floorvisheight, front->SP_floorvisheight);

                // Could the mid texture fill enough of this gap for us
                // to consider it completely closed?
                if(mat->height >= (openTop[0] - openBottom[0]) &&
                   mat->height >= (openTop[1] - openBottom[1]))
                {
                    // Possibly. Check the placement of the mid texture.
                    if(Rend_MidTexturePos
                       (&gapBottom[0], &gapBottom[1], &gapTop[0], &gapTop[1],
                        NULL, side->SW_middlevisoffset[VX], mat->height,
                        0 != (line->flags & DDLF_DONTPEGBOTTOM)))
                    {
                        if(openTop[0] >= gapTop[0] &&
                           openTop[1] >= gapTop[1] &&
                           openBottom[0] <= gapBottom[0] &&
                           openBottom[1] <= gapBottom[1])
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
            side->sections[j].frameFlags |= SUFINF_PVIS;

        // A two sided line?
        if(line->L_frontside && line->L_backside)
        {
            // Check middle texture
            if((!side->SW_middlematerial || side->SW_middlematerial->ofTypeID <= 0) || side->SW_middlergba[3] <= 0) // Check alpha
                side->sections[SEG_MIDDLE].frameFlags &= ~SUFINF_PVIS;
        }

        // Top
        if(!line->L_backside)
        {
            side->sections[SEG_TOP].frameFlags &= ~SUFINF_PVIS;
            side->sections[SEG_BOTTOM].frameFlags &= ~SUFINF_PVIS;
        }
        else
        {
            if(R_IsSkySurface(&line->L_backsector->SP_ceilsurface) &&
               R_IsSkySurface(&line->L_frontsector->SP_ceilsurface))
               side->sections[SEG_TOP].frameFlags &= ~SUFINF_PVIS;
            else
            {
                if(i != 0)
                {
                    if(line->L_backsector->SP_ceilvisheight <=
                       line->L_frontsector->SP_ceilvisheight)
                        side->sections[SEG_TOP].frameFlags &= ~SUFINF_PVIS;
                }
                else
                {
                    if(line->L_frontsector->SP_ceilvisheight <=
                       line->L_backsector->SP_ceilvisheight)
                        side->sections[SEG_TOP].frameFlags &= ~SUFINF_PVIS;
                }
            }

            // Bottom
            if(R_IsSkySurface(&line->L_backsector->SP_floorsurface) &&
               R_IsSkySurface(&line->L_frontsector->SP_floorsurface))
               side->sections[SEG_BOTTOM].frameFlags &= ~SUFINF_PVIS;
            else
            {
                if(i != 0)
                {
                    if(line->L_backsector->SP_floorvisheight >=
                       line->L_frontsector->SP_floorvisheight)
                        side->sections[SEG_BOTTOM].frameFlags &= ~SUFINF_PVIS;
                }
                else
                {
                    if(line->L_frontsector->SP_floorvisheight >=
                       line->L_backsector->SP_floorvisheight)
                        side->sections[SEG_BOTTOM].frameFlags &= ~SUFINF_PVIS;
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
int Rend_MidTexturePos(float* bottomleft, float* bottomright,
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

typedef struct {
    boolean         isWall;
    const float*    segLength;
    float           wallAngleLightLevelDelta;

    rendpolytype_t  type;
    short           flags; // RPF_*.
    float           texOffset[2]; // Texture coordinates for left/top
                                  // (in real texcoords).
    const float*    normal; // Surface normal.
    float           alpha;
    const float*    sectorLightLevel;
    const float*    sectorLightColor;
    const float*    surfaceColor;
    const float*    surfaceColor2; // Secondary color.
    gltexture_t     tex;
    gltexture_t     interTex;
    float           interPos; // Blending strength (0..1).
    uint            lightListIdx; // List of lights that affect this poly.
    blendmode_t     blendMode; // Primitive-specific blending mode.

    const ded_reflection_t* ref;

    // For FakeRadio:
    boolean         doFakeRadio;
    const shadowcorner_t* botCn;
    const shadowcorner_t* topCn;
    const shadowcorner_t* sideCn;
    const edgespan_t* spans;
    const float*    segOffset;
    const float*    linedefLength;
    const sector_t* frontSec;
    const sector_t* backSec;

    // For bias:
    void*           mapObject;
    uint            elmIdx;
    biasaffection_t* affection;
    biastracker_t*  tracker;
} rendsegsection_params_t;

typedef struct {
    rendpolytype_t  type;
    short           flags; // RPF_*.
    float           texOffset[2]; // Texture coordinates for left/top
                                  // (in real texcoords).
    const float*    normal;
    float           alpha;
    const float*    sectorLightLevel;
    const float*    sectorLightColor;
    const float*    surfaceColor;
    gltexture_t     tex;
    gltexture_t     interTex;
    float           interPos; // Blending strength (0..1).
    uint            lightListIdx; // List of lights that affect this poly.
    blendmode_t     blendMode; // Primitive-specific blending mode.

    const ded_reflection_t* ref;

    // For bias:
    void*           mapObject;
    uint            elmIdx;
    biasaffection_t* affection;
    biastracker_t*  tracker;
} renderplane_params_t;

static void renderSegSection2(const rvertex_t* rvertices, uint numVertices,
                              const walldiv_t* divs,
                              const rendsegsection_params_t* p)
{
    rendpoly_wall_t     wall;
    rendpoly_params_t   params;
    rcolor_t            rcolors[4];

    // Light this polygon.
    if(!(p->flags & RPF_SKY_MASK))
    {
        if((p->flags & RPF_GLOW) || levelFullBright)
        {
            Rend_VertexColorsGlow(rcolors, 4);
        }
        else
        {
            if(useBias)
            {
                // Do BIAS lighting for this poly.
                SB_RendPoly(rvertices, rcolors, 4,
                            p->normal, *p->sectorLightLevel,
                            p->tracker, p->affection,
                            p->mapObject, p->elmIdx, true);
            }
            else
            {
                float               ll = *p->sectorLightLevel +
                    p->wallAngleLightLevelDelta;

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

                    if(p->surfaceColor2)
                    {
                        lightVertex(&rcolors[1], &rvertices[1], ll, vColor);
                        lightVertex(&rcolors[3], &rvertices[3], ll, vColor);
                    }
                    else
                    {
                        lightVertex(&rcolors[0], &rvertices[0], ll, vColor);
                        lightVertex(&rcolors[1], &rvertices[1], ll, vColor);
                        lightVertex(&rcolors[2], &rvertices[2], ll, vColor);
                        lightVertex(&rcolors[3], &rvertices[3], ll, vColor);
                    }
                }
                else
                {
                    // Use sector light+color only
                    if(p->surfaceColor2)
                    {
                        lightVertex(&rcolors[1], &rvertices[1], ll, p->sectorLightColor);
                        lightVertex(&rcolors[3], &rvertices[3], ll, p->sectorLightColor);
                    }
                    else
                    {
                        lightVertex(&rcolors[0], &rvertices[0], ll, p->sectorLightColor);
                        lightVertex(&rcolors[1], &rvertices[1], ll, p->sectorLightColor);
                        lightVertex(&rcolors[2], &rvertices[2], ll, p->sectorLightColor);
                        lightVertex(&rcolors[3], &rvertices[3], ll, p->sectorLightColor);
                    }
                }

                // Bottom color (if different from top)?
                if(p->surfaceColor2)
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
        }

        Rend_VertexColorsApplyTorchLight(rcolors, rvertices, 4, p->flags);
        Rend_VertexColorsAlpha(rcolors, 4, p->flags, p->alpha);
    }
    else
    {
        memset(&rcolors, 0, sizeof(rcolors));
    }

    // Init the params.
    memset(&params, 0, sizeof(params));

    params.type = p->type;
    params.flags = p->flags;

    params.isWall = p->isWall;
    params.wall = &wall;
    params.wall->length = *p->segLength;
    if(divs)
        memcpy(params.wall->divs, divs, sizeof(params.wall->divs));
    else
        memset(params.wall->divs, 0, sizeof(params.wall->divs));

    params.texOffset[VX] = p->texOffset[VX];
    params.texOffset[VY] = p->texOffset[VY];

    params.tex.id = curTex = p->tex.id;
    params.tex.magMode = glmode[texMagMode];
    params.tex.detail = p->tex.detail;
    params.tex.masked = p->tex.masked;
    params.tex.height = p->tex.height;
    params.tex.width = p->tex.width;

    params.interTex.id = p->interTex.id;
    params.interTex.magMode = glmode[texMagMode];
    params.interTex.detail = p->interTex.detail;
    params.interTex.masked = p->interTex.masked;
    params.interTex.height = p->interTex.height;
    params.interTex.width = p->interTex.width;

    params.interPos = p->interPos;
    params.lightListIdx = p->lightListIdx;
    params.blendMode = p->blendMode;

    // Write multiple polys depending on rend params.
    RL_AddPoly(rvertices, rcolors, 4, &params);

    // FakeRadio?
    if(p->doFakeRadio)
    {
        rendsegradio_params_t radioParams;

        radioParams.sectorLightLevel = p->sectorLightLevel;
        radioParams.linedefLength = p->linedefLength;
        radioParams.botCn = p->botCn;
        radioParams.topCn = p->topCn;
        radioParams.sideCn = p->sideCn;
        radioParams.spans = p->spans;
        radioParams.segOffset = p->segOffset;
        radioParams.segLength = p->segLength;
        radioParams.frontSec = p->frontSec;
        radioParams.backSec = p->backSec;

        Rend_RadioSegSection(rvertices,
                             (params.type == RP_DIVQUAD? divs:NULL),
                             &radioParams);
    }

    // ShinySurfaces?
    if(useShinySurfaces && p->ref)
    {
        rendshinypoly_params_t shinyParams;

        shinyParams.flags = p->flags;
        shinyParams.type = p->type;
        shinyParams.height = p->tex.height;
        shinyParams.width = p->tex.width;
        shinyParams.texOffset[VX] = p->texOffset[VX];
        shinyParams.texOffset[VY] = p->texOffset[VY];
        shinyParams.ref = p->ref;
        shinyParams.segLength = p->segLength;

        Rend_AddShinyPoly(rvertices, rcolors, 4, divs, &shinyParams);
    }
}

static void renderPlane2(const rvertex_t* rvertices, uint numVertices,
                         const renderplane_params_t* p)
{
    rendpoly_params_t   params;
    rcolor_t*           rcolors;

    rcolors = R_AllocRendColors(numVertices);

    // Light this polygon.
    if(!(p->flags & RPF_SKY_MASK))
    {
        if((p->flags & RPF_GLOW) || levelFullBright)
        {
            Rend_VertexColorsGlow(rcolors, numVertices);
        }
        else
        {
            if(useBias)
            {
                // Do BIAS lighting for this poly.
                SB_RendPoly(rvertices, rcolors, numVertices,
                            p->normal, *p->sectorLightLevel,
                            p->tracker, p->affection,
                            p->mapObject, p->elmIdx, false);
            }
            else
            {
                uint                i;

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
                        lightVertex(&rcolors[i], &rvertices[i],
                                    *p->sectorLightLevel, vColor);
                }
                else
                {
                    // Use sector light+color only
                    for(i = 0; i < numVertices; ++i)
                        lightVertex(&rcolors[i], &rvertices[i],
                                    *p->sectorLightLevel, p->sectorLightColor);
                }
            }
        }

        Rend_VertexColorsApplyTorchLight(rcolors, rvertices, numVertices,
                                         p->flags);
        Rend_VertexColorsAlpha(rcolors, numVertices, p->flags, p->alpha);
    }
    else
    {
        memset(rcolors, 0, sizeof(*rcolors) * numVertices);
    }

    // Init the params.
    memset(&params, 0, sizeof(params));

    params.type = p->type;
    params.flags = p->flags;

    params.isWall = false;
    params.wall = NULL;

    params.texOffset[VX] = p->texOffset[VX];
    params.texOffset[VY] = p->texOffset[VY];

    params.tex.id = curTex = p->tex.id;
    params.tex.magMode = glmode[texMagMode];
    params.tex.detail = p->tex.detail;
    params.tex.masked = p->tex.masked;
    params.tex.height = p->tex.height;
    params.tex.width = p->tex.width;

    params.interTex.id = p->interTex.id;
    params.interTex.magMode = glmode[texMagMode];
    params.interTex.detail = p->interTex.detail;
    params.interTex.masked = p->interTex.masked;
    params.interTex.height = p->interTex.height;
    params.interTex.width = p->interTex.width;

    params.interPos = p->interPos;
    params.lightListIdx = p->lightListIdx;
    params.blendMode = p->blendMode;

    // Write multiple polys depending on rend params.
    RL_AddPoly(rvertices, rcolors, numVertices, &params);
    if(useShinySurfaces && p->ref)
    {
        rendshinypoly_params_t shinyParams;

        shinyParams.flags = p->flags;
        shinyParams.type = p->type;
        shinyParams.height = p->tex.height;
        shinyParams.width = p->tex.width;
        shinyParams.texOffset[VX] = p->texOffset[VX];
        shinyParams.texOffset[VY] = p->texOffset[VY];
        shinyParams.ref = p->ref;
        shinyParams.segLength = 0;

        Rend_AddShinyPoly(rvertices, rcolors, numVertices, NULL, &shinyParams);
    }

    R_FreeRendColors(rcolors);
}

#define RPF2_SHADOW 0x0001
#define RPF2_SHINY  0x0002
#define RPF2_GLOW   0x0004
#define RPF2_BLEND  0x0008

static boolean doRenderSeg(seg_t* seg, segsection_t section,
                           surface_t* surface, float bottom, float top,
                           float alpha, float texXOffset, float texYOffset,
                           sector_t* frontsec, boolean skyMask,
                           short sideFlags)
{
    rendsegsection_params_t params;
    rvertex_t           rvertices[4];
    walldiv_t           divs[2];
    short               tempflags = 0;
    boolean             solidSeg = true;

    // Init the params.
    memset(&params, 0, sizeof(params));

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

    params.type = RP_QUAD;
    params.segLength = &seg->length;
    params.alpha = alpha;
    params.mapObject = seg;
    params.elmIdx = (uint) section;
    params.tracker = &seg->tracker[section];
    params.affection = seg->affected;
    params.botCn = SEG_SIDEDEF(seg)->bottomCorners;
    params.topCn = SEG_SIDEDEF(seg)->topCorners;
    params.sideCn = SEG_SIDEDEF(seg)->sideCorners;
    params.spans = SEG_SIDEDEF(seg)->spans;
    params.segOffset = &seg->offset;
    params.linedefLength = &seg->lineDef->length;
    params.frontSec = seg->SG_frontsector;
    params.backSec = seg->SG_backsector;

    // Fill in the remaining params data.
    if(skyMask || !surface->material || R_IsSkySurface(surface))
    {
        // In devSkyMode mode we render all polys destined for the skymask as
        // regular world polys (with a few obvious properties).
        if(devSkyMode)
        {
            params.tex.id = GL_PrepareMaterial(skyMaskMaterial);
            params.tex.magMode = glmode[texMagMode];

            params.tex.width = skyMaskMaterial->width;
            params.tex.height = skyMaskMaterial->height;
            params.flags |= RPF_GLOW;
        }
        else
        {
            // We'll mask this.
            params.flags |= RPF_SKY_MASK;
        }
    }
    else
    {
        int                 surfaceFlags;

        // Prepare the flat/texture
        if(renderTextures == 2)
        {   // For lighting debug, render all solid surfaces using the gray texture.
            params.tex.id =
                GL_PrepareMaterial(R_GetMaterial(DDT_GRAY, MAT_DDTEX));
            params.tex.magMode = glmode[texMagMode];

            surfaceFlags = surface->flags & ~(SUF_TEXFIX);

            // We need the properties of the real flat/texture.
            if(surface->material)
            {
                material_t*     mat = surface->material;

                params.tex.width = mat->width;
                params.tex.height = mat->height;
                params.tex.detail =
                    (r_detail && mat->detail.tex? &mat->detail : 0);
                params.tex.masked = mat->dgl.masked;

                if(mat->dgl.masked)
                    surfaceFlags &= ~SUF_NO_RADIO;
            }
        }
        else if((surface->flags & SUF_TEXFIX) && devNoTexFix)
        {   // For debug, render the "missing" texture instead of the texture
            // chosen for surfaces to fix the HOMs.
            material_t*         mat = R_GetMaterial(DDT_MISSING, MAT_DDTEX);

            params.tex.id = GL_PrepareMaterial(mat);
            params.tex.magMode = glmode[texMagMode];
            params.tex.width = mat->width;
            params.tex.height = mat->height;
            params.tex.detail =
                (r_detail && mat->detail.tex? &mat->detail : 0);
            params.tex.masked = mat->dgl.masked;
            surfaceFlags = surface->flags;

            if(mat->dgl.masked)
                surfaceFlags &= ~SUF_NO_RADIO;

            //// \kludge
            surfaceFlags |= SUF_GLOW; // Make it stand out
            //// <kludge
        }
        else if(surface->material)
        {
            material_t*         mat = surface->material;

            params.tex.id = GL_PrepareMaterial(mat);
            params.tex.magMode = glmode[texMagMode];
            surfaceFlags = surface->flags;

            //// \kludge >
            if(surface->material->type == MAT_DDTEX)
                surfaceFlags = SUF_GLOW; // Make it stand out.
            ///// <kludge

            params.tex.width = mat->width;
            params.tex.height = mat->height;
            params.tex.detail =
                (r_detail && mat->detail.tex? &mat->detail : 0);
            params.tex.masked = mat->dgl.masked;

            if(mat->dgl.masked)
                surfaceFlags &= ~SUF_NO_RADIO;
        }

        // Calculate texture coordinates.
        params.texOffset[VX] = texXOffset;
        params.texOffset[VY] = texYOffset;

        if(section == SEG_MIDDLE)
        {
            // Blendmode.
            if(surface->blendMode == BM_NORMAL && noSpriteTrans)
                params.blendMode = BM_ZEROALPHA; // "no translucency" mode
            else
                params.blendMode = surface->blendMode;

            // If alpha, masked or blended we'll render as a vissprite.
            if(alpha < 1 || params.tex.masked || surface->blendMode > 0)
            {
                params.flags = RPF_MASKED;
                solidSeg = false;
            }
        }
        else
        {
            params.blendMode = BM_NORMAL;
        }

        if(solidSeg && !(surfaceFlags & SUF_NO_RADIO))
            tempflags |= RPF2_SHADOW;
        if(solidSeg && surface->material && !params.tex.masked)
            tempflags |= RPF2_SHINY;
        if(surfaceFlags & SUF_GLOW)
            tempflags |= RPF2_GLOW;
        if(surface->material && (surface->material->flags & MATF_GLOW))
            tempflags |= RPF2_GLOW;
    }

    /**
     * If this poly is destined for the skymask, we don't need to
     * do any further processing.
     */
    if(!(params.flags & RPF_SKY_MASK))
    {
        const float*    topColor = NULL;
        const float*    bottomColor = NULL;

        params.normal = surface->normal;
        params.sectorLightLevel = &frontsec->lightLevel;
        params.sectorLightColor = R_GetSectorLightColor(frontsec);
        params.wallAngleLightLevelDelta =
            R_WallAngleLightLevelDelta(seg->lineDef, seg->side);

        // Smooth Texture Animation?
        if(tempflags & RPF2_BLEND)
        {
            params.interPos =
                polyTexBlend(&params.interTex, surface->material);
        }

        if(tempflags & RPF2_SHADOW)
            Rend_RadioUpdateLinedef(seg->lineDef, seg->side);

        // Select the colors for this surface.
        switch(section)
        {
        case SEG_MIDDLE:
            if(sideFlags & SDF_BLENDMIDTOTOP)
            {
                params.surfaceColor = SEG_SIDEDEF(seg)->SW_toprgba;
                params.surfaceColor2 = SEG_SIDEDEF(seg)->SW_middlergba;
            }
            else if(sideFlags & SDF_BLENDMIDTOBOTTOM)
            {
                params.surfaceColor = SEG_SIDEDEF(seg)->SW_middlergba;
                params.surfaceColor2 = SEG_SIDEDEF(seg)->SW_bottomrgba;
            }
            else
            {
                params.surfaceColor = SEG_SIDEDEF(seg)->SW_middlergba;
                params.surfaceColor2 = NULL;
            }
            break;

        case SEG_TOP:
            if(sideFlags & SDF_BLENDTOPTOMID)
            {
                params.surfaceColor = SEG_SIDEDEF(seg)->SW_toprgba;
                params.surfaceColor2 = SEG_SIDEDEF(seg)->SW_middlergba;
            }
            else
            {
                params.surfaceColor = SEG_SIDEDEF(seg)->SW_toprgba;
                params.surfaceColor2 = NULL;
            }
            break;

        case SEG_BOTTOM:
            // Select the correct colors for this surface.
            if(sideFlags & SDF_BLENDBOTTOMTOMID)
            {
                params.surfaceColor = SEG_SIDEDEF(seg)->SW_middlergba;
                params.surfaceColor2 = SEG_SIDEDEF(seg)->SW_bottomrgba;
            }
            else
            {
                params.surfaceColor = SEG_SIDEDEF(seg)->SW_bottomrgba;
                params.surfaceColor2 = NULL;
            }
            break;

        default:
            break;
        }

        // Make it fullbright?
        if((tempflags & RPF2_GLOW) && glowingTextures)
            params.flags |= RPF_GLOW;

        // Dynamic lights.
        if(!(params.flags & RPF_GLOW))
            params.lightListIdx =
                DL_ProcessSegSection(seg, bottom, top,
                                     (params.flags & RPF_MASKED)? true:false);

        // Check for neighborhood division?
        if(!(params.flags & RPF_MASKED))
        {
            applyWallHeightDivision(divs, seg, frontsec, bottom, top);

            if(divs[0].num > 0 || divs[1].num > 0)
                params.type = RP_DIVQUAD;
        }

        // Render Fakeradio polys for this seg?
        if((tempflags & RPF2_SHADOW) && !(params.flags & RPF_GLOW))
        {
            params.doFakeRadio = true;
        }

        // Render Shiny polys for this seg?
        if(tempflags & RPF2_SHINY)
        {
            ded_reflection_t* ref =
                R_MaterialGetReflection(surface->material);

            // Make sure the texture has been loaded.
            if(GL_LoadReflectionMap(ref))
                params.ref = ref;
        }
    }

    renderSegSection2(rvertices, 4, divs, &params);
    return solidSeg;
}

static boolean renderSegSection(seg_t* seg, segsection_t section,
                                surface_t* surface, float bottom, float top,
                                float texXOffset, float texYOffset,
                                sector_t* frontsec, boolean softSurface,
                                boolean canMask, short sideFlags)
{
    boolean             visible = false, skyMask = false;
    boolean             solidSeg = true;

    if(bottom >= top)
        return true;

    if(surface->material && surface->material->ofTypeID != 0)
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
            mobj_t             *mo = viewPlayer->shared.mo;

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
            solidSeg = doRenderSeg(seg, section, surface, bottom, top,
                                   alpha, texXOffset, texYOffset, frontsec,
                                   skyMask, sideFlags);
        }
    }

    return solidSeg;
}

/**
 * Renders the given single-sided seg into the world.
 */
static boolean Rend_RenderSSWallSeg(seg_t* seg, subsector_t* ssec)
{
    boolean             solidSeg = true;
    sidedef_t*          side;
    linedef_t*          ldef;
    float               ffloor, fceil;
    boolean             backSide;
    sector_t*           frontsec, *fflinkSec, *fclinkSec;
    int                 pid = viewPlayer - ddPlayers;

    side = SEG_SIDEDEF(seg);
    frontsec = side->sector;
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

    fflinkSec = R_GetLinkedSector(ssec, PLN_FLOOR);
    ffloor = fflinkSec->SP_floorvisheight;
    fclinkSec = R_GetLinkedSector(ssec, PLN_CEILING);
    fceil = fclinkSec->SP_ceilvisheight;

    // Create the wall sections.

    // Middle section.
    if(side->SW_middlesurface.frameFlags & SUFINF_PVIS)
    {
        float               offset[2];
        surface_t*          surface = &side->SW_middlesurface;

        offset[VX] = surface->visOffset[VX] + seg->offset;
        offset[VY] = surface->visOffset[VY] +
            (ldef->flags & DDLF_DONTPEGBOTTOM)? -(fceil - ffloor) : 0;

        renderSegSection(seg, SEG_MIDDLE, &side->SW_middlesurface, ffloor, fceil,
                         offset[VX], offset[VY],
                         /*temp >*/ frontsec, /*< temp*/
                         false, true, side->flags);
    }

    if(P_IsInVoid(viewPlayer))
        solidSeg = false;

    return solidSeg;
}

/**
 * Renders wall sections for given two-sided seg.
 */
static boolean Rend_RenderWallSeg(seg_t* seg, subsector_t* ssec)
{
    int                 solidSeg = false;
    sector_t*           backsec;
    sidedef_t*          backsid, *side;
    linedef_t*          ldef;
    float               ffloor, fceil, bfloor, bceil, bsh;
    boolean             backSide;
    sector_t*           frontsec, *fflinkSec, *fclinkSec;
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

    fflinkSec = R_GetLinkedSector(ssec, PLN_FLOOR);
    ffloor = fflinkSec->SP_floorvisheight;
    fclinkSec = R_GetLinkedSector(ssec, PLN_CEILING);
    fceil = fclinkSec->SP_ceilvisheight;

    bceil = backsec->SP_ceilvisheight;
    bfloor = backsec->SP_floorvisheight;
    bsh = bceil - bfloor;

    // Create the wall sections.

    // We may need multiple wall sections.
    // Determine which parts of the segment are visible.

    // Middle section.
    if(side->SW_middlesurface.frameFlags & SUFINF_PVIS)
    {
        surface_t*          surface = &side->SW_middlesurface;
        float               texOffset[2];
        float               top, bottom, vL_ZBottom, vR_ZBottom, vL_ZTop, vR_ZTop;
        boolean             softSurface = false;

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

        if(Rend_MidTexturePos
           (&vL_ZBottom, &vR_ZBottom, &vL_ZTop, &vR_ZTop,
           &texOffset[VY], surface->visOffset[VY],
           surface->material->current->height,
            (ldef->flags & DDLF_DONTPEGBOTTOM)? true : false))
        {
            // Can we make this a soft surface?
            if((viewPlayer->shared.flags & (DDPF_NOCLIP|DDPF_CAMERA)) ||
               !(ldef->flags & DDLF_BLOCKING))
            {
                softSurface = true;
            }

            texOffset[VX] = surface->visOffset[VX] + seg->offset;

            solidSeg = renderSegSection(seg, SEG_MIDDLE, surface,
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
    if(side->SW_topsurface.frameFlags & SUFINF_PVIS)
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

        renderSegSection(seg, SEG_TOP, &side->SW_topsurface, bottom, fceil,
                         texOffset[VX], texOffset[VY], frontsec, false,
                         false, side->flags);
    }

    // Lower section.
    if(side->SW_bottomsurface.frameFlags & SUFINF_PVIS)
    {
        float               top = bfloor;
        float               texOffset[2];
        surface_t*          surface = &side->SW_bottomsurface;

        texOffset[VX] = surface->visOffset[VX] + seg->offset;
        texOffset[VY] = surface->visOffset[VY];

        if(bfloor > fceil)
        {   // Can't go over front ceiling, would induce polygon flaws.
            texOffset[VY] += bfloor - fceil;
            top = fceil;
        }

        if(ldef->flags & DDLF_DONTPEGBOTTOM)
            texOffset[VY] += (fceil - bfloor); // Align with normal middle texture.

        renderSegSection(seg, SEG_BOTTOM, &side->SW_bottomsurface, ffloor, top,
                         texOffset[VX], texOffset[VY], frontsec,
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
                (bsh == 0 && bfloor > ffloor && bceil < fceil &&
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
    rendpoly_params_t   params;
    rendpoly_wall_t     wall;
    float*              vBL, *vBR, *vTL, *vTR;
    sector_t*           frontsec, *backsec;
    uint                j, num;
    seg_t*              seg, **list;
    sidedef_t*          side;

    // Init the poly.
    memset(&params, 0, sizeof(params));
    params.type = RP_QUAD;
    params.flags = RPF_SKY_MASK;
    params.wall = &wall;

    // In devSkyMode mode we render all polys destined for the skymask as
    // regular world polys (with a few obvious properties).
    if(devSkyMode)
    {
        size_t              i;

        params.tex.id = curTex = GL_PrepareMaterial(skyMaskMaterial);
        params.tex.magMode = glmode[texMagMode];
        params.tex.width = skyMaskMaterial->width;
        params.tex.height = skyMaskMaterial->height;

        params.flags &= ~RPF_SKY_MASK;
        params.flags |= RPF_GLOW;

        for(i = 0; i < 4; ++i)
            rcolors[i].rgba[CR] =
                rcolors[i].rgba[CG] =
                    rcolors[i].rgba[CB] =
                        rcolors[i].rgba[CA] = 1;
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

        if(!seg->lineDef)    // "minisegs" have no linedefs.
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

        params.wall->length = seg->length;

        // Upper/lower normal skyfixes.
        if(!((frontsec->flags & SECF_SELFREFHACK) && backsec))
        {
            // Floor.
            if(frontsec->skyFix[PLN_FLOOR].offset < 0)
            {
                if(!backsec ||
                   (backsec && backsec != seg->SG_frontsector &&
                    (bfloor + backsec->skyFix[PLN_FLOOR].offset >
                     ffloor + frontsec->skyFix[PLN_FLOOR].offset)))
                {
                    vTL[VZ] = vTR[VZ] = ffloor;
                    vBL[VZ] = vBR[VZ] =
                        ffloor + frontsec->skyFix[PLN_FLOOR].offset;
                    RL_AddPoly(rvertices, rcolors, 4, &params);
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
                    vTL[VZ] = vTR[VZ] =
                        fceil + frontsec->skyFix[PLN_CEILING].offset;
                    vBL[VZ] = vBR[VZ] = fceil;

                    RL_AddPoly(rvertices, rcolors, 4, &params);
                }
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
                    vTL[VZ] = vTR[VZ] = bfloor;
                    vBL[VZ] = vBR[VZ] =
                        bfloor + backsec->skyFix[PLN_FLOOR].offset;
                    RL_AddPoly(rvertices, rcolors, 4, &params);
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
                    vTL[VZ] = vTR[VZ] =
                        bceil + backsec->skyFix[PLN_CEILING].offset;
                    vBL[VZ] = vBR[VZ] = bceil;
                    RL_AddPoly(rvertices, rcolors, 4, &params);
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
        if(seg->lineDef && seg->SG_backsector &&
           !(seg->flags & SEGF_POLYOBJ) && // Polyobjects don't occlude.
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

static void Rend_RenderPlane(subsector_t* subsector, uint planeID)
{
    sector_t*           sector = subsector->sector;
    float               height;
    surface_t*          surface;
    sector_t*           polySector;
    float               vec[3];
    plane_t*            plane;

    polySector = R_GetLinkedSector(subsector, planeID);
    surface = &polySector->planes[planeID]->surface;

    // Must have a visible surface.
    if(!surface->material)
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
        renderplane_params_t params;
        uint                numVertices = subsector->numVertices;
        rvertex_t*          rvertices;
        uint                subIndex = GET_SUBSECTOR_IDX(subsector);

        rvertices = R_AllocRendVertices(numVertices);

        memset(&params, 0, sizeof(params));

        params.type = RP_FLAT;
        params.blendMode = BM_NORMAL;
        params.mapObject = subsector;
        params.elmIdx = plane->planeID;

        // Check for sky.
        if(R_IsSkySurface(surface))
        {
            skyhemispheres |=
                (plane->type == PLN_FLOOR? SKYHEMI_LOWER : SKYHEMI_UPPER);

            params.flags |= RPF_SKY_MASK;
            params.alpha = 1;

            // In devSkyMode mode we render all polys destined for the skymask as
            // regular world polys (with a few obvious properties).
            if(devSkyMode)
            {
                params.tex.id = curTex =
                    GL_PrepareMaterial(skyMaskMaterial);
                params.tex.magMode = glmode[texMagMode];
                params.tex.width = skyMaskMaterial->width;
                params.tex.height = skyMaskMaterial->height;

                params.flags &= ~RPF_SKY_MASK;
                params.flags |= RPF_GLOW;
            }
        }
        else
        {
            int             surfaceFlags;

            // Prepare the flat/texture
            if(renderTextures == 2)
            {   // For lighting debug, render all solid surfaces using the gray texture.
                params.tex.id =
                    GL_PrepareMaterial(R_GetMaterial(DDT_GRAY, MAT_DDTEX));
                params.tex.magMode = glmode[texMagMode];

                surfaceFlags = surface->flags & ~(SUF_TEXFIX);

                // We need the properties of the real flat/texture.
                if(surface->material)
                {
                    material_t*     mat = surface->material;

                    params.tex.width = mat->width;
                    params.tex.height = mat->height;
                    params.tex.detail =
                        (r_detail && mat->detail.tex? &mat->detail : 0);
                    params.tex.masked = mat->dgl.masked;

                    if(mat->dgl.masked)
                        surfaceFlags &= ~SUF_NO_RADIO;
                }
            }
            else if((surface->flags & SUF_TEXFIX) && devNoTexFix)
            {   // For debug, render the "missing" texture instead of the texture
                // chosen for surfaces to fix the HOMs.
                material_t*         mat = R_GetMaterial(DDT_MISSING, MAT_DDTEX);

                params.tex.id = GL_PrepareMaterial(mat);
                params.tex.magMode = glmode[texMagMode];
                surfaceFlags = surface->flags;

                params.tex.width = mat->width;
                params.tex.height = mat->height;
                params.tex.detail =
                    (r_detail && mat->detail.tex? &mat->detail : 0);
                params.tex.masked = mat->dgl.masked;

                if(mat->dgl.masked)
                    surfaceFlags &= ~SUF_NO_RADIO;
                surfaceFlags |= SUF_GLOW; // Make it stand out
            }
            else if(surface->material)
            {
                material_t*         mat = surface->material;

                params.tex.id = GL_PrepareMaterial(mat);
                params.tex.magMode = glmode[texMagMode];
                surfaceFlags = surface->flags;

                //// \kludge >
                if(mat->type == MAT_DDTEX)
                    surfaceFlags = SUF_GLOW; // Make it stand out.
                 ///// <kludge

                params.tex.width = mat->width;
                params.tex.height = mat->height;
                params.tex.detail =
                    (r_detail && mat->detail.tex? &mat->detail : 0);
                params.tex.masked = mat->dgl.masked;

                if(mat->dgl.masked)
                    surfaceFlags &= ~SUF_NO_RADIO;
            }

            params.texOffset[VX] =
                sector->SP_plane(plane->planeID)->PS_visoffset[VX];
            params.texOffset[VY] =
                sector->SP_plane(plane->planeID)->PS_visoffset[VY];

            if(surface->material && !params.tex.masked)
                tempflags |= RPF2_SHINY;
            if(surfaceFlags & SUF_GLOW)
                tempflags |= RPF2_GLOW;
            if(surface->material && (surface->material->flags & MATF_GLOW))
                tempflags |= RPF2_GLOW;
            if(!(surface->flags & SUF_TEXFIX))
                tempflags |= RPF2_BLEND;

            if((tempflags & RPF2_GLOW) && glowingTextures) // Make it fullbright?
                params.flags |= RPF_GLOW;

            // Dynamic lights.
            if(!(params.flags & RPF_GLOW))
                params.lightListIdx =
                    DL_ProcessSubSectorPlane(subsector, plane->planeID);

            // Smooth Texture Animation?
            if(tempflags & RPF2_BLEND)
            {
                params.interPos =
                    polyTexBlend(&params.interTex, surface->material);
            }

            params.alpha = surface->rgba[CA];
        }

        Rend_PreparePlane(rvertices, numVertices, height, subsector,
                          !(surface->normal[VZ] > 0));

        params.normal = surface->normal;
        params.sectorLightLevel = &polySector->lightLevel;

        {
        subplaneinfo_t*     subPln =
            &plane->subPlanes[subsector->inSectorID];

        params.tracker = &subPln->tracker;
        params.affection = subPln->affected;
        }

        params.sectorLightColor = R_GetSectorLightColor(polySector);
        params.surfaceColor = surface->rgba;

        // Render Shiny polys for this seg?
        if((tempflags & RPF2_SHINY) && !R_IsSkySurface(surface))
        {
            ded_reflection_t*   ref =
                R_MaterialGetReflection(surface->material);

            // Make sure the texture has been loaded.
            if(GL_LoadReflectionMap(ref))
                params.ref = ref;
        }

        renderPlane2(rvertices, numVertices, &params);

        R_FreeRendVertices(rvertices);
    }
}

static void Rend_RenderSubsector(uint ssecidx)
{
    uint                i;
    subsector_t*        ssec = SUBSECTOR_PTR(ssecidx);
    seg_t*              seg, **ptr;
    sector_t*           sect = ssec->sector;
    float               sceil = sect->SP_ceilvisheight;
    float               sfloor = sect->SP_floorvisheight;

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

    // Prepare for dynamic lighting.
    LO_InitForSubsector(ssec);

    Rend_RadioSubsectorEdges(ssec);

    occludeSubsector(ssec, false);
    LO_ClipInSubsector(ssecidx);
    occludeSubsector(ssec, true);

    if(ssec->polyObj)
    {
        // Polyobjs don't obstruct, do clip lights with another algorithm.
        LO_ClipBySight(ssecidx);
    }

    // Mark the particle generators in the sector visible.
    PG_SectorIsVisible(sect);

    // Sprites for this sector have to be drawn. This must be done before
    // the segments of this subsector are added to the clipper. Otherwise
    // the sprites would get clipped by them, and that wouldn't be right.
    R_AddSprites(sect);

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
                solid = Rend_RenderSSWallSeg(seg, ssec);
            else
                solid = Rend_RenderWallSeg(seg, ssec);

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
                boolean             solid = Rend_RenderSSWallSeg(seg, ssec);

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
        RL_ClearLists();        // Clear the lists for new quads.
        C_ClearRanges();        // Clear the clipper.
        LO_ClearForFrame();     // Zeroes the links.
        LG_Update();
        SB_BeginFrame();
        Rend_RadioInitForFrame();

        // Generate surface decorations for the frame.
        Rend_InitDecorationsForFrame();

        if(doLums)
        {
            // Clear the projected dynlight lists.
            DL_InitForNewFrame();

            // Clear the luminous objects.
            LO_InitForNewFrame();
        }

        // Make vissprites of all the visible decorations.
        Rend_ProjectDecorations();

        // Maintain luminous objects.
        if(doLums)
        {
            LO_AddLuminousMobjs();
            LO_LinkLumobjs();
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

        // Wrap up with Shadow Bias.
        SB_EndFrame();
    }
    RL_RenderAllLists();

    // Draw the mobj bounding boxes.
    Rend_RenderBoundingBoxes();

    // Draw the Shadow Bias Editor's draw that identifies the current
    // light.
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

    DGL_Begin(DGL_QUADS);
    {
        DGL_Color4f(color3f[0], color3f[1], color3f[2], alpha);
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

    mobj_t             *mo;
    uint                i;
    sector_t           *sec;
    float               size;
    float               alpha;
    float               eye[3];

    if(!devMobjBBox || netGame)
        return;

    eye[VX] = vx;
    eye[VY] = vz;
    eye[VZ] = vy;

    if(usingFog)
        DGL_Disable(DGL_FOG);

    glDisable(GL_DEPTH_TEST);
    DGL_Enable(DGL_TEXTURING);
    glDisable(GL_CULL_FACE);

    DGL_Bind(GL_PrepareMaterial(R_GetMaterial(DDT_BBOX, MAT_DDTEX)));
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
                          alpha, 0.08f, true);

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
