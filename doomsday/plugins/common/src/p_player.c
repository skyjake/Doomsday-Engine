/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * p_player.c: Common playsim routines relating to players.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dd_share.h"

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "dmu_lib.h"
#include "d_netsv.h"
#include "d_net.h"
#include "hu_log.h"
#include "p_map.h"
#include "g_common.h"
#include "p_actor.h"
#include "p_start.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

#define MESSAGETICS             (4 * TICSPERSEC)
#define CAMERA_FRICTION_THRESHOLD (.4f)

// TYPES -------------------------------------------------------------------

typedef struct weaponslotinfo_s {
    uint                num;
    weapontype_t*       types;
} weaponslotinfo_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static weaponslotinfo_t weaponSlots[NUM_WEAPON_SLOTS];

// CODE --------------------------------------------------------------------

static byte slotForWeaponType(weapontype_t type, uint* position)
{
    byte                i = 0, found = 0;

    do
    {
        weaponslotinfo_t*   slot = &weaponSlots[i];
        uint                j = 0;

        while(!found && j < slot->num)
        {
            if(slot->types[j] == type)
            {
                found = i + 1;
                if(position)
                    *position = j;
            }
            else
                j++;
        }

    } while(!found && ++i < NUM_WEAPON_SLOTS);

    return found;
}

static void unlinkWeaponInSlot(byte slotidx, weapontype_t type)
{
    uint                i;
    weaponslotinfo_t*   slot = &weaponSlots[slotidx-1];

    for(i = 0; i < slot->num; ++i)
        if(slot->types[i] == type)
            break;

    if(i == slot->num)
        return; // Not linked to this slot.

    memmove(&slot->types[i], &slot->types[i+1], sizeof(weapontype_t) *
            (slot->num - 1 - i));
    slot->types = realloc(slot->types, sizeof(weapontype_t) * --slot->num);
}

static void linkWeaponInSlot(byte slotidx, weapontype_t type)
{
    weaponslotinfo_t*   slot = &weaponSlots[slotidx-1];

    slot->types = realloc(slot->types, sizeof(weapontype_t) * ++slot->num);
    if(slot->num > 1)
        memmove(&slot->types[1], &slot->types[0],
                sizeof(weapontype_t) * (slot->num - 1));

    slot->types[0] = type;
}

void P_InitWeaponSlots(void)
{
    memset(weaponSlots, 0, sizeof(weaponSlots));
}

void P_FreeWeaponSlots(void)
{
    byte                i;

    for(i = 0; i < NUM_WEAPON_SLOTS; ++i)
    {
        weaponslotinfo_t*   slot = &weaponSlots[i];

        if(slot->types)
            free(slot->types);
        slot->types = NULL;
        slot->num = 0;
    }
}

boolean P_SetWeaponSlot(weapontype_t type, byte slot)
{
    byte                currentSlot;

    if(slot > NUM_WEAPON_SLOTS)
        return false;

    currentSlot = slotForWeaponType(type, NULL);

    // First, remove the weapon (if found).
    if(currentSlot)
        unlinkWeaponInSlot(currentSlot, type);

    if(slot != 0)
    {   // Add this weapon to the specified slot (head).
        linkWeaponInSlot(slot, type);
    }

    return true;
}

byte P_GetWeaponSlot(weapontype_t type)
{
    if(type >= WT_FIRST && type < NUM_WEAPON_TYPES)
        return slotForWeaponType(type, NULL);

    return 0;
}

weapontype_t P_WeaponSlotCycle(weapontype_t type, boolean prev)
{
    if(type >= WT_FIRST && type < NUM_WEAPON_TYPES)
    {
        byte                slotidx;
        uint                position;

        if((slotidx = slotForWeaponType(type, &position)))
        {
            weaponslotinfo_t*   slot = &weaponSlots[slotidx-1];

            if(slot->num > 1)
            {
                if(prev)
                {
                    if(position == 0)
                        position = slot->num - 1;
                    else
                        position--;
                }
                else
                {
                    if(position == slot->num - 1)
                        position = 0;
                    else
                        position++;
                }

                return slot->types[position];
            }
        }
    }

    return type;
}

/**
 * Iterate the weapons of a given weapon slot.
 *
 * @param slot          Weapon slot number.
 * @param reverse       Iff @c = true, the traversal is done in reverse.
 * @param callback      Ptr to the callback to make for each element.
 *                      If the callback returns @c 0, iteration ends.
 * @param context       Passed as an argument to @a callback.
 *
 * @return              Non-zero if no weapon is bound to the slot @a slot,
 *                      or callback @a callback signals an end to iteration.
 */
