/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * g_ctrl.c: Control bindings
 */

// HEADER FILES ------------------------------------------------------------

#include "wolftc.h"

#include "g_controls.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

//
// !!! Add new controls to the end, the existing indices must remain unchanged !!!
//
const control_t controls[] = {
    // Actions (must be first so the A_* constants can be used).
    {"left", CLF_ACTION, DDBC_NORMAL, DDKEY_LEFTARROW, 0, 0},
    {"right", CLF_ACTION, DDBC_NORMAL, DDKEY_RIGHTARROW, 0, 0},
    {"forward", CLF_ACTION, DDBC_NORMAL, DDKEY_UPARROW, 0, 0},
    {"backward", CLF_ACTION, DDBC_NORMAL, DDKEY_DOWNARROW, 0, 0},
    {"strafel", CLF_ACTION, DDBC_NORMAL, ',', 0, 0},
    {"strafer", CLF_ACTION, DDBC_NORMAL, '.', 0, 0},
    {"fire", CLF_ACTION, DDBC_NORMAL, DDKEY_RCTRL, 1, 1},
    {"use", CLF_ACTION, DDBC_NORMAL, ' ', 0, 4},
    {"strafe", CLF_ACTION, DDBC_NORMAL, DDKEY_RALT, 3, 2},
    {"speed", CLF_ACTION, DDBC_NORMAL, DDKEY_RSHIFT, 0, 3},

    {"weap1", CLF_ACTION, DDBC_NORMAL, 0, 0, 0},
    {"weapon2", CLF_ACTION, DDBC_NORMAL, '2', 0, 0},
    {"weap3", CLF_ACTION, DDBC_NORMAL, 0, 0, 0},
    {"weapon4", CLF_ACTION, DDBC_NORMAL, '4', 0, 0},
    {"weapon5", CLF_ACTION, DDBC_NORMAL, '5', 0, 0},
    {"weapon6", CLF_ACTION, DDBC_NORMAL, '6', 0, 0},
    {"weapon7", CLF_ACTION, DDBC_NORMAL, '7', 0, 0},
    {"weapon8", CLF_ACTION, DDBC_NORMAL, '8', 0, 0},
    {"weapon9", CLF_ACTION, DDBC_NORMAL, '9', 0, 0},
    {"nextwpn", CLF_ACTION, DDBC_NORMAL, 0, 0, 0},

    {"prevwpn", CLF_ACTION, DDBC_NORMAL, 0, 0, 0},
    {"mlook", CLF_ACTION, DDBC_NORMAL, 'm', 0, 0},
    {"jlook", CLF_ACTION, DDBC_NORMAL, 'j', 0, 0},
    {"lookup", CLF_ACTION, DDBC_NORMAL, DDKEY_PGDN, 0, 6},
    {"lookdown", CLF_ACTION, DDBC_NORMAL, DDKEY_DEL, 0, 7},
    {"lookcntr", CLF_ACTION, DDBC_NORMAL, DDKEY_END, 0, 0},
    {"jump", CLF_ACTION, DDBC_NORMAL, 0, 0, 0},
    {"demostop", CLF_ACTION, DDBC_NORMAL, 'o', 0, 0},

    // Menu hotkeys (default: F1 - F12).
    /*28 */ {"HelpScreen", 0, DDBC_NORMAL, DDKEY_F1, 0, 0},
    {"SaveGame", 0, DDBC_NORMAL, DDKEY_F2, 0, 0},

    {"LoadGame", 0, DDBC_NORMAL, DDKEY_F3, 0, 0},
    {"SoundMenu", 0, DDBC_NORMAL, DDKEY_F4, 0, 0},
    {"QuickSave", 0, DDBC_NORMAL, DDKEY_F6, 0, 0},
    {"EndGame", 0, DDBC_NORMAL, DDKEY_F7, 0, 0},
    {"ToggleMsgs", 0, DDBC_NORMAL, DDKEY_F8, 0, 0},
    {"QuickLoad", 0, DDBC_NORMAL, DDKEY_F9, 0, 0},
    {"quit", 0, DDBC_NORMAL, DDKEY_F10, 0, 0},
    {"ToggleGamma", 0, DDBC_NORMAL, DDKEY_F11, 0, 0},
    {"spy", 0, DDBC_NORMAL, DDKEY_F12, 0, 0},

    // Screen controls.
    {"viewsize -", CLF_REPEAT, DDBC_NORMAL, '-', 0, 0},
    {"viewsize +", CLF_REPEAT, DDBC_NORMAL, '=', 0, 0},
    {"sbsize -", CLF_REPEAT, DDBC_NORMAL, 0, 0, 0},
    {"sbsize +", CLF_REPEAT, DDBC_NORMAL, 0, 0, 0},

    // Misc.
    {"pause", 0, DDBC_NORMAL, DDKEY_PAUSE, 0, 0},
    {"screenshot", 0, DDBC_NORMAL, 0, 0, 0},
    {"beginchat", 0, DDBC_NORMAL, 't', 0, 0},
    {"beginchat 0", 0, DDBC_NORMAL, 'g', 0, 0},
    {"beginchat 1", 0, DDBC_NORMAL, 'i', 0, 0},
    {"beginchat 2", 0, DDBC_NORMAL, 'b', 0, 0},
    {"beginchat 3", 0, DDBC_NORMAL, 'r', 0, 0},
    {"msgrefresh", 0, DDBC_NORMAL, DDKEY_ENTER, 0, 0},

    // More weapons.
    {"weapon1", CLF_ACTION, DDBC_NORMAL, '1', 0, 0},
    {"weapon3", CLF_ACTION, DDBC_NORMAL, '3', 0, 0},

    {"automap", 0, DDBC_NORMAL, DDKEY_TAB, 0, 0},
    {"follow", 0, GBC_CLASS1, 'f', 0, 0},
    {"rotate", 0, GBC_CLASS1, 'r', 0, 0},
    {"grid", 0, GBC_CLASS1, 'g', 0, 0},
    {"mzoomin", CLF_ACTION, GBC_CLASS1, '=', 0, 0},
    {"mzoomout", CLF_ACTION, GBC_CLASS1, '-', 0, 0},
    {"zoommax", 0, GBC_CLASS1, '0', 0, 0},
    {"addmark", 0, GBC_CLASS1, 'm', 0, 0},
    {"clearmarks", 0, GBC_CLASS1, 'c', 0, 0},
    {"mpanup", CLF_ACTION, GBC_CLASS2, DDKEY_UPARROW, 0, 0},
    {"mpandown", CLF_ACTION, GBC_CLASS2, DDKEY_DOWNARROW, 0, 0},
    {"mpanleft", CLF_ACTION, GBC_CLASS2, DDKEY_LEFTARROW, 0, 0},
    {"mpanright", CLF_ACTION, GBC_CLASS2, DDKEY_RIGHTARROW, 0, 0},

    // Menu actions.
    {"menuup", CLF_REPEAT, GBC_CLASS3, DDKEY_UPARROW, 0, 0},
    {"menudown", CLF_REPEAT, GBC_CLASS3, DDKEY_DOWNARROW, 0, 0},
    {"menuleft", CLF_REPEAT, GBC_CLASS3, DDKEY_LEFTARROW, 0, 0},
    {"menuright", CLF_REPEAT, GBC_CLASS3, DDKEY_RIGHTARROW, 0, 0},
    {"menuselect", 0, GBC_CLASS3, DDKEY_ENTER, 0, 0},
    {"menucancel", 0, GBC_CLASS3, DDKEY_BACKSPACE, 0, 0},
    {"menu", 0, GBC_MENUHOTKEY, DDKEY_ESCAPE, 0, 0},

    // More chat actions.
    {"chatcomplete", 0, GBC_CHAT, DDKEY_ENTER, 0, 0},
    {"chatcancel", 0, GBC_CHAT, DDKEY_ESCAPE, 0, 0},
    {"chatsendmacro 0", 0, GBC_CHAT, DDKEY_F1, 0, 0},
    {"chatsendmacro 1", 0, GBC_CHAT, DDKEY_F2, 0, 0},
    {"chatsendmacro 2", 0, GBC_CHAT, DDKEY_F3, 0, 0},
    {"chatsendmacro 3", 0, GBC_CHAT, DDKEY_F4, 0, 0},
    {"chatsendmacro 4", 0, GBC_CHAT, DDKEY_F5, 0, 0},
    {"chatsendmacro 5", 0, GBC_CHAT, DDKEY_F6, 0, 0},
    {"chatsendmacro 6", 0, GBC_CHAT, DDKEY_F7, 0, 0},
    {"chatsendmacro 7", 0, GBC_CHAT, DDKEY_F8, 0, 0},
    {"chatsendmacro 8", 0, GBC_CHAT, DDKEY_F9, 0, 0},
    {"chatsendmacro 9", 0, GBC_CHAT, DDKEY_F10, 0, 0},
    {"chatdelete", 0, GBC_CHAT, DDKEY_BACKSPACE, 0, 0},

    {"messageyes", 0, GBC_MESSAGE, 'y', 0, 0},
    {"messageno", 0, GBC_MESSAGE, 'n', 0, 0},
    {"messagecancel", 0, GBC_MESSAGE, DDKEY_ESCAPE, 0, 0},

    // More movement controls
    {"flyup", CLF_ACTION, DDBC_NORMAL, DDKEY_PGUP, 0, 8},
    {"flydown", CLF_ACTION, DDBC_NORMAL, DDKEY_INS, 0, 9},
    {"falldown", CLF_ACTION, DDBC_NORMAL, DDKEY_HOME, 0, 0},
    {"", 0, 0, 0, 0, 0}             // terminator
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------
