/** @file materials.h Material Collection.
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

enum materialschemeid_e; // Defined in dd_share.h

#ifdef __cplusplus

#include "uri.hh"

namespace de {

    /**
     * Specialized resource collection for a set of materials.
     * @ingroup resource
     */
    class Materials
    {
    public:
        /// To be called during init to register the cvars and ccmds for this module.
        static void consoleRegister();

        /// Initialize this module.
        static void init();

        /// Shutdown this module.
        static void shutdown();

        /// Process all outstanding tasks in the Material cache queue.
        static void processCacheQueue();

        /// Empty the Material cache queue, cancelling all outstanding tasks.
        static void purgeCacheQueue();

        /// To be called during a definition database reset to clear all links to defs.
        static void clearDefinitionLinks();

        /**
         * Process a tic of length @a elapsed, animating materials and anim-groups.
         * @param elapsed  Length of tic to be processed.
         */
        static void ticker(timespan_t elapsed);

        /// @return  Total number of unique Materials in the collection.
        static uint size();

        /// @return  Number of unique Materials in the identified @a schemeId.
        static uint count(enum materialschemeid_e schemeId);

        /// @return  Unique identifier associated with @a material else @c 0.
        static materialid_t id(material_t *material);

        /// @return  Material associated with unique identifier @a materialId else @c NULL.
        static material_t *toMaterial(materialid_t materialId);

        /**
         * Search the Materials collection for a material associated with @a uri.
         * @return  Found material else @c NOMATERIALID.
         */
        static materialid_t resolveUri2(de::Uri const &uri, bool quiet);
        static materialid_t resolveUri(de::Uri const &uri/*, quiet=!(verbose >= 1)*/);

        /**
         * Try to interpret a known material scheme identifier from @a str. If found to match
         * a known scheme name, return the associated identifier. If the reference @a str is
         * not valid (i.e., equal to NULL or is a zero-length string) then the special identifier
         * @c MS_ANY is returned. Otherwise @c MS_INVALID.
         */
        static enum materialschemeid_e parseSchemeName(char const *str);

        /// @return  Name associated with the identified @a schemeId else a zero-length string.
        static de::String const &schemeName(enum materialschemeid_e schemeId);

        /// @return  Unique identifier of the scheme this material is in.
        static enum materialschemeid_e scheme(materialid_t materialId);

        /// @return  Symbolic name/path-to this material. Must be destroyed with Str_Delete().
        static de::String composePath(materialid_t materialId);

        /// @return  Unique name/path-to this material. Must be destroyed with Uri_Delete().
        static de::Uri composeUri(materialid_t materialId);

        static void updateTextureLinks(materialid_t materialId);

        /**
         * Update @a material according to the supplied definition @a def.
         * To be called after an engine update/reset.
         *
         * @param material  Material to be updated.
         * @param def  Material definition to update using.
         */
        static void rebuild(material_t *material, ded_material_t *def);

        /// @return  (Particle) Generator definition associated with @a material else @c NULL.
        static ded_ptcgen_t const *ptcGenDef(material_t *material);

        /// @return  Decoration defintion associated with @a material else @c NULL.
        static ded_decor_t const *decorationDef(material_t *material);

        /// @return  @c true if one or more light decorations are defined for this material.
        static bool hasDecorations(material_t *material);

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
        static material_t *newFromDef(ded_material_t *def);

        /// To be called to reset all animation groups back to their initial state.
        static void resetAnimGroups();

        /// To be called to destroy all animation groups when they are no longer needed.
        static void clearAnimGroups();

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
        static materialvariantspecification_t const *variantSpecificationForContext(
            materialcontext_t materialContext, int flags, byte border, int tClass,
            int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
            bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha);

        /**
         * Add a variant of @a material to the cache queue for deferred preparation.
         *
         * @param material  Base Material from which to derive a variant.
         * @param spec  Specification for the desired derivation of @a material.
         * @param smooth  @c true= Select the current frame if the material is group-animated.
         * @param cacheGroups  @c true= variants for all Materials in any applicable
         *      animation groups are desired, else just this specific Material.
         */
        static void precache(material_t &material, materialvariantspecification_t const &spec,
                             bool smooth, bool cacheGroups = true);

        /**
         * Choose/create a variant of @a material which fulfills @a spec and then
         * immediately prepare it for render (e.g., upload textures if necessary).
         *
         * @note A convenient shorthand of the call tree:
         * <pre>
         *    Materials::prepareVariant( chooseVariant( @a material, @a spec, @a smooth, @c true ), @a forceSnapshotUpdate )
         * </pre>
         *
         * @param material  Base Material from which to derive a variant.
         * @param spec  Specification for the derivation of @a material.
         * @param smooth  @c true= Select the current frame if the material is group-animated.
         * @param forceSnapshotUpdate  @c true= Force an update of the variant's state snapshot.
         *
         * @return  Snapshot for the chosen and prepared variant of Material.
         */
        static de::MaterialSnapshot const *prepare(material_t &material, materialvariantspecification_t const &spec,
                                                   bool smooth, bool forceSnapshotUpdate = false);

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
        static de::MaterialSnapshot const *prepareVariant(de::MaterialVariant &material, bool forceSnapshotUpdate = false);

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
        static de::MaterialVariant *chooseVariant(material_t &material,
            materialvariantspecification_t const &spec, bool smoothed, bool canCreate);

        /// @return  Number of animation/precache groups in the collection.
        static int animGroupCount();

        /**
         * Create a new animation group.
         * @return  Logical (unique) identifier reference associated with the new group.
         */
        static int newAnimGroup(int flags);

        /**
         * Append a new @a material frame to the identified @a animGroupNum.
         *
         * @param animGroupNum  Logical identifier reference to the group being modified.
         * @param material  Material frame to be inserted into the group.
         * @param tics  Base duration of the new frame in tics.
         * @param randomTics  Extra frame duration in tics (randomized on each cycle).
         */
        static void addAnimGroupFrame(int animGroupNum, material_t *material, int tics, int randomTics);

        /// @todo Refactor; does not fit the current design.
        static bool isPrecacheAnimGroup(int animGroupNum);

        /// @return  @c true iff @a material is linked to the identified @a animGroupNum.
        static bool isMaterialInAnimGroup(material_t *material, int animGroupNum);
    };

} // namespace de

extern "C" {
#endif // __cplusplus

/*
 * C wrapper API:
 */

void Materials_Init(void);
void Materials_Shutdown(void);
void Materials_Ticker(timespan_t elapsed);
uint Materials_Size(void);
materialid_t Materials_Id(material_t *material);
material_t *Materials_ToMaterial(materialid_t materialId);
Uri *Materials_ComposeUri(materialid_t materialId);
boolean Materials_HasDecorations(material_t *material);
ded_ptcgen_t const *Materials_PtcGenDef(material_t *material);
boolean Materials_IsMaterialInAnimGroup(material_t *material, int animGroupNum);
materialid_t Materials_ResolveUri2(Uri const *uri, boolean quiet);
materialid_t Materials_ResolveUri(Uri const *uri/*, quiet=!(verbose >= 1)*/);

/// Same as Materials::resolveUri except @a uri is a C-string.
materialid_t Materials_ResolveUriCString2(char const *uri, boolean quiet);
materialid_t Materials_ResolveUriCString(char const *uri/*, quiet=!(verbose >= 1)*/);

int Materials_AnimGroupCount(void);
boolean Materials_IsPrecacheAnimGroup(int animGroupNum);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_MATERIALS_H */
