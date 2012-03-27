/**
 * @file masterserver.cpp
 * Communication with the Master Server. @ingroup network
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
#include <QDebug>
#include <vector>
#include <list>
#include "masterserver.h"
#include "de_platform.h"
#include "dd_main.h"
#include "con_main.h"
#include "m_misc.h"
#include "net_main.h"
#include "sv_def.h"

/*
#include "de_base.h"
#include "de_network.h"
#include "de_system.h"
#include "de_console.h"

#include "r_world.h"
*/

// Maximum time allowed time for a master server operation to take (seconds).
#define RESPONSE_TIMEOUT    15

typedef struct job_s {
    MasterWorker::Action act;
    void* data;
} job_t;

//static void Master_ClearList(void);

// Master server info. Hardcoded defaults.
char* masterAddress = const_cast<char*>("www.dengine.net"); /// @todo refactor cvars
int masterPort = 0; // Defaults to port 80.
char* masterPath = const_cast<char*>("/master.php"); /// @todo refactor cvars
boolean masterAware = false;

/*static job_t mwaQueue[MWA_MAX];
static int mwaHead;
static int mwaTail;
*/

static QString masterUrl(const char* suffix = 0)
{
    QString u = QString("http://%1:%2%3")
            .arg(masterAddress)
            .arg(masterPort? masterPort : 80)
            .arg(masterPath);
    if(suffix) u += suffix;
    return u;
}

struct MasterWorker::Instance
{
    QNetworkAccessManager* network;

    typedef std::list<job_t> Jobs;
    Jobs jobs;
    MasterWorker::Action currentAction;

    typedef std::vector<serverinfo_t> Servers;
    Servers servers;

    Instance() : network(0), currentAction(NONE) {}
};

MasterWorker::MasterWorker()
{
    d = new Instance;
    d->network = new QNetworkAccessManager(this);
    connect(d->network, SIGNAL(finished(QNetworkReply*)), this, SLOT(requestFinished(QNetworkReply*)));
}

MasterWorker::~MasterWorker()
{
    delete d->network;
}

void MasterWorker::newJob(Action action, void* data)
{
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
    req.setRawHeader("User-Agent", "Doomsday Engine " DOOMSDAY_VERSION_TEXT);

    if(d->currentAction == ANNOUNCE)
    {
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-deng-announce");

        // Include the server info.
        ddstring_t* msg = Str_NewStd();
        Sv_InfoToString((serverinfo_t*)job.data, msg);

        qDebug() << "MasterWorker: POST request" << req.url() << req.rawHeaderList() << "content:";
        qDebug() << Str_Text(msg);

        d->network->post(req, QString(Str_Text(msg)).toUtf8());
        Str_Delete(msg);
    }
    else
    {
        qDebug() << "MasterWorker: GET request" << req.url() << req.rawHeaderList();

        d->network->get(req);
    }

    // Ownership of the data was given to us, so get rid of it now.
    if(job.data) M_Free(job.data);
}

void MasterWorker::requestFinished(QNetworkReply* reply)
{
    // Make sure the reply gets deleted afterwards.
    reply->deleteLater();

    if(reply->error() != QNetworkReply::NoError)
    {
        qWarning() << "MasterWorker:" << reply->errorString();
        /// @todo Log the error.
        return;
    }

    qDebug() << "MasterWorker: Got reply.";

    if(d->currentAction == REQUEST_SERVERS)
    {
        parseResponse(reply->readAll());
    }

    d->currentAction = NONE;

    // Continue with the next job.
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
            Sv_StringToInfo(Str_Text(&line), info);
        }
    }

    qDebug() << "MasterWorker: Parsed" << serverCount() << "servers.";

    Str_Free(&line);
    Str_Free(&msg);
    return true;
}

static MasterWorker* worker;

#if 0
// A linked list of servers created from info retrieved from the master.
static serverlist_t *servers;
static int numServers;

/**
 * Creates a new server and links it into our copy of the server list.
 *
 * @return  Ptr to the newly created server.
 */
static serverlist_t *Master_NewServer(void)
{
    serverlist_t *node;

    //Sys_Lock(serversMutex);
    numServers++;
    node = M_Calloc(sizeof(serverlist_t));
    node->next = servers;
    servers = node;
    //Sys_Unlock(serversMutex);
    return node;
}

/**
 * Clears our copy of the server list returned by the master.
 */
static void Master_ClearList(void)
{
    //Sys_Lock(serversMutex);
    numServers = 0;
    while(servers)
    {
        serverlist_t *next = servers->next;
        M_Free(servers);
        servers = next;
    }
    //Sys_Unlock(serversMutex);
}

/**
 * Adds a new worker action to the queue.
 *
 * @param act   Action.
 * @param data  Data for the action. Ownership transferred to the worker.
 */
