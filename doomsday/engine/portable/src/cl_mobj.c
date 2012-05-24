/**
 * @file cl_mobj.c
 * Client map objects. @ingroup client
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

#include <math.h>

#include "de_base.h"
#include "de_defs.h"
#include "de_system.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_audio.h"

/// Convert 8.8/10.6 fixed point to 16.16.
#define UNFIXED8_8(x)   (((x) << 16) / 256)
#define UNFIXED10_6(x)  (((x) << 16) / 64)

/// Milliseconds it takes for Unpredictable and Hidden mobjs to be
/// removed from the hash. Under normal circumstances, the special
/// status should be removed fairly quickly.
#define CLMOBJ_TIMEOUT  4000    ///< milliseconds

/// Missiles don't hit mobjs only after a short delay. This'll
/// allow the missile to move free of the shooter. (Quite a hack!)
#define MISSILE_FREE_MOVE_TIME  1000

extern int gotFrame; ///< @todo Remove this...

/**
 * @return  Pointer to the hash chain with the specified id.
 */
static cmhash_t* GameMap_ClMobjHash(GameMap* map, thid_t id)
{
    assert(map);
    return &map->clMobjHash[(uint) id % CLIENT_MOBJ_HASH_SIZE];
}

/// @note Part of the Doomsday public API.
static cmhash_t* ClMobj_Hash(thid_t id)
{
    if(!theMap) return NULL;
    return GameMap_ClMobjHash(theMap, id);
}

#ifdef _DEBUG
void checkMobjHash(void)
{
    int i;
    assert(theMap != 0);
    for(i = 0; i < CLIENT_MOBJ_HASH_SIZE; ++i)
    {
        int count1 = 0, count2 = 0;
        clmoinfo_t* info;
        for(info = theMap->clMobjHash[i].first; info; info = info->next, count1++)
        {
            assert(ClMobj_MobjForInfo(info) != 0);
        }
        for(info = theMap->clMobjHash[i].last; info; info = info->prev, count2++)
        {
            assert(ClMobj_MobjForInfo(info) != 0);
        }
        assert(count1 == count2);
    }
}
#endif

/**
 * Links the clmobj into the client mobj hash table.
 */
static void ClMobj_LinkInHash(mobj_t* mo, thid_t id)
{
    /// @todo Do not assume the CURRENT map.
    cmhash_t* hash = GameMap_ClMobjHash(theMap, id);
    clmoinfo_t* info = ClMobj_GetInfo(mo);

    CL_ASSERT_CLMOBJ(mo);

#ifdef _DEBUG
    checkMobjHash();
#endif

    // Set the ID.
    mo->thinker.id = id;
    info->next = NULL;

    if(hash->last)
    {
        hash->last->next = info;
        info->prev = hash->last;
    }
    hash->last = info;

    if(!hash->first)
    {
        hash->first = info;
    }

#ifdef _DEBUG
    checkMobjHash();
#endif
}

/**
 * Unlinks the clmobj from the client mobj hash table.
 */
static void ClMobj_UnlinkInHash(mobj_t* mo)
{
    cmhash_t* hash = ClMobj_Hash(mo->thinker.id);
    clmoinfo_t* info = ClMobj_GetInfo(mo);

    CL_ASSERT_CLMOBJ(mo);

#ifdef _DEBUG
    checkMobjHash();
#endif

    if(hash->first == info)
        hash->first = info->next;
    if(hash->last == info)
        hash->last = info->prev;
    if(info->next)
        info->next->prev = info->prev;
    if(info->prev)
        info->prev->next = info->next;

#ifdef _DEBUG
    checkMobjHash();
#endif
}

mobj_t* ClMobj_MobjForInfo(clmoinfo_t* info)
{
    assert(info->startMagic == CLM_MAGIC1);
    assert(info->endMagic == CLM_MAGIC2);

    return (mobj_t*) ((char*)info + sizeof(clmoinfo_t));
}

struct mobj_s* ClMobj_Find(thid_t id)
{
    cmhash_t* hash = ClMobj_Hash(id);
    clmoinfo_t* info;

    if(!id) return NULL;

    // Scan the existing client mobjs.
    for(info = hash->first; info; info = info->next)
    {
        mobj_t* mo = ClMobj_MobjForInfo(info);
        if(mo->thinker.id == id)
            return mo;
    }

    // Not found!
    return NULL;
}

