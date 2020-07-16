/**
 * @file wadtool.h
 * WAD creation tool.
 *
 * @author Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef __WAD_TOOL_H__
#define __WAD_TOOL_H__

#define VERSION_STR     "1.1"

typedef struct fname_s {
    struct fname_s *next, *prev;
    char path[256];
    int size;
    int offset;
    char lump[9];
} fname_t;

typedef struct {
    char identification[4];
    int numlumps;
    int infotableofs;
} wadinfo_t;

typedef struct {
    int filepos;
    int size;
    char name[8];
} lumpinfo_t;

#endif
