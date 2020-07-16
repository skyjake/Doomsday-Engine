/** @file bank.cpp  Abstract data bank with multi-tiered caching.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/bank.h"
#include "de/folder.h"
#include "de/app.h"
#include "de/loop.h"
#include "de/filesystem.h"
#include "de/pathtree.h"
#include "de/waitablefifo.h"
#include "de/task.h"
#include "de/taskpool.h"
#include "de/logbuffer.h"
#include "de/writer.h"
#include "de/reader.h"
#include "de/math.h"

namespace de {

namespace internal {

/**
 * Cache of objects of type ItemType. Does not own the objects or the data,
 * merely guides operations on them.
 */
template <typename ItemType>
class Cache : public Lockable
{
public:
    enum Format {
        Source,     ///< Data is present only as source information.
        Object,     ///< Data is present as a fully usable object in memory.
        Serialized  ///< Data is present as a block of serialized bytes.
    };

    static const char *formatAsText(Format f) {
        switch (f) {
        case Source:     return "Source";     break;
        case Object:     return "Object";     break;
        case Serialized: return "Serialized"; break;
        }
        return "";
    }

    typedef Set<ItemType *> Items;

public:
    Cache(Format format)
        : _format(format),
          _maxBytes(Bank::Unlimited),
          _currentBytes(0),
          _maxItems(Bank::Unlimited) {}

    virtual ~Cache() {}

    void setMaxBytes(dint64 m) { _maxBytes = m; }
    void setMaxItems(int m) { _maxItems = m; }

    Format format() const { return _format; }
    dint64 maxBytes() const { return _maxBytes; }
    dint64 byteCount() const { return _currentBytes; }
    int maxItems() const { return _maxItems; }
    int itemCount() const { return _items.size(); }

    virtual void add(ItemType &data) {
        DE_GUARD(this);
        _items.insert(&data);
    }

    virtual void remove(ItemType &data) {
        DE_GUARD(this);
        _items.remove(&data);
    }

    bool contains(const ItemType *data) const {
        DE_GUARD(this);
        return _items.contains(const_cast<ItemType *>(data));
    }

    virtual void clear() {
        DE_GUARD(this);
        _items.clear();
        _currentBytes = 0;
    }

    const Items &items() const { return _items; }

protected:
    void addBytes(dint64 bytes) {
        _currentBytes = de::max(dint64(0), _currentBytes + bytes);
    }

private:
    Format _format;
    dint64 _maxBytes;
    dint64 _currentBytes;
    int _maxItems;
    Items _items;
};

} // namespace internal

