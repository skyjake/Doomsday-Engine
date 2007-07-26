/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

#ifndef __P_INTER__
#define __P_INTER__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#ifdef __GNUG__
#pragma interface
#endif

boolean         P_GivePower(player_t *, int);
boolean         P_TakePower(player_t *player, int power);
void            P_GiveKey(player_t *player, keytype_t card);
boolean         P_GiveBody(player_t *player, int num);
void            P_GiveBackpack(player_t *player);
boolean         P_GiveWeapon(player_t *player, weapontype_t weapon, boolean dropped);

#endif
