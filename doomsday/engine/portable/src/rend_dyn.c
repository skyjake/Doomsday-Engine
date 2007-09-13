/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#define LUM_FACTOR(dist, radius)        (1.5f - 1.5f*(dist)/(radius))

// TYPES -------------------------------------------------------------------

typedef struct {
    float           color[3];
    float           size;
    float           flareSize;
    float           xOffset, yOffset;
} lightconfig_t;

typedef struct lumcontact_s {
    struct lumcontact_s *next;  // Next in the subsector.
    struct lumcontact_s *nextUsed;  // Next used contact.
    lumobj_t       *lum;
} lumcontact_t;

typedef struct lumnode_s {
    struct lumnode_s *next;         // Next in the same DL block, or NULL.
    struct lumnode_s *ssNext;       // Next in the same subsector, or NULL.

    lumobj_t        lum;
} lumnode_t;

typedef struct dynnode_s {
	struct dynnode_s *next, *nextUsed;
    dynlight_t      dyn;
} dynnode_t;

typedef struct contactfinder_data_s {
    fixed_t         box[4];
    boolean         didSpread;
    lumobj_t       *lum;
    int             firstValid;
} contactfinder_data_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

boolean DLIT_LinkLumToSubSector(subsector_t *subsector, void *data);

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
uint    maxDynLights = 0;
int     useMobjAutoLights = true; // Enable automaticaly calculated lights
                                  // attached to mobjs.
byte    rendInfoLums = false;

int     dlMinRadForBias = 136; // Lights smaller than this will NEVER
                               // be converted to BIAS sources.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static lumnode_t *luminousList = NULL;
static uint numLuminous = 0, maxLuminous = 0;

// Dynlight nodes.
static dynnode_t *dynFirst, *dynCursor;

static lumnode_t **dlBlockLinks = 0;
static fixed_t dlBlockOrig[3];
static int dlBlockWidth, dlBlockHeight;    // In 128 blocks.

static lumnode_t **dlSubLinks = 0;

// Surface light link lists.
static uint numLightLinkLists = 0, lightLinkListCursor = 0;
static dynnode_t **lightLinkLists;

// List of unused and used lumobj-subsector contacts.
static lumcontact_t *contFirst, *contCursor;

// List of lumobj contacts for each subsector.
static lumcontact_t **subContacts;

// A frameCount for each block. Used to prevent multiple processing of
// a block during one frame.
static int *spreadBlocks;

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
static void deleteUsed(void)
{
    // Start reusing nodes from the first one in the list.
    dynCursor = dynFirst;
    contCursor = contFirst;

    // Clear the surface light link lists.
    lightLinkListCursor = 0;
    if(numLightLinkLists)
        memset(lightLinkLists, 0, numLightLinkLists * sizeof(dynnode_t*));

    // Clear lumobj contacts.
    memset(subContacts, 0, numsubsectors * sizeof(lumcontact_t *));
}

static dynnode_t *newDynNode(void)
{
	dynnode_t	*node;

    // Have we run out of nodes?
    if(dynCursor == NULL)
    {
        node = Z_Malloc(sizeof(dynnode_t), PU_STATIC, NULL);

        // Link the new node to the list.
        node->nextUsed = dynFirst;
        dynFirst = node;
    }
    else
    {
        node = dynCursor;
        dynCursor = dynCursor->nextUsed;
    }

    node->next = NULL;
	return node;
}

/**
 * Returns a new dynlight node. If the list of unused nodes is empty,
 * a new node is created.
 */
static dynnode_t *newDynLight(float *s, float *t)
{
	dynnode_t	*node = newDynNode();
    dynlight_t	*dyn = &node->dyn;

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

    return node;
}

/**
 * @return          The number of active lumobjs for this frame.
 */
uint DL_GetNumLuminous(void)
{
    return numLuminous;
}

/**
 * Link the given dynlight node to list.
 */
static void __inline linkDynNode(dynnode_t *node, dynnode_t **list, uint index)
{
    node->next = list[index];
    list[index] = node;
}

static void linkToSurfaceLightList(dynnode_t *node, uint index)
{
    linkDynNode(node, &lightLinkLists[index], 0);
}

/**
 * Link the given lumcontact node to list.
 */
static void __inline linkContact(lumcontact_t *con, lumcontact_t **list, uint index)
{
    con->next = list[index];
    list[index] = con;
}

static void linkLumToSubSector(lumcontact_t *node, uint index)
{
    linkContact(node, &subContacts[index], 0);
}

/**
 * Create a new lumcontact for the given lumobj. If there are nodes in the
 * list of unused nodes, the new contact is taken from there.
 *
 * @param lum       Ptr to the lumobj a lumcontact is required for.
 *
 * @return          Ptr to the new lumcontact.
 */
static lumcontact_t *newLumContact(lumobj_t *lum)
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
 * Blend the given light value with the lumobj's color, apply any global
 * modifiers and output the result.
 *
 * @param outRGB    The location the calculated result will be written to.
 * @param lum       Ptr to the lumobj from which the color will be used.
 * @param light     The light value to be used in the calculation.
 */
