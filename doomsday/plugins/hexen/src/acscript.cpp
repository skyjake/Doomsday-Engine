/** @file acscript.cpp  Hexen "ACS" scripting system.
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

#include "jhexen.h"
#include "acscript.h"

#include "dmu_lib.h"
#include "p_player.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include "p_sound.h"
#include "polyobjs.h"
#include <cstring>
#include <cstdio>

#define PRINT_BUFFER_SIZE 256

#define S_DROP activeScript->stackPtr--
#define S_POP activeScript->stack[--activeScript->stackPtr]
#define S_PUSH(x) activeScript->stack[activeScript->stackPtr++] = x

enum ACScriptState
{
    Inactive,
    Running,
    Suspended,
    WaitingForTag,
    WaitingForPolyobj,
    WaitingForScript,
    Terminating
};

static char const *scriptStateAsText(ACScriptState state)
{
    char const *names[] = {
        "Inactive",
        "Running",
        "Suspended",
        "Waiting for tag",
        "Waiting for polyobj",
        "Waiting for script",
        "Terminating"
    };
    return names[state];
}

/**
 * A deferred task is enqueued when a script is started on a map not currently loaded.
 */
struct DeferredTask
{
    uint map;     ///< Target map.
    int script;   ///< Script number on target map.
    byte args[4]; ///< Padded to 4 for alignment.
};
static int deferredTasksSize;
static DeferredTask *deferredTasks;

/**
 * Bytecode header. Read directly from the map lump.
 */
#pragma pack(1)
struct BytecodeHeader
{
    int32_t marker;
    int32_t infoOffset;
    int32_t code;
};
#pragma pack()

/**
 * Stores information about a script function entrypoint.
 */
struct BytecodeScriptInfo
{
    enum Flag {
        Open = 0x1
    };

    int flags;
    int number;
    int const *address;
    int argCount;

    // Current state:
    /// @todo Move to a separate array, in Interpreter
    ACScriptState state;
    int waitValue;
};

/**
 * Action-Code Script (ACS) bytecode interpreter.
 */
class Interpreter
{
public:
    Interpreter()
        : _pcode(0)
        , _scriptCount(0)
        , _scriptInfo(0)
        , _stringCount(0)
        , _strings(0)
    {}

    /**
     * Load new ACS bytecode from the specified @a lump.
     */
    void loadBytecode(lumpnum_t lump)
    {
#define OPEN_SCRIPTS_BASE 1000

        DENG_ASSERT(!IS_CLIENT);

        size_t const lumpLength = (lump >= 0? W_LumpLength(lump) : 0);

        App_Log(DE2_SCR_VERBOSE, "Loading ACS bytecode lump %s:%s (#%i)...",
                F_PrettyPath(Str_Text(W_LumpSourceFile(lump))),
                Str_Text(W_LumpName(lump)), lump);

        _scriptCount = 0;

        int const *buffer = 0;
        if(lumpLength >= sizeof(BytecodeHeader))
        {
            void *region = Z_Malloc(lumpLength, PU_MAP, 0);
            W_ReadLump(lump, (uint8_t *)region);
            BytecodeHeader const *header = (BytecodeHeader const *) region;
            _pcode = (byte const *) header;

            if(LONG(header->infoOffset) < (int)lumpLength)
            {
                buffer = (int *) ((byte const *) header + LONG(header->infoOffset));
                _scriptCount = LONG(*buffer++);
            }
        }

        if(!_scriptCount)
        {
            // Empty/Invalid lump.
            App_Log(DE2_SCR_WARNING, "Lump #%i does not appear to be valid ACS bytecode data", lump);
            return;
        }

        _scriptInfo = (BytecodeScriptInfo *) Z_Calloc(_scriptCount * sizeof(BytecodeScriptInfo), PU_MAP, 0);

        BytecodeScriptInfo *info = _scriptInfo;
        for(int i = 0; i < _scriptCount; ++i, info++)
        {
            info->number   = LONG(*buffer++);
            info->address  = (int const *)(_pcode + LONG(*buffer++));
            info->argCount = LONG(*buffer++);
            info->state    = Inactive;

            if(info->number >= OPEN_SCRIPTS_BASE)
            {
                info->flags |= BytecodeScriptInfo::Open;
                info->number -= OPEN_SCRIPTS_BASE;
            }
        }

        _stringCount = LONG(*buffer++);
        _strings = (char const **) Z_Malloc(_stringCount * sizeof(char *), PU_MAP, 0);
        for(int i = 0; i < _stringCount; ++i)
        {
            _strings[i] = (char const *)(_pcode + LONG(*buffer++));
        }

#undef OPEN_SCRIPTS_BASE
    }

    /**
     * Returns the total number of script entrypoints in the loaded bytecode.
     */
    int scriptCount() const
    {
        return _scriptCount;
    }

    /**
     * Start all scripts flagged to begin immediately "on open".
     */
    void startOpenScripts()
    {
        // Each is allotted 1 second for initialization.
        for(int i = 0; i < _scriptCount; ++i)
        {
            BytecodeScriptInfo &info = _scriptInfo[i];

            if(info.flags & BytecodeScriptInfo::Open)
            {
                newACScript(info, TICSPERSEC);
                info.state = Running;
            }
        }
    }

    /**
     * Start/resume the specified script.
     *
     * @return  Pointer to the @em newly-started script thinker; otherwise @c 0.
     */
    ACScript *startScript(int scriptNumber, byte *args, mobj_t *activator, Line *line, int side)
    {
        BytecodeScriptInfo &info = scriptInfo(scriptNumber);

        if(info.state == Suspended)
        {
            // Resume a suspended script.
            info.state = Running;
            return 0;
        }

        if(info.state != Inactive)
        {
            // Script is already executing.
            return 0;
        }

        ACScript *script = newACScript(info);

        script->activator = activator;
        script->line      = line;
        script->side      = side;
        for(int i = 0; i < info.argCount; ++i)
        {
            script->vars[i] = args[i];
        }
        info.state = Running;

        return script;
    }

    /**
     * Returns @c true iff @a scriptNumber is a known entrypoint.
     */
    bool hasScriptEntrypoint(int scriptNumber)
    {
        for(int i = 0; i < _scriptCount; ++i)
        {
            BytecodeScriptInfo &info = _scriptInfo[i];
            if(info.number == scriptNumber)
            {
                return true;
            }
        }
        return false;
    }

    /**
     * Lookup the info structure for the specified @a scriptNumber (entrypoint).
     */
    BytecodeScriptInfo &scriptInfo(int scriptNumber)
    {
        return _scriptInfo[scriptInfoIndex(scriptNumber)];
    }

    inline BytecodeScriptInfo *scriptInfoPtr(int scriptNumber) {
        return hasScriptEntrypoint(scriptNumber)? &scriptInfo(scriptNumber) : 0;
    }

