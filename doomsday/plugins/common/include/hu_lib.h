/**\file hu_lib.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_GUI_LIBRARY_H
#define LIBCOMMON_GUI_LIBRARY_H

#include "dd_types.h"

typedef enum {
    GUI_NONE,
    GUI_BOX,
    GUI_HEALTH,
    GUI_ARMOR,
    GUI_KEYS,
    GUI_READYAMMO,
    GUI_FRAGS,
    GUI_LOG,
    GUI_CHAT,
#if __JDOOM__
    GUI_AMMO,
    GUI_WEAPONSLOT,
    GUI_FACE,
    GUI_ARMORICON,
#endif
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
    GUI_WORLDTIMER,
#endif
#if __JDOOM__ || __JHERETIC__
    GUI_READYAMMOICON,
    GUI_KEYSLOT,
    GUI_SECRETS,
    GUI_ITEMS,
    GUI_KILLS,
#endif
#if __JHERETIC__ || __JHEXEN__
    GUI_INVENTORY,
    GUI_CHAIN,
    GUI_READYITEM,
    GUI_FLIGHT,
#endif
} guiwidgettype_t;

typedef struct {
    int value;
} guidata_health_t;

typedef struct {
    int value;
} guidata_armor_t;

typedef struct {
    int value;
} guidata_readyammo_t;

typedef struct {
    int value;
} guidata_frags_t;

#if __JDOOM__ || __JHERETIC__
typedef struct {
    int slot;
    keytype_t keytypeA;
    patchid_t patchId;
#  if __JDOOM__
    keytype_t keytypeB;
    patchid_t patchId2;
#  endif
} guidata_keyslot_t;
#endif

#if __JDOOM__
typedef struct {
    ammotype_t ammotype;
    int value;
} guidata_ammo_t;

typedef struct {
    int slot;
    patchid_t patchId;
} guidata_weaponslot_t;
#endif

#if __JDOOM__
typedef struct {
    int oldHealth; // Used to use appopriately pained face.
    int faceCount; // Count until face changes.
    int faceIndex; // Current face index, used by wFaces.
    int lastAttackDown;
    int priority;
    boolean oldWeaponsOwned[NUM_WEAPON_TYPES];
} guidata_face_t;
#endif

typedef struct {
    boolean keyBoxes[NUM_KEY_TYPES];
} guidata_keys_t;

#if __JDOOM__ || __JHERETIC__
typedef struct {
#if __JHERETIC__
    patchid_t patchId;
#else // __JDOOM__
    int sprite;
#endif
} guidata_readyammoicon_t;

typedef struct {
    int value;
} guidata_secrets_t;

typedef struct {
    int value;
} guidata_items_t;

typedef struct {
    int value;
} guidata_kills_t;
#endif

#if __JDOOM__
typedef struct {
    int sprite;
} guidata_armoricon_t;
#endif

#if __JHERETIC__
typedef struct {
    patchid_t patchId;
    int countdownSeconds; // Number of seconds remaining or zero if disabled.
    int play; // Used with the countdown sound.
} guidata_tomeofpower_t;
#endif

#if __JHEXEN__
typedef struct {
    struct armortype_s {
        int value;
    } types[NUMARMOR];
} guidata_armoricons_t;

typedef struct {
    int pieces;
} guidata_weaponpieces_t;

typedef struct {
    int iconIdx;
} guidata_bluemanaicon_t;

typedef struct {
    int value;
} guidata_bluemana_t;

typedef struct {
    int iconIdx;
    float filled;
} guidata_bluemanavial_t;

typedef struct {
    int iconIdx;
} guidata_greenmanaicon_t;

typedef struct {
    int value;
} guidata_greenmana_t;

typedef struct {
    int iconIdx;
    float filled;
} guidata_greenmanavial_t;

typedef struct {
    patchid_t patchId;
} guidata_boots_t;

typedef struct {
    patchid_t patchId;
} guidata_servant_t;

typedef struct {
    patchid_t patchId;
} guidata_defense_t;

typedef struct {
    int days, hours, minutes, seconds;
} guidata_worldtimer_t;
#endif

#if __JHERETIC__ || __JHEXEN__
typedef struct {
    int healthMarker;
    int wiggle;
} guidata_chain_t;

typedef struct {
    patchid_t patchId;
    int flashCounter;
} guidata_readyitem_t;

typedef struct {
    patchid_t patchId;
    boolean hitCenterFrame;
} guidata_flight_t;
#endif

void GUI_Init(void);
void GUI_Shutdown(void);

typedef struct uiwidget_s {
    guiwidgettype_t type;
    int player; /// \todo refactor away.
    int hideId;
    void (*dimensions) (struct uiwidget_s* obj, int* width, int* height);
    void (*drawer) (struct uiwidget_s* obj, int x, int y);
    void (*ticker) (struct uiwidget_s* obj);
    void* typedata;
} uiwidget_t;

typedef int uiwidgetid_t;

uiwidgetid_t GUI_CreateWidget(guiwidgettype_t type, int player, int hideId,
    void (*dimensions) (uiwidget_t* obj, int* width, int* height),
    void (*drawer) (uiwidget_t* obj, int x, int y),
    void (*ticker) (uiwidget_t* obj), void* typedata);

int GUI_CreateGroup(int name, short flags, int padding);

/**
 * @defgroup uiWidgetGroupFlags  UI Widget Group Flags.
 */
/*@{*/
#define UWGF_ALIGN_LEFT         0x0001
#define UWGF_ALIGN_RIGHT        0x0002
#define UWGF_ALIGN_TOP          0x0004
#define UWGF_ALIGN_BOTTOM       0x0008
#define UWGF_LEFTTORIGHT        0x0010
#define UWGF_RIGHTTOLEFT        0x0020
#define UWGF_TOPTOBOTTOM        0x0040
#define UWGF_BOTTOMTOTOP        0x0080
/*@}*/

typedef struct {
    int name; // Name of the group.
    short flags;
    int padding;
    int widgetIdCount;
    uiwidgetid_t* widgetIds;
} uiwidgetgroup_t;

int GUI_GroupName(uiwidgetgroup_t* group);

void GUI_GroupAddWidget(uiwidgetgroup_t* group, uiwidgetid_t id);
short GUI_GroupFlags(uiwidgetgroup_t* group);
void GUI_GroupSetFlags(uiwidgetgroup_t* group, short flags);

void GUI_DrawWidgets(uiwidgetgroup_t* group, int x, int y, int availWidth, int availHeight,
    float alpha, int* drawnWidth, int* drawnHeight);

void GUI_TickWidgets(uiwidgetgroup_t* group);

uiwidgetgroup_t* GUI_FindGroupForName(int name);

typedef struct ui_rendstate_s {
    float pageAlpha;
} ui_rendstate_t;
extern const ui_rendstate_t* uiRendState;

#endif /* LIBCOMMON_UI_LIBRARY_H */
