/**\file m_args.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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
 * Command Line Arguments
 */

#ifndef LIBDENG_LINE_ARGS_H
#define LIBDENG_LINE_ARGS_H

#ifdef __cplusplus
extern "C" {
#endif

void            ArgInit(const char* cmdline);
void            ArgShutdown(void);
void            ArgAbbreviate(const char* longname, const char* shortname);

int             Argc(void);
const char*     Argv(int i);
const char**    ArgvPtr(int i);
const char*     ArgNext(void);
int             ArgCheck(const char* check);
int             ArgCheckWith(const char* check, int num);
int             ArgExists(const char* check);
int             ArgIsOption(int i);
int             ArgRecognize(const char* first, const char* second);

#ifdef __cplusplus
}
#endif

#endif /* LIBDENG_LINE_ARGS_H */
