/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_LOGBUFFER_H
#define LIBDENG2_LOGBUFFER_H

#include "../Log"
#include "../File"
#include "../Lockable"

#include <QObject>

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
class DENG2_PUBLIC LogBuffer : public QObject, public Lockable, DENG2_OBSERVES(File, Deletion)
{
    Q_OBJECT

public:
    typedef QList<LogEntry const *> Entries;

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
     * Enables log entries at or over a level. When a level is disabled, the
     * entries will not be added to the log entry buffer.
     */
    void enable(LogEntry::Level overLevel = LogEntry::MESSAGE);

    /**
     * Disables the log.
     * @see enable()
     */
    void disable() { enable(LogEntry::MAX_LOG_LEVELS); }

    bool isEnabled(LogEntry::Level overLevel = LogEntry::MESSAGE) const;

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
     * Sets the path of the file used for writing log entries to.
     *
     * @param path  Path of the file.
     */
    void setOutputFile(String const &path);

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

    // File observer.
    void fileBeingDeleted(File const &file);

public:
    /**
     * Sets the application's global log buffer. This is available to all.
     * Ownership is not transferred, so whoever created the buffer is
     * reponsible for deleting it after no one needs the log any more.
     *
     * @param appBuffer  LogBuffer instance.
     */
    static void setAppBuffer(LogBuffer &appBuffer);

    static LogBuffer &appBuffer();

public slots:
    /**
     * Flushes all unflushed entries to the defined outputs.
     */
    void flush();

private:
    struct Instance;
    Instance *d;

    /// The globally available application buffer.
    static LogBuffer *_appBuffer;
};

} // namespace de

#endif // LIBDENG2_LOGBUFFER_H
