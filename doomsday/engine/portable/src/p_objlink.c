/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_objlink.c: Objlink management.
 *
 * Object>surface contacts and object>subsector spreading.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_defs.h"

// MACROS ------------------------------------------------------------------

#define X_TO_OBBX(bm, cx)           (((cx) - (bm)->origin[0]) >> (FRACBITS+7))
#define Y_TO_OBBY(bm, cy)           (((cy) - (bm)->origin[1]) >> (FRACBITS+7))
#define OBB_XY(bm, bx, by)          (&(bm)->blocks[(bx) + (by) * (bm)->width])

// TYPES -------------------------------------------------------------------

typedef struct objlink_s {
    struct objlink_s* nextInBlock; // Next in the same obj block, or NULL.
    struct objlink_s* nextUsed;
    struct objlink_s* next; // Next in list of ALL objlinks.
    objtype_t       type;
    void*           obj;
} objlink_t;

typedef struct objblock_s {
    objlink_t*      head;

    // Used to prevent multiple processing of a block during one refresh frame.
    boolean         doneSpread;
} objblock_t;

typedef struct objblockmap_s {
    objblock_t*     blocks;
    fixed_t         origin[2];
    int             width, height; // In blocks.
} objblockmap_t;

typedef struct contactfinder_data_s {
    void*           obj;
    objtype_t       objType;
    vec3_t          objPos;
    float           objRadius;
    vec2_t          box[2];
    boolean         didSpread;
    int             firstValid;
} contactfinderparams_t;

typedef struct objcontact_s {
    struct objcontact_s* next; // Next in the subsector.
    struct objcontact_s* nextUsed; // Next used contact.
    void*           obj;
} objcontact_t;

typedef struct objcontactlist_s {
    objcontact_t*   head[NUM_OBJ_TYPES];
} objcontactlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static objlink_t* objLinks = NULL;
static objlink_t* objLinkFirst, *objLinkCursor;

static objblockmap_t* objBlockmap = NULL;

// List of unused and used obj-subsector contacts.
static objcontact_t* contFirst, *contCursor;

// List of obj contacts for each subsector.
static objcontactlist_t* subContacts;

// CODE --------------------------------------------------------------------

/**
 * Link the given objcontact node to list.
 */
static __inline void linkContact(objcontact_t* con, objcontact_t** list,
                                 uint index)
{
    con->next = list[index];
    list[index] = con;
}

static void linkContactToSubSector(objcontact_t* node, objtype_t type,
                                   uint index)
{
    linkContact(node, &subContacts[index].head[type], 0);
}

/**
 * Create a new objcontact for the given obj. If there are free nodes in
 * the list of unused nodes, the new contact is taken from there.
 *
 * @param obj           Ptr to the obj the contact is required for.
 *
 * @return              Ptr to the new objcontact.
 */
static objcontact_t* allocObjContact(void)
{
    objcontact_t*       con;

    if(contCursor == NULL)
    {
        con = Z_Malloc(sizeof(*con), PU_STATIC, NULL);

        // Link to the list of objcontact nodes.
        con->nextUsed = contFirst;
        contFirst = con;
    }
    else
    {
        con = contCursor;
        contCursor = contCursor->nextUsed;
    }

    con->obj = NULL;

    return con;
}

static objlink_t* allocObjLink(void)
{
    objlink_t*          oLink;

    if(objLinkCursor == NULL)
    {
        oLink = Z_Malloc(sizeof(*oLink), PU_STATIC, NULL);

        // Link to the list of objlink nodes.
        oLink->nextUsed = objLinkFirst;
        objLinkFirst = oLink;
    }
    else
    {
        oLink = objLinkCursor;
        objLinkCursor = objLinkCursor->nextUsed;
    }

    oLink->nextInBlock = NULL;
    oLink->obj = NULL;

    // Link it to the list of in-use objlinks.
    oLink->next = objLinks;
    objLinks = oLink;

    return oLink;
}

