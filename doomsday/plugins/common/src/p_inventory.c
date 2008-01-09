/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/**
 * p_inventory.c: Common code for the player's inventory.
 *
 * \note The visual representation of the inventory is handled separately,
 * 		 in the HUD code.
 * \bug This file currently contains the various "do" functions for the
 *		artifacts which are Raven code. They should be moved out of here and
 *		interfaced through a callback interface.
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

boolean         usearti = true;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * @return              @c true, if artifact accepted.
 */
boolean P_GiveArtifact(player_t *player, artitype_e arti, mobj_t *mo)
{
    int         i;
#if __JHEXEN__
    int         j;
    boolean     slidePointer = false;
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
        {   // Can't carry more than 1 puzzle item in coop netplay.
            return false;
        }
#endif
        if(player->inventory[i].count >= MAXARTICOUNT)
        {   // Player already has max number of this item.
            return false;
        }

        player->inventory[i].count++;
    }

    if(!player->artifactCount)
    {
        player->readyArtifact = arti;
    }
#if __JHEXEN__
    else if(slidePointer && i <= player->invPtr)
    {
        player->invPtr++;
        player->curPos++;
        if(player->curPos > 6)
        {
            player->curPos = 6;
        }
    }
#else
    if(mo && (mo->flags & MF_COUNTITEM))
    {
        player->itemCount++;
    }
#endif

    player->artifactCount++;

    // Maybe unhide the HUD?
    if(player == &players[consoleplayer])
        ST_HUDUnHide(HUE_ON_PICKUP_INVITEM);
    return true;
}

# if __JHERETIC__
void P_InventoryCheckReadyArtifact(player_t *player)
{
    if(!player)
        return;

    if(!player->inventory[player->invPtr].count)
    {
        // Set position markers and get next readyArtifact
        player->invPtr--;
        if(player->invPtr < 6)
        {
            player->curPos--;
            if(player->curPos < 0)
            {
                player->curPos = 0;
            }
        }

        if(player->invPtr >= player->inventorySlotNum)
        {
            player->invPtr = player->inventorySlotNum - 1;
        }

        if(player->invPtr < 0)
        {
            player->invPtr = 0;
        }

        player->readyArtifact = player->inventory[player->invPtr].type;

        if(!player->inventorySlotNum)
            player->readyArtifact = arti_none;
    }
}
# endif

void P_InventoryNextArtifact(player_t *player)
{
    if(!player)
        return;

    player->invPtr--;
    if(player->invPtr < 6)
    {
        player->curPos--;
        if(player->curPos < 0)
        {
            player->curPos = 0;
        }
    }

    if(player->invPtr < 0)
    {
        player->invPtr = player->inventorySlotNum - 1;
        if(player->invPtr < 6)
        {
            player->curPos = player->invPtr;
        }
        else
        {
            player->curPos = 6;
        }
    }

    player->readyArtifact = player->inventory[player->invPtr].type;
}

void P_InventoryRemoveArtifact(player_t *player, int slot)
{
    int         i;

    if(!player || slot < 0 || slot > NUMINVENTORYSLOTS)
        return;

    player->update |= PSF_INVENTORY;
    player->artifactCount--;

    if(!(--player->inventory[slot].count))
    {   // Used last of a type - compact the artifact list
        player->readyArtifact = arti_none;
        player->inventory[slot].type = arti_none;

        for(i = slot + 1; i < player->inventorySlotNum; ++i)
        {
            player->inventory[i - 1] = player->inventory[i];
        }

        player->inventorySlotNum--;

        // Set position markers and get next readyArtifact.
        player->invPtr--;
        if(player->invPtr < 6)
        {
            player->curPos--;
            if(player->curPos < 0)
                player->curPos = 0;
        }

        if(player->invPtr >= player->inventorySlotNum)
            player->invPtr = player->inventorySlotNum - 1;

        if(player->invPtr < 0)
            player->invPtr = 0;

        player->readyArtifact = player->inventory[player->invPtr].type;
    }
}

