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

typedef struct mobj_s   mobj_t;
typedef struct player_s player_t;
typedef struct pspdef_s pspdef_t;

#define ACTION_MOBJ(name)   void C_DECL name(mobj_t *actor)
#define ACTION_WEAPON(name) void C_DECL name(player_t *player, pspdef_t *psp)

#ifdef __cplusplus
extern "C" {
#endif

DE_EXTERN_C actionlink_t actionlinks[];

ACTION_MOBJ(A_AccTeleGlitter);
ACTION_MOBJ(A_AddPlayerCorpse);
ACTION_MOBJ(A_AddPlayerRain);
ACTION_WEAPON(A_BeakAttackPL1);
ACTION_WEAPON(A_BeakAttackPL2);
ACTION_WEAPON(A_BeakRaise);
ACTION_WEAPON(A_BeakReady);
ACTION_MOBJ(A_BeastAttack);
ACTION_MOBJ(A_BeastPuff);
ACTION_MOBJ(A_BlueSpark);
ACTION_MOBJ(A_BoltSpark);
ACTION_MOBJ(A_BossDeath);
ACTION_MOBJ(A_Chase);
ACTION_MOBJ(A_CheckBurnGone);
ACTION_MOBJ(A_CheckSkullDone);
ACTION_MOBJ(A_CheckSkullFloor);
ACTION_MOBJ(A_ChicAttack);
ACTION_MOBJ(A_ChicChase);
ACTION_MOBJ(A_ChicLook);
ACTION_MOBJ(A_ChicPain);
ACTION_MOBJ(A_ClinkAttack);
ACTION_MOBJ(A_ContMobjSound);
ACTION_MOBJ(A_DeathBallImpact);
ACTION_MOBJ(A_DripBlood);
ACTION_MOBJ(A_ESound);
ACTION_MOBJ(A_Explode);
ACTION_MOBJ(A_FaceTarget);
ACTION_MOBJ(A_Feathers);
ACTION_WEAPON(A_FireBlasterPL1);
ACTION_WEAPON(A_FireBlasterPL2);
ACTION_WEAPON(A_FireCrossbowPL1);
ACTION_WEAPON(A_FireCrossbowPL2);
ACTION_WEAPON(A_FireGoldWandPL1);
ACTION_WEAPON(A_FireGoldWandPL2);
ACTION_WEAPON(A_FireMacePL1);
ACTION_WEAPON(A_FireMacePL2);
ACTION_WEAPON(A_FirePhoenixPL1);
ACTION_WEAPON(A_FirePhoenixPL2);
ACTION_WEAPON(A_FireSkullRodPL1);
ACTION_WEAPON(A_FireSkullRodPL2);
ACTION_MOBJ(A_FlameEnd);
ACTION_MOBJ(A_FlameSnd);
ACTION_MOBJ(A_FloatPuff);
ACTION_MOBJ(A_FreeTargMobj);
ACTION_WEAPON(A_GauntletAttack);
ACTION_MOBJ(A_GenWizard);
ACTION_MOBJ(A_GhostOff);
ACTION_MOBJ(A_HeadAttack);
ACTION_MOBJ(A_HeadFireGrow);
ACTION_MOBJ(A_HeadIceImpact);
ACTION_MOBJ(A_HideInCeiling);
ACTION_MOBJ(A_HideThing);
ACTION_MOBJ(A_ImpDeath);
ACTION_MOBJ(A_ImpExplode);
ACTION_MOBJ(A_ImpMeAttack);
ACTION_MOBJ(A_ImpMsAttack);
ACTION_MOBJ(A_ImpMsAttack2);
ACTION_MOBJ(A_ImpXDeath1);
ACTION_MOBJ(A_ImpXDeath2);
ACTION_MOBJ(A_InitKeyGizmo);
ACTION_WEAPON(A_InitPhoenixPL2);
ACTION_MOBJ(A_KnightAttack);
ACTION_WEAPON(A_Light0);
ACTION_MOBJ(A_Look);
ACTION_WEAPON(A_Lower);
ACTION_MOBJ(A_MaceBallImpact);
ACTION_MOBJ(A_MaceBallImpact2);
ACTION_MOBJ(A_MacePL1Check);
ACTION_MOBJ(A_MakePod);
ACTION_MOBJ(A_MinotaurAtk1);
ACTION_MOBJ(A_MinotaurAtk2);
ACTION_MOBJ(A_MinotaurAtk3);
ACTION_MOBJ(A_MinotaurCharge);
ACTION_MOBJ(A_MinotaurDecide);
ACTION_MOBJ(A_MntrFloorFire);
ACTION_MOBJ(A_MummyAttack);
ACTION_MOBJ(A_MummyAttack2);
ACTION_MOBJ(A_MummyFX1Seek);
ACTION_MOBJ(A_MummySoul);
ACTION_MOBJ(A_NoBlocking);
ACTION_MOBJ(A_Pain);
ACTION_MOBJ(A_PhoenixPuff);
ACTION_MOBJ(A_PodPain);
ACTION_MOBJ(A_RainImpact);
ACTION_WEAPON(A_Raise);
ACTION_WEAPON(A_ReFire);
ACTION_MOBJ(A_RemovePod);
ACTION_MOBJ(A_RestoreArtifact);

/**
 * Make a special 'thing' visible again.
 */
ACTION_MOBJ(A_RestoreSpecialThing1);

ACTION_MOBJ(A_RestoreSpecialThing2);
ACTION_MOBJ(A_Scream);
ACTION_WEAPON(A_ShutdownPhoenixPL2);
ACTION_MOBJ(A_SkullPop);
ACTION_MOBJ(A_SkullRodPL2Seek);
ACTION_MOBJ(A_SkullRodStorm);
ACTION_MOBJ(A_SnakeAttack);
ACTION_MOBJ(A_SnakeAttack2);
ACTION_MOBJ(A_Sor1Chase);
ACTION_MOBJ(A_Sor1Pain);
ACTION_MOBJ(A_Sor2DthInit);
ACTION_MOBJ(A_Sor2DthLoop);
ACTION_MOBJ(A_SorcererRise);
ACTION_MOBJ(A_SorDBon);
ACTION_MOBJ(A_SorDExp);
ACTION_MOBJ(A_SorDSph);
ACTION_MOBJ(A_SorRise);
ACTION_MOBJ(A_SorSightSnd);
ACTION_MOBJ(A_SorZap);
ACTION_MOBJ(A_SpawnRippers);
ACTION_MOBJ(A_SpawnTeleGlitter);
ACTION_MOBJ(A_SpawnTeleGlitter2);
ACTION_MOBJ(A_Srcr1Attack);
ACTION_MOBJ(A_Srcr2Attack);
ACTION_MOBJ(A_Srcr2Decide);
ACTION_WEAPON(A_StaffAttackPL1);
ACTION_WEAPON(A_StaffAttackPL2);
ACTION_MOBJ(A_UnHideThing);
ACTION_MOBJ(A_VolcanoBlast);
ACTION_MOBJ(A_VolcanoSet);
ACTION_MOBJ(A_VolcBallImpact);
ACTION_WEAPON(A_WeaponReady);
ACTION_MOBJ(A_WhirlwindSeek);
ACTION_MOBJ(A_WizAtk1);
ACTION_MOBJ(A_WizAtk2);
ACTION_MOBJ(A_WizAtk3);

// Inventory:
ACTION_MOBJ(A_FireBomb);
ACTION_MOBJ(A_TombOfPower);
ACTION_MOBJ(A_Egg);
ACTION_MOBJ(A_Wings);
ACTION_MOBJ(A_Teleport);
ACTION_MOBJ(A_Torch);
ACTION_MOBJ(A_Health);
ACTION_MOBJ(A_SuperHealth);
ACTION_MOBJ(A_Invisibility);
ACTION_MOBJ(A_Invulnerability);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
