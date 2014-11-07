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
#include <de/Log>
#include "clientapp.h"

#include "world/p_players.h" // P_ConsoleToLocal

#include "ui/inputdevice.h"
#include "ui/playerimpulse.h"

/// @todo: remove
#include "ui/inputdeviceaxiscontrol.h"
#include "ui/inputdevicebuttoncontrol.h"
#include "ui/inputdevicehatcontrol.h"
// todo ends

using namespace de;

/// @todo: Is this even necessary? -ds
struct ControlGroup
{
    int bindId    = 0;             ///< Unique identifier.
    int impulseId = 0;
    typedef QList<ImpulseBinding *> Bindings;
    Bindings binds[DDMAXPLAYERS];  ///< Separate bindings for each local player.

    ~ControlGroup() {
        for(int i = 0; i < DDMAXPLAYERS; ++i) {
            qDeleteAll(binds[i]);
        }
    }
};

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

    typedef QList<CommandBinding *> CommandBindings;
    CommandBindings commandBinds;

    typedef QList<ControlGroup *> ControlGroups;
    ControlGroups controlGroups;

    DDFallbackResponderFunc ddFallbackResponder = nullptr;
    FallbackResponderFunc fallbackResponder     = nullptr;

    Instance(Public *i) : Base(i) {}

    ControlGroup *findControlGroup(int impulseId, bool canCreate = false)
    {
        for(ControlGroup *group : controlGroups)
        {
            if(group->impulseId == impulseId)
                return group;
        }

        if(!canCreate) return nullptr;

        // Create a new one.
        ControlGroup *group = new ControlGroup;
        group->bindId    = B_NewIdentifier();
        group->impulseId = impulseId;
        controlGroups.append(group);

        // Notify interested parties.
        DENG2_FOR_PUBLIC_AUDIENCE2(BindingAddition, i) i->bindContextBindingAdded(*thisPublic, group, false/*not-command*/);

        return group;
    }

    void removeControlGroup(ControlGroup *group)
    {
        DENG2_ASSERT(controlGroups.contains(group));
        controlGroups.removeOne(group);
    }

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
    qDeleteAll(d->controlGroups);
    d->controlGroups.clear();
    LOG_INPUT_VERBOSE(_E(b) "'%s'" _E(.) " cleared") << d->name;
}

void BindContext::deleteMatching(CommandBinding const *cmdBinding, ImpulseBinding const *impBinding)
{
    ImpulseBinding *foundImp = nullptr;
    CommandBinding *foundCmd = nullptr;

    while(findMatchingBinding(cmdBinding, impBinding, &foundCmd, &foundImp))
    {
        // Only either foundCmd or foundImp is returned as non-NULL.
        int bindId = (foundCmd? foundCmd->id : (foundImp? foundImp->id : 0));
        if(bindId)
        {
            LOG_INPUT_VERBOSE("Deleting binding %i, it has been overridden by binding %i")
                    << bindId << (cmdBinding? cmdBinding->id : impBinding->id);
            deleteBinding(bindId);
        }
    }
}

CommandBinding *BindContext::bindCommand(char const *eventDesc, char const *command)
{
    DENG2_ASSERT(eventDesc && command && command[0]);
    LOG_AS("BindContext");
    try
    {
        std::unique_ptr<CommandBinding> newBind(new CommandBinding);
        inputSys().configure(*newBind, eventDesc, command, true/*assign an ID*/);

        CommandBinding *bind = newBind.get();
        d->commandBinds.prepend(newBind.release());

        /// @todo: In interactive binding mode, should ask the user if the
        /// replacement is ok. For now, just delete the other binding.
        deleteMatching(bind, nullptr);

        LOG_INPUT_VERBOSE("Command \"%s\" now bound to \"%s\" in " _E(b) "'%s'"
                          " with binding Id " _E(b) "%i")
                << command << eventDesc << d->name << bind->id;

        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(BindingAddition, i) i->bindContextBindingAdded(*this, bind, true/*is-command*/);

        return bind;
    }
    catch(InputSystem::BindError const &)
    {}
    return nullptr;
}

