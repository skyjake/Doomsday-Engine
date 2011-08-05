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

#ifndef LIBDENG_MATERIALS_H
#define LIBDENG_MATERIALS_H

#include "gl_texmanager.h"
#include "def_data.h"
#include "material.h"

struct texturevariantspecification_s;
struct materialvariant_s;
struct material_snapshot_s;

void P_MaterialsRegister(void);

void Materials_Initialize(void);

/**
 * Release all memory acquired for the materials list.
 */
void Materials_Shutdown(void);

/// To be called during a definition database reset to clear all links to defs.
void Materials_ClearDefinitionLinks(void);

/**
 * Update the Material according to the supplied definition.
 * To be called after an engine update/reset.
 *
 * @parma mat  Material to be updated.
 * @param def  Material definition to update using.
 */
void Materials_Rebuild(struct material_s* mat, struct ded_material_s* def);

/**
 * Empty the Material cache queue, cancelling all outstanding tasks.
 */
void Materials_PurgeCacheQueue(void);

/**
 * Process all outstanding tasks in the Material cache queue.
 */
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
void Materials_ReleaseGLTextures(const char* namespaceName);

void Materials_Ticker(timespan_t elapsed);

const ddstring_t* Materials_NamespaceNameForTextureNamespace(texturenamespaceid_t texNamespace);

/**
 * Create a new material. If there exists one by the same name and in the
 * same namespace, it is returned else a new material is created.
 *
 * \note: May fail on invalid definitions.
 *
 * @param def  Material definition to construct from.
 * @return  The created material else @c 0.
 */
struct material_s* Materials_CreateFromDef(ded_material_t* def);

/// @return  Material associated with the specified unique name else @c NULL.
struct material_s* Materials_ToMaterial(materialnum_t num);

/// @return  Unique name associated with the specified Material.
materialnum_t Materials_ToMaterialNum(struct material_s* mat);

/**
 * Prepare a MaterialVariantSpecification according to usage context.
 * If incomplete context information is supplied, suitable defaults are
 * chosen in their place.
 *
 * @param mc  Usage context.
 * @param flags  @see textureVariantSpecificationFlags
 * @param border  Border size in pixels (all edges).
 * @param tClass  Color palette translation class.
 * @param tMap  Color palette translation map.
 *
 * @return  Ptr to a rationalized and valid TextureVariantSpecification
 *      or @c NULL if out of memory.
 */
struct materialvariantspecification_s* Materials_VariantSpecificationForContext(
    materialvariantusagecontext_t mc, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha);

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

/**
 * Given a unique material identifier, lookup the associated name.
 * \note Part of the Doomsday public API.
 *
 * @param mat  The material to lookup the name for.
 *
 * @return  The associated name.
 */
const ddstring_t* Materials_GetSymbolicName(struct material_s* mat);

dduri_t* Materials_GetUri(struct material_s* mat);

uint Materials_Count(void);

const ded_decor_t*  Materials_DecorationDef(materialnum_t num);
const ded_ptcgen_t* Materials_PtcGenDef(materialnum_t num);

void Materials_Prepare(struct material_snapshot_s* snapshot, struct material_s* mat,
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

#endif /* LIBDENG_MATERIALS_H */
