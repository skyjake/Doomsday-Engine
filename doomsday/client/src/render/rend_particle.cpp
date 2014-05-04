/** @file rend_particle.cpp  Particle effect rendering.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "render/rend_particle.h"

#include "clientapp.h"
#include "con_main.h"
#include "r_util.h"

#include "filesys/fs_main.h"

#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "gl/texturecontent.h"

#include "world/map.h"
#include "world/p_players.h"
#include "BspLeaf"
#include "Line"
#include "Plane"
#include "SectorCluster"

#include "resource/image.h"

#include "render/r_main.h"
#include "render/viewports.h"
#include "render/rend_main.h"
#include "render/rend_model.h"
#include "render/vlight.h"

#include <de/vector1.h>
#include <cstdlib>

using namespace de;

// Point + custom textures.
#define NUM_TEX_NAMES (MAX_PTC_TEXTURES)

static DGLuint pointTex, ptctexname[MAX_PTC_TEXTURES];

static bool hasPoints, hasLines, hasModels, hasNoBlend, hasBlend;
static bool hasPointTexs[NUM_TEX_NAMES];

struct OrderedParticle
{
    Generator *generator;
    int ptID; // Particle id.
    float distance;
};
static OrderedParticle *order;
static size_t orderSize;

static size_t numParts;

/*
 * Console variables:
 */
byte useParticles = true;
static int maxParticles; ///< @c 0= Unlimited.
static int particleNearLimit;
static float particleDiffuse = 4;

void Rend_ParticleRegister()
{
    // Cvars
    C_VAR_BYTE ("rend-particle",                   &useParticles,      0,              0, 1);
    C_VAR_INT  ("rend-particle-max",               &maxParticles,      CVF_NO_MAX,     0, 0);
    C_VAR_FLOAT("rend-particle-diffuse",           &particleDiffuse,   CVF_NO_MAX,     0, 0);
    C_VAR_INT  ("rend-particle-visible-near",      &particleNearLimit, CVF_NO_MAX,     0, 0);
}

static float pointDist(fixed_t const c[3])
{
    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
    float dist = ((viewData->current.origin.y - FIX2FLT(c[VY])) * -viewData->viewSin) -
        ((viewData->current.origin.x - FIX2FLT(c[VX])) * viewData->viewCos);

    return de::abs(dist); // Always return positive.
}

