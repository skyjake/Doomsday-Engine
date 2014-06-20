/** @file resourcesystem.cpp  Resource subsystem.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
#include "resource/resourcesystem.h"

#ifdef __CLIENT__
#  include "clientapp.h"
#  include "ui/progress.h"
#  include "sys_system.h" // novideo
#endif

#include "def_main.h"
#include "dd_main.h"
#include "dd_def.h"

#include "resource/compositetexture.h"
#include "resource/patch.h"
#include "resource/patchname.h"
#ifdef __CLIENT__
#  include "MaterialSnapshot"
#  include "gl/gl_tex.h"
#  include "gl/gl_texmanager.h"
#  include "render/rend_model.h"
#  include "render/rend_particle.h" // Rend_ParticleReleaseSystemTextures

// For smart caching logics:
#  include "network/net_demo.h" // playback
#  include "render/rend_main.h" // Rend_MapSurfaceMaterialSpec
#  include "render/billboard.h" // Rend_SpriteMaterialSpec
#  include "render/sky.h"

#  include "world/worldsystem.h"
#  include "world/map.h"
#  include "world/p_object.h"
#  include "world/thinkers.h"
#  include "Sector"
#  include "Surface"
#endif

#include <doomsday/console/cmd.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include <doomsday/filesys/lumpindex.h>

#include <de/App>
#include <de/ByteRefArray>
#include <de/DirectoryFeed>
#include <de/game/SavedSession>
#include <de/game/Session>
#include <de/Log>
#include <de/Loop>
#include <de/Module>
#include <de/ArrayValue>
#include <de/NumberValue>
#include <de/Reader>
#include <de/Task>
#include <de/TaskPool>
#include <de/Time>
#ifdef __CLIENT__
#  include <de/ByteOrder>
#  include <de/NativePath>
#  include <de/StringPool>
#endif
#include <de/stack.h> /// @todo remove me
#include <de/memory.h>
#include <QHash>
#include <QVector>
#include <QtAlgorithms>

namespace de {
namespace internal
{
    /**
     * Compose the path to the data resource.
     * @note We do not use the lump name, instead we use the logical lump index
     * in the global LumpIndex. This is necessary because of the way id tech 1
     * manages graphic references in animations (intermediate frames are chosen
     * by their 'original indices' rather than by name).
     */
    static inline de::Uri composeLumpIndexResourceUrn(lumpnum_t lumpNum)
    {
        return de::Uri("LumpIndex", Path(String("%1").arg(lumpNum)));
    }

    /// Returns a value in the range [0..Sprite::max_angles] if @a rotCode can be
    /// interpreted as a sprite rotation number; otherwise @c -1
    static int toSpriteRotation(QChar rotCode)
    {
        int rot = -1; // Unknown.

        if(rotCode.isDigit())
        {
            rot = rotCode.digitValue();
        }
        else if(rotCode.isLetter())
        {
            char charCodeLatin1 = rotCode.toUpper().toLatin1();
            if(charCodeLatin1 >= 'A')
            {
                rot = charCodeLatin1 - 'A' + 10;
            }
        }

        if(rot > Sprite::max_angles)
        {
            rot = -1;
        }
        else if(rot > 0)
        {
            if(rot <= Sprite::max_angles / 2)
            {
                rot = (rot - 1) * 2 + 1;
            }
            else
            {
                rot = (rot - 9) * 2 + 2;
            }
        }

        return rot;
    }

    /// Returns @c true iff @a name is a well-formed sprite name.
    static bool validateSpriteName(String name)
    {
        if(name.length() < 6) return false;

        // Character at position 5 is a rotation number.
        if(toSpriteRotation(name.at(5)) < 0)
        {
            return false;
        }

        // If defined, the character at position 7 is also a rotation number.
        if(name.length() >= 7)
        {
            if(toSpriteRotation(name.at(7)) < 0)
            {
                return false;
            }
        }

        return true;
    }

    /// Ensure a texture has been derived for @a manifest.
    static Texture *deriveTexture(TextureManifest &manifest)
    {
        LOG_AS("deriveTexture");
        Texture *tex = manifest.derive();
        if(!tex)
        {
            LOGDEV_RES_WARNING("Failed to derive a Texture for \"%s\", ignoring") << manifest.composeUri();
        }
        return tex;
    }

    static QList<File1 *> collectPatchCompositeDefinitionFiles()
    {
        QList<File1 *> result;

        /*
         * Precedence order of definitions is defined by id tech1 which processes
         * the TEXTURE1/2 lumps in the following order:
         *
         * (last)TEXTURE2 > (last)TEXTURE1
         */
        LumpIndex const &index  = App_FileSystem().nameIndex();
        lumpnum_t firstTexLump  = App_FileSystem().lumpNumForName("TEXTURE1");
        lumpnum_t secondTexLump = App_FileSystem().lumpNumForName("TEXTURE2");

        /*
         * Also process all other lumps named TEXTURE1/2.
         */
        for(int i = 0; i < index.size(); ++i)
        {
            File1 &file = index[i];

            // Will this be processed anyway?
            if(i == firstTexLump) continue;
            if(i == secondTexLump) continue;

            String fileName = file.name().fileNameWithoutExtension();
            if(fileName.compareWithoutCase("TEXTURE1") &&
               fileName.compareWithoutCase("TEXTURE2"))
            {
                continue;
            }

            result.append(&file);
        }

        if(firstTexLump >= 0)
        {
            result.append(&index[firstTexLump]);
        }

        if(secondTexLump >= 0)
        {
            result.append(&index[secondTexLump]);
        }

        return result;
    }

    typedef QList<CompositeTexture *> CompositeTextures;
    typedef QList<PatchName> PatchNames;

    static PatchNames readPatchNames(File1 &file)
    {
        LOG_AS("readPatchNames");
        PatchNames names;

        if(file.size() < 4)
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
        if(numNames > 0)
        {
            if((unsigned) numNames > (file.size() - 4) / 8)
            {
                // The data appears to be truncated.
                LOG_RES_WARNING("File \"%s\" appears to be truncated (%u bytes, expected %u)")
                        << NativePath(file.composeUri().asText()).pretty()
                        << file.size() << (numNames * 8 + 4);

                // We'll only read this many names.
                numNames = (file.size() - 4) / 8;
            }

            // Read the names.
            for(int i = 0; i < numNames; ++i)
            {
                PatchName name;
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
    static CompositeTextures readCompositeTextureDefs(File1 &file,
        PatchNames const &patchNames, int origIndexBase, int &archiveCount)
    {
        LOG_AS("readCompositeTextureDefs");

        CompositeTextures result; ///< The resulting set of validated definitions.

        // The game data format determines the format of the archived data.
        CompositeTexture::ArchiveFormat format =
                (gameDataFormat == 0? CompositeTexture::DoomFormat : CompositeTexture::StrifeFormat);

        ByteRefArray data(file.cache(), file.size());
        de::Reader reader(data);

        // First is a count of the total number of definitions.
        dint32 definitionCount;
        reader >> definitionCount;

        // Next is directory of offsets to the definitions.
        typedef QMap<dint32, int> Offsets;
        Offsets offsets;
        for(int i = 0; i < definitionCount; ++i)
        {
            dint32 offset;
            reader >> offset;

            // Ensure the offset is within valid range.
            if(offset < 0 || (unsigned) offset < definitionCount * sizeof(offset) ||
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
            CompositeTexture *def = CompositeTexture::constructFrom(reader, patchNames, format);

            // Attribute the "original index".
            def->setOrigIndex(i.value());

            // If the composite contains at least one known component image it is
            // considered valid and we will therefore produce a Texture for it.
            DENG2_FOR_EACH_CONST(CompositeTexture::Components, it, def->components())
            {
                if(it->lumpNum() >= 0)
                {
                    // Its valid - include in the result.
                    result.append(def);
                    def = 0;
                    break;
                }
            }

            // Failed to validate? Dump it.
            if(def) delete def;
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

        if(tClass || tMap)
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

/**
 * Native Doomsday Script utility for scheduling conversion of a single legacy savegame.
 */
Value *Function_SavedSession_Convert(Context &, Function::ArgumentValues const &args)
{
    String gameId     = args[0]->asText();
    String sourcePath = args[1]->asText();
    return new NumberValue(App_ResourceSystem().convertLegacySavegames(gameId, sourcePath));
}

/**
 * Native Doomsday Script utility for scheduling conversion of @em all legacy savegames
 * for the specified gameId.
 */
Value *Function_SavedSession_ConvertAll(Context &, Function::ArgumentValues const &args)
{
    String gameId = args[0]->asText();
    return new NumberValue(App_ResourceSystem().convertLegacySavegames(gameId));
}

DENG2_PIMPL(ResourceSystem)
, DENG2_OBSERVES(Loop,             Iteration)       // post savegame conversion FS population
, DENG2_OBSERVES(Games,            Addition)        // savegames folder setup
, DENG2_OBSERVES(MaterialScheme,   ManifestDefined)
, DENG2_OBSERVES(MaterialManifest, MaterialDerived)
, DENG2_OBSERVES(MaterialManifest, Deletion)
, DENG2_OBSERVES(Material,         Deletion)
, DENG2_OBSERVES(TextureScheme,    ManifestDefined)
, DENG2_OBSERVES(TextureManifest,  TextureDerived)
, DENG2_OBSERVES(Texture,          Deletion)
#ifdef __CLIENT__
, DENG2_OBSERVES(FontScheme,       ManifestDefined)
, DENG2_OBSERVES(FontManifest,     Deletion)
, DENG2_OBSERVES(AbstractFont,     Deletion)
, DENG2_OBSERVES(ColorPalette,     ColorTableChange)
#endif
{
    typedef QList<ResourceClass *> ResourceClasses;
    ResourceClasses resClasses;
    NullResourceClass nullResourceClass;

    typedef QList<AnimGroup *> AnimGroups;
    AnimGroups animGroups;

    typedef QMap<colorpaletteid_t, ColorPalette *> ColorPalettes;
    ColorPalettes colorPalettes;

    typedef QMap<String, ColorPalette *> ColorPaletteNames;
    ColorPaletteNames colorPaletteNames;

    colorpaletteid_t defaultColorPalette;

    /// System subspace schemes containing the textures.
    TextureSchemes textureSchemes;
    QList<TextureScheme *> textureSchemeCreationOrder;
    /// All texture instances in the system (from all schemes).
    AllTextures textures;

    typedef QHash<lumpnum_t, rawtex_t *> RawTextureHash;
    RawTextureHash rawTexHash;

    /// System subspace schemes containing the manifests/resources.
    MaterialSchemes materialSchemes;
    QList<MaterialScheme *> materialSchemeCreationOrder;

    AllMaterials materials; ///< From all schemes.
    uint materialManifestCount; ///< Total number of material manifests (in all schemes).

    MaterialManifestGroups materialGroups;

    uint materialManifestIdMapSize;
    MaterialManifest **materialManifestIdMap; ///< Index with materialid_t-1

#ifdef __CLIENT__
    /// System subspace schemes containing the manifests/resources.
    FontSchemes fontSchemes;
    QList<FontScheme *> fontSchemeCreationOrder;

    AllFonts fonts; ///< From all schemes.
    uint fontManifestCount; ///< Total number of font manifests (in all schemes).

    uint fontManifestIdMapSize;
    FontManifest **fontManifestIdMap; ///< Index with fontid_t-1

    typedef QVector<ModelDef> ModelDefs;
    ModelDefs modefs;
    QVector<int> stateModefs; // Index to the modefs array.

    typedef StringPool ModelRepository;
    ModelRepository *modelRepository; // Owns Model instances.

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
        Material *material;
        MaterialVariantSpec const *spec; /// Interned context specification.

        MaterialCacheTask(Material &resource, MaterialVariantSpec const &contextSpec)
            : CacheTask(), material(&resource), spec(&contextSpec)
        {}

        void run()
        {
            ResourceSystem &resSys = App_ResourceSystem();

            // Ensure a variant for the specified context is created.
            material->createVariant(*spec);
            DENG2_ASSERT(material->chooseVariant(*spec) != 0);

            // Prepare all layer textures.
            foreach(MaterialLayer *layer, material->layers())
            foreach(MaterialLayer::Stage *stage, layer->stages())
            {
                if(stage->texture)
                {
                    stage->texture->prepareVariant(*spec->primarySpec);
                }
            }

            // Do we need to prepare detail texture(s)?
            if(!material->isSkyMasked() && material->isDetailed())
            foreach(MaterialDetailLayer::Stage *stage, material->detailLayer().stages())
            {
                if(stage->texture)
                {
                    float const contrast = de::clamp(0.f, stage->strength, 1.f)
                                         * detailFactor /*Global strength multiplier*/;
                    stage->texture->prepareVariant(resSys.detailTextureSpec(contrast));
                }
            }

            // Do we need to prepare a shiny texture (and possibly a mask)?
            if(!material->isSkyMasked() && material->isShiny())
            foreach(MaterialShineLayer::Stage *stage, material->shineLayer().stages())
            {
               if(stage->texture)
               {
                   stage->texture->prepareVariant(Rend_MapSurfaceShinyTextureSpec());
                   if(stage->maskTexture)
                   {
                       stage->maskTexture->prepareVariant(Rend_MapSurfaceShinyMaskTextureSpec());
                   }
               }
            }
        }
    };

    /// A FIFO queue of material variant caching tasks.
    /// Implemented as a list because we may need to remove tasks from the queue if
    /// the material is destroyed in the mean time.
    typedef QList<CacheTask *> CacheQueue;
    CacheQueue cacheQueue;
#endif

    struct SpriteGroup {
        SpriteSet sprites;

        ~SpriteGroup() {
            qDeleteAll(sprites);
        }
    };
    typedef QMap<spritenum_t, SpriteGroup> SpriteGroups;
    SpriteGroups spriteGroups;

    NativePath nativeSavePath;

    Binder binder;
    Record savedSessionModule; // SavedSession: manipulation, conversion, etc... (based on native class SavedSession)

    Instance(Public *i)
        : Base(i)
        , defaultColorPalette      (0)
        , materialManifestCount    (0)
        , materialManifestIdMapSize(0)
        , materialManifestIdMap    (0)
#ifdef __CLIENT__
        , fontManifestCount        (0)
        , fontManifestIdMapSize    (0)
        , fontManifestIdMap        (0)
        , modelRepository          (0)
#endif
        , nativeSavePath           (App::app().nativeHomePath() / "savegames") // default
    {
        de::Uri::setResolverFunc(ResourceSystem::resolveSymbol);

        LOG_AS("ResourceSystem");
        resClasses.append(new ResourceClass("RC_PACKAGE",    "Packages"));
        resClasses.append(new ResourceClass("RC_DEFINITION", "Defs"));
        resClasses.append(new ResourceClass("RC_GRAPHIC",    "Graphics"));
        resClasses.append(new ResourceClass("RC_MODEL",      "Models"));
        resClasses.append(new ResourceClass("RC_SOUND",      "Sfx"));
        resClasses.append(new ResourceClass("RC_MUSIC",      "Music"));
        resClasses.append(new ResourceClass("RC_FONT",       "Fonts"));

        /// @note Order here defines the ambigious-URI search order.
        createTextureScheme("Sprites");
        createTextureScheme("Textures");
        createTextureScheme("Flats");
        createTextureScheme("Patches");
        createTextureScheme("System");
        createTextureScheme("Details");
        createTextureScheme("Reflections");
        createTextureScheme("Masks");
        createTextureScheme("ModelSkins");
        createTextureScheme("ModelReflectionSkins");
        createTextureScheme("Lightmaps");
        createTextureScheme("Flaremaps");

        /// @note Order here defines the ambigious-URI search order.
        createMaterialScheme("Sprites");
        createMaterialScheme("Textures");
        createMaterialScheme("Flats");
        createMaterialScheme("System");

#ifdef __CLIENT__
        /// @note Order here defines the ambigious-URI search order.
        createFontScheme("System");
        createFontScheme("Game");
#endif

#ifdef __CLIENT__
        // Setup the SavedSession module.
        binder.init(savedSessionModule)
                << DENG2_FUNC(SavedSession_Convert,    "convert",    "gameId" << "savegamePath")
                << DENG2_FUNC(SavedSession_ConvertAll, "convertAll", "gameId");
        App::scriptSystem().addNativeModule("SavedSession", savedSessionModule);

        // Determine the root directory of the saved session repository.
        if(int arg = App::commandLine().check("-savedir", 1))
        {
            // Using a custom root save directory.
            App::commandLine().makeAbsolutePath(arg + 1);
            nativeSavePath = App::commandLine().at(arg + 1);
        }
        // Else use the default.
#endif

        App_Games().audienceForAddition() += this;

        // Create the user saved session folder in the local FS if it doesn't yet exist.
        // Once created, any SavedSessions in this folder will be found and indexed
        // automatically into the file system.
        App::fileSystem().makeFolder("/home/savegames");

        // Create the legacy savegame folder.
        App::fileSystem().makeFolder("/legacysavegames");
    }

    ~Instance()
    {
        convertSavegameTasks.waitForDone();

        App_Games().audienceForAddition() -= this;

        qDeleteAll(resClasses);
        self.clearAllAnimGroups();
#ifdef __CLIENT__
        self.clearAllFontSchemes();
        clearFontManifests();
#endif
        self.clearAllTextureSchemes();
        clearTextureManifests();
#ifdef __CLIENT__
        self.clearAllRawTextures();
#endif

#ifdef __CLIENT__
        self.purgeCacheQueue();
#endif

        self.clearAllMaterialGroups();
        self.clearAllMaterialSchemes();
        clearMaterialManifests();

#ifdef __CLIENT__
        clearAllTextureSpecs();
        clearMaterialSpecs();
#endif

        clearSprites();
#ifdef __CLIENT__
        clearModels();
#endif
        self.clearAllColorPalettes();
    }

    inline de::FS1 &fileSys()
    {
        return App_FileSystem();
    }

    void gameAdded(Game &game)
    {
        // Called from a non-UI thread.
        LOG_AS("ResourceSystem");
        // Make the /home/savegames/<gameId> subfolder in the local FS if it does not yet exist.
        App::fileSystem().makeFolder(String("/home/savegames") / game.id());
    }

    void clearMaterialManifests()
    {
        qDeleteAll(materialSchemes);
        materialSchemes.clear();
        materialSchemeCreationOrder.clear();

        // Clear the manifest index/map.
        if(materialManifestIdMap)
        {
            M_Free(materialManifestIdMap); materialManifestIdMap = 0;
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

    void clearTextureManifests()
    {
        qDeleteAll(textureSchemes);
        textureSchemes.clear();
        textureSchemeCreationOrder.clear();
    }

    void createTextureScheme(String name)
    {
        DENG2_ASSERT(name.length() >= TextureScheme::min_name_length);

        // Create a new scheme.
        TextureScheme *newScheme = new TextureScheme(name);
        textureSchemes.insert(name.toLower(), newScheme);
        textureSchemeCreationOrder.append(newScheme);

        // We want notification when a new manifest is defined in this scheme.
        newScheme->audienceForManifestDefined += this;
    }

#ifdef __CLIENT__
    void clearFontManifests()
    {
        qDeleteAll(fontSchemes);
        fontSchemes.clear();
        fontSchemeCreationOrder.clear();

        // Clear the manifest index/map.
        if(fontManifestIdMap)
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
#endif

    void clearSprites()
    {
        spriteGroups.clear();
    }

    SpriteGroup *spriteGroup(spritenum_t spriteId)
    {
        SpriteGroups::iterator found = spriteGroups.find(spriteId);
        if(found != spriteGroups.end())
        {
            return &found.value();
        }
        return 0;
    }

    SpriteGroup &newSpriteGroup(spritenum_t spriteId)
    {
        DENG2_ASSERT(!spriteGroup(spriteId)); // sanity check.
        return spriteGroups.insert(spriteId, SpriteGroup()).value();
    }

#ifdef __CLIENT__
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
        foreach(MaterialVariantSpec *spec, materialSpecs)
        {
            if(spec->compare(tpl)) return spec;
        }

        if(!canCreate) return 0;

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
        switch(contextId)
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
        tpl.context     = contextId;
        tpl.primarySpec = &primarySpec;

        return *findMaterialSpec(tpl, true);
    }

    TextureVariantSpec &linkTextureSpec(TextureVariantSpec *spec)
    {
        DENG2_ASSERT(spec != 0);

        switch(spec->type)
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
        switch(tpl.type)
        {
        case TST_GENERAL: {
            foreach(TextureVariantSpec *varSpec, textureSpecs)
            {
                if(*varSpec == tpl)
                {
                    return varSpec;
                }
            }
            break; }

        case TST_DETAIL: {
            int hash = hashDetailTextureSpec(tpl.detailVariant);
            foreach(TextureVariantSpec *varSpec, detailTextureSpecs[hash])
            {
                if(*varSpec == tpl)
                {
                    return varSpec;
                }

            }
            break; }
        }

        // Not found, can we create?
        if(canCreate)
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
        foreach(Texture *texture, textures)
        foreach(TextureVariant *variant, texture->variants())
        {
            if(&variant->spec() == &spec)
            {
                return true; // Found one; stop.
            }
        }
        return false;
    }

    int pruneUnusedTextureSpecs(TextureSpecs &list)
    {
        int numPruned = 0;
        QMutableListIterator<TextureVariantSpec *> it(list);
        while(it.hasNext())
        {
            TextureVariantSpec *spec = it.next();
            if(!textureSpecInUse(*spec))
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
        switch(specType)
        {
        case TST_GENERAL: return pruneUnusedTextureSpecs(textureSpecs);
        case TST_DETAIL: {
            int numPruned = 0;
            for(int i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
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

        for(int i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
        {
            qDeleteAll(detailTextureSpecs[i]);
            detailTextureSpecs[i].clear();
        }
    }
#endif // __CLIENT__

    void clearRuntimeTextures()
    {
        self.textureScheme("Flats").clear();
        self.textureScheme("Textures").clear();
        self.textureScheme("Patches").clear();
        self.textureScheme("Sprites").clear();
        self.textureScheme("Details").clear();
        self.textureScheme("Reflections").clear();
        self.textureScheme("Masks").clear();
        self.textureScheme("ModelSkins").clear();
        self.textureScheme("ModelReflectionSkins").clear();
        self.textureScheme("Lightmaps").clear();
        self.textureScheme("Flaremaps").clear();

#ifdef __CLIENT__
        self.pruneUnusedTextureSpecs();
#endif
    }

    void clearSystemTextures()
    {
        self.textureScheme("System").clear();

#ifdef __CLIENT__
        self.pruneUnusedTextureSpecs();
#endif
    }

#ifdef __CLIENT__
    void processCacheQueue()
    {
        while(!cacheQueue.isEmpty())
        {
            QScopedPointer<CacheTask> task(cacheQueue.takeFirst());
            task->run();
        }
    }

    void queueCacheTasksForMaterial(Material &material, MaterialVariantSpec const &contextSpec,
        bool cacheGroups = true)
    {
        // Already in the queue?
        bool alreadyQueued = false;
        foreach(CacheTask *baseTask, cacheQueue)
        {
            if(MaterialCacheTask *task = dynamic_cast<MaterialCacheTask *>(baseTask))
            {
                if(&material == task->material && &contextSpec == task->spec)
                {
                    alreadyQueued = true;
                    break;
                }
            }
        }

        if(!alreadyQueued)
        {
            cacheQueue.append(new MaterialCacheTask(material, contextSpec));
        }

        if(!cacheGroups) return;

        // If the material is part of one or more groups enqueue cache tasks
        // for all other materials within the same group(s). Although we could
        // use a flag in the task and have it find the groups come prepare time,
        // this way we can be sure there are no overlapping tasks.
        foreach(MaterialManifestGroup *group, materialGroups)
        {
            if(!group->contains(&material.manifest()))
            {
                continue;
            }

            foreach(MaterialManifest *manifest, *group)
            {
                if(!manifest->hasMaterial()) continue;

                // Have we already enqueued this material?
                if(&manifest->material() == &material) continue;

                queueCacheTasksForMaterial(manifest->material(), contextSpec,
                                           false /* do not cache groups */);
            }
        }
    }

    void queueCacheTasksForSprite(spritenum_t spriteId, MaterialVariantSpec const &contextSpec,
        bool cacheGroups = true)
    {
        if(SpriteGroup *group = spriteGroup(spriteId))
        {
            foreach(Sprite *sprite, group->sprites)
            foreach(SpriteViewAngle const &viewAngle, sprite->viewAngles())
            {
                if(Material *material = viewAngle.material)
                {
                    queueCacheTasksForMaterial(*material, contextSpec, cacheGroups);
                }
            }
        }
    }

    void queueCacheTasksForModel(ModelDef &modelDef)
    {
        if(!useModels) return;

        for(uint sub = 0; sub < modelDef.subCount(); ++sub)
        {
            SubmodelDef &subdef = modelDef.subModelDef(sub);
            Model *mdl = modelForId(subdef.modelId);
            if(!mdl) continue;

            // Load all skins.
            foreach(ModelSkin const &skin, mdl->skins())
            {
                if(Texture *tex = skin.texture)
                {
                    tex->prepareVariant(Rend_ModelDiffuseTextureSpec(mdl->flags().testFlag(Model::NoTextureCompression)));
                }
            }

            // Load the shiny skin too.
            if(Texture *shinyTex = subdef.shinySkin)
            {
                shinyTex->prepareVariant(Rend_ModelShinyTextureSpec());
            }
        }
    }
#endif

    void deriveAllTexturesInScheme(String schemeName)
    {
        TextureScheme &scheme = self.textureScheme(schemeName);

        PathTreeIterator<TextureScheme::Index> iter(scheme.index().leafNodes());
        while(iter.hasNext())
        {
            TextureManifest &manifest = iter.next();
            deriveTexture(manifest);
        }
    }

    CompositeTextures loadCompositeTextureDefs()
    {
        LOG_AS("loadCompositeTextureDefs");

        typedef QMultiMap<String, CompositeTexture *> CompositeTextureMap;

        // Load the patch names from the PNAMES lump.
        PatchNames pnames;
        try
        {
            pnames = readPatchNames(fileSys().lump(fileSys().lumpNumForName("PNAMES")));
        }
        catch(LumpIndex::NotFoundError const &er)
        {
            if(App_GameLoaded())
            {
                LOGDEV_RES_WARNING(er.asText());
            }
        }

        // If no patch names - there is no point continuing further.
        if(!pnames.count()) return CompositeTextures();

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
        CompositeTextures defs, customDefs;

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
            CompositeTextures newDefs = readCompositeTextureDefs(file, pnames, origIndexBase, archiveCount);

            // In which set do these belong?
            CompositeTextures *existingDefs =
                    (file.container().hasCustom()? &customDefs : &defs);

            if(!existingDefs->isEmpty())
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

        if(!customDefs.isEmpty())
        {
            // Custom definitions were found - we must cross compare them.

            // Map the definitions for O(log n) lookup performance,
            CompositeTextureMap mappedCustomDefs;
            foreach(CompositeTexture *custom, customDefs)
            {
                mappedCustomDefs.insert(custom->percentEncodedNameRef(), custom);
            }

            // Perform reclassification of replaced texture definitions.
            for(int i = 0; i < defs.count(); ++i)
            {
                CompositeTexture *orig = defs[i];

                // Does a potential replacement exist for this original definition?
                CompositeTextureMap::const_iterator found = mappedCustomDefs.constFind(orig->percentEncodedNameRef());
                if(found == mappedCustomDefs.constEnd())
                    continue;

                // Definition 'custom' is destined to replace 'orig'.
                CompositeTexture *custom = found.value();
                bool haveReplacement = false;

                if(custom->isFlagged(CompositeTexture::Custom))
                {
                    haveReplacement = true; // Uses a custom patch.
                }
                else if(*orig != *custom)
                {
                    haveReplacement = true;
                }

                if(haveReplacement)
                {
                    custom->setFlags(CompositeTexture::Custom);

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

        LOG_RES_VERBOSE("Initializing CompositeTextures...");

        // Load texture definitions from TEXTURE1/2 lumps.
        CompositeTextures allDefs = loadCompositeTextureDefs();
        while(!allDefs.isEmpty())
        {
            CompositeTexture &def = *allDefs.takeFirst();
            de::Uri uri("Textures", Path(def.percentEncodedName()));

            Texture::Flags flags;
            if(def.isFlagged(CompositeTexture::Custom)) flags |= Texture::Custom;

            /*
             * The id Tech 1 implementation of the texture collection has a flaw
             * which results in the first texture being used dually as a "NULL"
             * texture.
             */
            if(def.origIndex() == 0) flags |= Texture::NoDraw;

            try
            {
                TextureManifest &manifest =
                    self.declareTexture(uri, flags, def.logicalDimensions(),
                                        Vector2i(), def.origIndex());

                // Are we redefining an existing texture?
                if(manifest.hasTexture())
                {
                    // Yes. Destroy the existing definition (*should* exist).
                    Texture &tex = manifest.texture();
                    CompositeTexture *oldDef = reinterpret_cast<CompositeTexture *>(tex.userDataPointer());
                    if(oldDef)
                    {
                        tex.setUserDataPointer(0);
                        delete oldDef;
                    }

                    // Attach the new definition.
                    tex.setUserDataPointer((void *)&def);

                    continue;
                }
                // A new texture.
                else if(Texture *tex = manifest.derive())
                {
                    tex->setUserDataPointer((void *)&def);
                    continue;
                }
            }
            catch(TextureScheme::InvalidPathError const &er)
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

        LumpIndex const &index = fileSys().nameIndex();
        lumpnum_t firstFlatMarkerLumpNum = index.findFirst(Path("F_START.lmp"));
        if(firstFlatMarkerLumpNum >= 0)
        {
            lumpnum_t lumpNum;
            File1 *blockContainer = 0;
            for(lumpNum = index.size(); lumpNum --> firstFlatMarkerLumpNum + 1;)
            {
                File1 &file = index[lumpNum];
                String percentEncodedName = file.name().fileNameWithoutExtension();

                if(blockContainer && blockContainer != &file.container())
                {
                    blockContainer = 0;
                }

                if(!blockContainer)
                {
                    if(!percentEncodedName.compareWithoutCase("F_END") ||
                       !percentEncodedName.compareWithoutCase("FF_END"))
                    {
                        blockContainer = &file.container();
                    }
                    continue;
                }

                if(!percentEncodedName.compareWithoutCase("F_START"))
                {
                    blockContainer = 0;
                    continue;
                }

                // Ignore extra marker lumps.
                if(!percentEncodedName.compareWithoutCase("FF_START") ||
                   !percentEncodedName.compareWithoutCase("F_END")    ||
                   !percentEncodedName.compareWithoutCase("FF_END")) continue;

                de::Uri uri("Flats", Path(percentEncodedName));
                if(self.hasTextureManifest(uri)) continue;

                Texture::Flags flags;
                if(file.container().hasCustom()) flags |= Texture::Custom;

                /*
                 * Kludge Assume 64x64 else when the flat is loaded it will inherit the
                 * pixel dimensions of the graphic, which, if it has been replaced with
                 * a hires version - will be much larger than it should be.
                 *
                 * @todo Always determine size from the lowres original.
                 */
                Vector2i dimensions(64, 64);
                Vector2i origin(0, 0);
                int const uniqueId  = lumpNum - (firstFlatMarkerLumpNum + 1);
                de::Uri resourceUri = composeLumpIndexResourceUrn(lumpNum);

                self.declareTexture(uri, flags, dimensions, origin, uniqueId, &resourceUri);
            }
        }

        // Define any as yet undefined flat textures.
        /// @todo Defer until necessary (manifest texture is first referenced).
        deriveAllTexturesInScheme("Flats");

        LOG_RES_VERBOSE("Flat textures initialized in %.2f seconds") << begunAt.since();
    }

    void initSpriteTextures()
    {
        Time begunAt;

        LOG_RES_VERBOSE("Initializing Sprite textures...");

        int uniqueId = 1/*1-based index*/;

        /// @todo fixme: Order here does not respect id Tech 1 logic.
        ddstack_t *stack = Stack_New();

        LumpIndex const &index = fileSys().nameIndex();
        for(int i = 0; i < index.size(); ++i)
        {
            File1 &file = index[i];
            String fileName = file.name().fileNameWithoutExtension();

            if(fileName.beginsWith('S', Qt::CaseInsensitive) && fileName.length() >= 5)
            {
                if(fileName.endsWith("_START", Qt::CaseInsensitive))
                {
                    // We've arrived at *a* sprite block.
                    Stack_Push(stack, NULL);
                    continue;
                }

                if(fileName.endsWith("_END", Qt::CaseInsensitive))
                {
                    // The sprite block ends.
                    Stack_Pop(stack);
                    continue;
                }
            }

            if(!Stack_Height(stack)) continue;

            String decodedFileName = QString(QByteArray::fromPercentEncoding(fileName.toUtf8()));
            if(!validateSpriteName(decodedFileName))
            {
                LOG_RES_NOTE("Ignoring invalid sprite name '%s'") << decodedFileName;
                continue;
            }

            de::Uri uri("Sprites", Path(fileName));

            Texture::Flags flags = 0;
            // If this is from an add-on flag it as "custom".
            if(file.container().hasCustom())
            {
                flags |= Texture::Custom;
            }

            Vector2i dimensions;
            Vector2i origin;

            // If this is a Patch read the world dimension and origin offset values.
            ByteRefArray fileData = ByteRefArray(file.cache(), file.size());
            if(Patch::recognize(fileData))
            {
                try
                {
                    PatchMetadata info = Patch::loadMetadata(fileData);

                    dimensions = info.logicalDimensions;
                    origin     = -info.origin;
                }
                catch(IByteArray::OffsetError const &)
                {
                    LOG_RES_WARNING("File \"%s:%s\" does not appear to be a valid Patch. "
                                    "World dimension and origin offset not set for sprite \"%s\".")
                            << NativePath(file.container().composePath()).pretty()
                            << NativePath(file.composePath()).pretty()
                            << uri;
                }
            }
            file.unlock();

            de::Uri resourceUri = composeLumpIndexResourceUrn(i);
            try
            {
                self.declareTexture(uri, flags, dimensions, origin, uniqueId, &resourceUri);
                uniqueId++;
            }
            catch(TextureScheme::InvalidPathError const &er)
            {
                LOG_RES_WARNING("Failed declaring texture \"%s\": %s") << uri << er.asText();
            }
        }

        while(Stack_Height(stack))
        { Stack_Pop(stack); }

        Stack_Delete(stack);

        // Define any as yet undefined sprite textures.
        /// @todo Defer until necessary (manifest texture is first referenced).
        deriveAllTexturesInScheme("Sprites");

        LOG_RES_VERBOSE("Sprite textures initialized in %.2f seconds") << begunAt.since();
    }

#ifdef __CLIENT__
    void clearModels()
    {
        /// @todo Why only centralized memory deallocation? Bad (lazy) design...
        modefs.clear();
        stateModefs.clear();

        clearModelList();

        if(modelRepository)
        {
            delete modelRepository; modelRepository = 0;
        }
    }

    Model *modelForId(modelid_t modelId)
    {
        DENG2_ASSERT(modelRepository);
        return reinterpret_cast<Model *>(modelRepository->userPointer(modelId));
    }

    inline String const &findModelPath(modelid_t id)
    {
        return modelRepository->stringRef(id);
    }

    /**
     * Create a new modeldef or find an existing one. This is for ID'd models.
     */
    ModelDef *getModelDefWithId(String id)
    {
        if(id.isEmpty()) return 0;

        // First try to find an existing modef.
        if(self.hasModelDef(id))
        {
            return &self.modelDef(id);
        }

        // Get a new entry.
        modefs.append(ModelDef(id.toUtf8().constData()));
        return &modefs.last();
    }

    /**
     * Create a new modeldef or find an existing one. There can be only one model
     * definition associated with a state/intermark pair.
     */
    ModelDef *getModelDef(int state, float interMark, int select)
    {
        // Is this a valid state?
        if(state < 0 || state >= runtimeDefs.states.size())
        {
            return 0;
        }

        // First try to find an existing modef.
        foreach(ModelDef const &modef, modefs)
        {
            if(modef.state == &runtimeDefs.states[state] &&
               fequal(modef.interMark, interMark) && modef.select == select)
            {
                // Models are loaded in reverse order; this one already has a model.
                return 0;
            }
        }

        modefs.append(ModelDef());
        ModelDef *md = &modefs.last();

        // Set initial data.
        md->state     = &runtimeDefs.states[state];
        md->interMark = interMark;
        md->select    = select;

        return md;
    }

    String findSkinPath(Path const &skinPath, Path const &modelFilePath)
    {
        DENG2_ASSERT(!skinPath.isEmpty());

        // Try the "first choice" directory first.
        if(!modelFilePath.isEmpty())
        {
            // The "first choice" directory is that in which the model file resides.
            try
            {
                return fileSys().findPath(de::Uri("Models", modelFilePath.toString().fileNamePath() / skinPath.fileName()),
                                          RLF_DEFAULT, self.resClass(RC_GRAPHIC));
            }
            catch(FS1::NotFoundError const &)
            {} // Ignore this error.
        }

        /// @throws FS1::NotFoundError if no resource was found.
        return fileSys().findPath(de::Uri("Models", skinPath), RLF_DEFAULT,
                                  self.resClass(RC_GRAPHIC));
    }

    /**
     * Allocate room for a new skin file name.
     */
    short defineSkinAndAddToModelIndex(Model &mdl, Path const &skinPath)
    {
        if(Texture *tex = self.defineTexture("ModelSkins", de::Uri(skinPath)))
        {
            // A duplicate? (return existing skin number)
            for(int i = 0; i < mdl.skinCount(); ++i)
            {
                if(mdl.skin(i).texture == tex)
                    return i;
            }

            // Add this new skin.
            mdl.newSkin(skinPath.toString()).texture = tex;
            return mdl.skinCount() - 1;
        }

        return -1;
    }

    void defineAllSkins(Model &mdl)
    {
        String const &modelFilePath = findModelPath(mdl.modelId());

        int numFoundSkins = 0;
        for(int i = 0; i < mdl.skinCount(); ++i)
        {
            ModelSkin &skin = mdl.skin(i);

            if(skin.name.isEmpty())
                continue;

            try
            {
                de::Uri foundResourceUri(Path(findSkinPath(skin.name, modelFilePath)));

                skin.texture = self.defineTexture("ModelSkins", foundResourceUri);

                // We have found one more skin for this model.
                numFoundSkins += 1;
            }
            catch(FS1::NotFoundError const&)
            {
                LOG_RES_WARNING("Failed to locate \"%s\" (#%i) for model \"%s\"")
                    << skin.name << i << NativePath(modelFilePath).pretty();
            }
        }

        if(!numFoundSkins)
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
            catch(FS1::NotFoundError const&)
            {} // Ignore this error.
        }

        if(!numFoundSkins)
        {
            LOG_RES_WARNING("Model \"%s\" will be rendered without a skin (none found)")
                << NativePath(modelFilePath).pretty();
        }
    }

    /**
     * Scales the given model so that it'll be 'destHeight' units tall. Measurements
     * are based on submodel zero. Scale is applied uniformly.
     */
    void scaleModel(ModelDef &mf, float destHeight, float offset)
    {
        if(!mf.subCount()) return;

        SubmodelDef &smf = mf.subModelDef(0);

        // No model to scale?
        if(!smf.modelId) return;

        // Find the top and bottom heights.
        float top, bottom;
        float height = self.model(smf.modelId).frame(smf.frame).horizontalRange(&top, &bottom);
        if(!height) height = 1;

        float scale = destHeight / height;

        mf.scale    = Vector3f(scale, scale, scale);
        mf.offset.y = -bottom * scale + offset;
    }

    void scaleModelToSprite(ModelDef &mf, Sprite *sprite)
    {
        if(!sprite) return;
        if(!sprite->hasViewAngle(0)) return;

        MaterialSnapshot const &ms = sprite->viewAngle(0).material->prepare(Rend_SpriteMaterialSpec());
        Texture const &tex = ms.texture(MTU_PRIMARY).generalCase();
        int off = de::max(0, -tex.origin().y - ms.height());
        scaleModel(mf, ms.height(), off);
    }

    float calcModelVisualRadius(ModelDef *def)
    {
        if(!def || !def->subModelId(0)) return 0;

        // Use the first frame bounds.
        Vector3f min, max;
        float maxRadius = 0;
        for(uint i = 0; i < def->subCount(); ++i)
        {
            if(!def->subModelId(i)) break;

            SubmodelDef &sub = def->subModelDef(i);

            self.model(sub.modelId).frame(sub.frame).bounds(min, max);

            // Half the distance from bottom left to top right.
            float radius = (def->scale.x * (max.x - min.x) +
                            def->scale.z * (max.z - min.z)) / 3.5f;
            if(radius > maxRadius)
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

        int const modelScopeFlags = def.geti("flags") | defs.modelFlags;
        int const statenum = defs.getStateNum(def.gets("state"));

        // Is this an ID'd model?
        ModelDef *modef = getModelDefWithId(def.gets("id"));
        if(!modef)
        {
            // No, normal State-model.
            if(statenum < 0) return;

            modef = getModelDef(statenum + def.geti("off"), def.getf("interMark"), def.geti("selector"));
            if(!modef) return; // Overridden or invalid definition.
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
        for(int i = 0; i < 2; ++i)
        {
            modef->interRange[i] = float(def.geta("interRange")[i].asNumber());
        }

        // Submodels.
        modef->clearSubs();
        for(int i = 0; i < def.subCount(); ++i)
        {
            Record const &subdef = def.sub(i);
            SubmodelDef *sub = modef->addSub();

            sub->modelId = 0;

            if(subdef.gets("filename").isEmpty()) continue;

            de::Uri const searchPath(subdef.gets("filename"));
            if(searchPath.isEmpty()) continue;

            try
            {
                String foundPath = fileSys().findPath(searchPath, RLF_DEFAULT,
                                                      self.resClass(RC_MODEL));
                // Ensure the found path is absolute.
                foundPath = App_BasePath() / foundPath;

                // Have we already loaded this?
                modelid_t modelId = modelRepository->intern(foundPath);
                Model *mdl = modelForId(modelId);
                if(!mdl)
                {
                    // Attempt to load it in now.
                    QScopedPointer<FileHandle> hndl(&fileSys().openFile(foundPath, "rb"));

                    mdl = Model::loadFromFile(*hndl, modelAspectMod);

                    // We're done with the file.
                    fileSys().releaseFile(hndl->file());

                    // Loaded?
                    if(mdl)
                    {
                        // Add it to the repository,
                        mdl->setModelId(modelId);
                        modelRepository->setUserPointer(modelId, mdl);

                        defineAllSkins(*mdl);

                        // Enlarge the vertex buffers in preparation for drawing of this model.
                        if(!Rend_ModelExpandVertexBuffers(mdl->vertexCount()))
                        {
                            LOG_RES_WARNING("Model \"%s\" contains more than %u max vertices (%i), it will not be rendered")
                                << NativePath(foundPath).pretty()
                                << uint(RENDER_MAX_MODEL_VERTS) << mdl->vertexCount();
                        }
                    }
                }

                // Loaded?
                if(!mdl) continue;

                sub->modelId    = mdl->modelId();
                sub->frame      = mdl->frameNumber(subdef.gets("frame"));
                if(sub->frame < 0) sub->frame = 0;
                sub->frameRange = de::max(1, subdef.geti("frameRange")); // Frame range must always be greater than zero.

                sub->alpha      = byte(de::clamp(0, int(255 - subdef.getf("alpha") * 255), 255));
                sub->blendMode  = blendmode_t(subdef.geti("blendMode"));

                // Submodel-specific flags cancel out model-scope flags!
                sub->setFlags(modelScopeFlags ^ subdef.geti("flags"));

                // Flags may override alpha and/or blendmode.
                if(sub->testFlag(MFF_BRIGHTSHADOW))
                {
                    sub->alpha = byte(256 * .80f);
                    sub->blendMode = BM_ADD;
                }
                else if(sub->testFlag(MFF_BRIGHTSHADOW2))
                {
                    sub->blendMode = BM_ADD;
                }
                else if(sub->testFlag(MFF_DARKSHADOW))
                {
                    sub->blendMode = BM_DARK;
                }
                else if(sub->testFlag(MFF_SHADOW2))
                {
                    sub->alpha = byte(256 * .2f);
                }
                else if(sub->testFlag(MFF_SHADOW1))
                {
                    sub->alpha = byte(256 * .62f);
                }

                // Extra blendmodes:
                if(sub->testFlag(MFF_REVERSE_SUBTRACT))
                {
                    sub->blendMode = BM_REVERSE_SUBTRACT;
                }
                else if(sub->testFlag(MFF_SUBTRACT))
                {
                    sub->blendMode = BM_SUBTRACT;
                }

                if(!subdef.gets("skinFilename").isEmpty())
                {
                    // A specific file name has been given for the skin.
                    String const &skinFilePath  = de::Uri(subdef.gets("skinFilename")).path();
                    String const &modelFilePath = findModelPath(sub->modelId);
                    try
                    {
                        Path foundResourcePath(findSkinPath(skinFilePath, modelFilePath));

                        sub->skin = defineSkinAndAddToModelIndex(*mdl, foundResourcePath);
                    }
                    catch(FS1::NotFoundError const &)
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

                if(!subdef.gets("shinySkin").isEmpty())
                {
                    String const &skinFilePath  = de::Uri(subdef.gets("shinySkin")).path();
                    String const &modelFilePath = findModelPath(sub->modelId);
                    try
                    {
                        de::Uri foundResourceUri(Path(findSkinPath(skinFilePath, modelFilePath)));

                        sub->shinySkin = self.defineTexture("ModelReflectionSkins", foundResourceUri);
                    }
                    catch(FS1::NotFoundError const &)
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
                if(sub->testFlag(MFF_NO_TEXCOMP))
                {
                    // All skins of this model will no longer use compression.
                    mdl->setFlags(Model::NoTextureCompression);
                }
            }
            catch(FS1::NotFoundError const &)
            {
                LOG_RES_WARNING("Failed to locate \"%s\"") << searchPath;
            }
        }

        // Do scaling, if necessary.
        if(modef->resize)
        {
            scaleModel(*modef, modef->resize, modef->offset.y);
        }
        else if(modef->state && modef->testSubFlag(0, MFF_AUTOSCALE))
        {
            spritenum_t sprNum = Def_GetSpriteNum(def.gets("sprite"));
            int sprFrame       = def.geti("spriteFrame");

            if(sprNum < 0)
            {
                // No sprite ID given.
                sprNum   = modef->state->sprite;
                sprFrame = modef->state->frame;
            }

            if(Sprite *sprite = self.spritePtr(sprNum, sprFrame))
            {
                scaleModelToSprite(*modef, sprite);
            }
        }

        if(modef->state)
        {
            int stateNum = runtimeDefs.states.indexOf(modef->state);

            // Associate this modeldef with its state.
            if(stateModefs[stateNum] < 0)
            {
                // No modef; use this.
                stateModefs[stateNum] = self.indexOf(modef);
            }
            else
            {
                // Must check intermark; smallest wins!
                ModelDef *other = self.modelDefForState(stateNum);

                if((modef->interMark <= other->interMark && // Should never be ==
                    modef->select == other->select) || modef->select < other->select) // Smallest selector?
                {
                    stateModefs[stateNum] = self.indexOf(modef);
                }
            }
        }

        // Calculate the particle offset for each submodel.
        Vector3f min, max;
        for(uint i = 0; i < modef->subCount(); ++i)
        {
            SubmodelDef *sub = &modef->subModelDef(i);
            if(sub->modelId && sub->frame >= 0)
            {
                self.model(sub->modelId).frame(sub->frame).bounds(min, max);
                modef->setParticleOffset(i, ((max + min) / 2 + sub->offset) * modef->scale + modef->offset);
            }
        }

        // Calculate visual radius for shadows.
        /// @todo fixme: use a separate property.
        /*if(def.shadowRadius)
        {
            modef->visualRadius = def.shadowRadius;
        }
        else*/
        {
            modef->visualRadius = calcModelVisualRadius(modef);
        }
    }

    static int destroyModelInRepository(StringPool::Id id, void *context)
    {
        ModelRepository &modelRepository = *static_cast<ModelRepository *>(context);
        if(Model *model = reinterpret_cast<Model *>(modelRepository.userPointer(id)))
        {
            modelRepository.setUserPointer(id, 0);
            delete model;
        }
        return 0;
    }

    void clearModelList()
    {
        if(!modelRepository) return;

        modelRepository->iterate(destroyModelInRepository, modelRepository);
    }
#endif

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
        materialid_t const id = ++materialManifestCount; // 1-based.
        manifest.setId(id);

        // Add the new manifest to the id index/map.
        if(materialManifestCount > materialManifestIdMapSize)
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
        material.audienceForDeletion += this;
    }

    /// Observes MaterialManifest Deletion.
    void materialManifestBeingDeleted(MaterialManifest const &manifest)
    {
        foreach(MaterialManifestGroup *group, materialGroups)
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

    /// Observes TextureScheme ManifestDefined.
    void textureSchemeManifestDefined(TextureScheme & /*scheme*/, TextureManifest &manifest)
    {
        // We want notification when the manifest is derived to produce a texture.
        manifest.audienceForTextureDerived += this;
    }

    /// Observes TextureManifest TextureDerived.
    void textureManifestTextureDerived(TextureManifest & /*manifest*/, Texture &texture)
    {
        // Include this new texture in the scheme-agnostic list of instances.
        textures.append(&texture);

        // We want notification when the texture is about to be deleted.
        texture.audienceForDeletion += this;
    }

    /// Observes Texture Deletion.
    void textureBeingDeleted(Texture const &texture)
    {
        textures.removeOne(const_cast<Texture *>(&texture));
    }

#ifdef __CLIENT__
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
        if(fontManifestCount > fontManifestIdMapSize)
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
    void colorPaletteColorTableChanged(ColorPalette &colorPalette)
    {
        // Release all GL-textures prepared using @a colorPalette.
        foreach(Texture *texture, textures)
        {
            colorpalette_analysis_t *cp = reinterpret_cast<colorpalette_analysis_t *>(texture->analysisDataPointer(Texture::ColorPaletteAnalysis));
            if(cp && cp->paletteId == colorpaletteid_t(colorPalette.id()))
            {
                texture->releaseGLTextures();
            }
        }
    }
#endif // __CLIENT__

    /**
     * Asynchronous task that attempts conversion of a legacy savegame. Each converter
     * plugin is tried in turn.
     */
    class ConvertSavegameTask : public Task
    {
        ddhook_savegame_convert_t parm;

    public:
        ConvertSavegameTask(String const &sourcePath, String const &gameId)
        {
            // Ensure the game is defined (sanity check).
            /*Game &game = */ App_Games().byIdentityKey(gameId);

            // Ensure the output folder exists if it doesn't already.
            String const outputPath = String("/home/savegames") / gameId;
            App::fileSystem().makeFolder(outputPath);

            Str_Set(Str_InitStd(&parm.sourcePath),     sourcePath.toUtf8().constData());
            Str_Set(Str_InitStd(&parm.outputPath),     outputPath.toUtf8().constData());
            Str_Set(Str_InitStd(&parm.fallbackGameId), gameId.toUtf8().constData());
        }

        ~ConvertSavegameTask()
        {
            Str_Free(&parm.sourcePath);
            Str_Free(&parm.outputPath);
            Str_Free(&parm.fallbackGameId);
        }

        void runTask()
        {
            DD_CallHooks(HOOK_SAVEGAME_CONVERT, 0, &parm);
        }
    };
    TaskPool convertSavegameTasks;

    void loopIteration()
    {
        if(convertSavegameTasks.isDone())
        {
            LOG_AS("ResourceSystem");
            Loop::appLoop().audienceForIteration() -= this;
            try
            {
                // The newly converted savegame(s) should now be somewhere in /home/savegames
                App::rootFolder().locate<Folder>("/home/savegames").populate();
            }
            catch(Folder::NotFoundError const &)
            {} // Ignore.
        }
    }

    void beginConvertLegacySavegame(String const &sourcePath, String const &gameId)
    {
        LOG_AS("ResourceSystem");
        LOG_TRACE("Scheduling legacy savegame conversion for %s (gameId:%s)") << sourcePath << gameId;
        Loop::appLoop().audienceForIteration() += this;
        convertSavegameTasks.start(new ConvertSavegameTask(sourcePath, gameId));
    }

    void locateLegacySavegames(String const &gameId)
    {
        LOG_AS("ResourceSystem");
        String const legacySavePath = String("/legacysavegames") / gameId;
        if(Folder *oldSaveFolder = App::rootFolder().tryLocate<Folder>(legacySavePath))
        {
            // Add any new legacy savegames which may have appeared in this folder.
            oldSaveFolder->populate(Folder::PopulateOnlyThisFolder /* no need to go deep */);
        }
        else
        {
            try
            {
                // Make and setup a feed for the /legacysavegames/<gameId> subfolder if the game
                // might have legacy savegames we may need to convert later.
                NativePath const oldSavePath = App_Games().byIdentityKey(gameId).legacySavegamePath();
                if(oldSavePath.exists() && oldSavePath.isReadable())
                {
                    App::fileSystem().makeFolderWithFeed(legacySavePath,
                            new DirectoryFeed(oldSavePath),
                            Folder::PopulateOnlyThisFolder /* no need to go deep */);
                }
            }
            catch(Games::NotFoundError const &)
            {} // Ignore this error
        }
    }
};

ResourceSystem::ResourceSystem() : d(new Instance(this))
{}

void ResourceSystem::timeChanged(Clock const &)
{
    // Nothing to do.
}

ResourceClass &ResourceSystem::resClass(String name)
{
    if(!name.isEmpty())
    {
        foreach(ResourceClass *resClass, d->resClasses)
        {
            if(!resClass->name().compareWithoutCase(name))
                return *resClass;
        }
    }
    return d->nullResourceClass; // Not found.
}

ResourceClass &ResourceSystem::resClass(resourceclassid_t id)
{
    if(id == RC_NULL) return d->nullResourceClass;
    if(VALID_RESOURCECLASSID(id))
    {
        return *d->resClasses.at(uint(id));
    }
    /// @throw UnknownResourceClass Attempted with an unknown id.
    throw UnknownResourceClassError("ResourceSystem::toClass", QString("Invalid id '%1'").arg(int(id)));
}

void ResourceSystem::clearAllResources()
{
    clearAllRuntimeResources();
    clearAllSystemResources();
}

void ResourceSystem::clearAllRuntimeResources()
{
#ifdef __CLIENT__
    d->clearRuntimeFonts();
#endif
    d->clearRuntimeTextures();
}

void ResourceSystem::clearAllSystemResources()
{
#ifdef __CLIENT__
    d->clearSystemFonts();
#endif
    d->clearSystemTextures();
}

int ResourceSystem::spriteCount()
{
    return d->spriteGroups.count();
}

bool ResourceSystem::hasSprite(spritenum_t spriteId, int frame)
{
    if(Instance::SpriteGroup *group = d->spriteGroup(spriteId))
    {
        if(frame >= 0 && frame < group->sprites.count())
        {
            return true;
        }
    }
    return false;
}

Sprite &ResourceSystem::sprite(spritenum_t spriteId, int frame)
{
    return *spriteSet(spriteId).at(frame);
}

ResourceSystem::SpriteSet const &ResourceSystem::spriteSet(spritenum_t spriteId)
{
    if(Instance::SpriteGroup *group = d->spriteGroup(spriteId))
    {
        return group->sprites;
    }
    /// @throw MissingResourceError An unknown/invalid id was specified.
    throw MissingResourceError("ResourceSystem::spriteSet", "Invalid sprite id " + String::number(spriteId));
}

void ResourceSystem::initTextures()
{
    LOG_AS("ResourceSystem");

    d->initCompositeTextures();
    d->initFlatTextures();
    d->initSpriteTextures();
}

void ResourceSystem::initSystemTextures()
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

    for(uint i = 0; !texDefs[i].graphicName.isEmpty(); ++i)
    {
        struct TexDef const &def = texDefs[i];

        int uniqueId = i + 1/*1-based index*/;
        de::Uri resourceUri("Graphics", Path(def.graphicName));

        declareTexture(de::Uri("System", Path(def.path)), Texture::Custom,
                       Vector2i(), Vector2i(), uniqueId, &resourceUri);
    }

    // Define any as yet undefined system textures.
    /// @todo Defer until necessary (manifest texture is first referenced).
    d->deriveAllTexturesInScheme("System");
}

Texture *ResourceSystem::texture(String schemeName, de::Uri const *resourceUri)
{
    if(resourceUri && !resourceUri->isEmpty())
    {
        if(!resourceUri->path().toStringRef().compareWithoutCase("-")) return 0;

        try
        {
            return &textureScheme(schemeName).findByResourceUri(*resourceUri).texture();
        }
        catch(TextureManifest::MissingTextureError const &)
        {} // Ignore this error.
        catch(TextureScheme::NotFoundError const &)
        {} // Ignore this error.
    }
    return 0;
}

Texture *ResourceSystem::defineTexture(String schemeName, de::Uri const &resourceUri,
    Vector2i const &dimensions)
{
    LOG_AS("ResourceSystem::defineTexture");

    if(resourceUri.isEmpty()) return 0;

    // Have we already created one for this?
    TextureScheme &scheme = textureScheme(schemeName);
    try
    {
        return &scheme.findByResourceUri(resourceUri).texture();
    }
    catch(TextureManifest::MissingTextureError const &)
    {} // Ignore this error.
    catch(TextureScheme::NotFoundError const &)
    {} // Ignore this error.

    int uniqueId = scheme.count() + 1; // 1-based index.
    if(M_NumDigits(uniqueId) > 8)
    {
        LOG_RES_WARNING("Failed declaring texture manifest in scheme %s (max:%i)")
            << schemeName << DDMAXINT;
        return 0;
    }

    de::Uri uri(scheme.name(), Path(String("%1").arg(uniqueId, 8, 10, QChar('0'))));
    try
    {
        TextureManifest &manifest = declareTexture(uri, Texture::Custom, dimensions,
                                                   Vector2i(), uniqueId, &resourceUri);

        /// @todo Defer until necessary (manifest texture is first referenced).
        return deriveTexture(manifest);
    }
    catch(TextureScheme::InvalidPathError const &er)
    {
        LOG_RES_WARNING("Failed declaring texture \"%s\": %s") << uri << er.asText();
    }
    return 0;
}

patchid_t ResourceSystem::declarePatch(String encodedName)
{
    LOG_AS("ResourceSystem::declarePatch");

    if(encodedName.isEmpty())
        return 0;

    de::Uri uri("Patches", Path(encodedName));

    // Already defined as a patch?
    try
    {
        TextureManifest &manifest = textureManifest(uri);
        /// @todo We should instead define Materials from patches and return the material id.
        return patchid_t( manifest.uniqueId() );
    }
    catch(MissingManifestError const &)
    {} // Ignore this error.

    Path lumpPath = uri.path() + ".lmp";
    if(!d->fileSys().nameIndex().contains(lumpPath))
    {
        LOG_RES_WARNING("Failed to locate lump for \"%s\"") << uri;
        return 0;
    }

    lumpnum_t const lumpNum = d->fileSys().nameIndex().findLast(lumpPath);
    File1 &file = d->fileSys().lump(lumpNum);

    Texture::Flags flags;
    if(file.container().hasCustom()) flags |= Texture::Custom;

    Vector2i dimensions;
    Vector2i origin;

    // If this is a Patch (the format) read the world dimension and origin offset values.
    ByteRefArray fileData = ByteRefArray(file.cache(), file.size());
    if(Patch::recognize(fileData))
    {
        try
        {
            Patch::Metadata info = Patch::loadMetadata(fileData);

            dimensions = info.logicalDimensions;
            origin     = Vector2i(-info.origin.x, -info.origin.y);
        }
        catch(IByteArray::OffsetError const &)
        {
            LOG_RES_WARNING("File \"%s:%s\" does not appear to be a valid Patch. "
                            "World dimension and origin offset not set for patch \"%s\".")
                << NativePath(file.container().composePath()).pretty()
                << NativePath(file.composePath()).pretty()
                << uri;
        }
    }
    file.unlock();

    int uniqueId        = textureScheme("Patches").count() + 1; // 1-based index.
    de::Uri resourceUri = composeLumpIndexResourceUrn(lumpNum);

    try
    {
        TextureManifest &manifest = declareTexture(uri, flags, dimensions, origin,
                                                   uniqueId, &resourceUri);

        /// @todo Defer until necessary (manifest texture is first referenced).
        deriveTexture(manifest);

        return uniqueId;
    }
    catch(TextureScheme::InvalidPathError const &er)
    {
        LOG_RES_WARNING("Failed declaring texture \"%s\": %s") << uri << er.asText();
    }
    return 0;
}

rawtex_t *ResourceSystem::rawTexture(lumpnum_t lumpNum)
{
    LOG_AS("ResourceSystem::rawTexture");
    if(-1 == lumpNum || lumpNum >= App_FileSystem().lumpCount())
    {
        LOGDEV_RES_WARNING("LumpNum #%i out of bounds (%i), returning 0")
                << lumpNum << App_FileSystem().lumpCount();
        return 0;
    }

    Instance::RawTextureHash::iterator found = d->rawTexHash.find(lumpNum);
    if(found != d->rawTexHash.end())
    {
        return found.value();
    }
    return 0;
}

rawtex_t *ResourceSystem::declareRawTexture(lumpnum_t lumpNum)
{
    LOG_AS("ResourceSystem::rawTexture");
    if(-1 == lumpNum || lumpNum >= App_FileSystem().lumpCount())
    {
        LOGDEV_RES_WARNING("LumpNum #%i out of range %s, returning 0")
            << lumpNum << Rangeui(0, App_FileSystem().lumpCount()).asText();
        return 0;
    }

    // Has this raw texture already been declared?
    rawtex_t *raw = rawTexture(lumpNum);
    if(!raw)
    {
        // An entirely new raw texture.
        String const &name = App_FileSystem().lump(lumpNum).name();
        raw = new rawtex_t(name, lumpNum);
        d->rawTexHash.insert(lumpNum, raw);
    }

    return raw;
}

QList<rawtex_t *> ResourceSystem::collectRawTextures() const
{
    return d->rawTexHash.values();
}

void ResourceSystem::clearAllRawTextures()
{
    qDeleteAll(d->rawTexHash);
    d->rawTexHash.clear();
}

MaterialScheme &ResourceSystem::materialScheme(String name) const
{
    LOG_AS("ResourceSystem::materialScheme");
    if(!name.isEmpty())
    {
        MaterialSchemes::iterator found = d->materialSchemes.find(name.toLower());
        if(found != d->materialSchemes.end())
        {
            return **found;
        }
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw UnknownSchemeError("ResourceSystem::materialScheme", "No scheme found matching '" + name + "'");
}

bool ResourceSystem::knownMaterialScheme(String name) const
{
    if(!name.isEmpty())
    {
        return d->materialSchemes.contains(name.toLower());
    }
    return false;
}

ResourceSystem::MaterialSchemes const &ResourceSystem::allMaterialSchemes() const
{
    return d->materialSchemes;
}

MaterialManifest &ResourceSystem::toMaterialManifest(materialid_t id) const
{
    duint32 idx = id - 1; // 1-based index.
    if(idx < d->materialManifestCount)
    {
        if(d->materialManifestIdMap[idx])
        {
            return *d->materialManifestIdMap[idx];
        }
        // Internal bookeeping error.
        DENG2_ASSERT(false);
    }
    /// @throw InvalidMaterialIdError The specified material id is invalid.
    throw UnknownMaterialIdError("ResourceSystem::toMaterialManifest", "Invalid material ID " + String::number(id) + ", valid range " + Rangeui(1, d->materialManifestCount + 1).asText());
}

bool ResourceSystem::hasMaterialManifest(de::Uri const &path) const
{
    try
    {
        materialManifest(path);
        return true;
    }
    catch(MissingManifestError const &)
    {} // Ignore this error.
    return false;
}

MaterialManifest &ResourceSystem::materialManifest(de::Uri const &uri) const
{
    LOG_AS("ResourceSystem::materialManifest");

    // Does the user want a manifest in a specific scheme?
    if(!uri.scheme().isEmpty())
    {
        MaterialScheme &specifiedScheme = materialScheme(uri.scheme());
        if(specifiedScheme.has(uri.path()))
        {
            return specifiedScheme.find(uri.path());
        }
    }
    else
    {
        // No, check each scheme in priority order.
        foreach(MaterialScheme *scheme, d->materialSchemeCreationOrder)
        {
            if(scheme->has(uri.path()))
            {
                return scheme->find(uri.path());
            }
        }
    }

    /// @throw NotFoundError Failed to locate a matching manifest.
    throw MissingManifestError("ResourceSystem::materialManifest", "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

ResourceSystem::AllMaterials const &ResourceSystem::allMaterials() const
{
    return d->materials;
}

ResourceSystem::MaterialManifestGroup &ResourceSystem::newMaterialGroup()
{
    // Allocating one by one is inefficient, but it doesn't really matter.
    d->materialGroups.append(new MaterialManifestGroup());
    return *d->materialGroups.back();
}

ResourceSystem::MaterialManifestGroup &ResourceSystem::materialGroup(int groupIdx) const
{
    groupIdx -= 1; // 1-based index.
    if(groupIdx >= 0 && groupIdx < d->materialGroups.count())
    {
        return *d->materialGroups[groupIdx];
    }
    /// @throw UnknownMaterialGroupError An unknown material group was referenced.
    throw UnknownMaterialGroupError("ResourceSystem::materialGroup", "Invalid group #" + String::number(groupIdx+1) + ", valid range " + Rangeui(1, d->materialGroups.count() + 1).asText());
}

ResourceSystem::MaterialManifestGroups const &ResourceSystem::allMaterialGroups() const
{
    return d->materialGroups;
}

void ResourceSystem::clearAllMaterialGroups()
{
    qDeleteAll(d->materialGroups);
    d->materialGroups.clear();
}

TextureScheme &ResourceSystem::textureScheme(String name) const
{
    LOG_AS("ResourceSystem::textureScheme");
    if(!name.isEmpty())
    {
        TextureSchemes::iterator found = d->textureSchemes.find(name.toLower());
        if(found != d->textureSchemes.end())
        {
            return **found;
        }
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw UnknownSchemeError("ResourceSystem::textureScheme", "No scheme found matching '" + name + "'");
}

bool ResourceSystem::knownTextureScheme(String name) const
{
    if(!name.isEmpty())
    {
        return d->textureSchemes.contains(name.toLower());
    }
    return false;
}

ResourceSystem::TextureSchemes const& ResourceSystem::allTextureSchemes() const
{
    return d->textureSchemes;
}

bool ResourceSystem::hasTextureManifest(de::Uri const &path) const
{
    try
    {
        textureManifest(path);
        return true;
    }
    catch(MissingManifestError const &)
    {} // Ignore this error.
    return false;
}

TextureManifest &ResourceSystem::textureManifest(de::Uri const &uri) const
{
    LOG_AS("ResourceSystem::findTexture");

    // Perform the search.
    // Is this a URN? (of the form "urn:schemename:uniqueid")
    if(!uri.scheme().compareWithoutCase("urn"))
    {
        String const &pathStr = uri.path().toStringRef();
        int uIdPos = pathStr.indexOf(':');
        if(uIdPos > 0)
        {
            String schemeName = pathStr.left(uIdPos);
            int uniqueId      = pathStr.mid(uIdPos + 1 /*skip delimiter*/).toInt();

            try
            {
                return textureScheme(schemeName).findByUniqueId(uniqueId);
            }
            catch(TextureScheme::NotFoundError const &)
            {} // Ignore, we'll throw our own...
        }
    }
    else
    {
        // No, this is a URI.
        String const &path = uri.path();

        // Does the user want a manifest in a specific scheme?
        if(!uri.scheme().isEmpty())
        {
            try
            {
                return textureScheme(uri.scheme()).find(path);
            }
            catch(TextureScheme::NotFoundError const &)
            {} // Ignore, we'll throw our own...
        }
        else
        {
            // No, check each scheme in priority order.
            foreach(TextureScheme *scheme, d->textureSchemeCreationOrder)
            {
                try
                {
                    return scheme->find(path);
                }
                catch(TextureScheme::NotFoundError const &)
                {} // Ignore, we'll throw our own...
            }
        }
    }

    /// @throw MissingManifestError Failed to locate a matching manifest.
    throw MissingManifestError("ResourceSystem::findTexture", "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

ResourceSystem::AllTextures const &ResourceSystem::allTextures() const
{
    return d->textures;
}

#ifdef __CLIENT__

void ResourceSystem::releaseAllSystemGLTextures()
{
    if(novideo) return;

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

void ResourceSystem::releaseAllRuntimeGLTextures()
{
    if(novideo) return;

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

void ResourceSystem::releaseAllGLTextures()
{
    releaseAllRuntimeGLTextures();
    releaseAllSystemGLTextures();
}

void ResourceSystem::releaseGLTexturesByScheme(String schemeName)
{
    if(schemeName.isEmpty()) return;

    PathTreeIterator<TextureScheme::Index> iter(textureScheme(schemeName).index().leafNodes());
    while(iter.hasNext())
    {
        TextureManifest &manifest = iter.next();
        if(manifest.hasTexture())
        {
            manifest.texture().releaseGLTextures();
        }
    }
}

void ResourceSystem::clearAllTextureSpecs()
{
    d->clearAllTextureSpecs();
}

void ResourceSystem::pruneUnusedTextureSpecs()
{
    if(Sys_IsShuttingDown()) return;

    int numPruned = 0;
    numPruned += d->pruneUnusedTextureSpecs(TST_GENERAL);
    numPruned += d->pruneUnusedTextureSpecs(TST_DETAIL);

    LOGDEV_RES_VERBOSE("Pruned %i unused texture variant %s")
        << numPruned << (numPruned == 1? "specification" : "specifications");
}

TextureVariantSpec const &ResourceSystem::textureSpec(texturevariantusagecontext_t tc,
    int flags, byte border, int tClass, int tMap, int wrapS, int wrapT, int minFilter,
    int magFilter, int anisoFilter, dd_bool mipmapped, dd_bool gammaCorrection,
    dd_bool noStretch, dd_bool toAlpha)
{
    TextureVariantSpec *tvs =
        d->textureSpec(tc, flags, border, tClass, tMap, wrapS, wrapT, minFilter,
                       magFilter, anisoFilter, mipmapped, gammaCorrection,
                       noStretch, toAlpha);

#ifdef DENG_DEBUG
    if(tClass || tMap)
    {
        DENG2_ASSERT(tvs->variant.flags & TSF_HAS_COLORPALETTE_XLAT);
        DENG2_ASSERT(tvs->variant.tClass == tClass);
        DENG2_ASSERT(tvs->variant.tMap == tMap);
    }
#endif

    return *tvs;
}

TextureVariantSpec &ResourceSystem::detailTextureSpec(float contrast)
{
    return *d->detailTextureSpec(contrast);
}

FontScheme &ResourceSystem::fontScheme(String name) const
{
    LOG_AS("ResourceSystem::fontScheme");
    if(!name.isEmpty())
    {
        FontSchemes::iterator found = d->fontSchemes.find(name.toLower());
        if(found != d->fontSchemes.end())
        {
            return **found;
        }
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw UnknownSchemeError("ResourceSystem::fontScheme", "No scheme found matching '" + name + "'");
}

bool ResourceSystem::knownFontScheme(String name) const
{
    if(!name.isEmpty())
    {
        return d->fontSchemes.contains(name.toLower());
    }
    return false;
}

ResourceSystem::FontSchemes const &ResourceSystem::allFontSchemes() const
{
    return d->fontSchemes;
}

bool ResourceSystem::hasFont(de::Uri const &path) const
{
    try
    {
        fontManifest(path);
        return true;
    }
    catch(MissingManifestError const &)
    {} // Ignore this error.
    return false;
}

FontManifest &ResourceSystem::fontManifest(de::Uri const &uri) const
{
    LOG_AS("ResourceSystem::findFont");

    // Perform the search.
    // Is this a URN? (of the form "urn:schemename:uniqueid")
    if(!uri.scheme().compareWithoutCase("urn"))
    {
        String const &pathStr = uri.path().toStringRef();
        int uIdPos = pathStr.indexOf(':');
        if(uIdPos > 0)
        {
            String schemeName = pathStr.left(uIdPos);
            int uniqueId      = pathStr.mid(uIdPos + 1 /*skip delimiter*/).toInt();

            try
            {
                return fontScheme(schemeName).findByUniqueId(uniqueId);
            }
            catch(FontScheme::NotFoundError const &)
            {} // Ignore, we'll throw our own...
        }
    }
    else
    {
        // No, this is a URI.
        String const &path = uri.path();

        // Does the user want a manifest in a specific scheme?
        if(!uri.scheme().isEmpty())
        {
            try
            {
                return fontScheme(uri.scheme()).find(path);
            }
            catch(FontScheme::NotFoundError const &)
            {} // Ignore, we'll throw our own...
        }
        else
        {
            // No, check each scheme in priority order.
            foreach(FontScheme *scheme, d->fontSchemeCreationOrder)
            {
                try
                {
                    return scheme->find(path);
                }
                catch(FontScheme::NotFoundError const &)
                {} // Ignore, we'll throw our own...
            }
        }
    }

    /// @throw MissingManifestError Failed to locate a matching manifest.
    throw MissingManifestError("ResourceSystem::findFont", "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

FontManifest &ResourceSystem::toFontManifest(fontid_t id) const
{
    if(id > 0 && id <= d->fontManifestCount)
    {
        duint32 idx = id - 1; // 1-based index.
        if(d->fontManifestIdMap[idx])
        {
            return *d->fontManifestIdMap[idx];
        }

        // Internal bookeeping error.
        DENG2_ASSERT(false);
    }

    /// @throw UnknownIdError The specified manifest id is invalid.
    throw UnknownFontIdError("ResourceSystem::toFontManifest", QString("Invalid font ID %1, valid range [1..%2)").arg(id).arg(d->fontManifestCount + 1));
}

ResourceSystem::AllFonts const &ResourceSystem::allFonts() const
{
    return d->fonts;
}

AbstractFont *ResourceSystem::newFontFromDef(ded_compositefont_t const &def)
{
    LOG_AS("ResourceSystem::newFontFromDef");

    if(!def.uri) return 0;
    de::Uri const &uri = *reinterpret_cast<de::Uri *>(def.uri);

    try
    {
        // Create/retrieve a manifest for the would-be font.
        FontManifest &manifest = declareFont(uri);

        if(manifest.hasResource())
        {
            if(CompositeBitmapFont *compFont = manifest.resource().maybeAs<CompositeBitmapFont>())
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
        if(manifest.hasResource())
        {
            if(verbose >= 1)
            {
                LOG_RES_VERBOSE("New font \"%s\"")
                    << manifest.composeUri();
            }
            return &manifest.resource();
        }

        LOG_RES_WARNING("Failed defining new Font for \"%s\"")
            << NativePath(uri.asText()).pretty();
    }
    catch(UnknownSchemeError const &er)
    {
        LOG_RES_WARNING("Failed declaring font \"%s\": %s")
            << NativePath(uri.asText()).pretty() << er.asText();
    }
    catch(FontScheme::InvalidPathError const &er)
    {
        LOG_RES_WARNING("Failed declaring font \"%s\": %s")
            << NativePath(uri.asText()).pretty() << er.asText();
    }

    return 0;
}

AbstractFont *ResourceSystem::newFontFromFile(de::Uri const &uri, String filePath)
{
    LOG_AS("ResourceSystem::newFontFromFile");

    if(!d->fileSys().accessFile(de::Uri::fromNativePath(filePath)))
    {
        LOGDEV_RES_WARNING("Ignoring invalid filePath: ") << filePath;
        return 0;
    }

    try
    {
        // Create/retrieve a manifest for the would-be font.
        FontManifest &manifest = declareFont(uri);

        if(manifest.hasResource())
        {
            if(BitmapFont *bmapFont = manifest.resource().maybeAs<BitmapFont>())
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
        if(manifest.hasResource())
        {
            if(verbose >= 1)
            {
                LOG_RES_VERBOSE("New font \"%s\"")
                    << manifest.composeUri();
            }
            return &manifest.resource();
        }

        LOG_RES_WARNING("Failed defining new Font for \"%s\"")
            << NativePath(uri.asText()).pretty();
    }
    catch(UnknownSchemeError const &er)
    {
        LOG_RES_WARNING("Failed declaring font \"%s\": %s")
            << NativePath(uri.asText()).pretty() << er.asText();
    }
    catch(FontScheme::InvalidPathError const &er)
    {
        LOG_RES_WARNING("Failed declaring font \"%s\": %s")
            << NativePath(uri.asText()).pretty() << er.asText();
    }

    return 0;
}

void ResourceSystem::releaseFontGLTexturesByScheme(String schemeName)
{
    if(schemeName.isEmpty()) return;

    PathTreeIterator<FontScheme::Index> iter(fontScheme(schemeName).index().leafNodes());
    while(iter.hasNext())
    {
        FontManifest &manifest = iter.next();
        if(manifest.hasResource())
        {
            manifest.resource().glDeinit();
        }
    }
}

Model &ResourceSystem::model(modelid_t id)
{
    if(Model *model = d->modelForId(id))
    {
        return *model;
    }
    /// @throw MissingResourceError An unknown/invalid id was specified.
    throw MissingResourceError("ResourceSystem::model", "Invalid id " + String::number(id));
}

bool ResourceSystem::hasModelDef(de::String id) const
{
    if(!id.isEmpty())
    {
        foreach(ModelDef const &modef, d->modefs)
        {
            if(!id.compareWithoutCase(modef.id))
            {
                return true;
            }
        }
    }
    return false;
}

ModelDef &ResourceSystem::modelDef(int index)
{
    if(index >= 0 && index < modelDefCount())
    {
        return d->modefs[index];
    }
    /// @throw MissingModelDefError An unknown model definition was referenced.
    throw MissingModelDefError("ResourceSystem::modelDef", "Invalid index #" + String::number(index) + ", valid range " + Rangeui(0, modelDefCount()).asText());
}

ModelDef &ResourceSystem::modelDef(String id)
{
    if(!id.isEmpty())
    {
        foreach(ModelDef const &modef, d->modefs)
        {
            if(!id.compareWithoutCase(modef.id))
            {
                return const_cast<ModelDef &>(modef);
            }
        }
    }
    /// @throw MissingModelDefError An unknown model definition was referenced.
    throw MissingModelDefError("ResourceSystem::modelDef", "Invalid id '" + id + "'");
}

ModelDef *ResourceSystem::modelDefForState(int stateIndex, int select)
{
    if(stateIndex < 0 || stateIndex >= defs.states.size())
    {
        return 0;
    }
    if(stateIndex < 0 || stateIndex >= d->stateModefs.count())
    {
        return 0;
    }
    if(d->stateModefs[stateIndex] < 0)
    {
        return 0;
    }

    DENG2_ASSERT(d->stateModefs[stateIndex] >= 0);
    DENG2_ASSERT(d->stateModefs[stateIndex] < d->modefs.count());

    ModelDef *def = &d->modefs[d->stateModefs[stateIndex]];
    if(select)
    {
        // Choose the correct selector, or selector zero if the given one not available.
        int const mosel = select & DDMOBJ_SELECTOR_MASK;
        for(ModelDef *it = def; it; it = it->selectNext)
        {
            if(it->select == mosel)
            {
                return it;
            }
        }
    }

    return def;
}

int ResourceSystem::modelDefCount() const
{
    return d->modefs.count();
}

void ResourceSystem::initModels()
{
    LOG_AS("ResourceSystem");

    if(CommandLine_Check("-nomd2"))
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

    // There can't be more modeldefs than there are DED Models.
    d->modefs.resize(defs.models.size());

    // Clear the stateid => modeldef LUT.
    d->stateModefs.resize(runtimeDefs.states.size());
    for(int i = 0; i < runtimeDefs.states.size(); ++i)
    {
        d->stateModefs[i] = -1;
    }

    // Read in the model files and their data.
    // Use the latest definition available for each sprite ID.
    for(int i = int(defs.models.size()) - 1; i >= 0; --i)
    {
        if(!(i % 100))
        {
            // This may take a while, so keep updating the progress.
            Con_SetProgress(130 + 70*(defs.models.size() - i)/defs.models.size());
        }

        d->setupModel(defs.models[i]);
    }

    // Create interlinks. Note that the order in which the defs were loaded
    // is important. We want to allow "patch" definitions, right?

    // For each modeldef we will find the "next" def.
    for(int i = d->modefs.count() - 1; i >= 0; --i)
    {
        ModelDef *me = &d->modefs[i];

        float minmark = 2; // max = 1, so this is "out of bounds".

        ModelDef *closest = 0;
        for(int k = d->modefs.count() - 1; k >= 0; --k)
        {
            ModelDef *other = &d->modefs[k];

            /// @todo Need an index by state. -jk
            if(other->state != me->state) continue;

            // Same state and a bigger order are the requirements.
            if(other->def.order() > me->def.order() && // Defined after me.
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
    for(int i = d->modefs.count() - 1; i >= 0; --i)
    {
        ModelDef *me = &d->modefs[i];

        int minsel = DDMAXINT;

        ModelDef *closest = 0;

        // Start scanning from the next definition.
        for(int k = d->modefs.count() - 1; k >= 0; --k)
        {
            ModelDef *other = &d->modefs[k];

            // Same state and a bigger order are the requirements.
            if(other->state == me->state &&
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

int ResourceSystem::indexOf(ModelDef const *modelDef)
{
    int index = int(modelDef - &d->modefs[0]);
    if(index >= 0 && index < d->modefs.count())
    {
        return index;
    }
    return -1;
}

void ResourceSystem::setModelDefFrame(ModelDef &modef, int frame)
{
    for(uint i = 0; i < modef.subCount(); ++i)
    {
        SubmodelDef &subdef = modef.subModelDef(i);
        if(subdef.modelId == NOMODELID) continue;

        // Modify the modeldef itself: set the current frame.
        subdef.frame = frame % model(subdef.modelId).frameCount();
    }
}

#endif // __CLIENT__

void ResourceSystem::clearAllAnimGroups()
{
    qDeleteAll(d->animGroups);
    d->animGroups.clear();
}

int ResourceSystem::animGroupCount()
{
    return d->animGroups.count();
}

AnimGroup *ResourceSystem::animGroup(int uniqueId)
{
    LOG_AS("ResourceSystem::animGroup");
    if(uniqueId > 0 && uniqueId <= d->animGroups.count())
    {
        return d->animGroups.at(uniqueId - 1);
    }
    LOGDEV_RES_WARNING("Invalid group #%i, returning NULL") << uniqueId;
    return 0;
}

AnimGroup &ResourceSystem::newAnimGroup(int flags)
{
    LOG_AS("ResourceSystem");
    int const uniqueId = d->animGroups.count() + 1; // 1-based.
    // Allocating one by one is inefficient but it doesn't really matter.
    d->animGroups.append(new AnimGroup(uniqueId, flags));
    return *d->animGroups.last();
}

struct SpriteFrameDef
{
    byte frame[2];
    byte rotation[2];
    Material *mat;
};

struct SpriteDef
{
    String name;
    typedef QList<SpriteFrameDef> Frames;
    Frames frames;

    SpriteDef(String const &name = "") : name(name)
    {}

    SpriteFrameDef &addFrame(SpriteFrameDef const &frame = SpriteFrameDef()) {
        frames.append(frame);
        return frames.last();
    }
};

// Tempory storage, used when reading sprite definitions.
typedef QHash<String, SpriteDef> SpriteDefs;

/**
 * In DOOM, a sprite frame is a patch texture contained in a lump existing
 * between the S_START and S_END marker lumps (in WAD) whose lump name matches
 * the following pattern:
 *
 *      NAME|A|R(A|R) (for example: "TROOA0" or "TROOA2A8")
 *
 * NAME: Four character name of the sprite.
 * A: Animation frame ordinal 'A'... (ASCII).
 * R: Rotation angle 0...G
 *    0 : Use this frame for ALL angles.
 *    1...8: Angle of rotation in 45 degree increments.
 *    A...G: Angle of rotation in 22.5 degree increments.
 *
 * The second set of (optional) frame and rotation characters instruct that the
 * same sprite frame is to be used for an additional frame but that the sprite
 * patch should be flipped horizontally (right to left) during the loading phase.
 *
 * Sprite rotation 0 is facing the viewer, rotation 1 is one half-angle turn
 * CLOCKWISE around the axis. This is not the same as the angle, which increases
 * counter clockwise (protractor).
 */
static SpriteDefs generateSpriteDefs()
{
    ResourceSystem &resSys = App_ResourceSystem();
    TextureScheme::Index const &spriteTexIndex = resSys.textureScheme("Sprites").index();

    SpriteDefs spriteDefs;
    spriteDefs.reserve(spriteTexIndex.leafNodes().count() / 8); // overestimate

    PathTreeIterator<TextureScheme::Index> iter(spriteTexIndex.leafNodes());
    while(iter.hasNext())
    {
        TextureManifest &manifest = iter.next();

        String const desc = QString(QByteArray::fromPercentEncoding(manifest.path().toUtf8()));
        String const name = desc.left(4).toLower();

        // Have we already encountered this name?
        SpriteDef *def = &spriteDefs[name];
        if(def->frames.isEmpty())
        {
            // An entirely new sprite.
            def->name = name;
        }

        // Add the frame(s).
        int const frameNumber    = desc.at(4).toUpper().unicode() - QChar('A').unicode() + 1;
        int const rotationNumber = toSpriteRotation(desc.at(5));

        SpriteFrameDef *frameDef = 0;
        for(int i = 0; i < def->frames.count(); ++i)
        {
            SpriteFrameDef &cand = def->frames[i];
            if(cand.frame[0]    == frameNumber &&
               cand.rotation[0] == rotationNumber)
            {
                frameDef = &cand;
                break;
            }
        }

        if(!frameDef)
        {
            // A new frame.
            frameDef = &def->addFrame();
        }

        frameDef->mat         = &resSys.material(de::Uri("Sprites", manifest.path()));
        frameDef->frame[0]    = frameNumber;
        frameDef->rotation[0] = rotationNumber;

        // Mirrored?
        if(desc.length() >= 8)
        {
            frameDef->frame[1]    = desc.at(6).toUpper().unicode() - QChar('A').unicode() + 1;
            frameDef->rotation[1] = toSpriteRotation(desc.at(7));
        }
        else
        {
            frameDef->frame[1]    = 0;
            frameDef->rotation[1] = 0;
        }
    }

    return spriteDefs;
}

void ResourceSystem::initSprites()
{
    Time begunAt;

    LOG_AS("ResourceSystem");
    LOG_RES_VERBOSE("Building sprites...");

    d->clearSprites();

    SpriteDefs defs = generateSpriteDefs();

    if(!defs.isEmpty())
    {
        // Build the final sprites.
        int customIdx = 0;
        foreach(SpriteDef const &def, defs)
        {
            spritenum_t spriteId = Def_GetSpriteNum(def.name.toUtf8().constData());
            if(spriteId == -1)
            {
                spriteId = runtimeDefs.sprNames.size() + customIdx++;
            }

            Instance::SpriteGroup &group = d->newSpriteGroup(spriteId);

            Sprite sprTemp[29];

            int maxSprite = -1;
            foreach(SpriteFrameDef const &frameDef, def.frames)
            {
                int frame = frameDef.frame[0] - 1;
                DENG2_ASSERT(frame >= 0);
                if(frame < 29)
                {
                    sprTemp[frame].newViewAngle(frameDef.mat, frameDef.rotation[0], false);
                    if(frame > maxSprite)
                    {
                        maxSprite = frame;
                    }
                }

                if(frameDef.frame[1])
                {
                    frame = frameDef.frame[1] - 1;
                    DENG2_ASSERT(frame >= 0);
                    if(frame < 29)
                    {
                        sprTemp[frame].newViewAngle(frameDef.mat, frameDef.rotation[1], true);
                        if(frame > maxSprite)
                        {
                            maxSprite = frame;
                        }
                    }
                }
            }
            ++maxSprite;

            // Duplicate view angles to complete the rotation set (if defined).
            for(int frame = 0; frame < maxSprite; ++frame)
            {
                Sprite &sprite = sprTemp[frame];

                if(sprite.viewAngleCount() < 2)
                    continue;

                for(int rot = 0; rot < Sprite::max_angles / 2; ++rot)
                {
                    if(!sprite.hasViewAngle(rot * 2 + 1))
                    {
                        SpriteViewAngle const &src = sprite.viewAngle(rot * 2);
                        sprite.newViewAngle(src.material, rot * 2 + 2, src.mirrorX);
                    }
                    if(!sprite.hasViewAngle(rot * 2))
                    {
                        SpriteViewAngle const &src = sprite.viewAngle(rot * 2 + 1);
                        sprite.newViewAngle(src.material, rot * 2 + 1, src.mirrorX);
                    }
                }
            }

            for(int k = 0; k < maxSprite; ++k)
            {
                group.sprites.append(new Sprite(sprTemp[k]));
            }
        }
    }

    // We're done with the definitions.
    defs.clear();

    LOG_RES_VERBOSE("Sprites built in %.2f seconds") << begunAt.since();
}

void ResourceSystem::clearAllColorPalettes()
{
    d->colorPaletteNames.clear();

    qDeleteAll(d->colorPalettes);
    d->colorPalettes.clear();

    d->defaultColorPalette = 0;
}

int ResourceSystem::colorPaletteCount() const
{
    return d->colorPalettes.count();
}

ColorPalette &ResourceSystem::colorPalette(colorpaletteid_t id) const
{
    // Choose the default palette?
    if(!id)
    {
        id = d->defaultColorPalette;
    }

    Instance::ColorPalettes::const_iterator found = d->colorPalettes.find(id);
    if(found != d->colorPalettes.end())
    {
        return *found.value();
    }
    /// @throw MissingResourceError An unknown/invalid id was specified.
    throw MissingResourceError("ResourceSystem::colorPalette", "Invalid id " + String::number(id));
}

String ResourceSystem::colorPaletteName(ColorPalette &palette) const
{
    QList<String> names = d->colorPaletteNames.keys(&palette);
    if(!names.isEmpty())
    {
        return names.first();
    }
    return String();
}

bool ResourceSystem::hasColorPalette(String name) const
{
    return d->colorPaletteNames.contains(name);
}

ColorPalette &ResourceSystem::colorPalette(String name) const
{
    Instance::ColorPaletteNames::const_iterator found = d->colorPaletteNames.find(name);
    if(found != d->colorPaletteNames.end())
    {
        return *found.value();
    }
    /// @throw MissingResourceError An unknown name was specified.
    throw MissingResourceError("ResourceSystem::colorPalette", "Unknown name '" + name + "'");
}

void ResourceSystem::addColorPalette(ColorPalette &newPalette, String const &name)
{
    // Do we already own this palette?
    Instance::ColorPalettes::const_iterator found = d->colorPalettes.find(newPalette.id());
    if(found != d->colorPalettes.end())
    {
        return;
    }

    d->colorPalettes.insert(newPalette.id(), &newPalette);

#ifdef __CLIENT__
    // Observe changes to the color table so we can schedule texture updates.
    newPalette.audienceForColorTableChange += d;
#endif

    if(!name.isEmpty())
    {
        d->colorPaletteNames.insert(name, &newPalette);
    }

    // If this is the first palette automatically set it as the default.
    if(d->colorPalettes.count() == 1)
    {
        d->defaultColorPalette = newPalette.id();
    }
}

colorpaletteid_t ResourceSystem::defaultColorPalette() const
{
    return d->defaultColorPalette;
}

void ResourceSystem::setDefaultColorPalette(ColorPalette *newDefaultPalette)
{
    d->defaultColorPalette = newDefaultPalette? newDefaultPalette->id().asUInt32() : 0;
}

#ifdef __CLIENT__

void ResourceSystem::purgeCacheQueue()
{
    qDeleteAll(d->cacheQueue);
    d->cacheQueue.clear();
}

void ResourceSystem::processCacheQueue()
{
    d->processCacheQueue();
}

void ResourceSystem::cache(Material &material, MaterialVariantSpec const &spec,
    bool cacheGroups)
{
    d->queueCacheTasksForMaterial(material, spec, cacheGroups);
}

void ResourceSystem::cache(spritenum_t spriteId, MaterialVariantSpec const &spec)
{
    d->queueCacheTasksForSprite(spriteId, spec);
}

void ResourceSystem::cache(ModelDef *modelDef)
{
    if(!modelDef) return;
    d->queueCacheTasksForModel(*modelDef);
}

MaterialVariantSpec const &ResourceSystem::materialSpec(MaterialContextId contextId,
    int flags, byte border, int tClass, int tMap, int wrapS, int wrapT,
    int minFilter, int magFilter, int anisoFilter,
    bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
{
    return d->getMaterialSpecForContext(contextId, flags, border, tClass, tMap,
                                        wrapS, wrapT, minFilter, magFilter, anisoFilter,
                                        mipmapped, gammaCorrection, noStretch, toAlpha);
}

static int findSpriteOwner(thinker_t *th, void *context)
{
    mobj_t *mo = reinterpret_cast<mobj_t *>(th);
    int const sprite = *static_cast<int *>(context);

    if(mo->type >= 0 && mo->type < defs.mobjs.size())
    {
        /// @todo optimize: traverses the entire state list!
        for(int i = 0; i < defs.states.size(); ++i)
        {
            if(runtimeDefs.stateInfo[i].owner != &runtimeDefs.mobjInfo[mo->type])
            {
                continue;
            }

            state_t *state = Def_GetState(i);
            DENG2_ASSERT(state != 0);

            if(state->sprite == sprite)
            {
                return true; // Found one.
            }
        }
    }

    return false; // Continue iteration.
}

/**
 * @note The skins are also bound here once so they should be ready for use
 *       the next time they are needed.
 */
static int cacheModelsForMobj(thinker_t *th, void *context)
{
    mobj_t *mo = (mobj_t *) th;
    ResourceSystem &resSys = *static_cast<ResourceSystem *>(context);

    if(!(useModels && precacheSkins))
        return true;

    // Check through all the model definitions.
    for(int i = 0; i < resSys.modelDefCount(); ++i)
    {
        ModelDef &modef = resSys.modelDef(i);

        if(!modef.state) continue;
        if(mo->type < 0 || mo->type >= defs.mobjs.size()) continue; // Hmm?
        if(runtimeDefs.stateInfo[runtimeDefs.states.indexOf(modef.state)].owner != &runtimeDefs.mobjInfo[mo->type]) continue;

        resSys.cache(&modef);
    }

    return false; // Used as iterator.
}

void ResourceSystem::cacheForCurrentMap()
{
    // Don't precache when playing a demo (why not? -ds).
    if(playback) return;

    Map &map = App_WorldSystem().map();

    if(precacheMapMaterials)
    {
        MaterialVariantSpec const &spec = Rend_MapSurfaceMaterialSpec();

        foreach(Line *line, map.lines())
        for(int i = 0; i < 2; ++i)
        {
            LineSide &side = line->side(i);
            if(!side.hasSections()) continue;

            if(side.middle().hasMaterial())
                cache(side.middle().material(), spec);

            if(side.top().hasMaterial())
                cache(side.top().material(), spec);

            if(side.bottom().hasMaterial())
                cache(side.bottom().material(), spec);
        }

        foreach(Sector *sector, map.sectors())
        {
            // Skip sectors with no line sides as their planes will never be drawn.
            if(!sector->sideCount()) continue;

            foreach(Plane *plane, sector->planes())
            {
                if(plane->surface().hasMaterial())
                    cache(plane->surface().material(), spec);
            }
        }
    }

    if(precacheSprites)
    {
        MaterialVariantSpec const &spec = Rend_SpriteMaterialSpec();

        for(int i = 0; i < spriteCount(); ++i)
        {
            if(map.thinkers().iterate(reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                                      0x1/*mobjs are public*/,
                                      findSpriteOwner, &i))
            {
                // This sprite is used by some state of at least one mobj.
                cache(spritenum_t(i), spec);
            }
        }
    }

     // Sky models usually have big skins.
    theSky->cacheDrawableAssets();

    // Precache model skins?
    if(useModels && precacheSkins)
    {
        // All mobjs are public.
        map.thinkers().iterate(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1,
                               cacheModelsForMobj, this);
    }
}

#endif // __CLIENT__

NativePath ResourceSystem::nativeSavePath()
{
    return d->nativeSavePath;
}

bool ResourceSystem::convertLegacySavegames(String const &gameId, String const &sourcePath)
{
    // A converter plugin is required.
    if(!Plug_CheckForHook(HOOK_SAVEGAME_CONVERT)) return false;

    // Populate /legacysavegames/<gameId> with new savegames which may have appeared.
    d->locateLegacySavegames(gameId);

    bool didSchedule = false;
    if(sourcePath.isEmpty())
    {
        // Process all legacy savegames.
        if(Folder const *saveFolder = App::rootFolder().tryLocate<Folder>(String("legacysavegames") / gameId))
        {
            /// @todo File name pattern matching should not be done here. This is to prevent
            /// attempting to convert Hexen's map state side car files separately when this
            /// is called from Doomsday Script (in bootstrap.de).
            Game const &game = App_Games().byIdentityKey(gameId);
            QRegExp namePattern(game.legacySavegameNameExp(), Qt::CaseInsensitive);
            if(namePattern.isValid() && !namePattern.isEmpty())
            {
                DENG2_FOR_EACH_CONST(Folder::Contents, i, saveFolder->contents())
                {
                    if(namePattern.exactMatch(i->first.fileName()))
                    {
                        // Schedule the conversion task.
                        d->beginConvertLegacySavegame(i->second->path(), gameId);
                        didSchedule = true;
                    }
                }
            }
        }
    }
    // Just the one legacy savegame.
    else if(App::rootFolder().has(sourcePath))
    {
        // Schedule the conversion task.
        d->beginConvertLegacySavegame(sourcePath, gameId);
        didSchedule = true;
    }

    return didSchedule;
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

    for(int i = 0; i < 256; ++i)
    {
        texGammaLut[i] = byte(255.0f * pow(double(i / 255.0f), invGamma));
    }
}

template <typename ManifestType>
static bool pathBeginsWithComparator(ManifestType const &manifest, void *context)
{
    Path const *path = reinterpret_cast<Path*>(context);
    /// @todo Use PathTree::Node::compare()
    return manifest.path().toStringRef().beginsWith(*path, Qt::CaseInsensitive);
}

/**
 * @todo This logic should be implemented in de::PathTree -ds
 */
template <typename SchemeType, typename ManifestType>
static int collectManifestsInScheme(SchemeType const &scheme,
    bool (*predicate)(ManifestType const &manifest, void *context), void *context,
    QList<ManifestType *> *storage = 0)
{
    int count = 0;
    PathTreeIterator<typename SchemeType::Index> iter(scheme.index().leafNodes());
    while(iter.hasNext())
    {
        ManifestType &manifest = iter.next();
        if(predicate(manifest, context))
        {
            count += 1;
            if(storage) // Store mode?
            {
                storage->append(&manifest);
            }
        }
    }
    return count;
}

static QList<MaterialManifest *> collectMaterialManifests(MaterialScheme *scheme,
    Path const &path, QList<MaterialManifest *> *storage = 0)
{
    int count = 0;

    if(scheme)
    {
        // Consider resources in the specified scheme only.
        count += collectManifestsInScheme<MaterialScheme, MaterialManifest>(*scheme,
                     pathBeginsWithComparator, (void *)&path, storage);
    }
    else
    {
        // Consider resources in any scheme.
        foreach(MaterialScheme *scheme, App_ResourceSystem().allMaterialSchemes())
        {
            count += collectManifestsInScheme<MaterialScheme, MaterialManifest>(*scheme,
                         pathBeginsWithComparator, (void *)&path, storage);
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
    return collectMaterialManifests(scheme, path, &result);
}

static QList<TextureManifest *> collectTextureManifests(TextureScheme *scheme,
    Path const &path, QList<TextureManifest *> *storage = 0)
{
    int count = 0;

    if(scheme)
    {
        // Consider resources in the specified scheme only.
        count += collectManifestsInScheme<TextureScheme, TextureManifest>(*scheme,
                    pathBeginsWithComparator, (void *)&path, storage);
    }
    else
    {
        // Consider resources in any scheme.
        foreach(TextureScheme *scheme, App_ResourceSystem().allTextureSchemes())
        {
            count += collectManifestsInScheme<TextureScheme, TextureManifest>(*scheme,
                        pathBeginsWithComparator, (void *)&path, storage);
        }
    }

    // Are we done?
    if(storage) return *storage;

    // Collect and populate.
    QList<TextureManifest *> result;
    if(count == 0) return result;

#ifdef DENG2_QT_4_7_OR_NEWER
    result.reserve(count);
#endif
    return collectTextureManifests(scheme, path, &result);
}

#ifdef __CLIENT__
static QList<FontManifest *> collectFontManifests(FontScheme *scheme,
    Path const &path, QList<FontManifest *> *storage = 0)
{
    int count = 0;

    if(scheme)
    {
        // Consider resources in the specified scheme only.
        count += collectManifestsInScheme<FontScheme, FontManifest>(*scheme,
                    pathBeginsWithComparator, (void *)&path, storage);
    }
    else
    {
        // Consider resources in any scheme.
        foreach(FontScheme *scheme, App_ResourceSystem().allFontSchemes())
        {
            count += collectManifestsInScheme<FontScheme, FontManifest>(*scheme,
                        pathBeginsWithComparator, (void *)&path, storage);
        }
    }

    // Are we done?
    if(storage) return *storage;

    // Collect and populate.
    QList<FontManifest *> result;
    if(count == 0) return result;

#ifdef DENG2_QT_4_7_OR_NEWER
    result.reserve(count);
#endif
    return collectFontManifests(scheme, path, &result);
}
#endif // __CLIENT__

/**
 * Decode and then lexicographically compare the two manifest paths,
 * returning @c true if @a is less than @a b.
 */
template <typename ManifestType>
static bool compareManifestPathsAscending(ManifestType const *a, ManifestType const *b)
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
static int printMaterialIndex2(MaterialScheme *scheme, Path const &like,
    de::Uri::ComposeAsTextFlags composeUriFlags)
{
    QList<MaterialManifest *> found = collectMaterialManifests(scheme, like);
    if(found.isEmpty()) return 0;

    bool const printSchemeName = !(composeUriFlags & de::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known materials";
    if(!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if(!like.isEmpty())
        heading += " like \"" _E(b) + like.toStringRef() + _E(.) "\"";
    LOG_RES_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index.
    qSort(found.begin(), found.end(), compareManifestPathsAscending<MaterialManifest>);
    int const numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    foreach(MaterialManifest *manifest, found)
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

static void printMaterialIndex(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printMaterialIndex2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if(App_ResourceSystem().knownMaterialScheme(search.scheme()))
    {
        printTotal = printMaterialIndex2(&App_ResourceSystem().materialScheme(search.scheme()),
                                         search.path(), flags | de::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach(MaterialScheme *scheme, App_ResourceSystem().allMaterialSchemes())
        {
            int numPrinted = printMaterialIndex2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if(numPrinted)
            {
                LOG_MSG(_E(R));
                printTotal += numPrinted;
            }
        }
    }
    LOG_RES_MSG("Found " _E(b) "%i" _E(.) " %s.") << printTotal << (printTotal == 1? "material" : "materials in total");
}

/**
 * @param scheme    Texture subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Texture path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printTextureIndex2(TextureScheme *scheme, Path const &like,
    de::Uri::ComposeAsTextFlags composeUriFlags)
{
    QList<TextureManifest *> found = collectTextureManifests(scheme, like);
    if(found.isEmpty()) return 0;

    bool const printSchemeName = !(composeUriFlags & de::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known textures";
    if(!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if(!like.isEmpty())
        heading += " like \"" _E(b) + like.toStringRef() + _E(.) "\"";
    LOG_RES_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index key.
    qSort(found.begin(), found.end(), compareManifestPathsAscending<TextureManifest>);
    int numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    foreach(TextureManifest *manifest, found)
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

static void printTextureIndex(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printTextureIndex2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if(App_ResourceSystem().knownTextureScheme(search.scheme()))
    {
        printTotal = printTextureIndex2(&App_ResourceSystem().textureScheme(search.scheme()),
                                        search.path(), flags | de::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach(TextureScheme *scheme, App_ResourceSystem().allTextureSchemes())
        {
            int numPrinted = printTextureIndex2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if(numPrinted)
            {
                LOG_RES_MSG(_E(R));
                printTotal += numPrinted;
            }
        }
    }
    LOG_RES_MSG("Found " _E(b) "%i" _E(.) " %s") << printTotal << (printTotal == 1? "texture" : "textures in total");
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
    QList<FontManifest *> found = collectFontManifests(scheme, like);
    if(found.isEmpty()) return 0;

    bool const printSchemeName = !(composeUriFlags & de::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known fonts";
    if(!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if(!like.isEmpty())
        heading += " like \"" _E(b) + like.toStringRef() + _E(.) "\"";
    LOG_RES_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index.
    qSort(found.begin(), found.end(), compareManifestPathsAscending<FontManifest>);
    int numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    foreach(FontManifest *manifest, found)
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

static void printFontIndex(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printFontIndex2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if(App_ResourceSystem().knownFontScheme(search.scheme()))
    {
        printTotal = printFontIndex2(&App_ResourceSystem().fontScheme(search.scheme()),
                                     search.path(), flags | de::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach(FontScheme *scheme, App_ResourceSystem().allFontSchemes())
        {
            int numPrinted = printFontIndex2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if(numPrinted)
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
    return App_ResourceSystem().knownMaterialScheme(name);
}

static bool isKnownTextureSchemeCallback(String name)
{
    return App_ResourceSystem().knownTextureScheme(name);
}

#ifdef __CLIENT__
static bool isKnownFontSchemeCallback(String name)
{
    return App_ResourceSystem().knownFontScheme(name);
}
#endif

D_CMD(ListMaterials)
{
    DENG2_UNUSED(src);

    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1, &isKnownMaterialSchemeCallback);

    if(!search.scheme().isEmpty() &&
       !App_ResourceSystem().knownMaterialScheme(search.scheme()))
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

    if(!search.scheme().isEmpty() &&
       !App_ResourceSystem().knownTextureScheme(search.scheme()))
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
    if(!search.scheme().isEmpty() &&
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
    foreach(MaterialScheme *scheme, App_ResourceSystem().allMaterialSchemes())
    {
        MaterialScheme::Index const &index = scheme->index();

        uint count = index.count();
        LOG_MSG("Scheme: %s (%u %s)")
            << scheme->name() << count << (count == 1? "material" : "materials");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}

D_CMD(PrintTextureStats)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_MSG(_E(b) "Texture Statistics:");
    foreach(TextureScheme *scheme, App_ResourceSystem().allTextureSchemes())
    {
        TextureScheme::Index const &index = scheme->index();

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
    foreach(FontScheme *scheme, App_ResourceSystem().allFontSchemes())
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

D_CMD(InspectSavegame)
{
    DENG2_UNUSED2(src, argc);
    String savePath = argv[1];
    // Append a .save extension if none exists.
    if(savePath.fileNameExtension().isEmpty())
    {
        savePath += ".save";
    }
    // If a game is loaded assume the user is referring to those savegames if not specified.
    if(savePath.fileNamePath().isEmpty() && App_GameLoaded())
    {
        savePath = game::Session::savePath() / savePath;
    }

    if(game::SavedSession const *saved = App::rootFolder().tryLocate<game::SavedSession>(savePath))
    {
        LOG_SCR_MSG("%s") << saved->metadata().asStyledText();
        LOG_SCR_MSG(_E(D) "Resource: " _E(.)_E(i) "\"%s\"") << saved->path();
        return true;
    }

    LOG_WARNING("Failed to locate savegame with \"%s\"") << savePath;
    return false;
}

void ResourceSystem::consoleRegister() // static
{
    C_CMD("listtextures",   "ss",   ListTextures)
    C_CMD("listtextures",   "s",    ListTextures)
    C_CMD("listtextures",   "",     ListTextures)
#ifdef DENG_DEBUG
    C_CMD("texturestats",   NULL,   PrintTextureStats)
#endif

    C_CMD("inspectsavegame", "s",   InspectSavegame)

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

    Texture::consoleRegister();
    Material::consoleRegister();
}

String ResourceSystem::resolveSymbol(String const &symbol) // static
{
    if(!symbol.compare("App.DataPath", Qt::CaseInsensitive))
    {
        return "data";
    }
    else if(!symbol.compare("App.DefsPath", Qt::CaseInsensitive))
    {
        return "defs";
    }
    else if(!symbol.compare("Game.IdentityKey", Qt::CaseInsensitive))
    {
        if(!App_GameLoaded())
        {
            /// @throw de::Uri::ResolveSymbolError  An unresolveable symbol was encountered.
            throw de::Uri::ResolveSymbolError("ResourceSystem::resolveSymbol",
                    "Symbol 'Game' did not resolve (no game loaded)");
        }
        return App_CurrentGame().identityKey();
    }
    else if(!symbol.compare("GamePlugin.Name", Qt::CaseInsensitive))
    {
        if(!App_GameLoaded() || !gx.GetVariable)
        {
            /// @throw de::Uri::ResolveSymbolError  An unresolveable symbol was encountered.
            throw de::Uri::ResolveSymbolError("ResourceSystem::resolveSymbol",
                    "Symbol 'GamePlugin' did not resolve (no game plugin loaded)");
        }
        return String((char *)gx.GetVariable(DD_PLUGIN_NAME));
    }
    else
    {
        /// @throw UnknownSymbolError  An unknown symbol was encountered.
        throw de::Uri::UnknownSymbolError("ResourceSystem::resolveSymbol",
                                          "Symbol '" + symbol + "' is unknown");
    }
}
