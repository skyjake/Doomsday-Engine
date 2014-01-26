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
#include <string.h>
#include <stdio.h>

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

struct acsinfo_t
{
    int number;
    int const *address;
    int argCount;
    ACScriptState state;
    int waitValue;
};

/**
 * A deferred task is enqueued when a script is started on map not currently loaded.
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

static void StartOpenACS(int number, int infoIndex, int const *address);
static void ScriptFinished(int number);
static dd_bool TagBusy(int tag);
static dd_bool newDeferredTask(uint map, int number, byte const *args);
static int GetACSIndex(int number);
static void Push(int value);
static int Pop();
static int Top();
static void Drop();

static int CmdNOP();
static int CmdTerminate();
static int CmdSuspend();
static int CmdPushNumber();
static int CmdLSpec1();
static int CmdLSpec2();
static int CmdLSpec3();
static int CmdLSpec4();
static int CmdLSpec5();
static int CmdLSpec1Direct();
static int CmdLSpec2Direct();
static int CmdLSpec3Direct();
static int CmdLSpec4Direct();
static int CmdLSpec5Direct();
static int CmdAdd();
static int CmdSubtract();
static int CmdMultiply();
static int CmdDivide();
static int CmdModulus();
static int CmdEQ();
static int CmdNE();
static int CmdLT();
static int CmdGT();
static int CmdLE();
static int CmdGE();
static int CmdAssignScriptVar();
static int CmdAssignMapVar();
static int CmdAssignWorldVar();
static int CmdPushScriptVar();
static int CmdPushMapVar();
static int CmdPushWorldVar();
static int CmdAddScriptVar();
static int CmdAddMapVar();
static int CmdAddWorldVar();
static int CmdSubScriptVar();
static int CmdSubMapVar();
static int CmdSubWorldVar();
static int CmdMulScriptVar();
static int CmdMulMapVar();
static int CmdMulWorldVar();
static int CmdDivScriptVar();
static int CmdDivMapVar();
static int CmdDivWorldVar();
static int CmdModScriptVar();
static int CmdModMapVar();
static int CmdModWorldVar();
static int CmdIncScriptVar();
static int CmdIncMapVar();
static int CmdIncWorldVar();
static int CmdDecScriptVar();
static int CmdDecMapVar();
static int CmdDecWorldVar();
static int CmdGoto();
static int CmdIfGoto();
static int CmdDrop();
static int CmdDelay();
static int CmdDelayDirect();
static int CmdRandom();
static int CmdRandomDirect();
static int CmdThingCount();
static int CmdThingCountDirect();
static int CmdTagWait();
static int CmdTagWaitDirect();
static int CmdPolyWait();
static int CmdPolyWaitDirect();
static int CmdChangeFloor();
static int CmdChangeFloorDirect();
static int CmdChangeCeiling();
static int CmdChangeCeilingDirect();
static int CmdRestart();
static int CmdAndLogical();
static int CmdOrLogical();
static int CmdAndBitwise();
static int CmdOrBitwise();
static int CmdEorBitwise();
static int CmdNegateLogical();
static int CmdLShift();
static int CmdRShift();
static int CmdUnaryMinus();
static int CmdIfNotGoto();
static int CmdLineSide();
static int CmdScriptWait();
static int CmdScriptWaitDirect();
static int CmdClearLineSpecial();
static int CmdCaseGoto();
static int CmdBeginPrint();
static int CmdEndPrint();
static int CmdPrintString();
static int CmdPrintNumber();
static int CmdPrintCharacter();
static int CmdPlayerCount();
static int CmdGameType();
static int CmdGameSkill();
static int CmdTimer();
static int CmdSectorSound();
static int CmdAmbientSound();
static int CmdSoundSequence();
static int CmdSetLineTexture();
static int CmdSetLineBlocking();
static int CmdSetLineSpecial();
static int CmdThingSound();
static int CmdEndPrintBold();

static void ThingCount(int type, int tid);

static int ACScriptCount;
static byte const *ActionCodeBase;
static acsinfo_t *ACSInfo;
static int MapVars[MAX_ACS_MAP_VARS];
static int WorldVars[MAX_ACS_WORLD_VARS];

static int const *PCodePtr;
static byte SpecArgs[8];
static int ACStringCount;
static char const **ACStrings;
static char PrintBuffer[PRINT_BUFFER_SIZE];

static ACScript *activeScript;
static ACScript *newScript;

static char ErrorMsg[128];

static int (*PCodeCmds[]) () =
{
    CmdNOP, CmdTerminate, CmdSuspend, CmdPushNumber, CmdLSpec1, CmdLSpec2,
    CmdLSpec3, CmdLSpec4, CmdLSpec5, CmdLSpec1Direct, CmdLSpec2Direct,
    CmdLSpec3Direct, CmdLSpec4Direct, CmdLSpec5Direct, CmdAdd,
    CmdSubtract, CmdMultiply, CmdDivide, CmdModulus, CmdEQ, CmdNE,
    CmdLT, CmdGT, CmdLE, CmdGE, CmdAssignScriptVar, CmdAssignMapVar,
    CmdAssignWorldVar, CmdPushScriptVar, CmdPushMapVar,
    CmdPushWorldVar, CmdAddScriptVar, CmdAddMapVar, CmdAddWorldVar,
    CmdSubScriptVar, CmdSubMapVar, CmdSubWorldVar, CmdMulScriptVar,
    CmdMulMapVar, CmdMulWorldVar, CmdDivScriptVar, CmdDivMapVar,
    CmdDivWorldVar, CmdModScriptVar, CmdModMapVar, CmdModWorldVar,
    CmdIncScriptVar, CmdIncMapVar, CmdIncWorldVar, CmdDecScriptVar,
    CmdDecMapVar, CmdDecWorldVar, CmdGoto, CmdIfGoto, CmdDrop,
    CmdDelay, CmdDelayDirect, CmdRandom, CmdRandomDirect,
    CmdThingCount, CmdThingCountDirect, CmdTagWait, CmdTagWaitDirect,
    CmdPolyWait, CmdPolyWaitDirect, CmdChangeFloor,
    CmdChangeFloorDirect, CmdChangeCeiling, CmdChangeCeilingDirect,
    CmdRestart, CmdAndLogical, CmdOrLogical, CmdAndBitwise,
    CmdOrBitwise, CmdEorBitwise, CmdNegateLogical, CmdLShift,
    CmdRShift, CmdUnaryMinus, CmdIfNotGoto, CmdLineSide, CmdScriptWait,
    CmdScriptWaitDirect, CmdClearLineSpecial, CmdCaseGoto,
    CmdBeginPrint, CmdEndPrint, CmdPrintString, CmdPrintNumber,
    CmdPrintCharacter, CmdPlayerCount, CmdGameType, CmdGameSkill,
    CmdTimer, CmdSectorSound, CmdAmbientSound, CmdSoundSequence,
    CmdSetLineTexture, CmdSetLineBlocking, CmdSetLineSpecial,
    CmdThingSound, CmdEndPrintBold
};

char const *GetACString(int id)
{
    if(id < 0 || id > ACStringCount)
        return 0;

    return ACStrings[id];
}

void P_LoadACScripts(int lump)
{
    size_t const lumpLength = (lump >= 0? W_LumpLength(lump) : 0);

    ACScriptCount = 0;

    App_Log(DE2_SCR_VERBOSE, "Loading ACS bytecode lump %s:%s (#%i)...",
            F_PrettyPath(Str_Text(W_LumpSourceFile(lump))),
            Str_Text(W_LumpName(lump)), lump);

    int const *buffer = 0;
    if(lumpLength >= sizeof(BytecodeHeader))
    {
        void *region = Z_Malloc(lumpLength, PU_MAP, 0);
        W_ReadLump(lump, (uint8_t *)region);
        BytecodeHeader const *header = (BytecodeHeader const *) region;
        ActionCodeBase = (byte const *) header;

        if(LONG(header->infoOffset) < (int)lumpLength)
        {
            buffer = (int *) ((byte const *) header + LONG(header->infoOffset));
            ACScriptCount = LONG(*buffer++);
        }
    }

    if(ACScriptCount == 0 || IS_CLIENT)
    {
        // Empty/Invalid lump.
        App_Log(DE2_SCR_WARNING, "Lump #%i does not appear to be valid ACS bytecode data", lump);
        return;
    }

    ACSInfo = (acsinfo_t *) Z_Malloc(ACScriptCount * sizeof(acsinfo_t), PU_MAP, 0);
    memset(ACSInfo, 0, ACScriptCount * sizeof(acsinfo_t));

    acsinfo_t *info = ACSInfo;
    for(int i = 0; i < ACScriptCount; ++i, info++)
    {
        info->number   = LONG(*buffer++);
        info->address  = (int const *)(ActionCodeBase + LONG(*buffer++));
        info->argCount = LONG(*buffer++);
        if(info->number >= OPEN_SCRIPTS_BASE)
        {                       // Auto-activate
            info->number -= OPEN_SCRIPTS_BASE;
            StartOpenACS(info->number, i, info->address);
            info->state = Running;
        }
        else
        {
            info->state = Inactive;
        }
    }

    ACStringCount = LONG(*buffer++);
    ACStrings = (char const **) Z_Malloc(ACStringCount * sizeof(char *), PU_MAP, 0);
    for(int i = 0; i < ACStringCount; ++i)
    {
        ACStrings[i] = (char const *)(ActionCodeBase + LONG(*buffer++));
    }

    memset(MapVars, 0, sizeof(MapVars));
}

static void StartOpenACS(int number, int infoIndex, int const *address)
{
    ACScript *script = (ACScript *) Z_Calloc(sizeof(*script), PU_MAP, 0);
    script->number = number;

    // World objects are allotted 1 second for initialization
    script->delayCount = TICRATE;

    script->infoIndex = infoIndex;
    script->ip        = address;
    script->thinker.function = (thinkfunc_t) ACScript_Thinker;
    Thinker_Add(&script->thinker);
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

dd_bool P_StartACScript(int number, uint map, byte *args, mobj_t *activator, Line *line,
    int side)
{
    DENG_ASSERT(!IS_CLIENT);

    newScript = NULL;
    if(map && map-1 != gameMap)
    {
        // Script is not for the current map.
        // Add it to the store to be started when that map is next entered.
        return newDeferredTask(map-1, number, args);
    }

    int infoIndex = GetACSIndex(number);
    if(infoIndex == -1)
    {
        // Script not found.
        sprintf(ErrorMsg, "P_STARTACS ERROR: UNKNOWN SCRIPT %d", number);
        P_SetMessage(&players[CONSOLEPLAYER], LMF_NO_HIDE, ErrorMsg);
        return false;
    }

    ACScriptState *statePtr = &ACSInfo[infoIndex].state;
    if(*statePtr == Suspended)
    {
        // Resume a suspended script.
        *statePtr = Running;
        return true;
    }

    if(*statePtr != Inactive)
    {
        // Script is already executing.
        return false;
    }

    ACScript *script = (ACScript *) Z_Calloc(sizeof(*script), PU_MAP, 0);

    script->number    = number;
    script->infoIndex = infoIndex;
    script->activator = activator;
    script->line      = line;
    script->side      = side;
    script->ip        = ACSInfo[infoIndex].address;
    script->thinker.function = (thinkfunc_t) ACScript_Thinker;
    for(int i = 0; i < ACSInfo[infoIndex].argCount; ++i)
    {
        script->vars[i] = args[i];
    }

    *statePtr = Running;
    Thinker_Add(&script->thinker);
    newScript = script;
    return true;
}

static dd_bool newDeferredTask(uint map, int number, byte const *args)
{
    if(deferredTasksSize)
    {
        // Don't allow duplicates.
        for(int i = 0; i < deferredTasksSize; ++i)
        {
            DeferredTask &task = deferredTasks[i];
            if(task.script == number && task.map == map)
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
    store->script  = number;
    store->args[0] = args[0];
    store->args[1] = args[1];
    store->args[2] = args[2];
    store->args[3] = args[3];

    return true;
}

dd_bool P_TerminateACScript(int number, uint /*map*/)
{
    int infoIndex = GetACSIndex(number);
    if(infoIndex >= 0)
    {
        acsinfo_t &info = ACSInfo[infoIndex];
        // Some states disallow termination.
        if(info.state != Inactive && info.state != Terminating)
        {
            ACSInfo[infoIndex].state = Terminating;
            return true;
        }
    }
    return false;
}

