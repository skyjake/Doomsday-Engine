/** @file acscript.h  Hexen "ACS" scripting system.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHEXEN_PLAY_ACSCRIPT_H
#define LIBHEXEN_PLAY_ACSCRIPT_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "jhexen.h"
#include "p_mobj.h"
#ifdef __cplusplus
#  include "mapstatereader.h"
#  include "mapstatewriter.h"
#  include <de/ISerializable>
#  include <de/Reader>
#  include <de/Writer>
#  include <doomsday/filesys/file.h>
#  include <doomsday/uri.h>
#endif

#define MAX_ACS_SCRIPT_VARS     10
#define MAX_ACS_MAP_VARS        32
#define MAX_ACS_WORLD_VARS      64
#define ACS_STACK_DEPTH         32

struct BytecodeScriptInfo;
#ifdef __cplusplus
class ACScriptInterpreter;
#endif

/**
 * Action script thinker.
 */
typedef struct acscript_s {
    thinker_t thinker;
    mobj_t *activator;
    Line *line;
    int side;
    struct BytecodeScriptInfo *_info;
    int delayCount;
    struct Stack { // Local value stack.
        int values[ACS_STACK_DEPTH];
        int height;

#ifdef __cplusplus
        void push(int value);
        int pop();
        int top() const;
        void drop();
#endif
    } locals;
    int vars[MAX_ACS_SCRIPT_VARS];
    int const *pcodePtr;

#ifdef __cplusplus
    ACScriptInterpreter &interpreter() const;

    BytecodeScriptInfo &info() const;

    void runTick();

    /**
     * Serialize the thinker to the currently open save file.
     */
    void write(MapStateWriter *msw) const;

    /**
     * Deserialize the thinker from the currently open save file.
     */
    int read(MapStateReader *msr);
#endif // __cplusplus
} ACScript;

#ifdef __cplusplus
extern "C" {
#endif

void ACScript_Thinker(ACScript *script);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
/**
 * Action-Code Script (ACS) bytecode interpreter.
 */
class ACScriptInterpreter
{
public:
    int mapVars[MAX_ACS_MAP_VARS];
    int worldVars[MAX_ACS_WORLD_VARS];

    /// Logical script states:
    enum ScriptState {
        Inactive,
        Running,
        Suspended,
        WaitingForTag,
        WaitingForPolyobj,
        WaitingForScript,
        Terminating
    };

public:
    ACScriptInterpreter();

    /**
     * Load new ACS bytecode from the specified @a lump.
     */
    void loadBytecode(de::File1 &lump);

    /**
     * To be called when a new game session begins to reset the interpreter. The world state is
     * discarded and any deferred tasks are cleared.
     */
    void reset();

    /**
     * Returns the total number of script entrypoints in the loaded bytecode.
     */
    int scriptCount() const;

    /**
     * Start all scripts flagged to begin immediately "on open".
     */
    void startOpenScripts();

    /**
     * Start/resume the specified script.
     *
     * @param map  Map number on which the script is to be started.
     *             @c 0 = Current map. Otherwise 1-based index of the map to start on (deferred).
     *
     * @return  @c true iff a script was newly started (or deferred).
     */
    bool startScript(int scriptNumber, de::Uri const *mapUri, byte const args[4],
                     mobj_t *activator = 0, Line *line = 0, int side = 0);

    bool suspendScript(int scriptNumber, de::Uri const *mapUri);

    bool terminateScript(int scriptNumber, de::Uri const *mapUri);

    void tagFinished(int tag);
    void polyobjFinished(int tag);

    /**
     * Returns @c true iff @a scriptNumber is a known entrypoint.
     */
    bool hasScriptEntrypoint(int scriptNumber);

    /**
     * Lookup the info structure for the specified @a scriptNumber (entrypoint).
     */
    BytecodeScriptInfo &scriptInfo(int scriptNumber);

    inline BytecodeScriptInfo *scriptInfoPtr(int scriptNumber) {
        return hasScriptEntrypoint(scriptNumber)? &scriptInfo(scriptNumber) : 0;
    }

    /**
     * Provides readonly access to a string constant from the loaded bytecode.
     */
    Str const *string(int stringNumber) const;

    /**
     * Provides readonly access to the loaded bytecode.
     */
    byte const *bytecode() const;

    void clearDeferredTasks();

    /**
     * To be called when the current map changes to activate any deferred scripts
     * which should now begin/resume.
     */
    void runDeferredTasks(de::Uri const &mapUri);

    /**
     * To be called when the specified @a script is to be formally terminated.
     * All other scripts waiting on this are notified.
     *
     * @param script  The script to be formally terminated. Note that the script
     *                is considered free'd after calling this and pointers to it
     *                should be considered invalid.
     */
    void scriptFinished(ACScript *script);

    /**
     * Composes the human-friendly, textual name of the identified @a scriptNumber.
     */
    AutoStr *scriptName(int scriptNumber);

    /**
     * Composes a human-friendly, styled, textual description of the current status
     * of the identified @a scriptNumber.
     */
    AutoStr *scriptDescription(int scriptNumber);

    de::Block serializeWorldState() const;
    void readWorldState(de::Reader &from);

    void writeMapState(MapStateWriter *msw);
    void readMapState(MapStateReader *msr);

public: /// @todo make private:
    BytecodeScriptInfo &scriptInfoByIndex(int index);

private:
    /**
     * Returns the logical index of a @a scriptNumber; otherwise @c -1.
     */
    int scriptInfoIndex(int scriptNumber);

    ACScript *newACScript(BytecodeScriptInfo &info, byte const args[4], int delayCount = 0);

    /**
     * A deferred task is enqueued when a script is started on a map not currently loaded.
     */
    struct DeferredTask : public de::ISerializable
    {
        de::Uri mapUri;   ///< Target map.
        int scriptNumber; ///< On the target map.
        byte args[4];

        /**
         * @param mapUri        Unique identifier of the target map. A copy is made.
         * @param scriptNumber  Script number to execute on the target map.
         * @param args          Script arguments.
         */
        DeferredTask(de::Uri const &mapUri, int scriptNumber, byte const args[4]);

        static DeferredTask *newFromReader(de::Reader &from);

        void operator >> (de::Writer &to) const;
        void operator << (de::Reader &from);
    };

    bool newDeferredTask(de::Uri const &mapUri, int scriptNumber, byte const args[4]);

    byte const *_pcode; ///< Start of the loaded bytecode.

    int _scriptCount; ///< Number of script entrypoints.
    BytecodeScriptInfo *_scriptInfo;

    int _stringCount;
    Str *_strings;

    int _deferredTasksSize;
    DeferredTask **_deferredTasks;
};

/// @return  The game's global ACScript interpreter.
ACScriptInterpreter &Game_ACScriptInterpreter();

#endif

// C wrapper API, for legacy modules -------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

dd_bool Game_ACScriptInterpreter_StartScript(int scriptNumber, Uri const *mapUri,
    byte const args[4], mobj_t *activator, Line *line, int side);

dd_bool Game_ACScriptInterpreter_TerminateScript(int scriptNumber, Uri const *mapUri);

dd_bool Game_ACScriptInterpreter_SuspendScript(int scriptNumber, Uri const *mapUri);

void Game_ACScriptInterpreter_RunDeferredTasks(Uri const *mapUri);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHEXEN_PLAY_ACSCRIPT_H
