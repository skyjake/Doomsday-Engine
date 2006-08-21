/* DE1: $Id: template.c 2645 2006-01-21 12:58:39Z skyjake $
 * Copyright (C) 1999- Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/*
 * p_inventory.c: Common code for the player's inventory.
 *
 * Note:
 * The visual representation of which is handled in the HUD code.
 *
 * Compiles for jHeretic and jHexen.
 */

#if __JHERETIC__ || __JHEXEN__

// HEADER FILES ------------------------------------------------------------

#if __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#  include "p_player.h" // for P_SetYellowMessage()
#endif

#include "d_net.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

#if __JHEXEN__
boolean     P_UsePuzzleItem(player_t *player, int itemType);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int             inv_ptr;
boolean         usearti = true;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Returns true if artifact accepted.
 */
boolean P_GiveArtifact(player_t *player, artitype_e arti, mobj_t *mo)
{
    int     i;
#if __JHEXEN__
    int     j;
    boolean slidePointer = false;
#endif

    if(!player || arti < arti_none + 1 || arti > NUMARTIFACTS - 1)
        return false;

    player->update |= PSF_INVENTORY;

    i = 0;
    while(player->inventory[i].type != arti && i < player->inventorySlotNum)
        i++;

    if(i == player->inventorySlotNum)
    {
#if __JHEXEN__
        if(arti < arti_firstpuzzitem)
        {
            i = 0;
            while(player->inventory[i].type < arti_firstpuzzitem &&
                  i < player->inventorySlotNum)
            {
                i++;
            }
            if(i != player->inventorySlotNum)
            {
                for(j = player->inventorySlotNum; j > i; j--)
                {
                    player->inventory[j].count =
                        player->inventory[j - 1].count;
                    player->inventory[j].type = player->inventory[j - 1].type;
                    slidePointer = true;
                }
            }
        }
#endif
        player->inventory[i].count = 1;
        player->inventory[i].type = arti;
        player->inventorySlotNum++;
    }
    else
    {
#if __JHEXEN__
        if(arti >= arti_firstpuzzitem && IS_NETGAME && !deathmatch)
        {                       // Can't carry more than 1 puzzle item in coop netplay
            return false;
        }
#endif
        if(player->inventory[i].count >= MAXARTICOUNT)
        {                       // Player already has max number of this item
            return false;
        }
        player->inventory[i].count++;
    }

    if(!player->artifactCount)
    {
        player->readyArtifact = arti;
    }
#if __JHEXEN__
    else if(player == &players[consoleplayer] && slidePointer && i <= inv_ptr)
    {
        inv_ptr++;
        curpos++;
        if(curpos > 6)
        {
            curpos = 6;
        }
    }
#else
    if(mo && (mo->flags & MF_COUNTITEM))
    {
        player->itemcount++;
    }
#endif

    player->artifactCount++;

    return true;
}

# if __JHERETIC__
void P_InventoryCheckReadyArtifact(player_t *player)
{
    if(!player || player != &players[consoleplayer])
        return;

    if(!player->inventory[inv_ptr].count)
    {
        // Set position markers and get next readyArtifact
        inv_ptr--;
        if(inv_ptr < 6)
        {
            curpos--;
            if(curpos < 0)
            {
                curpos = 0;
            }
        }
        if(inv_ptr >= player->inventorySlotNum)
        {
            inv_ptr = player->inventorySlotNum - 1;
        }
        if(inv_ptr < 0)
        {
            inv_ptr = 0;
        }
        player->readyArtifact = player->inventory[inv_ptr].type;

        if(!player->inventorySlotNum)
            player->readyArtifact = arti_none;
    }
}
# endif

