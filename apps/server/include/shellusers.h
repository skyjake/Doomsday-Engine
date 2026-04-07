/** @file shellusers.h  All remote shell users.
 * @ingroup server
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#ifndef SERVER_SHELLUSERS_H
#define SERVER_SHELLUSERS_H

#include <doomsday/world/world.h>
#include "users.h"
#include "shelluser.h"

/**
 * All remote shell users.
 */
class ShellUsers : public Users, DE_OBSERVES(world::World, MapChange)
{
public:
    ShellUsers();

    void add(User *shellUser) override;
    void worldMapChanged() override;

private:
    DE_PRIVATE(d)
};

#endif  // SERVER_SHELLUSERS_H
