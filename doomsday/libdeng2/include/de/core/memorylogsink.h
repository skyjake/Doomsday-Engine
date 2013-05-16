/** @file memorylogsink.h  Log sink that stores log entries in memory.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBDENG2_MEMORYLOGSINK_H
#define LIBDENG2_MEMORYLOGSINK_H

#include "../LogSink"
#include "../Lockable"

#include <QList>

namespace de {

/**
 * Log sink that stores log entries in memory.
 */
class MemoryLogSink : public LogSink, public Lockable
{
public:
    MemoryLogSink();
    ~MemoryLogSink();

    LogSink &operator << (LogEntry const &entry);
    LogSink &operator << (String const &plainText);

    void flush() {}

    int entryCount() const;
    LogEntry const &entry(int index) const;
    void remove(int pos, int n = 1);
    void clear();

protected:
    virtual void addedNewEntry(LogEntry &entry);

private:
    QList<LogEntry *> _entries;
};

} // namespace de

#endif // LIBDENG2_MEMORYLOGSINK_H
