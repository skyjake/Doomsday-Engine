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

#include <de/String>
#include "b_util.h"
#include "ddevent.h"

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
};

#endif // CLIENT_INPUTSYSTEM_COMMANDBINDING_H
