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
#include "network/net_buf.h" // nodeid_t

/**
 * Subsystem for tending to clients.
 * @ingroup server
 */
class ServerSystem : public de::System
{
public:
    ServerSystem();
    ~ServerSystem();

    /**
     * Start listening for client connections.
     *
     * @param port  TCP port to listen on.
     */
    void start(de::duint16 port);

    void stop();

    bool isRunning() const;

    /**
     * The client is removed from the game immediately. This is used when the
     * server needs to terminate a client's connection abnormally.
     */
    void terminateNode(nodeid_t id);

    /**
     * Returns the player name associated with the given network node.
     */
    de::String nodeName(nodeid_t node) const;

    int nodeLegacySocket(nodeid_t node) const;

    bool hasNodeJoined(nodeid_t node) const;

    /**
     * Prints the status of the server into the log.
     */
    void printStatus();

    void timeChanged(de::Clock const &);

private:
    DENG2_PRIVATE(d)
};

ServerSystem &App_ServerSystem();

void    Server_Register(); // old-fashioned cvars
boolean N_ServerOpen(void);
boolean N_ServerClose(void);
void    N_TerminateNode(nodeid_t id);
int     N_GetNodeSocket(nodeid_t id);
boolean N_HasNodeJoined(nodeid_t id);
void    N_PrintNetworkStatus(void);

extern int nptIPPort; // cvar

#endif // SERVERSYSTEM_H
