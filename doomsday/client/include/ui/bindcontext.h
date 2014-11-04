/** @file bindcontext.h  Input system binding context.
 *
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/Action>
#include <de/Observers>
#include "dd_share.h"
#include "b_command.h"
#include "b_device.h"

/// @todo: Move to public API
typedef int (*FallbackResponderFunc)(event_t *);
typedef int (*DDFallbackResponderFunc)(ddevent_t const *);
// todo ends

/// @todo Is this even necessary? -ds
struct controlbindgroup_t
{
    controlbindgroup_t *next;
    controlbindgroup_t *prev;

    int bid;        ///< Unique identifier.
    int impulseId;
    ImpulseBinding binds[DDMAXPLAYERS];  ///< Separate bindings for each local player.
};

void B_DestroyControlBindGroup(controlbindgroup_t *bg);

void B_InitControlBindGroupList(controlbindgroup_t *listRoot);

void B_DestroyControlBindGroupList(controlbindgroup_t *listRoot);

// ------------------------------------------------------------------------------

/**
 * Contextualized grouping of input (and windowing system) event bindings.
 *
 * @ingroup ui
 */
class BindContext
{
public:
    /// Notified when the active state of the context changes.
    DENG2_DEFINE_AUDIENCE2(ActiveChange, void bindContextActiveChanged(BindContext &context))

    /// Notified when the list of devices to acquire changes.
    DENG2_DEFINE_AUDIENCE2(AcquireDeviceChange, void bindContextAcquireDeviceChanged(BindContext &context))

    /// Notified whenever a new binding is made in this context.
    DENG2_DEFINE_AUDIENCE2(BindingAddition, void bindContextBindingAdded(BindContext &context, void *binding, bool isCommand))

public:
    /**
     * @param name  Symbolic name for the context.
     */
    explicit BindContext(de::String const &name);
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
    void setName(de::String const &newName);

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

    void printAllBindings() const;
    void writeAllBindingsTo(FILE *file) const;

public: // Binding management: ------------------------------------------------------

    void clearAllBindings();

    /**
     * @return  @c true if the binding was found and deleted.
     */
    bool deleteBinding(int bid);

    ImpulseBinding *findImpulseBinding(int device, ibcontroltype_t bindType, int id);

    /**
     * Looks through the context for a binding that matches either @a match1 or @a match2.
     */
    bool findMatchingBinding(CommandBinding *match1, ImpulseBinding *match2,
                             CommandBinding **evResult, ImpulseBinding **dResult);

    void deleteMatching(CommandBinding *commandBind, ImpulseBinding *impulseBind);

    // ---

    CommandBinding *bindCommand(char const *eventDesc, char const *command);

    /**
     * @param deviceId  Use @c < 0 || >= NUM_INPUT_DEVICES for wildcard search.
     */
    CommandBinding *findCommandBinding(char const *command, int deviceId = -1) const;

    /**
     * Iterate through all the CommandBindings of the context.
     */
    de::LoopResult forAllCommandBindings(std::function<de::LoopResult (CommandBinding &)> func) const;

    // ---

    controlbindgroup_t *findControlBindGroup(int control) const;

    controlbindgroup_t *getControlBindGroup(int control);

    /**
     * Iterate through all the controlbindgroup_ts of the context.
     */
    de::LoopResult forAllControlBindGroups(std::function<de::LoopResult (controlbindgroup_t &)> func) const;

public: // Triggering: --------------------------------------------------------------

    /**
     * Finds the action bound to a given event.
     *
     * @param event                            Event to match against.
     * @param respectHigherAssociatedContexts  Bindings shadowed by higher active contexts.
     *
     * @return Action instance (caller gets ownership), or @c nullptr if not found.
     */
    de::Action *actionForEvent(ddevent_t const &event,
                               bool respectHigherAssociatedContexts = true) const;

    /**
     * @todo Conceptually the fallback responders don't belong: instead of "responding"
     * (immediately performing a reaction), we should be returning an Action instance. -jk
     */
    int tryFallbackResponders(ddevent_t const &event, event_t &ev, bool validGameEvent);
    void setFallbackResponder(FallbackResponderFunc newResponderFunc);
    void setDDFallbackResponder(DDFallbackResponderFunc newResponderFunc);

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_INPUTSYSTEM_BINDCONTEXT_H
