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
    int                 i;
    uint                count;

    if(!player || arti < AFT_NONE + 1 || arti > NUM_ARTIFACT_TYPES - 1)
        return 0;

    count = 0;
    for(i = 0; i < player->inventorySlotNum; ++i)
    {
        if(arti != AFT_NONE && player->inventory[i].type != arti)
            continue;

        count += player->inventory[i].count;
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
    int                 i;
# if __JHEXEN__
    boolean             slidePointer = false;
# endif

    if(!player || arti < AFT_NONE + 1 || arti > NUM_ARTIFACT_TYPES - 1)
        return false;

    player->update |= PSF_INVENTORY;

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
            {
                i++;
            }

            if(i != player->inventorySlotNum)
            {
                int             j;

                for(j = player->inventorySlotNum; j > i; j--)
                {
                    player->inventory[j].count =
                        player->inventory[j - 1].count;
                    player->inventory[j].type = player->inventory[j - 1].type;
                    slidePointer = true;
                }
            }
        }
# endif
        player->inventory[i].count = 1;
        player->inventory[i].type = arti;
        player->inventorySlotNum++;
    }
    else
    {
# if __JHEXEN__
        if(arti >= AFT_FIRSTPUZZITEM && IS_NETGAME && !GAMERULES.deathmatch)
        {   // Can't carry more than 1 puzzle item in coop netplay.
            return false;
        }
# endif
        if(player->inventory[i].count >= MAXARTICOUNT)
        {   // Player already has max number of this item.
            return false;
        }

        player->inventory[i].count++;
    }

    if(!P_InventoryCount(player, AFT_NONE))
    {   // This is the first artifact the player has been given; ready it.
        player->readyArtifact = arti;
    }
# if __JHEXEN__
    else if(slidePointer && i <= player->invPtr)
    {
        player->invPtr++;
        player->curPos++;
        if(player->curPos > 6)
        {
            player->curPos = 6;
        }
    }
# endif

    // Maybe unhide the HUD?
    ST_HUDUnHide(player - players, HUE_ON_PICKUP_INVITEM);

    return true;
}

/**
 * Take one of the specified artifact from the player (if owned).
 */
void P_InventoryTake(player_t* player, int slot)
{
    int                 i;

    if(!player || slot < 0 || slot > NUMINVENTORYSLOTS)
        return;

    player->update |= PSF_INVENTORY;

    if(!(--player->inventory[slot].count))
    {   // Used last of a type - compact the artifact list
        player->readyArtifact = AFT_NONE;
        player->inventory[slot].type = AFT_NONE;

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

# if __JHERETIC__
void P_InventoryCheckReadyArtifact(player_t* player)
{
    if(!player)
        return;

    if(!player->inventory[player->invPtr].count)
    {
        // Set position markers and get next readyArtifact.
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
            player->readyArtifact = AFT_NONE;
    }
}
# endif

void P_InventoryNext(player_t* player)
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
    int                 i;
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
                P_InventoryTake(player, i);
                S_ConsoleSound(artifacts[arti - 1].useSnd, NULL,
                               player - players);

                ST_InventoryFlashCurrent(player - players);
            }
            else
            {   // Unable to use artifact.
                // Set current artifact to the next available?
                if(PLRPROFILE.inventory.nextOnNoUse)
                {
# if __JHEXEN__
                    if(arti < AFT_FIRSTPUZZITEM)
# endif
                        P_InventoryNext(player);
                }
            }

            break;
        }
    }

    return success;
}

void P_InventoryResetCursor(player_t* player)
{
    if(!player)
        return;

    player->invPtr = 0;
    player->curPos = 0;
}

/**
 * Does not bother to check the validity of the params as the only
 * caller is DEFCC(CCmdInventory) (bellow).
 */
static boolean P_InventoryMove(player_t* plr, int dir)
{
    int                 player = plr - players;

    if(!ST_IsInventoryVisible(player))
    {
        ST_Inventory(player, true);
        return false;
    }

    ST_Inventory(player, true); // Reset the inventory auto-hide timer.

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
    int                 player = CONSOLEPLAYER;

    if(argc > 2)
    {
        Con_Printf("Usage: %s (player)\n", argv[0]);
        Con_Printf("If player is not specified, will default to CONSOLEPLAYER.\n");
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
