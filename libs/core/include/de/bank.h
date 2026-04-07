/** @file bank.h  Abstract data bank with multi-tiered caching.
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

#ifndef LIBCORE_BANK_H
#define LIBCORE_BANK_H

#include <set>
#include <functional>

#include "de/libcore.h"
#include "de/path.h"
#include "de/pathtree.h"
#include "de/iserializable.h"
#include "de/observers.h"
#include "de/time.h"

namespace de {

/**
 * Abstract data bank with multi-tiered caching. Bank has the following
 * characteristics:
 * - Organises a set of data using a PathTree into a logical structure
 *  (e.g., a set of images in the UI style).
 * - Generic caching mechanism applicable to any data objects.
 * - Supports use of serialization to move objects from memory to persistent
 *   disk cache (not intended to replicate what the OS virtual memory does,
 *   but rather as a way to avoid repetitive preprocessing tasks on source
 *   data, e.g., map format conversion).
 * - Utilizes concurrency by running tasks in background thread(s).
 *
 * Data items are identified using Paths. Serializable data items can be stored
 * in a persistent cache ("hot storage"), from where they can be deserialized
 * quickly in the future. @note Bank uses DotPath, so the path separator is
 * assumed to be "." unless explicitly specified in the arguments.
 *
 * Data is kept cached on multiple levels: in memory, in hot storage
 * (serialized in a file), or in cold storage (unprocessed source data). When a
 * cache level's maximum size is reached, the oldest items are moved to a lower
 * level (age determined by latest time of access).
 *
 * Bank supports both synchronous and asynchronous usage. (The latter requires
 * use of Bank::BackgroundThread.)
 *
 * @par Thread-safety
 *
 * When using BackgroundThread, the Bank will perform all heavy lifting in
 * separate worker threads (loading from source and (de)serialization). However,
 * audience notifications always occur in the main thread (where the
 * application event loop is running).
 *
 * A user of the Bank does not need to worry about cached items disappearing
 * suddenly from memory: purging items to lower cache levels only occurs on
 * request (Bank::purge()). Also, when an item is being removed from memory, it
 * will receive a notification beforehand (IData::aboutToUnload()).
 *
 * @ingroup data
 */
class DE_PUBLIC Bank
{
public:
    /// Failed to load data from the source. @ingroup errors
    DE_ERROR(LoadError);

    /// Attempted to add an item that already exists in the bank. @ingroup errors
    DE_ERROR(AlreadyExistsError);

    enum Flag
    {
        SingleThread = 0,

        /**
         * Separate thread used for managing the bank's data (loading, caching
         * data). Requires data items and sources to be thread-safe.
         */
        BackgroundThread = 0x1,

        EnableHotStorage = 0,

        /**
         * Do not use the hot storage to keep serialized copies of data items.
         * This is useful if recreating the data from source is trivial.
         */
        DisableHotStorage = 0x2,

        /**
         * When the Bank instance is destroyed (e.g., when the application
         * shuts down) the contents of the hot storage cache level are cleared.
         * If not specified, the hot storage is reused when the Bank is
         * recreated later (unless the source data is newer).
         */
        ClearHotStorageWhenBankDestroyed = 0x4,

        DefaultFlags = SingleThread | DisableHotStorage
    };

    enum CacheLevel
    {
        /**
         * Data is in its original storage container (e.g., source file) and it
         * has to be processed, parsed or decoded before it can be used.
         */
        InColdStorage = 0,

        /**
         * Data is not in memory but can be restored to memory relatively
         * quickly, for instance just be reading a file. Uses serialization to
         * convert the data to/from bytes.
         */
        InHotStorage = 1,

        /**
         * Data is in memory and available for use immediately.
         */
        InMemory = 2
    };

    enum Importance
    {
        ImmediatelyInCurrentThread, ///< Request handled in this thread synchronously.
        BeforeQueued,               ///< Request handled before any queued tasks.
        AfterQueued                 ///< Request handled after any queued tasks.
    };

    enum { Unlimited = -1 };

    /**
     * Interface for specifying the source of a data item.
     */
    class ISource
    {
    public:
        virtual ~ISource() = default;

        /**
         * Returns the timestamp of the source data, which determines when the
         * source data has last been modified. If the source is newer/older than
         * cached copies, the cached data is discarded. If the returned time is
         * Time::invalidTime(), no time checks are performed and the source
         * data is considered immutable.
         */
        virtual Time modifiedAt() const {
            return Time::invalidTime();
        }

        DE_CAST_METHODS()
    };

    /**
     * Interface for a data item kept in memory.
     */
    class IData
    {
    public:
        virtual ~IData() = default;

        /// Serialization should be done?
        virtual bool shouldBeSerialized() const { return true; }

        /// Returns an ISerializable pointer to the object. Required
        /// for putting the data in hot storage.
        virtual ISerializable *asSerializable() { return nullptr; }

        /// Returns the size of the data that it occupies in memory.
        virtual duint sizeInMemory() const { return 0; }

        /// Called to notify the data that it is leaving the memory cache.
        virtual void aboutToUnload() {}

        DE_CAST_METHODS()
    };

    typedef std::set<String> Names; // alphabetical order

    /**
     * Notified when a data item has been loaded to memory (cache level
     * InMemory). Notification are always called in the main thread.
     */
    DE_AUDIENCE(Load, void bankLoaded(const DotPath &path))

