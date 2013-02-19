/** @file materials.h Specialized resource collection for a set of materials.
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "Material"
#include "MaterialManifest"
#include "MaterialScheme"
#ifdef __CLIENT__
#  include "MaterialVariantSpec"
#endif
#include "def_data.h"
#include "uri.hh"
#include <de/Error>
#include <de/Path>
#include <de/String>
#include <QList>
#include <QMap>

namespace de {

/**
 * Specialized resource collection for a set of materials.
 *
 * - Pointers to Material are @em eternal, they are always valid and continue
 *   to reference the same logical material data even after engine reset.
 *
 * - Public material identifiers (materialid_t) are similarly eternal.
 *
 * - Material name bindings are semi-independant from the materials. There
 *   may be multiple name bindings for a given material (aliases). The only
 *   requirement is that their symbolic names must be unique among those in
 *   the same scheme.
 *
 * @ingroup resource
 */
class Materials
{
public:
    typedef class MaterialScheme Scheme;
    typedef class MaterialManifest Manifest;
    typedef struct MaterialVariantSpec VariantSpec;

    /**
     * Defines a group of one or more material manifests.
     */
    class ManifestGroup
    {
    public:
        /// Manifests.
        typedef QList<Manifest *> All;

    public:
        /// An invalid group member reference was specified. @ingroup errors
        DENG2_ERROR(InvalidManifestError);

    public:
        ManifestGroup(int id);
        ~ManifestGroup();

        /**
         * Returns the group's unique identifier.
         */
        int id() const;

        /**
         * Returns the total number of material manifests in the group.
         */
        int count() const { return all().count(); }

        /**
         * Lookup a manifest in the group by number.
         *
         * @param number  Material manifest number to lookup.
         * @return  Found manifest.
         */
        Manifest &toManifest(int number) const;

        /**
         * Extend the group by adding a new manifest to the end of the group.
         *
         * @param manifest  Manifest to add.
         */
        void add(Manifest &manifest);

        /**
         * Returns @c true iff @a manifest is part of this group.
         *
         * @param mat  Manifest to search for.
         */
        bool has(Manifest &manifest) const;

        /**
         * Provides access to the manifest list for efficient traversal.
         */
        All const &all() const;

    private:
        struct Instance;
        Instance *d;
    };

    /**
     * Flags determining URI validation logic.
     *
     * @see validateUri()
     */
    enum UriValidationFlag
    {
        /// The scheme of the URI may be of zero-length; signifying "any scheme".
        AnyScheme = 0x1
    };
    Q_DECLARE_FLAGS(UriValidationFlags, UriValidationFlag)

    /// System subspace schemes.
    typedef QMap<String, Scheme *> Schemes;

    /// Manifest groups.
    typedef QList<ManifestGroup *> ManifestGroups;

    /// Material instances.
    typedef QList<Material *> All;

public:
    /// The referenced material/manifest was not found. @ingroup errors
    DENG2_ERROR(NotFoundError);

    /// The specified material id was invalid (out of range). @ingroup errors
    DENG2_ERROR(InvalidMaterialIdError);

    /// An unknown group was referenced. @ingroup errors
    DENG2_ERROR(UnknownGroupError);

    /// An unknown scheme was referenced. @ingroup errors
    DENG2_ERROR(UnknownSchemeError);

public:
    /**
     * Constructs a new material collection.
     */
    Materials();

    virtual ~Materials();

    /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

    /**
     * Returns the total number of unique materials in the collection.
     */
    uint count() const { return all().count(); }

    /**
     * Returns the total number of unique materials in the collection.
     *
     * Same as size()
     */
    inline uint size() const { return count(); }

    /**
     * Validate @a uri to determine if it is well-formed and is usable as a
     * search argument.
     *
     * @param uri       Uri to be validated.
     * @param flags     Validation flags.
     * @param quiet     @c true= Do not output validation remarks to the log.
     *
     * @return  @c true if @a Uri passes validation.
     *
     * @todo Should throw de::Error exceptions -ds
     */
    bool validateUri(Uri const &uri, UriValidationFlags flags = 0,
                     bool quiet = false) const;

    /**
     * Determines if a manifest exists for a material on @a path.
     * @return @c true if a manifest exists; otherwise @a false.
     */
    bool has(Uri const &path) const;

    /**
     * Find the material manifest on @a path.
     *
     * @param path  The path to search for.
     * @return  Found material manifest.
     */
    Manifest &find(Uri const &path) const;

    /**
     * Lookup a manifest by unique identifier.
     *
     * @param id  Unique identifier for the manifest to be looked up. Note
     *            that @c 0 is not a valid identifier.
     *
     * @return  The associated manifest.
     */
    Manifest &toManifest(materialid_t id) const;

