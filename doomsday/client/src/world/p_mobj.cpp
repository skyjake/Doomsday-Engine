/** @file p_mobj.cpp  World map objects.
 *
 * Various routines for moving mobjs, collision and Z checking.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_resource.h"
#include "de_misc.h"
#include "de_audio.h"

#include "def_main.h"

#include "world/worldsystem.h" // validCount
#include "world/thinkers.h"
#include "BspLeaf"
#include "ConvexSubspace"
#include "SectorCluster"

#ifdef __CLIENT__
#  include "Lumobj"
#  include "render/viewports.h"
#  include "render/rend_main.h"
#  include "render/rend_model.h"
#  include "render/billboard.h"

#  include "gl/gl_tex.h"
#endif

#include <de/Error>
#include <doomsday/world/mobjthinkerdata.h>
#include <cmath>

using namespace de;

static mobj_t *unusedMobjs;

/*
 * Console variables:
 */
int useSRVO                = 2; ///< @c 1= models only, @c 2= sprites + models
int useSRVOAngle           = 1;

#ifdef __CLIENT__
static byte mobjAutoLights = true;
#endif

/**
 * Called during map loading.
 */
void P_InitUnusedMobjList()
{
    // Any zone memory allocated for the mobjs will have already been purged.
    unusedMobjs = 0;
}

/**
 * All mobjs must be allocated through this routine. Part of the public API.
 */
mobj_t *P_MobjCreate(thinkfunc_t function, Vector3d const &origin, angle_t angle,
                     coord_t radius, coord_t height, int ddflags)
{
    if(!function)
        App_Error("P_MobjCreate: Think function invalid, cannot create mobj.");

#ifdef _DEBUG
    if(isClient)
    {
        LOG_VERBOSE("P_MobjCreate: Client creating mobj at %s")
            << origin.asText();
    }
#endif

    // Do we have any unused mobjs we can reuse?
    mobj_t *mo;
    if(unusedMobjs)
    {
        mo = unusedMobjs;
        unusedMobjs = unusedMobjs->sNext;
    }
    else
    {
        // No, we need to allocate another.
        mo = MobjThinker(Thinker::AllocateMemoryZone).take();
    }

    V3d_Set(mo->origin, origin.x, origin.y, origin.z);
    mo->angle = angle;
    mo->visAngle = mo->angle >> 16; // "angle-servo"; smooth actor turning.
    mo->radius = radius;
    mo->height = height;
    mo->ddFlags = ddflags;
    mo->lumIdx = -1;
    mo->thinker.function = function;
    Mobj_Map(*mo).thinkers().add(mo->thinker);

    return mo;
}

/**
 * All mobjs must be destroyed through this routine. Part of the public API.
 *
 * @note Does not actually destroy the mobj. Instead, mobj is marked as
 * awaiting removal (which occurs when its turn for thinking comes around).
 */
#undef Mobj_Destroy
DENG_EXTERN_C void Mobj_Destroy(mobj_t *mo)
{
#ifdef _DEBUG
    if(mo->ddFlags & DDMF_MISSILE)
    {
        LOG_AS("Mobj_Destroy");
        LOG_MAP_XVERBOSE("Destroying missile %i") << mo->thinker.id;
    }
#endif

    // Unlink from sector and block lists.
    Mobj_Unlink(mo);

    S_StopSound(0, mo);

    Mobj_Map(*mo).thinkers().remove(reinterpret_cast<thinker_t &>(*mo));
}

/**
 * Called when a mobj is actually removed (when it's thinking turn comes around).
 * The mobj is moved to the unused list to be reused later.
 */
void P_MobjRecycle(mobj_t* mo)
{
    // Release the private data.
    MobjThinker::zap(*mo);

    // The sector next link is used as the unused mobj list links.
    mo->sNext = unusedMobjs;
    unusedMobjs = mo;
}

dd_bool Mobj_IsSectorLinked(mobj_t *mo)
{
    return mo != 0 && mo->_bspLeaf != 0 && mo->sPrev != 0;
}

