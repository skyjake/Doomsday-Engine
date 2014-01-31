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
#include "p_saveg.h"
#include "p_saveio.h"
#include "p_sound.h"
#include <cstring>
#include <cstdio>

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
    int scriptNumber;
    int const *pcodePtr;
    int argCount;

    // Current state:
    /// @todo Move to a separate array, in ACScriptInterpreter
    ACScriptState state;
    int waitValue;
};

int ACScriptInterpreter::scriptInfoIndex(int scriptNumber)
{
    for(int i = 0; i < _scriptCount; ++i)
    {
        BytecodeScriptInfo &info = _scriptInfo[i];
        if(info.scriptNumber == scriptNumber)
        {
            return i;
        }
    }
    return -1;
}

ACScript *ACScriptInterpreter::newACScript(BytecodeScriptInfo &info, byte const args[], int delayCount)
{
    // Is the script is already executing?
    if(info.state != Inactive) return 0;

    ACScript *script = (ACScript *) Z_Calloc(sizeof(*script), PU_MAP, 0);
    script->thinker.function = (thinkfunc_t) ACScript_Thinker;

    script->number     = info.scriptNumber;
    script->infoIndex  = scriptInfoIndex(info.scriptNumber);
    script->pcodePtr   = info.pcodePtr;
    script->delayCount = delayCount;

    for(int i = 0; i < info.argCount; ++i)
    {
        DENG_ASSERT(args != 0);
        script->vars[i] = args[i];
    }

    Thinker_Add(&script->thinker);

    info.state = Running;

    return script;
}

bool ACScriptInterpreter::newDeferredTask(uint map, int scriptNumber, byte const args[])
{
    if(_deferredTasksSize)
    {
        // Don't allow duplicates.
        for(int i = 0; i < _deferredTasksSize; ++i)
        {
            DeferredTask &task = _deferredTasks[i];
            if(task.scriptNumber == scriptNumber && task.map == map)
            {
                return false;
            }
        }

        _deferredTasks = (DeferredTask *) Z_Realloc(_deferredTasks, ++_deferredTasksSize * sizeof(DeferredTask), PU_GAMESTATIC);
    }
    else
    {
        _deferredTasks = (DeferredTask *) Z_Malloc(sizeof(DeferredTask), PU_GAMESTATIC, 0);
        _deferredTasksSize = 1;
    }

    DeferredTask *task = &_deferredTasks[_deferredTasksSize - 1];

    task->map          = map;
    task->scriptNumber = scriptNumber;
    task->args[0]      = args[0];
    task->args[1]      = args[1];
    task->args[2]      = args[2];
    task->args[3]      = args[3];

    return true;
}

ACScriptInterpreter::ACScriptInterpreter()
    : _pcode(0)
    , _scriptCount(0)
    , _scriptInfo(0)
    , _stringCount(0)
    , _strings(0)
    , _deferredTasksSize(0)
    , _deferredTasks(0)
{}

void ACScriptInterpreter::loadBytecode(lumpnum_t lump)
{
#define OPEN_SCRIPTS_BASE 1000

    DENG_ASSERT(!IS_CLIENT);

    size_t const lumpLength = (lump >= 0? W_LumpLength(lump) : 0);

    App_Log(DE2_SCR_VERBOSE, "Loading ACS bytecode lump %s:%s (#%i)...",
            F_PrettyPath(Str_Text(W_LumpSourceFile(lump))),
            Str_Text(W_LumpName(lump)), lump);

    _scriptCount = 0;

    int const *readBuf = 0;
    if(lumpLength >= sizeof(BytecodeHeader))
    {
        void *region = Z_Malloc(lumpLength, PU_MAP, 0);
        W_ReadLump(lump, (uint8_t *)region);
        BytecodeHeader const *header = (BytecodeHeader const *) region;
        _pcode = (byte const *) header;

        if(LONG(header->infoOffset) < (int)lumpLength)
        {
            readBuf = (int *) ((byte const *) header + LONG(header->infoOffset));
            _scriptCount = LONG(*readBuf++);
        }
    }

    if(!_scriptCount)
    {
        // Empty/Invalid lump.
        App_Log(DE2_SCR_WARNING, "Lump #%i does not appear to be valid ACS bytecode data", lump);
        return;
    }

    _scriptInfo = (BytecodeScriptInfo *) Z_Calloc(_scriptCount * sizeof(*_scriptInfo), PU_MAP, 0);

    BytecodeScriptInfo *info = _scriptInfo;
    for(int i = 0; i < _scriptCount; ++i, info++)
    {
        info->scriptNumber = LONG(*readBuf++);
        info->pcodePtr     = (int const *)(_pcode + LONG(*readBuf++));
        info->argCount     = LONG(*readBuf++);
        info->state        = Inactive;

        if(info->scriptNumber >= OPEN_SCRIPTS_BASE)
        {
            info->flags |= BytecodeScriptInfo::Open;
            info->scriptNumber -= OPEN_SCRIPTS_BASE;
        }

        DENG_ASSERT(info->argCount <= MAX_ACS_SCRIPT_VARS);
    }

    _stringCount = LONG(*readBuf++);
    _strings = (Str *) Z_Malloc(_stringCount * sizeof(*_strings), PU_MAP, 0);
    for(int i = 0; i < _stringCount; ++i)
    {
        Str_InitStatic(&_strings[i], (char const *)(_pcode + LONG(*readBuf++)));
    }

#undef OPEN_SCRIPTS_BASE
}

int ACScriptInterpreter::scriptCount() const
{
    return _scriptCount;
}

