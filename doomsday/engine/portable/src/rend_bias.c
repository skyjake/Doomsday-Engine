/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
 * rend_bias.c: Light/Shadow Bias
 *
 * Calculating macro-scale lighting on the fly.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_edit.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_defs.h"
#include "de_misc.h"
#include "p_sight.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct affection_s {
    float           intensities[MAX_BIAS_AFFECTED];
    int             numFound;
    biasaffection_t *affected;
} affection_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void         SB_EvalPoint(gl_rgba_t *light,
                          vertexillum_t *illum,
                          biasaffection_t *affectedSources,
                          const float *point, float *normal);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int useBias = false;
uint numSources = 0;
unsigned int currentTimeSB;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static source_t sources[MAX_BIAS_LIGHTS];
static int numSourceDelta = 0;

static int useSightCheck = true;
static float biasMin = 0.85f;
static float biasMax = 1.0f;
static int updateAffected = true;
static float biasIgnoreLimit = .005f;
static int lightSpeed = 130;
static unsigned int lastChangeOnFrame;

/*
 * BS_EvalPoint uses these, so they must be set before it is called.
 */
static biastracker_t trackChanged;
static biastracker_t trackApplied;

//static float biasColor[3];
static float biasAmount;

// CODE --------------------------------------------------------------------

/*
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

    C_VAR_INT("rend-dev-bias-affected", &updateAffected, CVF_NO_ARCHIVE, 0, 1);

/*    C_VAR_INT("rend-dev-bias-solo", &editSelector, CVF_NO_ARCHIVE, -1, 255);*/
}

/*
 * Creates a new bias light source and sets the appropriate properties
 * to the values of the passed parameters. The id of the new light source
 * is returned unless there are no free sources available.
 *
 * @param   x           X coordinate of the new light.
 * @param   y           Y coordinate of the new light.
 * @param   z           Z coordinate of the new light.
 * @param   size        Size (strength) of the new light.
 * @param   minLight    Lower sector light level limit.
 * @param   maxLight    Upper sector light level limit.
 * @param   rgb         Ptr to float[3], the color for the new light.
 *
 * @return int          The id of the newly created bias light source else 0.
 */
uint SB_NewSourceAt(float x, float y, float z, float size, float minLight,
                    float maxLight, float *rgb)
{
    source_t *src;

    if(numSources == MAX_BIAS_LIGHTS)
        return 0;

    src = &sources[numSources++];

    // New lights are automatically locked.
    src->flags = BLF_CHANGED | BLF_LOCKED;

    src->pos[VX] = x;
    src->pos[VY] = y;
    src->pos[VZ] = z;

    SB_SetColor(src->color,rgb);

    src->primaryIntensity = src->intensity = size;

    src->sectorLevel[0] = minLight;
    src->sectorLevel[1] = maxLight;

    // This'll enforce an update (although the vertices are also
    // STILL_UNSEEN).
    src->lastUpdateTime = 0;

    return numSources; // == index + 1;
}

/*
 * Same as above really but for updating an existing source.
 */
void SB_UpdateSource(uint which, float x, float y, float z, float size,
                     float minLight, float maxLight, float *rgb)
{
    source_t *src;

    if(which >= numSources)
        return;

    src = &sources[which];

    // Position change?
    src->pos[VX] = x;
    src->pos[VY] = y;
    src->pos[VZ] = z;

    SB_SetColor(src->color, rgb);

    src->primaryIntensity = src->intensity = size;

    src->sectorLevel[0] = minLight;
    src->sectorLevel[1] = maxLight;
}

/*
 * Return a ptr to the bias light source by id.
 */
source_t *SB_GetSource(int which)
{
    return &sources[which];
}

/*
 * Convert bias light source ptr to index.
 */
uint SB_ToIndex(source_t* source)
{
    if(!source)
        return 0;
    else
        return (source - sources) + 1;
}

/*
 * Removes the specified bias light source from the level.
 *
 * @param which         The id of the bias light source to be deleted.
 */
void SB_Delete(uint which)
{
    uint        i;
    uint        j;
    sector_t   *seciter;
    boolean     done;

    if(which > numSources)
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

    // Check to see if a mobj had acquired this source for it's own use.
    // You never know what trick the user might be trying pull :-)
    done = false;
    for(j = 0; j < numsectors && !done; ++j)
    {
        mobj_t *iter;

        seciter = SECTOR_PTR(j);
        for(iter = seciter->thinglist; iter && !done; iter = iter->snext)
        {
            if(iter->usingBias && iter->light == which + 1)
            {
                iter->light = 0;
                iter->usingBias = false;

                done = true;
            }
        }
    }
}