int P_IterateWeaponsBySlot(byte slot, boolean reverse,
                           int (*callback) (weapontype_t, void* context),
                           void* context)
{
    int                 result = 1;

    if(slot <= NUM_WEAPON_SLOTS)
    {
        uint                i = 0;
        const weaponslotinfo_t* sl = &weaponSlots[slot];

        while(i < sl->num &&
             (result = callback(sl->types[reverse ? sl->num - 1 - i : i],
                                context)) != 0)
            i++;
    }

    return result;
}

/**
 * Initialize player class info.
 */
#if __JHEXEN__
void P_InitPlayerClassInfo(void)
{
    PCLASS_INFO(PCLASS_FIGHTER)->niceName = GET_TXT(TXT_PLAYERCLASS1);
    PCLASS_INFO(PCLASS_CLERIC)->niceName = GET_TXT(TXT_PLAYERCLASS2);
    PCLASS_INFO(PCLASS_MAGE)->niceName = GET_TXT(TXT_PLAYERCLASS3);
    PCLASS_INFO(PCLASS_PIG)->niceName = GET_TXT(TXT_PLAYERCLASS4);
}
#endif

/**
 * @param player        The player to work with.
 *
 * @return              Number of the given player.
 */
int P_GetPlayerNum(player_t *player)
{
    int                 i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(player == &players[i])
        {
            return i;
        }
    }
    return 0;
}

/**
 * Return a bit field for the current player's cheats.
 *
 * @param               The player to work with.
 *
 * @return              Cheats active for the given player in a bitfield.
 */
int P_GetPlayerCheats(const player_t* player)
{
    if(!player)
    {
        return 0;
    }
    else
    {
        if(player->plr->flags & DDPF_CAMERA)
            return (player->cheats |
                    (CF_GODMODE | cfg.cameraNoClip? CF_NOCLIP : 0));
        else
            return player->cheats;
    }
}

int P_CountPlayersInGame(void)
{
    int c, count = 0;
    for(c = 0; c < MAXPLAYERS; ++c)
    {
        player_t* player = players + c;
        if(player->plr->inGame) count += 1;
    }
    return count;
}

/**
 * Determines whether the player's state is one of the walking states.
 *
 * @param pl  Player whose state to check.
 *
 * @return  @c true, if the player is walking.
 */
boolean P_PlayerInWalkState(player_t* pl)
{
    if(!pl->plr->mo) return false;

    /// @todo  Implementation restricts possibilities for modifying behavior solely with state definitions.

#if __JDOOM__
    return pl->plr->mo->state - STATES - PCLASS_INFO(pl->class_)->runState < 4;
#endif

#if __JHERETIC__
    return pl->plr->mo->state - STATES - PCLASS_INFO(pl->class_)->runState < 4;
#endif

#if __JHEXEN__
    return ((unsigned) ((pl->plr->mo->state - STATES) - PCLASS_INFO(pl->class_)->runState) < 4);
#endif

#if __JDOOM64__
    return pl->plr->mo->state - STATES - PCLASS_INFO(pl->class_)->runState < 4;
#endif
}

/**
 * Subtract the appropriate amount of ammo from the player for firing
 * the current ready weapon.
 *
 * @param player        The player doing the firing.
 */
void P_ShotAmmo(player_t *player)
{
    ammotype_t          i;
    int                 fireMode;
    weaponinfo_t*       wInfo =
        &weaponInfo[player->readyWeapon][player->class_];

    if(IS_CLIENT) return; // Server keeps track of this.

#if __JHERETIC__
    if(deathmatch)
        fireMode = 0; // In deathmatch always use mode zero.
    else
        fireMode = (player->powers[PT_WEAPONLEVEL2]? 1 : 0);
#else
    fireMode = 0;
#endif

    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        if(!wInfo->mode[fireMode].ammoType[i])
            continue; // Weapon does not take this ammo.

        // Don't let it fall below zero.
        player->ammo[i].owned = MAX_OF(0,
            player->ammo[i].owned - wInfo->mode[fireMode].perShot[i]);
    }
    player->update |= PSF_AMMO;
}

