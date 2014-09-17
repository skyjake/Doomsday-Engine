/** @file hu_menu.h  Menu widget stuff, episode selection and such.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_HU_MENU_H
#define LIBCOMMON_HU_MENU_H
#ifdef __cplusplus

#include "dd_types.h"
#include "hu_lib.h"
#include "menu/widgets/widget.h"
#include "menu/widgets/cvartogglewidget.h"

namespace common {

extern int menuTime;
extern dd_bool menuNominatingQuickSaveSlot;

extern menu::cvarbutton_t mnCVarButtons[];

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
#define SFX_DELETESAVEGAME_CONFIRM (SFX_SWTCHN)
#define SFX_REBORNLOAD_CONFIRM (SFX_SWTCHN)
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
#define SFX_DELETESAVEGAME_CONFIRM (SFX_CHAT)
#define SFX_REBORNLOAD_CONFIRM (SFX_CHAT)
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
#define SFX_DELETESAVEGAME_CONFIRM (SFX_CHAT)
#define SFX_REBORNLOAD_CONFIRM (SFX_CHAT)
#endif

#define MENU_CURSOR_REWIND_SPEED    20
#define MENU_CURSOR_FRAMECOUNT      2
#define MENU_CURSOR_TICSPERFRAME    8

void Hu_MenuInit();
void Hu_MenuShutdown();

/**
 * Returns @c true if a current menu Page is configured.
 */
bool Hu_MenuHasPage();

/**
 * Returns @c true if the menu contains a Page associated with @a name.
 * @see Hu_MenuPage()
 */
bool Hu_MenuHasPage(de::String name);

/**
 * Returns the currently configured menu Page.
 * @see Hu_MenuHasPage()
 */
menu::Page &Hu_MenuPage();

inline menu::Page *Hu_MenuPagePtr() {
    return Hu_MenuHasPage()? &Hu_MenuPage() : 0;
}

/**
 * Lookup a Page with the unique identifier @a name.
 * @see Hu_MenuHasPage()
 */
menu::Page &Hu_MenuPage(de::String name);

inline menu::Page *Hu_MenuPagePtr(de::String name) {
    return Hu_MenuHasPage(name)? &Hu_MenuPage(name) : 0;
}

/**
 * Change the current menu Page to @a page.
 * @see Hu_MenuPage(), Hu_MenuHasPage()
 */
void Hu_MenuSetPage(menu::Page *page, bool allowReactivate = false);

/**
 * Convenient method for changing the current menu Page to that with the @a name given.
 * @see Hu_MenuSetPage()
 */
inline void Hu_MenuSetPage(de::String name, bool allowReactivate = false) {
    Hu_MenuSetPage(Hu_MenuPagePtr(name), allowReactivate);
}

/**
 * Add a new Page to the menu. If the name of @a page is not unique, or the page
 * has already been added - an Error will be thrown and the page will not be added.
 *
 * @param page  Page to add.
 *
 * @return  Same as @a page, for caller convenience.
 */
menu::Page *Hu_MenuAddPage(menu::Page *page);

/**
 * Returns @c true if the menu is currently active (open).
 */
bool Hu_MenuIsActive();

/**
 * Change the opacity of the entire menu to @a newOpacity.
 * @see Hu_MenuOpacity()
 */
void Hu_MenuSetOpacity(float newOpacity);

/**
 * Returns the current menu opacity.
 * @see Hu_MenuSetOpacity()
 */
float Hu_MenuOpacity();

/**
 * Returns @c true if the menu is presently visible.
 */
bool Hu_MenuIsVisible();

/**
 * This is the main menu drawing routine (called every tic by the drawing loop).
 */
void Hu_MenuDrawer();

/**
 * Updates on Game Tick.
 */
void Hu_MenuTicker(timespan_t ticLength);

/// @return  @c true if the input event @a ev was eaten.
int Hu_MenuPrivilegedResponder(event_t *ev);

/// @return  @c true if the input event @a ev was eaten.
int Hu_MenuResponder(event_t *ev);

/// @return  @c true if the input event @a ev was eaten.
int Hu_MenuFallbackResponder(event_t *ev);

/// @return  @c true if the menu @a command was eaten.
void Hu_MenuCommand(menucommand_e command);

/// Register the console commands, variables, etc..., of this module.
void Hu_MenuConsoleRegister();

// ----------------------------------------------------------------------------------------

void Hu_MenuDefaultFocusAction(menu::Widget *wi, menu::Widget::mn_actionid_t action);

void Hu_MenuDrawFocusCursor(int x, int y, int focusObjectHeight, float alpha);

void Hu_MenuDrawPageTitle(de::String title, int x, int y);
void Hu_MenuDrawPageHelp(char const *help, int x, int y);

} // namespace common

#endif // __cplusplus
#endif // LIBCOMMON_HU_MENU_H
