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
    de::Log::Section _logSection = de::Log::Section(sectionName);
    
/// Macro for accessing the local log of the current thread and entering
/// a new log section with a std::string variable based name.
#define LOG_AS_STRING(str) \
    de::String _logSectionName = str; \
    LOG_AS(_logSectionName.c_str());
    
#define LOG_TRACE(str)      de::Log::threadLog().enter(de::LogBuffer::TRACE, str)
#define LOG_DEBUG(str)      de::Log::threadLog().enter(de::LogBuffer::DEBUG, str)
#define LOG_VERBOSE(str)    de::Log::threadLog().enter(de::LogBuffer::VERBOSE, str)
#define LOG_MESSAGE(str)    de::Log::threadLog().enter(str)
#define LOG_INFO(str)       de::Log::threadLog().enter(de::LogBuffer::INFO, str)
#define LOG_WARNING(str)    de::Log::threadLog().enter(de::LogBuffer::WARNING, str)
#define LOG_ERROR(str)      de::Log::threadLog().enter(de::LogBuffer::ERROR, str)
#define LOG_CRITICAL(str)   de::Log::threadLog().enter(de::LogBuffer::CRITICAL, str)

namespace de
{   
    class LogEntry; 

    /**
     * Buffer for log entries. The application owns one of these.
     *
     * @ingroup core
     */
    class LogBuffer
    {
    public:
        /// Level of the log entry.
        enum Level {
            TRACE,      ///< Trace messages.
            DEBUG,      ///< Debug messages.
            VERBOSE,    ///< Verbose log messages.
            MESSAGE,    ///< Normal log messages.
            INFO,       ///< Important messages.
            WARNING,    ///< A recoverable error.
            ERROR,      ///< A nonrecoverable (but not fatal) error.
            CRITICAL,   ///< Critical error (application will quit).
            MAX_LEVELS
        };
        
    public:
        LogBuffer(duint maxEntryCount);
        virtual ~LogBuffer();

        void setMaxEntryCount(duint maxEntryCount);
        
        /**
         * Adds an entry to the buffer. The buffer gets ownership.
         *
         * @param entry  Entry to add.
         */
        void add(LogEntry* entry);

        void clear();

        /**
         * Enables log entries at or over a level. When a level is disabled, the 
         * entries will not be added to the log entry buffer.
         */
        void enable(Level overLevel = MESSAGE);
        
        /**
         * Disables the log. @see enable()
         */
        void disable() { enable(MAX_LEVELS); }
        
        bool enabled(Level overLevel = MESSAGE) const { 
            return enabledOverLevel_ <= overLevel; 
        }

        /**
         * Enables or disables standard output of log messages. When enabled,
         * log entries are written with simple formatting to the standard
         * output and error streams when the buffer is flushed.
         *
         * @param yes  @c true or @c false.
         */
        void enableStandardOutput(bool yes = true) {
            standardOutput_ = yes;
        }
        
        
        void setOutputFile(const String& path);
        
        /**
         * Flushes all unflushed entries to the defined outputs.
         */
        void flush();
        
    private:
        dint enabledOverLevel_;
        duint maxEntryCount_;
        bool standardOutput_;

        typedef std::list<LogEntry*> Entries;
        Entries entries_;

        Entries toBeFlushed_;
        Time lastFlushedAt_;
    };    
    
    /**
     * Logs provide means for adding log entries into the log entry buffer.
     * Each thread uses its own logs.
     *
     * @ingroup core
     */
    class Log
    {
    public:
        class Section
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

            Log& log() const { return log_; }

        private:
            Log& log_;
            const char* name_;
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
        LogEntry& enter(LogBuffer::Level level, const String& format);

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
        SectionStack sectionStack_;
        
        LogEntry* throwawayEntry_;
    };
    
