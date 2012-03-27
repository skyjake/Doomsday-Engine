/**
 * @file masterserver.h
 * Communication with the Master Server. @ingroup network
 *
 * The master server maintains a real-time list of running public servers.
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

void N_MasterInit(void);
void N_MasterShutdown(void);
void N_MasterAnnounceServer(boolean isOpen);
void N_MasterRequestList(void);
int N_MasterGet(int index, serverinfo_t *info);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_MASTER_SERVER_H
