/**
 * @file rend_particle.c
 * Particle Effect Rendering. @ingroup render
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "de_render.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"

#include "image.h"
#include "texturecontent.h"
#include "generators.h"

// Point + custom textures.
#define NUM_TEX_NAMES           (MAX_PTC_TEXTURES)

typedef struct {
    ptcgenid_t ptcGenID; // Generator id.
    int ptID; // Particle id.
    float distance;
} porder_t;

extern float vang, vpitch;

DGLuint pointTex, ptctexname[MAX_PTC_TEXTURES];
int particleNearLimit = 0;
float particleDiffuse = 4;
byte devDrawGenerators = false; // Display active generators?

static size_t numParts;
static boolean hasPoints, hasLines, hasModels, hasNoBlend, hasBlend;
static boolean hasPointTexs[NUM_TEX_NAMES];
static byte visiblePtcGens[GENERATORS_MAX];

static size_t orderSize = 0;
static porder_t* order = NULL;

// Currently active Generators collection. Global for performance.
static Generators* gens;

void Rend_ParticleRegister(void)
{
    // Cvars
    C_VAR_BYTE("rend-particle", &useParticles, 0, 0, 1);
    C_VAR_INT("rend-particle-max", &maxParticles, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-particle-rate", &particleSpawnRate, 0, 0, 5);
    C_VAR_FLOAT("rend-particle-diffuse", &particleDiffuse, CVF_NO_MAX, 0, 0);
    C_VAR_INT("rend-particle-visible-near", &particleNearLimit, CVF_NO_MAX, 0, 0);
    C_VAR_BYTE("rend-dev-generator-show-indices", &devDrawGenerators, CVF_NO_ARCHIVE, 0, 1);
}

static int markPtcGenVisible(ptcgen_t* gen, void* parameters)
{
    visiblePtcGens[Generators_GeneratorId(gens, gen)] = true;

    return false; // Continue iteration.
}

static boolean isPtcGenVisible(const ptcgen_t* gen)
{
    return visiblePtcGens[Generators_GeneratorId(gens, gen)];
}

static float pointDist(fixed_t c[3])
{
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
    float dist = ((viewData->current.origin[VY] - FIX2FLT(c[VY])) * -viewData->viewSin) -
        ((viewData->current.origin[VX] - FIX2FLT(c[VX])) * viewData->viewCos);

    if(dist < 0)
        return -dist; // Always return positive.

    return dist;
}

// Try to load the texture.
static byte loadParticleTexture(uint particleTex, boolean silent)
{
    assert(particleTex < MAX_PTC_TEXTURES);
    {
    ddstring_t foundPath, searchPath, suffix = { "-ck" };
    image_t image;
    byte result = 0;

    Str_Init(&foundPath);

    Str_Init(&searchPath);
    Str_Appendf(&searchPath, TEXTURES_RESOURCE_NAMESPACE_NAME":Particle%02i", particleTex);

    if(F_FindResourceStr4(RC_GRAPHIC, &searchPath, &foundPath, RLF_DEFAULT, &suffix) != 0 &&
       GL_LoadImage(&image, Str_Text(&foundPath)))
    {
        result = 2;
    }

    Str_Free(&searchPath);
    Str_Free(&foundPath);

    if(result != 0)
    {
        // If 8-bit with no alpha, generate alpha automatically.
        if(image.pixelSize == 1)
            Image_ConvertToAlpha(&image, true);

        // Create a new texture and upload the image.
        ptctexname[particleTex] = GL_NewTextureWithParams(
            image.pixelSize == 4 ? DGL_RGBA :
            image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 : DGL_RGB,
            image.size.width, image.size.height, image.pixels,
            TXCF_NO_COMPRESSION);

        // Free the buffer.
        GL_DestroyImage(&image);
    }
    else if(!silent)
    {
        Con_Message("Warning: Texture \"Particle%02i\" not found.\n", particleTex);
    }
    return result;
    }
}

void Rend_ParticleLoadSystemTextures(void)
{
    if(novideo)
        return;
    if(pointTex)
        return; // Already been here.

    // Load the default "zeroth" texture - a modification of the dynlight texture (a blurred point).
    pointTex = GL_PrepareExtTexture("Zeroth", LGM_WHITE_ALPHA, true, GL_LINEAR, GL_LINEAR, 0 /*no anisotropy*/,
        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 0);
    if(pointTex == 0)
    {
        Con_Error("Rend_ParticleLoadSystemTextures: \"Zeroth\" not found.\n");
    }
}

