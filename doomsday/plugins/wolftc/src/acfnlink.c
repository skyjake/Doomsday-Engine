/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include "wolftc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

actionlink_t actionlinks[] = {
    {"A_BabyMetal", A_BabyMetal}
    ,
    {"A_BFGsound", A_BFGsound}
    ,
    {"A_BFGSpray", A_BFGSpray}
    ,
    {"A_BossDeath", A_BossDeath}
    ,
    {"A_BrainAwake", A_BrainAwake}
    ,
    {"A_BrainDie", A_BrainDie}
    ,
    {"A_BrainExplode", A_BrainExplode}
    ,
    {"A_BrainPain", A_BrainPain}
    ,
    {"A_BrainScream", A_BrainScream}
    ,
    {"A_BrainSpit", A_BrainSpit}
    ,
    {"A_BruisAttack", A_BruisAttack}
    ,
    {"A_BspiAttack", A_BspiAttack}
    ,
    {"A_Chase", A_Chase}
    ,
    {"A_CheckReload", A_CheckReload}
    ,
    {"A_CloseShotgun2", A_CloseShotgun2}
    ,
    {"A_CPosAttack", A_CPosAttack}
    ,
    {"A_CPosRefire", A_CPosRefire}
    ,
    {"A_CyberAttack", A_CyberAttack}
    ,
    {"A_Explode", A_Explode}
    ,
    {"A_FaceTarget", A_FaceTarget}
    ,
    {"A_Fall", A_Fall}
    ,
    {"A_FatAttack1", A_FatAttack1}
    ,
    {"A_FatAttack2", A_FatAttack2}
    ,
    {"A_FatAttack3", A_FatAttack3}
    ,
    {"A_FatRaise", A_FatRaise}
    ,
    {"A_Fire", A_Fire}
    ,
    {"A_FireBFG", A_FireBFG}
    ,
    {"A_FireCGun", A_FireCGun}
    ,
    {"A_FireCrackle", A_FireCrackle}
    ,
    {"A_FireMissile", A_FireMissile}
    ,
    {"A_FirePistol", A_FirePistol}
    ,
    {"A_FirePlasma", A_FirePlasma}
    ,
    {"A_FireShotgun", A_FireShotgun}
    ,
    {"A_FireShotgun2", A_FireShotgun2}
    ,
    {"A_GunFlash", A_GunFlash}
    ,
    {"A_HeadAttack", A_HeadAttack}
    ,
    {"A_Hoof", A_Hoof}
    ,
    {"A_KeenDie", A_KeenDie}
    ,
    {"A_Light0", A_Light0}
    ,
    {"A_Light1", A_Light1}
    ,
    {"A_Light2", A_Light2}
    ,
    {"A_LoadShotgun2", A_LoadShotgun2}
    ,
    {"A_Look", A_Look}
    ,
    {"A_Lower", A_Lower}
    ,
    {"A_Metal", A_Metal}
    ,
    {"A_OpenShotgun2", A_OpenShotgun2}
    ,
    {"A_Pain", A_Pain}
    ,
    {"A_PainAttack", A_PainAttack}
    ,
    {"A_PainDie", A_PainDie}
    ,
    {"A_PlayerScream", A_PlayerScream}
    ,
    {"A_PosAttack", A_PosAttack}
    ,
    {"A_Punch", A_Punch}
    ,
    {"A_Raise", A_Raise}
    ,
    {"A_ReFire", A_ReFire}
    ,
    {"A_SargAttack", A_SargAttack}
    ,
    {"A_Saw", A_Saw}
    ,
    {"A_Scream", A_Scream}
    ,
    {"A_SkelFist", A_SkelFist}
    ,
    {"A_SkelMissile", A_SkelMissile}
    ,
    {"A_SkelWhoosh", A_SkelWhoosh}
    ,
    {"A_SkullAttack", A_SkullAttack}
    ,
    {"A_SpawnFly", A_SpawnFly}
    ,
    {"A_SpawnSound", A_SpawnSound}
    ,
    {"A_SpidRefire", A_SpidRefire}
    ,
    {"A_SPosAttack", A_SPosAttack}
    ,
    {"A_StartFire", A_StartFire}
    ,
    {"A_Tracer", A_Tracer}
    ,
    {"A_TroopAttack", A_TroopAttack}
    ,
    {"A_VileAttack", A_VileAttack}
    ,
    {"A_VileChase", A_VileChase}
    ,
    {"A_VileStart", A_VileStart}
    ,
    {"A_VileTarget", A_VileTarget}
    ,
    {"A_WeaponReady", A_WeaponReady}
    ,
    {"A_XScream", A_XScream}
    ,
    {"A_Knife", A_Knife}
    ,
    {"A_FireWPistol", A_FireWPistol}
    ,
    {"A_FireSPistol", A_FireSPistol}
    ,
    {"A_FireCPistol", A_FireCPistol}
    ,
    {"A_Fire3Pistol", A_Fire3Pistol}
    ,
    {"A_FireUPistol", A_FireUPistol}
    ,
    {"A_UranusPlayerPistolBlur", A_UranusPlayerPistolBlur}
    ,
    {"A_FireMPistol", A_FireMPistol}
    ,
    {"A_FireWMachineGun", A_FireWMachineGun}
    ,
    {"A_FireSMachineGun", A_FireSMachineGun}
    ,
    {"A_FireOMachineGun", A_FireOMachineGun}
    ,
    {"A_FireCMachineGun", A_FireCMachineGun}
    ,
    {"A_Fire3MachineGun", A_Fire3MachineGun}
    ,
    {"A_FireUMachineGun", A_FireUMachineGun}
    ,
    {"A_UranusPlayerMachinegunBlur", A_UranusPlayerMachinegunBlur}
    ,
    {"A_FireWGattlingGun", A_FireWGattlingGun}
    ,
    {"A_FireSGattlingGun", A_FireSGattlingGun}
    ,
    {"A_FireAGattlingGun", A_FireAGattlingGun}
    ,
    {"A_FireOGattlingGun", A_FireOGattlingGun}
    ,
    {"A_FireEGattlingGun", A_FireEGattlingGun}
    ,
    {"A_FireCGattlingGun", A_FireCGattlingGun}
    ,
    {"A_Fire3GattlingGun", A_Fire3GattlingGun}
    ,
    {"A_FireUGattlingGun", A_FireUGattlingGun}
    ,
    {"A_UranusPlayerChaingunBlur", A_UranusPlayerChaingunBlur}
    ,
    {"A_FireMGattlingGun", A_FireMGattlingGun}
    ,
    {"A_FireORifle", A_FireORifle}
    ,
    {"A_FireMRifle", A_FireMRifle}
    ,
    {"A_FireORevolver", A_FireORevolver}
    ,
    {"A_FireRShotgun", A_FireRShotgun}
    ,
    {"A_FireMSyringe", A_FireMSyringe}
    ,
    {"A_FireWMissile", A_FireWMissile}
    ,
    {"A_FireLMissile", A_FireLMissile}
    ,
    {"A_FireCMissile", A_FireCMissile}
    ,
    {"A_FireFlame", A_FireFlame}
    ,
    {"A_FireCFlame", A_FireCFlame}
    ,
    {"A_Fire3Flame", A_Fire3Flame}
    ,
    {"A_FireCMissile1", A_FireCMissile1}
    ,
    {"A_FireCMissile2", A_FireCMissile2}
    ,
    {"A_FireCMissile2NA", A_FireCMissile2NA}
    ,
    {"A_FireCMissile3", A_FireCMissile3}
    ,
    {"A_KnifeThrust", A_KnifeThrust}
    ,
    {"A_CKnifeThrust", A_CKnifeThrust}
    ,
    {"A_LoadMachineGun", A_LoadMachineGun}
    ,
    {"A_SpawnMachineGun", A_SpawnMachineGun}
    ,
    {"A_SpawnFirstAidKit", A_SpawnFirstAidKit}
    ,
    {"A_SpawnClip", A_SpawnClip}
    ,
    {"A_SpawnFlameTAmmo", A_SpawnFlameTAmmo}
    ,
    {"A_SpawnSilverKey", A_SpawnSilverKey}
    ,
    {"A_SpawnGoldKey", A_SpawnGoldKey}
    ,
    {"A_SpawnSClip", A_SpawnSClip}
    ,
    {"A_Spawn3SClip", A_Spawn3SClip}
    ,
    {"A_SpawnMachineGunSClip", A_SpawnMachineGunSClip}
    ,
    {"A_SpawnSFlameTAmmo", A_SpawnSFlameTAmmo}
    ,
    {"A_SpawnSSilverKey", A_SpawnSSilverKey}
    ,
    {"A_SpawnSGoldKey", A_SpawnSGoldKey}
    ,
    {"A_SpawnAClip", A_SpawnAClip}
    ,
    {"A_SpawnCYellowKey", A_SpawnCYellowKey}
    ,
    {"A_SpawnOClip", A_SpawnOClip}
    ,
    {"A_SpawnIClip", A_SpawnIClip}
    ,
    {"A_SpawnUClip", A_SpawnUClip}
    ,
    {"A_SpawnIFlameTAmmo", A_SpawnIFlameTAmmo}
    ,
    {"A_WolfBullet", A_WolfBullet}
    ,
    {"A_BossBullet", A_BossBullet}
    ,
    {"A_SSAttack", A_SSAttack}
    ,
    {"A_OfficerAttack", A_OfficerAttack}
    ,
    {"A_EliteGuardAttack", A_EliteGuardAttack}
    ,
    {"A_SODMPSSAttack", A_SODMPSSAttack}
    ,
    {"A_SODMPEliteGuardAttack", A_SODMPEliteGuardAttack}
    ,
    {"A_OMS2SSAttack", A_OMS2SSAttack}
    ,
    {"A_FoxAttack", A_FoxAttack}
    ,
    {"A_ChaingunZombieAttack", A_ChaingunZombieAttack}
    ,
    {"A_WRocketAttack", A_WRocketAttack}
    ,
    {"A_Explosion", A_Explosion}
    ,
    {"A_LRocketAttack", A_LRocketAttack}
    ,
    {"A_Graze", A_Graze}
    ,
    {"A_Shatter", A_Shatter}
    ,
    {"A_SchabbsAttack", A_SchabbsAttack}
    ,
    {"A_Fsplash", A_Fsplash}
    ,
    {"A_FakeHitlerAttack", A_FakeHitlerAttack}
    ,
    {"A_BGFlameAttack", A_BGFlameAttack}
    ,
    {"A_DeathKnightAttack1", A_DeathKnightAttack1}
    ,
    {"A_DeathKnightAttack2", A_DeathKnightAttack2}
    ,
    {"A_AngelAttack1", A_AngelAttack1}
    ,
    {"A_AngelAttack2", A_AngelAttack2}
    ,
    {"A_TrackingAlways", A_TrackingAlways}
    ,
    {"A_RobotAttack", A_RobotAttack}
    ,
    {"A_DevilAttack1", A_DevilAttack1}
    ,
    {"A_DevilAttack2", A_DevilAttack2}
    ,
    {"A_DevilAttack3", A_DevilAttack3}
    ,
    {"A_MBatAttack", A_MBatAttack}
    ,
    {"A_CatMissle1", A_CatMissle1}
    ,
    {"A_CatMissle2", A_CatMissle2}
    ,
    {"A_NemesisAttack", A_NemesisAttack}
    ,
    {"A_Tracking", A_Tracking}
    ,
    {"A_MadDocAttack", A_MadDocAttack}
    ,
    {"A_BioBlasterAttack", A_BioBlasterAttack}
    ,
    {"A_ChimeraAttack1", A_ChimeraAttack1}
    ,
    {"A_BalrogAttack", A_BalrogAttack}
    ,
    {"A_OmegaAttack1", A_OmegaAttack1}
    ,
    {"A_OmegaAttack2", A_OmegaAttack2}
    ,
    {"A_OmegaAttack3", A_OmegaAttack3}
    ,
    {"A_DrakeAttack1", A_DrakeAttack1}
    ,
    {"A_DrakeAttack2", A_DrakeAttack2}
    ,
    {"A_DrakeAttack3", A_DrakeAttack3}
    ,
    {"A_StalkerAttack", A_StalkerAttack}
    ,
    {"A_HellGuardAttack", A_HellGuardAttack}
    ,
    {"A_SchabbsDemonAttack", A_SchabbsDemonAttack}
    ,
    {"A_NormalMelee", A_NormalMelee}
    ,
    {"A_LightMelee", A_LightMelee}
    ,
    {"A_DrainAttack", A_DrainAttack}
    ,
    {"A_GhostDrainAttack", A_GhostDrainAttack}
    ,
    {"A_RGhostDrainAttack", A_RGhostDrainAttack}
    ,
    {"A_RLGhostDrainAttack", A_RLGhostDrainAttack}
    ,
    {"A_TrollMelee", A_TrollMelee}
    ,
    {"A_HeavyMelee", A_HeavyMelee}
    ,
    {"A_MetalWalk", A_MetalWalk}
    ,
    {"A_TreadMoveN", A_TreadMoveN}
    ,
    {"A_TreadMoveS", A_TreadMoveS}
    ,
    {"A_WaterTrollSwim", A_WaterTrollSwim}
    ,
    {"A_WaterTrollChase", A_WaterTrollChase}
    ,
    {"A_ChaseNA", A_ChaseNA}
    ,
    {"A_Hitler", A_Hitler}
    ,
    {"A_SpectreRespawn", A_SpectreRespawn}
    ,
    {"A_LSpectreRespawn", A_LSpectreRespawn}
    ,
    {"A_LGreenMistSplit", A_LGreenMistSplit}
    ,
    {"A_NewSkeleton", A_NewSkeleton}
    ,
    {"A_NewSkeleton2", A_NewSkeleton2}
    ,
    {"A_ShadowNemRespawn", A_ShadowNemRespawn}
    ,
    {"A_ChimeraStage2", A_ChimeraStage2}
    ,
    {"A_GeneralDecide", A_GeneralDecide}
    ,
    {"A_WillDecide", A_WillDecide}
    ,
    {"A_DeathKnightDecide", A_DeathKnightDecide}
    ,
    {"A_AngelDecide", A_AngelDecide}
    ,
    {"A_AngelAttack1Continue1", A_AngelAttack1Continue1}
    ,
    {"A_AngelAttack1Continue2", A_AngelAttack1Continue2}
    ,
    {"A_QuarkDecide", A_QuarkDecide}
    ,
    {"A_RobotDecide", A_RobotDecide}
    ,
    {"A_DevilDecide", A_DevilDecide}
    ,
    {"A_DevilAttack2Continue1", A_DevilAttack2Continue1}
    ,
    {"A_DevilAttack2Continue2", A_DevilAttack2Continue2}
    ,
    {"A_Devil2Decide", A_Devil2Decide}
    ,
    {"A_Devil2Attack2Continue1", A_Devil2Attack2Continue1}
    ,
    {"A_Devil2Attack2Continue2", A_Devil2Attack2Continue2}
    ,
    {"A_BlackDemonDecide", A_BlackDemonDecide}
    ,
    {"A_BlackDemonDecide2", A_BlackDemonDecide2}
    ,
    {"A_SkelDecideResurect", A_SkelDecideResurect}
    ,
    {"A_SkelDecideResurectTime", A_SkelDecideResurectTime}
    ,
    {"A_Skel2DecideResurect", A_Skel2DecideResurect}
    ,
    {"A_Skel2DecideResurectTime", A_Skel2DecideResurectTime}
    ,
    {"A_WaterTrollDecide", A_WaterTrollDecide}
    ,
    {"A_NemesisDecide", A_NemesisDecide}
    ,
    {"A_MullerDecide", A_MullerDecide}
    ,
    {"A_Angel2Decide", A_Angel2Decide}
    ,
    {"A_Angel2Attack1Continue1", A_Angel2Attack1Continue1}
    ,
    {"A_Angel2Attack1Continue2", A_Angel2Attack1Continue2}
    ,
    {"A_PoopDecide", A_PoopDecide}
    ,
    {"A_SchabbsDDecide", A_SchabbsDDecide}
    ,
    {"A_CandelabraDecide", A_CandelabraDecide}
    ,
    {"A_LightningDecide", A_LightningDecide}
    ,
    {"A_WaterDropDecide", A_WaterDropDecide}
    ,
    {"A_RocksDecide", A_RocksDecide}
    ,
    {"A_PlayerDead", A_PlayerDead}
    ,
    {"A_GScream", A_GScream}
    ,
    {"A_BRefire", A_BRefire}
    ,
    {"A_HitlerSlop", A_HitlerSlop}
    ,
    {"A_PacmanAttack", A_PacmanAttack}
    ,
    {"A_PinkPacmanBlur1", A_PinkPacmanBlur1}
    ,
    {"A_RedPacmanBlur1", A_RedPacmanBlur1}
    ,
    {"A_OrangePacmanBlur1", A_OrangePacmanBlur1}
    ,
    {"A_BluePacmanBlur1", A_BluePacmanBlur1}
    ,
    {"A_PinkPacmanBlur2", A_PinkPacmanBlur2}
    ,
    {"A_RedPacmanBlur2", A_RedPacmanBlur2}
    ,
    {"A_OrangePacmanBlur2", A_OrangePacmanBlur2}
    ,
    {"A_BluePacmanBlur2", A_BluePacmanBlur2}
    ,
    {"A_PinkPacmanBlurA", A_PinkPacmanBlurA}
    ,
    {"A_RedPacmanBlurA", A_RedPacmanBlurA}
    ,
    {"A_PinkPacmanBlurA2", A_PinkPacmanBlurA2}
    ,
    {"A_RedPacmanBlurA2", A_RedPacmanBlurA2}
    ,
    {"A_PacmanSpawnerAwake", A_PacmanSpawnerAwake}
    ,
    {"A_PacmanSwastika", A_PacmanSwastika}
    ,
    {"A_PacmanBJHead", A_PacmanBJHead}
    ,
    {"A_AngMissileBlur1", A_AngMissileBlur1}
    ,
    {"A_AngMissileBlur2", A_AngMissileBlur2}
    ,
    {"A_AngMissileBlur3", A_AngMissileBlur3}
    ,
    {"A_AngMissileBlur4", A_AngMissileBlur4}
    ,
    {"A_AngelFireStart", A_AngelFireStart}
    ,
    {"A_AngelFire", A_AngelFire}
    ,
    {"A_AngelStartFire", A_AngelStartFire}
    ,
    {"A_AngelFireCrackle", A_AngelFireCrackle}
    ,
    {"A_AngelFireTarget", A_AngelFireTarget}
    ,
    {"A_AngelFireAttack", A_AngelFireAttack}
    ,
    {"A_DevilFireTarget", A_DevilFireTarget}
    ,
    {"A_SpawnRain", A_SpawnRain}
    ,
    {"A_ZombieFaceTarget", A_ZombieFaceTarget}
    ,
    {"A_EyeProjectile", A_EyeProjectile}
    ,
    {"A_WaterTrollSplashR", A_WaterTrollSplashR}
    ,
    {"A_WaterTrollSplashL", A_WaterTrollSplashL}
    ,
    {"A_ChestOpenSmall", A_ChestOpenSmall}
    ,
    {"A_ChestOpenMed", A_ChestOpenMed}
    ,
    {"A_ChestOpenLarge", A_ChestOpenLarge}
    ,
    {"A_PlayingDeadActive", A_PlayingDeadActive}
    ,
    {"A_RavenRefire", A_RavenRefire}
    ,
    {"A_DirtDevilChase", A_DirtDevilChase}
    ,
    {"A_DrakeFaceTarget", A_DrakeFaceTarget}
    ,
    {"A_DrakeHead", A_DrakeHead}
    ,
    {"A_StalkerFaceTarget", A_StalkerFaceTarget}
    ,
    {"A_StalkerProjectileSound", A_StalkerProjectileSound}
    ,
    {"A_FireGreenBlobMissile", A_FireGreenBlobMissile}
    ,
    {"A_FireRedBlobMissile", A_FireRedBlobMissile}
    ,
    {"A_MotherBlobAttack", A_MotherBlobAttack}
    ,
    {"A_SpawnGoldKeyCard", A_SpawnGoldKeyCard}
    ,
    {"A_ShadowNemesisResurectSound", A_ShadowNemesisResurectSound}
    ,
    {0, 0}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------
