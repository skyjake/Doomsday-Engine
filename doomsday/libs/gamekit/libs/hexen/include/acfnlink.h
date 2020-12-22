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

#ifndef __ACTION_LINK_H__
#define __ACTION_LINK_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "dd_share.h"

DE_EXTERN_C actionlink_t actionlinks[];

void C_DECL     A_AddPlayerCorpse();
void C_DECL     A_BatMove();
void C_DECL     A_BatSpawn();
void C_DECL     A_BatSpawnInit();
void C_DECL     A_BellReset1();
void C_DECL     A_BellReset2();
void C_DECL     A_BishopAttack();
void C_DECL     A_BishopAttack2();
void C_DECL     A_BishopChase();
void C_DECL     A_BishopDecide();
void C_DECL     A_BishopDoBlur();
void C_DECL     A_BishopMissileSeek();
void C_DECL     A_BishopMissileWeave();
void C_DECL     A_BishopPainBlur();
void C_DECL     A_BishopPuff();
void C_DECL     A_BishopSpawnBlur();
void C_DECL     A_BounceCheck();
void C_DECL     A_BridgeInit();
void C_DECL     A_BridgeOrbit();
void C_DECL     A_CentaurAttack();
void C_DECL     A_CentaurAttack2();
void C_DECL     A_CentaurDefend();
void C_DECL     A_CentaurDropStuff();
void C_DECL     A_CFlameAttack();
void C_DECL     A_CFlameMissile();
void C_DECL     A_CFlamePuff();
void C_DECL     A_CFlameRotate();
void C_DECL     A_Chase();
void C_DECL     A_CheckBurnGone();
void C_DECL     A_CheckFloor();
void C_DECL     A_CheckSkullDone();
void C_DECL     A_CheckSkullFloor();
void C_DECL     A_CheckTeleRing();
void C_DECL     A_CheckThrowBomb();
void C_DECL     A_CHolyAttack();
void C_DECL     A_CHolyAttack2();
void C_DECL     A_CHolyCheckScream();
void C_DECL     A_CHolyPalette();
void C_DECL     A_CHolySeek();
void C_DECL     A_CHolySpawnPuff();
void C_DECL     A_CHolyTail();
void C_DECL     A_ClassBossHealth();
void C_DECL     A_ClericAttack();
void C_DECL     A_CMaceAttack();
void C_DECL     A_ContMobjSound();
void C_DECL     A_CorpseBloodDrip();
void C_DECL     A_CorpseExplode();
void C_DECL     A_CStaffAttack();
void C_DECL     A_CStaffCheck();
void C_DECL     A_CStaffCheckBlink();
void C_DECL     A_CStaffInitBlink();
void C_DECL     A_CStaffMissileSlither();
void C_DECL     A_DelayGib();
void C_DECL     A_Demon2Death();
void C_DECL     A_DemonAttack1();
void C_DECL     A_DemonAttack2();
void C_DECL     A_DemonDeath();
void C_DECL     A_DragonAttack();
void C_DECL     A_DragonCheckCrash();
void C_DECL     A_DragonFlap();
void C_DECL     A_DragonFlight();
void C_DECL     A_DragonFX2();
void C_DECL     A_DragonInitFlight();
void C_DECL     A_DragonPain();
void C_DECL     A_DropMace();
void C_DECL     A_ESound();
void C_DECL     A_EttinAttack();
void C_DECL     A_Explode();
void C_DECL     A_FaceTarget();
void C_DECL     A_FastChase();
void C_DECL     A_FAxeAttack();
void C_DECL     A_FHammerAttack();
void C_DECL     A_FHammerThrow();
void C_DECL     A_FighterAttack();
void C_DECL     A_FireConePL1();
void C_DECL     A_FiredAttack();
void C_DECL     A_FiredChase();
void C_DECL     A_FiredRocks();
void C_DECL     A_FiredSplotch();
void C_DECL     A_FlameCheck();
void C_DECL     A_FloatGib();
void C_DECL     A_FogMove();
void C_DECL     A_FogSpawn();
void C_DECL     A_FPunchAttack();
void C_DECL     A_FreeTargMobj();
void C_DECL     A_FreezeDeath();
void C_DECL     A_FreezeDeathChunks();
void C_DECL     A_FSwordAttack();
void C_DECL     A_FSwordFlames();
void C_DECL     A_HideThing();
void C_DECL     A_IceCheckHeadDone();
void C_DECL     A_IceGuyAttack();
void C_DECL     A_IceGuyChase();
void C_DECL     A_IceGuyDie();
void C_DECL     A_IceGuyLook();
void C_DECL     A_IceGuyMissileExplode();
void C_DECL     A_IceGuyMissilePuff();
void C_DECL     A_IceSetTics();
void C_DECL     A_KBolt();
void C_DECL     A_KBoltRaise();
void C_DECL     A_KoraxBonePop();
void C_DECL     A_KoraxChase();
void C_DECL     A_KoraxCommand();
void C_DECL     A_KoraxDecide();
void C_DECL     A_KoraxMissile();
void C_DECL     A_KoraxStep();
void C_DECL     A_KoraxStep2();
void C_DECL     A_KSpiritRoam();
void C_DECL     A_LastZap();
void C_DECL     A_LeafCheck();
void C_DECL     A_LeafSpawn();
void C_DECL     A_LeafThrust();
void C_DECL     A_Light0();
void C_DECL     A_LightningClip();
void C_DECL     A_LightningReady();
void C_DECL     A_LightningRemove();
void C_DECL     A_LightningZap();
void C_DECL     A_Look();
void C_DECL     A_Lower();
void C_DECL     A_MageAttack();
void C_DECL     A_MinotaurAtk1();
void C_DECL     A_MinotaurAtk2();
void C_DECL     A_MinotaurAtk3();
void C_DECL     A_MinotaurCharge();
void C_DECL     A_MinotaurChase();
void C_DECL     A_MinotaurDecide();
void C_DECL     A_MinotaurFade0();
void C_DECL     A_MinotaurFade1();
void C_DECL     A_MinotaurFade2();
void C_DECL     A_MinotaurLook();
void C_DECL     A_MinotaurRoam();
void C_DECL     A_MLightningAttack();
void C_DECL     A_MntrFloorFire();
void C_DECL     A_MStaffAttack();
void C_DECL     A_MStaffPalette();
void C_DECL     A_MStaffTrack();
void C_DECL     A_MStaffWeave();
void C_DECL     A_MWandAttack();
void C_DECL     A_NoBlocking();
void C_DECL     A_NoGravity();
void C_DECL     A_Pain();
void C_DECL     A_PigAttack();
void C_DECL     A_PigChase();
void C_DECL     A_PigLook();
void C_DECL     A_PigPain();
void C_DECL     A_PoisonBagCheck();
void C_DECL     A_PoisonBagDamage();
void C_DECL     A_PoisonBagInit();
void C_DECL     A_PoisonShroom();
void C_DECL     A_PotteryCheck();
void C_DECL     A_PotteryChooseBit();
void C_DECL     A_PotteryExplode();
void C_DECL     A_Quake();
void C_DECL     A_QueueCorpse();
void C_DECL     A_Raise();
void C_DECL     A_ReFire();
void C_DECL     A_RestoreArtifact();
void C_DECL     A_RestoreSpecialThing1();
void C_DECL     A_RestoreSpecialThing2();
void C_DECL     A_Scream();
void C_DECL     A_SerpentBirthScream();
void C_DECL     A_SerpentChase();
void C_DECL     A_SerpentCheckForAttack();
void C_DECL     A_SerpentChooseAttack();
void C_DECL     A_SerpentDiveSound();
void C_DECL     A_SerpentHeadCheck();
void C_DECL     A_SerpentHeadPop();
void C_DECL     A_SerpentHide();
void C_DECL     A_SerpentHumpDecide();
void C_DECL     A_SerpentLowerHump();
void C_DECL     A_SerpentMeleeAttack();
void C_DECL     A_SerpentMissileAttack();
void C_DECL     A_SerpentRaiseHump();
void C_DECL     A_SerpentSpawnGibs();
void C_DECL     A_SerpentUnHide();
void C_DECL     A_SerpentWalk();
void C_DECL     A_SetAltShadow();
void C_DECL     A_SetReflective();
void C_DECL     A_SetShootable();
void C_DECL     A_ShedShard();
void C_DECL     A_SinkGib();
void C_DECL     A_SkullPop();
void C_DECL     A_SmBounce();
void C_DECL     A_SmokePuffExit();
void C_DECL     A_SnoutAttack();
void C_DECL     A_SoAExplode();
void C_DECL     A_SorcBallOrbit();
void C_DECL     A_SorcBallPop();
void C_DECL     A_SorcBossAttack();
void C_DECL     A_SorcererBishopEntry();
void C_DECL     A_SorcFX1Seek();
void C_DECL     A_SorcFX2Orbit();
void C_DECL     A_SorcFX2Split();
void C_DECL     A_SorcFX4Check();
void C_DECL     A_SorcSpinBalls();
void C_DECL     A_SpawnBishop();
void C_DECL     A_SpawnFizzle();
void C_DECL     A_SpeedBalls();
void C_DECL     A_SpeedFade();
void C_DECL     A_Summon();
void C_DECL     A_TeloSpawnA();
void C_DECL     A_TeloSpawnB();
void C_DECL     A_TeloSpawnC();
void C_DECL     A_TeloSpawnD();
void C_DECL     A_ThrustBlock();
void C_DECL     A_ThrustImpale();
void C_DECL     A_ThrustInitDn();
void C_DECL     A_ThrustInitUp();
void C_DECL     A_ThrustLower();
void C_DECL     A_ThrustRaise();
void C_DECL     A_TreeDeath();
void C_DECL     A_UnHideThing();
void C_DECL     A_UnSetInvulnerable();
void C_DECL     A_UnSetReflective();
void C_DECL     A_UnSetShootable();
void C_DECL     A_WeaponReady();
void C_DECL     A_WraithChase();
void C_DECL     A_WraithFX2();
void C_DECL     A_WraithFX3();
void C_DECL     A_WraithInit();
void C_DECL     A_WraithLook();
void C_DECL     A_WraithMelee();
void C_DECL     A_WraithMissile();
void C_DECL     A_WraithRaise();
void C_DECL     A_WraithRaiseInit();
void C_DECL     A_ZapMimic();
void C_DECL     A_FSwordAttack2();
void C_DECL     A_CHolyAttack3();
void C_DECL     A_MStaffAttack2();
void C_DECL     A_SorcBallOrbit();
void C_DECL     A_SorcSpinBalls();
void C_DECL     A_SpeedBalls();
void C_DECL     A_SlowBalls();
void C_DECL     A_StopBalls();
void C_DECL     A_AccelBalls();
void C_DECL     A_DecelBalls();
void C_DECL     A_SorcBossAttack();
void C_DECL     A_SpawnFizzle();
void C_DECL     A_CastSorcererSpell();
void C_DECL     A_SorcUpdateBallAngle();
void C_DECL     A_BounceCheck();
void C_DECL     A_SorcFX1Seek();
void C_DECL     A_SorcOffense1();
void C_DECL     A_SorcOffense2();

