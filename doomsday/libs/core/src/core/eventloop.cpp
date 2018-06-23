/** @file eventloop.cpp  Event loop.
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

#include "de/EventLoop"

#include "de/CoreEvent"
#include "de/Garbage"
#include "de/Log"
#include "de/NumberValue"
#include "de/ThreadLocal"
#include "de/WaitableFIFO"

namespace de {
namespace internal {

static LockableT<List<EventLoop *>> loopStack;

struct StackPusher {
    StackPusher(EventLoop *loop) { DE_GUARD(loopStack); loopStack.value.push_back(loop); }
    ~StackPusher()               { DE_GUARD(loopStack); loopStack.value.pop_back(); }
};

} // namespace internal

DE_PIMPL_NOREF(EventLoop)
{
    WaitableFIFO<Event> queue;
    DE_PIMPL_AUDIENCE(Event)
};
DE_AUDIENCE_METHOD(EventLoop, Event)

EventLoop::EventLoop() : d(new Impl)
{}

EventLoop::~EventLoop()
{}

int EventLoop::exec(const std::function<void ()> &postExec)
{
    try
    {
        internal::StackPusher sp(this);
        postExec();
        for (;;)
        {
            // Wait until an event is posted.
            std::unique_ptr<Event> event(d->queue.take());

            // Notify observers and/or the subclass.
            processEvent(*event);

            if (event->type() == Event::Quit)
            {
                return event->as<CoreEvent>().valuei();
            }
            if (d->queue.isEmpty())
            {
                // Nothing to do immediately, so take out the trash.
                Garbage_Recycle();
            }
        }
    }
    catch (const Error &er)
    {
        warning("[EventLoop] Event loop terminating due to an uncaught exception");
        er.warnPlainText();
        LOG_WARNING("Event loop stopped: %s") << er.asText();
        return 0;
    }
}

void EventLoop::quit(int exitCode)
{
    postEvent(new CoreEvent(Event::Quit, NumberValue(exitCode)));
}

void EventLoop::postEvent(Event *event)
{
    d->queue.put(event);
}

void EventLoop::processEvent(const Event &event)
{
    DE_FOR_AUDIENCE2(Event, i) { i->eventPosted(event); }

    // Handle core events.
    switch (event.type())
    {
    case Event::Callback:
    case Event::Timer:
        event.as<CoreEvent>().callback()();
        break;
    }
}

EventLoop *EventLoop::get()
{
    using namespace internal;
    DE_GUARD(loopStack);
    if (loopStack.value)
    {
        return loopStack.value.back();
    }
    return nullptr;
}

} // namespace de
