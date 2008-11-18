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

#include "r_data.h"
#include "s_environ.h"

#define TEXF_LOAD_AS_SKY      0x1
#define TEXF_TEX_ZEROMASK     0x2 // Zero the alpha of loaded textures.

typedef enum {
    MTT_FLAT,
    MTT_TEXTURE,
    MTT_SPRITE,
    MTT_DDTEX,
    NUM_MATERIALTEX_TYPES
} materialtextype_t;

typedef struct materialtex_s {
    materialtextype_t type;
    int             ofTypeID;
    boolean         isFromIWAD;
    void*           instances;
} materialtex_t;

// Material flags:
#define MATF_NO_DRAW            0x1 // Material should never be drawn.
#define MATF_GLOW               0x2 // Glowing material.

typedef struct material_s {
// Material def:
    byte            flags; // MATF_* flags
    short           width, height; // Defined width & height of the material (not texture!).
    materialtex_t*  tex;
    materialgroup_t group;
    struct shinydata_s {
        DGLuint         tex;
        blendmode_t     blendMode;
        float           shininess;
        float           minColor[3];
        DGLuint         maskTex;
        float           maskWidth, maskHeight;
    } shiny;

// Misc:
    materialclass_t envClass; // Used for environmental sound properties.
    detailtex_t*    detail;

// Associated enhancements/attachments to be used with surfaces:
    ded_decor_t* decoration;
    ded_ptcgen_t* ptcGen;

// For global animation:
    boolean         inAnimGroup; // True if belongs to some animgroup.
    struct material_s* current;
    struct material_s* next;
    float           inter;

    struct material_s* globalNext; // Linear list linking all materials.
} material_t;

void            R_InitMaterials(void);
void            R_ShutdownMaterials(void);
materialnum_t   R_GetNumMaterials(void);
void            R_DeleteMaterialTextures(materialgroup_t group);
void            R_SetAllMaterialsMinMode(int minMode);

// Creation:
material_t*     R_MaterialCreate(const char* name, short width,
                                 short height, byte flags,
                                 materialtex_t* mTex, materialgroup_t group);
materialtexinst_t* R_MaterialPrepare(struct material_s* mat, int flags,
                                     gltexture_t* glTex,
                                     gltexture_t* glDetailTex, byte* result);
// Set/Get:
void            R_MaterialSetMinMode(material_t* mat, int minMode);
void            R_MaterialSetTranslation(material_t* mat,
                                         material_t* current,
                                         material_t* next, float inter);

boolean         R_MaterialGetInfo(materialnum_t num, materialinfo_t* info);
const ded_decor_t* R_MaterialGetDecoration(material_t* mat);
const ded_ptcgen_t* R_MaterialGetPtcGen(material_t* mat);

// Lookup:
material_t*     R_GetMaterial(int ofTypeID, materialgroup_t group);
material_t*     R_GetMaterialByNum(materialnum_t num);
materialnum_t   R_GetMaterialNum(const material_t* mat);

// Misc:
boolean         R_MaterialIsCustom(materialnum_t num);
void            R_MaterialPrecache2(material_t* mat);

materialtex_t*  R_MaterialTexCreate(int ofTypeID, materialtextype_t type);
void            R_MaterialTexDelete(materialtex_t* mTex);

int             R_CreateAnimGroup(int flags);
void            R_AddToAnimGroup(int animGroupNum, materialnum_t num, int tics,
                                 int randomTics);
boolean         R_IsInAnimGroup(int animGroupNum, materialnum_t num);

void            R_AnimateAnimGroups(void);
void            R_DestroyAnimGroups(void);
void            R_ResetAnimGroups(void);

// Public Interface:
materialnum_t   R_MaterialCheckNumForName(const char* name, materialgroup_t group);
materialnum_t   R_MaterialNumForName(const char* name, materialgroup_t group);
materialnum_t   R_MaterialCheckNumForIndex(uint idx, materialgroup_t group);
materialnum_t   R_MaterialNumForIndex(uint idx, materialgroup_t group);
const char*     R_MaterialNameForNum(materialnum_t num);
void            R_MaterialPrecache(materialnum_t num);
#endif
