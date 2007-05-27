/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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
 */

/*
 * g_ctrl.c: Control bindings - DOOM specifc
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "g_controls.h"

// MACROS ------------------------------------------------------------------

// Control flags.
#define CLF_ACTION      0x1     // The control is an action (+/- in front).
#define CLF_REPEAT      0x2     // Bind down + repeat.

// TYPES -------------------------------------------------------------------

typedef struct {
    char   *deviceaxis;         // device-axis (e.g. "mouse-x").
    char   *control;            // Axis control (prefix with '-' to invert).
    unsigned int bindClass;     // Class it should be bound into
} defaxis_t;

typedef struct {
    char   *command;            // The command to execute.
    int     flags;
    unsigned int bindClass;     // Class it should be bound into
    int     defKey;             //
    int     defMouse;           // Zero means there is no default.
    int     defJoy;             //
} defcontrol_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Register all the various player controls with Doomsday.
 */
void G_RegisterPlayerControls(void)
{
    typedef struct {
        char   *command;                // The command to execute.
    } control_t;

    control_t axisCts[] = {
    {"WALK"},
    {"SIDESTEP"},
    {"turn"},
    {"ZFLY"},
    {"look"},
    {"MAPPANX"},
    {"MAPPANY"},
    {""}  // terminate
    };

    control_t toggleCts[] = {
    {"ATTACK"},
    {"USE"},
    {"strafe"},
    {"SPEED"},
    {"JUMP"},
    {"mlook"},
    {"jlook"},
    {"mzoomin"},
    {"mzoomout"},
    {""}  // terminate
    };

    control_t impulseCts[] = {
    {"falldown"},
    {"lookcntr"},
    {"weap1"},
    {"weapon1"},
    {"weapon2"},
    {"weap3"},
    {"weapon3"},
    {"weapon4"},
    {"weapon5"},
    {"weapon6"},
    {"weapon7"},
    {"weapon8"},
    {"weapon9"},
    {"nextwpn"},
    {"prevwpn"},
    {"demostop"},
    {""}  // terminate
    };
    uint        i;

    // Axis controls.
    for(i = 0; axisCts[i].command[0]; ++i)
    {
        P_RegisterPlayerControl(CC_AXIS, axisCts[i].command);
    }

    // Toggle controls.
    for(i = 0; toggleCts[i].command[0]; ++i)
    {
        P_RegisterPlayerControl(CC_TOGGLE, toggleCts[i].command);
    }

    // Impulse controls.
    for(i = 0; impulseCts[i].command[0]; ++i)
    {
        P_RegisterPlayerControl(CC_IMPULSE, impulseCts[i].command);
    }
}

/**
 * Set default bindings for unbound Controls.
 */
