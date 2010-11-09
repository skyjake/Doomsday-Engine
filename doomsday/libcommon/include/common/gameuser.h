/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_GAMEUSER_H
#define LIBCOMMON_GAMEUSER_H

#include "common.h"

#include <de/User>

/**
 * Base class for the game plugins' User.
 */
class LIBCOMMON_API GameUser : public de::User
{
public:
    GameUser();
    
    ~GameUser();
    
private:
};

#endif /* LIBCOMMON_GAMEUSER_H */
