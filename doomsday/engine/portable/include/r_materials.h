/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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

#include "def_data.h"

// Material flags:
#define MATF_GLOW               0x1 // Glowing material.
#define MATF_CHANGED            0x8 // Needs update.

typedef struct material_s {
    char            name[9];
    int             ofTypeID;
    materialtype_t  type;
    byte            flags;

    // Associated enhancements/attachments to be used with surfaces:
    ded_decor_t *decoration;
    ded_reflection_t *reflection;
    ded_ptcgen_t *ptcGen;

    // For global animation:
    boolean         inGroup; // True if belongs to some animgroup.
    struct material_s *current;
    struct material_s *next;
    float           inter;
} material_t;

extern uint numMaterials;

void            R_InitMaterials(void);
void            R_ShutdownMaterials(void);
void            R_MarkMaterialsForUpdating(void);

material_t     *R_MaterialCreate(const char *name, int ofTypeID, materialtype_t type);
material_t     *R_GetMaterial(int ofTypeID, materialtype_t type);

boolean         R_IsCustomMaterial(int ofTypeID, materialtype_t type);

int             R_GetMaterialFlags(material_t *material);
boolean         R_GetMaterialColor(const material_t *material, float *rgb);

void            R_PrecacheMaterial(material_t *mat);

// Returns the real DGL texture, if such exists
unsigned int    R_GetMaterialName(int ofTypeID, materialtype_t type);
ded_reflection_t* R_GetMaterialReflection(material_t* mat);
const ded_decor_t* R_GetMaterialDecoration(const material_t* mat);
const ded_ptcgen_t* P_GetMaterialPtcGen(const material_t* mat);

// Not for sprites, etc.
void            R_DeleteMaterialTex(int ofTypeID, materialtype_t type);

// Return values are the original IDs.
int             R_CheckMaterialNumForName(const char *name, materialtype_t type);
int             R_MaterialNumForName(const char *name, materialtype_t type);
const char     *R_MaterialNameForNum(int ofTypeID, materialtype_t type);
void            R_SetMaterialTranslation(material_t *mat,
                                         material_t *current,
                                         material_t *next, float inter);

#endif
