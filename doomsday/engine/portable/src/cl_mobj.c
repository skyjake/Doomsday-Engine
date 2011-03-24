/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * cl_mobj.c: Client Map Objects
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_defs.h"
#include "de_system.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_audio.h"

// MACROS ------------------------------------------------------------------

// The client mobjs are stored into a hash to speed up the searching.
#define HASH_SIZE       256

// Convert 8.8/10.6 fixed point to 16.16.
#define UNFIXED8_8(x)   (((x) << 16) / 256)
#define UNFIXED10_6(x)  (((x) << 16) / 64)

// Milliseconds it takes for Unpredictable and Hidden mobjs to be
// removed from the hash. Under normal circumstances, the special
// status should be removed fairly quickly (a matter of out-of-sequence
// frames or sounds playing before a mobj is sent).
#define CLMOBJ_TIMEOUT  10000   // 10 seconds

// Missiles don't hit mobjs only after a short delay. This'll
// allow the missile to move free of the shooter. (Quite a hack!)
#define MISSILE_FREE_MOVE_TIME  1000

// TYPES -------------------------------------------------------------------

typedef struct cmhash_s {
    clmobj_t *first, *last;
} cmhash_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int /*latest_frame_size, */ gotFrame;
extern int predicted_tics;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

cmhash_t cmHash[HASH_SIZE];

//int   predict_tics = 70;

// Time skew tells whether client time is running before or after frame time.
//int net_timeskew = 0;
//int net_skewdampen = 5;       // In frames.
//boolean net_showskew = false;

//boolean pred_forward = true;  // Predicting forward in time.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

//static byte previous_time = 0;

// CODE --------------------------------------------------------------------

/**
 * @return              Pointer to the hash chain with the specified id.
 */
cmhash_t *Cl_MobjHash(thid_t id)
{
    return &cmHash[(uint) id % HASH_SIZE];
}

/**
 * Links the clmobj into the client mobj hash table.
 */
void Cl_LinkMobj(clmobj_t *cmo, thid_t id)
{
    cmhash_t   *hash = Cl_MobjHash(id);

    // Set the ID.
    cmo->mo.thinker.id = id;
    cmo->next = NULL;

    if(hash->last)
    {
        hash->last->next = cmo;
        cmo->prev = hash->last;
    }
    hash->last = cmo;

    if(!hash->first)
    {
        hash->first = cmo;
    }
}

/**
 * Unlinks the clmobj from the client mobj hash table.
 */
void Cl_UnlinkMobj(clmobj_t *cmo)
{
    cmhash_t   *hash = Cl_MobjHash(cmo->mo.thinker.id);
/*
#ifdef _DEBUG
if(cmo->flags & CLMF_HIDDEN)
{
    Con_Printf("Cl_UnlinkMobj: Hidden mobj %i unlinked.\n",
    cmo->mo.thinker.id);
}
#endif
*/
    if(hash->first == cmo)
        hash->first = cmo->next;
    if(hash->last == cmo)
        hash->last = cmo->prev;
    if(cmo->next)
        cmo->next->prev = cmo->prev;
    if(cmo->prev)
        cmo->prev->next = cmo->next;
}

/**
 * Searches through the client mobj hash table and returns the clmobj
 * with the specified ID, if that exists.
 */
clmobj_t *Cl_FindMobj(thid_t id)
{
    cmhash_t   *hash = Cl_MobjHash(id);
    clmobj_t   *cmo;

    // Scan the existing client mobjs.
    for(cmo = hash->first; cmo; cmo = cmo->next)
    {
        if(cmo->mo.thinker.id == id)
            return cmo;
    }

    // Not found!
    return NULL;
}

/**
 * Iterate the client mobj hash, exec the callback on each. Abort if callback
 * returns @c false.
 *
 * @return              If the callback returns @c false.
 */