void Rend_ParticleLoadExtraTextures(void)
{
    int i;
    boolean reported = false;

    if(novideo) return;

    Rend_ParticleReleaseExtraTextures();
    if(!DD_GameLoaded()) return;

    for(i = 0; i < MAX_PTC_TEXTURES; ++i)
    {
        if(!loadParticleTexture(i, reported))
            reported = true;
    }
}

void Rend_ParticleReleaseSystemTextures(void)
{
    if(novideo) return;

    glDeleteTextures(1, (const GLuint*) &pointTex);
    pointTex = 0;
}

void Rend_ParticleReleaseExtraTextures(void)
{
    if(novideo) return;

    glDeleteTextures(NUM_TEX_NAMES, (const GLuint*) ptctexname);
    memset(ptctexname, 0, sizeof(ptctexname));
}

void Rend_ParticleInitForNewFrame(void)
{
    if(!useParticles) return;
    // Clear all visibility flags.
    memset(visiblePtcGens, 0, GENERATORS_MAX);
}

void Rend_ParticleMarkInSectorVisible(Sector* sector)
{
    if(!useParticles || !theMap || !sector) return;

    /// @todo Do the assume sector is from the CURRENT map.
    gens = GameMap_Generators(theMap);
    if(!gens) return;

    Generators_IterateList(gens, GameMap_SectorIndex(theMap, sector), markPtcGenVisible, NULL/*no parameters*/);
}

/**
 * Sorts in descending order.
 */
static int C_DECL comparePOrder(const void* pt1, const void* pt2)
{
    if(((porder_t *) pt1)->distance > ((porder_t *) pt2)->distance) return -1;
    else if(((porder_t *) pt1)->distance < ((porder_t *) pt2)->distance) return 1;
    // Highly unlikely (but possible)...
    return 0;
}

/**
 * Allocate more memory for the particle ordering buffer, if necessary.
 */
static void checkOrderBuffer(size_t max)
{
    size_t currentSize = orderSize;

    if(!orderSize)
    {
        orderSize = MAX_OF(max, 256);
    }
    else
    {
        while(max > orderSize)
            orderSize *= 2;
    }

    if(orderSize > currentSize)
        order = Z_Realloc(order, sizeof(porder_t) * orderSize, PU_APPSTATIC);
}

static int countParticles(ptcgen_t* gen, void* parameters)
{
    if(isPtcGenVisible(gen))
    {
        size_t* numParts = (size_t*) parameters;
        int p;
        for(p = 0; p < gen->count; ++p)
        {
            if(gen->ptcs[p].stage >= 0)
                (*numParts)++;
        }
    }
    return false; // Continue iteration.
}

