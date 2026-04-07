/** @file gltimer.h  GPU timer.
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

#pragma once

#include <de/id.h>
#include "de/libgui.h"

namespace de {

/**
 * GPU timer.
 *
 * Manages one or more GL timer queries. Each timer query can be started and
 * stopped independently.
 *
 * You should not try to create instances of GLTimer. Instead, use the one owned
 * by GLWindow.
 *
 * The timer results from the previous frame are available for reading.
 */
class LIBGUI_PUBLIC GLTimer
{
public:
    GLTimer();

    /**
     * Begins a timer query.
     *
     * @param id  Query identifier. The same Id should be used each frame so that
     *            results can be buffered.
     */
    void beginTimer(const Id &id);

    /**
     * Ends a timer query.
     *
     * @param id  Query identifier. The same Id should be used each frame so that
     *            results can be buffered.
     */
    void endTimer(const Id &id);

    /**
     * Returns the latest available timer query result. Note that result for a
     * particular timer query is not available immediately.
     *
     * @param id  Query identifier. The same Id should be used each frame so that
     *            results can be buffered.
     *
     * @return Time elapsed between the beginning and the end of the query.
     */
    TimeSpan elapsedTime(const Id &id) const;

private:
    DE_PRIVATE(d)
};

} // namespace de
