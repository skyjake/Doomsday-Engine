/** @file bank.cpp  Abstract data bank with multi-tiered caching.
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

#include "de/Bank"
#include "de/Folder"
#include "de/App"
#include "de/FS"
#include "de/PathTree"
#include "de/WaitableFIFO"
#include "de/Log"
#include "de/Writer"
#include "de/Reader"

#include <QThread>
#include <QTimer>
#include <QList>

namespace de {

static int const NUM_CACHE_LEVELS = 3;
static PathTree::ComparisonFlags const FIND_ITEM = PathTree::MatchFull | PathTree::NoBranch;

DENG2_PIMPL(Bank)
{
    /**
     * Hot storage contains data items serialized into files. The goal is to
     * allow quick recovery of data into memory. May be disabled.
     */
    struct HotStorage : public std::auto_ptr<Folder>, public Lockable
    {
        Bank &bank;
        dint64 totalSize;

        HotStorage(Bank &i, Folder *folder = 0)
            : auto_ptr<Folder>(folder), bank(i), totalSize(0)
        {}

        operator bool() const { return get() != 0; }

        void setLocation(String const &location)
        {
            DENG2_GUARD(this);

            if(!location.isEmpty() && !bank.d->flags.testFlag(DisableHotStorage))
            {
                // Serialized "hot" data is kept here.
                reset(&App::fileSystem().makeFolder(location));
            }
            else
            {
                reset();
            }

            updateTotal();
        }

        void clear()
        {
            if(!get()) return;

            DENG2_GUARD(this);

            PathTree::FoundPaths paths;
            bank.d->items.findAllPaths(paths, PathTree::NoBranch);

            DENG2_FOR_EACH(PathTree::FoundPaths, i, paths)
            {
                get()->removeFile(*i);
            }

            totalSize = 0;
        }

        void updateTotal()
        {
            DENG2_GUARD(this);

            totalSize = 0;
            if(!get() || bank.d->items.isEmpty()) return;

            PathTree::FoundPaths paths;
            bank.d->items.findAllPaths(paths, PathTree::NoBranch);

            DENG2_FOR_EACH_CONST(PathTree::FoundPaths, i, paths)
            {
                get()->locate<File>(*i).size();
            }
        }

        void clearPath(Path const &path)
        {
            try
            {
                DENG2_GUARD(this);
                if(get()) get()->removeFile(path);
            }
            catch(Folder::NotFoundError const &)
            {
                // Never put in the storage?
            }
        }
    };

    /**
     * Data item. Lockable because may be accessed from the worker
     * thread in addition to caller thread(s).
     */
    struct Data : public PathTree::Node, public Waitable, public Lockable
    {
        std::auto_ptr<IData> data;      ///< Non-NULL for in-memory items.
        std::auto_ptr<ISource> source;  ///< Always required.
        CacheLevel level;               ///< Current cache level.
        Time accessedAt;

        Data(PathTree::NodeArgs const &args)
            : Node(args),
              level(InColdStorage),
              accessedAt(Time::invalidTime())
        {}

        void clearData()
        {
            DENG2_GUARD(this);

            if(data.get()) data->aboutToUnload();
            data.reset();
        }

        void setData(IData *newData, Bank::Instance &bank)
        {
            DENG2_GUARD(this);
            DENG2_ASSERT(newData != 0);

            data.reset(newData);
            accessedAt = Time();

            bank.notify(Notification(Notification::Loaded, path()));
        }

        void setLevel(CacheLevel newLevel, Bank::Instance &bank)
        {
            DENG2_GUARD(this);

            if(level != newLevel)
            {
                level = newLevel;
                bank.notify(Notification(path(), newLevel));
            }
        }
    };

    typedef PathTreeT<Data> DataTree;

    /**
     * Job description for the worker thread.
     */
    struct Job
    {
        enum Task {
            Load,
            Serialize,
            Deserialize,
            Quit
        };
        Task task;
        Path path;

        Job(Task t, Path p = "") : task(t), path(p) {}
    };

    typedef WaitableFIFO<Job> Queue;

    /**
     * Thread that operates the cache queue jobs. If not running, the queue
     * jobs are performed immediately in the caller's thread.
     */
    class WorkerThread : public QThread
    {
    public:
        WorkerThread(Public &i) : bank(i), items(i.d->items) {}

        void run()
        {
            // This runs in a background thread.
            while(doWork()) {}
        }

        Data &item(Path const &path)
        {
            return bank.d->items.find(path, FIND_ITEM);
        }

        void doLoad(Job *job)
        {
            try
            {
                // Acquire the source information.
                Data &it = item(job->path);
                DENG2_GUARD(it);
                if(it.level == InMemory)
                {
                    // It's already loaded.
                    it.post(); // In case someone is waiting.
                    return;
                }

                Time startedAt;

                // Ask the concrete bank implementation to load the data for
                // us. This may take an unspecified amount of time.
                QScopedPointer<IData> loaded(bank.loadFromSource(*it.source));

                LOG_DEBUG("Loaded \"%s\" in %.2f seconds") << job->path << startedAt.since();

                if(loaded.data())
                {
                    // Put the loaded data into the memory cache.
                    it.setData(loaded.take(), *bank.d);
                    it.setLevel(InMemory, *bank.d);
                    it.post(); // Notify other thread waiting on this load.

                    // Now that it is in memory, a copy in hot storage is obsolete.
                    bank.d->hotStorage.clearPath(job->path);
                }
            }
            catch(Error const &er)
            {
                LOG_WARNING("Could not load \"%s\":\n") << job->path << er.asText();

                // Make sure a blocking load completes anyway.
                item(job->path).post();
            }
        }

        void doSerialize(Job *job)
        {
            try
            {
                LOG_DEBUG("Serializing \"%s\"") << job->path;

                Data &it = item(job->path);
                DENG2_GUARD(it);

                if(it.level == InMemory && it.data.get() &&
                   it.data->asSerializable() && bank.d->hotStorage)
                {
                    // Source timestamp is included in the serialization
                    // to check later whether the data is still fresh.
                    File &hot = bank.d->hotStorage->newFile(job->path, Folder::ReplaceExisting);
                    Writer(hot).withHeader()
                            << it.source->modifiedAt()
                            << *it.data->asSerializable();

                    it.setLevel(InHotStorage, *bank.d);

                    // Remove from memory.
                    it.clearData();
                }
            }
            catch(Error const &er)
            {
                LOG_WARNING("Could not serialize \"%s\" to hot storage:\n")
                        << job->path << er.asText();
            }
        }

        void doDeserialize(Job *job)
        {
            try
            {
                LOG_DEBUG("Deserializing \"%s\"") << job->path;

                Data &it = item(job->path);
                DENG2_GUARD(it);

                // Sanity check.
                DENG2_ASSERT(it.level == InHotStorage);
                DENG2_ASSERT(bank.d->hotStorage.get() != 0);

                QScopedPointer<IData> blank(bank.newData());

                Time modifiedAt;
                File const &hot = bank.d->hotStorage->locate<File>(job->path);
                Reader reader(hot);
                reader.withHeader() >> modifiedAt;

                // Has the serialized data gone stale?
                if(it.source->modifiedAt() > modifiedAt)
                {
                    throw StaleError("Bank::doWork",
                                     QString("\"$1\" is stale (source:$2 hot:$3)")
                                     .arg(job->path)
                                     .arg(it.source->modifiedAt().asText())
                                     .arg(modifiedAt.asText()));
                }

                reader >> *blank->asSerializable();

                it.setData(blank.take(), *bank.d);
                it.setLevel(InMemory, *bank.d);
                it.post();
            }
            catch(Error const &er)
            {
                LOG_WARNING("Could not deserialize \"%s\" from hot storage:\n")
                        << job->path << er.asText();

                // Try loading from source instead.
                bank.d->addJob(new Job(Job::Load, job->path));
            }
        }

        bool doWork()
        {
            LOG_AS("Bank::Worker");

            // Get the next job, waiting until one is available.
            QScopedPointer<Job> job(bank.d->jobs.take());
            if(!job || job->task == Job::Quit) return false;

            switch(job->task)
            {
            case Job::Load:
                doLoad(job.data());
                break;

            case Job::Serialize:
                doSerialize(job.data());
                break;

            case Job::Deserialize:
                doDeserialize(job.data());
                break;

            default:
                return false;
            }

            return true;
        }

    private:
        Public &bank;
        DataTree &items;
    };

    struct Notification
    {
        enum Kind { LevelChanged, Loaded };

        Kind kind;
        Path path;
        CacheLevel level;

        Notification(Kind k, Path const &p)
            : kind(k), path(p), level(InMemory) {}

        Notification(Path const &p, CacheLevel lev)
            : kind(LevelChanged), path(p), level(lev) {}
    };

    typedef FIFO<Notification> NotifyQueue;

    Flags flags;
    dint64 limits[NUM_CACHE_LEVELS];
    HotStorage hotStorage;
    DataTree items;
    Queue jobs;
    WorkerThread worker;
    QTimer *notifyTimer;
    NotifyQueue notifications;

    Instance(Public *i, Flags const &flg)
        : Base(i), flags(flg), hotStorage(*i), worker(*i)
    {
        limits[InColdStorage] = 0;
        limits[InHotStorage]  = Unlimited;
        limits[InMemory]      = Unlimited;

        notifyTimer = new QTimer(thisPublic);
        notifyTimer->setSingleShot(true);
        notifyTimer->setInterval(1);
        QObject::connect(notifyTimer, SIGNAL(timeout()), i, SLOT(performDeferredNotifications()));
    }

    ~Instance()
    {
        stopWorker();

        if(flags.testFlag(ClearHotStorageWhenBankDestroyed))
        {
            hotStorage.clear();
        }
    }

    inline bool isThreaded() const
    {
        return flags.testFlag(BackgroundThread);
    }

    void startWorker()
    {
        if(isThreaded())
        {
            worker.start();
        }
    }

    void stopWorker()
    {
        if(isThreaded())
        {
            jobs.put(new Job(Job::Quit), Queue::PutTail);
            worker.wait();
            jobs.clear();
        }
    }

    void addJob(Job *job, Queue::PutMode mode = Queue::PutTail)
    {
        jobs.put(job, mode);
        if(!isThreaded())
        {
            // Execute the job immediately.
            worker.doWork();
        }
    }

    void notify(Notification const &notif)
    {
        notifications.put(new Notification(notif));
        notifyTimer->start();
    }

    void clear()
    {
        // First make sure any background operations are done.
        stopWorker();

        items.clear();

        startWorker();
    }

    void setHotStorage(String const &location)
    {
        stopWorker();
        hotStorage.setLocation(location);
        startWorker();
    }

    /**
     * Remove the item identified by @a path from memory and hot storage.
     * (Synchronous operation.)
     */
    void clearFromCache(Path const &path)
    {
        Data &item = items.find(path, FIND_ITEM);
        DENG2_GUARD(item);

        if(item.level == InColdStorage) return;

        // Change the cache level.
        item.setLevel(InColdStorage, *this);

        // Clear the memory and hot storage caches.
        item.clearData();
        hotStorage.clearPath(path);
    }

    void load(Path const &path, LoadImportance importance)
    {       
        Queue::PutMode const mode = (importance == LoadImmediately? Queue::PutTail : Queue::PutHead);

        Data &item = items.find(path, FIND_ITEM);
        DENG2_GUARD(item);

        if(hotStorage && item.level == InHotStorage)
        {
            addJob(new Job(Job::Deserialize, path), mode);
        }
        else if(item.level == InColdStorage)
        {
            addJob(new Job(Job::Load, path), mode);
        }
    }

    void unload(Path const &path, CacheLevel toLevel)
    {
        // Is this a meaningful request?
        if(toLevel == InMemory || (toLevel == InHotStorage && !hotStorage))
            return; // Ignore...

        if(toLevel == InColdStorage)
        {
            clearFromCache(path);
            return;
        }

        Data &item = items.find(path, FIND_ITEM);
        DENG2_GUARD(item);

        if(item.level == InMemory && toLevel == InHotStorage)
        {
            // Serialize the data and remove from memory.
            addJob(new Job(Job::Serialize, path));
        }
    }

    void performNotifications()
    {
        forever
        {
            QScopedPointer<Notification> notif(notifications.take());
            if(!notif.data()) break;

            switch(notif->kind)
            {
            case Notification::Loaded:
                DENG2_FOR_PUBLIC_AUDIENCE(Load, i)
                {
                    i->bankLoaded(notif->path);
                }
                break;

            case Notification::LevelChanged:
                DENG2_FOR_PUBLIC_AUDIENCE(CacheLevel, i)
                {
                    i->bankCacheLevelChanged(notif->path, notif->level);
                }
                break;
            }
        }
    }
};

