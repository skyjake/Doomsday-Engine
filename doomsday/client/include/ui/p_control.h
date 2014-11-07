/** @file p_control.h  Player interaction impulse.
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
 */
struct PlayerImpulse
{
    int id;
    impulsetype_t type;
    de::String name;
    de::String bindContextName;

    short booleanCounts[DDMAXPLAYERS];

#ifdef __CLIENT__
    /**
     * Double-"clicks" actually mean double activations that occur within the double-click
     * threshold. This is to allow double-clicks also from the numeric impulses.
     */
    struct DoubleClick
    {
        enum State
        {
            None,
            Positive,
            Negative
        };

        bool triggered = false;           //< True if double-click has been detected.
        uint previousClickTime = 0;       //< Previous time an activation occurred.
        State lastState = None;           //< State at the previous time the check was made.
        State previousClickState = None;  /** Previous click state. When duplicated, triggers
                                              the double click. */
    } doubleClicks[DDMAXPLAYERS];
#endif

    PlayerImpulse(int id, impulsetype_t type, de::String const &name, de::String bindContextName)
        : id             (id)
        , type           (type)
        , name           (name)
        , bindContextName(bindContextName)
    {
        de::zap(booleanCounts);
    }

    /**
     * Returns @c true if the impulse is triggerable.
     */
    inline bool isTriggerable() const {
        return (type == IT_NUMERIC_TRIGGERED || type == IT_BOOLEAN);
    }

#ifdef __CLIENT__
    /**
     * Updates the double-click state of an impulse and marks it as double-clicked
     * when the double-click condition is met.
     *
     * @param playerNum  Player/console number.
     * @param pos        State of the impulse.
     */
    void maintainDoubleClicks(int playerNum, float pos);

    int takeDoubleClick(int playerNum);

    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();
#endif
};

void P_ImpulseShutdown();

PlayerImpulse *P_ImpulseById(int id);

PlayerImpulse *P_ImpulseByName(de::String const &name);

/**
 * Register the console commands and cvars of the player controls subsystem.
 */
void P_ConsoleRegister();

#endif // CLIENT_INPUTSYSTEM_PLAYERIMPULSE_H
