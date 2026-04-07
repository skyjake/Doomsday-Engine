/** @file memorylogsink.h  Log sink that stores log entries in memory.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_MEMORYLOGSINK_H
#define LIBCORE_MEMORYLOGSINK_H

#include "de/logsink.h"
#include "de/lockable.h"

namespace de {

/**
 * Log sink that stores log entries in memory.
 * @ingroup core
 */
class DE_PUBLIC MemoryLogSink : public LogSink, public Lockable
{
public:
    MemoryLogSink(LogEntry::Level minimumLevel = LogEntry::XVerbose);
    ~MemoryLogSink();

    void setPrivileged(bool onlyPrivileged);

    LogSink &operator << (const LogEntry &entry);
    LogSink &operator << (const String &plainText);

    void flush() {}

    int entryCount() const;
    const LogEntry &entry(int index) const;
    void remove(int pos, int n = 1);
    void clear();

protected:
    /**
     * Called after a new entry has been appended to the end of the entries list.
     *
     * @param entry  Added entry.
     */
    virtual void addedNewEntry(LogEntry &entry);

private:
    List<LogEntry *> _entries;
    LogEntry::Level _minLevel;
    bool _privileged;
};

} // namespace de

#endif // LIBCORE_MEMORYLOGSINK_H
