/**\file
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
#ifndef __JHEXEN_CONTROLS__
#define __JHEXEN_CONTROLS__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "jhexen.h"
#include "mn_def.h"
#include "g_controls.h"

extern const Control_t *grabbing;

#define NUM_CONTROLS_ITEMS 118

static const MenuItem_t ControlsItems[] = {
    {ITT_EMPTY, 0, "PLAYER ACTIONS", NULL, 0},
    {ITT_EFUNC, 0, "LEFT :", SCControlConfig, A_TURNLEFT},
    {ITT_EFUNC, 0, "RIGHT :", SCControlConfig, A_TURNRIGHT},
    {ITT_EFUNC, 0, "FORWARD :", SCControlConfig, A_FORWARD},
    {ITT_EFUNC, 0, "BACKWARD :", SCControlConfig, A_BACKWARD},
    {ITT_EFUNC, 0, "STRAFE LEFT :", SCControlConfig, A_STRAFELEFT},
    {ITT_EFUNC, 0, "STRAFE RIGHT :", SCControlConfig, A_STRAFERIGHT},
    {ITT_EFUNC, 0, "JUMP :", SCControlConfig, A_JUMP},
    {ITT_EFUNC, 0, "FIRE :", SCControlConfig, A_FIRE},
    {ITT_EFUNC, 0, "USE :", SCControlConfig, A_USE},
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
    {ITT_EFUNC, 0, "WEAPON 1 :", SCControlConfig, A_WEAPON1},
    {ITT_EFUNC, 0, "WEAPON 2 :", SCControlConfig, A_WEAPON2},
    {ITT_EFUNC, 0, "WEAPON 3 :", SCControlConfig, A_WEAPON3},
    {ITT_EFUNC, 0, "WEAPON 4 :", SCControlConfig, A_WEAPON4},
    {ITT_EFUNC, 0, "PANIC :", SCControlConfig, A_PANIC},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "ARTIFACTS", NULL, 0},
    {ITT_EFUNC, 0, "TORCH :", SCControlConfig, A_TORCH},
    {ITT_EFUNC, 0, "QUARTZ FLASK :", SCControlConfig, A_HEALTH},
    {ITT_EFUNC, 0, "MYSTIC URN :", SCControlConfig, A_MYSTICURN},
    {ITT_EFUNC, 0, "KRATER OF MIGHT :", SCControlConfig, A_KRATER},
    {ITT_EFUNC, 0, "BOOTS OF SPEED :", SCControlConfig, A_SPEEDBOOTS},
    {ITT_EFUNC, 0, "REPULSION :", SCControlConfig, A_BLASTRADIUS},
    {ITT_EFUNC, 0, "CHAOS DEVICE :", SCControlConfig, A_TELEPORT},
    {ITT_EFUNC, 0, "BANISHMENT :", SCControlConfig, A_TELEPORTOTHER},
    {ITT_EFUNC, 0, "BOOTS OF SPEED :", SCControlConfig, A_SPEEDBOOTS},
    {ITT_EFUNC, 0, "FLECHETTE :", SCControlConfig, A_POISONBAG},
    {ITT_EFUNC, 0, "DEFENDER :", SCControlConfig, A_INVULNERABILITY},
    {ITT_EFUNC, 0, "DARK SERVANT :", SCControlConfig, A_DARKSERVANT},
    {ITT_EFUNC, 0, "PORKELATOR :", SCControlConfig, A_EGG},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "INVENTORY", NULL, 0},
    {ITT_EFUNC, 0, "INVENTORY LEFT :", SCControlConfig, 52},
    {ITT_EFUNC, 0, "INVENTORY RIGHT :", SCControlConfig, 53},
    {ITT_EFUNC, 0, "USE ARTIFACT :", SCControlConfig, A_USEARTIFACT},
    {ITT_EMPTY, 0, NULL, NULL, 0},

    {ITT_EMPTY, 0, "MENU", NULL, 0},
    {ITT_EFUNC, 0, "OPEN/CLOSE MENU :", SCControlConfig, 84},
    {ITT_EFUNC, 0, "Cursor Up :", SCControlConfig, 78},
    {ITT_EFUNC, 0, "Cursor Down :", SCControlConfig, 79},
    {ITT_EFUNC, 0, "Cursor Left :", SCControlConfig, 80},
    {ITT_EFUNC, 0, "Cursor Right :", SCControlConfig, 81},
    {ITT_EFUNC, 0, "Accept :", SCControlConfig, 82},
    {ITT_EFUNC, 0, "Cancel :", SCControlConfig, 83},

    {ITT_EMPTY, 0, "MENU HOTKEYS", NULL, 0},
    {ITT_EFUNC, 0, "INFO :", SCControlConfig, 40},
    {ITT_EFUNC, 0, "SOUND MENU :", SCControlConfig, 43},
    {ITT_EFUNC, 0, "LOAD GAME :", SCControlConfig, 41},
    {ITT_EFUNC, 0, "SAVE GAME :", SCControlConfig, 42},
    {ITT_EFUNC, 0, "QUICK LOAD :", SCControlConfig, 48},
    {ITT_EFUNC, 0, "QUICK SAVE :", SCControlConfig, 45},
    {ITT_EFUNC, 0, "SUICIDE :", SCControlConfig, 44},
    {ITT_EFUNC, 0, "END GAME :", SCControlConfig, 46},
    {ITT_EFUNC, 0, "QUIT :", SCControlConfig, 49},
    {ITT_EMPTY, 0, NULL, NULL, 0},

    {ITT_EMPTY, 0, "SCREEN", NULL, 0},
    {ITT_EFUNC, 0, "SMALLER VIEW :", SCControlConfig, 55},
    {ITT_EFUNC, 0, "LARGER VIEW :", SCControlConfig, 54},
    {ITT_EFUNC, 0, "SMALLER STATBAR :", SCControlConfig, 57},
    {ITT_EFUNC, 0, "LARGER STATBAR :", SCControlConfig, 56},
    {ITT_EFUNC, 0, "SHOW HUD :", SCControlConfig, 102},
    {ITT_EFUNC, 0, "MESSAGES ON/OFF:", SCControlConfig, 47},
    {ITT_EFUNC, 0, "SPY MODE :", SCControlConfig, 51},
    {ITT_EFUNC, 0, "GAMMA CORRECTION :", SCControlConfig, 50},
    {ITT_EMPTY, 0, NULL, NULL, 0},

    {ITT_EMPTY, 0, "AUTOMAP KEYS", NULL, 0},
    {ITT_EFUNC, 0, "OPEN/COSE MAP :", SCControlConfig, 60},
    {ITT_EFUNC, 0, "PAN UP :", SCControlConfig, 69},
    {ITT_EFUNC, 0, "PAN DOWN :", SCControlConfig, 70},
    {ITT_EFUNC, 0, "PAN LEFT :", SCControlConfig, 71},
    {ITT_EFUNC, 0, "PAN RIGHT :", SCControlConfig, 72},
    {ITT_EFUNC, 0, "FOLLOW MODE :", SCControlConfig, 61},
    {ITT_EFUNC, 0, "ROTATE MODE :", SCControlConfig, 62},
    {ITT_EFUNC, 0, "TOGGLE GRID :", SCControlConfig, 63},
    {ITT_EFUNC, 0, "ZOOM IN :", SCControlConfig, 64},
    {ITT_EFUNC, 0, "ZOOM OUT :", SCControlConfig, 65},
    {ITT_EFUNC, 0, "ZOOM EXTENTS :", SCControlConfig, 66},
    {ITT_EFUNC, 0, "ADD MARK :", SCControlConfig, 67},
    {ITT_EFUNC, 0, "CLEAR MARKS :", SCControlConfig, 68},

    {ITT_EMPTY, 0, NULL, NULL,  0},
    {ITT_EMPTY, 0, "CHATMODE", NULL, 0},
    {ITT_EFUNC, 0, "OPEN CHAT :", SCControlConfig, 73},
    {ITT_EFUNC, 0, "GREEN CHAT :", SCControlConfig, 74},
    {ITT_EFUNC, 0, "YELLOW CHAT :", SCControlConfig, 75},
    {ITT_EFUNC, 0, "RED CHAT :", SCControlConfig, 76},
    {ITT_EFUNC, 0, "BLUE CHAT :", SCControlConfig, 77},
    {ITT_EFUNC, 0, "COMPLETE :", SCControlConfig, 86},
    {ITT_EFUNC, 0, "DELETE :", SCControlConfig, 98},
    {ITT_EFUNC, 0, "CANCEL CHAT :", SCControlConfig, 87},
    {ITT_EFUNC, 0, "MSG REFRESH :", SCControlConfig, 85},
    {ITT_EFUNC, 0, "MACRO 0:", SCControlConfig, 88},
    {ITT_EFUNC, 0, "MACRO 1:", SCControlConfig, 89},
    {ITT_EFUNC, 0, "MACRO 2:", SCControlConfig, 90},
    {ITT_EFUNC, 0, "MACRO 3:", SCControlConfig, 91},
    {ITT_EFUNC, 0, "MACRO 4:", SCControlConfig, 92},
    {ITT_EFUNC, 0, "MACRO 5:", SCControlConfig, 93},
    {ITT_EFUNC, 0, "MACRO 6:", SCControlConfig, 94},
    {ITT_EFUNC, 0, "MACRO 7:", SCControlConfig, 95},
    {ITT_EFUNC, 0, "MACRO 8:", SCControlConfig, 96},
    {ITT_EFUNC, 0, "MACRO 9:", SCControlConfig, 97},

    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "MISCELLANEOUS", NULL, 0},
    {ITT_EFUNC, 0, "SCREENSHOT :", SCControlConfig, 59},
    {ITT_EFUNC, 0, "PAUSE :", SCControlConfig, 58},
    {ITT_EFUNC, 0, "STOP DEMO :", SCControlConfig, A_STOPDEMO},
    {ITT_EFUNC, 0, "MESSAGE YES :", SCControlConfig, 99},
    {ITT_EFUNC, 0, "MESSAGE NO :", SCControlConfig, 100},
    {ITT_EFUNC, 0, "MESSAGE CANCEL :", SCControlConfig, 101}
};

void G_DefaultBindings(void);
void M_DrawControlsMenu(void);
void G_BindClassRegistration(void);

#endif
