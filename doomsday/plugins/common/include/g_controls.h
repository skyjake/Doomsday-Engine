/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
 * g_controls.h: Common code for game controls
 *
 */

#ifndef __COMMON_CONTROLS_H__
#define __COMMON_CONTROLS_H__

// Game registered bindClasses
enum {
    GBC_CLASS1 = NUM_DDBINDCLASSES,
    GBC_CLASS2,
    GBC_CLASS3,
    GBC_MENUHOTKEY,
    GBC_CHAT,
    GBC_MESSAGE
};

void        G_ControlRegister(void);
void        G_DefaultBindings(void);
void        G_RegisterBindClasses(void);
void        G_RegisterPlayerControls(void);

int         G_PrivilegedResponder(event_t *event);

boolean     G_AdjustControlState(event_t* ev);

void        G_LookAround(int pnum);
void        G_SpecialButton(int pnum);

void        G_ResetMousePos(void);
void        G_ControlReset(int pnum);

float       G_GetLookOffset(int pnum);
void        G_ResetLookOffset(int pnum);

#endif
