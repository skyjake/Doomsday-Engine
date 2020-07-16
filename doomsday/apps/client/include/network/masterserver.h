/**
 * @file masterserver.h
 * Communication with the Master Server. @ingroup network
 *
 * The master server maintains a real-time list of running public servers.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DE_MASTER_SERVER_H
#define DE_MASTER_SERVER_H

#include "dd_share.h"

#include <de/libcore.h>
#include <de/serverinfo.h>

/**
 * Network request worker for communicating with the master server.
 * @ingroup network
 */
class MasterWorker
{
public:
    // Actions for the master worker.
    enum Action { NONE, REQUEST_SERVERS, ANNOUNCE };

public:
    MasterWorker();

    void newJob(Action action, const de::Record &data = de::Record());

    bool isAllDone() const;
    bool isOngoing() const;
    int  serverCount() const;

    de::ServerInfo server(int index) const;

private:
    DE_PRIVATE(d)
};

/**
 * Called while initializing the low-level network subsystem.
 */
void N_MasterInit(void);

/**
 * Called during engine shutdown.
 */
void N_MasterShutdown(void);

/**
 * Sends a server announcement to the master. The announcement includes our
 * IP address and other information.
 *
 * @param isOpen            If @c true, then the server will be
 *                          visible on the server list for other clients to
 *                          find by querying the server list.
 */
void N_MasterAnnounceServer(bool isOpen);

/**
 * Requests the list of open servers from the master.
 */
void N_MasterRequestList(void);

/**
 * Returns information about the server @em N.
 *
 * @return @c 0, if communication with the master is currently in progress. If
 * param info is @c NULL, will return the number of known servers ELSE, will
 * return @c not zero, if param index was valid and the master returned info on
 * the requested server.
 */
int N_MasterGet(int index, de::ServerInfo *info);

#endif // DE_MASTER_SERVER_H
