/** @file commandbinding.h  Input system, event => command binding.
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

#ifndef CLIENT_INPUTSYSTEM_COMMANDBINDING_H
#define CLIENT_INPUTSYSTEM_COMMANDBINDING_H

#include <de/Action>
#include "b_util.h"
#include "dd_input.h"

class BindContext;

struct CommandBinding
{
    CommandBinding *prev;     ///< Previous in list of bindings.
    CommandBinding *next;     ///< Next in list of bindings.

    int id;                   ///< Unique identifier.
    char *command;            ///< Command to execute.

    int deviceId;             ///< Which device?
    ddeventtype_t type;       ///< Type of event.
    int controlId;            ///< Which control?
    ebstate_t state;
    float pos;
    char *symbolicName;       ///< Name of a symbolic event.

    int numConds;
    statecondition_t *conds;  ///< Additional conditions.
};

/**
 * Allocates a new command binding and gives it a unique identifier.
 */
CommandBinding *B_AllocCommandBinding();

/**
 * Destroys command binding @cb.
 */
void B_DestroyCommandBinding(CommandBinding *cb);

void B_InitCommandBindingList(CommandBinding *listRoot);

void B_DestroyCommandBindingList(CommandBinding *listRoot);

/**
 * Does the opposite of the B_Parse* methods for event descriptor, including the state conditions.
 */
void CommandBinding_ToString(CommandBinding const *cb, ddstring_t *str);

/**
 * Parses a textual descriptor of the conditions for triggering an event-command binding.
 * eventparams{+cond}*
 *
 * @param cb    The results of the parsing are stored here.
 * @param desc  Descriptor containing event information and possible additional conditions.
 *
 * @return  @c true, if successful; otherwise @c false.
 */
dd_bool B_ParseEventDescriptor(CommandBinding *cb, char const *desc);

/**
 * Checks if the event matches the binding's conditions, and if so, returns an
 * action with the bound command.
 *
 * @param cb          Event binding.
 * @param event       Event to match against.
 * @param eventClass  The event has been bound in this binding class. If the
 *                    bound state is associated with a higher-priority active
 *                    class, the binding cannot be executed.
 * @param respectHigherAssociatedContexts  Bindings are shadowed by higher active contexts.
 *
 * @return  Action to be triggered, or @c nullptr. Caller gets ownership.
 */
de::Action *CommandBinding_ActionForEvent(CommandBinding *cb, ddevent_t const *event,
    BindContext const *context, bool respectHigherAssociatedContexts);

#endif // CLIENT_INPUTSYSTEM_COMMANDBINDING_H
