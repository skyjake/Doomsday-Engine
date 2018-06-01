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

#include "de/Scheduler"

#include <QHash>

namespace de {

DE_PIMPL_NOREF(Scheduler)
{
    struct RunningTimeline
    {
        Timeline const *timeline = nullptr;
        std::unique_ptr<Timeline::Clock> clock;
        bool isOwned = false;

        ~RunningTimeline()
        {
            if (isOwned) delete timeline;
        }
    };

    QHash<String, RunningTimeline *> running;
    duint64 counter = 0;

    ~Impl()
    {
        clear();
    }

    void clear()
    {
        qDeleteAll(running);
        running.clear();
    }

    String internalName(String const &publicName)
    {
        if (publicName.isEmpty())
        {
            // Choose a name automatically.
            return QString("__TL%1__").arg(counter++, 0, 16);
        }
        return publicName;
    }

    String start(RunningTimeline *run, String const &name)
    {
        String const intName = internalName(name);
        stop(intName);
        running.insert(intName, run);
        return intName;
    }

    void stop(String const &intName)
    {
        if (running.contains(intName))
        {
            delete running[intName];
            running.remove(intName);
        }
    }

    void advanceTime(TimeSpan const &elapsed)
    {
        QMutableHashIterator<String, RunningTimeline *> iter(running);
        while (iter.hasNext())
        {
            RunningTimeline *rt = iter.next().value();
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

String Scheduler::start(Timeline *timeline, String const &name)
{
    auto *run = new Impl::RunningTimeline;
    run->isOwned = true;
    run->timeline = timeline;
    run->clock.reset(new Timeline::Clock(*timeline, timeline->context()));
    return d->start(run, name);
}

String Scheduler::start(Timeline const &sharedTimeline, Record *context, String const &name)
{
    auto *run = new Impl::RunningTimeline;
    run->isOwned = false;
    run->timeline = &sharedTimeline;
    run->clock.reset(new Timeline::Clock(sharedTimeline, context));
    return d->start(run, name);
}

void Scheduler::stop(String const &name)
{
    d->stop(name);
}

void Scheduler::advanceTime(TimeSpan const &elapsed)
{
    d->advanceTime(elapsed);
}

} // namespace de
