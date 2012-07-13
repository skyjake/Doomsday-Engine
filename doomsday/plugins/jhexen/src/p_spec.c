/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_spec.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <stdio.h>

#include "jhexen.h"

#include "dmu_lib.h"
#include "g_common.h"
#include "p_inventory.h"
#include "p_player.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_plat.h"
#include "p_floor.h"
#include "p_switch.h"
#include "p_user.h"

// MACROS ------------------------------------------------------------------

#define LIGHTNING_SPECIAL       198
#define LIGHTNING_SPECIAL2      199
#define SKYCHANGE_SPECIAL       200

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_LightningFlash(void);
static boolean CheckedLockedDoor(mobj_t* mo, byte lock);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

mobj_t lavaInflictor;

materialid_t sky1Material;
materialid_t sky2Material;
float sky1ColumnOffset;
float sky2ColumnOffset;
float sky1ScrollDelta;
float sky2ScrollDelta;
boolean doubleSky;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean mapHasLightning;
static int nextLightningFlash;
static int lightningFlash;
static float* lightningLightLevels;

// CODE --------------------------------------------------------------------

void P_InitLava(void)
{
    memset(&lavaInflictor, 0, sizeof(mobj_t));

    lavaInflictor.type = MT_CIRCLEFLAME;
    lavaInflictor.flags2 = MF2_FIREDAMAGE | MF2_NODMGTHRUST;
}

void P_InitSky(uint map)
{
    int                 ival;
    float               fval;

    sky1Material = P_GetMapSky1Material(map);
    sky2Material = P_GetMapSky2Material(map);
    sky1ScrollDelta = P_GetMapSky1ScrollDelta(map);
    sky2ScrollDelta = P_GetMapSky2ScrollDelta(map);
    sky1ColumnOffset = 0;
    sky2ColumnOffset = 0;
    doubleSky = P_GetMapDoubleSky(map);

    // First disable all sky layers.
    R_SkyParams(DD_SKY, DD_DISABLE, NULL);

    // Sky2 is layer zero and Sky1 is layer one.
    fval = 0;
    R_SkyParams(0, DD_OFFSET, &fval);
    R_SkyParams(1, DD_OFFSET, &fval);
    if(doubleSky && sky2Material)
    {
        R_SkyParams(0, DD_ENABLE, NULL);
        ival = DD_NO;
        R_SkyParams(0, DD_MASK, &ival);
        R_SkyParams(0, DD_MATERIAL, &sky2Material);

        R_SkyParams(1, DD_ENABLE, NULL);
        ival = DD_YES;
        R_SkyParams(1, DD_MASK, &ival);
        R_SkyParams(1, DD_MATERIAL, &sky1Material);
    }
    else
    {
        R_SkyParams(0, DD_ENABLE, NULL);
        ival = DD_NO;
        R_SkyParams(0, DD_MASK, &ival);
        R_SkyParams(0, DD_MATERIAL, &sky1Material);

        R_SkyParams(1, DD_DISABLE, NULL);
        ival = DD_NO;
        R_SkyParams(1, DD_MASK, &ival);
        R_SkyParams(1, DD_MATERIAL, &sky2Material);
    }
}

boolean EV_SectorSoundChange(byte* args)
{
    boolean             rtn = false;
    Sector*             sec = NULL;
    iterlist_t*         list;

    if(!args[0])
        return false;

    list = P_GetSectorIterListForTag((int) args[0], false);
    if(!list)
        return rtn;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);
    while((sec = IterList_MoveIterator(list)) != NULL)
    {
        P_ToXSector(sec)->seqType = args[1];
        rtn = true;
    }

    return rtn;
}

static boolean CheckedLockedDoor(mobj_t* mo, byte lock)
{
    extern int  TextKeyMessages[11];
    char        LockedBuffer[80];

    if(!mo->player)
        return false;

    if(!lock)
        return true;

    if(!(mo->player->keys & (1 << (lock - 1))))
    {
        sprintf(LockedBuffer, "YOU NEED THE %s\n",
                GET_TXT(TextKeyMessages[lock - 1]));

        P_SetMessage(mo->player, LockedBuffer, false);
        S_StartSound(SFX_DOOR_LOCKED, mo);
        return false;
    }

    return true;
}

boolean EV_LineSearchForPuzzleItem(LineDef* line, byte* args, mobj_t* mo)
{
    inventoryitemtype_t  type;

    if(!mo || !mo->player || !line)
        return false;

    type = IIT_FIRSTPUZZITEM + P_ToXLine(line)->arg1;

    if(type < IIT_FIRSTPUZZITEM)
        return false;

    // Search player's inventory for puzzle items
    return P_InventoryUse(mo->player - players, type, false);
}

