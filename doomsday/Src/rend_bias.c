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
#include "p_sight.h"

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

void         SB_EvalPoint(gl_rgba_t *light, struct vertexillum_s *illum,
                          float *point, float *normal, boolean forced,
                          short *affectedSources);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float viewfrontvec[3];

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

// CODE --------------------------------------------------------------------

/*
 * Register console variables for Shadow Bias.
 */
void SB_Register(void)
{
    // Editing variables.
    C_VAR_INT("edit-bias-blink", &editBlink, 0, 0, 1,
              "1=Blink the nearest light (when nothing is grabbed).");

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

    // Commands for light editing.
    // - bledit: enter edit mode
    // - blquit: exit edit mode
    // - blsave: save current lights to a DED
    // - blnew: allocate new light, grab it
    // - bldel: delete current/given light
    // - bllock: lock current/given light, ungrab
    // - blunlock: unlock current/given light
    // - blgrab: grab current/given light, or release the currently grabbed
	C_CMD("bledit", BLEditor, "Enter bias light edit mode.");
    C_CMD("blquit", BLEditor, "Quit bias light edit mode.");
    C_CMD("blclear", BLEditor, "Delete all lights.");
    C_CMD("blsave", BLEditor, "Write the current lights to a DED file.");
    C_CMD("blnew", BLEditor, "Allocate new light and grab it.");
    C_CMD("bldel", BLEditor, "Delete current/specified light.");
    C_CMD("bllock", BLEditor, "Lock current/specified light.");
    C_CMD("blunlock", BLEditor, "Unlock current/specified light.");
    C_CMD("blgrab", BLEditor, "Grab current/specified light, or ubgrab.");

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
    normal[VZ] = (theCeiling? -1 : 1);

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

            // Calculate minimum 2D distance to the subsector.
            if(!theCeiling)
            {
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
            }

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
 * Do initial processing that needs to be done before rendering a
 * frame.  Changed lights cause the tracker bits to the set for all
 * segs and planes.
 */
void SB_BeginFrame(void)
{
    biastracker_t allChanges;
    int i;

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
        SB_TrackerApply(&seginfo[i].tracker[0], &allChanges);
        SB_TrackerApply(&seginfo[i].tracker[1], &allChanges);
        SB_TrackerApply(&seginfo[i].tracker[2], &allChanges);
    }

    // Apply to all planes.
    for(i = 0; i < numsubsectors; ++i)
    {
        SB_TrackerApply(&subsecinfo[i].floor.tracker, &allChanges);
        SB_TrackerApply(&subsecinfo[i].ceil.tracker, &allChanges);
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

    if(!useBias)
        return;

    memcpy(&trackChanged, tracker, sizeof(trackChanged));
    memset(&trackApplied, 0, sizeof(trackApplied));
    
    if(poly->numvertices == 2)
    {
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
                         &illumination[i],
                         pos, normal, false,
                         seginfo[mapElementIndex].affected);
        }
    }
    else
    {
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
                         &illumination[i],
                         pos, normal, false,
                         subsecinfo[mapElementIndex].floor.affected);
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
void SB_EvalPoint(gl_rgba_t *light, vertexillum_t *illum,
                  float *point, float *normal, boolean forced,
                  short *affectedSources)
{
    gl_rgba_t new;
    float dot;
    float delta[3], surfacePoint[3];
    float distance;
    float level;
    int i, idx;
    source_t *affecting[MAX_BIAS_AFFECTED + 1], **aff;
    boolean illuminationChanged = (forced != 0);
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
        len = M_Distance(hand, sources[i].pos[k]);
        if(i == 0 || len < minDist)
        {
            minDist = len;
            nearest = &sources[i];
        }
    }
    return nearest;
}

void SB_EndFrame(void)
{
    source_t *src;
    int i;

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
        for(i = 0; i < 3; ++i)
            src->color[i] = editColor[i];

        src->intensity = editIntensity;

        if(!(src->flags & BLF_LOCKED))
        {
            // Update source properties.
            SBE_GetHand(src->pos);
        }

        // The light must be re-evaluated.
        src->flags |= BLF_CHANGED;
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

    memmove(&sources[i], &sources[i + 1],
            sizeof(source_t) * (numSources - i - 1));

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
        Con_Printf("Not implemented yet.\n");
        return true;
    }

    if(!stricmp(cmd, "clear"))
    {
        SBE_Clear();
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

/*
 * Editor HUD.
 */
void SBE_DrawHUD(void)
{
    if(!editActive)
        return;

    
}

void SBE_DrawCursor(void)
{
    source_t *s;
    float distance;
    float hand[3];
    float size = 20000;
   
    if(!editActive || !numSources)
        return;

    // The grabbed cursor blinks yellow.
    if(currentTime & 0x100)
        gl.Color4f(1.0f, 1.0f, .8f, .7f);
    else
        gl.Color4f(.5f, .5f, .4f, .3f);
    
    s = SBE_GetGrabbed();
    if(!s)
    {
        // The nearest cursor blinks blue.
        if(currentTime & 0x200)
            gl.Color4f(0, .1f, 1, .7f);
        else
            gl.Color4f(0, .05f, .5f, .3f);

        s = SBE_GetNearest();
    }
        
    gl.Disable(DGL_TEXTURING);

    SBE_GetHand(hand);
    if(M_Distance(s->pos, hand) > 2 * editDistance)
    {
        // Show where it is.
        gl.Disable(DGL_DEPTH_TEST);
    }

    gl.Begin(DGL_LINES);
    {
        gl.Vertex3f(s->pos[VX] - size, s->pos[VY], s->pos[VZ]);
        gl.Vertex3f(s->pos[VX] + size, s->pos[VY], s->pos[VZ]);

        gl.Vertex3f(s->pos[VX], s->pos[VY] - size, s->pos[VZ]);
        gl.Vertex3f(s->pos[VX], s->pos[VY] + size, s->pos[VZ]);

        gl.Vertex3f(s->pos[VX], s->pos[VY], s->pos[VZ] - size);
        gl.Vertex3f(s->pos[VX], s->pos[VY], s->pos[VZ] + size);
    }
    gl.End();

    gl.Enable(DGL_TEXTURING);
    gl.Enable(DGL_DEPTH_TEST);
}
