/** @file rend_decor.cpp Surface Decorations.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

//#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
//#include "de_graphics.h"
#include "de_render.h"

#include "def_main.h"
#include "m_profiler.h"
#include "MaterialVariantSpec"
#include <de/math.h>
#include <de/vector1.h>

#include "render/rend_decor.h"

using namespace de;

/// Quite a bit of decorations, there!
#define MAX_DECOR_LIGHTS        (16384)

/// No decorations are visible beyond this.
#define MAX_DECOR_DISTANCE      (2048)

BEGIN_PROF_TIMERS()
  PROF_DECOR_UPDATE,
  PROF_DECOR_BEGIN_FRAME,
  PROF_DECOR_ADD_LUMINOUS
END_PROF_TIMERS()

/// @todo This abstraction is now unnecessary (merge with Surface::DecorSource) -ds
typedef struct decorsource_s {
    coord_t origin[3];
    coord_t maxDistance;
    Surface const *surface;
    BspLeaf *bspLeaf;
    uint lumIdx; // Index+1 of linked lumobj, or 0.
    float fadeMul;
    MaterialSnapshot::Decoration const *decor;
    struct decorsource_s *next;
} decorsource_t;

byte useLightDecorations = true;
float decorLightBrightFactor = 1;
float decorLightFadeAngle = .1f;

static uint sourceCount = 0;
static decorsource_t *sourceFirst = 0;
static decorsource_t *sourceCursor = 0;

static void plotSourcesForLineDef(LineDef &lineDef, byte side, SideDefSection section);
static void plotSourcesForPlane(Plane &pln);

void Rend_DecorRegister()
{
    C_VAR_BYTE("rend-light-decor",         &useLightDecorations,    0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-angle",  &decorLightFadeAngle,    0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-bright", &decorLightBrightFactor, 0, 0, 10);
}

static void recycleSources()
{
    sourceCount = 0;
    sourceCursor = sourceFirst;
}

static void projectSource(decorsource_t const &src)
{
    MaterialSnapshot::Decoration const *decor = src.decor;

    // Don't project decorations which emit no color.
    if(decor->color.x == 0 && decor->color.y == 0 && decor->color.z == 0) return;

    // Does it pass the sector light limitation?
    float min = decor->lightLevels[0];
    float max = decor->lightLevels[1];

    float brightness = R_CheckSectorLight(src.bspLeaf->sector().lightLevel(), min, max);
    if(!(brightness > 0)) return;

    if(src.fadeMul <= 0) return;

    // Is the point in range?
    coord_t distance = Rend_PointDist3D(src.origin);
    if(distance > src.maxDistance) return;

    /// @todo dj: Why is LO_GetLuminous returning NULL given a supposedly valid index?
    if(!LO_GetLuminous(src.lumIdx)) return;

    /*
     * Light decorations become flare-type vissprites.
     */
    vissprite_t *vis = R_NewVisSprite();
    vis->type = VSPR_FLARE;

    V3d_Copy(vis->origin, src.origin);
    vis->distance = distance;

    lumobj_t const *lum = LO_GetLuminous(src.lumIdx);

    vis->data.flare.isDecoration = true;
    vis->data.flare.lumIdx = src.lumIdx;

    // Color is taken from the associated lumobj.
    V3f_Copy(vis->data.flare.color, LUM_OMNI(lum)->color);

    if(decor->haloRadius > 0)
        vis->data.flare.size = MAX_OF(1, decor->haloRadius * 60 * (50 + haloSize) / 100.0f);
    else
        vis->data.flare.size = 0;

    if(decor->flareTex != 0)
    {
        vis->data.flare.tex = decor->flareTex;
    }
    else
    {   // Primary halo disabled.
        vis->data.flare.flags |= RFF_NO_PRIMARY;
        vis->data.flare.tex = 0;
    }

    // Halo brightness drops as the angle gets too big.
    vis->data.flare.mul = 1;
    if(decor->elevation < 2 && decorLightFadeAngle > 0) // Close the surface?
    {
        float vector[3];
        V3f_Set(vector, src.origin[VX] - vOrigin[VX],
                        src.origin[VY] - vOrigin[VZ],
                        src.origin[VZ] - vOrigin[VY]);
        V3f_Normalize(vector);

        float dot = -(src.surface->normal[VX] * vector[VX] +
                      src.surface->normal[VY] * vector[VY] +
                      src.surface->normal[VZ] * vector[VZ]);

        if(dot < decorLightFadeAngle / 2)
            vis->data.flare.mul = 0;
        else if(dot < 3 * decorLightFadeAngle)
            vis->data.flare.mul = (dot - decorLightFadeAngle / 2) / (2.5f * decorLightFadeAngle);
    }
}

