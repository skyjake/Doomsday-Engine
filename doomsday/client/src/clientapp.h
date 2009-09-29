/*
 * The Doomsday Engine Project -- dengcl
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

#ifndef CLIENTAPP_H
#define CLIENTAPP_H

#include "usersession.h"
#include <de/core.h>

class LocalServer;

/**
 * @defgroup client Client
 * The client application.
 */

/**
 * The client application.
 *
 * @ingroup client
 */
class ClientApp : public de::App
{
public:
    ClientApp(const de::CommandLine& arguments);
    ~ClientApp();
    
    void iterate(const de::Time::Delta& elapsed);
    
private:
    LocalServer* _localServer;
    
    /// The game session.
    UserSession* _session;
};

#endif /* CLIENTAPP_H */
