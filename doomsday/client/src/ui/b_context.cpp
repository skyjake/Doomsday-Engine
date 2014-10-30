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
#include <de/memory.h>
#include <de/Log>
#include "dd_main.h"
#include "m_misc.h"
#include "ui/b_main.h"
#include "ui/p_control.h"

using namespace de;

static int bindContextCount;
static bcontext_t **bindContexts;

void B_DestroyAllContexts()
{
    if(bindContexts)
    {
        // Do not use the global bindContextCount to control the loop; it is
        // changed as a sideeffect of B_DestroyContext() and also, the
        // bindContexts array itself is shifted back to idx 0 afterwards.
        int const numBindClasses = bindContextCount;
        for(int i = 0; i < numBindClasses; ++i)
        {
            B_DestroyContext(bindContexts[0]);
        }
        M_Free(bindContexts);
    }
    bindContexts     = nullptr;
    bindContextCount = 0;
}

void B_UpdateAllDeviceStateAssociations()
{
    I_ClearAllDeviceContextAssociations();

    // We need to iterate through all the device bindings in all context.
    for(int i = 0; i < bindContextCount; ++i)
    {
        bcontext_t *bc = bindContexts[i];

        // Skip inactive contexts.
        if(!(bc->flags & BCF_ACTIVE))
            continue;

        // Mark all event bindings in the context.
        for(evbinding_t *eb = bc->commandBinds.next; eb != &bc->commandBinds; eb = eb->next)
        {
            InputDevice &dev = I_Device(eb->device);

            switch(eb->type)
            {
            case E_TOGGLE: {
                InputDeviceButtonControl &button = dev.button(eb->id);
                if(!button.association().bContext)
                {
                    button.association().bContext = bc;
                }
                break; }

            case E_AXIS: {
                InputDeviceAxisControl &axis = dev.axis(eb->id);
                if(!axis.association().bContext)
                {
                    axis.association().bContext = bc;
                }
                break; }

            case E_ANGLE: {
                InputDeviceHatControl &hat = dev.hat(eb->id);
                if(!hat.association().bContext)
                {
                    hat.association().bContext = bc;
                }
                break; }

            case E_SYMBOLIC:
                break;

            default:
                App_Error("B_UpdateAllDeviceStateAssociations: Invalid value eb->type: %i.", (int) eb->type);
                break;
            }
        }

        // All controls in the context.
        for(controlbinding_t *conBin = bc->controlBinds.next; conBin != &bc->controlBinds; conBin = conBin->next)
        {
            // Associate all the device bindings.
            for(int k = 0; k < DDMAXPLAYERS; ++k)
            for(dbinding_t *db = conBin->deviceBinds[k].next; db != &conBin->deviceBinds[k]; db = db->next)
            {
                InputDevice &dev = I_Device(db->device);

                switch(db->type)
                {
                case CBD_TOGGLE: {
                    InputDeviceButtonControl &button = dev.button(db->id);
                    if(!button.association().bContext)
                    {
                        button.association().bContext = bc;
                    }
                    break; }

                case CBD_AXIS: {
                    InputDeviceAxisControl &axis = dev.axis(db->id);
                    if(!axis.association().bContext)
                    {
                        axis.association().bContext = bc;
                    }
                    break; }

                case CBD_ANGLE: {
                    InputDeviceHatControl &hat = dev.hat(db->id);
                    if(!hat.association().bContext)
                    {
                        hat.association().bContext = bc;
                    }
                    break; }

                default:
                    App_Error("B_UpdateAllDeviceStateAssociations: Invalid value db->type: %i.", (int) db->type);
                    break;
                }
            }
        }

        // If the context have made a broad device acquisition, mark all
        // relevant states.
        if(bc->flags & BCF_ACQUIRE_KEYBOARD)
        {
            InputDevice &dev = I_Device(IDEV_KEYBOARD);

            for(int k = 0; k < dev.buttonCount(); ++k)
            {
                InputDeviceButtonControl &button = dev.button(k);
                if(!button.association().bContext)
                {
                    button.association().bContext = bc;
                }
            }
        }

        if(bc->flags & BCF_ACQUIRE_ALL)
        {
            for(int k = 0; k < NUM_INPUT_DEVICES; ++k)
            {
                InputDevice *dev = I_DevicePtr(k);
                if(!dev || !dev->isActive()) continue;

                for(int m = 0; m < dev->buttonCount(); ++m)
                {
                    InputDeviceButtonControl &button = dev->button(m);
                    if(!button.association().bContext)
                    {
                        button.association().bContext = bc;
                    }
                }
                for(int m = 0; m < dev->axisCount(); ++m)
                {
                    InputDeviceAxisControl &axis = dev->axis(m);
                    if(!axis.association().bContext)
                    {
                        axis.association().bContext = bc;
                    }
                }
                for(int m = 0; m < dev->hatCount(); ++m)
                {
                    InputDeviceHatControl &hat = dev->hat(m);
                    if(!hat.association().bContext)
                    {
                        hat.association().bContext = bc;
                    }
                }
            }
        }
    }

    // Now that we know what are the updated context associations, let's check
    // the devices and see if any of the states need to be expired.
    for(int i = 0; i < NUM_INPUT_DEVICES; ++i)
    {
        InputDevice &dev = I_Device(i);

        for(int k = 0; k < dev.buttonCount(); ++k)
        {
            InputDeviceButtonControl &button = dev.button(k);
            inputdevassoc_t &assoc = button.association();

            if(assoc.bContext != assoc.prevBContext && button.isDown())
            {
                // No longer valid.
                assoc.flags |= IDAF_EXPIRED;
                assoc.flags &= ~IDAF_TRIGGERED; // Not any more.
            }
        }

        for(int k = 0; k < dev.axisCount(); ++k)
        {
            InputDeviceAxisControl &axis = dev.axis(k);
            inputdevassoc_t &assoc = axis.association();

            if(assoc.bContext != assoc.prevBContext && axis.position() != 0)
            {
                // No longer valid.
                assoc.flags |= IDAF_EXPIRED;
            }
        }

        for(int k = 0; k < dev.hatCount(); ++k)
        {
            InputDeviceHatControl &hat = dev.hat(k);
            inputdevassoc_t &assoc = hat.association();

            if(assoc.bContext != assoc.prevBContext && hat.position() >= 0)
            {
                // No longer valid.
                assoc.flags |= IDAF_EXPIRED;
            }
        }
    }
}

