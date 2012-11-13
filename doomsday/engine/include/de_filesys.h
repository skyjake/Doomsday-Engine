/**\file de_filesys.h
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

#ifndef LIBDENG_FILESYS_H
#define LIBDENG_FILESYS_H

#include "dd_types.h"
#include "filehandlebuilder.h"
#include "fs_main.h"
#include "fs_util.h"
#include "sys_reslocator.h" /// \todo Does not belong here (higher level abstraction).
#include "sys_direc.h"
#include "sys_findfile.h"

#ifdef UNIX
#  include "unix/sys_path.h"
#endif

#endif /* LIBDENG_FILESYS_H */