static void MasterWorker_Do(workeraction_t act, void* data)
{
    int nextPos = 0;

    //Sys_Lock(mwaMutex);

    nextPos = (mwaHead + 1) % MWA_MAX;
    if(nextPos == mwaTail)
    {
        // It's full!
        Con_Message("MasterWorker_Add: Queue is full!\n");
    }
    else
    {
        mwaQueue[mwaHead].act = act;
        mwaQueue[mwaHead].data = data;
        mwaHead = nextPos;
    }
    //Sys_Unlock(mwaMutex);

    // Wake up the worker.
    //Sem_V(semPending);

}

static boolean MasterWorker_IsDone(void)
{
    boolean done;
    //Sys_Lock(mwaMutex);
    done = (mwaHead == mwaTail);
    //Sys_Unlock(mwaMutex);
    return done;
}

static boolean MasterWorker_Get(job_t* job)
{
    boolean success = false;

    // The default action.
    job->act = MWA_EXIT;
    job->data = 0;

    //Sys_Lock(mwaMutex);
    if(mwaHead != mwaTail)
    {
        success = true;
        job->act = mwaQueue[mwaTail].act;

        // Relinquish ownership of the associated data.
        job->data = mwaQueue[mwaTail].data;
        mwaQueue[mwaTail].data = 0;
    }
    //Sys_Unlock(mwaMutex);
    return success;
}

static void MasterWorker_Discard(void)
{
    //Sys_Lock(mwaMutex);
    assert(mwaHead != mwaTail);
    assert(mwaQueue[mwaTail].data == NULL);
    mwaTail = (mwaTail + 1) % MWA_MAX;
    //Sys_Unlock(mwaMutex);
}

static void MasterWorker_Clear(void)
{
    int i;

    Sys_Lock(mwaMutex);
    for(i = 0; i < MWA_MAX; ++i)
    {
        mwaQueue[i].act = MWA_EXIT;
        if(mwaQueue[i].data)
        {
            M_Free(mwaQueue[i].data);
            mwaQueue[i].data = 0;
        }
    }
    Sys_Unlock(mwaMutex);
}
#endif

#if 0
/**
 * Callback function that sends outgoing data with libcurl.
 */
static size_t C_DECL MasterWorker_ReadCallback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    ddstring_t *msg = stream;
    size_t bytes = size * nmemb;

    // Don't copy too much.
    bytes = MIN_OF(bytes, (unsigned)Str_Length(msg));
    memcpy(ptr, msg->str, bytes);

    // Remove the sent portion from the buffer.
    Str_Set(msg, msg->str + bytes);

    // Number of bytes written to the buffer.
    return bytes;
}

/**
 * Callback function that receives incoming data from libcurl.
 */
static size_t C_DECL MasterWorker_WriteCallback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    ddstring_t* response = stream;
    size_t bytes = size * nmemb;

    // Append the new data to the response.
    Str_PartAppend(response, ptr, 0, bytes);

    return bytes;
}
#endif

#if 0
static int C_DECL MasterWorker_Thread(void* param)
{
    ddstring_t msg;
    QString masterUrl;
    //char errorBuf[CURL_ERROR_SIZE];
    //CURL* session;

    Str_InitStd(&msg);

    // Prepare the curl session for our HTTP requests.
    //session = curl_easy_init();

    while(true)
    {
        job_t job;

        Sem_P(semPending);

        // Get the next action.
        MasterWorker_Get(&job);

        if(job.act == MWA_EXIT)
        {
            // Time to stop the thread.
            break;
        }
        else if(job.act == MWA_REQUEST)
        {
            assert(!job.data);

            masterUrl = MasterWorker_GetUrl() + "?list";
/*
#ifdef _DEBUG
            curl_easy_setopt(session, CURLOPT_VERBOSE, true);
#endif
            curl_easy_setopt(session, CURLOPT_HEADER, false);
            curl_easy_setopt(session, CURLOPT_WRITEFUNCTION, MasterWorker_WriteCallback);
            curl_easy_setopt(session, CURLOPT_WRITEDATA, &msg);
            curl_easy_setopt(session, CURLOPT_URL, masterUrl);
            curl_easy_setopt(session, CURLOPT_TIMEOUT, RESPONSE_TIMEOUT);
            curl_easy_setopt(session, CURLOPT_ERRORBUFFER, errorBuf);
            curl_easy_setopt(session, CURLOPT_DNS_CACHE_TIMEOUT, -1); // never clear hostcache

            // Perform the operation.
            if(!curl_easy_perform(session))
            {
#ifdef _DEBUG
                printf("%s", Str_Text(&msg));
#endif
                // Let's parse the message.
                MasterWorker_ParseResponse(&msg);
            }
            else
            {
                fprintf(outFile, "N_MasterSendRequest: %s\n", errorBuf);
            }
            */

            // We're done with the parsing.
            Str_Free(&msg);
        }
        else if(job.act == MWA_ANNOUNCE)
        {
            // Post a server announcement.
            //struct curl_slist* headers = 0;
            //headers = curl_slist_append(0, "Content-Type: application/x-deng-announce");

            masterUrl = MasterWorker_GetUrl();

            // Convert the serverinfo into plain text.
            Sv_InfoToString((serverinfo_t*)job.data, &msg);

            // The info can be freed now.
            M_Free(job.data);
            job.data = 0;

            /*
            // Setup the request.
#ifdef _DEBUG
            curl_easy_setopt(session, CURLOPT_VERBOSE, true);
#endif
            curl_easy_setopt(session, CURLOPT_POST, true);
            curl_easy_setopt(session, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(session, CURLOPT_HEADER, false);
            curl_easy_setopt(session, CURLOPT_READFUNCTION, MasterWorker_ReadCallback);
            curl_easy_setopt(session, CURLOPT_READDATA, &msg);
            curl_easy_setopt(session, CURLOPT_POSTFIELDSIZE, Str_Length(&msg));
            curl_easy_setopt(session, CURLOPT_URL, masterUrl);
            curl_easy_setopt(session, CURLOPT_TIMEOUT, RESPONSE_TIMEOUT);
            curl_easy_setopt(session, CURLOPT_ERRORBUFFER, errorBuf);
            curl_easy_setopt(session, CURLOPT_DNS_CACHE_TIMEOUT, -1); // never clear hostcache

            if(curl_easy_perform(session))
            {
                // Failure.
                fprintf(outFile, "N_MasterSendAnnouncement: %s\n", errorBuf);
            }

            // Cleanup.
            Str_Free(&msg);
            curl_slist_free_all(headers);
            */
        }

        // The job is done!
        MasterWorker_Discard();
    }

    /*
    // Cleanup the curl session.
    curl_easy_cleanup(session);
    session = NULL;
*/
    return 0;
}
#endif