dd_bool P_SuspendACScript(int number, uint map)
{
    int infoIndex = GetACSIndex(number);
    if(infoIndex >= 0)
    {
        acsinfo_t &info = ACSInfo[infoIndex];
        // Some states disallow suspension.
        if(info.state != Inactive && info.state != Suspended &&
           info.state != Terminating)
        {
            ACSInfo[infoIndex].state = Suspended;
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

void ACScript_Thinker(ACScript *script)
{
    DENG_ASSERT(script != 0);

    if(ACSInfo[script->infoIndex].state == Terminating)
    {
        ACSInfo[script->infoIndex].state = Inactive;
        ScriptFinished(activeScript->number);
        Thinker_Remove(&activeScript->thinker);
        return;
    }

    if(ACSInfo[script->infoIndex].state != Running)
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
        ACSInfo[script->infoIndex].state = Inactive;
        ScriptFinished(activeScript->number);
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
    SV_WriteLong(((byte const *)th->ip) - ActionCodeBase);
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

        th->ip              = (int *) (ActionCodeBase + SV_ReadLong());
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

        th->ip              = (int *) (ActionCodeBase + SV_ReadLong());
    }

    th->thinker.function = (thinkfunc_t) ACScript_Thinker;

    return true; // Add this thinker.
}

void P_ACScriptTagFinished(int tag)
{
    if(TagBusy(tag)) return;

    for(int i = 0; i < ACScriptCount; ++i)
    {
        acsinfo_t &info = ACSInfo[i];
        if(info.state == WaitingForTag && info.waitValue == tag)
        {
            info.state = Running;
        }
    }
}

void P_ACScriptPolyobjFinished(int tag)
{
    if(PO_Busy(tag)) return;

    for(int i = 0; i < ACScriptCount; ++i)
    {
        acsinfo_t &info = ACSInfo[i];
        if(info.state == WaitingForPolyobj && info.waitValue == tag)
        {
            ACSInfo[i].state = Running;
        }
    }
}

static void ScriptFinished(int number)
{
    for(int i = 0; i < ACScriptCount; ++i)
    {
        acsinfo_t &info = ACSInfo[i];
        if(info.state == WaitingForScript && info.waitValue == number)
        {
            info.state = Running;
        }
    }
}

static dd_bool TagBusy(int tag)
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

/**
 * @return  The index of a script number else @c -1,.
 */
static int GetACSIndex(int number)
{
    for(int i = 0; i < ACScriptCount; ++i)
    {
        acsinfo_t &info = ACSInfo[i];
        if(info.number == number)
        {
            return i;
        }
    }
    return -1;
}

static void Push(int value)
{
    activeScript->stack[activeScript->stackPtr++] = value;
}

static int Pop()
{
    return activeScript->stack[--activeScript->stackPtr];
}

static int Top()
{
    return activeScript->stack[activeScript->stackPtr - 1];
}

static void Drop()
{
    activeScript->stackPtr--;
}

static int CmdNOP()
{
    return SCRIPT_CONTINUE;
}

static int CmdTerminate()
{
    return SCRIPT_TERMINATE;
}

static int CmdSuspend()
{
    ACSInfo[activeScript->infoIndex].state = Suspended;
    return SCRIPT_STOP;
}

static int CmdPushNumber()
{
    Push(LONG(*PCodePtr++));
    return SCRIPT_CONTINUE;
}

static int CmdLSpec1()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec2()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec3()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[2] = Pop();
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec4()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[3] = Pop();
    SpecArgs[2] = Pop();
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec5()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[4] = Pop();
    SpecArgs[3] = Pop();
    SpecArgs[2] = Pop();
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec1Direct()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec2Direct()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec3Direct()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    SpecArgs[2] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, activeScript->line, activeScript->side,
                         activeScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec4Direct()
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

static int CmdLSpec5Direct()
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

static int CmdAdd()
{
    Push(Pop() + Pop());
    return SCRIPT_CONTINUE;
}

static int CmdSubtract()
{
    int operand2 = Pop();
    Push(Pop() - operand2);
    return SCRIPT_CONTINUE;
}

static int CmdMultiply()
{
    Push(Pop() * Pop());
    return SCRIPT_CONTINUE;
}

static int CmdDivide()
{
    int operand2 = Pop();
    Push(Pop() / operand2);
    return SCRIPT_CONTINUE;
}

static int CmdModulus()
{
    int operand2 = Pop();
    Push(Pop() % operand2);
    return SCRIPT_CONTINUE;
}

static int CmdEQ()
{
    Push(Pop() == Pop());
    return SCRIPT_CONTINUE;
}

static int CmdNE()
{
    Push(Pop() != Pop());
    return SCRIPT_CONTINUE;
}

static int CmdLT()
{
    int operand2 = Pop();
    Push(Pop() < operand2);
    return SCRIPT_CONTINUE;
}

static int CmdGT()
{
    int operand2 = Pop();
    Push(Pop() > operand2);
    return SCRIPT_CONTINUE;
}

static int CmdLE()
{
    int operand2 = Pop();
    Push(Pop() <= operand2);
    return SCRIPT_CONTINUE;
}

static int CmdGE()
{
    int operand2 = Pop();
    Push(Pop() >= operand2);
    return SCRIPT_CONTINUE;
}

static int CmdAssignScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)] = Pop();
    return SCRIPT_CONTINUE;
}

