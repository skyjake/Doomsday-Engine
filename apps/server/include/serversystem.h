/** @file serversystem.h  Subsystem for tending to clients.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/libcore.h>
#include <de/system.h>
#include <de/id.h>
#include <de/error.h>
#include "remoteuser.h"
#include "dd_types.h"

#define DEFAULT_TCP_PORT    13209
#define DEFAULT_UDP_PORT    13209

/**
 * Subsystem for tending to clients.
 * @ingroup server
 *
 * - Immediately after connecting to a server the socket is put into the
 *   set of remote users (RemoteUser class). One RemoteUser instance is
 *   responsible for each connected socket.
 * - Remote users may request upgrade to a Shell user, in which case ownership
 *   of the socket is given to a ShellUser instance.
 * - Remote users may join the game, becoming players in the game.
 * - Silent remote users that hang around too long will be automatically
 *   terminated if haven't joined the game.
 *
 * @todo This is a work in progress, as all remnants of the old network code
 * have not been removed/revised.
 */
class ServerSystem : public de::System
{
public:
    /// An error related to identifiers (e.g., invalid ID specified). @ingroup errors
    DE_ERROR(IdError);

public:
    ServerSystem();

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
    void terminateNode(const de::Id &id);

    RemoteUser &user(const de::Id &id) const;

    /**
     * A network node wishes to become a real client.
     * @return @c true if we allow this.
     */
    bool isUserAllowedToJoin(RemoteUser &user) const;

    void convertToShellUser(RemoteUser *user);

    void convertToRemoteFeedUser(RemoteUser *user);

    /**
     * Returns the total number of connected users (of all types).
     */
    int userCount() const;

    /**
     * Prints the status of the server into the log.
     */
    void printStatus();

    void timeChanged(const de::Clock &);

protected:
    void handleIncomingConnection();
    void userDestroyed(RemoteUser *);

private:
    DE_PRIVATE(d)
};

ServerSystem &App_ServerSystem();

void    Server_Register(); // old-fashioned cvars
dd_bool N_ServerOpen(void);
dd_bool N_ServerClose(void);
void    N_PrintNetworkStatus(void);

/**
 * Sends a server announcement to the master. The announcement includes our
 * IP address and other information.
 *
 * @param isOpen            If @c true, then the server will be
 *                          visible on the server list for other clients to
 *                          find by querying the server list.
 */
void N_MasterAnnounceServer(bool isOpen);

extern dd_bool  serverPublic;
extern char    *serverName, *serverInfo;
extern char *   nptIPAddress; // cvar
extern int      nptIPPort;    // cvar

#endif // SERVERSYSTEM_H
