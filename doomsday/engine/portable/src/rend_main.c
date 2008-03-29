/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
extern int loMaxRadius;
extern int devNoCulling;
extern boolean firstFrameAfterLoad;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean usingFog = false; // Is the fog in use?
float fogColor[4];
float fieldOfView = 95.0f;
float maxLightDist = 1024;
byte smoothTexAnim = true;
int useShinySurfaces = true;

float vx, vy, vz, vang, vpitch;
float viewsidex, viewsidey;

boolean willRenderSprites = true;
byte freezeRLs = false;

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

// Wall section colors.
// With accompanying ptrs to allow for "hot-swapping" (for speed).
const float *topColorPtr;
static float topColor[3];
const float *midColorPtr;
static float midColor[3];
const float *bottomColorPtr;
static float bottomColor[3];

// Current sector light color.
const float *sLightColor;

byte devNoTexFix = 0;
byte devNoLinkedSurfaces = 0;

// CODE --------------------------------------------------------------------

void Rend_Register(void)
{
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
    C_VAR_BYTE("rend-dev-tex-showfix", &devNoTexFix, 0, 0, 1);
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

#if 0 // unused atm
/**
 * Models the effect of distance to the light level. Extralight (torch)
 * is also noted. This is meant to be used for white light only
 * (a light level).
 */
int R_AttenuateLevel(float lightlevel, float distance)
{
    float   light = lightlevel, real, minimum;
    float   d, newmin;
    int     i;

    //boolean usewhite = false;

    real = light - (distance - 32) / maxLightDist * (1 - light);
    minimum = light * light + (light - .63f) / 2;
    if(real < minimum)
        real = minimum;         // Clamp it.

    // Add extra light.
    real += extraLight / 16.0f;

    // Check for torch.
    if(viewPlayer->fixedColorMap)
    {
        // Colormap 1 is the brightest. I'm guessing 16 would be the darkest.
        newmin =
            ((1024 - distance) / 512) * (16 -
                                         viewPlayer->fixedColorMap) / 15.0f;
        if(real < newmin)
        {
            real = newmin;
            //usewhite = true; // FIXME : Do some linear blending.
        }
    }

    real *= 256;

    // Clamp the final light.
    if(real < 0)
        real = 0;
    if(real > 255)
        real = 255;

    /*for(i = 0; i < 3; i++)
       vtx->color.rgb[i] = (DGLubyte) ((usewhite? 0xff : rgb[c]) * real); */
    return real;
}
#endif

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

static void Rend_ShinySurfaceColor(float color[4], ded_reflection_t *ref)
{
    uint                i;

    for(i = 0; i < 3; ++i)
    {
        float               min = ref->minColor[i];

        if(color[i] < min)
            color[i] = min;
    }

    color[CA] = ref->shininess;

    /*Con_Printf("shiny = %g %g %g %g\n", color[CR], color[CG], color[CB],
           color[CA]);*/
}

static ded_reflection_t *getReflectionDef(material_t *material, short *width,
                                          short *height)
{
    ded_reflection_t *ref = NULL;

    if(!material)
        return NULL;

    // Figure out what kind of surface properties have been defined
    // for the texture or flat in question.
    switch(material->type)
    {
    case MAT_FLAT:
        {
        flat_t         *flatptr =
            flats[flattranslation[material->ofTypeID].current];

        ref = flatptr->reflection;
        if(ref)
        {
            if(width)
                *width = flatptr->info.width;
            if(height)
                *height = flatptr->info.height;
        }
        break;
        }
    case MAT_TEXTURE:
        {
        texture_t      *texptr =
            textures[texturetranslation[material->ofTypeID].current];

        ref = texptr->reflection;
        if(ref)
        {
            if(width)
                *width = texptr->info.width;
            if(height)
                *height = texptr->info.height;
        }
        break;
        }
    default:
        break;
    }

    return ref;
}

/**
 * \pre As we modify param poly quite a bit it is the responsibility of
 *      the caller to ensure this is OK (ie if not it should pass us a
 *      copy instead (eg wall segs)).
 *
 * @param   texture     The texture/flat id of the texture on the poly.
 * @param   isFlat      @c true = param texture is a flat.
 * @param   poly        The poly to add the shiny poly for.
 */
static void Rend_AddShinyPoly(rendpoly_t *poly, ded_reflection_t *ref,
                              short width, short height)
{
    uint        i;

    // Make it a shiny polygon.
    poly->lightListIdx = 0;
    poly->flags |= RPF_SHINY;
    poly->blendMode = ref->blendMode;
    poly->tex.id = ref->useShiny->shinyTex;
    poly->tex.detail = NULL;
    poly->tex.masked = false;
    poly->interTex.detail = NULL;
    poly->interTex.masked = false;
    poly->interPos = 0;

    // Strength of the shine.
    for(i = 0; i < poly->numVertices; ++i)
    {
        Rend_ShinySurfaceColor(poly->vertices[i].color, ref);
    }

    // The mask texture is stored in the intertex.
    if(ref->useMask && ref->useMask->maskTex)
    {
        poly->interTex.id = ref->useMask->maskTex;
        poly->tex.width = poly->interTex.width = width * ref->maskWidth;
        poly->tex.height = poly->interTex.height = height * ref->maskHeight;
    }
    else
    {
        // No mask.
        poly->interTex.id = 0;
    }

    RL_AddPoly(poly);
}

static void polyTexBlend(rendpoly_t *poly, material_t *material)
{
    texinfo_t      *texinfo;
    translation_t  *xlat = NULL;

    if(!material)
        return;

    switch(material->type)
    {
    case MAT_FLAT:
        xlat = &flattranslation[material->ofTypeID];
        break;

    case MAT_TEXTURE:
        xlat = &texturetranslation[material->ofTypeID];
        break;

    default:
        break;
    }

    // If fog is active, inter=0 is accepted as well. Otherwise flickering
    // may occur if the rendering passes don't match for blended and
    // unblended surfaces.
    if(!xlat || !smoothTexAnim || numTexUnits < 2 || xlat->current == xlat->next ||
       (!usingFog && xlat->inter < 0) || renderTextures == 2)
    {
        // No blending for you, my friend.
        return;
    }

    // Get info of the blend target.
    poly->interTex.id =
        GL_PrepareMaterial2(R_GetMaterial(xlat->next, material->type), false, &texinfo);

    poly->interTex.width = texinfo->width;
    poly->interTex.height = texinfo->height;
    poly->interTex.detail = (r_detail && texinfo->detail.tex? &texinfo->detail : 0);
    poly->interTex.masked = texinfo->masked;
    poly->interPos = xlat->inter;
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
            texinfo_t          *texinfo = NULL;

            GL_GetMaterialInfo(side->SW_middlematerial->ofTypeID,
                               side->SW_middlematerial->type, &texinfo);

            if(!side->SW_middleblendmode && side->SW_middlergba[3] >= 1 && !texinfo->masked)
            {
                float               openTop[2], gapTop[2];
                float               openBottom[2], gapBottom[2];

                openTop[0] = openTop[1] = gapTop[0] = gapTop[1] =
                    MIN_OF(back->SP_ceilvisheight, front->SP_ceilvisheight);
                openBottom[0] = openBottom[1] = gapBottom[0] = gapBottom[1] =
                    MAX_OF(back->SP_floorvisheight, front->SP_floorvisheight);

                // Could the mid texture fill enough of this gap for us
                // to consider it completely closed?
                if(texinfo->height >= (openTop[0] - openBottom[0]) &&
                   texinfo->height >= (openTop[1] - openBottom[1]))
                {
                    // Possibly. Check the placement of the mid texture.
                    if(Rend_MidTexturePos
                       (&gapBottom[0], &gapBottom[1], &gapTop[0], &gapTop[1],
                        NULL, side->SW_middlevisoffset[VX], texinfo->height,
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
 * Prepares the correct flat/texture for the passed poly.
 *
 * @param poly          Ptr to the poly to apply the texture to.
 * @param surface       Ptr to the surface properties this rendpoly is using.
 * @return int          @c -1, if the texture reference is invalid,
 *                      else return the surface flags for this poly.
 */
static int prepareMaterialForPoly(rendpoly_t *poly, surface_t *surface,
                                  texinfo_t **texinfo)
{
    texinfo_t          *info;
    int                 flags = 0;

    if(R_IsSkySurface(surface))
        return 0;

    // Prepare the flat/texture
    if(renderTextures == 2)
    {   // For lighting debug, render all solid surfaces using the gray texture.
        poly->tex.id = curTex =
            GL_PrepareMaterial(R_GetMaterial(DDT_GRAY, MAT_DDTEX), &info);

        flags = surface->flags & ~(SUF_TEXFIX);

        // We need the properties of the real flat/texture.
        if(surface->material)
        {
            GL_GetMaterialInfo(surface->material->ofTypeID,
                               surface->material->type, &info);

            poly->tex.width = info->width;
            poly->tex.height = info->height;
            poly->tex.detail = (r_detail && info->detail.tex? &info->detail : 0);
            poly->tex.masked = info->masked;

            if(info->masked)
                flags &= ~SUF_NO_RADIO;
        }
    }
    else if((surface->flags & SUF_TEXFIX) && devNoTexFix)
    {   // For debug, render the "missing" texture instead of the texture
        // chosen for surfaces to fix the HOMs.
        poly->tex.id = curTex =
            GL_PrepareMaterial(R_GetMaterial(DDT_MISSING, MAT_DDTEX), &info);
        flags = SUF_GLOW; // Make it stand out
    }
    else if(surface->material)
    {
        poly->tex.id = curTex =
            GL_PrepareMaterial2(surface->material, true, &info);
        flags = surface->flags;

        //// \kludge >
        if(surface->material->type == MAT_DDTEX)
            flags = SUF_GLOW; // Make it stand out.
         ///// <kludge

        poly->tex.width = info->width;
        poly->tex.height = info->height;
        poly->tex.detail = (r_detail && info->detail.tex? &info->detail : 0);
        poly->tex.masked = info->masked;

        if(info->masked)
            flags &= ~SUF_NO_RADIO;
    }
    else
    {   // Shouldn't ever get here!
#if _DEBUG
        Con_Error("prepareMaterialForPoly: Surface with no material?");
#endif
    }

    if(texinfo)
        *texinfo = info;

    // Return the parameters for this surface.
    return flags;
}

/**
 * @return          @true,  if there is a division at the specified height.
 */
static int checkDiv(walldiv_t *div, float height)
{
    uint        i;

    for(i = 0; i < div->num; ++i)
        if(div->pos[i] == height)
            return true;

    return false;
}

static void doCalcSegDivisions(linedef_t *line, boolean backSide,sector_t *frontSec,
                               walldiv_t *div, float bottomZ, float topZ,
                               boolean doRight)
{
    uint        i, j;
    linedef_t     *iter;
    sector_t   *scanSec;
    lineowner_t *base, *own;
    boolean     clockwise = !doRight;
    boolean     stopScan = false;

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

static void calcSegDivisions(const seg_t *seg, sector_t *frontSec,
                             walldiv_t *div, float bottomZ, float topZ,
                             boolean doRight)
{
    sidedef_t          *side;

    div->num = 0;

    if(!seg->lineDef)
        return; // Mini-segs arn't drawn.

    side = SEG_SIDEDEF(seg);

    if(seg->flags & SEGF_POLYOBJ)
        return; // Polyobj segs are never split.

    // Only segs at sidedef ends can/should be split.
    if(!((seg == side->segs[0] && !doRight) ||
         (seg == side->segs[side->segCount -1] && doRight)))
        return;

    doCalcSegDivisions(seg->lineDef, seg->side, frontSec, div, bottomZ,
                       topZ, doRight);
}

/**
 * Division will only happen if it must be done.
 * Converts quads to divquads.
 */
static void applyWallHeightDivision(rendpoly_t *quad, const seg_t *seg,
                                    sector_t *frontsec, float low, float hi)
{
    uint                i;
    walldiv_t          *div;

    for(i = 0; i < 2; ++i)
    {
        div = &quad->wall->divs[i];
        calcSegDivisions(seg, frontsec, div, low, hi, i);

        if(div->num > 0)
            quad->type = RP_DIVQUAD;

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
int Rend_MidTexturePos(float *bottomleft, float *bottomright,
                       float *topleft, float *topright, float *texoffy,
                       float tcyoff, float texHeight, boolean lower_unpeg)
{
    int         side;
    float       openingTop, openingBottom;
    boolean     visible[2] = {false, false};

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
        if(lower_unpeg)
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

static void getColorsForSegSection(short sideFlags, segsection_t section,
                                   const float **bottomColor, const float **topColor)
{
    // Select the correct colors for this surface.
    switch(section)
    {
    case SEG_MIDDLE:
        if(sideFlags & SDF_BLENDMIDTOTOP)
        {
            *topColor = topColorPtr;
            *bottomColor = midColorPtr;
        }
        else if(sideFlags & SDF_BLENDMIDTOBOTTOM)
        {
            *topColor = midColorPtr;
            *bottomColor = bottomColorPtr;
        }
        else
        {
            *topColor = midColorPtr;
            *bottomColor = NULL;
        }
        break;

    case SEG_TOP:
        if(sideFlags & SDF_BLENDTOPTOMID)
        {
            *topColor = topColorPtr;
            *bottomColor = midColorPtr;
        }
        else
        {
            *topColor = topColorPtr;
            *bottomColor = NULL;
        }
        break;

    case SEG_BOTTOM:
        // Select the correct colors for this surface.
        if(sideFlags & SDF_BLENDBOTTOMTOMID)
        {
            *topColor = midColorPtr;
            *bottomColor = bottomColorPtr;
        }
        else
        {
            *topColor = bottomColorPtr;
            *bottomColor = NULL;
        }
        break;

    default:
        Con_Error("getColorsForSegSection: Invalid value, section = %i",
                  (int) section);
        break;
    }
}

#define RPF2_SHADOW 0x0001
#define RPF2_SHINY  0x0002
#define RPF2_GLOW   0x0004
#define RPF2_BLEND  0x0008

/**
 * Same as above but for planes. Ultimately we should consider merging the
 * two into a Rend_RenderSurface.
 */
static void doRenderPlane(rendpoly_t *poly, sector_t *polySector,
                          subsector_t *subsector,
                          subplaneinfo_t* plane, surface_t *surface,
                          float height, short flags)
{
    uint                subIndex = GET_SUBSECTOR_IDX(subsector);

    if((flags & RPF2_GLOW) && glowingTextures) // Make it fullbright?
        poly->flags |= RPF_GLOW;

    // Surface color/light.
    RL_PreparePlane(poly, height, subsector, Rend_SectorLight(polySector),
                    !(surface->normal[VZ] > 0),
                    R_GetSectorLightColor(polySector), surface->rgba);

    // Dynamic lights. Check for sky.
    if(R_IsSkySurface(surface))
    {
        poly->lightListIdx = 0;
        skyhemispheres |= (plane->type == PLN_FLOOR? SKYHEMI_LOWER : SKYHEMI_UPPER);
    }
    else
        poly->lightListIdx = DL_ProcessSubSectorPlane(subsector, plane->planeID);

    if(useBias)
    {
        // Do BIAS lighting for this poly.
        SB_RendPoly(poly, subsector->sector->lightLevel, plane->illumination,
                    &plane->tracker, plane->affected, subIndex);
    }

    // Smooth Texture Animation?
    if(flags & RPF2_BLEND)
        polyTexBlend(poly, surface->material);

    // Add the poly to the appropriate list
    RL_AddPoly(poly);

    // Fakeradio uses a seperate algorithm for planes.
    //if(flags & RPF2_SHADOW)

    // Render Shiny polys for this seg?
    if((flags & RPF2_SHINY) && useShinySurfaces)
    {
        ded_reflection_t *ref;
        short       width = 0;
        short       height = 0;

        if((ref = getReflectionDef(surface->material, &width, &height)) != NULL)
        {
            // Make sure the texture has been loaded.
            if(GL_LoadReflectionMap(ref))
            {
                Rend_AddShinyPoly(poly, ref, width, height);
            }
        }
    }
}

static void setSurfaceColorsForSide(sidedef_t *side)
{
    uint        i;

    // Top wall section color offset?
    if(side->SW_toprgba[0] < 1 || side->SW_toprgba[1] < 1 ||
       side->SW_toprgba[2] < 1)
    {
        for(i = 0; i < 3; ++i)
            topColor[i] = side->SW_toprgba[i] * sLightColor[i];

        topColorPtr = topColor;
    }
    else
        topColorPtr = sLightColor;

    // Mid wall section color offset?
    if(side->SW_middlergba[0] < 1 || side->SW_middlergba[1] < 1 ||
       side->SW_middlergba[2] < 1)
    {
        for(i = 0; i < 3; ++i)
            midColor[i] = side->SW_middlergba[i] * sLightColor[i];

        midColorPtr = midColor;
    }
    else
        midColorPtr = sLightColor;

    // Bottom wall section color offset?
    if(side->SW_bottomrgba[0] < 1 || side->SW_bottomrgba[1] < 1 ||
       side->SW_bottomrgba[2] < 1)
    {
        for(i = 0; i < 3; ++i)
            bottomColor[i] = side->SW_bottomrgba[i] * sLightColor[i];

        bottomColorPtr = bottomColor;
    }
    else
        bottomColorPtr = sLightColor;
}

static void lineDistanceAlpha(const float *point, float radius,
                              const float *from, const float *to,
                              float *alpha)
{
    // Calculate 2D distance to line.
    float       distance =  M_PointLineDistance(from, to, point);

    if(radius <= 0)
        radius = 1;

    if(distance < radius)
    {
        // Fade it out the closer the viewPlayer gets and clamp.
        *alpha = (*alpha / radius) * distance;
        *alpha = MINMAX_OF(0, *alpha, 1);
    }
}

static boolean renderSegSection(seg_t *seg, segsection_t section, surface_t *surface,
                                float bottom, float top, float extraTexYOffset,
                                sector_t *frontsec, boolean softSurface,
                                boolean canMask, short sideFlags)
{
    boolean     visible = false, skyMask = false;
    boolean     solidSeg = true;

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
        float       vL_ZTop, vL_ZBottom, vR_ZTop, vR_ZBottom, *vL_XY, *vR_XY;
        short       tempflags = 0;
        float       alpha = (section == SEG_MIDDLE? surface->rgba[3] : 1.0f);
        float       texOffset[2] = {0, 0};
        boolean     inView = true;
        texinfo_t  *texinfo = NULL;

        // Get the start and end vertices, left then right.
        vL_XY = seg->SG_v1pos;
        vR_XY = seg->SG_v2pos;

        // Calculate texture coordinates.
        vL_ZTop = vR_ZTop = top;
        vL_ZBottom = vR_ZBottom = bottom;

        texOffset[VX] = surface->visOffset[VX] + seg->offset;
        texOffset[VY] = surface->visOffset[VY] + extraTexYOffset;

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

            if(mo->subsector->sector == frontsec)
            {
                float               c[2];

                c[VX] = mo->pos[VX];
                c[VY] = mo->pos[VY];

                lineDistanceAlpha(c, mo->radius * .8f,
                                  vL_XY, vR_XY, &alpha);
                if(alpha < 1)
                    solidSeg = false;
            }
        }

        if(inView && alpha > 0)
        {
            rendpoly_t         *quad;
            int                 surfaceFlags;

            // Init the quad.
            quad = R_AllocRendPoly(RP_QUAD, true, 4);
            quad->tex.detail = NULL;
            quad->interTex.detail = NULL;
            quad->wall->length = seg->length;
            quad->flags = 0;
            memcpy(&quad->normal, &surface->normal, sizeof(quad->normal));

            quad->texOffset[VX] = texOffset[VX];
            quad->texOffset[VY] = texOffset[VY];

            // Fill in the remaining quad data.
            if(skyMask || !surface->material)
            {   // We'll mask this.
                quad->flags = RPF_SKY_MASK;
                quad->tex.id = 0;
                quad->lightListIdx = 0;
                quad->interTex.id = 0;
            }
            else
            {
                surfaceFlags = prepareMaterialForPoly(quad, surface, &texinfo);

                if(section == SEG_MIDDLE && softSurface)
                {
                    // Blendmode.
                    if(surface->blendMode == BM_NORMAL && noSpriteTrans)
                        quad->blendMode = BM_ZEROALPHA; // "no translucency" mode
                    else
                        quad->blendMode = surface->blendMode;

                    // If alpha, masked or blended we'll render as a vissprite.
                    if(alpha < 1 || texinfo->masked || surface->blendMode > 0)
                    {
                        quad->flags = RPF_MASKED;
                        solidSeg = false;
                    }
                }
                else
                {
                    quad->blendMode = BM_NORMAL;
                }

                if(solidSeg && !(surfaceFlags & SUF_NO_RADIO))
                    tempflags |= RPF2_SHADOW;
                if(solidSeg && surface->material && !texinfo->masked)
                    tempflags |= RPF2_SHINY;
                if(surfaceFlags & SUF_GLOW)
                    tempflags |= RPF2_GLOW;
                if(!(surface->flags & SUF_TEXFIX))
                    tempflags |= RPF2_BLEND;
            }

            // Bottom Left.
            quad->vertices[0].pos[0] = vL_XY[VX];
            quad->vertices[0].pos[1] = vL_XY[VY];
            quad->vertices[0].pos[2] = vL_ZBottom;

            // Bottom Right.
            quad->vertices[2].pos[0] = vR_XY[VX];
            quad->vertices[2].pos[1] = vR_XY[VY];
            quad->vertices[2].pos[2] = vR_ZBottom;

            // Top Left.
            quad->vertices[1].pos[0] = vL_XY[VX];
            quad->vertices[1].pos[1] = vL_XY[VY];
            quad->vertices[1].pos[2] = vL_ZTop;

            // Top Right.
            quad->vertices[3].pos[0] = vR_XY[VX];
            quad->vertices[3].pos[1] = vR_XY[VY];
            quad->vertices[3].pos[2] = vR_ZTop;

            if(skyMask)
            {
                /**
                 * This poly is destined for the skymask, so we don't need
                 * to do further processing.
                 */
                RL_AddPoly(quad);
            }
            else
            {
                const float    *topColor = NULL;
                const float    *bottomColor = NULL;
                float           sectorLightLevel = Rend_SectorLight(frontsec);

                // Check for neighborhood division?
                if(!(quad->flags & RPF_MASKED))
                    applyWallHeightDivision(quad, seg, frontsec, vL_ZBottom, vL_ZTop);

                getColorsForSegSection(sideFlags, section, &bottomColor, &topColor);

                // Dynamic lights.
                quad->lightListIdx =
                    DL_ProcessSegSection(seg, vL_ZBottom, vL_ZTop,
                                         (quad->flags & RPF_MASKED)? true:false);

                // Make it fullbright?
                if((tempflags & RPF2_GLOW) && glowingTextures)
                    quad->flags |= RPF_GLOW;

                // Surface color/light.
                if(useBias)
                {
                    // Do BIAS lighting for this poly.
                    SB_RendPoly(quad, sectorLightLevel, seg->illum[section],
                                &seg->tracker[section], seg->affected,
                                GET_SEG_IDX(seg));
                }
                else
                {
                    RL_VertexColors(quad, sectorLightLevel, -1, topColor, alpha);

                    // Bottom color (if different from top)?
                    if(bottomColor != NULL)
                    {
                        uint                i;

                        for(i = 0; i < 4; i += 2)
                        {
                            quad->vertices[i].color[CR] = bottomColor[0];
                            quad->vertices[i].color[CG] = bottomColor[1];
                            quad->vertices[i].color[CB] = bottomColor[2];
                        }
                    }
                }

                // Smooth Texture Animation?
                if(tempflags & RPF2_BLEND)
                    polyTexBlend(quad, surface->material);

                // Add the poly to the appropriate list
                RL_AddPoly(quad);

                // Render Fakeradio polys for this seg?
                if(tempflags & RPF2_SHADOW)
                {
                    Rend_RadioUpdateLinedef(seg->lineDef, seg->side);
                    Rend_RadioSegSection(quad, seg->lineDef, seg->side, seg->offset,
                                         seg->length);
                }

                // Render Shiny polys for this seg?
                if((tempflags & RPF2_SHINY) && useShinySurfaces)
                {
                    ded_reflection_t *ref;
                    short width = 0;
                    short height = 0;

                    if((ref = getReflectionDef(surface->material, &width, &height)) != NULL)
                    {
                        // Make sure the texture has been loaded.
                        if(GL_LoadReflectionMap(ref))
                        {
                            // We're going to modify the polygon quite a bit...
                            rendpoly_t *q = R_AllocRendPoly(RP_QUAD, true, 4);
                            R_MemcpyRendPoly(q, quad);
                            Rend_AddShinyPoly(q, ref, width, height);

                            R_FreeRendPoly(q);
                        }
                    }
                }
            }

            R_FreeRendPoly(quad);
        }
    }

    return solidSeg;
}

/**
 * Renders the given single-sided seg into the world.
 */
static boolean Rend_RenderSSWallSeg(seg_t *seg, subsector_t *ssec)
{
    boolean             solidSeg = true;
    sidedef_t          *side;
    linedef_t          *ldef;
    float               ffloor, fceil;
    boolean             backSide;
    sector_t           *frontsec, *fflinkSec, *fclinkSec;
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

    setSurfaceColorsForSide(side);

    // Create the wall sections.

    // Middle section.
    if(side->sections[SEG_MIDDLE].frameFlags & SUFINF_PVIS)
    {
        float       offsetY =
            (ldef->flags & DDLF_DONTPEGBOTTOM)? -(fceil - ffloor) : 0;

        renderSegSection(seg, SEG_MIDDLE, &side->SW_middlesurface, ffloor, fceil,
                         offsetY,
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
static boolean Rend_RenderWallSeg(seg_t *seg, subsector_t *ssec)
{
    int                 solidSeg = false;
    sector_t           *backsec;
    sidedef_t          *backsid, *side;
    linedef_t          *ldef;
    float               ffloor, fceil, bfloor, bceil, bsh;
    boolean             backSide;
    sector_t           *frontsec, *fflinkSec, *fclinkSec;
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

    setSurfaceColorsForSide(side);

    // Create the wall sections.

    // We may need multiple wall sections.
    // Determine which parts of the segment are visible.

    // Middle section.
    if(side->sections[SEG_MIDDLE].frameFlags & SUFINF_PVIS)
    {
        surface_t          *surface = &side->SW_middlesurface;
        texinfo_t          *texinfo = NULL;
        float               texOffsetY = 0;
        float               top, bottom, vL_ZBottom, vR_ZBottom, vL_ZTop, vR_ZTop;
        boolean             softSurface =
            (!(ldef->flags & DDLF_BLOCKING) ||
             !(viewPlayer->shared.flags & DDPF_NOCLIP));

        // We need the properties of the real flat/texture.
        if(surface->material)
        {
            if(surface->material->type == MAT_DDTEX)
                GL_GetMaterialInfo(DDT_UNKNOWN, MAT_DDTEX, &texinfo);
            else
                GL_GetMaterialInfo(surface->material->ofTypeID,
                                   surface->material->type, &texinfo);
        }

        vL_ZBottom = vR_ZBottom = bottom = MAX_OF(bfloor, ffloor);
        vL_ZTop    = vR_ZTop    = top    = MIN_OF(bceil, fceil);
        if(Rend_MidTexturePos
           (&vL_ZBottom, &vR_ZBottom, &vL_ZTop, &vR_ZTop,
            &texOffsetY, surface->visOffset[VY], texinfo->height,
            (ldef->flags & DDLF_DONTPEGBOTTOM)? true : false))
        {
            // Can we make this a soft surface?
            if(vL_ZTop >= top && vL_ZBottom <= bottom &&
               vR_ZTop >= top && vR_ZBottom <= bottom)
            {
                softSurface = true;
            }

            solidSeg = renderSegSection(seg, SEG_MIDDLE, surface,
                                        vL_ZBottom, vL_ZTop, texOffsetY,
                                        /*temp >*/ frontsec, /*< temp*/
                                        softSurface, false, side->flags);
        }
    }

    // Upper section.
    if(side->sections[SEG_TOP].frameFlags & SUFINF_PVIS)
    {
        float       bottom = bceil;
        float       texOffY = 0;

        if(bceil < ffloor)
        {   // Can't go over front ceiling, would induce polygon flaws.
            bottom = ffloor;
        }

        if(!(ldef->flags & DDLF_DONTPEGTOP))
            texOffY += -(fceil - bceil);  // Align with normal middle texture.

        renderSegSection(seg, SEG_TOP, &side->SW_topsurface, bottom, fceil,
                         texOffY, frontsec, false,
                         false, side->flags);
    }

    // Lower section.
    if(side->sections[SEG_BOTTOM].frameFlags & SUFINF_PVIS)
    {
        float       top = bfloor;
        float       texOffY = 0;

        if(bfloor > fceil)
        {   // Can't go over front ceiling, would induce polygon flaws.
            texOffY += bfloor - fceil;
            top = fceil;
        }

        if(ldef->flags & DDLF_DONTPEGBOTTOM)
            texOffY += (fceil - bfloor); // Align with normal middle texture.

        renderSegSection(seg, SEG_BOTTOM, &side->SW_bottomsurface, ffloor, top,
                         texOffY, frontsec,
                         false, false, side->flags);
    }

    // Can we make this a solid segment in the clipper?
    if(solidSeg == -1)
        return false; // NEVER (we have a hole we couldn't fix).

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

    // Apply light adaptation
    return sec->lightLevel + Rend_GetLightAdaptVal(sec->lightLevel);
}

static void Rend_MarkSegsFacingFront(subsector_t *sub)
{
    uint        i;
    seg_t      *seg, **ptr;

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
    float       ffloor, fceil, bfloor, bceil, bsh;
    rendpoly_t *quad;
    float      *vBL, *vBR, *vTL, *vTR;
    sector_t   *frontsec, *backsec;
    uint        j, num;
    seg_t      *seg, **list;
    sidedef_t     *side;

    // Init the quad.
    quad = R_AllocRendPoly(RP_QUAD, true, 4);
    quad->flags = RPF_SKY_MASK;
    quad->lightListIdx = 0;
    quad->tex.id = 0;
    quad->tex.detail = NULL;
    quad->interTex.id = 0;
    quad->interTex.detail = NULL;

    vBL = quad->vertices[0].pos;
    vBR = quad->vertices[2].pos;
    vTL = quad->vertices[1].pos;
    vTR = quad->vertices[3].pos;

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

        quad->wall->length = seg->length;
        memcpy(&quad->normal, &side->SW_middlenormal, sizeof(quad->normal));

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
                    RL_AddPoly(quad);
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

                    RL_AddPoly(quad);
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
                    RL_AddPoly(quad);
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
                    RL_AddPoly(quad);
                }
                // Ensure we add a solid view seg.
                seg->frameFlags |= SEGINF_BACKSECSKYFIX;
            }
        }
    }

    R_FreeRendPoly(quad);
}

/**
 * Creates new occlusion planes from the subsector's sides.
 * Before testing, occlude subsector's backfaces. After testing occlude
 * the remaining faces, i.e. the forward facing segs. This is done before
 * rendering segs, so solid segments cut out all unnecessary oranges.
 */
static void Rend_OccludeSubsector(subsector_t *sub, boolean forward_facing)
{
    float       fronth[2], backh[2];
    float      *startv, *endv;
    sector_t   *front = sub->sector, *back;
    seg_t      *seg, **ptr;

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
           (forward_facing = ((seg->frameFlags & SEGINF_FACINGFRONT)? true : false)))
        {
            back = seg->SG_backsector;
            backh[0] = back->SP_floorheight;
            backh[1] = back->SP_ceilheight;
            // Choose start and end vertices so that it's facing forward.
            if(forward_facing)
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

static void Rend_RenderPlane(subplaneinfo_t *plane, subsector_t *subsector)
{
    sector_t   *sector = subsector->sector;
    int         surfaceFlags;
    float       height;
    surface_t  *surface;
    sector_t   *polySector;
    float       vec[3];

    polySector = R_GetLinkedSector(subsector, plane->planeID);
    surface = &polySector->planes[plane->planeID]->surface;

    // Must have a visible surface.
    if(!surface->material)
        return;

    // We don't render planes for unclosed sectors when the polys would
    // be added to the skymask (a DOOM.EXE renderer hack).
    if((sector->flags & SECF_UNCLOSED) && R_IsSkySurface(surface))
        return;

    // Determine plane height.
    height = polySector->SP_planevisheight(plane->planeID);
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
        texinfo_t          *texInfo;
        rendpoly_t         *poly =
            R_AllocRendPoly(RP_FLAT, false, subsector->numVertices);

        surfaceFlags = prepareMaterialForPoly(poly, surface, &texInfo);

        // Fill in the remaining quad data.
        poly->flags = 0;
        memcpy(&poly->normal, &surface->normal, sizeof(poly->normal));

        // Check for sky.
        if(R_IsSkySurface(surface))
        {
            poly->flags |= RPF_SKY_MASK;
        }
        else
        {
            poly->texOffset[VX] =
                sector->SP_plane(plane->planeID)->PS_visoffset[VX];
            poly->texOffset[VY] =
                sector->SP_plane(plane->planeID)->PS_visoffset[VY];

            if(surface->material && !texInfo->masked)
                tempflags |= RPF2_SHINY;
            if(surfaceFlags & SUF_GLOW)
                tempflags |= RPF2_GLOW;
            if(!(surface->flags & SUF_TEXFIX))
                tempflags |= RPF2_BLEND;
        }

        doRenderPlane(poly, polySector, subsector, plane, surface,
                      height, tempflags);

        R_FreeRendPoly(poly);
    }
}

static void Rend_RenderSubsector(uint ssecidx)
{
    uint                i;
    subsector_t        *ssec = SUBSECTOR_PTR(ssecidx);
    seg_t              *seg, **ptr;
    sector_t           *sect = ssec->sector;
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

    // Prepare for FakeRadio.
    Rend_RadioInitForSubsector(ssec);
    Rend_RadioSubsectorEdges(ssec);

    Rend_OccludeSubsector(ssec, false);
    LO_ClipInSubsector(ssecidx);
    Rend_OccludeSubsector(ssec, true);

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
        Rend_RenderPlane(ssec->planes[i], ssec);
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

    DGL_Bind(GL_PrepareMaterial(R_GetMaterial(DDT_BBOX, MAT_DDTEX), NULL));
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
