/** @file styledlogsinkformatter.cpp  Rich text log entry formatter.
 *
 * @authors Copyright (c) 2013-2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/styledlogsinkformatter.h"
#include "de/config.h"

namespace de {

static const char *VAR_METADATA = "log.showMetadata";

DE_PIMPL(StyledLogSinkFormatter)
, DE_OBSERVES(Variable, Change)
{
    Flags format;
    bool  observe;
    bool  omitSectionIfNonDev = true;
    bool  showMetadata        = false;

    Impl(Public *i, bool observeVars)
        : Base(i)
        , observe(observeVars)
    {
        if (observe)
        {
            showMetadata = Config::get().getb(VAR_METADATA);
            Config::get(VAR_METADATA).audienceForChange() += this;
        }
    }

    void variableValueChanged(Variable &, const Value &newValue)
    {
        showMetadata = newValue.isTrue();
    }
};

StyledLogSinkFormatter::StyledLogSinkFormatter()
    : d(new Impl(this, true /*observe*/))
{
    d->format = LogEntry::Styled | LogEntry::OmitLevel;
}

StyledLogSinkFormatter::StyledLogSinkFormatter(const Flags &formatFlags)
    : d(new Impl(this, false /*don't observe*/))
{
    d->format = formatFlags;
}

LogSink::IFormatter::Lines StyledLogSinkFormatter::logEntryToTextLines(const LogEntry &entry)
{
    Flags formatFlags = d->format;

    if (!d->showMetadata)
    {
        formatFlags |= LogEntry::Simple | LogEntry::OmitDomain;
    }

    if (d->omitSectionIfNonDev && !(entry.context() & LogEntry::Dev))
    {
        // The sections refer to names of native code functions, etc.
        // These are relevant only to developers. Non-dev messages must be
        // clear enough to understand without the sections.
        formatFlags |= LogEntry::OmitSection;
    }

    // This will form a single long line. The line wrapper will
    // then determine how to wrap it onto the available width.
    return {entry.asText(formatFlags)};
}

void StyledLogSinkFormatter::setOmitSectionIfNonDev(bool omit)
{
    d->omitSectionIfNonDev = omit;
}

void StyledLogSinkFormatter::setShowMetadata(bool show)
{
    d->showMetadata = show;
}

} // namespace de
