#ifndef __INFINE_H__
#define __INFINE_H__

// Condition truth values (that clients can't deduce on their own).
enum {
	FICOND_SECRET,
	FICOND_LEAVEHUB,
	NUM_FICONDS
};

typedef enum infinemode_e {
	FIMODE_LOCAL,
	FIMODE_OVERLAY,
	FIMODE_BEFORE,
	FIMODE_AFTER
} infinemode_t;

extern boolean  fi_active;
extern boolean  brief_disabled;

void            FI_Reset(void);
void            FI_Start(char *finalescript, infinemode_t mode);
void            FI_End(void);
void            FI_SetCondition(int index, boolean value);
int             FI_Briefing(int episode, int map);
int             FI_Debriefing(int episode, int map);
void            FI_DemoEnds(void);
int             FI_SkipRequest(void);
void            FI_Ticker(void);
int             FI_Responder(event_t *ev);
void            FI_Drawer(void);
boolean         FI_IsMenuTrigger(event_t *ev);

DEFCC(CCmdStartInFine);
DEFCC(CCmdStopInFine);

#endif
