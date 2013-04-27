/**\file p_enemy.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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
 * Enemy thinking, AI (jDoom-specific).
 */

#ifndef LIBDOOM_P_ENEMY_H
#define LIBDOOM_P_ENEMY_H

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "jdoom.h"

/**
 * Global state of boss brain.
 */
typedef struct braindata_s {
    int easy;
    int targetOn;
    int numTargets;
    int maxTargets;
    mobj_t** targets;
} braindata_t;

DENG_EXTERN_C braindata_t brain;

DENG_EXTERN_C boolean bossKilled;

#ifdef __cplusplus
extern "C" {
#endif

void P_BrainInitForMap(void);
void P_BrainShutdown(void);
void P_BrainClearTargets(void);
void P_BrainAddTarget(mobj_t* mo);
void        P_NoiseAlert(mobj_t *target, mobj_t *emmiter);
int         P_Massacre(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDOOM_ENEMY_H */
