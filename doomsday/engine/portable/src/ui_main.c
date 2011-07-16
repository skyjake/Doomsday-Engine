/**\file ui_main.c
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
 * Graphical User Interface
 *
 * Has ties to the console routines.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_ui.h"
#include "de_misc.h"

#include "image.h"
#include "texturecontent.h"
#include "rend_console.h" // \todo Move Rend_ConsoleUpdateTitle somewhere more suitable.

// MACROS ------------------------------------------------------------------

#define SCROLL_TIME     3
#define UICURSORWIDTH   32
#define UICURSORHEIGHT  64

enum {
    UITEX_MOUSE,
    UITEX_CORNER,
    UITEX_FILL,
    UITEX_SHADE,
    UITEX_HINT,
    UITEX_LOGO,
    UITEX_BACKGROUND,
    NUM_UITEXTURES
};

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(UIColor);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int listItemHeight(uidata_list_t* listdata);
static int listButtonHeight(ui_object_t* ob);
static int listThumbPos(ui_object_t* ob);
static void strCpyLen(char* dest, const char* src, int maxWidth);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int glMaxTexSize;
extern int gameDrawHUD;
extern boolean stopTime;
extern boolean tickUI;
extern boolean drawGame;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean uiActive = false; /// The user interface is active.
static boolean uiShowMouse = true;
static ui_page_t* uiCurrentPage = 0; /// Currently active page.
static int uiFontHgt; /// Height of the UI font.
static DGLuint uiTextures[NUM_UITEXTURES]; /// Cursor texture.
static int uiCX, uiCY; /// Cursor position.
static int uiRestCX, uiRestCY;
static uint uiRestStart; /// Start time of current resting.
static uint uiRestTime = TICSPERSEC / 2; /// 500 ms.
static int uiRestOffsetLimit = 2;
static int uiMoved; /// True if the mouse has been moved.
static float uiAlpha = 1.0; /// Main alpha for the entire UI.
static float uiTargetAlpha = 1.0; // Target alpha for the entire UI.

/// The game view should be drawn while the UI active.
static boolean uiDrawGame = false;

static float uiCursorWidthMul = 0.75;
static float uiCursorHeightMul = 0.75;

/// Modify these colors to change the look of the UI.
static ui_color_t ui_colors[NUM_UI_COLORS] = {
    /* UIC_TEXT */      { .85f, .87f, 1 },
    /* UIC_TITLE */     { 1, 1, 1 },
    /* UIC_SHADOW */    { 0, 0, 0 },
    /* UIC_BG_LIGHT */  { .18f, .18f, .22f },
    /* UIC_BG_MEDIUM */ { .4f, .4f, .52f },
    /* UIC_BG_DARK */   { .28f, .28f, .33f },
    /* UIC_BRD_HI */    { 1, 1, 1 },
    /* UIC_BRD_MED */   { 0, 0, 0 },
    /* UIC_BRD_LOW */   { .25f, .25f, .55f },
    /* UIC_HELP */      { .4f, .4f, .52f }
};

static boolean allowEscape; /// Allow the user to exit a ui page using the escape key.

// CODE --------------------------------------------------------------------

void UI_Register(void)
{
    // Cvars
    C_VAR_FLOAT("ui-cursor-width", &uiCursorWidthMul, 0, 0.5f, 1.5f);
    C_VAR_FLOAT("ui-cursor-height", &uiCursorHeightMul, 0, 0.5f, 1.5f);

    // Ccmds
    C_CMD_FLAGS("uicolor", "sfff", UIColor, CMDF_NO_DEDICATED);

    CP_Register();
    Fonts_Register();
}

void UI_PageInit(boolean halttime, boolean tckui, boolean tckframe, boolean drwgame, boolean noescape)
{
    if(uiActive)
        return;

    uiActive = true;
    B_ActivateContext(B_ContextByName(UI_BINDING_CONTEXT_NAME), true);

    // Restore full alpha.
    uiAlpha = uiTargetAlpha = 1.0;

    // Adjust the engine state
    stopTime = halttime;
    tickUI = tckui;
    tickFrame = tckframe;
    uiDrawGame = drawGame = drwgame;
    // Advise the game not to draw any HUD displays
    gameDrawHUD = false;
    I_SetUIMouseMode(true);

    // Change font.
    FR_SetFont(glFontVariable[GLFS_NORMAL]);
    FR_LoadDefaultAttrib();
    uiFontHgt = FR_SingleLineHeight("UI");

    // Should the mouse cursor be visible?
    uiShowMouse = !ArgExists("-nomouse");

    // Allow use of the escape key to exit the ui?
    allowEscape = !noescape;

    // Init cursor to the center of the screen.
    uiCX = theWindow->width / 2;
    uiCY = theWindow->height / 2;
    uiMoved = false;
}

void UI_End(void)
{
    ddevent_t rel;

    if(!uiActive)
        return;
    uiActive = false;

    B_ActivateContext(B_ContextByName(UI_BINDING_CONTEXT_NAME), false);

    // Restore full alpha.
    uiAlpha = uiTargetAlpha = 1.0;

    // Update the secondary title and the game status.
    Rend_ConsoleUpdateTitle();

    // Restore the engine state
    tickFrame = true;
    stopTime = false;
    tickUI = false;
    drawGame = true;
    // Advise the game it can now draw HUD displays again.
    gameDrawHUD = true;

    I_SetUIMouseMode(false);

    // Inform everybody that the shift key was (possibly) released while
    // the UI was eating all the input events.
    if(!shiftDown)
    {
        rel.device = IDEV_KEYBOARD;
        rel.type = E_TOGGLE;
        rel.toggle.state = ETOG_UP;
        rel.toggle.id = DDKEY_RSHIFT;
        DD_PostEvent(&rel);
    }
}

boolean UI_IsActive(void)
{
    return uiActive;
}

int UI_FontHeight(void)
{
    return uiFontHgt;
}

ui_color_t* UI_Color(uint id)
{
    if(id >= NUM_UI_COLORS)
        return NULL;
    return &ui_colors[id];
}

void UI_SetAlpha(float alpha)
{
    // The UI's alpha will start moving towards this target value.
    uiTargetAlpha = alpha;
}

float UI_Alpha(void)
{
    return uiAlpha;
}

ui_page_t* UI_CurrentPage(void)
{
    if(uiActive)
        return uiCurrentPage;
    return NULL;
}

