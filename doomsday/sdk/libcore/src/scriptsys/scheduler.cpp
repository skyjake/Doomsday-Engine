/** @file scheduler.cpp  Script scheduling utility.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Scheduler"
#include "de/Record"
#include "de/Script"
#include "de/Process"

#include <queue>
#include <deque>

namespace de {

DENG2_PIMPL(Scheduler)
, DENG2_OBSERVES(Record, Deletion)
{
    Record *context = nullptr;
    TimeDelta at = 0.0;

    struct Event {
        TimeDelta at;
        Script script;

        Event(TimeDelta at, String const &source, String const &sourcePath)
            : at(at)
            , script(source)
        {
            script.setPath(sourcePath); // where the source comes from
        }

        struct Compare {
            bool operator () (Event const *a, Event const *b) { return a->at > b->at; }
        };
    };
    std::priority_queue<Event *, std::deque<Event *>, Event::Compare> events;
    QList<Event *> done;

    Instance(Public *i) : Base(i)
    {}

    ~Instance()
    {
        setContext(nullptr);
        clear();
    }

    void clear()
    {
        while(!events.empty())
        {
            delete events.top();
            events.pop();
        }
        qDeleteAll(done);
        done.clear();
        rewind();
    }

    void setContext(Record *rec)
    {
        if(context) context->audienceForDeletion() -= this;
        context = rec;
        if(context) context->audienceForDeletion() += this;
    }

    void recordBeingDeleted(Record &record)
    {
        if(context == &record)
        {
            context = nullptr;
        }
    }

    void rewind()
    {
        at = 0.0;

        // Restore all the past events into the queue.
        for(Event *ev : done)
        {
            events.push(ev);
        }
        done.clear();
    }

    void advanceTime(TimeDelta const &elapsed)
    {
        at += elapsed;

        while(!events.empty())
        {
            Event *ev = events.top();
            if(ev->at > at) break;

            events.pop();
            done.append(ev);

            // Execute the script in the specified context.
            Process process(context);
            process.run(ev->script);
            process.execute();
        }
    }
};

Scheduler::Scheduler()
    : d(new Instance(this))
{}

void Scheduler::clear()
{
    d->clear();
}

void Scheduler::setContext(Record &context)
{
    d->setContext(&context);
}

Script &Scheduler::addScript(TimeDelta at, String const &source, String const &sourcePath)
{
    auto *ev = new Instance::Event(at, source, sourcePath);
    d->events.push(ev);
    return ev->script;
}

TimeDelta Scheduler::at() const
{
    return d->at;
}

void Scheduler::rewind()
{
    d->rewind();
}

void Scheduler::advanceTime(TimeDelta const &elapsed)
{
    d->advanceTime(elapsed);
}

} // namespace de
