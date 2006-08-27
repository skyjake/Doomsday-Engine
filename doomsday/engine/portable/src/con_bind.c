/* DE1: $Id$
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
 * con_bind.c: Event/Command Binding
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

#include <assert.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    int     key;                // DDKEY
    char   *name;
} keyname_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void    B_EventBuilder(char *buff, event_t *ev);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern action_t *ddactions;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

binding_t *binds = NULL;
int     numBinds = 0;

bindclass_t *bindClasses = NULL;
int    numBindClasses = 0;
int    maxBindClasses = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static keyname_t keyNames[] = {
    {DDKEY_PAUSE, "pause"},
    {DDKEY_ESCAPE, "escape"},
    {DDKEY_ESCAPE, "esc"},
    {DDKEY_RIGHTARROW, "right"},
    {DDKEY_LEFTARROW, "left"},
    {DDKEY_UPARROW, "up"},
    {DDKEY_DOWNARROW, "down"},
    {DDKEY_ENTER, "enter"},
    {DDKEY_TAB, "tab"},
    {DDKEY_RSHIFT, "shift"},
    {DDKEY_RCTRL, "ctrl"},
    {DDKEY_RALT, "alt"},
    {DDKEY_INS, "ins"},
    {DDKEY_DEL, "del"},
    {DDKEY_PGUP, "pgup"},
    {DDKEY_PGDN, "pgdown"},
    {DDKEY_PGDN, "pgdn"},
    {DDKEY_HOME, "home"},
    {DDKEY_END, "end"},
    {DDKEY_BACKSPACE, "bkspc"},
    {' ', "space"},
    {';', "smcln"},
    {'\"', "quote"},
    {DDKEY_F10, "f10"},
    {DDKEY_F11, "f11"},
    {DDKEY_F12, "f12"},
    {DDKEY_F1, "f1"},
    {DDKEY_F2, "f2"},
    {DDKEY_F3, "f3"},
    {DDKEY_F4, "f4"},
    {DDKEY_F5, "f5"},
    {DDKEY_F6, "f6"},
    {DDKEY_F7, "f7"},
    {DDKEY_F8, "f8"},
    {DDKEY_F9, "f9"},
    {'`', "tilde"},
    {DDKEY_NUMLOCK, "numlock"},
    {DDKEY_SCROLL, "scrlock"},
    {DDKEY_NUMPAD0, "pad0"},
    {DDKEY_NUMPAD1, "pad1"},
    {DDKEY_NUMPAD2, "pad2"},
    {DDKEY_NUMPAD3, "pad3"},
    {DDKEY_NUMPAD4, "pad4"},
    {DDKEY_NUMPAD5, "pad5"},
    {DDKEY_NUMPAD6, "pad6"},
    {DDKEY_NUMPAD7, "pad7"},
    {DDKEY_NUMPAD8, "pad8"},
    {DDKEY_NUMPAD9, "pad9"},
    {DDKEY_DECIMAL, "pad,"},
    {DDKEY_SUBTRACT, "pad-"},    // not really used
    {DDKEY_ADD, "pad+"},        // not really used

    {0, NULL}                    // The terminator
};

static char *povDirNames[] = {
    "F", "FR", "R", "BR", "B", "BL", "L", "FL", NULL
};

// DJS 13/05/05
// Binding Classes
//       (very handy for gamepads with a limited number of buttons ;-))

// Binding classes are created dynamically at runtime. During (pre)init the
// game register the classes it NEEDS. The order of the bind Classes in the
// array (the class stack) determines the order in which bindings are checked
// in B_Responder(). Thus it is important that bind classes are created in the
// correct order (game specific).

// It would also be possible for users to create any additional binding classes
// they required at runtime via console commands.
// However that would mean a fair amount of extra book keeping, so for now there
// are three generic classes which can be used for this purpose.

// Bindings are saved with the classnames eg:
//   bind game +w +forward

// However, omission of the class name defaults the bind to class 0 (id 1)
// and the bindings in the cfg will be updated with the missing class names
// automatically on exit (for reading old cfg files).

// When a binding class is enabled/disabled we loop through the bindings
// looking for any that are bound to any keys/buttons being pressed at that time.
// If any are found we que extra up events that request a command in a specific
// binding class. Due to binding classes being ordered numericaly with the
// rule that only the command in the highest active binding class being executed
// we only need to check commands for bindings with a lower binding class id.

// A binding class may be "absolute". This means that when the class is active,
// if there is no binding in the current class (while decending the bind class
// stack in B_Responder()) and that class is "absolute" - all classes BELOW the
// current will be ignored and no binding will be found for the event.
// The event is NOT eaten and may continue on down the event responder chain.

bindclass_t ddBindClasses[] = {
    {"game", DDBC_NORMAL, 1, 0},
    {"class1", DDBC_UCLASS1, 0, 0}, // additonal classes that can be purposed by users
    {"class2", DDBC_UCLASS2, 0, 0},
    {"class3", DDBC_UCLASS3, 0, 0},
    {"biaseditor", DDBC_BIASEDITOR, 0, 0},
    {NULL}
};

// CODE --------------------------------------------------------------------

/*
 * Registers the engine's own binding classes. Called once during init.
 */
