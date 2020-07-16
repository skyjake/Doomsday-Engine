/** @file cl_mobj.cpp  Client map objects.
 * @ingroup client
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#define DE_NO_API_MACROS_CLIENT

#include "de_base.h"
#include "api_client.h"
#include "api_sound.h"
#include "client/cl_mobj.h"
#include "client/cl_player.h"
#include "client/cl_world.h"
#include "network/net_main.h"
#include "network/protocol.h"
#include "world/map.h"
#include "world/p_players.h"

#include <doomsday/world/mobjthinker.h>
#include <de/legacy/timer.h>
#include <de/legacy/vector1.h>
#include <de/logbuffer.h>
#include <cmath>

using namespace de;

using world::World;
using world::Sector;
using world::DmuArgs;

/// Convert 8.8/10.6 fixed point to 16.16.
#define UNFIXED8_8(x)   (((x) << 16) / 256)
#define UNFIXED10_6(x)  (((x) << 16) / 64)

#if 0
ClMobjInfo::ClMobjInfo()
    : startMagic(CLM_MAGIC1)
    , next      (0)
    , prev      (0)
    , flags     (0)
    , time      (Timer_RealMilliseconds())
    , sound     (0)
    , volume    (0)
    , endMagic  (CLM_MAGIC2)
{}

mobj_t *ClMobjInfo::mobj()
{
    DE_ASSERT(startMagic == CLM_MAGIC1);
    DE_ASSERT(endMagic == CLM_MAGIC2);

    return reinterpret_cast<mobj_t *>((char *)this + sizeof(ClMobjInfo));
}
#endif

void ClMobj_Link(mobj_t *mo)
{
    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(mo);

    CL_ASSERT_CLMOBJ(mo);

    if ((info->flags & (CLMF_HIDDEN | CLMF_UNPREDICTABLE)) || mo->dPlayer)
    {
        // We do not yet have all the details about Hidden mobjs.
        // The server hasn't sent us a Create Mobj delta for them.
        // Client mobjs that belong to players remain unlinked.
        return;
    }
    LOG_NET_XVERBOSE("ClMobj_Link: id %i, x%f Y%f, solid:%b",
                     mo->thinker.id << mo->origin[VX] << mo->origin[VY]
                     << (mo->ddFlags & DDMF_SOLID));

    Mobj_Link(mo, (mo->ddFlags & DDMF_DONTDRAW ? 0 : MLF_SECTOR) |
                  (mo->ddFlags & DDMF_SOLID ? MLF_BLOCKMAP : 0));
}

#undef ClMobj_EnableLocalActions
void ClMobj_EnableLocalActions(mobj_t *mo, dd_bool enable)
{
    LOG_AS("ClMobj_EnableLocalActions");

    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(mo);
    if (!isClient || !info) return;
    if (enable)
    {
        LOGDEV_NET_VERBOSE("Enabled for clmobj %i") << mo->thinker.id;
        info->flags |= CLMF_LOCAL_ACTIONS;
    }
    else
    {
        LOGDEV_NET_VERBOSE("Disabled for clmobj %i") << mo->thinker.id;
        info->flags &= ~CLMF_LOCAL_ACTIONS;
    }
}

#undef ClMobj_LocalActionsEnabled
dd_bool ClMobj_LocalActionsEnabled(mobj_t *mo)
{
    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(mo);
    if (!isClient || !info) return true;
    return (info->flags & CLMF_LOCAL_ACTIONS) != 0;
}

void ClMobj_SetState(mobj_t *mo, int stnum)
{
    if (stnum < 0) return;

    do
    {
        Mobj_SetState(mo, stnum);
        stnum = runtimeDefs.states[stnum].nextState;

    } while (!mo->tics && stnum > 0);
}

void Cl_UpdateRealPlayerMobj(mobj_t *localMobj, mobj_t *remoteClientMobj,
                             int flags, dd_bool onFloor)
{
    if (!localMobj || !remoteClientMobj)
    {
        LOGDEV_MAP_VERBOSE("Cl_UpdateRealPlayerMobj: mo=%p clmo=%p") << localMobj << remoteClientMobj;
        return;
    }

    localMobj->radius = Mobj_Radius(*remoteClientMobj);

    if (flags & MDF_MOM_X) localMobj->mom[MX] = remoteClientMobj->mom[MX];
    if (flags & MDF_MOM_Y) localMobj->mom[MY] = remoteClientMobj->mom[MY];
    if (flags & MDF_MOM_Z) localMobj->mom[MZ] = remoteClientMobj->mom[MZ];
    if (flags & MDF_ANGLE)
    {
        localMobj->angle = remoteClientMobj->angle;

        LOGDEV_MAP_XVERBOSE_DEBUGONLY("Cl_UpdateRealPlayerMobj: localMobj=%p angle=%x",
                                     localMobj << localMobj->angle);
    }
    localMobj->sprite = remoteClientMobj->sprite;
    localMobj->frame = remoteClientMobj->frame;
    //localMobj->nextframe = clmo->nextframe;
    localMobj->tics = remoteClientMobj->tics;
    localMobj->state = remoteClientMobj->state;
    //localMobj->nexttime = clmo->nexttime;
#define DDMF_KEEP_MASK (DDMF_REMOTE | DDMF_SOLID)
    localMobj->ddFlags = (localMobj->ddFlags & DDMF_KEEP_MASK) | (remoteClientMobj->ddFlags & ~DDMF_KEEP_MASK);
    if (flags & MDF_FLAGS)
    {
        localMobj->flags = (localMobj->flags & ~0x1c000000) |
                           (remoteClientMobj->flags & 0x1c000000); // color translation flags (MF_TRANSLATION)
        //DEBUG_Message(("UpdateRealPlayerMobj: translation=%i\n", (localMobj->flags >> 26) & 7));
    }

    localMobj->height = remoteClientMobj->height;
    localMobj->selector &= ~DDMOBJ_SELECTOR_MASK;
    localMobj->selector |= remoteClientMobj->selector & DDMOBJ_SELECTOR_MASK;
    localMobj->visAngle = remoteClientMobj->angle >> 16;

    if (flags & (MDF_ORIGIN_X | MDF_ORIGIN_Y))
    {
        /*
        // We have to unlink the real mobj before we move it.
        P_MobjUnlink(localMobj);
        localMobj->pos[VX] = remoteClientMobj->pos[VX];
        localMobj->pos[VY] = remoteClientMobj->pos[VY];
        P_MobjLink(localMobj, MLF_SECTOR | MLF_BLOCKMAP);
        */

        // This'll update the contacted floor and ceiling heights as well.
        if (gx.MobjTryMoveXYZ)
        {
            if (gx.MobjTryMoveXYZ(localMobj,
                                 remoteClientMobj->origin[VX],
                                 remoteClientMobj->origin[VY],
                                 (flags & MDF_ORIGIN_Z)? remoteClientMobj->origin[VZ] : localMobj->origin[VZ]))
            {
                if ((flags & MDF_ORIGIN_Z) && onFloor)
                {
                    localMobj->origin[VZ] = remoteClientMobj->origin[VZ] = localMobj->floorZ;
                }
            }
        }
    }
    if (flags & MDF_ORIGIN_Z)
    {
        if (!onFloor)
        {
            localMobj->floorZ = remoteClientMobj->floorZ;
        }
        localMobj->ceilingZ = remoteClientMobj->ceilingZ;

        localMobj->origin[VZ] = remoteClientMobj->origin[VZ];

        // Don't go below the floor level.
        if (localMobj->origin[VZ] < localMobj->floorZ)
            localMobj->origin[VZ] = localMobj->floorZ;
    }
}

