/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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
 * gl_pcx.h: PCX Images
 */

#ifndef __DOOMSDAY_GRAPHICS_PCX_H__
#define __DOOMSDAY_GRAPHICS_PCX_H__

boolean         PCX_GetSize(const char* fn, int* w, int* h);
boolean         PCX_Load(const char* fn, int bufW, int bufH,
                         byte* outBuffer);

boolean         PCX_MemoryGetSize(const void* imageData, int* w, int* h);
boolean         PCX_MemoryLoad(const byte* imgdata, size_t len, int bufW,
                               int bufH, byte* outBuffer);

#endif