void Rend_DecorInit()
{
    recycleSources();
}

void Rend_DecorProject()
{
    if(!useLightDecorations) return;

    for(decorsource_t *src = sourceFirst; src != sourceCursor; src = src->next)
    {
        projectSource(*src);
    }
}

static void addLuminousDecoration(decorsource_t &src)
{
    MaterialSnapshot::Decoration const *decor = src.decor;

    // Don't add decorations which emit no color.
    if(decor->color.x == 0 && decor->color.y == 0 && decor->color.z == 0) return;

    // Does it pass the sector light limitation?
    float min = decor->lightLevels[0];
    float max = decor->lightLevels[1];

    float brightness = R_CheckSectorLight(src.bspLeaf->sector().lightLevel(), min, max);
    if(!(brightness > 0)) return;

    // Apply the brightness factor (was calculated using sector lightlevel).
    src.fadeMul = brightness * decorLightBrightFactor;
    src.lumIdx = 0;

    if(src.fadeMul <= 0) return;

    /**
     * @todo From here on is pretty much the same as LO_AddLuminous,
     *       reconcile the two.
     */

    uint lumIdx = LO_NewLuminous(LT_OMNI, src.bspLeaf);
    lumobj_t *l = LO_GetLuminous(lumIdx);

    V3d_Copy(l->origin, src.origin);
    l->maxDistance = src.maxDistance;
    l->decorSource = (void*)&src;

    LUM_OMNI(l)->zOff = 0;
    LUM_OMNI(l)->tex      = decor->tex;
    LUM_OMNI(l)->ceilTex  = decor->ceilTex;
    LUM_OMNI(l)->floorTex = decor->floorTex;

    // These are the same rules as in DL_MobjRadius().
    LUM_OMNI(l)->radius = decor->radius * 40 * loRadiusFactor;

    // Don't make a too small or too large light.
    if(LUM_OMNI(l)->radius > loMaxRadius)
        LUM_OMNI(l)->radius = loMaxRadius;

    for(uint i = 0; i < 3; ++i)
        LUM_OMNI(l)->color[i] = decor->color[i] * src.fadeMul;

    src.lumIdx = lumIdx;
}

void Rend_DecorAddLuminous()
{
BEGIN_PROF( PROF_DECOR_ADD_LUMINOUS );

    if(useLightDecorations && sourceFirst != sourceCursor)
    {
        decorsource_t *src = sourceFirst;
        do
        {
            addLuminousDecoration(*src);
        } while((src = src->next) != sourceCursor);
    }

END_PROF( PROF_DECOR_ADD_LUMINOUS );
}

/**
 * Create a new decoration source.
 */
static decorsource_t *allocDecorSource()
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
        src->lumIdx  = 0;
        src->maxDistance = 0;
        V3d_Set(src->origin, 0, 0, 0);
        src->bspLeaf = 0;
        src->surface = 0;
        src->decor   = 0;

        // Advance the cursor.
        sourceCursor = sourceCursor->next;
    }

    return src;
}

/**
 * A source is created from the specified surface decoration.
 */
static void newSource(Surface const &suf, Surface::DecorSource const &dec)
{
    // Out of sources?
    if(sourceCount >= MAX_DECOR_LIGHTS) return;

    ++sourceCount;
    decorsource_t *src = allocDecorSource();

    // Fill in the data for a new surface decoration.
    V3d_Copy(src->origin, dec.origin);
    src->maxDistance = MAX_DECOR_DISTANCE;
    src->bspLeaf = dec.bspLeaf;
    src->surface = &suf;
    src->fadeMul = 1;
    src->decor = dec.decor;
}