boolean Cl_MobjIterator(boolean (*callback) (clmobj_t *, void *), void *parm)
{
    clmobj_t   *cmo;
    int         i;

    for(i = 0; i < HASH_SIZE; ++i)
    {
        for(cmo = cmHash[i].first; cmo; cmo = cmo->next)
        {
            if(!callback(cmo, parm))
                return false;
        }
    }
    return true;
}

/**
 * Unlinks the mobj from sectorlinks and if the object is solid,
 * the blockmap.
 */
void Cl_UnsetMobjPosition(clmobj_t *cmo)
{
    P_MobjUnlink(&cmo->mo);
}

/**
 * Links the mobj into sectorlinks and if the object is solid, the
 * blockmap. Linking to sectorlinks makes the mobj visible and linking
 * to the blockmap makes it possible to interact with it (collide).
 * If the client mobj is Hidden, it will not be linked anywhere.
 */
void Cl_SetMobjPosition(clmobj_t *cmo)
{
    mobj_t *mo = &cmo->mo;

    if((cmo->flags & (CLMF_HIDDEN | CLMF_UNPREDICTABLE)) || mo->dPlayer)
    {
        // We do not yet have all the details about Hidden mobjs.
        // The server hasn't sent us a Create Mobj delta for them.
        // Client mobjs that belong to players remain unlinked.
        return;
    }

    P_MobjLink(mo,
                (mo->ddFlags & DDMF_DONTDRAW ? 0 : DDLINK_SECTOR) |
                (mo->ddFlags & DDMF_SOLID ? DDLINK_BLOCKMAP : 0));
}

/**
 * Change the state of a mobj.
 */
void Cl_SetMobjState(mobj_t *mo, int stnum)
{
    if(stnum < 0)
        return;
    do
    {
        P_MobjSetState(mo, stnum);
        stnum = states[stnum].nextState;
    }
    while(!mo->tics && stnum > 0);

    // Update mobj's type (this is not perfectly reliable...)
    // from the stateOwners table.
    if(stateOwners[stnum])
        mo->type = stateOwners[stnum] - mobjInfo;
    else
        mo->type = 0;
}

/**
 * Updates floorz and ceilingz of the mobj.
 */
void Cl_CheckMobj(clmobj_t *cmo, boolean justCreated)
{
    mobj_t     *mo = &cmo->mo;
    boolean     onFloor = false, inCeiling = false;

    if(mo->pos[VZ] == DDMINFLOAT)
    {
        // Make the mobj stick to the floor.
        cmo->flags |= CLMF_STICK_FLOOR;

        // Give it a real Z coordinate.
        onFloor = true;
        mo->pos[VZ] = mo->floorZ;
    }

    if(mo->pos[VZ] == DDMAXFLOAT)
    {
        // Make the mobj stick to the ceiling.
        cmo->flags |= CLMF_STICK_CEILING;

        // Give it a real Z coordinate.
        inCeiling = true;
        mo->pos[VZ] = mo->ceilingZ - mo->height;
    }

    // Find out floor and ceiling z.
    P_CheckPosXYZ(mo, mo->pos[VX], mo->pos[VY], mo->pos[VZ]);
    mo->floorZ = tmpFloorZ;
    mo->ceilingZ = tmpCeilingZ;

    if(onFloor)
    {
        mo->pos[VZ] = mo->floorZ;
    }
    if(inCeiling)
    {
        mo->pos[VZ] = mo->ceilingZ - mo->height;
    }

#if 0
    // P_CheckPosition sets blockingMobj.
#  ifdef _DEBUG
    /*if(blockingMobj)
       Con_Printf("Collision %i\n", mo->thinker.id); */
    if(justCreated && mo->ddFlags & DDMF_MISSILE)
    {
        Con_Printf("Misl creat %i, (%g %g %g) mom (%g %g %g)\n",
                   mo->thinker.id, mo->pos[VX], mo->pos[VY], mo->pos[VZ],
                   mo->mom[MX], mo->mom[MY], mo->mom[MZ]);
    }
#  endif
    if(justCreated && mo->ddFlags & DDMF_MISSILE && blockingMobj != NULL)
    {
        // This happens when a missile is created inside an object
        // (the shooter, typically). We allow the missile to noclip
        // through mobjs until it's free.
        cmo->flags |= CLMF_OVERLAPPING;

#  ifdef _DEBUG
        Con_Printf("Overlap %i\n", mo->thinker.id);
#  endif
    }
#endif
}

