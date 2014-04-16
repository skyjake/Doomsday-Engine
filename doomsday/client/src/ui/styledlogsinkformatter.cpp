/** @file styledlogsinkformatter.cpp
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

#include "ui/styledlogsinkformatter.h"
#include <de/Variable>
#include <de/Value>
#include <de/App>

using namespace de;

static char const *VAR_METADATA = "log.showMetadata";

DENG2_PIMPL(StyledLogSinkFormatter)
, DENG2_OBSERVES(Variable, Change)
{
    LogEntry::Flags format;
    bool observe;
    bool omitSectionIfNonDev;
    bool showMetadata;

    Instance(Public *i, bool observeVars)
        : Base(i)
        , observe(observeVars)
        , omitSectionIfNonDev(true)
        , showMetadata(false)
    {
        if(observe)
        {
            showMetadata = App::config().getb(VAR_METADATA);
            App::config()[VAR_METADATA].audienceForChange() += this;
        }
    }

    ~Instance()
    {
        if(observe)
        {
            App::config()[VAR_METADATA].audienceForChange() -= this;
        }
    }

    void variableValueChanged(Variable &, Value const &newValue)
    {
        showMetadata = newValue.isTrue();
    }
};

StyledLogSinkFormatter::StyledLogSinkFormatter()
    : d(new Instance(this, true /*observe*/))
{
    d->format = LogEntry::Styled | LogEntry::OmitLevel;
}

StyledLogSinkFormatter::StyledLogSinkFormatter(LogEntry::Flags const &formatFlags)
    : d(new Instance(this, false /*don't observe*/))
{
    d->format = formatFlags;
}

LogSink::IFormatter::Lines StyledLogSinkFormatter::logEntryToTextLines(LogEntry const &entry)
{
    LogEntry::Flags form = d->format;

    if(!d->showMetadata)
    {
        form |= LogEntry::Simple | LogEntry::OmitDomain;
    }

    if(d->omitSectionIfNonDev && !(entry.context() & LogEntry::Dev))
    {
        // The sections refer to names of native code functions, etc.
        // These are relevant only to developers. Non-dev messages must be
        // clear enough to understand without the sections.
        form |= LogEntry::OmitSection;
    }

    // This will form a single long line. The line wrapper will
    // then determine how to wrap it onto the available width.
    return Lines() << entry.asText(form);
}

void StyledLogSinkFormatter::setOmitSectionIfNonDev(bool omit)
{
    d->omitSectionIfNonDev = omit;
}