static void plotSourcesForSurface(Surface &suf)
{
    if(suf.inFlags & SUIF_UPDATE_DECORATIONS)
    {
        suf.clearDecorations();

        switch(suf.owner->type())
        {
        case DMU_SIDEDEF: {
            SideDef *sideDef = suf.owner->castTo<SideDef>();
            LineDef *line = sideDef->line;
            plotSourcesForLineDef(*line, sideDef == line->frontSideDefPtr()? FRONT : BACK,
                                  &sideDef->SW_middlesurface == &suf? SS_MIDDLE :
                                  &sideDef->SW_bottomsurface == &suf? SS_BOTTOM : SS_TOP);
            break; }

        case DMU_PLANE: {
            Plane *plane = suf.owner->castTo<Plane>();
            plotSourcesForPlane(*plane);
            break; }

        default:
            DENG2_ASSERT(0); // Invalid type.
        }
        suf.inFlags &= ~SUIF_UPDATE_DECORATIONS;
    }

    if(useLightDecorations)
    {
        Surface::DecorSource const *decorations = (Surface::DecorSource const *)suf.decorations;
        for(uint i = 0; i < suf.numDecorations; ++i)
        {
            newSource(suf, decorations[i]);
        }
    }
}

static inline void getDecorationSkipPattern(Vector2i const &patternSkip, Vector2i &skip)
{
    // Skip values must be at least one.
    skip.x = de::max(patternSkip.x + 1, 1);
    skip.y = de::max(patternSkip.y + 1, 1);
}

static uint generateDecorLights(MaterialSnapshot::Decoration const &decor,
    Vector2i const &patternOffset, Vector2i const &patternSkip, Surface &suf,
    Material &material, Vector3d const &v1, Vector3d const &/*v2*/,
    Vector2d sufDimensions, Vector3d const &delta, int axis,
    Vector2f const &offset, Sector *containingSector)
{
    uint decorCount = 0;

    // Skip must be at least one.
    Vector2i skip;
    getDecorationSkipPattern(patternSkip, skip);

    float patternW = material.width()  * skip.x;
    float patternH = material.height() * skip.y;

    if(de::fequal(patternW, 0) && de::fequal(patternH, 0)) return 0;

    Vector3d topLeft = v1 + Vector3d(decor.elevation * suf.normal[VX],
                                     decor.elevation * suf.normal[VY],
                                     decor.elevation * suf.normal[VZ]);

    // Determine the leftmost point.
    float s = de::wrap(decor.pos[0] - material.width() * patternOffset.x + offset.x,
                       0.f, patternW);

    // Plot decorations.
    for(; s < sufDimensions.x; s += patternW)
    {
        // Determine the topmost point for this row.
        float t = de::wrap(decor.pos[1] - material.height() * patternOffset.y + offset.y,
                           0.f, patternH);

        for(; t < sufDimensions.y; t += patternH)
        {
            float const offS = s / sufDimensions.x;
            float const offT = t / sufDimensions.y;

            Vector3d origin = topLeft + Vector3d(delta.x * offS,
                                                 delta.y * (axis == VZ? offT : offS),
                                                 delta.z * (axis == VZ? offS : offT));
            if(containingSector)
            {
                // The point must be inside the correct sector.
                if(!P_IsPointXYInSector(origin.x, origin.y, containingSector))
                    continue;
            }

            if(Surface::DecorSource *source = suf.newDecoration())
            {
                V3d_Set(source->origin, origin.x, origin.y, origin.z);
                source->bspLeaf = P_BspLeafAtPoint(source->origin);
                source->decor   = &decor;
                decorCount++;
            }
        }
    }

    return decorCount;
}

/**
 * Generate decorations for the specified surface.
 */
