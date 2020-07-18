/** @file p_mobj.cpp  World map objects.
 *
 * Various routines for moving mobjs, collision and Z checking.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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
#include "world/p_object.h"

#include "def_main.h"
#include "api_sound.h"
#include "world/p_object.h"
#include "world/p_players.h"

#ifdef __CLIENT__
#  include "client/cl_mobj.h"
#  include "gl/gl_tex.h"
#  include "world/subsector.h"
#  include "world/generator.h"
#  include "network/net_demo.h"
#  include "render/viewports.h"
#  include "render/rend_main.h"
#  include "render/rend_model.h"
#  include "render/rend_halo.h"
#  include "render/billboard.h"
#  include "render/lumobj.h"
#endif

#include <doomsday/world/bspleaf.h>
#include <doomsday/world/convexsubspace.h>
#include <doomsday/world/subsector.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/var.h>
#include <doomsday/defs/sprite.h>
#include <doomsday/net.h>
#include <doomsday/res/textures.h>
#include <doomsday/res/sprites.h>
#include <doomsday/world/mobjthinkerdata.h>
#include <doomsday/world/materials.h>
#include <doomsday/world/thinkers.h>

#include <de/legacy/vector1.h>
#include <de/error.h>
#include <de/logbuffer.h>
#include <cmath>

using namespace de;

/*
 * Console variables:
 */
dint useSRVO      = 2;  ///< @c 1= models only, @c 2= sprites + models
dint useSRVOAngle = 1;

#ifdef __CLIENT__
static byte mobjAutoLights = true;
#endif

dd_bool Mobj_SetOrigin(struct mobj_s *mob, coord_t x, coord_t y, coord_t z)
{
    if(!gx.MobjTryMoveXYZ)
    {
        return false;
    }
    return gx.MobjTryMoveXYZ(mob, x, y, z);
}

bool Mobj_HasSubsector(const mobj_t &mob)
{
    if (!Mobj_IsLinked(mob)) return false;
    const auto &bspLeaf = Mobj_BspLeafAtOrigin(mob);
    if (!bspLeaf.hasSubspace()) return false;
    return bspLeaf.subspace().hasSubsector();
}

world_Subsector &Mobj_Subsector(const mobj_t &mob)
{
    return Mobj_BspLeafAtOrigin(mob).subspace().subsector();
}

world_Subsector *Mobj_SubsectorPtr(const mobj_t &mob)
{
    return Mobj_HasSubsector(mob) ? &Mobj_Subsector(mob) : nullptr;
}

void Mobj_SpawnParticleGen(mobj_t *mob, const ded_ptcgen_t *def)
{
#ifdef __CLIENT__
    DE_ASSERT(mob && def);

    //if (!useParticles) return;

    Generator *gen = Mobj_Map(*mob).as<Map>().newGenerator();
    if (!gen) return;

    /*LOG_INFO("SpawnPtcGen: %s/%i (src:%s typ:%s mo:%p)")
        << def->state << (def - defs.ptcgens) << defs.states[mob->state-states].id
        << defs.mobjs[mob->type].id << source;*/

    // Initialize the particle generator.
    gen->count = def->particles;
    // Size of source sector might determine count.
    if (def->flags & Generator::ScaledRate)
    {
        gen->spawnRateMultiplier = Mobj_BspLeafAtOrigin(*mob).sectorPtr()->roughArea() / (128 * 128);
    }
    else
    {
        gen->spawnRateMultiplier = 1;
    }

    gen->configureFromDef(def);
    gen->source = mob;
    gen->srcid  = mob->thinker.id;

    // Is there a need to pre-simulate?
    gen->presimulate(def->preSim);
#else
    DE_UNUSED(mob, def);
#endif
}

