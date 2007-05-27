/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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

static unsigned int numDecorLightSources;
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
 * Returns a pointer to the surface decoration, if any.
 */
static ded_decor_t *Rend_GetGraphicResourceDecoration(int id, boolean isFlat)
{
    if(!id)
        return NULL;

    if(isFlat)
        return flats[flattranslation[id].current]->decoration;
    else
        return textures[texturetranslation[id].current]->decoration;
}

/**
 * Clears the list of decoration dummies.
 */
static void Rend_ClearDecorations(void)
{
    numDecorLightSources = 0;
    sourceCursor = sourceFirst;
}

static void R_ProjectDecoration(decorsource_t *source)
{
    float   v1[2];
    vissprite_t *vis;

    // Calculate edges of the shape.
    v1[VX] = source->pos[VX];
    v1[VY] = source->pos[VY];

    vis = R_NewVisSprite();
    memset(vis, 0, sizeof(*vis));
    vis->type = VSPR_DECORATION;
    vis->distance = Rend_PointDist2D(v1);
    vis->light = DL_GetLuminous(source->light);
    vis->center[VX] = source->pos[VX];
    vis->center[VY] = source->pos[VY];
    vis->center[VZ] = source->pos[VZ];
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
        lumobj_t *lum = DL_GetLuminous(src->light);

        // Clipped sources don't get halos.
        if(lum->flags & LUMF_CLIPPED || lum->flareSize <= 0)
            continue;

        R_ProjectDecoration(src);
    }
}

/**
 * Create a new source for a light decoration.
 */
static decorsource_t *Rend_NewLightDecorationSource(void)
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
 * Does largely the same thing as DL_AddLuminous().
 */
static void Rend_AddLightDecoration(float pos[3], ded_decorlight_t *def,
                                    float brightness, boolean isWall,
                                    DGLuint decorMap)
{
    decorsource_t *source;
    lumobj_t   *lum;
    float       distance = Rend_PointDist3D(pos);
    float       fadeMul = 1, flareMul = 1;
    float       maxDist = (isWall ? decorWallMaxDist : decorPlaneMaxDist);
    unsigned int i;

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
        float   vector[3];
        float   dot;

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

    if(!(source = Rend_NewLightDecorationSource()))
        return;                 // Out of sources!

    // Fill in the data for a new luminous object.
    source->light = DL_NewLuminous();
    source->pos[VX] = pos[VX];
    source->pos[VY] = pos[VY];
    source->pos[VZ] = pos[VZ];
    lum = DL_GetLuminous(source->light);
    lum->pos[VX] = source->pos[VX];
    lum->pos[VY] = source->pos[VY];
    lum->pos[VZ] = source->pos[VZ];
    lum->subsector = R_PointInSubsector(FLT2FIX(lum->pos[VX]), FLT2FIX(lum->pos[VY]));
    lum->halofactor = 0xff; // Assumed visible.
    lum->zOff = 0;
    lum->flags = LUMF_CLIPPED;
    lum->tex = def->sides.tex;
    lum->ceilTex = def->up.tex;
    lum->floorTex = def->down.tex;

    // These are the same rules as in DL_ThingRadius().
    lum->radius = def->radius * 40 * dlRadFactor;

    // Don't make a too small or too large light.
    if(lum->radius > dlMaxRad)
        lum->radius = dlMaxRad;

    if(def->halo_radius > 0)
    {
        lum->flareSize = def->halo_radius * 60 * (50 + haloSize) / 100.0f;
        if(lum->flareSize < 1)
            lum->flareSize = 1;
    }
    else
    {
        lum->flareSize = 0;
    }

    if(def->flare.disabled)
        lum->flags |= LUMF_NOHALO;
    else
    {
        lum->flareCustom = def->flare.custom;
        lum->flareTex = def->flare.tex;
    }

    lum->flareMul = flareMul;

    // This light source is associated with a decoration map, if one is
    // available.
    lum->decorMap = decorMap;

    for(i = 0; i < 3; ++i)
        lum->rgb[i] = def->color[i] * fadeMul;

    // Approximate the distance.
    lum->distance =
        P_ApproxDistance3(FLT2FIX(lum->pos[VX]) - viewx,
                          FLT2FIX(lum->pos[VY]) - viewy,
                          FLT2FIX(lum->pos[VZ]) - viewz);
}

/**
 * Returns true if the view point is close enough to the bounding box
 * so that there could be visible decorations inside.
 */
static boolean Rend_CheckDecorationBounds(fixed_t bounds[6], float fMaxDist)
{
    fixed_t maxDist = FRACUNIT * fMaxDist;

    return viewx > bounds[BLEFT] - maxDist   && viewx < bounds[BRIGHT] + maxDist
        && viewy > bounds[BBOTTOM] - maxDist && viewy < bounds[BTOP] + maxDist
        && viewz > bounds[BFLOOR] - maxDist  && viewz < bounds[BCEILING] + maxDist;
}

