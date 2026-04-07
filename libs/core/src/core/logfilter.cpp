/** @file logfilter.cpp  Log entry filter.
 *
 * @authors Copyright © 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/logfilter.h"

namespace de {

namespace internal {
    enum FilterId {
        GenericFilter,
        ResourceFilter,
        MapFilter,
        ScriptFilter,
        GLFilter,
        AudioFilter,
        InputFilter,
        NetworkFilter,
        NUM_FILTERS
    };
    static const char *subRecName[NUM_FILTERS] = { // for Config
        "generic",
        "resource",
        "map",
        "script",
        "gl",
        "audio",
        "input",
        "network"
    };
}

using namespace internal;

/**
 * Filter for determining which log entries will be put in the log buffer.
 */
DE_PIMPL_NOREF(LogFilter)
{
    /// Filtering information for a domain.
    struct Filter {
        int domainBit;
        LogEntry::Level minLevel;
        bool allowDev;

        Filter()
            : domainBit(LogEntry::GenericBit)
            , minLevel(LogEntry::Message)
            , allowDev(false)
        {}

        inline bool checkContextBit(duint32 md) const
        {
            return (md & (1 << domainBit)) != 0;
        }

        void read(const Record &rec)
        {
            minLevel = LogEntry::Level(dint(rec["minLevel"].value().asNumber()));
            allowDev = rec["allowDev"].value().isTrue();
        }

        void write(Record &rec) const
        {
            rec.set("minLevel", dint(minLevel));
            rec.set("allowDev", allowDev);
        }
    };

    Filter filterByContext[NUM_FILTERS];

    Impl()
    {
        for (int i = 0; i < NUM_FILTERS; ++i)
        {
            filterByContext[i].domainBit = LogEntry::FirstDomainBit + i;
        }
    }

    bool isLogEntryAllowed(duint32 md) const
    {
        // Multiple contexts allowed, in which case if any one passes,
        // the entry is allowed.
        for (uint i = 0; i < NUM_FILTERS; ++i)
        {
            const Filter &ftr = filterByContext[i];
            if (ftr.checkContextBit(md))
            {
                if ((md & LogEntry::Dev) && !ftr.allowDev) continue; // No devs.
                if (ftr.minLevel <= int(md & LogEntry::LevelMask))
                {
                    // Pass due to entry level being enabled.
                    return true;
                }
                if ((md & LogEntry::Interactive) && i == ScriptFilter)
                {
                    // Interactive script entries pass.
                    return true;
                }
            }
        }
        return false;
    }

    LogEntry::Level minLevel(duint32 md) const
    {
        int lev = LogEntry::HighestLogLevel + 1;
        for (uint i = 0; i < NUM_FILTERS; ++i)
        {
            const Filter &ftr = filterByContext[i];
            if (ftr.checkContextBit(md))
            {
                lev = de::min(lev, int(ftr.minLevel));
            }
        }
        return LogEntry::Level(lev);
    }

    bool allowDev(duint32 md) const
    {
        for (uint i = 0; i < NUM_FILTERS; ++i)
        {
            const Filter &ftr = filterByContext[i];
            if (ftr.checkContextBit(md))
            {
                if (ftr.allowDev) return true;
            }
        }
        return false;
    }

    void setAllowDev(duint32 md, bool allow)
    {
        for (uint i = 0; i < NUM_FILTERS; ++i)
        {
            Filter &ftr = filterByContext[i];
            if (ftr.checkContextBit(md))
            {
                ftr.allowDev = allow;
            }
        }
    }

    void setMinLevel(duint32 md, LogEntry::Level level)
    {
        for (uint i = 0; i < NUM_FILTERS; ++i)
        {
            Filter &ftr = filterByContext[i];
            if (ftr.checkContextBit(md))
            {
                ftr.minLevel = level;
            }
        }
    }

    void read(const Record &rec)
    {
        try
        {
            for (uint i = 0; i < NUM_FILTERS; ++i)
            {
                filterByContext[i].read(rec.subrecord(subRecName[i]));
            }
        }
        catch (const Error &er)
        {
            LOGDEV_WARNING("Failed to read filter from record: %s\nThe record is:\n%s")
                    << er.asText() << rec.asText();

            LOG_WARNING("Log filter reset to defaults");
            *this = Impl(); // Reset.
        }
    }

    void write(Record &rec) const
    {
        for (uint i = 0; i < NUM_FILTERS; ++i)
        {
            // Reuse existing subrecords.
            if (!rec.hasSubrecord(subRecName[i]))
            {
                rec.add(subRecName[i], new Record);
            }
            filterByContext[i].write(rec.subrecord(subRecName[i]));
        }
    }
};

LogFilter::LogFilter() : d(new Impl)
{}

bool LogFilter::isLogEntryAllowed(duint32 metadata) const
{
    DE_ASSERT(metadata & LogEntry::DomainMask); // must have a domain
    return d->isLogEntryAllowed(metadata);
}

void LogFilter::setAllowDev(duint32 md, bool allow)
{
    d->setAllowDev(md, allow);
}

void LogFilter::setMinLevel(duint32 md, LogEntry::Level level)
{
    d->setMinLevel(md, level);
}

bool LogFilter::allowDev(duint32 md) const
{
    return d->allowDev(md);
}

LogEntry::Level LogFilter::minLevel(duint32 md) const
{
    return d->minLevel(md);
}

void LogFilter::read(const Record &rec)
{
    d->read(rec);
}

void LogFilter::write(Record &rec) const
{
    d->write(rec);
}

String LogFilter::domainRecordName(LogEntry::Context domain)
{
    for (int i = LogEntry::FirstDomainBit; i <= LogEntry::LastDomainBit; ++i)
    {
        if (domain & (1 << i)) return subRecName[i - LogEntry::FirstDomainBit];
    }
    return "";
}

} // namespace de