boolean P_ExecuteLineSpecial(int special, byte* args, LineDef* line,
                             int side, mobj_t* mo)
{
    boolean             success;

    success = false;
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
                G_LeaveMap((args[0]!= 0? args[0]-1 : 0), args[1], false);
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
                if(deathmatch)
                {
                    // Winning in deathmatch just goes back to map 1
                    G_LeaveMap(0, 0, false);
                }
                else
                {
                    // Passing DDMAXINT, DDMAXINT to G_LeaveMap() starts the Finale
                    G_LeaveMap(DDMAXINT, DDMAXINT, false);
                }
            }
        }
        break;

    case 80: // ACS_Execute
        success = P_StartACS(args[0], args[1], &args[2], mo, line, side);
        break;

    case 81: // ACS_Suspend
        success = P_SuspendACS(args[0], args[1]);
        break;

    case 82: // ACS_Terminate
        success = P_TerminateACS(args[0], args[1]);
        break;

    case 83: // ACS_LockedExecute
        success = P_StartLockedACS(line, args, mo, side);
        break;

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
        P_ForceLightning();
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
        success = EV_ThingSpawn(args, 1);
        break;

    case 136: // Thing_ProjectileGravity
        success = EV_ThingProjectile(args, 1);
        break;

    case 137: // Thing_SpawnNoFog
        success = EV_ThingSpawn(args, 0);
        break;

    case 138: // Floor_Waggle
        success =
            EV_StartFloorWaggle(args[0], args[1], args[2], args[3], args[4]);
        break;

    case 140: // Sector_SoundChange
        success = EV_SectorSoundChange(args);
        break;

    default:
        break;
    }

    return success;
}

boolean P_ActivateLine(LineDef *line, mobj_t *mo, int side, int activationType)
{
    int             lineActivation;
    boolean         repeat;
    boolean         buttonSuccess;
    xline_t        *xline = P_ToXLine(line);

    if(IS_CLIENT)
    {
        // Clients do not activate lines.
        return false;
    }

    lineActivation = GET_SPAC(xline->flags);
    if(lineActivation != activationType)
        return false;

    if(!mo->player && !(mo->flags & MF_MISSILE))
    {
        if(lineActivation != SPAC_MCROSS)
        {   // currently, monsters can only activate the MCROSS activation type
            return false;
        }

        if(xline->flags & ML_SECRET)
            return false; // never DT_OPEN secret doors
    }

    repeat = ((xline->flags & ML_REPEAT_SPECIAL)? true : false);

    buttonSuccess =
        P_ExecuteLineSpecial(xline->special, &xline->arg1, line, side, mo);
    if(!repeat && buttonSuccess)
    {
        // clear the special on non-retriggerable lines
        xline->special = 0;
    }

    if((lineActivation == SPAC_USE || lineActivation == SPAC_IMPACT) &&
       buttonSuccess)
    {
        P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), 0, false,
                       repeat? BUTTONTIME : 0);
    }

    return true;
}

/**
 * Called every tic frame that the player origin is in a special sector.
 */
