/**\file m_defs.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * Common menu defines and types.
 */

#ifndef LIBCOMMON_M_DEFS_H
#define LIBCOMMON_M_DEFS_H

#include "r_common.h"

#define LEFT_DIR                0
#define RIGHT_DIR               1

// Menu object types.
typedef enum {
    MN_NONE,
    MN_TEXT,
    MN_BUTTON,
    MN_BUTTON2, // Staydown/2-state button.
    MN_BUTTON2EX, // Staydown/2-state with additional data.
    MN_EDIT,
    MN_LIST,
    MN_LISTINLINE,
    MN_SLIDER,
    MN_COLORBOX,
    MN_BINDINGS
} mn_obtype_e;

/**
 * @defgroup menuObjectFlags Menu Object Flags
 */
/*@{*/
#define MNF_HIDDEN              0x1
#define MNF_DISABLED            0x2 // Can't be interacted with.
//#define MNF_PAUSED              0x4 // Ticker not called.
#define MNF_CLICKED             0x8
#define MNF_INACTIVE            0x10 // Object active.
//#define MNF_FOCUS               0x20 // Has focus.
//#define MNF_NO_FOCUS            0x40 // Can't receive focus.
//#define MNF_DEFAULT             0x80 // Has focus by default.
//#define MNF_LEFT_ALIGN          0x100
//#define MNF_FADE_AWAY           0x200 // Fade UI away while the control is active.
//#define MNF_NEVER_FADE          0x400
#define MNF_NO_ALTTEXT          0x800 // Don't use alt text instead of lump (M_NMARE)
#define MNF_ID0                 0x10000000
#define MNF_ID1                 0x20000000
#define MNF_ID2                 0x40000000
#define MNF_ID3                 0x80000000
/*@}*/

typedef struct mn_object_s {
    mn_obtype_e     type; // Type of the object.
    int             group;
    int             flags; // @see menuObjectFlags.
    const char*     text;
    int             fontIdx;
    int             colorIdx;
    patchid_t*      patch;
    void          (*drawer) (const struct mn_object_s* obj, int x, int y, float alpha);
    void          (*dimensions) (const struct mn_object_s* obj, int* width, int* height);
    void          (*action) (struct mn_object_s* obj, int option);
    void*           data; // Pointer to extra data.
    int             data2; // Extra numerical data.
    int             timer;
} mn_object_t;

/**
 * @defGroup menuPageFlags Menu Page Flags
 */
/*@{*/
#define MNPF_NOHOTKEYS          0x00000001 // Hotkeys are disabled.
/*@}*/

typedef struct mn_page_unscaledstate_s {
    uint            numVisObjects;
    int             y;
} mn_page_unscaledstate_t;

typedef struct mn_page_s {
    mn_object_t*    _objects; // List of objects.
    uint            _size;
    int             flags; // @see menuPageFlags.
    int             _offset[2];
    void          (*drawer) (const struct mn_page_s*, int, int);
    int             focus; // Index of the focus object.
    struct mn_page_s* previous; // Pointer to the previous page, if any.
    // For scrollable multi-pages.
    uint            firstObject, numVisObjects;
    // Scalable pages.
    mn_page_unscaledstate_t unscaled;
} mn_page_t;

mn_page_t*      MN_CurrentPage(void);

void            MNText_Drawer(const mn_object_t* obj, int x, int y, float alpha);
void            MNText_Dimensions(const mn_object_t* obj, int* width, int* height);

/**
 * Two-state button.
 */
typedef struct mndata_button_s {
    void*           data;
    const char*     yes, *no;
} mndata_button_t;

void            MNButton_Drawer(const mn_object_t* obj, int x, int y, float alpha);
void            MNButton_Dimensions(const mn_object_t* obj, int* width, int* height);

/**
 * Edit field.
 */
#define MNDATA_EDIT_TEXT_MAX_LENGTH 24

typedef struct mndata_edit_s {
    char            text[MNDATA_EDIT_TEXT_MAX_LENGTH+1];
    char            oldtext[MNDATA_EDIT_TEXT_MAX_LENGTH+1]; // If the current edit is canceled...
    uint            maxVisibleChars;
    const char*     emptyString; // Drawn when editfield is empty/null.
    int             data;
    void          (*onChange) (const struct mndata_edit_s*);
} mndata_edit_t;

void            MNEdit_Drawer(const mn_object_t* obj, int x, int y, float alpha);
boolean         MNEdit_Responder(mn_object_t* obj, const event_t* ev);
void            MNEdit_Dimensions(const mn_object_t* obj, int* width, int* height);

/**
 * List selection.
 */
#define MNDATA_LIST_LEADING     .5f
#define NUMLISTITEMS(x) (sizeof(x)/sizeof(mndata_listitem_t))

