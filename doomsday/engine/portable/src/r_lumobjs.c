/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * r_lumobjs.c: Lumobj (luminous object) management.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
  PROF_DYN_INIT_DEL,
  PROF_DYN_INIT_ADD,
  PROF_DYN_INIT_LINK
END_PROF_TIMERS()

#define X_TO_DLBX(cx)           ( ((cx) - loBlockOrig[VX]) >> (FRACBITS+7) )
#define Y_TO_DLBY(cy)           ( ((cy) - loBlockOrig[VY]) >> (FRACBITS+7) )
#define DLB_ROOT_DLBXY(bx, by)  (loBlockLinks + bx + by*loBlockWidth)

// TYPES -------------------------------------------------------------------

typedef struct {
    float           color[3];
    float           size;
    float           flareSize;
    float           xOffset, yOffset;
} lightconfig_t;

typedef struct lumlink_s {
    struct lumlink_s *next;         // Next in the same DL block, or NULL.
    struct lumlink_s *ssNext;       // Next in the same subsector, or NULL.

    lumobj_t        lum;
} lumlink_t;

typedef struct contactfinder_data_s {
    vec2_t          box[2];
    boolean         didSpread;
    lumobj_t       *lum;
    int             firstValid;
} contactfinder_data_t;

typedef struct objcontact_s {
    struct objcontact_s *next;  // Next in the subsector.
    struct objcontact_s *nextUsed;  // Next used contact.
    void           *data;
} objcontact_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int useBias;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean loInited = false;
uint    loMaxLumobjs = 0;

int     loMaxRadius = 256;         // Dynamic lights maximum radius.
float   loRadiusFactor = 3;
int     loMinRadForBias = 136; // Lights smaller than this will NEVER
                               // be converted to BIAS sources.

int     useMobjAutoLights = true; // Enable automaticaly calculated lights
                                  // attached to mobjs.
byte    rendInfoLums = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static lumlink_t *luminousList = NULL;
static uint numLuminous = 0, maxLuminous = 0;

static lumlink_t **loBlockLinks = 0;
static fixed_t loBlockOrig[3];
static int loBlockWidth, loBlockHeight;    // In 128 blocks.

static lumlink_t **loSubLinks = 0;

// A frameCount for each block. Used to prevent multiple processing of
// a block during one frame.
static int *spreadBlocks;

// List of unused and used obj-subsector contacts.
static objcontact_t *contFirst, *contCursor;

// List of obj contacts for each subsector.
static objcontact_t **subContacts;

// CODE --------------------------------------------------------------------

/**
 * Registers the cvars and ccmds for lumobj management.
 */
void LO_Register(void)
{
    C_VAR_INT("rend-light-num", &loMaxLumobjs, 0, 0, 8000);
    C_VAR_FLOAT("rend-light-radius-scale", &loRadiusFactor, 0, 0.1f, 10);
    C_VAR_INT("rend-light-radius-max", &loMaxRadius, 0, 64, 512);
    C_VAR_INT("rend-light-radius-min-bias", &loMinRadForBias, 0, 128, 1024);

    C_VAR_BYTE("rend-info-lums", &rendInfoLums, 0, 0, 1);
}

/**
 * Link the given objcontact node to list.
 */
static __inline void linkContact(objcontact_t *con, objcontact_t **list,
                                 uint index)
{
    con->next = list[index];
    list[index] = con;
}

static void linkObjToSubSector(objcontact_t *node, uint index)
{
    linkContact(node, &subContacts[index], 0);
}

/**
 * Create a new objcontact for the given. If there are free nodes in the
 * list of unused nodes, the new contact is taken from there.
 *
 * @param data          Ptr to the object the contact is required for.
 *
 * @return              Ptr to the new objcontact.
 */
static objcontact_t *newContact(void *data)
{
    objcontact_t       *con;

    if(contCursor == NULL)
    {
        con = Z_Malloc(sizeof(objcontact_t), PU_STATIC, NULL);

        // Link to the list of objcontact nodes.
        con->nextUsed = contFirst;
        contFirst = con;
    }
    else
    {
        con = contCursor;
        contCursor = contCursor->nextUsed;
    }

    con->data = data;
    return con;
}

/**
 * \note: No bounds checking, it is assumed callers know what they are doing.
 *
 * @return              Ptr to the lumnode at the given index.
 */
static __inline lumlink_t *getLumNode(uint idx)
{
    return &luminousList[idx];
}