#undef Mobj_SetState
DENG_EXTERN_C void Mobj_SetState(mobj_t *mobj, int statenum)
{
    if(!mobj) return;

    state_t const *oldState = mobj->state;

    DENG_ASSERT(statenum >= 0 && statenum < defs.states.size());

    mobj->state  = &runtimeDefs.states[statenum];
    mobj->tics   = mobj->state->tics;
    mobj->sprite = mobj->state->sprite;
    mobj->frame  = mobj->state->frame;

    if(!(mobj->ddFlags & DDMF_REMOTE))
    {
        if(defs.states[statenum].execute)
            Con_Execute(CMDS_SCRIPT, defs.states[statenum].execute, true, false);
    }

    // Notify private data about the changed state.
    if(mobj->thinker.d == nullptr) Thinker_InitPrivateData(&mobj->thinker);
    if(MobjThinkerData *data = THINKER_DATA_MAYBE(mobj->thinker, MobjThinkerData))
    {
        data->stateChanged(oldState);
    }
}

Vector3d Mobj_Origin(mobj_t const &mobj)
{
    return Vector3d(mobj.origin);
}

Vector3d Mobj_Center(mobj_t &mobj)
{
    return Vector3d(mobj.origin[0], mobj.origin[1], mobj.origin[2] + mobj.height / 2);
}

dd_bool Mobj_SetOrigin(struct mobj_s *mo, coord_t x, coord_t y, coord_t z)
{
    if(!gx.MobjTryMoveXYZ)
    {
        return false;
    }
    return gx.MobjTryMoveXYZ(mo, x, y, z);
}

#undef Mobj_OriginSmoothed
DENG_EXTERN_C void Mobj_OriginSmoothed(mobj_t *mo, coord_t origin[3])
{
    if(!origin) return;

    V3d_Set(origin, 0, 0, 0);
    if(!mo) return;

    V3d_Copy(origin, mo->origin);

    // Apply a Short Range Visual Offset?
    if(useSRVO && mo->state && mo->tics >= 0)
    {
        double const mul = mo->tics / float( mo->state->tics );
        vec3d_t srvo;

        V3d_Copy(srvo, mo->srvo);
        V3d_Scale(srvo, mul);
        V3d_Sum(origin, origin, srvo);
    }

#ifdef __CLIENT__
    if(mo->dPlayer)
    {
        /// @todo What about splitscreen? We have smoothed origins for all local players.
        if(P_GetDDPlayerIdx(mo->dPlayer) == consolePlayer &&
           // $voodoodolls: Must be a real player to use the smoothed origin.
           mo->dPlayer->mo == mo)
        {
            viewdata_t const *vd = R_ViewData(consolePlayer);
            V3d_Set(origin, vd->current.origin.x, vd->current.origin.y, vd->current.origin.z);
        }
        // The client may have a Smoother for this object.
        else if(isClient)
        {
            Smoother_Evaluate(clients[P_GetDDPlayerIdx(mo->dPlayer)].smoother, origin);
        }
    }
#endif
}

de::Map &Mobj_Map(mobj_t const &mobj)
{
    return Thinker_Map(mobj.thinker);
}

bool Mobj_IsLinked(mobj_t const &mobj)
{
    return mobj._bspLeaf != 0;
}

BspLeaf &Mobj_BspLeafAtOrigin(mobj_t const &mobj)
{
    if(Mobj_IsLinked(mobj))
    {
        return *mobj._bspLeaf;
    }
    throw Error("Mobj_BspLeafAtOrigin", "Mobj is not yet linked");
}

bool Mobj_HasSubspace(mobj_t const &mobj)
{
    if(!Mobj_IsLinked(mobj)) return false;
    return Mobj_BspLeafAtOrigin(mobj).hasSubspace();
}

SectorCluster &Mobj_Cluster(mobj_t const &mobj)
{
    return Mobj_BspLeafAtOrigin(mobj).subspace().cluster();
}

SectorCluster *Mobj_ClusterPtr(mobj_t const &mobj)
{
    return Mobj_HasSubspace(mobj)? &Mobj_Cluster(mobj) : 0;
}

#undef Mobj_Sector
DENG_EXTERN_C Sector *Mobj_Sector(mobj_t const *mobj)
{
    if(!mobj) return 0;
    return Mobj_BspLeafAtOrigin(*mobj).sectorPtr();
}

