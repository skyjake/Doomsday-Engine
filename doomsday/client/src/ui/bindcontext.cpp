/** @file b_context.cpp  Input system binding contexts.
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

#include "de_platform.h" // strdup macro
#include "ui/bindcontext.h"

#include <cstring>
#include <de/memory.h>
#include <QSet>
#include <de/Log>
#include "clientapp.h"
#include "m_misc.h" // M_WriteTextEsc

#include "ui/b_main.h"
#include "ui/commandbinding.h"
#include "ui/p_control.h"
#include "ui/inputdevice.h"

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

    CommandBinding commandBinds;
    controlbindgroup_t impulseBinds;

    DDFallbackResponderFunc ddFallbackResponder = nullptr;
    FallbackResponderFunc fallbackResponder     = nullptr;

    Instance(Public *i) : Base(i)
    {
        B_InitCommandBindingList(&commandBinds);
        B_InitControlBindGroupList(&impulseBinds);
    }

    /**
     * Construct a new event => command binding.
     *
     * @param eventDesc  Event descriptor.
     * @param commands   Command(s) to be executed when triggered.
     *
     * @return  New binding, or @c nullptr if there was an error.
     */
    CommandBinding *newCommandBinding(char const *eventDesc, char const *commands)
    {
        DENG2_ASSERT(commands && commands[0]);

        CommandBinding *b = B_AllocCommandBinding();
        DENG2_ASSERT(b);

        if(!B_ParseEventDescriptor(b, eventDesc))
        {
            // Failed parsing - we cannot continue.
            B_DestroyCommandBinding(b);
            return nullptr;
        }

        b->command = strdup(commands); // make a copy.

        // Link it in.
        b->next = &commandBinds;
        b->prev = commandBinds.prev;
        commandBinds.prev->next = b;
        commandBinds.prev = b;

        return b;
    }

    controlbindgroup_t *newControlBindGroup()
    {
        controlbindgroup_t *g = (controlbindgroup_t *) M_Calloc(sizeof(*g));
        g->bid = B_NewIdentifier();
        for(int i = 0; i < DDMAXPLAYERS; ++i)
        {
            B_InitImpulseBindingList(&g->binds[i]);
        }

        // Link it in.
        g->next = &impulseBinds;
        g->prev = impulseBinds.prev;
        impulseBinds.prev->next = g;
        impulseBinds.prev = g;

        return g;
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
    LOG_INPUT_VERBOSE("%s '%s'") << (yes? "Activating" : "Deactivating") << d->name;
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
    /// @todo: What about acquireAllDevices? (Ambiguous naming/usage).
    return d->acquireDevices.contains(deviceId);
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

int BindContext::tryFallbackResponders(ddevent_t const &event, event_t &ev, bool validGameEvent)
{
    if(d->ddFallbackResponder)
    {
        if(int result = d->ddFallbackResponder(&event))
            return result;
    }
    if(validGameEvent && d->fallbackResponder)
    {
        if(int result = d->fallbackResponder(&ev))
            return result;
    }
    return 0;
}

void BindContext::clearAllBindings()
{
    B_DestroyCommandBindingList(&d->commandBinds);
    B_DestroyControlBindGroupList(&d->impulseBinds);
}

void BindContext::deleteMatching(CommandBinding *eventBinding, ImpulseBinding *deviceBinding)
{
    ImpulseBinding *devb = nullptr;
    CommandBinding *evb = nullptr;

    while(findMatchingBinding(eventBinding, deviceBinding, &evb, &devb))
    {
        // Only either evb or devb is returned as non-NULL.
        int bid = (evb? evb->id : (devb? devb->id : 0));
        if(bid)
        {
            LOG_INPUT_VERBOSE("Deleting binding %i, it has been overridden by binding %i")
                    << bid << (eventBinding? eventBinding->id : deviceBinding->id);
            deleteBinding(bid);
        }
    }
}

CommandBinding *BindContext::bindCommand(char const *eventDesc, char const *command)
{
    DENG2_ASSERT(eventDesc && command);

    CommandBinding *b = d->newCommandBinding(eventDesc, command);
    if(b)
    {
        /// @todo: In interactive binding mode, should ask the user if the
        /// replacement is ok. For now, just delete the other binding.
        deleteMatching(b, nullptr);

        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(BindingAddition, i) i->bindContextBindingAdded(*this, b, true/*is-command*/);
    }

    return b;
}

CommandBinding *BindContext::findCommandBinding(char const *command, int deviceId) const
{
    if(command && command[0])
    {
        for(CommandBinding *i = d->commandBinds.next; i != &d->commandBinds; i = i->next)
        {
            if(qstricmp(i->command, command)) continue;

            if((deviceId < 0 || deviceId >= NUM_INPUT_DEVICES) || i->deviceId == deviceId)
            {
                return i;
            }
        }
    }
    return nullptr;
}

controlbindgroup_t *BindContext::findControlBindGroup(int control) const
{
    for(controlbindgroup_t *i = d->impulseBinds.next; i != &d->impulseBinds; i = i->next)
    {
        if(i->impulseId == control)
            return i;
    }
    return nullptr;
}

controlbindgroup_t *BindContext::getControlBindGroup(int control)
{
    controlbindgroup_t *b = findControlBindGroup(control);
    if(!b)
    {
        // Create a new one.
        b = d->newControlBindGroup();
        b->impulseId = control;

        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(BindingAddition, i) i->bindContextBindingAdded(*this, b, false/*not-command*/);
    }
    return b;
}

ImpulseBinding *BindContext::findImpulseBinding(int device, ibcontroltype_t bindType, int id)
{
    for(controlbindgroup_t *group = d->impulseBinds.next; group != &d->impulseBinds; group = group->next)
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    for(ImpulseBinding *bind = group->binds[i].next; bind != &group->binds[i]; bind = bind->next)
    {
        if(bind->deviceId == device && bind->type == bindType && bind->controlId == id)
        {
            return bind;
        }
    }
    return nullptr;
}

bool BindContext::deleteBinding(int bid)
{
    // Check if it is one of the command bindings.
    for(CommandBinding *bind = d->commandBinds.next; bind != &d->commandBinds; bind = bind->next)
    {
        if(bind->id == bid)
        {
            B_DestroyCommandBinding(bind);
            return true;
        }
    }

    // How about one of the impulse bindings?
    for(controlbindgroup_t *group = d->impulseBinds.next; group != &d->impulseBinds; group = group->next)
    {
        if(group->bid == bid)
        {
            B_DestroyControlBindGroup(group);
            return true;
        }

        for(int i = 0; i < DDMAXPLAYERS; ++i)
        {
            for(ImpulseBinding *bind = group->binds[i].next; bind != &group->binds[i]; bind = bind->next)
            {
                if(bind->id == bid)
                {
                    B_DestroyImpulseBinding(bind);
                    return true;
                }
            }
        }
    }

    return false;
}

Action *BindContext::actionForEvent(ddevent_t const &event, bool respectHigherAssociatedContexts) const
{
    // See if the command bindings will have it.
    for(CommandBinding *eb = d->commandBinds.next; eb != &d->commandBinds; eb = eb->next)
    {
        if(Action *act = CommandBinding_ActionForEvent(eb, &event, this, respectHigherAssociatedContexts))
        {
            return act;
        }
    }
    return nullptr;
}

static bool conditionsAreEqual(int count1, statecondition_t const *conds1,
                               int count2, statecondition_t const *conds2)
{
    //DENG2_ASSERT(conds1 && conds2);

    // Quick test (assumes there are no duplicated conditions).
    if(count1 != count2) return false;

    for(int i = 0; i < count1; ++i)
    {
        bool found = false;
        for(int k = 0; k < count2; ++k)
        {
            if(B_EqualConditions(conds1 + i, conds2 + k))
            {
                found = true;
                break;
            }
        }
        if(!found) return false;
    }

    return true;
}

bool BindContext::findMatchingBinding(CommandBinding *match1, ImpulseBinding *match2,
    CommandBinding **evResult, ImpulseBinding **dResult)
{
    DENG2_ASSERT(evResult && dResult);

    *evResult = nullptr;
    *dResult  = nullptr;

    for(CommandBinding *bind = d->commandBinds.next; bind != &d->commandBinds; bind = bind->next)
    {
        if(match1 && match1->id != bind->id)
        {
            if(conditionsAreEqual(match1->numConds, match1->conds, bind->numConds, bind->conds) &&
               match1->deviceId  == bind->deviceId &&
               match1->controlId == bind->controlId &&
               match1->type      == bind->type &&
               match1->state     == bind->state)
            {
                *evResult = bind;
                return true;
            }
        }
        if(match2)
        {
            if(conditionsAreEqual(match2->numConds, match2->conds, bind->numConds, bind->conds) &&
               match2->deviceId  == bind->deviceId &&
               match2->controlId == bind->controlId &&
               match2->type      == (ibcontroltype_t) bind->type)
            {
                *evResult = bind;
                return true;
            }
        }
    }

    for(controlbindgroup_t *group = d->impulseBinds.next; group != &d->impulseBinds; group = group->next)
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    for(ImpulseBinding *bind = group->binds[i].next; bind != &group->binds[i]; bind = bind->next)
    {
        if(match1)
        {
            if(conditionsAreEqual(match1->numConds, match1->conds, bind->numConds, bind->conds) &&
               match1->deviceId  == bind->deviceId &&
               match1->controlId == bind->controlId &&
               match1->type      == (ddeventtype_t) bind->type)
            {
                *dResult = bind;
                return true;
            }
        }

        if(match2 && match2->id != bind->id)
        {
            if(conditionsAreEqual(match2->numConds, match2->conds, bind->numConds, bind->conds) &&
               match2->deviceId  == bind->deviceId &&
               match2->controlId == bind->controlId &&
               match2->type      == bind->type)
            {
                *dResult = bind;
                return true;
            }
        }
    }

    // Nothing found.
    return false;
}

LoopResult BindContext::forAllCommandBindings(std::function<de::LoopResult (CommandBinding &)> func) const
{
    for(CommandBinding *bind = d->commandBinds.next; bind != &d->commandBinds; bind = bind->next)
    {
        if(auto result = func(*bind)) return result;
    }
    return LoopContinue;
}

LoopResult BindContext::forAllControlBindGroups(std::function<de::LoopResult (controlbindgroup_t &)> func) const
{
    for(controlbindgroup_t *i = d->impulseBinds.next; i != &d->impulseBinds; i = i->next)
    {
        if(auto result = func(*i)) return result;
    }
    return LoopContinue;
}

void BindContext::printAllBindings() const
{
#define BIDFORMAT "[%3i]"

    AutoStr *str = AutoStr_NewStd();

    LOG_INPUT_MSG(_E(b)"Context \"%s\" (%s):")
            << d->name << (isActive()? "active" : "inactive");

    // Commands.
    int count = 0;
    for(CommandBinding *e = d->commandBinds.next; e != &d->commandBinds; e = e->next, count++) {}

    if(count)
    {
        LOG_INPUT_MSG("  %i event bindings:") << count;
    }

    for(CommandBinding *e = d->commandBinds.next; e != &d->commandBinds; e = e->next)
    {
        CommandBinding_ToString(e, str);
        LOG_INPUT_MSG("  " BIDFORMAT " %s : " _E(>) "%s")
                << e->id << Str_Text(str) << e->command;
    }

    // Impulses.
    count = 0;
    for(controlbindgroup_t *g = d->impulseBinds.next; g != &d->impulseBinds; g = g->next, count++) {}

    if(count)
    {
        LOG_INPUT_MSG("  %i control bindings") << count;
    }

    for(controlbindgroup_t *group = d->impulseBinds.next; group != &d->impulseBinds; group = group->next)
    {
        char const *controlName = P_PlayerControlById(group->impulseId)->name;

        LOG_INPUT_MSG(_E(D) "  Control \"%s\" " BIDFORMAT ":") << controlName << group->bid;

        for(int k = 0; k < DDMAXPLAYERS; ++k)
        {
            count = 0;
            for(ImpulseBinding *bind = group->binds[k].next; bind != &group->binds[k];
                bind = bind->next, count++) {}

            if(!count)
            {
                continue;
            }

            LOG_INPUT_MSG("    Local player %i has %i device bindings for \"%s\":")
                    << k + 1 << count << controlName;

            for(ImpulseBinding *bind = group->binds[k].next; bind != &group->binds[k]; bind = bind->next)
            {
                ImpulseBinding_ToString(bind, str);
                LOG_INPUT_MSG("    " BIDFORMAT " %s") << bind->id << Str_Text(str);
            }
        }
    }

#undef BIDFORMAT
}

void BindContext::writeAllBindingsTo(FILE *file) const
{
    DENG2_ASSERT(file);
    AutoStr *str = AutoStr_NewStd();

    // Commands.
    for(CommandBinding *e = d->commandBinds.next; e != &d->commandBinds; e = e->next)
    {
        CommandBinding_ToString(e, str);
        fprintf(file, "bindevent \"%s:%s\" \"", d->name.toUtf8().constData(), Str_Text(str));
        M_WriteTextEsc(file, e->command);
        fprintf(file, "\"\n");
    }

    // Impulses.
    for(controlbindgroup_t *group = d->impulseBinds.next; group != &d->impulseBinds; group = group->next)
    {
        char const *controlName = P_PlayerControlById(group->impulseId)->name;

        for(int i = 0; i < DDMAXPLAYERS; ++i)
        for(ImpulseBinding *bind = group->binds[i].next; bind != &group->binds[i]; bind = bind->next)
        {
            ImpulseBinding_ToString(bind, str);
            fprintf(file, "bindcontrol local%i-%s \"%s\"\n", i + 1, controlName, Str_Text(str));
        }
    }
}

// ------------------------------------------------------------------------------

void B_InitControlBindGroupList(controlbindgroup_t *listRoot)
{
    DENG2_ASSERT(listRoot);
    de::zapPtr(listRoot);
    listRoot->next = listRoot->prev = listRoot;
}

void B_DestroyControlBindGroupList(controlbindgroup_t *listRoot)
{
    DENG2_ASSERT(listRoot);
    while(listRoot->next != listRoot)
    {
        B_DestroyControlBindGroup(listRoot->next);
    }
}

void B_DestroyControlBindGroup(controlbindgroup_t *g)
{
    if(!g) return;

    DENG2_ASSERT(g->bid != 0);

    // Unlink first, if linked.
    if(g->prev)
    {
        g->prev->next = g->next;
        g->next->prev = g->prev;
    }

    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        B_DestroyImpulseBindingList(&g->binds[i]);
    }
    M_Free(g);
}
