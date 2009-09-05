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

#include <iostream>
#include <iomanip>

using namespace de;

const Time::Delta FLUSH_INTERVAL = .1;
const duint SIMPLE_INDENT = 29;
const duint RULER_LENGTH = 98 - SIMPLE_INDENT;

LogBuffer::LogBuffer(duint maxEntryCount) 
    : _enabledOverLevel(MESSAGE), 
      _maxEntryCount(maxEntryCount),
      _standardOutput(false),
      _outputFile(0)
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

void LogBuffer::latestEntries(Entries& entries, dsize count) const
{
    entries.clear();
    FOR_EACH_REVERSE(i, _entries, EntryList::const_reverse_iterator)
    {
        entries.push_back(*i);
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
}

void LogBuffer::enable(LogLevel overLevel)
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
        _outputFile->setMode(File::WRITE);
        _outputFile->audienceForDeletion.add(this);
    }
}

void LogBuffer::flush()
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
        std::ostream* os = (_standardOutput? ((*i)->level() >= ERROR? &std::cerr : &std::cout) : 0);
        
        String message = (*i)->asText();
            
        // Print line by line.
        String::size_type pos = 0;
        while(pos != String::npos)
        {
            String::size_type next = message.find('\n', pos);
            if(pos > 0)
            {
                if(os)
                {
                    *os << std::setw(SIMPLE_INDENT) << "";
                }
                if(writer)
                {
                    *writer << FixedByteArray(String(SIMPLE_INDENT, ' '));
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
                *writer << FixedByteArray(lineText);
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
            *writer << FixedByteArray(String("\n"));
        }
    }
    _toBeFlushed.clear();
    _lastFlushedAt = Time();

    delete writer;
    
    if(_outputFile)
    {
        // Make sure they get written now.
        _outputFile->flush();
    }
    
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
    assert(_outputFile == &file);
    flush();
    _outputFile = 0;
}
