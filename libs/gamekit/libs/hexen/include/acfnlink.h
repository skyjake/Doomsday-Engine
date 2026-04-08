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
 * acfnlink.h:
 */

#pragma once

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
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

ACTION_MOBJ(A_AddPlayerCorpse);
ACTION_MOBJ(A_BatMove);
ACTION_MOBJ(A_BatSpawn);
ACTION_MOBJ(A_BatSpawnInit);
ACTION_MOBJ(A_BellReset1);
ACTION_MOBJ(A_BellReset2);
ACTION_MOBJ(A_BishopAttack);
ACTION_MOBJ(A_BishopAttack2);
ACTION_MOBJ(A_BishopChase);
ACTION_MOBJ(A_BishopDecide);
ACTION_MOBJ(A_BishopDoBlur);
ACTION_MOBJ(A_BishopMissileSeek);
ACTION_MOBJ(A_BishopMissileWeave);
ACTION_MOBJ(A_BishopPainBlur);
ACTION_MOBJ(A_BishopPuff);
ACTION_MOBJ(A_BishopSpawnBlur);
ACTION_MOBJ(A_BounceCheck);
ACTION_MOBJ(A_BridgeInit);
ACTION_MOBJ(A_BridgeOrbit);
ACTION_MOBJ(A_CentaurAttack);
ACTION_MOBJ(A_CentaurAttack2);
ACTION_MOBJ(A_CentaurDefend);
ACTION_MOBJ(A_CentaurDropStuff);
ACTION_WEAPON(A_CFlameAttack);
ACTION_MOBJ(A_CFlameMissile);
ACTION_MOBJ(A_CFlamePuff);
ACTION_MOBJ(A_CFlameRotate);
ACTION_MOBJ(A_Chase);
ACTION_MOBJ(A_CheckBurnGone);
ACTION_MOBJ(A_CheckFloor);
ACTION_MOBJ(A_CheckSkullDone);
ACTION_MOBJ(A_CheckSkullFloor);
ACTION_MOBJ(A_CheckTeleRing);
ACTION_MOBJ(A_CheckThrowBomb);
ACTION_WEAPON(A_CHolyAttack);
ACTION_MOBJ(A_CHolyAttack2);
ACTION_MOBJ(A_CHolyAttack3);
ACTION_MOBJ(A_CHolyCheckScream);
ACTION_WEAPON(A_CHolyPalette);
ACTION_MOBJ(A_CHolySeek);
ACTION_MOBJ(A_CHolySpawnPuff);
ACTION_MOBJ(A_CHolyTail);
ACTION_MOBJ(A_ClassBossHealth);
ACTION_MOBJ(A_ClericAttack);
ACTION_WEAPON(A_CMaceAttack);
ACTION_MOBJ(A_ContMobjSound);
ACTION_MOBJ(A_CorpseBloodDrip);
ACTION_MOBJ(A_CorpseExplode);
ACTION_WEAPON(A_CStaffAttack);
ACTION_WEAPON(A_CStaffCheck);
ACTION_WEAPON(A_CStaffCheckBlink);
ACTION_WEAPON(A_CStaffInitBlink);
ACTION_MOBJ(A_CStaffMissileSlither);
ACTION_MOBJ(A_DecelBalls);
ACTION_MOBJ(A_DelayGib);
ACTION_MOBJ(A_Demon2Death);
ACTION_MOBJ(A_DemonAttack1);
ACTION_MOBJ(A_DemonAttack2);
ACTION_MOBJ(A_DemonDeath);
ACTION_MOBJ(A_DragonAttack);
ACTION_MOBJ(A_DragonCheckCrash);
ACTION_MOBJ(A_DragonFlap);
ACTION_MOBJ(A_DragonFlight);
ACTION_MOBJ(A_DragonFX2);
ACTION_MOBJ(A_DragonInitFlight);
ACTION_MOBJ(A_DragonPain);
ACTION_MOBJ(A_DropMace);
ACTION_MOBJ(A_ESound);
ACTION_MOBJ(A_EttinAttack);
ACTION_MOBJ(A_Explode);
ACTION_MOBJ(A_FaceTarget);
ACTION_MOBJ(A_FastChase);
ACTION_WEAPON(A_FAxeAttack);
ACTION_WEAPON(A_FHammerAttack);
ACTION_WEAPON(A_FHammerThrow);
ACTION_MOBJ(A_FighterAttack);
ACTION_WEAPON(A_FireConePL1);
ACTION_MOBJ(A_FiredAttack);
ACTION_MOBJ(A_FiredChase);
ACTION_MOBJ(A_FiredRocks);
ACTION_MOBJ(A_FiredSplotch);
ACTION_MOBJ(A_FlameCheck);
ACTION_MOBJ(A_FloatGib);
ACTION_MOBJ(A_FogMove);
ACTION_MOBJ(A_FogSpawn);
ACTION_WEAPON(A_FPunchAttack);
ACTION_MOBJ(A_FreeTargMobj);
ACTION_MOBJ(A_FreezeDeath);
ACTION_MOBJ(A_FreezeDeathChunks);
ACTION_WEAPON(A_FSwordAttack);
ACTION_MOBJ(A_FSwordAttack2);
ACTION_MOBJ(A_FSwordFlames);
ACTION_MOBJ(A_HideThing);
ACTION_MOBJ(A_IceCheckHeadDone);
ACTION_MOBJ(A_IceGuyAttack);
ACTION_MOBJ(A_IceGuyChase);
ACTION_MOBJ(A_IceGuyDie);
ACTION_MOBJ(A_IceGuyLook);
ACTION_MOBJ(A_IceGuyMissileExplode);
ACTION_MOBJ(A_IceGuyMissilePuff);
ACTION_MOBJ(A_IceSetTics);
ACTION_MOBJ(A_KBolt);
ACTION_MOBJ(A_KBoltRaise);
ACTION_MOBJ(A_KoraxBonePop);
ACTION_MOBJ(A_KoraxChase);
ACTION_MOBJ(A_KoraxCommand);
ACTION_MOBJ(A_KoraxDecide);
ACTION_MOBJ(A_KoraxMissile);
ACTION_MOBJ(A_KoraxStep);
ACTION_MOBJ(A_KoraxStep2);
ACTION_MOBJ(A_KSpiritRoam);
ACTION_MOBJ(A_LastZap);
ACTION_MOBJ(A_LeafCheck);
ACTION_MOBJ(A_LeafSpawn);
ACTION_MOBJ(A_LeafThrust);
ACTION_WEAPON(A_Light0);
ACTION_MOBJ(A_LightningClip);
ACTION_WEAPON(A_LightningReady);
ACTION_MOBJ(A_LightningRemove);
ACTION_MOBJ(A_LightningZap);
ACTION_MOBJ(A_Look);
ACTION_WEAPON(A_Lower);
ACTION_MOBJ(A_MageAttack);
ACTION_MOBJ(A_MinotaurAtk1);
ACTION_MOBJ(A_MinotaurAtk2);
ACTION_MOBJ(A_MinotaurAtk3);
ACTION_MOBJ(A_MinotaurCharge);
ACTION_MOBJ(A_MinotaurChase);
ACTION_MOBJ(A_MinotaurDecide);
ACTION_MOBJ(A_MinotaurFade0);
ACTION_MOBJ(A_MinotaurFade1);
ACTION_MOBJ(A_MinotaurFade2);
ACTION_MOBJ(A_MinotaurLook);
ACTION_MOBJ(A_MinotaurRoam);
ACTION_WEAPON(A_MLightningAttack);
ACTION_MOBJ(A_MntrFloorFire);
ACTION_WEAPON(A_MStaffAttack);
ACTION_MOBJ(A_MStaffAttack2);
ACTION_WEAPON(A_MStaffPalette);
ACTION_MOBJ(A_MStaffTrack);
ACTION_MOBJ(A_MStaffWeave);
ACTION_WEAPON(A_MWandAttack);
ACTION_MOBJ(A_NoBlocking);
ACTION_MOBJ(A_NoGravity);
ACTION_MOBJ(A_Pain);
ACTION_MOBJ(A_PigAttack);
ACTION_MOBJ(A_PigChase);
ACTION_MOBJ(A_PigLook);
ACTION_MOBJ(A_PigPain);
ACTION_MOBJ(A_PoisonBagCheck);
ACTION_MOBJ(A_PoisonBagDamage);
ACTION_MOBJ(A_PoisonBagInit);
ACTION_MOBJ(A_PoisonShroom);
ACTION_MOBJ(A_PotteryCheck);
ACTION_MOBJ(A_PotteryChooseBit);
ACTION_MOBJ(A_PotteryExplode);
ACTION_MOBJ(A_Quake);
ACTION_MOBJ(A_QueueCorpse);
ACTION_WEAPON(A_Raise);
ACTION_WEAPON(A_ReFire);
ACTION_MOBJ(A_RestoreArtifact);
ACTION_MOBJ(A_RestoreSpecialThing1);
ACTION_MOBJ(A_RestoreSpecialThing2);
ACTION_MOBJ(A_Scream);
ACTION_MOBJ(A_SerpentBirthScream);
ACTION_MOBJ(A_SerpentChase);
ACTION_MOBJ(A_SerpentCheckForAttack);
ACTION_MOBJ(A_SerpentChooseAttack);
ACTION_MOBJ(A_SerpentDiveSound);
ACTION_MOBJ(A_SerpentHeadCheck);
ACTION_MOBJ(A_SerpentHeadPop);
ACTION_MOBJ(A_SerpentHide);
ACTION_MOBJ(A_SerpentHumpDecide);
ACTION_MOBJ(A_SerpentLowerHump);
ACTION_MOBJ(A_SerpentMeleeAttack);
ACTION_MOBJ(A_SerpentMissileAttack);
ACTION_MOBJ(A_SerpentRaiseHump);
ACTION_MOBJ(A_SerpentSpawnGibs);
ACTION_MOBJ(A_SerpentUnHide);
ACTION_MOBJ(A_SerpentWalk);
ACTION_MOBJ(A_SetAltShadow);
ACTION_MOBJ(A_SetReflective);
ACTION_MOBJ(A_SetShootable);
ACTION_MOBJ(A_ShedShard);
ACTION_MOBJ(A_SinkGib);
ACTION_MOBJ(A_SkullPop);
ACTION_MOBJ(A_SlowBalls);
ACTION_MOBJ(A_SmBounce);
ACTION_MOBJ(A_SmokePuffExit);
ACTION_WEAPON(A_SnoutAttack);
ACTION_MOBJ(A_SoAExplode);
ACTION_MOBJ(A_SorcBallOrbit);
ACTION_MOBJ(A_SorcBallPop);
ACTION_MOBJ(A_SorcBossAttack);
ACTION_MOBJ(A_SorcererBishopEntry);
ACTION_MOBJ(A_SorcFX1Seek);
ACTION_MOBJ(A_SorcFX2Orbit);
ACTION_MOBJ(A_SorcFX2Split);
ACTION_MOBJ(A_SorcFX4Check);
ACTION_MOBJ(A_SorcOffense1);
ACTION_MOBJ(A_SorcOffense2);
ACTION_MOBJ(A_SorcSpinBalls);
ACTION_MOBJ(A_SorcUpdateBallAngle);
ACTION_MOBJ(A_SpawnBishop);
ACTION_MOBJ(A_SpawnFizzle);
ACTION_MOBJ(A_SpeedBalls);
ACTION_MOBJ(A_SpeedFade);
ACTION_MOBJ(A_StopBalls);
ACTION_MOBJ(A_AccelBalls);
ACTION_MOBJ(A_CastSorcererSpell);
ACTION_MOBJ(A_Summon);
ACTION_MOBJ(A_TeloSpawnA);
ACTION_MOBJ(A_TeloSpawnB);
ACTION_MOBJ(A_TeloSpawnC);
ACTION_MOBJ(A_TeloSpawnD);
ACTION_MOBJ(A_ThrustBlock);
ACTION_MOBJ(A_ThrustImpale);
ACTION_MOBJ(A_ThrustInitDn);
ACTION_MOBJ(A_ThrustInitUp);
ACTION_MOBJ(A_ThrustLower);
ACTION_MOBJ(A_ThrustRaise);
ACTION_MOBJ(A_TreeDeath);
ACTION_MOBJ(A_UnHideThing);
ACTION_MOBJ(A_UnSetInvulnerable);
ACTION_MOBJ(A_UnSetReflective);
ACTION_MOBJ(A_UnSetShootable);
ACTION_WEAPON(A_WeaponReady);
ACTION_MOBJ(A_WraithChase);
ACTION_MOBJ(A_WraithFX2);
ACTION_MOBJ(A_WraithFX3);
ACTION_MOBJ(A_WraithInit);
ACTION_MOBJ(A_WraithLook);
ACTION_MOBJ(A_WraithMelee);
ACTION_MOBJ(A_WraithMissile);
ACTION_MOBJ(A_WraithRaise);
ACTION_MOBJ(A_WraithRaiseInit);
ACTION_MOBJ(A_ZapMimic);

