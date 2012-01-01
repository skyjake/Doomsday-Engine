/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef __EXTENDED_GENERAL_H__
#define __EXTENDED_GENERAL_H__

#include "p_xgline.h"
#include "p_xgsec.h"

extern int xgDev;
extern boolean xgDataLumps;

// Debug message printer.
void            XG_Dev(const char *format, ...) PRINTF_F(1,2);

// Called once, at post init.
void            XG_ReadTypes(void);

// Init both XG lines and sectors. Called for each map.
void            XG_Init(void);

// Thinks for XG lines and sectors.
void            XG_Ticker(void);

// Updates XG state during engine reset.
void            XG_Update(void);

//void            XG_WriteTypes(FILE * file);
void            XG_ReadTypes(void);

linetype_t     *XG_GetLumpLine(int id);
sectortype_t   *XG_GetLumpSector(int id);

#endif
