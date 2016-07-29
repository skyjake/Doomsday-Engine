/** @file clientresources.cpp  Client-side resource subsystem.
 *
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QHash>
#include <QVector>
#include <QtAlgorithms>
#include <de/memory.h>
#include <de/stack.h>  /// @todo remove me
#include <de/App>
#include <de/ArrayValue>
#include <de/ByteRefArray>
#include <de/DirectoryFeed>
#include <de/Function>
#include <de/Log>
#include <de/Loop>
#include <de/Module>
#include <de/NumberValue>
#include <de/PackageLoader>
#include <de/Reader>
#include <de/ScriptSystem>
#include <de/Task>
#include <de/TaskPool>
#include <de/Time>
#ifdef __CLIENT__
#  include <de/ByteOrder>
#  include <de/NativePath>
#  include <de/RecordValue>
#  include <de/StringPool>
#endif
#include <doomsday/doomsdayapp.h>
#include <doomsday/console/cmd.h>
#include <doomsday/defs/music.h>
#include <doomsday/defs/sprite.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include <doomsday/filesys/lumpindex.h>
#include <doomsday/res/AnimGroups>
#include <doomsday/res/ColorPalettes>
#include <doomsday/res/Composite>
#include <doomsday/res/MapManifests>
#include <doomsday/res/Patch>
#include <doomsday/res/PatchName>
#include <doomsday/res/Sprites>
#include <doomsday/res/TextureManifest>
#include <doomsday/res/Textures>
#include <doomsday/world/Material>
#include <doomsday/world/Materials>
#include <doomsday/SavedSession>
#include <doomsday/Session>
#include <doomsday/SaveGames>

#ifdef __CLIENT__
#  include "clientapp.h"
#  include "ui/progress.h"
#  include "sys_system.h"  // novideo
#endif
#include "def_main.h"
#include "dd_main.h"
#include "dd_def.h"

#ifdef __CLIENT__
#  include "gl/gl_tex.h"
#  include "gl/gl_texmanager.h"
#  include "gl/svg.h"
#  include "resource/clienttexture.h"
#  include "render/rend_model.h"
#  include "render/rend_particle.h"  // Rend_ParticleReleaseSystemTextures

// For smart caching logics:
#  include "network/net_demo.h"  // playback
#  include "render/rend_main.h"  // Rend_MapSurfaceMaterialSpec
#  include "render/billboard.h"  // Rend_SpriteMaterialSpec
#  include "render/skydrawable.h"

#  include "world/clientserverworld.h"
#  include "world/map.h"
#  include "world/p_object.h"
#  include "world/sky.h"
#  include "world/thinkers.h"
#  include "Sector"
#  include "Surface"
#endif

namespace de {
namespace internal
{
    static QList<File1 *> collectPatchCompositeDefinitionFiles()
    {
        QList<File1 *> result;

        // Precedence order of definitions is defined by id tech1 which processes
        // the TEXTURE1/2 lumps in the following order:
        //
        // (last)TEXTURE2 > (last)TEXTURE1
        LumpIndex const &index  = App_FileSystem().nameIndex();
        lumpnum_t firstTexLump  = App_FileSystem().lumpNumForName("TEXTURE1");
        lumpnum_t secondTexLump = App_FileSystem().lumpNumForName("TEXTURE2");

        // Also process all other lumps named TEXTURE1/2.
        for (dint i = 0; i < index.size(); ++i)
        {
            File1 &file = index[i];

            // Will this be processed anyway?
            if (i == firstTexLump ) continue;
            if (i == secondTexLump) continue;

            String fileName = file.name().fileNameWithoutExtension();
            if (fileName.compareWithoutCase("TEXTURE1") &&
               fileName.compareWithoutCase("TEXTURE2"))
            {
                continue;
            }

            result.append(&file);
        }

        if (firstTexLump >= 0)
        {
            result.append(&index[firstTexLump]);
        }

        if (secondTexLump >= 0)
        {
            result.append(&index[secondTexLump]);
        }

        return result;
    }

    typedef QList<res::Composite *> Composites;
    typedef QList<res::PatchName> PatchNames;

    static PatchNames readPatchNames(File1 &file)
    {
        LOG_AS("readPatchNames");
        PatchNames names;

        if (file.size() < 4)
        {
            LOG_RES_WARNING("File \"%s\" does not appear to be valid PNAMES data")
                    << NativePath(file.composeUri().asText()).pretty();
            return names;
        }

        ByteRefArray lumpData(file.cache(), file.size());
        de::Reader from(lumpData);

        // The data begins with the total number of patch names.
        dint32 numNames;
        from >> numNames;

        // Followed by the names (eight character ASCII strings).
        if (numNames > 0)
        {
            if ((unsigned) numNames > (file.size() - 4) / 8)
            {
                // The data appears to be truncated.
                LOG_RES_WARNING("File \"%s\" appears to be truncated (%u bytes, expected %u)")
                        << NativePath(file.composeUri().asText()).pretty()
                        << file.size() << (numNames * 8 + 4);

                // We'll only read this many names.
                numNames = (file.size() - 4) / 8;
            }

            // Read the names.
            for (int i = 0; i < numNames; ++i)
            {
                res::PatchName name;
                from >> name;
                names.append(name);
            }
        }

        file.unlock();

        return names;
    }

    /**
     * Reads patch composite texture definitions from @a file.
     *
     * @param file           File to be read.
     * @param origIndexBase  Base value for the "original index" logic.
     * @param archiveCount   Will be updated with the total number of definitions
     *                       in the file (which may not necessarily equal the total
     *                       number of definitions which are actually read).
     */
    static Composites readCompositeTextureDefs(File1 &file,
        PatchNames const &patchNames, int origIndexBase, int &archiveCount)
    {
        LOG_AS("readCompositeTextureDefs");

        Composites result; ///< The resulting set of validated definitions.

        // The game data format determines the format of the archived data.
        res::Composite::ArchiveFormat format =
                (gameDataFormat == 0? res::Composite::DoomFormat : res::Composite::StrifeFormat);

        ByteRefArray data(file.cache(), file.size());
        de::Reader reader(data);

        // First is a count of the total number of definitions.
        dint32 definitionCount;
        reader >> definitionCount;

        // Next is directory of offsets to the definitions.
        typedef QMap<dint32, int> Offsets;
        Offsets offsets;
        for (int i = 0; i < definitionCount; ++i)
        {
            dint32 offset;
            reader >> offset;

            // Ensure the offset is within valid range.
            if (offset < 0 || (unsigned) offset < definitionCount * sizeof(offset) ||
               (dsize) offset > reader.source()->size())
            {
                LOG_RES_WARNING("Ignoring definition #%i: invalid offset %i") << i << offset;
            }
            else
            {
                offsets.insert(offset, origIndexBase + i);
            }
        }

        // Seek to each offset and deserialize the definition.
        DENG2_FOR_EACH_CONST(Offsets, i, offsets)
        {
            // Read the next definition.
            reader.setOffset(i.key());
            res::Composite *def = res::Composite::constructFrom(reader, patchNames, format);

            // Attribute the "original index".
            def->setOrigIndex(i.value());

            // If the composite contains at least one known component image it is
            // considered valid and we will therefore produce a Texture for it.
            DENG2_FOR_EACH_CONST(res::Composite::Components, it, def->components())
            {
                if (it->lumpNum() >= 0)
                {
                    // Its valid - include in the result.
                    result.append(def);
                    def = 0;
                    break;
                }
            }

            // Failed to validate? Dump it.
            if (def) delete def;
        }

        file.unlock(); // We have now finished with this file.

        archiveCount = definitionCount;
        return result;
    }

#ifdef __CLIENT__
    static int hashDetailTextureSpec(detailvariantspecification_t const &spec)
    {
        return (spec.contrast * (1/255.f) * DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR + .5f);
    }

    static variantspecification_t &configureTextureSpec(variantspecification_t &spec,
        texturevariantusagecontext_t tc, int flags, byte border, int tClass, int tMap,
        int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
        dd_bool mipmapped, dd_bool gammaCorrection, dd_bool noStretch, dd_bool toAlpha)
    {
        DENG2_ASSERT(tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc));

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
        int const quantFactor = DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR;

        spec.contrast = 255 * de::clamp<int>(0, contrast * quantFactor + .5f, quantFactor) * (1 / float(quantFactor));
        return spec;
    }

#endif // __CLIENT__

} // namespace internal
} // namespace de

#ifdef __CLIENT__
/// @c TST_DETAIL type specifications are stored separately into a set of
/// buckets. Bucket selection is determined by their quantized contrast value.
#define DETAILVARIANT_CONTRAST_HASHSIZE     (DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR+1)
#endif

using namespace de;
using namespace internal;

