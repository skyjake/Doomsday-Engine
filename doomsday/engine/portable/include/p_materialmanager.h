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

struct texturevariantspecification_s;
struct materialvariant_s;
struct material_snapshot_s;

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

void Materials_PurgeCacheQueue(void);

void Materials_ProcessCacheQueue(void);

/**
 * Add a MaterialVariantSpecification to the cache queue.
 *
 * @param mat  Base Material from which to derive a variant.
 * @param spec  Specification of the desired variant.
 * @param cacheGroup  @c true = variants for all Materials in any applicable
 *      animation groups are desired, else just this specific Material.
 */
void Materials_Precache2(struct material_s* mat, struct materialvariantspecification_s* spec,
    boolean cacheGroup);
void Materials_Precache(struct material_s* mat, struct materialvariantspecification_s* spec);

/**
 * Deletes all GL texture instances, linked to materials.
 *
 * @param mnamespace @c MN_ANY = delete everything, ELSE
 *      Only delete those currently in use by materials in the specified namespace.
 */
void Materials_DeleteGLTextures(const char* namespaceName);

void Materials_Ticker(timespan_t elapsed);

const ddstring_t* Materials_NamespaceNameForTextureNamespace(texturenamespaceid_t texNamespace);

/**
 * Create a new material. If there exists one by the same name and in the
 * same namespace; it is returned else a new material is created.
 *
 * \important: May fail if @a name is invalid.
 *
 * @param name  Unique name for this material.
 * @param width  Logical width in world units.
 * @param height  Logical height in world units.
 * @param flags  @see materialFlags
 *
 * \deprecated: Old init stuff.
 * @param tex  Texture to use on layer one.
 * @param texOriginX
 * @param texOriginY
 *
 * @return  Newly created MaterialVariant else @c NULL.
 */
struct material_s* Materials_New(const dduri_t* name, int width,
    int height, short flags, textureid_t tex, int texOriginX, int texOriginY);

struct material_s* Materials_NewFromDef(ded_material_t* def);

struct material_s* Materials_ToMaterial(materialnum_t num);

materialnum_t Materials_ToMaterialNum(struct material_s* mat);

/**
 * Prepare a MaterialVariantSpecification according to usage context.
 * If incomplete context information is supplied, suitable defaults are
 * chosen in their place.
 *
 * @param tc  Usage context.
 * @param flags  @see textureVariantSpecificationFlags
 * @param border  Border size in pixels (all edges).
 * @param tClass  Color palette translation class.
 * @param tMap  Color palette translation map.
 *
 * @return  Ptr to a rationalized and valid TextureVariantSpecification
 *      or @c NULL if out of memory.
 */
struct materialvariantspecification_s* Materials_VariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass, int tMap);

struct materialvariant_s* Materials_ChooseVariant(struct material_s* mat,
    const struct materialvariantspecification_s* spec);

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

const char* Materials_GetSymbolicName(struct material_s* mat);
dduri_t* Materials_GetUri(struct material_s* mat);

uint Materials_Count(void);

const ded_detailtexture_t* Materials_Detail(materialnum_t num);
const ded_reflection_t* Materials_Reflection(materialnum_t num);
const ded_decor_t*  Materials_Decoration(materialnum_t num);
const ded_ptcgen_t* Materials_PtcGen(materialnum_t num);

byte Materials_Prepare(struct material_snapshot_s* snapshot, struct material_s* mat,
    boolean smoothed, struct materialvariantspecification_s* spec);

int Materials_AnimGroupCount(void);
void Materials_ResetAnimGroups(void);
void Materials_DestroyAnimGroups(void);

int Materials_CreateAnimGroup(int flags);
void Materials_AddAnimGroupFrame(int animGroupNum, materialnum_t num, int tics, int randomTics);
boolean Materials_MaterialLinkedToAnimGroup(int animGroupNum, struct material_s* mat);

void Materials_ClearTranslation(struct material_s* mat);

// @todo Refactor interface, doesn't fit the current design.
boolean Materials_IsPrecacheAnimGroup(int groupNum);

#endif /* LIBDENG2_MATERIALS_H */