void B_RegisterBindClasses(void)
{
    int    i;

    for(i = 0; ddBindClasses[i].name; i++)
        DD_AddBindClass(ddBindClasses + i);
}

#if _DEBUG
const char* EventType_Str(int type)
{
    static char typeStr[40];
    struct evnttype_s {
        int type;
        const char* str;
    } evnts[] =
    {
        { EV_KEY,           "key"},
        { EV_MOUSE_AXIS,    "mouse axis"},
        { EV_MOUSE_BUTTON,  "mouse button"},
        { EV_JOY_AXIS,      "joy axis"},
        { EV_JOY_SLIDER,    "joy slider"},
        { EV_JOY_BUTTON,    "joy button"},
        { EV_POV,           "pov"},
        { 0, NULL}
    };
    int i;

    for(i = 0; evnts[i].str; ++i)
        if(evnts[i].type == type)
            return evnts[i].str;

    sprintf(typeStr, "(unnamed %i)", type);
    return typeStr;
}
#endif

/*
 * Compares two sets of event data to see if they match.
 */
static boolean B_EventMatch(evtype_t eventTypeA, int dataA,
                            evtype_t eventTypeB, int dataB)
{
    boolean match = false;

    // Check the type.
    if(eventTypeA == eventTypeB)
    {
        // Check the data.
        switch (eventTypeA)
        {
        case EV_KEY:
        case EV_POV:
            if(dataA == dataB)
                match = true;
            break;

        case EV_MOUSE_BUTTON:
        case EV_JOY_BUTTON:
            if((dataA & dataB) != 0)
                match = true;
            break;

        default:
            // We don't know what to compare... :-]
            break;
        }
    }

//#if _DEBUG
//if(match) Con_Message("B_EventDataMatch: (%s)\n",
//                      EventType_Str(eventTypeA));
//#endif

    return match;
}

/*
 * Searches the bindings array for a command which matches the search critera.
 *
 * @param   eventType       The type of event-binding to search for.
 * @param   data            The data value to search for (eg key DDKEY_PGDN).
 * @param   forceClass      Only search for bindings in this class. Set this
 *                          parameter to -1 to search all classes.
 * @return  char*           Ptr to the found command ELSE NULL.
 */
static char *B_GetCommandForEvent(evtype_t eventType, evstate_t eventState,
                                  int data, int forceClass)
{
    int     i, k;
    binding_t *bnd;
    command_t *cmd;
    boolean done = false;

    for(i = 0, bnd = binds; i < numBinds && !done; ++i, bnd++)
    {
        // Do the parameters match those of this event-binding?
        if(B_EventMatch(eventType, data, bnd->type, bnd->data1))
        {
            if(forceClass != -1) // use a specific class? (active or not)
            {
                // FYI: These kind of events aren't sent via direct user input
                // Only by "us" when we need to switch binding classes and a
                // current input is active eg; a key is held down during the
                // switch that has commands in multiple binding classes.
                cmd = &bnd->commands[forceClass];
                if(cmd->command[eventState])
                {
//#if _DEBUG
//Con_Message("B_GetCommandForEvent: Returned bindID %i (%s) \"%s\"\n",
//            bnd - binds, bindClasses[forceClass].name,
//            cmd->command[eventState]);
//#endif
                    return cmd->command[eventState];
                }
            }
            else
            {
                // loop backwards through the active binding classes, the
                // command in the highest binding class slot that is currently
                // active is executed
                for(k = numBindClasses; k >= 0 && !done; --k)
                {
                    cmd = &bnd->commands[k];

                    if(bindClasses[k].active == 1)
                    {
                        if(cmd->command[eventState])
                        {
//#if _DEBUG
//Con_Message("B_GetCommandForEvent: Returned bindID %i (%s) \"%s\"\n",
//            bnd - binds, bindClasses[k].name, cmd->command[eventState]);
//#endif
                            return cmd->command[eventState];
                        }
                        else
                        {
                            // RULE: If a repeat event does not have a
                            // binding in BINDCLASS (k) we should ignore
                            // commands in all lower classes IF there is NOT
                            // a keydown binding for this event in this class.
                            if(eventState == EVS_REPEAT && cmd->command[EVS_DOWN])
                                done = true; // do nothing
                        }

                        // Should we ignore commands in lower classes?
                        if(bindClasses[k].absolute == 1)
                            done = true;
                    }
                }
            }
        }
    }
    return NULL;
}

