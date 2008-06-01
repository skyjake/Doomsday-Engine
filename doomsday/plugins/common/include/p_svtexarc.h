/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * p_svtexarc.c: Archived texture names (save games).
 */

#ifndef __DD_SAVEGAME_TEXTURE_ARCHIVE_H__
#define __DD_SAVEGAME_TEXTURE_ARCHIVE_H__

void            SV_InitTextureArchives(void);

unsigned short  SV_MaterialArchiveNum(int materialID, materialtype_t type);
int             SV_GetArchiveMaterial(int archiveID, materialtype_t type);

void            SV_WriteTextureArchive(void);
void            SV_ReadTextureArchive(void);

#endif
