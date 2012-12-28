/** @file materials.h Material Collection.
 *
 * Schemes, bindings and other management.
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_RESOURCE_MATERIALS_H
#define LIBDENG_RESOURCE_MATERIALS_H

#include "def_data.h"
#include "material.h"
#ifdef __cplusplus
#  include "resource/materialvariant.h"
#endif

struct texturevariantspecification_s;
struct materialsnapshot_s;

enum materialschemeid_e; // Defined in dd_share.h

/// Material (Usage) Context identifiers.
typedef enum {
    MC_UNKNOWN = -1,
    MATERIALCONTEXT_FIRST = 0,
    MC_UI = MATERIALCONTEXT_FIRST,
    MC_MAPSURFACE,
    MC_SPRITE,
    MC_MODELSKIN,
    MC_PSPRITE,
    MC_SKYSPHERE,
    MATERIALCONTEXT_LAST = MC_SKYSPHERE
} materialcontext_t;

#define MATERIALCONTEXT_COUNT (MATERIALCONTEXT_LAST + 1 - MATERIALCONTEXT_FIRST )

/// @c true= val can be interpreted as a valid material context identifier.
#define VALID_MATERIALCONTEXT(val) ((val) >= MATERIALCONTEXT_FIRST && (val) <= MATERIALCONTEXT_LAST)

struct texturevariantspecification_s;

typedef struct materialvariantspecification_s {
    materialcontext_t context;
    struct texturevariantspecification_s *primarySpec;
} materialvariantspecification_t;

#ifdef __cplusplus
extern "C" {
#endif

/// To be called during init to register the cvars and ccmds for this module.
void Materials_Register(void);

/// Initialize this module.
void Materials_Init(void);

/// Shutdown this module.
void Materials_Shutdown(void);

/**
 * Process a tic of length @a elapsed, animating materials and anim-groups.
 * @param elapsed  Length of tic to be processed.
 */
void Materials_Ticker(timespan_t elapsed);

/// Process all outstanding tasks in the Material cache queue.
void Materials_ProcessCacheQueue(void);

/// Empty the Material cache queue, cancelling all outstanding tasks.
void Materials_PurgeCacheQueue(void);

/// To be called during a definition database reset to clear all links to defs.
void Materials_ClearDefinitionLinks(void);

/**
 * Try to interpret a known material scheme identifier from @a str. If found to match
 * a known scheme name, return the associated identifier. If the reference @a str is
 * not valid (i.e., equal to NULL or is a zero-length string) then the special identifier
 * @c MS_ANY is returned. Otherwise @c MS_INVALID.
 */
enum materialschemeid_e Materials_ParseSchemeName(char const *str);

/// @return  Name associated with the identified @a schemeId else a zero-length string.
Str const *Materials_SchemeName(enum materialschemeid_e schemeId);

/// @return  Total number of unique Materials in the collection.
uint Materials_Size(void);

/// @return  Number of unique Materials in the identified @a schemeId.
uint Materials_Count(enum materialschemeid_e schemeId);

/// @return  Unique identifier associated with @a material else @c 0.
materialid_t Materials_Id(material_t *material);

/// @return  Material associated with unique identifier @a materialId else @c NULL.
material_t *Materials_ToMaterial(materialid_t materialId);

/// @return  Unique identifier of the scheme this material is in.
enum materialschemeid_e Materials_Scheme(materialid_t materialId);

/// @return  Symbolic name/path-to this material. Must be destroyed with Str_Delete().
AutoStr *Materials_ComposePath(materialid_t materialId);

/// @return  Unique name/path-to this material. Must be destroyed with Uri_Delete().
Uri *Materials_ComposeUri(materialid_t materialId);

void Materials_UpdateTextureLinks(materialid_t materialId);

/**
 * Update @a material according to the supplied definition @a def.
 * To be called after an engine update/reset.
 *
 * @param material  Material to be updated.
 * @param def  Material definition to update using.
 */
void Materials_Rebuild(material_t *material, ded_material_t *def);

/// @return  @c true if one or more light decorations are defined for this material.
boolean Materials_HasDecorations(material_t *material);

/// @return  Decoration defintion associated with @a material else @c NULL.
ded_decor_t const *Materials_DecorationDef(material_t *material);

/// @return  (Particle) Generator definition associated with @a material else @c NULL.
ded_ptcgen_t const *Materials_PtcGenDef(material_t *material);

/// @return  @c true iff @a material is linked to the identified @a animGroupNum.
boolean Materials_IsMaterialInAnimGroup(material_t *material, int animGroupNum);

/**
 * Search the Materials collection for a material associated with @a uri.
 * @return  Found material else @c NOMATERIALID.
 */
materialid_t Materials_ResolveUri2(Uri const *uri, boolean quiet);
materialid_t Materials_ResolveUri(Uri const *uri/*, quiet=!(verbose >= 1)*/);

/// Same as Materials::ResolveUri except @a uri is a C-string.
materialid_t Materials_ResolveUriCString2(char const *uri, boolean quiet);
materialid_t Materials_ResolveUriCString(char const *uri/*, quiet=!(verbose >= 1)*/);

/**
 * Create a new Material unless an existing Material is found at the path
 * (and within the same scheme) as that specified in @a def, in which case
 * it is returned instead.
 *
 * @note: May fail on invalid definitions (return= @c NULL).
 *
 * @param def  Material definition to construct from.
 * @return  The newly-created/existing Material else @c NULL.
 */
