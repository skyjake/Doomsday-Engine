/** @file rend_decor.cpp Surface Decorations.
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_misc.h"

#include <de/vector1.h>

#include "def_main.h"
#include "resource/materialvariant.h"

using namespace de;

// Quite a bit of decorations, there!
#define MAX_DECOR_LIGHTS    (16384)

BEGIN_PROF_TIMERS()
  PROF_DECOR_UPDATE,
  PROF_DECOR_PROJECT,
  PROF_DECOR_ADD_LUMINOUS
END_PROF_TIMERS()

typedef struct decorsource_s {
    coord_t origin[3];
    coord_t maxDistance;
    Surface const *surface;
    BspLeaf *bspLeaf;
    unsigned int lumIdx; // Index+1 of linked lumobj, or 0.
    float fadeMul;
    ded_decorlight_t const *def;
    DGLuint flareTex;
    struct decorsource_s *next;
} decorsource_t;

static void updateSideSectionDecorations(LineDef *lineDef, byte side, SideDefSection section);
static void updatePlaneDecorations(Plane *pln);

byte useLightDecorations = true;
float decorMaxDist = 2048; // No decorations are visible beyond this.
float decorLightBrightFactor = 1;
float decorLightFadeAngle = .1f;

static uint numDecorations = 0;

static decorsource_t *sourceFirst = 0;
static decorsource_t *sourceCursor = 0;

void Rend_DecorRegister()
{
    C_VAR_BYTE("rend-light-decor", &useLightDecorations, 0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-angle", &decorLightFadeAngle, 0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-bright", &decorLightBrightFactor, 0, 0, 10);
}

/**
 * Clears the list of decoration dummies.
 */
static void clearDecorations()
{
    numDecorations = 0;
    sourceCursor = sourceFirst;
}

extern void getLightingParams(coord_t x, coord_t y, coord_t z, BspLeaf* bspLeaf,
                              coord_t distance, boolean fullBright,
                              float ambientColor[3], uint* lightListIdx);

static void projectDecoration(decorsource_t *src)
{
    float brightness, min, max;
    lumobj_t const *lum;
    vissprite_t *vis;
    coord_t distance;

    // Does it pass the sector light limitation?
    min = src->def->lightLevels[0];
    max = src->def->lightLevels[1];

    if(!((brightness = R_CheckSectorLight(src->bspLeaf->sector->lightLevel, min, max)) > 0))
        return;

    if(src->fadeMul <= 0)
        return;

    // Is the point in range?
    distance = Rend_PointDist3D(src->origin);
    if(distance > src->maxDistance)
        return;

    /// @todo dj: Why is LO_GetLuminous returning NULL given a supposedly valid index?
    if(!LO_GetLuminous(src->lumIdx))
        return;

    /**
     * Model decorations become model-type vissprites.
     * Light decorations become flare-type vissprites.
     */
    vis = R_NewVisSprite();
    vis->type = VSPR_FLARE;

    V3d_Copy(vis->origin, src->origin);
    vis->distance = distance;

    lum = LO_GetLuminous(src->lumIdx);

    vis->data.flare.isDecoration = true;
    vis->data.flare.lumIdx = src->lumIdx;

    // Color is taken from the associated lumobj.
    V3f_Copy(vis->data.flare.color, LUM_OMNI(lum)->color);

    if(src->def->haloRadius > 0)
        vis->data.flare.size = MAX_OF(1, src->def->haloRadius * 60 * (50 + haloSize) / 100.0f);
    else
        vis->data.flare.size = 0;

    if(src->flareTex != 0)
        vis->data.flare.tex = src->flareTex;
    else
    {   // Primary halo disabled.
        vis->data.flare.flags |= RFF_NO_PRIMARY;
        vis->data.flare.tex = 0;
    }

    // Halo brightness drops as the angle gets too big.
    vis->data.flare.mul = 1;
    if(src->def->elevation < 2 && decorLightFadeAngle > 0) // Close the surface?
    {
        float vector[3], dot;

        V3f_Set(vector, src->origin[VX] - vOrigin[VX],
                        src->origin[VY] - vOrigin[VZ],
                        src->origin[VZ] - vOrigin[VY]);
        V3f_Normalize(vector);
        dot = -(src->surface->normal[VX] * vector[VX] +
                src->surface->normal[VY] * vector[VY] +
                src->surface->normal[VZ] * vector[VZ]);

        if(dot < decorLightFadeAngle / 2)
            vis->data.flare.mul = 0;
        else if(dot < 3 * decorLightFadeAngle)
            vis->data.flare.mul = (dot - decorLightFadeAngle / 2) / (2.5f * decorLightFadeAngle);
    }
}

