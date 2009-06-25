/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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
 * m_filehash.h: File name/directory search hashes.
 */

#ifndef __DOOMSDAY_MISC_FILE_HASH_H__
#define __DOOMSDAY_MISC_FILE_HASH_H__

typedef void* filehash_t;

filehash_t*     FileHash_Create(const char* pathList);
void            FileHash_Destroy(filehash_t* fh);

boolean         FileHash_Find(filehash_t* fh, char* foundPath,
                              const char* name, size_t len);

#endif
