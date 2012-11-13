/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/*
 * s_logic.h: The Logical Sound Manager
 */

#ifndef __DOOMSDAY_SOUND_LOGICAL_H__
#define __DOOMSDAY_SOUND_LOGICAL_H__

#ifdef __cplusplus
extern "C" {
#endif

void            Sfx_InitLogical(void);
void            Sfx_PurgeLogical(void);
void            Sfx_StartLogical(int id, mobj_t *origin, boolean isRepeating);
int             Sfx_StopLogical(int id, mobj_t *origin);
boolean         Sfx_IsPlaying(int id, mobj_t *origin);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