/**
 * Re-initialize the decoration source tracking (might be called during a map
 * load or othersuch situation).
 */
void Rend_DecorInit()
{
    clearDecorations();
}

/**
 * Project all the non-clipped decorations. They become regular vissprites.
 */
void Rend_ProjectDecorations()
{
    if(!useLightDecorations) return;

    if(sourceFirst == sourceCursor) return;

    decorsource_t *src = sourceFirst;
    do
    {
        projectDecoration(src);
    } while((src = src->next) != sourceCursor);
}

static void addLuminousDecoration(decorsource_t *src)
{
    ded_decorlight_t const *def = src->def;
    float brightness;
    uint i, lumIdx;
    float min, max;
    lumobj_t* l;

    // Does it pass the sector light limitation?
    min = def->lightLevels[0];
    max = def->lightLevels[1];

    if(!((brightness = R_CheckSectorLight(src->bspLeaf->sector->lightLevel, min, max)) > 0))
        return;

    // Apply the brightness factor (was calculated using sector lightlevel).
    src->fadeMul = brightness * decorLightBrightFactor;
    src->lumIdx = 0;

    if(src->fadeMul <= 0)
        return;

    /**
     * @todo From here on is pretty much the same as LO_AddLuminous,
     *       reconcile the two.
     */

    lumIdx = LO_NewLuminous(LT_OMNI, src->bspLeaf);
    l = LO_GetLuminous(lumIdx);

    V3d_Copy(l->origin, src->origin);
    l->maxDistance = src->maxDistance;
    l->decorSource = src;

    LUM_OMNI(l)->zOff = 0;
    LUM_OMNI(l)->tex = GL_PrepareLightmap(def->sides);
    LUM_OMNI(l)->ceilTex = GL_PrepareLightmap(def->up);
    LUM_OMNI(l)->floorTex = GL_PrepareLightmap(def->down);

    // These are the same rules as in DL_MobjRadius().
    LUM_OMNI(l)->radius = def->radius * 40 * loRadiusFactor;

    // Don't make a too small or too large light.
    if(LUM_OMNI(l)->radius > loMaxRadius)
        LUM_OMNI(l)->radius = loMaxRadius;

    for(i = 0; i < 3; ++i)
        LUM_OMNI(l)->color[i] = def->color[i] * src->fadeMul;

    src->lumIdx = lumIdx;
}

/**
 * Create lumobjs for all decorations who want them.
 */
void Rend_AddLuminousDecorations()
{
BEGIN_PROF( PROF_DECOR_ADD_LUMINOUS );

    if(useLightDecorations && sourceFirst != sourceCursor)
    {
        decorsource_t *src = sourceFirst;
        do
        {
            addLuminousDecoration(src);
        } while((src = src->next) != sourceCursor);
    }

END_PROF( PROF_DECOR_ADD_LUMINOUS );
}

/**
 * Create a new decoration source.
 */
static decorsource_t *addDecoration()
{
    decorsource_t *src;

    // If the cursor is NULL, new sources must be allocated.
    if(!sourceCursor)
    {
        // Allocate a new entry.
        src = (decorsource_t *) Z_Calloc(sizeof(decorsource_t), PU_APPSTATIC, NULL);

        if(!sourceFirst)
        {
            sourceFirst = src;
        }
        else
        {
            src->next = sourceFirst;
            sourceFirst = src;
        }
    }
    else
    {
        // There are old sources to use.
        src = sourceCursor;

        src->fadeMul = 0;
        src->lumIdx = 0;
        src->maxDistance = 0;
        V3d_Set(src->origin, 0, 0, 0);
        src->bspLeaf = 0;
        src->surface = 0;
        src->def = NULL;
        src->flareTex = 0;

        // Advance the cursor.
        sourceCursor = sourceCursor->next;
    }

    return src;
}

/**
 * A decorsource is created from the specified surface decoration.
 */
