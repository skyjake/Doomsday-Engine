/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
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

void        cht_LaserFunc(player_t *plyr); // d64tc

boolean     cht_Responder(event_t *ev);

#endif
