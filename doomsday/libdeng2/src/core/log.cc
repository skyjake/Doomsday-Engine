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
#include "de/LogBuffer"
#include "de/math.h"
#include "../sdl.h"

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <map>

using namespace de;

const char* MAIN_SECTION = "";

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

LogEntry::LogEntry(LogLevel level, const String& section, const String& format)
    : _level(level), _section(section), _format(format), _disabled(false)
{
    if(!App::logBuffer().enabled(level))
    {
        _disabled = true;
    }
}

LogEntry::~LogEntry()
{
    for(Args::iterator i = _args.begin(); i != _args.end(); ++i) 
    {
        delete *i;
    }    
}

String LogEntry::asText(const Flags& formattingFlags) const
{
    Flags flags = formattingFlags;
    std::ostringstream output;
    
    if(_defaultFlags[SIMPLE_BIT])
    {
        flags |= SIMPLE;
    }
    
    // In simple mode, skip the metadata.
    if(!flags[SIMPLE_BIT])
    {    
        // Begin with the timestamp.
        if(flags[STYLED_BIT]) output << TEXT_STYLE_LOG_TIME;
    
        output << Date(_when) << " ";

        if(!flags[STYLED_BIT])
        {
            const char* levelNames[MAX_LOG_LEVELS] = {
                "(...)",
                "(bug)",
                "(vbs)",
                "",
                "(inf)",
                "(WRN)",
                "(ERR)",
                "(!!!)"        
            };
            output << std::setfill(' ') << std::setw(5) << levelNames[_level] << " ";
        }
        else
        {
            const char* levelNames[MAX_LOG_LEVELS] = {
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
                << (_level >= WARNING? TEXT_STYLE_LOG_BAD_LEVEL : TEXT_STYLE_LOG_LEVEL)
                << levelNames[_level] << "\t\r";
        }
    }

    // Section name.
    if(!_section.empty())
    {
        if(!flags[STYLED_BIT])
        {
            output << _section << ": ";
        }
        else
        {
            output << TEXT_STYLE_SECTION << _section << ": ";
        }
    }

    if(flags[STYLED_BIT])
    {
        output << TEXT_STYLE_MESSAGE;
    }
    
    // Message text with the arguments formatted.
    if(_args.empty())
    {
        // Just verbatim.
        output << _format;
    }
    else
    {
        Args::const_iterator arg = _args.begin();
        
        for(String::const_iterator i = _format.begin(); i != _format.end(); ++i)
        {
            if(*i == '%')
            {
                if(arg == _args.end())
                {
                    // Out of args.
                    throw IllegalFormatError("LogEntry::asText", "Ran out of arguments");
                }
                                
                output << String::patternFormat(i, _format.end(), **arg);
                ++arg;
            }
            else
            {
                output << *i;
            }
        }
        
        // Just append the rest of the arguments without special instructions.
        for(; arg != _args.end(); ++arg)
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

Log::Section::Section(const char* name) : _log(threadLog()), _name(name)
{
    _log.beginSection(_name);
}

Log::Section::~Section()
{
    _log.endSection(_name);
}

Log::Log() : _throwawayEntry(0)
{
    _throwawayEntry = new LogEntry(MESSAGE, "", "");
    _sectionStack.push_back(MAIN_SECTION);
}

Log::~Log()
{
    delete _throwawayEntry;
}

void Log::beginSection(const char* name)
{
    _sectionStack.push_back(name);
}

void Log::endSection(const char* name)
{
    assert(_sectionStack.back() == name);
    _sectionStack.pop_back();
}

LogEntry& Log::enter(const String& format)
{
    return enter(MESSAGE, format);
}

LogEntry& Log::enter(LogLevel level, const String& format)
{
    if(!App::logBuffer().enabled(level))
    {
        // If the level is disabled, no messages are entered into it.
        return *_throwawayEntry;
    }
    
    // Collect the sections.
    String context;
    String latest;
    for(SectionStack::iterator i = _sectionStack.begin(); i != _sectionStack.end(); ++i)
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
