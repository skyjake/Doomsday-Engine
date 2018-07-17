/** @file timer.cpp  Simple timer.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Timer"
#include "de/CoreEvent"
#include "de/EventLoop"
#include "de/Thread"

#include <chrono>
#include <queue>

namespace sc = std::chrono;

namespace de {
namespace internal {

using TimePoint = sc::system_clock::time_point;

/**
 * Thread that posts timer events when it is time to trigger scheduled timers.
 */
struct TimerScheduler : public Thread, public Lockable
{
    struct Pending {
        TimePoint nextAt;
        Timer *   timer;
        TimeSpan  repeatDuration; // 0 for no repeat

        bool operator>(const Pending &other) const { return nextAt > other.nextAt; }
    };

    volatile bool running = true;
    Waitable      waiter;

    std::priority_queue<Pending, std::vector<Pending>, std::greater<Pending>> pending;

    void run() override
    {
        while (running)
        {
            TimeSpan timeToWait = 0.0;

            // Check the next pending trigger time.
            {
                DE_GUARD(this);
                while (!pending.empty())
                {
                    const auto now    = sc::system_clock::now();
                    const auto nextAt = pending.top().nextAt;

                    TimeSpan until = sc::duration_cast<sc::milliseconds>(nextAt - now).count() / 1.0e3;
                    if (until <= 0.0)
                    {
                        // Time to trigger this timer.
                        Pending pt = pending.top();
                        pending.pop();

                        // We'll have the event loop notify the timer's audience. This way the
                        // timer scheduling thread won't be blocked by slow trigger handlers.
                        pt.timer->post();

                        // Schedule the next trigger.
                        if (pt.repeatDuration > 0.0)
                        {
                            TimePoint nextAt = pt.nextAt + sc::microseconds(dint64(pt.repeatDuration * 1.0e6));
#if defined (DE_DEBUG)
                            // Debugger may halt the process, don't bother retrying to post.
                            nextAt = de::max(nextAt, sc::system_clock::now());
#endif
                            pending.push(Pending{nextAt, pt.timer, pt.repeatDuration});
                        }
                    }
                    else
                    {
                        timeToWait = until;
                        break;
                    }
                }
            }

            // Wait until it is time to post a timer event.
            // We'll wait indefinitely if no timers have been started.
            waiter.tryWait(timeToWait);
        }
    }

    void addPending(Timer &timer)
    {
        const TimeSpan repeatDuration = timer.interval();
        pending.push(
            Pending{sc::system_clock::now() + sc::microseconds(dint64(repeatDuration * 1.0e6)),
                    &timer,
                    timer.isSingleShot() ? TimeSpan() : repeatDuration});
        waiter.post();
    }

    void removePending(Timer &timer)
    {
        DE_GUARD(this);
        decltype(pending) filtered;
        while (!pending.empty())
        {
            const Pending pt = pending.top();
            if (pt.timer != &timer)
            {
                filtered.push(pt);
            }
            pending.pop();
        }
        pending = std::move(filtered);
    }

    void stop()
    {
        // Wake up.
        running = false;
        waiter.post();
        join();
    }
};

static LockableT<TimerScheduler *> scheduler;
static atomic_int timerCount; // Number of timers in existence.

} // namespace internal

DE_PIMPL_NOREF(Timer)
{
    TimeSpan interval     = 1.0;
    bool     isSingleShot = false;
    bool     isActive     = false;
    bool     isPending    = false; // posted but not executed yet

    ~Impl()
    {
        using namespace internal;

        // The timer scheduler thread is stopped after all timers have been deleted.
        DE_GUARD(scheduler);
        if (--timerCount == 0)
        {
            scheduler.value->stop();
            delete scheduler.value;
            scheduler.value = nullptr;
        }
    }

    DE_PIMPL_AUDIENCE(Trigger)
};

DE_AUDIENCE_METHOD(Timer, Trigger)

Timer::Timer()
    : d(new Impl)
{
    using namespace internal;

    DE_GUARD(scheduler);
    if (!scheduler.value)
    {
        scheduler.value = new TimerScheduler;
        scheduler.value->start();
    }
    ++timerCount;
}

Timer::~Timer()
{
    // The timer must be first stopped and all pending triggers must be handled.
    stop();

    /// @todo There might still be Timer events queued up in the event loop.
}

void Timer::setInterval(const TimeSpan &interval)
{
    d->interval = interval;
}

void Timer::setSingleShot(bool singleshot)
{
    d->isSingleShot = singleshot;
}

void Timer::start()
{
    using namespace internal;

    if (!d->isActive)
    {
        d->isActive = true;
        scheduler.value->addPending(*this);
    }
}

void Timer::trigger()
{
    DE_FOR_AUDIENCE2(Trigger, i) { i->triggered(*this); }

    if (d->isSingleShot)
    {
        d->isActive = false;
    }
}

void Timer::post()
{
    if (!d->isPending)
    {
        if (auto *loop = EventLoop::get())
        {
            d->isPending = true; // not reposted before triggered
            loop->postEvent(new CoreEvent(Event::Timer, [this]() {
                d->isPending = false;
                trigger();
            }));
        }
        else
        {
            warning("[TimerScheduler] Pending timer %p triggered with no event loop running (event "
                    "not posted)", this);
        }
    }
}

Timer &Timer::operator+=(const std::function<void ()> &callback)
{
    audienceForTrigger() += callback;
    return *this;
}

void Timer::stop()
{
    using namespace internal;

    if (d->isActive)
    {
        scheduler.value->removePending(*this);
        d->isActive = false;
    }
}

bool Timer::isActive() const
{
    return d->isActive;
}

bool Timer::isSingleShot() const
{
    return d->isSingleShot;
}

TimeSpan Timer::interval() const
{
    return d->interval;
}

} // namespace de
