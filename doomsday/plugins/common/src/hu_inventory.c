/**\file hu_inventory.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * Heads-up display(s) for the player inventory.
 */

#if defined(__JHERETIC__) || defined(__JHEXEN__)

// HEADER FILES ------------------------------------------------------------

#include <assert.h>
#include <math.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "p_tick.h"
#include "hu_stuff.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

// How many inventory slots are visible in the fixed-size inventory.
#define NUMVISINVSLOTS      (7)

#define ST_INVENTORYHEIGHT  (30)
#define ST_INVSLOTWIDTH     (31)

// Inventory item counts (relative to each slot).
#define ST_INVCOUNTDIGITS   (2)

#if __JHERETIC__
#define ST_INVICONOFFY      (0)

#define ST_INVCOUNTOFFX     (27)
#define ST_INVCOUNTOFFY     (22)

#define ST_INVSLOTOFFX      (1)
#define ST_INVSELECTOFFY    (ST_INVENTORYHEIGHT)
#else
#define ST_INVICONOFFY      (-1)

#define ST_INVCOUNTOFFX     (28)
#define ST_INVCOUNTOFFY     (22)

#define ST_INVSLOTOFFX      (1)
#define ST_INVSELECTOFFY    (1)
#endif

// HUD Inventory Flags:
#define HIF_VISIBLE         0x1
#define HIF_IS_DIRTY        0x8

// TYPES -------------------------------------------------------------------

