/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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

#ifndef __DD_SAVEGAME_TEXTURE_ARCHIVE_H__
#define __DD_SAVEGAME_TEXTURE_ARCHIVE_H__

#define MAX_ARCHIVED_TEXTURES	1024

typedef struct {
	char            name[9];
} texentry_t;

typedef struct {
	texentry_t      table[MAX_ARCHIVED_TEXTURES];
	int             count;
} texarchive_t;

extern texarchive_t flat_archive;
extern texarchive_t tex_archive;

void            SV_InitTextureArchives(void);
unsigned short  SV_TextureArchiveNum(int texnum);
unsigned short  SV_FlatArchiveNum(int flatnum);
int             SV_GetArchiveFlat(int archivenum);
int             SV_GetArchiveTexture(int archivenum);
void            SV_WriteTextureArchive(void);
void            SV_ReadTextureArchive(void);

#endif
