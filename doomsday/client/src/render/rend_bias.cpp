/** @file rend_bias.cpp Light/Shadow Bias.
 *
 * @authors Copyright © 2005-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_edit.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_defs.h"
#include "de_misc.h"
#include "de_play.h"
#include "map/gamemap.h"

#include "render/rend_bias.h"

using namespace de;

BEGIN_PROF_TIMERS()
  PROF_BIAS_UPDATE
END_PROF_TIMERS()

typedef struct affection_s {
    float intensities[MAX_BIAS_AFFECTED];
    int numFound;
    biasaffection_t *affected;
} affection_t;

void SB_EvalPoint(float light[4], vertexillum_t *illum, biasaffection_t *affectedSources,
    Vector3d const &point, Vector3f const &normal);

int useBias;
int numSources;
uint currentTimeSB;

static source_t sources[MAX_BIAS_LIGHTS];
static int numSourceDelta;

static int useSightCheck = true;
static float biasMin = .85f;
static float biasMax = 1.f;
static int doUpdateAffected = true;
static float biasIgnoreLimit = .005f;
static int lightSpeed = 130;
static uint lastChangeOnFrame;

/**
 * BS_EvalPoint uses these, so they must be set before it is called.
 */
static biastracker_t trackChanged;
static biastracker_t trackApplied;

static float biasAmount;

// Head of the biassurface list for the current map.
static BiasSurface *surfaces;
static zblockset_t *biasSurfaceBlockSet;

void SB_Register()
{
    C_VAR_INT("rend-bias", &useBias, 0, 0, 1);

    C_VAR_FLOAT("rend-bias-min", &biasMin, 0, 0, 1);

    C_VAR_FLOAT("rend-bias-max", &biasMax, 0, 0, 1);

    C_VAR_INT("rend-bias-lightspeed", &lightSpeed, 0, 0, 5000);

    // Development variables.
    C_VAR_INT("rend-dev-bias-sight", &useSightCheck, CVF_NO_ARCHIVE, 0, 1);

    C_VAR_INT("rend-dev-bias-affected", &doUpdateAffected, CVF_NO_ARCHIVE, 0, 1);

/*    C_VAR_INT("rend-dev-bias-solo", &editSelector, CVF_NO_ARCHIVE, -1, 255);*/
}

static inline BiasSurface *allocBiasSurface()
{
    if(biasSurfaceBlockSet)
    {
        // Use the block allocator.
        BiasSurface *bsuf = (BiasSurface *) ZBlockSet_Allocate(biasSurfaceBlockSet);
        std::memset(bsuf, 0, sizeof(*bsuf));
        return bsuf;
    }

    return (BiasSurface *) M_Calloc(sizeof(BiasSurface));
}

static inline void freeBiasSurface(BiasSurface *bsuf)
{
    if(biasSurfaceBlockSet)
    {
        // Ignore, it'll be free'd along with the block allocator.
        return;
    }

    M_Free(bsuf);
}

BiasSurface *SB_CreateSurface()
{
    BiasSurface *bsuf = allocBiasSurface();

    // Link it in to the global list.
    bsuf->next = surfaces;
    surfaces = bsuf;

    return bsuf;
}

void SB_DestroySurface(BiasSurface *bsuf)
{
    if(!bsuf) return;

    // Unlink this surface from the global list.
    if(surfaces)
    {
        if(bsuf == surfaces)
        {
            surfaces = surfaces->next;
        }
        else if(surfaces->next)
        {
            BiasSurface *p = surfaces->next, *last = surfaces;

            do
            {
                if(p == bsuf)
                {
                    last->next = p->next;
                    break;
                }

                last = p;
                p = p->next;
            } while(p);
        }
    }

    Z_Free(bsuf->illum);
    Z_Free(bsuf);
}

