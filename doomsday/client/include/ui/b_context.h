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

#include "dd_share.h"
#include "b_command.h"
#include "b_device.h"

typedef struct controlbinding_s {
    struct controlbinding_s *next;
    struct controlbinding_s *prev;
    int bid;      ///< Unique identifier.
    int control;  ///< Identifier of the player control.
    dbinding_t deviceBinds[DDMAXPLAYERS];  ///< Separate bindings for each local player.
} controlbinding_t;

// Binding Context Flags:
#define BCF_ACTIVE              0x01  ///< Context is only used when it is active.
#define BCF_PROTECTED           0x02  ///< Context cannot be (de)activated by plugins.
#define BCF_ACQUIRE_KEYBOARD    0x04  /**  Context has acquired all keyboard states, unless
                                           higher-priority contexts override it. */
#define BCF_ACQUIRE_ALL         0x08  ///< Context will acquire all unacquired states.

struct bcontext_t
{
    char *name;  ///< Name of the binding context.
    byte flags;
    evbinding_t commandBinds;  ///< List of command bindings.
    controlbinding_t controlBinds;
    int (*ddFallbackResponder)(ddevent_t const *ddev);
    int (*fallbackResponder)(event_t *event);
};

/**
 * Marks all device states with the highest-priority binding context to
 * which they have a connection via device bindings. This ensures that if a
 * high-priority context is using a particular device state, lower-priority
 * contexts will not be using the same state for their own controls.
 *
 * Called automatically whenever a context is activated or deactivated.
 */
void B_UpdateAllDeviceStateAssociations();

/**
 * Creates a new binding context. The new context has the highest priority
 * of all existing contexts, and is inactive.
 */
bcontext_t *B_NewContext(char const *name);

/**
 * Destroy all binding contexts and the bindings within the contexts.
 * To be called at shutdown time.
 */
void B_DestroyAllContexts();

void B_ActivateContext(bcontext_t *bc, dd_bool doActivate);

void B_AcquireKeyboard(bcontext_t *bc, dd_bool doAcquire);

void B_AcquireAll(bcontext_t *bc, dd_bool doAcquire);

void B_SetContextFallbackForDDEvents(char const *name, int (*ddResponderFunc)(ddevent_t const *));

bcontext_t *B_ContextByPos(int pos);

bcontext_t *B_ContextByName(char const *name);

int B_ContextCount();

int B_GetContextPos(bcontext_t *bc);

void B_ReorderContext(bcontext_t *bc, int pos);

void B_ClearContext(bcontext_t *bc);

void B_DestroyContext(bcontext_t *bc);

controlbinding_t *B_FindControlBinding(bcontext_t *bc, int control);

controlbinding_t *B_GetControlBinding(bcontext_t *bc, int control);

void B_DestroyControlBinding(controlbinding_t *conBin);

void B_InitControlBindingList(controlbinding_t *listRoot);

void B_DestroyControlBindingList(controlbinding_t *listRoot);

/**
 * @return  @c true if the binding was found and deleted.
 */
dd_bool B_DeleteBinding(bcontext_t *bc, int bid);

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
 * Finds the action bound to a given event.
 *
 * @param bc     Binding context to look in.
 * @param event  Event to match against.
 * @param respectHigherAssociatedContexts  Bindings shadowed by higher active contexts.
 *
 * @return Action instance (caller gets ownership), or @c nullptr if not found.
 */
de::Action *BindContext_ActionForEvent(bcontext_t *bc, ddevent_t const *event,
    bool respectHigherAssociatedContexts);

/**
 * Looks through context @a bc and looks for a binding that matches either
 * @a match1 or @a match2.
 */
dd_bool B_FindMatchingBinding(bcontext_t *bc, evbinding_t *match1, dbinding_t *match2,
    evbinding_t **evResult, dbinding_t **dResult);

void B_PrintContexts();

void B_PrintAllBindings();

void B_WriteContextToFile(bcontext_t const *bc, FILE *file);

#endif // CLIENT_INPUTSYSTEM_BINDCONTEXT_H