/*
 * Removes ALL bias light sources on the level.
 */
void SB_Clear(void)
{
    uint        i;
    sector_t   *seciter;

    if(!numSources)
        return;

    while(numSources-- > 0)
        sources[numSources].flags |= BLF_CHANGED;
    numSources = 0;

    // Zero Bias source indices acquired by mobjs.
    for(i = 0; i < numsectors; ++i)
    {
        mobj_t *iter;

        seciter = SECTOR_PTR(i);
        for(iter = seciter->thinglist; iter; iter = iter->snext)
        {
            if(iter->usingBias)
            {
                iter->light = 0;
                iter->usingBias = false;
            }
        }
    }
}

/**
 * Initializes the bias lights according to the loaded Light
 * definitions.
 */
void SB_InitForLevel(const char *uniqueId)
{
    ded_light_t *def;
    int         i;

    // Start with no sources whatsoever.
    numSources = 0;

    // Check all the loaded Light definitions for any matches.
    for(i = 0; i < defs.count.lights.num; ++i)
    {
        def = &defs.lights[i];

        if(def->state[0] || stricmp(uniqueId, def->level))
            continue;

        if(SB_NewSourceAt(def->offset[VX], def->offset[VY], def->offset[VZ],
                          def->size, def->lightlevel[0],
                          def->lightlevel[1], def->color) == 0)
            break;
    }
}

void SB_SetColor(float *dest, float *src)
{
    float       largest = 0;
    int         i;

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

/**
 * Conversion from HSV to RGB.  Everything is [0,1].
 */
void HSVtoRGB(float *rgb, float h, float s, float v)
{
    int         i;
    float       f, p, q, t;

    if(s == 0)
    {
        // achromatic (grey)
        rgb[0] = rgb[1] = rgb[2] = v;
        return;
    }

    if(h >= 1)
        h -= 1;

    h *= 6;                        // sector 0 to 5
    i = floor(h);
    f = h - i;                     // factorial part of h
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));

    switch(i)
    {
    case 0:
        rgb[0] = v;
        rgb[1] = t;
        rgb[2] = p;
        break;

    case 1:
        rgb[0] = q;
        rgb[1] = v;
        rgb[2] = p;
        break;

    case 2:
        rgb[0] = p;
        rgb[1] = v;
        rgb[2] = t;
        break;

    case 3:
        rgb[0] = p;
        rgb[1] = q;
        rgb[2] = v;
        break;

    case 4:
        rgb[0] = t;
        rgb[1] = p;
        rgb[2] = v;
        break;

    default:
        rgb[0] = v;
        rgb[1] = p;
        rgb[2] = q;
        break;
    }
}

