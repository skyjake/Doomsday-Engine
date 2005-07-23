#ifndef __ST_STUFF_H__
#define __ST_STUFF_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

// Called by main loop.
boolean         ST_Responder(event_t *ev);

// Called by main loop.
void            ST_Ticker(void);

// Called by main loop.
void            ST_Drawer(int fullscreenmode, boolean refresh);

// Called when the console player is spawned on each level.
void            ST_Start(void);

void            ST_Stop(void);

// Called by startup code.
void            ST_Init(void);

void            ST_updateGraphics(void);

// States for status bar code.
typedef enum {
	AutomapState,
	FirstPersonState
} st_stateenum_t;

// States for the chat code.
typedef enum {
	StartChatState,
	WaitDestState,
	GetChatState
} st_chatstateenum_t;

#endif
