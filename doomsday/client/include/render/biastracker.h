/** @file biastracker.h Shadow Bias change tracking buffer.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_SHADOWBIAS_TRACKER_H
#define DENG_RENDER_SHADOWBIAS_TRACKER_H

#include "world/map.h"

/**
 * Change tracking buffer for the Shadow Bias lighting model.
 */
class BiasTracker
{
public:
    static int const MAX_TRACKED = (de::Map::MAX_BIAS_SOURCES / 8);

public:
    BiasTracker();

    BiasTracker &operator = (BiasTracker const &other);

    /**
     * Clear the tracker reseting all bits to zero.
     */
    void clear();

    /**
     * Sets/clears a bit in the tracker for the given index.
     */
    void mark(uint index);

    /**
     * Checks if the given index bit is set in the tracker.
     */
    int check(uint index) const;

    /**
     * Copies changes from src.
     */
    void apply(BiasTracker const &src);

    /**
     * Remove changes of src.
     */
    void remove(BiasTracker const &src);

private:
    uint _changes[MAX_TRACKED];
};

#endif // DENG_RENDER_SHADOWBIAS_TRACKER_H
