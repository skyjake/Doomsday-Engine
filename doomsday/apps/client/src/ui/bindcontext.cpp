/** @file bindcontext.cpp  Input system, binding context.
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

#include "ui/bindcontext.h"

#include "clientapp.h"
#include "world/p_players.h"
#include "ui/commandbinding.h"
#include "ui/impulsebinding.h"
#include "ui/inputdevice.h"
#include "ui/inputsystem.h"

#include <doomsday/console/exec.h>
#include <de/set.h>
#include <de/log.h>

using namespace de;

static const char *VAR_ID         = "id";
static const char *VAR_TYPE       = "type";
static const char *VAR_TEST       = "test";
static const char *VAR_DEVICE_ID  = "deviceId";
static const char *VAR_CONTROL_ID = "controlId";

DE_PIMPL(BindContext)
{
    bool active  = false;  ///< @c true= Bindings are active.
    bool protect = false;  ///< @c true= Prevent explicit end user (de)activation.
    String name;           ///< Symbolic.

    // Acquired device states, unless higher-priority contexts override.
    typedef Set<int> DeviceIds;
    DeviceIds acquireDevices;
    bool acquireAllDevices = false;  ///< @c true= will ignore @var acquireDevices.

    typedef List<Record *> CommandBindings;
    CommandBindings commandBinds;

    typedef List<CompiledImpulseBindingRecord *> ImpulseBindings;
    ImpulseBindings impulseBinds[DDMAXPLAYERS];  ///< Group bindings for each local player.

    DDFallbackResponderFunc ddFallbackResponder = nullptr;
    FallbackResponderFunc fallbackResponder     = nullptr;

    Impl(Public *i) : Base(i) {}

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
    bool findMatchingBinding(const Record *matchCmdRec, const Record *matchImpRec,
        Record **cmdResult, Record **impResult) const
    {
        DE_ASSERT(cmdResult && impResult);

        *cmdResult = nullptr;
        *impResult = nullptr;

        if (!matchCmdRec && !matchImpRec) return false;

        CommandBinding matchCmd;
        if (matchCmdRec) matchCmd = matchCmdRec;

        ImpulseBinding matchImp;
        if (matchImpRec) matchImp = matchImpRec;

        for (const Record *rec : commandBinds)
        {
            CommandBinding bind(*rec);

            if (matchCmd && matchCmd.geti(VAR_ID) != rec->geti(VAR_ID))
            {
                if (matchCmd.geti(VAR_TYPE)       == bind.geti(VAR_TYPE) &&
                    matchCmd.geti(VAR_TEST)       == bind.geti(VAR_TEST) &&
                    matchCmd.geti(VAR_DEVICE_ID)  == bind.geti(VAR_DEVICE_ID) &&
                    matchCmd.geti(VAR_CONTROL_ID) == bind.geti(VAR_CONTROL_ID) &&
                    matchCmd.equalConditions(bind))
                {
                    if (bind.geti(VAR_TYPE) != E_ANGLE ||
                        (matchCmd.geti("pos") == bind.geti("pos"))) // Hat angles must also match.
                    {
                        *cmdResult = const_cast<Record *>(rec);
                        return true;
                    }
                }
            }
            if (matchImp)
            {
                if (matchImp.geti(VAR_TYPE)       == bind.geti(VAR_TYPE) &&
                    matchImp.geti(VAR_DEVICE_ID)  == bind.geti(VAR_DEVICE_ID) &&
                    matchImp.geti(VAR_CONTROL_ID) == bind.geti(VAR_CONTROL_ID) &&
                    matchImp.equalConditions(bind))
                {
                    *cmdResult = const_cast<Record *>(rec);
                    return true;
                }
            }
        }

        for (int i = 0; i < DDMAXPLAYERS; ++i)
        for (const auto *rec : impulseBinds[i])
        {
            const auto &bind = rec->compiled();

            if (matchCmd)
            {
                if (matchCmd.geti(VAR_TYPE)       == int(bind.type) &&
                    matchCmd.geti(VAR_DEVICE_ID)  == bind.deviceId &&
                    matchCmd.geti(VAR_CONTROL_ID) == bind.controlId &&
                    matchCmd.equalConditions(ImpulseBinding(*rec)))
                {
                    *impResult = const_cast<CompiledImpulseBindingRecord *>(rec);
                    return true;
                }
            }

            if (matchImp && matchImp.geti(VAR_ID) != bind.id)
            {
                if (matchImp.geti(VAR_TYPE)       == int(bind.type) &&
                    matchImp.geti(VAR_DEVICE_ID)  == bind.deviceId &&
                    matchImp.geti(VAR_CONTROL_ID) == bind.controlId &&
                    matchImp.equalConditions(ImpulseBinding(*rec)))
                {
                    *impResult = const_cast<CompiledImpulseBindingRecord *>(rec);
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
    void deleteMatching(const Record *cmdBinding, const Record *impBinding)
    {
        Record *foundCmd = nullptr;
        Record *foundImp = nullptr;

        while (findMatchingBinding(cmdBinding, impBinding, &foundCmd, &foundImp))
        {
            // Only either foundCmd or foundImp is returned as non-NULL.
            int bindId = (foundCmd? foundCmd->geti(VAR_ID) : (foundImp? foundImp->geti(VAR_ID) : 0));
            if (bindId)
            {
                LOG_INPUT_VERBOSE("Deleting binding %i, it has been overridden by binding %i")
                        << bindId << (cmdBinding? cmdBinding->geti(VAR_ID) : impBinding->geti(VAR_ID));
                self().deleteBinding(bindId);
            }
        }
    }

    DE_PIMPL_AUDIENCE(ActiveChange)
    DE_PIMPL_AUDIENCE(AcquireDeviceChange)
    DE_PIMPL_AUDIENCE(BindingAddition)
};

DE_AUDIENCE_METHOD(BindContext, ActiveChange)
DE_AUDIENCE_METHOD(BindContext, AcquireDeviceChange)
DE_AUDIENCE_METHOD(BindContext, BindingAddition)

BindContext::BindContext(const String &name) : d(new Impl(this))
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

void BindContext::setName(const String &newName)
{
    d->name = newName;
}

void BindContext::activate(bool yes)
{
    if (d->active == yes) return;

    LOG_AS("ui/bindcontext.h");
    LOG_INPUT_VERBOSE("%s " _E(b) "'%s'") << (yes? "Activating" : "Deactivating") << d->name;
    d->active = yes;

    // Notify interested parties.
    DE_NOTIFY(ActiveChange, i) i->bindContextActiveChanged(*this);
}

void BindContext::acquire(int deviceId, bool yes)
{
    DE_ASSERT(deviceId >= 0 && deviceId < NUM_INPUT_DEVICES);
    const int countBefore = d->acquireDevices.size();

    if (yes) d->acquireDevices.insert(deviceId);
    else     d->acquireDevices.remove(deviceId);

    if (countBefore != d->acquireDevices.size())
    {
        // Notify interested parties.
        DE_NOTIFY(AcquireDeviceChange, i) i->bindContextAcquireDeviceChanged(*this);
    }
}

void BindContext::acquireAll(bool yes)
{
    if (d->acquireAllDevices != yes)
    {
        d->acquireAllDevices = yes;

        // Notify interested parties.
        DE_NOTIFY(AcquireDeviceChange, i) i->bindContextAcquireDeviceChanged(*this);
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
    LOG_AS("ui/bindcontext.h");
    deleteAll(d->commandBinds);
    d->commandBinds.clear();
    for (int i = 0; i < DDMAXPLAYERS; ++i)
    {
        deleteAll(d->impulseBinds[i]);
        d->impulseBinds[i].clear();
    }
    LOG_INPUT_VERBOSE(_E(b) "'%s'" _E(.) " cleared") << d->name;
}

void BindContext::clearBindingsForDevice(int deviceId)
{
    // Collect all the bindings that should be deleted.
    Set<int> ids;
    forAllCommandBindings([&ids, &deviceId] (const Record &bind)
    {
        if (bind.geti(VAR_DEVICE_ID) == deviceId)
        {
            ids.insert(bind.geti(VAR_ID));
        }
        return LoopContinue;
    });
    forAllImpulseBindings([&ids, &deviceId] (const CompiledImpulseBindingRecord &bind)
    {
        if (bind.compiled().deviceId == deviceId)
        {
            ids.insert(bind.geti(VAR_ID));
        }
        return LoopContinue;
    });

    for (int id : ids)
    {
        deleteBinding(id);
    }
}

Record *BindContext::bindCommand(const char *eventDesc, const char *command)
{
    DE_ASSERT(eventDesc && command && command[0]);
    LOG_AS("ui/bindcontext.h");
    try
    {
        std::unique_ptr<Record> newBind(new Record);
        CommandBinding bind(*newBind.get());

        bind.configure(eventDesc, command); // Assign a new unique identifier.
        d->commandBinds.prepend(newBind.release());

        LOG_INPUT_VERBOSE("Command " _E(b) "\"%s\"" _E(.) " now bound to "
                          _E(b) "\"%s\"" _E(.) " in " _E(b) "'%s'" _E(.) " (id %i)")
                << command << bind.composeDescriptor() << d->name << bind.geti(VAR_ID);

        /// @todo: In interactive binding mode, should ask the user if the
        /// replacement is ok. For now, just delete the other bindings.
        d->deleteMatching(&bind.def(), nullptr);

        // Notify interested parties.
        DE_NOTIFY(BindingAddition, i) i->bindContextBindingAdded(*this, bind.def(), true/*is-command*/);

        Con_MarkAsChanged(true);

        return &bind.def();
    }
    catch (const Binding::ConfigureError &)
    {}
    return nullptr;
}

