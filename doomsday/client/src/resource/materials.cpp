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
#  include "gl/gl_texmanager.h" // GL_TextureVariantSpec
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

    MaterialVariantSpec &getVariantSpecForContext(MaterialContextId contextId,
        int flags, byte border, int tClass, int tMap, int wrapS, int wrapT,
        int minFilter, int magFilter, int anisoFilter,
        bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
    {
        static MaterialVariantSpec tpl;

        texturevariantusagecontext_t primaryContext;
        switch(contextId)
        {
        case UiContext:         primaryContext = TC_UI;                 break;
        case MapSurfaceContext: primaryContext = TC_MAPSURFACE_DIFFUSE; break;
        case SpriteContext:     primaryContext = TC_SPRITE_DIFFUSE;     break;
        case ModelSkinContext:  primaryContext = TC_MODELSKIN_DIFFUSE;  break;
        case PSpriteContext:    primaryContext = TC_PSPRITE_DIFFUSE;    break;
        case SkySphereContext:  primaryContext = TC_SKYSPHERE_DIFFUSE;  break;

        default: DENG2_ASSERT(0);
        }

        texturevariantspecification_t &primarySpec =
            GL_TextureVariantSpec(primaryContext, flags, border, tClass, tMap,
                                  wrapS, wrapT, minFilter, magFilter, anisoFilter,
                                  mipmapped, gammaCorrection, noStretch, toAlpha);

        // Apply the normalized spec to the template.
        tpl.context     = contextId;
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

    // We want notification when a new manifest is defined in this scheme.
    newScheme->audienceForManifestDefined += this;

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
    throw UnknownIdError("Materials::toManifest", QString("Invalid material ID %1, valid range [1..%2)").arg(id).arg(d->manifestCount + 1));
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

MaterialManifest &Materials::find(Uri const &uri) const
{
    LOG_AS("Materials::find");

    // Does the user want a manifest in a specific scheme?
    if(!uri.scheme().isEmpty())
    {
        Scheme &specifiedScheme = scheme(uri.scheme());
        if(specifiedScheme.has(uri.path()))
        {
            return specifiedScheme.find(uri.path());
        }
    }
    else
    {
        // No, check each scheme in priority order.
        foreach(Scheme *scheme, d->schemeCreationOrder)
        {
            if(scheme->has(uri.path()))
            {
                return scheme->find(uri.path());
            }
        }
    }

    /// @throw NotFoundError Failed to locate a matching manifest.
    throw NotFoundError("Materials::find", "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

void Materials::schemeManifestDefined(MaterialScheme &scheme, MaterialManifest &manifest)
{
    DENG2_UNUSED(scheme);

    // We want notification when the manifest is derived to produce a material.
    manifest.audienceForMaterialDerived += this;

    // Acquire a new unique identifier for the manifest.
    materialid_t const id = ++d->manifestCount; // 1-based.
    manifest.setId(id);

    // Add the new manifest to the id index/map.
    if(d->manifestCount > d->manifestIdMapSize)
    {
        // Allocate more memory.
        d->manifestIdMapSize += MANIFESTIDMAP_BLOCK_ALLOC;
        d->manifestIdMap = (Manifest **) M_Realloc(d->manifestIdMap, sizeof *d->manifestIdMap * d->manifestIdMapSize);
    }
    d->manifestIdMap[d->manifestCount - 1] = &manifest;
}

void Materials::manifestMaterialDerived(MaterialManifest &manifest, Material &material)
{
    DENG2_UNUSED(manifest);

    // Include this new material in the scheme-agnostic list of instances.
    d->materials.push_back(&material);

    // We want notification when the material is about to be deleted.
    material.audienceForDeletion += this;
}

void Materials::materialBeingDeleted(Material const &material)
{
    d->materials.removeOne(const_cast<Material *>(&material));
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

MaterialVariantSpec const &Materials::variantSpec(MaterialContextId contextId,
    int flags, byte border, int tClass, int tMap, int wrapS, int wrapT,
    int minFilter, int magFilter, int anisoFilter,
    bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
{
    return d->getVariantSpecForContext(contextId, flags, border, tClass, tMap, wrapS, wrapT,
                                       minFilter, magFilter, anisoFilter,
                                       mipmapped, gammaCorrection, noStretch, toAlpha);
}

#endif // __CLIENT__

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

    if(scheme)
    {
        // Consider materials in the specified scheme only.
        count += collectManifestsInScheme(*scheme, pathBeginsWithComparator, (void *)&path, storage);
    }
    else
    {
        // Consider materials in any scheme.
        foreach(MaterialScheme *scheme, App_Materials().allSchemes())
        {
            count += collectManifestsInScheme(*scheme, pathBeginsWithComparator, (void *)&path, storage);
        }
    }

    // Are we done?
    if(storage) return *storage;

    // Collect and populate.
    QList<MaterialManifest *> result;
    if(count == 0) return result;

#ifdef DENG2_QT_4_7_OR_NEWER
    result.reserve(count);
#endif
    return collectManifests(scheme, path, &result);
}

/**
 * Decode and then lexicographically compare the two manifest paths,
 * returning @c true if @a is less than @a b.
 */
static bool compareManifestPathsAssending(MaterialManifest const *a, MaterialManifest const *b)
{
    String pathA(QString(QByteArray::fromPercentEncoding(a->path().toUtf8())));
    String pathB(QString(QByteArray::fromPercentEncoding(b->path().toUtf8())));
    return pathA.compareWithoutCase(pathB) < 0;
}

/**
 * @param scheme    Material subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Material path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printIndex2(MaterialScheme *scheme, Path const &like,
                       Uri::ComposeAsTextFlags composeUriFlags)
{
    QList<MaterialManifest *> found = collectManifests(scheme, like);
    if(found.isEmpty()) return 0;

    bool const printSchemeName = !(composeUriFlags & Uri::OmitScheme);

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
    qSort(found.begin(), found.end(), compareManifestPathsAssending);
    int idx = 0;
    foreach(MaterialManifest *manifest, found)
    {
        String info = String(" %1: ").arg(idx, numFoundDigits)
                    + manifest->description(composeUriFlags);

        Con_FPrintf(!manifest->hasMaterial()? CPF_LIGHT : CPF_WHITE, "%s\n", info.toUtf8().constData());
        idx++;
    }

    return found.count();
}

static void printIndex(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    Materials &materials = App_Materials();

    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printIndex2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        Con_PrintRuler();
    }
    // Print results within only the one scheme?
    else if(materials.knownScheme(search.scheme()))
    {
        printTotal = printIndex2(&materials.scheme(search.scheme()), search.path(), flags | de::Uri::OmitScheme);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach(MaterialScheme *scheme, materials.allSchemes())
        {
            int numPrinted = printIndex2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if(numPrinted)
            {
                Con_PrintRuler();
                printTotal += numPrinted;
            }
        }
    }
    Con_Message("Found %i %s.", printTotal, printTotal == 1? "Material" : "Materials");
}

} // namespace de

static bool isKnownSchemeCallback(de::String name)
{
    return App_Materials().knownScheme(name);
}

D_CMD(ListMaterials)
{
    DENG2_UNUSED(src);

    de::Materials &materials = App_Materials();
    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1, &isKnownSchemeCallback);
    if(!search.scheme().isEmpty() && !materials.knownScheme(search.scheme()))
    {
        Con_Message("Unknown scheme '%s'.", search.schemeCStr());
        return false;
    }

    de::printIndex(search);
    return true;
}

D_CMD(InspectMaterial)
{
    DENG2_UNUSED(src);

    de::Materials &materials = App_Materials();
    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1);
    if(!search.scheme().isEmpty() && !materials.knownScheme(search.scheme()))
    {
        Con_Message("Unknown scheme '%s'.", search.schemeCStr());
        return false;
    }

    try
    {
        de::MaterialManifest &manifest = materials.find(search);
        if(manifest.hasMaterial())
        {
            Material &material = manifest.material();

            // Print material description:
            Con_Message(material.description().toUtf8().constData());

            // Print material synopsis:
            Con_Message(material.synopsis().toUtf8().constData());

#if defined(__CLIENT__) && defined(DENG_DEBUG)
            // Print current animation states?
            if(material.hasAnimatedLayers() || material.hasAnimatedDecorations())
            {
                Con_PrintRuler();

                foreach(MaterialAnimation *animation, material.animations())
                {
                    Con_Message("Animation Context #%i:"
                                "\n  Layer  Stage Tics", int(animation->context()));

                    QString indexKey = QString();
                    Con_Message(indexKey.toUtf8().constData());

                    // Print layer state info:
                    int const layerCount = material.layerCount();
                    for(int i = 0; i < layerCount; ++i)
                    {
                        Material::Layer *layer = material.layers()[i];
                        if(!layer->isAnimated()) continue;

                        MaterialAnimation::LayerState const &l = animation->layer(i);
                        QString info = QString("  %1 %2 %3")
                                           .arg(QString("#%1:").arg(i), 6)
                                           .arg(l.stage, -5)
                                           .arg(int(l.tics), -3);
                        Con_Message(info.toUtf8().constData());
                    }

                    // Print detail layer state info:
                    if(material.isDetailed() && material.detailLayer().isAnimated())
                    {
                        MaterialAnimation::LayerState const &l = animation->detailLayer();
                        QString info = QString("  %1 %2 %3")
                                           .arg("Detail:")
                                           .arg(l.stage, -5)
                                           .arg(int(l.tics), -3);
                        Con_Message(info.toUtf8().constData());
                    }

                    // Print shine layer state info:
                    if(material.isShiny() && material.shineLayer().isAnimated())
                    {
                        MaterialAnimation::LayerState const &l = animation->shineLayer();
                        QString info = QString("  %1 %2 %3")
                                           .arg("Shine:")
                                           .arg(l.stage, -5)
                                           .arg(int(l.tics), -3);
                        Con_Message(info.toUtf8().constData());
                    }

                    // Print decoration state info:
                    if(material.isDecorated())
                    {
                        Con_Message("  Decor  Stage Tics");

                        int const decorationCount = material.decorationCount();
                        for(int i = 0; i < decorationCount; ++i)
                        {
                            Material::Decoration *decor = material.decorations()[i];
                            if(!decor->isAnimated()) continue;

                            MaterialAnimation::DecorationState const &l = animation->decoration(i);
                            QString info = QString("  %1 %2 %3")
                                               .arg(QString("#%1:").arg(i), 6)
                                               .arg(l.stage, -5)
                                               .arg(int(l.tics), -3);
                            Con_Message(info.toUtf8().constData());
                        }
                    }
                }
            }

            if(material.variantCount())
            {
                // Print variant specs.
                Con_PrintRuler();

                int variantIdx = 0;
                foreach(MaterialVariant *variant, material.variants())
                {
                    Con_Message("Variant #%i: Spec:%p", variantIdx, de::dintptr(&variant->spec()));
                    ++variantIdx;
                }
            }

#endif // __CLIENT__ && DENG_DEBUG
        }
        else
        {
            de::String description = manifest.description();
            Con_FPrintf(CPF_LIGHT, "%s\n", description.toUtf8().constData());
        }
        return true;
    }
    catch(de::Materials::NotFoundError const &er)
    {
        QString msg = er.asText() + ".";
        Con_Message(msg.toUtf8().constData());
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
        Con_Message("Scheme: %s (%u %s)", scheme->name().toUtf8().constData(), count, count == 1? "material":"materials");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif
