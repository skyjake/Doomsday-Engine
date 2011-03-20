/**\file p_materialmanager.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_MATERIALS_H
#define LIBDENG2_MATERIALS_H

#include "gl_texmanager.h"
#include "def_data.h"
#include "p_material.h"
#include "texturevariantspecification.h"

void P_MaterialsRegister(void);

/**
 * One time initialization of the materials list.
 */
void Materials_Initialize(void);

/**
 * Release all memory acquired for the materials list.
 */
void Materials_Shutdown(void);

/**
 * Find name-associated definitions for the known material bindings.
 */
void Materials_LinkAssociatedDefinitions(void);

/**
 * Deletes all GL texture instances, linked to materials.
 *
 * @param mnamespace @c MN_ANY = delete everything, ELSE
 *      Only delete those currently in use by materials in the specified namespace.
 */
void Materials_DeleteTextures(const char* namespaceName);

void Materials_Ticker(timespan_t elapsed);

const ddstring_t* Materials_NamespaceNameForTextureNamespace(texturenamespaceid_t texNamespace);

material_t* Materials_New(const dduri_t* name, short width, short height, byte flags, textureid_t tex, short texOriginX, short texOriginY);
material_t* Materials_NewFromDef(ded_material_t* def);

material_t* Materials_ToMaterial(materialnum_t num);

materialnum_t Materials_ToMaterialNum(const material_t* mat);

/**
 * Search the Materials db for a match.
 * \note Part of the Doomsday public API.
 *
 * @param path  Path of the material to search for.
 *
 * @return  Unique identifier of the found material, else zero.
 */
materialnum_t Materials_IndexForUri(const dduri_t* uri);
materialnum_t Materials_IndexForName(const char* path);

const char* Materials_GetSymbolicName(material_t* mat);
dduri_t* Materials_GetUri(material_t* mat);

void Materials_Precache(material_t* mat, boolean yes);

uint Materials_Count(void);

const ded_detailtexture_t* Materials_Detail(materialnum_t num);
const ded_reflection_t* Materials_Reflection(materialnum_t num);
const ded_decor_t*  Materials_Decoration(materialnum_t num);
const ded_ptcgen_t* Materials_PtcGen(materialnum_t num);

byte Materials_Prepare(struct material_snapshot_s* snapshot, material_t* mat,
    boolean smoothed, texturevariantspecification_t* texSpec);

int Materials_AnimGroupCount(void);
void Materials_ResetAnimGroups(void);
void Materials_DestroyAnimGroups(void);

int Materials_CreateAnimGroup(int flags);
void Materials_AddAnimGroupFrame(int animGroupNum, materialnum_t num, int tics, int randomTics);
boolean Materials_MaterialLinkedToAnimGroup(int animGroupNum, material_t* mat);

// @todo Refactor interface, doesn't fit the current design.
boolean Materials_IsPrecacheAnimGroup(int groupNum);

// @todo Refactor away.
void Materials_PrecacheAnimGroup(material_t* mat, boolean yes);
#endif /* LIBDENG2_MATERIALS_H */