static void SB_AddAffected(affection_t *aff, uint k, float intensity)
{
    uint        i, worst;

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

/**
 * This must be called when a plane that the seg touches is moved, or
 * when a seg in a polyobj changes position.
 */
void SB_SegHasMoved(struct seg_s *seg)
{
    int         i;

    // Mark the affected lights changed.
    for(i = 0; i < MAX_BIAS_AFFECTED && seg->affected[i].source >= 0; ++i)
    {
        sources[seg->affected[i].source].flags |= BLF_CHANGED;
    }
}

/**
 * This must be called when a plane has moved.  Set 'theCeiling' to
 * true if the plane is a ceiling plane.
 */
void SB_PlaneHasMoved(subsector_t *subsector, uint plane)
{
    int         i;
    subplaneinfo_t *info = subsector->planes[plane];

    // Mark the affected lights changed.
    for(i = 0; i < MAX_BIAS_AFFECTED && info->affected[i].source >= 0; ++i)
    {
        sources[info->affected[i].source].flags |= BLF_CHANGED;
    }
}

/**
 * This could be enhanced so that only the lights on the right side of
 * the seg are taken into consideration.
 */
void SB_UpdateSegAffected(int segId, rendpoly_t *poly)
{
    seg_t      *seg = SEG_PTR(segId);
    int         i;
    uint        k;
    vec2_t      delta;
    source_t   *src;
    float       distance = 0, len;
    float       intensity;
    affection_t aff;

    // If the data is already up to date, nothing needs to be done.
    if(seg->updated == lastChangeOnFrame || !updateAffected)
        return;

    seg->updated = lastChangeOnFrame;
    aff.affected = seg->affected;
    aff.numFound = 0;
    memset(aff.affected, -1, sizeof(seg->affected));

    for(k = 0, src = sources; k < numSources; ++k, ++src)
    {
        if(src->intensity <= 0)
            continue;

        // Calculate minimum 2D distance to the seg.
        for(i = 0; i < 2; ++i)
        {
            V2_Set(delta,
                   poly->vertices[i*2].pos[VX] - src->pos[VX],
                   poly->vertices[i*2].pos[VY] - src->pos[VY]);
            len = V2_Normalize(delta);

            if(i == 0 || len < distance)
                distance = len;
        }

        if(M_DotProduct(delta, seg->sidedef->SW_middlenormal) >= 0)
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

static float SB_Dot(source_t *src, float point[3], float normal[3])
{
    float       delta[3];
    int         i;

    // Delta vector between source and given point.
    for(i = 0; i < 3; ++i)
        delta[i] = src->pos[i] - point[i];

    // Calculate the distance.
    M_Normalize(delta);

    return M_DotProduct(delta, normal);
}

/**
 * This could be enhanced so that only the lights on the right side of
 * the plane are taken into consideration.
 */
void SB_UpdateSubsectorAffected(uint sub, rendpoly_t *poly)
{
    subsector_t *subsector = SUBSECTOR_PTR(sub);
    uint        i, k;
    vec2_t      delta;
    float       point[3];
    source_t   *src;
    float       distance = 0, len, dot;
    float       intensity;
    affection_t *aff;

    // If the data is already up to date, nothing needs to be done.
    if(subsector->planes[PLN_FLOOR]->updated == lastChangeOnFrame || !updateAffected)
        return;

    //// \fixme NOT optimal.
    aff = M_Calloc(subsector->sector->planecount * sizeof(affection_t));

    // For each plane.
    for(k = 0; k < subsector->sector->planecount; ++k)
    {
        subsector->planes[k]->updated = lastChangeOnFrame;
        aff[k].affected = subsector->planes[k]->affected;
        aff[k].numFound = 0;
        memset(aff[k].affected, -1, sizeof(subsector->planes[k]->affected));
    }

    for(i = 0, src = sources; i < numSources; ++i, ++src)
    {
        if(src->intensity <= 0)
            continue;

        //// Calculate minimum 2D distance to the subsector.
        //// \fixme This is probably too accurate an estimate.
        for(k = 0; k < poly->numvertices; ++k)
        {
            V2_Set(delta,
                   poly->vertices[k].pos[VX] - src->pos[VX],
                   poly->vertices[k].pos[VY] - src->pos[VY]);
            len = V2_Length(delta);

            if(k == 0 || len < distance)
                distance = len;
        }
        if(distance < 1)
            distance = 1;

        // For each plane
        for(k = 0; k < subsector->sector->planecount; ++k)
        {
            // Estimate the effect on this plane.
            point[VX] = subsector->midpoint.pos[VX];
            point[VY] = subsector->midpoint.pos[VY];
            point[VZ] = subsector->sector->planes[k]->height;

            dot = SB_Dot(src, point, subsector->sector->planes[k]->surface.normal);
            if(dot <= 0)
                continue;

            intensity = /*dot * */ src->intensity / distance;

            // Is the source is too weak, ignore it entirely.
            if(intensity < biasIgnoreLimit)
                continue;

            SB_AddAffected(&aff[k], i, intensity);
        }
    }

    M_Free(aff);
}

/**
 * Sets/clears a bit in the tracker for the given index.
 */
void SB_TrackerMark(biastracker_t *tracker, int index)
{
    if(index >= 0)
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
int SB_TrackerCheck(biastracker_t *tracker, int index)
{
    return (tracker->changes[index >> 5] & (1 << (index & 0x1f))) != 0;
}

/**
 * Copies changes from src to dest.
 */
void SB_TrackerApply(biastracker_t *dest, const biastracker_t *src)
{
    unsigned int i;

    for(i = 0; i < sizeof(dest->changes)/sizeof(dest->changes[0]); ++i)
    {
        dest->changes[i] |= src->changes[i];
    }
}

/**
 * Clears changes of src from dest.
 */
void SB_TrackerClear(biastracker_t *dest, const biastracker_t *src)
{
    unsigned int i;

    for(i = 0; i < sizeof(dest->changes)/sizeof(dest->changes[0]); ++i)
    {
        dest->changes[i] &= ~src->changes[i];
    }
}

/**
 * Tests against trackChanged.
 */
static boolean SB_ChangeInAffected(biasaffection_t *affected,
                                   biastracker_t *changed)
{
    uint        i;

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
 * This is done in the beginning of the frame when a light source has
 * changed.  The planes that the change affects will need to be
 * re-evaluated.
 */
void SB_MarkPlaneChanges(subsector_t *ssec, uint plane,
                         biastracker_t *allChanges)
{
    uint        i;
    subplaneinfo_t *pinfo = ssec->planes[plane];

    SB_TrackerApply(&pinfo->tracker, allChanges);

    if(SB_ChangeInAffected(pinfo->affected, allChanges))
    {
        // Mark the illumination unseen to force an update.
        for(i = 0; i < ssec->numvertices; ++i)
            pinfo->illumination[i].flags |= VIF_STILL_UNSEEN;
    }
}

/**
 * Do initial processing that needs to be done before rendering a
 * frame.  Changed lights cause the tracker bits to the set for all
 * segs and planes.
 */
void SB_BeginFrame(void)
{
    biastracker_t allChanges;
    uint        i, j, k, l;
    subsector_t *sub;
    source_t   *s;

    if(!useBias)
        return;

    // The time that applies on this frame.
    currentTimeSB = Sys_GetRealTime();

    // Check which sources have changed.
    memset(&allChanges, 0, sizeof(allChanges));
    for(l = 0, s = sources; l < numSources; ++l, ++s)
    {
        if(s->sectorLevel[1] > 0 || s->sectorLevel[0] > 0)
        {
            float   minLevel = s->sectorLevel[0];
            float   maxLevel = s->sectorLevel[1];
            sector_t *sector = R_PointInSubsector
                (FRACUNIT * s->pos[VX], FRACUNIT * s->pos[VY])->sector;
            float oldIntensity = s->intensity;

            // The lower intensities are useless for light emission.
            if(sector->lightlevel >= maxLevel)
            {
                s->intensity = s->primaryIntensity;
            }
            if(sector->lightlevel >= minLevel && minLevel != maxLevel)
            {
                s->intensity = s->primaryIntensity *
                    (sector->lightlevel - minLevel) / (maxLevel - minLevel);
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
            lastChangeOnFrame = framecount;
        }
    }

    // Apply to all segs.
    for(i = 0; i < numsegs; ++i)
    {
        seg_t *seg = SEG_PTR(i);

        for(j = 0; j < 3; ++j)
            SB_TrackerApply(&seg->tracker[j], &allChanges);

        // Everything that is affected by the changed lights will need
        // an update.
        if(SB_ChangeInAffected(seg->affected, &allChanges))
        {
            // Mark the illumination unseen to force an update.
            for(j = 0; j < 3; ++j)
                for(k = 0; k < 4; ++k)
                    seg->illum[j][k].flags |= VIF_STILL_UNSEEN;
        }
    }

    // Apply to all planes.
    for(i = 0; i < numsubsectors; ++i)
    {
        sub = SUBSECTOR_PTR(i);

        for(j = 0; j < sub->sector->planecount; ++j)
            SB_MarkPlaneChanges(sub, j, &allChanges);
    }
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

void SB_AddLight(gl_rgba_t *dest, const float *color, float howMuch)
{
    int         i, newval;
    float       amplified[3], largest = 0;

    if(color == NULL)
    {
        for(i = 0; i < 3; ++i)
        {
            amplified[i] = dest->rgba[i] * reciprocal255;
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
        newval = dest->rgba[i] + (byte)
            (255 * (color ? color : amplified)[i] * howMuch);

        if(newval > 255)
            newval = 255;

        dest->rgba[i] = newval;
    }
}

#if 0
/**
 * Color override forces the bias light color to override biased
 * sectorlight.
 */
static boolean SB_CheckColorOverride(biasaffection_t *affected)
{
/*    int i;

    for(i = 0; affected[i].source >= 0 && i < MAX_BIAS_AFFECTED; ++i)
    {
        // If the color is completely black, it means no light was
        // reached from this affected source.
        if(!(affected[i].rgb[0] | affected[i].rgb[1] | affected[i].rgb[2]))
            continue;

        if(sources[affected[i].source].flags & BLF_COLOR_OVERRIDE)
            return true;
            }*/
    return false;
}
#endif

/**
 * Poly can be a either a wall or a plane (ceiling or a floor).
 *
 * Parameters:
 *
 * poly             Rendering polygon (wall or plane).
 * illumination     Illumination for each corner of the polygon.
 * tracker          Tracker of all the changed lights for this polygon.
 * mapElementIndex  Index of the seg or subsector in the global arrays.
 */
void SB_RendPoly(struct rendpoly_s *poly, struct surface_s *surface,
                 struct sector_s *sector, vertexillum_t *illumination,
                 biastracker_t *tracker, biasaffection_t *affected,
                 uint mapElementIndex)
{
    uint        i;
    boolean     forced;

    if(!useBias)
        return;

#if 1
    // Apply sectorlight bias.  Note: Distance darkening is not used
    // with bias lights.
    if(sector->lightlevel > biasMin && biasMax > biasMin)
    {
        biasAmount = (sector->lightlevel - biasMin) / (biasMax - biasMin);

        if(biasAmount > 1)
            biasAmount = 1;

//        memcpy(biasColor, R_GetSectorLightColor(sector), sizeof(float) * 3);

/*            // Planes and the top edge of walls.
            SB_AddLight(&poly->vertices[i].color,
                        colorOverride ? NULL : sectorColor, applied);
            }*/
    }
    else
    {
        biasAmount = 0;
    }
#endif


    memcpy(&trackChanged, tracker, sizeof(trackChanged));
    memset(&trackApplied, 0, sizeof(trackApplied));

    // Has any of the old affected lights changed?
    forced = false;

    if(poly->isWall)
        SB_UpdateSegAffected(mapElementIndex, poly);
    else
        SB_UpdateSubsectorAffected(mapElementIndex, poly);

    for(i = 0; i < poly->numvertices; ++i)
    {
        SB_EvalPoint(&poly->vertices[i].color,
                     &illumination[i], affected,
                     poly->vertices[i].pos, surface->normal);
    }

//    colorOverride = SB_CheckColorOverride(affected);

    SB_TrackerClear(tracker, &trackApplied);
}

/**
 * Interpolate between current and destination.
 */
void SB_LerpIllumination(vertexillum_t *illum, gl_rgba_t *result)
{
    uint        i;

    if(!(illum->flags & VIF_LERP))
    {
        // We're done with the interpolation, just use the
        // destination color.
        memcpy(result->rgba, illum->color.rgba, 4);
    }
    else
    {
        float inter = (currentTimeSB - illum->updatetime) / (float)lightSpeed;

        if(inter > 1)
        {
            illum->flags &= ~VIF_LERP;
            memcpy(illum->color.rgba, illum->dest.rgba, 4);
            memcpy(result->rgba, illum->color.rgba, 4);
        }
        else
        {
            for(i = 0; i < 3; ++i)
            {
                result->rgba[i] = (DGLuint)
                    (illum->color.rgba[i] +
                     (illum->dest.rgba[i] - illum->color.rgba[i]) * inter);
            }
        }
    }
}

/**
 * @return          Light contributed by the specified source.
 */
byte *SB_GetCasted(vertexillum_t *illum, int sourceIndex,
                   biasaffection_t *affectedSources)
{
    int         i, k;
    boolean     inUse;

    for(i = 0; i < MAX_BIAS_AFFECTED; ++i)
        if(illum->casted[i].source == sourceIndex)
            return illum->casted[i].rgb;

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
            memset(illum->casted[i].rgb, 0, 3);
            return illum->casted[i].rgb;
        }
    }

    Con_Error("SB_GetCasted: No light casted by source %i.\n", sourceIndex);
    return NULL;
}

/**
 * Add ambient light.
 */
void SB_AmbientLight(const float *point, gl_rgba_t *light)
{
    // Add grid light (represents ambient lighting).
    float       color[3];

    LG_Evaluate(point, color);
    SB_AddLight(light, color, 1.0f);
}

/**
 * Applies shadow bias to the given point.  If 'forced' is true, new
 * lighting is calculated regardless of whether the lights affecting
 * the point have changed.  This is needed when there has been world
 * geometry changes.  'illum' is allowed to be NULL.
 *
 * \fixme Only recalculate the changed lights.  The colors contributed
 * by the others can be saved with the 'affected' array.
 */
void SB_EvalPoint(gl_rgba_t *light,
                  vertexillum_t *illum, biasaffection_t *affectedSources,
                  const float *point, float *normal)
{
    gl_rgba_t new;
    float       dot;
    float       delta[3], surfacePoint[3];
    float       distance;
    float       level;
    uint        i, idx;
    boolean     illuminationChanged = false;
    unsigned int latestSourceUpdate = 0;
    source_t   *s;
    byte       *casted;

  //  gl_rgba_t Srgba;

    struct {
        uint             index;
        //uint           affNum; // Index in affectedSources.
        source_t        *source;
        biasaffection_t *affection;
        boolean          changed;
        boolean          overrider;
    } affecting[MAX_BIAS_AFFECTED + 1], *aff;

  //  memcpy(Srgba.rgba, light->rgba, 4);

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
            if(idx >= numSources)
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
/*
    for(i=0; i < 3; i++)
    light->rgba[i] = (byte)(((Srgba.rgba[i] * reciprocal255)) * light->rgba[i]);

    light->rgba[3] = Srgba.rgba[3];
*/
        return;
    }

    // Init to black.
    new.rgba[0] = new.rgba[1] = new.rgba[2] = 0;

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

        for(i = 0; i < 3; ++i)
        {
            delta[i] = s->pos[i] - point[i];
            surfacePoint[i] = point[i] + delta[i] / 100;
        }

        if(useSightCheck && !P_CheckLineSight(s->pos, surfacePoint))
        {
            // LOS fail.
            if(casted)
            {
                // This affecting source does not contribute any light.
                memset(casted, 0, 3);
                casted[3] = 1;
            }
            continue;
        }
        else
        {
            distance = M_Normalize(delta);
            dot = M_DotProduct(delta, normal);

            // The surface faces away from the light.
            if(dot <= 0)
            {
                if(casted)
                {
                    memset(casted, 0, 3);
                    casted[3] = 1;
                }
                continue;
            }

            level = dot * s->intensity / distance;
        }

        if(level > 1)
            level = 1;

        for(i = 0; i < 3; ++i)
        {
            //int v;

            // The light casted from this source.
            casted[i] = (byte) (255.0f * s->color[i] * level);

            //v = new.rgba[i] + 255.0f * s->color[i] * level;


            //if(v > 255) v = 255;
            //new.rgba[i] = (DGLubyte) v;
        }
        casted[3] = 1;

        // Are we already fully lit?
        /*if(new.rgba[0] == 255 &&
           new.rgba[1] == 255 &&
           new.rgba[2] == 255) break;*/


    }

    if(illum)
    {
        //Con_Message("\n");

        boolean willOverride = false;

        // Combine the casted light from each source.
        for(aff = affecting; aff->source; aff++)
        {
            byte *casted = SB_GetCasted(illum, aff->index,
                                        affectedSources);

            if(aff->overrider && (casted[0] | casted[1] | casted[2]) != 0)
                willOverride = true;

/*            if(!casted[3])
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
                printf("%i:(%i %i %i) ",
                            aff->index, casted[0], casted[1], casted[2]);
                printf("\n");
                }*/

            for(i = 0; i < 3; ++i)
            {
                int v = new.rgba[i] + casted[i];
                if(v > 255) v = 255;
                new.rgba[i] = v;
            }
        }

        /*if(biasAmount > 0)
        {
            SB_AddLight(&new, willOverride ? NULL : biasColor, biasAmount);
            }*/

        // Is there a new destination?
        if(memcmp(illum->dest.rgba, new.rgba, 3))
        {
            if(illum->flags & VIF_LERP)
            {
                // Must not lose the half-way interpolation.
                gl_rgba_t mid;
                SB_LerpIllumination(illum, &mid);

                // This is current color at this very moment.
                memcpy(&illum->color.rgba, &mid, 4);
            }

            // This is what we will be interpolating to.
            memcpy(illum->dest.rgba, new.rgba, 4);

            illum->flags |= VIF_LERP;
            illum->updatetime = latestSourceUpdate;
        }

        SB_LerpIllumination(illum, light);
    }
    else
    {
        memcpy(light->rgba, new.rgba, 4);
    }

    SB_AmbientLight(point, light);
/*
    for(i=0; i < 3; i++)
    light->rgba[i] = (byte)(((Srgba.rgba[i] * reciprocal255)) * light->rgba[i]);

    light->rgba[3] = Srgba.rgba[3];
*/
}