void LO_InitForMap(void)
{
    gamemap_t          *map = P_GetCurrentMap();
    float               min[3], max[3];

    // First initialize the subsector links (root pointers).
    loSubLinks =
        Z_Calloc(sizeof(lumlink_t*) * numSSectors, PU_LEVELSTATIC, 0);

    // Then the blocklinks.
    P_GetMapBounds(map, &min[0], &max[0]);

    // Origin has fixed-point coordinates.
    loBlockOrig[VX] = FLT2FIX(min[VX]);
    loBlockOrig[VY] = FLT2FIX(min[VY]);
    loBlockOrig[VZ] = FLT2FIX(min[VZ]);

    max[VX] -= min[VX];
    max[VY] -= min[VY];
    loBlockWidth  = (FLT2FIX(max[VX]) >> (FRACBITS + 7)) + 1;
    loBlockHeight = (FLT2FIX(max[VY]) >> (FRACBITS + 7)) + 1;

    // Blocklinks is a table of lumobj_t pointers.
    loBlockLinks =
        M_Realloc(loBlockLinks,
                sizeof(lumlink_t*) * loBlockWidth * loBlockHeight);

    // A frameCount was each block.
    spreadBlocks =
        Z_Malloc(sizeof(int) * loBlockWidth * loBlockHeight,
                 PU_LEVELSTATIC, 0);

    // Initialize obj -> subsector contacts.
    subContacts =
        Z_Calloc(numSSectors * sizeof(objcontact_t*), PU_LEVELSTATIC, 0);
}

/**
 * Called once during engine shutdown by Rend_Reset(). Releases any system
 * resources acquired by the lumobj management subsystem.
 */
void LO_Clear(void)
{
    if(luminousList)
        Z_Free(luminousList);
    luminousList = 0;
    maxLuminous = numLuminous = 0;

    M_Free(loBlockLinks);
    loBlockLinks = 0;
    loBlockOrig[VX] = loBlockOrig[VY] = 0;
    loBlockWidth = loBlockHeight = 0;
}

/**
 * Called at the begining of each frame (iff the render lists are not frozen)
 * by Rend_RenderMap().
 */
void LO_ClearForFrame(void)
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
    memset(loSubLinks, 0, sizeof(lumlink_t *) * numSSectors);
    memset(loBlockLinks, 0, sizeof(lumlink_t *) * loBlockWidth * loBlockHeight);

    numLuminous = 0;
}

/**
 * @return              The number of active lumobjs for this frame.
 */
uint LO_GetNumLuminous(void)
{
    return numLuminous;
}

/**
 * Allocate a new lumobj.
 *
 * @return              Index (name) by which the lumobj should be referred.
 */
uint LO_NewLuminous(lumtype_t type)
{
    lumlink_t          *node, *newList;

    numLuminous++;

    // Only allocate memory when it's needed.
    // \fixme No upper limit?
    if(numLuminous > maxLuminous)
    {
        maxLuminous *= 2;

        // The first time, allocate thirty two lumobjs.
        if(!maxLuminous)
            maxLuminous = 32;

        newList = Z_Malloc(sizeof(lumlink_t) * maxLuminous, PU_STATIC, 0);

        // Copy the old data over to the new list.
        if(luminousList)
        {
            memcpy(newList, luminousList,
                   sizeof(lumlink_t) * (numLuminous - 1));
            Z_Free(luminousList);
        }
        luminousList = newList;
    }

    // Clear the new lumobj.
    node = (luminousList + numLuminous - 1);
    memset(node, 0, sizeof(lumlink_t));

    node->lum.type = type;

    return numLuminous; // == index + 1
}

/**
 * Retrieve a ptr to the lumobj with the given index. A public interface to
 * the lumobj list.
 *
 * @return              Ptr to the lumobj with the given 1-based index.
 */
lumobj_t *LO_GetLuminous(uint idx)
{
    if(!(idx == 0 || idx > numLuminous))
        return &getLumNode(idx - 1)->lum;

    return NULL;
}

/**
 * Registers the given mobj as a luminous, light-emitting object.
 * NOTE: This is called each frame for each luminous object!
 *
 * @param mo            Ptr to the mobj to register.
 */
