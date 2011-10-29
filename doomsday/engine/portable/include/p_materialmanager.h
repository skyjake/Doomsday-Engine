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
struct materialbind_s;

// Substrings in Material names are delimited by this character.
#define MATERIALDIRECTORY_DELIMITER '/'

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
 * @parma material  Material to be updated.
 * @param def  Material definition to update using.
 */
void Materials_Rebuild(struct material_s* material, struct ded_material_s* def);

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
 * @param material  Base Material from which to derive a variant.
 * @param spec  Specification of the desired variant.
 * @param cacheGroup  @c true = variants for all Materials in any applicable
 *      animation groups are desired, else just this specific Material.
 */
void Materials_Precache2(struct material_s* material, struct materialvariantspecification_s* spec,
    boolean cacheGroup);
void Materials_Precache(struct material_s* material, struct materialvariantspecification_s* spec);

/**
 * Deletes all GL texture instances, linked to materials.
 *
 * @param mnamespace @c MN_ANY = delete everything, ELSE
 *      Only delete those currently in use by materials in the specified namespace.
 */
void Materials_ReleaseGLTextures(materialnamespaceid_t namespaceId);

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

/// @return  Unique identifier associated with @a material.
materialnum_t Materials_ToMaterialNum(struct material_s* material);

/// @return  Primary MaterialBind associated with @a material else @c NULL.
struct materialbind_s* Materials_PrimaryBind(struct material_s* material);

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

struct materialvariant_s* Materials_ChooseVariant(struct material_s* material,
    const struct materialvariantspecification_s* spec, boolean smoothed, boolean canCreate);

/**
 * Search the Materials collection for a material associated with @a uri.
 * @return  Found material else @c 0
 */
struct material_s* Materials_MaterialForUri2(const Uri* uri, boolean quiet);
struct material_s* Materials_MaterialForUri(const Uri* uri);
struct material_s* Materials_MaterialForUriCString2(const char* uri, boolean quiet);
struct material_s* Materials_MaterialForUriCString(const char* uri);

/// @return  Full symbolic name/path-to the Material. Must be destroyed with Uri_Delete().
Uri* Materials_ComposeUri(struct material_s* material);

uint Materials_Count(void);

const ded_decor_t*  Materials_DecorationDef(struct material_s* material);
const ded_ptcgen_t* Materials_PtcGenDef(struct material_s* material);

/**
 * Choose/create a variant of @a material which fulfills @a spec and then
 * prepare it for render (e.g., upload textures if necessary).
 *
 * @param material  Material to derive the variant from.
 * @param spec  Specification for the derivation of @a material.
 * @param smooth  @c true= Select the current frame if the material is group-animated.
 * @param updateSnapshot  @c true= Force an update the variant's state snapshot.
 *
 * @return  Snapshot for the chosen and prepared MaterialVariant.
 */
const material_snapshot_t* Materials_ChooseAndPrepare(material_t* material,
    const materialvariantspecification_t* spec, boolean smooth, boolean updateSnapshot);

/// Same as Materials::ChooseAndPrepare except the caller specifies the variant.
const material_snapshot_t* Materials_Prepare(materialvariant_t* material, boolean updateSnapshot);

int Materials_AnimGroupCount(void);
void Materials_ResetAnimGroups(void);
void Materials_DestroyAnimGroups(void);

/**
 * Create a new animation group.
 * @return  Logical identifier of the new group.
 */
int Materials_CreateAnimGroup(int flags);

void Materials_AddAnimGroupFrame(int animGroupNum, struct material_s* material, int tics, int randomTics);
boolean Materials_MaterialLinkedToAnimGroup(int animGroupNum, struct material_s* material);

void Materials_ClearTranslation(struct material_s* material);

// @todo Refactor interface, doesn't fit the current design.
boolean Materials_IsPrecacheAnimGroup(int groupNum);

#endif /* LIBDENG_MATERIALS_H */
