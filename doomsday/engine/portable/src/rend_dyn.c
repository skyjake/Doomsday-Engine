/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * rend_dyn.c: Dynamic Lights
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
  PROF_DYN_INIT_DEL,
  PROF_DYN_INIT_ADD,
  PROF_DYN_INIT_LINK
END_PROF_TIMERS()

#define X_TO_DLBX(cx)           ( ((cx) - dlBlockOrig[VX]) >> (FRACBITS+7) )
#define Y_TO_DLBY(cy)           ( ((cy) - dlBlockOrig[VY]) >> (FRACBITS+7) )
#define DLB_ROOT_DLBXY(bx, by)  (dlBlockLinks + bx + by*dlBlockWidth)
#define LUM_FACTOR(dist)        (1.5f - 1.5f*(dist)/lum->radius)

// TYPES -------------------------------------------------------------------

typedef struct {
    boolean isLit;
    float   height;
    DGLuint decorMap;
} planeitervars_t;

typedef struct {
    float   color[3];
    float   size;
    float   flareSize;
    float   xOffset, yOffset;
} lightconfig_t;

typedef struct seglight_s {
    dynlight_t *wallSection[3];
} seglight_t;

typedef struct subseclight_s {
    uint planeCount;
    dynlight_t **planes;
} subseclight_t;

typedef struct lumcontact_s {
    struct lumcontact_s *next;  // Next in the subsector.
    struct lumcontact_s *nextUsed;  // Next used contact.
    lumobj_t *lum;
} lumcontact_t;

typedef struct contactfinder_data_s {
    fixed_t box[4];
    boolean didSpread;
    lumobj_t *lum;
    int     firstValid;
} contactfinder_data_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int useBias;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean dlInited = false;
int     useDynLights = true, dlBlend = 0;
float   dlFactor = 0.7f;        // was 0.6f
int     useWallGlow = true;
float   glowHeightFactor = 3; // glow height as a multiplier
int     glowHeightMax = 100;     // 100 is the default (0-1024)
float   glowFogBright = .15f;
int     dlMaxRad = 256;         // Dynamic lights maximum radius.
float   dlRadFactor = 3;
unsigned int maxDynLights = 0;
int     useMobjAutoLights = true; // Enable automaticaly calculated lights
                                  // attached to mobjs.
byte    rendInfoLums = false;

int     dlMinRadForBias = 136; // Lights smaller than this will NEVER
                               // be converted to BIAS sources.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static lumobj_t *luminousList = NULL;
static unsigned int numLuminous = 0, maxLuminous = 0;

// Dynlight nodes.
static dynlight_t *dynFirst, *dynCursor;

static lumobj_t **dlBlockLinks = 0;
static fixed_t dlBlockOrig[3];
static int dlBlockWidth, dlBlockHeight;    // In 128 blocks.

static lumobj_t **dlSubLinks = 0;

// A list of dynlight nodes for each surface (seg, subsector-planes[]).
// The segs are indexed by seg index, subSecs are indexed by
// subsector index.
static seglight_t *segLightLinks;
static subseclight_t *subSecLightLinks;

// List of unused and used lumobj-subsector contacts.
static lumcontact_t *contFirst, *contCursor;

// List of lumobj contacts for each subsector.
static lumcontact_t **subContacts;

// A framecount for each block. Used to prevent multiple processing of
// a block during one frame.
static int *spreadBlocks;

// Used when iterating planes.
static uint maxPlanes = 0;
static planeitervars_t *planeVars;

// CODE --------------------------------------------------------------------

void DL_Register(void)
{
    // Cvars
    C_VAR_INT("rend-glow", &glowingTextures, 0, 0, 1);
    C_VAR_INT("rend-glow-wall", &useWallGlow, 0, 0, 1);
    C_VAR_INT("rend-glow-height", &glowHeightMax, 0, 0, 1024);
    C_VAR_FLOAT("rend-glow-scale", &glowHeightFactor, 0, 0.1f, 10);
    C_VAR_FLOAT("rend-glow-fog-bright", &glowFogBright, 0, 0, 1);

    C_VAR_BYTE("rend-info-lums", &rendInfoLums, 0, 0, 1);

    C_VAR_INT("rend-light", &useDynLights, 0, 0, 1);
    C_VAR_INT("rend-light-blend", &dlBlend, 0, 0, 2);

    C_VAR_FLOAT("rend-light-bright", &dlFactor, 0, 0, 1);
    C_VAR_INT("rend-light-num", &maxDynLights, 0, 0, 8000);

    C_VAR_FLOAT("rend-light-radius-scale", &dlRadFactor, 0, 0.1f, 10);
    C_VAR_INT("rend-light-radius-max", &dlMaxRad, 0, 64, 512);
    C_VAR_INT("rend-light-radius-min-bias", &dlMinRadForBias, 0, 128, 1024);
    C_VAR_INT("rend-light-multitex", &useMultiTexLights, 0, 0, 1);
    C_VAR_INT("rend-mobj-light-auto", &useMobjAutoLights, 0, 0, 1);
    Rend_DecorRegister();
}

/**
 * Moves all used dynlight nodes to the list of unused nodes, so they
 * can be reused.
 */
static void DL_DeleteUsed(void)
{
    uint        i;

    // Start reusing nodes from the first one in the list.
    dynCursor = dynFirst;
    contCursor = contFirst;

    // Clear the surface light links.
    memset(segLightLinks, 0, numsegs * sizeof(seglight_t));
    for(i = 0; i < numsubsectors; ++i)
    {
        if(subSecLightLinks[i].planeCount)
            memset(subSecLightLinks[i].planes, 0,
                   subSecLightLinks[i].planeCount * sizeof(dynlight_t*));
    }

    // Clear lumobj contacts.
    memset(subContacts, 0, numsubsectors * sizeof(lumcontact_t *));
}

/**
 * Returns a new dynlight node. If the list of unused nodes is empty,
 * a new node is created.
 */
dynlight_t *DL_New(float *s, float *t)
{
    dynlight_t *dyn;

    // Have we run out of nodes?
    if(dynCursor == NULL)
    {
        dyn = Z_Malloc(sizeof(dynlight_t), PU_STATIC, NULL);

        // Link the new node to the list.
        dyn->nextUsed = dynFirst;
        dynFirst = dyn;
    }
    else
    {
        dyn = dynCursor;
        dynCursor = dynCursor->nextUsed;
    }

    dyn->next = NULL;
    dyn->flags = 0;

    if(s)
    {
        dyn->s[0] = s[0];
        dyn->s[1] = s[1];
    }
    if(t)
    {
        dyn->t[0] = t[0];
        dyn->t[1] = t[1];
    }

    return dyn;
}

/**
 * @return          The number of active lumobjs for this frame.
 */
unsigned int DL_GetNumLuminous(void)
{
    return numLuminous;
}

/**
 * Links the dynlight to the list.
 */