material_t *Materials_CreateFromDef(ded_material_t *def);

/// @return  Number of animation/precache groups in the collection.
int Materials_AnimGroupCount(void);

/// To be called to reset all animation groups back to their initial state.
void Materials_ResetAnimGroups(void);

/// To be called to destroy all animation groups when they are no longer needed.
void Materials_ClearAnimGroups(void);

/**
 * Prepare a MaterialVariantSpecification according to a usage context. If
 * incomplete context information is supplied, suitable default values will
 * be chosen in their place.
 *
 * @param materialContext Material (usage) context identifier.
 * @param flags  @ref textureVariantSpecificationFlags
 * @param border  Border size in pixels (all edges).
 * @param tClass  Color palette translation class.
 * @param tMap  Color palette translation map.
 * @param wrapS  GL texture wrap/clamp mode on the horizontal axis (texture-space).
 * @param wrapT  GL texture wrap/clamp mode on the vertical axis (texture-space).
 * @param minFilter  Logical DGL texture minification level.
 * @param magFilter  Logical DGL texture magnification level.
 * @param anisoFilter  @c -1= User preference else a logical DGL anisotropic filter level.
 * @param mipmapped  @c true= use mipmapping.
 * @param gammaCorrection  @c true= apply gamma correction to textures.
 * @param noStretch  @c true= disallow stretching of textures.
 * @param toAlpha  @c true= convert textures to alpha data.
 *
 * @return  Rationalized (and interned) copy of the final specification.
 */
struct materialvariantspecification_s const *Materials_VariantSpecificationForContext(
    materialcontext_t materialContext, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha);

#ifdef __cplusplus

/**
 * Add a variant of @a material to the cache queue for deferred preparation.
 *
 * @param material  Base Material from which to derive a variant.
 * @param spec  Specification for the desired derivation of @a material.
 * @param smooth  @c true= Select the current frame if the material is group-animated.
 * @param cacheGroups  @c true= variants for all Materials in any applicable
 *      animation groups are desired, else just this specific Material.
 */
void Materials_Precache2(material_t &material, struct materialvariantspecification_s const &spec, bool smooth, bool cacheGroups);
void Materials_Precache(material_t &material, struct materialvariantspecification_s const &spec, bool smooth/*, cacheGroups = true*/);

/**
 * Choose/create a variant of @a material which fulfills @a spec and then
 * immediately prepare it for render (e.g., upload textures if necessary).
 *
 * @note A convenient shorthand of the call tree:
 * <pre>
 *    Materials_PrepareVariant( Materials_ChooseVariant( @a material, @a spec, @a smooth, @c true ), @a forceSnapshotUpdate )
 * </pre>
 *
 * @param material  Base Material from which to derive a variant.
 * @param spec  Specification for the derivation of @a material.
 * @param smooth  @c true= Select the current frame if the material is group-animated.
 * @param forceSnapshotUpdate  @c true= Force an update of the variant's state snapshot.
 *
 * @return  Snapshot for the chosen and prepared variant of Material.
 */
struct materialsnapshot_s const *Materials_Prepare2(material_t &material, struct materialvariantspecification_s const &spec, bool smooth, bool forceSnapshotUpdate);
struct materialsnapshot_s const *Materials_Prepare(material_t &material, struct materialvariantspecification_s const &spec, bool smooth/*, forceSnapshotUpdate = false*/);

/**
 * Prepare variant @a material for render (e.g., upload textures if necessary).
 *
 * @note Same as Materials::Prepare except the caller specifies the variant.
 * @see Materials::ChooseVariant
 *
 * @param material  MaterialVariant to be prepared.
 * @param forceSnapshotUpdate  @c true= Force an update of the variant's state snapshot.
 *
 * @return  Snapshot for the chosen and prepared variant of Material.
 */
struct materialsnapshot_s const *Materials_PrepareVariant2(de::MaterialVariant &material, bool forceSnapshotUpdate);
struct materialsnapshot_s const *Materials_PrepareVariant(de::MaterialVariant &material/*, forceSnapshotUpdate = false*/);

/**
 * Choose/create a variant of @a material which fulfills @a spec.
 *
 * @param material  Material to derive the variant from.
 * @param spec  Specification for the derivation of @a material.
 * @param smooth  @c true= Select the current frame if the material is group-animated.
 * @param canCreate  @c true= Create a new variant if a suitable one does exist.
 *
 * @return  Chosen variant else @c NULL if none suitable and not creating.
 */
de::MaterialVariant *Materials_ChooseVariant(material_t &material,
    struct materialvariantspecification_s const &spec, bool smoothed, bool canCreate);

/**
 * Create a new animation group.
 * @return  Logical (unique) identifier reference associated with the new group.
 */
int Materials_CreateAnimGroup(int flags);

/**
 * Append a new @a material frame to the identified @a animGroupNum.
 *
 * @param animGroupNum  Logical identifier reference to the group being modified.
 * @param material  Material frame to be inserted into the group.
 * @param tics  Base duration of the new frame in tics.
 * @param randomTics  Extra frame duration in tics (randomized on each cycle).
 */
void Materials_AddAnimGroupFrame(int animGroupNum, material_t *material, int tics, int randomTics);

#endif // __cplusplus

/// @todo Refactor; does not fit the current design.
boolean Materials_IsPrecacheAnimGroup(int animGroupNum);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_MATERIALS_H */
