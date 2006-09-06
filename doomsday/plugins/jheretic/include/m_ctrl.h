/**\file
 * Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 * Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/*
 * Controls menu definition
 */
#ifndef __JHERETIC_CONTROLS__
#define __JHERETIC_CONTROLS__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "jheretic.h"
#include "mn_def.h"
#include "g_controls.h"

extern const Control_t *grabbing;

#define NUM_CONTROLS_ITEMS 119

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
    {ITT_EFUNC, 0, "JUMP : ", SCControlConfig, 61 /*A_JUMP */ },
    {ITT_EFUNC, 0, "STRAFE :", SCControlConfig, A_STRAFE},
    {ITT_EFUNC, 0, "SPEED :", SCControlConfig, A_SPEED},
    {ITT_EFUNC, 0, "FLY UP :", SCControlConfig, A_FLYUP},
    {ITT_EFUNC, 0, "FLY DOWN :", SCControlConfig, A_FLYDOWN},
    {ITT_EFUNC, 0, "FALL DOWN :", SCControlConfig, A_FLYCENTER},
    {ITT_EFUNC, 0, "LOOK UP :", SCControlConfig, A_LOOKUP},
    {ITT_EFUNC, 0, "LOOK DOWN :", SCControlConfig, A_LOOKDOWN},
    {ITT_EFUNC, 0, "LOOK CENTER :", SCControlConfig, A_LOOKCENTER},
    {ITT_EFUNC, 0, "MOUSE LOOK :", SCControlConfig, A_MLOOK},
    {ITT_EFUNC, 0, "JOYSTICK LOOK :", SCControlConfig, A_JLOOK},
    {ITT_EFUNC, 0, "NEXT WEAPON :", SCControlConfig, A_NEXTWEAPON},
    {ITT_EFUNC, 0, "PREV WEAPON :", SCControlConfig, A_PREVIOUSWEAPON},
    {ITT_EFUNC, 0, "STAFF/GUANTLETS :", SCControlConfig, 42},
    {ITT_EFUNC, 0, "STAFF :", SCControlConfig, A_WEAPON1},
    {ITT_EFUNC, 0, "ELVENWAND :", SCControlConfig, A_WEAPON2},
    {ITT_EFUNC, 0, "CROSSBOW :", SCControlConfig, A_WEAPON3},
    {ITT_EFUNC, 0, "DRAGON CLAW :", SCControlConfig, A_WEAPON4},
    {ITT_EFUNC, 0, "HELLSTAFF :", SCControlConfig, A_WEAPON5},
    {ITT_EFUNC, 0, "PHOENIX ROD :", SCControlConfig, A_WEAPON6},
    {ITT_EFUNC, 0, "FIREMACE :", SCControlConfig, A_WEAPON7},
    {ITT_EFUNC, 0, "PANIC :", SCControlConfig, A_PANIC},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "ARTIFACTS", NULL, 0},
    {ITT_EFUNC, 0, "INVINCIBILITY :", SCControlConfig, A_INVULNERABILITY},
    {ITT_EFUNC, 0, "SHADOWSPHERE :", SCControlConfig, A_INVISIBILITY},
    {ITT_EFUNC, 0, "QUARTZ FLASK :", SCControlConfig, A_HEALTH},
    {ITT_EFUNC, 0, "MYSTIC URN :", SCControlConfig, A_SUPERHEALTH},
    {ITT_EFUNC, 0, "TOME OF POWER:", SCControlConfig, A_TOMEOFPOWER},
    {ITT_EFUNC, 0, "TORCH :", SCControlConfig, A_TORCH},
    {ITT_EFUNC, 0, "TIME BOMB :", SCControlConfig, A_FIREBOMB},
    {ITT_EFUNC, 0, "MORPH OVUM :", SCControlConfig, A_EGG},
    {ITT_EFUNC, 0, "WINGS OF WRATH :", SCControlConfig, A_FLY},
    {ITT_EFUNC, 0, "CHAOS DEVICE :", SCControlConfig, A_TELEPORT},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "INVENTORY", NULL, 0},
    {ITT_EFUNC, 0, "INVENTORY LEFT :", SCControlConfig, 54},
    {ITT_EFUNC, 0, "INVENTORY RIGHT :", SCControlConfig, 55},
    {ITT_EFUNC, 0, "USE ARTIFACT :", SCControlConfig, A_USEARTIFACT},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "MENU", NULL, 0},
    {ITT_EFUNC, 0, "OPEN/CLOSE MENU :", SCControlConfig, 87},
    {ITT_EFUNC, 0, "Cursor Up :", SCControlConfig, 81},
    {ITT_EFUNC, 0, "Cursor Down :", SCControlConfig, 82},
    {ITT_EFUNC, 0, "Cursor Left :", SCControlConfig, 83},
    {ITT_EFUNC, 0, "Cursor Right :", SCControlConfig, 84},
    {ITT_EFUNC, 0, "Accept :", SCControlConfig, 85},
    {ITT_EFUNC, 0, "Cancel :", SCControlConfig, 86},
    {ITT_EMPTY, 0, "MENU HOTKEYS", NULL, 0},
    {ITT_EFUNC, 0, "INFO :", SCControlConfig, 43},
    {ITT_EFUNC, 0, "SOUND MENU :", SCControlConfig, 46},
    {ITT_EFUNC, 0, "LOAD GAME :", SCControlConfig, 44},
    {ITT_EFUNC, 0, "SAVE GAME :", SCControlConfig, 45},
    {ITT_EFUNC, 0, "QUICK LOAD :", SCControlConfig, 50},
    {ITT_EFUNC, 0, "QUICK SAVE :", SCControlConfig, 47},
    {ITT_EFUNC, 0, "END GAME :", SCControlConfig, 48},
    {ITT_EFUNC, 0, "QUIT :", SCControlConfig, 51},
    {ITT_EFUNC, 0, "MESSAGES ON/OFF:", SCControlConfig, 49},
    {ITT_EFUNC, 0, "GAMMA CORRECTION :", SCControlConfig, 52},
    {ITT_EFUNC, 0, "SPY MODE :", SCControlConfig, 53},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "SCREEN", NULL, 0},
    {ITT_EFUNC, 0, "SMALLER VIEW :", SCControlConfig, 57},
    {ITT_EFUNC, 0, "LARGER VIEW :", SCControlConfig, 56},
    {ITT_EFUNC, 0, "SMALLER STATBAR :", SCControlConfig, 59},
    {ITT_EFUNC, 0, "LARGER STATBAR :", SCControlConfig, 58},
    {ITT_EMPTY, 0, NULL, NULL, 0},

    {ITT_EMPTY, 0, "AUTOMAP", NULL, 0},
    {ITT_EFUNC, 0, "OPEN/CLOSE MAP :", SCControlConfig, 68},
    {ITT_EFUNC, 0, "PAN UP :", SCControlConfig, 77},
    {ITT_EFUNC, 0, "PAN DOWN :", SCControlConfig, 78},
    {ITT_EFUNC, 0, "PAN LEFT :", SCControlConfig, 79},
    {ITT_EFUNC, 0, "PAN RIGHT :", SCControlConfig, 80},
    {ITT_EFUNC, 0, "FOLLOW MODE :", SCControlConfig, 69},
    {ITT_EFUNC, 0, "ROTATE MODE :", SCControlConfig, 70},
    {ITT_EFUNC, 0, "TOGGLE GRID :", SCControlConfig, 71},
    {ITT_EFUNC, 0, "ZOOM IN :", SCControlConfig, 72},
    {ITT_EFUNC, 0, "ZOOM OUT :", SCControlConfig, 73},
    {ITT_EFUNC, 0, "ZOOM EXTENTS :", SCControlConfig, 74},
    {ITT_EFUNC, 0, "ADD MARK :", SCControlConfig, 75},
    {ITT_EFUNC, 0, "CLEAR MARKS :", SCControlConfig, 76},

    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "CHATMODE", NULL, 0},
    {ITT_EFUNC, 0, "OPEN CHAT :", SCControlConfig, 62},
    {ITT_EFUNC, 0, "GREEN CHAT :", SCControlConfig, 63},
    {ITT_EFUNC, 0, "YELLOW CHAT :", SCControlConfig, 64},
    {ITT_EFUNC, 0, "RED CHAT :", SCControlConfig, 65},
    {ITT_EFUNC, 0, "BLUE CHAT :", SCControlConfig, 66},
    {ITT_EFUNC, 0, "COMPLETE :", SCControlConfig, 89},
    {ITT_EFUNC, 0, "DELETE :", SCControlConfig, 101},
    {ITT_EFUNC, 0, "CANCEL CHAT :", SCControlConfig, 90},
    {ITT_EFUNC, 0, "MSG REFRESH :", SCControlConfig, 88},
    {ITT_EFUNC, 0, "MACRO 0:", SCControlConfig, 91},
    {ITT_EFUNC, 0, "MACRO 1:", SCControlConfig, 92},
    {ITT_EFUNC, 0, "MACRO 2:", SCControlConfig, 93},
    {ITT_EFUNC, 0, "MACRO 3:", SCControlConfig, 94},
    {ITT_EFUNC, 0, "MACRO 4:", SCControlConfig, 95},
    {ITT_EFUNC, 0, "MACRO 5:", SCControlConfig, 96},
    {ITT_EFUNC, 0, "MACRO 6:", SCControlConfig, 97},
    {ITT_EFUNC, 0, "MACRO 7:", SCControlConfig, 98},
    {ITT_EFUNC, 0, "MACRO 8:", SCControlConfig, 99},
    {ITT_EFUNC, 0, "MACRO 9:", SCControlConfig, 100},

    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "MISCELLANEOUS", NULL, 0},
    {ITT_EFUNC, 0, "SCREENSHOT :", SCControlConfig, 67},
    {ITT_EFUNC, 0, "PAUSE :", SCControlConfig, 60},
    {ITT_EFUNC, 0, "STOP DEMO :", SCControlConfig, A_STOPDEMO},
    {ITT_EFUNC, 0, "MESSAGE YES :", SCControlConfig, 102},
    {ITT_EFUNC, 0, "MESSAGE NO :", SCControlConfig, 103},
    {ITT_EFUNC, 0, "MESSAGE CANCEL :", SCControlConfig, 104}
};

void G_DefaultBindings(void);
void M_DrawControlsMenu(void);
void G_BindClassRegistration(void);

#endif
