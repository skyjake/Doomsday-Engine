/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * acfnlink.h:
 */

#ifndef __ACTION_LINK_H__
#define __ACTION_LINK_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "dd_share.h"

DE_EXTERN_C actionlink_t actionlinks[];

void C_DECL     A_AccTeleGlitter();
void C_DECL     A_AddPlayerCorpse();
void C_DECL     A_AddPlayerRain();
void C_DECL     A_BeakAttackPL1();
void C_DECL     A_BeakAttackPL2();
void C_DECL     A_BeakRaise();
void C_DECL     A_BeakReady();
void C_DECL     A_BeastAttack();
void C_DECL     A_BeastPuff();
void C_DECL     A_BlueSpark();
void C_DECL     A_BoltSpark();
void C_DECL     A_BossDeath();
void C_DECL     A_Chase();
void C_DECL     A_CheckBurnGone();
void C_DECL     A_CheckSkullDone();
void C_DECL     A_CheckSkullFloor();
void C_DECL     A_ChicAttack();
void C_DECL     A_ChicChase();
void C_DECL     A_ChicLook();
void C_DECL     A_ChicPain();
void C_DECL     A_ClinkAttack();
void C_DECL     A_ContMobjSound();
void C_DECL     A_DeathBallImpact();
void C_DECL     A_DripBlood();
void C_DECL     A_ESound();
void C_DECL     A_Explode();
void C_DECL     A_FaceTarget();
void C_DECL     A_Feathers();
void C_DECL     A_FireBlasterPL1();
void C_DECL     A_FireBlasterPL2();
void C_DECL     A_FireCrossbowPL1();
void C_DECL     A_FireCrossbowPL2();
void C_DECL     A_FireGoldWandPL1();
void C_DECL     A_FireGoldWandPL2();
void C_DECL     A_FireMacePL1();
void C_DECL     A_FireMacePL2();
void C_DECL     A_FirePhoenixPL1();
void C_DECL     A_FirePhoenixPL2();
void C_DECL     A_FireSkullRodPL1();
void C_DECL     A_FireSkullRodPL2();
void C_DECL     A_FlameEnd();
void C_DECL     A_FlameSnd();
void C_DECL     A_FloatPuff();
void C_DECL     A_FreeTargMobj();
void C_DECL     A_GauntletAttack();
void C_DECL     A_GenWizard();
void C_DECL     A_GhostOff();
void C_DECL     A_HeadAttack();
void C_DECL     A_HeadFireGrow();
void C_DECL     A_HeadIceImpact();
void C_DECL     A_HideInCeiling();
void C_DECL     A_HideThing();
void C_DECL     A_ImpDeath();
void C_DECL     A_ImpExplode();
void C_DECL     A_ImpMeAttack();
void C_DECL     A_ImpMsAttack();
void C_DECL     A_ImpMsAttack2();
void C_DECL     A_ImpXDeath1();
void C_DECL     A_ImpXDeath2();
void C_DECL     A_InitKeyGizmo();
void C_DECL     A_InitPhoenixPL2();
void C_DECL     A_KnightAttack();
void C_DECL     A_Light0();
void C_DECL     A_Look();
void C_DECL     A_Lower();
void C_DECL     A_MaceBallImpact();
void C_DECL     A_MaceBallImpact2();
void C_DECL     A_MacePL1Check();
void C_DECL     A_MakePod();
void C_DECL     A_MinotaurAtk1();
void C_DECL     A_MinotaurAtk2();
void C_DECL     A_MinotaurAtk3();
void C_DECL     A_MinotaurCharge();
void C_DECL     A_MinotaurDecide();
void C_DECL     A_MntrFloorFire();
void C_DECL     A_MummyAttack();
void C_DECL     A_MummyAttack2();
void C_DECL     A_MummyFX1Seek();
void C_DECL     A_MummySoul();
void C_DECL     A_NoBlocking();
void C_DECL     A_Pain();
void C_DECL     A_PhoenixPuff();
void C_DECL     A_PodPain();
void C_DECL     A_RainImpact();
void C_DECL     A_Raise();
void C_DECL     A_ReFire();
void C_DECL     A_RemovePod();
void C_DECL     A_RestoreArtifact();

/**
 * Make a special 'thing' visible again.
 */
void C_DECL     A_RestoreSpecialThing1();

void C_DECL     A_RestoreSpecialThing2();
void C_DECL     A_Scream();
void C_DECL     A_ShutdownPhoenixPL2();
void C_DECL     A_SkullPop();
void C_DECL     A_SkullRodPL2Seek();
void C_DECL     A_SkullRodStorm();
void C_DECL     A_SnakeAttack();
void C_DECL     A_SnakeAttack2();
void C_DECL     A_Sor1Chase();
void C_DECL     A_Sor1Pain();
void C_DECL     A_Sor2DthInit();
void C_DECL     A_Sor2DthLoop();
void C_DECL     A_SorcererRise();
void C_DECL     A_SorDBon();
void C_DECL     A_SorDExp();
void C_DECL     A_SorDSph();
void C_DECL     A_SorRise();
void C_DECL     A_SorSightSnd();
void C_DECL     A_SorZap();
void C_DECL     A_SpawnRippers();
void C_DECL     A_SpawnTeleGlitter();
void C_DECL     A_SpawnTeleGlitter2();
void C_DECL     A_Srcr1Attack();
void C_DECL     A_Srcr2Attack();
void C_DECL     A_Srcr2Decide();
void C_DECL     A_StaffAttackPL1();
void C_DECL     A_StaffAttackPL2();
void C_DECL     A_UnHideThing();
void C_DECL     A_VolcanoBlast();
void C_DECL     A_VolcanoSet();
void C_DECL     A_VolcBallImpact();
void C_DECL     A_WeaponReady();
void C_DECL     A_WhirlwindSeek();
void C_DECL     A_WizAtk1();
void C_DECL     A_WizAtk2();
void C_DECL     A_WizAtk3();

// Inventory:
void C_DECL     A_FireBomb();
void C_DECL     A_TombOfPower();
void C_DECL     A_Egg();
void C_DECL     A_Wings();
void C_DECL     A_Teleport();
void C_DECL     A_Torch();
void C_DECL     A_Health();
void C_DECL     A_SuperHealth();
void C_DECL     A_Invisibility();
void C_DECL     A_Invulnerability();

#endif