void P_InventoryNextArtifact(player_t *player)
{
    if(!player || player != &players[consoleplayer])
        return;

    inv_ptr--;
    if(inv_ptr < 6)
    {
        curpos--;
        if(curpos < 0)
        {
            curpos = 0;
        }
    }
    if(inv_ptr < 0)
    {
        inv_ptr = player->inventorySlotNum - 1;
        if(inv_ptr < 6)
        {
            curpos = inv_ptr;
        }
        else
        {
            curpos = 6;
        }
    }
    player->readyArtifact = player->inventory[inv_ptr].type;
}

void P_InventoryRemoveArtifact(player_t *player, int slot)
{
    int     i;

    if(!player || slot < 0 || slot > NUMINVENTORYSLOTS)
        return;

    player->update |= PSF_INVENTORY;
    player->artifactCount--;

    if(!(--player->inventory[slot].count))
    {
        // Used last of a type - compact the artifact list
        player->readyArtifact = arti_none;
        player->inventory[slot].type = arti_none;

        for(i = slot + 1; i < player->inventorySlotNum; i++)
        {
            player->inventory[i - 1] = player->inventory[i];
        }

        player->inventorySlotNum--;

        if(player == &players[consoleplayer])
        {   // Set position markers and get next readyArtifact
            inv_ptr--;
            if(inv_ptr < 6)
            {
                curpos--;
                if(curpos < 0)
                    curpos = 0;
            }

            if(inv_ptr >= player->inventorySlotNum)
                inv_ptr = player->inventorySlotNum - 1;

            if(inv_ptr < 0)
                inv_ptr = 0;

            player->readyArtifact = player->inventory[inv_ptr].type;
        }
    }
}

boolean P_InventoryUseArtifact(player_t *player, artitype_e arti)
{
    int     i;
    boolean success = false;
# if __JHERETIC__
    boolean play_sound = false;
# endif

    if(!player || arti < arti_none + 1 || arti > NUMARTIFACTS - 1)
        return false;

    for(i = 0; i < player->inventorySlotNum; i++)
    {
        if(player->inventory[i].type == arti)
        {
            // Found match - try to use
            if((success = P_UseArtifactOnPlayer(player, arti)) == true)
            {
                // Artifact was used - remove it from inventory
                P_InventoryRemoveArtifact(player, i);
# if __JHERETIC__
                play_sound = true;
# else
                if(arti < arti_firstpuzzitem)
                {
                    S_ConsoleSound(SFX_ARTIFACT_USE, NULL, player - players);
                }
                else
                {
                    S_ConsoleSound(SFX_PUZZLE_SUCCESS, NULL, player - players);
                }
# endif
                ST_InventoryFlashCurrent(player);
            }
            else if(cfg.inventoryNextOnUnuse)
            {
                // Unable to use artifact, advance pointer?
# if __JHEXEN__
                if(arti < arti_firstpuzzitem)
#endif
                    P_InventoryNextArtifact(player);
            }
            break;
        }
    }

# if __JHERETIC__
    if(play_sound)
        S_ConsoleSound(sfx_artiuse, NULL, player - players);
# endif

    return success;
}

/*
 * Returns true if the artifact was used.
 */
