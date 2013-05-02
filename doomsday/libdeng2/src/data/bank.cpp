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
#include "de/Task"
#include "de/TaskPool"
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
        Bank *bank;                     ///< Bank that owns the data.
        std::auto_ptr<IData> data;      ///< Non-NULL for in-memory items.
        std::auto_ptr<ISource> source;  ///< Always required.
        CacheLevel level;               ///< Current cache level.
        Time accessedAt;

        Data(PathTree::NodeArgs const &args)
            : Node(args),
              bank(0),
              level(InColdStorage),
              accessedAt(Time::invalidTime())
        {}

        void clearData()
        {
            DENG2_GUARD(this);

            if(data.get())
            {
                LOG_DEBUG("Item \"%s\" data cleared from memory (%i bytes)")
                        << path() << data->sizeInMemory();
                data->aboutToUnload();
            }
            data.reset();
        }

        void setData(IData *newData)
        {
            DENG2_GUARD(this);
            DENG2_ASSERT(newData != 0);

            data.reset(newData);
            accessedAt = Time();
            bank->d->notify(Notification(Notification::Loaded, path()));
        }

        void setLevel(CacheLevel newLevel)
        {
            DENG2_GUARD(this);

            if(level != newLevel)
            {
                LOG_DEBUG("Item \"%s\" cache level changed to %i") << path() << newLevel;

                level = newLevel;
                bank->d->notify(Notification(path(), newLevel));
            }
        }
    };

    typedef PathTreeT<Data> DataTree;

    /**
     * Job description for the worker thread.
     */
    class Job : public Task
    {
    public:
        enum Task
        {
            Load,
            Serialize,
            Deserialize
        };

    public:
        Job(Bank &bk, Task t, Path p = "")
            : _bank(bk), _items(bk.d->items), _task(t), _path(p)
        {}

        /**
         * Performs the job. This is run by QThreadPool in a background thread.
         */
        void runTask()
        {
            LOG_AS("Bank::Job");

            switch(_task)
            {
            case Job::Load:
                doLoad();
                break;

            case Job::Serialize:
                doSerialize();
                break;

            case Job::Deserialize:
                doDeserialize();
                break;
            }
        }

        Data &item()
        {
            return _items.find(_path, FIND_ITEM);
        }

        void doLoad()
        {
            try
            {
                // Acquire the source information.
                Data &it = item();
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
                QScopedPointer<IData> loaded(_bank.loadFromSource(*it.source));

                LOG_DEBUG("Loaded \"%s\" in %.2f seconds") << _path << startedAt.since();

                if(loaded.data())
                {
                    // Put the loaded data into the memory cache.
                    it.setData(loaded.take());
                    it.setLevel(InMemory);
                    it.post(); // Notify other thread waiting on this load.

                    // Now that it is in memory, a copy in hot storage is obsolete.
                    _bank.d->hotStorage.clearPath(_path);
                }
            }
            catch(Error const &er)
            {
                LOG_WARNING("Could not load \"%s\":\n") << _path << er.asText();

                // Make sure a blocking load completes anyway.
                item().post();
            }
        }

        void doSerialize()
        {
            try
            {
                LOG_DEBUG("Serializing \"%s\"") << _path;

                Data &it = item();
                DENG2_GUARD(it);

                if(it.level == InMemory && it.data.get() &&
                   it.data->asSerializable() && _bank.d->hotStorage)
                {
                    // Source timestamp is included in the serialization
                    // to check later whether the data is still fresh.
                    File &hot = _bank.d->hotStorage->newFile(_path, Folder::ReplaceExisting);
                    Writer(hot).withHeader()
                            << it.source->modifiedAt()
                            << *it.data->asSerializable();

                    it.setLevel(InHotStorage);

                    // Remove from memory.
                    it.clearData();
                }
            }
            catch(Error const &er)
            {
                LOG_WARNING("Could not serialize \"%s\" to hot storage:\n")
                        << _path << er.asText();
            }
        }

        void doDeserialize()
        {
            try
            {
                LOG_DEBUG("Deserializing \"%s\"") << _path;

                Data &it = item();
                DENG2_GUARD(it);

                // Sanity check.
                DENG2_ASSERT(it.level == InHotStorage);
                DENG2_ASSERT(_bank.d->hotStorage.get() != 0);

                QScopedPointer<IData> blank(_bank.newData());

                Time modifiedAt;
                File const &hot = _bank.d->hotStorage->locate<File>(_path);
                Reader reader(hot);
                reader.withHeader() >> modifiedAt;

                // Has the serialized data gone stale?
                if(it.source->modifiedAt() > modifiedAt)
                {
                    throw StaleError("Bank::doWork",
                                     QString("\"$1\" is stale (source:$2 hot:$3)")
                                     .arg(_path)
                                     .arg(it.source->modifiedAt().asText())
                                     .arg(modifiedAt.asText()));
                }

                reader >> *blank->asSerializable();

                it.setData(blank.take());
                it.setLevel(InMemory);
                it.post();
            }
            catch(Error const &er)
            {
                LOG_WARNING("Could not deserialize \"%s\" from hot storage:\n")
                        << _path << er.asText();

                // Try loading from source instead.
                _bank.d->addJob(new Job(_bank, Job::Load, _path), Immediately);
            }
        }

    private:
        Bank &_bank;
        DataTree &_items;
        Task _task;
        Path _path;
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
    TaskPool jobs;
    QTimer *notifyTimer;
    NotifyQueue notifications;

    Instance(Public *i, Flags const &flg)
        : Base(i), flags(flg), hotStorage(*i)
    {
        limits[InColdStorage] = 0;
        limits[InHotStorage]  = Unlimited;
        limits[InMemory]      = Unlimited;

        // Timer for triggering main loop notifications from background workers.
        notifyTimer = new QTimer(thisPublic);
        notifyTimer->setSingleShot(true);
        notifyTimer->setInterval(1);
        QObject::connect(notifyTimer, SIGNAL(timeout()), i, SLOT(performDeferredNotifications()));
    }

    ~Instance()
    {
        jobs.waitForDone();

        if(flags.testFlag(ClearHotStorageWhenBankDestroyed))
        {
            hotStorage.clear();
        }
    }

    inline bool isThreaded() const
    {
        return flags.testFlag(BackgroundThread);
    }

    void addJob(Job *job, Importance importance)
    {
        if(!isThreaded())
        {
            // Execute the job immediately.
            QScopedPointer<Job> j(job);
            j->runTask();
        }
        else
        {
            jobs.start(job, importance == AfterQueued?
                           TaskPool::LowPriority : TaskPool::HighPriority);
        }
    }

    void notify(Notification const &notif)
    {
        if(isThreaded())
        {
            notifications.put(new Notification(notif));
            notifyTimer->start();
        }
        else
        {
            performNotification(notif);
        }
    }

    void clear()
    {
        jobs.waitForDone();
        items.clear();
    }

    void setHotStorage(String const &location)
    {
        jobs.waitForDone();
        hotStorage.setLocation(location);
    }

    void restoreFromHotStorage(Data &item)
    {
        if(!hotStorage) return;

        try
        {
            // Check if this item is available in hot storage; if so, mark its
            // level appropriately.
            if(hotStorage->has(item.path()))
            {
                Time hotTime;
                Reader(hotStorage->locate<File const>(item.path())).withHeader() >> hotTime;

                if(!item.source->modifiedAt().isValid() ||
                   item.source->modifiedAt() == hotTime)
                {
                    addJob(new Job(self, Job::Deserialize, item.path()), AfterQueued);
                }
            }
        }
        catch(Error const &er)
        {
            LOG_WARNING("Failed to restore \"%s\" from hot storage:\n")
                    << item.path() << er.asText();
        }
    }

    /**
     * Remove the item identified by @a path from memory and hot storage.
     * (Synchronous operation.)
     */
    void clearFromCache(Path const &path)
    {
        Data &item = items.find(path, FIND_ITEM);
        DENG2_GUARD(item);

        if(item.level != InColdStorage)
        {
            // Change the cache level.
            item.setLevel(InColdStorage);

            // Clear the memory and hot storage caches.
            item.clearData();
            hotStorage.clearPath(path);
        }
    }

    void load(Path const &path, Importance importance)
    {       
        Data &item = items.find(path, FIND_ITEM);
        DENG2_GUARD(item);

        if(hotStorage && item.level == InHotStorage)
        {
            addJob(new Job(self, Job::Deserialize, path), importance);
        }
        else if(item.level == InColdStorage)
        {
            addJob(new Job(self, Job::Load, path), importance);
        }
    }

    void unload(Path const &path, CacheLevel toLevel)
    {
        // Is this a meaningful request?
        if(toLevel == InHotStorage && !hotStorage)
        {
            toLevel = InColdStorage;
        }
        if(toLevel == InMemory)
        {
            return; // Ignore...
        }
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
            addJob(new Job(self, Job::Serialize, path), Immediately);
        }
    }

    void performNotifications()
    {
        forever
        {
            QScopedPointer<Notification> notif(notifications.take());
            if(!notif.data()) break;

            performNotification(*notif);
        }
    }

    void performNotification(Notification const &nt)
    {
        switch(nt.kind)
        {
        case Notification::Loaded:
            DENG2_FOR_PUBLIC_AUDIENCE(Load, i)
            {
                i->bankLoaded(nt.path);
            }
            break;

        case Notification::LevelChanged:
            DENG2_FOR_PUBLIC_AUDIENCE(CacheLevel, i)
            {
                i->bankCacheLevelChanged(nt.path, nt.level);
            }
            break;
        }
    }
};

Bank::Bank(Flags const &flags, String const &hotStorageLocation)
    : d(new Instance(this, flags))
{
    d->hotStorage.setLocation(hotStorageLocation);
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
    LOG_AS("Bank");

    QScopedPointer<ISource> src(source);
    Instance::Data &item = d->items.insert(path);

    DENG2_GUARD(item);

    item.bank = this;
    item.source.reset(src.take());

    d->restoreFromHotStorage(item);
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

void Bank::load(Path const &path, Importance importance)
{
    d->load(path, importance);
}

void Bank::loadAll()
{
    Names names;
    allItems(names);
    DENG2_FOR_EACH(Names, i, names)
    {
        load(*i, AfterQueued);
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
    d->load(path, Immediately);
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