ImpulseBinding *BindContext::bindImpulse(char const *ctrlDesc,
    PlayerImpulse const &impulse, int localPlayer)
{
    DENG2_ASSERT(ctrlDesc);
    DENG2_ASSERT(localPlayer >= 0 && localPlayer < DDMAXPLAYERS);
    LOG_AS("BindContext");
    try
    {
        std::unique_ptr<ImpulseBinding> newBind(new ImpulseBinding);
        inputSys().configure(*newBind, ctrlDesc, impulse.id, localPlayer); // Don't assign a new ID.

        ImpulseBinding *bind = newBind.get();
        ControlGroup &group  = *d->findControlGroup(impulse.id, true/*create if missing*/);
        group.binds[localPlayer].append(newBind.release());

        /// @todo: fix local player binding id management.
        bind->id = group.bindId;

        /// @todo: In interactive binding mode, should ask the user if the
        /// replacement is ok. For now, just delete the other binding.
        deleteMatching(nullptr, bind);

        LOG_INPUT_VERBOSE("Impulse " _E(b) "'%s'" _E(.) " of player%i now bound to \"%s\" in " _E(b) "'%s'"
                          " with binding Id " _E(b) "%i")
                << impulse.name << (localPlayer + 1) << ctrlDesc << d->name << bind->id;

        /// @todo: Notify interested parties.
        //DENG2_FOR_AUDIENCE2(BindingAddition, i) i->bindContextBindingAdded(*this, bind, false/*is-impulse*/);

        return bind;
    }
    catch(InputSystem::BindError const &)
    {}
    return nullptr;
}

CommandBinding *BindContext::findCommandBinding(char const *command, int deviceId) const
{
    if(command && command[0])
    {
        for(CommandBinding *bind : d->commandBinds)
        {
            if(bind->command.compareWithoutCase(command)) continue;

            if((deviceId < 0 || deviceId >= NUM_INPUT_DEVICES) || bind->deviceId == deviceId)
            {
                return bind;
            }
        }
    }
    return nullptr;
}

ImpulseBinding *BindContext::findImpulseBinding(int deviceId, ibcontroltype_t bindType, int controlId) const
{
    for(ControlGroup *group : d->controlGroups)
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    for(ImpulseBinding *bind : group->binds[i])
    {
        if(bind->deviceId == deviceId && bind->type == bindType && bind->controlId == controlId)
        {
            return bind;
        }
    }
    return nullptr;
}