DE_PIMPL(Bank)
{
    /**
     * Data item. Has ownership of the in-memory cached data and the source
     * information. Lockable because may be accessed from the worker thread in
     * addition to caller thread(s).
     */
    struct Data : public PathTree::Node, public Waitable, public Lockable
    {
        typedef internal::Cache<Data> Cache;

        Bank *bank;                     ///< Bank that owns the data.
        std::unique_ptr<IData> data;    ///< Non-NULL for in-memory items.
        std::unique_ptr<ISource> source;///< Always required.
        SafePtr<File> serial;           ///< Serialized representation (if one is present; not owned).
        Cache *cache;                   ///< Current cache for the data (never NULL).
        Time accessedAt;

        Data(const PathTree::NodeArgs &args)
            : Node(args)
            , bank(0)
            , cache(0)
            , accessedAt(Time::invalidTime())
        {}

        void clearData()
        {
            DE_GUARD(this);

            if (data.get())
            {
                LOG_XVERBOSE("Item \"%s\" data cleared from memory (%i bytes)",
                        path(bank->d->sepChar) << data->sizeInMemory());
                data->aboutToUnload();
                data.reset();
            }
        }

        void setData(IData *newData)
        {
            DE_GUARD(this);
            DE_ASSERT(newData != 0);

            data.reset(newData);
            accessedAt = Time::currentHighPerformanceTime();
            bank->d->notify(Notification(Notification::Loaded, path(bank->d->sepChar)));
        }

        /// Load the item into memory from its current cache.
        void load()
        {
            DE_ASSERT(cache != 0);

            switch (cache->format())
            {
            case Cache::Source:
                loadFromSource();
                break;

            case Cache::Serialized:
                loadFromSerialized();
                break;

            case Cache::Object:
                // No need to do anything, already loaded.
                break;
            }
        }

        void loadFromSource()
        {
            DE_ASSERT(source.get() != 0);

            Time startedAt;

            // Ask the concrete bank implementation to load the data for
            // us. This may take an unspecified amount of time.
            std::unique_ptr<IData> loaded(bank->loadFromSource(*source));

            LOG_XVERBOSE("Loaded \"%s\" from source in %.2f seconds",
                         path(bank->d->sepChar) << startedAt.since());

            if (loaded)
            {
                // Put the loaded data into the memory cache.
                setData(loaded.release());
            }
        }

        bool isValidSerialTime(const Time &serialTime) const
        {
            return (!source->modifiedAt().isValid() ||
                    source->modifiedAt() == serialTime);
        }

        void loadFromSerialized()
        {
            DE_ASSERT(serial);

            try
            {
                Time startedAt;

                Time timestamp(Time::invalidTime());
                Reader reader(*serial);
                reader.withHeader() >> timestamp;

                if (isValidSerialTime(timestamp))
                {
                    std::unique_ptr<IData> blank(bank->newData());
                    reader >> *blank->asSerializable();
                    setData(blank.release());
                    LOG_XVERBOSE("Deserialized \"%s\" in %.2f seconds",
                                 path(bank->d->sepChar) << startedAt.since());
                    return; // Done!
                }
                // We cannot use this.
            }
            catch (const Error &er)
            {
                LOG_WARNING("Failed to deserialize \"%s\":\n")
                        << path(bank->d->sepChar) << er.asText();
            }

            // Fallback option.
            loadFromSource();
        }

        void serialize(const Path &folderPath)
        {
            DE_GUARD(this);

            if (serial)
            {
                // Already serialized.
                return;
            }

            DE_ASSERT(source.get() != 0);

            if (!data.get())
            {
                // We must have the object in memory first.
                loadFromSource();
            }

            DE_ASSERT(data->asSerializable() != 0);

            try
            {
                // Make sure the correct folder exists.
                Folder &containingFolder = FileSystem::get()
                        .makeFolder(folderPath / path().toString().fileNamePath());

                if (!data->shouldBeSerialized())
                {
                    // Not necessary; the serialized version already exists?
                    if (File *existing = containingFolder.tryLocate<File>(name()))
                    {
                        serial.reset(existing);
                        return;
                    }
                }

                // Source timestamp is included in the serialization
                // to check later whether the data is still fresh.
                serial.reset(&containingFolder.createFile(name(), Folder::ReplaceExisting));

                LOG_XVERBOSE("Serializing into %s", serial->description());

                Writer(*serial).withHeader()
                        << source->modifiedAt()
                        << *data->asSerializable();
                serial->flush();
            }
            catch (...)
            {
                serial.reset();
                throw;
            }
        }

        void clearSerialized()
        {
            DE_GUARD(this);

            serial.reset();
        }

        void changeCache(Cache &toCache)
        {
            DE_GUARD(this);
            DE_ASSERT(cache != 0);

            if (cache != &toCache)
            {
                Cache &fromCache = *cache;
                toCache.add(*this);
                fromCache.remove(*this);
                cache = &toCache;

                const Path itemPath = path(bank->d->sepChar);

                LOGDEV_RES_XVERBOSE("Item \"%s\" moved to %s cache",
                                    itemPath << Cache::formatAsText(toCache.format()));

                bank->d->notify(Notification(itemPath, toCache));
            }
        }
    };

    typedef PathTreeT<Data> DataTree;
    typedef internal::Cache<Data> DataCache;

    /**
     * Dummy cache representing objects in the source data.
     */
    struct SourceCache : public DataCache
    {
        SourceCache() : DataCache(Source) {}
    };

    /**
     * Hot storage containing data items serialized into files. The goal is to
     * allow quick recovery of data into memory. May be disabled in a Bank.
     */
    class SerializedCache : public DataCache
    {
    public:
        SerializedCache() : DataCache(Serialized) {}

        void add(Data &item)
        {
            DE_GUARD(this);

            DE_ASSERT(!_path.isEmpty());
            item.serialize(_path);
            addBytes(item.serial->size());
            DataCache::add(item);
        }

        void remove(Data &item)
        {
            DE_GUARD(this);

            addBytes(-dint64(item.serial->size()));
            item.clearSerialized();
            DataCache::remove(item);
        }

        void setLocation(const String &location)
        {
            DE_ASSERT(!location.isEmpty());
            DE_GUARD(this);

            // Serialized "hot" data is kept here.
            _path = location;
        }

        const Path &path() const
        {
            return _path;
        }

        Folder *folder() const
        {
            return FS::tryLocate<Folder>(_path);
        }

    private:
        Path _path;
    };

    /**
     * Cache of objects in memory.
     */
    class ObjectCache : public DataCache
    {
    public:
        ObjectCache() : DataCache(Object)
        {}

        void add(Data &item)
        {
            // Acquire the object.
            item.load();

            DE_GUARD(this);

            DE_ASSERT(item.data.get() != nullptr);
            addBytes(item.data->sizeInMemory());
            DataCache::add(item);
        }

        void remove(Data &item)
        {
            DE_ASSERT(item.data.get() != nullptr);

            DE_GUARD(this);

            addBytes(-dint64(item.data->sizeInMemory()));
            item.clearData();
            DataCache::remove(item);
        }
    };

    /**
     * Operation on a data item (e.g., loading, serialization). Run by TaskPool
     * in a background thread.
     */
    class Job : public Task
    {
    public:
        enum Task {
            Load,
            Serialize,
            Unload
        };

    public:
        Job(Bank &bk, Task t, const Path &p = Path())
            : _bank(bk), _task(t), _path(p)
        {}

        void runTask()
        {
            LOG_AS("Bank::Job");

            switch (_task)
            {
            case Job::Load:
                doLoad();
                break;

            case Job::Serialize:
                doSerialize();
                break;

            case Job::Unload:
                doUnload();
                break;
            }
        }

        Data &item()
        {
            return _bank.d->items.find(_path, PathTree::MatchFull | PathTree::NoBranch);
        }

        void doLoad()
        {
            try
            {
                Data &it = item();
                it.changeCache(_bank.d->memoryCache);
            }
            catch (const Error &er)
            {
                LOG_WARNING("Failed to load \"%s\" from source:\n") << _path << er.asText();
            }
            // Ensure a blocking load completes.
            item().post();
        }

        void doSerialize()
        {
            try
            {
                DE_ASSERT(_bank.d->serialCache != 0);

                LOG_XVERBOSE("Serializing \"%s\"", _path);
                item().changeCache(*_bank.d->serialCache);
            }
            catch (const Error &er)
            {
                LOG_WARNING("Failed to serialize \"%s\" to hot storage:\n")
                        << _path << er.asText();
            }
        }

        void doUnload()
        {
            try
            {
                LOGDEV_RES_XVERBOSE("Unloading \"%s\"", _path);
                item().changeCache(_bank.d->sourceCache);
            }
            catch (const Error &er)
            {
                LOG_WARNING("Error when unloading \"%s\":\n")
                        << _path << er.asText();
            }
        }

    private:
        Bank &_bank;
        Task _task;
        Path _path;
    };

    /**
     * Notification about status changes of data in the tree.
     */
    struct Notification
    {
        enum Kind { CacheChanged, Loaded };

        Kind kind;
        Path path;
        DataCache *cache;

        Notification(Kind k, const Path &p)
            : kind(k), path(p), cache(0) {}

        Notification(const Path &p, DataCache &c)
            : kind(CacheChanged), path(p), cache(&c) {}
    };

    typedef FIFO<Notification> NotifyQueue;

    const char *nameForLog;
    Char sepChar { '.' }; ///< Default separator in identifier paths.
    Flags flags;
    SourceCache sourceCache;
    ObjectCache memoryCache;
    std::unique_ptr<SerializedCache> serialCache;
    DataTree items;
    TaskPool jobs;
    NotifyQueue notifications;
    Dispatch dispatch;

    Impl(Public *i, const char *name, const Flags &flg)
        : Base(i)
        , nameForLog(name)
        , flags(flg)
    {
        if (~flags & DisableHotStorage)
        {
            serialCache.reset(new SerializedCache);
        }
    }

    ~Impl()
    {
        destroySerialCache();
    }

    /**
     * Deletes everything in the hot storage folder.
     */
    void clearHotStorage()
    {
        DE_ASSERT(serialCache);

        FS::waitForIdle();
        if (Folder *folder = serialCache->folder())
        {
            folder->destroyAllFilesRecursively();
        }
    }

    void destroySerialCache()
    {
        jobs.waitForDone();

        // Should we delete the actual files where the data has been kept?
        if (serialCache && (flags & ClearHotStorageWhenBankDestroyed))
        {
            clearHotStorage();
        }

        serialCache.reset();
    }

    inline bool isThreaded() const
    {
        return (flags & BackgroundThread) != 0;
    }

    void beginJob(Job *job, Importance importance)
    {
        if (!isThreaded() || importance == ImmediatelyInCurrentThread)
        {
            // Execute the job immediately.
            std::unique_ptr<Job> j(job);
            j->runTask();
            performNotifications();
        }
        else
        {
            jobs.start(job, importance == AfterQueued?
                           TaskPool::LowPriority : TaskPool::HighPriority);
        }
    }

    void clear()
    {
        jobs.waitForDone();

        items.clear();
        sourceCache.clear();
        memoryCache.clear();
        if (serialCache)
        {
            serialCache->clear();
        }
    }

    void setSerialLocation(const String &location)
    {
        if (location.isEmpty() || (flags & DisableHotStorage))
        {
            destroySerialCache();
        }
        else
        {
            if (!serialCache) serialCache.reset(new SerializedCache);
            serialCache->setLocation(location);

            // Make sure that the cache folder is immediately populated so that the
            // cached data available for future operations.
            FS::get().makeFolder(location);
        }
    }

    void putInBestCache(Data &item)
    {
        DE_ASSERT(item.cache == nullptr);

        // The source cache is always good.
        DataCache *best = &sourceCache;

        if (serialCache)
        {
            // Check if this item is already available in hot storage.
            File *src = FS::tryLocate<File>(serialCache->path() / item.path());
            if (src)
            {
                Time hotTime;
                Reader(*src).withHeader() >> hotTime;

                if (item.isValidSerialTime(hotTime))
                {
                    LOGDEV_RES_VERBOSE("Found valid serialized copy of \"%s\"") << item.path(sepChar);

                    item.serial.reset(src);
                    best = serialCache.get();
                }
            }
        }

        item.cache = best;
        best->add(item);
    }

    void load(const Path &path, Importance importance)
    {
        beginJob(new Job(self(), Job::Load, path), importance);
    }

    void unload(const Path &path, CacheLevel toLevel, Importance importance)
    {
        if (toLevel < InMemory)
        {
            const Job::Task task = (toLevel == InHotStorage && serialCache?
                                    Job::Serialize : Job::Unload);
            beginJob(new Job(self(), task, path), importance);
        }
    }

    void notify(const Notification &notif)
    {
        notifications.put(new Notification(notif));
        if (isThreaded())
        {
            dispatch += [this] () { performNotifications(); };
        }
    }

    void performNotifications()
    {
        for (;;)
        {
            std::unique_ptr<Notification> notif(notifications.take());
            if (!notif.get()) break;

            performNotification(*notif);
        }
    }

    void performNotification(const Notification &nt)
    {
        switch (nt.kind)
        {
        case Notification::Loaded:
            DE_NOTIFY_PUBLIC(Load, i)
            {
                i->bankLoaded(nt.path);
            }
            break;

        case Notification::CacheChanged:
            DE_NOTIFY_PUBLIC(CacheLevel, i)
            {
                DE_ASSERT(nt.cache != 0);

                i->bankCacheLevelChanged(nt.path,
                      nt.cache == &memoryCache?      InMemory :
                      nt.cache == serialCache.get()? InHotStorage :
                                                     InColdStorage);
            }
            break;
        }
    }

    DE_PIMPL_AUDIENCE(Load)
    DE_PIMPL_AUDIENCE(CacheLevel)
};

