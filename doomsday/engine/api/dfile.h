/**\file dfile.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_FILEHANDLE_H
#define LIBDENG_FILESYS_FILEHANDLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dd_types.h"

/**
 * DFile.  DFile Represents a unique (public) file reference in the
 * engine's Virtual File System (vfs).
 *
 * @ingroup fs
 */
struct dfile_s; // The file handle instance (opaque).
typedef struct dfile_s DFile;

/// @return  @c true iff this handle's internal state is valid.
boolean DFile_IsValid(const DFile* hndl);

#if _DEBUG
void DFile_Print(const DFile* hndl);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_FILEHANDLE_H */
