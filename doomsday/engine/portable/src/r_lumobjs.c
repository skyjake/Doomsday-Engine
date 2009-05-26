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
#include "de_defs.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
  PROF_LUMOBJ_INIT_ADD,
  PROF_LUMOBJ_FRAME_SORT
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

typedef struct {
    float           color[3];
    float           size;
    float           flareSize;
    float           xOffset, yOffset;
} lightconfig_t;

typedef struct lumlistnode_s {
    struct lumlistnode_s* next;
    struct lumlistnode_s* nextUsed;
    void*           data;
} lumlistnode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean      iterateSubsectorLumObjs(subsector_t* ssec,
                                            boolean (*func) (void*, void*),
                                            void* data);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int useBias;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean loInited = false;
uint loMaxLumobjs = 0;

int loMaxRadius = 256; // Dynamic lights maximum radius.
float loRadiusFactor = 3;

int useMobjAutoLights = true; // Enable automaticaly calculated lights
                              // attached to mobjs.
byte rendInfoLums = false;
byte devDrawLums = false; // Display active lumobjs?

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static zblockset_t* luminousBlockSet = NULL;
static uint numLuminous = 0, maxLuminous = 0;
static lumobj_t** luminousList = NULL;
static float* luminousDist = NULL;
static byte* luminousClipped = NULL;
static uint* luminousOrder = NULL;

// List of unused and used list nodes, for linking lumobjs with subsectors.
static lumlistnode_t* listNodeFirst = NULL, *listNodeCursor = NULL;

// List of lumobjs for each subsector;
static lumlistnode_t** subLumObjList = NULL;

// CODE --------------------------------------------------------------------

/**
 * Registers the cvars and ccmds for lumobj management.
 */