    /**
     * Provides readonly access to a string constant from the loaded bytecode.
     */
    char const *string(int stringNumber)
    {
        DENG_ASSERT(stringNumber >= 0 && stringNumber < _stringCount);
        return _strings[stringNumber];
    }

    /**
     * Provides readonly access to the loaded bytecode.
     */
    byte const *bytecode() const
    {
        return _pcode;
    }

    /**
     * To be called when the specified @a script is to be formally terminated.
     * All other scripts waiting on this are notified.
     *
     * @param script  The script to be formally terminated. Note that the script
     *                is considered free'd after calling this and pointers to it
     *                should be considered invalid.
     */
    void scriptFinished(ACScript *script)
    {
        if(!script) return;

        // This script has now finished.
        scriptInfoByIndex(script->infoIndex).state = Inactive;

        // Notify any scripts which are waiting for this script to finish.
        for(int i = 0; i < _scriptCount; ++i)
        {
            BytecodeScriptInfo &info = _scriptInfo[i];
            if(info.state == WaitingForScript && info.waitValue == script->number)
            {
                info.state = Running;
            }
        }

        Thinker_Remove(&script->thinker);
    }

public: /// @todo make private:
    BytecodeScriptInfo &scriptInfoByIndex(int index)
    {
        DENG_ASSERT(index >= 0 && index < _scriptCount);
        return _scriptInfo[index];
    }

private:
    /**
     * Returns the logical index of a @a scriptNumber; otherwise @c -1.
     */
    int scriptInfoIndex(int scriptNumber)
    {
        for(int i = 0; i < _scriptCount; ++i)
        {
            BytecodeScriptInfo &info = _scriptInfo[i];
            if(info.number == scriptNumber)
            {
                return i;
            }
        }
        return -1;
    }

    ACScript *newACScript(BytecodeScriptInfo &info, int delayCount = 0)
    {
        ACScript *script = (ACScript *) Z_Calloc(sizeof(*script), PU_MAP, 0);
        script->thinker.function = (thinkfunc_t) ACScript_Thinker;

        script->number     = info.number;
        script->infoIndex  = scriptInfoIndex(info.number);
        script->ip         = info.address;
        script->delayCount = delayCount;

        Thinker_Add(&script->thinker);

        return script;
    }

    byte const *_pcode; ///< Start of the loaded bytecode.

    int _scriptCount; ///< Number of script entrypoints.
    BytecodeScriptInfo *_scriptInfo;

    int _stringCount;
    char const **_strings;
};

/// The One ACS interpreter instance.
static Interpreter interp;

static bool tagBusy(int tag);
static bool newDeferredTask(uint map, int number, byte const *args);

static void push(int value);
static int pop();
static int top();
static void drop();

enum CommandResult
{
    Continue,
    Stop,
    Terminate
};

/// Helper macro for declaring an ACS command function.
#define ACS_COMMAND(Name) CommandResult cmd##Name()

static ACS_COMMAND(NOP);
static ACS_COMMAND(Terminate);
static ACS_COMMAND(Suspend);
static ACS_COMMAND(PushNumber);
static ACS_COMMAND(LSpec1);
static ACS_COMMAND(LSpec2);
static ACS_COMMAND(LSpec3);
static ACS_COMMAND(LSpec4);
static ACS_COMMAND(LSpec5);
static ACS_COMMAND(LSpec1Direct);
static ACS_COMMAND(LSpec2Direct);
static ACS_COMMAND(LSpec3Direct);
static ACS_COMMAND(LSpec4Direct);
static ACS_COMMAND(LSpec5Direct);
static ACS_COMMAND(Add);
static ACS_COMMAND(Subtract);
static ACS_COMMAND(Multiply);
static ACS_COMMAND(Divide);
static ACS_COMMAND(Modulus);
static ACS_COMMAND(EQ);
static ACS_COMMAND(NE);
static ACS_COMMAND(LT);
static ACS_COMMAND(GT);
static ACS_COMMAND(LE);
static ACS_COMMAND(GE);
static ACS_COMMAND(AssignScriptVar);
static ACS_COMMAND(AssignMapVar);
static ACS_COMMAND(AssignWorldVar);
static ACS_COMMAND(PushScriptVar);
static ACS_COMMAND(PushMapVar);
static ACS_COMMAND(PushWorldVar);
static ACS_COMMAND(AddScriptVar);
static ACS_COMMAND(AddMapVar);
static ACS_COMMAND(AddWorldVar);
static ACS_COMMAND(SubScriptVar);
static ACS_COMMAND(SubMapVar);
static ACS_COMMAND(SubWorldVar);
static ACS_COMMAND(MulScriptVar);
static ACS_COMMAND(MulMapVar);
static ACS_COMMAND(MulWorldVar);
static ACS_COMMAND(DivScriptVar);
static ACS_COMMAND(DivMapVar);
static ACS_COMMAND(DivWorldVar);
static ACS_COMMAND(ModScriptVar);
static ACS_COMMAND(ModMapVar);
static ACS_COMMAND(ModWorldVar);
static ACS_COMMAND(IncScriptVar);
static ACS_COMMAND(IncMapVar);
static ACS_COMMAND(IncWorldVar);
static ACS_COMMAND(DecScriptVar);
static ACS_COMMAND(DecMapVar);
static ACS_COMMAND(DecWorldVar);
static ACS_COMMAND(Goto);
static ACS_COMMAND(IfGoto);
static ACS_COMMAND(Drop);
static ACS_COMMAND(Delay);
static ACS_COMMAND(DelayDirect);
static ACS_COMMAND(Random);
static ACS_COMMAND(RandomDirect);
static ACS_COMMAND(ThingCount);
static ACS_COMMAND(ThingCountDirect);
static ACS_COMMAND(TagWait);
static ACS_COMMAND(TagWaitDirect);
static ACS_COMMAND(PolyWait);
static ACS_COMMAND(PolyWaitDirect);
static ACS_COMMAND(ChangeFloor);
static ACS_COMMAND(ChangeFloorDirect);
static ACS_COMMAND(ChangeCeiling);
static ACS_COMMAND(ChangeCeilingDirect);
static ACS_COMMAND(Restart);
static ACS_COMMAND(AndLogical);
static ACS_COMMAND(OrLogical);
static ACS_COMMAND(AndBitwise);
static ACS_COMMAND(OrBitwise);
static ACS_COMMAND(EorBitwise);
static ACS_COMMAND(NegateLogical);
static ACS_COMMAND(LShift);
static ACS_COMMAND(RShift);
static ACS_COMMAND(UnaryMinus);
static ACS_COMMAND(IfNotGoto);
static ACS_COMMAND(LineSide);
static ACS_COMMAND(ScriptWait);
static ACS_COMMAND(ScriptWaitDirect);
static ACS_COMMAND(ClearLineSpecial);
static ACS_COMMAND(CaseGoto);
static ACS_COMMAND(BeginPrint);
static ACS_COMMAND(EndPrint);
static ACS_COMMAND(PrintString);
static ACS_COMMAND(PrintNumber);
static ACS_COMMAND(PrintCharacter);
static ACS_COMMAND(PlayerCount);
static ACS_COMMAND(GameType);
static ACS_COMMAND(GameSkill);
static ACS_COMMAND(Timer);
static ACS_COMMAND(SectorSound);
static ACS_COMMAND(AmbientSound);
static ACS_COMMAND(SoundSequence);
static ACS_COMMAND(SetLineTexture);
static ACS_COMMAND(SetLineBlocking);
static ACS_COMMAND(SetLineSpecial);
static ACS_COMMAND(ThingSound);
static ACS_COMMAND(EndPrintBold);