/*
 * Checks to see if we need to respond to the given input event in some way
 * and then if so executes the action associated to the event.
 *
 * @return boolean          <code>true</code> If any action was performed.
 */
boolean B_Responder(event_t *ev)
{
    char *cmd;

    // We won't even bother with axis data.
    if(ev->type == EV_MOUSE_AXIS || ev->type == EV_JOY_AXIS)
        return false;

    // Look for a command for this event.
    cmd = B_GetCommandForEvent(ev->type, ev->state, ev->data1, ev->useclass);
    if(cmd != NULL)
    {
        // Found one, execute!
        Con_Execute(CMDS_BIND, cmd, true);
        return true;
    }

    return false; // nope nothing.
}

/*
 *  Returns a binding for the given event.
 */
binding_t *B_GetBinding(event_t *event, boolean create_new)
{
    int     i, j;
    binding_t *newb;

    // We'll first have to search through the existing bindings
    // to see if there already is one for this event.
    for(i = 0; i < numBinds; i++)
    {
        if(B_EventMatch(event->type, event->data1, binds[i].type,
                        binds[i].data1))
            return binds + i;
    }

    if(!create_new)
        return NULL;

    // Hmm, no luck there. Let's create a new binding_t.
    binds = realloc(binds, sizeof(binding_t) * (numBinds + 1));
    newb = binds + numBinds++;
    memset(newb, 0, sizeof(*newb));
    newb->commands = malloc(sizeof(command_t) * maxBindClasses);

    for(i = 0; i < maxBindClasses; ++i)
        for(j = 0; j < 3; ++j)
            newb->commands[i].command[j] = NULL;

    // Copy the event data.
    newb->type  = event->type;
    newb->data1 = event->data1;
    return newb;
}

/*
 *
 */
void B_DeleteBindingIdx(int index)
{
    int i, k;

    if(index < 0 || index > numBinds - 1)
        return; // What?

    for(i = 0; i < numBindClasses; ++i)
        for(k = 0; k < 3; ++k)
            if(binds[index].commands[i].command[k])
            {
                free(binds[index].commands[i].command[k]);
                binds[index].commands[i].command[k] = NULL;
            }

    free(binds[index].commands);

    if(index < numBinds - 1) // If not the last one, do some rollback.
    {
        memmove(binds + index, binds + index + 1,
                sizeof(binding_t) * (numBinds - index - 1));
    }
    binds = realloc(binds, sizeof(binding_t) * --numBinds);
}

/*
 *  Binds the given event to the command. Also Rebinds old bindings.
 *
 *  1) Binding to NULL without specifying a class: deletes the binding.
 *
 *  2) Bindind to NULL and specifying a class: clears the command and
 *     if no more commands exist for this binding - will delete it
 */
void B_Bind(event_t *event, char *command, int bindClass)
{
    int i, k, count = 0;
    binding_t *bnd = B_GetBinding(event, true);

    if(!command) // No string given?
    {
        // What about a bind class?
        if(bindClass < 0)
            B_DeleteBindingIdx(bnd - binds); // nope, delete.
        else
        {
            // clear the command in bindClass only
            for(i = 0; i < numBindClasses; i++)
                for(k = 0; k < 3; ++k)
                    if(bnd->commands[i].command[k])
                    {
                        count++;
                        if(i == bindClass && k == event->state)
                            bnd->commands[i].command[k] = NULL;
                    }

            if(count == 1)
                // there are no more commands for this binding so delete.
                B_DeleteBindingIdx(bnd - binds);
        }
        return;
    }

    // Set the command.
    bnd->commands[bindClass].command[event->state] =
        realloc(bnd->commands[bindClass].command[event->state],
                strlen(command) + 1);
    strcpy(bnd->commands[bindClass].command[event->state], command);

//#if _DEBUG
//Con_Printf("B_Bind: (%s) state: %i data: %d cmd: \"%s\"\n",
//           EventType_Str(bnd->type), event->state,
//           bnd->data1, bnd->commands[bindClass].command[event->state]);
//#endif
}

/*
 *
 */
