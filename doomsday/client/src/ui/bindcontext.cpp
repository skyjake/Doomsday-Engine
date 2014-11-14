/** @file bindcontext.cpp  Input system, binding context.
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

#include "world/p_players.h"

#include "CommandBinding"
#include "ImpulseBinding"
#include "ui/inputdevice.h"

using namespace de;

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

    /**
     * Look through the context for a binding that matches either of @a matchCmd or
     * @a matchImp.
     *
     * @param matchCmd   Command binding record to match, if any.
     * @param matchImp   Impulse binding record to match, if any.
     * @param cmdResult  The address of any matching command binding is written here.
     * @param impResult  The address of any matching impulse binding is written here.
     *
     * @return  @c true if a match is found.
     */
    bool findMatchingBinding(Record const *matchCmdRec, Record const *matchImpRec,
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

        for(Record const *rec : commandBinds)
        {
            CommandBinding bind(*rec);

            if(matchCmd && matchCmd.geti("id") != rec->geti("id"))
            {
                if(matchCmd.geti("type")      == bind.geti("type") &&
                   matchCmd.geti("test")      == bind.geti("test") &&
                   matchCmd.geti("deviceId")  == bind.geti("deviceId") &&
                   matchCmd.geti("controlId") == bind.geti("controlId") &&
                   matchCmd.equalConditions(bind))
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
                   matchImp.equalConditions(bind))
                {
                    *cmdResult = const_cast<Record *>(rec);
                    return true;
                }
            }
        }

        for(int i = 0; i < DDMAXPLAYERS; ++i)
        for(Record const *rec : impulseBinds[i])
        {
            ImpulseBinding bind(*rec);

            if(matchCmd)
            {
                if(matchCmd.geti("type")      == bind.geti("type") &&
                   matchCmd.geti("deviceId")  == bind.geti("deviceId") &&
                   matchCmd.geti("controlId") == bind.geti("controlId") &&
                   matchCmd.equalConditions(bind))
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
                   matchImp.equalConditions(bind))
                {
                    *impResult = const_cast<Record *>(rec);
                    return true;
                }
            }
        }

        // Nothing found.
        return false;
    }

    /**
     * Delete all other bindings matching either @a cmdBinding or @a impBinding.
     */
    void deleteMatching(Record const *cmdBinding, Record const *impBinding)
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
                self.deleteBinding(bindId);
            }
        }
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
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        qDeleteAll(d->impulseBinds[i]);
        d->impulseBinds[i].clear();
    }
    LOG_INPUT_VERBOSE(_E(b) "'%s'" _E(.) " cleared") << d->name;
}

Record *BindContext::bindCommand(char const *eventDesc, char const *command)
{
    DENG2_ASSERT(eventDesc && command && command[0]);
    LOG_AS("BindContext");
    try
    {
        std::unique_ptr<Record> newBind(new Record);
        CommandBinding bind(*newBind.get());

        bind.configure(eventDesc, command); // Assign a new unique identifier.
        d->commandBinds.prepend(newBind.release());

        LOG_INPUT_VERBOSE("Command " _E(b) "\"%s\"" _E(.) " now bound to " _E(b) "\"%s\"" _E(.) " in " _E(b) "'%s'" _E(.)
                          " with binding Id " _E(b) "%i")
                << command << bind.composeDescriptor() << d->name << bind.geti("id");

        /// @todo: In interactive binding mode, should ask the user if the
        /// replacement is ok. For now, just delete the other bindings.
        d->deleteMatching(&bind.def(), nullptr);

        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(BindingAddition, i) i->bindContextBindingAdded(*this, bind.def(), true/*is-command*/);

        return &bind.def();
    }
    catch(Binding::ConfigureError const &)
    {}
    return nullptr;
}

Record *BindContext::bindImpulse(char const *ctrlDesc, PlayerImpulse const &impulse, int localPlayer)
{
    DENG2_ASSERT(ctrlDesc);
    DENG2_ASSERT(localPlayer >= 0 && localPlayer < DDMAXPLAYERS);
    LOG_AS("BindContext");
    try
    {
        std::unique_ptr<Record> newBind(new Record);
        ImpulseBinding bind(*newBind.get());

        bind.configure(ctrlDesc, impulse.id, localPlayer); // Assign a new unique identifier.
        d->impulseBinds[localPlayer].append(newBind.release());

        LOG_INPUT_VERBOSE("Impulse " _E(b) "'%s'" _E(.) " of player%i now bound to \"%s\" in " _E(b) "'%s'" _E(.)
                          " with binding Id " _E(b) "%i")
                << impulse.name << (localPlayer + 1) << bind.composeDescriptor() << d->name << bind.geti("id");

        /// @todo: In interactive binding mode, should ask the user if the
        /// replacement is ok. For now, just delete the other bindings.
        d->deleteMatching(nullptr, &bind.def());

        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(BindingAddition, i) i->bindContextBindingAdded(*this, bind.def(), false/*is-impulse*/);

        return &bind.def();
    }
    catch(Binding::ConfigureError const &)
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
            AutoRef<Action> act(bind.makeAction(event, *this, respectHigherContexts));
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