/**
 * Make the real player mobj identical with the client mobj.
 * The client mobj is always unlinked. Only the *real* mobj is visible.
 * (The real mobj was created by the Game.)
 */
void Cl_UpdateRealPlayerMobj(mobj_t *mo, mobj_t *clmo, int flags)
{
#if _DEBUG
if(!mo || !clmo)
{
    VERBOSE( Con_Message("Cl_UpdateRealPlayerMobj: mo=%p clmo=%p\n", mo, clmo) );
    return;
}
#endif

    if(flags & (MDF_POS_X | MDF_POS_Y))
    {
        // We have to unlink the real mobj before we move it.
        P_MobjUnlink(mo);
        mo->pos[VX] = clmo->pos[VX];
        mo->pos[VY] = clmo->pos[VY];
        P_MobjLink(mo, DDLINK_SECTOR | DDLINK_BLOCKMAP);
    }
    mo->pos[VZ] = clmo->pos[VZ];
    if(flags & MDF_MOM_X)
        mo->mom[MX] = clmo->mom[MX];
    if(flags & MDF_MOM_Y)
        mo->mom[MY] = clmo->mom[MY];
    if(flags & MDF_MOM_Z)
        mo->mom[MZ] = clmo->mom[MZ];
    if(flags & MDF_ANGLE)
    {
        mo->angle = clmo->angle;
#ifdef _DEBUG
VERBOSE( Con_Message("Cl_UpdateRealPlayerMobj: mo=%p angle=%x\n", mo, mo->angle) );
#endif
    }
    mo->sprite = clmo->sprite;
    mo->frame = clmo->frame;
    //mo->nextframe = clmo->nextframe;
    mo->tics = clmo->tics;
    mo->state = clmo->state;
    //mo->nexttime = clmo->nexttime;
#define DDMF_KEEP_MASK (DDMF_REMOTE | DDMF_SOLID)
    mo->ddFlags = (mo->ddFlags & DDMF_KEEP_MASK) | (clmo->ddFlags & ~DDMF_KEEP_MASK);
#ifdef _DEBUG
    Con_Message("Cl_UpdateRealPlayerMobj: Setting mo flags to 0x%x\n", mo->ddFlags);
#endif
    mo->radius = clmo->radius;
    mo->height = clmo->height;
    mo->floorClip = clmo->floorClip;
    mo->floorZ = clmo->floorZ;
    mo->ceilingZ = clmo->ceilingZ;
    mo->selector &= ~DDMOBJ_SELECTOR_MASK;
    mo->selector |= clmo->selector & DDMOBJ_SELECTOR_MASK;
    mo->visAngle = clmo->angle >> 16;

    //if(flags & MDF_FLAGS) CON_Printf("Cl_RMD: ddf=%x\n", mo->ddFlags);
}

/**
 * Initialize clientside data.
 */
void Cl_InitClientMobjs(void)
{
    //previousTime = gameTime;

    // List of client mobjs.
    memset(cmHash, 0, sizeof(cmHash));

    Cl_InitPlayers();
}

/**
 * Called when the client is shut down. Unlinks everything from the
 * sectors and the blockmap and clears the clmobj list.
 */
void Cl_DestroyClientMobjs(void)
{
    int                 i;
    clmobj_t*           cmo;

    for(i = 0; i < HASH_SIZE; ++i)
    {
        for(cmo = cmHash[i].first; cmo; cmo = cmo->next)
        {
            // Players' clmobjs are not linked anywhere.
            if(!cmo->mo.dPlayer)
                Cl_UnsetMobjPosition(cmo);
        }
    }

    Cl_Reset();
}

/**
 * Reset the client status. Called when the map changes.
 */