void LO_AddLuminous(mobj_t *mo)
{
    int                 lump;
    uint                i;
    float               mul;
    float               xOff;
    float               center;
    int                 flags = 0;
    int                 radius, flareSize;
    float               rgb[3];
    lumobj_t           *l;
    lightconfig_t       cf;
    ded_light_t        *def = 0;
    spritedef_t        *sprdef;
    spriteframe_t      *sprframe;

    mo->light = 0;

    if(((mo->state && (mo->state->flags & STF_FULLBRIGHT)) &&
         !(mo->ddFlags & DDMF_DONTDRAW)) ||
       (mo->ddFlags & DDMF_ALWAYSLIT))
    {
        // Are the automatically calculated light values for fullbright
        // sprite frames in use?
        if(mo->state &&
           (!useMobjAutoLights || (mo->state->flags & STF_NOAUTOLIGHT)) &&
           !mo->state->light)
           return;

        // Determine the sprite frame lump of the source.
        sprdef = &sprites[mo->sprite];
        sprframe = &sprdef->spriteFrames[mo->frame];
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
        cf.size = cf.flareSize = spritelumps[lump]->lumSize;
        cf.xOffset = spritelumps[lump]->flareX;
        cf.yOffset = spritelumps[lump]->flareY;

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
            if(def->haloRadius)
                cf.flareSize = def->haloRadius;
            flags |= def->flags;
        }

        center =
            spritelumps[lump]->topOffset -
            mo->floorClip - R_GetBobOffset(mo) -
            cf.yOffset;

        // Will the sprite be allowed to go inside the floor?
        mul =
            mo->pos[VZ] + spritelumps[lump]->topOffset -
            (float) spritelumps[lump]->height - mo->subsector->sector->SP_floorheight;
        if(!(mo->ddFlags & DDMF_NOFITBOTTOM) && mul < 0)
        {
            // Must adjust.
            center -= mul;
        }

        radius = cf.size * 40 * loRadiusFactor;

        // Don't make a too small light.
        if(radius < 32)
            radius = 32;

        flareSize = cf.flareSize * 60 * (50 + haloSize) / 100.0f;

        if(flareSize < 8)
            flareSize = 8;

        // Does the mobj use a light scale?
        if(mo->ddFlags & DDMF_LIGHTSCALE)
        {
            // Also reduce the size of the light according to
            // the scale flags. *Won't affect the flare.*
            mul =
                1.0f -
                ((mo->ddFlags & DDMF_LIGHTSCALE) >> DDMF_LIGHTSCALESHIFT) /
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

        // This'll allow a halo to be rendered. If the light is hidden from
        // view by world geometry, the light pointer will be set to NULL.
        mo->light = LO_NewLuminous(LT_OMNI);

        l = LO_GetLuminous(mo->light);
        l->flags = flags;
        l->flags |= LUMF_CLIPPED;
        l->pos[VX] = mo->pos[VX];
        l->pos[VY] = mo->pos[VY];
        l->pos[VZ] = mo->pos[VZ];
        // Approximate the distance in 3D.
        l->distanceToViewer =
            P_ApproxDistance3(mo->pos[VX] - viewX,
                              mo->pos[VY] - viewY,
                              mo->pos[VZ] - viewZ);

        l->subsector = mo->subsector;
        LUM_OMNI(l)->haloFactor = mo->haloFactor;
        LUM_OMNI(l)->zOff = center;
        LUM_OMNI(l)->xOff = xOff;

        // Don't make too large a light.
        if(radius > loMaxRadius)
            radius = loMaxRadius;

        LUM_OMNI(l)->radius = radius;
        LUM_OMNI(l)->flareMul = 1;
        LUM_OMNI(l)->flareSize = flareSize;
        for(i = 0; i < 3; ++i)
            l->color[i] = rgb[i];

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

boolean LOIT_LinkObjToSubSector(subsector_t *subsector, void *data)
{
    objcontact_t       *con = newContact(data);

    // Link it to the contact list for this subsector.
    linkObjToSubSector(con, GET_SUBSECTOR_IDX(subsector));

    return true; // Continue iteration.
}

/**
 * Iterate subsectors of sector, within or intersecting the specified
 * bounding box, looking for those which are close enough to be lit by the
 * given lumobj. For each, register a subsector > lumobj "contact".
 *
 * @param lum           Ptr to the lumobj hoping to contact the sector.
 * @param box           Subsectors within this bounding box will be processed.
 * @param sector        Ptr to the sector to check for contacts.
 */
static void contactSector(lumobj_t *lum, const arvec2_t box, sector_t *sector)
{
    P_SubsectorsBoxIteratorv(box, sector, LOIT_LinkObjToSubSector, lum);
}

/**
 * Attempt to spread the light from the given contact over a twosided
 * linedef, into the (relative) back sector.
 *
 * @param line          Ptr to linedef to attempt to spread over.
 * @param data          Ptr to contactfinder_data structure.
 *
 * @return              @c true, because this function is also used as an
 *                      iterator.
 */
boolean LOIT_ContactFinder(linedef_t *line, void *data)
{
    contactfinder_data_t *light = data;
    sector_t           *source, *dest;
    float               distance;
    vertex_t           *vtx;
    lumobj_t           *l;

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
    if(line->bBox[BOXRIGHT]  <= light->box[0][VX] ||
       line->bBox[BOXLEFT]   >= light->box[1][VX] ||
       line->bBox[BOXTOP]    <= light->box[0][VY] ||
       line->bBox[BOXBOTTOM] >= light->box[1][VY])
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
        ((vtx->V_pos[VY] - l->pos[VY]) * line->dX -
         (vtx->V_pos[VX] - l->pos[VX]) * line->dY)
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
 * @param lum           Ptr to lumobj to find subsector contacts for.
 */
static void findContacts(lumobj_t *lum)
{
    int                 firstValid;
    contactfinder_data_t light;
    vec2_t              point;
    float               radius;
    static uint         numSpreads = 0, numFinds = 0;

    if(lum->type != LT_OMNI)
        return; // Only omni lights spread.

    firstValid = ++validCount;

    // Use a slightly smaller radius than what the light really is.
    radius = LUM_OMNI(lum)->radius * 0.9f;

    // Do the sector spread. Begin from the light's own sector.
    lum->subsector->sector->validCount = validCount;

    light.lum = lum;
    light.firstValid = firstValid;
    V2_Set(point, lum->pos[VX] - radius, lum->pos[VY] - radius);
    V2_InitBox(light.box, point);
    V2_Set(point, lum->pos[VX] + radius, lum->pos[VY] + radius);
    V2_AddToBox(light.box, point);

    contactSector(lum, light.box, lum->subsector->sector);

    numFinds++;

    // We'll keep doing this until the light has spreaded everywhere
    // inside the bounding box.
    do
    {
        light.didSpread = false;
        numSpreads++;

        P_AllLinesBoxIteratorv(light.box, LOIT_ContactFinder, &light);

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
    int                 xl, xh, yl, yh, x, y;
    int                *count;
    lumlink_t          *iter;

    xl = X_TO_DLBX(FLT2FIX(subsector->bBox[0].pos[VX] - loMaxRadius));
    xh = X_TO_DLBX(FLT2FIX(subsector->bBox[1].pos[VX] + loMaxRadius));
    yl = Y_TO_DLBY(FLT2FIX(subsector->bBox[0].pos[VY] - loMaxRadius));
    yh = Y_TO_DLBY(FLT2FIX(subsector->bBox[1].pos[VY] + loMaxRadius));

    // Are we completely outside the blockmap?
    if(xh < 0 || xl >= loBlockWidth || yh < 0 || yl >= loBlockHeight)
        return;

    // Clip to blockmap bounds.
    if(xl < 0)
        xl = 0;
    if(xh >= loBlockWidth)
        xh = loBlockWidth - 1;
    if(yl < 0)
        yl = 0;
    if(yh >= loBlockHeight)
        yh = loBlockHeight - 1;

    for(x = xl; x <= xh; ++x)
        for(y = yl; y <= yh; ++y)
        {
            count = &spreadBlocks[x + y * loBlockWidth];
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
    lumobj_t           *lum1 = &getLumNode(*(const ushort *) e1)->lum;
    lumobj_t           *lum2 = &getLumNode(*(const ushort *) e2)->lum;

    if(lum1->distanceToViewer > lum2->distanceToViewer)
        return 1;
    else if(lum1->distanceToViewer < lum2->distanceToViewer)
        return -1;
    else
        return 0;
}

/**
 * Clears the loBlockLinks and then links all the listed luminous objects.
 * Called by LO_ClearForNewFrame() at the beginning of each frame (iff the
 * render lists are not frozen).
 */
static void linkLuminous(void)
{
#define MAX_LUMS 8192           // Normally 100-200, heavy: 1000

    int                 bx, by;
    uint                i,  num = numLuminous;
    lumlink_t         **root, *node;
    ushort              order[MAX_LUMS];

    // Should the proper order be determined?
    if(loMaxLumobjs)
    {
        if(num > loMaxLumobjs)
            num = loMaxLumobjs;

        // Init the indices.
        for(i = 0; i < numLuminous; ++i)
            order[i] = i;

        qsort(order, numLuminous, sizeof(ushort), lumobjSorter);
    }

    for(i = 0; i < num; ++i)
    {
        node = getLumNode((loMaxLumobjs ? order[i] : i));

        // Link this lumnode to the loBlockLinks, if it can be linked.
        node->next = NULL;
        bx = X_TO_DLBX(FLT2FIX(node->lum.pos[VX]));
        by = Y_TO_DLBY(FLT2FIX(node->lum.pos[VY]));

        if(bx >= 0 && by >= 0 && bx < loBlockWidth && by < loBlockHeight)
        {
            root = DLB_ROOT_DLBXY(bx, by);
            node->next = *root;
            *root = node;
        }

        // Link this lumobj into its subsector (always possible).
        root = loSubLinks + GET_SUBSECTOR_IDX(node->lum.subsector);
        node->ssNext = *root;
        *root = node;
    }

#undef MAX_LUMS
}

/**
 * Perform any processing needed before we can draw surfaces within the
 * specified subsector with dynamic lights.
 *
 * @param ssec          Ptr to the subsector to process.
 */
void LO_InitForSubsector(subsector_t *ssec)
{
    if(!useDynLights)
        return; // Disabled.

    // First make sure we know which lumobjs are contacting us.
    spreadLumobjsInSubSector(ssec);
}

/**
 * Generate one dynlight node for each plane glow.
 * The light is attached to the appropriate dynlight node list.
 *
 * @param ssec          Ptr to the subsector to process.
 */
static void createGlowLightPerPlaneForSubSector(subsector_t *ssec)
{
    uint                g;
    plane_t            *glowPlanes[2], *pln;

    glowPlanes[PLN_FLOOR] = R_GetLinkedSector(ssec, PLN_FLOOR)->planes[PLN_FLOOR];
    glowPlanes[PLN_CEILING] = R_GetLinkedSector(ssec, PLN_CEILING)->planes[PLN_CEILING];

    //// \fixme $nplanes
    for(g = 0; g < 2; ++g)
    {
        uint                light;
        lumobj_t           *l;

        pln = glowPlanes[g];

        if(pln->glow <= 0)
            continue;

        light = LO_NewLuminous(LT_PLANE);

        l = LO_GetLuminous(light);
        l->flags = LUMF_NOHALO | LUMF_CLIPPED;
        l->pos[VX] = ssec->midPoint.pos[VX];
        l->pos[VY] = ssec->midPoint.pos[VY];
        l->pos[VZ] = pln->visHeight;

        LUM_PLANE(l)->normal[VX] = pln->PS_normal[VX];
        LUM_PLANE(l)->normal[VY] = pln->PS_normal[VY];
        LUM_PLANE(l)->normal[VZ] = pln->PS_normal[VZ];

        // Approximate the distance in 3D.
        l->distanceToViewer =
            P_ApproxDistance3(l->pos[VX] - viewX,
                              l->pos[VY] - viewY,
                              l->pos[VZ] - viewZ);

        l->subsector = ssec;
        l->color[CR] = pln->glowRGB[CR];
        l->color[CG] = pln->glowRGB[CG];
        l->color[CB] = pln->glowRGB[CB];

        LUM_PLANE(l)->intensity = pln->glow;
        LUM_PLANE(l)->tex = GL_PrepareLSTexture(LST_GRADIENT, NULL);

        // Plane lights don't spread, so just link the lum to its own ssec.
        LOIT_LinkObjToSubSector(l->subsector, l);
    }
}

/**
 * Creates the lumobj links by removing everything and then linking this
 * frame's luminous objects. Called by Rend_RenderMap() at the beginning
 * of a new frame (if the render lists are not frozen).
 */
void LO_InitForNewFrame(void)
{
    uint                i;
    sector_t           *seciter;
    mobj_t             *iter;

BEGIN_PROF( PROF_DYN_INIT_DEL );

    // Start reusing nodes from the first one in the list.
    contCursor = contFirst;
    memset(subContacts, 0, numSSectors * sizeof(objcontact_t *));

END_PROF( PROF_DYN_INIT_DEL );

    // The luminousList already contains lumobjs if there are any light
    // decorations in use.
    loInited = true;

BEGIN_PROF( PROF_DYN_INIT_ADD );

    for(i = 0, seciter = sectors; i < numSectors; seciter++, ++i)
    {
        for(iter = seciter->mobjList; iter; iter = iter->sNext)
        {
            LO_AddLuminous(iter);
        }

        // If the segs of this subsector are affected by glowing planes we need
        // to create dynlights and link them.
        if(useWallGlow)
        {
            subsector_t **ssec = seciter->ssectors;
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
    linkLuminous();

END_PROF( PROF_DYN_INIT_LINK );
}

boolean LO_IterateSubsectorContacts(subsector_t *ssec,
                                    boolean (*func) (void *, void *),
                                    void *data)
{
    objcontact_t       *con;

    for(con = subContacts[GET_SUBSECTOR_IDX(ssec)]; con; con = con->next)
    {
        if(!func(con->data, data))
            return false;
    }

    return true;
}

typedef struct lumobjiterparams_s {
    float           origin[2];
    float           radius;
    void           *data;
    boolean       (*func) (lumobj_t *, float, void *data);
} lumobjiterparams_t;

boolean LOIT_RadiusLumobjs(void *ptr, void *data)
{
    lumobj_t       *lum = (lumobj_t*) ptr;
    lumobjiterparams_t *params = data;
    float           dist =
        P_ApproxDistance(lum->pos[VX] - params->origin[VX],
                         lum->pos[VY] - params->origin[VY]);

    if(dist <= params->radius && !params->func(lum, dist, params->data))
        return false; // Stop iteration.

    return true; // Continue iteration.
}

/**
 * Calls func for all luminous objects within the specified origin range.
 *
 * @param subsector     The subsector in which the origin resides.
 * @param x             X coordinate of the origin (must be within subsector).
 * @param y             Y coordinate of the origin (must be within subsector).
 * @param radius        Radius of the range around the origin point.
 * @param data          Ptr to pass to the callback.
 * @param func          Callback to make for each object.
 *
 * @return              @c true, iff every callback returns @c true, else @c false.
 */
boolean LO_LumobjsRadiusIterator(subsector_t *ssec, float x, float y,
                                 float radius, void *data,
                                 boolean (*func) (lumobj_t *, float, void *data))
{
    lumobjiterparams_t  params;

    if(!ssec)
        return true;

    params.origin[VX] = x;
    params.origin[VY] = y;
    params.radius = radius;
    params.func = func;
    params.data = data;

    return LO_IterateSubsectorContacts(ssec, LOIT_RadiusLumobjs,
                                       (void*) &params);
}

/**
 * Clip lights by subsector.
 *
 * @param ssecidx       Subsector index in which lights will be clipped.
 */
void LO_ClipInSubsector(uint ssecidx)
{
    lumlink_t          *lumi; // Lum Iterator, or 'snow' in Finnish. :-)

    // Determine which dynamic light sources in the subsector get clipped.
    for(lumi = loSubLinks[ssecidx]; lumi; lumi = lumi->ssNext)
    {
        lumobj_t           *lobj = &lumi->lum;

        if(lobj->type != LT_OMNI)
            continue; // Only interested in omnilights.

        lobj->flags &= ~LUMF_CLIPPED;

        // \fixme Determine the exact centerpoint of the light in
        // LO_AddLuminous!
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
void LO_ClipBySight(uint ssecidx)
{
    uint                num;
    vec2_t              eye;
    subsector_t        *ssec = SUBSECTOR_PTR(ssecidx);
    lumlink_t          *lumi;

    // Only checks the polyobj.
    if(ssec->polyObj == NULL) return;

    V2_Set(eye, vx, vz);

    num = ssec->polyObj->numSegs;
    for(lumi = loSubLinks[ssecidx]; lumi; lumi = lumi->ssNext)
    {
        lumobj_t           *lobj = &lumi->lum;

        if(!(lobj->flags & LUMF_CLIPPED))
        {
            uint                i;
            // We need to figure out if any of the polyobj's segments lies
            // between the viewpoint and the light source.
            for(i = 0; i < num; ++i)
            {
                seg_t              *seg = ssec->polyObj->segs[i];

                // Ignore segs facing the wrong way.
                if(seg->frameFlags & SEGINF_FACINGFRONT)
                {
                    vec2_t              source;

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
