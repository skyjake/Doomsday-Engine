/** @file
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * Player Controls.
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32_MSVC
#  pragma warning (disable:4100) // lots of unused arguments
#endif

#define DENG_NO_API_MACROS_PLAYER

#include <ctype.h>
#include <de/memory.h>

#include "de_console.h"
#include "de_misc.h"
#include "de_system.h"
#include "de_graphics.h"
#include "dd_main.h"

#include "world/p_players.h"
#ifdef __CLIENT__
#  include "ui/b_main.h"
#  include "ui/b_device.h"
#endif
#include "ui/p_control.h"

// MACROS ------------------------------------------------------------------

/*
// Number of triggered impulses buffered into each player's control state
// table.  The buffer is emptied when a ticcmd is built.
#define MAX_IMPULSES    8
#define MAX_DESCRIPTOR_LENGTH 20

#define SLOW_TURN_TIME  (6.0f / 35)
*/
// TYPES -------------------------------------------------------------------

/**
 * The control descriptors contain a mapping between symbolic control
 * names and the identifier numbers.
 */
/*
typedef struct controldesc_s {
    char    name[MAX_DESCRIPTOR_LENGTH + 1];
} controldesc_t;

typedef struct controlclass_s {
    uint    count;
    controldesc_t *desc;
} controlclass_t;
*/
/**
 * Each player has his own control state table.
 */
/*typedef struct controlstate_s {
    // The axes are updated whenever their values are needed,
    // i.e. during the call to P_BuildCommand.
    controlaxis_t *axes;

    // The toggles are modified via console commands.
    controltoggle_t *toggles;

    // The triggered impulses are stored into a ring buffer.
    uint    head, tail;
    impulse_t impulses[MAX_IMPULSES];
} controlstate_t;
*/

typedef enum doubleclickstate_s {
    DBCS_NONE,
    DBCS_POSITIVE,
    DBCS_NEGATIVE
} doubleclickstate_t;

/**
 * Double-"clicks" actually mean double activations that occur within the double-click
 * threshold. This is to allow double-clicks also from the numeric controls.
 */
typedef struct doubleclick_s {
    dd_bool triggered;                      // True if double-click has been detected.
    uint    previousClickTime;              // Previous time an activation occurred.
    doubleclickstate_t lastState;           // State at the previous time the check was made.
    doubleclickstate_t previousClickState;  // Previous click state. When duplicated, triggers
                                            // the double click.
} doubleclick_t;

typedef struct controlcounter_s {
    int     control;
    short   impulseCounts[DDMAXPLAYERS];
    doubleclick_t doubleClicks[DDMAXPLAYERS];
} controlcounter_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(ListPlayerControls);
D_CMD(ClearControlAccumulation);
D_CMD(Impulse);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------
/*
// Control class names - [singular, plural].
const char *ctlClassNames[NUM_CONTROL_CLASSES][NUM_CONTROL_CLASSES] = {
    {{"Axis"}, {"Axes"}},
    {{"Toggle"}, {"Toggles"}},
    {{"Impulse"}, {"Impulses"}}
};*/

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static playercontrol_t* playerControls;
static controlcounter_t** controlCounts;
static int playerControlCount;
static int doubleClickThresholdMilliseconds = 300;

// CODE --------------------------------------------------------------------

static playercontrol_t* P_AllocPlayerControl(void)
{
    playerControls = (playercontrol_t *) M_Realloc(playerControls, sizeof(playercontrol_t) *
                               ++playerControlCount);
    controlCounts = (controlcounter_t **) M_Realloc(controlCounts, sizeof(controlcounter_t*) *
                              playerControlCount);
    memset(&playerControls[playerControlCount - 1], 0, sizeof(playercontrol_t));
    controlCounts[playerControlCount - 1] = NULL;
    return &playerControls[playerControlCount - 1];
}

/**
 * Register the console commands and cvars of the player controls subsystem.
 */
