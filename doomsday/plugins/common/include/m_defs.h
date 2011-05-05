/**\file m_defs.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

// Menu object types.
typedef enum {
    MN_NONE,
    MN_TEXT,
    MN_BUTTON,
    MN_BUTTON2, // Staydown/2-state button.
    MN_BUTTON2EX, // Staydown/2-state with additional data.
    MN_EDIT,
    MN_LIST,
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
#define MNF_ACTIVE              0x10 // Object active.
#define MNF_FOCUS               0x20 // Has focus.
#define MNF_NO_FOCUS            0x40 // Can't receive focus.
#define MNF_DEFAULT             0x80 // Has focus by default.
//#define MNF_LEFT_ALIGN          0x100
//#define MNF_FADE_AWAY           0x200 // Fade UI away while the control is active.
//#define MNF_NEVER_FADE          0x400
#define MNF_NO_ALTTEXT          0x800 // Don't use alt text instead of lump (M_NMARE)
#define MNF_ID0                 0x10000000
#define MNF_ID1                 0x20000000
#define MNF_ID2                 0x40000000
#define MNF_ID3                 0x80000000
/*@}*/

/**
 * MNObject. Abstract base from which all menu page objects must be derived.
 */
typedef struct mn_object_s {
    /// Type of the object.
    mn_obtype_e type;

    /// Object group identifier.
    int group;

    /// @see menuObjectFlags.
    int flags;

    /// Used in various ways depending on the context.
    /// \todo Does not belong here, move it out.
    const char* text;

    /// Index of the predefined page font to use when drawing this.
    /// \todo Does not belong here, move it out.
    int pageFontIdx;

    /// Index of the predefined page color to use when drawing this.
    /// \todo Does not belong here, move it out.
    int pageColorIdx;

    /// Patch to be used when drawing this.
    /// \todo Does not belong here, move it out.
    patchid_t* patch;

    /// Calculate dimensions for this when visible on the specified page.
    void (*dimensions) (const struct mn_object_s* obj, struct mn_page_s* page, int* width, int* height);

    /// Draw this at the specified offset within the owning view-space.
    /// Can be @c NULL in which case this will never be drawn.
    void (*drawer) (struct mn_object_s* obj, int x, int y);

    /// "Action" callback to be exectued as directed by the logic of the
    /// responder callbacks. Can be @c NULL.
    void (*action) (struct mn_object_s* obj);

    /// Respond to the given (menu) @a command. Can be @c NULL.
    /// @return  @c true if the command is eaten.
    int (*cmdResponder) (struct mn_object_s* obj, menucommand_e command);

    /// Respond to the given (input) event @a ev. Can be @c NULL.
    /// @return  @c true if the event is eaten.
    int (*responder) (struct mn_object_s* obj, event_t* ev);

    /// Respond to the given (input) event @a ev. Can be @c NULL.
    /// @return  @c true if the event is eaten.
    int (*privilegedResponder) (struct mn_object_s* obj, event_t* ev);

    void* data; // Pointer to extra data.
    int data2; // Extra numerical data.
} mn_object_t;

int MNObject_DefaultCommandResponder(mn_object_t* obj, menucommand_e command);

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
    // Auto-initialized.
    uint objectsCount;
} mn_page_t;

void MNPage_ComposeSubpageString(mn_page_t* page, size_t bufSize, char* buf);
mn_object_t* MNPage_FocusObject(mn_page_t* page);
void MNPage_PredefinedColor(mn_page_t* page, mn_page_colorid_t id, float rgb[3]);
int MNPage_PredefinedFont(mn_page_t* page, mn_page_fontid_t id);

/**
 * Text objects.
 */
void MNText_Drawer(mn_object_t* obj, int x, int y);
void MNText_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height);

/**
 * Two-state button.
 */
typedef struct mndata_button_s {
    void* data;
    const char* yes, *no;
} mndata_button_t;

void MNButton_Drawer(mn_object_t* obj, int x, int y);
int MNButton_CommandResponder(mn_object_t* obj, menucommand_e command);
void MNButton_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height);

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
void MNEdit_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height);

/**
 * @defgroup mneditSetTextFlags  MNEdit Set Text Flags
 * @{
 */
#define MNEDIT_STF_NO_ACTION            0x1 /// Do not call any linked action function.
/**@}*/

/**
 * Change the current contents of the edit field.
 * @param flags  @see mneditSetTextFlags
 * @param string  New text string which will replace the existing string.
 */
void MNEdit_SetText(mn_object_t* obj, int flags, const char* string);

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
    int mask;
    int selection; // Selected item (-1 if none).
    int first; // First visible item.
    int numvis;
} mndata_list_t;

void MNList_Drawer(mn_object_t* obj, int x, int y);
void MNList_InlineDrawer(mn_object_t* obj, int x, int y);

int MNList_CommandResponder(mn_object_t* obj, menucommand_e command);
int MNList_InlineCommandResponder(mn_object_t* obj, menucommand_e command);

void MNList_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height);
void MNList_InlineDimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height);

int MNList_FindItem(const mn_object_t* obj, int dataValue);

/**
 * @defgroup mnlistSelectItemFlags  MNList Select Item Flags
 * @{
 */
#define MNLIST_SIF_NO_ACTION            0x1 /// Do not call any linked action function.
/**@}*/