dd_bool Cl_IsClientMobj(const mobj_t *mob)
{
    DE_ASSERT(mob);
    if (ClientMobjThinkerData *data = THINKER_DATA_MAYBE(mob->thinker, ClientMobjThinkerData))
    {
        return data->hasRemoteSync();
    }
    return false;
}

#undef ClMobj_IsValid
dd_bool ClMobj_IsValid(mobj_t *mob)
{
    if (!Cl_IsClientMobj(mob)) return true;

    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(mob);
    if (info->flags & (CLMF_HIDDEN | CLMF_UNPREDICTABLE))
    {
        // Should not use this for playsim.
        return false;
    }
    if (!mob->info)
    {
        // We haven't yet received info about the mobj's type?
        return false;
    }
    return true;
}

ClientMobjThinkerData::RemoteSync *ClMobj_GetInfo(mobj_t *mob)
{
    if (!mob) return nullptr;

    if (ClientMobjThinkerData *data = THINKER_DATA_MAYBE(mob->thinker, ClientMobjThinkerData))
    {
        if (!data->hasRemoteSync()) return nullptr;
        return &data->remoteSync();
    }

    return nullptr;
}

dd_bool ClMobj_Reveal(mobj_t *mob)
{
    LOG_AS("ClMobj_Reveal");

    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(mob);

    CL_ASSERT_CLMOBJ(mob);

    // Check that we know enough about the clmobj.
    if (mob->dPlayer != &DD_Player(consolePlayer)->publicData() &&
       (!(info->flags & CLMF_KNOWN_X) ||
        !(info->flags & CLMF_KNOWN_Y) ||
        //!(info->flags & CLMF_KNOWN_Z) ||
        !(info->flags & CLMF_KNOWN_STATE)))
    {
        // Don't reveal just yet. We lack a vital piece of information.
        return false;
    }

    LOGDEV_MAP_XVERBOSE("clmobj %i 'Hidden' status lifted (z=%f)", mob->thinker.id << mob->origin[VZ]);

    info->flags &= ~CLMF_HIDDEN;

    // Start a sound that has been queued for playing at the time
    // of unhiding. Sounds are queued if a sound delta arrives for an
    // object ID we don't know (yet).
    if (info->flags & CLMF_SOUND)
    {
        info->flags &= ~CLMF_SOUND;
        S_StartSoundAtVolume(info->sound, mob, info->volume);
    }

    LOGDEV_MAP_XVERBOSE("Revealing id %i, state %p (%i)",
                        mob->thinker.id << mob->state <<
                        ::runtimeDefs.states.indexOf(mob->state));

    return true;
}

