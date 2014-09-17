/** @file hu_lib.h  HUD widget library.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "common.h"
#include "hu_stuff.h"

typedef enum menucommand_e
{
    MCMD_OPEN, // Open the menu.
    MCMD_CLOSE, // Close the menu.
    MCMD_CLOSEFAST, // Instantly close the menu.
    MCMD_NAV_OUT, // Navigate "out" of the current menu/widget (up a level).
    MCMD_NAV_LEFT,
    MCMD_NAV_RIGHT,
    MCMD_NAV_DOWN,
    MCMD_NAV_UP,
    MCMD_NAV_PAGEDOWN,
    MCMD_NAV_PAGEUP,
    MCMD_SELECT, // Execute whatever action is attaced to the current item.
    MCMD_DELETE
} menucommand_e;

typedef enum mn_page_colorid_e
{
    MENU_COLOR1 = 0,
    MENU_COLOR2,
    MENU_COLOR3,
    MENU_COLOR4,
    MENU_COLOR5,
    MENU_COLOR6,
    MENU_COLOR7,
    MENU_COLOR8,
    MENU_COLOR9,
    MENU_COLOR10,
    MENU_COLOR_COUNT
} mn_page_colorid_t;

#define VALID_MNPAGE_COLORID(v)      ((v) >= MENU_COLOR1 && (v) < MENU_COLOR_COUNT)

typedef enum mn_page_fontid_e
{
    MENU_FONT1 = 0,
    MENU_FONT2,
    MENU_FONT3,
    MENU_FONT4,
    MENU_FONT5,
    MENU_FONT6,
    MENU_FONT7,
    MENU_FONT8,
    MENU_FONT9,
    MENU_FONT10,
    MENU_FONT_COUNT
} mn_page_fontid_t;

#define VALID_MNPAGE_FONTID(v)      ((v) >= MENU_FONT1 && (v) < MENU_FONT_COUNT)

#ifdef __cplusplus

namespace common {
namespace menu {

/**
 * @defgroup menuEffectFlags  Menu Effect Flags
 */
///@{
#define MEF_TEXT_TYPEIN             (DTF_NO_TYPEIN)
#define MEF_TEXT_SHADOW             (DTF_NO_SHADOW)
#define MEF_TEXT_GLITTER            (DTF_NO_GLITTER)

#define MEF_EVERYTHING              (MEF_TEXT_TYPEIN|MEF_TEXT_SHADOW|MEF_TEXT_GLITTER)
///@}

short MN_MergeMenuEffectWithDrawTextFlags(short f);

} // namespace menu
} // namespace common

#endif // __cplusplus

void lerpColor(float *dst, float const *a, float const *b, float t, dd_bool rgbaMode);

typedef enum {
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
    GUI_AUTOMAP
    //GUI_MAPNAME
} guiwidgettype_t;

typedef int uiwidgetid_t;

typedef struct uiwidget_s {
    /// Type of this widget.
    guiwidgettype_t type;

    /// Unique identifier associated with this widget.
    uiwidgetid_t id;

    /// @ref alignmentFlags
    int alignFlags;

    /// Maximum size of this widget in pixels.
    Size2Raw maxSize;

    /// Geometry of this widget in pixels.
    Rect* geometry;

    /// Local player number associated with this widget.
    /// \todo refactor away.
    int player;

    /// Current font used for text child objects of this widget.
    fontid_t font;

    /// Current opacity value for this widget.
    float opacity;

    void (*updateGeometry) (struct uiwidget_s* ob);
    void (*drawer) (struct uiwidget_s* ob, Point2Raw const *offset);
    void (*ticker) (struct uiwidget_s* ob, timespan_t ticLength);

    void* typedata;
} uiwidget_t;

#ifdef __cplusplus
extern "C" {
#endif

void GUI_DrawWidget(uiwidget_t* ob, Point2Raw const *origin);
void GUI_DrawWidgetXY(uiwidget_t* ob, int x, int y);

/// @return  @ref alignmentFlags
int UIWidget_Alignment(uiwidget_t* ob);

float UIWidget_Opacity(uiwidget_t* ob);

const Rect* UIWidget_Geometry(uiwidget_t* ob);

int UIWidget_MaximumHeight(uiwidget_t* ob);

const Size2Raw* UIWidget_MaximumSize(uiwidget_t* ob);

int UIWidget_MaximumWidth(uiwidget_t* ob);

const Point2* UIWidget_Origin(uiwidget_t* ob);

/// @return  Local player number of the owner of this widget.
int UIWidget_Player(uiwidget_t* ob);

void UIWidget_RunTic(uiwidget_t* ob, timespan_t ticLength);

void UIWidget_SetOpacity(uiwidget_t* ob, float alpha);

/**
 * @param alignFlags  @ref alignmentFlags
 */
void UIWidget_SetAlignment(uiwidget_t* ob, int alignFlags);

void UIWidget_SetMaximumHeight(uiwidget_t* ob, int height);

void UIWidget_SetMaximumSize(uiwidget_t* ob, const Size2Raw* size);

void UIWidget_SetMaximumWidth(uiwidget_t* ob, int width);

#ifdef __cplusplus
} // extern "C"
#endif

/**
 * @defgroup uiWidgetGroupFlags  UIWidget Group Flags
 */
///@{
#define UWGF_VERTICAL           0x0004
///@}

typedef struct {
    /// Order of child objects.
    order_t order;

    /// @ref uiWidgetGroupFlags
    int flags;

    int padding;

    int widgetIdCount;

    uiwidgetid_t* widgetIds;
} guidata_group_t;

#ifdef __cplusplus
extern "C" {
#endif

void UIGroup_AddWidget(uiwidget_t* ob, uiwidget_t* other);
int UIGroup_Flags(uiwidget_t* ob);
void UIGroup_SetFlags(uiwidget_t* ob, int flags);

void UIGroup_UpdateGeometry(uiwidget_t* ob);

#ifdef __cplusplus
} // extern "C"
#endif

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
    dd_bool oldWeaponsOwned[NUM_WEAPON_TYPES];
} guidata_face_t;
#endif

typedef struct {
    dd_bool keyBoxes[NUM_KEY_TYPES];
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
} guidata_readyitem_t;

typedef struct {
    patchid_t patchId;
    dd_bool hitCenterFrame;
} guidata_flight_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

void GUI_Register(void);

void GUI_Init(void);
void GUI_Shutdown(void);
void GUI_LoadResources(void);
void GUI_ReleaseResources(void);

uiwidget_t* GUI_FindObjectById(uiwidgetid_t id);

/// Identical to GUI_FindObjectById except results in a fatal error if not found.
uiwidget_t* GUI_MustFindObjectById(uiwidgetid_t id);

uiwidgetid_t GUI_CreateWidget(guiwidgettype_t type, int player, int alignFlags,
    fontid_t fontId, float opacity,
    void (*updateGeometry) (uiwidget_t* ob), void (*drawer) (uiwidget_t* ob, Point2Raw const *offset),
    void (*ticker) (uiwidget_t* ob, timespan_t ticLength), void* typedata);

uiwidgetid_t GUI_CreateGroup(int groupFlags, int player, int alignFlags, order_t order, int padding);

#ifdef __cplusplus
} // extern "C"
#endif

typedef struct ui_rendstate_s {
    float pageAlpha;
} ui_rendstate_t;

DENG_EXTERN_C const ui_rendstate_t* uiRendState;

#endif /* LIBCOMMON_UI_LIBRARY_H */
