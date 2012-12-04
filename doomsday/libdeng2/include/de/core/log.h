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

#ifndef LIBDENG2_LOG_H
#define LIBDENG2_LOG_H

#include "../Time"
#include "../String"
#include "../Lockable"

#include <QList>
#include <vector>
#include <string>
#include <cstdlib>

/// Macro for accessing the local log of the current thread.
#define LOG()               de::Log::threadLog()

/// Macro for accessing the local log of the current thread and entering
/// a new log section.
#define LOG_AS(sectionName) \
    de::Log::Section __logSection = de::Log::Section(sectionName);

/**
 * Macro for accessing the local log of the current thread and entering
 * a new log section with a de::String variable based name.
 *
 * @param str  Anything out of which a de::String can be constructed.
 *
 * @note Log::Section doesn't own the strings passed in; we have to
 * ensure that the string exists in memory as long as the section (scope)
 * is valid.
 */
#define LOG_AS_STRING(str) \
    de::String __logSectionName = str; \
    de::Block __logSectionUtf8 = __logSectionName.toUtf8(); \
    LOG_AS(__logSectionUtf8.constData());

#define LOG_TRACE(str)      de::LogEntryStager(de::LogEntry::TRACE,    str)
#define LOG_DEBUG(str)      de::LogEntryStager(de::LogEntry::DEBUG,    str)
#define LOG_VERBOSE(str)    de::LogEntryStager(de::LogEntry::VERBOSE,  str)
#define LOG_MSG(str)        de::LogEntryStager(de::LogEntry::MESSAGE,  str)
#define LOG_INFO(str)       de::LogEntryStager(de::LogEntry::INFO,     str)
#define LOG_WARNING(str)    de::LogEntryStager(de::LogEntry::WARNING,  str)
#define LOG_ERROR(str)      de::LogEntryStager(de::LogEntry::ERROR,    str)
#define LOG_CRITICAL(str)   de::LogEntryStager(de::LogEntry::CRITICAL, str)

#define LOG_AT_LEVEL(level, str)   de::LogEntryStager(level, str)

#ifdef DENG2_DEBUG
/**
 * Makes a developer-only TRACE level log entry. Only enabled in debug builds;
 * use this for internal messages that are only useful to / understood by
 * developers when debugging. (Note that parameters differ compared to the
 * normal LOG_* macros.)
 */
#  define LOG_DEV_TRACE(form, args) LOG_TRACE(form) << args
#else
#  define LOG_DEV_TRACE(form, args)
#endif

#ifdef WIN32
#   undef ERROR
#endif

namespace de {

class LogBuffer;


/**
 * An entry to be stored in the log entry buffer. Log entries are created with
 * Log::enter().
 *
 * Log entry arguments are appended after the creation of the entry and even
 * after it has been inserted to the buffer. Therefore it is possible that an
 * entry is being flushed while another thread is still adding arguments to it.
 * Due to this entries are lockable and will be locked whenever
 * LogEntry::asText() is being executed or when an argument is being added.
 *
 * @ingroup core
 */
class DENG2_PUBLIC LogEntry
{
public:
    /// Level of the log entry.
    enum Level
    {
        /**
         * Trace messages are intended for low-level debugging. They should be used
         * to log which methods are entered and exited, and mark certain points within
         * methods. Intended only for developers and debug builds.
         */
        TRACE = 0,

        /**
         * Debug messages are intended for normal debugging. They should be enabled
         * only in debug builds. An example of a debug message might be a printout of
         * a ZIP archive's file count and size once an archive has been successfully
         * opened. Intended only for developers and debug builds.
         */
        DEBUG = 1,

        /**
         * Verbose messages should be used to log technical information that is only
         * of interest to advanced users. An example of a verbose message could be
         * the summary of all the defined object types during the launch of a game.
         * Verbose messages should not be used for anything that produces a large
         * number of log entries, such as an entry about reading the contents of a
         * file within a ZIP archive (which would be suitable for the DEBUG level).
         */
        VERBOSE = 2,

        /**
         * Normal log entries are intended for regular users. An example: message about
         * which map is being loaded.
         */
        MESSAGE = 3,

        /**
         * Info messages are intended for situations that are particularly noteworthy.
         * An info message should be used for instance when a script has been stopped
         * because of an uncaught exception occurred during its execution.
         */
        INFO = 4,

        /**
         * Warning messages are reserved for recoverable error situations. A warning
         * might be logged for example when the expected resource could not be found,
         * and a fallback resource was used instead.
         */
        WARNING = 5,

        /**
         * Error messages are intended for nonrecoverable errors. The error is grave
         * enough to cause the shutting down of the current game, but the engine can
         * still remain running.
         */
        ERROR = 6,

