/** @file p_inventory.cpp  Common code for player inventory.
 *
 * @authors Copyright © 2009-2015 Daniel Swanson <danij@dengine.net>
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

#if defined(__JHERETIC__) || defined(__JHEXEN__) || defined(__JDOOM64__)

#include "common.h"
#include "p_inventory.h"

#include <cstring>
#include <de/legacy/memory.h>
#include "d_net.h"
#include "d_netcl.h"
#include "g_common.h"
#include "g_defs.h"
#include "gamesession.h"
#include "hu_inventory.h"
#include "player.h"

struct inventoryitem_t
{
    int useCount;
    inventoryitem_t *next;
};

struct playerinventory_t
{
    inventoryitem_t *items[NUM_INVENTORYITEM_TYPES-1];
    inventoryitemtype_t readyItem;
};

int didUseItem = false;

static def_invitem_t const itemDefs[NUM_INVENTORYITEM_TYPES-1] = {
#if __JHERETIC__
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIINVULNERABILITY", "A_Invulnerability", "artiuse", "ARTIINVU", CTL_INVULNERABILITY},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIINVISIBILITY", "A_Invisibility", "artiuse", "ARTIINVS", CTL_INVISIBILITY},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIHEALTH", "A_Health", "artiuse", "ARTIPTN2", CTL_HEALTH},
    {GM_NOT_SHAREWARE,  IIF_USE_PANIC, "TXT_ARTISUPERHEALTH", "A_SuperHealth", "artiuse", "ARTISPHL", CTL_SUPER_HEALTH},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTITOMEOFPOWER", "A_TombOfPower", "artiuse", "ARTIPWBK", CTL_TOME_OF_POWER},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTITORCH", "A_Torch", "artiuse", "ARTITRCH", CTL_TORCH},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIFIREBOMB", "A_FireBomb", "artiuse", "ARTIFBMB", CTL_FIREBOMB},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIEGG", "A_Egg", "artiuse", "ARTIEGGC", CTL_EGG},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIFLY", "A_Wings", "artiuse", "ARTISOAR", CTL_FLY},
    {GM_NOT_SHAREWARE,  IIF_USE_PANIC, "TXT_ARTITELEPORT", "A_Teleport", "artiuse", "ARTIATLP", CTL_TELEPORT},
#elif __JHEXEN__
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIINVULNERABILITY", "A_Invulnerability", "ARTIFACT_USE", "ARTIINVU", CTL_INVULNERABILITY},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIHEALTH", "A_Health", "ARTIFACT_USE", "ARTIPTN2", CTL_HEALTH},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTISUPERHEALTH", "A_SuperHealth", "ARTIFACT_USE", "ARTISPHL", CTL_MYSTIC_URN},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIHEALINGRADIUS", "A_HealRadius", "ARTIFACT_USE", "ARTIHRAD", -1},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTISUMMON", "A_SummonTarget", "ARTIFACT_USE", "ARTISUMN", CTL_DARK_SERVANT},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTITORCH", "A_Torch", "ARTIFACT_USE", "ARTITRCH", CTL_TORCH},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIEGG", "A_Egg", "ARTIFACT_USE", "ARTIPORK", CTL_EGG},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIFLY", "A_Wings", "ARTIFACT_USE", "ARTISOAR", CTL_FLY},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIBLASTRADIUS", "A_BlastRadius", "ARTIFACT_USE", "ARTIBLST", CTL_BLAST_RADIUS},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIPOISONBAG", "A_PoisonBag", "ARTIFACT_USE", "ARTIPSBG", CTL_POISONBAG},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTITELEPORTOTHER", "A_TeleportOther", "ARTIFACT_USE", "ARTITELO", CTL_TELEPORT_OTHER},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTISPEED", "A_Speed", "ARTIFACT_USE", "ARTISPED", CTL_SPEED_BOOTS},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIBOOSTMANA", "A_BoostMana", "ARTIFACT_USE", "ARTIBMAN", CTL_KRATER},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTIBOOSTARMOR", "A_BoostArmor", "ARTIFACT_USE", "ARTIBRAC", -1},
    {GM_ANY,            IIF_USE_PANIC, "TXT_ARTITELEPORT", "A_Teleport", "ARTIFACT_USE", "ARTIATLP", CTL_TELEPORT},
    {GM_ANY,            0, "TXT_ARTIPUZZSKULL", "A_PuzzSkull", "PUZZLE_SUCCESS", "ARTISKLL", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZGEMBIG", "A_PuzzGemBig", "PUZZLE_SUCCESS", "ARTIBGEM", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZGEMRED", "A_PuzzGemRed", "PUZZLE_SUCCESS", "ARTIGEMR", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZGEMGREEN1", "A_PuzzGemGreen1", "PUZZLE_SUCCESS", "ARTIGEMG", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZGEMGREEN2", "A_PuzzGemGreen2", "PUZZLE_SUCCESS", "ARTIGMG2", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZGEMBLUE1", "A_PuzzGemBlue1", "PUZZLE_SUCCESS", "ARTIGEMB", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZGEMBLUE2", "A_PuzzGemBlue2", "PUZZLE_SUCCESS", "ARTIGMB2", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZBOOK1", "A_PuzzBook1", "PUZZLE_SUCCESS", "ARTIBOK1", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZBOOK2", "A_PuzzBook2", "PUZZLE_SUCCESS", "ARTIBOK2", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZSKULL2", "A_PuzzSkull2", "PUZZLE_SUCCESS", "ARTISKL2", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZFWEAPON", "A_PuzzFWeapon", "PUZZLE_SUCCESS", "ARTIFWEP", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZCWEAPON", "A_PuzzCWeapon", "PUZZLE_SUCCESS", "ARTICWEP", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZMWEAPON", "A_PuzzMWeapon", "PUZZLE_SUCCESS", "ARTIMWEP", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZGEAR1", "A_PuzzGear1", "PUZZLE_SUCCESS", "ARTIGEAR", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZGEAR2", "A_PuzzGear2", "PUZZLE_SUCCESS", "ARTIGER2", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZGEAR3", "A_PuzzGear3", "PUZZLE_SUCCESS", "ARTIGER3", -1},
    {GM_ANY,            0, "TXT_ARTIPUZZGEAR4", "A_PuzzGear4", "PUZZLE_SUCCESS", "ARTIGER4", -1}
#elif __JDOOM64__
    {GM_ANY,            IIF_READY_ALWAYS, "DEMONKEY1", "", "", "", -1},
    {GM_ANY,            IIF_READY_ALWAYS, "DEMONKEY2", "", "", "", -1},
    {GM_ANY,            IIF_READY_ALWAYS, "DEMONKEY3", "", "", "", -1}
#endif
};

static invitem_t invItems[NUM_INVENTORYITEM_TYPES - 1];

static playerinventory_t inventories[MAXPLAYERS];

static inline const def_invitem_t *itemDefForType(inventoryitemtype_t type)
{
    return &itemDefs[type - 1];
}

static inventoryitem_t *allocItem()
{
    inventoryitem_t *item = (inventoryitem_t *)M_Malloc(sizeof(*item));
    return item;
}

static void freeItem(inventoryitem_t *item)
{
    M_Free(item);
}

static inline uint countItems2(const playerinventory_t *inv, inventoryitemtype_t type)
{
    uint count = 0;
    for(const inventoryitem_t *item = inv->items[type - 1]; item; item = item->next)
    {
        count++;
    }
    return count;
}

static uint countItems(const playerinventory_t *inv, inventoryitemtype_t type)
{
    if(type == IIT_NONE)
    {
        uint count = 0;
        for(int i = IIT_FIRST; i < NUM_INVENTORYITEM_TYPES; ++i)
        {
            count += countItems2(inv, inventoryitemtype_t(i));
        }
        return count;
    }
    return countItems2(inv, type);
}

static dd_bool useItem(playerinventory_t *inv, inventoryitemtype_t type, dd_bool panic)
{
    if(!countItems(inv, type))
        return false; // That was a non-starter.

    int plrnum = inv - inventories;
    const invitem_t *item = &invItems[type-1];

    // Is this usable?
    if(!item->action)
    {
        return false; // No.
    }

    // How about when panicked?
    if(panic)
    {
        const def_invitem_t *def = itemDefForType(type);
        if(!(def->flags & IIF_USE_PANIC))
        {
            return false;
        }
    }

    /**
     * @attention Kludge:
     * Action ptrs do not currently support return values. For now, rather
     * than rewrite each use routine, use a global var to get the result.
     * Ugly.
     */
    didUseItem = false;
    de::function_cast<void (*)(mobj_t *)>(item->action)(players[plrnum].plr->mo);

    return didUseItem;
}

