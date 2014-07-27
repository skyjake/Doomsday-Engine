/** @file def_main.cpp  Definitions Subsystem.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "def_main.h"

#include "de_system.h"
#include "de_platform.h"
#include "de_console.h"
#include "de_audio.h"
#include "de_misc.h"
#include "de_graphics.h"
#include "de_ui.h"
#include "de_filesys.h"
#include "de_resource.h"

#include "Generator"
#ifdef __CLIENT__
#  include "render/rend_particle.h"
#endif

#include "api_def.h"
#include "xgclass.h"

#include <doomsday/defs/dedfile.h>
#include <doomsday/defs/dedparser.h>
#include <de/App>
#include <de/NativePath>
#include <QTextStream>
#include <cctype>
#include <cstring>

using namespace de;

#define LOOPi(n)    for(i = 0; i < (n); ++i)
#define LOOPk(n)    for(k = 0; k < (n); ++k)

typedef struct {
    char* name; // Name of the routine.
    void (*func)(); // Pointer to the function.
} actionlink_t;

ded_t defs; // The main definitions database.
dd_bool firstDED;

/*
sprname_t* sprNames; // Sprite name list.
ded_count_t countSprNames;

state_t* states; // State list.
ded_count_t countStates;

mobjinfo_t* mobjInfo; // Map object info database.
ded_count_t countMobjInfo;

sfxinfo_t* sounds; // Sound effect list.
ded_count_t countSounds;

ddtext_t* texts; // Text list.
ded_count_t countTexts;

mobjinfo_t** stateOwners; // A pointer for each State.
ded_count_t countStateOwners;

ded_light_t** stateLights; // A pointer for each State.
ded_count_t countStateLights;

ded_ptcgen_t** statePtcGens; // A pointer for each State.
ded_count_t countStatePtcGens;
*/

RuntimeDefs runtimeDefs;

static dd_bool defsInited = false;
static mobjinfo_t* gettingFor;

static xgclass_t nullXgClassLinks; // Used when none defined.
static xgclass_t* xgClassLinks;

void RuntimeDefs::clear()
{
    for(int i = 0; i < sounds.size(); ++i)
    {
        Str_Free(&sounds[i].external);
    }
    sounds.clear();

    sprNames.clear();
    mobjInfo.clear();
    states.clear();
    texts.clear();
    stateInfo.clear();
}

/**
 * Retrieves the XG Class list from the Game.
 * XGFunc links are provided by the Game, who owns the actual
 * XG classes and their functions.
 */
int Def_GetGameClasses(void)
{
    xgClassLinks = 0;

    if(gx.GetVariable)
        xgClassLinks = (xgclass_t*) gx.GetVariable(DD_XGFUNC_LINK);

    if(!xgClassLinks)
    {
        memset(&nullXgClassLinks, 0, sizeof(nullXgClassLinks));
        xgClassLinks = &nullXgClassLinks;
    }

    // Let the parser know of the XG classes.
    DED_SetXGClassLinks(xgClassLinks);

    return 1;
}

/**
 * Initializes the definition databases.
 */
void Def_Init(void)
{
    runtimeDefs.clear();
    defs.clear();

    // Make the definitions visible in the global namespace.
    App::app().scriptSystem().addNativeModule("Defs", defs.names);
}

void Def_Destroy(void)
{
    App::app().scriptSystem().removeNativeModule("Defs");

    defs.clear();

    // Destroy the databases.
    runtimeDefs.clear();

    defsInited = false;
}

spritenum_t Def_GetSpriteNum(String const &name)
{
    return Def_GetSpriteNum(name.toLatin1());
}

spritenum_t Def_GetSpriteNum(char const *name)
{
    if(name && name[0])
    {
        for(int i = 0; i < runtimeDefs.sprNames.size(); ++i)
        {
            if(!stricmp(runtimeDefs.sprNames[i].name, name))
                return i;
        }
    }
    return -1; // Not found.
}

int Def_GetMobjNum(const char* id)
{
    return defs.getMobjNum(id);
}

int Def_GetMobjNumForName(const char* name)
{
    return defs.getMobjNumForName(name);
}

const char* Def_GetMobjName(int num)
{
    return defs.getMobjName(num);
}

state_t *Def_GetState(int num)
{
    if(num >= 0 && num < defs.states.size())
    {
        return &runtimeDefs.states[num];
    }
    return 0; // Not found.
}

int Def_GetStateNum(char const *id)
{
    return defs.getStateNum(id);
}

int Def_GetModelNum(const char* id)
{
    return defs.getModelNum(id);
}

int Def_GetSoundNum(const char* id)
{
    return defs.getSoundNum(id);
}

ded_music_t* Def_GetMusic(char const *id)
{
    return defs.getMusic(id);
}

int Def_GetMusicNum(const char* id)
{
    return defs.getMusicNum(id);
}

acfnptr_t Def_GetActionPtr(const char* name)
{
    actionlink_t* linkIt;

    if(!name || !name[0]) return 0;
    if(!App_GameLoaded()) return 0;

    // Action links are provided by the game, who owns the actual action functions.
    for(linkIt = (actionlink_t*) gx.GetVariable(DD_ACTION_LINK);
        linkIt && linkIt->name; linkIt++)
    {
        actionlink_t* link = linkIt;
        if(!stricmp(name, link->name))
            return link->func;
    }
    return 0;
}

int Def_GetActionNum(const char* name)
{
    if(name && name[0] && App_GameLoaded())
    {
        // Action links are provided by the game, who owns the actual action functions.
        actionlink_t* links = (actionlink_t*) gx.GetVariable(DD_ACTION_LINK);
        actionlink_t* linkIt;
        for(linkIt = links; linkIt && linkIt->name; linkIt++)
        {
            actionlink_t* link = linkIt;
            if(!stricmp(name, link->name))
                return linkIt - links;
        }
    }
    return -1; // Not found.
}

ded_value_t* Def_GetValueById(char const* id)
{
    return defs.getValueById(id);
}

ded_value_t* Def_GetValueByUri(struct uri_s const *_uri)
{
    if(!_uri) return 0;
    return defs.getValueByUri(*reinterpret_cast<de::Uri const *>(_uri));
}

ded_mapinfo_t* Def_GetMapInfo(struct uri_s const *_uri)
{
    return defs.getMapInfoNum(reinterpret_cast<de::Uri const *>(_uri));
}

ded_sky_t* Def_GetSky(char const* id)
{
    return defs.getSky(id);
}

ded_compositefont_t* Def_GetCompositeFont(const char* uri)
{
    return defs.getCompositeFont(uri);
}

/// @todo $revise-texture-animation
ded_decor_t *Def_GetDecoration(uri_s const *uri, /*bool hasExternal,*/ bool isCustom)
{
    DENG_ASSERT(uri);

    int i;
    for(i = defs.decorations.size() - 1; i >= 0; i--)
    {
        ded_decor_t *def = &defs.decorations[i];
        if(def->material && Uri_Equality((uri_s *)def->material, uri))
        {
            // Is this suitable?
            if(Def_IsAllowedDecoration(def, /*hasExternal,*/ isCustom))
                return def;
        }
    }
    return 0; // None found.
}

/// @todo $revise-texture-animation
ded_reflection_t *Def_GetReflection(uri_s const *uri, /* bool hasExternal,*/ bool isCustom)
{
    DENG_ASSERT(uri);

    int i;
    for(i = defs.reflections.size() - 1; i >= 0; i--)
    {
        ded_reflection_t *def = &defs.reflections[i];
        if(def->material && Uri_Equality((uri_s *)def->material, uri))
        {
            // Is this suitable?
            if(Def_IsAllowedReflection(def, /*hasExternal,*/ isCustom))
                return def;
        }
    }
    return 0; // None found.
}

