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

#ifndef LIBDENG2_LOG_H
#define LIBDENG2_LOG_H

#include "../Time"
#include "../Flag"
#include "../String"
#include "../Lockable"

#include <list>
#include <vector>
#include <string>
#include <cstdlib>

/// Macro for accessing the local log of the current thread.
#define LOG() \
    de::Log& log = de::Log::threadLog();

/// Macro for accessing the local log of the current thread and entering
/// a new log section.
#define LOG_AS(sectionName) \
    de::Log::Section __logSection = de::Log::Section(sectionName);
    
/// Macro for accessing the local log of the current thread and entering
/// a new log section with a std::string variable based name.
#define LOG_AS_STRING(str) \
    de::String __logSectionName = str; \
    LOG_AS(__logSectionName.c_str());
    
#define LOG_TRACE(str)      de::Log::threadLog().enter(de::Log::TRACE, str)
#define LOG_DEBUG(str)      de::Log::threadLog().enter(de::Log::DEBUG, str)
#define LOG_VERBOSE(str)    de::Log::threadLog().enter(de::Log::VERBOSE, str)
#define LOG_MESSAGE(str)    de::Log::threadLog().enter(str)
#define LOG_INFO(str)       de::Log::threadLog().enter(de::Log::INFO, str)
#define LOG_WARNING(str)    de::Log::threadLog().enter(de::Log::WARNING, str)
#define LOG_ERROR(str)      de::Log::threadLog().enter(de::Log::ERROR, str)
#define LOG_CRITICAL(str)   de::Log::threadLog().enter(de::Log::CRITICAL, str)

#ifdef WIN32
#   undef ERROR
#endif

namespace de
{   
    class LogBuffer;
    class LogEntry;
    
    /**
     * Logs provide means for adding log entries into the log entry buffer.
     * Each thread uses its own logs.
     *
     * @ingroup core
     */
    class LIBDENG2_API Log
    {
    public:
        /// Level of the log entry.
        enum LogLevel {
            TRACE,      ///< Trace messages.
            DEBUG,      ///< Debug messages.
            VERBOSE,    ///< Verbose log messages.
            MESSAGE,    ///< Normal log messages.
            INFO,       ///< Important messages.
            WARNING,    ///< A recoverable error.
            ERROR,      ///< A nonrecoverable (but not fatal) error.
            CRITICAL,   ///< Critical error (application will quit).
            MAX_LOG_LEVELS
        };

        class LIBDENG2_API Section
        {
        public:
            /**
             * The Section does not take a copy of @c name, so whatever
             * it's pointing to must exist while the Section exists.
             *
             * @param name  Name of the log section.
             */
            Section(const char* name);
            ~Section();

            Log& log() const { return _log; }

        private:
            Log& _log;
            const char* _name;
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
        void beginSection(const char* name);

        /**
         * Ends the topmost section in the log. 
         *
         * @param name  Name of the topmost section.
         */
        void endSection(const char* name);

        /**
         * Creates a new log entry with the default (MESSAGE) level.
         * Append the parameters of the entry using the << operator.
         *
         * @param format  Format template of the entry.
         */
        LogEntry& enter(const String& format);
        
        /**
         * Creates a new log entry with the specified level.
         * Append the parameters of the entry using the << operator.
         *
         * @param level   Level of the entry.
         * @param format  Format template of the entry.
         */
        LogEntry& enter(LogLevel level, const String& format);

    public:
        /** 
         * Returns the logger of the current thread.
         */
        static Log& threadLog();

        /**
         * Deletes the current thread's log. Threads should call this before
         * they quit.
         */
        static void disposeThreadLog();        

    private:
        typedef std::vector<const char*> SectionStack;
        SectionStack _sectionStack;
        
        LogEntry* _throwawayEntry;
    };
    
    /**
     * An entry to be stored in the log entry buffer. Log entries are created with
     * Log::enter().
     *
     * @ingroup core
     */
    class LIBDENG2_API LogEntry 
    {
    public:
        /**
         * Argument for a log entry.
         *
         * @ingroup core
         */
        class LIBDENG2_API Arg : public String::IPatternArg
        {
        public:
            /// The wrong type is used in accessing the value. @ingroup errors
            DEFINE_ERROR(TypeError);
            
            enum Type {
                INTEGER,
                FLOATING_POINT,
                STRING,
                WIDE_STRING
            };

            /**
             * Base class for classes that support adding to the arguments.
             */
            class Base {
            public:
                /// Attempted conversion from unsupported type.
                DEFINE_ERROR(TypeError);
                
            public:
                virtual ~Base() {}
                
