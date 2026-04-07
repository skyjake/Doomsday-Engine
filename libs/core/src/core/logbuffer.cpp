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

#include "de/logbuffer.h"
#include "de/app.h"
#include "de/debuglogsink.h"
#include "de/filelogsink.h"
#include "de/fixedbytearray.h"
#include "de/folder.h"
#include "de/guard.h"
#include "de/logsink.h"
#include "de/logfilter.h"
#include "de/textstreamlogsink.h"
#include "de/timer.h"
#include "de/writer.h"

#include <iostream>

namespace de {

const TimeSpan FLUSH_INTERVAL = .2; // seconds

DE_PIMPL(LogBuffer)
{
    typedef List<LogEntry *> EntryList;
    typedef Set<LogSink *> Sinks;

    SimpleLogFilter defaultFilter;
    const IFilter *entryFilter;
    dint maxEntryCount;
    bool useStandardOutput;
    bool flushingEnabled;
    String outputPath;
    FileLogSink *fileLogSink;
//#ifndef WIN32
    TextStreamLogSink outSink;
    TextStreamLogSink errSink;
//#else
//    DebugLogSink outSink;
//    DebugLogSink errSink;
//#endif
    EntryList entries;
    EntryList toBeFlushed;
    Time lastFlushedAt;
    std::unique_ptr<Timer> autoFlushTimer;
    Sinks sinks;

    Impl(Public *i, duint maxEntryCount)
        : Base(i)
        , entryFilter(&defaultFilter)
        , maxEntryCount(maxEntryCount)
        , useStandardOutput(true)
        , flushingEnabled(true)
        , fileLogSink(nullptr)
//#ifndef WIN32
        , outSink(std::cout)
        , errSink(std::cerr)
//#else
          // Windows GUI apps don't have stdout/stderr.
//        , outSink(QtDebugMsg)
//        , errSink(QtWarningMsg)
//#endif
        , lastFlushedAt(Time::invalidTime())
    {
        // Standard output enabled by default.
        outSink.setMode(LogSink::OnlyNormalEntries);
        errSink.setMode(LogSink::OnlyWarningEntries);

        sinks.insert(&outSink);
        sinks.insert(&errSink);
    }

    ~Impl()
    {
        if (autoFlushTimer) autoFlushTimer->stop();
        delete fileLogSink;
    }

    void enableAutoFlush(bool yes)
    {
        DE_ASSERT(App::appExists());
        if (yes)
        {
            if (!autoFlushTimer)
            {
                autoFlushTimer.reset(new Timer);
                *autoFlushTimer += [this]() { self().flush(); };
            }
            if (!autoFlushTimer->isActive())
            {
                // Every now and then the buffer will be flushed.
                autoFlushTimer->start(FLUSH_INTERVAL);
            }
        }
        else
        {
            autoFlushTimer->stop();
        }
    }

    void createFileLogSink(bool truncate)
    {
        if (!outputPath.isEmpty())
        {
            File *outputFile = nullptr;
            if (!truncate)
            {
                // Try to use the existing file.
                outputFile = App::rootFolder().tryLocate<File>(outputPath);
            }
            if (!outputFile)
            {
                // Start an empty log file.
                outputFile = &App::rootFolder().replaceFile(outputPath);
            }
            DE_ASSERT(outputFile);

            // Add a sink for the file.
            DE_ASSERT(!fileLogSink);
            fileLogSink = new FileLogSink(*outputFile);
            sinks.insert(fileLogSink);
        }
    }

    void disposeFileLogSink()
    {
        if (fileLogSink)
        {
            sinks.remove(fileLogSink);
            delete fileLogSink;
            fileLogSink = nullptr;
        }
    }
};

LogBuffer *LogBuffer::_appBuffer = nullptr;

LogBuffer::LogBuffer(duint maxEntryCount)
    : d(new Impl(this, maxEntryCount))
{}

LogBuffer::~LogBuffer()
{
    DE_GUARD(this);

    setOutputFile("");
    clear();

    if (_appBuffer == this) _appBuffer = nullptr;
}

void LogBuffer::clear()
{
    DE_GUARD(this);

    // Flush first, we don't want to miss any messages.
    flush();

    DE_FOR_EACH(Impl::EntryList, i, d->entries)
    {
        delete *i;
    }
    d->entries.clear();
}

dsize LogBuffer::size() const
{
    DE_GUARD(this);
    return d->entries.size();
}

void LogBuffer::latestEntries(Entries &entries, int count) const
{
    DE_GUARD(this);
    entries.clear();
    for (int i = d->entries.sizei() - 1; i >= 0; --i)
    {
        entries.append(d->entries[i]);
        if (count && entries.sizei() >= count)
        {
            return;
        }
    }
}

void LogBuffer::setEntryFilter(const IFilter *entryFilter)
{
    if (entryFilter)
    {
        d->entryFilter = entryFilter;
    }
    else
    {
        d->entryFilter = &d->defaultFilter;
    }
}

bool LogBuffer::isEnabled(duint32 entryMetadata) const
{
    DE_ASSERT(d->entryFilter != nullptr);
    DE_ASSERT(entryMetadata & LogEntry::DomainMask); // must have a domain
    if (entryMetadata & LogEntry::Privileged)
    {
        return true; // always passes
    }
    return d->entryFilter->isLogEntryAllowed(entryMetadata);
}

void LogBuffer::setMaxEntryCount(duint maxEntryCount)
{
    d->maxEntryCount = maxEntryCount;
}

void LogBuffer::add(LogEntry *entry)
{
    DE_GUARD(this);

    // We will not flush the new entry as it likely has not yet been given
    // all its arguments.
    if (d->lastFlushedAt.isValid() && d->lastFlushedAt.since() > FLUSH_INTERVAL)
    {
        flush();
    }

    d->entries.push_back(entry);
    d->toBeFlushed.push_back(entry);
}

void LogBuffer::enableStandardOutput(bool yes)
{
    DE_GUARD(this);

    d->useStandardOutput = yes;

    d->outSink.setMode(yes? LogSink::OnlyNormalEntries  : LogSink::Disabled);
    d->errSink.setMode(yes? LogSink::OnlyWarningEntries : LogSink::Disabled);
}

void LogBuffer::enableFlushing(bool yes)
{
    d->flushingEnabled = yes;
    d->enableAutoFlush(true);
}

void LogBuffer::setAutoFlushInterval(TimeSpan interval)
{
    enableFlushing();
    DE_ASSERT(d->autoFlushTimer);
    d->autoFlushTimer->setInterval(interval);
}

void LogBuffer::setOutputFile(const String &path, OutputChangeBehavior behavior)
{
    DE_GUARD(this);

    if (behavior == FlushFirstToOldOutputs)
    {
        flush();
    }

    d->disposeFileLogSink();
    d->outputPath = path;
    d->createFileLogSink(true /* truncated */);
}

String LogBuffer::outputFile() const
{
    return d->outputPath;
}

void LogBuffer::addSink(LogSink &sink)
{
    DE_GUARD(this);

    d->sinks.insert(&sink);
}

void LogBuffer::removeSink(LogSink &sink)
{
    DE_GUARD(this);

    d->sinks.remove(&sink);
}

void LogBuffer::flush()
{
    if (!d->flushingEnabled) return;

    DE_GUARD(this);

    if (!d->toBeFlushed.isEmpty())
    {
        for (const auto *entry : d->toBeFlushed)
        {
            DE_GUARD_FOR(*entry, guardingCurrentLogEntry);
            for (LogSink *sink : d->sinks)
            {
                if (sink->willAccept(*entry))
                {
                    try
                    {
                        *sink << *entry;
                    }
                    catch (const Error &error)
                    {
                        *sink << String("Exception during log flush:\n") +
                                        error.what() + "\n(the entry format is: '" +
                                        entry->format() + "')";
                    }
                }
            }
        }
        d->toBeFlushed.clear();

        // Make sure everything really gets written now.
        for (LogSink *sink : d->sinks) sink->flush();
    }

    d->lastFlushedAt = Time();

    // Too many entries? Now they can be destroyed since we have flushed everything.
    while (d->entries.sizei() > d->maxEntryCount)
    {
        LogEntry *old = d->entries.front();
        d->entries.pop_front();
        delete old;
    }
}

void LogBuffer::setAppBuffer(LogBuffer &appBuffer)
{
    _appBuffer = &appBuffer;
}

LogBuffer &LogBuffer::get()
{
    DE_ASSERT(_appBuffer != nullptr);
    return *_appBuffer;
}

bool LogBuffer::appBufferExists()
{
    return _appBuffer != nullptr;
}

} // namespace de
