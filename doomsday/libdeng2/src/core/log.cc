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

#include "de/Log"
#include "de/Time"
#include "de/Date"
#include "de/TextStyle"
#include "de/App"
#include "de/math.h"
#include "../sdl.h"

#include <cstdlib>
#include <cstring>
#include <map>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace de;

const Time::Delta FLUSH_INTERVAL = .1;
const char* MAIN_SECTION = "";
const duint SIMPLE_INDENT = 29;

/**
 * The logs table is lockable so that multiple threads can access their
 * logs at the same time.
 */
class Logs : public std::map<duint, Log*>, public Lockable
{
public:
    Logs() {}
    ~Logs() {
        // The logs are owned by the logs table.
        for(iterator i = begin(); i != end(); ++i) {
            delete i->second;
        }
    }
};

/// The logs table contains the log of each thread that uses logging.
static Logs logs;

LogEntry::LogEntry(LogBuffer::Level level, const String& section, const String& format)
    : logBuffer_(App::logBuffer()), level_(level), section_(section), format_(format)
{}

LogEntry::~LogEntry()
{
    for(Args::iterator i = args_.begin(); i != args_.end(); ++i) 
    {
        delete *i;
    }    
}

String LogEntry::asText(const Flags& _flags) const
{
    Flags flags = _flags;
    std::ostringstream output;
    
    if(defaultFlags_[SIMPLE_BIT])
    {
        flags |= SIMPLE;
    }
    
    // In simple mode, skip the metadata.
    if(!flags[SIMPLE_BIT])
    {    
        // Begin with the timestamp.
        if(flags[STYLED_BIT]) output << TEXT_STYLE_LOG_TIME;
    
        output << Date(when_) << " ";

        if(!flags[STYLED_BIT])
        {
            const char* levelNames[LogBuffer::MAX_LEVELS] = {
                "(...)",
                "(bug)",
                "(vbs)",
                "",
                "(inf)",
                "(WRN)",
                "(ERR)",
                "(!!!)"        
            };
            output << std::setfill(' ') << std::setw(5) << levelNames[level_] << " ";
        }
        else
        {
            const char* levelNames[LogBuffer::MAX_LEVELS] = {
                "Trace",
                "Debug",
                "Verbose",
                "",
                "Info",
                "Warning",
                "ERROR",
                "FATAL!"        
            };
            output << "\t" 
                << (level_ >= LogBuffer::WARNING? TEXT_STYLE_LOG_BAD_LEVEL : TEXT_STYLE_LOG_LEVEL)
                << levelNames[level_] << "\t\r";
        }
    
        // Section name.
        if(!section_.empty())
        {
            if(!flags[STYLED_BIT])
            {
                output << "[" << section_ << "]: ";
            }
            else
            {
                output << TEXT_STYLE_SECTION << section_ << ": ";
            }
        }
    }

    if(flags[STYLED_BIT])
    {
        output << TEXT_STYLE_MESSAGE;
    }
    
    // Message text with the arguments formatted.
    if(args_.empty())
    {
        // Just verbatim.
        output << format_;
    }
    else
    {
        Args::const_iterator arg = args_.begin();
        
        for(String::const_iterator i = format_.begin(); i != format_.end(); ++i)
        {
            if(*i == '%')
            {
                if(arg == args_.end())
                {
                    // Out of args.
                    throw IllegalFormatError("LogEntry::asText", "Ran out of arguments");
                }
                                
                output << String::patternFormat(i, format_.end(), **arg);
                ++arg;
            }
            else
            {
                output << *i;
            }
        }
        
        // Just append the rest of the arguments without special instructions.
        for(; arg != args_.end(); ++arg)
        {
            output << **arg;
        }        
    }
    
    if(flags[STYLED_BIT])
    {
        output << TEXT_STYLE_MESSAGE;
    }
    
    return output.str();
}