static void __inline DL_Link(dynlight_t *dyn, dynlight_t **list, unsigned int index)
{
    dyn->next = list[index];
    list[index] = dyn;
}

static void DL_LinkToSubSecPlane(dynlight_t *dyn, uint index, uint plane)
{
    DL_Link(dyn, &subSecLightLinks[index].planes[plane], 0);
}

/**
 * Returns a pointer to the list of dynlights for the subsector plane.
 */
dynlight_t *DL_GetSubSecPlaneLightLinks(uint ssec, uint plane)
{
    subseclight_t *ssll;

    if(!useDynLights)
        return NULL;

    assert(ssec < numsubsectors);
    ssll = &subSecLightLinks[ssec];
    assert(ssll->planeCount || plane < ssll->planeCount);

    return ssll->planes[plane];
}

static void DL_LinkToSegSection(dynlight_t *dyn, uint index, segsection_t segPart)
{
    DL_Link(dyn, &segLightLinks[index].wallSection[segPart], 0);
}

/**
 * Returns a pointer to the list of dynlights for the segment part.
 */
dynlight_t *DL_GetSegSectionLightLinks(uint seg, segsection_t section)
{
    if(!useDynLights)
        return NULL;

    return segLightLinks[seg].wallSection[section];
}

/**
 * Returns a new lumobj contact. If there are nodes in the list of unused
 * nodes, the new contact is taken from there.
 */
static lumcontact_t *DL_NewContact(lumobj_t *lum)
{
    lumcontact_t *con;

    if(contCursor == NULL)
    {
        con = Z_Malloc(sizeof(lumcontact_t), PU_STATIC, NULL);

        // Link to the list of lumcontact nodes.
        con->nextUsed = contFirst;
        contFirst = con;
    }
    else
    {
        con = contCursor;
        contCursor = contCursor->nextUsed;
    }

    con->lum = lum;
    return con;
}

/**
 * Link the contact to the subsector's list of contacts.
 * The lumobj is contacting the subsector.
 * This called if a light passes the sector spread test.
 * Returns true because this function is also used as an iterator.
 */
static boolean DL_AddContact(subsector_t *subsector, void *lum)
{
    lumcontact_t *con = DL_NewContact(lum);
    lumcontact_t **list = subContacts + GET_SUBSECTOR_IDX(subsector);

    con->next = *list;
    *list = con;
    return true;
}

#if 0 // Unused atm
void DL_ThingRadius(lumobj_t *lum, lightconfig_t *cf)
{
    lum->radius = cf->size * 40 * dlRadFactor;

    // Don't make a too small or too large light.
    if(lum->radius < 32)
        lum->radius = 32;
    if(lum->radius > dlMaxRad)
        lum->radius = dlMaxRad;

    lum->flareSize = cf->flareSize * 60 * (50 + haloSize) / 100.0f;

    if(lum->flareSize < 8)
        lum->flareSize = 8;
}
#endif

static void DL_ComputeLightColor(lumobj_t *lum, float *outRGB, float light)
{
    uint        i;

    if(light < 0)
        light = 0;
    if(light > 1)
        light = 1;
    light *= dlFactor;

    // If fog is enabled, make the light dimmer.
    // FIXME: This should be a cvar.
    if(usingFog)
        light *= .5f;           // Would be too much.

    // Multiply with the light color.
    if(lum->decorMap)
    {   // Decoration maps are pre-colored.
       for(i = 0; i < 3; ++i)
           outRGB[i] = light;
    }
    else
    {
        for(i = 0; i < 3; ++i)
        {
            outRGB[i] = light * lum->rgb[i];
        }
    }
}

void DL_InitForMap(void)
{
    uint        i;
    fixed_t     min[3], max[3];

    // First initialize the subsector links (root pointers).
    dlSubLinks = Z_Calloc(sizeof(lumobj_t*) * numsubsectors, PU_LEVEL, 0);

    // Then the blocklinks.
    R_GetMapSize(&min[0], &max[0]);

    // Origin has fixed-point coordinates.
    memcpy(&dlBlockOrig, &min, sizeof(min));
    max[VX] -= min[VX];
    max[VY] -= min[VY];
    dlBlockWidth  = (max[VX] >> (FRACBITS + 7)) + 1;
    dlBlockHeight = (max[VY] >> (FRACBITS + 7)) + 1;

    // Blocklinks is a table of lumobj_t pointers.
    dlBlockLinks =
        M_Realloc(dlBlockLinks,
                sizeof(lumobj_t*) * dlBlockWidth * dlBlockHeight);

    // Initialize the dynlight -> surface link list head ptrs.
    segLightLinks =
        Z_Calloc(numsegs * sizeof(seglight_t), PU_LEVELSTATIC, 0);
    subSecLightLinks =
        Z_Calloc(numsubsectors * sizeof(subseclight_t), PU_LEVELSTATIC, 0);
    for(i = 0; i < numsubsectors; ++i)
        subSecLightLinks[i].planeCount = 0;

    // Initialize lumobj -> subsector contacts.
    subContacts =
        Z_Calloc(numsubsectors * sizeof(lumcontact_t*), PU_LEVELSTATIC, 0);

    // A framecount was each block.
    spreadBlocks =
        Z_Malloc(sizeof(int) * dlBlockWidth * dlBlockHeight, PU_LEVEL, 0);
}

/**
 * Returns true if the coords are in range.
 */
static boolean DL_SegTexCoords(float *t, float top, float bottom,
                               lumobj_t *lum)
{
    float   lightZ = lum->pos[VZ] + lum->zOff;
    float   radius = lum->radius / DYN_ASPECT;
    float   radiusX2 = 2 * radius;

    t[0] = (lightZ + radius - top) / radiusX2;
    t[1] = t[0] + (top - bottom) / radiusX2;

    return t[0] < 1 && t[1] > 0;
}

/**
 * The front sector must be given because of polyobjs.
 */
