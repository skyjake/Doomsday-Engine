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

#include "de/log.h"
#include "de/time.h"
#include "de/date.h"
#include "de/logbuffer.h"
#include "de/guard.h"
#include "de/reader.h"
#include "de/writer.h"
#include "de/fifo.h"
#include "../src/core/logtextstyle.h"

#include <array>
#include <the_Foundation/stdthreads.h>

namespace de {

const char *TEXT_STYLE_NORMAL        = DE_ESC("0"); // normal
const char *TEXT_STYLE_MESSAGE       = DE_ESC("0"); // normal
const char *TEXT_STYLE_MAJOR_MESSAGE = DE_ESC("1"); // major
const char *TEXT_STYLE_MINOR_MESSAGE = DE_ESC("2"); // minor
const char *TEXT_STYLE_SECTION       = DE_ESC("3"); // meta
const char *TEXT_STYLE_MAJOR_SECTION = DE_ESC("4"); // major meta
const char *TEXT_STYLE_MINOR_SECTION = DE_ESC("5"); // minor meta
const char *TEXT_STYLE_LOG_TIME      = DE_ESC("6"); // aux meta
const char *TEXT_MARK_INDENT         = DE_ESC(">");

static const char *MAIN_SECTION = "";

/// If the section is longer than this, it will be alone on one line while
/// the rest of the entry continues after a break.
static const int LINE_BREAKING_SECTION_LENGTH = 30;

namespace internal {

#if 0
/**
 * @internal
 * The logs table is lockable so that multiple threads can access their logs at
 * the same time.
 */
class Logs : public Lockable, public QHash<QThread *, Log *>
{
public:
    Logs() {}
    ~Logs() {
        DE_GUARD_PTR(this);
        // The logs are owned by the logs table.
        for (Log *log : *this) delete log;
    }
};
#endif

} // namespace internal

/// The logs table contains the log of each thread that uses logging.
//static std::unique_ptr<internal::Logs> logsPtr;

/// Unused entry arguments are stored here in the pool.
static FIFO<LogEntry::Arg> argPool;

LogEntry::Arg::Arg() : _type(IntegerArgument)
{
    _data.intValue = 0;
}

LogEntry::Arg::~Arg()
{
    clear();
}

void LogEntry::Arg::clear()
{
    if (_type == StringArgument)
    {
        delete _data.stringValue;
        _data.stringValue = nullptr;
        _type = IntegerArgument;
    }
}

void LogEntry::Arg::setValue(int32_t i)
{
    clear();
    _type = IntegerArgument;
    _data.intValue = i;
}

void LogEntry::Arg::setValue(uint32_t i)
{
    clear();
    _type = IntegerArgument;
    _data.intValue = i;
}

void LogEntry::Arg::setValue(int64_t i)
{
    clear();
    _type = IntegerArgument;
    _data.intValue = i;
}

void LogEntry::Arg::setValue(uint64_t i)
{
    clear();
    _type = IntegerArgument;
    _data.intValue = dint64(i);
}

void LogEntry::Arg::setValue(ddouble d)
{
    clear();
    _type = FloatingPointArgument;
    _data.floatValue = d;
}

void LogEntry::Arg::setValue(const void *p)
{
    clear();
    _type = IntegerArgument;
    _data.intValue = dint64(p);
}

void LogEntry::Arg::setValue(const char *s)
{
    clear();
    _type = StringArgument;
    _data.stringValue = new String(s);
}

void LogEntry::Arg::setValue(const std::string &s)
{
    clear();
    _type = StringArgument;
    _data.stringValue = new String(s);
}

void LogEntry::Arg::setValue(const String &s)
{
    clear();
    _type = StringArgument;
    _data.stringValue = new String(s); // make a copy
}

void LogEntry::Arg::setValue(const Time &t)
{
    clear();
    _type = StringArgument;
    _data.stringValue = new String(t.asText());
}

void LogEntry::Arg::setValue(const std::array<char, 4> &typecode)
{
    setValue(String(typecode.data(), 4));
}

void LogEntry::Arg::setValue(const LogEntry::Arg::Base &arg)
{
    switch (arg.logEntryArgType())
    {
    case IntegerArgument:
        setValue(arg.asInt64());
        break;

    case FloatingPointArgument:
        setValue(arg.asDouble());
        break;

    case StringArgument:
        setValue(arg.asText()); // deep copy
        break;
    }
}

LogEntry::Arg &LogEntry::Arg::operator = (const Arg &other)
{
    clear();
    if (other._type == StringArgument)
    {
        setValue(*other._data.stringValue); // deep copy
    }
    else
    {
        _type = other._type;
        _data = other._data;
    }
    return *this;
}

ddouble LogEntry::Arg::asNumber() const
{
    if (_type == IntegerArgument)
    {
        return ddouble(_data.intValue);
    }
    else if (_type == FloatingPointArgument)
    {
        return _data.floatValue;
    }
    throw TypeError("Log::Arg::asNumber", "String argument cannot be used as a number");
}

String LogEntry::Arg::asText() const
{
    if (_type == StringArgument)
    {
        return *_data.stringValue;
    }
    else if (_type == IntegerArgument)
    {
        return String::asText(_data.intValue);
    }
    else if (_type == FloatingPointArgument)
    {
        return String::asText(_data.floatValue);
    }
    throw TypeError("Log::Arg::asText", "Number argument cannot be used a string");
}

void LogEntry::Arg::operator >> (Writer &to) const
{
    to << dbyte(_type);

    switch (_type)
    {
    case IntegerArgument:
        to << _data.intValue;
        break;

    case FloatingPointArgument:
        to << _data.floatValue;
        break;

    case StringArgument:
        to << *_data.stringValue;
        break;
    }
}

void LogEntry::Arg::operator << (Reader &from)
{
    if (_type == StringArgument) delete _data.stringValue;

    from.readAs<dbyte>(_type);

    switch (_type)
    {
    case IntegerArgument:
        from >> _data.intValue;
        break;

    case FloatingPointArgument:
        from >> _data.floatValue;
        break;

    case StringArgument:
        _data.stringValue = new String;
        from >> *_data.stringValue;
        break;
    }
}

LogEntry::Arg *LogEntry::Arg::newFromPool()
{
    Arg *arg = argPool.take();
    if (arg) return arg;
    // Need a new one.
    return new LogEntry::Arg;
}

void LogEntry::Arg::returnToPool(Arg *arg)
{
    arg->clear();
    argPool.put(arg);
}

LogEntry::LogEntry() : _metadata(0), _sectionDepth(0), _disabled(true)
{}

LogEntry::LogEntry(duint32       metadata,
                   const String &section,
                   int           sectionDepth,
                   const String &format,
                   const Args &  args)
    : _metadata(metadata)
    , _section(section)
    , _sectionDepth(sectionDepth)
    , _format(format)
    , _disabled(false)
    , _args(args)
{
    if (!LogBuffer::get().isEnabled(metadata))
    {
        _disabled = true;
    }
}

LogEntry::LogEntry(const LogEntry &other, Flags extraFlags)
    : Lockable()
    , ISerializable()
    , _when(other._when)
    , _metadata(other._metadata)
    , _section(other._section)
    , _sectionDepth(other._sectionDepth)
    , _format(other._format)
    , _defaultFlags(other._defaultFlags | extraFlags)
    , _disabled(other._disabled)
{
    for (const auto &i : other._args)
    {
        Arg *a = Arg::newFromPool();
        *a = *i;
        _args.append(a);
    }
}

LogEntry::~LogEntry()
{
    DE_GUARD(this);

    // Put the arguments back to the shared pool.
    for (Args::iterator i = _args.begin(); i != _args.end(); ++i)
    {
        Arg::returnToPool(*i);
    }
}

Flags LogEntry::flags() const
{
    return _defaultFlags;
}

String LogEntry::asText(const Flags &formattingFlags, dsize shortenSection) const
{
    DE_GUARD(this);

    /// @todo This functionality belongs in an entry formatter class.

    Flags flags = formattingFlags;
    std::ostringstream output;

    if (_defaultFlags & Simple)
    {
        flags |= Simple;
    }

    // In simple mode, skip the metadata.
    if (!(flags & Simple))
    {
        // Begin with the timestamp.
        if (flags & Styled) output << TEXT_STYLE_LOG_TIME << _E(m);

        output << _when.asText(Time::SecondsSinceStart) << " ";

        if (flags & Styled) output << _E(.);

        if (!(flags & OmitDomain))
        {
            char dc = (_metadata & Resource? 'R' :
                       _metadata & Map?      'M' :
                       _metadata & Script?   'S' :
                       _metadata & GL?       'G' :
                       _metadata & Audio?    'A' :
                       _metadata & Input?    'I' :
                       _metadata & Network?  'N' : ' ');
            if (_metadata & Dev)
            {
                if (dc != ' ')
                    dc = tolower(dc);
                else
                    dc = '-'; // Generic developer message
            }

            if (!(flags & Styled))
            {
                output << dc;
            }
            else
            {
                output << _E(s)_E(F)_E(m) << dc << " " << _E(.);
            }
        }

        if (!(flags & OmitLevel))
        {
            if (!(flags & Styled))
            {
                const char *levelNames[] = {
                    "", // not used
                    "(vv)",
                    "(v)",
                    "",
                    "(i)",
                    "(WRN)",
                    "(ERR)",
                    "(!!!)"
                };
                output << stringf("%5s ", levelNames[level()]);
            }
            else
            {
                const char *levelNames[] = {
                    "", // not used
                    "XVerbose",
                    "Verbose",
                    "",
                    "Note!",
                    "Warning",
                    "ERROR",
                    "FATAL!"
                };
                output << "\t"
                    << (level() >= LogEntry::Warning? TEXT_STYLE_MAJOR_SECTION :
                        level() <= LogEntry::Verbose? TEXT_STYLE_MINOR_SECTION :
                                                      TEXT_STYLE_SECTION)
                    << levelNames[level()] << "\t";
            }
        }
    }

    // Section name.
    if (!(flags & OmitSection) && !_section.empty())
    {
        if (flags & Styled)
        {
            output << TEXT_MARK_INDENT
                   << (level() >= LogEntry::Warning? TEXT_STYLE_MAJOR_SECTION :
                       level() <= LogEntry::Verbose? TEXT_STYLE_MINOR_SECTION :
                                                     TEXT_STYLE_SECTION);
        }

        // Process the section: shortening and possible abbreviation.
        String sect;
        if (flags & AbbreviateSection)
        {
            /*
             * We'll split the section into parts, and then abbreviate some of
             * the parts, trying not to lose too much information.
             * @a shortenSection controls how much of the section can be
             * abbreviated (num of chars from beginning).
             */
            StringList parts = _section.split(" > ");
            int len = 0;
            while (!parts.isEmpty())
            {
                if (!sect.isEmpty())
                {
                    len += 3;
                    sect += " > ";
                }

                if (parts.first().sizeb() + len >= shortenSection) break;

                len += parts.first().sizei();
                if (sect.isEmpty())
                {
                    // Never abbreviate the first part.
                    sect += parts.first();
                }
                else
                {
                    sect += "..";
                }
                parts.removeFirst();
            }
            // Append the remainer as-is.
            sect += _section.substr(BytePos(len));
        }
        else
        {
            if (shortenSection < _section.sizeu())
            {
                sect = _section.right(_section.sizeb() - shortenSection);
            }
        }

        if (flags & SectionSameAsBefore)
        {
            int visibleSectLen = (!sect.isEmpty() && shortenSection? sect.sizei() : 0);
            size_t fillLen = de::max(shortenSection, _section.sizeu()) - visibleSectLen;
            if (fillLen > LINE_BREAKING_SECTION_LENGTH) fillLen = 2;
            output << String(fillLen, Char(' '));
            if (visibleSectLen)
            {
                output << "[" << sect << "] ";
            }
            else
            {
                output << "   ";
            }
        }
        else
        {
            // If the section is very long, it's clearer to break the line here.
            const char *separator = (sect.size() > LINE_BREAKING_SECTION_LENGTH? "\n     " : " ");
            output << "[" << sect << "]" << separator;
        }
    }

    if (flags & Styled)
    {
        output << TEXT_MARK_INDENT
               << (level() >= LogEntry::Warning? TEXT_STYLE_MAJOR_MESSAGE :
                   level() <= LogEntry::Verbose? TEXT_STYLE_MINOR_MESSAGE :
                                                 TEXT_STYLE_MESSAGE);
    }

    // Message text with the arguments formatted.
    if (_args.empty())
    {
        output << _format; // Verbatim.
    }
    else
    {
        String::PatternArgs patArgs;
        DE_FOR_EACH_CONST(Args, i, _args) patArgs << *i;
        output << _format % patArgs;
    }

    return output.str();
}

void LogEntry::operator >> (Writer &to) const
{
    to << _when
       << _section
       << _format
       << duint32(_metadata)
       << dbyte(_sectionDepth)
       << duint32(_defaultFlags);

    to.writeObjects(_args);
}

void LogEntry::operator << (Reader &from)
{
    for (Arg *a : _args) delete a;
    _args.clear();

    from >> _when
         >> _section
         >> _format;

    if (from.version() >= DE_PROTOCOL_1_14_0_LogEntry_metadata)
    {
        // This version adds context information to the entry.
        from.readAs<duint32>(_metadata);
    }
    else
    {
        dbyte oldLevel;
        from >> oldLevel;
        _metadata = oldLevel; // lacks audience information
    }

    from.readAs<dbyte>(_sectionDepth)
        .readAs<duint32>(_defaultFlags)
        .readObjects<Arg>(_args);
}

std::ostream &operator << (std::ostream &stream, const LogEntry::Arg &arg)
{
    switch (arg.type())
    {
    case LogEntry::Arg::IntegerArgument:
        stream << arg.intValue();
        break;

    case LogEntry::Arg::FloatingPointArgument:
        stream << arg.floatValue();
        break;

    case LogEntry::Arg::StringArgument:
        stream << arg.stringValue();
        break;
    }
    return stream;
}

Log::Section::Section(const char *name) : _log(threadLog()), _name(name)
{
    _log.beginSection(_name);
}

Log::Section::~Section()
{
    _log.endSection(_name);
}

DE_PIMPL_NOREF(Log)
{
    typedef std::vector<const char *> SectionStack;
    SectionStack sectionStack;
    LogEntry *throwawayEntry;
    duint32 currentEntryMedata; ///< Applies to the current entry being staged in the thread.
    int interactive = 0;

    Impl()
        : throwawayEntry(new LogEntry) ///< A disabled LogEntry, so doesn't accept arguments.
        , currentEntryMedata(0)
    {
        sectionStack.push_back(MAIN_SECTION);
    }

    ~Impl()
    {
        delete throwawayEntry;
    }
};

Log::Log() : d(new Impl)
{}

Log::~Log() // virtual
{}

void Log::setCurrentEntryMetadata(duint32 metadata)
{
    d->currentEntryMedata = metadata;
}

duint32 Log::currentEntryMetadata() const
{
    return d->currentEntryMedata;
}

bool Log::isStaging() const
{
    return d->currentEntryMedata != 0;
}

void Log::beginSection(const char *name)
{
    d->sectionStack.push_back(name);
}

void Log::endSection(const char *DE_DEBUG_ONLY(name))
{
    DE_ASSERT(d->sectionStack.back() == name);
    d->sectionStack.pop_back();
}

void Log::beginInteractive()
{
    d->interactive++;
}

void Log::endInteractive()
{
    d->interactive--;
    DE_ASSERT(d->interactive >= 0);
}

bool Log::isInteractive() const
{
    return d->interactive > 0;
}

LogEntry &Log::enter(const String &format, LogEntry::Args arguments)
{
    return enter(LogEntry::Message, format, arguments);
}

LogEntry &Log::enter(duint32 metadata, const String &format, LogEntry::Args arguments)
{
    // Staging done.
    d->currentEntryMedata = 0;

    if (isInteractive())
    {
        // Ensure the Interactive flag is set.
        metadata |= LogEntry::Interactive;
    }

    if (!LogBuffer::get().isEnabled(metadata))
    {
        DE_ASSERT(arguments.isEmpty());

        // If the level is disabled, no messages are entered into it.
        return *d->throwawayEntry;
    }

    // Collect the sections.
    String  context;
    CString latest;
    int     depth = 0;
    for (const CString i : d->sectionStack)
    {
        if (i == latest)
        {
            // Don't repeat if it has the exact same name (due to recursive calls).
            continue;
        }
        if (context.size())
        {
            context += " > ";
        }
        latest = i;
        context += i;
        ++depth;
    }

    // Make a new entry.
    LogEntry *entry = new LogEntry(metadata, context, depth, format, arguments);

    // Add it to the application's buffer. The buffer gets ownership.
    LogBuffer::get().add(entry);

    return *entry;
}

/*static internal::Logs &theLogs()
{
    if (logsPtr.get()) return *logsPtr;
    static Lockable lock;
    DE_GUARD(lock);
    if (!logsPtr.get()) logsPtr.reset(new internal::Logs);
    return *logsPtr;
}*/

static tss_t threadLogKey;

static void deleteLog(void *log)
{
    if (log) delete static_cast<Log *>(log);
}

//static void destroyThreadLog()
//{
//    if (threadLogKey)
//    {
//        deleteLog(tss_get(threadLogKey));
//        tss_set(threadLogKey, nullptr);
//    }
//}

Log &Log::threadLog()
{
    if (!threadLogKey)
    {
        tss_create(&threadLogKey, deleteLog);
    }
    Log *log = static_cast<Log *>(tss_get(threadLogKey));
    if (!log)
    {
        tss_set(threadLogKey, log = new Log);
    }
    return *log;
}

#if 0
void Log::disposeThreadLog()
{
    /*internal::Logs &logs = theLogs();
    DE_GUARD(logs);

    QThread *thread = QThread::currentThread();
    auto found = logs.find(thread);
    if (found != logs.end())
    {
        delete found.value();
        logs.erase(found);
    }*/
}
#endif

LogEntryStager::LogEntryStager(duint32 metadata, const String &format)
    : _metadata(metadata)
{
    if (!LogBuffer::appBufferExists())
    {
        _disabled = true;
    }
    else
    {
        auto &log = LOG();

        // Automatically set the Generic domain.
        if (!(_metadata & LogEntry::DomainMask))
        {
            _metadata |= LogEntry::Generic;
        }

        // Flag interactive messages.
        if (log.isInteractive())
        {
            _metadata |= LogEntry::Interactive;
        }

        _disabled = !LogBuffer::get().isEnabled(_metadata);

        if (!_disabled)
        {
            _format = format;

            log.setCurrentEntryMetadata(_metadata);
        }
    }
}

LogEntryStager::~LogEntryStager()
{
    if (!_disabled)
    {
        // Ownership of the args is transferred to the LogEntry.
        LOG().enter(_metadata, _format, _args);
    }
}

} // namespace de