void Mobj_SpawnDamageParticleGen(const mobj_t *mob, const mobj_t *inflictor, int amount)
{
#ifdef __CLIENT__
    if (!mob || !inflictor || amount <= 0) return;

    // Are particles allowed?
    //if (!useParticles) return;

    const ded_ptcgen_t *def = Def_GetDamageGenerator(mob->type);
    if (def)
    {
        Generator *gen = Mobj_Map(*mob).as<Map>().newGenerator();
        if (!gen) return; // No more generators.

        gen->count = def->particles;
        gen->configureFromDef(def);
        gen->setUntriggered();

        gen->spawnRateMultiplier = de::max(amount, 1);

        // Calculate appropriate center coordinates.
        gen->originAtSpawn[0] += FLT2FIX(mob->origin[0]);
        gen->originAtSpawn[1] += FLT2FIX(mob->origin[1]);
        gen->originAtSpawn[2] += FLT2FIX(mob->origin[2] + mob->height / 2);

        // Calculate launch vector.
        vec3f_t vecDelta;
        V3f_Set(vecDelta, inflictor->origin[0] - mob->origin[0],
                inflictor->origin[1] - mob->origin[1],
                (inflictor->origin[2] - inflictor->height / 2) - (mob->origin[2] + mob->height / 2));

        vec3f_t vector;
        V3f_SetFixed(vector, gen->vector[0], gen->vector[1], gen->vector[2]);
        V3f_Sum(vector, vector, vecDelta);
        V3f_Normalize(vector);

        gen->vector[0] = FLT2FIX(vector[0]);
        gen->vector[1] = FLT2FIX(vector[1]);
        gen->vector[2] = FLT2FIX(vector[2]);

        // Is there a need to pre-simulate?
        gen->presimulate(def->preSim);
    }
#else
    DE_UNUSED(mob, inflictor, amount);
#endif
}

#if defined(__CLIENT__)

void Mobj_OriginSmoothed(const mobj_t *mob, coord_t origin[3])
{
    if (!origin) return;

    V3d_Set(origin, 0, 0, 0);
    if (!mob) return;

    V3d_Copy(origin, mob->origin);

    // Apply a Short Range Visual Offset?
    if (useSRVO && mob->state && mob->tics >= 0)
    {
        const ddouble mul = mob->tics / dfloat( mob->state->tics );
        vec3d_t srvo;

        V3d_Copy(srvo, mob->srvo);
        V3d_Scale(srvo, mul);
        V3d_Sum(origin, origin, srvo);
    }

    if (mob->dPlayer)
    {
        /// @todo What about splitscreen? We have smoothed origins for all local players.
        if (P_GetDDPlayerIdx(mob->dPlayer) == consolePlayer
            // $voodoodolls: Must be a real player to use the smoothed origin.
            && mob->dPlayer->mo == mob)
        {
            const viewdata_t *vd = &DD_Player(consolePlayer)->viewport();
            V3d_Set(origin, vd->current.origin.x, vd->current.origin.y, vd->current.origin.z);
        }
        // The client may have a Smoother for this object.
        else if (netState.isClient)
        {
            Smoother_Evaluate(DD_Player(P_GetDDPlayerIdx(mob->dPlayer))->smoother(), origin);
        }
    }
}

angle_t Mobj_AngleSmoothed(const mobj_t *mob)
{
    if (!mob) return 0;

    if (mob->dPlayer)
    {
        /// @todo What about splitscreen? We have smoothed angles for all local players.
        if (P_GetDDPlayerIdx(mob->dPlayer) == ::consolePlayer
            // $voodoodolls: Must be a real player to use the smoothed angle.
            && mob->dPlayer->mo == mob)
        {
            const viewdata_t *vd = &DD_Player(::consolePlayer)->viewport();
            return vd->current.angle();
        }
    }

    // Apply a Short Range Visual Offset?
    if (::useSRVOAngle && !netState.netGame && !::playback)
    {
        return mob->visAngle << 16;
    }

    return mob->angle;
}

dd_bool Mobj_OriginBehindVisPlane(mobj_t *mob)
{
    if (!mob || !Mobj_HasSubsector(*mob)) return false;

    auto &subsec = Mobj_Subsector(*mob).as<Subsector>();

    if (&subsec.sector().floor() != &subsec.visFloor()
        && mob->origin[2] < subsec.visFloor().heightSmoothed())
        return true;

    if (&subsec.sector().ceiling() != &subsec.visCeiling()
        && mob->origin[2] > subsec.visCeiling().heightSmoothed())
        return true;

    return false;
}

void Mobj_UnlinkLumobjs(mobj_t *mob)
{
    if (!mob) return;
    mob->lumIdx = Lumobj::NoIndex;
}

static ded_light_t *lightDefByMobjState(const state_t *state)
{
    if (state)
    {
        return runtimeDefs.stateInfo[runtimeDefs.states.indexOf(state)].light;
    }
    return nullptr;
}

static inline ClientTexture *lightmap(const res::Uri *textureUri)
{
    if(!textureUri) return nullptr;
    return static_cast<ClientTexture *>
            (res::Textures::get().tryFindTextureByResourceUri(DE_STR("Lightmaps"), *textureUri));
}

