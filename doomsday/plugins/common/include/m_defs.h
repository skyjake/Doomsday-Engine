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
#include "hu_stuff.h"

typedef enum menucommand_e {
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
    MN_BINDINGS,
    MN_MOBJPREVIEW
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
    mn_obtype_e type; // Type of the object.
    int group;
    int flags; // @see menuObjectFlags.
    const char* text;
    int pageFontIdx;
    int pageColorIdx;
    patchid_t* patch;
    void (*dimensions) (const struct mn_object_s* obj, int* width, int* height);
    void (*drawer) (struct mn_object_s* obj, int x, int y);
    void (*action) (struct mn_object_s* obj);
    int (*cmdResponder) (struct mn_object_s* obj, menucommand_e command);
    int (*responder) (struct mn_object_s* obj, event_t* ev);
    int (*privilegedResponder) (struct mn_object_s* obj, event_t* ev);
    void* data; // Pointer to extra data.
    int data2; // Extra numerical data.
    //int timer;
} mn_object_t;

/**
 * @defGroup menuPageFlags Menu Page Flags
 */
/*@{*/
#define MNPF_NOHOTKEYS          0x00000001 // Hotkeys are disabled.
/*@}*/

typedef enum {
    MENU_COLOR1,
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

typedef enum {
    MENU_FONT1,
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

/*typedef struct mn_page_unscaledstate_s {
    uint numVisObjects;
    int y;
} mn_page_unscaledstate_t;*/

typedef struct mn_page_s {
    mn_object_t* objects; // List of objects.
    uint objectsCount;
    int focus; // Index of the focus object.
    int flags; // @see menuPageFlags.
    int offset[2];
    gamefontid_t fonts[MENU_FONT_COUNT];
    uint colors[MENU_COLOR_COUNT];
    void (*drawer) (struct mn_page_s* page, int x, int y);
    int (*cmdResponder) (struct mn_page_s* page, menucommand_e cmd);
    struct mn_page_s* previous; // Pointer to the previous page, if any.
    void* data;
    // For scrollable multi-pages.
    //uint firstObject, numVisObjects;
    // Scalable pages.
    //mn_page_unscaledstate_t unscaled;
} mn_page_t;

void MNPage_ComposeSubpageString(mn_page_t* page, size_t bufSize, char* buf);
mn_object_t* MNPage_FocusObject(mn_page_t* page);
void MNPage_PredefinedColor(mn_page_t* page, mn_page_colorid_t id, float rgb[3]);
int MNPage_PredefinedFont(mn_page_t* page, mn_page_fontid_t id);

/**
 * Text objects.
 */
void MNText_Drawer(mn_object_t* obj, int x, int y);
void MNText_Dimensions(const mn_object_t* obj, int* width, int* height);

/**
 * Two-state button.
 */
typedef struct mndata_button_s {
    void* data;
    const char* yes, *no;
} mndata_button_t;

void MNButton_Drawer(mn_object_t* obj, int x, int y);
int MNButton_CommandResponder(mn_object_t* obj, menucommand_e command);
void MNButton_Dimensions(const mn_object_t* obj, int* width, int* height);

/**
 * Edit field.
 */
#define MNDATA_EDIT_TEXT_MAX_LENGTH 24

typedef struct mndata_edit_s {
    char text[MNDATA_EDIT_TEXT_MAX_LENGTH+1];
    char oldtext[MNDATA_EDIT_TEXT_MAX_LENGTH+1]; // If the current edit is canceled...
    uint maxVisibleChars;
    const char* emptyString; // Drawn when editfield is empty/null.
    void* data1;
    int data2;
    void (*onChange) (mn_object_t* obj);
} mndata_edit_t;

void MNEdit_Drawer(mn_object_t* obj, int x, int y);
int MNEdit_CommandResponder(mn_object_t* obj, menucommand_e command);
int MNEdit_Responder(mn_object_t* obj, const event_t* ev);
void MNEdit_Dimensions(const mn_object_t* obj, int* width, int* height);
void MNEdit_SetText(mn_object_t* obj, const char* string);

/**
 * List selection.
 */
#define MNDATA_LIST_LEADING     .5f
#define NUMLISTITEMS(x) (sizeof(x)/sizeof(mndata_listitem_t))

typedef struct {
    const char* text;
    int data;
} mndata_listitem_t;

typedef struct mndata_list_s {
    void* items;
    int count; // Number of items.
    void* data;
    int selection; // Selected item (-1 if none).
    int first; // First visible item.
} mndata_list_t;

void MNList_Drawer(mn_object_t* obj, int x, int y);
int MNList_CommandResponder(mn_object_t* obj, menucommand_e command);
void MNList_Dimensions(const mn_object_t* obj, int* width, int* height);
int MNList_FindItem(const mn_object_t* obj, int dataValue);

typedef mndata_list_t mndata_listinline_t;

void MNListInline_Drawer(mn_object_t* obj, int x, int y);
int MNListInline_CommandResponder(mn_object_t* obj, menucommand_e command);
void MNListInline_Dimensions(const mn_object_t* obj, int* width, int* height);

/**
 * Color preview box.
 */
#define MNDATA_COLORBOX_WIDTH   4 // Inner width in fixed 320x200 space.
#define MNDATA_COLORBOX_HEIGHT  4 // Inner height in fixed 320x200 space.
#define MNDATA_COLORBOX_PADDING_X   3 // Inclusive of the outer border.
#define MNDATA_COLORBOX_PADDING_Y   5 //

typedef struct mndata_colorbox_s {
    float* r, *g, *b, *a;
} mndata_colorbox_t;

void MNColorBox_Drawer(mn_object_t* obj, int x, int y);
int MNColorBox_CommandResponder(mn_object_t* obj, menucommand_e command);
void MNColorBox_Dimensions(const mn_object_t* obj, int* width, int* height);

/**
 * Graphical slider.
 */
#define MNDATA_SLIDER_SLOTS     10
#define MNDATA_SLIDER_SCALE     .75f
#if __JDOOM__ || __JDOOM64__
#  define MNDATA_SLIDER_PADDING_Y   2
#else
#  define MNDATA_SLIDER_PADDING_Y   0
#endif

typedef struct mndata_slider_s {
    float min, max;
    float value;
    float step; // Button step.
    boolean floatMode; // Otherwise only integers are allowed.
    /// \todo Turn this into a property record or something.
    void* data1;
    void* data2;
    void* data3;
    void* data4;
    void* data5;
} mndata_slider_t;

void MNSlider_Drawer(mn_object_t* obj, int x, int y);
void MNSlider_TextualValueDrawer(mn_object_t* obj, int x, int y);
int MNSlider_CommandResponder(mn_object_t* obj, menucommand_e command);
void MNSlider_Dimensions(const mn_object_t* obj, int* width, int* height);
void MNSlider_TextualValueDimensions(const mn_object_t* obj, int* width, int* height);
int MNSlider_ThumbPos(const mn_object_t* obj);

/**
 * Bindings visualizer.
 */

// Binding iteration flags
#define MIBF_IGNORE_REPEATS     0x1

typedef enum {
    MIBT_KEY,
    MIBT_MOUSE,
    MIBT_JOY
} bindingitertype_t;

typedef struct mndata_bindings_s {
    const char* text;
    const char* bindContext;
    const char* controlName;
    const char* command;
    int flags;
} mndata_bindings_t;

void MNBindings_Drawer(mn_object_t* obj, int x, int y);
int MNBindings_CommandResponder(mn_object_t* obj, menucommand_e command);
int MNBindings_PrivilegedResponder(mn_object_t* obj, event_t* ev);
void MNBindings_Dimensions(const mn_object_t* obj, int* width, int* height);
void MNBindings_IterateBinds(mn_object_t* obj, const char* bindings, int flags, void* paramaters,
    void (*callback)(bindingitertype_t type, int bid, const char* event, boolean isInverse, void* paramaters));

/**
 * Mobj preview visual.
 */
#define MNDATA_MOBJPREVIEW_WIDTH    38
#define MNDATA_MOBJPREVIEW_HEIGHT   52

typedef struct mndata_mobjpreview_s {
    mobjtype_t mobjType;
    /// Color translation class and map.
    int tClass, tMap;
#if __JHEXEN__
    int plrClass; /// Player class identifier.
#endif
} mndata_mobjpreview_t;

void MNMobjPreview_Drawer(mn_object_t* obj, int x, int y);
void MNMobjPreview_Dimensions(const mn_object_t* obj, int* width, int* height);

#define MENU_CURSOR_FRAMECOUNT      2
#define MENU_CURSOR_TICSPERFRAME    8

// Menu render state:
typedef struct mn_rendstate_s {
    float page_alpha;
    float text_glitter;
    float text_shadow;
} mn_rendstate_t;
extern const mn_rendstate_t* mnRendState;

/**
 * Retrieve the current menu page. Note that this is the menu systems'
 * state-defined current page and NOT the active page, which may or may
 * not be the same.
 * @return  Current menu page.
 */
mn_page_t* MN_CurrentPage(void);

/**
 * Change the current menu page. Note that this is the menu systems'
 * state-defined current page and NOT the active page, which may or may
 * not be the same.
 * @return  New current menu page, for caller convenience.
 */
mn_page_t* MN_SetCurrentPage(mn_page_t* page);

/**
 * Execute a menu navigation/action command.
 */
void Hu_MenuCommand(menucommand_e cmd);

#endif /* LIBCOMMON_M_DEFS_H */