static int CmdAssignMapVar()
{
    MapVars[LONG(*PCodePtr++)] = Pop();
    return SCRIPT_CONTINUE;
}

static int CmdAssignWorldVar()
{
    WorldVars[LONG(*PCodePtr++)] = Pop();
    return SCRIPT_CONTINUE;
}

static int CmdPushScriptVar()
{
    Push(activeScript->vars[LONG(*PCodePtr++)]);
    return SCRIPT_CONTINUE;
}

static int CmdPushMapVar()
{
    Push(MapVars[LONG(*PCodePtr++)]);
    return SCRIPT_CONTINUE;
}

static int CmdPushWorldVar()
{
    Push(WorldVars[LONG(*PCodePtr++)]);
    return SCRIPT_CONTINUE;
}

static int CmdAddScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)] += Pop();
    return SCRIPT_CONTINUE;
}

static int CmdAddMapVar()
{
    MapVars[LONG(*PCodePtr++)] += Pop();
    return SCRIPT_CONTINUE;
}

static int CmdAddWorldVar()
{
    WorldVars[LONG(*PCodePtr++)] += Pop();
    return SCRIPT_CONTINUE;
}

static int CmdSubScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)] -= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdSubMapVar()
{
    MapVars[LONG(*PCodePtr++)] -= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdSubWorldVar()
{
    WorldVars[LONG(*PCodePtr++)] -= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdMulScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)] *= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdMulMapVar()
{
    MapVars[LONG(*PCodePtr++)] *= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdMulWorldVar()
{
    WorldVars[LONG(*PCodePtr++)] *= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdDivScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)] /= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdDivMapVar()
{
    MapVars[LONG(*PCodePtr++)] /= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdDivWorldVar()
{
    WorldVars[LONG(*PCodePtr++)] /= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdModScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)] %= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdModMapVar()
{
    MapVars[LONG(*PCodePtr++)] %= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdModWorldVar()
{
    WorldVars[LONG(*PCodePtr++)] %= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdIncScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)]++;
    return SCRIPT_CONTINUE;
}

