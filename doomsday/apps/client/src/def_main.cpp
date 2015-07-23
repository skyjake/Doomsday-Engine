/** @file def_main.cpp  Definition subsystem.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#define DENG_NO_API_MACROS_DEFINITIONS

#include "def_main.h"

#include <cctype>
#include <cstring>
#include <QMap>
#include <QTextStream>
#include <de/findfile.h>
#include <de/App>
#include <de/NativePath>
#include <de/RecordValue>
#include <doomsday/doomsdayapp.h>
#include <doomsday/console/cmd.h>
#include <doomsday/defs/decoration.h>
#include <doomsday/defs/dedfile.h>
#include <doomsday/defs/dedparser.h>
#include <doomsday/defs/material.h>
#include <doomsday/defs/sky.h>
#include <doomsday/defs/state.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include <doomsday/filesys/sys_direc.h>
#include <doomsday/resource/manifest.h>
#include <doomsday/world/xg.h>

#include "dd_main.h"
#include "dd_def.h"

#include "api_def.h"
#include "api_sound.h"

#include "Generator"
#ifdef __CLIENT__
#  include "render/rend_particle.h"
#endif

#include "resource/materialdetaillayer.h"
#include "resource/materialshinelayer.h"
#include "resource/materialtexturelayer.h"
#ifdef __CLIENT__
#  include "resource/materiallightdecoration.h"
#endif

using namespace de;

#define LOOPi(n)    for(i = 0; i < (n); ++i)
#define LOOPk(n)    for(k = 0; k < (n); ++k)

ded_t defs;  ///< The main definitions database.

RuntimeDefs runtimeDefs;

static bool defsInited;
static mobjinfo_t *gettingFor;

static inline FS1 &fileSys()
{
    return App_FileSystem();
}

static inline ResourceSystem &resSys()
{
    return App_ResourceSystem();
}

void RuntimeDefs::clear()
{
    for(dint i = 0; i < sounds.size(); ++i)
    {
        Str_Free(&sounds[i].external);
    }
    sounds.clear();

    mobjInfo.clear();
    states.clear();
    texts.clear();
    stateInfo.clear();
}

void Def_Init()
{
    ::runtimeDefs.clear();
    ::defs.clear();

    // Make the definitions visible in the global namespace.
    App::app().scriptSystem().addNativeModule("Defs", defs.names);
    
    // Constants for definitions.
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SN_SPAWN);
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SN_SEE);
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SN_PAIN);
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SN_MELEE);
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SN_MISSILE);
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SN_CRASH);
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SN_DEATH);
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SN_XDEATH);
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SN_RAISE);

    DENG2_ADD_NUMBER_CONSTANT(defs.names, SDN_ACTIVE);
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SDN_ATTACK);
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SDN_DEATH);
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SDN_PAIN);
    DENG2_ADD_NUMBER_CONSTANT(defs.names, SDN_SEE);
}

void Def_Destroy()
{
    App::app().scriptSystem().removeNativeModule("Defs");

    ::defs.clear();

    // Destroy the databases.
    ::runtimeDefs.clear();

    ::defsInited = false;
}

state_t *Def_GetState(dint num)
{
    if(num >= 0 && num < ::defs.states.size())
    {
        return &::runtimeDefs.states[num];
    }
    return nullptr;  // Not found.
}

ded_compositefont_t *Def_GetCompositeFont(char const *uri)
{
    return ::defs.getCompositeFont(uri);
}

/// @todo $revise-texture-animation
static ded_reflection_t *tryFindReflection(de::Uri const &uri, /* bool hasExternal,*/ bool isCustom)
{
    for(dint i = ::defs.reflections.size() - 1; i >= 0; i--)
    {
        ded_reflection_t &def = ::defs.reflections[i];
        if(!def.material || *def.material !=  uri) continue;

        /*
        if(hasExternal)
        {
            if(!(def.flags & REFF_EXTERNAL)) continue;
        }
        else */
        if(!isCustom)
        {
            if(def.flags & REFF_NO_IWAD) continue;
        }
        else
        {
            if(!(def.flags & REFF_PWAD)) continue;
        }

        return &def;
    }
    return nullptr;  // Not found.
}

/// @todo $revise-texture-animation
static ded_detailtexture_t *tryFindDetailTexture(de::Uri const &uri, /*bool hasExternal,*/ bool isCustom)
{
    for(dint i = ::defs.details.size() - 1; i >= 0; i--)
    {
        ded_detailtexture_t &def = ::defs.details[i];
        for(dint k = 0; k < 2; ++k)
        {
            de::Uri const *matUri = (k == 0 ? def.material1 : def.material2);
            if(!matUri || *matUri != uri) continue;

            /*if(hasExternal)
            {
                if(!(def.flags & DTLF_EXTERNAL)) continue;
            }
            else*/
            if(!isCustom)
            {
                if(def.flags & DTLF_NO_IWAD) continue;
            }
            else
            {
                if(!(def.flags & DTLF_PWAD)) continue;
            }

            return &def;
        }
    }
    return nullptr;  // Not found.
}

ded_ptcgen_t *Def_GetGenerator(de::Uri const &uri)
{
    if(uri.isEmpty()) return nullptr;

    for(dint i = 0; i < ::defs.ptcGens.size(); ++i)
    {
        ded_ptcgen_t *def = &::defs.ptcGens[i];
        if(!def->material) continue;

        // Is this suitable?
        if(*def->material == uri)
            return def;

#if 0  /// @todo $revise-texture-animation
        if(def->flags & PGF_GROUP)
        {
            // Generator triggered by all materials in the (animation) group.
            // A search is necessary only if we know both the used material and
            // the specified material in this definition are in *a* group.
            if(Material_IsGroupAnimated(defMat) && Material_IsGroupAnimated(mat) &&
               &Material_AnimGroup(defMat) == &Material_AnimGroup(mat))
            {
                // Both are in this group! This def will do.
                return def;
            }
        }
#endif
    }

    return nullptr;  // None found.
}

ded_ptcgen_t *Def_GetGenerator(uri_s const *uri)
{
    if(!uri) return nullptr;
    return Def_GetGenerator(reinterpret_cast<de::Uri const &>(*uri));
}

ded_ptcgen_t *Def_GetDamageGenerator(int mobjType)
{
    // Search for a suitable definition.
    for(dint i = 0; i < ::defs.ptcGens.size(); ++i)
    {
        ded_ptcgen_t *def = &::defs.ptcGens[i];

        // It must be for this type of mobj.
        if(def->damageNum == mobjType)
            return def;
    }
    return nullptr;
}

/**
 * The following escape sequences are un-escaped:
 * <pre>
 *     \\n   Newline
 *     \\r   Carriage return
 *     \\t   Tab
 *     \\\_   Space
 *     \\s   Space
 * </pre>
 */
static void Def_InitTextDef(ddtext_t *txt, char const *str)
{
    DENG2_ASSERT(txt);

    // Handle null pointers with "".
    if(!str) str = "";

    txt->text = (char *) M_Calloc(qstrlen(str) + 1);

    char const *in = str;
    char *out = txt->text;
    for(; *in; out++, in++)
    {
        if(*in == '\\')
        {
            in++;

            if(*in == 'n')      *out = '\n'; // Newline.
            else if(*in == 'r') *out = '\r'; // Carriage return.
            else if(*in == 't') *out = '\t'; // Tab.
            else if(*in == '_'
                 || *in == 's') *out = ' '; // Space.
            else
            {
                *out = *in;
            }
        }
        else
        {
            *out = *in;
        }
    }

    // Adjust buffer to fix exactly.
    txt->text = (char *) M_Realloc(txt->text, qstrlen(txt->text) + 1);
}

/**
 * Prints a count with a 2-space indentation.
 */
static String defCountMsg(dint count, String const &label)
{
    if(!::verbose && !count)
        return "";  // Don't print zeros if not verbose.

    return String(_E(Ta) "  %1 " _E(Tb) "%2\n").arg(count).arg(label);
}

