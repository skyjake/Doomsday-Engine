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

#define S_DROP ACScript->stackPtr--
#define S_POP ACScript->stack[--ACScript->stackPtr]
#define S_PUSH(x) ACScript->stack[ACScript->stackPtr++] = x

typedef enum aste_e {
    ASTE_INACTIVE,
    ASTE_RUNNING,
    ASTE_SUSPENDED,
    ASTE_WAITING_FOR_TAG,
    ASTE_WAITING_FOR_POLY,
    ASTE_WAITING_FOR_SCRIPT,
    ASTE_TERMINATING
} aste_t;

typedef struct acsinfo_s {
    int number;
    int const *address;
    int argCount;
    aste_t state;
    int waitValue;
} acsinfo_t;

typedef struct acsstore_s {
    uint map;     ///< Target map.
    int script;   ///< Script number on target map.
    byte args[4]; ///< Padded to 4 for alignment.
} acsstore_t;

/**
 * Bytecode header. Read directly from the map lump.
 */
#pragma pack(1)
typedef struct acsheader_s {
    int32_t marker;
    int32_t infoOffset;
    int32_t code;
} acsheader_t;
#pragma pack()

static void StartOpenACS(int number, int infoIndex, int const *address);
static void ScriptFinished(int number);
static dd_bool TagBusy(int tag);
static dd_bool AddToACSStore(uint map, int number, byte const *args);
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
static int ACSStoreSize;
static acsstore_t *ACSStore;

static acs_t *ACScript;
static int const *PCodePtr;
static byte SpecArgs[8];
static int ACStringCount;
static char const **ACStrings;
static char PrintBuffer[PRINT_BUFFER_SIZE];
static acs_t *NewScript;

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
    if(lumpLength >= sizeof(acsheader_t))
    {
        void *region = Z_Malloc(lumpLength, PU_MAP, 0);
        W_ReadLump(lump, (uint8_t *)region);
        acsheader_t const *header = (acsheader_t const *) region;
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
            info->state = ASTE_RUNNING;
        }
        else
        {
            info->state = ASTE_INACTIVE;
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
    acs_t *script = (acs_t *) Z_Calloc(sizeof(*script), PU_MAP, 0);
    script->number = number;

    // World objects are allotted 1 second for initialization
    script->delayCount = TICRATE;

    script->infoIndex = infoIndex;
    script->ip        = address;
    script->thinker.function = (thinkfunc_t) ACScript_Thinker;
    Thinker_Add(&script->thinker);
}

void P_CheckACSStore(uint map)
{
    int const origSize = ACSStoreSize;

    int i = 0;
    while(i < ACSStoreSize)
    {
        acsstore_t *store = &ACSStore[i];

        if(store->map != map)
        {
            i++;
            continue;
        }

        P_StartACS(store->script, 0, store->args, NULL, NULL, 0);
        if(NewScript)
        {
            NewScript->delayCount = TICRATE;
        }

        ACSStoreSize -= 1;
        if(i == ACSStoreSize)
            break;

        memmove(&ACSStore[i], &ACSStore[i + 1], sizeof(acsstore_t) * (ACSStoreSize - i));
    }

    if(ACSStoreSize == origSize)
        return;

    if(ACSStoreSize)
    {
        ACSStore = (acsstore_t *) Z_Realloc(ACSStore, sizeof(acsstore_t) * ACSStoreSize, PU_GAMESTATIC);
        return;
    }

    Z_Free(ACSStore);
    ACSStore = 0;
}

