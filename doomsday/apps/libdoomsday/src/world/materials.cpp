/** @file materials.cpp
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/world/materials.h"
#include "doomsday/world/MaterialScheme"
#include "doomsday/world/MaterialManifest"
#include "doomsday/world/world.h"
#include "doomsday/resource/resources.h"

#include <de/memory.h>

using namespace de;

namespace world {

DENG2_PIMPL(Materials)
, DENG2_OBSERVES(MaterialScheme,   ManifestDefined)
, DENG2_OBSERVES(MaterialManifest, MaterialDerived)
, DENG2_OBSERVES(MaterialManifest, Deletion)
, DENG2_OBSERVES(Material,         Deletion)
{
    /// System subspace schemes containing the manifests/resources.
    QMap<String, MaterialScheme *> materialSchemes;
    QList<MaterialScheme *> materialSchemeCreationOrder;

    QList<Material *> materials;       ///< From all schemes.
    int materialManifestCount = 0;     ///< Total number of material manifests (in all schemes).

    MaterialManifestGroups materialGroups;

    uint materialManifestIdMapSize = 0;
    MaterialManifest **materialManifestIdMap = nullptr;  ///< Index with materialid_t-1

    Impl(Public *i) : Base(i)
    {
        /// @note Order here defines the ambigious-URI search order.
        createMaterialScheme("Sprites");
        createMaterialScheme("Textures");
        createMaterialScheme("Flats");
        createMaterialScheme("System");
    }

    ~Impl()
    {
        self.clearAllMaterialGroups();
        self.clearAllMaterialSchemes();
        clearMaterialManifests();
    }

    void clearMaterialManifests()
    {
        qDeleteAll(materialSchemes);
        materialSchemes.clear();
        materialSchemeCreationOrder.clear();

        // Clear the manifest index/map.
        if (materialManifestIdMap)
        {
            M_Free(materialManifestIdMap);
            materialManifestIdMap = 0;
            materialManifestIdMapSize = 0;
        }
        materialManifestCount = 0;
    }

    void createMaterialScheme(String name)
    {
        DENG2_ASSERT(name.length() >= MaterialScheme::min_name_length);

        // Create a new scheme.
        MaterialScheme *newScheme = new MaterialScheme(name);
        materialSchemes.insert(name.toLower(), newScheme);
        materialSchemeCreationOrder.append(newScheme);

        // We want notification when a new manifest is defined in this scheme.
        newScheme->audienceForManifestDefined += this;
    }

    /// Observes MaterialScheme ManifestDefined.
    void materialSchemeManifestDefined(MaterialScheme & /*scheme*/, MaterialManifest &manifest)
    {
        /// Number of elements to block-allocate in the material index to material manifest map.
        int const MANIFESTIDMAP_BLOCK_ALLOC = 32;

        // We want notification when the manifest is derived to produce a material.
        manifest.audienceForMaterialDerived += this;

        // We want notification when the manifest is about to be deleted.
        manifest.audienceForDeletion += this;

        // Acquire a new unique identifier for the manifest.
        materialid_t const id = materialid_t(++materialManifestCount); // 1-based.
        manifest.setId(id);

        // Add the new manifest to the id index/map.
        if (materialManifestCount > int(materialManifestIdMapSize))
        {
            // Allocate more memory.
            materialManifestIdMapSize += MANIFESTIDMAP_BLOCK_ALLOC;
            materialManifestIdMap = (MaterialManifest **) M_Realloc(materialManifestIdMap, sizeof(*materialManifestIdMap) * materialManifestIdMapSize);
        }
        materialManifestIdMap[materialManifestCount - 1] = &manifest;
    }

    /// Observes MaterialManifest MaterialDerived.
    void materialManifestMaterialDerived(MaterialManifest & /*manifest*/, Material &material)
    {
        // Include this new material in the scheme-agnostic list of instances.
        materials.append(&material);

        // We want notification when the material is about to be deleted.
        material.audienceForDeletion() += this;
    }

    /// Observes MaterialManifest Deletion.
    void materialManifestBeingDeleted(MaterialManifest const &manifest)
    {
        foreach (MaterialManifestGroup *group, materialGroups)
        {
            group->remove(const_cast<MaterialManifest *>(&manifest));
        }
        materialManifestIdMap[manifest.id() - 1 /*1-based*/] = 0;

        // There will soon be one fewer manifest in the system.
        materialManifestCount -= 1;
    }

    /// Observes Material Deletion.
    void materialBeingDeleted(Material const &material)
    {
        materials.removeOne(const_cast<Material *>(&material));
    }
};

