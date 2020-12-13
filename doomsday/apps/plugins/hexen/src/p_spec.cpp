/** @file p_spec.cpp  Special map actions.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
#include "p_spec.h"

#include <acs/interpreter.h>

#include <cstdio>
#include <cstring>
#include "acs/system.h"
#include "dmu_lib.h"
#include "d_netsv.h"
#include "g_common.h"
#include "gamesession.h"
#include "lightninganimator.h"
#include "p_inventory.h"
#include "player.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_plat.h"
#include "p_floor.h"
#include "p_scroll.h"
#include "p_switch.h"
#include "p_user.h"
#include "polyobjs.h"

using namespace de;
using namespace common;

static inline acs::System &acscriptSys()
{
    return gfw_Session()->acsSystem();
}

LightningAnimator lightningAnimator;

ThinkerT<mobj_t> lavaInflictor;

mobj_t *P_LavaInflictor()
{
    return lavaInflictor;
}

void P_InitLava()
{
    lavaInflictor = ThinkerT<mobj_t>();

    lavaInflictor->type = MT_CIRCLEFLAME;
    lavaInflictor->flags2 = MF2_FIREDAMAGE | MF2_NODMGTHRUST;
}

dd_bool EV_SectorSoundChange(byte *args)
{
    if(!args[0]) return false;

    dd_bool result = false;

    if(iterlist_t *list = P_GetSectorIterListForTag((int) args[0], false))
    {
        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);
        Sector *sec = 0;
        while((sec = (Sector *)IterList_MoveIterator(list)))
        {
            P_ToXSector(sec)->seqType = seqtype_t(args[1]);
            result = true;
        }
    }

    return result;
}

static dd_bool CheckedLockedDoor(mobj_t *mo, byte lock)
{
    DENG2_ASSERT(mo != 0);

    if(!mo->player) return false;
    if(!lock) return true;

    if(!(mo->player->keys & (1 << (lock - 1))))
    {
        char LockedBuffer[80];
        sprintf(LockedBuffer, "YOU NEED THE %s\n", GET_TXT(TextKeyMessages[lock - 1]));

        P_SetMessage(mo->player, LockedBuffer);
        S_StartSound(SFX_DOOR_LOCKED, mo);
        return false;
    }

    return true;
}

dd_bool EV_LineSearchForPuzzleItem(Line *line, byte * /*args*/, mobj_t *mo)
{
    if(!mo || !mo->player || !line)
        return false;

    inventoryitemtype_t type = inventoryitemtype_t(IIT_FIRSTPUZZITEM + P_ToXLine(line)->arg1);
    if(type < IIT_FIRSTPUZZITEM)
        return false;

    // Search player's inventory for puzzle items
    return P_InventoryUse(mo->player - players, type, false);
}

static bool isThingSpawnEventAllowed()
{
    if (gameMode == hexen_deathkings && acs::Interpreter::currentScriptNumber == 255)
    {
        // This is the auto-respawn script.
        if (randf() >= float(cfg.deathkingsAutoRespawnChance) / 100.0f)
        {
            App_Log(DE2_MAP_VERBOSE, "Monster autorespawn suppressed in ACS script 255");
            return false;
        }
    }
    return true;
}