void Cl_Reset(void)
{
    Cl_ResetFrame();

    // The PU_MAP memory was freed, so just clear the hash.
    memset(cmHash, 0, sizeof(cmHash));

    // Clear player data, too, since we just lost all clmobjs.
    Cl_InitPlayers();
}

/**
 * The client mobj is moved linearly, with collision checking.
 */
void Cl_MobjMove(clmobj_t* cmo)
{
    mobj_t*             mo = &cmo->mo;
    boolean             collided = false;
    float               gravity = FIX2FLT(mapGravity);

    // First do XY movement.
    if(mo->mom[MX] != 0 || mo->mom[MY] != 0)
    {
        // Missiles don't hit mobjs only after a short delay. This'll
        // allow the missile to move free of the shooter. (Quite a hack!)
        if(mo->ddFlags & DDMF_MISSILE &&
           Sys_GetRealTime() - cmo->time < MISSILE_FREE_MOVE_TIME)
        {
            // The mobj should be allowed to move freely though mobjs.
            // Use the quick and dirty global variable.
            dontHitMobjs = true;
        }

        // Move while doing collision checking.
        if(!P_StepMove(mo, mo->mom[MX], mo->mom[MY], 0))
        {
            // There was a collision!
            collided = true;
        }

        // Allow mobj hit checks once again.
        dontHitMobjs = false;
    }

    if(mo->mom[MZ] != 0)
    {
        mo->pos[VZ] += mo->mom[MZ];

        if(mo->pos[VZ] < mo->floorZ)
        {
            mo->pos[VZ] = mo->floorZ;
            mo->mom[MZ] = 0;
            collided = true;
        }

        if(mo->pos[VZ] + mo->height > mo->ceilingZ)
        {
            mo->pos[VZ] = mo->ceilingZ - mo->height;
            mo->mom[MZ] = 0;
            collided = true;
        }
    }

    if(mo->pos[VZ] > mo->floorZ)
    {   // Gravity will affect the prediction.
        //// \fixme What about sector-specific gravity?
        if(mo->ddFlags & DDMF_LOWGRAVITY)
        {
            if(mo->mom[MZ] == 0)
                mo->mom[MZ] = -(gravity / 8) * 2;
            else
                mo->mom[MZ] -= gravity / 8;
        }
        else if(!(mo->ddFlags & DDMF_NOGRAVITY))
        {
            if(mo->mom[MZ] == 0)
                mo->mom[MZ] = -gravity * 2;
            else
                mo->mom[MZ] -= gravity;
        }
    }

    if(collided)
    {
        // Missiles are immediately removed.
        // (An explosion should follow shortly.)
        if(mo->ddFlags & DDMF_MISSILE)
        {
            // We don't know how to proceed from this, but merely
            // stopping the mobj is not enough. Let's hide it until
            // next delta arrives.
            cmo->flags |= CLMF_UNPREDICTABLE;
            Cl_UnsetMobjPosition(cmo);
        }
        // [Kaboom!]
    }

    // Stick to a plane?
    if(cmo->flags & CLMF_STICK_FLOOR)
    {
        mo->pos[VZ] = mo->floorZ;
    }
    if(cmo->flags & CLMF_STICK_CEILING)
    {
        mo->pos[VZ] = mo->ceilingZ - mo->height;
    }
}

/**
 * Decrement tics counter and changes the state of the mobj if necessary.
 */
void Cl_MobjAnimate(mobj_t *mo)
{
    if(!mo->state || mo->tics < 0)
        return;                 // In stasis.

    mo->tics--;
    if(mo->tics <= 0)
    {
        // Go to next state, if possible.
        if(mo->state->nextState >= 0)
        {
            Cl_SetMobjState(mo, mo->state->nextState);

            // Players have both client mobjs and regular mobjs. This
            // routine modifies the *client* mobj, so for players we need
            // to update the real, visible mobj as well.
            if(mo->dPlayer)
                Cl_UpdateRealPlayerMobj(mo->dPlayer->mo, mo, 0);
        }
        else
        {
            // Freeze it; the server will probably to remove it soon.
            mo->tics = -1;
        }
    }
}