static void thingCount(int type, int tid);

static int MapVars[MAX_ACS_MAP_VARS];
static int WorldVars[MAX_ACS_WORLD_VARS];

static int const *PCodePtr;
static byte SpecArgs[8];

static char PrintBuffer[PRINT_BUFFER_SIZE];

static ACScript *activeScript;
static ACScript *newScript;

static char ErrorMsg[128];

static CommandResult (*PCodeCmds[]) () =
{
    cmdNOP, cmdTerminate, cmdSuspend, cmdPushNumber, cmdLSpec1, cmdLSpec2,
    cmdLSpec3, cmdLSpec4, cmdLSpec5, cmdLSpec1Direct, cmdLSpec2Direct,
    cmdLSpec3Direct, cmdLSpec4Direct, cmdLSpec5Direct, cmdAdd,
    cmdSubtract, cmdMultiply, cmdDivide, cmdModulus, cmdEQ, cmdNE,
    cmdLT, cmdGT, cmdLE, cmdGE, cmdAssignScriptVar, cmdAssignMapVar,
    cmdAssignWorldVar, cmdPushScriptVar, cmdPushMapVar,
    cmdPushWorldVar, cmdAddScriptVar, cmdAddMapVar, cmdAddWorldVar,
    cmdSubScriptVar, cmdSubMapVar, cmdSubWorldVar, cmdMulScriptVar,
    cmdMulMapVar, cmdMulWorldVar, cmdDivScriptVar, cmdDivMapVar,
    cmdDivWorldVar, cmdModScriptVar, cmdModMapVar, cmdModWorldVar,
    cmdIncScriptVar, cmdIncMapVar, cmdIncWorldVar, cmdDecScriptVar,
    cmdDecMapVar, cmdDecWorldVar, cmdGoto, cmdIfGoto, cmdDrop,
    cmdDelay, cmdDelayDirect, cmdRandom, cmdRandomDirect,
    cmdThingCount, cmdThingCountDirect, cmdTagWait, cmdTagWaitDirect,
    cmdPolyWait, cmdPolyWaitDirect, cmdChangeFloor,
    cmdChangeFloorDirect, cmdChangeCeiling, cmdChangeCeilingDirect,
    cmdRestart, cmdAndLogical, cmdOrLogical, cmdAndBitwise,
    cmdOrBitwise, cmdEorBitwise, cmdNegateLogical, cmdLShift,
    cmdRShift, cmdUnaryMinus, cmdIfNotGoto, cmdLineSide, cmdScriptWait,
    cmdScriptWaitDirect, cmdClearLineSpecial, cmdCaseGoto,
    cmdBeginPrint, cmdEndPrint, cmdPrintString, cmdPrintNumber,
    cmdPrintCharacter, cmdPlayerCount, cmdGameType, cmdGameSkill,
    cmdTimer, cmdSectorSound, cmdAmbientSound, cmdSoundSequence,
    cmdSetLineTexture, cmdSetLineBlocking, cmdSetLineSpecial,
    cmdThingSound, cmdEndPrintBold
};

void P_LoadACScripts(lumpnum_t lump)
{
    if(IS_CLIENT) return;

    interp.loadBytecode(lump);

    memset(MapVars, 0, sizeof(MapVars));

    // Start all scripts flagged to begin immediately.
    interp.startOpenScripts();
}

void P_ACScriptRunDeferredTasks(uint map/*Uri const *mapUri*/)
{
    //DENG_ASSERT(mapUri != 0);

    if(deathmatch)
    {
        /// @todo Do we really want to disallow deferred ACS tasks in deathmatch?
        /// What is the actual intention here? -ds
        return;
    }

    int const origSize = deferredTasksSize;

    int i = 0;
    while(i < deferredTasksSize)
    {
        DeferredTask *task = &deferredTasks[i];

        if(task->map != map)
        {
            i++;
            continue;
        }

        P_StartACScript(task->script, 0, task->args, NULL, NULL, 0);
        if(newScript)
        {
            newScript->delayCount = TICRATE;
        }

        deferredTasksSize -= 1;
        if(i == deferredTasksSize)
            break;

        memmove(&deferredTasks[i], &deferredTasks[i + 1], sizeof(DeferredTask) * (deferredTasksSize - i));
    }

    if(deferredTasksSize == origSize)
        return;

    if(deferredTasksSize)
    {
        deferredTasks = (DeferredTask *) Z_Realloc(deferredTasks, sizeof(DeferredTask) * deferredTasksSize, PU_GAMESTATIC);
        return;
    }

    Z_Free(deferredTasks);
    deferredTasks = 0;
}

dd_bool P_StartACScript(int scriptNumber, uint map, byte *args, mobj_t *activator,
    Line *line, int side)
{
    DENG_ASSERT(!IS_CLIENT);

    newScript = 0;

    if(map && map - 1 != gameMap)
    {
        // Script is not for the current map.
        // Add it to the store to be started when that map is next entered.
        return newDeferredTask(map - 1, scriptNumber, args);
    }

    if(!interp.hasScriptEntrypoint(scriptNumber))
    {
        // Script not found.
        sprintf(ErrorMsg, "P_STARTACS ERROR: UNKNOWN SCRIPT %d", scriptNumber);
        P_SetMessage(&players[CONSOLEPLAYER], LMF_NO_HIDE, ErrorMsg);
        return false;
    }

    newScript = interp.startScript(scriptNumber, args, activator, line, side);

    return true;
}

static bool newDeferredTask(uint map, int scriptNumber, byte const *args)
{
    if(deferredTasksSize)
    {
        // Don't allow duplicates.
        for(int i = 0; i < deferredTasksSize; ++i)
        {
            DeferredTask &task = deferredTasks[i];
            if(task.script == scriptNumber && task.map == map)
            {
                return false;
            }
        }

        deferredTasks = (DeferredTask *) Z_Realloc(deferredTasks, ++deferredTasksSize * sizeof(DeferredTask), PU_GAMESTATIC);
    }
    else
    {
        deferredTasks = (DeferredTask *) Z_Malloc(sizeof(DeferredTask), PU_GAMESTATIC, 0);
        deferredTasksSize = 1;
    }

    DeferredTask *store = &deferredTasks[deferredTasksSize-1];

    store->map     = map;
    store->script  = scriptNumber;
    store->args[0] = args[0];
    store->args[1] = args[1];
    store->args[2] = args[2];
    store->args[3] = args[3];

    return true;
}

