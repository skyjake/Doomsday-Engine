/** @file materials.cpp Material Resource Collection.
 *
 * @authors Copyright � 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright � 2005-2013 Daniel Swanson <danij@dengine.net>
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
#include "de_audio.h" // For environmental audio properties.

#include <de/Error>
#include <de/Log>
#include <de/PathTree>
#include <de/memory.h>
#include <de/math.h>

#include <QtAlgorithms>

#include "gl/gl_texmanager.h" // GL_TextureVariantSpecificationForContext
#include "r_util.h" // R_NameForBlendMode
#include "MaterialSnapshot"

#include "Materials"

/// Number of elements to block-allocate in the material index to material manifest map.
#define MANIFESTIDMAP_BLOCK_ALLOC (32)

D_CMD(InspectMaterial);
D_CMD(ListMaterials);
#if _DEBUG
D_CMD(PrintMaterialStats);
#endif

namespace de {

struct Materials::ManifestGroup::Instance
{
    /// Unique identifier.
    int id;

    /// All material manifests in the group.
    Materials::ManifestGroup::All manifests;

    Instance(int _id) : id(_id)
    {}
};

Materials::ManifestGroup::ManifestGroup(int id) : d(new Instance(id))
{}

Materials::ManifestGroup::~ManifestGroup()
{
    delete d;
}

int Materials::ManifestGroup::id() const
{
    return d->id;
}

Materials::Manifest &Materials::ManifestGroup::toManifest(int number) const
{
    if(number >= 0 && number < count())
    {
        return *d->manifests[number];
    }
    /// @throw InvalidManifestError Attempt to access an invalid manifest.
    throw InvalidManifestError("Materials::Group::manifest", QString("Invalid manifest #%1, valid range [0..%2)").arg(number).arg(count()));
}

void Materials::ManifestGroup::add(Materials::Manifest &manifest)
{
    LOG_AS("Materials::Group::addManifest");
    // Ensure the manifest is not already a group member.
    if(has(manifest)) return;
    d->manifests.push_back(&manifest);
}

bool Materials::ManifestGroup::has(Materials::Manifest &manifest) const
{
    return d->manifests.contains(&manifest);
}

Materials::ManifestGroup::All const &Materials::ManifestGroup::all() const
{
    return d->manifests;
}

static void applyVariantSpec(MaterialVariantSpec &spec, materialcontext_t mc,
    texturevariantspecification_t *primarySpec)
{
    DENG2_ASSERT(mc == MC_UNKNOWN || VALID_MATERIALCONTEXT(mc) && primarySpec);
    spec.context     = mc;
    spec.primarySpec = primarySpec;
}

/// A list of specifications for material variants.
typedef QList<MaterialVariantSpec *> VariantSpecs;

/**
 * Stores the arguments for a material variant cache work item.
 */
struct VariantCacheTask
{
    /// The material from which to cache a variant.
    Material *material;

    /// Specification of the variant to be cached.
    MaterialVariantSpec const *spec;

    VariantCacheTask(Material &_material, MaterialVariantSpec const &_spec)
        : material(&_material), spec(&_spec)
    {}
};

/// A FIFO queue of material variant caching tasks.
/// Implemented as a list because we may need to remove tasks from the queue if
/// the material is destroyed in the mean time.
typedef QList<VariantCacheTask *> VariantCacheQueue;

