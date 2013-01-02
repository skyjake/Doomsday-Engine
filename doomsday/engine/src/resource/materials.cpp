/**
 * @file materials.cpp
 * Materials collection, schemes, bindings and other management. @ingroup resource
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

static void applyVariantSpecification(materialvariantspecification_t &spec, materialcontext_t mc,
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

/// A list of material animation groups.
typedef QList<MaterialAnim> MaterialAnims;

/// A list of specifications for material variants.
typedef QList<materialvariantspecification_t *> VariantSpecs;

/**
 * Stores the arguments for a material variant cache work item.
 */
struct VariantCacheTask
{
    /// The material from which to cache a variant.
    material_t *mat;

    /// Specification of the variant to be cached.
    materialvariantspecification_t const *spec;

    /// @c true= Select the current frame if the material is group-animated.
    bool smooth;

    VariantCacheTask(material_t &_mat, materialvariantspecification_t const &_spec, bool _smooth)
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

    /// Queue of material variants caching tasks.
    VariantCacheQueue variantCacheQueue;

    /// All materials in the system.
    MaterialList materials;

    /// Animation groups.
    MaterialAnims groups;

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

    MaterialAnim *getAnimGroup(int number)
    {
        number -= 1; // 1-based index.
        if(number < 0 || number >= groups.count()) return 0;
        return &groups[number];
    }

    void animateAllGroups()
    {
        DENG2_FOR_EACH(MaterialAnims, i, groups)
        {
            i->animate();
        }
    }

    materialvariantspecification_t *findVariantSpecification(
        materialvariantspecification_t const &tpl, bool canCreate)
    {
        DENG2_FOR_EACH(VariantSpecs, i, variantSpecs)
        {
            if((*i)->compare(tpl)) return *i;
        }

        if(!canCreate) return 0;

        materialvariantspecification_t *spec = new materialvariantspecification_t(tpl);
        variantSpecs.push_back(spec);
        return spec;
    }