void P_ControlRegister(void)
{
    C_CMD("listcontrols",   "",     ListPlayerControls);
    C_CMD("impulse",        NULL,   Impulse);

#ifdef __CLIENT__
    C_CMD("resetctlaccum",  "",     ClearControlAccumulation);
#endif

    C_VAR_INT("input-doubleclick-threshold", &doubleClickThresholdMilliseconds, 0, 0, 2000);
}

/**
 * This function is exported, so that plugins can register their controls.
 */
DENG_EXTERN_C void P_NewPlayerControl(int id, controltype_t type, const char *name, const char* bindContext)
{
    playercontrol_t *pc = P_AllocPlayerControl();
    pc->id = id;
    pc->type = type;
    pc->name = strdup(name);
    pc->isTriggerable = (type == CTLT_NUMERIC_TRIGGERED || type == CTLT_IMPULSE);
    pc->bindContextName = strdup(bindContext);
    // Also allocate the impulse and double-click counters.
    controlCounts[pc - playerControls] = (controlcounter_t *) M_Calloc(sizeof(controlcounter_t));
}

playercontrol_t* P_PlayerControlById(int id)
{
    int     i;

    for(i = 0; i < playerControlCount; ++i)
    {
        if(playerControls[i].id == id)
            return playerControls + i;
    }
    return NULL;
}

int P_PlayerControlIndexForId(int id)
{
    playercontrol_t* pc = P_PlayerControlById(id);
    if(!pc)
    {
        return -1;
    }
    return pc - playerControls;
}

playercontrol_t* P_PlayerControlByName(const char* name)
{
    int     i;

    for(i = 0; i < playerControlCount; ++i)
    {
        if(!strcasecmp(playerControls[i].name, name))
            return playerControls + i;
    }
    return NULL;
}

void P_ControlShutdown(void)
{
    if(playerControls)
    {
        int i;
        for(i = 0; i < playerControlCount; ++i)
        {
            M_Free(playerControls[i].name);
            M_Free(playerControls[i].bindContextName);
            M_Free(controlCounts[i]);
        }
        playerControlCount = 0;
        M_Free(playerControls);
    }
    playerControls = 0;
    if(controlCounts)
        M_Free(controlCounts);
    controlCounts = 0;
}

#ifdef __CLIENT__
/**
 * Updates the double-click state of a control and marks it as double-clicked
 * when the double-click condition is met.
 *
 * @param playerNum  Player/console number.
 * @param control    Index of the control.
 * @param pos        State of the control.
 */
void P_MaintainControlDoubleClicks(int playerNum, int control, float pos)
{
    doubleclickstate_t newState = DBCS_NONE;
    uint nowTime = 0;
    doubleclick_t* db = 0;

    if(!controlCounts || !controlCounts[control])
    {
        return;
    }
    db = &controlCounts[control]->doubleClicks[playerNum];

    if(doubleClickThresholdMilliseconds <= 0)
    {
        // Let's not waste time here.
        db->triggered = false;
        db->previousClickTime = 0;
        db->previousClickState = DBCS_NONE;
        return;
    }

    if(pos > .5)
    {
        newState = DBCS_POSITIVE;
    }
    else if(pos < -.5)
    {
        newState = DBCS_NEGATIVE;
    }
    else
    {
        // Release.
        db->lastState = newState;
        return;
    }

    // But has it actually changed?
    if(newState == db->lastState)
    {
        return;
    }

    // We have an activation!
    nowTime = Timer_RealMilliseconds();

    if(newState == db->previousClickState &&
       nowTime - db->previousClickTime < (uint) MAX_OF(0, doubleClickThresholdMilliseconds))
    {
        Str *symbolicName = Str_NewStd();

        db->triggered = true;

        switch(newState)
        {
        case DBCS_POSITIVE: Str_Append(symbolicName, "control-doubleclick-positive-"); break;
        case DBCS_NEGATIVE: Str_Append(symbolicName, "control-doubleclick-negative-"); break;

        default: break;
        }

        // Compose the name of the symbolic event.
        Str_Append(symbolicName, playerControls[control].name);

        LOG_AS("P_MaintainControlDoubleClicks");
        LOG_INPUT_XVERBOSE("Triggered plr %i, ctl %i, "
                          "state %i - threshold %i (%s)")
                << playerNum << control << newState << nowTime - db->previousClickTime
                << Str_Text(symbolicName);

        ddevent_t ev; de::zap(ev);
        ev.device = uint(-1);
        ev.type   = E_SYMBOLIC;
        ev.symbolic.id = playerNum;
        ev.symbolic.name = Str_Text(symbolicName);

        I_PostEvent(&ev);

        Str_Delete(symbolicName);
    }

    db->previousClickTime = nowTime;
    db->previousClickState = newState;
    db->lastState = newState;
}
#endif // __CLIENT__