dd_bool P_ExecuteLineSpecial(int special, byte args[5], Line *line, int side, mobj_t *mo)
{
    dd_bool success = false;

    App_Log(DE2_MAP_VERBOSE, "Executing line special %i, mobj:%i", special, mo? mo->thinker.id : 0);

    switch(special)
    {
    case 1: // Poly Start Line
        break;

    case 2: // Poly Rotate Left
        success = EV_RotatePoly(line, args, 1, false);
        break;

    case 3: // Poly Rotate Right
        success = EV_RotatePoly(line, args, -1, false);
        break;

    case 4: // Poly Move
        success = EV_MovePoly(line, args, false, false);
        break;

    case 5: // Poly Explicit Line:  Only used in initialization
        break;

    case 6: // Poly Move Times 8
        success = EV_MovePoly(line, args, true, false);
        break;

    case 7: // Poly Door Swing
        success = EV_OpenPolyDoor(line, args, PODOOR_SWING);
        break;

    case 8: // Poly Door Slide
        success = EV_OpenPolyDoor(line, args, PODOOR_SLIDE);
        break;

    case 10: // Door Close
        success = EV_DoDoor(line, args, DT_CLOSE);
        break;

    case 11: // Door Open
        if(!args[0])
        {
            success = EV_VerticalDoor(line, mo);
        }
        else
        {
            success = EV_DoDoor(line, args, DT_OPEN);
        }
        break;

    case 12: // Door Raise
        if(!args[0])
        {
            success = EV_VerticalDoor(line, mo);
        }
        else
        {
            success = EV_DoDoor(line, args, DT_NORMAL);
        }
        break;

    case 13: // Door Locked_Raise
        if(CheckedLockedDoor(mo, args[3]))
        {
            if(!args[0])
            {
                success = EV_VerticalDoor(line, mo);
            }
            else
            {
                success = EV_DoDoor(line, args, DT_NORMAL);
            }
        }
        break;

    case 20: // Floor Lower by Value
        success = EV_DoFloor(line, args, FT_LOWERBYVALUE);
        break;

    case 21: // Floor Lower to Lowest
        success = EV_DoFloor(line, args, FT_LOWERTOLOWEST);
        break;

    case 22: // Floor Lower to Nearest
        success = EV_DoFloor(line, args, FT_LOWER);
        break;

    case 23: // Floor Raise by Value
        success = EV_DoFloor(line, args, FT_RAISEFLOORBYVALUE);
        break;

    case 24: // Floor Raise to Highest
        success = EV_DoFloor(line, args, FT_RAISEFLOOR);
        break;

    case 25: // Floor Raise to Nearest
        success = EV_DoFloor(line, args, FT_RAISEFLOORTONEAREST);
        break;

    case 26: // Stairs Build Down Normal
        success = EV_BuildStairs(line, args, -1, STAIRS_NORMAL);
        break;

    case 27: // Build Stairs Up Normal
        success = EV_BuildStairs(line, args, 1, STAIRS_NORMAL);
        break;

    case 28: // Floor Raise and Crush
        success = EV_DoFloor(line, args, FT_RAISEFLOORCRUSH);
        break;

    case 29: // Build Pillar (no crushing)
        success = EV_BuildPillar(line, args, false);
        break;

    case 30: // Open Pillar
        success = EV_OpenPillar(line, args);
        break;

    case 31: // Stairs Build Down Sync
        success = EV_BuildStairs(line, args, -1, STAIRS_SYNC);
        break;

    case 32: // Build Stairs Up Sync
        success = EV_BuildStairs(line, args, 1, STAIRS_SYNC);
        break;

    case 35: // Raise Floor by Value Times 8
        success = EV_DoFloor(line, args, FT_RAISEBYVALUEMUL8);
        break;

    case 36: // Lower Floor by Value Times 8
        success = EV_DoFloor(line, args, FT_LOWERBYVALUEMUL8);
        break;

    case 40: // Ceiling Lower by Value
        success = EV_DoCeiling(line, args, CT_LOWERBYVALUE);
        break;

    case 41: // Ceiling Raise by Value
        success = EV_DoCeiling(line, args, CT_RAISEBYVALUE);
        break;

    case 42: // Ceiling Crush and Raise
        success = EV_DoCeiling(line, args, CT_CRUSHANDRAISE);
        break;

    case 43: // Ceiling Lower and Crush
        success = EV_DoCeiling(line, args, CT_LOWERANDCRUSH);
        break;

    case 44: // Ceiling Crush Stop
        success = P_CeilingDeactivate((short) args[0]);
        break;

    case 45: // Ceiling Crush Raise and Stay
        success = EV_DoCeiling(line, args, CT_CRUSHRAISEANDSTAY);
        break;

    case 46: // Floor Crush Stop
        success = EV_FloorCrushStop(line, args);
        break;

    case 60: // Plat Perpetual Raise
        success = EV_DoPlat(line, args, PT_PERPETUALRAISE, 0);
        break;

    case 61: // Plat Stop
        P_PlatDeactivate((short) args[0]);
        break;

    case 62: // Plat Down-Wait-Up-Stay
        success = EV_DoPlat(line, args, PT_DOWNWAITUPSTAY, 0);
        break;

    case 63: // Plat Down-by-Value*8-Wait-Up-Stay
        success = EV_DoPlat(line, args, PT_DOWNBYVALUEWAITUPSTAY, 0);
        break;

    case 64: // Plat Up-Wait-Down-Stay
        success = EV_DoPlat(line, args, PT_UPWAITDOWNSTAY, 0);
        break;

    case 65: // Plat Up-by-Value*8-Wait-Down-Stay
        success = EV_DoPlat(line, args, PT_UPBYVALUEWAITDOWNSTAY, 0);
        break;

    case 66: // Floor Lower Instant * 8
        success = EV_DoFloor(line, args, FT_LOWERMUL8INSTANT);
        break;

    case 67: // Floor Raise Instant * 8
        success = EV_DoFloor(line, args, FT_RAISEMUL8INSTANT);
        break;

    case 68: // Floor Move to Value * 8
        success = EV_DoFloor(line, args, FT_TOVALUEMUL8);
        break;

    case 69: // Ceiling Move to Value * 8
        success = EV_DoCeiling(line, args, CT_MOVETOVALUEMUL8);
        break;

    case 70: // Teleport
        if(side == 0)
        {   // Only teleport when crossing the front side of a line
            success = EV_Teleport(args[0], mo, true);
        }
        break;

    case 71: // Teleport, no fog
        if(side == 0)
        {   // Only teleport when crossing the front side of a line
            success = EV_Teleport(args[0], mo, false);
        }
        break;

    case 72: // Thrust Mobj
        if(!side) // Only thrust on side 0
        {
            P_ThrustMobj(mo, args[0] * (ANGLE_90 / 64), (float) args[1]);
            success = 1;
        }
        break;

    case 73: // Damage Mobj
        if(args[0])
        {
            P_DamageMobj(mo, NULL, NULL, args[0], false);
        }
        else
        {   // If arg1 is zero, then guarantee a kill
            P_DamageMobj(mo, NULL, NULL, 10000, false);
        }
        success = 1;
        break;

    case 74: // Teleport_NewMap
        if(side == 0) // Only teleport when crossing the front side of a line
        {
            // Players must be alive to teleport
            if(!(mo && mo->player && mo->player->playerState == PST_DEAD))
            {
                // Assume the referenced map is from the current episode.
                dint epIdx  = gfw_Session()->episodeId().toInt();
                if(epIdx > 0) epIdx -= 1;
                dint mapIdx = args[0];
                if(mapIdx > 0) mapIdx -= 1;
                G_SetGameActionMapCompleted(G_ComposeMapUri(epIdx, mapIdx), args[1]);
                success = true;
            }
        }
        break;

    case 75: // Teleport_EndGame
        if(side == 0)  // Only teleport when crossing the front side of a line
        {
            // Players must be alive to teleport
            if(!(mo && mo->player && mo->player->playerState == PST_DEAD))
            {
                success = true;
                if(gfw_Rule(deathmatch))
                {
                    // Winning in deathmatch goes back to the first map of the current episode.
                    G_SetGameActionMapCompleted(de::makeUri(gfw_Session()->episodeDef()->gets("startMap")));
                }
                else
                {
                    // Passing a URI with an empty path starts the Finale
                    G_SetGameActionMapCompleted(de::makeUri("Maps:"));
                }
            }
        }
        break;

    case 83: // ACS_LockedExecute

        // Only players can operate locks.
        if(!mo || !mo->player) break;

        // Is a lock in effect?
        if(int lock = args[4])
        {
            // Does the player possess the necessary key(s)?
            if(!(mo->player->keys & (1 << (lock - 1))))
            {
                auto const msg = String("You need the ") + String(GET_TXT(TextKeyMessages[lock - 1]));
                P_SetMessage(mo->player, msg.toUtf8().constData());
                S_StartSound(SFX_DOOR_LOCKED, mo);
                break;
            }
        }

        // Intentional fall-through.

    case 80: /* ACS_Execute */ {
        dint const scriptNumber = args[0];
        acs::Script::Args const scriptArgs(&args[2], 3);

        // Assume the referenced map is from the current episode.
        dint epIdx  = gfw_Session()->episodeId().toInt();
        if(epIdx > 0) epIdx -= 1;

        dint mapIdx = args[1];
        de::Uri const mapUri = (mapIdx == 0? gfw_Session()->mapUri()
                                           : G_ComposeMapUri(epIdx, mapIdx - 1) );
        if(gfw_Session()->mapUri() == mapUri)
        {
            if(acscriptSys().hasScript(scriptNumber))
            {
               success = acscriptSys().script(scriptNumber).start(scriptArgs, mo, line, side);
            }
        }
        else
        {
            success = acscriptSys().deferScriptStart(mapUri, scriptNumber, scriptArgs);
        }
        break; }

    case 81: /* ACS_Suspend */ {
        int const scriptNumber = args[0];
        if(acscriptSys().hasScript(scriptNumber))
        {
            success = acscriptSys().script(scriptNumber).suspend();
        }
        break; }

    case 82: /* ACS_Terminate */ {
        int const scriptNumber = args[0];
        if(acscriptSys().hasScript(scriptNumber))
        {
            success = acscriptSys().script(scriptNumber).terminate();
        }
        break; }

    case 90: // Poly Rotate Left Override
        success = EV_RotatePoly(line, args, 1, true);
        break;

    case 91: // Poly Rotate Right Override
        success = EV_RotatePoly(line, args, -1, true);
        break;

    case 92: // Poly Move Override
        success = EV_MovePoly(line, args, false, true);
        break;

    case 93: // Poly Move Times 8 Override
        success = EV_MovePoly(line, args, true, true);
        break;

    case 94: // Build Pillar Crush
        success = EV_BuildPillar(line, args, true);
        break;

    case 95: // Lower Floor and Ceiling
        success = EV_DoFloorAndCeiling(line, args, FT_LOWERBYVALUE, CT_LOWERBYVALUE);
        break;

    case 96: // Raise Floor and Ceiling
        success = EV_DoFloorAndCeiling(line, args, FT_RAISEFLOORBYVALUE, CT_RAISEBYVALUE);
        break;

    case 109: // Force Lightning
        success = true;
        ::lightningAnimator.triggerFlash();
        break;

    case 110: // Light Raise by Value
        success = EV_SpawnLight(line, args, LITE_RAISEBYVALUE);
        break;

    case 111: // Light Lower by Value
        success = EV_SpawnLight(line, args, LITE_LOWERBYVALUE);
        break;

    case 112: // Light Change to Value
        success = EV_SpawnLight(line, args, LITE_CHANGETOVALUE);
        break;

    case 113: // Light Fade
        success = EV_SpawnLight(line, args, LITE_FADE);
        break;

    case 114: // Light Glow
        success = EV_SpawnLight(line, args, LITE_GLOW);
        break;

    case 115: // Light Flicker
        success = EV_SpawnLight(line, args, LITE_FLICKER);
        break;

    case 116: // Light Strobe
        success = EV_SpawnLight(line, args, LITE_STROBE);
        break;

    case 120: // Quake Tremor
        success = A_LocalQuake(args, mo);
        break;

    case 129: // UsePuzzleItem
        success = EV_LineSearchForPuzzleItem(line, args, mo);
        break;

    case 130: // Thing_Activate
        success = EV_ThingActivate(args[0]);
        break;

    case 131: // Thing_Deactivate
        success = EV_ThingDeactivate(args[0]);
        break;

    case 132: // Thing_Remove
        success = EV_ThingRemove(args[0]);
        break;

    case 133: // Thing_Destroy
        success = EV_ThingDestroy(args[0]);
        break;

    case 134: // Thing_Projectile
        success = EV_ThingProjectile(args, 0);
        break;

    case 135: // Thing_Spawn
        if (isThingSpawnEventAllowed())
        {
            success = EV_ThingSpawn(args, 1);
        }
        else
        {
            success = 1;
        }
        break;

    case 136: // Thing_ProjectileGravity
        success = EV_ThingProjectile(args, 1);
        break;

    case 137: // Thing_SpawnNoFog
        if (isThingSpawnEventAllowed())
        {
            success = EV_ThingSpawn(args, 0);
        }
        else
        {
            success = 1;
        }
        break;

    case 138: // Floor_Waggle
        success = EV_StartFloorWaggle(args[0], args[1], args[2], args[3], args[4]);
        break;

    case 140: // Sector_SoundChange
        success = EV_SectorSoundChange(args);
        break;

    default:
        break;
    }

    return success;
}