void UI_LoadTextures(void)
{
    const char* picNames[NUM_UITEXTURES] = {
        "Mouse",
        "BoxCorner",
        "BoxFill",
        "BoxShade",
        "Hint",
        "Logo",
        "Background",
    };
    int i;

    if(isDedicated)
        return;

    for(i = 0; i < NUM_UITEXTURES; ++i)
    {
        image_t image;
        if(uiTextures[i])
            continue;
        
        if(GL_LoadExtTexture(&image, picNames[i], (i == UITEX_BACKGROUND? LGM_GRAYSCALE : LGM_NORMAL)))
        {   // Loaded successfully and converted accordingly.
            // Upload the image to GL.
            uiTextures[i] = GL_NewTextureWithParams2(
                ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                  image.pixelSize == 3 ? DGL_RGB :
                  image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
                image.width, image.height, image.pixels,
                TXCF_NO_COMPRESSION, 0, GL_LINEAR, GL_LINEAR,
                0 /*no anisotropy*/, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

            GL_DestroyImagePixels(&image);
        }
        else
            uiTextures[i] = 0;
    }
}

void UI_ClearTextures(void)
{
    glDeleteTextures(NUM_UITEXTURES, (const GLuint*) uiTextures);
    memset(uiTextures, 0, sizeof(uiTextures));
}

void UI_DefaultFocus(ui_page_t* page)
{
    ui_object_t* deffocus = NULL;
    uint i;

    for(i = 0; i < page->count; ++i)
    {
        page->_objects[i].flags &= ~UIF_FOCUS;
        if(page->_objects[i].flags & UIF_DEFAULT)
            deffocus = page->_objects + i;
    }

    if(deffocus)
    {
        page->focus = deffocus - page->_objects;
        deffocus->flags |= UIF_FOCUS;
        return;
    }

    // Find an object for focus.
    for(i = 0; i < page->count; ++i)
    {
        if(!(page->_objects[i].flags & UIF_NO_FOCUS))
        {
            // Set default focus.
            page->focus = i;
            page->_objects[i].flags |= UIF_FOCUS;
            break;
        }
    }
}

void UI_InitPage(ui_page_t* page, ui_object_t* objects)
{
    ui_object_t meta;
    uint i;

    memset(&meta, 0, sizeof(meta));
    memset(page, 0, sizeof(*page));
    page->_objects = objects;
    page->capture = -1; /// No capture.
    page->focus = -1;
    page->responder = UIPage_Responder;
    page->drawer = UIPage_Drawer;
    page->flags.showBackground = true; /// Draw background by default.
    page->ticker = UIPage_Ticker;
    page->count = UI_CountObjects(objects);
    for(i = 0; i < page->count; ++i)
    {
        if(objects[i].type == UI_TEXT || objects[i].type == UI_BOX ||
           objects[i].type == UI_META)
        {
            objects[i].flags |= UIF_NO_FOCUS;
        }
        // Reset timer.
        objects[i].timer = 0;
    }
    UI_DefaultFocus(page);
    // Meta effects.
    for(i = 0, meta.type = 0; i < page->count; ++i)
    {
        if(!meta.type && objects[i].type != UI_META)
            continue;
        if(objects[i].type == UI_META)
        {
            // This will be the meta for now.
            memcpy(&meta, objects + i, sizeof(meta));
            // Neutralize the actual object.
            objects[i].group = UIG_NONE;
            objects[i].flags |= UIF_HIDDEN;
            objects[i].relx = objects[i].rely = 0;
            objects[i].relw = objects[i].relh = 0;
            continue;
        }
        // Apply the meta.
        if(meta.group != UIG_NONE)
            objects[i].group = meta.group;
        objects[i].relx += meta.relx;
        objects[i].rely += meta.rely;
        objects[i].relw += meta.relw;
        objects[i].relh += meta.relh;
    }
}

int UI_AvailableWidth(void)
{
    return theWindow->width - UI_BORDER * 4;
}

int UI_AvailableHeight(void)
{
    return theWindow->height - UI_BORDER * 4;
}

int UI_ScreenX(int relx)
{
    return UI_BORDER * 2 + ((relx / UI_WIDTH) * UI_AvailableWidth());
}

int UI_ScreenY(int rely)
{
    return UI_BORDER * 2 + ((rely / UI_HEIGHT) * UI_AvailableHeight());
}

int UI_ScreenW(int relw)
{
    return (relw / UI_WIDTH) * UI_AvailableWidth();
}

int UI_ScreenH(int relh)
{
    return (relh / UI_HEIGHT) * UI_AvailableHeight();
}

void UI_SetPage(ui_page_t* page)
{
    ui_object_t* ob;
    uint i;

    uiCurrentPage = page;
    if(!page)
        return;
    // Init objects.
    for(i = 0, ob = page->_objects; i < page->count; ++i, ob++)
    {
        // Calculate real coordinates.
        ob->x = UI_ScreenX(ob->relx);
        ob->w = UI_ScreenW(ob->relw);
        ob->y = UI_ScreenY(ob->rely);
        ob->h = UI_ScreenH(ob->relh);

        // Update objects on page
        if(ob->type == UI_EDIT)
        {
            // Update edit box text.
            memset(ob->text, 0, sizeof(ob->text));
            strncpy(ob->text, ((uidata_edit_t*) ob->data)->ptr, 255);
        }
        else if(ob->type == UI_BUTTON2)
        {
            // Stay-down button state.
            if(*(char*) ob->data)
                ob->flags |= UIF_ACTIVE;
            else
                ob->flags &= ~UIF_ACTIVE;
        }
        else if(ob->type == UI_BUTTON2EX)
        {
            // Stay-down button state, with extended data.
            if(*(char*) ((uidata_button_t*)ob->data)->data)
                ob->flags |= UIF_ACTIVE;
            else
                ob->flags &= ~UIF_ACTIVE;
        }
        else if(ob->type == UI_LIST)
        {
            // List box number of visible items.
            uidata_list_t* dat = ob->data;

            dat->numvis = (ob->h - 2 * UI_BORDER) / listItemHeight(dat);
            if(dat->selection >= 0)
            {
                if(dat->selection < dat->first)
                    dat->first = dat->selection;
                if(dat->selection > dat->first + dat->numvis - 1)
                    dat->first = dat->selection - dat->numvis + 1;
            }
            UI_InitColumns(ob);
        }
    }
    // The mouse has not yet been moved on this page.
    uiMoved = false;
}

int UI_Responder(ddevent_t* ev)
{
    if(!uiActive)
        return false;
    if(!uiCurrentPage)
        return false;

    if(IS_MOUSE_MOTION(ev))
    {
        uiMoved = true;

        if(ev->axis.id == 0) // xaxis.
        {
            uiCX += ev->axis.pos;
            if(uiCX < 0)
                uiCX = 0;
            if(uiCX >= theWindow->width)
                uiCX = theWindow->width - 1;
        }
        else if(ev->axis.id == 1) // yaxis.
        {
            uiCY += ev->axis.pos;
            if(uiCY < 0)
                uiCY = 0;
            if(uiCY >= theWindow->height)
                uiCY = theWindow->height - 1;
        }
    }

    // Call the page's responder.
    uiCurrentPage->responder(uiCurrentPage, ev);
    // If the UI is active, all events are eaten by it.

    return true;
}

void UI_Ticker(timespan_t time)
{
#define UIALPHA_FADE_STEP .07

    static trigger_t fixed = { 1 / 35.0, 0 };
    float diff = 0;

    if(!uiActive)
        return;
    if(!uiCurrentPage)
        return;

    if(!M_RunTrigger(&fixed, time))
        return;

    // Move towards the target alpha level for the entire UI.
    diff = uiTargetAlpha - uiAlpha;
    if(fabs(diff) > UIALPHA_FADE_STEP)
    {
        uiAlpha += UIALPHA_FADE_STEP * (diff > 0? 1 : -1);
    }
    else
    {
        uiAlpha = uiTargetAlpha;
    }

    if(!uiDrawGame)
    {
        // By default, the game is not visible, but since the alpha is not
        // fully opaque, it must be shown anyway.
        drawGame = (uiAlpha < 1.0);
    }

    // Call the active page's ticker.
    uiCurrentPage->ticker(uiCurrentPage);

#undef UIALPHA_FADE_STEP
}

void UI_Drawer(void)
{
    if(!uiActive || !uiCurrentPage)
        return;

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    // Call the active page's drawer.
    uiCurrentPage->drawer(uiCurrentPage);

    // Draw mouse cursor?
    if(uiShowMouse)
    {
        float width, height, scale;

        if(theWindow->width >= theWindow->height)
            scale = (theWindow->width  / UI_WIDTH)  * (theWindow->height / (float) theWindow->width);
        else
            scale = (theWindow->height / UI_HEIGHT) * (theWindow->width  / (float) theWindow->height);

        width = UICURSORWIDTH * scale * uiCursorWidthMul;
        height = UICURSORHEIGHT * scale * uiCursorHeightMul;

        UI_DrawMouse(uiCX - 1, uiCY - 1, width, height);
    }

    // Restore the original matrices.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

int UI_CountObjects(ui_object_t* list)
{
    int count;
    for(count = 0; list->type != UI_NONE; list++, count++);
    return count;
}

void UI_FlagGroup(ui_object_t* list, int group, int flags, int set)
{
    for(; list->type; list++)
    {
        if(list->group != group)
            continue;

        switch(set)
        {
        case UIFG_CLEAR:
            list->flags &= ~flags;
            break;

        case UIFG_SET:
            list->flags |= flags;
            break;

        case UIFG_XOR:
            list->flags ^= flags;
            break;
        default:
            Con_Error("UI_FlagGroup: Unknown flag bit op %i\n", set);
            break;
        }
    }
}

ui_object_t* UI_FindObject(ui_object_t* list, int group, int flags)
{
    for(; list->type; list++)
        if(list->group == group && (list->flags & flags) == flags)
            return list;
    return NULL;
}

void UI_MouseFocus(void)
{
    ui_object_t* ob;
    uint i;

    for(i = 0, ob = uiCurrentPage->_objects; i < uiCurrentPage->count; ++i, ob++)
        if(!(ob->flags & UIF_NO_FOCUS) && UI_MouseInside(ob))
        {
            UI_Focus(ob);
            break;
        }
}

void UI_Focus(ui_object_t* ob)
{
    uint i;

    if(!ob)
        Con_Error("UI_Focus: Tried to set focus on NULL.\n");

    // Can the object receive focus?
    if(ob->flags & UIF_NO_FOCUS)
        return;

    uiCurrentPage->focus = ob - uiCurrentPage->_objects;
    for(i = 0; i < uiCurrentPage->count; ++i)
    {
        if(i == uiCurrentPage->focus)
            uiCurrentPage->_objects[i].flags |= UIF_FOCUS;
        else
            uiCurrentPage->_objects[i].flags &= ~UIF_FOCUS;
    }
}

void UI_Capture(ui_object_t* ob)
{
    if(!ob)
    {
        // End capture.
        uiCurrentPage->capture = -1;
        return;
    }
    if(!ob->responder)
        return;

    // Set the capture object.
    uiCurrentPage->capture = ob - uiCurrentPage->_objects;
    // Set focus.
    UI_Focus(ob);
}

//---------------------------------------------------------------------------
// Default Callback Functions
//---------------------------------------------------------------------------

int UIPage_Responder(ui_page_t* page, ddevent_t* ev)
{
    ui_object_t* ob;
    ddevent_t translated;
    uint i;

    // Translate mouse wheel?
    if(IS_MOUSE_DOWN(ev))
    {
        if(ev->toggle.id == DD_MWHEEL_UP || ev->toggle.id == DD_MWHEEL_DOWN)
        {
            UI_MouseFocus();
            translated.device = IDEV_KEYBOARD;
            translated.type = E_TOGGLE;
            translated.toggle.state = ETOG_DOWN;
            translated.toggle.id = (ev->toggle.id == DD_MWHEEL_UP ? DDKEY_UPARROW : DDKEY_DOWNARROW);
            ev = &translated;
        }
    }

    if(page->capture >= 0)
    {
        // There is an object that has captured input.
        ob = page->_objects + page->capture;
        // Capture objects must have a responder!
        // This object gets to decide what happens.
        return ob->responder(ob, ev);
    }

    // Check for Esc key.
    if(IS_KEY_PRESS(ev))
    {
        // We won't accept repeats with Esc.
        if(ev->toggle.id == DDKEY_ESCAPE && ev->toggle.state == ETOG_DOWN &&
           allowEscape)
        {
            UI_SetPage(page->previous);
            // If we have no more a page, disactive UI.
            if(!uiCurrentPage)
                UI_End();
            return true; // The event was used.
        }

        // Tab is used for navigation.
        if(ev->toggle.id == DDKEY_TAB)
        {
            uint k;
            // Remove the focus flag from the current focus object.
            page->_objects[page->focus].flags &= ~UIF_FOCUS;
            // Move focus.
            k = 0;
            do
            {
                page->focus += shiftDown ? -1 : 1;
                // Check range.
                if(page->focus < 0)
                    page->focus = page->count - 1;
                else if((unsigned) page->focus >= page->count)
                    page->focus = 0;
            } while(++k < page->count && (page->_objects[page->focus].flags & (UIF_DISABLED | UIF_NO_FOCUS | UIF_HIDDEN)));

            // Flag the new focus object.
            page->_objects[page->focus].flags |= UIF_FOCUS;
            return true; // The event was used.
        }
    }

    // Call responders until someone uses the event.
    // We start with the focus object.
    for(i = 0; i < page->count; ++i)
    {
        int k;

        // Determine the index of the object to process.
        k = page->focus + i;
        // Wrap around.
        if(k < 0)
            k += page->count;
        if((unsigned)k >= page->count)
            k -= page->count;
        ob = page->_objects + k;
        // Check the flags of this object.
        if((ob->flags & UIF_HIDDEN) || (ob->flags & UIF_DISABLED))
            continue; // These flags prevent response.

        if(!ob->responder)
            continue; // Must have a responder.
        if(ob->responder(ob, ev))
        {   // The event was used by this object.
            UI_Focus(ob); // Move focus to it.
            return true;
        }
    }

    if(uiTargetAlpha < 1.0 && IS_MOUSE_DOWN(ev))
    {
        // When the UI the faded, an unhandled click brings back the UI.
        UI_DefaultFocus(page);
    }

    // We didn't use the event.
    return false;
}

void UIPage_Ticker(ui_page_t* page)
{
    boolean fadedAway = false;
    ui_object_t* ob;
    uint i;

    // Call the ticker of each object, unless they're hidden or paused.
    for(i = 0, ob = page->_objects; i < page->count; ++i, ob++)
    {
        if((ob->flags & UIF_PAUSED) || (ob->flags & UIF_HIDDEN))
            continue;

        // Fadeaway objects cause the UI to fade away when the mouse is over the control.
        if((ob->flags & UIF_FOCUS) && (ob->flags & UIF_FADE_AWAY))
        {
            UI_SetAlpha(0);
            fadedAway = true;
        }

        if(ob->ticker)
            ob->ticker(ob);
        // Advance object timer.
        ob->timer++;
    }

    if(!fadedAway)
    {
        UI_SetAlpha(1.0);
    }

    page->_timer++;

    // Check mouse resting.
    if(abs(uiCX - uiRestCX) > uiRestOffsetLimit || abs(uiCY - uiRestCY) > uiRestOffsetLimit)
    {
        // Restart resting period.
        uiRestCX = uiCX;
        uiRestCY = uiCY;
        uiRestStart = page->_timer;
    }
}

void UIPage_Drawer(ui_page_t* page)
{
    ui_object_t* ob;
    ui_color_t focuscol;
    float t;
    uint i;

    // Draw background?
    if(page->flags.showBackground)
        UI_DrawDDBackground(0, 0, theWindow->width, theWindow->height, uiAlpha);

    // Draw each object, unless they're hidden.
    for(i = 0, ob = page->_objects; i < page->count; ++i, ob++)
    {
        float currentUIAlpha = uiAlpha;

        if((ob->flags & UIF_HIDDEN) || !ob->drawer)
            continue;

        if(ob->flags & UIF_FADE_AWAY)
        {
            if(ob->flags & UIF_FOCUS)
            {
                // The focused object must not fade away completely.
                uiAlpha = MAX_OF(currentUIAlpha, 0.75);
            }
            else
            {
                // Other fadeaway objects remain slightly visible.
                uiAlpha = MAX_OF(currentUIAlpha, 0.333);
            }
        }
        if(ob->flags & UIF_NEVER_FADE)
        {
            uiAlpha = 1.0;
        }

        // Draw the object itself.
        ob->drawer(ob);

        if((ob->flags & UIF_FOCUS) &&
            (ob->type != UI_EDIT || !(ob->flags & UIF_ACTIVE)))
        {
            t = (1 + sin(page->_timer / (float) TICSPERSEC * 1.5f * PI)) / 2;
            UI_MixColors(UI_Color(UIC_BRD_LOW), UI_Color(UIC_BRD_HI), &focuscol, t);
            glEnable(GL_TEXTURE_2D);
            UI_Shade(ob->x, ob->y, ob->w, ob->h, UI_BORDER, UI_Color(UIC_BRD_LOW), UI_Color(UIC_BRD_LOW), .2f + t * .3f, -1);
            GL_BlendMode(BM_ADD);
            // Draw a focus rectangle.
            UI_DrawRect(ob->x - 1, ob->y - 1, ob->w + 2, ob->h + 2, UI_BORDER, &focuscol, 1);
            GL_BlendMode(BM_NORMAL);
            glDisable(GL_TEXTURE_2D);
        }

        // Restore the correct UI alpha.
        uiAlpha = currentUIAlpha;
    }
}

void UIFrame_Drawer(ui_object_t* ob)
{
    int b = UI_BORDER;
    glEnable(GL_TEXTURE_2D);
    UI_GradientEx(ob->x, ob->y, ob->w, ob->h, b, UI_Color(UIC_BG_MEDIUM), 0, .6f, 0);
    UI_DrawRect(ob->x, ob->y, ob->w, ob->h, b, UI_Color(UIC_BRD_HI), 1);
    glDisable(GL_TEXTURE_2D);
}

void UIText_Drawer(ui_object_t* ob)
{
    glEnable(GL_TEXTURE_2D);
    FR_SetFont(glFontVariable[GLFS_NORMAL]);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    UI_TextOutEx2(ob->text, ob->x, ob->y + ob->h / 2, UI_Color(UIC_TEXT), ob->flags & UIF_DISABLED ? .2f : 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    glDisable(GL_TEXTURE_2D);
}

void UIText_BrightDrawer(ui_object_t* ob)
{
    glEnable(GL_TEXTURE_2D);
    FR_SetFont(glFontVariable[GLFS_NORMAL]);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    UI_TextOutEx2(ob->text, ob->x, ob->y + ob->h / 2, UI_Color(UIC_TITLE), ob->flags & UIF_DISABLED ? .2f : 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    glDisable(GL_TEXTURE_2D);
}

int UIButton_Responder(ui_object_t* ob, ddevent_t* ev)
{
    if(ob->flags & UIF_CLICKED)
    {
        if((ev->device == IDEV_KEYBOARD || ev->device == IDEV_MOUSE) && IS_TOGGLE_UP(ev))
        {
            UI_Capture(0);
            ob->flags &= ~UIF_CLICKED;
            if(UI_MouseInside(ob) || ev->device == IDEV_KEYBOARD)
            {
                // Activate?
                if(ob->action)
                    ob->action(ob);
            }
            return true;
        }
    }
    else if(IS_TOGGLE_DOWN(ev) &&
            ((ev->device == IDEV_MOUSE && UI_MouseInside(ob)) ||
             (ev->device == IDEV_KEYBOARD && IS_ACTKEY(ev->toggle.id))))
    {
        if(ob->type == UI_BUTTON)
        {
            // Capture input.
            UI_Capture(ob);
            ob->flags |= UIF_CLICKED;
        }
        else
        {
            // Stay-down buttons change state.
            ob->flags ^= UIF_ACTIVE;

            if(ob->data)
            {
                void* data;

                if(ob->type == UI_BUTTON2EX)
                    data = ((uidata_button_t*) ob->data)->data;
                else
                    data = ob->data;

                *(char*) data = (ob->flags & UIF_ACTIVE) != 0;
            }

            // Call the action function.
            if(ob->action)
                ob->action(ob);
        }
        ob->timer = 0;
        return true;
    }
    return false;
}

void UIButton_Drawer(ui_object_t* ob)
{
    int dis = (ob->flags & UIF_DISABLED) != 0;
    int act = (ob->flags & UIF_ACTIVE) != 0;
    int click = (ob->flags & UIF_CLICKED) != 0;
    boolean down = act || click;
    ui_color_t back;
    float t = ob->timer / 15.0f;
    float alpha = (dis ? .2f : 1);
    const char* text;

    if(ob->type == UI_BUTTON2EX)
    {
        uidata_button_t* data = ob->data;

        if(down)
            text = data->yes;
        else
            text = data->no;
    }
    else
    {
        text = ob->text;
    }

    // Mix the background color.
    if(!click || t > .5f)
        t = .5f;
    if(act && t > .1f)
        t = .1f;

    glEnable(GL_TEXTURE_2D);
    UI_MixColors(UI_Color(UIC_TEXT), UI_Color(UIC_SHADOW), &back, t);
    UI_GradientEx(ob->x, ob->y, ob->w, ob->h, UI_BUTTON_BORDER, &back, 0, alpha, 0);
    UI_Shade(ob->x, ob->y, ob->w, ob->h, UI_BUTTON_BORDER * (down ? -1 : 1), UI_Color(UIC_BRD_HI), UI_Color(UIC_BRD_LOW), alpha / 3, -1);
    UI_DrawRectEx(ob->x, ob->y, ob->w, ob->h, UI_BUTTON_BORDER * (down ? -1 : 1), false, UI_Color(UIC_BRD_HI), NULL, alpha, -1);
    FR_SetFont(glFontVariable[GLFS_NORMAL]);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    UI_TextOutEx2(text, down + ob->x + (ob->flags & UIF_LEFT_ALIGN ? UI_BUTTON_BORDER * 2 : ob->w / 2), down + ob->y + ob->h / 2, UI_Color(UIC_TITLE), alpha, ((ob->flags & UIF_LEFT_ALIGN)? ALIGN_LEFT : 0), DTF_ONLY_SHADOW);
    glDisable(GL_TEXTURE_2D);
}

int UIEdit_Responder(ui_object_t* ob, ddevent_t* ev)
{
    uidata_edit_t* dat = ob->data;

    if(ob->flags & UIF_ACTIVE)
    {
        if(!IS_KEY_PRESS(ev))
            return false;

        switch(ev->toggle.id)
        {
        case DDKEY_LEFTARROW:
            if(dat->cp > 0)
                dat->cp--;
            break;

        case DDKEY_RIGHTARROW:
            if(dat->cp < (uint) strlen(ob->text))
                dat->cp++;
            break;

        case DDKEY_HOME:
            dat->cp = 0;
            break;

        case DDKEY_END:
            dat->cp = (uint) strlen(ob->text);
            break;

        case DDKEY_BACKSPACE:
            if(dat->cp == 0)
                break;
            dat->cp--;
            // Fall through.
        case DDKEY_DEL:
            memmove(ob->text + dat->cp, ob->text + dat->cp + 1, strlen(ob->text) - dat->cp);
            break;

        case DDKEY_RETURN:
            // Store changes.
            memset(dat->ptr, 0, dat->maxlen);
            strncpy(dat->ptr, ob->text, dat->maxlen);
            if(ob->action)
                ob->action(ob);
            // Fall through.
        case DDKEY_ESCAPE:
            memset(ob->text, 0, sizeof(ob->text));
            strncpy(ob->text, dat->ptr, 255);
            ob->flags &= ~UIF_ACTIVE;
            UI_Capture(0);
            break;

        default:
            if((int) strlen(ob->text) < dat->maxlen && ev->toggle.id >= 32 &&
               (DD_ModKey(ev->toggle.id) <= 127 || DD_ModKey(ev->toggle.id) >= DD_HIGHEST_KEYCODE))
            {
                memmove(ob->text + dat->cp + 1, ob->text + dat->cp, strlen(ob->text) - dat->cp);
                ob->text[dat->cp++] = DD_ModKey(ev->toggle.id);
            }
            break;
        }
        return true;
    }
    else if(IS_TOGGLE_DOWN(ev) &&
            ((ev->device == IDEV_MOUSE && UI_MouseInside(ob)) ||
             (ev->device == IDEV_KEYBOARD && IS_ACTKEY(ev->toggle.id))))
    {
        // Activate and capture.
        ob->flags |= UIF_ACTIVE;
        ob->timer = 0;
        UI_Capture(ob);
        memset(ob->text, 0, sizeof(ob->text));
        strncpy(ob->text, dat->ptr, 255);
        dat->cp = (uint) strlen(ob->text);
        return true;
    }
    return false;
}

void UIEdit_Drawer(ui_object_t* ob)
{
    uidata_edit_t* dat = ob->data;
    int act = (ob->flags & UIF_ACTIVE) != 0;
    int dis = (ob->flags & UIF_DISABLED) != 0;
    int textWidth;
    ui_color_t back;
    float t = ob->timer / 8.0f;
    char buf[256];
    uint curx, i, maxw = ob->w - UI_BORDER * 4, firstInBuf = 0;
    float alpha = (dis ? .2f : .5f);

    // Mix the background color.
    if(!act || t > 1)
        t = 1;
    UI_MixColors(UI_Color(UIC_TEXT), UI_Color(UIC_SHADOW), &back, t);
    glEnable(GL_TEXTURE_2D);
    UI_GradientEx(ob->x, ob->y, ob->w, ob->h, UI_BORDER, &back, 0, alpha, 0);
    UI_Shade(ob->x, ob->y, ob->w, ob->h, UI_BORDER, UI_Color(UIC_BRD_HI), UI_Color(UIC_BRD_LOW), alpha / 3, -1);
    UI_DrawRectEx(ob->x, ob->y, ob->w, ob->h, UI_BORDER * (act ? -1 : 1), false, UI_Color(UIC_BRD_HI), NULL, dis ? .2f : 1, -1);
    // Draw text.
    FR_SetFont(glFontVariable[GLFS_LIGHT]);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    memset(buf, 0, sizeof(buf));

    // Does all of it fit in the box?
    textWidth = FR_TextWidth(ob->text);
    if(textWidth > 0 && (unsigned) textWidth > maxw)
    {   // No, it doesn't fit.
        if(!act)
            strCpyLen(buf, ob->text, maxw);
        else
        {
            // Can we show to the cursor?
            for(curx = 0, i = 0; i < dat->cp; ++i)
                curx += FR_CharWidth(ob->text[i]);

            // How much do we need to skip forward?
            for(; curx > maxw; ++firstInBuf)
                curx -= FR_CharWidth(ob->text[firstInBuf]);
            strCpyLen(buf, ob->text + firstInBuf, maxw);
        }
    }
    else
    {   // It fits!
        strcpy(buf, ob->text);
    }
    UI_TextOutEx2(buf, ob->x + UI_BORDER * 2, ob->y + ob->h / 2, UI_Color(UIC_TEXT), dis ? .2f : 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    if(act && ob->timer & 4)
    {
        // Draw cursor.
        // Determine position.
        for(curx = 0, i = firstInBuf; i < dat->cp; ++i)
            curx += FR_CharWidth(ob->text[i]);
        UI_Gradient(ob->x + UI_BORDER * 2 + curx - 1, ob->y + ob->h / 2 - uiFontHgt / 2, 2, uiFontHgt, UI_Color(UIC_TEXT), 0, 1, 1);
    }
    glDisable(GL_TEXTURE_2D);
}

int UIList_Responder(ui_object_t* ob, ddevent_t* ev)
{
    uidata_list_t* dat = ob->data;
    int i, oldsel = dat->selection, buth, barh;
    int used = false;

    if(ob->flags & UIF_CLICKED)
    {
        // We've captured all input.
        if(ev->device == IDEV_MOUSE)
        {
            if(ev->type == E_AXIS)
            {
                // Calculate the new position.
                buth = listButtonHeight(ob);
                barh = ob->h - 2 * (UI_BORDER + buth);
                if(barh - buth)
                {
                    dat->first = ((uiCY - ob->y - UI_BORDER - (buth * 3) / 2) * (dat->count - dat->numvis) + (barh - buth) / 2) / (barh - buth);
                }
                else
                {
                    dat->first = 0;
                }
                // Check that it's in range.
                if(dat->first > dat->count - dat->numvis)
                    dat->first = dat->count - dat->numvis;
                if(dat->first < 0)
                    dat->first = 0;
            }
            else if(IS_TOGGLE_UP(ev))
            {
                dat->button[1] = false;
                // Release capture.
                UI_Capture(0);
                ob->flags &= ~UIF_CLICKED;
            }
        }
        // We're eating everything.
        return true;
    }
    else if(IS_KEY_PRESS(ev))
    {
        used = true;
        switch(ev->toggle.id)
        {
        case DDKEY_UPARROW:
            if(dat->selection > 0)
                dat->selection--;
            break;

        case DDKEY_DOWNARROW:
            if(dat->selection < dat->count - 1)
                dat->selection++;
            break;

        case DDKEY_HOME:
            dat->selection = 0;
            break;

        case DDKEY_END:
            dat->selection = dat->count - 1;
            break;

        default:
            used = false;
        }
    }
    else if(IS_MOUSE_DOWN(ev))
    {
        if(!UI_MouseInside(ob))
            return false;
        // Now we know we're going to eat this event.
        used = true;
        buth = listButtonHeight(ob);
        // Clicked in the item section?
        if(dat->count > 0 && UI_MouseInsideBox(ob->x + UI_BORDER, ob->y + UI_BORDER, ob->w - 2 * UI_BORDER - (dat->count >= dat->numvis ? UI_BAR_WDH : 0), ob->h - 2 * UI_BORDER))
        {
            dat->selection = dat->first + (uiCY - ob->y - UI_BORDER) / listItemHeight(dat);
            if(dat->selection >= dat->count)
                dat->selection = dat->count - 1;
        }
        else if(dat->count < dat->numvis)
        {
            // No scrollbar.
            return true;
        }
        // Clicked the Up button?
        else if(UI_MouseInsideBox(ob->x + ob->w - UI_BORDER - UI_BAR_WDH, ob->y + UI_BORDER, UI_BAR_WDH, buth))
        {
            // The Up button is now pressed.
            dat->button[0] = true;
            ob->timer = SCROLL_TIME; // Ticker does the scrolling.
            return true;
        }
        // Clicked the Down button?
        else if(UI_MouseInsideBox(ob->x + ob->w - UI_BORDER - UI_BAR_WDH, ob->y + ob->h - UI_BORDER - buth, UI_BAR_WDH, buth))
        {
            // The Down button is now pressed.
            dat->button[2] = true;
            ob->timer = SCROLL_TIME; // Ticker does the scrolling.
            return true;
        }
        // Clicked the Thumb?
        else if(UI_MouseInsideBox(ob->x + ob->w - UI_BORDER - UI_BAR_WDH, listThumbPos(ob), UI_BAR_WDH, buth))
        {
            dat->button[1] = true;
            // Capture input and start tracking mouse movement.
            UI_Capture(ob);
            ob->flags |= UIF_CLICKED;
            return true;
        }
        else
            return false;
    }
    else if(IS_MOUSE_UP(ev))
    {
        // Release all buttons.
        for(i = 0; i < 3; ++i)
            dat->button[i] = false;
        // We might have leaved the object's area, so let other objects
        // process this event, too.
        return false;
    }
    else
    {
        // We are not going to use this event.
        return false;
    }
    // Adjust the first visible item.
    if(dat->selection >= 0)
    {
        if(dat->selection < dat->first)
            dat->first = dat->selection;
        if(dat->selection > dat->first + dat->numvis - 1)
            dat->first = dat->selection - dat->numvis + 1;
    }
    // Call action function?
    if(oldsel != dat->selection && ob->action)
        ob->action(ob);
    return used;
}

void UIList_Ticker(ui_object_t* ob)
{
    uidata_list_t* dat = ob->data;

    if(ob->timer >= SCROLL_TIME && (dat->button[0] || dat->button[2]))
    {
        // Next time in 3 ticks.
        ob->timer = 0;
        // Send a direct event.
        if(dat->button[0] && dat->first > 0)
            dat->first--;
        if(dat->button[2] && dat->first < dat->count - dat->numvis)
            dat->first++;
    }
}

void UIList_Drawer(ui_object_t* ob)
{
    uidata_list_t* dat = ob->data;
    uidata_listitem_t* items = dat->items;
    int dis = (ob->flags & UIF_DISABLED) != 0;
    int i, c, x, y, ihgt, maxw = ob->w - 2 * UI_BORDER;
    int maxh = ob->h - 2 * UI_BORDER, buth;
    char buf[256], *ptr, *endptr, tmp[256];
    float alpha = dis ? .2f : 1;
    int barw;

    // The background.
    glEnable(GL_TEXTURE_2D);
    UI_GradientEx(ob->x, ob->y, ob->w, ob->h, UI_BORDER, UI_Color(UIC_SHADOW), 0, alpha / 2, 0);
    // The borders.
    UI_DrawRectEx(ob->x, ob->y, ob->w, ob->h, -UI_BORDER, false, UI_Color(UIC_BRD_HI), NULL, alpha, -1);
    // The title.
    FR_SetFont(glFontVariable[GLFS_NORMAL]);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    UI_TextOutEx(ob->text, ob->x, ob->y - UI_BORDER - uiFontHgt, UI_Color(UIC_TEXT), alpha);
    glDisable(GL_TEXTURE_2D);

    // Is a scroll bar necessary?
    ihgt = listItemHeight(dat);
    if(dat->numvis < dat->count)
    {
        barw = UI_BAR_WDH;
        maxw -= barw;
        buth = listButtonHeight(ob);
        x = ob->x + ob->w - UI_BORDER - barw;
        y = ob->y + UI_BORDER;

        glEnable(GL_TEXTURE_2D);
        UI_GradientEx(x, y, barw, maxh, UI_BAR_BUTTON_BORDER, UI_Color(UIC_TEXT), 0, alpha * .2f, alpha * .2f);
        glDisable(GL_TEXTURE_2D);

        // Up Button.
        UI_DrawButton(x, y, barw, buth, UI_BAR_BUTTON_BORDER, !dat->first ? alpha * .2f : alpha, NULL, dat->button[0], dis, UIBA_UP);
        // Thumb Button.
        UI_DrawButton(x, listThumbPos(ob), barw, buth, UI_BAR_BUTTON_BORDER, alpha, NULL, dat->button[1], dis, UIBA_NONE);
        // Down Button.
        UI_DrawButton(x, y + maxh - buth, barw, buth, UI_BAR_BUTTON_BORDER, dat->first + dat->numvis >= dat->count ? alpha * .2f : alpha, NULL, dat->button[2], dis, UIBA_DOWN);
    }
    x = ob->x + UI_BORDER;
    y = ob->y + UI_BORDER;
    // Draw columns?
    glEnable(GL_TEXTURE_2D);
    for(c = 0; c < UI_MAX_COLUMNS; ++c)
    {
        if(!dat->column[c] || dat->column[c] > maxw - 2 * UI_BORDER)
            continue;
        UI_Gradient(x + UI_BORDER + dat->column[c] - 2, ob->y + UI_BORDER, 1, maxh, UI_Color(UIC_TEXT), 0, alpha * .5f, alpha * .5f);
    }
    FR_SetFont(glFontVariable[GLFS_LIGHT]);
    for(i = dat->first; i < dat->count && i < dat->first + dat->numvis; ++i, y += ihgt)
    {
        // The selection has a white background.
        if(i == dat->selection)
        {
            UI_GradientEx(x, y, maxw, ihgt, UI_BAR_BORDER, UI_Color(UIC_TEXT), 0, alpha * .6f, alpha * .2f);
        }
        // The text, clipped w/columns.
        ptr = items[i].text;
        for(c = 0; c < UI_MAX_COLUMNS; ++c)
        {
            endptr = strchr(ptr, '\t');
            memset(tmp, 0, sizeof(tmp));
            if(endptr)
                strncpy(tmp, ptr, endptr - ptr);
            else
                strcpy(tmp, ptr);
            memset(buf, 0, sizeof(buf));
            strCpyLen(buf, tmp, maxw - 2 * UI_BORDER - dat->column[c]);
            UI_TextOutEx2(buf, x + UI_BORDER + dat->column[c], y + ihgt / 2, UI_Color(UIC_TEXT), alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);
            if(!endptr)
                break;
            ptr = endptr + 1;
        }
    }
    glDisable(GL_TEXTURE_2D);
}

int UI_SliderButtonWidth(ui_object_t* ob)
{
    int width = ob->h - UI_BAR_BORDER * 2;
    if(width < UI_BAR_BORDER * 3)
        width = UI_BAR_BORDER * 3;
    return width;
}

int UI_SliderThumbPos(ui_object_t* ob)
{
    uidata_slider_t* dat = ob->data;
    float range = dat->max - dat->min, useval;
    int butw = UI_SliderButtonWidth(ob);

    if(!range)
        range = 1; // Should never happen.
    if(dat->floatmode)
        useval = dat->value;
    else
    {
        if(dat->value >= 0)
            useval = (int) (dat->value + .5f);
        else
            useval = (int) (dat->value - .5f);
    }
    useval -= dat->min;
    return ob->x + UI_BAR_BORDER + butw + useval / range * (ob->w - UI_BAR_BORDER * 2 - butw * 3);
}

int UISlider_Responder(ui_object_t* ob, ddevent_t* ev)
{
    uidata_slider_t* dat = ob->data;
    float oldvalue = dat->value;
    boolean used = false;
    int i, butw, inw;

    if(ob->flags & UIF_CLICKED)
    {
        // We've captured all input.
        if(ev->device == IDEV_MOUSE)
        {
            if(ev->type == E_AXIS)
            {
                // Calculate new value from the mouse position.
                butw = UI_SliderButtonWidth(ob);
                inw = ob->w - 2 * UI_BAR_BORDER - 3 * butw;
                if(inw > 0)
                {
                    dat->value = dat->min + (dat->max - dat->min) * (uiCX - ob->x - UI_BAR_BORDER - (3 * butw) / 2) / (float) inw;
                }
                else
                {
                    dat->value = dat->min;
                }
                if(dat->value < dat->min)
                    dat->value = dat->min;
                if(dat->value > dat->max)
                    dat->value = dat->max;
                if(!dat->floatmode)
                {
                    if(dat->value >= 0)
                        dat->value = (int) (dat->value + .5f);
                    else
                        dat->value = (int) (dat->value - .5f);
                }
                if(ob->action)
                    ob->action(ob);
            }
            else if(IS_TOGGLE_UP(ev))
            {
                dat->button[1] = false; // Release thumb.
                UI_Capture(0);
                ob->flags &= ~UIF_CLICKED;
            }
        }
        return true;
    }
    else if(IS_KEY_PRESS(ev))
    {
        used = true;
        switch(ev->toggle.id)
        {
        case DDKEY_HOME:
            dat->value = dat->min;
            break;

        case DDKEY_END:
            dat->value = dat->max;
            break;

        case DDKEY_LEFTARROW:
            dat->value -= dat->step;
            if(dat->value < dat->min)
                dat->value = dat->min;
            break;

        case DDKEY_RIGHTARROW:
            dat->value += dat->step;
            if(dat->value > dat->max)
                dat->value = dat->max;
            break;

        case DDKEY_UPARROW:
        case DDKEY_DOWNARROW:
            // Ignore up/down while in the slider.
            break;

        default:
            used = false;
        }
    }
    else if(IS_MOUSE_DOWN(ev))
    {
        if(!UI_MouseInside(ob))
            return false;
        used = true;
        butw = UI_SliderButtonWidth(ob);
        // Where is the mouse cursor?
        if(UI_MouseInsideBox(ob->x, ob->y, butw + UI_BAR_BORDER, ob->h))
        {
            // The Left button is now pressed.
            dat->button[0] = true;
            ob->timer = SCROLL_TIME; // Ticker does the scrolling.
            return true;
        }
        if(UI_MouseInsideBox(ob->x + ob->w - butw - UI_BAR_BORDER, ob->y, butw + UI_BAR_BORDER, ob->h))
        {
            // The Right button is now pressed.
            dat->button[2] = true;
            ob->timer = SCROLL_TIME; // Tickes does the scrolling.
            return true;
        }
        if(UI_MouseInsideBox(UI_SliderThumbPos(ob), ob->y, butw, ob->h))
        {
            // Capture input and start tracking mouse movement.
            dat->button[1] = true;
            UI_Capture(ob);
            ob->flags |= UIF_CLICKED;
            return true;
        }
    }
    else if(IS_MOUSE_UP(ev))
    {
        // Release all buttons.
        for(i = 0; i < 3; ++i)
            dat->button[i] = false;
        // We might be outside the object's area (or none of the buttons
        // might even be pressed), so let others have this event, too.
        return false;
    }
    else
    {
        // We are not going to use this event.
        return false;
    }
    // Did the value change?
    if(oldvalue != dat->value && ob->action)
        ob->action(ob);
    return used;
}

void UISlider_Ticker(ui_object_t* ob)
{
    uidata_slider_t* dat = ob->data;
    float oldval;

    if(ob->timer >= SCROLL_TIME && (dat->button[0] || dat->button[2]))
    {
        ob->timer = 0;
        oldval = dat->value;
        if(dat->button[0])
            dat->value -= dat->step;
        if(dat->button[2])
            dat->value += dat->step;
        if(dat->value < dat->min)
            dat->value = dat->min;
        if(dat->value > dat->max)
            dat->value = dat->max;
        if(oldval != dat->value && ob->action)
            ob->action(ob);
    }
}

void UISlider_Drawer(ui_object_t* ob)
{
    uidata_slider_t* dat = ob->data;
    boolean dis = (ob->flags & UIF_DISABLED) != 0;
    int inwidth = ob->w - UI_BAR_BORDER * 2;
    int inheight = ob->h - UI_BAR_BORDER * 2;
    int butw = UI_SliderButtonWidth(ob);
    int butbor = UI_BAR_BUTTON_BORDER;
    int x, y, thumbx;
    float alpha = dis ? .2f : 1;
    char buf[80];

    // The background.
    glEnable(GL_TEXTURE_2D);
    UI_GradientEx(ob->x, ob->y, ob->w, ob->h, UI_BAR_BORDER, UI_Color(UIC_SHADOW), 0, alpha / 2, 0);
    // The borders.
    UI_DrawRectEx(ob->x, ob->y, ob->w, ob->h, -UI_BAR_BORDER, false, UI_Color(UIC_BRD_HI), NULL, alpha, -1);
    glDisable(GL_TEXTURE_2D);

    x = ob->x + UI_BAR_BORDER;
    y = ob->y + UI_BAR_BORDER;

    // The left button.
    UI_DrawButton(x, y, butw, inheight, butbor, alpha * (dat->value == dat->min ? .2f : 1), NULL, dat->button[0], dis, UIBA_LEFT);

    // The right button.
    UI_DrawButton(x + inwidth - butw, y, butw, inheight, butbor, alpha * (dat->value == dat->max ? .2f : 1), NULL, dat->button[2], dis, UIBA_RIGHT);

    // The thumb.
    UI_DrawButton(thumbx = UI_SliderThumbPos(ob), y, butw, inheight, butbor, alpha, NULL, dat->button[1], dis, UIBA_NONE);

    // The value.
    if(dat->floatmode)
    {
        if(dat->step >= .01f)
        {
            sprintf(buf, "%.2f", dat->value);
        }
        else
        {
            sprintf(buf, "%.5f", dat->value);
        }
    }
    else
        sprintf(buf, "%i", (int) dat->value);
    if(dat->zerotext && dat->value == dat->min)
        strcpy(buf, dat->zerotext);
    glEnable(GL_TEXTURE_2D);
    FR_SetFont(glFontVariable[GLFS_LIGHT]);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    UI_TextOutEx2(buf, x + (dat->value < (dat->min + dat->max) / 2 ? inwidth - butw - UI_BAR_BORDER - FR_TextWidth(buf) : butw + UI_BAR_BORDER), y + inheight / 2, UI_Color(UIC_TEXT), alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);
    glDisable(GL_TEXTURE_2D);
}

//---------------------------------------------------------------------------
// Helper Functions
//---------------------------------------------------------------------------

void UI_InitColumns(ui_object_t* ob)
{
    uidata_list_t* dat = ob->data;
    uidata_listitem_t* list = dat->items;
    int i, c, w, sep, last, maxw;
    char* endptr, *ptr, temp[256];
    int width[UI_MAX_COLUMNS];
    int numcols = 1;

    FR_SetFont(glFontVariable[GLFS_LIGHT]);
    FR_LoadDefaultAttrib();

    memset(dat->column, 0, sizeof(dat->column));
    memset(width, 0, sizeof(width));
    for(i = 0; i < dat->count; ++i)
    {
        ptr = list[i].text;
        for(c = 0; c < UI_MAX_COLUMNS; ++c)
        {
            if(c + 1 > numcols)
                numcols = c + 1;
            endptr = strchr(ptr, '\t');
            memset(temp, 0, sizeof(temp));
            if(endptr)
                strncpy(temp, ptr, endptr - ptr);
            else
                strcpy(temp, ptr);
            w = FR_TextWidth(temp);
            if(w > width[c])
                width[c] = w;
            if(!endptr)
                break;
            ptr = endptr + 1;
        }
    }
    // Calculate the total maximum width.
    for(w = i = 0; i < UI_MAX_COLUMNS; ++i)
    {
        w += width[i];
        if(width[i])
            last = width[i];
    }
    // Calculate the offset for each column.
    maxw = ob->w - 4 * UI_BORDER - (dat->count > dat->numvis ? UI_BAR_WDH : 0);
    sep = maxw - w;
    if(numcols > 1)
        sep /= numcols - 1;
    if(sep < 0)
        sep = 0;
    for(c = i = 0; i < numcols; ++i)
    {
        dat->column[i] = c;
        c += sep + width[i];
    }
}

static int listItemHeight(uidata_list_t* listdata)
{
    int h = listdata->itemhgt;
    if(h < uiFontHgt * 8 / 10)
        h = uiFontHgt * 8 / 10;
    return h;
}

static int listButtonHeight(ui_object_t* ob)
{
    int barh = ob->h - 2 * UI_BORDER;
    int buth = UI_BAR_WDH;
    if(buth > barh / 3)
        buth = barh / 3;
    return buth;
}

static int listThumbPos(ui_object_t* ob)
{
    uidata_list_t* dat = ob->data;
    int buth = listButtonHeight(ob);
    int barh = ob->h - 2 * (UI_BORDER + buth);

    if(dat->count <= dat->numvis)
        return 0;
    return ob->y + UI_BORDER + buth + ((barh - buth) * dat->first) / (dat->count - dat->numvis);
}

int UI_ListFindItem(ui_object_t* ob, int dataValue)
{
    uidata_list_t* dat = ob->data;
    int i;
    for(i = 0; i < dat->count; ++i)
        if(((uidata_listitem_t*) dat->items)[i].data == dataValue)
            return i;
    return -1;
}

static void strCpyLen(char* dest, const char* src, int maxWidth)
{
    int i, width;
    for(i = 0, width = 0; src[i]; ++i)
    {
        dest[i] = src[i];
        width += FR_CharWidth(src[i]);
        if(width > maxWidth)
        {
            dest[i] = 0;
            break;
        }
    }
}

int UI_MouseInsideBox(int x, int y, int w, int h)
{
    return (uiCX >= x && uiCX <= x + w && uiCY >= y && uiCY <= y + h);
}

int UI_MouseInside(ui_object_t* ob)
{
    return UI_MouseInsideBox(ob->x, ob->y, ob->w, ob->h);
}

int UI_MouseResting(ui_page_t* page)
{
    if(!uiMoved)
        return false;
    return page->_timer - uiRestStart >= uiRestTime;
}

void UI_MixColors(ui_color_t* a, ui_color_t* b, ui_color_t* dest, float amount)
{
    dest->red   = (1 - amount) * a->red   + amount * b->red;
    dest->green = (1 - amount) * a->green + amount * b->green;
    dest->blue  = (1 - amount) * a->blue  + amount * b->blue;
}

void UI_SetColorA(ui_color_t* color, float alpha)
{
    glColor4f(color->red, color->green, color->blue, alpha);
}

void UI_SetColor(ui_color_t* color)
{
    glColor3f(color->red, color->green, color->blue);
}

void UI_Shade(int x, int y, int w, int h, int border, ui_color_t* main,
    ui_color_t *secondary, float alpha, float bottomAlpha)
{
    float s[2][2] = { {0, 1}, {1, 0} };
    float t[2][2] = { {0, 1}, {1, 0} };
    ui_color_t* color;
    uint i, flip = 0;
    float* u, *v;
    float beta = 1;

    alpha *= uiAlpha;
    bottomAlpha *= uiAlpha;

    if(border < 0)
    {
        //flip = 1;
        border = -border;
    }
    border = 0;
    if(bottomAlpha < 0)
        bottomAlpha = alpha;

    GL_BlendMode(BM_ADD);
    glBindTexture(GL_TEXTURE_2D, uiTextures[UITEX_SHADE]);
    glBegin(GL_QUADS);
    for(i = 0; i < 2; ++i)
    {
        if(!secondary)
            continue;

        color = (i == 0 ? main : secondary);
        u = (i == flip ? s[0] : s[1]);
        v = (i == flip ? t[0] : t[1]);
        if(i == 1)
            beta = 0.5f;

        UI_SetColorA(color, alpha * beta);
        glTexCoord2f(u[0], v[0]);
        glVertex2f(x + border, y + border);
        glTexCoord2f(u[1], v[0]);
        glVertex2f(x + w - border, y + border);
        UI_SetColorA(color, bottomAlpha * beta);
        glTexCoord2f(u[1], v[1]);
        glVertex2f(x + w - border, y + h - border);
        glTexCoord2f(u[0], v[1]);
        glVertex2f(x + border, y + h - border);
    }
    glEnd();

    GL_BlendMode(BM_NORMAL);
}

void UI_Gradient(int x, int y, int w, int h, ui_color_t* top, ui_color_t* bottom,
    float topAlpha, float bottomAlpha)
{
    UI_GradientEx(x, y, w, h, 0, top, bottom, topAlpha, bottomAlpha);
}

void UI_GradientEx(int x, int y, int w, int h, int border, ui_color_t* top,
    ui_color_t* bottom, float topAlpha, float bottomAlpha)
{
    UI_DrawRectEx(x, y, w, h, border, true, top, bottom, topAlpha, bottomAlpha);
}

void UI_HorizGradient(int x, int y, int w, int h, ui_color_t* left, ui_color_t* right,
    float leftAlpha, float rightAlpha)
{
    leftAlpha  *= uiAlpha;
    rightAlpha *= uiAlpha;

    glBindTexture(GL_TEXTURE_2D, uiTextures[UITEX_HINT]);
    glBegin(GL_QUADS);
        UI_SetColorA(left, leftAlpha);
        glTexCoord2f(0, 1);
        glVertex2f(x, y + h);
        glTexCoord2f(0, 0);
        glVertex2f(x, y);
        UI_SetColorA(right ? right : left, rightAlpha);
        glTexCoord2f(1, 0);
        glVertex2f(x + w, y);
        glTexCoord2f(1, 1);
        glVertex2f(x + w, y + h);
    glEnd();
}

void UI_Line(int x1, int y1, int x2, int y2, ui_color_t* start, ui_color_t* end,
    float startAlpha, float endAlpha)
{
    startAlpha *= uiAlpha;
    endAlpha   *= uiAlpha;

    glBegin(GL_LINES);
        UI_SetColorA(start, startAlpha);
        glVertex2f(x1, y1);
        UI_SetColorA(end ? end : start, endAlpha);
        glVertex2f(x2, y2);
    glEnd();
}

void UI_TextOutEx2(const char* text, int x, int y, ui_color_t* color, float alpha,
    int alignFlags, short textFlags)
{
    alpha *= uiAlpha;
    if(alpha <= 0) return;
    FR_SetColorAndAlpha(color->red, color->green, color->blue, alpha);
    FR_DrawText3(text, x, y, alignFlags, textFlags);
}

void UI_TextOutEx(const char* text, int x, int y, ui_color_t* color, float alpha)
{
    UI_TextOutEx2(text, x, y, color, alpha, DEFAULT_ALIGNFLAGS, DEFAULT_DRAWFLAGS);
}

int UI_TextOutWrap(const char* text, int x, int y, int w, int h)
{
    return UI_TextOutWrapEx(text, x, y, w, h, UI_Color(UIC_TEXT), 1);
}

int UI_TextOutWrapEx(const char* text, int x, int y, int w, int h, ui_color_t* color,
    float alpha)
{
    char word[2048], *wp = word;
    int len, tx = x, ty = y;
    int linehgt = FR_SingleLineHeight("A");
    byte c;

    alpha *= uiAlpha;

    FR_SetColorAndAlpha(color->red, color->green, color->blue, alpha);

    for(;; text++)
    {
        c = *text;
        // Whitespace?
        if(!c || c == ' ' || c == '\n' || c == '\b' || c == '-')
        {
            if(c == '-')
                *wp++ = c; // Hyphens should be included in the word.
            // Time to print the word.
            *wp = 0;
            len = FR_TextWidth(word);
            if(tx + len > x + w) // Doesn't fit?
            {
                tx = x;
                ty += linehgt;
            }
            // Can't print any more? (always print the 1st line)
            if(ty + linehgt > y + h && ty != y)
                return ty;
            FR_DrawText(word, tx, ty);
            tx += len;
            wp = word;
            // React to delimiter.
            switch (c)
            {
            case 0:
                return ty; // All of the text has been printed.

            case ' ':
                tx += FR_TextWidth(" ");
                break;

            case '\n':
                tx = x;
                ty += linehgt;
                break;

            case '\b': // Break.
                tx = x;
                ty += 3 * linehgt / 2;
                break;

            default:
                break;
            }
        }
        else
        {
            // Append to word buffer.
            *wp++ = c;
        }
    }
}

void UI_DrawRectEx(int x, int y, int w, int h, int brd, boolean filled, ui_color_t* top,
    ui_color_t* bottom, float alpha, float bottomAlpha)
{
    float s[2] = { 0, 1 }, t[2] = { 0, 1 };

    alpha *= uiAlpha;
    bottomAlpha *= uiAlpha;
    if(alpha <= 0 && bottomAlpha <= 0)
        return;

    if(brd < 0)
    {
        brd = -brd;
        s[0] = t[0] = 1;
        s[1] = t[1] = 0;
    }
    if(bottomAlpha < 0)
        bottomAlpha = alpha;
    if(!bottom)
        bottom = top;

    // The fill comes first, if there's one.
    if(filled)
    {
        glBindTexture(GL_TEXTURE_2D, uiTextures[UITEX_FILL]);
        glBegin(GL_QUADS);
        glTexCoord2f(0.5f, 0.5f);
        UI_SetColorA(top, alpha);
        glVertex2f(x + brd, y + brd);
        glVertex2f(x + w - brd, y + brd);
        UI_SetColorA(bottom, bottomAlpha);
        glVertex2f(x + w - brd, y + h - brd);
        glVertex2f(x + brd, y + h - brd);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, uiTextures[UITEX_CORNER]);
        glBegin(GL_QUADS);
    }
    if(!filled || brd > 0)
    {
        // Top Left.
        UI_SetColorA(top, alpha);
        glTexCoord2f(s[0], t[0]);
        glVertex2f(x, y);
        glTexCoord2f(0.5f, t[0]);
        glVertex2f(x + brd, y);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(x + brd, y + brd);
        glTexCoord2f(s[0], 0.5f);
        glVertex2f(x, y + brd);
        // Top.
        glTexCoord2f(0.5f, t[0]);
        glVertex2f(x + brd, y);
        glTexCoord2f(0.5f, t[0]);
        glVertex2f(x + w - brd, y);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(x + w - brd, y + brd);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(x + brd, y + brd);
        // Top Right.
        glTexCoord2f(0.5f, t[0]);
        glVertex2f(x + w - brd, y);
        glTexCoord2f(s[1], t[0]);
        glVertex2f(x + w, y);
        glTexCoord2f(s[1], 0.5f);
        glVertex2f(x + w, y + brd);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(x + w - brd, y + brd);
        // Right.
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(x + w - brd, y + brd);
        glTexCoord2f(s[1], 0.5f);
        glVertex2f(x + w, y + brd);
        UI_SetColorA(bottom, bottomAlpha);
        glTexCoord2f(s[1], 0.5f);
        glVertex2f(x + w, y + h - brd);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(x + w - brd, y + h - brd);
        // Bottom Right.
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(x + w - brd, y + h - brd);
        glTexCoord2f(s[1], 0.5f);
        glVertex2f(x + w, y + h - brd);
        glTexCoord2f(s[1], t[1]);
        glVertex2f(x + w, y + h);
        glTexCoord2f(0.5f, t[1]);
        glVertex2f(x + w - brd, y + h);
        // Bottom.
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(x + brd, y + h - brd);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(x + w - brd, y + h - brd);
        glTexCoord2f(0.5f, t[1]);
        glVertex2f(x + w - brd, y + h);
        glTexCoord2f(0.5f, t[1]);
        glVertex2f(x + brd, y + h);
        // Bottom Left.
        glTexCoord2f(s[0], 0.5f);
        glVertex2f(x, y + h - brd);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(x + brd, y + h - brd);
        glTexCoord2f(0.5f, t[1]);
        glVertex2f(x + brd, y + h);
        glTexCoord2f(s[0], t[1]);
        glVertex2f(x, y + h);
        // Left.
        UI_SetColorA(top, alpha);
        glTexCoord2f(s[0], 0.5f);
        glVertex2f(x, y + brd);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(x + brd, y + brd);
        UI_SetColorA(bottom, bottomAlpha);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(x + brd, y + h - brd);
        glTexCoord2f(s[0], 0.5f);
        glVertex2f(x, y + h - brd);
    }
    glEnd();
}

void UI_DrawRect(int x, int y, int w, int h, int brd, ui_color_t* color, float alpha)
{
    UI_DrawRectEx(x, y, w, h, brd, false, color, NULL, alpha, alpha);
}

void UI_DrawTriangle(int x, int y, int radius, ui_color_t* hi, ui_color_t* med,
    ui_color_t* low, float alpha)
{
    float xrad = radius * .866f; // cos(60)
    float yrad = radius / 2; // sin(60)

    alpha *= uiAlpha;
    if(alpha <= 0) return;

    glBegin(GL_TRIANGLES);

    y += radius / 4;

    // Upper left triangle.
    UI_SetColorA(radius > 0 ? hi : med, alpha);
    glVertex2f(x, y);
    glVertex2f(x - xrad, y + yrad);
    UI_SetColorA(radius > 0 ? hi : low, alpha);
    glVertex2f(x, y - radius);

    // Upper right triangle.
    UI_SetColorA(low, alpha);
    glVertex2f(x, y);
    glVertex2f(x, y - radius);
    UI_SetColorA(med, alpha);
    glVertex2f(x + xrad, y + yrad);

    // Bottom triangle.
    if(radius < 0)
        UI_SetColorA(hi, alpha);
    glVertex2f(x, y);
    glVertex2f(x + xrad, y + yrad);
    UI_SetColorA(radius > 0 ? low : med, alpha);
    glVertex2f(x - xrad, y + yrad);

    glEnd();
}

void UI_DrawHorizTriangle(int x, int y, int radius, ui_color_t* hi, ui_color_t* med,
    ui_color_t* low, float alpha)
{
    float yrad = radius * .866f; // cos(60)
    float xrad = radius / 2; // sin(60)

    alpha *= uiAlpha;
    if(alpha <= 0)
        return;

    glBegin(GL_TRIANGLES);

    x += radius / 4;

    // Upper left triangle.
    UI_SetColorA(radius > 0 ? hi : med, alpha);
    glVertex2f(x, y);
    if(radius < 0)
        UI_SetColorA(low, alpha);
    glVertex2f(x - radius, y);
    glVertex2f(x + xrad, y - yrad);

    // Lower left triangle.
    UI_SetColorA(radius > 0 ? med : hi, alpha);
    glVertex2f(x, y);
    if(radius < 0)
        UI_SetColorA(hi, alpha);
    glVertex2f(x + xrad, y + yrad);
    UI_SetColorA(radius > 0 ? low : med, alpha);
    glVertex2f(x - radius, y);

    // Right triangle.
    UI_SetColorA(radius > 0 ? med : hi, alpha);
    glVertex2f(x, y);
    UI_SetColorA(radius > 0 ? hi : med, alpha);
    glVertex2f(x + xrad, y - yrad);
    UI_SetColorA(radius > 0 ? low : hi, alpha);
    glVertex2f(x + xrad, y + yrad);

    glEnd();
}

void UI_DefaultButtonBackground(ui_color_t* col, boolean down)
{
    UI_MixColors(UI_Color(UIC_TEXT), UI_Color(UIC_SHADOW), col, down ? .1f : .5f);
}

void UI_DrawButton(int x, int y, int w, int h, int brd, float alpha,
    ui_color_t* background, boolean down, boolean disabled, int arrow)
{
    int inside = MIN_OF(w - brd * 2, h - brd * 2);
    int boff = down ? 2 : 0;
    ui_color_t back;

    if(!background)
    {
        // Calculate the default button color.
        UI_DefaultButtonBackground(&back, down);
        background = &back;
    }

    glEnable(GL_TEXTURE_2D);
    UI_GradientEx(x, y, w, h, brd, background, 0, disabled ? .2f : 1, 0);
    UI_Shade(x, y, w, h, UI_BUTTON_BORDER * (down ? -1 : 1), UI_Color(UIC_BRD_HI), UI_Color(UIC_BRD_LOW), alpha / 3, -1);
    UI_DrawRectEx(x, y, w, h, brd * (down ? -1 : 1), false, UI_Color(UIC_BRD_HI), NULL, alpha, -1);
    glDisable(GL_TEXTURE_2D);

    switch(arrow)
    {
    case UIBA_UP:
    case UIBA_DOWN:
        UI_DrawTriangle(x + w / 2 + boff, y + h / 2 + boff, inside / 2.75f * (arrow == UIBA_DOWN ? -1 : 1), UI_Color(UIC_TEXT), UI_Color(UIC_TEXT), UI_Color(UIC_TEXT), alpha * (disabled ? .2f : 1));
        break;

    case UIBA_LEFT:
    case UIBA_RIGHT:
        UI_DrawHorizTriangle(x + w / 2 + boff, y + h / 2 + boff, inside / 2.75f * (arrow == UIBA_RIGHT ? -1 : 1), UI_Color(UIC_TEXT), UI_Color(UIC_TEXT), UI_Color(UIC_TEXT), alpha * (disabled ? .2f : 1));
        break;

    default:
        break;
    }
}

void UI_DrawHelpBox(int x, int y, int w, int h, float alpha, char* text)
{
    int bor = UI_BUTTON_BORDER;

    glEnable(GL_TEXTURE_2D);
    UI_GradientEx(x, y, w, h, bor, UI_Color(UIC_HELP), UI_Color(UIC_HELP), alpha / 4, alpha / 2);
    UI_DrawRectEx(x, y, w, h, bor, false, UI_Color(UIC_BRD_HI), NULL, alpha, -1);

    if(text)
    {
        bor = 2 * UI_BORDER / 3;
        FR_SetFont(glFontVariable[GLFS_LIGHT]);
        FR_LoadDefaultAttrib();
        UI_TextOutWrapEx(text, x + 2 * bor, y + 2 * bor, w - 4 * bor, h - 4 * bor, UI_Color(UIC_TEXT), alpha);
    }
    glDisable(GL_TEXTURE_2D);
}

void UI_DrawMouse(int x, int y, int w, int h)
{
    glColor3f(1, 1, 1);
    glBindTexture(GL_TEXTURE_2D, uiTextures[UITEX_MOUSE]);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex2f(x, y);
        glTexCoord2f(1, 0);
        glVertex2f(x + w, y);
        glTexCoord2f(1, 1);
        glVertex2f(x + w, y + h);
        glTexCoord2f(0, 1);
        glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void UI_DrawLogo(int x, int y, int w, int h)
{
    glBindTexture(GL_TEXTURE_2D, uiTextures[UITEX_LOGO]);
    glEnable(GL_TEXTURE_2D);
    GL_DrawRectColor(x, y, w, h, 1, 1, 1, uiAlpha);
    glDisable(GL_TEXTURE_2D);
}

void UI_DrawDDBackground(float x, float y, float w, float h, float alpha)
{
    float mul = (uiTextures[UITEX_BACKGROUND]? 1.5f : 1.0f);
    ui_color_t* dark = UI_Color(UIC_BG_DARK);
    ui_color_t* light = UI_Color(UIC_BG_LIGHT);

    // Background gradient picture.
    glBindTexture(GL_TEXTURE_2D, uiTextures[UITEX_BACKGROUND]);
    glEnable(GL_TEXTURE_2D);

    if(alpha < 1.0)
    {
        glEnable(GL_BLEND);
        GL_BlendMode(BM_NORMAL);
    }
    else
    {
        glDisable(GL_BLEND);
    }

    glBegin(GL_QUADS);
        // Top color.
        glColor4f(dark->red * mul, dark->green * mul, dark->blue * mul, alpha);
        glTexCoord2f(0, 0);
        glVertex2f(x, y);
        glTexCoord2f(1, 0);
        glVertex2f(x + w, y);

        // Bottom color.
        glColor4f(light->red * mul, light->green * mul, light->blue * mul, alpha);
        glTexCoord2f(1, 1);
        glVertex2f(x + w, y + h);
        glTexCoord2f(0, 1);
        glVertex2f(0, y + h);
    glEnd();

    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

/**
 * CCmd: Change the UI colors.
 */
D_CMD(UIColor)
{
    struct colorname_s {
        const char* str;
        uint colorIdx;
    } colors[] =
    {
        { "text",       UIC_TEXT },
        { "title",      UIC_TITLE },
        { "shadow",     UIC_SHADOW },
        { "bglight",    UIC_BG_LIGHT },
        { "bgmed",      UIC_BG_MEDIUM },
        { "bgdark",     UIC_BG_DARK },
        { "borhigh",    UIC_BRD_HI },
        { "bormed",     UIC_BRD_MED },
        { "borlow",     UIC_BRD_LOW },
        { "help",       UIC_HELP },
        { 0, 0 }
    };
    size_t i;
    for(i = 0; colors[i].str; ++i)
        if(!stricmp(argv[1], colors[i].str))
        {
            uint idx = colors[i].colorIdx;
            ui_colors[idx].red   = strtod(argv[2], 0);
            ui_colors[idx].green = strtod(argv[3], 0);
            ui_colors[idx].blue  = strtod(argv[4], 0);
            return true;
        }

    Con_Printf("Unknown UI color '%s'.\n", argv[1]);
    return false;
}
