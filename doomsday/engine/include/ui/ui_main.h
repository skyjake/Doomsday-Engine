/**\file ui_main.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Graphical User Interface.
 */

#ifndef LIBDENG_UI_MAIN_H
#define LIBDENG_UI_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <de/rect.h>

#define IS_ACTKEY(x)    (x == ' ' || x == DDKEY_RETURN)

typedef enum {
    UI_NONE,
    UI_TEXT,
    UI_BOX,
    UI_FOCUSBOX, /// Can receive focus.
    UI_BUTTON,
    UI_BUTTON2, /// Staydown/2-state button.
    UI_BUTTON2EX, /// Staydown/2-state with additional data.
    UI_EDIT,
    UI_LIST,
    UI_SLIDER,
    UI_META /// Special: affects all objects up to the next meta.
} ui_obtype_e;

/// Standard dimensions.
#define UI_WIDTH            (1000.0f)
#define UI_HEIGHT           (1000.0f)
#define UI_BORDER           (UI_WIDTH/120) /// All borders are this wide.
#define UI_SHADOW_OFFSET    (MIN_OF(3, UI_WIDTH/320))
#define UI_SHADOW_STRENGTH  (.6f)
#define UI_BUTTON_BORDER    (UI_BORDER)
#define UI_BAR_WDH          (UI_BORDER * 3)
#define UI_BAR_BORDER       (UI_BORDER / 2)
#define UI_BAR_BUTTON_BORDER (3 * UI_BAR_BORDER / 2)
#define UI_MAX_COLUMNS      10 /// Maximum columns for list box.

/// Object flags.
#define UIF_HIDDEN          0x1
#define UIF_DISABLED        0x2 /// Can't be interacted with.
#define UIF_PAUSED          0x4 /// Ticker not called.
#define UIF_CLICKED         0x8
#define UIF_ACTIVE          0x10 /// Object active.
#define UIF_FOCUS           0x20 /// Has focus.
#define UIF_NO_FOCUS        0x40 /// Can't receive focus.
#define UIF_DEFAULT         0x80 /// Has focus by default.
#define UIF_LEFT_ALIGN      0x100
#define UIF_FADE_AWAY       0x200 /// Fade UI away while the control is active.
#define UIF_NEVER_FADE      0x400
#define UIF_ID0             0x10000000
#define UIF_ID1             0x20000000
#define UIF_ID2             0x40000000
#define UIF_ID3             0x80000000

/// Special group: no group.
#define UIG_NONE            -1

/**
 * Flag group modes (for UI_FlagGroup).
 */
enum {
    UIFG_CLEAR,
    UIFG_SET,
    UIFG_XOR
};

/**
 * Button arrows.
 */
enum {
    UIBA_NONE,
    UIBA_UP,
    UIBA_DOWN,
    UIBA_LEFT,
    UIBA_RIGHT
};

typedef struct {
    float red, green, blue;
} ui_color_t;

typedef struct ui_object_s {
    ui_obtype_e type; /// Type of the object.
    int group;
    int flags;
    int relx, rely, relw, relh; /// Relative placement.
    char text[256]; /// Used in various ways.
    void (*drawer) (struct ui_object_s*);
    int (*responder) (struct ui_object_s*, ddevent_t*);
    void (*ticker) (struct ui_object_s*);
    void (*action) (struct ui_object_s*);
    void* data; /// Pointer to extra data.
    int data2; /// Extra numerical data.
    int timer;
    /// Position and dimensions, auto-inited.
    RectRaw geometry;
} ui_object_t;

/**
 * UI Pages consist of one or more controls.
 */
typedef struct ui_page_s {
    struct ui_page_flags_s {
        char showBackground:1; /// Draw the background?
    } flags;

    /// List of objects, UI_NONE terminates.
    ui_object_t* _objects;

    /// Index of the focus object.
    int focus;

    /// Index of the capture object.
    int capture;

    void (*drawer) (struct ui_page_s*);
    void (*ticker) (struct ui_page_s*);
    int  (*responder) (struct ui_page_s*, ddevent_t*);

    /// Pointer to the previous page, if any.
    struct ui_page_s* previous;

    uint _timer;
    /// Object count, no need to initialize.
    uint count;
} ui_page_t;

typedef struct {
    void* data;
    const char* yes;
    const char* no;
} uidata_button_t;

typedef struct {
    char* ptr; /// Text to modify.
    int maxlen; /// Maximum allowed length.
    void* data;
    uint cp; /// Cursor position.
} uidata_edit_t;

typedef struct {
    char text[256];
    int data;
    int data2;
} uidata_listitem_t;

