/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * b_main.h: Bindings
 */

#ifndef __DOOMSDAY_BIND_MAIN_H__
#define __DOOMSDAY_BIND_MAIN_H__

void        B_Register(void);
void        B_Init(void);
void        B_Shutdown(void);
boolean     B_Responder(ddevent_t *ev);
void        B_WriteToFile(FILE *file);

struct evbinding_s* B_BindCommand(const char* eventDesc, const char* command);
struct controlbinding_s* B_BindControl(const char* controlDesc, const char* device);
struct dbinding_s* B_GetControlDeviceBindings(int localNum, int control, struct bclass_s** bClass);

void        DD_AddBindClass(struct bindclass_s *);
boolean     B_SetBindClass(uint classID, uint type);

// Utils
// TODO: move to b_util.h
int         B_NewIdentifier(void);
const char *B_ShortNameForKey(int ddkey);
int         B_KeyForShortName(const char *key);
int         DD_GetKeyCode(const char *key);

#endif
