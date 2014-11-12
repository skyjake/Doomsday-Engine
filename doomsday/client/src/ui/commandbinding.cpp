/** @file commandbinding.cpp  Command binding record accessor.
 *
 * @authors Copyright © 2009-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/commandbinding.h"

#include <de/str.hh>
#include <de/Block>
#include <de/Log>
#include "CommandAction"
#include "clientapp.h"

#include "world/p_players.h" // P_ConsoleToLocal

#include "ui/b_util.h"
#include "ui/inputdeviceaxiscontrol.h"
#include "ui/inputdevicebuttoncontrol.h"
#include "ui/inputdevicehatcontrol.h"

using namespace de;

static InputSystem &inputSys()
{
    return ClientApp::inputSystem();
}

void CommandBinding::resetToDefaults()
{
    def().addNumber("id", 0);  ///< Unique identifier.

    def().addNumber("deviceId", -1);
    def().addNumber("controlId", -1);
    def().addNumber("type", int(E_TOGGLE));  ///< Type of event.
    def().addText  ("symbolicName", "");
    def().addNumber("test", int(Condition::None));
    def().addNumber("pos", 0);

    def().addText  ("command", "");  ///< Command to execute.
}

String CommandBinding::composeDescriptor()
{
    LOG_AS("CommandBinding");
    if(!*this) return "";

    String str = B_ControlDescToString(geti("deviceId"), ddeventtype_t(geti("type")), geti("controlId"));
    switch(geti("type"))
    {
    case E_TOGGLE:      str += B_ButtonStateToString(Condition::ControlTest(geti("test"))); break;
    case E_AXIS:        str += B_AxisPositionToString(Condition::ControlTest(geti("test")), getf("pos")); break;
    case E_ANGLE:       str += B_HatAngleToString(getf("pos")); break;
    case E_SYMBOLIC:    str += "-" + gets("symbolicName"); break;

    default: DENG2_ASSERT(!"CommandBinding::composeDescriptor: Unknown bind.type"); break;
    }

    // Append any state conditions.
    for(BindingCondition const &cond : conditions)
    {
        str += " + " + B_ConditionToString(cond);
    }

    return str;
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
 * @param out      String with placeholders replaced.
 */
static void substituteInCommand(String const &command, ddevent_t const &event, ddstring_t *out)
{
    DENG2_ASSERT(out);
    Block const str = command.toUtf8();
    for(char const *ptr = str.constData(); *ptr; ptr++)
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
                switch(event.type)
                {
                case E_TOGGLE:   id = event.toggle.id;   break;
                case E_AXIS:     id = event.axis.id;     break;
                case E_ANGLE:    id = event.angle.id;    break;
                case E_SYMBOLIC: id = event.symbolic.id; break;

                default: break;
                }
                Str_Appendf(out, "%i", id);
            }
            else if(*ptr == 'p')
            {
                int id = 0;
                if(event.type == E_SYMBOLIC)
                {
                    id = P_ConsoleToLocal(event.symbolic.id);
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

Action *CommandBinding::makeAction(ddevent_t const &event, BindContext const &context,
    bool respectHigherContexts) const
{
    if(geti("type") != event.type)   return nullptr;

    InputDevice const *dev = nullptr;
    if(event.type != E_SYMBOLIC)
    {
        if(geti("deviceId") != event.device) return nullptr;

        dev = inputSys().devicePtr(geti("deviceId"));
        if(!dev || !dev->isActive())
        {
            // The device is not active, there is no way this could get executed.
            return nullptr;
        }
    }

    switch(event.type)
    {
    case E_TOGGLE: {
        if(geti("controlId") != event.toggle.id)
            return nullptr;

        DENG2_ASSERT(dev);
        InputDeviceButtonControl &button = dev->button(geti("controlId"));

        if(respectHigherContexts)
        {
            if(button.bindContext() != &context)
                return nullptr; // Shadowed by a more important active class.
        }

        // We're checking it, so clear the triggered flag.
        button.setBindContextAssociation(InputDeviceControl::Triggered, UnsetFlags);

        // Is the state as required?
        switch(Condition::ControlTest(geti("test")))
        {
        case Condition::ButtonStateAny:
            // Passes no matter what.
            break;

        case Condition::ButtonStateDown:
            if(event.toggle.state != ETOG_DOWN)
                return nullptr;
            break;

        case Condition::ButtonStateUp:
            if(event.toggle.state != ETOG_UP)
                return nullptr;
            break;

        case Condition::ButtonStateRepeat:
            if(event.toggle.state != ETOG_REPEAT)
                return nullptr;
            break;

        case Condition::ButtonStateDownOrRepeat:
            if(event.toggle.state == ETOG_UP)
                return nullptr;
            break;

        default: return nullptr;
        }
        break; }

    case E_AXIS:
        if(geti("controlId") != event.axis.id)
            return nullptr;

        DENG2_ASSERT(dev);
        if(dev->axis(geti("controlId")).bindContext() != &context)
            return nullptr; // Shadowed by a more important active class.

        // Is the position as required?
        if(!B_CheckAxisPosition(Condition::ControlTest(geti("test")), getf("pos"),
                                inputSys().device(event.device).axis(event.axis.id)
                                    .translateRealPosition(event.axis.pos)))
            return nullptr;
        break;

    case E_ANGLE:
        if(geti("controlId") != event.angle.id)
            return nullptr;

        DENG2_ASSERT(dev);
        if(dev->hat(geti("controlId")).bindContext() != &context)
            return nullptr; // Shadowed by a more important active class.

        // Is the position as required?
        if(event.angle.pos != getf("pos"))
            return nullptr;
        break;

    case E_SYMBOLIC:
        if(gets("symbolicName").compareWithCase(event.symbolic.name))
            return nullptr;
        break;

    default: return nullptr;
    }

    // Any conditions on the current state of the input devices?
    for(BindingCondition const &cond : conditions)
    {
        if(!B_CheckCondition(&cond, 0, nullptr))
            return nullptr;
    }

    // Substitute parameters in the command.
    AutoStr *command = Str_Reserve(AutoStr_NewStd(), gets("command").length());
    substituteInCommand(gets("command"), event, command);

    return new CommandAction(Str_Text(command), CMDS_BIND);
}
