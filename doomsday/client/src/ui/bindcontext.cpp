/** @file bindcontext.cpp  Input system binding contexts.
 *
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/bindcontext.h"

#include <QList>
#include <QSet>
#include <QtAlgorithms>
#include <de/Action>
#include <de/Log>
#include "clientapp.h"

#include "world/p_players.h" // P_ConsoleToLocal

#include "CommandAction"
#include "CommandBinding"
#include "ImpulseBinding"
#include "ui/b_util.h"
#include "ui/inputdevice.h"
#include "ui/playerimpulse.h"

/// @todo: remove
#include "ui/inputdeviceaxiscontrol.h"
#include "ui/inputdevicebuttoncontrol.h"
#include "ui/inputdevicehatcontrol.h"
// todo ends

using namespace de;

static inline InputSystem &inputSys()
{
    return ClientApp::inputSystem();
}

DENG2_PIMPL(BindContext)
{
    bool active  = false;  ///< @c true= Bindings are active.
    bool protect = false;  ///< @c true= Prevent explicit end user (de)activation.
    String name;           ///< Symbolic.

    // Acquired device states, unless higher-priority contexts override.
    typedef QSet<int> DeviceIds;
    DeviceIds acquireDevices;
    bool acquireAllDevices = false;  ///< @c true= will ignore @var acquireDevices.

    typedef QList<Record *> CommandBindings;
    CommandBindings commandBinds;

    typedef QList<Record *> ImpulseBindings;
    ImpulseBindings impulseBinds[DDMAXPLAYERS];  ///< Group bindings for each local player.

    DDFallbackResponderFunc ddFallbackResponder = nullptr;
    FallbackResponderFunc fallbackResponder     = nullptr;

    Instance(Public *i) : Base(i) {}

    DENG2_PIMPL_AUDIENCE(ActiveChange)
    DENG2_PIMPL_AUDIENCE(AcquireDeviceChange)
    DENG2_PIMPL_AUDIENCE(BindingAddition)
};

DENG2_AUDIENCE_METHOD(BindContext, ActiveChange)
DENG2_AUDIENCE_METHOD(BindContext, AcquireDeviceChange)
DENG2_AUDIENCE_METHOD(BindContext, BindingAddition)

BindContext::BindContext(String const &name) : d(new Instance(this))
{
    setName(name);
}

BindContext::~BindContext()
{
    clearAllBindings();
}

bool BindContext::isActive() const
{
    return d->active;
}

bool BindContext::isProtected() const
{
    return d->protect;
}

void BindContext::protect(bool yes)
{
    d->protect = yes;
}

String BindContext::name() const
{
    return d->name;
}

void BindContext::setName(String const &newName)
{
    d->name = newName;
}

void BindContext::activate(bool yes)
{
    if(d->active == yes) return;

    LOG_AS("BindContext");
    LOG_INPUT_VERBOSE("%s " _E(b) "'%s'") << (yes? "Activating" : "Deactivating") << d->name;
    d->active = yes;

    // Notify interested parties.
    DENG2_FOR_AUDIENCE2(ActiveChange, i) i->bindContextActiveChanged(*this);
}

void BindContext::acquire(int deviceId, bool yes)
{
    DENG2_ASSERT(deviceId >= 0 && deviceId < NUM_INPUT_DEVICES);
    int const countBefore = d->acquireDevices.count();

    if(yes) d->acquireDevices.insert(deviceId);
    else    d->acquireDevices.remove(deviceId);

    if(countBefore != d->acquireDevices.count())
    {
        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(AcquireDeviceChange, i) i->bindContextAcquireDeviceChanged(*this);
    }
}

void BindContext::acquireAll(bool yes)
{
    if(d->acquireAllDevices != yes)
    {
        d->acquireAllDevices = yes;

        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(AcquireDeviceChange, i) i->bindContextAcquireDeviceChanged(*this);
    }
}

bool BindContext::willAcquire(int deviceId) const
{
    return d->acquireAllDevices || d->acquireDevices.contains(deviceId);
}


bool BindContext::willAcquireAll() const
{
    return d->acquireAllDevices;
}

void BindContext::setDDFallbackResponder(DDFallbackResponderFunc newResponderFunc)
{
    d->ddFallbackResponder = newResponderFunc;
}

void BindContext::setFallbackResponder(FallbackResponderFunc newResponderFunc)
{
    d->fallbackResponder = newResponderFunc;
}

void BindContext::clearAllBindings()
{
    LOG_AS("BindContext");
    qDeleteAll(d->commandBinds);
    d->commandBinds.clear();
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        qDeleteAll(d->impulseBinds[i]);
        d->impulseBinds[i].clear();
    }
    LOG_INPUT_VERBOSE(_E(b) "'%s'" _E(.) " cleared") << d->name;
}

void BindContext::deleteMatching(Record const *cmdBinding, Record const *impBinding)
{
    Record *foundCmd = nullptr;
    Record *foundImp = nullptr;

    while(findMatchingBinding(cmdBinding, impBinding, &foundCmd, &foundImp))
    {
        // Only either foundCmd or foundImp is returned as non-NULL.
        int bindId = (foundCmd? foundCmd->geti("id") : (foundImp? foundImp->geti("id") : 0));
        if(bindId)
        {
            LOG_INPUT_VERBOSE("Deleting binding %i, it has been overridden by binding %i")
                    << bindId << (cmdBinding? cmdBinding->geti("id") : impBinding->geti("id"));
            deleteBinding(bindId);
        }
    }
}

Record *BindContext::bindCommand(char const *eventDesc, char const *command)
{
    DENG2_ASSERT(eventDesc && command && command[0]);
    LOG_AS("BindContext");
    try
    {
        std::unique_ptr<Record> newBind(new Record);
        inputSys().configureCommandBinding(*newBind, eventDesc, command);

        Record *bind = newBind.get();
        d->commandBinds.prepend(newBind.release());

        LOG_INPUT_VERBOSE("Command " _E(b) "\"%s\"" _E(.) " now bound to " _E(b) "\"%s\"" _E(.) " in " _E(b) "'%s'" _E(.)
                          " with binding Id " _E(b) "%i")
                << command << eventDesc << d->name << bind->geti("id");

        /// @todo: In interactive binding mode, should ask the user if the
        /// replacement is ok. For now, just delete the other bindings.
        deleteMatching(bind, nullptr);

        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(BindingAddition, i) i->bindContextBindingAdded(*this, *bind, true/*is-command*/);

        return bind;
    }
    catch(InputSystem::ConfigureError const &)
    {}
    return nullptr;
}