static void createDecorSource(Surface const *suf, surfacedecor_t const *dec, float maxDistance)
{
    // Out of sources?
    if(numDecorations >= MAX_DECOR_LIGHTS) return;

    ++numDecorations;

    // Fill in the data for a new surface decoration.
    decorsource_t *src = addDecoration();
    V3d_Copy(src->origin, dec->origin);
    src->maxDistance = maxDistance;
    src->bspLeaf = dec->bspLeaf;
    src->surface = suf;
    src->fadeMul = 1;
    src->def = dec->def;
    if(src->def)
    {
        ded_decorlight_t const *def = src->def;
        if(!def->flare || Str_CompareIgnoreCase(Uri_Path(def->flare), "-"))
        {
            src->flareTex = GL_PrepareFlareTexture(def->flare, def->flareTexture);
        }
    }
}

/**
 * @return  As this can also be used with iterators, will always return @c true.
 */
boolean R_ProjectSurfaceDecorations(Surface *suf, void *context)
{
    float maxDist = *((float *) context);
    uint i;

    if(suf->inFlags & SUIF_UPDATE_DECORATIONS)
    {
        R_ClearSurfaceDecorations(suf);

        switch(DMU_GetType(suf->owner))
        {
        case DMU_SIDEDEF: {
            SideDef *sideDef = (SideDef *)suf->owner;
            LineDef *line = sideDef->line;
            updateSideSectionDecorations(line, sideDef == line->L_frontsidedef? FRONT : BACK,
                                         &sideDef->SW_middlesurface == suf? SS_MIDDLE :
                                         &sideDef->SW_bottomsurface == suf? SS_BOTTOM : SS_TOP);
            break; }
        case DMU_PLANE:
            updatePlaneDecorations((Plane *)suf->owner);
            break;
        default:
            Con_Error("R_ProjectSurfaceDecorations: Internal Error, unknown type %s.", DMU_Str(DMU_GetType(suf->owner)));
            break;
        }
        suf->inFlags &= ~SUIF_UPDATE_DECORATIONS;
    }

    if(useLightDecorations)
    for(i = 0; i < suf->numDecorations; ++i)
    {
        surfacedecor_t const *d = &suf->decorations[i];
        createDecorSource(suf, d, maxDist);
    }

    return true;
}

/**
 * Determine proper skip values.
 */
static void getDecorationSkipPattern(int const patternSkip[2], int *skip)
{
    for(uint i = 0; i < 2; ++i)
    {
        // Skip must be at least one.
        skip[i] = patternSkip[i] + 1;
        if(skip[i] < 1) skip[i] = 1;
    }
}

static uint generateDecorLights(ded_decorlight_t const *def, Surface *suf,
    material_t *mat, pvec3d_t const v1, pvec3d_t const /*v2*/, coord_t width, coord_t height,
    pvec3d_t const delta, int axis, float offsetS, float offsetT, Sector* sec)
{
    vec3d_t originBase, origin;
    coord_t patternW, patternH;
    float s, t; // Horizontal and vertical offset.
    int skip[2];
    uint num;

    if(!mat || !R_IsValidLightDecoration(def)) return 0;

    // Skip must be at least one.
    getDecorationSkipPattern(def->patternSkip, skip);

    patternW = Material_Width(mat)  * skip[0];
    patternH = Material_Height(mat) * skip[1];

    if(0 == patternW && 0 == patternH) return 0;

    V3d_Set(originBase, def->elevation * suf->normal[VX],
                        def->elevation * suf->normal[VY],
                        def->elevation * suf->normal[VZ]);
    V3d_Sum(originBase, originBase, v1);

    // Let's see where the top left light is.
    s = M_CycleIntoRange(def->pos[0] -
                         Material_Width(mat) * def->patternOffset[0] +
                         offsetS, patternW);
    num = 0;
    for(; s < width; s += patternW)
    {
        t = M_CycleIntoRange(def->pos[1] -
                             Material_Height(mat) * def->patternOffset[1] +
                             offsetT, patternH);

        for(; t < height; t += patternH)
        {
            surfacedecor_t *d;
            float offS = s / width, offT = t / height;

            V3d_Set(origin, delta[VX] * offS,
                            delta[VY] * (axis == VZ? offT : offS),
                            delta[VZ] * (axis == VZ? offS : offT));
            V3d_Sum(origin, originBase, origin);

            if(sec)
            {
                // The point must be inside the correct sector.
                if(!P_IsPointXYInSector(origin[VX], origin[VY], sec))
                    continue;
            }

            d = R_CreateSurfaceDecoration(suf);
            if(d)
            {
                V3d_Copy(d->origin, origin);
                d->bspLeaf = P_BspLeafAtPoint(d->origin);
                d->def = def;
                num++;
            }
        }
    }

    return num;
}

