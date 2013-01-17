/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * b_command.h: Event-Command Bindings
 */

#ifndef __DOOMSDAY_BIND_COMMAND_H__
#define __DOOMSDAY_BIND_COMMAND_H__

#include "b_util.h"

typedef struct evbinding_s {
    struct evbinding_s* prev;       // Previous in list of bindings.
    struct evbinding_s* next;       // Next in list of bindings.
    int         bid;                // Binding identifier.
    char*       command;            // Command to execute.

    uint        device;             // Which device?
    ddeventtype_t type;             // Type of event.
    int         id;                 // Identifier.
    ebstate_t   state;
    float       pos;
    char*       symbolicName;       // Name of a symbolic event.

    // Additional conditions.
    int         numConds;
    statecondition_t* conds;
} evbinding_t;

void         B_InitCommandBindingList(evbinding_t* listRoot);
void         B_DestroyCommandBindingList(evbinding_t* listRoot);
evbinding_t* B_NewCommandBinding(evbinding_t* listRoot, const char* desc, const char* command);
void         B_DestroyCommandBinding(evbinding_t* eb);
void         B_EventBindingToString(const evbinding_t* eb, ddstring_t* str);

/**
 * Checks if the event matches the binding's conditions, and if so, executes the
 * bound command.
 *
 * @param eb          Event binding.
 * @param event       Event to match against.
 * @param eventClass  The event has been bound in this binding class. If the
 *                    bound state is associated with a higher-priority active
 *                    class, the binding cannot be executed.
 *
 * @return  @c true, if the bound command was executed. @c false otherwise, as the
 *          event didn't match all the conditions.
 */
boolean B_TryCommandBinding(evbinding_t* eb, ddevent_t* event, struct bcontext_s* eventClass);

#endif // __DOOMSDAY_BIND_COMMAND_H__