typedef struct {
    byte            flags; // HIF_* flags
    int             hideTics;
    uint            numOwnedItemTypes;

    uint            slots[NUM_INVENTORYITEM_TYPES - 1];
    uint            numUsedSlots;
    uint            selected;

    uint            varCursorPos; // Variable-range, fullscreen inv cursor.
    uint            fixedCursorPos; // Fixed-range, statusbar inv cursor.
} hud_inventory_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void        ST_ResizeInventory(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void inventoryIndexes(const player_t* plr, const hud_inventory_t* hud,
                             uint maxVisSlots, int origCursor,
                             uint* firstVisible, uint* cursorPos,
                             uint* startSlot, uint* endSlot);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hud_inventory_t hudInventories[MAXPLAYERS];

cvartemplate_t hudInvCVars[] = {
    {"hud-inventory-timer", 0, CVT_FLOAT, &cfg.inventoryTimer, 0, 30},
    {"hud-inventory-slot-showempty", 0, CVT_BYTE, &cfg.inventorySlotShowEmpty, 0, 1},
    {"hud-inventory-slot-max", CVF_NO_MAX, CVT_INT, &cfg.inventorySlotMaxVis, 0, 0, ST_ResizeInventory},
    {NULL}
};

// CODE -------------------------------------------------------------------

/**
 * Register CVARs and CCmds for the HUD inventory.
 */
void Hu_InventoryRegister(void)
{
    int i;
    for(i = 0; hudInvCVars[i].path; ++i)
        Con_AddVariable(hudInvCVars + i);
}

/**
 * Rebuild the inventory item type table.
 * These indices can be used to associate slots in an inventory browser
 * to the items held within.
 */
static void rebuildInventory(hud_inventory_t* inv)
{
    int                 i, plrNum = inv - hudInventories;
    uint                numOwnedItemTypes;
    inventoryitemtype_t selectedType = P_GetInvItem(
        inv->slots[inv->selected])->type;

    inv->selected = 0;

    numOwnedItemTypes = 0;
    for(i = 0; i < NUM_INVENTORYITEM_TYPES - 1; ++i)
        if(P_InventoryCount(plrNum, IIT_FIRST + i) > 0)
            numOwnedItemTypes++;

    // Always 1:1
    inv->numOwnedItemTypes = inv->numUsedSlots =
        numOwnedItemTypes;

    memset(inv->slots, 0,
           sizeof(inv->slots[0]) * (NUM_INVENTORYITEM_TYPES - 1));
    if(inv->numUsedSlots)
    {
        uint                idx = 0;

        for(i = 0; i < NUM_INVENTORYITEM_TYPES - 1; ++i)
        {
            if(P_InventoryCount(plrNum, IIT_FIRST + i))
            {
                inv->slots[idx] = i;
                if(P_GetInvItem(i)->type == selectedType)
                    inv->selected = idx;

                if(++idx >= inv->numUsedSlots)
                    break;
            }
        }
    }

    if(inv->flags & HIF_IS_DIRTY)
        inv->flags &= ~HIF_IS_DIRTY;
}

static void inventoryIndexes(const player_t* plr, const hud_inventory_t* inv,
                             uint maxVisSlots, int origCursor,
                             uint* firstVisible, uint* cursorPos,
                             uint* fromSlot, uint* toSlot)
{
    int                 cursor, first, from, to;

    if(!firstVisible && !cursorPos && !fromSlot && !toSlot)
        return;

    if(cfg.inventorySelectMode)
    {   // Scroll.
        int                 last;

        cursor = maxVisSlots / 2;

        if(cfg.inventoryWrap)
        {
            first = inv->selected - cursor;
            if(first < 0)
                first += inv->numUsedSlots;

            from = 0;
            to = maxVisSlots;
        }
        else
        {
            first = inv->selected - cursor;
            if(first < 0)
                first = 0;

            from = cursor - inv->selected;
            if(from < 0)
                from = 0;

            last = inv->selected + cursor + (maxVisSlots % 2? 1 : 0);
            if(last - 1 < (signed) inv->numUsedSlots)
                to = maxVisSlots;
            else
                to = maxVisSlots - (last - inv->numUsedSlots);
        }
    }
    else
    {   // Cursor.
        cursor = origCursor;

        if(inv->numUsedSlots < maxVisSlots)
        {
            from = (maxVisSlots - inv->numUsedSlots) / 2;
            to = from + inv->numUsedSlots;
            cursor += from;
        }
        else
        {
            if(cfg.inventoryWrap)
            {
                from = 0;
                to = maxVisSlots;
            }
            else
            {
                from = MAX_OF(0, cursor - (signed)inv->selected);
                to = maxVisSlots;
            }
        }

        first = inv->selected - origCursor;
        if(cfg.inventoryWrap)
        {
            if(first < 0)
                first += inv->numUsedSlots;
        }
        else
        {
            if(inv->numUsedSlots < maxVisSlots ||
               first + maxVisSlots > inv->numUsedSlots)
            {
                int             shift = inv->numUsedSlots -
                    (first + maxVisSlots);

                first += shift;
                if(first < 0)
                {
                    first = 0;
                    cursor = from + inv->selected;
                }
                else
                    cursor -= shift;
            }
        }

        if(first < 0)
            first = 0;
    }

    if(from < 0)
        from = 0;

    if(firstVisible)
        *firstVisible = (unsigned) first;
    if(cursorPos)
        *cursorPos = (unsigned) cursor;
    if(fromSlot)
        *fromSlot = (unsigned) from;
    if(toSlot)
        *toSlot = (unsigned) to;
}

void Hu_InventoryDraw(int player, int x, int y, float textAlpha, float iconAlpha)
{
#define BORDER                  (1)
#if __JHERETIC__
# define TRACKING               (2)
#else
# define TRACKING               (0)
#endif

    const hud_inventory_t* inv;
    uint i, from, to, idx, slot, first, selected, numVisSlots, maxVisSlots, startSlot, endSlot;
    float invScale, lightDelta;
    player_t* plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    inv = &hudInventories[player];
    plr = &players[player];

    if(cfg.inventorySlotMaxVis)
        maxVisSlots = cfg.inventorySlotMaxVis;
    else
        maxVisSlots = NUM_INVENTORYITEM_TYPES - 1;

    inventoryIndexes(plr, inv, maxVisSlots, inv->varCursorPos, &first, &selected, &startSlot, &endSlot);

    numVisSlots = maxVisSlots;

    {
    float availWidth = SCREENWIDTH - 50 * 2, width = (numVisSlots * ST_INVSLOTWIDTH);

    if(width > availWidth)
        invScale = availWidth / width;
    else
        invScale = 1;
    }

    if(maxVisSlots % 2)
        lightDelta = 1.f / maxVisSlots;
    else
        lightDelta = 1.f / (maxVisSlots - 1);
    lightDelta *= 2;

    idx = first;
    slot = (cfg.inventorySlotShowEmpty? 0 : startSlot);
    from = (cfg.inventorySlotShowEmpty? 0 : startSlot);

    if(cfg.inventorySlotShowEmpty)
    {
        to = maxVisSlots;
    }
    else
    {
        if(startSlot > 0)
            to = maxVisSlots;
        else
            to = endSlot - startSlot;
        if(inv->numUsedSlots - 1 < endSlot - startSlot)
            to = from + inv->numUsedSlots;
    }

Draw_BeginZoom(invScale, x, y + ST_INVENTORYHEIGHT);

    x -= (numVisSlots * ST_INVSLOTWIDTH) / 2.f;

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_SMALLIN));
    FR_SetTracking(TRACKING);

    for(i = from; i < to; ++i)
    {
        float light, a;

        if(i < maxVisSlots / 2)
            light = (i + 1) * lightDelta;
        else
            light = (maxVisSlots - i) * lightDelta;
        a = i == selected? .5f : light / 2;

        DGL_Color4f(light, light, light, a * iconAlpha);
        GL_DrawPatchXY(pInvItemBox, x + slot * ST_INVSLOTWIDTH + (slot > 1? (slot-1) * ST_INVSLOTOFFX : 0), y);

        if(i >= startSlot && i < endSlot)
        {
            const invitem_t* item = P_GetInvItem(inv->slots[idx]);
            uint count;

            if((count = P_InventoryCount(player, item->type)))
            {
#if __JHEXEN__
                int posX = x + slot * ST_INVSLOTWIDTH + (slot > 1? (slot-1) * ST_INVSLOTOFFX : 0) - 1;
#else
                int posX = x + slot * ST_INVSLOTWIDTH + (slot > 1? (slot-1) * ST_INVSLOTOFFX : 0);
#endif
                DGL_Color4f(1, 1, 1, slot == selected? iconAlpha : iconAlpha / 2);
                GL_DrawPatchXY(item->patchId, posX, y + ST_INVICONOFFY);

                if(count > 1)
                {
                    char buf[20];
                    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], slot == selected? textAlpha : textAlpha / 2);
                    dd_snprintf(buf, 20, "%i", count);
                    FR_DrawTextXY3(buf, posX + ST_INVCOUNTOFFX, y + ST_INVCOUNTOFFY, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
                }
            }

            if(++idx > inv->numOwnedItemTypes - 1)
                idx = 0;
        }
        slot++;
    }

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatchXY(pInvSelectBox, x + selected * ST_INVSLOTWIDTH + (selected > 1? (selected-1) * ST_INVSLOTOFFX : 0), y + ST_INVSELECTOFFY - BORDER);

    if(inv->numUsedSlots > maxVisSlots)
    {
#define ARROW_RELXOFF          2
#define ARROW_YOFFSET          9

        if(cfg.inventoryWrap || first != 0)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            GL_DrawPatchXY3(pInvPageLeft[!(mapTime & 4)? 1 : 0], x - ARROW_RELXOFF, y + ARROW_YOFFSET, ALIGN_TOPRIGHT, 0);
        }

        if(cfg.inventoryWrap || inv->numUsedSlots - first > numVisSlots)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            GL_DrawPatchXY(pInvPageRight[!(mapTime & 4)? 1 : 0], x + numVisSlots * ST_INVSLOTWIDTH + (numVisSlots > 1? (numVisSlots-1) * ST_INVSLOTOFFX : 0) + ARROW_RELXOFF - 2, y + ARROW_YOFFSET);
        }