DE_AUDIENCE_METHOD(Bank, Load)
DE_AUDIENCE_METHOD(Bank, CacheLevel)

Bank::Bank(const char *nameForLog, const Flags &flags, const String &hotStorageLocation)
    : d(new Impl(this, nameForLog, flags))
{
    d->setSerialLocation(hotStorageLocation);
}

Bank::~Bank()
{
    clear();
}

const char *Bank::nameForLog() const
{
    return d->nameForLog;
}

Flags Bank::flags() const
{
    return d->flags;
}

void Bank::setSeparator(Char sep)
{
    d->sepChar = sep;
}

void Bank::setHotStorageCacheLocation(const String &location)
{
    d->setSerialLocation(location);
}

void Bank::setHotStorageSize(dint64 maxBytes)
{
    if (d->serialCache)
    {
        d->serialCache->setMaxBytes(maxBytes);
    }
}

void Bank::setMemoryCacheSize(dint64 maxBytes)
{
    d->memoryCache.setMaxBytes(maxBytes);
}

Path Bank::hotStorageCacheLocation() const
{
    if (d->serialCache)
    {
        return d->serialCache->path();
    }
    return Path();
}

dint64 Bank::hotStorageSize() const
{
    if (d->serialCache)
    {
        return d->serialCache->maxBytes();
    }
    return 0;
}

