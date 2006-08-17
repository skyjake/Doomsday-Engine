/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
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

// FakeRadio should not be rendered.
#define RWSF_NO_RADIO 0x100

// TYPES -------------------------------------------------------------------

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

boolean willRenderSprites = true, freezeRLs = false;
int     missileBlend = 1;
int     litSprites = 1;
int     r_ambient = 0;

int     viewpw, viewph;         // Viewport size, in pixels.
int     viewpx, viewpy;         // Viewpoint top left corner, in pixels.

float   yfov;

int    gamedrawhud = 1;    // Set to zero when we advise that the HUD should not be drawn

extern DGLuint ddTextures[];

int     playerLightRange[MAXPLAYERS];

typedef struct {
    int value;
    int currentlight;
    sector_t *sector;
    unsigned int updateTime;
} lightsample_t;

lightsample_t playerLastLightSample[MAXPLAYERS];

float   r_lightAdapt = 0.8f; // Amount of light adaption

int     r_lightAdaptDarkTime = 80;
int     r_lightAdaptBrightTime = 3;

float   r_lightAdaptRamp = 0.001f;
float   r_lightAdaptMul = 0.02f;

int     r_lightrangeshift = 0;
float   r_lightcompression = 0;

signed short     lightRangeModMatrix[MOD_RANGE][255];

int     debugLightModMatrix = 0;
int     devMobjBBox = 0; // 1 = Draw mobj bounding boxes (for debug)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstsubsector;  // No range checking for the first one.

// Wall section colors.
// With accompanying ptrs to allow for "hot-swapping" (for speed).
const byte *topColorPtr;
static byte topColor[3];
const byte *midColorPtr;
static byte midColor[3];
const byte *bottomColorPtr;
static byte bottomColor[3];

// Current sector light color.
const byte *sLightColor;

// CODE --------------------------------------------------------------------

void Rend_Register(void)
{
    C_VAR_INT("rend-dev-freeze", &freezeRLs, CVF_NO_ARCHIVE, 0, 1);

    C_VAR_INT("rend-dev-cull-subsectors", &devNoCulling, CVF_NO_ARCHIVE, 0, 1);

    C_VAR_INT("rend-dev-mobj-bbox", &devMobjBBox, 0, 0, 1);

    C_VAR_FLOAT("rend-camera-fov", &fieldOfView, 0, 1, 179);

    C_VAR_BYTE("rend-tex-anim-smooth", &smoothTexAnim, 0, 0, 1);

    C_VAR_INT("rend-tex-shiny", &useShinySurfaces, 0, 0, 1);

    C_VAR_FLOAT2("rend-light-compression", &r_lightcompression, 0, -100, 100,
                 Rend_CalcLightRangeModMatrix);

    C_VAR_FLOAT("rend-light-adaptation", &r_lightAdapt, 0, 0, 1);

    C_VAR_FLOAT2("rend-light-adaptation-mul", &r_lightAdaptMul, CVF_PROTECTED, 0, 1,
                 Rend_CalcLightRangeModMatrix);

    C_VAR_FLOAT2("rend-light-adaptation-ramp", &r_lightAdaptRamp, CVF_PROTECTED, 0, 1,
                 Rend_CalcLightRangeModMatrix);

    C_VAR_INT("rend-light-adaptation-darktime", &r_lightAdaptDarkTime, 0, 0, 200);

    C_VAR_INT("rend-light-adaptation-brighttime", &r_lightAdaptBrightTime, 0, 0, 200);

    C_VAR_INT("rend-dev-light-modmatrix", &debugLightModMatrix, CVF_NO_ARCHIVE, 0, 1);

    RL_Register();
    SB_Register();
    LG_Register();
    Rend_ModelRegister();
    Rend_RadioRegister();
}

float Rend_SignedPointDist2D(float c[2])
{
    /*          (YA-YC)(XB-XA)-(XA-XC)(YB-YA)
       s = -----------------------------
       L**2
       Luckily, L**2 is one. dist = s*L. Even more luckily, L is also one.
     */
    return (vz - c[VY]) * viewsidex - (vx - c[VX]) * viewsidey;
}

/*
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

/*
 * Used to be called before starting a new level.
 */
void Rend_Reset(void)
{
    // Textures are deleted (at least skies need this???).
    GL_ClearRuntimeTextures();
    DL_Clear();
}

void Rend_ModelViewMatrix(boolean use_angles)
{
    vx = FIX2FLT(viewx);
    vy = FIX2FLT(viewz);
    vz = FIX2FLT(viewy);
    vang = viewangle / (float) ANGLE_MAX *360 - 90;

    gl.MatrixMode(DGL_MODELVIEW);
    gl.LoadIdentity();
    if(use_angles)
    {
        gl.Rotatef(vpitch = viewpitch * 85.0 / 110.0, 1, 0, 0);
        gl.Rotatef(vang, 0, 1, 0);
    }
    gl.Scalef(1, 1.2f, 1);      // This is the aspect correction.
    gl.Translatef(-vx, -vy, -vz);
}

#if 0
/*
 * Models the effect of distance to the light level. Extralight (torch)
 * is also noted. This is meant to be used for white light only
 * (a light level).
 */
int R_AttenuateLevel(int lightlevel, float distance)
{
    float   light = lightlevel / 255.0f, real, minimum;
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

int Rend_SegFacingDir(float v1[2], float v2[2])
{
    // The dot product. (1 if facing front.)
    return (v1[VY] - v2[VY]) * (v1[VX] - vx) + (v2[VX] - v1[VX]) * (v1[VY] -
                                                                    vz) > 0;
}

int Rend_FixedSegFacingDir(const seg_t *seg)
{
    // The dot product. (1 if facing front.)
    return FIX2FLT(seg->v1->y - seg->v2->y) * FIX2FLT(seg->v1->x - viewx) +
        FIX2FLT(seg->v2->x - seg->v1->x) * FIX2FLT(seg->v1->y - viewy) > 0;
}

int Rend_SegFacingPoint(float v1[2], float v2[2], float pnt[2])
{
    float   nx = v1[VY] - v2[VY], ny = v2[VX] - v1[VX];
    float   vvx = v1[VX] - pnt[VX], vvy = v1[VY] - pnt[VY];

    // The dot product.
    if(nx * vvx + ny * vvy > 0)
        return 1;               // Facing front.
    return 0;                   // Facing away.
}

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
    int i;

    for(i = 0; i < 3; ++i)
    {
        DGLubyte minimum = (DGLubyte) (ref->min_color[i] * 255);

        if(color->rgba[i] < minimum)
        {
            color->rgba[i] = minimum;
        }
    }

    color->rgba[3] = (DGLubyte) (ref->shininess * 255);
}

/*
 * Pre: As we modify param poly quite a bit it is the responsibility of
 *      the caller to ensure this is OK (ie if not it should pass us a
 *      copy instead (eg wall segs)).
 *
 * @param   texture     The texture/flat id of the texture on the poly.
 * @param   isFlat      (TRUE) = param texture is a flat.
 * @param   poly        The poly to add the shiny poly for.
 */
