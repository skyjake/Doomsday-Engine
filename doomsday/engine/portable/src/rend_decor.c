/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * rend_decor.c: Decorations
 *
 * Surface decorations.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// Quite a bit of decorations, there!
#define MAX_SOURCES     16384

// TYPES -------------------------------------------------------------------

typedef struct decorsource_s {
    float           pos[3];
    float           maxDist;
    const surface_t* surface;
    subsector_t*    subsector;
    decortype_t     type;
    union decorsource_data_s {
        struct decorsource_data_light_s {
            const ded_decorlight_t* def;
        } light;
        struct decorsource_data_model_s {
            struct modeldef_s* mf;
            float          pitch, yaw;
        } model;
    } data;
    struct decorsource_s* next;
} decorsource_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte useDecorations = true;
float decorMaxDist = 2048; // No decorations are visible beyond this.
float decorFactor = 1;
float decorFadeAngle = .1f;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static uint numDecorLightSources;
static decorsource_t* sourceFirst = NULL, *sourceLast = NULL;
static decorsource_t* sourceCursor = NULL;

// CODE --------------------------------------------------------------------

void Rend_DecorRegister(void)
{
    C_VAR_BYTE("rend-light-decor", &useDecorations, 0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-far", &decorMaxDist, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-light-decor-bright", &decorFactor, 0, 0, 10);
    C_VAR_FLOAT("rend-light-decor-angle", &decorFadeAngle, 0, 0, 1);
}

/**
 * Clears the list of decoration dummies.
 */
static void clearDecorations(void)
{
    numDecorLightSources = 0;
    sourceCursor = sourceFirst;
}

/**
 * @return              @c > 0, if the sector lightlevel passes the
 *                      limit condition.
 */
static float checkSectorLight(float lightlevel,
                              const ded_decorlight_t* lightDef)
{
    float               factor;

    // Has a limit been set?
    if(lightDef->lightLevels[0] == lightDef->lightLevels[1])
        return 1;

    // Apply adaptation
    Rend_ApplyLightAdaptation(&lightlevel);

    factor = (lightlevel - lightDef->lightLevels[0]) /
        (float) (lightDef->lightLevels[1] - lightDef->lightLevels[0]);

    if(factor < 0)
        return 0;
    if(factor > 1)
        return 1;

    return factor;
}

