/** @file commandbinding.h  Command binding record accessor.
 *
 * @authors Copyright © 2009-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/String>
#include "Binding"
#include "ddevent.h"

class BindContext;

/**
 * Utility for handling event => command binding records.
 *
 * @ingroup ui
 */
class CommandBinding : public Binding
{
public:
    CommandBinding()                            : Binding() {}
    CommandBinding(CommandBinding const &other) : Binding(other) {}
    CommandBinding(de::Record &d)               : Binding(d) {}
    CommandBinding(de::Record const &d)         : Binding(d) {}

    CommandBinding &operator = (de::Record const *d) {
        *static_cast<Binding *>(this) = d;
        return *this;
    }

    void resetToDefaults();

    de::String composeDescriptor();

    /**
     * Evaluate the given @a event according to the binding configuration, and if all
     * binding conditions pass - attempt to generate an Action.
     *
     * @param event                  Event to match against.
     * @param context                Context in which the binding exists.
     * @param respectHigherContexts  Bindings are shadowed by higher active contexts.
     *
     * @return Action instance (caller gets ownership), or @c nullptr if no matching.
     */
    de::Action *makeAction(ddevent_t const &event, BindContext const &context,
                           bool respectHigherContexts) const;
};

#endif // CLIENT_INPUTSYSTEM_COMMANDBINDING_H
