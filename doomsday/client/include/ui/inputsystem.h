/** @file inputsystem.h  Input subsystem.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_INPUTSYSTEM_H
#define CLIENT_INPUTSYSTEM_H

#include <functional>
#include <de/types.h>
#include <de/System>
#include "dd_input.h" // ddevent_t
#include "SettingsRegister"

class InputDevice;

/**
 * Input devices and events. @ingroup ui
 *
 * @todo Input drivers belong in this system.
 */
class InputSystem : public de::System
{
public:
    InputSystem();

    SettingsRegister &settings();

    // System.
    void timeChanged(de::Clock const &);

    /**
     * Lookup an InputDevice by it's unique @a id.
     */
    InputDevice &device(int id) const;

    /**
     * Lookup an InputDevice by it's unique @a id.
     *
     * @return  Pointer to the associated InputDevice; otherwise @c nullptr.
     */
    InputDevice *devicePtr(int id) const;

    /**
     * Iterate through all the InputDevices.
     */
    de::LoopResult forAllDevices(std::function<de::LoopResult (InputDevice &)> func) const;

    /**
     * Returns @c true if the shift key of the keyboard is thought to be down.
     * @todo: Refactor away
     */
    bool shiftDown() const;

public: /// Event processing --------------------------------------------------
    /**
     * Clear the input event queue.
     */
    void clearEvents();

    bool ignoreEvents(bool yes = true);

    /**
     * @param ev  A copy is made.
     */
    void postEvent(ddevent_t *ev);

    /**
     * Process all incoming input for the given timestamp.
     * This is called only in the main thread, and also from the busy loop.
     *
     * This gets called at least 35 times per second. Usually more frequently
     * than that.
     */
    void processEvents(timespan_t ticLength);

    void processSharpEvents(timespan_t ticLength);

    /**
     * Update the input devices.
     */
    void trackEvent(ddevent_t *ev);

public:
    static bool convertEvent(ddevent_t const *ddEvent, event_t *ev);

    /**
     * Converts a libcore Event into an old-fashioned ddevent_t.
     *
     * @param event    Event instance.
     * @param ddEvent  ddevent_t instance.
     */
    static void convertEvent(de::Event const &event, ddevent_t *ddEvent);

public:
    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_INPUTSYSTEM_H