void P_PlayerInSpecialSector(player_t* player)
{
    static const coord_t pushTab[3] = {
        1.0 / 32 * 5,
        1.0 / 32 * 10,
        1.0 / 32 * 25
    };
    Sector* sector = P_GetPtrp(player->plr->mo->bspLeaf, DMU_SECTOR);
    xsector_t* xsector;

    if(!FEQUAL(player->plr->mo->origin[VZ], P_GetDoublep(sector, DMU_FLOOR_HEIGHT)))
        return; // Player is not touching the floor

    xsector = P_ToXSector(sector);
    switch(xsector->special)
    {
    case 9: // SecretArea
        if(!IS_CLIENT)
        {
            player->secretCount++;
            xsector->special = 0;
        }
        break;

    case 201:
    case 202:
    case 203: // Scroll_North_xxx
        P_Thrust(player, ANG90, pushTab[xsector->special - 201]);
        break;

    case 204:
    case 205:
    case 206: // Scroll_East_xxx
        P_Thrust(player, 0, pushTab[xsector->special - 204]);
        break;

    case 207:
    case 208:
    case 209: // Scroll_South_xxx
        P_Thrust(player, ANG270, pushTab[xsector->special - 207]);
        break;

    case 210:
    case 211:
    case 212: // Scroll_West_xxx
        P_Thrust(player, ANG180, pushTab[xsector->special - 210]);
        break;

    case 213:
    case 214:
    case 215: // Scroll_NorthWest_xxx
        P_Thrust(player, ANG90 + ANG45, pushTab[xsector->special - 213]);
        break;

    case 216:
    case 217:
    case 218: // Scroll_NorthEast_xxx
        P_Thrust(player, ANG45, pushTab[xsector->special - 216]);
        break;

    case 219:
    case 220:
    case 221: // Scroll_SouthEast_xxx
        P_Thrust(player, ANG270 + ANG45, pushTab[xsector->special - 219]);
        break;

    case 222:
    case 223:
    case 224: // Scroll_SouthWest_xxx
        P_Thrust(player, ANG180 + ANG45, pushTab[xsector->special - 222]);
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

void P_PlayerOnSpecialFloor(player_t* player)
{
    const terraintype_t* tt = P_MobjGetFloorTerrainType(player->plr->mo);

    if(!(tt->flags & TTF_DAMAGING))
        return;

    if(player->plr->mo->origin[VZ] >
       P_GetDoublep(player->plr->mo->bspLeaf, DMU_FLOOR_HEIGHT))
    {
        return; // Player is not touching the floor
    }

    if(!(mapTime & 31))
    {
        P_DamageMobj(player->plr->mo, &lavaInflictor, NULL, 10, false);
        S_StartSound(SFX_LAVA_SIZZLE, player->plr->mo);
    }
}

void P_UpdateSpecials(void)
{
    // Stub.
}

/**
 * After the map has been loaded, scan for specials that spawn thinkers.
 */
void P_SpawnSpecials(void)
{
    uint        i;
    LineDef    *line;
    xline_t    *xline;
    iterlist_t *list;
    Sector     *sec;
    xsector_t  *xsec;

    // Init special SECTORs.
    P_DestroySectorTagLists();
    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        xsec = P_ToXSector(sec);

        if(xsec->tag)
        {
           list = P_GetSectorIterListForTag(xsec->tag, true);
           IterList_Push(list, sec);
        }

        // Clients do not spawn sector specials.
        if(IS_CLIENT)
            break;

        switch(xsec->special)
        {
        case 1: // Phased light
            // Hardcoded base, use sector->lightLevel as the index.
            P_SpawnPhasedLight(sec, (80.f / 255.0f), -1);
            break;

        case 2: // Phased light sequence start.
            P_SpawnLightSequence(sec, 1);
            break;

        default:
            break;
        }
    }

    // Init animating line specials.
    IterList_Empty(linespecials);
    P_DestroyLineTagLists();
    for(i = 0; i < numlines; ++i)
    {
        line = P_ToPtr(DMU_LINEDEF, i);
        xline = P_ToXLine(line);

        switch(xline->special)
        {
        case 100: // Scroll_Texture_Left
        case 101: // Scroll_Texture_Right
        case 102: // Scroll_Texture_Up
        case 103: // Scroll_Texture_Down
            IterList_Push(linespecials, line);
            break;

        case 121: // Line_SetIdentification
            if(xline->arg1)
            {
                list = P_GetLineIterListForTag((int) xline->arg1, true);
                IterList_Push(list, line);
            }
            xline->special = 0;
            break;

        default:
            break;
        }
    }
}

void P_AnimateSurfaces(void)
{
#define PLANE_MATERIAL_SCROLLUNIT (8.f/35*2)

    uint                i;
    LineDef*            line;

    // Update scrolling plane materials.
    for(i = 0; i < numsectors; ++i)
    {
        xsector_t*          sect = P_ToXSector(P_ToPtr(DMU_SECTOR, i));
        float               texOff[2];

        switch(sect->special)
        {
        case 201:
        case 202:
        case 203: // Scroll_North_xxx
            texOff[VY] = P_GetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_Y);
            texOff[VY] -= PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 201);
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_Y, texOff[VY]);
            break;

        case 204:
        case 205:
        case 206: // Scroll_East_xxx
            texOff[VX] = P_GetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_X);
            texOff[VX] -= PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 204);
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_X, texOff[VX]);
            break;

        case 207:
        case 208:
        case 209: // Scroll_South_xxx
            texOff[VY] = P_GetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_Y);
            texOff[VY] += PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 207);
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_Y, texOff[VY]);
            break;

        case 210:
        case 211:
        case 212: // Scroll_West_xxx
            texOff[VX] = P_GetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_X);
            texOff[VX] += PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 210);
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_X, texOff[VX]);
            break;

        case 213:
        case 214:
        case 215: // Scroll_NorthWest_xxx
            P_GetFloatv(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_XY, texOff);
            texOff[VX] += PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 213);
            texOff[VY] -= PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 213);
            P_SetFloatv(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_XY, texOff);
            break;

        case 216:
        case 217:
        case 218: // Scroll_NorthEast_xxx
            P_GetFloatv(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_XY, texOff);
            texOff[VX] -= PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 216);
            texOff[VY] -= PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 216);
            P_SetFloatv(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_XY, texOff);
            break;

        case 219:
        case 220:
        case 221: // Scroll_SouthEast_xxx
            P_GetFloatv(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_XY, texOff);
            texOff[VX] -= PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 219);
            texOff[VY] += PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 219);
            P_SetFloatv(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_XY, texOff);
            break;

        case 222:
        case 223:
        case 224: // Scroll_SouthWest_xxx
            P_GetFloatv(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_XY, texOff);
            texOff[VX] += PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 222);
            texOff[VY] += PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 222);
            P_SetFloatv(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_XY, texOff);
            break;

        default:
            // dj - Is this really necessary every tic?
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_X, 0);
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_Y, 0);
            break;
        }
    }

    // Update scrolling wall materials.
    if(IterList_Size(linespecials))
    {
        IterList_SetIteratorDirection(linespecials, ITERLIST_BACKWARD);
        IterList_RewindIterator(linespecials);
        while((line = IterList_MoveIterator(linespecials)) != NULL)
        {
            SideDef*            side = 0;
            fixed_t             texOff[2];
            xline_t*            xline = P_ToXLine(line);

            side = P_GetPtrp(line, DMU_SIDEDEF0);
            for(i = 0; i < 3; ++i)
            {
                P_GetFixedpv(side,
                             (i==0? DMU_TOP_MATERIAL_OFFSET_XY :
                              i==1? DMU_MIDDLE_MATERIAL_OFFSET_XY :
                              DMU_BOTTOM_MATERIAL_OFFSET_XY), texOff);

                switch(xline->special)
                {
                case 100: // Scroll_Texture_Left
                    texOff[0] += xline->arg1 << 10;
                    break;
                case 101: // Scroll_Texture_Right
                    texOff[0] -= xline->arg1 << 10;
                    break;
                case 102: // Scroll_Texture_Up
                    texOff[1] += xline->arg1 << 10;
                    break;
                case 103: // Scroll_Texture_Down
                    texOff[1] -= xline->arg1 << 10;
                    break;
                default:
                    Con_Error("P_AnimateSurfaces: Invalid line special %i for "
                              "material scroller on linedef %ui.",
                              xline->special, P_ToIndex(line));
                }

                P_SetFixedpv(side,
                             (i==0? DMU_TOP_MATERIAL_OFFSET_XY :
                              i==1? DMU_MIDDLE_MATERIAL_OFFSET_XY :
                              DMU_BOTTOM_MATERIAL_OFFSET_XY), texOff);
            }
        }
    }

    // Update sky column offsets
    sky1ColumnOffset += sky1ScrollDelta;
    sky2ColumnOffset += sky2ScrollDelta;
    R_SkyParams(1, DD_OFFSET, &sky1ColumnOffset);
    R_SkyParams(0, DD_OFFSET, &sky2ColumnOffset);

    if(mapHasLightning)
    {
        if(!nextLightningFlash || lightningFlash)
        {
            P_LightningFlash();
        }
        else
        {
            nextLightningFlash--;
        }
    }

