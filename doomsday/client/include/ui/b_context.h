/** @file b_context.h  Input system binding contexts.
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
#include "dd_share.h"
#include "b_command.h"
#include "b_device.h"

struct controlbinding_t
{
    controlbinding_t *next;
    controlbinding_t *prev;

    int bid;      ///< Unique identifier.
    int control;  ///< Identifier of the player control.
    dbinding_t deviceBinds[DDMAXPLAYERS];  ///< Separate bindings for each local player.
};

/// @todo: Move to public API
typedef int (*FallbackResponderFunc)(event_t *);
typedef int (*DDFallbackResponderFunc)(ddevent_t const *);

void B_DestroyControlBinding(controlbinding_t *conBin);

void B_InitControlBindingList(controlbinding_t *listRoot);

void B_DestroyControlBindingList(controlbinding_t *listRoot);

// ------------------------------------------------------------------------------

/**
 * Contextualized grouping of input system and windowing event bindings.
 *
 * @ingroup ui
 */
class BindContext
{
public:
    /**
     * @param name  Symbolic name for the context.
     */
    explicit BindContext(de::String const &name);
    ~BindContext();

    /**
     * Returns @c true if the context is @em active, meaning, bindings in the context are
     * in effect and their associated action(s) will be executed if triggered.
     *
     * @see activate(), deactivate()
     */
    bool isActive() const;

    /**
     * Returns @c true if the context is @em protected, meaning, it should not be manually
     * (de)activated directly, by the end user.
     *
     * @see protect()
     */
    bool isProtected() const;

    /**
     * Change the @em protected state of the context.
     *
     * @param yes  @c true= protected.
     * @see isProtected()
     */
    void protect(bool yes = true);

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

    void acquireAll(bool yex = true);
    void acquireKeyboard(bool yes = true);

    bool willAcquireAll() const;
    bool willAcquireKeyboard() const;

    void printAllBindings() const;
    void writeToFile(FILE *file) const;

public: // Binding management: ------------------------------------------------------

    void clearAllBindings();

    /**
     * @return  @c true if the binding was found and deleted.
     */
    bool deleteBinding(int bid);

    dbinding_t *findDeviceBinding(int device, cbdevtype_t bindType, int id);

    /**
     * Looks through context @a bc and looks for a binding that matches either
     * @a match1 or @a match2.
     */
    bool findMatchingBinding(cbinding_t *match1, dbinding_t *match2,
                             cbinding_t **evResult, dbinding_t **dResult);

    void deleteMatching(cbinding_t *eventBinding, dbinding_t *deviceBinding);

    // ---

    cbinding_t *bindCommand(char const *eventDesc, char const *command);

    /**
     * @param device  Use @c < 0 || >= NUM_INPUT_DEVICES for wildcard search.
     */
    cbinding_t *findCommandBinding(char const *command, int device) const;

    /**
     * Iterate through all the evbinding_ts of the context.
     */
    de::LoopResult forAllCommandBindings(std::function<de::LoopResult (cbinding_t &)> func) const;

    // ---

    controlbinding_t *findControlBinding(int control) const;

    controlbinding_t *getControlBinding(int control);

    /**
     * Iterate through all the evbinding_ts of the context.
     */
    de::LoopResult forAllControlBindings(std::function<de::LoopResult (controlbinding_t &)> func) const;

public: // Triggering: --------------------------------------------------------------

    /**
     * Finds the action bound to a given event.
     *
     * @param event                            Event to match against.
     * @param respectHigherAssociatedContexts  Bindings shadowed by higher active contexts.
     *
     * @return Action instance (caller gets ownership), or @c nullptr if not found.
     */
    de::Action *actionForEvent(ddevent_t const *event,
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