dd_bool P_StartACS(int number, uint map, byte *args, mobj_t *activator, Line *line,
    int side)
{
    DENG_ASSERT(!IS_CLIENT);

    NewScript = NULL;
    if(map && map-1 != gameMap)
    {
        // Script is not for the current map.
        // Add it to the store to be started when that map is next entered.
        return AddToACSStore(map-1, number, args);
    }

    int infoIndex = GetACSIndex(number);
    if(infoIndex == -1)
    {
        // Script not found.
        sprintf(ErrorMsg, "P_STARTACS ERROR: UNKNOWN SCRIPT %d", number);
        P_SetMessage(&players[CONSOLEPLAYER], LMF_NO_HIDE, ErrorMsg);
        return false;
    }

    aste_t *statePtr = &ACSInfo[infoIndex].state;
    if(*statePtr == ASTE_SUSPENDED)
    {
        // Resume a suspended script.
        *statePtr = ASTE_RUNNING;
        return true;
    }

    if(*statePtr != ASTE_INACTIVE)
    {
        // Script is already executing.
        return false;
    }

    acs_t *script = (acs_t *) Z_Calloc(sizeof(*script), PU_MAP, 0);

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

    *statePtr = ASTE_RUNNING;
    Thinker_Add(&script->thinker);
    NewScript = script;
    return true;
}

static dd_bool AddToACSStore(uint map, int number, byte const *args)
{
    if(ACSStoreSize)
    {
        // Don't allow duplicates.
        for(int i = 0; i < ACSStoreSize; ++i)
        {
            acsstore_t &store = ACSStore[i];
            if(store.script == number && store.map == map)
            {
                return false;
            }
        }

        ACSStore = (acsstore_t *) Z_Realloc(ACSStore, ++ACSStoreSize * sizeof(acsstore_t), PU_GAMESTATIC);
    }
    else
    {
        ACSStore = (acsstore_t *) Z_Malloc(sizeof(acsstore_t), PU_GAMESTATIC, 0);
        ACSStoreSize = 1;
    }

    acsstore_t *store = &ACSStore[ACSStoreSize-1];

    store->map     = map;
    store->script  = number;
    store->args[0] = args[0];
    store->args[1] = args[1];
    store->args[2] = args[2];
    store->args[3] = args[3];

    return true;
}

dd_bool P_StartLockedACS(Line *line, byte *args, mobj_t *mo, int side)
{
    DENG_ASSERT(args != 0);

    if(!mo->player)
    {
        return false;
    }

    if(int const lock = args[4])
    {
        if(!(mo->player->keys & (1 << (lock - 1))))
        {
            char LockedBuffer[80];
            sprintf(LockedBuffer, "YOU NEED THE %s\n", GET_TXT(TextKeyMessages[lock - 1]));
            P_SetMessage(mo->player, 0, LockedBuffer);
            S_StartSound(SFX_DOOR_LOCKED, mo);
            return false;
        }
    }

    byte newArgs[5];
    for(int i = 0; i < 4; ++i)
    {
        newArgs[i] = args[i];
    }
    newArgs[4] = 0;

    return P_StartACS(newArgs[0], newArgs[1], &newArgs[2], mo, line, side);
}

dd_bool P_TerminateACS(int number, uint /*map*/)
{
    int infoIndex = GetACSIndex(number);
    if(infoIndex >= 0)
    {
        acsinfo_t &info = ACSInfo[infoIndex];
        // Some states disallow termination.
        if(info.state != ASTE_INACTIVE && info.state != ASTE_TERMINATING)
        {
            ACSInfo[infoIndex].state = ASTE_TERMINATING;
            return true;
        }
    }
    return false;
}

dd_bool P_SuspendACS(int number, uint map)
{
    int infoIndex = GetACSIndex(number);
    if(infoIndex >= 0)
    {
        acsinfo_t &info = ACSInfo[infoIndex];
        // Some states disallow suspension.
        if(info.state != ASTE_INACTIVE && info.state != ASTE_SUSPENDED &&
           info.state != ASTE_TERMINATING)
        {
            ACSInfo[infoIndex].state = ASTE_SUSPENDED;
            return true;
        }
    }
    return false;
}

void P_ACSInitNewGame()
{
    memset(WorldVars, 0, sizeof(WorldVars));
    Z_Free(ACSStore); ACSStore = 0;
    ACSStoreSize = 0;
}

