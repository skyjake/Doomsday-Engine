/** @file scheduler.cpp  Scheduler for scripts and timelines.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/scripting/scheduler.h"
#include "de/hash.h"

namespace de {

DE_PIMPL_NOREF(Scheduler)
{
    struct RunningTimeline
    {
        const Timeline *timeline = nullptr;
        std::unique_ptr<Timeline::Clock> clock;
        bool isOwned = false;

        ~RunningTimeline()
        {
            if (isOwned) delete timeline;
        }
    };

    Hash<String, RunningTimeline *> running;
    duint64 counter = 0;

    ~Impl()
    {
        clear();
    }

    void clear()
    {
        running.deleteAll();
        running.clear();
    }

    String internalName(const String &publicName)
    {
        if (publicName.isEmpty())
        {
            // Choose a name automatically.
            return Stringf("__TL%x__", counter++);
        }
        return publicName;
    }

    String start(RunningTimeline *run, const String &name)
    {
        const String intName = internalName(name);
        stop(intName);
        running.insert(intName, run);
        return intName;
    }

    void stop(const String &intName)
    {
        if (running.contains(intName))
        {
            delete running[intName];
            running.remove(intName);
        }
    }

    void advanceTime(TimeSpan elapsed)
    {
        MutableHashIterator<String, RunningTimeline *> iter(running);
        while (iter.hasNext())
        {
            RunningTimeline *rt = iter.next()->second;
            rt->clock->advanceTime(elapsed);
            if (rt->clock->isFinished())
            {
                delete rt;
                iter.remove();
            }
        }
    }
};

Scheduler::Scheduler()
    : d(new Impl)
{}

void Scheduler::clear()
{
    d->clear();
}

String Scheduler::start(Timeline *timeline, const String &name)
{
    auto *run = new Impl::RunningTimeline;
    run->isOwned = true;
    run->timeline = timeline;
    run->clock.reset(new Timeline::Clock(*timeline, timeline->context()));
    return d->start(run, name);
}

String Scheduler::start(const Timeline &sharedTimeline, Record *context, const String &name)
{
    auto *run = new Impl::RunningTimeline;
    run->isOwned = false;
    run->timeline = &sharedTimeline;
    run->clock.reset(new Timeline::Clock(sharedTimeline, context));
    return d->start(run, name);
}

void Scheduler::stop(const String &name)
{
    d->stop(name);
}

void Scheduler::advanceTime(TimeSpan elapsed)
{
    d->advanceTime(elapsed);
}

} // namespace de