        /**
         * Critical messages are intended for fatal errors that cause the engine to be
         * shut down.
         */
        CRITICAL = 7,

        MAX_LOG_LEVELS
    };

    static String levelToText(Level level)
    {
        switch(level)
        {
        case TRACE:     return "TRACE";
        case DEBUG:     return "DEBUG";
        case VERBOSE:   return "VERBOSE";
        case MESSAGE:   return "MESSAGE";
        case INFO:      return "INFO";
        case WARNING:   return "WARNING";
        case ERROR:     return "ERROR";
        case CRITICAL:  return "CRITICAL";
        default:        return "";
        }
    }

    static Level textToLevel(String text)
    {
        for(int i = TRACE; i < MAX_LOG_LEVELS; ++i)
        {
            if(!levelToText(Level(i)).compareWithoutCase(text))
                return Level(i);
        }
        throw Error("Log::textToLevel", "'" + text + "' is not a valid log level");
    }

    /**
     * Argument for a log entry.
     *
     * @ingroup core
     */
    class DENG2_PUBLIC Arg : public String::IPatternArg
    {
    public:
        /// The wrong type is used in accessing the value. @ingroup errors
        DENG2_ERROR(TypeError);

        enum Type {
            INTEGER,
            FLOATING_POINT,
            STRING
        };

        /**
         * Base class for classes that support adding to the arguments.
         */
        class Base {
        public:
            /// Attempted conversion from unsupported type.
            DENG2_ERROR(TypeError);

        public:
            virtual ~Base() {}

            virtual Type logEntryArgType() const = 0;
            virtual dint64 asInt64() const {
                throw TypeError("LogEntry::Arg::Base", "dint64 not supported");
            }
            virtual ddouble asDouble() const {
                throw TypeError("LogEntry::Arg::Base", "ddouble not supported");
            }
            virtual String asText() const {
                throw TypeError("LogEntry::Arg::Base", "String not supported");
            }
        };

    public:
        Arg(dint i)              : _type(INTEGER)        { _data.intValue   = i; }
        Arg(duint i)             : _type(INTEGER)        { _data.intValue   = i; }
        Arg(long int i)          : _type(INTEGER)        { _data.intValue   = i; }
        Arg(long unsigned int i) : _type(INTEGER)        { _data.intValue   = i; }
        Arg(duint64 i)           : _type(INTEGER)        { _data.intValue   = dint64(i); }
        Arg(dint64 i)            : _type(INTEGER)        { _data.intValue   = i; }
        Arg(ddouble d)           : _type(FLOATING_POINT) { _data.floatValue = d; }
        Arg(void const *p)       : _type(INTEGER)        { _data.intValue   = dint64(p); }
        Arg(char const *s) : _type(STRING) {
            _data.stringValue = new String(s);
        }
        Arg(String const &s) : _type(STRING) {
            _data.stringValue = new String(s.data(), s.size());
        }
        Arg(Base const &arg) : _type(arg.logEntryArgType()) {
            switch(_type) {
            case INTEGER:
                _data.intValue = arg.asInt64();
                break;
            case FLOATING_POINT:
                _data.floatValue = arg.asDouble();
                break;
            case STRING: {
                String s = arg.asText();
                _data.stringValue = new String(s.data(), s.size());
                break; }
            }
        }
        ~Arg() {
            if(_type == STRING) {
                delete _data.stringValue;
            }
        }

        Type type() const { return _type; }
        dint64 intValue() const {
            DENG2_ASSERT(_type == INTEGER);
            return _data.intValue;
        }
        ddouble floatValue() const {
            DENG2_ASSERT(_type == FLOATING_POINT);
            return _data.floatValue;
        }
        QString stringValue() const {
            DENG2_ASSERT(_type == STRING);
            return *_data.stringValue;
        }

        // Implements String::IPatternArg.
        ddouble asNumber() const {
            if(_type == INTEGER) {
                return ddouble(_data.intValue);
            }
            else if(_type == FLOATING_POINT) {
                return _data.floatValue;
            }
            throw TypeError("Log::Arg::asNumber", "String argument cannot be used as a number");
        }
        String asText() const {
            if(_type == STRING) {
                return *_data.stringValue;
            }
            else if(_type == INTEGER) {
                return String::number(_data.intValue);
            }
            else if(_type == FLOATING_POINT) {
                return String::number(_data.floatValue);
            }
            throw TypeError("Log::Arg::asText", "Number argument cannot be used a string");
        }

    private:
        Type _type;
        union Data {
            dint64 intValue;
            ddouble floatValue;
            String *stringValue;
        } _data;
    };

public:
    enum Flag
    {
        /// In simple mode, only print the actual message contents,
        /// without metadata.
        Simple = 0x1,

        /// Use escape sequences to format the entry with text styles
        /// (for graphical output).
        Styled = 0x2,

