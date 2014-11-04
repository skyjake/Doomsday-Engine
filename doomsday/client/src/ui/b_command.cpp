/** @file b_command.cpp  Input system, event => command binding.
 *
 * @authors Copyright © 2007-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h" // strdup macro
#include "ui/b_command.h"

#include <de/memory.h>
#include <de/vector1.h>
#include "clientapp.h"
#include "world/p_players.h" // P_ConsoleToLocal
#include "ui/b_main.h"
#include "ui/inputdevice.h"
#include "ui/inputdeviceaxiscontrol.h"
#include "ui/inputdevicebuttoncontrol.h"
#include "ui/inputdevicehatcontrol.h"
#include "CommandAction"

#include <de/Log>

using namespace de;

static inline InputSystem &inputSys()
{
    return ClientApp::inputSystem();
}

void B_InitCommandBindingList(CommandBinding *listRoot)
{
    DENG2_ASSERT(listRoot);
    de::zapPtr(listRoot);
    listRoot->next = listRoot;
    listRoot->prev = listRoot;
}

void B_DestroyCommandBindingList(CommandBinding *listRoot)
{
    DENG2_ASSERT(listRoot);
    while(listRoot->next != listRoot)
    {
        B_DestroyCommandBinding(listRoot->next);
    }
}

CommandBinding *B_AllocCommandBinding()
{
    CommandBinding *eb = (CommandBinding *) M_Calloc(sizeof(CommandBinding));
    eb->id = B_NewIdentifier();
    return eb;
}

/**
 * Allocates a device state condition within an event binding.
 *
 * @return  Pointer to the new condition, which should be filled with the condition parameters.
 */
static statecondition_t *B_AllocCommandBindingCondition(CommandBinding *eb)
{
    DENG2_ASSERT(eb);
    eb->conds = (statecondition_t *) M_Realloc(eb->conds, ++eb->numConds * sizeof(statecondition_t));
    de::zap(eb->conds[eb->numConds - 1]);
    return &eb->conds[eb->numConds - 1];
}

/**
 * Parse the main part of the event descriptor, with no conditions included.
 */
static dd_bool B_ParseEvent(CommandBinding *eb, char const *desc)
{
    DENG2_ASSERT(eb);
    LOG_AS("B_ParseEvent");

    AutoStr *str = AutoStr_NewStd();

    // First, we expect to encounter a device name.
    desc = Str_CopyDelim(str, desc, '-');
    if(!Str_CompareIgnoreCase(str, "key"))
    {
        eb->deviceId = IDEV_KEYBOARD;
        // Keyboards only have toggles (as far as we know).
        eb->type = E_TOGGLE;

        // Parse the key.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseKeyId(Str_Text(str), &eb->controlId))
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
        eb->deviceId = IDEV_MOUSE;

        // Next comes a button or axis name.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseMouseTypeAndId(Str_Text(str), &eb->type, &eb->controlId))
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
    else if(!Str_CompareIgnoreCase(str, "joy") ||
            !Str_CompareIgnoreCase(str, "head"))
    {
        eb->deviceId = (!Str_CompareIgnoreCase(str, "joy")? IDEV_JOY1 : IDEV_HEAD_TRACKER);

        // Next part defined button, axis, or hat.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseJoystickTypeAndId(inputSys().device(eb->deviceId), Str_Text(str), &eb->type, &eb->controlId))
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
        // A symbolic event.
        eb->type         = E_SYMBOLIC;
        eb->deviceId       = 0;
        eb->symbolicName = strdup(desc);
        desc = nullptr;
    }
    else
    {
        LOG_INPUT_WARNING("Unknown device \"%s\"") << Str_Text(str);
        return false;
    }

    // Anything left that wasn't used?
    if(desc)
    {
        LOG_INPUT_WARNING("Unrecognized \"%s\"") << desc;
        return false;
    }

    // No errors detected.
    return true;
}

dd_bool B_ParseEventDescriptor(CommandBinding *eb, char const *desc)
{
    AutoStr *str = AutoStr_NewStd();

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
        // A new condition.
        desc = Str_CopyDelim(str, desc, '+');

        statecondition_t *cond = B_AllocCommandBindingCondition(eb);
        if(!B_ParseStateCondition(cond, Str_Text(str)))
        {
            // Failure parsing the condition.
            return false;
        }
    }

    // Success.
    return true;
}