void Mobj_SpawnParticleGen(mobj_t *source, ded_ptcgen_t const *def)
{
#ifdef __CLIENT__
    DENG2_ASSERT(def != 0 && source != 0);

    //if(!useParticles) return;

    Generator *gen = Mobj_Map(*source).newGenerator();
    if(!gen) return;

    /*LOG_INFO("SpawnPtcGen: %s/%i (src:%s typ:%s mo:%p)")
        << def->state << (def - defs.ptcgens) << defs.states[source->state-states].id
        << defs.mobjs[source->type].id << source;*/

    // Initialize the particle generator.
    gen->count = def->particles;
    // Size of source sector might determine count.
    if(def->flags & Generator::ScaledRate)
    {
        gen->spawnRateMultiplier = Mobj_BspLeafAtOrigin(*source).sectorPtr()->roughArea() / (128 * 128);
    }
    else
    {
        gen->spawnRateMultiplier = 1;
    }

    gen->configureFromDef(def);
    gen->source = source;
    gen->srcid = source->thinker.id;

    // Is there a need to pre-simulate?
    gen->presimulate(def->preSim);
#else
    DENG2_UNUSED2(source, def);
#endif
}

#undef Mobj_SpawnDamageParticleGen
DENG_EXTERN_C void Mobj_SpawnDamageParticleGen(mobj_t *mo, mobj_t *inflictor, int amount)
{
#ifdef __CLIENT__
    if(!mo || !inflictor || amount <= 0) return;

    // Are particles allowed?
    //if(!useParticles) return;

    ded_ptcgen_t const *def = Def_GetDamageGenerator(mo->type);
    if(def)
    {
        Generator *gen = Mobj_Map(*mo).newGenerator();
        if(!gen) return; // No more generators.

        gen->count = def->particles;
        gen->configureFromDef(def);
        gen->setUntriggered();

        gen->spawnRateMultiplier = de::max(amount, 1);

        // Calculate appropriate center coordinates.
        gen->originAtSpawn[VX] += FLT2FIX(mo->origin[VX]);
        gen->originAtSpawn[VY] += FLT2FIX(mo->origin[VY]);
        gen->originAtSpawn[VZ] += FLT2FIX(mo->origin[VZ] + mo->height / 2);

        // Calculate launch vector.
        vec3f_t vecDelta;
        V3f_Set(vecDelta, inflictor->origin[VX] - mo->origin[VX],
                inflictor->origin[VY] - mo->origin[VY],
                (inflictor->origin[VZ] - inflictor->height / 2) - (mo->origin[VZ] + mo->height / 2));

        vec3f_t vector;
        V3f_SetFixed(vector, gen->vector[VX], gen->vector[VY], gen->vector[VZ]);
        V3f_Sum(vector, vector, vecDelta);
        V3f_Normalize(vector);

        gen->vector[VX] = FLT2FIX(vector[VX]);
        gen->vector[VY] = FLT2FIX(vector[VY]);
        gen->vector[VZ] = FLT2FIX(vector[VZ]);

        // Is there a need to pre-simulate?
        gen->presimulate(def->preSim);
    }
#else
    DENG2_UNUSED3(mo, inflictor, amount);
#endif
}

#ifdef __CLIENT__

dd_bool Mobj_OriginBehindVisPlane(mobj_t *mo)
{
    if(!mo || !Mobj_HasSubspace(*mo))
        return false;
    SectorCluster &cluster = Mobj_Cluster(*mo);

    if(&cluster.floor() != &cluster.visFloor() &&
       mo->origin[VZ] < cluster.visFloor().heightSmoothed())
        return true;

    if(&cluster.ceiling() != &cluster.visCeiling() &&
       mo->origin[VZ] > cluster.visCeiling().heightSmoothed())
        return true;

    return false;
}

void Mobj_UnlinkLumobjs(mobj_t *mo)
{
    if(!mo) return;
    mo->lumIdx = Lumobj::NoIndex;
}

static ded_light_t *lightDefByMobjState(state_t const *state)
{
    if(state)
    {
        return runtimeDefs.stateInfo[runtimeDefs.states.indexOf(state)].light;
    }
    return 0;
}

static inline Texture *lightmap(de::Uri const *textureUri)
{
    return App_ResourceSystem().texture("Lightmaps", textureUri);
}

