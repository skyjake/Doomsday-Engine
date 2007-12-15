/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

// Quite a bit of lights, there!
#define MAX_SOURCES     16384

// TYPES -------------------------------------------------------------------

typedef struct decorsource_s {
    uint        light;
    float       pos[3];
    struct decorsource_s *next;
} decorsource_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte    useDecorations = true;
float   decorWallMaxDist = 1500;    // No decorations are visible beyond this.
float   decorPlaneMaxDist = 1500;
float   decorWallFactor = 1;
float   decorPlaneFactor = 1;
float   decorFadeAngle = .1f;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static uint numDecorLightSources;
static decorsource_t *sourceFirst, *sourceLast, *sourceCursor;

// Lights near surfaces get dimmer if the angle is too small.
static float surfaceNormal[3];

// CODE --------------------------------------------------------------------

void Rend_DecorRegister(void)
{
    C_VAR_BYTE("rend-light-decor", &useDecorations, 0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-plane-far", &decorPlaneMaxDist, CVF_NO_MAX,
                0, 0);
    C_VAR_FLOAT("rend-light-decor-wall-far", &decorWallMaxDist, CVF_NO_MAX,
                0, 0);
    C_VAR_FLOAT("rend-light-decor-plane-bright", &decorPlaneFactor, 0, 0,
                10);
    C_VAR_FLOAT("rend-light-decor-wall-bright", &decorWallFactor, 0, 0, 10);
    C_VAR_FLOAT("rend-light-decor-angle", &decorFadeAngle, 0, 0, 1);
}

/**
 * @return              Ptr to the surface decoration, if any.
 */
static ded_decor_t *getMaterialDecoration(material_t *mat)
{
    if(!mat)
        return NULL;

    switch(mat->type)
    {
    case MAT_FLAT:
        return flats[flattranslation[mat->ofTypeID].current]->decoration;

    case MAT_TEXTURE:
        return textures[texturetranslation[mat->ofTypeID].current]->decoration;

    default:
        return NULL;
    }
}

/**
 * Clears the list of decoration dummies.
 */
static void clearDecorations(void)
{
    numDecorLightSources = 0;
    sourceCursor = sourceFirst;
}

static void projectDecoration(lumobj_t *lum, float x, float y, float z)
{
    float       v1[2];
    vissprite_t *vis;

    // Calculate edges of the shape.
    v1[VX] = x;
    v1[VY] = y;

    vis = R_NewVisSprite();
    memset(vis, 0, sizeof(*vis));
    vis->type = VSPR_DECORATION;
    vis->distance = Rend_PointDist2D(v1);
    vis->light = lum;
    vis->center[VX] = x;
    vis->center[VY] = y;
    vis->center[VZ] = z;
}

/**
 * Project all the non-clipped decorations. They become regular vissprites.
 * This is needed for rendering halos.
 */
void Rend_ProjectDecorations(void)
{
    decorsource_t *src;

    // No need for this if no halos are rendered.
    if(!haloMode)
        return;

    for(src = sourceFirst; src != sourceCursor; src = src->next)
    {
        lumobj_t *l = LO_GetLuminous(src->light);

        // Only omni lights get halos.
        if(l->type != LT_OMNI)
            continue;

        // Clipped sources don't get halos.
        if((l->flags & LUMF_CLIPPED) || LUM_OMNI(l)->flareSize <= 0)
            continue;

        projectDecoration(l, src->pos[VX], src->pos[VY], src->pos[VZ]);
    }
}

/**
 * Create a new source for a light decoration.
 */