static void calcDynLightColor(float *outRGB, lumobj_t *lum, float light)
{
    uint        i;

    if(light < 0)
        light = 0;
    else if(light > 1)
        light = 1;
    light *= dlFactor;

    // If fog is enabled, make the light dimmer.
    // \fixme This should be a cvar.
    if(usingFog)
        light *= .5f;           // Would be too much.

    // Multiply with the light color.
    if(lum->type == LT_OMNI && LUM_OMNI(lum)->decorMap)
    {   // Decoration maps are pre-colored.
       for(i = 0; i < 3; ++i)
           outRGB[i] = light;
    }
    else
    {
        for(i = 0; i < 3; ++i)
        {
            outRGB[i] = light * lum->color[i];
        }
    }
}

/**
 * Initialize the dynlight system in preparation for rendering view(s) of the
 * game world. Called by R_InitLevel().
 */
void DL_InitForMap(void)
{
    fixed_t     min[3], max[3];

    // First initialize the subsector links (root pointers).
    dlSubLinks =
        Z_Calloc(sizeof(lumnode_t*) * numsubsectors, PU_LEVELSTATIC, 0);

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
                sizeof(lumnode_t*) * dlBlockWidth * dlBlockHeight);

    // Initialize lumobj -> subsector contacts.
    subContacts =
        Z_Calloc(numsubsectors * sizeof(lumcontact_t*), PU_LEVELSTATIC, 0);

    // A frameCount was each block.
    spreadBlocks =
        Z_Malloc(sizeof(int) * dlBlockWidth * dlBlockHeight,
                 PU_LEVELSTATIC, 0);

    lightLinkLists = NULL;
    numLightLinkLists = 0, lightLinkListCursor = 0;
}

/**
 * Project the given planelight onto the specified seg. If it would be lit,
 * a new dynlight node will be created and returned.
 *
 * @param lum       Ptr to the lumobj lighting the seg.
 * @param bottom    Z height (bottom) of the section being lit.
 * @param top       Z height (top) of the section being lit.
 *
 * @return          Ptr to the projected light, ELSE @c NULL.
 */
static dynnode_t *projectPlaneGlowOnSegSection(lumobj_t *lum, float bottom,
                                               float top)
{
    uint        i;
    float       glowHeight;
    dynlight_t *dyn;
    dynnode_t  *node;
    float       s[2], t[2];

    if(bottom >= top)
        return NULL; // No height.

    if(!LUM_PLANE(lum)->tex)
        return NULL;

    glowHeight =
        (MAX_GLOWHEIGHT * LUM_PLANE(lum)->intensity) * glowHeightFactor;

    // Don't make too small or too large glows.
    if(glowHeight <= 2)
        return NULL;

    if(glowHeight > glowHeightMax)
        glowHeight = glowHeightMax;

    // Calculate texture coords for the light.
    if(LUM_PLANE(lum)->normal[VZ] < 0)
    {   // Light is cast downwards.
        t[1] = t[0] = (lum->pos[VZ] - top) / glowHeight;
        t[1]+= (top - bottom) / glowHeight;

        if(t[0] > 1 || t[1] < 0)
            return NULL;
    }
    else
    {   // Light is cast upwards.
        t[0] = t[1] = (bottom - lum->pos[VZ]) / glowHeight;
        t[0]+= (top - bottom) / glowHeight;

        if(t[1] > 1 || t[0] < 0)
            return NULL;
    }

    // The horizontal direction is easy.
    s[0] = 0;
    s[1] = 1;

    node = newDynLight(s, t);
    dyn = &node->dyn;
    dyn->texture = LUM_PLANE(lum)->tex;

    for(i = 0; i < 3; ++i)
    {
        dyn->color[i] = lum->color[i] * dlFactor;

        // In fog, additive blending is used. The normal fog color
        // is way too bright.
        if(usingFog)
            dyn->color[i] *= glowFogBright;
    }

    return node;
}

/**
 * Generate one dynlight node for each plane glow.
 * The light is attached to the appropriate dynlight node list.
 *
 * @param ssec          Ptr to the subsector to process.
 */
static void createGlowLightPerPlaneForSubSector(subsector_t *ssec)
{
    uint        g;
    plane_t    *glowPlanes[2], *pln;

    glowPlanes[PLN_FLOOR] = R_GetLinkedSector(ssec, PLN_FLOOR)->planes[PLN_FLOOR];
    glowPlanes[PLN_CEILING] = R_GetLinkedSector(ssec, PLN_CEILING)->planes[PLN_CEILING];

    // \fixme $nplanes
    for(g = 0; g < 2; ++g)
    {
        uint        light;
        lumobj_t   *l;

        pln = glowPlanes[g];

        if(pln->glow <= 0)
            continue;

        light = DL_NewLuminous(LT_PLANE);

        l = DL_GetLuminous(light);
        l->flags = LUMF_NOHALO | LUMF_CLIPPED;
        l->pos[VX] = ssec->midpoint.pos[VX];
        l->pos[VY] = ssec->midpoint.pos[VY];
        l->pos[VZ] = pln->visheight;

        LUM_PLANE(l)->normal[VX] = pln->PS_normal[VX];
        LUM_PLANE(l)->normal[VY] = pln->PS_normal[VY];
        LUM_PLANE(l)->normal[VZ] = pln->PS_normal[VZ];

        // Approximate the distance in 3D.
        l->distanceToViewer =
            FIX2FLT(P_ApproxDistance3(FLT2FIX(l->pos[VX] - viewX),
                                      FLT2FIX(l->pos[VY] - viewY),
                                      FLT2FIX(l->pos[VZ] - viewZ)));

        l->subsector = ssec;
        memcpy(l->color, pln->glowrgb, sizeof(l->color));
        LUM_PLANE(l)->intensity = pln->glow;
        LUM_PLANE(l)->tex = GL_PrepareLSTexture(LST_GRADIENT, NULL);

        // Plane lights don't spread, so just link the lum to its own ssec.
        DLIT_LinkLumToSubSector(l->subsector, l);
    }
}

