/** @file dmu_lib.cpp Helper routines for accessing the DMU API.
 *
 * @authors Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cstdio>
#include <cstring>

#include "common.h"
#include "dmu_lib.h"
#include "p_terraintype.h"

#include "dmu_lib.h"

int P_PathXYTraverse2(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY,
    int flags, traverser_t callback, void *context)
{
    vec2d_t from = { fromX, fromY };
    vec2d_t to   = { toX, toY };
    return P_PathTraverse2(from, to, flags, callback, context);
}

int P_PathXYTraverse(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY,
    traverser_t callback, void *context)
{
    vec2d_t from = { fromX, fromY };
    vec2d_t to   = { toX, toY };
    return P_PathTraverse(from, to, callback, context);
}

struct TagList
{
    iterlist_t *list;
    int tag;
};

static TagList *lineTagLists;
static uint numLineTagLists;

static TagList *sectorTagLists;
static uint numSectorTagLists;

Line *P_AllocDummyLine()
{
    xline_t *extra = (xline_t *)Z_Calloc(sizeof(xline_t), PU_GAMESTATIC, 0);
    return (Line *)P_AllocDummy(DMU_LINE, extra);
}

void P_FreeDummyLine(Line *line)
{
    Z_Free(P_DummyExtraData(line));
    P_FreeDummy(line);
}

void P_CopyLine(Line *dest, Line *src)
{
    xline_t *xsrc  = P_ToXLine(src);
    xline_t *xdest = P_ToXLine(dest);

    if(src == dest)
        return; // no point copying self

    // Copy the built-in properties
    for(int i = 0; i < 2; ++i) // For each side
    {
        int sidx = (i==0? DMU_FRONT : DMU_BACK);

        Side *sidefrom = (Side *)P_GetPtrp(src, sidx);
        Side *sideto = (Side *)P_GetPtrp(dest, sidx);

        if(!sidefrom || !sideto)
            continue;

        float temp[4];
        coord_t itemp[2];

        P_SetPtrp    (sideto,   DMU_TOP_MATERIAL,               P_GetPtrp(sidefrom, DMU_TOP_MATERIAL));
        P_GetDoublepv(sidefrom, DMU_TOP_MATERIAL_OFFSET_XY,     itemp);
        P_SetDoublepv(sideto,   DMU_TOP_MATERIAL_OFFSET_XY,     itemp);
        P_GetFloatpv (sidefrom, DMU_TOP_COLOR,                  temp);
        P_SetFloatpv (sideto,   DMU_TOP_COLOR,                  temp);

        P_SetPtrp    (sideto,   DMU_MIDDLE_MATERIAL,            P_GetPtrp(sidefrom, DMU_MIDDLE_MATERIAL));
        P_GetDoublepv(sidefrom, DMU_MIDDLE_MATERIAL_OFFSET_XY,  itemp);
        P_SetDoublepv(sideto,   DMU_MIDDLE_MATERIAL_OFFSET_XY,  itemp);
        P_SetFloatpv (sideto,   DMU_MIDDLE_COLOR,               temp);
        P_SetIntp    (sideto,   DMU_MIDDLE_BLENDMODE,           P_GetIntp(sidefrom, DMU_MIDDLE_BLENDMODE));

        P_SetPtrp    (sideto,   DMU_BOTTOM_MATERIAL,            P_GetPtrp(sidefrom, DMU_BOTTOM_MATERIAL));
        P_GetDoublepv(sidefrom, DMU_BOTTOM_MATERIAL_OFFSET_XY,  itemp);
        P_SetDoublepv(sideto,   DMU_BOTTOM_MATERIAL_OFFSET_XY,  itemp);
        P_GetFloatpv (sidefrom, DMU_BOTTOM_COLOR,               temp);
        P_SetFloatpv (sideto,   DMU_BOTTOM_COLOR,               temp);
    }

    // Copy the extended properties too
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    xdest->special = xsrc->special;
    xdest->tag     = xsrc->tag;
    if(xsrc->xg && xdest->xg)
        std::memcpy(xdest->xg, xsrc->xg, sizeof(*xdest->xg));
    else
        xdest->xg = 0;
#else
    xdest->special = xsrc->special;
    xdest->arg1 = xsrc->arg1;
    xdest->arg2 = xsrc->arg2;
    xdest->arg3 = xsrc->arg3;
    xdest->arg4 = xsrc->arg4;
    xdest->arg5 = xsrc->arg5;
#endif
}

void P_CopySector(Sector *dest, Sector *src)
{
    xsector_t *xsrc = P_ToXSector(src);
    xsector_t *xdest = P_ToXSector(dest);

    if(src == dest)
        return; // no point copying self.

    // Copy the built-in properties.
    float ftemp[4];
    coord_t dtemp[2];

    P_SetFloatp  (dest, DMU_LIGHT_LEVEL,                P_GetFloatp(src, DMU_LIGHT_LEVEL));
    P_GetFloatpv (src,  DMU_COLOR,                      ftemp);
    P_SetFloatpv (dest, DMU_COLOR,                      ftemp);

    P_SetDoublep (dest, DMU_FLOOR_HEIGHT,               P_GetDoublep(src, DMU_FLOOR_HEIGHT));
    P_SetPtrp    (dest, DMU_FLOOR_MATERIAL,             P_GetPtrp(src, DMU_FLOOR_MATERIAL));
    P_GetFloatpv (src,  DMU_FLOOR_COLOR,                ftemp);
    P_SetFloatpv (dest, DMU_FLOOR_COLOR,                ftemp);
    P_GetDoublepv(src,  DMU_FLOOR_MATERIAL_OFFSET_XY,   dtemp);
    P_SetDoublepv(dest, DMU_FLOOR_MATERIAL_OFFSET_XY,   dtemp);
    P_SetIntp    (dest, DMU_FLOOR_SPEED,                P_GetIntp(src, DMU_FLOOR_SPEED));
    P_SetDoublep (dest, DMU_FLOOR_TARGET_HEIGHT,        P_GetFloatp(src, DMU_FLOOR_TARGET_HEIGHT));

    P_SetDoublep (dest, DMU_CEILING_HEIGHT,             P_GetDoublep(src, DMU_CEILING_HEIGHT));
    P_SetPtrp    (dest, DMU_CEILING_MATERIAL,           P_GetPtrp(src, DMU_CEILING_MATERIAL));
    P_GetFloatpv (src,  DMU_CEILING_COLOR,              ftemp);
    P_SetFloatpv (dest, DMU_CEILING_COLOR,              ftemp);
    P_GetDoublepv(src,  DMU_CEILING_MATERIAL_OFFSET_XY, dtemp);
    P_SetDoublepv(dest, DMU_CEILING_MATERIAL_OFFSET_XY, dtemp);
    P_SetIntp    (dest, DMU_CEILING_SPEED,              P_GetIntp(src, DMU_CEILING_SPEED));
    P_SetDoublep (dest, DMU_CEILING_TARGET_HEIGHT,      P_GetFloatp(src, DMU_CEILING_TARGET_HEIGHT));

    // Copy the extended properties too
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    xdest->special = xsrc->special;
    xdest->soundTraversed = xsrc->soundTraversed;
    xdest->soundTarget = xsrc->soundTarget;
#if __JHERETIC__
    xdest->seqType = xsrc->seqType;
#endif
    xdest->SP_floororigheight = xsrc->SP_floororigheight;
    xdest->SP_ceilorigheight = xsrc->SP_ceilorigheight;
    xdest->origLight = xsrc->origLight;
    std::memcpy(xdest->origRGB, xsrc->origRGB, sizeof(float) * 3);
    if(xsrc->xg && xdest->xg)
        std::memcpy(xdest->xg, xsrc->xg, sizeof(*xdest->xg));
    else
        xdest->xg = 0;
#else
    xdest->special = xsrc->special;
    xdest->soundTraversed = xsrc->soundTraversed;
    xdest->soundTarget = xsrc->soundTarget;
    xdest->seqType = xsrc->seqType;
#endif
}

void P_BuildLineTagLists()
{
    P_DestroyLineTagLists();

    for(int i = 0; i < numlines; ++i)
    {
        Line *line  = (Line *)P_ToPtr(DMU_LINE, i);
        xline_t *xline = P_ToXLine(line);

#if !__JHEXEN__
        if(xline->tag)
        {
           iterlist_t *list = P_GetLineIterListForTag(xline->tag, true);
           IterList_PushBack(list, line);
        }
#else
        switch(xline->special)
        {
        default: break;
        case 121: ///< Line_SetIdentification.
            if(xline->arg1)
            {
                iterlist_t *list = P_GetLineIterListForTag((int) xline->arg1, true);
                IterList_PushBack(list, line);
            }
            xline->special = 0;
            break;
        }
#endif
    }
}

void P_DestroyLineTagLists()
{
    if(numLineTagLists == 0)
        return;

    for(uint i = 0; i < numLineTagLists; ++i)
    {
        IterList_Clear(lineTagLists[i].list);
        IterList_Delete(lineTagLists[i].list);
    }

    free(lineTagLists);
    lineTagLists = 0;
    numLineTagLists = 0;
}

iterlist_t *P_GetLineIterListForTag(int tag, dd_bool createNewList)
{
    // Do we have an existing list for this tag?
    for(uint i = 0; i < numLineTagLists; ++i)
    {
        if(lineTagLists[i].tag == tag)
            return lineTagLists[i].list;
    }

    if(!createNewList)
        return 0;

    // Nope, we need to allocate another.
    numLineTagLists++;
    lineTagLists = (TagList *)realloc(lineTagLists, sizeof(TagList) * numLineTagLists);
    TagList *tagList = &lineTagLists[numLineTagLists - 1];
    tagList->tag = tag;

    return (tagList->list = IterList_New());
}

void P_BuildSectorTagLists()
{
    P_DestroySectorTagLists();

    for(int i = 0; i < numsectors; ++i)
    {
        Sector *sec = (Sector *)P_ToPtr(DMU_SECTOR, i);
        xsector_t *xsec = P_ToXSector(sec);

        if(xsec->tag)
        {
            iterlist_t *list = P_GetSectorIterListForTag(xsec->tag, true);
            IterList_PushBack(list, sec);
        }
    }
}

void P_DestroySectorTagLists()
{
    if(numSectorTagLists == 0)
        return;

    for(uint i = 0; i < numSectorTagLists; ++i)
    {
        IterList_Clear(sectorTagLists[i].list);
        IterList_Delete(sectorTagLists[i].list);
    }

    free(sectorTagLists);
    sectorTagLists = 0;
    numSectorTagLists = 0;
}

iterlist_t *P_GetSectorIterListForTag(int tag, dd_bool createNewList)
{
    // Do we have an existing list for this tag?
    for(uint i = 0; i < numSectorTagLists; ++i)
    {
        if(sectorTagLists[i].tag == tag)
            return sectorTagLists[i].list;
    }

    if(!createNewList)
        return 0;

    // Nope, we need to allocate another.
    numSectorTagLists++;
    sectorTagLists = (TagList *)realloc(sectorTagLists, sizeof(TagList) * numSectorTagLists);
    TagList *tagList = &sectorTagLists[numSectorTagLists - 1];
    tagList->tag = tag;

    return (tagList->list = IterList_New());
}

void P_BuildAllTagLists()
{
    P_BuildSectorTagLists();
    P_BuildLineTagLists();
}

void P_DestroyAllTagLists()
{
    P_DestroyLineTagLists();
    P_DestroySectorTagLists();
}

Sector *P_GetNextSector(Line *line, Sector *sec)
{
    if(!sec || !line)
        return 0;

    if(!(P_ToXLine(line)->flags & ML_TWOSIDED))
        return 0;

    Sector *frontSec = (Sector *)P_GetPtrp(line, DMU_FRONT_SECTOR);
    if(frontSec == sec)
        return (Sector *)P_GetPtrp(line, DMU_BACK_SECTOR);
    return frontSec;
}

int findExtremalLightLevelInAdjacentSectors(void *ptr, void *context)
{
    findlightlevelparams_t *params = (findlightlevelparams_t *) context;
    Sector *other = P_GetNextSector((Line *) ptr, params->baseSec);

    if(!other)
        return false; // Continue iteration.

    float lightLevel = P_GetFloatp(other, DMU_LIGHT_LEVEL);
    if(params->flags & FELLF_MIN)
    {
        if(lightLevel < params->val)
        {
            params->val = lightLevel;
            params->foundSec = other;
            if(params->val <= 0)
                return true; // Stop iteration. Can't get any darker.
        }
    }
    else if(lightLevel > params->val)
    {
        params->val = lightLevel;
        params->foundSec = other;
        if(params->val >= 1)
            return true; // Stop iteration. Can't get any brighter.
    }
    return false; // Continue iteration.
}

Sector *P_FindSectorSurroundingLowestLight(Sector *sec, float *val)
{
    findlightlevelparams_t params;
    params.flags = FELLF_MIN;
    params.val = DDMAXFLOAT;
    params.baseSec = sec;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINE, findExtremalLightLevelInAdjacentSectors, &params);
    if(*val)
        *val = params.val;
    return params.foundSec;
}

Sector *P_FindSectorSurroundingHighestLight(Sector *sec, float *val)
{
    findlightlevelparams_t params;
    params.flags = 0;
    params.val = DDMINFLOAT;
    params.baseSec = sec;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINE, findExtremalLightLevelInAdjacentSectors, &params);
    if(val)
        *val = params.val;
    return params.foundSec;
}

int findNextLightLevel(void* ptr, void *context)
{
    findnextlightlevelparams_t *params = (findnextlightlevelparams_t *) context;
    Line *li = (Line *) ptr;

    Sector *other = P_GetNextSector(li, params->baseSec);
    if(!other)
        return false; // Continue iteration.

    float otherLight = P_GetFloatp(other, DMU_LIGHT_LEVEL);
    if(params->flags & FNLLF_ABOVE)
    {
        if(otherLight < params->val && otherLight > params->baseLight)
        {
            params->val = otherLight;
            params->foundSec = other;

            if(!(params->val > 0))
                return true; // Stop iteration. Can't get any darker.
        }
    }
    else if(otherLight > params->val && otherLight < params->baseLight)
    {
        params->val = otherLight;
        params->foundSec = other;

        if(!(params->val < 1))
            return true; // Stop iteration. Can't get any brighter.
    }
    return false; // Continue iteration.
}

Sector *P_FindSectorSurroundingNextLowestLight(Sector *sec, float baseLight, float *val)
{
    findnextlightlevelparams_t params;
    params.flags = 0;
    params.val = DDMINFLOAT;
    params.baseSec = sec;
    params.baseLight = baseLight;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINE, findNextLightLevel, &params);
    if(*val)
        *val = params.val;
    return params.foundSec;
}

Sector *P_FindSectorSurroundingNextHighestLight(Sector *sec, float baseLight, float *val)
{
    findnextlightlevelparams_t params;
    params.flags = FNLLF_ABOVE;
    params.val = DDMAXFLOAT;
    params.baseSec = sec;
    params.baseLight = baseLight;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINE, findNextLightLevel, &params);
    if(*val)
        *val = params.val;
    return params.foundSec;
}

int findExtremalPlaneHeight(void *ptr, void *context)
{
    findextremalplaneheightparams_t *params = (findextremalplaneheightparams_t *) context;
    Sector *other = P_GetNextSector((Line *) ptr, params->baseSec);

    if(!other)
        return false; // Continue iteration.

    coord_t height = P_GetDoublep(other, ((params->flags & FEPHF_FLOOR)? DMU_FLOOR_HEIGHT : DMU_CEILING_HEIGHT));
    if(params->flags & FEPHF_MIN)
    {
        if(height < params->val)
        {
            params->val = height;
            params->foundSec = other;
        }
    }
    else if(height > params->val)
    {
        params->val = height;
        params->foundSec = other;
    }

    return false; // Continue iteration.
}

Sector *P_FindSectorSurroundingLowestFloor(Sector *sec, coord_t max, coord_t *val)
{
    findextremalplaneheightparams_t params;
    params.flags = FEPHF_MIN | FEPHF_FLOOR;
    params.val = max;
    params.baseSec = sec;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINE, findExtremalPlaneHeight, &params);
    if(val)
        *val = params.val;
    return params.foundSec;
}

Sector *P_FindSectorSurroundingHighestFloor(Sector *sec, coord_t min, coord_t *val)
{
    findextremalplaneheightparams_t params;
    params.flags = FEPHF_FLOOR;
    params.val = min;
    params.baseSec = sec;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINE, findExtremalPlaneHeight, &params);
    if(val)
        *val = params.val;
    return params.foundSec;
}

Sector *P_FindSectorSurroundingLowestCeiling(Sector *sec, coord_t max, coord_t *val)
{
    findextremalplaneheightparams_t params;
    params.flags = FEPHF_MIN;
    params.val = max;
    params.baseSec = sec;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINE, findExtremalPlaneHeight, &params);
    if(val)
        *val = params.val;
    return params.foundSec;
}

Sector *P_FindSectorSurroundingHighestCeiling(Sector *sec, coord_t min, coord_t *val)
{
    findextremalplaneheightparams_t params;
    params.flags = 0;
    params.val = min;
    params.baseSec = sec;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINE, findExtremalPlaneHeight, &params);
    if(val)
        *val = params.val;
    return params.foundSec;
}

int findNextPlaneHeight(void *ptr, void *context)
{
    findnextplaneheightparams_t *params = (findnextplaneheightparams_t *) context;
    Sector *other = P_GetNextSector((Line *) ptr, params->baseSec);

    if(!other)
        return false; // Continue iteration.

    coord_t otherHeight = P_GetDoublep(other, ((params->flags & FNPHF_FLOOR)? DMU_FLOOR_HEIGHT : DMU_CEILING_HEIGHT));
    if(params->flags & FNPHF_ABOVE)
    {
        if(otherHeight < params->val && otherHeight > params->baseHeight)
        {
            params->val = otherHeight;
            params->foundSec = other;
        }
    }
    else if(otherHeight > params->val && otherHeight < params->baseHeight)
    {
        params->val = otherHeight;
        params->foundSec = other;
    }
    return false; // Continue iteration.
}

Sector *P_FindSectorSurroundingNextHighestFloor(Sector *sec, coord_t baseHeight, coord_t *val)
{
    findnextplaneheightparams_t params;
    params.flags = FNPHF_FLOOR | FNPHF_ABOVE;
    params.val = DDMAXFLOAT;
    params.baseSec = sec;
    params.baseHeight = baseHeight;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINE, findNextPlaneHeight, &params);
    if(val)
        *val = params.val;
    return params.foundSec;
}

Sector *P_FindSectorSurroundingNextHighestCeiling(Sector *sec, coord_t baseHeight, coord_t *val)
{
    findnextplaneheightparams_t params;
    params.flags = FNPHF_ABOVE;
    params.val = DDMAXFLOAT;
    params.baseSec = sec;
    params.baseHeight = baseHeight;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINE, findNextPlaneHeight, &params);
    if(val)
        *val = params.val;
    return params.foundSec;
}

Sector *P_FindSectorSurroundingNextLowestFloor(Sector *sec, coord_t baseHeight, coord_t *val)
{
    findnextplaneheightparams_t params;
    params.flags = FNPHF_FLOOR;
    params.val = DDMINFLOAT;
    params.baseSec = sec;
    params.baseHeight = baseHeight;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINE, findNextPlaneHeight, &params);
    if(val)
        *val = params.val;
    return params.foundSec;
}

Sector *P_FindSectorSurroundingNextLowestCeiling(Sector *sec, coord_t baseHeight, coord_t *val)
{
    findnextplaneheightparams_t params;
    params.flags = 0;
    params.val = DDMINFLOAT;
    params.baseSec = sec;
    params.baseHeight = baseHeight;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINE, findNextPlaneHeight, &params);
    if(val)
        *val = params.val;
    return params.foundSec;
}

float P_SectorLight(Sector *sector)
{
    return P_GetFloatp(sector, DMU_LIGHT_LEVEL);
}

void P_SectorSetLight(Sector *sector, float level)
{
    P_SetFloatp(sector, DMU_LIGHT_LEVEL, level);
}

void P_SectorModifyLight(Sector *sector, float value)
{
    float level = MINMAX_OF(0.f, P_SectorLight(sector) + value, 1.f);
    P_SectorSetLight(sector, level);
}

void P_SectorModifyLightx(Sector *sector, fixed_t value)
{
    P_SetFloatp(sector, DMU_LIGHT_LEVEL, P_SectorLight(sector) + FIX2FLT(value) / 255.0f);
}

const terraintype_t *P_PlaneMaterialTerrainType(Sector *sec, int plane)
{
    return P_TerrainTypeForMaterial((world_Material *)P_GetPtrp(sec, (plane? DMU_CEILING_MATERIAL : DMU_FLOOR_MATERIAL)));
}

void P_TranslateSideMaterialOrigin(Side *side, SideSection section, float deltaXY[2])
{
    DE_ASSERT(side);
    DE_ASSERT(VALID_SIDESECTION(section));

    const uint dmuSurfaceOriginFlags = DMU_OFFSET_XY | DMU_FLAG_FOR_SIDESECTION(section);
    float origin[2];

    if(IS_ZERO(deltaXY[0]) && IS_ZERO(deltaXY[1])) return;

    P_GetFloatpv(side, dmuSurfaceOriginFlags, origin);
    if(NON_ZERO(deltaXY[0]))
    {
        origin[0] += deltaXY[0];
    }
    if(NON_ZERO(deltaXY[1]))
    {
        origin[1] += deltaXY[1];
    }
    P_SetFloatpv(side, dmuSurfaceOriginFlags, origin);
}

void P_TranslateSideMaterialOriginXY(Side *side, SideSection section,
    float deltaX, float deltaY)
{
    float delta[2];
    delta[0] = deltaX;
    delta[1] = deltaY;
    P_TranslateSideMaterialOrigin(side, section, delta);
}

void P_TranslatePlaneMaterialOrigin(Plane *plane, float deltaXY[2])
{
    float origin[2];

    DE_ASSERT(plane);

    if(IS_ZERO(deltaXY[0]) && IS_ZERO(deltaXY[1])) return;

    P_GetFloatpv(plane, DMU_OFFSET_XY, origin);
    if(NON_ZERO(deltaXY[0]))
    {
        origin[0] += deltaXY[0];
    }
    if(NON_ZERO(deltaXY[1]))
    {
        origin[1] += deltaXY[1];
    }
    P_SetFloatpv(plane, DMU_OFFSET_XY, origin);
}

void P_TranslatePlaneMaterialOriginXY(Plane *plane, float deltaX, float deltaY)
{
    float delta[2];
    delta[0] = deltaX;
    delta[1] = deltaY;
    P_TranslatePlaneMaterialOrigin(plane, delta);
}
