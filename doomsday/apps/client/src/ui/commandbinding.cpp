/** @file commandbinding.cpp  Command binding record accessor.
 *
 * @authors Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/legacy/str.hh>
#include <de/block.h>
#include <de/log.h>
#include <de/recordvalue.h>
#include "ui/commandaction.h"
#include "clientapp.h"

#include "world/p_players.h" // P_ConsoleToLocal

#include "ui/inputsystem.h"
#include "ui/b_util.h"
#include "ui/axisinputcontrol.h"
#include "ui/buttoninputcontrol.h"
#include "ui/hatinputcontrol.h"

using namespace de;

void CommandBinding::resetToDefaults()
{
    Binding::resetToDefaults();

    def().addNumber("deviceId", -1);
    def().addNumber("controlId", -1);
    def().addNumber("type", int(E_TOGGLE));  ///< Type of event.
    def().addText  ("symbolicName", "");
    def().addNumber("test", int(None));
    def().addNumber("pos", 0);

    def().addText  ("command", "");  ///< Command to execute.
}

String CommandBinding::composeDescriptor()
{
    LOG_AS("CommandBinding");
    if (!*this) return "";

    String str = B_ControlDescToString(geti("deviceId"), ddeventtype_t(geti("type")), geti("controlId"));
    switch (geti("type"))
    {
    case E_TOGGLE:      str += B_ButtonStateToString(ControlTest(geti("test"))); break;
    case E_AXIS:        str += B_AxisPositionToString(ControlTest(geti("test")), getf("pos")); break;
    case E_ANGLE:       str += B_HatAngleToString(getf("pos")); break;
    case E_SYMBOLIC:    str += "-" + gets("symbolicName"); break;

    default: DE_ASSERT_FAIL("CommandBinding::composeDescriptor: Unknown bind.type"); break;
    }

    // Append any state conditions.
    const ArrayValue &conds = def().geta("condition");
    DE_FOR_EACH_CONST(ArrayValue::Elements, i, conds.elements())
    {
        str += " + " + B_ConditionToString(*(*i)->as<RecordValue>().record());
    }

    return str;
}

/**
 * Parse the main part of the event descriptor, with no conditions included.
 */