bool BindContext::deleteBinding(int id)
{
    // Check if it is one of the command bindings.
    for(int i = 0; i < d->commandBinds.count(); ++i)
    {
        CommandBinding *bind = d->commandBinds.at(i);
        if(bind->id == id)
        {
            d->commandBinds.removeAt(i);
            delete bind;
            return true;
        }
    }

    // How about one of the impulse bindings?
    for(ControlGroup *group : d->controlGroups)
    {
        if(group->bindId == id)
        {
            d->removeControlGroup(group);
            delete group;
            return true;
        }

        for(int i = 0; i < DDMAXPLAYERS; ++i)
        for(int k = 0; k < group->binds[i].count(); ++k)
        {
            ImpulseBinding *bind = group->binds[i].at(k);
            if(bind->id == id)
            {
                group->binds[i].removeAt(k);
                delete bind;
                return true;
            }
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
    if(bind.deviceId != event.device) return nullptr;
    if(bind.type     != event.type)   return nullptr;

    InputDevice const *dev = nullptr;
    if(event.type != E_SYMBOLIC)
    {
        dev = inputSys().devicePtr(bind.deviceId);
        if(!dev || !dev->isActive())
        {
            // The device is not active, there is no way this could get executed.
            return nullptr;
        }
    }

    switch(event.type)
    {
    case E_TOGGLE: {
        if(bind.controlId != event.toggle.id)
            return nullptr;

        DENG2_ASSERT(dev);
        InputDeviceButtonControl &button = dev->button(bind.controlId);

        if(respectHigherContexts)
        {
            if(button.bindContext() != &context)
                return nullptr; // Shadowed by a more important active class.
        }

        // We're checking it, so clear the triggered flag.
        button.setBindContextAssociation(InputDeviceControl::Triggered, UnsetFlags);

        // Is the state as required?
        switch(bind.state)
        {
        case EBTOG_UNDEFINED:
            // Passes no matter what.
            break;

        case EBTOG_DOWN:
            if(event.toggle.state != ETOG_DOWN)
                return nullptr;
            break;

        case EBTOG_UP:
            if(event.toggle.state != ETOG_UP)
                return nullptr;
            break;

        case EBTOG_REPEAT:
            if(event.toggle.state != ETOG_REPEAT)
                return nullptr;
            break;

        case EBTOG_PRESS:
            if(event.toggle.state == ETOG_UP)
                return nullptr;
            break;

        default: return nullptr;
        }
        break; }

    case E_AXIS:
        if(bind.controlId != event.axis.id)
            return nullptr;

        DENG2_ASSERT(dev);
        if(dev->axis(bind.controlId).bindContext() != &context)
            return nullptr; // Shadowed by a more important active class.

        // Is the position as required?
        if(!B_CheckAxisPos(bind.state, bind.pos,
                           inputSys().device(event.device).axis(event.axis.id)
                               .translateRealPosition(event.axis.pos)))
            return nullptr;
        break;

    case E_ANGLE:
        if(bind.controlId != event.angle.id)
            return nullptr;

        DENG2_ASSERT(dev);
        if(dev->hat(bind.controlId).bindContext() != &context)
            return nullptr; // Shadowed by a more important active class.

        // Is the position as required?
        if(event.angle.pos != bind.pos)
            return nullptr;
        break;

    case E_SYMBOLIC:
        if(bind.symbolicName.compareWithCase(event.symbolic.name))
            return nullptr;
        break;

    default: return nullptr;
    }

    // Any conditions on the current state of the input devices?
    for(statecondition_t const &cond : bind.conditions)
    {
        if(!B_CheckCondition(&cond, 0, nullptr))
            return nullptr;
    }

    // Substitute parameters in the command.
    AutoStr *command = Str_Reserve(AutoStr_NewStd(), bind.command.length());
    substituteInCommand(bind.command, event, command);

    return new CommandAction(Str_Text(command), CMDS_BIND);
}

bool BindContext::tryEvent(ddevent_t const &event, bool respectHigherContexts) const
{
    LOG_AS("BindContext");

    // Inactive contexts never respond.
    if(!isActive()) return false;

    // Is this event bindable to an action?
    if(event.type != EV_FOCUS)
    {
        // See if the command bindings will have it.
        for(CommandBinding const *bind : d->commandBinds)
        {
            AutoRef<Action> act(commandActionFor(*this, *bind, event, respectHigherContexts));
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

static bool conditionsAreEqual(QVector<statecondition_t> const &conds1,
                               QVector<statecondition_t> const &conds2)
{
    // Quick test (assumes there are no duplicated conditions).
    if(conds1.count() != conds2.count()) return false;

    for(statecondition_t const &a : conds1)
    {
        bool found = false;
        for(statecondition_t const &b : conds2)
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

bool BindContext::findMatchingBinding(CommandBinding const *match1, ImpulseBinding const *match2,
    CommandBinding **cmdResult, ImpulseBinding **impResult) const
{
    DENG2_ASSERT(cmdResult && impResult);

    *cmdResult = nullptr;
    *impResult = nullptr;

    for(CommandBinding *bind : d->commandBinds)
    {
        if(match1 && match1->id != bind->id)
        {
            if(conditionsAreEqual(match1->conditions, bind->conditions) &&
               match1->deviceId  == bind->deviceId &&
               match1->controlId == bind->controlId &&
               match1->type      == bind->type &&
               match1->state     == bind->state)
            {
                *cmdResult = bind;
                return true;
            }
        }
        if(match2)
        {
            if(conditionsAreEqual(match2->conditions, bind->conditions) &&
               match2->deviceId  == bind->deviceId &&
               match2->controlId == bind->controlId &&
               match2->type      == (ibcontroltype_t) bind->type)
            {
                *cmdResult = bind;
                return true;
            }
        }
    }

    for(ControlGroup *group : d->controlGroups)
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    for(ImpulseBinding *bind : group->binds[i])
    {
        if(match1)
        {
            if(conditionsAreEqual(match1->conditions, bind->conditions) &&
               match1->deviceId  == bind->deviceId &&
               match1->controlId == bind->controlId &&
               match1->type      == (ddeventtype_t) bind->type)
            {
                *impResult = bind;
                return true;
            }
        }

        if(match2 && match2->id != bind->id)
        {
            if(conditionsAreEqual(match2->conditions, bind->conditions) &&
               match2->deviceId  == bind->deviceId &&
               match2->controlId == bind->controlId &&
               match2->type      == bind->type)
            {
                *impResult = bind;
                return true;
            }
        }
    }

    // Nothing found.
    return false;
}

LoopResult BindContext::forAllCommandBindings(
    std::function<de::LoopResult (CommandBinding &)> func) const
{
    for(CommandBinding *bind : d->commandBinds)
    {
        if(auto result = func(*bind)) return result;
    }
    return LoopContinue;
}

LoopResult BindContext::forAllImpulseBindings(int localPlayer,
    std::function<de::LoopResult (ImpulseBinding &)> func) const
{
    for(ControlGroup *group : d->controlGroups)
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        if((localPlayer < 0 || localPlayer >= DDMAXPLAYERS) || localPlayer == i)
        {
            for(ImpulseBinding *bind : group->binds[i])
            {
                if(auto result = func(*bind)) return result;
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
    for(ControlGroup const *group : d->controlGroups)
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        if((localPlayer < 0 || localPlayer >= DDMAXPLAYERS) || localPlayer == i)
        {
            count += group->binds[i].count();
        }
    }
    return count;
}