dd_bool P_TerminateACScript(int scriptNumber, uint /*map*/)
{
    if(BytecodeScriptInfo *info = interp.scriptInfoPtr(scriptNumber))
    {
        // Some states disallow termination.
        if(info->state != Inactive && info->state != Terminating)
        {
            info->state = Terminating;
            return true;
        }
    }
    return false;
}

dd_bool P_SuspendACScript(int scriptNumber, uint /*map*/)
{
    if(BytecodeScriptInfo *info = interp.scriptInfoPtr(scriptNumber))
    {
        // Some states disallow suspension.
        if(info->state != Inactive && info->state != Suspended &&
           info->state != Terminating)
        {
            info->state = Suspended;
            return true;
        }
    }
    return false;
}

void P_InitACScript()
{
    memset(WorldVars, 0, sizeof(WorldVars));
    Z_Free(deferredTasks); deferredTasks = 0;
    deferredTasksSize = 0;
}

void P_ACScriptTagFinished(int tag)
{
    if(tagBusy(tag)) return;

    for(int i = 0; i < interp.scriptCount(); ++i)
    {
        BytecodeScriptInfo &info = interp.scriptInfoByIndex(i);
        if(info.state == WaitingForTag && info.waitValue == tag)
        {
            info.state = Running;
        }
    }
}

void P_ACScriptPolyobjFinished(int tag)
{
    if(PO_Busy(tag)) return;

    for(int i = 0; i < interp.scriptCount(); ++i)
    {
        BytecodeScriptInfo &info = interp.scriptInfoByIndex(i);
        if(info.state == WaitingForPolyobj && info.waitValue == tag)
        {
            info.state = Running;
        }
    }
}

static bool tagBusy(int tag)
{
    // NOTE: We can't use the sector tag lists here as we might already be
    // in an iteration at a higher level.
    for(int i = 0; i < numsectors; ++i)
    {
        Sector *sec     = (Sector *) P_ToPtr(DMU_SECTOR, i);
        xsector_t *xsec = P_ToXSector(sec);

        if(xsec->tag != tag) continue;

        if(xsec->specialData)
        {
            return true;
        }
    }
    return false;
}

static void push(int value)
{
    activeScript->stack[activeScript->stackPtr++] = value;
}

static int pop()
{
    return activeScript->stack[--activeScript->stackPtr];
}

static int top()
{
    return activeScript->stack[activeScript->stackPtr - 1];
}

static void drop()
{
    activeScript->stackPtr--;
}

static ACS_COMMAND(NOP)
{
    return Continue;
}

static ACS_COMMAND(Terminate)
{
    return Terminate;
}

static ACS_COMMAND(Suspend)
{
    interp.scriptInfoByIndex(activeScript->infoIndex).state = Suspended;
    return Stop;
}

static ACS_COMMAND(PushNumber)
{
    push(LONG(*PCodePtr++));
    return Continue;
}

static ACS_COMMAND(LSpec1)
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return Continue;
}

static ACS_COMMAND(LSpec2)
{
    int special = LONG(*PCodePtr++);
    SpecArgs[1] = pop();
    SpecArgs[0] = pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return Continue;
}

static ACS_COMMAND(LSpec3)
{
    int special = LONG(*PCodePtr++);
    SpecArgs[2] = pop();
    SpecArgs[1] = pop();
    SpecArgs[0] = pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return Continue;
}

static ACS_COMMAND(LSpec4)
{
    int special = LONG(*PCodePtr++);
    SpecArgs[3] = pop();
    SpecArgs[2] = pop();
    SpecArgs[1] = pop();
    SpecArgs[0] = pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return Continue;
}

static ACS_COMMAND(LSpec5)
{
    int special = LONG(*PCodePtr++);
    SpecArgs[4] = pop();
    SpecArgs[3] = pop();
    SpecArgs[2] = pop();
    SpecArgs[1] = pop();
    SpecArgs[0] = pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return Continue;
}

static ACS_COMMAND(LSpec1Direct)
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return Continue;
}

static ACS_COMMAND(LSpec2Direct)
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return Continue;
}

static ACS_COMMAND(LSpec3Direct)
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    SpecArgs[2] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return Continue;
}

static ACS_COMMAND(LSpec4Direct)
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    SpecArgs[2] = LONG(*PCodePtr++);
    SpecArgs[3] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return Continue;
}

static ACS_COMMAND(LSpec5Direct)
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    SpecArgs[2] = LONG(*PCodePtr++);
    SpecArgs[3] = LONG(*PCodePtr++);
    SpecArgs[4] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return Continue;
}

static ACS_COMMAND(Add)
{
    push(pop() + pop());
    return Continue;
}

static ACS_COMMAND(Subtract)
{
    int operand2 = pop();
    push(pop() - operand2);
    return Continue;
}

static ACS_COMMAND(Multiply)
{
    push(pop() * pop());
    return Continue;
}

static ACS_COMMAND(Divide)
{
    int operand2 = pop();
    push(pop() / operand2);
    return Continue;
}

static ACS_COMMAND(Modulus)
{
    int operand2 = pop();
    push(pop() % operand2);
    return Continue;
}

static ACS_COMMAND(EQ)
{
    push(pop() == pop());
    return Continue;
}

static ACS_COMMAND(NE)
{
    push(pop() != pop());
    return Continue;
}

static ACS_COMMAND(LT)
{
    int operand2 = pop();
    push(pop() < operand2);
    return Continue;
}

static ACS_COMMAND(GT)
{
    int operand2 = pop();
    push(pop() > operand2);
    return Continue;
}

static ACS_COMMAND(LE)
{
    int operand2 = pop();
    push(pop() <= operand2);
    return Continue;
}

static ACS_COMMAND(GE)
{
    int operand2 = pop();
    push(pop() >= operand2);
    return Continue;
}

static ACS_COMMAND(AssignScriptVar)
{
    activeScript->vars[LONG(*PCodePtr++)] = pop();
    return Continue;
}

static ACS_COMMAND(AssignMapVar)
{
    MapVars[LONG(*PCodePtr++)] = pop();
    return Continue;
}

static ACS_COMMAND(AssignWorldVar)
{
    WorldVars[LONG(*PCodePtr++)] = pop();
    return Continue;
}

static ACS_COMMAND(PushScriptVar)
{
    push(activeScript->vars[LONG(*PCodePtr++)]);
    return Continue;
}

static ACS_COMMAND(PushMapVar)
{
    push(MapVars[LONG(*PCodePtr++)]);
    return Continue;
}

