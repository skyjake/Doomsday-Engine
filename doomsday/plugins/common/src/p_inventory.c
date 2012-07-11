/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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

#if defined(__JHERETIC__) || defined(__JHEXEN__) || defined(__JDOOM64__)

// HEADER FILES ------------------------------------------------------------

#include <assert.h>
#include <string.h>

#if __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#endif

#include "p_player.h"
#include "d_net.h"
#include "p_inventory.h"
#include "hu_inventory.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct inventoryitem_s {
    int             useCount;
    struct inventoryitem_s* next;
} inventoryitem_t;

typedef struct {
    inventoryitem_t* items[NUM_INVENTORYITEM_TYPES-1];
    inventoryitemtype_t readyItem;
} playerinventory_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int didUseItem = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const def_invitem_t itemDefs[NUM_INVENTORYITEM_TYPES-1] = {
#if __JHERETIC__
    {IIF_USE_PANIC, "TXT_ARTIINVULNERABILITY", "A_Invulnerability", "artiuse", "ARTIINVU", CTL_INVULNERABILITY},
    {IIF_USE_PANIC, "TXT_ARTIINVISIBILITY", "A_Invisibility", "artiuse", "ARTIINVS", CTL_INVISIBILITY},
    {IIF_USE_PANIC, "TXT_ARTIHEALTH", "A_Health", "artiuse", "ARTIPTN2", CTL_HEALTH},
    {IIF_USE_PANIC, "TXT_ARTISUPERHEALTH", "A_SuperHealth", "artiuse", "ARTISPHL", CTL_SUPER_HEALTH},
    {IIF_USE_PANIC, "TXT_ARTITOMEOFPOWER", "A_TombOfPower", "artiuse", "ARTIPWBK", CTL_TOME_OF_POWER},
    {IIF_USE_PANIC, "TXT_ARTITORCH", "A_Torch", "artiuse", "ARTITRCH", CTL_TORCH},
    {IIF_USE_PANIC, "TXT_ARTIFIREBOMB", "A_FireBomb", "artiuse", "ARTIFBMB", CTL_FIREBOMB},
    {IIF_USE_PANIC, "TXT_ARTIEGG", "A_Egg", "artiuse", "ARTIEGGC", CTL_EGG},
    {IIF_USE_PANIC, "TXT_ARTIFLY", "A_Wings", "artiuse", "ARTISOAR", CTL_FLY},
    {IIF_USE_PANIC, "TXT_ARTITELEPORT", "A_Teleport", "artiuse", "ARTIATLP", CTL_TELEPORT},
#elif __JHEXEN__
    {IIF_USE_PANIC, "TXT_ARTIINVULNERABILITY", "A_Invulnerability", "ARTIFACT_USE", "ARTIINVU", CTL_INVULNERABILITY},
    {IIF_USE_PANIC, "TXT_ARTIHEALTH", "A_Health", "ARTIFACT_USE", "ARTIPTN2", CTL_HEALTH},
    {IIF_USE_PANIC, "TXT_ARTISUPERHEALTH", "A_SuperHealth", "ARTIFACT_USE", "ARTISPHL", CTL_MYSTIC_URN},
    {IIF_USE_PANIC, "TXT_ARTIHEALINGRADIUS", "A_HealRadius", "ARTIFACT_USE", "ARTIHRAD", -1},
    {IIF_USE_PANIC, "TXT_ARTISUMMON", "A_SummonTarget", "ARTIFACT_USE", "ARTISUMN", CTL_DARK_SERVANT},
    {IIF_USE_PANIC, "TXT_ARTITORCH", "A_Torch", "ARTIFACT_USE", "ARTITRCH", CTL_TORCH},
    {IIF_USE_PANIC, "TXT_ARTIEGG", "A_Egg", "ARTIFACT_USE", "ARTIPORK", CTL_EGG},
    {IIF_USE_PANIC, "TXT_ARTIFLY", "A_Wings", "ARTIFACT_USE", "ARTISOAR", CTL_FLY},
    {IIF_USE_PANIC, "TXT_ARTIBLASTRADIUS", "A_BlastRadius", "ARTIFACT_USE", "ARTIBLST", CTL_BLAST_RADIUS},
    {IIF_USE_PANIC, "TXT_ARTIPOISONBAG", "A_PoisonBag", "ARTIFACT_USE", "ARTIPSBG", CTL_POISONBAG},
    {IIF_USE_PANIC, "TXT_ARTITELEPORTOTHER", "A_TeleportOther", "ARTIFACT_USE", "ARTITELO", CTL_TELEPORT_OTHER},
    {IIF_USE_PANIC, "TXT_ARTISPEED", "A_Speed", "ARTIFACT_USE", "ARTISPED", CTL_SPEED_BOOTS},
    {IIF_USE_PANIC, "TXT_ARTIBOOSTMANA", "A_BoostMana", "ARTIFACT_USE", "ARTIBMAN", CTL_KRATER},
    {IIF_USE_PANIC, "TXT_ARTIBOOSTARMOR", "A_BoostArmor", "ARTIFACT_USE", "ARTIBRAC", -1},
    {IIF_USE_PANIC, "TXT_ARTITELEPORT", "A_Teleport", "ARTIFACT_USE", "ARTIATLP", CTL_TELEPORT},
    {0, "TXT_ARTIPUZZSKULL", "A_PuzzSkull", "PUZZLE_SUCCESS", "ARTISKLL", -1},
    {0, "TXT_ARTIPUZZGEMBIG", "A_PuzzGemBig", "PUZZLE_SUCCESS", "ARTIBGEM", -1},
    {0, "TXT_ARTIPUZZGEMRED", "A_PuzzGemRed", "PUZZLE_SUCCESS", "ARTIGEMR", -1},
    {0, "TXT_ARTIPUZZGEMGREEN1", "A_PuzzGemGreen1", "PUZZLE_SUCCESS", "ARTIGEMG", -1},
    {0, "TXT_ARTIPUZZGEMGREEN2", "A_PuzzGemGreen2", "PUZZLE_SUCCESS", "ARTIGMG2", -1},
    {0, "TXT_ARTIPUZZGEMBLUE1", "A_PuzzGemBlue1", "PUZZLE_SUCCESS", "ARTIGEMB", -1},
    {0, "TXT_ARTIPUZZGEMBLUE2", "A_PuzzGemBlue2", "PUZZLE_SUCCESS", "ARTIGMB2", -1},
    {0, "TXT_ARTIPUZZBOOK1", "A_PuzzBook1", "PUZZLE_SUCCESS", "ARTIBOK1", -1},
    {0, "TXT_ARTIPUZZBOOK2", "A_PuzzBook2", "PUZZLE_SUCCESS", "ARTIBOK2", -1},
    {0, "TXT_ARTIPUZZSKULL2", "A_PuzzSkull2", "PUZZLE_SUCCESS", "ARTISKL2", -1},
    {0, "TXT_ARTIPUZZFWEAPON", "A_PuzzFWeapon", "PUZZLE_SUCCESS", "ARTIFWEP", -1},
    {0, "TXT_ARTIPUZZCWEAPON", "A_PuzzCWeapon", "PUZZLE_SUCCESS", "ARTICWEP", -1},
    {0, "TXT_ARTIPUZZMWEAPON", "A_PuzzMWeapon", "PUZZLE_SUCCESS", "ARTIMWEP", -1},
    {0, "TXT_ARTIPUZZGEAR1", "A_PuzzGear1", "PUZZLE_SUCCESS", "ARTIGEAR", -1},
    {0, "TXT_ARTIPUZZGEAR2", "A_PuzzGear2", "PUZZLE_SUCCESS", "ARTIGER2", -1},
    {0, "TXT_ARTIPUZZGEAR3", "A_PuzzGear3", "PUZZLE_SUCCESS", "ARTIGER3", -1},
    {0, "TXT_ARTIPUZZGEAR4", "A_PuzzGear4", "PUZZLE_SUCCESS", "ARTIGER4", -1}
#elif __JDOOM64__
    {IIF_READY_ALWAYS, "DEMONKEY1", "", "", "", -1},
    {IIF_READY_ALWAYS, "DEMONKEY2", "", "", "", -1},
    {IIF_READY_ALWAYS, "DEMONKEY3", "", "", "", -1}
#endif
};