void Mobj_GenerateLumobjs(mobj_t *mo)
{
    if(!mo) return;

    Mobj_UnlinkLumobjs(mo);

    if(!Mobj_HasSubspace(*mo)) return;
    SectorCluster &cluster = Mobj_Cluster(*mo);

    if(!(((mo->state && (mo->state->flags & STF_FULLBRIGHT)) &&
         !(mo->ddFlags & DDMF_DONTDRAW)) ||
       (mo->ddFlags & DDMF_ALWAYSLIT)))
    {
        return;
    }

    // Are the automatically calculated light values for fullbright sprite frames in use?
    if(mo->state &&
       (!mobjAutoLights || (mo->state->flags & STF_NOAUTOLIGHT)) &&
       !runtimeDefs.stateInfo[runtimeDefs.states.indexOf(mo->state)].light)
    {
       return;
    }

    // If the mobj's origin is outside the BSP leaf it is linked within, then
    // this means it is outside the playable map (and no light should be emitted).
    /// @todo Optimize: Mobj_Link() should do this and flag the mobj accordingly.
    if(!Mobj_BspLeafAtOrigin(*mo).subspace().contains(mo->origin))
    {
        return;
    }

    Sprite *sprite = Mobj_Sprite(*mo);
    if(!sprite) return;

    // Always use the front rotation when determining light properties.
    if(!sprite->hasViewAngle(0)) return;
    SpriteViewAngle const &sprViewAngle = sprite->viewAngle(0);

    DENG2_ASSERT(sprViewAngle.material);
    MaterialAnimator &matAnimator = sprViewAngle.material->getAnimator(Rend_SpriteMaterialSpec());

    // Ensure we've up to date info about the material.
    matAnimator.prepare();

    TextureVariant *tex = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;
    if(!tex) return;  // Unloadable texture?
    Vector2i const &texOrigin = tex->base().origin();

    // Will the visual be allowed to go inside the floor?
    /// @todo Handle this as occlusion so that the halo fades smoothly.
    coord_t impacted = mo->origin[VZ] + -texOrigin.y - matAnimator.dimensions().y - cluster.visFloor().heightSmoothed();

    // If the floor is a visual plane then no light should be emitted.
    if(impacted < 0 && &cluster.visFloor() != &cluster.floor())
        return;

    // Attempt to generate luminous object from the sprite.
    QScopedPointer<Lumobj> lum(sprite->generateLumobj());
    if(lum.isNull()) return;

    // A light definition may override the (auto-calculated) defaults.
    if(ded_light_t *def = lightDefByMobjState(mo->state))
    {
        if(!de::fequal(def->size, 0))
        {
            lum->setRadius(de::max(def->size, 32.f / (40 * lum->radiusFactor())));
        }

        if(!de::fequal(def->offset[1], 0))
        {
            lum->setZOffset(-texOrigin.y - def->offset[1]);
        }

        if(Vector3f(def->color) != Vector3f(0, 0, 0))
        {
            lum->setColor(def->color);
        }

        lum->setLightmap(Lumobj::Side, lightmap(def->sides))
            .setLightmap(Lumobj::Down, lightmap(def->down))
            .setLightmap(Lumobj::Up,   lightmap(def->up));
    }

    // Translate to the mobj's origin in map space.
    lum->move(mo->origin);

    // Does the mobj need a Z origin offset?
    coord_t zOffset = -mo->floorClip - Mobj_BobOffset(mo);
    if(!(mo->ddFlags & DDMF_NOFITBOTTOM) && impacted < 0)
    {
        // Raise the light out of the impacted surface.
        zOffset -= impacted;
    }
    lum->setZOffset(lum->zOffset() + zOffset);

    // Insert a copy of the temporary lumobj in the map and remember it's unique
    // index in the mobj (this'll allow a halo to be rendered).
    mo->lumIdx = cluster.sector().map().addLumobj(*lum).indexInMap();
}

