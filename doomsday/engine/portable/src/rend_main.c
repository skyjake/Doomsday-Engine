/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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

/*
 * rend_main.c: Rendering Subsystem
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

// MACROS ------------------------------------------------------------------

#define ROUND(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))

// TYPES -------------------------------------------------------------------

typedef struct {
    float       value;
    float       currentlight;
    sector_t   *sector;
    uint        updateTime;
} lightsample_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void Rend_RenderBoundingBoxes(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int useDynLights, translucentIceCorpse;
extern int skyhemispheres, haloMode;
extern int dlMaxRad;
extern int devNoCulling;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean usingFog = false;         // Is the fog in use?
byte    fogColor[4];
float   fieldOfView = 95.0f;
float   maxLightDist = 1024;
byte    smoothTexAnim = true;
int     useShinySurfaces = true;

float   vx, vy, vz, vang, vpitch;
float   viewsidex, viewsidey;

boolean willRenderSprites = true;
boolean freezeRLs = false;

int     missileBlend = 1;
int     litSprites = 1;
// Ambient lighting, r_ambient is used within the renderer, ambientLight is
// used to store the value of the ambient light cvar. ambientLight is used
// The value chosen for r_ambient occurs in Rend_CalcLightRangeModMatrix
// for convenience (since we would have to recalculate the matrix anyway).
int     r_ambient = 0, ambientLight = 0;

int     viewpw, viewph;         // Viewport size, in pixels.
int     viewpx, viewpy;         // Viewpoint top left corner, in pixels.

float   yfov;

int     gamedrawhud = 1;    // Set to zero when we advise that the HUD
                            // should not be drawn

uint    playerLightRange[MAXPLAYERS];

lightsample_t playerLastLightSample[MAXPLAYERS];

float   r_lightAdapt = 0.8f; // Amount of light adaption
int     r_lightAdaptDarkTime = 80;
int     r_lightAdaptBrightTime = 8;

float   lightRangeCompression = 0;

float   lightRangeModMatrix[MOD_RANGE][255];
float   lightRangeAdaptRamp = 0.2f;

int     debugLightModMatrix = 0;
int     devMobjBBox = 0; // 1 = Draw mobj bounding boxes (for debug)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstsubsector;  // No range checking for the first one.

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
    C_VAR_INT("rend-dev-freeze", &freezeRLs, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-cull-subsectors", &devNoCulling,CVF_NO_ARCHIVE,0,1);
    C_VAR_INT("rend-dev-mobj-bbox", &devMobjBBox, 0, 0, 1);
    C_VAR_FLOAT("rend-camera-fov", &fieldOfView, 0, 1, 179);
    C_VAR_BYTE("rend-tex-anim-smooth", &smoothTexAnim, 0, 0, 1);
    C_VAR_INT("rend-tex-shiny", &useShinySurfaces, 0, 0, 1);
    C_VAR_FLOAT2("rend-light-compression", &lightRangeCompression, 0, -1, 1,
                 Rend_CalcLightRangeModMatrix);
    C_VAR_FLOAT("rend-light-adaptation", &r_lightAdapt, 0, 0, 1);
    C_VAR_FLOAT2("rend-light-adaptation-ramp", &lightRangeAdaptRamp,
                 CVF_PROTECTED, -1, 1, Rend_CalcLightRangeModMatrix);
    C_VAR_INT("rend-light-adaptation-darktime", &r_lightAdaptDarkTime,
              0, 0, 200);
    C_VAR_INT("rend-light-adaptation-brighttime", &r_lightAdaptBrightTime,
              0, 0, 200);
    C_VAR_INT2("rend-light-ambient", &ambientLight, 0, 0, 255,
               Rend_CalcLightRangeModMatrix);
    C_VAR_INT("rend-light-sky", &rendSkyLight, 0, 0, 1);
    C_VAR_FLOAT("rend-light-wall-angle", &rend_light_wall_angle, CVF_NO_MAX,
                0, 0);
    C_VAR_INT("rend-dev-light-modmatrix", &debugLightModMatrix,
              CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-tex-showfix", &devNoTexFix, 0, 0, 1);
    C_VAR_BYTE("rend-dev-surface-linked", &devNoLinkedSurfaces, 0, 0, 1);

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
float Rend_PointDist3D(float c[3])
{
    return M_ApproxDistance3f(vx - c[VX], vz - c[VY], 1.2f * (vy - c[VZ]));
}

void Rend_Init(void)
{
    C_Init();                   // Clipper.
    RL_Init();                  // Rendering lists.
    Rend_InitSky();             // The sky.
    Rend_CalcLightRangeModMatrix(NULL);
}

/**
 * Used to be called before starting a new level.
 */
void Rend_Reset(void)
{
    // Textures are deleted (at least skies need this???).
    GL_ClearRuntimeTextures();
    DL_Clear();
}