static dd_bool giveItem(playerinventory_t *inv, inventoryitemtype_t type)
{
    uint count = countItems(inv, type);

    // Do not give items unavailable for the current game mode.
    const def_invitem_t *def = itemDefForType(type);
    if(!(def->gameModeBits & gameModeBits))
        return false;

#if __JHEXEN__
    // Can't carry more than 1 puzzle item in coop netplay.
    if(count && type >= IIT_FIRSTPUZZITEM && IS_NETGAME &&
       !gfw_Rule(deathmatch))
    {
        return false;
    }
#endif

    // Carrying the maximum allowed number of these items?
    if(count >= MAXINVITEMCOUNT)
    {
        return false;
    }

    inventoryitem_t *item = allocItem();
    item->useCount = 0;

    item->next = inv->items[type-1];
    inv->items[type-1] = item;

    return true;
}

static int takeItem(playerinventory_t *inv, inventoryitemtype_t type)
{
    if(!inv->items[type - 1])
    {
        return false; // Don't have one to take.
    }

    inventoryitem_t *next = inv->items[type - 1]->next;
    freeItem(inv->items[type -1]);
    inv->items[type - 1] = next;

    if(!inv->items[type - 1])
    {
        // Took last item of this type.
        if(inv->readyItem == type)
        {
            inv->readyItem = IIT_NONE;
        }
    }

    return true;
}