/**
 * Called once during engine shutdown by Rend_Reset(). Releases any system
 * resources acquired by the dynlight subsystem.
 */
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
}

/**
 * Called at the begining of each frame (iff the render lists are not frozen)
 * by Rend_RenderMap().
 */
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
    memset(dlSubLinks, 0, sizeof(lumnode_t *) * numsubsectors);
    memset(dlBlockLinks, 0, sizeof(lumnode_t *) * dlBlockWidth * dlBlockHeight);

    numLuminous = 0;
}

/**
 * Allocate a new lumobj.
 *
 * @return          Index (name) by which the lumobj should be referred.
 */
uint DL_NewLuminous(lumtype_t type)
{
    lumnode_t  *node, *newList;

    numLuminous++;

    // Only allocate memory when it's needed.
    // \fixme No upper limit?
    if(numLuminous > maxLuminous)
    {
        maxLuminous *= 2;

        // The first time, allocate thirty two lumobjs.
        if(!maxLuminous)
            maxLuminous = 32;

        newList = Z_Malloc(sizeof(lumnode_t) * maxLuminous, PU_STATIC, 0);

        // Copy the old data over to the new list.
        if(luminousList)
        {
            memcpy(newList, luminousList,
                   sizeof(lumnode_t) * (numLuminous - 1));
            Z_Free(luminousList);
        }
        luminousList = newList;
    }

    // Clear the new lumobj.
    node = (luminousList + numLuminous - 1);
    memset(node, 0, sizeof(lumnode_t));

    node->lum.type = type;

    return numLuminous; // == index + 1
}

/**
 * NOTE: No bounds checking occurs, it is assumed callers know what they are
 *       doing.
 *
 * @return          Ptr to the lumnode at the given index.
 */
static lumnode_t __inline *getLumNode(uint idx)
{
    return &luminousList[idx];
}

/**
 * Retrieve a ptr to the lumobj with the given index. A public interface to
 * the lumobj list.
 *
 * @return          Ptr to the lumobj with the given 1-based index.
 */
lumobj_t *DL_GetLuminous(uint idx)
{
    if(!(idx == 0 || idx > numLuminous))
        return &getLumNode(idx - 1)->lum;

    return NULL;
}

/**
 * Must we use a dynlight to represent the given light?
 *
 * @param def       Ptr to the ded_light to examine.
 *
 * @return          @c true, if we HAVE to use a dynamic light for this
 *                  light defintion (as opposed to a bias light source).
 */
static boolean mustUseDynamic(ded_light_t *def)
{
    // Are any of the light directions disabled or use a custom lightmap?
    if(def && (def->sides.tex != 0 || def->up.tex != 0 || def->down.tex != 0))
        return true;

    return false;
}

/**
 * Registers the given mobj as a luminous, light-emitting object.
 * NOTE: This is called each frame for each luminous object!
 *
 * @param mo        Ptr to the mobj to register.
 */