objblockmap_t* R_ObjBlockmapCreate(float originX, float originY, int width,
                                   int height)
{
    objblockmap_t*      obm;

    obm = Z_Malloc(sizeof(objblockmap_t), PU_MAPSTATIC, 0);

    // Origin has fixed-point coordinates.
    obm->origin[0] = FLT2FIX(originX);
    obm->origin[1] = FLT2FIX(originY);
    obm->width = width;
    obm->height = height;
    obm->blocks = Z_Malloc(sizeof(*obm->blocks) * obm->width * obm->height,
                           PU_MAPSTATIC, 0);

    return obm;
}

void R_InitObjLinksForMap(void)
{
    gamemap_t*          map = P_GetCurrentMap();
    float               min[2], max[2];
    int                 width, height;

    // Determine the dimensions of the objblockmap.
    P_GetMapBounds(map, &min[0], &max[0]);
    max[0] -= min[0];
    max[1] -= min[1];

    // In blocks, 128x128 world units.
    width  = (FLT2FIX(max[0]) >> (FRACBITS + 7)) + 1;
    height = (FLT2FIX(max[1]) >> (FRACBITS + 7)) + 1;

    // Create the blockmap.
    objBlockmap = R_ObjBlockmapCreate(min[0], min[1], width, height);

    // Initialize obj -> subsector contact lists.
    subContacts =
        Z_Calloc(sizeof(*subContacts) * numSSectors, PU_MAPSTATIC, 0);
}

void R_ObjBlockmapClear(objblockmap_t* obm)
{
    if(obm)
    {   // Clear the list head ptrs and doneSpread flags.
        memset(obm->blocks, 0, sizeof(*obm->blocks) * obm->width * obm->height);
    }
}

/**
 * Called at the begining of each frame (iff the render lists are not frozen)
 * by R_BeginWorldFrame().
 */
void R_ClearObjLinksForFrame(void)
{
    // Clear all the roots.
    R_ObjBlockmapClear(objBlockmap);

    objLinkCursor = objLinkFirst;
    objLinks = NULL;
}

void R_ObjLinkCreate(void* obj, objtype_t type)
{
    objlink_t*          ol = allocObjLink();

    ol->obj = obj;
    ol->type = type;
}

boolean RIT_LinkObjToSubSector(subsector_t* subsector, void* data)
{
    linkobjtossecparams_t* params = (linkobjtossecparams_t*) data;
    objcontact_t*       con = allocObjContact();

    con->obj = params->obj;

    // Link the contact list for this subsector.
    linkContactToSubSector(con, params->type, GET_SUBSECTOR_IDX(subsector));

    return true; // Continue iteration.
}

/**
 * Iterate subsectors of sector, within or intersecting the specified
 * bounding box, looking for those which are close enough to the given obj.
 * For each, register a subsector > obj "contact".
 *
 * @param obj           The obj hoping contacting the sector.
 * @param box           Subsectors within this bounding box will be processed.
 * @param sector        Ptr to the sector to check for contacts.
 */
static void contactSector(void* obj, objtype_t type, const arvec2_t box,
                          sector_t* sector)
{
    linkobjtossecparams_t params;

    params.obj = obj;
    params.type = type;

    P_SubsectorsBoxIteratorv(box, sector, RIT_LinkObjToSubSector, &params);
}

/**
 * Attempt to spread the obj from the given contact over a twosided linedef,
 * into the (relative) back sector.
 *
 * @param line          Ptr to linedef to attempt to spread over.
 * @param data          Ptr to contactfinder_data structure.
 *
 * @return              @c true, because this function is also used as an
 *                      iterator.
 */