/// @todo $revise-texture-animation
ded_detailtexture_t *Def_GetDetailTex(uri_s const *uri, /*bool hasExternal,*/ bool isCustom)
{
    DENG_ASSERT(uri);

    int i;
    for(i = defs.details.size() - 1; i >= 0; i--)
    {
        ded_detailtexture_t *def = &defs.details[i];

        if(def->material1 && Uri_Equality((uri_s *)def->material1, uri))
        {
            // Is this suitable?
            if(Def_IsAllowedDetailTex(def, /*hasExternal,*/ isCustom))
                return def;
        }

        if(def->material2 && Uri_Equality((uri_s *)def->material2, uri))
        {
            // Is this suitable?
            if(Def_IsAllowedDetailTex(def, /*hasExternal,*/ isCustom))
                return def;
        }
    }
    return 0; // None found.
}

ded_ptcgen_t *Def_GetGenerator(de::Uri const &uri)
{
    if(uri.isEmpty()) return 0;

    for(int i = 0; i < defs.ptcGens.size(); ++i)
    {
        ded_ptcgen_t *def = &defs.ptcGens[i];
        if(!def->material) continue;

        // Is this suitable?
        if(*def->material == uri)
            return def;

#if 0 /// @todo $revise-texture-animation
        if(def->flags & PGF_GROUP)
        {
            /**
             * Generator triggered by all materials in the (animation) group.
             * A search is necessary only if we know both the used material and
             * the specified material in this definition are in *a* group.
             */
            if(Material_IsGroupAnimated(defMat) && Material_IsGroupAnimated(mat) &&
               &Material_AnimGroup(defMat) == &Material_AnimGroup(mat))
            {
                // Both are in this group! This def will do.
                return def;
            }
        }
#endif
    }

    return 0; // None found.
}

ded_ptcgen_t *Def_GetGenerator(uri_s const *uri)
{
    if(!uri) return 0;
    return Def_GetGenerator(reinterpret_cast<de::Uri const &>(*uri));
}

ded_ptcgen_t* Def_GetDamageGenerator(int mobjType)
{
    // Search for a suitable definition.
    for(int i = 0; i < defs.ptcGens.size(); ++i)
    {
        ded_ptcgen_t *def = &defs.ptcGens[i];

        // It must be for this type of mobj.
        if(def->damageNum == mobjType)
            return def;
    }
    return 0;
}

#undef Def_EvalFlags
int Def_EvalFlags(char const *ptr)
{
    return defs.evalFlags2(ptr);
}