void B_ClearBinding(char *command, int bindClass)
{
    int     i, j, k, count;
    boolean match;

    for(i = 0; i < numBinds; ++i)
    {
        match = false;
        count = 0;
        if(bindClass == -1)
        {
            // clear all commands in all binding classes, all states.
            for(j = 0; j < numBindClasses; ++j)
                for(k = 0; k < 3; ++k)
                    if(binds[i].commands[j].command[k])
                        if(!stricmp(binds[i].commands[j].command[k], command))
                            B_DeleteBindingIdx(i--);
        }
        else
        {
            // clear the command in bindClass only, all states.
            for(j = 0; j < numBindClasses; ++j)
            {
                for(k = 0; k < 3; ++k)
                    if(binds[i].commands[j].command[k])
                    {
                        count++;
                        if((!stricmp(binds[i].commands[j].command[k], command)) &&
                            j == bindClass)
                        {
                            match = true;
                            binds[i].commands[j].command[k] = NULL;
                        }
                    }
            }

            if(match && count == 1)
                // there are no more commands for this binding so delete.
                B_DeleteBindingIdx(i--);
        }
    }
}

/*
 * Deallocates the memory for the commands and bindings.
 */
void B_Shutdown()
{
    int     i, j, k;

    for(i = 0; i < numBinds; ++i)
    {
        for(j = 0; j < numBindClasses; ++j)
        {
            for(k = 0; k < 3; ++k)
                if(binds[i].commands[j].command[k])
                    free(binds[i].commands[j].command[k]);
        }

        free(binds[i].commands);
    }
    free(binds);
    binds = NULL;
    numBinds = 0;

    // Now we can clear the bindClasses
    for(i = 0; i < numBindClasses; ++i)
        free(bindClasses[i].name);

    free(bindClasses);
    bindClasses = NULL;
    numBindClasses = maxBindClasses = 0;
}

/*
 * shortNameForKey
 *  If buff is "" upon returning, the key is not valid for controls.
 */
static char *shortNameForKey(int ddkey)
{
    int     i;

    for(i = 0; keyNames[i].key; i++)
        if(ddkey == keyNames[i].key)
            return keyNames[i].name;
    return NULL;
}

/*
 * getByShortName
 */
static int getByShortName(const char *key)
{
    int     i;

    for(i = 0; keyNames[i].key; i++)
        if(!strnicmp(key, keyNames[i].name, strlen(keyNames[i].name)))
            return keyNames[i].key;
    return 0;
}

/*
 * buttonNumber
 */
static int buttonNumber(int flags)
{
    int     i;

    for(i = 0; i < 32; i++)
        if(flags & (1 << i))
            return i;
    return -1;
}

/*
 * Converts a textual representation of an event to the real thing.
 * Buff must be valid source buffer ev a valid destination.
 *
 * @param buff      Src buffer containing the textual event string.
 * @param ev        The destination event to be updated.
 */
static void B_EventBuilder(char *buff, event_t *ev)
{
    char    prefix;
    char   *begin;
    int     key;

    prefix = buff[0];
    begin = buff;
    if(strlen(buff) > 1)
    {
        if(prefix != '+' && prefix != '-' && prefix != '*')
            prefix = '+';    // 'Down' by default.
        begin = buff + 1;
    }
    else
        prefix = '+';

    // First check the obvious cases.
    if(!strnicmp(begin, "mb", 2))    // Mouse button?
    {
        ev->type = EV_MOUSE_BUTTON;
        ev->state = (prefix == '+' ? EVS_DOWN : EVS_UP);
        ev->data1 = 1 << (atoi(begin + 2) - 1);
    }
    else if(!strnicmp(begin, "mw", 2))    // Mouse wheel?
    {
        ev->type = EV_MOUSE_BUTTON;
        ev->state = (prefix == '+' ? EVS_DOWN : EVS_UP);
        ev->data1 =
            !stricmp(begin + 2, "up") ? DDMB_MWHEELUP : DDMB_MWHEELDOWN;
    }
    else if(!strnicmp(begin, "jb", 2))    // Joystick button?
    {
        ev->type = EV_JOY_BUTTON;
        ev->state = (prefix == '+' ? EVS_DOWN : EVS_UP);
        ev->data1 = 1 << (atoi(begin + 2) - 1);
    }
    else if(!strnicmp(begin, "pov", 3))    // A POV angle?
    {
        int     i;

        ev->type = EV_POV;
        ev->state = (prefix == '+' ? EVS_DOWN : EVS_UP);
        ev->data1 = -1;
        for(i = 0; povDirNames[i]; i++)
            if(!stricmp(begin + 3, povDirNames[i]))
            {
                ev->data1 = i;
                break;
            }
    }
    else
    {
        ev->type = EV_KEY;
        ev->state =
            (prefix == '+' ? EVS_DOWN : (prefix == '*' ? EVS_REPEAT :
             EVS_UP));
        if((key = getByShortName(begin)))
            ev->data1 = key;
        else
            ev->data1 = begin[0];
    }
}