static int tryTakeItem(playerinventory_t *inv, inventoryitemtype_t type)
{
    if(takeItem(inv, type))
    {
        // An item was taken.
        int player = inv - inventories;

        players[player].update |= PSF_INVENTORY;

#if __JHERETIC__ || __JHEXEN__
        // Inform the HUD.
        Hu_InventoryMarkDirty(player);

        // Set position markers and set the next readyItem?
        if(inv->readyItem == IIT_NONE)
        {
            Hu_InventoryMove(player, -1, false, true);
        }
#endif

        return true;
    }

    return false;
}

static int tryUseItem(playerinventory_t *inv, inventoryitemtype_t type, int panic)
{
    if(useItem(inv, type, panic))
    {
        // Item was used.
        return tryTakeItem(inv, type);
    }

    return false;
}

const def_invitem_t *P_GetInvItemDef(inventoryitemtype_t type)
{
    DE_ASSERT(type >= IIT_FIRST && type < NUM_INVENTORYITEM_TYPES);
    return itemDefForType(type);
}

void P_InitInventory()
{
    de::zap(invItems);

    for(int i = 0; i < NUM_INVENTORYITEM_TYPES - 1; ++i)
    {
        invitem_t *data          = &invItems[i];
        inventoryitemtype_t type = inventoryitemtype_t(IIT_FIRST + i);
        const def_invitem_t *def = P_GetInvItemDef(type);

        // Skip items unavailable for the current game mode.
        if(!(def->gameModeBits & gameModeBits))
            continue;

        data->type     = type;
        data->niceName = textenum_t(Defs().getTextNum(def->niceName));
        Def_Get(DD_DEF_ACTION, def->action, &data->action);
        data->useSnd   = sfxenum_t(Defs().getSoundNum(def->useSnd));
        data->patchId  = R_DeclarePatch(def->patch);
    }

    de::zap(inventories);
}

void P_ShutdownInventory()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        P_InventoryEmpty(i);
    }
}

const invitem_t *P_GetInvItem(int id)
{
    if(id < 0 || id >= NUM_INVENTORYITEM_TYPES - 1)
        return 0;

    return &invItems[id];
}

void P_InventoryEmpty(int player)
{
    if(player < 0 || player >= MAXPLAYERS)
        return;

    playerinventory_t *inv = &inventories[player];

    for(int i = 0; i < NUM_INVENTORYITEM_TYPES - 1; ++i)
    {
        inventoryitem_t *item;
        while((item = inv->items[i]))
        {
            inventoryitem_t *next = item->next;
            freeItem(item);
            inv->items[i] = next;
        }
    }
    de::zap(inv->items);

    inv->readyItem = IIT_NONE;
}

uint P_InventoryCount(int player, inventoryitemtype_t type)
{
    if(player < 0 || player >= MAXPLAYERS)
        return 0;

    if(!(type == IIT_NONE ||
        (type >= IIT_FIRST && type < NUM_INVENTORYITEM_TYPES)))
    {
        return 0;
    }

    return countItems(&inventories[player], type);
}