// Inventory
ACTION_MOBJ(A_Invulnerability);
ACTION_MOBJ(A_Health);
ACTION_MOBJ(A_SuperHealth);
ACTION_MOBJ(A_HealRadius);
ACTION_MOBJ(A_SummonTarget);
ACTION_MOBJ(A_Torch);
ACTION_MOBJ(A_Egg);
ACTION_MOBJ(A_Wings);
ACTION_MOBJ(A_BlastRadius);
ACTION_MOBJ(A_PoisonBag);
ACTION_MOBJ(A_TeleportOther);
ACTION_MOBJ(A_Speed);
ACTION_MOBJ(A_BoostMana);
ACTION_MOBJ(A_BoostArmor);
ACTION_MOBJ(A_Teleport);
ACTION_MOBJ(A_PuzzSkull);
ACTION_MOBJ(A_PuzzGemBig);
ACTION_MOBJ(A_PuzzGemRed);
ACTION_MOBJ(A_PuzzGemGreen1);
ACTION_MOBJ(A_PuzzGemGreen2);
ACTION_MOBJ(A_PuzzGemBlue1);
ACTION_MOBJ(A_PuzzGemBlue2);
ACTION_MOBJ(A_PuzzBook1);
ACTION_MOBJ(A_PuzzBook2);
ACTION_MOBJ(A_PuzzSkull2);
ACTION_MOBJ(A_PuzzFWeapon);
ACTION_MOBJ(A_PuzzCWeapon);
ACTION_MOBJ(A_PuzzMWeapon);
ACTION_MOBJ(A_PuzzGear1);
ACTION_MOBJ(A_PuzzGear2);
ACTION_MOBJ(A_PuzzGear3);
ACTION_MOBJ(A_PuzzGear4);

#ifdef __cplusplus
} // extern "C"
#endif

