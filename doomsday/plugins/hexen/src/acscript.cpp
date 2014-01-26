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

#define SCRIPT_CONTINUE 0
#define SCRIPT_STOP 1
#define SCRIPT_TERMINATE 2
#define OPEN_SCRIPTS_BASE 1000
#define PRINT_BUFFER_SIZE 256
#define GAME_SINGLE_PLAYER 0
#define GAME_NET_COOPERATIVE 1
#define GAME_NET_DEATHMATCH 2
#define TEXTURE_TOP 0
#define TEXTURE_MIDDLE 1
#define TEXTURE_BOTTOM 2

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
    int number;
    int const *address;
    int argCount;

    // Current state:
    /// @todo Move to a separate array, in Interpreter
    ACScriptState state;
    int waitValue;
};

/**
 * Action-Code Script Interpreter (ACS).
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

        _scriptInfo = (BytecodeScriptInfo *) Z_Malloc(_scriptCount * sizeof(BytecodeScriptInfo), PU_MAP, 0);
        memset(_scriptInfo, 0, _scriptCount * sizeof(BytecodeScriptInfo));

        BytecodeScriptInfo *info = _scriptInfo;
        for(int i = 0; i < _scriptCount; ++i, info++)
        {
            info->number   = LONG(*buffer++);
            info->address  = (int const *)(_pcode + LONG(*buffer++));
            info->argCount = LONG(*buffer++);
            info->state    = Inactive;
        }

        _stringCount = LONG(*buffer++);
        _strings = (char const **) Z_Malloc(_stringCount * sizeof(char *), PU_MAP, 0);
        for(int i = 0; i < _stringCount; ++i)
        {
            _strings[i] = (char const *)(_pcode + LONG(*buffer++));
        }
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

            if(info.number >= OPEN_SCRIPTS_BASE)
            {
                info.number -= OPEN_SCRIPTS_BASE;
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
        DENG_ASSERT(scriptNumber >= 0 && scriptNumber < _scriptCount);
        return _scriptInfo[scriptNumber];
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

static void scriptFinished(int number);
static bool tagBusy(int tag);
static bool newDeferredTask(uint map, int number, byte const *args);

static void push(int value);
static int pop();
static int top();
static void drop();

static int cmdNOP();
static int cmdTerminate();
static int cmdSuspend();
static int cmdPushNumber();
static int cmdLSpec1();
static int cmdLSpec2();
static int cmdLSpec3();
static int cmdLSpec4();
static int cmdLSpec5();
static int cmdLSpec1Direct();
static int cmdLSpec2Direct();
static int cmdLSpec3Direct();
static int cmdLSpec4Direct();
static int cmdLSpec5Direct();
static int cmdAdd();
static int cmdSubtract();
static int cmdMultiply();
static int cmdDivide();
static int cmdModulus();
static int cmdEQ();
static int cmdNE();
static int cmdLT();
static int cmdGT();
static int cmdLE();
static int cmdGE();
static int cmdAssignScriptVar();
static int cmdAssignMapVar();
static int cmdAssignWorldVar();
static int cmdPushScriptVar();
static int cmdPushMapVar();
static int cmdPushWorldVar();
static int cmdAddScriptVar();
static int cmdAddMapVar();
static int cmdAddWorldVar();
static int cmdSubScriptVar();
static int cmdSubMapVar();
static int cmdSubWorldVar();
static int cmdMulScriptVar();
static int cmdMulMapVar();
static int cmdMulWorldVar();
static int cmdDivScriptVar();
static int cmdDivMapVar();
static int cmdDivWorldVar();
static int cmdModScriptVar();
static int cmdModMapVar();
static int cmdModWorldVar();
static int cmdIncScriptVar();
static int cmdIncMapVar();
static int cmdIncWorldVar();
static int cmdDecScriptVar();
static int cmdDecMapVar();
static int cmdDecWorldVar();
static int cmdGoto();
static int cmdIfGoto();
static int cmdDrop();
static int cmdDelay();
static int cmdDelayDirect();
static int cmdRandom();
static int cmdRandomDirect();
static int cmdThingCount();
static int cmdThingCountDirect();
static int cmdTagWait();
static int cmdTagWaitDirect();
static int cmdPolyWait();
static int cmdPolyWaitDirect();
static int cmdChangeFloor();
static int cmdChangeFloorDirect();
static int cmdChangeCeiling();
static int cmdChangeCeilingDirect();
static int cmdRestart();
static int cmdAndLogical();
static int cmdOrLogical();
static int cmdAndBitwise();
static int cmdOrBitwise();
static int cmdEorBitwise();
static int cmdNegateLogical();
static int cmdLShift();
static int cmdRShift();
static int cmdUnaryMinus();
static int cmdIfNotGoto();
static int cmdLineSide();
static int cmdScriptWait();
static int cmdScriptWaitDirect();
static int cmdClearLineSpecial();
static int cmdCaseGoto();
static int cmdBeginPrint();
static int cmdEndPrint();
static int cmdPrintString();
static int cmdPrintNumber();
static int cmdPrintCharacter();
static int cmdPlayerCount();
static int cmdGameType();
static int cmdGameSkill();
static int cmdTimer();
static int cmdSectorSound();
static int cmdAmbientSound();
static int cmdSoundSequence();
static int cmdSetLineTexture();
static int cmdSetLineBlocking();
static int cmdSetLineSpecial();
static int cmdThingSound();
static int cmdEndPrintBold();

static void thingCount(int type, int tid);

static int MapVars[MAX_ACS_MAP_VARS];
static int WorldVars[MAX_ACS_WORLD_VARS];

static int const *PCodePtr;
static byte SpecArgs[8];

static char PrintBuffer[PRINT_BUFFER_SIZE];

static ACScript *activeScript;
static ACScript *newScript;

static char ErrorMsg[128];

static int (*PCodeCmds[]) () =
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
        BytecodeScriptInfo &info = interp.scriptInfo(i);
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
        BytecodeScriptInfo &info = interp.scriptInfo(i);
        if(info.state == WaitingForPolyobj && info.waitValue == tag)
        {
            info.state = Running;
        }
    }
}

static void scriptFinished(int number)
{
    for(int i = 0; i < interp.scriptCount(); ++i)
    {
        BytecodeScriptInfo &info = interp.scriptInfo(i);
        if(info.state == WaitingForScript && info.waitValue == number)
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

static int cmdNOP()
{
    return SCRIPT_CONTINUE;
}

static int cmdTerminate()
{
    return SCRIPT_TERMINATE;
}

static int cmdSuspend()
{
    interp.scriptInfo(activeScript->infoIndex).state = Suspended;
    return SCRIPT_STOP;
}

static int cmdPushNumber()
{
    push(LONG(*PCodePtr++));
    return SCRIPT_CONTINUE;
}

static int cmdLSpec1()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int cmdLSpec2()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[1] = pop();
    SpecArgs[0] = pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int cmdLSpec3()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[2] = pop();
    SpecArgs[1] = pop();
    SpecArgs[0] = pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int cmdLSpec4()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[3] = pop();
    SpecArgs[2] = pop();
    SpecArgs[1] = pop();
    SpecArgs[0] = pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int cmdLSpec5()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[4] = pop();
    SpecArgs[3] = pop();
    SpecArgs[2] = pop();
    SpecArgs[1] = pop();
    SpecArgs[0] = pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int cmdLSpec1Direct()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int cmdLSpec2Direct()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int cmdLSpec3Direct()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    SpecArgs[2] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int cmdLSpec4Direct()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    SpecArgs[2] = LONG(*PCodePtr++);
    SpecArgs[3] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int cmdLSpec5Direct()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    SpecArgs[2] = LONG(*PCodePtr++);
    SpecArgs[3] = LONG(*PCodePtr++);
    SpecArgs[4] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int cmdAdd()
{
    push(pop() + pop());
    return SCRIPT_CONTINUE;
}

static int cmdSubtract()
{
    int operand2 = pop();
    push(pop() - operand2);
    return SCRIPT_CONTINUE;
}

static int cmdMultiply()
{
    push(pop() * pop());
    return SCRIPT_CONTINUE;
}

static int cmdDivide()
{
    int operand2 = pop();
    push(pop() / operand2);
    return SCRIPT_CONTINUE;
}

static int cmdModulus()
{
    int operand2 = pop();
    push(pop() % operand2);
    return SCRIPT_CONTINUE;
}

static int cmdEQ()
{
    push(pop() == pop());
    return SCRIPT_CONTINUE;
}

static int cmdNE()
{
    push(pop() != pop());
    return SCRIPT_CONTINUE;
}

static int cmdLT()
{
    int operand2 = pop();
    push(pop() < operand2);
    return SCRIPT_CONTINUE;
}

static int cmdGT()
{
    int operand2 = pop();
    push(pop() > operand2);
    return SCRIPT_CONTINUE;
}

static int cmdLE()
{
    int operand2 = pop();
    push(pop() <= operand2);
    return SCRIPT_CONTINUE;
}

static int cmdGE()
{
    int operand2 = pop();
    push(pop() >= operand2);
    return SCRIPT_CONTINUE;
}

static int cmdAssignScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)] = pop();
    return SCRIPT_CONTINUE;
}

static int cmdAssignMapVar()
{
    MapVars[LONG(*PCodePtr++)] = pop();
    return SCRIPT_CONTINUE;
}

static int cmdAssignWorldVar()
{
    WorldVars[LONG(*PCodePtr++)] = pop();
    return SCRIPT_CONTINUE;
}

static int cmdPushScriptVar()
{
    push(activeScript->vars[LONG(*PCodePtr++)]);
    return SCRIPT_CONTINUE;
}

static int cmdPushMapVar()
{
    push(MapVars[LONG(*PCodePtr++)]);
    return SCRIPT_CONTINUE;
}

static int cmdPushWorldVar()
{
    push(WorldVars[LONG(*PCodePtr++)]);
    return SCRIPT_CONTINUE;
}

static int cmdAddScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)] += pop();
    return SCRIPT_CONTINUE;
}

static int cmdAddMapVar()
{
    MapVars[LONG(*PCodePtr++)] += pop();
    return SCRIPT_CONTINUE;
}

static int cmdAddWorldVar()
{
    WorldVars[LONG(*PCodePtr++)] += pop();
    return SCRIPT_CONTINUE;
}

static int cmdSubScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)] -= pop();
    return SCRIPT_CONTINUE;
}

static int cmdSubMapVar()
{
    MapVars[LONG(*PCodePtr++)] -= pop();
    return SCRIPT_CONTINUE;
}

static int cmdSubWorldVar()
{
    WorldVars[LONG(*PCodePtr++)] -= pop();
    return SCRIPT_CONTINUE;
}

static int cmdMulScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)] *= pop();
    return SCRIPT_CONTINUE;
}

static int cmdMulMapVar()
{
    MapVars[LONG(*PCodePtr++)] *= pop();
    return SCRIPT_CONTINUE;
}

static int cmdMulWorldVar()
{
    WorldVars[LONG(*PCodePtr++)] *= pop();
    return SCRIPT_CONTINUE;
}

static int cmdDivScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)] /= pop();
    return SCRIPT_CONTINUE;
}

static int cmdDivMapVar()
{
    MapVars[LONG(*PCodePtr++)] /= pop();
    return SCRIPT_CONTINUE;
}

static int cmdDivWorldVar()
{
    WorldVars[LONG(*PCodePtr++)] /= pop();
    return SCRIPT_CONTINUE;
}

static int cmdModScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)] %= pop();
    return SCRIPT_CONTINUE;
}

static int cmdModMapVar()
{
    MapVars[LONG(*PCodePtr++)] %= pop();
    return SCRIPT_CONTINUE;
}

static int cmdModWorldVar()
{
    WorldVars[LONG(*PCodePtr++)] %= pop();
    return SCRIPT_CONTINUE;
}

static int cmdIncScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)]++;
    return SCRIPT_CONTINUE;
}

static int cmdIncMapVar()
{
    MapVars[LONG(*PCodePtr++)]++;
    return SCRIPT_CONTINUE;
}

static int cmdIncWorldVar()
{
    WorldVars[LONG(*PCodePtr++)]++;
    return SCRIPT_CONTINUE;
}

static int cmdDecScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)]--;
    return SCRIPT_CONTINUE;
}

static int cmdDecMapVar()
{
    MapVars[LONG(*PCodePtr++)]--;
    return SCRIPT_CONTINUE;
}

static int cmdDecWorldVar()
{
    WorldVars[LONG(*PCodePtr++)]--;
    return SCRIPT_CONTINUE;
}

static int cmdGoto()
{
    PCodePtr = (int *) (interp.bytecode() + LONG(*PCodePtr));
    return SCRIPT_CONTINUE;
}

static int cmdIfGoto()
{
    if(pop())
    {
        PCodePtr = (int *) (interp.bytecode() + LONG(*PCodePtr));
    }
    else
    {
        PCodePtr++;
    }
    return SCRIPT_CONTINUE;
}

static int cmdDrop()
{
    drop();
    return SCRIPT_CONTINUE;
}

static int cmdDelay()
{
    activeScript->delayCount = pop();
    return SCRIPT_STOP;
}

static int cmdDelayDirect()
{
    activeScript->delayCount = LONG(*PCodePtr++);
    return SCRIPT_STOP;
}

static int cmdRandom()
{
    int high = pop();
    int low  = pop();
    push(low + (P_Random() % (high - low + 1)));
    return SCRIPT_CONTINUE;
}

static int cmdRandomDirect()
{
    int low  = LONG(*PCodePtr++);
    int high = LONG(*PCodePtr++);
    push(low + (P_Random() % (high - low + 1)));
    return SCRIPT_CONTINUE;
}

static int cmdThingCount()
{
    int tid = pop();
    thingCount(pop(), tid);
    return SCRIPT_CONTINUE;
}

static int cmdThingCountDirect()
{
    int type = LONG(*PCodePtr++);
    thingCount(type, LONG(*PCodePtr++));
    return SCRIPT_CONTINUE;
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

static int cmdTagWait()
{
    interp.scriptInfo(activeScript->infoIndex).waitValue = pop();
    interp.scriptInfo(activeScript->infoIndex).state = WaitingForTag;
    return SCRIPT_STOP;
}

static int cmdTagWaitDirect()
{
    interp.scriptInfo(activeScript->infoIndex).waitValue = LONG(*PCodePtr++);
    interp.scriptInfo(activeScript->infoIndex).state = WaitingForTag;
    return SCRIPT_STOP;
}

static int cmdPolyWait()
{
    interp.scriptInfo(activeScript->infoIndex).waitValue = pop();
    interp.scriptInfo(activeScript->infoIndex).state = WaitingForPolyobj;
    return SCRIPT_STOP;
}

static int cmdPolyWaitDirect()
{
    interp.scriptInfo(activeScript->infoIndex).waitValue = LONG(*PCodePtr++);
    interp.scriptInfo(activeScript->infoIndex).state = WaitingForPolyobj;
    return SCRIPT_STOP;
}

static int cmdChangeFloor()
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

    return SCRIPT_CONTINUE;
}

static int cmdChangeFloorDirect()
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

    return SCRIPT_CONTINUE;
}

static int cmdChangeCeiling()
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

    return SCRIPT_CONTINUE;
}

static int cmdChangeCeilingDirect()
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

    return SCRIPT_CONTINUE;
}

static int cmdRestart()
{
    PCodePtr = interp.scriptInfo(activeScript->infoIndex).address;
    return SCRIPT_CONTINUE;
}

static int cmdAndLogical()
{
    push(pop() && pop());
    return SCRIPT_CONTINUE;
}

static int cmdOrLogical()
{
    push(pop() || pop());
    return SCRIPT_CONTINUE;
}

static int cmdAndBitwise()
{
    push(pop() & pop());
    return SCRIPT_CONTINUE;
}

static int cmdOrBitwise()
{
    push(pop() | pop());
    return SCRIPT_CONTINUE;
}

static int cmdEorBitwise()
{
    push(pop() ^ pop());
    return SCRIPT_CONTINUE;
}

static int cmdNegateLogical()
{
    push(!pop());
    return SCRIPT_CONTINUE;
}

static int cmdLShift()
{
    int operand2 = pop();
    push(pop() << operand2);
    return SCRIPT_CONTINUE;
}

static int cmdRShift()
{
    int operand2 = pop();
    push(pop() >> operand2);
    return SCRIPT_CONTINUE;
}

static int cmdUnaryMinus()
{
    push(-pop());
    return SCRIPT_CONTINUE;
}

static int cmdIfNotGoto()
{
    if(pop())
    {
        PCodePtr++;
    }
    else
    {
        PCodePtr = (int *) (interp.bytecode() + LONG(*PCodePtr));
    }
    return SCRIPT_CONTINUE;
}

static int cmdLineSide()
{
    push(activeScript->side);
    return SCRIPT_CONTINUE;
}

static int cmdScriptWait()
{
    interp.scriptInfo(activeScript->infoIndex).waitValue = pop();
    interp.scriptInfo(activeScript->infoIndex).state = WaitingForScript;
    return SCRIPT_STOP;
}

static int cmdScriptWaitDirect()
{
    interp.scriptInfo(activeScript->infoIndex).waitValue = LONG(*PCodePtr++);
    interp.scriptInfo(activeScript->infoIndex).state = WaitingForScript;
    return SCRIPT_STOP;
}

static int cmdClearLineSpecial()
{
    if(activeScript->line)
    {
        P_ToXLine(activeScript->line)->special = 0;
    }
    return SCRIPT_CONTINUE;
}

static int cmdCaseGoto()
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
    return SCRIPT_CONTINUE;
}

static int cmdBeginPrint()
{
    *PrintBuffer = 0;
    return SCRIPT_CONTINUE;
}

static int cmdEndPrint()
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

    return SCRIPT_CONTINUE;
}

static int cmdEndPrintBold()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            P_SetYellowMessage(&players[i], 0, PrintBuffer);
        }
    }
    return SCRIPT_CONTINUE;
}

static int cmdPrintString()
{
    strcat(PrintBuffer, interp.string(pop()));
    return SCRIPT_CONTINUE;
}

static int cmdPrintNumber()
{
    char tempStr[16];
    sprintf(tempStr, "%d", pop());
    strcat(PrintBuffer, tempStr);
    return SCRIPT_CONTINUE;
}

static int cmdPrintCharacter()
{
    char *bufferEnd = PrintBuffer + strlen(PrintBuffer);
    *bufferEnd++ = pop();
    *bufferEnd = 0;
    return SCRIPT_CONTINUE;
}

static int cmdPlayerCount()
{
    int count = 0;
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        count += players[i].plr->inGame;
    }
    push(count);
    return SCRIPT_CONTINUE;
}

static int cmdGameType()
{
    int gametype;

    if(IS_NETGAME == false)
    {
        gametype = GAME_SINGLE_PLAYER;
    }
    else if(deathmatch)
    {
        gametype = GAME_NET_DEATHMATCH;
    }
    else
    {
        gametype = GAME_NET_COOPERATIVE;
    }
    push(gametype);

    return SCRIPT_CONTINUE;
}

static int cmdGameSkill()
{
    push((int)gameSkill);
    return SCRIPT_CONTINUE;
}

static int cmdTimer()
{
    push(mapTime);
    return SCRIPT_CONTINUE;
}

static int cmdSectorSound()
{
    mobj_t *mobj = 0;
    if(activeScript->line)
    {
        Sector *front = (Sector *) P_GetPtrp(activeScript->line, DMU_FRONT_SECTOR);
        mobj = (mobj_t *) P_GetPtrp(front, DMU_EMITTER);
    }
    int volume = pop();

    S_StartSoundAtVolume(S_GetSoundID(interp.string(pop())), mobj, volume / 127.0f);
    return SCRIPT_CONTINUE;
}

static int cmdThingSound()
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

    return SCRIPT_CONTINUE;
}

static int cmdAmbientSound()
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

    return SCRIPT_CONTINUE;
}

static int cmdSoundSequence()
{
    mobj_t *mobj = 0;
    if(activeScript->line)
    {
        Sector *front = (Sector *) P_GetPtrp(activeScript->line, DMU_FRONT_SECTOR);
        mobj = (mobj_t *) P_GetPtrp(front, DMU_EMITTER);
    }
    SN_StartSequenceName(mobj, interp.string(pop()));

    return SCRIPT_CONTINUE;
}

static int cmdSetLineTexture()
{
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
            else
            {                       // TEXTURE_TOP
                P_SetPtrp(sdef, DMU_TOP_MATERIAL, mat);
            }
        }
    }

    return SCRIPT_CONTINUE;
}

static int cmdSetLineBlocking()
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

    return SCRIPT_CONTINUE;
}

static int cmdSetLineSpecial()
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

    return SCRIPT_CONTINUE;
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
        BytecodeScriptInfo &info = interp.scriptInfo(i);
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
        BytecodeScriptInfo &info = interp.scriptInfo(i);

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

    BytecodeScriptInfo &info = interp.scriptInfo(script->infoIndex);
    if(info.state == Terminating)
    {
        info.state = Inactive;
        scriptFinished(activeScript->number);
        Thinker_Remove(&activeScript->thinker);
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

    PCodePtr = activeScript->ip;
    int action = 0;
    do
    {
        int const cmd = LONG(*PCodePtr++);
        action = PCodeCmds[cmd] ();
    } while(action == SCRIPT_CONTINUE);

    activeScript->ip = PCodePtr;
    if(action == SCRIPT_TERMINATE)
    {
        interp.scriptInfo(script->infoIndex).state = Inactive;
        scriptFinished(activeScript->number);
        Thinker_Remove(&activeScript->thinker);
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
        BytecodeScriptInfo &info = interp.scriptInfo(i);

        if(whichOne != -1 && whichOne != info.number)
            continue;

        App_Log(DE2_SCR_MSG, "%d %s (a: %d, w: %d)", info.number,
                scriptStateAsText(info.state), info.argCount, info.waitValue);
    }

    return true;
}
