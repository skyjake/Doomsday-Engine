/* $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * Controls: Menu definitions
 */

#ifndef __JDOOM_CONTROLS__
#define __JDOOM_CONTROLS__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "jDoom/Mn_def.h"
#include "jDoom/D_Action.h"

#define CTLCFG_TYPE void

CTLCFG_TYPE SCControlConfig(int option, void *data);

// Control flags.
#define CLF_ACTION      0x1     // The control is an action (+/- in front).
#define CLF_REPEAT      0x2     // Bind down + repeat.

typedef struct {
    char   *command;            // The command to execute.
    int     flags;
    int     bindClass;          // Class it should be bound into
    int     defKey;             //
    int     defMouse;           // Zero means there is no default.
    int     defJoy;             //
} Control_t;

// Game registered bindClasses
enum {
    GBC_CLASS1 = NUM_DDBINDCLASSES,
    GBC_CLASS2,
    GBC_CLASS3,
    GBC_MENUHOTKEY,
    GBC_CHAT,
    GBC_MESSAGE
};

//
// !!! Add new controls to the end, the existing indices must remain unchanged !!!
//
const static Control_t controls[] = {
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

extern const Control_t *grabbing;

#define NUM_CONTROLS_ITEMS 105

static const MenuItem_t ControlsItems[] = {
    {ITT_EMPTY, 0, "PLAYER ACTIONS", NULL, 0},
    {ITT_EFUNC, 0, "LEFT :", SCControlConfig, A_TURNLEFT},
    {ITT_EFUNC, 0, "RIGHT :", SCControlConfig, A_TURNRIGHT},
    {ITT_EFUNC, 0, "FORWARD :", SCControlConfig, A_FORWARD},
    {ITT_EFUNC, 0, "BACKWARD :", SCControlConfig, A_BACKWARD},
    {ITT_EFUNC, 0, "STRAFE LEFT :", SCControlConfig, A_STRAFELEFT},
    {ITT_EFUNC, 0, "STRAFE RIGHT :", SCControlConfig, A_STRAFERIGHT},
    {ITT_EFUNC, 0, "FIRE :", SCControlConfig, A_FIRE},
    {ITT_EFUNC, 0, "USE :", SCControlConfig, A_USE},
    {ITT_EFUNC, 0, "JUMP : ", SCControlConfig, A_JUMP},
    {ITT_EFUNC, 0, "STRAFE :", SCControlConfig, A_STRAFE},
    {ITT_EFUNC, 0, "SPEED :", SCControlConfig, A_SPEED},
    {ITT_EFUNC, 0, "FLY UP :", SCControlConfig, 89},
    {ITT_EFUNC, 0, "FLY DOWN :", SCControlConfig, 90},
    {ITT_EFUNC, 0, "FALL DOWN :", SCControlConfig, 91},
    {ITT_EFUNC, 0, "LOOK UP :", SCControlConfig, A_LOOKUP},
    {ITT_EFUNC, 0, "LOOK DOWN :", SCControlConfig, A_LOOKDOWN},
    {ITT_EFUNC, 0, "LOOK CENTER :", SCControlConfig, A_LOOKCENTER},
    {ITT_EFUNC, 0, "MOUSE LOOK :", SCControlConfig, A_MLOOK},
    {ITT_EFUNC, 0, "JOYSTICK LOOK :", SCControlConfig, A_JLOOK},
    {ITT_EFUNC, 0, "NEXT WEAPON :", SCControlConfig, A_NEXTWEAPON},
    {ITT_EFUNC, 0, "PREV WEAPON :", SCControlConfig, A_PREVIOUSWEAPON},
    {ITT_EFUNC, 0, "FIST/CHAINSAW :", SCControlConfig, 51},
    {ITT_EFUNC, 0, "FIST :", SCControlConfig, A_WEAPON1},
    {ITT_EFUNC, 0, "CHAINSAW :", SCControlConfig, A_WEAPON8},
    {ITT_EFUNC, 0, "PISTOL :", SCControlConfig, A_WEAPON2},
    {ITT_EFUNC, 0, "SUPER SG/SHOTGUN :", SCControlConfig, 52},
    {ITT_EFUNC, 0, "SHOTGUN :", SCControlConfig, A_WEAPON3},
    {ITT_EFUNC, 0, "SUPER SHOTGUN :", SCControlConfig, A_WEAPON9},
    {ITT_EFUNC, 0, "CHAINGUN :", SCControlConfig, A_WEAPON4},
    {ITT_EFUNC, 0, "ROCKET LAUNCHER :", SCControlConfig, A_WEAPON5},
    {ITT_EFUNC, 0, "PLASMA RIFLE :", SCControlConfig, A_WEAPON6},
    {ITT_EFUNC, 0, "BFG 9000 :", SCControlConfig, A_WEAPON7},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "MENU", NULL, 0},
    {ITT_EFUNC, 0, "OPEN/CLOSE MENU :", SCControlConfig, 72},
    {ITT_EFUNC, 0, "Cursor Up :", SCControlConfig, 66},
    {ITT_EFUNC, 0, "Cursor Down :", SCControlConfig, 67},
    {ITT_EFUNC, 0, "Cursor Left :", SCControlConfig, 68},
    {ITT_EFUNC, 0, "Cursor Right :", SCControlConfig, 69},
    {ITT_EFUNC, 0, "Accept :", SCControlConfig, 70},
    {ITT_EFUNC, 0, "Cancel :", SCControlConfig, 71},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "MENU HOTKEYS", NULL, 0},
    {ITT_EFUNC, 0, "HELP :", SCControlConfig, 28},
    {ITT_EFUNC, 0, "SOUND MENU :", SCControlConfig, 31},
    {ITT_EFUNC, 0, "LOAD GAME :", SCControlConfig, 30},
    {ITT_EFUNC, 0, "SAVE GAME :", SCControlConfig, 29},
    {ITT_EFUNC, 0, "QUICK LOAD :", SCControlConfig, 35},
    {ITT_EFUNC, 0, "QUICK SAVE :", SCControlConfig, 32},
    {ITT_EFUNC, 0, "END GAME :", SCControlConfig, 33},
    {ITT_EFUNC, 0, "QUIT :", SCControlConfig, 36},
    {ITT_EFUNC, 0, "MESSAGES ON/OFF:", SCControlConfig, 34},
    {ITT_EFUNC, 0, "GAMMA CORRECTION :", SCControlConfig, 37},
    {ITT_EFUNC, 0, "SPY MODE :", SCControlConfig, 38},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "SCREEN", NULL, 0},
    {ITT_EFUNC, 0, "SMALLER VIEW :", SCControlConfig, 39},
    {ITT_EFUNC, 0, "LARGER VIEW :", SCControlConfig, 40},
    {ITT_EFUNC, 0, "SMALLER STATBAR :", SCControlConfig, 41},
    {ITT_EFUNC, 0, "LARGER STATBAR :", SCControlConfig, 42},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "AUTOMAP", NULL, 0},
    {ITT_EFUNC, 0, "OPEN/CLOSE MAP :", SCControlConfig, 53},
    {ITT_EFUNC, 0, "PAN UP :", SCControlConfig, 62},
    {ITT_EFUNC, 0, "PAN DOWN :", SCControlConfig, 63},
    {ITT_EFUNC, 0, "PAN LEFT :", SCControlConfig, 64},
    {ITT_EFUNC, 0, "PAN RIGHT :", SCControlConfig, 65},
    {ITT_EFUNC, 0, "FOLLOW MODE :", SCControlConfig, 54},
    {ITT_EFUNC, 0, "ROTATE MODE :", SCControlConfig, 55},
    {ITT_EFUNC, 0, "TOGGLE GRID :", SCControlConfig, 56},
    {ITT_EFUNC, 0, "ZOOM IN :", SCControlConfig, 57},
    {ITT_EFUNC, 0, "ZOOM OUT :", SCControlConfig, 58},
    {ITT_EFUNC, 0, "ZOOM EXTENTS :", SCControlConfig, 59},
    {ITT_EFUNC, 0, "ADD MARK :", SCControlConfig, 60},
    {ITT_EFUNC, 0, "CLEAR MARKS :", SCControlConfig, 61},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "CHATMODE", NULL, 0},
    {ITT_EFUNC, 0, "OPEN CHAT :", SCControlConfig, 45},
    {ITT_EFUNC, 0, "GREEN CHAT :", SCControlConfig, 46},
    {ITT_EFUNC, 0, "INDIGO CHAT :", SCControlConfig, 47},
    {ITT_EFUNC, 0, "BROWN CHAT :", SCControlConfig, 48},
    {ITT_EFUNC, 0, "RED CHAT :", SCControlConfig, 49},
    {ITT_EFUNC, 0, "COMPLETE :", SCControlConfig, 73},
    {ITT_EFUNC, 0, "DELETE :", SCControlConfig, 85},
    {ITT_EFUNC, 0, "CANCEL CHAT :", SCControlConfig, 74},
    {ITT_EFUNC, 0, "MSG REFRESH :", SCControlConfig, 50},
    {ITT_EFUNC, 0, "MACRO 0:", SCControlConfig, 75},
    {ITT_EFUNC, 0, "MACRO 1:", SCControlConfig, 76},
    {ITT_EFUNC, 0, "MACRO 2:", SCControlConfig, 77},
    {ITT_EFUNC, 0, "MACRO 3:", SCControlConfig, 78},
    {ITT_EFUNC, 0, "MACRO 4:", SCControlConfig, 79},
    {ITT_EFUNC, 0, "MACRO 5:", SCControlConfig, 80},
    {ITT_EFUNC, 0, "MACRO 6:", SCControlConfig, 81},
    {ITT_EFUNC, 0, "MACRO 7:", SCControlConfig, 82},
    {ITT_EFUNC, 0, "MACRO 8:", SCControlConfig, 83},
    {ITT_EFUNC, 0, "MACRO 9:", SCControlConfig, 84},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "MISCELLANEOUS", NULL, 0},
    {ITT_EFUNC, 0, "PAUSE :", SCControlConfig, 43},
    {ITT_EFUNC, 0, "SCREENSHOT :", SCControlConfig, 44},
    {ITT_EFUNC, 0, "MESSAGE YES :", SCControlConfig, 86},
    {ITT_EFUNC, 0, "MESSAGE NO :", SCControlConfig, 87},
    {ITT_EFUNC, 0, "MESSAGE CANCEL :", SCControlConfig, 88}
};

void G_DefaultBindings(void);
void M_DrawControlsMenu(void);
void G_BindClassRegistration(void);

#endif
