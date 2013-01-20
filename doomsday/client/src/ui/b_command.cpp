/** @file b_command.cpp
 *
 * @authors Copyright © 2007-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * Event-Command Bindings.
 */

// HEADER FILES ------------------------------------------------------------

#include <de/memory.h>

#include "de_console.h"
#include "de_misc.h"

#include "map/p_players.h"
#include "ui/b_main.h"
#include "ui/b_command.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void B_InitCommandBindingList(evbinding_t* listRoot)
{
    memset(listRoot, 0, sizeof(*listRoot));
    listRoot->next = listRoot;
    listRoot->prev = listRoot;
}

void B_DestroyCommandBindingList(evbinding_t* listRoot)
{
    while(listRoot->next != listRoot)
    {
        B_DestroyCommandBinding(listRoot->next);
    }
}

/**
 * Allocates a new command binding and gives it a unique identifier.
 */
static evbinding_t* B_AllocCommandBinding(void)
{
    evbinding_t* eb = (evbinding_t *) M_Calloc(sizeof(evbinding_t));
    eb->bid = B_NewIdentifier();
    return eb;
}

/**
 * Allocates a device state condition within an event binding.
 *
 * @return  Pointer to the new condition, which should be filled with the condition parameters.
 */
static statecondition_t* B_AllocCommandBindingCondition(evbinding_t* eb)
{
    eb->conds = (statecondition_t *) M_Realloc(eb->conds, ++eb->numConds * sizeof(statecondition_t));
    memset(&eb->conds[eb->numConds - 1], 0, sizeof(statecondition_t));
    return &eb->conds[eb->numConds - 1];
}

/**
 * Parse the main part of the event descriptor, with no conditions included.
 */
boolean B_ParseEvent(evbinding_t* eb, const char* desc)
{
    AutoStr* str = AutoStr_NewStd();

    // First, we expect to encounter a device name.
    desc = Str_CopyDelim(str, desc, '-');
    if(!Str_CompareIgnoreCase(str, "key"))
    {
        eb->device = IDEV_KEYBOARD;
        // Keyboards only have toggles (as far as we know).
        eb->type = E_TOGGLE;

        // Parse the key.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseKeyId(Str_Text(str), &eb->id))
        {
            return false;
        }

        // The final part of a key event is the state of the key toggle.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseToggleState(Str_Text(str), &eb->state))
        {
            return false;
        }
    }
    else if(!Str_CompareIgnoreCase(str, "mouse"))
    {
        eb->device = IDEV_MOUSE;

        // Next comes a button or axis name.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseMouseTypeAndId(Str_Text(str), &eb->type, &eb->id))
        {
            return false;
        }

        // The last part determines the toggle state or the axis position.
        desc = Str_CopyDelim(str, desc, '-');
        if(eb->type == E_TOGGLE)
        {
            if(!B_ParseToggleState(Str_Text(str), &eb->state))
            {
                return false;
            }
        }
        else // Axis position.
        {
            if(!B_ParseAxisPosition(Str_Text(str), &eb->state, &eb->pos))
            {
                return false;
            }
        }
    }
    else if(!Str_CompareIgnoreCase(str, "joy"))
    {
        eb->device = IDEV_JOY1;

        // Next part defined button, axis, or hat.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseJoystickTypeAndId(eb->device, Str_Text(str), &eb->type, &eb->id))
        {
            return false;
        }

        // What is the state of the toggle, axis, or hat?
        desc = Str_CopyDelim(str, desc, '-');
        if(eb->type == E_TOGGLE)
        {
            if(!B_ParseToggleState(Str_Text(str), &eb->state))
            {
                return false;
            }
        }
        else if(eb->type == E_AXIS)
        {
            if(!B_ParseAxisPosition(Str_Text(str), &eb->state, &eb->pos))
            {
                return false;
            }
        }
        else // Angle.
        {
            if(!B_ParseAnglePosition(Str_Text(str), &eb->pos))
            {
                return false;
            }
        }
    }
    else if(!Str_CompareIgnoreCase(str, "sym"))
    {
        // It must be a symbolic event.
        eb->type = E_SYMBOLIC;
        eb->device = 0;
        eb->symbolicName = strdup(desc);
        desc = NULL;
    }
    else
    {
        Con_Message("B_ParseEvent: Device \"%s\" unknown.\n", Str_Text(str));
        return false;
    }

    // Anything left that wasn't used?
    if(desc)
    {
        Con_Message("B_ParseEvent: Unrecognized \"%s\".\n", desc);
        return false;
    }

    // No errors detected.
    return true;
}

