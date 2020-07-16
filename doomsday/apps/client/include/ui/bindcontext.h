/** @file bindcontext.h  Input system binding context.
 *
 * @authors Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_INPUTSYSTEM_BINDCONTEXT_H
#define CLIENT_INPUTSYSTEM_BINDCONTEXT_H

#include <functional>
#include <de/observers.h>
#include <de/record.h>
#include "ui/impulsebinding.h" // ibcontroltype_t

/// @todo: Move to public API
typedef int (*FallbackResponderFunc)(event_t *);
typedef int (*DDFallbackResponderFunc)(const ddevent_t *);
// todo ends

struct PlayerImpulse;

/**
 * Contextualized grouping of input (and windowing system) event bindings.
 *
 * @todo There should be one of these in every Widget that has bindable actions.
 * When that's done, many of the existing binding contexts become obsolete. There
 * should still be support for several alternative contexts within one widget,
 * for instance depending on the mode of the widget (e.g., automap pan).
 *
 * @todo: Conceptually the fallback responders don't belong; instead of "responding"
 * (immediately performing a reaction), we should be returning an Action instance. -jk
 *
 * @ingroup ui
 */
class BindContext
{
public:
    /// Notified when the active state of the context changes.
    DE_AUDIENCE(ActiveChange, void bindContextActiveChanged(BindContext &context))

    /// Notified when the list of devices to acquire changes.
    DE_AUDIENCE(AcquireDeviceChange, void bindContextAcquireDeviceChanged(BindContext &context))

    /// Notified whenever a new binding is made in this context.
    DE_AUDIENCE(BindingAddition, void bindContextBindingAdded(BindContext &context, de::Record &binding, bool isCommand))

public:
    /**
     * @param name  Symbolic name for the context.
     */
    explicit BindContext(const de::String &name);
    ~BindContext();

    /**
     * Returns @c true if the context is @em active, meaning, bindings in the context
     * are in effect and their associated action(s) will be executed if triggered.
     *
     * @see activate(), deactivate()
     */
    bool isActive() const;

    /**
     * Returns @c true if the context is @em protected, meaning, it should not be
     * manually (de)activated by the end user, directly.
     *
     * @see protect(), unprotect()
     */
    bool isProtected() const;

    /**
     * Change the @em protected state of the context.
     *
     * @param yes  @c true= protected.
     *
     * @see isProtected()
     */
    void protect(bool yes = true);
    inline void unprotect(bool yes = true) { protect(!yes); }

    /**
     * Returns the symbolic name of the context.
     */
    de::String name() const;
    void setName(const de::String &newName);

    /**
     * (De)activate the context, causing re-evaluation of the binding context stack.
     * The effective bindings for events may change as a result of calling this.
     *
     * @param yes  @c true= activate if inactive, and vice versa.
     */
    void activate(bool yes = true);
    inline void deactivate(bool yes = true) { activate(!yes); }

    void acquire(int deviceId, bool yes = true);
    void acquireAll(bool yes = true);

    bool willAcquire(int deviceId) const;
    bool willAcquireAll() const;

public: // Binding management: ------------------------------------------------------

    void clearAllBindings();

    void clearBindingsForDevice(int deviceId);

    /**
     * @param id  Unique identifier of the binding to delete.
     * @return  @c true if the binding was found and deleted.
     */
    bool deleteBinding(int id);

    // Commands ---------------------------------------------------------------------

    de::Record *bindCommand(const char *eventDesc, const char *command);

    /**
     * @param deviceId  (@c < 0 || >= NUM_INPUT_DEVICES) for wildcard search.
     */
    de::Record *findCommandBinding(const char *command, int deviceId = -1) const;

    /**
     * Iterate through all the CommandBindings of the context.
     */
    de::LoopResult forAllCommandBindings(const std::function<de::LoopResult (de::Record &)>& func) const;

    /**
     * Returns the total number of command bindings in the context.
     */
    int commandBindingCount() const;

    // Impulses ---------------------------------------------------------------------

    /**
     * @param ctrlDesc     Device-control descriptor.
     * @param impulse      Player impulse to bind to.
     * @param localPlayer  Local player number.
     *
     * @todo: Parse the the impulse descriptor here? -ds
     */
    de::Record *bindImpulse(const char *ctrlDesc, const PlayerImpulse &impulse,
                            int localPlayer);

    de::Record *findImpulseBinding(int deviceId, ibcontroltype_t bindType, int controlId) const;

    /**
     * Iterate through all the ImpulseBindings of the context.
     *
     * @param localPlayer  (@c < 0 || >= DDMAXPLAYERS) for all local players.
     */
    de::LoopResult forAllImpulseBindings(int localPlayer,
            const std::function<de::LoopResult (CompiledImpulseBindingRecord &)>& func) const;

    inline de::LoopResult forAllImpulseBindings(
            std::function<de::LoopResult (CompiledImpulseBindingRecord &)> func) const {
        return forAllImpulseBindings(-1/*all local players*/, func);
    }

    /**
     * Returns the total number of impulse bindings in the context.
     *
     * @param localPlayer  (@c < 0 || >= DDMAXPLAYERS) for all local players.
     */
    int impulseBindingCount(int localPlayer = -1) const;

public: // Triggering: --------------------------------------------------------------

    /**
     * @param event                  Event to be tried.
     * @param respectHigherContexts  Bindings shadowed by higher active contexts.
     *
     * @return  @c true if the event triggered an action or was eaten by a responder.
     */
    bool tryEvent(const ddevent_t &event, bool respectHigherContexts = true) const;

    void setFallbackResponder(FallbackResponderFunc newResponderFunc);
    void setDDFallbackResponder(DDFallbackResponderFunc newResponderFunc);

private:
    DE_PRIVATE(d)
};

#endif // CLIENT_INPUTSYSTEM_BINDCONTEXT_H
