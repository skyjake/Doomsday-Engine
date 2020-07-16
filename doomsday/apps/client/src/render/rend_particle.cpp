/** @file rend_particle.cpp  Particle effect rendering.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "gl/texturecontent.h"

#include "world/map.h"
#include "world/p_players.h"
#include "world/convexsubspace.h"
#include "world/line.h"
#include "world/plane.h"
#include "world/subsector.h"

#include "resource/image.h"

#include "render/r_main.h"
#include "render/viewports.h"
#include "render/rend_main.h"
#include "render/rend_model.h"
#include "render/vissprite.h"

#include "clientapp.h"
#include "misc/r_util.h"
#include "sys_system.h"  // novideo

#include <doomsday/console/var.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/world/bspleaf.h>
#include <de/legacy/concurrency.h>
#include <de/legacy/vector1.h>
#include <de/folder.h>
#include <de/glinfo.h>
#include <de/imagefile.h>
#include <cstdlib>

using namespace de;
using namespace res;

// Point + custom textures.
#define NUM_TEX_NAMES (MAX_PTC_TEXTURES)

static DGLuint pointTex, ptctexname[MAX_PTC_TEXTURES];

static bool hasPoints, hasLines, hasModels, hasNoBlend, hasAdditive;
static bool hasPointTexs[NUM_TEX_NAMES];

struct OrderedParticle
{
    const Generator *generator;
    int particleId;
    float distance;
};
static OrderedParticle *order;
static size_t orderSize;

static size_t numParts;

/*
 * Console variables:
 */
dbyte useParticles = true;
static int maxParticles;           ///< @c 0= Unlimited.
static int particleNearLimit;
static float particleDiffuse = 4;

static float pointDist(fixed_t const c[3])
{
    const viewdata_t *viewData = &viewPlayer->viewport();
    float dist = ((viewData->current.origin.y - FIX2FLT(c[1])) * -viewData->viewSin)
                - ((viewData->current.origin.x - FIX2FLT(c[0])) * viewData->viewCos);

    return de::abs(dist);  // Always return positive.
}

static Path tryFindImage(String name)
{
    //
    // First look for a colorkeyed version.
    //
    try
    {
        String foundPath = App_FileSystem().findPath(res::Uri("Textures", name + "-ck"),
                                                     RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
        // Ensure the path is absolute.
        return App_BasePath() / foundPath;
    }
    catch(FS1::NotFoundError const&)
    {}  // Ignore this error.

    //
    // Look for the regular version.
    //
    try
    {
        String foundPath = App_FileSystem().findPath(res::Uri("Textures", name),
                                                     RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
        // Ensure the path is absolute.
        return App_BasePath() / foundPath;
    }
    catch(FS1::NotFoundError const&)
    {}  // Ignore this error.

    return Path();  // Not found.
}

// Try to load the texture.
static dbyte loadParticleTexture(duint particleTex)
{
    DE_ASSERT(particleTex < MAX_PTC_TEXTURES);

    image_t image;

    try
    {
        // First check if there is a texture asset for this particle.
        const String assetId = Stringf("texture.particle.%02i", particleTex);
        if (App::assetExists(assetId))
        {
            auto asset = App::asset(assetId);

            const ImageFile &img = App::rootFolder().locate<ImageFile const>
                    (asset.absolutePath(DE_STR("path")));

            Image_InitFromImage(image, img.image());
        }
        else
        {
            // Fallback: look in the Textures scheme.
            auto particleImageName = Stringf("Particle%02i", particleTex);
            Path foundPath = tryFindImage(particleImageName);
            if (foundPath.isEmpty())
                return 0;

            if (!GL_LoadImage(image, foundPath))
            {
                LOG_RES_WARNING("Failed to load \"%s\"") << NativePath(foundPath).pretty();
                return 0;
            }
        }
    }
    catch (const Error &er)
    {
        LOG_RES_ERROR("Failed to load texture for particle %i: %s")
                << particleTex << er.asText();
        return 0;
    }

    // If 8-bit with no alpha, generate alpha automatically.
    if (image.pixelSize == 1)
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

            DE_ASSERT(pointTex != 0);
        }
        Image_ClearPixelData(image);
    }
}

