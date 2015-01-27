/** @file system.h  Action Code Script (ACS) system.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef COMMON_ACS_SYSTEM_H
#define COMMON_ACS_SYSTEM_H

#include "common.h"
#ifdef __cplusplus
#  include <array>
#  include <functional>
#  include <de/Error>
#  include <de/Block>
#  include <de/Reader>
#  include <de/Writer>
#  include <doomsday/filesys/file.h>
#  include <doomsday/uri.h>
#  include "acs/interpreter.h"
#  include "acs/script.h"
#  include "mapstatereader.h"
#  include "mapstatewriter.h"

namespace acs {

/**
 * Action Code Script (ACS) system.
 *
 * @ingroup playsim
 */
class System
{
public:  /// @todo make private:
    std::array<int, 32> mapVars;
    std::array<int, 64> worldVars;

public:
    /// Required/referenced script is missing. @ingroup errors
    DENG2_ERROR(MissingScriptError);

    /// Required/referenced string-constant is missing. @ingroup errors
    DENG2_ERROR(MissingStringError);

public:
    System();

    /**
     * To be called when a new game session begins to reset the system. All
     * global scripting variables are discarded and deferred-tasks purged.
     */
    void reset();

    /**
     * Load new ACS bytecode from the specified @a file.
     */
    void loadBytecode(de::File1 &file);

    /**
     * Returns the total number of script entry points in the loaded bytecode.
     */
    int scriptCount() const;

    /**
     * Returns @c true iff @a scriptNumber is a known entry point.
     */
    bool hasScript(int scriptNumber);

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
    bool deferScriptStart(de::Uri const &mapUri, int scriptNumber, Script::Args const &args);

    /**
     * Provides readonly access to the loaded bytecode.
     */
    byte const *pcode() const;

    /**
     * Provides readonly access to a string constant from the loaded bytecode.
     */
    de::String stringConstant(int stringNumber) const;

    de::Block serializeWorldState() const;
    void readWorldState(de::Reader &from);

    void writeMapState(MapStateWriter *msw) const;
    void readMapState(MapStateReader *msr);

public:  /// @todo make private: -----------------------------------------------

    /**
     * To be called when the current map changes to activate any deferred scripts
     * which should now begin/resume.
     */
    void runDeferredTasks(de::Uri const &mapUri);

public:
    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

} // namespace acs

/**
 * Returns the game's global acs::System.
 */
acs::System &Game_ACScriptSystem();

#endif  // __cplusplus

// C wrapper API: --------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

dd_bool Game_ACScriptSystem_StartScript(int scriptNumber, Uri const *mapUri,
    byte const args[4], struct mobj_s *activator, Line *line, int side);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COMMON_ACS_SYSTEM_H