void Rend_AddShinyPoly(int texture, boolean isFlat, rendpoly_t *poly)
{
    int i;
    ded_reflection_t *ref = NULL;
    short width = 0;
    short height = 0;

    if(!useShinySurfaces)
    {
        // Shiny surfaces are not enabled.
        return;
    }

    // Figure out what kind of surface properties have been defined
    // for the texture or flat in question.
    if(isFlat)
    {
        flat_t *flat = R_GetFlat(texture);

        if(flat->translation.current != texture)
        {
            // The flat is currently translated to another one.
            flat = R_GetFlat(flat->translation.current);
        }

        ref = flat->reflection;
        width = 64;
        height = 64;
    }
    else
    {
        texture_t *texptr = textures[texturetranslation[texture].current];

        ref = texptr->reflection;
        width = texptr->width;
        height = texptr->height;
    }

    if(ref == NULL)
    {
        // The surface doesn't shine.
        return;
    }

    // Make sure the texture has been loaded.
    if(!GL_LoadReflectionMap(ref))
    {
        // Can't shine because there is no shiny map.
        return;
    }

    // Make it a shiny polygon.
    poly->lights = NULL;
    poly->flags |= RPF_SHINY;
    poly->blendmode = ref->blend_mode;
    poly->tex.id = ref->use_shiny->shiny_tex;
    poly->tex.detail = NULL;
    poly->intertex.detail = NULL;
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

static void Rend_PolyTexBlend(int texture, boolean isFlat, rendpoly_t *poly)
{
    translation_t *xlat = NULL;

    if(texture >= 0)
    {
        if(isFlat)
            xlat = &R_GetFlat(texture)->translation;
        else
            xlat = &texturetranslation[texture];
    }

    // If fog is active, inter=0 is accepted as well. Otherwise flickering
    // may occur if the rendering passes don't match for blended and
    // unblended surfaces.
    if(!xlat || !smoothTexAnim || numTexUnits < 2 ||
       xlat->current == xlat->next || (!usingFog && xlat->inter < 0))
    {
        // No blending for you, my friend.
        memset(&poly->intertex, 0, sizeof(poly->intertex));
        poly->interpos = 0;
        return;
    }

    // Get info of the blend target. The globals texw and texh are modified.
    if(isFlat)
        poly->intertex.id = GL_PrepareFlat2(xlat->next, false);
    else
        poly->intertex.id = GL_PrepareTexture2(xlat->next, false);

    poly->intertex.width = texw;
    poly->intertex.height = texh;
    poly->intertex.detail = texdetail;
    poly->interpos = xlat->inter;
}

boolean Rend_IsWallSectionPVisible(line_t* line, int section,
                                   boolean backside)
{
    if(!line)
        return false; // huh?

    // Missing side?
    if(line->sidenum[backside] == NO_INDEX)
        return false;

    // Non middle section of a single-sided line?
    if(section != SEG_MIDDLE && line->sidenum[1] == NO_INDEX)
        return false;

    switch(section)
    {
    case SEG_MIDDLE:
        // A two sided line?
        if(line->sidenum[0] != NO_INDEX && line->sidenum[1] != NO_INDEX)
        {
            side_t* side = SIDE_PTR(line->sidenum[backside]);

            // Check middle texture
            if(!(side->middle.texture || side->middle.texture == -1))
                return false;

            // Check alpha
            if(side->middle.rgba[3] == 0)
                return false;

            // Check Y placement?
        }
        break;

    case SEG_TOP:
        if(R_IsSkySurface(&line->backsector->SP_ceilsurface) &&
           R_IsSkySurface(&line->frontsector->SP_ceilsurface))
           return false;

        if(backside)
        {
            if(SECT_CEIL(line->backsector) <= SECT_CEIL(line->frontsector))
                return false;
        }
        else
        {
            if(SECT_CEIL(line->frontsector) <= SECT_CEIL(line->backsector))
                return false;
        }
        break;

    case SEG_BOTTOM:
        if(R_IsSkySurface(&line->backsector->SP_floorsurface) &&
           R_IsSkySurface(&line->frontsector->SP_floorsurface))
           return false;

        if(backside)
        {
            if(SECT_FLOOR(line->backsector) >= SECT_FLOOR(line->frontsector))
                return false;
        }
        else
        {
            if(SECT_FLOOR(line->frontsector) >= SECT_FLOOR(line->backsector))
                return false;
        }
        break;

    default:
        Con_Error("Rend_IsWallSectionPVisible: Invalid wall section %i\n",
                  section);
    }

    return true;
}

/*
 * Prepares the correct flat/texture for the passed poly.
 *
 * TODO: We shouldn't pass tex and isFlat and use the properties of the
 * surface instead. However due to dynamic texture replacements (missing
 * texture fixes) we cannot yet. Fixing these needs to be moved higher up
 * the logic chain (ie when a plane move exposes the missing surface the
 * fix should be made).
 *
 * @param poly:     Ptr to the poly to apply the texture to.
 * @param tex:      Texture/Flat id number. If -1 we'll apply the
 *                  special "missing" texture instead.
 * @param isFlat:   (True) "tex" is a flat id ELSE "tex" is a texture id.
 * @return int      (-1) If the texture reference is invalid.
 *                  Else return the flags for the texture chosen.
 */
int Rend_PrepareTextureForPoly(rendpoly_t *poly, surface_t *surface)
{
    if(surface->texture == 0)
        return -1;

    if(R_IsSkySurface(surface))
        return 0;

    if(surface->texture == -1) // A missing texture, draw the "missing" graphic
    {
        poly->tex.id = curtex = GL_PrepareDDTexture(DDT_MISSING);
        poly->tex.width = texw;
        poly->tex.height = texh;
        poly->tex.detail = texdetail;
        return 0 | TXF_GLOW; // Make it stand out
    }
    else
    {
        // Prepare the flat/texture
        if(surface->isflat)
            poly->tex.id = curtex = GL_PrepareFlat(surface->texture);
        else
            poly->tex.id = curtex = GL_PrepareTexture(surface->texture);

        poly->tex.width = texw;
        poly->tex.height = texh;
        poly->tex.detail = texdetail;

        // Return the parameters for this surface texture.
        return R_GraphicResourceFlags((surface->isflat? RC_FLAT : RC_TEXTURE),
                                      surface->texture);
    }
}

/*
 * Returns true if the quad has a division at the specified height.
 */
int Rend_CheckDiv(rendpoly_t *quad, int side, float height)
{
    int     i;

    for(i = 0; i < quad->wall.divs[side].num; i++)
        if(quad->wall.divs[side].pos[i] == height)
            return true;
    return false;
}

/*
 * Division will only happen if it must be done.
 * Converts quads to divquads.
 */
void Rend_WallHeightDivision(rendpoly_t *quad, const seg_t *seg, sector_t *frontsec,
                             int mode)
{
    int     i, k, vtx[2];       // Vertex indices.
    vertexowner_t *own;
    sector_t *sec;
    float   hi, low;
    float   sceil, sfloor;

    switch (mode)
    {
    case SEG_MIDDLE:
        hi = SECT_CEIL(frontsec);
        low = SECT_FLOOR(frontsec);
        break;

    case SEG_TOP:
        hi = SECT_CEIL(frontsec);
        low = SECT_CEIL(seg->backsector);
        if(SECT_FLOOR(frontsec) > low)
            low = SECT_FLOOR(frontsec);
        break;

    case SEG_BOTTOM:
        hi = SECT_FLOOR(seg->backsector);
        low = SECT_FLOOR(frontsec);
        if(SECT_CEIL(frontsec) < hi)
            hi = SECT_CEIL(frontsec);
        break;

    default:
        return;
    }

    vtx[0] = GET_VERTEX_IDX(seg->v1);
    vtx[1] = GET_VERTEX_IDX(seg->v2);
    quad->wall.divs[0].num = 0;
    quad->wall.divs[1].num = 0;

    // Check both ends.
    for(i = 0; i < 2; i++)
    {
        own = vertexowners + vtx[i];
        if(own->num > 1)
        {
            // More than one sectors! The checks must be made.
            for(k = 0; k < own->num; k++)
            {
                sec = SECTOR_PTR(own->list[k]);
                if(sec == frontsec)
                    continue;   // Skip this sector.
                if(sec == seg->backsector)
                    continue;

                sceil = SECT_CEIL(sec);
                sfloor = SECT_FLOOR(sec);

                // Divide at the sector's ceiling height?
                if(sceil > low && sceil < hi)
                {
                    quad->type = RP_DIVQUAD;
                    if(!Rend_CheckDiv(quad, i, sceil))
                        quad->wall.divs[i].pos[quad->wall.divs[i].num++] = sceil;
                }
                // Do we need to break?
                if(quad->wall.divs[i].num == RL_MAX_DIVS)
                    break;

                // Divide at the sector's floor height?
                if(sfloor > low && sfloor < hi)
                {
                    quad->type = RP_DIVQUAD;
                    if(!Rend_CheckDiv(quad, i, sfloor))
                        quad->wall.divs[i].pos[quad->wall.divs[i].num++] = sfloor;
                }
                // Do we need to break?
                if(quad->wall.divs[i].num == RL_MAX_DIVS)
                    break;
            }
            // We need to sort the divisions for the renderer.
            if(quad->wall.divs[i].num > 1)
            {
                // Sorting is required. This shouldn't take too long...
                // There seldom are more than one or two divisions.
                qsort(quad->wall.divs[i].pos, quad->wall.divs[i].num, sizeof(float),
                      i ? DivSortDescend : DivSortAscend);
            }
#ifdef RANGECHECK
            for(k = 0; k < quad->wall.divs[i].num; k++)
                if(quad->wall.divs[i].pos[k] > hi || quad->wall.divs[i].pos[k] < low)
                {
                    Con_Error("DivQuad: i=%i, pos (%f), hi (%f), "
                              "low (%f), num=%i\n", i, quad->wall.divs[i].pos[k],
                              hi, low, quad->wall.divs[i].num);
                }
#endif
        }
    }
}

/*
 * Calculates the placement for a middle texture (top, bottom, offset).
 * Texture must be prepared so texh is known.
 * texoffy may be NULL.
 * Returns false if the middle texture isn't visible (in the opening).
 */
int Rend_MidTexturePos(float *bottomleft, float *bottomright,
                       float *topleft, float *topright, float *texoffy,
                       float tcyoff, boolean lower_unpeg)
{
    int     side;
    float   openingTop, openingBottom;
    boolean visible[2] = {false, false};

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
                *(side? bottomright : bottomleft) + texh;
        }
        else
        {
            *(side? topright : topleft) += tcyoff;
            *(side? bottomright : bottomleft) = *(side? topright : topleft) - texh;
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

/*
 * Called by Rend_RenderWallSeg to render ALL wall sections.
 *
 * TODO: Combine all the boolean (hint) parameters into a single flags var.
 */
static void Rend_RenderWallSection(rendpoly_t *quad, const seg_t *seg, side_t *side,
                            sector_t *frontsec, surface_t *surface,
                            int mode, byte alpha,
                            boolean shadow, boolean shiny, boolean glow)
{
    int  i;
    int  segIndex = GET_SEG_IDX(seg);
    rendpoly_vertex_t *vtx;
    const byte *colorPtr = NULL;
    const byte *color2Ptr = NULL;

    // Select the correct colors for this surface.
    switch(mode)
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
        Con_Error("Rend_RenderWallSection: Invalid wall section mode %i", mode);
    }

    if(glow)        // Make it fullbright?
        quad->flags |= RPF_GLOW;

    // Check for neighborhood division?
    if(!(mode == SEG_MIDDLE && seg->backsector))
        Rend_WallHeightDivision(quad, seg, frontsec, mode);

    // Surface color/light.
    RL_VertexColors(quad, Rend_SectorLight(frontsec), colorPtr);

    // Alpha?
    for(i = 0, vtx = quad->vertices; i < quad->numvertices; i++, vtx++)
        vtx->color.rgba[3] = alpha;

    // Bottom color (if different from top)?
    if(color2Ptr != NULL)
    {
        for(i=0; i < 2; i++)
            memcpy(quad->vertices[i].color.rgba, color2Ptr, 3);
    }

    // Dynamic lights.
    quad->lights = DL_GetSegLightLinks(segIndex, mode);

    // Do BIAS lighting for this poly.
    SB_RendPoly(quad, false, frontsec, seginfo[segIndex].illum[1],
                &seginfo[segIndex].tracker[1], segIndex);

    // Smooth Texture Animation?
    Rend_PolyTexBlend(surface->texture, surface->isflat, quad);
    // Add the poly to the appropriate list
    RL_AddPoly(quad);

    if(shadow)      // Render Fakeradio polys for this seg?
        Rend_RadioWallSection(seg, quad);

    if(shiny)       // Render Shiny polys for this seg?
    {
        rendpoly_t q;

        // We're going to modify the polygon quite a bit.
        memcpy(&q, quad, sizeof(rendpoly_t));

        Rend_AddShinyPoly(surface->texture, surface->isflat, &q);
    }
}

/*
 * Same as above but for planes. Ultimately we should consider merging the
 * two into a Rend_RenderSurface.
 */
static void Rend_DoRenderPlane(rendpoly_t *poly, subsector_t *subsector,
                planeinfo_t* plane, surface_t *surface, float height, byte alpha,
                boolean shadow, boolean shiny,
                boolean glow)
{
    int i;
    int subIndex = GET_SUBSECTOR_IDX(subsector);
    rendpoly_vertex_t *vtx;

    if(glow)        // Make it fullbright?
        poly->flags |= RPF_GLOW;

    // Surface color/light.
    RL_PreparePlane(plane, poly, height, subsector);

    // Alpha?
    for(i = 0, vtx = poly->vertices; i < poly->numvertices; i++, vtx++)
        vtx->color.rgba[3] = alpha;

    // Dynamic lights. Check for sky.
    if(R_IsSkySurface(surface))
    {
        poly->lights = NULL;
        skyhemispheres |= (plane->type == PLN_FLOOR? SKYHEMI_LOWER : SKYHEMI_UPPER);
    }
    else
        poly->lights = DL_GetSubSecLightLinks(subIndex, plane->type);

    // Do BIAS lighting for this poly.
    SB_RendPoly(poly, plane->type, subsector->sector, plane->illumination,
                &plane->tracker, subIndex);

    // Smooth Texture Animation?
    Rend_PolyTexBlend(surface->texture, surface->isflat, poly);
    // Add the poly to the appropriate list
    RL_AddPoly(poly);

    //if(shadow)      // Fakeradio uses a seperate algorithm for planes.

    // Add a shine to this flat existence.
    if(shiny)
    {
        Rend_AddShinyPoly(surface->texture, surface->isflat, poly);
    }
}

/*
 * The sector height should've been checked by now.
 * This seriously needs to be rewritten! Witness the accumulation of hacks
 * on kludges...
 */
void Rend_RenderWallSeg(const seg_t *seg, sector_t *frontsec, int flags)
{
    int i;
    int texFlags;
    int solidSeg = false; // -1 means NEVER.
    sector_t *backsec;
    surface_t *surface;
    side_t *sid, *backsid, *side;
    line_t *ldef;
    float   ffloor, fceil, bfloor, bceil, fsh, bsh, mceil, tcxoff, tcyoff;
    rendpoly_t quad;
    float  *v1, *v2, *v3, *v4;
    boolean backSide = true;
    boolean mid_covers_top = false;
    boolean backsecSkyFix = false;

    // Let's first check which way this seg is facing.
    if(!Rend_FixedSegFacingDir(seg))
        return;

    backsec = seg->backsector;
    sid = SIDE_PTR(seg->linedef->sidenum[0]);
    backsid = SIDE_PTR(seg->linedef->sidenum[1]);
    ldef = seg->linedef;
    ldef->flags |= ML_MAPPED; // This line is now seen in the map.

    // Which side are we using?
    if(sid == seg->sidedef)
    {
        side = sid;
        backSide = false;
    }
    else
        side = backsid;

    if(backsec && backsec == seg->frontsector &&
       side->top.texture == 0 && side->bottom.texture == 0 && side->middle.texture == 0)
       return; // Ugh... an obvious wall seg hack. Best take no chances...

    ffloor = SECT_FLOOR(frontsec);
    fceil = SECT_CEIL(frontsec);
    fsh = fceil - ffloor;

    if(backsec)
    {
        bceil = SECT_CEIL(backsec);
        bfloor = SECT_FLOOR(backsec);
        bsh = bceil - bfloor;
    }
    else
        bfloor = bceil = bsh = 0;

    // Init the quad.
    quad.type = RP_QUAD;
    quad.flags = 0;
    quad.tex.detail = NULL;
    quad.intertex.detail = NULL;
    quad.sector = frontsec;
    quad.numvertices = 4;
    quad.isWall = true;

    v1 = quad.vertices[0].pos;
    v2 = quad.vertices[1].pos;
    v3 = quad.vertices[2].pos;
    v4 = quad.vertices[3].pos;

    // Get the start and end points.
    v1[VX] = v3[VX] = FIX2FLT(seg->v1->x);
    v1[VY] = v3[VY] = FIX2FLT(seg->v1->y);
    v2[VX] = v4[VX] = FIX2FLT(seg->v2->x);
    v2[VY] = v4[VY] = FIX2FLT(seg->v2->y);

    // Calculate the distances.
    quad.vertices[0].dist = quad.vertices[2].dist = Rend_PointDist2D(v1);
    quad.vertices[1].dist = quad.vertices[3].dist = Rend_PointDist2D(v2);

    // Some texture coordinates.
    quad.wall.length = seg->length;
    tcxoff = FIX2FLT(side->textureoffset + seg->offset);
    tcyoff = FIX2FLT(side->rowoffset);

    // Top wall section color offset?
    if(side->top.rgba[0] < 255 || side->top.rgba[1] < 255 || side->top.rgba[2] < 255)
    {
        for(i=0; i < 3; i++)
            topColor[i] = (byte)(((side->top.rgba[i]/ 255.0f)) * sLightColor[i]);

        topColorPtr = topColor;
    }
    else
        topColorPtr = sLightColor;

    // Mid wall section color offset?
    if(side->middle.rgba[0] < 255 || side->middle.rgba[1] < 255 || side->middle.rgba[2] < 255)
    {
        for(i=0; i < 3; i++)
            midColor[i] = (byte)(((side->middle.rgba[i]/ 255.0f)) * sLightColor[i]);

        midColorPtr = midColor;
    }
    else
        midColorPtr = sLightColor;

    // Bottom wall section color offset?
    if(side->bottom.rgba[0] < 255 || side->bottom.rgba[1] < 255 || side->bottom.rgba[2] < 255)
    {
        for(i=0; i < 3; i++)
            bottomColor[i] = (byte)(((side->bottom.rgba[i]/ 255.0f)) * sLightColor[i]);

        bottomColorPtr = bottomColor;
    }
    else
        bottomColorPtr = sLightColor;

    // The skyfixes
    if(frontsec->skyfix)
    {
        if(!backsec ||
           (backsec && backsec != seg->frontsector &&
            (bceil + backsec->skyfix < fceil + frontsec->skyfix)))
        {
            if(backsec && backSide && sid->middle.texture != 0)
            {
                // This traps the skyfix glitch as seen in ICARUS.wad MAP01's
                // engine tubes (scrolling mid texture doors)
            }
            else
            {
                quad.flags = RPF_SKY_MASK;
                quad.vertices[2].pos[VZ] = quad.vertices[3].pos[VZ] = fceil + frontsec->skyfix;
                quad.vertices[0].pos[VZ] = quad.vertices[1].pos[VZ] = fceil;
                quad.tex.id = 0;
                quad.lights = NULL;
                quad.intertex.id = 0;
                RL_AddPoly(&quad);
            }
        }
    }
    if(backsec && bceil - bfloor <= 0 &&
       ((R_IsSkySurface(&frontsec->SP_floorsurface) && R_IsSkySurface(&backsec->SP_floorsurface)) ||
        (R_IsSkySurface(&frontsec->SP_ceilsurface) && R_IsSkySurface(&backsec->SP_ceilsurface))))
    {
        quad.flags = RPF_SKY_MASK;
        quad.vertices[2].pos[VZ] = quad.vertices[3].pos[VZ] = fceil + frontsec->skyfix;
        quad.vertices[0].pos[VZ] = quad.vertices[1].pos[VZ] = bceil;
        quad.tex.id = 0;
        quad.lights = NULL;
        quad.intertex.id = 0;
        RL_AddPoly(&quad);
        backsecSkyFix = true;
    }

    // Create the wall sections.

    // A onesided seg?
    if(!backsec)
    {
        // We only need one wall section extending from floor to ceiling.
        if(Rend_IsWallSectionPVisible(seg->linedef, SEG_MIDDLE, backSide))
        {
            surface = &side->middle;
            texFlags = Rend_PrepareTextureForPoly(&quad, surface);

            // Is there a visible surface?
            if(texFlags != -1)
            {
                // Fill in the remaining quad data.
                quad.flags = 0;
                quad.vertices[2].pos[VZ] = quad.vertices[3].pos[VZ] = fceil;
                quad.vertices[0].pos[VZ] = quad.vertices[1].pos[VZ] = ffloor;

                quad.texoffx = tcxoff;
                quad.texoffy = tcyoff;
                if(ldef->flags & ML_DONTPEGBOTTOM)
                    quad.texoffy += texh - fsh;

                Rend_RenderWallSection(&quad, seg, side, frontsec, surface, SEG_MIDDLE,
                          /*Alpha*/    255,
                          /*Shadow?*/  !(flags & RWSF_NO_RADIO),
                          /*Shiny?*/   (surface->texture != -1),
                          /*Glow?*/    (texFlags & TXF_GLOW));
            }

            // This is guaranteed to be a solid segment.
            solidSeg = true;
        }
    }
    else if(backsec) // A twosided seg
    {
        // We may need multiple wall sections.
        // Determine which parts of the segment are visible.

        // Quite probably a masked texture. Won't be drawn if a visible
        // top or bottom texture is missing.
        if(Rend_IsWallSectionPVisible(seg->linedef, SEG_MIDDLE, backSide))
        {
            // Use actual (smoothed) sector heights (non-linked).
            float   rbceil = SECT_CEIL(seg->backsector);
            float  rbfloor = SECT_FLOOR(seg->backsector);
            float   rfceil = SECT_CEIL(seg->frontsector);
            float  rffloor = SECT_FLOOR(seg->frontsector);
            float   gaptop, gapbottom;
            byte    alpha = side->middle.rgba[3];

            surface = &side->middle;
            texFlags = Rend_PrepareTextureForPoly(&quad, surface);

            // Is there a visible surface?
            if(texFlags != -1)
            {
                // Calculate texture coordinates.
                quad.vertices[2].pos[VZ] = quad.vertices[3].pos[VZ] = gaptop = MIN_OF(rbceil, rfceil);
                quad.vertices[0].pos[VZ] = quad.vertices[1].pos[VZ] = gapbottom = MAX_OF(rbfloor, rffloor);

                if(bceil < fceil && side->top.texture == 0)
                {
                    mceil = quad.vertices[2].pos[VZ];
                    // Extend to cover missing top texture.
                    quad.vertices[2].pos[VZ] = quad.vertices[3].pos[VZ] = MAX_OF(bceil, fceil);
                    if(texh > quad.vertices[2].pos[VZ] - quad.vertices[0].pos[VZ])
                    {
                        mid_covers_top = true;  // At least partially...
                        tcyoff -= quad.vertices[2].pos[VZ] - mceil;
                    }
                }
                quad.texoffx = tcxoff;

                if(Rend_MidTexturePos
                   (&quad.vertices[0].pos[VZ], &quad.vertices[1].pos[VZ],
                    &quad.vertices[2].pos[VZ], &quad.vertices[3].pos[VZ],
                    &quad.texoffy, tcyoff,
                    0 != (ldef->flags & ML_DONTPEGBOTTOM)))
                {
                    // Should a solid segment be added here?
                    if(quad.vertices[2].pos[VZ] >= gaptop && quad.vertices[0].pos[VZ] <= gapbottom &&
                       quad.vertices[3].pos[VZ] >= gaptop && quad.vertices[1].pos[VZ] <= gapbottom)
                    {
                        if(!texmask && alpha == 255 && side->blendmode == 0)
                            solidSeg = true; // We could add clipping seg.

                        // Can the player walk through this surface?
                        if(!(ldef->flags & ML_BLOCKING) ||
                           !(viewplayer->flags & DDPF_NOCLIP))
                        {
                            mobj_t* mo = viewplayer->mo;
                            // If the player is close enough we should NOT add a solid seg
                            // otherwise they'd get HOM when they are directly on top of the
                            // line (eg passing through an opaque waterfall).
                            if(mo->subsector->sector == side->sector)
                            {
                                // Calculate 2D distance to line.
                                float a[2], b[2], c[2];
                                float distance;

                                a[VX] = FIX2FLT(ldef->v1->x);
                                a[VY] = FIX2FLT(ldef->v1->y);

                                b[VX] = FIX2FLT(ldef->v2->x);
                                b[VY] = FIX2FLT(ldef->v2->y);

                                c[VX] = FIX2FLT(mo->pos[VX]);
                                c[VY] = FIX2FLT(mo->pos[VY]);
                                distance = M_PointLineDistance(a, b, c);

                                if(distance < (FIX2FLT(mo->radius)*.8f))
                                {
                                    int temp = 0;

                                    // Fade it out the closer the viewplayer gets.
                                    solidSeg = false;
                                    temp = ((float)alpha / (FIX2FLT(mo->radius)*.8f));
                                    temp *= distance;

                                    // Clamp it.
                                    if(temp > 255)
                                        temp = 255;
                                    if(temp < 0)
                                        temp = 0;

                                    alpha = temp;
                                }
                            }
                        }
                    }

                    // Blendmode
                    if(sid->blendmode == BM_NORMAL && noSpriteTrans)
                        quad.blendmode = BM_ZEROALPHA; // "no translucency" mode
                    else
                        quad.blendmode = sid->blendmode;

                    // If alpha, masked or blended we must render as a vissprite
                    if(alpha < 255 || texmask || side->blendmode > 0)
                        quad.flags = RPF_MASKED;
                    else
                        quad.flags = 0;

                    Rend_RenderWallSection(&quad, seg, side, frontsec, surface, SEG_MIDDLE,
                              /*Alpha*/    alpha,
                              /*Shadow?*/  (solidSeg && !(flags & RWSF_NO_RADIO) && !texmask),
                              /*Shiny?*/   (solidSeg && surface->texture != -1 && !texmask),
                              /*Glow?*/    (texFlags & TXF_GLOW));
                }
            }
        }

        // Restore original type, height division may change this.
        quad.type = RP_QUAD;

        // Upper wall.
        if(Rend_IsWallSectionPVisible(seg->linedef, SEG_TOP, backSide) &&
           !mid_covers_top)
        {
            surface = &side->top;
            texFlags = Rend_PrepareTextureForPoly(&quad, surface);

            // Is there a visible surface?
            if(texFlags != -1)
            {
                // Calculate texture coordinates.
                quad.texoffx = tcxoff;
                quad.texoffy = tcyoff;
                if(!(ldef->flags & ML_DONTPEGTOP)) // Normal alignment to bottom.
                    quad.texoffy += texh - (fceil - bceil);

                quad.flags = 0;
                quad.vertices[2].pos[VZ] = quad.vertices[3].pos[VZ] = fceil;
                quad.vertices[0].pos[VZ] = quad.vertices[1].pos[VZ] = bceil;
                if(quad.vertices[0].pos[VZ] < ffloor)
                    quad.vertices[0].pos[VZ] = ffloor;

                Rend_RenderWallSection(&quad, seg, side, frontsec, surface, SEG_TOP,
                          /*Alpha*/    255,
                          /*Shadow?*/  !(flags & RWSF_NO_RADIO),
                          /*Shiny?*/   (surface->texture != -1),
                          /*Glow?*/    (texFlags & TXF_GLOW));
            }
        }

        // Restore original type, height division may change this.
        quad.type = RP_QUAD;

        // Lower wall.
        if(Rend_IsWallSectionPVisible(seg->linedef, SEG_BOTTOM, backSide))
        {
            surface = &side->bottom;
            texFlags = Rend_PrepareTextureForPoly(&quad, surface);

            // Is there a visible surface?
            if(texFlags != -1)
            {
                // Calculate texture coordinates.
                quad.texoffx = tcxoff;
                quad.texoffy = tcyoff;
                if(ldef->flags & ML_DONTPEGBOTTOM)
                    quad.texoffy += fceil - bfloor; // Align with normal middle texture.

                quad.flags = 0;
                quad.vertices[2].pos[VZ] = quad.vertices[3].pos[VZ] = bfloor;
                quad.vertices[0].pos[VZ] = quad.vertices[1].pos[VZ] = ffloor;
                if(quad.vertices[2].pos[VZ] > fceil)
                {
                    // Can't go over front ceiling, would induce polygon flaws.
                    quad.texoffy += quad.vertices[2].pos[VZ] - fceil;
                    quad.vertices[2].pos[VZ] = quad.vertices[3].pos[VZ] = fceil;
                }

                Rend_RenderWallSection(&quad, seg, side, frontsec, surface, SEG_BOTTOM,
                          /*Alpha*/    255,
                          /*Shadow?*/  !(flags & RWSF_NO_RADIO),
                          /*Shiny?*/   (surface->texture != -1),
                          /*Glow?*/    (texFlags & TXF_GLOW));
            }
        }
    }

    // Can we make this a solid segment in the clipper?
    if(solidSeg == -1)
        return; // NEVER (we have a hole we couldn't fix).

    if(solidSeg)
    {
        // We KNOW we can make it solid.
        C_AddViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]);
    }
    else // We'll have to determine whether we can...
    {
        if(backsec && backsec == seg->frontsector)
        {
            // An obvious hack, what to do though??
        }
        else if((bceil <= ffloor && (side->top.texture != 0 || side->middle.texture != 0)) ||
                (bfloor >= fceil && (side->bottom.texture != 0 || side->middle.texture !=0)))
        {
            // A closed gap.
            solidSeg = true;
        }
        else if(backsecSkyFix ||
                (bsh == 0 && bfloor > ffloor && bceil < fceil &&
                side->top.texture != 0 && side->bottom.texture != 0))
        {
            // A zero height back segment
            solidSeg = true;
        }

        if(solidSeg)
            C_AddViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]);
    }
}