int Def_GetTextNumForName(const char* name)
{
    return defs.getTextNumForName(name);
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
static void Def_InitTextDef(ddtext_t* txt, char const* str)
{
    // Handle null pointers with "".
    if(!str) str = "";

    txt->text = (char*) M_Calloc(strlen(str) + 1);

    char const* in = str;
    char* out = txt->text;
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
    txt->text = (char*) M_Realloc(txt->text, strlen(txt->text) + 1);
}

/**
 * Prints a count with a 2-space indentation.
 */
static de::String defCountMsg(int count, de::String const &label)
{
    if(!verbose && !count)
        return ""; // Don't print zeros if not verbose.

    return de::String(_E(Ta) "  %1 " _E(Tb) "%2\n").arg(count).arg(label);
}

/**
 * Read all DD_DEFNS lumps in the primary lump index.
 */
static void Def_ReadLumpDefs()
{
    LOG_AS("Def_ReadLumpDefs");

    LumpIndex const &lumpIndex = App_FileSystem().nameIndex();
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

    int const numProcessedLumps = foundDefns.size();
    if(verbose && numProcessedLumps > 0)
    {
        LOG_RES_NOTE("Processed %i %s")
            << numProcessedLumps << (numProcessedLumps != 1 ? "lumps" : "lump");
    }
}

/**
 * Uses gettingFor. Initializes the state-owners information.
 */
int Def_StateForMobj(const char* state)
{
    int                 num = Def_GetStateNum(state);
    int                 st, count = 16;

    if(num < 0)
        num = 0;

    // State zero is the NULL state.
    if(num > 0)
    {
        runtimeDefs.stateInfo[num].owner = gettingFor;
        // Scan forward at most 'count' states, or until we hit a state with
        // an owner, or the NULL state.
        for(st = runtimeDefs.states[num].nextState; st > 0 && count-- && !runtimeDefs.stateInfo[st].owner;
            st = runtimeDefs.states[st].nextState)
        {
            runtimeDefs.stateInfo[st].owner = gettingFor;
        }
    }

    return num;
}

int Def_GetIntValue(char* val, int* returned_val)
{
    char* data;

    // First look for a DED Value
    if(Def_Get(DD_DEF_VALUE, val, &data) >= 0)
    {
        *returned_val = strtol(data, 0, 0);
        return true;
    }

    // Convert the literal string
    *returned_val = strtol(val, 0, 0);
    return false;
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
    DENG2_ASSERT(dst != 0 && src != 0);

    if(!F_IsAbsolute(src))
    {
        char *curPath = Dir_CurrentPath();
        Str_Prepend(dst, curPath);
        Dir_CleanPathStr(dst);
        free(curPath);
        return;
    }

    // Do we need to copy anyway?
    if(dst != src)
    {
        Str_Set(dst, Str_Text(src));
    }
}

static void readAllDefinitions()
{
    de::Time begunAt;

    /*
     * Start with engine's own top-level definition file.
     */

    /*
    String foundPath = App_FileSystem().findPath(de::Uri("doomsday.ded", RC_DEFINITION),
                                                  RLF_DEFAULT, App_ResourceClass(RC_DEFINITION));
    foundPath = App_BasePath() / foundPath; // Ensure the path is absolute.

    readDefinitionFile(foundPath);
    */

    readDefinitionFile(App::packageLoader().package("net.dengine.base").root()
                       .locate<File const>("defs/doomsday.ded").path());

    /*
     * Now any definition files required by the game on load.
     */
    if(App_GameLoaded())
    {
        Game::Manifests const& gameResources = App_CurrentGame().manifests();
        int packageIdx = 0;
        for(Game::Manifests::const_iterator i = gameResources.find(RC_DEFINITION);
            i != gameResources.end() && i.key() == RC_DEFINITION; ++i, ++packageIdx)
        {
            ResourceManifest &record = **i;
            /// Try to locate this resource now.
            QString const &path = record.resolvedPath(true/*try to locate*/);
            if(path.isEmpty())
            {
                QByteArray names = record.names().join(";").toUtf8();
                App_Error("readAllDefinitions: Error, failed to locate required game definition \"%s\".", names.constData());
            }

            readDefinitionFile(path);
        }
    }

    /*
     * Next up are definition files in the games' /auto directory.
     */
    if(!CommandLine_Exists("-noauto") && App_GameLoaded())
    {
        FS1::PathList foundPaths;
        if(App_FileSystem().findAllPaths(de::Uri("$(App.DefsPath)/$(GamePlugin.Name)/auto/*.ded", RC_NULL).resolved(), 0, foundPaths))
        {
            foreach(FS1::PathListItem const &found, foundPaths)
            {
                // Ignore directories.
                if(found.attrib & A_SUBDIR) continue;

                readDefinitionFile(found.path);
            }
        }
    }

    /*
     * Next up are any definition files specified on the command line.
     */
    AutoStr *buf = AutoStr_NewStd();
    for(int p = 0; p < CommandLine_Count(); ++p)
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
        p--; /* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }

    /*
     * Last up are any DD_DEFNS definition lumps from loaded add-ons.
     */
    Def_ReadLumpDefs();

    LOG_RES_VERBOSE("readAllDefinitions: Completed in %.2f seconds") << begunAt.since();
}

static AnimGroup const *findAnimGroupForTexture(TextureManifest &textureManifest)
{
    // Group ids are 1-based.
    // Search backwards to allow patching.
    for(int i = App_ResourceSystem().animGroupCount(); i > 0; i--)
    {
        AnimGroup *animGroup = App_ResourceSystem().animGroup(i);
        if(animGroup->hasFrameFor(textureManifest))
        {
            return animGroup;
        }
    }
    return 0; // Not found.
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

    App_ResourceSystem().defineTexture("Flaremaps", resourceUri);
}

static void defineLightmap(de::Uri const &resourceUri)
{
    if(resourceUri.isEmpty()) return;

    // Reference to none?
    if(!resourceUri.path().toStringRef().compareWithoutCase("-")) return;

    App_ResourceSystem().defineTexture("Lightmaps", resourceUri);
}

static void generateMaterialDefForTexture(TextureManifest &manifest)
{
    LOG_AS("generateMaterialDefForTexture");

    de::Uri texUri = manifest.composeUri();

    int matIdx = DED_AddMaterial(&defs, 0);
    ded_material_t *mat = &defs.materials[matIdx];
    mat->autoGenerated = true;
    mat->uri = new de::Uri(DD_MaterialSchemeNameForTextureScheme(texUri.scheme()), texUri.path());

    if(manifest.hasTexture())
    {
        Texture &tex = manifest.texture();
        mat->width  = tex.width();
        mat->height = tex.height();
        mat->flags = (tex.isFlagged(Texture::NoDraw)? Material::NoDraw : 0);
    }
    else
    {
        LOGDEV_RES_MSG("Texture \"%s\" not yet defined, resultant Material will inherit dimensions") << texUri;
    }

    // The first stage is implicit.
    int layerIdx = DED_AddMaterialLayerStage(&mat->layers[0]);
    ded_material_layer_stage_t *st = &mat->layers[0].stages[layerIdx];
    DENG_ASSERT(st != 0);
    st->texture = new de::Uri(texUri);

    // Is there an animation for this?
    AnimGroup const *anim = findAnimGroupForTexture(manifest);
    if(anim && anim->frameCount() > 1)
    {
        AnimGroupFrame const *animFrame;

        // Determine the start frame.
        int startFrame = 0;
        while(&anim->frame(startFrame).textureManifest() != &manifest)
        {
            startFrame++;
        }

        // Just animate the first in the sequence?
        if(startFrame && (anim->flags() & AGF_FIRST_ONLY))
            return;

        // Complete configuration of the first stage.
        animFrame = &anim->frame(startFrame);
        st->tics = animFrame->tics() + animFrame->randomTics();
        if(animFrame->randomTics())
        {
            st->variance = animFrame->randomTics() / float( st->tics );
        }

        // Add further stages according to the animation group.
        startFrame++;
        for(int i = 0; i < anim->frameCount() - 1; ++i)
        {
            int frame = de::wrap(startFrame + i, 0, anim->frameCount());

            animFrame = &anim->frame(frame);
            TextureManifest &frameManifest = animFrame->textureManifest();

            int layerIdx = DED_AddMaterialLayerStage(&mat->layers[0]);
            ded_material_layer_stage_t *st = &mat->layers[0].stages[layerIdx];
            st->texture = new de::Uri(frameManifest.composeUrn());
            st->tics = animFrame->tics() + animFrame->randomTics();
            if(animFrame->randomTics())
            {
                st->variance = animFrame->randomTics() / float( st->tics );
            }
        }
    }
}

static void generateMaterialDefsForAllTexturesInScheme(String schemeName)
{
    TextureScheme &scheme = App_ResourceSystem().textureScheme(schemeName);

    PathTreeIterator<TextureScheme::Index> iter(scheme.index().leafNodes());
    while(iter.hasNext())
    {
        generateMaterialDefForTexture(iter.next());
    }
}

static void generateMaterialDefs()
{
    generateMaterialDefsForAllTexturesInScheme("Textures");
    generateMaterialDefsForAllTexturesInScheme("Flats");
    generateMaterialDefsForAllTexturesInScheme("Sprites");
}

static ded_group_t *findGroupDefByFrameTextureUri(de::Uri const &uri)
{
    if(uri.isEmpty()) return 0;

    // Reverse iteration (later defs override earlier ones).
    for(int i = defs.groups.size(); i--> 0; )
    {
        ded_group_t &grp = defs.groups[i];

        // We aren't interested in precache groups.
        if(grp.flags & AGF_PRECACHE) continue;

        // Or empty/single-frame groups.
        if(grp.members.size() < 2) continue;

        for(int k = 0; k < grp.members.size(); ++k)
        {
            ded_group_member_t &gm = grp.members[k];

            if(!gm.material) continue;

            if(*gm.material == uri)
            {
                // Found one.
                return &grp;
            }

            // Only animate if the first frame in the group?
            if(grp.flags & AGF_FIRST_ONLY) break;
        }
    }

    return 0; // Not found.
}

static void rebuildMaterialLayers(Material &material, ded_material_t const &def)
{
    material.clearLayers();

    for(int i = 0; i < DED_MAX_MATERIAL_LAYERS; ++i)
    {
        material.newLayer(&def.layers[i]);
    }

    if(material.layerCount() && material.layers()[0]->stageCount())
    {
        Material::Layer *layer0 = material.layers()[0];
        Material::Layer::Stage *stage0 = layer0->stages()[0];

        if(stage0->texture)
        {
            // We may need to interpret the layer animation from the now
            // deprecated Group definitions.

            if(def.autoGenerated && layer0->stageCount() == 1)
            {
                de::Uri textureUri(stage0->texture->manifest().composeUri());

                // Possibly; see if there is a compatible definition with
                // a member named similarly to the texture for layer #0.

                if(ded_group_t const *grp = findGroupDefByFrameTextureUri(textureUri))
                {
                    // Determine the start frame.
                    int startFrame = 0;
                    while(!grp->members[startFrame].material ||
                          *grp->members[startFrame].material != textureUri)
                    {
                        startFrame++;
                    }

                    // Configure the first stage.
                    ded_group_member_t const &gm0 = grp->members[startFrame];
                    stage0->tics = gm0.tics;
                    stage0->variance = gm0.randomTics / float( gm0.tics );

                    // Add further stages for each frame in the group.
                    startFrame++;
                    for(int i = 0; i < grp->members.size() - 1; ++i)
                    {
                        int frame = de::wrap(startFrame + i, 0, grp->members.size());
                        ded_group_member_t const &gm = grp->members[frame];

                        if(!gm.material) continue;

                        try
                        {
                            Texture &texture = App_ResourceSystem().texture(*gm.material);
                            layer0->addStage(Material::Layer::Stage(&texture, gm.tics, gm.randomTics / float( gm.tics )));
                        }
                        catch(TextureManifest::MissingTextureError const &)
                        {} // Ignore this error.
                        catch(ResourceSystem::MissingManifestError const &)
                        {} // Ignore this error.
                    }
                }
            }

            if(!material.isDetailed())
            {
                // Are there Detail definitions we need to produce a layer for?
                Material::DetailLayer *dlayer = 0;

                for(int i = 0; i < layer0->stageCount(); ++i)
                {
                    Material::Layer::Stage *stage = layer0->stages()[i];
                    de::Uri textureUri(stage->texture->manifest().composeUri());

                    ded_detailtexture_t const *detailDef =
                        Def_GetDetailTex(reinterpret_cast<uri_s *>(&textureUri),
                                         /*UNKNOWN VALUE,*/ material.manifest().isCustom());

                    if(!detailDef || !detailDef->stage.texture) continue;

                    if(!dlayer)
                    {
                        // Add a new detail layer.
                        dlayer = material.newDetailLayer(detailDef);
                    }
                    else
                    {
                        // Add a new stage.
                        try
                        {
                            Texture &texture = App_ResourceSystem().textureScheme("Details").findByResourceUri(*detailDef->stage.texture).texture();
                            dlayer->addStage(Material::DetailLayer::Stage(&texture, stage->tics, stage->variance,
                                                                          detailDef->stage.scale, detailDef->stage.strength,
                                                                          detailDef->stage.maxDistance));

                            if(dlayer->stageCount() == 2)
                            {
                                // Update the first stage with timing info.
                                Material::Layer::Stage const *stage0  = layer0->stages()[0];
                                Material::DetailLayer::Stage *dstage0 = dlayer->stages()[0];

                                dstage0->tics     = stage0->tics;
                                dstage0->variance = stage0->variance;
                            }
                        }
                        catch(TextureManifest::MissingTextureError const &)
                        {} // Ignore this error.
                        catch(ResourceSystem::MissingManifestError const &)
                        {} // Ignore this error.
                    }
                }
            }

            if(!material.isShiny())
            {
                // Are there Reflection definition we need to produce a layer for?
                Material::ShineLayer *slayer = 0;

                for(int i = 0; i < layer0->stageCount(); ++i)
                {
                    Material::Layer::Stage *stage = layer0->stages()[i];
                    de::Uri textureUri(stage->texture->manifest().composeUri());

                    ded_reflection_t const *shineDef =
                        Def_GetReflection(reinterpret_cast<uri_s *>(&textureUri),
                                          /*UNKNOWN VALUE,*/ material.manifest().isCustom());

                    if(!shineDef || !shineDef->stage.texture) continue;

                    if(!slayer)
                    {
                        // Add a new shine layer.
                        slayer = material.newShineLayer(shineDef);
                    }
                    else
                    {
                        // Add a new stage.
                        try
                        {
                            Texture &texture = App_ResourceSystem().textureScheme("Reflections")
                                                   .findByResourceUri(*shineDef->stage.texture).texture();

                            Texture *maskTexture = 0;
                            if(shineDef->stage.maskTexture)
                            {
                                try
                                {
                                    maskTexture = &App_ResourceSystem().textureScheme("Masks")
                                                       .findByResourceUri(*shineDef->stage.maskTexture).texture();
                                }
                                catch(TextureManifest::MissingTextureError const &)
                                {} // Ignore this error.
                                catch(ResourceSystem::MissingManifestError const &)
                                {} // Ignore this error.
                            }

                            slayer->addStage(Material::ShineLayer::Stage(&texture, stage->tics, stage->variance,
                                                                         maskTexture, shineDef->stage.blendMode,
                                                                         shineDef->stage.shininess,
                                                                         Vector3f(shineDef->stage.minColor),
                                                                         Vector2f(shineDef->stage.maskWidth, shineDef->stage.maskHeight)));

                            if(slayer->stageCount() == 2)
                            {
                                // Update the first stage with timing info.
                                Material::Layer::Stage const *stage0 = layer0->stages()[0];
                                Material::ShineLayer::Stage *sstage0 = slayer->stages()[0];

                                sstage0->tics     = stage0->tics;
                                sstage0->variance = stage0->variance;
                            }
                        }
                        catch(TextureManifest::MissingTextureError const &)
                        {} // Ignore this error.
                        catch(ResourceSystem::MissingManifestError const &)
                        {} // Ignore this error.
                    }
                }
            }
        }
    }
}

#ifdef __CLIENT__
static void rebuildMaterialDecorations(Material &material, ded_material_t const &def)
{
    material.clearDecorations();

    // Add (light) decorations to the material.
    // Prefer decorations defined within the material.
    for(int i = 0; i < DED_MAX_MATERIAL_DECORATIONS; ++i)
    {
        ded_material_decoration_t const &lightDef = def.decorations[i];

        // Is this valid? (A zero number of stages signifies the last).
        if(!lightDef.stages.size()) break;

        for(int k = 0; k < lightDef.stages.size(); ++k)
        {
            ded_decorlight_stage_t *stage = &lightDef.stages[k];

            if(stage->up)
            {
                defineLightmap(*stage->up);
            }
            if(stage->down)
            {
                defineLightmap(*stage->down);
            }
            if(stage->sides)
            {
                defineLightmap(*stage->sides);
            }
            if(stage->flare)
            {
                defineFlaremap(*stage->flare);
            }
        }

        MaterialDecoration *decor = MaterialDecoration::fromDef(lightDef);
        material.addDecoration(*decor);
    }

    if(!material.decorationCount())
    {
        // Perhaps an oldschool linked decoration definition?
        de::Uri materialUri = material.manifest().composeUri();
        ded_decor_t *decorDef = Def_GetDecoration(reinterpret_cast<uri_s *>(&materialUri),
                                                  material.manifest().isCustom());
        if(decorDef)
        {
            for(int i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
            {
                ded_decoration_t &lightDef = decorDef->lights[i];
                // Is this valid? (A zero-strength color signifies the last).
                if(V3f_IsZero(lightDef.stage.color)) break;

                MaterialDecoration *decor = MaterialDecoration::fromDef(lightDef);
                material.addDecoration(*decor);
            }
        }
    }
}
#endif // __CLIENT__

static Material::Flags translateMaterialDefFlags(ded_flags_t flags)
{
    Material::Flags mf;
    if(flags & MATF_NO_DRAW) mf |= Material::NoDraw;
    if(flags & MATF_SKYMASK) mf |= Material::SkyMask;
    return mf;
}

static void interpretMaterialDef(ded_material_t const &def)
{
    LOG_AS("interpretMaterialDef");

    if(!def.uri) return;

    try
    {
        // Create/retrieve a manifest for the would-be material.
        MaterialManifest *manifest = &App_ResourceSystem().declareMaterial(*def.uri);

        // Update manifest classification:
        manifest->setFlags(MaterialManifest::AutoGenerated, def.autoGenerated? SetFlags : UnsetFlags);
        manifest->setFlags(MaterialManifest::Custom, UnsetFlags);
        if(def.layers[0].stages.size() > 0)
        {
            ded_material_layer_t const &firstLayer = def.layers[0];
            if(firstLayer.stages[0].texture) // Not unused.
            {
                try
                {
                    Texture &texture = App_ResourceSystem().texture(*firstLayer.stages[0].texture);
                    if(texture.isFlagged(Texture::Custom))
                    {
                        manifest->setFlags(MaterialManifest::Custom);
                    }
                }
                catch(ResourceSystem::MissingManifestError const &er)
                {
                    // Log but otherwise ignore this error.
                    LOG_RES_WARNING("Ignoring unknown texture \"%s\" in Material \"%s\" (layer %i stage %i): %s")
                        << *firstLayer.stages[0].texture
                        << *def.uri
                        << 0 << 0
                        << er.asText();
                }
            }
        }

        /*
         * (Re)configure the material:
         */
        /// @todo Defer until necessary.
        Material &material = *manifest->derive();

        material.setFlags(translateMaterialDefFlags(def.flags));
        material.setDimensions(Vector2i(def.width, def.height));
#ifdef __CLIENT__
        material.setAudioEnvironment(S_AudioEnvironmentId(def.uri));
#endif

        rebuildMaterialLayers(material, def);
#ifdef __CLIENT__
        rebuildMaterialDecorations(material, def);
#endif

        material.markValid(true);
    }
    catch(ResourceSystem::UnknownSchemeError const &er)
    {
        LOG_RES_WARNING("Failed to declare material \"%s\": %s")
            << *def.uri << er.asText();
    }
    catch(MaterialScheme::InvalidPathError const &er)
    {
        LOG_RES_WARNING("Failed to declare material \"%s\": %s")
            << *def.uri << er.asText();
    }
}

static void invalidateAllMaterials()
{
    foreach(Material *material, App_ResourceSystem().allMaterials())
    {
        material->markValid(false);
    }
}

#ifdef __CLIENT__
static void clearFontDefinitionLinks()
{
    foreach(AbstractFont *font, App_ResourceSystem().allFonts())
    {
        if(CompositeBitmapFont *compFont = font->maybeAs<CompositeBitmapFont>())
        {
            compFont->setDefinition(0);
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
        FS1::Scheme &scheme = App_FileSystem().scheme(App_ResourceClass("RC_MODEL").defaultScheme());
        scheme.reset();

        invalidateAllMaterials();
#ifdef __CLIENT__
        clearFontDefinitionLinks();
#endif

        Def_Destroy();
    }

    firstDED = true;

    // Now we can clear all existing definitions and re-init.
    defs.clear();

    // Generate definitions.
    generateMaterialDefs();

    // Read all definitions files and lumps.
    LOG_RES_MSG("Parsing definition files...");
    readAllDefinitions();

    // Any definition hooks?
    DD_CallHooks(HOOK_DEFS, 0, &defs);

#ifdef __CLIENT__
    // Composite fonts.
    for(int i = 0; i < defs.compositeFonts.size(); ++i)
    {
        App_ResourceSystem().newFontFromDef(defs.compositeFonts[i]);
    }
#endif

    // Sprite names.
    runtimeDefs.sprNames.append(defs.sprites.size());
    for(int i = 0; i < runtimeDefs.sprNames.size(); ++i)
    {
        strcpy(runtimeDefs.sprNames[i].name, defs.sprites[i].id);
    }

    // States.
    runtimeDefs.states.append(defs.states.size());

    for(int i = 0; i < runtimeDefs.states.size(); ++i)
    {
        ded_state_t *dst = &defs.states[i];
        // Make sure duplicate IDs overwrite the earliest.
        int stateNum = Def_GetStateNum(dst->id);

        if(stateNum == -1) continue;

        ded_state_t *dstNew = &defs.states[stateNum];
        state_t *st = &runtimeDefs.states[stateNum];

        st->sprite    = Def_GetSpriteNum(dst->sprite.id);
        st->flags     = dst->flags;
        st->frame     = dst->frame;
        st->tics      = dst->tics;
        st->action    = Def_GetActionPtr(dst->action);
        st->nextState = Def_GetStateNum(dst->nextState);

        for(int k = 0; k < NUM_STATE_MISC; ++k)
        {
            st->misc[k] = dst->misc[k];
        }

        // Replace the older execute string.
        if(dst != dstNew)
        {
            if(dstNew->execute)
                M_Free(dstNew->execute);
            dstNew->execute = dst->execute;
            dst->execute = 0;
        }
    }

    runtimeDefs.stateInfo.append(defs.states.size());

    // Mobj info.
    runtimeDefs.mobjInfo.append(defs.mobjs.size());

    for(int i = 0; i < runtimeDefs.mobjInfo.size(); ++i)
    {
        ded_mobj_t* dmo = &defs.mobjs[i];
        // Make sure duplicate defs overwrite the earliest.
        mobjinfo_t* mo = &runtimeDefs.mobjInfo[Def_GetMobjNum(dmo->id)];

        gettingFor       = mo;
        mo->doomEdNum    = dmo->doomEdNum;
        mo->spawnHealth  = dmo->spawnHealth;
        mo->reactionTime = dmo->reactionTime;
        mo->painChance   = dmo->painChance;
        mo->speed        = dmo->speed;
        mo->radius       = dmo->radius;
        mo->height       = dmo->height;
        mo->mass         = dmo->mass;
        mo->damage       = dmo->damage;
        mo->flags        = dmo->flags[0];
        mo->flags2       = dmo->flags[1];
        mo->flags3       = dmo->flags[2];
        for(int k = 0; k < STATENAMES_COUNT; ++k)
        {
            mo->states[k] = Def_StateForMobj(dmo->states[k]);
        }
        mo->seeSound     = Def_GetSoundNum(dmo->seeSound);
        mo->attackSound  = Def_GetSoundNum(dmo->attackSound);
        mo->painSound    = Def_GetSoundNum(dmo->painSound);
        mo->deathSound   = Def_GetSoundNum(dmo->deathSound);
        mo->activeSound  = Def_GetSoundNum(dmo->activeSound);
        for(int k = 0; k < NUM_MOBJ_MISC; ++k)
        {
            mo->misc[k] = dmo->misc[k];
        }
    }

    // Decorations. (Define textures).
    for(int i = 0; i < defs.decorations.size(); ++i)
    {
        ded_decor_t *dec = &defs.decorations[i];
        for(int k = 0; k < DED_DECOR_NUM_LIGHTS; ++k)
        {
            ded_decoration_t *dl = &dec->lights[k];

            if(V3f_IsZero(dl->stage.color)) break;

            if(dl->stage.up)
            {
                defineLightmap(*dl->stage.up);
            }
            if(dl->stage.down)
            {
                defineLightmap(*dl->stage.down);
            }
            if(dl->stage.sides)
            {
                defineLightmap(*dl->stage.sides);
            }
            if(dl->stage.flare)
            {
                defineFlaremap(*dl->stage.flare);
            }
        }
    }

    // Detail textures (Define textures).
    App_ResourceSystem().textureScheme("Details").clear();
    for(int i = 0; i < defs.details.size(); ++i)
    {
        ded_detailtexture_t *dtl = &defs.details[i];

        // Ignore definitions which do not specify a material.
        if((!dtl->material1 || dtl->material1->isEmpty()) &&
           (!dtl->material2 || dtl->material2->isEmpty())) continue;

        if(!dtl->stage.texture) continue;

        App_ResourceSystem().defineTexture("Details", *dtl->stage.texture);
    }

    // Surface reflections (Define textures).
    App_ResourceSystem().textureScheme("Reflections").clear();
    App_ResourceSystem().textureScheme("Masks").clear();
    for(int i = 0; i < defs.reflections.size(); ++i)
    {
        ded_reflection_t *ref = &defs.reflections[i];

        // Ignore definitions which do not specify a material.
        if(!ref->material || ref->material->isEmpty()) continue;

        if(ref->stage.texture)
        {
            App_ResourceSystem().defineTexture("Reflections", *ref->stage.texture);
        }
        if(ref->stage.maskTexture)
        {
            App_ResourceSystem().defineTexture("Masks", *ref->stage.maskTexture,
                            Vector2i(ref->stage.maskWidth, ref->stage.maskHeight));
        }
    }

    // Materials.
    for(int i = 0; i < defs.materials.size(); ++i)
    {
        interpretMaterialDef(defs.materials[i]);
    }

    //DED_NewEntries((void **) &stateLights, &countStateLights, sizeof(*stateLights), defs.states.size());

    // Dynamic lights. Update the sprite numbers.
    for(int i = 0; i < defs.lights.size(); ++i)
    {
        int const stateIdx = Def_GetStateNum(defs.lights[i].state);
        if(stateIdx < 0)
        {
            // It's probably a bias light definition, then?
            if(!defs.lights[i].uniqueMapID[0])
            {
                LOG_RES_WARNING("Undefined state '%s' in Light definition") << defs.lights[i].state;
            }
            continue;
        }
        runtimeDefs.stateInfo[stateIdx].light = &defs.lights[i];
    }

    // Sound effects.
    runtimeDefs.sounds.append(defs.sounds.size());

    for(int i = 0; i < runtimeDefs.sounds.size(); ++i)
    {
        ded_sound_t *snd = &defs.sounds[i];
        // Make sure duplicate defs overwrite the earliest.
        sfxinfo_t *si = &runtimeDefs.sounds[Def_GetSoundNum(snd->id)];

        strcpy(si->id, snd->id);
        strcpy(si->lumpName, snd->lumpName);
        si->lumpNum     = (strlen(snd->lumpName) > 0? App_FileSystem().lumpNumForName(snd->lumpName) : -1);
        strcpy(si->name, snd->name);

        int const soundIdx = Def_GetSoundNum(snd->link);
        si->link        = (soundIdx >= 0 ? &runtimeDefs.sounds[soundIdx] : 0);

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
    for(int i = 0; i < defs.music.size(); ++i)
    {
        ded_music_t *mus = &defs.music[i];
        // Make sure duplicate defs overwrite the earliest.
        ded_music_t *earliest = &defs.music[Def_GetMusicNum(mus->id)];

        if(earliest == mus) continue;

        strcpy(earliest->lumpName, mus->lumpName);
        earliest->cdTrack = mus->cdTrack;

        if(mus->path)
        {
            if(earliest->path)
                *earliest->path = *mus->path;
            else
                earliest->path = new de::Uri(*mus->path);
        }
        else if(earliest->path)
        {
            delete earliest->path;
            earliest->path = 0;
        }
    }

    // Text.
    runtimeDefs.texts.append(defs.text.size());

    for(int i = 0; i < defs.text.size(); ++i)
    {
        Def_InitTextDef(&runtimeDefs.texts[i], defs.text[i].text);
    }

    // Handle duplicate strings.
    for(int i = 0; i < runtimeDefs.texts.size(); ++i)
    {
        if(!runtimeDefs.texts[i].text) continue;

        for(int k = i + 1; k < runtimeDefs.texts.size(); ++k)
        {
            if(!runtimeDefs.texts[k].text) continue; // Already done.
            if(stricmp(defs.text[i].id, defs.text[k].id)) continue; // ID mismatch.

            // Update the earlier string.
            runtimeDefs.texts[i].text = (char *) M_Realloc(runtimeDefs.texts[i].text, strlen(runtimeDefs.texts[k].text) + 1);
            strcpy(runtimeDefs.texts[i].text, runtimeDefs.texts[k].text);

            // Free the later string, it isn't used (>NUMTEXT).
            M_Free(runtimeDefs.texts[k].text);
            runtimeDefs.texts[k].text = 0;
        }
    }

    //DED_NewEntries((void **) &statePtcGens, &countStatePtcGens, sizeof(*statePtcGens), defs.states.size());

    // Particle generators.
    for(int i = 0; i < defs.ptcGens.size(); ++i)
    {
        ded_ptcgen_t *pg = &defs.ptcGens[i];
        int st = Def_GetStateNum(pg->state);

        if(!strcmp(pg->type, "*"))
            pg->typeNum = DED_PTCGEN_ANY_MOBJ_TYPE;
        else
            pg->typeNum = Def_GetMobjNum(pg->type);
        pg->type2Num = Def_GetMobjNum(pg->type2);
        pg->damageNum = Def_GetMobjNum(pg->damage);

        // Figure out embedded sound ID numbers.
        for(int k = 0; k < pg->stages.size(); ++k)
        {
            if(pg->stages[k].sound.name[0])
            {
                pg->stages[k].sound.id =
                    Def_GetSoundNum(pg->stages[k].sound.name);
            }
            if(pg->stages[k].hitSound.name[0])
            {
                pg->stages[k].hitSound.id =
                    Def_GetSoundNum(pg->stages[k].hitSound.name);
            }
        }

        if(st <= 0)
            continue; // Not state triggered, then...

        stateinfo_t *stinfo = &runtimeDefs.stateInfo[st];

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

                stinfo->ptcGens->stateNext = 0;
                stinfo->ptcGens = temp;
            }
            stinfo->ptcGens = pg;
            pg->stateNext = 0;
        }
    }

    // Map infos.
    for(int i = 0; i < defs.mapInfo.size(); ++i)
    {
        ded_mapinfo_t *mi = &defs.mapInfo[i];

        /**
         * Historically, the map info flags field was used for sky flags,
         * here we copy those flags to the embedded sky definition for
         * backward-compatibility.
         */
        if(mi->flags & MIF_DRAW_SPHERE)
            mi->sky.flags |= SIF_DRAW_SPHERE;
    }

    // Log a summary of the definition database.
    LOG_RES_MSG(_E(b) "Definitions:");
    de::String str;
    QTextStream os(&str);
    os << defCountMsg(defs.groups.size(), "animation groups");
    os << defCountMsg(defs.compositeFonts.size(), "composite fonts");
    os << defCountMsg(defs.details.size(), "detail textures");
    os << defCountMsg(defs.finales.size(), "finales");
    os << defCountMsg(defs.lights.size(), "lights");
    os << defCountMsg(defs.lineTypes.size(), "line types");
    os << defCountMsg(defs.mapInfos.size(), "map infos");

    int nonAutoGeneratedCount = 0;
    for(int i = 0; i < defs.materials.size(); ++i)
    {
        if(!defs.materials[i].autoGenerated)
            ++nonAutoGeneratedCount;
    }
    os << defCountMsg(nonAutoGeneratedCount, "materials");

    os << defCountMsg(defs.models.size(), "models");
    os << defCountMsg(defs.ptcGens.size(), "particle generators");
    os << defCountMsg(defs.skies.size(), "skies");
    os << defCountMsg(defs.sectorTypes.size(), "sector types");
    os << defCountMsg(defs.music.size(), "songs");
    os << defCountMsg(runtimeDefs.sounds.size(), "sound effects");
    os << defCountMsg(runtimeDefs.sprNames.size(), "sprite names");
    os << defCountMsg(runtimeDefs.states.size(), "states");
    os << defCountMsg(defs.decorations.size(), "surface decorations");
    os << defCountMsg(defs.reflections.size(), "surface reflections");
    os << defCountMsg(runtimeDefs.texts.size(), "text strings");
    os << defCountMsg(defs.textureEnv.size(), "texture environments");
    os << defCountMsg(runtimeDefs.mobjInfo.size(), "things");

    LOG_RES_MSG("%s") << str.rightStrip();

    defsInited = true;
}

static void initMaterialGroup(ded_group_t &def)
{
    ResourceSystem::MaterialManifestGroup *group = 0;
    for(int i = 0; i < def.members.size(); ++i)
    {
        ded_group_member_t *gm = &def.members[i];
        if(!gm->material) continue;

        try
        {
            MaterialManifest &manifest = App_ResourceSystem().materialManifest(*gm->material);

            if(def.flags & AGF_PRECACHE) // A precache group.
            {
                // Only create the group once the first material has been found.
                if(!group)
                {
                    group = &App_ResourceSystem().newMaterialGroup();
                }

                group->insert(&manifest);
            }
#if 0 /// @todo $revise-texture-animation
            else // An animation group.
            {
                // Only create the group once the first material has been found.
                if(animNumber == -1)
                {
                    animNumber = App_ResourceSystem().newAnimGroup(def.flags & ~AGF_PRECACHE);
                }

                App_ResourceSystem().animGroup(animNumber).addFrame(manifest.material(), gm->tics, gm->randomTics);
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
    for(int i = 0; i < defs.ptcGens.size(); ++i)
    {
        ded_ptcgen_t *gen = &defs.ptcGens[i];

        for(int k = 0; k < gen->stages.size(); ++k)
        {
            ded_ptcstage_t *st = &gen->stages[k];

            if(st->type < PTC_MODEL || st->type >= PTC_MODEL + MAX_PTC_MODELS)
                continue;

            st->model = -1;
            try
            {
                ModelDef &modef = App_ResourceSystem().modelDef(String("Particle%1").arg(st->type - PTC_MODEL, 2, 10, QChar('0')));
                if(modef.subModelId(0) == NOMODELID)
                {
                    continue;
                }

                Model &mdl = App_ResourceSystem().model(modef.subModelId(0));

                st->model = App_ResourceSystem().indexOf(&modef);
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
            {} // Ignore this error.
        }
    }

#endif // __CLIENT__

    // Lights.
    for(int i = 0; i < defs.lights.size(); ++i)
    {
        ded_light_t *lig = &defs.lights[i];

        if(lig->up)
        {
            defineLightmap(*lig->up);
        }
        if(lig->down)
        {
            defineLightmap(*lig->down);
        }
        if(lig->sides)
        {
            defineLightmap(*lig->sides);
        }
        if(lig->flare)
        {
            defineFlaremap(*lig->flare);
        }
    }

    // Material groups (e.g., for precaching).
    App_ResourceSystem().clearAllMaterialGroups();
    for(int i = 0; i < defs.groups.size(); ++i)
    {
        initMaterialGroup(defs.groups[i]);
    }
}

/**
 * Can we reach 'snew' if we start searching from 'sold'?
 * Take a maximum of 16 steps.
 */
dd_bool Def_SameStateSequence(state_t* snew, state_t* sold)
{
    int it;
    int target = runtimeDefs.states.indexOf(snew);
    int start  = runtimeDefs.states.indexOf(sold);
    int count = 0;

    if(!snew || !sold)
        return false;

    if(snew == sold)
        return true; // Trivial.

    for(it = sold->nextState; it >= 0 && it != start && count < 16;
        it = runtimeDefs.states[it].nextState, ++count)
    {
        if(it == target)
            return true;

        if(it == runtimeDefs.states[it].nextState)
            break;
    }
    return false;
}

const char* Def_GetStateName(state_t* state)
{
    int idx = runtimeDefs.states.indexOf(state);
    if(!state) return "(nullptr)";
    //if(idx < 0) return "(<0)";
    //if(idx >= defs.states.size()) return "(>states)";
    return defs.states[idx].id;
}

static int Friendly(int num)
{
    if(num < 0)
        num = 0;
    return num;
}

/**
 * Converts a DED line type to the internal format.
 * Bit of a nuisance really...
 */
void Def_CopyLineType(linetype_t* l, ded_linetype_t* def)
{
    int i, k, a, temp;

    l->id = def->id;
    l->flags = def->flags[0];
    l->flags2 = def->flags[1];
    l->flags3 = def->flags[2];
    l->lineClass = def->lineClass;
    l->actType = def->actType;
    l->actCount = def->actCount;
    l->actTime = def->actTime;
    l->actTag = def->actTag;

    for(i = 0; i < 10; ++i)
    {
        if(i == 9)
            l->aparm[i] = Def_GetMobjNum(def->aparm9);
        else
            l->aparm[i] = def->aparm[i];
    }

    l->tickerStart = def->tickerStart;
    l->tickerEnd = def->tickerEnd;
    l->tickerInterval = def->tickerInterval;
    l->actSound = Friendly(Def_GetSoundNum(def->actSound));
    l->deactSound = Friendly(Def_GetSoundNum(def->deactSound));
    l->evChain = def->evChain;
    l->actChain = def->actChain;
    l->deactChain = def->deactChain;
    l->actLineType = def->actLineType;
    l->deactLineType = def->deactLineType;
    l->wallSection = def->wallSection;

    if(def->actMaterial)
    {
        try
        {
            l->actMaterial = App_ResourceSystem().materialManifest(*def->actMaterial).id();
        }
        catch(ResourceSystem::MissingManifestError const &)
        {} // Ignore this error.
    }

    if(def->deactMaterial)
    {
        try
        {
            l->deactMaterial = App_ResourceSystem().materialManifest(*def->deactMaterial).id();
        }
        catch(ResourceSystem::MissingManifestError const &)
        {} // Ignore this error.
    }

    l->actMsg = def->actMsg;
    l->deactMsg = def->deactMsg;
    l->materialMoveAngle = def->materialMoveAngle;
    l->materialMoveSpeed = def->materialMoveSpeed;
    memcpy(l->iparm, def->iparm, sizeof(int) * 20);
    memcpy(l->fparm, def->fparm, sizeof(int) * 20);
    LOOPi(5) l->sparm[i] = def->sparm[i];

    // Some of the parameters might be strings depending on the line class.
    // Find the right mapping table.
    for(k = 0; k < 20; ++k)
    {
        temp = 0;
        a = xgClassLinks[l->lineClass].iparm[k].map;
        if(a < 0)
            continue;

        if(a & MAP_SND)
        {
            l->iparm[k] = Friendly(Def_GetSoundNum(def->iparmStr[k]));
        }
        else if(a & MAP_MATERIAL)
        {
            if(def->iparmStr[k][0])
            {
                if(!stricmp(def->iparmStr[k], "-1"))
                {
                    l->iparm[k] = -1;
                }
                else
                {
                    try
                    {
                        l->iparm[k] = App_ResourceSystem().materialManifest(de::Uri(def->iparmStr[k], RC_NULL)).id();
                    }
                    catch(ResourceSystem::MissingManifestError const &)
                    {} // Ignore this error.
                }
            }
        }
        else if(a & MAP_MUS)
        {
            temp = Friendly(Def_GetMusicNum(def->iparmStr[k]));

            if(temp == 0)
            {
                temp = Def_EvalFlags(def->iparmStr[k]);
                if(temp)
                    l->iparm[k] = temp;
            }
            else
            {
                l->iparm[k] = Friendly(Def_GetMusicNum(def->iparmStr[k]));
            }
        }
        else
        {
            temp = Def_EvalFlags(def->iparmStr[k]);

            if(temp)
                l->iparm[k] = temp;
        }
    }
}

/**
 * Converts a DED sector type to the internal format.
 */
void Def_CopySectorType(sectortype_t* s, ded_sectortype_t* def)
{
    int                 i, k;

    s->id = def->id;
    s->flags = def->flags;
    s->actTag = def->actTag;
    LOOPi(5)
    {
        s->chain[i] = def->chain[i];
        s->chainFlags[i] = def->chainFlags[i];
        s->start[i] = def->start[i];
        s->end[i] = def->end[i];
        LOOPk(2) s->interval[i][k] = def->interval[i][k];
        s->count[i] = def->count[i];
    }
    s->ambientSound = Friendly(Def_GetSoundNum(def->ambientSound));
    LOOPi(2)
    {
        s->soundInterval[i] = def->soundInterval[i];
        s->materialMoveAngle[i] = def->materialMoveAngle[i];
        s->materialMoveSpeed[i] = def->materialMoveSpeed[i];
    }
    s->windAngle = def->windAngle;
    s->windSpeed = def->windSpeed;
    s->verticalWind = def->verticalWind;
    s->gravity = def->gravity;
    s->friction = def->friction;
    s->lightFunc = def->lightFunc;
    LOOPi(2) s->lightInterval[i] = def->lightInterval[i];
    LOOPi(3)
    {
        s->colFunc[i] = def->colFunc[i];
        LOOPk(2) s->colInterval[i][k] = def->colInterval[i][k];
    }
    s->floorFunc = def->floorFunc;
    s->floorMul = def->floorMul;
    s->floorOff = def->floorOff;
    LOOPi(2) s->floorInterval[i] = def->floorInterval[i];
    s->ceilFunc = def->ceilFunc;
    s->ceilMul = def->ceilMul;
    s->ceilOff = def->ceilOff;
    LOOPi(2) s->ceilInterval[i] = def->ceilInterval[i];
}

int Def_Get(int type, const char* id, void* out)
{
    int i;

    switch(type)
    {
    case DD_DEF_MOBJ:
        return Def_GetMobjNum(id);

    case DD_DEF_MOBJ_BY_NAME:
        return Def_GetMobjNumForName(id);

    case DD_DEF_STATE:
        return Def_GetStateNum(id);

    case DD_DEF_ACTION:
        return Def_GetActionNum(id);

    case DD_DEF_SPRITE:
        return Def_GetSpriteNum(id);

    case DD_DEF_SOUND:
        return Def_GetSoundNum(id);

    case DD_DEF_SOUND_BY_NAME:
        return defs.getSoundNumForName(id);

    case DD_DEF_SOUND_LUMPNAME:
        i = *((long*) id);
        if(i < 0 || i >= runtimeDefs.sounds.size())
            return false;
        strcpy((char*)out, runtimeDefs.sounds[i].lumpName);
        return true;

    case DD_DEF_MUSIC:
        return Def_GetMusicNum(id);

    case DD_DEF_MUSIC_CDTRACK:
        if(ded_music_t *music = Def_GetMusic(id))
        {
            return music->cdTrack;
        }
        return false;

    case DD_DEF_MAP_INFO: {
        ddmapinfo_t *mout;
        struct uri_s *mapUri = Uri_NewWithPath2(id, RC_NULL);
        ded_mapinfo_t *map   = Def_GetMapInfo(mapUri);

        Uri_Delete(mapUri);
        if(!map) return false;

        mout = (ddmapinfo_t *) out;
        mout->name       = map->name;
        mout->author     = map->author;
        mout->music      = Def_GetMusicNum(map->music);
        mout->flags      = map->flags;
        mout->ambient    = map->ambient;
        mout->gravity    = map->gravity;
        mout->parTime    = map->parTime;
        mout->fogStart   = map->fogStart;
        mout->fogEnd     = map->fogEnd;
        mout->fogDensity = map->fogDensity;
        memcpy(mout->fogColor, map->fogColor, sizeof(mout->fogColor));
        return true; }

    case DD_DEF_TEXT:
        if(id && id[0])
        {
            // Read backwards to allow patching.
            for(i = defs.text.size() - 1; i >= 0; i--)
            {
                if(stricmp(defs.text[i].id, id)) continue;
                if(out) *(char**) out = defs.text[i].text;
                return i;
            }
        }
        return -1;

    case DD_DEF_VALUE: {
        int idx = -1; // Not found.
        if(id && id[0])
        {
            // Read backwards to allow patching.
            for(idx = defs.values.size() - 1; idx >= 0; idx--)
            {
                if(!stricmp(defs.values[idx].id, id))
                    break;
            }
        }
        if(out) *(char**) out = (idx >= 0? defs.values[idx].text : 0);
        return idx; }

    case DD_DEF_VALUE_BY_INDEX: {
        int idx = *((long*) id);
        if(idx >= 0 && idx < defs.values.size())
        {
            if(out) *(char**) out = defs.values[idx].text;
            return true;
        }
        if(out) *(char**) out = 0;
        return false; }

    case DD_DEF_FINALE: { // Find InFine script by ID.
        finalescript_t* fin = (finalescript_t*) out;
        for(i = defs.finales.size() - 1; i >= 0; i--)
        {
            if(stricmp(defs.finales[i].id, id)) continue;

            if(fin)
            {
                fin->before = (uri_s *)defs.finales[i].before;
                fin->after  = (uri_s *)defs.finales[i].after;
                fin->script = defs.finales[i].script;
            }
            return true;
        }
        return false; }

    case DD_DEF_FINALE_BEFORE: {
        finalescript_t *fin = (finalescript_t *) out;
        de::Uri *uri = new de::Uri(id, RC_NULL);
        for(i = defs.finales.size() - 1; i >= 0; i--)
        {
            if(!defs.finales[i].before || *defs.finales[i].before != *uri) continue;

            if(fin)
            {
                fin->before = (uri_s *)defs.finales[i].before;
                fin->after  = (uri_s *)defs.finales[i].after;
                fin->script = defs.finales[i].script;
            }
            delete uri;
            return true;
        }
        delete uri;
        return false; }

    case DD_DEF_FINALE_AFTER: {
        finalescript_t *fin = (finalescript_t *) out;
        de::Uri *uri = new de::Uri(id, RC_NULL);
        for(i = defs.finales.size() - 1; i >= 0; i--)
        {
            if(!defs.finales[i].after || *defs.finales[i].after != *uri) continue;

            if(fin)
            {
                fin->before = (uri_s *)defs.finales[i].before;
                fin->after  = (uri_s *)defs.finales[i].after;
                fin->script = defs.finales[i].script;
            }
            delete uri;
            return true;
        }
        delete uri;
        return false; }

    case DD_DEF_LINE_TYPE: {
        int typeId = strtol(id, (char **)NULL, 10);
        for(i = defs.lineTypes.size() - 1; i >= 0; i--)
        {
            if(defs.lineTypes[i].id != typeId) continue;
            if(out) Def_CopyLineType((linetype_t*)out, &defs.lineTypes[i]);
            return true;
        }
        return false;
      }
    case DD_DEF_SECTOR_TYPE: {
        int typeId = strtol(id, (char **)NULL, 10);
        for(i = defs.sectorTypes.size() - 1; i >= 0; i--)
        {
            if(defs.sectorTypes[i].id != typeId) continue;
            if(out) Def_CopySectorType((sectortype_t*)out, &defs.sectorTypes[i]);
            return true;
        }
        return false;
      }
    default:
        return false;
    }
}

int Def_Set(int type, int index, int value, const void* ptr)
{
    ded_music_t* musdef = 0;
    int i;

    LOG_AS("Def_Set");

    switch(type)
    {
    case DD_DEF_TEXT:
        if(index < 0 || index >= defs.text.size())
        {
            DENG_ASSERT(!"Def_Set: Text index is invalid");
            return false;
        }
        defs.text[index].text = (char*) M_Realloc(defs.text[index].text, strlen((char*)ptr) + 1);
        strcpy(defs.text[index].text, (char const*) ptr);
        break;

    case DD_DEF_STATE: {
        ded_state_t* stateDef;
        if(index < 0 || index >= defs.states.size())
        {
            DENG_ASSERT(!"Def_Set: State index is invalid");
            return false;
        }
        stateDef = &defs.states[index];
        switch(value)
        {
        case DD_SPRITE: {
            int sprite = *(int*) ptr;

            if(sprite < 0 || sprite >= defs.sprites.size())
            {
                LOGDEV_RES_WARNING("Unknown sprite index %i") << sprite;
                break;
            }

            strcpy((char*) stateDef->sprite.id, defs.sprites[value].id);
            break; }

        case DD_FRAME:
            stateDef->frame = *(int*)ptr;
            break;

        default: break;
        }
        break; }

    case DD_DEF_SOUND:
        if(index < 0 || index >= runtimeDefs.sounds.size())
        {
            DENG_ASSERT(!"Sound index is invalid");
            return false;
        }

        switch(value)
        {
        case DD_LUMP:
            S_StopSound(index, 0);
            strcpy(runtimeDefs.sounds[index].lumpName, (char const*) ptr);
            if(strlen(runtimeDefs.sounds[index].lumpName))
            {
                runtimeDefs.sounds[index].lumpNum = App_FileSystem().lumpNumForName(runtimeDefs.sounds[index].lumpName);
                if(runtimeDefs.sounds[index].lumpNum < 0)
                {
                    LOG_RES_WARNING("Unknown sound lump name \"%s\"; sound #%i will be inaudible")
                            << runtimeDefs.sounds[index].lumpName << index;
                }
            }
            else
            {
                runtimeDefs.sounds[index].lumpNum = 0;
            }
            break;

        default:
            break;
        }
        break;

    case DD_DEF_MUSIC:
        if(index == DD_NEW)
        {
            // We should create a new music definition.
            i = DED_AddMusic(&defs, "");    // No ID is known at this stage.
            musdef = &defs.music[i];
        }
        else if(index >= 0 && index < defs.music.size())
        {
            musdef = &defs.music[index];
        }
        else
        {
            DENG_ASSERT(!"Def_Set: Music index is invalid");
            return false;
        }

        // Which key to set?
        switch(value)
        {
        case DD_ID:
            if(ptr)
                strcpy(musdef->id, (char const*) ptr);
            break;

        case DD_LUMP:
            if(ptr)
                strcpy(musdef->lumpName, (char const*) ptr);
            break;

        case DD_CD_TRACK:
            musdef->cdTrack = *(int *) ptr;
            break;

        default:
            break;
        }

        // If the def was just created, return its index.
        if(index == DD_NEW)
            return defs.music.indexOf(musdef);
        break;

    default:
        return false;
    }
    return true;
}

StringArray* Def_ListMobjTypeIDs(void)
{
    StringArray* array = StringArray_New();
    int i;
    for(i = 0; i < defs.mobjs.size(); ++i)
    {
        StringArray_Append(array, defs.mobjs[i].id);
    }
    return array;
}

StringArray* Def_ListStateIDs(void)
{
    StringArray* array = StringArray_New();
    int i;
    for(i = 0; i < defs.states.size(); ++i)
    {
        StringArray_Append(array, defs.states[i].id);
    }
    return array;
}

bool Def_IsAllowedDecoration(ded_decor_t const *def, /*bool hasExternal,*/ bool isCustom)
{
    //if(hasExternal) return (def->flags & DCRF_EXTERNAL) != 0;
    if(!isCustom)   return (def->flags & DCRF_NO_IWAD) == 0;
    return (def->flags & DCRF_PWAD) != 0;
}

bool Def_IsAllowedReflection(ded_reflection_t const *def, /*bool hasExternal,*/ bool isCustom)
{
    //if(hasExternal) return (def->flags & REFF_EXTERNAL) != 0;
    if(!isCustom)   return (def->flags & REFF_NO_IWAD) == 0;
    return (def->flags & REFF_PWAD) != 0;
}

bool Def_IsAllowedDetailTex(ded_detailtexture_t const *def, /*bool hasExternal,*/ bool isCustom)
{
    //if(hasExternal) return (def->flags & DTLF_EXTERNAL) != 0;
    if(!isCustom)   return (def->flags & DTLF_NO_IWAD) == 0;
    return (def->flags & DTLF_PWAD) != 0;
}

/**
 * Prints a list of all the registered mobjs to the console.
 * @todo Does this belong here?
 */
D_CMD(ListMobjs)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    if(defs.mobjs.size() <= 0)
    {
        LOG_RES_MSG("No mobjtypes defined/loaded");
        return true;
    }

    LOG_RES_MSG(_E(b) "Registered Mobjs (ID | Name):");
    for(int i = 0; i < defs.mobjs.size(); ++i)
    {
        if(defs.mobjs[i].name[0])
            LOG_RES_MSG(" %s | %s") << defs.mobjs[i].id << defs.mobjs[i].name;
        else
            LOG_RES_MSG(" %s | " _E(l) "(Unnamed)") << defs.mobjs[i].id;
    }

    return true;
}

DENG_DECLARE_API(Def) =
{
    { DE_API_DEFINITIONS },

    Def_Get,
    Def_Set,
    Def_EvalFlags
};
