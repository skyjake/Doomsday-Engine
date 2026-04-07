/** @file hu_lib.h  HUD widget library.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_GUI_LIBRARY_H
#define LIBCOMMON_GUI_LIBRARY_H

#include "hud/hudwidget.h"

// TODO If HudElementName needs to be this specific, then it it needs to be refactored
//      in to something that's not an enum.
//      If HudElementName is the means to refer to hud elements, and plugins can create their
//      own, then this is severely broken
enum HudElementName
{
    GUI_NONE,
    GUI_BOX,
    GUI_GROUP,
    GUI_HEALTH,
    GUI_ARMOR,
    GUI_KEYS,
    GUI_READYAMMO,
    GUI_FRAGS,
    GUI_LOG,
    GUI_CHAT,
    GUI_AMMO,
    GUI_MAXAMMO,
    GUI_WEAPONSLOT,
    GUI_FACE,
    GUI_HEALTHICON,
    GUI_ARMORICON,
#if __JHERETIC__
    GUI_TOME,
#endif
#if __JHEXEN__
    GUI_ARMORICONS,
    GUI_WEAPONPIECES,
    GUI_BLUEMANAICON,
    GUI_BLUEMANA,
    GUI_BLUEMANAVIAL,
    GUI_GREENMANAICON,
    GUI_GREENMANA,
    GUI_GREENMANAVIAL,
    GUI_BOOTS,
    GUI_SERVANT,
    GUI_DEFENSE,
    GUI_WORLDTIME,
#endif
    GUI_READYAMMOICON,
    GUI_KEYSLOT,
    GUI_SECRETS,
    GUI_ITEMS,
    GUI_KILLS,
#if __JHERETIC__ || __JHEXEN__
    GUI_INVENTORY,
    GUI_CHAIN,
    GUI_READYITEM,
    GUI_FLIGHT,
#endif
    GUI_AUTOMAP
};

void GUI_Register();

void GUI_Init();
void GUI_Shutdown();
void GUI_LoadResources();
void GUI_ReleaseResources();

HudWidget *GUI_TryFindWidgetById(uiwidgetid_t id);

HudWidget &GUI_FindWidgetById(uiwidgetid_t id);

HudWidget *GUI_AddWidget(HudWidget *wi);

void GUI_UpdateWidgetGeometry(HudWidget *wi);

void GUI_DrawWidget(HudWidget *wi, const Point2Raw *origin);
void GUI_DrawWidgetXY(HudWidget *wi, int x, int y);

void GUI_SpriteSize(int sprite, float scale, int *width, int *height);

enum hotloc_t
{
    HOT_TLEFT,
    HOT_TRIGHT,
    HOT_BRIGHT,
    HOT_BLEFT,
    HOT_B,
    HOT_LEFT
};

void GUI_DrawSprite(int sprite, float x, float y, hotloc_t hotspot, float scale, float alpha,
    dd_bool flip, int *drawnWidth, int *drawnHeight);

struct ui_rendstate_t
{
    float pageAlpha;
};

DE_EXTERN_C const ui_rendstate_t *uiRendState;

#endif  // LIBCOMMON_UI_LIBRARY_H