int Rend_SectorLight(sector_t *sec)
{
    int     i;

    i = LevelFullBright ? 255 : sec->lightlevel;

    // Apply light adaptation
    Rend_ApplyLightAdaptation(&i);

    return i;
}

/*
 * Creates new occlusion planes from the subsector's sides.
 * Before testing, occlude subsector's backfaces. After testing occlude
 * the remaining faces, i.e. the forward facing segs. This is done before
 * rendering segs, so solid segments cut out all unnecessary oranges.
 */
void Rend_OccludeSubsector(subsector_t *sub, boolean forward_facing)
{
    sector_t *front = sub->sector, *back;
    int     segfacing;
    int     i;
    float   v1[2], v2[2], fronth[2], backh[2];
    float  *startv, *endv;
    seg_t  *seg;

    fronth[0] = FIX2FLT(front->planes[PLN_FLOOR].height);
    fronth[1] = FIX2FLT(front->planes[PLN_CEILING].height);

    if(devNoCulling || P_IsInVoid(viewplayer))
        return;

    for(i = 0; i < sub->linecount; i++)
    {
        seg = SEG_PTR(sub->firstline + i);
        // Occlusions can only happen where two sectors contact.
        if(!seg->linedef || !seg->backsector)
            continue;
        back = seg->backsector;
        v1[VX] = FIX2FLT(seg->v1->x);
        v1[VY] = FIX2FLT(seg->v1->y);
        v2[VX] = FIX2FLT(seg->v2->x);
        v2[VY] = FIX2FLT(seg->v2->y);
        // Which way should it be facing?
        segfacing = Rend_SegFacingDir(v1, v2);  // 1=front
        if(forward_facing != (segfacing != 0))
            continue;
        backh[0] = FIX2FLT(back->planes[PLN_FLOOR].height);
        backh[1] = FIX2FLT(back->planes[PLN_CEILING].height);
        // Choose start and end vertices so that it's facing forward.
        if(forward_facing)
        {
            startv = v1;
            endv = v2;
        }
        else
        {
            startv = v2;
            endv = v1;
        }
        // Do not create an occlusion for sky floors.
        if(!R_IsSkySurface(&back->SP_floorsurface) || !R_IsSkySurface(&front->SP_floorsurface))
        {
            // Do the floors create an occlusion?
            if((backh[0] > fronth[0] && vy <= backh[0]) ||
               (backh[0] < fronth[0] && vy >= fronth[0]))
            {
                // Occlude down.
                C_AddViewRelOcclusion(startv, endv, MAX_OF(fronth[0], backh[0]), false);
            }
        }
        // Do not create an occlusion for sky ceilings.
        if(!R_IsSkySurface(&back->SP_ceilsurface) || !R_IsSkySurface(&front->SP_ceilsurface))
        {
            // Do the ceilings create an occlusion?
            if((backh[1] < fronth[1] && vy >= backh[1]) ||
               (backh[1] > fronth[1] && vy <= fronth[1]))
            {
                // Occlude up.
                C_AddViewRelOcclusion(startv, endv, MIN_OF(fronth[1], backh[1]), true);
            }
        }
    }
}

