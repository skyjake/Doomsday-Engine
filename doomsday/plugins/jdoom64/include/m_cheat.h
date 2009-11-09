/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

#ifndef M_CHEAT_H
#define M_CHEAT_H

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

void        Cht_Init(void);
boolean     Cht_Responder(event_t* ev);

void        Cht_GodFunc(player_t* plyr);
void        Cht_SuicideFunc(player_t* plyr);
void        Cht_GiveWeaponsFunc(player_t* plyr);
void        Cht_GiveAmmoFunc(player_t* plyr);
void        Cht_GiveKeysFunc(player_t* plyr);
int         Cht_MusicFunc(player_t* plyr, char* buf);
void        Cht_NoClipFunc(player_t* plyr);
boolean     Cht_WarpFunc(player_t* plyr, char* buf);
boolean     Cht_PowerUpFunc(player_t* plyr, int i);
void        Cht_ChoppersFunc(player_t* plyr);
void        Cht_MyPosFunc(player_t* plyr);
void        Cht_LaserFunc(player_t* plyr);
#endif /* M_CHEAT_H */