void B_DestroyCommandBinding(CommandBinding *eb)
{
    if(!eb) return;

    DENG2_ASSERT(eb->id != 0);

    // Unlink first, if linked.
    if(eb->prev)
    {
        eb->prev->next = eb->next;
        eb->next->prev = eb->prev;
    }

    M_Free(eb->command);
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
static void B_SubstituteInCommand(char const *command, ddevent_t const *event,
                                  CommandBinding * /*eb*/, ddstring_t *out)
{
    DENG2_ASSERT(command && event && out);

    for(char const *ptr = command; *ptr; ptr++)
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
                case E_TOGGLE:   id = event->toggle.id;   break;
                case E_AXIS:     id = event->axis.id;     break;
                case E_ANGLE:    id = event->angle.id;    break;
                case E_SYMBOLIC: id = event->symbolic.id; break;

                default: break;
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

Action *CommandBinding_ActionForEvent(CommandBinding *eb, ddevent_t const *event,
    BindContext const *bindContext, bool respectHigherAssociatedContexts)
{
    DENG2_ASSERT(eb && bindContext);

    if(eb->deviceId != event->device) return nullptr;
    if(eb->type != event->type) return nullptr;

    InputDevice *dev = nullptr;
    if(event->type != E_SYMBOLIC)
    {
        dev = inputSys().devicePtr(eb->deviceId);
        if(!dev || !dev->isActive())
        {
            // The device is not active, there is no way this could get executed.
            return nullptr;
        }
    }
    DENG2_ASSERT(dev);

    switch(event->type)
    {
    case E_TOGGLE: {
        if(eb->controlId != event->toggle.id)
            return nullptr;

        InputDeviceButtonControl &button = dev->button(eb->controlId);

        if(respectHigherAssociatedContexts)
        {
            if(button.bindContext() != bindContext)
                return nullptr; // Shadowed by a more important active class.
        }

        // We're checking it, so clear the triggered flag.
        button.setBindContextAssociation(InputDeviceControl::Triggered, UnsetFlags);

        // Is the state as required?
        switch(eb->state)
        {
        case EBTOG_UNDEFINED:
            // Passes no matter what.
            break;

        case EBTOG_DOWN:
            if(event->toggle.state != ETOG_DOWN)
                return nullptr;
            break;

        case EBTOG_UP:
            if(event->toggle.state != ETOG_UP)
                return nullptr;
            break;

        case EBTOG_REPEAT:
            if(event->toggle.state != ETOG_REPEAT)
                return nullptr;
            break;

        case EBTOG_PRESS:
            if(event->toggle.state == ETOG_UP)
                return nullptr;
            break;

        default: return nullptr;
        }
        break; }

    case E_AXIS:
        if(eb->controlId != event->axis.id)
            return nullptr;

        if(bindContext && dev->axis(eb->controlId).bindContext() != bindContext)
            return nullptr; // Shadowed by a more important active class.

        // Is the position as required?
        if(!B_CheckAxisPos(eb->state, eb->pos,
                           inputSys().device(event->device).axis(event->axis.id)
                               .translateRealPosition(event->axis.pos)))
            return nullptr;
        break;

    case E_ANGLE:
        if(eb->controlId != event->angle.id)
            return nullptr;

        if(bindContext && dev->hat(eb->controlId).bindContext() != bindContext)
            return nullptr; // Shadowed by a more important active class.

        // Is the position as required?
        if(event->angle.pos != eb->pos)
            return nullptr;
        break;

    case E_SYMBOLIC:
        if(strcmp(event->symbolic.name, eb->symbolicName))
            return nullptr;
        break;

    default: return nullptr;
    }

    // Any conditions on the current state of the input devices?
    for(int i = 0; i < eb->numConds; ++i)
    {
        if(!B_CheckCondition(&eb->conds[i], 0, nullptr))
            return nullptr;
    }

    // Substitute parameters in the command.
    ddstring_t command; Str_Init(&command);
    Str_Reserve(&command, strlen(eb->command));
    B_SubstituteInCommand(eb->command, event, eb, &command);

    Action *act = new CommandAction(Str_Text(&command), CMDS_BIND);

    Str_Free(&command);
    return act;
}

void CommandBinding_ToString(CommandBinding const *eb, ddstring_t *str)
{
    DENG2_ASSERT(eb && str);

    Str_Clear(str);
    B_AppendControlDescToString(inputSys().device(eb->deviceId), eb->type, eb->controlId, str);

    switch(eb->type)
    {
    case E_TOGGLE:
        B_AppendToggleStateToString(eb->state, str);
        break;

    case E_AXIS:
        B_AppendAxisPositionToString(eb->state, eb->pos, str);
        break;

    case E_ANGLE:
        B_AppendAnglePositionToString(eb->pos, str);
        break;

    case E_SYMBOLIC:
        Str_Appendf(str, "-%s", eb->symbolicName);
        break;

    default: break;
    }

    // Append any state conditions.
    for(int i = 0; i < eb->numConds; ++i)
    {
        Str_Append(str, " + ");
        B_AppendConditionToString(&eb->conds[i], str);
    }
}
