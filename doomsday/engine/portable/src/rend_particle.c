/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

// MACROS ------------------------------------------------------------------

// Point + custom textures.
#define NUM_TEX_NAMES (1 + MAX_PTC_TEXTURES)

// TYPES -------------------------------------------------------------------

typedef struct pglink_s {
    struct pglink_s *next;
    ptcgen_t *gen;
} pglink_t;

typedef struct {
    unsigned char gen;          // Index of the generator (activePtcGens)
    short   index;
    fixed_t distance;
} porder_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float vang, vpitch;

extern ptcgen_t *activePtcGens[MAX_ACTIVE_PTCGENS];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

DGLuint ptctexname[NUM_TEX_NAMES];
int     particleNearLimit = 0;
float   particleDiffuse = 4;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static pglink_t **pgLinks;      // Array of pointers to pgLinks in pgStore.
static pglink_t *pgStore;
static unsigned int pgCursor, pgMax;
static uint orderSize;
static porder_t *order;
static unsigned int numParts;
static boolean hasPoints[NUM_TEX_NAMES], hasLines, hasNoBlend, hasBlend;
static boolean hasModels;

// CODE --------------------------------------------------------------------

void Rend_ParticleRegister(void)
{
    // Cvars
    C_VAR_INT("rend-particle", &useParticles, 0, 0, 1);
    C_VAR_INT("rend-particle-max", &maxParticles, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-particle-rate", &particleSpawnRate, 0, 0, 5);
    C_VAR_FLOAT("rend-particle-diffuse", &particleDiffuse,
                CVF_NO_MAX, 0, 0);
    C_VAR_INT("rend-particle-visible-near", &particleNearLimit,
              CVF_NO_MAX, 0, 0);
}

static fixed_t PG_PointDist(fixed_t c[3])
{
    fixed_t dist = FixedMul(viewy - c[VY], -viewsin)    //viewsidex
        - FixedMul(viewx - c[VX], viewcos);

    if(dist < 0)
        return -dist;           // Always return positive.
    return dist;

    // Approximate the 3D distance to the point.
    /*return M_AproxDistance(M_AproxDistance(c[VX] - viewx, c[VY] - viewy),
       c[VZ] - viewz); */
}

/**
 * The particle texture is a modification of the dynlight texture.
 */
void PG_InitTextures(void)
{
    int     i;
    boolean reported = false;

    if(ptctexname[0])
        return;                 // Already been here.

    // Clear the texture names array.
    memset(ptctexname, 0, sizeof(ptctexname));

    // Load the zeroth texture (the default: a blurred point).
    ptctexname[0] =
        GL_LoadGraphics2(RC_GRAPHICS, "Zeroth", LGM_WHITE_ALPHA, DGL_TRUE, true);

    if(ptctexname[0] == 0)
    {
        Con_Error("PG_InitTextures: \"Zeroth\" not found.\n");
    }

    // Load any custom particle textures. They are loaded from the
    // highres texture directory and are named "ParticleNN.(tga|png|pcx)".
    // The first is "Particle00". (based on Leesz' textured particles mod)
    for(i = 0; i < MAX_PTC_TEXTURES; ++i)
    {
        char    filename[80];
        image_t image;

        // Try to load the texture.
        sprintf(filename, "Particle%02i", i);
        if(GL_LoadTexture(&image, filename) == NULL)
        {
            // Just show the first 'not found'.
            if(verbose && !reported)
            {
                Con_Message("PG_InitTextures: %s not found.\n", filename);
            }
            reported = true;
            continue;
        }

        VERBOSE(Con_Message
                ("PG_InitTextures: Texture %02i: %i * %i * %i\n", i,
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

void PG_ShutdownTextures(void)
{
    gl.DeleteTextures(NUM_TEX_NAMES, ptctexname);
    memset(ptctexname, 0, sizeof(ptctexname));
}

void PG_InitForLevel(void)
{
    pgLinks = Z_Malloc(sizeof(pglink_t *) * numsectors, PU_LEVEL, 0);
    memset(pgLinks, 0, sizeof(pglink_t *) * numsectors);

    // We can link 64 generators each into four sectors before
    // running out of pgLinks.
    pgMax = 4 * MAX_ACTIVE_PTCGENS;
    pgStore = Z_Malloc(sizeof(pglink_t) * pgMax, PU_LEVEL, 0);
    pgCursor = 0;

    memset(activePtcGens, 0, sizeof(activePtcGens));

    // Allocate a small ordering buffer.
    orderSize = 256;
    order = Z_Malloc(sizeof(porder_t) * orderSize, PU_LEVEL, 0);
}

/**
 * Returns an unused link from the pgStore.
 */
static pglink_t *PG_GetLink(void)
{
    if(pgCursor == pgMax)
    {
        if(verbose)
            Con_Message("PG_GetLink: Out of PGen store.\n");
        return NULL;
    }
    return &pgStore[pgCursor++];
}

static void PG_LinkPtcGen(ptcgen_t *gen, sector_t *sector)
{
    int     si = GET_SECTOR_IDX(sector);
    pglink_t *link;
    pglink_t *it;

    // Must check that it isn't already there...
    for(it = pgLinks[si]; it; it = it->next)
        if(it->gen == gen)
            return;             // No, no...

    // We need a new PG link.
    link = PG_GetLink();
    if(!link)
        return;                 // Out of links!

    link->gen = gen;
    link->next = pgLinks[si];
    pgLinks[si] = link;
}

/**
 * Init all active particle generators for a new frame.
 */
void PG_InitForNewFrame(void)
{
    unsigned int i;
    int         k;
    ptcgen_t   *gen;

    // Clear the PG links.
    memset(pgLinks, 0, sizeof(pglink_t *) * numsectors);
    pgCursor = 0;

    // Clear all visibility flags.
    for(i = 0; i < MAX_ACTIVE_PTCGENS; ++i)
        if((gen = activePtcGens[i]) != NULL)
        {
            gen->flags &= ~PGF_VISIBLE;
            // FIXME: Overkill?
            for(k = 0; k < gen->count; ++k)
                if(gen->ptcs[k].stage >= 0)
                    PG_LinkPtcGen(gen, gen->ptcs[k].sector);
        }
}

/**
 * The given sector is visible. All PGs in it should be rendered.
 * Scans PG links.
 */
void PG_SectorIsVisible(sector_t *sector)
{
    pglink_t *it = pgLinks[GET_SECTOR_IDX(sector)];

    for(; it; it = it->next)
        it->gen->flags |= PGF_VISIBLE;
}

/**
 * Sorts in descending order.
 */
static int C_DECL PG_Sorter(const void *pt1, const void *pt2)
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
static void PG_CheckOrderBuffer(uint max)
{
    while(max > orderSize)
        orderSize *= 2;
    order = Z_Realloc(order, sizeof(*order) * orderSize, PU_LEVEL);
}

/**
 * Returns true if there are particles to render.
 */
static int PG_ListVisibleParticles(void)
{
    unsigned int i, m;
    int     p, stagetype;
    fixed_t maxdist, mindist = FRACUNIT * particleNearLimit;
    ptcgen_t *gen;
    ded_ptcgen_t *def;
    particle_t *pt;

    hasModels = hasLines = hasBlend = hasNoBlend = false;
    memset(hasPoints, 0, sizeof(hasPoints));

    // First count how many particles are in the visible generators.
    for(i = 0, numParts = 0; i < MAX_ACTIVE_PTCGENS; ++i)
    {
        gen = activePtcGens[i];
        if(!gen || !(gen->flags & PGF_VISIBLE))
            continue;

        for(p = 0; p < gen->count; ++p)
            if(gen->ptcs[p].stage >= 0)
                numParts++;
    }
    if(!numParts)
        return false;

    // Allocate the rendering order list.
    PG_CheckOrderBuffer(numParts);

    // Fill in the order list and see what kind of particles we'll
    // need to render.
    for(i = 0, m = 0; i < MAX_ACTIVE_PTCGENS; ++i)
    {
        gen = activePtcGens[i];
        if(!gen || !(gen->flags & PGF_VISIBLE))
            continue;

        def = gen->def;
        maxdist = FRACUNIT * def->maxdist;
        for(p = 0, pt = gen->ptcs; p < gen->count; ++p, pt++)
        {
            if(pt->stage < 0)
                continue;
            // Is the particle's sector visible?
            if(!(pt->sector->frameflags & SIF_VISIBLE))
                continue;       // No; this particle can't be seen.

            order[m].gen = i;
            order[m].index = p;
            order[m].distance = PG_PointDist(pt->pos);
            // Don't allow zero distance.
            if(!order[m].distance)
                order[m].distance = 1;
            if(maxdist && order[m].distance > maxdist)
                continue;       // Too far.
            if(order[m].distance < mindist)
                continue;       // Too near.

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
            m++;
        }
    }
    if(!m)
    {
        // No particles left (all too far?).
        return false;
    }

    // This is the real number of possibly visible particles.
    numParts = m;

    // Sort the order list back->front. A quicksort is fast enough.
    qsort(order, numParts, sizeof(*order), PG_Sorter);
    return true;
}

static void PG_RenderParticles(int rtype, boolean withBlend)
{
    float   leftoff[3], rightoff[3], mark, invMark;
    float   size, color[4], center[3];
    float   dist, maxdist;
    float   projected[2];
    ptcgen_t *gen;
    ptcstage_t *st;
    particle_t *pt;
    ded_ptcstage_t *dst, *nextDst;
    unsigned int i;
    int     c;
    int     usingTexture = -1;
    int     primType = DGL_QUADS;
    blendmode_t mode = BM_NORMAL, newMode;
    boolean flatOnPlane, flatOnWall, nearPlane, nearWall;

    // viewsidevec points to the left.
    for(c = 0; c < 3; ++c)
    {
        leftoff[c]  = viewupvec[c] + viewsidevec[c];
        rightoff[c] = viewupvec[c] - viewsidevec[c];
    }

    // Should we use a texture?
    if(rtype == PTC_POINT)
        usingTexture = 0;
    else if(rtype >= PTC_TEXTURE && rtype < PTC_TEXTURE + MAX_PTC_TEXTURES)
        usingTexture = rtype - PTC_TEXTURE + 1;

    if(rtype == PTC_MODEL)
    {
        gl.Enable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
    }
    else if(usingTexture >= 0)
    {
        gl.Disable(DGL_DEPTH_WRITE);
        gl.Disable(DGL_CULL_FACE);
        gl.Bind(renderTextures ? ptctexname[usingTexture] : 0);
        gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
        gl.Begin(primType = DGL_QUADS);
    }
    else
    {
        gl.Disable(DGL_TEXTURING);  // Lines don't use textures.
        gl.Begin(primType = DGL_LINES);
    }

    // How many particles can we render?
    if(maxParticles)
    {
        i = numParts - maxParticles;
        if(i < 0)
            i = 0;
    }
    else
        i = 0;

    for(; i < numParts; ++i)
    {
        gen = activePtcGens[order[i].gen];
        pt = gen->ptcs + order[i].index;
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
                gl.End();
                GL_BlendMode(mode = newMode);
                gl.Begin(primType);
            }
        }

        // Is there a next stage for this particle?
        if(pt->stage >= gen->def->stage_count.num - 1 ||
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
        size =
            P_GetParticleRadius(dst,
                                order[i].index) * invMark +
            P_GetParticleRadius(nextDst, order[i].index) * mark;
        if(!size)
            continue;           // Infinitely small.
        for(c = 0; c < 4; ++c)
        {
            color[c] = dst->color[c] * invMark + nextDst->color[c] * mark;
            if(!(st->flags & PTCF_BRIGHT) && c < 3 && !levelFullBright)
            {
                // This is a simplified version of sectorlight (no distance
                // attenuation or range compression).
                if(pt->sector)
                    color[c] *= pt->sector->lightlevel;
            }
        }

        maxdist = gen->def->maxdist;
        dist = FIX2FLT(order[i].distance);

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

        gl.Color4fv(color);

        nearPlane = (pt->sector &&
                     (FLT2FIX(pt->sector->SP_floorheight) + 2 * FRACUNIT >= pt->pos[VZ] ||
                      FLT2FIX(pt->sector->SP_ceilheight) - 2 * FRACUNIT <= pt->pos[VZ]));
        flatOnPlane = (st->flags & PTCF_PLANE_FLAT && nearPlane);

        nearWall = (pt->contact && !pt->mov[VX] && !pt->mov[VY]);
        flatOnWall = (st->flags & PTCF_WALL_FLAT && nearWall);

        center[VX] = FIX2FLT(pt->pos[VX]);
        center[VZ] = FIX2FLT(pt->pos[VY]);
        center[VY] = FIX2FLT(P_GetParticleZ(pt));

        if(!flatOnPlane && !flatOnWall)
        {
            center[VX] += frameTimePos * FIX2FLT(pt->mov[VX]);
            center[VZ] += frameTimePos * FIX2FLT(pt->mov[VY]);
            if(!nearPlane) center[VY] += frameTimePos * FIX2FLT(pt->mov[VZ]);
        }

        // Model particles are rendered using the normal model rendering
        // routine.
        if(rtype == PTC_MODEL && dst->model >= 0)
        {
            int         frame;
            const float *slc;
            modelparams_t params;

            memset(&params, 0, sizeof(params));

            // Render the particle as a model.
            params.distance = dist;
            params.subsector =
                R_PointInSubsector(pt->pos[VX], pt->pos[VY]);
            params.center[VX] = center[VX];
            params.center[VY] = center[VZ];
            params.center[VZ] = params.gzt = center[VY];
            params.extraScale = size;   // Extra scaling factor.
            params.mf = &modefs[dst->model];
            params.alwaysInterpolate = true;

            if(dst->end_frame < 0)
            {
                frame = dst->frame;
                params.inter = 0;
            }
            else
            {
                frame = dst->frame + (dst->end_frame - dst->frame) * mark;
                params.inter =
                    M_CycleIntoRange(mark * (dst->end_frame - dst->frame), 1);
            }
            R_SetModelFrame(params.mf, frame);
            // Set the correct orientation for the particle.
            if(params.mf->sub[0].flags & MFF_MOVEMENT_YAW)
            {
                params.yaw = R_MovementYaw(pt->mov[0], pt->mov[1]);
            }
            else
            {
                params.yaw = pt->yaw / 32768.0f * 180;
            }
            if(params.mf->sub[0].flags & MFF_MOVEMENT_PITCH)
            {
                params.pitch =
                    R_MovementPitch(pt->mov[0], pt->mov[1], pt->mov[2]);
            }
            else
            {
                params.pitch = pt->pitch / 32768.0f * 180;
            }
            if(st->flags & PTCF_BRIGHT || levelFullBright)
                params.lightLevel = -1;    // Fullbright.
            else
            {
                params.lightLevel = pt->sector->lightlevel;
                Rend_ApplyLightAdaptation(&params.lightLevel);
            }
            slc = R_GetSectorLightColor(pt->sector);
            params.rgb[0] = slc[0];
            params.rgb[1] = slc[1];
            params.rgb[2] = slc[2];
            params.alpha = color[3];

            if(useWallGlow)
            {
                uint        c;
                sector_t   *sec = params.subsector->sector;

                // Do ceiling.
                if(sec && sec->planes[PLN_CEILING]->glow)
                {
                    for(c = 0; c < 3; ++c)
                        params.ceilGlowRGB[c] =
                            sec->planes[PLN_CEILING]->glowrgb[c] * dlFactor;

                    params.hasGlow = true;
                    params.ceilGlowAmount = sec->planes[PLN_CEILING]->glow;
                }

                // Do floor.
                if(sec && sec->planes[PLN_FLOOR]->glow)
                {
                    for(c = 0; c < 3; ++c)
                        params.floorGlowRGB[c] =
                            sec->planes[PLN_FLOOR]->glowrgb[c] * dlFactor;

                    params.hasGlow = true;
                    params.floorGlowAmount = sec->planes[PLN_FLOOR]->glow;
                }
            }
            Rend_RenderModel(&params);
            continue;
        }

        // The vertices, please.
        if(usingTexture >= 0)
        {
            // Should the particle be flat against a plane?
            if(flatOnPlane)
            {
                gl.TexCoord2f(0, 0);
                gl.Vertex3f(center[VX] - size, center[VY], center[VZ] - size);

                gl.TexCoord2f(1, 0);
                gl.Vertex3f(center[VX] + size, center[VY], center[VZ] - size);

                gl.TexCoord2f(1, 1);
                gl.Vertex3f(center[VX] + size, center[VY], center[VZ] + size);

                gl.TexCoord2f(0, 1);
                gl.Vertex3f(center[VX] - size, center[VY], center[VZ] + size);
            }
            // Flat against a wall, then?
            else if(flatOnWall)
            {
                float       line[2];
                float       pos[3];
                vertex_t   *vtx;

                line[0] = pt->contact->dx;
                line[1] = pt->contact->dy;
                vtx = pt->contact->L_v1;

                // There will be a slight approximation on the XY plane since
                // the particles aren't that accurate when it comes to wall
                // collisions.

                // Calculate a new center point (project onto the wall).
                // Also move 1 unit away from the wall to avoid the worst
                // Z-fighting.
                pos[VX] = FIX2FLT(pt->pos[VX]);
                pos[VY] = FIX2FLT(pt->pos[VY]);
                pos[VZ] = FIX2FLT(pt->pos[VZ]);
                M_ProjectPointOnLine(pos, &vtx->pos[VX], line, 1, projected);

                P_LineUnitVector(pt->contact, line);

                gl.TexCoord2f(0, 0);
                gl.Vertex3f(projected[VX] - size * line[VX], center[VY] - size,
                            projected[VY] - size * line[VY]);

                gl.TexCoord2f(1, 0);
                gl.Vertex3f(projected[VX] - size * line[VX], center[VY] + size,
                            projected[VY] - size * line[VY]);

                gl.TexCoord2f(1, 1);
                gl.Vertex3f(projected[VX] + size * line[VX], center[VY] + size,
                            projected[VY] + size * line[VY]);

                gl.TexCoord2f(0, 1);
                gl.Vertex3f(projected[VX] + size * line[VX], center[VY] - size,
                            projected[VY] + size * line[VY]);
            }
            else
            {
                gl.TexCoord2f(0, 0);
                gl.Vertex3f(center[VX] + size * leftoff[VX],
                            center[VY] + size * leftoff[VY] / 1.2f,
                            center[VZ] + size * leftoff[VZ]);

                gl.TexCoord2f(1, 0);
                gl.Vertex3f(center[VX] + size * rightoff[VX],
                            center[VY] + size * rightoff[VY] / 1.2f,
                            center[VZ] + size * rightoff[VZ]);

                gl.TexCoord2f(1, 1);
                gl.Vertex3f(center[VX] - size * leftoff[VX],
                            center[VY] - size * leftoff[VY] / 1.2f,
                            center[VZ] - size * leftoff[VZ]);

                gl.TexCoord2f(0, 1);
                gl.Vertex3f(center[VX] - size * rightoff[VX],
                            center[VY] - size * rightoff[VY] / 1.2f,
                            center[VZ] - size * rightoff[VZ]);
            }
        }
        else                    // It's a line.
        {
            gl.Vertex3f(center[VX], center[VY], center[VZ]);
            gl.Vertex3f(center[VX] - FIX2FLT(pt->mov[VX]),
                        center[VY] - FIX2FLT(pt->mov[VZ]),
                        center[VZ] - FIX2FLT(pt->mov[VY]));
        }
    }

    if(rtype != PTC_MODEL)
    {
        gl.End();

        if(usingTexture >= 0)
        {
            gl.Enable(DGL_DEPTH_WRITE);
            gl.Enable(DGL_CULL_FACE);
            gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
        }
        else
        {
            gl.Enable(DGL_TEXTURING);
        }
    }

    if(!withBlend)
    {
        // We may have rendered subtractive stuff.
        GL_BlendMode(BM_NORMAL);
    }
}

static void PG_RenderPass(boolean useBlending)
{
    int     i;

    // Set blending mode.
    if(useBlending)
        GL_BlendMode(BM_ADD);

    if(hasModels)
        PG_RenderParticles(PTC_MODEL, useBlending);

    if(hasLines)
        PG_RenderParticles(PTC_LINE, useBlending);

    for(i = 0; i < NUM_TEX_NAMES; ++i)
        if(hasPoints[i])
        {
            PG_RenderParticles(!i ? PTC_POINT : PTC_TEXTURE + i - 1,
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
void PG_Render(void)
{
    if(!useParticles)
        return;
    if(!PG_ListVisibleParticles())
        return;                 // No visible particles at all?

    // Render all the visible particles.
    if(hasNoBlend)
    {
        PG_RenderPass(false);
    }

    if(hasBlend)
    {
        // A second pass with additive blending.
        // This makes the additive particles 'glow' through all other
        // particles.
        PG_RenderPass(true);
    }
}