boolean GameMap_ClMobjIterator(GameMap* map, boolean (*callback) (mobj_t*, void*), void* parm)
{
    clmoinfo_t* info;
    int i;
    assert(map);

    for(i = 0; i < CLIENT_MOBJ_HASH_SIZE; ++i)
    {
        for(info = map->clMobjHash[i].first; info; info = info->next)
        {
            if(!callback(ClMobj_MobjForInfo(info), parm))
                return false;
        }
    }
    return true;
}

/**
 * Unlinks the mobj from sectorlinks and if the object is solid,
 * the blockmap.
 */
void ClMobj_Unlink(mobj_t* mo)
{
    P_MobjUnlink(mo);
}

/**
 * Links the mobj into sectorlinks and if the object is solid, the
 * blockmap. Linking to sectorlinks makes the mobj visible and linking
 * to the blockmap makes it possible to interact with it (collide).
 * If the client mobj is Hidden, it will not be linked anywhere.
 */
void ClMobj_Link(mobj_t* mo)
{
    clmoinfo_t* info = ClMobj_GetInfo(mo);

    CL_ASSERT_CLMOBJ(mo);

    if((info->flags & (CLMF_HIDDEN | CLMF_UNPREDICTABLE)) || mo->dPlayer)
    {
        // We do not yet have all the details about Hidden mobjs.
        // The server hasn't sent us a Create Mobj delta for them.
        // Client mobjs that belong to players remain unlinked.
        return;
    }
    DEBUG_VERBOSE2_Message(("ClMobj_Link: id %i, x%f Y%f, solid:%s\n", mo->thinker.id,
                            mo->origin[VX], mo->origin[VY], mo->ddFlags & DDMF_SOLID? "yes" : "no"));

    P_MobjLink(mo, (mo->ddFlags & DDMF_DONTDRAW ? 0 : DDLINK_SECTOR) |
                   (mo->ddFlags & DDMF_SOLID ? DDLINK_BLOCKMAP : 0));
}

void ClMobj_EnableLocalActions(struct mobj_s *mo, boolean enable)
{
    clmoinfo_t* info = ClMobj_GetInfo(mo);
    if(!isClient || !info) return;
    if(enable)
    {
        DEBUG_Message(("ClMobj_EnableLocalActions: Enabled for mobj %i.\n", mo->thinker.id));
        info->flags |= CLMF_LOCAL_ACTIONS;
    }
    else
    {
        DEBUG_Message(("ClMobj_EnableLocalActions: Disabled for mobj %i.\n", mo->thinker.id));
        info->flags &= ~CLMF_LOCAL_ACTIONS;
    }
}

boolean ClMobj_LocalActionsEnabled(struct mobj_s *mo)
{
    clmoinfo_t* info = ClMobj_GetInfo(mo);
    if(!isClient || !info) return true;
    return (info->flags & CLMF_LOCAL_ACTIONS) != 0;
}

/**
 * Change the state of a mobj.
 *
 * @todo  Should use the gameside function for this?
 */
void ClMobj_SetState(mobj_t *mo, int stnum)
{
    if(stnum < 0)
        return;
    do
    {
        P_MobjSetState(mo, stnum);
        stnum = states[stnum].nextState;
    }
    while(!mo->tics && stnum > 0);
}

/**
 * Make the real player mobj identical with the client mobj.
 * The client mobj is always unlinked. Only the *real* mobj is visible.
 * (The real mobj was created by the Game.)
 */