static invitem_t invItems[NUM_INVENTORYITEM_TYPES - 1];

static playerinventory_t inventories[MAXPLAYERS];

// CODE --------------------------------------------------------------------

static __inline const def_invitem_t* itemDefForType(inventoryitemtype_t type)
{
    return &itemDefs[type - 1];
}

static inventoryitem_t* allocItem(void)
{
    return malloc(sizeof(inventoryitem_t));
}

static void freeItem(inventoryitem_t* item)
{
    free(item);
}

static __inline unsigned int countItems2(const playerinventory_t* inv,
                                         inventoryitemtype_t type)
{
    unsigned int        count;
    const inventoryitem_t* item;

    count = 0;
    for(item = inv->items[type-1]; item; item = item->next)
        count++;

    return count;
}

static unsigned int countItems(const playerinventory_t* inv,
                               inventoryitemtype_t type)
{
    if(type == IIT_NONE)
    {
        inventoryitemtype_t i;
        unsigned int        count;

        count = 0;
        for(i = IIT_FIRST; i < NUM_INVENTORYITEM_TYPES; ++i)
            count += countItems2(inv, i);

        return count;
    }

    return countItems2(inv, type);
}

static boolean useItem(playerinventory_t* inv, inventoryitemtype_t type,
                       boolean panic)
{
    int                 plrnum;
    const invitem_t*    item;

    if(!countItems(inv, type))
        return false; // That was a non-starter.

    plrnum = inv - inventories;
    item = &invItems[type-1];

    // Is this usable?
    if(!item->action)
        return false; // No.

    // How about when panicked?
    if(panic)
    {
        const def_invitem_t* def = itemDefForType(type);

        if(!(def->flags & IIF_USE_PANIC))
            return false;
    }

    /**
     * \kludge
     * Action ptrs do not currently support return values. For now, rather
     * than rewrite each invitem use routine, use a global var to get the
     * result. Ugly.
     */
    didUseItem = false;
    item->action(players[plrnum].plr->mo);

    return didUseItem;
}