static void B_SetContextCount(int count)
{
    bindContexts     = (bcontext_t **) M_Realloc(bindContexts, sizeof(*bindContexts) * count);
    bindContextCount = count;
}

static void B_InsertContext(bcontext_t *bc, int contextIdx)
{
    DENG2_ASSERT(bc);

    B_SetContextCount(bindContextCount + 1);
    if(contextIdx < bindContextCount - 1)
    {
        // We need to make room for this new binding context.
        std::memmove(&bindContexts[contextIdx + 1], &bindContexts[contextIdx],
                     sizeof(bcontext_t *) * (bindContextCount - 1 - contextIdx));
    }
    bindContexts[contextIdx] = bc;
}

static void B_RemoveContext(bcontext_t *bc)
{
    if(!bc) return;

    int contextIdx = B_GetContextPos(bc);

    if(contextIdx >= 0)
    {
        std::memmove(&bindContexts[contextIdx], &bindContexts[contextIdx + 1],
                     sizeof(bcontext_t *) * (bindContextCount - 1 - contextIdx));

        B_SetContextCount(bindContextCount - 1);
    }
}

bcontext_t *B_NewContext(char const *name)
{
    bcontext_t *bc = (bcontext_t *) M_Calloc(sizeof(bcontext_t));
    bc->name = strdup(name);
    B_InitCommandBindingList(&bc->commandBinds);
    B_InitControlBindingList(&bc->controlBinds);
    B_InsertContext(bc, 0);
    return bc;
}

void B_DestroyContext(bcontext_t *bc)
{
    if(!bc) return;

    B_RemoveContext(bc);
    M_Free(bc->name);
    B_ClearContext(bc);
    M_Free(bc);
}

