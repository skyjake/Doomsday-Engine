/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * sc_man.h:
 */

#ifndef __SC_MAN_H__
#define __SC_MAN_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

void            SC_OpenLump(lumpnum_t lump);
void            SC_OpenFile(char* name);
void            SC_OpenFileCLib(char* name);
void            SC_Close(void);
boolean         SC_GetString(void);
void            SC_MustGetString(void);
void            SC_MustGetStringName(char* name);
boolean         SC_GetNumber(void);
void            SC_MustGetNumber(void);
void            SC_UnGet(void);

boolean         SC_Compare(char* text);
int             SC_MatchString(char** strings);
int             SC_MustMatchString(char** strings);
void            SC_ScriptError(char* message);

int             SC_LastReadInteger(void);
const char*     SC_LastReadString(void);

#endif
