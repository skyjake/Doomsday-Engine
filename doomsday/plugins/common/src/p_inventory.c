/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * p_inventory.c: Common code for the player's inventory.
 *
 * \note The visual representation of the inventory is handled separately,
 *       in the HUD code.
 */

#if __JHERETIC__ || __JHEXEN__

// HEADER FILES ------------------------------------------------------------

#if __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "p_player.h"
#include "d_net.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean         usearti = true;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Retrieve the number of artifacts possessed by the player.
 *
 * @param player        Player to check the inventory of.
 * @param arti          Artifact type requirement, limits the count to the
 *                      specified type else @c AFT_NONE to count all.
 * @return              Result.
 */
uint P_InventoryCount(player_t* player, artitype_e arti)
{
    uint                count = 0;

    if(player && (arti == AFT_NONE ||
                 (arti >= AFT_FIRST && arti < NUM_ARTIFACT_TYPES)))
    {
        uint                i;

        for(i = 0; i < player->inventorySlotNum; ++i)
        {
            if(arti != AFT_NONE && player->inventory[i].type != arti)
                continue;

            count += player->inventory[i].count;
        }
    }

    return count;
}

/**
 * Give one artifact of the specified type to the player.
 *
 * @param player        Player to give the artifact to.
 * @param arti          Type of artifact to give.
 *
 * @return              @c true, if artifact accepted.
 */
boolean P_InventoryGive(player_t* player, artitype_e arti)
{
    uint                i;
    uint                oldNumArtifacts;
    boolean             givenArtifact = false;

    if(!player || arti < AFT_NONE + 1 || arti > NUM_ARTIFACT_TYPES - 1)
        return false;

    // Count total number of owned artifacts.
    oldNumArtifacts = P_InventoryCount(player, AFT_NONE);

    i = 0;
    while(player->inventory[i].type != arti && i < player->inventorySlotNum)
        i++;

    if(i == player->inventorySlotNum)
    {
# if __JHEXEN__
        if(arti < AFT_FIRSTPUZZITEM)
        {
            i = 0;
            while(player->inventory[i].type < AFT_FIRSTPUZZITEM &&
                  i < player->inventorySlotNum)
                i++;

            if(i != player->inventorySlotNum)
            {
                uint                j;

                for(j = player->inventorySlotNum; j > i; j--)
                {
                    player->inventory[j].count =
                        player->inventory[j - 1].count;
                    player->inventory[j].type = player->inventory[j - 1].type;
                }
            }
        }
# endif
        player->inventory[i].count = 1;
        player->inventory[i].type = arti;
        player->inventorySlotNum++;
        player->update |= PSF_INVENTORY;

        givenArtifact = true;
    }
    else
    {
# if __JHEXEN__
        if(arti >= AFT_FIRSTPUZZITEM && IS_NETGAME && !deathmatch)
        {   // Can't carry more than 1 puzzle item in coop netplay.
            return false;
        }
# endif
        if(player->inventory[i].count >= MAXARTICOUNT)
        {   // Player already has max number of this item.
            return false;
        }

        player->inventory[i].count++;
        player->update |= PSF_INVENTORY;

        givenArtifact = true;
    }

    if(givenArtifact)
    {
        int                 plrnum = player - players;

        if(oldNumArtifacts == 0)
        {   // This is the first artifact the player has been given; ready it.
            ST_InventorySelect(plrnum, arti);
        }

        // Maybe unhide the HUD?
        ST_HUDUnHide(plrnum, HUE_ON_PICKUP_INVITEM);
    }

    return true;
}

/**
 * Take one of the specified artifact from the player (if owned).
 */