dd_bool P_ActivateLine(Line *line, mobj_t *mo, int side, int activationType)
{
    // Clients do not activate lines.
    if(IS_CLIENT) return false;

    xline_t *xline           = P_ToXLine(line);
    int const lineActivation = GET_SPAC(xline->flags);
    if(lineActivation != activationType)
        return false;

    if(!mo->player && !(mo->flags & MF_MISSILE))
    {
        // Currently, monsters can only activate the MCROSS activation type.
        if(lineActivation != SPAC_MCROSS) return false;

        // Never DT_OPEN secret doors
        if(xline->flags & ML_SECRET) return false;
    }

    bool const repeat        = ((xline->flags & ML_REPEAT_SPECIAL)? true : false);
    bool const buttonSuccess = P_ExecuteLineSpecial(xline->special, &xline->arg1, line, side, mo);

    if(!repeat && buttonSuccess)
    {
        // Clear the special on non-retriggerable lines.
        xline->special = 0;
    }

    if((lineActivation == SPAC_USE || lineActivation == SPAC_IMPACT) &&
       buttonSuccess)
    {
        P_ToggleSwitch((Side *)P_GetPtrp(line, DMU_FRONT), 0, false,
                       repeat? BUTTONTIME : 0);
    }

    return true;
}

/**
 * @note Called every tic frame that the player origin is in a special sector.
 */
