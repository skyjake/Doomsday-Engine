/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
#define MAX_DECOR_LIGHTS    (16384)
#define MAX_DECOR_MODELS    (8192)

BEGIN_PROF_TIMERS()
  PROF_DECOR_UPDATE,
  PROF_DECOR_PROJECT,
  PROF_DECOR_ADD_LUMINOUS
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

typedef struct decorsource_s {
    float           pos[3];
    float           maxDistance;
    const surface_t* surface;
    subsector_t*    subsector;
    decortype_t     type;
    unsigned int    lumIdx; // index+1 of linked lumobj, or 0.
    float           fadeMul;
    union decorsource_data_s {
        struct decorsource_data_light_s {
            const ded_decorlight_t* def;
        } light;
        struct decorsource_data_model_s {
            const ded_decormodel_t* def;
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

static uint numDecorLights = 0, numDecorModels = 0;
static decorsource_t* sourceFirst = NULL, *sourceLast = NULL;
static decorsource_t* sourceCursor = NULL;

// CODE --------------------------------------------------------------------

void Rend_DecorRegister(void)
{
    C_VAR_BYTE("rend-light-decor", &useDecorations, 0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-angle", &decorFadeAngle, 0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-bright", &decorFactor, 0, 0, 10);
}

/**
 * Clears the list of decoration dummies.
 */
static void clearDecorations(void)
{
    numDecorLights = numDecorModels = 0;
    sourceCursor = sourceFirst;
}

extern void setupModelParamsForVisSprite(rendmodelparams_t *params,
                                         float x, float y, float z, float distance,
                                         float visOffX, float visOffY, float visOffZ, float gzt, float yaw, float yawAngleOffset, float pitch, float pitchAngleOffset,
                                         struct modeldef_s* mf, struct modeldef_s* nextMF, float inter,
                                         float ambientColorR, float ambientColorG, float ambientColorB, float alpha,
                                         vlight_t* lightList, uint numLights,
                                         int id, int selector, subsector_t* ssec, int mobjDDFlags, int tmap,
                                         boolean viewAlign, boolean fullBright,
                                         boolean alwaysInterpolate);
extern void getLightingParams(float x, float y, float z, subsector_t* ssec,
                              float distance, boolean fullBright,
                              uint maxLights, float ambientColor[3],
                              vlight_t** lights, uint* numLights);

static void projectDecoration(decorsource_t* src)
{
    float               v1[2], min, max;
    vissprite_t*        vis;
    float               distance, brightness;

    // Does it pass the sector light limitation?
    if(src->type == DT_LIGHT)
    {
        min = src->data.light.def->lightLevels[0];
        max = src->data.light.def->lightLevels[1];
    }
    else // Its a decor model.
    {
        min = src->data.model.def->lightLevels[0];
        max = src->data.model.def->lightLevels[0];
    }

    if(!((brightness = R_CheckSectorLight(src->subsector->sector->lightLevel,
                                          min, max)) > 0))
        return;

    if(src->fadeMul <= 0)
        return;

    // Is the point in range?
    distance = Rend_PointDist3D(src->pos);
    if(distance > src->maxDistance)
        return;

    // Calculate edges of the shape.
    v1[VX] = src->pos[VX];
    v1[VY] = src->pos[VY];

    /**
     * Model decorations become model-type vissprites.
     * Light decorations become flare-type vissprites.
     */
    vis = R_NewVisSprite();
    vis->type = ((src->type == DT_MODEL)? VSPR_MODEL : VSPR_FLARE);

    vis->center[VX] = src->pos[VX];
    vis->center[VY] = src->pos[VY];
    vis->center[VZ] = src->pos[VZ];
    vis->distance = distance;

    switch(src->type)
    {
    case DT_MODEL:
        {
        float           ambientColor[3];
        vlight_t*       lightList = NULL;
        uint            numLights = 0;

        getLightingParams(src->pos[VX], src->pos[VY], src->pos[VZ],
                          src->subsector, distance, levelFullBright,
                          modelLight,
                          ambientColor, &lightList, &numLights);

        setupModelParamsForVisSprite(&vis->data.model, src->pos[VX], src->pos[VY], src->pos[VZ],
                                     distance, 0, 0, 0, src->pos[VZ], src->data.model.yaw, 0,
                                     src->data.model.pitch, 0,
                                     src->data.model.mf, NULL, 0,
                                     ambientColor[CR], ambientColor[CG], ambientColor[CB],
                                     src->fadeMul, lightList, numLights, 0, 0, src->subsector,
                                     0, 0, false, levelFullBright, true);
        break;
        }
    case DT_LIGHT:
        {
        const ded_decorlight_t* def = src->data.light.def;
        const lumobj_t* lum = LO_GetLuminous(src->lumIdx);

        vis->data.flare.isDecoration = true;
        vis->data.flare.lumIdx = src->lumIdx;

        // Color is taken from the associated lumobj.
        V3_Copy(vis->data.flare.color, LUM_OMNI(lum)->color);

        if(def->haloRadius > 0)
        {
            vis->data.flare.size = MAX_OF(1,
                def->haloRadius * 60 * (50 + haloSize) / 100.0f);
        }
        else
        {
            vis->data.flare.size = 0;
        }

        if(!(def->flare.id && def->flare.id[0] == '-'))
        {
            vis->data.flare.tex = GL_GetFlareTexture(def->flare.id, def->flareTexture);
        }
        else
        {   // Primary halo disabled.
            vis->data.flare.flags |= RFF_NO_PRIMARY;
            vis->data.flare.tex = 0;
        }

        // Halo brightness drops as the angle gets too big.
        vis->data.flare.mul = 1;
        if(src->data.light.def->elevation < 2 && decorFadeAngle > 0) // Close the surface?
        {
            float vector[3], dot;

            vector[VX] = src->pos[VX] - vx;
            vector[VY] = src->pos[VY] - vz;
            vector[VZ] = src->pos[VZ] - vy;
            M_Normalize(vector);
            dot = -(src->surface->normal[VX] * vector[VX] +
                    src->surface->normal[VY] * vector[VY] +
                    src->surface->normal[VZ] * vector[VZ]);

            if(dot < decorFadeAngle / 2)
                vis->data.flare.mul = 0;
            else if(dot < 3 * decorFadeAngle)
                vis->data.flare.mul = (dot - decorFadeAngle / 2) / (2.5f * decorFadeAngle);
        }
        break;
        }
    }
}

/**
 * Project all the non-clipped decorations. They become regular vissprites.
 */
void Rend_ProjectDecorations(void)
{
    if(sourceFirst != sourceCursor)
    {
        decorsource_t*      src = sourceFirst;

        do
        {
            projectDecoration(src);
        } while((src = src->next) != sourceCursor);
    }
}

static void addLuminousDecoration(decorsource_t* src)
{
    uint                i;
    float               min, max;
    uint                lumIdx;
    lumobj_t*           l;
    float               brightness;
    const ded_decorlight_t* def = src->data.light.def;

    src->lumIdx = 0;
    src->fadeMul = 1;

    if(src->type != DT_LIGHT)
        return;

    // Does it pass the sector light limitation?
    min = def->lightLevels[0];
    max = def->lightLevels[1];

    if(!((brightness = R_CheckSectorLight(
            src->subsector->sector->lightLevel, min, max)) > 0))
        return;

    // Apply the brightness factor (was calculated using sector lightlevel).
    src->fadeMul *= brightness * decorFactor;

    if(src->fadeMul <= 0)
        return;

    /**
     * \todo From here on is pretty much the same as LO_AddLuminous,
     * reconcile the two.
     */

    lumIdx = LO_NewLuminous(LT_OMNI, src->subsector);
    l = LO_GetLuminous(lumIdx);

    l->pos[VX] = src->pos[VX];
    l->pos[VY] = src->pos[VY];
    l->pos[VZ] = src->pos[VZ];
    l->maxDistance = src->maxDistance;
    l->decorSource = src;

    LUM_OMNI(l)->zOff = 0;
    LUM_OMNI(l)->tex = GL_GetLightMapTexture(def->sides.id);
    LUM_OMNI(l)->ceilTex = GL_GetLightMapTexture(def->up.id);
    LUM_OMNI(l)->floorTex = GL_GetLightMapTexture(def->down.id);

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
void Rend_AddLuminousDecorations(void)
{
BEGIN_PROF( PROF_DECOR_ADD_LUMINOUS );

    if(sourceFirst != sourceCursor)
    {
        decorsource_t*      src = sourceFirst;

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
static decorsource_t* addDecoration(void)
{
    decorsource_t*      src;

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
 * A decorsource is created from the specified surface decoration.
 */
static void createDecorSource(const surface_t* suf,
                              const surfacedecor_t* dec,
                              const float maxDistance)
{
    decorsource_t*      src;

    if(dec->type == DT_LIGHT)
    {
        if(numDecorLights > MAX_DECOR_LIGHTS)
            return; // Out of sources!

        numDecorLights++;
    }
    else // It's a model decoration.
    {
        if(numDecorModels > MAX_DECOR_MODELS)
            return; // Out of sources!

        numDecorModels++;
    }

    // Fill in the data for a new surface decoration.
    src = addDecoration();
    src->pos[VX] = dec->pos[VX];
    src->pos[VY] = dec->pos[VY];
    src->pos[VZ] = dec->pos[VZ];
    src->maxDistance = maxDistance;
    src->subsector = dec->subsector;
    src->surface = suf;
    src->type = dec->type;
    switch(src->type)
    {
    case DT_LIGHT:
        src->data.light.def = DEC_LIGHT(dec)->def;
        break;

    case DT_MODEL:
        src->data.model.def = DEC_MODEL(dec)->def;
        src->data.model.mf = DEC_MODEL(dec)->mf;
        src->data.model.pitch = DEC_MODEL(dec)->pitch;
        src->data.model.yaw = DEC_MODEL(dec)->yaw;
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

        createDecorSource(suf, d, maxDist);
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
    material_t*         mat = suf->material;

    if(!R_IsValidLightDecoration(def))
        return 0;

    // Skip must be at least one.
    getDecorationSkipPattern(def->patternSkip, skip);

    patternW = mat->width * skip[0];
    patternH = mat->height * skip[1];

    V3_Set(posBase, def->elevation * suf->normal[VX],
                    def->elevation * suf->normal[VY],
                    def->elevation * suf->normal[VZ]);
    V3_Sum(posBase, posBase, v1);

    // Let's see where the top left light is.
    s = M_CycleIntoRange(def->pos[0] - suf->visOffset[0] -
                         mat->width * def->patternOffset[0] +
                         offsetS, patternW);
    num = 0;
    for(; s < width; s += patternW)
    {
        t = M_CycleIntoRange(def->pos[1] - suf->visOffset[1] -
                             mat->height * def->patternOffset[1] +
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
    material_t*         mat = suf->material;

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

    patternW = mat->width * skip[0];
    patternH = mat->height * skip[1];

    V3_Set(posBase, def->elevation * suf->normal[VX],
                    def->elevation * suf->normal[VY],
                    def->elevation * suf->normal[VZ]);
    V3_Sum(posBase, posBase, v1);

    // Let's see where the top left light is.
    s = M_CycleIntoRange(def->pos[0] - suf->visOffset[0] -
                         mat->width * def->patternOffset[0] +
                         offsetS, patternW);
    num = 0;
    for(; s < width; s += patternW)
    {
        t = M_CycleIntoRange(def->pos[1] - suf->visOffset[1] -
                             mat->height * def->patternOffset[1] +
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

    if(visible &&
       (delta[VX] * delta[VY] != 0 ||
        delta[VX] * delta[VZ] != 0 ||
        delta[VY] * delta[VZ] != 0))
    {
        uint                i;
        float               matW, matH, width, height;
        int                 axis = V3_MajorAxis(suf->normal);
        const ded_decor_t*  def = Material_GetDecoration(suf->material);

        if(def)
        {
            matW = suf->material->width;
            matH = suf->material->height;
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

    suf->inFlags &= ~SUIF_UPDATE_DECORATIONS;
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

    updateSurfaceDecorations(suf, offsetS, offsetT, v1, v2, sec, suf->material? true : false);
}

static void updateSideSectionDecorations(sidedef_t* side, segsection_t section)
{
    linedef_t*          line;
    surface_t*          suf;
    vec3_t              v1, v2;
    int                 sid;
    float               offsetS = 0, offsetT = 0;
    boolean             visible = false;
    const plane_t*      frontCeil, *frontFloor, *backCeil, *backFloor;
    float               bottom, top;

    if(!side->segs || !side->segs[0])
        return;

    line = side->segs[0]->lineDef;
    sid = (line->L_backside && line->L_backside == side)? 1 : 0;
    frontCeil  = line->L_sector(sid)->SP_plane(PLN_CEILING);
    frontFloor = line->L_sector(sid)->SP_plane(PLN_FLOOR);

    if(line->L_backside)
    {
        backCeil  = line->L_sector(sid^1)->SP_plane(PLN_CEILING);
        backFloor = line->L_sector(sid^1)->SP_plane(PLN_FLOOR);
    }

    switch(section)
    {
    case SEG_MIDDLE:
        suf = &side->SW_middlesurface;
        if(suf->material)
            if(!line->L_backside)
            {
                top = frontCeil->visHeight;
                bottom = frontFloor->visHeight;
                if(line->flags & DDLF_DONTPEGBOTTOM)
                    offsetT += frontCeil->visHeight - frontFloor->visHeight;
                visible = true;
            }
            else
            {
                float texOffset[2];
                if(R_FindBottomTop(SEG_MIDDLE, 0, suf,
                             frontFloor, frontCeil, backFloor, backCeil,
                             (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
                             (line->flags & DDLF_DONTPEGTOP)? true : false,
                             (side->flags & SDF_MIDDLE_STRETCH)? true : false,
                             LINE_SELFREF(line)? true : false,
                             &bottom, &top, texOffset))
                {
                    offsetS = texOffset[VX];
                    offsetT = texOffset[VY];
                    visible = true;
                }
            }
        break;

    case SEG_TOP:
        suf = &side->SW_topsurface;
        if(suf->material)
            if(line->L_backside && backCeil->visHeight < frontCeil->visHeight &&
               (!R_IsSkySurface(&backCeil->surface) || !R_IsSkySurface(&frontCeil->surface)))
            {
                top = frontCeil->visHeight;
                bottom  = backCeil->visHeight;
                if(!(line->flags & DDLF_DONTPEGTOP))
                    offsetT += frontCeil->visHeight - backCeil->visHeight;
                visible = true;
            }
        break;

    case SEG_BOTTOM:
        suf = &side->SW_bottomsurface;
        if(suf->material)
            if(line->L_backside && backFloor->visHeight > frontFloor->visHeight &&
               (!R_IsSkySurface(&backFloor->surface) || !R_IsSkySurface(&frontFloor->surface)))
            {
                top = backFloor->visHeight;
                bottom  = frontFloor->visHeight;
                if(line->flags & DDLF_DONTPEGBOTTOM)
                    offsetT -= frontCeil->visHeight - backFloor->visHeight;
                visible = true;
            }
        break;
    }

    if(visible)
    {
        V3_Set(v1, line->L_vpos(sid  )[VX], line->L_vpos(sid  )[VY], top);
        V3_Set(v2, line->L_vpos(sid^1)[VX], line->L_vpos(sid^1)[VY], bottom);
    }

    updateSurfaceDecorations(suf, offsetS, offsetT, v1, v2, NULL, visible);
}

void Rend_UpdateSurfaceDecorations(void)
{
BEGIN_PROF( PROF_DECOR_UPDATE );

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
            if(suf->inFlags & SUIF_UPDATE_DECORATIONS)
                updateSideSectionDecorations(side, SEG_MIDDLE);

            suf = &side->SW_topsurface;
            if(suf->inFlags & SUIF_UPDATE_DECORATIONS)
                updateSideSectionDecorations(side, SEG_TOP);

            suf = &side->SW_bottomsurface;
            if(suf->inFlags & SUIF_UPDATE_DECORATIONS)
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
                if(pln->surface.inFlags & SUIF_UPDATE_DECORATIONS)
                    updatePlaneDecorations(pln);
            }
        }
    }

END_PROF( PROF_DECOR_UPDATE );
}

/**
 * Decorations are generated for each frame.
 */
void Rend_InitDecorationsForFrame(void)
{
#ifdef DD_PROFILE
    static int          i;

    if(++i > 40)
    {
        i = 0;
        PRINT_PROF( PROF_DECOR_UPDATE );
        PRINT_PROF( PROF_DECOR_PROJECT );
        PRINT_PROF( PROF_DECOR_ADD_LUMINOUS );
    }
#endif

    clearDecorations();

    // This only needs to be done if decorations have been enabled.
    if(useDecorations)
    {
        Rend_UpdateSurfaceDecorations(); // temporary.

BEGIN_PROF( PROF_DECOR_PROJECT );

        R_SurfaceListIterate(decoratedSurfaceList,
                             R_ProjectSurfaceDecorations, &decorMaxDist);

END_PROF( PROF_DECOR_PROJECT );
    }
}