void ACScript_Thinker(acs_t *script)
{
    DENG_ASSERT(script != 0);

    if(ACSInfo[script->infoIndex].state == ASTE_TERMINATING)
    {
        ACSInfo[script->infoIndex].state = ASTE_INACTIVE;
        ScriptFinished(ACScript->number);
        Thinker_Remove(&ACScript->thinker);
        return;
    }

    if(ACSInfo[script->infoIndex].state != ASTE_RUNNING)
    {
        return;
    }

    if(script->delayCount)
    {
        script->delayCount--;
        return;
    }
    ACScript = script;

    PCodePtr = ACScript->ip;
    int action = 0;
    do
    {
        int const cmd = LONG(*PCodePtr++);
        action = PCodeCmds[cmd] ();
    } while(action == SCRIPT_CONTINUE);

    ACScript->ip = PCodePtr;
    if(action == SCRIPT_TERMINATE)
    {
        ACSInfo[script->infoIndex].state = ASTE_INACTIVE;
        ScriptFinished(ACScript->number);
        Thinker_Remove(&ACScript->thinker);
    }
}

void ACScript_Write(acs_t const *th)
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

int ACScript_Read(acs_t *th, int mapVersion)
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

void P_TagFinished(int tag)
{
    if(TagBusy(tag)) return;

    for(int i = 0; i < ACScriptCount; ++i)
    {
        acsinfo_t &info = ACSInfo[i];
        if(info.state == ASTE_WAITING_FOR_TAG && info.waitValue == tag)
        {
            info.state = ASTE_RUNNING;
        }
    }
}

void P_ACSPolyobjFinished(int tag)
{
    if(PO_Busy(tag)) return;

    for(int i = 0; i < ACScriptCount; ++i)
    {
        acsinfo_t &info = ACSInfo[i];
        if(info.state == ASTE_WAITING_FOR_POLY && info.waitValue == tag)
        {
            ACSInfo[i].state = ASTE_RUNNING;
        }
    }
}

