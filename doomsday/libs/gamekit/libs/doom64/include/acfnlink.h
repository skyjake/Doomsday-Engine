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

/**
 * acfnlink.h:
 */

#ifndef __ACTION_LINK_H__
#define __ACTION_LINK_H__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "dd_share.h"
#include <doomsday/world/actions.h>

DE_EXTERN_C actionlink_t actionlinks[];

void C_DECL A_BabyMetal();
void C_DECL A_BFGsound();
void C_DECL A_BFGSpray();
void C_DECL A_BossDeath();
void C_DECL A_BruisAttack();
void C_DECL A_BspiAttack();
void C_DECL A_Chase();
void C_DECL A_RectChase();
void C_DECL A_CheckReload();
void C_DECL A_CyberAttack();
void C_DECL A_Explode();
void C_DECL A_BarrelExplode();
void C_DECL A_FaceTarget();
void C_DECL A_BspiFaceTarget();
void C_DECL A_Fall();
void C_DECL A_FatAttack1();
void C_DECL A_FatAttack2();
void C_DECL A_FatAttack3();
void C_DECL A_FatRaise();
void C_DECL A_FireBFG();
void C_DECL A_FireCGun();
void C_DECL A_FireMissile();
void C_DECL A_FirePistol();
void C_DECL A_FirePlasma();
void C_DECL A_FireShotgun();
void C_DECL A_FireShotgun2();
void C_DECL A_GunFlash();
void C_DECL A_HeadAttack();
void C_DECL A_Hoof();
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
void C_DECL A_PosAttack();
void C_DECL A_Punch();
void C_DECL A_Raise();
void C_DECL A_PlasmaShock();
void C_DECL A_ReFire();
void C_DECL A_SargAttack();
void C_DECL A_Saw();
void C_DECL A_Scream();
void C_DECL A_SkelFist();
void C_DECL A_SkelMissile();
void C_DECL A_SkelWhoosh();
void C_DECL A_SkullAttack();
void C_DECL A_SpidRefire();
void C_DECL A_SPosAttack();
void C_DECL A_Tracer();
void C_DECL A_TroopAttack();
void C_DECL A_WeaponReady();
void C_DECL A_XScream();

//jd64
void C_DECL A_Lasersmoke();
void C_DECL A_FireSingleLaser();
void C_DECL A_FireDoubleLaser();
void C_DECL A_FireDoubleLaser1();
void C_DECL A_FireDoubleLaser2();
void C_DECL A_PossSpecial();
void C_DECL A_SposSpecial();
void C_DECL A_TrooSpecial();
void C_DECL A_SargSpecial();
void C_DECL A_HeadSpecial();
void C_DECL A_SkulSpecial();
void C_DECL A_Bos2Special();
void C_DECL A_BossSpecial();
void C_DECL A_PainSpecial();
void C_DECL A_FattSpecial();
void C_DECL A_BabySpecial();
void C_DECL A_CybrSpecial();
void C_DECL A_RectSpecial();
void C_DECL A_Rocketpuff();
void C_DECL A_CyberDeath();
void C_DECL A_TroopClaw();
void C_DECL A_MotherFloorFire();
void C_DECL A_MotherMissle();
void C_DECL A_SetFloorFire();
void C_DECL A_MotherBallExplode();
void C_DECL A_RectTracerPuff();
void C_DECL A_TargetCamera();
void C_DECL A_ShadowsAction1();
void C_DECL A_ShadowsAction2();
void C_DECL A_EMarineAttack2();

#endif