static bool doConfigure(CommandBinding &bind, const char *eventDesc, const char *command)
{
    DE_ASSERT(eventDesc);
    //InputSystem &isys = ClientApp::input();

    bind.resetToDefaults();
    // Take a copy of the command string.
    bind.def().set("command", String(command));

    // Parse the event descriptor.
    AutoStr *str = AutoStr_NewStd();

    // First, we expect to encounter a device name.
    eventDesc = Str_CopyDelim(str, eventDesc, '-');
    if (!Str_CompareIgnoreCase(str, "key"))
    {
        bind.def().set("deviceId", IDEV_KEYBOARD);
        bind.def().set("type", int(E_TOGGLE)); // Keyboards only have toggles (as far as we know).

        // Parse the key.
        eventDesc = Str_CopyDelim(str, eventDesc, '-');
        int controlId = 0;
        bool ok = B_ParseKeyId(controlId, Str_Text(str));
        if (!ok) return false;

        bind.def().set("controlId", controlId);

        // The final part of a key event is the state of the key toggle.
        eventDesc = Str_CopyDelim(str, eventDesc, '-');
        Binding::ControlTest test = Binding::ControlTest::None;
        ok = B_ParseButtonState(test, Str_Text(str));
        if (!ok) return false;

        bind.def().set("test", int(test));
    }
    else if (!Str_CompareIgnoreCase(str, "mouse"))
    {
        bind.def().set("deviceId", IDEV_MOUSE);

        // Next comes a button or axis name.
        eventDesc = Str_CopyDelim(str, eventDesc, '-');
        ddeventtype_t type = E_TOGGLE;
        int controlId = 0;
        bool ok = B_ParseMouseTypeAndId(type, controlId, Str_Text(str));
        if (!ok) return false;

        bind.def().set("type", type);
        bind.def().set("controlId", controlId);

        // The last part determines the toggle state or the axis position.
        eventDesc = Str_CopyDelim(str, eventDesc, '-');
        switch (bind.geti("type"))
        {
        case E_TOGGLE: {
            Binding::ControlTest test = Binding::ControlTest::None;
            ok = B_ParseButtonState(test, Str_Text(str));
            if (!ok) return false;

            bind.def().set("test", int(test));
            break; }

        case E_AXIS: {
            Binding::ControlTest test = Binding::ControlTest::None;
            float pos;
            ok = B_ParseAxisPosition(test, pos, Str_Text(str));
            if (!ok) return false;

            bind.def().set("test", int(test));
            bind.def().set("pos", pos);
            break; }

        default: DE_ASSERT_FAIL("InputSystem::configure: Invalid bind.type"); break;
        }
    }
    else if (!Str_CompareIgnoreCase(str, "joy") ||
            !Str_CompareIgnoreCase(str, "head"))
    {
        bind.def().set("deviceId", (!Str_CompareIgnoreCase(str, "joy")? IDEV_JOY1 : IDEV_HEAD_TRACKER));

        // Next part defined button, axis, or hat.
        eventDesc = Str_CopyDelim(str, eventDesc, '-');
        ddeventtype_t type = E_TOGGLE;
        int controlId = 0;
        bool ok = B_ParseJoystickTypeAndId(type, controlId, bind.geti("deviceId"), Str_Text(str));
        if (!ok) return false;

        bind.def().set("type", type);
        bind.def().set("controlId", controlId);

        // What is the state of the toggle, axis, or hat?
        eventDesc = Str_CopyDelim(str, eventDesc, '-');
        switch (bind.geti("type"))
        {
        case E_TOGGLE: {
            Binding::ControlTest test = Binding::ControlTest::None;
            ok = B_ParseButtonState(test, Str_Text(str));
            if (!ok) return false;

            bind.def().set("test", int(test));
            break; }

        case E_AXIS: {
            Binding::ControlTest test = Binding::ControlTest::None;
            float pos;
            ok = B_ParseAxisPosition(test, pos, Str_Text(str));
            if (!ok) return false;

            bind.def().set("test", int(test));
            bind.def().set("pos", pos);
            break; }

        case E_ANGLE: {
            float pos;
            ok = B_ParseHatAngle(pos, Str_Text(str));
            if (!ok) return false;

            bind.def().set("pos", pos);
            break; }

        default: DE_ASSERT_FAIL("InputSystem::configure: Invalid bind.type") break;
        }
    }
    else if (!Str_CompareIgnoreCase(str, "sym"))
    {
        // A symbolic event.
        bind.def().set("type", int(E_SYMBOLIC));
        bind.def().set("deviceId", -1);
        bind.def().set("symbolicName", eventDesc);

        eventDesc = nullptr;
    }
    else
    {
        LOG_INPUT_WARNING("Unknown device \"%s\"") << Str_Text(str);
        return false;
    }

    // Anything left that wasn't used?
    if (eventDesc)
    {
        LOG_INPUT_WARNING("Unrecognized \"%s\"") << eventDesc;
        return false;
    }

    // No errors detected.
    return true;
}