Record *BindContext::bindImpulse(char const *ctrlDesc, PlayerImpulse const &impulse,
    int localPlayer)
{
    DENG2_ASSERT(ctrlDesc);
    DENG2_ASSERT(localPlayer >= 0 && localPlayer < DDMAXPLAYERS);
    LOG_AS("BindContext");
    try
    {
        std::unique_ptr<Record> newBind(new Record);
        inputSys().configureImpulseBinding(*newBind, ctrlDesc, impulse.id(), localPlayer);

        Record *bind = newBind.get();
        d->impulseBinds[localPlayer].append(newBind.release());

        LOG_INPUT_VERBOSE("Impulse " _E(b) "'%s'" _E(.) " of player%i now bound to \"%s\" in " _E(b) "'%s'" _E(.)
                          " with binding Id " _E(b) "%i")
                << impulse.name() << (localPlayer + 1) << ctrlDesc << d->name << bind->geti("id");

        /// @todo: In interactive binding mode, should ask the user if the
        /// replacement is ok. For now, just delete the other bindings.
        deleteMatching(nullptr, bind);

        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(BindingAddition, i) i->bindContextBindingAdded(*this, *bind, false/*is-impulse*/);

        return bind;
    }
    catch(InputSystem::ConfigureError const &)
    {}
    return nullptr;
}

Record *BindContext::findCommandBinding(char const *command, int deviceId) const
{
    if(command && command[0])
    {
        for(Record const *rec : d->commandBinds)
        {
            CommandBinding bind(*rec);
            if(bind.gets("command").compareWithoutCase(command)) continue;

            if((deviceId < 0 || deviceId >= NUM_INPUT_DEVICES) || bind.geti("deviceId") == deviceId)
            {
                return const_cast<Record *>(rec);
            }
        }
    }
    return nullptr;
}

Record *BindContext::findImpulseBinding(int deviceId, ibcontroltype_t bindType, int controlId) const
{
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    for(Record const *rec : d->impulseBinds[i])
    {
        ImpulseBinding bind(*rec);
        if(bind.geti("type")      == bindType &&
           bind.geti("deviceId")  == deviceId &&
           bind.geti("controlId") == controlId)
        {
            return const_cast<Record *>(rec);
        }
    }
    return nullptr;
}

