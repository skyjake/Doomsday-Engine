/** @file sv_pool.cpp  Delta Pools.
 *
 * Delta Pools use PU_MAP, which means all the memory allocated for them
 * is deallocated when the map changes. Sv_InitPools() is called in
 * R_SetupMap() to clear out all the old data.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "server/sv_pool.h"
#include "def_main.h"  // Def_SameStateSequence
#include "network/net_main.h"
#include "world/p_object.h"
#include "world/p_players.h"

#include <doomsday/world/sector.h>
#include <doomsday/world/thinkers.h>
#include <de/legacy/mathutil.h>
#include <de/legacy/timer.h>
#include <de/legacy/vector1.h>
#include <de/logbuffer.h>
#include <cmath>

using namespace de;

using world::Plane;
using world::Sector;
using world::Surface;

#define DEFAULT_DELTA_BASE_SCORE    ( 10000 )

#define REG_MOBJ_HASH_SIZE          ( 1024 )
#define REG_MOBJ_HASH_FUNCTION_MASK ( 0x3ff )

// Maximum difference in plane height where the absolute height doesn't need to be sent.
#define PLANE_SKIP_LIMIT            ( 40 )

struct reg_mobj_t
{
    reg_mobj_t *next;  ///< In the register hash.
    reg_mobj_t *prev;  ///< In the register hash.
    dt_mobj_t mo;      ///< The state of the mobj.
};

struct mobjhash_t
{
    reg_mobj_t *first, *last;
};

/**
 * One cregister_t holds the state of the entire world.
 */
struct cregister_t
{
    dint gametic;       ///< The time the register was last updated.
    dd_bool isInitial;  ///< @c true if *this* register contains a read-only copy of the initial state of the world.

    // The mobjs are stored in a hash for efficiency (ID is the key).
    mobjhash_t mobjs[REG_MOBJ_HASH_SIZE];

    dt_player_t ddPlayers[DDMAXPLAYERS];
    dt_sector_t *sectors;
    dt_side_t *sides;
    dt_poly_t *polyObjs;
};

void Sv_RegisterWorld(cregister_t *reg, dd_bool isInitial);
void Sv_NewDelta(void *deltaPtr, deltatype_t type, duint id);
dd_bool Sv_IsVoidDelta(const void *delta);
void Sv_PoolQueueClear(pool_t *pool);
void Sv_GenerateNewDeltas(cregister_t *reg, dint clientNumber, dd_bool doUpdate);

// The register contains the previous state of the world.
cregister_t worldRegister;

// The initial register is used when generating deltas for a new client.
cregister_t initialRegister;

static dfloat deltaBaseScores[NUM_DELTA_TYPES];

// Keep this zeroed out. Used if the register doesn't have data for
// the mobj being compared.
static ThinkerT<dt_mobj_t> dummyZeroMobj;

/**
 * Called once for each map, from R_SetupMap(). Initialize the world
 * register and drain all pools.
 */
void Sv_InitPools()
{
    Time startedAt;

    // Clients don't register anything.
    if (::isClient) return;

    LOG_AS("Sv_InitPools");

    // Set base priority scores for all the delta types.
    for (dint i = 0; i < NUM_DELTA_TYPES; ++i)
    {
        ::deltaBaseScores[i] = DEFAULT_DELTA_BASE_SCORE;
    }

    // Priorities for all deltas that will be sent out by the server.
    // No priorities need to be declared for obsolete delta types.
    ::deltaBaseScores[DT_MOBJ        ] = 1000;
    ::deltaBaseScores[DT_PLAYER      ] = 1000;
    ::deltaBaseScores[DT_SECTOR      ] = 2000;
    ::deltaBaseScores[DT_SIDE        ] = 800;
    ::deltaBaseScores[DT_POLY        ] = 2000;
    ::deltaBaseScores[DT_LUMP        ] = 0;
    ::deltaBaseScores[DT_SOUND       ] = 2000;
    ::deltaBaseScores[DT_MOBJ_SOUND  ] = 3000;
    ::deltaBaseScores[DT_SECTOR_SOUND] = 5000;
    ::deltaBaseScores[DT_SIDE_SOUND  ] = 5500;
    ::deltaBaseScores[DT_POLY_SOUND  ] = 5000;

    // Since the map has changed, PU_MAP memory has been freed.
    // Reset all pools (set numbers are kept, though).
    for (dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        pool_t &pool = *Sv_GetPool(i);

        pool.owner         = i;
        pool.resendDealer  = 1;
        de::zap(pool.hash);
        de::zap(pool.misHash);
        pool.queueSize     = 0;
        pool.allocatedSize = 0;
        pool.queue         = nullptr;

        pool.isFirst       = true;  // Set to @c false when a frame is sent.
    }

    // Store the current state of the world into both the registers.
    Sv_RegisterWorld(&::worldRegister, false);
    Sv_RegisterWorld(&::initialRegister, true);

    // How much time did we spend?
    LOG_MAP_VERBOSE("World registered in %.2f seconds") << startedAt.since();
}

/**
 * Called during server shutdown (when shutting down the engine).
 */
void Sv_ShutdownPools()
{
    // Nothing to do.
}

/**
 * Called when a client joins the game.
 */
void Sv_InitPoolForClient(duint clientNumber)
{
    DE_ASSERT(clientNumber < DDMAXPLAYERS);

    // Free everything that might exist in the pool.
    Sv_DrainPool(clientNumber);

    // Generate deltas by comparing against the initial state of the world.
    // The initial register remains unmodified.
    Sv_GenerateNewDeltas(&initialRegister, clientNumber, false);

    // No frames have yet been sent for this client.
    // The first frame is processed a bit more thoroughly than the others
    // (e.g. *all* sides are compared, not just a portion).
    Sv_GetPool(clientNumber)->isFirst = true;
}

/**
 * Returns a pointer to the delta pool associated with the given console number.
 */
pool_t *Sv_GetPool(duint consoleNumber)
{
    return &DD_Player(consoleNumber)->deltaPool();
}

/**
 * The hash function for the register mobj hash.
 */
duint Sv_RegisterHashFunction(thid_t id)
{
    return (duint) id & REG_MOBJ_HASH_FUNCTION_MASK;
}

/**
 * Returns a pointer to the register map-object, if it already exists.
 */
reg_mobj_t *Sv_RegisterFindMobj(cregister_t *reg, thid_t id)
{
    DE_ASSERT(reg);

    // See if there already is a register-mobj for this id.
    const mobjhash_t &hash = reg->mobjs[Sv_RegisterHashFunction(id)];
    for (reg_mobj_t *it = hash.first; it; it = it->next)
    {
        if (it->mo.thinker.id == id) return it;
    }

    return nullptr;  // Not found.
}

/**
 * Adds a new reg_mobj_t to the register's mobj hash.
 */
reg_mobj_t *Sv_RegisterAddMobj(cregister_t *reg, thid_t id)
{
    DE_ASSERT(reg);
    mobjhash_t &hash = reg->mobjs[Sv_RegisterHashFunction(id)];

    // Try to find an existing register-mobj.
    if (reg_mobj_t *newRegMo = Sv_RegisterFindMobj(reg, id))
        return newRegMo;

    // Allocate the new register-mobj.
    auto *newRegMo = (reg_mobj_t *) Z_Calloc(sizeof(reg_mobj_t), PU_MAP, 0);

    // Link it to the end of the hash list.
    if (hash.last)
    {
        hash.last->next = newRegMo;
        newRegMo->prev = hash.last;
    }
    hash.last = newRegMo;

    if (!hash.first)
    {
        hash.first = newRegMo;
    }

    return newRegMo;
}

/**
 * Removes a reg_mobj_t from the register's mobj hash.
 */
void Sv_RegisterRemoveMobj(cregister_t *reg, reg_mobj_t *regMo)
{
    DE_ASSERT(regMo);
    mobjhash_t &hash = reg->mobjs[Sv_RegisterHashFunction(regMo->mo.thinker.id)];

    // Update the first and last links.
    if (hash.last == regMo)
    {
        hash.last = regMo->prev;
    }
    if (hash.first == regMo)
    {
        hash.first = regMo->next;
    }

    // Link out of the list.
    if (regMo->next)
    {
        regMo->next->prev = regMo->prev;
    }
    if (regMo->prev)
    {
        regMo->prev->next = regMo->next;
    }

    // Destroy the register-mobj.
    Z_Free(regMo);
}

/**
 * @return @c DDMINFLOAT= @a mob is on the floor.
 *         @c DDMAXFLOAT= @a mob is touching the ceiling.
 *         Otherwise returns the actual world Z coordinate.
 */
dfloat Sv_GetMaxedMobjZ(const mobj_t *mob)
{
    // No maxing for now.
    /*if (mob->origin[VZ] == mob->floorZ)
    {
        return DDMINFLOAT;
    }
    if (mob->origin[VZ] + mob->height == mob->ceilingZ)
    {
        return DDMAXFLOAT;
    }*/
    return mob->origin[VZ];
}

/**
 * Store the state of the mobj into the register map-object.
 * Called at register init and after each delta generation cycle.
 */
void Sv_RegisterMobj(dt_mobj_t *reg, const mobj_t *mob)
{
    DE_ASSERT(reg && mob);
    // Just copy the data we need.
    reg->thinker.id   = mob->thinker.id;
    reg->type         = mob->type;
    reg->dPlayer      = mob->dPlayer;
    reg->_bspLeaf     = mob->_bspLeaf;
    reg->origin[0]    = mob->origin[0];
    reg->origin[1]    = mob->origin[1];
    reg->origin[2]    = Sv_GetMaxedMobjZ(mob);
    reg->floorZ       = mob->floorZ;
    reg->ceilingZ     = mob->ceilingZ;
    reg->mom[0]       = mob->mom[0];
    reg->mom[1]       = mob->mom[1];
    reg->mom[2]       = mob->mom[2];
    reg->angle        = mob->angle;
    reg->selector     = mob->selector;
    reg->state        = mob->state;
    reg->radius       = mob->radius;
    reg->height       = mob->height;
    reg->ddFlags      = mob->ddFlags;
    reg->flags        = mob->flags;
    reg->flags2       = mob->flags2;
    reg->flags3       = mob->flags3;
    reg->health       = mob->health;
    reg->floorClip    = mob->floorClip;
    reg->translucency = mob->translucency;
    reg->visTarget    = mob->visTarget;
}

/**
 * Reset the data of the registered mobj to reasonable defaults.
 * In effect, forces a resend of the zeroed entries as deltas.
 */
void Sv_RegisterResetMobj(dt_mobj_t *reg)
{
    DE_ASSERT(reg);

    reg->origin[0]    = DDMINFLOAT;
    reg->origin[1]    = DDMINFLOAT;
    reg->origin[2]    = -1000000;
    reg->angle        = 0;
    reg->type         = -1;
    reg->selector     = 0;
    reg->state        = 0;
    reg->radius       = -1;
    reg->height       = -1;
    reg->ddFlags      = 0;
    reg->flags        = 0;
    reg->flags2       = 0;
    reg->flags3       = 0;
    reg->health       = 0;
    reg->floorClip    = 0;
    reg->translucency = 0;
    reg->visTarget    = 0;
}

/**
 * Store the state of the player into the register-player.
 * Called at register init and after each delta generation cycle.
 */