void CommandBinding::configure(const char *eventDesc, const char *command, bool assignNewId)
{
    DE_ASSERT(eventDesc);
    LOG_AS("CommandBinding");

    // The first part specifies the event condition.
    AutoStr *str = AutoStr_NewStd();
    eventDesc = Str_CopyDelim(str, eventDesc, '+');

    if (!doConfigure(*this, Str_Text(str), command))
    {
        throw ConfigureError("CommandBinding::configure", "Descriptor parse error");
    }

    // Any conditions?
    def()["condition"].array().clear();
    while (eventDesc)
    {
        // A new condition.
        eventDesc = Str_CopyDelim(str, eventDesc, '+');

        Record &cond = addCondition();
        if (!B_ParseBindingCondition(cond, Str_Text(str)))
        {
            throw ConfigureError("CommandBinding::configure", "Descriptor parse error");
        }
    }

    if (assignNewId)
    {
        def().set("id", newIdentifier());
    }
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
static void substituteInCommand(const String &command, const ddevent_t &event, ddstring_t *out)
{
    DE_ASSERT(out);
    for (auto ptr = command.begin(); ptr != command.end(); ++ptr)
    {
        if (*ptr == '%')
        {
            // Escape.
            ptr++;

            // Must have another character in the placeholder.
            if (!*ptr) break;

            if (*ptr == 'i')
            {
                int id = 0;
                switch (event.type)
                {
                case E_TOGGLE:   id = event.toggle.id;   break;
                case E_AXIS:     id = event.axis.id;     break;
                case E_ANGLE:    id = event.angle.id;    break;
                case E_SYMBOLIC: id = event.symbolic.id; break;

                default: break;
                }
                Str_Appendf(out, "%i", id);
            }
            else if (*ptr == 'p')
            {
                int id = 0;
                if (event.type == E_SYMBOLIC)
                {
                    id = P_ConsoleToLocal(event.symbolic.id);
                }
                Str_Appendf(out, "%i", id);
            }
            else if (*ptr == '%')
            {
                Str_AppendChar(out, char(*ptr));
            }
            continue;
        }

        iMultibyteChar mb;
        init_MultibyteChar(&mb, *ptr);
        Str_Append(out, mb.bytes);
    }
}

Action *CommandBinding::makeAction(const ddevent_t &event, const BindContext &context,
    bool respectHigherContexts) const
{
    if (geti("type") != event.type) return nullptr;

    const InputDevice *dev = nullptr;
    if (event.type != E_SYMBOLIC)
    {
        if (geti("deviceId") != event.device) return nullptr;

        dev = InputSystem::get().devicePtr(geti("deviceId"));
        if (!dev || !dev->isActive())
        {
            // The device is not active, there is no way this could get executed.
            return nullptr;
        }
    }

    switch (event.type)
    {
    case E_TOGGLE: {
        if (geti("controlId") != event.toggle.id)
            return nullptr;

        DE_ASSERT(dev);
        ButtonInputControl &button = dev->button(geti("controlId"));

        if (respectHigherContexts)
        {
            if (button.bindContext() != &context)
                return nullptr; // Shadowed by a more important active class.
        }

        // We're checking it, so clear the triggered flag.
        button.setBindContextAssociation(InputControl::Triggered, UnsetFlags);

        // Is the state as required?
        switch (ControlTest(geti("test")))
        {
        case ButtonStateAny:
            // Passes no matter what.
            break;

        case ButtonStateDown:
            if (event.toggle.state != ETOG_DOWN)
                return nullptr;
            break;

        case ButtonStateUp:
            if (event.toggle.state != ETOG_UP)
                return nullptr;
            break;

        case ButtonStateRepeat:
            if (event.toggle.state != ETOG_REPEAT)
                return nullptr;
            break;

        case ButtonStateDownOrRepeat:
            if (event.toggle.state == ETOG_UP)
                return nullptr;
            break;

        default: return nullptr;
        }
        break; }

    case E_AXIS:
        if (geti("controlId") != event.axis.id)
            return nullptr;

        DE_ASSERT(dev);
        if (dev->axis(geti("controlId")).bindContext() != &context)
            return nullptr; // Shadowed by a more important active class.

        // Is the position as required?
        if (!B_CheckAxisPosition(ControlTest(geti("test")), getf("pos"),
                                InputSystem::get().device(event.device).axis(event.axis.id)
                                    .translateRealPosition(event.axis.pos)))
            return nullptr;
        break;

    case E_ANGLE:
        if (geti("controlId") != event.angle.id)
            return nullptr;

        DE_ASSERT(dev);
        if (dev->hat(geti("controlId")).bindContext() != &context)
            return nullptr; // Shadowed by a more important active class.

        // Is the position as required?
        if (!fequal(event.angle.pos, getf("pos")))
            return nullptr;
        break;

    case E_SYMBOLIC:
        if (gets("symbolicName").compareWithCase(event.symbolic.name))
            return nullptr;
        break;

    default: return nullptr;
    }

    // Any conditions on the current state of the input devices?
    const ArrayValue &conds = def().geta("condition");
    DE_FOR_EACH_CONST(ArrayValue::Elements, i, conds.elements())
    {
        if (!B_CheckCondition(static_cast<Binding::CompiledConditionRecord *>
                              ((*i)->as<RecordValue>().record()), 0, &context))
            return nullptr;
    }

    // Substitute parameters in the command.
    AutoStr *command = Str_Reserve(AutoStr_NewStd(), gets("command").length());
    substituteInCommand(gets("command"), event, command);

    return new CommandAction(Str_Text(command), CMDS_BIND);
}