/**
 * Returns > 0 if the sector lightlevel passes the limit condition.
 */
static float Rend_CheckSectorLight(sector_t *sector, ded_decorlight_t *lightDef)
{
    float       lightlevel;
    float       factor;

    lightlevel = sector->lightlevel;

    // Has a limit been set?
    if(lightDef->lightlevels[0] == lightDef->lightlevels[1])
        return 1;

    // Apply adaptation
    Rend_ApplyLightAdaptation(&lightlevel);

    factor =
        (lightlevel -
         lightDef->lightlevels[0]) / (float) (lightDef->lightlevels[1] -
                                               lightDef->lightlevels[0]);
    if(factor < 0)
        return 0;
    if(factor > 1)
        return 1;
    return factor;
}

/**
 * Determine proper skip values.
 */
static void Rend_DecorationPatternSkip(ded_decorlight_t *lightDef, int *skip)
{
    unsigned int k;

    for(k = 0; k < 2; ++k)
    {
        // Skip must be at least one.
        skip[k] = lightDef->pattern_skip[k] + 1;
        if(skip[k] < 1)
            skip[k] = 1;
    }
}

/**
 * Generate decorations for the specified section of a line.
 */
static void Rend_DecorateLineSection(line_t *line, side_t *side,
                                     surface_t *surface, float top,
                                     float bottom, float texOffY)
{
    ded_decor_t *def;
    ded_decorlight_t *lightDef;
    vertex_t   *v[2];
    float       lh, s, t;           // Horizontal and vertical offset.
    float       posBase[2], delta[2], pos[3], brightMul;
    float       surfTexW, surfTexH, patternW, patternH;
    int         skip[2];
    uint        i;
    texinfo_t  *texinfo;

    // Is this a valid section?
    if(bottom > top || line->length == 0)
        return;

    // Should this be decorated at all?
    if(!(def = Rend_GetGraphicResourceDecoration(surface->SM_texture,
                                                 surface->SM_isflat)))
        return;

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

    delta[VX] = v[1]->pos[VX] - v[0]->pos[VX];
    delta[VY] = v[1]->pos[VY] - v[0]->pos[VY];
    surfaceNormal[VX] = delta[VY] / line->length;
    surfaceNormal[VZ] = -delta[VX] / line->length;
    surfaceNormal[VY] = 0;

    // Height of the section.
    lh = top - bottom;

    // Setup the global texture info variables.
    if(surface->SM_isflat)
        GL_GetFlatInfo(surface->SM_texture, &texinfo);
    else
        GL_GetTextureInfo(surface->SM_texture, &texinfo);

    surfTexW = texinfo->width;
    surfTexH = texinfo->height;

    // Generate a number of lights.
    for(i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
    {
        lightDef = def->lights + i;

        // No more?
        if(!R_IsValidLightDecoration(lightDef))
            break;

        // Does it pass the sectorlight limitation?
        if((brightMul = Rend_CheckSectorLight(side->sector, lightDef)) <= 0)
            continue;

        // Skip must be at least one.
        Rend_DecorationPatternSkip(lightDef, skip);

        posBase[VX] = v[0]->pos[VX] + lightDef->elevation * surfaceNormal[VX];
        posBase[VY] = v[0]->pos[VY] + lightDef->elevation * surfaceNormal[VZ];

        patternW = surfTexW * skip[VX];
        patternH = surfTexH * skip[VY];

        // Let's see where the top left light is.
        s = M_CycleIntoRange(lightDef->pos[VX] - surface->offx -
                             surfTexW * lightDef->pattern_offset[VX],
                             patternW);

        for(; s < line->length; s += patternW)
        {
            t = M_CycleIntoRange(lightDef->pos[VY] - surface->offy -
                                 surfTexH * lightDef->pattern_offset[VY] +
                                 texOffY, patternH);

            for(; t < lh; t += patternH)
            {
                // Let there be light.
                pos[VX] = posBase[VX] + delta[VX] * s / line->length;
                pos[VY] = posBase[VY] + delta[VY] * s / line->length;
                pos[VZ] = top - t;
                Rend_AddLightDecoration(pos, lightDef, brightMul, true,
                                        def->pregen_lightmap);
            }
        }
    }
}

/**
 * Returns the side that faces the sector (if any).
 */
static side_t *R_GetSectorSide(line_t *line, sector_t *sector)
{
    side_t *side = line->L_frontside;

    // Swap if that wasn't the right one.
    if(side->sector != sector)
        return line->L_backside;

    return side;
}

/**
 * Return true if the line is within the visible decoration 'box'.
 */
static boolean Rend_LineDecorationBounds(line_t *line)
{
    fixed_t     bounds[6];
    sector_t   *sector;

    bounds[BLEFT]   = line->bbox[BOXLEFT];
    bounds[BRIGHT]  = line->bbox[BOXRIGHT];
    bounds[BTOP]    = line->bbox[BOXTOP];
    bounds[BBOTTOM] = line->bbox[BOXBOTTOM];

    // Figure out the highest and lowest Z height.
    sector = line->L_frontsector;
    bounds[BFLOOR]   = FLT2FIX(sector->SP_floorheight);
    bounds[BCEILING] = FLT2FIX(sector->SP_ceilheight);

    // Is the other sector higher/lower?
    sector = (line->L_backside? line->L_backsector : NULL);
    if(sector)
    {
        float bfloor = FLT2FIX(sector->SP_floorheight);
        float bceil  = FLT2FIX(sector->SP_ceilheight);

        if(bfloor < bounds[BFLOOR])
            bounds[BFLOOR] = bfloor;

        if(bceil > bounds[BCEILING])
            bounds[BCEILING] = bceil;
    }

    return Rend_CheckDecorationBounds(bounds, decorWallMaxDist);
}

/**
 * Return true if the sector is within the visible decoration 'box'.
 */
static boolean Rend_SectorDecorationBounds(sector_t *sector)
{
    fixed_t     bounds[6];

    bounds[BLEFT]    = FRACUNIT * sector->bounds[BLEFT];
    bounds[BRIGHT]   = FRACUNIT * sector->bounds[BRIGHT];

    // Sectorinfo has top and bottom the other way around.
    bounds[BBOTTOM]  = FRACUNIT * sector->bounds[BTOP];
    bounds[BTOP]     = FRACUNIT * sector->bounds[BBOTTOM];
    bounds[BFLOOR]   = FRACUNIT * sector->SP_floorvisheight;
    bounds[BCEILING] = FRACUNIT * sector->SP_ceilvisheight;

    return Rend_CheckDecorationBounds(bounds, decorPlaneMaxDist);
}

/**
 * Generate decorations for upper, middle and bottom parts of the line,
 * on both sides.
 */
static void Rend_DecorateLine(int index)
{
    line_t     *line = LINE_PTR(index);
    side_t     *side;
    sector_t   *highSector, *lowSector;
    float       frontCeil, frontFloor, backCeil, backFloor;

    // Ignore benign linedefs.
    if(line->flags & LINEF_BENIGN)
        return;

    // Only the lines within the decoration visibility bounding box
    // are processed.
    if(!Rend_LineDecorationBounds(line))
        return;

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
            side = R_GetSectorSide(line, highSector);

            if(side->SW_toptexture > 0)
            {
                texinfo_t *texinfo;

                if(side->SW_topisflat)
                    GL_GetFlatInfo(side->SW_toptexture, &texinfo);
                else
                    GL_GetTextureInfo(side->SW_toptexture, &texinfo);

                Rend_DecorateLineSection(line, side, &side->SW_topsurface,
                                         highSector->SP_ceilvisheight,
                                         lowSector->SP_ceilvisheight,
                                         line->mapflags & ML_DONTPEGTOP ? 0 : -texinfo->height +
                                         (highSector->SP_ceilvisheight -
                                          lowSector->SP_ceilvisheight));
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
            side = R_GetSectorSide(line, lowSector);

            if(side->SW_bottomtexture > 0)
            {
                Rend_DecorateLineSection(line, side, &side->SW_bottomsurface,
                                         highSector->SP_floorvisheight,
                                         lowSector->SP_floorvisheight,
                                         line->mapflags & ML_DONTPEGBOTTOM ?
                                         highSector->SP_floorvisheight -
                                         lowSector->SP_ceilvisheight : 0);
            }
        }

        // 2-sided middle texture?
        // FIXME: Since halos aren't usually clipped by 2-sided middle
        // textures, this looks a bit silly.
        /*if(line->L_frontside && side = line->L_frontside->SW_middletexture)
        {
            rendpoly_t *quad = R_AllocRendPoly(RP_QUAD, true, 4);

            quad->top = MIN_OF(frontCeil, backCeil);
            quad->bottom = MAX_OF(frontFloor, backFloor);
            quad->texoffy = FIX2FLT(side->SW_middleoffy);
            if(Rend_MidTexturePos(&quad->top, &quad->bottom, &quad->texoffy, 0,
                                  (line->flags & ML_DONTPEGBOTTOM) != 0))
            {
                Rend_DecorateLineSection(line, side, &side->SW_middlesurface,
                                         quad->top, quad->bottom, quad->texoffy);
            }
            R_FreeRendPoly(quad);
        }*/
    }
    else
    {
        // This is a single-sided line. We only need to worry about the
        // middle texture.
        side = line->L_side(line->L_frontside? FRONT:BACK);

        if(side->SW_middletexture > 0)
        {
            texinfo_t      *texinfo;

            if(side->SW_middleisflat)
                GL_GetFlatInfo(side->SW_middletexture, &texinfo);
            else
                GL_GetTextureInfo(side->SW_middletexture, &texinfo);

            Rend_DecorateLineSection(line, side, &side->SW_middlesurface, frontCeil,
                                     frontFloor,
                                     line->mapflags & ML_DONTPEGBOTTOM ? -texinfo->height +
                                     (frontCeil - frontFloor) : 0);
        }
    }
}