void Sv_RegisterPlayer(dt_player_t *reg, duint number)
{
#define FMAKERGBA(r, g, b, a) \
    ( byte( 0xff * r ) + ( byte( 0xff * g ) << 8 ) + ( byte( 0xff * b ) << 16 ) + ( byte (0xff * a ) << 24 ) )

    DE_ASSERT(reg);
    DE_ASSERT(number < DDMAXPLAYERS);
    player_t *plr    = DD_Player(number);
    ddplayer_t *ddpl = &plr->publicData();

    reg->mobj          = (ddpl->mo ? ddpl->mo->thinker.id : 0);
    reg->forwardMove   = 0;
    reg->sideMove      = 0;
    reg->angle         = (ddpl->mo ? ddpl->mo->angle : 0);
    reg->turnDelta     = (ddpl->mo ? ddpl->mo->angle - ddpl->lastAngle : 0);
    reg->friction      = ddpl->mo && (gx.MobjFriction ? gx.MobjFriction(ddpl->mo) : DEFAULT_FRICTION);
    reg->extraLight    = ddpl->extraLight;
    reg->fixedColorMap = ddpl->fixedColorMap;
    if (ddpl->flags & DDPF_VIEW_FILTER)
    {
        reg->filter = FMAKERGBA(ddpl->filterColor[0],
                                ddpl->filterColor[1],
                                ddpl->filterColor[2],
                                ddpl->filterColor[3]);
    }
    else
    {
        reg->filter = 0;
    }
    reg->clYaw   = (ddpl->mo ? ddpl->mo->angle : 0);
    reg->clPitch = ddpl->lookDir;
    std::memcpy(reg->psp, ddpl->pSprites, sizeof(ddpsprite_t) * 2);

#undef FMAKERGBA
}

/**
 * Store the state of the sector into the register-sector.
 * Called at register init and after each delta generation.
 *
 * @param reg     The sector register to be initialized.
 * @param number  The world sector number to be registered.
 */
void Sv_RegisterSector(dt_sector_t *reg, dint number)
{
    DE_ASSERT(reg);
    Sector &sector = ServerWorld::get().map().sector(number);

    reg->lightLevel = sector.lightLevel();
    for (dint i = 0; i < 3; ++i)
    {
        reg->rgb[i] = sector.lightColor()[i];
    }

    // @todo $nplanes
    for (dint i = 0; i < 2; ++i) // number of planes in sector.
    {
        const Plane &plane = sector.plane(i);

        // Plane properties
        reg->planes[i].height = plane.height();
        reg->planes[i].target = plane.heightTarget();
        reg->planes[i].speed  = plane.speed();

        // Surface properties.
        const Surface &surface = plane.surface();

        const Vec3f &tintColor = surface.color();
        for (dint c = 0; c < 3; ++c)
        {
            reg->planes[i].surface.rgba[c] = tintColor[c];
        }
        reg->planes[i].surface.rgba[CA] = surface.opacity();
        reg->planes[i].surface.material = surface.materialPtr();
    }
}

/**
 * Store the state of the side into the register-side.
 * Called at register init and after each delta generation.
 */
void Sv_RegisterSide(dt_side_t *reg, dint number)
{
    DE_ASSERT(reg);

    auto *side = ServerWorld::get().map().sidePtr(number);

    if (side->hasSections())
    {
        reg->top   .material = side->top   ().materialPtr();
        reg->middle.material = side->middle().materialPtr();
        reg->bottom.material = side->bottom().materialPtr();

        for (dint c = 0; c < 3; ++c)
        {
            reg->middle.rgba[c] = side->middle().color()[c];
            reg->bottom.rgba[c] = side->bottom().color()[c];
            reg->top   .rgba[c] = side->top   ().color()[c];
        }

        // Only middle sections support blending.
        reg->middle.rgba[3]   = side->middle().opacity();
        reg->middle.blendMode = side->middle().blendMode();
    }

    reg->lineFlags = side->line().flags() & 0xff;
    reg->flags     = side->flags() & 0xff;
}

/**
 * Store the state of the polyobj into the register-poly.
 * Called at register init and after each delta generation.
 */
void Sv_RegisterPoly(dt_poly_t *reg, duint number)
{
    DE_ASSERT(reg);
    const Polyobj &pob = ServerWorld::get().map().polyobj(number);

    reg->dest[0]    = pob.dest[0];
    reg->dest[1]    = pob.dest[1];
    reg->speed      = pob.speed;
    reg->destAngle  = pob.destAngle;
    reg->angleSpeed = pob.angleSpeed;
}

/**
 * Returns @c true if the result is not void.
 */
dd_bool Sv_RegisterCompareMobj(cregister_t *reg, const mobj_t *s, mobjdelta_t *d)
{
    dint df;
    const dt_mobj_t *r = ::dummyZeroMobj;
    reg_mobj_t *regMo  = Sv_RegisterFindMobj(reg, s->thinker.id);
    if (regMo)
    {
        // Use the registered data.
        r  = &regMo->mo;
        df = 0;
    }
    else
    {
        // This didn't exist in the register, so it's a new mobj.
        df = MDFC_CREATE | MDF_EVERYTHING | MDFC_TYPE;
    }

    if (r->origin[0] != s->origin[0])
        df |= MDF_ORIGIN_X;
    if (r->origin[1] != s->origin[1])
        df |= MDF_ORIGIN_Y;
    if (r->origin[2] != Sv_GetMaxedMobjZ(s) || r->floorZ != s->floorZ || r->ceilingZ != s->ceilingZ)
    {
        df |= MDF_ORIGIN_Z;
        if (!(df & MDFC_CREATE) && s->origin[2] <= s->floorZ)
        {
            // It is currently on the floor. The client will place it on its
            // clientside floor and disregard the Z coordinate.
            df |= MDFC_ON_FLOOR;
        }
    }

    if (r->mom[0] != s->mom[0])
        df |= MDF_MOM_X;
    if (r->mom[1] != s->mom[1])
        df |= MDF_MOM_Y;
    if (r->mom[2] != s->mom[2])
        df |= MDF_MOM_Z;

    if (r->angle != s->angle)
        df |= MDF_ANGLE;
    if (r->selector != s->selector)
        df |= MDF_SELECTOR;
    if (r->translucency != s->translucency)
        df |= MDFC_TRANSLUCENCY;
    if (r->visTarget != s->visTarget)
        df |= MDFC_FADETARGET;
    if (r->type != s->type)
        df |= MDFC_TYPE;

    // Mobj state sent periodically, if the sequence keeps changing.
    if (regMo && !Def_SameStateSequence(s->state, r->state))
    {
        df |= MDF_STATE;

        if (s->state == nullptr)
        {
            // No valid comparison can be generated because the mobj is gone.
            return false;
        }
    }

    if (r->radius != s->radius)
        df |= MDF_RADIUS;
    if (r->height != s->height)
        df |= MDF_HEIGHT;
    if ((r->ddFlags & DDMF_PACK_MASK) != (s->ddFlags & DDMF_PACK_MASK) ||
       r->flags != s->flags || r->flags2 != s->flags2 || r->flags3 != s->flags3)
    {
        df |= MDF_FLAGS;
    }
    if (r->health != s->health)
        df |= MDF_HEALTH;
    if (r->floorClip != s->floorClip)
        df |= MDF_FLOORCLIP;

    if (df)
    {
        // Init the delta with current data.
        Sv_NewDelta(d, DT_MOBJ, s->thinker.id);
        Sv_RegisterMobj(&d->mo, s);
    }

    d->delta.flags = df;

    return !Sv_IsVoidDelta(d);
}

/**
 * Returns @c true if the result is not void.
 */
dd_bool Sv_RegisterComparePlayer(cregister_t *reg, duint number, playerdelta_t *d)
{
    DE_ASSERT(number < DDMAXPLAYERS);
    const dt_player_t *r = &reg->ddPlayers[number];
    dt_player_t *s       = &d->player;
    dint df = 0;

    // Init the delta with current data.
    Sv_NewDelta(d, DT_PLAYER, number);
    Sv_RegisterPlayer(&d->player, number);

    // Determine which data is different.
    if (r->mobj != s->mobj)
        df |= PDF_MOBJ;
    if (r->forwardMove != s->forwardMove)
        df |= PDF_FORWARDMOVE;
    if (r->sideMove != s->sideMove)
        df |= PDF_SIDEMOVE;
    if (r->turnDelta != s->turnDelta)
        df |= PDF_TURNDELTA;
    if (r->friction != s->friction)
        df |= PDF_FRICTION;
    if (r->extraLight != s->extraLight || r->fixedColorMap != s->fixedColorMap)
        df |= PDF_EXTRALIGHT;
    if (r->filter != s->filter)
        df |= PDF_FILTER;

    d->delta.flags = df;
    return !Sv_IsVoidDelta(d);
}

/**
 * @return  @c true if the result is not void.
 */
dd_bool Sv_RegisterCompareSector(cregister_t *reg, dint number, sectordelta_t *d, byte doUpdate)
{
    DE_ASSERT(reg && d);
    dt_sector_t *r  = &reg->sectors[number];
    const Sector &s = ServerWorld::get().map().sector(number);
    dint df = 0;

    // Determine which data is different.
    if (s.floor().surface().materialPtr() != r->planes[PLN_FLOOR].surface.material)
        df |= SDF_FLOOR_MATERIAL;
    if (s.ceiling().surface().materialPtr() != r->planes[PLN_CEILING].surface.material)
       df |= SDF_CEILING_MATERIAL;
    if (r->lightLevel != s.lightLevel())
        df |= SDF_LIGHT;
    if (r->rgb[0] != s.lightColor().x)
        df |= SDF_COLOR_RED;
    if (r->rgb[1] != s.lightColor().y)
        df |= SDF_COLOR_GREEN;
    if (r->rgb[2] != s.lightColor().z)
        df |= SDF_COLOR_BLUE;

    if (r->planes[PLN_FLOOR].surface.rgba[0] != s.floor().surface().color().x)
        df |= SDF_FLOOR_COLOR_RED;
    if (r->planes[PLN_FLOOR].surface.rgba[1] != s.floor().surface().color().y)
        df |= SDF_FLOOR_COLOR_GREEN;
    if (r->planes[PLN_FLOOR].surface.rgba[2] != s.floor().surface().color().z)
        df |= SDF_FLOOR_COLOR_BLUE;

    if (r->planes[PLN_CEILING].surface.rgba[0] != s.ceiling().surface().color().x)
        df |= SDF_CEIL_COLOR_RED;
    if (r->planes[PLN_CEILING].surface.rgba[1] != s.ceiling().surface().color().y)
        df |= SDF_CEIL_COLOR_GREEN;
    if (r->planes[PLN_CEILING].surface.rgba[2] != s.ceiling().surface().color().z)
        df |= SDF_CEIL_COLOR_BLUE;

    // The cases where an immediate change to a plane's height is needed:
    // 1) Plane is not moving, but the heights are different. This means
    //    the plane's height was changed unpredictably.
    // 2) Plane is moving, but there is a large difference in the heights.
    //    The clientside height should be fixed.

    // Should we make an immediate change in floor height?
    if (fequal(r->planes[PLN_FLOOR].speed, 0) && fequal(s.floor().speed(), 0))
    {
        if (!fequal(r->planes[PLN_FLOOR].height, s.floor().height()))
            df |= SDF_FLOOR_HEIGHT;
    }
    else
    {
        if (fabs(r->planes[PLN_FLOOR].height - s.floor().height()) > PLANE_SKIP_LIMIT)
            df |= SDF_FLOOR_HEIGHT;
    }

    // How about the ceiling?
    if (fequal(r->planes[PLN_CEILING].speed, 0) && fequal(s.ceiling().speed(), 0))
    {
        if (!fequal(r->planes[PLN_CEILING].height, s.ceiling().height()))
            df |= SDF_CEILING_HEIGHT;
    }
    else
    {
        if (fabs(r->planes[PLN_CEILING].height - s.ceiling().height()) > PLANE_SKIP_LIMIT)
            df |= SDF_CEILING_HEIGHT;
    }

    // Check planes, too.
    if (!fequal(r->planes[PLN_FLOOR].target, s.floor().heightTarget()))
    {
        // Target and speed are always sent together.
        df |= SDF_FLOOR_TARGET | SDF_FLOOR_SPEED;
    }
    if (!fequal(r->planes[PLN_FLOOR].speed, s.floor().speed()))
    {
        // Target and speed are always sent together.
        df |= SDF_FLOOR_SPEED | SDF_FLOOR_TARGET;
    }
    if (!fequal(r->planes[PLN_CEILING].target, s.ceiling().heightTarget()))
    {
        // Target and speed are always sent together.
        df |= SDF_CEILING_TARGET | SDF_CEILING_SPEED;
    }
    if (!fequal(r->planes[PLN_CEILING].speed, s.ceiling().speed()))
    {
        // Target and speed are always sent together.
        df |= SDF_CEILING_SPEED | SDF_CEILING_TARGET;
    }

#ifdef DE_DEBUG
    if (df & (SDF_CEILING_HEIGHT | SDF_CEILING_SPEED | SDF_CEILING_TARGET))
    {
        LOGDEV_NET_XVERBOSE("Sector %i: ceiling state change noted (target = %f)",
                            number << s.ceiling().heightTarget());
    }
#endif

    // Only do a delta when something changes.
    if (df)
    {
        // Init the delta with current data.
        Sv_NewDelta(d, DT_SECTOR, number);
        Sv_RegisterSector(&d->sector, number);

        if (doUpdate)
        {
            Sv_RegisterSector(r, number);
        }
    }

    if (doUpdate)
    {
        // The plane heights should be tracked regardless of the
        // change flags.
        r->planes[PLN_FLOOR  ].height = s.floor().height();
        r->planes[PLN_CEILING].height = s.ceiling().height();
    }

    d->delta.flags = df;
    return !Sv_IsVoidDelta(d);
}

