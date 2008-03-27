/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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

// Quite a bit of lights, there!
#define MAX_SOURCES     16384

// TYPES -------------------------------------------------------------------

typedef struct decorsource_s {
    float           pos[3];
    float           maxDist;
    const surface_t *surface;
    subsector_t    *subsector;
    const ded_decorlight_t *def;
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
static decorsource_t *sourceFirst, *sourceLast, *sourceCursor;

// CODE --------------------------------------------------------------------

void Rend_DecorRegister(void)
{
    C_VAR_BYTE("rend-light-decor", &useDecorations, 0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-far", &decorMaxDist, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-light-decor-bright", &decorFactor, 0, 0, 10);
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

    // Does it pass the sectorlight limitation?
    if(!((brightness = checkSectorLight(src->subsector->sector->lightLevel,
                                        src->def)) > 0))
        return;

    // Close enough to the maximum distance, the lights fade out.
    if(distance > .67f * src->maxDist)
    {
        fadeMul = (src->maxDist - distance) / (.33f * src->maxDist);
    }

    // Apply the brightness factor (was calculated using sector lightlevel).
    fadeMul *= brightness * decorFactor;

    // Brightness drops as the angle gets too big.
    if(src->def->elevation < 2 && decorFadeAngle > 0) // Close the surface?
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

    light = LO_NewLuminous(LT_OMNI);
    l = LO_GetLuminous(light);

    l->pos[VX] = src->pos[VX];
    l->pos[VY] = src->pos[VY];
    l->pos[VZ] = src->pos[VZ];
    l->subsector = src->subsector;

    LUM_OMNI(l)->haloFactor = 0xff; // Assumed visible.
    LUM_OMNI(l)->zOff = 0;
    l->flags = LUMF_CLIPPED;
    LUM_OMNI(l)->tex = src->def->sides.tex;
    LUM_OMNI(l)->ceilTex = src->def->up.tex;
    LUM_OMNI(l)->floorTex = src->def->down.tex;

    // These are the same rules as in DL_MobjRadius().
    LUM_OMNI(l)->radius = src->def->radius * 40 * loRadiusFactor;

    // Don't make a too small or too large light.
    if(LUM_OMNI(l)->radius > loMaxRadius)
        LUM_OMNI(l)->radius = loMaxRadius;

    if(src->def->haloRadius > 0)
    {
        LUM_OMNI(l)->flareSize =
            src->def->haloRadius * 60 * (50 + haloSize) / 100.0f;
        if(LUM_OMNI(l)->flareSize < 1)
            LUM_OMNI(l)->flareSize = 1;
    }
    else
    {
        LUM_OMNI(l)->flareSize = 0;
    }

    if(src->def->flare.disabled)
    {
        l->flags |= LUMF_NOHALO;
    }
    else
    {
        LUM_OMNI(l)->flareCustom = src->def->flare.custom;
        LUM_OMNI(l)->flareTex = src->def->flare.tex;
    }

    LUM_OMNI(l)->flareMul = flareMul;

    for(i = 0; i < 3; ++i)
        l->color[i] = src->def->color[i] * fadeMul;

    // Approximate the distance.
    l->distanceToViewer =
            P_ApproxDistance3(l->pos[VX] - viewX,
                              l->pos[VY] - viewY,
                              l->pos[VZ] - viewZ);

    vis->light = l;
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
static void createSurfaceDecoration(const surface_t *suf, const float pos[3],
                                    const ded_decorlight_t *def,
                                    const float maxDist)
{
    decorsource_t      *source = addDecoration();

    if(!source)
        return; // Out of sources!

    // Fill in the data for a new surface decoration.
    source->pos[VX] = pos[VX];
    source->pos[VY] = pos[VY];
    source->pos[VZ] = pos[VZ];
    source->maxDist = maxDist;
    source->def = def;
    source->subsector = R_PointInSubsector(pos[VX], pos[VY]);
    source->surface = suf;
}

/**
 * @return              @c true, if the view point is close enough to
 *                      the bounding box so that there could be visible
 *                      decorations inside.
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

static void projectSurfaceDecorations(const surface_t *suf, float maxDist)
{
    uint                i;

    for(i = 0; i < suf->numDecorations; ++i)
    {
        const surfacedecor_t *d = &suf->decorations[i];

        if(!R_IsValidLightDecoration(d->def))
            break;

        createSurfaceDecoration(suf, d->pos, d->def, maxDist);
    }
}

/**
 * Determine proper skip values.
 */
static void getDecorationSkipPattern(const ded_decorlight_t *lightDef,
                                     int *skip)
{
    uint                i;

    for(i = 0; i < 2; ++i)
    {
        // Skip must be at least one.
        skip[i] = lightDef->patternSkip[i] + 1;

        if(skip[i] < 1)
            skip[i] = 1;
    }
}

/**
 * Generate decorations for the specified section of a line.
 */
static void decorateLineSection(const linedef_t *line, sidedef_t *side,
                                surface_t *suf, float top,
                                float bottom, float texOffY,
                                ded_decor_t *def, const float maxDist)
{
    if(suf->flags & SUF_UPDATE_DECORATIONS)
    {
        ded_decorlight_t   *lightDef;
        float               lh, s, t; // Horizontal and vertical offset.
        float               posBase[2], pos[3];
        float               surfTexW, surfTexH, patternW, patternH;
        int                 skip[2];
        uint                i;
        texinfo_t          *texinfo;
        vertex_t           *v[2];
        float               delta[2];

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

        R_ClearSurfaceDecorations(suf);

        // Height of the section.
        lh = top - bottom;

        // Setup the global texture info variables.
        GL_GetMaterialInfo(suf->material->ofTypeID,
                           suf->material->type, &texinfo);

        surfTexW = texinfo->width;
        surfTexH = texinfo->height;

        // Generate a number of lights.
        for(i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
        {
            lightDef = def->lights + i;

            // No more?
            if(!R_IsValidLightDecoration(lightDef))
                break;

            // Skip must be at least one.
            getDecorationSkipPattern(lightDef, skip);

            posBase[VX] = v[0]->V_pos[VX] + lightDef->elevation * suf->normal[VX];
            posBase[VY] = v[0]->V_pos[VY] + lightDef->elevation * suf->normal[VZ];

            patternW = surfTexW * skip[VX];
            patternH = surfTexH * skip[VY];

            // Let's see where the top left light is.
            s = M_CycleIntoRange(lightDef->pos[VX] - suf->offset[VX] -
                                 surfTexW * lightDef->patternOffset[VX],
                                 patternW);

            for(; s < line->length; s += patternW)
            {
                t = M_CycleIntoRange(lightDef->pos[VY] - suf->offset[VY] -
                                     surfTexH * lightDef->patternOffset[VY] +
                                     texOffY, patternH);

                for(; t < lh; t += patternH)
                {
                    surfacedecor_t         *d;

                    pos[VX] = posBase[VX] + delta[VX] * s / line->length;
                    pos[VY] = posBase[VY] + delta[VY] * s / line->length;
                    pos[VZ] = top - t;

                    if(NULL != (d = R_CreateSurfaceDecoration(suf, pos)))
                    {
                        d->def = lightDef;
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

/**
 * @return              @c true, if the line is within the visible
 *                      decoration 'box'.
 */
static boolean checkLineDecorationBounds(const linedef_t *line,
                                         const float *viewer,
                                         const float maxDist)
{
    float               bounds[6];
    sector_t           *sector;

    bounds[BOXLEFT]   = line->bBox[BOXLEFT];
    bounds[BOXRIGHT]  = line->bBox[BOXRIGHT];
    bounds[BOXTOP]    = line->bBox[BOXTOP];
    bounds[BOXBOTTOM] = line->bBox[BOXBOTTOM];

    // Figure out the highest and lowest Z height.
    sector = line->L_frontsector;
    bounds[BOXFLOOR]   = sector->SP_floorheight;
    bounds[BOXCEILING] = sector->SP_ceilheight;

    // Is the other sector higher/lower?
    sector = (line->L_backside? line->L_backsector : NULL);
    if(sector)
    {
        float               bfloor = sector->SP_floorheight;
        float               bceil  = sector->SP_ceilheight;

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
    float               bounds[6];

    bounds[BOXLEFT]    = sector->bBox[BOXLEFT];
    bounds[BOXRIGHT]   = sector->bBox[BOXRIGHT];
    bounds[BOXBOTTOM]  = sector->bBox[BOXBOTTOM];
    bounds[BOXTOP]     = sector->bBox[BOXTOP];

    bounds[BOXFLOOR]   = sector->SP_floorvisheight;
    bounds[BOXCEILING] = sector->SP_ceilvisheight;

    return pointInBounds(bounds, viewer, maxDist);
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
                    ded_decor_t        *def;

                    // Should this be decorated at all?
                    def = getMaterialDecoration(suf->material);
                    if(def)
                    {
                        float               offsetY;

                        if(line->flags & DDLF_DONTPEGTOP)
                        {
                            offsetY = 0;
                        }
                        else
                        {
                            texinfo_t          *texinfo;

                            GL_GetMaterialInfo(suf->material->ofTypeID,
                                               suf->material->type, &texinfo);
                            offsetY = -texinfo->height + (top - bottom);
                        }

                        decorateLineSection(line, side, suf, top, bottom,
                                            offsetY, def, maxDist);
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

            if(suf->material)
            {
                float               bottom = lowSector->SP_ceilvisheight;
                float               top = highSector->SP_floorvisheight;

                // Is this a valid section?
                if(bottom < top && line->length > 0)
                {
                    ded_decor_t        *def;

                    // Should this be decorated at all?
                    def = getMaterialDecoration(suf->material);
                    if(def)
                    {
                        float               offsetY;

                        if(line->flags & DDLF_DONTPEGBOTTOM)
                            offsetY = (top - bottom);
                        else
                            offsetY = 0;

                        decorateLineSection(line, side, suf, top, bottom,
                                            offsetY, def, maxDist);
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

        if(suf->material)
        {
            float               bottom = frontFloor;
            float               top = frontCeil;

            // Is this a valid section?
            if(bottom < top && line->length > 0)
            {
                ded_decor_t        *def;

                // Should this be decorated at all?
                def = getMaterialDecoration(suf->material);
                if(def)
                {
                    float               offsetY;

                    if(line->flags & DDLF_DONTPEGBOTTOM)
                    {
                        texinfo_t          *texinfo;

                        GL_GetMaterialInfo(suf->material->ofTypeID,
                                           suf->material->type, &texinfo);
                        offsetY = -texinfo->height + (top - bottom);
                    }
                    else
                    {
                        offsetY = 0;
                    }

                    decorateLineSection(line, side, suf, top, bottom,
                                        offsetY, def, maxDist);
                }
            }
        }
    }
}

/**
 * Generate decorations for upper, middle and bottom parts of the line,
 * on both sides.
 */
static void Rend_DecorateLine(const linedef_t *line, const float viewer[3],
                              const float maxDist)
{
    // Only the lines within the decoration visibility bounding box
    // are processed.
    if(checkLineDecorationBounds(line, viewer, maxDist))
        decorateLine(line, maxDist);
}

/**
 * Generate decorations for a plane.
 */
static void decoratePlane(const sector_t *sec, plane_t *pln,
                          ded_decor_t *def, const float maxDist)
{
    uint                i;
    surface_t          *suf = &pln->surface;

    if(suf->flags & SUF_UPDATE_DECORATIONS)
    {
        float               pos[3], tileSize = 64;
        int                 skip[2];
        ded_decorlight_t   *lightDef;

        R_ClearSurfaceDecorations(suf);

        // Generate a number of lights.
        for(i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
        {
            lightDef = &def->lights[i];

            // No more?
            if(!R_IsValidLightDecoration(lightDef))
                break;

            // Skip must be at least one.
            getDecorationSkipPattern(lightDef, skip);

            pos[VY] =
                (int) (sec->bBox[BOXBOTTOM] / tileSize) * tileSize - pln->PS_offset[VY] -
                lightDef->pos[VY] - lightDef->patternOffset[VY] * tileSize;

            while(pos[VY] > sec->bBox[BOXBOTTOM])
                pos[VY] -= tileSize * skip[VY];

            for(; pos[VY] < sec->bBox[BOXTOP]; pos[VY] += tileSize * skip[VY])
            {
                if(pos[VY] < sec->bBox[BOXBOTTOM])
                    continue;

                pos[VX] =
                    (int) (sec->bBox[BOXLEFT] / tileSize) * tileSize - pln->PS_offset[VX] +
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
                        pln->visHeight + lightDef->elevation * suf->normal[VY];

                    if(NULL != (d = R_CreateSurfaceDecoration(suf, pos)))
                    {
                        d->def = lightDef;
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
    plane_t            *pln;
    ded_decor_t        *def;

    for(i = 0; i < sec->planeCount; ++i)
    {
        pln = sec->SP_plane(i);
        def = getMaterialDecoration(pln->PS_material);

        if(def != NULL) // The surface is decorated.
            decoratePlane(sec, pln, def, maxDist);
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
        decorateSector(sec, maxDist);
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
        float               viewer[3];

        viewer[VX] = viewX;
        viewer[VY] = viewY;
        viewer[VZ] = viewZ;

        // Process all lines. This could also be done during sectors,
        // but validCount would need to be used to prevent duplicate
        // processing.
        for(i = 0; i < numLineDefs; ++i)
            Rend_DecorateLine(&lineDefs[i], viewer, decorMaxDist);

        // Process all planes.
        for(i = 0; i < numSectors; ++i)
            Rend_DecorateSector(&sectors[i], viewer, decorMaxDist);
    }
}
