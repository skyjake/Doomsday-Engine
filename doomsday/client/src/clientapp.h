/*
 * The Doomsday Engine Project -- dengcl
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/core.h>

#include "usersession.h"

class LocalServer;
class Video;

/**
 * @defgroup client Client
 * The client application.
 */

/**
 * The client application.
 *
 * @ingroup client
 */
class ClientApp : public de::GUIApp
{
public:
    ClientApp(int argc, char** argv);
    ~ClientApp();
    
    void iterate(const de::Time::Delta& elapsed);

    /**
     * Returns the video subsystem.
     */
    Video& video();
    
private:
    LocalServer* _localServer;
    
    /// The game session.
    UserSession* _session;

    Video* _video;
};

/// Returns the client app instance.
ClientApp& theApp();

#endif /* CLIENTAPP_H */
