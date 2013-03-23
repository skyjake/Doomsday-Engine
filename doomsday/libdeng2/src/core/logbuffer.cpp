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

#include "de/LogBuffer"
#include "de/LogSink"
#include "de/FileLogSink"
#include "de/DebugLogSink"
#include "de/TextStreamLogSink"
#include "de/Writer"
#include "de/FixedByteArray"
#include "de/Guard"
#include "de/App"

#include <stdio.h>
#include <QTextStream>
#include <QCoreApplication>
#include <QList>
#include <QSet>
#include <QTimer>
#include <QDebug>

namespace de {

TimeDelta const FLUSH_INTERVAL = .2; // seconds

DENG2_PIMPL_NOREF(LogBuffer)
{
    typedef QList<LogEntry *> EntryList;
    typedef QSet<LogSink *> Sinks;

    dint enabledOverLevel;
    dint maxEntryCount;
    bool useStandardOutput;
    bool flushingEnabled;
    File *outputFile;
    FileLogSink *fileLogSink;
#ifndef WIN32
    TextStreamLogSink outSink;
    TextStreamLogSink errSink;
#else
    DebugLogSink outSink;
    DebugLogSink errSink;
#endif
    EntryList entries;
    EntryList toBeFlushed;
    Time lastFlushedAt;
    QTimer *autoFlushTimer;
    Sinks sinks;

    Instance(duint maxEntryCount)
        : enabledOverLevel(LogEntry::MESSAGE),
          maxEntryCount(maxEntryCount),
          useStandardOutput(true),
          flushingEnabled(true),
          outputFile(0),
          fileLogSink(0),
#ifndef WIN32
          outSink(new QTextStream(stdout)),
          errSink(new QTextStream(stderr)),
#else
          // Windows GUI apps don't have stdout/stderr.
          outSink(QtDebugMsg),
          errSink(QtWarningMsg),
#endif
          autoFlushTimer(0)
    {
        // Standard output enabled by default.
        outSink.setMode(LogSink::OnlyNormalEntries);
        errSink.setMode(LogSink::OnlyWarningEntries);

        sinks.insert(&outSink);
        sinks.insert(&errSink);
    }

    ~Instance()
    {
        if(autoFlushTimer) autoFlushTimer->stop();
        delete fileLogSink;
    }

    void disposeFileLogSink()
    {
        if(fileLogSink)
        {
            sinks.remove(fileLogSink);
            delete fileLogSink;
            fileLogSink = 0;
        }
    }
};

LogBuffer *LogBuffer::_appBuffer = 0;

LogBuffer::LogBuffer(duint maxEntryCount) 
    : d(new Instance(maxEntryCount))
{
    d->autoFlushTimer = new QTimer(this);
    connect(d->autoFlushTimer, SIGNAL(timeout()), this, SLOT(flush()));
}

LogBuffer::~LogBuffer()
{
    DENG2_GUARD(this);

    setOutputFile("");
    clear();

    if(_appBuffer == this) _appBuffer = 0;
}

void LogBuffer::clear()
{
    DENG2_GUARD(this);

    // Flush first, we don't want to miss any messages.
    flush();

    DENG2_FOR_EACH(Instance::EntryList, i, d->entries)
    {
        delete *i;
    }
    d->entries.clear();
}

dsize LogBuffer::size() const
{
    DENG2_GUARD(this);
    return d->entries.size();
}

void LogBuffer::latestEntries(Entries &entries, int count) const
{
    DENG2_GUARD(this);
    entries.clear();
    for(int i = d->entries.size() - 1; i >= 0; --i)
    {
        entries.append(d->entries[i]);
        if(count && entries.size() >= count)
        {
            return;
        }
    }
}

void LogBuffer::setMaxEntryCount(duint maxEntryCount)
{
    d->maxEntryCount = maxEntryCount;
}

void LogBuffer::add(LogEntry *entry)
{       
    DENG2_GUARD(this);

    // We will not flush the new entry as it likely has not yet been given
    // all its arguments.
    if(d->lastFlushedAt.since() > FLUSH_INTERVAL)
    {
        flush();
    }

    d->entries.push_back(entry);
    d->toBeFlushed.push_back(entry);

    // Should we start autoflush?
    if(!d->autoFlushTimer->isActive() && qApp)
    {
        // Every now and then the buffer will be flushed.
        d->autoFlushTimer->start(FLUSH_INTERVAL * 1000);
    }
}

void LogBuffer::enable(LogEntry::Level overLevel)
{
    d->enabledOverLevel = overLevel;
}

bool LogBuffer::isEnabled(LogEntry::Level overLevel) const
{
    return d->enabledOverLevel <= overLevel;
}

void LogBuffer::enableStandardOutput(bool yes)
{
    DENG2_GUARD(this);

    d->useStandardOutput = yes;

    d->outSink.setMode(yes? LogSink::OnlyNormalEntries  : LogSink::Disabled);
    d->errSink.setMode(yes? LogSink::OnlyWarningEntries : LogSink::Disabled);
}

void LogBuffer::enableFlushing(bool yes)
{
    d->flushingEnabled = yes;
}

void LogBuffer::setOutputFile(String const &path)
{
    DENG2_GUARD(this);

    flush();
    d->disposeFileLogSink();

    if(d->outputFile)
    {
        d->outputFile->audienceForDeletion -= this;
        d->outputFile = 0;
    }

    if(!path.isEmpty())
    {
        d->outputFile = &App::rootFolder().replaceFile(path);
        d->outputFile->setMode(File::Write);
        d->outputFile->audienceForDeletion += this;

        // Add a sink for the file.
        d->fileLogSink = new FileLogSink(*d->outputFile);
        d->sinks.insert(d->fileLogSink);
    }
}

void LogBuffer::addSink(LogSink &sink)
{
    DENG2_GUARD(this);

    d->sinks.insert(&sink);
}

void LogBuffer::removeSink(LogSink &sink)
{
    DENG2_GUARD(this);

    d->sinks.remove(&sink);
}

void LogBuffer::flush()
{
    if(!d->flushingEnabled) return;

    DENG2_GUARD(this);

    if(!d->toBeFlushed.isEmpty())
    {
        try
        {
            DENG2_FOR_EACH(Instance::EntryList, i, d->toBeFlushed)
            {
                DENG2_GUARD_FOR(**i, guardingCurrentLogEntry);

                foreach(LogSink *sink, d->sinks)
                {
                    if(sink->willAccept(**i))
                    {
                        *sink << **i;
                    }
                }
            }
        }
        catch(Error const &error)
        {
            foreach(LogSink *sink, d->sinks)
            {
                *sink << "Exception during log flush:\n" << error.what();
            }
        }

        d->toBeFlushed.clear();

        // Make sure everything really gets written now.
        foreach(LogSink *sink, d->sinks) sink->flush();
    }

    d->lastFlushedAt = Time();

    // Too many entries? Now they can be destroyed since we have flushed everything.
    while(d->entries.size() > d->maxEntryCount)
    {
        LogEntry *old = d->entries.front();
        d->entries.pop_front();
        delete old;
    }
}

void LogBuffer::fileBeingDeleted(File const &file)
{
    DENG2_ASSERT(d->outputFile == &file);
    DENG2_UNUSED(file);

    flush();
    d->disposeFileLogSink();
    d->outputFile = 0;   
}

void LogBuffer::setAppBuffer(LogBuffer &appBuffer)
{
    _appBuffer = &appBuffer;
}

LogBuffer &LogBuffer::appBuffer()
{
    DENG2_ASSERT(_appBuffer != 0);
    return *_appBuffer;
}

bool LogBuffer::isAppBufferAvailable()
{
    return _appBuffer != 0;
}

} // namespace de
