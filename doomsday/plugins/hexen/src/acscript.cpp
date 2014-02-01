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
#include "g_common.h"
#include "p_player.h"
#include "p_map.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include "p_sound.h"
#include <cstring>
#include <cstdio>

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
    ACScriptInterpreter::ScriptState state;
    int waitValue;
};

void ACScriptInterpreter::DeferredTask::write(Writer *writer) const
{
    Uri_Write(mapUri, writer);
    Writer_WriteInt32(writer, scriptNumber);
    for(int i = 0; i < 4; ++i)
    {
        Writer_WriteByte(writer, args[i]);
    }
}

void ACScriptInterpreter::DeferredTask::read(Reader *reader, int segmentVersion)
{
    if(segmentVersion >= 4)
    {
        mapUri = Uri_FromReader(reader);
    }
    else
    {
        mapUri = G_ComposeMapUri(gameEpisode, Reader_ReadInt32(reader));
    }
    scriptNumber = Reader_ReadInt32(reader);
    for(int i = 0; i < 4; ++i)
    {
        args[i] = Reader_ReadByte(reader);
    }
}

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

    script->_info      = &info;
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

bool ACScriptInterpreter::newDeferredTask(Uri const *mapUri, int scriptNumber, byte const args[])
{
    // Don't allow duplicates.
    for(int i = 0; i < _deferredTasksSize; ++i)
    {
        DeferredTask &task = _deferredTasks[i];
        if(task.scriptNumber == scriptNumber &&
           Uri_Equality(task.mapUri, mapUri))
        {
            return false;
        }
    }

    _deferredTasks = (DeferredTask *) Z_Realloc(_deferredTasks, sizeof(*_deferredTasks) * ++_deferredTasksSize, PU_GAMESTATIC);
    DeferredTask *task = &_deferredTasks[_deferredTasksSize - 1];

    task->mapUri       = Uri_Dup(mapUri);
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

bool ACScriptInterpreter::startScript(int scriptNumber, Uri const *mapUri,
    byte const args[], mobj_t *activator, Line *line, int side)
{
    DENG_ASSERT(!IS_CLIENT);

    if(mapUri)
    {
        Uri *currentMapUri = G_CurrentMapUri();
        if(!Uri_Equality(mapUri, currentMapUri))
        {
            Uri_Delete(currentMapUri);

            // Script is not for the current map.
            // Add it to the store to be started when that map is next entered.
            return newDeferredTask(mapUri, scriptNumber, args);
        }
        Uri_Delete(currentMapUri);
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

bool ACScriptInterpreter::suspendScript(int scriptNumber, Uri const * /*mapUri*/)
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

bool ACScriptInterpreter::terminateScript(int scriptNumber, Uri const * /*mapUri*/)
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
    for(int i = 0; i < _deferredTasksSize; ++i)
    {
        Uri_Delete(_deferredTasks[i].mapUri);
    }
    Z_Free(_deferredTasks); _deferredTasks = 0;
    _deferredTasksSize = 0;
}

void ACScriptInterpreter::runDeferredTasks(Uri const *mapUri)
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

        if(!Uri_Equality(task->mapUri, mapUri))
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

        Uri_Delete(_deferredTasks[i].mapUri);

        _deferredTasksSize -= 1;
        if(i == _deferredTasksSize)
            break;

        memmove(&_deferredTasks[i], &_deferredTasks[i + 1], sizeof(DeferredTask) * (_deferredTasksSize - i));
    }

    if(_deferredTasksSize < origSize)
    {
        if(_deferredTasksSize)
        {
            _deferredTasks = (DeferredTask *) Z_Realloc(_deferredTasks, sizeof(DeferredTask) * _deferredTasksSize, PU_GAMESTATIC);
        }
        else
        {
            Z_Free(_deferredTasks);
            _deferredTasks = 0;
        }
    }
}

void ACScriptInterpreter::scriptFinished(ACScript *script)
{
    if(!script) return;

    // This script has now finished.
    script->info().state = Inactive;

    // Notify any scripts which are waiting for this script to finish.
    for(int i = 0; i < _scriptCount; ++i)
    {
        BytecodeScriptInfo &info = _scriptInfo[i];
        if(info.state == WaitingForScript && info.waitValue == script->info().scriptNumber)
        {
            info.state = Running;
        }
    }

    Thinker_Remove(&script->thinker);
}

