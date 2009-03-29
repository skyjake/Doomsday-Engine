/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * rend_particle.c: Particle Effects
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

// Point + custom textures.
#define NUM_TEX_NAMES           (1 + MAX_PTC_TEXTURES)

// TYPES -------------------------------------------------------------------

typedef struct {
    ptcgenid_t      ptcGenID; // Generator id.
    int             ptID; // Particle id.
    float           distance;
} porder_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float vang, vpitch;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

DGLuint ptctexname[NUM_TEX_NAMES];
int particleNearLimit = 0;
float particleDiffuse = 4;
byte devDrawGenerators = false; // Display active generators?

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static size_t numParts;
static boolean hasPoints[NUM_TEX_NAMES], hasLines, hasNoBlend, hasBlend;
static boolean hasModels;
static byte visiblePtcGens[MAX_ACTIVE_PTCGENS];

static size_t orderSize = 0;
static porder_t* order = NULL;

// CODE --------------------------------------------------------------------

void Rend_ParticleRegister(void)
{
    // Cvars
    C_VAR_BYTE("rend-particle", &useParticles, 0, 0, 1);
    C_VAR_INT("rend-particle-max", &maxParticles, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-particle-rate", &particleSpawnRate, 0, 0, 5);
    C_VAR_FLOAT("rend-particle-diffuse", &particleDiffuse,
                CVF_NO_MAX, 0, 0);
    C_VAR_INT("rend-particle-visible-near", &particleNearLimit,
              CVF_NO_MAX, 0, 0);
    C_VAR_BYTE("rend-dev-generator-show-indices", &devDrawGenerators,
               CVF_NO_ARCHIVE, 0, 1);
}

static boolean markPtcGenVisible(ptcgen_t* gen, void* context)
{
    visiblePtcGens[P_PtcGenToIndex(gen)] = true;

    return true; // Continue iteration.
}

static boolean isPtcGenVisible(const ptcgen_t* gen)
{
    return visiblePtcGens[P_PtcGenToIndex(gen)];
}

static float pointDist(fixed_t c[3])
{
    float               dist = ((viewY - FIX2FLT(c[VY])) * -viewSin) -
        ((viewX - FIX2FLT(c[VX])) * viewCos);

    if(dist < 0)
        return -dist; // Always return positive.

    return dist;
}

/**
 * The particle texture is a modification of the dynlight texture.
 */
void Rend_ParticleInitTextures(void)
{
    int                 i;
    boolean             reported = false;

    if(ptctexname[0])
        return; // Already been here.

    // Clear the texture names array.
    memset(ptctexname, 0, sizeof(ptctexname));

    // Load the zeroth texture (the default: a blurred point).
    ptctexname[0] =
        GL_LoadGraphics2(RC_GRAPHICS, "Zeroth", LGM_WHITE_ALPHA, true, true, 0);

    if(ptctexname[0] == 0)
    {
        Con_Error("Rend_ParticleInitTextures: \"Zeroth\" not found.\n");
    }

    // Load any custom particle textures. They are loaded from the
    // highres texture directory and are named "ParticleNN.(tga|png|pcx)".
    // The first is "Particle00". (based on Leesz' textured particles mod)
    for(i = 0; i < MAX_PTC_TEXTURES; ++i)
    {
        char                filename[80];
        image_t             image;

        // Try to load the texture.
        sprintf(filename, "Particle%02i", i);
        if(GL_LoadTexture(&image, filename) == NULL)
        {
            // Just show the first 'not found'.
            if(verbose && !reported)
            {
                Con_Message("Rend_ParticleInitTextures: %s not found.\n", filename);
            }
            reported = true;
            continue;
        }

        VERBOSE(Con_Message
                ("Rend_ParticleInitTextures: Texture %02i: %i * %i * %i\n", i,
                 image.width, image.height, image.pixelSize));

        // If the source is 8-bit with no alpha, generate alpha automatically.
        if(image.originalBits == 8)
        {
            GL_ConvertToAlpha(&image, true);
        }

        // Create a new texture and upload the image.
        ptctexname[i + 1] = GL_NewTextureWithParams(image.pixelSize == 4 ? DGL_RGBA : image.pixelSize ==
                                                    2 ? DGL_LUMINANCE_PLUS_A8 : DGL_RGB,
                                                    image.width, image.height, image.pixels,
                                                    TXCF_NO_COMPRESSION);

        // Free the buffer.
        GL_DestroyImage(&image);
    }
}

void Rend_ParticleShutdownTextures(void)
{
    glDeleteTextures(NUM_TEX_NAMES, (const GLuint*) ptctexname);
    memset(ptctexname, 0, sizeof(ptctexname));
}

/**
 * Prepare for rendering a new view of the world.
 */
void Rend_ParticleInitForNewFrame(void)
{
    if(!useParticles)
        return;

    // Clear all visibility flags.
    memset(visiblePtcGens, 0, MAX_ACTIVE_PTCGENS);
}

/**
 * The given sector is visible. All PGs in it should be rendered.
 * Scans PG links.
 */
void Rend_ParticleMarkInSectorVisible(sector_t* sector)
{
    if(!useParticles)
        return;

    P_IterateSectorLinkedPtcGens(sector, markPtcGenVisible, NULL);
}

/**
 * Sorts in descending order.
 */
static int C_DECL comparePOrder(const void* pt1, const void* pt2)
{
    if(((porder_t *) pt1)->distance > ((porder_t *) pt2)->distance)
        return -1;
    else if(((porder_t *) pt1)->distance < ((porder_t *) pt2)->distance)
        return 1;

    // Highly unlikely (but possible)...
    return 0;
}

/**
 * Allocate more memory for the particle ordering buffer, if necessary.
 */
static void checkOrderBuffer(size_t max)
{
    size_t              currentSize = orderSize;

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
        order = Z_Realloc(order, sizeof(porder_t) * orderSize, PU_STATIC);
}

static boolean countParticles(ptcgen_t* gen, void* context)
{
    if(isPtcGenVisible(gen))
    {
        int                 p;
        size_t*             numParts = (uint*) context;

        for(p = 0; p < gen->count; ++p)
            if(gen->ptcs[p].stage >= 0)
                (*numParts)++;
    }

    return true; // Continue iteration.
}

static boolean populateSortBuffer(ptcgen_t* gen, void* context)
{
    int                 p;
    const ded_ptcgen_t* def;
    particle_t*         pt;
    size_t*             m = (size_t*) context;

    if(!isPtcGenVisible(gen))
        return true; // Continue iteration.

    def = gen->def;
    for(p = 0, pt = gen->ptcs; p < gen->count; ++p, pt++)
    {
        int                 stagetype;
        float               dist;
        porder_t*           slot;

        if(pt->stage < 0)
            continue;

        // Is the particle's sector visible?
        if(!(pt->sector->frameFlags & SIF_VISIBLE))
            continue; // No; this particle can't be seen.

        // Don't allow zero distance.
        dist = MAX_OF(pointDist(pt->pos), 1);
        if(def->maxDist != 0 && dist > def->maxDist)
            continue; // Too far.
        if(dist < (float) particleNearLimit)
            continue; // Too near.

        // This particle is visible. Add it to the sort buffer.
        slot = &order[(*m)++];

        slot->ptcGenID = P_PtcGenToIndex(gen);
        slot->ptID = p;
        slot->distance = dist;

        // Determine what type of particle this is, as this will affect how
        // we go order our render passes and manipulate the render state.
        stagetype = gen->stages[pt->stage].type;
        if(stagetype == PTC_POINT)
        {
            hasPoints[0] = true;
        }
        else if(stagetype == PTC_LINE)
        {
            hasLines = true;
        }
        else if(stagetype >= PTC_TEXTURE &&
                stagetype < PTC_TEXTURE + MAX_PTC_TEXTURES)
        {
            hasPoints[stagetype - PTC_TEXTURE + 1] = true;
        }
        else if(stagetype >= PTC_MODEL &&
                stagetype < PTC_MODEL + MAX_PTC_MODELS)
        {
            hasModels = true;
        }

        if(gen->flags & PGF_ADD_BLEND)
            hasBlend = true;
        else
            hasNoBlend = true;
    }

    return true; // Continue iteration.
}

/**
 * @return              @c true if there are particles to render.
 */
static int listVisibleParticles(void)
{
    size_t              numVisibleParticles;

    hasModels = hasLines = hasBlend = hasNoBlend = false;
    memset(hasPoints, 0, sizeof(hasPoints));

    // First count how many particles are in the visible generators.
    numParts = 0;
    P_IteratePtcGens(countParticles, &numParts);
    if(!numParts)
        return false; // No visible generators.

    // Allocate the particle depth sort buffer.
    checkOrderBuffer(numParts);

    // Populate the particle sort buffer and determine what type(s) of
    // particle (model/point/line/etc...) we'll need to draw.
    numVisibleParticles = 0;
    P_IteratePtcGens(populateSortBuffer, &numVisibleParticles);
    if(!numVisibleParticles)
        return false; // No visible particles (all too far?).

    // This is the real number of possibly visible particles.
    numParts = numVisibleParticles;

    // Sort the order list back->front. A quicksort is fast enough.
    qsort(order, numParts, sizeof(porder_t), comparePOrder);

    return true;
}

static void setupModelParamsForParticle(rendmodelparams_t* params,
                                        const particle_t* pt,
                                        const ptcstage_t* st,
                                        const ded_ptcstage_t* dst,
                                        float* center, float dist,
                                        float size, float mark, float alpha)
{
    int                 frame;
    subsector_t*        ssec;

    // Render the particle as a model.
    params->center[VX] = center[VX];
    params->center[VY] = center[VZ];
    params->center[VZ] = params->gzt = center[VY];
    params->distance = dist;
    ssec = R_PointInSubsector(center[VX], center[VY]);

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
        params->inter =
            M_CycleIntoRange(mark * (dst->endFrame - dst->frame), 1);
    }

    R_SetModelFrame(params->mf, frame);
    // Set the correct orientation for the particle.
    if(params->mf->sub[0].flags & MFF_MOVEMENT_YAW)
    {
        params->yaw =
            R_MovementYaw(FIX2FLT(pt->mov[0]), FIX2FLT(pt->mov[1]));
    }
    else
    {
        params->yaw = pt->yaw / 32768.0f * 180;
    }

    if(params->mf->sub[0].flags & MFF_MOVEMENT_PITCH)
    {
        params->pitch =
            R_MovementPitch(pt->mov[0], pt->mov[1], pt->mov[2]);
    }
    else
    {
        params->pitch = pt->pitch / 32768.0f * 180;
    }

    params->ambientColor[CA] = alpha;

    if((st->flags & PTCF_BRIGHT) || levelFullBright)
    {
        params->ambientColor[CR] = params->ambientColor[CG] =
            params->ambientColor[CB] = 1;
        params->vLightListIdx = 0;
    }
    else
    {
        collectaffectinglights_params_t lparams;

        if(useBias)
        {
            LG_Evaluate(params->center, params->ambientColor);
        }
        else
        {
            float               lightLevel = pt->sector->lightLevel;
            const float*        secColor = R_GetSectorLightColor(pt->sector);

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
        lparams.center[VX] = params->center[VX];
        lparams.center[VY] = params->center[VY];
        lparams.center[VZ] = params->center[VZ];
        lparams.subsector = ssec;
        lparams.ambientColor = params->ambientColor;

        params->vLightListIdx = R_CollectAffectingLights(&lparams);
    }
}

static void renderParticles(int rtype, boolean withBlend)
{
    size_t              i;
    int                 c, usingTexture = -1;
    float               leftoff[3], rightoff[3];
    ushort              primType = GL_QUADS;
    blendmode_t         mode = BM_NORMAL, newMode;

    // viewSideVec points to the left.
    for(c = 0; c < 3; ++c)
    {
        leftoff[c]  = viewUpVec[c] + viewSideVec[c];
        rightoff[c] = viewUpVec[c] - viewSideVec[c];
    }

    // Should we use a texture?
    if(rtype == PTC_POINT)
        usingTexture = 0;
    else if(rtype >= PTC_TEXTURE && rtype < PTC_TEXTURE + MAX_PTC_TEXTURES)
        usingTexture = rtype - PTC_TEXTURE + 1;

    if(rtype == PTC_MODEL)
    {
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }
    else if(usingTexture >= 0)
    {
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glBindTexture(GL_TEXTURE_2D, renderTextures ? ptctexname[usingTexture] : 0);
        glDepthFunc(GL_LEQUAL);
        glBegin(primType = GL_QUADS);
    }
    else
    {
        glDisable(GL_TEXTURE_2D); // Lines don't use textures.
        glBegin(primType = GL_LINES);
    }

    // How many particles can we render?
    if(maxParticles)
        i = numParts - (unsigned) maxParticles;
    else
        i = 0;

    for(; i < numParts; ++i)
    {
        const porder_t*     slot = &order[i];
        const ptcgen_t*     gen;
        const particle_t*   pt;
        const ptcstage_t*   st;
        const ded_ptcstage_t* dst, *nextDst;
        float               size, color[4], center[3], mark, invMark;
        float               dist, maxdist, projected[2];
        boolean             flatOnPlane, flatOnWall, nearPlane, nearWall;

        gen = P_IndexToPtcGen(slot->ptcGenID);
        pt = &gen->ptcs[slot->ptID];

        st = &gen->stages[pt->stage];
        dst = &gen->def->stages[pt->stage];

        // Only render one type of particles.
        if((rtype == PTC_MODEL && dst->model < 0) ||
           (rtype != PTC_MODEL && st->type != rtype))
        {
            continue;
        }

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
        {
            // There is no "next stage". Use the current one.
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
                     (FLT2FIX(pt->sector->SP_floorheight) + 2 * FRACUNIT >= pt->pos[VZ] ||
                      FLT2FIX(pt->sector->SP_ceilheight) - 2 * FRACUNIT <= pt->pos[VZ]));
        flatOnPlane = (st->flags & PTCF_PLANE_FLAT && nearPlane);

        nearWall = (pt->contact && !pt->mov[VX] && !pt->mov[VY]);
        flatOnWall = (st->flags & PTCF_WALL_FLAT && nearWall);

        center[VX] = FIX2FLT(pt->pos[VX]);
        center[VZ] = FIX2FLT(pt->pos[VY]);
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
            rendmodelparams_t       params;

            setupModelParamsForParticle(&params, pt, st, dst, center, dist,
                                        size, mark, color[CA]);
            Rend_RenderModel(&params);
            continue;
        }

        // The vertices, please.
        if(usingTexture >= 0)
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
                float               line[2], pos[2];
                vertex_t*           vtx;

                line[0] = pt->contact->dX;
                line[1] = pt->contact->dY;
                vtx = pt->contact->L_v1;

                // There will be a slight approximation on the XY plane since
                // the particles aren't that accurate when it comes to wall
                // collisions.

                // Calculate a new center point (project onto the wall).
                // Also move 1 unit away from the wall to avoid the worst
                // Z-fighting.
                pos[VX] = FIX2FLT(pt->pos[VX]);
                pos[VY] = FIX2FLT(pt->pos[VY]);
                M_ProjectPointOnLine(pos, &vtx->V_pos[VX], line, 1,
                                     projected);

                P_LineUnitVector(pt->contact, line);

                glTexCoord2f(0, 0);
                glVertex3f(projected[VX] - size * line[VX], center[VY] - size,
                           projected[VY] - size * line[VY]);

                glTexCoord2f(1, 0);
                glVertex3f(projected[VX] - size * line[VX], center[VY] + size,
                           projected[VY] - size * line[VY]);

                glTexCoord2f(1, 1);
                glVertex3f(projected[VX] + size * line[VX], center[VY] + size,
                           projected[VY] + size * line[VY]);

                glTexCoord2f(0, 1);
                glVertex3f(projected[VX] + size * line[VX], center[VY] - size,
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

        if(usingTexture >= 0)
        {
            glEnable(GL_CULL_FACE);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
        }
        else
        {
            glEnable(GL_TEXTURE_2D);
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
    int                 i;

    // Set blending mode.
    if(useBlending)
        GL_BlendMode(BM_ADD);

    if(hasModels)
        renderParticles(PTC_MODEL, useBlending);

    if(hasLines)
        renderParticles(PTC_LINE, useBlending);

    for(i = 0; i < NUM_TEX_NAMES; ++i)
        if(hasPoints[i])
        {
            renderParticles(!i ? PTC_POINT : PTC_TEXTURE + i - 1,
                            useBlending);
        }

    // Restore blending mode.
    if(useBlending)
        GL_BlendMode(BM_NORMAL);
}

/**
 * Render all the visible particle generators.
 * We must render all particles ordered back->front, or otherwise
 * particles from one generator will obscure particles from another.
 * This would be especially bad with smoke trails.
 */
void Rend_RenderParticles(void)
{
    if(!useParticles)
        return;

    if(!listVisibleParticles())
        return; // No visible particles at all?

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

static boolean drawGeneratorOrigin(ptcgen_t* gen, void* context)
{
#define MAX_GENERATOR_DIST  2048

    float*              eye = (float*) context;

    // Determine approximate center.
    if((gen->source || (gen->flags & PGF_UNTRIGGERED)))
    {
        float               pos[3], dist, alpha;

        if(gen->source)
        {
            pos[VX] = gen->source->pos[VX];
            pos[VY] = gen->source->pos[VY];
            pos[VZ] = gen->source->pos[VZ] - gen->source->floorClip +
                FIX2FLT(gen->center[VZ]);
        }
        else
        {
            pos[VX] = FIX2FLT(gen->center[VX]);
            pos[VY] = FIX2FLT(gen->center[VY]);
            pos[VZ] = FIX2FLT(gen->center[VZ]);
        }

        dist = M_Distance(pos, eye);
        alpha = 1 - MIN_OF(dist, MAX_GENERATOR_DIST) / MAX_GENERATOR_DIST;

        if(alpha > 0)
        {
            char                buf[80];
            float               scale = dist / (theWindow->width / 2);

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();

            glTranslatef(pos[VX], pos[VZ], pos[VY]);
            glRotatef(-vang + 180, 0, 1, 0);
            glRotatef(vpitch, 1, 0, 0);
            glScalef(-scale, -scale, 1);

            sprintf(buf, "%i", P_PtcGenToIndex(gen));
            UI_TextOutEx(buf, 2, 2, false, false, UI_Color(UIC_TITLE), alpha);

            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
        }
    }

    return true; // Continue iteration.

#undef MAX_GENERATOR_DIST
}

/**
 * Debugging aid; Draw all active generators.
 */
void Rend_RenderGenerators(void)
{
    float               eye[3];

    if(!devDrawGenerators)
        return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    eye[VX] = vx;
    eye[VY] = vz;
    eye[VZ] = vy;

    P_IteratePtcGens(drawGeneratorOrigin, eye);

    // Restore previous state.
    glEnable(GL_DEPTH_TEST);
}
