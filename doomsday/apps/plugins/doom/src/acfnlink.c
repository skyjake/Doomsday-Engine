/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * acfnlink.c: Global action function (AI routines) link table.
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

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
    {"A_BrainAwake",        A_BrainAwake},
    {"A_BrainDie",          A_BrainDie},
    {"A_BrainExplode",      A_BrainExplode},
    {"A_BrainPain",         A_BrainPain},
    {"A_BrainScream",       A_BrainScream},
    {"A_BrainSpit",         A_BrainSpit},
    {"A_BruisAttack",       A_BruisAttack},
    {"A_BspiAttack",        A_BspiAttack},
    {"A_Chase",             A_Chase},
    {"A_CheckReload",       A_CheckReload},
    {"A_CloseShotgun2",     A_CloseShotgun2},
    {"A_CPosAttack",        A_CPosAttack},
    {"A_CPosRefire",        A_CPosRefire},
    {"A_CyberAttack",       A_CyberAttack},
    {"A_Explode",           A_Explode},
    {"A_FaceTarget",        A_FaceTarget},
    {"A_Fall",              A_Fall},
    {"A_FatAttack1",        A_FatAttack1},
    {"A_FatAttack2",        A_FatAttack2},
    {"A_FatAttack3",        A_FatAttack3},
    {"A_FatRaise",          A_FatRaise},
    {"A_Fire",              A_Fire},
    {"A_FireBFG",           A_FireBFG},
    {"A_FireCGun",          A_FireCGun},
    {"A_FireCrackle",       A_FireCrackle},
    {"A_FireMissile",       A_FireMissile},
    {"A_FirePistol",        A_FirePistol},
    {"A_FirePlasma",        A_FirePlasma},
    {"A_FireShotgun",       A_FireShotgun},
    {"A_FireShotgun2",      A_FireShotgun2},
    {"A_GunFlash",          A_GunFlash},
    {"A_HeadAttack",        A_HeadAttack},
    {"A_Hoof",              A_Hoof},
    {"A_KeenDie",           A_KeenDie},
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
    {"A_PlayerScream",      A_PlayerScream},
    {"A_PosAttack",         A_PosAttack},
    {"A_Punch",             A_Punch},
    {"A_Raise",             A_Raise},
    {"A_ReFire",            A_ReFire},
    {"A_SargAttack",        A_SargAttack},
    {"A_Saw",               A_Saw},
    {"A_Scream",            A_Scream},
    {"A_SkelFist",          A_SkelFist},
    {"A_SkelMissile",       A_SkelMissile},
    {"A_SkelWhoosh",        A_SkelWhoosh},
    {"A_SkullAttack",       A_SkullAttack},
    {"A_SpawnFly",          A_SpawnFly},
    {"A_SpawnSound",        A_SpawnSound},
    {"A_SpidRefire",        A_SpidRefire},
    {"A_SPosAttack",        A_SPosAttack},
    {"A_StartFire",         A_StartFire},
    {"A_Tracer",            A_Tracer},
    {"A_TroopAttack",       A_TroopAttack},
    {"A_VileAttack",        A_VileAttack},
    {"A_VileChase",         A_VileChase},
    {"A_VileStart",         A_VileStart},
    {"A_VileTarget",        A_VileTarget},
    {"A_WeaponReady",       A_WeaponReady},
    {"A_XScream",           A_XScream},
    {0, 0}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------