void Cl_UpdateRealPlayerMobj(mobj_t *localMobj, mobj_t *remoteClientMobj, int flags, boolean onFloor)
{
    if(!localMobj || !remoteClientMobj)
    {
        DEBUG_VERBOSE_Message(("Cl_UpdateRealPlayerMobj: mo=%p clmo=%p\n", localMobj, remoteClientMobj));
        return;
    }

    localMobj->radius = remoteClientMobj->radius;

    if(flags & MDF_MOM_X) localMobj->mom[MX] = remoteClientMobj->mom[MX];
    if(flags & MDF_MOM_Y) localMobj->mom[MY] = remoteClientMobj->mom[MY];
    if(flags & MDF_MOM_Z) localMobj->mom[MZ] = remoteClientMobj->mom[MZ];
    if(flags & MDF_ANGLE)
    {
        localMobj->angle = remoteClientMobj->angle;
#ifdef _DEBUG
        VERBOSE2( Con_Message("Cl_UpdateRealPlayerMobj: localMobj=%p angle=%x\n", localMobj, localMobj->angle) );
#endif
    }
    localMobj->sprite = remoteClientMobj->sprite;
    localMobj->frame = remoteClientMobj->frame;
    //localMobj->nextframe = clmo->nextframe;
    localMobj->tics = remoteClientMobj->tics;
    localMobj->state = remoteClientMobj->state;
    //localMobj->nexttime = clmo->nexttime;
#define DDMF_KEEP_MASK (DDMF_REMOTE | DDMF_SOLID)
    localMobj->ddFlags = (localMobj->ddFlags & DDMF_KEEP_MASK) | (remoteClientMobj->ddFlags & ~DDMF_KEEP_MASK);
    localMobj->flags = (localMobj->flags & ~0x1c000000) |
                       (remoteClientMobj->flags & 0x1c000000); // color translation flags (MF_TRANSLATION)
    localMobj->height = remoteClientMobj->height;
/*#ifdef _DEBUG
    if(localMobj->floorClip != remoteClientMobj->floorClip)
    {
        Con_Message("Cl_UpdateRealPlayerMobj: Floorclip=%f\n", remoteClientMobj->floorClip);
    }
#endif
    localMobj->floorClip = remoteClientMobj->floorClip;*/
    localMobj->selector &= ~DDMOBJ_SELECTOR_MASK;
    localMobj->selector |= remoteClientMobj->selector & DDMOBJ_SELECTOR_MASK;
    localMobj->visAngle = remoteClientMobj->angle >> 16;

    if(flags & (MDF_ORIGIN_X | MDF_ORIGIN_Y))
    {
        /*
        // We have to unlink the real mobj before we move it.
        P_MobjUnlink(localMobj);
        localMobj->pos[VX] = remoteClientMobj->pos[VX];
        localMobj->pos[VY] = remoteClientMobj->pos[VY];
        P_MobjLink(localMobj, DDLINK_SECTOR | DDLINK_BLOCKMAP);
        */

        // This'll update the contacted floor and ceiling heights as well.
        if(gx.MobjTryMoveXYZ)
        {
            if(gx.MobjTryMoveXYZ(localMobj,
                                remoteClientMobj->origin[VX],
                                remoteClientMobj->origin[VY],
                                (flags & MDF_ORIGIN_Z)? remoteClientMobj->origin[VZ] : localMobj->origin[VZ]))
            {
                if((flags & MDF_ORIGIN_Z) && onFloor)
                {
                    localMobj->origin[VZ] = remoteClientMobj->origin[VZ] = localMobj->floorZ;
                }
            }
        }
    }
    if(flags & MDF_ORIGIN_Z)
    {
        if(!onFloor)
        {
            localMobj->floorZ = remoteClientMobj->floorZ;
        }
        localMobj->ceilingZ = remoteClientMobj->ceilingZ;

        localMobj->origin[VZ] = remoteClientMobj->origin[VZ];

        // Don't go below the floor level.
        if(localMobj->origin[VZ] < localMobj->floorZ)
            localMobj->origin[VZ] = localMobj->floorZ;
    }
}

void GameMap_InitClMobjs(GameMap* map)
{
    assert(map);
    memset(map->clMobjHash, 0, sizeof(map->clMobjHash));
}

void GameMap_DestroyClMobjs(GameMap* map)
{
    clmoinfo_t* info;
    int i;
    assert(map);

    for(i = 0; i < CLIENT_MOBJ_HASH_SIZE; ++i)
    {
        for(info = map->clMobjHash[i].first; info; info = info->next)
        {
            mobj_t* mo = ClMobj_MobjForInfo(info);
            // Players' clmobjs are not linked anywhere.
            if(!mo->dPlayer)
                ClMobj_Unlink(mo);
        }
    }

    GameMap_ClMobjReset(map);
}

void GameMap_ClMobjReset(GameMap* map)
{
    assert(map);

    Cl_ResetFrame();

    // The PU_MAP memory was freed, so just clear the hash.
    memset(map->clMobjHash, 0, sizeof(map->clMobjHash));
}