float Mobj_ShadowStrength(mobj_t *mo)
{
    if(!mo) return 0;

    float const minSpriteAlphaLimit = .1f;
    float ambientLightLevel, strength = .65f; ///< Default strength factor.

    // Is this mobj in a valid state for shadow casting?
    if(!mo->state) return 0;
    if(!Mobj_HasSubspace(*mo)) return 0;

    // Should this mobj even have a shadow?
    if((mo->state->flags & STF_FULLBRIGHT) ||
       (mo->ddFlags & DDMF_DONTDRAW) || (mo->ddFlags & DDMF_ALWAYSLIT))
        return 0;

    SectorCluster &cluster = Mobj_Cluster(*mo);

    // Sample the ambient light level at the mobj's position.
    Map &map = cluster.sector().map();
    if(useBias && map.hasLightGrid())
    {
        // Evaluate in the light grid.
        ambientLightLevel = map.lightGrid().evaluateIntensity(mo->origin);
    }
    else
    {
        ambientLightLevel = cluster.lightSourceIntensity();
    }
    Rend_ApplyLightAdaptation(ambientLightLevel);

    // Sprites have their own shadow strength factor.
    if(!useModels || !Mobj_ModelDef(*mo))
    {
        if(Sprite *sprite = Mobj_Sprite(*mo))
        {
            if(sprite->hasViewAngle(0))
            {
                SpriteViewAngle const &sprViewAngle = sprite->viewAngle(0);
                DENG2_ASSERT(sprViewAngle.material);
                MaterialAnimator &matAnimator = sprViewAngle.material->getAnimator(Rend_SpriteMaterialSpec());

                // Ensure we've up to date info about the material.
                matAnimator.prepare();

                TextureVariant const *texture = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;
                DENG2_ASSERT(texture);
                auto const *aa = (averagealpha_analysis_t const *)texture->base().analysisDataPointer(Texture::AverageAlphaAnalysis);
                DENG2_ASSERT(aa);

                // We use an average which factors in the coverage ratio
                // of alpha:non-alpha pixels.
                /// @todo Constant weights could stand some tweaking...
                float weightedSpriteAlpha = aa->alpha * (0.4f + (1 - aa->coverage) * 0.6f);

                // Almost entirely translucent sprite? => no shadow.
                if(weightedSpriteAlpha < minSpriteAlphaLimit) return 0;

                // Apply this factor.
                strength *= de::min(1.f, .2f + weightedSpriteAlpha);
            }
        }
    }

    // Factor in Mobj alpha.
    strength *= Mobj_Alpha(mo);

    /// @note This equation is the same as that used for fakeradio.
    return (0.6f - ambientLightLevel * 0.4f) * strength;
}

Sprite *Mobj_Sprite(mobj_t const &mo)
{
    return App_ResourceSystem().spritePtr(mo.sprite, mo.frame);
}

