/** @file bank.h  Abstract data bank with multi-tiered caching.
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

#ifndef LIBDENG2_BANK_H
#define LIBDENG2_BANK_H

#include <QObject>
#include <set>

#include "../libdeng2.h"
#include "../Path"
#include "../PathTree"
#include "../ISerializable"
#include "../Observers"
#include "../Time"

namespace de {

/**
 * Abstract data bank with multi-tiered caching.
 *
 * Data items are identified using Paths. All data items are required to be
 * serializable; anything that is serializable can be stored in the Bank as
 * log as it derived from Bank::IData.
 *
 * Data is kept cached on multiple levels: in memory, in hot storage
 * (serialized in a file), or in cold storage (unprocessed source data). When a
 * cache level's maximum size is reached, the oldest items are moved to a lower
 * level (age determined by latest time of access).
 *
 * Bank supports both synchronous and asynchronous usage: optionally a
 * background thread can be used for moving items between cache levels.
 *
 * @par Thread-safety
 *
 * When using BackgroundThread, the Bank will perform all heavy lifting in a
 * separate worker thread (loading from source and (de)serialization). However,
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
class DENG2_PUBLIC Bank : public QObject
{
    Q_OBJECT

public:
    /// Failed to load data from the source. @ingroup errors
    DENG2_ERROR(LoadError);

    /// Data in the hot storage has gone stale (older than source). @ingroup errors
    DENG2_ERROR(StaleError);

    enum Flag
    {
        /**
         * Separate thread used for managing the bank's data (loading, caching
         * data). Requires data items and sources to be thread-safe.
         */
        BackgroundThread = 0x1,

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

        DefaultFlags = DisableHotStorage
    };
    Q_DECLARE_FLAGS(Flags, Flag)

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

    enum { Unlimited = -1 };

    /**
     * Interface for specifying the source of a data item.
     */
    class ISource
    {
    public:
        virtual ~ISource() {}

        /**
         * Returns the timestamp of the source data, which specifies when the
         * source data has last been modified. If the source is newer than
         * cached copies, the cached data is discarded. If the returned time is
         * Time::invalidTime(), no time checks are performed and the source
         * data is considered immutable.
         */
        virtual Time modifiedAt() const {
            return Time::invalidTime();
        }
    };

    /**
     * Interface for a data item kept in memory.
     */
    class IData
    {
    public:
        virtual ~IData() {}

        /// Returns an ISerializable pointer to the object. Required
        /// for putting the data in hot storage.
        virtual ISerializable *asSerializable() { return 0; }

        /// Returns the size of the data that it occupies in memory.
        virtual duint sizeInMemory() const = 0;

        /// Called to notify the data that it is leaving the memory cache.
        virtual void aboutToUnload() {}
    };

    typedef std::set<String> Names; // alphabetical order

    /**
     * Notified when a data item has been loaded to memory (cache level
     * InMemory). May be called from the background thread, if one is running.
     */
    DENG2_DEFINE_AUDIENCE(Load, void bankLoaded(Path const &path))

    /**
     * Notified when a data item's cache level changes (in addition to the Load
     * notification).
     */
    DENG2_DEFINE_AUDIENCE(CacheLevel, void bankCacheLevelChanged(Path const &path, CacheLevel level))

public:
    /**
     * Constructs a data bank.
     *
     * @param flags  Flags that determine the behavior of the bank.
     * @param hotStorageLocation  Location where the hot storage files are kept.
     */
    Bank(Flags const &flags = DefaultFlags, String const &hotStorageLocation = "/home/cache");

    virtual ~Bank();

    Flags flags() const;

    /**
     * Sets the folder where the hot storage (serialized data) is kept. A
     * subfolder structure is created to match the elements of the data items'
     * paths.
     *
     * @param location  Hot storage location.
     */
    void setHotStorageCacheLocation(String const &location);

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

    String hotStorageCacheLocation() const;
    dint64 hotStorageSize() const;
    dint64 memoryCacheSize() const;

    /**
     * Removes all items and their source information from the bank. This is
     * not the same as unloading the data to a lower cache level. The data in
     * the hot storage is unaffected.
     */
    void clear();

    /**
     * Adds a new data item to the bank.
     *
     * @param path    Identifier of the data.
     * @param source  Source information that is required for loading the data to
     *                memory. Bank takes ownership.
     */
    void add(Path const &path, ISource *source);

    void remove(Path const &path);

    /**
     * Determines whether the Bank contains an item (not a folder).
     *
     * @param path  Identifier of the data.
     *
     * @return  @c true or @c false.
     */
    bool has(Path const &path) const;

    /**
     * Collects a list of the paths of all items in the bank.
     *
     * @param names  Names.
     *
     * @return Number of names returned in @a names.
     */
    dint allItems(Names &names) const;

    PathTree const &index() const;

    enum LoadImportance {
        LoadImmediately,    ///< Load request handled at the head of the queue.
        LoadAfterQueued     ///< Load request handled at the end of the queue.
    };

    /**
     * Requests a data item to be loaded. When using BackgroundThread, this is
     * an asynchronous operation. When the data is available, audienceForLoad
     * is notified. Loading is done using the source information specified in
     * the call to add().
     *
     * @param path        Identifier of the data.
     * @param importance  When/how to carry out the load request (with BackgroundThread).
     */
    void load(Path const &path, LoadImportance importance = LoadImmediately);

    void loadAll();

    /**
     * Returns the data of an item.
     *
     * If the item is presently not in memory, it will first be loaded (using
     * LoadImmediately; blocks until complete). The data is automatically
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
    IData &data(Path const &path) const;

    /**
     * Moves a data item to a lower cache level. When using BackgroundThread,
     * this is an asynchronous operation. audienceForLevelChanged is notified
     * when the data has been stored.
     *
     * @param path     Identifier of the data.
     * @param toLevel  Destination level for the data.
     */
    void unload(Path const &path, CacheLevel toLevel = InHotStorage);

    /**
     * Moves all data items to a lower cache level.
     *
     * @param maxLevel  Maximum cache level for all items.
     */
    void unloadAll(CacheLevel maxLevel = InColdStorage);

    /**
     * Removes an item's cached data from all cache levels.
     *
     * @param path  Identifier of the data.
     */
    void clearFromCache(Path const &path);

    /**
     * Moves excess items on each cache level to lower level(s).
     */
    void purge();

public slots:
    void performDeferredNotifications();

protected:
    virtual IData *loadFromSource(ISource &source) = 0;

    /**
     * Construct a new concrete instance of the data item.
     * Called before deserialization.
     *
     * @return IData instance. Ownership given to caller.
     */
    virtual IData *newData() = 0;

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Bank::Flags)

} // namespace de

#endif // LIBDENG2_BANK_H
