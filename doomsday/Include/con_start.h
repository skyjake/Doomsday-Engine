//===========================================================================
// CON_START.H
//===========================================================================
#ifndef __DOOMSDAY_STARTUP_CONSOLE_H__
#define __DOOMSDAY_STARTUP_CONSOLE_H__

extern int		startupLogo;
extern boolean	startupScreen;

void Con_StartupInit(void);
void Con_StartupDone(void);
void Con_SetBgFlat(int lump);
void Con_DrawStartupBackground(void);
void Con_DrawStartupScreen(int show);

#endif