    /**
     * An entry to be stored in the log entry buffer. Log entries are created with
     * Log::enter().
     *
     * @ingroup core
     */
    class LogEntry 
    {
    public:
        /**
         * Argument for a log entry.
         *
         * @ingroup core
         */
        class Arg : public String::IPatternArg
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
            Arg(dint i) : type_(INTEGER) { data_.intValue = i; }
            Arg(duint i) : type_(INTEGER) { data_.intValue = i; }
            Arg(dint64 i) : type_(INTEGER) { data_.intValue = i; }
            Arg(ddouble d) : type_(FLOATING_POINT) { data_.floatValue = d; }
            Arg(const void* p) : type_(INTEGER) { data_.intValue = dint64(p); }
            Arg(const char* s) : type_(STRING) {
                data_.stringValue = new String(s);
            }
            Arg(const wchar_t* ws) : type_(WIDE_STRING) {
                data_.wideStringValue = new std::wstring(ws);
            }
            Arg(const String& s) : type_(STRING) {
                data_.stringValue = new String(s); 
            }
            Arg(const std::wstring& ws) : type_(WIDE_STRING) {
                data_.wideStringValue = new std::wstring(ws);
            }
            Arg(const Base& arg) : type_(arg.logEntryArgType()) {
                switch(type_) {
                case INTEGER:
                    data_.intValue = arg.asInt64();
                    break;
                case FLOATING_POINT:
                    data_.floatValue = arg.asDouble();
                    break;
                case STRING:
                    data_.stringValue = new String(arg.asText());
                    break;
                case WIDE_STRING:
                    data_.wideStringValue = new std::wstring(arg.asWideString());
                    break;
                }
            }
            ~Arg() {
                if(type_ == STRING) {
                    delete data_.stringValue;
                }
                else if(type_ == WIDE_STRING) {
                    delete data_.wideStringValue;
                }
            }
            
            Type type() const { return type_; }
            dint64 intValue() const {
                assert(type_ == INTEGER);
                return data_.intValue;
            }
            ddouble floatValue() const {
                assert(type_ == FLOATING_POINT);
                return data_.floatValue;
            }
            const std::string& stringValue() const {
                assert(type_ == STRING);
                return *data_.stringValue;
            }
            const std::wstring& wideStringValue() const {
                assert(type_ == WIDE_STRING);
                return *data_.wideStringValue;
            }

            // Implements String::IPatternArg.
            ddouble asNumber() const {
                if(type_ == INTEGER) {
                    return data_.intValue;
                } 
                else if(type_ == FLOATING_POINT) {
                    return data_.floatValue;
                }
                throw TypeError("Log::Arg::asNumber", 
                    "String argument cannot be used as a number");
            }
            String asText() const {
                if(type_ == STRING) {
                    return *data_.stringValue;
                }
                else if(type_ == WIDE_STRING) {
                    return String::wideToString(*data_.wideStringValue);
                }
                throw TypeError("Log::Arg::asText",
                    "Number argument cannot be used a string");
            }

        private:
            Type type_;
            union Data {
                dint64 intValue;
                ddouble floatValue;
                String* stringValue;
                std::wstring* wideStringValue;
            } data_;
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
        LogEntry(LogBuffer::Level level, const String& section, const String& format); 
        ~LogEntry();
        
        /// Appends a new argument to the entry.
        template <typename ValueType>
        LogEntry& operator << (const ValueType& v) {
            if(logBuffer_.enabled(level_)) {
                args_.push_back(new Arg(v));
            }
            return *this;
        }

        /// Returns the timestamp of the entry.
        Time when() const { return when_; }

        LogBuffer::Level level() const { return level_; }

        /// Converts the log entry to a string.
        String asText(const Flags& flags = 0) const;

        /// Make this entry print without metadata.
        LogEntry& simple() {
            defaultFlags_ |= SIMPLE;
            return *this;
        }

    private:
        void advanceFormat(String::const_iterator& i) const;

    private:
        LogBuffer& logBuffer_;
        Time when_;
        LogBuffer::Level level_;
        String section_;
        String format_;
        Flags defaultFlags_;

        typedef std::vector<Arg*> Args;        
        Args args_;
    };
    
    std::ostream& operator << (std::ostream& stream, const LogEntry::Arg& arg);
}

#endif /* LIBDENG2_LOG_H */