void Rend_RenderPlane(planeinfo_t *plane, subsector_t *subsector,
                      sectorinfo_t *sin, boolean checkSelfRef)
{
    rendpoly_t poly;
    sector_t *sector = subsector->sector, *link = NULL;
    int     texFlags;
    float   height;
    surface_t *surface;

    // Sky planes of self-referrencing hack sectors are never rendered.
    if(checkSelfRef && sin->selfRefHack && R_IsSkySurface(&sector->planes[plane->type].surface))
        return;

    // Determine the height of the plane.
    if(sin->planeinfo[plane->type].linked)
    {
        poly.sector = link =
            R_GetLinkedSector(sin->planeinfo[plane->type].linked, plane->type);

        // This sector has an invisible plane.
        height = SECT_INFO(link)->planeinfo[plane->type].visheight;

        surface = &link->planes[plane->type].surface;
    }
    else
    {
        poly.sector = sector;
        height = sin->planeinfo[plane->type].visheight;
        if(plane->type == PLN_CEILING)
            height += sector->skyfix;  // Add the skyfix.

        surface = &sector->planes[plane->type].surface;
    }

    // We don't render planes for unclosed sectors when the polys would
    // be added to the skymask (a DOOM.EXE renderer hack).
    if(sin->unclosed && R_IsSkySurface(surface))
        return;

    // Has the texture changed?
    if(surface->texture != plane->pic)
    {
        plane->pic = surface->texture;

        // The sky?
        if(R_IsSkySurface(surface))
            plane->flags |= RPF_SKY_MASK;
        else
            plane->flags &= ~RPF_SKY_MASK;
    }

    // Is the plane visible?
    if((plane->type == PLN_FLOOR && vy > height) ||
       (plane->type == PLN_CEILING && vy < height))
    {
        texFlags =
            Rend_PrepareTextureForPoly(&poly, surface);

        // Is there a visible surface?
        if(texFlags != -1)
        {
            // Fill in the remaining quad data.
            poly.type = RP_FLAT; // We're creating a flat.
            poly.flags = plane->flags;
            poly.isWall = false;

            // Check for sky.
            if(!R_IsSkySurface(surface))
            {
                poly.texoffx = sector->planes[plane->type].offx;
                poly.texoffy = sector->planes[plane->type].offy;
            }

            Rend_DoRenderPlane(&poly, subsector, plane, surface, height,
                /*Alpha*/    255,
                /*Shadow?*/  false, // unused
                /*Shiny?*/   (surface->texture != -1),
                /*Glow?*/    (texFlags & TXF_GLOW));
        }
    }
}

