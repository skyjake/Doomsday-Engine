// Header file for the action routine link table

#ifndef __ACTION_LINK_H__
#define __ACTION_LINK_H__

#include "dd_share.h"

typedef struct
{
	char *name;	// Name of the routine.
	void (C_DECL *func)();	// Pointer to the function.
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

#endif
