/** @file system.h  Action Code Script (ACS) system.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBCOMMON_ACS_SYSTEM_H
#define LIBCOMMON_ACS_SYSTEM_H

#include "common.h"
#include <array>
#include <functional>
#include <de/error.h>
#include <de/block.h>
#include <de/reader.h>
#include <de/writer.h>
#include <doomsday/uri.h>
#include "acs/module.h"
#include "acs/script.h"
#include "mapstatereader.h"
#include "mapstatewriter.h"

namespace acs {

/**
 * Action Code Script (ACS) system.
 *
 * @ingroup playsim
 */
class System
{
public:  /// @todo make private:
    std::array<de::dint32, 32> mapVars;
    std::array<de::dint32, 64> worldVars;

public:
    /// Required/referenced script is missing. @ingroup errors
    DE_ERROR(MissingScriptError);

public:
    System();

    /**
     * To be called when a new game session begins to reset the system. All
     * global scripting variables are discarded and deferred-tasks purged.
     */
    void reset();

public:  // Modules: -----------------------------------------------------------

    /**
     * Discard the currently loaded ACS code module and attempt to load the module
     * associated with the given @a mapUri reference.
     */
    void loadModuleForMap(const res::Uri &mapUri);

    /**
     * Provides readonly access to the currently loaded bytecode module.
     */
    const Module &module() const;

public:  // Scripts: -----------------------------------------------------------

    /**
     * Returns the total number of script entry points in the loaded bytecode.
     */
    int scriptCount() const;

    /**
     * Returns @c true iff @a scriptNumber is a known entry point.
     */
    bool hasScript(int scriptNumber) const;

    /**
     * Lookup the Script info for the given @a scriptNumber.
     */
    Script &script(int scriptNumber) const;

    /**
     * Iterate through the Scripts of the loaded bytecode.
     *
     * @param func  Callback to make for each Script.
     */
    de::LoopResult forAllScripts(std::function<de::LoopResult (Script &)> func) const;

    /**
     * Defer a script start task until the identified map is next current.
     *
     * @param mapUri  Unique identifier of the map on which to start the script.
     *
     * @return  @c true iff a script was newly started (or deferred).
     */
    bool deferScriptStart(const res::Uri &mapUri, int scriptNumber, const Script::Args &args);

public:  // (De)serialization: -------------------------------------------------

    de::Block serializeWorldState() const;
    void readWorldState(de::Reader &from);

    void writeMapState(MapStateWriter *msw) const;
    void readMapState(MapStateReader *msr);

public:  /// @todo make private: -----------------------------------------------

    /**
     * To be called when the current map changes to activate any deferred scripts
     * which should now begin/resume.
     */
    void runDeferredTasks(const res::Uri &mapUri);

    /**
     * Start all scripts flagged to begin immediately (but allow a 1 second delay
     * for map initialization to complete).
     *
     * @todo Run deferred tasks at this time also?
     */
    void worldSystemMapChanged();

public:
    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

private:
    DE_PRIVATE(d)
};

}  // namespace acs

#endif  // LIBCOMMON_ACS_SYSTEM_H