/**
 * Parses a textual descriptor of the conditions for triggering an event-command binding.
 * eventparams{+cond}*
 *
 * @param eb  The results of the parsing are stored here.
 * @param desc  Descriptor containing event information and possible additional conditions.
 *
 * @return  @c true, if successful; otherwise @c false.
 */
boolean B_ParseEventDescriptor(evbinding_t* eb, const char* desc)
{
    AutoStr* str = AutoStr_NewStd();

    // The main part, i.e., the first part.
    desc = Str_CopyDelim(str, desc, '+');

    if(!B_ParseEvent(eb, Str_Text(str)))
    {
        // Failure parsing the event.
        return false;
    }

    // Any conditions?
    while(desc)
    {
        statecondition_t *cond;

        // A new condition.
        desc = Str_CopyDelim(str, desc, '+');

        cond = B_AllocCommandBindingCondition(eb);
        if(!B_ParseStateCondition(cond, Str_Text(str)))
        {
            // Failure parsing the condition.
            return false;
        }
    }

    // Success.
    return true;
}

/**
 * Creates a new event-command binding.
 *
 * @param bindsList     List of bindings where the binding will be added.
 * @param desc          Descriptor of the event.
 * @param command       Command that will be executed by the binding.
 *
 * @return              New binding, or @c NULL if there was an error.
 */
evbinding_t* B_NewCommandBinding(evbinding_t* bindsList, const char* desc,
                                 const char* command)
{
    evbinding_t*        eb = B_AllocCommandBinding();

    // Parse the description of the event.
    if(!B_ParseEventDescriptor(eb, desc))
    {
        // Error in parsing, failure to create binding.
        B_DestroyCommandBinding(eb);
        return NULL;
    }

    // The command string.
    eb->command = strdup(command);

    // Link it into the list.
    eb->next = bindsList;
    eb->prev = bindsList->prev;
    bindsList->prev->next = eb;
    bindsList->prev = eb;

    return eb;
}

/**
 * Destroys a command binding.
 *
 * @param eb  Command binding to destroy.
 */
void B_DestroyCommandBinding(evbinding_t* eb)
{
    if(!eb)
        return;

    assert(eb->bid != 0);

    // Unlink first, if linked.
    if(eb->prev)
    {
        eb->prev->next = eb->next;
        eb->next->prev = eb->prev;
    }

    if(eb->command)
        M_Free(eb->command);
    if(eb->conds)
        M_Free(eb->conds);
    free(eb->symbolicName);

    M_Free(eb);
}

/**
 * Substitute placeholders in a command string. Placeholders consist of two characters,
 * the first being a %. Use %% to output a plain % character.
 *
 * - <code>%i</code>: id member of the event
 * - <code>%p</code>: (symbolic events only) local player number
 *
 * @param command  Original command string with the placeholders.
 * @param event    Event data.
 * @param eb       Binding data.
 * @param out      String with placeholders replaced.
 */
void B_SubstituteInCommand(const char* command, ddevent_t* event, evbinding_t* eb, ddstring_t* out)
{
    const char* ptr = command;

    for(; *ptr; ptr++)
    {
        if(*ptr == '%')
        {
            // Escape.
            ptr++;

            // Must have another character in the placeholder.
            if(!*ptr) break;

            if(*ptr == 'i')
            {
                int id = 0;
                switch(event->type)
                {
                    case E_TOGGLE:
                        id = event->toggle.id;
                        break;

                    case E_AXIS:
                        id = event->axis.id;
                        break;

                    case E_ANGLE:
                        id = event->angle.id;
                        break;

                    case E_SYMBOLIC:
                        id = event->symbolic.id;
                        break;

                    default:
                        break;
                }
                Str_Appendf(out, "%i", id);
            }
            else if(*ptr == 'p')
            {
                int id = 0;
                if(event->type == E_SYMBOLIC)
                {
                    id = P_ConsoleToLocal(event->symbolic.id);
                }
                Str_Appendf(out, "%i", id);
            }
            else if(*ptr == '%')
            {
                Str_AppendChar(out, *ptr);
            }
            continue;
        }

        Str_AppendChar(out, *ptr);
    }
}

