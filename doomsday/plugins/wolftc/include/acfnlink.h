/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
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

#ifndef __ACTION_LINK_H__
#define __ACTION_LINK_H__

#include "dd_share.h"

typedef struct
{
    char *name; // Name of the routine.
    void (C_DECL *func)();  // Pointer to the function.
} actionlink_t;

extern actionlink_t actionlinks[];

void C_DECL A_BabyMetal();
void C_DECL A_BFGsound();
void C_DECL A_BFGSpray();
void C_DECL A_BossDeath();
void C_DECL A_BrainAwake();
void C_DECL A_BrainDie();
void C_DECL A_BrainExplode();
void C_DECL A_BrainPain();
void C_DECL A_BrainScream();
void C_DECL A_BrainSpit();
void C_DECL A_BruisAttack();
void C_DECL A_BspiAttack();
void C_DECL A_Chase();
void C_DECL A_CheckReload();
void C_DECL A_CloseShotgun2();
void C_DECL A_CPosAttack();
void C_DECL A_CPosRefire();
void C_DECL A_CyberAttack();
void C_DECL A_Explode();
void C_DECL A_FaceTarget();
void C_DECL A_Fall();
void C_DECL A_FatAttack1();
void C_DECL A_FatAttack2();
void C_DECL A_FatAttack3();
void C_DECL A_FatRaise();
void C_DECL A_Fire();
void C_DECL A_FireBFG();
void C_DECL A_FireCGun();
void C_DECL A_FireCrackle();
void C_DECL A_FireMissile();
void C_DECL A_FirePistol();
void C_DECL A_FirePlasma();
void C_DECL A_FireShotgun();
void C_DECL A_FireShotgun2();
void C_DECL A_GunFlash();
void C_DECL A_HeadAttack();
void C_DECL A_Hoof();
void C_DECL A_KeenDie();
void C_DECL A_Light0();
void C_DECL A_Light1();
void C_DECL A_Light2();
void C_DECL A_LoadShotgun2();
void C_DECL A_Look();
void C_DECL A_Lower();
void C_DECL A_Metal();
void C_DECL A_OpenShotgun2();
void C_DECL A_Pain();
void C_DECL A_PainAttack();
void C_DECL A_PainDie();
void C_DECL A_PlayerScream();
void C_DECL A_PosAttack();
void C_DECL A_Punch();
void C_DECL A_Raise();
void C_DECL A_ReFire();
void C_DECL A_SargAttack();
void C_DECL A_Saw();
void C_DECL A_Scream();
void C_DECL A_SkelFist();
void C_DECL A_SkelMissile();
void C_DECL A_SkelWhoosh();
void C_DECL A_SkullAttack();
void C_DECL A_SpawnFly();
void C_DECL A_SpawnSound();
void C_DECL A_SpidRefire();
void C_DECL A_SPosAttack();
void C_DECL A_StartFire();
void C_DECL A_Tracer();
void C_DECL A_TroopAttack();
void C_DECL A_VileAttack();
void C_DECL A_VileChase();
void C_DECL A_VileStart();
void C_DECL A_VileTarget();
void C_DECL A_WeaponReady();
void C_DECL A_XScream();