static boolean giveItem(playerinventory_t* inv, inventoryitemtype_t type)
{
    unsigned int        count = countItems(inv, type);
    inventoryitem_t*    item;

#if __JHEXEN__
    // Can't carry more than 1 puzzle item in coop netplay.
    if(count && type >= IIT_FIRSTPUZZITEM && IS_NETGAME && !deathmatch)
        return false;
#endif

    // Carrying the maximum allowed number of these items?
    if(count >= MAXINVITEMCOUNT)
        return false;

    item = allocItem();
    item->useCount = 0;

    item->next = inv->items[type-1];
    inv->items[type-1] = item;

    return true;
}

static int takeItem(playerinventory_t* inv, inventoryitemtype_t type)
{
    inventoryitem_t*    next;

    if(!inv->items[type - 1])
        return false; // Don't have one to take.

    next = inv->items[type - 1]->next;
    freeItem(inv->items[type -1]);
    inv->items[type - 1] = next;

    if(!inv->items[type - 1])
    {   // Took last item of this type.
        if(inv->readyItem == type)
            inv->readyItem = IIT_NONE;
    }

    return true;
}

static int tryTakeItem(playerinventory_t* inv, inventoryitemtype_t type)
{
    if(takeItem(inv, type))
    {   // An item was taken.
        int                 player = inv - inventories;

        players[player].update |= PSF_INVENTORY;

#if __JHERETIC__ || __JHEXEN__
        // Inform the HUD.
        Hu_InventoryMarkDirty(player);

        // Set position markers and set the next readyItem?
        if(inv->readyItem == IIT_NONE)
            Hu_InventoryMove(player, -1, false, true);
#endif

        return true;
    }

    return false;
}

static int tryUseItem(playerinventory_t* inv, inventoryitemtype_t type,
                      int panic)
{
    if(useItem(inv, type, panic))
    {   // Item was used.
        return tryTakeItem(inv, type);
    }

    return false;
}