void B_ClearContext(bcontext_t *bc)
{
    DENG2_ASSERT(bc);
    B_DestroyCommandBindingList(&bc->commandBinds);
    B_DestroyControlBindingList(&bc->controlBinds);
}

void B_ActivateContext(bcontext_t *bc, dd_bool doActivate)
{
    if(!bc) return;

    LOG_INPUT_VERBOSE("%s binding context '%s'")
            << (doActivate? "Activating" : "Deactivating")
            << bc->name;

    bc->flags &= ~BCF_ACTIVE;
    if(doActivate) bc->flags |= BCF_ACTIVE;

    B_UpdateAllDeviceStateAssociations();

    if(bc->flags & BCF_ACQUIRE_ALL)
    {
        I_ResetAllDevices();
    }
}

void B_AcquireKeyboard(bcontext_t *bc, dd_bool doAcquire)
{
    DENG2_ASSERT(bc);

    bc->flags &= ~BCF_ACQUIRE_KEYBOARD;
    if(doAcquire) bc->flags |= BCF_ACQUIRE_KEYBOARD;

    B_UpdateAllDeviceStateAssociations();
}

void B_AcquireAll(bcontext_t *bc, dd_bool doAcquire)
{
    DENG2_ASSERT(bc);

    bc->flags &= ~BCF_ACQUIRE_ALL;
    if(doAcquire) bc->flags |= BCF_ACQUIRE_ALL;

    B_UpdateAllDeviceStateAssociations();
}

void B_SetContextFallbackForDDEvents(char const *name, int (*ddResponderFunc)(ddevent_t const *))
{
    if(bcontext_t *ctx = B_ContextByName(name))
    {
        ctx->ddFallbackResponder = ddResponderFunc;
    }
}

void B_SetContextFallback(char const *name, int (*responderFunc)(event_t *))
{
    if(bcontext_t *ctx = B_ContextByName(name))
    {
        ctx->fallbackResponder = responderFunc;
    }
}

bcontext_t *B_ContextByName(char const *name)
{
    for(int i = 0; i < bindContextCount; ++i)
    {
        if(!strcasecmp(name, bindContexts[i]->name))
        {
            return bindContexts[i];
        }
    }
    return nullptr;
}

bcontext_t *B_ContextByPos(int pos)
{
    if(pos >= 0 && pos < bindContextCount)
    {
        return bindContexts[pos];
    }
    return nullptr;
}

int B_ContextCount()
{
    return bindContextCount;
}

int B_GetContextPos(bcontext_t *bc)
{
    if(bc)
    {
        for(int i = 0; i < bindContextCount; ++i)
        {
            if(bindContexts[i] == bc)
                return i;
        }
    }
    return -1;
}

void B_ReorderContext(bcontext_t *bc, int pos)
{
    B_RemoveContext(bc);
    B_InsertContext(bc, pos);
}

controlbinding_t *B_NewControlBinding(bcontext_t *bc)
{
    DENG2_ASSERT(bc);

    controlbinding_t *conBin = (controlbinding_t *) M_Calloc(sizeof(*conBin));
    conBin->bid = B_NewIdentifier();
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        B_InitDeviceBindingList(&conBin->deviceBinds[i]);
    }

    // Link it in.
    conBin->next = &bc->controlBinds;
    conBin->prev = bc->controlBinds.prev;
    bc->controlBinds.prev->next = conBin;
    bc->controlBinds.prev = conBin;

    return conBin;
}

controlbinding_t *B_FindControlBinding(bcontext_t *bc, int control)
{
    if(bc)
    {
        for(controlbinding_t *i = bc->controlBinds.next; i != &bc->controlBinds; i = i->next)
        {
            if(i->control == control)
                return i;
        }
    }
    return nullptr;
}