dint64 Bank::memoryCacheSize() const
{
    return d->memoryCache.maxBytes();
}

void Bank::clearHotStorage()
{
    if (d->serialCache)
    {
        d->clearHotStorage();
    }
}

void Bank::clear()
{
    d->clear();
}

void Bank::add(const DotPath &path, ISource *source)
{
    LOG_AS(d->nameForLog);

    std::unique_ptr<ISource> src(source);

    // Paths are unique.
    if (d->items.has(path, PathTree::MatchFull | PathTree::NoBranch))
    {
        throw AlreadyExistsError(stringf("%s::add", d->nameForLog),
                                 "Item '" + path + "' already exists");
    }

    Impl::Data &item = d->items.insert(path);

    DE_GUARD(item);

    item.bank = this;
    item.source.reset(src.release());

    d->putInBestCache(item);
}

void Bank::remove(const DotPath &path)
{
    d->items.remove(path, PathTree::NoBranch);
}

bool Bank::has(const DotPath &path) const
{
    return d->items.has(path);
}

Bank::ISource &Bank::source(const DotPath &path) const
{
    return *d->items.find(path, PathTree::MatchFull | PathTree::NoBranch).source.get();
}

dint Bank::allItems(Names &names) const
{
    names.clear();
    iterate([&names] (const DotPath &path) {
        names.insert(path.toString());
    });
    return dint(names.size());
}