/**
 * Determines whether @a mo happens to reside inside one of the local players.
 * In normal gameplay solid mobjs cannot enter inside each other.
 *
 * @param mo  Client mobj (must be solid).
 */
static dd_bool ClMobj_IsStuckInsideLocalPlayer(mobj_t *mo)
{
    if (!(mo->ddFlags & DDMF_SOLID) || mo->dPlayer)
        return false;

    for (int i = 0; i < DDMAXPLAYERS; ++i)
    {
        if (!DD_Player(i)->publicData().inGame) continue;
        if (P_ConsoleToLocal(i) < 0) continue; // Not a local player.

        mobj_t *plmo = DD_Player(i)->publicData().mo;
        if (!plmo) continue;

        float blockRadius = Mobj_Radius(*mo) + Mobj_Radius(*plmo);
        if (fabs(mo->origin[VX] - plmo->origin[VX]) >= blockRadius ||
           fabs(mo->origin[VY] - plmo->origin[VY]) >= blockRadius)
            continue; // Too far.

        if (mo->origin[VZ] > plmo->origin[VZ] + plmo->height)
            continue; // Above.

        if (plmo->origin[VZ] > mo->origin[VZ] + mo->height)
            continue; // Under.

        // Seems to be blocking the player...
        return true;
    }

    return false; // Not stuck.
}