void LO_Register(void)
{
    C_VAR_INT("rend-light-num", &loMaxLumobjs, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-light-radius-scale", &loRadiusFactor, 0, 0.1f, 10);
    C_VAR_INT("rend-light-radius-max", &loMaxRadius, 0, 64, 512);

    C_VAR_BYTE("rend-info-lums", &rendInfoLums, 0, 0, 1);
    C_VAR_BYTE("rend-dev-lums", &devDrawLums, CVF_NO_ARCHIVE, 0, 1);
}

static lumlistnode_t* allocListNode(void)
{
    lumlistnode_t*         ln;

    if(listNodeCursor == NULL)
    {
        ln = Z_Malloc(sizeof(*ln), PU_STATIC, 0);

        // Link to the list of list nodes.
        ln->nextUsed = listNodeFirst;
        listNodeFirst = ln;
    }
    else
    {
        ln = listNodeCursor;
        listNodeCursor = listNodeCursor->nextUsed;
    }

    ln->next = NULL;
    ln->data = NULL;

    return ln;
}

static void linkLumObjToSSec(lumobj_t* lum)
{
    lumlistnode_t*         ln = allocListNode();
    lumlistnode_t**        root;

    root = &subLumObjList[GET_SUBSECTOR_IDX(lum->subsector)];
    ln->next = *root;
    ln->data = lum;
    *root = ln;
}

static uint lumToIndex(const lumobj_t* lum)
{
    uint                i;

    for(i = 0; i < numLuminous; ++i)
        if(luminousList[i] == lum)
            return i;

    Con_Error("lumToIndex: Invalid lumobj.\n");
    return 0;
}

void LO_InitForMap(void)
{
    // First initialize the subsector links (root pointers).
    subLumObjList =
        Z_Calloc(sizeof(*subLumObjList) * numSSectors, PU_MAPSTATIC, 0);

    maxLuminous = 0;
    luminousBlockSet = NULL; // Will have already been free'd.
}

/**
 * Called once during engine shutdown by Rend_Reset(). Releases any system
 * resources acquired by the objlink + obj contact management subsystem.
 */
void LO_Clear(void)
{
    Z_BlockDestroy(luminousBlockSet);

    if(luminousList)
        M_Free(luminousList);
    luminousList = NULL;

    if(luminousDist)
        M_Free(luminousDist);
    luminousDist = NULL;

    if(luminousClipped)
        M_Free(luminousClipped);
    luminousClipped = NULL;

    if(luminousOrder)
        M_Free(luminousOrder);
    luminousOrder = NULL;

    maxLuminous = numLuminous = 0;
}

/**
 * Called at the begining of each frame (iff the render lists are not frozen)
 * by R_BeginWorldFrame().
 */
void LO_ClearForFrame(void)
{
#ifdef DD_PROFILE
    static int i;

    if(++i > 40)
    {
        i = 0;
        PRINT_PROF(PROF_LUMOBJ_INIT_ADD);
        PRINT_PROF(PROF_LUMOBJ_FRAME_SORT);
    }
#endif

    // Start reusing nodes from the first one in the list.
    listNodeCursor = listNodeFirst;
    if(subLumObjList)
        memset(subLumObjList, 0, sizeof(lumlistnode_t*) * numSSectors);
    numLuminous = 0;
}

/**
 * @return              The number of active lumobjs for this frame.
 */
uint LO_GetNumLuminous(void)
{
    return numLuminous;
}

static lumobj_t* allocLumobj(void)
{
#define LUMOBJ_BATCH_SIZE       (32)

    lumobj_t*           lum;

    // Only allocate memory when it's needed.
    // \fixme No upper limit?
    if(++numLuminous > maxLuminous)
    {
        uint                i, newMax = maxLuminous + LUMOBJ_BATCH_SIZE;

        if(!luminousBlockSet)
        {
            luminousBlockSet =
                Z_BlockCreate(sizeof(lumobj_t), LUMOBJ_BATCH_SIZE, PU_MAP);
        }

        luminousList =
            M_Realloc(luminousList, sizeof(lumobj_t*) * newMax);

        // Add the new lums to the end of the list.
        for(i = maxLuminous; i < newMax; ++i)
            luminousList[i] = Z_BlockNewElement(luminousBlockSet);

        maxLuminous = newMax;

        // Resize the associated buffers used for per-frame stuff.
        luminousDist =
            M_Realloc(luminousDist, sizeof(*luminousDist) * maxLuminous);
        luminousClipped =
            M_Realloc(luminousClipped, sizeof(*luminousClipped) * maxLuminous);
        luminousOrder =
            M_Realloc(luminousOrder, sizeof(*luminousOrder) * maxLuminous);
    }

    lum = luminousList[numLuminous - 1];
    memset(lum, 0, sizeof(*lum));

    return lum;

#undef LUMOBJ_BATCH_SIZE
}

/**
 * Allocate a new lumobj.
 *
 * @return              Index (name) by which the lumobj should be referred.
 */
uint LO_NewLuminous(lumtype_t type, subsector_t* ssec)
{
    lumobj_t*           lum = allocLumobj();

    lum->type = type;
    lum->subsector = ssec;

    linkLumObjToSSec(lum);

    R_ObjLinkCreate(lum, OT_LUMOBJ); // For spreading purposes.

    return numLuminous; // == index + 1
}

/**
 * Retrieve a ptr to the lumobj with the given index. A public interface to
 * the lumobj list.
 *
 * @return              Ptr to the lumobj with the given 1-based index.
 */
lumobj_t* LO_GetLuminous(uint idx)
{
    if(!(idx == 0 || idx > numLuminous))
        return luminousList[idx - 1];

    return NULL;
}

/**
 * @return              Index of the specified lumobj.
 */
uint LO_ToIndex(const lumobj_t* lum)
{
    return lumToIndex(lum)+1;
}

/**
 * Is the specified lumobj clipped for a given viewer?
 */
boolean LO_IsClipped(uint idx, int i)
{
    if(!(idx == 0 || idx > numLuminous))
        return (luminousClipped[idx - 1]? true : false);

    return false;
}

/**
 * Is the specified lumobj hidden for the given viewer?
 */
boolean LO_IsHidden(uint idx, int i)
{
    if(!(idx == 0 || idx > numLuminous))
        return (luminousClipped[idx - 1] == 2? true : false);

    return false;
}


/**
 * @return              Approximated distance between the lumobj and the
 *                      viewer.
 */
float LO_DistanceToViewer(uint idx, int i)
{
    if(!(idx == 0 || idx > numLuminous))
        return luminousDist[idx - 1];

    return 0;
}

/**
 * Registers the given mobj as a luminous, light-emitting object.
 * NOTE: This is called each frame for each luminous object!
 *
 * @param mo            Ptr to the mobj to register.
 */
void LO_AddLuminous(mobj_t* mo)
{
    mo->lumIdx = 0;

    if(((mo->state && (mo->state->flags & STF_FULLBRIGHT)) &&
         !(mo->ddFlags & DDMF_DONTDRAW)) ||
       (mo->ddFlags & DDMF_ALWAYSLIT))
    {
        uint                i;
        float               mul, xOff, center;
        int                 flags = 0;
        int                 radius, flareSize;
        float               rgb[3];
        lumobj_t*           l;
        lightconfig_t       cf;
        ded_light_t*        def = 0;
        spritedef_t*        sprDef;
        spriteframe_t*      sprFrame;
        material_t*         mat;
        rgbcol_t            autoLightColor;
        material_snapshot_t ms;
        const gltexture_inst_t* texInst;

        // Are the automatically calculated light values for fullbright
        // sprite frames in use?
        if(mo->state &&
           (!useMobjAutoLights || (mo->state->flags & STF_NOAUTOLIGHT)) &&
           !stateLights[mo->state - states])
           return;

        // Determine the sprite frame lump of the source.
        sprDef = &sprites[mo->sprite];
        sprFrame = &sprDef->spriteFrames[mo->frame];
        if(sprFrame->rotate)
        {
            mat =
                sprFrame->
                mats[(R_PointToAngle(mo->pos[VX], mo->pos[VY]) - mo->angle +
                      (unsigned) (ANG45 / 2) * 9) >> 29];
        }
        else
        {
            mat = sprFrame->mats[0];
        }

#if _DEBUG
if(!mat)
    Con_Error("LO_AddLuminous: Sprite '%i' frame '%i' missing material.",
              (int) mo->sprite, mo->frame);
#endif

        // Ensure we have up-to-date information about the material.
        Material_Prepare(&ms, mat, true, NULL);

        if(ms.units[MTU_PRIMARY].texInst->tex->type != GLT_SPRITE)
            return; // *Very* strange...

        texInst = ms.units[MTU_PRIMARY].texInst;

        cf.size = cf.flareSize = texInst->data.sprite.lumSize;
        cf.xOffset = texInst->data.sprite.flareX;
        cf.yOffset = texInst->data.sprite.flareY;
        autoLightColor[CR] = texInst->data.sprite.autoLightColor[CR];
        autoLightColor[CG] = texInst->data.sprite.autoLightColor[CG];
        autoLightColor[CB] = texInst->data.sprite.autoLightColor[CB];

        // X offset to the flare position.
        xOff = (cf.xOffset - (float) ms.width / 2) -
            (spriteTextures[texInst->tex->ofTypeID]->offX -
                (float) ms.width / 2);

        // Does the mobj have an active light definition?
        if(mo->state)
        {
            def = stateLights[mo->state - states];

            if(def)
            {
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
        }

        center = spriteTextures[texInst->tex->ofTypeID]->offY -
            mo->floorClip - R_GetBobOffset(mo) - cf.yOffset;

        // Will the sprite be allowed to go inside the floor?
        mul = mo->pos[VZ] + spriteTextures[texInst->tex->ofTypeID]->offY -
            (float) ms.height - mo->subsector->sector->SP_floorheight;
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

        // If any of the color components are != 0, use the def's color.
        if(def && (def->color[0] || def->color[1] || def->color[2]))
        {
            for(i = 0; i < 3; ++i)
                rgb[i] = def->color[i];
        }
        else
        {   // Use the auto-calculated color.
            for(i = 0; i < 3; ++i)
                rgb[i] = autoLightColor[i];
        }

        // This'll allow a halo to be rendered. If the light is hidden from
        // view by world geometry, the light pointer will be set to NULL.
        mo->lumIdx = LO_NewLuminous(LT_OMNI, mo->subsector);

        l = LO_GetLuminous(mo->lumIdx);
        l->pos[VX] = mo->pos[VX];
        l->pos[VY] = mo->pos[VY];
        l->pos[VZ] = mo->pos[VZ];
        l->maxDistance = 0;
        l->decorSource = NULL;

        LUM_OMNI(l)->flags = flags;
        LUM_OMNI(l)->haloFactors = &mo->haloFactors[0];
        LUM_OMNI(l)->zOff = center;
        LUM_OMNI(l)->xOff = xOff;

        // Don't make too large a light.
        if(radius > loMaxRadius)
            radius = loMaxRadius;

        LUM_OMNI(l)->radius = radius;
        LUM_OMNI(l)->flareMul = 1;
        LUM_OMNI(l)->flareSize = flareSize;
        for(i = 0; i < 3; ++i)
            LUM_OMNI(l)->color[i] = rgb[i];

        if(def)
        {
            LUM_OMNI(l)->tex = GL_GetLightMapTexture(def->sides.id);
            LUM_OMNI(l)->ceilTex = GL_GetLightMapTexture(def->up.id);
            LUM_OMNI(l)->floorTex = GL_GetLightMapTexture(def->down.id);

            if(def->flare.disabled)
                LUM_OMNI(l)->flags |= LUMOF_NOHALO;
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
                LUM_OMNI(l)->floorTex = GL_PrepareLSTexture(LST_DYNAMIC);
        }
    }
}

/**
 * Used to sort lumobjs by distance from viewpoint.
 */
static int C_DECL lumobjSorter(const void* e1, const void* e2)
{
    float               a = luminousDist[*(const uint *) e1];
    float               b = luminousDist[*(const uint *) e2];

    if(a > b)
        return 1;
    else if(a < b)
        return -1;
    else
        return 0;
}

/**
 * Called by Rend_RenderMap() if the render lists are not frozen.
 */
void LO_BeginFrame(void)
{
    uint                i;

    if(!(numLuminous > 0))
        return;

BEGIN_PROF( PROF_LUMOBJ_FRAME_SORT );

    // Update lumobj distances ready for linking and sorting.
    for(i = 0; i < numLuminous; ++i)
    {
        lumobj_t*           lum = luminousList[i];

        // Approximate the distance in 3D.
        luminousDist[i] = P_ApproxDistance3(lum->pos[VX] - viewX,
                                            lum->pos[VY] - viewY,
                                            lum->pos[VZ] - viewZ);
    }

    if(loMaxLumobjs > 0 && numLuminous > loMaxLumobjs)
    {   // Sort lumobjs by distance from the viewer. Then clip all lumobjs
        // so that only the closest are visible (max loMaxLumobjs).
        uint                n;

        // Init the lumobj indices, sort array.
        for(i = 0; i < numLuminous; ++i)
            luminousOrder[i] = i;

        qsort(luminousOrder, numLuminous, sizeof(uint), lumobjSorter);

        // Mark all as hidden.
        memset(luminousClipped, 2, numLuminous * sizeof(*luminousClipped));

        n = 0;
        for(i = 0; i < numLuminous; ++i)
        {
            if(n++ > loMaxLumobjs)
                break;

            // Unhide this lumobj.
            luminousClipped[luminousOrder[i]] = 1;
        }
    }
    else
    {
        // Mark all as clipped.
        memset(luminousClipped, 1, numLuminous * sizeof(*luminousClipped));
    }

    // objLinks already contains links if there are any light decorations
    // currently in use.
    loInited = true;

END_PROF( PROF_LUMOBJ_FRAME_SORT );
}

/**
 * Generate one dynlight node for each plane glow.
 * The light is attached to the appropriate dynlight node list.
 *
 * @param ssec          Ptr to the subsector to process.
 */
static void createGlowLightPerPlaneForSubSector(subsector_t* ssec)
{
    uint                g;
    plane_t*            glowPlanes[2], *pln;

    glowPlanes[PLN_FLOOR] = ssec->sector->planes[PLN_FLOOR];
    glowPlanes[PLN_CEILING] = ssec->sector->planes[PLN_CEILING];

    //// \fixme $nplanes
    for(g = 0; g < 2; ++g)
    {
        uint                lumIdx;
        lumobj_t*           l;

        pln = glowPlanes[g];

        if(pln->glow <= 0)
            continue;

        lumIdx = LO_NewLuminous(LT_PLANE, ssec);

        l = LO_GetLuminous(lumIdx);
        l->pos[VX] = ssec->midPoint.pos[VX];
        l->pos[VY] = ssec->midPoint.pos[VY];
        l->pos[VZ] = pln->visHeight;
        l->maxDistance = 0;
        l->decorSource = NULL;

        LUM_PLANE(l)->normal[VX] = pln->PS_normal[VX];
        LUM_PLANE(l)->normal[VY] = pln->PS_normal[VY];
        LUM_PLANE(l)->normal[VZ] = pln->PS_normal[VZ];

        LUM_PLANE(l)->color[CR] = pln->glowRGB[CR];
        LUM_PLANE(l)->color[CG] = pln->glowRGB[CG];
        LUM_PLANE(l)->color[CB] = pln->glowRGB[CB];

        LUM_PLANE(l)->intensity = pln->glow;
        LUM_PLANE(l)->tex = GL_PrepareLSTexture(LST_GRADIENT);

        // Planar lights don't spread, so just link the lum to its own ssec.
        {
        linkobjtossecparams_t params;

        params.obj = l;
        params.type = OT_LUMOBJ;
        RIT_LinkObjToSubSector(l->subsector, &params);
        }
    }
}

/**
 * Create lumobjs for all sector-linked mobjs who want them.
 */
void LO_AddLuminousMobjs(void)
{
    uint                i;
    sector_t*           seciter;

    if(!useDynLights && !useWallGlow)
        return;

BEGIN_PROF( PROF_LUMOBJ_INIT_ADD );

    for(i = 0, seciter = sectors; i < numSectors; seciter++, ++i)
    {
        if(useDynLights)
        {
            mobj_t*             iter;

            for(iter = seciter->mobjList; iter; iter = iter->sNext)
            {
                LO_AddLuminous(iter);
            }
        }

        // If the segs of this subsector are affected by glowing planes we need
        // to create dynlights and link them.
        if(useWallGlow && seciter->ssectors)
        {
            subsector_t**       ssec = seciter->ssectors;

            while(*ssec)
            {
                createGlowLightPerPlaneForSubSector(*ssec);
                *ssec++;
            }
        }
    }

END_PROF( PROF_LUMOBJ_INIT_ADD );
}

typedef struct lumobjiterparams_s {
    float           origin[2];
    float           radius;
    void*           data;
    boolean       (*func) (const lumobj_t*, float, void* data);
} lumobjiterparams_t;

boolean LOIT_RadiusLumobjs(void* ptr, void* data)
{
    const lumobj_t*  lum = (const lumobj_t*) ptr;
    lumobjiterparams_t* params = data;
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
boolean LO_LumobjsRadiusIterator(subsector_t* ssec, float x, float y,
                                 float radius, void* data,
                                 boolean (*func) (const lumobj_t*, float, void*))
{
    lumobjiterparams_t  params;

    if(!ssec)
        return true;

    params.origin[VX] = x;
    params.origin[VY] = y;
    params.radius = radius;
    params.func = func;
    params.data = data;

    return R_IterateSubsectorContacts(ssec, OT_LUMOBJ, LOIT_RadiusLumobjs,
                                      (void*) &params);
}

boolean LOIT_ClipLumObj(void* data, void* context)
{
    lumobj_t*           lum = (lumobj_t*) data;
    uint                lumIdx = lumToIndex(lum);

    if(lum->type != LT_OMNI)
        return true; // Only interested in omnilights.

    if(luminousClipped[lumIdx] > 1)
        return true; // Already hidden by some other means.

    luminousClipped[lumIdx] = 0;

    // \fixme Determine the exact centerpoint of the light in
    // LO_AddLuminous!
    if(!C_IsPointVisible(lum->pos[VX], lum->pos[VY],
                         lum->pos[VZ] + LUM_OMNI(lum)->zOff))
        luminousClipped[lumIdx] = 1; // Won't have a halo.

    return true; // Continue iteration.
}

/**
 * Clip lumobj, omni lights in the given subsector.
 *
 * @param ssecidx       Subsector index in which lights will be clipped.
 */
void LO_ClipInSubsector(uint ssecidx)
{
    iterateSubsectorLumObjs(&ssectors[ssecidx], LOIT_ClipLumObj, NULL);
}

boolean LOIT_ClipLumObjBySight(void* data, void* context)
{
    lumobj_t*           lum = (lumobj_t*) data;
    uint                lumIdx = lumToIndex(lum);
    subsector_t*        ssec = (subsector_t*) context;

    if(lum->type != LT_OMNI)
        return true; // Only interested in omnilights.

    if(!luminousClipped[lumIdx])
    {
        uint                i;
        vec2_t              eye;

        V2_Set(eye, vx, vz);

        // We need to figure out if any of the polyobj's segments lies
        // between the viewpoint and the lumobj.
        for(i = 0; i < ssec->polyObj->numSegs; ++i)
        {
            seg_t*              seg = ssec->polyObj->segs[i];

            // Ignore segs facing the wrong way.
            if(seg->frameFlags & SEGINF_FACINGFRONT)
            {
                vec2_t              source;

                V2_Set(source, lum->pos[VX], lum->pos[VY]);

                if(V2_Intercept2(source, eye, seg->SG_v1pos,
                                 seg->SG_v2pos, NULL, NULL, NULL))
                {
                    luminousClipped[lumIdx] = 1;
                    break;
                }
            }
        }
    }

    return true; // Continue iteration.
}

/**
 * In the situation where a subsector contains both lumobjs and a polyobj,
 * the lumobjs must be clipped more carefully. Here we check if the line of
 * sight intersects any of the polyobj segs that face the camera.
 *
 * @param ssecidx       Subsector index in which lumobjs will be clipped.
 */
void LO_ClipInSubsectorBySight(uint ssecidx)
{
    iterateSubsectorLumObjs(&ssectors[ssecidx], LOIT_ClipLumObjBySight,
                              &ssectors[ssecidx]);
}

static boolean iterateSubsectorLumObjs(subsector_t* ssec,
                                       boolean (*func) (void*, void*),
                                       void* data)
{
    lumlistnode_t*      ln;

    ln = subLumObjList[GET_SUBSECTOR_IDX(ssec)];
    while(ln)
    {
        if(!func(ln->data, data))
            return false;

        ln = ln->next;
    }

    return true;
}

void LO_UnlinkMobjLumobj(mobj_t* mo)
{
    mo->lumIdx = 0;
}

boolean LOIT_UnlinkMobjLumobj(thinker_t* th, void* context)
{
    LO_UnlinkMobjLumobj((mobj_t*) th);

    return true; // Continue iteration.
}

void LO_UnlinkMobjLumobjs(cvar_t* var)
{
    if(!useDynLights)
    {
        // Mobjs are always public.
        P_IterateThinkers(gx.MobjThinker, 0x1, LOIT_UnlinkMobjLumobj, NULL);
    }
}

void LO_DrawLumobjs(void)
{
    static const float  black[4] = { 0, 0, 0, 0 };
    static const float  white[4] = { 1, 1, 1, 1 };
    float               color[4];
    uint                i;

    if(!devDrawLums)
        return;

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    for(i = 0; i < numLuminous; ++i)
    {
        lumobj_t*           lum = luminousList[i];
        vec3_t              lumCenter;

        if(!(lum->type == LT_OMNI || lum->type == LT_PLANE))
            continue;

        if(lum->type == LT_OMNI && loMaxLumobjs > 0 && luminousClipped[i] == 2)
            continue;

        V3_Copy(lumCenter, lum->pos);
        if(lum->type == LT_OMNI)
            lumCenter[VZ] += LUM_OMNI(lum)->zOff;

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(lumCenter[VX], lumCenter[VZ], lumCenter[VY]);

        switch(lum->type)
        {
        case LT_OMNI:
            {
            float               scale = LUM_OMNI(lum)->radius;

            color[CR] = LUM_OMNI(lum)->color[CR];
            color[CG] = LUM_OMNI(lum)->color[CG];
            color[CB] = LUM_OMNI(lum)->color[CB];
            color[CA] = 1;

            glBegin(GL_LINES);
            {
                glColor4fv(black);
                glVertex3f(-scale, 0, 0);
                glColor4fv(color);
                glVertex3f(0, 0, 0);
                glVertex3f(0, 0, 0);
                glColor4fv(black);
                glVertex3f(scale, 0, 0);

                glVertex3f(0, -scale, 0);
                glColor4fv(color);
                glVertex3f(0, 0, 0);
                glVertex3f(0, 0, 0);
                glColor4fv(black);
                glVertex3f(0, scale, 0);

                glVertex3f(0, 0, -scale);
                glColor4fv(color);
                glVertex3f(0, 0, 0);
                glVertex3f(0, 0, 0);
                glColor4fv(black);
                glVertex3f(0, 0, scale);
            }
            glEnd();
            break;
            }

        case LT_PLANE:
            {
            float               scale = LUM_PLANE(lum)->intensity * 10;

            color[CR] = LUM_PLANE(lum)->color[CR];
            color[CG] = LUM_PLANE(lum)->color[CG];
            color[CB] = LUM_PLANE(lum)->color[CB];
            color[CA] = 1;

            glBegin(GL_LINES);
            {
                glColor4fv(black);
                glVertex3f(scale * LUM_PLANE(lum)->normal[VX],
                             scale * LUM_PLANE(lum)->normal[VZ],
                             scale * LUM_PLANE(lum)->normal[VY]);
                glColor4fv(color);
                glVertex3f(0, 0, 0);

            }
            glEnd();
            break;
            }

        default:
            break;
        }

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
}
