/**\file hu_menu.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Menu widget stuff, episode selection and such.
 */

#ifndef LIBCOMMON_HU_MENU_H
#define LIBCOMMON_HU_MENU_H

#include "dd_types.h"
#include "hu_lib.h"

// Sounds played in the menu.
#if __JDOOM__ || __JDOOM64__
#define SFX_MENU_CLOSE      (SFX_SWTCHX)
#define SFX_MENU_OPEN       (SFX_SWTCHN)
#define SFX_MENU_CANCEL     (SFX_SWTCHN)
#define SFX_MENU_NAV_UP     (SFX_PSTOP)
#define SFX_MENU_NAV_DOWN   (SFX_PSTOP)
#define SFX_MENU_NAV_RIGHT  (SFX_PSTOP)
#define SFX_MENU_ACCEPT     (SFX_PISTOL)
#define SFX_MENU_CYCLE      (SFX_PISTOL) // Cycle available options.
#define SFX_MENU_SLIDER_MOVE (SFX_STNMOV)
#define SFX_QUICKSAVE_PROMPT (SFX_SWTCHN)
#define SFX_QUICKLOAD_PROMPT (SFX_SWTCHN)
#elif __JHERETIC__
#define SFX_MENU_CLOSE      (SFX_DORCLS)
#define SFX_MENU_OPEN       (SFX_SWITCH)
#define SFX_MENU_CANCEL     (SFX_SWITCH)
#define SFX_MENU_NAV_UP     (SFX_SWITCH)
#define SFX_MENU_NAV_DOWN   (SFX_SWITCH)
#define SFX_MENU_NAV_RIGHT  (SFX_SWITCH)
#define SFX_MENU_ACCEPT     (SFX_DORCLS)
#define SFX_MENU_CYCLE      (SFX_DORCLS) // Cycle available options.
#define SFX_MENU_SLIDER_MOVE (SFX_STNMOV)
#define SFX_QUICKSAVE_PROMPT (SFX_CHAT)
#define SFX_QUICKLOAD_PROMPT (SFX_CHAT)
#elif __JHEXEN__
#define SFX_MENU_CLOSE      (SFX_DOOR_LIGHT_CLOSE)
#define SFX_MENU_OPEN       (SFX_DOOR_LIGHT_CLOSE)
#define SFX_MENU_CANCEL     (SFX_PICKUP_KEY)
#define SFX_MENU_NAV_UP     (SFX_FIGHTER_HAMMER_HITWALL)
#define SFX_MENU_NAV_DOWN   (SFX_FIGHTER_HAMMER_HITWALL)
#define SFX_MENU_NAV_RIGHT  (SFX_FIGHTER_HAMMER_HITWALL)
#define SFX_MENU_ACCEPT     (SFX_PLATFORM_STOP)
#define SFX_MENU_CYCLE      (SFX_CHAT) // Cycle available options.
#define SFX_MENU_SLIDER_MOVE (SFX_PICKUP_KEY)
#define SFX_QUICKSAVE_PROMPT (SFX_CHAT)
#define SFX_QUICKLOAD_PROMPT (SFX_CHAT)
#endif

#define MENU_CURSOR_REWIND_SPEED    20
#define MENU_CURSOR_FRAMECOUNT      2
#define MENU_CURSOR_TICSPERFRAME    8

extern int menuTime;
extern boolean menuNominatingQuickSaveSlot;

/// Register the console commands, variables, etc..., of this module.
void Hu_MenuRegister(void);

/**
 * Menu initialization.
 * Called during (post-engine) init and after updating game/engine state.
 *
 * Initializes the various vars, fonts, adjust the menu structs and
 * anything else that needs to be done before the menu can be used.
 */
void Hu_MenuInit(void);

/**
 * Menu shutdown, to be called when the game menu is no longer needed.
 */
void Hu_MenuShutdown(void);

/**
 * Load any resources the menu needs.
 */
void Hu_MenuLoadResources(void);

/**
 * Updates on Game Tick.
 */
void Hu_MenuTicker(timespan_t ticLength);

/// @return  @c true if the menu is presently visible.
boolean Hu_MenuIsVisible(void);

mn_page_t* Hu_MenuFindPageByName(const char* name);

/**
 * @param name  Symbolic name.
 * @param origin  Topleft corner.
 * @param flags  @see menuPageFlags.
 * @param ticker  Ticker callback.
 * @param drawer  Page drawing routine.
 * @param cmdResponder  Menu-command responder routine.
 * @param userData  User data pointer to be associated with the page.
 */
mn_page_t* Hu_MenuNewPage(const char* name, const Point2Raw* origin, int flags,
    void (*ticker) (mn_page_t* page),
    void (*drawer) (mn_page_t* page, const Point2Raw* origin),
    int (*cmdResponder) (mn_page_t* page, menucommand_e cmd),
    void* userData);

/**
 * This is the main menu drawing routine (called every tic by the drawing
 * loop) Draws the current menu 'page' by calling the funcs attached to
 * each menu obj.
 */
void Hu_MenuDrawer(void);

void Hu_MenuPageTicker(struct mn_page_s* page);

void Hu_MenuDrawFocusCursor(int x, int y, int focusObjectHeight, float alpha);

void Hu_MenuDrawPageTitle(const char* title, int x, int y);
void Hu_MenuDrawPageHelp(const char* help, int x, int y);

/// @return  @c true if the input event @a ev was eaten.
int Hu_MenuPrivilegedResponder(event_t* ev);

/// @return  @c true if the input event @a ev was eaten.
int Hu_MenuResponder(event_t* ev);

/**
 * Handles "hotkey" navigation in the menu.
 * @return  @c true if the input event @a ev was eaten.
 */
int Hu_MenuFallbackResponder(event_t* ev);

/**
 * @return  @c true iff the menu is currently active (open).
 */
boolean Hu_MenuIsActive(void);

/**
 * @return  Current alpha level of the menu.
 */
float Hu_MenuAlpha(void);

/**
 * Set the alpha level of the entire menu.
 *
 * @param alpha  Alpha level to set the menu too (0...1)
 */
void Hu_MenuSetAlpha(float alpha);

/**
 * Retrieve the currently active page.
 */
mn_page_t* Hu_MenuActivePage(void);

/**
 * Change the current active page.
 */
void Hu_MenuSetActivePage(mn_page_t* page);

/**
 * Initialize a new singleplayer game according to the options set via the menu.
 * @param confirmed  If @c true this game configuration has already been confirmed.
 */
void Hu_MenuInitNewGame(boolean confirmed);

void Hu_MenuUpdateGameSaveWidgets(void);

int Hu_MenuDefaultFocusAction(mn_object_t* obj, mn_actionid_t action, void* paramaters);

int Hu_MenuCvarButton(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuCvarList(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuCvarSlider(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuCvarEdit(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuCvarColorBox(mn_object_t* obj, mn_actionid_t action, void* paramaters);

int Hu_MenuSaveSlotEdit(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuBindings(mn_object_t* obj, mn_actionid_t action, void* paramaters);

int Hu_MenuActivateColorWidget(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuUpdateColorWidgetColor(mn_object_t* obj, mn_actionid_t action, void* paramaters);

D_CMD(MenuOpen);
D_CMD(MenuCommand);

#endif /* LIBCOMMON_HU_MENU_H */