/**
 * Decides if an automatic weapon change should occur and does it.
 *
 * Called when:
 * A) the player has ran out of ammo for the readied weapon.
 * B) the player has been given a NEW weapon.
 * C) the player is ABOUT TO be given some ammo.
 *
 * If "weapon" is non-zero then we'll always try to change weapon.
 * If "ammo" is non-zero then we'll consider the ammo level of weapons that
 * use this ammo type.
 * If both non-zero - no more ammo for the current weapon.
 *
 * \todo Should be called AFTER ammo is given but we need to
 * remember the old count before the change.
 *
 * @param player        The player given the weapon.
 * @param weapon        The weapon given to the player (if any).
 * @param ammo          The ammo given to the player (if any).
 * @param force         @c true = Force a weapon change.
 *
 * @return              The weapon we changed to OR WT_NOCHANGE.
 */
weapontype_t P_MaybeChangeWeapon(player_t *player, weapontype_t weapon,
                                 ammotype_t ammo, boolean force)
{
    int                 i, lvl, pclass;
    ammotype_t          ammotype;
    weapontype_t        candidate;
    weapontype_t        returnval = WT_NOCHANGE;
    weaponinfo_t       *winf;
    boolean             found;

    if(IS_NETWORK_SERVER)
    {
        // This is done on clientside.
        NetSv_MaybeChangeWeapon(player - players, weapon, ammo, force);
        return WT_NOCHANGE;
    }

#ifdef _DEBUG
    Con_Message("P_MaybeChangeWeapon: plr %i, weapon %i, ammo %i, force %i\n",
                (int)(player - players), weapon, ammo, force);
#endif

    // Assume weapon power level zero.
    lvl = 0;

    pclass = player->class_;

#if __JHERETIC__
    if(player->powers[PT_WEAPONLEVEL2])
        lvl = 1;
#endif

    if(weapon == WT_NOCHANGE && ammo == AT_NOAMMO) // Out of ammo.
    {
        boolean good;

        // Note we have no auto-logical choice for a forced change.
        // Preferences are set by the user.
        found = false;
        for(i = 0; i < NUM_WEAPON_TYPES && !found; ++i)
        {
            candidate = cfg.weaponOrder[i];
            winf = &weaponInfo[candidate][pclass];

            // Is candidate available in this game mode?
            if(!(winf->mode[lvl].gameModeBits & gameModeBits))
                continue;

            // Does the player actually own this candidate?
            if(!player->weapons[candidate].owned)
                continue;

            // Is there sufficent ammo for the candidate weapon?
            // Check amount for each used ammo type.
            good = true;
            for(ammotype = 0; ammotype < NUM_AMMO_TYPES && good; ++ammotype)
            {
                if(!winf->mode[lvl].ammoType[ammotype])
                    continue; // Weapon does not take this type of ammo.
#if __JHERETIC__
                // Heretic always uses lvl 0 ammo requirements in deathmatch
                if(deathmatch &&
                   player->ammo[ammotype].owned < winf->mode[0].perShot[ammotype])
                {   // Not enough ammo of this type. Candidate is NOT good.
                    good = false;
                }
                else
#endif
                if(player->ammo[ammotype].owned < winf->mode[lvl].perShot[ammotype])
                {   // Not enough ammo of this type. Candidate is NOT good.
                    good = false;
                }
            }

            if(good)
            {   // Candidate weapon meets the criteria.
                returnval = candidate;
                found = true;
            }
        }
    }
    else if(weapon != WT_NOCHANGE) // Player was given a NEW weapon.
    {
        // A forced weapon change?
        if(force)
        {
            returnval = weapon;
        }
        // Is the player currently firing?
        else if(!((player->brain.attack) &&
                  cfg.noWeaponAutoSwitchIfFiring))
        {
            // Should we change weapon automatically?
            if(cfg.weaponAutoSwitch == 2)
            {   // Always change weapon mode
                returnval = weapon;
            }
            else if(cfg.weaponAutoSwitch == 1)
            {   // Change if better mode

                // Iterate the weapon order array and see if a weapon change
                // should be made. Preferences are user selectable.
                for(i = 0; i < NUM_WEAPON_TYPES; ++i)
                {
                    candidate = cfg.weaponOrder[i];
                    winf = &weaponInfo[candidate][pclass];

                    // Is candidate available in this game mode?
                    if(!(winf->mode[lvl].gameModeBits & gameModeBits))
                        continue;

                    if(weapon == candidate)
                    {   // weapon has a higher priority than the readyweapon.
                        returnval = weapon;
                    }
                    else if(player->readyWeapon == candidate)
                    {   // readyweapon has a higher priority so don't change.
                        break;
                    }
                }
            }
        }
    }
    else if(ammo != AT_NOAMMO) // Player is about to be given some ammo.
    {
        if((!(player->ammo[ammo].owned > 0) && cfg.ammoAutoSwitch != 0) ||
           force)
        {   // We were down to zero, so select a new weapon.

            // Iterate the weapon order array and see if the player owns a
            // weapon that can be used now they have this ammo.
            // Preferences are user selectable.
            for(i = 0; i < NUM_WEAPON_TYPES; ++i)
            {
                candidate = cfg.weaponOrder[i];
                winf = &weaponInfo[candidate][pclass];

                // Is candidate available in this game mode?
                if(!(winf->mode[lvl].gameModeBits & gameModeBits))
                    continue;

                // Does the player actually own this candidate?
                if(!player->weapons[candidate].owned)
                    continue;

                // Does the weapon use this type of ammo?
                if(!(winf->mode[lvl].ammoType[ammo]))
                    continue;

                /**
                 * @todo Have we got enough of ALL used ammo types?
                 * Problem, since the ammo has not been given yet (could
                 * be an object that gives several ammo types eg backpack)
                 * we can't test for this with what we know!
                 *
                 * This routine should be called AFTER the new ammo has
                 * been given. Somewhat complex logic to decipher first...
                 */

                if(cfg.ammoAutoSwitch == 2)
                {   // Always change weapon mode.
                    returnval = candidate;
                    break;
                }
                else if(cfg.ammoAutoSwitch == 1 &&
                        player->readyWeapon == candidate)
                {   // readyweapon has a higher priority so don't change.
                    break;
                }
            }
        }
    }

    // Don't change to the existing weapon.
    if(returnval == player->readyWeapon)
        returnval = WT_NOCHANGE;

    // Choosen a weapon to change to?
    if(returnval != WT_NOCHANGE)
    {
#ifdef _DEBUG
        Con_Message("P_MaybeChangeWeapon: Decided to change to weapon %i.\n", returnval);
#endif
        player->pendingWeapon = returnval;

        if(IS_CLIENT)
        {
            // Tell the server.
            NetCl_PlayerActionRequest(player, GPA_CHANGE_WEAPON, player->pendingWeapon);
        }
    }

    return returnval;
}