void ACScriptInterpreter::startOpenScripts()
{
    DENG_ASSERT(!IS_CLIENT);

    // Each is allotted 1 second for initialization.
    for(int i = 0; i < _scriptCount; ++i)
    {
        BytecodeScriptInfo &info = _scriptInfo[i];

        if(info.flags & BytecodeScriptInfo::Open)
        {
            ACScript *script = newACScript(info, 0/*no args*/, TICSPERSEC);
            DENG_ASSERT(script != 0);
            DENG_UNUSED(script);
        }
    }
}

bool ACScriptInterpreter::startScript(int scriptNumber, uint map, byte const args[],
    mobj_t *activator, Line *line, int side)
{
    DENG_ASSERT(!IS_CLIENT);

    if(map && map - 1 != gameMap)
    {
        // Script is not for the current map.
        // Add it to the store to be started when that map is next entered.
        return newDeferredTask(map - 1, scriptNumber, args);
    }

    if(BytecodeScriptInfo *info = scriptInfoPtr(scriptNumber))
    {
        // Resume a suspended script?
        if(info->state == Suspended)
        {
            info->state = Running;
            return true;
        }
        else if(ACScript *script = newACScript(*info, args))
        {
            script->activator = activator;
            script->line      = line;
            script->side      = side;
            return true;
        }
    }
    else
    {
        // Script not found.
        App_Log(DE2_SCR_WARNING, "ACS: Unknown script #%i", scriptNumber);
    }

    return false; // Perhaps its already executing?
}

