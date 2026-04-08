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

#pragma once

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "dd_share.h"
#include <doomsday/world/actions.h>

typedef struct mobj_s   mobj_t;
typedef struct player_s player_t;
typedef struct pspdef_s pspdef_t;

#define ACTION_MOBJ(name)   void C_DECL name(mobj_t *actor)
#define ACTION_WEAPON(name) void C_DECL name(player_t *player, pspdef_t *psp)

#ifdef __cplusplus
extern "C" {
#endif

DE_EXTERN_C actionlink_t actionlinks[];

ACTION_MOBJ(A_BabyMetal);
ACTION_WEAPON(A_BFGsound);
ACTION_MOBJ(A_BFGSpray);
ACTION_MOBJ(A_BossDeath);
ACTION_MOBJ(A_BruisAttack);
ACTION_MOBJ(A_BspiAttack);
ACTION_MOBJ(A_Chase);
ACTION_MOBJ(A_RectChase);
ACTION_WEAPON(A_CheckReload);
ACTION_MOBJ(A_CyberAttack);
ACTION_MOBJ(A_Explode);
ACTION_MOBJ(A_BarrelExplode);
ACTION_MOBJ(A_FaceTarget);
ACTION_MOBJ(A_BspiFaceTarget);
ACTION_MOBJ(A_Fall);
ACTION_MOBJ(A_FatAttack1);
ACTION_MOBJ(A_FatAttack2);
ACTION_MOBJ(A_FatAttack3);
ACTION_MOBJ(A_FatRaise);
ACTION_WEAPON(A_FireBFG);
ACTION_WEAPON(A_FireCGun);
ACTION_WEAPON(A_FireMissile);
ACTION_WEAPON(A_FirePistol);
ACTION_WEAPON(A_FirePlasma);
ACTION_WEAPON(A_FireShotgun);
ACTION_WEAPON(A_FireShotgun2);
ACTION_WEAPON(A_GunFlash);
ACTION_MOBJ(A_HeadAttack);
ACTION_MOBJ(A_Hoof);
ACTION_WEAPON(A_Light0);
ACTION_WEAPON(A_Light1);
ACTION_WEAPON(A_Light2);
ACTION_WEAPON(A_LoadShotgun2);
ACTION_MOBJ(A_Look);
ACTION_WEAPON(A_Lower);
ACTION_MOBJ(A_Metal);
ACTION_WEAPON(A_OpenShotgun2);
ACTION_MOBJ(A_Pain);
ACTION_MOBJ(A_PainAttack);
ACTION_MOBJ(A_PainDie);
ACTION_MOBJ(A_PosAttack);
ACTION_WEAPON(A_Punch);
ACTION_WEAPON(A_Raise);
ACTION_WEAPON(A_PlasmaShock);
ACTION_WEAPON(A_ReFire);
ACTION_MOBJ(A_SargAttack);
ACTION_WEAPON(A_Saw);
ACTION_MOBJ(A_Scream);
ACTION_MOBJ(A_SkelFist);
ACTION_MOBJ(A_SkelMissile);
ACTION_MOBJ(A_SkelWhoosh);
ACTION_MOBJ(A_SkullAttack);
ACTION_MOBJ(A_SpidRefire);
ACTION_MOBJ(A_SPosAttack);
ACTION_MOBJ(A_Tracer);
ACTION_MOBJ(A_TroopAttack);
ACTION_WEAPON(A_WeaponReady);
ACTION_MOBJ(A_XScream);

//jd64
ACTION_MOBJ(A_Lasersmoke);
ACTION_WEAPON(A_FireSingleLaser);
ACTION_WEAPON(A_FireDoubleLaser);
ACTION_WEAPON(A_FireDoubleLaser1);
ACTION_WEAPON(A_FireDoubleLaser2);
ACTION_MOBJ(A_PossSpecial);
ACTION_MOBJ(A_SposSpecial);
ACTION_MOBJ(A_TrooSpecial);
ACTION_MOBJ(A_SargSpecial);
ACTION_MOBJ(A_HeadSpecial);
ACTION_MOBJ(A_SkulSpecial);
ACTION_MOBJ(A_Bos2Special);
ACTION_MOBJ(A_BossSpecial);
ACTION_MOBJ(A_PainSpecial);
ACTION_MOBJ(A_FattSpecial);
ACTION_MOBJ(A_BabySpecial);
ACTION_MOBJ(A_CybrSpecial);
ACTION_MOBJ(A_RectSpecial);
ACTION_MOBJ(A_Rocketpuff);
ACTION_MOBJ(A_CyberDeath);
ACTION_MOBJ(A_TroopClaw);
ACTION_MOBJ(A_MotherFloorFire);
ACTION_MOBJ(A_MotherMissle);
ACTION_MOBJ(A_SetFloorFire);
ACTION_MOBJ(A_MotherBallExplode);
ACTION_MOBJ(A_RectTracerPuff);
ACTION_MOBJ(A_TargetCamera);
ACTION_MOBJ(A_ShadowsAction1);
ACTION_MOBJ(A_ShadowsAction2);
ACTION_MOBJ(A_EMarineAttack2);

#ifdef __cplusplus
} // extern "C"
#endif