/**
 * Change the currently selected item.
 * @param flags  @see mnlistSelectItemFlags
 * @param itemIndex  Index of the new selection.
 * @return  @c true if the selected item changed.
 */
boolean MNList_SelectItem(mn_object_t* obj, int flags, int itemIndex);

/**
 * Change the currently selected item by looking up its data value.
 * @param flags  @see mnlistSelectItemFlags
 * @param dataValue  Value associated to the candidate item being selected.
 * @return  @c true if the selected item changed.
 */
boolean MNList_SelectItemByValue(mn_object_t* obj, int flags, int itemIndex);

/**
 * Color preview box.
 */
#define MNDATA_COLORBOX_WIDTH   4 // Default inner width in fixed 320x200 space.
#define MNDATA_COLORBOX_HEIGHT  4 // Default inner height in fixed 320x200 space.
#define MNDATA_COLORBOX_PADDING_X   3 // Inclusive of the outer border.
#define MNDATA_COLORBOX_PADDING_Y   5 //

typedef struct mndata_colorbox_s {
    /// Inner dimensions in fixed 320x200 space. If @c <= 0 the default
    /// dimensions will be used instead.
    int width, height;
    float r, g, b, a;
    boolean rgbaMode;
    void* data1;
    void* data2;
    void* data3;
    void* data4;
} mndata_colorbox_t;

void MNColorBox_Drawer(mn_object_t* obj, int x, int y);
int MNColorBox_CommandResponder(mn_object_t* obj, menucommand_e command);
void MNColorBox_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height);

/**
 * @defgroup mncolorboxSetColorFlags  MNColorBox Set Color Flags.
 * @{
 */
#define MNCOLORBOX_SCF_NO_ACTION        0x1 /// Do not call any linked action function.
/**@}*/

/**
 * Change the current color of the color box.
 * @param flags  @see mncolorboxSetColorFlags
 * @param rgba  New color and alpha. Note: will be NOP if this colorbox
 *      is not operating in "rgba mode".
 * @return  @c true if the current color changed.
 */
boolean MNColorBox_SetColor4fv(mn_object_t* obj, int flags, float rgba[4]);
boolean MNColorBox_SetColor4f(mn_object_t* obj, int flags, float red, float green,
    float blue, float alpha);

/// Change the current red color component.
/// @return  @c true if the value changed.
boolean MNColorBox_SetRedf(mn_object_t* obj, int flags, float red);

/// Change the current green color component.
/// @return  @c true if the value changed.
boolean MNColorBox_SetGreenf(mn_object_t* obj, int flags, float green);

/// Change the current blue color component.
/// @return  @c true if the value changed.
boolean MNColorBox_SetBluef(mn_object_t* obj, int flags, float blue);

/// Change the current alpha value. Note: will be NOP if this colorbox
/// is not operating in "rgba mode".
/// @return  @c true if the value changed.
boolean MNColorBox_SetAlphaf(mn_object_t* obj, int flags, float alpha);

/**
 * Copy the current color from @a other.
 * @param flags  @see mncolorboxSetColorFlags
 * @return  @c true if the current color changed.
 */
boolean MNColorBox_CopyColor(mn_object_t* obj, int flags, const mn_object_t* otherObj);

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
void MNSlider_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height);
void MNSlider_TextualValueDimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height);
int MNSlider_ThumbPos(const mn_object_t* obj);

/**
 * @defgroup mnsliderSetValueFlags  MNSlider Set Value Flags
 * @{
 */
#define MNSLIDER_SVF_NO_ACTION            0x1 /// Do not call any linked action function.
/**@}*/

/**
 * Change the current value represented by the slider.
 * @param flags  @see mnsliderSetValueFlags
 * @param value  New value.
 */
void MNSlider_SetValue(mn_object_t* obj, int flags, float value);

/**
 * Bindings visualizer.
 */
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
void MNBindings_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height);

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
void MNMobjPreview_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height);

// Menu render state:
typedef struct mn_rendstate_s {
    float pageAlpha;
    float textGlitter;
    float textShadow;
    float textColors[MENU_COLOR_COUNT][4];
    gamefontid_t textFonts[MENU_FONT_COUNT];
} mn_rendstate_t;
extern const mn_rendstate_t* mnRendState;

/**
 * @defgroup menuEffectFlags  Menu Effect Flags
 * @{
 */
#define DTFTOMEF_SHIFT              (4) // 0x10000 to 0x1

#define MEF_TEXT_TYPEIN             (DTF_NO_TYPEIN  >> DTFTOMEF_SHIFT)
#define MEF_TEXT_SHADOW             (DTF_NO_SHADOW  >> DTFTOMEF_SHIFT)
#define MEF_TEXT_GLITTER            (DTF_NO_GLITTER >> DTFTOMEF_SHIFT)

#define MEF_EVERYTHING              (MEF_TEXT_TYPEIN|MEF_TEXT_SHADOW|MEF_TEXT_GLITTER)
/**@}*/

short MN_MergeMenuEffectWithDrawTextFlags(short f);

int MN_CountObjects(mn_object_t* list);

/**
 * Execute a menu navigation/action command.
 */
void Hu_MenuCommand(menucommand_e cmd);

#endif /* LIBCOMMON_M_DEFS_H */
