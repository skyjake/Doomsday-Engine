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
#include "ui/b_command.h"
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

    cbinding_t commandBinds;
    controlbinding_t controlBinds;

    DDFallbackResponderFunc ddFallbackResponder = nullptr;
    FallbackResponderFunc fallbackResponder     = nullptr;

    Instance(Public *i) : Base(i)
    {
        B_InitCommandBindingList(&commandBinds);
        B_InitControlBindingList(&controlBinds);
    }

    controlbinding_t *newControlBinding()
    {
        controlbinding_t *conBin = (controlbinding_t *) M_Calloc(sizeof(*conBin));
        conBin->bid = B_NewIdentifier();
        for(int i = 0; i < DDMAXPLAYERS; ++i)
        {
            B_InitDeviceBindingList(&conBin->deviceBinds[i]);
        }

        // Link it in.
        conBin->next = &controlBinds;
        conBin->prev = controlBinds.prev;
        controlBinds.prev->next = conBin;
        controlBinds.prev = conBin;

        return conBin;
    }

    /**
     * Creates a new event-command binding.
     *
     * @param desc       Descriptor of the event.
     * @param command    Command that will be executed by the binding.
     *
     * @return  New binding, or @c nullptr if there was an error.
     */
    cbinding_t *newCommandBinding(char const *desc, char const *command)
    {
        DENG2_ASSERT(command && command[0]);

        cbinding_t *eb = B_AllocCommandBinding();
        DENG2_ASSERT(eb);

        // Parse the description of the event.
        if(!B_ParseEventDescriptor(eb, desc))
        {
            // Error in parsing, failure to create binding.
            B_DestroyCommandBinding(eb);
            return nullptr;
        }

        // The command string.
        eb->command = strdup(command);

        // Link it into the list.
        eb->next = &commandBinds;
        eb->prev = commandBinds.prev;
        commandBinds.prev->next = eb;
        commandBinds.prev = eb;

        return eb;
    }

    DENG2_PIMPL_AUDIENCE(ActiveChange)
};

DENG2_AUDIENCE_METHOD(BindContext, ActiveChange)

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
    bool const oldActive = d->active;

    LOG_INPUT_VERBOSE("%s binding context '%s'")
            << (yes? "Activating" : "Deactivating")
            << d->name;

    d->active = yes;

    inputSys().updateAllDeviceStateAssociations();

    if(d->acquireAllDevices)
    {
        inputSys().forAllDevices([] (InputDevice &device)
        {
            device.reset();
            return LoopContinue;
        });
    }

    if(oldActive != d->active)
    {
        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(ActiveChange, i) i->bindContextActiveChanged(*this);
    }
}

void BindContext::acquire(int deviceId, bool yes)
{
    DENG2_ASSERT(deviceId >= 0 && deviceId < NUM_INPUT_DEVICES);

    if(yes) d->acquireDevices.insert(deviceId);
    else    d->acquireDevices.remove(deviceId);

    inputSys().updateAllDeviceStateAssociations();
}

void BindContext::acquireAll(bool yes)
{
    d->acquireAllDevices = yes;

    inputSys().updateAllDeviceStateAssociations();
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
    B_DestroyControlBindingList(&d->controlBinds);
}

void BindContext::deleteMatching(cbinding_t *eventBinding, dbinding_t *deviceBinding)
{
    dbinding_t *devb = nullptr;
    cbinding_t *evb = nullptr;

    while(findMatchingBinding(eventBinding, deviceBinding, &evb, &devb))
    {
        // Only either evb or devb is returned as non-NULL.
        int bid = (evb? evb->bid : (devb? devb->bid : 0));
        if(bid)
        {
            LOG_INPUT_VERBOSE("Deleting binding %i, it has been overridden by binding %i")
                    << bid << (eventBinding? eventBinding->bid : deviceBinding->bid);
            deleteBinding(bid);
        }
    }
}

cbinding_t *BindContext::bindCommand(char const *eventDesc, char const *command)
{
    DENG2_ASSERT(eventDesc && command);

    cbinding_t *b = d->newCommandBinding(eventDesc, command);
    if(b)
    {
        /// @todo: In interactive binding mode, should ask the user if the
        /// replacement is ok. For now, just delete the other binding.
        deleteMatching(b, nullptr);
        inputSys().updateAllDeviceStateAssociations();
    }

    return b;
}