void Rend_ParticleLoadExtraTextures()
{
    if (novideo) return;

    Rend_ParticleReleaseExtraTextures();
    if (!App_GameLoaded()) return;

    List<int> loaded;
    for (int i = 0; i < MAX_PTC_TEXTURES; ++i)
    {
        if(loadParticleTexture(i))
        {
            loaded.append(i);
        }
    }

    if (!loaded.isEmpty())
    {
        LOG_RES_NOTE("Loaded textures for particle IDs: %s") << Rangei::contiguousRangesAsText(loaded);
    }
}

void Rend_ParticleReleaseSystemTextures()
{
    if(novideo) return;

    Deferred_glDeleteTextures(1, (const GLuint *) &pointTex);
    pointTex = 0;
}

void Rend_ParticleReleaseExtraTextures()
{
    if(novideo) return;

    Deferred_glDeleteTextures(NUM_TEX_NAMES, (const GLuint *) ptctexname);
    de::zap(ptctexname);
}

/**
 * Sorts in descending order.
 */
static int comparePOrder(const void *a, const void *b)
{
    const auto &ptA = *(const OrderedParticle *) a;
    const auto &ptB = *(const OrderedParticle *) b;

    if(ptA.distance > ptB.distance) return -1;
    if(ptA.distance < ptB.distance) return 1;
    return 0;  // Highly unlikely (but possible).
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

/**
 * Determines whether the given particle is potentially visible for the current viewer.
 */
static bool particlePVisible(const ParticleInfo &pinfo)
{
    // Never if it has already expired.
    if(pinfo.stage < 0) return false;

    // Never if the origin lies outside the map.
    if(!pinfo.bspLeaf || !pinfo.bspLeaf->hasSubspace())
        return false;

    // Potentially, if the subspace at the origin is visible.
    return R_ViewerSubspaceIsVisible(pinfo.bspLeaf->subspace().as<ConvexSubspace>());
 }

/**
 * @return  @c true if there are particles to be drawn.
 */
static int listVisibleParticles(Map &map)
{
    ::hasPoints = ::hasModels = ::hasLines = false;
    ::hasAdditive = ::hasNoBlend = false;
    de::zap(::hasPointTexs);

    // Count the total number of particles used by generators marked 'visible'.
    ::numParts = 0;
    map.forAllGenerators([] (Generator &gen)
    {
        if(R_ViewerGeneratorIsVisible(gen))
        {
            ::numParts += gen.activeParticleCount();
        }
        return LoopContinue;
    });
    if(!::numParts) return false;

    // Allocate the particle depth sort buffer.
    expandOrderBuffer(::numParts);

    // Populate the particle sort buffer and determine what type(s) of
    // particle (model/point/line/etc...) we'll need to draw.
    size_t numVisibleParts = 0;
    map.forAllGenerators([&numVisibleParts] (Generator &gen)
    {
        if(!R_ViewerGeneratorIsVisible(gen)) return LoopContinue;  // Skip.

        for(int i = 0; i < gen.count; ++i)
        {
            const ParticleInfo &pinfo = gen.particleInfo()[i];

            if(!particlePVisible(pinfo)) continue;  // Skip.

            // Skip particles too far from, or near to, the viewer.
            const float dist = de::max(pointDist(pinfo.origin), 1.f);
            if(gen.def->maxDist != 0 && dist > gen.def->maxDist) continue;
            if(dist < float( ::particleNearLimit )) continue;

            // This particle is visible. Add it to the sort buffer.
            OrderedParticle *slot = &::order[numVisibleParts++];
            slot->generator  = &gen;
            slot->particleId = i;
            slot->distance   = dist;

            // Determine what type of particle this is, as this will affect how
            // we go order our render passes and manipulate the render state.
            const int psType = gen.stages[pinfo.stage].type;
            if(psType == PTC_POINT)
            {
                ::hasPoints = true;
            }
            else if(psType == PTC_LINE)
            {
                ::hasLines = true;
            }
            else if(psType >= PTC_TEXTURE && psType < PTC_TEXTURE + MAX_PTC_TEXTURES)
            {
                if(::ptctexname[psType - PTC_TEXTURE])
                {
                    ::hasPointTexs[psType - PTC_TEXTURE] = true;
                }
                else
                {
                    ::hasPoints = true;
                }
            }
            else if(psType >= PTC_MODEL && psType < PTC_MODEL + MAX_PTC_MODELS)
            {
                ::hasModels = true;
            }

            if(gen.blendmode() == BM_ADD)
            {
                ::hasAdditive = true;
            }
            else
            {
                ::hasNoBlend = true;
            }
        }
        return LoopContinue;
    });

    // No visible particles?
    if(!numVisibleParts) return false;

    // This is the real number of possibly visible particles.
    ::numParts = numVisibleParts;

    // Sort the order list back->front. A quicksort is fast enough.
    qsort(::order, ::numParts, sizeof(OrderedParticle), comparePOrder);

    return true;
}

static void setupModelParamsForParticle(vissprite_t &spr, const ParticleInfo *pinfo,
    const GeneratorParticleStage *st, const ded_ptcstage_t *dst, const Vec3f &origin,
    float dist, float size, float mark, float alpha)
{
    drawmodelparams_t &parm = *VS_MODEL(&spr);

    spr.pose.origin     = Vec3d(origin.xz(), spr.pose.topZ = origin.y);
    spr.pose.distance   = dist;
    spr.pose.extraScale = size;  // Extra scaling factor.

    parm.mf = &ClientApp::resources().modelDef(dst->model);
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

    ClientApp::resources().setModelDefFrame(*parm.mf, frame);
    // Set the correct orientation for the particle.
    if(parm.mf->testSubFlag(0, MFF_MOVEMENT_YAW))
    {
        spr.pose.yaw = R_MovementXYYaw(FIX2FLT(pinfo->mov[0]), FIX2FLT(pinfo->mov[1]));
    }
    else
    {
        spr.pose.yaw = pinfo->yaw / 32768.0f * 180;
    }

    if(parm.mf->testSubFlag(0, MFF_MOVEMENT_PITCH))
    {
        spr.pose.pitch = R_MovementXYZPitch(FIX2FLT(pinfo->mov[0]), FIX2FLT(pinfo->mov[1]), FIX2FLT(pinfo->mov[2]));
    }
    else
    {
        spr.pose.pitch = pinfo->pitch / 32768.0f * 180;
    }

    spr.light.ambientColor.w = alpha;

    if(st->flags.testFlag(GeneratorParticleStage::Bright) || levelFullBright)
    {
        spr.light.ambientColor.x = spr.light.ambientColor.y = spr.light.ambientColor.z = 1;
        spr.light.vLightListIdx = 0;
    }
    else
    {
        world::Map &map = pinfo->bspLeaf->subspace().sector().map();

#if 0
        if(useBias && map.hasLightGrid())
        {
            Vec4f color = map.lightGrid().evaluate(spr.pose.origin);
            // Apply light range compression.
            for(int i = 0; i < 3; ++i)
            {
                color[i] += Rend_LightAdaptationDelta(color[i]);
            }
            spr.light.ambientColor.x = color.x;
            spr.light.ambientColor.y = color.y;
            spr.light.ambientColor.z = color.z;
        }
        else
#endif
        {
            const Vec4f color = pinfo->bspLeaf->subspace().subsector().as<Subsector>()
                                       .lightSourceColorfIntensity();

            float lightLevel = color.w;

            // Apply distance attenuation.
            lightLevel = Rend_AttenuateLightLevel(spr.pose.distance, lightLevel);

            // Add extra light.
            lightLevel += Rend_ExtraLightDelta();

            // The last step is to compress the resultant light value by
            // the global lighting function.
            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final ambientColor.
            for(int i = 0; i < 3; ++i)
            {
                spr.light.ambientColor[i] = lightLevel * color[i];
            }
        }
        Rend_ApplyTorchLight(spr.light.ambientColor, spr.pose.distance);

        spr.light.vLightListIdx =
                Rend_CollectAffectingLights(spr.pose.origin, spr.light.ambientColor,
                                            map.bspLeafAt(spr.pose.origin).subspacePtr());
    }
}

/**
 * Calculate a unit vector parallel to @a line.
 *
 * @todo No longer needed (Surface has tangent space vectors).
 *
 * @param unitVect  Unit vector is written here.
 */
static Vec2f lineUnitVector(const Line &line)
{
    double len = M_ApproxDistance(line.direction().x, line.direction().y);
    if(len)
    {
        return line.direction() / len;
    }
    return Vec2f();
}

static void drawParticles(int rtype, bool withBlend)
{
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    const viewdata_t *viewData = &viewPlayer->viewport();
    const Vec3f leftoff     = viewData->upVec + viewData->sideVec;
    const Vec3f rightoff    = viewData->upVec - viewData->sideVec;

    // Should we use a texture?
    DGLuint tex = 0;
    if (rtype == PTC_POINT
       || (rtype >= PTC_TEXTURE && rtype < PTC_TEXTURE + MAX_PTC_TEXTURES))
    {
        if (renderTextures)
        {
            if (rtype == PTC_POINT || 0 == ptctexname[rtype - PTC_TEXTURE])
                tex = pointTex;
            else
                tex = ptctexname[rtype - PTC_TEXTURE];
        }
    }

    dglprimtype_t primType = DGL_QUADS;
    if (rtype == PTC_MODEL)
    {
        DGL_Enable(DGL_DEPTH_WRITE);
        DGL_Enable(DGL_DEPTH_TEST);
    }
    else if (tex != 0)
    {
        DGL_Disable(DGL_DEPTH_WRITE);
        DGL_DepthFunc(DGL_LEQUAL);
        DGL_CullFace(DGL_NONE);

        GL_BindTextureUnmanaged(tex, gfx::ClampToEdge, gfx::ClampToEdge);
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Begin(primType = DGL_QUADS);
    }
    else
    {
        DGL_Begin(primType = DGL_LINES);
    }

    // How many particles will be drawn?
    size_t i = 0;
    if (maxParticles)
    {
        i = numParts - (unsigned) maxParticles;
    }

    blendmode_t mode = BM_NORMAL, newMode;
    for (; i < numParts; ++i)
    {
        const OrderedParticle *slot = &order[i];
        const Generator *gen        = slot->generator;
        const ParticleInfo &pinfo   = gen->particleInfo()[slot->particleId];

        const GeneratorParticleStage *st = &gen->stages[pinfo.stage];
        const ded_ptcstage_t *stDef      = &gen->def->stages[pinfo.stage];

        dshort stageType = st->type;
        if (stageType >= PTC_TEXTURE && stageType < PTC_TEXTURE + MAX_PTC_TEXTURES &&
            0 == ptctexname[stageType - PTC_TEXTURE])
        {
            stageType = PTC_POINT;
        }

        // Only render one type of particles.
        if ((rtype == PTC_MODEL && stDef->model < 0) ||
            (rtype != PTC_MODEL && stageType != rtype))
        {
            continue;
        }

        if (rtype >= PTC_TEXTURE && rtype < PTC_TEXTURE + MAX_PTC_TEXTURES &&
            0 == ptctexname[rtype - PTC_TEXTURE])
            continue;

        if ((gen->blendmode() != BM_ADD) == withBlend)
            continue;

        if (rtype != PTC_MODEL && !withBlend)
        {
            // We may need to change the blending mode.
            newMode = gen->blendmode();

            if(newMode != mode)
            {
                DGL_End();
                GL_BlendMode(mode = newMode);
                DGL_Begin(primType);
            }
        }

        // Is there a next stage for this particle?
        const ded_ptcstage_t *nextStDef;
        if (pinfo.stage >= gen->def->stages.size() - 1 ||
            !gen->stages[pinfo.stage + 1].type)
        {
            // There is no "next stage". Use the current one.
            nextStDef = &gen->def->stages[pinfo.stage];
        }
        else
        {
            nextStDef = &gen->def->stages[pinfo.stage + 1];
        }

        // Where is intermark?
        const float inter = 1 - float( pinfo.tics ) / stDef->tics;

        // Calculate size and color.
        float size = de::lerp(    stDef->particleRadius(slot->particleId),
                               nextStDef->particleRadius(slot->particleId), inter);

        // Infinitely small?
        if(!size) continue;

        Vec4f color = de::lerp(Vec4f(stDef->color), Vec4f(nextStDef->color), inter);

        if (!st->flags.testFlag(GeneratorParticleStage::Bright) && !levelFullBright)
        {
            // This is a simplified version of sectorlight (no distance attenuation or
            // range compression).
            if (world::ConvexSubspace *subspace = pinfo.bspLeaf->subspacePtr())
            {
                const float intensity = subspace->subsector().as<Subsector>()
                                            .lightSourceIntensity();
                color *= Vec4f(intensity, intensity, intensity, 1);
            }
        }

        const float maxDist = gen->def->maxDist;
        const float dist    = order[i].distance;

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

        DGL_Color4f(color.x, color.y, color.z, color.w);

        const bool nearWall = (pinfo.contact && !pinfo.mov[0] && !pinfo.mov[1]);

        bool nearPlane = false;
        if (world::ConvexSubspace *space = pinfo.bspLeaf->subspacePtr())
        {
            auto &subsec = space->subsector().as<Subsector>();
            if (   FLT2FIX(subsec.  visFloor().heightSmoothed()) + 2 * FRACUNIT >= pinfo.origin[2]
                || FLT2FIX(subsec.visCeiling().heightSmoothed()) - 2 * FRACUNIT <= pinfo.origin[2])
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

        Vec3f center = gen->particleOrigin(pinfo).xzy();

        if(!flatOnPlane && !flatOnWall)
        {
            Vec3f offset(frameTimePos, nearPlane ? 0 : frameTimePos, frameTimePos);
            center += offset * gen->particleMomentum(pinfo).xzy();
        }

        // Model particles are rendered using the normal model rendering routine.
        if(rtype == PTC_MODEL && stDef->model >= 0)
        {
            vissprite_t temp;
            setupModelParamsForParticle(temp, &pinfo, st, stDef, center, dist, size, inter, color.w);
            Rend_DrawModel(temp);
            continue;
        }

        // The vertices, please.
        if(tex != 0)
        {
            // Should the particle be flat against a plane?
            if(flatOnPlane)
            {
                DGL_TexCoord2f(0, 0, 0);
                DGL_Vertex3f(center.x - size, center.y, center.z - size);

                DGL_TexCoord2f(0, 1, 0);
                DGL_Vertex3f(center.x + size, center.y, center.z - size);

                DGL_TexCoord2f(0, 1, 1);
                DGL_Vertex3f(center.x + size, center.y, center.z + size);

                DGL_TexCoord2f(0, 0, 1);
                DGL_Vertex3f(center.x - size, center.y, center.z + size);
            }
            // Flat against a wall, then?
            else if(flatOnWall)
            {
                DE_ASSERT(pinfo.contact);
                const Line &contact = *pinfo.contact;

                // There will be a slight approximation on the XY plane since
                // the particles aren't that accurate when it comes to wall
                // collisions.

                // Calculate a new center point (project onto the wall).
                vec2d_t origin;
                V2d_Set(origin, FIX2FLT(pinfo.origin[0]), FIX2FLT(pinfo.origin[1]));

                vec2d_t projected;
                V2d_ProjectOnLine(projected, origin,
                                  contact.from().origin().data().baseAs<double>(),
                                  contact.direction().data().baseAs<double>());

                // Move away from the wall to avoid the worst Z-fighting.
                const double gap = -1;  // 1 map unit.
                double diff[2], dist;
                V2d_Subtract(diff, projected, origin);
                if((dist = V2d_Length(diff)) != 0)
                {
                    projected[0] += diff[0] / dist * gap;
                    projected[1] += diff[1] / dist * gap;
                }

                Vec2f unitVec = lineUnitVector(*pinfo.contact);

                DGL_TexCoord2f(0, 0, 0);
                DGL_Vertex3f(projected[0] - size * unitVec.x, center.y - size,
                           projected[1] - size * unitVec.y);

                DGL_TexCoord2f(0, 1, 0);
                DGL_Vertex3f(projected[0] - size * unitVec.x, center.y + size,
                           projected[1] - size * unitVec.y);

                DGL_TexCoord2f(0, 1, 1);
                DGL_Vertex3f(projected[0] + size * unitVec.x, center.y + size,
                           projected[1] + size * unitVec.y);

                DGL_TexCoord2f(0, 0, 1);
                DGL_Vertex3f(projected[0] + size * unitVec.x, center.y - size,
                           projected[1] + size * unitVec.y);
            }
            else
            {
                DGL_TexCoord2f(0, 0, 0);
                DGL_Vertex3f(center.x + size * leftoff.x,
                           center.y + size * leftoff.y / 1.2f,
                           center.z + size * leftoff.z);

                DGL_TexCoord2f(0, 1, 0);
                DGL_Vertex3f(center.x + size * rightoff.x,
                           center.y + size * rightoff.y / 1.2f,
                           center.z + size * rightoff.z);

                DGL_TexCoord2f(0, 1, 1);
                DGL_Vertex3f(center.x - size * leftoff.x,
                           center.y - size * leftoff.y / 1.2f,
                           center.z - size * leftoff.z);

                DGL_TexCoord2f(0, 0, 1);
                DGL_Vertex3f(center.x - size * rightoff.x,
                           center.y - size * rightoff.y / 1.2f,
                           center.z - size * rightoff.z);
            }
        }
        else  // It's a line.
        {
            DGL_Vertex3f(center.x, center.y, center.z);
            DGL_Vertex3f(center.x - FIX2FLT(pinfo.mov[0]),
                         center.y - FIX2FLT(pinfo.mov[2]),
                         center.z - FIX2FLT(pinfo.mov[1]));
        }
    }

    if(rtype != PTC_MODEL)
    {
        DGL_End();

        if(tex != 0)
        {
            DGL_CullFace(DGL_BACK);
            DGL_Enable(DGL_DEPTH_WRITE);
            DGL_DepthFunc(DGL_LESS);
            DGL_Disable(DGL_TEXTURE_2D);
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
    DE_ASSERT(!Sys_GLCheckError());

    // Set blending mode.
    if(useBlending)
    {
        GL_BlendMode(BM_ADD);
    }

    if(hasModels)
    {
        drawParticles(PTC_MODEL, useBlending);
    }

    if(hasLines)
    {
        drawParticles(PTC_LINE, useBlending);
    }

    if(hasPoints)
    {
        drawParticles(PTC_POINT, useBlending);
    }

    for(int i = 0; i < NUM_TEX_NAMES; ++i)
    {
        if(hasPointTexs[i])
        {
            drawParticles(PTC_TEXTURE + i, useBlending);
        }
    }

    // Restore blending mode.
    if(useBlending)
    {
        GL_BlendMode(BM_NORMAL);
    }

    DE_ASSERT(!Sys_GLCheckError());
}

void Rend_RenderParticles(Map &map)
{
    if(!useParticles) return;

    // No visible particles at all?
    if (!listVisibleParticles(map)) return;

    // Render all the visible particles.
    if(hasNoBlend)
    {
        renderPass(false);
    }

    if(hasAdditive)
    {
        // A second pass with additive blending.
        // This makes the additive particles 'glow' through all other
        // particles.
        renderPass(true);
    }
}

void Rend_ParticleRegister()
{
    C_VAR_BYTE ("rend-particle",                   &useParticles,      0,              0, 1);
    C_VAR_INT  ("rend-particle-max",               &maxParticles,      CVF_NO_MAX,     0, 0);
    C_VAR_FLOAT("rend-particle-diffuse",           &particleDiffuse,   CVF_NO_MAX,     0, 0);
    C_VAR_INT  ("rend-particle-visible-near",      &particleNearLimit, CVF_NO_MAX,     0, 0);
}
