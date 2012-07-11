/**\file lumpfile.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_LUMPFILE_H
#define LIBDENG_FILESYS_LUMPFILE_H

#include "lumpinfo.h"
#include "abstractfile.h"

struct lumpdirectory_s;

/**
 * LumpFile. Runtime representation of a lump-file for use with LumpDirectory
 *
 * @ingroup FS
 */
typedef struct lumpfile_s {
    // Base file.
    abstractfile_t _base;
} LumpFile;

LumpFile* LumpFile_New(DFile* file, const char* path, const LumpInfo* info);
void LumpFile_Delete(LumpFile* lump);

const LumpInfo* LumpFile_LumpInfo(LumpFile* lump, int lumpIdx);

/**
 * Accessors:
 */

/// @return  Number of lumps contained within this file.
int LumpFile_LumpCount(LumpFile* lump);

#endif /// LIBDENG_FILESYS_LUMPFILE_H