void GameMap_ExpireClMobjs(GameMap* map)
{
    uint nowTime = Sys_GetRealTime();
    clmoinfo_t* info;
    clmoinfo_t* next = 0;
    mobj_t* mo;
    int i;
    assert(map);

    // Move all client mobjs.
    for(i = 0; i < CLIENT_MOBJ_HASH_SIZE; ++i)
    {
        for(info = map->clMobjHash[i].first; info; info = next)
        {
            next = info->next;
            mo = ClMobj_MobjForInfo(info);

            // Already deleted?
            if(mo->thinker.function == (think_t)-1) continue;

            // Don't expire player mobjs.
            if(mo->dPlayer) continue;

            if((info->flags & (CLMF_UNPREDICTABLE | CLMF_HIDDEN | CLMF_NULLED)) || !mo->info)
            {
                // Has this mobj timed out?
                if(nowTime - info->time > CLMOBJ_TIMEOUT)
                {
#ifdef _DEBUG
                    Con_Message("GameMap_ExpireClMobjs: Mobj %i has expired (%i << %i), in state %s [%c%c%c].\n",
                                mo->thinker.id,
                                info->time, nowTime,
                                Def_GetStateName(mo->state),
                                info->flags & CLMF_UNPREDICTABLE? 'U' : '_',
                                info->flags & CLMF_HIDDEN? 'H' : '_',
                                info->flags & CLMF_NULLED? '0' : '_');
#endif
                    // Too long. The server will probably never send anything
                    // for this mobj, so get rid of it. (Both unpredictable
                    // and hidden mobjs are not visible or bl/seclinked.)
                    P_MobjDestroy(mo);
                }
            }
        }
    }
}

/**
 * All client mobjs are moved and animated using the data we have.
 */
void Cl_PredictMovement(void)
{
#if 0
    clmobj_t           *cmo, *next = NULL;
    int                 i;
    int                 moCount = 0;
    uint                nowTime = Sys_GetRealTime();

    predicted_tics++;

    // Move all client mobjs.
    for(i = 0; i < CLIENT_MOBJ_HASH_SIZE; ++i)
    {
        for(cmo = cmHash[i].first; cmo; cmo = next)
        {
            next = cmo->next;
            moCount++;

            if(cmo->mo.dPlayer != &ddPlayers[consolePlayer].shared &&
               (cmo->flags & (CLMF_UNPREDICTABLE | CLMF_HIDDEN)) /*||
               (cmo->mo.ddFlags & DDMF_MISSILE)*/)
            {
                // Has this mobj timed out?
                if(nowTime - cmo->time > CLMOBJ_TIMEOUT)
                {
                    // Too long. The server will probably never send anything
                    // for this mobj, so get rid of it. (Both unpredictable
                    // and hidden mobjs are not visible or bl/seclinked.)
                    Cl_DestroyMobj(cmo);
                }
                // We can't predict what Hidden and Unpredictable mobjs do.
                // Must wait for the next delta to arrive. This mobj won't be
                // visible until then.
                continue;
            }

            // The local player is moved by the common game logic.
            if(!cmo->mo.dPlayer)
            {
                // Linear movement prediction with collisions, then.
                Cl_MobjMove(cmo);
            }

            // Tic away.
            Cl_MobjAnimate(&cmo->mo);

            // Remove mobjs who have reached the NULL state, from whose
            // bourn no traveller returns.
            if(cmo->mo.state == states)
            {
#ifdef _DEBUG
                if(!cmo->mo.thinker.id) Con_Error("No clmobj id!!!!\n");
#endif
                Cl_DestroyMobj(cmo);
                continue;
            }

            // Update the visual angle of the mobj (no SRVO action).
            cmo->mo.visAngle = cmo->mo.angle >> 16;
        }
    }
#ifdef _DEBUG
    if(verbose >= 2)
    {
        static int timer = 0;

        if(++timer > 5 * 35)
        {
            timer = 0;
            Con_Printf("moCount=%i\n", moCount);
        }
    }
#endif
#endif
}

/**
 * Create a new client mobj.
 *
 * Memory layout of a client mobj:
 * - client mobj magic1 (4 bytes)
 * - engineside clmobj info
 * - client mobj magic2 (4 bytes)
 * - gameside mobj (mobjSize bytes) <- this is returned from the function
 *
 * To check whether a given mobj_t is a clmobj_t, just check the presence of
 * the client mobj magic number (by calling Cl_IsClientMobj()).
 * The clmoinfo_s can then be accessed with ClMobj_GetInfo().
 *
 * @param id  Identifier of the client mobj. Every client mobj has a unique
 *            identifier.
 *
 * @return  Pointer to the gameside mobj.
 */
