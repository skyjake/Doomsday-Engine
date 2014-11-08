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

#ifndef CLIENT_INPUTSYSTEM_PLAYERIMPULSE_H
#define CLIENT_INPUTSYSTEM_PLAYERIMPULSE_H

#include <de/String>
#include "api_player.h"

/**
 * Describes a player interaction impulse.
 *
 * @todo Is "take" the wrong verb in this context?
 * Player impulses are acted upon by the player Brain (on game side). Does it make
 * sense for a "brain" to "consume" an impulse? (Also note established convention
 * in Qt containers for removing an element from the container). Perhaps we need
 * another abstraction here? -ds
 *
 * @todo Cleanup client/server confusion. On server side, each player will have a
 * local model of a remote human player's impulses. However, Double-clicks can be
 * handled entirely on client side. -ds
 */
class PlayerImpulse
{
public:
    impulsetype_t type;

public:
    PlayerImpulse(int id, impulsetype_t type, de::String const &name,
                  de::String bindContextName);

    bool isTriggerable() const;

    /**
     * Returns the unique identifier of the impulse.
     */
    int id() const;

    /**
     * Returns the symbolic name of the impulse. This name is used when resolving
     * or generating textual binding descriptors.
     */
    de::String name() const;

    /**
     * Returns the symbolic name of the attributed binding context.
     */
    de::String bindContextName() const;

#ifdef __CLIENT__
    /**
     * Returns @c true if one or more ImpulseBindings exist in @em any bindContext.
     *
     * @param playerNum  Console/player number.
     */
    bool haveBindingsFor(int playerNum) const;
#endif

public:
    /**
     * @param playerNum  Console/player number.
     */
    void triggerBoolean(int playerNum);

    /**
     * @param playerNum  Console/player number.
     */
    int takeBoolean(int playerNum);

#ifdef __CLIENT__
    /**
     * @param playerNum  Console/player number.
     * @param pos
     * @param relOffset
     */
    void takeNumeric(int playerNum, float *pos = nullptr, float *relOffset = nullptr);

    /**
     * @param playerNum  Console/player number.
     */
    int takeDoubleClick(int playerNum);

    /**
     * @param playerNum  Console/player number. Use @c < 0 || >= DDMAXPLAYERS for all.
     */
    void clearAccumulation(int playerNum = -1 /*all local players*/);

public:

    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

#endif

private:
    DENG2_PRIVATE(d)
};

void P_ImpulseShutdown();

PlayerImpulse *P_ImpulsePtr(int id);

PlayerImpulse *P_ImpulseByName(de::String const &name);

/**
 * Register the console commands and cvars of the player controls subsystem.
 */
void P_ConsoleRegister();

#endif // CLIENT_INPUTSYSTEM_PLAYERIMPULSE_H
