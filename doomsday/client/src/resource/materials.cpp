/** @file materials.cpp Material Resource Collection.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#ifdef __CLIENT__
#  include "gl/gl_texmanager.h" // GL_TextureVariantSpecificationForContext
#  include "MaterialSnapshot"
#endif
#include <de/Log>
#include <de/math.h>
#include <de/mathutil.h> // for M_NumDigits
#include <de/memory.h>
#include <QtAlgorithms>

#include "resource/materials.h"

/// Number of elements to block-allocate in the material index to material manifest map.
#define MANIFESTIDMAP_BLOCK_ALLOC (32)

D_CMD(InspectMaterial);
D_CMD(ListMaterials);
#ifdef DENG_DEBUG
D_CMD(PrintMaterialStats);
#endif

#ifdef __CLIENT__

/// A list of specifications for material variants.
typedef QList<de::MaterialVariantSpec *> VariantSpecs;

/**
 * Stores the arguments for a material variant cache work item.
 */
struct VariantCacheTask
{
    /// The material from which to cache a variant.
    Material *material;

    /// Specification of the variant to be cached.
    de::MaterialVariantSpec const *spec;

    VariantCacheTask(Material &_material, de::MaterialVariantSpec const &_spec)
        : material(&_material), spec(&_spec)
    {}
};

/// A FIFO queue of material variant caching tasks.
/// Implemented as a list because we may need to remove tasks from the queue if
/// the material is destroyed in the mean time.
typedef QList<VariantCacheTask *> VariantCacheQueue;

#endif // __CLIENT__

