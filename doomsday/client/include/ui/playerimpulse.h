/** @file playerimpulse.h  Player interaction impulse.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_PLAY_PLAYERIMPULSE_H
#define CLIENT_PLAY_PLAYERIMPULSE_H

#include <de/String>
#include "api_player.h"

/**
 * Receives player interaction impulses and normalizes them for later consumption
 * by the player Brain (on game side).
 *
 * @todo The player Brain should have ownership of it's ImpulseAccumulators.
 *
 * @todo Cleanup client/server confusion. On server side, each player will have a
 * local model of a remote human player's impulses. However, Double-clicks can be
 * handled entirely on client side. -ds
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
    DENG2_PRIVATE(d)
};

/**
 * Describes a player interaction impulse.
 *
 * @ingroup playsim
 */
struct PlayerImpulse
{
    int id = 0;
    impulsetype_t type;
    de::String name;             ///< Symbolic. Used when resolving or generating textual binding descriptors.
    de::String bindContextName;  ///< Symbolic name of the associated binding context.

#ifdef __CLIENT__
    /**
     * Returns @c true if one or more bindings for this impulse exist, for the
     * given @a localPlayer number in the associated BindContext.
     *
     * @param localPlayer  Local player number.
     */
    bool haveBindingsFor(int playerNumber) const;
#endif
};

void P_ImpulseShutdown();

PlayerImpulse *P_ImpulsePtr(int id);

PlayerImpulse *P_ImpulseByName(de::String const &name);

/**
 * Register the console commands and variables of this module.
 */
void P_ImpulseConsoleRegister();

#endif // CLIENT_PLAY_PLAYERIMPULSE_H