/*
 *  Forms a textual representation for an input event.
 *  Part of the Doomsday public API.
 *
 *  @param  buff        Destination buffer to hold the formed string.
 *  @param  type        The event type (e.g. EV_KEY = keyboard).
 *  @param  state       The event state (EVS_DOWN/EVS_UP/EVS_REPEAT).
 *  @param  data        Usage depends on <code>type</code> eg keynumber.
 */
void B_FormEventString(char *buff, evtype_t type, evstate_t state,
                       int data1)
{
    char    prefix[3] = {'+', '-', '*'};
    char   *begin;

    if(state < EVS_DOWN || state > EVS_REPEAT)
        Con_Error("B_FormEventString: bad event state (%d)\n", state);

    switch (type)
    {
    case EV_KEY:
        if((begin = shortNameForKey(data1)))
        {
            sprintf(buff, "%c%s", prefix[state], begin);
        }
        else if(data1 > 32 && data1 < 128)
        {
            sprintf(buff, "%c%c", prefix[state], data1);
        }
        break;

    case EV_MOUSE_BUTTON:
        if(data1 & (DDMB_MWHEELUP | DDMB_MWHEELDOWN))
            sprintf(buff, "%cMW%s", prefix[state],
                    data1 & DDMB_MWHEELUP ? "up" : "down");
        else
            sprintf(buff, "%cMB%d", prefix[state], buttonNumber(data1) + 1);
        break;

    case EV_JOY_BUTTON:
        sprintf(buff, "%cJB%d", prefix[state], buttonNumber(data1) + 1);
        break;

    case EV_POV:
        sprintf(buff, "%cPOV%s", prefix[state], povDirNames[data1]);
        break;

    default:
        Con_Error("B_FormEventString: bad event type (%d).\n", type);
    }
}

/*
 * CCmdBind
 */
D_CMD(Bind)
{
    boolean prefixGiven = true;
    boolean bindClassGiven = false;
    char    validEventName[16], buff[80];
    char    prefix = '+', *begin;
    char    *evntptr = argv[2];
    char    *cmdptr = argv[3];
    event_t event;
    int     repeat = !stricmp(argv[0], "bindr") ||
        !stricmp(argv[0], "safebindr");
    int     safe = !strnicmp(argv[0], "safe", 4);
    int        i, bc = -1;
    binding_t *existing;

    if(argc < 2 || argc > 4)
    {
        Con_Printf("Usage: %s (class) (event) (cmd)\n", argv[0]);
        Con_Printf("Binding Classes:\n");
        for(i = 0; i < numBindClasses; i++)
            Con_Printf("  %s\n", bindClasses[i].name);
        return true;
    }

    // Check for a specified binding class
    for(i = 0; i < numBindClasses && !bindClassGiven; ++i)
    {
        if(!(stricmp(argv[1], bindClasses[i].name)) ||
            ((!strnicmp(argv[1], "bdc", 3) &&
            (atoi(argv[1]+3) == bindClasses[i].id) )))
        {
            bc = bindClasses[i].id;
            bindClassGiven = true;
        }
    }

    // Has a binding class been specified?
    if(!bindClassGiven)
    {
        // No it hasn't! default to normal
        bc = DDBC_NORMAL;
        evntptr = argv[1];
        cmdptr = argv[2];
    }

    begin = evntptr;
    if(strlen(evntptr) > 1)        // Can the event have a prefix?
    {
        prefix = evntptr[0];
        begin = evntptr + 1;
        if(prefix != '+' && prefix != '-' && prefix != '*')
        {
            begin = evntptr;
            prefix = '+';
            prefixGiven = false;
        }
    }
    else
        prefixGiven = false;

    if((argc == 3 && !prefixGiven && bindClassGiven) ||
        ((argc == 2) && !prefixGiven && !bindClassGiven))
    {
        char    prefixes[3] = { '+', '-', '*' };
        int     i;

        // We're clearing a binding. If no prefix has been given,
        // all event states (+, - and *) are all cleared.
        for(i = 0; i < 3; i++)
        {
            sprintf(buff, "%c%s", prefixes[i], evntptr);
            B_EventBuilder(buff, &event);
            B_Bind(&event, NULL, bc);
        }
        return true;
    }
    if(argc == 4 || (argc == 3 && !bindClassGiven))
    {
        char    cprefix = cmdptr[0];

        if(cprefix != '+' && cprefix != '-' && begin == evntptr)
        {
            // Bind both the + and -.
            sprintf(validEventName, "-%s", evntptr);
            sprintf(buff, "-%s", cmdptr);
            if(Con_ActionCommand(buff, true))
            {
                B_EventBuilder(validEventName, &event);
                if(safe && (existing = B_GetBinding(&event, false)))
                    if(existing->commands[bc].command[event.state])
                        return false;

                B_Bind(&event, buff, bc);
                sprintf(validEventName, "+%s", evntptr);
                sprintf(buff, "+%s", cmdptr);
                B_EventBuilder(validEventName, &event);
                B_Bind(&event, buff, bc);
                return true;
            }
        }
    }
    sprintf(validEventName, "%c%s", prefix, begin);

    //Con_Printf( "Binding %s : %s.\n", validEventName, argc==2? "(nothing)" : cmdptr);

    // Convert the name to an event.
    B_EventBuilder(validEventName, &event);
    if(safe && (existing = B_GetBinding(&event, false)))
        if(existing->commands[bc].command[event.state])
            return false;

    // Now we can create a binding for it.
    if((argc == 2 && !bindClassGiven && !prefixGiven) ||
            (argc == 3 && bindClassGiven))
        B_Bind(&event, NULL, bc);
    else
        B_Bind(&event, cmdptr, bc);

    // A repeater?
    if(repeat && event.type == EV_KEY && event.state == EVS_DOWN)
    {
        event.state = EVS_REPEAT;

        if((argc == 2 && !bindClassGiven && !prefixGiven) ||
                (argc == 3 && bindClassGiven))
            B_Bind(&event, NULL, bc);
        else
            B_Bind(&event, cmdptr, bc);
    }
    return true;
}