void Mobj_GenerateLumobjs(mobj_t *mob)
{
    if (!mob) return;

    Mobj_UnlinkLumobjs(mob);

    if (!Mobj_HasSubsector(*mob)) return;
    auto &subsec = Mobj_Subsector(*mob).as<Subsector>();

    if (!(((mob->state && (mob->state->flags & STF_FULLBRIGHT))
            && !(mob->ddFlags & DDMF_DONTDRAW))
          || (mob->ddFlags & DDMF_ALWAYSLIT)))
    {
        return;
    }

    // Are the automatically calculated light values for fullbright sprite frames in use?
    if (mob->state
        && (!mobjAutoLights || (mob->state->flags & STF_NOAUTOLIGHT))
        && !runtimeDefs.stateInfo[runtimeDefs.states.indexOf(mob->state)].light)
    {
       return;
    }

    // If the mobj's origin is outside the BSP leaf it is linked within, then
    // this means it is outside the playable map (and no light should be emitted).
    /// @todo Optimize: Mobj_Link() should do this and flag the mobj accordingly.
    if (!Mobj_BspLeafAtOrigin(*mob).subspace().contains(Vec2d(mob->origin)))
        return;

    // Always use the front view of the Sprite when determining light properties.
    const Record *spriteRec = Mobj_SpritePtr(*mob);
    if (!spriteRec) return;

    // Lookup the Material for the Sprite and prepare the animator.
    MaterialAnimator *matAnimator = Rend_SpriteMaterialAnimator(*spriteRec);
    if (!matAnimator) return;
    matAnimator->prepare();  // Ensure we have up-to-date info.

    TextureVariant *tex = matAnimator->texUnit(MaterialAnimator::TU_LAYER0).texture;
    if (!tex) return;  // Unloadable texture?
    const Vec2i &texOrigin = tex->base().origin();

    // Will the visual be allowed to go inside the floor?
    /// @todo Handle this as occlusion so that the halo fades smoothly.
    coord_t impacted = mob->origin[2] + -texOrigin.y - matAnimator->dimensions().y
                     - subsec.visFloor().heightSmoothed();

    // If the floor is a visual plane then no light should be emitted.
    if (impacted < 0 && &subsec.visFloor() != &subsec.sector().floor())
        return;

    // Attempt to generate luminous object from the sprite.
    std::unique_ptr<Lumobj> lum(Rend_MakeLumobj(*spriteRec));
    if (!lum) return;

    lum->setSourceMobj(mob);

    // A light definition may override the (auto-calculated) defaults.
    if (ded_light_t *def = lightDefByMobjState(mob->state))
    {
        if (!de::fequal(def->size, 0))
        {
            lum->setRadius(de::max(def->size, 32.f / (40 * lum->radiusFactor())));
        }

        if (!de::fequal(def->offset[1], 0))
        {
            lum->setZOffset(-texOrigin.y - def->offset[1]);
        }

        if (Vec3f(def->color) != Vec3f(0.0f))
        {
            lum->setColor(Vec3f(def->color));
        }

        lum->setLightmap(Lumobj::Side, lightmap(def->sides))
            .setLightmap(Lumobj::Down, lightmap(def->down))
            .setLightmap(Lumobj::Up,   lightmap(def->up));
    }

    // Translate to the mobj's origin in map space.
    lum->move(Vec3d(mob->origin));

    // Does the mobj need a Z origin offset?
    coord_t zOffset = -mob->floorClip - Mobj_BobOffset(*mob);
    if (!(mob->ddFlags & DDMF_NOFITBOTTOM) && impacted < 0)
    {
        // Raise the light out of the impacted surface.
        zOffset -= impacted;
    }
    lum->setZOffset(lum->zOffset() + zOffset);

    // Insert a copy of the temporary lumobj in the map and remember it's unique
    // index in the mobj (this'll allow a halo to be rendered).
    mob->lumIdx = subsec.sector().map().as<Map>().addLumobj(lum.release()).indexInMap();
}