int P_InventorySetReadyItem(int player, inventoryitemtype_t type)
{
    if(player < 0 || player >= MAXPLAYERS)
        return 0;

    if(type < IIT_NONE || type >= NUM_INVENTORYITEM_TYPES)
        return 0;

    playerinventory_t *inv = &inventories[player];

    if(type == IIT_NONE || countItems(inv, type))
    {
        // A valid ready request.
        dd_bool mustEquip = true;

        if(type != IIT_NONE)
        {
            const def_invitem_t *def = P_GetInvItemDef(type);
            mustEquip = ((def->flags & IIF_READY_ALWAYS)? false : true);
        }

        if(mustEquip && inv->readyItem != type)
        {
            // Make it so.
            inv->readyItem = type;

#if __JHERETIC__ || __JHEXEN__
            // Inform the HUD.
            Hu_InventoryMarkDirty(player);
#endif
        }

        return 1;
    }

    return 0;
}

inventoryitemtype_t P_InventoryReadyItem(int player)
{
    if(player < 0 || player >= MAXPLAYERS)
        return IIT_NONE;

    return inventories[player].readyItem;
}

int P_InventoryGive(int player, inventoryitemtype_t type, int silent)
{
    if(player < 0 || player >= MAXPLAYERS)
        return 0;

    if(!(type >= IIT_FIRST && type < NUM_INVENTORYITEM_TYPES))
        return 0;

    playerinventory_t *inv = &inventories[player];
    uint oldNumItems       = countItems(inv, IIT_NONE);

    if(giveItem(inv, type))
    {
        // Item was given.
        players[player].update |= PSF_INVENTORY;

#if __JHERETIC__ || __JHEXEN__
        // Inform the HUD.
        Hu_InventoryMarkDirty(player);
#endif

        if(oldNumItems == 0)
        {
            // This is the first item the player has been given; ready it.
            const def_invitem_t *def = P_GetInvItemDef(type);

            if(!(def->flags & IIF_READY_ALWAYS))
            {
                inv->readyItem = type;
#if __JHERETIC__ || __JHEXEN__
                Hu_InventorySelect(player, type);
#endif
            }
        }

        // Maybe unhide the HUD?
#if __JHERETIC__ || __JHEXEN__
        if(!silent)
        {
            ST_HUDUnHide(player, HUE_ON_PICKUP_INVITEM);
        }
#else
        DE_UNUSED(silent);
#endif

        return 1;
    }

    return 0;
}

int P_InventoryTake(int player, inventoryitemtype_t type, int /*silent*/)
{
    if(player < 0 || player >= MAXPLAYERS)
        return 0;

    if(!(type >= IIT_FIRST && type < NUM_INVENTORYITEM_TYPES))
        return 0;

    playerinventory_t *inv = &inventories[player];
    return tryTakeItem(inv, type);
}

int P_InventoryUse(int player, inventoryitemtype_t type, int silent)
{
    inventoryitemtype_t lastUsed = IIT_NONE;

    if(player < 0 || player >= MAXPLAYERS)
        return false;

    if(!(type >= IIT_FIRST || type <= NUM_INVENTORYITEM_TYPES))
        return false;

    playerinventory_t *inv = &inventories[player];

    App_Log(DE2_DEV_MAP_VERBOSE, "P_InventoryUse: Player %i using item %i", player, type);

    if(IS_CLIENT)
    {
        if(countItems(inv, type))
        {
            // Clients will send a request to use the item, nothing else.
            NetCl_PlayerActionRequest(&players[player], GPA_USE_FROM_INVENTORY, type);
            lastUsed = type;
        }
    }
    else
    {
        if(type != NUM_INVENTORYITEM_TYPES)
        {
            if(tryUseItem(inv, type, false))
            {
                lastUsed = type;
            }
        }
        else
        {
            // Panic! Use one of each item that is usable when panicked.
            for(int i = IIT_FIRST; i < NUM_INVENTORYITEM_TYPES; ++i)
            {
                if(tryUseItem(inv, inventoryitemtype_t(i), true))
                {
                    lastUsed = inventoryitemtype_t(i);
                }
            }
        }

        if(lastUsed == IIT_NONE)
        {
            // Failed to use an item.
            // Set current to the next available?
#if __JHERETIC__ || __JHEXEN__
            if(type != NUM_INVENTORYITEM_TYPES && cfg.inventoryUseNext)
            {
# if __JHEXEN__
                // Puzzle items do not cause the current item to change.
                if(type < IIT_FIRSTPUZZITEM)
# endif
                {
                    Hu_InventoryMove(player, -1, true /* allow wrap */, true);
                }
            }
#endif
            return false;
        }
    }

    if(!silent && lastUsed != IIT_NONE)
    {
        invitem_t *item = &invItems[lastUsed - 1];
        S_ConsoleSound(item->useSnd, NULL, player);
#if __JHERETIC__ || __JHEXEN__
        ST_FlashCurrentItem(player);
#endif
    }

    return true;
}

#endif
