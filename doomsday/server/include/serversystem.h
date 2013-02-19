/** @file serversystem.h  Subsystem for tending to clients.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef SERVERSYSTEM_H
#define SERVERSYSTEM_H

#include <de/libdeng2.h>
#include <de/System>
#include <de/Id>
#include <de/Error>
#include "remoteuser.h"
#include "dd_types.h"

#include <QObject>

/**
 * Subsystem for tending to clients.
 * @ingroup server
 *
 * @todo The entire concept of "nodes" should be retired along with
 * LegacyNetwork. Instead:
 * - Immediately after connecting to a server the socket is put into the
 *   set of connected sockets.
 * - Sockets may request upgrade to Shell user, in which case ownership
 *   of the socket is given to a ShellUser instance.
 * - Sockets may join the game, becoming clients.
 * - Silent sockets that hang around too long will be automatically
 *   terminated if haven't joined the game.
 */
class ServerSystem : public QObject, public de::System
{   
    Q_OBJECT

public:
    /// An error related to identifiers (e.g., invalid ID specified). @ingroup errors
    DENG2_ERROR(IdError);

public:
    ServerSystem();

    ~ServerSystem();

    /**
     * Start listening for incoming connections.
     *
     * @param port  TCP port to listen on.
     */
    void start(de::duint16 port);

    void stop();

    bool isListening() const;

    /**
     * The client is removed from the game immediately. This is used when the
     * server needs to terminate a client's connection abnormally.
     */
    void terminateNode(de::Id const &id);

    RemoteUser &user(de::Id const &id) const;

    /**
     * A network node wishes to become a real client.
     * @return @c true if we allow this.
     */
    bool isUserAllowedToJoin(RemoteUser &user) const;

    void convertToShellUser(RemoteUser *user);

    /**
     * Prints the status of the server into the log.
     */
    void printStatus();

    void timeChanged(de::Clock const &);

protected slots:
    void handleIncomingConnection();
    void userDestroyed(QObject *);

private:
    DENG2_PRIVATE(d)
};

ServerSystem &App_ServerSystem();

void    Server_Register(); // old-fashioned cvars
boolean N_ServerOpen(void);
boolean N_ServerClose(void);
void    N_PrintNetworkStatus(void);

extern int nptIPPort; // cvar

#endif // SERVERSYSTEM_H