typedef struct {
    void* items;
    int count; /// Number of items.
    void* data;
    int selection; /// Selected item (-1 if none).
    int first; /// First visible item.
    int itemhgt; /// Height of each item (0 = fonthgt).
    int numvis; /// Number of visible items (updated at SetPage).
    byte button[3]; /// Button states (0=normal, 1=down).
    int column[UI_MAX_COLUMNS]; /// Column offsets (real coords).
} uidata_list_t;

typedef struct {
    float min, max;
    float value;
    float step; /// Button step.
    boolean floatmode; /// Otherwise only integers are allowed.
    void* data;
    char* zerotext;
    byte button[3]; /// Button states (0=normal, 1=down).
} uidata_slider_t;

void UI_Register(void);

/// Called when entering a UI page.
void UI_PageInit(boolean halttime, boolean tckui, boolean tckframe, boolean drwgame, boolean noescape);

/// Called upon exiting a ui page.
void UI_End(void);

/// @return  @c true, if the UI is currently active.
boolean UI_IsActive(void);

/// @return  Ptr to the current UI page if active.
ui_page_t* UI_CurrentPage(void);

/**
 * Set the alpha level of the entire UI. Alpha levels below one automatically
 * show the game view in addition to the UI.
 *
 * @param alpha  Alpha level to set the UI too (0...1)
 */
void UI_SetAlpha(float alpha);

/// @return  Current alpha level of the UI.
float UI_Alpha(void);

/// @param id  Id number of the color to return e.g. "UIC_TEXT".
ui_color_t* UI_Color(uint id);

/// @return  Height of the current UI font.
int UI_FontHeight(void);
void UI_LoadTextures(void);
void UI_ReleaseTextures(void);

/// Initializes UI page data prior to use.
void UI_InitPage(ui_page_t* page, ui_object_t* objects);

/// Change and prepare the active page.
void UI_SetPage(ui_page_t* page);

/// Update active page's layout for a new window size.
void UI_UpdatePageLayout(void);

/// Directs events through the ui and current page if active.
int UI_Responder(const ddevent_t* ev);

void UI_Ticker(timespan_t time);

/// Draws the current ui page if active.
void UI_Drawer(void);

int UI_CountObjects(ui_object_t* list);
void UI_FlagGroup(ui_object_t* list, int group, int flags, int set);

/// All the specified flags must be set.
ui_object_t* UI_FindObject(ui_object_t* list, int group, int flags);

/// @param ob Must be on the current page! It can't be NULL.
void UI_Focus(ui_object_t* ob);

/// Set focus to the object under the mouse cursor.
void UI_MouseFocus(void);

/// Set focus to the object that should get focus by default.
void UI_DefaultFocus(ui_page_t* page);

/// @param ob  If @c NULL,, capture is ended. Must be on the current page!
void UI_Capture(ui_object_t* ob);

/**
 * Default callbacks.
 */

int UIPage_Responder(ui_page_t* page, ddevent_t* ev);

/// Call the ticker routine for each object.
void UIPage_Ticker(ui_page_t* page);

/// Draws the ui including all objects on the current page.
void UIPage_Drawer(ui_page_t* page);

void UIFrame_Drawer(ui_object_t* ob);

void UIText_Drawer(ui_object_t* ob);
void UIText_BrightDrawer(ui_object_t* ob);

int UIButton_Responder(ui_object_t* ob, ddevent_t* ev);
void UIButton_Drawer(ui_object_t* ob);

int UIEdit_Responder(ui_object_t* ob, ddevent_t* ev);
void UIEdit_Drawer(ui_object_t* ob);

int UIList_Responder(ui_object_t* ob, ddevent_t* ev);
void UIList_Ticker(ui_object_t* ob);
void UIList_Drawer(ui_object_t* ob);

/**
 * Calculate the geometry of the visible Item Selection region in screen space.
 *
 * @param rect  Calculated values written here.
 * @return  Same as @a rect for caller convenience.
 */
RectRaw* UIList_ItemGeometry(ui_object_t* ob, RectRaw* rect);

/**
 * Calculate the geometry of the Up Button in screen space.
 *
 * @param rect  Calculated values written here.
 * @return  Same as @a rect for caller convenience.
 */
RectRaw* UIList_ButtonUpGeometry(ui_object_t* ob, RectRaw* rect);

/**
 * Calculate the geometry of the Down Button in screen space.
 *
 * @param rect  Calculated values written here.
 * @return  Same as @a rect for caller convenience.
 */
RectRaw* UIList_ButtonDownGeometry(ui_object_t* ob, RectRaw* rect);

/**
 * Calculate the geometry of the Thumb scroller in screen space.
 *
 * @param rect  Calculated values written here.
 * @return  Same as @a rect for caller convenience.
 */
RectRaw* UIList_ThumbGeometry(ui_object_t* ob, RectRaw* rect);