namespace de {

DENG2_PIMPL(Materials)
{
    /// System subspace schemes containing the manifests.
    Materials::Schemes schemes;
    QList<MaterialScheme *> schemeCreationOrder;

#ifdef __CLIENT__
    /// Material variant specifications.
    VariantSpecs variantSpecs;

    /// Queue of variant cache tasks.
    VariantCacheQueue variantCacheQueue;
#endif

    /// All material instances in the system (from all schemes).
    Materials::All materials;

    /// Manifest groups.
    Materials::ManifestGroups groups;

    /// Total number of URI material manifests (in all schemes).
    uint manifestCount;

    /// LUT which translates materialid_t => MaterialManifest*.
    /// Index with materialid_t-1
    uint manifestIdMapSize;
    MaterialManifest **manifestIdMap;

    Instance(Public *i) : Base(i),
        manifestCount(0),
        manifestIdMapSize(0),
        manifestIdMap(0)
    {}

    ~Instance()
    {
#ifdef __CLIENT__
        self.purgeCacheQueue();
#endif

        self.destroyAllGroups();
        clearManifests();
        clearMaterials();

#ifdef __CLIENT__
        clearVariantSpecs();
#endif
    }

#ifdef __CLIENT__
    void clearVariantSpecs()
    {
        qDeleteAll(variantSpecs);
        variantSpecs.clear();
    }
#endif

    void clearMaterials()
    {
        qDeleteAll(materials);
        materials.clear();
    }

    void clearManifests()
    {
        qDeleteAll(schemes);
        schemes.clear();
        schemeCreationOrder.clear();

        // Clear the manifest index/map.
        if(manifestIdMap)
        {
            M_Free(manifestIdMap); manifestIdMap = 0;
            manifestIdMapSize = 0;
        }
        manifestCount = 0;
    }

#ifdef __CLIENT__
    MaterialVariantSpec *findVariantSpec(MaterialVariantSpec const &tpl,
                                         bool canCreate)
    {
        foreach(MaterialVariantSpec *spec, variantSpecs)
        {
            if(spec->compare(tpl)) return spec;
        }

        if(!canCreate) return 0;

        variantSpecs.push_back(new MaterialVariantSpec(tpl));
        return variantSpecs.back();
    }

    MaterialVariantSpec &getVariantSpecForContext(materialcontext_t mc,
        int flags, byte border, int tClass, int tMap, int wrapS, int wrapT,
        int minFilter, int magFilter, int anisoFilter,
        bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
    {
        static MaterialVariantSpec tpl;

        DENG2_ASSERT(mc == MC_UNKNOWN || VALID_MATERIALCONTEXT(mc));

        texturevariantusagecontext_t primaryContext;
        switch(mc)
        {
        case MC_UI:             primaryContext = TC_UI;                 break;
        case MC_MAPSURFACE:     primaryContext = TC_MAPSURFACE_DIFFUSE; break;
        case MC_SPRITE:         primaryContext = TC_SPRITE_DIFFUSE;     break;
        case MC_MODELSKIN:      primaryContext = TC_MODELSKIN_DIFFUSE;  break;
        case MC_PSPRITE:        primaryContext = TC_PSPRITE_DIFFUSE;    break;
        case MC_SKYSPHERE:      primaryContext = TC_SKYSPHERE_DIFFUSE;  break;
        default:                primaryContext = TC_UNKNOWN;            break;
        }

        texturevariantspecification_t &primarySpec =
            GL_TextureVariantSpecificationForContext(primaryContext, flags, border,
                                                     tClass, tMap, wrapS, wrapT,
                                                     minFilter, magFilter, anisoFilter,
                                                     mipmapped, gammaCorrection, noStretch,
                                                     toAlpha);

        // Apply the normalized spec to the template.
        DENG2_ASSERT(mc == MC_UNKNOWN || VALID_MATERIALCONTEXT(mc));
        tpl.context     = mc;
        tpl.primarySpec = &primarySpec;

        return *findVariantSpec(tpl, true);
    }
#endif
};

void Materials::consoleRegister()
{
    C_CMD("inspectmaterial",    "ss",   InspectMaterial)
    C_CMD("inspectmaterial",    "s",    InspectMaterial)
    C_CMD("listmaterials",      "ss",   ListMaterials)
    C_CMD("listmaterials",      "s",    ListMaterials)
    C_CMD("listmaterials",      "",     ListMaterials)

#ifdef DENG_DEBUG
    C_CMD("materialstats",      NULL,   PrintMaterialStats)
#endif
}

Materials::Materials() : d(new Instance(this))
{}

Materials::~Materials()
{
    delete d;
}

MaterialScheme &Materials::scheme(String name) const
{
    LOG_AS("Materials::scheme");
    if(!name.isEmpty())
    {
        Schemes::iterator found = d->schemes.find(name.toLower());
        if(found != d->schemes.end()) return **found;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw Materials::UnknownSchemeError("Materials::scheme", "No scheme found matching '" + name + "'");
}

MaterialScheme &Materials::createScheme(String name)
{
    DENG_ASSERT(name.length() >= Scheme::min_name_length);

    // Ensure this is a unique name.
    if(knownScheme(name)) return scheme(name);

    // Create a new scheme.
    Scheme *newScheme = new Scheme(name);
    d->schemes.insert(name.toLower(), newScheme);
    d->schemeCreationOrder.push_back(newScheme);
    return *newScheme;
}

bool Materials::knownScheme(String name) const
{
    if(!name.isEmpty())
    {
        Schemes::iterator found = d->schemes.find(name.toLower());
        if(found != d->schemes.end()) return true;
    }
    return false;
}

Materials::Schemes const& Materials::allSchemes() const
{
    return d->schemes;
}

MaterialManifest &Materials::toManifest(materialid_t id) const
{
    if(id > 0 && id <= d->manifestCount)
    {
        duint32 idx = id - 1; // 1-based index.
        if(d->manifestIdMap[idx])
            return *d->manifestIdMap[idx];

        // Internal bookeeping error.
        DENG_ASSERT(0);
    }
    /// @throw InvalidMaterialIdError The specified material id is invalid.
    throw InvalidMaterialIdError("Materials::toManifest", QString("Invalid material ID %1, valid range [1..%2)").arg(id).arg(d->manifestCount + 1));
}

void Materials::validateUri(Uri const &uri, UriValidationFlags flags) const
{
    if(uri.isEmpty())
    {
        /// @throw UriMissingPathError The URI is missing the required path component.
        throw UriMissingPathError("Materials::validateUri", "Missing path in URI \"" + uri.asText() + "\"");
    }

    if(uri.scheme().isEmpty())
    {
        if(!flags.testFlag(AnyScheme))
        {
            /// @throw UriMissingSchemeError The URI is missing the required scheme component.
            throw UriMissingSchemeError("Materials::validateUri", "Missing scheme in URI \"" + uri.asText() + "\"");
        }
    }
    else if(!knownScheme(uri.scheme()))
    {
        /// @throw UriUnknownSchemeError The URI specifies an unknown scheme.
        throw UriUnknownSchemeError("Materials::validateUri", "Unknown scheme in URI \"" + uri.asText() + "\"");
    }
}

MaterialManifest &Materials::find(Uri const &uri) const
{
    LOG_AS("Materials::find");

    try
    {
        validateUri(uri, AnyScheme);

        // Does the user want a manifest in a specific scheme?
        if(!uri.scheme().isEmpty())
        {
            try
            {
                return scheme(uri.scheme()).find(uri.path());
            }
            catch(Scheme::NotFoundError const &)
            {} // Ignore, we'll throw our own...
        }
        else
        {
            // No, check each scheme in priority order.
            foreach(Scheme *scheme, d->schemeCreationOrder)
            {
                try
                {
                    return scheme->find(uri.path());
                }
                catch(Scheme::NotFoundError const &)
                {} // Ignore, we'll throw our own...
            }
        }
    }
    catch(UriValidationError const &er)
    {
        /// @throw NotFoundError Failed to locate a matching manifest.
        throw NotFoundError("Materials::find", er.asText());
    }

    /// @throw NotFoundError Failed to locate a matching manifest.
    throw NotFoundError("Materials::find", "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

bool Materials::has(Uri const &path) const
{
    try
    {
        find(path);
        return true;
    }
    catch(NotFoundError const &)
    {} // Ignore this error.
    return false;
}

MaterialManifest &Materials::declare(de::Uri const &uri)
{
    LOG_AS("Materials::newManifest");

    // Have we already created a manifest for this?
    try
    {
        // We require a properly formed URI (but not a URN - this is a resource path).
        validateUri(uri, 0);

        return find(uri);
    }
    catch(NotFoundError const &)
    {
        // Acquire a new unique identifier for the manifest.
        materialid_t const id = ++d->manifestCount;

        Manifest *manifest = &scheme(uri.scheme()).insertManifest(uri.path(), id);

        // Add the new manifest to the id index/map.
        if(d->manifestCount > d->manifestIdMapSize)
        {
            // Allocate more memory.
            d->manifestIdMapSize += MANIFESTIDMAP_BLOCK_ALLOC;
            d->manifestIdMap = (Manifest **) M_Realloc(d->manifestIdMap, sizeof *d->manifestIdMap * d->manifestIdMapSize);
        }
        d->manifestIdMap[d->manifestCount - 1] = manifest; /* 1-based index */

        return *manifest;
    }
}

void Materials::addMaterial(Material &material)
{
    d->materials.push_back(&material);
}

Materials::All const &Materials::all() const
{
    return d->materials;
}

Materials::ManifestGroup &Materials::createGroup()
{
    // Allocating one by one is inefficient, but it doesn't really matter.
    d->groups.push_back(new ManifestGroup());
    return *d->groups.back();
}

Materials::ManifestGroup &Materials::group(int number) const
{
    number -= 1; // 1-based index.
    if(number >= 0 && number < d->groups.count())
    {
        return *d->groups[number];
    }
    /// @throw UnknownGroupError An unknown scheme was referenced.
    throw UnknownGroupError("Materials::group", QString("Invalid group number #%1, valid range [0..%2)")
                                                    .arg(number).arg(d->groups.count()));
}

Materials::ManifestGroups const &Materials::allGroups() const
{
    return d->groups;
}

void Materials::destroyAllGroups()
{
    qDeleteAll(d->groups);
    d->groups.clear();
}

#ifdef __CLIENT__

void Materials::purgeCacheQueue()
{
    qDeleteAll(d->variantCacheQueue);
    d->variantCacheQueue.clear();
}

void Materials::processCacheQueue()
{
    while(!d->variantCacheQueue.isEmpty())
    {
         QScopedPointer<VariantCacheTask> task(d->variantCacheQueue.takeFirst());

         /// @todo $revise-texture-animation: prepare all textures in the animation (if animated).
         task->material->prepare(*task->spec);
    }
}

void Materials::cache(Material &material, MaterialVariantSpec const &spec,
    bool cacheGroups)
{
    // Already in the queue?
    foreach(VariantCacheTask *task, d->variantCacheQueue)
    {
        if(&material == task->material && &spec == task->spec) return;
    }

    VariantCacheTask *newTask = new VariantCacheTask(material, spec);
    d->variantCacheQueue.push_back(newTask);

    if(!cacheGroups) return;

    // If the material is part of one or more groups; enqueue cache tasks for all
    // other materials within the same group(s). Although we could use a flag in
    // the task and have it find the groups come prepare time, this way we can be
    // sure there are no overlapping tasks.
    foreach(ManifestGroup *group, d->groups)
    {
        if(!group->contains(&material.manifest())) continue;

        foreach(Manifest *manifest, *group)
        {
            if(!manifest->hasMaterial()) continue;
            if(&manifest->material() == &material) continue; // We've already enqueued this.

            cache(manifest->material(), spec, false /* do not cache groups */);
        }
    }
}

MaterialVariantSpec const &Materials::variantSpecForContext(
    materialcontext_t mc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
{
    return d->getVariantSpecForContext(mc, flags, border, tClass, tMap, wrapS, wrapT,
                                       minFilter, magFilter, anisoFilter,
                                       mipmapped, gammaCorrection, noStretch, toAlpha);
}

static void printVariantInfo(MaterialVariant &variant, int variantIdx)
{
    Con_Message("Variant #%i: Spec:%p", variantIdx, de::dintptr(&variant.spec()));

    // Print layer state info:
    int const layerCount = variant.generalCase().layerCount();
    for(int i = 0; i < layerCount; ++i)
    {
        MaterialVariant::LayerState const &l = variant.layer(i);
        Con_Message("  Layer #%i: Stage:%i Tics:%i", i, l.stage, int(l.tics));
    }

    // Print detail layer state info:
    if(variant.generalCase().isDetailed())
    {
        MaterialVariant::LayerState const &l = variant.detailLayer();
        Con_Message("  DetailLayer #0: Stage:%i Tics:%i", l.stage, int(l.tics));
    }

    // Print shine layer state info:
    if(variant.generalCase().isShiny())
    {
        MaterialVariant::LayerState const &l = variant.shineLayer();
        Con_Message("  ShineLayer #0: Stage:%i Tics:%i", l.stage, int(l.tics));
    }

    // Print decoration state info:
    int const decorationCount = variant.generalCase().decorationCount();
    for(int i = 0; i < decorationCount; ++i)
    {
        MaterialVariant::DecorationState const &l = variant.decoration(i);
        Con_Message("  Decoration #%i: Stage:%i Tics:%i", i, l.stage, int(l.tics));
    }
}

#endif // __CLIENT__

static void printMaterialInfo(Material &material)
{
    // Print description:
    Con_Message(material.composeDescription().toUtf8().constData());

    // Print synopsis:
    Con_Message(material.composeSynopsis().toUtf8().constData());

#ifdef __CLIENT__
    if(!material.variantCount()) return;

    // Print variant specs and current animation states:
    Con_PrintRuler();

    int variantIdx = 0;
    foreach(MaterialVariant *variant, material.variants())
    {
        printVariantInfo(*variant, variantIdx);
        ++variantIdx;
    }

#endif // __CLIENT__
}

static void printManifestInfo(MaterialManifest &manifest,
    de::Uri::ComposeAsTextFlags uriCompositionFlags = de::Uri::DefaultComposeAsTextFlags)
{
    String info = String("%1 %2")
                      .arg(manifest.composeUri().compose(uriCompositionFlags | de::Uri::DecodePath),
                           ( uriCompositionFlags.testFlag(de::Uri::OmitScheme)? -14 : -22 ) )
                      .arg(manifest.sourceDescription(), -7);
#ifdef __CLIENT__
    info += String("x%1").arg(!manifest.hasMaterial()? 0 : manifest.material().variantCount());
#endif

    info += "\n";
    Con_FPrintf(!manifest.hasMaterial()? CPF_LIGHT : CPF_WHITE, info.toUtf8().constData());
}

static bool pathBeginsWithComparator(MaterialManifest const &manifest, void *parameters)
{
    Path const *path = reinterpret_cast<Path*>(parameters);
    /// @todo Use PathTree::Node::compare()
    return manifest.path().toStringRef().beginsWith(*path, Qt::CaseInsensitive);
}

/**
 * @todo This logic should be implemented in de::PathTree -ds
 */
static int collectManifestsInScheme(MaterialScheme const &scheme,
    bool (*predicate)(MaterialManifest const &manifest, void *parameters), void *parameters,
    QList<MaterialManifest *> *storage = 0)
{
    int count = 0;
    PathTreeIterator<MaterialScheme::Index> iter(scheme.index().leafNodes());
    while(iter.hasNext())
    {
        MaterialManifest &manifest = iter.next();
        if(predicate(manifest, parameters))
        {
            count += 1;
            if(storage) // Store mode?
            {
                storage->push_back(&manifest);
            }
        }
    }
    return count;
}

static QList<MaterialManifest *> collectManifests(MaterialScheme *scheme,
    Path const &path, QList<MaterialManifest *> *storage = 0)
{
    int count = 0;

    if(!path.isEmpty())
    {
        if(scheme)
        {
            // Consider materials in the specified scheme only.
            count += collectManifestsInScheme(*scheme, pathBeginsWithComparator, (void*)&path, storage);
        }
        else
        {
            // Consider materials in any scheme.
            foreach(MaterialScheme *scheme, App_Materials().allSchemes())
            {
                count += collectManifestsInScheme(*scheme, pathBeginsWithComparator, (void*)&path, storage);
            }
        }
    }

    if(storage)
    {
        return *storage;
    }

    QList<MaterialManifest *> result;
    if(count == 0) return result;

#ifdef DENG2_QT_4_7_OR_NEWER
    result.reserve(count);
#endif
    return collectManifests(scheme, path, &result);
}

/**
 * Decode and then lexicographically compare the two manifest
 * paths, returning @c true if @a is less than @a b.
 */
static bool compareMaterialManifestPathsAssending(MaterialManifest const *a, MaterialManifest const *b)
{
    String pathA = QString(QByteArray::fromPercentEncoding(a->path().toUtf8()));
    String pathB = QString(QByteArray::fromPercentEncoding(b->path().toUtf8()));
    return pathA.compareWithoutCase(pathB) < 0;
}

/**
 * @param scheme    Material subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Material path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printMaterials2(MaterialScheme *scheme, Path const &like,
                           Uri::ComposeAsTextFlags composeUriflags)
{
    QList<MaterialManifest *> found = collectManifests(scheme, like);
    if(found.isEmpty()) return 0;

    bool const printSchemeName = !(composeUriflags & Uri::OmitScheme);

    // Print a heading.
    String heading = "Known materials";
    if(!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if(!like.isEmpty())
        heading += " like \"" + like + "\"";
    heading += ":";
    Con_FPrintf(CPF_YELLOW, "%s\n", heading.toUtf8().constData());

    // Print an index key.
    int const numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    String indexKey = QString(" %1  %2 source")
                          .arg("idx", -numFoundDigits)
                          .arg(printSchemeName? "scheme:path" : "path", printSchemeName? -22 : -14);
#ifdef __CLIENT__
    indexKey += " n#";
#endif
    Con_Message(indexKey.toUtf8().constData());
    Con_PrintRuler();

    // Sort and print the index.
    qSort(found.begin(), found.end(), compareMaterialManifestPathsAssending);
    int idx = 0;
    foreach(MaterialManifest *manifest, found)
    {
        Con_Printf(" %*i: ", numFoundDigits, idx++);
        printManifestInfo(*manifest, composeUriflags);
    }

    return found.count();
}

static void printMaterials(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    Materials &materials = App_Materials();

    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printMaterials2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        Con_PrintRuler();
    }
    // Print results within only the one scheme?
    else if(materials.knownScheme(search.scheme()))
    {
        printTotal = printMaterials2(&materials.scheme(search.scheme()), search.path(), flags | de::Uri::OmitScheme);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach(MaterialScheme *scheme, materials.allSchemes())
        {
            int numPrinted = printMaterials2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if(numPrinted)
            {
                Con_PrintRuler();
                printTotal += numPrinted;
            }
        }
    }
    Con_Printf("Found %i %s.\n", printTotal, printTotal == 1? "Material" : "Materials");
}

} // namespace de

static bool isKnownSchemeCallback(de::String name)
{
    return App_Textures().knownScheme(name);
}

D_CMD(ListMaterials)
{
    DENG2_UNUSED(src);

    de::Materials &materials = App_Materials();
    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1, &isKnownSchemeCallback);
    if(!search.scheme().isEmpty() && !materials.knownScheme(search.scheme()))
    {
        Con_Printf("Unknown scheme '%s'.\n", search.schemeCStr());
        return false;
    }

    de::printMaterials(search);
    return true;
}

D_CMD(InspectMaterial)
{
    DENG2_UNUSED(src);

    de::Materials &materials = App_Materials();
    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1);
    if(!search.scheme().isEmpty() && !materials.knownScheme(search.scheme()))
    {
        Con_Printf("Unknown scheme '%s'.\n", search.schemeCStr());
        return false;
    }

    try
    {
        de::MaterialManifest &manifest = materials.find(search);
        if(manifest.hasMaterial())
        {
            de::printMaterialInfo(manifest.material());
        }
        else
        {
            de::printManifestInfo(manifest);
        }
        return true;
    }
    catch(de::Materials::NotFoundError const &er)
    {
        QString msg = er.asText() + ".";
        Con_Printf("%s\n", msg.toUtf8().constData());
    }
    return false;
}

#ifdef DENG_DEBUG
D_CMD(PrintMaterialStats)
{
    DENG2_UNUSED3(src, argc, argv);

    de::Materials &materials = App_Materials();

    Con_FPrintf(CPF_YELLOW, "Material Statistics:\n");
    foreach(de::MaterialScheme *scheme, materials.allSchemes())
    {
        de::MaterialScheme::Index const &index = scheme->index();

        uint count = index.count();
        Con_Printf("Scheme: %s (%u %s)\n", scheme->name().toUtf8().constData(), count, count == 1? "material":"materials");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif
