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

#define DENG_NO_API_MACROS_BINDING

#include "de_platform.h" // strdup macro
#include "ui/b_context.h"

#include <cstring>
#include <QList>
#include <QtAlgorithms>
#include <de/memory.h>
#include <de/Log>
#include "clientapp.h"
#include "dd_main.h"
#include "m_misc.h"
#include "ui/b_main.h"
#include "ui/b_command.h"
#include "ui/p_control.h"
#include "ui/inputdevice.h"
#include "ui/inputdeviceaxiscontrol.h"
#include "ui/inputdevicebuttoncontrol.h"
#include "ui/inputdevicehatcontrol.h"

using namespace de;

static inline InputSystem &inputSys()
{
    return ClientApp::inputSystem();
}

// Binding Context Flags:
#define BCF_ACTIVE              0x01  ///< Context is only used when it is active.
#define BCF_PROTECTED           0x02  ///< Context cannot be (de)activated by plugins.
#define BCF_ACQUIRE_KEYBOARD    0x04  /**  Context has acquired all keyboard states, unless
                                           higher-priority contexts override it. */
#define BCF_ACQUIRE_ALL         0x08  ///< Context will acquire all unacquired states.

DENG2_PIMPL(BindContext)
{
    byte flags = 0;
    String name;                    ///< Symbolic.
    evbinding_t commandBinds;       ///< List of command bindings.
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
    evbinding_t *newCommandBinding(char const *desc, char const *command)
    {
        DENG2_ASSERT(command && command[0]);

        evbinding_t *eb = B_AllocCommandBinding();
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
};

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
    return (d->flags & BCF_ACTIVE) != 0;
}

bool BindContext::isProtected() const
{
    return (d->flags & BCF_PROTECTED) != 0;
}

void BindContext::setProtected(bool yes)
{
    if(yes) d->flags |= BCF_PROTECTED;
    else    d->flags &= ~BCF_PROTECTED;
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
    LOG_INPUT_VERBOSE("%s binding context '%s'")
            << (yes? "Activating" : "Deactivating")
            << d->name;

    d->flags &= ~BCF_ACTIVE;
    if(yes) d->flags |= BCF_ACTIVE;

    B_UpdateAllDeviceStateAssociations();

    if(d->flags & BCF_ACQUIRE_ALL)
    {
        inputSys().forAllDevices([] (InputDevice &device)
        {
            device.reset();
            return LoopContinue;
        });
    }
}

void BindContext::acquireKeyboard(bool yes)
{
    d->flags &= ~BCF_ACQUIRE_KEYBOARD;
    if(yes) d->flags |= BCF_ACQUIRE_KEYBOARD;

    B_UpdateAllDeviceStateAssociations();
}

void BindContext::acquireAll(bool yes)
{
    d->flags &= ~BCF_ACQUIRE_ALL;
    if(yes) d->flags |= BCF_ACQUIRE_ALL;

    B_UpdateAllDeviceStateAssociations();
}

bool BindContext::willAcquireAll() const
{
    return (d->flags & BCF_ACQUIRE_ALL) != 0;
}

bool BindContext::willAcquireKeyboard() const
{
    return (d->flags & BCF_ACQUIRE_KEYBOARD) != 0;
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

void BindContext::deleteMatching(evbinding_t *eventBinding, dbinding_t *deviceBinding)
{
    dbinding_t *devb = nullptr;
    evbinding_t *evb = nullptr;

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

evbinding_t *BindContext::bindCommand(char const *eventDesc, char const *command)
{
    DENG2_ASSERT(eventDesc && command);

    evbinding_t *b = d->newCommandBinding(eventDesc, command);
    if(b)
    {
        /// @todo: In interactive binding mode, should ask the user if the
        /// replacement is ok. For now, just delete the other binding.
        deleteMatching(b, nullptr);
        B_UpdateAllDeviceStateAssociations();
    }

    return b;
}

evbinding_t *BindContext::findCommandBinding(char const *command, int device) const
{
    if(command && command[0])
    {
        for(evbinding_t *i = d->commandBinds.next; i != &d->commandBinds; i = i->next)
        {
            if(qstricmp(i->command, command)) continue;

            if((device < 0 || device >= NUM_INPUT_DEVICES) || i->device == device)
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
    for(evbinding_t *eb = d->commandBinds.next; eb != &d->commandBinds; eb = eb->next)
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

Action *BindContext::actionForEvent(ddevent_t const *event, bool respectHigherAssociatedContexts) const
{
    // See if the command bindings will have it.
    for(evbinding_t *eb = d->commandBinds.next; eb != &d->commandBinds; eb = eb->next)
    {
        if(Action *act = EventBinding_ActionForEvent(eb, event, this, respectHigherAssociatedContexts))
        {
            return act;
        }
    }
    return nullptr;
}

static bool B_AreConditionsEqual(int count1, statecondition_t const *conds1,
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

bool BindContext::findMatchingBinding(evbinding_t *match1, dbinding_t *match2,
    evbinding_t **evResult, dbinding_t **dResult)
{
    DENG2_ASSERT(evResult && dResult);

    *evResult = nullptr;
    *dResult  = nullptr;

    for(evbinding_t *e = d->commandBinds.next; e != &d->commandBinds; e = e->next)
    {
        if(match1 && match1->bid != e->bid)
        {
            if(B_AreConditionsEqual(match1->numConds, match1->conds, e->numConds, e->conds) &&
               match1->device == e->device && match1->id == e->id &&
               match1->type == e->type && match1->state == e->state)
            {
                *evResult = e;
                return true;
            }
        }
        if(match2)
        {
            if(B_AreConditionsEqual(match2->numConds, match2->conds, e->numConds, e->conds) &&
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
            if(B_AreConditionsEqual(match1->numConds, match1->conds, d->numConds, d->conds) &&
               match1->device == d->device && match1->id == d->id &&
               match1->type == (ddeventtype_t) d->type)
            {
                *dResult = d;
                return true;
            }
        }

        if(match2 && match2->bid != d->bid)
        {
            if(B_AreConditionsEqual(match2->numConds, match2->conds, d->numConds, d->conds) &&
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

LoopResult BindContext::forAllCommandBindings(std::function<de::LoopResult (evbinding_t &)> func) const
{
    for(evbinding_t *cb = d->commandBinds.next; cb != &d->commandBinds; cb = cb->next)
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
    for(evbinding_t *e = d->commandBinds.next; e != &d->commandBinds; e = e->next, count++) {}

    if(count)
    {
        LOG_INPUT_MSG("  %i event bindings:") << count;
    }

    for(evbinding_t *e = d->commandBinds.next; e != &d->commandBinds; e = e->next)
    {
        B_EventBindingToString(e, str);
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
                B_DeviceBindingToString(d, str);
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
    for(evbinding_t *e = d->commandBinds.next; e != &d->commandBinds; e = e->next)
    {
        B_EventBindingToString(e, str);
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
            B_DeviceBindingToString(db, str);
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

// ------------------------------------------------------------------------------

typedef QList<BindContext *> BindContexts;
static BindContexts bindContexts; ///< Ordered from highest to lowest priority.

void B_DestroyAllContexts()
{
    if(bindContexts.isEmpty()) return;

    qDeleteAll(bindContexts);
    bindContexts.clear();
}

int B_ContextCount()
{
    return bindContexts.count();
}

bool B_HasContext(String const &name)
{
    return B_ContextPtr(name) != nullptr;
}

BindContext &B_Context(String const &name)
{
    if(BindContext *bc = B_ContextPtr(name))
    {
        return *bc;
    }
    throw Error("B_Context", "Unknown binding context name:" + name);
}

/// @todo: Optimize O(n) search...
BindContext *B_ContextPtr(de::String const &name)
{
    for(BindContext const *bc : bindContexts)
    {
        if(!bc->name().compareWithoutCase(name))
        {
            return const_cast<BindContext *>(bc);
        }
    }
    return nullptr;
}

BindContext &B_ContextAt(int position)
{
    if(position >= 0 && position < bindContexts.count())
    {
        return *bindContexts.at(position);
    }
    throw Error("B_ContextAt", "Invalid position:" + String::number(position));
}

int B_ContextPositionOf(BindContext *bc)
{
    return (bc? bindContexts.indexOf(bc) : -1);
}

BindContext *B_NewContext(String const &name)
{
    BindContext *bc = new BindContext(name);
    bindContexts.prepend(bc);
    return bc;
}

Action *B_ActionForEvent(ddevent_t const *event)
{
    event_t ev;
    bool validGameEvent = InputSystem::convertEvent(event, &ev);

    for(BindContext *bc : bindContexts)
    {
        if(!bc->isActive()) continue;

        if(Action *act = bc->actionForEvent(event))
        {
            return act;
        }

        /**
         * @todo Conceptually the fallback responders don't belong: instead of
         * "responding" (immediately performing a reaction), we should be
         * returning an Action instance. -jk
         */

        // Try the fallback responders.
        if(bc->tryFallbackResponders(*event, ev, validGameEvent))
        {
            return nullptr; // fallback responder executed something
        }
    }

    return nullptr; // Nobody had a binding for this event.
}

/// @todo: Don't format a string - just collect pointers.
int B_BindingsForCommand(char const *cmd, char *buf, size_t bufSize)
{
    DENG2_ASSERT(cmd && buf);
    AutoStr *result = AutoStr_NewStd();
    AutoStr *str    = AutoStr_NewStd();

    int numFound = 0;
    for(BindContext const *bc : bindContexts)
    {
        bc->forAllCommandBindings([&bc, &numFound, &str, &cmd, &result] (evbinding_t &eb)
        {
            if(strcmp(eb.command, cmd))
                return LoopContinue;

            // It's here!
            if(numFound)
            {
                Str_Append(result, " ");
            }
            numFound++;
            B_EventBindingToString(&eb, str);
            Str_Appendf(result, "%i@%s:%s", eb.bid, bc->name().toUtf8().constData(), Str_Text(str));
            return LoopContinue;
        });
    }

    // Copy the result to the return buffer.
    std::memset(buf, 0, bufSize);
    strncpy(buf, Str_Text(result), bufSize - 1);

    return numFound;
}

/// @todo: Don't format a string - just collect pointers.
int B_BindingsForControl(int localPlayer, char const *controlName, int inverse,
    char *buf, size_t bufSize)
{
    DENG2_ASSERT(controlName && buf);

    if(localPlayer < 0 || localPlayer >= DDMAXPLAYERS)
        return 0;

    AutoStr *result = AutoStr_NewStd();
    AutoStr *str    = AutoStr_NewStd();

    int numFound = 0;
    for(BindContext *bc : bindContexts)
    {
        bc->forAllControlBindings([&bc, &controlName, &inverse, &localPlayer, &numFound, &result, &str] (controlbinding_t &cb)
        {
            char const *name = P_PlayerControlById(cb.control)->name;

            if(strcmp(name, controlName))
                return LoopContinue; // Wrong control.

            for(dbinding_t *db = cb.deviceBinds[localPlayer].next; db != &cb.deviceBinds[localPlayer]; db = db->next)
            {
                if(inverse == BFCI_BOTH ||
                   (inverse == BFCI_ONLY_NON_INVERSE && !(db->flags & CBDF_INVERSE)) ||
                   (inverse == BFCI_ONLY_INVERSE && (db->flags & CBDF_INVERSE)))
                {
                    // It's here!
                    if(numFound)
                    {
                        Str_Append(result, " ");
                    }
                    numFound++;

                    B_DeviceBindingToString(db, str);
                    Str_Appendf(result, "%i@%s:%s", db->bid, bc->name().toUtf8().constData(), Str_Text(str));
                }
            }
            return LoopContinue;
        });
    }

    // Copy the result to the return buffer.
    std::memset(buf, 0, bufSize);
    strncpy(buf, Str_Text(result), bufSize - 1);

    return numFound;
}

/// @todo: Most of this logic belongs in BindContext.
void B_UpdateAllDeviceStateAssociations()
{
    // Clear all existing associations.
    inputSys().forAllDevices([] (InputDevice &device)
    {
        device.forAllControls([] (InputDeviceControl &control)
        {
            control.clearBindContextAssociation();
            return LoopContinue;
        });
        return LoopContinue;
    });

    // Mark all bindings in all contexts.
    for(BindContext *bc : bindContexts)
    {
        // Skip inactive contexts.
        if(!bc->isActive())
            continue;

        bc->forAllCommandBindings([&bc] (evbinding_t &cb)
        {
            InputDevice &device = inputSys().device(cb.device);

            switch(cb.type)
            {
            case E_TOGGLE: {
                InputDeviceControl &ctrl = device.button(cb.id);
                if(!ctrl.hasBindContext())
                {
                    ctrl.setBindContext(bc);
                }
                break; }

            case E_AXIS: {
                InputDeviceControl &ctrl = device.axis(cb.id);
                if(!ctrl.hasBindContext())
                {
                    ctrl.setBindContext(bc);
                }
                break; }

            case E_ANGLE: {
                InputDeviceControl &ctrl = device.hat(cb.id);
                if(!ctrl.hasBindContext())
                {
                    ctrl.setBindContext(bc);
                }
                break; }

            case E_SYMBOLIC:
                break;

            default:
                DENG2_ASSERT(!"B_UpdateAllDeviceStateAssociations: Invalid eb.type");
                break;
            }
            return LoopContinue;
        });

        bc->forAllControlBindings([&bc] (controlbinding_t &cb)
        {
            // Associate all the device bindings.
            for(int k = 0; k < DDMAXPLAYERS; ++k)
            for(dbinding_t *db = cb.deviceBinds[k].next; db != &cb.deviceBinds[k]; db = db->next)
            {
                InputDevice &device = inputSys().device(db->device);

                switch(db->type)
                {
                case CBD_TOGGLE: {
                    InputDeviceControl &ctrl = device.button(db->id);
                    if(!ctrl.hasBindContext())
                    {
                        ctrl.setBindContext(bc);
                    }
                    break; }

                case CBD_AXIS: {
                    InputDeviceControl &ctrl = device.axis(db->id);
                    if(!ctrl.hasBindContext())
                    {
                        ctrl.setBindContext(bc);
                    }
                    break; }

                case CBD_ANGLE: {
                    InputDeviceControl &ctrl = device.hat(db->id);
                    if(!ctrl.hasBindContext())
                    {
                        ctrl.setBindContext(bc);
                    }
                    break; }

                default:
                    DENG2_ASSERT(!"B_UpdateAllDeviceStateAssociations: Invalid db->type");
                    break;
                }
            }
            return LoopContinue;
        });

        // If the context have made a broad device acquisition, mark all relevant states.
        if(bc->willAcquireKeyboard())
        {
            InputDevice &device = inputSys().device(IDEV_KEYBOARD);
            if(device.isActive())
            {
                device.forAllControls([&bc] (InputDeviceControl &control)
                {
                    if(!control.hasBindContext())
                    {
                        control.setBindContext(bc);
                    }
                    return LoopContinue;
                });
            }
        }

        if(bc->willAcquireAll())
        {
            inputSys().forAllDevices([&bc] (InputDevice &device)
            {
                if(device.isActive())
                {
                    device.forAllControls([&bc] (InputDeviceControl &control)
                    {
                        if(!control.hasBindContext())
                        {
                            control.setBindContext(bc);
                        }
                        return LoopContinue;
                    });
                }
                return LoopContinue;
            });
        }
    }

    // Now that we know what are the updated context associations, let's check the devices
    // and see if any of the states need to be expired.
    inputSys().forAllDevices([] (InputDevice &device)
    {
        device.forAllControls([] (InputDeviceControl &control)
        {
            if(!control.inDefaultState())
            {
                control.expireBindContextAssociationIfChanged();
            }
            return LoopContinue;
        });
        return LoopContinue;
    });
}

LoopResult B_ForAllContexts(std::function<LoopResult (BindContext &)> func)
{
    for(BindContext *bc : bindContexts)
    {
        if(auto result = func(*bc)) return result;
    }
    return LoopContinue;
}

#undef B_SetContextFallback
void B_SetContextFallback(char const *name, int (*responderFunc)(event_t *))
{
    if(B_HasContext(name))
    {
        B_Context(name).setFallbackResponder(responderFunc);
    }
}

// b_main.c
DENG_EXTERN_C int DD_GetKeyCode(char const *key);

DENG_DECLARE_API(B) =
{
    { DE_API_BINDING },
    B_SetContextFallback,
    B_BindingsForCommand,
    B_BindingsForControl,
    DD_GetKeyCode
};