static void projectDecoration(decorsource_t* src)
{
    uint                i;
    float               v1[2];
    uint                light;
    lumobj_t*           l;
    vissprite_t*        vis;
    float               distance = Rend_PointDist3D(src->pos);
    float               fadeMul = 1, flareMul = 1, brightness;

    // Is the point in range?
    if(distance > src->maxDist)
        return;

    // Close enough to the maximum distance, decorations fade out.
    if(distance > .67f * src->maxDist)
    {
        fadeMul = (src->maxDist - distance) / (.33f * src->maxDist);
    }

    if(src->type == DT_LIGHT)
    {
        // Does it pass the sectorlight limitation?
        if(!((brightness =
              checkSectorLight(src->subsector->sector->lightLevel,
                               src->data.light.def)) > 0))
            return;

        // Apply the brightness factor (was calculated using sector lightlevel).
        fadeMul *= brightness * decorFactor;

        // Brightness drops as the angle gets too big.
        if(src->data.light.def->elevation < 2 && decorFadeAngle > 0) // Close the surface?
        {
            float               vector[3];
            float               dot;

            vector[VX] = src->pos[VX] - vx;
            vector[VY] = src->pos[VZ] - vy;
            vector[VZ] = src->pos[VY] - vz;
            M_Normalize(vector);
            dot = -(src->surface->normal[VX] * vector[VX] +
                    src->surface->normal[VZ] * vector[VY] +
                    src->surface->normal[VY] * vector[VZ]);
            if(dot < decorFadeAngle / 2)
            {
                flareMul = 0;
            }
            else if(dot < 3 * decorFadeAngle)
            {
                flareMul *= (dot - decorFadeAngle / 2) / (2.5f * decorFadeAngle);
            }
        }
    }

    if(fadeMul <= 0)
        return;

    // Calculate edges of the shape.
    v1[VX] = src->pos[VX];
    v1[VY] = src->pos[VY];

    vis = R_NewVisSprite();
    memset(vis, 0, sizeof(*vis));
    vis->type = VSPR_DECORATION;
    vis->distance = distance;
    vis->center[VX] = src->pos[VX];
    vis->center[VY] = src->pos[VY];
    vis->center[VZ] = src->pos[VZ];

    if(src->type == DT_MODEL)
    {
        vis->data.decormodel.mf = src->data.model.mf;
        vis->data.decormodel.subsector = src->subsector;
        vis->data.decormodel.alpha = fadeMul;
        vis->data.decormodel.pitch = src->data.model.pitch;
        vis->data.decormodel.pitchAngleOffset = 0;
        vis->data.decormodel.yaw = src->data.model.yaw;
        vis->data.decormodel.yawAngleOffset = 0;
    }
    else if(src->type == DT_LIGHT)
    {
        /**
         * \todo From here on is pretty much the same as LO_AddLuminous, reconcile
         * the two.
         */
        light = LO_NewLuminous(LT_OMNI);
        l = LO_GetLuminous(light);

        l->pos[VX] = src->pos[VX];
        l->pos[VY] = src->pos[VY];
        l->pos[VZ] = src->pos[VZ];
        l->subsector = src->subsector;
        l->flags = LUMF_CLIPPED;

        LUM_OMNI(l)->haloFactor = 0xff; // Assumed visible.
        LUM_OMNI(l)->zOff = 0;
        LUM_OMNI(l)->tex = src->data.light.def->sides.tex;
        LUM_OMNI(l)->ceilTex = src->data.light.def->up.tex;
        LUM_OMNI(l)->floorTex = src->data.light.def->down.tex;

        // These are the same rules as in DL_MobjRadius().
        LUM_OMNI(l)->radius = src->data.light.def->radius * 40 * loRadiusFactor;

        // Don't make a too small or too large light.
        if(LUM_OMNI(l)->radius > loMaxRadius)
            LUM_OMNI(l)->radius = loMaxRadius;

        if(src->data.light.def->haloRadius > 0)
        {
            LUM_OMNI(l)->flareSize =
                src->data.light.def->haloRadius * 60 * (50 + haloSize) / 100.0f;
            if(LUM_OMNI(l)->flareSize < 1)
                LUM_OMNI(l)->flareSize = 1;
        }
        else
        {
            LUM_OMNI(l)->flareSize = 0;
        }

        if(src->data.light.def->flare.disabled)
        {
            LUM_OMNI(l)->flags |= LUMOF_NOHALO;
        }
        else
        {
            LUM_OMNI(l)->flareCustom = src->data.light.def->flare.custom;
            LUM_OMNI(l)->flareTex = src->data.light.def->flare.tex;
        }

        LUM_OMNI(l)->flareMul = flareMul;

        for(i = 0; i < 3; ++i)
            LUM_OMNI(l)->color[i] = src->data.light.def->color[i] * fadeMul;

        l->distanceToViewer = distance;

        vis->light = l;
    }
}

/**
 * Project all the non-clipped decorations. They become regular vissprites.
 * This is needed for rendering halos.
 */
void Rend_ProjectDecorations(void)
{
    decorsource_t*      src;

    for(src = sourceFirst; src != sourceCursor; src = src->next)
    {
        projectDecoration(src);
    }
}

/**
 * Create a new source for a light decoration.
 */
static decorsource_t* addDecoration(void)
{
    decorsource_t*      src;

    if(numDecorLightSources > MAX_SOURCES)
        return NULL;

    numDecorLightSources++;

    // If the cursor is NULL, new sources must be allocated.
    if(!sourceCursor)
    {
        // Allocate a new entry.
        src = Z_Calloc(sizeof(decorsource_t), PU_STATIC, NULL);

        if(sourceLast)
            sourceLast->next = src;
        sourceLast = src;

        if(!sourceFirst)
            sourceFirst = src;
    }
    else
    {
        // There are old sources to use.
        src = sourceCursor;

        // Advance the cursor.
        sourceCursor = sourceCursor->next;
    }

    return src;
}