/**
 * All client mobjs are moved and animated using the data we have.
 */
void Cl_PredictMovement(void)
{
    clmobj_t           *cmo, *next = NULL;
    int                 i;
    int                 moCount = 0;
    uint                nowTime = Sys_GetRealTime();

    predicted_tics++;

    // Move all client mobjs.
    for(i = 0; i < HASH_SIZE; ++i)
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
}

/**
 * Create a new client mobj.
 */
clmobj_t *Cl_CreateMobj(thid_t id)
{
    clmobj_t   *cmo = Z_Calloc(sizeof(*cmo), PU_MAP, 0);

    cmo->mo.ddFlags |= DDMF_REMOTE;
    cmo->time = Sys_GetRealTime();
    Cl_LinkMobj(cmo, id);
    P_SetMobjID(id, true);      // Mark this ID as used.

    return cmo;
}

/**
 * Destroys the client mobj.
 */
void Cl_DestroyMobj(clmobj_t *cmo)
{
    // Stop any sounds originating from this mobj.
    S_StopSound(0, &cmo->mo);

    // The ID is free once again.
    P_SetMobjID(cmo->mo.thinker.id, false);
    Cl_UnsetMobjPosition(cmo);
    Cl_UnlinkMobj(cmo);
    Z_Free(cmo);
}

/**
 * Call for Hidden client mobjs to make then visible.
 * If a sound is waiting, it's now played.
 *
 * @return              @c true, if the mobj was revealed.
 */
boolean Cl_RevealMobj(clmobj_t *cmo)
{
    // Check that we know enough about the clmobj.
    if(cmo->mo.dPlayer != &ddPlayers[consolePlayer].shared &&
       (!(cmo->flags & CLMF_KNOWN_X) ||
        !(cmo->flags & CLMF_KNOWN_Y) ||
        !(cmo->flags & CLMF_KNOWN_Z) ||
        !(cmo->flags & CLMF_KNOWN_STATE)))
    {
        // Don't reveal just yet. We lack a vital piece of information.
        return false;
    }
#ifdef _DEBUG
    Con_Message("Cl_RevealMobj: clmobj %i Hidden status lifted.\n", cmo->mo.thinker.id);
#endif

    cmo->flags &= ~CLMF_HIDDEN;

    // Start a sound that has been queued for playing at the time
    // of unhiding. Sounds are queued if a sound delta arrives for an
    // object ID we don't know (yet).
    if(cmo->flags & CLMF_SOUND)
    {
        cmo->flags &= ~CLMF_SOUND;
        S_StartSoundAtVolume(cmo->sound, &cmo->mo, cmo->volume);
    }

#ifdef _DEBUG
    VERBOSE2( Con_Printf("Cl_RevealMobj: Revealing id %i, state %p (%i)\n",
                         cmo->mo.thinker.id, cmo->mo.state,
                         (int)(cmo->mo.state - states)) );
#endif

    return true;
}

/**
 * Reads a single mobj PSV_FRAME2 delta from the message buffer and
 * applies it to the client mobj in question.
 *
 * For client mobjs that belong to players, updates the real player mobj.
 */