static int populateSortBuffer(ptcgen_t* gen, void* parameters)
{
    size_t* m = (size_t*) parameters;
    const ded_ptcgen_t* def;
    particle_t* pt;
    int p;

    if(!isPtcGenVisible(gen))
        return false; // Continue iteration.

    def = gen->def;
    for(p = 0, pt = gen->ptcs; p < gen->count; ++p, pt++)
    {
        int stagetype;
        float dist;
        porder_t* slot;

        if(pt->stage < 0 || !pt->sector)
            continue;

        // Is the particle's sector visible?
        if(!(pt->sector->frameFlags & SIF_VISIBLE))
            continue; // No; this particle can't be seen.

        // Don't allow zero distance.
        dist = MAX_OF(pointDist(pt->origin), 1);
        if(def->maxDist != 0 && dist > def->maxDist)
            continue; // Too far.
        if(dist < (float) particleNearLimit)
            continue; // Too near.

        // This particle is visible. Add it to the sort buffer.
        slot = &order[(*m)++];

        slot->ptcGenID = Generators_GeneratorId(gens, gen);
        slot->ptID = p;
        slot->distance = dist;

        // Determine what type of particle this is, as this will affect how
        // we go order our render passes and manipulate the render state.
        stagetype = gen->stages[pt->stage].type;
        if(stagetype == PTC_POINT)
        {
            hasPoints = true;
        }
        else if(stagetype == PTC_LINE)
        {
            hasLines = true;
        }
        else if(stagetype >= PTC_TEXTURE && stagetype < PTC_TEXTURE + MAX_PTC_TEXTURES)
        {
            if(ptctexname[stagetype - PTC_TEXTURE])
                hasPointTexs[stagetype - PTC_TEXTURE] = true;
            else
                hasPoints = true;
        }
        else if(stagetype >= PTC_MODEL && stagetype < PTC_MODEL + MAX_PTC_MODELS)
        {
            hasModels = true;
        }

        if(gen->flags & PGF_ADD_BLEND)
            hasBlend = true;
        else
            hasNoBlend = true;
    }

    return false; // Continue iteration.
}

/**
 * @return              @c true if there are particles to render.
 */
static int listVisibleParticles(void)
{
    size_t numVisibleParticles;

    hasPoints = hasModels = hasLines = hasBlend = hasNoBlend = false;
    memset(hasPointTexs, 0, sizeof(hasPointTexs));

    // First count how many particles are in the visible generators.
    numParts = 0;
    Generators_Iterate(gens, countParticles, &numParts);
    if(!numParts)
        return false; // No visible generators.

    // Allocate the particle depth sort buffer.
    checkOrderBuffer(numParts);

    // Populate the particle sort buffer and determine what type(s) of
    // particle (model/point/line/etc...) we'll need to draw.
    numVisibleParticles = 0;
    Generators_Iterate(gens, populateSortBuffer, &numVisibleParticles);
    if(!numVisibleParticles)
        return false; // No visible particles (all too far?).

    // This is the real number of possibly visible particles.
    numParts = numVisibleParticles;

    // Sort the order list back->front. A quicksort is fast enough.
    qsort(order, numParts, sizeof(porder_t), comparePOrder);

    return true;
}

static void setupModelParamsForParticle(rendmodelparams_t* params,
    const particle_t* pt, const ptcstage_t* st, const ded_ptcstage_t* dst,
    float* origin, float dist, float size, float mark, float alpha)
{
    BspLeaf* bspLeaf;
    int frame;

    // Render the particle as a model.
    params->origin[VX] = origin[VX];
    params->origin[VY] = origin[VZ];
    params->origin[VZ] = params->gzt = origin[VY];
    params->distance = dist;
    bspLeaf = P_BspLeafAtPointXY(origin[VX], origin[VY]);

    params->extraScale = size; // Extra scaling factor.
    params->mf = &modefs[dst->model];
    params->alwaysInterpolate = true;

    if(dst->endFrame < 0)
    {
        frame = dst->frame;
        params->inter = 0;
    }
    else
    {
        frame = dst->frame + (dst->endFrame - dst->frame) * mark;
        params->inter = M_CycleIntoRange(mark * (dst->endFrame - dst->frame), 1);
    }

    R_SetModelFrame(params->mf, frame);
    // Set the correct orientation for the particle.
    if(params->mf->sub[0].flags & MFF_MOVEMENT_YAW)
    {
        params->yaw = R_MovementXYYaw(FIX2FLT(pt->mov[0]), FIX2FLT(pt->mov[1]));
    }
    else
    {
        params->yaw = pt->yaw / 32768.0f * 180;
    }

    if(params->mf->sub[0].flags & MFF_MOVEMENT_PITCH)
    {
        params->pitch = R_MovementXYZPitch(FIX2FLT(pt->mov[0]), FIX2FLT(pt->mov[1]), FIX2FLT(pt->mov[2]));
    }
    else
    {
        params->pitch = pt->pitch / 32768.0f * 180;
    }

    params->ambientColor[CA] = alpha;

    if((st->flags & PTCF_BRIGHT) || levelFullBright)
    {
        params->ambientColor[CR] = params->ambientColor[CG] = params->ambientColor[CB] = 1;
        params->vLightListIdx = 0;
    }
    else
    {
        collectaffectinglights_params_t lparams;

        if(useBias)
        {
            LG_Evaluate(params->origin, params->ambientColor);
        }
        else
        {
            float lightLevel = pt->sector->lightLevel;
            const float* secColor = R_GetSectorLightColor(pt->sector);

            // Apply distance attenuation.
            lightLevel = R_DistAttenuateLightLevel(params->distance, lightLevel);

            // Add extra light.
            lightLevel += R_ExtraLightDelta();

            Rend_ApplyLightAdaptation(&lightLevel);

            // Determine the final ambientColor in affect.
            params->ambientColor[CR] = lightLevel * secColor[CR];
            params->ambientColor[CG] = lightLevel * secColor[CG];
            params->ambientColor[CB] = lightLevel * secColor[CB];
        }

        Rend_ApplyTorchLight(params->ambientColor, params->distance);

        lparams.starkLight = false;
        lparams.origin[VX] = params->origin[VX];
        lparams.origin[VY] = params->origin[VY];
        lparams.origin[VZ] = params->origin[VZ];
        lparams.bspLeaf = bspLeaf;
        lparams.ambientColor = params->ambientColor;

        params->vLightListIdx = R_CollectAffectingLights(&lparams);
    }
}

