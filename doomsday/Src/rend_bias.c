/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * rend_bias.c: Light/Shadow Bias
 *
 * Calculating macro-scale lighting on the fly.  The editing
 * functionality is also implemented here.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_ui.h"
#include "de_defs.h"
#include "p_sight.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

// Bias light source macros.
#define BLF_LOCKED  0x00000001
#define BLF_CHANGED 0x80000000

// TYPES -------------------------------------------------------------------

typedef struct source_s {
    int             flags;
    float           pos[3];
    float           color[3];
    float           intensity;
    unsigned int    lastUpdateTime;
} source_t;

typedef struct affection_s {
    float           intensities[MAX_BIAS_AFFECTED];
    int             numFound;
    short          *affected;
} affection_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(BLEditor);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void         SB_EvalPoint(gl_rgba_t *light,
                          vertexillum_t *illum, short *affectedSources,
                          float *point, float *normal);

static void  SBE_SetColor(float *dest, float *src);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static source_t sources[MAX_BIAS_LIGHTS];
static int numSources = 0;

static int useBias = false;
static int useSightCheck = true;
//static int moveBiasLight = true;
static int updateAffected = true;
static float biasIgnoreLimit = .005f;
static int lightSpeed = 150;
static unsigned int lastChangeOnFrame;
static unsigned int currentTime;

/*
 * BS_EvalPoint uses these, so they must be set before it is called.
 */
static biastracker_t trackChanged;
static biastracker_t trackApplied;

/*
 * Editing variables:
 *
 * edit-bias-blink: keep blinking the nearest light (unless a light is grabbed)
 * edit-bias-grab-distance: how far the light is when grabbed
 * edit-bias-{red,green,blue,intensity}: RGBI of the light
 */
static int editBlink = false;
static float editDistance = 300;
static float editColor[3];
static float editIntensity;

/*
 * Editor status.
 */
static int editActive = false; // Edit mode active?
static int editGrabbed = -1;
static int editHidden = false;
static int editShowAll = false;
static int editHueCircle = false;
static float hueDistance = 100;
static vec3_t hueOrigin, hueSide, hueUp;

// CODE --------------------------------------------------------------------

/*
 * Register console variables for Shadow Bias.
 */
void SB_Register(void)
{
    // Editing variables.
    C_VAR_INT("edit-bias-blink", &editBlink, 0, 0, 1,
              "1=Blink the cursor.");

    C_VAR_FLOAT("edit-bias-grab-distance", &editDistance, 0, 10, 1000,
                "Distance to the grabbed bias light.");

    C_VAR_FLOAT("edit-bias-red", &editColor[0], 0, 0, 1,
                "Red component of the bias light color.");

    C_VAR_FLOAT("edit-bias-green", &editColor[1], 0, 0, 1,
                "Green component of the bias light color.");

    C_VAR_FLOAT("edit-bias-blue", &editColor[2], 0, 0, 1,
                "Blue component of the bias light color.");

    C_VAR_FLOAT("edit-bias-intensity", &editIntensity, 0, 1, 50000,
                "Intensity of the bias light.");

    C_VAR_INT("edit-bias-hide", &editHidden, 0, 0, 1,
              "1=Hide bias light editor's HUD.");

    C_VAR_INT("edit-bias-show-sources", &editShowAll, 0, 0, 1,
              "1=Show all light sources.");
    
    // Commands for light editing.
	C_CMD("bledit", BLEditor, "Enter bias light edit mode.");
    C_CMD("blquit", BLEditor, "Exit bias light edit mode.");
    C_CMD("blclear", BLEditor, "Delete all lights.");
    C_CMD("blsave", BLEditor, "Write the current lights to a DED file.");
    C_CMD("blnew", BLEditor, "Allocate new light and grab it.");
    C_CMD("bldel", BLEditor, "Delete current/specified light.");
    C_CMD("bllock", BLEditor, "Lock current/specified light.");
    C_CMD("blunlock", BLEditor, "Unlock current/specified light.");
    C_CMD("blgrab", BLEditor, "Grab current/specified light, or ubgrab.");
    C_CMD("bldup", BLEditor, "Duplicate current/specified light, grab it.");
    C_CMD("blc", BLEditor, "Set color of light at cursor.");
    C_CMD("bli", BLEditor, "Set intensity of light at cursor.");
    C_CMD("blhue", BLEditor, "Show/hide the hue circle for color selection.");

    // Normal variables.
    C_VAR_INT("rend-bias", &useBias, 0, 0, 1,
              "1=Enable the experimental shadow bias test setup.");

    C_VAR_INT("rend-bias-lightspeed", &lightSpeed, 0, 0, 5000,
              "Milliseconds it takes for light changes to "
              "become effective.");

    // Development variables.
    C_VAR_INT("rend-dev-bias-sight", &useSightCheck, CVF_NO_ARCHIVE, 0, 1,
              "1=Enable the use of line-of-sight checking with shadow bias.");

    C_VAR_INT("rend-dev-bias-affected", &updateAffected, CVF_NO_ARCHIVE, 0, 1,
              "1=Keep track which sources affect which surfaces.");
}

/*
 * Initializes the bias lights according to the loaded Light
 * definitions.
 */
void SB_InitForLevel(const char *uniqueId)
{
    ded_light_t *def;
    source_t *src;
    int i;

    // Start with no sources whatsoever.
    numSources = 0;

    // Check all the loaded Light definitions for any matches.
    for(i = 0; i < defs.count.lights.num; ++i)
    {
        def = &defs.lights[i];

        if(def->state[0] || stricmp(uniqueId, def->level))
            continue;

        src = &sources[numSources++];

        // All lights loaded from a DED are automatically locked.
        src->flags = BLF_CHANGED | BLF_LOCKED; 

        // The color is amplified automatically.
        SBE_SetColor(src->color, def->color);

        src->intensity = def->size;

        src->pos[VX] = def->offset[VX];
        src->pos[VY] = def->offset[VY];
        src->pos[VZ] = def->offset[VZ];

        // This'll enforce an update (although the vertices are also
        // STILL_UNSEEN).
        src->lastUpdateTime = 0;
        
        if(numSources == MAX_BIAS_LIGHTS)
            break;
    }
}

