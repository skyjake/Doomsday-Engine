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
#include <de/reader.h>
#include <de/writer.h>

#define MAX_ACS_SCRIPT_VARS     10
#define MAX_ACS_MAP_VARS        32
#define MAX_ACS_WORLD_VARS      64
#define ACS_STACK_DEPTH         32

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
    int number;
    int infoIndex;
    int delayCount;
    int stack[ACS_STACK_DEPTH];
    int stackPtr;
    int vars[MAX_ACS_SCRIPT_VARS];
    int const *pcodePtr;

#ifdef __cplusplus
    ACScriptInterpreter &interpreter() const;

    void runTick();

    /**
     * Serialize the thinker to the currently open save file.
     */
    void write(Writer *writer) const;

    /**
     * Deserialize the thinker from the currently open save file.
     */
    int read(Reader *reader, int mapVersion);

public: /// @todo make private:
    void push(int value);
    int pop();
    int top() const;
    void drop();

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
struct BytecodeScriptInfo;

/**
 * Action-Code Script (ACS) bytecode interpreter.
 */
class ACScriptInterpreter
{
public:
    int mapVars[MAX_ACS_MAP_VARS];
    int worldVars[MAX_ACS_WORLD_VARS];

    ACScriptInterpreter();

    /**
     * Load new ACS bytecode from the specified @a lump.
     */
    void loadBytecode(lumpnum_t lump);

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
    bool startScript(int scriptNumber, uint map, byte *args, mobj_t *activator = 0,
        Line *line = 0, int side = 0);

    bool suspendScript(int scriptNumber, uint map);

    bool terminateScript(int scriptNumber, uint map);

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
    void runDeferredTasks(uint map/*Uri const *mapUri*/);

    /**
     * To be called when the specified @a script is to be formally terminated.
     * All other scripts waiting on this are notified.
     *
     * @param script  The script to be formally terminated. Note that the script
     *                is considered free'd after calling this and pointers to it
     *                should be considered invalid.
     */
    void scriptFinished(ACScript *script);

    void writeWorldScriptData(Writer *writer);
    void readWorldScriptData(Reader *reader, int saveVersion);

    void writeMapScriptData(Writer *writer);
    void readMapScriptData(Reader *reader);

public: /// @todo make private:
    BytecodeScriptInfo &scriptInfoByIndex(int index);

    BytecodeScriptInfo &scriptInfoFor(ACScript *script);

private:
    /**
     * Returns the logical index of a @a scriptNumber; otherwise @c -1.
     */
    int scriptInfoIndex(int scriptNumber);

    ACScript *newACScript(BytecodeScriptInfo &info, byte const *args, int delayCount = 0);

    /**
     * A deferred task is enqueued when a script is started on a map not currently loaded.
     */
    struct DeferredTask
    {
        uint map;         ///< Target map.
        int scriptNumber; ///< On the target map.
        byte args[4];
    };

    bool newDeferredTask(uint map, int scriptNumber, byte const *args);

    byte const *_pcode; ///< Start of the loaded bytecode.

    int _scriptCount; ///< Number of script entrypoints.
    BytecodeScriptInfo *_scriptInfo;

    int _stringCount;
    Str *_strings;

    int _deferredTasksSize;
    DeferredTask *_deferredTasks;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * To be called when a new game session begins to initialize ACS scripting.
 */
void P_InitACScript(void);

void P_LoadACScripts(lumpnum_t lump);

dd_bool P_StartACScript(int number, uint map, byte *args, mobj_t *activator, Line *line, int side);

dd_bool P_TerminateACScript(int number, uint map);

dd_bool P_SuspendACScript(int number, uint map);

void P_ACScriptTagFinished(int tag);

void P_ACScriptPolyobjFinished(int tag);

void P_ACScriptRunDeferredTasks(uint map/*Uri const *map*/);

void P_WriteGlobalACScriptData(Writer *writer);
void P_ReadGlobalACScriptData(Reader *reader, int saveVersion);

void P_WriteMapACScriptData(Writer *writer);
void P_ReadMapACScriptData(Reader *reader);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHEXEN_PLAY_ACSCRIPT_H
