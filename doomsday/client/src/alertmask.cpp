/** @file alertmask.cpp
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

#include "alertmask.h"
#include "clientapp.h"

using namespace de;

DENG2_PIMPL_NOREF(AlertMask)
, DENG2_OBSERVES(Variable, Change)
{
    duint32 mask[LogEntry::HighestLogLevel + 1];

    Instance()
    {
        zap(mask);

        // By default, alerts are enabled for Warnings and above.
        mask[LogEntry::Warning] = mask[LogEntry::Error] = mask[LogEntry::Critical]
                = LogEntry::AllDomains;
    }

    void variableValueChanged(Variable &, Value const &)
    {
        updateMask();
    }

    void updateMask()
    {
        zap(mask);

        Config const &cfg = App::config();
        for(int bit = LogEntry::FirstDomainBit; bit <= LogEntry::LastDomainBit; ++bit)
        {
            int const alertLevel = cfg.geti(String("alert.") +
                                            LogFilter::domainRecordName(LogEntry::Context(1 << bit)));
            for(int i = LogEntry::LowestLogLevel; i <= LogEntry::HighestLogLevel; ++i)
            {
                if(alertLevel <= i)
                {
                    mask[i] |= (1 << bit);
                }
            }
        }

        qDebug() << "alert mask:";
        for(int i = 0; i < 8; ++i)
        {
            qDebug() << i << QString::number(mask[i], 16);
        }
    }
};

AlertMask::AlertMask() : d(new Instance)
{}

void AlertMask::init()
{
    foreach(Variable const *var, App::config().names().subrecord("alert").members())
    {
        var->audienceForChange += d;
    }
    d->updateMask();
}

bool AlertMask::shouldRaiseAlert(duint32 entryMetadata) const
{
    int const level = entryMetadata & LogEntry::LevelMask;
    return ((entryMetadata & LogEntry::DomainMask) & d->mask[level]) != 0;
}