void ACScriptInterpreter::writeWorldScriptData(Writer *writer)
{
    SV_BeginSegment(ASEG_GLOBALSCRIPTDATA);

    Writer_WriteByte(writer, 4); // version byte

    for(int i = 0; i < MAX_ACS_WORLD_VARS; ++i)
    {
        Writer_WriteInt32(writer, worldVars[i]);
    }

    // Serialize the deferred task queue.
    Writer_WriteInt32(writer, _deferredTasksSize);
    for(int i = 0; i < _deferredTasksSize; ++i)
    {
        _deferredTasks[i].write(writer);
    }
}

void ACScriptInterpreter::readWorldScriptData(Reader *reader, int mapVersion)
{
    int ver = 1;

    if(mapVersion >= 7)
    {
        SV_AssertSegment(ASEG_GLOBALSCRIPTDATA);
        ver = Reader_ReadByte(reader);
    }

    for(int i = 0; i < MAX_ACS_WORLD_VARS; ++i)
    {
        worldVars[i] = Reader_ReadInt32(reader);
    }

    // Deserialize the deferred task queue.
    clearDeferredTasks();
    if(ver >= 3)
    {
        _deferredTasksSize = Reader_ReadInt32(reader);
        if(_deferredTasksSize)
        {
            _deferredTasks = (DeferredTask *) Z_Realloc(_deferredTasks, sizeof(*_deferredTasks) * _deferredTasksSize, PU_GAMESTATIC);
            for(int i = 0; i < _deferredTasksSize; ++i)
            {
                _deferredTasks[i].read(reader, ver);
            }
        }
    }
    else
    {
        // Old format.
        for(int i = 0; i < 20; ++i)
        {
            int map          = Reader_ReadInt32(reader);
            int scriptNumber = Reader_ReadInt32(reader);
            byte args[4];
            for(int k = 0; k < 4; ++k)
            {
                args[k] = Reader_ReadByte(reader);
            }

            if(map > 0)
            {
                Uri *mapUri = G_ComposeMapUri(gameEpisode, map - 1);
                newDeferredTask(mapUri, scriptNumber, args);
                Uri_Delete(mapUri);
            }
        }

        if(mapVersion < 7)
        {
            SV_Seek(12); // Junk.
        }
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

void ACScriptInterpreter::readMapScriptData(Reader *reader, int /*mapVersion*/)
{
    SV_AssertSegment(ASEG_SCRIPTS);

    for(int i = 0; i < _scriptCount; ++i)
    {
        BytecodeScriptInfo &info = _scriptInfo[i];

        info.state     = ScriptState( Reader_ReadInt16(reader) );
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

AutoStr *ACScriptInterpreter::scriptName(int scriptNumber)
{
    BytecodeScriptInfo *info = scriptInfoPtr(scriptNumber);
    if(!info)
    {
        return AutoStr_FromTextStd("(invalid-scriptnumber)");
    }
    bool const open = (info->flags & BytecodeScriptInfo::Open) != 0;
    return Str_Appendf(AutoStr_NewStd(), "#%i%s", scriptNumber, open? " (Open)" : "");
}

AutoStr *ACScriptInterpreter::scriptDescription(int scriptNumber)
{
    BytecodeScriptInfo *info = scriptInfoPtr(scriptNumber);
    if(!info)
    {
        return AutoStr_FromTextStd("(invalid-scriptnumber)");
    }

    static char const *stateLabels[] = {
        "Inactive",
        "Running",
        "Suspended",
        "Waiting for tag",
        "Waiting for polyobj",
        "Waiting for script",
        "Terminating"
    };

    return Str_Appendf(AutoStr_NewStd(), "ACScript " DE2_ESC(b) "%s\n"
                                         DE2_ESC(l) "State: " DE2_ESC(.) DE2_ESC(i) "%s" DE2_ESC(.)
                                         DE2_ESC(l) " Wait-for: " DE2_ESC(.) DE2_ESC(i) "%i",
                                         Str_Text(scriptName(scriptNumber)),
                                         stateLabels[info->state], info->waitValue);
}

static byte SpecArgs[5];

#define PRINT_BUFFER_SIZE 256
static char PrintBuffer[PRINT_BUFFER_SIZE];

/// Bytecode command return value.
enum CommandResult
{
    Continue,
    Stop,
    Terminate
};

typedef CommandResult (*CommandFunc) (ACScript &acs);

/// Helper macro for declaring an ACS command function.
#define ACS_COMMAND(Name) CommandResult cmd##Name(ACScript &acs)

ACS_COMMAND(NOP)
{
    DENG_UNUSED(acs);
    return Continue;
}

ACS_COMMAND(Terminate)
{
    DENG_UNUSED(acs);
    return Terminate;
}

ACS_COMMAND(Suspend)
{
    acs.info().state = ACScriptInterpreter::Suspended;
    return Stop;
}

ACS_COMMAND(PushNumber)
{
    acs.locals.push(LONG(*acs.pcodePtr++));
    return Continue;
}

ACS_COMMAND(LSpec1)
{
    int special = LONG(*acs.pcodePtr++);
    SpecArgs[0] = acs.locals.pop();
    P_ExecuteLineSpecial(special, SpecArgs, acs.line, acs.side, acs.activator);

    return Continue;
}

ACS_COMMAND(LSpec2)
{
    int special = LONG(*acs.pcodePtr++);
    SpecArgs[1] = acs.locals.pop();
    SpecArgs[0] = acs.locals.pop();
    P_ExecuteLineSpecial(special, SpecArgs, acs.line, acs.side, acs.activator);

    return Continue;
}

ACS_COMMAND(LSpec3)
{
    int special = LONG(*acs.pcodePtr++);
    SpecArgs[2] = acs.locals.pop();
    SpecArgs[1] = acs.locals.pop();
    SpecArgs[0] = acs.locals.pop();
    P_ExecuteLineSpecial(special, SpecArgs, acs.line, acs.side, acs.activator);

    return Continue;
}

ACS_COMMAND(LSpec4)
{
    int special = LONG(*acs.pcodePtr++);
    SpecArgs[3] = acs.locals.pop();
    SpecArgs[2] = acs.locals.pop();
    SpecArgs[1] = acs.locals.pop();
    SpecArgs[0] = acs.locals.pop();
    P_ExecuteLineSpecial(special, SpecArgs, acs.line, acs.side, acs.activator);

    return Continue;
}

ACS_COMMAND(LSpec5)
{
    int special = LONG(*acs.pcodePtr++);
    SpecArgs[4] = acs.locals.pop();
    SpecArgs[3] = acs.locals.pop();
    SpecArgs[2] = acs.locals.pop();
    SpecArgs[1] = acs.locals.pop();
    SpecArgs[0] = acs.locals.pop();
    P_ExecuteLineSpecial(special, SpecArgs, acs.line, acs.side,
                         acs.activator);

    return Continue;
}

ACS_COMMAND(LSpec1Direct)
{
    int special = LONG(*acs.pcodePtr++);
    SpecArgs[0] = LONG(*acs.pcodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, acs.line, acs.side,
                         acs.activator);

    return Continue;
}

ACS_COMMAND(LSpec2Direct)
{
    int special = LONG(*acs.pcodePtr++);
    SpecArgs[0] = LONG(*acs.pcodePtr++);
    SpecArgs[1] = LONG(*acs.pcodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, acs.line, acs.side,
                         acs.activator);

    return Continue;
}

ACS_COMMAND(LSpec3Direct)
{
    int special = LONG(*acs.pcodePtr++);
    SpecArgs[0] = LONG(*acs.pcodePtr++);
    SpecArgs[1] = LONG(*acs.pcodePtr++);
    SpecArgs[2] = LONG(*acs.pcodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, acs.line, acs.side,
                         acs.activator);

    return Continue;
}

ACS_COMMAND(LSpec4Direct)
{
    int special = LONG(*acs.pcodePtr++);
    SpecArgs[0] = LONG(*acs.pcodePtr++);
    SpecArgs[1] = LONG(*acs.pcodePtr++);
    SpecArgs[2] = LONG(*acs.pcodePtr++);
    SpecArgs[3] = LONG(*acs.pcodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, acs.line, acs.side,
                         acs.activator);

    return Continue;
}

ACS_COMMAND(LSpec5Direct)
{
    int special = LONG(*acs.pcodePtr++);
    SpecArgs[0] = LONG(*acs.pcodePtr++);
    SpecArgs[1] = LONG(*acs.pcodePtr++);
    SpecArgs[2] = LONG(*acs.pcodePtr++);
    SpecArgs[3] = LONG(*acs.pcodePtr++);
    SpecArgs[4] = LONG(*acs.pcodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, acs.line, acs.side,
                         acs.activator);

    return Continue;
}

ACS_COMMAND(Add)
{
    acs.locals.push(acs.locals.pop() + acs.locals.pop());
    return Continue;
}

ACS_COMMAND(Subtract)
{
    int operand2 = acs.locals.pop();
    acs.locals.push(acs.locals.pop() - operand2);
    return Continue;
}

ACS_COMMAND(Multiply)
{
    acs.locals.push(acs.locals.pop() * acs.locals.pop());
    return Continue;
}

ACS_COMMAND(Divide)
{
    int operand2 = acs.locals.pop();
    acs.locals.push(acs.locals.pop() / operand2);
    return Continue;
}

ACS_COMMAND(Modulus)
{
    int operand2 = acs.locals.pop();
    acs.locals.push(acs.locals.pop() % operand2);
    return Continue;
}

ACS_COMMAND(EQ)
{
    acs.locals.push(acs.locals.pop() == acs.locals.pop());
    return Continue;
}

ACS_COMMAND(NE)
{
    acs.locals.push(acs.locals.pop() != acs.locals.pop());
    return Continue;
}

ACS_COMMAND(LT)
{
    int operand2 = acs.locals.pop();
    acs.locals.push(acs.locals.pop() < operand2);
    return Continue;
}

ACS_COMMAND(GT)
{
    int operand2 = acs.locals.pop();
    acs.locals.push(acs.locals.pop() > operand2);
    return Continue;
}

ACS_COMMAND(LE)
{
    int operand2 = acs.locals.pop();
    acs.locals.push(acs.locals.pop() <= operand2);
    return Continue;
}

ACS_COMMAND(GE)
{
    int operand2 = acs.locals.pop();
    acs.locals.push(acs.locals.pop() >= operand2);
    return Continue;
}

ACS_COMMAND(AssignScriptVar)
{
    acs.vars[LONG(*acs.pcodePtr++)] = acs.locals.pop();
    return Continue;
}

ACS_COMMAND(AssignMapVar)
{
    acs.interpreter().mapVars[LONG(*acs.pcodePtr++)] = acs.locals.pop();
    return Continue;
}

ACS_COMMAND(AssignWorldVar)
{
    acs.interpreter().worldVars[LONG(*acs.pcodePtr++)] = acs.locals.pop();
    return Continue;
}

ACS_COMMAND(PushScriptVar)
{
    acs.locals.push(acs.vars[LONG(*acs.pcodePtr++)]);
    return Continue;
}

ACS_COMMAND(PushMapVar)
{
    acs.locals.push(acs.interpreter().mapVars[LONG(*acs.pcodePtr++)]);
    return Continue;
}

ACS_COMMAND(PushWorldVar)
{
    acs.locals.push(acs.interpreter().worldVars[LONG(*acs.pcodePtr++)]);
    return Continue;
}

ACS_COMMAND(AddScriptVar)
{
    acs.vars[LONG(*acs.pcodePtr++)] += acs.locals.pop();
    return Continue;
}

ACS_COMMAND(AddMapVar)
{
    acs.interpreter().mapVars[LONG(*acs.pcodePtr++)] += acs.locals.pop();
    return Continue;
}

ACS_COMMAND(AddWorldVar)
{
    acs.interpreter().worldVars[LONG(*acs.pcodePtr++)] += acs.locals.pop();
    return Continue;
}

ACS_COMMAND(SubScriptVar)
{
    acs.vars[LONG(*acs.pcodePtr++)] -= acs.locals.pop();
    return Continue;
}

ACS_COMMAND(SubMapVar)
{
    acs.interpreter().mapVars[LONG(*acs.pcodePtr++)] -= acs.locals.pop();
    return Continue;
}

ACS_COMMAND(SubWorldVar)
{
    acs.interpreter().worldVars[LONG(*acs.pcodePtr++)] -= acs.locals.pop();
    return Continue;
}

ACS_COMMAND(MulScriptVar)
{
    acs.vars[LONG(*acs.pcodePtr++)] *= acs.locals.pop();
    return Continue;
}

ACS_COMMAND(MulMapVar)
{
    acs.interpreter().mapVars[LONG(*acs.pcodePtr++)] *= acs.locals.pop();
    return Continue;
}

ACS_COMMAND(MulWorldVar)
{
    acs.interpreter().worldVars[LONG(*acs.pcodePtr++)] *= acs.locals.pop();
    return Continue;
}

ACS_COMMAND(DivScriptVar)
{
    acs.vars[LONG(*acs.pcodePtr++)] /= acs.locals.pop();
    return Continue;
}

ACS_COMMAND(DivMapVar)
{
    acs.interpreter().mapVars[LONG(*acs.pcodePtr++)] /= acs.locals.pop();
    return Continue;
}

ACS_COMMAND(DivWorldVar)
{
    acs.interpreter().worldVars[LONG(*acs.pcodePtr++)] /= acs.locals.pop();
    return Continue;
}

ACS_COMMAND(ModScriptVar)
{
    acs.vars[LONG(*acs.pcodePtr++)] %= acs.locals.pop();
    return Continue;
}

ACS_COMMAND(ModMapVar)
{
    acs.interpreter().mapVars[LONG(*acs.pcodePtr++)] %= acs.locals.pop();
    return Continue;
}

ACS_COMMAND(ModWorldVar)
{
    acs.interpreter().worldVars[LONG(*acs.pcodePtr++)] %= acs.locals.pop();
    return Continue;
}

ACS_COMMAND(IncScriptVar)
{
    acs.vars[LONG(*acs.pcodePtr++)]++;
    return Continue;
}

ACS_COMMAND(IncMapVar)
{
    acs.interpreter().mapVars[LONG(*acs.pcodePtr++)]++;
    return Continue;
}

ACS_COMMAND(IncWorldVar)
{
    acs.interpreter().worldVars[LONG(*acs.pcodePtr++)]++;
    return Continue;
}

ACS_COMMAND(DecScriptVar)
{
    acs.vars[LONG(*acs.pcodePtr++)]--;
    return Continue;
}

ACS_COMMAND(DecMapVar)
{
    acs.interpreter().mapVars[LONG(*acs.pcodePtr++)]--;
    return Continue;
}

ACS_COMMAND(DecWorldVar)
{
    acs.interpreter().worldVars[LONG(*acs.pcodePtr++)]--;
    return Continue;
}

ACS_COMMAND(Goto)
{
    acs.pcodePtr = (int *) (acs.interpreter().bytecode() + LONG(*acs.pcodePtr));
    return Continue;
}

ACS_COMMAND(IfGoto)
{
    if(acs.locals.pop())
    {
        acs.pcodePtr = (int *) (acs.interpreter().bytecode() + LONG(*acs.pcodePtr));
    }
    else
    {
        acs.pcodePtr++;
    }
    return Continue;
}

ACS_COMMAND(Drop)
{
    acs.locals.drop();
    return Continue;
}

ACS_COMMAND(Delay)
{
    acs.delayCount = acs.locals.pop();
    return Stop;
}

ACS_COMMAND(DelayDirect)
{
    acs.delayCount = LONG(*acs.pcodePtr++);
    return Stop;
}

ACS_COMMAND(Random)
{
    int high = acs.locals.pop();
    int low  = acs.locals.pop();
    acs.locals.push(low + (P_Random() % (high - low + 1)));
    return Continue;
}

ACS_COMMAND(RandomDirect)
{
    int low  = LONG(*acs.pcodePtr++);
    int high = LONG(*acs.pcodePtr++);
    acs.locals.push(low + (P_Random() % (high - low + 1)));
    return Continue;
}

ACS_COMMAND(ThingCount)
{
    int tid  = acs.locals.pop();
    int type = acs.locals.pop();
    // Anything to count?
    if(type + tid)
    {
        acs.locals.push(P_MobjCount(type, tid));
    }
    return Continue;
}

ACS_COMMAND(ThingCountDirect)
{
    int type = LONG(*acs.pcodePtr++);
    int tid  = LONG(*acs.pcodePtr++);
    // Anything to count?
    if(type + tid)
    {
        acs.locals.push(P_MobjCount(type, tid));
    }
    return Continue;
}

ACS_COMMAND(TagWait)
{
    acs.info().waitValue = acs.locals.pop();
    acs.info().state = ACScriptInterpreter::WaitingForTag;
    return Stop;
}

ACS_COMMAND(TagWaitDirect)
{
    acs.info().waitValue = LONG(*acs.pcodePtr++);
    acs.info().state = ACScriptInterpreter::WaitingForTag;
    return Stop;
}

ACS_COMMAND(PolyWait)
{
    acs.info().waitValue = acs.locals.pop();
    acs.info().state = ACScriptInterpreter::WaitingForPolyobj;
    return Stop;
}

ACS_COMMAND(PolyWaitDirect)
{
    acs.info().waitValue = LONG(*acs.pcodePtr++);
    acs.info().state = ACScriptInterpreter::WaitingForPolyobj;
    return Stop;
}

ACS_COMMAND(ChangeFloor)
{
    AutoStr *path = Str_PercentEncode(Str_Copy(AutoStr_New(), acs.interpreter().string(acs.locals.pop())));
    Uri *uri = Uri_NewWithPath3("Flats", Str_Text(path));

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    int tag = acs.locals.pop();

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
    int tag = LONG(*acs.pcodePtr++);

    AutoStr *path = Str_PercentEncode(Str_Copy(AutoStr_New(), acs.interpreter().string(LONG(*acs.pcodePtr++))));
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
    AutoStr *path = Str_PercentEncode(Str_Copy(AutoStr_New(), acs.interpreter().string(acs.locals.pop())));
    Uri *uri = Uri_NewWithPath3("Flats", Str_Text(path));

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    int tag = acs.locals.pop();

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
    int tag = LONG(*acs.pcodePtr++);

    AutoStr *path = Str_PercentEncode(Str_Copy(AutoStr_New(), acs.interpreter().string(LONG(*acs.pcodePtr++))));
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
    acs.pcodePtr = acs.info().pcodePtr;
    return Continue;
}

ACS_COMMAND(AndLogical)
{
    acs.locals.push(acs.locals.pop() && acs.locals.pop());
    return Continue;
}

ACS_COMMAND(OrLogical)
{
    acs.locals.push(acs.locals.pop() || acs.locals.pop());
    return Continue;
}

ACS_COMMAND(AndBitwise)
{
    acs.locals.push(acs.locals.pop() & acs.locals.pop());
    return Continue;
}

ACS_COMMAND(OrBitwise)
{
    acs.locals.push(acs.locals.pop() | acs.locals.pop());
    return Continue;
}

ACS_COMMAND(EorBitwise)
{
    acs.locals.push(acs.locals.pop() ^ acs.locals.pop());
    return Continue;
}

ACS_COMMAND(NegateLogical)
{
    acs.locals.push(!acs.locals.pop());
    return Continue;
}

ACS_COMMAND(LShift)
{
    int operand2 = acs.locals.pop();
    acs.locals.push(acs.locals.pop() << operand2);
    return Continue;
}

ACS_COMMAND(RShift)
{
    int operand2 = acs.locals.pop();
    acs.locals.push(acs.locals.pop() >> operand2);
    return Continue;
}

ACS_COMMAND(UnaryMinus)
{
    acs.locals.push(-acs.locals.pop());
    return Continue;
}

ACS_COMMAND(IfNotGoto)
{
    if(acs.locals.pop())
    {
        acs.pcodePtr++;
    }
    else
    {
        acs.pcodePtr = (int *) (acs.interpreter().bytecode() + LONG(*acs.pcodePtr));
    }
    return Continue;
}

ACS_COMMAND(LineSide)
{
    acs.locals.push(acs.side);
    return Continue;
}

ACS_COMMAND(ScriptWait)
{
    acs.info().waitValue = acs.locals.pop();
    acs.info().state = ACScriptInterpreter::WaitingForScript;
    return Stop;
}

ACS_COMMAND(ScriptWaitDirect)
{
    acs.info().waitValue = LONG(*acs.pcodePtr++);
    acs.info().state = ACScriptInterpreter::WaitingForScript;
    return Stop;
}

ACS_COMMAND(ClearLineSpecial)
{
    if(acs.line)
    {
        P_ToXLine(acs.line)->special = 0;
    }
    return Continue;
}

ACS_COMMAND(CaseGoto)
{
    if(acs.locals.top() == LONG(*acs.pcodePtr++))
    {
        acs.pcodePtr = (int *) (acs.interpreter().bytecode() + LONG(*acs.pcodePtr));
        acs.locals.drop();
    }
    else
    {
        acs.pcodePtr++;
    }
    return Continue;
}

ACS_COMMAND(BeginPrint)
{
    DENG_UNUSED(acs);
    *PrintBuffer = 0;
    return Continue;
}

ACS_COMMAND(EndPrint)
{
    if(acs.activator && acs.activator->player)
    {
        P_SetMessage(acs.activator->player, 0, PrintBuffer);
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
    DENG_UNUSED(acs);
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
    strcat(PrintBuffer, Str_Text(acs.interpreter().string(acs.locals.pop())));
    return Continue;
}

ACS_COMMAND(PrintNumber)
{
    char tempStr[16];
    sprintf(tempStr, "%d", acs.locals.pop());
    strcat(PrintBuffer, tempStr);
    return Continue;
}

ACS_COMMAND(PrintCharacter)
{
    char *bufferEnd = PrintBuffer + strlen(PrintBuffer);
    *bufferEnd++ = acs.locals.pop();
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
    acs.locals.push(count);
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
    acs.locals.push(gametype);

    return Continue;
}

ACS_COMMAND(GameSkill)
{
    acs.locals.push((int)gameSkill);
    return Continue;
}

ACS_COMMAND(Timer)
{
    acs.locals.push(mapTime);
    return Continue;
}

ACS_COMMAND(SectorSound)
{
    mobj_t *mobj = 0;
    if(acs.line)
    {
        Sector *front = (Sector *) P_GetPtrp(acs.line, DMU_FRONT_SECTOR);
        mobj = (mobj_t *) P_GetPtrp(front, DMU_EMITTER);
    }
    int volume = acs.locals.pop();

    S_StartSoundAtVolume(S_GetSoundID(Str_Text(acs.interpreter().string(acs.locals.pop()))), mobj, volume / 127.0f);
    return Continue;
}

ACS_COMMAND(ThingSound)
{
    int volume   = acs.locals.pop();
    int sound    = S_GetSoundID(Str_Text(acs.interpreter().string(acs.locals.pop())));
    int tid      = acs.locals.pop();
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

    int volume = acs.locals.pop();

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

    int sound = S_GetSoundID(Str_Text(acs.interpreter().string(acs.locals.pop())));
    S_StartSoundAtVolume(sound, mobj, volume / 127.0f);

    return Continue;
}

ACS_COMMAND(SoundSequence)
{
    mobj_t *mobj = 0;
    if(acs.line)
    {
        Sector *front = (Sector *) P_GetPtrp(acs.line, DMU_FRONT_SECTOR);
        mobj = (mobj_t *) P_GetPtrp(front, DMU_EMITTER);
    }
    SN_StartSequenceName(mobj, Str_Text(acs.interpreter().string(acs.locals.pop())));

    return Continue;
}

ACS_COMMAND(SetLineTexture)
{
#define TEXTURE_TOP 0
#define TEXTURE_MIDDLE 1
#define TEXTURE_BOTTOM 2

    AutoStr *path = Str_PercentEncode(Str_Copy(AutoStr_New(), acs.interpreter().string(acs.locals.pop())));
    Uri *uri = Uri_NewWithPath3("Textures", Str_Text(path));

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    int position = acs.locals.pop();
    int side     = acs.locals.pop();
    int lineTag  = acs.locals.pop();

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
    int lineFlags = acs.locals.pop()? DDLF_BLOCKING : 0;
    int lineTag   = acs.locals.pop();

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
    int arg5    = acs.locals.pop();
    int arg4    = acs.locals.pop();
    int arg3    = acs.locals.pop();
    int arg2    = acs.locals.pop();
    int arg1    = acs.locals.pop();
    int special = acs.locals.pop();
    int lineTag = acs.locals.pop();

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

ACScriptInterpreter &ACScript::interpreter() const
{
    return Game_ACScriptInterpreter();
}

BytecodeScriptInfo &ACScript::info() const
{
    DENG_ASSERT(_info != 0);
    return *_info;
}

void ACScript::runTick()
{
    ACScriptInterpreter &interp = interpreter();
    if(info().state == ACScriptInterpreter::Terminating)
    {
        interp.scriptFinished(this);
        return;
    }

    if(info().state != ACScriptInterpreter::Running)
    {
        return;
    }

    if(delayCount)
    {
        delayCount--;
        return;
    }

    int action;
    while((action = pcodeCmds[LONG(*pcodePtr++)](*this)) == Continue)
    {}

    if(action == Terminate)
    {
        interp.scriptFinished(this);
    }
}

void ACScript::Stack::push(int value)
{
    if(height == ACS_STACK_DEPTH)
        App_Log(DE2_SCR_ERROR, "ACScript::Stack::push: Overflow");
    values[height++] = value;
}

int ACScript::Stack::pop()
{
    if(height == 0)
        App_Log(DE2_SCR_ERROR, "ACScript::Stack::pop: Underflow");
    return values[--height];
}

int ACScript::Stack::top() const
{
    if(height == 0)
        App_Log(DE2_SCR_ERROR, "ACScript::Stack::top: Underflow");
    return values[height - 1];
}

void ACScript::Stack::drop()
{
    if(height == 0)
        App_Log(DE2_SCR_ERROR, "ACScript::Stack::drop: Underflow");
    height--;
}

void ACScript::write(Writer *writer) const
{
    Writer_WriteByte(writer, 2); // Write a version byte.

    Writer_WriteInt32(writer, SV_ThingArchiveId(activator));
    Writer_WriteInt32(writer, P_ToIndex(line));
    Writer_WriteInt32(writer, side);
    Writer_WriteInt32(writer, info().scriptNumber);
    Writer_WriteInt32(writer, delayCount);
    for(uint i = 0; i < ACS_STACK_DEPTH; ++i)
    {
        Writer_WriteInt32(writer, locals.values[i]);
    }
    Writer_WriteInt32(writer, locals.height);
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
        int ver = Reader_ReadByte(reader); // version byte.

        activator  = INT2PTR(mobj_t, Reader_ReadInt32(reader));
        activator  = SV_GetArchiveThing(PTR2INT(activator), &activator);

        int temp = Reader_ReadInt32(reader);
        if(temp >= 0)
        {
            line   = (Line *)P_ToPtr(DMU_LINE, temp);
            DENG_ASSERT(line != 0);
        }
        else
        {
            line   = 0;
        }

        side       = Reader_ReadInt32(reader);
        _info      = interpreter().scriptInfoPtr(Reader_ReadInt32(reader));
        if(ver < 2)
        {
            /*infoIndex =*/ Reader_ReadInt32(reader);
        }
        delayCount = Reader_ReadInt32(reader);

        for(uint i = 0; i < ACS_STACK_DEPTH; ++i)
        {
            locals.values[i] = Reader_ReadInt32(reader);
        }
        locals.height = Reader_ReadInt32(reader);

        for(uint i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
        {
            vars[i] = Reader_ReadInt32(reader);
        }

        pcodePtr   = (int *) (interpreter().bytecode() + Reader_ReadInt32(reader));
    }
    else
    {
        // Its in the old pre V4 format which serialized acs_t
        // Padding at the start (an old thinker_t struct)
        byte junk[16]; // sizeof thinker_t
        Reader_Read(reader, junk, 16);

        // Start of used data members.
        activator  = INT2PTR(mobj_t, Reader_ReadInt32(reader));
        activator  = SV_GetArchiveThing(PTR2INT(activator), &activator);

        int temp = Reader_ReadInt32(reader);
        if(temp >= 0)
        {
            line   = (Line *)P_ToPtr(DMU_LINE, temp);
            DENG_ASSERT(line != 0);
        }
        else
        {
            line   = 0;
        }

        side       = Reader_ReadInt32(reader);
        _info      = interpreter().scriptInfoPtr(Reader_ReadInt32(reader));
        /*infoIndex  =*/ Reader_ReadInt32(reader);
        delayCount = Reader_ReadInt32(reader);

        for(uint i = 0; i < ACS_STACK_DEPTH; ++i)
        {
            locals.values[i] = Reader_ReadInt32(reader);
        }
        locals.height = Reader_ReadInt32(reader);

        for(uint i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
        {
            vars[i] = Reader_ReadInt32(reader);
        }

        pcodePtr   = (int *) (interpreter().bytecode() + Reader_ReadInt32(reader));
    }

    thinker.function = (thinkfunc_t) ACScript_Thinker;

    return true; // Add this thinker.
}

/// The One ACS interpreter instance.
static ACScriptInterpreter interp;

ACScriptInterpreter &Game_ACScriptInterpreter()
{
    return interp;
}

void Game_InitACScriptsForNewGame()
{
    memset(interp.worldVars, 0, sizeof(interp.worldVars));
    interp.clearDeferredTasks();
}

void Game_ACScriptInterpreter_RunDeferredTasks(Uri const *mapUri)
{
    interp.runDeferredTasks(mapUri);
}

dd_bool Game_ACScriptInterpreter_StartScript(int scriptNumber, Uri const *mapUri,
    byte const args[], mobj_t *activator, Line *line, int side)
{
    return interp.startScript(scriptNumber, mapUri, args, activator, line, side);
}

dd_bool Game_ACScriptInterpreter_TerminateScript(int scriptNumber, Uri const *mapUri)
{
    return interp.terminateScript(scriptNumber, mapUri);
}

dd_bool Game_ACScriptInterpreter_SuspendScript(int scriptNumber, Uri const *mapUri)
{
    return interp.suspendScript(scriptNumber, mapUri);
}

void ACScript_Thinker(ACScript *script)
{
    DENG_ASSERT(script != 0);
    script->runTick();
}

D_CMD(InspectACScript)
{
    DENG_UNUSED(src);

    if(!interp.scriptCount())
    {
        App_Log(DE2_SCR_MSG, "No ACScripts are currently loaded.");
        return true;
    }

    int whichOne = atoi(argv[1]);
    if(!interp.hasScriptEntrypoint(whichOne))
    {
        App_Log(DE2_SCR_WARNING, "Unknown ACScript #%i", whichOne);
        return false;
    }

    App_Log(DE2_SCR_MSG, "%s", Str_Text(interp.scriptDescription(whichOne)));
    return true;
}

D_CMD(ListACScripts)
{
    DENG_UNUSED(src);

    if(!interp.scriptCount())
    {
        App_Log(DE2_SCR_MSG, "No ACScripts are currently loaded.");
        return true;
    }

    App_Log(DE2_SCR_MSG, "Available ACScripts:");
    for(int i = 0; i < interp.scriptCount(); ++i)
    {
        BytecodeScriptInfo &info = interp.scriptInfoByIndex(i);
        App_Log(DE2_SCR_MSG, "%s - args: %i",
                             Str_Text(interp.scriptName(info.scriptNumber)), info.argCount);
    }

    return true;
}
