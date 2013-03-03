/** @file loop.h  Continually triggered loop.
 *
 * Long summary of the functionality.
 *
 * @todo Update the fields above as appropriate.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBDENG2_LOOP_H
#define LIBDENG2_LOOP_H

#include <QObject>
#include <de/Observers>

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
    DENG2_DEFINE_AUDIENCE(Iteration, void loopIteration())

public:
    /**
     * Constructs a new loop with the default rate (iterating as often as
     * possible).
     */
    Loop();

    ~Loop();

    /**
     * Sets the frequency for loop iteration (e.g., 35 Hz for a dedicated
     * server). Not very accurate: the actual rate at which the function is
     * called is likely less than this value (but never more frequently).
     *
     * @param freqHz  Frequency in Hz.
     */
    void setRate(int freqHz);

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

public slots:
    void nextLoopIteration();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_LOOP_H