#undef ARROW_XOFFSET
#undef ARROW_YOFFSET
    }

    DGL_Disable(DGL_TEXTURE_2D);

Draw_EndZoom();

#undef TRACKING
#undef BORDER
}

void Hu_InventoryDraw2(int player, int x, int y, float alpha)
{
#define BORDER                  (1)
#if __JHERETIC__
# define TRACKING               (2)
#else
# define TRACKING               (0)
#endif

    uint i, idx, slot, from, to, first, cursor, startSlot, endSlot;
    const hud_inventory_t* inv;
    player_t* plr;

    if(alpha <= 0)
        return;

    if(player < 0 || player >= MAXPLAYERS)
        return;
    inv = &hudInventories[player];
    plr = &players[player];

    inventoryIndexes(plr, inv, NUMVISINVSLOTS, inv->fixedCursorPos, &first, &cursor, &startSlot, &endSlot);

    idx = first;
    from = startSlot;
    slot = startSlot;
    if(startSlot > 0)
        to = NUMVISINVSLOTS;
    else
        to = endSlot - startSlot;
    if(inv->numUsedSlots - 1 < endSlot - startSlot)
        to = from + inv->numUsedSlots;

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_SMALLIN));
    FR_SetTracking(TRACKING);

    for(i = from; i < to; ++i)
    {
        if(i >= startSlot && i < endSlot)
        {
            const invitem_t* item = P_GetInvItem(inv->slots[idx]);
            uint count;

            if((count = P_InventoryCount(player, item->type)))
            {
                DGL_Color4f(1, 1, 1, alpha);
                GL_DrawPatchXY(item->patchId, x + slot * ST_INVSLOTWIDTH, y + ST_INVICONOFFY);

                if(count > 1)
                {
                    char buf[20];
                    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], alpha);
                    dd_snprintf(buf, 20, "%i", count);
                    FR_DrawTextXY3(buf, x + slot * ST_INVSLOTWIDTH + ST_INVCOUNTOFFX, y + ST_INVCOUNTOFFY, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
                }
            }

            if(++idx > inv->numOwnedItemTypes - 1)
                idx = 0;
        }

        slot++;
    }

    DGL_Color4f(1, 1, 1, alpha);
    GL_DrawPatchXY(pInvSelectBox, x + cursor * ST_INVSLOTWIDTH, y + ST_INVSELECTOFFY - BORDER);

    if(inv->numUsedSlots > NUMVISINVSLOTS)
    {
        // Draw more left indicator.
        if(cfg.inventoryWrap || first != 0)
        {
            DGL_Color4f(1, 1, 1, alpha);
            GL_DrawPatchXY(pInvPageLeft[!(mapTime & 4)? 1 : 0], x - 12, y - 1);
        }

        // Draw more right indicator.
        if(cfg.inventoryWrap || inv->numUsedSlots - first > NUMVISINVSLOTS)
        {
            DGL_Color4f(1, 1, 1, alpha);
            GL_DrawPatchXY(pInvPageRight[!(mapTime & 4)? 1 : 0], x + NUMVISINVSLOTS * ST_INVSLOTWIDTH + (NUMVISINVSLOTS-1) * ST_INVSLOTOFFX - 2, y - 1);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);

#undef TRACKING
#undef BORDER
}