#undef P_IsControlBound
DENG_EXTERN_C int P_IsControlBound(int playerNum, int control)
{
#ifdef __CLIENT__
    struct bcontext_s* bc = 0;
    struct dbinding_s* binds = 0;
    playercontrol_t* pc = P_PlayerControlById(control);

    // Check that this is really a numeric control.
    DENG_ASSERT(pc);
    DENG_ASSERT(pc->type == CTLT_NUMERIC || pc->type == CTLT_NUMERIC_TRIGGERED);
    DENG_UNUSED(pc);

    // Bindings are associated with the ordinal of the local player, not
    // the actual console number (playerNum) being used. That is why
    // P_ConsoleToLocal() is called here.
    binds = B_GetControlDeviceBindings(P_ConsoleToLocal(playerNum), control, &bc);
    if(!binds) return false;

    // There must be bindings to active input devices.
    for(dbinding_t *cb = binds->next; cb != binds; cb = cb->next)
    {
        if(InputDevice *dev = I_DevicePtr(cb->device))
        {
            if(dev->isActive()) return true;
        }
    }
    return false;

#else
    DENG_UNUSED(playerNum);
    DENG_UNUSED(control);
    return 0;
#endif
}

#undef P_GetControlState
DENG_EXTERN_C void P_GetControlState(int playerNum, int control, float* pos, float* relativeOffset)
{
#ifdef __CLIENT__
    float tmp;
    struct bcontext_s* bc = 0;
    struct dbinding_s* binds = 0;
    int localNum;
    playercontrol_t* pc = P_PlayerControlById(control);

    // Check that this is really a numeric control.
    DENG_ASSERT(pc);
    DENG_ASSERT(pc->type == CTLT_NUMERIC || pc->type == CTLT_NUMERIC_TRIGGERED);

    // Ignore NULLs.
    if(!pos) pos = &tmp;
    if(!relativeOffset) relativeOffset = &tmp;

    // Bindings are associated with the ordinal of the local player, not
    // the actual console number (playerNum) being used. That is why
    // P_ConsoleToLocal() is called here.
    localNum = P_ConsoleToLocal(playerNum);
    binds = B_GetControlDeviceBindings(localNum, control, &bc);
    B_EvaluateDeviceBindingList(localNum, binds, pos, relativeOffset, bc, pc->isTriggerable);

    // Mark for double-clicks.
    P_MaintainControlDoubleClicks(playerNum, P_PlayerControlIndexForId(control), *pos);
#else
    DENG_UNUSED(playerNum);
    DENG_UNUSED(control);
    DENG_UNUSED(pos);
    DENG_UNUSED(relativeOffset);
#endif
}

/**
 * @return  Number of times the impulse has been triggered since the last call.
 */
