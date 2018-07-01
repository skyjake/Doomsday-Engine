/** @file masterserver.cpp Communication with the Master Server.
 * @ingroup network
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

//#include <QNetworkAccessManager>
#include <de/Config>
#include <de/LogBuffer>
#include <de/shell/ServerInfo>
#include <de/data/json.h>
#include <de/memory.h>
#include "de_platform.h"
#include "network/masterserver.h"
#include "network/net_main.h"
#include "network/protocol.h"
#ifdef __SERVER__
#  include "serverapp.h"
#  include "server/sv_def.h"
#endif
#include "dd_main.h"

#include <de/App>
#include <vector>
#include <list>

using namespace de;

// Maximum time allowed time for a master server operation to take (seconds).
#define RESPONSE_TIMEOUT    15

typedef struct job_s {
    MasterWorker::Action act;
    Record data;
} job_t;

dd_bool serverPublic = false; // cvar

static String masterUrl(char const *suffix = 0)
{
    String u = App::apiUrl() + "master_server";
    if (suffix) u += suffix;
    return std::move(u);
}

DE_PIMPL_NOREF(MasterWorker)
{
//    QNetworkAccessManager *network;

    typedef std::list<job_t> Jobs;
    Jobs jobs;
    MasterWorker::Action currentAction;

    typedef List<de::shell::ServerInfo> Servers;
    Servers servers;

    Impl() : network(0), currentAction(NONE) {}

    ~Impl()
    {
        delete network;
    }
};

MasterWorker::MasterWorker() : d(new Impl)
{
    d->network = new QNetworkAccessManager(this);
    connect(d->network, SIGNAL(finished(QNetworkReply*)), this, SLOT(requestFinished(QNetworkReply*)));
}

void MasterWorker::newJob(Action action, Record const &data)
{
    LOG_AS("MasterWorker");

    if (masterUrl().isEmpty()) return;

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
    return d->servers.size();
}

shell::ServerInfo MasterWorker::server(int index) const
{
    assert(index >= 0 && index < serverCount());
    return d->servers[index];
}

void MasterWorker::nextJob()
{
    if (isOngoing() || isAllDone()) return; // Not a good time, or nothing to do.

    // Get the next job from the queue.
    job_t job = d->jobs.front();
    d->jobs.pop_front();
    d->currentAction = job.act;

    // Let's form an HTTP request.
    QNetworkRequest req(masterUrl(d->currentAction == REQUEST_SERVERS? "?op=list" : 0));
    req.setRawHeader("User-Agent", Net_UserAgent().toLatin1());

#ifdef __SERVER__
    if (d->currentAction == ANNOUNCE)
    {
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-deng-announce");

        // Include the server info.
        Block const msg = composeJSON(job.data);

        LOGDEV_NET_VERBOSE("POST request ") << req.url().toString();
        for (QByteArray const &hdr : req.rawHeaderList())
        {
            LOGDEV_NET_VERBOSE("%s: %s") << String(hdr) << String(req.rawHeader(hdr));
        }
        LOGDEV_NET_VERBOSE("Request contents:\n%s") << msg.constData();

        d->network->post(req, msg);
    }
    else
#endif
    {
        LOGDEV_NET_VERBOSE("GET request ") << req.url().toString();
        for (QByteArray const &hdr : req.rawHeaderList())
        {
            LOGDEV_NET_VERBOSE("%s: %s") << String(hdr) << String(req.rawHeader(hdr));
        }

        d->network->get(req);
    }
}

void MasterWorker::requestFinished(QNetworkReply* reply)
{
    LOG_AS("MasterWorker");

    // Make sure the reply gets deleted afterwards.
    reply->deleteLater();

    if (reply->error() == QNetworkReply::NoError)
    {
        LOG_NET_XVERBOSE("Got reply", "");

        if (d->currentAction == REQUEST_SERVERS)
        {
            parseResponse(reply->readAll());
        }
        else
        {
            String replyText = String::fromUtf8(reply->readAll()).strip();
            if (!replyText.isEmpty())
            {
                LOGDEV_NET_VERBOSE("Reply contents:\n") << replyText;
            }
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
bool MasterWorker::parseResponse(QByteArray const &response)
{
    try
    {
        d->servers.clear();

        // The syntax of the response is a JSON array containing server objects.
        for (QVariant entry : parseJSON(String::fromUtf8(response)).toList())
        {
            try
            {
                std::unique_ptr<Value> entryValue(Value::constructFrom(entry));
                if (!is<RecordValue>(*entryValue))
                {
                    LOG_NET_WARNING("Server information was in unexpected format");
                    continue;
                }
                d->servers.append(*entryValue->as<RecordValue>().record());
            }
            catch (Error const &er)
            {
                LOG_NET_WARNING("Server information in master server response has "
                                "an error: %s") << er.asText();
            }
        }
    }
    catch (Error const &er)
    {
        LOG_NET_WARNING("Failed to parse master server response: %s") << er.asText();
    }

    LOG_NET_MSG("Received %i servers from master") << serverCount();
    return true;
}

static MasterWorker *worker;

void N_MasterInit(void)
{
    assert(worker == 0);
    worker = new MasterWorker;
}

void N_MasterShutdown(void)
{
    if (!worker) return;

    delete worker;
    worker = 0;
}

void N_MasterAnnounceServer(bool isOpen)
{
#ifdef __SERVER__
    // Must be a server.
    if (isClient) return;

    LOG_AS("N_MasterAnnounceServer");

    if (isOpen && !strlen(netPassword))
    {
        LOG_NET_WARNING("Cannot announce server as public: no shell password set! "
                        "You must set one with the 'server-password' cvar.");
        return;
    }

    LOG_NET_MSG("Announcing server (open:%b)") << isOpen;

    // Let's figure out what we want to tell about ourselves.
    shell::ServerInfo info = ServerApp::currentServerInfo();
    if (!isOpen)
    {
        info.setFlags(info.flags() & ~shell::ServerInfo::AllowJoin);
    }

    DE_ASSERT(worker);
    worker->newJob(MasterWorker::ANNOUNCE, info.asRecord());
#else
    DE_UNUSED(isOpen);
#endif
}

void N_MasterRequestList(void)
{
    DE_ASSERT(worker);
    worker->newJob(MasterWorker::REQUEST_SERVERS);
}

int N_MasterGet(int index, shell::ServerInfo *info)
{
    DE_ASSERT(worker);

    if (!worker->isAllDone())
    {
        // Not done yet.
        return -1;
    }

    if (!info)
    {
        return worker->serverCount();
    }
    else
    {
        if (index >= 0 && index < worker->serverCount())
        {
            *info = worker->server(index);
            return true;
        }
        else
        {
            *info = shell::ServerInfo();
            return false;
        }
    }
}