mobj_t* ClMobj_Create(thid_t id)
{
    void* data = 0;
    clmoinfo_t* info = 0;
    mobj_t* mo = 0; // Gameside object.

    // Allocate enough memory for all the data.
    info = data = Z_Calloc(sizeof(clmoinfo_t) + MOBJ_SIZE, PU_MAP, 0);
    mo = (mobj_t*) ((char*)data + sizeof(clmoinfo_t));

    // Initialize the data.
    info->time = Sys_GetRealTime();
    info->startMagic = CLM_MAGIC1;
    info->endMagic = CLM_MAGIC2;
    mo->ddFlags = DDMF_REMOTE;

    ClMobj_LinkInHash(mo, id);
    GameMap_SetMobjID(theMap, id, true); // Mark this ID as used.

    // Client mobjs are full-fludged game mobjs as well.
    mo->thinker.function = gx.MobjThinker;
    GameMap_ThinkerAdd(theMap, (thinker_t*) mo, true);

    return mo;
}

/**
 * Destroys the client mobj. Before this is called, the client mobj should be
 * unlinked from the thinker list (GameMap_ThinkerRemove).
 */
void ClMobj_Destroy(mobj_t* mo)
{
    clmoinfo_t* info = 0;

#ifdef _DEBUG
    checkMobjHash();
#endif

    DEBUG_VERBOSE2_Message(("ClMobj_Destroy: mobj %i being destroyed.\n", mo->thinker.id));

    CL_ASSERT_CLMOBJ(mo);
    info = ClMobj_GetInfo(mo);

    // Stop any sounds originating from this mobj.
    S_StopSound(0, mo);

    // The ID is free once again.
    GameMap_SetMobjID(theMap, mo->thinker.id, false);
    ClMobj_UnlinkInHash(mo);
    ClMobj_Unlink(mo); // from block/sector

    // This will free the entire mobj + info.
    Z_Free(info);

#ifdef _DEBUG
    checkMobjHash();
#endif
}

/**
 * Determines whether a mobj is a client mobj.
 *
 * @param mo  Mobj to check.
 *
 * @return  @c true, if the mobj is a client mobj; otherwise @c false.
 */
boolean Cl_IsClientMobj(mobj_t* mo)
{
    return ClMobj_GetInfo(mo) != 0;
}

/**
 * Determines whether a client mobj is valid for playsim.
 * The primary reason for it not to be valid is that we haven't received
 * enough information about it yet.
 *
 * @param mo  Mobj to check.
 *
 * @return  @c true, if the mobj is a client mobj; otherwise @c false.
 */
boolean ClMobj_IsValid(mobj_t* mo)
{
    clmoinfo_t* info = ClMobj_GetInfo(mo);

    if(!Cl_IsClientMobj(mo)) return true;
    if(info->flags & (CLMF_HIDDEN | CLMF_UNPREDICTABLE))
    {
        // Should not use this for playsim.
        return false;
    }
    if(!mo->info)
    {
        // We haven't yet received info about the mobj's type?
        return false;
    }
    return true;
}

clmoinfo_t* ClMobj_GetInfo(mobj_t* mo)
{
    clmoinfo_t* info = (clmoinfo_t*) ((char*)mo - sizeof(clmoinfo_t));
    if(!mo || info->startMagic != CLM_MAGIC1 || info->endMagic != CLM_MAGIC2)
    {
        // There is no valid info block preceding the mobj.
        return 0;
    }
    return info;
}

/**
 * Call for Hidden client mobjs to make then visible.
 * If a sound is waiting, it's now played.
 *
 * @return              @c true, if the mobj was revealed.
 */