Bank::Bank(Flags const &flags, String const &hotStorageLocation)
    : d(new Instance(this, flags))
{
    d->hotStorage.setLocation(hotStorageLocation);
    d->startWorker();
}

Bank::~Bank()
{}

Bank::Flags Bank::flags() const
{
    return d->flags;
}

void Bank::setHotStorageCacheLocation(String const &location)
{
    d->setHotStorage(location);
}

void Bank::setHotStorageSize(dint64 maxBytes)
{
    d->limits[InHotStorage] = maxBytes;
}

void Bank::setMemoryCacheSize(dint64 maxBytes)
{
    d->limits[InMemory] = maxBytes;
}

String Bank::hotStorageCacheLocation() const
{
    if(d->hotStorage)
    {
        return d->hotStorage->path();
    }
    return "";
}

dint64 Bank::hotStorageSize() const
{
    return d->limits[InHotStorage];
}

dint64 Bank::memoryCacheSize() const
{
    return d->limits[InMemory];
}

void Bank::clear()
{
    d->clear();
}

void Bank::add(Path const &path, ISource *source)
{
    QScopedPointer<ISource> src(source);
    d->items.insert(path).source.reset(src.take());
}

void Bank::remove(Path const &path)
{
    d->items.remove(path, PathTree::NoBranch);
}