boolean RIT_ContactFinder(linedef_t* line, void* data)
{
    contactfinderparams_t* params = (contactfinderparams_t*) data;
    sector_t*           source, *dest;
    float               distance;
    vertex_t*           vtx;

    if(!line->L_frontside || !line->L_backside ||
       line->L_frontsector == line->L_backsector)
    {
        // Line must be between two different sectors.
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

    if(dest->validCount >= params->firstValid &&
       dest->validCount <= validCount + 1)
    {
        // This was already spreaded to.
        return true;
    }

    // Is this line inside the params's bounds?
    if(line->bBox[BOXRIGHT]  <= params->box[0][VX] ||
       line->bBox[BOXLEFT]   >= params->box[1][VX] ||
       line->bBox[BOXTOP]    <= params->box[0][VY] ||
       line->bBox[BOXBOTTOM] >= params->box[1][VY])
    {
        // The line is not inside the params's bounds.
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

    // Don't spread if the middle material completely fills the gap between
    // floor and ceiling (direction is from dest to source).
    if(Rend_DoesMidTextureFillGap(line,
        dest == line->L_backsector? BACK : FRONT))
        return true;

    // Calculate 2D distance to line.
    vtx = line->L_v1;
    distance = ((vtx->V_pos[VY] - params->objPos[VY]) * line->dX -
                (vtx->V_pos[VX] - params->objPos[VX]) * line->dY) /
                   line->length;

    if((source == line->L_frontsector && distance < 0) ||
       (source == line->L_backsector && distance > 0))
    {
        // Can't spread in this direction.
        return true;
    }

    // Check distance against the obj radius.
    if(distance < 0)
        distance = -distance;
    if(distance >= params->objRadius)
    {   // The obj doesn't reach that far.
        return true;
    }

    // Obj spreads to the destination sector.
    params->didSpread = true;

    // During next step, obj will continue spreading from there.
    dest->validCount = validCount + 1;

    // Add this obj to the destination's subsectors.
    contactSector(params->obj, params->objType, params->box, dest);

    return true;
}

/**
 * Create a contact for the objlink in all the subsectors the linked obj is
 * contacting (tests done on bounding boxes and the sector spread test).
 *
 * @param oLink         Ptr to objlink to find subsector contacts for.
 */
static void findContacts(objlink_t* oLink)
{
    static uint         numSpreads = 0, numFinds = 0;

    int                 firstValid;
    contactfinderparams_t params;
    vec2_t              point;
    float*              radius;
    pvec3_t             pos;
    subsector_t**       ssec;

    firstValid = ++validCount;

    switch(oLink->type)
    {
    case OT_LUMOBJ:
        {
        lumobj_t*           lum = (lumobj_t*) oLink->obj;

        if(lum->type != LT_OMNI)
            return; // Only omni lights spread.

        pos = lum->pos;
        radius = &LUM_OMNI(lum)->radius;
        ssec = &lum->subsector;
        break;
        }
    case OT_MOBJ:
        {
        mobj_t*             mo = (mobj_t*) oLink->obj;

        pos = mo->pos;
        radius = &mo->radius;
        ssec = &mo->subsector;
        break;
        }
    }

    // Do the sector spread. Begin from the obj's own sector.
    (*ssec)->sector->validCount = validCount;

    params.obj = oLink->obj;
    params.objType = oLink->type;
    V3_Copy(params.objPos, pos);
    // Use a slightly smaller radius than what the obj really is.
    params.objRadius = *radius * .9f;
    params.firstValid = firstValid;
    V2_Set(point, params.objPos[VX] - *radius, params.objPos[VY] - *radius);
    V2_InitBox(params.box, point);
    V2_Set(point, params.objPos[VX] + *radius, params.objPos[VY] + *radius);
    V2_AddToBox(params.box, point);

    // Always contact the obj's own sector.
    contactSector(oLink->obj, oLink->type, params.box, (*ssec)->sector);

    numFinds++;

    // We'll keep doing this until the obj has spreaded everywhere inside
    // the bounding box.
    do
    {
        params.didSpread = false;
        numSpreads++;

        P_AllLinesBoxIteratorv(params.box, RIT_ContactFinder, &params);

        // Increment validCount for the next round of spreading.
        validCount++;
    }
    while(params.didSpread);

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
 * Spread obj contacts in the subsector > obj blockmap to all other
 * subsectors within the block.
 *
 * @param subsector     Ptr to the subsector to spread the obj contacts of.
 */
void R_ObjBlockmapSpreadObjsInSubSector(const objblockmap_t* obm,
                                        const subsector_t* ssec,
                                        float maxRadius)
{
    int                 xl, xh, yl, yh, x, y;
    objlink_t*          iter;

    if(!obm || !ssec)
        return; // Wha?

    xl = X_TO_OBBX(obm, FLT2FIX(ssec->bBox[0].pos[VX] - maxRadius));
    xh = X_TO_OBBX(obm, FLT2FIX(ssec->bBox[1].pos[VX] + maxRadius));
    yl = Y_TO_OBBY(obm, FLT2FIX(ssec->bBox[0].pos[VY] - maxRadius));
    yh = Y_TO_OBBY(obm, FLT2FIX(ssec->bBox[1].pos[VY] + maxRadius));

    // Are we completely outside the blockmap?
    if(xh < 0 || xl >= obm->width || yh < 0 || yl >= obm->height)
        return;

    // Clip to blockmap bounds.
    if(xl < 0)
        xl = 0;
    if(xh >= obm->width)
        xh = obm->width - 1;
    if(yl < 0)
        yl = 0;
    if(yh >= obm->height)
        yh = obm->height - 1;

    for(x = xl; x <= xh; ++x)
        for(y = yl; y <= yh; ++y)
        {
            objblock_t*         block = OBB_XY(obm, x, y);

            if(!block->doneSpread)
            {
                // Spread the objs in this block.
                iter = block->head;
                while(iter)
                {
                    findContacts(iter);
                    iter = iter->nextInBlock;
                }

                block->doneSpread = true;
            }
        }
}

/**
 * Perform any processing needed before we can draw surfaces within the
 * specified subsector with dynamic lights.
 *
 * @param ssec          Ptr to the subsector to process.
 */
void R_InitForSubsector(subsector_t* ssec)
{
    float               maxRadius = MAX_OF(DDMOBJ_RADIUS_MAX, loMaxRadius);

    // Make sure we know which objs are contacting us.
    R_ObjBlockmapSpreadObjsInSubSector(objBlockmap, ssec, maxRadius);
}

/**
 * Link all objlinks into the objlink blockmap.
 */
void R_ObjBlockmapLinkObjLink(objblockmap_t* obm, objlink_t* oLink,
                               float x, float y)
{
    int                 bx, by;
    objlink_t**         root;

    if(!obm || !oLink)
        return; // Wha?

    oLink->nextInBlock = NULL;
    bx = X_TO_OBBX(obm, FLT2FIX(x));
    by = Y_TO_OBBY(obm, FLT2FIX(y));

    if(bx >= 0 && by >= 0 && bx < obm->width && by < obm->height)
    {
        root = &OBB_XY(obm, bx, by)->head;
        oLink->nextInBlock = *root;
        *root = oLink;
    }
}

/**
 * Called by R_BeginWorldFrame() at the beginning of render tic (iff the
 * render lists are not frozen) to link all objlinks into the objlink
 * blockmap.
 */
void R_LinkObjs(void)
{
    objlink_t*          oLink;

    // Link objlinks into the objlink blockmap.
    oLink = objLinks;
    while(oLink)
    {
        pvec3_t             pos;

        switch(oLink->type)
        {
        case OT_LUMOBJ:
            pos = ((lumobj_t*) oLink->obj)->pos;
            break;

        case OT_MOBJ:
            pos = ((mobj_t*) oLink->obj)->pos;
            break;
        }

        R_ObjBlockmapLinkObjLink(objBlockmap, oLink, pos[VX], pos[VY]);

        oLink = oLink->next;
    }
}

/**
 * Initialize the obj > subsector contact lists ready for adding new
 * luminous objects. Called by R_BeginWorldFrame() at the beginning of a new
 * frame (if the render lists are not frozen).
 */
void R_InitForNewFrame(void)
{
    // Start reusing nodes from the first one in the list.
    contCursor = contFirst;
    memset(subContacts, 0, numSSectors * sizeof(*subContacts));
}

boolean R_IterateSubsectorContacts(subsector_t* ssec, objtype_t type,
                                   boolean (*func) (void*, void*),
                                   void* data)
{
    objcontact_t*       con;

    con = subContacts[GET_SUBSECTOR_IDX(ssec)].head[type];
    while(con)
    {
        if(!func(con->obj, data))
            return false;

        con = con->next;
    }

    return true;
}