#undef P_GetImpulseControlState
DENG_EXTERN_C int P_GetImpulseControlState(int playerNum, int control)
{
    playercontrol_t* pc = P_PlayerControlById(control);
    short *counter;
    int count = 0;

    assert(pc != 0);

#if _DEBUG
    // Check that this is really an impulse control.
    assert(pc->type == CTLT_IMPULSE);
#endif
    if(!controlCounts[pc - playerControls])
        return 0;

    counter = &controlCounts[pc - playerControls]->impulseCounts[playerNum];
    count = *counter;
    *counter = 0;
    return count;
}

int P_GetControlDoubleClick(int playerNum, int control)
{
    playercontrol_t* pc = P_PlayerControlById(control);
    doubleclick_t *doubleClick = 0;
    int triggered = false;

    if(!pc || playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return 0;

    if(controlCounts[pc - playerControls])
    {
        doubleClick = &controlCounts[pc - playerControls]->doubleClicks[playerNum];
        if(doubleClick->triggered)
        {
            triggered = true;
            doubleClick->triggered = false;
        }
    }
    return triggered;
}

#undef P_Impulse
DENG_EXTERN_C void P_Impulse(int playerNum, int control)
{
    playercontrol_t* pc = P_PlayerControlById(control);

    DENG_ASSERT(pc);

    LOG_AS("P_Impulse");

    // Check that this is really an impulse control.
    if(pc->type != CTLT_IMPULSE)
    {
        LOG_INPUT_WARNING("Control '%s' is not an impulse control") << pc->name;
        return;
    }

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return;

    control = pc - playerControls;

    controlCounts[control]->impulseCounts[playerNum]++;

#ifdef __CLIENT__
    // Mark for double clicks.
    P_MaintainControlDoubleClicks(playerNum, control, 1);
    P_MaintainControlDoubleClicks(playerNum, control, 0);
#endif
}

void P_ImpulseByName(int playerNum, const char* control)
{
    playercontrol_t* pc = P_PlayerControlByName(control);
    if(pc)
    {
        P_Impulse(playerNum, pc->id);
    }
}

void P_ControlTicker(timespan_t time)
{
    // Check for triggered double-clicks, and generate the appropriate impulses.

}

#ifdef __CLIENT__
D_CMD(ClearControlAccumulation)
{
    int     i, p;
    playercontrol_t* pc;

    I_ResetAllDevices();

    for(i = 0; i < playerControlCount; ++i)
    {
        pc = &playerControls[i];
        for(p = 0; p < DDMAXPLAYERS; ++p)
        {
            if(pc->type == CTLT_NUMERIC)
                P_GetControlState(p, pc->id, NULL, NULL);
            else if(pc->type == CTLT_IMPULSE)
                P_GetImpulseControlState(p, pc->id);
            // Also clear the double click state.
            P_GetControlDoubleClick(p, pc->id);
        }
    }
    return true;
}
#endif

/**
 * Prints a list of the registered control descriptors.
 */
D_CMD(ListPlayerControls)
{
    LOG_MSG(_E(b) "Player Controls:");
    LOG_MSG("%i controls have been defined") << playerControlCount;

    for(int i = 0; i < playerControlCount; ++i)
    {
        playercontrol_t *pc = &playerControls[i];

        LOG_MSG("ID %i: " _E(>)_E(b) "%s " _E(.) "(%s) "
                _E(l) "%s%s")
                << pc->id
                << pc->name
                << pc->bindContextName
                << (pc->isTriggerable? "triggerable " : "")
                << (pc->type == CTLT_IMPULSE? "impulse" : "numeric");
    }
    return true;
}

D_CMD(Impulse)
{
    int playerNum = consolePlayer;

    if(argc < 2 || argc > 3)
    {
        LOG_SCR_NOTE("Usage:\n  %s (impulse-name)\n  %s (impulse-name) (player-ordinal)")
                << argv[0] << argv[0];
        return true;
    }
    if(argc == 3)
    {
        // Convert the local player number to an actual player console.
        playerNum = P_LocalToConsole(strtoul(argv[2], NULL, 10));
    }
    P_ImpulseByName(playerNum, argv[1]);
    return true;
}