/**
 * Checks if the player has enough ammo to fire their readied weapon.
 * If not, a weapon change is instigated.
 *
 * @return              @c true, if there is enough ammo to fire.
 */
boolean P_CheckAmmo(player_t* plr)
{
    int                 fireMode;
    boolean             good;
    ammotype_t          i;
    weaponinfo_t*       wInfo;

    wInfo = &weaponInfo[plr->readyWeapon][plr->class_];
#if __JDOOM__ || __JDOOM64__ || __JHEXEN__
    fireMode = 0;
#endif
#if __JHERETIC__
    // If deathmatch always use firemode two ammo requirements.
    if(plr->powers[PT_WEAPONLEVEL2] && !deathmatch)
        fireMode = 1;
    else
        fireMode = 0;
#endif

#if __JHEXEN__
    //// @todo Kludge: Work around the multiple firing modes problems.
    //// We need to split the weapon firing routines and implement them as
    //// new fire modes.
    if(plr->class_ == PCLASS_FIGHTER && plr->readyWeapon != WT_FOURTH)
        return true;
    // < KLUDGE
#endif

    // Check we have enough of ALL ammo types used by this weapon.
    good = true;
    for(i = 0; i < NUM_AMMO_TYPES && good; ++i)
    {
        if(!wInfo->mode[fireMode].ammoType[i])
            continue; // Weapon does not take this type of ammo.

        // Minimal amount for one shot varies.
        if(plr->ammo[i].owned < wInfo->mode[fireMode].perShot[i])
        {   // Insufficent ammo to fire this weapon.
            good = false;
        }
    }

    if(good)
        return true;

    // Out of ammo, pick a weapon to change to.
    P_MaybeChangeWeapon(plr, WT_NOCHANGE, AT_NOAMMO, false);

    // Now set appropriate weapon overlay.
    if(plr->pendingWeapon != WT_NOCHANGE)
        P_SetPsprite(plr, ps_weapon, wInfo->mode[fireMode].states[WSN_DOWN]);

    return false;
}

/**
 * Return the next weapon for the given player. Can return the existing
 * weapon if no other valid choices. Preferences are NOT user selectable.
 *
 * @param player        The player to work with.
 * @param prev          Search direction @c true = previous, @c false = next.
 */