void P_PlayerInSpecialSector(player_t *player)
{
    DENG2_ASSERT(player);
    static coord_t const pushTab[3] = {
        1.0 / 32 * 5,
        1.0 / 32 * 10,
        1.0 / 32 * 25
    };

    Sector *sec = Mobj_Sector(player->plr->mo);
    if(!de::fequal(player->plr->mo->origin[VZ], P_GetDoublep(sec, DMU_FLOOR_HEIGHT)))
        return; // Player is not touching the floor

    xsector_t *xsec = P_ToXSector(sec);
    switch(xsec->special)
    {
    case 9: // SecretArea
        if(!IS_CLIENT)
        {
            player->secretCount++;
            player->update |= PSF_COUNTERS;
            xsec->special = 0;
        }
        break;

    case 201:
    case 202:
    case 203: // Scroll_North_xxx
        P_Thrust(player, ANG90, pushTab[xsec->special - 201]);
        break;

    case 204:
    case 205:
    case 206: // Scroll_East_xxx
        P_Thrust(player, 0, pushTab[xsec->special - 204]);
        break;

    case 207:
    case 208:
    case 209: // Scroll_South_xxx
        P_Thrust(player, ANG270, pushTab[xsec->special - 207]);
        break;

    case 210:
    case 211:
    case 212: // Scroll_West_xxx
        P_Thrust(player, ANG180, pushTab[xsec->special - 210]);
        break;

    case 213:
    case 214:
    case 215: // Scroll_NorthWest_xxx
        P_Thrust(player, ANG90 + ANG45, pushTab[xsec->special - 213]);
        break;

    case 216:
    case 217:
    case 218: // Scroll_NorthEast_xxx
        P_Thrust(player, ANG45, pushTab[xsec->special - 216]);
        break;

    case 219:
    case 220:
    case 221: // Scroll_SouthEast_xxx
        P_Thrust(player, ANG270 + ANG45, pushTab[xsec->special - 219]);
        break;

    case 222:
    case 223:
    case 224: // Scroll_SouthWest_xxx
        P_Thrust(player, ANG180 + ANG45, pushTab[xsec->special - 222]);
        break;

    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
    case 51:
        // Wind specials are handled in (P_mobj):P_MobjMoveXY
        break;

    case 26: // Stairs_Special1
    case 27: // Stairs_Special2
        // Used in (P_floor):ProcessStairSector
        break;

    case 198: // Lightning Special
    case 199: // Lightning Flash special
    case 200: // Sky2
        // Used in (R_plane):R_Drawplanes
        break;

    default:
        break;
    }
}