cbinding_t *BindContext::findCommandBinding(char const *command, int deviceId) const
{
    if(command && command[0])
    {
        for(cbinding_t *i = d->commandBinds.next; i != &d->commandBinds; i = i->next)
        {
            if(qstricmp(i->command, command)) continue;

            if((deviceId < 0 || deviceId >= NUM_INPUT_DEVICES) || i->device == deviceId)
            {
                return i;
            }
        }
    }
    return nullptr;
}

controlbinding_t *BindContext::findControlBinding(int control) const
{
    for(controlbinding_t *i = d->controlBinds.next; i != &d->controlBinds; i = i->next)
    {
        if(i->control == control)
            return i;
    }
    return nullptr;
}

controlbinding_t *BindContext::getControlBinding(int control)
{
    controlbinding_t *b = findControlBinding(control);
    if(!b)
    {
        // Create a new one.
        b = d->newControlBinding();
        b->control = control;
    }
    return b;
}

dbinding_t *BindContext::findDeviceBinding(int device, cbdevtype_t bindType, int id)
{
    for(controlbinding_t *cb = d->controlBinds.next; cb != &d->controlBinds; cb = cb->next)
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    for(dbinding_t *d = cb->deviceBinds[i].next; d != &cb->deviceBinds[i]; d = d->next)
    {
        if(d->device == device && d->type == bindType && d->id == id)
        {
            return d;
        }
    }
    return nullptr;
}