weapontype_t P_PlayerFindWeapon(player_t* player, boolean prev)
{
    weapontype_t*       list, w = 0, initial;
    int                 lvl, i;
#if __JDOOM__
    static weapontype_t wp_list[] = {
        WT_FIRST, WT_SECOND, WT_THIRD, WT_NINETH, WT_FOURTH,
        WT_FIFTH, WT_SIXTH, WT_SEVENTH, WT_EIGHTH
    };

#elif __JDOOM64__
    static weapontype_t wp_list[] = {
        WT_FIRST, WT_SECOND, WT_THIRD, WT_NINETH, WT_FOURTH,
        WT_FIFTH, WT_SIXTH, WT_SEVENTH, WT_EIGHTH, WT_TENTH
    };
#elif __JHERETIC__
    static weapontype_t wp_list[] = {
        WT_FIRST, WT_SECOND, WT_THIRD, WT_FOURTH, WT_FIFTH,
        WT_SIXTH, WT_SEVENTH, WT_EIGHTH
    };

#elif __JHEXEN__ || __JSTRIFE__
    static weapontype_t wp_list[] = {
        WT_FIRST, WT_SECOND, WT_THIRD, WT_FOURTH
    };
#endif

#if __JHERETIC__
    lvl = (player->powers[PT_WEAPONLEVEL2]? 1 : 0);
#else
    lvl = 0;
#endif

    // Are we using weapon order preferences for next/previous?
    if(cfg.weaponNextMode)
    {
        list = (weapontype_t*) cfg.weaponOrder;
        prev = !prev; // Invert order.
    }
    else
        list = wp_list;

    // Find the current position in the weapon list.
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        w = list[i];
        if(!cfg.weaponCycleSequential || player->pendingWeapon == WT_NOCHANGE)
        {
            if(w == player->readyWeapon)
                break;
        }
        else if(w == player->pendingWeapon)
            break;
    }

    // Locate the next or previous weapon owned by the player.
    initial = w;
    for(;;)
    {
        // Move the iterator.
        if(prev)
            i--;
        else
            i++;

        if(i < 0)
            i = NUM_WEAPON_TYPES - 1;
        else if(i > NUM_WEAPON_TYPES - 1)
            i = 0;

        w = list[i];

        // Have we circled around?
        if(w == initial)
            break;

        // Available in this game mode? And a valid weapon?
        if((weaponInfo[w][player->class_].mode[lvl].
            gameModeBits & gameModeBits) && player->weapons[w].owned)
            break;
    }

    return w;
}

#if __JHEXEN__
/**
 * Changes the class of the given player. Will not work if the player
 * is currently morphed.
 */
void P_PlayerChangeClass(player_t* player, playerclass_t newClass)
{
    int i;

    // Don't change if morphed.
    if(player->morphTics) return;
    if(!PCLASS_INFO(newClass)->userSelectable) return;

    player->class_ = newClass;
    cfg.playerClass[player - players] = newClass;
    P_ClassForPlayerWhenRespawning(player - players, true /*clear change request*/);

    // Take away armor.
    for(i = 0; i < NUMARMOR; ++i)
    {
        player->armorPoints[i] = 0;
    }
    player->update |= PSF_ARMOR_POINTS;

    P_PostMorphWeapon(player, WT_FIRST);

    if(player->plr->mo)
    {
        // Respawn the player and destroy the old mobj.
        mobj_t* oldMo = player->plr->mo;

        P_SpawnPlayer(player - players, newClass, oldMo->origin[VX],
                      oldMo->origin[VY], oldMo->origin[VZ], oldMo->angle, 0,
                      P_MobjIsCamera(oldMo), true);
        P_MobjRemove(oldMo, true);
    }
}
#endif

void P_SetMessage(player_t* pl, int flags, const char* msg)
{
    if(!msg || !msg[0]) return;

    ST_LogPost(pl - players, flags, msg);

    if(pl == &players[CONSOLEPLAYER] && cfg.echoMsg)
        Con_FPrintf(CPF_CYAN, "%s\n", msg);

    // Servers are responsible for sending these messages to the clients.
    NetSv_SendMessage(pl - players, msg);
}

#if __JHEXEN__
void P_SetYellowMessage(player_t* pl, int flags, const char* msg)
{
#define YELLOW_FMT      "{r=1;g=0.7;b=0.3;}"
#define YELLOW_FMT_LEN  18

    size_t len;
    AutoStr* buf;

    if(!msg || !msg[0]) return;
    len = strlen(msg);

    buf = AutoStr_NewStd();
    Str_Reserve(buf, YELLOW_FMT_LEN + len+1);
    Str_Set(buf, YELLOW_FMT);
    Str_Appendf(buf, "%s", msg);

    ST_LogPost(pl - players, flags, Str_Text(buf));

    if(pl == &players[CONSOLEPLAYER] && cfg.echoMsg)
        Con_FPrintf(CPF_CYAN, "%s\n", msg);

    // Servers are responsible for sending these messages to the clients.
    /// @todo We shouldn't need to send the format string along with every
    /// important game message. Instead flag a bit in the packet and then
    /// reconstruct on the other end.
    NetSv_SendMessage(pl - players, Str_Text(buf));

#undef YELLOW_FMT
}
#endif