void ClMobj_ReadDelta()
{
    /// @todo Do not assume the CURRENT map.
    Map &map = World::get().map().as<Map>();

    const thid_t id = Reader_ReadUInt16(msgReader); // Read the ID.
    const dint df   = Reader_ReadUInt16(msgReader); // Flags.

    // More flags?
    byte moreFlags = 0, fastMom = false;
    if (df & MDF_MORE_FLAGS)
    {
        moreFlags = Reader_ReadByte(msgReader);

        // Fast momentum uses 10.6 fixed point instead of the normal 8.8.
        if (moreFlags & MDFE_FAST_MOM)
            fastMom = true;
    }

    LOGDEV_NET_XVERBOSE("Reading mobj delta for %i (df:0x%x edf:0x%x)",
                        id << df << moreFlags);

    // Get the client mobj for this.
    mobj_t *mo = map.clMobjFor(id);
    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(mo);
    bool needsLinking = false, justCreated = false;
    if (!mo)
    {
        LOG_NET_XVERBOSE("Creating new clmobj %i (hidden)", id);

        // This is a new ID, allocate a new mobj.
        mo = map.clMobjFor(id, true /* create it now */);
        info = ClMobj_GetInfo(mo);
        justCreated = true;
        needsLinking = true;

        // Always create new mobjs as hidden. They will be revealed when
        // we know enough about them.
        info->flags |= CLMF_HIDDEN;
    }

    if (!(info->flags & CLMF_NULLED))
    {
        // Now that we've received a delta, the mobj's Predictable again.
        info->flags &= ~CLMF_UNPREDICTABLE;

        // This clmobj is evidently alive.
        info->time = Timer_RealMilliseconds();
    }

    mobj_t *d = mo;

    /*if (d->dPlayer && d->dPlayer == DD_Player(consolePlayer))
    {
        // Mark the local player known.
        cmo->flags |= CLMF_KNOWN;
    }*/

    // Need to unlink? (Flags because DDMF_SOLID determines block-linking.)
    if (df & (MDF_ORIGIN_X | MDF_ORIGIN_Y | MDF_ORIGIN_Z | MDF_FLAGS) &&
       !justCreated && !d->dPlayer)
    {
        needsLinking = true;
        Mobj_Unlink(mo);
    }

    // Remember where the mobj used to be in case we need to cancel a move.
    //mobj_t oldState; zap(oldState);
    MobjThinker oldState(*mo);
    bool onFloor = false;

    // Coordinates with three bytes.
    if (df & MDF_ORIGIN_X)
    {
        d->origin[VX] = FIX2FLT((Reader_ReadInt16(msgReader) << FRACBITS) | (Reader_ReadByte(msgReader) << 8));
        if (info)
            info->flags |= CLMF_KNOWN_X;
    }
    if (df & MDF_ORIGIN_Y)
    {
        d->origin[VY] = FIX2FLT((Reader_ReadInt16(msgReader) << FRACBITS) | (Reader_ReadByte(msgReader) << 8));
        if (info)
            info->flags |= CLMF_KNOWN_Y;
    }
    if (df & MDF_ORIGIN_Z)
    {
        if (!(moreFlags & MDFE_Z_FLOOR))
        {
            d->origin[VZ] = FIX2FLT((Reader_ReadInt16(msgReader) << FRACBITS) | (Reader_ReadByte(msgReader) << 8));
            if (info)
            {
                info->flags |= CLMF_KNOWN_Z;

                // The mobj won't stick if an explicit coordinate is supplied.
                info->flags &= ~(CLMF_STICK_FLOOR | CLMF_STICK_CEILING);
            }
            d->floorZ = Reader_ReadFloat(msgReader);
        }
        else
        {
            onFloor = true;

            // Ignore these.
            Reader_ReadInt16(msgReader);
            Reader_ReadByte(msgReader);
            Reader_ReadFloat(msgReader);

            info->flags |= CLMF_KNOWN_Z;
            //d->pos[VZ] = d->floorZ;
        }

        d->ceilingZ = Reader_ReadFloat(msgReader);
    }

    // Momentum using 8.8 fixed point.
    if (df & MDF_MOM_X)
    {
        short mom = Reader_ReadInt16(msgReader);
        d->mom[MX] = FIX2FLT(fastMom? UNFIXED10_6(mom) : UNFIXED8_8(mom));
    }
    if (df & MDF_MOM_Y)
    {
        short mom = Reader_ReadInt16(msgReader);
        d->mom[MY] = FIX2FLT(fastMom ? UNFIXED10_6(mom) : UNFIXED8_8(mom));
    }
    if (df & MDF_MOM_Z)
    {
        short mom = Reader_ReadInt16(msgReader);
        d->mom[MZ] = FIX2FLT(fastMom ? UNFIXED10_6(mom) : UNFIXED8_8(mom));
    }

    // Angles with 16-bit accuracy.
    if (df & MDF_ANGLE)
        d->angle = Reader_ReadInt16(msgReader) << 16;

    // MDF_SELSPEC is never used without MDF_SELECTOR.
    if (df & MDF_SELECTOR)
        d->selector = Reader_ReadPackedUInt16(msgReader);
    if (df & MDF_SELSPEC)
        d->selector |= Reader_ReadByte(msgReader) << 24;

    if (df & MDF_STATE)
    {
        int stateIdx = Reader_ReadPackedUInt16(msgReader);

        // Translate.
        stateIdx = Cl_LocalMobjState(stateIdx);

        // When local actions are allowed, the assumption is that
        // the client will be doing the state changes.
        if (!(info->flags & CLMF_LOCAL_ACTIONS))
        {
            ClMobj_SetState(d, stateIdx);
            info->flags |= CLMF_KNOWN_STATE;
        }
    }

    if (df & MDF_FLAGS)
    {
        // Only the flags in the pack mask are affected.
        d->ddFlags &= ~DDMF_PACK_MASK;
        d->ddFlags |= DDMF_REMOTE | (Reader_ReadUInt32(msgReader) & DDMF_PACK_MASK);

        d->flags  = Reader_ReadUInt32(msgReader);
        d->flags2 = Reader_ReadUInt32(msgReader);
        d->flags3 = Reader_ReadUInt32(msgReader);
    }

    if (df & MDF_HEALTH)
        d->health = Reader_ReadInt32(msgReader);

    if (df & MDF_RADIUS)
        d->radius = Reader_ReadFloat(msgReader);

    if (df & MDF_HEIGHT)
        d->height = Reader_ReadFloat(msgReader);

    if (df & MDF_FLOORCLIP)
        d->floorClip = Reader_ReadFloat(msgReader);

    if (moreFlags & MDFE_TRANSLUCENCY)
        d->translucency = Reader_ReadByte(msgReader);

    if (moreFlags & MDFE_FADETARGET)
        d->visTarget = ((short)Reader_ReadByte(msgReader)) - 1;

    if (moreFlags & MDFE_TYPE)
    {
        d->type = Cl_LocalMobjType(Reader_ReadInt32(msgReader));
        d->info = &runtimeDefs.mobjInfo[d->type];
    }

    // Is it time to remove the Hidden status?
    if (info->flags & CLMF_HIDDEN)
    {
        // Now it can be displayed (potentially).
        if (ClMobj_Reveal(d))
        {
            // Now it can be linked to the world.
            needsLinking = true;
        }
    }

    // Non-player mobjs: update the Z position to be on the local floor, which may be
    // different than the server-side floor.
    if (!d->dPlayer && onFloor && gx.MobjCheckPositionXYZ)
    {
        if (coord_t *floorZ = (coord_t *) gx.GetPointer(DD_TM_FLOOR_Z))
        {
            gx.MobjCheckPositionXYZ(d, d->origin[VX], d->origin[VY], DDMAXFLOAT);
            d->origin[VZ] = d->floorZ = *floorZ;
        }
    }

    // If the clmobj is Hidden (or Nulled), it will not be linked back to
    // the world until it's officially Created. (Otherwise, partially updated
    // mobjs may be visible for a while.)
    if (!(info->flags & (CLMF_HIDDEN | CLMF_NULLED)))
    {
        // Link again.
        if (needsLinking && !d->dPlayer)
        {
            ClMobj_Link(mo);

            if (ClMobj_IsStuckInsideLocalPlayer(mo))
            {
                // Oopsie, on second thought we shouldn't do this move.
                Mobj_Unlink(mo);
                V3d_Copy(mo->origin, oldState->origin);
                mo->floorZ = oldState->floorZ;
                mo->ceilingZ = oldState->ceilingZ;
                ClMobj_Link(mo);
            }
        }

        // Update players.
        if (d->dPlayer)
        {
            LOG_NET_XVERBOSE("Updating player %i local mobj with new clmobj state {%f, %f, %f}",
                             P_GetDDPlayerIdx(d->dPlayer) <<
                             d->origin[VX] << d->origin[VY] << d->origin[VZ]);

            // Players have real mobjs. The client mobj is hidden (unlinked).
            Cl_UpdateRealPlayerMobj(d->dPlayer->mo, d, df, onFloor);
        }
    }
}