void G_DefaultBindings(void)
{
/*  defaxis_t defAxes[] = {
    {"WALK",        DDBC_NORMAL},
    {"SIDESTEP",    DDBC_NORMAL},
    {"turn",        DDBC_NORMAL},
    {"ZFLY",        DDBC_NORMAL},
    {"look",        DDBC_NORMAL},
    {"MAPPANX",     GBC_CLASS1},
    {"MAPPANY",     GBC_CLASS1},
    {""}  // terminate
    };*/

    defcontrol_t defCtls[] = {
    {"attack",      CLF_ACTION,     DDBC_NORMAL,    DDKEY_RCTRL, 1, 1},
    {"use",         CLF_ACTION,     DDBC_NORMAL,    ' ', 0, 4},
    {"strafe",      CLF_ACTION,     DDBC_NORMAL,    DDKEY_RALT, 3, 2},
    {"speed",       CLF_ACTION,     DDBC_NORMAL,    DDKEY_RSHIFT, 0, 3},
    {"jump",        CLF_ACTION,     DDBC_NORMAL,    0, 0, 0},
    {"mlook",       CLF_ACTION,     DDBC_NORMAL,    'm', 0, 0},
    {"jlook",       CLF_ACTION,     DDBC_NORMAL,    'j', 0, 0},
    {"falldown",    CLF_ACTION,     DDBC_NORMAL,    DDKEY_HOME, 0, 0},
    {"lookcntr",    CLF_ACTION,     DDBC_NORMAL,    DDKEY_END, 0, 0},
    {"weap1",       CLF_ACTION,     DDBC_NORMAL,    0, 0, 0},
    {"weapon1",     CLF_ACTION,     DDBC_NORMAL,    '1', 0, 0},
    {"weapon2",     CLF_ACTION,     DDBC_NORMAL,    '2', 0, 0},
    {"weap3",       CLF_ACTION,     DDBC_NORMAL,    0, 0, 0},
    {"weapon3",     CLF_ACTION,     DDBC_NORMAL,    '3', 0, 0},
    {"weapon4",     CLF_ACTION,     DDBC_NORMAL,    '4', 0, 0},
    {"weapon5",     CLF_ACTION,     DDBC_NORMAL,    '5', 0, 0},
    {"weapon6",     CLF_ACTION,     DDBC_NORMAL,    '6', 0, 0},
    {"weapon7",     CLF_ACTION,     DDBC_NORMAL,    '7', 0, 0},
    {"weapon8",     CLF_ACTION,     DDBC_NORMAL,    '8', 0, 0},
    {"weapon9",     CLF_ACTION,     DDBC_NORMAL,    '9', 0, 0},
    {"nextwpn",     CLF_ACTION,     DDBC_NORMAL,    0, 0, 0},
    {"prevwpn",     CLF_ACTION,     DDBC_NORMAL,    0, 0, 0},

    // Menu actions.
    {"menuup",      CLF_REPEAT,     GBC_CLASS3,     DDKEY_UPARROW, 0, 0},
    {"menudown",    CLF_REPEAT,     GBC_CLASS3,     DDKEY_DOWNARROW, 0, 0},
    {"menuleft",    CLF_REPEAT,     GBC_CLASS3,     DDKEY_LEFTARROW, 0, 0},
    {"menuright",   CLF_REPEAT,     GBC_CLASS3,     DDKEY_RIGHTARROW, 0, 0},
    {"menuselect",  0,              GBC_CLASS3,     DDKEY_ENTER, 0, 0},
    {"menucancel",  0,              GBC_CLASS3,     DDKEY_BACKSPACE, 0, 0},
    {"menu",        0,              GBC_MENUHOTKEY, DDKEY_ESCAPE, 0, 0},

    // Menu hotkeys (default: F1 - F12).
    {"helpscreen",  0,              DDBC_NORMAL,    DDKEY_F1, 0, 0},
    {"savegame",    0,              DDBC_NORMAL,    DDKEY_F2, 0, 0},
    {"loadgame",    0,              DDBC_NORMAL,    DDKEY_F3, 0, 0},
    {"soundmenu",   0,              DDBC_NORMAL,    DDKEY_F4, 0, 0},
    {"quicksave",   0,              DDBC_NORMAL,    DDKEY_F6, 0, 0},
    {"endgame",     0,              DDBC_NORMAL,    DDKEY_F7, 0, 0},
    {"togglemsgs",  0,              DDBC_NORMAL,    DDKEY_F8, 0, 0},
    {"quickload",   0,              DDBC_NORMAL,    DDKEY_F9, 0, 0},
    {"quit",        0,              DDBC_NORMAL,    DDKEY_F10, 0, 0},
    {"togglegamma", 0,              DDBC_NORMAL,    DDKEY_F11, 0, 0},
    {"spy",         0,              DDBC_NORMAL,    DDKEY_F12, 0, 0},

    // Screen controls.
    {"viewsize -",  CLF_REPEAT,     DDBC_NORMAL,    '-', 0, 0},
    {"viewsize +",  CLF_REPEAT,     DDBC_NORMAL,    '=', 0, 0},
    {"sbsize -",    CLF_REPEAT,     DDBC_NORMAL,    0, 0, 0},
    {"sbsize +",    CLF_REPEAT,     DDBC_NORMAL,    0, 0, 0},
    // Misc.
    {"pause",       0,              DDBC_NORMAL,    DDKEY_PAUSE, 0, 0},
    {"screenshot",  0,              DDBC_NORMAL,    0, 0, 0},
    {"showhud",     0,              DDBC_NORMAL,    'h', 0, 0},

    // Automap.
    {"automap",     0,              DDBC_NORMAL,    DDKEY_TAB, 0, 0},
    {"mzoomin",     CLF_ACTION,     GBC_CLASS1,     '=', 0, 0},
    {"mzoomout",    CLF_ACTION,     GBC_CLASS1,     '-', 0, 0},
    {"follow",      0,              GBC_CLASS1,     'f', 0, 0},
    {"rotate",      0,              GBC_CLASS1,     'r', 0, 0},
    {"grid",        0,              GBC_CLASS1,     'g', 0, 0},
    {"zoommax",     0,              GBC_CLASS1,     '0', 0, 0},
    {"addmark",     0,              GBC_CLASS1,     'm', 0, 0},
    {"clearmarks",  0,              GBC_CLASS1,     'c', 0, 0},

    // Chating/messages
    {"beginchat",   0,              DDBC_NORMAL,    't', 0, 0},
    {"beginchat 0", 0,              DDBC_NORMAL,    'g', 0, 0},
    {"beginchat 1", 0,              DDBC_NORMAL,    'i', 0, 0},
    {"beginchat 2", 0,              DDBC_NORMAL,    'b', 0, 0},
    {"beginchat 3", 0,              DDBC_NORMAL,    'r', 0, 0},
    {"chatcomplete", 0,             GBC_CHAT,       DDKEY_ENTER, 0, 0},
    {"chatcancel",  0,              GBC_CHAT,       DDKEY_ESCAPE, 0, 0},
    {"chatsendmacro 0", 0,          GBC_CHAT,       DDKEY_F1, 0, 0},
    {"chatsendmacro 1", 0,          GBC_CHAT,       DDKEY_F2, 0, 0},
    {"chatsendmacro 2", 0,          GBC_CHAT,       DDKEY_F3, 0, 0},
    {"chatsendmacro 3", 0,          GBC_CHAT,       DDKEY_F4, 0, 0},
    {"chatsendmacro 4", 0,          GBC_CHAT,       DDKEY_F5, 0, 0},
    {"chatsendmacro 5", 0,          GBC_CHAT,       DDKEY_F6, 0, 0},
    {"chatsendmacro 6", 0,          GBC_CHAT,       DDKEY_F7, 0, 0},
    {"chatsendmacro 7", 0,          GBC_CHAT,       DDKEY_F8, 0, 0},
    {"chatsendmacro 8", 0,          GBC_CHAT,       DDKEY_F9, 0, 0},
    {"chatsendmacro 9", 0,          GBC_CHAT,       DDKEY_F10, 0, 0},
    {"chatdelete",  0,              GBC_CHAT,       DDKEY_BACKSPACE, 0, 0},

    {"messageyes",  0,              GBC_MESSAGE,    'y', 0, 0},
    {"messageno",   0,              GBC_MESSAGE,    'n', 0, 0},
    {"messagecancel", 0,            GBC_MESSAGE,    DDKEY_ESCAPE, 0, 0},
    {"msgrefresh",  0,              DDBC_NORMAL,    DDKEY_ENTER, 0, 0},
    {"", 0, 0, 0, 0, 0}  // terminate
    };

    int         i;
    char        evname[80], cmd[256], buff[256];
    evtype_t    evType;
    evstate_t   evState;
    int         evData;

    // Check all Controls (toggles, impulses).
    for(i = 0; defCtls[i].command[0]; ++i)
    {
        defcontrol_t *ctr = &defCtls[i];
        // If this command is bound to something, skip it.
        sprintf(cmd, "%s%s", (ctr->flags & CLF_ACTION ? "+" : ""), ctr->command);
        memset(buff, 0, sizeof(buff));
        if(B_BindingsForCommand(cmd, buff, 0, true))
            continue;

        // This Control has no bindings, set it to the default.
        sprintf(buff, "\"%s\"", ctr->command);
        if(ctr->defKey)
        {
            evType = EV_KEY;
            evState = EVS_DOWN;
            evData = ctr->defKey;
            B_FormEventString(evname, evType, evState, evData);
            sprintf(cmd, "%s bdc%d %s %s",
                    (ctr->flags & CLF_REPEAT ? "safebindr" : "safebind"),
                    ctr->bindClass, evname + 1, buff);
            DD_Execute(cmd, true);
        }

        if(ctr->defMouse)
        {
            evType = EV_MOUSE_BUTTON;
            evState = EVS_DOWN;
            evData = 1 << (ctr->defMouse - 1);
            B_FormEventString(evname, evType, evState, evData);
            sprintf(cmd, "%s bdc%d %s %s",
                    (ctr->flags & CLF_REPEAT ? "safebindr" : "safebind"),
                    ctr->bindClass, evname + 1, buff);
            DD_Execute(cmd, true);
        }

        if(ctr->defJoy)
        {
            evType = EV_JOY_BUTTON;
            evState = EVS_DOWN;
            evData = 1 << (ctr->defJoy - 1);
            B_FormEventString(evname, evType, evState, evData);
            sprintf(cmd, "%s bdc%d %s %s",
                    (ctr->flags & CLF_REPEAT ? "safebindr" : "safebind"),
                    ctr->bindClass, evname + 1, buff);
            DD_Execute(cmd, true);
        }
    }

/*
    // Check axes.
    for(i = 0; defAxes[i].command[0]; ++i)
    {
        defaxis_t *axe = &defAxes[i];
        // FIXME!
    }
 */
}
