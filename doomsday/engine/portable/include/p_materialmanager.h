/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_materialmanager.h: Materials manager.
 */

#ifndef __DOOMSDAY_MATERIAL_MANAGER_H__
#define __DOOMSDAY_MATERIAL_MANAGER_H__

#include "gl_texmanager.h"
#include "def_data.h"

extern materialnum_t numMaterialBinds;

void            P_MaterialManagerRegister(void);

void            P_MaterialManagerTicker(timespan_t time);

void            P_InitMaterialManager(void);
void            P_ShutdownMaterialManager(void);
void            P_DeleteMaterialTextures(material_namespace_t mnamespace);
void            P_LinkAssociatedDefinitionsToMaterials(void);

material_t*     P_MaterialCreate(const char* name, short width,
                                 short height, byte flags, gltextureid_t tex,
                                 material_namespace_t mnamespace, ded_material_t* def);

material_t*     P_ToMaterial(materialnum_t num);
materialnum_t   P_ToMaterialNum(const material_t* mat);

// Lookup:
const char*     P_GetMaterialName(material_t* mat);

materialnum_t   P_MaterialCheckNumForName(const char* name, material_namespace_t mnamespace);
materialnum_t   P_MaterialNumForName(const char* name, material_namespace_t mnamespace);

material_t*     P_GetMaterial(int ofTypeID, material_namespace_t mnamespace);
materialnum_t   P_MaterialCheckNumForIndex(uint idx, material_namespace_t mnamespace);
materialnum_t   P_MaterialNumForIndex(uint idx, material_namespace_t mnamespace);

void            P_MaterialPrecache(material_t* mat, boolean yes);

byte                Materials_Prepare(struct material_snapshot_s* snapshot, material_t* mat, boolean smoothed, struct material_load_params_s* params);
const ded_detailtexture_t* Materials_Detail(materialnum_t num);
const ded_reflection_t* Materials_Reflection(materialnum_t num);
const ded_decor_t*  Materials_Decoration(materialnum_t num);
const ded_ptcgen_t* Materials_PtcGen(materialnum_t num);

// Anim groups:
int             R_NumAnimGroups(void);
int             R_CreateAnimGroup(int flags);
void            R_AddToAnimGroup(int animGroupNum, materialnum_t num, int tics,
                                 int randomTics);
boolean         R_IsInAnimGroup(int animGroupNum, material_t* mat);
boolean         R_IsPrecacheGroup(int groupNum);
void            R_DestroyAnimGroups(void);
void            R_ResetAnimGroups(void);
void            R_MaterialsPrecacheGroup(material_t* mat);
#endif