void Rend_RenderSubsector(int ssecidx)
{
    subsector_t *ssec = SUBSECTOR_PTR(ssecidx);
    int     i;
    unsigned long j;
    seg_t  *seg;
    sector_t *sect = ssec->sector;
    int     sectoridx = GET_SECTOR_IDX(sect);
    sectorinfo_t *sin = secinfo + sectoridx;
    int     flags = 0;
    float   sceil = sin->planeinfo[PLN_CEILING].visheight;
    float  sfloor = sin->planeinfo[PLN_FLOOR].visheight;
    lumobj_t *lumi;             // Lum Iterator, or 'snow' in Finnish. :-)
    subsectorinfo_t *subin;
    boolean checkSelfRef[2] = {false, false};

    if(sceil - sfloor <= 0 || ssec->numverts < 3)
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
    sin->flags |= SIF_VISIBLE;

    // Retrieve the sector light color.
    sLightColor = R_GetSectorLightColor(sect);

    // Dynamic lights.
    if(useDynLights)
        DL_ProcessSubsector(ssec);

    // Prepare for FakeRadio.
    Rend_RadioInitForSector(sect);
    Rend_RadioSubsectorEdges(ssec);

    Rend_OccludeSubsector(ssec, false);
    // Determine which dynamic light sources in the subsector get clipped.
    for(lumi = dlSubLinks[ssecidx]; lumi; lumi = lumi->ssNext)
    {
        lumi->flags &= ~LUMF_CLIPPED;
        // FIXME: Determine the exact centerpoint of the light in
        // DL_AddLuminous!
        if(!C_IsPointVisible
           (FIX2FLT(lumi->thing->pos[VX]), FIX2FLT(lumi->thing->pos[VY]),
            FIX2FLT(lumi->thing->pos[VZ]) + lumi->center))
            lumi->flags |= LUMF_CLIPPED;    // Won't have a halo.
    }
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

    // Draw the walls.
    for(j = ssec->linecount, seg = &segs[ssec->firstline]; j > 0;
        --j, seg++)
    {
        if(seg->linedef != NULL)    // "minisegs" have no linedefs.
        {
            lineinfo_t* linfo = LINE_INFO(seg->linedef);

            Rend_RenderWallSeg(seg, sect, flags);

            // If this is a "root" of self-referencing hack sector
            // we should check whether the planes SHOULD be rendered.
            if(linfo->selfrefhackroot)
            {
                // NOTE: we already KNOW its twosided.
                // These checks are placed here instead of in the initial
                // selfref hack test as the textures can change at any time,
                // whilest the front/back sectors can't.
                if(SIDE_PTR(seg->linedef->sidenum[0])->top.texture == 0 &&
                   SIDE_PTR(seg->linedef->sidenum[1])->top.texture == 0)
                    checkSelfRef[PLN_CEILING] = true;

                if(SIDE_PTR(seg->linedef->sidenum[0])->bottom.texture == 0 &&
                   SIDE_PTR(seg->linedef->sidenum[1])->bottom.texture == 0)
                    checkSelfRef[PLN_FLOOR] = true;
            }
        }
    }

    // Is there a polyobj on board?
    if(ssec->poly)
    {
        for(i = 0; i < ssec->poly->numsegs; ++i)
            Rend_RenderWallSeg(ssec->poly->segs[i], sect, RWSF_NO_RADIO);
    }

    subin = &subsecinfo[ssecidx];
    // Render all planes of this sector.
    for(i = 0; i < NUM_PLANES; ++i)
    {
        Rend_RenderPlane(&subin->plane[i], ssec, sin, (i < 2? checkSelfRef[i] : false));
    }
}