static ACS_COMMAND(PushWorldVar)
{
    push(WorldVars[LONG(*PCodePtr++)]);
    return Continue;
}

static ACS_COMMAND(AddScriptVar)
{
    activeScript->vars[LONG(*PCodePtr++)] += pop();
    return Continue;
}

static ACS_COMMAND(AddMapVar)
{
    MapVars[LONG(*PCodePtr++)] += pop();
    return Continue;
}

static ACS_COMMAND(AddWorldVar)
{
    WorldVars[LONG(*PCodePtr++)] += pop();
    return Continue;
}

static ACS_COMMAND(SubScriptVar)
{
    activeScript->vars[LONG(*PCodePtr++)] -= pop();
    return Continue;
}

static ACS_COMMAND(SubMapVar)
{
    MapVars[LONG(*PCodePtr++)] -= pop();
    return Continue;
}

static ACS_COMMAND(SubWorldVar)
{
    WorldVars[LONG(*PCodePtr++)] -= pop();
    return Continue;
}

static ACS_COMMAND(MulScriptVar)
{
    activeScript->vars[LONG(*PCodePtr++)] *= pop();
    return Continue;
}

static ACS_COMMAND(MulMapVar)
{
    MapVars[LONG(*PCodePtr++)] *= pop();
    return Continue;
}

static ACS_COMMAND(MulWorldVar)
{
    WorldVars[LONG(*PCodePtr++)] *= pop();
    return Continue;
}

static ACS_COMMAND(DivScriptVar)
{
    activeScript->vars[LONG(*PCodePtr++)] /= pop();
    return Continue;
}

static ACS_COMMAND(DivMapVar)
{
    MapVars[LONG(*PCodePtr++)] /= pop();
    return Continue;
}

static ACS_COMMAND(DivWorldVar)
{
    WorldVars[LONG(*PCodePtr++)] /= pop();
    return Continue;
}

static ACS_COMMAND(ModScriptVar)
{
    activeScript->vars[LONG(*PCodePtr++)] %= pop();
    return Continue;
}

static ACS_COMMAND(ModMapVar)
{
    MapVars[LONG(*PCodePtr++)] %= pop();
    return Continue;
}

static ACS_COMMAND(ModWorldVar)
{
    WorldVars[LONG(*PCodePtr++)] %= pop();
    return Continue;
}

static ACS_COMMAND(IncScriptVar)
{
    activeScript->vars[LONG(*PCodePtr++)]++;
    return Continue;
}

static ACS_COMMAND(IncMapVar)
{
    MapVars[LONG(*PCodePtr++)]++;
    return Continue;
}

static ACS_COMMAND(IncWorldVar)
{
    WorldVars[LONG(*PCodePtr++)]++;
    return Continue;
}

static ACS_COMMAND(DecScriptVar)
{
    activeScript->vars[LONG(*PCodePtr++)]--;
    return Continue;
}

static ACS_COMMAND(DecMapVar)
{
    MapVars[LONG(*PCodePtr++)]--;
    return Continue;
}

static ACS_COMMAND(DecWorldVar)
{
    WorldVars[LONG(*PCodePtr++)]--;
    return Continue;
}

static ACS_COMMAND(Goto)
{
    PCodePtr = (int *) (interp.bytecode() + LONG(*PCodePtr));
    return Continue;
}

static ACS_COMMAND(IfGoto)
{
    if(pop())
    {
        PCodePtr = (int *) (interp.bytecode() + LONG(*PCodePtr));
    }
    else
    {
        PCodePtr++;
    }
    return Continue;
}

static ACS_COMMAND(Drop)
{
    drop();
    return Continue;
}

static ACS_COMMAND(Delay)
{
    activeScript->delayCount = pop();
    return Stop;
}

static ACS_COMMAND(DelayDirect)
{
    activeScript->delayCount = LONG(*PCodePtr++);
    return Stop;
}

static ACS_COMMAND(Random)
{
    int high = pop();
    int low  = pop();
    push(low + (P_Random() % (high - low + 1)));
    return Continue;
}

static ACS_COMMAND(RandomDirect)
{
    int low  = LONG(*PCodePtr++);
    int high = LONG(*PCodePtr++);
    push(low + (P_Random() % (high - low + 1)));
    return Continue;
}

static ACS_COMMAND(ThingCount)
{
    int tid = pop();
    thingCount(pop(), tid);
    return Continue;
}

static ACS_COMMAND(ThingCountDirect)
{
    int type = LONG(*PCodePtr++);
    thingCount(type, LONG(*PCodePtr++));
    return Continue;
}

struct countmobjoftypeparams_t
{
    mobjtype_t type;
    int count;
};

static int countMobjOfType(thinker_t *th, void *context)
{
    countmobjoftypeparams_t *params = (countmobjoftypeparams_t *) context;
    mobj_t *mo = (mobj_t *) th;

    // Does the type match?
    if(mo->type != params->type)
        return false; // Continue iteration.

    // Minimum health requirement?
    if((mo->flags & MF_COUNTKILL) && mo->health <= 0)
        return false; // Continue iteration.

    params->count++;

    return false; // Continue iteration.
}

static void thingCount(int type, int tid)
{
    // Nothing to count?
    if(!(type + tid)) return;

    mobjtype_t moType = TranslateThingType[type];
    int count = 0;

    if(tid)
    {
        // Count TID things.
        mobj_t *mo;
        int searcher = -1;

        while((mo = P_FindMobjFromTID(tid, &searcher)) != NULL)
        {
            if(type == 0)
            {
                // Just count TIDs.
                count++;
            }
            else if(moType == mo->type)
            {
                // Don't count dead monsters.
                if((mo->flags & MF_COUNTKILL) && mo->health <= 0)
                {
                    continue;
                }
                count++;
            }
        }
    }
    else
    {
        // Count only types.
        countmobjoftypeparams_t params;

        params.type = moType;
        params.count = 0;
        Thinker_Iterate(P_MobjThinker, countMobjOfType, &params);

        count = params.count;
    }

    push(count);
}

static ACS_COMMAND(TagWait)
{
    interp.scriptInfoByIndex(activeScript->infoIndex).waitValue = pop();
    interp.scriptInfoByIndex(activeScript->infoIndex).state = WaitingForTag;
    return Stop;
}

static ACS_COMMAND(TagWaitDirect)
{
    interp.scriptInfoByIndex(activeScript->infoIndex).waitValue = LONG(*PCodePtr++);
    interp.scriptInfoByIndex(activeScript->infoIndex).state = WaitingForTag;
    return Stop;
}

static ACS_COMMAND(PolyWait)
{
    interp.scriptInfoByIndex(activeScript->infoIndex).waitValue = pop();
    interp.scriptInfoByIndex(activeScript->infoIndex).state = WaitingForPolyobj;
    return Stop;
}