bool BindContext::deleteBinding(int id)
{
    // Check if it is one of the command bindings.
    for(int i = 0; i < d->commandBinds.count(); ++i)
    {
        Record *rec = d->commandBinds.at(i);
        if(rec->geti("id") == id)
        {
            d->commandBinds.removeAt(i);
            delete rec;
            return true;
        }
    }

    // How about one of the impulse bindings?
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    for(int k = 0; k < d->impulseBinds[i].count(); ++k)
    {
        Record *rec = d->impulseBinds[i].at(k);
        if(rec->geti("id") == id)
        {
            d->impulseBinds[i].removeAt(k);
            delete rec;
            return true;
        }
    }

    return false;
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

/**
 * Evaluate the given binding and event pair and attempt to generate an Action.
 *
 * @param context                Context in which @a bind exists.
 * @param bind                   Binding to match against.
 * @param event                  Event to match against.
 * @param respectHigherContexts  Bindings are shadowed by higher active contexts.
 *
 * @return Action instance (caller gets ownership), or @c nullptr if no matching.
 */
static Action *commandActionFor(BindContext const &context, CommandBinding const &bind,
    ddevent_t const &event, bool respectHigherContexts)
{
    if(bind.geti("type") != event.type)   return nullptr;

    InputDevice const *dev = nullptr;
    if(event.type != E_SYMBOLIC)
    {
        if(bind.geti("deviceId") != event.device) return nullptr;

        dev = inputSys().devicePtr(bind.geti("deviceId"));
        if(!dev || !dev->isActive())
        {
            // The device is not active, there is no way this could get executed.
            return nullptr;
        }
    }

    switch(event.type)
    {
    case E_TOGGLE: {
        if(bind.geti("controlId") != event.toggle.id)
            return nullptr;

        DENG2_ASSERT(dev);
        InputDeviceButtonControl &button = dev->button(bind.geti("controlId"));

        if(respectHigherContexts)
        {
            if(button.bindContext() != &context)
                return nullptr; // Shadowed by a more important active class.
        }

        // We're checking it, so clear the triggered flag.
        button.setBindContextAssociation(InputDeviceControl::Triggered, UnsetFlags);

        // Is the state as required?
        switch(BindingCondition::ControlTest(bind.geti("test")))
        {
        case BindingCondition::ButtonStateAny:
            // Passes no matter what.
            break;

        case BindingCondition::ButtonStateDown:
            if(event.toggle.state != ETOG_DOWN)
                return nullptr;
            break;

        case BindingCondition::ButtonStateUp:
            if(event.toggle.state != ETOG_UP)
                return nullptr;
            break;

        case BindingCondition::ButtonStateRepeat:
            if(event.toggle.state != ETOG_REPEAT)
                return nullptr;
            break;

        case BindingCondition::ButtonStateDownOrRepeat:
            if(event.toggle.state == ETOG_UP)
                return nullptr;
            break;

        default: return nullptr;
        }
        break; }

    case E_AXIS:
        if(bind.geti("controlId") != event.axis.id)
            return nullptr;

        DENG2_ASSERT(dev);
        if(dev->axis(bind.geti("controlId")).bindContext() != &context)
            return nullptr; // Shadowed by a more important active class.

        // Is the position as required?
        if(!B_CheckAxisPosition(BindingCondition::ControlTest(bind.geti("test")), bind.getf("pos"),
                                inputSys().device(event.device).axis(event.axis.id)
                                    .translateRealPosition(event.axis.pos)))
            return nullptr;
        break;

    case E_ANGLE:
        if(bind.geti("controlId") != event.angle.id)
            return nullptr;

        DENG2_ASSERT(dev);
        if(dev->hat(bind.geti("controlId")).bindContext() != &context)
            return nullptr; // Shadowed by a more important active class.

        // Is the position as required?
        if(event.angle.pos != bind.getf("pos"))
            return nullptr;
        break;

    case E_SYMBOLIC:
        if(bind.gets("symbolicName").compareWithCase(event.symbolic.name))
            return nullptr;
        break;

    default: return nullptr;
    }

    // Any conditions on the current state of the input devices?
    for(BindingCondition const &cond : bind.conditions)
    {
        if(!B_CheckCondition(&cond, 0, nullptr))
            return nullptr;
    }

    // Substitute parameters in the command.
    AutoStr *command = Str_Reserve(AutoStr_NewStd(), bind.gets("command").length());
    substituteInCommand(bind.gets("command"), event, command);

    return new CommandAction(Str_Text(command), CMDS_BIND);
}

bool BindContext::tryEvent(ddevent_t const &event, bool respectHigherContexts) const
{
    LOG_AS("BindContext");

    // Inactive contexts never respond.
    if(!isActive()) return false;

    // Is this event bindable to an action?
    if(event.type != E_FOCUS)
    {
        // See if the command bindings will have it.
        for(Record const *rec : d->commandBinds)
        {
            CommandBinding bind(*rec);
            AutoRef<Action> act(commandActionFor(*this, bind, event, respectHigherContexts));
            if(act.get())
            {
                act->trigger();
                return true; // Triggered!
            }
        }
    }

    // Try the fallback responders.
    if(d->ddFallbackResponder)
    {
        if(d->ddFallbackResponder(&event))
            return true;
    }
    if(d->fallbackResponder)
    {
        event_t ev;
        if(/*bool const validGameEvent =*/ InputSystem::convertEvent(event, ev))
        {
            if(d->fallbackResponder(&ev))
                return true;
        }
    }

    return false;
}

static bool conditionsAreEqual(QVector<BindingCondition> const &conds1,
                               QVector<BindingCondition> const &conds2)
{
    // Quick test (assumes there are no duplicated conditions).
    if(conds1.count() != conds2.count()) return false;

    for(BindingCondition const &a : conds1)
    {
        bool found = false;
        for(BindingCondition const &b : conds2)
        {
            if(B_EqualConditions(a, b))
            {
                found = true;
                break;
            }
        }
        if(!found) return false;
    }

    return true;
}

bool BindContext::findMatchingBinding(Record const *matchCmdRec, Record const *matchImpRec,
    Record **cmdResult, Record **impResult) const
{
    DENG2_ASSERT(cmdResult && impResult);

    *cmdResult = nullptr;
    *impResult = nullptr;

    if(!matchCmdRec && !matchImpRec) return false;

    CommandBinding matchCmd;
    if(matchCmdRec) matchCmd = matchCmdRec;

    ImpulseBinding matchImp;
    if(matchImpRec) matchImp = matchImpRec;

    for(Record const *rec : d->commandBinds)
    {
        CommandBinding bind(*rec);

        if(matchCmd && matchCmd.geti("id") != rec->geti("id"))
        {
            if(matchCmd.geti("type")      == bind.geti("type") &&
               matchCmd.geti("test")      == bind.geti("test") &&
               matchCmd.geti("deviceId")  == bind.geti("deviceId") &&
               matchCmd.geti("controlId") == bind.geti("controlId") &&
               conditionsAreEqual(matchCmd.conditions, bind.conditions))
            {
                *cmdResult = const_cast<Record *>(rec);
                return true;
            }
        }
        if(matchImp)
        {
            if(matchImp.geti("type")      == bind.geti("type") &&
               matchImp.geti("deviceId")  == bind.geti("deviceId") &&
               matchImp.geti("controlId") == bind.geti("controlId") &&
               conditionsAreEqual(matchImp.conditions, bind.conditions))
            {
                *cmdResult = const_cast<Record *>(rec);
                return true;
            }
        }
    }

    for(int i = 0; i < DDMAXPLAYERS; ++i)
    for(Record const *rec : d->impulseBinds[i])
    {
        ImpulseBinding bind(*rec);

        if(matchCmd)
        {
            if(matchCmd.geti("type")      == bind.geti("type") &&
               matchCmd.geti("deviceId")  == bind.geti("deviceId") &&
               matchCmd.geti("controlId") == bind.geti("controlId") &&
               conditionsAreEqual(matchCmd.conditions, bind.conditions))
            {
                *impResult = const_cast<Record *>(rec);
                return true;
            }
        }

        if(matchImp && matchImp.geti("id") != bind.geti("id"))
        {
            if(matchImp.geti("type")      == bind.geti("type") &&
               matchImp.geti("deviceId")  == bind.geti("deviceId") &&
               matchImp.geti("controlId") == bind.geti("controlId") &&
               conditionsAreEqual(matchImp.conditions, bind.conditions))
            {
                *impResult = const_cast<Record *>(rec);
                return true;
            }
        }
    }

    // Nothing found.
    return false;
}

LoopResult BindContext::forAllCommandBindings(
    std::function<de::LoopResult (Record &)> func) const
{
    for(Record *rec : d->commandBinds)
    {
        if(auto result = func(*rec)) return result;
    }
    return LoopContinue;
}

LoopResult BindContext::forAllImpulseBindings(int localPlayer,
    std::function<de::LoopResult (Record &)> func) const
{
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        if((localPlayer < 0 || localPlayer >= DDMAXPLAYERS) || localPlayer == i)
        {
            for(Record *rec : d->impulseBinds[i])
            {
                if(auto result = func(*rec)) return result;
            }
        }
    }
    return LoopContinue;
}

int BindContext::commandBindingCount() const
{
    return d->commandBinds.count();
}

int BindContext::impulseBindingCount(int localPlayer) const
{
    int count = 0;
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        if((localPlayer < 0 || localPlayer >= DDMAXPLAYERS) || localPlayer == i)
        {
            count += d->impulseBinds[i].count();
        }
    }
    return count;
}
