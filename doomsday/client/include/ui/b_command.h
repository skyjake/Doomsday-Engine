/** @file b_command.h  Input system, event => command binding.
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

#ifndef CLIENT_INPUTSYSTEM_EVENTBINDING_H
#define CLIENT_INPUTSYSTEM_EVENTBINDING_H

#include <de/Action>
#include "b_util.h"
#include "dd_input.h"

struct bcontext_t;

typedef struct evbinding_s {
    struct evbinding_s *prev;  ///< Previous in list of bindings.
    struct evbinding_s *next;  ///< Next in list of bindings.
    int bid;                   ///< Binding identifier.
    char *command;             ///< Command to execute.

    int device;                ///< Which device?
    ddeventtype_t type;        ///< Type of event.
    int id;                    ///< Identifier.
    ebstate_t state;
    float pos;
    char *symbolicName;        ///< Name of a symbolic event.

    int numConds;
    statecondition_t *conds;   ///< Additional conditions.
} evbinding_t;

void B_InitCommandBindingList(evbinding_t *listRoot);

void B_DestroyCommandBindingList(evbinding_t *listRoot);

/**
 * Creates a new event-command binding.
 *
 * @param bindsList  List of bindings where the binding will be added.
 * @param desc       Descriptor of the event.
 * @param command    Command that will be executed by the binding.
 *
 * @return  New binding, or @c nullptr if there was an error.
 */
evbinding_t *B_NewCommandBinding(evbinding_t *listRoot, char const *desc, char const *command);

/**
 * Destroys command binding @eb.
 */
void B_DestroyCommandBinding(evbinding_t *eb);

/**
 * Does the opposite of the B_Parse* methods for event descriptor, including the
 * state conditions.
 */
void B_EventBindingToString(evbinding_t const *eb, ddstring_t *str);

/**
 * @param device  Use @c < 0 || >= NUM_INPUT_DEVICES for wildcard search.
 */
evbinding_t *B_FindCommandBinding(evbinding_t const *listRoot, char const *command, int device);

/**
 * Checks if the event matches the binding's conditions, and if so, returns an
 * action with the bound command.
 *
 * @param eb          Event binding.
 * @param event       Event to match against.
 * @param eventClass  The event has been bound in this binding class. If the
 *                    bound state is associated with a higher-priority active
 *                    class, the binding cannot be executed.
 * @param respectHigherAssociatedContexts  Bindings are shadowed by higher active contexts.
 *
 * @return  Action to be triggered, or @c nullptr. Caller gets ownership.
 */
de::Action *EventBinding_ActionForEvent(evbinding_t *eb, ddevent_t const *event,
                                        bcontext_t *eventClass, bool respectHigherAssociatedContexts);

#endif // CLIENT_INPUTSYSTEM_EVENTBINDING_H