boolean ClMobj_Reveal(mobj_t *mo)
{
    clmoinfo_t* info = ClMobj_GetInfo(mo);

    CL_ASSERT_CLMOBJ(mo);

    // Check that we know enough about the clmobj.
    if(mo->dPlayer != &ddPlayers[consolePlayer].shared &&
       (!(info->flags & CLMF_KNOWN_X) ||
        !(info->flags & CLMF_KNOWN_Y) ||
        //!(info->flags & CLMF_KNOWN_Z) ||
        !(info->flags & CLMF_KNOWN_STATE)))
    {
        // Don't reveal just yet. We lack a vital piece of information.
        return false;
    }
#ifdef _DEBUG
    VERBOSE2( Con_Message("Cl_RevealMobj: clmobj %i Hidden status lifted (z=%f).\n", mo->thinker.id, mo->origin[VZ]) );
#endif

    info->flags &= ~CLMF_HIDDEN;

    // Start a sound that has been queued for playing at the time
    // of unhiding. Sounds are queued if a sound delta arrives for an
    // object ID we don't know (yet).
    if(info->flags & CLMF_SOUND)
    {
        info->flags &= ~CLMF_SOUND;
        S_StartSoundAtVolume(info->sound, mo, info->volume);
    }

#ifdef _DEBUG
    VERBOSE2( Con_Printf("Cl_RevealMobj: Revealing id %i, state %p (%i)\n",
                         mo->thinker.id, mo->state, (int)(mo->state - states)) );
#endif

    return true;
}

/**
 * Determines whether @a mo happens to reside inside one of the local players.
 * In normal gameplay solid mobjs cannot enter inside each other.
 *
 * @param mo  Client mobj (must be solid).
 */
static boolean ClMobj_IsStuckInsideLocalPlayer(mobj_t* mo)
{
    int i;
    mobj_t* plmo;
    float blockRadius;

    if(!(mo->ddFlags & DDMF_SOLID) || mo->dPlayer)
        return false;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(!ddPlayers[i].shared.inGame) continue;
        if(P_ConsoleToLocal(i) < 0) continue; // Not a local player.

        plmo = ddPlayers[i].shared.mo;
        if(!plmo) continue;

        blockRadius = mo->radius + plmo->radius;
        if(fabs(mo->origin[VX] - plmo->origin[VX]) >= blockRadius ||
           fabs(mo->origin[VY] - plmo->origin[VY]) >= blockRadius)
            continue; // Too far.

        if(mo->origin[VZ] > plmo->origin[VZ] + plmo->height)
            continue; // Above.

        if(plmo->origin[VZ] > mo->origin[VZ] + mo->height)
            continue; // Under.

        // Seems to be blocking the player...
        return true;
    }

    return false; // Not stuck.
}

