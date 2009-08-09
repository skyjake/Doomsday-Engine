/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * acfnlink.h: Action functions (mobj AI).
 */

#ifndef __ACTION_LINK_H__
#define __ACTION_LINK_H__

#include "dd_share.h"
#include "p_mobj.h"

typedef struct {
    char *name; // Name of the routine.
    void (C_DECL *func)(mobj_t*);  // Pointer to the function.
} actionlink_t;

extern actionlink_t actionlinks[];

void C_DECL A_BabyMetal(mobj_t*);
void C_DECL A_BFGsound(mobj_t*);
void C_DECL A_BFGSpray(mobj_t*);
void C_DECL A_BossDeath(mobj_t*);
void C_DECL A_BrainAwake(mobj_t*);
void C_DECL A_BrainDie(mobj_t*);
void C_DECL A_BrainExplode(mobj_t*);
void C_DECL A_BrainPain(mobj_t*);
void C_DECL A_BrainScream(mobj_t*);
void C_DECL A_BrainSpit(mobj_t*);
void C_DECL A_BruisAttack(mobj_t*);
void C_DECL A_BspiAttack(mobj_t*);
void C_DECL A_Chase(mobj_t*);
void C_DECL A_CheckReload(mobj_t*);
void C_DECL A_CloseShotgun2(mobj_t*);
void C_DECL A_CPosAttack(mobj_t*);
void C_DECL A_CPosRefire(mobj_t*);
void C_DECL A_CyberAttack(mobj_t*);
void C_DECL A_Explode(mobj_t*);
void C_DECL A_FaceTarget(mobj_t*);
void C_DECL A_Fall(mobj_t*);
void C_DECL A_FatAttack1(mobj_t*);
void C_DECL A_FatAttack2(mobj_t*);
void C_DECL A_FatAttack3(mobj_t*);
void C_DECL A_FatRaise(mobj_t*);
void C_DECL A_Fire(mobj_t*);
void C_DECL A_FireBFG(mobj_t*);
void C_DECL A_FireCGun(mobj_t*);
void C_DECL A_FireCrackle(mobj_t*);
void C_DECL A_FireMissile(mobj_t*);
void C_DECL A_FirePistol(mobj_t*);
void C_DECL A_FirePlasma(mobj_t*);
void C_DECL A_FireShotgun(mobj_t*);
void C_DECL A_FireShotgun2(mobj_t*);
void C_DECL A_GunFlash(mobj_t*);
void C_DECL A_HeadAttack(mobj_t*);
void C_DECL A_Hoof(mobj_t*);
void C_DECL A_KeenDie(mobj_t*);
void C_DECL A_Light0(mobj_t*);
void C_DECL A_Light1(mobj_t*);
void C_DECL A_Light2(mobj_t*);
void C_DECL A_LoadShotgun2(mobj_t*);
void C_DECL A_Look(mobj_t*);
void C_DECL A_Lower(mobj_t*);
void C_DECL A_Metal(mobj_t*);
void C_DECL A_OpenShotgun2(mobj_t*);
void C_DECL A_Pain(mobj_t*);
void C_DECL A_PainAttack(mobj_t*);
void C_DECL A_PainDie(mobj_t*);
void C_DECL A_PlayerScream(mobj_t*);
void C_DECL A_PosAttack(mobj_t*);
void C_DECL A_Punch(mobj_t*);
void C_DECL A_Raise(mobj_t*);
void C_DECL A_ReFire(mobj_t*);
void C_DECL A_SargAttack(mobj_t*);
void C_DECL A_Saw(mobj_t*);
void C_DECL A_Scream(mobj_t*);
void C_DECL A_SkelFist(mobj_t*);
void C_DECL A_SkelMissile(mobj_t*);
void C_DECL A_SkelWhoosh(mobj_t*);
void C_DECL A_SkullAttack(mobj_t*);
void C_DECL A_SpawnFly(mobj_t*);
void C_DECL A_SpawnSound(mobj_t*);
void C_DECL A_SpidRefire(mobj_t*);
void C_DECL A_SPosAttack(mobj_t*);
void C_DECL A_StartFire(mobj_t*);
void C_DECL A_Tracer(mobj_t*);
void C_DECL A_TroopAttack(mobj_t*);
void C_DECL A_VileAttack(mobj_t*);
void C_DECL A_VileChase(mobj_t*);
void C_DECL A_VileStart(mobj_t*);
void C_DECL A_VileTarget(mobj_t*);
void C_DECL A_WeaponReady(mobj_t*);
void C_DECL A_XScream(mobj_t*);

#endif