// Inventory
void C_DECL     A_Invulnerability();
void C_DECL     A_Health();
void C_DECL     A_SuperHealth();
void C_DECL     A_HealRadius();
void C_DECL     A_SummonTarget();
void C_DECL     A_Torch();
void C_DECL     A_Egg();
void C_DECL     A_Wings();
void C_DECL     A_BlastRadius();
void C_DECL     A_PoisonBag();
void C_DECL     A_TeleportOther();
void C_DECL     A_Speed();
void C_DECL     A_BoostMana();
void C_DECL     A_BoostArmor();
void C_DECL     A_Teleport();
void C_DECL     A_PuzzSkull();
void C_DECL     A_PuzzGemBig();
void C_DECL     A_PuzzGemRed();
void C_DECL     A_PuzzGemGreen1();
void C_DECL     A_PuzzGemGreen2();
void C_DECL     A_PuzzGemBlue1();
void C_DECL     A_PuzzGemBlue2();
void C_DECL     A_PuzzBook1();
void C_DECL     A_PuzzBook2();
void C_DECL     A_PuzzSkull2();
void C_DECL     A_PuzzFWeapon();
void C_DECL     A_PuzzCWeapon();
void C_DECL     A_PuzzMWeapon();
void C_DECL     A_PuzzGear1();
void C_DECL     A_PuzzGear2();
void C_DECL     A_PuzzGear3();
void C_DECL     A_PuzzGear4();
#endif