typedef struct {
    char            text[256];
    int             data;
    int             data2;
} mndata_listitem_t;

typedef struct mndata_list_s {
    void*           items;
    int             count; // Number of items.
    void*           data;
    int             selection; // Selected item (-1 if none).
    int             first; // First visible item.
} mndata_list_t;

void            MNList_Drawer(const mn_object_t* obj, int x, int y, float alpha);
void            MNList_Dimensions(const mn_object_t* obj, int* width, int* height);

typedef mndata_list_t mndata_listinline_t;

void            MNListInline_Drawer(const mn_object_t* obj, int x, int y, float alpha);
void            MNListInline_Dimensions(const mn_object_t* obj, int* width, int* height);

/**
 * Color preview box.
 */
#define MNDATA_COLORBOX_WIDTH   4 // Inner width in fixed 320x200 space.
#define MNDATA_COLORBOX_HEIGHT  4 // Inner height in fixed 320x200 space.

void            MNColorBox_Drawer(const mn_object_t* obj, int x, int y, float alpha);
void            MNColorBox_Dimensions(const mn_object_t* obj, int* width, int* height);

/**
 * Graphical slider.
 */
#define MNDATA_SLIDER_SLOTS     10
#define MNDATA_SLIDER_SCALE     .75f

typedef struct mndata_slider_s {
    float           min, max;
    float           value;
    float           step; // Button step.
    boolean         floatMode; // Otherwise only integers are allowed.
    /// \todo Turn this into a property record or something.
    void*           data1;
    void*           data2;
    void*           data3;
    void*           data4;
    void*           data5;
} mndata_slider_t;

void            MNSlider_Drawer(const mn_object_t* obj, int x, int y, float alpha);
void            MNSlider_TextualValueDrawer(const mn_object_t* obj, int x, int y, float alpha);
void            MNSlider_Dimensions(const mn_object_t* obj, int* width, int* height);
void            MNSlider_TextualValueDimensions(const mn_object_t* obj, int* width, int* height);
int             MNSlider_ThumbPos(const mn_object_t* obj);

/**
 * Bindings visualizer.
 */
typedef struct mndata_bindings_s {
    const char*     text;
    const char*     bindContext;
    const char*     controlName;
    const char*     command;
    int             flags;
} mndata_bindings_t;

void            MNBindings_Drawer(const mn_object_t* obj, int x, int y, float alpha);
void            MNBindings_Dimensions(const mn_object_t* obj, int* width, int* height);

#define MN_CURSOR_COUNT         2
#define MN_CURSOR_TICSPERFRAME  8

extern int mnTime;
extern boolean shiftdown;
extern mn_page_t* mnCurrentPage;
extern short mnFocusObjectIndex;

extern mn_page_t MainMenu;
extern mn_page_t GameTypeMenu;
#if __JHEXEN__
extern mn_page_t PlayerClassMenu;
#endif
#if __JDOOM__ || __JHERETIC__
extern mn_page_t EpisodeMenu;
#endif
extern mn_page_t SkillLevelMenu;
extern mn_page_t OptionsMenu;
extern mn_page_t SoundMenu;
extern mn_page_t GameplayMenu;
extern mn_page_t HUDMenu;
extern mn_page_t AutomapMenu;
extern mn_object_t MAPItems[];
#if __JHERETIC__ || __JHEXEN__
extern mn_page_t FilesMenu;
#endif
extern mn_page_t LoadMenu;
extern mn_page_t SaveMenu;
extern mn_page_t MultiplayerMenu;
extern mn_object_t MultiplayerItems[];
extern mn_object_t MultiplayerServerItems[];
extern mn_object_t MultiplayerClientItems[];
extern mn_page_t GameSetupMenu;
extern mn_object_t GameSetupItems[];
extern mn_page_t PlayerSetupMenu;
extern mn_object_t PlayerSetupItems[];
#if __JHERETIC__ || __JHEXEN__
extern mn_page_t InventoryMenu;
#endif
extern mn_page_t WeaponMenu;
extern mn_page_t ControlsMenu;

void            M_DrawSaveLoadBorder(int x, int y, int width);
void            MN_GotoPage(mn_page_t* page);

void            M_StartControlPanel(void);
void            M_QuitDOOM(mn_object_t* obj, int option);

void            M_FloatMod10(float* variable, int option);

void            SCEnterMultiplayerMenu(mn_object_t* obj, int option);

void            MN_TickerEx(void); // The extended ticker.

void            MN_UpdateGameSaveWidgets(void);

// Color widget.
void            MN_ActivateColorBox(mn_object_t* obj, int option);
void            M_WGCurrentColor(mn_object_t* obj, int option);

#endif /* LIBCOMMON_M_DEFS_H */