static void DL_ProcessWallSeg(lumobj_t *lum, seg_t *seg, subsector_t *ssec)
{
#define SMIDDLE 0x1
#define STOP    0x2
#define SBOTTOM 0x4
    int         present = 0;
    sector_t   *backsec = seg->SG_backsector;
    side_t     *sdef = seg->sidedef;
    float       s[2], t[2];
    float       dist, pntLight[2];
    float       fceil, ffloor, bceil, bfloor;
    float       top[2], bottom[2];
    dynlight_t *dyn;
    uint        segindex = GET_SEG_IDX(seg);
    boolean     backSide = false;
    float       lumRGB[3];
    sector_t   *linkSec;

    linkSec = R_GetLinkedSector(ssec, PLN_CEILING);
    fceil  = linkSec->SP_ceilvisheight;
    linkSec = R_GetLinkedSector(ssec, PLN_FLOOR);
    ffloor = linkSec->SP_floorvisheight;

    // A zero-volume sector?
    if(fceil <= ffloor)
        return;

    // Which side?
    if(seg->linedef->L_frontside != seg->sidedef)
        backSide = true;

    if(backsec)
    {
        bceil  = backsec->SP_ceilvisheight;
        bfloor = backsec->SP_floorvisheight;
    }
    else
    {
        bceil = bfloor = 0;
    }

    // Let's begin with an analysis of the visible surfaces.
    // Is there a mid wall segment?
    if(Rend_IsWallSectionPVisible(seg->linedef, SEG_MIDDLE, backSide))
    {
        present |= SMIDDLE;
    }

    // Is there a top wall segment?
    if(Rend_IsWallSectionPVisible(seg->linedef, SEG_TOP, backSide))
        present |= STOP;

    // Is there a lower wall segment?
    if(Rend_IsWallSectionPVisible(seg->linedef, SEG_BOTTOM, backSide))
        present |= SBOTTOM;

    // There are no surfaces to light!
    if(!present)
        return;

    // We will only calculate light placement for segs that are facing
    // the viewpoint.
    if(!(seg->frameflags & SEGINF_FACINGFRONT))
        return;

    pntLight[VX] = lum->pos[VX];
    pntLight[VY] = lum->pos[VY];

    // Calculate distance between seg and light source.
    dist = ((seg->SG_v1->pos[VY] - pntLight[VY]) * (seg->SG_v2->pos[VX] - seg->SG_v1->pos[VX]) -
            (seg->SG_v1->pos[VX] - pntLight[VX]) * (seg->SG_v2->pos[VY] - seg->SG_v1->pos[VY]))
            / seg->length;

    // Is it close enough and on the right side?
    if(dist < 0 || dist > lum->radius)
        return; // Nope.

    // Do a scalar projection for the offset.
    s[0] = (-((seg->SG_v1->pos[VY] - pntLight[VY]) * (seg->SG_v1->pos[VY] - seg->SG_v2->pos[VY]) -
              (seg->SG_v1->pos[VX] - pntLight[VX]) * (seg->SG_v2->pos[VX] - seg->SG_v1->pos[VX]))
              / seg->length +
               lum->radius) / (2 * lum->radius);

    s[1] = s[0] + seg->length / (2 * lum->radius);

    // Would the light be visible?
    if(s[0] >= 1 || s[1] <= 0)
        return;  // Outside the seg.

    DL_ComputeLightColor(lum, lumRGB, LUM_FACTOR(dist));

    // Process the visible parts of the segment.
    if(present & SMIDDLE)
    {
        if(backsec)
        {
            texinfo_t      *texinfo;

            top[0]    = top[1]    = (fceil < bceil ? fceil : bceil);
            bottom[0] = bottom[1] = (ffloor > bfloor ? ffloor : bfloor);

            // We need the properties of the real flat/texture.
            if(sdef->SW_middleisflat)
                GL_GetFlatInfo(sdef->SW_middletexture, &texinfo);
            else
                GL_GetTextureInfo(sdef->SW_middletexture, &texinfo);

            Rend_MidTexturePos(&bottom[0], &bottom[1], &top[0], &top[1],
                               NULL, sdef->SW_middleoffy, texinfo->height,
                               seg->linedef ? (seg->linedef->
                                               mapflags & ML_DONTPEGBOTTOM) !=
                               0 : false);
        }
        else
        {
            top[0]    = top[1]    = fceil;
            bottom[0] = bottom[1] = ffloor;
        }

        if(DL_SegTexCoords(t, top[0], bottom[0], lum) &&
           DL_SegTexCoords(t, top[1], bottom[1], lum))
        {
            dyn = DL_New(s, t);
            memcpy(dyn->color, lumRGB, sizeof(float) * 3);
            dyn->texture = lum->tex;

            DL_LinkToSegSection(dyn, segindex, SEG_MIDDLE);
        }
    }
    if(present & STOP)
    {
        if(DL_SegTexCoords(t, fceil, MAX_OF(ffloor, bceil), lum))
        {
            dyn = DL_New(s, t);
            memcpy(dyn->color, lumRGB, sizeof(float) * 3);
            dyn->texture = lum->tex;

            DL_LinkToSegSection(dyn, segindex, SEG_TOP);
        }
    }
    if(present & SBOTTOM)
    {
        if(DL_SegTexCoords(t, MIN_OF(bfloor, fceil), ffloor, lum))
        {
            dyn = DL_New(s, t);
            memcpy(dyn->color, lumRGB, sizeof(float) * 3);
            dyn->texture = lum->tex;

            DL_LinkToSegSection(dyn, segindex, SEG_BOTTOM);
        }
    }
#undef SMIDDLE
#undef STOP
#undef SBOTTOM
}

/**
 * Generates one dynlight node per segplane glow. The light is attached to
 * the appropriate seg part.
 */
static void DL_CreateGlowLightPerPlaneForSegSection(seg_t *seg, segsection_t part,
                                                    float segtop, float segbottom,
                                                    boolean glow_floor,
                                                    boolean glow_ceil)
{
    uint        i, g, segindex;
    float       ceil, floor, top, bottom, s[2], t[2];
    float       glowHeight;
    dynlight_t *dyn;
    sector_t   *sect = seg->sidedef->sector;

    // Check the heights.
    if(segtop <= segbottom)
        return;                 // No height.

    ceil  = sect->SP_ceilvisheight;
    floor = sect->SP_floorvisheight;

    if(segtop > ceil)
        segtop = ceil;
    if(segbottom < floor)
        segbottom = floor;

    segindex = GET_SEG_IDX(seg);
    // FIXME: $nplanes
    for(g = 0; g < 2; ++g)
    {
        // Only do what's told.
        if((g == PLN_CEILING && !glow_ceil) || (g == PLN_FLOOR && !glow_floor))
            continue;

        glowHeight =
            (MAX_GLOWHEIGHT * sect->planes[g]->glow) * glowHeightFactor;

        // Don't make too small or too large glows.
        if(glowHeight <= 2)
            continue;

        if(glowHeight > glowHeightMax)
            glowHeight = glowHeightMax;

        // Calculate texture coords for the light.
        if(g == PLN_CEILING)
        {   // Ceiling glow.
            top = ceil;
            bottom = ceil - glowHeight;

            t[1] = t[0] = (top - segtop) / glowHeight;
            t[1]+= (segtop - segbottom) / glowHeight;

            if(t[0] > 1 || t[1] < 0)
                continue;
        }
        else
        {   // Floor glow.
            bottom = floor;
            top = floor + glowHeight;

            t[0] = t[1] = (segbottom - bottom) / glowHeight;
            t[0]+= (segtop - segbottom) / glowHeight;

            if(t[1] > 1 || t[0] < 0)
                continue;
        }

        // The horizontal direction is easy.
        s[0] = 0;
        s[1] = 1;

        dyn = DL_New(s, t);
        dyn->texture = GL_PrepareLSTexture(LST_GRADIENT, NULL);

        for(i = 0; i < 3; ++i)
        {
            dyn->color[i] = sect->planes[g]->glowrgb[i] * dlFactor;

            // In fog, additive blending is used. The normal fog color
            // is way too bright.
            if(usingFog)
                dyn->color[i] *= glowFogBright;
        }
        DL_LinkToSegSection(dyn, segindex, part);
    }
}

