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
 * dd_input.h: Input Subsystem
 */

#ifndef __DOOMSDAY_BASEINPUT_H__
#define __DOOMSDAY_BASEINPUT_H__

#include "dd_share.h"              // For event_t.
#include "con_decl.h"

extern int      repWait1, repWait2;
extern int      keyRepeatDelay1, keyRepeatDelay2;   // milliseconds
extern int      mouseFilter;
extern int      mouseDisableX, mouseDisableY;
extern int      mouseInverseY;
extern int      mouseWheelSensi;
extern int      joySensitivity;
extern int      joyDeadZone;
extern byte     showScanCodes;
extern boolean  shiftDown, altDown;

void            DD_RegisterInput(void);
void            DD_InitInput(void);
void            DD_ShutdownInput(void);
void            DD_StartInput(void);
void            DD_StopInput(void);

void            DD_ReadKeyboard(void);
void            DD_ReadMouse(timespan_t ticLength);
void            DD_ReadJoystick(void);

void            DD_PostEvent(event_t *ev);
void            DD_ProcessEvents(timespan_t ticLength);
void            DD_ClearEvents(void);
void            DD_ClearKeyRepeaters(void);
byte            DD_ScanToKey(byte scan);
byte            DD_KeyToScan(byte key);
byte            DD_ModKey(byte key);
int     DD_IsKeyDown(int code);
int     DD_IsMouseBDown(int code);
int     DD_IsJoyBDown(int code);

#endif
