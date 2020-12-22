/** @file loop.h  Continually triggered loop.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_LOOP_H
#define LIBCORE_LOOP_H

#include "de/observers.h"
#include "de/time.h"
#include "de/list.h"

#include <functional>

namespace de {

/**
 * Continually iterating loop, running as part of the application event loop.
 * Each frame/update originates from here.
 *
 * @ingroup core
 */
class DE_PUBLIC Loop
{
public:
    /**
     * Audience to be notified each time the loop iterates.
     */
    DE_AUDIENCE(Iteration, void loopIteration())

public:
    /**
     * Constructs a new loop with the default rate (iterating as often as
     * possible).
     */
    Loop();

    virtual ~Loop();

    /**
     * Sets the frequency for loop iteration (e.g., 35 Hz for a dedicated
     * server). Not very accurate: the actual rate at which the function is
     * called is likely less than this value (but never more frequently).
     *
     * @param freqHz  Frequency in Hz.
     */
    void setRate(double freqHz);

    double rate() const;

    /**
     * Starts the loop.
     */
    void start();

    /**
     * Stops the loop.
     */
    void stop();

    void pause();

    void resume();

    /**
     * Manually perform one iteration of the loop. Usually it is unnecessary to call this.
     */
    void iterate();

    /**
     * Calls a function in the main thread. If the current thread is the main thread,
     * the function is called immediately. Otherwise a loop callback is enqueued.
     */
    static void mainCall(const std::function<void ()> &func);

    /**
     * Registers a new single-shot timer that will do a callback.
     *
     * @param delay  Time to wait before calling.
     * @param func   Callback to call.
     */
    static void timer(TimeSpan delay, const std::function<void ()> &func);

    static Loop &get();

    virtual void nextLoopIteration();

private:
    DE_PRIVATE(d)
};

/**
 * Utility for deferring callbacks via the Loop to be called later in the main thread.
 *
 * Use this as a member in the object where the callback occurs in, so that if the
 * Dispatch is deleted, the callbacks will be cancelled.
 */
class DE_PUBLIC Dispatch : public Lockable, DE_OBSERVES(Loop, Iteration)
{
public:
    typedef std::function<void ()> Callback;

    Dispatch();
    ~Dispatch() override;

    bool isEmpty() const;
    inline operator bool() const { return !isEmpty(); }

    void enqueue(const Callback& func);
    inline Dispatch &operator+=(const Callback &func) { enqueue(func); return *this; }

    void loopIteration() override;

private:
    List<Callback> _funcs;
};

} // namespace de

#endif // LIBCORE_LOOP_H