static ACS_COMMAND(PolyWaitDirect)
{
    interp.scriptInfoByIndex(activeScript->infoIndex).waitValue = LONG(*PCodePtr++);
    interp.scriptInfoByIndex(activeScript->infoIndex).state = WaitingForPolyobj;
    return Stop;
}

static ACS_COMMAND(ChangeFloor)
{
    ddstring_t path; Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, interp.string(pop())));
    Uri *uri = Uri_NewWithPath2("Flats:", RC_NULL);
    Uri_SetPath(uri, Str_Text(&path));
    Str_Free(&path);

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    int tag = pop();

    if(iterlist_t *list = P_GetSectorIterListForTag(tag, false))
    {
        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);

        Sector *sec;
        while((sec = (Sector *) IterList_MoveIterator(list)))
        {
            P_SetPtrp(sec, DMU_FLOOR_MATERIAL, mat);
        }
    }

    return Continue;
}

static ACS_COMMAND(ChangeFloorDirect)
{
    int tag = LONG(*PCodePtr++);

    ddstring_t path; Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, interp.string(LONG(*PCodePtr++))));

    Uri *uri = Uri_NewWithPath2("Flats:", RC_NULL);
    Uri_SetPath(uri, Str_Text(&path));
    Str_Free(&path);

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    if(iterlist_t *list = P_GetSectorIterListForTag(tag, false))
    {
        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);

        Sector *sec;
        while((sec = (Sector *) IterList_MoveIterator(list)))
        {
            P_SetPtrp(sec, DMU_FLOOR_MATERIAL, mat);
        }
    }

    return Continue;
}

static ACS_COMMAND(ChangeCeiling)
{
    ddstring_t path; Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, interp.string(pop())));

    Uri *uri = Uri_NewWithPath2("Flats:", RC_NULL);
    Uri_SetPath(uri, Str_Text(&path));
    Str_Free(&path);

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    int tag = pop();

    if(iterlist_t *list = P_GetSectorIterListForTag(tag, false))
    {
        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);

        Sector *sec;
        while((sec = (Sector *) IterList_MoveIterator(list)))
        {
            P_SetPtrp(sec, DMU_CEILING_MATERIAL, mat);
        }
    }

    return Continue;
}

static ACS_COMMAND(ChangeCeilingDirect)
{
    int tag = LONG(*PCodePtr++);

    ddstring_t path; Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, interp.string(LONG(*PCodePtr++))));

    Uri *uri = Uri_NewWithPath2("Flats:", RC_NULL);
    Uri_SetPath(uri, Str_Text(&path));
    Str_Free(&path);

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    if(iterlist_t *list = P_GetSectorIterListForTag(tag, false))
    {
        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);

        Sector *sec;
        while((sec = (Sector *) IterList_MoveIterator(list)))
        {
            P_SetPtrp(sec, DMU_CEILING_MATERIAL, mat);
        }
    }

    return Continue;
}

static ACS_COMMAND(Restart)
{
    PCodePtr = interp.scriptInfoByIndex(activeScript->infoIndex).address;
    return Continue;
}

static ACS_COMMAND(AndLogical)
{
    push(pop() && pop());
    return Continue;
}

static ACS_COMMAND(OrLogical)
{
    push(pop() || pop());
    return Continue;
}

static ACS_COMMAND(AndBitwise)
{
    push(pop() & pop());
    return Continue;
}

static ACS_COMMAND(OrBitwise)
{
    push(pop() | pop());
    return Continue;
}

static ACS_COMMAND(EorBitwise)
{
    push(pop() ^ pop());
    return Continue;
}

static ACS_COMMAND(NegateLogical)
{
    push(!pop());
    return Continue;
}

static ACS_COMMAND(LShift)
{
    int operand2 = pop();
    push(pop() << operand2);
    return Continue;
}

static ACS_COMMAND(RShift)
{
    int operand2 = pop();
    push(pop() >> operand2);
    return Continue;
}

static ACS_COMMAND(UnaryMinus)
{
    push(-pop());
    return Continue;
}

static ACS_COMMAND(IfNotGoto)
{
    if(pop())
    {
        PCodePtr++;
    }
    else
    {
        PCodePtr = (int *) (interp.bytecode() + LONG(*PCodePtr));
    }
    return Continue;
}

static ACS_COMMAND(LineSide)
{
    push(activeScript->side);
    return Continue;
}

static ACS_COMMAND(ScriptWait)
{
    interp.scriptInfoByIndex(activeScript->infoIndex).waitValue = pop();
    interp.scriptInfoByIndex(activeScript->infoIndex).state = WaitingForScript;
    return Stop;
}

static ACS_COMMAND(ScriptWaitDirect)
{
    interp.scriptInfoByIndex(activeScript->infoIndex).waitValue = LONG(*PCodePtr++);
    interp.scriptInfoByIndex(activeScript->infoIndex).state = WaitingForScript;
    return Stop;
}

static ACS_COMMAND(ClearLineSpecial)
{
    if(activeScript->line)
    {
        P_ToXLine(activeScript->line)->special = 0;
    }
    return Continue;
}

static ACS_COMMAND(CaseGoto)
{
    if(top() == LONG(*PCodePtr++))
    {
        PCodePtr = (int *) (interp.bytecode() + LONG(*PCodePtr));
        drop();
    }
    else
    {
        PCodePtr++;
    }
    return Continue;
}

static ACS_COMMAND(BeginPrint)
{
    *PrintBuffer = 0;
    return Continue;
}

static ACS_COMMAND(EndPrint)
{
    if(activeScript->activator && activeScript->activator->player)
    {
        P_SetMessage(activeScript->activator->player, 0, PrintBuffer);
    }
    else
    {
        // Send to everybody.
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
            {
                P_SetMessage(&players[i], 0, PrintBuffer);
            }
        }
    }

    return Continue;
}

static ACS_COMMAND(EndPrintBold)
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            P_SetYellowMessage(&players[i], 0, PrintBuffer);
        }
    }
    return Continue;
}

static ACS_COMMAND(PrintString)
{
    strcat(PrintBuffer, interp.string(pop()));
    return Continue;
}

static ACS_COMMAND(PrintNumber)
{
    char tempStr[16];
    sprintf(tempStr, "%d", pop());
    strcat(PrintBuffer, tempStr);
    return Continue;
}

static ACS_COMMAND(PrintCharacter)
{
    char *bufferEnd = PrintBuffer + strlen(PrintBuffer);
    *bufferEnd++ = pop();
    *bufferEnd = 0;
    return Continue;
}

static ACS_COMMAND(PlayerCount)
{
    int count = 0;
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        count += players[i].plr->inGame;
    }
    push(count);
    return Continue;
}

static ACS_COMMAND(GameType)
{
    int gametype;

    if(!IS_NETGAME)
    {
        gametype = 0; // singleplayer
    }
    else if(deathmatch)
    {
        gametype = 2; // deathmatch
    }
    else
    {
        gametype = 1; // cooperative
    }
    push(gametype);

    return Continue;
}