static Path tryFindImage(String name)
{
    /*
     * First look for a colorkeyed version.
     */
    try
    {
        String foundPath = App_FileSystem().findPath(de::Uri("Textures", name + "-ck"),
                                                     RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
        // Ensure the path is absolute.
        return App_BasePath() / foundPath;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.

    /*
     * Look for the regular version.
     */
    try
    {
        String foundPath = App_FileSystem().findPath(de::Uri("Textures", name),
                                                     RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
        // Ensure the path is absolute.
        return App_BasePath() / foundPath;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.

    return Path(); // Not found.
}

// Try to load the texture.
static byte loadParticleTexture(uint particleTex)
{
    DENG2_ASSERT(particleTex < MAX_PTC_TEXTURES);

    String particleImageName = String("Particle%1").arg(particleTex, 2, 10, QChar('0'));
    Path foundPath = tryFindImage(particleImageName);
    if(foundPath.isEmpty())
        return 0;

    image_t image;
    if(!GL_LoadImage(image, foundPath.toUtf8().constData()))
    {
        LOG_RES_WARNING("Failed to load \"%s\"") << foundPath;
        return 0;
    }

    // If 8-bit with no alpha, generate alpha automatically.
    if(image.pixelSize == 1)
        Image_ConvertToAlpha(image, true);

    // Create a new texture and upload the image.
    ptctexname[particleTex] = GL_NewTextureWithParams(
        image.pixelSize == 4 ? DGL_RGBA :
        image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 : DGL_RGB,
        image.size.x, image.size.y, image.pixels,
        TXCF_NO_COMPRESSION);

    // Free the buffer.
    Image_ClearPixelData(image);
    return 2; // External
}

void Rend_ParticleLoadSystemTextures()
{
    if(novideo) return;

    if(!pointTex)
    {
        // Load the default "zeroth" texture (a blurred point).
        /// @todo Create a logical Texture in the "System" scheme for this.
        image_t image;
        if(GL_LoadExtImage(image, "Zeroth", LGM_WHITE_ALPHA))
        {
            // Loaded successfully and converted accordingly.
            // Upload the image to GL.
            pointTex = GL_NewTextureWithParams(
                ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                  image.pixelSize == 3 ? DGL_RGB :
                  image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
                image.size.x, image.size.y, image.pixels,
                ( TXCF_MIPMAP | TXCF_NO_COMPRESSION ),
                0, glmode[mipmapping], GL_LINEAR, 0 /*no anisotropy*/, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

            DENG2_ASSERT(pointTex != 0);
        }
        Image_ClearPixelData(image);
    }
}

void Rend_ParticleLoadExtraTextures()
{
    if(novideo) return;

    Rend_ParticleReleaseExtraTextures();
    if(!App_GameLoaded()) return;

    QList<int> loaded;
    for(int i = 0; i < MAX_PTC_TEXTURES; ++i)
    {
        if(loadParticleTexture(i))
        {
            loaded.append(i);
        }
    }

    if(!loaded.isEmpty())
    {
        LOG_RES_NOTE("Loaded textures for particle IDs: %s") << Rangei::contiguousRangesAsText(loaded);
    }
}

void Rend_ParticleReleaseSystemTextures()
{
    if(novideo) return;

    glDeleteTextures(1, (GLuint const *) &pointTex);
    pointTex = 0;
}

void Rend_ParticleReleaseExtraTextures()
{
    if(novideo) return;

    glDeleteTextures(NUM_TEX_NAMES, (GLuint const *) ptctexname);
    de::zap(ptctexname);
}

/**
 * Sorts in descending order.
 */
static int comparePOrder(void const *pt1, void const *pt2)
{
    if(((OrderedParticle *) pt1)->distance > ((OrderedParticle *) pt2)->distance) return -1;
    else if(((OrderedParticle *) pt1)->distance < ((OrderedParticle *) pt2)->distance) return 1;
    // Highly unlikely (but possible)...
    return 0;
}

/**
 * Allocate more memory for the particle ordering buffer, if necessary.
 */
static void expandOrderBuffer(size_t max)
{
    size_t currentSize = orderSize;

    if(!orderSize)
    {
        orderSize = MAX_OF(max, 256);
    }
    else
    {
        while(max > orderSize)
        {
            orderSize *= 2;
        }
    }

    if(orderSize > currentSize)
    {
        order = (OrderedParticle *) Z_Realloc(order, sizeof(OrderedParticle) * orderSize, PU_APPSTATIC);
    }
}

static int countActiveGeneratorParticlesWorker(Generator *gen, void *context)
{
    if(R_ViewerGeneratorIsVisible(*gen))
    {
        *static_cast<size_t *>(context) += gen->activeParticleCount();
    }
    return false; // Continue iteration.
}

static int populateSortBuffer(Generator *gen, void *context)
{
    size_t *sortIndex = (size_t *) context;

    if(!R_ViewerGeneratorIsVisible(*gen))
        return false; // Continue iteration.

    ded_ptcgen_t const *def = gen->def;
    ParticleInfo const *pinfo = gen->particleInfo();
    for(int p = 0; p < gen->count; ++p, pinfo++)
    {
        if(pinfo->stage < 0) continue;

        ConvexSubspace *subspace = pinfo->bspLeaf? pinfo->bspLeaf->subspacePtr() : 0;
        if(!subspace) continue;

        // Is the BSP leaf at the particle's origin visible?
        if(!R_ViewerSubspaceIsVisible(*subspace))
            continue; // No; this particle can't be seen.

        // Don't allow zero distance.
        float dist = de::max(pointDist(pinfo->origin), 1.f);
        if(def->maxDist != 0 && dist > def->maxDist)
            continue; // Too far.
        if(dist < (float) particleNearLimit)
            continue; // Too near.

        // This particle is visible. Add it to the sort buffer.
        OrderedParticle *slot = &order[(*sortIndex)++];

        slot->generator = gen;
        slot->ptID      = p;
        slot->distance  = dist;

        // Determine what type of particle this is, as this will affect how
        // we go order our render passes and manipulate the render state.
        int stagetype = gen->stages[pinfo->stage].type;
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

        if(gen->blendmode() == BM_ADD)
        {
            hasBlend = true;
        }
        else
        {
            hasNoBlend = true;
        }
    }

    return false; // Continue iteration.
}

/**
 * @return  @c true if there are particles to be drawn.
 */
static int listVisibleParticles(Map &map)
{
    size_t numVisibleParticles;

    hasPoints = hasModels = hasLines = hasBlend = hasNoBlend = false;
    de::zap(hasPointTexs);

    // First count how many particles are in the visible generators.
    numParts = 0;
    map.generatorIterator(countActiveGeneratorParticlesWorker, &numParts);
    if(!numParts)
        return false; // No visible generators.

    // Allocate the particle depth sort buffer.
    expandOrderBuffer(numParts);

    // Populate the particle sort buffer and determine what type(s) of
    // particle (model/point/line/etc...) we'll need to draw.
    numVisibleParticles = 0;
    map.generatorIterator(populateSortBuffer, &numVisibleParticles);
    if(!numVisibleParticles)
        return false; // No visible particles (all too far?).

    // This is the real number of possibly visible particles.
    numParts = numVisibleParticles;

    // Sort the order list back->front. A quicksort is fast enough.
    qsort(order, numParts, sizeof(OrderedParticle), comparePOrder);

    return true;
}

static void setupModelParamsForParticle(drawmodelparams_t &parm,
    ParticleInfo const *pinfo, GeneratorParticleStage const *st,
    ded_ptcstage_t const *dst, Vector3f const &origin, float dist, float size,
    float mark, float alpha)
{
    zap(parm);

    // Render the particle as a model.
    parm.origin[VX] = origin.x;
    parm.origin[VY] = origin.z;
    parm.origin[VZ] = parm.gzt = origin.y;
    parm.distance = dist;

    parm.extraScale = size; // Extra scaling factor.
    parm.mf = &ClientApp::resourceSystem().modelDef(dst->model);
    parm.alwaysInterpolate = true;

    int frame;
    if(dst->endFrame < 0)
    {
        frame = dst->frame;
        parm.inter = 0;
    }
    else
    {
        frame = dst->frame + (dst->endFrame - dst->frame) * mark;
        parm.inter = M_CycleIntoRange(mark * (dst->endFrame - dst->frame), 1);
    }

    ClientApp::resourceSystem().setModelDefFrame(*parm.mf, frame);
    // Set the correct orientation for the particle.
    if(parm.mf->testSubFlag(0, MFF_MOVEMENT_YAW))
    {
        parm.yaw = R_MovementXYYaw(FIX2FLT(pinfo->mov[0]), FIX2FLT(pinfo->mov[1]));
    }
    else
    {
        parm.yaw = pinfo->yaw / 32768.0f * 180;
    }

    if(parm.mf->testSubFlag(0, MFF_MOVEMENT_PITCH))
    {
        parm.pitch = R_MovementXYZPitch(FIX2FLT(pinfo->mov[0]), FIX2FLT(pinfo->mov[1]), FIX2FLT(pinfo->mov[2]));
    }
    else
    {
        parm.pitch = pinfo->pitch / 32768.0f * 180;
    }

    parm.ambientColor[CA] = alpha;

    if(st->flags.testFlag(GeneratorParticleStage::Bright) || levelFullBright)
    {
        parm.ambientColor[CR] = parm.ambientColor[CG] = parm.ambientColor[CB] = 1;
        parm.vLightListIdx = 0;
    }
    else
    {
        Map &map = pinfo->bspLeaf->map();

        if(useBias && map.hasLightGrid())
        {
            Vector4f color = map.lightGrid().evaluate(parm.origin);
            // Apply light range compression.
            for(int i = 0; i < 3; ++i)
            {
                color[i] += Rend_LightAdaptationDelta(color[i]);
            }
            V3f_Set(parm.ambientColor, color.x, color.y, color.z);
        }
        else
        {
            Vector4f const color = pinfo->bspLeaf->cluster().lightSourceColorfIntensity();

            float lightLevel = color.w;

            // Apply distance attenuation.
            lightLevel = Rend_AttenuateLightLevel(parm.distance, lightLevel);

            // Add extra light.
            lightLevel += Rend_ExtraLightDelta();

            // The last step is to compress the resultant light value by
            // the global lighting function.
            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final ambientColor.
            for(int i = 0; i < 3; ++i)
            {
                parm.ambientColor[i] = lightLevel * color[i];
            }
        }

        Rend_ApplyTorchLight(parm.ambientColor, parm.distance);

        collectaffectinglights_params_t lparams; zap(lparams);
        lparams.origin       = Vector3d(parm.origin);
        lparams.subspace     = map.bspLeafAt(lparams.origin).subspacePtr();
        lparams.ambientColor = Vector3f(parm.ambientColor);

        parm.vLightListIdx = R_CollectAffectingLights(&lparams);
    }
}

/**
 * Calculate a unit vector parallel to @a line.
 *
 * @todo No longer needed (Surface has tangent space vectors).
 *
 * @param unitVect  Unit vector is written here.
 */
static Vector2f lineUnitVector(Line const &line)
{
    coord_t len = M_ApproxDistance(line.direction().x, line.direction().y);
    if(len)
    {
        return line.direction() / len;
    }
    return Vector2f(0, 0);
}

static void renderParticles(int rtype, bool withBlend)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

    Vector3f const leftoff  = viewData->upVec + viewData->sideVec;
    Vector3f const rightoff = viewData->upVec - viewData->sideVec;

    // Should we use a texture?
    DGLuint tex = 0;
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

    ushort primType = GL_QUADS;
    if(rtype == PTC_MODEL)
    {
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }
    else if(tex != 0)
    {
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        GL_BindTextureUnmanaged(tex, gl::ClampToEdge, gl::ClampToEdge);
        glEnable(GL_TEXTURE_2D);

        glDepthFunc(GL_LEQUAL);
        glBegin(primType = GL_QUADS);
    }
    else
    {
        glBegin(primType = GL_LINES);
    }

    // How many particles will be drawn?
    size_t i = 0;
    if(maxParticles)
    {
        i = numParts - (unsigned) maxParticles;
    }

    blendmode_t mode = BM_NORMAL, newMode;
    for(; i < numParts; ++i)
    {
        OrderedParticle const *slot      = &order[i];
        Generator const *gen      = slot->generator;
        ParticleInfo const *pinfo = &gen->particleInfo()[slot->ptID];

        GeneratorParticleStage const *st = &gen->stages[pinfo->stage];
        ded_ptcstage_t const *stDef      = &gen->def->stages[pinfo->stage];

        short stageType = st->type;
        if(stageType >= PTC_TEXTURE && stageType < PTC_TEXTURE + MAX_PTC_TEXTURES &&
           0 == ptctexname[stageType - PTC_TEXTURE])
        {
            stageType = PTC_POINT;
        }

        // Only render one type of particles.
        if((rtype == PTC_MODEL && stDef->model < 0) ||
           (rtype != PTC_MODEL && stageType != rtype))
        {
            continue;
        }

        if(rtype >= PTC_TEXTURE && rtype < PTC_TEXTURE + MAX_PTC_TEXTURES &&
           0 == ptctexname[rtype - PTC_TEXTURE])
            continue;

        if((gen->blendmode() != BM_ADD) == withBlend)
            continue;

        if(rtype != PTC_MODEL && !withBlend)
        {
            // We may need to change the blending mode.
            newMode = gen->blendmode();

            if(newMode != mode)
            {
                glEnd();
                GL_BlendMode(mode = newMode);
                glBegin(primType);
            }
        }

        // Is there a next stage for this particle?
        ded_ptcstage_t const *nextStDef;
        if(pinfo->stage >= gen->def->stageCount.num - 1 ||
           !gen->stages[pinfo->stage + 1].type)
        {
            // There is no "next stage". Use the current one.
            nextStDef = gen->def->stages + pinfo->stage;
        }
        else
        {
            nextStDef = gen->def->stages + (pinfo->stage + 1);
        }

        // Where is intermark?
        float const inter = 1 - float(pinfo->tics) / stDef->tics;

        // Calculate size and color.
        float size = de::lerp(    stDef->particleRadius(slot->ptID),
                              nextStDef->particleRadius(slot->ptID), inter);

        // Infinitely small?
        if(!size) continue;

        Vector4f color = de::lerp(Vector4f(stDef->color),
                                  Vector4f(nextStDef->color), inter);

        if(!st->flags.testFlag(GeneratorParticleStage::Bright) && !levelFullBright)
        {
            // This is a simplified version of sectorlight (no distance
            // attenuation or range compression).
            if(SectorCluster *cluster = pinfo->bspLeaf->clusterPtr())
            {
                float const lightLevel = cluster->lightSourceIntensity();
                color *= Vector4f(lightLevel, lightLevel, lightLevel, 1);
            }
        }

        float const maxDist = gen->def->maxDist;
        float const dist    = order[i].distance;

        // Far diffuse?
        if(maxDist)
        {
            if(dist > maxDist * .75f)
            {
                color.w *= 1 - (dist - maxDist * .75f) / (maxDist * .25f);
            }
        }
        // Near diffuse?
        if(particleDiffuse > 0)
        {
            if(dist < particleDiffuse * size)
            {
                color.w -= 1 - dist / (particleDiffuse * size);
            }
        }

        // Fully transparent?
        if(color.w <= 0)
            continue;

        glColor4f(color.x, color.y, color.z, color.w);

        bool nearWall = (pinfo->contact && !pinfo->mov[VX] && !pinfo->mov[VY]);

        bool nearPlane = false;
        if(SectorCluster *cluster = pinfo->bspLeaf->clusterPtr())
        {
            if(FLT2FIX(cluster->  visFloor().heightSmoothed()) + 2 * FRACUNIT >= pinfo->origin[VZ] ||
               FLT2FIX(cluster->visCeiling().heightSmoothed()) - 2 * FRACUNIT <= pinfo->origin[VZ])
            {
                nearPlane = true;
            }
        }

        bool flatOnPlane = false, flatOnWall = false;
        if(stageType == PTC_POINT ||
           (stageType >= PTC_TEXTURE && stageType < PTC_TEXTURE + MAX_PTC_TEXTURES))
        {
            if(st->flags.testFlag(GeneratorParticleStage::PlaneFlat) && nearPlane)
                flatOnPlane = true;
            if(st->flags.testFlag(GeneratorParticleStage::WallFlat) && nearWall)
                flatOnWall = true;
        }

        Vector3f center = gen->particleOrigin(*pinfo).xzy();

        if(!flatOnPlane && !flatOnWall)
        {
            Vector3f offset(frameTimePos, nearPlane? 0 : frameTimePos, frameTimePos);
            center += offset * gen->particleMomentum(*pinfo).xzy();
        }

        // Model particles are rendered using the normal model rendering routine.
        if(rtype == PTC_MODEL && stDef->model >= 0)
        {
            drawmodelparams_t parms;
            setupModelParamsForParticle(parms, pinfo, st, stDef, center, dist, size, inter, color.w);
            Rend_DrawModel(parms);
            continue;
        }

        // The vertices, please.
        if(tex != 0)
        {
            // Should the particle be flat against a plane?
            if(flatOnPlane)
            {
                glTexCoord2f(0, 0);
                glVertex3f(center.x - size, center.y, center.z - size);

                glTexCoord2f(1, 0);
                glVertex3f(center.x + size, center.y, center.z - size);

                glTexCoord2f(1, 1);
                glVertex3f(center.x + size, center.y, center.z + size);

                glTexCoord2f(0, 1);
                glVertex3f(center.x - size, center.y, center.z + size);
            }
            // Flat against a wall, then?
            else if(flatOnWall)
            {
                vec2d_t origin, projected;

                // There will be a slight approximation on the XY plane since
                // the particles aren't that accurate when it comes to wall
                // collisions.

                // Calculate a new center point (project onto the wall).
                V2d_Set(origin, FIX2FLT(pinfo->origin[VX]), FIX2FLT(pinfo->origin[VY]));

                coord_t linePoint[2]     = { pinfo->contact->fromOrigin().x, pinfo->contact->fromOrigin().y };
                coord_t lineDirection[2] = { pinfo->contact->direction().x, pinfo->contact->direction().y };
                V2d_ProjectOnLine(projected, origin, linePoint, lineDirection);

                // Move away from the wall to avoid the worst Z-fighting.
                double const gap = -1; // 1 map unit.
                double diff[2], dist;
                V2d_Subtract(diff, projected, origin);
                if((dist = V2d_Length(diff)) != 0)
                {
                    projected[VX] += diff[VX] / dist * gap;
                    projected[VY] += diff[VY] / dist * gap;
                }

                DENG2_ASSERT(pinfo->contact != 0);
                Vector2f unitVec = lineUnitVector(*pinfo->contact);

                glTexCoord2f(0, 0);
                glVertex3d(projected[VX] - size * unitVec.x, center.y - size,
                           projected[VY] - size * unitVec.y);

                glTexCoord2f(1, 0);
                glVertex3d(projected[VX] - size * unitVec.x, center.y + size,
                           projected[VY] - size * unitVec.y);

                glTexCoord2f(1, 1);
                glVertex3d(projected[VX] + size * unitVec.x, center.y + size,
                           projected[VY] + size * unitVec.y);

                glTexCoord2f(0, 1);
                glVertex3d(projected[VX] + size * unitVec.x, center.y - size,
                           projected[VY] + size * unitVec.y);
            }
            else
            {
                glTexCoord2f(0, 0);
                glVertex3f(center.x + size * leftoff[VX],
                           center.y + size * leftoff[VY] / 1.2f,
                           center.z + size * leftoff[VZ]);

                glTexCoord2f(1, 0);
                glVertex3f(center.x + size * rightoff[VX],
                           center.y + size * rightoff[VY] / 1.2f,
                           center.z + size * rightoff[VZ]);

                glTexCoord2f(1, 1);
                glVertex3f(center.x - size * leftoff[VX],
                           center.y - size * leftoff[VY] / 1.2f,
                           center.z - size * leftoff[VZ]);

                glTexCoord2f(0, 1);
                glVertex3f(center.x - size * rightoff[VX],
                           center.y - size * rightoff[VY] / 1.2f,
                           center.z - size * rightoff[VZ]);
            }
        }
        else // It's a line.
        {
            glVertex3f(center.x, center.y, center.z);
            glVertex3f(center.x - FIX2FLT(pinfo->mov[VX]),
                       center.y - FIX2FLT(pinfo->mov[VZ]),
                       center.z - FIX2FLT(pinfo->mov[VY]));
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

static void renderPass(bool useBlending)
{
    DENG2_ASSERT(!Sys_GLCheckError());

    // Set blending mode.
    if(useBlending)
    {
        GL_BlendMode(BM_ADD);
    }

    if(hasModels)
    {
        renderParticles(PTC_MODEL, useBlending);
    }

    if(hasLines)
    {
        renderParticles(PTC_LINE, useBlending);
    }

    if(hasPoints)
    {
        renderParticles(PTC_POINT, useBlending);
    }

    for(int i = 0; i < NUM_TEX_NAMES; ++i)
    {
        if(hasPointTexs[i])
        {
            renderParticles(PTC_TEXTURE + i, useBlending);
        }
    }

    // Restore blending mode.
    if(useBlending)
    {
        GL_BlendMode(BM_NORMAL);
    }

    DENG2_ASSERT(!Sys_GLCheckError());
}

void Rend_RenderParticles(Map &map)
{
    if(!useParticles) return;

    // No visible particles at all?
    if(!listVisibleParticles(map)) return;

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