void Rend_RenderNode(uint bspnum)
{
    node_t *bsp;
    int     side;

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

/*
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
void Rend_CalcLightRangeModMatrix(cvar_t* unused)
{
    int r, j, n;
    int mid = MOD_RANGE / 2;
    float f, mod, factor;
    double multiplier, mx;

    memset(lightRangeModMatrix, 0, (sizeof(byte) * 255) * MOD_RANGE);

    if(r_lightcompression > 0)
        factor = r_lightcompression;
    else
        factor = 1;

    multiplier = r_lightAdaptMul / MOD_RANGE;

    for(n = 0, r = -mid; r < mid; ++n, ++r)
    {
        // Calculate the light mod value for this range
        mod = (MOD_RANGE + n) / 255.0f;

        // Calculate the multiplier.
        mx = (r * multiplier) * MOD_RANGE;

        for(j = 0; j < 255; ++j)
        {
            if(r < 0)  // Dark to light range
            {
                // Apply the mod factor
                f = -((mod * j) / (n + 1));

                // Apply the multiplier
                f += -r * ((f * (mx * j)) * r_lightAdaptRamp);
            }
            else  // Light to dark range
            {
                f = ((255 - j) * mod) / (MOD_RANGE - n);
                f -= r * ((f * (mx * (255 - j))) * r_lightAdaptRamp);
            }

            // Adjust the white point/dark point
            if(r_lightcompression >= 0)
                f += (int)((255.f - j) * (r_lightcompression / 255.f));
            else
                f += (int)((j) * (r_lightcompression / 255.f));

            // Apply the linear range shift
            f += r_lightrangeshift;

            // Round to nearest (signed) whole.
            if(f >= 0)
                f += 0.5f;
            else
                f -= 0.5f;

            // Clamp the result as a modifier to the light value (j).
            if(r < 0)
            {
                if((j+f) >= 255)
                    f = 254 - j;
            }
            else
            {
                if((j+f) <= 0)
                    f = -j;
            }

            // Lower than the ambient limit?
            if(j+f <= r_ambient)
                f = r_ambient - j;

            // Insert it into the matrix
            lightRangeModMatrix[n][j] = (signed short) f;
        }
    }
}

/*
 * Grabs the light value of the sector each player is currently
 * in and chooses an appropriate light range.
 *
 * TODO: Interpolate between current range and last when player
 *       changes sector.
 */
void Rend_RetrieveLightSample(void)
{
    int i, diff, midpoint;
    short light;
    float range;
    float mod;
    float inter;
    ddplayer_t *player;
    subsector_t *sub;

    unsigned int currentTime = Sys_GetRealTime();

    midpoint = MOD_RANGE / 2;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(!players[i].ingame)
            continue;

        player = &players[i];
        if(!player->mo || !player->mo->subsector)
            continue;

        sub = player->mo->subsector;

        // In some circumstances we should disable light adaptation.
        if(LevelFullBright || P_IsInVoid(player))
        {
            playerLightRange[i] = midpoint;
            continue;
        }

        // Evaluate the light level at the point this player is at.
        // Uses a very simply method which uses the sector in which the player
        // is at currently and uses the light level of that sector.
        // Really this is junk. We should evaluate the lighting condition using
        // a much better method (perhaps obtain an average color from all the
        // vertexes (selected lists only) rendered in the previous frame?)
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

            // TODO: Should be affected by BIAS sources...
            LG_Evaluate(pos, color);
            light = (int) ((color[0] + color[1] + color[2]) / 3);
        }
        else*/
        {
            // use the light level of the sector they are currently in.
            light = sub->sector->lightlevel;
        }

        // Pick the range based on the current lighting conditions compared
        // to the previous lighting conditions.
        // We adapt to predominantly bright lighting conditions (mere seconds)
        // much quicker than very dark lighting conditions (upto 30mins).
        // Obviously that would be much too slow for a fast paced action game.
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
                mod = -(((diff - (diff * inter)) / MOD_RANGE) * 25); //midpoint
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
                mod = ((diff - (diff * inter)) / MOD_RANGE) * 25;
            }
        }
        else
            mod = 0;

        range = midpoint -
                ((/*( ((float) light / 255.f) * 10) +*/ mod) * r_lightAdapt);
        // Round to nearest whole.
        range += 0.5f;

        // Clamp the range
        if(range >= MOD_RANGE)
            range = MOD_RANGE - 1;
        else if(range < 0)
            range = 0;

        playerLightRange[i] = (int) range;

        // Have the lighting conditions changed?

        // Light differences between sectors?
        if(!(playerLastLightSample[i].sector == sub->sector))
        {
            // If this sample is different to the previous sample
            // update the last sample value;
            if(playerLastLightSample[i].sector)
                playerLastLightSample[i].value = playerLastLightSample[i].sector->lightlevel;
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

/*
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
void Rend_ApplyLightAdaptation(int* lightvar)
{
    // The default range.
    int range = MOD_RANGE / 2;
    int lightval;

    if(lightvar == NULL)
        return; // Can't apply adaptation to a NULL val ptr...

    lightval = *lightvar;

    if(lightval > 255)
        lightval = 255;
    else if(lightval < 0)
        lightval = 0;

    // Apply light adaptation?
    if(r_lightAdapt)
        range = playerLightRange[viewplayer - players];

    *lightvar += lightRangeModMatrix[range][lightval];
}

/*
 * Same as above but instead of applying light adaptation to the var directly
 * it returns the light adaptation value for the passed light value.
 *
 * @param lightvalue    Light value to look up the adaptation value of.
 * @return int          Adaptation value for the passed light value.
 */
int Rend_GetLightAdaptVal(int lightvalue)
{
    int range = MOD_RANGE / 2;
    int lightval = lightvalue;

    if(lightval > 255)
        lightval = 255;
    else if(lightval < 0)
        lightval = 0;

    if(r_lightAdapt)
        range = playerLightRange[viewplayer - players];

    return lightRangeModMatrix[range][lightval];
}

static void DrawRangeBox(int x, int y, int w, int h, ui_color_t *c)
{
    UI_GradientEx(x, y, w, h, 6,
                  c ? c : UI_COL(UIC_BG_MEDIUM),
                  c ? c : UI_COL(UIC_BG_LIGHT),
                  1, 1);
    UI_DrawRectEx(x, y, w, h, 6, false, c ? c : UI_COL(UIC_BRD_HI),
                  NULL, 1, -1);
}

#define bWidth 1.0f
#define bHeight ((bWidth / MOD_RANGE) * 255.0f)
#define BORDER 20

/*
 * Draws the lightRangeModMatrix (for debug)
 */
void R_DrawLightRange(void)
{
    int i, r;
    int y;
    char *title = "Light Range Matrix";
    signed short v;
    byte a, c;
    ui_color_t color;

    if(!debugLightModMatrix)
        return;

    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, glScreenWidth, glScreenHeight, -1, 1);

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
        for(i = 0; i < 255; i++)
        {
            // Get the result of the source light level + offset
            v = i + lightRangeModMatrix[r-1][i];

            c = (byte) v;

            // Show the ambient light modifier using alpha as well
            if(r_ambient < 0 && i > r_ambient)
                a = 128;
            else if(r_ambient > 0 && i < r_ambient)
                a = 128;
            else
                a = 255;

            // Draw in red if the range matches that of the current viewplayer
            if(r == playerLightRange[viewplayer - players])
                gl.Color4ub(255, 0, 0, 255);
            else
                gl.Color4ub(c, c, c, a);

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
                 -12, true, true, UI_COL(UIC_TITLE), .7f);

    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
}