/**
 * @return  @c true= the result is not void.
 */
dd_bool Sv_RegisterCompareSide(cregister_t *reg, duint number, sidedelta_t *d, byte doUpdate)
{
    DE_ASSERT(reg/* && d*/);
    const auto *side = ServerWorld::get().map().sidePtr(number);
    dt_side_t * r    = &reg->sides[number];

    byte lineFlags = side->line().flags() & 0xff;
    byte sideFlags = side->flags() & 0xff;
    dint df = 0;

    if (side->hasSections())
    {
        if (!side->top().hasFixMaterial() && r->top.material != side->top().materialPtr())
        {
            df |= SIDF_TOP_MATERIAL;
            if (doUpdate)
                r->top.material = side->top().materialPtr();
        }

        if (!side->middle().hasFixMaterial() && r->middle.material != side->middle().materialPtr())
        {
            df |= SIDF_MID_MATERIAL;
            if (doUpdate)
                r->middle.material = side->middle().materialPtr();
        }

        if (!side->bottom().hasFixMaterial() && r->bottom.material != side->bottom().materialPtr())
        {
            df |= SIDF_BOTTOM_MATERIAL;
            if (doUpdate)
                r->bottom.material = side->bottom().materialPtr();
        }

        if (r->top.rgba[0] != side->top().color().x)
        {
            df |= SIDF_TOP_COLOR_RED;
            if (doUpdate)
                r->top.rgba[0] = side->top().color().x;
        }

        if (r->top.rgba[1] != side->top().color().y)
        {
            df |= SIDF_TOP_COLOR_GREEN;
            if (doUpdate)
                r->top.rgba[1] = side->top().color().y;
        }

        if (r->top.rgba[2] != side->top().color().z)
        {
            df |= SIDF_TOP_COLOR_BLUE;
            if (doUpdate)
                r->top.rgba[3] = side->top().color().z;
        }

        if (r->middle.rgba[0] != side->middle().color().x)
        {
            df |= SIDF_MID_COLOR_RED;
            if (doUpdate)
                r->middle.rgba[0] = side->middle().color().x;
        }

        if (r->middle.rgba[1] != side->middle().color().y)
        {
            df |= SIDF_MID_COLOR_GREEN;
            if (doUpdate)
                r->middle.rgba[1] = side->middle().color().y;
        }

        if (r->middle.rgba[2] != side->middle().color().z)
        {
            df |= SIDF_MID_COLOR_BLUE;
            if (doUpdate)
                r->middle.rgba[3] = side->middle().color().z;
        }

        if (r->middle.rgba[3] != side->middle().opacity())
        {
            df |= SIDF_MID_COLOR_ALPHA;
            if (doUpdate)
                r->middle.rgba[3] = side->middle().opacity();
        }

        if (r->bottom.rgba[0] != side->bottom().color().x)
        {
            df |= SIDF_BOTTOM_COLOR_RED;
            if (doUpdate)
                r->bottom.rgba[0] = side->bottom().color().x;
        }

        if (r->bottom.rgba[1] != side->bottom().color().y)
        {
            df |= SIDF_BOTTOM_COLOR_GREEN;
            if (doUpdate)
                r->bottom.rgba[1] = side->bottom().color().y;
        }

        if (r->bottom.rgba[2] != side->bottom().color().z)
        {
            df |= SIDF_BOTTOM_COLOR_BLUE;
            if (doUpdate)
                r->bottom.rgba[3] = side->bottom().color().z;
        }

        if (r->middle.blendMode != side->middle().blendMode())
        {
            df |= SIDF_MID_BLENDMODE;
            if (doUpdate)
                r->middle.blendMode = side->middle().blendMode();
        }
    }

    if (r->lineFlags != lineFlags)
    {
        df |= SIDF_LINE_FLAGS;
        if (doUpdate)
            r->lineFlags = lineFlags;
    }

    if (r->flags != sideFlags)
    {
        df |= SIDF_FLAGS;
        if (doUpdate)
            r->flags = sideFlags;
    }

    // Was there any change?
    if (df)
    {
        // This happens quite rarely.
        // Init the delta with current data.
        Sv_NewDelta(d, DT_SIDE, number);
        Sv_RegisterSide(&d->side, number);
    }

    d->delta.flags = df;
    return !Sv_IsVoidDelta(d);
}

/**
 * @return  @c true if the result is not void.
 */
dd_bool Sv_RegisterComparePoly(cregister_t *reg, dint number, polydelta_t *d)
{
    DE_ASSERT(reg/* && d*/);
    const dt_poly_t *r = &reg->polyObjs[number];
    dt_poly_t *s       = &d->po;
    dint df = 0;

    // Init the delta with current data.
    Sv_NewDelta(d, DT_POLY, number);
    Sv_RegisterPoly(&d->po, number);

    // What is different?
    if (r->dest[VX] != s->dest[VX])
        df |= PODF_DEST_X;
    if (r->dest[VY] != s->dest[VY])
        df |= PODF_DEST_Y;
    if (r->speed != s->speed)
        df |= PODF_SPEED;
    if (r->destAngle != s->destAngle)
        df |= PODF_DEST_ANGLE;
    if (r->angleSpeed != s->angleSpeed)
        df |= PODF_ANGSPEED;

    d->delta.flags = df;
    return !Sv_IsVoidDelta(d);
}

/**
 * Returns @c true if the map-object can be excluded from delta processing.
 */
dd_bool Sv_IsMobjIgnored(const mobj_t &mob)
{
    return (mob.ddFlags & DDMF_LOCAL) != 0;
}

/**
 * Returns @c true if the player can be excluded from delta processing.
 */
dd_bool Sv_IsPlayerIgnored(dint plrNum)
{
    DE_ASSERT(plrNum >= 0 && plrNum < DDMAXPLAYERS);
    return !DD_Player(plrNum)->publicData().inGame;
}

/**
 * Initialize the register with the current state of the world.
 *
 * The arrays are allocated and the data is copied, nothing else is done.
 *
 * An initial register doesn't contain any mobjs. When new clients enter, they know
 * nothing about any mobjs. If the mobjs were included in the initial register, clients
 * wouldn't receive much info from mobjs that haven't moved since the beginning.
 */
void Sv_RegisterWorld(cregister_t *reg, dd_bool isInitial)
{
    DE_ASSERT(reg);

    world::Map &map = ServerWorld::get().map();

    de::zapPtr(reg);
    reg->gametic = SECONDS_TO_TICKS(gameTime);

    // Is this the initial state?
    reg->isInitial = isInitial;

    // Init sectors.
    reg->sectors = (dt_sector_t *) Z_Calloc(sizeof(*reg->sectors) * map.sectorCount(), PU_MAP, 0);
    for (dint i = 0; i < map.sectorCount(); ++i)
    {
        Sv_RegisterSector(&reg->sectors[i], i);
    }

    // Init sides.
    reg->sides = (dt_side_t *) Z_Calloc(sizeof(*reg->sides) * map.sideCount(), PU_MAP, 0);
    for (dint i = 0; i < map.sideCount(); ++i)
    {
        Sv_RegisterSide(&reg->sides[i], i);
    }

    // Init polyobjs.
    dint numPolyobjs = map.polyobjCount();
    if (numPolyobjs)
    {
        reg->polyObjs = (dt_poly_t *) Z_Calloc(sizeof(*reg->polyObjs) * map.polyobjCount(), PU_MAP, 0);
        for (dint i = 0; i < numPolyobjs; ++i)
        {
            Sv_RegisterPoly(&reg->polyObjs[i], i);
        }
    }
    else
    {
        reg->polyObjs = nullptr;
    }
}

/**
 * Update the pool owner's info.
 */
void Sv_UpdateOwnerInfo(pool_t *pool)
{
    DE_ASSERT(pool);
    player_t *plr     = DD_Player(pool->owner);
    ownerinfo_t *info = &pool->ownerInfo;

    de::zapPtr(info);

    // Pointer to the owner's pool.
    info->pool = pool;

    if (plr->publicData().mo)
    {
        mobj_t *mob = plr->publicData().mo;

        V3d_Copy(info->origin, mob->origin);
        info->angle = mob->angle;
        info->speed = M_ApproxDistance(mob->mom[0], mob->mom[1]);
    }

    // The acknowledgement threshold is a multiple of the average ack time of the
    // client. If an unacked delta is not acked within the threshold, it'll be
    // re-included in the ratings.
    info->ackThreshold = 0; //Net_GetAckThreshold(pool->owner);
}

/**
 * @return  A timestamp that is used to track how old deltas are.
 */
uint Sv_GetTimeStamp()
{
    return Timer_RealMilliseconds();
}

/**
 * Initialize a new delta.
 */
void Sv_NewDelta(void *deltaPtr, deltatype_t type, uint id)
{
    if (!deltaPtr) return;

    delta_t *delta = (delta_t *) deltaPtr;
    /// @note: This only clears the common delta_t part, not the extra data.
    de::zapPtr(delta);

    delta->id = id;
    delta->type = type;
    delta->state = DELTA_NEW;
    delta->timeStamp = Sv_GetTimeStamp();
}