/**
 * A decoration is created at the specified coordinates.
 */
static void createSurfaceDecoration(const surface_t* suf,
                                    const surfacedecor_t* dec,
                                    const float maxDist)
{
    decorsource_t*      source = addDecoration();

    if(!source)
        return; // Out of sources!

    // Fill in the data for a new surface decoration.
    source->pos[VX] = dec->pos[VX];
    source->pos[VY] = dec->pos[VY];
    source->pos[VZ] = dec->pos[VZ];
    source->maxDist = maxDist;
    source->subsector = dec->subsector;
    source->surface = suf;
    source->type = dec->type;
    switch(source->type)
    {
    case DT_LIGHT:
        source->data.light.def = DEC_LIGHT(dec)->def;
        break;

    case DT_MODEL:
        source->data.model.mf = DEC_MODEL(dec)->mf;
        source->data.model.pitch = DEC_MODEL(dec)->pitch;
        source->data.model.yaw = DEC_MODEL(dec)->yaw;
        break;
    }
}

boolean R_IsValidModelDecoration(const ded_decormodel_t* modelDef)
{
    return ((modelDef && modelDef->id && modelDef->id[0])? true : false);
}

/**
 * @return              As this can also be used with iterators, will always
 *                      return @c true.
 */
boolean R_ProjectSurfaceDecorations(surface_t* suf, void* context)
{
    uint                i;
    float               maxDist = *((float*) context);

    for(i = 0; i < suf->numDecorations; ++i)
    {
        const surfacedecor_t* d = &suf->decorations[i];

        switch(d->type)
        {
        case DT_LIGHT:
            if(!R_IsValidLightDecoration(DEC_LIGHT(d)->def))
                return true;
            break;

        case DT_MODEL:
            if(!R_IsValidModelDecoration(DEC_MODEL(d)->def))
                return true;
            break;
        }

        createSurfaceDecoration(suf, d, maxDist);
    }

    return true;
}

/**
 * Determine proper skip values.
 */
static void getDecorationSkipPattern(const int patternSkip[2], int* skip)
{
    uint                i;

    for(i = 0; i < 2; ++i)
    {
        // Skip must be at least one.
        skip[i] = patternSkip[i] + 1;

        if(skip[i] < 1)
            skip[i] = 1;
    }
}

static uint generateDecorLights(const ded_decorlight_t* def,
                                surface_t* suf, const pvec3_t v1,
                                const pvec3_t v2, float width, float height,
                                const pvec3_t delta, int axis,
                                float offsetS, float offsetT, sector_t* sec)
{
    uint                num;
    float               s, t; // Horizontal and vertical offset.
    vec3_t              posBase, pos;
    float               patternW, patternH;
    int                 skip[2];

    if(!R_IsValidLightDecoration(def))
        return 0;

    // Skip must be at least one.
    getDecorationSkipPattern(def->patternSkip, skip);
    patternW = suf->material->width * skip[0];
    patternH = suf->material->height * skip[1];

    V3_Set(posBase, def->elevation * suf->normal[VX],
                    def->elevation * suf->normal[VY],
                    def->elevation * suf->normal[VZ]);
    V3_Sum(posBase, posBase, v1);

    // Let's see where the top left light is.
    s = M_CycleIntoRange(def->pos[0] - suf->visOffset[0] -
                         suf->material->width * def->patternOffset[0] +
                         offsetS, patternW);
    num = 0;
    for(; s < width; s += patternW)
    {
        t = M_CycleIntoRange(def->pos[1] - suf->visOffset[1] -
                             suf->material->height * def->patternOffset[1] +
                             offsetT, patternH);

        for(; t < height; t += patternH)
        {
            surfacedecor_t*     d;
            float               offS = s / width, offT = t / height;

            V3_Set(pos, delta[VX] * offS,
                        delta[VY] * (axis == VZ? offT : offS),
                        delta[VZ] * (axis == VZ? offS : offT));
            V3_Sum(pos, posBase, pos);

            if(sec)
            {
                // The point must be inside the correct sector.
                if(!R_IsPointInSector(pos[VX], pos[VY], sec))
                    continue;
            }

            if(NULL != (d = R_CreateSurfaceDecoration(DT_LIGHT, suf)))
            {
                V3_Copy(d->pos, pos);
                d->subsector = R_PointInSubsector(d->pos[VX], d->pos[VY]);
                DEC_LIGHT(d)->def = def;

                R_SurfaceListAdd(decoratedSurfaceList, suf);

                num++;
            }
        }
    }

    return num;
}