int SB_NewSourceAt(coord_t x, coord_t y, coord_t z, float size, float minLight,
    float maxLight, float *rgb)
{
    if(numSources == MAX_BIAS_LIGHTS)
        return -1;

    source_t *src = &sources[numSources++];

    // New lights are automatically locked.
    src->flags = BLF_CHANGED | BLF_LOCKED;

    src->origin[VX] = x;
    src->origin[VY] = y;
    src->origin[VZ] = z;

    SB_SetColor(src->color,rgb);

    src->primaryIntensity = src->intensity = size;

    src->sectorLevel[0] = minLight;
    src->sectorLevel[1] = maxLight;

    // This'll enforce an update (although the vertices are also
    // STILL_UNSEEN).
    src->lastUpdateTime = 0;

    return numSources; // == index + 1;
}

void SB_UpdateSource(int which, coord_t x, coord_t y, coord_t z, float size,
    float minLight, float maxLight, float *rgb)
{
    if(which < 0 || which >= numSources) return;

    source_t *src = &sources[which];

    // Position change?
    src->origin[VX] = x;
    src->origin[VY] = y;
    src->origin[VZ] = z;

    SB_SetColor(src->color, rgb);

    src->primaryIntensity = src->intensity = size;

    src->sectorLevel[0] = minLight;
    src->sectorLevel[1] = maxLight;
}

source_t *SB_GetSource(int which)
{
    return &sources[which];
}

int SB_ToIndex(source_t *source)
{
    if(source)
    {
        return (source - sources);
    }
    return -1;
}

void SB_Delete(int which)
{
    if(which < 0 || which >= numSources)
        return; // Very odd...

    // Do a memory move.
    for(int i = which; i < numSources; ++i)
        sources[i].flags |= BLF_CHANGED;

    if(which < numSources)
        std::memmove(&sources[which], &sources[which + 1],
                     sizeof(source_t) * (numSources - which - 1));

    sources[numSources - 1].intensity = 0;

    // Will be one fewer very soon.
    numSourceDelta--;
}

void SB_Clear()
{
    while(numSources-- > 0)
    {
        sources[numSources].flags |= BLF_CHANGED;
    }
    numSources = 0;
}