void Mobj_AnimateHaloOcclussion(mobj_t &mob)
{
    for (dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        dbyte *haloFactor = &mob.haloFactors[i];

        // Set the high bit of halofactor if the light is clipped. This will
        // make P_Ticker diminish the factor to zero. Take the first step here
        // and now, though.
        if (mob.lumIdx == Lumobj::NoIndex || R_ViewerLumobjIsClipped(mob.lumIdx))
        {
            if (*haloFactor & 0x80)
            {
                dint f = (*haloFactor & 0x7f);  // - haloOccludeSpeed;
                if (f < 0) f = 0;
                *haloFactor = f;
            }
        }
        else
        {
            if (!(*haloFactor & 0x80))
            {
                dint f = (*haloFactor & 0x7f);  // + haloOccludeSpeed;
                if (f > 127) f = 127;
                *haloFactor = 0x80 | f;
            }
        }

        // Handle halofactor.
        dint f = *haloFactor & 0x7f;
        if (*haloFactor & 0x80)
        {
            // Going up.
            f += ::haloOccludeSpeed;
            if (f > 127)
                f = 127;
        }
        else
        {
            // Going down.
            f -= ::haloOccludeSpeed;
            if (f < 0)
                f = 0;
        }

        *haloFactor &= ~0x7f;
        *haloFactor |= f;
    }
}

dfloat Mobj_ShadowStrength(const mobj_t &mob)
{
    static const dfloat minSpriteAlphaLimit = .1f;

    // A shadow is not cast if the map-object is not linked in the map.
    if (!Mobj_HasSubsector(mob)) return 0;
    // ...or the current state is invalid or full-bright.
    if (!mob.state || (mob.state->flags & STF_FULLBRIGHT)) return 0;
    // ...or it won't be drawn at all.
    if (mob.ddFlags & DDMF_DONTDRAW) return 0;
    // ...or is "always lit" (?).
    if (mob.ddFlags & DDMF_ALWAYSLIT) return 0;

    // Evaluate the ambient light level at our map origin.
    const auto &subsec = Mobj_Subsector(mob).as<Subsector>();
    dfloat ambientLightLevel;
#if 0
    if (::useBias && subsec.sector().map().hasLightGrid())
    {
        ambientLightLevel = subsec.sector().map().lightGrid().evaluateIntensity(mob.origin);
    }
    else
#endif
    {
        ambientLightLevel = subsec.lightSourceIntensity();
    }
    Rend_ApplyLightAdaptation(ambientLightLevel);

    // Sprites have their own shadow strength factor.
    dfloat strength = .65f;  ///< Default.
    if (!::useModels || !Mobj_ModelDef(mob))
    {
        if (const Record *spriteRec = Mobj_SpritePtr(mob))
        {
            auto &matAnimator = *Rend_SpriteMaterialAnimator(*spriteRec); // world::Materials::get().materialPtr(sprite.viewMaterial(0)))
            matAnimator.prepare();  // Ensure we have up-to-date info.

            if (const TextureVariant *texture = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture)
            {
                const auto *aa = (const averagealpha_analysis_t *)texture->base().analysisDataPointer(res::Texture::AverageAlphaAnalysis);
                DE_ASSERT(aa);

                // We use an average which factors in the coverage ratio of
                // alpha:non-alpha pixels.
                /// @todo Constant weights could stand some tweaking...
                dfloat weightedSpriteAlpha = aa->alpha * (0.4f + (1 - aa->coverage) * 0.6f);

                // Almost entirely translucent sprite? => no shadow.
                if(weightedSpriteAlpha < minSpriteAlphaLimit) return 0;

                // Apply this factor.
                strength *= de::min(1.f, .2f + weightedSpriteAlpha);
            }
        }
    }

    // Factor in Mobj alpha.
    strength *= Mobj_Alpha(mob);

    /// @note This equation is the same as that used for fakeradio.
    return (0.6f - ambientLightLevel * 0.4f) * strength;
}

const Record *Mobj_SpritePtr(const mobj_t &mob)
{
    return res::Sprites::get().spritePtr(mob.sprite, mob.frame);
}

