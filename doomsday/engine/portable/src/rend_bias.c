/**\file rend_bias.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Light/Shadow Bias.
 *
 * Calculating macro-scale lighting on the fly.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_edit.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_defs.h"
#include "de_misc.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
  PROF_BIAS_UPDATE
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

typedef struct affection_s {
    float           intensities[MAX_BIAS_AFFECTED];
    int             numFound;
    biasaffection_t* affected;
} affection_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void SB_EvalPoint(float light[4], vertexillum_t* illum, biasaffection_t* affectedSources,
    const coord_t* point, const float* normal);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int useBias = false;
int numSources = 0;
uint currentTimeSB;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static source_t sources[MAX_BIAS_LIGHTS];
static int numSourceDelta = 0;

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
static biassurface_t* surfaces = NULL;
static zblockset_t* biasSurfaceBlockSet = NULL;

// CODE --------------------------------------------------------------------

/**
 * Register console variables for Shadow Bias.
 */
void SB_Register(void)
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

static __inline biassurface_t* allocBiasSurface(void)
{
    if(biasSurfaceBlockSet)
    {   // Use the block allocator.
        biassurface_t*      bsuf = ZBlockSet_Allocate(biasSurfaceBlockSet);
        memset(bsuf, 0, sizeof(*bsuf));
        return bsuf;
    }

    return M_Calloc(sizeof(biassurface_t));
}

static __inline void freeBiasSurface(biassurface_t* bsuf)
{
    if(biasSurfaceBlockSet)
    {   // Ignore, it'll be free'd along with the block allocator.
        return;
    }

    M_Free(bsuf);
}

biassurface_t* SB_CreateSurface(void)
{
    biassurface_t*      bsuf = allocBiasSurface();

    // Link it in to the global list.
    bsuf->next = surfaces;
    surfaces = bsuf;

    return bsuf;
}