static void updateSurfaceDecorations(Surface &suf, Vector2f const &offset,
    Vector3d const &v1, Vector3d const &v2, Sector *sec = 0)
{
    Vector3d delta = v2 - v1;
    if(de::fequal(delta.length(), 0)) return;

    int const axis = V3f_MajorAxis(suf.normal);
    Vector2d sufDimensions;
    if(axis == VX || axis == VY)
    {
        sufDimensions.x = std::sqrt(delta[VX] * delta[VX] + delta[VY] * delta[VY]);
        sufDimensions.y = delta[VZ];
    }
    else
    {
        sufDimensions.x = std::sqrt(delta[VX] * delta[VX]);
        sufDimensions.y = delta[VY];
    }

    if(sufDimensions.x < 0) sufDimensions.x = -sufDimensions.x;
    if(sufDimensions.y < 0) sufDimensions.y = -sufDimensions.y;

    // Generate a number of lights.
    MaterialSnapshot const &ms = suf.material->prepare(Rend_MapSurfaceMaterialSpec());

    Material::Decorations const &decorations = suf.material->decorations();
    for(int i = 0; i < decorations.count(); ++i)
    {
        MaterialSnapshot::Decoration const &decor = ms.decoration(i);
        MaterialDecoration const *def = decorations[i];

        generateDecorLights(decor, def->patternOffset(), def->patternSkip(),
                            suf, *suf.material, v1, v2, sufDimensions,
                            delta, axis, offset, sec);
    }
}

static void plotSourcesForPlane(Plane &pln)
{
    Surface &suf = pln.surface();
    if(!suf.material) return;

    Sector &sector = pln.sector();
    AABoxd const &sectorAABox = sector.aaBox();

    Vector3d v1(sectorAABox.minX, pln.type() == Plane::Floor? sectorAABox.maxY : sectorAABox.minY, pln.visHeight());
    Vector3d v2(sectorAABox.maxX, pln.type() == Plane::Floor? sectorAABox.minY : sectorAABox.maxY, pln.visHeight());

    Vector2f offset(-fmod(sectorAABox.minX, 64) - suf.visOffset[0],
                    -fmod(sectorAABox.minY, 64) - suf.visOffset[1]);

    updateSurfaceDecorations(suf, offset, v1, v2, &sector);
}

static void plotSourcesForLineDef(LineDef &line, byte side, SideDefSection section)
{
    if(!line.hasSideDef(side)) return;

    Sector *frontSec  = line.sectorPtr(side);
    Sector *backSec   = line.sectorPtr(side ^ 1);
    SideDef *frontDef = line.sideDefPtr(side);
    SideDef *backDef  = line.sideDefPtr(side ^ 1);
    Surface &suf      = line.sideDef(side).SW_surface(section);

    if(!suf.material) return;

    // Is the line section potentially visible?
    coord_t low, hi;
    float matOffset[2] = { 0, 0 };
    if(!R_FindBottomTop2(section, line.flags(), frontSec, backSec, frontDef, backDef,
                         &low, &hi, matOffset)) return;

    Vector3d v1(line.vertexOrigin(side  )[VX], line.vertexOrigin(side  )[VY], hi);
    Vector3d v2(line.vertexOrigin(side^1)[VX], line.vertexOrigin(side^1)[VY], low);

    updateSurfaceDecorations(suf, Vector2f(-matOffset[0], -matOffset[1]), v1, v2);
}

void Rend_DecorBeginFrame()
{
#ifdef DD_PROFILE
    static int i;
    if(++i > 40)
    {
        i = 0;
        PRINT_PROF( PROF_DECOR_UPDATE );
        PRINT_PROF( PROF_DECOR_BEGIN_FRAME );
        PRINT_PROF( PROF_DECOR_ADD_LUMINOUS );
    }
#endif

    // This only needs to be done if decorations have been enabled.
    if(!useLightDecorations) return;

BEGIN_PROF( PROF_DECOR_BEGIN_FRAME );

    recycleSources();

    foreach(Surface *surface, theMap->decoratedSurfaces())
    {
        plotSourcesForSurface(*surface);
    }

END_PROF( PROF_DECOR_BEGIN_FRAME );
}
