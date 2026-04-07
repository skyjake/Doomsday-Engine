/** @file persistentstate.h  Persistent UI state.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBAPPFW_PERSISTENTSTATE_H
#define LIBAPPFW_PERSISTENTSTATE_H

#include "libgui.h"
#include <de/refuge.h>
#include <de/widget.h>

namespace de {

class IPersistent;

/**
 * Stores and recalls persistent state across running sessions.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC PersistentState : public Refuge
{
public:
    PersistentState(const String &name);

    PersistentState &operator << (const IPersistent &object);
    PersistentState &operator >> (IPersistent &object);
};

} // namespace de

#endif // LIBAPPFW_PERSISTENTSTATE_H