void P_Thrust3D(player_t* player, angle_t angle, float lookdir,
                coord_t forwardMove, coord_t sideMove)
{
    angle_t pitch = LOOKDIR2DEG(lookdir) / 360 * ANGLE_MAX;
    angle_t sideangle = angle - ANG90;
    mobj_t* mo = player->plr->mo;
    coord_t zmul, mom[3];

    angle >>= ANGLETOFINESHIFT;
    pitch >>= ANGLETOFINESHIFT;
    mom[MX] = forwardMove * FIX2FLT(finecosine[angle]);
    mom[MY] = forwardMove * FIX2FLT(finesine[angle]);
    mom[MZ] = forwardMove * FIX2FLT(finesine[pitch]);

    zmul = FIX2FLT(finecosine[pitch]);
    mom[MX] *= zmul;
    mom[MY] *= zmul;

    sideangle >>= ANGLETOFINESHIFT;
    mom[MX] += sideMove * FIX2FLT(finecosine[sideangle]);
    mom[MY] += sideMove * FIX2FLT(finesine[sideangle]);

    mo->mom[MX] += mom[MX];
    mo->mom[MY] += mom[MY];
    mo->mom[MZ] += mom[MZ];
}

int P_CameraXYMovement(mobj_t *mo)
{
    if(!P_MobjIsCamera(mo))
        return false;
#if __JDOOM__ || __JDOOM64__
    if(mo->flags & MF_NOCLIP ||
       // This is a very rough check! Sometimes you get stuck in things.
       P_CheckPositionXYZ(mo, mo->origin[VX] + mo->mom[MX], mo->origin[VY] + mo->mom[MY], mo->origin[VZ]))
    {
#endif

        P_MobjUnsetOrigin(mo);
        mo->origin[VX] += mo->mom[MX];
        mo->origin[VY] += mo->mom[MY];
        P_MobjSetOrigin(mo);
        P_CheckPositionXY(mo, mo->origin[VX], mo->origin[VY]);
        mo->floorZ = tmFloorZ;
        mo->ceilingZ = tmCeilingZ;

#if __JDOOM__ || __JDOOM64__
    }
#endif

    // Friction.
    if(!INRANGE_OF(mo->player->brain.forwardMove, 0, CAMERA_FRICTION_THRESHOLD) ||
       !INRANGE_OF(mo->player->brain.sideMove, 0, CAMERA_FRICTION_THRESHOLD) ||
       !INRANGE_OF(mo->player->brain.upMove, 0, CAMERA_FRICTION_THRESHOLD))
    {
        // While moving; normal friction applies.
        mo->mom[MX] *= FRICTION_NORMAL;
        mo->mom[MY] *= FRICTION_NORMAL;
    }
    else
    {
        // Else lose momentum, quickly!.
        mo->mom[MX] *= FRICTION_HIGH;
        mo->mom[MY] *= FRICTION_HIGH;
    }

    return true;
}

int P_CameraZMovement(mobj_t *mo)
{
    if(!P_MobjIsCamera(mo))
        return false;

    mo->origin[VZ] += mo->mom[MZ];

    // Friction.
    if(!INRANGE_OF(mo->player->brain.forwardMove, 0, CAMERA_FRICTION_THRESHOLD) ||
       !INRANGE_OF(mo->player->brain.sideMove, 0, CAMERA_FRICTION_THRESHOLD) ||
       !INRANGE_OF(mo->player->brain.upMove, 0, CAMERA_FRICTION_THRESHOLD))
    {   // While moving; normal friction applies.
        mo->mom[MZ] *= FRICTION_NORMAL;
    }
    else
    {   // Else, lose momentum quickly!.
        mo->mom[MZ] *= FRICTION_HIGH;
    }

    return true;
}

/**
 * Set appropriate parameters for a camera.
 */