/**
 * If necessary, generate dynamic lights for plane glow.
 */
static void DL_ProcessSegForGlow(seg_t *seg, sector_t *sect)
{
    boolean     do_floor = (sect->SP_floorglow > 0)? true : false;
    boolean     do_ceil = (sect->SP_ceilglow > 0)? true : false;
    sector_t   *back = seg->SG_backsector;
    side_t     *sdef = seg->sidedef;
    float       fceil, ffloor, bceil, bfloor;
    float       opentop, openbottom;    //, top, bottom;
    boolean     backSide = false;

    // Check if this segment is actually facing our way.
    if(!(seg->frameflags & SEGINF_FACINGFRONT))
        return;                 // Nope...

    // Which side?
    if(seg->linedef->L_frontside != seg->sidedef)
        backSide = true;

    // Visible plane heights.
    fceil  = sect->SP_ceilvisheight;
    ffloor = sect->SP_floorvisheight;

    // Determine which portions of the segment get lit.
    if(!back)
    {
        // One sided.
        DL_CreateGlowLightPerPlaneForSegSection(seg, SEG_MIDDLE, fceil, ffloor,
                                                do_floor, do_ceil);
    }
    else
    {
        // Two-sided.
        bceil  = back->SP_ceilvisheight;
        bfloor = back->SP_floorvisheight;
        opentop = MIN_OF(fceil, bceil);
        openbottom = MAX_OF(ffloor, bfloor);

        // The glow can only be visible in the front sector's height range.

        // Is there a middle?
        if(Rend_IsWallSectionPVisible(seg->linedef, SEG_MIDDLE, backSide))
        {
            texinfo_t      *texinfo = NULL;

            if(sdef->SW_middletexture > 0)
            {
                if(sdef->SW_middleisflat)
                    GL_GetFlatInfo(sdef->SW_middletexture, &texinfo);
                else
                    GL_GetTextureInfo(sdef->SW_middletexture, &texinfo);
            }

            if(!texinfo->masked)
            {
                DL_CreateGlowLightPerPlaneForSegSection(seg, SEG_MIDDLE, opentop,
                                                        openbottom, do_floor, do_ceil);
            }
        }

        // Top?
        if(Rend_IsWallSectionPVisible(seg->linedef, SEG_TOP, backSide))
        {
            DL_CreateGlowLightPerPlaneForSegSection(seg, SEG_TOP, fceil, bceil,
                                                    do_floor, do_ceil);
        }

        // Bottom?
        if(Rend_IsWallSectionPVisible(seg->linedef, SEG_BOTTOM, backSide))
        {
            DL_CreateGlowLightPerPlaneForSegSection(seg, SEG_BOTTOM, bfloor, ffloor,
                                                    do_floor, do_ceil);
        }
    }
}

void DL_Clear(void)
{
    if(luminousList)
        Z_Free(luminousList);
    luminousList = 0;
    maxLuminous = numLuminous = 0;

    M_Free(dlBlockLinks);
    dlBlockLinks = 0;
    dlBlockOrig[VX] = dlBlockOrig[VY] = 0;
    dlBlockWidth = dlBlockHeight = 0;

    if(planeVars)
        M_Free(planeVars);
    maxPlanes = 0;
}

void DL_ClearForFrame(void)
{
#ifdef DD_PROFILE
    static int i;

    if(++i > 40)
    {
        i = 0;
        PRINT_PROF(PROF_DYN_INIT_DEL);
        PRINT_PROF(PROF_DYN_INIT_ADD);
        PRINT_PROF(PROF_DYN_INIT_LINK);
    }
#endif

    // Clear all the roots.
    memset(dlSubLinks, 0, sizeof(lumobj_t *) * numsubsectors);
    memset(dlBlockLinks, 0, sizeof(lumobj_t *) * dlBlockWidth * dlBlockHeight);

    numLuminous = 0;
}

/**
 * Allocates a new lumobj and returns a pointer to it.
 */
unsigned int DL_NewLuminous(void)
{
    lumobj_t   *newList;

    numLuminous++;

    // Only allocate memory when it's needed.
    // FIXME: No upper limit?
    if(numLuminous > maxLuminous)
    {
        maxLuminous *= 2;

        // The first time, allocate thirty two lumobjs.
        if(!maxLuminous)
            maxLuminous = 32;

        newList = Z_Malloc(sizeof(lumobj_t) * maxLuminous, PU_STATIC, 0);

        // Copy the old data over to the new list.
        if(luminousList)
        {
            memcpy(newList, luminousList,
                   sizeof(lumobj_t) * (numLuminous - 1));
            Z_Free(luminousList);
        }
        luminousList = newList;
    }

    // Clear the new lumobj.
    memset(luminousList + numLuminous - 1, 0, sizeof(lumobj_t));

    return numLuminous;         // == index + 1
}

/**
 * Returns a pointer to the lumobj with the given 1-based index.
 */
lumobj_t *DL_GetLuminous(unsigned int index)
{
    if(index == 0 || index > numLuminous)
        return NULL;

    return luminousList + index - 1;
}

/**
 * Returns true if we HAVE to use a dynamic light for this light defintion.
 */
static boolean DL_MustUseDynamic(ded_light_t *def)
{
    // Are any of the light directions disabled or use a custom lightmap?
    if(def && (def->sides.tex != 0 || def->up.tex != 0 || def->down.tex != 0))
        return true;
    else
        return false;
}

/**
 * Registers the given thing as a luminous, light-emitting object.
 * Note that this is called each frame for each luminous object!
 */