        /// Omit the section from the entry text.
        OmitSection = 0x4,

        /// Indicate that the section is the same as on the previous line.
        SectionSameAsBefore = 0x8,

        /// Parts of the section can be abbreviated because they are clear
        /// from the context (e.g., previous line).
        AbbreviateSection = 0x10
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /// The format string has incorrect syntax. @ingroup errors
    DENG2_ERROR(IllegalFormatError);

    typedef QList<Arg *> Args;

public:
    /**
     * Constructs a disabled log entry.
     */
    LogEntry();

    LogEntry(Level level, String const &section, int sectionDepth, String const &format, Args args);

    ~LogEntry();

    /// Returns the timestamp of the entry.
    Time when() const { return _when; }

    Level level() const { return _level; }

    /// Returns a reference to the entry's section part. Reference is valid
    /// for the lifetime of the entry.
    String const &section() const { return _section; }

    /// Returns the number of sub-sections in the entry's section part.
    int sectionDepth() const { return _sectionDepth; }

    /**
     * Converts the log entry to a string.
     *
     * @param flags           Flags that control how the text is composed.
     * @param shortenSection  Number of characters to cut from the beginning of the section.
     *                        With AbbreviateSection this limits which portion of the
     *                        section is subject to abbreviation.
     *
     * @return Composed textual representation of the entry.
     */
    String asText(Flags const &flags = 0, int shortenSection = 0) const;

private:
    void advanceFormat(String::const_iterator &i) const;

private:
    Time _when;
    Level _level;
    String _section;
    int _sectionDepth;
    String _format;
    Flags _defaultFlags;
    bool _disabled;
    Args _args;
};

QTextStream &operator << (QTextStream &stream, LogEntry::Arg const &arg);

Q_DECLARE_OPERATORS_FOR_FLAGS(LogEntry::Flags)

/**
 * Provides means for adding log entries into the log entry buffer (LogBuffer).
 * A thread's Log keeps track of the thread-local section stack, but there is
 * only one LogBuffer where all the entries are collected.
 *
 * @ingroup core
 */
class DENG2_PUBLIC Log
{
public:
    class DENG2_PUBLIC Section
    {
    public:
        /**
         * The Section does not take a copy of @c name, so whatever
         * it's pointing to must exist while the Section exists.
         *
         * @param name  Name of the log section.
         */
        Section(char const *name);
        ~Section();

        Log &log() const { return _log; }

    private:
        Log &_log;
        char const *_name;
    };

public:
    Log();
    virtual ~Log();

    /**
     * Begins a new section in the log. Sections can be nested.
     *
     * @param name  Name of the section. No copy of this string is made,
     *              so it must exist while the section is in use.
     */
    void beginSection(char const *name);

    /**
     * Ends the topmost section in the log.
     *
     * @param name  Name of the topmost section.
     */
    void endSection(char const *name);

    /**
     * Creates a new log entry with the default (MESSAGE) level.
     * Append the parameters of the entry using the << operator.
     *
     * @param format  Format template of the entry.
     */
    LogEntry &enter(String const &format);

    /**
     * Creates a new log entry with the specified level.
     * Append the parameters of the entry using the << operator.
     *
     * @param level   Level of the entry.
     * @param format  Format template of the entry.
     * @param arguments  List of arguments. The entry is given ownership of
     *                each Arg instance.
     */
    LogEntry &enter(LogEntry::Level level, String const &format, LogEntry::Args arguments = LogEntry::Args());

public:
    /**
     * Returns the logger of the current thread.
     */
    static Log &threadLog();

    /**
     * Deletes the current thread's log. Threads should call this before
     * they quit.
     */
    static void disposeThreadLog();

private:
    typedef QList<char const *> SectionStack;
    SectionStack _sectionStack;

    LogEntry *_throwawayEntry;
};

/**
 * Stages a log entry for insertion into LogBuffer. Instances of LogEntryStager
 * are built on the stack.
 *
 * You should use the LOG_* macros instead of using LogEntryStager directly.
 */
class DENG2_PUBLIC LogEntryStager
{
public:
    LogEntryStager(LogEntry::Level level, String const &format);

    /// Appends a new argument to the entry.
    template <typename ValueType>
    inline LogEntryStager &operator << (ValueType const &v) {
        if(!_disabled) {
            // Args are created only if the level is enabled.
            _args.append(new LogEntry::Arg(v));
        }
        return *this;
    }

    ~LogEntryStager() {
        if(!_disabled) {
            // Ownership of the entries is transferred to the LogEntry.
            LOG().enter(_level, _format, _args);
        }
    }

private:
    bool _disabled;
    LogEntry::Level _level;
    String _format;
    LogEntry::Args _args;
};

} // namespace de

#endif /* LIBDENG2_LOG_H */