/**
 * @return  @c true, if the delta contains no information.
 */
dd_bool Sv_IsVoidDelta(const void *delta)
{
    return ((const delta_t *) delta)->flags == 0;
}

/**
 * @return  @c true, if the delta is a Sound delta.
 */
dd_bool Sv_IsSoundDelta(const void *delta)
{
    const delta_t *d = (const delta_t *) delta;

    return (d->type == DT_SOUND ||
            d->type == DT_MOBJ_SOUND ||
            d->type == DT_SECTOR_SOUND ||
            d->type == DT_SIDE_SOUND ||
            d->type == DT_POLY_SOUND);
}

/**
 * @return  @c true, if the delta is a Start Sound delta.
 */
dd_bool Sv_IsStartSoundDelta(const void *delta)
{
    const sounddelta_t *d = (const sounddelta_t *) delta;

    return Sv_IsSoundDelta(delta) &&
        ((d->delta.flags & SNDDF_VOLUME) && d->volume > 0);
}

/**
 * @return  @c true, if the delta is Stop Sound delta.
 */
dd_bool Sv_IsStopSoundDelta(const void *delta)
{
    const sounddelta_t *d = (const sounddelta_t *) delta;

    return Sv_IsSoundDelta(delta) &&
        ((d->delta.flags & SNDDF_VOLUME) && d->volume <= 0);
}

/**
 * @return  @c true, if the delta is a Null Mobj delta.
 */
dd_bool Sv_IsNullMobjDelta(const void *delta)
{
    return ((const delta_t *) delta)->type == DT_MOBJ &&
        (((const delta_t *) delta)->flags & MDFC_NULL);
}

/**
 * @return  @c true, if the delta is a Create Mobj delta.
 */
dd_bool Sv_IsCreateMobjDelta(const void *delta)
{
    return ((const delta_t *) delta)->type == DT_MOBJ &&
        (((const delta_t *) delta)->flags & MDFC_CREATE);
}

/**
 * @return  @c true, if the deltas refer to the same object.
 */
dd_bool Sv_IsSameDelta(const void *delta1, const void *delta2)
{
    const delta_t *a = (const delta_t *) delta1, *b = (const delta_t *) delta2;

    return (a->type == b->type) && (a->id == b->id);
}

/**
 * Makes a copy of the delta.
 */
void* Sv_CopyDelta(void* deltaPtr)
{
    void*               newDelta;
    delta_t*            delta = (delta_t *) deltaPtr;
    size_t              size =
        ( delta->type == DT_MOBJ ?         sizeof(mobjdelta_t)
        : delta->type == DT_PLAYER ?       sizeof(playerdelta_t)
        : delta->type == DT_SECTOR ?       sizeof(sectordelta_t)
        : delta->type == DT_SIDE ?         sizeof(sidedelta_t)
        : delta->type == DT_POLY ?         sizeof(polydelta_t)
        : delta->type == DT_SOUND ?        sizeof(sounddelta_t)
        : delta->type == DT_MOBJ_SOUND ?   sizeof(sounddelta_t)
        : delta->type == DT_SECTOR_SOUND ? sizeof(sounddelta_t)
        : delta->type == DT_SIDE_SOUND ?   sizeof(sounddelta_t)
        : delta->type == DT_POLY_SOUND ?   sizeof(sounddelta_t)
         /* : delta->type == DT_LUMP?   sizeof(lumpdelta_t) */
        : 0);

    if (size == 0)
    {
        App_Error("Sv_CopyDelta: Unknown delta type %i.\n", delta->type);
    }

    newDelta = Z_Malloc(size, PU_MAP, 0);
    memcpy(newDelta, deltaPtr, size);
    return newDelta;
}

/**
 * Subtracts the contents of the second delta from the first delta.
 * Subtracting means that if a given flag is defined for both 1 and 2,
 * the flag for 1 is cleared (2 overrides 1). The result is that the
 * deltas can be applied in any order and the result is still correct.
 *
 * 1 and 2 must refer to the same entity!
 */
void Sv_SubtractDelta(void* deltaPtr1, const void* deltaPtr2)
{
    delta_t*            delta = (delta_t *) deltaPtr1;
    const delta_t*      sub = (const delta_t *) deltaPtr2;

#ifdef _DEBUG
    if (!Sv_IsSameDelta(delta, sub))
    {
        App_Error("Sv_SubtractDelta: Not the same!\n");
    }
#endif

    if (Sv_IsNullMobjDelta(sub))
    {
        // Null deltas kill everything.
        delta->flags = 0;
    }
    else
    {
        // Clear the common flags.
        delta->flags &= ~(delta->flags & sub->flags);
    }
}

/**
 * Applies the data in the source delta to the destination delta.
 * Both must be in the NEW state. Handles all types of deltas.
 */
void Sv_ApplyDeltaData(void *destDelta, const void *srcDelta)
{
    const delta_t *src = (const delta_t *) srcDelta;
    delta_t *dest      = (delta_t *) destDelta;
    int sf = src->flags;

    if (src->type == DT_MOBJ)
    {
        const dt_mobj_t *s = &((const mobjdelta_t *) src)->mo;
        dt_mobj_t *d       = &((mobjdelta_t *) dest)->mo;

        // *Always* set the player pointer.
        d->dPlayer = s->dPlayer;

        if (sf & (MDF_ORIGIN_X | MDF_ORIGIN_Y))
            d->_bspLeaf = s->_bspLeaf;
        if (sf & MDF_ORIGIN_X)
            d->origin[VX] = s->origin[VX];
        if (sf & MDF_ORIGIN_Y)
            d->origin[VY] = s->origin[VY];
        if (sf & MDF_ORIGIN_Z)
            d->origin[VZ] = s->origin[VZ];
        if (sf & MDF_MOM_X)
            d->mom[MX] = s->mom[MX];
        if (sf & MDF_MOM_Y)
            d->mom[MY] = s->mom[MY];
        if (sf & MDF_MOM_Z)
            d->mom[MZ] = s->mom[MZ];
        if (sf & MDF_ANGLE)
            d->angle = s->angle;
        if (sf & MDF_SELECTOR)
            d->selector = s->selector;
        if (sf & MDF_STATE)
        {
            d->state = s->state;
            d->tics = (s->state ? s->state->tics : 0);
        }
        if (sf & MDF_RADIUS)
            d->radius = s->radius;
        if (sf & MDF_HEIGHT)
            d->height = s->height;
        if (sf & MDF_FLAGS)
        {
            d->ddFlags = s->ddFlags;
            d->flags   = s->flags;
            d->flags2  = s->flags2;
            d->flags3  = s->flags3;
        }
        if (sf & MDF_FLOORCLIP)
            d->floorClip = s->floorClip;
        if (sf & MDFC_TRANSLUCENCY)
            d->translucency = s->translucency;
        if (sf & MDFC_FADETARGET)
            d->visTarget = s->visTarget;
    }
    else if (src->type == DT_PLAYER)
    {
        const dt_player_t*  s = &((const playerdelta_t *) src)->player;
        dt_player_t*        d = &((playerdelta_t *) dest)->player;

        if (sf & PDF_MOBJ)
            d->mobj = s->mobj;
        if (sf & PDF_FORWARDMOVE)
            d->forwardMove = s->forwardMove;
        if (sf & PDF_SIDEMOVE)
            d->sideMove = s->sideMove;
        /*if (sf & PDF_ANGLE)
            d->angle = s->angle;*/
        if (sf & PDF_TURNDELTA)
            d->turnDelta = s->turnDelta;
        if (sf & PDF_FRICTION)
            d->friction = s->friction;
        if (sf & PDF_EXTRALIGHT)
        {
            d->extraLight = s->extraLight;
            d->fixedColorMap = s->fixedColorMap;
        }
        if (sf & PDF_FILTER)
            d->filter = s->filter;
        if (sf & PDF_PSPRITES)
        {
            uint                i;

            for (i = 0; i < 2; ++i)
            {
                int                 off = 16 + i * 8;

                if (sf & (PSDF_STATEPTR << off))
                {
                    d->psp[i].statePtr = s->psp[i].statePtr;
                    d->psp[i].tics =
                        (s->psp[i].statePtr ? s->psp[i].statePtr->tics : 0);
                }
                /*if (sf & (PSDF_LIGHT << off))
                    d->psp[i].light = s->psp[i].light;*/
                if (sf & (PSDF_ALPHA << off))
                    d->psp[i].alpha = s->psp[i].alpha;
                if (sf & (PSDF_STATE << off))
                    d->psp[i].state = s->psp[i].state;
                if (sf & (PSDF_OFFSET << off))
                {
                    d->psp[i].offset[VX] = s->psp[i].offset[VX];
                    d->psp[i].offset[VY] = s->psp[i].offset[VY];
                }
            }
        }
    }
    else if (src->type == DT_SECTOR)
    {
        const dt_sector_t*  s = &((const sectordelta_t *) src)->sector;
        dt_sector_t*        d = &((sectordelta_t *) dest)->sector;

        if (sf & SDF_FLOOR_MATERIAL)
            d->planes[PLN_FLOOR].surface.material =
                s->planes[PLN_FLOOR].surface.material;
        if (sf & SDF_CEILING_MATERIAL)
            d->planes[PLN_CEILING].surface.material =
                s->planes[PLN_CEILING].surface.material;
        if (sf & SDF_LIGHT)
            d->lightLevel = s->lightLevel;
        if (sf & SDF_FLOOR_TARGET)
            d->planes[PLN_FLOOR].target = s->planes[PLN_FLOOR].target;
        if (sf & SDF_FLOOR_SPEED)
            d->planes[PLN_FLOOR].speed = s->planes[PLN_FLOOR].speed;
        if (sf & SDF_CEILING_TARGET)
            d->planes[PLN_CEILING].target = s->planes[PLN_CEILING].target;
        if (sf & SDF_CEILING_SPEED)
            d->planes[PLN_CEILING].speed = s->planes[PLN_CEILING].speed;
        if (sf & SDF_FLOOR_HEIGHT)
            d->planes[PLN_FLOOR].height = s->planes[PLN_FLOOR].height;
        if (sf & SDF_CEILING_HEIGHT)
            d->planes[PLN_CEILING].height = s->planes[PLN_CEILING].height;
        if (sf & SDF_COLOR_RED)
            d->rgb[0] = s->rgb[0];
        if (sf & SDF_COLOR_GREEN)
            d->rgb[1] = s->rgb[1];
        if (sf & SDF_COLOR_BLUE)
            d->rgb[2] = s->rgb[2];

        if (sf & SDF_FLOOR_COLOR_RED)
            d->planes[PLN_FLOOR].surface.rgba[0] =
                s->planes[PLN_FLOOR].surface.rgba[0];
        if (sf & SDF_FLOOR_COLOR_GREEN)
            d->planes[PLN_FLOOR].surface.rgba[1] =
                s->planes[PLN_FLOOR].surface.rgba[1];
        if (sf & SDF_FLOOR_COLOR_BLUE)
            d->planes[PLN_FLOOR].surface.rgba[2] =
                s->planes[PLN_FLOOR].surface.rgba[2];

        if (sf & SDF_CEIL_COLOR_RED)
            d->planes[PLN_CEILING].surface.rgba[0] =
                s->planes[PLN_CEILING].surface.rgba[0];
        if (sf & SDF_CEIL_COLOR_GREEN)
            d->planes[PLN_CEILING].surface.rgba[1] =
                s->planes[PLN_CEILING].surface.rgba[1];
        if (sf & SDF_CEIL_COLOR_BLUE)
            d->planes[PLN_CEILING].surface.rgba[2] =
                s->planes[PLN_CEILING].surface.rgba[2];
    }
    else if (src->type == DT_SIDE)
    {
        const dt_side_t*    s = &((const sidedelta_t *) src)->side;
        dt_side_t*          d = &((sidedelta_t *) dest)->side;

        if (sf & SIDF_TOP_MATERIAL)
            d->top.material = s->top.material;
        if (sf & SIDF_MID_MATERIAL)
            d->middle.material = s->middle.material;
        if (sf & SIDF_BOTTOM_MATERIAL)
            d->bottom.material = s->bottom.material;
        if (sf & SIDF_LINE_FLAGS)
            d->lineFlags = s->lineFlags;

        if (sf & SIDF_TOP_COLOR_RED)
            d->top.rgba[0] = s->top.rgba[0];
        if (sf & SIDF_TOP_COLOR_GREEN)
            d->top.rgba[1] = s->top.rgba[1];
        if (sf & SIDF_TOP_COLOR_BLUE)
            d->top.rgba[2] = s->top.rgba[2];

        if (sf & SIDF_MID_COLOR_RED)
            d->middle.rgba[0] = s->middle.rgba[0];
        if (sf & SIDF_MID_COLOR_GREEN)
            d->middle.rgba[1] = s->middle.rgba[1];
        if (sf & SIDF_MID_COLOR_BLUE)
            d->middle.rgba[2] = s->middle.rgba[2];
        if (sf & SIDF_MID_COLOR_ALPHA)
            d->middle.rgba[3] = s->middle.rgba[3];

        if (sf & SIDF_BOTTOM_COLOR_RED)
            d->bottom.rgba[0] = s->bottom.rgba[0];
        if (sf & SIDF_BOTTOM_COLOR_GREEN)
            d->bottom.rgba[1] = s->bottom.rgba[1];
        if (sf & SIDF_BOTTOM_COLOR_BLUE)
            d->bottom.rgba[2] = s->bottom.rgba[2];

        if (sf & SIDF_MID_BLENDMODE)
            d->middle.blendMode = s->middle.blendMode;

        if (sf & SIDF_FLAGS)
            d->flags = s->flags;
    }
    else if (src->type == DT_POLY)
    {
        const dt_poly_t*    s = &((const polydelta_t *) src)->po;
        dt_poly_t*          d = &((polydelta_t *) dest)->po;

        if (sf & PODF_DEST_X)
            d->dest[VX] = s->dest[VX];
        if (sf & PODF_DEST_Y)
            d->dest[VY] = s->dest[VY];
        if (sf & PODF_SPEED)
            d->speed = s->speed;
        if (sf & PODF_DEST_ANGLE)
            d->destAngle = s->destAngle;
        if (sf & PODF_ANGSPEED)
            d->angleSpeed = s->angleSpeed;
    }
    else if (Sv_IsSoundDelta(src))
    {
        const sounddelta_t* s = (const sounddelta_t *) srcDelta;
        sounddelta_t*       d = (sounddelta_t *) destDelta;

        if (sf & SNDDF_VOLUME)
            d->volume = s->volume;
        d->sound = s->sound;
    }
    else
    {
        App_Error("Sv_ApplyDeltaData: Unknown delta type %i.\n", src->type);
    }
}