bool ACScriptInterpreter::suspendScript(int scriptNumber, uint /*map*/)
{
    if(BytecodeScriptInfo *info = scriptInfoPtr(scriptNumber))
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

bool ACScriptInterpreter::terminateScript(int scriptNumber, uint /*map*/)
{
    if(BytecodeScriptInfo *info = scriptInfoPtr(scriptNumber))
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

void ACScriptInterpreter::tagFinished(int tag)
{
    if(P_SectorTagIsBusy(tag)) return;

    for(int i = 0; i < _scriptCount; ++i)
    {
        BytecodeScriptInfo &info = _scriptInfo[i];
        if(info.state == WaitingForTag && info.waitValue == tag)
        {
            info.state = Running;
        }
    }
}

void ACScriptInterpreter::polyobjFinished(int tag)
{
    if(P_SectorTagIsBusy(tag)) return;

    for(int i = 0; i < _scriptCount; ++i)
    {
        BytecodeScriptInfo &info = _scriptInfo[i];
        if(info.state == WaitingForPolyobj && info.waitValue == tag)
        {
            info.state = Running;
        }
    }
}

bool ACScriptInterpreter::hasScriptEntrypoint(int scriptNumber)
{
    for(int i = 0; i < _scriptCount; ++i)
    {
        BytecodeScriptInfo &info = _scriptInfo[i];
        if(info.scriptNumber == scriptNumber)
        {
            return true;
        }
    }
    return false;
}

BytecodeScriptInfo &ACScriptInterpreter::scriptInfo(int scriptNumber)
{
    return _scriptInfo[scriptInfoIndex(scriptNumber)];
}

Str const *ACScriptInterpreter::string(int stringNumber) const
{
    DENG_ASSERT(stringNumber >= 0 && stringNumber < _stringCount);
    return &_strings[stringNumber];
}

byte const *ACScriptInterpreter::bytecode() const
{
    return _pcode;
}

void ACScriptInterpreter::clearDeferredTasks()
{
    Z_Free(_deferredTasks); _deferredTasks = 0;
    _deferredTasksSize = 0;
}

void ACScriptInterpreter::runDeferredTasks(uint map/*Uri const *mapUri*/)
{
    //DENG_ASSERT(mapUri != 0);

    if(deathmatch)
    {
        /// @todo Do we really want to disallow deferred ACS tasks in deathmatch?
        /// What is the actual intention here? -ds
        return;
    }

    int const origSize = _deferredTasksSize;

    int i = 0;
    while(i < _deferredTasksSize)
    {
        DeferredTask *task = &_deferredTasks[i];
        int scriptNumber = task->scriptNumber;

        if(task->map != map)
        {
            i++;
            continue;
        }

        if(BytecodeScriptInfo *info = scriptInfoPtr(scriptNumber))
        {
            // Resume a suspended script?
            if(info->state == Suspended)
            {
                info->state = Running;
            }
            else
            {
                newACScript(*info, task->args, TICSPERSEC);
            }
        }
        else
        {
            App_Log(DE2_SCR_WARNING, "ACS: Unknown script #%i", scriptNumber);
        }

        _deferredTasksSize -= 1;
        if(i == _deferredTasksSize)
            break;

        memmove(&_deferredTasks[i], &_deferredTasks[i + 1], sizeof(DeferredTask) * (_deferredTasksSize - i));
    }

    if(_deferredTasksSize == origSize)
        return;

    if(_deferredTasksSize)
    {
        _deferredTasks = (DeferredTask *) Z_Realloc(_deferredTasks, sizeof(DeferredTask) * _deferredTasksSize, PU_GAMESTATIC);
        return;
    }

    Z_Free(_deferredTasks);
    _deferredTasks = 0;
}

void ACScriptInterpreter::scriptFinished(ACScript *script)
{
    if(!script) return;

    // This script has now finished.
    scriptInfoFor(script).state = Inactive;

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

void ACScriptInterpreter::writeWorldScriptData(Writer *writer)
{
    SV_BeginSegment(ASEG_GLOBALSCRIPTDATA);

    Writer_WriteByte(writer, 3); // version byte

    for(int i = 0; i < MAX_ACS_WORLD_VARS; ++i)
    {
        Writer_WriteInt32(writer, worldVars[i]);
    }

    // Serialize the deferred task queue.
    Writer_WriteInt32(writer, _deferredTasksSize);
    for(int i = 0; i < _deferredTasksSize; ++i)
    {
        DeferredTask const *task = &_deferredTasks[i];
        Writer_WriteInt32(writer, task->map);
        Writer_WriteInt32(writer, task->scriptNumber);
        for(int k = 0; k < 4; ++k)
        {
            Writer_WriteByte(writer, task->args[k]);
        }
    }
}

void ACScriptInterpreter::readWorldScriptData(Reader *reader, int saveVersion)
{
    int ver = 1;

    if(saveVersion >= 7)
    {
        SV_AssertSegment(ASEG_GLOBALSCRIPTDATA);
        ver = Reader_ReadByte(reader);
    }

    for(int i = 0; i < MAX_ACS_WORLD_VARS; ++i)
    {
        worldVars[i] = Reader_ReadInt32(reader);
    }

    // Deserialize the deferred task queue.
    if(ver >= 3)
    {
        _deferredTasksSize = Reader_ReadInt32(reader);
        if(_deferredTasksSize)
        {
            if(_deferredTasks)
                _deferredTasks = (DeferredTask *) Z_Realloc(_deferredTasks, sizeof(DeferredTask) * _deferredTasksSize, PU_GAMESTATIC);
            else
                _deferredTasks = (DeferredTask *) Z_Malloc(sizeof(DeferredTask) * _deferredTasksSize, PU_GAMESTATIC, 0);

            for(int i = 0; i < _deferredTasksSize; ++i)
            {
                DeferredTask *task = &_deferredTasks[i];

                task->map          = Reader_ReadInt32(reader);
                task->scriptNumber = Reader_ReadInt32(reader);
                for(int k = 0; k < 4; ++k)
                {
                    task->args[k] = Reader_ReadByte(reader);
                }
            }
        }
    }
    else
    {
        // Old format.
        DeferredTask tempTasks[20];

        _deferredTasksSize = 0;
        for(int i = 0; i < 20; ++i)
        {
            int map            = Reader_ReadInt32(reader);
            DeferredTask *task = &tempTasks[map < 0? 19 : _deferredTasksSize++];

            task->map          = map < 0? 0 : map-1;
            task->scriptNumber = Reader_ReadInt32(reader);
            for(int k = 0; k < 4; ++k)
            {
                task->args[k] = Reader_ReadByte(reader);
            }
        }

        if(saveVersion < 7)
        {
            SV_Seek(12); // Junk.
        }

        if(_deferredTasksSize)
        {
            if(_deferredTasks)
                _deferredTasks = (DeferredTask *) Z_Realloc(_deferredTasks, sizeof(DeferredTask) * _deferredTasksSize, PU_GAMESTATIC);
            else
                _deferredTasks = (DeferredTask *) Z_Malloc(sizeof(DeferredTask) * _deferredTasksSize, PU_GAMESTATIC, 0);

            memcpy(_deferredTasks, tempTasks, sizeof(DeferredTask) * _deferredTasksSize);
        }
    }

    if(!_deferredTasksSize && _deferredTasks)
    {
        Z_Free(_deferredTasks); _deferredTasks = 0;
    }
}

void ACScriptInterpreter::writeMapScriptData(Writer *writer)
{
    SV_BeginSegment(ASEG_SCRIPTS);

    for(int i = 0; i < _scriptCount; ++i)
    {
        BytecodeScriptInfo &info = _scriptInfo[i];
        Writer_WriteInt16(writer, info.state);
        Writer_WriteInt16(writer, info.waitValue);
    }

    for(int i = 0; i < MAX_ACS_MAP_VARS; ++i)
    {
        Writer_WriteInt32(writer, mapVars[i]);
    }
}

void ACScriptInterpreter::readMapScriptData(Reader *reader)
{
    SV_AssertSegment(ASEG_SCRIPTS);

    for(int i = 0; i < _scriptCount; ++i)
    {
        BytecodeScriptInfo &info = _scriptInfo[i];

        info.state     = ACScriptState( Reader_ReadInt16(reader) );
        info.waitValue = Reader_ReadInt16(reader);
    }

    for(int i = 0; i < MAX_ACS_MAP_VARS; ++i)
    {
        mapVars[i] = Reader_ReadInt32(reader);
    }
}

BytecodeScriptInfo &ACScriptInterpreter::scriptInfoByIndex(int index)
{
    DENG_ASSERT(index >= 0 && index < _scriptCount);
    return _scriptInfo[index];
}

BytecodeScriptInfo &ACScriptInterpreter::scriptInfoFor(ACScript *script)
{
    DENG_ASSERT(script != 0);
    return scriptInfoByIndex(script->infoIndex);
}

static byte SpecArgs[5];

#define PRINT_BUFFER_SIZE 256
static char PrintBuffer[PRINT_BUFFER_SIZE];

/// The One ACS interpreter instance.
static ACScriptInterpreter interp;

/// POD structure passed to bytecode commands.
struct CommandArgs
{
    ACScript *script;
    BytecodeScriptInfo *scriptInfo;
};

/// Bytecode command return value.
enum CommandResult
{
    Continue,
    Stop,
    Terminate
};

typedef CommandResult (*CommandFunc) (CommandArgs const &args);

/// Helper macro for declaring an ACS command function.
#define ACS_COMMAND(Name) CommandResult cmd##Name(CommandArgs const &args)

/// ACS command helper macros:
#define S_INTERPRETER() args.script->interpreter()
#define S_PCODEPTR      args.script->pcodePtr
#define S_PUSH(value)   args.script->push(value)
#define S_POP()         args.script->pop()
#define S_TOP()         args.script->top()
#define S_DROP()        args.script->drop()

ACS_COMMAND(NOP)
{
    return Continue;
}

ACS_COMMAND(Terminate)
{
    return Terminate;
}

ACS_COMMAND(Suspend)
{
    args.scriptInfo->state = Suspended;
    return Stop;
}

ACS_COMMAND(PushNumber)
{
    S_PUSH(LONG(*S_PCODEPTR++));
    return Continue;
}

ACS_COMMAND(LSpec1)
{
    int special = LONG(*S_PCODEPTR++);
    SpecArgs[0] = S_POP();
    P_ExecuteLineSpecial(special, SpecArgs, args.script->line, args.script->side,
                         args.script->activator);

    return Continue;
}

ACS_COMMAND(LSpec2)
{
    int special = LONG(*S_PCODEPTR++);
    SpecArgs[1] = S_POP();
    SpecArgs[0] = S_POP();
    P_ExecuteLineSpecial(special, SpecArgs, args.script->line, args.script->side,
                         args.script->activator);

    return Continue;
}

ACS_COMMAND(LSpec3)
{
    int special = LONG(*S_PCODEPTR++);
    SpecArgs[2] = S_POP();
    SpecArgs[1] = S_POP();
    SpecArgs[0] = S_POP();
    P_ExecuteLineSpecial(special, SpecArgs, args.script->line, args.script->side,
                         args.script->activator);

    return Continue;
}

ACS_COMMAND(LSpec4)
{
    int special = LONG(*S_PCODEPTR++);
    SpecArgs[3] = S_POP();
    SpecArgs[2] = S_POP();
    SpecArgs[1] = S_POP();
    SpecArgs[0] = S_POP();
    P_ExecuteLineSpecial(special, SpecArgs, args.script->line, args.script->side,
                         args.script->activator);

    return Continue;
}

ACS_COMMAND(LSpec5)
{
    int special = LONG(*S_PCODEPTR++);
    SpecArgs[4] = S_POP();
    SpecArgs[3] = S_POP();
    SpecArgs[2] = S_POP();
    SpecArgs[1] = S_POP();
    SpecArgs[0] = S_POP();
    P_ExecuteLineSpecial(special, SpecArgs, args.script->line, args.script->side,
                         args.script->activator);

    return Continue;
}

ACS_COMMAND(LSpec1Direct)
{
    int special = LONG(*S_PCODEPTR++);
    SpecArgs[0] = LONG(*S_PCODEPTR++);
    P_ExecuteLineSpecial(special, SpecArgs, args.script->line, args.script->side,
                         args.script->activator);

    return Continue;
}

ACS_COMMAND(LSpec2Direct)
{
    int special = LONG(*S_PCODEPTR++);
    SpecArgs[0] = LONG(*S_PCODEPTR++);
    SpecArgs[1] = LONG(*S_PCODEPTR++);
    P_ExecuteLineSpecial(special, SpecArgs, args.script->line, args.script->side,
                         args.script->activator);

    return Continue;
}

ACS_COMMAND(LSpec3Direct)
{
    int special = LONG(*S_PCODEPTR++);
    SpecArgs[0] = LONG(*S_PCODEPTR++);
    SpecArgs[1] = LONG(*S_PCODEPTR++);
    SpecArgs[2] = LONG(*S_PCODEPTR++);
    P_ExecuteLineSpecial(special, SpecArgs, args.script->line, args.script->side,
                         args.script->activator);

    return Continue;
}

ACS_COMMAND(LSpec4Direct)
{
    int special = LONG(*S_PCODEPTR++);
    SpecArgs[0] = LONG(*S_PCODEPTR++);
    SpecArgs[1] = LONG(*S_PCODEPTR++);
    SpecArgs[2] = LONG(*S_PCODEPTR++);
    SpecArgs[3] = LONG(*S_PCODEPTR++);
    P_ExecuteLineSpecial(special, SpecArgs, args.script->line, args.script->side,
                         args.script->activator);

    return Continue;
}

ACS_COMMAND(LSpec5Direct)
{
    int special = LONG(*S_PCODEPTR++);
    SpecArgs[0] = LONG(*S_PCODEPTR++);
    SpecArgs[1] = LONG(*S_PCODEPTR++);
    SpecArgs[2] = LONG(*S_PCODEPTR++);
    SpecArgs[3] = LONG(*S_PCODEPTR++);
    SpecArgs[4] = LONG(*S_PCODEPTR++);
    P_ExecuteLineSpecial(special, SpecArgs, args.script->line, args.script->side,
                         args.script->activator);

    return Continue;
}

ACS_COMMAND(Add)
{
    S_PUSH(S_POP() + S_POP());
    return Continue;
}

ACS_COMMAND(Subtract)
{
    int operand2 = S_POP();
    S_PUSH(S_POP() - operand2);
    return Continue;
}

ACS_COMMAND(Multiply)
{
    S_PUSH(S_POP() * S_POP());
    return Continue;
}

ACS_COMMAND(Divide)
{
    int operand2 = S_POP();
    S_PUSH(S_POP() / operand2);
    return Continue;
}

ACS_COMMAND(Modulus)
{
    int operand2 = S_POP();
    S_PUSH(S_POP() % operand2);
    return Continue;
}

ACS_COMMAND(EQ)
{
    S_PUSH(S_POP() == S_POP());
    return Continue;
}

ACS_COMMAND(NE)
{
    S_PUSH(S_POP() != S_POP());
    return Continue;
}

ACS_COMMAND(LT)
{
    int operand2 = S_POP();
    S_PUSH(S_POP() < operand2);
    return Continue;
}

ACS_COMMAND(GT)
{
    int operand2 = S_POP();
    S_PUSH(S_POP() > operand2);
    return Continue;
}

ACS_COMMAND(LE)
{
    int operand2 = S_POP();
    S_PUSH(S_POP() <= operand2);
    return Continue;
}

ACS_COMMAND(GE)
{
    int operand2 = S_POP();
    S_PUSH(S_POP() >= operand2);
    return Continue;
}

ACS_COMMAND(AssignScriptVar)
{
    args.script->vars[LONG(*S_PCODEPTR++)] = S_POP();
    return Continue;
}

ACS_COMMAND(AssignMapVar)
{
    S_INTERPRETER().mapVars[LONG(*S_PCODEPTR++)] = S_POP();
    return Continue;
}

ACS_COMMAND(AssignWorldVar)
{
    S_INTERPRETER().worldVars[LONG(*S_PCODEPTR++)] = S_POP();
    return Continue;
}

ACS_COMMAND(PushScriptVar)
{
    S_PUSH(args.script->vars[LONG(*S_PCODEPTR++)]);
    return Continue;
}

ACS_COMMAND(PushMapVar)
{
    S_PUSH(S_INTERPRETER().mapVars[LONG(*S_PCODEPTR++)]);
    return Continue;
}

ACS_COMMAND(PushWorldVar)
{
    S_PUSH(S_INTERPRETER().worldVars[LONG(*S_PCODEPTR++)]);
    return Continue;
}

ACS_COMMAND(AddScriptVar)
{
    args.script->vars[LONG(*S_PCODEPTR++)] += S_POP();
    return Continue;
}

ACS_COMMAND(AddMapVar)
{
    S_INTERPRETER().mapVars[LONG(*S_PCODEPTR++)] += S_POP();
    return Continue;
}

ACS_COMMAND(AddWorldVar)
{
    S_INTERPRETER().worldVars[LONG(*S_PCODEPTR++)] += S_POP();
    return Continue;
}

ACS_COMMAND(SubScriptVar)
{
    args.script->vars[LONG(*S_PCODEPTR++)] -= S_POP();
    return Continue;
}

ACS_COMMAND(SubMapVar)
{
    S_INTERPRETER().mapVars[LONG(*S_PCODEPTR++)] -= S_POP();
    return Continue;
}

ACS_COMMAND(SubWorldVar)
{
    S_INTERPRETER().worldVars[LONG(*S_PCODEPTR++)] -= S_POP();
    return Continue;
}

ACS_COMMAND(MulScriptVar)
{
    args.script->vars[LONG(*S_PCODEPTR++)] *= S_POP();
    return Continue;
}

ACS_COMMAND(MulMapVar)
{
    S_INTERPRETER().mapVars[LONG(*S_PCODEPTR++)] *= S_POP();
    return Continue;
}

ACS_COMMAND(MulWorldVar)
{
    S_INTERPRETER().worldVars[LONG(*S_PCODEPTR++)] *= S_POP();
    return Continue;
}

ACS_COMMAND(DivScriptVar)
{
    args.script->vars[LONG(*S_PCODEPTR++)] /= S_POP();
    return Continue;
}

ACS_COMMAND(DivMapVar)
{
    S_INTERPRETER().mapVars[LONG(*S_PCODEPTR++)] /= S_POP();
    return Continue;
}

ACS_COMMAND(DivWorldVar)
{
    S_INTERPRETER().worldVars[LONG(*S_PCODEPTR++)] /= S_POP();
    return Continue;
}

ACS_COMMAND(ModScriptVar)
{
    args.script->vars[LONG(*S_PCODEPTR++)] %= S_POP();
    return Continue;
}

ACS_COMMAND(ModMapVar)
{
    S_INTERPRETER().mapVars[LONG(*S_PCODEPTR++)] %= S_POP();
    return Continue;
}

ACS_COMMAND(ModWorldVar)
{
    S_INTERPRETER().worldVars[LONG(*S_PCODEPTR++)] %= S_POP();
    return Continue;
}

ACS_COMMAND(IncScriptVar)
{
    args.script->vars[LONG(*S_PCODEPTR++)]++;
    return Continue;
}

ACS_COMMAND(IncMapVar)
{
    S_INTERPRETER().mapVars[LONG(*S_PCODEPTR++)]++;
    return Continue;
}

ACS_COMMAND(IncWorldVar)
{
    S_INTERPRETER().worldVars[LONG(*S_PCODEPTR++)]++;
    return Continue;
}

ACS_COMMAND(DecScriptVar)
{
    args.script->vars[LONG(*S_PCODEPTR++)]--;
    return Continue;
}

ACS_COMMAND(DecMapVar)
{
    S_INTERPRETER().mapVars[LONG(*S_PCODEPTR++)]--;
    return Continue;
}

ACS_COMMAND(DecWorldVar)
{
    S_INTERPRETER().worldVars[LONG(*S_PCODEPTR++)]--;
    return Continue;
}

ACS_COMMAND(Goto)
{
    S_PCODEPTR = (int *) (S_INTERPRETER().bytecode() + LONG(*S_PCODEPTR));
    return Continue;
}

ACS_COMMAND(IfGoto)
{
    if(S_POP())
    {
        S_PCODEPTR = (int *) (S_INTERPRETER().bytecode() + LONG(*S_PCODEPTR));
    }
    else
    {
        S_PCODEPTR++;
    }
    return Continue;
}

ACS_COMMAND(Drop)
{
    S_DROP();
    return Continue;
}

ACS_COMMAND(Delay)
{
    args.script->delayCount = S_POP();
    return Stop;
}

ACS_COMMAND(DelayDirect)
{
    args.script->delayCount = LONG(*S_PCODEPTR++);
    return Stop;
}

ACS_COMMAND(Random)
{
    int high = S_POP();
    int low  = S_POP();
    S_PUSH(low + (P_Random() % (high - low + 1)));
    return Continue;
}

ACS_COMMAND(RandomDirect)
{
    int low  = LONG(*S_PCODEPTR++);
    int high = LONG(*S_PCODEPTR++);
    S_PUSH(low + (P_Random() % (high - low + 1)));
    return Continue;
}

ACS_COMMAND(ThingCount)
{
    int tid  = S_POP();
    int type = S_POP();
    // Anything to count?
    if(type + tid)
    {
        S_PUSH(P_MobjCount(type, tid));
    }
    return Continue;
}

ACS_COMMAND(ThingCountDirect)
{
    int type = LONG(*S_PCODEPTR++);
    int tid  = LONG(*S_PCODEPTR++);
    // Anything to count?
    if(type + tid)
    {
        S_PUSH(P_MobjCount(type, tid));
    }
    return Continue;
}

ACS_COMMAND(TagWait)
{
    args.scriptInfo->waitValue = S_POP();
    args.scriptInfo->state = WaitingForTag;
    return Stop;
}

ACS_COMMAND(TagWaitDirect)
{
    args.scriptInfo->waitValue = LONG(*S_PCODEPTR++);
    args.scriptInfo->state = WaitingForTag;
    return Stop;
}

ACS_COMMAND(PolyWait)
{
    args.scriptInfo->waitValue = S_POP();
    args.scriptInfo->state = WaitingForPolyobj;
    return Stop;
}

ACS_COMMAND(PolyWaitDirect)
{
    args.scriptInfo->waitValue = LONG(*S_PCODEPTR++);
    args.scriptInfo->state = WaitingForPolyobj;
    return Stop;
}

ACS_COMMAND(ChangeFloor)
{
    AutoStr *path = Str_PercentEncode(Str_Copy(AutoStr_New(), S_INTERPRETER().string(S_POP())));
    Uri *uri = Uri_NewWithPath3("Flats", Str_Text(path));

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    int tag = S_POP();

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

ACS_COMMAND(ChangeFloorDirect)
{
    int tag = LONG(*S_PCODEPTR++);

    AutoStr *path = Str_PercentEncode(Str_Copy(AutoStr_New(), S_INTERPRETER().string(LONG(*S_PCODEPTR++))));
    Uri *uri = Uri_NewWithPath3("Flats", Str_Text(path));

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

ACS_COMMAND(ChangeCeiling)
{
    AutoStr *path = Str_PercentEncode(Str_Copy(AutoStr_New(), S_INTERPRETER().string(S_POP())));
    Uri *uri = Uri_NewWithPath3("Flats", Str_Text(path));

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    int tag = S_POP();

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

ACS_COMMAND(ChangeCeilingDirect)
{
    int tag = LONG(*S_PCODEPTR++);

    AutoStr *path = Str_PercentEncode(Str_Copy(AutoStr_New(), S_INTERPRETER().string(LONG(*S_PCODEPTR++))));
    Uri *uri = Uri_NewWithPath3("Flats", Str_Text(path));

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

ACS_COMMAND(Restart)
{
    S_PCODEPTR = args.scriptInfo->pcodePtr;
    return Continue;
}

ACS_COMMAND(AndLogical)
{
    S_PUSH(S_POP() && S_POP());
    return Continue;
}

ACS_COMMAND(OrLogical)
{
    S_PUSH(S_POP() || S_POP());
    return Continue;
}

ACS_COMMAND(AndBitwise)
{
    S_PUSH(S_POP() & S_POP());
    return Continue;
}

ACS_COMMAND(OrBitwise)
{
    S_PUSH(S_POP() | S_POP());
    return Continue;
}

ACS_COMMAND(EorBitwise)
{
    S_PUSH(S_POP() ^ S_POP());
    return Continue;
}

ACS_COMMAND(NegateLogical)
{
    S_PUSH(!S_POP());
    return Continue;
}

ACS_COMMAND(LShift)
{
    int operand2 = S_POP();
    S_PUSH(S_POP() << operand2);
    return Continue;
}

ACS_COMMAND(RShift)
{
    int operand2 = S_POP();
    S_PUSH(S_POP() >> operand2);
    return Continue;
}

ACS_COMMAND(UnaryMinus)
{
    S_PUSH(-S_POP());
    return Continue;
}

ACS_COMMAND(IfNotGoto)
{
    if(S_POP())
    {
        S_PCODEPTR++;
    }
    else
    {
        S_PCODEPTR = (int *) (S_INTERPRETER().bytecode() + LONG(*S_PCODEPTR));
    }
    return Continue;
}

ACS_COMMAND(LineSide)
{
    S_PUSH(args.script->side);
    return Continue;
}

ACS_COMMAND(ScriptWait)
{
    args.scriptInfo->waitValue = S_POP();
    args.scriptInfo->state = WaitingForScript;
    return Stop;
}

ACS_COMMAND(ScriptWaitDirect)
{
    args.scriptInfo->waitValue = LONG(*S_PCODEPTR++);
    args.scriptInfo->state = WaitingForScript;
    return Stop;
}

ACS_COMMAND(ClearLineSpecial)
{
    if(args.script->line)
    {
        P_ToXLine(args.script->line)->special = 0;
    }
    return Continue;
}

ACS_COMMAND(CaseGoto)
{
    if(S_TOP() == LONG(*S_PCODEPTR++))
    {
        S_PCODEPTR = (int *) (S_INTERPRETER().bytecode() + LONG(*S_PCODEPTR));
        S_DROP();
    }
    else
    {
        S_PCODEPTR++;
    }
    return Continue;
}

ACS_COMMAND(BeginPrint)
{
    *PrintBuffer = 0;
    return Continue;
}

ACS_COMMAND(EndPrint)
{
    if(args.script->activator && args.script->activator->player)
    {
        P_SetMessage(args.script->activator->player, 0, PrintBuffer);
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

ACS_COMMAND(EndPrintBold)
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

ACS_COMMAND(PrintString)
{
    strcat(PrintBuffer, Str_Text(S_INTERPRETER().string(S_POP())));
    return Continue;
}

ACS_COMMAND(PrintNumber)
{
    char tempStr[16];
    sprintf(tempStr, "%d", S_POP());
    strcat(PrintBuffer, tempStr);
    return Continue;
}

ACS_COMMAND(PrintCharacter)
{
    char *bufferEnd = PrintBuffer + strlen(PrintBuffer);
    *bufferEnd++ = S_POP();
    *bufferEnd = 0;
    return Continue;
}

ACS_COMMAND(PlayerCount)
{
    int count = 0;
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        count += players[i].plr->inGame;
    }
    S_PUSH(count);
    return Continue;
}

ACS_COMMAND(GameType)
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
    S_PUSH(gametype);

    return Continue;
}

ACS_COMMAND(GameSkill)
{
    S_PUSH((int)gameSkill);
    return Continue;
}

ACS_COMMAND(Timer)
{
    S_PUSH(mapTime);
    return Continue;
}

ACS_COMMAND(SectorSound)
{
    mobj_t *mobj = 0;
    if(args.script->line)
    {
        Sector *front = (Sector *) P_GetPtrp(args.script->line, DMU_FRONT_SECTOR);
        mobj = (mobj_t *) P_GetPtrp(front, DMU_EMITTER);
    }
    int volume = S_POP();

    S_StartSoundAtVolume(S_GetSoundID(Str_Text(S_INTERPRETER().string(S_POP()))), mobj, volume / 127.0f);
    return Continue;
}

ACS_COMMAND(ThingSound)
{
    int volume   = S_POP();
    int sound    = S_GetSoundID(Str_Text(S_INTERPRETER().string(S_POP())));
    int tid      = S_POP();
    int searcher = -1;

    mobj_t *mobj;
    while(sound && (mobj = P_FindMobjFromTID(tid, &searcher)))
    {
        S_StartSoundAtVolume(sound, mobj, volume / 127.0f);
    }

    return Continue;
}

ACS_COMMAND(AmbientSound)
{
    mobj_t *mobj = 0; // For 3D positioning.
    mobj_t *plrmo = players[DISPLAYPLAYER].plr->mo;

    int volume = S_POP();

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

    int sound = S_GetSoundID(Str_Text(S_INTERPRETER().string(S_POP())));
    S_StartSoundAtVolume(sound, mobj, volume / 127.0f);

    return Continue;
}

ACS_COMMAND(SoundSequence)
{
    mobj_t *mobj = 0;
    if(args.script->line)
    {
        Sector *front = (Sector *) P_GetPtrp(args.script->line, DMU_FRONT_SECTOR);
        mobj = (mobj_t *) P_GetPtrp(front, DMU_EMITTER);
    }
    SN_StartSequenceName(mobj, Str_Text(S_INTERPRETER().string(S_POP())));

    return Continue;
}

ACS_COMMAND(SetLineTexture)
{
#define TEXTURE_TOP 0
#define TEXTURE_MIDDLE 1
#define TEXTURE_BOTTOM 2

    AutoStr *path = Str_PercentEncode(Str_Copy(AutoStr_New(), S_INTERPRETER().string(S_POP())));
    Uri *uri = Uri_NewWithPath3("Textures", Str_Text(path));

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    int position = S_POP();
    int side     = S_POP();
    int lineTag  = S_POP();

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

ACS_COMMAND(SetLineBlocking)
{
    int lineFlags = S_POP()? DDLF_BLOCKING : 0;
    int lineTag   = S_POP();

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

ACS_COMMAND(SetLineSpecial)
{
    int arg5    = S_POP();
    int arg4    = S_POP();
    int arg3    = S_POP();
    int arg2    = S_POP();
    int arg1    = S_POP();
    int special = S_POP();
    int lineTag = S_POP();

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

static CommandFunc pcodeCmds[] =
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

void P_InitACScript()
{
    memset(interp.worldVars, 0, sizeof(interp.worldVars));
    interp.clearDeferredTasks();
}

void P_LoadACScripts(lumpnum_t lump)
{
    if(IS_CLIENT) return;

    interp.loadBytecode(lump);

    memset(interp.mapVars, 0, sizeof(interp.mapVars));

    // Start all scripts flagged to begin immediately.
    interp.startOpenScripts();
}

void P_ACScriptRunDeferredTasks(uint map/*Uri const *mapUri*/)
{
    interp.runDeferredTasks(map);
}

dd_bool P_StartACScript(int scriptNumber, uint map, byte const args[], mobj_t *activator,
    Line *line, int side)
{
    return interp.startScript(scriptNumber, map, args, activator, line, side);
}

dd_bool P_TerminateACScript(int scriptNumber, uint map)
{
    return interp.terminateScript(scriptNumber, map);
}

dd_bool P_SuspendACScript(int scriptNumber, uint map)
{
    return interp.suspendScript(scriptNumber, map);
}

void P_ACScriptTagFinished(int tag)
{
    interp.tagFinished(tag);
}

void P_ACScriptPolyobjFinished(int tag)
{
    interp.polyobjFinished(tag);
}

void P_WriteGlobalACScriptData(Writer *writer)
{
    interp.writeWorldScriptData(writer);
}

void P_ReadGlobalACScriptData(Reader *reader, int saveVersion)
{
    interp.readWorldScriptData(reader, saveVersion);
}

void P_WriteMapACScriptData(Writer *writer)
{
    interp.writeMapScriptData(writer);
}

void P_ReadMapACScriptData(Reader *reader)
{
    interp.readMapScriptData(reader);
}

ACScriptInterpreter &ACScript::interpreter() const
{
    return interp;
}

void ACScript::runTick()
{
    ACScriptInterpreter &interp = interpreter();
    BytecodeScriptInfo &info = interp.scriptInfoFor(this);
    if(info.state == Terminating)
    {
        interp.scriptFinished(this);
        return;
    }

    if(info.state != Running)
    {
        return;
    }

    if(delayCount)
    {
        delayCount--;
        return;
    }

    CommandArgs args;
    args.script     = this;
    args.scriptInfo = &info;

    int action;
    while((action = pcodeCmds[LONG(*pcodePtr++)](args)) == Continue)
    {}

    if(action == Terminate)
    {
        interp.scriptFinished(this);
    }
}

void ACScript::push(int value)
{
    stack[stackPtr++] = value;
}

int ACScript::pop()
{
    return stack[--stackPtr];
}

int ACScript::top() const
{
    return stack[stackPtr - 1];
}

void ACScript::drop()
{
    stackPtr--;
}

void ACScript_Thinker(ACScript *script)
{
    DENG_ASSERT(script != 0);
    script->runTick();
}

void ACScript::write(Writer *writer) const
{
    Writer_WriteByte(writer, 1); // Write a version byte.

    Writer_WriteInt32(writer, SV_ThingArchiveId(activator));
    Writer_WriteInt32(writer, P_ToIndex(line));
    Writer_WriteInt32(writer, side);
    Writer_WriteInt32(writer, number);
    Writer_WriteInt32(writer, infoIndex);
    Writer_WriteInt32(writer, delayCount);
    for(uint i = 0; i < ACS_STACK_DEPTH; ++i)
    {
        Writer_WriteInt32(writer, stack[i]);
    }
    Writer_WriteInt32(writer, stackPtr);
    for(uint i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
    {
        Writer_WriteInt32(writer, vars[i]);
    }
    Writer_WriteInt32(writer, ((byte const *)pcodePtr) - interpreter().bytecode());
}

int ACScript::read(Reader *reader, int mapVersion)
{
    if(mapVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ Reader_ReadByte(reader); // version byte.

        activator       = (mobj_t *) Reader_ReadInt32(reader);
        activator       = SV_GetArchiveThing(PTR2INT(activator), &activator);

        int temp = SV_ReadLong();
        if(temp >= 0)
        {
            line        = (Line *)P_ToPtr(DMU_LINE, temp);
            DENG_ASSERT(line != 0);
        }
        else
        {
            line        = 0;
        }

        side            = Reader_ReadInt32(reader);
        number          = Reader_ReadInt32(reader);
        infoIndex       = Reader_ReadInt32(reader);
        delayCount      = Reader_ReadInt32(reader);

        for(uint i = 0; i < ACS_STACK_DEPTH; ++i)
        {
            stack[i] = Reader_ReadInt32(reader);
        }

        stackPtr        = Reader_ReadInt32(reader);

        for(uint i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
        {
            vars[i] = Reader_ReadInt32(reader);
        }

        pcodePtr        = (int *) (interpreter().bytecode() + Reader_ReadInt32(reader));
    }
    else
    {
        // Its in the old pre V4 format which serialized acs_t
        // Padding at the start (an old thinker_t struct)
        byte junk[16]; // sizeof thinker_t
        Reader_Read(reader, junk, 16);

        // Start of used data members.
        activator       = (mobj_t *) Reader_ReadInt32(reader);
        activator       = SV_GetArchiveThing(PTR2INT(activator), &activator);

        int temp = Reader_ReadInt32(reader);
        if(temp >= 0)
        {
            line        = (Line *)P_ToPtr(DMU_LINE, temp);
            DENG_ASSERT(line != 0);
        }
        else
        {
            line        = 0;
        }

        side            = Reader_ReadInt32(reader);
        number          = Reader_ReadInt32(reader);
        infoIndex       = Reader_ReadInt32(reader);
        delayCount      = Reader_ReadInt32(reader);

        for(uint i = 0; i < ACS_STACK_DEPTH; ++i)
        {
            stack[i] = Reader_ReadInt32(reader);
        }

        stackPtr        = Reader_ReadInt32(reader);

        for(uint i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
        {
            vars[i] = Reader_ReadInt32(reader);
        }

        pcodePtr        = (int *) (interpreter().bytecode() + Reader_ReadInt32(reader));
    }

    thinker.function = (thinkfunc_t) ACScript_Thinker;

    return true; // Add this thinker.
}

D_CMD(ScriptInfo)
{
    int whichOne = argc == 2? atoi(argv[1]) : -1;

    for(int i = 0; i < interp.scriptCount(); ++i)
    {
        BytecodeScriptInfo &info = interp.scriptInfoByIndex(i);

        if(whichOne != -1 && whichOne != info.scriptNumber)
            continue;

        App_Log(DE2_SCR_MSG, "%d %s (a: %d, w: %d)", info.scriptNumber,
                scriptStateAsText(info.state), info.argCount, info.waitValue);
    }

    return true;
}