static void inventoryMove(hud_inventory_t* inv, int dir, boolean canWrap)
{
    if(dir == 1)
    {   // Move right.
        uint maxVisSlots;

        if(inv->selected == inv->numUsedSlots - 1)
        {
            if(canWrap)
                inv->selected = 0;
        }
        else
            inv->selected++;

        // First the fixed range statusbar cursor.
        if(inv->fixedCursorPos < NUMVISINVSLOTS - 1 &&
           !(inv->fixedCursorPos + 1 > inv->numUsedSlots - 1))
            inv->fixedCursorPos++;

        // Now the variable range full-screen cursor.
        if(cfg.inventorySlotMaxVis)
            maxVisSlots = cfg.inventorySlotMaxVis;
        else
            maxVisSlots = NUM_INVENTORYITEM_TYPES - 1;

        if(inv->varCursorPos < maxVisSlots - 1 &&
           !(inv->varCursorPos + 1 > inv->numUsedSlots - 1))
            inv->varCursorPos++;

        return;
    }

    // Else, a move left.

    if(inv->selected == 0)
    {
        if(canWrap)
            inv->selected = inv->numUsedSlots - 1;
    }
    else
        inv->selected--;

    // First the fixed range statusbar cursor.
    if(inv->fixedCursorPos > 0)
        inv->fixedCursorPos--;

    // Now the variable range full-screen cursor.
    if(inv->varCursorPos > 0)
        inv->varCursorPos--;
}

void Hu_InventoryOpen(int player, boolean show)
{
    hud_inventory_t* inv;
    player_t* plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!plr->plr->inGame)
        return;

    inv = &hudInventories[player];

    if(show)
    {
        inv->flags |= HIF_VISIBLE;
        inv->hideTics = (int) (cfg.inventoryTimer * TICSPERSEC);

        ST_HUDUnHide(player, HUE_FORCE);
    }
    else
    {
        inv->flags &= ~HIF_VISIBLE;
        P_InventorySetReadyItem(player, P_GetInvItem(inv->slots[inv->selected])->type);
    }
}