boolean P_UseArtifactOnPlayer(player_t *player, artitype_e arti)
{
    mobj_t *mo;
    angle_t angle;
# if __JHEXEN__
    int     i;
    int     count;
# endif

    if(!player || arti < arti_none + 1 || arti > NUMARTIFACTS - 1)
        return false;

    switch (arti)
    {
    case arti_invulnerability:
        if(!P_GivePower(player, pw_invulnerability))
        {
            return (false);
        }
        break;
# if __JHERETIC__
    case arti_invisibility:
        if(!P_GivePower(player, pw_invisibility))
        {
            return (false);
        }
        break;
# endif
    case arti_health:
        if(!P_GiveBody(player, 25))
        {
            return (false);
        }
        break;
    case arti_superhealth:
        if(!P_GiveBody(player, 100))
        {
            return (false);
        }
        break;
# if __JHEXEN__
    case arti_healingradius:
        if(!P_HealRadius(player))
        {
            return (false);
        }
        break;
# endif
# if __JHERETIC__
    case arti_tomeofpower:
        if(player->morphTics)
        {                       // Attempt to undo chicken
            if(P_UndoPlayerMorph(player) == false)
            {                   // Failed
                P_DamageMobj(player->plr->mo, NULL, NULL, 10000);
            }
            else
            {                   // Succeeded
                player->morphTics = 0;
                S_StartSound(sfx_wpnup, player->plr->mo);
            }
        }
        else
        {
            if(!P_GivePower(player, pw_weaponlevel2))
            {
                return (false);
            }
            if(player->readyweapon == WP_FIRST)
            {
                P_SetPsprite(player, ps_weapon, S_STAFFREADY2_1);
            }
            else if(player->readyweapon == WP_EIGHTH)
            {
                P_SetPsprite(player, ps_weapon, S_GAUNTLETREADY2_1);
            }
        }
        break;
# endif
    case arti_torch:
        if(!P_GivePower(player, pw_infrared))
        {
            return (false);
        }
        break;
# if __JHERETIC__
    case arti_firebomb:
        angle = player->plr->mo->angle >> ANGLETOFINESHIFT;
        mo = P_SpawnMobj(player->plr->mo->pos[VX] + 24 * finecosine[angle],
                         player->plr->mo->pos[VY] + 24 * finesine[angle],
                         player->plr->mo->pos[VZ] - player->plr->mo->floorclip +
                         15 * FRACUNIT, MT_FIREBOMB);
        mo->target = player->plr->mo;
        break;
# endif
    case arti_egg:
        mo = player->plr->mo;
        P_SpawnPlayerMissile(mo, MT_EGGFX);
        P_SPMAngle(mo, MT_EGGFX, mo->angle - (ANG45 / 6));
        P_SPMAngle(mo, MT_EGGFX, mo->angle + (ANG45 / 6));
        P_SPMAngle(mo, MT_EGGFX, mo->angle - (ANG45 / 3));
        P_SPMAngle(mo, MT_EGGFX, mo->angle + (ANG45 / 3));
        break;
    case arti_teleport:
        P_ArtiTele(player);
        break;
    case arti_fly:
        if(!P_GivePower(player, pw_flight))
        {
            return (false);
        }
# if __JHEXEN__
        if(player->plr->mo->momz <= -35 * FRACUNIT)
        {   // stop falling scream
            S_StopSound(0, player->plr->mo);
        }
# endif
        break;
# if __JHEXEN__
    case arti_summon:
        mo = P_SpawnPlayerMissile(player->plr->mo, MT_SUMMON_FX);
        if(mo)
        {
            mo->target = player->plr->mo;
            mo->tracer = player->plr->mo;
            mo->momz = 5 * FRACUNIT;
        }
        break;
    case arti_teleportother:
        P_ArtiTeleportOther(player);
        break;
    case arti_poisonbag:
        angle = player->plr->mo->angle >> ANGLETOFINESHIFT;
        if(player->class == PCLASS_CLERIC)
        {
            mo = P_SpawnMobj(player->plr->mo->pos[VX] + 16 * finecosine[angle],
                             player->plr->mo->pos[VY] + 24 * finesine[angle],
                             player->plr->mo->pos[VZ] - player->plr->mo->floorclip +
                             8 * FRACUNIT, MT_POISONBAG);
            if(mo)
            {
                mo->target = player->plr->mo;
            }
        }
        else if(player->class == PCLASS_MAGE)
        {
            mo = P_SpawnMobj(player->plr->mo->pos[VX] + 16 * finecosine[angle],
                             player->plr->mo->pos[VY] + 24 * finesine[angle],
                             player->plr->mo->pos[VZ] - player->plr->mo->floorclip +
                             8 * FRACUNIT, MT_FIREBOMB);
            if(mo)
            {
                mo->target = player->plr->mo;
            }
        }
        else // PCLASS_FIGHTER, obviously (also pig, not so obviously)
        {
            mo = P_SpawnMobj(player->plr->mo->pos[VX],
                             player->plr->mo->pos[VY],
                             player->plr->mo->pos[VZ] - player->plr->mo->floorclip +
                             35 * FRACUNIT, MT_THROWINGBOMB);
            if(mo)
            {
                mo->angle =
                    player->plr->mo->angle + (((P_Random() & 7) - 4) << 24);
                mo->momz =
                    4 * FRACUNIT +
                    (((int) player->plr->lookdir) << (FRACBITS - 4));
                mo->pos[VZ] += ((int) player->plr->lookdir) << (FRACBITS - 4);
                P_ThrustMobj(mo, mo->angle, mo->info->speed);
                mo->momx += player->plr->mo->momx >> 1;
                mo->momy += player->plr->mo->momy >> 1;
                mo->target = player->plr->mo;
                mo->tics -= P_Random() & 3;
                P_CheckMissileSpawn(mo);
            }
        }
        break;
    case arti_speed:
        if(!P_GivePower(player, pw_speed))
        {
            return (false);
        }
        break;
    case arti_boostmana:
        if(!P_GiveMana(player, MANA_1, MAX_MANA))
        {
            if(!P_GiveMana(player, MANA_2, MAX_MANA))
            {
                return false;
            }

        }
        else
        {
            P_GiveMana(player, MANA_2, MAX_MANA);
        }
        break;
    case arti_boostarmor:
        count = 0;

        for(i = 0; i < NUMARMOR; i++)
        {
            count += P_GiveArmor(player, i, 1); // 1 point per armor type
        }
        if(!count)
        {
            return false;
        }
        break;
    case arti_blastradius:
        P_BlastRadius(player);
        break;

    case arti_puzzskull:
    case arti_puzzgembig:
    case arti_puzzgemred:
    case arti_puzzgemgreen1:
    case arti_puzzgemgreen2:
    case arti_puzzgemblue1:
    case arti_puzzgemblue2:
    case arti_puzzbook1:
    case arti_puzzbook2:
    case arti_puzzskull2:
    case arti_puzzfweapon:
    case arti_puzzcweapon:
    case arti_puzzmweapon:
    case arti_puzzgear1:
    case arti_puzzgear2:
    case arti_puzzgear3:
    case arti_puzzgear4:
        if(P_UsePuzzleItem(player, arti - arti_firstpuzzitem))
        {
            return true;
        }
        else
        {
            P_SetYellowMessage(player, TXT_USEPUZZLEFAILED, false);
            return false;
        }
        break;
# endif
    default:
        return false;
    }
    return true;
}

/*
 * Does not bother to check the validity of the params as the only
 * caller is DEFCC(CCmdInventory) (bellow).
 */
static boolean P_InventoryMove(player_t *plr, int dir)
{
    if(!ST_IsInventoryVisible())
    {
        ST_Inventory(true);
        return false;
    }

    ST_Inventory(true); // reset the inventory auto-hide timer

    if(dir == 0)
    {
        inv_ptr--;
        if(inv_ptr < 0)
        {
            inv_ptr = 0;
        }
        else
        {
            curpos--;
            if(curpos < 0)
            {
                curpos = 0;
            }
        }
    }
    else
    {
        inv_ptr++;
        if(inv_ptr >= plr->inventorySlotNum)
        {
            inv_ptr--;
            if(inv_ptr < 0)
                inv_ptr = 0;
        }
        else
        {
            curpos++;
            if(curpos > 6)
            {
                curpos = 6;
            }
        }
    }
    return true;
}

/*
 * Move the inventory selector
 */
DEFCC(CCmdInventory)
{
    P_InventoryMove(players + consoleplayer, !stricmp(argv[0], "invright"));
    return true;
}
#endif