void Cl_ReadMobjDelta2(boolean skip)
{
    boolean     needsLinking = false, justCreated = false;
    clmobj_t   *cmo = NULL;
    mobj_t     *d;
    static mobj_t dummy;
    int         df = 0;
    byte        moreFlags = 0, fastMom = false;
    short       mom;
    thid_t      id = Msg_ReadShort();   // Read the ID.

    // Flags.
    df = Msg_ReadShort();

    // More flags?
    if(df & MDF_MORE_FLAGS)
    {
        moreFlags = Msg_ReadByte();

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
        cmo = Cl_FindMobj(id);
        if(!cmo)
        {
#ifdef _DEBUG
            Con_Message("Cl_ReadMobjDelta: Creating new clmobj %i (hidden).\n", id);
#endif

            // This is a new ID, allocate a new mobj.
            cmo = Cl_CreateMobj(id);
            cmo->mo.ddFlags |= DDMF_NOGRAVITY; // safer this way
            justCreated = true;
            needsLinking = true;

            // Always create new mobjs as hidden. They will be revealed when
            // we know enough about them.
            cmo->flags |= CLMF_HIDDEN;
        }

        if(!(cmo->flags & CLMF_NULLED))
        {
            // Now that we've received a delta, the mobj's Predictable again.
            cmo->flags &= ~CLMF_UNPREDICTABLE;

            // This clmobj is evidently alive.
            cmo->time = Sys_GetRealTime();
        }

        d = &cmo->mo;

        /*if(d->dPlayer && d->dPlayer == &ddPlayers[consolePlayer])
        {
            // Mark the local player known.
            cmo->flags |= CLMF_KNOWN;
        }*/

        // Need to unlink? (Flags because DDMF_SOLID determines block-linking.)
        if(df & (MDF_POS_X | MDF_POS_Y | MDF_POS_Z | MDF_FLAGS) &&
           !justCreated && !d->dPlayer)
        {
            needsLinking = true;
            Cl_UnsetMobjPosition(cmo);
        }
    }
    else
    {
        // We're skipping.
        d = &dummy;
    }

    // Coordinates with three bytes.
    if(df & MDF_POS_X)
    {
        d->pos[VX] = FIX2FLT((Msg_ReadShort() << FRACBITS) | (Msg_ReadByte() << 8));
        if(cmo)
            cmo->flags |= CLMF_KNOWN_X;
    }
    if(df & MDF_POS_Y)
    {
        d->pos[VY] = FIX2FLT((Msg_ReadShort() << FRACBITS) | (Msg_ReadByte() << 8));
        if(cmo)
            cmo->flags |= CLMF_KNOWN_Y;
    }
    if(df & MDF_POS_Z)
    {
        d->pos[VZ] = FIX2FLT((Msg_ReadShort() << FRACBITS) | (Msg_ReadByte() << 8));
        if(cmo)
        {
            cmo->flags |= CLMF_KNOWN_Z;

            // The mobj won't stick if an explicit coordinate is supplied.
            cmo->flags &= ~(CLMF_STICK_FLOOR | CLMF_STICK_CEILING);
        }
    }

    // When these flags are set, the normal Z coord is not included.
    if(moreFlags & MDFE_Z_FLOOR)
    {
        d->pos[VZ] = DDMINFLOAT;
        if(cmo)
            cmo->flags |= CLMF_KNOWN_Z;
    }
    if(moreFlags & MDFE_Z_CEILING)
    {
        d->pos[VZ] = DDMAXFLOAT;
        if(cmo)
            cmo->flags |= CLMF_KNOWN_Z;
    }

    // Momentum using 8.8 fixed point.
    if(df & MDF_MOM_X)
    {
        mom = Msg_ReadShort();
        d->mom[MX] = FIX2FLT(fastMom? UNFIXED10_6(mom) : UNFIXED8_8(mom));
    }
    if(df & MDF_MOM_Y)
    {
        mom = Msg_ReadShort();
        d->mom[MY] = FIX2FLT(fastMom ? UNFIXED10_6(mom) : UNFIXED8_8(mom));
    }
    if(df & MDF_MOM_Z)
    {
        mom = Msg_ReadShort();
        d->mom[MZ] = FIX2FLT(fastMom ? UNFIXED10_6(mom) : UNFIXED8_8(mom));
    }

    // Angles with 16-bit accuracy.
    if(df & MDF_ANGLE)
        d->angle = Msg_ReadShort() << 16;

    // MDF_SELSPEC is never used without MDF_SELECTOR.
    if(df & MDF_SELECTOR)
        d->selector = Msg_ReadPackedShort();
    if(df & MDF_SELSPEC)
        d->selector |= Msg_ReadByte() << 24;

    if(df & MDF_STATE)
    {
        int stateIdx = Msg_ReadPackedShort();
        if(!skip)
        {
            Cl_SetMobjState(d, stateIdx);
            cmo->flags |= CLMF_KNOWN_STATE;
        }
    }

    // Pack flags into a word (3 bytes?).
    // \fixme Do the packing!
    if(df & MDF_FLAGS)
    {
        // Only the flags in the pack mask are affected.
        d->ddFlags &= ~DDMF_PACK_MASK;
        d->ddFlags |= DDMF_REMOTE | (Msg_ReadLong() & DDMF_PACK_MASK);
    }

    // Radius, height and floorclip are all bytes.
    if(df & MDF_RADIUS)
        d->radius = (float) Msg_ReadByte();
    if(df & MDF_HEIGHT)
        d->height = (float) Msg_ReadByte();
    if(df & MDF_FLOORCLIP)
    {
        if(df & MDF_LONG_FLOORCLIP)
            d->floorClip = FIX2FLT(Msg_ReadPackedShort() << 14);
        else
            d->floorClip = FIX2FLT(Msg_ReadByte() << 14);
    }

    if(moreFlags & MDFE_TRANSLUCENCY)
        d->translucency = Msg_ReadByte();

    if(moreFlags & MDFE_FADETARGET)
        d->visTarget = ((short)Msg_ReadByte()) -1;

    // The delta has now been read. We can now skip if necessary.
    if(skip)
        return;

    // Is it time to remove the Hidden status?
    if(cmo->flags & CLMF_HIDDEN)
    {
        // Now it can be displayed (potentially).
        if(Cl_RevealMobj(cmo))
        {
            // Now it can be linked to the world.
            needsLinking = true;
        }
    }

    if(df & (MDF_POS_X | MDF_POS_Y | MDF_POS_Z) ||
       moreFlags & (MDFE_Z_FLOOR | MDFE_Z_CEILING))
    {
        // This'll update floorz and ceilingz.
        Cl_CheckMobj(cmo, justCreated);
    }

    // If the clmobj is Hidden (or Nulled), it will not be linked back to
    // the world until it's officially Created. (Otherwise, partially updated
    // mobjs may be visible for a while.)
    if(!(cmo->flags & (CLMF_HIDDEN | CLMF_NULLED)))
    {
        // Link again.
        if(needsLinking && !d->dPlayer)
        {
            Cl_SetMobjPosition(cmo);
        }

        // Update players.
        if(d->dPlayer)
        {
            // Players have real mobjs. The client mobj is hidden (unlinked).
            Cl_UpdateRealPlayerMobj(d->dPlayer->mo, d, df);
        }
    }
}