void DL_AddLuminous(mobj_t *thing)
{
    int         lump;
    unsigned int i;
    float       mul;
    float       xOff;
    float       center;
    int         flags = 0;
    int         radius, flareSize;
    float       rgb[3];
    lumobj_t   *lum;
    lightconfig_t cf;
    ded_light_t *def = 0;
    modeldef_t *mf, *nextmf;
    spritedef_t *sprdef;
    spriteframe_t *sprframe;

    // Has BIAS lighting been disabled?
    // If this thing has aquired a BIAS source we need to delete it.
    if(thing->usingBias)
    {
        if(!useBias)
        {
            SB_Delete(thing->light - 1);

            thing->light = 0;
            thing->usingBias = false;
        }
    }
    else
        thing->light = 0;

    if(((thing->state && (thing->state->flags & STF_FULLBRIGHT)) &&
         !(thing->ddflags & DDMF_DONTDRAW)) ||
       (thing->ddflags & DDMF_ALWAYSLIT))
    {
        // Are the automatically calculated light values for fullbright
        // sprite frames in use?
        if(thing->state &&
           (!useMobjAutoLights || (thing->state->flags & STF_NOAUTOLIGHT)) &&
           !thing->state->light)
           return;

        // Determine the sprite frame lump of the source.
        sprdef = &sprites[thing->sprite];
        sprframe = &sprdef->spriteframes[thing->frame];
        if(sprframe->rotate)
        {
            lump =
                sprframe->
                lump[(R_PointToAngle(thing->pos[VX], thing->pos[VY]) - thing->angle +
                      (unsigned) (ANG45 / 2) * 9) >> 29];
        }
        else
        {
            lump = sprframe->lump[0];
        }

        // This'll ensure we have up-to-date information about the texture.
        GL_PrepareSprite(lump, 0);

        // Let's see what our light should look like.
        cf.size = cf.flareSize = spritelumps[lump]->lumsize;
        cf.xOffset = spritelumps[lump]->flarex;
        cf.yOffset = spritelumps[lump]->flarey;

        // X offset to the flare position.
        xOff = cf.xOffset - spritelumps[lump]->width / 2.0f;

        // Does the thing have an active light definition?
        if(thing->state && thing->state->light)
        {
            def = (ded_light_t *) thing->state->light;
            if(def->size)
                cf.size = def->size;
            if(def->offset[VX])
            {
                // Set the x offset here.
                xOff = cf.xOffset = def->offset[VX];
            }
            if(def->offset[VY])
                cf.yOffset = def->offset[VY];
            if(def->halo_radius)
                cf.flareSize = def->halo_radius;
            flags |= def->flags;
        }

        center =
            spritelumps[lump]->topoffset -
            thing->floorclip - R_GetBobOffset(thing) -
            cf.yOffset;

        // Will the sprite be allowed to go inside the floor?
        mul =
            FIX2FLT(thing->pos[VZ]) + spritelumps[lump]->topoffset -
            spritelumps[lump]->height - thing->subsector->sector->SP_floorheight;
        if(!(thing->ddflags & DDMF_NOFITBOTTOM) && mul < 0)
        {
            // Must adjust.
            center -= mul;
        }

        // Sets the dynlight and flare radii.
        //DL_ThingRadius(lum, &cf);

        radius = cf.size * 40 * dlRadFactor;

        // Don't make a too small light.
        if(radius < 32)
            radius = 32;

        flareSize = cf.flareSize * 60 * (50 + haloSize) / 100.0f;

        if(flareSize < 8)
            flareSize = 8;

        // Does the mobj use a light scale?
        if(thing->ddflags & DDMF_LIGHTSCALE)
        {
            // Also reduce the size of the light according to
            // the scale flags. *Won't affect the flare.*
            mul =
                1.0f -
                ((thing->ddflags & DDMF_LIGHTSCALE) >> DDMF_LIGHTSCALESHIFT) /
                4.0f;
            radius *= mul;
        }

        if(def && (def->color[0] || def->color[1] || def->color[2]))
        {
            // If any of the color components are != 0, use the
            // definition's color.
            for(i = 0; i < 3; ++i)
                rgb[i] = def->color[i];
        }
        else
        {
            // Use the sprite's (amplified) color.
            GL_GetSpriteColorf(lump, rgb);
        }

        if(useBias && thing->usingBias)
        {   // We have previously acquired a BIAS source for this thing.
            if(radius < dlMinRadForBias || DL_MustUseDynamic(def))
            {   // We can nolonger use a BIAS source for this light.
                // Delete the bias source (it will be replaced with a
                // dynlight shortly).
                SB_Delete(thing->light - 1);

                thing->light = 0;
                thing->usingBias = false;
            }
            else
            {   // Update BIAS source properties.
                SB_UpdateSource(thing->light - 1,
                                FIX2FLT(thing->pos[VX]),
                                FIX2FLT(thing->pos[VY]),
                                FIX2FLT(thing->pos[VZ]) + center,
                                radius * 0.3f, 0, 1, rgb);
                return;
            }
        }

        // Should we attempt to acquire a BIAS light source for this?
        if(useBias && radius >= dlMinRadForBias &&
           !DL_MustUseDynamic(def))
        {
            if((thing->light =
                    SB_NewSourceAt(FIX2FLT(thing->pos[VX]),
                                   FIX2FLT(thing->pos[VY]),
                                   FIX2FLT(thing->pos[VZ]) + center,
                                   radius * 0.3f, 0, 1, rgb)) != 0)
            {
                // We've acquired a BIAS source for this light.
                thing->usingBias = true;
            }
        }

        if(!thing->usingBias) // Nope, a dynlight then.
        {
            // This'll allow a halo to be rendered. If the light is hidden from
            // view by world geometry, the light pointer will be set to NULL.
            thing->light = DL_NewLuminous();

            lum = DL_GetLuminous(thing->light);
            lum->pos[VX] = FIX2FLT(thing->pos[VX]);
            lum->pos[VY] = FIX2FLT(thing->pos[VY]);
            lum->pos[VZ] = FIX2FLT(thing->pos[VZ]);
            lum->subsector = thing->subsector;
            lum->halofactor = thing->halofactor;
            lum->patch = lump;
            lum->zOff = center;
            lum->xOff = xOff;
            lum->flags = flags;
            lum->flags |= LUMF_CLIPPED;

            // Don't make too large a light.
            if(radius > dlMaxRad)
                radius = dlMaxRad;

            lum->radius = radius;
            lum->flareMul = 1;
            lum->flareSize = flareSize;
            for(i = 0; i < 3; ++i)
                lum->rgb[i] = rgb[i];

            // Approximate the distance in 3D.
            lum->distance =
                P_ApproxDistance3(thing->pos[VX] - viewx, thing->pos[VY] - viewy,
                                  thing->pos[VZ] - viewz);

            // Is there a model definition?
            R_CheckModelFor(thing, &mf, &nextmf);
            if(mf && useModels)
                lum->xyScale = MAX_OF(mf->scale[VX], mf->scale[VZ]);
            else
                lum->xyScale = 1;

            // This light source is not associated with a decormap.
            lum->decorMap = 0;

            if(def)
            {
                lum->tex = def->sides.tex;
                lum->ceilTex = def->up.tex;
                lum->floorTex = def->down.tex;

                if(def->flare.disabled)
                    lum->flags |= LUMF_NOHALO;
                else
                {
                    lum->flareCustom = def->flare.custom;
                    lum->flareTex = def->flare.tex;
                }
            }
            else
            {
                // Use the same default light texture for all directions.
                lum->tex = lum->ceilTex = lum->floorTex =
                    GL_PrepareLSTexture(LST_DYNAMIC, NULL);
            }
        }
    }
    else if(thing->usingBias)
    {   // light is nolonger needed & there is a previously aquired BIAS source.
        // Delete the existing Bias source.
        SB_Delete(thing->light - 1);

        thing->light = 0;
        thing->usingBias = false;
    }
}