/*
static void MasterWorker_Init(void)
{
    semPending = Sem_Create(0);
    mwaMutex = Sys_CreateMutex("mwaQueue");

    // Clear the queue.
    memset(mwaQueue, 0, sizeof(mwaQueue));
    mwaHead = mwaTail = 0;

    VERBOSE( Con_Message("MasterWorker_Init: Starting worker thread...\n") );

    // Start the worker thread.
    worker = Sys_StartThread(MasterWorker_Thread, NULL);
}
    */

/*
static void MasterWorker_Shutdown(void)
{
    MasterWorker_Clear();

    // Wake up the worker.
    Sem_V(semPending);

    VERBOSE( Con_Message("MasterWorker_Shutdown: Waiting for thread to stop...\n") );
    Sys_WaitThread(worker);

    Sys_DestroyMutex(mwaMutex);
    Sem_Destroy(semPending);
    mwaMutex = 0;
    semPending = 0;
}
    */

/**
 * Called by N_Init() while initializing the low-level network subsystem.
 */
void N_MasterInit(void)
{
    assert(worker == 0);
    worker = new MasterWorker;
}

/**
 * Called by N_Shutdown() during engine shutdown.
 */
void N_MasterShutdown(void)
{
    assert(worker != 0);
    delete worker;
    worker = 0;
}

/**
 * Sends a server announcement to the master. The announcement includes our
 * IP address and other information.
 *
 * @param isOpen            If @c true, then the server will be
 *                          visible on the server list for other clients to
 *                          find by querying the server list.
 */
void N_MasterAnnounceServer(boolean isOpen)
{
    // Must be a server.
   if(isClient) return;

    DEBUG_Message(("N_MasterAnnounceServer: Announcing as open=%i.\n", isOpen));

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

    //MasterWorker_Do(MWA_ANNOUNCE, info);
}

/**
 * Requests the list of open servers from the master.
 */
void N_MasterRequestList(void)
{
    //MasterWorker_Do(MWA_REQUEST, 0);

    assert(worker);
    worker->newJob(MasterWorker::REQUEST_SERVERS, 0);
}

/**
 * Returns information about the server #N.
 *
 * @return @c 0, if communication with the master is currently in progress. If
 * param info is @c NULL, will return the number of known servers ELSE, will
 * return @c not zero, if param index was valid and the master returned info on
 * the requested server.
 */
int N_MasterGet(int index, serverinfo_t *info)
{
    assert(worker);

    if(!worker->isAllDone())
    {
        // Not done yet.
        return -1;
    }

    //Sys_Lock(serversMutex);
    if(!info)
    {
        return worker->serverCount();
    }
    else
    {
        /*
        serverlist_t *it;

        // Find the index'th entry.
        for(it = servers; index > 0 && it; index--, it = it->next);

        // Was the index valid?
        if(!it)
        {
            memset(info, 0, sizeof(*info));
            result = false;
        }
        else
        {
            // Make a copy of the info.
            memcpy(info, &it->info, sizeof(*info));
            result = true;
        }
        */

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
    //Sys_Unlock(serversMutex);
    //return result;
}
