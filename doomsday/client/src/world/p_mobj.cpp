/** @file p_mobj.cpp World map objects.
 *
 * Various routines for moving mobjs, collision and Z checking.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cmath>

#include <de/Error>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_resource.h"
#include "de_misc.h"
#include "de_audio.h"

#include "def_main.h"

#include "render/r_main.h" // validCount, viewport
#ifdef __CLIENT__
#  include "render/lumobj.h"
#  include "render/rend_main.h"
#  include "render/sprite.h"

#  include "gl/gl_tex.h"
#endif

#include "world/thinkers.h"
#include "BspLeaf"

using namespace de;

static mobj_t *unusedMobjs;

/*
 * Console variables:
 */
int useSRVO                = 2; ///< @c 1= models only, @c 2= sprites + models
int useSRVOAngle           = 1;
static byte mobjAutoLights = true;

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
mobj_t *P_MobjCreate(thinkfunc_t function, coord_t const pos[3], angle_t angle,
    coord_t radius, coord_t height, int ddflags)
{
    mobj_t *mo;

    if(!function)
        Con_Error("P_MobjCreateXYZ: Think function invalid, cannot create mobj.");

#ifdef _DEBUG
    if(isClient)
    {
        VERBOSE2( Con_Message("P_MobjCreate: Client creating mobj at [x:%f, y:%f, z:%f]", pos[VX], pos[VY], pos[VZ]) );
    }
#endif

    // Do we have any unused mobjs we can reuse?
    if(unusedMobjs)
    {
        mo = unusedMobjs;
        unusedMobjs = unusedMobjs->sNext;
        memset(mo, 0, MOBJ_SIZE);
    }
    else
    {
        // No, we need to allocate another.
        mo = (mobj_t *) Z_Calloc(MOBJ_SIZE, PU_MAP, NULL);
    }

    V3d_Copy(mo->origin, pos);
    mo->angle = angle;
    mo->visAngle = mo->angle >> 16; // "angle-servo"; smooth actor turning.
    mo->radius = radius;
    mo->height = height;
    mo->ddFlags = ddflags;
    mo->thinker.function = function;
    if(mo->thinker.function)
    {
        App_World().map().thinkers().add(mo->thinker);
    }

    return mo;
}

#undef P_MobjCreateXYZ
DENG_EXTERN_C mobj_t* P_MobjCreateXYZ(thinkfunc_t function, coord_t x, coord_t y, coord_t z,
    angle_t angle, coord_t radius, coord_t height, int ddflags)
{
    coord_t pos[3];
    V3d_Set(pos, x, y, z);
    return P_MobjCreate(function, pos, angle, radius, height, ddflags);
}

/**
 * All mobjs must be destroyed through this routine. Part of the public API.
 *
 * @note Does not actually destroy the mobj. Instead, mobj is marked as
 * awaiting removal (which occurs when its turn for thinking comes around).
 */
#undef P_MobjDestroy
DENG_EXTERN_C void P_MobjDestroy(mobj_t *mo)
{
#ifdef _DEBUG
    if(mo->ddFlags & DDMF_MISSILE)
    {
        VERBOSE2( Con_Message("P_MobjDestroy: Destroying missile %i.", mo->thinker.id) );
    }
#endif

    // Unlink from sector and block lists.
    P_MobjUnlink(mo);

    S_StopSound(0, mo);

    App_World().map().thinkers().remove(reinterpret_cast<thinker_t &>(*mo));
}

/**
 * Called when a mobj is actually removed (when it's thinking turn comes around).
 * The mobj is moved to the unused list to be reused later.
 */
void P_MobjRecycle(mobj_t* mo)
{
    // The sector next link is used as the unused mobj list links.
    mo->sNext = unusedMobjs;
    unusedMobjs = mo;
}

/**
 * Two links to update:
 * 1) The link to us from the previous node (sprev, always set) will
 *    be modified to point to the node following us.
 * 2) If there is a node following us, set its sprev pointer to point
 *    to the pointer that points back to it (our sprev, just modified).
 */
boolean Mobj_UnlinkFromSector(mobj_t *mo)
{
    if(!mo || !IS_SECTOR_LINKED(mo))
        return false;

    if((*mo->sPrev = mo->sNext))
        mo->sNext->sPrev = mo->sPrev;

    // Not linked any more.
    mo->sNext = 0;
    mo->sPrev = 0;

    return true;
}

