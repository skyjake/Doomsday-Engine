//===========================================================================
// DD_INPUT.H
//===========================================================================
#ifndef __DOOMSDAY_BASEINPUT_H__
#define __DOOMSDAY_BASEINPUT_H__

#include "dd_share.h"	// For event_t.
#include "con_decl.h"

extern event_t	events[MAXEVENTS];
extern int		eventhead;
extern int		eventtail;
extern int		repWait1, repWait2;
extern int		mouseDisableX, mouseDisableY;
extern int		mouseInverseY;
extern int		mouseWheelSensi;
extern int		joySensitivity;
extern int		joyDeadZone;
extern boolean	showScanCodes;
extern int		shiftDown, altDown;

void DD_InitInput(void);

void DD_ReadKeyboard(void);
void DD_ReadMouse(void);
void DD_ReadJoystick(void);

void DD_PostEvent(event_t *ev);
void DD_ProcessEvents(void);
void DD_ClearKeyRepeaters(void);
byte DD_ScanToKey(byte scan);
byte DD_KeyToScan(byte key);
byte DD_ModKey(byte key);

D_CMD( KeyMap );
D_CMD( DumpKeyMap );

#endif 