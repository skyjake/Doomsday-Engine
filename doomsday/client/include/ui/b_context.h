/** @file b_context.h  Input system binding contexts.
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

#ifndef CLIENT_INPUTSYSTEM_BINDCONTEXT_H
#define CLIENT_INPUTSYSTEM_BINDCONTEXT_H

#include <functional>
#include "dd_share.h"
#include "b_command.h"
#include "b_device.h"

struct controlbinding_t
{
    controlbinding_t *next;
    controlbinding_t *prev;
    int bid;      ///< Unique identifier.
    int control;  ///< Identifier of the player control.
    dbinding_t deviceBinds[DDMAXPLAYERS];  ///< Separate bindings for each local player.
};

typedef int (*DDFallbackResponderFunc)(ddevent_t const *);
typedef int (*FallbackResponderFunc)(event_t *);

/**
 * Binding context.
 *
 * @ingroup ui
 */
class BindContext
{
public:
    explicit BindContext(de::String const &name);
    ~BindContext();

    bool isActive() const;
    bool isProtected() const;

    void setProtected(bool yes = true);

    de::String name() const;
    void setName(de::String const &newName);

    void activate(bool yes = true);
    inline void deactivate(bool yes = true) { activate(!yes); }

    void acquireAll(bool yex = true);
    void acquireKeyboard(bool yes = true);

    bool willAcquireAll() const;
    bool willAcquireKeyboard() const;

    void printAllBindings() const;
    void writeToFile(FILE *file) const;

    int tryFallbackResponders(ddevent_t const &event, event_t &ev, bool validGameEvent);
    void setDDFallbackResponder(DDFallbackResponderFunc newResponderFunc);
    void setFallbackResponder(FallbackResponderFunc newResponderFunc);

public: // --------------------------------------------------------------------------

    void clearAllBindings();

    /**
     * @return  @c true if the binding was found and deleted.
     */
    bool deleteBinding(int bid);

    dbinding_t *findDeviceBinding(int device, cbdevtype_t bindType, int id);

    /**
     * Looks through context @a bc and looks for a binding that matches either
     * @a match1 or @a match2.
     */
    bool findMatchingBinding(evbinding_t *match1, dbinding_t *match2,
                             evbinding_t **evResult, dbinding_t **dResult);

    void deleteMatching(evbinding_t *eventBinding, dbinding_t *deviceBinding);

    // ---

    evbinding_t *bindCommand(char const *eventDesc, char const *command);

    /**
     * @param device  Use @c < 0 || >= NUM_INPUT_DEVICES for wildcard search.
     */
    evbinding_t *findCommandBinding(char const *command, int device) const;

    /**
     * Iterate through all the evbinding_ts of the context.
     */
    de::LoopResult forAllCommandBindings(std::function<de::LoopResult (evbinding_t &)> func) const;

    // ---

    controlbinding_t *findControlBinding(int control) const;

    controlbinding_t *getControlBinding(int control);

    /**
     * Iterate through all the evbinding_ts of the context.
     */
    de::LoopResult forAllControlBindings(std::function<de::LoopResult (controlbinding_t &)> func) const;

public: // --------------------------------------------------------------------------

    /**
     * Finds the action bound to a given event.
     *
     * @param event                            Event to match against.
     * @param respectHigherAssociatedContexts  Bindings shadowed by higher active contexts.
     *
     * @return Action instance (caller gets ownership), or @c nullptr if not found.
     */
    de::Action *actionForEvent(ddevent_t const *event,
                               bool respectHigherAssociatedContexts = true) const;

private:
    DENG2_PRIVATE(d)
};

// ------------------------------------------------------------------------------

void B_DestroyControlBinding(controlbinding_t *conBin);

void B_InitControlBindingList(controlbinding_t *listRoot);

void B_DestroyControlBindingList(controlbinding_t *listRoot);

// ------------------------------------------------------------------------------

/**
 * Destroy all binding contexts and the bindings within the contexts.
 * To be called at shutdown time.
 */
void B_DestroyAllContexts();

int B_ContextCount();

bool B_HasContext(de::String const &name);

BindContext &B_Context(de::String const &name);

BindContext *B_ContextPtr(de::String const &name);

BindContext &B_ContextAt(int position);

int B_ContextPositionOf(BindContext *bc);

/**
 * Creates a new binding context. The new context has the highest priority
 * of all existing contexts, and is inactive.
 */
BindContext *B_NewContext(de::String const &name);

/**
 * Finds the action bound to a given event, iterating through all enabled
 * binding contexts.
 *
 * @param event  Event to match against.
 *
 * @return Action instance (caller gets ownership), or @c nullptr if not found.
 */
de::Action *B_ActionForEvent(ddevent_t const *event);

/**
 * Marks all device states with the highest-priority binding context to which they have
 * a connection via device bindings. This ensures that if a high-priority context is
 * using a particular device state, lower-priority contexts will not be using the same
 * state for their own controls.
 *
 * Called automatically whenever a context is activated or deactivated.
 */
void B_UpdateAllDeviceStateAssociations();

/**
 * Iterate through all the BindContexts from highest to lowest priority.
 */
de::LoopResult B_ForAllContexts(std::function<de::LoopResult (BindContext &)> func);

#endif // CLIENT_INPUTSYSTEM_BINDCONTEXT_H