void SB_DestroySurface(biassurface_t* bsuf)
{
    if(!bsuf)
        return;

    // Unlink this surface from the global list.
    if(surfaces)
    {
        if(bsuf == surfaces)
        {
            surfaces = surfaces->next;
        }
        else if(surfaces->next)
        {
            biassurface_t*      p = surfaces->next, *last = surfaces;

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

/**
 * Creates a new bias light source and sets the appropriate properties to
 * the values of the passed parameters. The id of the new light source is
 * returned unless there are no free sources available.
 *
 * @param   x           X coordinate of the new light.
 * @param   y           Y coordinate of the new light.
 * @param   z           Z coordinate of the new light.
 * @param   size        Size (strength) of the new light.
 * @param   minLight    Lower sector light level limit.
 * @param   maxLight    Upper sector light level limit.
 * @param   rgb         Ptr to float[3], the color for the new light.
 *
 * @return              The id of the newly created bias light source else -1.
 */
int SB_NewSourceAt(coord_t x, coord_t y, coord_t z, float size, float minLight,
                   float maxLight, float* rgb)
{
    source_t*           src;

    if(numSources == MAX_BIAS_LIGHTS)
        return -1;

    src = &sources[numSources++];

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

/**
 * Same as above but for updating an existing source.
 */
void SB_UpdateSource(int which, coord_t x, coord_t y, coord_t z, float size,
    float minLight, float maxLight, float *rgb)
{
    source_t* src;

    if(which < 0 || which >= numSources) return;

    src = &sources[which];

    // Position change?
    src->origin[VX] = x;
    src->origin[VY] = y;
    src->origin[VZ] = z;

    SB_SetColor(src->color, rgb);

    src->primaryIntensity = src->intensity = size;

    src->sectorLevel[0] = minLight;
    src->sectorLevel[1] = maxLight;
}

/**
 * @return  Ptr to the bias light source for the given id.
 */
source_t* SB_GetSource(int which)
{
    return &sources[which];
}

/**
 * Convert bias light source ptr to index.
 */
int SB_ToIndex(source_t* source)
{
    if(!source)
        return -1;
    else
        return (source - sources);
}

/**
 * Removes the specified bias light source from the map.
 *
 * @param which         The id of the bias light source to be deleted.
 */
void SB_Delete(int which)
{
    int                 i;

    if(which < 0 || which >= numSources)
        return; // Very odd...

    // Do a memory move.
    for(i = which; i < numSources; ++i)
        sources[i].flags |= BLF_CHANGED;

    if(which < numSources)
        memmove(&sources[which], &sources[which + 1],
                sizeof(source_t) * (numSources - which - 1));

    sources[numSources - 1].intensity = 0;

    // Will be one fewer very soon.
    numSourceDelta--;
}

/**
 * Removes ALL bias light sources on the map.
 */
void SB_Clear(void)
{
    while(numSources-- > 0)
        sources[numSources].flags |= BLF_CHANGED;
    numSources = 0;
}

/**
 * Initializes the bias lights according to the loaded Light
 * definitions.
 */
void SB_InitForMap(const char* uniqueID)
{
    uint startTime = Sys_GetRealTime();
    ded_light_t* def;
    int i;

    assert(theMap);

    // Start with no sources whatsoever.
    numSources = 0;

    if(biasSurfaceBlockSet)
        ZBlockSet_Delete(biasSurfaceBlockSet);

    biasSurfaceBlockSet = ZBlockSet_New(sizeof(biassurface_t), 512, PU_APPSTATIC);
    surfaces = NULL;

    // Check all the loaded Light definitions for any matches.
    for(i = 0; i < defs.count.lights.num; ++i)
    {
        def = &defs.lights[i];

        if(def->state[0] || stricmp(uniqueID, def->uniqueMapID))
            continue;

        if(SB_NewSourceAt(def->offset[VX], def->offset[VY], def->offset[VZ],
                          def->size, def->lightLevel[0],
                          def->lightLevel[1], def->color) == -1)
            break;
    }

    // Create biassurfaces for all current worldmap surfaces.
    {
    size_t numVertIllums = 0;
    vertexillum_t* illums;
    uint i;

    // First, determine the total number of vertexillum_ts we need.
    for(i = 0; i < NUM_HEDGES; ++i)
    {
        HEdge* hedge = GameMap_HEdge(theMap, i);
        if(hedge->lineDef)
            numVertIllums++;
    }

    numVertIllums *= 3 * 4;

    for(i = 0; i < NUM_SECTORS; ++i)
    {
        Sector* sec = &sectors[i];
        if(sec->bspLeafs && *sec->bspLeafs)
        {
            BspLeaf** bspLeafIter = sec->bspLeafs;
            for(; *bspLeafIter; bspLeafIter++)
            {
                BspLeaf* leaf = *bspLeafIter;
                numVertIllums += Rend_NumFanVerticesForBspLeaf(leaf) * sec->planeCount;
            }
        }
    }

    for(i = 0; i < NUM_POLYOBJS; ++i)
    {
        Polyobj* po = polyObjs[i];
        numVertIllums += po->lineCount * 3 * 4;
    }

    // Allocate and initialize the vertexillum_ts.
    illums = Z_Calloc(sizeof(vertexillum_t) * numVertIllums, PU_MAP, 0);
    for(i = 0; i < numVertIllums; ++i)
        SB_InitVertexIllum(&illums[i]);

    // Allocate bias surfaces and attach vertexillum_ts.
    for(i = 0; i < NUM_HEDGES; ++i)
    {
        HEdge* hedge = GameMap_HEdge(theMap, i);
        int j;

        if(!hedge->lineDef) continue;

        for(j = 0; j < 3; ++j)
        {
            biassurface_t* bsuf = SB_CreateSurface();

            bsuf->size = 4;
            bsuf->illum = illums;
            illums += 4;

            hedge->bsuf[j] = bsuf;
        }
    }

    for(i = 0; i < NUM_SECTORS; ++i)
    {
        Sector* sec = &sectors[i];
        if(sec->bspLeafs && *sec->bspLeafs)
        {
            BspLeaf** bspLeafIter = sec->bspLeafs;
            for(; *bspLeafIter; bspLeafIter++)
            {
                BspLeaf* leaf = *bspLeafIter;
                uint j;

                for(j = 0; j < sec->planeCount; ++j)
                {
                    biassurface_t* bsuf = SB_CreateSurface();

                    bsuf->size = Rend_NumFanVerticesForBspLeaf(leaf);
                    bsuf->illum = illums;
                    illums += bsuf->size;

                    leaf->bsuf[j] = bsuf;
                }
            }
        }
    }

    for(i = 0; i < NUM_POLYOBJS; ++i)
    {
        Polyobj* po = polyObjs[i];
        uint j;

        for(j = 0; j < po->lineCount; ++j)
        {
            LineDef* line = po->lines[j];
            HEdge* hedge = line->L_frontside->hedgeLeft;
            int k;

            for(k = 0; k < 3; ++k)
            {
                biassurface_t* bsuf = SB_CreateSurface();

                bsuf->size = 4;
                bsuf->illum = illums;
                illums += 4;

                hedge->bsuf[k] = bsuf;
            }
        }
    }
    }

    VERBOSE2( Con_Message("SB_InitForMap: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) )
}

void SB_SetColor(float* dest, float* src)
{
    int                 i;
    float               largest = 0;

    // Amplify the color.
    for(i = 0; i < 3; ++i)
    {
        dest[i] = src[i];
        if(largest < dest[i])
            largest = dest[i];
    }

    if(largest > 0)
    {
        for(i = 0; i < 3; ++i)
            dest[i] /= largest;
    }
    else
    {
        // Replace black with white.
        dest[0] = dest[1] = dest[2] = 1;
    }
}

static void SB_AddAffected(affection_t* aff, uint k, float intensity)
{
    uint                i, worst;

    if(aff->numFound < MAX_BIAS_AFFECTED)
    {
        aff->affected[aff->numFound].source = k;
        aff->intensities[aff->numFound] = intensity;
        aff->numFound++;
    }
    else
    {
        // Drop the weakest.
        worst = 0;
        for(i = 1; i < MAX_BIAS_AFFECTED; ++i)
        {
            if(aff->intensities[i] < aff->intensities[worst])
                worst = i;
        }

        aff->affected[worst].source = k;
        aff->intensities[worst] = intensity;
    }
}

void SB_InitVertexIllum(vertexillum_t* villum)
{
    int                 i;

    villum->flags |= VIF_STILL_UNSEEN;

    for(i = 0; i < MAX_BIAS_AFFECTED; ++i)
        villum->casted[i].source = -1;
}

void SB_SurfaceInit(biassurface_t* bsuf)
{
    uint i;
    for(i = 0; i < bsuf->size; ++i)
    {
        SB_InitVertexIllum(&bsuf->illum[i]);
    }
}

void SB_SurfaceMoved(biassurface_t* bsuf)
{
    int i;
    for(i = 0; i < MAX_BIAS_AFFECTED && bsuf->affected[i].source >= 0; ++i)
    {
        sources[bsuf->affected[i].source].flags |= BLF_CHANGED;
    }
}

static float SB_Dot(source_t* src, const coord_t* point, const vectorcompf_t* normal)
{
    coord_t delta[3];

    // Delta vector between source and given point.
    V3d_Subtract(delta, src->origin, point);

    // Calculate the distance.
    V3d_Normalize(delta);

    return V3d_DotProductf(delta, normal);
}

static void updateAffected(biassurface_t* bsuf, coord_t const from[2],
                           coord_t const to[2], const vectorcompf_t* normal)
{
    int i, k;
    vec2f_t delta;
    source_t* src;
    float distance = 0, len;
    float intensity;
    affection_t aff;

    // If the data is already up to date, nothing needs to be done.
    if(bsuf->updated == lastChangeOnFrame)
        return;

    bsuf->updated = lastChangeOnFrame;
    aff.affected = bsuf->affected;
    aff.numFound = 0;
    memset(aff.affected, -1, sizeof(bsuf->affected));

    for(k = 0, src = sources; k < numSources; ++k, ++src)
    {
        if(src->intensity <= 0)
            continue;

        // Calculate minimum 2D distance to the hedge.
        for(i = 0; i < 2; ++i)
        {
            if(!i)
                V2f_Set(delta, from[VX] - src->origin[VX],
                               from[VY] - src->origin[VY]);
            else
                V2f_Set(delta, to[VX] - src->origin[VX],
                               to[VY] - src->origin[VY]);
            len = V2f_Normalize(delta);

            if(i == 0 || len < distance)
                distance = len;
        }

        if(V3f_DotProduct(delta, normal) >= 0)
            continue;

        if(distance < 1)
            distance = 1;

        intensity = src->intensity / distance;

        // Is the source is too weak, ignore it entirely.
        if(intensity < biasIgnoreLimit)
            continue;

        SB_AddAffected(&aff, k, intensity);
    }
}

static void updateAffected2(biassurface_t* bsuf, const struct rvertex_s* rvertices,
    size_t numVertices, const coord_t* point, const vectorcompf_t* normal)
{
    int i;
    uint k;
    coord_t delta[2];
    source_t* src;
    coord_t distance = 0, len;
    float dot, intensity;
    affection_t aff;

    // If the data is already up to date, nothing needs to be done.
    if(bsuf->updated == lastChangeOnFrame)
        return;

    bsuf->updated = lastChangeOnFrame;
    aff.affected = bsuf->affected;
    aff.numFound = 0;
    memset(aff.affected, -1, sizeof(aff.affected));

    for(i = 0, src = sources; i < numSources; ++i, ++src)
    {
        if(src->intensity <= 0)
            continue;

        // Calculate minimum 2D distance to the BSP leaf.
        /// @fixme This is probably too accurate an estimate.
        for(k = 0; k < bsuf->size; ++k)
        {
            V2d_Set(delta, rvertices[k].pos[VX] - src->origin[VX],
                           rvertices[k].pos[VY] - src->origin[VY]);
            len = V2d_Length(delta);

            if(k == 0 || len < distance)
                distance = len;
        }
        if(distance < 1)
            distance = 1;

        // Estimate the effect on this surface.
        dot = SB_Dot(src, point, normal);
        if(dot <= 0)
            continue;

        intensity = /*dot * */ src->intensity / distance;

        // Is the source is too weak, ignore it entirely.
        if(intensity < biasIgnoreLimit)
            continue;

        SB_AddAffected(&aff, i, intensity);
    }
}

/**
 * Sets/clears a bit in the tracker for the given index.
 */
void SB_TrackerMark(biastracker_t* tracker, uint index)
{
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
int SB_TrackerCheck(biastracker_t* tracker, uint index)
{
    // Assume 32-bit uint.
    return (tracker->changes[index >> 5] & (1 << (index & 0x1f))) != 0;
}

/**
 * Copies changes from src to dest.
 */
void SB_TrackerApply(biastracker_t* dest, const biastracker_t* src)
{
    uint i;

    for(i = 0; i < MAX_BIAS_TRACKED; ++i)
    {
        dest->changes[i] |= src->changes[i];
    }
}

/**
 * Clears changes of src from dest.
 */
void SB_TrackerClear(biastracker_t* dest, const biastracker_t* src)
{
    uint i;

    for(i = 0; i < MAX_BIAS_TRACKED; ++i)
    {
        dest->changes[i] &= ~src->changes[i];
    }
}

/**
 * Tests against trackChanged.
 */
static boolean SB_ChangeInAffected(biasaffection_t* affected,
                                   biastracker_t* changed)
{
    uint i;

    for(i = 0; i < MAX_BIAS_AFFECTED; ++i)
    {
        if(affected[i].source < 0)
            break;

        if(SB_TrackerCheck(changed, affected[i].source))
            return true;
    }
    return false;
}

/**
 * Do initial processing that needs to be done before rendering a
 * frame.  Changed lights cause the tracker bits to the set for all
 * hedges and planes.
 */
void SB_BeginFrame(void)
{
    int                 l;
    source_t*           s;
    biastracker_t       allChanges;

#ifdef DD_PROFILE
    static int          i;

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
    currentTimeSB = Sys_GetRealTime();

    // Check which sources have changed.
    memset(&allChanges, 0, sizeof(allChanges));
    for(l = 0, s = sources; l < numSources; ++l, ++s)
    {
        if(s->sectorLevel[1] > 0 || s->sectorLevel[0] > 0)
        {
            float minLevel = s->sectorLevel[0];
            float maxLevel = s->sectorLevel[1];
            Sector* sector;
            float oldIntensity = s->intensity;

            sector = P_BspLeafAtPoint(s->origin)->sector;

            // The lower intensities are useless for light emission.
            if(sector->lightLevel >= maxLevel)
            {
                s->intensity = s->primaryIntensity;
            }

            if(sector->lightLevel >= minLevel && minLevel != maxLevel)
            {
                s->intensity = s->primaryIntensity *
                    (sector->lightLevel - minLevel) / (maxLevel - minLevel);
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
    {
    biassurface_t*      bsuf = surfaces;

    for(bsuf = surfaces; bsuf; bsuf = bsuf->next)
    {
        SB_TrackerApply(&bsuf->tracker, &allChanges);

        // Everything that is affected by the changed lights will need an
        // update.
        if(SB_ChangeInAffected(bsuf->affected, &allChanges))
        {
            uint                i;

            // Mark the illumination unseen to force an update.
            for(i = 0; i < bsuf->size; ++i)
                bsuf->illum[i].flags |= VIF_STILL_UNSEEN;
        }
    }
    }

END_PROF( PROF_BIAS_UPDATE );
}

void SB_EndFrame(void)
{
    if(numSourceDelta != 0)
    {
        numSources += numSourceDelta;
        numSourceDelta = 0;
    }

    // Update the editor.
    SBE_EndFrame();
}

void SB_AddLight(float dest[4], const float *color, float howMuch)
{
    int                 i;
    float               newval, amplified[3], largest = 0;

    if(color == NULL)
    {
        for(i = 0; i < 3; ++i)
        {
            amplified[i] = dest[i];
            if(i == 0 || amplified[i] > largest)
                largest = amplified[i];
        }

        if(largest == 0) // Black!
        {
            amplified[0] = amplified[1] = amplified[2] = 1;
        }
        else
        {
            for(i = 0; i < 3; ++i)
            {
                amplified[i] = amplified[i] / largest;
            }
        }
    }

    for(i = 0; i < 3; ++i)
    {
        newval = dest[i] + ((color ? color : amplified)[i] * howMuch);

        if(newval > 1)
            newval = 1;

        dest[i] = newval;
    }
}

#if 0
/**
 * Color override forces the bias light color to override biased
 * sectorlight.
 */
static boolean SB_CheckColorOverride(biasaffection_t *affected)
{
    int                 i;

    for(i = 0; affected[i].source >= 0 && i < MAX_BIAS_AFFECTED; ++i)
    {
        // If the color is completely black, it means no light was
        // reached from this affected source.
        if(!(affected[i].rgb[0] | affected[i].rgb[1] | affected[i].rgb[2]))
            continue;

        if(sources[affected[i].source].flags & BLF_COLOR_OVERRIDE)
            return true;
    }

    return false;
}
#endif

/**
 * Surface can be a either a wall or a plane (ceiling or a floor).
 *
 * @param colors        Array of colors to be written to.
 * @param bsuf          Bias data for this surface.
 * @param vertices      Array of vertices to be lit.
 * @param numVertices   Number of vertices (in the array) to be lit.
 * @param normal        Surface normal.
 * @param sectorLightLevel Sector light level.
 * @param mapObject     Ptr to either a HEdge or BspLeaf.
 * @param elmIdx        Used with BspLeafs to select a specific plane.
 * @param isHEdge       @c true, if @a mapObject is a HEdge ELSE a BspLeaf.
 */
void SB_RendPoly(struct ColorRawf_s* rcolors, biassurface_t* bsuf,
                 const struct rvertex_s* rvertices,
                 size_t numVertices, const vectorcompf_t* normal,
                 float sectorLightLevel,
                 void* mapObject, uint elmIdx, boolean isHEdge)
{
    uint                i;
    boolean             forced;

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

    memcpy(&trackChanged, &bsuf->tracker, sizeof(trackChanged));
    memset(&trackApplied, 0, sizeof(trackApplied));

    // Has any of the old affected lights changed?
    forced = false;

    if(doUpdateAffected)
    {
        /**
         * @todo This could be enhanced so that only the lights on the
         * right side of the surface are taken into consideration.
         */
        if(isHEdge)
        {
            HEdge* hedge = (HEdge*) mapObject;

            updateAffected(bsuf, hedge->HE_v1origin, hedge->HE_v2origin, normal);
        }
        else
        {
            BspLeaf* bspLeaf = (BspLeaf*) mapObject;
            vec3d_t point;

            V3d_Set(point, bspLeaf->midPoint[VX], bspLeaf->midPoint[VY],
                           bspLeaf->sector->planes[elmIdx]->height);

            updateAffected2(bsuf, rvertices, numVertices, point, normal);
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
        vec3d_t point;
        for(i = 0; i < numVertices; ++i)
        {
            V3d_Copyf(point, rvertices[i].pos);
            SB_EvalPoint(rcolors[i].rgba, &bsuf->illum[i], bsuf->affected, point, normal);
        }
    }

//    colorOverride = SB_CheckColorOverride(affected);

    SB_TrackerClear(&bsuf->tracker, &trackApplied);
}

/**
 * Interpolate between current and destination.
 */
void SB_LerpIllumination(vertexillum_t* illum, float* result)
{
    uint                i;

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
        float               inter =
            (currentTimeSB - illum->updatetime) / (float)lightSpeed;

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
            for(i = 0; i < 3; ++i)
            {
                result[i] = (illum->color[i] +
                     (illum->dest[i] - illum->color[i]) * inter);
            }
        }
    }
}

/**
 * @return              Light contributed by the specified source.
 */
float* SB_GetCasted(vertexillum_t* illum, int sourceIndex,
                    biasaffection_t* affectedSources)
{
    int                 i, k;
    boolean             inUse;

    for(i = 0; i < MAX_BIAS_AFFECTED; ++i)
        if(illum->casted[i].source == sourceIndex)
            return illum->casted[i].color;

    // Choose an array element not used by the affectedSources.
    for(i = 0; i < MAX_BIAS_AFFECTED; ++i)
    {
        inUse = false;
        for(k = 0; k < MAX_BIAS_AFFECTED; ++k)
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
    return NULL;
}

/**
 * Add ambient light.
 */
void SB_AmbientLight(coord_t const* point, float* light)
{
    // Add grid light (represents ambient lighting).
    float color[3];

    LG_Evaluate(point, color);
    SB_AddLight(light, color, 1.0f);
}

/**
 * Applies shadow bias to the given point.  If 'forced' is true, new
 * lighting is calculated regardless of whether the lights affecting the
 * point have changed.  This is needed when there has been world geometry
 * changes. 'illum' is allowed to be NULL.
 *
 * @fixme Only recalculate the changed lights.  The colors contributed
 *        by the others can be saved with the 'affected' array.
 */
void SB_EvalPoint(float light[4], vertexillum_t* illum,
                  biasaffection_t* affectedSources, const coord_t* point,
                  const float* normal)
{
#define COLOR_CHANGE_THRESHOLD  0.1f

    float               newColor[3];
    float               dot;
    coord_t             delta[3], surfacePoint[3];
    coord_t             distance;
    float               level;
    uint                i;
    int                 idx;
    boolean             illuminationChanged = false;
    unsigned int        latestSourceUpdate = 0;
    source_t*           s;
    float*              casted;
    struct {
        int              index;
        //uint           affNum; // Index in affectedSources.
        source_t*        source;
        biasaffection_t* affection;
        boolean          changed;
        boolean          overrider;
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
        for(i = 0;
            affectedSources[i].source >= 0 && i < MAX_BIAS_AFFECTED; ++i)
        {
            idx = affectedSources[i].source;

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
        SB_LerpIllumination(illum, light);
        SB_AmbientLight(point, light);
        return;
    }

    // Init to black.
    newColor[CR] = newColor[CG] = newColor[CB] = 0;

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

        s = aff->source;
        if(illum)
            casted = SB_GetCasted(illum, aff->index, affectedSources);
        else
            casted = NULL;

        V3d_Subtract(delta, s->origin, point);
        V3d_Copy(surfacePoint, delta);
        V3d_Scale(surfacePoint, 1.f / 100);
        V3d_Sum(surfacePoint, surfacePoint, point);

        if(useSightCheck && !P_CheckLineSight(s->origin, surfacePoint, -1, 1, 0))
        {
            // LOS fail.
            if(casted)
            {
                // This affecting source does not contribute any light.
                casted[CR] = casted[CG] = casted[CB] = 0;
            }

            continue;
        }
        else
        {
            distance = V3d_Normalize(delta);
            dot = V3d_DotProductf(delta, normal);

            // The surface faces away from the light.
            if(dot <= 0)
            {
                if(casted)
                {
                    casted[CR] = casted[CG] = casted[CB] = 0;
                }

                continue;
            }

            level = dot * s->intensity / distance;
        }

        if(level > 1)
            level = 1;

        for(i = 0; i < 3; ++i)
        {   // The light casted from this source.
            casted[i] =  s->color[i] * level;
        }

        // Are we already fully lit?
        /*if(!(newColor[CR] < 1 && newColor[CG] < 1 && new.rgba[2] < 1))
            break;*/
    }

    if(illum)
    {
        //Con_Message("\n");

        boolean             willOverride = false;

        // Combine the casted light from each source.
        for(aff = affecting; aff->source; aff++)
        {
            float              *casted =
                SB_GetCasted(illum, aff->index, affectedSources);

            if(aff->overrider &&
               (casted[CR] > 0 || casted[CG] > 0 || casted[CB] > 0))
                willOverride = true;

/*          if(!(casted[3] > 0))
            {
                int n;
                Con_Message("affected: ");
                for(n = 0; n < MAX_BIAS_AFFECTED; ++n)
                    Con_Message("%i ", affectedSources[n].source);
                Con_Message("\n");
                Con_Error("not updated: s=%i\n", aff->index);
            }
*/
/*            if(editSelector >= 0 && aff->index != editSelector)
              continue;*/


/*            {
                int n;
                printf("affected: ");
                for(n = 0; n < MAX_BIAS_AFFECTED; ++n)
                    printf("%i ", affectedSources[n].source);
                printf("casted: ");
                for(n = 0; n < MAX_BIAS_AFFECTED; ++n)
                    printf("%i ", illum->casted[n].source);
                printf("%i:(%g %g %g) ",
                            aff->index, casted[CR], casted[CG], casted[CB]);
                printf("\n");
                }*/

            for(i = 0; i < 3; ++i)
            {
                newColor[i] = MINMAX_OF(0, newColor[i] + casted[i], 1);
            }
        }

        /*if(biasAmount > 0)
        {
            SB_AddLight(&new, willOverride ? NULL : biasColor, biasAmount);
            }*/

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
                float           mid[3];

                SB_LerpIllumination(illum, mid);

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

        SB_LerpIllumination(illum, light);
    }
    else
    {
        light[CR] = newColor[CR];
        light[CG] = newColor[CG];
        light[CB] = newColor[CB];
    }

    SB_AmbientLight(point, light);

#undef COLOR_CHANGE_THRESHOLD
}