void P_PlayerThinkCamera(player_t* player)
{
    mobj_t* mo = player->plr->mo;

    if(!mo) return;

    // If this player is not a camera, get out of here.
    if(!(player->plr->flags & DDPF_CAMERA))
    {
        if(player->playerState == PST_LIVE)
        {
            mo->flags |= (MF_SOLID | MF_SHOOTABLE | MF_PICKUP);
        }
        return;
    }

    mo->flags &= ~(MF_SOLID | MF_SHOOTABLE | MF_PICKUP);

    // How about viewlock?
    if(player->viewLock)
    {
        angle_t angle;
        int full;
        coord_t dist;
        mobj_t* target = players->viewLock;

        if(!target->player || !target->player->plr->inGame)
        {
            player->viewLock = NULL;
            return;
        }

        full = player->lockFull;

        angle = M_PointToAngle2(mo->origin, target->origin);
        //player->plr->flags |= DDPF_FIXANGLES;
        /* $unifiedangles */
        mo->angle = angle;
        //player->plr->clAngle = angle;
        player->plr->flags |= DDPF_INTERYAW;

        if(full)
        {
            dist = M_ApproxDistance(mo->origin[VX] - target->origin[VX],
                                    mo->origin[VY] - target->origin[VY]);
            angle = M_PointXYToAngle2(0, 0,
                                      target->origin[VZ] + (target->height / 2) - mo->origin[VZ],
                                      dist);

            player->plr->lookDir = -(angle / (float) ANGLE_MAX * 360.0f - 90);
            if(player->plr->lookDir > 180)
                player->plr->lookDir -= 360;

            player->plr->lookDir *= 110.0f / 85.0f;

            if(player->plr->lookDir > 110)
                player->plr->lookDir = 110;
            if(player->plr->lookDir < -110)
                player->plr->lookDir = -110;

            player->plr->flags |= DDPF_INTERPITCH;
        }
    }
}

D_CMD(SetCamera)
{
    int                 p;
    player_t           *player;

    p = atoi(argv[1]);
    if(p < 0 || p >= MAXPLAYERS)
    {
        Con_Printf("Invalid console number %i.\n", p);
        return false;
    }

    player = &players[p];

    player->plr->flags ^= DDPF_CAMERA;
    if(player->plr->inGame)
    {
        if(player->plr->flags & DDPF_CAMERA)
        {   // Is now a camera.
            if(player->plr->mo)
                player->plr->mo->origin[VZ] += player->viewHeight;
        }
        else
        {   // Is now a "real" player.
            if(player->plr->mo)
                player->plr->mo->origin[VZ] -= player->viewHeight;
        }
    }
    return true;
}

/**
 * Give the player an armor bonus (points delta).
 *
 * @param plr           This.
 * @param points        Points delta.
 *
 * @return              Number of points applied.
 */
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
int P_PlayerGiveArmorBonus(player_t* plr, int points)
#else // __JHEXEN__
int P_PlayerGiveArmorBonus(player_t* plr, armortype_t type, int points)
#endif
{
    int                 delta, oldPoints;
    int*                current;

    if(!points)
        return 0;

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    current = &plr->armorPoints;
#else // __JHEXEN__
    current = &plr->armorPoints[type];
#endif

    oldPoints = *current;
    if(points > 0)
    {
        delta = points; /// @todo No upper limit?
    }
    else
    {
        if(*current + points < 0)
            delta = -(*current);
        else
            delta = points;
    }

    *current += delta;
    if(*current != oldPoints)
        plr->update |= PSF_ARMOR_POINTS;

    return delta;
}

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void P_PlayerSetArmorType(player_t* plr, int type)
{
    int                 oldType = plr->armorType;

    plr->armorType = type;

    if(plr->armorType != oldType)
        plr->update |= PSF_ARMOR_TYPE;
}
#endif

D_CMD(SetViewMode)
{
    int                 pl = CONSOLEPLAYER;

    if(argc > 2)
        return false;

    if(argc == 2)
        pl = atoi(argv[1]);

    if(pl < 0 || pl >= MAXPLAYERS)
        return false;

    if(!(players[pl].plr->flags & DDPF_CHASECAM))
        players[pl].plr->flags |= DDPF_CHASECAM;
    else
        players[pl].plr->flags &= ~DDPF_CHASECAM;
    return true;
}

D_CMD(SetViewLock)
{
    int                 pl = CONSOLEPLAYER, lock;

    if(!stricmp(argv[0], "lockmode"))
    {
        lock = atoi(argv[1]);
        if(lock)
            players[pl].lockFull = true;
        else
            players[pl].lockFull = false;
        return true;
    }
    if(argc < 2)
        return false;

    if(argc >= 3)
        pl = atoi(argv[2]); // Console number.
    lock = atoi(argv[1]);

    if(!(lock == pl || lock < 0 || lock >= MAXPLAYERS))
    {
        if(players[lock].plr->inGame && players[lock].plr->mo)
        {
            players[pl].viewLock = players[lock].plr->mo;
            return true;
        }
    }

    players[pl].viewLock = NULL;
    return false;
}

