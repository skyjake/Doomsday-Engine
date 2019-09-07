/** @file hu_inventory.cpp  HUD player inventory widget.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#if defined(__JHERETIC__) || defined(__JHEXEN__)

#include "common.h"
#include "hu_inventory.h"

#include "hu_stuff.h"
#include "p_tick.h"
#include "p_inventory.h"

#include <doomsday/gamefw/defs.h>
#include <cmath>
#include <cstring>

using namespace de;

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

struct hud_inventory_t
{
    byte flags;  ///< HIF_* flags
    int hideTics;
    uint numOwnedItemTypes;

    uint invSlots[NUM_INVENTORYITEM_TYPES - 1];
    uint numUsedSlots;
    uint selected;

    uint varCursorPos;    ///< Variable-range, fullscreen inv cursor.
    uint fixedCursorPos;  ///< Fixed-range, statusbar inv cursor.
};

static hud_inventory_t hudInventories[MAXPLAYERS];

/**
 * Returns the maximum number of visible slots for the fullscreen HUD inventory.
 */
static uint maxVisibleSlots()
{
    if(cfg.inventorySlotMaxVis)
    {
        return cfg.inventorySlotMaxVis;
    }
    return NUM_INVENTORYITEM_TYPES - 1;
}

/**
 * Rebuild the inventory item type table.
 * These indices can be used to associate slots in an inventory browser
 * to the items held within.
 */
static void rebuildInventory(hud_inventory_t *inv)
{
    DE_ASSERT(inv);
    const int plrNum = inv - hudInventories;
    const inventoryitemtype_t selectedType = P_GetInvItem(inv->invSlots[inv->selected])->type;

    inv->selected = 0;

    uint numOwnedItemTypes = 0;
    for(int i = 0; i < NUM_INVENTORYITEM_TYPES - 1; ++i)
    {
        if(P_InventoryCount(plrNum, inventoryitemtype_t(IIT_FIRST + i)) > 0)
            numOwnedItemTypes++;
    }

    inv->numOwnedItemTypes = inv->numUsedSlots = numOwnedItemTypes;  // Always 1:1

    std::memset(inv->invSlots, 0, sizeof(inv->invSlots[0]) * (NUM_INVENTORYITEM_TYPES - 1));

    if(inv->numUsedSlots)
    {
        uint idx = 0;
        for(int i = 0; i < NUM_INVENTORYITEM_TYPES - 1; ++i)
        {
            if(P_InventoryCount(plrNum, inventoryitemtype_t(IIT_FIRST + i)))
            {
                inv->invSlots[idx] = i;
                if(P_GetInvItem(i)->type == selectedType)
                {
                    inv->selected = idx;
                }

                if(++idx >= inv->numUsedSlots)
                    break;
            }
        }
    }

    inv->flags &= ~HIF_IS_DIRTY;
}

static void inventoryIndexes(const player_t * /*plr*/, const hud_inventory_t *inv,
    uint maxVisSlots, int origCursor, uint *firstVisible, uint *cursorPos,
    uint *fromSlot, uint *toSlot)
{
    int cursor, first, from, to;

    if(!firstVisible && !cursorPos && !fromSlot && !toSlot)
        return;

    if(cfg.inventorySelectMode)
    {
        // Scroll.
        cursor = maxVisSlots / 2;

        if(cfg.inventoryWrap)
        {
            first = inv->selected - cursor;
            while(first < 0) first += inv->numUsedSlots;

            from = 0;
            to   = maxVisSlots;
        }
        else
        {
            first = inv->selected - cursor;
            if(first < 0) first = 0;

            from = cursor - inv->selected;
            if(from < 0) from = 0;

            int last = inv->selected + cursor + (maxVisSlots % 2? 1 : 0);
            if(last - 1 < (signed) inv->numUsedSlots)
            {
                to = maxVisSlots;
            }
            else
            {
                to = maxVisSlots - (last - inv->numUsedSlots);
            }
        }
    }
    else
    {
        // Cursor.
        cursor = origCursor;

        if(inv->numUsedSlots < maxVisSlots)
        {
            from = (maxVisSlots - inv->numUsedSlots) / 2;
            to   = from + inv->numUsedSlots;

            cursor += from;
        }
        else
        {
            if(cfg.inventoryWrap)
            {
                from = 0;
                to   = maxVisSlots;
            }
            else
            {
                from = de::max(0, cursor - (signed)inv->selected);
                to   = maxVisSlots;
            }
        }

        first = inv->selected - origCursor;
        if(cfg.inventoryWrap)
        {
            while(first < 0) first += inv->numUsedSlots;
        }
        else
        {
            if(inv->numUsedSlots < maxVisSlots ||
               first + maxVisSlots > inv->numUsedSlots)
            {
                int shift = inv->numUsedSlots - (first + maxVisSlots);

                first += shift;
                if(first < 0)
                {
                    first = 0;
                    cursor = from + inv->selected;
                }
                else
                {
                    cursor -= shift;
                }
            }
        }

        if(first < 0) first = 0;
    }

    if(from < 0) from = 0;

    if(firstVisible) *firstVisible = (unsigned) first;
    if(cursorPos)    *cursorPos    = (unsigned) cursor;
    if(fromSlot)     *fromSlot     = (unsigned) from;
    if(toSlot)       *toSlot       = (unsigned) to;
}

