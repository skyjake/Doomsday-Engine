/**
 * @file de_filesys.h
 *
 * File system. @ingroup fs
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_H
#define LIBDENG_FILESYS_H

#include "dd_types.h"

#include "resourceclass.h"
#include "filetype.h"

#include "filesys/filehandlebuilder.h"
#include "filesys/filenamespace.h"
#include "filesys/fs_main.h"
#include "filesys/fs_util.h"
#include "filesys/locator.h"
#include "filesys/lumpindex.h"
#include "filesys/metafile.h"
#include "filesys/sys_direc.h"
#include "filesys/sys_findfile.h"

#ifdef UNIX
#  include "unix/sys_path.h"
#endif

#endif /* LIBDENG_FILESYS_H */