                virtual Type logEntryArgType() const = 0;                
                virtual dint64 asInt64() const {
                    throw TypeError("LogEntry::Arg::Base", "dint64 not supported");
                };
                virtual ddouble asDouble() const {
                    throw TypeError("LogEntry::Arg::Base", "ddouble not supported");
                };
                virtual String asText() const {
                    throw TypeError("LogEntry::Arg::Base", "String not supported");
                }
                virtual std::wstring asWideString() const {
                    throw TypeError("LogEntry::Arg::Base", "Wide string not supported");
                }
            };

        public:
            Arg(dint i) : _type(INTEGER) { _data.intValue = i; }
            Arg(duint i) : _type(INTEGER) { _data.intValue = i; }
#ifndef WIN32
            Arg(dsize i) : _type(INTEGER) { _data.intValue = i; }
#endif
            Arg(dint64 i) : _type(INTEGER) { _data.intValue = i; }
            Arg(ddouble d) : _type(FLOATING_POINT) { _data.floatValue = d; }
            Arg(const void* p) : _type(INTEGER) { _data.intValue = dint64(p); }
            Arg(const char* s) : _type(STRING) {
                _data.stringValue = new String(s);
            }
            Arg(const wchar_t* ws) : _type(WIDE_STRING) {
                _data.wideStringValue = new std::wstring(ws);
            }
            Arg(const String& s) : _type(STRING) {
                _data.stringValue = new String(s); 
            }
            Arg(const std::wstring& ws) : _type(WIDE_STRING) {
                _data.wideStringValue = new std::wstring(ws);
            }
            Arg(const Base& arg) : _type(arg.logEntryArgType()) {
                switch(_type) {
                case INTEGER:
                    _data.intValue = arg.asInt64();
                    break;
                case FLOATING_POINT:
                    _data.floatValue = arg.asDouble();
                    break;
                case STRING:
                    _data.stringValue = new String(arg.asText());
                    break;
                case WIDE_STRING:
                    _data.wideStringValue = new std::wstring(arg.asWideString());
                    break;
                }
            }
            ~Arg() {
                if(_type == STRING) {
                    delete _data.stringValue;
                }
                else if(_type == WIDE_STRING) {
                    delete _data.wideStringValue;
                }
            }
            
            Type type() const { return _type; }
            dint64 intValue() const {
                assert(_type == INTEGER);
                return _data.intValue;
            }
            ddouble floatValue() const {
                assert(_type == FLOATING_POINT);
                return _data.floatValue;
            }
            const std::string& stringValue() const {
                assert(_type == STRING);
                return *_data.stringValue;
            }
            const std::wstring& wideStringValue() const {
                assert(_type == WIDE_STRING);
                return *_data.wideStringValue;
            }

            // Implements String::IPatternArg.
            ddouble asNumber() const {
                if(_type == INTEGER) {
                    return ddouble(_data.intValue);
                } 
                else if(_type == FLOATING_POINT) {
                    return _data.floatValue;
                }
                throw TypeError("Log::Arg::asNumber", 
                    "String argument cannot be used as a number");
            }
            String asText() const {
                if(_type == STRING) {
                    return *_data.stringValue;
                }
                else if(_type == WIDE_STRING) {
                    return String::wideToString(*_data.wideStringValue);
                }
                throw TypeError("Log::Arg::asText",
                    "Number argument cannot be used a string");
            }

        private:
            Type _type;
            union Data {
                dint64 intValue;
                ddouble floatValue;
                String* stringValue;
                std::wstring* wideStringValue;
            } _data;
        };
        
    public:
        /// In simple mode, only print the actual message contents, 
        /// without metadata.
        DEFINE_FLAG(SIMPLE, 0);
        
        /// Use escape sequences to format the entry with text styles 
        /// (for graphical output).
        DEFINE_FINAL_FLAG(STYLED, 1, Flags);

        /// The format string has incorrect syntax. @ingroup errors
        DEFINE_ERROR(IllegalFormatError);
        
    public:
        LogEntry(Log::LogLevel level, const String& section, const String& format); 
        ~LogEntry();
        
        /// Appends a new argument to the entry.
        template <typename ValueType>
        LogEntry& operator << (const ValueType& v) {
            if(!_disabled) {
                _args.push_back(new Arg(v));
            }
            return *this;
        }

        /// Returns the timestamp of the entry.
        Time when() const { return _when; }

        Log::LogLevel level() const { return _level; }

        /// Converts the log entry to a string.
        String asText(const Flags& flags = 0) const;

        /// Make this entry print without metadata.
        LogEntry& simple() {
            _defaultFlags |= SIMPLE;
            return *this;
        }

    private:
        void advanceFormat(String::const_iterator& i) const;

    private:
        Time _when;
        Log::LogLevel _level;
        String _section;
        String _format;
        Flags _defaultFlags;
        bool _disabled;

        typedef std::vector<Arg*> Args;        
        Args _args;
    };
    
    std::ostream& operator << (std::ostream& stream, const LogEntry::Arg& arg);
}

#endif /* LIBDENG2_LOG_H */