D_CMD(ClearBindings)
{
    B_Shutdown();
    Con_Printf("All bindings cleared.\n");
    return true;
}

D_CMD(DeleteBind)
{
    int     i, bc = -1;
    int     start = 1;
    char   *cmdptr = argv[1];

    if(argc < 2)
    {
        Con_Printf("Usage: %s (binding class) (cmd) ...\n", argv[0]);
        Con_Printf(": Omit Binding class to clear cmds in all binding classes\n");
        return true;
    }

    // Check for a specified binding class
    if(argc > 2)
        for(i = 0 ; i < numBindClasses; ++i)
        {
            if((!stricmp(argv[1], bindClasses[i].name)) ||
                (atoi(argv[1]) == bindClasses[i].id))
            {
                if(argc < 3)
                {
                    Con_Printf("Usage: %s (binding class) (cmd) ...\n", argv[0]);
                    Con_Printf(": Omit Binding class to clear cmds in all binding classes\n");
                    return true;
                }
                else
                {
                    bc = i;
                    cmdptr = argv[2];
                    start = 2;
                    break;
                }
            }
        }

    if(bc == -1 || bc < numBindClasses)
    {
        for(i = start; i < argc && cmdptr; cmdptr = argv[i++])
            B_ClearBinding(cmdptr, bc);
    }
    else
    {
        Con_Printf("Not a valid binding class. Enter listbindclasses.\n");
        return false;
    }

    return true;
}

D_CMD(ListBindClasses)
{
    int k;

    // Show the available binding classes
    Con_Printf("Binding Classes:\n");

    for(k = 0; k < numBindClasses; ++k)
        Con_Printf("  %s\n", bindClasses[k].name);

    return true;
}

D_CMD(ListBindings)
{
    int     i, j, k, onlythis = -1;
    int        comcount = 0;
    char    buffer[20];

    // Are we showing bindings in a particular class only?
    if(argc >= 2)
    {
        for(k = 0; k < numBindClasses; ++k)
            if(!stricmp(argv[1], bindClasses[k].name))
            {
                // only show bindings in this class
                onlythis = bindClasses[k].id;
            }
    }

    // loop through the bindings
    for(i = 0; i < numBinds; ++i)
    {
        // loop through the bindclasses
        for(j = 0; j < numBindClasses; ++j)
        {
            for(k = 0; k < 3; ++k) // for each event state
            {
                if(binds[i].commands[j].command[k])
                {
                    B_FormEventString(buffer, binds[i].type, k, binds[i].data1);

                    if(argc >= 2)
                    {
                        if(onlythis == -1 && strnicmp(buffer + 1, argv[1], strlen(argv[1])))
                            continue;        // Doesn't match the search pattern.
                        else
                        {
                            if(argc >= 3)
                            {
                                if(strnicmp(buffer + 1, argv[2], strlen(argv[2])) ||
                                   (j != -1 && j != onlythis))
                                    continue; // Doesn't match the search pattern
                            }
                            else if(onlythis != -1 && j != onlythis)
                                continue;    // Doesn't match the search pattern
                        }
                    }
                    comcount++;

                    if(onlythis != -1)
                        Con_Printf("%-8s : %s\n", buffer, binds[i].commands[j].command[k]);
                    else
                        Con_Printf("%-8s : %s : %s\n", buffer, bindClasses[j].name,
                                   binds[i].commands[j].command[k]);
                }
            }
        }
    }

    if(onlythis != -1)
    {
        Con_Printf("Showing %d (%s class) commands from %d bindings.\n",
            comcount, bindClasses[onlythis].name, numBinds);
    }
    else
    {
        Con_Printf("Showing %d commands from %d bindings.\n",
            comcount, numBinds);
    }
    return true;
}