static void DL_ContactSector(lumobj_t *lum, fixed_t *box, sector_t *sector)
{
    P_SubsectorBoxIterator(box, sector, DL_AddContact, lum);
}

static boolean DLIT_ContactFinder(line_t *line, void *data)
{
    contactfinder_data_t *light = data;
    sector_t   *source, *dest;
    float       distance;
    vertex_t   *vtx;

    if(!line->L_backside || !line->L_frontside ||
       line->L_frontsector == line->L_backsector)
    {
        // Line must be between two different sectors.
        return true;
    }

    // Which way does the spread go?
    if(line->L_frontsector->validcount == validcount)
    {
        source = line->L_frontsector;
        dest = line->L_backsector;
    }
    else if(line->L_backsector->validcount == validcount)
    {
        source = line->L_backsector;
        dest = line->L_frontsector;
    }
    else
    {
        // Not eligible for spreading.
        return true;
    }

    if(dest->validcount >= light->firstValid &&
       dest->validcount <= validcount + 1)
    {
        // This was already spreaded to.
        return true;
    }

    // Is this line inside the light's bounds?
    if(line->bbox[BOXRIGHT] <= light->box[BOXLEFT] ||
       line->bbox[BOXLEFT]  >= light->box[BOXRIGHT] ||
       line->bbox[BOXTOP]   <= light->box[BOXBOTTOM] ||
       line->bbox[BOXBOTTOM]>= light->box[BOXTOP])
    {
        // The line is not inside the light's bounds.
        return true;
    }

    // Can the spread happen?
    if(dest->planes[PLN_CEILING]->height <= dest->planes[PLN_FLOOR]->height ||
       dest->planes[PLN_CEILING]->height <= source->planes[PLN_FLOOR]->height ||
       dest->planes[PLN_FLOOR]->height   >= source->planes[PLN_CEILING]->height)
    {
        // No; destination sector is closed with no height.
        return true;
    }

    if(line->length <= 0)
    {
        // This can't be a good line.
        return true;
    }

    // Calculate distance to line.
    vtx = line->L_v1;
    distance =
        ((vtx->pos[VY] - light->lum->pos[VY]) * line->dx -
         (vtx->pos[VX] - light->lum->pos[VX]) * line->dy)
         / line->length;

    if((source == line->L_frontsector && distance < 0) ||
       (source == line->L_backsector && distance > 0))
    {
        // Can't spread in this direction.
        return true;
    }

    // Check distance against the light radius.
    if(distance < 0)
        distance = -distance;
    if(distance >= light->lum->radius)
    {
        // The light doesn't reach that far.
        return true;
    }

    // Light spreads to the destination sector.
    light->didSpread = true;

    // During next step, light will continue spreading from there.
    dest->validcount = validcount + 1;

    // Add this lumobj to the destination's subsectors.
    DL_ContactSector(light->lum, light->box, dest);

    return true;
}

/**
 * Create a contact for this lumobj in all the subsectors this light
 * source is contacting (tests done on bounding boxes and the sector
 * spread test).
 */