    /**
     * Lookup a subspace scheme by symbolic name.
     *
     * @param name  Symbolic name of the scheme.
     * @return  Scheme associated with @a name.
     *
     * @throws UnknownSchemeError If @a name is unknown.
     */
    Scheme &scheme(String name) const;

    /**
     * Create a new subspace scheme.
     *
     * @note Scheme creation order defines the order in which schemes are tried
     *       by @ref find() when presented with an ambiguous URI (i.e., those
     *       without a scheme).
     *
     * @param name  Unique symbolic name of the new scheme. Must be at least
     *              @c Scheme::min_name_length characters long.
     */
    Scheme &createScheme(String name);

    /**
     * Returns @c true iff a Scheme exists with the symbolic @a name.
     */
    bool knownScheme(String name) const;

    /**
     * Returns a list of all the schemes for efficient traversal.
     */
    Schemes const &allSchemes() const;

    /**
     * Returns the total number of manifest schemes in the collection.
     */
    inline int schemeCount() const { return allSchemes().count(); }

    /**
     * Clear all materials in all schemes.
     * @see allSchemes(), Scheme::clear().
     */
    inline void clearAllSchemes()
    {
        foreach(Scheme *scheme, allSchemes())
        {
            scheme->clear();
        }
    }

    /**
     * Lookup a manifest group by unique @a number.
     */
    ManifestGroup &group(int number) const;

    /**
     * Create a new (empty) manifest group.
     */
    ManifestGroup &createGroup();

    /**
     * To be called to destroy all manifest groups when they are no longer needed.
     */
    void destroyAllGroups();

    /**
     * Provides access to the list of manifest groups for efficient traversal.
     */
    ManifestGroups const &allGroups() const;

    /**
     * Returns the total number of manifest groups in the collection.
     */
    inline int groupCount() const { return allGroups().count(); }

    ///
    Manifest &newManifest(Scheme &scheme, Path const &path);

    /**
     * Create a new Material unless an existing Material is found at the path
     * (and within the same scheme) as that specified in @a def, in which case
     * it is returned instead.
     *
     * @note: May fail on invalid definitions (return= @c NULL).
     *
     * @param def  Material definition to construct from.
     * @return  The newly-created/existing material; otherwise @c NULL.
     */
    Material *newFromDef(ded_material_t &def);

    /**
     * Returns a list of all the unique material instances in the collection,
     * from all schemes.
     */
    All const &all() const;

#ifdef __CLIENT__

    /**
     * Reset all material instance animations back to their initial state.
     *
     * @see all(), MaterialVariant::resetAnim()
     */
    inline void resetAllAnims() const
    {
        foreach(Material *material, all())
        foreach(MaterialVariant *variant, material->variants())
        {
            variant->resetAnim();
        }
    }

    /**
     * Prepare a material variant specification in accordance to the specified
     * usage context. If incomplete context information is supplied, suitable
     * default values will be chosen in their place.
     *
     * @param materialContext   Material (usage) context identifier.
     * @param flags             @ref textureVariantSpecificationFlags
     * @param border            Border size in pixels (all edges).
     * @param tClass            Color palette translation class.
     * @param tMap              Color palette translation map.
     * @param wrapS             GL texture wrap/clamp mode on the horizontal axis (texture-space).
     * @param wrapT             GL texture wrap/clamp mode on the vertical axis (texture-space).
     * @param minFilter         Logical DGL texture minification level.
     * @param magFilter         Logical DGL texture magnification level.
     * @param anisoFilter       @c -1= User preference else a logical DGL anisotropic filter level.
     * @param mipmapped         @c true= use mipmapping.
     * @param gammaCorrection   @c true= apply gamma correction to textures.
     * @param noStretch         @c true= disallow stretching of textures.
     * @param toAlpha           @c true= convert textures to alpha data.
     *
     * @return  Rationalized (and interned) copy of the final specification.
     */
    VariantSpec const &variantSpecForContext(materialcontext_t materialContext,
        int flags, byte border, int tClass, int tMap, int wrapS, int wrapT,
        int minFilter, int magFilter, int anisoFilter,
        bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha);

    /**
     * Add a variant of @a material to the cache queue for deferred preparation.
     *
     * @param material      Base material from which to derive a context variant.
     * @param spec          Specification for the derivation of @a material.
     * @param cacheGroups   @c true= variants for all materials in any applicable
     *                      groups are desired; otherwise just specified material.
     */
    void cache(Material &material, VariantSpec const &spec, bool cacheGroups = true);

    /// Process all outstanding tasks in the cache queue.
    void processCacheQueue();

    /// Empty the Material cache queue, cancelling all outstanding tasks.
    void purgeCacheQueue();

#endif // __CLIENT__

private:
    struct Instance;
    Instance *d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Materials::UriValidationFlags)

} // namespace de

#endif /* LIBDENG_RESOURCE_MATERIALS_H */
