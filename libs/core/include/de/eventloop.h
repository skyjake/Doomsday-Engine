/** @file eventloop.h  Event loop.
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

#ifndef LIBCORE_EVENTLOOP_H
#define LIBCORE_EVENTLOOP_H

#include "de/event.h"
#include "de/observers.h"

namespace de {

/**
 * Event loop.
 *
 * When an event loop is running, it puts the thread to sleep until an event is posted. After
 * waking up, it calls the virtual EventLoop::processEvent() method that notifies the Event
 * audience about the received event.
 *
 * Events can be posted from any thread, but event processing and notifications only occur on the
 * thread where the event loop is running.
 *
 * The event loop can be stopped by posting a Quit event. This can be conveniently done with
 * the EventLoop::quit() method. The exit code in the event is returned to the caller of
 * EventLoop::exec().
 *
 * @ingroup core
 */
class DE_PUBLIC EventLoop
{
public:
    DE_AUDIENCE(Event, void eventPosted(const Event &))

    /// Returns the current event loop. It is assumed that only one thread is running event loops.
    /// Returns nullptr if no event loop is currently running.
    static EventLoop *get();

    enum RunMode { Automatic, Manual };

public:
    EventLoop(RunMode runMode = Automatic);

    virtual ~EventLoop();

    int exec(const std::function<void ()> &postExec = {});

    void quit(int exitCode);

    void processQueuedEvents();

    /**
     * Determines if this the currently running event loop. Note that if an event loop is
     * started inside another event loop, only the latest one is considered running.
     */
    bool isRunning() const;

    virtual void processEvent(const Event &event);

public:
    /**
     * Posts a new event into the event queue.
     *
     * @param event  Event to post. Ownership taken. The event will be deleted
     *               once it has been processed.
     */
    static void post(Event *event);

    static void callback(const std::function<void()> &);

    /**
     * Cancel pending events for which @a condition returns true.
     * @param condition  Filter condition.
     */
    static void cancel(const std::function<bool(Event *)> &cancelCondition);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_EVENTLOOP_H