std::ostream& de::operator << (std::ostream& stream, const LogEntry::Arg& arg)
{
    switch(arg.type())
    {
    case LogEntry::Arg::INTEGER:
        stream << arg.intValue();
        break;
        
    case LogEntry::Arg::FLOATING_POINT:
        stream << arg.floatValue();
        break;
        
    case LogEntry::Arg::STRING:
        stream << arg.stringValue();
        break;
        
    case LogEntry::Arg::WIDE_STRING:
        stream << String::wideToString(arg.wideStringValue());
        break;
    }
    return stream;
}

Log::Section::Section(const char* name) : log_(threadLog()), name_(name)
{
    log_.beginSection(name_);
}

Log::Section::~Section()
{
    log_.endSection(name_);
}

Log::Log() : throwawayEntry_(0)
{
    throwawayEntry_ = new LogEntry(LogBuffer::MESSAGE, "", "");
    sectionStack_.push_back(MAIN_SECTION);
}

Log::~Log()
{
    delete throwawayEntry_;
}

void Log::beginSection(const char* name)
{
    sectionStack_.push_back(name);
}

void Log::endSection(const char* name)
{
    assert(sectionStack_.back() == name);
    sectionStack_.pop_back();
}

LogEntry& Log::enter(const String& format)
{
    return enter(LogBuffer::MESSAGE, format);
}

LogEntry& Log::enter(LogBuffer::Level level, const String& format)
{
    if(!App::logBuffer().enabled(level))
    {
        // If the level is disabled, no messages are entered into it.
        return *throwawayEntry_;
    }
    
    // Collect the sections.
    String context;
    String latest;
    for(SectionStack::iterator i = sectionStack_.begin(); i != sectionStack_.end(); ++i)
    {
        if(*i == latest)
        {
            // Don't repeat if it has the exact same name (due to recursive calls).
            continue;
        }
        if(context.size())
        {
            context += " > ";
        }
        latest = *i;
        context += *i;
    }

    // Make a new entry.
    LogEntry* entry = new LogEntry(level, context, format);
    
    // Add it to the application's buffer. The buffer gets ownership.
    App::logBuffer().add(entry);
    
    return *entry;
}

Log& Log::threadLog()
{
    // Each thread has its own log.
    duint thread = SDL_ThreadID();
    Log* theLog = 0;
    logs.lock();
    Logs::iterator found = logs.find(thread);
    if(found == logs.end())
    {
        // Create a new log.
        theLog = new Log();
        logs[thread] = theLog;
    }
    else
    {
        theLog = found->second;
    }
    logs.unlock();
    return *theLog;
}

void Log::disposeThreadLog()
{
    duint thread = SDL_ThreadID();
    logs.lock();
    Logs::iterator found = logs.find(thread);
    if(found != logs.end())
    {
        delete found->second;
        logs.erase(found);
    }
    logs.unlock();
}

LogBuffer::LogBuffer(duint maxEntryCount) : enabledOverLevel_(MESSAGE), maxEntryCount_(maxEntryCount)
{}

LogBuffer::~LogBuffer()
{
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

void LogBuffer::enable(Level overLevel)
{
    enabledOverLevel_ = overLevel;
}

void LogBuffer::flush()
{
    for(Entries::iterator i = toBeFlushed_.begin(); i != toBeFlushed_.end(); ++i)
    {
        if(standardOutput_)
        {
            std::ostream* os = ((*i)->level() >= ERROR? &std::cerr : &std::cout);
            String message = (*i)->asText();
            
            // Print line by line.
            String::size_type pos = 0;
            while(pos != String::npos)
            {
                String::size_type next = message.find('\n', pos);
                if(pos > 0)
                {
                    *os << std::setw(SIMPLE_INDENT) << "";
                }
                *os << message.substr(pos, next != String::npos? next - pos + 1 : next);
                pos = next;
                if(pos != String::npos) pos++;
            }
            *os << "\n";
        }
    }
    toBeFlushed_.clear();
    lastFlushedAt_ = Time();
    
    // Too many entries? Now they can be destroyed since we have flushed everything.
    while(entries_.size() > maxEntryCount_)
    {
        LogEntry* old = entries_.front();
        entries_.pop_front();
        delete old;
    }
}