Record *BindContext::bindImpulse(const char *ctrlDesc, const PlayerImpulse &impulse, int localPlayer)
{
    DE_ASSERT(ctrlDesc);
    DE_ASSERT(localPlayer >= 0 && localPlayer < DDMAXPLAYERS);
    LOG_AS("ui/bindcontext.h");
    try
    {
        std::unique_ptr<CompiledImpulseBindingRecord> newBind(new CompiledImpulseBindingRecord);
        ImpulseBinding bind(*newBind.get());

        bind.configure(ctrlDesc, impulse.id, localPlayer); // Assign a new unique identifier.
        d->impulseBinds[localPlayer].append(newBind.release());

        LOG_INPUT_VERBOSE("Impulse " _E(b) "'%s'" _E(.) " of player%i now bound to \"%s\" in "
                          _E(b) "'%s'" _E(.) " (id %i)")
                << impulse.name << (localPlayer + 1) << bind.composeDescriptor()
                << d->name << bind.def().compiled().id;

        /// @todo: In interactive binding mode, should ask the user if the
        /// replacement is ok. For now, just delete the other bindings.
        d->deleteMatching(nullptr, &bind.def());

        // Notify interested parties.
        DE_NOTIFY(BindingAddition, i) i->bindContextBindingAdded(*this, bind.def(), false/*is-impulse*/);

        Con_MarkAsChanged(true);

        return &bind.def();
    }
    catch (const Binding::ConfigureError &)
    {}
    return nullptr;
}

