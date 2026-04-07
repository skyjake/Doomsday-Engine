/** @file materials.cpp
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

#include "doomsday/world/materials.h"
#include "doomsday/world/materialscheme.h"
#include "doomsday/world/materialmanifest.h"
#include "doomsday/world/world.h"
#include "doomsday/res/resources.h"

#include <de/legacy/memory.h>
#include <de/keymap.h>
#include <de/list.h>
#include <unordered_set>

using namespace de;

namespace world {

//struct SchemeHashKey
//{
//    String scheme;

//    SchemeHashKey(const String &s) : scheme(s) {}
//    bool operator == (const SchemeHashKey &other) const {
//        return !scheme.compare(other.scheme, Qt::CaseInsensitive);
//    }
//};

//uint qHash(const SchemeHashKey &key)
//{
//    return key.scheme.at(1).toLower().unicode();
//}

DE_PIMPL(Materials)
, DE_OBSERVES(MaterialScheme,   ManifestDefined)
, DE_OBSERVES(MaterialManifest, MaterialDerived)
, DE_OBSERVES(MaterialManifest, Deletion)
, DE_OBSERVES(Material,         Deletion)
{
    /// System subspace schemes containing the manifests/resources.
    de::KeyMap<String, MaterialScheme *, String::InsensitiveLessThan> materialSchemes;
    List<MaterialScheme *> materialSchemeCreationOrder;

    List<Material *> materials;       ///< From all schemes.
    int materialManifestCount = 0;     ///< Total number of material manifests (in all schemes).

    std::unordered_set<Material *> animatedMaterialsSubset; ///< Subset of materials (not owned) that need to animate.

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
        self().clearAllMaterialGroups();
        self().clearAllMaterialSchemes();
        clearMaterialManifests();
    }

    void clearMaterialManifests()
    {
        materialSchemes.deleteAll();
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
        DE_ASSERT(name.length() >= MaterialScheme::min_name_length);

        // Create a new scheme.
        MaterialScheme *newScheme = new MaterialScheme(name);
        materialSchemes.insert(name, newScheme);
        materialSchemeCreationOrder.append(newScheme);

        // We want notification when a new manifest is defined in this scheme.
        newScheme->audienceForManifestDefined += this;
    }

    /// Observes MaterialScheme ManifestDefined.
    void materialSchemeManifestDefined(MaterialScheme & /*scheme*/, MaterialManifest &manifest)
    {
        /// Number of elements to block-allocate in the material index to material manifest map.
        const int MANIFESTIDMAP_BLOCK_ALLOC = 32;

        // We want notification when the manifest is derived to produce a material.
        manifest.audienceForMaterialDerived += this;

        // We want notification when the manifest is about to be deleted.
        manifest.audienceForDeletion += this;

        // Acquire a new unique identifier for the manifest.
        const materialid_t id = materialid_t(++materialManifestCount); // 1-based.
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
    void materialManifestBeingDeleted(const MaterialManifest &manifest)
    {
        for (MaterialManifestGroup *group : materialGroups)
        {
            group->remove(const_cast<MaterialManifest *>(&manifest));
        }
        materialManifestIdMap[manifest.id() - 1 /*1-based*/] = 0;

        // There will soon be one fewer manifest in the system.
        materialManifestCount -= 1;
    }

    /// Observes Material Deletion.
    void materialBeingDeleted(const Material &material)
    {
        Material *pMat = const_cast<Material *>(&material);
        materials.removeOne(pMat);
        animatedMaterialsSubset.erase(pMat);
    }
};

Materials::Materials()
    : d(new Impl(this))
{}

MaterialScheme &Materials::materialScheme(const String& name) const
{
    if (!name.isEmpty())
    {
        auto found = d->materialSchemes.find(name);
        if (found != d->materialSchemes.end()) return *found->second;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw Resources::UnknownSchemeError("Materials::materialScheme",
                                        "No scheme found matching '" + name + "'");
}

bool Materials::isKnownMaterialScheme(const String& name) const
{
    if (!name.isEmpty())
    {
        return d->materialSchemes.contains(name);
    }
    return false;
}

int Materials::materialSchemeCount() const
{
    return d->materialSchemes.size();
}

LoopResult Materials::forAllMaterialSchemes(const std::function<LoopResult (MaterialScheme &)>& func) const
{
    for (const auto &s : d->materialSchemes)
    {
        if (auto result = func(*s.second)) return result;
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
        DE_ASSERT_FAIL("Bad index");
    }
    /// @throw InvalidMaterialIdError The specified material id is invalid.
    throw UnknownMaterialIdError("Materials::toMaterialManifest",
                                 "Invalid material ID " + String::asText(id) +
                                 ", valid range " +
                                 Rangei(1, d->materialManifestCount + 1).asText());
}

Material *Materials::materialPtr(const res::Uri &path)
{
    if (auto *manifest = materialManifestPtr(path)) return manifest->materialPtr();
    return nullptr;
}

bool Materials::hasMaterialManifest(const res::Uri &path) const
{
    return materialManifestPtr(path) != nullptr;
}

MaterialManifest &Materials::materialManifest(const res::Uri &uri) const
{
    if (auto *mm = materialManifestPtr(uri))
    {
        return *mm;
    }
    /// @throw MissingResourceManifestError  Failed to locate a matching manifest.
    throw Resources::MissingResourceManifestError("Materials::materialManifest",
                                                  "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

MaterialManifest *Materials::materialManifestPtr(const res::Uri &uri) const
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
        for (MaterialScheme *scheme : d->materialSchemeCreationOrder)
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

LoopResult Materials::forAllMaterials(const std::function<LoopResult (Material &)> &func) const
{
    for (Material *mat : d.getConst()->materials)
    {
        if (auto result = func(*mat))
        {
            return result;
        }
    }
    return LoopContinue;
}

LoopResult Materials::forAnimatedMaterials(const std::function<LoopResult (Material &)> &func) const
{
    for (Material *mat : d.getConst()->animatedMaterialsSubset)
    {
        if (auto result = func(*mat))
        {
            return result;
        }
    }
    return LoopContinue;
}

void Materials::updateLookup()
{
    d->animatedMaterialsSubset.clear();
    for (auto *mat : d->materials)
    {
        if (mat->isAnimated())
        {
            d->animatedMaterialsSubset.insert(mat);
        }
    }
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
                                    "Invalid group #" + String::asText(groupIdx + 1) +
                                        ", valid range " +
                                    Rangeui(1, d->materialGroups.count() + 1).asText());
}

const Materials::MaterialManifestGroups &Materials::allMaterialGroups() const
{
    return d->materialGroups;
}

void Materials::clearAllMaterialGroups()
{
    deleteAll(d->materialGroups);
    d->materialGroups.clear();
}

void Materials::clearAllMaterialSchemes()
{
    forAllMaterialSchemes([] (MaterialScheme &scheme) {
        scheme.clear();
        return LoopContinue;
    });
    DE_ASSERT(materialCount() == 0); // sanity check
}

Materials &Materials::get() // static
{
    return World::get().materials();
}

} // namespace world