void ClMobj_ReadNullDelta()
{
    LOG_AS("ClMobj_ReadNullDelta");

    /// @todo Do not assume the CURRENT map.
    auto &map = World::get().map().as<Map>();

    // The delta only contains an ID.
    thid_t id = Reader_ReadUInt16(msgReader);
    LOGDEV_NET_XVERBOSE("Null %i", id);

    mobj_t *mo = map.clMobjFor(id);
    if (!mo)
    {
        // Wasted bandwidth...
        LOGDEV_NET_MSG("Request to remove id %i that doesn't exist here") << id;
        return;
    }

    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(mo);

    // Get rid of this mobj.
    if (!mo->dPlayer)
    {
        Mobj_Unlink(mo);
    }
    else
    {
        LOGDEV_NET_MSG("clmobj of player %i deleted") << P_GetDDPlayerIdx(mo->dPlayer);

        // The clmobjs of players aren't linked.
        ClPlayer_State(P_GetDDPlayerIdx(mo->dPlayer))->clMobjId = 0;
    }

    // This'll allow playing sounds from the mobj for a little while.
    // The mobj will soon time out and be permanently removed.
    info->time = Timer_RealMilliseconds();
    info->flags |= CLMF_UNPREDICTABLE | CLMF_NULLED;
}

#undef ClMobj_Find
mobj_t *ClMobj_Find(thid_t id)
{
    if (!world::World::get().hasMap()) return nullptr;

    /// @todo Do not assume the CURRENT map.
    return World::get().map().as<Map>().clMobjFor(id);
}

// cl_player.c
#undef ClPlayer_ClMobj
DE_EXTERN_C mobj_t *ClPlayer_ClMobj(int plrNum);

DE_DECLARE_API(Client) =
{
    { DE_API_CLIENT },
    ClMobj_Find,
    ClMobj_EnableLocalActions,
    ClMobj_LocalActionsEnabled,
    ClMobj_IsValid,
    ClPlayer_ClMobj
};