boolean B_TryCommandBinding(evbinding_t* eb, ddevent_t* event, struct bcontext_s* eventClass)
{
    int         i;
    inputdev_t* dev;
    ddstring_t  command;

    if(eb->device != event->device || eb->type != event->type)
        return false;

    if(event->type != E_SYMBOLIC)
    {
        dev = I_GetDevice(eb->device, true);
        if(!dev)
        {
            // The device is not active, there is no way this could get executed.
            return false;
        }
    }

    switch(event->type)
    {
    case E_TOGGLE:
        if(eb->id != event->toggle.id)
            return false;
        if(eventClass && dev->keys[eb->id].assoc.bContext != eventClass)
            return false; // Shadowed by a more important active class.

        // We're checking it, so clear the triggered flag.
        dev->keys[eb->id].assoc.flags &= ~IDAF_TRIGGERED;

        // Is the state as required?
        switch(eb->state)
        {
            case EBTOG_UNDEFINED:
                // Passes no matter what.
                break;

            case EBTOG_DOWN:
                if(event->toggle.state != ETOG_DOWN)
                    return false;
                break;

            case EBTOG_UP:
                if(event->toggle.state != ETOG_UP)
                    return false;
                break;

            case EBTOG_REPEAT:
                if(event->toggle.state != ETOG_REPEAT)
                    return false;
                break;

            case EBTOG_PRESS:
                if(event->toggle.state == ETOG_UP)
                    return false;
                break;

            default:
                return false;
        }
        break;

    case E_AXIS:
        if(eb->id != event->axis.id)
            return false;
        if(eventClass && dev->axes[eb->id].assoc.bContext != eventClass)
            return false; // Shadowed by a more important active class.

        // Is the position as required?
        if(!B_CheckAxisPos(eb->state, eb->pos,
                           I_TransformAxis(I_GetDevice(event->device, false),
                                           event->axis.id, event->axis.pos)))
            return false;
        break;

    case E_ANGLE:
        if(eb->id != event->angle.id)
            return false;
        if(eventClass && dev->hats[eb->id].assoc.bContext != eventClass)
            return false; // Shadowed by a more important active class.
        // Is the position as required?
        if(event->angle.pos != eb->pos)
            return false;
        break;

    case E_SYMBOLIC:
        if(strcmp(event->symbolic.name, eb->symbolicName))
            return false;
        break;

    default:
        return false;
    }

    // Any conditions on the current state of the input devices?
    for(i = 0; i < eb->numConds; ++i)
    {
        if(!B_CheckCondition(&eb->conds[i], 0, NULL))
            return false;
    }

    // Substitute parameters in the command.
    Str_Init(&command);
    Str_Reserve(&command, strlen(eb->command));
    B_SubstituteInCommand(eb->command, event, eb, &command);

    // Do the command.
    Con_Executef(CMDS_BIND, false, "%s", Str_Text(&command));

    Str_Free(&command);
    return true;
}

/**
 * Does the opposite of the B_Parse* methods for event descriptor, including the
 * state conditions.
 */
void B_EventBindingToString(const evbinding_t* eb, ddstring_t* str)
{
    int         i;

    Str_Clear(str);
    B_AppendDeviceDescToString(eb->device, eb->type, eb->id, str);

    if(eb->type == E_TOGGLE)
    {
        B_AppendToggleStateToString(eb->state, str);
    }
    else if(eb->type == E_AXIS)
    {
        B_AppendAxisPositionToString(eb->state, eb->pos, str);
    }
    else if(eb->type == E_ANGLE)
    {
        B_AppendAnglePositionToString(eb->pos, str);
    }
    else if(eb->type == E_SYMBOLIC)
    {
        Str_Appendf(str, "-%s", eb->symbolicName);
    }

    // Append any state conditions.
    for(i = 0; i < eb->numConds; ++i)
    {
        Str_Append(str, " + ");
        B_AppendConditionToString(&eb->conds[i], str);
    }
}
