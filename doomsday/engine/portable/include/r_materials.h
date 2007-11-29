/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * r_materials.h: Materials (texture/flat/sprite/etc abstract interface).
 */

#ifndef __DOOMSDAY_REFRESH_MATERIALS_H__
#define __DOOMSDAY_REFRESH_MATERIALS_H__

// Material flags:
#define MATF_CHANGED            0x1 // Needs update. 

typedef struct material_s {
    char            name[9];
    int             ofTypeID;
    materialtype_t  type;
    short           flags;
} material_t;

void            R_InitMaterials(void);
void            R_ShutdownMaterials(void);
void            R_MarkMaterialsForUpdating(void);

material_t     *R_MaterialCreate(const char *name, int ofTypeID, materialtype_t type);
material_t     *R_GetMaterial(int ofTypeID, materialtype_t type);

boolean         R_IsCustomMaterial(int ofTypeID, materialtype_t type);

int             R_GetMaterialFlags(material_t *material);
void            R_GetMaterialColor(int ofTypeID, materialtype_t type, float *rgb);

// Returns the real DGL texture, if such exists
unsigned int    R_GetMaterialName(int ofTypeID, materialtype_t type);

// Not for sprites, etc.
void            R_DeleteMaterial(int ofTypeID, materialtype_t type);

// Return values are the original IDs.
int             R_CheckMaterialNumForName(const char *name, materialtype_t type);
int             R_MaterialNumForName(const char *name, materialtype_t type);
const char     *R_MaterialNameForNum(int ofTypeID, materialtype_t type);
int             R_SetMaterialTranslation(int ofTypeID, materialtype_t type, int translateTo);

#endif