FrameModelDef *Mobj_ModelDef(const mobj_t &mo, FrameModelDef **retNextModef, float *retInter)
{
    // By default there are no models.
    if (retNextModef) *retNextModef = 0;
    if (retInter)     *retInter = -1;

    // On the client it is possible that we don't know the mobj's state.
    if (!mo.state) return 0;

    state_t &st = *mo.state;
    FrameModelDef *modef = App_Resources().modelDefForState(runtimeDefs.states.indexOf(&st), mo.selector);
    if (!modef) return 0; // No model available.

    dfloat interp = -1;

    // World time animation?
    bool worldTime = false;
    if (modef->flags & MFF_WORLD_TIME_ANIM)
    {
        dfloat duration = modef->interRange[0];
        dfloat offset   = modef->interRange[1];

        // Validate/modify the values.
        if (duration == 0) duration = 1;

        if (offset == -1)
        {
            offset = M_CycleIntoRange(MOBJ_TO_ID(&mo), duration);
        }

        interp = M_CycleIntoRange(App_World().time() / duration + offset, 1);
        worldTime = true;
    }
    else
    {
        // Calculate the currently applicable intermark.
        interp = 1.0f - (mo.tics - frameTimePos) / dfloat( st.tics );
    }

/*#if _DEBUG
    if (mo.dPlayer)
    {
        qDebug() << "itp:" << interp << " mot:" << mo.tics << " stt:" << st.tics;
    }
#endif*/

    // First find the modef for the interpoint. Intermark is 'stronger' than interrange.

    // Scan interlinks.
    while (modef->interNext && modef->interNext->interMark <= interp)
    {
        modef = modef->interNext;
    }

    if (!worldTime)
    {
        // Scale to the modeldef's interpolation range.
        interp = modef->interRange[0] + interp
               * (modef->interRange[1] - modef->interRange[0]);
    }

    // What would be the next model? Check interlinks first.
    if (retNextModef)
    {
        if (modef->interNext)
        {
            *retNextModef = modef->interNext;
        }
        else if (worldTime)
        {
            *retNextModef = App_Resources().modelDefForState(runtimeDefs.states.indexOf(&st), mo.selector);
        }
        else if (st.nextState > 0) // Check next state.
        {
            // Find the appropriate state based on interrange.
            state_t *it = &runtimeDefs.states[st.nextState];
            bool foundNext = false;
            if (modef->interRange[1] < 1)
            {
                // Current modef doesn't interpolate to the end, find the proper destination
                // modef (it isn't just the next one). Scan the states that follow (and
                // interlinks of each).
                bool stopScan = false;
                dint max = 20; // Let's not be here forever...
                while (!stopScan)
                {
                    if(!((  !App_Resources().modelDefForState(runtimeDefs.states.indexOf(it))
                          || App_Resources().modelDefForState(runtimeDefs.states.indexOf(it), mo.selector)->interRange[0] > 0)
                         && it->nextState > 0))
                    {
                        stopScan = true;
                    }
                    else
                    {
                        // Scan interlinks, then go to the next state.
                        FrameModelDef *mdit = App_Resources().modelDefForState(runtimeDefs.states.indexOf(it), mo.selector);
                        if (mdit && mdit->interNext)
                        {
                            for (;;)
                            {
                                mdit = mdit->interNext;
                                if (mdit)
                                {
                                    if (mdit->interRange[0] <= 0) // A new beginning?
                                    {
                                        *retNextModef = mdit;
                                        foundNext = true;
                                    }
                                }

                                if (!mdit || foundNext)
                                {
                                    break;
                                }
                            }
                        }

                        if (foundNext)
                        {
                            stopScan = true;
                        }
                        else
                        {
                            it = &runtimeDefs.states[it->nextState];
                        }
                    }

                    if (max-- <= 0)
                        stopScan = true;
                }
                // @todo What about max == -1? What should 'it' be then?
            }

            if (!foundNext)
            {
                *retNextModef = App_Resources().modelDefForState(runtimeDefs.states.indexOf(it), mo.selector);
            }
        }
    }

    if (retInter) *retInter = interp;

    return modef;
}

coord_t Mobj_ShadowRadius(const mobj_t &mobj)
{
    if (useModels)
    {
        if (FrameModelDef *modef = Mobj_ModelDef(mobj))
        {
            if (modef->shadowRadius > 0)
            {
                return modef->shadowRadius;
            }
        }
    }
    // Fall back to the visual radius.
    return Mobj_VisualRadius(mobj);
}

#endif // __CLIENT__

coord_t Mobj_ApproxPointDistance(const mobj_t *mob, const coord_t *point)
{
    if (!mob || !point) return 0;
    return M_ApproxDistance(point[2] - mob->origin[2],
                            M_ApproxDistance(point[0] - mob->origin[0],
                                             point[1] - mob->origin[1]));
}

coord_t Mobj_BobOffset(const mobj_t &mob)
{
    if (mob.ddFlags & DDMF_BOB)
    {
        return (sin(MOBJ_TO_ID(&mob) + App_World().time() / 1.8286 * 2 * PI) * 8);
    }
    return 0;
}

