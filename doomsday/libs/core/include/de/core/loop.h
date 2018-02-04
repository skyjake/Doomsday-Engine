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

#ifndef LIBDENG2_LOOP_H
#define LIBDENG2_LOOP_H

#include <QObject>
#include <QList>
#include <de/Observers>
#include <de/Time>

#include <functional>

namespace de {

/**
 * Continually iterating loop, running as part of the Qt event loop.
 * Each frame/update originates from here.
 *
 * @ingroup core
 */
class DENG2_PUBLIC Loop : public QObject
{
    Q_OBJECT

public:
    /**
     * Audience to be notified each time the loop iterates.
     */
    DENG2_DEFINE_AUDIENCE2(Iteration, void loopIteration())

public:
    /**
     * Constructs a new loop with the default rate (iterating as often as
     * possible).
     */
    Loop();

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
     * Calls a function in the main thread. If the current thread is the main thread,
     * the function is called immediately. Otherwise a loop callback is enqueued.
     */
    static void mainCall(std::function<void ()> func);

    /**
     * Registers a new single-shot timer that will do a callback.
     *
     * @param delay  Time to wait before calling.
     * @param func   Callback to call.
     */
    static void timer(TimeSpan const &delay, std::function<void ()> func);

    static Loop &get();

public slots:
    virtual void nextLoopIteration();

private:
    DENG2_PRIVATE(d)
};

/**
 * Utility for deferring callbacks via the Loop.
 */
class DENG2_PUBLIC LoopCallback : public Lockable, DENG2_OBSERVES(Loop, Iteration)
{
public:
    typedef std::function<void ()> Callback;

    LoopCallback();
    ~LoopCallback();

    bool isEmpty() const;
    inline operator bool() const { return !isEmpty(); }

    void enqueue(Callback func);
    void loopIteration() override;

private:
    QList<Callback> _funcs;
};

} // namespace de

#endif // LIBDENG2_LOOP_H