bool Bank::has(Path const &path) const
{
    return d->items.has(path);
}

dint Bank::allItems(Names &names) const
{
    names.clear();
    PathTree::FoundPaths paths;
    d->items.findAllPaths(paths, PathTree::NoBranch);
    DENG2_FOR_EACH(PathTree::FoundPaths, i, paths)
    {
        names.insert(*i);
    }
    return names.size();
}

PathTree const &Bank::index() const
{
    return d->items;
}

void Bank::load(Path const &path, LoadImportance importance)
{
    d->load(path, importance);
}

void Bank::loadAll()
{
    Names names;
    allItems(names);
    DENG2_FOR_EACH(Names, i, names)
    {
        load(*i, LoadAfterQueued);
    }
}

Bank::IData &Bank::data(Path const &path) const
{
    LOG_AS("Bank");

    // First check if the item is already in memory.
    Instance::Data &item = d->items.find(path, FIND_ITEM);
    DENG2_GUARD(item);

    // Mark it used.
    item.accessedAt = Time();

    if(item.data.get())
    {
        return *item.data;
    }

    // We'll have to request and wait.
    item.reset();
    item.unlock();

    LOG_DEBUG("Loading \"%s\"...") << path;

    Time requestedAt;
    d->load(path, LoadImmediately);
    item.wait();

    LOG_DEBUG("\"%s\" is available (waited %.2f seconds)") << path << requestedAt.since();

    item.lock();
    if(!item.data.get())
    {
        throw LoadError("Bank::data", "Failed to load \"" + path + "\"");
    }
    return *item.data;
}

void Bank::unload(Path const &path, CacheLevel toLevel)
{
    d->unload(path, toLevel);
}

void Bank::unloadAll(CacheLevel maxLevel)
{
    if(maxLevel >= InMemory) return;

    Names names;
    allItems(names);

    DENG2_FOR_EACH(Names, i, names)
    {
        unload(*i, maxLevel);
    }
}

void Bank::clearFromCache(Path const &path)
{
    d->clearFromCache(path);
}

void Bank::purge()
{
    /**
     * @todo Implement cache purging (and different purging strategies?).
     * Purge criteria can be age and cache level maximum limits.
     */
}

void Bank::performDeferredNotifications()
{
    d->performNotifications();
}

} // namespace de