static int CmdIncMapVar()
{
    MapVars[LONG(*PCodePtr++)]++;
    return SCRIPT_CONTINUE;
}

static int CmdIncWorldVar()
{
    WorldVars[LONG(*PCodePtr++)]++;
    return SCRIPT_CONTINUE;
}

static int CmdDecScriptVar()
{
    activeScript->vars[LONG(*PCodePtr++)]--;
    return SCRIPT_CONTINUE;
}

static int CmdDecMapVar()
{
    MapVars[LONG(*PCodePtr++)]--;
    return SCRIPT_CONTINUE;
}

static int CmdDecWorldVar()
{
    WorldVars[LONG(*PCodePtr++)]--;
    return SCRIPT_CONTINUE;
}

static int CmdGoto()
{
    PCodePtr = (int *) (ActionCodeBase + LONG(*PCodePtr));
    return SCRIPT_CONTINUE;
}

static int CmdIfGoto()
{
    if(Pop())
    {
        PCodePtr = (int *) (ActionCodeBase + LONG(*PCodePtr));
    }
    else
    {
        PCodePtr++;
    }
    return SCRIPT_CONTINUE;
}

static int CmdDrop()
{
    Drop();
    return SCRIPT_CONTINUE;
}

static int CmdDelay()
{
    activeScript->delayCount = Pop();
    return SCRIPT_STOP;
}