/*
 * Conversion from HSV to RGB.  Everything is [0,1].
 */
static void HSVtoRGB(float *rgb, float h, float s, float v)
{
    int i;
    float f, p, q, t;
    
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

static void SB_WallNormal(rendpoly_t *poly, float normal[3])
{
    normal[VY] = (poly->vertices[0].pos[VX] -
                  poly->vertices[1].pos[VX]) / poly->length;
    normal[VX] = (poly->vertices[1].pos[VY] -
                  poly->vertices[0].pos[VY]) / poly->length;
    normal[VZ] = 0;
}

static void SB_AddAffected(affection_t *aff, int k, float intensity)
{
    int i, worst;
    
    if(aff->numFound < MAX_BIAS_AFFECTED)
    {
        aff->affected[aff->numFound] = k;
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
        
        aff->affected[worst] = k;
        aff->intensities[worst] = intensity;
    }
}

/*
 * This must be called when a plane that the seg touches is moved, or
 * when a seg in a polyobj changes position.
 */
void SB_SegHasMoved(seg_t *seg)
{
    int i;
    seginfo_t *info = SEG_INFO(seg);
    
    // Mark the affected lights changed.
    for(i = 0; i < MAX_BIAS_AFFECTED && info->affected[i] >= 0; ++i)
    {
        sources[info->affected[i]].flags |= BLF_CHANGED;
    }
}

/*
 * This must be called when a plane has moved.  Set 'theCeiling' to
 * true if the plane is a ceiling plane.
 */
void SB_PlaneHasMoved(subsector_t *subsector, boolean theCeiling)
{
    int i;
    subsectorinfo_t *subInfo = SUBSECT_INFO(subsector);
    planeinfo_t *info = (theCeiling ? &subInfo->ceil : &subInfo->floor);
    
    // Mark the affected lights changed.
    for(i = 0; i < MAX_BIAS_AFFECTED && info->affected[i] >= 0; ++i)
    {
        sources[info->affected[i]].flags |= BLF_CHANGED;
    }
}

/*
 * This could be enhanced so that only the lights on the right side of
 * the seg are taken into consideration.
 */
void SB_UpdateSegAffected(int seg, rendpoly_t *poly)
{
    seginfo_t *info = &seginfo[seg];
    int i, k;
    vec2_t delta;
    source_t *src;
    float distance, len, normal[3];
    float intensity;
    affection_t aff;

    // If the data is already up to date, nothing needs to be done.
    if(info->updated == lastChangeOnFrame || !updateAffected)
        return;

    info->updated = lastChangeOnFrame;
    aff.affected = info->affected;
    aff.numFound = 0;
    memset(aff.affected, -1, sizeof(info->affected));

    for(k = 0, src = sources; k < numSources; ++k, ++src)
    {
        if(src->intensity <= 0)
            continue;

        // Calculate minimum 2D distance to the seg.
        for(i = 0; i < 2; ++i)
        {
            V2_Set(delta,
                   poly->vertices[i].pos[VX] - src->pos[VX],
                   poly->vertices[i].pos[VY] - src->pos[VY]);
            len = V2_Normalize(delta);

            if(i == 0 || len < distance)
                distance = len;
        }

        SB_WallNormal(poly, normal);
        if(M_DotProduct(delta, normal) >= 0)
            continue;

        if(distance < 1)
            distance = 1;

        intensity = src->intensity/distance;
        
        // Is the source is too weak, ignore it entirely.
        if(intensity < biasIgnoreLimit)
            continue;

        SB_AddAffected(&aff, k, intensity);
    }
}

static float SB_Dot(source_t *src, float point[3], float normal[3])
{
    float delta[3];
    int i;

    // Delta vector between source and given point.
    for(i = 0; i < 3; ++i)
        delta[i] = src->pos[i] - point[i];

    // Calculate the distance.
    M_Normalize(delta);

    return M_DotProduct(delta, normal);
}

static float SB_PlaneDot(source_t *src, float point[3], boolean theCeiling)
{
    float normal[3];

    normal[VX] = 0;
    normal[VY] = 0;
    normal[VZ] = (theCeiling ? -1 : 1);

    return SB_Dot(src, point, normal);
}

/*
 * This could be enhanced so that only the lights on the right side of
 * the plane are taken into consideration.
 */
void SB_UpdateSubsectorAffected(int sub, rendpoly_t *poly)
{
    subsector_t *subsector = SUBSECTOR_PTR(sub);
    subsectorinfo_t *info = &subsecinfo[sub];
    int i, k, theCeiling;
    vec2_t delta;
    float point[3];
    source_t *src;
    float distance, len, dot;
    float intensity;
    affection_t aff[2];

    // If the data is already up to date, nothing needs to be done.
    if(info->floor.updated == lastChangeOnFrame || !updateAffected)
        return;

    info->floor.updated = lastChangeOnFrame;
    info->ceil.updated = lastChangeOnFrame;
    aff[0].affected = info->floor.affected;
    aff[1].affected = info->ceil.affected;
    aff[0].numFound = 0;
    aff[1].numFound = 0;
    memset(aff[0].affected, -1, sizeof(info->floor.affected));
    memset(aff[1].affected, -1, sizeof(info->ceil.affected));

    for(k = 0, src = sources; k < numSources; ++k, ++src)
    {
        if(src->intensity <= 0)
            continue;

        // Calculate minimum 2D distance to the subsector.
        // FIXME: This is probably too accurate an estimate.
        for(i = 0; i < poly->numvertices; ++i)
        {
            V2_Set(delta,
                   poly->vertices[i].pos[VX] - src->pos[VX],
                   poly->vertices[i].pos[VY] - src->pos[VY]);
            len = V2_Length(delta);
            
            if(i == 0 || len < distance)
                distance = len;
        }
        if(distance < 1)
            distance = 1;
        
        for(theCeiling = 0; theCeiling < 2; ++theCeiling)
        {
            // Estimate the effect on this plane.
            point[VX] = subsector->midpoint.x;
            point[VY] = subsector->midpoint.y;
            point[VZ] = (theCeiling ?
                         FIX2FLT(subsector->sector->ceilingheight) :
                         FIX2FLT(subsector->sector->floorheight));
            dot = SB_PlaneDot(src, point, theCeiling);
            if(dot <= 0)
                continue;

            intensity = /*dot * */ src->intensity / distance;
        
            // Is the source is too weak, ignore it entirely.
            if(intensity < biasIgnoreLimit)
                continue;

            SB_AddAffected(&aff[theCeiling], k, intensity);
        }
    }
}

/*
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

/*
 * Checks if the given index bit is set in the tracker.
 */
int SB_TrackerCheck(biastracker_t *tracker, int index)
{
    return (tracker->changes[index >> 5] & (1 << (index & 0x1f))) != 0;
}

/*
 * Copies changes from src to dest.
 */
void SB_TrackerApply(biastracker_t *dest, const biastracker_t *src)
{
    int i;

    for(i = 0; i < sizeof(dest->changes)/sizeof(dest->changes[0]); ++i)
    {
        dest->changes[i] |= src->changes[i];
    }
}

/*
 * Clears changes of src from dest.
 */
void SB_TrackerClear(biastracker_t *dest, const biastracker_t *src)
{
    int i;

    for(i = 0; i < sizeof(dest->changes)/sizeof(dest->changes[0]); ++i)
    {
        dest->changes[i] &= ~src->changes[i];
    }
}

/*
 * Tests against trackChanged.
 */
static boolean SB_ChangeInAffected(short *affected, biastracker_t *changed)
{
    int i;

    for(i = 0; i < MAX_BIAS_AFFECTED; ++i)
    {
        if(affected[i] < 0) break;
        if(SB_TrackerCheck(changed, affected[i]))
            return true;
    }
    return false;
}                                      

/*
 * This is done in the beginning of the frame when a light source has
 * changed.  The planes that the change affects will need to be
 * re-evaluated.
 */
void SB_MarkPlaneChanges(planeinfo_t *plane, biastracker_t *allChanges)
{
    int i;
    
    SB_TrackerApply(&plane->tracker, allChanges);

    if(SB_ChangeInAffected(plane->affected, allChanges))
    {
        // Mark the illumination unseen to force an update.
        for(i = 0; i < plane->numvertices; ++i)
            plane->illumination[i].flags |= VIF_STILL_UNSEEN;
    }
}

/*
 * Do initial processing that needs to be done before rendering a
 * frame.  Changed lights cause the tracker bits to the set for all
 * segs and planes.
 */
void SB_BeginFrame(void)
{
    biastracker_t allChanges;
    int i, j, k;
    seginfo_t *sin;

    // The time that applies on this frame.
    currentTime = Sys_GetRealTime();
    
    // Check which sources have changed.
    memset(&allChanges, 0, sizeof(allChanges));
    for(i = 0; i < numSources; ++i)
    {
        if(sources[i].flags & BLF_CHANGED)
        {
            SB_TrackerMark(&allChanges, i);
            sources[i].flags &= ~BLF_CHANGED;

            // This is used for interpolation.
            sources[i].lastUpdateTime = currentTime;
            
            // Recalculate which sources affect which surfaces.
            lastChangeOnFrame = framecount;
        }
    }
    
    // Apply to all segs.
    for(i = 0; i < numsegs; ++i)
    {
        sin = &seginfo[i];

        for(j = 0; j < 3; ++j)
            SB_TrackerApply(&sin->tracker[j], &allChanges);

        // Everything that is affected by the changed lights will need
        // an update.
        if(SB_ChangeInAffected(sin->affected, &allChanges))
        {
            // Mark the illumination unseen to force an update.
            for(j = 0; j < 3; ++j)
                for(k = 0; k < 4; ++k)
                    sin->illum[j][k].flags |= VIF_STILL_UNSEEN;
        }
    }

    // Apply to all planes.
    for(i = 0; i < numsubsectors; ++i)
    {
        SB_MarkPlaneChanges(&subsecinfo[i].floor, &allChanges);
        SB_MarkPlaneChanges(&subsecinfo[i].ceil, &allChanges);
    }
}

/*
 * Poly can be a either a wall or a plane (ceiling or a floor).
 */
void SB_RendPoly(struct rendpoly_s *poly, boolean isFloor,
                 struct vertexillum_s *illumination,
                 biastracker_t *tracker,
                 int mapElementIndex)
{
    float pos[3];
    float normal[3];
    int i;
    boolean forced;
    short *affected;

    if(!useBias)
        return;

    memcpy(&trackChanged, tracker, sizeof(trackChanged));
    memset(&trackApplied, 0, sizeof(trackApplied));
    
    if(poly->numvertices == 2)
    {
        // Has any of the old affected lights changed?
        affected = seginfo[mapElementIndex].affected;
          /*forced = SB_ChangeInAffected(affected);*/

        forced = false; //seginfo[mapElementIndex].forced;
        
        SB_UpdateSegAffected(mapElementIndex, poly);
        
        // It's a wall.
        SB_WallNormal(poly, normal);

        for(i = 0; i < 4; ++i)
        {
            pos[VX] = poly->vertices[i % 2].pos[VX];
            pos[VY] = poly->vertices[i % 2].pos[VY];
            pos[VZ] = (i >= 2 ? poly->bottom : poly->top);
            
            SB_EvalPoint((i >= 2 ? &poly->bottomcolor[i - 2] :
                          &poly->vertices[i].color),
                         &illumination[i], affected,
                         pos, normal);
        }
    }
    else
    {
        affected = (isFloor ? subsecinfo[mapElementIndex].floor.affected :
                    subsecinfo[mapElementIndex].ceil.affected);

/*        // Has any of the old affected lights changed?
        forced = SB_ChangeInAffected(affected);
*/
        forced = false; /*(isFloor ? subsecinfo[mapElementIndex].floor.forced :
                          subsecinfo[mapElementIndex].ceil.forced);*/
        
        SB_UpdateSubsectorAffected(mapElementIndex, poly);

        // It's a plane.
        normal[VX] = normal[VY] = 0;
        normal[VZ] = (isFloor? 1 : -1);

        for(i = 0; i < poly->numvertices; ++i)
        {
            pos[VX] = poly->vertices[i].pos[VX];
            pos[VY] = poly->vertices[i].pos[VY];
            pos[VZ] = poly->top;
            
            SB_EvalPoint(&poly->vertices[i].color,
                         &illumination[i], affected,
                         pos, normal);
        }
    }

    SB_TrackerClear(tracker, &trackApplied);
}

/*
 * Interpolate between current and destination.
 */
void SB_LerpIllumination(vertexillum_t *illum, gl_rgba_t *result)
{
    int i;
    
    if(!(illum->flags & VIF_LERP))
    {
        // We're done with the interpolation, just use the
        // destination color.
        memcpy(result->rgba, illum->color.rgba, 4);
    }
    else
    {
        float inter = (currentTime - illum->updatetime) / (float)lightSpeed;

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

/*
 * Applies shadow bias to the given point.  If 'forced' is true, new
 * lighting is calculated regardless of whether the lights affecting
 * the point have changed.  This is needed when there has been world
 * geometry changes.  'illum' is allowed to be NULL.
 *
 * FIXME: Only recalculate the changed lights.  The colors contributed
 * by the others can be saved with the 'affected' array.
 */
void SB_EvalPoint(gl_rgba_t *light,
                  vertexillum_t *illum, short *affectedSources,
                  float *point, float *normal)
{
    gl_rgba_t new;
    float dot;
    float delta[3], surfacePoint[3];
    float distance;
    float level;
    int i, idx;
    source_t *affecting[MAX_BIAS_AFFECTED + 1], **aff;
    boolean illuminationChanged = false;
    unsigned int latestSourceUpdate = 0;

    // Vertices that are rendered for the first time need to be fully
    // evaluated.
    if(illum->flags & VIF_STILL_UNSEEN)
    {
        illuminationChanged = true;
        illum->flags &= ~VIF_STILL_UNSEEN;
    }

    // Determine if any of the affecting lights have changed since
    // last frame.
    for(i = 0, aff = affecting;
        affectedSources[i] >= 0 && i < MAX_BIAS_AFFECTED; ++i)
    {
        idx = affectedSources[i];

        // Is this a valid index?
        if(idx < 0 || idx >= numSources)
            continue;
        
        *aff++ = &sources[idx];

        if(SB_TrackerCheck(&trackChanged, idx))
        {
            illuminationChanged = true;
            SB_TrackerMark(&trackApplied, idx);

            // Keep track of the earliest time when an affected source
            // was changed.
            if(latestSourceUpdate < sources[idx].lastUpdateTime)
            {
                latestSourceUpdate = sources[idx].lastUpdateTime;
            }
        }
    }
    *aff = NULL;

    if(!illuminationChanged && illum != NULL)
    {
        // Reuse the previous value.
        SB_LerpIllumination(illum, light);
        return;
    }
    
    // Init to black (~sectorlight, really).
    new.rgba[0] = new.rgba[1] = new.rgba[2] = 0;

    // Apply the light.
    for(aff = affecting; *aff; aff++)
    {
        for(i = 0; i < 3; ++i)
        {
            delta[i] = (*aff)->pos[i] - point[i];
            surfacePoint[i] = point[i] + delta[i] / 100;
        }
    
        if(useSightCheck && !P_CheckLineSight((*aff)->pos, surfacePoint))
        {
            continue;
        }
        else
        {
            distance = M_Normalize(delta);
            dot = M_DotProduct(delta, normal);
        
            // The surface faces away from the light.
            if(dot <= 0)
                continue;
        
            level = dot * (*aff)->intensity / distance;
        }

        if(level > 1) level = 1;
        
        for(i = 0; i < 3; ++i)
        {
            int v = new.rgba[i] + 255 * (*aff)->color[i] * level;
            if(v > 255) v = 255;
            new.rgba[i] = (DGLubyte) v;
        }

        // Are we already fully lit?
        if(new.rgba[0] == 255 &&
           new.rgba[1] == 255 &&
           new.rgba[2] == 255) break;
    }

    if(illum)
    {
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
}

/*
 * Editor Functionality:
 */

static void SBE_GetHand(float pos[3])
{
    pos[0] = vx + viewfrontvec[VX] * editDistance;
    pos[1] = vz + viewfrontvec[VZ] * editDistance;
    pos[2] = vy + viewfrontvec[VY] * editDistance;
}

static source_t *SBE_GrabSource(int index)
{
    source_t *s;
    int i;
    
    editGrabbed = index;
    s = &sources[index];

    // Update the property cvars.
    editIntensity = s->intensity;
    for(i = 0; i < 3; ++i)
        editColor[i] = s->color[i];

    return s;
}

static source_t *SBE_GetGrabbed(void)
{
    if(editGrabbed >= 0 && editGrabbed < numSources)
    {    
        return &sources[editGrabbed];
    }
    return NULL;
}

static source_t *SBE_GetNearest(void)
{
    float hand[3];
    source_t *nearest = NULL;
    float minDist, len;
    int i;

    SBE_GetHand(hand);
        
    for(i = 0; i < numSources; ++i)
    {
        len = M_Distance(hand, sources[i].pos);
        if(i == 0 || len < minDist)
        {
            minDist = len;
            nearest = &sources[i];
        }
    }
    return nearest;
}

static void SBE_SetColor(float *dest, float *src)
{
    float largest = 0;
    int i;

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

static void SBE_GetHueColor(float *color, float *angle, float *sat)
{
    int i;
    float dot;
    float saturation, hue, scale;
    float minAngle = 0.1f, range = 0.19f;
    vec3_t h, proj;

    dot = M_DotProduct(viewfrontvec, hueOrigin);
    saturation = (acos(dot) - minAngle) / range;
    
    if(saturation < 0)
        saturation = 0;
    if(saturation > 1)
        saturation = 1;
    if(sat)
        *sat = saturation;
    
    if(saturation == 0)
    {
        if(angle)
            *angle = 0;
        
        HSVtoRGB(color, 0, 0, 1);
        return;
    }
    
    // Calculate hue angle by projecting the current viewfront to the
    // hue circle plane.  Project onto the normal and subtract.
    scale = M_DotProduct(viewfrontvec, hueOrigin) /
        M_DotProduct(hueOrigin, hueOrigin);
    M_Scale(h, hueOrigin, scale);

    for(i = 0; i < 3; ++i)
        proj[i] = viewfrontvec[i] - h[i];

    // Now we have the projected view vector on the circle's plane.
    // Normalize the projected vector.
    M_Normalize(proj);

    hue = acos(M_DotProduct(proj, hueUp));

    if(M_DotProduct(proj, hueSide) > 0)
        hue = 2*PI - hue;

    hue /= 2*PI;
    hue += 0.25;
    
    if(angle)
        *angle = hue;

    //Con_Printf("sat=%f, hue=%f\n", saturation, hue);
   
    HSVtoRGB(color, hue, saturation, 1);
}

void SB_EndFrame(void)
{
    source_t *src;

#if 0
    if(moveBiasLight)
    {
        /*source[0] = vx + viewfrontvec[VX]*300;
        source[1] = vz + viewfrontvec[VZ]*300;
        source[2] = vy + viewfrontvec[VY]*300;*/

        // Set up 50 random lights (das blinkenlights!).
        for(i = 0; i < numSources; ++i)
        {
            source_t *s = &sources[i];

            s->pos[VX] = M_FRandom() * 4000 - 2000 + vx;
            s->pos[VY] = M_FRandom() * 4000 - 2000 + vz;
            s->pos[VZ] = M_FRandom() * 200 - 100 + vy;

            s->color[0] = .5 + M_FRandom()/2;
            s->color[1] = .5 + M_FRandom()/2;
            s->color[2] = .5 + M_FRandom()/2;

            s->intensity = 200 + M_FRandom() * 500;

            s->flags |= BLF_CHANGED;
        }
    }
#endif

    // Update the grabbed light.
    if(editActive && (src = SBE_GetGrabbed()) != NULL)
    {
        source_t old;

        memcpy(&old, src, sizeof(old));

        if(editHueCircle)
        {
            // Get the new color from the circle.
            SBE_GetHueColor(editColor, NULL, NULL);
        }
        
        SBE_SetColor(src->color, editColor);
        src->intensity = editIntensity;
        if(!(src->flags & BLF_LOCKED))
        {
            // Update source properties.
            SBE_GetHand(src->pos);
        }

        if(memcmp(&old, src, sizeof(old)))
        {
            // The light must be re-evaluated.
            src->flags |= BLF_CHANGED;
        }
    }
}

static void SBE_Begin(void)
{
    editActive = true;
    editGrabbed = -1;
    Con_Printf("Bias light editor: ON\n");
}

static void SBE_End(void)
{
    editActive = false;
    Con_Printf("Bias light editor: OFF\n");
}

static boolean SBE_New(void)
{
    source_t *s;
    
    if(numSources == MAX_BIAS_LIGHTS)
        return false;
    
    s = SBE_GrabSource(numSources);
    s->flags &= ~BLF_LOCKED;
    editIntensity = 200;
    editColor[0] = editColor[1] = editColor[2] = 1;

    numSources++;
    
    return true;
}

static void SBE_Clear(void)
{
    while(numSources-- > 0)
        sources[numSources].flags |= BLF_CHANGED;

    numSources = 0;
    editGrabbed = -1;

    SBE_New();
}

static void SBE_Delete(int which)
{
    int i;
    
    if(editGrabbed == which)
        editGrabbed = -1;
    else if(editGrabbed > which)
        editGrabbed--;

    // Do a memory move.
    for(i = which; i < numSources; ++i)
        sources[i].flags |= BLF_CHANGED;

    memmove(&sources[which], &sources[which + 1],
            sizeof(source_t) * (numSources - which - 1));

    // One fewer.
    numSources--;    
}

static void SBE_Lock(int which)
{
    sources[which].flags |= BLF_LOCKED;
}

static void SBE_Unlock(int which)
{
    sources[which].flags &= ~BLF_LOCKED;
}

static void SBE_Grab(int which)
{
    if(editGrabbed != which)
        SBE_GrabSource(which);
    else
        editGrabbed = -1;
}

static void SBE_Dupe(int which)
{
    source_t *orig = &sources[which], *s;
    int i;

    if(SBE_New())
    {
        s = SBE_GetGrabbed();
        s->flags &= ~BLF_LOCKED;
        editIntensity = orig->intensity;
        for(i = 0; i < 3; ++i)
            editColor[i] = orig->color[i];
    }    
}

static boolean SBE_Save(const char *name)
{
    filename_t fileName;
    FILE *file;
    int i;
    source_t *s;
    const char *uid = R_GetUniqueLevelID();

    if(!name)
    {
        sprintf(fileName, "%s.ded", R_GetCurrentLevelID());
    }
    else
    {
        strcpy(fileName, name);
        if(!strchr(fileName, '.'))
        {
            // Append the file name extension.
            strcat(fileName, ".ded");
        }
    }

    Con_Printf("Saving to %s...\n", fileName);

    if((file = fopen(fileName, "wt")) == NULL)
        return false;

    fprintf(file, "# %i Bias Lights for %s\n\n", numSources, uid);
    
    // Since there can be quite a lot of these, make sure we'll skip
    // the ones that are definitely not suitable.
    fprintf(file, "SkipIf Not %s\n", gx.Get(DD_GAME_MODE));

    for(i = 0, s = sources; i < numSources; ++i, ++s)
    {
        fprintf(file, "\nLight {\n");
        fprintf(file, "  Level = \"%s\"\n", uid);
        fprintf(file, "  Origin { %g %g %g }\n",
                s->pos[0], s->pos[1], s->pos[2]);
        fprintf(file, "  Color { %g %g %g }\n",
                s->color[0], s->color[1], s->color[2]);
        fprintf(file, "  Intensity = %g\n", s->intensity);
        fprintf(file, "}\n");
    }
    
    fclose(file);
    return true;
}

void SBE_SetHueCircle(boolean activate)
{
    int i;
    
    if(activate == editHueCircle)
        return; // No change in state.

    if(activate && SBE_GetGrabbed() == NULL)
        return;
    
    editHueCircle = activate;

    if(activate)
    {        
        // Determine the orientation of the hue circle.
        for(i = 0; i < 3; ++i)
        {
            hueOrigin[i] = viewfrontvec[i];
            hueSide[i] = viewsidevec[i];
            hueUp[i] = viewupvec[i];
        }
    }
}

/*
 * Editor commands.
 */
int CCmdBLEditor(int argc, char **argv)
{
    char *cmd = argv[0] + 2;
    int which;

    if(!stricmp(cmd, "edit"))
    {
        if(!editActive)
        {
            SBE_Begin();
            return true;
        }
        return false;
    }

    if(!editActive)
    {
        Con_Printf("The bias light editor is not active.\n");
        return false;
    }

    if(!stricmp(cmd, "quit"))
    {
        SBE_End();
        return true;
    }

    if(!stricmp(cmd, "save"))
    {
        return SBE_Save(argc >= 2 ? argv[1] : NULL);
    }

    if(!stricmp(cmd, "clear"))
    {
        SBE_Clear();
        return true;
    }

    if(!stricmp(cmd, "hue"))
    {
        int activate = (argc >= 2 ? stricmp(argv[1], "off") : !editHueCircle);
        SBE_SetHueCircle(activate);
        return true;
    }

    if(!stricmp(cmd, "new"))
    {
        return SBE_New();
    }

    // Has the light index been given as an argument?
    if(editGrabbed >= 0)
        which = editGrabbed;
    else
        which = SBE_GetNearest() - sources;

    if(!stricmp(cmd, "c") && numSources > 0)
    {
        source_t *src = &sources[which];
        float r = 1, g = 1, b = 1;

        if(argc >= 4)
        {
            r = strtod(argv[1], NULL);
            g = strtod(argv[2], NULL);
            b = strtod(argv[3], NULL);
        }

        editColor[0] = r;
        editColor[1] = g;
        editColor[2] = b;
        SBE_SetColor(src->color, editColor);
        src->flags |= BLF_CHANGED;
        return true;
    }

    if(!stricmp(cmd, "i") && numSources > 0)
    {
        source_t *src = &sources[which];

        if(argc >= 2)
        {
            editIntensity = strtod(argv[1], NULL);
        }
        
        src->intensity = editIntensity;
        src->flags |= BLF_CHANGED;
        return true;
    }
    
    if(argc > 1)
    {
        which = atoi(argv[1]);
    }

    if(which < 0 || which >= numSources)
    {
        Con_Printf("Invalid light index %i.\n", which);
        return false;
    }
    
    if(!stricmp(cmd, "del"))
    {
        SBE_Delete(which);
        return true;
    }

    if(!stricmp(cmd, "dup"))
    {
        SBE_Dupe(which);
        return true;
    }
    
    if(!stricmp(cmd, "lock"))
    {
        SBE_Lock(which);
        return true;
    }

    if(!stricmp(cmd, "unlock"))
    {
        SBE_Unlock(which);
        return true;
    }

    if(!stricmp(cmd, "grab"))
    {
        SBE_Grab(which);
        return true;
    }

    return false;
}

static SBE_DrawBox(int x, int y, int w, int h, ui_color_t *c)
{
    UI_GradientEx(x, y, w, h, 6,
                  c ? c : UI_COL(UIC_BG_MEDIUM),
                  c ? c : UI_COL(UIC_BG_LIGHT),
                  .2f, .4f);
    UI_DrawRectEx(x, y, w, h, 6, false, c ? c : UI_COL(UIC_BRD_HI),
                  NULL, .4f, -1);
}

static void SBE_InfoBox(source_t *s, int rightX, char *title, float alpha)
{
    float eye[3] = { vx, vz, vy };
    int w = 16 + FR_TextWidth("R:0.000 G:0.000 B:0.000");
    int th = FR_TextHeight("a"), h = th * 6 + 16;
    int x = screenWidth - 10 - w - rightX, y = screenHeight - 10 - h;
    char buf[80];
    ui_color_t color;

    color.red = s->color[0];
    color.green = s->color[1];
    color.blue = s->color[2];

    SBE_DrawBox(x, y, w, h, &color);
    x += 8;
    y += 8 + th/2;
    
    // - index #
    // - locked status
    // - coordinates
    // - distance from eye
    // - intensity
    // - color

    UI_TextOutEx(title, x, y, false, true, UI_COL(UIC_TITLE), alpha);
    y += th;

    sprintf(buf, "# %03i %s", s - sources,
            s->flags & BLF_LOCKED ? "(lock)" : "");
    UI_TextOutEx(buf, x, y, false, true, UI_COL(UIC_TEXT), alpha);
    y += th;

    sprintf(buf, "(%+06.0f,%+06.0f,%+06.0f)", s->pos[0], s->pos[1], s->pos[2]);
    UI_TextOutEx(buf, x, y, false, true, UI_COL(UIC_TEXT), alpha);
    y += th;
    
    sprintf(buf, "Distance:  %-.0f", M_Distance(eye, s->pos));
    UI_TextOutEx(buf, x, y, false, true, UI_COL(UIC_TEXT), alpha);
    y += th;

    sprintf(buf, "Intensity: %-.0f", s->intensity);
    UI_TextOutEx(buf, x, y, false, true, UI_COL(UIC_TEXT), alpha);
    y += th;

/*void UI_Gradient(int x, int y, int w, int h, ui_color_t * top,
  ui_color_t * bottom, float top_alpha, float bottom_alpha)*/

    sprintf(buf, "R:%.3f G:%.3f B:%.3f",
            s->color[0], s->color[1], s->color[2]);
    UI_TextOutEx(buf, x, y, false, true, UI_COL(UIC_TEXT), alpha);
    y += th;
    
}

/*
 * Editor HUD.
 */
void SBE_DrawHUD(void)
{
    int w, h, y;
    char buf[80];
    float alpha = .8f;
    source_t *s;
    
    if(!editActive || editHidden)
        return;

	// Go into screen projection mode.
	gl.MatrixMode(DGL_PROJECTION);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

    // Overall stats: numSources / MAX (left)
    sprintf(buf, "%i / %i (%i free)", numSources, MAX_BIAS_LIGHTS,
            MAX_BIAS_LIGHTS - numSources);
    w = FR_TextWidth(buf) + 16;
    h = FR_TextHeight(buf) + 16;
    y = screenHeight - 10 - h;
    SBE_DrawBox(10, y, w, h, 0);
    UI_TextOutEx(buf, 18, y + h / 2, false, true,
                 UI_COL(UIC_TITLE), alpha);

    // The map ID.
    UI_TextOutEx((char*)R_GetUniqueLevelID(), 18, y - h/2, false, true,
                 UI_COL(UIC_TITLE), alpha);

    // Stats for nearest & grabbed:
    if(numSources)
    {
        s = SBE_GetNearest();
        SBE_InfoBox(s, 0, SBE_GetGrabbed() ? "Nearest" : "Highlighted", alpha);
    }

    if((s = SBE_GetGrabbed()) != NULL)
    {
        SBE_InfoBox(s, FR_TextWidth("0") * 26, "Grabbed", alpha);
    }
    
	gl.MatrixMode(DGL_PROJECTION);
	gl.PopMatrix();
}

void SBE_DrawStar(float pos[3], float size, float color[4])
{
    float black[4] = { 0, 0, 0, 0 };

    gl.Begin(DGL_LINES);
    {
        gl.Color4fv(black);
        gl.Vertex3f(pos[VX] - size, pos[VZ], pos[VY]);
        gl.Color4fv(color);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY]);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY]);
        gl.Color4fv(black);
        gl.Vertex3f(pos[VX] + size, pos[VZ], pos[VY]);

        gl.Vertex3f(pos[VX], pos[VZ] - size, pos[VY]);
        gl.Color4fv(color);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY]);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY]);
        gl.Color4fv(black);
        gl.Vertex3f(pos[VX], pos[VZ] + size, pos[VY]);
        
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY] - size);
        gl.Color4fv(color);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY]);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY]);
        gl.Color4fv(black);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY] + size);
    }
    gl.End();
}

