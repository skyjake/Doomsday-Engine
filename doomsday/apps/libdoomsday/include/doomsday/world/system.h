/** @file system.h  World subsystem.
 *
 * @authors Copyright © 2014-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_WORLD_SYSTEM_H
#define LIBDOOMSDAY_WORLD_SYSTEM_H

#include "../libdoomsday.h"
#include <de/Observers>
#include <de/System>

namespace world {

/**
 * Base class for the world management subsystem.
 *
 * Singleton: there can only be one instance of the world system at a time.
 */
class LIBDOOMSDAY_PUBLIC System : public de::System
{
public:
    static System &get();

public:
    System();

    // Systems observe the passage of time.
    void timeChanged(de::Clock const &) override;

public:
    /// Notified whenever the "current" map changes.
    DENG2_DEFINE_AUDIENCE2(MapChange, void worldSystemMapChanged())

public:  /// @todo make private:
    void notifyMapChange();

private:
    DENG2_PRIVATE(d)
};

}  // namespace world

#endif  // LIBDOOMSDAY_WORLD_SYSTEM_H