ModelDef *Mobj_ModelDef(mobj_t const &mo, ModelDef **retNextModef, float *retInter)
{
    ResourceSystem &resSys = App_ResourceSystem();

    // By default there are no models.
    if(retNextModef) *retNextModef = 0;
    if(retInter)     *retInter = -1;

    // On the client it is possible that we don't know the mobj's state.
    if(!mo.state) return 0;

    state_t &st = *mo.state;
    ModelDef *modef = resSys.modelDefForState(runtimeDefs.states.indexOf(&st), mo.selector);
    if(!modef) return 0; // No model available.

    float interp = -1;

    // World time animation?
    bool worldTime = false;
    if(modef->flags & MFF_WORLD_TIME_ANIM)
    {
        float duration = modef->interRange[0];
        float offset   = modef->interRange[1];

        // Validate/modify the values.
        if(duration == 0) duration = 1;

        if(offset == -1)
        {
            offset = M_CycleIntoRange(MOBJ_TO_ID(&mo), duration);
        }

        interp = M_CycleIntoRange(App_WorldSystem().time() / duration + offset, 1);
        worldTime = true;
    }
    else
    {
        // Calculate the currently applicable intermark.
        interp = 1.0f - (mo.tics - frameTimePos) / float( st.tics );
    }

/*#if _DEBUG
    if(mo.dPlayer)
    {
        qDebug() << "itp:" << interp << " mot:" << mo.tics << " stt:" << st.tics;
    }
#endif*/

    // First find the modef for the interpoint. Intermark is 'stronger' than interrange.

    // Scan interlinks.
    while(modef->interNext && modef->interNext->interMark <= interp)
    {
        modef = modef->interNext;
    }

    if(!worldTime)
    {
        // Scale to the modeldef's interpolation range.
        interp = modef->interRange[0] + interp
               * (modef->interRange[1] - modef->interRange[0]);
    }

    // What would be the next model? Check interlinks first.
    if(retNextModef)
    {
        if(modef->interNext)
        {
            *retNextModef = modef->interNext;
        }
        else if(worldTime)
        {
            *retNextModef = resSys.modelDefForState(runtimeDefs.states.indexOf(&st), mo.selector);
        }
        else if(st.nextState > 0) // Check next state.
        {
            // Find the appropriate state based on interrange.
            state_t *it = &runtimeDefs.states[st.nextState];
            bool foundNext = false;
            if(modef->interRange[1] < 1)
            {
                // Current modef doesn't interpolate to the end, find the proper destination
                // modef (it isn't just the next one). Scan the states that follow (and
                // interlinks of each).
                bool stopScan = false;
                int max = 20; // Let's not be here forever...
                while(!stopScan)
                {
                    if(!((!resSys.modelDefForState(runtimeDefs.states.indexOf(it)) ||
                          resSys.modelDefForState(runtimeDefs.states.indexOf(it), mo.selector)->interRange[0] > 0) &&
                         it->nextState > 0))
                    {
                        stopScan = true;
                    }
                    else
                    {
                        // Scan interlinks, then go to the next state.
                        ModelDef *mdit = resSys.modelDefForState(runtimeDefs.states.indexOf(it), mo.selector);
                        if(mdit && mdit->interNext)
                        {
                            forever
                            {
                                mdit = mdit->interNext;
                                if(mdit)
                                {
                                    if(mdit->interRange[0] <= 0) // A new beginning?
                                    {
                                        *retNextModef = mdit;
                                        foundNext = true;
                                    }
                                }

                                if(!mdit || foundNext)
                                {
                                    break;
                                }
                            }
                        }

                        if(foundNext)
                        {
                            stopScan = true;
                        }
                        else
                        {
                            it = &runtimeDefs.states[it->nextState];
                        }
                    }

                    if(max-- <= 0)
                        stopScan = true;
                }
                // @todo What about max == -1? What should 'it' be then?
            }

            if(!foundNext)
            {
                *retNextModef = resSys.modelDefForState(runtimeDefs.states.indexOf(it), mo.selector);
            }
        }
    }

    if(retInter) *retInter = interp;

    return modef;
}

#endif // __CLIENT__

#undef Mobj_AngleSmoothed
DENG_EXTERN_C angle_t Mobj_AngleSmoothed(mobj_t* mo)
{
    if(!mo) return 0;

#ifdef __CLIENT__
    if(mo->dPlayer)
    {
        /// @todo What about splitscreen? We have smoothed angles for all local players.
        if(P_GetDDPlayerIdx(mo->dPlayer) == consolePlayer &&
           // $voodoodolls: Must be a real player to use the smoothed angle.
           mo->dPlayer->mo == mo)
        {
            const viewdata_t* vd = R_ViewData(consolePlayer);
            return vd->current.angle();
        }
    }

    // Apply a Short Range Visual Offset?
    if(useSRVOAngle && !netGame && !playback)
    {
        return mo->visAngle << 16;
    }
#endif

    return mo->angle;
}

coord_t Mobj_ApproxPointDistance(mobj_t* mo, coord_t const* point)
{
    if(!mo || !point) return 0;
    return M_ApproxDistance(point[VZ] - mo->origin[VZ],
                            M_ApproxDistance(point[VX] - mo->origin[VX],
                                             point[VY] - mo->origin[VY]));
}

coord_t Mobj_BobOffset(mobj_t *mo)
{
    if(mo->ddFlags & DDMF_BOB)
    {
        return (sin(MOBJ_TO_ID(mo) + App_WorldSystem().time() / 1.8286 * 2 * PI) * 8);
    }
    return 0;
}

float Mobj_Alpha(mobj_t *mo)
{
    DENG_ASSERT(mo);

    float alpha = (mo->ddFlags & DDMF_BRIGHTSHADOW)? .80f :
                  (mo->ddFlags & DDMF_SHADOW      )? .33f :
                  (mo->ddFlags & DDMF_ALTSHADOW   )? .66f : 1;
    /**
     * The three highest bits of the selector are used for alpha.
     * 0 = opaque (alpha -1)
     * 1 = 1/8 transparent
     * 4 = 1/2 transparent
     * 7 = 7/8 transparent
     */
    int selAlpha = mo->selector >> DDMOBJ_SELECTOR_SHIFT;
    if(selAlpha & 0xe0)
    {
        alpha *= 1 - ((selAlpha & 0xe0) >> 5) / 8.0f;
    }
    else if(mo->translucency)
    {
        alpha *= 1 - mo->translucency * reciprocal255;
    }
    return alpha;
}