static int CmdDelayDirect()
{
    activeScript->delayCount = LONG(*PCodePtr++);
    return SCRIPT_STOP;
}

static int CmdRandom()
{
    int high = Pop();
    int low  = Pop();
    Push(low + (P_Random() % (high - low + 1)));
    return SCRIPT_CONTINUE;
}

static int CmdRandomDirect()
{
    int low  = LONG(*PCodePtr++);
    int high = LONG(*PCodePtr++);
    Push(low + (P_Random() % (high - low + 1)));
    return SCRIPT_CONTINUE;
}

static int CmdThingCount()
{
    int tid = Pop();
    ThingCount(Pop(), tid);
    return SCRIPT_CONTINUE;
}

static int CmdThingCountDirect()
{
    int type = LONG(*PCodePtr++);
    ThingCount(type, LONG(*PCodePtr++));
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

static void ThingCount(int type, int tid)
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

    Push(count);
}

static int CmdTagWait()
{
    ACSInfo[activeScript->infoIndex].waitValue = Pop();
    ACSInfo[activeScript->infoIndex].state = WaitingForTag;
    return SCRIPT_STOP;
}

static int CmdTagWaitDirect()
{
    ACSInfo[activeScript->infoIndex].waitValue = LONG(*PCodePtr++);
    ACSInfo[activeScript->infoIndex].state = WaitingForTag;
    return SCRIPT_STOP;
}

