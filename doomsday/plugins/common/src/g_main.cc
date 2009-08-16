/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file g_main.cc  Public interface of the game plugin.
 */

#include "dd_export.h"
#include "g_main.h"
#include <common/GameWorld>
#include <common/GameMap>
#include <common/GameUser>

using namespace de;

BEGIN_EXTERN_C

/**
 * Constructs a new game world.
 */
DENG_EXPORT World* deng_NewWorld()
{
    return new GameWorld();
}

/**
 * Constructs a new user.
 */
DENG_EXPORT User* deng_NewUser()
{
    return new GameUser();
}

END_EXTERN_C
