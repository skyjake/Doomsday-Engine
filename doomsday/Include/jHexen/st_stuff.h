
#ifndef __ST_STUFF_H__
#define __ST_STUFF_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

// Size of statusbar.
// Now sensitive for scaling.
#define SCREEN_MUL 1
#define ST_HEIGHT   42*SCREEN_MUL
#define ST_WIDTH    SCREENWIDTH
#define ST_Y        (SCREENHEIGHT - ST_HEIGHT)

// Called by main loop.
void    ST_Ticker(void);

// Called by main loop.
void    ST_Drawer(int fullscreenmode, boolean refresh);

// Called when the console player is spawned on each level.
void    ST_Start(void);

void    ST_Stop(void);

// Called by startup code.
void    ST_Register(void);
void    ST_Init(void);

void    ST_updateGraphics(void);

// Called in P_inter & P_enemy
void        ST_doPaletteStuff(boolean forceChange);

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