/**
 * Read all DD_DEFNS lumps in the primary lump index.
 */
static void Def_ReadLumpDefs()
{
    LOG_AS("Def_ReadLumpDefs");

    LumpIndex const &lumpIndex = fileSys().nameIndex();
    LumpIndex::FoundIndices foundDefns;
    lumpIndex.findAll("DD_DEFNS.lmp", foundDefns);
    DENG2_FOR_EACH_CONST(LumpIndex::FoundIndices, i, foundDefns)
    {
        if(!DED_ReadLump(&defs, *i))
        {
            QByteArray path = NativePath(lumpIndex[*i].container().composePath()).pretty().toUtf8();
            App_Error("Def_ReadLumpDefs: Parse error reading \"%s:DD_DEFNS\".\n", path.constData());
        }
    }

    dint const numProcessedLumps = foundDefns.size();
    if(::verbose && numProcessedLumps > 0)
    {
        LOG_RES_NOTE("Processed %i %s")
                << numProcessedLumps << (numProcessedLumps != 1 ? "lumps" : "lump");
    }
}

/**
 * Uses gettingFor. Initializes the state-owners information.
 */
dint Def_StateForMobj(String const &state)
{
    dint num = ::defs.getStateNum(state);
    if(num < 0) num = 0;

    // State zero is the NULL state.
    if(num > 0)
    {
        ::runtimeDefs.stateInfo[num].owner = ::gettingFor;
        // Scan forward at most 'count' states, or until we hit a state with
        // an owner, or the NULL state.
        dint st, count = 16;
        for(st = ::runtimeDefs.states[num].nextState;
            st > 0 && count-- && !::runtimeDefs.stateInfo[st].owner;
            st = ::runtimeDefs.states[st].nextState)
        {
            ::runtimeDefs.stateInfo[st].owner = ::gettingFor;
        }
    }

    return num;
}

static void readDefinitionFile(String path)
{
    if(path.isEmpty()) return;

    LOG_RES_VERBOSE("Reading \"%s\"") << NativePath(path).pretty();
    Def_ReadProcessDED(&defs, path);
}

/**
 * Attempt to prepend the current work path. If @a src is already absolute do nothing.
 *
 * @param dst  Absolute path written here.
 * @param src  Original path.
 */
static void prependWorkPath(ddstring_t *dst, ddstring_t const *src)
{
    DENG2_ASSERT(dst && src);

    if(!F_IsAbsolute(src))
    {
        char *curPath = Dir_CurrentPath();
        Str_Prepend(dst, curPath);
        Dir_CleanPathStr(dst);
        M_Free(curPath);
        return;
    }

    // Do we need to copy anyway?
    if(dst != src)
    {
        Str_Set(dst, Str_Text(src));
    }
}

/**
 * Returns a URN list (in load order) for all lumps whose name matches the pattern "MAPINFO.lmp".
 */
static QStringList allMapInfoUrns()
{
    QStringList foundPaths;

    // The game's main MAPINFO definitions should be processed first.
    bool ignoreNonCustom = false;
    try
    {
        String mainMapInfo = fileSys().findPath(de::Uri(App_CurrentGame().mainMapInfo()), RLF_MATCH_EXTENSION);
        if(!mainMapInfo.isEmpty())
        {
            foundPaths << mainMapInfo;
            ignoreNonCustom = true;
        }
    }
    catch(FS1::NotFoundError const &)
    {}  // Ignore this error.

    // Process all other lumps named MAPINFO.lmp
    LumpIndex const &lumpIndex = fileSys().nameIndex();
    LumpIndex::FoundIndices foundLumps;
    lumpIndex.findAll("MAPINFO.lmp", foundLumps);
    for(auto const &lumpNumber : foundLumps)
    {
        // Ignore MAPINFO definition data in IWADs?
        if(ignoreNonCustom)
        {
            File1 const &file = lumpIndex[lumpNumber];
            /// @todo Custom status for contained files is not inherited from the container?
            if(file.isContained())
            {
                if(!file.container().hasCustom())
                    continue;
            }
            else if(!file.hasCustom())
                continue;
        }

        foundPaths << String("LumpIndex:%1").arg(lumpNumber);
    }

    return foundPaths;
}

/**
 * @param mapInfoUrns  MAPINFO definitions to translate, in load order.
 */
static void translateMapInfos(QStringList const &mapInfoUrns, String &xlat, String &xlatCustom)
{
    xlat.clear();
    xlatCustom.clear();

    String delimitedPaths = mapInfoUrns.join(";");
    if(delimitedPaths.isEmpty()) return;

    ddhook_mapinfo_convert_t parm;
    Str_InitStd(&parm.paths);
    Str_InitStd(&parm.translated);
    Str_InitStd(&parm.translatedCustom);
    try
    {
        Str_Set(&parm.paths, delimitedPaths.toUtf8().constData());
        if(DoomsdayApp::plugins().callHooks(HOOK_MAPINFO_CONVERT, 0, &parm))
        {
            xlat       = Str_Text(&parm.translated);
            xlatCustom = Str_Text(&parm.translatedCustom);
        }
    }
    catch(...)
    {}
    Str_Free(&parm.translatedCustom);
    Str_Free(&parm.translated);
    Str_Free(&parm.paths);
}

