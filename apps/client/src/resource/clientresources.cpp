/** @file clientresources.cpp  Client-side resource subsystem.
 *
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "de_platform.h"
#include "resource/clientresources.h"

#include <de/legacy/memory.h>
#include <de/app.h>
#include <de/byteorder.h>
#include <de/byterefarray.h>
#include <de/directoryfeed.h>
#include <de/dscript.h>
#include <de/hash.h>
#include <de/logbuffer.h>
#include <de/loop.h>
//#include <de/module.h>
#include <de/nativepath.h>
#include <de/packageloader.h>
#include <de/reader.h>
#include <de/stringpool.h>
#include <de/task.h>
#include <de/time.h>

#include <doomsday/console/cmd.h>
#include <doomsday/defs/music.h>
#include <doomsday/defs/sprite.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include <doomsday/filesys/lumpindex.h>
#include <doomsday/res/animgroups.h>
#include <doomsday/res/colorpalettes.h>
#include <doomsday/res/composite.h>
#include <doomsday/res/mapmanifests.h>
#include <doomsday/res/patch.h>
#include <doomsday/res/patchname.h>
#include <doomsday/res/sprites.h>
#include <doomsday/res/texturemanifest.h>
#include <doomsday/res/textures.h>
#include <doomsday/world/material.h>
#include <doomsday/world/materials.h>

#include "def_main.h"
#include "dd_main.h"
#include "dd_def.h"

#include "clientapp.h"
#include "ui/progress.h"
#include "sys_system.h"  // novideo
#include "gl/gl_tex.h"
#include "gl/gl_texmanager.h"
#include "gl/svg.h"
#include "resource/clienttexture.h"
#include "render/rend_model.h"
#include "render/rend_particle.h"  // Rend_ParticleReleaseSystemTextures
#include "render/rendersystem.h"

// For smart caching logics:
#include "network/net_demo.h"  // playback
#include "render/rend_main.h"  // Rend_MapSurfaceMaterialSpec
#include "render/billboard.h"  // Rend_SpriteMaterialSpec
#include "render/skydrawable.h"

#include "world/clientworld.h"
#include "world/map.h"
#include "world/p_object.h"
#include "world/sky.h"
#include "world/surface.h"

#include <doomsday/world/sector.h>
#include <doomsday/world/thinkers.h>

using namespace de;
using namespace res;

/// @c TST_DETAIL type specifications are stored separately into a set of
/// buckets. Bucket selection is determined by their quantized contrast value.
#define DETAILVARIANT_CONTRAST_HASHSIZE     (DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR+1)

// Console variables (globals).
byte precacheMapMaterials = true;
byte precacheSprites      = true;

ClientResources &ClientResources::get() // static
{
    return static_cast<ClientResources &>(Resources::get());
}

DE_PIMPL(ClientResources)
, DE_OBSERVES(FontScheme,         ManifestDefined)
, DE_OBSERVES(FontManifest,       Deletion)
, DE_OBSERVES(AbstractFont,       Deletion)
, DE_OBSERVES(res::ColorPalettes, Addition)
, DE_OBSERVES(res::ColorPalette,  ColorTableChange)
{
    typedef Hash<lumpnum_t, rawtex_t *> RawTextureHash;
    RawTextureHash rawTexHash;

    /// System subspace schemes containing the manifests/resources.
    FontSchemes fontSchemes;
    List<FontScheme *> fontSchemeCreationOrder;

    AllFonts fonts;                    ///< From all schemes.
    uint fontManifestCount;            ///< Total number of font manifests (in all schemes).

    uint fontManifestIdMapSize;
    FontManifest **fontManifestIdMap;  ///< Index with fontid_t-1

    typedef List<FrameModelDef> ModelDefs;
    ModelDefs modefs;
    List<int> stateModefs;          ///< Index to the modefs array.

    typedef StringPool ModelRepository;
    ModelRepository *modelRepository;  ///< Owns FrameModel instances.

    /// A list of specifications for material variants.
    typedef List<MaterialVariantSpec *> MaterialSpecs;
    MaterialSpecs materialSpecs;

    typedef List<TextureVariantSpec *> TextureSpecs;
    TextureSpecs textureSpecs;
    TextureSpecs detailTextureSpecs[DETAILVARIANT_CONTRAST_HASHSIZE];

    struct CacheTask
    {
        virtual ~CacheTask() {}
        virtual void run() = 0;
    };

    /**
     * Stores the arguments for a resource cache work item.
     */
    struct MaterialCacheTask : public CacheTask
    {
        ClientMaterial *material;
        const MaterialVariantSpec *spec; /// Interned context specification.

        MaterialCacheTask(ClientMaterial &resource, const MaterialVariantSpec &contextSpec)
            : CacheTask()
            , material(&resource)
            , spec(&contextSpec)
        {}

        void run()
        {
            // Cache all dependent assets and upload GL textures if necessary.
            material->getAnimator(*spec).cacheAssets();
        }
    };

    /// A FIFO queue of material variant caching tasks.
    /// Implemented as a list because we may need to remove tasks from the queue if
    /// the material is destroyed in the mean time.
    typedef List<CacheTask *> CacheQueue;
    CacheQueue cacheQueue;

    Impl(Public *i)
        : Base(i)
        , fontManifestCount        (0)
        , fontManifestIdMapSize    (0)
        , fontManifestIdMap        (0)
        , modelRepository          (0)
    {
        LOG_AS("ClientResources");

        res::TextureManifest::setTextureConstructor([] (res::TextureManifest &m) -> res::Texture * {
            return new ClientTexture(m);
        });

        /// @note Order here defines the ambigious-URI search order.
        createFontScheme("System");
        createFontScheme("Game");

        self().colorPalettes().audienceForAddition() += this;
    }

    ~Impl()
    {
        self().clearAllFontSchemes();
        clearFontManifests();
        self().clearAllRawTextures();
        self().purgeCacheQueue();

        clearAllTextureSpecs();
        clearMaterialSpecs();

        clearModels();
    }

    inline res::FS1 &fileSys() { return App_FileSystem(); }

    void clearFontManifests()
    {
        fontSchemes.deleteAll();
        fontSchemes.clear();
        fontSchemeCreationOrder.clear();

        // Clear the manifest index/map.
        if (fontManifestIdMap)
        {
            M_Free(fontManifestIdMap); fontManifestIdMap = 0;
            fontManifestIdMapSize = 0;
        }
        fontManifestCount = 0;
    }

    void createFontScheme(const String& name)
    {
        DE_ASSERT(name.length() >= FontScheme::min_name_length);

        // Create a new scheme.
        FontScheme *newScheme = new FontScheme(name);
        fontSchemes.insert(name.lower(), newScheme);
        fontSchemeCreationOrder.append(newScheme);

        // We want notification when a new manifest is defined in this scheme.
        newScheme->audienceForManifestDefined += this;
    }

    void clearRuntimeFonts()
    {
        self().fontScheme("Game").clear();

        self().pruneUnusedTextureSpecs();
    }

    void clearSystemFonts()
    {
        self().fontScheme("System").clear();

        self().pruneUnusedTextureSpecs();
    }

    void clearMaterialSpecs()
    {
        deleteAll(materialSpecs);
        materialSpecs.clear();
    }

    MaterialVariantSpec *findMaterialSpec(const MaterialVariantSpec &tpl,
        bool canCreate)
    {
        for (MaterialVariantSpec *spec : materialSpecs)
        {
            if (spec->compare(tpl)) return spec;
        }

        if (!canCreate) return 0;

        materialSpecs.append(new MaterialVariantSpec(tpl));
        return materialSpecs.back();
    }

    MaterialVariantSpec &getMaterialSpecForContext(MaterialContextId contextId,
                                                   int               flags,
                                                   byte              border,
                                                   int               tClass,
                                                   int               tMap,
                                                   GLenum            wrapS,
                                                   GLenum            wrapT,
                                                   int               minFilter,
                                                   int               magFilter,
                                                   int               anisoFilter,
                                                   bool              mipmapped,
                                                   bool              gammaCorrection,
                                                   bool              noStretch,
                                                   bool              toAlpha)
    {
        static MaterialVariantSpec tpl;

        texturevariantusagecontext_t primaryContext = TC_UNKNOWN;
        switch (contextId)
        {
        case UiContext:         primaryContext = TC_UI;                 break;
        case MapSurfaceContext: primaryContext = TC_MAPSURFACE_DIFFUSE; break;
        case SpriteContext:     primaryContext = TC_SPRITE_DIFFUSE;     break;
        case ModelSkinContext:  primaryContext = TC_MODELSKIN_DIFFUSE;  break;
        case PSpriteContext:    primaryContext = TC_PSPRITE_DIFFUSE;    break;
        case SkySphereContext:  primaryContext = TC_SKYSPHERE_DIFFUSE;  break;

        default: DE_ASSERT_FAIL("Invalid material context ID");
        }

        const TextureVariantSpec &primarySpec =
            self().textureSpec(primaryContext, flags, border, tClass, tMap,
                             wrapS, wrapT, minFilter, magFilter,
                             anisoFilter, mipmapped, gammaCorrection,
                             noStretch, toAlpha);

        // Apply the normalized spec to the template.
        tpl.contextId     = contextId;
        tpl.primarySpec = &primarySpec;

        return *findMaterialSpec(tpl, true);
    }

    static int hashDetailTextureSpec(const detailvariantspecification_t &spec)
    {
        return (spec.contrast * (1/255.f) * DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR + .5f);
    }

    static variantspecification_t &configureTextureSpec(variantspecification_t & spec,
                                                        texturevariantusagecontext_t tc,
                                                        int     flags,
                                                        byte    border,
                                                        int     tClass,
                                                        int     tMap,
                                                        GLenum  wrapS,
                                                        GLenum  wrapT,
                                                        int     minFilter,
                                                        int     magFilter,
                                                        int     anisoFilter,
                                                        dd_bool mipmapped,
                                                        dd_bool gammaCorrection,
                                                        dd_bool noStretch,
                                                        dd_bool toAlpha)
    {
        DE_ASSERT(tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc));

        flags &= ~TSF_INTERNAL_MASK;

        spec.context         = tc;
        spec.flags           = flags;
        spec.border          = (flags & TSF_UPSCALE_AND_SHARPEN)? 1 : border;
        spec.mipmapped       = mipmapped;
        spec.wrapS           = wrapS;
        spec.wrapT           = wrapT;
        spec.minFilter       = de::clamp(-1, minFilter, spec.mipmapped? 3:1);
        spec.magFilter       = de::clamp(-3, magFilter, 1);
        spec.anisoFilter     = de::clamp(-1, anisoFilter, 4);
        spec.gammaCorrection = gammaCorrection;
        spec.noStretch       = noStretch;
        spec.toAlpha         = toAlpha;

        if (tClass || tMap)
        {
            spec.flags      |= TSF_HAS_COLORPALETTE_XLAT;
            spec.tClass      = de::max(0, tClass);
            spec.tMap        = de::max(0, tMap);
        }

        return spec;
    }

    static detailvariantspecification_t &configureDetailTextureSpec(
        detailvariantspecification_t &spec, float contrast)
    {
        const int quantFactor = DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR;

        spec.contrast = 255 * de::clamp<int>(0, contrast * quantFactor + .5f, quantFactor) * (1 / float(quantFactor));
        return spec;
    }

    TextureVariantSpec &linkTextureSpec(TextureVariantSpec *spec)
    {
        DE_ASSERT(spec != 0);

        switch (spec->type)
        {
        case TST_GENERAL:
            textureSpecs.append(spec);
            break;
        case TST_DETAIL: {
            int hash = hashDetailTextureSpec(spec->detailVariant);
            detailTextureSpecs[hash].append(spec);
            break; }
        }

        return *spec;
    }

    TextureVariantSpec *findTextureSpec(const TextureVariantSpec &tpl, bool canCreate)
    {
        // Do we already have a concrete version of the template specification?
        switch (tpl.type)
        {
        case TST_GENERAL: {
            for (TextureVariantSpec *varSpec : textureSpecs)
            {
                if (*varSpec == tpl)
                {
                    return varSpec;
                }
            }
            break; }

        case TST_DETAIL: {
            int hash = hashDetailTextureSpec(tpl.detailVariant);
            for (TextureVariantSpec *varSpec : detailTextureSpecs[hash])
            {
                if (*varSpec == tpl)
                {
                    return varSpec;
                }

            }
            break; }
        }

        // Not found, can we create?
        if (canCreate)
        {
            return &linkTextureSpec(new TextureVariantSpec(tpl));
        }

        return 0;
    }

    TextureVariantSpec *textureSpec(texturevariantusagecontext_t tc,
                                    int     flags,
                                    byte    border,
                                    int     tClass,
                                    int     tMap,
                                    GLenum  wrapS,
                                    GLenum  wrapT,
                                    int     minFilter,
                                    int     magFilter,
                                    int     anisoFilter,
                                    dd_bool mipmapped,
                                    dd_bool gammaCorrection,
                                    dd_bool noStretch,
                                    dd_bool toAlpha)
    {
        static TextureVariantSpec tpl;
        tpl.type = TST_GENERAL;

        configureTextureSpec(tpl.variant, tc, flags, border, tClass, tMap, wrapS,
            wrapT, minFilter, magFilter, anisoFilter, mipmapped, gammaCorrection,
            noStretch, toAlpha);

        // Retrieve a concrete version of the rationalized specification.
        return findTextureSpec(tpl, true);
    }

    TextureVariantSpec *detailTextureSpec(float contrast)
    {
        static TextureVariantSpec tpl;

        tpl.type = TST_DETAIL;
        configureDetailTextureSpec(tpl.detailVariant, contrast);
        return findTextureSpec(tpl, true);
    }

    bool textureSpecInUse(const TextureVariantSpec &spec)
    {
        for (res::Texture *texture : self().textures().allTextures())
        {
            for (TextureVariant *variant : static_cast<ClientTexture *>(texture)->variants())
            {
                if (&variant->spec() == &spec)
                {
                    return true; // Found one; stop.
                }
            }
        }
        return false;
    }

    int pruneUnusedTextureSpecs(TextureSpecs &list)
    {
        int numPruned = 0;
        for (auto i = list.begin(); i != list.end(); )
        {
            TextureVariantSpec *spec = *i;
            if (!textureSpecInUse(*spec))
            {
                i = list.erase(i);
                delete spec;
                numPruned += 1;
            }
            else
            {
                ++i;
            }
        }
        return numPruned;
    }

    int pruneUnusedTextureSpecs(texturevariantspecificationtype_t specType)
    {
        switch (specType)
        {
        case TST_GENERAL: return pruneUnusedTextureSpecs(textureSpecs);
        case TST_DETAIL: {
            int numPruned = 0;
            for (int i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
            {
                numPruned += pruneUnusedTextureSpecs(detailTextureSpecs[i]);
            }
            return numPruned; }
        }
        return 0;
    }

    void clearAllTextureSpecs()
    {
        deleteAll(textureSpecs);
        textureSpecs.clear();

        for (int i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
        {
            deleteAll(detailTextureSpecs[i]);
            detailTextureSpecs[i].clear();
        }
    }

    void processCacheQueue()
    {
        while (!cacheQueue.isEmpty())
        {
            std::unique_ptr<CacheTask> task(cacheQueue.takeFirst());
            task->run();
        }
    }

    void queueCacheTasksForMaterial(ClientMaterial &material,
                                    const MaterialVariantSpec &contextSpec,
                                    bool cacheGroups = true)
    {
        // Already in the queue?
        bool alreadyQueued = false;
        for (CacheTask *baseTask : cacheQueue)
        {
            if (MaterialCacheTask *task = dynamic_cast<MaterialCacheTask *>(baseTask))
            {
                if (&material == task->material && &contextSpec == task->spec)
                {
                    alreadyQueued = true;
                    break;
                }
            }
        }

        if (!alreadyQueued)
        {
            cacheQueue.append(new MaterialCacheTask(material, contextSpec));
        }

        if (!cacheGroups) return;

        // If the material is part of one or more groups enqueue cache tasks
        // for all other materials within the same group(s). Although we could
        // use a flag in the task and have it find the groups come prepare time,
        // this way we can be sure there are no overlapping tasks.
        for (world::Materials::MaterialManifestGroup *group :
                 world::Materials::get().allMaterialGroups())
        {
            if (!group->contains(&material.manifest()))
            {
                continue;
            }

            for (world::MaterialManifest *manifest : *group)
            {
                if (!manifest->hasMaterial()) continue;

                // Have we already enqueued this material?
                if (&manifest->material() == &material) continue;

                queueCacheTasksForMaterial(manifest->material().as<ClientMaterial>(),
                                           contextSpec, false /* do not cache groups */);
            }
        }
    }

    void queueCacheTasksForSprite(spritenum_t id,
                                  const MaterialVariantSpec &contextSpec,
                                  bool cacheGroups = true)
    {
        if (const auto *sprites = self().sprites().tryFindSpriteSet(id))
        {
            for (const auto &sprite : *sprites)
            {
                defn::Sprite const spriteDef(sprite.second);
                for (const auto &view : spriteDef.def().compiled().views)
                {
                    if (auto *material =
                            maybeAs<ClientMaterial>(world::Materials::get().materialPtr(view.uri)))
                    {
                        queueCacheTasksForMaterial(*material, contextSpec, cacheGroups);
                    }
                }
            }
        }
    }

    void queueCacheTasksForModel(FrameModelDef &modelDef)
    {
        if (!useModels) return;

        for (duint sub = 0; sub < modelDef.subCount(); ++sub)
        {
            SubmodelDef &subdef = modelDef.subModelDef(sub);
            FrameModel *mdl = modelForId(subdef.modelId);
            if (!mdl) continue;

            // Load all skins.
            for (const FrameModelSkin &skin : mdl->skins())
            {
                if (ClientTexture *tex = static_cast<ClientTexture *>(skin.texture))
                {
                    tex->prepareVariant(Rend_ModelDiffuseTextureSpec(mdl->flags().testFlag(FrameModel::NoTextureCompression)));
                }
            }

            // Load the shiny skin too.
            if (ClientTexture *shinyTex = static_cast<ClientTexture *>(subdef.shinySkin))
            {
                shinyTex->prepareVariant(Rend_ModelShinyTextureSpec());
            }
        }
    }

    void clearModels()
    {
        /// @todo Why only centralized memory deallocation? Bad (lazy) design...
        modefs.clear();
        stateModefs.clear();

        clearModelList();

        if (modelRepository)
        {
            delete modelRepository; modelRepository = nullptr;
        }
    }

    FrameModel *modelForId(modelid_t id)
    {
        DE_ASSERT(modelRepository);
        return reinterpret_cast<FrameModel *>(modelRepository->userPointer(id));
    }

    inline const String &findModelPath(modelid_t id)
    {
        return modelRepository->stringRef(id);
    }

    /**
     * Create a new modeldef or find an existing one. This is for ID'd models.
     */
    FrameModelDef *getModelDefWithId(const String& id)
    {
        if (id.isEmpty()) return nullptr;

        // First try to find an existing modef.
        if (self().hasModelDef(id))
        {
            return &self().modelDef(id);
        }

        // Get a new entry.
        modefs.append(FrameModelDef(id));
        return &modefs.last();
    }

    /**
     * Create a new modeldef or find an existing one. There can be only one model
     * definition associated with a state/intermark pair.
     */
    FrameModelDef *getModelDef(int state, dfloat interMark, int select)
    {
        // Is this a valid state?
        if (state < 0 || state >= runtimeDefs.states.size())
        {
            return nullptr;
        }

        // First try to find an existing modef.
        for (const FrameModelDef &modef : modefs)
        {
            if (modef.state == &runtimeDefs.states[state] &&
               fequal(modef.interMark, interMark) && modef.select == select)
            {
                // Models are loaded in reverse order; this one already has a model.
                return nullptr;
            }
        }

        modefs.append(FrameModelDef());
        FrameModelDef *md = &modefs.last();

        // Set initial data.
        md->state     = &runtimeDefs.states[state];
        md->interMark = interMark;
        md->select    = select;

        return md;
    }

    String findSkinPath(const Path &skinPath, const Path &modelFilePath)
    {
        //DE_ASSERT(!skinPath.isEmpty());

        // Try the "first choice" directory first.
        if (!modelFilePath.isEmpty())
        {
            // The "first choice" directory is that in which the model file resides.
            try
            {
                return fileSys().findPath(
                    res::Uri("Models",
                             modelFilePath.toString().fileNamePath() / skinPath.fileName()),
                    RLF_DEFAULT,
                    self().resClass(RC_GRAPHIC));
            }
            catch (const FS1::NotFoundError &)
            {}  // Ignore this error.
        }

        /// @throws FS1::NotFoundError if no resource was found.
        return fileSys().findPath(res::Uri("Models", skinPath), RLF_DEFAULT,
                                  self().resClass(RC_GRAPHIC));
    }

    /**
     * Allocate room for a new skin file name.
     */
    short defineSkinAndAddToModelIndex(FrameModel &mdl, const Path &skinPath)
    {
        if (ClientTexture *tex = static_cast<ClientTexture *>(self().textures().defineTexture("ModelSkins", res::Uri(skinPath))))
        {
            // A duplicate? (return existing skin number)
            for (int i = 0; i < mdl.skinCount(); ++i)
            {
                if (mdl.skin(i).texture == tex)
                    return i;
            }

            // Add this new skin.
            mdl.newSkin(skinPath.toString()).texture = tex;
            return mdl.skinCount() - 1;
        }

        return -1;
    }

    void defineAllSkins(FrameModel &mdl)
    {
        const String &modelFilePath = findModelPath(mdl.modelId());

        int numFoundSkins = 0;
        for (int i = 0; i < mdl.skinCount(); ++i)
        {
            FrameModelSkin &skin = mdl.skin(i);
            try
            {
                res::Uri foundResourceUri(Path(findSkinPath(skin.name, modelFilePath)));

                skin.texture = self().textures().defineTexture("ModelSkins", foundResourceUri);

                // We have found one more skin for this model.
                numFoundSkins += 1;
            }
            catch (const FS1::NotFoundError &)
            {
                LOG_RES_VERBOSE("Failed to locate \"%s\" (#%i) for model \"%s\"")
                        << skin.name << i << NativePath(modelFilePath).pretty();
            }
        }

        if (!numFoundSkins)
        {
            // Lastly try a skin named similarly to the model in the same directory.
            res::Uri searchPath(modelFilePath.fileNamePath() / modelFilePath.fileNameWithoutExtension(), RC_GRAPHIC);
            try
            {
                String foundPath = fileSys().findPath(searchPath, RLF_DEFAULT,
                                                      self().resClass(RC_GRAPHIC));
                // Ensure the found path is absolute.
                foundPath = App_BasePath() / foundPath;

                defineSkinAndAddToModelIndex(mdl, foundPath);
                // We have found one more skin for this model.
                numFoundSkins = 1;

                LOG_RES_MSG("Assigned fallback skin \"%s\" to index #0 for model \"%s\"")
                    << NativePath(foundPath).pretty()
                    << NativePath(modelFilePath).pretty();
            }
            catch (const FS1::NotFoundError &)
            {}  // Ignore this error.
        }

        if (!numFoundSkins)
        {
            LOG_RES_MSG("No skins found for model \"%s\" (it may use a custom skin specified in a DED)")
                << NativePath(modelFilePath).pretty();
        }

#ifdef DE_DEBUG
        LOGDEV_RES_XVERBOSE("Model \"%s\" skins:", NativePath(modelFilePath).pretty());
        int skinIdx = 0;
        for (const FrameModelSkin &skin : mdl.skins())
        {
            const res::TextureManifest *texManifest = skin.texture? &skin.texture->manifest() : 0;
            LOGDEV_RES_XVERBOSE("  %i: %s %s",
                       (skinIdx++) << skin.name
                    << (texManifest? (String("\"") + texManifest->composeUri() + "\"") : "(missing texture)")
                    << (texManifest? (String(" => \"") + NativePath(texManifest->resourceUri().compose()).pretty() + "\"") : ""));
        }
#endif
    }

    /**
     * Scales the given model so that it'll be 'destHeight' units tall. Measurements
     * are based on submodel zero. Scale is applied uniformly.
     */
    void scaleModel(FrameModelDef &mf, dfloat destHeight, dfloat offset)
    {
        if (!mf.subCount()) return;

        SubmodelDef &smf = mf.subModelDef(0);

        // No model to scale?
        if (!smf.modelId) return;

        // Find the top and bottom heights.
        dfloat top, bottom;
        dfloat height = self().model(smf.modelId).frame(smf.frame).horizontalRange(&top, &bottom);
        if (fequal(height, 0.f)) height = 1;

        dfloat scale = destHeight / height;

        mf.scale    = Vec3f(scale, scale, scale);
        mf.offset.y = -bottom * scale + offset;
    }

    void scaleModelToSprite(FrameModelDef &mf, const Record *spriteRec)
    {
        if (!spriteRec) return;

        defn::Sprite sprite(*spriteRec);
        if (!sprite.hasView(0)) return;

        world::Material *mat = world::Materials::get().materialPtr(sprite.viewMaterial(0));
        if (!mat) return;

        MaterialAnimator &matAnimator = mat->as<ClientMaterial>().getAnimator(Rend_SpriteMaterialSpec());
        matAnimator.prepare();  // Ensure we have up-to-date info.

        const ClientTexture &texture = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture->base();
        int off = de::max(0, -texture.origin().y - int(matAnimator.dimensions().y));

        scaleModel(mf, matAnimator.dimensions().y, off);
    }

    dfloat calcModelVisualRadius(FrameModelDef *def)
    {
        if (!def || !def->subModelId(0)) return 0;

        // Use the first frame bounds.
        Vec3f min, max;
        dfloat maxRadius = 0;
        for (duint i = 0; i < def->subCount(); ++i)
        {
            if (!def->subModelId(i)) break;

            SubmodelDef &sub = def->subModelDef(i);

            self().model(sub.modelId).frame(sub.frame).bounds(min, max);

            // Half the distance from bottom left to top right.
            dfloat radius = (  def->scale.x * (max.x - min.x)
                             + def->scale.z * (max.z - min.z)) / 3.5f;
            if (radius > maxRadius)
            {
                maxRadius = radius;
            }
        }

        return maxRadius;
    }

    /**
     * Creates a modeldef based on the given DED info. A pretty straightforward
     * operation. No interlinks are set yet. Autoscaling is done and the scale
     * factors set appropriately. After this has been called for all available
     * Model DEDs, each State that has a model will have a pointer to the one
     * with the smallest intermark (start of a chain).
     */
    void setupModel(const defn::Model &def)
    {
        LOG_AS("setupModel");

        auto &defs = *DED_Definitions();

        const int modelScopeFlags = def.geti("flags") | defs.modelFlags;
        const int statenum = defs.getStateNum(def.gets("state"));

        // Is this an ID'd model?
        FrameModelDef *modef = getModelDefWithId(def.gets("id"));
        if (!modef)
        {
            // No, normal State-model.
            if (statenum < 0) return;

            modef = getModelDef(statenum + def.geti("off"), def.getf("interMark"), def.geti("selector"));
            if (!modef) return; // Overridden or invalid definition.
        }

        // Init modef info (state & intermark already set).
        modef->def       = def;
        modef->group     = def.getui("group");
        modef->flags     = modelScopeFlags;
        modef->offset    = Vec3f(def.get("offset"));
        modef->offset.y += defs.modelOffset; // Common Y axis offset.
        modef->scale     = Vec3f(def.get("scale"));
        modef->scale.y  *= defs.modelScale;  // Common Y axis scaling.
        modef->resize    = def.getf("resize");
        modef->skinTics  = de::max(def.geti("skinTics"), 1);
        for (int i = 0; i < 2; ++i)
        {
            modef->interRange[i] = float(def.geta("interRange")[i].asNumber());
        }

        // Submodels.
        modef->clearSubs();
        for (int i = 0; i < def.subCount(); ++i)
        {
            const Record &subdef = def.sub(i);
            SubmodelDef *sub = modef->addSub();

            sub->modelId = 0;

            if (subdef.gets("filename").isEmpty()) continue;

            res::Uri const searchPath(subdef.gets("filename"));
            if (searchPath.isEmpty()) continue;

            try
            {
                String foundPath = fileSys().findPath(searchPath, RLF_DEFAULT,
                                                      self().resClass(RC_MODEL));
                // Ensure the found path is absolute.
                foundPath = App_BasePath() / foundPath;

                // Have we already loaded this?
                modelid_t modelId = modelRepository->intern(foundPath);
                FrameModel *mdl = modelForId(modelId);
                if (!mdl)
                {
                    // Attempt to load it in now.
                    std::unique_ptr<FileHandle> hndl(&fileSys().openFile(foundPath, "rb"));

                    mdl = FrameModel::loadFromFile(*hndl, modelAspectMod);

                    // We're done with the file.
                    fileSys().releaseFile(hndl->file());

                    // Loaded?
                    if (mdl)
                    {
                        // Add it to the repository,
                        mdl->setModelId(modelId);
                        modelRepository->setUserPointer(modelId, mdl);

                        defineAllSkins(*mdl);

                        // Enlarge the vertex buffers in preparation for drawing of this model.
                        if (!Rend_ModelExpandVertexBuffers(mdl->vertexCount()))
                        {
                            LOG_RES_WARNING("Model \"%s\" contains more than %u max vertices (%i), it will not be rendered")
                                << NativePath(foundPath).pretty()
                                << uint(RENDER_MAX_MODEL_VERTS) << mdl->vertexCount();
                        }
                    }
                }

                // Loaded?
                if (!mdl) continue;

                sub->modelId    = mdl->modelId();
                sub->frame      = mdl->frameNumber(subdef.gets("frame"));
                if (sub->frame < 0) sub->frame = 0;
                sub->frameRange = de::max(1, subdef.geti("frameRange")); // Frame range must always be greater than zero.

                sub->alpha      = byte(de::clamp(0, int(255 - subdef.getf("alpha") * 255), 255));
                sub->blendMode  = blendmode_t(subdef.geti("blendMode"));

                // Submodel-specific flags cancel out model-scope flags!
                sub->setFlags(modelScopeFlags ^ subdef.geti("flags"));

                // Flags may override alpha and/or blendmode.
                if (sub->testFlag(MFF_BRIGHTSHADOW))
                {
                    sub->alpha = byte(256 * .80f);
                    sub->blendMode = BM_ADD;
                }
                else if (sub->testFlag(MFF_BRIGHTSHADOW2))
                {
                    sub->blendMode = BM_ADD;
                }
                else if (sub->testFlag(MFF_DARKSHADOW))
                {
                    sub->blendMode = BM_DARK;
                }
                else if (sub->testFlag(MFF_SHADOW2))
                {
                    sub->alpha = byte(256 * .2f);
                }
                else if (sub->testFlag(MFF_SHADOW1))
                {
                    sub->alpha = byte(256 * .62f);
                }

                // Extra blendmodes:
                if (sub->testFlag(MFF_REVERSE_SUBTRACT))
                {
                    sub->blendMode = BM_REVERSE_SUBTRACT;
                }
                else if (sub->testFlag(MFF_SUBTRACT))
                {
                    sub->blendMode = BM_SUBTRACT;
                }

                if (!subdef.gets("skinFilename").isEmpty())
                {
                    // A specific file name has been given for the skin.
                    const String &skinFilePath  = res::Uri(subdef.gets("skinFilename")).path();
                    const String &modelFilePath = findModelPath(sub->modelId);
                    try
                    {
                        Path foundResourcePath(findSkinPath(skinFilePath, modelFilePath));

                        sub->skin = defineSkinAndAddToModelIndex(*mdl, foundResourcePath);
                    }
                    catch (const FS1::NotFoundError &)
                    {
                        LOG_RES_WARNING("Failed to locate skin \"%s\" for model \"%s\"")
                            << subdef.gets("skinFilename") << NativePath(modelFilePath).pretty();
                    }
                }
                else
                {
                    sub->skin = subdef.geti("skin");
                }

                // Skin range must always be greater than zero.
                sub->skinRange = de::max(subdef.geti("skinRange"), 1);

                // Offset within the model.
                sub->offset = subdef.get("offset");

                if (!subdef.gets("shinySkin").isEmpty())
                {
                    const String &skinFilePath  = res::Uri(subdef.gets("shinySkin")).path();
                    const String &modelFilePath = findModelPath(sub->modelId);
                    try
                    {
                        res::Uri foundResourceUri(Path(findSkinPath(skinFilePath, modelFilePath)));

                        sub->shinySkin = self().textures().defineTexture("ModelReflectionSkins", foundResourceUri);
                    }
                    catch (const FS1::NotFoundError &)
                    {
                        LOG_RES_WARNING("Failed to locate skin \"%s\" for model \"%s\"")
                            << skinFilePath << NativePath(modelFilePath).pretty();
                    }
                }
                else
                {
                    sub->shinySkin = 0;
                }

                // Should we allow texture compression with this model?
                if (sub->testFlag(MFF_NO_TEXCOMP))
                {
                    // All skins of this model will no longer use compression.
                    mdl->setFlags(FrameModel::NoTextureCompression);
                }
            }
            catch (const FS1::NotFoundError &)
            {
                LOG_RES_WARNING("Failed to locate \"%s\"") << searchPath;
            }
        }

        // Do scaling, if necessary.
        if (modef->resize)
        {
            scaleModel(*modef, modef->resize, modef->offset.y);
        }
        else if (modef->state && modef->testSubFlag(0, MFF_AUTOSCALE))
        {
            spritenum_t sprNum = DED_Definitions()->getSpriteNum(def.gets("sprite"));
            int sprFrame       = def.geti("spriteFrame");

            if (sprNum < 0)
            {
                // No sprite ID given.
                sprNum   = modef->state->sprite;
                sprFrame = modef->state->frame;
            }

            if (const Record *sprite = self().sprites().spritePtr(sprNum, sprFrame))
            {
                scaleModelToSprite(*modef, sprite);
            }
        }

        if (modef->state)
        {
            int stateNum = runtimeDefs.states.indexOf(modef->state);

            // Associate this modeldef with its state.
            if (stateModefs[stateNum] < 0)
            {
                // No modef; use this.
                stateModefs[stateNum] = self().indexOf(modef);
            }
            else
            {
                // Must check intermark; smallest wins!
                FrameModelDef *other = self().modelDefForState(stateNum);

                if ((modef->interMark <= other->interMark && // Should never be ==
                    modef->select == other->select) || modef->select < other->select) // Smallest selector?
                {
                    stateModefs[stateNum] = self().indexOf(modef);
                }
            }
        }

        // Calculate the particle offset for each submodel.
        Vec3f min, max;
        for (uint i = 0; i < modef->subCount(); ++i)
        {
            SubmodelDef *sub = &modef->subModelDef(i);
            if (sub->modelId && sub->frame >= 0)
            {
                self().model(sub->modelId).frame(sub->frame).bounds(min, max);
                modef->setParticleOffset(i, ((max + min) / 2 + sub->offset) * modef->scale + modef->offset);
            }
        }

        modef->visualRadius = calcModelVisualRadius(modef); // based on geometry bounds

        // Shadow radius can be specified manually.
        modef->shadowRadius = def.getf("shadowRadius");
    }

    void clearModelList()
    {
        if (!modelRepository) return;

        modelRepository->forAll([this] (StringPool::Id id)
        {
            if (auto *model = reinterpret_cast<FrameModel *>(modelRepository->userPointer(id)))
            {
                modelRepository->setUserPointer(id, nullptr);
                delete model;
            }
            return LoopContinue;
        });
    }

    /// Observes FontScheme ManifestDefined.
    void fontSchemeManifestDefined(FontScheme & /*scheme*/, FontManifest &manifest)
    {
        // We want notification when the manifest is derived to produce a resource.
        //manifest.audienceForFontDerived += this;

        // We want notification when the manifest is about to be deleted.
        manifest.audienceForDeletion += this;

        // Acquire a new unique identifier for the manifest.
        const fontid_t id = ++fontManifestCount; // 1-based.
        manifest.setUniqueId(id);

        // Add the new manifest to the id index/map.
        if (fontManifestCount > fontManifestIdMapSize)
        {
            // Allocate more memory.
            fontManifestIdMapSize += 32;
            fontManifestIdMap = (FontManifest **) M_Realloc(
                fontManifestIdMap, sizeof(*fontManifestIdMap) * fontManifestIdMapSize);
        }
        fontManifestIdMap[fontManifestCount - 1] = &manifest;
    }

#if 0
    /// Observes FontManifest FontDerived.
    void fontManifestFontDerived(FontManifest & /*manifest*/, AbstractFont &font)
    {
        // Include this new font in the scheme-agnostic list of instances.
        fonts.append(&font);

        // We want notification when the font is about to be deleted.
        font.audienceForDeletion += this;
    }
#endif

    /// Observes FontManifest Deletion.
    void fontManifestBeingDeleted(const FontManifest &manifest)
    {
        fontManifestIdMap[manifest.uniqueId() - 1 /*1-based*/] = 0;

        // There will soon be one fewer manifest in the system.
        fontManifestCount -= 1;
    }

    /// Observes AbstractFont Deletion.
    void fontBeingDeleted(const AbstractFont &font)
    {
        fonts.removeOne(const_cast<AbstractFont *>(&font));
    }

    void colorPaletteAdded(res::ColorPalette &newPalette)
    {
        // Observe changes to the color table so we can schedule texture updates.
        newPalette.audienceForColorTableChange += this;
    }

    /// Observes ColorPalette ColorTableChange
    void colorPaletteColorTableChanged(res::ColorPalette &colorPalette)
    {
        // Release all GL-textures prepared using @a colorPalette.
        for (res::Texture *texture : self().textures().allTextures())
        {
            colorpalette_analysis_t *cp = reinterpret_cast<colorpalette_analysis_t *>(texture->analysisDataPointer(res::Texture::ColorPaletteAnalysis));
            if (cp && cp->paletteId == colorpaletteid_t(colorPalette.id()))
            {
                texture->release();
            }
        }
    }
};

ClientResources::ClientResources() : d(new Impl(this))
{}

void ClientResources::clear()
{
    Resources::clear();

    R_ShutdownSvgs();
}

void ClientResources::clearAllRuntimeResources()
{
    Resources::clearAllRuntimeResources();

    d->clearRuntimeFonts();
    pruneUnusedTextureSpecs();
}

void ClientResources::clearAllSystemResources()
{
    Resources::clearAllSystemResources();

    d->clearSystemFonts();
    pruneUnusedTextureSpecs();
}

void ClientResources::initSystemTextures()
{
    Resources::initSystemTextures();

    if (novideo) return;

    LOG_AS("ClientResources");

    static struct {
        String const graphicName;
        Path const path;
    } const texDefs[] = {
        { "bbox",       "bbox" },
        { "gray",       "gray" },
        //{ "boxcorner",  "ui/boxcorner" },
        //{ "boxfill",    "ui/boxfill" },
        //{ "boxshade",   "ui/boxshade" }
    };

    LOG_RES_VERBOSE("Initializing System textures...");

    for (const auto &def : texDefs)
    {
        textures().declareSystemTexture(def.path, res::Uri("Graphics", def.graphicName));
    }

    // Define any as yet undefined system textures.
    /// @todo Defer until necessary (manifest texture is first referenced).
    textures().deriveAllTexturesInScheme("System");
}

void ClientResources::reloadAllResources()
{
    DE_ASSERT_IN_MAIN_THREAD();
    LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();

    Resources::reloadAllResources();
    DD_UpdateEngineState();
}

rawtex_t *ClientResources::rawTexture(lumpnum_t lumpNum)
{
    LOG_AS("ClientResources::rawTexture");
    if (-1 == lumpNum || lumpNum >= App_FileSystem().lumpCount())
    {
        LOGDEV_RES_WARNING("LumpNum #%i out of bounds (%i), returning 0")
                << lumpNum << App_FileSystem().lumpCount();
        return nullptr;
    }

    auto found = d->rawTexHash.find(lumpNum);
    return (found != d->rawTexHash.end() ? found->second : nullptr);
}

rawtex_t *ClientResources::declareRawTexture(lumpnum_t lumpNum)
{
    LOG_AS("ClientResources::rawTexture");
    if (-1 == lumpNum || lumpNum >= App_FileSystem().lumpCount())
    {
        LOGDEV_RES_WARNING("LumpNum #%i out of range %s, returning 0")
            << lumpNum << Rangeui(0, App_FileSystem().lumpCount()).asText();
        return nullptr;
    }

    // Has this raw texture already been declared?
    rawtex_t *raw = rawTexture(lumpNum);
    if (!raw)
    {
        // An entirely new raw texture.
        raw = new rawtex_t(App_FileSystem().lump(lumpNum).name(), lumpNum);
        d->rawTexHash.insert(lumpNum, raw);
    }

    return raw;
}

List<rawtex_t *> ClientResources::collectRawTextures() const
{
    return map<List<rawtex_t *>>(
        d->rawTexHash, [](const Impl::RawTextureHash::value_type &v) { return v.second; });
}

void ClientResources::clearAllRawTextures()
{
    d->rawTexHash.deleteAll();
    d->rawTexHash.clear();
}

void ClientResources::releaseAllSystemGLTextures()
{
    if (::novideo) return;

    LOG_AS("ResourceSystem");
    LOG_RES_VERBOSE("Releasing system textures...");

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    ClientApp::render().clearDrawLists();

    GL_ReleaseAllLightingSystemTextures();
    GL_ReleaseAllFlareTextures();

    releaseGLTexturesByScheme("System");
    Rend_ParticleReleaseSystemTextures();
    releaseFontGLTexturesByScheme("System");

    pruneUnusedTextureSpecs();
}

void ClientResources::releaseAllRuntimeGLTextures()
{
    if (::novideo) return;

    LOG_AS("ResourceSystem");
    LOG_RES_VERBOSE("Releasing runtime textures...");

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    ClientApp::render().clearDrawLists();

    // texture-wrapped GL textures; textures, flats, sprites...
    releaseGLTexturesByScheme("Flats");
    releaseGLTexturesByScheme("Textures");
    releaseGLTexturesByScheme("Patches");
    releaseGLTexturesByScheme("Sprites");
    releaseGLTexturesByScheme("Details");
    releaseGLTexturesByScheme("Reflections");
    releaseGLTexturesByScheme("Masks");
    releaseGLTexturesByScheme("ModelSkins");
    releaseGLTexturesByScheme("ModelReflectionSkins");
    releaseGLTexturesByScheme("Lightmaps");
    releaseGLTexturesByScheme("Flaremaps");
    GL_ReleaseTexturesForRawImages();

    Rend_ParticleReleaseExtraTextures();
    releaseFontGLTexturesByScheme("Game");

    pruneUnusedTextureSpecs();
}

void ClientResources::releaseAllGLTextures()
{
    releaseAllRuntimeGLTextures();
    releaseAllSystemGLTextures();
}

void ClientResources::releaseGLTexturesByScheme(const String& schemeName)
{
    if (schemeName.isEmpty()) return;

    PathTreeIterator<res::TextureScheme::Index> iter(textures().textureScheme(schemeName).index().leafNodes());
    while (iter.hasNext())
    {
        res::TextureManifest &manifest = iter.next();
        if (manifest.hasTexture())
        {
            manifest.texture().release();
        }
    }
}

void ClientResources::clearAllTextureSpecs()
{
    d->clearAllTextureSpecs();
}

void ClientResources::pruneUnusedTextureSpecs()
{
    if (Sys_IsShuttingDown()) return;

    int numPruned = 0;
    numPruned += d->pruneUnusedTextureSpecs(TST_GENERAL);
    numPruned += d->pruneUnusedTextureSpecs(TST_DETAIL);

    LOGDEV_RES_VERBOSE("Pruned %i unused texture variant %s")
        << numPruned << (numPruned == 1? "specification" : "specifications");
}

const TextureVariantSpec &ClientResources::textureSpec(texturevariantusagecontext_t tc,
                                                       int    flags,
                                                       byte    border,
                                                       int    tClass,
                                                       int    tMap,
                                                       GLenum  wrapS,
                                                       GLenum  wrapT,
                                                       int    minFilter,
                                                       int    magFilter,
                                                       int    anisoFilter,
                                                       dd_bool mipmapped,
                                                       dd_bool gammaCorrection,
                                                       dd_bool noStretch,
                                                       dd_bool toAlpha)
{
    TextureVariantSpec *tvs =
        d->textureSpec(tc, flags, border, tClass, tMap, wrapS, wrapT, minFilter,
                       magFilter, anisoFilter, mipmapped, gammaCorrection,
                       noStretch, toAlpha);

#ifdef DE_DEBUG
    if (tClass || tMap)
    {
        DE_ASSERT(tvs->variant.flags & TSF_HAS_COLORPALETTE_XLAT);
        DE_ASSERT(tvs->variant.tClass == tClass);
        DE_ASSERT(tvs->variant.tMap == tMap);
    }
#endif

    return *tvs;
}

TextureVariantSpec &ClientResources::detailTextureSpec(dfloat contrast)
{
    return *d->detailTextureSpec(contrast);
}

FontScheme &ClientResources::fontScheme(const String& name) const
{
    LOG_AS("ClientResources::fontScheme");
    if (!name.isEmpty())
    {
        FontSchemes::iterator found = d->fontSchemes.find(name.lower());
        if (found != d->fontSchemes.end()) return *found->second;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw UnknownSchemeError("ClientResources::fontScheme", "No scheme found matching '" + name + "'");
}

bool ClientResources::knownFontScheme(const String& name) const
{
    if (!name.isEmpty())
    {
        return d->fontSchemes.contains(name.lower());
    }
    return false;
}

const ClientResources::FontSchemes &ClientResources::allFontSchemes() const
{
    return d->fontSchemes;
}

bool ClientResources::hasFont(const res::Uri &path) const
{
    try
    {
        fontManifest(path);
        return true;
    }
    catch (const MissingResourceManifestError &)
    {}  // Ignore this error.
    return false;
}

FontManifest &ClientResources::fontManifest(const res::Uri &uri) const
{
    LOG_AS("ClientResources::findFont");

    // Perform the search.
    // Is this a URN? (of the form "urn:schemename:uniqueid")
    if (!uri.scheme().compareWithoutCase("urn"))
    {
        const String pathStr = uri.path();
        auto uIdPos = pathStr.indexOf(':');
        if (uIdPos > 0)
        {
            String schemeName = pathStr.left(uIdPos);
            int uniqueId     = pathStr.substr(uIdPos + 1 /*skip delimiter*/).toInt();

            try
            {
                return fontScheme(schemeName).findByUniqueId(uniqueId);
            }
            catch (const FontScheme::NotFoundError &)
            {}  // Ignore, we'll throw our own...
        }
    }
    else
    {
        // No, this is a URI.
        const String &path = uri.path();

        // Does the user want a manifest in a specific scheme?
        if (!uri.scheme().isEmpty())
        {
            try
            {
                return fontScheme(uri.scheme()).find(path);
            }
            catch (const FontScheme::NotFoundError &)
            {}  // Ignore, we'll throw our own...
        }
        else
        {
            // No, check each scheme in priority order.
            for (FontScheme *scheme : d->fontSchemeCreationOrder)
            {
                try
                {
                    return scheme->find(path);
                }
                catch (const FontScheme::NotFoundError &)
                {}  // Ignore, we'll throw our own...
            }
        }
    }

    /// @throw MissingResourceManifestError  Failed to locate a matching manifest.
    throw MissingResourceManifestError("ClientResources::findFont", "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

FontManifest &ClientResources::toFontManifest(fontid_t id) const
{
    if (id > 0 && id <= d->fontManifestCount)
    {
        duint32 idx = id - 1;  // 1-based index.
        if (d->fontManifestIdMap[idx])
        {
            return *d->fontManifestIdMap[idx];
        }
        DE_ASSERT_FAIL("Bookkeeping error");
    }

    /// @throw UnknownIdError The specified manifest id is invalid.
    throw UnknownFontIdError(
        "ClientResources::toFontManifest",
        stringf("Invalid font ID %u, valid range [1..%u)", id, d->fontManifestCount + 1));
}

const ClientResources::AllFonts &ClientResources::allFonts() const
{
    return d->fonts;
}

AbstractFont *ClientResources::newFontFromDef(const ded_compositefont_t &def)
{
    LOG_AS("ClientResources::newFontFromDef");

    if (!def.uri) return nullptr;
    const res::Uri &uri = *def.uri;

    try
    {
        // Create/retrieve a manifest for the would-be font.
        FontManifest &manifest = declareFont(uri);
        if (manifest.hasResource())
        {
            if (auto *compFont = maybeAs<CompositeBitmapFont>(manifest.resource()))
            {
                /// @todo Do not update fonts here (not enough knowledge). We should
                /// instead return an invalid reference/signal and force the caller
                /// to implement the necessary update logic.
                LOGDEV_RES_XVERBOSE("Font with uri \"%s\" already exists, returning existing",
                                    manifest.composeUri());

                compFont->rebuildFromDef(def);
            }
            return &manifest.resource();
        }

        // A new font.
        manifest.setResource(CompositeBitmapFont::fromDef(manifest, def));
        if (manifest.hasResource())
        {
            if (ClientApp::verbose >= 1)
            {
                LOG_RES_VERBOSE("New font \"%s\"")
                    << manifest.composeUri();
            }
            return &manifest.resource();
        }

        LOG_RES_WARNING("Failed defining new Font for \"%s\"")
            << NativePath(uri.asText()).pretty();
    }
    catch (const UnknownSchemeError &er)
    {
        LOG_RES_WARNING("Failed declaring font \"%s\": %s")
            << NativePath(uri.asText()).pretty() << er.asText();
    }
    catch (const FontScheme::InvalidPathError &er)
    {
        LOG_RES_WARNING("Failed declaring font \"%s\": %s")
            << NativePath(uri.asText()).pretty() << er.asText();
    }

    return nullptr;
}

AbstractFont *ClientResources::newFontFromFile(const res::Uri &uri, const String& filePath)
{
    LOG_AS("ClientResources::newFontFromFile");

    if (!d->fileSys().accessFile(res::Uri::fromNativePath(filePath)))
    {
        LOGDEV_RES_WARNING("Ignoring invalid filePath: ") << filePath;
        return nullptr;
    }

    try
    {
        // Create/retrieve a manifest for the would-be font.
        FontManifest &manifest = declareFont(uri);

        if (manifest.hasResource())
        {
            if (auto *bmapFont = maybeAs<BitmapFont>(manifest.resource()))
            {
                /// @todo Do not update fonts here (not enough knowledge). We should
                /// instead return an invalid reference/signal and force the caller
                /// to implement the necessary update logic.
                LOGDEV_RES_XVERBOSE("Font with uri \"%s\" already exists, returning existing",
                                    manifest.composeUri());

                bmapFont->setFilePath(filePath);
            }
            return &manifest.resource();
        }

        // A new font.
        manifest.setResource(BitmapFont::fromFile(manifest, filePath));
        if (manifest.hasResource())
        {
            if (ClientApp::verbose >= 1)
            {
                LOG_RES_VERBOSE("New font \"%s\"")
                    << manifest.composeUri();
            }
            return &manifest.resource();
        }

        LOG_RES_WARNING("Failed defining new Font for \"%s\"")
            << NativePath(uri.asText()).pretty();
    }
    catch (const UnknownSchemeError &er)
    {
        LOG_RES_WARNING("Failed declaring font \"%s\": %s")
            << NativePath(uri.asText()).pretty() << er.asText();
    }
    catch (const FontScheme::InvalidPathError &er)
    {
        LOG_RES_WARNING("Failed declaring font \"%s\": %s")
            << NativePath(uri.asText()).pretty() << er.asText();
    }

    return nullptr;
}

void ClientResources::releaseFontGLTexturesByScheme(String schemeName)
{
    if (schemeName.isEmpty()) return;

    PathTreeIterator<FontScheme::Index> iter(fontScheme(schemeName).index().leafNodes());
    while (iter.hasNext())
    {
        FontManifest &manifest = iter.next();
        if (manifest.hasResource())
        {
            manifest.resource().glDeinit();
        }
    }
}

FrameModel &ClientResources::model(modelid_t id)
{
    if (FrameModel *model = d->modelForId(id)) return *model;
    /// @throw MissingResourceError An unknown/invalid id was specified.
    throw MissingResourceError("ClientResources::model", "Invalid id " + String::asText(id));
}

bool ClientResources::hasModelDef(String id) const
{
    if (!id.isEmpty())
    {
        for (const FrameModelDef &modef : d->modefs)
        {
            if (!id.compareWithoutCase(modef.id))
            {
                return true;
            }
        }
    }
    return false;
}

FrameModelDef &ClientResources::modelDef(int index)
{
    if (index >= 0 && index < modelDefCount()) return d->modefs[index];
    /// @throw MissingModelDefError An unknown model definition was referenced.
    throw MissingModelDefError("ClientResources::modelDef", "Invalid index #" + String::asText(index) + ", valid range " + Rangeui(0, modelDefCount()).asText());
}

FrameModelDef &ClientResources::modelDef(String id)
{
    if (!id.isEmpty())
    {
        for (const FrameModelDef &modef : d->modefs)
        {
            if (!id.compareWithoutCase(modef.id))
            {
                return const_cast<FrameModelDef &>(modef);
            }
        }
    }
    /// @throw MissingModelDefError An unknown model definition was referenced.
    throw MissingModelDefError("ClientResources::modelDef", "Invalid id '" + id + "'");
}

FrameModelDef *ClientResources::modelDefForState(int stateIndex, int select)
{
    if (stateIndex < 0 || stateIndex >= DED_Definitions()->states.size())
        return nullptr;
    if (stateIndex < 0 || stateIndex >= d->stateModefs.count())
        return nullptr;
    if (d->stateModefs[stateIndex] < 0)
        return nullptr;

    DE_ASSERT(d->stateModefs[stateIndex] >= 0);
    DE_ASSERT(d->stateModefs[stateIndex] < d->modefs.count());

    FrameModelDef *def = &d->modefs[d->stateModefs[stateIndex]];
    if (select)
    {
        // Choose the correct selector, or selector zero if the given one not available.
        const int mosel = (select & DDMOBJ_SELECTOR_MASK);
        for (FrameModelDef *it = def; it; it = it->selectNext)
        {
            if (it->select == mosel)
            {
                return it;
            }
        }
    }

    return def;
}

int ClientResources::modelDefCount() const
{
    return d->modefs.count();
}

void ClientResources::initModels()
{
    LOG_AS("ResourceSystem");

    if (CommandLine_Check("-nomd2"))
    {
        LOG_RES_NOTE("3D models are disabled");
        return;
    }

    LOG_RES_VERBOSE("Initializing Models...");
    Time begunAt;

    d->clearModelList();
    d->modefs.clear();

    delete d->modelRepository;
    d->modelRepository = new StringPool();

    auto &defs = *DED_Definitions();

    // There can't be more modeldefs than there are DED Models.
    d->modefs.resize(defs.models.size());

    // Clear the stateid => modeldef LUT.
    d->stateModefs.resize(runtimeDefs.states.size());
    for (int i = 0; i < runtimeDefs.states.size(); ++i)
    {
        d->stateModefs[i] = -1;
    }

    // Read in the model files and their data.
    // Use the latest definition available for each sprite ID.
    for (int i = int(defs.models.size()) - 1; i >= 0; --i)
    {
        if (!(i % 100))
        {
            // This may take a while, so keep updating the progress.
            Con_SetProgress(130 + 70*(defs.models.size() - i)/defs.models.size());
        }

        d->setupModel(defs.models[i]);
    }

    // Create interlinks. Note that the order in which the defs were loaded
    // is important. We want to allow "patch" definitions, right?

    // For each modeldef we will find the "next" def.
    for (int i = d->modefs.count() - 1; i >= 0; --i)
    {
        FrameModelDef *me = &d->modefs[i];

        dfloat minmark = 2; // max = 1, so this is "out of bounds".

        FrameModelDef *closest = 0;
        for (int k = d->modefs.count() - 1; k >= 0; --k)
        {
            FrameModelDef *other = &d->modefs[k];

            /// @todo Need an index by state. -jk
            if (other->state != me->state) continue;

            // Same state and a bigger order are the requirements.
            if (other->def.order() > me->def.order() && // Defined after me.
                other->interMark > me->interMark &&
                other->interMark < minmark &&
                other->select == me->select)
            {
                minmark = other->interMark;
                closest = other;
            }
        }

        me->interNext = closest;
    }

    // Create selectlinks.
    for (int i = d->modefs.count() - 1; i >= 0; --i)
    {
        FrameModelDef *me = &d->modefs[i];

        int minsel = DDMAXINT;

        FrameModelDef *closest = 0;

        // Start scanning from the next definition.
        for (int k = d->modefs.count() - 1; k >= 0; --k)
        {
            FrameModelDef *other = &d->modefs[k];

            // Same state and a bigger order are the requirements.
            if (other->state == me->state &&
                other->def.order() > me->def.order() && // Defined after me.
                other->select > me->select && other->select < minsel &&
                other->interMark >= me->interMark)
            {
                minsel = other->select;
                closest = other;
            }
        }

        me->selectNext = closest;
    }

    LOG_RES_MSG("Model init completed in %.2f seconds") << begunAt.since();
}

int ClientResources::indexOf(const FrameModelDef *modelDef)
{
    int index = int(modelDef - &d->modefs[0]);
    return (index >= 0 && index < d->modefs.count() ? index : -1);
}

void ClientResources::setModelDefFrame(FrameModelDef &modef, int frame)
{
    for (duint i = 0; i < modef.subCount(); ++i)
    {
        SubmodelDef &subdef = modef.subModelDef(i);
        if (subdef.modelId == NOMODELID) continue;

        // Modify the modeldef itself: set the current frame.
        subdef.frame = frame % model(subdef.modelId).frameCount();
    }
}

void ClientResources::purgeCacheQueue()
{
    deleteAll(d->cacheQueue);
    d->cacheQueue.clear();
}

void ClientResources::processCacheQueue()
{
    d->processCacheQueue();
}

void ClientResources::cache(ClientMaterial &material, const MaterialVariantSpec &spec,
                            bool cacheGroups)
{
    d->queueCacheTasksForMaterial(material, spec, cacheGroups);
}

void ClientResources::cache(spritenum_t spriteId, const MaterialVariantSpec &spec)
{
    d->queueCacheTasksForSprite(spriteId, spec);
}

void ClientResources::cache(FrameModelDef *modelDef)
{
    if (!modelDef) return;
    d->queueCacheTasksForModel(*modelDef);
}

const MaterialVariantSpec &ClientResources::materialSpec(MaterialContextId contextId,
    int flags, byte border, int tClass, int tMap, GLenum wrapS, GLenum wrapT,
    int minFilter, int magFilter, int anisoFilter,
    bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
{
    return d->getMaterialSpecForContext(contextId, flags, border, tClass, tMap,
                                        wrapS, wrapT, minFilter, magFilter, anisoFilter,
                                        mipmapped, gammaCorrection, noStretch, toAlpha);
}

void ClientResources::cacheForCurrentMap()
{
    // Don't precache when playing a demo (why not? -ds).
    if (playback) return;

    world::Map &map = App_World().map();

    if (precacheMapMaterials)
    {
        const MaterialVariantSpec &spec = Rend_MapSurfaceMaterialSpec();

        map.forAllLines([this, &spec] (world::Line &line)
        {
            for (int i = 0; i < 2; ++i)
            {
                auto &side = line.side(i);
                if (!side.hasSections()) continue;

                if (side.middle().hasMaterial())
                    cache(side.middle().material().as<ClientMaterial>(), spec);

                if (side.top().hasMaterial())
                    cache(side.top().material().as<ClientMaterial>(), spec);

                if (side.bottom().hasMaterial())
                    cache(side.bottom().material().as<ClientMaterial>(), spec);
            }
            return LoopContinue;
        });

        map.forAllSectors([this, &spec] (world::Sector &sector)
        {
            // Skip sectors with no line sides as their planes will never be drawn.
            if (sector.sideCount())
            {
                sector.forAllPlanes([this, &spec] (world::Plane &plane)
                {
                    if (plane.surface().hasMaterial())
                    {
                        cache(plane.surface().material().as<ClientMaterial>(), spec);
                    }
                    return LoopContinue;
                });
            }
            return LoopContinue;
        });
    }

    if (precacheSprites)
    {
        const MaterialVariantSpec &matSpec = Rend_SpriteMaterialSpec();

        for (int i = 0; i < sprites().spriteCount(); ++i)
        {
            const auto sprite = spritenum_t(i);

            // Is this sprite used by a state of at least one mobj?
            LoopResult found = map.thinkers().forAll(reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                                                     0x1/*public*/, [&sprite] (thinker_t *th)
            {
                const auto &mob = *reinterpret_cast<mobj_t *>(th);
                if (mob.type >= 0 && mob.type < runtimeDefs.mobjInfo.size())
                {
                    /// @todo optimize: traverses the entire state list!
                    for (int k = 0; k < DED_Definitions()->states.size(); ++k)
                    {
                        if (runtimeDefs.stateInfo[k].owner != &runtimeDefs.mobjInfo[mob.type])
                            continue;

                        if (Def_GetState(k)->sprite == sprite)
                        {
                            return LoopAbort;  // Found one.
                        }
                    }
                }
                return LoopContinue;
            });

            if (found)
            {
                cache(sprite, matSpec);
            }
        }
    }

    // Precache model skins?
    /// @note The skins are also bound here once so they should be ready
    /// for use the next time they are needed.
    if (useModels && precacheSkins)
    {
        map.thinkers().forAll(reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                              0x1/*public*/, [this] (thinker_t *th)
        {
            const auto &mob = *reinterpret_cast<mobj_t *>(th);
            // Check through all the model definitions.
            for (int i = 0; i < modelDefCount(); ++i)
            {
                FrameModelDef &modef = modelDef(i);

                if (!modef.state) continue;
                if (mob.type < 0 || mob.type >= runtimeDefs.mobjInfo.size()) continue; // Hmm?
                if (runtimeDefs.stateInfo[runtimeDefs.states.indexOf(modef.state)].owner != &runtimeDefs.mobjInfo[mob.type]) continue;

                cache(&modef);
            }
            return LoopContinue;
        });
    }
}

/**
 * @param scheme    Resource subspace scheme being printed. Can be @c NULL in
 *                  which case resources are printed from all schemes.
 * @param like      Resource path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printFontIndex2(FontScheme *scheme, const Path &like,
    res::Uri::ComposeAsTextFlags composeUriFlags)
{
    FontScheme::Index::FoundNodes found;
    if (scheme) // Only resources in this scheme.
    {
        scheme->index().findAll(found, res::pathBeginsWithComparator, const_cast<Path *>(&like));
    }
    else // Consider resources in any scheme.
    {
        for (const auto &scheme : App_Resources().allFontSchemes())
        {
            scheme.second->index().findAll(found, res::pathBeginsWithComparator, const_cast<Path *>(&like));
        }
    }
    if (found.isEmpty()) return 0;

    const bool printSchemeName = !(composeUriFlags & res::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known fonts";
    if (!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if (!like.isEmpty())
        heading += " like \"" _E(b) + like.toString() + _E(.) "\"";
    LOG_RES_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index.
    std::sort(found.begin(), found.end(), comparePathTreeNodePathsAscending<FontManifest>);
    //int numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    for (FontManifest *manifest : found)
    {
        String info = Stringf("%31: %s%s" _E(.),
                                     idx,
                                     manifest->hasResource() ? _E(1) : _E(2),
                                     manifest->description(composeUriFlags).c_str());

        LOG_RES_MSG("  " _E(>)) << info;
        idx++;
    }

    return found.count();
}

static void printFontIndex(const res::Uri &search,
    res::Uri::ComposeAsTextFlags flags = res::Uri::DefaultComposeAsTextFlags)
{
    int printTotal = 0;

    // Collate and print results from all schemes?
    if (search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printFontIndex2(0/*any scheme*/, search.path(), flags & ~res::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if (App_Resources().knownFontScheme(search.scheme()))
    {
        printTotal = printFontIndex2(&App_Resources().fontScheme(search.scheme()),
                                     search.path(), flags | res::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        for (const auto &scheme : App_Resources().allFontSchemes())
        {
            int numPrinted = printFontIndex2(scheme.second, search.path(), flags | res::Uri::OmitScheme);
            if (numPrinted)
            {
                LOG_MSG(_E(R));
                printTotal += numPrinted;
            }
        }
    }
    LOG_RES_MSG("Found " _E(b) "%i" _E(.) " %s.")
        << printTotal << (printTotal == 1 ? "font" : "fonts in total");
}

static bool isKnownFontSchemeCallback(const String& name)
{
    return App_Resources().knownFontScheme(name);
}

D_CMD(ListFonts)
{
    DE_UNUSED(src);

    res::Uri search = res::Uri::fromUserInput(&argv[1], argc - 1, &isKnownFontSchemeCallback);
    if (!search.scheme().isEmpty() &&
       !App_Resources().knownFontScheme(search.scheme()))
    {
        LOG_RES_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    printFontIndex(search);
    return true;
}

#ifdef DE_DEBUG
D_CMD(PrintFontStats)
{
    DE_UNUSED(src, argc, argv);

    LOG_MSG(_E(b) "Font Statistics:");
    for (const auto &scheme : App_Resources().allFontSchemes())
    {
        const FontScheme::Index &index = scheme.second->index();

        const uint count = index.count();
        LOG_MSG("Scheme: %s (%u %s)")
            << scheme.second->name() << count << (count == 1? "font" : "fonts");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif // DE_DEBUG

void ClientResources::consoleRegister() // static
{
    Resources::consoleRegister();

    C_CMD("listfonts",      "ss",   ListFonts)
    C_CMD("listfonts",      "s",    ListFonts)
    C_CMD("listfonts",      "",     ListFonts)
#ifdef DE_DEBUG
    C_CMD("fontstats",      NULL,   PrintFontStats)
#endif
}