coord_t Mobj_Radius(mobj_t const &mobj)
{
    return mobj.radius;
}

#ifdef __CLIENT__
coord_t Mobj_ShadowRadius(mobj_t const &mobj)
{
    if(useModels)
    {
        if(ModelDef *modef = Mobj_ModelDef(mobj))
        {
            if(modef->shadowRadius > 0)
            {
                return modef->shadowRadius;
            }
        }
    }
    // Fall back to the visual radius.
    return Mobj_VisualRadius(mobj);
}
#endif

coord_t Mobj_VisualRadius(mobj_t const &mobj)
{
#ifdef __CLIENT__
    // Is a model in effect?
    if(useModels)
    {
        if(ModelDef *modef = Mobj_ModelDef(mobj))
        {
            return modef->visualRadius;
        }
    }

    // Is a sprite in effect?
    if(Sprite *sprite = Mobj_Sprite(mobj))
    {
        return sprite->visualRadius();
    }
#endif

    // Use the physical radius.
    return Mobj_Radius(mobj);
}

AABoxd Mobj_AABox(mobj_t const &mobj)
{
    Vector2d const origin = Mobj_Origin(mobj);
    ddouble const radius  = Mobj_Radius(mobj);
    return AABoxd(origin.x - radius, origin.y - radius,
                  origin.x + radius, origin.y + radius);
}

D_CMD(InspectMobj)
{
    DENG2_UNUSED(src);

    if(argc != 2)
    {
        LOG_SCR_NOTE("Usage: %s (mobj-id)") << argv[0];
        return true;
    }

    // Get the ID.
    thid_t id = strtol(argv[1], NULL, 10);

    // Find the mobj.
    mobj_t *mo = App_WorldSystem().map().thinkers().mobjById(id);
    if(!mo)
    {
        LOG_MAP_ERROR("Mobj with id %i not found") << id;
        return false;
    }

    char const *moType = "Mobj";
#ifdef __CLIENT__
    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(mo);
    if(info) moType = "CLMOBJ";
#endif

    LOG_MAP_MSG("%s %i [%p] State:%s (%i)")
            << moType << id << mo << Def_GetStateName(mo->state) << runtimeDefs.states.indexOf(mo->state);
    LOG_MAP_MSG("Type:%s (%i) Info:[%p] %s")
            << Def_GetMobjName(mo->type) << mo->type << mo->info
            << (mo->info? QString(" (%1)").arg(runtimeDefs.mobjInfo.indexOf(mo->info)) : "");
    LOG_MAP_MSG("Tics:%i ddFlags:%08x") << mo->tics << mo->ddFlags;
#ifdef __CLIENT__
    if(info)
    {
        LOG_MAP_MSG("Cltime:%i (now:%i) Flags:%04x") << info->time << Timer_RealMilliseconds() << info->flags;
    }
#endif
    LOG_MAP_MSG("Flags:%08x Flags2:%08x Flags3:%08x") << mo->flags << mo->flags2 << mo->flags3;
    LOG_MAP_MSG("Height:%f Radius:%f") << mo->height << mo->radius;
    LOG_MAP_MSG("Angle:%x Pos:%s Mom:%s")
            << mo->angle
            << Vector3d(mo->origin).asText()
            << Vector3d(mo->mom).asText();
    LOG_MAP_MSG("FloorZ:%f CeilingZ:%f") << mo->floorZ << mo->ceilingZ;
    if(SectorCluster *cluster = Mobj_ClusterPtr(*mo))
    {
        LOG_MAP_MSG("Sector:%i (FloorZ:%f CeilingZ:%f)")
                << cluster->sector().indexInMap()
                << cluster->floor().height()
                << cluster->ceiling().height();
    }
    if(mo->onMobj)
    {
        LOG_MAP_MSG("onMobj:%i") << mo->onMobj->thinker.id;
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