/*
 *  Add a new binding class
 */
void DD_AddBindClass(bindclass_t *newbc)
{
    int i;
    bindclass_t *added;

    VERBOSE2(Con_Printf("DD_AddBindClass: %s.\n", newbc->name));

    if(++numBindClasses > maxBindClasses)
    {
        // Allocate more memory.
        maxBindClasses *= 2;
        if(maxBindClasses < numBindClasses)
            maxBindClasses = numBindClasses;

        bindClasses = realloc(bindClasses, sizeof(bindclass_t) * maxBindClasses);

        for(i = 0; i < numBinds; ++i)
        {
            binds[i].commands = realloc(binds[i].commands,
                                        sizeof(command_t) * maxBindClasses);
        }
    }

    added = &bindClasses[numBindClasses - 1];
    memcpy(added, newbc, sizeof(bindclass_t));
    added->id = numBindClasses - 1;

    // Allocate a copy of the class name.
    added->name = strdup(newbc->name);
}

/*
 * Enables/disables binding classes
 * Ques extra input events as required
 */
D_CMD(EnableBindClass)
{
    int i = -1;

    if(argc < 2 || argc > 3)
    {
        for(i = 0; i < numBindClasses; ++i)
            Con_Printf("%d: %s is %s\n", i, bindClasses[i].name,
                       (bindClasses[i].active)? "On" : "Off");

        Con_Printf("Usage: %s (binding class) (1=On, 0=Off (leave blank to toggle))\n",
                   argv[0]);
        return true;
    }

    // look for a binding class with either a name or id
    // that matches the argument
    for(i = 0; i < numBindClasses; ++i)
    {
        if(!(stricmp(argv[1], bindClasses[i].name)))
        {
            i = bindClasses[i].id;
            break;
        }
    }

    if(i >= 0 && i < numBindClasses)
    {
        if(B_SetBindClass(i, (argc == 3)? atoi(argv[2]) : 2))
            return true;
    }
    else
        Con_Printf("Not a valid binding class. Enter listbindclasses.\n");

    return false;
}

/*
 *  Enables/disables binding classes
 *  Wrapper for the game dll.
 *  This way we can allow users to create their own binding classes that can be placed
 *  anywhere in the bindClass stack without the dll having to keep track of the classIDs
 */
boolean DD_SetBindClass(int classID, int type)
{
    // creation of user bind classes not implemented yet so there is no offset
    return B_SetBindClass(classID, type);
}

/*
 *  Enables/disables binding classes
 *  Ques extra input events as required
 */