/**
 * 'statenum' must be a valid state (not null!).
 */
#undef P_MobjSetState
DENG_EXTERN_C void P_MobjSetState(mobj_t *mobj, int statenum)
{
    state_t *st = states + statenum;
#ifdef __CLIENT__
    boolean spawning = (mobj->state == 0);
#endif

#ifdef DENG_DEBUG
    if(statenum < 0 || statenum >= defs.count.states.num)
        Con_Error("P_MobjSetState: statenum %i out of bounds.\n", statenum);
    /*
    if(mobj->ddFlags & DDMF_MISSILE)
    {
        Con_Message("P_MobjSetState: Missile %i going to state %i.", mobj->thinker.id, statenum);
    }
    */
#endif

    mobj->state = st;
    mobj->tics = st->tics;
    mobj->sprite = st->sprite;
    mobj->frame = st->frame;

#ifdef __CLIENT__
    // Check for a ptcgen trigger.
    for(ded_ptcgen_t *pg = statePtcGens[statenum]; pg; pg = pg->stateNext)
    {
        if(!(pg->flags & PGF_SPAWN_ONLY) || spawning)
        {
            // We are allowed to spawn the generator.
            P_SpawnMobjParticleGen(pg, mobj);
        }
    }
#endif

    if(!(mobj->ddFlags & DDMF_REMOTE))
    {
        if(defs.states[statenum].execute)
            Con_Execute(CMDS_SCRIPT, defs.states[statenum].execute, true, false);
    }
}

boolean Mobj_SetOrigin(struct mobj_s *mo, coord_t x, coord_t y, coord_t z)
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
        const double mul = mo->tics / (float) mo->state->tics;
        vec3d_t srvo;

        V3d_Copy(srvo, mo->srvo);
        V3d_Scale(srvo, mul);
        V3d_Sum(origin, origin, srvo);
    }

    if(mo->dPlayer)
    {
        /// @todo What about splitscreen? We have smoothed origins for all local players.
        if(P_GetDDPlayerIdx(mo->dPlayer) == consolePlayer &&
           // $voodoodolls: Must be a real player to use the smoothed origin.
           mo->dPlayer->mo == mo)
        {
            const viewdata_t* vd = R_ViewData(consolePlayer);
            V3d_Copy(origin, vd->current.origin);
        }
        // The client may have a Smoother for this object.
        else if(isClient)
        {
            Smoother_Evaluate(clients[P_GetDDPlayerIdx(mo->dPlayer)].smoother, origin);
        }
    }
}

#ifdef __CLIENT__

static modeldef_t *currentModelDefForMobj(mobj_t *mo)
{
    if(useModels)
    {
        modeldef_t *mf = 0, *nextmf = 0;
        Models_ModelForMobj(mo, &mf, &nextmf);
        return mf;
    }
    return 0;
}

boolean Mobj_OriginBehindVisPlane(mobj_t *mo)
{
    if(!mo || !mo->bspLeaf)
        return false;

    Plane const &visFloor = mo->bspLeaf->visFloor();
    if(&mo->bspLeaf->floor() != &visFloor && mo->origin[VZ] < visFloor.heightSmoothed())
        return true;

    Plane const &visCeiling = mo->bspLeaf->visCeiling();
    if(&mo->bspLeaf->ceiling() != &visCeiling && mo->origin[VZ] > visCeiling.heightSmoothed())
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
        return stateLights[state - states];
    }
    return 0;
}

static inline Texture *lightmap(uri_s const *textureUri)
{
    return R_FindTextureByResourceUri("Lightmaps", reinterpret_cast<de::Uri const *>(textureUri));
}