static ACS_COMMAND(GameSkill)
{
    push((int)gameSkill);
    return Continue;
}

static ACS_COMMAND(Timer)
{
    push(mapTime);
    return Continue;
}

static ACS_COMMAND(SectorSound)
{
    mobj_t *mobj = 0;
    if(activeScript->line)
    {
        Sector *front = (Sector *) P_GetPtrp(activeScript->line, DMU_FRONT_SECTOR);
        mobj = (mobj_t *) P_GetPtrp(front, DMU_EMITTER);
    }
    int volume = pop();

    S_StartSoundAtVolume(S_GetSoundID(interp.string(pop())), mobj, volume / 127.0f);
    return Continue;
}

static ACS_COMMAND(ThingSound)
{
    int volume   = pop();
    int sound    = S_GetSoundID(interp.string(pop()));
    int tid      = pop();
    int searcher = -1;

    mobj_t *mobj;
    while(sound && (mobj = P_FindMobjFromTID(tid, &searcher)))
    {
        S_StartSoundAtVolume(sound, mobj, volume / 127.0f);
    }

    return Continue;
}

static ACS_COMMAND(AmbientSound)
{
    mobj_t *mobj = 0; // For 3D positioning.
    mobj_t *plrmo = players[DISPLAYPLAYER].plr->mo;

    int volume = pop();

    // If we are playing 3D sounds, create a temporary source mobj for the sound.
    if(cfg.snd3D && plrmo)
    {
        // SpawnMobj calls P_Random. We don't want that the random generator gets
        // out of sync.
        if((mobj = P_SpawnMobjXYZ(MT_CAMERA,
                                  plrmo->origin[VX] + (((M_Random() - 127) * 2) << FRACBITS),
                                  plrmo->origin[VY] + (((M_Random() - 127) * 2) << FRACBITS),
                                  plrmo->origin[VZ] + (((M_Random() - 127) * 2) << FRACBITS),
                                  0, 0)))
        {
            mobj->tics = 5 * TICSPERSEC; // Five seconds should be enough.
        }
    }

    int sound = S_GetSoundID(interp.string(pop()));
    S_StartSoundAtVolume(sound, mobj, volume / 127.0f);

    return Continue;
}

static ACS_COMMAND(SoundSequence)
{
    mobj_t *mobj = 0;
    if(activeScript->line)
    {
        Sector *front = (Sector *) P_GetPtrp(activeScript->line, DMU_FRONT_SECTOR);
        mobj = (mobj_t *) P_GetPtrp(front, DMU_EMITTER);
    }
    SN_StartSequenceName(mobj, interp.string(pop()));

    return Continue;
}

static ACS_COMMAND(SetLineTexture)
{
#define TEXTURE_TOP 0
#define TEXTURE_MIDDLE 1
#define TEXTURE_BOTTOM 2

    ddstring_t path; Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, interp.string(pop())));

    Uri *uri = Uri_NewWithPath2("Textures:", RC_NULL);
    Uri_SetPath(uri, Str_Text(&path));
    Str_Free(&path);

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    int position = pop();
    int side     = pop();
    int lineTag  = pop();

    if(iterlist_t *list = P_GetLineIterListForTag(lineTag, false))
    {
        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);

        Line *line;
        while((line = (Line *) IterList_MoveIterator(list)))
        {
            Side *sdef = (Side *) P_GetPtrp(line, (side == 0? DMU_FRONT : DMU_BACK));

            if(position == TEXTURE_MIDDLE)
            {
                P_SetPtrp(sdef, DMU_MIDDLE_MATERIAL, mat);
            }
            else if(position == TEXTURE_BOTTOM)
            {
                P_SetPtrp(sdef, DMU_BOTTOM_MATERIAL, mat);
            }
            else // TEXTURE_TOP
            {
                P_SetPtrp(sdef, DMU_TOP_MATERIAL, mat);
            }
        }
    }

    return Continue;

#undef TEXTURE_BOTTOM
#undef TEXTURE_MIDDLE
#undef TEXTURE_TOP
}

static ACS_COMMAND(SetLineBlocking)
{
    int lineFlags = pop()? DDLF_BLOCKING : 0;
    int lineTag   = pop();

    if(iterlist_t *list = P_GetLineIterListForTag(lineTag, false))
    {
        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);

        Line *line;
        while((line = (Line *) IterList_MoveIterator(list)))
        {
            P_SetIntp(line, DMU_FLAGS, (P_GetIntp(line, DMU_FLAGS) & ~DDLF_BLOCKING) | lineFlags);
        }
    }

    return Continue;
}

static ACS_COMMAND(SetLineSpecial)
{
    int arg5    = pop();
    int arg4    = pop();
    int arg3    = pop();
    int arg2    = pop();
    int arg1    = pop();
    int special = pop();
    int lineTag = pop();

    if(iterlist_t *list = P_GetLineIterListForTag(lineTag, false))
    {
        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);

        Line *line;
        while((line = (Line *) IterList_MoveIterator(list)))
        {
            xline_t *xline = P_ToXLine(line);

            xline->special = special;
            xline->arg1 = arg1;
            xline->arg2 = arg2;
            xline->arg3 = arg3;
            xline->arg4 = arg4;
            xline->arg5 = arg5;
        }
    }

    return Continue;
}

void P_WriteGlobalACScriptData()
{
    SV_BeginSegment(ASEG_GLOBALSCRIPTDATA);

    SV_WriteByte(3); // version byte

    for(int i = 0; i < MAX_ACS_WORLD_VARS; ++i)
    {
        SV_WriteLong(WorldVars[i]);
    }

    // Serialize the deferred task queue.
    SV_WriteLong(deferredTasksSize);
    for(int i = 0; i < deferredTasksSize; ++i)
    {
        DeferredTask const *task = &deferredTasks[i];
        SV_WriteLong(task->map);
        SV_WriteLong(task->script);
        for(int k = 0; k < 4; ++k)
        {
            SV_WriteByte(task->args[k]);
        }
    }
}