void DL_AddLuminous(mobj_t *mo)
{
    int         lump;
    uint        i;
    float       mul;
    float       xOff;
    float       center;
    int         flags = 0;
    int         radius, flareSize;
    float       rgb[3];
    lumobj_t   *l;
    lightconfig_t cf;
    ded_light_t *def = 0;
    spritedef_t *sprdef;
    spriteframe_t *sprframe;

    // Has BIAS lighting been disabled?
    // If this thing has aquired a BIAS source we need to delete it.
    if(mo->usingBias)
    {
        if(!useBias)
        {
            SB_Delete(mo->light - 1);

            mo->light = 0;
            mo->usingBias = false;
        }
    }
    else
        mo->light = 0;

    if(((mo->state && (mo->state->flags & STF_FULLBRIGHT)) &&
         !(mo->ddflags & DDMF_DONTDRAW)) ||
       (mo->ddflags & DDMF_ALWAYSLIT))
    {
        // Are the automatically calculated light values for fullbright
        // sprite frames in use?
        if(mo->state &&
           (!useMobjAutoLights || (mo->state->flags & STF_NOAUTOLIGHT)) &&
           !mo->state->light)
           return;

        // Determine the sprite frame lump of the source.
        sprdef = &sprites[mo->sprite];
        sprframe = &sprdef->spriteframes[mo->frame];
        if(sprframe->rotate)
        {
            lump =
                sprframe->
                lump[(R_PointToAngle(mo->pos[VX], mo->pos[VY]) - mo->angle +
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
        xOff = cf.xOffset - (float) spritelumps[lump]->width / 2.0f;

        // Does the mobj have an active light definition?
        if(mo->state && mo->state->light)
        {
            def = (ded_light_t *) mo->state->light;
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
            mo->floorclip - R_GetBobOffset(mo) -
            cf.yOffset;

        // Will the sprite be allowed to go inside the floor?
        mul =
            FIX2FLT(mo->pos[VZ]) + spritelumps[lump]->topoffset -
            (float) spritelumps[lump]->height - mo->subsector->sector->SP_floorheight;
        if(!(mo->ddflags & DDMF_NOFITBOTTOM) && mul < 0)
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
        if(mo->ddflags & DDMF_LIGHTSCALE)
        {
            // Also reduce the size of the light according to
            // the scale flags. *Won't affect the flare.*
            mul =
                1.0f -
                ((mo->ddflags & DDMF_LIGHTSCALE) >> DDMF_LIGHTSCALESHIFT) /
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

        if(useBias && mo->usingBias)
        {   // We have previously acquired a BIAS source for this mobj.
            if(radius < dlMinRadForBias || mustUseDynamic(def))
            {   // We can nolonger use a BIAS source for this light.
                // Delete the bias source (it will be replaced with a
                // dynlight shortly).
                SB_Delete(mo->light - 1);

                mo->light = 0;
                mo->usingBias = false;
            }
            else
            {   // Update BIAS source properties.
                SB_UpdateSource(mo->light - 1,
                                FIX2FLT(mo->pos[VX]),
                                FIX2FLT(mo->pos[VY]),
                                FIX2FLT(mo->pos[VZ]) + center,
                                radius * 0.3f, 0, 1, rgb);
                return;
            }
        }

        // Should we attempt to acquire a BIAS light source for this?
        if(useBias && radius >= dlMinRadForBias &&
           !mustUseDynamic(def))
        {
            if((mo->light =
                    SB_NewSourceAt(FIX2FLT(mo->pos[VX]),
                                   FIX2FLT(mo->pos[VY]),
                                   FIX2FLT(mo->pos[VZ]) + center,
                                   radius * 0.3f, 0, 1, rgb)) != 0)
            {
                // We've acquired a BIAS source for this light.
                mo->usingBias = true;
            }
        }

        if(!mo->usingBias) // Nope, a dynlight then.
        {
            // This'll allow a halo to be rendered. If the light is hidden from
            // view by world geometry, the light pointer will be set to NULL.
            mo->light = DL_NewLuminous(LT_OMNI);

            l = DL_GetLuminous(mo->light);
            l->flags = flags;
            l->flags |= LUMF_CLIPPED;
            l->pos[VX] = FIX2FLT(mo->pos[VX]);
            l->pos[VY] = FIX2FLT(mo->pos[VY]);
            l->pos[VZ] = FIX2FLT(mo->pos[VZ]);
            // Approximate the distance in 3D.
            l->distanceToViewer =
                FIX2FLT(P_ApproxDistance3(mo->pos[VX] - FLT2FIX(viewX),
                                          mo->pos[VY] - FLT2FIX(viewY),
                                          mo->pos[VZ] - FLT2FIX(viewZ)));

            l->subsector = mo->subsector;
            LUM_OMNI(l)->halofactor = mo->halofactor;
            LUM_OMNI(l)->zOff = center;
            LUM_OMNI(l)->xOff = xOff;

            // Don't make too large a light.
            if(radius > dlMaxRad)
                radius = dlMaxRad;

            LUM_OMNI(l)->radius = radius;
            LUM_OMNI(l)->flareMul = 1;
            LUM_OMNI(l)->flareSize = flareSize;
            for(i = 0; i < 3; ++i)
                l->color[i] = rgb[i];

            // This light source is not associated with a decormap.
            LUM_OMNI(l)->decorMap = 0;

            if(def)
            {
                LUM_OMNI(l)->tex = def->sides.tex;
                LUM_OMNI(l)->ceilTex = def->up.tex;
                LUM_OMNI(l)->floorTex = def->down.tex;

                if(def->flare.disabled)
                    l->flags |= LUMF_NOHALO;
                else
                {
                    LUM_OMNI(l)->flareCustom = def->flare.custom;
                    LUM_OMNI(l)->flareTex = def->flare.tex;
                }
            }
            else
            {
                // Use the same default light texture for all directions.
                LUM_OMNI(l)->tex = LUM_OMNI(l)->ceilTex =
                    LUM_OMNI(l)->floorTex =
                    GL_PrepareLSTexture(LST_DYNAMIC, NULL);
            }
        }
    }
    else if(mo->usingBias)
    {   // light is nolonger needed & there is a previously aquired BIAS source.
        // Delete the existing Bias source.
        SB_Delete(mo->light - 1);

        mo->light = 0;
        mo->usingBias = false;
    }
}

boolean DLIT_LinkLumToSubSector(subsector_t *subsector, void *data)
{
    lumobj_t   *lum = (lumobj_t*) data;
    lumcontact_t *con = newLumContact(lum);

    // Link it to the contact list for this subsector.
    linkLumToSubSector(con, GET_SUBSECTOR_IDX(subsector));

    return true; // Continue iteration.
}

/**
 * Iterate subsectors of sector, within or intersecting the specified
 * bounding box, looking for those which are close enough to be lit by the
 * given lumobj. For each, register a subsector > lumobj "contact".
 *
 * @param lum       Ptr to the lumobj hoping to contact the sector.
 * @param box       Subsectors within this bounding box will be processed.
 * @param sector    Ptr to the sector to check for contacts.
 */
static void contactSector(lumobj_t *lum, fixed_t *box, sector_t *sector)
{
    P_SubsectorBoxIterator(box, sector, DLIT_LinkLumToSubSector, lum);
}

/**
 * Attempt to spread the light from the given contact over a twosided
 * linedef, into the (relative) back sector.
 *
 * @param line      Ptr to linedef to attempt to spread over.
 * @param data      Ptr to contactfinder_data structure.
 *
 * @return          @c true, because this function is also used as an
 *                  iterator.
 */
boolean DLIT_ContactFinder(line_t *line, void *data)
{
    contactfinder_data_t *light = data;
    sector_t   *source, *dest;
    float       distance;
    vertex_t   *vtx;
    lumobj_t   *l;

    if(light->lum->type != LT_OMNI)
        return true; // Only interested in omni lights.
    l = light->lum;

    if(!line->L_backside || !line->L_frontside ||
       line->L_frontsector == line->L_backsector)
    {
        // Line must be between two different sectors.
        return true;
    }

    if(line->length <= 0)
    {
        // This can't be a good line.
        return true;
    }

    // Which way does the spread go?
    if(line->L_frontsector->validCount == validCount)
    {
        source = line->L_frontsector;
        dest = line->L_backsector;
    }
    else if(line->L_backsector->validCount == validCount)
    {
        source = line->L_backsector;
        dest = line->L_frontsector;
    }
    else
    {
        // Not eligible for spreading.
        return true;
    }

    if(dest->validCount >= light->firstValid &&
       dest->validCount <= validCount + 1)
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

    // Calculate distance to line.
    vtx = line->L_v1;
    distance =
        ((vtx->V_pos[VY] - l->pos[VY]) * line->dx -
         (vtx->V_pos[VX] - l->pos[VX]) * line->dy)
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
    if(distance >= LUM_OMNI(l)->radius)
    {   // The light doesn't reach that far.
        return true;
    }

    // Light spreads to the destination sector.
    light->didSpread = true;

    // During next step, light will continue spreading from there.
    dest->validCount = validCount + 1;

    // Add this lumobj to the destination's subsectors.
    contactSector(l, light->box, dest);

    return true;
}

/**
 * Create a contact for this lumobj in all the subsectors this light
 * source is contacting (tests done on bounding boxes and the sector
 * spread test).
 *
 * @param lum       Ptr to lumobj to find subsector contacts for.
 */
static void findContacts(lumobj_t *lum)
{
    int         firstValid = ++validCount;
    int         xl, yl, xh, yh, bx, by;
    contactfinder_data_t light;

    // Use a slightly smaller radius than what the light really is.
    fixed_t radius;
    static uint numSpreads = 0, numFinds = 0;

    if(lum->type != LT_OMNI)
        return; // Only omni lights spread.

    radius = LUM_OMNI(lum)->radius * FRACUNIT - 2 * FRACUNIT;
    // Do the sector spread. Begin from the light's own sector.
    lum->subsector->sector->validCount = validCount;

    light.lum = lum;
    light.firstValid = firstValid;
    light.box[BOXTOP]    = FLT2FIX(lum->pos[VY]) + radius;
    light.box[BOXBOTTOM] = FLT2FIX(lum->pos[VY]) - radius;
    light.box[BOXRIGHT]  = FLT2FIX(lum->pos[VX]) + radius;
    light.box[BOXLEFT]   = FLT2FIX(lum->pos[VX]) - radius;

    contactSector(lum, light.box, lum->subsector->sector);

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

        // Increment validCount for the next round of spreading.
        validCount++;
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

/**
 * Spread lumobj contacts in the subsector > dynnode blockmap to all other
 * subsectors within the block.
 *
 * @param subsector Ptr to the subsector to spread the lumobj contacts of.
 */
static void spreadLumobjsInSubSector(subsector_t *subsector)
{
    int         xl, xh, yl, yh, x, y;
    int        *count;
    lumnode_t  *iter;

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
            if(*count != frameCount)
            {
                *count = frameCount;

                // Spread the lumobjs in this block.
                for(iter = *DLB_ROOT_DLBXY(x, y); iter; iter = iter->next)
                    findContacts(&iter->lum);
            }
        }
}

/**
 * Used to sort lumobjs by distance from viewpoint.
 */
static int C_DECL lumobjSorter(const void *e1, const void *e2)
{
    lumobj_t *lum1 = &getLumNode(*(const ushort *) e1)->lum;
    lumobj_t *lum2 = &getLumNode(*(const ushort *) e2)->lum;

    if(lum1->distanceToViewer > lum2->distanceToViewer)
        return 1;
    else if(lum1->distanceToViewer < lum2->distanceToViewer)
        return -1;
    else
        return 0;
}

/**
 * Clears the dlBlockLinks and then links all the listed luminous objects.
 * Called by DL_InitForNewFrame() at the beginning of each frame (iff the
 * render lists are not frozen).
 */
static void linkDynNodeLuminous(void)
{
#define MAX_LUMS 8192           // Normally 100-200, heavy: 1000

    fixed_t     bx, by;
    uint        i,  num = numLuminous;
    lumnode_t **root, *node;
    ushort      order[MAX_LUMS];

    // Should the proper order be determined?
    if(maxDynLights)
    {
        if(num > maxDynLights)
            num = maxDynLights;

        // Init the indices.
        for(i = 0; i < numLuminous; ++i)
            order[i] = i;

        qsort(order, numLuminous, sizeof(ushort), lumobjSorter);
    }

    for(i = 0; i < num; ++i)
    {
        node = getLumNode((maxDynLights ? order[i] : i));

        // Link this lumnode to the dlBlockLinks, if it can be linked.
        node->next = NULL;
        bx = X_TO_DLBX(FLT2FIX(node->lum.pos[VX]));
        by = Y_TO_DLBY(FLT2FIX(node->lum.pos[VY]));

        if(bx >= 0 && by >= 0 && bx < dlBlockWidth && by < dlBlockHeight)
        {
            root = DLB_ROOT_DLBXY(bx, by);
            node->next = *root;
            *root = node;
        }

        // Link this lumobj into its subsector (always possible).
        root = dlSubLinks + GET_SUBSECTOR_IDX(node->lum.subsector);
        node->ssNext = *root;
        *root = node;
    }

#undef MAX_LUMS
}

/**
 * @return          @ true, if the texture is already used in the list of
 *                  dynlights.
 */
#if 0 // Currently unused
static boolean isTexUsed(dynlight_t *node, DGLuint texture)
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
 * @param surface   Surface to retreive the current texture name from.
 *
 * @return          The texture name of the decoration light map for the
 *                  flat, else @c 0 = no such texture exists.
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
 * Project the given omnilight onto the specified seg. If it would be lit, a new
 * dynlight node will be created and returned.
 *
 * @param lum       Ptr to the lumobj lighting the seg.
 * @param seg       Ptr to the seg being lit.
 * @param bottom    Z height (bottom) of the section being lit.
 * @param top       Z height (top) of the section being lit.
 *
 * @return          Ptr to the projected light, ELSE @c NULL.
 */
static dynnode_t *projectOmniLightOnSegSection(lumobj_t *lum, seg_t *seg,
                                               float bottom, float top)
{
    float       s[2], t[2];
    float       dist, pntLight[2];
    dynlight_t *dyn;
    dynnode_t  *node;
    float       lumRGB[3];
    float       radius, radiusX2;

    if(!LUM_OMNI(lum)->tex)
        return NULL;

    radius = LUM_OMNI(lum)->radius / DYN_ASPECT;
    radiusX2 = 2 * radius;

    if(bottom >= top || radiusX2 == 0)
        return NULL;

    // Clip to the z section height first.
    t[0] = (lum->pos[VZ] + LUM_OMNI(lum)->zOff + radius - top) / radiusX2;
    t[1] = t[0] + (top - bottom) / radiusX2;

    if(!(t[0] < 1 && t[1] > 0))
        return NULL;

    pntLight[VX] = lum->pos[VX];
    pntLight[VY] = lum->pos[VY];

    // Calculate 2D distance between seg and light source.
    dist = ((seg->SG_v1pos[VY] - pntLight[VY]) * (seg->SG_v2pos[VX] - seg->SG_v1pos[VX]) -
            (seg->SG_v1pos[VX] - pntLight[VX]) * (seg->SG_v2pos[VY] - seg->SG_v1pos[VY]))
            / seg->length;

    // Is it close enough and on the right side?
    if(dist < 0 || dist > LUM_OMNI(lum)->radius)
        return NULL; // Nope.

    // Do a scalar projection for the offset.
    s[0] = (-((seg->SG_v1pos[VY] - pntLight[VY]) * (seg->SG_v1pos[VY] - seg->SG_v2pos[VY]) -
              (seg->SG_v1pos[VX] - pntLight[VX]) * (seg->SG_v2pos[VX] - seg->SG_v1pos[VX]))
              / seg->length +
               LUM_OMNI(lum)->radius) / (2 * LUM_OMNI(lum)->radius);

    s[1] = s[0] + seg->length / (2 * LUM_OMNI(lum)->radius);

    // Would the light be visible?
    if(s[0] >= 1 || s[1] <= 0)
        return NULL;  // Is left/right of the seg on the X/Y plane.

    calcDynLightColor(lumRGB, lum, LUM_FACTOR(dist, LUM_OMNI(lum)->radius));

    node = newDynLight(s, t);
    dyn = &node->dyn;
    memcpy(dyn->color, lumRGB, sizeof(float) * 3);
    dyn->texture = LUM_OMNI(lum)->tex;

    return node;
}

/**
 * Process the given lumobj to maybe add a dynamic light for the plane.
 *
 * @param lum       Ptr to the lumobj on which the dynlight will be based.
 */
static dynnode_t *projectOmniLightOnSubSectorPlane(lumobj_t *lum,
                    uint planeType, float height)
{
    DGLuint     lightTex;
    float       diff, lightStrength, srcRadius;
    float       s[2], t[2];
    float       pos[3];

    pos[VX] = lum->pos[VX];
    pos[VY] = lum->pos[VY];
    pos[VZ] = lum->pos[VZ];

    // Center the Z.
    pos[VZ] += LUM_OMNI(lum)->zOff;
    srcRadius = LUM_OMNI(lum)->radius / 4;
    if(srcRadius == 0)
        srcRadius = 1;

    // Determine on which side the light is for all planes.
    lightTex = 0;
    lightStrength = 0;

    if(planeType == PLN_FLOOR)
    {
        if((lightTex = LUM_OMNI(lum)->floorTex) != 0)
        {
            if(pos[VZ] > height)
                lightStrength = 1;
            else if(pos[VZ] > height - srcRadius)
                lightStrength = 1 - (height - pos[VZ]) / srcRadius;
        }
    }
    else
    {
        if((lightTex = LUM_OMNI(lum)->ceilTex) != 0)
        {
            if(pos[VZ] < height)
                lightStrength = 1;
            else if(pos[VZ] < height + srcRadius)
                lightStrength = 1 - (pos[VZ] - height) / srcRadius;
        }
    }

    // Is there light in this direction? Is it strong enough?
    if(!lightTex || !lightStrength)
        return NULL;

    // Check that the height difference is tolerable.
    if(planeType == PLN_CEILING)
        diff = height - pos[VZ];
    else
        diff = pos[VZ] - height;

    // Clamp it.
    if(diff < 0)
        diff = 0;

    if(diff < LUM_OMNI(lum)->radius)
    {
        dynlight_t *dyn;
        dynnode_t  *node;

        // Calculate dynlight position. It may still be outside
        // the bounding box the subsector.
        s[0] = -pos[VX] + LUM_OMNI(lum)->radius;
        t[0] = pos[VY] + LUM_OMNI(lum)->radius;
        s[1] = t[1] = 1.0f / (2 * LUM_OMNI(lum)->radius);

        // A dynamic light will be generated.
        node = newDynLight(s, t);
        dyn = &node->dyn;
        dyn->texture = lightTex;

        calcDynLightColor(dyn->color, lum,
                          LUM_FACTOR(diff, LUM_OMNI(lum)->radius) * lightStrength);
        return node;
    }

    return NULL;
}

static uint newLightListForSurface(void)
{
    // Ran out of light link lists?
    if(++lightLinkListCursor >= numLightLinkLists)
    {
        uint        i;
        uint        newNum = numLightLinkLists * 2;
        dynnode_t **newList;

        if(!newNum)
            newNum = 2;

        newList = Z_Calloc(newNum * sizeof(dynnode_t*), PU_LEVEL, 0);

        // Copy existing links.
        for(i = 0; i < lightLinkListCursor - 1; ++i)
        {
            newList[i] = lightLinkLists[i];
        }

        if(lightLinkLists)
            Z_Free(lightLinkLists);
        lightLinkLists = newList;
        numLightLinkLists = newNum;
    }

    return lightLinkListCursor - 1;
}

/**
 * Process dynamic lights for the specified seg.
 */
uint DL_ProcessSegSection(seg_t *seg, float bottom, float top)
{
    uint        listIdx = 0;
    lumcontact_t *con;
    boolean     haveList = false;

    if(!useDynLights)
        return 0; // Disabled.

    if(!seg || !seg->subsector)
        return 0;

    for(con = subContacts[GET_SUBSECTOR_IDX(seg->subsector)]; con; con = con->next)
    {
        dynnode_t *node = NULL;

        switch(con->lum->type)
        {
        case LT_OMNI:
            node = projectOmniLightOnSegSection(con->lum, seg, bottom, top);
            break;

        case LT_PLANE:
            node = projectPlaneGlowOnSegSection(con->lum, bottom, top);
            break;
        }

        if(node)
        {
            // Got a list for this surface yet?
            if(!haveList)
            {
                listIdx = newLightListForSurface();
                haveList = true;
            }

            linkToSurfaceLightList(node, listIdx);
        }
    }

    // Did we generate a light list?
    if(haveList)
        return listIdx + 1;

    return 0; // Nope.
}

uint DL_ProcessSubSectorPlane(subsector_t *ssec, uint plane)
{
    lumcontact_t *con;
    uint        listIdx = 0;
    boolean     haveList = false;

    if(!useDynLights)
        return 0; // Disabled.

    if(!ssec)
        return 0;

    // Sanity check.
    assert(plane >= 0 && plane < ssec->sector->planecount);

    // Process each lumobj contacting the subsector.
    for(con = subContacts[GET_SUBSECTOR_IDX(ssec)]; con; con = con->next)
    {
        lumobj_t *lum = con->lum;
        sector_t *linkSec = R_GetLinkedSector(ssec, plane);
        float     height = linkSec->SP_planevisheight(plane);
        boolean   isLit = (!R_IsSkySurface(&linkSec->SP_planesurface(plane)));
        uint      planeType = ssec->SP_plane(plane)->type;

        // View height might prevent us from seeing the light.
        if(planeType == PLN_FLOOR)
        {
            if(vy < height)
                isLit = false;
        }
        else
        {
            if(vy > height)
                isLit = false;
        }

        if(isLit)
        {
            dynnode_t *node = NULL;

            switch(lum->type)
            {
            case LT_OMNI:
                node = projectOmniLightOnSubSectorPlane(lum, plane, height);
                break;

            case LT_PLANE: // Planar lights don't affect planes.
                node = NULL;
                break;
            }

            if(node)
            {
                // Got a list for this surface yet?
                if(!haveList)
                {
                    listIdx = newLightListForSurface();
                    haveList = true;
                }

                // Link to this plane's list.
                linkToSurfaceLightList(node, listIdx);
            }
        }
    }

    // Did we generate a light list?
    if(haveList)
        return listIdx + 1;

    return 0; // Nope.
}

/**
 * Perform any processing needed before we can draw surfaces within the
 * specified subsector with dynamic lights.
 *
 * @param ssec      Ptr to the subsector to process.
 */
void DL_InitForSubsector(subsector_t *ssec)
{
    if(!useDynLights)
        return; // Disabled.

    // First make sure we know which lumobjs are contacting us.
    spreadLumobjsInSubSector(ssec);
}

/**
 * Creates the dynlight links by removing everything and then linking
 * this frame's luminous objects. Called by Rend_RenderMap() at the beginning
 * of a new frame (if the render lists are not frozen).
 */
void DL_InitForNewFrame(void)
{
    uint        i;
    sector_t   *seciter;
    mobj_t     *iter;

BEGIN_PROF( PROF_DYN_INIT_DEL );

    // Clear the dynlight lists, which are used to track the lights on
    // each surface of the map.
    deleteUsed();

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

        // If the segs of this subsector are affected by glowing planes we need
        // to create dynlights and link them.
        if(useWallGlow)
        {
            subsector_t **ssec = seciter->subsectors;
            while(*ssec)
            {
                createGlowLightPerPlaneForSubSector(*ssec);
                *ssec++;
            }
        }
    }

END_PROF( PROF_DYN_INIT_ADD );
BEGIN_PROF( PROF_DYN_INIT_LINK );

    // Link the luminous objects into the blockmap.
    linkDynNodeLuminous();

END_PROF( PROF_DYN_INIT_LINK );
}

/**
 * Calls func for all projected dynlights in the given list.
 *
 * @param listIdx   Identifier of the list to process.
 * @param data      Ptr to pass to the callback.
 * @param func      Callback to make for each object.
 *
 * @return          @c true, iff every callback returns @c true, else @c false.
 */
boolean DL_DynLightIterator(uint listIdx, void *data,
                            boolean (*func) (const dynlight_t *, void *data))
{
    dynnode_t  *node;
    boolean     retVal, isDone;

    if(listIdx == 0 || listIdx > numLightLinkLists)
        return true;
    listIdx--;

    node = lightLinkLists[listIdx];
    retVal = true;
    isDone = false;
    while(node && !isDone)
    {
        if(!func(&node->dyn, data))
        {
            retVal = false;
            isDone = true;
        }
        else
            node = node->next;
    }

    return retVal;
}

/**
 * Calls func for all luminous objects within the specified origin range.
 *
 * @param subsector The subsector in which the origin resides.
 * @param x         X coordinate of the origin (must be within subsector).
 * @param y         Y coordinate of the origin (must be within subsector).
 * @param radius    Radius of the range around the origin point.
 * @param data      Ptr to pass to the callback.
 * @param func      Callback to make for each object.
 *
 * @return          @c true, iff every callback returns @c true, else @c false.
 */
boolean DL_LumRadiusIterator(subsector_t *subsector, fixed_t x, fixed_t y,
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
    lumnode_t   *lumi; // Lum Iterator, or 'snow' in Finnish. :-)

    // Determine which dynamic light sources in the subsector get clipped.
    for(lumi = dlSubLinks[ssecidx]; lumi; lumi = lumi->ssNext)
    {
        lumobj_t *lobj = &lumi->lum;

        if(lobj->type != LT_OMNI)
            continue; // Only interested in omnilights.

        lobj->flags &= ~LUMF_CLIPPED;

        // \fixme Determine the exact centerpoint of the light in
        // DL_AddLuminous!
        if(!C_IsPointVisible(lobj->pos[VX], lobj->pos[VY],
                             lobj->pos[VZ] + LUM_OMNI(lobj)->zOff))
            lobj->flags |= LUMF_CLIPPED;    // Won't have a halo.
    }
}

/**
 * In the situation where a subsector contains both dynamic lights and
 * a polyobj, the lights must be clipped more carefully.  Here we
 * check if the line of sight intersects any of the polyobj segs that
 * face the camera.
 *
 * @param ssecidx       Subsector index in which lights will be clipped.
 */
void DL_ClipBySight(uint ssecidx)
{
    uint        num;
    vec2_t      eye;
    subsector_t *ssec = SUBSECTOR_PTR(ssecidx);
    lumnode_t  *lumi;

    // Only checks the polyobj.
    if(ssec->poly == NULL) return;

    V2_Set(eye, vx, vz);

    num = ssec->poly->numsegs;
    for(lumi = dlSubLinks[ssecidx]; lumi; lumi = lumi->ssNext)
    {
        lumobj_t   *lobj = &lumi->lum;

        if(!(lobj->flags & LUMF_CLIPPED))
        {
            uint        i;
            // We need to figure out if any of the polyobj's segments lies
            // between the viewpoint and the light source.
            for(i = 0; i < num; ++i)
            {
                seg_t      *seg = ssec->poly->segs[i];

                // Ignore segs facing the wrong way.
                if(seg->frameflags & SEGINF_FACINGFRONT)
                {
                    vec2_t      source;

                    V2_Set(source, lobj->pos[VX], lobj->pos[VY]);

                    if(V2_Intercept2(source, eye, seg->SG_v1pos,
                                     seg->SG_v2pos, NULL, NULL, NULL))
                    {
                        lobj->flags |= LUMF_CLIPPED;
                    }
                }
            }
        }
    }
}