boolean B_SetBindClass(int classID, int type)
{
    int    i, k;
    int    count;
    binding_t *bind;

    // Change the active state of the bindClass
    switch(type)
    {
        case 0:  // implicitly set
        case 1:
            bindClasses[classID].active = type? 1 : 0;
            break;
        case 2:  // toggle
            bindClasses[classID].active = bindClasses[classID].active? 0 : 1;
            break;
    }

    VERBOSE2(Con_Printf("B_SetBindClass: %s %s %s.\n",
                        bindClasses[classID].name,
                        (type==2)? "TOGGLE" : "SET",
                        bindClasses[classID].active? "ON" : "OFF"));

    // Now we need to do a check in case there are keys currently
    // being pressed that should be released if the event binding they are
    // bound too has commands in the bind class being enabled/disabled.

    // loop through the bindings
    for(i=0, bind = binds; i < numBinds; ++i, ++bind)
    {
        // we're only interested in bindings for down events currently being pressed
        // that have a binding in the class being enabled/disabled (classID)
        if(bind->commands[classID].command[EVS_DOWN] != NULL &&
            ( (bind->type == EV_KEY && (DD_IsKeyDown(bind->data1))) ||
                (bind->type == EV_MOUSE_BUTTON && (DD_IsMouseBDown(bind->data1))) ||
                (bind->type == EV_JOY_BUTTON && (DD_IsJoyBDown(bind->data1))) ))
        {
            count = 0;
            // loop through the commands for this binding,
            // count the number of commands for this binding that are for currently active
            // bind classes with a lower id than the class being enabled/disabled (i)
            for(k = 0; k < numBindClasses; ++k)
                if(bindClasses[k].active && bind->commands[k].command[EVS_DOWN])
                {
                    // if there is a command for this event binding in a class that is
                    // currently active (current is k), that has a greater id than the
                    // class being enabled/disabled (classID) then we don't need to que
                    // any extra events at all as that will have been done when the binding
                    // class with the higher id was enabled. The commands in the lower
                    // classes can't have been active (for this event), as the highest
                    // class command is ALWAYS executed unless a specific class is requested.
                    if(k > classID)
                    {
                        // don't need to que any extra events
                        count = 0;
                        break;
                    }
                    else
                        count++;
                }

            if(count > 0)
            {
                // We need to send up events with a forced binding
                // command request for all the binding classes with a
                // lower id than the class being enabled/disabled (classID)
                // that are also active.
                for(k = 0; k < classID; ++k)
                {
                    if(bindClasses[k].active && bind->commands[k].command[EVS_UP])
                    {
                        event_t ev;
                        // que an up event for this down event.
                        ev.type = bind->type;
                        ev.data1 = bind->data1;
                        ev.state = EVS_UP;

                        // request a command in this class
                        ev.useclass = bindClasses[k].id;
                        // Finally, post the event
                        DD_PostEvent(&ev);
                    }
                }
            }

            // Also send an up event for this binding if the currently
            // currently active command is in the class being disabled
            // and it has the highest id of the active bindClass commands
            // for this binding
            for(k = numBindClasses - 1; k > 0; --k)
            {
                if((k > classID && bindClasses[k].active &&
                   bind->commands[k].command[EVS_DOWN]) || k < classID)
                    break;

                if(!bindClasses[k].active && bind->commands[k].command[EVS_DOWN])
                {
                    event_t ev;
                    // que an up event for this down event.
                    ev.type = bind->type;
                    ev.data1 = bind->data1;
                    ev.state = EVS_UP;

                    // request a command in this class
                    ev.useclass = bindClasses[k].id;
                    // Finally, post the event
                    DD_PostEvent(&ev);
                }
            }
        }
    }

    return true;
}

void B_WriteToFile(FILE * file)
{
    int     i, j, k, count;
    char    buffer[20];

    for(k = 0; k < numBindClasses; ++k)
    {
        count = 0;
        for(i = 0; i < numBinds; i++)
        {
            for(j = 0; j < 3; ++j)
            {
                B_FormEventString(buffer, binds[i].type, j, binds[i].data1);
                if(binds[i].commands[k].command[j])
                {
                    fprintf(file, "bind ");
                    fprintf(file, "%s ", bindClasses[k].name);
                    fprintf(file, "%s", buffer);
                    fprintf(file, " \"");
                    M_WriteTextEsc(file, binds[i].commands[k].command[j]);
                    fprintf(file, "\"\n");
                    count++;
                }
            }
        }

        if(count > 0)
            fprintf(file,"\n");
    }
}

/*
 *  Returns the number of bindings. The buffer will be filled with the
 *  names of the events. Part of the Doomsday public API.
 */
int B_BindingsForCommand(char *command, char *buffer, int bindClass)
{
    int     i, j, k, count = 0;
    char    bindname[20];

    strcpy(buffer, "");

    if(bindClass > numBindClasses)
        return count;

    for(i = 0; i < numBinds; i++)
    {
        if(bindClass == -1) // check all bindclasses
        {
            for(k = 0; k < numBindClasses; ++k)
                for(j = 0; j < 3; ++j)
                    if(binds[i].commands[k].command[j])
                    {
                        B_FormEventString(bindname, binds[i].type, j, binds[i].data1);
                        if(!stricmp(command, binds[i].commands[k].command[j]))
                        {
                            if(buffer[0])        // If the buffer is not empty...
                                strcat(buffer, " ");
                            strcat(buffer, bindname);
                            count++;
                        }
                    }
        }
        else // only check bindClass
        {
            for(j = 0; j < 3; ++j)
                if(binds[i].commands[bindClass].command[j])
                {
                    B_FormEventString(bindname, binds[i].type, j, binds[i].data1);
                    if(!stricmp(command, binds[i].commands[bindClass].command[j]))
                    {
                        if(buffer[0])        // If the buffer is not empty...
                            strcat(buffer, " ");
                        strcat(buffer, bindname);
                        count++;
                    }
                }
        }
    }
    return count;
}

/*
 * Return the key code that corresponds the given key identifier name.
 * Part of the Doomsday public API.
 */
int DD_GetKeyCode(const char *key)
{
    int     code = getByShortName(key);

    return code ? code : key[0];
}