static int CmdPolyWait()
{
    ACSInfo[activeScript->infoIndex].waitValue = Pop();
    ACSInfo[activeScript->infoIndex].state = WaitingForPolyobj;
    return SCRIPT_STOP;
}

static int CmdPolyWaitDirect()
{
    ACSInfo[activeScript->infoIndex].waitValue = LONG(*PCodePtr++);
    ACSInfo[activeScript->infoIndex].state = WaitingForPolyobj;
    return SCRIPT_STOP;
}

static int CmdChangeFloor()
{
    ddstring_t path; Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, GetACString(Pop())));
    Uri *uri = Uri_NewWithPath2("Flats:", RC_NULL);
    Uri_SetPath(uri, Str_Text(&path));
    Str_Free(&path);

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    int tag = Pop();

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

static int CmdChangeFloorDirect()
{
    int tag = LONG(*PCodePtr++);

    ddstring_t path; Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, GetACString(LONG(*PCodePtr++))));

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

static int CmdChangeCeiling()
{
    ddstring_t path; Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, GetACString(Pop())));

    Uri *uri = Uri_NewWithPath2("Flats:", RC_NULL);
    Uri_SetPath(uri, Str_Text(&path));
    Str_Free(&path);

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    int tag = Pop();

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

static int CmdChangeCeilingDirect()
{
    int tag = LONG(*PCodePtr++);

    ddstring_t path; Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, GetACString(LONG(*PCodePtr++))));

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

static int CmdRestart()
{
    PCodePtr = ACSInfo[activeScript->infoIndex].address;
    return SCRIPT_CONTINUE;
}

static int CmdAndLogical()
{
    Push(Pop() && Pop());
    return SCRIPT_CONTINUE;
}

static int CmdOrLogical()
{
    Push(Pop() || Pop());
    return SCRIPT_CONTINUE;
}

