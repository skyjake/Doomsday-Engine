/**
 * @file masterserver.h
 * Communication with the Master Server. @ingroup network
 *
 * The master server maintains a real-time list of running public servers.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_MASTER_SERVER_H
#define LIBDENG_MASTER_SERVER_H

#include "dd_share.h"

#ifdef __cplusplus

#include <QObject>
#include <QNetworkReply>
#include <QByteArray>

/**
 * Network request worker for communicating with the master server.
 */
class MasterWorker : public QObject
{
    Q_OBJECT

public:
    // Actions for the master worker.
    enum Action {
        NONE,
        REQUEST_SERVERS,
        ANNOUNCE
    };

public:
    MasterWorker();
    ~MasterWorker();

    void newJob(Action action, void* data);

    bool isAllDone() const;
    bool isOngoing() const;
    int serverCount() const;
    serverinfo_t server(int index) const;

protected:
    void nextJob();
    bool parseResponse(const QByteArray& response);

public slots:
    void requestFinished(QNetworkReply* reply);

private:
    struct Instance;
    Instance* d;
};

#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif

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
void N_MasterAnnounceServer(boolean isOpen);

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
int N_MasterGet(int index, serverinfo_t *info);

#ifdef __cplusplus
} // extern "C"
#endif

extern char *masterAddress;
extern int   masterPort;
extern char *masterPath;

#endif // LIBDENG_MASTER_SERVER_H