void Mobj_GenerateLumobjs(mobj_t *mo)
{
    if(!mo) return;

    Mobj_UnlinkLumobjs(mo);

    if(!mo->bspLeaf) return;

    if(!(((mo->state && (mo->state->flags & STF_FULLBRIGHT)) &&
         !(mo->ddFlags & DDMF_DONTDRAW)) ||
       (mo->ddFlags & DDMF_ALWAYSLIT)))
        return;

    // Are the automatically calculated light values for fullbright sprite frames in use?
    if(mo->state &&
       (!mobjAutoLights || (mo->state->flags & STF_NOAUTOLIGHT)) &&
       !stateLights[mo->state - states])
       return;

    // If the mobj's origin is outside the BSP leaf it is linked within, then
    // this means it is outside the playable map (and no light should be emitted).
    /// @todo Optimize: P_MobjLink() should do this and flag the mobj accordingly.
    if(!mo->bspLeaf->polyContains(mo->origin))
        return;

    spritedef_t *sprDef = R_SpriteDef(mo->sprite);
    if(!sprDef) return;

    spriteframe_t *sprFrame = SpriteDef_Frame(*sprDef, mo->frame);
    if(!sprFrame) return;

    // Always use the front rotation when determining light properties.
    Material *mat = SpriteFrame_Material(*sprFrame);
    if(!mat) return;

    MaterialSnapshot const &ms = mat->prepare(Rend_SpriteMaterialSpec());
    if(!ms.hasTexture(MTU_PRIMARY)) return; // Unloadable texture?
    Texture &tex = ms.texture(MTU_PRIMARY).generalCase();

    // Will the visual be allowed to go inside the floor?
    /// @todo Handle this as occlusion so that the halo fades smoothly.
    coord_t impacted = mo->origin[VZ] + -tex.origin().y - ms.height() - mo->bspLeaf->visFloorHeightSmoothed();

    // If the floor is a visual plane then no light should be emitted.
    if(impacted < 0 && &mo->bspLeaf->visFloor() != &mo->bspLeaf->floor())
        return;

    // Attempt to generate luminous object from the sprite.
    QScopedPointer<Lumobj> lum(SpriteDef_GenerateLumobj(*sprDef, mo->frame));
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
            lum->setZOffset(-tex.origin().y - def->offset[1]);
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

    // Does the mobj use a light scale?
    if(mo->ddFlags & DDMF_LIGHTSCALE)
    {
        float scale = 1.0f - ((mo->ddFlags & DDMF_LIGHTSCALE) >> DDMF_LIGHTSCALESHIFT) / 4.0f;
        lum->setRadius(lum->radius() * scale);
    }

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
    mo->lumIdx = mo->bspLeaf->map().addLumobj(*lum).indexInMap();
}

