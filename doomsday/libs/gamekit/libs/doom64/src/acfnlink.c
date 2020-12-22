/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
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

// HEADER FILES ------------------------------------------------------------

#include "jdoom64.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

actionlink_t actionlinks[] = {
    {"A_BabyMetal",         A_BabyMetal},
    {"A_BFGsound",          A_BFGsound},
    {"A_BFGSpray",          A_BFGSpray},
    {"A_BossDeath",         A_BossDeath},
    {"A_BruisAttack",       A_BruisAttack},
    {"A_BspiAttack",        A_BspiAttack},
    {"A_Chase",             A_Chase},
    {"A_Chase",             A_RectChase}, //jd64
    {"A_CheckReload",       A_CheckReload},
    {"A_CyberAttack",       A_CyberAttack},
    {"A_Explode",           A_Explode},
    {"A_BarrelExplode",     A_BarrelExplode}, //jd64
    {"A_FaceTarget",        A_FaceTarget},
    {"A_BspiFaceTarget",    A_BspiFaceTarget}, //jd64
    {"A_Fall",              A_Fall},
    {"A_FatAttack1",        A_FatAttack1},
    {"A_FatAttack2",        A_FatAttack2},
    {"A_FatAttack3",        A_FatAttack3},
    {"A_FatRaise",          A_FatRaise},
    {"A_FireBFG",           A_FireBFG},
    {"A_FireCGun",          A_FireCGun},
    {"A_FireMissile",       A_FireMissile},
    {"A_FirePistol",        A_FirePistol},
    {"A_FirePlasma",        A_FirePlasma},
    {"A_Lasersmoke",        A_Lasersmoke}, //jd64
    {"A_FireSingleLaser",   A_FireSingleLaser}, //jd64
    {"A_FireDoubleLaser",   A_FireDoubleLaser}, //jd64
    {"A_FireDoubleLaser1",  A_FireDoubleLaser1}, //jd64
    {"A_FireDoubleLaser2",  A_FireDoubleLaser2}, //jd64
    {"A_FireShotgun",       A_FireShotgun},
    {"A_FireShotgun2",      A_FireShotgun2},
    {"A_GunFlash",          A_GunFlash},
    {"A_HeadAttack",        A_HeadAttack},
    {"A_Hoof",              A_Hoof},
    {"A_PossSpecial",       A_PossSpecial}, //jd64
    {"A_SposSpecial",       A_SposSpecial}, //jd64
    {"A_TrooSpecial",       A_TrooSpecial}, //jd64
    {"A_SargSpecial",       A_SargSpecial}, //jd64
    {"A_HeadSpecial",       A_HeadSpecial}, //jd64
    {"A_SkulSpecial",       A_SkulSpecial}, //jd64
    {"A_Bos2Special",       A_Bos2Special}, //jd64
    {"A_BossSpecial",       A_BossSpecial}, //jd64
    {"A_PainSpecial",       A_PainSpecial}, //jd64
    {"A_FattSpecial",       A_FattSpecial}, //jd64
    {"A_BabySpecial",       A_BabySpecial}, //jd64
    {"A_CybrSpecial",       A_CybrSpecial}, //jd64
    {"A_RectSpecial",       A_RectSpecial}, //jd64
    {"A_Light0",            A_Light0},
    {"A_Light1",            A_Light1},
    {"A_Light2",            A_Light2},
    {"A_LoadShotgun2",      A_LoadShotgun2},
    {"A_Look",              A_Look},
    {"A_Lower",             A_Lower},
    {"A_Metal",             A_Metal},
    {"A_OpenShotgun2",      A_OpenShotgun2},
    {"A_Pain",              A_Pain},
    {"A_PainAttack",        A_PainAttack},
    {"A_PainDie",           A_PainDie},
    {"A_Rocketpuff",        A_Rocketpuff}, //jd64
    {"A_PosAttack",         A_PosAttack},
    {"A_Punch",             A_Punch},
    {"A_Raise",             A_Raise},
    {"A_PlasmaShock",       A_PlasmaShock},
    {"A_ReFire",            A_ReFire},
    {"A_SargAttack",        A_SargAttack},
    {"A_Saw",               A_Saw},
    {"A_Scream",            A_Scream},
    {"A_CyberDeath",        A_CyberDeath}, //jd64
    {"A_SkelFist",          A_SkelFist},
    {"A_SkelMissile",       A_SkelMissile},
    {"A_SkelWhoosh",        A_SkelWhoosh},
    {"A_SkullAttack",       A_SkullAttack},
    {"A_SpidRefire",        A_SpidRefire},
    {"A_SPosAttack",        A_SPosAttack},
    {"A_Tracer",            A_Tracer},
    {"A_TroopAttack",       A_TroopAttack},
    {"A_TroopClaw",         A_TroopClaw}, //jd64
    {"A_MotherFloorFire",   A_MotherFloorFire}, //jd64
    {"A_MotherMissle",      A_MotherMissle}, //jd64
    {"A_SetFloorFire",      A_SetFloorFire}, //jd64
    {"A_MotherBallExplode", A_MotherBallExplode}, //jd64
    {"A_RectTracerPuff",    A_RectTracerPuff}, //jd64
    {"A_WeaponReady",       A_WeaponReady},
    {"A_XScream",           A_XScream},
    {"A_TargetCamera",      A_TargetCamera}, //jd64
    {"A_ShadowsAction1",    A_ShadowsAction1}, //jd64
    {"A_ShadowsAction2",    A_ShadowsAction2}, //jd64
    {"A_EMarineAttack2",    A_EMarineAttack2}, //jd64
    {0, 0}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------