const def_invitem_t* P_GetInvItemDef(inventoryitemtype_t type)
{
    assert(type >= IIT_FIRST && type < NUM_INVENTORYITEM_TYPES);

    return itemDefForType(type);
}

static acfnptr_t getActionPtr(const char* name)
{
    actionlink_t*       link = actionlinks;

    if(!name || !name[0])
        return NULL;

    for(; link->name; link++)
        if(!strcmp(name, link->name))
            return link->func;

    return NULL;
}

/**
 * Called during (post-game) init and after updating game/engine state.
 */
void P_InitInventory(void)
{
    int i;

    memset(invItems, 0, sizeof(invItems));
    for(i = 0; i < NUM_INVENTORYITEM_TYPES - 1; ++i)
    {
        inventoryitemtype_t type = IIT_FIRST + i;
        const def_invitem_t* def = P_GetInvItemDef(type);
        invitem_t* data = &invItems[i];

        data->type = type;
        data->niceName = Def_Get(DD_DEF_TEXT, (char*) def->niceName, NULL);
        data->action = getActionPtr(def->action);
        data->useSnd = Def_Get(DD_DEF_SOUND, (char*) def->useSnd, NULL);
        data->patchId = R_DeclarePatch(def->patch);
    }

    memset(inventories, 0, sizeof(inventories));
}

/**
 * Called once, during shutdown.
 */
void P_ShutdownInventory(void)
{
    int                 i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        P_InventoryEmpty(i);
    }
}

const invitem_t* P_GetInvItem(int id)
{
    if(id < 0 || id >= NUM_INVENTORYITEM_TYPES - 1)
        return NULL;

    return &invItems[id];
}

/**
 * Should be called only outside of normal play (e.g., when starting a new
 * game or similar).
 *
 * @param player        Player to empty the inventory of.
 */
void P_InventoryEmpty(int player)
{
    inventoryitemtype_t i;
    playerinventory_t*  inv;

    if(player < 0 || player >= MAXPLAYERS)
        return;
    inv = &inventories[player];

    for(i = 0; i < NUM_INVENTORYITEM_TYPES - 1; ++i)
    {
        if(inv->items[i])
        {
            inventoryitem_t*    item, *n;

            item = inv->items[i];
            do
            {
                n = item->next;
                freeItem(item);
                item = n;
            } while(item);
        }
    }
    memset(inv->items, 0, sizeof(inv->items[0]) * (NUM_INVENTORYITEM_TYPES-1));

    inv->readyItem = IIT_NONE;
}

/**
 * Retrieve the number of items possessed by the player.
 *
 * @param player        Player to check the inventory of.
 * @param type          Type requirement, limits the count to the
 *                      specified type else @c IIT_NONE to count all.
 * @return              Result.
 */
unsigned int P_InventoryCount(int player, inventoryitemtype_t type)
{
    if(player < 0 || player >= MAXPLAYERS)
        return 0;

    if(!(type == IIT_NONE ||
        (type >= IIT_FIRST && type < NUM_INVENTORYITEM_TYPES)))
        return 0;

    return countItems(&inventories[player], type);
}

/**
 * Attempt to ready an item (or unready a currently readied item).
 *
 * @param player        Player to change the ready item of.
 * @param type          Item type to be readied.
 *
 * @return              Non-zero iff successful.
 */
