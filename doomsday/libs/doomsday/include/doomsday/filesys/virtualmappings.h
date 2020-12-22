/** @file virtualappings.h  Maps WAD lumps and native files to virtual FS1 files.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_FILESYS_VIRTUALMAPPINGS_H
#define LIBDOOMSDAY_FILESYS_VIRTUALMAPPINGS_H

#include "../libdoomsday.h"

/**
 * (Re-)Initialize the VFS path mappings. Checks the -vdmap command line
 * options.
 */
DE_EXTERN_C LIBDOOMSDAY_PUBLIC void FS_InitVirtualPathMappings();

/**
 * (Re-)Initialize the virtual path => WAD lump mappings.
 * @note Should be called after WADs have been processed.
 */
DE_EXTERN_C LIBDOOMSDAY_PUBLIC void FS_InitPathLumpMappings();

#endif // LIBDOOMSDAY_FILESYS_VIRTUALMAPPINGS_H