/**
 * Merges the second delta with the first one.
 * The source and destination must refer to the same entity.
 *
 * @return              @c false, if the result of the merge is a void delta.
 */
dd_bool Sv_MergeDelta(void* destDelta, const void* srcDelta)
{
    const delta_t *src = (const delta_t *) srcDelta;
    delta_t *dest = (delta_t *) destDelta;

#ifdef _DEBUG
    if (!Sv_IsSameDelta(src, dest))
    {
        App_Error("Sv_MergeDelta: Not the same!\n");
    }
    if (dest->state != DELTA_NEW)
    {
        App_Error("Sv_MergeDelta: Dest is not NEW.\n");
    }
#endif

    if (Sv_IsNullMobjDelta(dest))
    {
        // Nothing can be merged with a null mobj delta.
        return true;
    }

    if (Sv_IsNullMobjDelta(src))
    {
        // Null mobj deltas kill the destination.
        dest->flags = MDFC_NULL;
        return true;
    }

    if (Sv_IsCreateMobjDelta(dest) && Sv_IsNullMobjDelta(src))
    {
        // Applying a Null mobj delta on a Create mobj delta causes
        // the two deltas to negate each other. Returning false
        // signifies that both should be removed from the pool.
        dest->flags = 0;
        return false;
    }

    if (Sv_IsStartSoundDelta(src) || Sv_IsStopSoundDelta(src))
    {
        // Sound deltas completely override what they're being
        // merged with. (Only one sound per source.) Stop Sound deltas must
        // kill NEW sound deltas, because what is the benefit of sending
        // both in the same frame: first start a sound and then immediately
        // stop it? We don't want that.

        sounddelta_t *destSound = (sounddelta_t *) destDelta;
        const sounddelta_t* srcSound = (const sounddelta_t *) srcDelta;

        // Destination becomes equal to source.
        dest->flags = src->flags;

        destSound->sound = srcSound->sound;
        destSound->mobj = srcSound->mobj;
        destSound->volume = srcSound->volume;
        return true;
    }

    // The destination will contain all of source's data in addition
    // to the existing data.
    dest->flags |= src->flags;

    // The time stamp must NOT be always updated: the delta already
    // contains data which should've been sent some time ago. If we
    // update the time stamp now, the overdue data might not be sent.
    /* dest->timeStamp = src->timeStamp; */

    Sv_ApplyDeltaData(dest, src);
    return true;
}

/**
 * @return              The age of the delta, in milliseconds.
 */
uint Sv_DeltaAge(const delta_t* delta)
{
    return Sv_GetTimeStamp() - delta->timeStamp;
}

/**
 * Approximate the distance to the given sector. Set 'mayBeGone' to true
 * if the mobj may have been destroyed and should not be processed.
 */
coord_t Sv_MobjDistance(const mobj_t *mo, const ownerinfo_t *info, dd_bool isReal)
{
    coord_t z;

    if (isReal && !Mobj_Map(*mo).thinkers().isUsedMobjId(mo->thinker.id))
    {
        // This mobj does not exist any more!
        return DDMAXFLOAT;
    }

    z = mo->origin[VZ];

    // Registered mobjs may have a maxed out Z coordinate.
    if (!isReal)
    {
        if (z == DDMINFLOAT)
            z = mo->floorZ;
        if (z == DDMAXFLOAT)
            z = mo->ceilingZ - mo->height;
    }

    return M_ApproxDistance3(info->origin[VX] - mo->origin[VX],
                             info->origin[VY] - mo->origin[VY],
                             (info->origin[VZ] - z + mo->height / 2) * 1.2);
}

/**
 * Approximate the distance to the given sector.
 */
coord_t Sv_SectorDistance(int index, const ownerinfo_t *info)
{
    DE_ASSERT(info);
    const Sector &sector = ServerWorld::get().map().sector(index);

    return M_ApproxDistance3(info->origin[0] - sector.soundEmitter().origin[0],
                             info->origin[1] - sector.soundEmitter().origin[1],
                             (info->origin[2] - sector.soundEmitter().origin[2]) * 1.2);
}

coord_t Sv_SideDistance(int index, int deltaFlags, const ownerinfo_t *info)
{
    DE_ASSERT(info);
    const auto *side = ServerWorld::get().map().sidePtr(index);

    const SoundEmitter &emitter = (  deltaFlags & SNDDF_SIDE_MIDDLE? side->middleSoundEmitter()
                                   : deltaFlags & SNDDF_SIDE_TOP   ? side->topSoundEmitter()
                                                                   : side->bottomSoundEmitter());

    return M_ApproxDistance3(info->origin[0]  - emitter.origin[0],
                             info->origin[1]  - emitter.origin[1],
                             (info->origin[2] - emitter.origin[2]) * 1.2);
}

/**
 * @return  The distance to the origin of the delta's entity.
 */
coord_t Sv_DeltaDistance(const void *deltaPtr, const ownerinfo_t *info)
{
    const delta_t *delta = (const delta_t *) deltaPtr;

    if (delta->type == DT_MOBJ)
    {
        // Use the delta's registered mobj position. For old unacked data,
        // it may be somewhat inaccurate.
        return Sv_MobjDistance(&((mobjdelta_t *) deltaPtr)->mo, info, false);
    }

    if (delta->type == DT_PLAYER)
    {
        // Use the player's actual position.
        const mobj_t *mo = DD_Player(delta->id)->publicData().mo;
        if (mo)
        {
            return Sv_MobjDistance(mo, info, true);
        }
    }

    if (delta->type == DT_SECTOR)
    {
        return Sv_SectorDistance(delta->id, info);
    }

    if (delta->type == DT_SIDE)
    {
        auto *side = ServerWorld::get().map().sidePtr(delta->id);
        auto &line = side->line();
        return M_ApproxDistance(info->origin[0] - line.center().x,
                                info->origin[1] - line.center().y);
    }

    if (delta->type == DT_POLY)
    {
        const Polyobj &pob = ServerWorld::get().map().polyobj(delta->id);
        return M_ApproxDistance(info->origin[0] - pob.origin[0],
                                info->origin[1] - pob.origin[1]);
    }

    if (delta->type == DT_MOBJ_SOUND)
    {
        const sounddelta_t *sound = (const sounddelta_t *) deltaPtr;
        return Sv_MobjDistance(sound->mobj, info, true);
    }

    if (delta->type == DT_SECTOR_SOUND)
    {
        return Sv_SectorDistance(delta->id, info);
    }

    if (delta->type == DT_SIDE_SOUND)
    {
        return Sv_SideDistance(delta->id, delta->flags, info);
    }

    if (delta->type == DT_POLY_SOUND)
    {
        const Polyobj &pob = ServerWorld::get().map().polyobj(delta->id);
        return M_ApproxDistance(info->origin[VX] - pob.origin[VX],
                                info->origin[VY] - pob.origin[VY]);
    }

    // Unknown distance.
    return 1;
}

/**
 * The hash function for the pool delta hash.
 */
deltalink_t* Sv_PoolHash(pool_t* pool, int id)
{
    return &pool->hash[(uint) id & POOL_HASH_FUNCTION_MASK];
}

/**
 * The delta is removed from the pool's delta hash.
 */
void Sv_RemoveDelta(pool_t* pool, void* deltaPtr)
{
    delta_t*            delta = (delta_t *) deltaPtr;
    deltalink_t*        hash = Sv_PoolHash(pool, delta->id);

    // Update first and last links.
    if (hash->last == delta)
    {
        hash->last = delta->prev;
    }
    if (hash->first == delta)
    {
        hash->first = delta->next;
    }

    // Link the delta out of the list.
    if (delta->next)
    {
        delta->next->prev = delta->prev;
    }

    if (delta->prev)
    {
        delta->prev->next = delta->next;
    }

    // Destroy it.
    Z_Free(delta);
}

/**
 * Draining the pool means emptying it of all contents. (Doh?)
 */
