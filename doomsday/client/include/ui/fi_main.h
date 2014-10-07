/** @file fi_main.h  Interactive animation sequence system.
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

#ifndef DENG_UI_INFINE_MAIN_H
#define DENG_UI_INFINE_MAIN_H

#include <QList>
#include <de/Error>
#include <de/Observers>
#include <de/String>
#include "dd_input.h" // ddevent_t
#include "api_infine.h" // finaleid_t

class FinaleInterpreter;

#define FINF_BEGIN          0x01
#define FINF_END            0x02
#define FINF_SCRIPT         0x04 // Script included.
#define FINF_SKIP           0x10

/**
 * A Finale instance contains the high-level state of an InFine script.
 *
 * @see FinaleInterpreter (interactive script interpreter)
 *
 * @ingroup InFine
 */
class Finale
{
public:
    /// Notified when the finale is about to be deleted.
    DENG2_DEFINE_AUDIENCE2(Deletion, void finaleBeingDeleted(Finale const &finale))

public:
    /**
     * @param flags   @ref finaleFlags
     * @param id      Unique identifier for the script.
     * @param script  The InFine script to be interpreted (a copy is made).
     */
    Finale(int flags, finaleid_t id, de::String const &script);

    int flags() const;
    finaleid_t id() const;

    bool isActive() const;

    void resume();
    void suspend();
    bool terminate();

    /**
     * @return @c false if the end of the script was reached.
     */
    bool runTicks();

    int handleEvent(ddevent_t const &ev);
    bool requestSkip();
    bool isMenuTrigger() const;

    /**
     * Provides access to the script interpreter. Mainly for debug purposes.
     */
    FinaleInterpreter const &interpreter() const;

private:
    DENG2_PRIVATE(d)
};

/**
 * InFine script system.
 *
 * @ingroup InFine
 */
class InFineSystem
{
public:
    /// The referenced Finale could not be found. @ingroup errors
    DENG2_ERROR(MissingFinaleError);

    typedef QList<Finale *> Finales;

public:
    InFineSystem();

    void runTicks(/*timespan_t delta*/);

    /**
     * Terminate and clear all running Finales.
     */
    void reset();

    /**
     * Add a new Finale to the system.
     *
     * @param flags      @ref finaleFlags
     * @param script     InFine script to be interpreted.
     * @param setupCmds  InFine script for setting up the script environment on load.
     */
    Finale &newFinale(int flags, de::String script, de::String const &setupCmds = "");

    /**
     * Returns @c true if @a id references a known Finale.
     */
    bool hasFinale(finaleid_t id) const;

    /**
     * Lookup a Finale by it's unique @a id.
     */
    Finale &finale(finaleid_t id);

    /**
     * Provides a list of all the Finales in the system, in order, for efficient traversal.
     */
    Finales const &finales() const;

public:
#ifdef __CLIENT__
    static void initBindingContext();
    static void deinitBindingContext();
#endif

    /**
     * Register the console commands and cvars of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_UI_INFINE_MAIN_H