dfloat Mobj_Alpha(const mobj_t &mob)
{
    dfloat alpha = (mob.ddFlags & DDMF_BRIGHTSHADOW)? .80f :
                   (mob.ddFlags & DDMF_SHADOW      )? .33f :
                   (mob.ddFlags & DDMF_ALTSHADOW   )? .66f : 1;

    // The three highest bits of the selector are used for alpha.
    // 0 = opaque (alpha -1)
    // 1 = 1/8 transparent
    // 4 = 1/2 transparent
    // 7 = 7/8 transparent
    dint selAlpha = mob.selector >> DDMOBJ_SELECTOR_SHIFT;
    if (selAlpha & 0xe0)
    {
        alpha *= 1 - ((selAlpha & 0xe0) >> 5) / 8.0f;
    }
    else if (mob.translucency)
    {
        alpha *= 1 - mob.translucency * reciprocal255;
    }
    return alpha;
}

coord_t Mobj_VisualRadius(const mobj_t &mob)
{
#ifdef __CLIENT__
    // Is a model in effect?
    if (useModels)
    {
        if (FrameModelDef *modef = Mobj_ModelDef(mob))
        {
            return modef->visualRadius;
        }
    }

    // Is a sprite in effect?
    if (const Record *sprite = Mobj_SpritePtr(mob))
    {
        return Rend_VisualRadius(*sprite);
    }
#endif

    // Use the physical radius.
    return Mobj_Radius(mob);
}

D_CMD(InspectMobj)
{
    using world::Sector;
    
    DE_UNUSED(src);

    if (argc != 2)
    {
        LOG_SCR_NOTE("Usage: %s (mobj-id)") << argv[0];
        return true;
    }

    // Get the ID.
    const auto id = thid_t( String(argv[1]).toInt() );
    // Find the map-object.
    mobj_t *mob   = App_World().map().thinkers().mobjById(id);
    if (!mob)
    {
        LOG_MAP_ERROR("Mobj with id %i not found") << id;
        return false;
    }

    const char *mobType = "Mobj";
#ifdef __CLIENT__
    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(mob);
    if (info) mobType = "CLMOBJ";
#endif

    LOG_MAP_MSG("%s %i [%p] State:%s (%i)")
            << mobType << id << mob << Def_GetStateName(mob->state) << ::runtimeDefs.states.indexOf(mob->state);
    LOG_MAP_MSG("Type:%s (%i) Info:[%p] %s")
            << DED_Definitions()->getMobjName(mob->type) << mob->type << mob->info
            << (mob->info ? Stringf(" (%i)", runtimeDefs.mobjInfo.indexOf(mob->info)).c_str() : "");
    LOG_MAP_MSG("Tics:%i ddFlags:%08x") << mob->tics << mob->ddFlags;
#ifdef __CLIENT__
    if (info)
    {
        LOG_MAP_MSG("Cltime:%i (now:%i) Flags:%04x") << info->time << Timer_RealMilliseconds() << info->flags;
    }
#endif
    LOG_MAP_MSG("Flags:%08x Flags2:%08x Flags3:%08x") << mob->flags << mob->flags2 << mob->flags3;
    LOG_MAP_MSG("Height:%f Radius:%f") << mob->height << mob->radius;
    LOG_MAP_MSG("Angle:%x Pos:%s Mom:%s")
            << mob->angle
            << Vec3d(mob->origin).asText()
            << Vec3d(mob->mom).asText();
#ifdef __CLIENT__
    LOG_MAP_MSG("VisAngle:%x") << mob->visAngle;
#endif
    LOG_MAP_MSG("%sZ:%f %sZ:%f")
        << Sector::planeIdAsText(Sector::Floor  ).upperFirstChar() << mob->floorZ
        << Sector::planeIdAsText(Sector::Ceiling).upperFirstChar() << mob->ceilingZ;

    if (auto *subsec = Mobj_SubsectorPtr(*mob))
    {
        LOG_MAP_MSG("Sector:%i (%sZ:%f %sZ:%f)")
                << subsec->sector().indexInMap()
                << Sector::planeIdAsText(Sector::Floor  ) << subsec->sector().floor  ().height()
                << Sector::planeIdAsText(Sector::Ceiling) << subsec->sector().ceiling().height();
    }

    if (mob->onMobj)
    {
        LOG_MAP_MSG("onMobj:%i") << mob->onMobj->thinker.id;
    }

    return true;
}

void Mobj_ConsoleRegister()
{
    C_CMD("inspectmobj",    "i",    InspectMobj);

#ifdef __CLIENT__
    C_VAR_BYTE("rend-mobj-light-auto", &mobjAutoLights, 0, 0, 1);
#endif
}
