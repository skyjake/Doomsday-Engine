/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/App"
#include "de/Folder"
#include "de/Writer"
#include "de/FixedByteArray"

#include <stdio.h>
#include <QTextStream>
#include <QCoreApplication>
#include <QTimer>

using namespace de;

const Time::Delta FLUSH_INTERVAL = .2;
const duint SIMPLE_INDENT = 30;
const duint RULER_LENGTH = 98 - SIMPLE_INDENT;

LogBuffer::LogBuffer(duint maxEntryCount) 
    : _enabledOverLevel(Log::MESSAGE), 
      _maxEntryCount(maxEntryCount),
      _standardOutput(false),
      _outputFile(0),
      _autoFlushTimer(0)
{}

LogBuffer::~LogBuffer()
{
    setOutputFile("");
    clear();
}

void LogBuffer::clear()
{
    flush();
    FOR_EACH(i, _entries, EntryList::iterator)
    {
        delete *i;
    }
    _entries.clear();
}

dsize LogBuffer::size() const
{
    return _entries.size();
}

void LogBuffer::latestEntries(Entries& entries, int count) const
{
    entries.clear();
    for(int i = _entries.size() - 1; i >= 0; --i)
    {
        entries.append(_entries[i]);
        if(count && entries.size() >= count)
        {
            return;
        }
    }
}

void LogBuffer::setMaxEntryCount(duint maxEntryCount)
{
    _maxEntryCount = maxEntryCount;
}

void LogBuffer::add(LogEntry* entry)
{       
    // We will not flush the new entry as it likely has not yet been given
    // all its arguments.
    if(_lastFlushedAt.since() > FLUSH_INTERVAL)
    {
        flush();
    }

    _entries.push_back(entry);
    _toBeFlushed.push_back(entry);

    // Should we start autoflush?
    if(!_autoFlushTimer && qApp)
    {
        // Every now and then the buffer will be flushed.
        _autoFlushTimer = new QTimer(this);
        connect(_autoFlushTimer, SIGNAL(timeout()), this, SLOT(flush()));
        _autoFlushTimer->start(FLUSH_INTERVAL * 1000);
    }
}

void LogBuffer::enable(Log::LogLevel overLevel)
{
    _enabledOverLevel = overLevel;
}

void LogBuffer::setOutputFile(const String& path)
{
    if(_outputFile)
    {
        flush();
        _outputFile->audienceForDeletion.remove(this);
        _outputFile = 0;
    }
    if(!path.empty())
    {
        _outputFile = &App::fileRoot().replaceFile(path);
        _outputFile->setMode(File::Write);
        _outputFile->audienceForDeletion.add(this);
    }
}

void LogBuffer::flush()
{
    if(!_toBeFlushed.isEmpty())
    {
        Writer* writer = 0;
        if(_outputFile)
        {
            // We will add to the end.
            writer = new Writer(*_outputFile, _outputFile->size());
        }

        FOR_EACH(i, _toBeFlushed, EntryList::iterator)
        {
            // Error messages will go to stderr instead of stdout.
            QScopedPointer<QTextStream> os(_standardOutput?
                                           ((*i)->level() >= Log::ERROR? new QTextStream(stderr) :
                                            new QTextStream(stdout)) : 0);

            String message = (*i)->asText();

            // Print line by line.
            String::size_type pos = 0;
            while(pos != String::npos)
            {
                String::size_type next = message.indexOf('\n', pos);
                if(pos > 0)
                {
                    if(os)
                    {
                        *os << qSetFieldWidth(SIMPLE_INDENT) << "" << qSetFieldWidth(0);
                    }
                    if(writer)
                    {
                        *writer << FixedByteArray(String(SIMPLE_INDENT, ' ').toUtf8());
                    }
                }
                String lineText = message.substr(pos, next != String::npos? next - pos + 1 : next);

                // Check for formatting symbols.
                if(lineText == "$R")
                {
                    lineText = String(RULER_LENGTH, '-');
                }

                if(os)
                {
                    *os << lineText;
                }
                if(writer)
                {
                    *writer << FixedByteArray(lineText.toUtf8());
                }
                pos = next;
                if(pos != String::npos) pos++;
            }

            if(os)
            {
                *os << "\n";
            }
            if(writer)
            {
                *writer << FixedByteArray(String("\n").toUtf8());
            }
        }
        delete writer;

        _toBeFlushed.clear();

        if(_outputFile)
        {
            // Make sure they get written now.
            _outputFile->flush();
        }
    }

    _lastFlushedAt = Time();

    // Too many entries? Now they can be destroyed since we have flushed everything.
    while(_entries.size() > _maxEntryCount)
    {
        LogEntry* old = _entries.front();
        _entries.pop_front();
        delete old;
    }
}

void LogBuffer::fileBeingDeleted(const File& file)
{
    Q_ASSERT(_outputFile == &file);
    flush();
    _outputFile = 0;
}