static void DL_FindContacts(lumobj_t *lum)
{
    int         firstValid = ++validcount;
    int         xl, yl, xh, yh, bx, by;
    contactfinder_data_t light;

    // Use a slightly smaller radius than what the light really is.
    fixed_t radius = lum->radius * FRACUNIT - 2 * FRACUNIT;
    static uint numSpreads = 0, numFinds = 0;

    // Do the sector spread. Begin from the light's own sector.
    lum->subsector->sector->validcount = validcount;

    light.lum = lum;
    light.firstValid = firstValid;
    light.box[BOXTOP]    = FLT2FIX(lum->pos[VY]) + radius;
    light.box[BOXBOTTOM] = FLT2FIX(lum->pos[VY]) - radius;
    light.box[BOXRIGHT]  = FLT2FIX(lum->pos[VX]) + radius;
    light.box[BOXLEFT]   = FLT2FIX(lum->pos[VX]) - radius;

    DL_ContactSector(lum, light.box, lum->subsector->sector);

    xl = (light.box[BOXLEFT]   - bmaporgx) >> MAPBLOCKSHIFT;
    xh = (light.box[BOXRIGHT]  - bmaporgx) >> MAPBLOCKSHIFT;
    yl = (light.box[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
    yh = (light.box[BOXTOP]    - bmaporgy) >> MAPBLOCKSHIFT;

    numFinds++;

    // We'll keep doing this until the light has spreaded everywhere
    // inside the bounding box.
    do
    {
        light.didSpread = false;

        numSpreads++;

        for(bx = xl; bx <= xh; ++bx)
            for(by = yl; by <= yh; ++by)
                P_BlockLinesIterator(bx, by, DLIT_ContactFinder, &light);

        // Increment validcount for the next round of spreading.
        validcount++;
    }
    while(light.didSpread);

/*
#ifdef _DEBUG
if(!((numFinds + 1) % 1000))
{
    Con_Printf("finds=%i avg=%.3f\n", numFinds,
               numSpreads / (float)numFinds);
}
#endif
*/
}

static void DL_SpreadBlocks(subsector_t *subsector)
{
    int         xl, xh, yl, yh, x, y;
    int        *count;
    lumobj_t   *iter;

    xl = X_TO_DLBX(FLT2FIX(subsector->bbox[0].pos[VX] - dlMaxRad));
    xh = X_TO_DLBX(FLT2FIX(subsector->bbox[1].pos[VX] + dlMaxRad));
    yl = Y_TO_DLBY(FLT2FIX(subsector->bbox[0].pos[VY] - dlMaxRad));
    yh = Y_TO_DLBY(FLT2FIX(subsector->bbox[1].pos[VY] + dlMaxRad));

    // Are we completely outside the blockmap?
    if(xh < 0 || xl >= dlBlockWidth || yh < 0 || yl >= dlBlockHeight)
        return;

    // Clip to blockmap bounds.
    if(xl < 0)
        xl = 0;
    if(xh >= dlBlockWidth)
        xh = dlBlockWidth - 1;
    if(yl < 0)
        yl = 0;
    if(yh >= dlBlockHeight)
        yh = dlBlockHeight - 1;

    for(x = xl; x <= xh; ++x)
        for(y = yl; y <= yh; ++y)
        {
            count = &spreadBlocks[x + y * dlBlockWidth];
            if(*count != framecount)
            {
                *count = framecount;

                // Spread the lumobjs in this block.
                for(iter = *DLB_ROOT_DLBXY(x, y); iter; iter = iter->next)
                    DL_FindContacts(iter);
            }
        }
}

/**
 * Used to sort lumobjs by distance from viewpoint.
 */
static int C_DECL lumobjSorter(const void *e1, const void *e2)
{
    lumobj_t *lum1 = DL_GetLuminous(*(const ushort *) e1);
    lumobj_t *lum2 = DL_GetLuminous(*(const ushort *) e2);

    if(lum1->distance > lum2->distance)
        return 1;
    else if(lum1->distance < lum2->distance)
        return -1;
    else
        return 0;
}

/**
 * Clears the dlBlockLinks and then links all the listed luminous objects.
 */
static void DL_LinkLuminous(void)
{
#define MAX_LUMS 8192           // Normally 100-200, heavy: 1000
    int         bx, by;
    unsigned int i,  num = numLuminous;
    lumobj_t  **root, *lum;
    ushort order[MAX_LUMS];

    // Should the proper order be determined?
    if(maxDynLights)
    {
        if(num > maxDynLights)
            num = maxDynLights;

        // Init the indices.
        for(i = 0; i < numLuminous; ++i)
            order[i] = i + 1;

        qsort(order, numLuminous, sizeof(ushort), lumobjSorter);
    }

    for(i = 0; i < num; ++i)
    {
        lum = (maxDynLights ? DL_GetLuminous(order[i]) : luminousList + i);

        // Link this lumobj to the dlBlockLinks, if it can be linked.
        lum->next = NULL;
        bx = X_TO_DLBX(FLT2FIX(lum->pos[VX]));
        by = Y_TO_DLBY(FLT2FIX(lum->pos[VY]));

        if(bx >= 0 && by >= 0 && bx < dlBlockWidth && by < dlBlockHeight)
        {
            root = DLB_ROOT_DLBXY(bx, by);
            lum->next = *root;
            *root = lum;
        }

        // Link this lumobj into its subsector (always possible).
        root = dlSubLinks + GET_SUBSECTOR_IDX(lum->subsector);
        lum->ssNext = *root;
        *root = lum;
    }
#undef MAX_LUMS
}

/**
 * Returns true if the texture is already used in the list of dynlights.
 */
#if 0 // currently unused
static boolean DL_IsTexUsed(dynlight_t *node, DGLuint texture)
{
    boolean     found;

    found = false;
    while(node && !found)
    {
        if(node->texture == texture)
            found = true;
        else
            node = node->next;
    }

    return found;
}
#endif

/**
 * Process the given lumobj to maybe add a dynamic light for the plane.
 */
static void DL_ProcessPlane(lumobj_t *lum, subsector_t *subsector,
                            uint planeID, planeitervars_t *planeVars)
{
    dynlight_t *dyn;
    DGLuint     lightTex;
    float       diff, lightStrength, srcRadius;
    float       s[2], t[2];
    float       pos[3];

    pos[VX] = lum->pos[VX];
    pos[VY] = lum->pos[VY];
    pos[VZ] = lum->pos[VZ];

    // Center the Z.
    pos[VZ] += lum->zOff;
    srcRadius = lum->radius / 4;
    if(srcRadius == 0)
        srcRadius = 1;

    // Determine on which side the light is for all planes.
    lightTex = 0;
    lightStrength = 0;

    if(subsector->planes[planeID]->type == PLN_FLOOR)
    {
        if((lightTex = lum->floorTex) != 0)
        {
            if(pos[VZ] > planeVars->height)
                lightStrength = 1;
            else if(pos[VZ] > planeVars->height - srcRadius)
                lightStrength = 1 - (planeVars->height - pos[VZ]) / srcRadius;
        }
    }
    else
    {
        if((lightTex = lum->ceilTex) != 0)
        {
            if(pos[VZ] < planeVars->height)
                lightStrength = 1;
            else if(pos[VZ] < planeVars->height + srcRadius)
                lightStrength = 1 - (pos[VZ] - planeVars->height) / srcRadius;
        }
    }

    // Is there light in this direction? Is it strong enough?
    if(!lightTex || !lightStrength)
        return;

    // Check that the height difference is tolerable.
    if(subsector->planes[planeID]->type == PLN_CEILING)
        diff = planeVars->height - pos[VZ];
    else
        diff = pos[VZ] - planeVars->height;

    // Clamp it.
    if(diff < 0)
        diff = 0;

    if(diff < lum->radius)
    {
        // Calculate dynlight position. It may still be outside
        // the bounding box the subsector.
        s[0] = -pos[VX] + lum->radius;
        t[0] = pos[VY] + lum->radius;
        s[1] = t[1] = 1.0f / (2 * lum->radius);

        // A dynamic light will be generated.
        dyn = DL_New(s, t);
        dyn->flags = 0;
        dyn->texture = lightTex;

        DL_ComputeLightColor(lum, dyn->color, LUM_FACTOR(diff) * lightStrength);

        // Link to this plane's list.
        DL_LinkToSubSecPlane(dyn, GET_SUBSECTOR_IDX(subsector), planeID);
    }
}

static boolean DL_LightSegIteratorFunc(lumobj_t *lum, subsector_t *ssec)
{
    uint        j;
    seg_t      *seg;

    // The wall segments.
    for(j = 0, seg = ssec->firstseg; j < ssec->segcount; ++j, seg++)
    {
        if(seg->linedef)    // "minisegs" have no linedefs.
        {
            if(seg->flags & SEGF_POLYOBJ)
                continue;

            DL_ProcessWallSeg(lum, seg, ssec);
        }
    }

    // Is there a polyobj on board? Light it, too.
    if(ssec->poly)
        for(j = 0; j < ssec->poly->numsegs; ++j)
        {
            DL_ProcessWallSeg(lum, ssec->poly->segs[j], ssec);
        }

    return true;
}

/**
 * Return the texture name of the decoration light map for the flat.
 * (Zero if no such texture exists.)
 */
#if 0 // Unused
DGLuint DL_GetFlatDecorLightMap(surface_t *surface)
{
    ded_decor_t *decor;

    if(R_IsSkySurface(surface) || !surface->isflat)
        return 0;

    decor = R_GetFlat(surface->texture)->decoration;
    return decor ? decor->pregen_lightmap : 0;
}
#endif

/**
 * Process dynamic lights for the specified subsector.
 */
void DL_ProcessSubsector(subsector_t *ssec)
{
    uint        pln, j;
    uint        num, ssecidx = GET_SUBSECTOR_IDX(ssec);
    seg_t      *seg;
    lumcontact_t *con;
    sector_t   *sect = ssec->sector, *linkSec;
    planeitervars_t *pVars;

    // Do we need to enlarge the planeVars buffer?
    if(sect->planecount > maxPlanes)
    {
        maxPlanes *= 2;

        if(!maxPlanes)
            maxPlanes = 2;

        planeVars =
            M_Realloc(planeVars, maxPlanes * sizeof(planeitervars_t));
    }

    // Has the number of planes changed for this subsector?
    if(sect->planecount != subSecLightLinks[ssecidx].planeCount)
    {
        subseclight_t *sseclight = &subSecLightLinks[ssecidx];

        sseclight->planeCount = sect->planecount;
        sseclight->planes =
                Z_Realloc(sseclight->planes,
                          sseclight->planeCount * sizeof(dynlight_t*),
                          PU_LEVEL);

        memset(sseclight->planes, 0,
               sseclight->planeCount * sizeof(dynlight_t*));
    }

    // First make sure we know which lumobjs are contacting us.
    DL_SpreadBlocks(ssec);

    // Check if lighting can be skipped for each plane.
    num = sect->planecount;
    for(pln = 0, pVars = planeVars; pln < num; pVars++, ++pln)
    {
        linkSec = R_GetLinkedSector(ssec, pln);
        pVars->height = linkSec->SP_planevisheight(pln);
        pVars->isLit = (!R_IsSkySurface(&sect->SP_planesurface(pln)));
        pVars->decorMap = 0;

        // View height might prevent us from seeing the light.
        if(ssec->SP_plane(pln)->type == PLN_FLOOR)
        {
            if(vy < pVars->height)
                pVars->isLit = false;
        }
        else
        {
            if(vy > pVars->height)
                pVars->isLit = false;
        }
    }

    // Process each lumobj contacting the subsector.
    for(con = subContacts[ssecidx]; con; con = con->next)
    {
        lumobj_t *lum = con->lum;

        if(haloMode)
        {
            if(lum->subsector == ssec)
                lum->flags |= LUMF_RENDERED;
        }

        // Process the planes
        for(pln = 0, pVars = planeVars; pln < num; pVars++, ++pln)
        {
            if(pVars->isLit)
                DL_ProcessPlane(lum, ssec, pln, pVars);
        }

        // If the light has no texture for the 'sides', there's no point in
        // going through the wall segments.
        if(lum->tex)
            DL_LightSegIteratorFunc(con->lum, ssec);
    }

    // If the segs of this subsector are affected by glowing planes we need
    // to create dynlights and link them.
    if(useWallGlow &&
       (sect->SP_floorglow || sect->SP_ceilglow))
    {
        // The wall segments.
        num = ssec->segcount;
        for(j = 0, seg = ssec->firstseg; j < num; ++j, seg++)
        {
            if(seg->linedef)    // "minisegs" have no linedefs.
            {
                if(seg->flags & SEGF_POLYOBJ)
                    continue;

                DL_ProcessSegForGlow(seg, sect);
            }
        }

        // Is there a polyobj on board? Light it, too.
        if(ssec->poly)
        {
            num = ssec->poly->numsegs;
            for(j = 0; j < num; ++j)
            {
                seg = ssec->poly->segs[j];
                DL_ProcessSegForGlow(seg, sect);
            }
        }
    }
}

/**
 * Creates the dynlight links by removing everything and then linking
 * this frame's luminous objects.
 */
void DL_InitForNewFrame(void)
{
    uint        i;
    sector_t   *seciter;
    mobj_t     *iter;

BEGIN_PROF( PROF_DYN_INIT_DEL );

    // Clear the dynlight lists, which are used to track the lights on
    // each surface of the map.
    DL_DeleteUsed();

END_PROF( PROF_DYN_INIT_DEL );

    // The luminousList already contains lumobjs if there are any light
    // decorations in use.
    dlInited = true;

BEGIN_PROF( PROF_DYN_INIT_ADD );

    for(i = 0, seciter = sectors; i < numsectors; seciter++, ++i)
    {
        for(iter = seciter->thinglist; iter; iter = iter->snext)
        {
            DL_AddLuminous(iter);
        }
    }

END_PROF( PROF_DYN_INIT_ADD );
BEGIN_PROF( PROF_DYN_INIT_LINK );

    // Link the luminous objects into the blockmap.
    DL_LinkLuminous();

END_PROF( PROF_DYN_INIT_LINK );
}

/**
 * Calls func for all luminous objects within the specified range from (x,y).
 * 'subsector' is the subsector in which (x,y) resides.
 */
boolean DL_RadiusIterator(subsector_t *subsector, fixed_t x, fixed_t y,
                          fixed_t radius, void *data,
                          boolean (*func) (lumobj_t *, fixed_t, void *data))
{
    fixed_t     dist;
    lumcontact_t *con;
    boolean     retVal, isDone;

    if(!subsector)
        return true;

    con = subContacts[GET_SUBSECTOR_IDX(subsector)];
    retVal = true;
    isDone = false;
    while(con && !isDone)
    {
        dist = P_ApproxDistance(FLT2FIX(con->lum->pos[VX]) - x,
                                FLT2FIX(con->lum->pos[VY]) - y);

        if(dist <= radius && !func(con->lum, dist, data))
        {
            retVal = false;
            isDone = true;
        }
        else
            con = con->next;
    }

    return retVal;
}

/**
 * Clip lights by subsector.
 *
 * @param ssecidx       Subsector index in which lights will be clipped.
 */
void DL_ClipInSubsector(uint ssecidx)
{
    lumobj_t   *lumi; // Lum Iterator, or 'snow' in Finnish. :-)

    // Determine which dynamic light sources in the subsector get clipped.
    for(lumi = dlSubLinks[ssecidx]; lumi; lumi = lumi->ssNext)
    {
        lumi->flags &= ~LUMF_CLIPPED;

        // FIXME: Determine the exact centerpoint of the light in
        // DL_AddLuminous!
        if(!C_IsPointVisible(lumi->pos[VX], lumi->pos[VY],
                             lumi->pos[VZ] + lumi->zOff))
            lumi->flags |= LUMF_CLIPPED;    // Won't have a halo.
    }
}

/**
 * In the situation where a subsector contains both dynamic lights and
 * a polyobj, the lights must be clipped more carefully.  Here we
 * check if the line of sight intersects any of the polyobj segs that
 * face the camera.
 */
void DL_ClipBySight(uint ssecidx)
{
    int         i, num;
    vec2_t      eye, source;
    subsector_t *ssec = SUBSECTOR_PTR(ssecidx);
    lumobj_t   *lumi;
    seg_t      *seg;

    // Only checks the polyobj.
    if(ssec->poly == NULL) return;

    V2_Set(eye, vx, vz);

    num = ssec->poly->numsegs;
    for(lumi = dlSubLinks[ssecidx]; lumi; lumi = lumi->ssNext)
    {
        if(!(lumi->flags & LUMF_CLIPPED))
        {
            // We need to figure out if any of the polyobj's segments lies
            // between the viewpoint and the light source.
            for(i = 0; i < num; ++i)
            {
                seg = ssec->poly->segs[i];

                // Ignore segs facing the wrong way.
                if(seg->frameflags & SEGINF_FACINGFRONT)
                {
                    V2_Set(source, lumi->pos[VX], lumi->pos[VY]);

                    if(V2_Intercept2(source, eye, seg->SG_v1->pos,
                                     seg->SG_v2->pos, NULL, NULL, NULL))
                    {
                        lumi->flags |= LUMF_CLIPPED;
                    }
                }
            }
        }
    }
}