static void renderParticles(int rtype, boolean withBlend)
{
    float leftoff[3], rightoff[3];
    ushort primType = GL_QUADS;
    blendmode_t mode = BM_NORMAL, newMode;
    DGLuint tex = 0;
    size_t i;
    int c;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    {
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
    // viewSideVec points to the left.
    for(c = 0; c < 3; ++c)
    {
        leftoff[c]  = viewData->upVec[c] + viewData->sideVec[c];
        rightoff[c] = viewData->upVec[c] - viewData->sideVec[c];
    }
    }

    // Should we use a texture?
    if(rtype == PTC_POINT ||
       (rtype >= PTC_TEXTURE && rtype < PTC_TEXTURE + MAX_PTC_TEXTURES))
    {
        if(renderTextures)
        {
            if(rtype == PTC_POINT || 0 == ptctexname[rtype - PTC_TEXTURE])
                tex = pointTex;
            else
                tex = ptctexname[rtype - PTC_TEXTURE];
        }
    }

    if(rtype == PTC_MODEL)
    {
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }
    else if(tex != 0)
    {
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        GL_BindTextureUnmanaged(tex, GL_LINEAR);
        glEnable(GL_TEXTURE_2D);

        glDepthFunc(GL_LEQUAL);
        glBegin(primType = GL_QUADS);
    }
    else
    {
        glBegin(primType = GL_LINES);
    }

    // How many particles can we render?
    if(maxParticles)
        i = numParts - (unsigned) maxParticles;
    else
        i = 0;

    for(; i < numParts; ++i)
    {
        const porder_t* slot = &order[i];
        const ptcgen_t* gen;
        const particle_t* pt;
        const ptcstage_t* st;
        const ded_ptcstage_t* dst, *nextDst;
        float size, color[4], center[3], mark, invMark;
        float dist, maxdist;
        boolean flatOnPlane = false, flatOnWall = false, nearPlane, nearWall;
        short stageType;

        gen = Generators_Generator(gens, slot->ptcGenID);
        pt = &gen->ptcs[slot->ptID];

        st = &gen->stages[pt->stage];
        dst = &gen->def->stages[pt->stage];

        stageType = st->type;
        if(stageType >= PTC_TEXTURE && stageType < PTC_TEXTURE + MAX_PTC_TEXTURES &&
           0 == ptctexname[stageType - PTC_TEXTURE])
            stageType = PTC_POINT;

        // Only render one type of particles.
        if((rtype == PTC_MODEL && dst->model < 0) ||
           (rtype != PTC_MODEL && stageType != rtype))
        {
            continue;
        }

        if(rtype >= PTC_TEXTURE && rtype < PTC_TEXTURE + MAX_PTC_TEXTURES &&
           0 == ptctexname[rtype - PTC_TEXTURE])
            continue;

        if(!(gen->flags & PGF_ADD_BLEND) == withBlend)
            continue;

        if(rtype != PTC_MODEL && !withBlend)
        {
            // We may need to change the blending mode.
            newMode =
                (gen->flags & PGF_SUB_BLEND ? BM_SUBTRACT : gen->
                 flags & PGF_REVSUB_BLEND ? BM_REVERSE_SUBTRACT : gen->
                 flags & PGF_MUL_BLEND ? BM_MUL : gen->
                 flags & PGF_INVMUL_BLEND ? BM_INVERSE_MUL : BM_NORMAL);
            if(newMode != mode)
            {
                glEnd();
                GL_BlendMode(mode = newMode);
                glBegin(primType);
            }
        }

        // Is there a next stage for this particle?
        if(pt->stage >= gen->def->stageCount.num - 1 ||
           !gen->stages[pt->stage + 1].type)
        {   // There is no "next stage". Use the current one.
            nextDst = gen->def->stages + pt->stage;
        }
        else
            nextDst = gen->def->stages + (pt->stage + 1);

        // Where is intermark?
        invMark = pt->tics / (float) dst->tics;
        mark = 1 - invMark;

        // Calculate size and color.
        size = P_GetParticleRadius(dst, slot->ptID) * invMark +
               P_GetParticleRadius(nextDst, slot->ptID) * mark;
        if(!size)
            continue; // Infinitely small.

        for(c = 0; c < 4; ++c)
        {
            color[c] = dst->color[c] * invMark + nextDst->color[c] * mark;
            if(!(st->flags & PTCF_BRIGHT) && c < 3 && !levelFullBright)
            {
                // This is a simplified version of sectorlight (no distance
                // attenuation or range compression).
                if(pt->sector)
                    color[c] *= pt->sector->lightLevel;
            }
        }

        maxdist = gen->def->maxDist;
        dist = order[i].distance;

        // Far diffuse?
        if(maxdist)
        {
            if(dist > maxdist * .75f)
                color[3] *= 1 - (dist - maxdist * .75f) / (maxdist * .25f);
        }

        // Near diffuse?
        if(particleDiffuse > 0)
        {
            if(dist < particleDiffuse * size)
                color[3] -= 1 - dist / (particleDiffuse * size);
        }

        // Fully transparent?
        if(color[3] <= 0)
            continue;

        glColor4fv(color);

        nearPlane = (pt->sector &&
                     (FLT2FIX(pt->sector->SP_floorheight) + 2 * FRACUNIT >= pt->origin[VZ] ||
                      FLT2FIX(pt->sector->SP_ceilheight)  - 2 * FRACUNIT <= pt->origin[VZ]));
        nearWall = (pt->contact && !pt->mov[VX] && !pt->mov[VY]);

        if(stageType == PTC_POINT || (stageType >= PTC_TEXTURE && stageType < PTC_TEXTURE + MAX_PTC_TEXTURES))
        {
            if((st->flags & PTCF_PLANE_FLAT) && nearPlane)
                flatOnPlane = true;
            if((st->flags & PTCF_WALL_FLAT) && nearWall)
                flatOnWall = true;
        }

        center[VX] = FIX2FLT(pt->origin[VX]);
        center[VZ] = FIX2FLT(pt->origin[VY]);
        center[VY] = P_GetParticleZ(pt);

        if(!flatOnPlane && !flatOnWall)
        {
            center[VX] += frameTimePos * FIX2FLT(pt->mov[VX]);
            center[VZ] += frameTimePos * FIX2FLT(pt->mov[VY]);
            if(!nearPlane)
                center[VY] += frameTimePos * FIX2FLT(pt->mov[VZ]);
        }

        // Model particles are rendered using the normal model rendering
        // routine.
        if(rtype == PTC_MODEL && dst->model >= 0)
        {
            rendmodelparams_t params;

            memset(&params, 0, sizeof(rendmodelparams_t));

            setupModelParamsForParticle(&params, pt, st, dst, center, dist, size, mark, color[CA]);
            Rend_RenderModel(&params);
            continue;
        }

        // The vertices, please.
        if(tex != 0)
        {
            // Should the particle be flat against a plane?
            if(flatOnPlane)
            {
                glTexCoord2f(0, 0);
                glVertex3f(center[VX] - size, center[VY], center[VZ] - size);

                glTexCoord2f(1, 0);
                glVertex3f(center[VX] + size, center[VY], center[VZ] - size);

                glTexCoord2f(1, 1);
                glVertex3f(center[VX] + size, center[VY], center[VZ] + size);

                glTexCoord2f(0, 1);
                glVertex3f(center[VX] - size, center[VY], center[VZ] + size);
            }
            // Flat against a wall, then?
            else if(flatOnWall)
            {
                vec2d_t origin, projected;
                float line[2];

                // There will be a slight approximation on the XY plane since
                // the particles aren't that accurate when it comes to wall
                // collisions.

                // Calculate a new center point (project onto the wall).
                V2d_Set(origin, FIX2FLT(pt->origin[VX]), FIX2FLT(pt->origin[VY]));
                V2d_ProjectOnLine(projected, origin, pt->contact->L_v1origin, pt->contact->direction);

                // Move away from the wall to avoid the worst Z-fighting.
                {
                const double gap = -1; // 1 map unit.
                double diff[2], dist;
                V2d_Subtract(diff, projected, origin);
                if((dist = V2d_Length(diff)) != 0)
                {
                    projected[VX] += diff[VX] / dist * gap;
                    projected[VY] += diff[VY] / dist * gap;
                }
                }

                LineDef_UnitVector(pt->contact, line);

                glTexCoord2f(0, 0);
                glVertex3d(projected[VX] - size * line[VX], center[VY] - size,
                           projected[VY] - size * line[VY]);

                glTexCoord2f(1, 0);
                glVertex3d(projected[VX] - size * line[VX], center[VY] + size,
                           projected[VY] - size * line[VY]);

                glTexCoord2f(1, 1);
                glVertex3d(projected[VX] + size * line[VX], center[VY] + size,
                           projected[VY] + size * line[VY]);

                glTexCoord2f(0, 1);
                glVertex3d(projected[VX] + size * line[VX], center[VY] - size,
                           projected[VY] + size * line[VY]);
            }
            else
            {
                glTexCoord2f(0, 0);
                glVertex3f(center[VX] + size * leftoff[VX],
                           center[VY] + size * leftoff[VY] / 1.2f,
                           center[VZ] + size * leftoff[VZ]);

                glTexCoord2f(1, 0);
                glVertex3f(center[VX] + size * rightoff[VX],
                           center[VY] + size * rightoff[VY] / 1.2f,
                           center[VZ] + size * rightoff[VZ]);

                glTexCoord2f(1, 1);
                glVertex3f(center[VX] - size * leftoff[VX],
                           center[VY] - size * leftoff[VY] / 1.2f,
                           center[VZ] - size * leftoff[VZ]);

                glTexCoord2f(0, 1);
                glVertex3f(center[VX] - size * rightoff[VX],
                           center[VY] - size * rightoff[VY] / 1.2f,
                           center[VZ] - size * rightoff[VZ]);
            }
        }
        else // It's a line.
        {
            glVertex3f(center[VX], center[VY], center[VZ]);
            glVertex3f(center[VX] - FIX2FLT(pt->mov[VX]),
                       center[VY] - FIX2FLT(pt->mov[VZ]),
                       center[VZ] - FIX2FLT(pt->mov[VY]));
        }
    }

    if(rtype != PTC_MODEL)
    {
        glEnd();

        if(tex != 0)
        {
            glEnable(GL_CULL_FACE);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);

            glDisable(GL_TEXTURE_2D);
        }
    }

    if(!withBlend)
    {
        // We may have rendered subtractive stuff.
        GL_BlendMode(BM_NORMAL);
    }
}