    /**
     * Notified when a data item's cache level changes (in addition to the Load
     * notification). Notification are always called in the main thread.
     */
    DE_AUDIENCE(CacheLevel, void bankCacheLevelChanged(const DotPath &path, CacheLevel level))

public:
    /**
     * Constructs a data bank.
     *
     * @param nameForLog  Name to identify the bank in log entries (latin1).
     * @param flags  Flags that determine the behavior of the bank.
     * @param hotStorageLocation  Location where the hot storage files are kept.
     */
    Bank(const char *nameForLog = "Bank",
         const Flags &flags = DefaultFlags,
         const String &hotStorageLocation = "/home/cache");

    virtual ~Bank();

    const char *nameForLog() const;
    Flags flags() const;

    /**
     * Sets the default separator character for identifiers. Paths added to
     * the bank may use other separators, but when paths get printed to the
     * log, they will use this one.
     *
     * @param sep  Path separator character. The default is a period '.'.
     */
    void setSeparator(Char sep);

    /**
     * Sets the folder where the hot storage (serialized data) is kept. A
     * subfolder structure is created to match the elements of the data items'
     * paths.
     *
     * @param location  Hot storage location.
     */
    void setHotStorageCacheLocation(const String &location);

    /**
     * Sets the maximum amount of data to keep in the hot storage.
     * Default is Unlimited.
     *
     * @param maxBytes  Maximum number of bytes. May also be Unlimited.
     */
    void setHotStorageSize(dint64 maxBytes);

    /**
     * Sets the maximum amount of data to keep in memory. Default is Unlimited.
     *
     * @param maxBytes   Maximum number of bytes. May also be Unlimited.
     */
    void setMemoryCacheSize(dint64 maxBytes);

    Path hotStorageCacheLocation() const;
    dint64 hotStorageSize() const;
    dint64 memoryCacheSize() const;

    /**
     * Deletes the contents of the hot storage folder. The items in the bank are
     * otherwise unaffected.
     */
    void clearHotStorage();

    /**
     * Removes all items and their source information from the bank. This is
     * not the same as unloading the data to a lower cache level.
     */
    void clear();

    /**
     * Adds a new data item to the bank.
     *
     * @param path    Identifier of the data.
     * @param source  Source information that is required for loading the data to
     *                memory. Bank takes ownership.
     */
    void add(const DotPath &path, ISource *source);

    void remove(const DotPath &path);

    /**
     * Determines whether the Bank contains an item (not a folder).
     *
     * @param path  Identifier of the data.
     *
     * @return  @c true or @c false.
     */
    bool has(const DotPath &path) const;

    /**
     * Returns the source of an item.
     *
     * @param path  Identifier of a data item.
     *
     * @return Source.
     */
    ISource &source(const DotPath &path) const;

    /**
     * Iterates through the entire contents of the bank.
     *
     * @param func  Function that gets called with the path of each item.
     */
    void iterate(const std::function<void (const DotPath &)> &func) const;

    /**
     * Collects a list of the paths of all items in the bank.
     *
     * @param names  Names.
     *
     * @return Number of names returned in @a names.
     */
    dint allItems(Names &names) const;

    const PathTree &index() const;

    /**
     * Requests a data item to be loaded. When using BackgroundThread, this is
     * an asynchronous operation. When the data is available, audienceForLoad
     * is notified. Loading is done using the source information specified in
     * the call to add().
     *
     * @param path        Identifier of the data.
     * @param importance  When/how to carry out the load request (with BackgroundThread).
     */
    void load(const DotPath &path, Importance importance = BeforeQueued);

    void loadAll();

    /**
     * Returns the data of an item.
     *
     * If the item is presently not in memory, it will first be loaded (using
     * BeforeQueued; blocks until complete). The data is automatically
     * marked as used at the current time, so it will not leave the memory
     * cache level until sometime in the future.
     *
     * If the caller retains the IData reference for a long time, it is
     * obligated to join audienceForLevelChanged to be notified when the data
     * is removed from the cache.
     *
     * @param path  Identifier of the data.
     *
     * @return IData instance. Ownership kept by the Bank.
     */
    IData &data(const DotPath &path) const;

    /**
     * Determines if an item is currently loaded (InMemory).
     *
     * @param path  Identifier of the item.
     *
     * @return @c true if the item is in memory, @c false otherwise.
     */
    bool isLoaded(const DotPath &path) const;

    /**
     * Moves a data item to a lower cache level. When using BackgroundThread,
     * this is an asynchronous operation. audienceForLevelChanged is notified
     * when the data has been stored.
     *
     * @param path        Identifier of the data.
     * @param toLevel     Destination level for the data.
     * @param importance  Priority for choosing appropriate queued behavior.
     */
    void unload(const DotPath &path, CacheLevel toLevel = InHotStorage,
                Importance importance = AfterQueued);

    /**
     * Moves all data items to a lower cache level. Jobs are done with AfterQueued
     * importance.
     *
     * @param maxLevel  Maximum cache level for all items.
     */
    void unloadAll(CacheLevel maxLevel = InColdStorage);

    void unloadAll(Importance importance, CacheLevel maxLevel = InColdStorage);

    /**
     * Removes an item's cached data from all cache levels.
     *
     * @param path  Identifier of the data.
     */
    void clearFromCache(const DotPath &path);

    /**
     * Moves excess items on each cache level to lower level(s).
     */
    void purge();

protected:
    virtual IData *loadFromSource(ISource &source) = 0;

    /**
     * Construct a new concrete instance of the data item. Called before
     * deserialization. Default implementation just returns NULL (serialization
     * not supported).
     *
     * @return IData instance. Ownership given to caller.
     */
    virtual IData *newData();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_BANK_H