/**
 * Null mobjs deltas have their own type in a PSV_FRAME2 packet.
 * Here we remove the mobj in question.
 */
void Cl_ReadNullMobjDelta2(boolean skip)
{
    clmobj_t   *cmo;
    thid_t      id;

    // The delta only contains an ID.
    id = Msg_ReadShort();

    if(skip)
        return;

#ifdef _DEBUG
Con_Printf("Cl_ReadNullMobjDelta2: Null %i\n", id);
#endif

    if((cmo = Cl_FindMobj(id)) == NULL)
    {
        // Wasted bandwidth...
#ifdef _DEBUG
    Con_Printf("Cl_ReadNullMobjDelta2: Request to remove id %i that has "
               "not been received.\n", id);
#endif
        return;
    }

    // Get rid of this mobj.
    if(!cmo->mo.dPlayer)
    {
        Cl_UnsetMobjPosition(cmo);
    }
    else
    {
        // The clmobjs of players aren't linked.
        clPlayerStates[P_GetDDPlayerIdx(cmo->mo.dPlayer)].cmo = NULL;
    }

    // This'll allow playing sounds from the mobj for a little while.
    // The mobj will soon time out and be permanently removed.
    cmo->time = Sys_GetRealTime();
    cmo->flags |= CLMF_UNPREDICTABLE | CLMF_NULLED;
}
