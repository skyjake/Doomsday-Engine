/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * dd_input.h: Input Subsystem
 */

#ifndef __DOOMSDAY_BASEINPUT_H__
#define __DOOMSDAY_BASEINPUT_H__

#include "dd_share.h"	// For event_t.
#include "con_decl.h"

extern int		repWait1, repWait2;
extern int		mouseFilter;
extern int		mouseDisableX, mouseDisableY;
extern int		mouseInverseY;
extern int		mouseWheelSensi;
extern int		joySensitivity;
extern int		joyDeadZone;
extern boolean	showScanCodes;
extern boolean	shiftDown, altDown;

void DD_InitInput(void);
void DD_ShutdownInput(void);
void DD_StartInput(void);
void DD_StopInput(void);

void DD_ReadKeyboard(void);
void DD_ReadMouse(void);
void DD_ReadJoystick(void);

void DD_PostEvent(event_t *ev);
void DD_ProcessEvents(void);
void DD_ClearEvents(void);
void DD_ClearKeyRepeaters(void);
byte DD_ScanToKey(byte scan);
byte DD_KeyToScan(byte key);
byte DD_ModKey(byte key);

D_CMD( KeyMap );
D_CMD( DumpKeyMap );

#endif 
