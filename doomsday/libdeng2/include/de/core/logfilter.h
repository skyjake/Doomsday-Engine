/** @file logfilter.h  Log entry filter.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_LOGFILTER_H
#define LIBDENG2_LOGFILTER_H

#include "../Log"
#include "../LogBuffer"
#include "../Record"

namespace de {

/**
 * Filter for determining which log entries will be put in a LogBuffer.
 *
 * Filtering is done separately for each entry domain. Also, developer entries can be
 * separately allowed or disallowed for each domain.
 *
 * The filter can be read from and written to a Record. This is used for saving the
 * filter to Config.
 *
 * @ingroup core
 */
class DENG2_PUBLIC LogFilter : public LogBuffer::IFilter
{
public:
    LogFilter();

    bool isLogEntryAllowed(duint32 metadata) const;

    void setAllowDev(duint32 md, bool allow);
    void setAllowDev(bool allow) { setAllowDev(LogEntry::AllDomains, allow); }
    void setMinLevel(duint32 md, LogEntry::Level level);
    void setMinLevel(LogEntry::Level level) { setMinLevel(LogEntry::AllDomains, level); }

    bool allowDev(duint32 md) const;
    LogEntry::Level minLevel(duint32 md) const;

    void read(Record const &rec);
    void write(Record &rec) const;

private:
    DENG2_PRIVATE(d)
};

/**
 * Very basic log filter that allows non-dev Messages in a release build, and all Verbose
 * messages in a debug build.
 *
 * @ingroup core
 */
struct DENG2_PUBLIC SimpleLogFilter : public LogBuffer::IFilter
{
    bool isLogEntryAllowed(duint32 metadata) const
    {
#ifdef DENG2_DEBUG
        return (metadata & LogEntry::LevelMask) >= LogEntry::Verbose;
#else
        return !(metadata & LogEntry::Dev) &&
               (metadata & LogEntry::LevelMask) >= LogEntry::Message;
#endif
    }
};

} // namespace de

#endif // LIBDENG2_LOGFILTER_H