controlbinding_t *B_GetControlBinding(bcontext_t *bc, int control)
{
    controlbinding_t *b = B_FindControlBinding(bc, control);
    if(!b)
    {
        // Create a new one.
        b = B_NewControlBinding(bc);
        b->control = control;
    }
    return b;
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

dd_bool B_DeleteBinding(bcontext_t *bc, int bid)
{
    DENG2_ASSERT(bc);

    // Check if it is one of the command bindings.
    for(evbinding_t *eb = bc->commandBinds.next; eb != &bc->commandBinds; eb = eb->next)
    {
        if(eb->bid == bid)
        {
            B_DestroyCommandBinding(eb);
            return true;
        }
    }

    // How about one of the control bindings?
    for(controlbinding_t *conBin = bc->controlBinds.next; conBin != &bc->controlBinds; conBin = conBin->next)
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

de::Action *BindContext_ActionForEvent(bcontext_t *bc, ddevent_t const *event,
                                       bool respectHigherAssociatedContexts)
{
    DENG2_ASSERT(bc);
    // See if the command bindings will have it.
    for(evbinding_t *eb = bc->commandBinds.next; eb != &bc->commandBinds; eb = eb->next)
    {
        if(de::Action *act = EventBinding_ActionForEvent(eb, event, bc, respectHigherAssociatedContexts))
        {
            return act;
        }
    }
    return nullptr;
}

Action *B_ActionForEvent(ddevent_t const *event)
{
    event_t ev;
    bool validGameEvent = I_ConvertEvent(event, &ev);

    for(int i = 0; i < bindContextCount; ++i)
    {
        bcontext_t *bc = bindContexts[i];
        if(!(bc->flags & BCF_ACTIVE))
            continue;

        if(de::Action *act = BindContext_ActionForEvent(bc, event, true))
        {
            return act;
        }

        /**
         * @todo Conceptually the fallback responders don't belong: instead of
         * "responding" (immediately performing a reaction), we should be
         * returning an Action instance. -jk
         */

        // Try the fallback responders.
        if(bc->ddFallbackResponder && bc->ddFallbackResponder(event))
            return nullptr; // fallback responder executed something

        if(validGameEvent && bc->fallbackResponder && bc->fallbackResponder(&ev))
            return nullptr; // fallback responder executed something
    }

    return nullptr; // Nobody had a binding for this event.
}

int B_BindingsForCommand(char const *cmd, char *buf, size_t bufSize)
{
    DENG2_ASSERT(cmd && buf);
    ddstring_t result; Str_Init(&result);
    ddstring_t str; Str_Init(&str);

    int numFound = 0;
    for(int i = 0; i < bindContextCount; ++i)
    {
        bcontext_t *bc = bindContexts[i];
        for(evbinding_t *eb = bc->commandBinds.next; eb != &bc->commandBinds; eb = eb->next)
        {
            if(strcmp(eb->command, cmd))
                continue;

            // It's here!
            if(numFound)
            {
                Str_Append(&result, " ");
            }
            numFound++;
            B_EventBindingToString(eb, &str);
            Str_Appendf(&result, "%i@%s:%s", eb->bid, bc->name, Str_Text(&str));
        }
    }

    // Copy the result to the return buffer.
    std::memset(buf, 0, bufSize);
    strncpy(buf, Str_Text(&result), bufSize - 1);

    Str_Free(&result);
    Str_Free(&str);
    return numFound;
}

int B_BindingsForControl(int localPlayer, char const *controlName, int inverse,
    char *buf, size_t bufSize)
{
    DENG2_ASSERT(controlName && buf);

    if(localPlayer < 0 || localPlayer >= DDMAXPLAYERS)
        return 0;

    ddstring_t result; Str_Init(&result);
    ddstring_t str; Str_Init(&str);

    int numFound = 0;
    for(int i = 0; i < bindContextCount; ++i)
    {
        bcontext_t *bc = bindContexts[i];
        for(controlbinding_t *cb = bc->controlBinds.next; cb != &bc->controlBinds; cb = cb->next)
        {
            char const *name = P_PlayerControlById(cb->control)->name;

            if(strcmp(name, controlName))
                continue; // Wrong control.

            for(dbinding_t *db = cb->deviceBinds[localPlayer].next; db != &cb->deviceBinds[localPlayer]; db = db->next)
            {
                if(inverse == BFCI_BOTH ||
                   (inverse == BFCI_ONLY_NON_INVERSE && !(db->flags & CBDF_INVERSE)) ||
                   (inverse == BFCI_ONLY_INVERSE && (db->flags & CBDF_INVERSE)))
                {
                    // It's here!
                    if(numFound)
                    {
                        Str_Append(&result, " ");
                    }
                    numFound++;

                    B_DeviceBindingToString(db, &str);
                    Str_Appendf(&result, "%i@%s:%s", db->bid, bc->name, Str_Text(&str));
                }
            }
        }
    }

    // Copy the result to the return buffer.
    std::memset(buf, 0, bufSize);
    strncpy(buf, Str_Text(&result), bufSize - 1);

    Str_Free(&result);
    Str_Free(&str);
    return numFound;
}

dd_bool B_AreConditionsEqual(int count1, statecondition_t const *conds1,
                             int count2, statecondition_t const *conds2)
{
    //DENG2_ASSERT(conds1 && conds2);

    // Quick test (assumes there are no duplicated conditions).
    if(count1 != count2) return false;

    for(int i = 0; i < count1; ++i)
    {
        dd_bool found = false;
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

dd_bool B_FindMatchingBinding(bcontext_t *bc, evbinding_t *match1, dbinding_t *match2,
    evbinding_t **evResult, dbinding_t **dResult)
{
    DENG2_ASSERT(bc && evResult && dResult);

    *evResult = nullptr;
    *dResult  = nullptr;

    for(evbinding_t *e = bc->commandBinds.next; e != &bc->commandBinds; e = e->next)
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

    for(controlbinding_t *c = bc->controlBinds.next; c != &bc->controlBinds; c = c->next)
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

void B_PrintContexts()
{
    LOG_INPUT_MSG("%i binding contexts defined:") << bindContextCount;

    for(int i = 0; i < bindContextCount; ++i)
    {
        bcontext_t *bc = bindContexts[i];
        LOG_INPUT_MSG("[%3i] %s\"%s\"" _E(.) " (%s)")
                << i
                << (bc->flags & BCF_ACTIVE? _E(b) : _E(w))
                << bc->name
                << (bc->flags & BCF_ACTIVE? "active" : "inactive");
    }
}

void B_PrintAllBindings()
{
#define BIDFORMAT "[%3i]"

    LOG_INPUT_MSG("%i binding contexts defined") << bindContextCount;

    AutoStr *str = AutoStr_NewStd();
    for(int i = 0; i < bindContextCount; ++i)
    {
        bcontext_t *bc = bindContexts[i];

        LOG_INPUT_MSG(_E(b)"Context \"%s\" (%s):")
                << bc->name << (bc->flags & BCF_ACTIVE? "active" : "inactive");

        // Commands.
        int count = 0;
        for(evbinding_t *e = bc->commandBinds.next; e != &bc->commandBinds; e = e->next, count++) {}

        if(count)
        {
            LOG_INPUT_MSG("  %i event bindings:") << count;
        }

        for(evbinding_t *e = bc->commandBinds.next; e != &bc->commandBinds; e = e->next)
        {
            B_EventBindingToString(e, str);
            LOG_INPUT_MSG("  " BIDFORMAT " %s : " _E(>) "%s")
                    << e->bid << Str_Text(str) << e->command;
        }

        // Controls.
        count = 0;
        for(controlbinding_t *c = bc->controlBinds.next; c != &bc->controlBinds; c = c->next, count++) {}

        if(count)
        {
            LOG_INPUT_MSG("  %i control bindings") << count;
        }

        for(controlbinding_t *c = bc->controlBinds.next; c != &bc->controlBinds; c = c->next)
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
    }

#undef BIDFORMAT
}

void B_WriteContextToFile(bcontext_t const *bc, FILE *file)
{
    DENG2_ASSERT(bc && file);
    AutoStr *str = AutoStr_NewStd();

    // Commands.
    for(evbinding_t *e = bc->commandBinds.next; e != &bc->commandBinds; e = e->next)
    {
        B_EventBindingToString(e, str);
        fprintf(file, "bindevent \"%s:%s\" \"", bc->name, Str_Text(str));
        M_WriteTextEsc(file, e->command);
        fprintf(file, "\"\n");
    }

    // Controls.
    for(controlbinding_t *cb = bc->controlBinds.next; cb != &bc->controlBinds; cb = cb->next)
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
