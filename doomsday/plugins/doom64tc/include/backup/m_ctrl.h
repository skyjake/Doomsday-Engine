/* $Id: m_ctrl.h 3318 2006-06-13 02:18:47Z danij $
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

#include "mn_def.h"
#include "d_action.h"
#include "g_controls.h"

static const menuobject_t ControlsItems[] = {
    {MOT_TEXT, ITT_EMPTY, 0, "PLAYER ACTIONS", NULL, 0},
    {MOT_TEXT, ITT_EFUNC, 0, "LEFT :", SCControlConfig, A_TURNLEFT},
    {MOT_TEXT, ITT_EFUNC, 0, "RIGHT :", SCControlConfig, A_TURNRIGHT},
    {MOT_TEXT, ITT_EFUNC, 0, "FORWARD :", SCControlConfig, A_FORWARD},
    {MOT_TEXT, ITT_EFUNC, 0, "BACKWARD :", SCControlConfig, A_BACKWARD},
    {MOT_TEXT, ITT_EFUNC, 0, "STRAFE LEFT :", SCControlConfig, A_STRAFELEFT},
    {MOT_TEXT, ITT_EFUNC, 0, "STRAFE RIGHT :", SCControlConfig, A_STRAFERIGHT},
    {MOT_TEXT, ITT_EFUNC, 0, "FIRE :", SCControlConfig, A_FIRE},
    {MOT_TEXT, ITT_EFUNC, 0, "USE :", SCControlConfig, A_USE},
    {MOT_TEXT, ITT_EFUNC, 0, "JUMP : ", SCControlConfig, A_JUMP},
    {MOT_TEXT, ITT_EFUNC, 0, "STRAFE :", SCControlConfig, A_STRAFE},
    {MOT_TEXT, ITT_EFUNC, 0, "SPEED :", SCControlConfig, A_SPEED},
    {MOT_TEXT, ITT_EFUNC, 0, "FLY UP :", SCControlConfig, 89},
    {MOT_TEXT, ITT_EFUNC, 0, "FLY DOWN :", SCControlConfig, 90},
    {MOT_TEXT, ITT_EFUNC, 0, "FALL DOWN :", SCControlConfig, 91},
    {MOT_TEXT, ITT_EFUNC, 0, "LOOK UP :", SCControlConfig, A_LOOKUP},
    {MOT_TEXT, ITT_EFUNC, 0, "LOOK DOWN :", SCControlConfig, A_LOOKDOWN},
    {MOT_TEXT, ITT_EFUNC, 0, "LOOK CENTER :", SCControlConfig, A_LOOKCENTER},
    {MOT_TEXT, ITT_EFUNC, 0, "MOUSE LOOK :", SCControlConfig, A_MLOOK},
    {MOT_TEXT, ITT_EFUNC, 0, "JOYSTICK LOOK :", SCControlConfig, A_JLOOK},
    {MOT_TEXT, ITT_EFUNC, 0, "NEXT WEAPON :", SCControlConfig, A_NEXTWEAPON},
    {MOT_TEXT, ITT_EFUNC, 0, "PREV WEAPON :", SCControlConfig, A_PREVIOUSWEAPON},
    {MOT_TEXT, ITT_EFUNC, 0, "FIST/CHAINSAW :", SCControlConfig, 51},
    {MOT_TEXT, ITT_EFUNC, 0, "FIST :", SCControlConfig, A_WEAPON1},
    {MOT_TEXT, ITT_EFUNC, 0, "CHAINSAW :", SCControlConfig, A_WEAPON8},
    {MOT_TEXT, ITT_EFUNC, 0, "PISTOL :", SCControlConfig, A_WEAPON2},
    {MOT_TEXT, ITT_EFUNC, 0, "SUPER SG/SHOTGUN :", SCControlConfig, 52},
    {MOT_TEXT, ITT_EFUNC, 0, "SHOTGUN :", SCControlConfig, A_WEAPON3},
    {MOT_TEXT, ITT_EFUNC, 0, "SUPER SHOTGUN :", SCControlConfig, A_WEAPON9},
    {MOT_TEXT, ITT_EFUNC, 0, "CHAINGUN :", SCControlConfig, A_WEAPON4},
    {MOT_TEXT, ITT_EFUNC, 0, "ROCKET LAUNCHER :", SCControlConfig, A_WEAPON5},
    {MOT_TEXT, ITT_EFUNC, 0, "PLASMA RIFLE :", SCControlConfig, A_WEAPON6},
    {MOT_TEXT, ITT_EFUNC, 0, "BFG 9000 :", SCControlConfig, A_WEAPON7},
    {MOT_NONE, ITT_EMPTY, 0, NULL, NULL, 0},
    {MOT_NONE, ITT_EMPTY, 0, NULL, NULL, 0},
    {MOT_TEXT, ITT_EMPTY, 0, "MENU", NULL, 0},
    {MOT_TEXT, ITT_EFUNC, 0, "OPEN/CLOSE MENU :", SCControlConfig, 72},
    {MOT_TEXT, ITT_EFUNC, 0, "Cursor Up :", SCControlConfig, 66},
    {MOT_TEXT, ITT_EFUNC, 0, "Cursor Down :", SCControlConfig, 67},
    {MOT_TEXT, ITT_EFUNC, 0, "Cursor Left :", SCControlConfig, 68},
    {MOT_TEXT, ITT_EFUNC, 0, "Cursor Right :", SCControlConfig, 69},
    {MOT_TEXT, ITT_EFUNC, 0, "Next page :", SCControlConfig, 93},
    {MOT_TEXT, ITT_EFUNC, 0, "Previous page :", SCControlConfig, 94},
    {MOT_TEXT, ITT_EFUNC, 0, "Accept :", SCControlConfig, 70},
    {MOT_TEXT, ITT_EFUNC, 0, "Cancel :", SCControlConfig, 71},
    {MOT_NONE, ITT_EMPTY, 0, NULL, NULL, 0},
    {MOT_TEXT, ITT_EMPTY, 0, "MENU HOTKEYS", NULL, 0},
    {MOT_TEXT, ITT_EFUNC, 0, "HELP :", SCControlConfig, 28},
    {MOT_TEXT, ITT_EFUNC, 0, "SOUND MENU :", SCControlConfig, 31},
    {MOT_TEXT, ITT_EFUNC, 0, "LOAD GAME :", SCControlConfig, 30},
    {MOT_TEXT, ITT_EFUNC, 0, "SAVE GAME :", SCControlConfig, 29},
    {MOT_TEXT, ITT_EFUNC, 0, "QUICK LOAD :", SCControlConfig, 35},
    {MOT_TEXT, ITT_EFUNC, 0, "QUICK SAVE :", SCControlConfig, 32},
    {MOT_TEXT, ITT_EFUNC, 0, "END GAME :", SCControlConfig, 33},
    {MOT_TEXT, ITT_EFUNC, 0, "QUIT :", SCControlConfig, 36},
    {MOT_TEXT, ITT_EFUNC, 0, "MESSAGES ON/OFF:", SCControlConfig, 34},
    {MOT_TEXT, ITT_EFUNC, 0, "GAMMA CORRECTION :", SCControlConfig, 37},
    {MOT_TEXT, ITT_EFUNC, 0, "SPY MODE :", SCControlConfig, 38},
    {MOT_NONE, ITT_EMPTY, 0, NULL, NULL, 0},
    {MOT_TEXT, ITT_EMPTY, 0, "SCREEN", NULL, 0},
    {MOT_TEXT, ITT_EFUNC, 0, "SMALLER VIEW :", SCControlConfig, 39},
    {MOT_TEXT, ITT_EFUNC, 0, "LARGER VIEW :", SCControlConfig, 40},
    {MOT_TEXT, ITT_EFUNC, 0, "SMALLER STATBAR :", SCControlConfig, 41},
    {MOT_TEXT, ITT_EFUNC, 0, "LARGER STATBAR :", SCControlConfig, 42},
    {MOT_TEXT, ITT_EFUNC, 0, "SHOW HUD :", SCControlConfig, 92},
    {MOT_NONE, ITT_EMPTY, 0, NULL, NULL, 0},
    {MOT_TEXT, ITT_EMPTY, 0, "AUTOMAP", NULL, 0},
    {MOT_TEXT, ITT_EFUNC, 0, "OPEN/CLOSE MAP :", SCControlConfig, 53},
    {MOT_TEXT, ITT_EFUNC, 0, "PAN UP :", SCControlConfig, 62},
    {MOT_TEXT, ITT_EFUNC, 0, "PAN DOWN :", SCControlConfig, 63},
    {MOT_TEXT, ITT_EFUNC, 0, "PAN LEFT :", SCControlConfig, 64},
    {MOT_TEXT, ITT_EFUNC, 0, "PAN RIGHT :", SCControlConfig, 65},
    {MOT_TEXT, ITT_EFUNC, 0, "FOLLOW MODE :", SCControlConfig, 54},
    {MOT_TEXT, ITT_EFUNC, 0, "ROTATE MODE :", SCControlConfig, 55},
    {MOT_TEXT, ITT_EFUNC, 0, "TOGGLE GRID :", SCControlConfig, 56},
    {MOT_TEXT, ITT_EFUNC, 0, "ZOOM IN :", SCControlConfig, 57},
    {MOT_TEXT, ITT_EFUNC, 0, "ZOOM OUT :", SCControlConfig, 58},
    {MOT_TEXT, ITT_EFUNC, 0, "ZOOM EXTENTS :", SCControlConfig, 59},
    {MOT_TEXT, ITT_EFUNC, 0, "ADD MARK :", SCControlConfig, 60},
    {MOT_TEXT, ITT_EFUNC, 0, "CLEAR MARKS :", SCControlConfig, 61},
    {MOT_NONE, ITT_EMPTY, 0, NULL, NULL, 0},
    {MOT_TEXT, ITT_EMPTY, 0, "CHATMODE", NULL, 0},
    {MOT_TEXT, ITT_EFUNC, 0, "OPEN CHAT :", SCControlConfig, 45},
    {MOT_TEXT, ITT_EFUNC, 0, "GREEN CHAT :", SCControlConfig, 46},
    {MOT_TEXT, ITT_EFUNC, 0, "INDIGO CHAT :", SCControlConfig, 47},
    {MOT_TEXT, ITT_EFUNC, 0, "BROWN CHAT :", SCControlConfig, 48},
    {MOT_TEXT, ITT_EFUNC, 0, "RED CHAT :", SCControlConfig, 49},
    {MOT_TEXT, ITT_EFUNC, 0, "COMPLETE :", SCControlConfig, 73},
    {MOT_TEXT, ITT_EFUNC, 0, "DELETE :", SCControlConfig, 85},
    {MOT_TEXT, ITT_EFUNC, 0, "CANCEL CHAT :", SCControlConfig, 74},
    {MOT_TEXT, ITT_EFUNC, 0, "MSG REFRESH :", SCControlConfig, 50},
    {MOT_TEXT, ITT_EFUNC, 0, "MACRO 0:", SCControlConfig, 75},
    {MOT_TEXT, ITT_EFUNC, 0, "MACRO 1:", SCControlConfig, 76},
    {MOT_TEXT, ITT_EFUNC, 0, "MACRO 2:", SCControlConfig, 77},
    {MOT_TEXT, ITT_EFUNC, 0, "MACRO 3:", SCControlConfig, 78},
    {MOT_TEXT, ITT_EFUNC, 0, "MACRO 4:", SCControlConfig, 79},
    {MOT_TEXT, ITT_EFUNC, 0, "MACRO 5:", SCControlConfig, 80},
    {MOT_TEXT, ITT_EFUNC, 0, "MACRO 6:", SCControlConfig, 81},
    {MOT_TEXT, ITT_EFUNC, 0, "MACRO 7:", SCControlConfig, 82},
    {MOT_TEXT, ITT_EFUNC, 0, "MACRO 8:", SCControlConfig, 83},
    {MOT_TEXT, ITT_EFUNC, 0, "MACRO 9:", SCControlConfig, 84},
    {MOT_NONE, ITT_EMPTY, 0, NULL, NULL, 0},
    {MOT_TEXT, ITT_EMPTY, 0, "MISCELLANEOUS", NULL, 0},
    {MOT_TEXT, ITT_EFUNC, 0, "PAUSE :", SCControlConfig, 43},
    {MOT_TEXT, ITT_EFUNC, 0, "SCREENSHOT :", SCControlConfig, 44},
    {MOT_TEXT, ITT_EFUNC, 0, "MESSAGE YES :", SCControlConfig, 86},
    {MOT_TEXT, ITT_EFUNC, 0, "MESSAGE NO :", SCControlConfig, 87},
    {MOT_TEXT, ITT_EFUNC, 0, "MESSAGE CANCEL :", SCControlConfig, 88},
    {MOT_NONE, ITT_NONE}
};

#endif