void ClMobj_ReadDelta2(boolean skip)
{
    boolean     needsLinking = false, justCreated = false;
    clmoinfo_t *info = 0;
    mobj_t     *mo = 0;
    mobj_t     *d;
    static mobj_t dummy;
    int         df = 0;
    byte        moreFlags = 0, fastMom = false;
    short       mom;
    thid_t      id = Reader_ReadUInt16(msgReader);   // Read the ID.
    boolean     onFloor = false;
    mobj_t      oldState;

    // Flags.
    df = Reader_ReadUInt16(msgReader);

    // More flags?
    if(df & MDF_MORE_FLAGS)
    {
        moreFlags = Reader_ReadByte(msgReader);

        // Fast momentum uses 10.6 fixed point instead of the normal 8.8.
        if(moreFlags & MDFE_FAST_MOM)
            fastMom = true;
    }

#ifdef _DEBUG
    VERBOSE2( Con_Message("Cl_ReadMobjDelta: Reading mobj delta for %i (df:0x%x edf:0x%x skip:%i)\n", id, df, moreFlags, skip) );
#endif

    if(!skip)
    {
        // Get a mobj for this.
        mo = ClMobj_Find(id);
        info = ClMobj_GetInfo(mo);
        if(!mo)
        {
#ifdef _DEBUG
            VERBOSE2( Con_Message("Cl_ReadMobjDelta: Creating new clmobj %i (hidden).\n", id) );
#endif

            // This is a new ID, allocate a new mobj.
            mo = ClMobj_Create(id);
            info = ClMobj_GetInfo(mo);
            justCreated = true;
            needsLinking = true;

            // Always create new mobjs as hidden. They will be revealed when
            // we know enough about them.
            info->flags |= CLMF_HIDDEN;
        }

        if(!(info->flags & CLMF_NULLED))
        {
            // Now that we've received a delta, the mobj's Predictable again.
            info->flags &= ~CLMF_UNPREDICTABLE;

            // This clmobj is evidently alive.
            info->time = Sys_GetRealTime();
        }

        d = mo;

        /*if(d->dPlayer && d->dPlayer == &ddPlayers[consolePlayer])
        {
            // Mark the local player known.
            cmo->flags |= CLMF_KNOWN;
        }*/

        // Need to unlink? (Flags because DDMF_SOLID determines block-linking.)
        if(df & (MDF_ORIGIN_X | MDF_ORIGIN_Y | MDF_ORIGIN_Z | MDF_FLAGS) &&
           !justCreated && !d->dPlayer)
        {
            needsLinking = true;
            ClMobj_Unlink(mo);
        }
    }
    else
    {
        // We're skipping.
        d = &dummy;
        info = 0;
    }

    // Remember where the mobj used to be in case we need to cancel a move.
    memcpy(&oldState, d, sizeof(mobj_t));

    // Coordinates with three bytes.
    if(df & MDF_ORIGIN_X)
    {
        d->origin[VX] = FIX2FLT((Reader_ReadInt16(msgReader) << FRACBITS) | (Reader_ReadByte(msgReader) << 8));
        if(info)
            info->flags |= CLMF_KNOWN_X;
    }
    if(df & MDF_ORIGIN_Y)
    {
        d->origin[VY] = FIX2FLT((Reader_ReadInt16(msgReader) << FRACBITS) | (Reader_ReadByte(msgReader) << 8));
        if(info)
            info->flags |= CLMF_KNOWN_Y;
    }
    if(df & MDF_ORIGIN_Z)
    {
        if(!(moreFlags & MDFE_Z_FLOOR))
        {
            d->origin[VZ] = FIX2FLT((Reader_ReadInt16(msgReader) << FRACBITS) | (Reader_ReadByte(msgReader) << 8));
            if(info)
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

/*#if _DEBUG
    if((df & MDF_ORIGIN_Z) && d->dPlayer && P_GetDDPlayerIdx(d->dPlayer) != consolePlayer)
    {
        Con_Message("ClMobj_ReadDelta2: Player=%i z=%f onFloor=%i\n", P_GetDDPlayerIdx(d->dPlayer),
                    d->pos[VZ], onFloor);
    }
#endif*/

    // Momentum using 8.8 fixed point.
    if(df & MDF_MOM_X)
    {
        mom = Reader_ReadInt16(msgReader);
        d->mom[MX] = FIX2FLT(fastMom? UNFIXED10_6(mom) : UNFIXED8_8(mom));
    }
    if(df & MDF_MOM_Y)
    {
        mom = Reader_ReadInt16(msgReader);
        d->mom[MY] = FIX2FLT(fastMom ? UNFIXED10_6(mom) : UNFIXED8_8(mom));
    }
    if(df & MDF_MOM_Z)
    {
        mom = Reader_ReadInt16(msgReader);
        d->mom[MZ] = FIX2FLT(fastMom ? UNFIXED10_6(mom) : UNFIXED8_8(mom));
    }

    // Angles with 16-bit accuracy.
    if(df & MDF_ANGLE)
        d->angle = Reader_ReadInt16(msgReader) << 16;

    // MDF_SELSPEC is never used without MDF_SELECTOR.
    if(df & MDF_SELECTOR)
        d->selector = Reader_ReadPackedUInt16(msgReader);
    if(df & MDF_SELSPEC)
        d->selector |= Reader_ReadByte(msgReader) << 24;

    if(df & MDF_STATE)
    {
        int stateIdx = Reader_ReadPackedUInt16(msgReader);

        // Translate.
        stateIdx = Cl_LocalMobjState(stateIdx);

        // When local actions are allowed, the assumption is that
        // the client will be doing the state changes.
        if(!skip && !(info->flags & CLMF_LOCAL_ACTIONS))
        {
            ClMobj_SetState(d, stateIdx);
            info->flags |= CLMF_KNOWN_STATE;
        }
    }

    if(df & MDF_FLAGS)
    {
        // Only the flags in the pack mask are affected.
        d->ddFlags &= ~DDMF_PACK_MASK;
        d->ddFlags |= DDMF_REMOTE | (Reader_ReadUInt32(msgReader) & DDMF_PACK_MASK);

        d->flags = Reader_ReadUInt32(msgReader);
        d->flags2 = Reader_ReadUInt32(msgReader);
        d->flags3 = Reader_ReadUInt32(msgReader);
    }

    if(df & MDF_HEALTH)
        d->health = Reader_ReadInt32(msgReader);

    if(df & MDF_RADIUS)
        d->radius = Reader_ReadFloat(msgReader);

    if(df & MDF_HEIGHT)
        d->height = Reader_ReadFloat(msgReader);

    if(df & MDF_FLOORCLIP)
        d->floorClip = Reader_ReadFloat(msgReader);

    if(moreFlags & MDFE_TRANSLUCENCY)
        d->translucency = Reader_ReadByte(msgReader);

    if(moreFlags & MDFE_FADETARGET)
        d->visTarget = ((short)Reader_ReadByte(msgReader)) - 1;

    if(moreFlags & MDFE_TYPE)
    {
        d->type = Cl_LocalMobjType(Reader_ReadInt32(msgReader));
        d->info = &mobjInfo[d->type];
        assert(d->info);
    }

    // The delta has now been read. We can now skip if necessary.
    if(skip) return;

    assert(d != &dummy);

    // Is it time to remove the Hidden status?
    if(info->flags & CLMF_HIDDEN)
    {
        // Now it can be displayed (potentially).
        if(ClMobj_Reveal(d))
        {
            // Now it can be linked to the world.
            needsLinking = true;
        }
    }

    // Non-player mobjs: update the Z position to be on the local floor, which may be
    // different than the server-side floor.
    if(!d->dPlayer && onFloor && gx.MobjCheckPositionXYZ)
    {
        coord_t* floorZ = gx.GetVariable(DD_TM_FLOOR_Z);
        if(floorZ)
        {
            gx.MobjCheckPositionXYZ(d, d->origin[VX], d->origin[VY], DDMAXFLOAT);
            d->origin[VZ] = d->floorZ = *floorZ;
        }
    }

    // If the clmobj is Hidden (or Nulled), it will not be linked back to
    // the world until it's officially Created. (Otherwise, partially updated
    // mobjs may be visible for a while.)
    if(!(info->flags & (CLMF_HIDDEN | CLMF_NULLED)))
    {
        // Link again.
        if(needsLinking && !d->dPlayer)
        {
            ClMobj_Link(mo);

            if(ClMobj_IsStuckInsideLocalPlayer(mo))
            {
                // Oopsie, on second thought we shouldn't do this move.
                ClMobj_Unlink(mo);
                V3d_Copy(mo->origin, oldState.origin);
                mo->floorZ = oldState.floorZ;
                mo->ceilingZ = oldState.ceilingZ;
                ClMobj_Link(mo);
            }
        }

        // Update players.
        if(d->dPlayer)
        {
#ifdef _DEBUG
            VERBOSE2( Con_Message("ClMobj_ReadDelta2: Updating player %i local mobj with new clmobj state {%f, %f, %f}.\n",
                                  P_GetDDPlayerIdx(d->dPlayer), d->origin[VX], d->origin[VY], d->origin[VZ]) );
#endif
            // Players have real mobjs. The client mobj is hidden (unlinked).
            Cl_UpdateRealPlayerMobj(d->dPlayer->mo, d, df, onFloor);
        }
    }
}

/**
 * Null mobjs deltas have their own type in a PSV_FRAME2 packet.
 * Here we remove the mobj in question.
 */
void ClMobj_ReadNullDelta2(boolean skip)
{
    mobj_t *mo;
    clmoinfo_t* info;
    thid_t  id;

    // The delta only contains an ID.
    id = Reader_ReadUInt16(msgReader);

    if(skip)
        return;

#ifdef _DEBUG
    Con_Printf("Cl_ReadNullMobjDelta2: Null %i\n", id);
#endif

    if((mo = ClMobj_Find(id)) == NULL)
    {
        // Wasted bandwidth...
#ifdef _DEBUG
        Con_Printf("Cl_ReadNullMobjDelta2: Request to remove id %i that has "
                   "not been received.\n", id);
#endif
        return;
    }

    info = ClMobj_GetInfo(mo);

    // Get rid of this mobj.
    if(!mo->dPlayer)
    {
        ClMobj_Unlink(mo);
    }
    else
    {
#ifdef _DEBUG
        Con_Message("ClMobj_ReadNullDelta2: clmobj of player %i deleted.\n",
                    P_GetDDPlayerIdx(mo->dPlayer));
#endif

        // The clmobjs of players aren't linked.
        ClPlayer_State(P_GetDDPlayerIdx(mo->dPlayer))->clMobjId = 0;
    }

    // This'll allow playing sounds from the mobj for a little while.
    // The mobj will soon time out and be permanently removed.
    info->time = Sys_GetRealTime();
    info->flags |= CLMF_UNPREDICTABLE | CLMF_NULLED;

#ifdef _DEBUG
    checkMobjHash();
#endif
}
