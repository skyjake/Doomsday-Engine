/** @file impulseaccumulator.h  Player impulse accumulation.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_PLAY_IMPULSEACCUMULATOR_H
#define CLIENT_PLAY_IMPULSEACCUMULATOR_H

#include <de/string.h>

/**
 * Receives player interaction impulses and normalizes them for later consumption
 * by the player Brain (on game side).
 *
 * @todo The player Brain should have ownership of it's ImpulseAccumulators.
 *
 * @ingroup playsim
 */
class ImpulseAccumulator
{
public:
    enum AccumulatorType
    {
        Analog,
        Binary
    };

public:
    /**
     * @param impulseId  Unique identifier of the player impulse to accumulate for.
     * @param type       Logical accumulator type.
     *
     * @param expireBeforeSharpTick  If the source of the accumulation has changed
     * state when a sharp tick occurs, the accumulation will expire automatically.
     * For example, if the key bound to "attack" is not held down when a sharp tick
     * occurs, it should not considered active even though it has been pressed and
     * released since the previous sharp tick.
     */
    ImpulseAccumulator(int impulseId, AccumulatorType type, bool expireBeforeSharpTick);

    /**
     * Returns the unique identifier of the impulse.
     */
    int impulseId() const;

    AccumulatorType type() const;

    bool expireBeforeSharpTick() const;

    void setPlayerNum(int newPlayerNum);

    // ---

    void receiveBinary();

    int takeBinary();

#ifdef __CLIENT__
    void takeAnalog(float *pos = nullptr, float *relOffset = nullptr);

    void clearAll();

public:

    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();
#endif

private:
    DE_PRIVATE(d)
};

#endif // CLIENT_PLAY_IMPULSEACCUMULATOR_H
