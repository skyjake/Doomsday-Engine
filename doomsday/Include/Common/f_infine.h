#ifndef __INFINE_H__
#define __INFINE_H__

extern boolean brief_disabled;

void	FI_StartScript(char *finalescript, boolean after);
void	FI_End(void);
int		FI_Briefing(int episode, int map);
int		FI_Debriefing(int episode, int map);
int		FI_SkipRequest(void);
void	FI_Ticker(void);
int		FI_Responder(event_t *ev);
void	FI_Drawer(void);

int		CCmdStopInFine(int argc, char **argv);

#endif