void P_InventoryTake(player_t* player, artitype_e arti)
{
    uint                i;

    if(!player || arti < AFT_NONE + 1 || arti > NUM_ARTIFACT_TYPES - 1)
        return;

    player->update |= PSF_INVENTORY;

    for(i = 0; i < player->inventorySlotNum; ++i)
    {
        if(player->inventory[i].type != arti)
            continue;

        if(!(--player->inventory[i].count))
        {   // Used last of a type - compact the artifact list
            uint                j;

            player->readyArtifact = AFT_NONE;
            player->inventory[i].type = AFT_NONE;

            for(j = i + 1; j < player->inventorySlotNum; ++j)
            {
                player->inventory[j - 1] = player->inventory[j];
            }

            player->inventorySlotNum--;

            // Set position markers and get next readyArtifact.
            ST_InventoryMove(player - players, -1, true);
        }
    }
}

boolean P_InventoryUse(player_t* player, artitype_e arti)
{
    struct {
        boolean (*func) (player_t* player);
        sfxenum_t       useSnd;
    } artifacts[] = {
        {P_UseArtiInvulnerability, SFX_ARTIFACT_USE},
# if __JHERETIC__
        {P_UseArtiInvisibility, SFX_ARTIFACT_USE},
# endif
        {P_UseArtiHealth, SFX_ARTIFACT_USE},
        {P_UseArtiSuperHealth, SFX_ARTIFACT_USE},
# if __JHERETIC__
        {P_UseArtiTombOfPower, SFX_ARTIFACT_USE},
# elif __JHEXEN__
        {P_UseArtiHealRadius, SFX_ARTIFACT_USE},
        {P_UseArtiSummon, SFX_ARTIFACT_USE},
# endif
        {P_UseArtiTorch, SFX_ARTIFACT_USE},
# if __JHERETIC__
        {P_UseArtiFireBomb, SFX_ARTIFACT_USE},
# endif
        {P_UseArtiEgg, SFX_ARTIFACT_USE},
        {P_UseArtiFly, SFX_ARTIFACT_USE},
# if __JHEXEN__
        {P_UseArtiBlastRadius, SFX_ARTIFACT_USE},
        {P_UseArtiPoisonBag, SFX_ARTIFACT_USE},
        {P_UseArtiTeleportOther, SFX_ARTIFACT_USE},
        {P_UseArtiSpeed, SFX_ARTIFACT_USE},
        {P_UseArtiBoostMana, SFX_ARTIFACT_USE},
        {P_UseArtiBoostArmor, SFX_ARTIFACT_USE},
# endif
        {P_UseArtiTeleport, SFX_ARTIFACT_USE},
# if __JHEXEN__
        {P_UseArtiPuzzSkull, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzGemBig, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzGemRed, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzGemGreen1, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzGemGreen2, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzGemBlue1, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzGemBlue2, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzBook1, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzBook2, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzSkull2, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzFWeapon, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzCWeapon, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzMWeapon, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzGear1, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzGear2, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzGear3, SFX_PUZZLE_SUCCESS},
        {P_UseArtiPuzzGear4, SFX_PUZZLE_SUCCESS},
# endif
        {NULL, SFX_NONE}
    };
    uint                i;
    boolean             success = false;

    if(!player || arti < AFT_NONE + 1 || arti > NUM_ARTIFACT_TYPES - 1)
        return false;

    for(i = 0; i < player->inventorySlotNum; ++i)
    {
        if(player->inventory[i].type == arti)
        {   // Found match - try to use.
            if(artifacts[arti - 1].func(player))
            {   // Artifact was used.
                success = true;
                P_InventoryTake(player, arti);
                S_ConsoleSound(artifacts[arti - 1].useSnd, NULL,
                               player - players);

                ST_InventoryFlashCurrent(player - players);
            }
            else
            {   // Unable to use artifact.
                // Set current artifact to the next available?
                if(cfg.inventoryUseNext)
                {
# if __JHEXEN__
                    if(arti < AFT_FIRSTPUZZITEM)
# endif
                        ST_InventoryMove(player - players, -1, true);
                }
            }

            break;
        }
    }

    return success;
}

#endif