void Bank::iterate(const std::function<void (const DotPath &)> &func) const
{
    PathTree::FoundPaths paths;
    d->items.findAllPaths(paths, PathTree::NoBranch, d->sepChar);
    for (const String &path : paths)
    {
        func(path);
    }
}

const PathTree &Bank::index() const
{
    return d->items;
}

void Bank::load(const DotPath &path, Importance importance)
{
    d->load(path, importance);
}

void Bank::loadAll()
{
    Names names;
    allItems(names);
    DE_FOR_EACH(Names, i, names)
    {
        load(*i, AfterQueued);
    }
}

Bank::IData &Bank::data(const DotPath &path) const
{
    LOG_AS(d->nameForLog);

    // First check if the item is already in memory.
    Impl::Data &item = d->items.find(path, PathTree::MatchFull | PathTree::NoBranch);
    DE_GUARD(item);

    // Mark it used.
    item.accessedAt = Time::currentHighPerformanceTime();

    if (item.data.get())
    {
        // Item is already in memory.
        return *item.data;
    }

    // We'll have to request and wait.
    //item.reset();
    item.unlock();

    LOG_XVERBOSE("Loading \"%s\"...", path);

    Time requestedAt;
    d->load(path, BeforeQueued);
    item.wait();

    const TimeSpan waitTime = requestedAt.since();

    item.lock();
    if (!item.data.get())
    {
        throw LoadError(stringf("%s::data", d->nameForLog), "Failed to load \"" + path + "\"");
    }

    if (waitTime > 0.0)
    {
        LOG_VERBOSE("\"%s\" loaded (waited %.3f seconds)") << path << waitTime;
    }
    else
    {
        LOG_VERBOSE("\"%s\" loaded") << path;
    }

    return *item.data;
}

bool Bank::isLoaded(const DotPath &path) const
{
    if (const Impl::Data *item = d->items.tryFind(path, PathTree::MatchFull | PathTree::NoBranch))
    {
        return d->memoryCache.contains(item);
    }
    return false;
}

void Bank::unload(const DotPath &path, CacheLevel toLevel, Importance importance)
{
    d->unload(path, toLevel, importance);
}

void Bank::unloadAll(CacheLevel maxLevel)
{
    return unloadAll(AfterQueued, maxLevel);
}

void Bank::unloadAll(Importance importance, CacheLevel maxLevel)
{
    if (maxLevel >= InMemory) return;

    Names names;
    allItems(names);
    DE_FOR_EACH(Names, i, names)
    {
        unload(Path(*i, d->sepChar), maxLevel, importance);
    }
}

void Bank::clearFromCache(const DotPath &path)
{
    d->unload(path, InColdStorage, AfterQueued);
}

void Bank::purge()
{
    /**
     * @todo Implement cache purging (and different purging strategies?).
     * Purge criteria can be age and cache level maximum limits.
     */
}

Bank::IData *Bank::newData()
{
    return 0;
}

} // namespace de