Record *BindContext::findCommandBinding(const char *command, int deviceId) const
{
    if (command && command[0])
    {
        for (const Record *rec : d->commandBinds)
        {
            CommandBinding bind(*rec);
            if (bind.gets(DE_STR("command")).compareWithoutCase(command)) continue;

            if ((deviceId < 0 || deviceId >= NUM_INPUT_DEVICES) || bind.geti(VAR_DEVICE_ID) == deviceId)
            {
                return const_cast<Record *>(rec);
            }
        }
    }
    return nullptr;
}

Record *BindContext::findImpulseBinding(int deviceId, ibcontroltype_t bindType, int controlId) const
{
    for (int i = 0; i < DDMAXPLAYERS; ++i)
    for (const auto *rec : d->impulseBinds[i])
    {
        const auto &bind = rec->compiled();
        if (bind.type      == bindType &&
            bind.deviceId  == deviceId &&
            bind.controlId == controlId)
        {
            return const_cast<CompiledImpulseBindingRecord *>(rec);
        }
    }
    return nullptr;
}

bool BindContext::deleteBinding(int id)
{
    // Check if it is one of the command bindings.
    for (int i = 0; i < d->commandBinds.count(); ++i)
    {
        Record *rec = d->commandBinds.at(i);
        if (rec->geti(VAR_ID) == id)
        {
            d->commandBinds.removeAt(i);
            delete rec;
            return true;
        }
    }

    // How about one of the impulse bindings?
    for (int i = 0; i < DDMAXPLAYERS; ++i)
    for (int k = 0; k < d->impulseBinds[i].count(); ++k)
    {
        auto *rec = d->impulseBinds[i].at(k);
        if (rec->compiled().id == id)
        {
            d->impulseBinds[i].removeAt(k);
            delete rec;
            return true;
        }
    }

    return false;
}

bool BindContext::tryEvent(const ddevent_t &event, bool respectHigherContexts) const
{
    LOG_AS("ui/bindcontext.h");

    // Inactive contexts never respond.
    if (!isActive()) return false;

    // Is this event bindable to an action?
    if (event.type != E_FOCUS)
    {
        // See if the command bindings will have it.
        for (const Record *rec : d->commandBinds)
        {
            CommandBinding bind(*rec);
            AutoRef<Action> act(bind.makeAction(event, *this, respectHigherContexts));
            if (act.get())
            {
                act->trigger();
                return true; // Triggered!
            }
        }
    }

    // Try the fallback responders.
    if (d->ddFallbackResponder)
    {
        if (d->ddFallbackResponder(&event))
            return true;
    }
    if (d->fallbackResponder)
    {
        event_t ev;
        if (/*const bool validGameEvent =*/ InputSystem::convertEvent(event, ev))
        {
            if (d->fallbackResponder(&ev))
                return true;
        }
    }

    return false;
}

LoopResult BindContext::forAllCommandBindings(
    const std::function<de::LoopResult (Record &)>& func) const
{
    for (Record *rec : d->commandBinds)
    {
        if (auto result = func(*rec)) return result;
    }
    return LoopContinue;
}

LoopResult BindContext::forAllImpulseBindings(int localPlayer,
    const std::function<de::LoopResult (CompiledImpulseBindingRecord &)>& func) const
{
    for (int i = 0; i < DDMAXPLAYERS; ++i)
    {
        if ((localPlayer < 0 || localPlayer >= DDMAXPLAYERS) || localPlayer == i)
        {
            for (auto *rec : d->impulseBinds[i])
            {
                if (auto result = func(*rec)) return result;
            }
        }
    }
    return LoopContinue;
}

int BindContext::commandBindingCount() const
{
    return d.getConst()->commandBinds.count();
}

int BindContext::impulseBindingCount(int localPlayer) const
{
    int count = 0;
    for (int i = 0; i < DDMAXPLAYERS; ++i)
    {
        if ((localPlayer < 0 || localPlayer >= DDMAXPLAYERS) || localPlayer == i)
        {
            count += d.getConst()->impulseBinds[i].count();
        }
    }
    return count;
}