void P_PlayerOnSpecialFloor(player_t *player)
{
    DENG2_ASSERT(player);
    mobj_t *plrMo           = player->plr->mo;
    terraintype_t const *tt = P_MobjFloorTerrain(plrMo);
    DENG2_ASSERT(tt);

    if(!(tt->flags & TTF_DAMAGING))
        return;

    if(plrMo->origin[VZ] > P_GetDoublep(Mobj_Sector(plrMo), DMU_FLOOR_HEIGHT))
    {
        return; // Player is not touching the floor
    }

    if(!(mapTime & 31))
    {
        P_DamageMobj(plrMo, P_LavaInflictor(), nullptr, 10, false);
        S_StartSound(SFX_LAVA_SIZZLE, plrMo);
    }
}

void P_SpawnSectorSpecialThinkers()
{
    // Clients do not spawn sector specials.
    if(IS_CLIENT) return;

    for(int i = 0; i < numsectors; ++i)
    {
        Sector *sec     = (Sector *)P_ToPtr(DMU_SECTOR, i);
        xsector_t *xsec = P_ToXSector(sec);

        // XG sector types override the game's built-in types.
        //if(xsec->xg) continue;

        switch(xsec->special)
        {
        default: break;

        case 1: ///< Phased light.
            // Static base, use sector->lightLevel as the index.
            P_SpawnPhasedLight(sec, (80.f / 255.0f), -1);
            break;

        case 2: ///< Phased light sequence start.
            P_SpawnLightSequence(sec, 1);
            break;
        }
    }
}

void P_SpawnLineSpecialThinkers()
{
    // Stub.
}

void P_SpawnAllSpecialThinkers()
{
    P_SpawnSectorSpecialThinkers();
    P_SpawnLineSpecialThinkers();
}

void P_InitLightning()
{
    ::lightningAnimator.initForMap();
}

void P_AnimateLightning()
{
    ::lightningAnimator.advanceTime();
}

dd_bool P_StartACScript(int scriptNumber, byte const args[],
    mobj_t *activator, Line *line, int side)
{
    if(acscriptSys().hasScript(scriptNumber))
    {
        return acscriptSys().script(scriptNumber)
                                .start(acs::Script::Args(args, 4), activator, line, side);
    }
    return false;
}
