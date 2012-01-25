/**\file
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
 * s_wav.h: WAV Files
 */

#ifndef __DOOMSDAY_SOUND_WAVE_FILE_H__
#define __DOOMSDAY_SOUND_WAVE_FILE_H__

int             WAV_CheckFormat(const char* data);
void*           WAV_Load(const char* filename, int* bits, int* rate,
                         int* samples);
void*           WAV_MemoryLoad(const byte* data, size_t datalength,
                               int* bits, int* rate, int* samples);

#endif