DENG2_PIMPL(Materials)
{
    /// System subspace schemes containing the manifests.
    Materials::Schemes schemes;
    QList<Materials::Scheme *> schemeCreationOrder;

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
    Materials::Manifest **manifestIdMap;

    Instance(Public &a) : Base(a),
        manifestCount(0),
        manifestIdMapSize(0),
        manifestIdMap(0)
    {}

    ~Instance()
    {
        clearGroups();
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

    void clearGroups()
    {
        qDeleteAll(groups);
        groups.clear();
    }

#ifdef __CLIENT__
    Materials::VariantSpec *findVariantSpec(Materials::VariantSpec const &tpl,
                                            bool canCreate)
    {
        foreach(Materials::VariantSpec *spec, variantSpecs)
        {
            if(spec->compare(tpl)) return spec;
        }

        if(!canCreate) return 0;

        variantSpecs.push_back(new Materials::VariantSpec(tpl));
        return variantSpecs.back();
    }

    Materials::VariantSpec &getVariantSpecForContext(materialcontext_t mc,
        int flags, byte border, int tClass, int tMap, int wrapS, int wrapT,
        int minFilter, int magFilter, int anisoFilter,
        bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
    {
        static Materials::VariantSpec tpl;

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

        texturevariantspecification_t* primarySpec =
            GL_TextureVariantSpecificationForContext(primaryContext, flags, border,
                                                     tClass, tMap, wrapS, wrapT,
                                                     minFilter, magFilter, anisoFilter,
                                                     mipmapped, gammaCorrection, noStretch,
                                                     toAlpha);

        applyVariantSpec(tpl, mc, primarySpec);
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

Materials::Materials() : d(new Instance(*this))
{}

Materials::~Materials()
{
    purgeCacheQueue();
    delete d;
}

Materials::Scheme &Materials::scheme(String name) const
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

Materials::Scheme& Materials::createScheme(String name)
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

void Materials::purgeCacheQueue()
{
#ifdef __CLIENT__
    qDeleteAll(d->variantCacheQueue);
    d->variantCacheQueue.clear();
#endif
}

void Materials::processCacheQueue()
{
#ifdef __CLIENT__
    while(!d->variantCacheQueue.isEmpty())
    {
         QScopedPointer<VariantCacheTask> task(d->variantCacheQueue.takeFirst());

         /// @todo $revise-texture-animation: prepare all textures in the animation (if animated).
         task->material->prepare(*task->spec);
    }
#endif
}

Materials::Manifest &Materials::toManifest(materialid_t id) const
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

bool Materials::validateUri(Uri const &uri, UriValidationFlags flags, bool quiet) const
{
    LOG_AS("Materials::validateUri");

    if(uri.isEmpty())
    {
        if(!quiet) LOG_MSG("Empty path in material URI \"%s\".") << uri;
        return false;
    }

    if(uri.scheme().isEmpty())
    {
        if(!flags.testFlag(AnyScheme))
        {
            if(!quiet) LOG_MSG("Missing scheme in material URI \"%s\".") << uri;
            return false;
        }
    }
    else if(!knownScheme(uri.scheme()))
    {
        if(!quiet) LOG_MSG("Unknown scheme in material URI \"%s\".") << uri;
        return false;
    }

    return true;
}

Materials::Manifest &Materials::find(Uri const &uri) const
{
    LOG_AS("Materials::find");

    if(!validateUri(uri, AnyScheme, true /*quiet please*/))
        /// @throw NotFoundError Failed to locate a matching manifest.
        throw NotFoundError("Materials::find", "URI \"" + uri.asText() + "\" failed validation");

    // Perform the search.
    String const &path = uri.path();

    // Does the user want a manifest in a specific scheme?
    if(!uri.scheme().isEmpty())
    {
        try
        {
            return scheme(uri.scheme()).find(path);
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
                return scheme->find(path);
            }
            catch(Scheme::NotFoundError const &)
            {} // Ignore this error.
        }
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

Materials::Manifest &Materials::newManifest(Materials::Scheme &scheme, Path const &path)
{
    LOG_AS("Materials::newManifest");

    // Have we already created a manifest for this?
    Manifest *manifest = 0;
    try
    {
        manifest = &find(de::Uri(scheme.name(), path));
    }
    catch(NotFoundError const &)
    {
        // Acquire a new unique identifier for the manifest.
        materialid_t const id = ++d->manifestCount;

        manifest = &scheme.insertManifest(path, id);

        // Add the new manifest to the id index/map.
        if(d->manifestCount > d->manifestIdMapSize)
        {
            // Allocate more memory.
            d->manifestIdMapSize += MANIFESTIDMAP_BLOCK_ALLOC;
            d->manifestIdMap = (Manifest **) M_Realloc(d->manifestIdMap, sizeof *d->manifestIdMap * d->manifestIdMapSize);
        }
        d->manifestIdMap[d->manifestCount - 1] = manifest; /* 1-based index */
    }

    return *manifest;
}

Material *Materials::newFromDef(ded_material_t &def)
{
    LOG_AS("Materials::newFromDef");

    if(!def.uri) return 0;
    de::Uri &uri = reinterpret_cast<de::Uri &>(*def.uri);
    if(uri.isEmpty()) return 0;

    // We require a properly formed uri (but not a urn - this is a path).
    if(!validateUri(uri, 0, (verbose >= 1)))
    {
        LOG_WARNING("Failed creating Material \"%s\" from definition %p, ignoring.") << uri << dintptr(&def);
        return 0;
    }

    // Have we already created a material for this?
    Manifest *manifest = 0;
    try
    {
        manifest = &find(uri);
        if(manifest->hasMaterial())
        {
            LOG_DEBUG("A Material with uri \"%s\" already exists, returning existing.") << uri;
            return &manifest->material();
        }
    }
    catch(NotFoundError const &)
    {} // Ignore for now.

    // Ensure the primary layer has a valid texture reference.
    Texture *tex = 0;
    if(def.layers[0].stageCount.num > 0)
    {
        ded_material_layer_t const &layer = def.layers[0];
        de::Uri *texUri = reinterpret_cast<de::Uri *>(layer.stages[0].texture);
        if(texUri) // Not unused.
        {
            try
            {
                tex = &App_Textures().find(*texUri).texture();
            }
            catch(Textures::NotFoundError const &er)
            {
                // Log but otherwise ignore this error.
                LOG_WARNING(er.asText() + ". Unknown texture \"%s\" in Material \"%s\" (layer %i stage %i), ignoring.")
                    << reinterpret_cast<de::Uri *>(layer.stages[0].texture)
                    << reinterpret_cast<de::Uri *>(def.uri)
                    << 0 << 0;
            }
        }
    }

    if(!tex) return 0;

    // A new manifest is needed?
    if(!manifest)
    {
        manifest = &newManifest(scheme(uri.scheme()), uri.path());
    }
    manifest->setFlags(MaterialManifest::AutoGenerated, CPP_BOOL(def.autoGenerated));
    manifest->setFlags(MaterialManifest::Custom,        tex->flags().testFlag(Texture::Custom));

    /*
     * Create a material for this right away.
     */
    Material *material = new Material(*manifest, &def);

    material->setAudioEnvironment(S_AudioEnvironmentForMaterial(reinterpret_cast<struct uri_s const *>(&uri)));

    // Associate the material with the manifest.
    manifest->setMaterial(material);

    // Include the material in the scheme-agnostic list of instances.
    d->materials.push_back(material);

    // Add (light) decorations to the material.
    // Prefer decorations defined within the material.
    for(int i = 0; i < DED_MAX_MATERIAL_DECORATIONS; ++i)
    {
        ded_material_decoration_t &lightDef = def.decorations[i];

        // Is this valid? (A zero number of stages signifies the last).
        if(!lightDef.stageCount.num) break;

        MaterialDecoration *decor = MaterialDecoration::fromDef(lightDef);
        material->addDecoration(*decor);
    }

    if(!material->decorationCount())
    {
        // Perhaps an oldschool linked decoration definition?
        ded_decor_t *decorDef = Def_GetDecoration(reinterpret_cast<uri_s *>(&uri));
        if(decorDef)
        {
            for(int i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
            {
                ded_decoration_t &lightDef = decorDef->lights[i];
                // Is this valid? (A zero-strength color signifies the last).
                if(V3f_IsZero(lightDef.stage.color)) break;

                MaterialDecoration *decor = MaterialDecoration::fromDef(lightDef);
                material->addDecoration(*decor);
            }
        }
    }

    return material;
}

void Materials::cache(Material &material, MaterialVariantSpec const &spec,
    bool cacheGroups)
{
#ifdef __CLIENT__
    // Already in the queue?
    DENG2_FOR_EACH_CONST(VariantCacheQueue, i, d->variantCacheQueue)
    {
        if(&material == (*i)->material && &spec == (*i)->spec) return;
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
        if(!group->has(material.manifest())) continue;

        foreach(Manifest *manifest, group->all())
        {
            if(!manifest->hasMaterial()) continue;
            if(&manifest->material() == &material) continue; // We've already enqueued this.

            cache(manifest->material(), spec, false /* do not cache groups */);
        }
    }
#else
    DENG2_UNUSED3(material, spec, cacheGroups);
#endif
}

#ifdef __CLIENT__
MaterialVariantSpec const &Materials::variantSpecForContext(
    materialcontext_t mc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
{
    return d->getVariantSpecForContext(mc, flags, border, tClass, tMap, wrapS, wrapT,
                                       minFilter, magFilter, anisoFilter,
                                       mipmapped, gammaCorrection, noStretch, toAlpha);
}
#endif

Materials::ManifestGroup &Materials::createGroup()
{
    // Allocating one by one is inefficient, but it doesn't really matter.
    // The group number is (index + 1)
    int groupNumber = d->groups.count() + 1;
    d->groups.push_back(new ManifestGroup(groupNumber));
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
    d->clearGroups();
}

Materials::All const &Materials::all() const
{
    return d->materials;
}

static void printVariantInfo(MaterialVariant &variant, int variantIdx)
{
    Con_Printf("Variant #%i: Spec:%p\n", variantIdx, de::dintptr(&variant.spec()));

    // Print layer state info:
    int const layerCount = variant.generalCase().layerCount();
    for(int i = 0; i < layerCount; ++i)
    {
        MaterialVariant::LayerState const &l = variant.layer(i);
        Con_Printf("  Layer #%i: Stage:%i Tics:%i\n", i, l.stage, int(l.tics));
    }

    // Print detail layer state info:
    if(variant.generalCase().isDetailed())
    {
        MaterialVariant::LayerState const &l = variant.detailLayer();
        Con_Printf("  DetailLayer #0: Stage:%i Tics:%i\n", l.stage, int(l.tics));
    }

    // Print shine layer state info:
    if(variant.generalCase().isShiny())
    {
        MaterialVariant::LayerState const &l = variant.shineLayer();
        Con_Printf("  ShineLayer #0: Stage:%i Tics:%i\n", l.stage, int(l.tics));
    }

    // Print decoration state info:
    int const decorationCount = variant.generalCase().decorationCount();
    for(int i = 0; i < decorationCount; ++i)
    {
        MaterialVariant::DecorationState const &l = variant.decoration(i);
        Con_Printf("  Decoration #%i: Stage:%i Tics:%i\n", i, l.stage, int(l.tics));
    }
}

static void printMaterialInfo(Material &material)
{
    Materials::Manifest &manifest = material.manifest();
    Uri uri = manifest.composeUri();
    QByteArray path = uri.asText().toUtf8();
    QByteArray sourceDescription = material.manifest().sourceDescription().toUtf8();

    // Print summary:
    Con_Printf("Material \"%s\" [%p] x%u source:%s\n",
               path.constData(), (void *) &material, material.variantCount(),
               sourceDescription.constData());

    if(material.width() <= 0 || material.height() <= 0)
        Con_Printf("Dimensions:unknown (not yet prepared)\n");
    else
        Con_Printf("Dimensions:%d x %d\n", material.width(), material.height());

    // Print synopsis:
    Con_Printf("Drawable:%s EnvClass:\"%s\" Decorated:%s"
               "\nDetailed:%s Glowing:%s Shiny:%s SkyMasked:%s\n",
               material.isDrawable()     ? "yes" : "no",
               material.audioEnvironment() == AEC_UNKNOWN? "N/A" : S_AudioEnvironmentName(material.audioEnvironment()),
               material.isDecorated()    ? "yes" : "no",
               material.isDetailed()     ? "yes" : "no",
               material.hasGlow()        ? "yes" : "no",
               material.isShiny()        ? "yes" : "no",
               material.isSkyMasked()    ? "yes" : "no");

    // Print full layer config:
    Material::Layers const &layers = material.layers();
    for(int i = 0; i < layers.count(); ++i)
    {
        Material::Layer const &lDef = *(layers[i]);
        int const stageCount = lDef.stageCount();

        Con_Printf("Layer #%i (%i %s):\n",
                   i, stageCount, stageCount == 1? "Stage" : "Stages");

        for(int k = 0; k < stageCount; ++k)
        {
            Material::Layer::Stage const &sDef = *(lDef.stages()[k]);
            QByteArray path = sDef.texture? sDef.texture->manifest().composeUri().asText().toUtf8()
                                          : QString("(prev)").toUtf8();

            Con_Printf("  #%i: Texture:\"%s\" Tics:%i (~%.2f)"
                       "\n      Offset:%.2f x %.2f Glow:%.2f (~%.2f)\n",
                       k, path.constData(), sDef.tics, sDef.variance,
                       sDef.texOrigin[0], sDef.texOrigin[1],
                       sDef.glowStrength, sDef.glowStrengthVariance);
        }
    }

    // Print detail layer config:
    if(material.isDetailed())
    {
        Material::DetailLayer const &lDef = material.detailLayer();
        int const stageCount = lDef.stageCount();

        Con_Printf("DetailLayer #0 (%i %s):\n",
                   stageCount, stageCount == 1? "Stage" : "Stages");

        for(int i = 0; i < stageCount; ++i)
        {
            Material::DetailLayer::Stage const &sDef = *(lDef.stages()[i]);
            QByteArray path = sDef.texture? sDef.texture->manifest().composeUri().asText().toUtf8()
                                          : QString("(prev)").toUtf8();

            Con_Printf("  #%i: Texture:\"%s\" Tics:%i (~%.2f)"
                       "\n      Scale:%.2f Strength:%.2f MaxDistance:%.2f\n",
                       i, path.constData(), sDef.tics, sDef.variance,
                       sDef.scale, sDef.strength, sDef.maxDistance);
        }
    }

    // Print shine layer config:
    if(material.isShiny())
    {
        Material::ShineLayer const &lDef = material.shineLayer();
        int const stageCount = lDef.stageCount();

        Con_Printf("ShineLayer #0 (%i %s):\n",
                   stageCount, stageCount == 1? "Stage" : "Stages");

        for(int i = 0; i < stageCount; ++i)
        {
            Material::ShineLayer::Stage const &sDef = *(lDef.stages()[i]);
            QByteArray path = sDef.texture? sDef.texture->manifest().composeUri().asText().toUtf8()
                                          : QString("(prev)").toUtf8();
            QByteArray maskPath = sDef.maskTexture? sDef.maskTexture->manifest().composeUri().asText().toUtf8()
                                                  : QString("(none)").toUtf8();

            Con_Printf("  #%i: Texture:\"%s\" MaskTexture:\"%s\" Tics:%i (~%.2f)"
                       "\n      Shininess:%.2f BlendMode:%s MaskWidth:%.2f MaskHeight:%.2f"
                       "\n      MinColor:(r:%.2f, g:%.2f, b:%.2f)\n",
                       i, path.constData(), maskPath.constData(), sDef.tics, sDef.variance,
                       sDef.shininess, R_NameForBlendMode(sDef.blendMode),
                       sDef.maskDimensions.width(), sDef.maskDimensions.height(),
                       sDef.minColor[CR], sDef.minColor[CG], sDef.minColor[CB]);
        }
    }

    // Print decoration config:
    Material::Decorations const &decorations = material.decorations();
    for(int i = 0; i < decorations.count(); ++i)
    {
        MaterialDecoration const *lDef = decorations[i];

        Con_Printf("Decoration #%i (%i %s):\n", i, lDef->stageCount(),
                   lDef->stageCount() == 1? "Stage" : "Stages");

        for(int k = 0; k < lDef->stageCount(); ++k)
        {
            ded_decorlight_stage_t const *sDef = lDef->stages()[k];

            Con_Printf("  #%i: Tics:%i (~%.2f) Offset:%.2f x %.2f Elevation:%.2f"
                       "\n      Color:(r:%.2f, g:%.2f, g:%.2f) Radius:%.2f HaloRadius:%.2f"
                       "\n      LightLevels:(min:%.2f, max:%.2f)\n",
                       k, sDef->tics, sDef->variance,
                       sDef->pos[0], sDef->pos[1], sDef->elevation,
                       sDef->color[0], sDef->color[1], sDef->color[2],
                       sDef->radius, sDef->haloRadius,
                       sDef->lightLevels[0], sDef->lightLevels[1]);
        }
    }

    if(!material.variantCount()) return;

    Con_PrintRuler();

    // Print variant specs and current animation states:
    int variantIdx = 0;
    foreach(MaterialVariant *variant, material.variants())
    {
        printVariantInfo(*variant, variantIdx);
        ++variantIdx;
    }
}

static void printMaterialSummary(Materials::Manifest &manifest, bool printSchemeName = true)
{
    Uri uri = manifest.composeUri();
    QByteArray path = printSchemeName? uri.asText().toUtf8() : QByteArray::fromPercentEncoding(uri.path().toStringRef().toUtf8());
    QByteArray sourceDescription = manifest.sourceDescription().toUtf8();

    Con_FPrintf(!manifest.hasMaterial()? CPF_LIGHT : CPF_WHITE,
                "%-*s %-6s x%u\n", printSchemeName? 22 : 14, path.constData(),
                sourceDescription.constData(),
                !manifest.hasMaterial()? 0 : manifest.material().variantCount());
}

/**
 * @todo This logic should be implemented in de::PathTree -ds
 */
static QList<Materials::Manifest *> collectMaterialManifests(Materials::Scheme *scheme,
    Path const &path, QList<Materials::Manifest *> *storage = 0)
{
    int count = 0;

    if(scheme)
    {
        // Only consider materials in this scheme.
        PathTreeIterator<Materials::Scheme::Index> iter(scheme->index().leafNodes());
        while(iter.hasNext())
        {
            Materials::Manifest &manifest = iter.next();
            if(!path.isEmpty())
            {
                /// @todo Use PathTree::Node::compare()
                if(!manifest.path().toString().beginsWith(path, Qt::CaseInsensitive)) continue;
            }

            if(storage) // Store mode.
            {
                storage->push_back(&manifest);
            }
            else // Count mode.
            {
                ++count;
            }
        }
    }
    else
    {
        // Consider materials in any scheme.
        foreach(Materials::Scheme *scheme, App_Materials().allSchemes())
        {
            PathTreeIterator<Materials::Scheme::Index> iter(scheme->index().leafNodes());
            while(iter.hasNext())
            {
                Materials::Manifest &manifest = iter.next();
                if(!path.isEmpty())
                {
                    /// @todo Use PathTree::Node::compare()
                    if(!manifest.path().toString().beginsWith(path, Qt::CaseInsensitive)) continue;
                }

                if(storage) // Store mode.
                {
                    storage->push_back(&manifest);
                }
                else // Count mode.
                {
                    ++count;
                }
            }
        }
    }

    if(storage)
    {
        return *storage;
    }

    QList<Materials::Manifest *> result;
    if(count == 0) return result;

#ifdef DENG2_QT_4_7_OR_NEWER
    result.reserve(count);
#endif
    return collectMaterialManifests(scheme, path, &result);
}

/**
 * Decode and then lexicographically compare the two manifest
 * paths, returning @c true if @a is less than @a b.
 */
static bool compareMaterialManifestPathsAssending(Materials::Manifest const *a, Materials::Manifest const *b)
{
    String pathA = QString(QByteArray::fromPercentEncoding(a->path().toUtf8()));
    String pathB = QString(QByteArray::fromPercentEncoding(b->path().toUtf8()));
    return pathA.compareWithoutCase(pathB) < 0;
}

/**
 * @defgroup printMaterialFlags  Print Material Flags
 * @ingroup flags
 */
///@{
#define PTF_TRANSFORM_PATH_NO_SCHEME        0x1 ///< Do not print the scheme.
///@}

#define DEFAULT_PRINTMATERIALFLAGS          0

/**
 * @param scheme    Material subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Material path search term.
 * @param flags     @ref printTextureFlags
 */
static int printMaterials2(Materials::Scheme *scheme, Path const &like, int flags)
{
    QList<Materials::Manifest *> found = collectMaterialManifests(scheme, like);
    if(found.isEmpty()) return 0;

    bool const printSchemeName = !(flags & PTF_TRANSFORM_PATH_NO_SCHEME);

    // Compose a heading.
    String heading = "Known materials";
    if(!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if(!like.isEmpty())
        heading += " like \"" + like + "\"";
    heading += ":\n";

    // Print the result heading.
    Con_FPrintf(CPF_YELLOW, "%s", heading.toUtf8().constData());

    // Print the result index key.
    int numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));

    Con_Printf(" %*s: %-*s source n#\n", numFoundDigits, "idx",
               printSchemeName? 22 : 14, printSchemeName? "scheme:path" : "path");
    Con_PrintRuler();

    // Sort and print the index.
    qSort(found.begin(), found.end(), compareMaterialManifestPathsAssending);
    int idx = 0;
    foreach(Materials::Manifest *manifest, found)
    {
        Con_Printf(" %*i: ", numFoundDigits, idx++);
        printMaterialSummary(*manifest, printSchemeName);
    }

    return found.count();
}

static void printMaterials(de::Uri const &search, int flags = DEFAULT_PRINTMATERIALFLAGS)
{
    Materials &materials = App_Materials();

    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printMaterials2(0/*any scheme*/, search.path(), flags & ~PTF_TRANSFORM_PATH_NO_SCHEME);
        Con_PrintRuler();
    }
    // Print results within only the one scheme?
    else if(materials.knownScheme(search.scheme()))
    {
        printTotal = printMaterials2(&materials.scheme(search.scheme()), search.path(), flags | PTF_TRANSFORM_PATH_NO_SCHEME);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach(Materials::Scheme *scheme, materials.allSchemes())
        {
            int numPrinted = printMaterials2(scheme, search.path(), flags | PTF_TRANSFORM_PATH_NO_SCHEME);
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

/**
 * @param argv  The arguments to be composed. All are assumed to be in non-encoded
 *              representation.
 *
 *              Supported forms (where <> denote keyword component names):
 *               - [0: "<scheme>:<path>"]
 *               - [0: "<scheme>"]              (if @a matchSchemeOnly)
 *               - [0: "<path>"]
 *               - [0: "<scheme>", 1: "<path>"]
 * @param argc  The number of elements in @a argv.
 * @param matchSchemeOnly  @c true= check if the sole argument matches a known scheme.
 */
static de::Uri composeSearchUri(char **argv, int argc, bool matchSchemeOnly = true)
{
    if(argv)
    {
        // [0: <scheme>:<path>] or [0: <scheme>] or [0: <path>].
        if(argc == 1)
        {
            // Try to extract the scheme and encode the rest of the path.
            de::String rawUri = de::String(argv[0]);
            int pos = rawUri.indexOf(':');
            if(pos >= 0)
            {
                de::String scheme = rawUri.left(pos);
                rawUri.remove(0, pos + 1);
                return de::Uri(scheme, QString(QByteArray(rawUri.toUtf8()).toPercentEncoding()));
            }

            // Just a scheme name?
            if(matchSchemeOnly && App_Materials().knownScheme(rawUri))
            {
                return de::Uri().setScheme(rawUri);
            }

            // Just a path.
            return de::Uri(de::Path(QString(QByteArray(rawUri.toUtf8()).toPercentEncoding())));
        }
        // [0: <scheme>, 1: <path>]
        if(argc == 2)
        {
            // Assign the scheme and encode the path.
            return de::Uri(argv[0], QString(QByteArray(argv[1]).toPercentEncoding()));
        }
    }
    return de::Uri();
}

D_CMD(ListMaterials)
{
    DENG2_UNUSED(src);

    de::Materials &materials = App_Materials();
    de::Uri search = composeSearchUri(&argv[1], argc - 1);
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
    de::Uri search = composeSearchUri(&argv[1], argc - 1, false /*don't match schemes*/);
    if(!search.scheme().isEmpty() && !materials.knownScheme(search.scheme()))
    {
        Con_Printf("Unknown scheme '%s'.\n", search.schemeCStr());
        return false;
    }

    try
    {
        de::Materials::Manifest &manifest = materials.find(search);
        if(manifest.hasMaterial())
        {
            de::printMaterialInfo(manifest.material());
        }
        else
        {
            de::printMaterialSummary(manifest);
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

#if _DEBUG
D_CMD(PrintMaterialStats)
{
    DENG2_UNUSED3(src, argc, argv);

    de::Materials &materials = App_Materials();

    Con_FPrintf(CPF_YELLOW, "Material Statistics:\n");
    foreach(de::Materials::Scheme *scheme, materials.allSchemes())
    {
        de::Materials::Scheme::Index const &index = scheme->index();

        uint count = index.count();
        Con_Printf("Scheme: %s (%u %s)\n", scheme->name().toUtf8().constData(), count, count == 1? "material":"materials");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif
