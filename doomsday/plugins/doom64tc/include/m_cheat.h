/* $Id: m_cheat.h 3443 2006-08-01 15:09:16Z danij $
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Cheat code checking.
 */

#ifndef __M_CHEAT__
#define __M_CHEAT__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "doomstat.h"

//
// CHEAT SEQUENCE PACKAGE
//

#define SCRAMBLE(a) \
((((a)&1)<<7) + (((a)&2)<<5) + ((a)&4) + (((a)&8)<<1) \
 + (((a)&16)>>1) + ((a)&32) + (((a)&64)>>5) + (((a)&128)>>7))

typedef struct {
    unsigned char  *sequence;
    unsigned char  *p;

} cheatseq_t;

void        cht_Init(void);

int         cht_CheckCheat(cheatseq_t * cht, char key);

void        cht_GetParam(cheatseq_t * cht, char *buffer);

void        cht_GodFunc(player_t *plyr);
void        cht_SuicideFunc(player_t *plyr);
void        cht_GiveFunc(player_t *plyr, boolean weapons, boolean ammo,
                         boolean armor, boolean cards, cheatseq_t *cheat);
void        cht_MusicFunc(player_t *plyr, char *buf);
void        cht_NoClipFunc(player_t *plyr);
boolean     cht_WarpFunc(player_t *plyr, char *buf);
boolean     cht_PowerUpFunc(player_t *plyr, int i);
void        cht_ChoppersFunc(player_t *plyr);
void        cht_PosFunc(player_t *plyr);

boolean     cht_Responder(event_t *ev);

#endif