static uint generateDecorModels(const ded_decormodel_t* def,
                                surface_t* suf, const pvec3_t v1,
                                const pvec3_t v2, float width, float height,
                                const pvec3_t delta, int axis,
                                float offsetS, float offsetT, sector_t* sec)
{
    uint                num;
    modeldef_t*         mf;
    float               pitch, yaw;
    float               patternW, patternH;
    float               s, t; // Horizontal and vertical offset.
    vec3_t              posBase, pos;
    int                 skip[2];

    if(!R_IsValidModelDecoration(def))
        return 0;

    if((mf = R_CheckIDModelFor(def->id)) == NULL)
        return 0;

    yaw = R_MovementYaw(suf->normal[VX], suf->normal[VY]);
    if(axis == VZ)
        yaw += 90;
    pitch = R_MovementPitch(suf->normal[VX], suf->normal[VY],
                            suf->normal[VZ]);

    // Skip must be at least one.
    getDecorationSkipPattern(def->patternSkip, skip);
    patternW = suf->material->width * skip[0];
    patternH = suf->material->height * skip[1];

    V3_Set(posBase, def->elevation * suf->normal[VX],
                    def->elevation * suf->normal[VY],
                    def->elevation * suf->normal[VZ]);
    V3_Sum(posBase, posBase, v1);

    // Let's see where the top left light is.
    s = M_CycleIntoRange(def->pos[0] - suf->visOffset[0] -
                         suf->material->width * def->patternOffset[0] +
                         offsetS, patternW);
    num = 0;
    for(; s < width; s += patternW)
    {
        t = M_CycleIntoRange(def->pos[1] - suf->visOffset[1] -
                             suf->material->height * def->patternOffset[1] +
                             offsetT, patternH);

        for(; t < height; t += patternH)
        {
            surfacedecor_t     *d;
            float               offS = s / width, offT = t / height;

            V3_Set(pos, delta[VX] * offS,
                        delta[VY] * (axis == VZ? offT : offS),
                        delta[VZ] * (axis == VZ? offS : offT));
            V3_Sum(pos, posBase, pos);

            if(sec)
            {
                // The point must be inside the correct sector.
                if(!R_IsPointInSector(pos[VX], pos[VY], sec))
                    continue;
            }

            if(NULL != (d = R_CreateSurfaceDecoration(DT_MODEL, suf)))
            {
                V3_Copy(d->pos, pos);
                d->subsector = R_PointInSubsector(d->pos[VX], d->pos[VY]);
                DEC_MODEL(d)->def = def;
                DEC_MODEL(d)->mf = mf;
                DEC_MODEL(d)->pitch = pitch;
                DEC_MODEL(d)->yaw = yaw;

                R_SurfaceListAdd(decoratedSurfaceList, suf);

                num++;
            }
        }
    }

    return num;
}

/**
 * Generate decorations for the specified surface.
 */