static decorsource_t *addDecoration(void)
{
    decorsource_t *src;

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
 * A light decoration is created in the specified coordinates.
 * Does largely the same thing as LO_AddLuminous().
 */
static void projectDecorLight(const float pos[3],
                              const ded_decorlight_t *def,
                              const float brightness,
                              const boolean isWall)
{
    decorsource_t *source;
    lumobj_t   *l;
    float       distance = Rend_PointDist3D(pos);
    float       fadeMul = 1, flareMul = 1;
    float       maxDist = (isWall ? decorWallMaxDist : decorPlaneMaxDist);
    uint        i;

    // Is the point in range?
    if(distance > maxDist)
        return;

    // Close enough to the maximum distance, the lights fade out.
    if(distance > .67f * maxDist)
    {
        fadeMul = (maxDist - distance) / (.33f * maxDist);
    }

    // Apply the brightness factor (was calculated using sector lightlevel).
    fadeMul *= brightness * (isWall ? decorWallFactor : decorPlaneFactor);

    // Brightness drops as the angle gets too big.
    if(def->elevation < 2 && decorFadeAngle > 0)    // Close the surface?
    {
        float       vector[3];
        float       dot;

        vector[0] = pos[VX] - vx;
        vector[1] = pos[VZ] - vy;
        vector[2] = pos[VY] - vz;
        M_Normalize(vector);
        dot =
            -(surfaceNormal[VX] * vector[VX] + surfaceNormal[VY] * vector[VY] +
              surfaceNormal[VZ] * vector[VZ]);
        if(dot < decorFadeAngle / 2)
        {
            flareMul = 0;
        }
        else if(dot < 3 * decorFadeAngle)
        {
            flareMul *= (dot - decorFadeAngle / 2) / (2.5f * decorFadeAngle);
        }
    }

    if(fadeMul <= 0)
        return;

    if(!(source = addDecoration()))
        return;                 // Out of sources!

    // Fill in the data for a new luminous object.
    source->light = LO_NewLuminous(LT_OMNI);
    source->pos[VX] = pos[VX];
    source->pos[VY] = pos[VY];
    source->pos[VZ] = pos[VZ];
    l = LO_GetLuminous(source->light);
    l->pos[VX] = source->pos[VX];
    l->pos[VY] = source->pos[VY];
    l->pos[VZ] = source->pos[VZ];
    l->subsector = R_PointInSubsector(l->pos[VX], l->pos[VY]);

    LUM_OMNI(l)->halofactor = 0xff; // Assumed visible.
    LUM_OMNI(l)->zOff = 0;
    l->flags = LUMF_CLIPPED;
    LUM_OMNI(l)->tex = def->sides.tex;
    LUM_OMNI(l)->ceilTex = def->up.tex;
    LUM_OMNI(l)->floorTex = def->down.tex;

    // These are the same rules as in DL_MobjRadius().
    LUM_OMNI(l)->radius = def->radius * 40 * loRadiusFactor;

    // Don't make a too small or too large light.
    if(LUM_OMNI(l)->radius > loMaxRadius)
        LUM_OMNI(l)->radius = loMaxRadius;

    if(def->halo_radius > 0)
    {
        LUM_OMNI(l)->flareSize = def->halo_radius * 60 * (50 + haloSize) / 100.0f;
        if(LUM_OMNI(l)->flareSize < 1)
            LUM_OMNI(l)->flareSize = 1;
    }
    else
    {
        LUM_OMNI(l)->flareSize = 0;
    }

    if(def->flare.disabled)
        l->flags |= LUMF_NOHALO;
    else
    {
        LUM_OMNI(l)->flareCustom = def->flare.custom;
        LUM_OMNI(l)->flareTex = def->flare.tex;
    }

    LUM_OMNI(l)->flareMul = flareMul;

    for(i = 0; i < 3; ++i)
        l->color[i] = def->color[i] * fadeMul;

    // Approximate the distance.
    l->distanceToViewer =
            P_ApproxDistance3(l->pos[VX] - viewX,
                              l->pos[VY] - viewY,
                              l->pos[VZ] - viewZ);
}

/**
 * @return                  @c true, if the view point is close enough to
 *                          the bounding box so that there could be visible
 *                          decorations inside.
 */
static __inline
boolean pointInBounds(const float bounds[6], const float viewer[3],
                      const float maxDist)
{
    return viewer[VX] > bounds[BOXLEFT]    - maxDist &&
           viewer[VX] < bounds[BOXRIGHT]   + maxDist &&
           viewer[VY] > bounds[BOXBOTTOM]  - maxDist &&
           viewer[VY] < bounds[BOXTOP]     + maxDist &&
           viewer[VZ] > bounds[BOXFLOOR]   - maxDist &&
           viewer[VZ] < bounds[BOXCEILING] + maxDist;
}

/**
 * @return                  @c > 0, if the sector lightlevel passes the
 *                          limit condition.
 */
static float checkSectorLight(float lightlevel,
                              const ded_decorlight_t *lightDef)
{
    float       factor;

    // Has a limit been set?
    if(lightDef->lightlevels[0] == lightDef->lightlevels[1])
        return 1;

    // Apply adaptation
    Rend_ApplyLightAdaptation(&lightlevel);

    factor = (lightlevel - lightDef->lightlevels[0]) /
        (float) (lightDef->lightlevels[1] - lightDef->lightlevels[0]);

    if(factor < 0)
        return 0;
    if(factor > 1)
        return 1;

    return factor;
}

static void projectSurfaceDecorations(const surface_t *suf,
                                      float lightLevel)
{
    uint            i;

    for(i = 0; i < MAX_SURFACE_DECORATIONS; ++i)
    {
        float           brightMul;
        const surfacedecor_t *d = &suf->decorations[i];

        if(!R_IsValidLightDecoration(d->def))
            break;

        // Does it pass the sectorlight limitation?
        if((brightMul = checkSectorLight(lightLevel, d->def)) > 0)
            projectDecorLight(d->pos, d->def, brightMul, false);
    }
}

/**
 * Determine proper skip values.
 */
static void getDecorationSkipPattern(const ded_decorlight_t *lightDef,
                                     int *skip)
{
    uint        i;

    for(i = 0; i < 2; ++i)
    {
        // Skip must be at least one.
        skip[i] = lightDef->pattern_skip[i] + 1;

        if(skip[i] < 1)
            skip[i] = 1;
    }
}

/**
 * Generate decorations for the specified section of a line.
 */
static void decorateLineSection(const line_t *line, side_t *side,
                                surface_t *suf, float top,
                                float bottom, float texOffY,
                                ded_decor_t *def)
{
    if(suf->flags & SUF_UPDATE_DECORATIONS)
    {
        ded_decorlight_t *lightDef;
        vertex_t   *v[2];
        float       lh, s, t;           // Horizontal and vertical offset.
        float       posBase[2], delta[2], pos[3];
        float       surfTexW, surfTexH, patternW, patternH;
        int         skip[2];
        uint        i, n;
        texinfo_t  *texinfo;

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
        surfaceNormal[VX] = delta[VY] / line->length;
        surfaceNormal[VZ] = -delta[VX] / line->length;
        surfaceNormal[VY] = 0;

        // Height of the section.
        lh = top - bottom;

        // Setup the global texture info variables.
        GL_GetMaterialInfo(suf->material->ofTypeID,
                           suf->material->type, &texinfo);

        surfTexW = texinfo->width;
        surfTexH = texinfo->height;

        // Generate a number of lights.
        for(i = 0, n = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
        {
            lightDef = def->lights + i;

            // No more?
            if(!R_IsValidLightDecoration(lightDef))
                break;

            // Skip must be at least one.
            getDecorationSkipPattern(lightDef, skip);

            posBase[VX] = v[0]->V_pos[VX] + lightDef->elevation * surfaceNormal[VX];
            posBase[VY] = v[0]->V_pos[VY] + lightDef->elevation * surfaceNormal[VZ];

            patternW = surfTexW * skip[VX];
            patternH = surfTexH * skip[VY];

            // Let's see where the top left light is.
            s = M_CycleIntoRange(lightDef->pos[VX] - suf->offset[VX] -
                                 surfTexW * lightDef->pattern_offset[VX],
                                 patternW);

            for(; s < line->length; s += patternW)
            {
                t = M_CycleIntoRange(lightDef->pos[VY] - suf->offset[VY] -
                                     surfTexH * lightDef->pattern_offset[VY] +
                                     texOffY, patternH);

                for(; t < lh; t += patternH)
                {
                    pos[VX] = posBase[VX] + delta[VX] * s / line->length;
                    pos[VY] = posBase[VY] + delta[VY] * s / line->length;
                    pos[VZ] = top - t;

                    if(n < MAX_SURFACE_DECORATIONS)
                    {
                        suf->decorations[n].pos[VX] = pos[VX];
                        suf->decorations[n].pos[VY] = pos[VY];
                        suf->decorations[n].pos[VZ] = pos[VZ];
                        suf->decorations[n].def = lightDef;
                        n++;
                    }
                }
            }
        }

        suf->flags &= ~SUF_UPDATE_DECORATIONS;
    }

    projectSurfaceDecorations(suf, side->sector->lightlevel);
}

/**
 * @return              The side that faces the sector (if any).
 */
static side_t *getSectorSide(const line_t *line, sector_t *sector)
{
    side_t *side = line->L_frontside;

    // Swap if that wasn't the right one.
    if(side->sector != sector)
        return line->L_backside;

    return side;
}

/**
 * @return              @c true, if the line is within the visible
 *                      decoration 'box'.
 */
static boolean checkLineDecorationBounds(const line_t *line,
                                         const float *viewer,
                                         const float maxDist)
{
    float       bounds[6];
    sector_t   *sector;

    bounds[BOXLEFT]   = line->bbox[BOXLEFT];
    bounds[BOXRIGHT]  = line->bbox[BOXRIGHT];
    bounds[BOXTOP]    = line->bbox[BOXTOP];
    bounds[BOXBOTTOM] = line->bbox[BOXBOTTOM];

    // Figure out the highest and lowest Z height.
    sector = line->L_frontsector;
    bounds[BOXFLOOR]   = sector->SP_floorheight;
    bounds[BOXCEILING] = sector->SP_ceilheight;

    // Is the other sector higher/lower?
    sector = (line->L_backside? line->L_backsector : NULL);
    if(sector)
    {
        float       bfloor = sector->SP_floorheight;
        float       bceil  = sector->SP_ceilheight;

        if(bfloor < bounds[BOXFLOOR])
            bounds[BOXFLOOR] = bfloor;

        if(bceil > bounds[BOXCEILING])
            bounds[BOXCEILING] = bceil;
    }

    return pointInBounds(bounds, viewer, maxDist);
}

/**
 * @return              @c true, if the sector is within the visible
 *                      decoration 'box'.
 */
static boolean checkSectorDecorationBounds(const sector_t *sector,
                                           const float *viewer,
                                           const float maxDist)
{
    float       bounds[6];

    bounds[BOXLEFT]    = sector->bbox[BOXLEFT];
    bounds[BOXRIGHT]   = sector->bbox[BOXRIGHT];
    bounds[BOXBOTTOM]  = sector->bbox[BOXBOTTOM];
    bounds[BOXTOP]     = sector->bbox[BOXTOP];

    bounds[BOXFLOOR]   = sector->SP_floorvisheight;
    bounds[BOXCEILING] = sector->SP_ceilvisheight;

    return pointInBounds(bounds, viewer, maxDist);
}

static void decorateLine(const line_t *line)
{
    side_t         *side;
    sector_t       *highSector, *lowSector;
    float           frontCeil, frontFloor, backCeil, backFloor;
    surface_t      *suf;

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

            if(suf->material &&
               (suf->material->type == MAT_TEXTURE ||
                suf->material->type == MAT_FLAT))
            {
                float           bottom = lowSector->SP_ceilvisheight;
                float           top = highSector->SP_ceilvisheight;

                // Is this a valid section?
                if(bottom < top && line->length > 0)
                {
                    ded_decor_t    *def;

                    // Should this be decorated at all?
                    def = getMaterialDecoration(suf->material);
                    if(def)
                    {
                        float           offsetY;

                        if(line->mapflags & ML_DONTPEGTOP)
                        {
                            offsetY = 0;
                        }
                        else
                        {
                            texinfo_t      *texinfo;

                            GL_GetMaterialInfo(suf->material->ofTypeID,
                                               suf->material->type, &texinfo);
                            offsetY = -texinfo->height + (top - bottom);
                        }

                        decorateLineSection(line, side, suf,
                                            top, bottom, offsetY, def);
                    }
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

            if(suf->material &&
               (suf->material->type == MAT_TEXTURE ||
                suf->material->type == MAT_FLAT))
            {
                float           bottom = lowSector->SP_ceilvisheight;
                float           top = highSector->SP_floorvisheight;

                // Is this a valid section?
                if(bottom < top && line->length > 0)
                {
                    ded_decor_t    *def;

                    // Should this be decorated at all?
                    def = getMaterialDecoration(suf->material);
                    if(def)
                    {
                        float           offsetY;

                        if(line->mapflags & ML_DONTPEGBOTTOM)
                            offsetY = (top - bottom);
                        else
                            offsetY = 0;

                        decorateLineSection(line, side, suf,
                                            top, bottom, offsetY, def);
                    }
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

        if(suf->material &&
           (suf->material->type == MAT_TEXTURE ||
            suf->material->type == MAT_FLAT))
        {
            float           bottom = frontFloor;
            float           top = frontCeil;

            // Is this a valid section?
            if(bottom < top && line->length > 0)
            {
                ded_decor_t    *def;

                // Should this be decorated at all?
                def = getMaterialDecoration(suf->material);
                if(def)
                {
                    float           offsetY;

                    if(line->mapflags & ML_DONTPEGBOTTOM)
                    {
                        texinfo_t      *texinfo;

                        GL_GetMaterialInfo(suf->material->ofTypeID,
                                           suf->material->type, &texinfo);
                        offsetY = -texinfo->height + (top - bottom);
                    }
                    else
                    {
                        offsetY = 0;
                    }

                    decorateLineSection(line, side, suf,
                                        top, bottom, offsetY, def);
                }
            }
        }
    }
}

/**
 * Generate decorations for upper, middle and bottom parts of the line,
 * on both sides.
 */
static void Rend_DecorateLine(const line_t *line, const float viewer[3],
                              const float maxDist)
{
    // Only the lines within the decoration visibility bounding box
    // are processed.
    if(checkLineDecorationBounds(line, viewer, maxDist))
        decorateLine(line);
}

/**
 * Generate decorations for a plane.
 */
static void decoratePlane(const sector_t *sec, plane_t *pln,
                          ded_decor_t *def)
{
    uint                i;
    surface_t          *suf = &pln->surface;

    if(suf->flags & SUF_UPDATE_DECORATIONS)
    {
        uint                n;
        float               pos[3], tileSize = 64;
        int                 skip[2];
        ded_decorlight_t   *lightDef;

        surfaceNormal[VX] = suf->normal[VX];
        surfaceNormal[VY] = suf->normal[VY];
        surfaceNormal[VZ] = suf->normal[VZ];

        memset(suf->decorations, 0, sizeof(suf->decorations));

        // Generate a number of lights.
        for(i = 0, n = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
        {
            lightDef = &def->lights[i];

            // No more?
            if(!R_IsValidLightDecoration(lightDef))
                break;

            // Skip must be at least one.
            getDecorationSkipPattern(lightDef, skip);

            pos[VY] =
                (int) (sec->bbox[BOXBOTTOM] / tileSize) * tileSize - pln->PS_offset[VY] -
                lightDef->pos[VY] - lightDef->pattern_offset[VY] * tileSize;

            while(pos[VY] > sec->bbox[BOXBOTTOM])
                pos[VY] -= tileSize * skip[VY];

            for(; pos[VY] < sec->bbox[BOXTOP]; pos[VY] += tileSize * skip[VY])
            {
                if(pos[VY] < sec->bbox[BOXBOTTOM])
                    continue;

                pos[VX] =
                    (int) (sec->bbox[BOXLEFT] / tileSize) * tileSize - pln->PS_offset[VX] +
                    lightDef->pos[VX] - lightDef->pattern_offset[VX] * tileSize;

                while(pos[VX] > sec->bbox[BOXLEFT])
                    pos[VX] -= tileSize * skip[VX];

                for(; pos[VX] < sec->bbox[BOXRIGHT];
                    pos[VX] += tileSize * skip[VX])
                {
                    if(pos[VX] < sec->bbox[BOXLEFT])
                        continue;

                    // The point must be inside the correct sector.
                    if(!R_IsPointInSector(pos[VX], pos[VY], sec))
                        continue;

                    pos[VZ] =
                        pln->visheight + lightDef->elevation * surfaceNormal[VZ];

                    if(n < MAX_SURFACE_DECORATIONS)
                    {
                        suf->decorations[n].pos[VX] = pos[VX];
                        suf->decorations[n].pos[VY] = pos[VY];
                        suf->decorations[n].pos[VZ] = pos[VZ];
                        suf->decorations[n].def = lightDef;
                        n++;
                    }
                }
            }
        }

        suf->flags &= ~SUF_UPDATE_DECORATIONS;
    }

    projectSurfaceDecorations(suf, sec->lightlevel);
}

static void decorateSector(const sector_t *sec)
{
    uint        i;
    plane_t    *pln;
    ded_decor_t *def;

    for(i = 0; i < sec->planecount; ++i)
    {
        pln = sec->SP_plane(i);
        def = getMaterialDecoration(pln->PS_material);

        if(def != NULL) // The surface is decorated.
            decoratePlane(sec, pln, def);
    }
}

/**
 * Generate decorations for the planes of the sector.
 */
static void Rend_DecorateSector(const sector_t *sec,
                                const float viewer[3],
                                const float maxDist)
{
    // The sector must have height if it wants decorations.
    if(sec->SP_ceilheight <= sec->SP_floorheight)
        return;

    // Is this sector close enough for the decorations to be visible?
    if(checkSectorDecorationBounds(sec, viewer, maxDist))
        decorateSector(sec);
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
        uint        i;
        float       viewer[3], maxDist;

        viewer[VX] = viewX;
        viewer[VY] = viewY;
        viewer[VZ] = viewZ;

        // Process all lines. This could also be done during sectors,
        // but validCount would need to be used to prevent duplicate
        // processing.
        maxDist = decorWallMaxDist;
        for(i = 0; i < numlines; ++i)
            Rend_DecorateLine(&lines[i], viewer, maxDist);

        // Process all planes.
        maxDist = decorPlaneMaxDist;
        for(i = 0; i < numsectors; ++i)
            Rend_DecorateSector(&sectors[i], viewer, maxDist);
    }
}
