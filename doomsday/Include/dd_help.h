//===========================================================================
// DD_HELP.H
//===========================================================================
#ifndef __DOOMSDAY_HELP_H__
#define __DOOMSDAY_HELP_H__

// Help string types.
enum 
{
	HST_DESCRIPTION,
	HST_CONSOLE_VARIABLE,
	HST_DEFAULT_VALUE
};

void	DD_InitHelp(void);
void	DD_ShutdownHelp(void);
void *	DH_Find(char *id);
char *	DH_GetString(void *found, int type);

#endif 