/** @file materials.cpp Material Resource Collection.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cstdlib>
#include <cstring>

#include "de_base.h"
#include "de_console.h"
#include "de_network.h" // playback / clientPaused
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h" // M_NumDigits()
#include "de_audio.h" // For environmental audio properties.

#include <QtAlgorithms>
#include <QList>

#include <de/Error>
#include <de/Log>
#include <de/PathTree>
#include <de/memory.h>

#include "resource/materials.h"
#include "resource/materialsnapshot.h"

/// Number of elements to block-allocate in the material index to materialbind map.
#define MATERIALS_BINDINGMAP_BLOCK_ALLOC (32)

D_CMD(InspectMaterial);
D_CMD(ListMaterials);
#if _DEBUG
D_CMD(PrintMaterialStats);
#endif

namespace de {

Materials::Group::Group(int _id) : id_(_id)
{}

int Materials::Group::id() const
{
    return id_;
}

int Materials::Group::materialCount() const
{
    return materials.count();
}

material_t &Materials::Group::material(int number)
{
    if(number < 0 || number >= materialCount())
    {
        /// @throw InvalidMaterialError Attempt to access an invalid material.
        throw InvalidMaterialError("Materials::Group::material", QString("Invalid material #%1, valid range [0..%2)").arg(number).arg(materialCount()));
    }
    return *materials[number];
}

void Materials::Group::addMaterial(material_t &mat)
{
    LOG_AS("Materials::Group::addMaterial");
    if(hasMaterial(mat)) return;
    materials.push_back(&mat);
}

bool Materials::Group::hasMaterial(material_t &mat) const
{
    return materials.contains(&mat);
}

Materials::Group::Materials const &Materials::Group::allMaterials() const
{
    return materials;
}

static void applyVariantSpec(MaterialVariantSpec &spec, materialcontext_t mc,
    texturevariantspecification_t *primarySpec)
{
    DENG2_ASSERT(mc == MC_UNKNOWN || VALID_MATERIALCONTEXT(mc) && primarySpec);
    spec.context     = mc;
    spec.primarySpec = primarySpec;
}

static void updateMaterialBindInfo(MaterialBind &mb, bool canCreateInfo)
{
    MaterialBind::Info *info = mb.info();
    if(!info)
    {
        if(!canCreateInfo) return;

        // Create new info and attach to this binding.
        info = new MaterialBind::Info();
        mb.attachInfo(*info);
    }

    info->linkDefinitions(mb.material());
}

/// A list of materials.
typedef QList<material_t *> MaterialList;

/// A list of specifications for material variants.
typedef QList<MaterialVariantSpec *> VariantSpecs;

/**
 * Stores the arguments for a material variant cache work item.
 */
struct VariantCacheTask
{
    /// The material from which to cache a variant.
    material_t *mat;

    /// Specification of the variant to be cached.
    MaterialVariantSpec const *spec;

    /// @c true= Select the current frame if the material is group-animated.
    bool smooth;

    VariantCacheTask(material_t &_mat, MaterialVariantSpec const &_spec, bool _smooth)
        : mat(&_mat), spec(&_spec), smooth(_smooth)
    {}
};

/// A FIFO queue of material variant caching tasks.
/// Implemented as a list because we may need to remove tasks from the queue if
/// the material is destroyed in the mean time.
typedef QList<VariantCacheTask *> VariantCacheQueue;

struct Materials::Instance
{
    /// System subspace schemes containing the materials.
    Materials::Schemes schemes;

    /// Material variant specifications.
    VariantSpecs variantSpecs;

    /// Queue of variant cache tasks.
    VariantCacheQueue variantCacheQueue;

    /// All materials in the system.
    MaterialList materials;

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
    /// Animation groups.
    Materials::AnimGroups animGroups;
#endif

    /// Material groups.
    Materials::Groups groups;

    /// Total number of URI material bindings (in all schemes).
    uint bindingCount;

    /// LUT which translates materialid_t => MaterialBind*.
    /// Index with materialid_t-1
    uint bindingIdMapSize;
    MaterialBind **bindingIdMap;