static void ScriptFinished(int number)
{
    for(int i = 0; i < ACScriptCount; ++i)
    {
        acsinfo_t &info = ACSInfo[i];
        if(info.state == ASTE_WAITING_FOR_SCRIPT && info.waitValue == number)
        {
            info.state = ASTE_RUNNING;
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
    ACScript->stack[ACScript->stackPtr++] = value;
}

static int Pop()
{
    return ACScript->stack[--ACScript->stackPtr];
}

static int Top()
{
    return ACScript->stack[ACScript->stackPtr - 1];
}

static void Drop()
{
    ACScript->stackPtr--;
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
    ACSInfo[ACScript->infoIndex].state = ASTE_SUSPENDED;
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
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec2()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec3()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[2] = Pop();
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec4()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[3] = Pop();
    SpecArgs[2] = Pop();
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

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
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec1Direct()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec2Direct()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec3Direct()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    SpecArgs[2] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec4Direct()
{
    int special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    SpecArgs[2] = LONG(*PCodePtr++);
    SpecArgs[3] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

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
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

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
    ACScript->vars[LONG(*PCodePtr++)] = Pop();
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
    Push(ACScript->vars[LONG(*PCodePtr++)]);
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
    ACScript->vars[LONG(*PCodePtr++)] += Pop();
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
    ACScript->vars[LONG(*PCodePtr++)] -= Pop();
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
    ACScript->vars[LONG(*PCodePtr++)] *= Pop();
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
    ACScript->vars[LONG(*PCodePtr++)] /= Pop();
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
    ACScript->vars[LONG(*PCodePtr++)] %= Pop();
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
    ACScript->vars[LONG(*PCodePtr++)]++;
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
    ACScript->vars[LONG(*PCodePtr++)]--;
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
    ACScript->delayCount = Pop();
    return SCRIPT_STOP;
}

static int CmdDelayDirect()
{
    ACScript->delayCount = LONG(*PCodePtr++);
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
    ACSInfo[ACScript->infoIndex].waitValue = Pop();
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITING_FOR_TAG;
    return SCRIPT_STOP;
}

static int CmdTagWaitDirect()
{
    ACSInfo[ACScript->infoIndex].waitValue = LONG(*PCodePtr++);
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITING_FOR_TAG;
    return SCRIPT_STOP;
}

static int CmdPolyWait()
{
    ACSInfo[ACScript->infoIndex].waitValue = Pop();
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITING_FOR_POLY;
    return SCRIPT_STOP;
}

static int CmdPolyWaitDirect()
{
    ACSInfo[ACScript->infoIndex].waitValue = LONG(*PCodePtr++);
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITING_FOR_POLY;
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
    PCodePtr = ACSInfo[ACScript->infoIndex].address;
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
    Push(ACScript->side);
    return SCRIPT_CONTINUE;
}

static int CmdScriptWait()
{
    ACSInfo[ACScript->infoIndex].waitValue = Pop();
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITING_FOR_SCRIPT;
    return SCRIPT_STOP;
}

static int CmdScriptWaitDirect()
{
    ACSInfo[ACScript->infoIndex].waitValue = LONG(*PCodePtr++);
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITING_FOR_SCRIPT;
    return SCRIPT_STOP;
}

static int CmdClearLineSpecial()
{
    if(ACScript->line)
    {
        P_ToXLine(ACScript->line)->special = 0;
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
    if(ACScript->activator && ACScript->activator->player)
    {
        P_SetMessage(ACScript->activator->player, 0, PrintBuffer);
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
    if(ACScript->line)
    {
        Sector *front = (Sector *) P_GetPtrp(ACScript->line, DMU_FRONT_SECTOR);
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
    if(ACScript->line)
    {
        Sector *front = (Sector *) P_GetPtrp(ACScript->line, DMU_FRONT_SECTOR);
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

    SV_WriteLong(ACSStoreSize);
    for(int i = 0; i < ACSStoreSize; ++i)
    {
        acsstore_t const *store = &ACSStore[i];
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

    if(ver >= 3)
    {
        ACSStoreSize = SV_ReadLong();
        if(ACSStoreSize)
        {
            if(ACSStore)
                ACSStore = (acsstore_t *) Z_Realloc(ACSStore, sizeof(acsstore_t) * ACSStoreSize, PU_GAMESTATIC);
            else
                ACSStore = (acsstore_t *) Z_Malloc(sizeof(acsstore_t) * ACSStoreSize, PU_GAMESTATIC, 0);

            for(int i = 0; i < ACSStoreSize; ++i)
            {
                acsstore_t *store = &ACSStore[i];

                store->map    = SV_ReadLong();
                store->script = SV_ReadLong();
                for(int k = 0; k < 4; ++k)
                {
                    store->args[k] = SV_ReadByte();
                }
            }
        }
    }
    else
    {
        // Old format.
        acsstore_t tempStore[20];

        ACSStoreSize = 0;
        for(int i = 0; i < 20; ++i)
        {
            int map           = SV_ReadLong();
            acsstore_t *store = &tempStore[map < 0? 19 : ACSStoreSize++];

            store->map    = map < 0? 0 : map-1;
            store->script = SV_ReadLong();
            for(int k = 0; k < 4; ++k)
            {
                store->args[k] = SV_ReadByte();
            }
        }

        if(saveVersion < 7)
        {
            SV_Seek(12); // Junk.
        }

        if(ACSStoreSize)
        {
            if(ACSStore)
                ACSStore = (acsstore_t *) Z_Realloc(ACSStore, sizeof(acsstore_t) * ACSStoreSize, PU_GAMESTATIC);
            else
                ACSStore = (acsstore_t *) Z_Malloc(sizeof(acsstore_t) * ACSStoreSize, PU_GAMESTATIC, 0);

            memcpy(ACSStore, tempStore, sizeof(acsstore_t) * ACSStoreSize);
        }
    }

    if(!ACSStoreSize && ACSStore)
    {
        Z_Free(ACSStore); ACSStore = 0;
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
        ACSInfo[i].state     = (aste_t) SV_ReadShort();
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