void Hu_InventoryDraw(int player, int x, int y, float textOpacity, float iconOpacity)
{
#define BORDER                  (1)
#if __JHERETIC__
# define TRACKING               (2)
#else
# define TRACKING               (0)
#endif

    if(player < 0 || player >= MAXPLAYERS) return;
    player_t *plr = &players[player];

    const hud_inventory_t *inv = &hudInventories[player];
    const uint maxVisSlots     = maxVisibleSlots();

    uint first, selected, startSlot, endSlot;
    inventoryIndexes(plr, inv, maxVisSlots, inv->varCursorPos, &first, &selected, &startSlot, &endSlot);

    const uint numVisSlots = maxVisSlots;
    const float availWidth = SCREENWIDTH - 50 * 2, width = (numVisSlots * ST_INVSLOTWIDTH);
    const float invScale   = (width > availWidth)? availWidth / width : 1;
    const float lightDelta = ((maxVisSlots % 2)? 1.f / maxVisSlots : 1.f / (maxVisSlots - 1)) * 2;

    uint idx = first;
    uint slot = (cfg.inventorySlotShowEmpty? 0 : startSlot);
    uint from = (cfg.inventorySlotShowEmpty? 0 : startSlot);

    uint to;
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

    for(uint i = from; i < to; ++i)
    {
        float light;
        if(i < maxVisSlots / 2)
            light = (i + 1) * lightDelta;
        else
            light = (maxVisSlots - i) * lightDelta;

        float a = (i == selected? .5f : light / 2);

        DGL_Color4f(light, light, light, a * iconOpacity);
        GL_DrawPatch(pInvItemBox, Vec2i(x + slot * ST_INVSLOTWIDTH + (slot > 1? (slot-1) * ST_INVSLOTOFFX : 0), y));

        if(i >= startSlot && i < endSlot)
        {
            const invitem_t *item = P_GetInvItem(inv->invSlots[idx]);
            uint count            = P_InventoryCount(player, item->type);
            if(count)
            {
#if __JHEXEN__
                int posX = x + slot * ST_INVSLOTWIDTH + (slot > 1? (slot-1) * ST_INVSLOTOFFX : 0) - 1;
#else
                int posX = x + slot * ST_INVSLOTWIDTH + (slot > 1? (slot-1) * ST_INVSLOTOFFX : 0);
#endif
                DGL_Color4f(1, 1, 1, slot == selected? iconOpacity : iconOpacity / 2);
                GL_DrawPatch(item->patchId, Vec2i(posX, y + ST_INVICONOFFY));

                if(count > 1)
                {
                    char buf[20];
                    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], slot == selected? textOpacity : textOpacity / 2);
                    dd_snprintf(buf, 20, "%i", count);
                    FR_DrawTextXY3(buf, posX + ST_INVCOUNTOFFX, y + ST_INVCOUNTOFFY, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
                }
            }

            if(++idx > inv->numOwnedItemTypes - 1)
                idx = 0;
        }
        slot++;
    }

    DGL_Color4f(1, 1, 1, iconOpacity);
    GL_DrawPatch(pInvSelectBox, Vec2i(x + selected * ST_INVSLOTWIDTH + (selected > 1? (selected-1) * ST_INVSLOTOFFX : 0), y + ST_INVSELECTOFFY - BORDER));

    if(inv->numUsedSlots > maxVisSlots)
    {
#define ARROW_RELXOFF          2
#define ARROW_YOFFSET          9

        if(cfg.inventoryWrap || first != 0)
        {
            DGL_Color4f(1, 1, 1, iconOpacity);
            GL_DrawPatch(pInvPageLeft[!(mapTime & 4)? 1 : 0], Vec2i(x - ARROW_RELXOFF, y + ARROW_YOFFSET), ALIGN_TOPRIGHT, 0);
        }

        if(cfg.inventoryWrap || inv->numUsedSlots - first > numVisSlots)
        {
            DGL_Color4f(1, 1, 1, iconOpacity);
            GL_DrawPatch(pInvPageRight[!(mapTime & 4)? 1 : 0], Vec2i(x + numVisSlots * ST_INVSLOTWIDTH + (numVisSlots > 1? (numVisSlots-1) * ST_INVSLOTOFFX : 0) + ARROW_RELXOFF - 2, y + ARROW_YOFFSET));
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

    if(alpha <= 0) return;

    if(player < 0 || player >= MAXPLAYERS) return;
    player_t *plr = &players[player];

    const hud_inventory_t *inv = &hudInventories[player];

    uint first, cursor, startSlot, endSlot;
    inventoryIndexes(plr, inv, NUMVISINVSLOTS, inv->fixedCursorPos, &first, &cursor, &startSlot, &endSlot);

    uint idx = first;
    uint from = startSlot;
    uint slot = startSlot;
    uint to;
    if(startSlot > 0)
        to = NUMVISINVSLOTS;
    else
        to = endSlot - startSlot;
    if(inv->numUsedSlots - 1 < endSlot - startSlot)
        to = from + inv->numUsedSlots;

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_SMALLIN));
    FR_SetTracking(TRACKING);

    for(uint i = from; i < to; ++i)
    {
        if(i >= startSlot && i < endSlot)
        {
            const invitem_t *item = P_GetInvItem(inv->invSlots[idx]);
            const uint count      = P_InventoryCount(player, item->type);
            if(count)
            {
                DGL_Color4f(1, 1, 1, alpha);
                GL_DrawPatch(item->patchId, Vec2i(x + slot * ST_INVSLOTWIDTH, y + ST_INVICONOFFY));

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
    GL_DrawPatch(pInvSelectBox, Vec2i(x + cursor * ST_INVSLOTWIDTH, y + ST_INVSELECTOFFY - BORDER));

    if(inv->numUsedSlots > NUMVISINVSLOTS)
    {
        // Draw more left indicator.
        if(cfg.inventoryWrap || first != 0)
        {
            DGL_Color4f(1, 1, 1, alpha);
            GL_DrawPatch(pInvPageLeft[!(mapTime & 4)? 1 : 0], Vec2i(x - 12, y - 1));
        }

        // Draw more right indicator.
        if(cfg.inventoryWrap || inv->numUsedSlots - first > NUMVISINVSLOTS)
        {
            DGL_Color4f(1, 1, 1, alpha);
            GL_DrawPatch(pInvPageRight[!(mapTime & 4)? 1 : 0], Vec2i(x + NUMVISINVSLOTS * ST_INVSLOTWIDTH + (NUMVISINVSLOTS-1) * ST_INVSLOTOFFX - 2, y - 1));
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);

#undef TRACKING
#undef BORDER
}

static void inventoryMove(hud_inventory_t *inv, int dir, dd_bool canWrap)
{
    DE_ASSERT(inv);
    const uint maxVisSlots = maxVisibleSlots();

    if(dir == 1)
    {
        // Move right.
        if(inv->selected < inv->numUsedSlots - 1)
        {
            inv->selected++;
        }
        else if(canWrap)
        {
            inv->selected = 0;
        }

        // Fixed range statusbar cursor.
        if(inv->fixedCursorPos < NUMVISINVSLOTS - 1 &&
           inv->fixedCursorPos < inv->numUsedSlots - 1)
        {
            inv->fixedCursorPos++;
        }
        else if(canWrap && !cfg.inventoryWrap)
        {
            inv->fixedCursorPos = 0;
        }

        // Variable range full-screen cursor.
        if(inv->varCursorPos < maxVisSlots - 1 &&
           inv->varCursorPos < inv->numUsedSlots - 1)
        {
            inv->varCursorPos++;
        }
        else if(canWrap && !cfg.inventoryWrap)
        {
            inv->varCursorPos = 0;
        }
    }
    else
    {
        // Move left.
        if(inv->selected == 0)
        {
            if(canWrap) inv->selected = inv->numUsedSlots - 1;
        }
        else
        {
            inv->selected--;
        }

        // Fixed range statusbar cursor.
        if(inv->fixedCursorPos > 0)
        {
            inv->fixedCursorPos--;
        }
        else if(canWrap && !cfg.inventoryWrap)
        {
            inv->fixedCursorPos = de::min(uint(NUMVISINVSLOTS), inv->numUsedSlots) - 1;
        }

        // Variable range full-screen cursor.
        if(inv->varCursorPos > 0)
        {
            inv->varCursorPos--;
        }
        else if(canWrap && !cfg.inventoryWrap)
        {
            inv->varCursorPos = de::min(maxVisSlots, inv->numUsedSlots) - 1;
        }
    }
}

void Hu_InventoryOpen(int player, dd_bool show)
{
    if(player < 0 || player >= MAXPLAYERS) return;
    player_t *plr = &players[player];
    if(!plr->plr->inGame) return;

    hud_inventory_t *inv = &hudInventories[player];

    if(show)
    {
        inv->flags |= HIF_VISIBLE;
        inv->hideTics = int(cfg.inventoryTimer * TICSPERSEC);

        ST_HUDUnHide(player, HUE_FORCE);
    }
    else
    {
        inv->flags &= ~HIF_VISIBLE;
        P_InventorySetReadyItem(player, P_GetInvItem(inv->invSlots[inv->selected])->type);
    }
}

dd_bool Hu_InventoryIsOpen(int player)
{
    if(player < 0 || player >= MAXPLAYERS) return false;

    hud_inventory_t *inv = &hudInventories[player];
    return (inv->flags & HIF_VISIBLE) != 0;
}

void Hu_InventoryMarkDirty(int player)
{
    if(player < 0 || player >= MAXPLAYERS) return;

    hudInventories[player].flags |= HIF_IS_DIRTY;
}

dd_bool Hu_InventorySelect(int player, inventoryitemtype_t type)
{
    DE_ASSERT(type == IIT_NONE || (type >= IIT_FIRST && type < NUM_INVENTORYITEM_TYPES));

    if(player >= 0 && player < MAXPLAYERS)
    {
        hud_inventory_t *inv = &hudInventories[player];

        if(P_InventoryCount(player, type))
        {
            for(uint i = 0; i < inv->numUsedSlots; ++i)
            {
                if(P_GetInvItem(inv->invSlots[i])->type == type)
                {
                    inv->selected = i;
                    inv->varCursorPos = inv->fixedCursorPos = 0;
                    return true;
                }
            }
        }
    }

    return false;
}

dd_bool Hu_InventoryMove(int player, int dir, dd_bool canWrap, dd_bool silent)
{
    if(player < 0 || player >= MAXPLAYERS) return false;
    player_t *plr = &players[player];
    if(!plr->plr->inGame) return false;

    hud_inventory_t *inv = &hudInventories[player];

    // Do the move first, before updating a possibly out of date inventory.
    if(inv->numOwnedItemTypes > 1)
    {
        inventoryMove(inv, dir, canWrap);
    }

    if(inv->flags & HIF_IS_DIRTY)
    {
        rebuildInventory(inv);
    }

    if(inv->numOwnedItemTypes >= 1)
    {
        P_InventorySetReadyItem(player, P_GetInvItem(inv->invSlots[inv->selected])->type);
    }

    if(!silent)
    {
        inv->hideTics = int(cfg.inventoryTimer * TICSPERSEC);
    }

    return true;
}

void Hu_InventoryInit()
{
    std::memset(hudInventories, 0, sizeof(hudInventories));

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        hud_inventory_t *inv = &hudInventories[i];
        inv->flags = HIF_IS_DIRTY;
    }
}

void Hu_InventoryTicker()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr = &players[i];
        hud_inventory_t *inv = &hudInventories[i];

        if(!plr->plr->inGame)
            continue;

        if(inv->flags & HIF_IS_DIRTY)
        {
            rebuildInventory(inv);
        }

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

void ST_ResizeInventory()
{
    const uint maxVisSlots = maxVisibleSlots();
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        hud_inventory_t *inv = &hudInventories[i];

        if(inv->varCursorPos >= maxVisSlots - 1)
        {
            inv->varCursorPos = maxVisSlots - 1;
        }
        inv->flags |= HIF_IS_DIRTY;
    }
}

void Hu_InventoryRegister()
{
    cvartemplate_t hudInvCVars[] = {
        {"hud-inventory-timer", 0, CVT_FLOAT, &cfg.inventoryTimer, 0, 30},
        {"hud-inventory-slot-showempty", 0, CVT_BYTE, &cfg.inventorySlotShowEmpty, 0, 1},
        {"hud-inventory-slot-max", CVF_NO_MAX, CVT_INT, &cfg.inventorySlotMaxVis, 0, 0, ST_ResizeInventory},
        {NULL}
    };

    for(int i = 0; hudInvCVars[i].path; ++i)
    {
        Con_AddVariable(hudInvCVars + i);
    }
}

#endif