void C_DECL A_Knife();
void C_DECL A_FireWPistol();
void C_DECL A_FireSPistol();
void C_DECL A_FireCPistol();
void C_DECL A_Fire3Pistol();
void C_DECL A_FireUPistol();
void C_DECL A_UranusPlayerPistolBlur();
void C_DECL A_FireMPistol();
void C_DECL A_FireWMachineGun();
void C_DECL A_FireSMachineGun();
void C_DECL A_FireOMachineGun();
void C_DECL A_FireCMachineGun();
void C_DECL A_Fire3MachineGun();
void C_DECL A_FireUMachineGun();
void C_DECL A_UranusPlayerMachinegunBlur();
void C_DECL A_FireWGattlingGun();
void C_DECL A_FireSGattlingGun();
void C_DECL A_FireAGattlingGun();
void C_DECL A_FireOGattlingGun();
void C_DECL A_FireEGattlingGun();
void C_DECL A_FireCGattlingGun();
void C_DECL A_Fire3GattlingGun();
void C_DECL A_FireUGattlingGun();
void C_DECL A_UranusPlayerChaingunBlur();
void C_DECL A_FireMGattlingGun();
void C_DECL A_FireORifle();
void C_DECL A_FireMRifle();
void C_DECL A_FireORevolver();
void C_DECL A_FireRShotgun();
void C_DECL A_FireMSyringe();
void C_DECL A_FireWMissile();
void C_DECL A_FireLMissile();
void C_DECL A_FireCMissile();
void C_DECL A_FireFlame();
void C_DECL A_FireCFlame();
void C_DECL A_Fire3Flame();
void C_DECL A_FireCMissile1();
void C_DECL A_FireCMissile2();
void C_DECL A_FireCMissile2NA();
void C_DECL A_FireCMissile3();
void C_DECL A_KnifeThrust();
void C_DECL A_CKnifeThrust();
void C_DECL A_LoadMachineGun();
void C_DECL A_SpawnMachineGun();
void C_DECL A_SpawnFirstAidKit();
void C_DECL A_SpawnClip();
void C_DECL A_SpawnFlameTAmmo();
void C_DECL A_SpawnSilverKey();
void C_DECL A_SpawnGoldKey();
void C_DECL A_SpawnSClip();
void C_DECL A_Spawn3SClip();
void C_DECL A_SpawnMachineGunSClip();
void C_DECL A_SpawnSFlameTAmmo();
void C_DECL A_SpawnSSilverKey();
void C_DECL A_SpawnSGoldKey();
void C_DECL A_SpawnAClip();
void C_DECL A_SpawnCYellowKey();
void C_DECL A_SpawnOClip();
void C_DECL A_SpawnIClip();
void C_DECL A_SpawnUClip();
void C_DECL A_SpawnIFlameTAmmo();
void C_DECL A_WolfBullet();
void C_DECL A_BossBullet();
void C_DECL A_SSAttack();
void C_DECL A_OfficerAttack();
void C_DECL A_EliteGuardAttack();
void C_DECL A_SODMPSSAttack();
void C_DECL A_SODMPEliteGuardAttack();
void C_DECL A_OMS2SSAttack();
void C_DECL A_FoxAttack();
void C_DECL A_ChaingunZombieAttack();
void C_DECL A_WRocketAttack();
void C_DECL A_Explosion();
void C_DECL A_LRocketAttack();
void C_DECL A_Graze();
void C_DECL A_Shatter();
void C_DECL A_SchabbsAttack();
void C_DECL A_Fsplash();
void C_DECL A_FakeHitlerAttack();
void C_DECL A_BGFlameAttack();
void C_DECL A_DeathKnightAttack1();
void C_DECL A_DeathKnightAttack2();
void C_DECL A_AngelAttack1();
void C_DECL A_AngelAttack2();
void C_DECL A_TrackingAlways();
void C_DECL A_RobotAttack();
void C_DECL A_DevilAttack1();
void C_DECL A_DevilAttack2();
void C_DECL A_DevilAttack3();
void C_DECL A_MBatAttack();
void C_DECL A_CatMissle1();
void C_DECL A_CatMissle2();
void C_DECL A_NemesisAttack();
void C_DECL A_Tracking();
void C_DECL A_MadDocAttack();
void C_DECL A_BioBlasterAttack();
void C_DECL A_ChimeraAttack1();
void C_DECL A_BalrogAttack();
void C_DECL A_OmegaAttack1();
void C_DECL A_OmegaAttack2();
void C_DECL A_OmegaAttack3();
void C_DECL A_DrakeAttack1();
void C_DECL A_DrakeAttack2();
void C_DECL A_DrakeAttack3();
void C_DECL A_StalkerAttack();
void C_DECL A_HellGuardAttack();
void C_DECL A_SchabbsDemonAttack();
void C_DECL A_NormalMelee();
void C_DECL A_LightMelee();
void C_DECL A_DrainAttack();
void C_DECL A_GhostDrainAttack();
void C_DECL A_RGhostDrainAttack();
void C_DECL A_RLGhostDrainAttack();
void C_DECL A_TrollMelee();
void C_DECL A_HeavyMelee();
void C_DECL A_MetalWalk();
void C_DECL A_TreadMoveN();
void C_DECL A_TreadMoveS();
void C_DECL A_WaterTrollSwim();
void C_DECL A_WaterTrollChase();
void C_DECL A_ChaseNA();
void C_DECL A_Hitler();
void C_DECL A_SpectreRespawn();
void C_DECL A_LSpectreRespawn();
void C_DECL A_LGreenMistSplit();
void C_DECL A_NewSkeleton();
void C_DECL A_NewSkeleton2();
void C_DECL A_ShadowNemRespawn();
void C_DECL A_ChimeraStage2();
void C_DECL A_GeneralDecide();
void C_DECL A_WillDecide();
void C_DECL A_DeathKnightDecide();
void C_DECL A_AngelDecide();
void C_DECL A_AngelAttack1Continue1();
void C_DECL A_AngelAttack1Continue2();
void C_DECL A_QuarkDecide();
void C_DECL A_RobotDecide();
void C_DECL A_DevilDecide();
void C_DECL A_DevilAttack2Continue1();
void C_DECL A_DevilAttack2Continue2();
void C_DECL A_Devil2Decide();
void C_DECL A_Devil2Attack2Continue1();
void C_DECL A_Devil2Attack2Continue2();
void C_DECL A_BlackDemonDecide();
void C_DECL A_BlackDemonDecide2();
void C_DECL A_SkelDecideResurect();
void C_DECL A_SkelDecideResurectTime();
void C_DECL A_Skel2DecideResurect();
void C_DECL A_Skel2DecideResurectTime();
void C_DECL A_WaterTrollDecide();
void C_DECL A_NemesisDecide();
void C_DECL A_MullerDecide();
void C_DECL A_Angel2Decide();
void C_DECL A_Angel2Attack1Continue1();
void C_DECL A_Angel2Attack1Continue2();
void C_DECL A_PoopDecide();
void C_DECL A_SchabbsDDecide();
void C_DECL A_CandelabraDecide();
void C_DECL A_LightningDecide();
void C_DECL A_WaterDropDecide();
void C_DECL A_RocksDecide();
void C_DECL A_PlayerDead();
void C_DECL A_GScream();
void C_DECL A_BRefire();
void C_DECL A_HitlerSlop();
void C_DECL A_PacmanAttack();
void C_DECL A_PinkPacmanBlur1();
void C_DECL A_RedPacmanBlur1();
void C_DECL A_OrangePacmanBlur1();
void C_DECL A_BluePacmanBlur1();
void C_DECL A_PinkPacmanBlur2();
void C_DECL A_RedPacmanBlur2();
void C_DECL A_OrangePacmanBlur2();
void C_DECL A_BluePacmanBlur2();
void C_DECL A_PinkPacmanBlurA();
void C_DECL A_RedPacmanBlurA();
void C_DECL A_PinkPacmanBlurA2();
void C_DECL A_RedPacmanBlurA2();
void C_DECL A_PacmanSpawnerAwake();
void C_DECL A_PacmanSwastika();
void C_DECL A_PacmanBJHead();
void C_DECL A_AngMissileBlur1();
void C_DECL A_AngMissileBlur2();
void C_DECL A_AngMissileBlur3();
void C_DECL A_AngMissileBlur4();
void C_DECL A_AngelFireStart();
void C_DECL A_AngelFire();
void C_DECL A_AngelStartFire();
void C_DECL A_AngelFireCrackle();
void C_DECL A_AngelFireTarget();
void C_DECL A_AngelFireAttack();
void C_DECL A_DevilFireTarget();
void C_DECL A_SpawnRain();
void C_DECL A_ZombieFaceTarget();
void C_DECL A_EyeProjectile();
void C_DECL A_WaterTrollSplashR();
void C_DECL A_WaterTrollSplashL();
void C_DECL A_ChestOpenSmall();
void C_DECL A_ChestOpenMed();
void C_DECL A_ChestOpenLarge();
void C_DECL A_PlayingDeadActive();
void C_DECL A_RavenRefire();
void C_DECL A_DirtDevilChase();
void C_DECL A_DrakeFaceTarget();
void C_DECL A_DrakeHead();
void C_DECL A_StalkerFaceTarget();
void C_DECL A_StalkerProjectileSound();
void C_DECL A_FireGreenBlobMissile();
void C_DECL A_FireRedBlobMissile();
void C_DECL A_MotherBlobAttack();
void C_DECL A_SpawnGoldKeyCard();
void C_DECL A_ShadowNemesisResurectSound();
#endif