/*
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
 * @param   alignToBase (TRUE) = Align the base of the box to the Z coordinate.
 */
void Rend_DrawBBox(float pos3f[3], float w, float l, float h, float color3f[3],
                   float alpha, float br, boolean alignToBase)
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

/*
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

/*
 * Renders bounding boxes for all mobj's (linked in sec->thinglist, except
 * the console player) in all sectors that are currently marked as vissible.
 *
 * Depth test is disabled to show all mobjs that are being rendered, regardless
 * if they are actually vissible (hidden by previously drawn map geometry).
 */
static void Rend_RenderBoundingBoxes(void)
{
    mobj_t *mo;
    int     i;
    sector_t *sec;
    float   size;
    float   red[3] = { 1, 0.2f, 0.2f}; // non-solid objects
    float   green[3] = { 0.2f, 1, 0.2f}; // solid objects
    float   yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles
    float   alpha;
    float   eye[3] = {FIX2FLT(viewplayer->mo->pos[VX]),
                      FIX2FLT(viewplayer->mo->pos[VY]),
                      FIX2FLT(viewplayer->mo->pos[VZ])};
    float   pos[3];

    if(!devMobjBBox || netgame)
        return;

    if(usingFog)
        gl.Disable(DGL_FOG);

    gl.Disable(DGL_DEPTH_TEST);
    gl.Enable(DGL_TEXTURING);
    gl.Disable(DGL_CULL_FACE);

    gl.Bind(ddTextures[DDT_BBOX]);
    GL_BlendMode(BM_ADD);

    // For every sector
    for(i = 0; i < numsectors; i++)
    {
        // Is it vissible?
        if(!(secinfo[i].flags & SIF_VISIBLE))
            continue;

        // For every mobj in the sector's thinglist
        sec = SECTOR_PTR(i);
        for(mo = sec->thinglist; mo; mo = mo->snext)
        {
            if(mo == players[consoleplayer].mo)
                continue; // We don't want the console player.

            pos[VX] = FIX2FLT(mo->pos[VX]);
            pos[VY] = FIX2FLT(mo->pos[VY]);
            pos[VZ] = FIX2FLT(mo->pos[VZ]);
            alpha = 1 - ((M_Distance(pos, eye)/(glScreenWidth/2))/4);

            if(alpha < .25f)
                alpha = .25f; // Don't make them totally invisible.

            // Draw a bounding box in an appropriate color.
            size = FIX2FLT(mo->radius);
            Rend_DrawBBox(pos, size, size, FIX2FLT(mo->height)/2,
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

// Console commands.
D_CMD(Fog)
{
    int     i;

    if(argc == 1)
    {
        Con_Printf("Usage: %s (cmd) (args)\n", argv[0]);
        Con_Printf("Commands: on, off, mode, color, start, end, density.\n");
        Con_Printf("Modes: linear, exp, exp2.\n");
        //Con_Printf( "Hints: fastest, nicest, dontcare.\n");
        Con_Printf("Color is given as RGB (0-255).\n");
        Con_Printf
            ("Start and end are for linear fog, density for exponential.\n");
        return true;
    }
    if(!stricmp(argv[1], "on"))
    {
        GL_UseFog(true);
        Con_Printf("Fog is now active.\n");
    }
    else if(!stricmp(argv[1], "off"))
    {
        GL_UseFog(false);
        Con_Printf("Fog is now disabled.\n");
    }
    else if(!stricmp(argv[1], "mode") && argc == 3)
    {
        if(!stricmp(argv[2], "linear"))
        {
            gl.Fog(DGL_FOG_MODE, DGL_LINEAR);
            Con_Printf("Fog mode set to linear.\n");
        }
        else if(!stricmp(argv[2], "exp"))
        {
            gl.Fog(DGL_FOG_MODE, DGL_EXP);
            Con_Printf("Fog mode set to exp.\n");
        }
        else if(!stricmp(argv[2], "exp2"))
        {
            gl.Fog(DGL_FOG_MODE, DGL_EXP2);
            Con_Printf("Fog mode set to exp2.\n");
        }
        else
            return false;
    }
    else if(!stricmp(argv[1], "color") && argc == 5)
    {
        for(i = 0; i < 3; i++)
            fogColor[i] = strtol(argv[2 + i], NULL, 0) /*/255.0f */ ;
        fogColor[3] = 255;
        gl.Fogv(DGL_FOG_COLOR, fogColor);
        Con_Printf("Fog color set.\n");
    }
    else if(!stricmp(argv[1], "start") && argc == 3)
    {
        gl.Fog(DGL_FOG_START, strtod(argv[2], NULL));
        Con_Printf("Fog start distance set.\n");
    }
    else if(!stricmp(argv[1], "end") && argc == 3)
    {
        gl.Fog(DGL_FOG_END, strtod(argv[2], NULL));
        Con_Printf("Fog end distance set.\n");
    }
    else if(!stricmp(argv[1], "density") && argc == 3)
    {
        gl.Fog(DGL_FOG_DENSITY, strtod(argv[2], NULL));
        Con_Printf("Fog density set.\n");
    }
    else
        return false;
    // Exit with a success.
    return true;
}
