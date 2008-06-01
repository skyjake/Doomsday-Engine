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
 * Surface decorations (dynamic lights).
 */

// HEADER FILES ------------------------------------------------------------

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
    const surface_t *surface;
    subsector_t    *subsector;
    decortype_t     type;
    union decorsource_data_s {
        struct decorsource_data_light_s {
            const ded_decorlight_t *def;
        } light;
        struct decorsource_data_model_s {
            struct modeldef_s *mf;
            float          pitch, yaw;
        } model;
    } data;
    struct decorsource_s *next;
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
static decorsource_t *sourceFirst = NULL, *sourceLast = NULL;
static decorsource_t *sourceCursor = NULL;

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
                              const ded_decorlight_t *lightDef)
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

static void projectDecoration(decorsource_t *src)
{
    float               v1[2];
    uint                light;
    lumobj_t           *l;
    vissprite_t        *vis;
    float               distance = Rend_PointDist3D(src->pos);
    float               fadeMul = 1, flareMul = 1, brightness;
    uint                i;

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
            l->flags |= LUMF_NOHALO;
        }
        else
        {
            LUM_OMNI(l)->flareCustom = src->data.light.def->flare.custom;
            LUM_OMNI(l)->flareTex = src->data.light.def->flare.tex;
        }

        LUM_OMNI(l)->flareMul = flareMul;

        for(i = 0; i < 3; ++i)
            l->color[i] = src->data.light.def->color[i] * fadeMul;

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
    decorsource_t      *src;

    for(src = sourceFirst; src != sourceCursor; src = src->next)
    {
        projectDecoration(src);
    }
}

/**
 * Create a new source for a light decoration.
 */