    Instance()
        :  bindingCount(0), bindingIdMapSize(0), bindingIdMap(0)
    {}

    ~Instance()
    {
        clearBindings();
        clearMaterials();
        clearVariantSpecs();
    }

    void clearVariantSpecs()
    {
        while(!variantSpecs.isEmpty())
        {
            delete variantSpecs.takeLast();
        }
    }

    void clearMaterials()
    {
        while(!materials.isEmpty())
        {
            Material_Delete(materials.takeLast());
        }
    }

    void clearBindings()
    {
        DENG2_FOR_EACH(Materials::Schemes, i, schemes)
        {
            delete *i;
        }
        schemes.clear();

        // Clear the binding index/map.
        if(bindingIdMap)
        {
            M_Free(bindingIdMap); bindingIdMap = 0;
            bindingIdMapSize = 0;
        }
        bindingCount = 0;
    }

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
    void animateAllGroups()
    {
        DENG2_FOR_EACH(Materials::AnimGroups, i, animGroups)
        {
            i->animate();
        }
    }
#endif

    MaterialVariantSpec *findVariantSpec(MaterialVariantSpec const &tpl,
                                         bool canCreate)
    {
        DENG2_FOR_EACH(VariantSpecs, i, variantSpecs)
        {
            if((*i)->compare(tpl)) return *i;
        }

        if(!canCreate) return 0;

        MaterialVariantSpec *spec = new MaterialVariantSpec(tpl);
        variantSpecs.push_back(spec);
        return spec;
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

        texturevariantspecification_t* primarySpec =
            GL_TextureVariantSpecificationForContext(primaryContext, flags, border,
                                                     tClass, tMap, wrapS, wrapT,
                                                     minFilter, magFilter, anisoFilter,
                                                     mipmapped, gammaCorrection, noStretch,
                                                     toAlpha);

        applyVariantSpec(tpl, mc, primarySpec);
        return *findVariantSpec(tpl, true);
    }