void Sv_DrainPool(uint clientNumber)
{
    pool_t*             pool = Sv_GetPool(clientNumber);
    delta_t*            delta;
    misrecord_t*        mis;
    void*               next = NULL;
    int                 i;

    // Update the number of the owner.
    pool->owner = clientNumber;

    // Reset the counters.
    pool->setDealer = 0;
    pool->resendDealer = 0;

    Sv_PoolQueueClear(pool);

    // Free all deltas stored in the hash.
    for (i = 0; i < POOL_HASH_SIZE; ++i)
    {
        for (delta = (delta_t *) pool->hash[i].first; delta; delta = (delta_t *) next)
        {
            next = delta->next;
            Z_Free(delta);
        }
    }

    // Free all missile records in the pool.
    for (i = 0; i < POOL_MISSILE_HASH_SIZE; ++i)
    {
        for (mis = (misrecord_t *) pool->misHash[i].first; mis; mis = (misrecord_t *) next)
        {
            next = mis->next;
            Z_Free(mis);
        }
    }

    // Clear all the chains.
    de::zap(pool->hash);
    de::zap(pool->misHash);
}

/**
 * Returns the maximum distance for the sound. If the origin is any farther,
 * the delta will not be sent to the client in question.
 */
dfloat Sv_GetMaxSoundDistance(const sounddelta_t *delta)
{
    dfloat volume = 1;

    // Volume shortens the maximum distance (why send it if it's not
    // audible?).
    if (delta->delta.flags & SNDDF_VOLUME)
    {
        volume = delta->volume;
    }

    if (volume <= 0)
    {
        // Silence is heard all over the world.
        return DDMAXFLOAT;
    }

    return volume * ::soundMaxDist;
}

/**
 * @return  The flags that remain after exclusion.
 */
int Sv_ExcludeDelta(pool_t* pool, const void* deltaPtr)
{
    const delta_t*      delta = (const delta_t *) deltaPtr;
    player_t*           plr = DD_Player(pool->owner);
    mobj_t*             poolViewer = plr->publicData().mo;
    int                 flags = delta->flags;

    // Can we exclude information from the delta? (for this player only)
    if (delta->type == DT_MOBJ)
    {
        const mobjdelta_t *mobjDelta = (const mobjdelta_t *) deltaPtr;

        if (poolViewer && poolViewer->thinker.id == delta->id)
        {
            // This is the mobj the owner of the pool uses as a camera.
            flags &= ~MDF_CAMERA_EXCLUDE;

            // This information is sent in the PSV_PLAYER_FIX packet,
            // but only under specific circumstances. Most of the time
            // the client is responsible for updating its own pos/mom/angle.
            flags &= ~MDF_ORIGIN;
            flags &= ~MDF_MOM;
            flags &= ~MDF_ANGLE;
        }

        // What about missiles? We might be allowed to exclude some
        // information.
        if (mobjDelta->mo.ddFlags & DDMF_MISSILE)
        {
            if (Sv_IsNullMobjDelta(delta))
            {
                // The missile is being removed entirely.
                // Remove the entry from the missile record.
                Sv_MRRemove(pool, delta->id);
            }
            else if (!Sv_IsCreateMobjDelta(delta))
            {
                // This'll might exclude the coordinates.
                // The missile is put on record when the client acknowledges
                // the Create Mobj delta.
                flags &= ~Sv_MRCheck(pool, mobjDelta);
            }
        }
    }
    else if (delta->type == DT_PLAYER)
    {
        if (pool->owner == delta->id)
        {
            // All information does not need to be sent.
            flags &= ~PDF_CAMERA_EXCLUDE;

            /* $unifiedangles */
            /*
            if (!(player->flags & DDPF_FIXANGLES))
            {
                // Fixangles means that the server overrides the clientside
                // view angles. Normally they are always clientside, so
                // exclude them here.
                flags &= ~(PDF_CLYAW | PDF_CLPITCH);
            }
             */
        }
        else
        {
            // This is a remote player, the owner of the pool doesn't need
            // to know everything about it (like psprites).
            flags &= ~PDF_NONCAMERA_EXCLUDE;
        }
    }
    else if (Sv_IsSoundDelta(delta))
    {
        // Sounds that originate from too far away are not added to a pool.
        // Stop Sound deltas have an infinite max distance, though.
        if (Sv_DeltaDistance(delta, &pool->ownerInfo) >
           Sv_GetMaxSoundDistance((const sounddelta_t *) deltaPtr))
        {
            // Don't add it.
            return 0;
        }
    }

    // These are the flags that remain.
    return flags;
}

/**
 * When adding a delta to the pool, it subtracts from the unacked deltas
 * there and is merged with matching new deltas. If a delta becomes void
 * after subtraction, it's removed from the pool. All the processing is
 * done based on the ID number of the delta (and type), so to make things
 * more efficient, a hash table is used (key is ID).
 *
 * Deltas are unique only in the NEW state. There may be multiple UNACKED
 * deltas for the same entity.
 *
 * The contents of the delta must not be modified.
 */
void Sv_AddDelta(pool_t* pool, void* deltaPtr)
{
    delta_t*            iter, *next = NULL, *existingNew = NULL;
    delta_t*            delta = (delta_t *) deltaPtr;
    deltalink_t*        hash = Sv_PoolHash(pool, delta->id);
    int                 flags, originalFlags;

    // Sometimes we can exclude a part of the data, if the client has no
    // use for it.
    flags = Sv_ExcludeDelta(pool, delta);

    if (!flags)
    {
        // No data remains... No need to add this delta.
        return;
    }

    // Temporarily use the excluded flags.
    originalFlags = delta->flags;
    delta->flags = flags;

    // While subtracting from old deltas, we'll look for a pointer to
    // an existing NEW delta.
    for (iter = hash->first; iter; iter = next)
    {
        // Iter is removed if it becomes void.
        next = iter->next;

        // Sameness is determined with type and ID.
        if (Sv_IsSameDelta(iter, delta))
        {
            if (iter->state == DELTA_NEW)
            {
                // We'll merge with this instead of adding a new delta.
                existingNew = iter;
            }
            else if (iter->state == DELTA_UNACKED)
            {
                // The new information in the delta overrides the info in
                // this unacked delta. Let's subtract. This way, if the
                // unacked delta needs to be resent, it won't contain
                // obsolete data.
                Sv_SubtractDelta(iter, delta);

                // Was everything removed?
                if (Sv_IsVoidDelta(iter))
                {
                    Sv_RemoveDelta(pool, iter);
                    continue;
                }
            }
        }
    }

    if (existingNew)
    {
        // Merge the new delta with the older NEW delta.
        if (!Sv_MergeDelta(existingNew, delta))
        {
            // The deltas negated each other (Null -> Create).
            // The existing delta must be removed.
            Sv_RemoveDelta(pool, existingNew);
        }
    }
    else
    {
        // Add it to the end of the hash chain. We must take a copy
        // of the delta so it can be stored in the hash.
        iter = (delta_t *) Sv_CopyDelta(delta);

        if (hash->last)
        {
            hash->last->next = iter;
            iter->prev = hash->last;
        }
        hash->last = iter;

        if (!hash->first)
        {
            hash->first = iter;
        }
    }

    // This delta may yet be added to other pools. They should use the
    // original flags, not the ones we might've used (hackish: copying the
    // whole delta is not really an option, though).
    delta->flags = originalFlags;
}

/**
 * Add the delta to all the pools in the NULL-terminated array.
 */
void Sv_AddDeltaToPools(void* deltaPtr, pool_t** targets)
{
    for (; *targets; targets++)
    {
        Sv_AddDelta(*targets, deltaPtr);
    }
}

/**
 * All NEW deltas for the mobj are removed from the pool as obsolete.
 */
void Sv_PoolMobjRemoved(pool_t* pool, thid_t id)
{
    deltalink_t*        hash = Sv_PoolHash(pool, id);
    delta_t*            delta, *next = NULL;

    for (delta = hash->first; delta; delta = next)
    {
        next = delta->next;

        if (delta->state == DELTA_NEW && delta->type == DT_MOBJ &&
           delta->id == id)
        {
            // This must be removed!
            Sv_RemoveDelta(pool, delta);
        }
    }

    // Also check the missile record.
    Sv_MRRemove(pool, id);
}

/**
 * This is called when a mobj is removed in a predictable fashion.
 * (Mobj state is NULL when it's destroyed. Assumption: The NULL state
 * is set only when animation reaches its end.) Because the register-mobj
 * is removed, no Null Mobj delta is generated for the mobj.
 */
void Sv_MobjRemoved(thid_t id)
{
    uint                i;
    reg_mobj_t*         mo = Sv_RegisterFindMobj(&worldRegister, id);

    if (mo)
    {
        Sv_RegisterRemoveMobj(&worldRegister, mo);

        // We must remove all NEW deltas for this mobj from the pools.
        // One possibility: there are mobj deltas waiting in the pool,
        // but the mobj is removed here. Because it'll be no longer in
        // the register, no Null Mobj delta is generated, and thus the
        // client will eventually receive those mobj deltas unnecessarily.

        for (i = 0; i < DDMAXPLAYERS; ++i)
        {
            if (DD_Player(i)->isConnected())
            {
                Sv_PoolMobjRemoved(Sv_GetPool(i), id);
            }
        }
    }
}

/**
 * When a player leaves the game, his data is removed from the register.
 * Otherwise he'll not get all the data if he reconnects before the map
 * is changed.
 */
void Sv_PlayerRemoved(uint playerNumber)
{
    de::zap(worldRegister.ddPlayers[playerNumber]);
}

/**
 * @return              @c true, if the pool is in the targets array.
 */
dd_bool Sv_IsPoolTargeted(pool_t* pool, pool_t** targets)
{
    for (; *targets; targets++)
    {
        if (pool == *targets)
            return true;
    }

    return false;
}

/**
 * Fills the array with pointers to the pools of the connected clients,
 * if specificClient is < 0.
 *
 * @return              The number of pools in the list.
 */
int Sv_GetTargetPools(pool_t** targets, int clientsMask)
{
    int i, numTargets = 0;

    for (i = 0; i < DDMAXPLAYERS; ++i)
    {
        if (clientsMask & (1 << i) && DD_Player(i)->isConnected())
        {
            targets[numTargets++] = Sv_GetPool(i);
        }
    }
/*
    if (specificClient & SVSF_ specificClient >= 0)
    {
        targets[0] = &pools[specificClient];
        targets[1] = NULL;
        return 1;
    }

    for (i = 0; i < DDMAXPLAYERS; ++i)
    {
        // Deltas must be generated for all connected players, even
        // if they aren't yet ready to receive them.
        if (clients[i].connected)
        {
            if (specificClient == SV_TARGET_ALL_POOLS || i != -specificClient)
                targets[numTargets++] = &pools[i];
        }
    }
*/
    // A NULL pointer marks the end of target pools.
    targets[numTargets] = NULL;

    return numTargets;
}

/**
 * Null deltas are generated for mobjs that have been destroyed.
 * The register's mobj hash is scanned to see which mobjs no longer exist.
 *
 * When updating, the destroyed mobjs are removed from the register.
 */
