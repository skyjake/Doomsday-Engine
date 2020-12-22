/** @file materials.h  World materials.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_WORLD_MATERIALS_H
#define LIBDOOMSDAY_WORLD_MATERIALS_H

#include "material.h"
#include "materialmanifest.h"
#include "materialscheme.h"
#include "../uri.h"

#include <de/set.h>
#include <de/list.h>

namespace world {

class LIBDOOMSDAY_PUBLIC Materials
{
public:
    /// An unknown material group was referenced. @ingroup errors
    DE_ERROR(UnknownMaterialGroupError);

    /// The specified material id was invalid (out of range). @ingroup errors
    DE_ERROR(UnknownMaterialIdError);

    typedef Set<MaterialManifest *>       MaterialManifestGroup;
    typedef List<MaterialManifestGroup *> MaterialManifestGroups;

    static Materials &get();

public:
    Materials();

    /**
     * Clear all materials (and their manifests) in all schemes.
     *
     * @see forAllMaterialSchemes(), MaterialScheme::clear().
     */
    void clearAllMaterialSchemes();

    /**
     * Destroys all material manifest groups.
     */
    void clearAllMaterialGroups();

    /**
     * Determines if a material exists for a @a path.
     *
     * @return @c true if a material exists; otherwise @a false.
     *
     * @see hasMaterialManifest(), MaterialManifest::hasMaterial()
     */
    inline bool hasMaterial(const res::Uri &path) const {
        if (hasMaterialManifest(path)) return materialManifest(path).hasMaterial();
        return false;
    }

    /**
     * Lookup a material resource for the specified @a path.
     *
     * @return The found material.
     *
     * @see MaterialManifest::material()
     */
    inline Material &material(const res::Uri &path) const {
        return materialManifest(path).material();
    }

    /**
     * Returns a pointer to the identified Material.
     *
     * @see hasMaterialManifest(), MaterialManifest::materialPtr()
     */
    Material *materialPtr(const res::Uri &path);

    /**
     * Determines if a manifest exists for a material on @a path.
     *
     * @return @c true if a manifest exists; otherwise @a false.
     */
    bool hasMaterialManifest(const res::Uri &path) const;

    /**
     * Look up a material manifest by its unique resource @a path.
     *
     * @param path  The path to search for.
     * @return  Found material manifest.
     */
    MaterialManifest &materialManifest(const res::Uri &path) const;

    /**
     * Look up a material manifest by its unique resource @a path.
     *
     * @param path  The path to search for.
     * @return  Found material manifest, or nullptr if not found.
     */
    MaterialManifest *materialManifestPtr(const res::Uri &path) const;

    /**
     * Lookup a manifest by unique identifier.
     *
     * @param id  Unique identifier for the manifest to be looked up. Note
     *            that @c 0 is not a valid identifier.
     *
     * @return  The associated manifest.
     */
    MaterialManifest &toMaterialManifest(materialid_t id) const;

    /**
     * Returns the total number of unique materials in the collection.
     */
    int materialCount() const;

    /**
     * Returns @c true iff a MaterialScheme exists with the symbolic @a name.
     */
    bool isKnownMaterialScheme(const de::String& name) const;

    /**
     * Lookup a material resource scheme by symbolic name.
     *
     * @param name  Symbolic name of the scheme.
     * @return  MaterialScheme associated with @a name.
     *
     * @throws UnknownSchemeError If @a name is unknown.
     *
     * @see knownMaterialScheme()
     */
    MaterialScheme &materialScheme(const de::String& name) const;

    /**
     * Returns the total number of material manifest schemes in the collection.
     */
    int materialSchemeCount() const;

    /**
     * Iterate through all the material resource schemes of the resource system.
     *
     * @param func  Callback to make for each MaterialScheme.
     */
    de::LoopResult forAllMaterialSchemes(const std::function<de::LoopResult (MaterialScheme &)>& func) const;

    /**
     * Lookup a material manifest group by unique @a number.
     */
    MaterialManifestGroup &materialGroup(int number) const;

    /**
     * Create a new (empty) material manifest group.
     */
    MaterialManifestGroup &newMaterialGroup();

    /**
     * Provides a list of all material manifest groups, for efficient traversal.
     */
    const MaterialManifestGroups &allMaterialGroups() const;

    /**
     * Returns the total number of material manifest groups in the collection.
     */
    inline int materialGroupCount() const { return allMaterialGroups().count(); }

    /**
     * Declare a material in the collection, producing a manifest for a logical
     * Material which will be defined later. If a manifest with the specified
     * @a uri already exists the existing manifest will be returned.
     *
     * @param uri  Uri representing a path to the material in the virtual hierarchy.
     *
     * @return  Manifest for this URI.
     */
    inline MaterialManifest &declareMaterial(const res::Uri &uri) {
        return materialScheme(uri.scheme()).declare(uri.path());
    }

    /**
     * Iterate through all the materials of the resource system.
     *
     * @param func  Callback to make for each Material.
     */
    de::LoopResult forAllMaterials(const std::function<de::LoopResult (Material &)> &func) const;

    de::LoopResult forAnimatedMaterials(const std::function<de::LoopResult (Material &)> &func) const;

    void updateLookup();

private:
    DE_PRIVATE(d)
};

} // namespace world

#endif // LIBDOOMSDAY_WORLD_MATERIALS_H