static decorsource_t *addDecoration(void)
{
    decorsource_t      *src;

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
static void createSurfaceDecoration(const surface_t *suf,
                                    const surfacedecor_t *dec,
                                    const float maxDist)
{
    decorsource_t      *source = addDecoration();

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

boolean R_IsValidModelDecoration(const ded_decormodel_t *modelDef)
{
    return ((modelDef && modelDef->id && modelDef->id[0])? true : false);
}

static void projectSurfaceDecorations(const surface_t *suf, float maxDist)
{
    uint                i;

    for(i = 0; i < suf->numDecorations; ++i)
    {
        const surfacedecor_t *d = &suf->decorations[i];

        switch(d->type)
        {
        case DT_LIGHT:
            if(!R_IsValidLightDecoration(DEC_LIGHT(d)->def))
                return;
            break;

        case DT_MODEL:
            if(!R_IsValidModelDecoration(DEC_MODEL(d)->def))
                return;
            break;
        }

        createSurfaceDecoration(suf, d, maxDist);
    }
}

/**
 * Determine proper skip values.
 */
static void getDecorationSkipPattern(const int patternSkip[2], int *skip)
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

/**
 * Generate decorations for the specified section of a line.
 */
static void decorateLineSection(const linedef_t *line, sidedef_t *side,
                                surface_t *suf, float top,
                                float bottom, const float maxDist)
{
    if(suf->flags & SUF_UPDATE_DECORATIONS)
    {
        float               lh, s, t; // Horizontal and vertical offset.
        float               posBase[2], pos[3];
        float               surfTexW, surfTexH, patternW, patternH;
        int                 skip[2];
        uint                i;
        texinfo_t*          texinfo;
        vertex_t*           v[2];
        float               delta[2];
        const ded_decor_t*  def;
        float               offsetY;

        R_ClearSurfaceDecorations(suf);

        def = R_GetMaterialDecoration(suf->material);
        if(def)
        {
            if(line->L_backside)
            {
                if(suf == &side->SW_topsurface)
                {
                    if(line->flags & DDLF_DONTPEGTOP)
                    {
                        offsetY = 0;
                    }
                    else
                    {
                        texinfo_t          *texinfo;

                        GL_GetMaterialInfo2(suf->material, true, &texinfo);
                        offsetY = -texinfo->height + (top - bottom);
                    }
                }
                else // Its a bottom section.
                {
                    if(line->flags & DDLF_DONTPEGBOTTOM)
                        offsetY = (top - bottom);
                    else
                        offsetY = 0;
                }
            }
            else
            {
                if(line->flags & DDLF_DONTPEGBOTTOM)
                {
                    texinfo_t          *texinfo;

                    GL_GetMaterialInfo2(suf->material, true, &texinfo);
                    offsetY = -texinfo->height + (top - bottom);
                }
                else
                {
                    offsetY = 0;
                }
            }

            // Let's see which sidedef is present.
            if(line->L_backside && line->L_backside == side)
            {
                // Flip vertices, this is the backside.
                v[0] = line->L_v2;
                v[1] = line->L_v1;
            }
            else
            {
                v[0] = line->L_v1;
                v[1] = line->L_v2;
            }

            delta[VX] = v[1]->V_pos[VX] - v[0]->V_pos[VX];
            delta[VY] = v[1]->V_pos[VY] - v[0]->V_pos[VY];

            // Height of the section.
            lh = top - bottom;

            // Setup the global texture info variables.
            GL_GetMaterialInfo2(suf->material, true, &texinfo);

            surfTexW = texinfo->width;
            surfTexH = texinfo->height;

            // Generate a number of models.
            for(i = 0; i < DED_DECOR_NUM_MODELS; ++i)
            {
                const ded_decormodel_t* modelDef = &def->models[i];
                modeldef_t*         mf;
                float               pitch, yaw;

                if(!R_IsValidModelDecoration(modelDef))
                    break;

                if((mf = R_CheckIDModelFor(modelDef->id)) == NULL)
                    break;

                yaw = R_MovementYaw(suf->normal[VX], suf->normal[VY]);
                pitch = R_MovementPitch(suf->normal[VX], suf->normal[VY],
                                        suf->normal[VZ]);

                // Skip must be at least one.
                getDecorationSkipPattern(modelDef->patternSkip, skip);

                posBase[VX] = v[0]->V_pos[VX] + modelDef->elevation * suf->normal[VX];
                posBase[VY] = v[0]->V_pos[VY] + modelDef->elevation * suf->normal[VY];

                patternW = surfTexW * skip[VX];
                patternH = surfTexH * skip[VY];

                // Let's see where the top left light is.
                s = M_CycleIntoRange(modelDef->pos[VX] - suf->visOffset[VX] -
                                     surfTexW * modelDef->patternOffset[VX],
                                     patternW);

                for(; s < line->length; s += patternW)
                {
                    subsector_t        *subsector;

                    t = M_CycleIntoRange(modelDef->pos[VY] - suf->visOffset[VY] -
                                         surfTexH * modelDef->patternOffset[VY] +
                                         offsetY, patternH);

                    pos[VX] = posBase[VX] + delta[VX] * s / line->length;
                    pos[VY] = posBase[VY] + delta[VY] * s / line->length;
                    subsector = R_PointInSubsector(pos[VX], pos[VY]);

                    for(; t < lh; t += patternH)
                    {
                        surfacedecor_t     *d;

                        pos[VZ] = top - t;

                        if(NULL != (d = R_CreateSurfaceDecoration(DT_MODEL, suf)))
                        {
                            d->pos[VX] = pos[VX];
                            d->pos[VY] = pos[VY];
                            d->pos[VZ] = pos[VZ];
                            d->subsector = subsector;

                            DEC_MODEL(d)->mf = mf;
                            DEC_MODEL(d)->def = modelDef;
                            DEC_MODEL(d)->pitch = pitch;
                            DEC_MODEL(d)->yaw = yaw;
                        }
                    }
                }
            }

            // Generate a number of lights.
            for(i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
            {
                const ded_decorlight_t* lightDef = def->lights + i;

                // No more?
                if(!R_IsValidLightDecoration(lightDef))
                    break;

                // Skip must be at least one.
                getDecorationSkipPattern(lightDef->patternSkip, skip);

                posBase[VX] = v[0]->V_pos[VX] + lightDef->elevation * suf->normal[VX];
                posBase[VY] = v[0]->V_pos[VY] + lightDef->elevation * suf->normal[VZ];

                patternW = surfTexW * skip[VX];
                patternH = surfTexH * skip[VY];

                // Let's see where the top left light is.
                s = M_CycleIntoRange(lightDef->pos[VX] - suf->visOffset[VX] -
                                     surfTexW * lightDef->patternOffset[VX],
                                     patternW);

                for(; s < line->length; s += patternW)
                {
                    subsector_t        *subsector;

                    t = M_CycleIntoRange(lightDef->pos[VY] - suf->visOffset[VY] -
                                         surfTexH * lightDef->patternOffset[VY] +
                                         offsetY, patternH);

                    pos[VX] = posBase[VX] + delta[VX] * s / line->length;
                    pos[VY] = posBase[VY] + delta[VY] * s / line->length;
                    subsector = R_PointInSubsector(pos[VX], pos[VY]);

                    for(; t < lh; t += patternH)
                    {
                        surfacedecor_t     *d;

                        pos[VZ] = top - t;

                        if(NULL != (d = R_CreateSurfaceDecoration(DT_LIGHT, suf)))
                        {
                            d->pos[VX] = pos[VX];
                            d->pos[VY] = pos[VY];
                            d->pos[VZ] = pos[VZ];
                            d->subsector = subsector;

                            DEC_LIGHT(d)->def = lightDef;
                        }
                    }
                }
            }
        }

        suf->flags &= ~SUF_UPDATE_DECORATIONS;
    }

    projectSurfaceDecorations(suf, maxDist);
}

/**
 * @return              The side that faces the sector (if any).
 */
static sidedef_t *getSectorSide(const linedef_t *line, sector_t *sector)
{
    sidedef_t          *side = line->L_frontside;

    // Swap if that wasn't the right one.
    if(side->sector != sector)
        return line->L_backside;

    return side;
}

static void decorateLine(const linedef_t *line, const float maxDist)
{
    sidedef_t          *side;
    sector_t           *highSector, *lowSector;
    float               frontCeil, frontFloor, backCeil, backFloor;
    surface_t          *suf;

    frontCeil  = line->L_frontsector->SP_ceilvisheight;
    frontFloor = line->L_frontsector->SP_floorvisheight;

    // Do we have a double-sided line?
    if(line->L_backside)
    {
        backCeil  = line->L_backsector->SP_ceilvisheight;
        backFloor = line->L_backsector->SP_floorvisheight;

        // Is there a top section visible on either side?
        if(backCeil != frontCeil &&
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

            // Figure out the right side.
            side = getSectorSide(line, highSector);
            suf = &side->SW_topsurface;

            if(suf->material)
            {
                float               bottom = lowSector->SP_ceilvisheight;
                float               top = highSector->SP_ceilvisheight;

                // Is this a valid section?
                if(bottom < top && line->length > 0)
                {
                    decorateLineSection(line, side, suf, top, bottom,
                                        maxDist);
                }
            }
        }

        // Is there a bottom section visible?
        if(backFloor != frontFloor &&
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

            // Figure out the right side.
            side = getSectorSide(line, lowSector);
            suf = &side->SW_bottomsurface;

            if(suf->material)
            {
                float               bottom = lowSector->SP_ceilvisheight;
                float               top = highSector->SP_floorvisheight;

                // Is this a valid section?
                if(bottom < top && line->length > 0)
                {

                    decorateLineSection(line, side, suf, top, bottom,
                                        maxDist);
                }
            }
        }
    }
    else
    {
        // This is a single-sided line. We only need to worry about the
        // middle texture.
        side = line->L_side(line->L_frontside? FRONT:BACK);
        suf = &side->SW_middlesurface;

        if(suf->material)
        {
            float               bottom = frontFloor;
            float               top = frontCeil;

            // Is this a valid section?
            if(bottom < top && line->length > 0)
            {
                decorateLineSection(line, side, suf, top, bottom, maxDist);
            }
        }
    }
}

/**
 * Generate decorations for a plane.
 */
static void decoratePlane(const sector_t *sec, plane_t *pln,
                          const float maxDist)
{
    uint                i;
    surface_t          *suf = &pln->surface;

    if(suf->flags & SUF_UPDATE_DECORATIONS)
    {
        float               pos[3], tileSize = 64;
        int                 skip[2];
        const ded_decor_t*  def;

        R_ClearSurfaceDecorations(suf);

        def = R_GetMaterialDecoration(pln->PS_material);
        if(def)
        {
            // Generate a number of models.
            for(i = 0; i < DED_DECOR_NUM_MODELS; ++i)
            {
                const ded_decormodel_t* modelDef = &def->models[i];
                modeldef_t*         mf;
                float               pitch, yaw;

                if(!R_IsValidModelDecoration(modelDef))
                    break;

                if((mf = R_CheckIDModelFor(modelDef->id)) == NULL)
                    break;

                yaw = 90 + R_MovementYaw(suf->normal[VX],
                                         suf->normal[VY]);
                pitch = R_MovementPitch(suf->normal[VX], suf->normal[VY],
                                        suf->normal[VZ]);

                // Skip must be at least one.
                getDecorationSkipPattern(modelDef->patternSkip, skip);

                pos[VY] =
                    (int) (sec->bBox[BOXBOTTOM] / tileSize) * tileSize - pln->PS_visoffset[VY] -
                    modelDef->pos[VY] - modelDef->patternOffset[VY] * tileSize;

                while(pos[VY] > sec->bBox[BOXBOTTOM])
                    pos[VY] -= tileSize * skip[VY];

                for(; pos[VY] < sec->bBox[BOXTOP]; pos[VY] += tileSize * skip[VY])
                {
                    if(pos[VY] < sec->bBox[BOXBOTTOM])
                        continue;

                    pos[VX] =
                        (int) (sec->bBox[BOXLEFT] / tileSize) * tileSize - pln->PS_visoffset[VX] +
                        modelDef->pos[VX] - modelDef->patternOffset[VX] * tileSize;

                    while(pos[VX] > sec->bBox[BOXLEFT])
                        pos[VX] -= tileSize * skip[VX];

                    for(; pos[VX] < sec->bBox[BOXRIGHT];
                        pos[VX] += tileSize * skip[VX])
                    {
                        surfacedecor_t         *d;

                        if(pos[VX] < sec->bBox[BOXLEFT])
                            continue;

                        // The point must be inside the correct sector.
                        if(!R_IsPointInSector(pos[VX], pos[VY], sec))
                            continue;

                        pos[VZ] =
                            pln->visHeight + modelDef->elevation * suf->normal[VZ];

                        if(NULL != (d = R_CreateSurfaceDecoration(DT_MODEL, suf)))
                        {
                            d->pos[VX] = pos[VX];
                            d->pos[VY] = pos[VY];
                            d->pos[VZ] = pos[VZ];
                            d->subsector = R_PointInSubsector(d->pos[VX], d->pos[VY]);

                            DEC_MODEL(d)->def = modelDef;
                            DEC_MODEL(d)->mf = mf;
                            DEC_MODEL(d)->pitch = pitch;
                            DEC_MODEL(d)->yaw = yaw;
                        }
                    }
                }
            }

            // Generate a number of lights.
            for(i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
            {
                const ded_decorlight_t* lightDef = &def->lights[i];

                // No more?
                if(!R_IsValidLightDecoration(lightDef))
                    break;

                // Skip must be at least one.
                getDecorationSkipPattern(lightDef->patternSkip, skip);

                pos[VY] =
                    (int) (sec->bBox[BOXBOTTOM] / tileSize) * tileSize - pln->PS_visoffset[VY] -
                    lightDef->pos[VY] - lightDef->patternOffset[VY] * tileSize;

                while(pos[VY] > sec->bBox[BOXBOTTOM])
                    pos[VY] -= tileSize * skip[VY];

                for(; pos[VY] < sec->bBox[BOXTOP]; pos[VY] += tileSize * skip[VY])
                {
                    if(pos[VY] < sec->bBox[BOXBOTTOM])
                        continue;

                    pos[VX] =
                        (int) (sec->bBox[BOXLEFT] / tileSize) * tileSize - pln->PS_visoffset[VX] +
                        lightDef->pos[VX] - lightDef->patternOffset[VX] * tileSize;

                    while(pos[VX] > sec->bBox[BOXLEFT])
                        pos[VX] -= tileSize * skip[VX];

                    for(; pos[VX] < sec->bBox[BOXRIGHT];
                        pos[VX] += tileSize * skip[VX])
                    {
                        surfacedecor_t         *d;

                        if(pos[VX] < sec->bBox[BOXLEFT])
                            continue;

                        // The point must be inside the correct sector.
                        if(!R_IsPointInSector(pos[VX], pos[VY], sec))
                            continue;

                        pos[VZ] =
                            pln->visHeight + lightDef->elevation * suf->normal[VZ];

                        if(NULL != (d = R_CreateSurfaceDecoration(DT_LIGHT, suf)))
                        {
                            d->pos[VX] = pos[VX];
                            d->pos[VY] = pos[VY];
                            d->pos[VZ] = pos[VZ];
                            d->subsector = R_PointInSubsector(d->pos[VX], d->pos[VY]);

                            DEC_LIGHT(d)->def = lightDef;
                        }
                    }
                }
            }
        }

        suf->flags &= ~SUF_UPDATE_DECORATIONS;
    }

    projectSurfaceDecorations(suf, maxDist);
}

static void decorateSector(const sector_t *sec, const float maxDist)
{
    uint                i;

    for(i = 0; i < sec->planeCount; ++i)
    {
        decoratePlane(sec, sec->SP_plane(i), maxDist);
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
        uint                i;

        // Process all lines. This could also be done during sectors,
        // but validCount would need to be used to prevent duplicate
        // processing.
        for(i = 0; i < numLineDefs; ++i)
            decorateLine(&lineDefs[i], decorMaxDist);

        // Process all planes.
        for(i = 0; i < numSectors; ++i)
            decorateSector(&sectors[i], decorMaxDist);
    }
}
