/**\file r_lumobjs.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <stdio.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_defs.h"

#include "sys_opengl.h"
#include "texture.h"
#include "texturevariant.h"
#include "materialvariant.h"

BEGIN_PROF_TIMERS()
  PROF_LUMOBJ_INIT_ADD,
  PROF_LUMOBJ_FRAME_SORT
END_PROF_TIMERS()

typedef struct lumlistnode_s {
    struct lumlistnode_s* next;
    struct lumlistnode_s* nextUsed;
    void* data;
} lumlistnode_t;
typedef struct listnode_s {
    struct listnode_s* next, *nextUsed;
    dynlight_t projection;
} listnode_t;

/**
 * @defgroup lightProjectionListFlags  Light Projection List Flags
 * @{
 */
#define SPLF_SORT_LUMINOUS_DESC  0x1 /// Sort by luminosity in descending order.
/**@}*/

typedef struct {
    int flags; /// @see lightProjectionListFlags
    listnode_t* head;
} lightprojectionlist_t;

/// Orientation is toward the projectee.
typedef struct {
    int flags; /// @see lightProjectFlags
    float blendFactor; /// Multiplied with projection alpha.
    pvec3f_t v1; /// Top left vertex of the surface being projected to.
    pvec3f_t v2; /// Bottom right vertex of the surface being projected to.
    pvec3f_t tangent; /// Normalized tangent of the surface being projected to.
    pvec3f_t bitangent; /// Normalized bitangent of the surface being projected to.
    pvec3f_t normal; /// Normalized normal of the surface being projected to.
} lightprojectparams_t;

static boolean iterateBspLeafLumObjs(BspLeaf* bspLeaf, boolean (*func) (void*, void*), void* data);

extern int useBias;

boolean loInited = false;
uint loMaxLumobjs = 0;

int loMaxRadius = 256; // Dynamic lights maximum radius.
float loRadiusFactor = 3;

int useMobjAutoLights = true; // Enable automaticaly calculated lights
                              // attached to mobjs.
byte rendInfoLums = false;
byte devDrawLums = false; // Display active lumobjs?

static zblockset_t* luminousBlockSet = NULL;
static uint numLuminous = 0, maxLuminous = 0;
static lumobj_t** luminousList = NULL;
static float* luminousDist = NULL;
static byte* luminousClipped = NULL;
static uint* luminousOrder = NULL;

// List of unused and used list nodes, for linking lumobjs with BSP leafs.
static lumlistnode_t* listNodeFirst = NULL, *listNodeCursor = NULL;

// List of lumobjs for each BSP leaf;
static lumlistnode_t** bspLeafLumObjList = NULL;

// Projection list nodes.
static listnode_t* firstNode, *cursorNode;

// Light projection (dynlight) lists.
static uint projectionListCount, cursorList;
static lightprojectionlist_t* projectionLists;

