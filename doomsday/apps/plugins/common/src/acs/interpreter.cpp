/** @file interpreter.cpp  Action Code Script (ACS), interpreter.
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

#include "acs/interpreter.h"

#include <de/Log>
#include "acs/system.h"
#include "dmu_lib.h"
#include "g_common.h"
#include "gamesession.h"
#include "player.h"
#include "p_map.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include "p_sound.h"

using namespace de;

int acs::Interpreter::currentScriptNumber = -1;

namespace internal
{
    /// Status to return from ACScript command functions.
    enum CommandResult
    {
        Continue,
        Stop,
        Terminate
    };

    typedef CommandResult (*CommandFunc) (acs::Interpreter &);

/// Helper macro for declaring ACScript command functions.
#define ACS_COMMAND(Name) CommandResult cmd##Name(acs::Interpreter &interp)

    static String printBuffer;

#ifdef __JHEXEN__
    static byte specArgs[5];

    ACS_COMMAND(NOP)
    {
        DENG2_UNUSED(interp);
        return Continue;
    }

    ACS_COMMAND(Terminate)
    {
        DENG2_UNUSED(interp);
        return Terminate;
    }

    ACS_COMMAND(Suspend)
    {
        interp.script().setState(acs::Script::Suspended);
        return Stop;
    }

    ACS_COMMAND(PushNumber)
    {
        interp.locals.push(DD_LONG(*interp.pcodePtr++));
        return Continue;
    }

    ACS_COMMAND(LSpec1)
    {
        int special = DD_LONG(*interp.pcodePtr++);
        specArgs[0] = interp.locals.pop();
        P_ExecuteLineSpecial(special, specArgs, interp.line, interp.side, interp.activator);

        return Continue;
    }

    ACS_COMMAND(LSpec2)
    {
        int special = DD_LONG(*interp.pcodePtr++);
        specArgs[1] = interp.locals.pop();
        specArgs[0] = interp.locals.pop();
        P_ExecuteLineSpecial(special, specArgs, interp.line, interp.side, interp.activator);

        return Continue;
    }

    ACS_COMMAND(LSpec3)
    {
        int special = DD_LONG(*interp.pcodePtr++);
        specArgs[2] = interp.locals.pop();
        specArgs[1] = interp.locals.pop();
        specArgs[0] = interp.locals.pop();
        P_ExecuteLineSpecial(special, specArgs, interp.line, interp.side, interp.activator);

        return Continue;
    }

    ACS_COMMAND(LSpec4)
    {
        int special = DD_LONG(*interp.pcodePtr++);
        specArgs[3] = interp.locals.pop();
        specArgs[2] = interp.locals.pop();
        specArgs[1] = interp.locals.pop();
        specArgs[0] = interp.locals.pop();
        P_ExecuteLineSpecial(special, specArgs, interp.line, interp.side, interp.activator);

        return Continue;
    }

    ACS_COMMAND(LSpec5)
    {
        int special = DD_LONG(*interp.pcodePtr++);
        specArgs[4] = interp.locals.pop();
        specArgs[3] = interp.locals.pop();
        specArgs[2] = interp.locals.pop();
        specArgs[1] = interp.locals.pop();
        specArgs[0] = interp.locals.pop();
        P_ExecuteLineSpecial(special, specArgs, interp.line, interp.side,
                             interp.activator);

        return Continue;
    }

    ACS_COMMAND(LSpec1Direct)
    {
        int special = DD_LONG(*interp.pcodePtr++);
        specArgs[0] = DD_LONG(*interp.pcodePtr++);
        P_ExecuteLineSpecial(special, specArgs, interp.line, interp.side,
                             interp.activator);

        return Continue;
    }

    ACS_COMMAND(LSpec2Direct)
    {
        int special = DD_LONG(*interp.pcodePtr++);
        specArgs[0] = DD_LONG(*interp.pcodePtr++);
        specArgs[1] = DD_LONG(*interp.pcodePtr++);
        P_ExecuteLineSpecial(special, specArgs, interp.line, interp.side,
                             interp.activator);

        return Continue;
    }

    ACS_COMMAND(LSpec3Direct)
    {
        int special = DD_LONG(*interp.pcodePtr++);
        specArgs[0] = DD_LONG(*interp.pcodePtr++);
        specArgs[1] = DD_LONG(*interp.pcodePtr++);
        specArgs[2] = DD_LONG(*interp.pcodePtr++);
        P_ExecuteLineSpecial(special, specArgs, interp.line, interp.side,
                             interp.activator);

        return Continue;
    }

    ACS_COMMAND(LSpec4Direct)
    {
        int special = DD_LONG(*interp.pcodePtr++);
        specArgs[0] = DD_LONG(*interp.pcodePtr++);
        specArgs[1] = DD_LONG(*interp.pcodePtr++);
        specArgs[2] = DD_LONG(*interp.pcodePtr++);
        specArgs[3] = DD_LONG(*interp.pcodePtr++);
        P_ExecuteLineSpecial(special, specArgs, interp.line, interp.side,
                             interp.activator);

        return Continue;
    }

    ACS_COMMAND(LSpec5Direct)
    {
        int special = DD_LONG(*interp.pcodePtr++);
        specArgs[0] = DD_LONG(*interp.pcodePtr++);
        specArgs[1] = DD_LONG(*interp.pcodePtr++);
        specArgs[2] = DD_LONG(*interp.pcodePtr++);
        specArgs[3] = DD_LONG(*interp.pcodePtr++);
        specArgs[4] = DD_LONG(*interp.pcodePtr++);
        P_ExecuteLineSpecial(special, specArgs, interp.line, interp.side,
                             interp.activator);

        return Continue;
    }

    ACS_COMMAND(Add)
    {
        interp.locals.push(interp.locals.pop() + interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(Subtract)
    {
        int operand2 = interp.locals.pop();
        interp.locals.push(interp.locals.pop() - operand2);
        return Continue;
    }

    ACS_COMMAND(Multiply)
    {
        interp.locals.push(interp.locals.pop() * interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(Divide)
    {
        int operand2 = interp.locals.pop();
        interp.locals.push(interp.locals.pop() / operand2);
        return Continue;
    }

    ACS_COMMAND(Modulus)
    {
        int operand2 = interp.locals.pop();
        interp.locals.push(interp.locals.pop() % operand2);
        return Continue;
    }

    ACS_COMMAND(EQ)
    {
        interp.locals.push(interp.locals.pop() == interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(NE)
    {
        interp.locals.push(interp.locals.pop() != interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(LT)
    {
        int operand2 = interp.locals.pop();
        interp.locals.push(interp.locals.pop() < operand2);
        return Continue;
    }

    ACS_COMMAND(GT)
    {
        int operand2 = interp.locals.pop();
        interp.locals.push(interp.locals.pop() > operand2);
        return Continue;
    }

    ACS_COMMAND(LE)
    {
        int operand2 = interp.locals.pop();
        interp.locals.push(interp.locals.pop() <= operand2);
        return Continue;
    }

    ACS_COMMAND(GE)
    {
        int operand2 = interp.locals.pop();
        interp.locals.push(interp.locals.pop() >= operand2);
        return Continue;
    }

    ACS_COMMAND(AssignScriptVar)
    {
        interp.args[DD_LONG(*interp.pcodePtr++)] = interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(AssignMapVar)
    {
        interp.scriptSys().mapVars[DD_LONG(*interp.pcodePtr++)] = interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(AssignWorldVar)
    {
        interp.scriptSys().worldVars[DD_LONG(*interp.pcodePtr++)] = interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(PushScriptVar)
    {
        interp.locals.push(interp.args[DD_LONG(*interp.pcodePtr++)]);
        return Continue;
    }

    ACS_COMMAND(PushMapVar)
    {
        interp.locals.push(interp.scriptSys().mapVars[DD_LONG(*interp.pcodePtr++)]);
        return Continue;
    }

    ACS_COMMAND(PushWorldVar)
    {
        interp.locals.push(interp.scriptSys().worldVars[DD_LONG(*interp.pcodePtr++)]);
        return Continue;
    }

    ACS_COMMAND(AddScriptVar)
    {
        interp.args[DD_LONG(*interp.pcodePtr++)] += interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(AddMapVar)
    {
        interp.scriptSys().mapVars[DD_LONG(*interp.pcodePtr++)] += interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(AddWorldVar)
    {
        interp.scriptSys().worldVars[DD_LONG(*interp.pcodePtr++)] += interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(SubScriptVar)
    {
        interp.args[DD_LONG(*interp.pcodePtr++)] -= interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(SubMapVar)
    {
        interp.scriptSys().mapVars[DD_LONG(*interp.pcodePtr++)] -= interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(SubWorldVar)
    {
        interp.scriptSys().worldVars[DD_LONG(*interp.pcodePtr++)] -= interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(MulScriptVar)
    {
        interp.args[DD_LONG(*interp.pcodePtr++)] *= interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(MulMapVar)
    {
        interp.scriptSys().mapVars[DD_LONG(*interp.pcodePtr++)] *= interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(MulWorldVar)
    {
        interp.scriptSys().worldVars[DD_LONG(*interp.pcodePtr++)] *= interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(DivScriptVar)
    {
        interp.args[DD_LONG(*interp.pcodePtr++)] /= interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(DivMapVar)
    {
        interp.scriptSys().mapVars[DD_LONG(*interp.pcodePtr++)] /= interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(DivWorldVar)
    {
        interp.scriptSys().worldVars[DD_LONG(*interp.pcodePtr++)] /= interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(ModScriptVar)
    {
        interp.args[DD_LONG(*interp.pcodePtr++)] %= interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(ModMapVar)
    {
        interp.scriptSys().mapVars[DD_LONG(*interp.pcodePtr++)] %= interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(ModWorldVar)
    {
        interp.scriptSys().worldVars[DD_LONG(*interp.pcodePtr++)] %= interp.locals.pop();
        return Continue;
    }

    ACS_COMMAND(IncScriptVar)
    {
        interp.args[DD_LONG(*interp.pcodePtr++)]++;
        return Continue;
    }

    ACS_COMMAND(IncMapVar)
    {
        interp.scriptSys().mapVars[DD_LONG(*interp.pcodePtr++)]++;
        return Continue;
    }

    ACS_COMMAND(IncWorldVar)
    {
        interp.scriptSys().worldVars[DD_LONG(*interp.pcodePtr++)]++;
        return Continue;
    }

    ACS_COMMAND(DecScriptVar)
    {
        interp.args[DD_LONG(*interp.pcodePtr++)]--;
        return Continue;
    }

    ACS_COMMAND(DecMapVar)
    {
        interp.scriptSys().mapVars[DD_LONG(*interp.pcodePtr++)]--;
        return Continue;
    }

    ACS_COMMAND(DecWorldVar)
    {
        interp.scriptSys().worldVars[DD_LONG(*interp.pcodePtr++)]--;
        return Continue;
    }

    ACS_COMMAND(Goto)
    {
        interp.pcodePtr = (int const *) (interp.scriptSys().module().pcode().constData() + DD_LONG(*interp.pcodePtr));
        return Continue;
    }

    ACS_COMMAND(IfGoto)
    {
        if(interp.locals.pop())
        {
            interp.pcodePtr = (int const *) (interp.scriptSys().module().pcode().constData() + DD_LONG(*interp.pcodePtr));
        }
        else
        {
            interp.pcodePtr++;
        }
        return Continue;
    }

    ACS_COMMAND(Drop)
    {
        interp.locals.drop();
        return Continue;
    }

    ACS_COMMAND(Delay)
    {
        interp.delayCount = interp.locals.pop();
        return Stop;
    }

    ACS_COMMAND(DelayDirect)
    {
        interp.delayCount = DD_LONG(*interp.pcodePtr++);
        return Stop;
    }

    ACS_COMMAND(Random)
    {
        int high = interp.locals.pop();
        int low  = interp.locals.pop();
        interp.locals.push(low + (P_Random() % (high - low + 1)));
        return Continue;
    }

    ACS_COMMAND(RandomDirect)
    {
        int low  = DD_LONG(*interp.pcodePtr++);
        int high = DD_LONG(*interp.pcodePtr++);
        interp.locals.push(low + (P_Random() % (high - low + 1)));
        return Continue;
    }

    ACS_COMMAND(ThingCount)
    {
        int tid  = interp.locals.pop();
        int type = interp.locals.pop();
        // Anything to count?
        if(type + tid)
        {
            interp.locals.push(P_MobjCount(type, tid));
        }
        return Continue;
    }

    ACS_COMMAND(ThingCountDirect)
    {
        int type = DD_LONG(*interp.pcodePtr++);
        int tid  = DD_LONG(*interp.pcodePtr++);
        // Anything to count?
        if(type + tid)
        {
            interp.locals.push(P_MobjCount(type, tid));
        }
        return Continue;
    }

    ACS_COMMAND(TagWait)
    {
        interp.script().waitForSector(interp.locals.pop());
        return Stop;
    }

    ACS_COMMAND(TagWaitDirect)
    {
        interp.script().waitForSector(DD_LONG(*interp.pcodePtr++));
        return Stop;
    }

    ACS_COMMAND(PolyWait)
    {
        interp.script().waitForPolyobj(interp.locals.pop());
        return Stop;
    }

    ACS_COMMAND(PolyWaitDirect)
    {
        interp.script().waitForPolyobj(DD_LONG(*interp.pcodePtr++));
        return Stop;
    }

    ACS_COMMAND(ChangeFloor)
    {
        AutoStr *path = Str_PercentEncode(AutoStr_FromTextStd(interp.scriptSys().module().constant(interp.locals.pop()).toUtf8().constData()));
        uri_s *uri = Uri_NewWithPath3("Flats", Str_Text(path));

        world_Material *mat = (world_Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
        Uri_Delete(uri);

        int tag = interp.locals.pop();

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
        int tag = DD_LONG(*interp.pcodePtr++);

        AutoStr *path = Str_PercentEncode(AutoStr_FromTextStd(interp.scriptSys().module().constant(DD_LONG(*interp.pcodePtr++)).toUtf8().constData()));
        uri_s *uri = Uri_NewWithPath3("Flats", Str_Text(path));

        world_Material *mat = (world_Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
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
        AutoStr *path = Str_PercentEncode(AutoStr_FromTextStd(interp.scriptSys().module().constant(interp.locals.pop()).toUtf8().constData()));
        uri_s *uri = Uri_NewWithPath3("Flats", Str_Text(path));

        world_Material *mat = (world_Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
        Uri_Delete(uri);

        int tag = interp.locals.pop();

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
        int tag = DD_LONG(*interp.pcodePtr++);

        AutoStr *path = Str_PercentEncode(AutoStr_FromTextStd(interp.scriptSys().module().constant(DD_LONG(*interp.pcodePtr++)).toUtf8().constData()));
        uri_s *uri = Uri_NewWithPath3("Flats", Str_Text(path));

        world_Material *mat = (world_Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
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
        interp.pcodePtr = interp.script().entryPoint().pcodePtr;
        return Continue;
    }

    ACS_COMMAND(AndLogical)
    {
        interp.locals.push(interp.locals.pop() && interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(OrLogical)
    {
        interp.locals.push(interp.locals.pop() || interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(AndBitwise)
    {
        interp.locals.push(interp.locals.pop() & interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(OrBitwise)
    {
        interp.locals.push(interp.locals.pop() | interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(EorBitwise)
    {
        interp.locals.push(interp.locals.pop() ^ interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(NegateLogical)
    {
        interp.locals.push(!interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(LShift)
    {
        int operand2 = interp.locals.pop();
        interp.locals.push(interp.locals.pop() << operand2);
        return Continue;
    }

    ACS_COMMAND(RShift)
    {
        int operand2 = interp.locals.pop();
        interp.locals.push(interp.locals.pop() >> operand2);
        return Continue;
    }

    ACS_COMMAND(UnaryMinus)
    {
        interp.locals.push(-interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(IfNotGoto)
    {
        if(interp.locals.pop())
        {
            interp.pcodePtr++;
        }
        else
        {
            interp.pcodePtr = (int const *) (interp.scriptSys().module().pcode().constData() + DD_LONG(*interp.pcodePtr));
        }
        return Continue;
    }

    ACS_COMMAND(LineSide)
    {
        interp.locals.push(interp.side);
        return Continue;
    }

    ACS_COMMAND(ScriptWait)
    {
        interp.script().waitForScript(interp.locals.pop());
        return Stop;
    }

    ACS_COMMAND(ScriptWaitDirect)
    {
        interp.script().waitForScript(DD_LONG(*interp.pcodePtr++));
        return Stop;
    }

    ACS_COMMAND(ClearLineSpecial)
    {
        if(interp.line)
        {
            P_ToXLine(interp.line)->special = 0;
        }
        return Continue;
    }

    ACS_COMMAND(CaseGoto)
    {
        if(interp.locals.top() == DD_LONG(*interp.pcodePtr++))
        {
            interp.pcodePtr = (int const *) (interp.scriptSys().module().pcode().constData() + DD_LONG(*interp.pcodePtr));
            interp.locals.drop();
        }
        else
        {
            interp.pcodePtr++;
        }
        return Continue;
    }

    ACS_COMMAND(BeginPrint)
    {
        DENG2_UNUSED(interp);
        printBuffer.clear();
        return Continue;
    }

    ACS_COMMAND(EndPrint)
    {
        if(interp.activator && interp.activator->player)
        {
            P_SetMessage(interp.activator->player, printBuffer.toUtf8().constData());
        }
        else
        {
            // Send to everybody.
            for(int i = 0; i < MAXPLAYERS; ++i)
            {
                if(players[i].plr->inGame)
                {
                    P_SetMessage(&players[i], printBuffer.toUtf8().constData());
                }
            }
        }

        return Continue;
    }

    ACS_COMMAND(EndPrintBold)
    {
        DENG2_UNUSED(interp);
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
            {
                P_SetYellowMessage(&players[i], printBuffer.toUtf8().constData());
            }
        }
        return Continue;
    }

    ACS_COMMAND(PrintString)
    {
        printBuffer += interp.scriptSys().module().constant(interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(PrintNumber)
    {
        printBuffer += String::number(interp.locals.pop());
        return Continue;
    }

    ACS_COMMAND(PrintCharacter)
    {
        char ch[2];
        ch[0] = interp.locals.pop();
        ch[1] = 0;
        printBuffer += String(ch);
        return Continue;
    }

    ACS_COMMAND(PlayerCount)
    {
        int count = 0;
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            count += players[i].plr->inGame;
        }
        interp.locals.push(count);
        return Continue;
    }

    ACS_COMMAND(GameType)
    {
        int gametype;

        if(!IS_NETGAME)
        {
            gametype = 0; // singleplayer
        }
        else if(gfw_Rule(deathmatch))
        {
            gametype = 2; // deathmatch
        }
        else
        {
            gametype = 1; // cooperative
        }
        interp.locals.push(gametype);

        return Continue;
    }

    ACS_COMMAND(GameSkill)
    {
        interp.locals.push((int)gfw_Rule(skill));
        return Continue;
    }

    ACS_COMMAND(Timer)
    {
        interp.locals.push(mapTime);
        return Continue;
    }

    ACS_COMMAND(SectorSound)
    {
        mobj_t *emitter = nullptr;
        if(interp.line)
        {
            auto *sector = (Sector *) P_GetPtrp(interp.line, DMU_FRONT_SECTOR);
            emitter = (mobj_t *) P_GetPtrp(sector, DMU_EMITTER);
        }
        int volume = interp.locals.pop();

        S_StartSoundAtVolume(S_GetSoundID(interp.scriptSys().module().constant(interp.locals.pop()).toUtf8().constData()),
                             emitter, volume / 127.0f);
        return Continue;
    }

    ACS_COMMAND(ThingSound)
    {
        int volume   = interp.locals.pop();
        int sound    = S_GetSoundID(interp.scriptSys().module().constant(interp.locals.pop()).toUtf8().constData());
        int tid      = interp.locals.pop();
        int searcher = -1;

        mobj_t *emitter;
        while(sound && (emitter = P_FindMobjFromTID(tid, &searcher)))
        {
            S_StartSoundAtVolume(sound, emitter, volume / 127.0f);
        }

        return Continue;
    }

    ACS_COMMAND(AmbientSound)
    {
        mobj_t *emitter = nullptr; // For 3D positioning.
        mobj_t *plrMo   = players[DISPLAYPLAYER].plr->mo;

        int volume = interp.locals.pop();

        // If we are playing 3D sounds, create a temporary source mobj for the sound.
        if(Con_GetInteger("sound-3d") && plrMo)
        {
            // SpawnMobj calls P_Random. We don't want that the random generator gets
            // out of sync.
            if((emitter = P_SpawnMobjXYZ(MT_CAMERA,
                                         plrMo->origin[VX] + ((M_Random() - 127) * 2),
                                         plrMo->origin[VY] + ((M_Random() - 127) * 2),
                                         plrMo->origin[VZ] + ((M_Random() - 127) * 2),
                                         0, 0)))
            {
                emitter->tics = 5 * TICSPERSEC; // Five seconds should be enough.
            }
        }

        int sound = S_GetSoundID(interp.scriptSys().module().constant(interp.locals.pop()).toUtf8().constData());
        S_StartSoundAtVolume(sound, emitter, volume / 127.0f);

        return Continue;
    }

    ACS_COMMAND(SoundSequence)
    {
        mobj_t *emitter = nullptr;
        if(interp.line)
        {
            auto *sector = (Sector *) P_GetPtrp(interp.line, DMU_FRONT_SECTOR);
            emitter = (mobj_t *) P_GetPtrp(sector, DMU_EMITTER);
        }
        SN_StartSequenceName(emitter, interp.scriptSys().module().constant(interp.locals.pop()).toUtf8().constData());

        return Continue;
    }

    ACS_COMMAND(SetLineTexture)
    {
#define TEXTURE_TOP 0
#define TEXTURE_MIDDLE 1
#define TEXTURE_BOTTOM 2

        AutoStr *path = Str_PercentEncode(AutoStr_FromTextStd(interp.scriptSys().module().constant(interp.locals.pop()).toUtf8().constData()));
        uri_s *uri = Uri_NewWithPath3("Textures", Str_Text(path));

        world_Material *mat = (world_Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
        Uri_Delete(uri);

        int position = interp.locals.pop();
        int side     = interp.locals.pop();
        int lineTag  = interp.locals.pop();

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
        int lineFlags = interp.locals.pop()? DDLF_BLOCKING : 0;
        int lineTag   = interp.locals.pop();

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
        int arg5    = interp.locals.pop();
        int arg4    = interp.locals.pop();
        int arg3    = interp.locals.pop();
        int arg2    = interp.locals.pop();
        int arg1    = interp.locals.pop();
        int special = interp.locals.pop();
        int lineTag = interp.locals.pop();

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

    static CommandFunc const &findCommand(int name)
    {
        static CommandFunc const cmds[] =
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
        static int const numCmds = sizeof(cmds) / sizeof(cmds[0]);
        if(name >= 0 && name < numCmds) return cmds[name];
        /// @throw Error  Invalid command name specified.
        throw Error("acs::Interpreter::findCommand", "Unknown command #" + String::number(name));
    }

#endif  // __JHEXEN__

} // namespace internal

namespace acs {

using namespace internal;

thinker_t *Interpreter::newThinker(Script &script, Script::Args const &scriptArgs,
    mobj_t *activator, Line *line, int side, int delayCount)
{
    Module::EntryPoint const &ep = script.entryPoint();

    Interpreter *th = (Interpreter *) Z_Calloc(sizeof(*th), PU_MAP, nullptr);
    th->thinker.function = (thinkfunc_t) acs_Interpreter_Think;

    th->_script    = &script;
    th->pcodePtr   = ep.pcodePtr;
    th->delayCount = delayCount;
    th->activator  = activator;
    th->line       = line;
    th->side       = side;
    th->delayCount = delayCount;

    for(int i = 0; i < ep.scriptArgCount; ++i)
    {
        th->args[i] = scriptArgs[i];
    }

    Thinker_Add(&th->thinker);

    return &th->thinker;
}

void Interpreter::think()
{
#ifdef __JHEXEN__
    int action = (script().state() == Script::Terminating? Terminate : Continue);

    if(script().isRunning())
    {
        if(delayCount)
        {
            delayCount--;
            return;
        }

        currentScriptNumber = script().entryPoint().scriptNumber;

        while((action = findCommand(DD_LONG(*pcodePtr++))(*this)) == Continue)
        {}

        currentScriptNumber = -1;
    }

    if(action == Terminate)
    {
        // This script has now finished - notify interested parties.
        /// @todo Use a de::Observers -based mechanism for this.
        script().setState(Script::Inactive);

        // Notify any scripts which are waiting for this script to finish.
        scriptSys().forAllScripts([this] (Script &otherScript)
        {
            otherScript.resumeIfWaitingForScript(script());
            return LoopContinue;
        });

        Thinker_Remove(&thinker);
    }
#endif
}

System &Interpreter::scriptSys() const
{
    return gfw_Session()->acsSystem();
}

Script &Interpreter::script() const
{
    DENG2_ASSERT(_script);
    return *_script;
}

void Interpreter::Stack::push(int value)
{
    if (height >= ACS_INTERPRETER_SCRIPT_STACK_DEPTH)
    { 
        LOG_SCR_ERROR("acs::Interpreter::Stack::push: Overflow");
        return;
    }
    values[height++] = value;
}

int Interpreter::Stack::pop()
{
    if (height <= 0)
    {
        LOG_SCR_ERROR("acs::Interpreter::Stack::pop: Underflow");
        return 0;
    }
    return values[--height];
}

int Interpreter::Stack::top() const
{
    if (height == 0)
    {
        LOG_SCR_ERROR("acs::Interpreter::Stack::top: Underflow");
        return 0;
    }
    return values[height - 1];
}

void Interpreter::Stack::drop()
{
    if(height == 0)
        LOG_SCR_ERROR("acs::Interpreter::Stack::drop: Underflow");
    height--;
}

void Interpreter::write(MapStateWriter *msw) const
{
    writer_s *writer = msw->writer();

    Writer_WriteByte(writer, 2); // Write a version byte.

    Writer_WriteInt32(writer, msw->serialIdFor(activator));
    Writer_WriteInt32(writer, P_ToIndex(line));
    Writer_WriteInt32(writer, side);
    Writer_WriteInt32(writer, script().entryPoint().scriptNumber);
    Writer_WriteInt32(writer, delayCount);
    for(int i = 0; i < ACS_INTERPRETER_SCRIPT_STACK_DEPTH; ++i)
    {
        Writer_WriteInt32(writer, locals.values[i]);
    }
    Writer_WriteInt32(writer, locals.height);
    for(int i = 0; i < ACS_INTERPRETER_MAX_SCRIPT_ARGS; ++i)
    {
        Writer_WriteInt32(writer, args[i]);
    }
    Writer_WriteInt32(writer, ((dbyte const *)pcodePtr) - (dbyte const *)scriptSys().module().pcode().constData());
}

int Interpreter::read(MapStateReader *msr)
{
    reader_s *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    if(mapVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        int ver = Reader_ReadByte(reader); // version byte.

        // Activator.
        activator = INT2PTR(mobj_t, Reader_ReadInt32(reader));
        activator = msr->mobj(PTR2INT(activator), &activator);

        // Line.
        int lineIndex = Reader_ReadInt32(reader);
        if(lineIndex >= 0)
        {
            line = (Line *) P_ToPtr(DMU_LINE, lineIndex);
            DENG2_ASSERT(line);
        }
        else
        {
            line = nullptr;
        }

        // Side index.
        side = Reader_ReadInt32(reader);

        // Script number.
        int scriptNumber = Reader_ReadInt32(reader);
        _script = &scriptSys().script(scriptNumber);

        // Obsolete ignored value in the old format?
        if(ver < 2)
        {
            /*infoIndex =*/ Reader_ReadInt32(reader);
        }

        delayCount = Reader_ReadInt32(reader);

        for(int i = 0; i < ACS_INTERPRETER_SCRIPT_STACK_DEPTH; ++i)
        {
            locals.values[i] = Reader_ReadInt32(reader);
        }
        locals.height = Reader_ReadInt32(reader);

        for(int i = 0; i < ACS_INTERPRETER_MAX_SCRIPT_ARGS; ++i)
        {
            args[i] = Reader_ReadInt32(reader);
        }

        pcodePtr = (int const *) (scriptSys().module().pcode().constData() + Reader_ReadInt32(reader));
    }
    else
    {
        // Its in the old pre V4 format which serialized acs_t
        // Padding at the start (an old thinker_t struct)
        byte junk[16]; // sizeof thinker_t
        Reader_Read(reader, junk, 16);

        // Start of used data members.
        activator  = INT2PTR(mobj_t, Reader_ReadInt32(reader));
        activator  = msr->mobj(PTR2INT(activator), &activator);

        int temp = Reader_ReadInt32(reader);
        if(temp >= 0)
        {
            line = (Line *) P_ToPtr(DMU_LINE, temp);
            DENG2_ASSERT(line);
        }
        else
        {
            line = nullptr;
        }

        side       = Reader_ReadInt32(reader);
        _script    = &scriptSys().script(Reader_ReadInt32(reader));
        /*infoIndex  =*/ Reader_ReadInt32(reader);
        delayCount = Reader_ReadInt32(reader);

        for(int i = 0; i < ACS_INTERPRETER_SCRIPT_STACK_DEPTH; ++i)
        {
            locals.values[i] = Reader_ReadInt32(reader);
        }
        locals.height = Reader_ReadInt32(reader);

        for(int i = 0; i < ACS_INTERPRETER_MAX_SCRIPT_ARGS; ++i)
        {
            args[i] = Reader_ReadInt32(reader);
        }

        pcodePtr = (int const *) (scriptSys().module().pcode().constData() + Reader_ReadInt32(reader));
    }

    thinker.function = (thinkfunc_t) acs_Interpreter_Think;

    return true; // Add this thinker.
}

}  // namespace acs

void acs_Interpreter_Think(acs_Interpreter *interp)
{
    DENG2_ASSERT(interp);
    reinterpret_cast<acs::Interpreter *>(interp)->think();
}