static void renderPass(boolean useBlending)
{
    int i;
    assert(!Sys_GLCheckError());

    // Set blending mode.
    if(useBlending)
        GL_BlendMode(BM_ADD);

    if(hasModels)
        renderParticles(PTC_MODEL, useBlending);

    if(hasLines)
        renderParticles(PTC_LINE, useBlending);

    if(hasPoints)
        renderParticles(PTC_POINT, useBlending);

    for(i = 0; i < NUM_TEX_NAMES; ++i)
    {
        if(hasPointTexs[i])
            renderParticles(PTC_TEXTURE + i, useBlending);
    }

    // Restore blending mode.
    if(useBlending)
        GL_BlendMode(BM_NORMAL);

    assert(!Sys_GLCheckError());
}

void Rend_RenderParticles(void)
{
    if(!useParticles || !theMap) return;

    gens = GameMap_Generators(theMap);
    if(!gens) return;

    // No visible particles at all?
    if(!listVisibleParticles()) return;

    // Render all the visible particles.
    if(hasNoBlend)
    {
        renderPass(false);
    }

    if(hasBlend)
    {
        // A second pass with additive blending.
        // This makes the additive particles 'glow' through all other
        // particles.
        renderPass(true);
    }
}

static int drawGeneratorOrigin(ptcgen_t* gen, void* parameters)
{
#define MAX_GENERATOR_DIST  2048

    float* eye = (float*) parameters;

    // Determine approximate center.
    if((gen->source || (gen->flags & PGF_UNTRIGGERED)))
    {
        float pos[3], dist, alpha;

        if(gen->source)
        {
            V3f_Copyd(pos, gen->source->origin);
            pos[VZ] += -gen->source->floorClip + FIX2FLT(gen->center[VZ]);
        }
        else
        {
            pos[VX] = FIX2FLT(gen->center[VX]);
            pos[VY] = FIX2FLT(gen->center[VY]);
            pos[VZ] = FIX2FLT(gen->center[VZ]);
        }

        dist = V3f_Distance(pos, eye);
        alpha = 1 - MIN_OF(dist, MAX_GENERATOR_DIST) / MAX_GENERATOR_DIST;

        if(alpha > 0)
        {
            const Point2Raw labelOrigin = { 2, 2 };
            float scale = dist / (Window_Width(theWindow) / 2);
            char buf[80];

            sprintf(buf, "%i", Generators_GeneratorId(gens, gen));

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();

            glTranslatef(pos[VX], pos[VZ], pos[VY]);
            glRotatef(-vang + 180, 0, 1, 0);
            glRotatef(vpitch, 1, 0, 0);
            glScalef(-scale, -scale, 1);

            glEnable(GL_TEXTURE_2D);
            FR_SetFont(fontFixed);
            FR_LoadDefaultAttrib();
            FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
            FR_SetShadowStrength(UI_SHADOW_STRENGTH);
            UI_TextOutEx(buf, &labelOrigin, UI_Color(UIC_TITLE), alpha);
            glDisable(GL_TEXTURE_2D);

            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
        }
    }

    return false; // Continue iteration.

#undef MAX_GENERATOR_DIST
}

void Rend_RenderGenerators(void)
{
    float eye[3];

    if(!devDrawGenerators || !theMap) return;

    gens = GameMap_Generators(theMap);
    if(!gens) return;

    eye[VX] = vOrigin[VX];
    eye[VY] = vOrigin[VZ];
    eye[VZ] = vOrigin[VY];

    glDisable(GL_DEPTH_TEST);

    Generators_Iterate(gens, drawGeneratorOrigin, eye);

    // Restore previous state.
    glEnable(GL_DEPTH_TEST);
}