static void readAllDefinitions()
{
    Time begunAt;

    // Start with engine's own top-level definition file.
    readDefinitionFile(App::packageLoader().package("net.dengine.base").root()
                       .locate<File const>("defs/doomsday.ded").path());

    if(App_GameLoaded())
    {
        Game &game = App_CurrentGame();

        // Some games use definitions that are translated to DED.
        QStringList mapInfoUrns = allMapInfoUrns();
        if(!mapInfoUrns.isEmpty())
        {
            String xlat, xlatCustom;
            translateMapInfos(mapInfoUrns, xlat, xlatCustom);

            if(!xlat.isEmpty())
            {
                LOGDEV_MAP_VERBOSE("Non-custom translated MAPINFO definitions:\n") << xlat;

                if(!DED_ReadData(&defs, xlat.toUtf8().constData(),
                                 "[TranslatedMapInfos]", false /*not custom*/))
                {
                    App_Error("readAllDefinitions: DED parse error:\n%s", DED_Error());
                }
            }

            if(!xlatCustom.isEmpty())
            {
                LOGDEV_MAP_VERBOSE("Custom translated MAPINFO definitions:\n") << xlatCustom;

                if(!DED_ReadData(&defs, xlatCustom.toUtf8().constData(),
                                 "[TranslatedMapInfos]", true /*custom*/))
                {
                    App_Error("readAllDefinitions: DED parse error:\n%s", DED_Error());
                }
            }
        }

        // Now any startup definition files required by the game.
        Game::Manifests const &gameResources = game.manifests();
        dint packageIdx = 0;
        for(Game::Manifests::const_iterator i = gameResources.find(RC_DEFINITION);
            i != gameResources.end() && i.key() == RC_DEFINITION; ++i, ++packageIdx)
        {
            ResourceManifest &record = **i;
            /// Try to locate this resource now.
            String const &path = record.resolvedPath(true/*try to locate*/);
            if(path.isEmpty())
            {
                QByteArray names = record.names().join(";").toUtf8();
                App_Error("readAllDefinitions: Error, failed to locate required game definition \"%s\".", names.constData());
            }

            readDefinitionFile(path);
        }

        // Next are definition files in the games' /auto directory.
        if(!CommandLine_Exists("-noauto"))
        {
            FS1::PathList foundPaths;
            if(fileSys().findAllPaths(de::Uri("$(App.DefsPath)/$(GamePlugin.Name)/auto/*.ded", RC_NULL).resolved(), 0, foundPaths))
            {
                for(FS1::PathListItem const &found : foundPaths)
                {
                    // Ignore directories.
                    if(found.attrib & A_SUBDIR) continue;

                    readDefinitionFile(found.path);
                }
            }
        }
    }

    // Next are any definition files specified on the command line.
    AutoStr *buf = AutoStr_NewStd();
    for(dint p = 0; p < CommandLine_Count(); ++p)
    {
        char const *arg = CommandLine_At(p);
        if(!CommandLine_IsMatchingAlias("-def", arg) &&
           !CommandLine_IsMatchingAlias("-defs", arg)) continue;

        while(++p != CommandLine_Count() && !CommandLine_IsOption(p))
        {
            char const *searchPath = CommandLine_PathAt(p);

            Str_Clear(buf); Str_Set(buf, searchPath);
            F_FixSlashes(buf, buf);
            F_ExpandBasePath(buf, buf);
            // We must have an absolute path. If we still do not have one then
            // prepend the current working directory if necessary.
            prependWorkPath(buf, buf);

            readDefinitionFile(String(Str_Text(buf)));
        }
        p--;  /* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }

    // Last are DD_DEFNS definition lumps from loaded add-ons.
    /// @todo Shouldn't these be processed before definitions on the command line?
    Def_ReadLumpDefs();

    LOG_RES_VERBOSE("readAllDefinitions: Completed in %.2f seconds") << begunAt.since();
}

static void defineFlaremap(de::Uri const &resourceUri)
{
    if(resourceUri.isEmpty()) return;

    // Reference to none?
    if(!resourceUri.path().toStringRef().compareWithoutCase("-")) return;

    // Reference to a "built-in" flaremap?
    String const &resourcePathStr = resourceUri.path().toStringRef();
    if(resourcePathStr.length() == 1 &&
       resourcePathStr.first() >= '0' && resourcePathStr.first() <= '4')
        return;

    resSys().defineTexture("Flaremaps", resourceUri);
}

static void defineLightmap(de::Uri const &resourceUri)
{
    if(resourceUri.isEmpty()) return;

    // Reference to none?
    if(!resourceUri.path().toStringRef().compareWithoutCase("-")) return;

    resSys().defineTexture("Lightmaps", resourceUri);
}

static void generateMaterialDefForTexture(TextureManifest const &manifest)
{
    LOG_AS("generateMaterialDefForTexture");

    Record &mat = defs.materials[defs.addMaterial()];
    mat.set("autoGenerated", true);

    de::Uri const texUri = manifest.composeUri();
    mat.set("id", de::Uri(DD_MaterialSchemeNameForTextureScheme(texUri.scheme()), texUri.path()).compose());

    if(manifest.hasTexture())
    {
        Texture &tex = manifest.texture();
        mat.set("dimensions", new ArrayValue(tex.dimensions()));
        mat.set("flags", dint(tex.isFlagged(Texture::NoDraw) ? MATF_NO_DRAW : 0));
    }
    else
    {
        LOGDEV_RES_MSG("Texture \"%s\" not yet defined, resultant Material will inherit dimensions") << texUri;
    }

    // The first layer and stage is implicit.
    defn::Material matDef(mat);
    defn::MaterialLayer layerDef(matDef.addLayer());

    Record &st0 = layerDef.addStage();
    st0.set("texture", texUri.compose());

    // Is there an animation for this?
    AnimGroup const *anim = resSys().animGroupForTexture(manifest);
    if(anim && anim->frameCount() > 1)
    {
        // Determine the start frame.
        dint startFrame = 0;
        while(&anim->frame(startFrame).textureManifest() != &manifest)
        {
            startFrame++;
        }

        // Just animate the first in the sequence?
        if(startFrame && (anim->flags() & AGF_FIRST_ONLY))
            return;

        // Complete configuration of the first stage.
        AnimGroupFrame const &animFrame0 = anim->frame(startFrame);
        st0.set("tics", dint( animFrame0.tics() + animFrame0.randomTics()) );
        if(animFrame0.randomTics())
        {
            st0.set("variance", animFrame0.randomTics() / st0.getf("tics"));
        }

        // Add further stages according to the animation group.
        startFrame++;
        for(dint i = 0; i < anim->frameCount() - 1; ++i)
        {
            AnimGroupFrame const &animFrame      = anim->frame(de::wrap(startFrame + i, 0, anim->frameCount()));
            TextureManifest const &frameManifest = animFrame.textureManifest();

            Record &st = layerDef.addStage();
            st.set("texture", frameManifest.composeUrn().compose());
            st.set("tics", dint( animFrame.tics() + animFrame.randomTics() ));
            if(animFrame.randomTics())
            {
                st.set("variance", animFrame.randomTics() / st.getf("tics"));
            }
        }
    }
}

static void generateMaterialDefsForAllTexturesInScheme(TextureScheme &scheme)
{
    PathTreeIterator<TextureScheme::Index> iter(scheme.index().leafNodes());
    while(iter.hasNext()) generateMaterialDefForTexture(iter.next());
}

static inline void generateMaterialDefsForAllTexturesInScheme(String const &schemeName)
{
    generateMaterialDefsForAllTexturesInScheme(resSys().textureScheme(schemeName));
}

static void generateMaterialDefs()
{
    generateMaterialDefsForAllTexturesInScheme("Textures");
    generateMaterialDefsForAllTexturesInScheme("Flats");
    generateMaterialDefsForAllTexturesInScheme("Sprites");
}

#ifdef __CLIENT__

/**
 * Returns @c true iff @a decorDef is compatible with the specified context.
 */
static bool decorationIsCompatible(Record const &decorDef, de::Uri const &textureUri,
    bool materialIsCustom)
{
    if(de::Uri(decorDef.gets("texture"), RC_NULL) != textureUri)
        return false;

    if(materialIsCustom)
    {
        return (decorDef.geti("flags") & DCRF_PWAD) != 0;
    }
    return (decorDef.geti("flags") & DCRF_NO_IWAD) == 0;
}

/**
 * (Re)Decorate the given @a material according to definition @a def. Any existing
 * decorations will be cleared in the process.
 *
 * @param material  The material being (re)decorated.
 * @param def       Definition to apply.
 */
static void redecorateMaterial(Material &material, Record const &def)
{
    defn::Material matDef(def);

    material.clearAllDecorations();

    // Prefer decorations defined within the material.
    for(dint i = 0; i < matDef.decorationCount(); ++i)
    {
        defn::MaterialDecoration decorDef(matDef.decoration(i));

        for(dint k = 0; k < decorDef.stageCount(); ++k)
        {
            Record const &st = decorDef.stage(k);

            defineLightmap(de::Uri(st.gets("lightmapUp"), RC_NULL));
            defineLightmap(de::Uri(st.gets("lightmapDown"), RC_NULL));
            defineLightmap(de::Uri(st.gets("lightmapSide"), RC_NULL));
            defineFlaremap(de::Uri(st.gets("haloTexture"), RC_NULL));
        }

        material.addDecoration(MaterialLightDecoration::fromDef(decorDef.def()));
    }

    if(material.hasDecorations())
        return;

    // Perhaps old style linked decoration definitions?
    if(material.layerCount())
    {
        // The animation configuration of layer0 determines decoration animation.
        auto const &decorationsByTexture = defs.decorations.lookup("texture").elements();
        auto const &layer0               = material.layer(0).as<MaterialTextureLayer>();

        bool haveDecorations = false;
        QVector<Record const *> stageDecorations(layer0.stageCount());
        for(dint i = 0; i < layer0.stageCount(); ++i)
        {
            MaterialTextureLayer::AnimationStage const &stage = layer0.stage(i);
            try
            {
                TextureManifest &texManifest = resSys().textureManifest(de::Uri(stage.gets("texture"), RC_NULL));
                de::Uri const texUri = texManifest.composeUri();
                for(auto const &pair : decorationsByTexture)
                {
                    Record const &rec = *pair.second->as<RecordValue>().record();
                    if(decorationIsCompatible(rec, texUri, material.manifest().isCustom()))
                    {
                        stageDecorations[i] = &rec;
                        haveDecorations = true;
                        break;
                    }
                }
            }
            catch(ResourceSystem::MissingManifestError const &)
            {}  // Ignore this error
        }

        if(!haveDecorations) return;

        for(dint i = 0; i < layer0.stageCount(); ++i)
        {
            if(!stageDecorations[i]) continue;

            defn::Decoration mainDef(*stageDecorations[i]);
            for(dint k = 0; k < mainDef.lightCount(); ++k)
            {
                defn::MaterialDecoration decorDef(mainDef.light(k));
                DENG2_ASSERT(decorDef.stageCount() == 1); // sanity check.

                std::unique_ptr<MaterialLightDecoration> decor(
                        new MaterialLightDecoration(Vector2i(decorDef.geta("patternSkip")),
                                                    Vector2i(decorDef.geta("patternOffset")),
                                                    false /*don't use interpolation*/));

                std::unique_ptr<MaterialLightDecoration::AnimationStage> definedDecorStage(
                        MaterialLightDecoration::AnimationStage::fromDef(decorDef.stage(0)));

                definedDecorStage->tics = layer0.stage(i).tics;

                for(dint m = 0; m < i; ++m)
                {
                    MaterialLightDecoration::AnimationStage preStage(*definedDecorStage);
                    preStage.tics  = layer0.stage(m).tics;
                    preStage.color = Vector3f();
                    decor->addStage(preStage);  // makes a copy.
                }

                decor->addStage(*definedDecorStage);

                for(dint m = i + 1; m < layer0.stageCount(); ++m)
                {
                    MaterialLightDecoration::AnimationStage postStage(*definedDecorStage);
                    postStage.tics  = layer0.stage(m).tics;
                    postStage.color = Vector3f();
                    decor->addStage(postStage);
                }

                material.addDecoration(decor.release());  // takes ownership.
            }
        }
    }
}

#endif // __CLIENT__

static ded_group_t *findGroupForMaterialLayerAnimation(de::Uri const &uri)
{
    if(uri.isEmpty()) return nullptr;

    // Reverse iteration (later defs override earlier ones).
    for(dint i = defs.groups.size(); i--> 0; )
    {
        ded_group_t &grp = defs.groups[i];

        // We aren't interested in precache groups.
        if(grp.flags & AGF_PRECACHE) continue;

        // Or empty/single-frame groups.
        if(grp.members.size() < 2) continue;

        // The referenced material must be a member.
        if(!grp.tryFindFirstMemberWithMaterial(uri)) continue;

        // Only consider groups where each frame has a valid duration.
        dint k;
        for(k = 0; k < grp.members.size(); ++k)
        {
            if(grp.members[k].tics < 0) break;
        }
        if(k < grp.members.size()) continue;

        // Found a suitable Group.
        return &grp;
    }

    return nullptr;  // Not found.
}

static void configureMaterial(Material &mat, Record const &definition)
{
    defn::Material matDef(definition);
    de::Uri const materialUri(matDef.gets("id"), RC_NULL);

    // Reconfigure basic properties.
    mat.setDimensions(Vector2i(matDef.geta("dimensions")));
    mat.markDontDraw((matDef.geti("flags") & MATF_NO_DRAW) != 0);
    mat.markSkyMasked((matDef.geti("flags") & MATF_SKYMASK) != 0);

#ifdef __CLIENT__
    mat.setAudioEnvironment(S_AudioEnvironmentId(&materialUri));
#endif

    // Reconfigure the layers.
    mat.clearAllLayers();
    for(dint i = 0; i < matDef.layerCount(); ++i)
    {
        mat.addLayerAt(MaterialTextureLayer::fromDef(matDef.layer(i)), mat.layerCount());
    }

    if(mat.layerCount() && mat.layer(0).stageCount())
    {
        auto &layer0 = mat.layer(0).as<MaterialTextureLayer>();
        MaterialTextureLayer::AnimationStage &stage0 = layer0.stage(0);

        if(!stage0.gets("texture").isEmpty())
        {
            // We may need to interpret the layer animation from the now
            // deprecated Group definitions.

            if(matDef.getb("autoGenerated") && layer0.stageCount() == 1)
            {
                de::Uri const textureUri(stage0.gets("texture"), RC_NULL);

                // Possibly; see if there is a compatible definition with
                // a member named similarly to the texture for layer #0.

                if(ded_group_t const *grp = findGroupForMaterialLayerAnimation(textureUri))
                {
                    // Determine the start frame.
                    dint startFrame = 0;
                    while(!grp->members[startFrame].material ||
                          *grp->members[startFrame].material != textureUri)
                    {
                        startFrame++;
                    }

                    // Configure the first stage.
                    ded_group_member_t const &gm0 = grp->members[startFrame];
                    stage0.tics     = gm0.tics;
                    stage0.variance = de::max(gm0.randomTics, 0) / dfloat( gm0.tics );

                    // Add further stages for each frame in the group.
                    startFrame++;
                    for(dint i = 0; i < grp->members.size() - 1; ++i)
                    {
                        dint const frame             = de::wrap(startFrame + i, 0, grp->members.size());
                        ded_group_member_t const &gm = grp->members[frame];

                        if(gm.material)
                        {
                            dint const tics       = gm.tics;
                            dfloat const variance = de::max(gm.randomTics, 0) / dfloat( gm.tics );

                            layer0.addStage(MaterialTextureLayer::AnimationStage(*gm.material, tics, variance));
                        }
                    }
                }
            }

            // Are there Detail definitions we need to produce a layer for?
            MaterialDetailLayer *dlayer = nullptr;
            for(dint i = 0; i < layer0.stageCount(); ++i)
            {
                MaterialTextureLayer::AnimationStage &stage = layer0.stage(i);
                ded_detailtexture_t const *detailDef =
                    tryFindDetailTexture(de::Uri(stage.gets("texture"), RC_NULL),
                                         /*UNKNOWN VALUE,*/ mat.manifest().isCustom());

                if(!detailDef || !detailDef->stage.texture)
                    continue;

                if(!dlayer)
                {
                    // Add a new detail layer.
                    mat.addLayerAt(dlayer = MaterialDetailLayer::fromDef(*detailDef), 0);
                }
                else
                {
                    // Add a new stage.
                    try
                    {
                        TextureManifest &texture = resSys().textureScheme("Details").findByResourceUri(*detailDef->stage.texture);
                        dlayer->addStage(MaterialDetailLayer::AnimationStage(texture.composeUri(), stage.tics, stage.variance,
                                                                             detailDef->stage.scale, detailDef->stage.strength,
                                                                             detailDef->stage.maxDistance));

                        if(dlayer->stageCount() == 2)
                        {
                            // Update the first stage with timing info.
                            MaterialTextureLayer::AnimationStage const &stage0 = layer0.stage(0);
                            MaterialTextureLayer::AnimationStage &dstage0      = dlayer->stage(0);

                            dstage0.tics     = stage0.tics;
                            dstage0.variance = stage0.variance;
                        }
                    }
                    catch(ResourceSystem::MissingManifestError const &)
                    {}  // Ignore this error.
                }
            }

            // Are there Reflection definition we need to produce a layer for?
            MaterialShineLayer *slayer = nullptr;
            for(dint i = 0; i < layer0.stageCount(); ++i)
            {
                MaterialTextureLayer::AnimationStage &stage = layer0.stage(i);
                ded_reflection_t const *shineDef =
                    tryFindReflection(de::Uri(stage.gets("texture"), RC_NULL),
                                      /*UNKNOWN VALUE,*/ mat.manifest().isCustom());

                if(!shineDef || !shineDef->stage.texture)
                    continue;

                if(!slayer)
                {
                    // Add a new shine layer.
                    mat.addLayerAt(slayer = MaterialShineLayer::fromDef(*shineDef), mat.layerCount());
                }
                else
                {
                    // Add a new stage.
                    try
                    {
                        TextureManifest &texture = resSys().textureScheme("Reflections")
                                                               .findByResourceUri(*shineDef->stage.texture);

                        TextureManifest *maskTexture = nullptr;
                        if(shineDef->stage.maskTexture)
                        {
                            try
                            {
                                maskTexture = &resSys().textureScheme("Masks")
                                                   .findByResourceUri(*shineDef->stage.maskTexture);
                            }
                            catch(ResourceSystem::MissingManifestError const &)
                            {}  // Ignore this error.
                        }

                        slayer->addStage(MaterialShineLayer::AnimationStage(texture.composeUri(), stage.tics, stage.variance,
                                                                            maskTexture->composeUri(), shineDef->stage.blendMode,
                                                                            shineDef->stage.shininess,
                                                                            Vector3f(shineDef->stage.minColor),
                                                                            Vector2f(shineDef->stage.maskWidth, shineDef->stage.maskHeight)));

                        if(slayer->stageCount() == 2)
                        {
                            // Update the first stage with timing info.
                            MaterialTextureLayer::AnimationStage const &stage0 = layer0.stage(0);
                            MaterialTextureLayer::AnimationStage &sstage0      = slayer->stage(0);

                            sstage0.tics     = stage0.tics;
                            sstage0.variance = stage0.variance;
                        }
                    }
                    catch(ResourceSystem::MissingManifestError const &)
                    {}  // Ignore this error.
                }
            }
        }
    }

#ifdef __CLIENT__
    redecorateMaterial(mat, definition);
#endif

    // At this point we know the material is usable.
    mat.markValid();
}

static void interpretMaterialDef(Record const &definition)
{
    LOG_AS("interpretMaterialDef");
    defn::Material matDef(definition);
    de::Uri const materialUri(matDef.gets("id"), RC_NULL);
    try
    {
        // Create/retrieve a manifest for the would-be material.
        MaterialManifest *manifest = &resSys().declareMaterial(materialUri);

        // Update manifest classification:
        manifest->setFlags(MaterialManifest::AutoGenerated, matDef.getb("autoGenerated")? SetFlags : UnsetFlags);
        manifest->setFlags(MaterialManifest::Custom, UnsetFlags);
        if(matDef.layerCount())
        {
            defn::MaterialLayer layerDef(matDef.layer(0));
            if(layerDef.stageCount() > 0)
            {
                de::Uri const textureUri(layerDef.stage(0).gets("texture"), RC_NULL);
                try
                {
                    TextureManifest &texManifest = resSys().textureManifest(textureUri);
                    if(texManifest.hasTexture() && texManifest.texture().isFlagged(Texture::Custom))
                    {
                        manifest->setFlags(MaterialManifest::Custom);
                    }
                }
                catch(ResourceSystem::MissingManifestError const &er)
                {
                    // Log but otherwise ignore this error.
                    LOG_RES_WARNING("Ignoring unknown texture \"%s\" in Material \"%s\" (layer 0 stage 0): %s")
                        << textureUri
                        << materialUri
                        << er.asText();
                }
            }
        }

        // (Re)configure the material.
        /// @todo Defer until necessary.
        configureMaterial(*manifest->derive(), definition);
    }
    catch(ResourceSystem::UnknownSchemeError const &er)
    {
        LOG_RES_WARNING("Failed to declare material \"%s\": %s")
            << materialUri << er.asText();
    }
    catch(MaterialScheme::InvalidPathError const &er)
    {
        LOG_RES_WARNING("Failed to declare material \"%s\": %s")
            << materialUri << er.asText();
    }
}

static void invalidateAllMaterials()
{
    resSys().forAllMaterials([] (Material &material)
    {
        material.markValid(false);
        return LoopContinue;
    });
}

#ifdef __CLIENT__
static void clearFontDefinitionLinks()
{
    for(AbstractFont *font : resSys().allFonts())
    {
        if(CompositeBitmapFont *compFont = font->maybeAs<CompositeBitmapFont>())
        {
            compFont->setDefinition(nullptr);
        }
    }
}
#endif // __CLIENT__

void Def_Read()
{
    LOG_AS("Def_Read");

    if(defsInited)
    {
        // We've already initialized the definitions once.
        // Get rid of everything.
        FS1::Scheme &scheme = fileSys().scheme(App_ResourceClass("RC_MODEL").defaultScheme());
        scheme.reset();

        invalidateAllMaterials();
#ifdef __CLIENT__
        clearFontDefinitionLinks();
#endif
        defsInited = false;
    }

    // Now we can clear all existing definitions and re-init.
    defs.clear();
    runtimeDefs.clear();

    // Generate definitions.
    generateMaterialDefs();

    // Read all definitions files and lumps.
    LOG_RES_MSG("Parsing definition files...");
    readAllDefinitions();

    // Any definition hooks?
    DoomsdayApp::plugins().callHooks(HOOK_DEFS, 0, &defs);

#ifdef __CLIENT__
    // Composite fonts.
    for(dint i = 0; i < defs.compositeFonts.size(); ++i)
    {
        resSys().newFontFromDef(defs.compositeFonts[i]);
    }
#endif

    // States.
    ::runtimeDefs.states.append(::defs.states.size());
    for(dint i = 0; i < ::runtimeDefs.states.size(); ++i)
    {
        Record &dst = ::defs.states[i];

        // Make sure duplicate IDs overwrite the earliest.
        dint stateNum = ::defs.getStateNum(dst.gets("id"));
        if(stateNum == -1) continue;

        Record &dstNew = ::defs.states[stateNum];
        state_t *st = &::runtimeDefs.states[stateNum];

        st->sprite    = ::defs.getSpriteNum(dst.gets("sprite"));
        st->flags     = dst.geti("flags");
        st->frame     = dst.geti("frame");
        st->tics      = dst.geti("tics");
        st->action    = P_GetAction(dst.gets("action"));
        st->nextState = defs.getStateNum(dst.gets("nextState"));

        auto const &misc = dst.geta("misc");
        for(dint k = 0; k < NUM_STATE_MISC; ++k)
        {
            st->misc[k] = misc[k].asInt();
        }

        // Replace the older execute string.
        if(&dst != &dstNew)
        {
            dstNew.set("execute", dst.gets("execute"));
        }
    }

    ::runtimeDefs.stateInfo.append(defs.states.size());

    // Mobj info.
    ::runtimeDefs.mobjInfo.append(defs.things.size());
    for(dint i = 0; i < runtimeDefs.mobjInfo.size(); ++i)
    {
        Record *dmo = &defs.things[i];
        
        // Make sure duplicate defs overwrite the earliest.
        mobjinfo_t *mo = &runtimeDefs.mobjInfo[defs.getMobjNum(dmo->gets("id"))];

        gettingFor       = mo;
        mo->doomEdNum    = dmo->geti("doomEdNum");
        mo->spawnHealth  = dmo->geti("spawnHealth");
        mo->reactionTime = dmo->geti("reactionTime");
        mo->painChance   = dmo->geti("painChance");
        mo->speed        = dmo->getf("speed");
        mo->radius       = dmo->getf("radius");
        mo->height       = dmo->getf("height");
        mo->mass         = dmo->geti("mass");
        mo->damage       = dmo->geti("damage");
        mo->flags        = dmo->geta("flags")[0].asInt();
        mo->flags2       = dmo->geta("flags")[1].asInt();
        mo->flags3       = dmo->geta("flags")[2].asInt();

        auto const &states = dmo->geta("states");
        auto const &sounds = dmo->geta("sounds");
        
        for(dint k = 0; k < STATENAMES_COUNT; ++k)
        {
            mo->states[k] = Def_StateForMobj(states[k].asText());
        }
        
        mo->seeSound     = defs.getSoundNum(sounds[SDN_SEE].asText());
        mo->attackSound  = defs.getSoundNum(sounds[SDN_ATTACK].asText());
        mo->painSound    = defs.getSoundNum(sounds[SDN_PAIN].asText());
        mo->deathSound   = defs.getSoundNum(sounds[SDN_DEATH].asText());
        mo->activeSound  = defs.getSoundNum(sounds[SDN_ACTIVE].asText());
        
        for(dint k = 0; k < NUM_MOBJ_MISC; ++k)
        {
            mo->misc[k] = dmo->geta("misc")[k].asInt();
        }
    }

    // Decorations. (Define textures).
    for(dint i = 0; i < ::defs.decorations.size(); ++i)
    {
        defn::Decoration decorDef(::defs.decorations[i]);
        for(dint k = 0; k < decorDef.lightCount(); ++k)
        {
            Record const &st = defn::MaterialDecoration(decorDef.light(k)).stage(0);
            if(Vector3f(st.geta("color")) != Vector3f(0, 0, 0))
            {
                defineLightmap(de::Uri(st["lightmapUp"], RC_NULL));
                defineLightmap(de::Uri(st["lightmapDown"], RC_NULL));
                defineLightmap(de::Uri(st["lightmapSide"], RC_NULL));
                defineFlaremap(de::Uri(st["haloTexture"], RC_NULL));
            }
        }
    }

    // Detail textures (Define textures).
    resSys().textureScheme("Details").clear();
    for(dint i = 0; i < ::defs.details.size(); ++i)
    {
        ded_detailtexture_t *dtl = &::defs.details[i];

        // Ignore definitions which do not specify a material.
        if((!dtl->material1 || dtl->material1->isEmpty()) &&
           (!dtl->material2 || dtl->material2->isEmpty())) continue;

        if(!dtl->stage.texture) continue;

        resSys().defineTexture("Details", *dtl->stage.texture);
    }

    // Surface reflections (Define textures).
    resSys().textureScheme("Reflections").clear();
    resSys().textureScheme("Masks").clear();
    for(dint i = 0; i < ::defs.reflections.size(); ++i)
    {
        ded_reflection_t *ref = &::defs.reflections[i];

        // Ignore definitions which do not specify a material.
        if(!ref->material || ref->material->isEmpty()) continue;

        if(ref->stage.texture)
        {
            resSys().defineTexture("Reflections", *ref->stage.texture);
        }
        if(ref->stage.maskTexture)
        {
            resSys().defineTexture("Masks", *ref->stage.maskTexture,
                            Vector2i(ref->stage.maskWidth, ref->stage.maskHeight));
        }
    }

    // Materials.
    for(dint i = 0; i < ::defs.materials.size(); ++i)
    {
        interpretMaterialDef(::defs.materials[i]);
    }

    // Dynamic lights. Update the sprite numbers.
    for(dint i = 0; i < ::defs.lights.size(); ++i)
    {
        dint const stateIdx = ::defs.getStateNum(defs.lights[i].state);
        if(stateIdx < 0)
        {
            // It's probably a bias light definition, then?
            if(!::defs.lights[i].uniqueMapID[0])
            {
                LOG_RES_WARNING("Undefined state '%s' in Light definition") << ::defs.lights[i].state;
            }
            continue;
        }
        ::runtimeDefs.stateInfo[stateIdx].light = &::defs.lights[i];
    }

    // Sound effects.
    ::runtimeDefs.sounds.append(::defs.sounds.size());
    for(dint i = 0; i < ::runtimeDefs.sounds.size(); ++i)
    {
        ded_sound_t *snd = &::defs.sounds[i];
        // Make sure duplicate defs overwrite the earliest.
        sfxinfo_t *si    = &::runtimeDefs.sounds[::defs.getSoundNum(snd->id)];

        qstrcpy(si->id, snd->id);
        qstrcpy(si->lumpName, snd->lumpName);
        si->lumpNum     = (qstrlen(snd->lumpName) > 0? fileSys().lumpNumForName(snd->lumpName) : -1);
        qstrcpy(si->name, snd->name);

        dint const soundIdx = ::defs.getSoundNum(snd->link);
        si->link        = (soundIdx >= 0 ? &::runtimeDefs.sounds[soundIdx] : 0);

        si->linkPitch   = snd->linkPitch;
        si->linkVolume  = snd->linkVolume;
        si->priority    = snd->priority;
        si->channels    = snd->channels;
        si->flags       = snd->flags;
        si->group       = snd->group;

        Str_Init(&si->external);
        if(snd->ext)
        {
            Str_Set(&si->external, snd->ext->pathCStr());
        }
    }

    // Music.
    for(dint i = 0; i < ::defs.musics.size(); ++i)
    {
        Record *mus = &::defs.musics[i];
        // Make sure duplicate defs overwrite the earliest.
        Record *earliest = &::defs.musics[::defs.getMusicNum(mus->gets("id").toUtf8().constData())];

        if(earliest == mus) continue;

        earliest->set("lumpName", mus->gets("lumpName"));
        earliest->set("cdTrack",  mus->geti("cdTrack"));

        if(!mus->gets("path").isEmpty())
        {
            earliest->set("path", mus->gets("path"));
        }
        else if(!earliest->gets("path").isEmpty())
        {
            earliest->set("path", "");
        }
    }

    // Text.
    ::runtimeDefs.texts.append(::defs.text.size());
    for(dint i = 0; i < ::defs.text.size(); ++i)
    {
        Def_InitTextDef(&::runtimeDefs.texts[i], ::defs.text[i].text);
    }
    // Handle duplicate strings.
    for(dint i = 0; i < ::runtimeDefs.texts.size(); ++i)
    {
        if(!::runtimeDefs.texts[i].text) continue;

        for(dint k = i + 1; k < ::runtimeDefs.texts.size(); ++k)
        {
            if(!::runtimeDefs.texts[k].text) continue; // Already done.
            if(qstricmp(defs.text[i].id, defs.text[k].id)) continue; // ID mismatch.

            // Update the earlier string.
            ::runtimeDefs.texts[i].text = (char *) M_Realloc(::runtimeDefs.texts[i].text, qstrlen(::runtimeDefs.texts[k].text) + 1);
            qstrcpy(::runtimeDefs.texts[i].text, ::runtimeDefs.texts[k].text);

            // Free the later string, it isn't used (>NUMTEXT).
            M_Free(::runtimeDefs.texts[k].text);
            ::runtimeDefs.texts[k].text = nullptr;
        }
    }

    // Particle generators.
    for(dint i = 0; i < ::defs.ptcGens.size(); ++i)
    {
        ded_ptcgen_t *pg = &::defs.ptcGens[i];
        dint st = ::defs.getStateNum(pg->state);

        if(!qstrcmp(pg->type, "*"))
            pg->typeNum = DED_PTCGEN_ANY_MOBJ_TYPE;
        else
            pg->typeNum = ::defs.getMobjNum(pg->type);
        pg->type2Num  = ::defs.getMobjNum(pg->type2);
        pg->damageNum = ::defs.getMobjNum(pg->damage);

        // Figure out embedded sound ID numbers.
        for(dint k = 0; k < pg->stages.size(); ++k)
        {
            if(pg->stages[k].sound.name[0])
            {
                pg->stages[k].sound.id = ::defs.getSoundNum(pg->stages[k].sound.name);
            }
            if(pg->stages[k].hitSound.name[0])
            {
                pg->stages[k].hitSound.id = ::defs.getSoundNum(pg->stages[k].hitSound.name);
            }
        }

        if(st <= 0)
            continue; // Not state triggered, then...

        stateinfo_t *stinfo = &::runtimeDefs.stateInfo[st];

        // Link the definition to the state.
        if(pg->flags & Generator::StateChain)
        {
            // Add to the chain.
            pg->stateNext = stinfo->ptcGens;
            stinfo->ptcGens = pg;
        }
        else
        {
            // Make sure the previously built list is unlinked.
            while(stinfo->ptcGens)
            {
                ded_ptcgen_t *temp = stinfo->ptcGens->stateNext;

                stinfo->ptcGens->stateNext = nullptr;
                stinfo->ptcGens = temp;
            }
            stinfo->ptcGens = pg;
            pg->stateNext = nullptr;
        }
    }

    // Map infos.
    for(dint i = 0; i < ::defs.mapInfos.size(); ++i)
    {
        Record &mi = ::defs.mapInfos[i];

        // Historically, the map info flags field was used for sky flags, here we copy
        // those flags to the embedded sky definition for backward-compatibility.
        if(mi.geti("flags") & MIF_DRAW_SPHERE)
        {
            mi.set("sky.flags", mi.geti("sky.flags") | SIF_DRAW_SPHERE);
        }
    }

    // Log a summary of the definition database.
    LOG_RES_MSG(_E(b) "Definitions:");
    String str;
    QTextStream os(&str);
    os << defCountMsg(::defs.episodes.size(),         "episodes");
    os << defCountMsg(::defs.groups.size(),           "animation groups");
    os << defCountMsg(::defs.compositeFonts.size(),   "composite fonts");
    os << defCountMsg(::defs.details.size(),          "detail textures");
    os << defCountMsg(::defs.finales.size(),          "finales");
    os << defCountMsg(::defs.lights.size(),           "lights");
    os << defCountMsg(::defs.lineTypes.size(),        "line types");
    os << defCountMsg(::defs.mapInfos.size(),         "map infos");

    dint nonAutoGeneratedCount = 0;
    for(dint i = 0; i < ::defs.materials.size(); ++i)
    {
        if(!::defs.materials[i].getb("autoGenerated"))
            ++nonAutoGeneratedCount;
    }
    os << defCountMsg(nonAutoGeneratedCount,        "materials");

    os << defCountMsg(::defs.models.size(),           "models");
    os << defCountMsg(::defs.ptcGens.size(),          "particle generators");
    os << defCountMsg(::defs.skies.size(),            "skies");
    os << defCountMsg(::defs.sectorTypes.size(),      "sector types");
    os << defCountMsg(::defs.musics.size(),           "songs");
    os << defCountMsg(::runtimeDefs.sounds.size(),    "sound effects");
    os << defCountMsg(::defs.sprites.size(),          "sprite names");
    os << defCountMsg(::runtimeDefs.states.size(),    "states");
    os << defCountMsg(::defs.decorations.size(),      "surface decorations");
    os << defCountMsg(::defs.reflections.size(),      "surface reflections");
    os << defCountMsg(::runtimeDefs.texts.size(),     "text strings");
    os << defCountMsg(::defs.textureEnv.size(),       "texture environments");
    os << defCountMsg(::runtimeDefs.mobjInfo.size(),  "things");

    LOG_RES_MSG("%s") << str.rightStrip();

    ::defsInited = true;
}

static void initMaterialGroup(ded_group_t &def)
{
    ResourceSystem::MaterialManifestGroup *group = nullptr;
    for(dint i = 0; i < def.members.size(); ++i)
    {
        ded_group_member_t *gm = &def.members[i];
        if(!gm->material) continue;

        try
        {
            MaterialManifest &manifest = resSys().materialManifest(*gm->material);

            if(def.flags & AGF_PRECACHE) // A precache group.
            {
                // Only create the group once the first material has been found.
                if(!group)
                {
                    group = &resSys().newMaterialGroup();
                }

                group->insert(&manifest);
            }
#if 0 /// @todo $revise-texture-animation
            else // An animation group.
            {
                // Only create the group once the first material has been found.
                if(animNumber == -1)
                {
                    animNumber = resSys().newAnimGroup(def.flags & ~AGF_PRECACHE);
                }

                resSys().animGroup(animNumber).addFrame(manifest.material(), gm->tics, gm->randomTics);
            }
#endif
        }
        catch(ResourceSystem::MissingManifestError const &er)
        {
            // Log but otherwise ignore this error.
            LOG_RES_WARNING("Unknown material \"%s\" in group def %i: %s")
                    << *gm->material
                    << i << er.asText();
        }
    }
}

void Def_PostInit()
{
#ifdef __CLIENT__

    // Particle generators: model setup.
    for(dint i = 0; i < ::defs.ptcGens.size(); ++i)
    {
        ded_ptcgen_t *gen = &::defs.ptcGens[i];

        for(dint k = 0; k < gen->stages.size(); ++k)
        {
            ded_ptcstage_t *st = &gen->stages[k];

            if(st->type < PTC_MODEL || st->type >= PTC_MODEL + MAX_PTC_MODELS)
                continue;

            st->model = -1;
            try
            {
                ModelDef &modef = resSys().modelDef(String("Particle%1").arg(st->type - PTC_MODEL, 2, 10, QChar('0')));
                if(modef.subModelId(0) == NOMODELID)
                {
                    continue;
                }

                Model &mdl = resSys().model(modef.subModelId(0));

                st->model = resSys().indexOf(&modef);
                st->frame = mdl.frameNumber(st->frameName);
                if(st->frame < 0) st->frame = 0;
                if(st->endFrameName[0])
                {
                    st->endFrame = mdl.frameNumber(st->endFrameName);
                    if(st->endFrame < 0) st->endFrame = 0;
                }
                else
                {
                    st->endFrame = -1;
                }
            }
            catch(ResourceSystem::MissingModelDefError const &)
            {}  // Ignore this error.
        }
    }

#endif // __CLIENT__

    // Lights.
    for(dint i = 0; i < ::defs.lights.size(); ++i)
    {
        ded_light_t &lightDef = ::defs.lights[i];

        if(lightDef.up)    defineLightmap(*lightDef.up);
        if(lightDef.down)  defineLightmap(*lightDef.down);
        if(lightDef.sides) defineLightmap(*lightDef.sides);
        if(lightDef.flare) defineFlaremap(*lightDef.flare);
    }

    // Material groups (e.g., for precaching).
    resSys().clearAllMaterialGroups();
    for(dint i = 0; i < ::defs.groups.size(); ++i)
    {
        initMaterialGroup(::defs.groups[i]);
    }
}

bool Def_SameStateSequence(state_t *snew, state_t *sold)
{
    if(!snew || !sold) return false;
    if(snew == sold) return true;  // Trivial.

    dint const target = ::runtimeDefs.states.indexOf(snew);
    dint const start  = ::runtimeDefs.states.indexOf(sold);

    dint count = 0;
    for(dint it = sold->nextState; it >= 0 && it != start && count < 16;
        it = ::runtimeDefs.states[it].nextState, ++count)
    {
        if(it == target)
            return true;

        if(it == ::runtimeDefs.states[it].nextState)
            break;
    }
    return false;
}

String Def_GetStateName(state_t *state)
{
    if(!state) return "(nullptr)";
    dint const idx = ::runtimeDefs.states.indexOf(state);
    DENG2_ASSERT(idx >= 0);
    return ::defs.states[idx].gets("id");
}

static inline dint Friendly(dint num)
{
    return de::max(0, num);
}

/**
 * Converts a DED line type to the internal format.
 * Bit of a nuisance really...
 */
void Def_CopyLineType(linetype_t *l, ded_linetype_t *def)
{
    DENG2_ASSERT(l && def);

    l->id               = def->id;
    l->flags            = def->flags[0];
    l->flags2           = def->flags[1];
    l->flags3           = def->flags[2];
    l->lineClass        = def->lineClass;
    l->actType          = def->actType;
    l->actCount         = def->actCount;
    l->actTime          = def->actTime;
    l->actTag           = def->actTag;

    for(dint i = 0; i < 10; ++i)
    {
        if(i == 9)
            l->aparm[i] = ::defs.getMobjNum(def->aparm9);
        else
            l->aparm[i] = def->aparm[i];
    }

    l->tickerStart      = def->tickerStart;
    l->tickerEnd        = def->tickerEnd;
    l->tickerInterval   = def->tickerInterval;
    l->actSound         = Friendly(::defs.getSoundNum(def->actSound));
    l->deactSound       = Friendly(::defs.getSoundNum(def->deactSound));
    l->evChain          = def->evChain;
    l->actChain         = def->actChain;
    l->deactChain       = def->deactChain;
    l->actLineType      = def->actLineType;
    l->deactLineType    = def->deactLineType;
    l->wallSection      = def->wallSection;

    if(def->actMaterial)
    {
        try
        {
            l->actMaterial = resSys().materialManifest(*def->actMaterial).id();
        }
        catch(ResourceSystem::MissingManifestError const &)
        {}  // Ignore this error.
    }

    if(def->deactMaterial)
    {
        try
        {
            l->deactMaterial = resSys().materialManifest(*def->deactMaterial).id();
        }
        catch(ResourceSystem::MissingManifestError const &)
        {}  // Ignore this error.
    }

    l->actMsg            = def->actMsg;
    l->deactMsg          = def->deactMsg;
    l->materialMoveAngle = def->materialMoveAngle;
    l->materialMoveSpeed = def->materialMoveSpeed;

    dint i;
    LOOPi(20) l->iparm[i] = def->iparm[i];
    LOOPi(20) l->fparm[i] = def->fparm[i];
    LOOPi(5)  l->sparm[i] = def->sparm[i];

    // Some of the parameters might be strings depending on the line class.
    // Find the right mapping table.
    for(dint k = 0; k < 20; ++k)
    {
        dint const a = XG_Class(l->lineClass)->iparm[k].map;
        if(a < 0) continue;

        if(a & MAP_SND)
        {
            l->iparm[k] = Friendly(::defs.getSoundNum(def->iparmStr[k]));
        }
        else if(a & MAP_MATERIAL)
        {
            if(def->iparmStr[k][0])
            {
                if(!qstricmp(def->iparmStr[k], "-1"))
                {
                    l->iparm[k] = -1;
                }
                else
                {
                    try
                    {
                        l->iparm[k] = resSys().materialManifest(de::Uri(def->iparmStr[k], RC_NULL)).id();
                    }
                    catch(ResourceSystem::MissingManifestError const &)
                    {}  // Ignore this error.
                }
            }
        }
        else if(a & MAP_MUS)
        {
            dint temp = Friendly(::defs.getMusicNum(def->iparmStr[k]));

            if(temp == 0)
            {
                temp = ::defs.evalFlags(def->iparmStr[k]);
                if(temp)
                    l->iparm[k] = temp;
            }
            else
            {
                l->iparm[k] = Friendly(::defs.getMusicNum(def->iparmStr[k]));
            }
        }
        else
        {
            dint temp = ::defs.evalFlags(def->iparmStr[k]);
            if(temp)
                l->iparm[k] = temp;
        }
    }
}

/**
 * Converts a DED sector type to the internal format.
 */
void Def_CopySectorType(sectortype_t *s, ded_sectortype_t *def)
{
    DENG2_ASSERT(s && def);
    dint i, k;

    s->id           = def->id;
    s->flags        = def->flags;
    s->actTag       = def->actTag;
    LOOPi(5)
    {
        s->chain[i]      = def->chain[i];
        s->chainFlags[i] = def->chainFlags[i];
        s->start[i]      = def->start[i];
        s->end[i]        = def->end[i];
        LOOPk(2) s->interval[i][k] = def->interval[i][k];
        s->count[i]      = def->count[i];
    }
    s->ambientSound = Friendly(::defs.getSoundNum(def->ambientSound));
    LOOPi(2)
    {
        s->soundInterval[i]     = def->soundInterval[i];
        s->materialMoveAngle[i] = def->materialMoveAngle[i];
        s->materialMoveSpeed[i] = def->materialMoveSpeed[i];
    }
    s->windAngle    = def->windAngle;
    s->windSpeed    = def->windSpeed;
    s->verticalWind = def->verticalWind;
    s->gravity      = def->gravity;
    s->friction     = def->friction;
    s->lightFunc    = def->lightFunc;
    LOOPi(2) s->lightInterval[i] = def->lightInterval[i];
    LOOPi(3)
    {
        s->colFunc[i] = def->colFunc[i];
        LOOPk(2) s->colInterval[i][k] = def->colInterval[i][k];
    }
    s->floorFunc    = def->floorFunc;
    s->floorMul     = def->floorMul;
    s->floorOff     = def->floorOff;
    LOOPi(2) s->floorInterval[i] = def->floorInterval[i];
    s->ceilFunc     = def->ceilFunc;
    s->ceilMul      = def->ceilMul;
    s->ceilOff      = def->ceilOff;
    LOOPi(2) s->ceilInterval[i] = def->ceilInterval[i];
}

dint Def_Get(dint type, char const *id, void *out)
{
    switch(type)
    {
    case DD_DEF_ACTION:
        if(acfnptr_t action = P_GetAction(id))
        {
            if(out) *(acfnptr_t *)out = action;
            return true;
        }
        return false;

    case DD_DEF_SOUND_LUMPNAME: {
        dint32 i = *((dint32 *) id);
        if(i < 0 || i >= ::runtimeDefs.sounds.size())
            return false;
        qstrcpy((char *)out, ::runtimeDefs.sounds[i].lumpName);
        return true; }

    case DD_DEF_LINE_TYPE: {
        dint typeId = strtol(id, (char **)nullptr, 10);
        for(dint i = ::defs.lineTypes.size() - 1; i >= 0; i--)
        {
            if(::defs.lineTypes[i].id != typeId) continue;
            if(out) Def_CopyLineType((linetype_t *)out, &::defs.lineTypes[i]);
            return true;
        }
        return false; }

    case DD_DEF_SECTOR_TYPE: {
        dint typeId = strtol(id, (char **)nullptr, 10);
        for(dint i = ::defs.sectorTypes.size() - 1; i >= 0; i--)
        {
            if(::defs.sectorTypes[i].id != typeId) continue;
            if(out) Def_CopySectorType((sectortype_t *)out, &::defs.sectorTypes[i]);
            return true;
        }
        return false; }

    default: return false;
    }
}

dint Def_Set(dint type, dint index, dint value, void const *ptr)
{
    LOG_AS("Def_Set");

    switch(type)
    {
    case DD_DEF_SOUND:
        if(index < 0 || index >= ::runtimeDefs.sounds.size())
        {
            DENG2_ASSERT(!"Sound index is invalid");
            return false;
        }

        switch(value)
        {
        case DD_LUMP:
            S_StopSound(index, 0);
            qstrcpy(::runtimeDefs.sounds[index].lumpName, (char const *) ptr);
            if(qstrlen(::runtimeDefs.sounds[index].lumpName))
            {
                ::runtimeDefs.sounds[index].lumpNum = fileSys().lumpNumForName(::runtimeDefs.sounds[index].lumpName);
                if(::runtimeDefs.sounds[index].lumpNum < 0)
                {
                    LOG_RES_WARNING("Unknown sound lump name \"%s\"; sound #%i will be inaudible")
                            << ::runtimeDefs.sounds[index].lumpName << index;
                }
            }
            else
            {
                ::runtimeDefs.sounds[index].lumpNum = 0;
            }
            break;

        default: break;
        }
        break;

    default: return false;
    }

    return true;
}

/**
 * Prints a list of all the registered mobjs to the console.
 * @todo Does this belong here?
 */
D_CMD(ListMobjs)
{
    DENG2_UNUSED3(src, argc, argv);

    if(::defs.things.size() <= 0)
    {
        LOG_RES_MSG("No mobjtypes defined/loaded");
        return true;
    }

    LOG_RES_MSG(_E(b) "Registered Mobjs (ID | Name):");
    for(dint i = 0; i < ::defs.things.size(); ++i)
    {
        auto const &name = ::defs.things[i].gets("name");
        if(!name.isEmpty())
            LOG_RES_MSG(" %s | %s") << ::defs.things[i].gets("id") << name;
        else
            LOG_RES_MSG(" %s | " _E(l) "(Unnamed)") << ::defs.things[i].gets("id");
    }

    return true;
}

void Def_ConsoleRegister()
{
    C_CMD("listmobjtypes", "", ListMobjs);
}

DENG_DECLARE_API(Def) =
{
    { DE_API_DEFINITIONS },

    Def_Get,
    Def_Set
};