float Mobj_ShadowStrength(mobj_t *mo)
{
    if(!mo) return 0;

    float const minSpriteAlphaLimit = .1f;
    float ambientLightLevel, strength = .65f; ///< Default strength factor.

    // Is this mobj in a valid state for shadow casting?
    if(!mo->state || !mo->bspLeaf) return 0;

    // Should this mobj even have a shadow?
    if((mo->state->flags & STF_FULLBRIGHT) ||
       (mo->ddFlags & DDMF_DONTDRAW) || (mo->ddFlags & DDMF_ALWAYSLIT))
        return 0;

    Map &map = mo->bspLeaf->map();

    // Sample the ambient light level at the mobj's position.
    if(useBias && map.hasLightGrid())
    {
        // Evaluate in the light grid.
        ambientLightLevel = map.lightGrid().evaluateLightLevel(mo->origin);
    }
    else
    {
        ambientLightLevel = mo->bspLeaf->sector().lightLevel();
        Rend_ApplyLightAdaptation(ambientLightLevel);
    }

    // Sprites have their own shadow strength factor.
    if(!currentModelDefForMobj(mo))
    {
        if(Material *mat = R_MaterialForSprite(mo->sprite, mo->frame))
        {
            // Ensure we've prepared this.
            MaterialSnapshot const &ms = mat->prepare(Rend_SpriteMaterialSpec());

            averagealpha_analysis_t const *aa = (averagealpha_analysis_t const *)
                ms.texture(MTU_PRIMARY).generalCase().analysisDataPointer(Texture::AverageAlphaAnalysis);
            DENG_ASSERT(aa != 0);

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

    // Factor in Mobj alpha.
    strength *= Mobj_Alpha(mo);

    /// @note This equation is the same as that used for fakeradio.
    return (0.6f - ambientLightLevel * 0.4f) * strength;
}

#endif // __CLIENT__

#undef Mobj_AngleSmoothed
DENG_EXTERN_C angle_t Mobj_AngleSmoothed(mobj_t* mo)
{
    if(!mo) return 0;

    if(mo->dPlayer)
    {
        /// @todo What about splitscreen? We have smoothed angles for all local players.
        if(P_GetDDPlayerIdx(mo->dPlayer) == consolePlayer &&
           // $voodoodolls: Must be a real player to use the smoothed angle.
           mo->dPlayer->mo == mo)
        {
            const viewdata_t* vd = R_ViewData(consolePlayer);
            return vd->current.angle;
        }
    }

#ifdef __CLIENT__
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
        return (sin(MOBJ_TO_ID(mo) + App_World().time() / 1.8286 * 2 * PI) * 8);
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

coord_t Mobj_VisualRadius(mobj_t *mo)
{
#ifdef __CLIENT__

    // If models are being used, use the model's radius.
    if(modeldef_t *mf = currentModelDefForMobj(mo))
    {
        return mf->visualRadius;
    }

    // Use the sprite frame's width?
    if(Material *material = R_MaterialForSprite(mo->sprite, mo->frame))
    {
        MaterialSnapshot const &ms = material->prepare(Rend_SpriteMaterialSpec());
        return ms.width() / 2;
    }

#endif

    // Use the physical radius.
    return mo->radius;
}

void Mobj_ConsoleRegister(void)
{
#ifdef __CLIENT__
    C_VAR_BYTE("rend-mobj-light-auto", &mobjAutoLights, 0, 0, 1);
#endif
}

D_CMD(InspectMobj)
{
    DENG2_UNUSED(src);

    mobj_t* mo = 0;
    thid_t id = 0;
    char const *moType = "Mobj";
#ifdef __CLIENT__
    clmoinfo_t* info = 0;
#endif

    if(argc != 2)
    {
        Con_Printf("Usage: %s (mobj-id)\n", argv[0]);
        return true;
    }

    // Get the ID.
    id = strtol(argv[1], NULL, 10);

    // Find the mobj.
    mo = App_World().map().thinkers().mobjById(id);
    if(!mo)
    {
        Con_Printf("Mobj with id %i not found.\n", id);
        return false;
    }

#ifdef __CLIENT__
    info = ClMobj_GetInfo(mo);
    if(info) moType = "CLMOBJ";
#endif

    Con_Printf("%s %i [%p] State:%s (%i)\n", moType, id, mo, Def_GetStateName(mo->state), (int)(mo->state - states));
    Con_Printf("Type:%s (%i) Info:[%p]", Def_GetMobjName(mo->type), mo->type, mo->info);
    if(mo->info)
    {
        Con_Printf(" (%i)\n", (int)(mo->info - mobjInfo));
    }
    else
    {
        Con_Printf("\n");
    }
    Con_Printf("Tics:%i ddFlags:%08x\n", mo->tics, mo->ddFlags);
#ifdef __CLIENT__
    if(info)
    {
        Con_Printf("Cltime:%i (now:%i) Flags:%04x\n", info->time, Timer_RealMilliseconds(), info->flags);
    }
#endif
    Con_Printf("Flags:%08x Flags2:%08x Flags3:%08x\n", mo->flags, mo->flags2, mo->flags3);
    Con_Printf("Height:%f Radius:%f\n", mo->height, mo->radius);
    Con_Printf("Angle:%x Pos:(%f,%f,%f) Mom:(%f,%f,%f)\n",
               mo->angle,
               mo->origin[0], mo->origin[1], mo->origin[2],
               mo->mom[0], mo->mom[1], mo->mom[2]);
    Con_Printf("FloorZ:%f CeilingZ:%f\n", mo->floorZ, mo->ceilingZ);
    if(mo->bspLeaf && mo->bspLeaf->hasSector())
    {
        Con_Printf("Sector:%i (FloorZ:%f CeilingZ:%f)\n", mo->bspLeaf->sector().indexInMap(),
                   mo->bspLeaf->floorHeight(), mo->bspLeaf->ceilingHeight());
    }
    if(mo->onMobj)
    {
        Con_Printf("onMobj:%i\n", mo->onMobj->thinker.id);
    }

    return true;
}
