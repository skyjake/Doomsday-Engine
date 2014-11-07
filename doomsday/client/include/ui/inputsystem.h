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
#include <de/Action>
#include <de/Error>
#include <de/System>
#include "dd_input.h" // ddevent_t
#include "ui/commandaction.h"
#include "SettingsRegister"

class BindContext;
class InputDevice;
struct CommandBinding;
struct ImpulseBinding;

#define DEFAULT_BINDING_CONTEXT_NAME    "game"
#define CONSOLE_BINDING_CONTEXT_NAME    "console"
#define UI_BINDING_CONTEXT_NAME         "deui"
#define GLOBAL_BINDING_CONTEXT_NAME     "global"

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
     * Returns the total number of InputDevices initialized.
     */
    int deviceCount() const;

    /**
     * (Re)initialize the input device models, returning all controls to their
     * default states.
     */
    void initAllDevices();

    /**
     * Returns @c true if the shift key of the keyboard is thought to be down.
     * @todo: Refactor away
     */
    bool shiftDown() const;

public: // Event processing --------------------------------------------------
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

    /**
     * Finds the action bound to a given event, iterating through all enabled
     * binding contexts.
     *
     * @param event  Event to match against.
     *
     * @return Action instance (caller gets ownership), or @c nullptr if not found.
     */
    de::Action *actionFor(ddevent_t const &event) const;

    /**
     * Checks if the event matches the binding's conditions, and if so, returns an
     * action with the bound command.
     *
     * @param bind     CommandBinding.
     * @param event    Event to match against.
     * @param context  The bound binding context. If the bound state is associated with a
                       higher-priority context, the binding cannot be executed.
     * @param respectHigherAssociatedContexts  Bindings are shadowed by higher active contexts.
     *
     * @return  Action to be triggered, or @c nullptr. Caller gets ownership.
     */
    de::Action *actionFor(CommandBinding const &bind, ddevent_t const &event,
        BindContext const *context, bool respectHigherAssociatedContexts);

public:

    static bool convertEvent(ddevent_t const *ddEvent, event_t *ev);

    /**
     * Converts a libcore Event into an old-fashioned ddevent_t.
     *
     * @param event    Event instance.
     * @param ddEvent  ddevent_t instance.
     */
    static void convertEvent(de::Event const &event, ddevent_t *ddEvent);

public: // Binding (context) management --------------------------------------

    /// Base class for binding configuration errors. @ingroup errors
    DENG2_ERROR(BindError);

    /// Required/referenced binding context is missing. @ingroup errors
    DENG2_ERROR(MissingContextError);

    /**
     * Try to make a new command binding.
     *
     * @param eventDesc  Textual descriptor for the event.
     * @param command    Console command(s) which the binding will execute when
     *                   triggered, if a binding is created.
     *
     * @return  Resultant command binding.
     */
    CommandBinding *bindCommand(char const *eventDesc, char const *command);

    /**
     * Try to make a new (player) impulse binding.
     *
     * @param ctrlDesc     Textual descriptor for the input device control event.
     * @param impulseDesc  Player impulse which the binding will execute when
     *                     triggered, if a binding is created.
     *
     * @return  Resultant impulse binding.
     */
    ImpulseBinding *bindImpulse(char const *ctrlDesc, char const *impulseDesc);

    /**
     * Try to remove the one unique binding associated with @a id.
     *
     * @return  @c true if that binding was removed.
     */
    bool removeBinding(int id);

    // ---

    /**
     * Enable the contexts for the initial state.
     */
    void initialContextActivations();

    /**
     * Destroy all binding contexts and the bindings within the contexts.
     */
    void clearAllContexts();

    /**
     * Returns the total number of binding contexts in the system.
     */
    int contextCount() const;

    /**
     * Returns @c true if the symbolic @a name references a known context.
     */
    bool hasContext(de::String const &name) const;

    /**
     * Lookup a binding context by symbolic @a name.
     */
    BindContext &context(de::String const &name) const;
    BindContext *contextPtr(de::String const &name) const;

    /**
     * Lookup a binding context by stack @a position.
     */
    BindContext &contextAt(int position) const;

    /**
     * Returns the stack position of the specified binding @a context.
     */
    int contextPositionOf(BindContext *context) const;

    /**
     * Creates a new binding context. The new context has the highest priority
     * of all existing contexts, and is inactive.
     */
    BindContext *newContext(de::String const &name);

    /**
     * Iterate through all the BindContexts from highest to lowest priority.
     */
    de::LoopResult forAllContexts(std::function<de::LoopResult (BindContext &)> func) const;

    /**
     * Write all bindings in all contexts to a text (cfg) file. Outputs console commands.
     */
    void writeAllBindingsTo(FILE *file);

    // ---

    /**
     * Parse an event => command trigger descriptor and configure the given @a binding.
     *
     * eventparams{+cond}*
     *
     * @param binding    Command binding to configure.
     * @param eventDesc  Descriptor for event information and any additional conditions.
     * @param command    Console command to execute when triggered, if any.
     * @param newId      @c true= assign a new unique identifier.
     */
    void configure(CommandBinding &binding, char const *eventDesc,
                   char const *command = nullptr, bool newId = false);

    /**
     * Parse a device-control => player impulse trigger descriptor and configure the given
     * @a binding.
     *
     * @param binding      Impulse binding to configure.
     * @param ctrlDesc     Descriptor for control information and any additional conditions.
     * @param impulseId    Identifier of the player impulse to execute when triggered, if any.
     * @param localPlayer  Local player number to execute the impulse for when triggered.
     * @param newId        @c true= assign a new unique identifier.
     */
    void configure(ImpulseBinding &binding, char const *ctrlDesc,
                   int impulseId, int localPlayer, bool newId = false);

    /**
     * Does the opposite of the B_Parse* methods for event descriptor, including the
     * state conditions.
     */
    de::String composeBindsFor(CommandBinding const &binding);

    /**
     * Does the opposite of the B_Parse* methods for a device binding, including the
     * state conditions.
     */
    de::String composeBindsFor(ImpulseBinding const &binding);

public:
    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_INPUTSYSTEM_H
