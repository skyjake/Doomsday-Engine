//===========================================================================
// SYS_STWIN.H
//===========================================================================
#ifndef __DOOMSDAY_STARTUP_WINDOW_H__
#define __DOOMSDAY_STARTUP_WINDOW_H__

void	SW_Init(void);
void	SW_Shutdown(void);
int		SW_IsActive(void);
void	SW_Printf(const char *format, ...);
void	SW_DrawBar(void);
void	SW_SetBarPos(int pos);
void	SW_SetBarMax(int max);

#endif 