void Sv_NewNullDeltas(cregister_t *reg, dd_bool doUpdate, pool_t **targets)
{
    int i;
    mobjhash_t *hash;
    reg_mobj_t *obj, *next = 0;
    mobjdelta_t null;

    for (i = 0, hash = reg->mobjs; i < REG_MOBJ_HASH_SIZE; ++i, hash++)
    {
        for (obj = hash->first; obj; obj = next)
        {
            // This reg_mobj_t might be removed.
            next = obj->next;

            /// @todo Do not assume mobj is from the CURRENT map.
            if (!ServerWorld::get().map().thinkers().isUsedMobjId(obj->mo.thinker.id))
            {
                // This object no longer exists!
                Sv_NewDelta(&null, DT_MOBJ, obj->mo.thinker.id);
                null.delta.flags = MDFC_NULL;

                // We need all the data for positioning.
                memcpy(&null.mo, &obj->mo, sizeof(dt_mobj_t));

                Sv_AddDeltaToPools(&null, targets);

                if (doUpdate)
                {
                    // Keep the register up to date.
                    Sv_RegisterRemoveMobj(reg, obj);
                }
            }
        }
    }
}

/**
 * Mobj deltas are generated for all mobjs that have changed.
 */
void Sv_NewMobjDeltas(cregister_t *reg, dd_bool doUpdate, pool_t **targets)
{
    ServerWorld::get().map().thinkers().forAll(reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                                       0x1 /*public*/, [&reg, &doUpdate, &targets] (thinker_t *th)
    {
        auto &mob = *reinterpret_cast<mobj_t *>(th);

        // Some objects should not be processed.
        if (!Sv_IsMobjIgnored(mob))
        {
            // Compare to produce a delta.
            mobjdelta_t delta;
            if (Sv_RegisterCompareMobj(reg, &mob, &delta))
            {
                Sv_AddDeltaToPools(&delta, targets);

                if (doUpdate)
                {
                    // This'll add a new register-mobj if it doesn't already exist.
                    Sv_RegisterMobj(&Sv_RegisterAddMobj(reg, mob.thinker.id)->mo, &mob);
                }
            }
        }
        return LoopContinue;
    });
}

/**
 * Player deltas are generated for changed player data.
 */
void Sv_NewPlayerDeltas(cregister_t* reg, dd_bool doUpdate, pool_t** targets)
{
    playerdelta_t player;
    uint i;

    for (i = 0; i < DDMAXPLAYERS; ++i)
    {
        if (Sv_IsPlayerIgnored(i)) continue;

        // Compare to produce a delta.
        if (Sv_RegisterComparePlayer(reg, i, &player))
        {
            // Did the mobj change? If so, the old mobj must be zeroed
            // in the register. Otherwise, the clients may not receive
            // all the data they need (because of viewpoint exclusion
            // flags).
            if (doUpdate && (player.delta.flags & PDF_MOBJ))
            {
                reg_mobj_t* registered = Sv_RegisterFindMobj(reg, reg->ddPlayers[i].mobj);

                if (registered)
                {
                    Sv_RegisterResetMobj(&registered->mo);
                }
            }

            Sv_AddDeltaToPools(&player, targets);
        }

        if (doUpdate)
        {
            Sv_RegisterPlayer(&reg->ddPlayers[i], i);
        }

        // What about forced deltas?
        if (Sv_IsPoolTargeted(Sv_GetPool(i), targets))
        {
#if 0
            if (DD_Player(i).flags & DDPF_FIXANGLES)
            {
                Sv_NewDelta(&player, DT_PLAYER, i);
                Sv_RegisterPlayer(&player.player, i);
                //player.delta.flags = PDF_CLYAW | PDF_CLPITCH; /* $unifiedangles */

                // Once added to the pool, the information will not get lost.
                Sv_AddDelta(&pools[i], &player);

                // Doing this once is enough.
                DD_Player(i).flags &= ~DDPF_FIXANGLES;
            }

            // Generate a FIXPOS/FIXMOM mobj delta, too?
            if (DD_Player(i).mo && (DD_Player(i).flags & (DDPF_FIXORIGIN | DDPF_FIXMOM)))
            {
                const mobj_t *mo = DD_Player(i).mo;
                mobjdelta_t mobj;

                Sv_NewDelta(&mobj, DT_MOBJ, mo->thinker.id);
                Sv_RegisterMobj(&mobj.mo, mo);
                if (DD_Player(i).flags & DDPF_FIXORIGIN)
                {
                    mobj.delta.flags |= MDF_ORIGIN;
                }
                if (DD_Player(i).flags & DDPF_FIXMOM)
                {
                    mobj.delta.flags |= MDF_MOM;
                }

                Sv_AddDelta(&pools[i], &mobj);

                // Doing this once is enough.
                DD_Player(i).flags &= ~(DDPF_FIXORIGIN | DDPF_FIXMOM);
            }
#endif
        }
    }
}

/**
 * Sector deltas are generated for changed sectors.
 */
void Sv_NewSectorDeltas(cregister_t *reg, dd_bool doUpdate, pool_t **targets)
{
    sectordelta_t delta;

    for (int i = 0; i < ServerWorld::get().map().sectorCount(); ++i)
    {
        if (Sv_RegisterCompareSector(reg, i, &delta, doUpdate))
        {
            Sv_AddDeltaToPools(&delta, targets);
        }
    }
}

/**
 * Side deltas are generated for changed sides (and line flags).
 * Changes in sides (textures) are so rare that all sides need not be
 * checked on every tic.
 */
void Sv_NewSideDeltas(cregister_t *reg, dd_bool doUpdate, pool_t **targets)
{
    static uint numShifts = 2, shift = 0;

    /// @todo fixme: Do not assume the current map.
    world::Map &map = ServerWorld::get().map();

    // When comparing against an initial register, always compare all
    // sides (since the comparing is only done once, not continuously).
    uint start, end;
    if (reg->isInitial)
    {
        start = 0;
        end = map.sideCount();
    }
    else
    {
        // Because there are so many sides in a typical map, the number
        // of compared sides soon accumulates to millions. To reduce the
        // load, we'll check only a portion of all sides for a frame.
        start = shift * map.sideCount() / numShifts;
        end = ++shift * map.sideCount() / numShifts;
        shift %= numShifts;
    }

    sidedelta_t delta;
    for (uint i = start; i < end; ++i)
    {
        if (Sv_RegisterCompareSide(reg, i, &delta, doUpdate))
        {
            Sv_AddDeltaToPools(&delta, targets);
        }
    }
}

/**
 * Poly deltas are generated for changed polyobjs.
 */
void Sv_NewPolyDeltas(cregister_t *reg, dd_bool doUpdate, pool_t **targets)
{
    LOG_AS("Sv_NewPolyDeltas");

    polydelta_t delta;

    /// @todo fixme: Do not assume the current map.
    for (int i = 0; i < ServerWorld::get().map().polyobjCount(); ++i)
    {
        if (Sv_RegisterComparePoly(reg, i, &delta))
        {
            LOGDEV_NET_XVERBOSE_DEBUGONLY("Change in poly %i", i);

            Sv_AddDeltaToPools(&delta, targets);
        }

        if (doUpdate)
        {
            Sv_RegisterPoly(&reg->polyObjs[i], i);
        }
    }
}

void Sv_NewSoundDelta(int soundId, const mobj_t *emitter, world::Sector *sourceSector,
    Polyobj *sourcePoly, world::Plane *sourcePlane, world::Surface *sourceSurface,
    float volume, dd_bool isRepeating, int clientsMask)
{
    pool_t *targets[DDMAXPLAYERS + 1];
    sounddelta_t soundDelta;
    int type = DT_SOUND, df = 0;
    int id = soundId;

    // Determine the target pools.
    Sv_GetTargetPools(targets, clientsMask);

    if (sourceSector)
    {
        type = DT_SECTOR_SOUND;
        id = sourceSector->indexInMap();
        // Client assumes the sector's sound origin.
    }
    else if (sourcePoly)
    {
        type = DT_POLY_SOUND;
        id = sourcePoly->indexInMap();
    }
    else if (sourcePlane)
    {
        type = DT_SECTOR_SOUND;

        // Clients need to know which emitter to use.
        if (emitter && emitter == (const mobj_t *) &sourcePlane->soundEmitter())
        {
            if (sourcePlane->isSectorFloor())
            {
                df |= SNDDF_PLANE_FLOOR;
            }
            else if (sourcePlane->isSectorCeiling())
            {
                df |= SNDDF_PLANE_CEILING;
            }
        }
        // else client assumes the sector's sound emitter.

        id = sourcePlane->sector().indexInMap();
    }
    else if (sourceSurface)
    {
        DE_ASSERT(sourceSurface->parent().type() == DMU_SIDE);
        DE_ASSERT(emitter == 0); // surface sound emitter rather than a real mobj

        type = DT_SIDE_SOUND;

        // Clients need to know which emitter to use.
        auto &side = sourceSurface->parent().as<world::LineSide>();

        if (&side.middle() == sourceSurface)
        {
            df |= SNDDF_SIDE_MIDDLE;
        }
        else if (&side.bottom() == sourceSurface)
        {
            df |= SNDDF_SIDE_BOTTOM;
        }
        else if (&side.top() == sourceSurface)
        {
            df |= SNDDF_SIDE_TOP;
        }

        id = side.indexInMap();
    }
    else if (emitter)
    {
        type = DT_MOBJ_SOUND;
        id = emitter->thinker.id;
        soundDelta.mobj = emitter;
    }

    // Init to the right type.
    Sv_NewDelta(&soundDelta, (deltatype_t) type, id);

    // Always set volume.
    df |= SNDDF_VOLUME;
    soundDelta.volume = volume;

    if (isRepeating)
        df |= SNDDF_REPEAT;

    LOGDEV_NET_XVERBOSE("New sound delta: type=%i id=%i flags=%x", type << id << df);

    // This is used by mobj/sector sounds.
    soundDelta.sound = soundId;

    soundDelta.delta.flags = df;
    Sv_AddDeltaToPools(&soundDelta, targets);
}

/**
 * Returns @c true if the client should receive frames.
 */
dd_bool Sv_IsFrameTarget(duint plrNum)
{
    DE_ASSERT(plrNum < DDMAXPLAYERS);

    const player_t &plr = *DD_Player(plrNum);

    // Clients must tell us they are ready before we can begin sending.
    return (plr.publicData().inGame && plr.ready);
}

/**
 * Compare the current state of the world with the register and add the
 * deltas to all the pools, or if a specific client number is given, only
 * to its pool (done when a new client enters the game). No deltas will be
 * generated for predictable changes (state changes, linear movement...).
 *
 * @param reg           World state register.
 * @param clientNumber  Client for whom to generate deltas. < 0 = all ingame
 *                      clients should get the deltas.
 * @param doUpdate      Updating the register means that the current state
 *                      of the world is stored in the register after the
 *                      deltas have been generated.
 */
void Sv_GenerateNewDeltas(cregister_t* reg, int clientNumber, dd_bool doUpdate)
{
    pool_t* targets[DDMAXPLAYERS + 1], **pool;

    // Determine the target pools.
    Sv_GetTargetPools(targets, (clientNumber < 0 ? 0xff : (1 << clientNumber)));

    // Update the info of the pool owners.
    for (pool = targets; *pool; pool++)
    {
        Sv_UpdateOwnerInfo(*pool);
    }

    // Generate null deltas (removed mobjs).
    Sv_NewNullDeltas(reg, doUpdate, targets);

    // Generate mobj deltas.
    Sv_NewMobjDeltas(reg, doUpdate, targets);

    // Generate player deltas.
    Sv_NewPlayerDeltas(reg, doUpdate, targets);

    // Generate sector deltas.
    Sv_NewSectorDeltas(reg, doUpdate, targets);

    // Generate side deltas.
    Sv_NewSideDeltas(reg, doUpdate, targets);

    // Generate poly deltas.
    Sv_NewPolyDeltas(reg, doUpdate, targets);

    if (doUpdate)
    {
        // The register has now been updated to the current time.
        reg->gametic = SECONDS_TO_TICKS(gameTime);
    }
}