void SB_InitForMap(char const *uniqueID)
{
    de::Time begunAt;

    DENG_ASSERT(theMap);

    // Start with no sources whatsoever.
    numSources = 0;

    if(biasSurfaceBlockSet)
        ZBlockSet_Delete(biasSurfaceBlockSet);

    biasSurfaceBlockSet = ZBlockSet_New(sizeof(BiasSurface), 512, PU_APPSTATIC);
    surfaces = NULL;

    // Check all the loaded Light definitions for any matches.
    for(int i = 0; i < defs.count.lights.num; ++i)
    {
        ded_light_t *def = &defs.lights[i];

        if(def->state[0] || stricmp(uniqueID, def->uniqueMapID))
            continue;

        if(SB_NewSourceAt(def->offset[VX], def->offset[VY], def->offset[VZ],
                          def->size, def->lightLevel[0],
                          def->lightLevel[1], def->color) == -1)
            break;
    }

    // Create biassurfaces for all current worldmap surfaces.
    uint numVertIllums = 0;

    // First, determine the total number of vertexillum_ts we need.
    foreach(Segment *segment, theMap->segments())
    {
        if(segment->hasLineSide())
            numVertIllums++;
    }

    numVertIllums *= 3 * 4;

    foreach(Sector *sector, theMap->sectors())
    foreach(BspLeaf *bspLeaf, sector->bspLeafs())
    {
        if(bspLeaf->isDegenerate()) continue;

        numVertIllums += bspLeaf->numFanVertices() * sector->planeCount();
    }

    foreach(Polyobj *polyobj, theMap->polyobjs())
    {
        numVertIllums += polyobj->lineCount() * 3 * 4;
    }

    // Allocate and initialize the vertexillum_ts.
    vertexillum_t *illums = (vertexillum_t *) Z_Calloc(sizeof(*illums) * numVertIllums, PU_MAP, 0);
    for(uint i = 0; i < numVertIllums; ++i)
    {
        SB_InitVertexIllum(&illums[i]);
    }

    // Allocate bias surfaces and attach vertexillum_ts.
    foreach(Segment *segment, theMap->segments())
    {
        if(!segment->hasLineSide()) continue;

        for(int i = 0; i < 3; ++i)
        {
            BiasSurface *bsuf = SB_CreateSurface();

            bsuf->size  = 4;
            bsuf->illum = illums;

            segment->setBiasSurface(i, bsuf);

            illums += bsuf->size;
        }
    }

    foreach(Sector *sector, theMap->sectors())
    foreach(BspLeaf *bspLeaf, sector->bspLeafs())
    {
        if(bspLeaf->isDegenerate()) continue;

        for(int i = 0; i < sector->planeCount(); ++i)
        {
            BiasSurface *bsuf = SB_CreateSurface();

            bsuf->size  = bspLeaf->numFanVertices();
            bsuf->illum = illums;

            bspLeaf->setBiasSurface(i, bsuf);

            illums += bsuf->size;
        }
    }

    foreach(Polyobj *polyobj, theMap->polyobjs())
    foreach(Line *line, polyobj->lines())
    {
        Segment *segment = line->front().leftSegment();

        for(int i = 0; i < 3; ++i)
        {
            BiasSurface *bsuf = SB_CreateSurface();

            bsuf->size = 4;
            bsuf->illum = illums;
            illums += 4;

            segment->setBiasSurface(i, bsuf);
        }
    }

    LOG_INFO(String("SB_InitForMap: Done in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

void SB_SetColor(float *dest, float *src)
{
    float largest = 0;

    // Amplify the color.
    for(int i = 0; i < 3; ++i)
    {
        dest[i] = src[i];
        if(largest < dest[i])
            largest = dest[i];
    }

    if(largest > 0)
    {
        for(int i = 0; i < 3; ++i)
            dest[i] /= largest;
    }
    else
    {
        // Replace black with white.
        dest[0] = dest[1] = dest[2] = 1;
    }
}

static void SB_AddAffected(affection_t *aff, uint sourceIdx, float intensity)
{
    DENG_ASSERT(aff);

    if(aff->numFound < MAX_BIAS_AFFECTED)
    {
        aff->affected[aff->numFound].source = sourceIdx;
        aff->intensities[aff->numFound] = intensity;
        aff->numFound++;
    }
    else
    {
        // Drop the weakest.
        uint weakest = 0;

        for(uint i = 1; i < MAX_BIAS_AFFECTED; ++i)
        {
            if(aff->intensities[i] < aff->intensities[weakest])
                weakest = i;
        }

        aff->affected[weakest].source = sourceIdx;
        aff->intensities[weakest] = intensity;
    }
}

void SB_InitVertexIllum(vertexillum_t *villum)
{
    DENG_ASSERT(villum);

    villum->flags |= VIF_STILL_UNSEEN;

    for(int i = 0; i < MAX_BIAS_AFFECTED; ++i)
        villum->casted[i].source = -1;
}

void SB_SurfaceInit(BiasSurface *bsuf)
{
    DENG_ASSERT(bsuf);

    for(uint i = 0; i < bsuf->size; ++i)
    {
        SB_InitVertexIllum(&bsuf->illum[i]);
    }
}

void SB_SurfaceMoved(BiasSurface *bsuf)
{
    DENG_ASSERT(bsuf);

    for(int i = 0; i < MAX_BIAS_AFFECTED && bsuf->affected[i].source >= 0; ++i)
    {
        sources[bsuf->affected[i].source].flags |= BLF_CHANGED;
    }
}

static float SB_Dot(source_t *src, Vector3d const &point, Vector3f const &normal)
{
    DENG_ASSERT(src != 0);

    // Delta vector between source and given point.
    Vector3d delta = Vector3d(Vector3d(src->origin) - point).normalize();

    // Calculate the distance.
    return delta.dot(normal);
}

static void updateAffected(BiasSurface *bsuf, Vector2d const &from,
                           Vector2d const &to, Vector3f const &normal)
{
    DENG_ASSERT(bsuf != 0);

    // If the data is already up to date, nothing needs to be done.
    if(bsuf->updated == lastChangeOnFrame)
        return;

    bsuf->updated = lastChangeOnFrame;

    affection_t aff;
    aff.affected = bsuf->affected;
    aff.numFound = 0;
    std::memset(aff.affected, -1, sizeof(bsuf->affected));

    source_t *src = sources;
    Vector2f delta;
    for(int i = 0; i < numSources; ++i, src++)
    {
        if(src->intensity <= 0)
            continue;

        // Calculate minimum 2D distance to the segment.
        float distance = 0;
        for(int k = 0; k < 2; ++k)
        {
            if(!k)
                delta = Vector2f(from - Vector2d(src->origin));
            else
                delta = Vector2f(to - Vector2d(src->origin));

            float len = delta.length();
            if(k == 0 || len < distance)
                distance = len;
        }

        if(delta.normalize().dot(normal) >= 0)
            continue;

        if(distance < 1)
            distance = 1;

        float intensity = src->intensity / distance;

        // Is the source is too weak, ignore it entirely.
        if(intensity < biasIgnoreLimit)
            continue;

        SB_AddAffected(&aff, i, intensity);
    }
}

static void updateAffected2(BiasSurface *bsuf, struct rvertex_s const *rvertices,
    size_t numVertices, Vector3d const &point, Vector3f const &normal)
{
    DENG_ASSERT(bsuf != 0 && rvertices != 0);
    DENG_UNUSED(numVertices);

    // If the data is already up to date, nothing needs to be done.
    if(bsuf->updated == lastChangeOnFrame)
        return;

    bsuf->updated = lastChangeOnFrame;

    affection_t aff;
    aff.affected = bsuf->affected;
    aff.numFound = 0;
    std::memset(aff.affected, -1, sizeof(bsuf->affected)); // array of MAX_BIAS_AFFECTED

    source_t *src = sources;
    Vector2f delta;
    for(int i = 0; i < numSources; ++i, src++)
    {
        if(src->intensity <= 0)
            continue;

        // Calculate minimum 2D distance to the BSP leaf.
        /// @todo This is probably too accurate an estimate.
        coord_t distance = 0;
        for(uint k = 0; k < bsuf->size; ++k)
        {
            float const *vtxPos = rvertices[k].pos;
            delta = Vector2d(vtxPos[VX], vtxPos[VY]) - Vector2d(src->origin);

            float len = delta.length();
            if(k == 0 || len < distance)
                distance = len;
        }
        if(distance < 1)
            distance = 1;

        // Estimate the effect on this surface.
        float dot = SB_Dot(src, point, normal);
        if(dot <= 0)
            continue;

        float intensity = src->intensity / distance;

        // Is the source is too weak, ignore it entirely.
        if(intensity < biasIgnoreLimit)
            continue;

        SB_AddAffected(&aff, i, intensity);
    }
}

/**
 * Sets/clears a bit in the tracker for the given index.
 */
void SB_TrackerMark(biastracker_t *tracker, uint index)
{
    DENG_ASSERT(tracker);

    // Assume 32-bit uint.
    //if(index >= 0)
    {
        tracker->changes[index >> 5] |= (1 << (index & 0x1f));
    }
    /*else
    {
        tracker->changes[(-index) >> 5] &= ~(1 << ((-index) & 0x1f));
    }*/
}

/**
 * Checks if the given index bit is set in the tracker.
 */
int SB_TrackerCheck(biastracker_t *tracker, uint index)
{
    DENG_ASSERT(tracker);

    // Assume 32-bit uint.
    return (tracker->changes[index >> 5] & (1 << (index & 0x1f))) != 0;
}

/**
 * Copies changes from src to dest.
 */
void SB_TrackerApply(biastracker_t *dest, biastracker_t const *src)
{
    DENG_ASSERT(dest && src);

    for(uint i = 0; i < MAX_BIAS_TRACKED; ++i)
    {
        dest->changes[i] |= src->changes[i];
    }
}

/**
 * Clears changes of src from dest.
 */
void SB_TrackerClear(biastracker_t *dest, biastracker_t const *src)
{
    DENG_ASSERT(dest && src);

    for(uint i = 0; i < MAX_BIAS_TRACKED; ++i)
    {
        dest->changes[i] &= ~src->changes[i];
    }
}

/**
 * Tests against trackChanged.
 */
static boolean SB_ChangeInAffected(biasaffection_t *affected, biastracker_t *changed)
{
    DENG_ASSERT(affected && changed);

    for(uint i = 0; i < MAX_BIAS_AFFECTED; ++i)
    {
        if(affected[i].source < 0)
            break;

        if(SB_TrackerCheck(changed, affected[i].source))
            return true;
    }
    return false;
}

void SB_BeginFrame()
{
#ifdef DD_PROFILE
    static int i;

    if(++i > 40)
    {
        i = 0;
        PRINT_PROF( PROF_BIAS_UPDATE );
    }
#endif

    if(!useBias)
        return;

BEGIN_PROF( PROF_BIAS_UPDATE );

    // The time that applies on this frame.
    currentTimeSB = Timer_RealMilliseconds();

    // Check which sources have changed.
    biastracker_t allChanges;
    std::memset(&allChanges, 0, sizeof(allChanges));

    source_t *s = sources;
    for(int l = 0; l < numSources; ++l, s++)
    {
        if(s->sectorLevel[1] > 0 || s->sectorLevel[0] > 0)
        {
            float const minLevel = s->sectorLevel[0];
            float const maxLevel = s->sectorLevel[1];
            float const oldIntensity = s->intensity;

            Sector &sector = theMap->bspLeafAtPoint(s->origin)->sector();

            // The lower intensities are useless for light emission.
            if(sector.lightLevel() >= maxLevel)
            {
                s->intensity = s->primaryIntensity;
            }

            if(sector.lightLevel() >= minLevel && minLevel != maxLevel)
            {
                s->intensity = s->primaryIntensity *
                    (sector.lightLevel() - minLevel) / (maxLevel - minLevel);
            }
            else
            {
                s->intensity = 0;
            }

            if(s->intensity != oldIntensity)
                sources[l].flags |= BLF_CHANGED;
        }

        if(sources[l].flags & BLF_CHANGED)
        {
            SB_TrackerMark(&allChanges, l);
            sources[l].flags &= ~BLF_CHANGED;

            // This is used for interpolation.
            sources[l].lastUpdateTime = currentTimeSB;

            // Recalculate which sources affect which surfaces.
            lastChangeOnFrame = frameCount;
        }
    }

    // Apply to all surfaces.
    for(BiasSurface *bsuf = surfaces; bsuf; bsuf = bsuf->next)
    {
        SB_TrackerApply(&bsuf->tracker, &allChanges);

        // Everything that is affected by the changed lights will need an
        // update.
        if(SB_ChangeInAffected(bsuf->affected, &allChanges))
        {
            // Mark the illumination unseen to force an update.
            for(uint i = 0; i < bsuf->size; ++i)
                bsuf->illum[i].flags |= VIF_STILL_UNSEEN;
        }
    }

END_PROF( PROF_BIAS_UPDATE );
}

void SB_EndFrame()
{
    if(numSourceDelta != 0)
    {
        numSources += numSourceDelta;
        numSourceDelta = 0;
    }

    // Update the editor.
    SBE_EndFrame();
}

static void addLight(float dest[4], Vector3f const &color, float howMuch = 1.0f)
{
    DENG_ASSERT(dest != 0);

    for(int i = 0; i < 3; ++i)
    {
        float newval = dest[i] + (color[i] * howMuch);

        if(newval > 1)
            newval = 1;

        dest[i] = newval;
    }
}

void SB_RendPoly(struct ColorRawf_s *rcolors, BiasSurface *bsuf,
    struct rvertex_s const *rvertices, size_t numVertices,
    Vector3f const &surfaceNormal, float sectorLightLevel,
    de::MapElement const *mapElement, int elmIdx)
{
    DENG_ASSERT(bsuf != 0);

    // Apply sectorlight bias.  Note: Distance darkening is not used
    // with bias lights.
    if(sectorLightLevel > biasMin && biasMax > biasMin)
    {
        biasAmount = (sectorLightLevel - biasMin) / (biasMax - biasMin);

        if(biasAmount > 1)
            biasAmount = 1;
    }
    else
    {
        biasAmount = 0;
    }

    std::memcpy(&trackChanged, &bsuf->tracker, sizeof(trackChanged));
    std::memset(&trackApplied, 0, sizeof(trackApplied));

    // Has any of the old affected lights changed?
    //bool forced = false;

    if(doUpdateAffected)
    {
        /**
         * @todo This could be enhanced so that only the lights on the
         * right side of the surface are taken into consideration.
         */
        if(mapElement->type() == DMU_SEGMENT)
        {
            Segment const *segment = mapElement->castTo<Segment>();

            updateAffected(bsuf, segment->from().origin(), segment->to().origin(), surfaceNormal);
        }
        else
        {
            BspLeaf const *bspLeaf = mapElement->castTo<BspLeaf>();
            DENG_ASSERT(!bspLeaf->isDegenerate());

            Vector3d point(bspLeaf->face().center(),
                           bspLeaf->sector().plane(elmIdx).height());

            updateAffected2(bsuf, rvertices, numVertices, point, surfaceNormal);
        }
    }

/*#if _DEBUG
    // Assign primary colors rather than the real values.
    if(isHEdge)
    {
        rcolors[0].rgba[CR] = 1; rcolors[0].rgba[CG] = 0; rcolors[0].rgba[CB] = 0; rcolors[0].rgba[CA] = 1;
        rcolors[1].rgba[CR] = 0; rcolors[1].rgba[CG] = 1; rcolors[1].rgba[CB] = 0; rcolors[1].rgba[CA] = 1;
        rcolors[2].rgba[CR] = 0; rcolors[2].rgba[CG] = 0; rcolors[2].rgba[CB] = 1; rcolors[2].rgba[CA] = 1;
        rcolors[3].rgba[CR] = 1; rcolors[3].rgba[CG] = 1; rcolors[3].rgba[CB] = 0; rcolors[3].rgba[CA] = 1;
    }
    else
#endif*/
    {
        for(uint i = 0; i < numVertices; ++i)
        {
            rvertex_t const &vtx = rvertices[i];
            SB_EvalPoint(rcolors[i].rgba, &bsuf->illum[i], bsuf->affected,
                         Vector3d(vtx.pos[VX], vtx.pos[VY], vtx.pos[VZ]), surfaceNormal);
        }
    }

//    colorOverride = SB_CheckColorOverride(affected);

    SB_TrackerClear(&bsuf->tracker, &trackApplied);
}

/**
 * Interpolate between current and destination.
 */
static void lerpIllumination(vertexillum_t *illum, float *result)
{
    DENG_ASSERT(illum != 0);

    if(!(illum->flags & VIF_LERP))
    {
        // We're done with the interpolation, just use the
        // destination color.
        result[CR] = illum->color[CR];
        result[CG] = illum->color[CG];
        result[CB] = illum->color[CB];
    }
    else
    {
        float inter = (currentTimeSB - illum->updatetime) / float( lightSpeed );

        if(inter > 1)
        {
            illum->flags &= ~VIF_LERP;

            illum->color[CR] = illum->dest[CR];
            illum->color[CG] = illum->dest[CG];
            illum->color[CB] = illum->dest[CB];

            result[CR] = illum->color[CR];
            result[CG] = illum->color[CG];
            result[CB] = illum->color[CB];
        }
        else
        {
            for(uint i = 0; i < 3; ++i)
            {
                result[i] = (illum->color[i] +
                     (illum->dest[i] - illum->color[i]) * inter);
            }
        }
    }
}

/**
 * @return Light contributed by the specified source.
 */
float *SB_GetCasted(vertexillum_t *illum, int sourceIndex,
                    biasaffection_t *affectedSources)
{
    DENG_ASSERT(illum && affectedSources);

    for(int i = 0; i < MAX_BIAS_AFFECTED; ++i)
    {
        if(illum->casted[i].source == sourceIndex)
            return illum->casted[i].color;
    }

    // Choose an array element not used by the affectedSources.
    for(int i = 0; i < MAX_BIAS_AFFECTED; ++i)
    {
        bool inUse = false;
        for(int k = 0; k < MAX_BIAS_AFFECTED; ++k)
        {
            if(affectedSources[k].source < 0)
                break;

            if(affectedSources[k].source == illum->casted[i].source)
            {
                inUse = true;
                break;
            }
        }

        if(!inUse)
        {
            illum->casted[i].source = sourceIndex;
            illum->casted[i].color[CR] =
                illum->casted[i].color[CG] =
                    illum->casted[i].color[CB] = 0;

            return illum->casted[i].color;
        }
    }

    Con_Error("SB_GetCasted: No light casted by source %i.\n", sourceIndex);
    return 0;
}

static Vector3f ambientLight(Vector3d const &point)
{
    if(theMap->hasLightGrid())
        return theMap->lightGrid().evaluate(point);
    return Vector3f(0, 0, 0);
}

/**
 * Applies shadow bias to the given point.  If 'forced' is true, new
 * lighting is calculated regardless of whether the lights affecting the
 * point have changed.  This is needed when there has been world geometry
 * changes. 'illum' is allowed to be NULL.
 *
 * @todo Only recalculate the changed lights.  The colors contributed
 *        by the others can be saved with the 'affected' array.
 */
void SB_EvalPoint(float light[4], vertexillum_t *illum,
                  biasaffection_t *affectedSources, Vector3d const &point,
                  Vector3f const &normal)
{
#define COLOR_CHANGE_THRESHOLD  0.1f

    boolean illuminationChanged = false;
    uint latestSourceUpdate = 0;
    struct {
        int index;
        //uint affNum; // Index in affectedSources.
        source_t *source;
        biasaffection_t *affection;
        boolean changed;
        boolean overrider;
    } affecting[MAX_BIAS_AFFECTED + 1], *aff;

    // Vertices that are rendered for the first time need to be fully
    // evaluated.
    if(illum->flags & VIF_STILL_UNSEEN)
    {
        illuminationChanged = true;
        illum->flags &= ~VIF_STILL_UNSEEN;
    }

    // Determine if any of the affecting lights have changed since
    // last frame.
    aff = affecting;
    if(numSources > 0)
    {
        for(uint i = 0; affectedSources[i].source >= 0 && i < MAX_BIAS_AFFECTED; ++i)
        {
            int idx = affectedSources[i].source;

            // Is this a valid index?
            if(idx < 0 || idx >= numSources)
                continue;

            aff->index = idx;
            //aff->affNum = i;
            aff->source = &sources[idx];
            aff->affection = &affectedSources[i];
            aff->overrider = (aff->source->flags & BLF_COLOR_OVERRIDE) != 0;

            if(SB_TrackerCheck(&trackChanged, idx))
            {
                aff->changed = true;
                illuminationChanged = true;
                SB_TrackerMark(&trackApplied, idx);

                // Keep track of the earliest time when an affected source
                // was changed.
                if(latestSourceUpdate < sources[idx].lastUpdateTime)
                {
                    latestSourceUpdate = sources[idx].lastUpdateTime;
                }
            }
            else
            {
                aff->changed = false;
            }

            // Move to the next.
            aff++;
        }
    }
    aff->source = NULL;

    if(!illuminationChanged && illum != NULL)
    {
        // Reuse the previous value.
        lerpIllumination(illum, light);

        // Add ambient lighting.
        addLight(light, ambientLight(point));

        return;
    }

    // Init to black.
    float newColor[3] = { 0, 0, 0 };

    // Calculate the contribution from each light.
    for(aff = affecting; aff->source; aff++)
    {
        if(illum && !aff->changed) //SB_TrackerCheck(&trackChanged, aff->index))
        {
            // We can reuse the previously calculated value.  This can
            // only be done if this particular light source hasn't
            // changed.
            continue;
        }

        source_t *s = aff->source;

        float *casted = 0;
        if(illum)
            casted = SB_GetCasted(illum, aff->index, affectedSources);

        Vector3d delta = Vector3d(s->origin) - point;
        Vector3d surfacePoint = point + delta / 100;

        if(useSightCheck && !LineSightTest(s->origin, surfacePoint).trace(*theMap->bspRoot()))
        {
            // LOS fail.
            if(casted)
            {
                // This affecting source does not contribute any light.
                casted[CR] = casted[CG] = casted[CB] = 0;
            }
            continue;
        }

        double distance = delta.length();
        double dot = delta.normalize().dot(normal);

        // The surface faces away from the light.
        if(dot <= 0)
        {
            if(casted)
            {
                casted[CR] = casted[CG] = casted[CB] = 0;
            }

            continue;
        }

        float level = dot * s->intensity / distance;
        if(level > 1)
            level = 1;

        for(uint i = 0; i < 3; ++i)
        {
            // The light casted from this source.
            casted[i] =  s->color[i] * level;
        }
    }

    if(illum)
    {
        // Combine the casted light from each source.
        for(aff = affecting; aff->source; aff++)
        {
            float *casted = SB_GetCasted(illum, aff->index, affectedSources);

            for(uint i = 0; i < 3; ++i)
            {
                newColor[i] = MINMAX_OF(0, newColor[i] + casted[i], 1);
            }
        }

        // Is there a new destination?
        if(!(illum->dest[CR] < newColor[CR] + COLOR_CHANGE_THRESHOLD &&
             illum->dest[CR] > newColor[CR] - COLOR_CHANGE_THRESHOLD) ||
           !(illum->dest[CG] < newColor[CG] + COLOR_CHANGE_THRESHOLD &&
             illum->dest[CG] > newColor[CG] - COLOR_CHANGE_THRESHOLD) ||
           !(illum->dest[CB] < newColor[CB] + COLOR_CHANGE_THRESHOLD &&
             illum->dest[CB] > newColor[CB] - COLOR_CHANGE_THRESHOLD))
        {
            if(illum->flags & VIF_LERP)
            {
                // Must not lose the half-way interpolation.
                float mid[3]; lerpIllumination(illum, mid);

                // This is current color at this very moment.
                illum->color[CR] = mid[CR];
                illum->color[CG] = mid[CG];
                illum->color[CB] = mid[CB];
            }

            // This is what we will be interpolating to.
            illum->dest[CR] = newColor[CR];
            illum->dest[CG] = newColor[CG];
            illum->dest[CB] = newColor[CB];

            illum->flags |= VIF_LERP;
            illum->updatetime = latestSourceUpdate;
        }

        lerpIllumination(illum, light);
    }
    else
    {
        light[CR] = newColor[CR];
        light[CG] = newColor[CG];
        light[CB] = newColor[CB];
    }

    // Add ambient lighting.
    addLight(light, ambientLight(point));

#undef COLOR_CHANGE_THRESHOLD
}