Materials::Materials()
    : d(new Impl(this))
{}

MaterialScheme &Materials::materialScheme(String name) const
{
    if (!name.isEmpty())
    {
        auto found = d->materialSchemes.find(name.toLower());
        if (found != d->materialSchemes.end()) return **found;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw Resources::UnknownSchemeError("Materials::materialScheme",
                                        "No scheme found matching '" + name + "'");
}

bool Materials::isKnownMaterialScheme(String name) const
{
    if (!name.isEmpty())
    {
        return d->materialSchemes.contains(name.toLower());
    }
    return false;
}

int Materials::materialSchemeCount() const
{
    return d->materialSchemes.count();
}

LoopResult Materials::forAllMaterialSchemes(std::function<LoopResult (MaterialScheme &)> func) const
{
    for (MaterialScheme *scheme : d->materialSchemes)
    {
        if (auto result = func(*scheme)) return result;
    }
    return LoopContinue;
}

MaterialManifest &Materials::toMaterialManifest(materialid_t id) const
{
    duint32 idx = id - 1; // 1-based index.
    if (idx < duint32(d->materialManifestCount))
    {
        if (d->materialManifestIdMap[idx])
        {
            return *d->materialManifestIdMap[idx];
        }
        // Internal bookeeping error.
        DENG2_ASSERT(false);
    }
    /// @throw InvalidMaterialIdError The specified material id is invalid.
    throw UnknownMaterialIdError("Materials::toMaterialManifest",
                                 "Invalid material ID " + String::number(id) +
                                 ", valid range " +
                                 Rangei(1, d->materialManifestCount + 1).asText());
}

Material *Materials::materialPtr(de::Uri const &path)
{
    if (auto *manifest = materialManifestPtr(path)) return manifest->materialPtr();
    return nullptr;
}

bool Materials::hasMaterialManifest(de::Uri const &path) const
{
    return materialManifestPtr(path) != nullptr;
}

MaterialManifest &Materials::materialManifest(de::Uri const &uri) const
{
    if (auto *mm = materialManifestPtr(uri))
    {
        return *mm;
    }
    /// @throw MissingResourceManifestError  Failed to locate a matching manifest.
    throw Resources::MissingResourceManifestError("Materials::materialManifest",
                                                  "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

MaterialManifest *Materials::materialManifestPtr(de::Uri const &uri) const
{
    // Does the user want a manifest in a specific scheme?
    if (!uri.scheme().isEmpty())
    {
        MaterialScheme &specifiedScheme = materialScheme(uri.scheme());
        return specifiedScheme.tryFind(uri.path());
    }
    else
    {
        // No, check each scheme in priority order.
        foreach (MaterialScheme *scheme, d->materialSchemeCreationOrder)
        {
            if (auto *manifest = scheme->tryFind(uri.path()))
            {
                return manifest;
            }
        }
    }
    return nullptr;
}

dint Materials::materialCount() const
{
    return d->materials.count();
}

LoopResult Materials::forAllMaterials(std::function<LoopResult (Material &)> func) const
{
    for (Material *mat : d->materials)
    {
        if (auto result = func(*mat))
        {
            return result;
        }
    }
    return LoopContinue;
}

Materials::MaterialManifestGroup &Materials::newMaterialGroup()
{
    // Allocating one by one is inefficient, but it doesn't really matter.
    d->materialGroups.append(new MaterialManifestGroup());
    return *d->materialGroups.back();
}

Materials::MaterialManifestGroup &Materials::materialGroup(dint groupIdx) const
{
    groupIdx -= 1; // 1-based index.
    if (groupIdx >= 0 && groupIdx < d->materialGroups.count())
    {
        return *d->materialGroups[groupIdx];
    }
    /// @throw UnknownMaterialGroupError An unknown material group was referenced.
    throw UnknownMaterialGroupError("Materials::materialGroup",
                                    "Invalid group #" + String::number(groupIdx+1) + ", valid range " +
                                    Rangeui(1, d->materialGroups.count() + 1).asText());
}

Materials::MaterialManifestGroups const &Materials::allMaterialGroups() const
{
    return d->materialGroups;
}

void Materials::clearAllMaterialGroups()
{
    qDeleteAll(d->materialGroups);
    d->materialGroups.clear();
}

void Materials::clearAllMaterialSchemes()
{
    forAllMaterialSchemes([] (MaterialScheme &scheme) {
        scheme.clear();
        return LoopContinue;
    });
    DENG2_ASSERT(materialCount() == 0); // sanity check
}

Materials &Materials::get() // static
{
    return World::get().materials();
}

} // namespace world
