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

#ifndef LIBDENG2_LOGBUFFER_H
#define LIBDENG2_LOGBUFFER_H

#include "../Log"
#include "../File"

namespace de
{
    class LogEntry; 

    /**
     * Buffer for log entries. The application owns one of these.
     *
     * @ingroup core
     */
    class LogBuffer : OBSERVES(File, Deletion)
    {
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
        void enable(LogLevel overLevel = MESSAGE);
        
        /**
         * Disables the log. @see enable()
         */
        void disable() { enable(MAX_LOG_LEVELS); }
        
        bool enabled(LogLevel overLevel = MESSAGE) const { 
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
        
        /**
         * Sets the path of the file used for writing log entries to.
         *
         * @param path  Path of the file.
         */
        void setOutputFile(const String& path);
        
        /**
         * Flushes all unflushed entries to the defined outputs.
         */
        void flush();

        // Observes File deletion.
        void fileBeingDeleted(const File& file);
        
    private:
        dint enabledOverLevel_;
        duint maxEntryCount_;
        bool standardOutput_;
        File* outputFile_;

        typedef std::list<LogEntry*> Entries;
        Entries entries_;

        Entries toBeFlushed_;
        Time lastFlushedAt_;
    };    
}

#endif /* LIBDENG2_LOGBUFFER_H */