DENG2_PIMPL(ClientResources)
#ifdef __CLIENT__
, DENG2_OBSERVES(FontScheme,        ManifestDefined)
, DENG2_OBSERVES(FontManifest,      Deletion)
, DENG2_OBSERVES(AbstractFont,      Deletion)
, DENG2_OBSERVES(res::ColorPalette, ColorTableChange)
#endif
{
#ifdef __CLIENT__
    typedef QHash<lumpnum_t, rawtex_t *> RawTextureHash;
    RawTextureHash rawTexHash;

    /// System subspace schemes containing the manifests/resources.
    FontSchemes fontSchemes;
    QList<FontScheme *> fontSchemeCreationOrder;

    AllFonts fonts;                    ///< From all schemes.
    uint fontManifestCount;            ///< Total number of font manifests (in all schemes).

    uint fontManifestIdMapSize;
    FontManifest **fontManifestIdMap;  ///< Index with fontid_t-1

    typedef QVector<FrameModelDef> ModelDefs;
    ModelDefs modefs;
    QVector<int> stateModefs;          ///< Index to the modefs array.

    typedef StringPool ModelRepository;
    ModelRepository *modelRepository;  ///< Owns FrameModel instances.

    /// A list of specifications for material variants.
    typedef QList<MaterialVariantSpec *> MaterialSpecs;
    MaterialSpecs materialSpecs;

    typedef QList<TextureVariantSpec *> TextureSpecs;
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
        MaterialVariantSpec const *spec; /// Interned context specification.

        MaterialCacheTask(ClientMaterial &resource, MaterialVariantSpec const &contextSpec)
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
    typedef QList<CacheTask *> CacheQueue;
    CacheQueue cacheQueue;
#endif

    Impl(Public *i)
        : Base(i)
#ifdef __CLIENT__
        , fontManifestCount        (0)
        , fontManifestIdMapSize    (0)
        , fontManifestIdMap        (0)
        , modelRepository          (0)
#endif
    {
#ifdef __CLIENT__
        res::TextureManifest::setTextureConstructor([] (res::TextureManifest &m) -> res::Texture * {
            return new ClientTexture(m);
        });
#else
        res::TextureManifest::setTextureConstructor([] (res::TextureManifest &m) -> res::Texture * {
            return new res::Texture(m);
        });
#endif

        LOG_AS("ResourceSystem");

#ifdef __CLIENT__
        /// @note Order here defines the ambigious-URI search order.
        createFontScheme("System");
        createFontScheme("Game");
#endif

        App::packageLoader().loadFromCommandLine();
    }

    ~Impl()
    {
#ifdef __CLIENT__
        self.clearAllFontSchemes();
        clearFontManifests();
        self.clearAllRawTextures();
        self.purgeCacheQueue();

        clearAllTextureSpecs();
        clearMaterialSpecs();

        clearModels();
#endif
    }

    inline de::FS1 &fileSys() { return App_FileSystem(); }

    void clearRuntimeTextures()
    {
        auto &textures = self.textures();
        textures.textureScheme("Flats").clear();
        textures.textureScheme("Textures").clear();
        textures.textureScheme("Patches").clear();
        textures.textureScheme("Sprites").clear();
        textures.textureScheme("Details").clear();
        textures.textureScheme("Reflections").clear();
        textures.textureScheme("Masks").clear();
        textures.textureScheme("ModelSkins").clear();
        textures.textureScheme("ModelReflectionSkins").clear();
        textures.textureScheme("Lightmaps").clear();
        textures.textureScheme("Flaremaps").clear();

#ifdef __CLIENT__
        self.pruneUnusedTextureSpecs();
#endif
    }

    void clearSystemTextures()
    {
        self.textures().textureScheme("System").clear();

#ifdef __CLIENT__
        self.pruneUnusedTextureSpecs();
#endif
    }

#ifdef __CLIENT__
    void clearFontManifests()
    {
        qDeleteAll(fontSchemes);
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

    void createFontScheme(String name)
    {
        DENG2_ASSERT(name.length() >= FontScheme::min_name_length);

        // Create a new scheme.
        FontScheme *newScheme = new FontScheme(name);
        fontSchemes.insert(name.toLower(), newScheme);
        fontSchemeCreationOrder.append(newScheme);

        // We want notification when a new manifest is defined in this scheme.
        newScheme->audienceForManifestDefined += this;
    }

    void clearRuntimeFonts()
    {
        self.fontScheme("Game").clear();

        self.pruneUnusedTextureSpecs();
    }

    void clearSystemFonts()
    {
        self.fontScheme("System").clear();

        self.pruneUnusedTextureSpecs();
    }

    void clearMaterialSpecs()
    {
        qDeleteAll(materialSpecs);
        materialSpecs.clear();
    }

    MaterialVariantSpec *findMaterialSpec(MaterialVariantSpec const &tpl,
        bool canCreate)
    {
        foreach (MaterialVariantSpec *spec, materialSpecs)
        {
            if (spec->compare(tpl)) return spec;
        }

        if (!canCreate) return 0;

        materialSpecs.append(new MaterialVariantSpec(tpl));
        return materialSpecs.back();
    }

    MaterialVariantSpec &getMaterialSpecForContext(MaterialContextId contextId,
        int flags, byte border, int tClass, int tMap, int wrapS, int wrapT,
        int minFilter, int magFilter, int anisoFilter,
        bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
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

        default: DENG2_ASSERT(false);
        }

        TextureVariantSpec const &primarySpec =
            self.textureSpec(primaryContext, flags, border, tClass, tMap,
                             wrapS, wrapT, minFilter, magFilter,
                             anisoFilter, mipmapped, gammaCorrection,
                             noStretch, toAlpha);

        // Apply the normalized spec to the template.
        tpl.contextId     = contextId;
        tpl.primarySpec = &primarySpec;

        return *findMaterialSpec(tpl, true);
    }

    TextureVariantSpec &linkTextureSpec(TextureVariantSpec *spec)
    {
        DENG2_ASSERT(spec != 0);

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

    TextureVariantSpec *findTextureSpec(TextureVariantSpec const &tpl, bool canCreate)
    {
        // Do we already have a concrete version of the template specification?
        switch (tpl.type)
        {
        case TST_GENERAL: {
            foreach (TextureVariantSpec *varSpec, textureSpecs)
            {
                if (*varSpec == tpl)
                {
                    return varSpec;
                }
            }
            break; }

        case TST_DETAIL: {
            int hash = hashDetailTextureSpec(tpl.detailVariant);
            foreach (TextureVariantSpec *varSpec, detailTextureSpecs[hash])
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

    TextureVariantSpec *textureSpec(texturevariantusagecontext_t tc, int flags,
        byte border, int tClass, int tMap, int wrapS, int wrapT, int minFilter,
        int magFilter, int anisoFilter, dd_bool mipmapped, dd_bool gammaCorrection,
        dd_bool noStretch, dd_bool toAlpha)
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

    bool textureSpecInUse(TextureVariantSpec const &spec)
    {
        for (res::Texture *texture : self.textures().allTextures())
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
        QMutableListIterator<TextureVariantSpec *> it(list);
        while (it.hasNext())
        {
            TextureVariantSpec *spec = it.next();
            if (!textureSpecInUse(*spec))
            {
                it.remove();
                delete spec;
                numPruned += 1;
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
        qDeleteAll(textureSpecs);
        textureSpecs.clear();

        for (int i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
        {
            qDeleteAll(detailTextureSpecs[i]);
            detailTextureSpecs[i].clear();
        }
    }

    void processCacheQueue()
    {
        while (!cacheQueue.isEmpty())
        {
            QScopedPointer<CacheTask> task(cacheQueue.takeFirst());
            task->run();
        }
    }

    void queueCacheTasksForMaterial(ClientMaterial &material,
                                    MaterialVariantSpec const &contextSpec,
                                    bool cacheGroups = true)
    {
        // Already in the queue?
        bool alreadyQueued = false;
        foreach (CacheTask *baseTask, cacheQueue)
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
        foreach (world::Materials::MaterialManifestGroup *group,
                 world::Materials::get().allMaterialGroups())
        {
            if (!group->contains(&material.manifest()))
            {
                continue;
            }

            foreach (world::MaterialManifest *manifest, *group)
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
                                  MaterialVariantSpec const &contextSpec,
                                  bool cacheGroups = true)
    {
        if (auto const *sprites = self.sprites().tryFindSpriteSet(id))
        {
            for (Record const &sprite : *sprites)
            {
                for (Value const *val : sprite.geta("views").elements())
                {
                    Record const &spriteView = val->as<RecordValue>().dereference();
                    if (world::Material *material = world::Materials::get().materialPtr(de::Uri(spriteView.gets("material"), RC_NULL)))
                    {
                        queueCacheTasksForMaterial(material->as<ClientMaterial>(),
                                                   contextSpec, cacheGroups);
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
            for (FrameModelSkin const &skin : mdl->skins())
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
#endif

    Composites loadCompositeTextureDefs()
    {
        LOG_AS("loadCompositeTextureDefs");

        typedef QMultiMap<String, res::Composite *> CompositeTextureMap;

        // Load the patch names from the PNAMES lump.
        PatchNames pnames;
        try
        {
            pnames = readPatchNames(fileSys().lump(fileSys().lumpNumForName("PNAMES")));
        }
        catch (LumpIndex::NotFoundError const &er)
        {
            if (App_GameLoaded())
            {
                LOGDEV_RES_WARNING(er.asText());
            }
        }

        // If no patch names - there is no point continuing further.
        if (!pnames.count()) return Composites();

        // Collate an ordered list of all the definition files we intend to process.
        QList<File1 *> defFiles = collectPatchCompositeDefinitionFiles();

        /**
         * Definitions are read into two discreet sets.
         *
         * Older add-ons contain copies of the original games' texture definitions,
         * with their own new definitions appended on the end. However, Doomsday needs
         * to classify all definitions according to whether they originate from the
         * original game data. To achieve the correct user-expected results, we must
         * compare each definition originating from an add-on to determine whether it
         * should instead be classified as "original" data.
         */
        Composites defs, customDefs;

        // Process each definition file.
        int origIndexBase = 0;
        DENG2_FOR_EACH_CONST(QList<File1 *>, i, defFiles)
        {
            File1 &file = **i;

            LOG_RES_VERBOSE("Processing \"%s:%s\"...")
                << NativePath(file.container().composeUri().asText()).pretty()
                << NativePath(file.composeUri().asText()).pretty();

            // Buffer the file and read the next set of definitions.
            int archiveCount;
            Composites newDefs = readCompositeTextureDefs(file, pnames, origIndexBase, archiveCount);

            // In which set do these belong?
            Composites *existingDefs =
                    (file.container().hasCustom()? &customDefs : &defs);

            if (!existingDefs->isEmpty())
            {
                // Merge with the existing definitions.
                existingDefs->append(newDefs);
            }
            else
            {
                *existingDefs = newDefs;
            }

            // Maintain the original index.
            origIndexBase += archiveCount;

            // Print a summary.
            LOG_RES_MSG("Loaded %s texture definitions from \"%s:%s\"")
                << (newDefs.count() == archiveCount? String("all %1").arg(newDefs.count())
                                                   : String("%1 of %1").arg(newDefs.count()).arg(archiveCount))
                << NativePath(file.container().composeUri().asText()).pretty()
                << NativePath(file.composeUri().asText()).pretty();
        }

        if (!customDefs.isEmpty())
        {
            // Custom definitions were found - we must cross compare them.

            // Map the definitions for O(log n) lookup performance,
            CompositeTextureMap mappedCustomDefs;
            foreach (res::Composite *custom, customDefs)
            {
                mappedCustomDefs.insert(custom->percentEncodedNameRef(), custom);
            }

            // Perform reclassification of replaced texture definitions.
            for (int i = 0; i < defs.count(); ++i)
            {
                res::Composite *orig = defs[i];

                // Does a potential replacement exist for this original definition?
                CompositeTextureMap::const_iterator found = mappedCustomDefs.constFind(orig->percentEncodedNameRef());
                if (found == mappedCustomDefs.constEnd())
                    continue;

                // Definition 'custom' is destined to replace 'orig'.
                res::Composite *custom = found.value();
                bool haveReplacement = false;

                if (custom->isFlagged(res::Composite::Custom))
                {
                    haveReplacement = true; // Uses a custom patch.
                }
                else if (*orig != *custom)
                {
                    haveReplacement = true;
                }

                if (haveReplacement)
                {
                    custom->setFlags(res::Composite::Custom);

                    // Let the PWAD "copy" override the IWAD original.
                    defs.takeAt(i);
                    delete orig;

                    --i; // Process the new next definition item.
                }
            }

            /*
             * List 'defs' now contains only those definitions which are not
             * superceeded by those in the 'customDefs' list.
             */

            // Add definitions from the custom list to the end of the main set.
            defs.append(customDefs);
        }

        return defs;
    }

    void initCompositeTextures()
    {
        Time begunAt;

        LOG_RES_VERBOSE("Initializing composite textures...");

        //self.textures().textureScheme("Textures").clear();

        // Load texture definitions from TEXTURE1/2 lumps.
        Composites allDefs = loadCompositeTextureDefs();
        while (!allDefs.isEmpty())
        {
            res::Composite &def = *allDefs.takeFirst();
            de::Uri uri("Textures", Path(def.percentEncodedName()));

            res::Texture::Flags flags;
            if (def.isFlagged(res::Composite::Custom)) flags |= res::Texture::Custom;

            /*
             * The id Tech 1 implementation of the texture collection has a flaw
             * which results in the first texture being used dually as a "NULL"
             * texture.
             */
            if (def.origIndex() == 0) flags |= res::Texture::NoDraw;

            try
            {
                res::TextureManifest &manifest =
                    self.textures().declareTexture(uri, flags, def.logicalDimensions(),
                                        Vector2i(), def.origIndex());

                // Are we redefining an existing texture?
                if (manifest.hasTexture())
                {
                    // Yes. Destroy the existing definition (*should* exist).
                    res::Texture &tex = manifest.texture();
                    res::Composite *oldDef = reinterpret_cast<res::Composite *>(tex.userDataPointer());
                    if (oldDef)
                    {
                        tex.setUserDataPointer(0);
                        delete oldDef;
                    }

                    // Attach the new definition.
                    tex.setUserDataPointer((void *)&def);

                    continue;
                }
                // A new texture.
                else if (res::Texture *tex = manifest.derive())
                {
                    tex->setUserDataPointer((void *)&def);
                    continue;
                }
            }
            catch (res::TextureScheme::InvalidPathError const &er)
            {
                LOG_RES_WARNING("Failed declaring texture \"%s\": %s")
                        << uri << er.asText();
            }

            delete &def;
        }

        LOG_RES_VERBOSE("initCompositeTextures: Completed in %.2f seconds") << begunAt.since();
    }

    void initFlatTextures()
    {
        Time begunAt;

        LOG_RES_VERBOSE("Initializing Flat textures...");

        //self.textures().textureScheme("Flats").clear();

        LumpIndex const &index = fileSys().nameIndex();
        lumpnum_t firstFlatMarkerLumpNum = index.findFirst(Path("F_START.lmp"));
        if (firstFlatMarkerLumpNum >= 0)
        {
            lumpnum_t lumpNum;
            File1 *blockContainer = 0;
            for (lumpNum = index.size(); lumpNum --> firstFlatMarkerLumpNum + 1;)
            {
                File1 &file = index[lumpNum];
                String percentEncodedName = file.name().fileNameWithoutExtension();

                if (blockContainer && blockContainer != &file.container())
                {
                    blockContainer = 0;
                }

                if (!blockContainer)
                {
                    if (!percentEncodedName.compareWithoutCase("F_END") ||
                       !percentEncodedName.compareWithoutCase("FF_END"))
                    {
                        blockContainer = &file.container();
                    }
                    continue;
                }

                if (!percentEncodedName.compareWithoutCase("F_START"))
                {
                    blockContainer = 0;
                    continue;
                }

                // Ignore extra marker lumps.
                if (!percentEncodedName.compareWithoutCase("FF_START") ||
                   !percentEncodedName.compareWithoutCase("F_END")    ||
                   !percentEncodedName.compareWithoutCase("FF_END")) continue;

                de::Uri uri("Flats", Path(percentEncodedName));
                if (self.textures().hasTextureManifest(uri)) continue;

                res::Texture::Flags flags;
                if (file.container().hasCustom()) flags |= res::Texture::Custom;

                /*
                 * Kludge Assume 64x64 else when the flat is loaded it will inherit the
                 * pixel dimensions of the graphic, which, if it has been replaced with
                 * a hires version - will be much larger than it should be.
                 *
                 * @todo Always determine size from the lowres original.
                 */
                Vector2ui dimensions(64, 64);
                Vector2i origin(0, 0);
                int const uniqueId  = lumpNum - (firstFlatMarkerLumpNum + 1);
                de::Uri resourceUri = LumpIndex::composeResourceUrn(lumpNum);

                self.textures().declareTexture(uri, flags, dimensions, origin, uniqueId, &resourceUri);
            }
        }

        // Define any as yet undefined flat textures.
        /// @todo Defer until necessary (manifest texture is first referenced).
        self.textures().deriveAllTexturesInScheme("Flats");

        LOG_RES_VERBOSE("Flat textures initialized in %.2f seconds") << begunAt.since();
    }

    void initSpriteTextures()
    {
        Time begunAt;

        LOG_RES_VERBOSE("Initializing Sprite textures...");

        //self.textures().textureScheme("Sprites").clear();

        dint uniqueId = 1/*1-based index*/;

        /// @todo fixme: Order here does not respect id Tech 1 logic.
        ddstack_t *stack = Stack_New();

        LumpIndex const &index = fileSys().nameIndex();
        for (dint i = 0; i < index.size(); ++i)
        {
            File1 &file = index[i];
            String fileName = file.name().fileNameWithoutExtension();

            if (fileName.beginsWith('S', Qt::CaseInsensitive) && fileName.length() >= 5)
            {
                if (fileName.endsWith("_START", Qt::CaseInsensitive))
                {
                    // We've arrived at *a* sprite block.
                    Stack_Push(stack, NULL);
                    continue;
                }

                if (fileName.endsWith("_END", Qt::CaseInsensitive))
                {
                    // The sprite block ends.
                    Stack_Pop(stack);
                    continue;
                }
            }

            if (!Stack_Height(stack)) continue;

            String decodedFileName = QString(QByteArray::fromPercentEncoding(fileName.toUtf8()));
            if (!res::Sprites::isValidSpriteName(decodedFileName))
            {
                LOG_RES_NOTE("Ignoring invalid sprite name '%s'") << decodedFileName;
                continue;
            }

            de::Uri const uri("Sprites", Path(fileName));

            res::Texture::Flags flags = 0;
            // If this is from an add-on flag it as "custom".
            if (file.container().hasCustom())
            {
                flags |= res::Texture::Custom;
            }

            Vector2ui dimensions;
            Vector2i origin;

            if (file.size())
            {
                // If this is a Patch read the world dimension and origin offset values.
                ByteRefArray const fileData(file.cache(), file.size());
                if (res::Patch::recognize(fileData))
                {
                    try
                    {
                        auto info = res::Patch::loadMetadata(fileData);

                        dimensions = info.logicalDimensions;
                        origin     = -info.origin;
                    }
                    catch (IByteArray::OffsetError const &)
                    {
                        LOG_RES_WARNING("File \"%s:%s\" does not appear to be a valid Patch. "
                                        "World dimension and origin offset not set for sprite \"%s\".")
                                << NativePath(file.container().composePath()).pretty()
                                << NativePath(file.composePath()).pretty()
                                << uri;
                    }
                }
                file.unlock();
            }

            de::Uri const resourceUri = LumpIndex::composeResourceUrn(i);
            try
            {
                self.textures().declareTexture(uri, flags, dimensions, origin, uniqueId, &resourceUri);
                uniqueId++;
            }
            catch (res::TextureScheme::InvalidPathError const &er)
            {
                LOG_RES_WARNING("Failed declaring texture \"%s\": %s") << uri << er.asText();
            }
        }

        while (Stack_Height(stack))
        { Stack_Pop(stack); }

        Stack_Delete(stack);

        // Define any as yet undefined sprite textures.
        /// @todo Defer until necessary (manifest texture is first referenced).
        self.textures().deriveAllTexturesInScheme("Sprites");

        LOG_RES_VERBOSE("Sprite textures initialized in %.2f seconds") << begunAt.since();
    }

#ifdef __CLIENT__
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
        DENG2_ASSERT(modelRepository);
        return reinterpret_cast<FrameModel *>(modelRepository->userPointer(id));
    }

    inline String const &findModelPath(modelid_t id)
    {
        return modelRepository->stringRef(id);
    }

    /**
     * Create a new modeldef or find an existing one. This is for ID'd models.
     */
    FrameModelDef *getModelDefWithId(String id)
    {
        if (id.isEmpty()) return nullptr;

        // First try to find an existing modef.
        if (self.hasModelDef(id))
        {
            return &self.modelDef(id);
        }

        // Get a new entry.
        modefs.append(FrameModelDef(id.toUtf8().constData()));
        return &modefs.last();
    }

    /**
     * Create a new modeldef or find an existing one. There can be only one model
     * definition associated with a state/intermark pair.
     */
    FrameModelDef *getModelDef(dint state, dfloat interMark, dint select)
    {
        // Is this a valid state?
        if (state < 0 || state >= runtimeDefs.states.size())
        {
            return nullptr;
        }

        // First try to find an existing modef.
        for (FrameModelDef const &modef : modefs)
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

    String findSkinPath(Path const &skinPath, Path const &modelFilePath)
    {
        //DENG2_ASSERT(!skinPath.isEmpty());

        // Try the "first choice" directory first.
        if (!modelFilePath.isEmpty())
        {
            // The "first choice" directory is that in which the model file resides.
            try
            {
                return fileSys().findPath(de::Uri("Models", modelFilePath.toString().fileNamePath() / skinPath.fileName()),
                                          RLF_DEFAULT, self.resClass(RC_GRAPHIC));
            }
            catch (FS1::NotFoundError const &)
            {}  // Ignore this error.
        }

        /// @throws FS1::NotFoundError if no resource was found.
        return fileSys().findPath(de::Uri("Models", skinPath), RLF_DEFAULT,
                                  self.resClass(RC_GRAPHIC));
    }

    /**
     * Allocate room for a new skin file name.
     */
    short defineSkinAndAddToModelIndex(FrameModel &mdl, Path const &skinPath)
    {
        if (ClientTexture *tex = static_cast<ClientTexture *>(self.textures().defineTexture("ModelSkins", de::Uri(skinPath))))
        {
            // A duplicate? (return existing skin number)
            for (dint i = 0; i < mdl.skinCount(); ++i)
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
        String const &modelFilePath = findModelPath(mdl.modelId());

        dint numFoundSkins = 0;
        for (dint i = 0; i < mdl.skinCount(); ++i)
        {
            FrameModelSkin &skin = mdl.skin(i);
            try
            {
                de::Uri foundResourceUri(Path(findSkinPath(skin.name, modelFilePath)));

                skin.texture = self.textures().defineTexture("ModelSkins", foundResourceUri);

                // We have found one more skin for this model.
                numFoundSkins += 1;
            }
            catch (FS1::NotFoundError const &)
            {
                LOG_RES_WARNING("Failed to locate \"%s\" (#%i) for model \"%s\"")
                        << skin.name << i << NativePath(modelFilePath).pretty();
            }
        }

        if (!numFoundSkins)
        {
            // Lastly try a skin named similarly to the model in the same directory.
            de::Uri searchPath(modelFilePath.fileNamePath() / modelFilePath.fileNameWithoutExtension(), RC_GRAPHIC);
            try
            {
                String foundPath = fileSys().findPath(searchPath, RLF_DEFAULT,
                                                      self.resClass(RC_GRAPHIC));
                // Ensure the found path is absolute.
                foundPath = App_BasePath() / foundPath;

                defineSkinAndAddToModelIndex(mdl, foundPath);
                // We have found one more skin for this model.
                numFoundSkins = 1;

                LOG_RES_MSG("Assigned fallback skin \"%s\" to index #0 for model \"%s\"")
                    << NativePath(foundPath).pretty()
                    << NativePath(modelFilePath).pretty();
            }
            catch (FS1::NotFoundError const &)
            {}  // Ignore this error.
        }

        if (!numFoundSkins)
        {
            LOG_RES_MSG("No skins found for model \"%s\" (it may use a custom skin specified in a DED)")
                << NativePath(modelFilePath).pretty();
        }

#ifdef DENG2_DEBUG
        LOGDEV_RES_XVERBOSE("Model \"%s\" skins:") << NativePath(modelFilePath).pretty();
        dint skinIdx = 0;
        for (FrameModelSkin const &skin : mdl.skins())
        {
            res::TextureManifest const *texManifest = skin.texture? &skin.texture->manifest() : 0;
            LOGDEV_RES_XVERBOSE("  %i: %s %s")
                    << (skinIdx++) << skin.name
                    << (texManifest? (String("\"") + texManifest->composeUri() + "\"") : "(missing texture)")
                    << (texManifest? (String(" => \"") + NativePath(texManifest->resourceUri().compose()).pretty() + "\"") : "");
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
        dfloat height = self.model(smf.modelId).frame(smf.frame).horizontalRange(&top, &bottom);
        if (!height) height = 1;

        dfloat scale = destHeight / height;

        mf.scale    = Vector3f(scale, scale, scale);
        mf.offset.y = -bottom * scale + offset;
    }

    void scaleModelToSprite(FrameModelDef &mf, Record const *spriteRec)
    {
        if (!spriteRec) return;

        defn::Sprite sprite(*spriteRec);
        if (!sprite.hasView(0)) return;

        world::Material *mat = world::Materials::get().materialPtr(de::Uri(sprite.view(0).gets("material"), RC_NULL));
        if (!mat) return;

        MaterialAnimator &matAnimator = mat->as<ClientMaterial>().getAnimator(Rend_SpriteMaterialSpec());
        matAnimator.prepare();  // Ensure we have up-to-date info.

        ClientTexture const &texture = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture->base();
        dint off = de::max(0, -texture.origin().y - int(matAnimator.dimensions().y));

        scaleModel(mf, matAnimator.dimensions().y, off);
    }

    dfloat calcModelVisualRadius(FrameModelDef *def)
    {
        if (!def || !def->subModelId(0)) return 0;

        // Use the first frame bounds.
        Vector3f min, max;
        dfloat maxRadius = 0;
        for (duint i = 0; i < def->subCount(); ++i)
        {
            if (!def->subModelId(i)) break;

            SubmodelDef &sub = def->subModelDef(i);

            self.model(sub.modelId).frame(sub.frame).bounds(min, max);

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
    void setupModel(defn::Model const &def)
    {
        LOG_AS("setupModel");

        auto &defs = *DED_Definitions();

        dint const modelScopeFlags = def.geti("flags") | defs.modelFlags;
        dint const statenum = defs.getStateNum(def.gets("state"));

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
        modef->offset    = Vector3f(def.get("offset"));
        modef->offset.y += defs.modelOffset; // Common Y axis offset.
        modef->scale     = Vector3f(def.get("scale"));
        modef->scale.y  *= defs.modelScale;  // Common Y axis scaling.
        modef->resize    = def.getf("resize");
        modef->skinTics  = de::max(def.geti("skinTics"), 1);
        for (dint i = 0; i < 2; ++i)
        {
            modef->interRange[i] = float(def.geta("interRange")[i].asNumber());
        }

        // Submodels.
        modef->clearSubs();
        for (dint i = 0; i < def.subCount(); ++i)
        {
            Record const &subdef = def.sub(i);
            SubmodelDef *sub = modef->addSub();

            sub->modelId = 0;

            if (subdef.gets("filename").isEmpty()) continue;

            de::Uri const searchPath(subdef.gets("filename"));
            if (searchPath.isEmpty()) continue;

            try
            {
                String foundPath = fileSys().findPath(searchPath, RLF_DEFAULT,
                                                      self.resClass(RC_MODEL));
                // Ensure the found path is absolute.
                foundPath = App_BasePath() / foundPath;

                // Have we already loaded this?
                modelid_t modelId = modelRepository->intern(foundPath);
                FrameModel *mdl = modelForId(modelId);
                if (!mdl)
                {
                    // Attempt to load it in now.
                    QScopedPointer<FileHandle> hndl(&fileSys().openFile(foundPath, "rb"));

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
                    String const &skinFilePath  = de::Uri(subdef.gets("skinFilename")).path();
                    String const &modelFilePath = findModelPath(sub->modelId);
                    try
                    {
                        Path foundResourcePath(findSkinPath(skinFilePath, modelFilePath));

                        sub->skin = defineSkinAndAddToModelIndex(*mdl, foundResourcePath);
                    }
                    catch (FS1::NotFoundError const &)
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
                    String const &skinFilePath  = de::Uri(subdef.gets("shinySkin")).path();
                    String const &modelFilePath = findModelPath(sub->modelId);
                    try
                    {
                        de::Uri foundResourceUri(Path(findSkinPath(skinFilePath, modelFilePath)));

                        sub->shinySkin = self.textures().defineTexture("ModelReflectionSkins", foundResourceUri);
                    }
                    catch (FS1::NotFoundError const &)
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
            catch (FS1::NotFoundError const &)
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

            if (Record const *sprite = self.sprites().spritePtr(sprNum, sprFrame))
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
                stateModefs[stateNum] = self.indexOf(modef);
            }
            else
            {
                // Must check intermark; smallest wins!
                FrameModelDef *other = self.modelDefForState(stateNum);

                if ((modef->interMark <= other->interMark && // Should never be ==
                    modef->select == other->select) || modef->select < other->select) // Smallest selector?
                {
                    stateModefs[stateNum] = self.indexOf(modef);
                }
            }
        }

        // Calculate the particle offset for each submodel.
        Vector3f min, max;
        for (uint i = 0; i < modef->subCount(); ++i)
        {
            SubmodelDef *sub = &modef->subModelDef(i);
            if (sub->modelId && sub->frame >= 0)
            {
                self.model(sub->modelId).frame(sub->frame).bounds(min, max);
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
        fontid_t const id = ++fontManifestCount; // 1-based.
        manifest.setUniqueId(id);

        // Add the new manifest to the id index/map.
        if (fontManifestCount > fontManifestIdMapSize)
        {
            // Allocate more memory.
            fontManifestIdMapSize += 32;
            fontManifestIdMap = (FontManifest **) M_Realloc(fontManifestIdMap, sizeof(*fontManifestIdMap) * fontManifestIdMapSize);
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
    void fontManifestBeingDeleted(FontManifest const &manifest)
    {
        fontManifestIdMap[manifest.uniqueId() - 1 /*1-based*/] = 0;

        // There will soon be one fewer manifest in the system.
        fontManifestCount -= 1;
    }

    /// Observes AbstractFont Deletion.
    void fontBeingDeleted(AbstractFont const &font)
    {
        fonts.removeOne(const_cast<AbstractFont *>(&font));
    }

    /// Observes ColorPalette ColorTableChange
    void colorPaletteColorTableChanged(res::ColorPalette &colorPalette)
    {
        // Release all GL-textures prepared using @a colorPalette.
        foreach (res::Texture *texture, self.textures().allTextures())
        {
            colorpalette_analysis_t *cp = reinterpret_cast<colorpalette_analysis_t *>(texture->analysisDataPointer(res::Texture::ColorPaletteAnalysis));
            if (cp && cp->paletteId == colorpaletteid_t(colorPalette.id()))
            {
                texture->release();
            }
        }
    }
#endif // __CLIENT__
};

ClientResources::ClientResources() : d(new Impl(this))
{}

ClientResources &ClientResources::get() // static
{
    return static_cast<ClientResources &>(Resources::get());
}

void ClientResources::clear()
{
    Resources::clear();

#ifdef __CLIENT__
    R_ShutdownSvgs();
#endif
    clearAllRuntimeResources();
    animGroups().clearAllAnimGroups();
}

void ClientResources::clearAllResources()
{
    clearAllRuntimeResources();
    clearAllSystemResources();
}

void ClientResources::clearAllRuntimeResources()
{
#ifdef __CLIENT__
    d->clearRuntimeFonts();
#endif
    d->clearRuntimeTextures();
}

void ClientResources::clearAllSystemResources()
{
#ifdef __CLIENT__
    d->clearSystemFonts();
#endif
    d->clearSystemTextures();
}

void ClientResources::addColorPalette(res::ColorPalette &newPalette, String const &name)
{
    colorPalettes().addColorPalette(newPalette, name);

#ifdef __CLIENT__
    // Observe changes to the color table so we can schedule texture updates.
    newPalette.audienceForColorTableChange += d;
#endif
}

void ClientResources::initTextures()
{
    LOG_AS("ResourceSystem");

    d->initCompositeTextures();
    d->initFlatTextures();
    d->initSpriteTextures();
}

void ClientResources::initSystemTextures()
{
    LOG_AS("ResourceSystem");

    static const struct TexDef {
        String const graphicName;
        String const path;
    } texDefs[] = {
        { "unknown",    "unknown" },
        { "missing",    "missing" },
        { "bbox",       "bbox" },
        { "gray",       "gray" },
        { "boxcorner",  "ui/boxcorner" },
        { "boxfill",    "ui/boxfill" },
        { "boxshade",   "ui/boxshade" },
        { "", "" }
    };

    LOG_RES_VERBOSE("Initializing System textures...");

    for (duint i = 0; !texDefs[i].graphicName.isEmpty(); ++i)
    {
        struct TexDef const &def = texDefs[i];

        dint uniqueId = i + 1/*1-based index*/;
        de::Uri resourceUri("Graphics", Path(def.graphicName));

        textures().declareTexture(de::Uri("System", Path(def.path)), res::Texture::Custom,
                       Vector2ui(), Vector2i(), uniqueId, &resourceUri);
    }

    // Define any as yet undefined system textures.
    /// @todo Defer until necessary (manifest texture is first referenced).
    textures().deriveAllTexturesInScheme("System");
}

patchid_t ClientResources::declarePatch(String encodedName)
{
    LOG_AS("ClientResources::declarePatch");

    if (encodedName.isEmpty())
        return 0;

    de::Uri uri("Patches", Path(encodedName));

    // Already defined as a patch?
    try
    {
        res::TextureManifest &manifest = textures().textureManifest(uri);
        /// @todo We should instead define Materials from patches and return the material id.
        return patchid_t( manifest.uniqueId() );
    }
    catch (MissingResourceManifestError const &)
    {}  // Ignore this error.

    Path lumpPath = uri.path() + ".lmp";
    if (!d->fileSys().nameIndex().contains(lumpPath))
    {
        LOG_RES_WARNING("Failed to locate lump for \"%s\"") << uri;
        return 0;
    }

    lumpnum_t const lumpNum = d->fileSys().nameIndex().findLast(lumpPath);
    File1 &file = d->fileSys().lump(lumpNum);

    res::Texture::Flags flags;
    if (file.container().hasCustom()) flags |= res::Texture::Custom;

    Vector2ui dimensions;
    Vector2i origin;

    // If this is a Patch (the format) read the world dimension and origin offset values.
    ByteRefArray fileData = ByteRefArray(file.cache(), file.size());
    if (res::Patch::recognize(fileData))
    {
        try
        {
            auto info = res::Patch::loadMetadata(fileData);

            dimensions = info.logicalDimensions;
            origin     = Vector2i(-info.origin.x, -info.origin.y);
        }
        catch (IByteArray::OffsetError const &)
        {
            LOG_RES_WARNING("File \"%s:%s\" does not appear to be a valid Patch. "
                            "World dimension and origin offset not set for patch \"%s\".")
                << NativePath(file.container().composePath()).pretty()
                << NativePath(file.composePath()).pretty()
                << uri;
        }
    }
    file.unlock();

    dint uniqueId       = textures().textureScheme("Patches").count() + 1;  // 1-based index.
    de::Uri resourceUri = LumpIndex::composeResourceUrn(lumpNum);

    try
    {
        res::TextureManifest &manifest = textures().declareTexture(uri, flags, dimensions, origin,
                                                   uniqueId, &resourceUri);

        /// @todo Defer until necessary (manifest texture is first referenced).
        textures().deriveTexture(manifest);

        return uniqueId;
    }
    catch (res::TextureScheme::InvalidPathError const &er)
    {
        LOG_RES_WARNING("Failed declaring texture \"%s\": %s") << uri << er.asText();
    }
    return 0;
}

#ifdef __CLIENT__

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
    return (found != d->rawTexHash.end() ? found.value() : nullptr);
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

QList<rawtex_t *> ClientResources::collectRawTextures() const
{
    return d->rawTexHash.values();
}

void ClientResources::clearAllRawTextures()
{
    qDeleteAll(d->rawTexHash);
    d->rawTexHash.clear();
}

void ClientResources::releaseAllSystemGLTextures()
{
    if (::novideo) return;

    LOG_AS("ResourceSystem");
    LOG_RES_VERBOSE("Releasing system textures...");

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    ClientApp::renderSystem().clearDrawLists();

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
    ClientApp::renderSystem().clearDrawLists();

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

void ClientResources::releaseGLTexturesByScheme(String schemeName)
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

    dint numPruned = 0;
    numPruned += d->pruneUnusedTextureSpecs(TST_GENERAL);
    numPruned += d->pruneUnusedTextureSpecs(TST_DETAIL);

    LOGDEV_RES_VERBOSE("Pruned %i unused texture variant %s")
        << numPruned << (numPruned == 1? "specification" : "specifications");
}

TextureVariantSpec const &ClientResources::textureSpec(texturevariantusagecontext_t tc,
    dint flags, byte border, dint tClass, dint tMap, dint wrapS, dint wrapT, dint minFilter,
    dint magFilter, dint anisoFilter, dd_bool mipmapped, dd_bool gammaCorrection,
    dd_bool noStretch, dd_bool toAlpha)
{
    TextureVariantSpec *tvs =
        d->textureSpec(tc, flags, border, tClass, tMap, wrapS, wrapT, minFilter,
                       magFilter, anisoFilter, mipmapped, gammaCorrection,
                       noStretch, toAlpha);

#ifdef DENG_DEBUG
    if (tClass || tMap)
    {
        DENG2_ASSERT(tvs->variant.flags & TSF_HAS_COLORPALETTE_XLAT);
        DENG2_ASSERT(tvs->variant.tClass == tClass);
        DENG2_ASSERT(tvs->variant.tMap == tMap);
    }
#endif

    return *tvs;
}

TextureVariantSpec &ClientResources::detailTextureSpec(dfloat contrast)
{
    return *d->detailTextureSpec(contrast);
}

FontScheme &ClientResources::fontScheme(String name) const
{
    LOG_AS("ClientResources::fontScheme");
    if (!name.isEmpty())
    {
        FontSchemes::iterator found = d->fontSchemes.find(name.toLower());
        if (found != d->fontSchemes.end()) return **found;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw UnknownSchemeError("ClientResources::fontScheme", "No scheme found matching '" + name + "'");
}

bool ClientResources::knownFontScheme(String name) const
{
    if (!name.isEmpty())
    {
        return d->fontSchemes.contains(name.toLower());
    }
    return false;
}

ClientResources::FontSchemes const &ClientResources::allFontSchemes() const
{
    return d->fontSchemes;
}

bool ClientResources::hasFont(de::Uri const &path) const
{
    try
    {
        fontManifest(path);
        return true;
    }
    catch (MissingResourceManifestError const &)
    {}  // Ignore this error.
    return false;
}

FontManifest &ClientResources::fontManifest(de::Uri const &uri) const
{
    LOG_AS("ClientResources::findFont");

    // Perform the search.
    // Is this a URN? (of the form "urn:schemename:uniqueid")
    if (!uri.scheme().compareWithoutCase("urn"))
    {
        String const &pathStr = uri.path().toStringRef();
        dint uIdPos = pathStr.indexOf(':');
        if (uIdPos > 0)
        {
            String schemeName = pathStr.left(uIdPos);
            dint uniqueId     = pathStr.mid(uIdPos + 1 /*skip delimiter*/).toInt();

            try
            {
                return fontScheme(schemeName).findByUniqueId(uniqueId);
            }
            catch (FontScheme::NotFoundError const &)
            {}  // Ignore, we'll throw our own...
        }
    }
    else
    {
        // No, this is a URI.
        String const &path = uri.path();

        // Does the user want a manifest in a specific scheme?
        if (!uri.scheme().isEmpty())
        {
            try
            {
                return fontScheme(uri.scheme()).find(path);
            }
            catch (FontScheme::NotFoundError const &)
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
                catch (FontScheme::NotFoundError const &)
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
        DENG2_ASSERT(!"Bookeeping error");
    }

    /// @throw UnknownIdError The specified manifest id is invalid.
    throw UnknownFontIdError("ClientResources::toFontManifest", QString("Invalid font ID %1, valid range [1..%2)").arg(id).arg(d->fontManifestCount + 1));
}

ClientResources::AllFonts const &ClientResources::allFonts() const
{
    return d->fonts;
}

AbstractFont *ClientResources::newFontFromDef(ded_compositefont_t const &def)
{
    LOG_AS("ClientResources::newFontFromDef");

    if (!def.uri) return nullptr;
    de::Uri const &uri = *def.uri;

    try
    {
        // Create/retrieve a manifest for the would-be font.
        FontManifest &manifest = declareFont(uri);
        if (manifest.hasResource())
        {
            if (auto *compFont = manifest.resource().maybeAs<CompositeBitmapFont>())
            {
                /// @todo Do not update fonts here (not enough knowledge). We should
                /// instead return an invalid reference/signal and force the caller
                /// to implement the necessary update logic.
                LOGDEV_RES_XVERBOSE("Font with uri \"%s\" already exists, returning existing")
                    << manifest.composeUri();

                compFont->rebuildFromDef(def);
            }
            return &manifest.resource();
        }

        // A new font.
        manifest.setResource(CompositeBitmapFont::fromDef(manifest, def));
        if (manifest.hasResource())
        {
            if (verbose >= 1)
            {
                LOG_RES_VERBOSE("New font \"%s\"")
                    << manifest.composeUri();
            }
            return &manifest.resource();
        }

        LOG_RES_WARNING("Failed defining new Font for \"%s\"")
            << NativePath(uri.asText()).pretty();
    }
    catch (UnknownSchemeError const &er)
    {
        LOG_RES_WARNING("Failed declaring font \"%s\": %s")
            << NativePath(uri.asText()).pretty() << er.asText();
    }
    catch (FontScheme::InvalidPathError const &er)
    {
        LOG_RES_WARNING("Failed declaring font \"%s\": %s")
            << NativePath(uri.asText()).pretty() << er.asText();
    }

    return nullptr;
}

AbstractFont *ClientResources::newFontFromFile(de::Uri const &uri, String filePath)
{
    LOG_AS("ClientResources::newFontFromFile");

    if (!d->fileSys().accessFile(de::Uri::fromNativePath(filePath)))
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
            if (auto *bmapFont = manifest.resource().maybeAs<BitmapFont>())
            {
                /// @todo Do not update fonts here (not enough knowledge). We should
                /// instead return an invalid reference/signal and force the caller
                /// to implement the necessary update logic.
                LOGDEV_RES_XVERBOSE("Font with uri \"%s\" already exists, returning existing")
                    << manifest.composeUri();

                bmapFont->setFilePath(filePath);
            }
            return &manifest.resource();
        }

        // A new font.
        manifest.setResource(BitmapFont::fromFile(manifest, filePath));
        if (manifest.hasResource())
        {
            if (verbose >= 1)
            {
                LOG_RES_VERBOSE("New font \"%s\"")
                    << manifest.composeUri();
            }
            return &manifest.resource();
        }

        LOG_RES_WARNING("Failed defining new Font for \"%s\"")
            << NativePath(uri.asText()).pretty();
    }
    catch (UnknownSchemeError const &er)
    {
        LOG_RES_WARNING("Failed declaring font \"%s\": %s")
            << NativePath(uri.asText()).pretty() << er.asText();
    }
    catch (FontScheme::InvalidPathError const &er)
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
    throw MissingResourceError("ClientResources::model", "Invalid id " + String::number(id));
}

bool ClientResources::hasModelDef(String id) const
{
    if (!id.isEmpty())
    {
        for (FrameModelDef const &modef : d->modefs)
        {
            if (!id.compareWithoutCase(modef.id))
            {
                return true;
            }
        }
    }
    return false;
}

FrameModelDef &ClientResources::modelDef(dint index)
{
    if (index >= 0 && index < modelDefCount()) return d->modefs[index];
    /// @throw MissingModelDefError An unknown model definition was referenced.
    throw MissingModelDefError("ClientResources::modelDef", "Invalid index #" + String::number(index) + ", valid range " + Rangeui(0, modelDefCount()).asText());
}

FrameModelDef &ClientResources::modelDef(String id)
{
    if (!id.isEmpty())
    {
        for (FrameModelDef const &modef : d->modefs)
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

FrameModelDef *ClientResources::modelDefForState(dint stateIndex, dint select)
{
    if (stateIndex < 0 || stateIndex >= DED_Definitions()->states.size())
        return nullptr;
    if (stateIndex < 0 || stateIndex >= d->stateModefs.count())
        return nullptr;
    if (d->stateModefs[stateIndex] < 0)
        return nullptr;

    DENG2_ASSERT(d->stateModefs[stateIndex] >= 0);
    DENG2_ASSERT(d->stateModefs[stateIndex] < d->modefs.count());

    FrameModelDef *def = &d->modefs[d->stateModefs[stateIndex]];
    if (select)
    {
        // Choose the correct selector, or selector zero if the given one not available.
        dint const mosel = (select & DDMOBJ_SELECTOR_MASK);
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

dint ClientResources::modelDefCount() const
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
    for (dint i = 0; i < runtimeDefs.states.size(); ++i)
    {
        d->stateModefs[i] = -1;
    }

    // Read in the model files and their data.
    // Use the latest definition available for each sprite ID.
    for (dint i = dint(defs.models.size()) - 1; i >= 0; --i)
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
    for (dint i = d->modefs.count() - 1; i >= 0; --i)
    {
        FrameModelDef *me = &d->modefs[i];

        dfloat minmark = 2; // max = 1, so this is "out of bounds".

        FrameModelDef *closest = 0;
        for (dint k = d->modefs.count() - 1; k >= 0; --k)
        {
            FrameModelDef *other = &d->modefs[k];

            /// @todo Need an index by state. -jk
            if (other->state != me->state) continue;

            // Same state and a bigger order are the requirements.
            if (other->def.order() > me->def.order() && // Defined after me.
               other->interMark > me->interMark &&
               other->interMark < minmark)
            {
                minmark = other->interMark;
                closest = other;
            }
        }

        me->interNext = closest;
    }

    // Create selectlinks.
    for (dint i = d->modefs.count() - 1; i >= 0; --i)
    {
        FrameModelDef *me = &d->modefs[i];

        dint minsel = DDMAXINT;

        FrameModelDef *closest = 0;

        // Start scanning from the next definition.
        for (dint k = d->modefs.count() - 1; k >= 0; --k)
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

dint ClientResources::indexOf(FrameModelDef const *modelDef)
{
    dint index = dint(modelDef - &d->modefs[0]);
    return (index >= 0 && index < d->modefs.count() ? index : -1);
}

void ClientResources::setModelDefFrame(FrameModelDef &modef, dint frame)
{
    for (duint i = 0; i < modef.subCount(); ++i)
    {
        SubmodelDef &subdef = modef.subModelDef(i);
        if (subdef.modelId == NOMODELID) continue;

        // Modify the modeldef itself: set the current frame.
        subdef.frame = frame % model(subdef.modelId).frameCount();
    }
}

#endif // __CLIENT__

#ifdef __CLIENT__

void ClientResources::purgeCacheQueue()
{
    qDeleteAll(d->cacheQueue);
    d->cacheQueue.clear();
}

void ClientResources::processCacheQueue()
{
    d->processCacheQueue();
}

void ClientResources::cache(ClientMaterial &material, MaterialVariantSpec const &spec,
                            bool cacheGroups)
{
    d->queueCacheTasksForMaterial(material, spec, cacheGroups);
}

void ClientResources::cache(spritenum_t spriteId, MaterialVariantSpec const &spec)
{
    d->queueCacheTasksForSprite(spriteId, spec);
}

void ClientResources::cache(FrameModelDef *modelDef)
{
    if (!modelDef) return;
    d->queueCacheTasksForModel(*modelDef);
}

MaterialVariantSpec const &ClientResources::materialSpec(MaterialContextId contextId,
    dint flags, byte border, dint tClass, dint tMap, dint wrapS, dint wrapT,
    dint minFilter, dint magFilter, dint anisoFilter,
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
        MaterialVariantSpec const &spec = Rend_MapSurfaceMaterialSpec();

        map.forAllLines([this, &spec] (Line &line)
        {
            for (dint i = 0; i < 2; ++i)
            {
                LineSide &side = line.side(i);
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

        map.forAllSectors([this, &spec] (Sector &sector)
        {
            // Skip sectors with no line sides as their planes will never be drawn.
            if (sector.sideCount())
            {
                sector.forAllPlanes([this, &spec] (Plane &plane)
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
        MaterialVariantSpec const &matSpec = Rend_SpriteMaterialSpec();

        for (dint i = 0; i < sprites().spriteCount(); ++i)
        {
            auto const sprite = spritenum_t(i);

            // Is this sprite used by a state of at least one mobj?
            LoopResult found = map.thinkers().forAll(reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                                                     0x1/*public*/, [&sprite] (thinker_t *th)
            {
                auto const &mob = *reinterpret_cast<mobj_t *>(th);
                if (mob.type >= 0 && mob.type < runtimeDefs.mobjInfo.size())
                {
                    /// @todo optimize: traverses the entire state list!
                    for (dint k = 0; k < DED_Definitions()->states.size(); ++k)
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
            auto const &mob = *reinterpret_cast<mobj_t *>(th);
            // Check through all the model definitions.
            for (dint i = 0; i < modelDefCount(); ++i)
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

#endif // __CLIENT__

String ClientResources::tryFindMusicFile(Record const &definition)
{
    LOG_AS("ClientResources::tryFindMusicFile");

    defn::Music const music(definition);

    de::Uri songUri(music.gets("path"), RC_NULL);
    if (!songUri.path().isEmpty())
    {
        // All external music files are specified relative to the base path.
        String fullPath = App_BasePath() / songUri.path();
        if (F_Access(fullPath.toUtf8().constData()))
        {
            return fullPath;
        }

        LOG_AUDIO_WARNING("Music file \"%s\" not found (id '%s')")
            << songUri << music.gets("id");
    }

    // Try the resource locator.
    String const lumpName = music.gets("lumpName");
    if (!lumpName.isEmpty())
    {
        try
        {
            String const foundPath = App_FileSystem().findPath(de::Uri(lumpName, RC_MUSIC), RLF_DEFAULT,
                                                               App_ResourceClass(RC_MUSIC));
            return App_BasePath() / foundPath;  // Ensure the path is absolute.
        }
        catch (FS1::NotFoundError const &)
        {}  // Ignore this error.
    }
    return "";  // None found.
}

byte precacheMapMaterials = true;
byte precacheSprites = true;
byte texGammaLut[256];

void R_BuildTexGammaLut()
{
#ifdef __SERVER__
    double invGamma = 1.0f;
#else
    double invGamma = 1.0f - de::clamp(0.f, texGamma, 1.f); // Clamp to a sane range.
#endif

    for (int i = 0; i < 256; ++i)
    {
        texGammaLut[i] = byte(255.0f * pow(double(i / 255.0f), invGamma));
    }
}

template <typename ManifestType>
static bool pathBeginsWithComparator(ManifestType const &manifest, void *context)
{
    auto const *path = reinterpret_cast<Path *>(context);
    /// @todo Use PathTree::Node::compare()
    return manifest.path().toStringRef().beginsWith(*path, Qt::CaseInsensitive);
}

/**
 * Decode and then lexicographically compare the two manifest paths,
 * returning @c true if @a is less than @a b.
 */
template <typename PathTreeNodeType>
static bool comparePathTreeNodePathsAscending(PathTreeNodeType const *a, PathTreeNodeType const *b)
{
    String pathA(QString(QByteArray::fromPercentEncoding(a->path().toUtf8())));
    String pathB(QString(QByteArray::fromPercentEncoding(b->path().toUtf8())));
    return pathA.compareWithoutCase(pathB) < 0;
}

/**
 * @param like             Map path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static dint printMapsIndex2(Path const &like, de::Uri::ComposeAsTextFlags composeUriFlags)
{
    res::MapManifests::Tree::FoundNodes found;
    App_ResourceSystem().mapManifests().allMapManifests()
            .findAll(found, pathBeginsWithComparator, const_cast<Path *>(&like));
    if (found.isEmpty()) return 0;

    //bool const printSchemeName = !(composeUriFlags & de::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known maps";
    //if (!printSchemeName && scheme)
    //    heading += " in scheme '" + scheme->name() + "'";
    if (!like.isEmpty())
        heading += " like \"" _E(b) + like.toStringRef() + _E(.) "\"";
    LOG_RES_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index.
    qSort(found.begin(), found.end(), comparePathTreeNodePathsAscending<res::MapManifest>);
    dint const numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    dint idx = 0;
    for (res::MapManifest *mapManifest : found)
    {
        String info = String("%1: " _E(1) "%2" _E(.))
                        .arg(idx, numFoundDigits)
                        .arg(mapManifest->description(composeUriFlags));

        LOG_RES_MSG("  " _E(>)) << info;
        idx++;
    }

    return found.count();
}

/**
 * @param scheme    Material subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Material path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printMaterialIndex2(world::MaterialScheme *scheme, Path const &like,
    de::Uri::ComposeAsTextFlags composeUriFlags)
{
    world::MaterialScheme::Index::FoundNodes found;
    if (scheme) // Consider resources in the specified scheme only.
    {
        scheme->index().findAll(found, pathBeginsWithComparator, const_cast<Path *>(&like));
    }
    else // Consider resources in any scheme.
    {
        world::Materials::get().forAllMaterialSchemes([&found, &like] (world::MaterialScheme &scheme)
        {
            scheme.index().findAll(found, pathBeginsWithComparator, const_cast<Path *>(&like));
            return LoopContinue;
        });
    }
    if (found.isEmpty()) return 0;

    bool const printSchemeName = !(composeUriFlags & de::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known materials";
    if (!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if (!like.isEmpty())
        heading += " like \"" _E(b) + like.toStringRef() + _E(.) "\"";
    LOG_RES_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index.
    qSort(found.begin(), found.end(), comparePathTreeNodePathsAscending<world::MaterialManifest>);
    int const numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    foreach (world::MaterialManifest *manifest, found)
    {
        String info = String("%1: %2%3" _E(.))
                        .arg(idx, numFoundDigits)
                        .arg(manifest->hasMaterial()? _E(1) : _E(2))
                        .arg(manifest->description(composeUriFlags));

        LOG_RES_MSG("  " _E(>)) << info;
        idx++;
    }

    return found.count();
}

/**
 * @param scheme    Texture subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Texture path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printTextureIndex2(res::TextureScheme *scheme, Path const &like,
                              de::Uri::ComposeAsTextFlags composeUriFlags)
{
    res::TextureScheme::Index::FoundNodes found;
    if (scheme)  // Consider resources in the specified scheme only.
    {
        scheme->index().findAll(found, pathBeginsWithComparator, const_cast<Path *>(&like));
    }
    else  // Consider resources in any scheme.
    {
        foreach (res::TextureScheme *scheme, res::Textures::get().allTextureSchemes())
        {
            scheme->index().findAll(found, pathBeginsWithComparator, const_cast<Path *>(&like));
        }
    }
    if (found.isEmpty()) return 0;

    bool const printSchemeName = !(composeUriFlags & de::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known textures";
    if (!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if (!like.isEmpty())
        heading += " like \"" _E(b) + like.toStringRef() + _E(.) "\"";
    LOG_RES_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index key.
    qSort(found.begin(), found.end(), comparePathTreeNodePathsAscending<res::TextureManifest>);
    int numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    foreach (res::TextureManifest *manifest, found)
    {
        String info = String("%1: %2%3")
                        .arg(idx, numFoundDigits)
                        .arg(manifest->hasTexture()? _E(0) : _E(2))
                        .arg(manifest->description(composeUriFlags));

        LOG_RES_MSG("  " _E(>)) << info;
        idx++;
    }

    return found.count();
}

#ifdef __CLIENT__

/**
 * @param scheme    Resource subspace scheme being printed. Can be @c NULL in
 *                  which case resources are printed from all schemes.
 * @param like      Resource path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printFontIndex2(FontScheme *scheme, Path const &like,
    de::Uri::ComposeAsTextFlags composeUriFlags)
{
    FontScheme::Index::FoundNodes found;
    if (scheme) // Only resources in this scheme.
    {
        scheme->index().findAll(found, pathBeginsWithComparator, const_cast<Path *>(&like));
    }
    else // Consider resources in any scheme.
    {
        foreach (FontScheme *scheme, App_ResourceSystem().allFontSchemes())
        {
            scheme->index().findAll(found, pathBeginsWithComparator, const_cast<Path *>(&like));
        }
    }
    if (found.isEmpty()) return 0;

    bool const printSchemeName = !(composeUriFlags & de::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known fonts";
    if (!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if (!like.isEmpty())
        heading += " like \"" _E(b) + like.toStringRef() + _E(.) "\"";
    LOG_RES_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index.
    qSort(found.begin(), found.end(), comparePathTreeNodePathsAscending<FontManifest>);
    int numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    foreach (FontManifest *manifest, found)
    {
        String info = String("%1: %2%3" _E(.))
                        .arg(idx, numFoundDigits)
                        .arg(manifest->hasResource()? _E(1) : _E(2))
                        .arg(manifest->description(composeUriFlags));

        LOG_RES_MSG("  " _E(>)) << info;
        idx++;
    }

    return found.count();
}

#endif // __CLIENT__

static void printMaterialIndex(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    int printTotal = 0;

    // Collate and print results from all schemes?
    if (search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printMaterialIndex2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if (world::Materials::get().isKnownMaterialScheme(search.scheme()))
    {
        printTotal = printMaterialIndex2(&world::Materials::get().materialScheme(search.scheme()),
                                         search.path(), flags | de::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        world::Materials::get().forAllMaterialSchemes([&search, &flags, &printTotal] (world::MaterialScheme &scheme)
        {
            int numPrinted = printMaterialIndex2(&scheme, search.path(), flags | de::Uri::OmitScheme);
            if (numPrinted)
            {
                LOG_MSG(_E(R));
                printTotal += numPrinted;
            }
            return LoopContinue;
        });
    }
    LOG_RES_MSG("Found " _E(b) "%i" _E(.) " %s.") << printTotal << (printTotal == 1? "material" : "materials in total");
}

static void printMapsIndex(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    int printTotal = printMapsIndex2(search.path(), flags | de::Uri::OmitScheme);
    LOG_RES_MSG(_E(R));
    LOG_RES_MSG("Found " _E(b) "%i" _E(.) " %s.") << printTotal << (printTotal == 1? "map" : "maps in total");
}

static void printTextureIndex(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    auto &textures = res::Textures::get();

    int printTotal = 0;

    // Collate and print results from all schemes?
    if (search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printTextureIndex2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if (textures.isKnownTextureScheme(search.scheme()))
    {
        printTotal = printTextureIndex2(&textures.textureScheme(search.scheme()),
                                        search.path(), flags | de::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach (res::TextureScheme *scheme, textures.allTextureSchemes())
        {
            int numPrinted = printTextureIndex2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if (numPrinted)
            {
                LOG_RES_MSG(_E(R));
                printTotal += numPrinted;
            }
        }
    }
    LOG_RES_MSG("Found " _E(b) "%i" _E(.) " %s") << printTotal << (printTotal == 1? "texture" : "textures in total");
}

#ifdef __CLIENT__

static void printFontIndex(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    int printTotal = 0;

    // Collate and print results from all schemes?
    if (search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printFontIndex2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if (App_ResourceSystem().knownFontScheme(search.scheme()))
    {
        printTotal = printFontIndex2(&App_ResourceSystem().fontScheme(search.scheme()),
                                     search.path(), flags | de::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach (FontScheme *scheme, App_ResourceSystem().allFontSchemes())
        {
            int numPrinted = printFontIndex2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if (numPrinted)
            {
                LOG_MSG(_E(R));
                printTotal += numPrinted;
            }
        }
    }
    LOG_RES_MSG("Found " _E(b) "%i" _E(.) " %s.") << printTotal << (printTotal == 1? "font" : "fonts in total");
}

#endif // __CLIENT__

static bool isKnownMaterialSchemeCallback(String name)
{
    return world::Materials::get().isKnownMaterialScheme(name);
}

static bool isKnownTextureSchemeCallback(String name)
{
    return res::Textures::get().isKnownTextureScheme(name);
}

#ifdef __CLIENT__
static bool isKnownFontSchemeCallback(String name)
{
    return App_ResourceSystem().knownFontScheme(name);
}
#endif

/**
 * Print a list of all currently available maps and the location of the source
 * file which contains them.
 *
 * @todo Improve output: find common map id prefixes, find "suffixed" numerical
 * ranges, etc... (Do not assume *anything* about the formatting of map ids -
 * ZDoom allows the mod author to give a map any id they wish, which, is then
 * specified in MAPINFO when defining the map progression, clusters, etc...).
 */
D_CMD(ListMaps)
{
    DENG2_UNUSED(src);

    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1);
    if (search.scheme().isEmpty()) search.setScheme("Maps");

    if (!search.scheme().isEmpty() && search.scheme().compareWithoutCase("Maps"))
    {
        LOG_RES_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    printMapsIndex(search);
    return true;
}

D_CMD(ListMaterials)
{
    DENG2_UNUSED(src);

    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1, &isKnownMaterialSchemeCallback);

    if (!search.scheme().isEmpty() &&
        !world::Materials::get().isKnownMaterialScheme(search.scheme()))
    {
        LOG_RES_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    printMaterialIndex(search);
    return true;
}

D_CMD(ListTextures)
{
    DENG2_UNUSED(src);

    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1, &isKnownTextureSchemeCallback);

    if (!search.scheme().isEmpty() &&
        !res::Textures::get().isKnownTextureScheme(search.scheme()))
    {
        LOG_RES_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    printTextureIndex(search);
    return true;
}

#ifdef __CLIENT__
D_CMD(ListFonts)
{
    DENG2_UNUSED(src);

    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1, &isKnownFontSchemeCallback);
    if (!search.scheme().isEmpty() &&
       !App_ResourceSystem().knownFontScheme(search.scheme()))
    {
        LOG_RES_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    printFontIndex(search);
    return true;
}
#endif // __CLIENT__

#ifdef DENG_DEBUG
D_CMD(PrintMaterialStats)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_MSG(_E(b) "Material Statistics:");
    world::Materials::get().forAllMaterialSchemes([] (world::MaterialScheme &scheme)
    {
        world::MaterialScheme::Index const &index = scheme.index();

        uint count = index.count();
        LOG_MSG("Scheme: %s (%u %s)")
                << scheme.name() << count << (count == 1? "material" : "materials");
        index.debugPrintHashDistribution();
        index.debugPrint();
        return LoopContinue;
    });
    return true;
}

D_CMD(PrintTextureStats)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_MSG(_E(b) "Texture Statistics:");
    foreach (res::TextureScheme *scheme, res::Textures::get().allTextureSchemes())
    {
        res::TextureScheme::Index const &index = scheme->index();

        uint const count = index.count();
        LOG_MSG("Scheme: %s (%u %s)")
            << scheme->name() << count << (count == 1? "texture" : "textures");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}

#  ifdef __CLIENT__
D_CMD(PrintFontStats)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_MSG(_E(b) "Font Statistics:");
    foreach (FontScheme *scheme, App_ResourceSystem().allFontSchemes())
    {
        FontScheme::Index const &index = scheme->index();

        uint const count = index.count();
        LOG_MSG("Scheme: %s (%u %s)")
            << scheme->name() << count << (count == 1? "font" : "fonts");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#  endif // __CLIENT__
#endif // DENG_DEBUG

void ClientResources::consoleRegister() // static
{
    C_CMD("listtextures",   "ss",   ListTextures)
    C_CMD("listtextures",   "s",    ListTextures)
    C_CMD("listtextures",   "",     ListTextures)
#ifdef DENG_DEBUG
    C_CMD("texturestats",   NULL,   PrintTextureStats)
#endif

#ifdef __CLIENT__
    C_CMD("listfonts",      "ss",   ListFonts)
    C_CMD("listfonts",      "s",    ListFonts)
    C_CMD("listfonts",      "",     ListFonts)
#  ifdef DENG_DEBUG
    C_CMD("fontstats",      NULL,   PrintFontStats)
#  endif
#endif

    C_CMD("listmaterials",  "ss",   ListMaterials)
    C_CMD("listmaterials",  "s",    ListMaterials)
    C_CMD("listmaterials",  "",     ListMaterials)
#ifdef DENG_DEBUG
    C_CMD("materialstats",  NULL,   PrintMaterialStats)
#endif
    C_CMD("listmaps",       "s",    ListMaps)
    C_CMD("listmaps",       "",     ListMaps)

    SaveGames      ::consoleRegister();
    res  ::Texture ::consoleRegister();
    world::Material::consoleRegister();
}