int P_InventorySetReadyItem(int player, inventoryitemtype_t type)
{
    playerinventory_t*  inv;

    if(player < 0 || player >= MAXPLAYERS)
        return 0;

    if(type < IIT_NONE || type >= NUM_INVENTORYITEM_TYPES)
        return 0;
    inv = &inventories[player];

    if(type == IIT_NONE || countItems(inv, type))
    {   // A valid ready request.
        boolean             mustEquip = true;

        if(type != IIT_NONE)
        {
            const def_invitem_t* def = P_GetInvItemDef(type);
            mustEquip = ((def->flags & IIF_READY_ALWAYS)? false : true);
        }

        if(mustEquip && inv->readyItem != type)
        {   // Make it so.
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

/**
 * @return              The currently readied item, else @c IIT_NONE.
 */
inventoryitemtype_t P_InventoryReadyItem(int player)
{
    if(player < 0 || player >= MAXPLAYERS)
        return IIT_NONE;

    return inventories[player].readyItem;
}

/**
 * Give one item of the specified type to the player.
 *
 * @param player        Player to give the item to.
 * @param type          Type of item to give.
 * @param silent        Non-zero = don't alert the player.
 *
 * @return              Non-zero iff an item of the specified type was
 *                      successfully given to the player.
 */
int P_InventoryGive(int player, inventoryitemtype_t type, int silent)
{
    unsigned int        oldNumItems;
    playerinventory_t*  inv;

    if(player < 0 || player >= MAXPLAYERS)
        return 0;

    if(!(type >= IIT_FIRST && type < NUM_INVENTORYITEM_TYPES))
        return 0;
    inv = &inventories[player];

    oldNumItems = countItems(inv, IIT_NONE);

    if(giveItem(inv, type))
    {   // Item was given.
        players[player].update |= PSF_INVENTORY;

#if __JHERETIC__ || __JHEXEN__
        // Inform the HUD.
        Hu_InventoryMarkDirty(player);
#endif

        if(oldNumItems == 0)
        {   // This is the first item the player has been given; ready it.
            const def_invitem_t* def = P_GetInvItemDef(type);

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
            ST_HUDUnHide(player, HUE_ON_PICKUP_INVITEM);
#endif

        return 1;
    }

    return 0;
}

/**
 * Take one of the specified items from the player (if owned).
 *
 * @param player        Player to take the item from.
 * @param type          Type of item being taken.
 * @param silent        Non-zero = don't alert the player.
 *
 * @return              Non-zero iff an item of the specified type was
 *                      successfully taken from the player.
 */
int P_InventoryTake(int player, inventoryitemtype_t type,
                    int silent)
{
    playerinventory_t*  inv;

    if(player < 0 || player >= MAXPLAYERS)
        return 0;

    if(!(type >= IIT_FIRST && type < NUM_INVENTORYITEM_TYPES))
        return 0;
    inv = &inventories[player];

    return tryTakeItem(inv, type);
}

/**
 * Attempt to use an item of the specified type.
 *
 * @param player        Player using the item.
 * @param type          The type of item being used ELSE
 *                      IIT_NONE = Ignored.
 *                      NUM_INVENTORYITEM_TYPES = Panic. Use one of everything!!
 * @param silent        Non-zero - don't alert the player.
 *
 * @return              Non-zero iff one (or more) item was successfully
 *                      used by the player.
 */
int P_InventoryUse(int player, inventoryitemtype_t type, int silent)
{
    inventoryitemtype_t lastUsed = IIT_NONE;
    playerinventory_t*  inv;

    if(player < 0 || player >= MAXPLAYERS)
        return false;

    if(!(type >= IIT_FIRST || type <= NUM_INVENTORYITEM_TYPES))
        return false;
    inv = &inventories[player];

#ifdef _DEBUG
    Con_Message("P_InventoryUse: Player %i using item %i\n", player, type);
#endif

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
                lastUsed = type;
        }
        else
        {   // Panic! Use one of each item that is usable when panicked.
            inventoryitemtype_t i;

            for(i = IIT_FIRST; i < NUM_INVENTORYITEM_TYPES; ++i)
                if(tryUseItem(inv, i, true))
                    lastUsed = i;
        }

        if(lastUsed == IIT_NONE)
        {   // Failed to use an item.
            // Set current to the next available?
#if __JHERETIC__ || __JHEXEN__
            if(type != NUM_INVENTORYITEM_TYPES && cfg.inventoryUseNext)
            {
# if __JHEXEN__
                if(lastUsed < IIT_FIRSTPUZZITEM)
# endif
                    Hu_InventoryMove(player, -1, false, true);
            }
#endif
            return false;
        }
    }

    if(!silent && lastUsed != IIT_NONE)
    {
        invitem_t* item = &invItems[lastUsed - 1];
        S_ConsoleSound(item->useSnd, NULL, player);
#if __JHERETIC__ || __JHEXEN__
        ST_FlashCurrentItem(player);
#endif
    }

    return true;
}

#endif
