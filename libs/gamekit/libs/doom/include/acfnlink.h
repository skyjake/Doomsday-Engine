/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * Possibly trigger special effects if there are no more bosses.
 */
ACTION_MOBJ(A_BossDeath);

ACTION_MOBJ(A_BrainAwake);
ACTION_MOBJ(A_BrainDie);
ACTION_MOBJ(A_BrainExplode);
ACTION_MOBJ(A_BrainPain);
ACTION_MOBJ(A_BrainScream);
ACTION_MOBJ(A_BrainSpit);
ACTION_MOBJ(A_BruisAttack);
ACTION_MOBJ(A_BspiAttack);
ACTION_MOBJ(A_Chase);
ACTION_WEAPON(A_CheckReload);
ACTION_WEAPON(A_CloseShotgun2);
ACTION_MOBJ(A_CPosAttack);
ACTION_MOBJ(A_CPosRefire);
ACTION_MOBJ(A_CyberAttack);
ACTION_MOBJ(A_Explode);
ACTION_MOBJ(A_FaceTarget);
ACTION_MOBJ(A_Fall);
ACTION_MOBJ(A_FatAttack1);
ACTION_MOBJ(A_FatAttack2);
ACTION_MOBJ(A_FatAttack3);
ACTION_MOBJ(A_FatRaise);
ACTION_MOBJ(A_Fire);
ACTION_WEAPON(A_FireBFG);
ACTION_WEAPON(A_FireCGun);
ACTION_MOBJ(A_FireCrackle);
ACTION_WEAPON(A_FireMissile);
ACTION_WEAPON(A_FirePistol);
ACTION_WEAPON(A_FirePlasma);
ACTION_WEAPON(A_FireShotgun);
ACTION_WEAPON(A_FireShotgun2);
ACTION_WEAPON(A_GunFlash);
ACTION_MOBJ(A_HeadAttack);
ACTION_MOBJ(A_Hoof);

/**
 * DOOM II special targeting sectors with tag 666.
 */
ACTION_MOBJ(A_KeenDie);

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
ACTION_MOBJ(A_PlayerScream);
ACTION_MOBJ(A_PosAttack);
ACTION_WEAPON(A_Punch);
ACTION_WEAPON(A_Raise);
ACTION_WEAPON(A_ReFire);
ACTION_MOBJ(A_SargAttack);
ACTION_WEAPON(A_Saw);
ACTION_MOBJ(A_Scream);
ACTION_MOBJ(A_SkelFist);
ACTION_MOBJ(A_SkelMissile);
ACTION_MOBJ(A_SkelWhoosh);
ACTION_MOBJ(A_SkullAttack);
ACTION_MOBJ(A_SpawnFly);
ACTION_MOBJ(A_SpawnSound);
ACTION_MOBJ(A_SpidRefire);
ACTION_MOBJ(A_SPosAttack);
ACTION_MOBJ(A_StartFire);
ACTION_MOBJ(A_Tracer);
ACTION_MOBJ(A_TroopAttack);
ACTION_MOBJ(A_VileAttack);
ACTION_MOBJ(A_VileChase);
ACTION_MOBJ(A_VileStart);
ACTION_MOBJ(A_VileTarget);
ACTION_WEAPON(A_WeaponReady);
ACTION_MOBJ(A_XScream);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