static int CmdAndBitwise()
{
    Push(Pop() & Pop());
    return SCRIPT_CONTINUE;
}

static int CmdOrBitwise()
{
    Push(Pop() | Pop());
    return SCRIPT_CONTINUE;
}

static int CmdEorBitwise()
{
    Push(Pop() ^ Pop());
    return SCRIPT_CONTINUE;
}

static int CmdNegateLogical()
{
    Push(!Pop());
    return SCRIPT_CONTINUE;
}

static int CmdLShift()
{
    int operand2 = Pop();
    Push(Pop() << operand2);
    return SCRIPT_CONTINUE;
}

static int CmdRShift()
{
    int operand2 = Pop();
    Push(Pop() >> operand2);
    return SCRIPT_CONTINUE;
}

static int CmdUnaryMinus()
{
    Push(-Pop());
    return SCRIPT_CONTINUE;
}

static int CmdIfNotGoto()
{
    if(Pop())
    {
        PCodePtr++;
    }
    else
    {
        PCodePtr = (int *) (ActionCodeBase + LONG(*PCodePtr));
    }
    return SCRIPT_CONTINUE;
}

static int CmdLineSide()
{
    Push(activeScript->side);
    return SCRIPT_CONTINUE;
}

static int CmdScriptWait()
{
    ACSInfo[activeScript->infoIndex].waitValue = Pop();
    ACSInfo[activeScript->infoIndex].state = WaitingForScript;
    return SCRIPT_STOP;
}

static int CmdScriptWaitDirect()
{
    ACSInfo[activeScript->infoIndex].waitValue = LONG(*PCodePtr++);
    ACSInfo[activeScript->infoIndex].state = WaitingForScript;
    return SCRIPT_STOP;
}

static int CmdClearLineSpecial()
{
    if(activeScript->line)
    {
        P_ToXLine(activeScript->line)->special = 0;
    }
    return SCRIPT_CONTINUE;
}

static int CmdCaseGoto()
{
    if(Top() == LONG(*PCodePtr++))
    {
        PCodePtr = (int *) (ActionCodeBase + LONG(*PCodePtr));
        Drop();
    }
    else
    {
        PCodePtr++;
    }
    return SCRIPT_CONTINUE;
}

static int CmdBeginPrint()
{
    *PrintBuffer = 0;
    return SCRIPT_CONTINUE;
}

static int CmdEndPrint()
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

static int CmdEndPrintBold()
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

static int CmdPrintString()
{
    strcat(PrintBuffer, GetACString(Pop()));
    return SCRIPT_CONTINUE;
}

static int CmdPrintNumber()
{
    char tempStr[16];
    sprintf(tempStr, "%d", Pop());
    strcat(PrintBuffer, tempStr);
    return SCRIPT_CONTINUE;
}

static int CmdPrintCharacter()
{
    char *bufferEnd = PrintBuffer + strlen(PrintBuffer);
    *bufferEnd++ = Pop();
    *bufferEnd = 0;
    return SCRIPT_CONTINUE;
}

static int CmdPlayerCount()
{
    int count = 0;
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        count += players[i].plr->inGame;
    }
    Push(count);
    return SCRIPT_CONTINUE;
}

static int CmdGameType()
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
    Push(gametype);

    return SCRIPT_CONTINUE;
}

static int CmdGameSkill()
{
    Push((int)gameSkill);
    return SCRIPT_CONTINUE;
}

static int CmdTimer()
{
    Push(mapTime);
    return SCRIPT_CONTINUE;
}

static int CmdSectorSound()
{
    mobj_t *mobj = 0;
    if(activeScript->line)
    {
        Sector *front = (Sector *) P_GetPtrp(activeScript->line, DMU_FRONT_SECTOR);
        mobj = (mobj_t *) P_GetPtrp(front, DMU_EMITTER);
    }
    int volume = Pop();

    S_StartSoundAtVolume(S_GetSoundID(GetACString(Pop())), mobj, volume / 127.0f);
    return SCRIPT_CONTINUE;
}