boolean Hu_InventoryIsOpen(int player)
{
    hud_inventory_t* inv;

    if(player < 0 || player >= MAXPLAYERS)
        return false;
    inv = &hudInventories[player];

    return ((inv->flags & HIF_VISIBLE)? true : false);
}

/**
 * Mark the HUD inventory as dirty (i.e., the player inventory state has
 * changed in such a way that would require the HUD inventory display(s)
 * to be updated e.g., the player gains a new item).
 *
 * @param player        Player whoose in HUD inventory is dirty.
 */
void Hu_InventoryMarkDirty(int player)
{
    if(player < 0 || player >= MAXPLAYERS)
        return;

    hudInventories[player].flags |= HIF_IS_DIRTY;
}

boolean Hu_InventorySelect(int player, inventoryitemtype_t type)
{
    assert(type == IIT_NONE ||
           (type >= IIT_FIRST && type < NUM_INVENTORYITEM_TYPES));

    if(player >= 0 && player < MAXPLAYERS)
    {
        hud_inventory_t*    inv = &hudInventories[player];

        if(P_InventoryCount(player, type))
        {
            uint                i;

            for(i = 0; i < inv->numUsedSlots; ++i)
                if(P_GetInvItem(inv->slots[i])->type == type)
                {
                    inv->selected = i;
                    inv->varCursorPos = inv->fixedCursorPos = 0;

                    return true;
                }
        }
    }

    return false;
}

boolean Hu_InventoryMove(int player, int dir, boolean canWrap, boolean silent)
{
    player_t*           plr;
    hud_inventory_t*    inv;

    if(player < 0 || player >= MAXPLAYERS)
        return false;

    plr = &players[player];
    if(!plr->plr->inGame)
        return false;
    inv = &hudInventories[player];

    // Do the move first, before updating a possibly out of date inventory.
    if(inv->numOwnedItemTypes > 1)
    {
        inventoryMove(inv, dir, canWrap);
    }

    if(inv->flags & HIF_IS_DIRTY)
    {
        rebuildInventory(inv);
    }

    if(inv->numOwnedItemTypes > 1)
    {
        P_InventorySetReadyItem(player,
            P_GetInvItem(inv->selected)->type);
    }

    if(!silent)
    {
        inv->hideTics = (int) (cfg.inventoryTimer * TICSPERSEC);
    }

    return true;
}

void Hu_InventoryInit(void)
{
    int                 i;

    memset(hudInventories, 0, sizeof(hudInventories));

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hud_inventory_t*    inv = &hudInventories[i];

        inv->flags = HIF_IS_DIRTY;
    }
}

void Hu_InventoryTicker(void)
{
    int                 i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t*           plr = &players[i];
        hud_inventory_t*    inv = &hudInventories[i];

        if(!plr->plr->inGame)
            continue;

        if(inv->flags & HIF_IS_DIRTY)
            rebuildInventory(inv);

        if(!Pause_IsPaused())
        {
            if(Hu_InventoryIsOpen(i))
            {
                // Turn inventory off after a certain amount of time?
                if(cfg.inventoryTimer == 0)
                {
                    inv->hideTics = 0;
                }
                else
                {
                    if(inv->hideTics > 0)
                        inv->hideTics--;
                    if(inv->hideTics == 0 && cfg.inventoryTimer > 0)
                        Hu_InventoryOpen(i, false); // Close the inventory.
                }
            }
        }
    }
}

void ST_ResizeInventory(void)
{
    int i;
    uint maxVisSlots;

    if(cfg.inventorySlotMaxVis)
        maxVisSlots = cfg.inventorySlotMaxVis;
    else
        maxVisSlots = NUM_INVENTORYITEM_TYPES - 1;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hud_inventory_t*    inv = &hudInventories[i];

        if(inv->varCursorPos >= maxVisSlots - 1)
        {
            inv->varCursorPos = maxVisSlots - 1;
        }
        inv->flags |= HIF_IS_DIRTY;
    }
}

#endif
