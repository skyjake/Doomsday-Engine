/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * ui_main.h: Graphical User Interface
 */

#ifndef __DOOMSDAY_UI_H__
#define __DOOMSDAY_UI_H__

#include "con_decl.h"

enum                               // Colors.
{
    UIC_TEXT,
    UIC_TITLE,
    UIC_SHADOW,
    UIC_BG_LIGHT,
    UIC_BG_MEDIUM,
    UIC_BG_DARK,
    UIC_BRD_HI,
    UIC_BRD_MED,
    UIC_BRD_LOW,
    UIC_HELP,
    NUM_UI_COLORS
};

#define IS_ACTKEY(x)    (x == ' ' || x == DDKEY_RETURN)

typedef enum {
    UI_NONE,
    UI_TEXT,
    UI_BOX,
    UI_FOCUSBOX,                   // Can receive focus.
    UI_BUTTON,
    UI_BUTTON2,                    // Staydown/2-state button.
    UI_BUTTON2EX,                  // Staydown/2-state with additional data.
    UI_EDIT,
    UI_LIST,
    UI_SLIDER,
    UI_META                        // Special: affects all objects up to the next meta.
} ui_obtype_e;

// Standard dimensions.
#define UI_WIDTH        (1000.0f)
#define UI_HEIGHT       (1000.0f)
#define UI_BORDER       (UI_WIDTH/120) // All borders are this wide.
#define UI_SHADOW_OFFSET (MIN_OF(3, UI_WIDTH/320))
#define UI_BUTTON_BORDER (UI_BORDER)
#define UI_BAR_WDH      (UI_BORDER * 3)
#define UI_BAR_BORDER   (UI_BORDER / 2)
#define UI_BAR_BUTTON_BORDER (3 * UI_BAR_BORDER / 2)
#define UI_MAX_COLUMNS  10         // Maximum columns for list box.

// Object flags.
#define UIF_HIDDEN      0x1
#define UIF_DISABLED    0x2         // Can't be interacted with.
#define UIF_PAUSED      0x4         // Ticker not called.
#define UIF_CLICKED     0x8
#define UIF_ACTIVE      0x10        // Object active.
#define UIF_FOCUS       0x20        // Has focus.
#define UIF_NO_FOCUS    0x40        // Can't receive focus.
#define UIF_DEFAULT     0x80        // Has focus by default.
#define UIF_LEFT_ALIGN  0x100
#define UIF_FADE_AWAY   0x200       // Fade UI away while the control is active.
#define UIF_NEVER_FADE  0x400
#define UIF_ID0         0x10000000
#define UIF_ID1         0x20000000
#define UIF_ID2         0x40000000
#define UIF_ID3         0x80000000

// Special group: no group.
#define UIG_NONE        -1

// Flag group modes (for UI_FlagGroup).
enum {
    UIFG_CLEAR,
    UIFG_SET,
    UIFG_XOR
};

// Button arrows.
enum {
    UIBA_NONE,
    UIBA_UP,
    UIBA_DOWN,
    UIBA_LEFT,
    UIBA_RIGHT
};

// Types.
typedef struct {
    float           red, green, blue;
} ui_color_t;

typedef struct ui_object_s {
    ui_obtype_e     type;          // Type of the object.
    int             group;
    int             flags;
    int             relx, rely, relw, relh; // Relative placement.
    char            text[256];     // Used in various ways.
    void            (*drawer) (struct ui_object_s *);
    int             (*responder) (struct ui_object_s *, ddevent_t *);
    void            (*ticker) (struct ui_object_s *);
    void            (*action) (struct ui_object_s *);
    void           *data;          // Pointer to extra data.
    int             data2;         // Extra numerical data.
    int             timer;
    int             x, y, w, h;    // Position and dimensions, auto-inited.
} ui_object_t;

// UI Pages consist of one or more controls.
typedef struct ui_page_s {
    ui_object_t    *objects;       // List of objects, UI_NONE terminates.
    char            title[128];    // Title for the screen.
    boolean         background;    // Draw the background?
    boolean         header;        // Draw the header bar?
    int             focus;         // Index of the focus object.
    int             capture;       // Index of the capture object.
    void            (*drawer) (struct ui_page_s *);
    int             (*responder) (struct ui_page_s *, ddevent_t *);
    void            (*ticker) (struct ui_page_s *);
    struct ui_page_s *previous;    // Pointer to the previous page, if any.
    int             timer;
    int             count;         // Object count, no need to initialize.
} ui_page_t;

typedef struct {
    void           *data;
    const char     *yes;
    const char     *no;
} uidata_button_t;

typedef struct {
    char           *ptr;           // Text to modify.
    int             maxlen;        // Maximum allowed length.
    const void     *data;
    uint            cp;            // Cursor position.
} uidata_edit_t;

typedef struct {
    char            text[256];
    int             data;
    int             data2;
} uidata_listitem_t;

typedef struct {
    void           *items;
    int             count;         // Number of items.
    const void     *data;
    int             selection;     // Selected item (-1 if none).
    int             first;         // First visible item.
    int             itemhgt;       // Height of each item (0 = fonthgt).
    int             numvis;        // Number of visible items (updated at SetPage).
    byte            button[3];     // Button states (0=normal, 1=down).
    int             column[UI_MAX_COLUMNS]; // Column offsets (real coords).
} uidata_list_t;