static int CmdThingSound()
{
    int volume   = Pop();
    int sound    = S_GetSoundID(GetACString(Pop()));
    int tid      = Pop();
    int searcher = -1;

    mobj_t *mobj;
    while(sound && (mobj = P_FindMobjFromTID(tid, &searcher)))
    {
        S_StartSoundAtVolume(sound, mobj, volume / 127.0f);
    }

    return SCRIPT_CONTINUE;
}

static int CmdAmbientSound()
{
    mobj_t *mobj = 0; // For 3D positioning.
    mobj_t *plrmo = players[DISPLAYPLAYER].plr->mo;

    int volume = Pop();

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

    int sound = S_GetSoundID(GetACString(Pop()));
    S_StartSoundAtVolume(sound, mobj, volume / 127.0f);

    return SCRIPT_CONTINUE;
}

static int CmdSoundSequence()
{
    mobj_t *mobj = 0;
    if(activeScript->line)
    {
        Sector *front = (Sector *) P_GetPtrp(activeScript->line, DMU_FRONT_SECTOR);
        mobj = (mobj_t *) P_GetPtrp(front, DMU_EMITTER);
    }
    SN_StartSequenceName(mobj, GetACString(Pop()));

    return SCRIPT_CONTINUE;
}

static int CmdSetLineTexture()
{
    ddstring_t path; Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, GetACString(Pop())));

    Uri *uri = Uri_NewWithPath2("Textures:", RC_NULL);
    Uri_SetPath(uri, Str_Text(&path));
    Str_Free(&path);

    Material *mat = (Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    Uri_Delete(uri);

    int position = Pop();
    int side     = Pop();
    int lineTag  = Pop();

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

static int CmdSetLineBlocking()
{
    dd_bool blocking = Pop()? DDLF_BLOCKING : 0;
    int lineTag      = Pop();

    if(iterlist_t *list = P_GetLineIterListForTag(lineTag, false))
    {
        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);

        Line *line;
        while((line = (Line *) IterList_MoveIterator(list)))
        {
            P_SetIntp(line, DMU_FLAGS, (P_GetIntp(line, DMU_FLAGS) & ~DDLF_BLOCKING) | blocking);
        }
    }

    return SCRIPT_CONTINUE;
}

static int CmdSetLineSpecial()
{
    int arg5    = Pop();
    int arg4    = Pop();
    int arg3    = Pop();
    int arg2    = Pop();
    int arg1    = Pop();
    int special = Pop();
    int lineTag = Pop();

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
        DeferredTask const *store = &deferredTasks[i];
        SV_WriteLong(store->map);
        SV_WriteLong(store->script);
        for(int k = 0; k < 4; ++k)
        {
            SV_WriteByte(store->args[k]);
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

    for(int i = 0; i < ACScriptCount; ++i)
    {
        SV_WriteShort(ACSInfo[i].state);
        SV_WriteShort(ACSInfo[i].waitValue);
    }

    for(int i = 0; i < MAX_ACS_MAP_VARS; ++i)
    {
        SV_WriteLong(MapVars[i]);
    }
}

void P_ReadMapACScriptData()
{
    SV_AssertSegment(ASEG_SCRIPTS);

    for(int i = 0; i < ACScriptCount; ++i)
    {
        ACSInfo[i].state     = (ACScriptState) SV_ReadShort();
        ACSInfo[i].waitValue = SV_ReadShort();
    }

    for(int i = 0; i < MAX_ACS_MAP_VARS; ++i)
    {
        MapVars[i] = SV_ReadLong();
    }
}

D_CMD(ScriptInfo)
{
    char const *scriptStates[] = {
        "Inactive", "Running", "Suspended", "Waiting for tag",
        "Waiting for poly", "Waiting for script", "Terminating"
    };

    int whichOne = argc == 2? atoi(argv[1]) : -1;

    for(int i = 0; i < ACScriptCount; ++i)
    {
        acsinfo_t *aptr = ACSInfo + i;

        if(whichOne != -1 && whichOne != aptr->number)
            continue;

        App_Log(DE2_SCR_MSG, "%d %s (a: %d, w: %d)", aptr->number,
                scriptStates[aptr->state], aptr->argCount, aptr->waitValue);
    }

    return true;
}
