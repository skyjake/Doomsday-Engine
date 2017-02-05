/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * acfnlink.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

actionlink_t actionlinks[] = {
    {"A_AccTeleGlitter",    A_AccTeleGlitter},
    {"A_AddPlayerCorpse",   A_AddPlayerCorpse},
    {"A_AddPlayerRain",     A_AddPlayerRain},
    {"A_BeakAttackPL1",     A_BeakAttackPL1},
    {"A_BeakAttackPL2",     A_BeakAttackPL2},
    {"A_BeakRaise",         A_BeakRaise},
    {"A_BeakReady",         A_BeakReady},
    {"A_BeastAttack",       A_BeastAttack},
    {"A_BeastPuff",         A_BeastPuff},
    {"A_BlueSpark",         A_BlueSpark},
    {"A_BoltSpark",         A_BoltSpark},
    {"A_BossDeath",         A_BossDeath},
    {"A_Chase",             A_Chase},
    {"A_CheckBurnGone",     A_CheckBurnGone},
    {"A_CheckSkullDone",    A_CheckSkullDone},
    {"A_CheckSkullFloor",   A_CheckSkullFloor},
    {"A_ChicAttack",        A_ChicAttack},
    {"A_ChicChase",         A_ChicChase},
    {"A_ChicLook",          A_ChicLook},
    {"A_ChicPain",          A_ChicPain},
    {"A_ClinkAttack",       A_ClinkAttack},
    {"A_ContMobjSound",     A_ContMobjSound},
    {"A_DeathBallImpact",   A_DeathBallImpact},
    {"A_DripBlood",         A_DripBlood},
    {"A_ESound",            A_ESound},
    {"A_Explode",           A_Explode},
    {"A_FaceTarget",        A_FaceTarget},
    {"A_Feathers",          A_Feathers},
    {"A_FireBlasterPL1",    A_FireBlasterPL1},
    {"A_FireBlasterPL2",    A_FireBlasterPL2},
    {"A_FireCrossbowPL1",   A_FireCrossbowPL1},
    {"A_FireCrossbowPL2",   A_FireCrossbowPL2},
    {"A_FireGoldWandPL1",   A_FireGoldWandPL1},
    {"A_FireGoldWandPL2",   A_FireGoldWandPL2},
    {"A_FireMacePL1",       A_FireMacePL1},
    {"A_FireMacePL2",       A_FireMacePL2},
    {"A_FirePhoenixPL1",    A_FirePhoenixPL1},
    {"A_FirePhoenixPL2",    A_FirePhoenixPL2},
    {"A_FireSkullRodPL1",   A_FireSkullRodPL1},
    {"A_FireSkullRodPL2",   A_FireSkullRodPL2},
    {"A_FlameEnd",          A_FlameEnd},
    {"A_FlameSnd",          A_FlameSnd},
    {"A_FloatPuff",         A_FloatPuff},
    {"A_FreeTargMobj",      A_FreeTargMobj},
    {"A_GauntletAttack",    A_GauntletAttack},
    {"A_GenWizard",         A_GenWizard},
    {"A_GhostOff",          A_GhostOff},
    {"A_HeadAttack",        A_HeadAttack},
    {"A_HeadFireGrow",      A_HeadFireGrow},
    {"A_HeadIceImpact",     A_HeadIceImpact},
    {"A_HideInCeiling",     A_HideInCeiling},
    {"A_HideThing",         A_HideThing},
    {"A_ImpDeath",          A_ImpDeath},
    {"A_ImpExplode",        A_ImpExplode},
    {"A_ImpMeAttack",       A_ImpMeAttack},
    {"A_ImpMsAttack",       A_ImpMsAttack},
    {"A_ImpMsAttack2",      A_ImpMsAttack2},
    {"A_ImpXDeath1",        A_ImpXDeath1},
    {"A_ImpXDeath2",        A_ImpXDeath2},
    {"A_InitKeyGizmo",      A_InitKeyGizmo},
    {"A_InitPhoenixPL2",    A_InitPhoenixPL2},
    {"A_KnightAttack",      A_KnightAttack},
    {"A_Light0",            A_Light0},
    {"A_Look",              A_Look},
    {"A_Lower",             A_Lower},
    {"A_MaceBallImpact",    A_MaceBallImpact},
    {"A_MaceBallImpact2",   A_MaceBallImpact2},
    {"A_MacePL1Check",      A_MacePL1Check},
    {"A_MakePod",           A_MakePod},
    {"A_MinotaurAtk1",      A_MinotaurAtk1},
    {"A_MinotaurAtk2",      A_MinotaurAtk2},
    {"A_MinotaurAtk3",      A_MinotaurAtk3},
    {"A_MinotaurCharge",    A_MinotaurCharge},
    {"A_MinotaurDecide",    A_MinotaurDecide},
    {"A_MntrFloorFire",     A_MntrFloorFire},
    {"A_MummyAttack",       A_MummyAttack},
    {"A_MummyAttack2",      A_MummyAttack2},
    {"A_MummyFX1Seek",      A_MummyFX1Seek},
    {"A_MummySoul",         A_MummySoul},
    {"A_NoBlocking",        A_NoBlocking},
    {"A_Pain",              A_Pain},
    {"A_PhoenixPuff",       A_PhoenixPuff},
    {"A_PodPain",           A_PodPain},
    {"A_RainImpact",        A_RainImpact},
    {"A_Raise",             A_Raise},
    {"A_ReFire",            A_ReFire},
    {"A_RemovePod",         A_RemovePod},
    {"A_RestoreArtifact",   A_RestoreArtifact},
    {"A_RestoreSpecialThing1", A_RestoreSpecialThing1},
    {"A_RestoreSpecialThing2", A_RestoreSpecialThing2},
    {"A_Scream",            A_Scream},
    {"A_ShutdownPhoenixPL2", A_ShutdownPhoenixPL2},
    {"A_SkullPop",          A_SkullPop},
    {"A_SkullRodPL2Seek",   A_SkullRodPL2Seek},
    {"A_SkullRodStorm",     A_SkullRodStorm},
    {"A_SnakeAttack",       A_SnakeAttack},
    {"A_SnakeAttack2",      A_SnakeAttack2},
    {"A_Sor1Chase",         A_Sor1Chase},
    {"A_Sor1Pain",          A_Sor1Pain},
    {"A_Sor2DthInit",       A_Sor2DthInit},
    {"A_Sor2DthLoop",       A_Sor2DthLoop},
    {"A_SorcererRise",      A_SorcererRise},
    {"A_SorDBon",           A_SorDBon},
    {"A_SorDExp",           A_SorDExp},
    {"A_SorDSph",           A_SorDSph},
    {"A_SorRise",           A_SorRise},
    {"A_SorSightSnd",       A_SorSightSnd},
    {"A_SorZap",            A_SorZap},
    {"A_SpawnRippers",      A_SpawnRippers},
    {"A_SpawnTeleGlitter",  A_SpawnTeleGlitter},
    {"A_SpawnTeleGlitter2", A_SpawnTeleGlitter2},
    {"A_Srcr1Attack",       A_Srcr1Attack},
    {"A_Srcr2Attack",       A_Srcr2Attack},
    {"A_Srcr2Decide",       A_Srcr2Decide},
    {"A_StaffAttackPL1",    A_StaffAttackPL1},
    {"A_StaffAttackPL2",    A_StaffAttackPL2},
    {"A_UnHideThing",       A_UnHideThing},
    {"A_VolcanoBlast",      A_VolcanoBlast},
    {"A_VolcanoSet",        A_VolcanoSet},
    {"A_VolcBallImpact",    A_VolcBallImpact},
    {"A_WeaponReady",       A_WeaponReady},
    {"A_WhirlwindSeek",     A_WhirlwindSeek},
    {"A_WizAtk1",           A_WizAtk1},
    {"A_WizAtk2",           A_WizAtk2},
    {"A_WizAtk3",           A_WizAtk3},
    // Inventory:
    {"A_FireBomb",          A_FireBomb},
    {"A_TombOfPower",       A_TombOfPower},
    {"A_Egg",               A_Egg},
    {"A_Wings",             A_Wings},
    {"A_Teleport",          A_Teleport},
    {"A_Torch",             A_Torch},
    {"A_Health",            A_Health},
    {"A_SuperHealth",       A_SuperHealth},
    {"A_Invisibility",      A_Invisibility},
    {"A_Invulnerability",   A_Invulnerability},
    {0, 0}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------
