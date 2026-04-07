/** @file masterserver.cpp Communication with the Master Server.
 * @ingroup network
 *
 * @authors Copyright © 2003-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/network/masterserver.h"

#include <de/app.h>
#include <de/config.h>
#include <de/logbuffer.h>
#include <de/webrequest.h>
#include <de/serverinfo.h>
#include <de/json.h>
#include <de/legacy/memory.h>
#include <vector>
#include <list>

using namespace de;

// Maximum time allowed time for a master server operation to take (seconds).
#define RESPONSE_TIMEOUT    15

typedef struct job_s {
    MasterWorker::Action act;
    Record data;
} job_t;

static String masterUrl(const char *suffix = nullptr)
{
    String u = App::apiUrl() + "master_server";
    if (suffix) u += suffix;
    return u;
}

DE_PIMPL(MasterWorker)
, DE_OBSERVES(WebRequest, Finished)
{
    using Jobs    = std::list<job_t>;
    using Servers = List<de::ServerInfo>;

    Action     currentAction = NONE;
    Jobs       jobs;
    Servers    servers;
    WebRequest web;

    Impl(Public *i) : Base(i)
    {
        web.setUserAgent(Version::currentBuild().userAgent());
    }

    void nextJob()
    {
        if (self().isOngoing() || self().isAllDone()) return; // Not a good time, or nothing to do.

        // Get the next job from the queue.
        job_t job = jobs.front();
        jobs.pop_front();
        currentAction = job.act;

        // Let's form an HTTP request.
        String uri = masterUrl(currentAction == REQUEST_SERVERS? "?op=list" : nullptr);

        if (currentAction == ANNOUNCE)
        {
            // Include the server info.
            const Block msg = composeJSON(job.data);

            LOGDEV_NET_VERBOSE("POST request ") << uri;
            LOGDEV_NET_VERBOSE("Request contents:\n%s") << String::fromUtf8(msg);

            web.post(uri, msg, "application/x-deng-announce");
        }
        else
        {
            LOGDEV_NET_VERBOSE("GET request ") << uri;

            web.get(uri);
        }
    }

    void webRequestFinished(WebRequest &)
    {
        LOG_AS("MasterWorker");

        if (!web.isFailed())
        {
            LOG_NET_XVERBOSE("Got reply", "");

            if (currentAction == REQUEST_SERVERS)
            {
                parseResponse(web.result());
            }
            else
            {
                const String replyText = String::fromUtf8(web.result()).strip();
                if (replyText)
                {
                    LOGDEV_NET_VERBOSE("Reply contents:\n") << replyText;
                }
            }
        }
        else
        {
            LOG_NET_WARNING(web.errorMessage());
        }

        // Continue with the next job.
        currentAction = NONE;
        nextJob();
    }

    /**
     * Attempts to parse a list of servers from the given text string.
     *
     * @param response  The string to be parsed.
     *
     * @return @c true, if successful.
     */
    bool parseResponse(const Block &response)
    {
        try
        {
            servers.clear();

            // The syntax of the response is a JSON array containing server objects.
            std::unique_ptr<Value> results(parseJSONValue(response));
            if (auto *list = maybeAs<ArrayValue>(results.get()))
            {
                for (const auto *entry : list->elements())
                {
                    try
                    {
                        if (!is<RecordValue>(entry))
                        {
                            LOG_NET_WARNING("Server information was in unexpected format");
                            continue;
                        }
                        servers.push_back(entry->as<RecordValue>().dereference());
                    }
                    catch (const Error &er)
                    {
                        LOG_NET_WARNING("Server information in master server response has "
                                        "an error: %s") << er.asText();
                    }
                }
            }
        }
        catch (const Error &er)
        {
            LOG_NET_WARNING("Failed to parse master server response: %s") << er.asText();
        }

        LOG_NET_MSG("Received %i servers from master") << self().serverCount();
        return true;
    }
};

MasterWorker::MasterWorker() : d(new Impl(this))
{
    d->web.audienceForFinished() += d;
}

void MasterWorker::newJob(Action action, const Record &data)
{
    LOG_AS("MasterWorker");

    if (masterUrl().isEmpty()) return;

    job_t job;
    job.act = action;
    job.data = data;
    d->jobs.push_back(job);

    // Let's get to it!
    d->nextJob();
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
    return d->servers.sizei();
}

ServerInfo MasterWorker::server(int index) const
{
    assert(index >= 0 && index < serverCount());
    return d->servers[index];
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

void N_MasterExec(MasterWorker::Action action, const de::Record &data)
{
    DE_ASSERT(worker);
    worker->newJob(action, data);
}

void N_MasterRequestList(void)
{
    if (worker)
    {
        worker->newJob(MasterWorker::REQUEST_SERVERS);
    }
}

int N_MasterGet(int index, ServerInfo *info)
{
    if (!worker || !worker->isAllDone())
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
            *info = ServerInfo();
            return false;
        }
    }
}