static void updateSurfaceDecorations(surface_t* suf, float offsetS,
                                     float offsetT, vec3_t v1, vec3_t v2,
                                     sector_t* sec, boolean visible)
{
    vec3_t              delta;

    R_ClearSurfaceDecorations(suf);
    R_SurfaceListRemove(decoratedSurfaceList, suf);

    V3_Subtract(delta, v2, v1);

    if(visible && suf->material &&
       (delta[VX] * delta[VY] != 0 ||
        delta[VX] * delta[VZ] != 0 ||
        delta[VY] * delta[VZ] != 0))
    {
        uint                i;
        float               matW, matH, width, height;
        int                 axis = V3_MajorAxis(suf->normal);
        const ded_decor_t*  def = R_MaterialGetDecoration(suf->material);

        if(def)
        {
            matW = suf->material->current->width;
            matH = suf->material->current->height;
            if(axis == VX || axis == VY)
            {
                width = sqrt(delta[VX] * delta[VX] + delta[VY] * delta[VY]);
                height = delta[VZ];
            }
            else
            {
                width = sqrt(delta[VX] * delta[VX]);
                height = delta[VY];
            }
            if(width < 0)
                width = -width;
            if(height < 0)
                height = -height;

            // Generate a number of models.
            for(i = 0; i < DED_DECOR_NUM_MODELS; ++i)
            {
                generateDecorModels(&def->models[i], suf, v1, v2, width,
                                    height, delta, axis, offsetS, offsetT,
                                    sec);
            }

            // Generate a number of lights.
            for(i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
            {
                generateDecorLights(&def->lights[i], suf, v1, v2, width,
                                    height, delta, axis, offsetS, offsetT,
                                    sec);
            }
        }
    }

    suf->flags &= ~SUF_UPDATE_DECORATIONS;
}

/**
 * Generate decorations for a plane.
 */
static void updatePlaneDecorations(plane_t* pln)
{
    sector_t*           sec = pln->sector;
    surface_t*          suf = &pln->surface;
    vec3_t              v1, v2;
    float               offsetS, offsetT;

    if(pln->type == PLN_FLOOR)
    {
        V3_Set(v1, sec->bBox[BOXLEFT], sec->bBox[BOXTOP], pln->visHeight);
        V3_Set(v2, sec->bBox[BOXRIGHT], sec->bBox[BOXBOTTOM], pln->visHeight);
    }
    else
    {
        V3_Set(v1, sec->bBox[BOXLEFT], sec->bBox[BOXBOTTOM], pln->visHeight);
        V3_Set(v2, sec->bBox[BOXRIGHT], sec->bBox[BOXTOP], pln->visHeight);
    }

    offsetS = -fmod(sec->bBox[BOXLEFT], 64);
    offsetT = -fmod(sec->bBox[BOXBOTTOM], 64);

    updateSurfaceDecorations(suf, offsetS, offsetT, v1, v2, sec, true);
}

static void updateSideSectionDecorations(sidedef_t* side, segsection_t section)
{
    linedef_t*          line;
    surface_t*          suf;
    vec3_t              v1, v2;
    float               offsetS, offsetT;
    boolean             visible = false;
    sector_t*           highSector, *lowSector;
    float               frontCeil, frontFloor, backCeil, backFloor, bottom,
                        top;

    if(!side->segs || !side->segs[0])
        return;

    line = side->segs[0]->lineDef;
    frontCeil  = line->L_frontsector->SP_ceilvisheight;
    frontFloor = line->L_frontsector->SP_floorvisheight;

    if(line->L_backside)
    {
        backCeil  = line->L_backsector->SP_ceilvisheight;
        backFloor = line->L_backsector->SP_floorvisheight;
    }

    switch(section)
    {
    case SEG_MIDDLE:
        suf = &side->SW_middlesurface;
        bottom = frontFloor;
        top = frontCeil;
        visible = true;
        break;

    case SEG_TOP:
        suf = &side->SW_topsurface;
        // Is the top section visible on either side?
        if(line->L_frontside && line->L_backside && backCeil != frontCeil &&
           (!R_IsSkySurface(&line->L_backsector->SP_ceilsurface) ||
            !R_IsSkySurface(&line->L_frontsector->SP_ceilsurface)))
        {
            if(frontCeil > backCeil)
            {
                highSector = line->L_frontsector;
                lowSector  = line->L_backsector;
            }
            else
            {
                lowSector  = line->L_frontsector;
                highSector = line->L_backsector;
            }

            bottom = lowSector->SP_ceilvisheight;
            top = highSector->SP_ceilvisheight;
            visible = true;
        }
        break;

    case SEG_BOTTOM:
        suf = &side->SW_bottomsurface;
        if(line->L_frontside && line->L_backside && backFloor != frontFloor &&
           (!R_IsSkySurface(&line->L_backsector->SP_floorsurface) ||
            !R_IsSkySurface(&line->L_frontsector->SP_floorsurface)))
        {
            if(frontFloor > backFloor)
            {
                highSector = line->L_frontsector;
                lowSector  = line->L_backsector;
            }
            else
            {
                lowSector  = line->L_frontsector;
                highSector = line->L_backsector;
            }

            bottom = lowSector->SP_ceilvisheight;
            top = highSector->SP_floorvisheight;
            visible = true;
        }
        break;
    }

    if(visible && suf->material)
    {
        if(line->L_backside)
        {
            if(suf == &side->SW_topsurface)
            {
                if(line->flags & DDLF_DONTPEGTOP)
                {
                    offsetT = 0;
                }
                else
                {
                    offsetT = -suf->material->current->height + (top - bottom);
                }
            }
            else // Its a bottom section.
            {
                if(line->flags & DDLF_DONTPEGBOTTOM)
                    offsetT = (top - bottom);
                else
                    offsetT = 0;
            }
        }
        else
        {
            if(line->flags & DDLF_DONTPEGBOTTOM)
            {
                offsetT = -suf->material->current->height + (top - bottom);
            }
            else
            {
                offsetT = 0;
            }
        }

        // Let's see which sidedef is present.
        if(line->L_backside && line->L_backside == side)
        {
            // Flip vertices, this is the backside.
            V3_Set(v1, line->L_v2pos[VX], line->L_v2pos[VY], top);
            V3_Set(v2, line->L_v1pos[VX], line->L_v1pos[VY], bottom);
        }
        else
        {
            V3_Set(v1, line->L_v1pos[VX], line->L_v1pos[VY], top);
            V3_Set(v2, line->L_v2pos[VX], line->L_v2pos[VY], bottom);
        }
    }

    offsetS = 0;

    updateSurfaceDecorations(suf, offsetS, offsetT, v1, v2, NULL, visible);
}

void Rend_UpdateSurfaceDecorations(void)
{
    // This only needs to be done if decorations have been enabled.
    if(useDecorations)
    {
        uint                i;

        // Process all sidedefs.
        for(i = 0; i < numSideDefs; ++i)
        {
            sidedef_t*          side = &sideDefs[i];
            surface_t*          suf;

            suf = &side->SW_middlesurface;
            if(suf->flags & SUF_UPDATE_DECORATIONS)
                updateSideSectionDecorations(side, SEG_MIDDLE);

            suf = &side->SW_topsurface;
            if(suf->flags & SUF_UPDATE_DECORATIONS)
                updateSideSectionDecorations(side, SEG_TOP);

            suf = &side->SW_bottomsurface;
            if(suf->flags & SUF_UPDATE_DECORATIONS)
                updateSideSectionDecorations(side, SEG_BOTTOM);
        }

        // Process all planes.
        for(i = 0; i < numSectors; ++i)
        {
            uint                j;
            sector_t*           sec = &sectors[i];

            for(j = 0; j < sec->planeCount; ++j)
            {
                plane_t*            pln = sec->SP_plane(j);
                if(pln->surface.flags & SUF_UPDATE_DECORATIONS)
                    updatePlaneDecorations(pln);
            }
        }
    }
}

/**
 * Decorations are generated for each frame.
 */
void Rend_InitDecorationsForFrame(void)
{
    clearDecorations();

    // This only needs to be done if decorations have been enabled.
    if(useDecorations)
    {
        Rend_UpdateSurfaceDecorations(); // temporary.

        R_SurfaceListIterate(decoratedSurfaceList,
                             R_ProjectSurfaceDecorations, &decorMaxDist);
    }
}
