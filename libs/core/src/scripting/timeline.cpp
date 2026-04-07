/** @file scheduler.cpp  Script scheduling utility.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/scripting/timeline.h"
#include "de/scripting/scriptedinfo.h"
#include "de/record.h"
#include "de/scripting/script.h"
#include "de/scripting/process.h"

#include <queue>
#include <deque>

namespace de {

DE_PIMPL(Timeline)
, DE_OBSERVES(Record, Deletion)
{
    Record *context = nullptr;

    struct Event {
        TimeSpan at;
        Script script;

        Event(TimeSpan at, const String &source, const String &sourcePath)
            : at(at)
            , script(source)
        {
            script.setPath(sourcePath); // where the source comes from
        }

        struct Compare {
            bool operator () (const Event *a, const Event *b) { return a->at > b->at; }
        };
    };
    typedef std::priority_queue<Event *, std::deque<Event *>, Event::Compare> Events;
    Events events;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        setContext(nullptr);
        clear();
    }

    void clear()
    {
        while (!events.empty())
        {
            delete events.top();
            events.pop();
        }
    }

    void setContext(Record *rec)
    {
        if (context) context->audienceForDeletion() -= this;
        context = rec;
        if (context) context->audienceForDeletion() += this;
    }

    void recordBeingDeleted(Record &record)
    {
        if (context == &record)
        {
            context = nullptr;
        }
    }
};

Timeline::Timeline()
    : d(new Impl(this))
{}

void Timeline::clear()
{
    d->clear();
}

void Timeline::setContext(Record &context)
{
    d->setContext(&context);
}

Record *Timeline::context() const
{
    return d->context;
}

Script &Timeline::addScript(TimeSpan at, const String &source, const String &sourcePath)
{
    auto *ev = new Impl::Event(at, source, sourcePath);
    d->events.push(ev);
    return ev->script;
}

void Timeline::addFromInfo(const Record &timelineRecord)
{
    auto scripts = ScriptedInfo::subrecordsOfType(ScriptedInfo::SCRIPT, timelineRecord);
    for (String key : ScriptedInfo::sortRecordsBySource(scripts))
    {
        const auto &def = *scripts[key];
        try
        {
            addScript(def.getd("at", 0.0),
                      def.gets(ScriptedInfo::SCRIPT),
                      ScriptedInfo::sourceLocation(def));
        }
        catch (const Error &er)
        {
            LOG_RES_ERROR("%s: Error in timeline script: %s")
                    << ScriptedInfo::sourceLocation(def)
                    << er.asText();
        }
    }
}

//----------------------------------------------------------------------------

DE_PIMPL_NOREF(Timeline::Clock)
{
    typedef Timeline::Impl::Event  Event;
    typedef Timeline::Impl::Events Events; // Events not owned

    Record *context = nullptr;
    const Timeline *scheduler = nullptr;
    TimeSpan at = 0.0;
    Events events;

    void rewind(TimeSpan toTime)
    {
        at = toTime;

        // Restore events into the queue.
        events = scheduler->d->events;
        while (!events.empty())
        {
            if (events.top()->at < at)
            {
                events.pop();
            }
            else break;
        }
    }

    void advanceTime(TimeSpan elapsed)
    {
        at += elapsed;

        while (!events.empty())
        {
            const Event *ev = events.top();
            if (ev->at > at) break;

            events.pop();

            // Execute the script in the specified context.
            Process process(context? context : scheduler->d->context);
            process.run(ev->script);
            process.execute();
        }
    }
};

Timeline::Clock::Clock(const Timeline &schedule, Record *context)
    : d(new Impl)
{
    d->scheduler = &schedule;
    d->context   = context;
    d->rewind(0.0);
}

TimeSpan Timeline::Clock::at() const
{
    return d->at;
}

void Timeline::Clock::rewind(TimeSpan toTime)
{
    d->rewind(toTime);
}

void Timeline::Clock::advanceTime(TimeSpan elapsed)
{
    d->advanceTime(elapsed);
}

bool Timeline::Clock::isFinished() const
{
    return d->events.empty();
}

} // namespace de