void Rend_ModelViewMatrix(boolean useAngles)
{
    vx = FIX2FLT(viewx);
    vy = FIX2FLT(viewz);
    vz = FIX2FLT(viewy);
    vang = viewangle / (float) ANGLE_MAX *360 - 90;
    vpitch = viewpitch * 85.0 / 110.0;

    gl.MatrixMode(DGL_MODELVIEW);
    gl.LoadIdentity();
    if(useAngles)
    {
        gl.Rotatef(vpitch, 1, 0, 0);
        gl.Rotatef(vang, 0, 1, 0);
    }
    gl.Scalef(1, 1.2f, 1);      // This is the aspect correction.
    gl.Translatef(-vx, -vy, -vz);
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
    real += extralight / 16.0f;

    // Check for torch.
    if(viewplayer->fixedcolormap)
    {
        // Colormap 1 is the brightest. I'm guessing 16 would be the darkest.
        newmin =
            ((1024 - distance) / 512) * (16 -
                                         viewplayer->fixedcolormap) / 15.0f;
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

static __inline int Rend_SegFacingDir(float v1[2], float v2[2])
{
    // The dot product. (1 if facing front.)
    return (v1[VY] - v2[VY]) * (v1[VX] - vx) + (v2[VX] - v1[VX]) * (v1[VY] -
                                                                    vz) > 0;
}

#if 0 // unused atm
static int Rend_FixedSegFacingDir(const seg_t *seg)
{
    // The dot product. (1 if facing front.)
    return((seg->fv1.pos[VY] - seg->fv2.pos[VY]) * (seg->fv1.pos[VX] - viewx) +
           (seg->fv2.pos[VX] - seg->fv1.pos[VX]) * (seg->fv1.pos[VY] - viewy)) > 0;
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

static void Rend_ShinySurfaceColor(gl_rgba_t *color, ded_reflection_t *ref)
{
    uint        i;

    for(i = 0; i < 3; ++i)
    {
        DGLubyte minimum = (DGLubyte) (ref->min_color[i] * 255);

        if(color->rgba[i] < minimum)
        {
            color->rgba[i] = minimum;
        }
    }

    color->rgba[3] = (DGLubyte) (ref->shininess * 255);
/*
Con_Printf("shiny = %i %i %i %i\n",
           (int) color->rgba[0], (int) color->rgba[1], (int) color->rgba[2],
           (int) color->rgba[3]);
*/
}

static ded_reflection_t *Rend_ShinyDefForSurface(surface_t *suf, short *width,
                                                 short *height)
{
    ded_reflection_t *ref = NULL;

    // Figure out what kind of surface properties have been defined
    // for the texture or flat in question.
    if(suf->SM_isflat)
    {
        flat_t *flatptr;

        if(suf->SM_xlat)
        {
            // The flat is currently translated to another one.
            flatptr = flats[suf->SM_xlat->current];
        }
        else
        {
            flatptr = flats[suf->SM_texture];
        }

        ref = flatptr->reflection;
        if(ref)
        {
            if(width)
                *width = flatptr->info.width;
            if(height)
                *height = flatptr->info.height;
        }
    }
    else
    {
        texture_t *texptr;

        if(suf->SM_xlat)
        {
            texptr = textures[suf->SM_xlat->current];
        }
        else
        {
            texptr = textures[suf->SM_texture];
        }

        ref = texptr->reflection;
        if(ref)
        {
            if(width)
                *width = texptr->info.width;
            if(height)
                *height = texptr->info.height;
        }
    }

    return ref;
}

/**
 * Pre: As we modify param poly quite a bit it is the responsibility of
 *      the caller to ensure this is OK (ie if not it should pass us a
 *      copy instead (eg wall segs)).
 *
 * @param   texture     The texture/flat id of the texture on the poly.
 * @param   isFlat      (TRUE) = param texture is a flat.
 * @param   poly        The poly to add the shiny poly for.
 */
static void Rend_AddShinyPoly(rendpoly_t *poly, ded_reflection_t *ref,
                              short width, short height)
{
    uint        i;

    // Make it a shiny polygon.
    poly->lights = NULL;
    poly->flags |= RPF_SHINY;
    poly->blendmode = ref->blend_mode;
    poly->tex.id = ref->use_shiny->shiny_tex;
    poly->tex.detail = NULL;
    poly->tex.masked = false;
    poly->intertex.detail = NULL;
    poly->intertex.masked = false;
    poly->interpos = 0;

    // Strength of the shine.
    for(i = 0; i < poly->numvertices; ++i)
    {
        Rend_ShinySurfaceColor(&poly->vertices[i].color, ref);
    }

    // The mask texture is stored in the intertex.
    if(ref->use_mask && ref->use_mask->mask_tex)
    {
        poly->intertex.id = ref->use_mask->mask_tex;
        poly->tex.width = poly->intertex.width = width * ref->mask_width;
        poly->tex.height = poly->intertex.height = height * ref->mask_height;
    }
    else
    {
        // No mask.
        poly->intertex.id = 0;
    }

    RL_AddPoly(poly);
}

static void Rend_PolyTexBlend(surface_t *surface, rendpoly_t *poly,
                              boolean enabled)
{
    texinfo_t      *texinfo;
    translation_t *xlat = surface->SM_xlat;

    // If fog is active, inter=0 is accepted as well. Otherwise flickering
    // may occur if the rendering passes don't match for blended and
    // unblended surfaces.
    if(!enabled || !xlat || !smoothTexAnim || numTexUnits < 2 ||
       xlat->current == xlat->next || (!usingFog && xlat->inter < 0) ||
       ((surface->flags & SUF_TEXFIX) && devNoTexFix) ||
       renderTextures == 2)
    {
        // No blending for you, my friend.
        memset(&poly->intertex, 0, sizeof(poly->intertex));
        poly->interpos = 0;
        return;
    }

    // Get info of the blend target.
    if(surface->SM_isflat)
        poly->intertex.id = GL_PrepareFlat2(xlat->next, false, &texinfo);
    else
        poly->intertex.id = GL_PrepareTexture2(xlat->next, false, &texinfo);

    poly->intertex.width = texinfo->width;
    poly->intertex.height = texinfo->height;
    poly->intertex.detail = (r_detail && texinfo->detail.tex? &texinfo->detail : 0);
    poly->intertex.masked = texinfo->masked;
    poly->interpos = xlat->inter;
}

/**
 * \fixme No need to do this each frame. Set a flag in side_t->flags to
 * denote this. Is sensitive to plane heights, surface properties
 * (e.g. alpha) and surface texture properties.
 */
boolean Rend_DoesMidTextureFillGap(line_t *line, int backside)
{
    // Check for unmasked midtextures on twosided lines that completely
    // fill the gap between floor and ceiling (we don't want to give away
    // the location of any secret areas (false walls)).
    if(line->L_backside)
    {
        sector_t *front = line->L_sector(backside);
        sector_t *back  = line->L_sector(backside^1);
        side_t   *side  = line->L_side(backside);

        if(side->SW_middletexture != 0)
        {
            texinfo_t      *texinfo = NULL;

            if(side->SW_middletexture > 0)
            {
                if(side->SW_middleisflat)
                    GL_GetFlatInfo(side->SW_middletexture, &texinfo);
                else
                    GL_GetTextureInfo(side->SW_middletexture, &texinfo);
            }
            else // It's a DDay texture.
            {
                GL_PrepareDDTexture(side->SW_middletexture, &texinfo);
            }

            if(!side->SW_middleblendmode && side->SW_middlergba[3] >= 1 && !texinfo->masked)
            {
                float openTop[2], gapTop[2];
                float openBottom[2], gapBottom[2];

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
                        NULL, side->SW_middleoffy, texinfo->height,
                        0 != (line->mapflags & ML_DONTPEGBOTTOM)))
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
    uint        i, j;
    side_t     *side;
    line_t     *line;

    if(!seg || !seg->linedef)
        return; // huh?

    line = seg->linedef;
    for(i = 0; i < 2; ++i)
    {
        // Missing side?
        if(!line->L_side(i))
            continue;

        side = line->L_side(i);
        for(j = 0; j < 3; ++j)
            side->sections[j].frameflags |= SUFINF_PVIS;

        // A two sided line?
        if(line->L_frontside && line->L_backside)
        {
            // Check middle texture
            if(!(side->SW_middletexture || side->SW_middletexture == -1) ||
               side->SW_middlergba[3] <= 0) // Check alpha
                side->sections[SEG_MIDDLE].frameflags &= ~SUFINF_PVIS;
        }

        // Top
        if(!line->L_backside)
        {
            side->sections[SEG_TOP].frameflags &= ~SUFINF_PVIS;
            side->sections[SEG_BOTTOM].frameflags &= ~SUFINF_PVIS;
        }
        else
        {
            if(R_IsSkySurface(&line->L_backsector->SP_ceilsurface) &&
               R_IsSkySurface(&line->L_frontsector->SP_ceilsurface))
               side->sections[SEG_TOP].frameflags &= ~SUFINF_PVIS;
            else
            {
                if(i != 0)
                {
                    if(line->L_backsector->SP_ceilvisheight <=
                       line->L_frontsector->SP_ceilvisheight)
                        side->sections[SEG_TOP].frameflags &= ~SUFINF_PVIS;
                }
                else
                {
                    if(line->L_frontsector->SP_ceilvisheight <=
                       line->L_backsector->SP_ceilvisheight)
                        side->sections[SEG_TOP].frameflags &= ~SUFINF_PVIS;
                }
            }

            // Bottom
            if(R_IsSkySurface(&line->L_backsector->SP_floorsurface) &&
               R_IsSkySurface(&line->L_frontsector->SP_floorsurface))
               side->sections[SEG_BOTTOM].frameflags &= ~SUFINF_PVIS;
            else
            {
                if(i != 0)
                {
                    if(line->L_backsector->SP_floorvisheight >=
                       line->L_frontsector->SP_floorvisheight)
                        side->sections[SEG_BOTTOM].frameflags &= ~SUFINF_PVIS;
                }
                else
                {
                    if(line->L_frontsector->SP_floorvisheight >=
                       line->L_backsector->SP_floorvisheight)
                        side->sections[SEG_BOTTOM].frameflags &= ~SUFINF_PVIS;
                }
            }
        }
    }
}

/**
 * Prepares the correct flat/texture for the passed poly.
 *
 * @param poly:     Ptr to the poly to apply the texture to.
 * @param surface:  Ptr to the surface properties this rendpoly is using.
 * @return int      (-1) If the texture reference is invalid.
 *                  Else return the surface flags for this poly.
 */
static int Rend_PrepareTextureForPoly(rendpoly_t *poly, surface_t *surface,
                                      texinfo_t **texinfo)
{
    texinfo_t   *info;
    int         flags;

    if(R_IsSkySurface(surface))
        return 0;

    // Prepare the flat/texture
    if(renderTextures == 2)
    {   // For lighting debug, render all solid surfaces using the gray texture.
        poly->tex.id = curtex = GL_PrepareDDTexture(DDT_GRAY, NULL);

        // We need the properties of the real flat/texture.
        if(surface->SM_isflat)
            GL_GetFlatInfo(surface->SM_texture, &info);
        else
            GL_GetTextureInfo(surface->SM_texture, &info);

        flags = surface->flags & ~(SUF_TEXFIX|SUF_BLEND);
    }
    else if((surface->flags & SUF_TEXFIX) && devNoTexFix)
    {   // For debug, render the "missing" texture instead of the texture
        // chosen for surfaces to fix the HOMs.
        poly->tex.id = curtex = GL_PrepareDDTexture(DDT_MISSING, &info);
        flags = SUF_GLOW; // Make it stand out
    }
    else if(surface->SM_texture == -1)
    {   // An unknown texture. The "unknown" graphic will be used
        // NOTE: It has already been bound and paramaters set.
        return SUF_GLOW; // Make it stand out
    }
    else
    {
        if(surface->SM_isflat)
            poly->tex.id = curtex =
                GL_PrepareFlat2(surface->SM_texture, true, &info);
        else
            poly->tex.id = curtex =
                GL_PrepareTexture2(surface->SM_texture, true, &info);

        flags = surface->flags;
    }

    poly->tex.width = info->width;
    poly->tex.height = info->height;
    poly->tex.detail = (r_detail && info->detail.tex? &info->detail : 0);
    poly->tex.masked = info->masked;

    if(info->masked)
        flags &= ~SUF_NO_RADIO;

    if(texinfo)
        *texinfo = info;

    // Return the parameters for this surface.
    return flags;
}

/**
 * @return          <code>true</code> if the quad has a division at the
 *                  specified height.
 */
static int Rend_CheckDiv(rendpoly_t *quad, int side, float height)
{
    uint        i, num;
    walldiv_t  *div;

    div = &quad->wall->divs[side];
    num = div->num;
    for(i = 0; i < num; ++i)
        if(div->pos[i] == height)
            return true;

    return false;
}

/**
 * Division will only happen if it must be done.
 * Converts quads to divquads.
 */
static void Rend_WallHeightDivision(rendpoly_t *quad, const seg_t *seg,
                                    sector_t *frontsec, segsection_t mode)
{
    uint        i, k;
    vertex_t   *vtx;
    sector_t   *sec;
    float       hi, low;
    float       sceil, sfloor;
    walldiv_t  *div;

    switch(mode)
    {
    case SEG_MIDDLE:
        hi = frontsec->SP_ceilvisheight;
        low = frontsec->SP_floorvisheight;
        break;

    case SEG_TOP:
        hi = frontsec->SP_ceilvisheight;
        low = seg->SG_backsector->SP_ceilvisheight;
        if(frontsec->SP_floorvisheight > low)
            low = frontsec->SP_floorvisheight;
        break;

    case SEG_BOTTOM:
        hi = seg->SG_backsector->SP_floorvisheight;
        low = frontsec->SP_floorvisheight;
        if(frontsec->SP_ceilvisheight < hi)
            hi = frontsec->SP_ceilvisheight;
        break;

    default:
        return;
    }

    quad->wall->divs[0].num = 0;
    quad->wall->divs[1].num = 0;

    // Check both ends.
    for(i = 0; i < 2; ++i)
    {
        vtx = seg->v[i];
        div = &quad->wall->divs[i];
        if(vtx->numsecowners > 1)
        {
            boolean isDone;

            // More than one sectors! The checks must be made.
            isDone = false;
            for(k = 0; !isDone && k < vtx->numsecowners; ++k)
            {
                sec = SECTOR_PTR(vtx->secowners[k]);
                if(!(sec == frontsec || sec == seg->SG_backsector))
                {
                    sceil = sec->SP_ceilvisheight;
                    sfloor = sec->SP_floorvisheight;

                    // Divide at the sector's ceiling height?
                    if(sceil > low && sceil < hi)
                    {
                        quad->type = RP_DIVQUAD;
                        if(!Rend_CheckDiv(quad, i, sceil))
                            div->pos[div->num++] = sceil;
                    }
                    // Have we reached the div limit?
                    if(div->num == RL_MAX_DIVS)
                        isDone = true;
                    
                    if(!isDone)
                    {
                        // Divide at the sector's floor height?
                        if(sfloor > low && sfloor < hi)
                        {
                            quad->type = RP_DIVQUAD;
                            if(!Rend_CheckDiv(quad, i, sfloor))
                                div->pos[div->num++] = sfloor;
                        }
                        // Have we reached the div limit?
                        if(div->num == RL_MAX_DIVS)
                            isDone = true;
                    }
                }
            }

            // We need to sort the divisions for the renderer.
            if(div->num > 1)
            {
                // Sorting is required. This shouldn't take too long...
                // There seldom are more than one or two divisions.
                qsort(div->pos, div->num, sizeof(float),
                      i!=0 ? DivSortDescend : DivSortAscend);
            }
#ifdef RANGECHECK
for(k = 0; k < div->num; ++k)
    if(div->pos[k] > hi || div->pos[k] < low)
    {
        Con_Error("DivQuad: i=%i, pos (%f), hi (%f), low (%f), num=%i\n",
                  i, div->pos[k], hi, low, div->num);
    }
#endif
        }
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

#define RPF2_SHADOW 0x0001
#define RPF2_SHINY  0x0002
#define RPF2_GLOW   0x0004
#define RPF2_BLEND  0x0008

/**
 * Called by Rend_RenderWallSeg to render ALL wall sections.
 */
static void Rend_RenderWallSection(rendpoly_t *quad, seg_t *seg, side_t *side,
                                   sector_t *frontsec, surface_t *surface,
                                   segsection_t section, float alpha, short flags)
{
    uint        i;
    uint        segIndex = GET_SEG_IDX(seg);
    const float *colorPtr = NULL;
    const float *color2Ptr = NULL;

    if(alpha <= 0)
        return; // no point rendering transparent polys...

    // Select the correct colors for this surface.
    switch(section)
    {
    case SEG_MIDDLE:
        if(side->flags & SDF_BLENDMIDTOTOP)
        {
            colorPtr = topColorPtr;
            color2Ptr = midColorPtr;
        }
        else if(side->flags & SDF_BLENDMIDTOBOTTOM)
        {
            colorPtr = midColorPtr;
            color2Ptr = bottomColorPtr;
        }
        else
        {
            colorPtr = midColorPtr;
            color2Ptr = NULL;
        }
        break;

    case SEG_TOP:
        if(side->flags & SDF_BLENDTOPTOMID)
        {
            colorPtr = topColorPtr;
            color2Ptr = midColorPtr;
        }
        else
        {
            colorPtr = topColorPtr;
            color2Ptr = NULL;
        }
        break;

    case SEG_BOTTOM:
        // Select the correct colors for this surface.
        if(side->flags & SDF_BLENDBOTTOMTOMID)
        {
            colorPtr = midColorPtr;
            color2Ptr = bottomColorPtr;
        }
        else
        {
            colorPtr = bottomColorPtr;
            color2Ptr = NULL;
        }
        break;
    default:
        Con_Error("Rend_RenderWallSection: Invalid wall section %i", section);
    }

    // Make it fullbright?
    if((flags & RPF2_GLOW) && glowingTextures)
        quad->flags |= RPF_GLOW;

    // Surface color/light.
    RL_VertexColors(quad, Rend_SectorLight(frontsec), -1, colorPtr, alpha);

    // Bottom color (if different from top)?
    if(color2Ptr != NULL)
    {
        for(i = 0; i < 4; i += 2)
        {
            quad->vertices[i].color.rgba[0] = (DGLubyte) (255 * color2Ptr[0]);
            quad->vertices[i].color.rgba[1] = (DGLubyte) (255 * color2Ptr[1]);
            quad->vertices[i].color.rgba[2] = (DGLubyte) (255 * color2Ptr[2]);
        }
    }

    // Dynamic lights.
    quad->lights = DL_GetSegSectionLightLinks(segIndex, section);

    // Do BIAS lighting for this poly.
    SB_RendPoly(quad, surface, frontsec, seg->illum[section],
                &seg->tracker[section],  seg->affected,
                segIndex);

    // Smooth Texture Animation?
    Rend_PolyTexBlend(surface, quad, (flags & RPF2_BLEND));

    // Add the poly to the appropriate list
    RL_AddPoly(quad);

    // Render Fakeradio polys for this seg?
    if(flags & RPF2_SHADOW)
        Rend_RadioWallSection(seg, quad);

    // Render Shiny polys for this seg?
    if((flags & RPF2_SHINY) && useShinySurfaces)
    {
        ded_reflection_t *ref;
        short width = 0;
        short height = 0;

        if((ref = Rend_ShinyDefForSurface(surface, &width, &height)) != NULL)
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

/**
 * Same as above but for planes. Ultimately we should consider merging the
 * two into a Rend_RenderSurface.
 */
static void Rend_DoRenderPlane(rendpoly_t *poly, sector_t *polySector,
                               subsector_t *subsector,
                               subplaneinfo_t* plane, surface_t *surface,
                               float height, short flags)
{
    uint        subIndex = GET_SUBSECTOR_IDX(subsector);

    if((flags & RPF2_GLOW) && glowingTextures)        // Make it fullbright?
        poly->flags |= RPF_GLOW;

    // Surface color/light.
    RL_PreparePlane(plane, poly, height, subsector,
                    Rend_SectorLight(polySector),
                    R_GetSectorLightColor(polySector), surface->rgba);

    // Dynamic lights. Check for sky.
    if(R_IsSkySurface(surface))
    {
        poly->lights = NULL;
        skyhemispheres |= (plane->type == PLN_FLOOR? SKYHEMI_LOWER : SKYHEMI_UPPER);
    }
    else
        poly->lights = DL_GetSubSecPlaneLightLinks(subIndex, plane->type);

    // Do BIAS lighting for this poly.
    SB_RendPoly(poly, surface, subsector->sector, plane->illumination,
                &plane->tracker, plane->affected, subIndex);

    // Smooth Texture Animation?
    Rend_PolyTexBlend(surface, poly, (flags & RPF2_BLEND));

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

        if((ref = Rend_ShinyDefForSurface(surface, &width, &height)) != NULL)
        {
            // Make sure the texture has been loaded.
            if(GL_LoadReflectionMap(ref))
            {
                Rend_AddShinyPoly(poly, ref, width, height);
            }
        }
    }
}

static void Rend_SetSurfaceColorsForSide(side_t *side)
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

/**
 * Renders the given single-sided seg into the world.
 */
static void Rend_RenderSSWallSeg(seg_t *seg, subsector_t *ssec)
{
    int         surfaceFlags;
    surface_t  *surface;
    side_t     *side;
    line_t     *ldef;
    float       ffloor, fceil, fsh;
    rendpoly_t *quad;
    float       vL_ZTop, vL_ZBottom, vR_ZTop, vR_ZBottom, *vL_XY, *vR_XY;
    boolean     backSide;
    sector_t   *frontsec, *fflinkSec, *fclinkSec;
    int         pid = viewplayer - players;

    frontsec = seg->sidedef->sector;
    side = seg->sidedef;
    backSide = seg->side;
    ldef = seg->linedef;

    if(!ldef->mapped[pid])
    {
        ldef->mapped[pid] = true; // This line is now seen in the map.

        // Send a status report.
        if(gx.HandleMapObjectStatusReport)
            gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED,
                                           GET_LINE_IDX(ldef),
                                           DMU_LINE, &pid);
    }

    fflinkSec = R_GetLinkedSector(ssec, PLN_FLOOR);
    ffloor = fflinkSec->SP_floorvisheight;
    fclinkSec = R_GetLinkedSector(ssec, PLN_CEILING);
    fceil = fclinkSec->SP_ceilvisheight;
    fsh = fceil - ffloor;

    // Init the quad.
    quad = R_AllocRendPoly(RP_QUAD, true, 4);
    quad->flags = 0;
    quad->tex.detail = NULL;
    quad->intertex.detail = NULL;

    // Get the start and end vertices, left then right.
    vL_XY = seg->SG_v1pos;
    vR_XY = seg->SG_v2pos;

    vL_ZTop = vR_ZTop = fceil;
    vL_ZBottom = vR_ZBottom = ffloor;

    quad->wall->length = seg->length;

    surface = &side->SW_middlesurface;
    // Is there a visible surface?
    if(surface->SM_texture != 0)
    {
        texinfo_t      *texinfo = NULL;
        short           tempflags = 0;

        Rend_SetSurfaceColorsForSide(side);

        // Check for neighborhood division?
        Rend_WallHeightDivision(quad, seg, frontsec, SEG_MIDDLE);

        // Fill in the remaining quad data.
        quad->flags = 0;
        quad->texoffx = side->SW_middleoffx + seg->offset;
        quad->texoffy = side->SW_middleoffy;
 
        surfaceFlags = Rend_PrepareTextureForPoly(quad, surface, &texinfo);

        if(ldef->mapflags & ML_DONTPEGBOTTOM)
            quad->texoffy += texinfo->height - fsh;

        if(!(surfaceFlags & SUF_NO_RADIO))
            tempflags |= RPF2_SHADOW;
        if(surface->SM_texture != -1)
            tempflags |= RPF2_SHINY;
        if(surfaceFlags & SUF_GLOW)
            tempflags |= RPF2_GLOW;
        if(surfaceFlags & SUF_BLEND)
            tempflags |= RPF2_BLEND;

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

        Rend_RenderWallSection(quad, seg, side, frontsec, surface, SEG_MIDDLE,
                  /*Alpha*/    1, tempflags);
    }
    else
    {
        if(R_IsSkySurface(&frontsec->SP_ceilsurface) &&
           R_IsSkySurface(&frontsec->SP_floorsurface))
        {   // We'll mask this.
            quad->flags = RPF_SKY_MASK;
            quad->tex.id = 0;
            quad->lights = NULL;
            quad->intertex.id = 0;

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

            RL_AddPoly(quad);
        }
    }

    // We KNOW we can make it solid.
    if(!P_IsInVoid(viewplayer))
        C_AddViewRelSeg(vL_XY[VX], vL_XY[VY], vR_XY[VX], vR_XY[VY]);

    R_FreeRendPoly(quad);
}

/**
 * Renders wall sections for given two-sided seg.
 */
static void Rend_RenderWallSeg(seg_t *seg, subsector_t *ssec)
{
    int         solidSeg = false; // -1 means NEVER.
    sector_t   *backsec;
    surface_t  *surface;
    side_t     *backsid, *side;
    line_t     *ldef;
    float       ffloor, fceil, bfloor, bceil, fsh, bsh;
    float       vL_ZTop, vL_ZBottom, vR_ZTop, vR_ZBottom, *vL_XY, *vR_XY;
    boolean     backSide;
    sector_t   *frontsec, *fflinkSec, *fclinkSec;
    int         pid = viewplayer - players;

    frontsec = seg->sidedef->sector;
    backsec = seg->backseg->sidedef->sector;
    backsid = seg->backseg->sidedef;
    side = seg->sidedef;
    backSide = seg->side;
    ldef = seg->linedef;

    if(!ldef->mapped[pid])
    {
        ldef->mapped[pid] = true; // This line is now seen in the map.

        // Send a status report.
        if(gx.HandleMapObjectStatusReport)
            gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED,
                                           GET_LINE_IDX(ldef),
                                           DMU_LINE, &pid);
    }

    if(backsec == frontsec &&
       side->SW_toptexture == 0 && side->SW_bottomtexture == 0 &&
       side->SW_middletexture == 0)
       return; // Ugh... an obvious wall seg hack. Best take no chances...

    fflinkSec = R_GetLinkedSector(ssec, PLN_FLOOR);
    ffloor = fflinkSec->SP_floorvisheight;
    fclinkSec = R_GetLinkedSector(ssec, PLN_CEILING);
    fceil = fclinkSec->SP_ceilvisheight;
    fsh = fceil - ffloor;

    bceil = backsec->SP_ceilvisheight;
    bfloor = backsec->SP_floorvisheight;
    bsh = bceil - bfloor;

    // Get the start and end vertices, left then right.
    vL_XY = seg->SG_v1pos;
    vR_XY = seg->SG_v2pos;

    Rend_SetSurfaceColorsForSide(side);

    // Create the wall sections.

    // We may need multiple wall sections.
    // Determine which parts of the segment are visible.

    // Quite probably a masked texture. Won't be drawn if a visible
    // top or bottom texture is missing.
    if((side->sections[SEG_MIDDLE].frameflags & SUFINF_PVIS) /*&&
       !(side->flags & SDF_MIDTEXUPPER)*/)
    {
        float   gaptop, gapbottom;
        float   alpha = side->SW_middlergba[3];

        surface = &side->SW_middlesurface;
        // Is there a visible surface?
        if(surface->SM_texture != 0)
        {
            float texOffY = 0;
            texinfo_t *texinfo = NULL;

            //if(ldef - lines == 497)
            //    Con_Message("Me\n");

            // Calculate texture coordinates.
            vL_ZTop = vR_ZTop = gaptop = MIN_OF(bceil, fceil);
            vL_ZBottom = vR_ZBottom = gapbottom = MAX_OF(bfloor, ffloor);

            // We need the properties of the real flat/texture.
            if(surface->SM_texture < 0)
                GL_PrepareDDTexture(side->SW_middletexture, &texinfo);
            else if(side->SW_middleisflat)
                GL_GetFlatInfo(side->SW_middletexture, &texinfo);
            else
                GL_GetTextureInfo(side->SW_middletexture, &texinfo);

            if(Rend_MidTexturePos
               (&vL_ZBottom, &vR_ZBottom, &vL_ZTop, &vR_ZTop,
                &texOffY, side->SW_middleoffy, texinfo->height,
                0 != (ldef->mapflags & ML_DONTPEGBOTTOM)))
            {
                // Should a solid segment be added here?
                if(vL_ZTop >= gaptop && vL_ZBottom <= gapbottom &&
                   vR_ZTop >= gaptop && vR_ZBottom <= gapbottom)
                {
                    if(!texinfo->masked && alpha >= 1 && side->SW_middleblendmode == 0)
                        solidSeg = true; // We could add clipping seg.

                    // Can the player walk through this surface?
                    if(!(ldef->mapflags & ML_BLOCKING) ||
                       !(viewplayer->flags & DDPF_NOCLIP))
                    {
                        mobj_t* mo = viewplayer->mo;
                        // If the player is close enough we should NOT add a solid seg
                        // otherwise they'd get HOM when they are directly on top of the
                        // line (eg passing through an opaque waterfall).
                        if(mo->subsector->sector == ssec->sector)
                        {
                            // Calculate 2D distance to line.
                            float c[2];
                            float distance;

                            c[VX] = FIX2FLT(mo->pos[VX]);
                            c[VY] = FIX2FLT(mo->pos[VY]);
                            distance =
                                M_PointLineDistance(vL_XY, vR_XY, c);

                            if(distance < (FIX2FLT(mo->radius)*.8f))
                            {
                                float temp = 0;

                                // Fade it out the closer the viewplayer gets.
                                solidSeg = false;
                                temp = (alpha / (FIX2FLT(mo->radius)*.8f));
                                temp *= distance;

                                // Clamp it.
                                if(temp > 1)
                                    temp = 1;
                                if(temp < 0)
                                    temp = 0;

                                alpha = temp;
                            }
                        }
                    }
                }

                {
                    short       tempflags = 0;
                    rendpoly_t *quad;
                    int         surfaceFlags;
                    texinfo_t  *texinfo;

                    // Init the quad.
                    quad = R_AllocRendPoly(RP_QUAD, true, 4);
                    quad->flags = 0;
                    quad->tex.detail = NULL;
                    quad->intertex.detail = NULL;
                    quad->wall->length = seg->length;

                    quad->texoffx = side->SW_middleoffx + seg->offset;
                    quad->texoffy = texOffY;

                    // Blendmode
                    if(side->SW_middleblendmode == BM_NORMAL && noSpriteTrans)
                        quad->blendmode = BM_ZEROALPHA; // "no translucency" mode
                    else
                        quad->blendmode = side->SW_middleblendmode;

                    surfaceFlags = Rend_PrepareTextureForPoly(quad, surface, &texinfo);
                    if(solidSeg && !(surfaceFlags & SUF_NO_RADIO))
                        tempflags |= RPF2_SHADOW;
                    if(solidSeg && surface->SM_texture != -1 && !texinfo->masked)
                        tempflags |= RPF2_SHINY;
                    if(surfaceFlags & SUF_GLOW)
                        tempflags |= RPF2_GLOW;
                    if(surfaceFlags & SUF_BLEND)
                        tempflags |= RPF2_BLEND;

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

                    // If alpha, masked or blended we must render as a vissprite
                    if(alpha < 1 || texinfo->masked ||
                       side->SW_middleblendmode > 0)
                        quad->flags = RPF_MASKED;
                    else
                        quad->flags = 0;

                    Rend_RenderWallSection(quad, seg, side, frontsec, surface, SEG_MIDDLE,
                              /*Alpha*/    alpha, tempflags);

                    R_FreeRendPoly(quad);
                }
            }
        }
    }

    // Upper wall.
    if(side->sections[SEG_TOP].frameflags & SUFINF_PVIS)
    {
        surface = &side->SW_topsurface;
        // Is there a visible surface?
        if(surface->SM_texture != 0)
        {
            float   alpha;
            boolean isVisible = true;
            float   tcxoff, tcyoff;

            // Calculate texture coordinates.
            tcxoff = side->SW_topoffx + seg->offset;
            tcyoff = 0;

           /* if(side->flags & SDF_MIDTEXUPPER)
            {
                // DOOM.EXE rendering hack.
                // Mid texture uppers use a relative offset to the lower
                // ceiling plus peggedness is always ignored.
                // However, negative yoffsets mean we get no upper.

                if(tcyoff)
                {
                    float relOffset = bceil + tcyoff;

                    vTL[VZ] = vTR[VZ] = relOffset;
                    vBL[VZ] = vBR[VZ] = relOffset - texh;
                    quad->texoffy = 0;

                    // We allow all properties normally associated with
                    // middle textures on mid texture uppers.

                    // Blendmode
                    if(side->SW_middleblendmode == BM_NORMAL && noSpriteTrans)
                        quad->blendmode = BM_ZEROALPHA; // "no translucency" mode
                    else
                        quad->blendmode = side->SW_middleblendmode;

                    alpha = side->SW_middlergba[3];

                    // If alpha, masked or blended we must render as a vissprite
                    if(alpha < 255 || texmask || side->SW_middleblendmode > 0)
                        quad->flags = RPF_MASKED;
                }
                else
                    isVisible = false;
            }
            else*/
            {
                vL_ZTop = vR_ZTop = fceil;
                vL_ZBottom = vR_ZBottom = bceil;

                alpha = 1;
            }

            if(vL_ZBottom < ffloor)
                vL_ZBottom = vR_ZBottom = ffloor;

            if(isVisible)
            {
                short       tempflags = 0;
                int         surfaceFlags;
                texinfo_t  *texinfo;
                rendpoly_t *quad;

                // Init the quad.
                quad = R_AllocRendPoly(RP_QUAD, true, 4);
                quad->flags = 0;
                quad->tex.detail = NULL;
                quad->intertex.detail = NULL;

                quad->texoffx = tcxoff;
                quad->texoffy = side->SW_topoffy;

                quad->wall->length = seg->length;

                surfaceFlags = Rend_PrepareTextureForPoly(quad, surface, &texinfo);
                if(!(surfaceFlags & SUF_NO_RADIO))
                    tempflags |= RPF2_SHADOW;
                if(surface->SM_texture != -1)
                    tempflags |= RPF2_SHINY;
                if(surfaceFlags & SUF_GLOW)
                    tempflags |= RPF2_GLOW;
                if(surfaceFlags & SUF_BLEND)
                    tempflags |= RPF2_BLEND;

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

                if(!(ldef->mapflags & ML_DONTPEGTOP)) // Normal alignment to bottom.
                    quad->texoffy += texinfo->height - (fceil - bceil);

                // Check for neighborhood division?
                Rend_WallHeightDivision(quad, seg, frontsec, SEG_TOP);

                Rend_RenderWallSection(quad, seg, side, frontsec, surface, SEG_TOP,
                          /*Alpha*/    alpha, tempflags);

                R_FreeRendPoly(quad);
            }
        }
    }

    // Lower wall.
    if(side->sections[SEG_BOTTOM].frameflags & SUFINF_PVIS)
    {
        surface = &side->SW_bottomsurface;
        // Is there a visible surface?
        if(surface->SM_texture != 0)
        {
            short       tempflags = 0;
            int         surfaceFlags;
            rendpoly_t *quad;

            // Init the quad.
            quad = R_AllocRendPoly(RP_QUAD, true, 4);
            quad->flags = 0;
            quad->tex.detail = NULL;
            quad->intertex.detail = NULL;
            quad->wall->length = seg->length;

            // Calculate texture coordinates.
            quad->texoffx = side->SW_bottomoffx + seg->offset;
            quad->texoffy = side->SW_bottomoffy;

            quad->flags = 0;
            vL_ZTop = vR_ZTop = bfloor;
            vL_ZBottom = vR_ZBottom = ffloor;
            if(vL_ZTop > fceil)
            {
                // Can't go over front ceiling, would induce polygon flaws.
                quad->texoffy += vL_ZTop - fceil;
                vL_ZTop = vR_ZTop = fceil;
            }

            surfaceFlags = Rend_PrepareTextureForPoly(quad, surface, NULL);
            if(!(surfaceFlags & SUF_NO_RADIO))
                tempflags |= RPF2_SHADOW;
            if(surface->SM_texture != -1)
                tempflags |= RPF2_SHINY;
            if(surfaceFlags & SUF_GLOW)
                tempflags |= RPF2_GLOW;
            if(surfaceFlags & SUF_BLEND)
                tempflags |= RPF2_BLEND;

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

            if(ldef->mapflags & ML_DONTPEGBOTTOM)
                quad->texoffy += fceil - bfloor; // Align with normal middle texture.

            // Check for neighborhood division?
            Rend_WallHeightDivision(quad, seg, frontsec, SEG_BOTTOM);

            Rend_RenderWallSection(quad, seg, side, frontsec, surface, SEG_BOTTOM,
                      /*Alpha*/    1, tempflags);

            R_FreeRendPoly(quad);
        }
    }

    // Can we make this a solid segment in the clipper?
    if(solidSeg == -1)
        return; // NEVER (we have a hole we couldn't fix).

    if(!solidSeg) // We'll have to determine whether we can...
    {
        if(backsec == frontsec)
        {
            // An obvious hack, what to do though??
        }
        else if((bceil <= ffloor &&
                    ((side->SW_toptexture != 0 /* && !(side->flags & SDF_MIDTEXUPPER)*/) ||
                     (side->SW_middletexture != 0))) ||
                (bfloor >= fceil &&
                    (side->SW_bottomtexture != 0 || side->SW_middletexture !=0)))
        {
            // A closed gap.
            solidSeg = true;
        }
        else if((seg->frameflags & SEGINF_BACKSECSKYFIX) ||
                (bsh == 0 && bfloor > ffloor && bceil < fceil &&
                (side->SW_toptexture != 0 /*&& !(side->flags & SDF_MIDTEXUPPER)*/) &&
                (side->SW_bottomtexture != 0)))
        {
            // A zero height back segment
            solidSeg = true;
        }
    }

    if(solidSeg && !P_IsInVoid(viewplayer))
        C_AddViewRelSeg(vL_XY[VX], vL_XY[VY], vR_XY[VX], vR_XY[VY]);
}

float Rend_SectorLight(sector_t *sec)
{
    if(levelFullBright)
        return 1.0f;
    
    // Apply light adaptation
    return sec->lightlevel + Rend_GetLightAdaptVal(sec->lightlevel);
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
        if(seg->linedef && !(seg->flags & SEGF_POLYOBJ))
        {
            seg->frameflags &= ~SEGINF_BACKSECSKYFIX;

            // Which way should it be facing?
            if(Rend_SegFacingDir(seg->SG_v1pos, seg->SG_v2pos))  // 1=front
                seg->frameflags |= SEGINF_FACINGFRONT;
            else
                seg->frameflags &= ~SEGINF_FACINGFRONT;

            Rend_MarkSegSectionsPVisible(seg);
        }
        *ptr++;
     }

    if(sub->poly)
    {
        for(i = 0; i < sub->poly->numsegs; ++i)
        {
            seg = sub->poly->segs[i];

            seg->frameflags &= ~SEGINF_BACKSECSKYFIX;

            // Which way should it be facing?
            if(Rend_SegFacingDir(seg->SG_v1pos, seg->SG_v2pos))  // 1=front
                seg->frameflags |= SEGINF_FACINGFRONT;
            else
                seg->frameflags &= ~SEGINF_FACINGFRONT;

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
    uint        j, num, pass;
    seg_t      *seg, **list;
    side_t     *side;

    // Init the quad.
    quad = R_AllocRendPoly(RP_QUAD, true, 4);
    quad->flags = RPF_SKY_MASK;
    quad->lights = NULL;
    quad->tex.id = 0;
    quad->tex.detail = NULL;
    quad->intertex.id = 0;
    quad->intertex.detail = NULL;

    vBL = quad->vertices[0].pos;
    vBR = quad->vertices[2].pos;
    vTL = quad->vertices[1].pos;
    vTR = quad->vertices[3].pos;

    // First pass = walls, second pass = poly objects.
    for(pass = 0; pass < 2; ++pass)
    {
        if(pass == 0)
        {
            num  = ssec->segcount;
            list = ssec->segs;
        }
        else
        {
            if(!ssec->poly)
                break; // we're done

            num  = ssec->poly->numsegs;
            list = ssec->poly->segs;
        }

        for(j = 0; j < num; ++j)
        {
            seg = list[j];

            if(!seg->linedef)    // "minisegs" have no linedefs.
                continue;

            // Let's first check which way this seg is facing.
            if(!(seg->frameflags & SEGINF_FACINGFRONT))
                continue;

            side = seg->sidedef;
            if(!side)
                continue;

            if(seg->flags & SEGF_POLYOBJ) // No sky fixes for polyobj segs.
                continue;

            backsec = seg->SG_backsector;
            frontsec = seg->SG_frontsector;

            if(backsec == frontsec &&
               side->SW_toptexture == 0 && side->SW_bottomtexture == 0 &&
               side->SW_middletexture == 0)
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

            // Upper/lower normal skyfixes.
            if(!(frontsec->selfRefHack && backsec))
            {
                // Floor.
                if(frontsec->skyfix[PLN_FLOOR].offset < 0)
                {
                    if(!backsec ||
                       (backsec && backsec != seg->SG_frontsector &&
                        (bfloor + backsec->skyfix[PLN_FLOOR].offset >
                         ffloor + frontsec->skyfix[PLN_FLOOR].offset)))
                    {
                        vTL[VZ] = vTR[VZ] = ffloor;
                        vBL[VZ] = vBR[VZ] =
                            ffloor + frontsec->skyfix[PLN_FLOOR].offset;
                        RL_AddPoly(quad);
                    }
                }

                // Ceiling.
                if(frontsec->skyfix[PLN_CEILING].offset > 0)
                {
                    if(!backsec ||
                       (backsec && backsec != seg->SG_frontsector &&
                        (bceil + backsec->skyfix[PLN_CEILING].offset <
                         fceil + frontsec->skyfix[PLN_CEILING].offset)))
                    {
                        vTL[VZ] = vTR[VZ] =
                            fceil + frontsec->skyfix[PLN_CEILING].offset;
                        vBL[VZ] = vBR[VZ] = fceil;

                        RL_AddPoly(quad);
                    }
                }
            }

            // Upper/lower zero height backsec skyfixes.
            if(backsec && bsh <= 0)
            {
                if(R_IsSkySurface(&frontsec->SP_floorsurface) &&
                   R_IsSkySurface(&backsec->SP_floorsurface))
                {
                    // Floor.
                    if(backsec->skyfix[PLN_FLOOR].offset < 0)
                    {
                        vTL[VZ] = vTR[VZ] = bfloor;
                        vBL[VZ] = vBR[VZ] =
                            bfloor + backsec->skyfix[PLN_FLOOR].offset;
                        RL_AddPoly(quad);
                    }
                    // Ensure we add a solid view seg.
                    seg->frameflags |= SEGINF_BACKSECSKYFIX;
                }

                    // Ceiling.
                if(R_IsSkySurface(&frontsec->SP_ceilsurface) &&
                   R_IsSkySurface(&backsec->SP_ceilsurface))
                {
                    if(backsec->skyfix[PLN_CEILING].offset > 0)
                    {
                        vTL[VZ] = vTR[VZ] =
                            bceil + backsec->skyfix[PLN_CEILING].offset;
                        vBL[VZ] = vBR[VZ] = bceil;
                        RL_AddPoly(quad);
                    }
                    // Ensure we add a solid view seg.
                    seg->frameflags |= SEGINF_BACKSECSKYFIX;
                }
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

    if(devNoCulling || P_IsInVoid(viewplayer))
        return;

    fronth[0] = front->SP_floorheight;
    fronth[1] = front->SP_ceilheight;

    ptr = sub->segs;
    while(*ptr)
    {
        seg = *ptr;

        // Occlusions can only happen where two sectors contact.
        if(seg->linedef && seg->SG_backsector &&
           !(seg->flags & SEGF_POLYOBJ) && // Polyobjects don't occlude.
           (forward_facing = (seg->frameflags & SEGINF_FACINGFRONT)))
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
    int         flags = 0;
    sector_t   *polySector;
    float       vec[3];

    polySector = R_GetLinkedSector(subsector, plane->type);
    surface = &polySector->planes[plane->type]->surface;

    // We don't render planes for unclosed sectors when the polys would
    // be added to the skymask (a DOOM.EXE renderer hack).
    if(sector->unclosed && R_IsSkySurface(surface))
        return;

    // Determine plane height.
    height = polySector->SP_planevisheight(plane->type);
    // Add the skyfix
    height += polySector->S_skyfix(plane->type).offset;

    vec[VX] = vx - subsector->midpoint.pos[VX];
    vec[VY] = vz - subsector->midpoint.pos[VY];
    vec[VZ] = vy - height;

    // Don't bother with planes facing away from the camera.
    if(!(M_DotProduct(vec, surface->normal) < 0))
    {
        rendpoly_t *poly =
            R_AllocRendPoly(RP_FLAT, false, subsector->numvertices);

        surfaceFlags = Rend_PrepareTextureForPoly(poly, surface, NULL);

        // Is there a visible surface?
        if(surface->SM_texture != 0)
        {
            short tempflags = 0;

            // Fill in the remaining quad data.
            poly->flags = 0;

            // Check for sky.
            if(R_IsSkySurface(surface))
            {
                poly->flags |= RPF_SKY_MASK;
            }
            else
            {
                poly->texoffx = sector->planes[plane->type]->surface.offx;
                poly->texoffy = sector->planes[plane->type]->surface.offy;
            }

            if(surface->SM_texture != -1)
                tempflags |= RPF2_SHINY;
            if(surfaceFlags & SUF_GLOW)
                tempflags |= RPF2_GLOW;
            if(surfaceFlags & SUF_BLEND)
                tempflags |= RPF2_BLEND;

            Rend_DoRenderPlane(poly, polySector, subsector, plane, surface,
                               height, tempflags);
        }

        R_FreeRendPoly(poly);
    }
}

static void Rend_RenderSubsector(uint ssecidx)
{
    uint        i;
    subsector_t *ssec = SUBSECTOR_PTR(ssecidx);
    seg_t      *seg, **ptr;
    sector_t   *sect = ssec->sector;
    float       sceil = sect->SP_ceilvisheight;
    float       sfloor = sect->SP_floorvisheight;

    if(sceil - sfloor <= 0 || ssec->numvertices < 3)
    {
        // Skip this, it has no volume.
        // Neighbors handle adding the solid clipper segments.
        return;
    }

    if(!firstsubsector)
    {
        if(!C_CheckSubsector(ssec))
            return;             // This isn't visible.
    }
    else
    {
        firstsubsector = false;
    }

    // Mark the sector visible for this frame.
    sect->frameflags |= SIF_VISIBLE;

    // Retrieve the sector light color.
    sLightColor = R_GetSectorLightColor(sect);

    Rend_MarkSegsFacingFront(ssec);

    // Dynamic lights.
    if(useDynLights)
        DL_ProcessSubsector(ssec);

    // Prepare for FakeRadio.
    Rend_RadioInitForSubsector(ssec);
    Rend_RadioSubsectorEdges(ssec);

    Rend_OccludeSubsector(ssec, false);
    DL_ClipInSubsector(ssecidx);
    Rend_OccludeSubsector(ssec, true);

    if(ssec->poly)
    {
        // Polyobjs don't obstruct, do clip lights with another algorithm.
        DL_ClipBySight(ssecidx);
    }

    // Mark the particle generators in the sector visible.
    PG_SectorIsVisible(sect);

    // Sprites for this sector have to be drawn. This must be done before
    // the segments of this subsector are added to the clipper. Otherwise
    // the sprites would get clipped by them, and that wouldn't be right.
    R_AddSprites(sect);

    // Draw the various skyfixes for all front facing segs in this ssec
    // (includes polyobject segs).
    if(R_IsSkySurface(&ssec->sector->SP_floorsurface) ||
       R_IsSkySurface(&ssec->sector->SP_ceilsurface))
        Rend_SSectSkyFixes(ssec);

    // Draw the walls.
    ptr = ssec->segs;
    while(*ptr)
    {
        seg = *ptr;

        if(!(seg->flags & SEGF_POLYOBJ)  &&// Not handled here.
           seg->linedef && // "minisegs" have no linedefs.
           !(seg->linedef->flags & LINEF_BENIGN) && // Benign linedefs are not rendered.
           (seg->frameflags & SEGINF_FACINGFRONT))
        {
            if(!seg->SG_backsector || !seg->SG_frontsector)
                Rend_RenderSSWallSeg(seg, ssec);
            else
                Rend_RenderWallSeg(seg, ssec);
        }

        *ptr++;
    }

    // Is there a polyobj on board?
    if(ssec->poly)
    {
        for(i = 0; i < ssec->poly->numsegs; ++i)
        {
            seg = ssec->poly->segs[i];

            // Let's first check which way this seg is facing.
            if(seg->frameflags & SEGINF_FACINGFRONT)
                Rend_RenderSSWallSeg(seg, ssec);
        }
    }

    // Render all planes of this sector.
    for(i = 0; i < sect->planecount; ++i)
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
        node_t     *bsp;
        int         side;

        // Descend deeper into the nodes.
        bsp = NODE_PTR(bspnum);

        // Decide which side the view point is on.
        side = R_PointOnSide(viewx, viewy, bsp);

        Rend_RenderNode(bsp->children[side]);   // Recursively divide front space.
        Rend_RenderNode(bsp->children[side ^ 1]);   // ...and back space.
    }
}

void Rend_RenderMap(void)
{
    binangle_t viewside;

    // Set to true if dynlights are inited for this frame.
    dlInited = false;

    // This is all the clearing we'll do.
    if(P_IsInVoid(viewplayer) || freezeRLs)
        gl.Clear(DGL_COLOR_BUFFER_BIT | DGL_DEPTH_BUFFER_BIT);
    else
        gl.Clear(DGL_DEPTH_BUFFER_BIT);

    // Setup the modelview matrix.
    Rend_ModelViewMatrix(true);

    if(!freezeRLs)
    {
        // Prepare for rendering.
        RL_ClearLists();        // Clear the lists for new quads.
        C_ClearRanges();        // Clear the clipper.
        DL_ClearForFrame();     // Zeroes the links.
        LG_Update();
        SB_BeginFrame();
        Rend_RadioInitForFrame();

        // Generate surface decorations for the frame.
        Rend_InitDecorationsForFrame();

        // Maintain luminous objects.
        if(useDynLights || haloMode || litSprites || useDecorations)
        {
            DL_InitForNewFrame();
        }

        // Add the backside clipping range (if vpitch allows).
        if(vpitch <= 90 - yfov / 2 && vpitch >= -90 + yfov / 2)
        {
            float   a = fabs(vpitch) / (90 - yfov / 2);
            binangle_t startAngle =
                (binangle_t) (BANG_45 * fieldOfView / 90) * (1 + a);
            binangle_t angLen = BANG_180 - startAngle;

            viewside = (viewangle >> (32 - BAMS_BITS)) + startAngle;
            C_SafeAddRange(viewside, viewside + angLen);
            C_SafeAddRange(viewside + angLen, viewside + 2 * angLen);
        }

        // The viewside line for the depth cue.
        viewsidex = -FIX2FLT(viewsin);
        viewsidey = FIX2FLT(viewcos);

        // We don't want subsector clipchecking for the first subsector.
        firstsubsector = true;
        Rend_RenderNode(numnodes - 1);

        // Make vissprites of all the visible decorations.
        Rend_ProjectDecorations();

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
 * Updates the lightRangeModMatrix which is used to applify sector light to
 * help compensate for the differences between the OpenGL lighting equation,
 * the software Doom lighting model and the light grid (ambient lighting).
 *
 * Also calculates applified values (the curve of set with r_lightAdaptMul),
 * used for emulation of the effects of light adaptation in the human eye.
 * This is not strictly accurate as the amplification happens at differing
 * rates in each color range but we'll just use one value applied uniformly
 * to each range (RGB).
 *
 * The offsets in the lightRangeModTables are added to the sector->lightlevel
 * during rendering (both positive and negative).
 */
void Rend_CalcLightRangeModMatrix(cvar_t *unused)
{
    int         j, n;
    float       f;
    double      ramp = 0, rm = 0;

    memset(lightRangeModMatrix, 0, (sizeof(float) * 255) * MOD_RANGE);

    if(mapambient > ambientLight)
        r_ambient = mapambient;
    else
        r_ambient = ambientLight;

    if(lightRangeAdaptRamp != 0)
    {
        ramp = -MOD_RANGE * (lightRangeAdaptRamp / 10.0f);
        rm = -(ramp * (MOD_RANGE / 2));
    }

    for(n = 0; n < MOD_RANGE; ++n, rm += ramp)
    {
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

            // Apply the light range adaption ramp.
            f += rm;

            // Lower than the ambient limit?
            if(r_ambient != 0 && j+f <= r_ambient)
                f = r_ambient - j;

            // Clamp the result as a modifier to the light value (j).
            if((j+f) >= 255)
                f = 255 - j;
            else if((j+f) <= 0)
                f = -j;

            // Insert it into the matrix
            lightRangeModMatrix[n][j] = f / 255.0f;
        }
    }
}

/**
 * Initializes the light range of all players.
 */
void Rend_InitPlayerLightRanges(void)
{
    uint        i;
    sector_t   *sec;
    ddplayer_t *player;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].ingame)
            continue;

        player = &players[i];
        if(!player->mo || !player->mo->subsector)
            continue;

        sec = player->mo->subsector->sector;

        playerLightRange[i] = (MOD_RANGE/2) - 1;
        playerLastLightSample[i].value =
            playerLastLightSample[i].currentlight = sec->lightlevel;
        playerLastLightSample[i].sector = sec;
    }
}

/**
 * Grabs the light value of the sector each player is currently
 * in and chooses an appropriate light range.
 *
 * \todo Interpolate between current range and last when player
 * changes sector.
 */
void Rend_RetrieveLightSample(void)
{
    uint        i;
    int         midpoint;
    float       light, diff, range, mod, inter;
    ddplayer_t *player;
    subsector_t *sub;

    unsigned int currentTime = Sys_GetRealTime();

    midpoint = MOD_RANGE / 2;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].ingame)
            continue;

        player = &players[i];
        if(!player->mo || !player->mo->subsector)
            continue;

        sub = player->mo->subsector;

        // In some circumstances we should disable light adaptation.
        if(levelFullBright || P_IsInVoid(player))
        {
            playerLightRange[i] = midpoint;
            continue;
        }

        /** Evaluate the light level at the point this player is at.
        * Uses a very simply method which uses the sector in which the player
        * is at currently and uses the light level of that sector.
        * \todo Really this is junk. We should evaluate the lighting condition using
        * a much better method (perhaps obtain an average color from all the
        * vertexes (selected lists only) rendered in the previous frame?)
	*/
     /*   if(useBias)
        {
            byte color[3];

            // use the bias lighting to evaluate the point.
            // todo this we'll create a vertex at the players position
            // so that it can be evaluated with both ambient light (light grid)
            // and with the bias light sources.

            // Use the players postion.
            pos[VX] = FIX2FLT(player->mo->[VX]);
            pos[VY] = FIX2FLT(player->mo->[VY]);
            pos[VZ] = FIX2FLT(player->mo->[VZ]);

            // \todo Should be affected by BIAS sources...
            LG_Evaluate(pos, color);
            light = ((float)(color[0] + color[1] + color[2]) / 3) / 255.0f;
        }
        else*/
        {
            // use the light level of the sector they are currently in.
            light = sub->sector->lightlevel;
        }

        /** Pick the range based on the current lighting conditions compared
        * to the previous lighting conditions.
        * We adapt to predominantly bright lighting conditions (mere seconds)
        * much quicker than very dark lighting conditions (upto 30mins).
        * Obviously that would be much too slow for a fast paced action game.
	*/
        if(light < playerLastLightSample[i].value)
        {
            // Going from a bright area to a darker one
            inter = (currentTime - playerLastLightSample[i].updateTime) /
                           (float) SECONDS_TO_TICKS(r_lightAdaptDarkTime);

            if(inter > 1)
                mod = 0;
            else
            {
                diff = playerLastLightSample[i].value - light;
                mod = -(MOD_RANGE * diff) * (1.0f - inter);
            }
        }
        else if(light > playerLastLightSample[i].value)
        {
            // Going from a dark area to a bright one
            inter = (currentTime - playerLastLightSample[i].updateTime) /
                           (float) SECONDS_TO_TICKS(r_lightAdaptBrightTime);

            if(inter > 1)
                mod = 0;
            else
            {
                diff = light - playerLastLightSample[i].value;
                mod = (MOD_RANGE * diff) * (1.0f - inter);
            }
        }
        else
            mod = 0;

        range = midpoint - (mod * r_lightAdapt);

        // Round to nearest whole.
        range += 0.5f;

        // Clamp the range
        if(range >= MOD_RANGE)
            range = MOD_RANGE - 1;
        else if(range < 0)
            range = 0;

        playerLightRange[i] = (uint) range;

        // Have the lighting conditions changed?

        // Light differences between sectors?
        if(!(playerLastLightSample[i].sector == sub->sector))
        {
            // If this sample is different to the previous sample
            // update the last sample value;
            if(playerLastLightSample[i].sector)
                playerLastLightSample[i].value =
                    playerLastLightSample[i].sector->lightlevel;
            else
                playerLastLightSample[i].value = light;

            playerLastLightSample[i].updateTime = currentTime;
            playerLastLightSample[i].sector = sub->sector;
            playerLastLightSample[i].currentlight = light;
        }
        else // Light changes in the same sector?
        {
            if(playerLastLightSample[i].currentlight != light)
            {
                playerLastLightSample[i].value = light;
                playerLastLightSample[i].updateTime = currentTime;
            }
        }
    }
}

/**
 * Applies the offset from the lightRangeModMatrix to the given light value.
 *
 * The range chosen is that of the current viewplayer.
 * (calculated based on the lighting conditions for that player)
 *
 * The lightRangeModMatrix is used to implement (precalculated) ambient light
 * limit, light range compression, light range shift AND light adaptation
 * response curves. By generating this precalculated matrix we save on a LOT
 * of calculations (replacing them all with one single addition) which would
 * otherwise all have to be performed each frame for each call. The values are
 * applied to sector lightlevels, lightgrid point evaluations, vissprite
 * lightlevels, fakeradio shadows etc, etc...
 *
 * NOTE: There is no need to clamp the result. Since the offset values in
 *       the lightRangeModMatrix have already been clamped so that the resultant
 *       lightvalue is NEVER outside the range 0-254 when the original lightvalue
 *       is used as the index.
 *
 * @param lightvar    Ptr to the value to apply the adaptation to.
 */
void Rend_ApplyLightAdaptation(float *lightvar)
{
    // The default range.
    uint        range = (MOD_RANGE/2) - 1;
    float       lightval;

    if(lightvar == NULL)
        return; // Can't apply adaptation to a NULL val ptr...

    // Apply light adaptation?
    if(r_lightAdapt)
        range = playerLightRange[viewplayer - players];

    lightval = ROUND(255.0f * *lightvar);
    if(lightval > 254)
        lightval = 254;
    else if(lightval < 0)
        lightval = 0;

    *lightvar += lightRangeModMatrix[range][(int) lightval];
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
    uint        range = (MOD_RANGE/2) - 1;
    float       lightval;

    // Apply light adaptation?
    if(r_lightAdapt)
        range = playerLightRange[viewplayer - players];

    lightval = ROUND(255.0f * lightvalue);
    if(lightval > 254)
        lightval = 254;
    else if(lightval < 0)
        lightval = 0;

    return lightRangeModMatrix[range][(int) lightval];
}

static void DrawRangeBox(int x, int y, int w, int h, ui_color_t *c)
{
    UI_GradientEx(x, y, w, h, 6,
                  c ? c : UI_Color(UIC_BG_MEDIUM),
                  c ? c : UI_Color(UIC_BG_LIGHT),
                  1, 1);
    UI_DrawRectEx(x, y, w, h, 6, false, c ? c : UI_Color(UIC_BRD_HI),
                  NULL, 1, -1);
}

#define bWidth 1.0f
#define bHeight ((bWidth / MOD_RANGE) * 255.0f)
#define BORDER 20

/**
 * Draws the lightRangeModMatrix (for debug)
 */
void R_DrawLightRange(void)
{
    int         i, r;
    int         winWidth, winHeight;
    int         y;
    static char *title = "Light Range Matrix";
    float       c, off;
    ui_color_t  color;

    if(!debugLightModMatrix)
        return;

    if(!Sys_GetWindowDimensions(windowIDX, NULL, NULL, &winWidth, &winHeight))
    {
        Con_Message("R_DrawLightRange: Failed retrieving window dimensions.");
        return;
    }

    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, winWidth, winHeight, -1, 1);

    gl.Translatef(BORDER, BORDER + BORDER, 0);

    color.red = 0.2f;
    color.green = 0;
    color.blue = 0.6f;

    DrawRangeBox(0 - (BORDER / 2), 0 - BORDER - 10,
                 bWidth * 255 + BORDER,
                 bHeight * MOD_RANGE + BORDER + BORDER, &color);

    gl.Disable(DGL_TEXTURING);

    for(r = MOD_RANGE, y = 0; r > 0; r--, y++)
    {
        gl.Begin(DGL_QUADS);
        for(i = 0, c = 0; i < 255; ++i, c += (1.0f/255.0f))
        {
            // Get the result of the source light level + offset.
            off = lightRangeModMatrix[r-1][i];

            // Draw the range bar to match that of the current viewplayer.
            if(r == playerLightRange[viewplayer - players])
                gl.Color4f(c + off, c + off, c + off, 1);
            else
                gl.Color4f(c + off, c + off, c + off, .75);

            gl.Vertex2f(i * bWidth, y * bHeight);
            gl.Vertex2f(i * bWidth + bWidth, y * bHeight);
            gl.Vertex2f(i * bWidth + bWidth,
                        y * bHeight + bHeight);
            gl.Vertex2f(i * bWidth, y * bHeight + bHeight);
        }
        gl.End();
    }

    gl.Enable(DGL_TEXTURING);

    UI_TextOutEx(title, ((bWidth * 255) / 2),
                 -12, true, true, UI_Color(UIC_TITLE), .7f);

    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
}

/**
 * Draws a textured cube using the currently bound gl texture.
 * Used to draw mobj bounding boxes.
 *
 * @param   pos3f       Coordinates of the center of the box.
 *                      (in "world" coordinates [VX, VY, VZ])
 * @param   w           Width of the box.
 * @param   l           Length of the box.
 * @param   h           Height of the box.
 * @param   color3f     Color to make the box (uniform vertex color).
 * @param   alpha       Alpha to make the box (uniform vertex color).
 * @param   br          Border amount to overlap box faces.
 * @param   alignToBase If <code>true</code> align the base of the box
 *                      to the Z coordinate.
 */
void Rend_DrawBBox(float pos3f[3], float w, float l, float h,
                   float color3f[3], float alpha, float br,
                   boolean alignToBase)
{
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();

    if(alignToBase)
        // The Z coordinate is to the bottom of the object.
        gl.Translatef(pos3f[VX], pos3f[VZ] + h, pos3f[VY]);
    else
        gl.Translatef(pos3f[VX], pos3f[VZ], pos3f[VY]);

    gl.Scalef(w - br - br, h - br - br, l - br - br);

    gl.Begin(DGL_QUADS);
    {
        gl.Color4f(color3f[0], color3f[1], color3f[2], alpha);
        // Top
        gl.TexCoord2f(1.0f, 1.0f); gl.Vertex3f( 1.0f+br, 1.0f,-1.0f-br);  // TR
        gl.TexCoord2f(0.0f, 1.0f); gl.Vertex3f(-1.0f-br, 1.0f,-1.0f-br);  // TL
        gl.TexCoord2f(0.0f, 0.0f); gl.Vertex3f(-1.0f-br, 1.0f, 1.0f+br);  // BL
        gl.TexCoord2f(1.0f, 0.0f); gl.Vertex3f( 1.0f+br, 1.0f, 1.0f+br);  // BR
        // Bottom
        gl.TexCoord2f(1.0f, 1.0f); gl.Vertex3f( 1.0f+br,-1.0f, 1.0f+br);  // TR
        gl.TexCoord2f(0.0f, 1.0f); gl.Vertex3f(-1.0f-br,-1.0f, 1.0f+br);  // TL
        gl.TexCoord2f(0.0f, 0.0f); gl.Vertex3f(-1.0f-br,-1.0f,-1.0f-br);  // BL
        gl.TexCoord2f(1.0f, 0.0f); gl.Vertex3f( 1.0f+br,-1.0f,-1.0f-br);  // BR
        // Front
        gl.TexCoord2f(1.0f, 1.0f); gl.Vertex3f( 1.0f+br, 1.0f+br, 1.0f);  // TR
        gl.TexCoord2f(0.0f, 1.0f); gl.Vertex3f(-1.0f-br, 1.0f+br, 1.0f);  // TL
        gl.TexCoord2f(0.0f, 0.0f); gl.Vertex3f(-1.0f-br,-1.0f-br, 1.0f);  // BL
        gl.TexCoord2f(1.0f, 0.0f); gl.Vertex3f( 1.0f+br,-1.0f-br, 1.0f);  // BR
        // Back
        gl.TexCoord2f(1.0f, 1.0f); gl.Vertex3f( 1.0f+br,-1.0f-br,-1.0f);  // TR
        gl.TexCoord2f(0.0f, 1.0f); gl.Vertex3f(-1.0f-br,-1.0f-br,-1.0f);  // TL
        gl.TexCoord2f(0.0f, 0.0f); gl.Vertex3f(-1.0f-br, 1.0f+br,-1.0f);  // BL
        gl.TexCoord2f(1.0f, 0.0f); gl.Vertex3f( 1.0f+br, 1.0f+br,-1.0f);  // BR
        // Left
        gl.TexCoord2f(1.0f, 1.0f); gl.Vertex3f(-1.0f, 1.0f+br, 1.0f+br);  // TR
        gl.TexCoord2f(0.0f, 1.0f); gl.Vertex3f(-1.0f, 1.0f+br,-1.0f-br);  // TL
        gl.TexCoord2f(0.0f, 0.0f); gl.Vertex3f(-1.0f,-1.0f-br,-1.0f-br);  // BL
        gl.TexCoord2f(1.0f, 0.0f); gl.Vertex3f(-1.0f,-1.0f-br, 1.0f+br);  // BR
        // Right
        gl.TexCoord2f(1.0f, 1.0f); gl.Vertex3f( 1.0f, 1.0f+br,-1.0f-br);  // TR
        gl.TexCoord2f(0.0f, 1.0f); gl.Vertex3f( 1.0f, 1.0f+br, 1.0f+br);  // TL
        gl.TexCoord2f(0.0f, 0.0f); gl.Vertex3f( 1.0f,-1.0f-br, 1.0f+br);  // BL
        gl.TexCoord2f(1.0f, 0.0f); gl.Vertex3f( 1.0f,-1.0f-br,-1.0f-br);  // BR
    }
    gl.End();

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();
}

/**
 * Draws a textured triangle using the currently bound gl texture.
 * Used to draw mobj angle direction arrow.
 *
 * @param   pos3f       Coordinates of the center of the base of the triangle.
 *                      (in "world" coordinates [VX, VY, VZ])
 * @param   a           Angle to point the triangle in.
 * @param   s           Scale of the triangle.
 * @param   color3f     Color to make the box (uniform vertex color).
 * @param   alpha       Alpha to make the box (uniform vertex color).
 */
void Rend_DrawArrow(float pos3f[3], angle_t a, float s, float color3f[3],
                    float alpha)
{
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();

    gl.Translatef(pos3f[VX], pos3f[VZ], pos3f[VY]);

    gl.Rotatef(0, 0, 0, 1);
    gl.Rotatef(0, 1, 0, 0);
    gl.Rotatef((a / (float) ANGLE_MAX *-360), 0, 1, 0);

    gl.Scalef(s, 0, s);

    gl.Begin(DGL_TRIANGLES);
    {
        gl.Color4f(0.0f, 0.0f, 0.0f, 0.5f);
        gl.TexCoord2f(1.0f, 1.0f); gl.Vertex3f( 1.0f, 1.0f,-1.0f);  // L

        gl.Color4f(color3f[0], color3f[1], color3f[2], alpha);
        gl.TexCoord2f(0.0f, 1.0f); gl.Vertex3f(-1.0f, 1.0f,-1.0f);  // Point

        gl.Color4f(0.0f, 0.0f, 0.0f, 0.5f);
        gl.TexCoord2f(0.0f, 0.0f); gl.Vertex3f(-1.0f, 1.0f, 1.0f);  // R
    }
    gl.End();

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();
}

/**
 * Renders bounding boxes for all mobj's (linked in sec->thinglist, except
 * the console player) in all sectors that are currently marked as vissible.
 *
 * Depth test is disabled to show all mobjs that are being rendered, regardless
 * if they are actually vissible (hidden by previously drawn map geometry).
 */
static void Rend_RenderBoundingBoxes(void)
{
    mobj_t     *mo;
    uint        i;
    sector_t   *sec;
    int         winWidth;
    float       size;
    static float red[3] = { 1, 0.2f, 0.2f}; // non-solid objects
    static float green[3] = { 0.2f, 1, 0.2f}; // solid objects
    static float yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles
    float       alpha;
    float       eye[3];
    float       pos[3];

    if(!devMobjBBox || netgame)
        return;

    if(!Sys_GetWindowDimensions(windowIDX, NULL, NULL, &winWidth, NULL))
    {
        Con_Message("Rend_RenderBoundingBoxes: Failed retrieving window "
                    "dimensions.");
        return;
    }

    eye[VX] = FIX2FLT(viewplayer->mo->pos[VX]);
    eye[VY] = FIX2FLT(viewplayer->mo->pos[VY]);
    eye[VZ] = FIX2FLT(viewplayer->mo->pos[VZ]);

    if(usingFog)
        gl.Disable(DGL_FOG);

    gl.Disable(DGL_DEPTH_TEST);
    gl.Enable(DGL_TEXTURING);
    gl.Disable(DGL_CULL_FACE);

    gl.Bind(GL_PrepareDDTexture(DDT_BBOX, NULL));
    GL_BlendMode(BM_ADD);

    // For every sector
    for(i = 0; i < numsectors; ++i)
    {
        sec = SECTOR_PTR(i);

        // Is it vissible?
        if(!(sec->frameflags & SIF_VISIBLE))
            continue;

        // For every mobj in the sector's thinglist
        for(mo = sec->thinglist; mo; mo = mo->snext)
        {
            if(mo == players[consoleplayer].mo)
                continue; // We don't want the console player.

            pos[VX] = FIX2FLT(mo->pos[VX]);
            pos[VY] = FIX2FLT(mo->pos[VY]);
            pos[VZ] = FIX2FLT(mo->pos[VZ]);
            alpha = 1 - ((M_Distance(pos, eye)/(winWidth/2))/4);

            if(alpha < .25f)
                alpha = .25f; // Don't make them totally invisible.

            // Draw a bounding box in an appropriate color.
            size = FIX2FLT(mo->radius);
            Rend_DrawBBox(pos, size, size, mo->height/2,
                          (mo->ddflags & DDMF_MISSILE)? yellow :
                          (mo->ddflags & DDMF_SOLID)? green : red,
                          alpha, 0.08f, true);

            Rend_DrawArrow(pos, mo->angle + ANG45 + ANG90 , size*1.25,
                           (mo->ddflags & DDMF_MISSILE)? yellow :
                           (mo->ddflags & DDMF_SOLID)? green : red, alpha);
        }
    }

    GL_BlendMode(BM_NORMAL);

    gl.Enable(DGL_CULL_FACE);
    gl.Enable(DGL_DEPTH_TEST);

    if(usingFog)
        gl.Enable(DGL_FOG);
}