void LO_Register(void)
{
    C_VAR_INT("rend-mobj-light-auto", &useMobjAutoLights, 0, 0, 1);
    C_VAR_INT("rend-light-num", &loMaxLumobjs, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-light-radius-scale", &loRadiusFactor, 0, 0.1f, 10);
    C_VAR_INT("rend-light-radius-max", &loMaxRadius, 0, 64, 512);

    C_VAR_BYTE("rend-info-lums", &rendInfoLums, 0, 0, 1);
    C_VAR_BYTE("rend-dev-lums", &devDrawLums, CVF_NO_ARCHIVE, 0, 1);
}

static lumlistnode_t* allocListNode(void)
{
    lumlistnode_t* ln;

    if(listNodeCursor == NULL)
    {
        ln = Z_Malloc(sizeof(*ln), PU_APPSTATIC, 0);

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

static void linkLumObjToSSec(lumobj_t* lum, BspLeaf* bspLeaf)
{
    lumlistnode_t* ln = allocListNode();
    lumlistnode_t** root;

    root = &bspLeafLumObjList[GET_BSPLEAF_IDX(bspLeaf)];
    ln->next = *root;
    ln->data = lum;
    *root = ln;
}

static uint lumToIndex(const lumobj_t* lum)
{
    uint i;
    for(i = 0; i < numLuminous; ++i)
        if(luminousList[i] == lum)
            return i;
    Con_Error("lumToIndex: Invalid lumobj.\n");
    return 0;
}

static void initProjectionLists(void)
{
    static boolean firstTime = true;
    if(firstTime)
    {
        firstNode = NULL;
        cursorNode = NULL;
        firstTime = false;
    }
    // All memory for the lists is allocated from Zone so we can "forget" it.
    projectionLists = NULL;
    projectionListCount = 0;
    cursorList = 0;
}

static void clearProjectionLists(void)
{
    // Start reusing nodes from the first one in the list.
    cursorNode = firstNode;

    // Clear the lists.
    cursorList = 0;
    if(projectionListCount)
    {
        memset(projectionLists, 0, projectionListCount * sizeof *projectionLists);
    }
}

/**
 * Create a new projection list.
 *
 * @param flags  @see lightProjectionListFlags
 * @return  Unique identifier attributed to the new list.
 */
static uint newProjectionList(int flags)
{
    lightprojectionlist_t* list;

    // Do we need to allocate more lists?
    if(++cursorList >= projectionListCount)
    {
        projectionListCount *= 2;
        if(!projectionListCount) projectionListCount = 2;

        projectionLists = (lightprojectionlist_t*)Z_Realloc(projectionLists, projectionListCount * sizeof *projectionLists, PU_MAP);
        if(!projectionLists) Con_Error(__FILE__":newProjectionList failed on allocation of %lu bytes resizing the projection list.", (unsigned long) (projectionListCount * sizeof *projectionLists));
    }

    list = &projectionLists[cursorList-1];
    list->head = NULL;
    list->flags = flags;

    return cursorList;
}

/**
 * @param listIdx  Address holding the list index to retrieve.
 *      If the referenced list index is non-zero return the associated list.
 *      Otherwise allocate a new list and write it's index back to this address.
 * @param flags  @see ProjectionListFlags
 * @return  ProjectionList associated with the (possibly newly attributed) index.
 */
static lightprojectionlist_t* getProjectionList(uint* listIdx, int flags)
{
    // Do we need to allocate a list?
    if(!(*listIdx))
    {
        *listIdx = newProjectionList(flags);
    }
    return projectionLists + ((*listIdx)-1); // 1-based index.
}

static listnode_t* newListNode(void)
{
    listnode_t* node;

    // Do we need to allocate mode nodes?
    if(cursorNode == NULL)
    {
        node = (listnode_t*)Z_Malloc(sizeof *node, PU_APPSTATIC, NULL);
        if(!node) Con_Error(__FILE__":newListNode failed on allocation of %lu bytes for new node.", (unsigned long) sizeof *node);

        // Link the new node to the list.
        node->nextUsed = firstNode;
        firstNode = node;
    }
    else
    {
        node = cursorNode;
        cursorNode = cursorNode->nextUsed;
    }

    node->next = NULL;
    return node;
}

static listnode_t* newProjection(DGLuint texture, const float s[2],
    const float t[2], const float color[3], float alpha)
{
    assert(texture != 0 && s && t && color);
    {
    listnode_t* node = newListNode();
    dynlight_t* tp = &node->projection;

    tp->texture = texture;
    tp->s[0] = s[0];
    tp->s[1] = s[1];
    tp->t[0] = t[0];
    tp->t[1] = t[1];
    tp->color.rgba[CR] = color[CR];
    tp->color.rgba[CG] = color[CG];
    tp->color.rgba[CB] = color[CB];
    tp->color.rgba[CA] = MINMAX_OF(0, alpha, 1);

    return node;
    }
}

static __inline float calcProjectionLuminosity(dynlight_t* tp)
{
    assert(tp);
    return ColorRawf_AverageColorMulAlpha(&tp->color);
}

/// @return  Same as @a node for convenience (chaining).
static listnode_t* linkProjectionToList(listnode_t* node, lightprojectionlist_t* list)
{
    assert(node && list);
    if((list->flags & SPLF_SORT_LUMINOUS_DESC) && list->head)
    {
        float luma = calcProjectionLuminosity(&node->projection);
        listnode_t* iter = list->head, *last = iter;
        do
        {
            // Is this brighter than that being added?
            if(calcProjectionLuminosity(&iter->projection) > luma)
            {
                last = iter;
                iter = iter->next;
            }
            else
            {
                // Insert it here.
                node->next = last->next;
                last->next = node;
                return node;
            }
        } while(iter);
    }

    node->next = list->head;
    list->head = node;
    return node;
}

/**
 * Construct a new surface projection (and a list, if one has not already been
 * constructed for the referenced index).
 *
 * @param listIdx  Address holding the list index to retrieve.
 *      If the referenced list index is non-zero return the associated list.
 *      Otherwise allocate a new list and write it's index back to this address.
 * @param flags  @see ProjectionListFlags
 *      Used when constructing a new projection list to configure it.
 * @param texture  GL identifier to texture attributed to the new projection.
 * @param s  GL texture coordinates on the S axis [left, right] in texture space.
 * @param t  GL texture coordinates on the T axis [bottom, top] in texture space.
 * @param colorRGB  RGB color attributed to the new projection.
 * @param alpha  Alpha attributed to the new projection.
 */
static void newLightProjection(uint* listIdx, int flags, DGLuint texture,
    const float s[2], const float t[2], const float colorRGB[3], float alpha)
{
    linkProjectionToList(newProjection(texture, s, t, colorRGB, alpha), getProjectionList(listIdx, flags));
}

/**
 * Blend the given light value with the lumobj's color, apply any global
 * modifiers and output the result.
 *
 * @param outRGB  Calculated result will be written here.
 * @param color  Lumobj color.
 * @param light  Ambient light level of the surface being projected to.
 */
static void calcLightColor(float outRGB[3], const float color[3], float light)
{
    int i;

    light = MINMAX_OF(0, light, 1) * dynlightFactor;
    // In fog additive blending is used; the normal fog color is way too bright.
    if(usingFog) light *= dynlightFogBright;

    // Multiply light with (ambient) color.
    for(i = 0; i < 3; ++i)
    {
        outRGB[i] = light * color[i];
    }
}

typedef struct {
    uint listIdx;
    lightprojectparams_t spParams;
} projectlighttosurfaceiteratorparams_t;

/**
 * Project a plane glow onto the surface. If valid and the surface is
 * contacted a new projection node will constructed and returned.
 *
 * @param lum  Lumobj representing the light being projected.
 * @param paramaters  ProjectLightToSurfaceIterator paramaters.
 *
 * @return  @c 0 = continue iteration.
 */
static int projectPlaneLightToSurface(const lumobj_t* lum, void* paramaters)
{
    assert(lum && paramaters);
    {
    projectlighttosurfaceiteratorparams_t* p = (projectlighttosurfaceiteratorparams_t*)paramaters;
    lightprojectparams_t* spParams = &p->spParams;
    float bottom = spParams->v2[VZ], top = spParams->v1[VZ];
    float glowHeight, s[2], t[2], color[3];

    if(spParams->flags & PLF_NO_PLANE) return 0; // Continue iteration.

    // No lightmap texture?
    if(!LUM_PLANE(lum)->tex) return 0; // Continue iteration.

    // No height?
    if(bottom >= top) return 0; // Continue iteration.

    // Do not make too small glows.
    glowHeight = (GLOW_HEIGHT_MAX * LUM_PLANE(lum)->intensity) * glowHeightFactor;
    if(glowHeight <= 2) return 0; // Continue iteration.

    if(glowHeight > glowHeightMax)
        glowHeight = glowHeightMax;

    // Calculate texture coords for the light.
    if(LUM_PLANE(lum)->normal[VZ] < 0)
    {
        // Light is cast downwards.
        t[1] = t[0] = (lum->pos[VZ] - top) / glowHeight;
        t[1]+= (top - bottom) / glowHeight;
    }
    else
    {
        // Light is cast upwards.
        t[0] = t[1] = (bottom - lum->pos[VZ]) / glowHeight;
        t[0]+= (top - bottom) / glowHeight;
    }

    // Above/below on the Y axis?
    if(!(t[0] <= 1 || t[1] >= 0)) return 0; // Continue iteration.

    // The horizontal direction is easy.
    s[0] = 0;
    s[1] = 1;

    calcLightColor(color, LUM_PLANE(lum)->color, LUM_PLANE(lum)->intensity);

    newLightProjection(&p->listIdx, ((spParams->flags & PLF_SORT_LUMINOSITY_DESC)? SPLF_SORT_LUMINOUS_DESC : 0),
        LUM_PLANE(lum)->tex, s, t, color, 1 * spParams->blendFactor);

    return 0; // Continue iteration.
    }
}

static boolean genTexCoords(pvec2f_t s, pvec2f_t t, const_pvec3f_t point, float scale,
    const_pvec3f_t v1, const_pvec3f_t v2, const_pvec3f_t tangent, const_pvec3f_t bitangent)
{
    // Counteract aspect correction slightly (not too round mind).
    return R_GenerateTexCoords(s, t, point, scale, scale * 1.08f, v1, v2, tangent, bitangent);
}

static DGLuint chooseOmniLightTexture(lumobj_t* lum, const lightprojectparams_t* spParams)
{
    assert(lum && lum->type == LT_OMNI && spParams);
    if(spParams->flags & PLF_TEX_CEILING)
        return LUM_OMNI(lum)->ceilTex;
    if(spParams->flags & PLF_TEX_FLOOR)
        return LUM_OMNI(lum)->floorTex;
    return LUM_OMNI(lum)->tex;
}

/**
 * Project a omni light onto the surface. If valid and the surface is
 * contacted a new projection node will constructed and returned.
 *
 * @param lum  Lumobj representing the light being projected.
 * @param paramaters  ProjectLightToSurfaceIterator paramaters.
 *
 * @return  @c 0 = continue iteration.
 */
static int projectOmniLightToSurface(lumobj_t* lum, void* paramaters)
{
    assert(lum && paramaters);
    {
    projectlighttosurfaceiteratorparams_t* p = (projectlighttosurfaceiteratorparams_t*)paramaters;
    lightprojectparams_t* spParams = &p->spParams;
    float dist, luma, scale, color[3];
    vec3f_t lumCenter, vToLum;
    vec3f_t point;
    uint lumIdx;
    DGLuint tex;
    vec2f_t s, t;

    // Early test of the external blend factor for quick rejection.
    if(spParams->blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN) return false; // Continue iteration.

    // No lightmap texture?
    tex = chooseOmniLightTexture(lum, spParams);
    if(!tex) return false; // Continue iteration.

    // Has this already been occluded?
    lumIdx = LO_ToIndex(lum);
    if(LO_IsHidden(lumIdx, viewPlayer - ddPlayers)) return false; // Continue iteration.

    V3f_Set(lumCenter, lum->pos[VX], lum->pos[VY], lum->pos[VZ] + LUM_OMNI(lum)->zOff);
    V3f_Subtract(vToLum, spParams->v1, lumCenter);

    // On the right side?
    if(V3f_DotProduct(vToLum, spParams->normal) > 0.f) return false; // Continue iteration.

    // Calculate 3D distance between surface and lumobj.
    V3f_ClosestPointOnPlane(point, spParams->normal, spParams->v1, lumCenter);
    dist = V3f_Distance(point, lumCenter);
    if(dist <= 0 || dist > LUM_OMNI(lum)->radius) return false; // Continue iteration.

    // Calculate the final surface light attribution factor.
    luma = 1.5f - 1.5f * dist / LUM_OMNI(lum)->radius;

    // If distance limit is set this light will fade out.
    if(lum->maxDistance > 0)
    {
        float distance = LO_DistanceToViewer(lumIdx, viewPlayer - ddPlayers);
        luma *= LO_AttenuationFactor(lumIdx, distance);
    }

    // Would this light be seen?
    if(luma * spParams->blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN) return false; // Continue iteration.

    // Project this light.
    scale = 1.0f / ((2.f * LUM_OMNI(lum)->radius) - dist);
    if(!genTexCoords(s, t, point, scale, spParams->v1, spParams->v2, spParams->tangent, spParams->bitangent)) return false; // Continue iteration.

    // Attach to the projection list.
    calcLightColor(color, LUM_OMNI(lum)->color, luma);
    newLightProjection(&p->listIdx, ((spParams->flags & PLF_SORT_LUMINOSITY_DESC)? SPLF_SORT_LUMINOUS_DESC : 0),
        tex, s, t, color, 1 * spParams->blendFactor);

    return false; // Continue iteration.
    }
}

void LO_InitForMap(void)
{
    // First initialize the BSP leaf links (root pointers).
    bspLeafLumObjList = Z_Calloc(sizeof(*bspLeafLumObjList) * NUM_BSPLEAFS, PU_MAPSTATIC, 0);

    maxLuminous = 0;
    luminousBlockSet = 0; // Will have already been free'd.

    initProjectionLists();
}

void LO_Clear(void)
{
    if(luminousBlockSet)
        ZBlockSet_Delete(luminousBlockSet);
    luminousBlockSet = 0;

    if(luminousList)
        M_Free(luminousList);
    luminousList = 0;

    if(luminousDist)
        M_Free(luminousDist);
    luminousDist = 0;

    if(luminousClipped)
        M_Free(luminousClipped);
    luminousClipped = 0;

    if(luminousOrder)
        M_Free(luminousOrder);
    luminousOrder = 0;

    maxLuminous = numLuminous = 0;
}

void LO_BeginWorldFrame(void)
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
    if(bspLeafLumObjList)
        memset(bspLeafLumObjList, 0, sizeof(lumlistnode_t*) * NUM_BSPLEAFS);
    numLuminous = 0;
}

uint LO_GetNumLuminous(void)
{
    return numLuminous;
}

static lumobj_t* allocLumobj(void)
{
#define LUMOBJ_BATCH_SIZE       (32)

    lumobj_t* lum;

    // Only allocate memory when it's needed.
    // \fixme No upper limit?
    if(++numLuminous > maxLuminous)
    {
        uint i, newMax = maxLuminous + LUMOBJ_BATCH_SIZE;

        if(!luminousBlockSet)
        {
            luminousBlockSet = ZBlockSet_New(sizeof(lumobj_t), LUMOBJ_BATCH_SIZE, PU_MAP);
        }

        luminousList = M_Realloc(luminousList, sizeof(lumobj_t*) * newMax);

        // Add the new lums to the end of the list.
        for(i = maxLuminous; i < newMax; ++i)
            luminousList[i] = ZBlockSet_Allocate(luminousBlockSet);

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

static lumobj_t* createLuminous(lumtype_t type, BspLeaf* bspLeaf)
{
    lumobj_t* lum = allocLumobj();

    lum->type = type;
    lum->bspLeaf = bspLeaf;
    linkLumObjToSSec(lum, bspLeaf);

    if(type != LT_PLANE)
        R_ObjlinkCreate(lum, OT_LUMOBJ); // For spreading purposes.

    return lum;
}

uint LO_NewLuminous(lumtype_t type, BspLeaf* bspLeaf)
{
    createLuminous(type, bspLeaf);
    return numLuminous; // == index + 1
}

lumobj_t* LO_GetLuminous(uint idx)
{
    if(!(idx == 0 || idx > numLuminous))
        return luminousList[idx - 1];
    return NULL;
}

uint LO_ToIndex(const lumobj_t* lum)
{
    return lumToIndex(lum)+1;
}

boolean LO_IsClipped(uint idx, int i)
{
    if(!(idx == 0 || idx > numLuminous))
        return (luminousClipped[idx - 1]? true : false);
    return false;
}

boolean LO_IsHidden(uint idx, int i)
{
    if(!(idx == 0 || idx > numLuminous))
        return (luminousClipped[idx - 1] == 2? true : false);
    return false;
}

float LO_DistanceToViewer(uint idx, int i)
{
    if(!(idx == 0 || idx > numLuminous))
        return luminousDist[idx - 1];
    return 0;
}

float LO_AttenuationFactor(uint idx, float distance)
{
    lumobj_t* lum = LO_GetLuminous(idx);
    if(lum)
    switch(lum->type)
    {
    case LT_OMNI:
        if(distance <= 0) return 1;
        if(distance > lum->maxDistance) return 0;
        if(distance > .67f * lum->maxDistance)
            return (lum->maxDistance - distance) / (.33f * lum->maxDistance);
        break;
    case LT_PLANE: break;
    default:
        Con_Error("LO_AttenuationFactor: Invalid lumobj type %i.", (int)lum->type);
        exit(1); // Unreachable.
    }
    return 1;
}

/**
 * Registers the given mobj as a luminous, light-emitting object.
 * \note: This is called each frame for each luminous object!
 *
 * @param mo  Ptr to the mobj to register.
 */
static void addLuminous(mobj_t* mo)
{
    uint i;
    float mul, center;
    int radius;
    float rgb[3], yOffset, size;
    lumobj_t* l;
    ded_light_t* def;
    spritedef_t* sprDef;
    spriteframe_t* sprFrame;
    patchtex_t* pTex;
    material_t* mat;
    const materialsnapshot_t* ms;
    const materialvariantspecification_t* spec;
    const pointlight_analysis_t* pl;

    if(!(((mo->state && (mo->state->flags & STF_FULLBRIGHT)) &&
         !(mo->ddFlags & DDMF_DONTDRAW)) ||
       (mo->ddFlags & DDMF_ALWAYSLIT)))
        return;

    // Are the automatically calculated light values for fullbright
    // sprite frames in use?
    if(mo->state &&
       (!useMobjAutoLights || (mo->state->flags & STF_NOAUTOLIGHT)) &&
       !stateLights[mo->state - states])
       return;

    def = (mo->state? stateLights[mo->state - states] : NULL);

    // Determine the sprite frame lump of the source.
    sprDef = &sprites[mo->sprite];
    sprFrame = &sprDef->spriteFrames[mo->frame];
    // Always use rotation zero.
    mat = sprFrame->mats[0];

#if _DEBUG
    if(!mat)
        Con_Error("LO_AddLuminous: Sprite '%i' frame '%i' missing material.", (int) mo->sprite, mo->frame);
#endif

    // Ensure we have up-to-date information about the material.
    spec = Materials_VariantSpecificationForContext(MC_SPRITE, 0, 1, 0, 0,
        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 1, -2, -1, true, true, true, false);
    ms = Materials_Prepare(mat, spec, true);

    pl = (const pointlight_analysis_t*)
        Texture_Analysis(MSU_texture(ms, MTU_PRIMARY), TA_SPRITE_AUTOLIGHT);
    if(!pl)
        Con_Error("addLuminous: Texture id:%u has no TA_SPRITE_AUTOLIGHT analysis.", Textures_Id(MSU_texture(ms, MTU_PRIMARY)));

    size = pl->brightMul;
    yOffset = ms->size.height * pl->originY;
    // Does the mobj have an active light definition?
    if(def)
    {
        if(def->size)
            size = def->size;
        if(def->offset[VY])
            yOffset = def->offset[VY];
    }

#if _DEBUG
    if(Textures_Namespace(Textures_Id(MSU_texture(ms, MTU_PRIMARY))) != TN_SPRITES)
        Con_Error("LO_AddLuminous: Internal error, material snapshot's primary texture is not a SpriteTex!");
#endif

    pTex = (patchtex_t*) Texture_UserData(MSU_texture(ms, MTU_PRIMARY));
    assert(pTex);

    center = -pTex->offY - mo->floorClip - R_GetBobOffset(mo) - yOffset;

    // Will the sprite be allowed to go inside the floor?
    mul = mo->pos[VZ] + -pTex->offY - (float) ms->size.height - mo->bspLeaf->sector->SP_floorheight;
    if(!(mo->ddFlags & DDMF_NOFITBOTTOM) && mul < 0)
    {
        // Must adjust.
        center -= mul;
    }

    radius = size * 40 * loRadiusFactor;

    // Don't make a too small light.
    if(radius < 32)
        radius = 32;

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
    {
        // Use the auto-calculated color.
        for(i = 0; i < 3; ++i)
            rgb[i] = pl->color.rgb[i];
    }

    // This'll allow a halo to be rendered. If the light is hidden from
    // view by world geometry, the light pointer will be set to NULL.
    mo->lumIdx = LO_NewLuminous(LT_OMNI, mo->bspLeaf);

    l = LO_GetLuminous(mo->lumIdx);
    l->maxDistance = 0;
    l->decorSource = NULL;

    /**
     *  Determine the exact center point of the light.
     *
     * @todo We cannot use smoothing here because this could move the
     * light into another BSP leaf (thereby breaking the rules of the
     * optimized BSP leaf contact/spread algorithm).
    V3f_Set(l->pos, 0, 0, 0);
    if(mo->state && mo->tics >= 0)
    {
        V3f_Copy(l->pos, mo->srvo);
        V3f_Scale(l->pos, (mo->tics - frameTimePos) / (float) mo->state->tics);
    }

    if(!INRANGE_OF(mo->mom[MX], 0, NOMOMENTUM_THRESHOLD) ||
       !INRANGE_OF(mo->mom[MY], 0, NOMOMENTUM_THRESHOLD) ||
       !INRANGE_OF(mo->mom[MZ], 0, NOMOMENTUM_THRESHOLD))
    {
        // Use the object's momentum to calculate a short-range offset.
        vec3f_t tmp;
        V3f_Copy(tmp, mo->mom);
        V3f_Scale(tmp, frameTimePos);
        V3f_Sum(l->pos, l->pos, tmp);
    }

    // Translate to world-space origin.
    V3f_Sum(l->pos, l->pos, mo->pos);
    */
    V3f_Copy(l->pos, mo->pos);

    // Don't make too large a light.
    if(radius > loMaxRadius)
        radius = loMaxRadius;

    LUM_OMNI(l)->radius = radius;
    for(i = 0; i < 3; ++i)
        LUM_OMNI(l)->color[i] = rgb[i];
    LUM_OMNI(l)->zOff = center;

    if(def)
    {
        LUM_OMNI(l)->tex = GL_PrepareLightMap(def->sides);
        LUM_OMNI(l)->ceilTex = GL_PrepareLightMap(def->up);
        LUM_OMNI(l)->floorTex = GL_PrepareLightMap(def->down);
    }
    else
    {
        // Use the same default light texture for all directions.
        LUM_OMNI(l)->tex = LUM_OMNI(l)->ceilTex =
            LUM_OMNI(l)->floorTex = GL_PrepareLSTexture(LST_DYNAMIC);
    }
}

/// Used to sort lumobjs by distance from viewpoint.
static int C_DECL lumobjSorter(const void* e1, const void* e2)
{
    float a = luminousDist[*(const uint *) e1];
    float b = luminousDist[*(const uint *) e2];
    if(a > b) return 1;
    if(a < b) return -1;
    return 0;
}

void LO_BeginFrame(void)
{
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
    uint i;

    if(useDynlights || useLightDecorations)
    {
        /**
         * Clear the projected dynlight lists. This is done here as
         * the projections are sensitive to distance from the viewer
         * (e.g. some may fade out when far away).
         */
        clearProjectionLists();
    }

    if(!(numLuminous > 0))
        return;

BEGIN_PROF( PROF_LUMOBJ_FRAME_SORT );

    // Update lumobj distances ready for linking and sorting.
    for(i = 0; i < numLuminous; ++i)
    {
        lumobj_t* lum = luminousList[i];
        float pos[3];

        V3f_Subtract(pos, lum->pos, viewData->current.pos);

        // Approximate the distance in 3D.
        luminousDist[i] = P_ApproxDistance3(pos[VX], pos[VY], pos[VZ]);
    }

    if(loMaxLumobjs > 0 && numLuminous > loMaxLumobjs)
    {   // Sort lumobjs by distance from the viewer. Then clip all lumobjs
        // so that only the closest are visible (max loMaxLumobjs).
        uint n;

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
 */
static boolean createGlowLightForSurface(Surface* suf, void* paramaters)
{
    switch(DMU_GetType(suf->owner))
    {
    case DMU_PLANE: {
        Plane* pln = (Plane*)suf->owner;
        Sector* sec = pln->sector;
        const averagecolor_analysis_t* avgColorAmplified;
        const materialvariantspecification_t* spec;
        const materialsnapshot_t* ms;
        linkobjtobspleafparams_t params;
        lumobj_t* lum;
        uint i;

        // Only produce a light for sectors with open space.
        /// @todo Do not add surfaces from sectors with zero BSP leafs to the glowing list.
        if(!sec->bspLeafCount || sec->SP_floorvisheight >= sec->SP_ceilvisheight)
            return true; // Continue iteration.

        // Are we glowing at this moment in time?
        spec = Materials_VariantSpecificationForContext(MC_MAPSURFACE, 0, 0, 0, 0,
            GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
        ms = Materials_Prepare(suf->material, spec, true);
        if(!(ms->glowing > .001f)) return true; // Continue iteration.

        avgColorAmplified = (const averagecolor_analysis_t*)
            Texture_Analysis(MSU_texture(ms, MTU_PRIMARY), TA_COLOR_AMPLIFIED);
        if(!avgColorAmplified)
            Con_Error("createGlowLightForSurface: Texture id:%u has no TA_COLOR_AMPLIFIED analysis.", Textures_Id(MSU_texture(ms, MTU_PRIMARY)));

        // @note Plane lights do not spread so simply link to all BspLeafs of this sector.
        lum = createLuminous(LT_PLANE, sec->bspLeafs[0]);
        V3f_Set(lum->pos, pln->origin.pos[VX], pln->origin.pos[VY], pln->visHeight);

        V3f_Copy(LUM_PLANE(lum)->normal, pln->PS_normal);
        V3f_Copy(LUM_PLANE(lum)->color, avgColorAmplified->color.rgb);
        LUM_PLANE(lum)->intensity = ms->glowing;
        LUM_PLANE(lum)->tex = GL_PrepareLSTexture(LST_GRADIENT);
        lum->maxDistance = 0;
        lum->decorSource = 0;

        params.obj = lum;
        params.type = OT_LUMOBJ;
        RIT_LinkObjToBspLeaf(sec->bspLeafs[0], (void*)&params);
        for(i = 1; i < sec->bspLeafCount; ++i)
        {
            linkLumObjToSSec(lum, sec->bspLeafs[i]);
            RIT_LinkObjToBspLeaf(sec->bspLeafs[i], (void*)&params);
        }
        break;
      }
    case DMU_SIDEDEF:
        return true; // Not yet supported by this algorithm.

    default:
        Con_Error("createGlowLightForSurface: Internal error, unknown type %s.",
            DMU_Str(DMU_GetType(suf->owner)));
    }
    return true;
}

void LO_AddLuminousMobjs(void)
{
    if(!useDynlights && !useWallGlow) return;

BEGIN_PROF( PROF_LUMOBJ_INIT_ADD );

    if(useDynlights)
    {
        Sector* seciter;
        uint i;
        for(i = 0, seciter = sectors; i < NUM_SECTORS; seciter++, ++i)
        {
            mobj_t* iter;
            for(iter = seciter->mobjList; iter; iter = iter->sNext)
            {
                iter->lumIdx = 0;
                addLuminous(iter);
            }
        }
    }

    // Create dynlights for all glowing surfaces.
    if(useWallGlow)
    {
        surfacelist_t* slist = GameMap_GlowingSurfaces(theMap);
        if(slist)
        {
            R_SurfaceListIterate(slist, createGlowLightForSurface, 0);
        }
    }

END_PROF( PROF_LUMOBJ_INIT_ADD );
}

typedef struct lumobjiterparams_s {
    float origin[2];
    float radius;
    void* paramaters;
    int (*callback) (const lumobj_t*, float distance, void* paramaters);
} lumobjiterparams_t;

int LOIT_RadiusLumobjs(void* ptr, void* paramaters)
{
    const lumobj_t* lum = (const lumobj_t*) ptr;
    lumobjiterparams_t* p = (lumobjiterparams_t*)paramaters;
    float dist = P_ApproxDistance(lum->pos[VX] - p->origin[VX], lum->pos[VY] - p->origin[VY]);
    int result = false; // Continue iteration.
    if(dist <= p->radius)
    {
        result = p->callback(lum, dist, p->paramaters);
    }
    return result;
}

int LO_LumobjsRadiusIterator2(BspLeaf* bspLeaf, float x, float y, float radius,
    int (*callback) (const lumobj_t*, float distance, void* paramaters), void* paramaters)
{
    lumobjiterparams_t p;
    if(!bspLeaf || !callback) return 0;

    p.origin[VX] = x;
    p.origin[VY] = y;
    p.radius = radius;
    p.callback = callback;
    p.paramaters = paramaters;

    return R_IterateBspLeafContacts2(bspLeaf, OT_LUMOBJ, LOIT_RadiusLumobjs, (void*) &p);
}

int LO_LumobjsRadiusIterator(BspLeaf* bspLeaf, float x, float y, float radius,
    int (*callback) (const lumobj_t*, float distance, void* paramaters))
{
    return LO_LumobjsRadiusIterator2(bspLeaf, x, y, radius, callback, NULL);
}

boolean LOIT_ClipLumObj(void* data, void* context)
{
    lumobj_t* lum = (lumobj_t*) data;
    uint lumIdx = lumToIndex(lum);
    vec3f_t pos;

    if(lum->type != LT_OMNI)
        return true; // Only interested in omnilights.

    if(luminousClipped[lumIdx] > 1)
        return true; // Already hidden by some other means.

    luminousClipped[lumIdx] = 0;

    // \fixme Determine the exact centerpoint of the light in
    // addLuminous!
    V3f_Set(pos, lum->pos[VX], lum->pos[VY], lum->pos[VZ] + LUM_OMNI(lum)->zOff);

    /**
     * Select clipping strategy:
     *
     * If culling world surfaces with the angle clipper and the viewer is
     * not in the void; use the angle clipper here too. Otherwise, use the
     * BSP-based LOS algorithm.
     */
    if(!(devNoCulling || P_IsInVoid(&ddPlayers[displayPlayer])))
    {
        if(!C_IsPointVisible(pos[VX], pos[VY], pos[VZ]))
            luminousClipped[lumIdx] = 1; // Won't have a halo.
    }
    else
    {
        vec3f_t              vpos;

        V3f_Set(vpos, vx, vz, vy);

        luminousClipped[lumIdx] = 1;
        if(P_CheckLineSight(vpos, pos, -1, 1, LS_PASSLEFT | LS_PASSOVER | LS_PASSUNDER))
        {
            luminousClipped[lumIdx] = 0; // Will have a halo.
        }
    }

    return true; // Continue iteration.
}

void LO_ClipInBspLeaf(uint bspLeafIdx)
{
    iterateBspLeafLumObjs(GameMap_BspLeaf(theMap, bspLeafIdx), LOIT_ClipLumObj, NULL);
}

boolean LOIT_ClipLumObjBySight(void* data, void* context)
{
    lumobj_t* lum = (lumobj_t*) data;
    uint lumIdx = lumToIndex(lum);
    BspLeaf* bspLeaf = (BspLeaf*) context;

    if(lum->type != LT_OMNI)
        return true; // Only interested in omnilights.

    if(!luminousClipped[lumIdx])
    {
        vec2f_t eye;
        uint i;

        V2f_Set(eye, vx, vz);

        // We need to figure out if any of the polyobj's segments lies
        // between the viewpoint and the lumobj.
        for(i = 0; i < bspLeaf->polyObj->lineCount; ++i)
        {
            LineDef* line = bspLeaf->polyObj->lines[i];
            HEdge* hedge = line->L_frontside->hedgeLeft;

            // Ignore hedges facing the wrong way.
            if(hedge->frameFlags & HEDGEINF_FACINGFRONT)
            {
                vec2f_t source;

                V2f_Set(source, lum->pos[VX], lum->pos[VY]);
                if(V2f_Intercept2(source, eye, hedge->HE_v1pos, hedge->HE_v2pos, NULL, NULL, NULL))
                {
                    luminousClipped[lumIdx] = 1;
                    break;
                }
            }
        }
    }

    return true; // Continue iteration.
}

void LO_ClipInBspLeafBySight(uint bspLeafIdx)
{
    BspLeaf* leaf = GameMap_BspLeaf(theMap, bspLeafIdx);
    iterateBspLeafLumObjs(leaf, LOIT_ClipLumObjBySight, leaf);
}

static boolean iterateBspLeafLumObjs(BspLeaf* bspLeaf, boolean (*func) (void*, void*),
    void* data)
{
    lumlistnode_t* ln = bspLeafLumObjList[GET_BSPLEAF_IDX(bspLeaf)];
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

int LOIT_UnlinkMobjLumobj(thinker_t* th, void* context)
{
    LO_UnlinkMobjLumobj((mobj_t*) th);
    return false; // Continue iteration.
}

void LO_UnlinkMobjLumobjs(void)
{
    if(!useDynlights && theMap)
    {
        // Mobjs are always public.
        GameMap_IterateThinkers(theMap, gx.MobjThinker, 0x1, LOIT_UnlinkMobjLumobj, NULL);
    }
}

/**
 * @param paramaters  ProjectLightToSurfaceIterator paramaters.
 *
 * @return  @c 0 = continue iteration.
 */
int RIT_ProjectLightToSurfaceIterator(void* obj, void* paramaters)
{
    lumobj_t* lum = (lumobj_t*)obj;
    assert(obj);
    switch(lum->type)
    {
    case LT_OMNI:  return  projectOmniLightToSurface(lum, paramaters);
    case LT_PLANE: return projectPlaneLightToSurface(lum, paramaters);
    default:
        Con_Error("RIT_ProjectLightToSurface: Invalid lumobj type %i.", (int) lum->type);
        exit(1); // Unreachable.
    }
}

uint LO_ProjectToSurface(int flags, BspLeaf* bspLeaf, float blendFactor,
    vec3f_t topLeft, vec3f_t bottomRight, vec3f_t tangent, vec3f_t bitangent, vec3f_t normal)
{
    projectlighttosurfaceiteratorparams_t p;

    p.listIdx = 0;
    p.spParams.blendFactor = blendFactor;
    p.spParams.flags = flags;
    p.spParams.v1 = topLeft;
    p.spParams.v2 = bottomRight;
    p.spParams.tangent = tangent;
    p.spParams.bitangent = bitangent;
    p.spParams.normal = normal;

    R_IterateBspLeafContacts2(bspLeaf, OT_LUMOBJ, RIT_ProjectLightToSurfaceIterator, (void*)&p);
    // Did we produce a projection list?
    return p.listIdx;
}

int LO_IterateProjections2(uint listIdx, int (*callback) (const dynlight_t*, void*), void* paramaters)
{
    int result = 0; // Continue iteration.
    if(callback && listIdx != 0 && listIdx <= projectionListCount)
    {
        listnode_t* node = projectionLists[listIdx-1].head;
        while(node)
        {
            result = callback(&node->projection, paramaters);
            node = (!result? node->next : NULL /* Early out */);
        }
    }
    return result;
}

int LO_IterateProjections(uint listIdx, int (*callback) (const dynlight_t*, void*))
{
    return LO_IterateProjections2(listIdx, callback, NULL);
}

void LO_DrawLumobjs(void)
{
    static const float  black[4] = { 0, 0, 0, 0 };
    float color[4];
    uint i;

    if(!devDrawLums)
        return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    for(i = 0; i < numLuminous; ++i)
    {
        lumobj_t* lum = luminousList[i];
        vec3f_t lumCenter;

        if(!(lum->type == LT_OMNI || lum->type == LT_PLANE))
            continue;

        if(lum->type == LT_OMNI && loMaxLumobjs > 0 && luminousClipped[i] == 2)
            continue;

        V3f_Copy(lumCenter, lum->pos);
        if(lum->type == LT_OMNI)
            lumCenter[VZ] += LUM_OMNI(lum)->zOff;

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(lumCenter[VX], lumCenter[VZ], lumCenter[VY]);

        switch(lum->type)
        {
        case LT_OMNI: {
            float scale = LUM_OMNI(lum)->radius;

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
        case LT_PLANE: {
            float scale = LUM_PLANE(lum)->intensity * 200;

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
        default: break;
        }

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}