/**
 * Generate decorations for a plane.
 */
static void Rend_DecoratePlane(uint sectorIndex, float z, float elevateDir,
                               float offX, float offY, ded_decor_t *def)
{
    sector_t   *sector = SECTOR_PTR(sectorIndex);
    ded_decorlight_t *lightDef;
    float       pos[3], tileSize = 64, brightMul;
    int         skip[2];
    unsigned int i;

    surfaceNormal[VX] = 0;
    surfaceNormal[VY] = elevateDir;
    surfaceNormal[VZ] = 0;

    // Generate a number of lights.
    for(i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
    {
        lightDef = def->lights + i;

        // No more?
        if(!R_IsValidLightDecoration(lightDef))
            break;

        // Does it pass the sectorlight limitation?
        if((brightMul = Rend_CheckSectorLight(sector, lightDef)) <= 0)
            continue;

        // Skip must be at least one.
        Rend_DecorationPatternSkip(lightDef, skip);

        pos[VY] =
            (int) (sector->bounds[BTOP] / tileSize) * tileSize - offY -
            lightDef->pos[VY] - lightDef->pattern_offset[VY] * tileSize;
        while(pos[VY] > sector->bounds[BTOP])
            pos[VY] -= tileSize * skip[VY];

        for(; pos[VY] < sector->bounds[BBOTTOM]; pos[VY] += tileSize * skip[VY])
        {
            if(pos[VY] < sector->bounds[BTOP])
                continue;

            pos[VX] =
                (int) (sector->bounds[BLEFT] / tileSize) * tileSize - offX +
                lightDef->pos[VX] - lightDef->pattern_offset[VX] * tileSize;
            while(pos[VX] > sector->bounds[BLEFT])
                pos[VX] -= tileSize * skip[VX];

            for(; pos[VX] < sector->bounds[BRIGHT];
                pos[VX] += tileSize * skip[VX])
            {
                if(pos[VX] < sector->bounds[BLEFT])
                    continue;

                // The point must be inside the correct sector.
                if(!R_IsPointInSector
                   (pos[VX] * FRACUNIT, pos[VY] * FRACUNIT, sector))
                    continue;

                pos[VZ] = z + lightDef->elevation * elevateDir;
                Rend_AddLightDecoration(pos, lightDef, brightMul, false,
                                        def->pregen_lightmap);
            }
        }
    }
}

/**
 * Generate decorations for the planes of the sector.
 */
static void Rend_DecorateSector(uint index)
{
    uint        i;
    plane_t    *pln;
    sector_t   *sector = SECTOR_PTR(index);
    ded_decor_t *def;

    // The sector must have height if it wants decorations.
    if(sector->SP_ceilheight <= sector->SP_floorheight)
        return;

    // Is this sector close enough for the decorations to be visible?
    if(!Rend_SectorDecorationBounds(sector))
        return;

    for(i = 0; i < sector->planecount; ++i)
    {
        pln = sector->SP_plane(i);
        def = Rend_GetGraphicResourceDecoration(pln->PS_texture,
                                                pln->PS_isflat);

        if(def != NULL) // The surface is decorated.
            Rend_DecoratePlane(index, pln->visheight,
                               pln->PS_normal[VZ],
                               pln->PS_offx, pln->PS_offy, def);
    }
}

/**
 * Decorations are generated for each frame.
 */
void Rend_InitDecorationsForFrame(void)
{
    uint    i;

    Rend_ClearDecorations();

    // This only needs to be done if decorations have been enabled.
    if(!useDecorations)
        return;

    // Process all lines. This could also be done during sectors,
    // but validcount would need to be used to prevent duplicate
    // processing.
    for(i = 0; i < numlines; ++i)
        Rend_DecorateLine(i);

    // Process all planes.
    for(i = 0; i < numsectors; ++i)
        Rend_DecorateSector(i);
}