    MaterialBind *materialBindForId(materialid_t id)
    {
        if(0 == id || id > bindingCount) return 0;
        return bindingIdMap[id - 1];
    }
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

Materials::Materials()
{
    LOG_VERBOSE("Initializing Materials collection...");
    d = new Instance();
}

Materials::~Materials()
{
#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
    clearAllAnimGroups();
#endif
    clearAllGroups();
    purgeCacheQueue();
    delete d;
}

bool Materials::knownScheme(String name) const
{
    if(!name.isEmpty())
    {
        DENG2_FOR_EACH(Schemes, i, d->schemes)
        {
            if(!(*i)->name().compareWithoutCase(name))
                return true;
        }
        //Schemes::iterator found = d->schemes.find(name.toLower());
        //if(found != d->schemes.end()) return true;
    }
    return false;
}

Materials::Scheme &Materials::scheme(String name) const
{
    LOG_AS("Materials::scheme");
    DENG2_FOR_EACH(Schemes, i, d->schemes)
    {
        if(!(*i)->name().compareWithoutCase(name))
            return **i;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw Materials::UnknownSchemeError("Materials::scheme", "No scheme found matching '" + name + "'");
}

Materials::Schemes const& Materials::allSchemes() const
{
    return d->schemes;
}

void Materials::clearDefinitionLinks()
{
    DENG2_FOR_EACH(MaterialList, i, d->materials)
    {
        Material_SetDefinition(*i, 0);
    }

    DENG2_FOR_EACH_CONST(Schemes, i, d->schemes)
    {
        PathTreeIterator<MaterialScheme::Index> iter((*i)->index().leafNodes());
        while(iter.hasNext())
        {
            if(MaterialBind::Info *info = iter.next().info())
            {
                info->clearDefinitionLinks();
            }
        }
    }
}

void Materials::rebuild(material_t &mat, ded_material_t *def)
{
    if(!def) return;

    /// @todo We should be able to rebuild the variants.
    Material_ClearVariants(&mat);
    Material_SetDefinition(&mat, def);

    // Update bindings.
    for(uint i = 0; i < d->bindingCount; ++i)
    {
        MaterialBind *mb = d->bindingIdMap[i];
        if(!mb || mb->material() != &mat) continue;

        updateMaterialBindInfo(*mb, false /*do not create, only update if present*/);
    }
}

void Materials::purgeCacheQueue()
{
    d->variantCacheQueue.clear();
}

void Materials::processCacheQueue()
{
    while(!d->variantCacheQueue.isEmpty())
    {
         VariantCacheTask *cacheTask = d->variantCacheQueue.takeFirst();
         prepare(*cacheTask->mat, *cacheTask->spec, cacheTask->smooth);
         delete cacheTask;
    }
}

MaterialBind *Materials::toMaterialBind(materialid_t id)
{
    MaterialBind *mb = d->materialBindForId(id);
    if(!mb) return 0;
    return mb;
}

Materials::Scheme& Materials::createScheme(String name)
{
    DENG_ASSERT(name.length() >= Scheme::min_name_length);

    // Ensure this is a unique name.
    if(knownScheme(name)) return scheme(name);

    // Create a new scheme.
    Scheme *newScheme = new Scheme(name);
    d->schemes.push_back(newScheme);
    return *newScheme;
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

MaterialBind &Materials::find(Uri const &uri) const
{
    LOG_AS("Materials::find");

    if(!validateUri(uri, AnyScheme, true /*quiet please*/))
        /// @throw NotFoundError Failed to locate a matching bind.
        throw NotFoundError("Materials::find", "URI \"" + uri.asText() + "\" failed validation");

    // Perform the search.
    String const &path = uri.path();

    // Does the user want a bind in a specific scheme?
    if(!uri.scheme().isEmpty())
    {
        try
        {
            return scheme(uri.scheme()).find(path);
        }
        catch(MaterialScheme::NotFoundError const &)
        {} // Ignore, we'll throw our own...
    }
    else
    {
        // No, check in each of these schemes (in priority order).
        /// @todo This priorty order should be defined by the user.
        static String const order[] = {
            "Sprites",
            "Textures",
            "Flats",
            ""
        };
        for(int i = 0; !order[i].isEmpty(); ++i)
        {
            try
            {
                return scheme(order[i]).find(path);
            }
            catch(MaterialScheme::NotFoundError const &)
            {} // Ignore this error.
        }
    }

    /// @throw NotFoundError Failed to locate a matching bind.
    throw NotFoundError("Materials::find", "Failed to locate a bind matching \"" + uri.asText() + "\"");
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

MaterialBind &Materials::newBind(MaterialScheme &scheme, Path const &path, material_t *material)
{
    LOG_AS("Materials::newBind");

    // Have we already created a bind for this?
    MaterialBind *bind = 0;
    try
    {
        bind = &find(de::Uri(scheme.name(), path));
    }
    catch(NotFoundError const &)
    {
        // Acquire a new unique identifier for this binding.
        materialid_t const bindId = ++d->bindingCount;

        bind = &scheme.insertBind(path, bindId);
        if(material)
        {
            Material_SetPrimaryBind(material, bindId);
        }

        // Add the new binding to the bindings index/map.
        if(d->bindingCount > d->bindingIdMapSize)
        {
            // Allocate more memory.
            d->bindingIdMapSize += MATERIALS_BINDINGMAP_BLOCK_ALLOC;
            d->bindingIdMap = (MaterialBind **) M_Realloc(d->bindingIdMap, sizeof *d->bindingIdMap * d->bindingIdMapSize);
            if(!d->bindingIdMap) Con_Error("Materials::newBind: Failed on (re)allocation of %lu bytes enlarging MaterialBind map.", (unsigned long) sizeof *d->bindingIdMap * d->bindingIdMapSize);
        }
        d->bindingIdMap[d->bindingCount - 1] = bind; /* 1-based index */
    }

    // (Re)configure the binding.
    bind->setMaterial(material);
    updateMaterialBindInfo(*bind, false/*do not create, only update if present*/);

    return *bind;
}

material_t *Materials::newFromDef(ded_material_t &def)
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
    MaterialBind *bind = 0;
    try
    {
        bind = &find(uri);
        if(bind->material())
        {
            LOG_DEBUG("A Material with uri \"%s\" already exists, returning existing.") << uri;
            return bind->material();
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
                tex = App_Textures()->find(*texUri).texture();
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

    // A new Material.
    Size2Raw dimensions(MAX_OF(0, def.width), MAX_OF(0, def.height));
    material_env_class_t envClass = S_MaterialEnvClassForUri(reinterpret_cast<struct uri_s const *>(&uri));

    material_t *mat = Material_New(def.flags, tex->flags().testFlag(Texture::Custom), &def,
                                   &dimensions, envClass);
    d->materials.push_back(mat);

    if(!bind)
    {
        newBind(scheme(uri.scheme()), uri.path(), mat);
    }
    else
    {
        bind->setMaterial(mat);
    }

    return mat;
}

void Materials::cache(material_t &mat, MaterialVariantSpec const &spec,
    bool smooth, bool cacheGroups)
{
#ifdef __SERVER__
    return;
#endif

#ifdef __CLIENT__
    // Don't cache when playing a demo. (at all???)
    if(isDedicated || playback) return;
#endif

    // Already in the queue?
    DENG2_FOR_EACH_CONST(VariantCacheQueue, i, d->variantCacheQueue)
    {
        if(&mat == (*i)->mat && &spec == (*i)->spec) return;
    }

    VariantCacheTask *newTask = new VariantCacheTask(mat, spec, smooth);
    d->variantCacheQueue.push_back(newTask);

    if(!cacheGroups) return;

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
    // If the material is part of one or more animations; cache all frames.
    if(Material_IsGroupAnimated(&mat))
    {
        DENG2_FOR_EACH(AnimGroups, i, d->animGroups)
        {
            MaterialAnim &anim = *i;
            if(!anim.hasFrameForMaterial(mat)) continue;

            DENG2_FOR_EACH_CONST(MaterialAnim::Frames, k, anim.allFrames())
            {
                if(&k->material() == &mat) continue;

                cache(k->material(), spec, smooth, false /* do not cache groups */);
            }
        }
    }
#endif

    // If the material is part of one or more groups; cache all other
    // materials within the same group(s).
    DENG2_FOR_EACH_CONST(Groups, i, d->groups)
    {
        Group const &group = *i;
        if(!group.hasMaterial(mat)) continue;

        DENG2_FOR_EACH_CONST(Group::Materials, k, group.allMaterials())
        {
            if(*k == &mat) continue;

            cache(**k, spec, smooth, false /* do not cache groups */);
        }
    }
}

void Materials::ticker(timespan_t time)
{
#ifdef __CLIENT__
    // Animations will only progress when the game is not paused.
    if(clientPaused) return;
#endif

    DENG2_FOR_EACH(MaterialList, i, d->materials)
    {
        Material_Ticker(*i, time);
    }

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
    if(DD_IsSharpTick())
    {
        d->animateAllGroups();
    }
#endif
}

static Texture *findTextureByResourceUri(String nameOfScheme, de::Uri const &resourceUri)
{
    if(resourceUri.isEmpty()) return 0;
    try
    {
        return App_Textures()->scheme(nameOfScheme).findByResourceUri(resourceUri).texture();
    }
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.
    return 0;
}

static inline Texture *findDetailTextureForDef(ded_detailtexture_t const &def)
{
    if(!def.detailTex) return 0;
    return findTextureByResourceUri("Details", reinterpret_cast<de::Uri const &>(*def.detailTex));
}

static inline Texture *findShinyTextureForDef(ded_reflection_t const &def)
{
    if(!def.shinyMap) return 0;
    return findTextureByResourceUri("Reflections", reinterpret_cast<de::Uri const &>(*def.shinyMap));
}

static inline Texture *findShinyMaskTextureForDef(ded_reflection_t const &def)
{
    if(!def.maskMap) return 0;
    return findTextureByResourceUri("Masks", reinterpret_cast<de::Uri const &>(*def.maskMap));
}

static void updateMaterialTextureLinks(MaterialBind &mb)
{
    material_t *mat = mb.material();

    // We may need to need to construct and attach the info.
    updateMaterialBindInfo(mb, true /* create if not present */);

    if(!mat) return;

    ded_detailtexture_t const *dtlDef = mb.detailTextureDef();
    Material_SetDetailTexture(mat,  reinterpret_cast<texture_s *>(dtlDef? findDetailTextureForDef(*dtlDef) : NULL));
    Material_SetDetailStrength(mat, (dtlDef? dtlDef->strength : 0));
    Material_SetDetailScale(mat,    (dtlDef? dtlDef->scale : 0));

    ded_reflection_t const *refDef = mb.reflectionDef();
    Material_SetShinyTexture(mat,     reinterpret_cast<texture_s *>(refDef? findShinyTextureForDef(*refDef) : NULL));
    Material_SetShinyMaskTexture(mat, reinterpret_cast<texture_s *>(refDef? findShinyMaskTextureForDef(*refDef) : NULL));
    Material_SetShinyBlendmode(mat,   (refDef? refDef->blendMode : BM_ADD));
    float const black[3] = { 0, 0, 0 };
    Material_SetShinyMinColor(mat,    (refDef? refDef->minColor : black));
    Material_SetShinyStrength(mat,    (refDef? refDef->shininess : 0));
}

void Materials::updateTextureLinks(MaterialBind &bind)
{
    updateMaterialTextureLinks(bind);
}

MaterialSnapshot const &Materials::prepare(MaterialVariant &variant,
    bool updateSnapshot)
{
    // Acquire the snapshot we are interested in.
    MaterialSnapshot *snapshot = variant.snapshot();
    if(!snapshot)
    {
        // Time to allocate the snapshot.
        snapshot = new MaterialSnapshot(variant);
        variant.attachSnapshot(*snapshot);

        // Update the snapshot right away.
        updateSnapshot = true;
    }
    else if(variant.snapshotPrepareFrame() != frameCount)
    {
        // Time to update the snapshot.
        updateSnapshot = true;
    }

    // We have work to do?
    if(updateSnapshot)
    {
        variant.setSnapshotPrepareFrame(frameCount);
        snapshot->update();
    }

    return *snapshot;
}

ded_decor_t const *Materials::decorationDef(material_t &mat)
{
    if(!Material_Prepared(&mat))
    {
        prepare(mat, Rend_MapSurfaceMaterialSpec(), false);
    }
    return d->materialBindForId(Material_PrimaryBind(&mat))->decorationDef();
}

ded_ptcgen_t const *Materials::ptcGenDef(material_t &mat)
{
    if(isDedicated) return 0;
    if(!Material_Prepared(&mat))
    {
        prepare(mat, Rend_MapSurfaceMaterialSpec(), false);
    }
    return d->materialBindForId(Material_PrimaryBind(&mat))->ptcGenDef();
}

uint Materials::size() const
{
    return d->materials.size();
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

int Materials::newGroup()
{
    // Allocating one by one is inefficient, but it doesn't really matter.
    // The group id is (index + 1)
    int groupId = d->groups.count() + 1;
    d->groups.push_back(Group(groupId));
    return groupId;
}

Materials::Group &Materials::group(int number) const
{
    number -= 1; // 1-based index.
    if(number >= 0 && number < d->groups.count())
    {
        return d->groups[number];
    }

    /// @throw UnknownGroupError An unknown scheme was referenced.
    throw UnknownGroupError("Materials::group", QString("Invalid group number #%1, valid range [0..%2)")
                                                    .arg(number).arg(d->groups.count()));
}

Materials::Groups const &Materials::allGroups() const
{
    return d->groups;
}

void Materials::clearAllGroups()
{
    d->groups.clear();
}

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
int Materials::newAnimGroup(int flags)
{
    // Allocating one by one is inefficient, but it doesn't really matter.
    // The group id is (index + 1)
    int groupId = d->animGroups.count() + 1;
    d->animGroups.push_back(MaterialAnim(groupId, flags));
    return groupId;
}

void Materials::clearAllAnimGroups()
{
    d->animGroups.clear();
}

Materials::AnimGroups const &Materials::allAnimGroups() const
{
    return d->animGroups;
}

MaterialAnim &Materials::animGroup(int number) const
{
    number -= 1; // 1-based index.
    if(number >= 0 && number < d->animGroups.count())
    {
        return d->animGroups[number];
    }

    /// @throw UnknownAnimGroupError An unknown scheme was referenced.
    throw UnknownAnimGroupError("Materials::animGroup", QString("Invalid anim group number #%1, valid range [0..%2)")
                                                            .arg(number).arg(d->animGroups.count()));
}
#endif

void Materials::resetAllMaterialAnimations()
{
    DENG2_FOR_EACH(MaterialList, i, d->materials)
    {
        Material::Variants const &variants = Material_Variants(*i);
        DENG2_FOR_EACH_CONST(Material::Variants, k, variants)
        {
            (*k)->resetAnim();
        }
    }

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
    DENG2_FOR_EACH(AnimGroups, i, d->animGroups)
    {
        i->reset();
    }

    // This'll get every group started on the first step.
    d->animateAllGroups();
#endif
}

static void printVariantInfo(MaterialVariant &variant, int variantIdx)
{
    Con_Printf("Variant #%i: Spec:%p\n", variantIdx, (void *) &variant.spec());

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
    // Print translation info:
    if(Material_HasTranslation(&variant.generalCase()))
    {
        MaterialVariant *cur  = variant.translationCurrent();
        MaterialVariant *next = variant.translationNext();
        float inter = variant.translationPoint();

        /// @todo kludge: Should not use App_Materials() here.
        QByteArray curPath  = App_Materials()->toMaterialBind(Material_PrimaryBind(&cur->generalCase()))->composeUri().asText().toUtf8();
        QByteArray nextPath = App_Materials()->toMaterialBind(Material_PrimaryBind(&next->generalCase()))->composeUri().asText().toUtf8();

        Con_Printf("  Translation: Current:\"%s\" Next:\"%s\" Inter:%f\n",
                   curPath.constData(), nextPath.constData(), inter);
    }
#endif

    // Print layer info:
    int const layerCount = Material_LayerCount(&variant.generalCase());
    for(int i = 0; i < layerCount; ++i)
    {
        MaterialVariant::LayerState const &l = variant.layer(i);
        de::Uri uri = reinterpret_cast<Texture &>(*l.texture).manifest().composeUri();
        QByteArray path = uri.asText().toUtf8();

        Con_Printf("  #%i: Stage:%i Tics:%i Texture:\"%s\""
                   "\n      Offset: %.2f x %.2f Glow:%.2f\n",
                   i, l.stage, int(l.tics), path.constData(),
                   l.texOrigin[0], l.texOrigin[1], l.glowStrength);
    }

}

static void printMaterialInfo(material_t &mat)
{
    /// @todo kludge: Should not use App_Materials() here.
    Uri uri = App_Materials()->toMaterialBind(Material_PrimaryBind(&mat))->composeUri();
    QByteArray path = uri.asText().toUtf8();

    Con_Printf("Material \"%s\" [%p] x%u origin:%s\n",
               path.constData(), (void *) &mat, Material_VariantCount(&mat),
               !Material_IsCustom(&mat)? "game" : (Material_Definition(&mat)->autoGenerated? "addon" : "def"));

    if(Material_Width(&mat) <= 0 || Material_Height(&mat) <= 0)
        Con_Printf("Dimensions: unknown (not yet prepared)\n");
    else
        Con_Printf("Dimensions: %d x %d\n", Material_Width(&mat), Material_Height(&mat));

    Con_Printf("Layers:%i Drawable:%s EnvClass:%s Decorated:%s"
               "\nDetailed:%s Glowing:%s Shiny:%s%s SkyMasked:%s\n",
               Material_LayerCount(&mat),
               Material_IsDrawable(&mat)     ? "yes" : "no",
               Material_EnvironmentClass(&mat) == MEC_UNKNOWN? "N/A" : S_MaterialEnvClassName(Material_EnvironmentClass(&mat)),
               Material_HasDecorations(&mat) ? "yes" : "no",
               Material_DetailTexture(&mat)  ? "yes" : "no",
               Material_HasGlow(&mat)        ? "yes" : "no",
               Material_ShinyTexture(&mat)   ? "yes" : "no",
               Material_ShinyMaskTexture(&mat)? "(masked)" : "",
               Material_IsSkyMasked(&mat)    ? "yes" : "no");

    int variantIdx = 0;
    Material::Variants const &variants = Material_Variants(&mat);
    DENG2_FOR_EACH_CONST(Material::Variants, i, variants)
    {
        printVariantInfo(**i, variantIdx);
        ++variantIdx;
    }
}

static void printMaterialSummary(MaterialBind &bind, bool printSchemeName = true)
{
    material_t *material = bind.material();
    Uri uri = bind.composeUri();
    QByteArray path = printSchemeName? uri.asText().toUtf8() : QByteArray::fromPercentEncoding(uri.path().toStringRef().toUtf8());

    Con_FPrintf(!material? CPF_LIGHT : CPF_WHITE,
                "%-*s %-6s x%u\n", printSchemeName? 22 : 14, path.constData(),
                !material? "unknown" : (!Material_IsCustom(material) ? "game" : (Material_Definition(material)->autoGenerated? "addon" : "def")),
                !material? 0 : Material_VariantCount(material));
}

/**
 * @todo This logic should be implemented in de::PathTree -ds
 */
static QList<MaterialBind *> collectMaterialBinds(MaterialScheme *scheme,
    Path const &path, QList<MaterialBind *> *storage = 0)
{
    int count = 0;

    if(scheme)
    {
        // Only consider materials in this scheme.
        PathTreeIterator<MaterialScheme::Index> iter(scheme->index().leafNodes());
        while(iter.hasNext())
        {
            MaterialBind &bind = iter.next();
            if(!path.isEmpty())
            {
                /// @todo Use PathTree::Node::compare()
                if(!bind.path().toString().beginsWith(path, Qt::CaseInsensitive)) continue;
            }

            if(storage) // Store mode.
            {
                storage->push_back(&bind);
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
        Materials::Schemes const &schemes = App_Materials()->allSchemes();
        DENG2_FOR_EACH_CONST(Materials::Schemes, i, schemes)
        {
            PathTreeIterator<MaterialScheme::Index> iter((*i)->index().leafNodes());
            while(iter.hasNext())
            {
                MaterialBind &bind = iter.next();
                if(!path.isEmpty())
                {
                    /// @todo Use PathTree::Node::compare()
                    if(!bind.path().toString().beginsWith(path, Qt::CaseInsensitive)) continue;
                }

                if(storage) // Store mode.
                {
                    storage->push_back(&bind);
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

    QList<MaterialBind *> result;
    if(count == 0) return result;

#ifdef DENG2_QT_4_7_OR_NEWER
    result.reserve(count);
#endif
    return collectMaterialBinds(scheme, path, &result);
}

/**
 * Decode and then lexicographically compare the two manifest
 * paths, returning @c true if @a is less than @a b.
 */
static bool compareMaterialBindPathsAssending(MaterialBind const *a, MaterialBind const *b)
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
static int printMaterials2(MaterialScheme *scheme, Path const &like, int flags)
{
    QList<MaterialBind *> found = collectMaterialBinds(scheme, like);
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
    int numFoundDigits = MAX_OF(3/*idx*/, M_NumDigits(found.count()));

    Con_Printf(" %*s: %-*s origin n#\n", numFoundDigits, "idx",
               printSchemeName? 22 : 14, printSchemeName? "scheme:path" : "path");
    Con_PrintRuler();

    // Sort and print the index.
    qSort(found.begin(), found.end(), compareMaterialBindPathsAssending);
    int idx = 0;
    DENG2_FOR_EACH(QList<MaterialBind *>, i, found)
    {
        Con_Printf(" %*i: ", numFoundDigits, idx++);
        printMaterialSummary(**i, printSchemeName);
    }

    return found.count();
}

static void printMaterials(de::Uri const &search, int flags = DEFAULT_PRINTMATERIALFLAGS)
{
    Materials &materials = *App_Materials();

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
        Materials::Schemes const &schemes = materials.allSchemes();
        DENG2_FOR_EACH_CONST(Materials::Schemes, i, schemes)
        {
            int numPrinted = printMaterials2(*i, search.path(), flags | PTF_TRANSFORM_PATH_NO_SCHEME);
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
            if(matchSchemeOnly && App_Materials()->knownScheme(rawUri))
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

    de::Materials &materials = *App_Materials();
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

    de::Materials &materials = *App_Materials();
    de::Uri search = composeSearchUri(&argv[1], argc - 1, false /*don't match schemes*/);
    if(!search.scheme().isEmpty() && !materials.knownScheme(search.scheme()))
    {
        Con_Printf("Unknown scheme '%s'.\n", search.schemeCStr());
        return false;
    }

    try
    {
        de::MaterialBind &manifest = materials.find(search);
        if(material_t *mat = manifest.material())
        {
            de::printMaterialInfo(*mat);
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
    DENG2_UNUSED(src); DENG2_UNUSED(argc); DENG2_UNUSED(argv);

    de::Materials &materials = *App_Materials();

    Con_FPrintf(CPF_YELLOW, "Material Statistics:\n");
    de::Materials::Schemes const &schemes = materials.allSchemes();
    DENG2_FOR_EACH_CONST(de::Materials::Schemes, i, schemes)
    {
        de::Materials::Scheme &scheme = **i;
        de::MaterialScheme::Index const &index = scheme.index();

        uint count = index.count();
        Con_Printf("Scheme: %s (%u %s)\n", scheme.name().toUtf8().constData(), count, count == 1? "material":"materials");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif

/*
 * C wrapper API:
 */

static de::Materials *materials;

de::Materials *App_Materials()
{
    if(!materials) throw de::Error("App_Materials", "Materials collection not yet initialized");
    return materials;
}

void Materials_Init()
{
    DENG_ASSERT(!materials);
    materials = new de::Materials();

    materials->createScheme("System");
    materials->createScheme("Flats");
    materials->createScheme("Textures");
    materials->createScheme("Sprites");
}

void Materials_Shutdown()
{
    if(!materials) return;
    delete materials; materials = 0;
}

void Materials_Ticker(timespan_t elapsed)
{
    App_Materials()->ticker(elapsed);
}

uint Materials_Count()
{
    return App_Materials()->count();
}

materialid_t Materials_Id(material_t *material)
{
    if(!material) return NOMATERIALID;
    return Material_PrimaryBind(material);
}

material_t *Materials_ToMaterial(materialid_t materialId)
{
    de::MaterialBind *bind = App_Materials()->toMaterialBind(materialId);
    if(bind) return bind->material();
    return 0;
}

#undef Materials_ComposeUri
DENG_EXTERN_C struct uri_s *Materials_ComposeUri(materialid_t materialId)
{
    de::MaterialBind *bind = App_Materials()->toMaterialBind(materialId);
    if(bind)
    {
        de::Uri uri = bind->composeUri();
        return Uri_Dup(reinterpret_cast<uri_s *>(&uri));
    }
    return Uri_New();
}

#undef Materials_ResolveUri
DENG_EXTERN_C materialid_t Materials_ResolveUri(struct uri_s const *uri)
{
    try
    {
        return App_Materials()->find(*reinterpret_cast<de::Uri const *>(uri)).id();
    }
    catch(de::Materials::NotFoundError const &)
    {} // Ignore this error.
    return NOMATERIALID;
}

#undef Materials_ResolveUriCString
DENG_EXTERN_C materialid_t Materials_ResolveUriCString(char const *uriCString)
{
    if(uriCString && uriCString[0])
    {
        try
        {
            return App_Materials()->find(de::Uri(uriCString, RC_NULL)).id();
        }
        catch(de::Materials::NotFoundError const &)
        {} // Ignore this error.
    }
    return NOMATERIALID;
}