static void SBE_DrawSource(source_t *src)
{
    float col[4], d;
    float eye[3] = { vx, vz, vy };
    
    col[0] = src->color[0];
    col[1] = src->color[1];
    col[2] = src->color[2];
    d = (M_Distance(eye, src->pos) - 100) / 1000;
    if(d < 1) d = 1;
    col[3] = 1.0f / d;

    SBE_DrawStar(src->pos, 50 + src->intensity/10, col);
}

static void SBE_HueOffset(double angle, float *offset)
{
    offset[0] = cos(angle) * hueSide[VX] + sin(angle) * hueUp[VX];
    offset[1] = sin(angle) * hueUp[VY];
    offset[2] = cos(angle) * hueSide[VZ] + sin(angle) * hueUp[VZ];
}

static void SBE_DrawHue(void)
{
    vec3_t eye = { vx, vy, vz };
    vec3_t center, off, off2;
    float steps = 32, inner = 10, outer = 30, s;
    double angle;
    float color[4], sel[4], hue, saturation;
    int i;
    
    gl.Disable(DGL_DEPTH_TEST);
    gl.Disable(DGL_TEXTURING);
    gl.Disable(DGL_CULL_FACE);

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();

    gl.Translatef(vx, vy, vz);
    gl.Scalef(1, 1.0f/1.2f, 1);
    gl.Translatef(-vx, -vy, -vz);

    // The origin of the circle.
    for(i = 0; i < 3; ++i)
        center[i] = eye[i] + hueOrigin[i] * hueDistance;

    // Draw the circle.
    gl.Begin(DGL_QUAD_STRIP);
    for(i = 0; i <= steps; ++i)
    {
        angle = 2*PI * i/steps;

        // Calculate the hue color for this angle.
        HSVtoRGB(color, i/steps, 1, 1);
        color[3] = .5f;

        SBE_HueOffset(angle, off);
            
        gl.Color4fv(color);
        gl.Vertex3f(center[0] + outer * off[0], center[1] + outer * off[1],
                    center[2] + outer * off[2]);

        // Saturation decreases in the center.
        color[0] = 1;
        color[1] = 1;
        color[2] = 1;
        color[3] = .15f;
        gl.Color4fv(color);
        gl.Vertex3f(center[0] + inner * off[0], center[1] + inner * off[1],
                    center[2] + inner * off[2]);
    }
    gl.End();

    gl.Begin(DGL_LINES);

    // Draw the current hue.
    SBE_GetHueColor(sel, &hue, &saturation);
    SBE_HueOffset(2*PI * hue, off);
    sel[3] = 1;
    gl.Color4fv(sel);
    gl.Vertex3f(center[0] + outer * off[0], center[1] + outer * off[1],
                center[2] + outer * off[2]);
    gl.Vertex3f(center[0] + inner * off[0], center[1] + inner * off[1],
                center[2] + inner * off[2]);

    // Draw the edges.
    for(i = 0; i < steps; ++i)
    {
        SBE_HueOffset(2*PI * i/steps, off);
        SBE_HueOffset(2*PI * (i + 1)/steps, off2);

        // Calculate the hue color for this angle.
        HSVtoRGB(color, i/steps, 1, 1);
        color[3] = 1;
            
        gl.Color4fv(color);
        gl.Vertex3f(center[0] + outer * off[0], center[1] + outer * off[1],
                    center[2] + outer * off[2]);
        gl.Vertex3f(center[0] + outer * off2[0], center[1] + outer * off2[1],
                    center[2] + outer * off2[2]);

        // Saturation decreases in the center.
        gl.Color4fv(sel);
        s = inner + (outer - inner) * saturation;
        gl.Vertex3f(center[0] + s * off[0], center[1] + s * off[1],
                    center[2] + s * off[2]);
        gl.Vertex3f(center[0] + s * off2[0], center[1] + s * off2[1],
                    center[2] + s * off2[2]);
    }
    gl.End();

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();
    
    gl.Enable(DGL_DEPTH_TEST);
    gl.Enable(DGL_TEXTURING);
    gl.Enable(DGL_CULL_FACE);
}