#undef PLANE_MATERIAL_SCROLLUNIT
}

static boolean isLightningSector(Sector* sec)
{
    xsector_t*              xsec = P_ToXSector(sec);

    if(xsec->special == LIGHTNING_SPECIAL ||
       xsec->special == LIGHTNING_SPECIAL2)
        return true;

    if(P_GetIntp(P_GetPtrp(sec, DMU_CEILING_MATERIAL),
                   DMU_FLAGS) & MATF_SKYMASK)
        return true;

    if(P_GetIntp(P_GetPtrp(sec, DMU_FLOOR_MATERIAL),
                   DMU_FLAGS) & MATF_SKYMASK)
        return true;

    return false;
}

static void P_LightningFlash(void)
{
    uint                i;
    float*              tempLight;
    boolean             foundSec;
    float               flashLight;

    if(lightningFlash)
    {
        lightningFlash--;
        tempLight = lightningLightLevels;

        if(lightningFlash)
        {
            for(i = 0; i < numsectors; ++i)
            {
                Sector*                 sec = P_ToPtr(DMU_SECTOR, i);

                if(isLightningSector(sec))
                {
                    float               lightLevel =
                        P_GetFloat(DMU_SECTOR, i, DMU_LIGHT_LEVEL);

                    if(*tempLight < lightLevel - (4.f / 255))
                        P_SetFloat(DMU_SECTOR, i, DMU_LIGHT_LEVEL,
                                   lightLevel - (1.f / 255) * 4);

                    tempLight++;
                }
            }
        }
        else
        {   // Remove the alternate lightning flash special.
            for(i = 0; i < numsectors; ++i)
            {
                Sector*                 sec = P_ToPtr(DMU_SECTOR, i);

                if(isLightningSector(sec))
                {
                    P_SetFloatp(sec, DMU_LIGHT_LEVEL, *tempLight);
                    tempLight++;
                }
            }

            R_SkyParams(1, DD_DISABLE, NULL);
            R_SkyParams(0, DD_ENABLE, NULL);
        }

        return;
    }

    lightningFlash = (P_Random() & 7) + 8;
    flashLight = (float) (200 + (P_Random() & 31)) / 255.0f;
    tempLight = lightningLightLevels;
    foundSec = false;
    for(i = 0; i < numsectors; ++i)
    {
        Sector*             sec = P_ToPtr(DMU_SECTOR, i);

        if(isLightningSector(sec))
        {
            xsector_t*          xsec = P_ToXSector(sec);
            float               newLevel = P_GetFloatp(sec, DMU_LIGHT_LEVEL);

            *tempLight = newLevel;

            if(xsec->special == LIGHTNING_SPECIAL)
            {
                newLevel += .25f;
                if(newLevel > flashLight)
                    newLevel = flashLight;
            }
            else if(xsec->special == LIGHTNING_SPECIAL2)
            {
                newLevel += .125f;
                if(newLevel > flashLight)
                    newLevel = flashLight;
            }
            else
            {
                newLevel = flashLight;
            }

            if(newLevel < *tempLight)
                newLevel = *tempLight;

            P_SetFloatp(sec, DMU_LIGHT_LEVEL, newLevel);
            tempLight++;
            foundSec = true;
        }
    }

    if(foundSec)
    {
        mobj_t*             plrmo = players[DISPLAYPLAYER].plr->mo;
        mobj_t*             crashOrigin = NULL;

        // Set the alternate (lightning) sky.
        R_SkyParams(0, DD_DISABLE, NULL);
        R_SkyParams(1, DD_ENABLE, NULL);

        // If 3D sounds are active, position the clap somewhere above
        // the player.
        if(cfg.snd3D && plrmo && !IS_NETGAME)
        {
            if((crashOrigin =
                P_SpawnMobjXYZ(plrmo->origin[VX] + (16 * (M_Random() - 127) << FRACBITS),
                              plrmo->origin[VY] + (16 * (M_Random() - 127) << FRACBITS),
                              plrmo->origin[VZ] + (4000 << FRACBITS), MT_CAMERA,
                              0, 0)))
                crashOrigin->tics = 5 * TICSPERSEC; // Five seconds will do.
        }

        // Make it loud!
        S_StartSound(SFX_THUNDER_CRASH | DDSF_NO_ATTENUATION, crashOrigin);
    }

    // Calculate the next lighting flash
    if(!nextLightningFlash)
    {
        if(P_Random() < 50)
        {   // Immediate Quick flash
            nextLightningFlash = (P_Random() & 15) + 16;
        }
        else
        {
            if(P_Random() < 128 && !(mapTime & 32))
            {
                nextLightningFlash = ((P_Random() & 7) + 2) * TICSPERSEC;
            }
            else
            {
                nextLightningFlash = ((P_Random() & 15) + 5) * TICSPERSEC;
            }
        }
    }
}

void P_ForceLightning(void)
{
    nextLightningFlash = 0;
}

void P_InitLightning(void)
{
    uint                i, secCount;

    if(!P_GetMapLightning(gameMap))
    {
        mapHasLightning = false;
        lightningFlash = 0;
        return;
    }

    lightningFlash = 0;
    secCount = 0;
    for(i = 0; i < numsectors; ++i)
    {
        Sector*             sec = P_ToPtr(DMU_SECTOR, i);

        if(isLightningSector(sec))
        {
            secCount++;
        }
    }

    if(secCount > 0)
    {
        mapHasLightning = true;

        lightningLightLevels =
            Z_Malloc(secCount * sizeof(float), PU_MAP, NULL);

        // Don't flash immediately on entering the map.
        nextLightningFlash = ((P_Random() & 15) + 5) * TICSPERSEC;
    }
    else
    {
        mapHasLightning = false;
    }
}