    materialvariantspecification_t *getVariantSpecificationForContext(
        materialcontext_t mc, int flags, byte border, int tClass,
        int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
        bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
    {
        static materialvariantspecification_t tpl;

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

        applyVariantSpecification(tpl, mc, primarySpec);
        return findVariantSpecification(tpl, true);
    }

    MaterialBind *getMaterialBindForId(materialid_t id)
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

    createScheme("System");
    createScheme("Flats");
    createScheme("Textures");
    createScheme("Sprites");
}

Materials::~Materials()
{
    clearAnimGroups();
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
    Material_DestroyVariants(&mat);
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
    MaterialBind *mb = d->getMaterialBindForId(id);
    if(!mb) return 0;
    return mb;
}

Materials::Scheme& Materials::createScheme(String name)
{
    DENG_ASSERT(name.length() >= Scheme::min_name_length);

    // Ensure this is a unique name.
    if(knownScheme(name)) return scheme(name);

    // Create a new scheme.
    Scheme* newScheme = new Scheme(name);
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
    material_t *mat = Material_New();
    d->materials.push_back(mat);

    mat->_flags     = def.flags;
    mat->_isCustom  = tex->flags().testFlag(Texture::Custom);
    mat->_def       = &def;
    Size2_SetWidthHeight(mat->_size, MAX_OF(0, def.width), MAX_OF(0, def.height));
    mat->_envClass  = S_MaterialEnvClassForUri(reinterpret_cast<struct uri_s const *>(&uri));

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

void Materials::cache(material_t &mat, materialvariantspecification_t const &spec,
    bool smooth, bool cacheGroup)
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

    if(cacheGroup && Material_IsGroupAnimated(&mat))
    {
        // Material belongs in one or more animgroups; cache the group.
        DENG2_FOR_EACH(MaterialAnims, i, d->groups)
        {
            MaterialAnim &group = *i;
            if(!group.hasFrameForMaterial(mat)) continue;

            DENG2_FOR_EACH_CONST(MaterialAnim::Frames, k, group.allFrames())
            {
                if(&k->material() == &mat) continue;

                cache(k->material(), spec, smooth, false /* do not cache groups */);
            }
        }
    }
}

void Materials::ticker(timespan_t time)
{
#ifdef __CLIENT__
    // The animation will only progress when the game is not paused.
    if(clientPaused) return;
#endif
    if(novideo) return;

    DENG2_FOR_EACH(MaterialList, i, d->materials)
    {
        Material_Ticker(*i, time);
    }

    if(DD_IsSharpTick())
    {
        d->animateAllGroups();
    }
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

MaterialSnapshot const *Materials::prepareVariant(MaterialVariant &variant, bool updateSnapshot)
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

    return snapshot;
}

MaterialSnapshot const *Materials::prepare(material_t &mat, materialvariantspecification_t const &spec,
    bool smooth, bool updateSnapshot)
{
    return prepareVariant(*chooseVariant(mat, spec, smooth, true), updateSnapshot);
}

ded_decor_t const *Materials::decorationDef(material_t &mat)
{
    if(!Material_Prepared(&mat))
    {
        prepare(mat, *Rend_MapSurfaceDiffuseMaterialSpec(), false);
    }
    MaterialBind *mb = d->getMaterialBindForId(Material_PrimaryBind(&mat));
    return mb->decorationDef();
}

ded_ptcgen_t const *Materials::ptcGenDef(material_t &mat)
{
    if(isDedicated) return 0;
    if(!Material_Prepared(&mat))
    {
        prepare(mat, *Rend_MapSurfaceDiffuseMaterialSpec(), false);
    }
    MaterialBind *mb = d->getMaterialBindForId(Material_PrimaryBind(&mat));
    return mb->ptcGenDef();
}

uint Materials::size() const
{
    return d->materials.size();
}

struct materialvariantspecification_s const *Materials::variantSpecificationForContext(
    materialcontext_t mc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
{
    return d->getVariantSpecificationForContext(mc, flags, border, tClass, tMap, wrapS, wrapT,
                                                minFilter, magFilter, anisoFilter,
                                                mipmapped, gammaCorrection, noStretch, toAlpha);
}

typedef struct {
    materialvariantspecification_t const *spec;
    MaterialVariant* chosen;
} choosevariantworker_parameters_t;

static int chooseVariantWorker(struct materialvariant_s *_variant, void *parameters)
{
    MaterialVariant *variant = reinterpret_cast<MaterialVariant *>(_variant);
    choosevariantworker_parameters_t *p = (choosevariantworker_parameters_t *) parameters;
    materialvariantspecification_t const &cand = variant->spec();
    DENG2_ASSERT(p);

    if(cand.compare(*p->spec))
    {
        // This will do fine.
        p->chosen = variant;
        return true; // Stop iteration.
    }
    return false; // Continue iteration.
}

static MaterialVariant *chooseVariant2(material_t &mat, materialvariantspecification_t const &spec)
{
    choosevariantworker_parameters_t params;
    params.spec   = &spec;
    params.chosen = NULL;
    Material_IterateVariants(&mat, chooseVariantWorker, &params);
    return params.chosen;
}

MaterialVariant *Materials::chooseVariant(material_t &mat,
    materialvariantspecification_t const &spec, bool smoothed, bool canCreate)
{
    MaterialVariant* variant = chooseVariant2(mat, spec);
    if(!variant)
    {
        if(!canCreate) return 0;

        variant = new MaterialVariant(mat, spec, *Material_Definition(&mat));
        Material_AddVariant(&mat, reinterpret_cast<materialvariant_s *>(variant));
    }

    if(smoothed)
    {
        variant = variant->translationCurrent();
    }

    return variant;
}

bool Materials::isMaterialInAnimGroup(material_t &mat, int groupNum)
{
    MaterialAnim *group = d->getAnimGroup(groupNum);
    if(!group) return false;
    return group->hasFrameForMaterial(mat);
}

bool Materials::hasDecorations(material_t &mat)
{
    if(novideo) return false;

    /// @todo We should not need to prepare to determine this.
    /// Nor should we need to process the group each time. Cache this decision.
    if(decorationDef(mat)) return true;

    if(Material_IsGroupAnimated(&mat))
    {
        DENG2_FOR_EACH_CONST(MaterialAnims, i, d->groups)
        {
            MaterialAnim const &group = *i;

            // Precache groups don't apply.
            if(group.flags() & AGF_PRECACHE) continue;

            // Is this material in this group?
            if(!group.hasFrameForMaterial(mat)) continue;

            // If any material in this group has decorations then this
            // material is considered to be decorated also.
            DENG2_FOR_EACH_CONST(MaterialAnim::Frames, k, group.allFrames())
            {
                if(decorationDef(k->material())) return true;
            }
        }
    }
    return false;
}

int Materials::animGroupCount()
{
    return d->groups.count();
}

int Materials::newAnimGroup(int flags)
{
    // Allocating one by one is inefficient, but it doesn't really matter.
    // The group id is (index + 1)
    int groupId = d->groups.count() + 1;
    d->groups.push_back(MaterialAnim(groupId, flags));
    return groupId;
}

void Materials::clearAnimGroups()
{
    d->groups.clear();
}

void Materials::addAnimGroupFrame(int groupNum, material_t &mat, int tics, int randomTics)
{
    LOG_AS("Materials::addAnimGroupFrame");

    MaterialAnim *group = d->getAnimGroup(groupNum);
    if(!group)
    {
        LOG_WARNING("Unknown anim group '%i', ignoring.") << groupNum;
        return;
    }

    // Mark the material as being in an animgroup.
    Material_SetGroupAnimated(&mat, true);

    // Allocate a new animframe.
    group->addFrame(mat, tics, randomTics);
}

bool Materials::isPrecacheAnimGroup(int groupNum)
{
    MaterialAnim *group = d->getAnimGroup(groupNum);
    if(!group) return false;
    return ((group->flags() & AGF_PRECACHE) != 0);
}

static int resetVariantGroupAnimWorker(struct materialvariant_s *mat, void* /*parameters*/)
{
    reinterpret_cast<MaterialVariant *>(mat)->resetAnim();
    return 0; // Continue iteration.
}

void Materials::resetAnimGroups()
{
    DENG2_FOR_EACH(MaterialList, i, d->materials)
    {
        Material_IterateVariants(*i, resetVariantGroupAnimWorker, 0);
    }

    DENG2_FOR_EACH(MaterialAnims, i, d->groups)
    {
        i->reset();
    }

    // This'll get every group started on the first step.
    d->animateAllGroups();
}

static void printVariantInfo(MaterialVariant &variant, int variantIdx)
{
    MaterialVariant *next = variant.translationNext();
    int i, layers = Material_LayerCount(&variant.generalCase());

    Con_Printf("Variant #%i: Spec:%p\n", variantIdx, (void *) &variant.spec());

    // Print translation info:
    if(Material_HasTranslation(&variant.generalCase()))
    {
        MaterialVariant *cur = variant.translationCurrent();
        float inter = variant.translationPoint();

        /// @todo kludge: Should not use App_Materials() here.
        QByteArray curPath  = App_Materials()->toMaterialBind(Material_PrimaryBind(&cur->generalCase()))->composeUri().asText().toUtf8();
        QByteArray nextPath = App_Materials()->toMaterialBind(Material_PrimaryBind(&next->generalCase()))->composeUri().asText().toUtf8();

        Con_Printf("  Translation: Current:\"%s\" Next:\"%s\" Inter:%f\n",
                   curPath.constData(), nextPath.constData(), inter);
    }

    // Print layer info:
    for(i = 0; i < layers; ++i)
    {
        MaterialVariant::Layer const &l = variant.layer(i);
        de::Uri uri = reinterpret_cast<Texture &>(*l.texture).manifest().composeUri();
        QByteArray path = uri.asText().toUtf8();

        Con_Printf("  #%i: Stage:%i Tics:%i Texture:\"%s\""
                   "\n      Offset: %.2f x %.2f Glow:%.2f\n",
                   i, l.stage, int(l.tics), path.constData(),
                   l.texOrigin[0], l.texOrigin[1], l.glow);
    }

}

static int printVariantInfoWorker(struct materialvariant_s *variant, void* parameters)
{
    printVariantInfo(reinterpret_cast<MaterialVariant &>(*variant), *(int *)parameters);
    ++(*(int *)parameters);
    return 0; // Continue iteration.
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

    Con_Printf("Layers:%i InGroup:%s Drawable:%s EnvClass:%s"
               "\nDecorated:%s Detailed:%s Glowing:%s Shiny:%s%s SkyMasked:%s\n",
               Material_LayerCount(&mat),
               Material_IsGroupAnimated(&mat)? "yes" : "no",
               Material_IsDrawable(&mat)     ? "yes" : "no",
               Material_EnvironmentClass(&mat) == MEC_UNKNOWN? "N/A" : S_MaterialEnvClassName(Material_EnvironmentClass(&mat)),
               App_Materials()->hasDecorations(mat) ? "yes" : "no",
               Material_DetailTexture(&mat)  ? "yes" : "no",
               Material_HasGlow(&mat)        ? "yes" : "no",
               Material_ShinyTexture(&mat)   ? "yes" : "no",
               Material_ShinyMaskTexture(&mat)? "(masked)" : "",
               Material_IsSkyMasked(&mat)    ? "yes" : "no");

    int variantIdx = 0;
    Material_IterateVariants(&mat, printVariantInfoWorker, (void *)&variantIdx);
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

uint Materials_Size()
{
    return App_Materials()->size();
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

boolean Materials_HasDecorations(material_t *material)
{
    if(!material) return false;
    return App_Materials()->hasDecorations(*material);
}

ded_ptcgen_t const *Materials_PtcGenDef(material_t *material)
{
    if(!material) return 0;
    return App_Materials()->ptcGenDef(*material);
}

boolean Materials_IsMaterialInAnimGroup(material_t *material, int animGroupNum)
{
    if(!material) return false;
    return App_Materials()->isMaterialInAnimGroup(*material, animGroupNum);
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

int Materials_AnimGroupCount()
{
    return App_Materials()->animGroupCount();
}

boolean Materials_IsPrecacheAnimGroup(int animGroupNum)
{
    return App_Materials()->isPrecacheAnimGroup(animGroupNum);
}
