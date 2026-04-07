/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_LOGBUFFER_H
#define LIBCORE_LOGBUFFER_H

#include "de/log.h"
#include "de/file.h"
#include "de/lockable.h"

namespace de {

class LogEntry;
class LogSink;

/**
 * Central buffer for log entries.
 *
 * Log entries may be created in any thread, and they get collected into a
 * central LogBuffer. The buffer is flushed whenever a new entry triggers the
 * flush condition, which means flushing may occur in any thread.
 *
 * The application owns an instance of LogBuffer.
 *
 * @ingroup core
 */
class DE_PUBLIC LogBuffer : public Lockable
{
public:
    typedef List<const LogEntry *> Entries;

    /**
     * Interface for objects that filter log entries.
     */
    class IFilter
    {
    public:
        virtual ~IFilter() = default;

        /**
         * Determines if a log entry should be allowed into the log buffer
         * if it has a particular set of metadata. Note that this method
         * will be called from several threads, so it needs to be thread-safe.
         *
         * @param metadata  Entry metadata.
         *
         * @return @c true, to allow entering into the buffer.
         */
        virtual bool isLogEntryAllowed(duint32 metadata) const = 0;
    };

public:
    /**
     * Constructs a new log buffer. By default log levels starting with MESSAGE
     * are enabled. Output goes to stdout/stderr.
     *
     * @param maxEntryCount  Maximum number of entries to keep in memory.
     *
     * @see enableStandardOutput()
     */
    LogBuffer(duint maxEntryCount = 1000);

    virtual ~LogBuffer();

    void setMaxEntryCount(duint maxEntryCount);

    /**
     * Adds an entry to the buffer. The buffer gets ownership.
     *
     * @param entry  Entry to add.
     */
    void add(LogEntry *entry);

    /**
     * Clears the buffer by deleting all entries from memory. However, they are
     * first flushed, so the no entries are lost.
     */
    void clear();

    /**
     * Returns the number of entries stored in the buffer.
     */
    dsize size() const;

    /**
     * Returns the latest entries from the buffer. Note that when new entries
     * are added the older entries may be deleted. The entries returned in @a
     * entries should either be used immediately, or copies should be made in
     * the case they're needed later on.
     *
     * @param entries  The entries are placed here. The first entry of the
     *                 array is the latest entry in the buffer.
     * @param count    Number of entries to get. If zero, all entries are
     *                 returned.
     */
    void latestEntries(Entries &entries, int count = 0) const;

    /**
     * Sets the filter that determines if a log entry will be permitted into
     * the buffer.
     *
     * @param entryFilter  Filter object. @c NULL to use the default filter.
     */
    void setEntryFilter(const IFilter *entryFilter);

    /**
     * Checks if a log entry will be enabled if it has a particular set of
     * context metadata bits.
     *
     * @param entryMetadata  Metadata of an entry.
     *
     * @return @c true, if the entry should be added to the buffer. @c false,
     * if the entry should be ignored.
     */
    bool isEnabled(duint32 entryMetadata = LogEntry::Message) const;

    /**
     * Enables or disables standard output of log messages. When enabled,
     * log entries are written with simple formatting to the standard
     * output and error streams when the buffer is flushed.
     *
     * @param yes  @c true or @c false.
     */
    void enableStandardOutput(bool yes = true);

    /**
     * Enables or disables flushing of log messages.
     *
     * @param yes  @c true or @c false.
     */
    void enableFlushing(bool yes = true);

    /**
     * Sets the interval for autoflushing. Also automatically enables flushing.
     *
     * @param interval  Interval for autoflushing.
     */
    void setAutoFlushInterval(TimeSpan interval);

    enum OutputChangeBehavior {
        FlushFirstToOldOutputs,
        DontFlush
    };

    /**
     * Sets the path of the file used for writing log entries to.
     *
     * @param path      Path of the file.
     * @param behavior  What to do with existing unflushed entries.
     */
    void setOutputFile(const String &path,
                       OutputChangeBehavior behavior = FlushFirstToOldOutputs);

    /**
     * Returns the path of the file used for log output.
     */
    String outputFile() const;

    /**
     * Adds a new sink where log entries will be flushed. There can be any
     * number of sinks in use. The sink must not be deleted while it is
     * being used in the log buffer.
     *
     * @param sink  Log sink. Caller retains ownership.
     */
    void addSink(LogSink &sink);

    /**
     * Removes a log sink from use.
     *
     * @param sink  Log sink to remove.
     */
    void removeSink(LogSink &sink);

public:
    /**
     * Sets the application's global log buffer. This is available to all.
     * Ownership is not transferred, so whoever created the buffer is
     * reponsible for deleting it after no one needs the log any more.
     *
     * @param appBuffer  LogBuffer instance.
     */
    static void setAppBuffer(LogBuffer &appBuffer);

    static bool appBufferExists();

    static LogBuffer &get();

    /**
     * Flushes all unflushed entries to the defined outputs.
     */
    void flush();

private:
    DE_PRIVATE(d)

    /// The globally available application buffer.
    static LogBuffer *_appBuffer;
};

} // namespace de

#endif // LIBCORE_LOGBUFFER_H