int UISlider_Responder(ui_object_t* ob, ddevent_t* ev);
void UISlider_Ticker(ui_object_t* ob);
void UISlider_Drawer(ui_object_t* ob);

/**
 * Helpers.
 */

/// Width of the available page area, in pixels.
int UI_AvailableWidth(void);

/// Height of the available page area, in pixels.
int UI_AvailableHeight(void);

/// Coordinate space conversion: relative to screen space.
int UI_ScreenX(int relx);
int UI_ScreenY(int rely);
int UI_ScreenW(int relw);
int UI_ScreenH(int relh);

void UI_InitColumns(ui_object_t* ob);

int UI_MouseInsideRect(const RectRaw* rect);
int UI_MouseInsideBox(const Point2Raw* origin, const Size2Raw* size);

/// @return  @c true, if the mouse is inside the object.
int UI_MouseInside(ui_object_t* ob);

/// @return  @c true, if the mouse hasn't been moved for a while.
int UI_MouseResting(ui_page_t* page);

int UI_ListFindItem(ui_object_t* ob, int data_value);
void UI_DrawLogo(const Point2Raw* origin, const Size2Raw* size);

/**
 * Background with the "The Doomsday Engine" text superimposed.
 *
 * @param origin  Screen-space coordinate origin (top left).
 * @param size  Screen-space dimensions of the cursor in pixels.
 * @param alpha  Alpha level to use when drawing the background.
 */
void UI_DrawDDBackground(const Point2Raw* origin, const Size2Raw* size, float alpha);

/**
 * Draw the mouse cursor at the given co-ordinates.
 *
 * @param origin  Screen-space coordinate origin (top left).
 * @param size  Screen-space dimensions of the cursor in pixels.
 */
void UI_DrawMouse(const Point2Raw* origin, const Size2Raw* size);
void UI_DrawTitle(ui_page_t* page);
void UI_DrawTitleEx(char* text, int height, float alpha);
void UI_MixColors(ui_color_t* a, ui_color_t* b, ui_color_t* dest, float amount);
void UI_SetColorA(ui_color_t* color, float alpha);
void UI_SetColor(ui_color_t* color);
void UI_Line(const Point2Raw* start, const Point2Raw* end, ui_color_t* startColor, ui_color_t* endColor, float startAlpha, float endAlpha);
void UI_Shade(const Point2Raw* origin, const Size2Raw* size, int border, ui_color_t* main, ui_color_t* secondary, float alpha, float bottom_alpha);
void UI_Gradient(const Point2Raw* origin, const Size2Raw* size, ui_color_t* top, ui_color_t* bottom, float topAlpha, float bottomAlpha);
void UI_GradientEx(const Point2Raw* origin, const Size2Raw* size, int border, ui_color_t* top, ui_color_t* bottom, float topAlpha, float bottomAlpha);
void UI_HorizGradient(const Point2Raw* origin, const Size2Raw* size, ui_color_t* left, ui_color_t* right, float leftAlpha, float rightAlpha);
void UI_DrawRect(const Point2Raw* origin, const Size2Raw* size, int brd, ui_color_t* color, float alpha);
void UI_DrawRectEx(const Point2Raw* origin, const Size2Raw* size, int brd, boolean filled, ui_color_t* top, ui_color_t* bottom, float alpha, float bottomAlpha);
void UI_DrawTriangle(const Point2Raw* origin, int radius, ui_color_t* hi, ui_color_t* med, ui_color_t* low, float alpha);

/**
 * A horizontal triangle, pointing left or right. Positive radius
 * means left.
 */
void UI_DrawHorizTriangle(const Point2Raw* origin, int radius, ui_color_t* hi, ui_color_t* med, ui_color_t *low, float alpha);

void UI_DrawButton(const Point2Raw* origin, const Size2Raw* size, int border, float alpha, ui_color_t* background, boolean down, boolean disabled, int arrow);

/// Draw shadowed text.
void UI_TextOutEx(const char* text, const Point2Raw* origin, ui_color_t* color, float alpha);
void UI_TextOutEx2(const char* text, const Point2Raw* origin, ui_color_t* color, float alpha, int alignFlags, short textFlags);
int UI_TextOutWrap(const char* text, const Point2Raw* origin, const Size2Raw* size);

/**
 * Draw line-wrapped text inside a box. Returns the Y coordinate of the
 * last word.
 */
int UI_TextOutWrapEx(const char* text, const Point2Raw* origin, const Size2Raw* size, ui_color_t* color, float alpha);
void UI_DrawHelpBox(const Point2Raw* origin, const Size2Raw* size, float alpha, char* text);

#ifdef __cplusplus
}
#endif

#endif /* LIBDENG_UI_MAIN_H */
