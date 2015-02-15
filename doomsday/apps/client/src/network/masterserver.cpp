/** @file masterserver.cpp Communication with the Master Server. 
 * @ingroup network
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <QNetworkAccessManager>
#include <de/Log>
#include <de/memory.h>
#include "de_platform.h"
#include <vector>
#include <list>
#include "network/masterserver.h"
#include "network/net_main.h"
#include "network/protocol.h"
#ifdef __SERVER__
#  include "server/sv_def.h"
#endif
#include "dd_main.h"
#include "m_misc.h"

#include <de/App>

using namespace de;

// Maximum time allowed time for a master server operation to take (seconds).
#define RESPONSE_TIMEOUT    15

typedef struct job_s {
    MasterWorker::Action act;
    void* data;
} job_t;

static String const DEFAULT_API_URL = "www.dengine.net/master.php";

dd_bool masterAware = false; // cvar

static QString masterUrl(const char* suffix = 0)
{
    String u = App::config().gets("masterServer.apiUrl", DEFAULT_API_URL);
    if(!u.startsWith("http")) u = "http://" + u;
    if(suffix) u += suffix;
    return u;
}

DENG2_PIMPL_NOREF(MasterWorker)
{
    QNetworkAccessManager* network;

    typedef std::list<job_t> Jobs;
    Jobs jobs;
    MasterWorker::Action currentAction;

    typedef std::vector<serverinfo_t> Servers;
    Servers servers;

    Instance() : network(0), currentAction(NONE) {}

    ~Instance()
    {
        delete network;
    }
};

MasterWorker::MasterWorker() : d(new Instance)
{
    d->network = new QNetworkAccessManager(this);
    connect(d->network, SIGNAL(finished(QNetworkReply*)), this, SLOT(requestFinished(QNetworkReply*)));
}

void MasterWorker::newJob(Action action, void* data)
{
    LOG_AS("MasterWorker");

    job_t job;
    job.act = action;
    job.data = data;
    d->jobs.push_back(job);

    // Let's get to it!
    nextJob();
}

bool MasterWorker::isAllDone() const
{
    return d->jobs.empty() && !isOngoing();
}

bool MasterWorker::isOngoing() const
{
    return d->currentAction != NONE;
}

int MasterWorker::serverCount() const
{
    return (int) d->servers.size();
}

serverinfo_t MasterWorker::server(int index) const
{
    assert(index >= 0 && index < serverCount());
    return d->servers[index];
}

void MasterWorker::nextJob()
{
    if(isOngoing() || isAllDone()) return; // Not a good time, or nothing to do.

    // Get the next job from the queue.
    job_t job = d->jobs.front();
    d->jobs.pop_front();
    d->currentAction = job.act;

    // Let's form an HTTP request.
    QNetworkRequest req(masterUrl(d->currentAction == REQUEST_SERVERS? "?list" : 0));
    req.setRawHeader("User-Agent", Net_UserAgent().toLatin1());

#ifdef __SERVER__
    if(d->currentAction == ANNOUNCE)
    {
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-deng-announce");

        // Include the server info.
        ddstring_t* msg = Str_NewStd();
        Sv_InfoToString((serverinfo_t*)job.data, msg);

        LOGDEV_NET_VERBOSE("POST request ") << req.url().toString();
        foreach(const QByteArray& hdr, req.rawHeaderList())
        {
            LOGDEV_NET_VERBOSE("%s: %s") << QString(hdr) << QString(req.rawHeader(hdr));
        }
        LOGDEV_NET_VERBOSE("Request contents:\n%s") << Str_Text(msg);

        d->network->post(req, QString(Str_Text(msg)).toUtf8());
        Str_Delete(msg);
    }
    else
#endif
    {
        LOGDEV_NET_VERBOSE("GET request ") << req.url().toString();
        foreach(const QByteArray& hdr, req.rawHeaderList())
        {
            LOGDEV_NET_VERBOSE("%s: %s") << QString(hdr) << QString(req.rawHeader(hdr));
        }

        d->network->get(req);
    }

    // Ownership of the data was given to us, so get rid of it now.
    if(job.data) M_Free(job.data);
}

void MasterWorker::requestFinished(QNetworkReply* reply)
{
    LOG_AS("MasterWorker");

    // Make sure the reply gets deleted afterwards.
    reply->deleteLater();

    if(reply->error() == QNetworkReply::NoError)
    {
        LOG_NET_XVERBOSE("Got reply");

        if(d->currentAction == REQUEST_SERVERS)
        {
            parseResponse(reply->readAll());
        }
    }
    else
    {
        LOG_NET_WARNING(reply->errorString());
    }

    // Continue with the next job.
    d->currentAction = NONE;
    nextJob();
}

/**
 * Attempts to parse a list of servers from the given text string.
 *
 * @param response  The string to be parsed.
 *
 * @return @c true, if successful.
 */
bool MasterWorker::parseResponse(const QByteArray& response)
{
    ddstring_t msg;
    ddstring_t line;
    serverinfo_t* info = NULL;

    Str_InitStd(&msg);
    Str_PartAppend(&msg, response.constData(), 0, response.size());

    Str_InitStd(&line);

    d->servers.clear();

    // The syntax of the response is simple:
    // label:value
    // One or more empty lines separate servers.

    const char* pos = Str_Text(&msg);
    while(*pos)
    {
        pos = Str_GetLine(&line, pos);

        if(Str_Length(&line) && !info)
        {
            // A new server begins.
            d->servers.push_back(serverinfo_t());
            info = &d->servers.back();
            memset(info, 0, sizeof(*info));
        }
        else if(!Str_Length(&line) && info)
        {
            // No more current server.
            info = NULL;
        }

        if(info)
        {
            ServerInfo_FromString(info, Str_Text(&line));
        }
    }

    LOG_NET_MSG("Received %i servers from master") << serverCount();

    Str_Free(&line);
    Str_Free(&msg);
    return true;
}

static MasterWorker* worker;

void N_MasterInit(void)
{
    assert(worker == 0);
    worker = new MasterWorker;
}

void N_MasterShutdown(void)
{
    if(!worker) return;

    delete worker;
    worker = 0;
}

void N_MasterAnnounceServer(dd_bool isOpen)
{
#ifdef __SERVER__
    // Must be a server.
    if(isClient) return;

    LOG_AS("N_MasterAnnounceServer");

    if(isOpen && !strlen(netPassword))
    {
        LOG_NET_WARNING("Cannot announce server as public: no shell password set! "
                        "You must set one with the 'server-password' cvar.");
        return;
    }

    LOG_NET_MSG("Announcing server (open:%b)") << isOpen;

    // This will be freed by the worker after the request has been made.
    serverinfo_t *info = (serverinfo_t*) M_Calloc(sizeof(*info));

    // Let's figure out what we want to tell about ourselves.
    Sv_GetInfo(info);
    if(!isOpen)
    {
        info->canJoin = false;
    }

    assert(worker);
    worker->newJob(MasterWorker::ANNOUNCE, info);
#else
    DENG_UNUSED(isOpen);
#endif
}

void N_MasterRequestList(void)
{
    assert(worker);
    worker->newJob(MasterWorker::REQUEST_SERVERS, 0);
}

int N_MasterGet(int index, serverinfo_t *info)
{
    assert(worker);

    if(!worker->isAllDone())
    {
        // Not done yet.
        return -1;
    }

    if(!info)
    {
        return worker->serverCount();
    }
    else
    {
        if(index >= 0 && index < worker->serverCount())
        {
            *info = worker->server(index);
            return true;
        }
        else
        {
            memset(info, 0, sizeof(*info));
            return false;
        }
    }
}
