/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * m_cheat.h: Cheat code checking.
 */

#ifndef __M_CHEAT__
#define __M_CHEAT__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

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

void        Cht_Init(void);

int         Cht_CheckCheat(cheatseq_t *cht, char key);

void        Cht_GetParam(cheatseq_t *cht, char *buffer);

void        Cht_GodFunc(player_t *plyr);
void        Cht_SuicideFunc(player_t *plyr);
void        Cht_GiveFunc(player_t *plyr, boolean weapons, boolean ammo,
                         boolean armor, boolean cards, cheatseq_t *cht);
void        Cht_MusicFunc(player_t *plyr, char *buf);
void        Cht_NoClipFunc(player_t *plyr);
boolean     Cht_WarpFunc(player_t *plyr, char *buf);
boolean     Cht_PowerUpFunc(player_t *plyr, int i);
void        Cht_ChoppersFunc(player_t *plyr);
void        Cht_MyPosFunc(player_t *plyr);

boolean     Cht_Responder(event_t *ev);

#endif