void SBE_DrawCursor(void)
{
#define SET_COL(x, r, g, b, a) {x[0]=(r); x[1]=(g); x[2]=(b); x[3]=(a);}

    double t = Sys_GetRealTime()/100.0f;
    source_t *s;
    float hand[3];
    float size = 10000, distance;
    float col[4];
    float eye[3] = { vx, vz, vy };
   
    if(!editActive || !numSources || editHidden)
        return;

    if(editHueCircle && SBE_GetGrabbed())
        SBE_DrawHue();
    
    // The grabbed cursor blinks yellow.
    if(!editBlink || currentTime & 0x80)
        SET_COL(col, 1.0f, 1.0f, .8f, .5f)
    else
        SET_COL(col, .7f, .7f, .5f, .4f)
    
    s = SBE_GetGrabbed();
    if(!s)
    {
        // The nearest cursor phases blue.
        SET_COL(col, sin(t)*.2f, .2f + sin(t)*.15f, .9f + sin(t)*.3f,
                   .8f - sin(t)*.2f)

        s = SBE_GetNearest();
    }
        
    gl.Disable(DGL_TEXTURING);

    SBE_GetHand(hand);
    if((distance = M_Distance(s->pos, hand)) > 2 * editDistance)
    {
        // Show where it is.
        gl.Disable(DGL_DEPTH_TEST);
    }

    SBE_DrawStar(s->pos, size, col);

    // Show if the source is locked.
    if(s->flags & BLF_LOCKED)
    {
        float lock = 2 + M_Distance(eye, s->pos)/100;

        gl.Color4f(1, 1, 1, 1);
        
        gl.MatrixMode(DGL_MODELVIEW);
        gl.PushMatrix();

        gl.Translatef(s->pos[VX], s->pos[VZ], s->pos[VY]);

        gl.Rotatef(t/2, 0, 0, 1);
        gl.Rotatef(t, 1, 0, 0);
        gl.Rotatef(t * 15, 0, 1, 0);

        gl.Begin(DGL_LINES);
        gl.Vertex3f(-lock, 0, -lock);
        gl.Vertex3f(+lock, 0, -lock);
        
        gl.Vertex3f(+lock, 0, -lock);
        gl.Vertex3f(+lock, 0, +lock);
        
        gl.Vertex3f(+lock, 0, +lock);
        gl.Vertex3f(-lock, 0, +lock);
        
        gl.Vertex3f(-lock, 0, +lock);
        gl.Vertex3f(-lock, 0, -lock);
        gl.End();
        
        gl.PopMatrix();
    }

    if(SBE_GetNearest() != SBE_GetGrabbed() && SBE_GetGrabbed())
    {
        gl.Disable(DGL_DEPTH_TEST);
        SBE_DrawSource(SBE_GetNearest());
    }
    
    // Show all sources?
    if(editShowAll)
    {
        int i;

        gl.Disable(DGL_DEPTH_TEST);
        for(i = 0; i < numSources; ++i)
        {
            if(s == &sources[i])
                continue;
            SBE_DrawSource(&sources[i]);
        }
    }

    gl.Enable(DGL_TEXTURING);
    gl.Enable(DGL_DEPTH_TEST);
}