/**
 * This is called once for each frame, in Sv_TransmitFrame().
 */
void Sv_GenerateFrameDeltas(void)
{
    // Generate new deltas for all clients and update the world register.
    Sv_GenerateNewDeltas(&worldRegister, -1, true);
}

/**
 * Clears the priority queue of the pool.
 */
void Sv_PoolQueueClear(pool_t* pool)
{
    pool->queueSize = 0;
}

/**
 * Exchanges two elements in the queue.
 */
void Sv_PoolQueueExchange(pool_t* pool, int index1, int index2)
{
    delta_t *temp = pool->queue[index1];

    pool->queue[index1] = pool->queue[index2];
    pool->queue[index2] = temp;
}

/**
 * Adds the delta to the priority queue. More memory is allocated for the
 * queue if necessary.
 */
void Sv_PoolQueueAdd(pool_t* pool, delta_t* delta)
{
    int                 i, parent;

    // Do we need more memory?
    if (pool->allocatedSize == pool->queueSize)
    {
        delta_t**           newQueue;

        // Double the memory.
        pool->allocatedSize *= 2;
        if (!pool->allocatedSize)
        {
            // At least eight.
            pool->allocatedSize = 8;
        }

        // Allocate the new queue.
        newQueue = (delta_t **) Z_Malloc(pool->allocatedSize * sizeof(delta_t *), PU_MAP, 0);

        // Copy the old data.
        if (pool->queue)
        {
            memcpy(newQueue, pool->queue,
                   sizeof(delta_t *) * pool->queueSize);

            // Get rid of the old queue.
            Z_Free(pool->queue);
        }

        pool->queue = newQueue;
    }

    // Add the new delta to the end of the queue array.
    i = pool->queueSize;
    pool->queue[i] = delta;
    ++pool->queueSize;

    // Rise in the heap until the correct place is found.
    while (i > 0)
    {
        parent = HEAP_PARENT(i);

        // Is it good now?
        if (pool->queue[parent]->score >= delta->score)
            break;

        // Exchange with the parent.
        Sv_PoolQueueExchange(pool, parent, i);

        i = parent;
    }
}

/**
 * Extracts the delta with the highest priority from the queue.
 *
 * @return              @c NULL, if there are no more deltas.
 */
delta_t* Sv_PoolQueueExtract(pool_t* pool)
{
    delta_t*            max;
    int                 i, left, right, big;
    dd_bool             isDone;

    if (!pool->queueSize)
    {
        // There is nothing in the queue.
        return NULL;
    }

    // This is what we'll return.
    max = pool->queue[0];

    // Remove the first element from the queue.
    pool->queue[0] = pool->queue[--pool->queueSize];

    // Heapify the heap. This is O(log n).
    i = 0;
    isDone = false;
    while (!isDone)
    {
        left = HEAP_LEFT(i);
        right = HEAP_RIGHT(i);
        big = i;

        // Which child is more important?
        if (left < pool->queueSize &&
           pool->queue[left]->score > pool->queue[i]->score)
        {
            big = left;
        }
        if (right < pool->queueSize &&
           pool->queue[right]->score > pool->queue[big]->score)
        {
            big = right;
        }

        // Can we stop now?
        if (big != i)
        {
            // Exchange and continue.
            Sv_PoolQueueExchange(pool, i, big);
            i = big;
        }
        else
        {
            // Heapifying is complete.
            isDone = true;
        }
    }

    return max;
}

/**
 * Postponed deltas can't be sent yet.
 */
dd_bool Sv_IsPostponedDelta(void* deltaPtr, ownerinfo_t* info)
{
    delta_t *delta = (delta_t *) deltaPtr;
    uint age = Sv_DeltaAge(delta);

    if (delta->state == DELTA_UNACKED)
    {
        // Is it old enough? If not, it's still possible that the ack
        // has not reached us yet.
        return age < info->ackThreshold;
    }
    else if (delta->state == DELTA_NEW)
    {
        // Normally NEW deltas are never postponed. They are sent as soon
        // as possible.
        if (Sv_IsStopSoundDelta(delta))
        {
            // Stop Sound deltas require a bit of care. To make sure they
            // arrive to the client in the correct order, we won't send
            // a Stop Sound until we can be sure all the Start Sound deltas
            // have arrived. (i.e. the pool must contain no Unacked Start
            // Sound deltas for the same source.)

            delta_t*            iter;

            for (iter = Sv_PoolHash(info->pool, delta->id)->first; iter;
                iter = iter->next)
            {
                if (iter->state == DELTA_UNACKED && Sv_IsSameDelta(iter, delta)
                   && Sv_IsStartSoundDelta(iter))
                {
                    // Must postpone this Stop Sound delta until this one
                    // has been sent.
                    return true;
                }
            }
        }
    }

    // This delta is not postponed.
    return false;
}

/**
 * Calculate a priority score for the delta. A higher score indicates
 * greater importance.
 *
 * @return              @c true iff the delta should be included in the
 *                      queue.
 */
dd_bool Sv_RateDelta(void* deltaPtr, ownerinfo_t* info)
{
    float score, size;
    coord_t distance;
    delta_t *delta = (delta_t *) deltaPtr;
    int df = delta->flags;
    uint age = Sv_DeltaAge(delta);

    // The importance doubles normally in 1 second.
    float ageScoreDouble = 1.0f;

    if (Sv_IsPostponedDelta(delta, info))
    {
        // This delta will not be considered at this time.
        return false;
    }

    // Calculate the distance to the delta's origin.
    // If no distance can be determined, it's 1.0.
    distance = Sv_DeltaDistance(delta, info);
    if (distance < 1)
        distance = 1;
    distance = distance * distance; // Power of two.

    // What is the base score?
    score = deltaBaseScores[delta->type] / distance;

    // It's very important to send sound deltas in time.
    if (Sv_IsSoundDelta(delta))
    {
        // Score doubles very quickly.
        ageScoreDouble = 1;
    }

    // Deltas become more important with age (milliseconds).
    score *= 1 + age / (ageScoreDouble * 1000.0f);

    /// @todo Consider viewpoint speed and angle.

    // Priority bonuses based on the contents of the delta.
    if (delta->type == DT_MOBJ)
    {
        const mobj_t* mo = &((mobjdelta_t *) delta)->mo;

        // Seeing new mobjs is interesting.
        if (df & MDFC_CREATE)
            score *= 1.5f;

        // Position changes are important.
        if (df & (MDF_ORIGIN_X | MDF_ORIGIN_Y))
            score *= 1.2f;

        // Small objects are not that important.
        size = MAX_OF(mo->radius, mo->height);
        if (size < 16)
        {
            // Not too small, though.
            if (size < 2)
                size = 2;

            score *= size / 16;
        }
        else if (size > 50)
        {
            // Large objects are important.
            score *= size / 50;
        }
    }
    else if (delta->type == DT_PLAYER)
    {
        // Knowing the player's mobj is quite important.
        if (df & PDF_MOBJ)
            score *= 2;
    }
    else if (delta->type == DT_SECTOR)
    {
        // Lightlevel changes are very noticeable.
        if (df & SDF_LIGHT)
            score *= 1.2f;

        // Plane movements are very important (can be seen from far away).
        if (df &
           (SDF_FLOOR_HEIGHT | SDF_CEILING_HEIGHT | SDF_FLOOR_SPEED |
            SDF_CEILING_SPEED | SDF_FLOOR_TARGET | SDF_CEILING_TARGET))
        {
            score *= 3;
        }
    }
    else if (delta->type == DT_POLY)
    {
        // Changes in speed are noticeable.
        if (df & PODF_SPEED)
            score *= 1.2f;
    }

    // This is the final score. Only positive scores are accepted in
    // the frame (deltas with nonpositive scores as ignored).
    delta->score = score;
    return (score > 0);
}

/**
 * Calculate a priority score for each delta and build the priority queue.
 * The most important deltas will be included in a frame packet.
 * A pool is rated after new deltas have been generated.
 */
void Sv_RatePool(pool_t* pool)
{
#ifdef _DEBUG
    player_t*           plr = DD_Player(pool->owner);
#endif
    delta_t*            delta;
    int                 i;

#ifdef _DEBUG
    if (!plr->publicData().mo)
    {
        App_Error("Sv_RatePool: Player %i has no mobj.\n", pool->owner);
    }
#endif

    // Clear the queue.
    Sv_PoolQueueClear(pool);

    // We will rate all the deltas in the pool. After each delta
    // has been rated, it's added to the priority queue.
    for (i = 0; i < POOL_HASH_SIZE; ++i)
    {
        for (delta = pool->hash[i].first; delta; delta = delta->next)
        {
            if (Sv_RateDelta(delta, &pool->ownerInfo))
            {
                Sv_PoolQueueAdd(pool, delta);
            }
        }
    }
}

/**
 * Do special things that need to be done when the delta has been acked.
 */
void Sv_AckDelta(pool_t* pool, delta_t* delta)
{
    if (Sv_IsCreateMobjDelta(delta))
    {
        mobjdelta_t*        mobjDelta = (mobjdelta_t *) delta;

        // Created missiles are put on record.
        if (mobjDelta->mo.ddFlags & DDMF_MISSILE)
        {
            // Once again, we're assuming the delta is always completely
            // filled with valid information. (There are no 'partial' deltas.)
            Sv_MRAdd(pool, mobjDelta);
        }
    }
}

/**
 * Acknowledged deltas are removed from the pool, never to be seen again.
 * Clients ack deltas to tell the server they've received them.
 *
 * @note This is obsolete: deltas no longer need to be acknowledged as
 * they are sent over TCP.
 *
 * @param clientNumber  Client whose deltas to ack.
 * @param set           Delta set number.
 * @param resent        If nonzero, ignore 'set' and ack by resend ID.
 */
void Sv_AckDeltaSet(uint clientNumber, int set, byte resent)
{
    pool_t  *pool = Sv_GetPool(clientNumber);
    delta_t *delta, *next = NULL;

    // Iterate through the entire hash table.
    for (int i = 0; i < POOL_HASH_SIZE; ++i)
    {
        for (delta = pool->hash[i].first; delta; delta = next)
        {
            next = delta->next;
            if (delta->state == DELTA_UNACKED &&
               ((!resent && delta->set == set) ||
                (resent && delta->resend == resent)))
            {
                // There may be something that we need to do now that the
                // delta has been acknowledged.
                Sv_AckDelta(pool, delta);

                // This delta is now finished!
                Sv_RemoveDelta(pool, delta);
            }
        }
    }
}

/**
 * Debugging metric.
 */
uint Sv_CountUnackedDeltas(uint clientNumber)
{
    uint                i, count;
    pool_t*             pool = Sv_GetPool(clientNumber);
    delta_t*            delta;

    // Iterate through the entire hash table.
    count = 0;
    for (i = 0; i < POOL_HASH_SIZE; ++i)
    {
        for (delta = pool->hash[i].first; delta; delta = delta->next)
        {
            if (delta->state == DELTA_UNACKED)
               ++count;
        }
    }
    return count;
}
