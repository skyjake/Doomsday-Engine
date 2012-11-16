/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/LogBuffer"
#include "logtextstyle.h"

#include <QMap>
#include <QTextStream>
#include <QThread>

using namespace de;

const char* MAIN_SECTION = "";

/**
 * The logs table is lockable so that multiple threads can access their
 * logs at the same time.
 */
class Logs : public QMap<QThread*, Log*>, public Lockable
{
public:
    Logs() {}
    ~Logs() {
        lock();
        // The logs are owned by the logs table.
        foreach(Log* log, values()) {
            delete log;
        }
        unlock();
    }
};

/// The logs table contains the log of each thread that uses logging.
static Logs logs;

LogEntry::LogEntry() : _level(Log::TRACE), _disabled(true)
{}

LogEntry::LogEntry(Log::LogLevel level, const String& section, const String& format)
    : _level(level), _section(section), _format(format), _disabled(false)
{
    if(!LogBuffer::appBuffer().enabled(level))
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
    QString result;
    QTextStream output(&result);

    if(_defaultFlags & Simple)
    {
        flags |= Simple;
    }
    
    // In simple mode, skip the metadata.
    if(!flags.testFlag(Simple))
    {    
        // Begin with the timestamp.
        if(flags & Styled) output << TEXT_STYLE_LOG_TIME;
    
        output << _when.asText(Date::BuildNumberAndTime) << " ";

        if(!flags.testFlag(Styled))
        {
            const char* levelNames[Log::MAX_LOG_LEVELS] = {
                "(...)",
                "(deb)",
                "(vrb)",
                "",
                "(inf)",
                "(WRN)",
                "(ERR)",
                "(!!!)"        
            };
            output << qSetPadChar(' ') << qSetFieldWidth(5) << levelNames[_level] <<
                      qSetFieldWidth(0) << " ";
        }
        else
        {
            const char* levelNames[Log::MAX_LOG_LEVELS] = {
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
                << (_level >= Log::WARNING? TEXT_STYLE_LOG_BAD_LEVEL : TEXT_STYLE_LOG_LEVEL)
                << levelNames[_level] << "\t\r";
        }
    }

    // Section name.
    if(!_section.empty())
    {
        if(!flags.testFlag(Styled))
        {
            output << _section << ": ";
        }
        else
        {
            output << TEXT_STYLE_SECTION << _section << ": ";
        }
    }

    if(flags.testFlag(Styled))
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

        DENG2_FOR_EACH_CONST(String, i, _format)
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
    
    if(flags & Styled)
    {
        output << TEXT_STYLE_MESSAGE;
    }
    
    return result;
}

QTextStream& de::operator << (QTextStream& stream, const LogEntry::Arg& arg)
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
    _throwawayEntry = new LogEntry; // disabled LogEntry, so doesn't accept arguments
    _sectionStack.push_back(MAIN_SECTION);
}

Log::~Log()
{
    delete _throwawayEntry;
}

void Log::beginSection(const char* name)
{
    _sectionStack.append(name);
}

void Log::endSection(const char* DENG2_DEBUG_ONLY(name))
{
    DENG2_ASSERT(_sectionStack.back() == name);
    _sectionStack.takeLast();
}

LogEntry& Log::enter(const String& format)
{
    return enter(MESSAGE, format);
}

LogEntry& Log::enter(Log::LogLevel level, const String& format)
{
    if(!LogBuffer::appBuffer().enabled(level))
    {
        // If the level is disabled, no messages are entered into it.
        return *_throwawayEntry;
    }
    
    // Collect the sections.
    String context;
    String latest;
    foreach(const char* i, _sectionStack)
    {
        if(i == latest)
        {
            // Don't repeat if it has the exact same name (due to recursive calls).
            continue;
        }
        if(context.size())
        {
            context += " > ";
        }
        latest = i;
        context += i;
    }

    // Make a new entry.
    LogEntry* entry = new LogEntry(level, context, format);
    
    // Add it to the application's buffer. The buffer gets ownership.
    LogBuffer::appBuffer().add(entry);
    
    return *entry;
}

Log& Log::threadLog()
{
    // Each thread has its own log.
    QThread* thread = QThread::currentThread();
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
        theLog = found.value();
    }
    logs.unlock();
    return *theLog;
}

void Log::disposeThreadLog()
{
    QThread* thread = QThread::currentThread();
    logs.lock();
    Logs::iterator found = logs.find(thread);
    if(found != logs.end())
    {
        delete found.value();
        logs.remove(found.key());
    }
    logs.unlock();
}
