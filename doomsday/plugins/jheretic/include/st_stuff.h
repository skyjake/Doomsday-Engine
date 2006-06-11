#ifndef __ST_STUFF_H__
#define __ST_STUFF_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "h_player.h"

// Size of statusbar.
// Now sensitive for scaling.
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

// Called when the console player->class changes.
void    SB_SetClassData(void);

// Called to execute the change of player class.
void    SB_ChangePlayerClass(player_t *player, int newclass);

// Called by startup code.
void    ST_Register(void);
void    ST_Init(void);

void    ST_updateGraphics(void);

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