boolean P_InventoryUseArtifact(player_t *player, artitype_e arti)
{
    int         i;
    boolean     success = false;
# if __JHERETIC__
    boolean     play_sound = false;
# endif

    if(!player || arti < arti_none + 1 || arti > NUMARTIFACTS - 1)
        return false;

    for(i = 0; i < player->inventorySlotNum; ++i)
    {
        if(player->inventory[i].type == arti)
        {   // Found match - try to use.
            if((success = P_UseArtifactOnPlayer(player, arti)) == true)
            {
                // Artifact was used - remove it from inventory.
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

void P_InventoryResetCursor(player_t *player)
{
    if(!player)
        return;

    player->invPtr = 0;
    player->curPos = 0;
}

/**
 * @return              @c true, if the artifact was used.
 */
boolean P_UseArtifactOnPlayer(player_t *player, artitype_e arti)
{
    mobj_t      *mo;
    angle_t     angle;
# if __JHEXEN__
    int         i;
    int         count;
# endif

    if(!player || arti < arti_none + 1 || arti > NUMARTIFACTS - 1)
        return false;

    switch (arti)
    {
    case arti_invulnerability:
        if(!P_GivePower(player, PT_INVULNERABILITY))
        {
            return (false);
        }
        break;
# if __JHERETIC__
    case arti_invisibility:
        if(!P_GivePower(player, PT_INVISIBILITY))
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
        {   // Attempt to undo chicken.
            if(P_UndoPlayerMorph(player) == false)
            {   // Failed.
                P_DamageMobj(player->plr->mo, NULL, NULL, 10000);
            }
            else
            {   // Succeeded.
                player->morphTics = 0;
                S_StartSound(sfx_wpnup, player->plr->mo);
            }
        }
        else
        {
            if(!P_GivePower(player, PT_WEAPONLEVEL2))
            {
                return false;
            }

            if(player->readyWeapon == WT_FIRST)
            {
                P_SetPsprite(player, ps_weapon, S_STAFFREADY2_1);
            }
            else if(player->readyWeapon == WT_EIGHTH)
            {
                P_SetPsprite(player, ps_weapon, S_GAUNTLETREADY2_1);
            }
        }
        break;
# endif
    case arti_torch:
        if(!P_GivePower(player, PT_INFRARED))
        {
            return (false);
        }
        break;
# if __JHERETIC__
    case arti_firebomb:
        angle = player->plr->mo->angle >> ANGLETOFINESHIFT;
        mo = P_SpawnMobj3f(MT_FIREBOMB,
                           player->plr->mo->pos[VX] + 24 * FIX2FLT(finecosine[angle]),
                           player->plr->mo->pos[VY] + 24 * FIX2FLT(finesine[angle]),
                           player->plr->mo->pos[VZ] - player->plr->mo->floorClip + 15);
        mo->target = player->plr->mo;
        break;
# endif
    case arti_egg:
        mo = player->plr->mo;
# if __JHEXEN__
        P_SpawnPlayerMissile(MT_EGGFX, mo);
        P_SPMAngle(MT_EGGFX, mo, mo->angle - (ANG45 / 6));
        P_SPMAngle(MT_EGGFX, mo, mo->angle + (ANG45 / 6));
        P_SPMAngle(MT_EGGFX, mo, mo->angle - (ANG45 / 3));
        P_SPMAngle(MT_EGGFX, mo, mo->angle + (ANG45 / 3));
# else
        P_SpawnMissile(MT_EGGFX, mo, NULL);
        P_SpawnMissileAngle(MT_EGGFX, mo, mo->angle - (ANG45 / 6), -12345);
        P_SpawnMissileAngle(MT_EGGFX, mo, mo->angle + (ANG45 / 6), -12345);
        P_SpawnMissileAngle(MT_EGGFX, mo, mo->angle - (ANG45 / 3), -12345);
        P_SpawnMissileAngle(MT_EGGFX, mo, mo->angle + (ANG45 / 3), -12345);
# endif
        break;
    case arti_teleport:
        P_ArtiTele(player);
        break;
    case arti_fly:
        if(!P_GivePower(player, PT_FLIGHT))
        {
            return false;
        }
# if __JHEXEN__
        if(player->plr->mo->mom[MZ] <= -35)
        {   // Stop falling scream.
            S_StopSound(0, player->plr->mo);
        }
# endif
        break;
# if __JHEXEN__
    case arti_summon:
        mo = P_SpawnPlayerMissile(MT_SUMMON_FX, player->plr->mo);
        if(mo)
        {
            mo->target = player->plr->mo;
            mo->tracer = player->plr->mo;
            mo->mom[MZ] = 5;
        }
        break;
    case arti_teleportother:
        P_ArtiTeleportOther(player);
        break;
    case arti_poisonbag:
        angle = player->plr->mo->angle >> ANGLETOFINESHIFT;
        if(player->class == PCLASS_CLERIC)
        {
            mo = P_SpawnMobj3f(MT_POISONBAG,
                               player->plr->mo->pos[VX] + 16 * FIX2FLT(finecosine[angle]),
                               player->plr->mo->pos[VY] + 24 * FIX2FLT(finesine[angle]),
                               player->plr->mo->pos[VZ] - player->plr->mo->floorClip + 8);
            if(mo)
            {
                mo->target = player->plr->mo;
            }
        }
        else if(player->class == PCLASS_MAGE)
        {
            mo = P_SpawnMobj3f(MT_FIREBOMB,
                               player->plr->mo->pos[VX] + 16 * FIX2FLT(finecosine[angle]),
                               player->plr->mo->pos[VY] + 24 * FIX2FLT(finesine[angle]),
                               player->plr->mo->pos[VZ] - player->plr->mo->floorClip + 8);
            if(mo)
            {
                mo->target = player->plr->mo;
            }
        }
        else // PCLASS_FIGHTER, obviously (also pig, not so obviously)
        {
            mo = P_SpawnMobj3f(MT_THROWINGBOMB,
                               player->plr->mo->pos[VX],
                               player->plr->mo->pos[VY],
                               player->plr->mo->pos[VZ] - player->plr->mo->floorClip + 35);
            if(mo)
            {
                mo->angle =
                    player->plr->mo->angle + (((P_Random() & 7) - 4) << 24);
                mo->mom[MZ] =
                    4 +
                    FIX2FLT(((int) player->plr->lookDir) << (FRACBITS - 4));
                mo->pos[VZ] += FIX2FLT(((int) player->plr->lookDir) << (FRACBITS - 4));
                P_ThrustMobj(mo, mo->angle, mo->info->speed);
                mo->mom[MX] += player->plr->mo->mom[MX] / 2;
                mo->mom[MY] += player->plr->mo->mom[MY] / 2;
                mo->target = player->plr->mo;
                mo->tics -= P_Random() & 3;
                P_CheckMissileSpawn(mo);
            }
        }
        break;

    case arti_speed:
        if(!P_GivePower(player, PT_SPEED))
        {
            return false;
        }
        break;

    case arti_boostmana:
        if(!P_GiveMana(player, AT_BLUEMANA, MAX_MANA))
        {
            if(!P_GiveMana(player, AT_GREENMANA, MAX_MANA))
            {
                return false;
            }
        }
        else
        {
            P_GiveMana(player, AT_GREENMANA, MAX_MANA);
        }
        break;

    case arti_boostarmor:
        count = 0;

        for(i = 0; i < NUMARMOR; ++i)
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

/**
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
        plr->invPtr--;
        if(plr->invPtr < 0)
        {
            plr->invPtr = 0;
        }
        else
        {
            plr->curPos--;
            if(plr->curPos < 0)
            {
                plr->curPos = 0;
            }
        }
    }
    else
    {
        plr->invPtr++;
        if(plr->invPtr >= plr->inventorySlotNum)
        {
            plr->invPtr--;
            if(plr->invPtr < 0)
                plr->invPtr = 0;
        }
        else
        {
            plr->curPos++;
            if(plr->curPos > 6)
            {
                plr->curPos = 6;
            }
        }
    }
    return true;
}

/**
 * Move the inventory selector
 */
DEFCC(CCmdInventory)
{
    int         player = consoleplayer;

    if(argc > 2)
    {
        Con_Printf("Usage: %s (player)\n", argv[0]);
        Con_Printf("If player is not specified, will default to consoleplayer.\n");
        return true;
    }

    if(argc == 2)
        player = strtol(argv[1], NULL, 0);

    if(player < 0)
        player = 0;
    if(player > MAXPLAYERS -1)
        player = MAXPLAYERS -1;

    P_InventoryMove(&players[player], !stricmp(argv[0], "invright"));
    return true;
}
#endif