D_CMD(MakeLocal)
{
    int                 p;
    char                buf[20];
    player_t           *plr;

    if(G_GameState() != GS_MAP)
    {
        Con_Printf("You must be in a game to create a local player.\n");
        return false;
    }

    p = atoi(argv[1]);
    if(p < 0 || p >= MAXPLAYERS)
    {
        Con_Printf("Invalid console number %i.\n", p);
        return false;
    }
    plr = &players[p];

    if(plr->plr->inGame)
    {
        Con_Printf("Player %i is already in the game.\n", p);
        return false;
    }

    plr->playerState = PST_REBORN;
    plr->plr->inGame = true;
    sprintf(buf, "conlocp %i", p);
    DD_Execute(false, buf);
    P_DealPlayerStarts(0);
    return true;
}

/**
 * Print the console player's coordinates.
 */
D_CMD(PrintPlayerCoords)
{
    mobj_t* mo;

    if(G_GameState() != GS_MAP)
        return false;

    if(!(mo = players[CONSOLEPLAYER].plr->mo))
        return false;

    Con_Printf("Console %i: X=%g Y=%g Z=%g\n", CONSOLEPLAYER,
               mo->origin[VX], mo->origin[VY], mo->origin[VZ]);

    return true;
}

D_CMD(CycleSpy)
{
    //// @todo The engine should do this.
    Con_Printf("Spying not allowed.\n");
#if 0
    if(G_GameState() == GS_MAP && !deathmatch)
    {   // Cycle the display player.
        do
        {
            Set(DD_DISPLAYPLAYER, DISPLAYPLAYER + 1);
            if(DISPLAYPLAYER == MAXPLAYERS)
            {
                Set(DD_DISPLAYPLAYER, 0);
            }
        } while(!players[DISPLAYPLAYER].plr->inGame &&
                DISPLAYPLAYER != CONSOLEPLAYER);
    }
#endif
    return true;
}

D_CMD(SpawnMobj)
{
    mobjtype_t type;
    coord_t pos[3];
    mobj_t* mo;
    angle_t angle;
    int spawnFlags = 0;

    if(argc != 5 && argc != 6)
    {
        Con_Printf("Usage: %s (type) (x) (y) (z) (angle)\n", argv[0]);
        Con_Printf("Type must be a defined Thing ID or Name.\n");
        Con_Printf("Z is an offset from the floor, 'floor', 'ceil' or 'random'.\n");
        Con_Printf("Angle (0..360) is optional.\n");
        return true;
    }

    if(IS_CLIENT)
    {
        Con_Printf("%s can't be used by clients.\n", argv[0]);
        return false;
    }

    // First try to find the thing by ID.
    if((type = Def_Get(DD_DEF_MOBJ, argv[1], 0)) < 0)
    {
        // Try to find it by name instead.
        if((type = Def_Get(DD_DEF_MOBJ_BY_NAME, argv[1], 0)) < 0)
        {
            Con_Printf("Undefined thing type %s.\n", argv[1]);
            return false;
        }
    }

    // The coordinates.
    pos[VX] = strtod(argv[2], 0);
    pos[VY] = strtod(argv[3], 0);
    pos[VZ] = 0;

    if(!stricmp(argv[4], "ceil"))
        spawnFlags |= MSF_Z_CEIL;
    else if(!stricmp(argv[4], "random"))
        spawnFlags |= MSF_Z_RANDOM;
    else
    {
        spawnFlags |= MSF_Z_FLOOR;
        if(stricmp(argv[4], "floor"))
            pos[VZ] = strtod(argv[4], 0);
    }

    if(argc == 6)
        angle = ((int) (strtod(argv[5], 0) / 360 * FRACUNIT)) << 16;
    else
        angle = 0;

    if((mo = P_SpawnMobj(type, pos, angle, spawnFlags)))
    {
#if __JDOOM64__
        // jd64 > kaiser - another cheesy hack!!!
        if(mo->type == MT_DART)
        {
            S_StartSound(SFX_SKESWG, mo); // We got darts! spawn skeswg sound!
        }
        else
        {
            S_StartSound(SFX_ITMBK, mo); // If not dart, then spawn itmbk sound
            mo->translucency = 255;
            mo->spawnFadeTics = 0;
            mo->intFlags |= MIF_FADE;
        }
    // << d64tc
#endif
    }

    return true;
}
