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

LogBuffer::LogBuffer(duint maxEntryCount) 
    : enabledOverLevel_(MESSAGE), 
      maxEntryCount_(maxEntryCount),
      standardOutput_(false),
      outputFile_(0)
{}

LogBuffer::~LogBuffer()
{
    setOutputFile("");
    clear();
}

void LogBuffer::clear()
{
    flush();
    for(Entries::iterator i = entries_.begin(); i != entries_.end(); ++i)
    {
        delete *i;
    }
    entries_.clear();
}

void LogBuffer::setMaxEntryCount(duint maxEntryCount)
{
    maxEntryCount_ = maxEntryCount;
}

void LogBuffer::add(LogEntry* entry)
{
    // We will not flush the new entry as it likely has not yet been given
    // all its arguments.
    if(lastFlushedAt_.since() > FLUSH_INTERVAL)
    {
        flush();
    }

    entries_.push_back(entry);
    toBeFlushed_.push_back(entry);
}

void LogBuffer::enable(LogLevel overLevel)
{
    enabledOverLevel_ = overLevel;
}

void LogBuffer::setOutputFile(const String& path)
{
    if(outputFile_)
    {
        flush();
        outputFile_->audienceForDeletion.remove(this);
        outputFile_ = 0;
    }
    if(!path.empty())
    {
        outputFile_ = &App::fileRoot().replaceFile(path);
        outputFile_->setMode(File::WRITE);
        outputFile_->audienceForDeletion.add(this);
    }
}

void LogBuffer::flush()
{
    Writer* writer = 0;
    if(outputFile_)
    {
        // We will add to the end.
        writer = new Writer(*outputFile_, outputFile_->size());
    }
    
    for(Entries::iterator i = toBeFlushed_.begin(); i != toBeFlushed_.end(); ++i)
    {
        // Error messages will go to stderr instead of stdout.
        std::ostream* os = (standardOutput_? ((*i)->level() >= ERROR? &std::cerr : &std::cout) : 0);
        
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
    toBeFlushed_.clear();
    lastFlushedAt_ = Time();

    delete writer;
    
    // Too many entries? Now they can be destroyed since we have flushed everything.
    while(entries_.size() > maxEntryCount_)
    {
        LogEntry* old = entries_.front();
        entries_.pop_front();
        delete old;
    }
}

void LogBuffer::fileBeingDeleted(const File& file)
{
    assert(outputFile_ == &file);
    flush();
    outputFile_ = 0;
}
