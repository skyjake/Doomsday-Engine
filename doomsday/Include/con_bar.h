//===========================================================================
// CON_BAR.H
//===========================================================================
#ifndef __DOOMSDAY_CONSOLE_BAR_H__
#define __DOOMSDAY_CONSOLE_BAR_H__

#define	PBARF_INIT			1
#define PBARF_SET			2
#define PBARF_DONTSHOW		4
#define PBARF_NOBACKGROUND	8
#define PBARF_NOBLIT		16

extern int progress_enabled;

void Con_InitProgress(const char *title, int full);
void Con_HideProgress(void);
void Con_Progress(int count, int flags);

#endif 