/** @file infinesystem.h  Interactive animation sequence system.
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

#ifndef DE_UI_INFINESYSTEM_H
#define DE_UI_INFINESYSTEM_H

#include <de/list.h>
#include <de/error.h>
#include <de/string.h>
#include "finale.h"

/**
 * InFine script system.
 *
 * @ingroup infine
 */
class InFineSystem
{
public:
    /// The referenced Finale could not be found. @ingroup errors
    DE_ERROR(MissingFinaleError);

    typedef de::List<Finale *> Finales;

public:
    InFineSystem();

    void runTicks(timespan_t timeDelta);

    /**
     * Terminate and clear all running Finales.
     */
    void reset();

    /**
     * Returns @c true if one or more Finales are currently in progress. For the purpose of
     * this test, suspended scripts are interpreted as being in progress.
     */
    bool finaleInProgess() const;

    /**
     * Add a new Finale to the system.
     *
     * @param flags      @ref finaleFlags
     * @param script     InFine script to be interpreted.
     * @param setupCmds  InFine script for setting up the script environment on load.
     */
    Finale &newFinale(int flags, de::String script, const de::String &setupCmds = "");

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
    const Finales &finales() const;

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
    DE_PRIVATE(d)
};

#endif // DE_UI_INFINESYSTEM_H