void P_ReadGlobalACScriptData(int saveVersion)
{
    int ver = 1;

    if(saveVersion >= 7)
    {
        SV_AssertSegment(ASEG_GLOBALSCRIPTDATA);
        ver = SV_ReadByte();
    }

    for(int i = 0; i < MAX_ACS_WORLD_VARS; ++i)
    {
        WorldVars[i] = SV_ReadLong();
    }

    // Deserialize the deferred task queue.
    if(ver >= 3)
    {
        deferredTasksSize = SV_ReadLong();
        if(deferredTasksSize)
        {
            if(deferredTasks)
                deferredTasks = (DeferredTask *) Z_Realloc(deferredTasks, sizeof(DeferredTask) * deferredTasksSize, PU_GAMESTATIC);
            else
                deferredTasks = (DeferredTask *) Z_Malloc(sizeof(DeferredTask) * deferredTasksSize, PU_GAMESTATIC, 0);

            for(int i = 0; i < deferredTasksSize; ++i)
            {
                DeferredTask *task = &deferredTasks[i];

                task->map    = SV_ReadLong();
                task->script = SV_ReadLong();
                for(int k = 0; k < 4; ++k)
                {
                    task->args[k] = SV_ReadByte();
                }
            }
        }
    }
    else
    {
        // Old format.
        DeferredTask tempTasks[20];

        deferredTasksSize = 0;
        for(int i = 0; i < 20; ++i)
        {
            int map            = SV_ReadLong();
            DeferredTask *task = &tempTasks[map < 0? 19 : deferredTasksSize++];

            task->map    = map < 0? 0 : map-1;
            task->script = SV_ReadLong();
            for(int k = 0; k < 4; ++k)
            {
                task->args[k] = SV_ReadByte();
            }
        }

        if(saveVersion < 7)
        {
            SV_Seek(12); // Junk.
        }

        if(deferredTasksSize)
        {
            if(deferredTasks)
                deferredTasks = (DeferredTask *) Z_Realloc(deferredTasks, sizeof(DeferredTask) * deferredTasksSize, PU_GAMESTATIC);
            else
                deferredTasks = (DeferredTask *) Z_Malloc(sizeof(DeferredTask) * deferredTasksSize, PU_GAMESTATIC, 0);

            memcpy(deferredTasks, tempTasks, sizeof(DeferredTask) * deferredTasksSize);
        }
    }

    if(!deferredTasksSize && deferredTasks)
    {
        Z_Free(deferredTasks); deferredTasks = 0;
    }
}

void P_WriteMapACScriptData()
{
    SV_BeginSegment(ASEG_SCRIPTS);

    for(int i = 0; i < interp.scriptCount(); ++i)
    {
        BytecodeScriptInfo &info = interp.scriptInfoByIndex(i);
        SV_WriteShort(info.state);
        SV_WriteShort(info.waitValue);
    }

    for(int i = 0; i < MAX_ACS_MAP_VARS; ++i)
    {
        SV_WriteLong(MapVars[i]);
    }
}

void P_ReadMapACScriptData()
{
    SV_AssertSegment(ASEG_SCRIPTS);

    for(int i = 0; i < interp.scriptCount(); ++i)
    {
        BytecodeScriptInfo &info = interp.scriptInfoByIndex(i);

        info.state     = (ACScriptState) SV_ReadShort();
        info.waitValue = SV_ReadShort();
    }

    for(int i = 0; i < MAX_ACS_MAP_VARS; ++i)
    {
        MapVars[i] = SV_ReadLong();
    }
}

void ACScript_Thinker(ACScript *script)
{
    DENG_ASSERT(script != 0);

    BytecodeScriptInfo &info = interp.scriptInfoByIndex(script->infoIndex);
    if(info.state == Terminating)
    {
        interp.scriptFinished(script);
        return;
    }

    if(info.state != Running)
    {
        return;
    }

    if(script->delayCount)
    {
        script->delayCount--;
        return;
    }

    activeScript = script;
    PCodePtr     = script->ip;

    int action;
    while((action = PCodeCmds[LONG(*PCodePtr++)]()) == Continue)
    {}

    script->ip = PCodePtr;

    if(action == Terminate)
    {
        interp.scriptFinished(script);
    }
}

void ACScript_Write(ACScript const *th)
{
    DENG_ASSERT(th != 0);

    SV_WriteByte(1); // Write a version byte.

    SV_WriteLong(SV_ThingArchiveId(th->activator));
    SV_WriteLong(P_ToIndex(th->line));
    SV_WriteLong(th->side);
    SV_WriteLong(th->number);
    SV_WriteLong(th->infoIndex);
    SV_WriteLong(th->delayCount);
    for(uint i = 0; i < ACS_STACK_DEPTH; ++i)
    {
        SV_WriteLong(th->stack[i]);
    }
    SV_WriteLong(th->stackPtr);
    for(uint i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
    {
        SV_WriteLong(th->vars[i]);
    }
    SV_WriteLong(((byte const *)th->ip) - interp.bytecode());
}

int ACScript_Read(ACScript *th, int mapVersion)
{
    DENG_ASSERT(th != 0);

    if(mapVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        th->activator       = (mobj_t *) SV_ReadLong();
        th->activator       = SV_GetArchiveThing(PTR2INT(th->activator), &th->activator);

        int temp = SV_ReadLong();
        if(temp >= 0)
        {
            th->line        = (Line *)P_ToPtr(DMU_LINE, temp);
            DENG_ASSERT(th->line != 0);
        }
        else
        {
            th->line        = 0;
        }

        th->side            = SV_ReadLong();
        th->number          = SV_ReadLong();
        th->infoIndex       = SV_ReadLong();
        th->delayCount      = SV_ReadLong();

        for(uint i = 0; i < ACS_STACK_DEPTH; ++i)
        {
            th->stack[i] = SV_ReadLong();
        }

        th->stackPtr        = SV_ReadLong();

        for(uint i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
        {
            th->vars[i] = SV_ReadLong();
        }

        th->ip              = (int *) (interp.bytecode() + SV_ReadLong());
    }
    else
    {
        // Its in the old pre V4 format which serialized acs_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        th->activator       = (mobj_t*) SV_ReadLong();
        th->activator       = SV_GetArchiveThing(PTR2INT(th->activator), &th->activator);

        int temp = SV_ReadLong();
        if(temp >= 0)
        {
            th->line        = (Line *)P_ToPtr(DMU_LINE, temp);
            DENG_ASSERT(th->line != 0);
        }
        else
        {
            th->line        = 0;
        }

        th->side            = SV_ReadLong();
        th->number          = SV_ReadLong();
        th->infoIndex       = SV_ReadLong();
        th->delayCount      = SV_ReadLong();

        for(uint i = 0; i < ACS_STACK_DEPTH; ++i)
        {
            th->stack[i] = SV_ReadLong();
        }

        th->stackPtr        = SV_ReadLong();

        for(uint i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
        {
            th->vars[i] = SV_ReadLong();
        }

        th->ip              = (int *) (interp.bytecode() + SV_ReadLong());
    }

    th->thinker.function = (thinkfunc_t) ACScript_Thinker;

    return true; // Add this thinker.
}

D_CMD(ScriptInfo)
{
    int whichOne = argc == 2? atoi(argv[1]) : -1;

    for(int i = 0; i < interp.scriptCount(); ++i)
    {
        BytecodeScriptInfo &info = interp.scriptInfoByIndex(i);

        if(whichOne != -1 && whichOne != info.number)
            continue;

        App_Log(DE2_SCR_MSG, "%d %s (a: %d, w: %d)", info.number,
                scriptStateAsText(info.state), info.argCount, info.waitValue);
    }

    return true;
}
