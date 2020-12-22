/**
 * @file masterserver.h
 * Communication with the Master Server. @ingroup network
 *
 * The master server maintains a real-time list of running public servers.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include "../libdoomsday.h"
#include "dd_share.h"
#include <de/libcore.h>
#include <de/serverinfo.h>

/**
 * Network request worker for communicating with the master server.
 * @ingroup network
 */
class LIBDOOMSDAY_PUBLIC MasterWorker
{
public:
    // Actions for the master worker.
    enum Action { NONE, REQUEST_SERVERS, ANNOUNCE };

public:
    MasterWorker();

    void newJob(Action action, const de::Record &data = {});

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
LIBDOOMSDAY_PUBLIC void N_MasterInit(void);

/**
 * Called during engine shutdown.
 */
LIBDOOMSDAY_PUBLIC void N_MasterShutdown(void);

LIBDOOMSDAY_PUBLIC void N_MasterExec(MasterWorker::Action, const de::Record &data);

/**
 * Requests the list of open servers from the master.
 */
LIBDOOMSDAY_PUBLIC void N_MasterRequestList(void);

/**
 * Returns information about the server @em N.
 *
 * @return @c 0, if communication with the master is currently in progress. If
 * param info is @c NULL, will return the number of known servers ELSE, will
 * return @c not zero, if param index was valid and the master returned info on
 * the requested server.
 */
LIBDOOMSDAY_PUBLIC int N_MasterGet(int index, de::ServerInfo *info);