bool BindContext::deleteBinding(int bid)
{
    // Check if it is one of the command bindings.
    for(cbinding_t *eb = d->commandBinds.next; eb != &d->commandBinds; eb = eb->next)
    {
        if(eb->bid == bid)
        {
            B_DestroyCommandBinding(eb);
            return true;
        }
    }

    // How about one of the control bindings?
    for(controlbinding_t *conBin = d->controlBinds.next; conBin != &d->controlBinds; conBin = conBin->next)
    {
        if(conBin->bid == bid)
        {
            B_DestroyControlBinding(conBin);
            return true;
        }

        // It may also be a device binding.
        for(int i = 0; i < DDMAXPLAYERS; ++i)
        {
            for(dbinding_t *db = conBin->deviceBinds[i].next; db != &conBin->deviceBinds[i]; db = db->next)
            {
                if(db->bid == bid)
                {
                    B_DestroyDeviceBinding(db);
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
    for(cbinding_t *eb = d->commandBinds.next; eb != &d->commandBinds; eb = eb->next)
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

bool BindContext::findMatchingBinding(cbinding_t *match1, dbinding_t *match2,
    cbinding_t **evResult, dbinding_t **dResult)
{
    DENG2_ASSERT(evResult && dResult);

    *evResult = nullptr;
    *dResult  = nullptr;

    for(cbinding_t *e = d->commandBinds.next; e != &d->commandBinds; e = e->next)
    {
        if(match1 && match1->bid != e->bid)
        {
            if(conditionsAreEqual(match1->numConds, match1->conds, e->numConds, e->conds) &&
               match1->device == e->device && match1->id == e->id &&
               match1->type == e->type && match1->state == e->state)
            {
                *evResult = e;
                return true;
            }
        }
        if(match2)
        {
            if(conditionsAreEqual(match2->numConds, match2->conds, e->numConds, e->conds) &&
               match2->device == e->device && match2->id == e->id &&
               match2->type == (cbdevtype_t) e->type)
            {
                *evResult = e;
                return true;
            }
        }
    }

    for(controlbinding_t *c = d->controlBinds.next; c != &d->controlBinds; c = c->next)
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    for(dbinding_t *d = c->deviceBinds[i].next; d != &c->deviceBinds[i]; d = d->next)
    {
        if(match1)
        {
            if(conditionsAreEqual(match1->numConds, match1->conds, d->numConds, d->conds) &&
               match1->device == d->device && match1->id == d->id &&
               match1->type == (ddeventtype_t) d->type)
            {
                *dResult = d;
                return true;
            }
        }

        if(match2 && match2->bid != d->bid)
        {
            if(conditionsAreEqual(match2->numConds, match2->conds, d->numConds, d->conds) &&
               match2->device == d->device && match2->id == d->id &&
               match2->type == d->type)
            {
                *dResult = d;
                return true;
            }
        }
    }

    // Nothing found.
    return false;
}

LoopResult BindContext::forAllCommandBindings(std::function<de::LoopResult (cbinding_t &)> func) const
{
    for(cbinding_t *cb = d->commandBinds.next; cb != &d->commandBinds; cb = cb->next)
    {
        if(auto result = func(*cb)) return result;
    }
    return LoopContinue;
}

LoopResult BindContext::forAllControlBindings(std::function<de::LoopResult (controlbinding_t &)> func) const
{
    for(controlbinding_t *cb = d->controlBinds.next; cb != &d->controlBinds; cb = cb->next)
    {
        if(auto result = func(*cb)) return result;
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
    for(cbinding_t *e = d->commandBinds.next; e != &d->commandBinds; e = e->next, count++) {}

    if(count)
    {
        LOG_INPUT_MSG("  %i event bindings:") << count;
    }

    for(cbinding_t *e = d->commandBinds.next; e != &d->commandBinds; e = e->next)
    {
        CommandBinding_ToString(e, str);
        LOG_INPUT_MSG("  " BIDFORMAT " %s : " _E(>) "%s")
                << e->bid << Str_Text(str) << e->command;
    }

    // Controls.
    count = 0;
    for(controlbinding_t *c = d->controlBinds.next; c != &d->controlBinds; c = c->next, count++) {}

    if(count)
    {
        LOG_INPUT_MSG("  %i control bindings") << count;
    }

    for(controlbinding_t *c = d->controlBinds.next; c != &d->controlBinds; c = c->next)
    {
        char const *controlName = P_PlayerControlById(c->control)->name;

        LOG_INPUT_MSG(_E(D) "  Control \"%s\" " BIDFORMAT ":") << controlName << c->bid;

        for(int k = 0; k < DDMAXPLAYERS; ++k)
        {
            count = 0;
            for(dbinding_t *d = c->deviceBinds[k].next; d != &c->deviceBinds[k];
                d = d->next, count++) {}

            if(!count)
            {
                continue;
            }

            LOG_INPUT_MSG("    Local player %i has %i device bindings for \"%s\":")
                    << k + 1 << count << controlName;

            for(dbinding_t *d = c->deviceBinds[k].next; d != &c->deviceBinds[k]; d = d->next)
            {
                DeviceBinding_ToString(d, str);
                LOG_INPUT_MSG("    " BIDFORMAT " %s") << d->bid << Str_Text(str);
            }
        }
    }

#undef BIDFORMAT
}

void BindContext::writeToFile(FILE *file) const
{
    DENG2_ASSERT(file);
    AutoStr *str = AutoStr_NewStd();

    // Commands.
    for(cbinding_t *e = d->commandBinds.next; e != &d->commandBinds; e = e->next)
    {
        CommandBinding_ToString(e, str);
        fprintf(file, "bindevent \"%s:%s\" \"", d->name.toUtf8().constData(), Str_Text(str));
        M_WriteTextEsc(file, e->command);
        fprintf(file, "\"\n");
    }

    // Controls.
    for(controlbinding_t *cb = d->controlBinds.next; cb != &d->controlBinds; cb = cb->next)
    {
        char const *controlName = P_PlayerControlById(cb->control)->name;

        for(int k = 0; k < DDMAXPLAYERS; ++k)
        for(dbinding_t *db = cb->deviceBinds[k].next; db != &cb->deviceBinds[k]; db = db->next)
        {
            DeviceBinding_ToString(db, str);
            fprintf(file, "bindcontrol local%i-%s \"%s\"\n", k + 1,
                    controlName, Str_Text(str));
        }
    }
}

// ------------------------------------------------------------------------------

void B_InitControlBindingList(controlbinding_t *listRoot)
{
    DENG2_ASSERT(listRoot);
    de::zapPtr(listRoot);
    listRoot->next = listRoot->prev = listRoot;
}

void B_DestroyControlBindingList(controlbinding_t *listRoot)
{
    DENG2_ASSERT(listRoot);
    while(listRoot->next != listRoot)
    {
        B_DestroyControlBinding(listRoot->next);
    }
}

void B_DestroyControlBinding(controlbinding_t *conBin)
{
    if(!conBin) return;

    DENG2_ASSERT(conBin->bid != 0);

    // Unlink first, if linked.
    if(conBin->prev)
    {
        conBin->prev->next = conBin->next;
        conBin->next->prev = conBin->prev;
    }

    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        B_DestroyDeviceBindingList(&conBin->deviceBinds[i]);
    }
    M_Free(conBin);
}