typedef struct {
    float           min, max;
    float           value;
    float           step;          // Button step.
    boolean         floatmode;     // Otherwise only integers are allowed.
    const void     *data;
    char           *zerotext;
    byte            button[3];     // Button states (0=normal, 1=down).
} uidata_slider_t;

void            UI_Register(void);

// Functions.
void            UI_Init(boolean halttime, boolean tckui, boolean tckframe,
                        boolean drwgame, boolean noescape);
void            UI_End(void);
boolean         UI_IsActive(void);
ui_page_t      *UI_CurrentPage(void);
void            UI_SetAlpha(float alpha);
float           UI_Alpha(void);
ui_color_t     *UI_Color(uint id);
int             UI_FontHeight(void);
void            UI_LoadTextures(void);
void            UI_ClearTextures(void);
void            UI_InitPage(ui_page_t *page, ui_object_t *objects);
void            UI_SetPage(ui_page_t *page);
int             UI_Responder(ddevent_t *ev);
void            UI_Ticker(timespan_t time);
void            UI_Drawer(void);
int             UI_CountObjects(ui_object_t *list);
void            UI_FlagGroup(ui_object_t *list, int group, int flags, int set);
ui_object_t    *UI_FindObject(ui_object_t *list, int group, int flags);
void            UI_Focus(ui_object_t *ob);
void            UI_Capture(ui_object_t *ob);

// Default callbacks.
int             UIPage_Responder(ui_page_t *page, ddevent_t *ev);
void            UIPage_Ticker(ui_page_t *page);
void            UIPage_Drawer(ui_page_t *page);
void            UIFrame_Drawer(ui_object_t *ob);
void            UIText_Drawer(ui_object_t *ob);
void            UIText_BrightDrawer(ui_object_t *ob);
int             UIButton_Responder(ui_object_t *ob, ddevent_t *ev);
void            UIButton_Drawer(ui_object_t *ob);
int             UIEdit_Responder(ui_object_t *ob, ddevent_t *ev);
void            UIEdit_Drawer(ui_object_t *ob);
int             UIList_Responder(ui_object_t *ob, ddevent_t *ev);
void            UIList_Ticker(ui_object_t *ob);
void            UIList_Drawer(ui_object_t *ob);
int             UISlider_Responder(ui_object_t *ob, ddevent_t *ev);
void            UISlider_Ticker(ui_object_t *ob);
void            UISlider_Drawer(ui_object_t *ob);

// Helpers.
int             UI_ScreenX(int relx);
int             UI_ScreenY(int rely);
int             UI_ScreenW(int relw);
int             UI_ScreenH(int relh);
void            UI_InitColumns(ui_object_t *ob);
int             UI_MouseInsideBox(int x, int y, int w, int h);
int             UI_MouseInside(ui_object_t *ob);
int             UI_MouseResting(ui_page_t *page);
int             UI_ListFindItem(ui_object_t *ob, int data_value);
void            UI_DrawLogo(int x, int y, int w, int h);
void            UI_DrawDDBackground(float x, float y, float w, float h,
                                    float alpha);
void            UI_DrawMouse(int x, int y, int w, int h);
void            UI_DrawTitle(ui_page_t *page);
void            UI_DrawTitleEx(char *text, int height, float alpha);
void            UI_MixColors(ui_color_t *a, ui_color_t *b, ui_color_t *dest,
                             float amount);
void            UI_SetColorA(ui_color_t *color, float alpha);
void            UI_SetColor(ui_color_t *color);
void            UI_Line(int x1, int y1, int x2, int y2, ui_color_t *start,
                        ui_color_t *end, float startAlpha, float endAlpha);
void            UI_Shade(int x, int y, int w, int h, int border,
                         ui_color_t *main, ui_color_t *secondary,
                         float alpha, float bottom_alpha);
void            UI_Gradient(int x, int y, int w, int h, ui_color_t *top,
                            ui_color_t *bottom, float topAlpha,
                            float bottomAlpha);
void            UI_GradientEx(int x, int y, int w, int h, int border,
                              ui_color_t *top, ui_color_t *bottom,
                              float topAlpha, float bottomAlpha);
void            UI_HorizGradient(int x, int y, int w, int h, ui_color_t *left,
                                 ui_color_t *right, float leftAlpha,
                                 float rightAlpha);
void            UI_DrawRect(int x, int y, int w, int h, int brd,
                            ui_color_t *color, float alpha);
void            UI_DrawRectEx(int x, int y, int w, int h, int brd,
                              boolean filled, ui_color_t *top,
                              ui_color_t *bottom, float alpha,
                              float bottomAlpha);
void            UI_DrawTriangle(int x, int y, int radius, ui_color_t *hi,
                                ui_color_t *med, ui_color_t *low,
                                float alpha);
void            UI_DrawButton(int x, int y, int w, int h, int brd, float alpha,
                              ui_color_t *background, boolean down,
                              boolean disabled, int arrow);
void            UI_TextOut(const char *text, int x, int y);
void            UI_TextOutEx(const char *text, int x, int y, int horiz_center,
                             int vert_center, ui_color_t *color, float alpha);
int             UI_TextOutWrap(const char *text, int x, int y, int w, int h);
int             UI_TextOutWrapEx(const char *text, int x, int y, int w, int h,
                                 ui_color_t *color, float alpha);
void            UI_DrawHelpBox(int x, int y, int w, int h, float alpha,
                               char *text);

#endif