/**
 * Generate decorations for the specified surface.
 */
static void updateSurfaceDecorations2(Surface *suf, float offsetS, float offsetT,
    vec3d_t v1, vec3d_t v2, Sector *sec, boolean visible)
{
    vec3d_t delta;

    V3d_Subtract(delta, v2, v1);

    if(visible &&
       (delta[VX] * delta[VY] != 0 ||
        delta[VX] * delta[VZ] != 0 ||
        delta[VY] * delta[VZ] != 0))
    {
        // Ensure we've prepared a variant of this material.
        App_Materials()->prepare(*suf->material, Rend_MapSurfaceDiffuseMaterialSpec(),
                                 true /*smooth*/, true /*do-create*/);

        ded_decor_t const *def = App_Materials()->decorationDef(*suf->material);
        if(def)
        {
            int const axis = V3f_MajorAxis(suf->normal);
            coord_t width, height;
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

            if(width < 0)  width  = -width;
            if(height < 0) height = -height;

            // Generate a number of lights.
            for(uint i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
            {
                generateDecorLights(&def->lights[i], suf, suf->material, v1, v2, width, height,
                                    delta, axis, offsetS, offsetT, sec);
            }
        }
    }
}

/**
 * Generate decorations for a plane.
 */
static void updatePlaneDecorations(Plane *pln)
{
    Sector *sec = pln->sector;
    Surface *suf = &pln->surface;
    vec3d_t v1, v2;
    float offsetS, offsetT;

    if(pln->type == PLN_FLOOR)
    {
        V3d_Set(v1, sec->aaBox.minX, sec->aaBox.maxY, pln->visHeight);
        V3d_Set(v2, sec->aaBox.maxX, sec->aaBox.minY, pln->visHeight);
    }
    else
    {
        V3d_Set(v1, sec->aaBox.minX, sec->aaBox.minY, pln->visHeight);
        V3d_Set(v2, sec->aaBox.maxX, sec->aaBox.maxY, pln->visHeight);
    }

    offsetS = -fmod(sec->aaBox.minX, 64) - suf->visOffset[0];
    offsetT = -fmod(sec->aaBox.minY, 64) - suf->visOffset[1];

    updateSurfaceDecorations2(suf, offsetS, offsetT, v1, v2, sec, suf->material? true : false);
}

static void updateSideSectionDecorations(LineDef *line, byte side, SideDefSection section)
{
    float matOffset[2];
    Surface *surface;
    vec3d_t v1, v2;
    boolean visible = false;

    if(!line || !line->L_sidedef(side)) return;

    surface = &line->L_sidedef(side)->SW_surface(section);
    if(surface->material)
    {
        Sector *frontSec  = line->L_sector(side);
        Sector *backSec   = line->L_sector(side^1);
        SideDef *frontDef = line->L_sidedef(side);
        SideDef *backDef  = line->L_sidedef(side^1);
        coord_t low, hi;
        visible = R_FindBottomTop2(section, line->flags, frontSec, backSec, frontDef, backDef,
                                   &low, &hi, matOffset);
        if(visible)
        {
            V3d_Set(v1, line->L_vorigin(side  )[VX], line->L_vorigin(side  )[VY], hi);
            V3d_Set(v2, line->L_vorigin(side^1)[VX], line->L_vorigin(side^1)[VY], low);
        }
    }

    updateSurfaceDecorations2(surface, -matOffset[0], -matOffset[1], v1, v2, NULL, visible);
}

/**
 * Decorations are generated for each frame.
 */
void Rend_InitDecorationsForFrame()
{
    surfacelist_t *slist;
#ifdef DD_PROFILE
    static int i;

    if(++i > 40)
    {
        i = 0;
        PRINT_PROF( PROF_DECOR_UPDATE );
        PRINT_PROF( PROF_DECOR_PROJECT );
        PRINT_PROF( PROF_DECOR_ADD_LUMINOUS );
    }
#endif

    // This only needs to be done if decorations have been enabled.
    if(!useLightDecorations) return;

BEGIN_PROF( PROF_DECOR_PROJECT );

    clearDecorations();

    slist = GameMap_DecoratedSurfaces(theMap);
    if(slist)
    {
        R_SurfaceListIterate(slist, R_ProjectSurfaceDecorations, &decorMaxDist);
    }

END_PROF( PROF_DECOR_PROJECT );
}
