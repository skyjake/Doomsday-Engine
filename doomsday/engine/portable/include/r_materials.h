/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "s_environ.h"

// Detail texture information.
typedef struct detailinfo_s {
    DGLuint         tex;
    int             width, height;
    float           strength;
    float           scale;
    float           maxDist;
} detailinfo_t;

// Material flags:
#define MATF_NO_DRAW            0x1 // Material should never be drawn.
#define MATF_GLOW               0x2 // Glowing material.
#define MATF_CHANGED            0x8 // Needs update.

typedef struct material_s {
// Material def:
    char            name[9];
    int             ofTypeID;
    materialtype_t  type;
    byte            flags; // MATF_* flags
    short           width, height; // Defined width + height (note, DGL tex may not be the same!)

// Misc:
    materialclass_t envClass; // Used for environmental sound properties.

// DGL texture + detailinfo:
    struct {
        DGLuint         tex; // Name of the associated DGL texture.
        byte            masked;
        float           color[3]; // Average color (for lighting).
    } dgl;
    detailinfo_t    detail; // Detail texture information.

// Associated enhancements/attachments to be used with surfaces:
    ded_decor_t* decoration;
    ded_reflection_t* reflection;
    ded_ptcgen_t* ptcGen;

// For global animation:
    boolean         inGroup; // True if belongs to some animgroup.
    struct material_s* current;
    struct material_s* next;
    float           inter;
} material_t;

extern uint numMaterials;

void            R_InitMaterials(void);
void            R_ShutdownMaterials(void);
void            R_MarkMaterialsForUpdating(void);
void            R_DeleteMaterialTextures(materialtype_t type);
void            R_SetAllMaterialsMinMode(int minMode);

// Lookup:
material_t*     R_GetMaterial(int ofTypeID, materialtype_t type);
material_t*     R_GetMaterialByNum(materialnum_t num);
materialnum_t   R_GetMaterialNum(const material_t* mat);

// Lookup (public):
materialnum_t   R_MaterialCheckNumForName(const char* name, materialtype_t type);
materialnum_t   R_MaterialNumForName(const char* name, materialtype_t type);
const char*     R_MaterialNameForNum(materialnum_t num);

// Creation:
material_t*     R_MaterialCreate(const char* name, int ofTypeID, materialtype_t type);

// Set/Get:
void            R_MaterialSetMinMode(material_t* mat, int minMode);
void            R_MaterialSetTranslation(material_t* mat,
                                         material_t* current,
                                         material_t* next, float inter);

boolean         R_MaterialGetInfo(materialnum_t num, materialinfo_t* info);
void            R_MaterialGetColor(material_t* mat, float* rgb);
ded_reflection_t* R_MaterialGetReflection(material_t* mat);
const ded_decor_t* R_MaterialGetDecoration(material_t* mat);
const ded_ptcgen_t* R_MaterialGetPtcGen(material_t* mat);

// Misc:
boolean         R_MaterialIsCustom(materialnum_t num);
boolean         R_MaterialIsCustom2(const material_t* mat);

void            R_MaterialPrecache(material_t* mat);
void            R_MaterialDeleteTex(material_t* mat);

#endif
