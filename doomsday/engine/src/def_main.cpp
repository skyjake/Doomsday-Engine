/**
 * @file def_main.cpp
 *
 * Definitions Subsystem.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <string.h>
#include <ctype.h>

#include <de/NativePath>

#include "de_base.h"
#include "de_system.h"
#include "de_platform.h"
#include "de_console.h"
#include "de_audio.h"
#include "de_misc.h"
#include "de_graphics.h"
#include "de_ui.h"
#include "de_filesys.h"
#include "de_resource.h"

// XGClass.h is actually a part of the engine.
#include "../../../plugins/common/include/xgclass.h"

#define LOOPi(n)    for(i = 0; i < (n); ++i)
#define LOOPk(n)    for(k = 0; k < (n); ++k)

typedef struct {
    char* name; // Name of the routine.
    void (*func)(); // Pointer to the function.
} actionlink_t;

void Def_ReadProcessDED(const char* filename);

ded_t defs; // The main definitions database.
boolean firstDED;

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

static boolean defsInited = false;
static mobjinfo_t* gettingFor;

xgclass_t nullXgClassLinks; // Used when none defined.
xgclass_t* xgClassLinks;

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
    return 1;
}

/**
 * Initializes the definition databases.
 */
void Def_Init(void)
{
    sprNames = 0;
    DED_ZCount(&countSprNames);

    mobjInfo = 0;
    DED_ZCount(&countMobjInfo);

    states = 0;
    DED_ZCount(&countStates);

    sounds = 0;
    DED_ZCount(&countSounds);

    texts = 0;
    DED_ZCount(&countTexts);

    stateOwners = 0;
    DED_ZCount(&countStateOwners);

    statePtcGens = 0;
    DED_ZCount(&countStatePtcGens);

    stateLights = 0;
    DED_ZCount(&countStateLights);

    DED_Init(&defs);
}

void Def_Destroy(void)
{
    int i;

    // To make sure...
    DED_Clear(&defs);
    DED_Init(&defs);

    // Destroy the databases.
    DED_DelArray((void**) &sprNames, &countSprNames);
    DED_DelArray((void**) &states, &countStates);
    DED_DelArray((void**) &mobjInfo, &countMobjInfo);

    for(i = 0; i < countSounds.num; ++i)
    {
        Str_Free(&sounds[i].external);
    }
    DED_DelArray((void**) &sounds, &countSounds);

    DED_DelArray((void**) &texts, &countTexts);
    DED_DelArray((void**) &stateOwners, &countStateOwners);
    DED_DelArray((void**) &statePtcGens, &countStatePtcGens);
    DED_DelArray((void**) &stateLights, &countStateLights);

    defsInited = false;
}

/**
 * @return              Number of the given sprite else,
 *                      @c -1, if it doesn't exist.
 */
int Def_GetSpriteNum(const char* name)
{
    int                 i;

    if(!name || !name[0])
        return -1;

    for(i = 0; i < countSprNames.num; ++i)
        if(!stricmp(sprNames[i].name, name))
            return i;

    return -1;
}

int Def_GetMobjNum(const char* id)
{
    int i;

    if(!id || !id[0])
        return -1;

    for(i = 0; i < defs.count.mobjs.num; ++i)
        if(!stricmp(defs.mobjs[i].id, id))
            return i;

    return -1;
}

int Def_GetMobjNumForName(const char* name)
{
    int                 i;

    if(!name || !name[0])
        return -1;

    for(i = defs.count.mobjs.num -1; i >= 0; --i)
        if(!stricmp(defs.mobjs[i].name, name))
            return i;

    return -1;
}

const char* Def_GetMobjName(int num)
{
    if(num < 0) return "(<0)";
    if(num >= defs.count.mobjs.num) return "(>mobjtypes)";
    return defs.mobjs[num].id;
}

int Def_GetStateNum(const char* id)
{
    int idx = -1;
    if(id && id[0] && defs.count.states.num)
    {
        int i = 0;
        do
        {
            if(!stricmp(defs.states[i].id, id))
                idx = i;
        } while(idx == -1 && ++i < defs.count.states.num);
    }
    return idx;
}

int Def_GetModelNum(const char* id)
{
    int idx = -1;
    if(id && id[0] && defs.count.models.num)
    {
        int i = 0;
        do
        {
            if(!stricmp(defs.models[i].id, id))
                idx = i;
        } while(idx == -1 && ++i < defs.count.models.num);
    }
    return idx;
}

int Def_GetSoundNum(const char* id)
{
    int idx = -1;
    if(id && id[0] && defs.count.sounds.num)
    {
        int i = 0;
        do
        {
            if(!stricmp(defs.sounds[i].id, id))
                idx = i;
        } while(idx == -1 && ++i < defs.count.sounds.num);
    }
    return idx;
}

/**
 * Looks up a sound using the Name key. If the name is not found, returns
 * the NULL sound index (zero).
 */
int Def_GetSoundNumForName(const char* name)
{
    int                 i;

    if(!name || !name[0])
        return -1;

    for(i = 0; i < defs.count.sounds.num; ++i)
        if(!stricmp(defs.sounds[i].name, name))
            return i;

    return 0;
}

int Def_GetMusicNum(const char* id)
{
    int idx = -1;
    if(id && id[0] && defs.count.music.num)
    {
        int i = 0;
        do
        {
            if(!stricmp(defs.music[i].id, id))
                idx = i;
        } while(idx == -1 && ++i < defs.count.music.num);
    }
    return idx;
}

acfnptr_t Def_GetActionPtr(const char* name)
{
    actionlink_t* linkIt;

    if(!name || !name[0]) return 0;
    if(!DD_GameLoaded()) return 0;

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
    if(name && name[0] && DD_GameLoaded())
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
    if(!id || !id[0]) return NULL;

    // Read backwards to allow patching.
    for(int i = defs.count.values.num - 1; i >= 0; i--)
    {
        if(!stricmp(defs.values[i].id, id))
            return defs.values + i;
    }
    return 0;
}

ded_value_t* Def_GetValueByUri(Uri const* _uri)
{
    if(!_uri) return 0;
    de::Uri const& uri = reinterpret_cast<de::Uri const&>(*_uri);

    if(uri.scheme().compareWithoutCase("Values")) return 0;
    return Def_GetValueById(uri.pathCStr());
}

ded_mapinfo_t* Def_GetMapInfo(Uri const* _uri)
{
    if(!_uri) return 0;
    de::Uri const& uri = reinterpret_cast<de::Uri const&>(*_uri);

    for(int i = defs.count.mapInfo.num - 1; i >= 0; i--)
    {
        if(defs.mapInfo[i].uri && uri == reinterpret_cast<de::Uri const&>(*defs.mapInfo[i].uri))
            return defs.mapInfo + i;
    }
    return 0;
}

ded_sky_t* Def_GetSky(char const* id)
{
    if(!id || !id[0]) return NULL;

    for(int i = defs.count.skies.num - 1; i >= 0; i--)
    {
        if(!stricmp(defs.skies[i].id, id))
            return defs.skies + i;
    }
    return NULL;
}

static ded_material_t* findMaterialDef(de::Uri const& uri)
{
    for(int i = defs.count.materials.num - 1; i >= 0; i--)
    {
        ded_material_t* def = &defs.materials[i];
        if(!def->uri || uri != reinterpret_cast<de::Uri&>(*def->uri)) continue;
        return def;
    }
    return NULL;
}

ded_material_t* Def_GetMaterial(char const* uriCString)
{
    ded_material_t* def = NULL;
    if(uriCString && uriCString[0])
    {
        de::Uri uri = de::Uri(uriCString, FC_NONE);

        if(uri.scheme().isEmpty())
        {
            // Caller doesn't care which namespace - use a priority search order.
            de::Uri temp = de::Uri(uri);

            temp.setScheme(MN_SPRITES_NAME);
            def = findMaterialDef(temp);
            if(!def)
            {
                temp.setScheme(MN_TEXTURES_NAME);
                def = findMaterialDef(temp);
            }
            if(!def)
            {
                temp.setScheme(MN_FLATS_NAME);
                def = findMaterialDef(temp);
            }
        }

        if(!def)
        {
            def = findMaterialDef(uri);
        }
    }
    return def;
}

static ded_compositefont_t* findCompositeFontDef(de::Uri const& uri)
{
    for(int i = defs.count.compositeFonts.num - 1; i >= 0; i--)
    {
        ded_compositefont_t* def = &defs.compositeFonts[i];
        if(!def->uri || uri != reinterpret_cast<de::Uri&>(*def->uri)) continue;
        return def;
    }
    return NULL;
}

ded_compositefont_t* Def_GetCompositeFont(char const* uriCString)
{
    ded_compositefont_t* def = NULL;
    if(uriCString && uriCString[0])
    {
        de::Uri uri = de::Uri(uriCString, FC_NONE);

        if(uri.scheme().isEmpty())
        {
            // Caller doesn't care which namespace - use a priority search order.
            de::Uri temp = de::Uri(uri);

            temp.setScheme(FN_GAME_NAME);
            def = findCompositeFontDef(temp);
            if(!def)
            {
                temp.setScheme(FN_SYSTEM_NAME);
                def = findCompositeFontDef(temp);
            }
        }

        if(!def)
        {
            def = findCompositeFontDef(uri);
        }
    }
    return def;
}

ded_decor_t* Def_GetDecoration(materialid_t matId, boolean hasExternal, boolean isCustom)
{
    ded_decor_t* def;
    int i;
    for(i = defs.count.decorations.num - 1, def = defs.decorations + i; i >= 0; i--, def--)
    {
        materialid_t defMatId;

        if(!def->material) continue;

        // Is this suitable?
        defMatId = Materials_ResolveUri2(def->material, true/*quiet please*/);
        if(matId == defMatId && R_IsAllowedDecoration(def, hasExternal, isCustom))
            return def;
    }
    return NULL;
}

ded_reflection_t* Def_GetReflection(materialid_t matId, boolean hasExternal, boolean isCustom)
{
    ded_reflection_t* def;
    int i;
    for(i = defs.count.reflections.num - 1, def = defs.reflections + i; i >= 0; i--, def--)
    {
        materialid_t defMatId;

        if(!def->material) continue;

        // Is this suitable?
        defMatId = Materials_ResolveUri2(def->material, true/*quiet please*/);
        if(matId == defMatId && R_IsAllowedReflection(def, hasExternal, isCustom))
            return def;
    }
    return NULL;
}

ded_detailtexture_t* Def_GetDetailTex(materialid_t matId, boolean hasExternal, boolean isCustom)
{
    ded_detailtexture_t* def;
    int i;
    for(i = defs.count.details.num - 1, def = defs.details + i; i >= 0; i--, def--)
    {
        if(def->material1)
        {
            materialid_t defMatId = Materials_ResolveUri2(def->material1, true/*quiet please*/);
            // Is this suitable?
            if(matId == defMatId && R_IsAllowedDetailTex(def, hasExternal, isCustom))
                return def;
        }

        if(def->material2)
        {
            materialid_t defMatId = Materials_ResolveUri2(def->material2, true/*quiet please*/);
            // Is this suitable?
            if(matId == defMatId && R_IsAllowedDetailTex(def, hasExternal, isCustom))
                return def;
        }
    }
    return NULL;
}

ded_ptcgen_t* Def_GetGenerator(materialid_t matId, boolean hasExternal, boolean isCustom)
{
    DENG_UNUSED(hasExternal); DENG_UNUSED(isCustom);

    ded_ptcgen_t* def = defs.ptcGens;
    for(int i = 0; i < defs.count.ptcGens.num; ++i, def++)
    {
        materialid_t defMatId;

        if(!def->material) continue;
        defMatId = Materials_ResolveUri2(def->material, true/*quiet please*/);
        if(defMatId == NOMATERIALID) continue;

        // Is this suitable?
        if(def->flags & PGF_GROUP)
        {
            /**
             * Generator triggered by all materials in the (animation) group.
             * A search is necessary only if we know both the used material and
             * the specified material in this definition are in *a* group.
             */
            material_t* mat = Materials_ToMaterial(matId);
            material_t* defMat = Materials_ToMaterial(defMatId);
            if(Material_IsGroupAnimated(defMat) && Material_IsGroupAnimated(mat))
            {
                int g, numGroups = Materials_AnimGroupCount();

                for(g = 0; g < numGroups; ++g)
                {
                    if(Materials_IsPrecacheAnimGroup(g))
                        continue; // Precache groups don't apply.

                    if(Materials_IsMaterialInAnimGroup(defMat, g) &&
                       Materials_IsMaterialInAnimGroup(mat, g))
                    {
                        // Both are in this group! This def will do.
                        return def;
                    }
                }
            }
        }

        if(matId == defMatId)
            return def;
    }
    return NULL; // Not found.
}

ded_ptcgen_t* Def_GetDamageGenerator(int mobjType)
{
    int                 i;
    ded_ptcgen_t*       def;

    // Search for a suitable definition.
    for(i = 0, def = defs.ptcGens; i < defs.count.ptcGens.num; ++i, def++)
    {
        // It must be for this type of mobj.
        if(def->damageNum == mobjType)
            return def;
    }

    return NULL;
}

ded_flag_t* Def_GetFlag(const char* flag)
{
    int i;

    if(!flag || !flag[0])
    {
        DEBUG_Message(("Attempted Def_GetFlagValue with %s flag argument.\n",
                       flag? "zero-length" : "<null>"));
        return 0;
    }

    for(i = defs.count.flags.num - 1; i >= 0; i--)
    {
        if(!stricmp(defs.flags[i].id, flag))
            return defs.flags + i;
    }

    return 0;
}

/**
 * Attempts to retrieve a flag by its prefix and value.
 * Returns a ptr to the text string of the first flag it
 * finds that matches the criteria, else NULL.
 */
const char* Def_GetFlagTextByPrefixVal(const char* prefix, int val)
{
    int i;
    for(i = defs.count.flags.num - 1; i >= 0; i--)
    {
        if(strnicmp(defs.flags[i].id, prefix, strlen(prefix)) == 0 && defs.flags[i].value == val)
            return defs.flags[i].text;
    }
    return NULL;
}

int Def_EvalFlags(char* ptr)
{
    ded_flag_t* flag;
    int value = 0, len;
    char buf[64];

    while(*ptr)
    {
        ptr = M_SkipWhite(ptr);
        len = M_FindWhite(ptr) - ptr;
        strncpy(buf, ptr, len);
        buf[len] = 0;

        flag = Def_GetFlag(buf);
        if(flag)
        {
            value |= flag->value;
        }
        else
        {
            Con_Message("Warning: Def_EvalFlags: Undefined flag '%s'.\n", buf);
        }
        ptr += len;
    }
    return value;
}

int Def_GetTextNumForName(const char* name)
{
    int idx = -1;
    if(name && name[0] && defs.count.text.num)
    {
        int i = 0;
        do
        {
            if(!stricmp(defs.text[i].id, name))
                idx = i;
        } while(idx == -1 && ++i < defs.count.text.num);
    }
    return idx;
}

/**
 * The following escape sequences are un-escaped:
 * <pre>
 *     \n   Newline
 *     \r   Carriage return
 *     \t   Tab
 *     \_   Space
 *     \s   Space
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

void Def_ReadProcessDED(char const* fileName)
{
    LOG_AS("Def_ReadProcessDED");

    if(!fileName || !fileName[0]) return;

    if(!App_FileSystem()->accessFile(fileName))
    {
        LOG_WARNING("\"%s\" not found!") << de::NativePath(fileName).pretty();
        return;
    }

    // We use the File Ids to prevent loading the same files multiple times.
    if(!App_FileSystem()->checkFileId(fileName))
    {
        // Already handled.
        LOG_DEBUG("\"%s\" has already been read.") << de::NativePath(fileName).pretty();
        return;
    }

    if(!DED_Read(&defs, fileName))
    {
        Con_Error("Def_ReadProcessDED: %s\n", dedReadError);
    }
}

/**
 * Prints a count with a 2-space indentation.
 */
void Def_CountMsg(int count, const char* label)
{
    if(!verbose && !count)
        return; // Don't print zeros if not verbose.

    Con_Message("%5i %s\n", count, label);
}

/**
 * Read all DD_DEFNS lumps in the primary lump index.
 */
void Def_ReadLumpDefs(void)
{
    int numProcessedLumps = 0;
    DENG2_FOR_EACH_CONST(de::LumpIndex::Lumps, i, App_FileSystem()->nameIndex().lumps())
    {
        de::File1 const& lump = **i;
        if(!lump.name().beginsWith("DD_DEFNS", Qt::CaseInsensitive)) continue;

        numProcessedLumps += 1;

        if(!DED_ReadLump(&defs, lump.info().lumpIdx))
        {
            QByteArray path = de::NativePath(lump.container().composePath()).pretty().toUtf8();
            Con_Error("DD_ReadLumpDefs: Parse error reading \"%s:DD_DEFNS\".\n", path.constData());
        }
    }

    if(verbose && numProcessedLumps > 0)
    {
        Con_Message("ReadLumpDefs: %i definition lump%s read.\n", numProcessedLumps, numProcessedLumps != 1 ? "s" : "");
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
        stateOwners[num] = gettingFor;
        // Scan forward at most 'count' states, or until we hit a state with
        // an owner, or the NULL state.
        for(st = states[num].nextState; st > 0 && count-- && !stateOwners[st];
            st = states[st].nextState)
        {
            stateOwners[st] = gettingFor;
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

static __inline void readDefinitionFile(const char* fileName)
{
    if(!fileName || !fileName[0])
        return;
    Def_ReadProcessDED(fileName);
}

static void readAllDefinitions(void)
{
    uint startTime = Timer_RealMilliseconds();
    ddstring_t buf;
    int p;

    // Start with engine's own top-level definition file, it is always read first.
    de::Uri searchPath = de::Uri("doomsday.ded", FC_DEFINITION);
    AutoStr* foundPath = AutoStr_NewStd();
    if(F_Find2(FC_DEFINITION, reinterpret_cast<uri_s*>(&searchPath), foundPath))
    {
        VERBOSE2( Con_Message("  Processing '%s'...\n", F_PrettyPath(Str_Text(foundPath))) )
        readDefinitionFile(Str_Text(foundPath));
    }
    else
    {
        Con_Error("readAllDefinitions: Error, failed to locate main engine definition file \"doomsday.ded\".");
    }

    // Now any definition files required by the game on load.
    if(DD_GameLoaded())
    {
        de::Game::Resources const& gameResources = reinterpret_cast<de::Game*>(App_CurrentGame())->resources();
        int packageIdx = 0;
        for(de::Game::Resources::const_iterator i = gameResources.find(FC_DEFINITION);
            i != gameResources.end() && i.key() == FC_DEFINITION; ++i, ++packageIdx)
        {
            de::ResourceRecord& record = **i;
            /// Try to locate this resource now.
            QString const& path = record.resolvedPath(true/*try to locate*/);

            if(path.isEmpty())
            {
                QByteArray names = record.names().join(";").toUtf8();
                Con_Error("readAllDefinitions: Error, failed to locate required game definition \"%s\".", names.constData());
            }

            QByteArray pathUtf8 = path.toUtf8();
            LOG_VERBOSE("  Processing '%s'...") << F_PrettyPath(pathUtf8.constData());

            readDefinitionFile(pathUtf8.constData());
        }
    }

    // Next up are definition files in the Games' /auto directory.
    if(!CommandLine_Exists("-noauto") && DD_GameLoaded())
    {
        de::FS1::PathList found;
        if(App_FileSystem()->findAllPaths(de::Uri("$(App.DefsPath)/$(GamePlugin.Name)/auto/*.ded", FC_NONE).resolved(), 0, found))
        {
            DENG2_FOR_EACH_CONST(de::FS1::PathList, i, found)
            {
                // Ignore directories.
                if(i->attrib & A_SUBDIR) continue;

                QByteArray foundPathUtf8 = i->path.toUtf8();
                readDefinitionFile(foundPathUtf8.constData());
            }
        }
    }

    // Any definition files on the command line?
    Str_Init(&buf);
    for(p = 0; p < CommandLine_Count(); ++p)
    {
        const char* arg = CommandLine_At(p);
        if(!CommandLine_IsMatchingAlias("-def", arg) &&
           !CommandLine_IsMatchingAlias("-defs", arg)) continue;

        while(++p != CommandLine_Count() && !CommandLine_IsOption(p))
        {
            const char* searchPath = CommandLine_PathAt(p);

            Con_Message("  Processing '%s'...\n", F_PrettyPath(searchPath));

            Str_Clear(&buf); Str_Set(&buf, searchPath);
            F_FixSlashes(&buf, &buf);
            F_ExpandBasePath(&buf, &buf);
            // We must have an absolute path. If we still do not have one then
            // prepend the current working directory if necessary.
            F_PrependWorkPath(&buf, &buf);

            readDefinitionFile(Str_Text(&buf));
        }
        p--; /* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }
    Str_Free(&buf);

    // Read DD_DEFNS definition lumps.
    Def_ReadLumpDefs();

    VERBOSE2( Con_Message("  Done in %.2f seconds.\n", (Timer_RealMilliseconds() - startTime) / 1000.0f) );
}

void Def_GenerateGroupsFromAnims(void)
{
    int groupCount = R_AnimGroupCount(), i;
    if(!groupCount) return;

    // Group ids are 1-based.
    for(i = 1; i < groupCount+1; ++i)
    {
        const animgroup_t* anim = R_ToAnimGroup(i);
        ded_group_member_t* gmbr;
        ded_group_t* grp;
        int idx, j;

        idx = DED_AddGroup(&defs);
        grp = &defs.groups[idx];
        grp->autoGenerated = true;
        grp->flags = anim->flags;

        for(j = 0; j < anim->count; ++j)
        {
            const animframe_t* frame = &anim->frames[j];

            idx = DED_AddGroupMember(grp);
            gmbr = &grp->members[idx];
            gmbr->tics = frame->tics;
            gmbr->randomTics = frame->randomTics;
            gmbr->material = Textures_ComposeUri(frame->texture);
            Uri_SetScheme(gmbr->material, Str_Text(DD_MaterialNamespaceNameForTextureNamespace(Textures_Namespace(frame->texture))));
        }
    }
}

static int generateMaterialDefForPatchCompositeTexture(textureid_t texId, void* parameters)
{
    DENG_UNUSED(parameters);

    Uri* texUri = Textures_ComposeUri(texId);
    ded_material_layer_stage_t* st;
    ded_material_t* mat;
    int stage, idx;
    Texture* tex;

    idx = DED_AddMaterial(&defs, NULL);
    mat = &defs.materials[idx];
    mat->autoGenerated = true;

    mat->uri = Uri_Dup(texUri);
    Uri_SetScheme(mat->uri, MN_TEXTURES_NAME);

    tex = Textures_ToTexture(texId);
    if(tex)
    {
        patchcompositetex_t* pcTex = (patchcompositetex_t*)Texture_UserDataPointer(tex);
        assert(pcTex);
        mat->width  = Texture_Width(tex);
        mat->height = Texture_Height(tex);
        mat->flags = ((pcTex->flags & TXDF_NODRAW)? MATF_NO_DRAW : 0);
    }
#if _DEBUG
    else
    {
        AutoStr* path = Uri_ToString(texUri);
        Con_Message("Warning:generateMaterialDefForPatchCompositeTexture: Texture \"%s\" has not yet been defined, resultant Material will inherit dimensions.\n", Str_Text(path));
    }
#endif

    stage = DED_AddMaterialLayerStage(&mat->layers[0]);
    st = &mat->layers[0].stages[stage];
    st->texture = texUri;
    return 0; // Continue iteration.
}

static int generateMaterialDefForFlatTexture(textureid_t texId, void* parameters)
{
    DENG_UNUSED(parameters);

    Uri* texUri = Textures_ComposeUri(texId);
    ded_material_layer_stage_t* st;
    ded_material_t* mat;
    int stage, idx;
    Texture* tex;

    idx = DED_AddMaterial(&defs, NULL);
    mat = &defs.materials[idx];
    mat->autoGenerated = true;

    mat->uri = Uri_Dup(texUri);
    Uri_SetScheme(mat->uri, MN_FLATS_NAME);

    stage = DED_AddMaterialLayerStage(&mat->layers[0]);
    st = &mat->layers[0].stages[stage];
    st->texture = texUri;

    tex = Textures_ToTexture(texId);
    if(tex)
    {
        mat->width  = Texture_Width(tex);
        mat->height = Texture_Height(tex);
    }
#if _DEBUG
    else
    {
        AutoStr* path = Uri_ToString(texUri);
        Con_Message("Warning:generateMaterialDefForFlatTexture: Texture \"%s\" has not yet been defined, resultant Material will inherit dimensions.\n", Str_Text(path));
    }
#endif

    return 0; // Continue iteration.
}

static int generateMaterialDefForSpriteTexture(textureid_t texId, void* parameters)
{
    DENG_UNUSED(parameters);

    Uri* texUri = Textures_ComposeUri(texId);
    ded_material_layer_stage_t* st;
    ded_material_t* mat;
    int stage, idx;
    Texture* tex;

    idx = DED_AddMaterial(&defs, NULL);
    mat = &defs.materials[idx];
    mat->autoGenerated = true;

    mat->uri = Uri_Dup(texUri);
    Uri_SetScheme(mat->uri, MN_SPRITES_NAME);

    stage = DED_AddMaterialLayerStage(&mat->layers[0]);
    st = &mat->layers[0].stages[stage];
    st->texture = texUri;

    tex = Textures_ToTexture(texId);
    if(tex)
    {
        mat->width  = Texture_Width(tex);
        mat->height = Texture_Height(tex);
    }
#if _DEBUG
    else
    {
        AutoStr* path = Uri_ToString(texUri);
        Con_Message("Warning:generateMaterialDefForSpriteTexture: Texture \"%s\" has not yet been defined, resultant Material will inherit dimensions.\n", Str_Text(path));
    }
#endif

    return 0; // Continue iteration.
}

void Def_GenerateAutoMaterials(void)
{
    Textures_IterateDeclared(TN_TEXTURES, generateMaterialDefForPatchCompositeTexture);
    Textures_IterateDeclared(TN_FLATS,    generateMaterialDefForFlatTexture);
    Textures_IterateDeclared(TN_SPRITES,  generateMaterialDefForSpriteTexture);
}

void Def_Read(void)
{
    int i, k;

    if(defsInited)
    {
        // We've already initialized the definitions once.
        // Get rid of everything.
        de::FileNamespace* fnamespace = F_FileNamespaceByName(F_FileClassByName("FC_MODEL").defaultNamespace());
        DENG_ASSERT(fnamespace);
        fnamespace->reset();

        Materials_ClearDefinitionLinks();
        Fonts_ClearDefinitionLinks();

        Def_Destroy();
    }

    firstDED = true;

    // Now we can clear all existing definitions and re-init.
    DED_Clear(&defs);
    DED_Init(&defs);

    // Generate definitions.
    Def_GenerateGroupsFromAnims();
    Def_GenerateAutoMaterials();

    // Read all definitions files and lumps.
    Con_Message("Parsing definition files%s\n", verbose >= 1? ":" : "...");
    readAllDefinitions();

    // Any definition hooks?
    DD_CallHooks(HOOK_DEFS, 0, &defs);

    // Composite fonts.
    for(i = 0; i < defs.count.compositeFonts.num; ++i)
    {
        R_CreateFontFromDef(defs.compositeFonts + i);
    }

    // Sprite names.
    DED_NewEntries((void**) &sprNames, &countSprNames, sizeof(*sprNames), defs.count.sprites.num);
    for(i = 0; i < countSprNames.num; ++i)
        strcpy(sprNames[i].name, defs.sprites[i].id);

    // States.
    DED_NewEntries((void**) &states, &countStates, sizeof(*states), defs.count.states.num);

    for(i = 0; i < countStates.num; ++i)
    {
        ded_state_t* dstNew, *dst = &defs.states[i];
        // Make sure duplicate IDs overwrite the earliest.
        int stateNum = Def_GetStateNum(dst->id);
        state_t* st;

        if(stateNum == -1) continue;

        dstNew = defs.states + stateNum;
        st = states + stateNum;
        st->sprite = Def_GetSpriteNum(dst->sprite.id);
        st->flags = dst->flags;
        st->frame = dst->frame;

        st->tics = dst->tics;
        st->action = Def_GetActionPtr(dst->action);
        st->nextState = Def_GetStateNum(dst->nextState);
        for(k = 0; k < NUM_STATE_MISC; ++k)
            st->misc[k] = dst->misc[k];

        // Replace the older execute string.
        if(dst != dstNew)
        {
            if(dstNew->execute)
                M_Free(dstNew->execute);
            dstNew->execute = dst->execute;
            dst->execute = NULL;
        }
    }

    DED_NewEntries((void**) &stateOwners, &countStateOwners, sizeof(mobjinfo_t *), defs.count.states.num);

    // Mobj info.
    DED_NewEntries((void**) &mobjInfo, &countMobjInfo, sizeof(*mobjInfo), defs.count.mobjs.num);
    for(i = 0; i < countMobjInfo.num; ++i)
    {
        ded_mobj_t*         dmo = &defs.mobjs[i];
        // Make sure duplicate defs overwrite the earliest.
        mobjinfo_t*         mo = &mobjInfo[Def_GetMobjNum(dmo->id)];

        gettingFor = mo;
        mo->doomEdNum = dmo->doomEdNum;
        mo->spawnHealth = dmo->spawnHealth;
        mo->reactionTime = dmo->reactionTime;
        mo->painChance = dmo->painChance;
        mo->speed = dmo->speed;
        mo->radius = dmo->radius;
        mo->height = dmo->height;
        mo->mass = dmo->mass;
        mo->damage = dmo->damage;
        mo->flags = dmo->flags[0];
        mo->flags2 = dmo->flags[1];
        mo->flags3 = dmo->flags[2];
        for(k = 0; k < STATENAMES_COUNT; ++k)
        {
            mo->states[k] = Def_StateForMobj(dmo->states[k]);
        }
        mo->seeSound = Def_GetSoundNum(dmo->seeSound);
        mo->attackSound = Def_GetSoundNum(dmo->attackSound);
        mo->painSound = Def_GetSoundNum(dmo->painSound);
        mo->deathSound = Def_GetSoundNum(dmo->deathSound);
        mo->activeSound = Def_GetSoundNum(dmo->activeSound);
        for(k = 0; k < NUM_MOBJ_MISC; ++k)
            mo->misc[k] = dmo->misc[k];
    }

    // Materials.
    for(i = 0; i < defs.count.materials.num; ++i)
    {
        ded_material_t* def = &defs.materials[i];
        material_t* mat = Materials_ToMaterial(Materials_ResolveUri2(def->uri, true/*quiet please*/));
        if(!mat)
        {
            // A new Material.
            Materials_CreateFromDef(def);
            continue;
        }
        // Update existing.
        Materials_Rebuild(mat, def);
    }

    DED_NewEntries((void**) &stateLights, &countStateLights, sizeof(*stateLights), defs.count.states.num);

    // Dynamic lights. Update the sprite numbers.
    for(i = 0; i < defs.count.lights.num; ++i)
    {
        k = Def_GetStateNum(defs.lights[i].state);
        if(k < 0)
        {
            // It's probably a bias light definition, then?
            if(!defs.lights[i].uniqueMapID[0])
            {
                Con_Message("Warning: Def_Read: Undefined state '%s' in Light definition.\n", defs.lights[i].state);
            }
            continue;
        }
        stateLights[k] = &defs.lights[i];
    }

    // Sound effects.
    DED_NewEntries((void **) &sounds, &countSounds, sizeof(*sounds),
                   defs.count.sounds.num);
    for(i = 0; i < countSounds.num; ++i)
    {
        ded_sound_t* snd = defs.sounds + i;
        // Make sure duplicate defs overwrite the earliest.
        sfxinfo_t* si = sounds + Def_GetSoundNum(snd->id);

        strcpy(si->id, snd->id);
        strcpy(si->lumpName, snd->lumpName);
        si->lumpNum = (strlen(snd->lumpName) > 0? App_FileSystem()->lumpNumForName(snd->lumpName) : -1);
        strcpy(si->name, snd->name);
        k = Def_GetSoundNum(snd->link);
        si->link = (k >= 0 ? sounds + k : 0);
        si->linkPitch = snd->linkPitch;
        si->linkVolume = snd->linkVolume;
        si->priority = snd->priority;
        si->channels = snd->channels;
        si->flags = snd->flags;
        si->group = snd->group;
        Str_Init(&si->external);
        if(NULL != snd->ext)
            Str_Set(&si->external, Str_Text(Uri_Path(snd->ext)));
    }

    // Music.
    for(i = 0; i < defs.count.music.num; ++i)
    {
        ded_music_t* mus = defs.music + i;
        // Make sure duplicate defs overwrite the earliest.
        ded_music_t* earliest = defs.music + Def_GetMusicNum(mus->id);

        if(earliest == mus) continue;

        strcpy(earliest->lumpName, mus->lumpName);
        earliest->cdTrack = mus->cdTrack;

        if(mus->path)
        {
            if(earliest->path)
                Uri_Copy(earliest->path, mus->path);
            else
                earliest->path = Uri_Dup(mus->path);
        }
        else if(earliest->path)
        {
            Uri_Delete(earliest->path);
            earliest->path = NULL;
        }
    }

    // Text.
    DED_NewEntries((void **) &texts, &countTexts, sizeof(*texts),
                   defs.count.text.num);

    for(i = 0; i < countTexts.num; ++i)
        Def_InitTextDef(texts + i, defs.text[i].text);

    // Handle duplicate strings.
    for(i = 0; i < countTexts.num; ++i)
    {
        if(!texts[i].text)
            continue;

        for(k = i + 1; k < countTexts.num; ++k)
            if(!stricmp(defs.text[i].id, defs.text[k].id) && texts[k].text)
            {
                // Update the earlier string.
                texts[i].text = (char*) M_Realloc(texts[i].text, strlen(texts[k].text) + 1);
                strcpy(texts[i].text, texts[k].text);

                // Free the later string, it isn't used (>NUMTEXT).
                M_Free(texts[k].text);
                texts[k].text = 0;
            }
    }

    DED_NewEntries((void**) &statePtcGens, &countStatePtcGens, sizeof(*statePtcGens), defs.count.states.num);

    // Particle generators.
    for(i = 0; i < defs.count.ptcGens.num; ++i)
    {
        ded_ptcgen_t*       pg = &defs.ptcGens[i];
        int                 st = Def_GetStateNum(pg->state);

        if(!strcmp(pg->type, "*"))
            pg->typeNum = DED_PTCGEN_ANY_MOBJ_TYPE;
        else
            pg->typeNum = Def_GetMobjNum(pg->type);
        pg->type2Num = Def_GetMobjNum(pg->type2);
        pg->damageNum = Def_GetMobjNum(pg->damage);

        // Figure out embedded sound ID numbers.
        for(k = 0; k < pg->stageCount.num; ++k)
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

        // Link the definition to the state.
        if(pg->flags & PGF_STATE_CHAIN)
        {
            // Add to the chain.
            pg->stateNext = statePtcGens[st];
            statePtcGens[st] = pg;
        }
        else
        {
            // Make sure the previously built list is unlinked.
            while(statePtcGens[st])
            {
                ded_ptcgen_t*       temp = statePtcGens[st]->stateNext;

                statePtcGens[st]->stateNext = NULL;
                statePtcGens[st] = temp;
            }
            statePtcGens[st] = pg;
            pg->stateNext = NULL;
        }
    }

    // Map infos.
    for(i = 0; i < defs.count.mapInfo.num; ++i)
    {
        ded_mapinfo_t* mi = &defs.mapInfo[i];

        /**
         * Historically, the map info flags field was used for sky flags,
         * here we copy those flags to the embedded sky definition for
         * backward-compatibility.
         */
        if(mi->flags & MIF_DRAW_SPHERE)
            mi->sky.flags |= SIF_DRAW_SPHERE;
    }

    // Log a summary of the definition database.
    Con_Message("Definitions:\n");
    Def_CountMsg(defs.count.groups.num, "animation groups");
    Def_CountMsg(defs.count.compositeFonts.num, "composite fonts");
    Def_CountMsg(defs.count.details.num, "detail textures");
    Def_CountMsg(defs.count.finales.num, "finales");
    Def_CountMsg(defs.count.lights.num, "lights");
    Def_CountMsg(defs.count.lineTypes.num, "line types");
    Def_CountMsg(defs.count.mapInfo.num, "map infos");
    { int nonAutoGeneratedCount = 0;
    for(i = 0; i < defs.count.materials.num; ++i)
        if(!defs.materials[i].autoGenerated)
            ++nonAutoGeneratedCount;
    Def_CountMsg(nonAutoGeneratedCount, "materials");
    }
    Def_CountMsg(defs.count.models.num, "models");
    Def_CountMsg(defs.count.ptcGens.num, "particle generators");
    Def_CountMsg(defs.count.skies.num, "skies");
    Def_CountMsg(defs.count.sectorTypes.num, "sector types");
    Def_CountMsg(defs.count.music.num, "songs");
    Def_CountMsg(countSounds.num, "sound effects");
    Def_CountMsg(countSprNames.num, "sprite names");
    Def_CountMsg(countStates.num, "states");
    Def_CountMsg(defs.count.decorations.num, "surface decorations");
    Def_CountMsg(defs.count.reflections.num, "surface reflections");
    Def_CountMsg(countTexts.num, "text strings");
    Def_CountMsg(defs.count.textureEnv.num, "texture environments");
    Def_CountMsg(countMobjInfo.num, "things");

    defsInited = true;
}

static void initAnimGroup(ded_group_t* def)
{
    int i, groupNumber = -1;
    for(i = 0; i < def->count.num; ++i)
    {
        ded_group_member_t* gm = &def->members[i];
        material_t* mat;

        if(!gm->material) continue;

        mat = Materials_ToMaterial(Materials_ResolveUri2(gm->material, true/*quiet please*/));
        if(!mat) continue;

        // Only create a group when the first texture is found.
        if(groupNumber == -1)
        {
            groupNumber = Materials_CreateAnimGroup(def->flags);
        }

        Materials_AddAnimGroupFrame(groupNumber, mat, gm->tics, gm->randomTics);
    }
}

void Def_PostInit(void)
{
    char name[40];

    // Particle generators: model setup.
    ded_ptcgen_t* gen = defs.ptcGens;
    for(int i = 0; i < defs.count.ptcGens.num; ++i, gen++)
    {
        ded_ptcstage_t* st = gen->stages;
        for(int k = 0; k < gen->stageCount.num; ++k, st++)
        {
            if(st->type < PTC_MODEL || st->type >= PTC_MODEL + MAX_PTC_MODELS)
                continue;

            sprintf(name, "Particle%02i", st->type - PTC_MODEL);
            modeldef_t* modef;
            if(!(modef = Models_Definition(name)) || modef->sub[0].modelId <= 0)
            {
                st->model = -1;
                continue;
            }

            model_t* mdl = Models_ToModel(modef->sub[0].modelId);
            DENG_ASSERT(mdl);

            st->model = modef - modefs;
            st->frame = mdl->frameNumForName(st->frameName);
            if(st->endFrameName[0])
            {
                st->endFrame = mdl->frameNumForName(st->endFrameName);
            }
            else
            {
                st->endFrame = -1;
            }
        }
    }

    // Detail textures.
    Textures_ClearNamespace(TN_DETAILS);
    for(int i = 0; i < defs.count.details.num; ++i)
    {
        R_CreateDetailTextureFromDef(&defs.details[i]);
    }

    // Lightmaps and flare textures.
    Textures_ClearNamespace(TN_LIGHTMAPS);
    Textures_ClearNamespace(TN_FLAREMAPS);
    for(int i = 0; i < defs.count.lights.num; ++i)
    {
        ded_light_t* lig = &defs.lights[i];

        R_CreateLightMap(lig->up);
        R_CreateLightMap(lig->down);
        R_CreateLightMap(lig->sides);

        R_CreateFlareTexture(lig->flare);
    }

    for(int i = 0; i < defs.count.decorations.num; ++i)
    {
        ded_decor_t* decor = &defs.decorations[i];

        for(int k = 0; k < DED_DECOR_NUM_LIGHTS; ++k)
        {
            ded_decorlight_t* lig = &decor->lights[k];

            if(!R_IsValidLightDecoration(lig)) break;

            R_CreateLightMap(lig->up);
            R_CreateLightMap(lig->down);
            R_CreateLightMap(lig->sides);

            R_CreateFlareTexture(lig->flare);
        }
    }

    // Surface reflections.
    Textures_ClearNamespace(TN_REFLECTIONS);
    Textures_ClearNamespace(TN_MASKS);
    for(int i = 0; i < defs.count.reflections.num; ++i)
    {
        ded_reflection_t* ref = &defs.reflections[i];
        Size2Raw size;

        R_CreateReflectionTexture(ref->shinyMap);

        size.width  = ref->maskWidth;
        size.height = ref->maskHeight;
        R_CreateMaskTexture(ref->maskMap, &size);
    }

    // Animation groups.
    Materials_ClearAnimGroups();
    for(int i = 0; i < defs.count.groups.num; ++i)
    {
        initAnimGroup(&defs.groups[i]);
    }
}

/**
 * Can we reach 'snew' if we start searching from 'sold'?
 * Take a maximum of 16 steps.
 */
boolean Def_SameStateSequence(state_t* snew, state_t* sold)
{
    int it, target = snew - states, start = sold - states;
    int count = 0;

    if(!snew || !sold)
        return false;

    if(snew == sold)
        return true; // Trivial.

    for(it = sold->nextState; it >= 0 && it != start && count < 16;
        it = states[it].nextState, ++count)
    {
        if(it == target)
            return true;

        if(it == states[it].nextState)
            break;
    }
    return false;
}

const char* Def_GetStateName(state_t* state)
{
    int idx = state - states;
    if(!state) return "(nullptr)";
    if(idx < 0) return "(<0)";
    if(idx >= defs.count.states.num) return "(>states)";
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

    l->actMaterial = Materials_ResolveUri2(def->actMaterial, true/*quiet please*/);
    l->deactMaterial = Materials_ResolveUri2(def->deactMaterial, true/*quiet please*/);

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
                    l->iparm[k] = -1;
                else
                    l->iparm[k] = Materials_ResolveUriCString2(def->iparmStr[k], true/*quiet please*/);
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
        return Def_GetSoundNumForName(id);

    case DD_DEF_SOUND_LUMPNAME:
        i = *((long*) id);
        if(i < 0 || i >= countSounds.num)
            return false;
        strcpy((char*)out, sounds[i].lumpName);
        return true;

    case DD_DEF_MUSIC:
        return Def_GetMusicNum(id);

    case DD_DEF_MAP_INFO: {
        ddmapinfo_t* mout;
        Uri* mapUri = Uri_NewWithPath2(id, FC_NONE);
        ded_mapinfo_t* map = Def_GetMapInfo(mapUri);

        Uri_Delete(mapUri);
        if(!map) return false;

        mout = (ddmapinfo_t*) out;
        mout->name = map->name;
        mout->author = map->author;
        mout->music = Def_GetMusicNum(map->music);
        mout->flags = map->flags;
        mout->ambient = map->ambient;
        mout->gravity = map->gravity;
        mout->parTime = map->parTime;
        mout->fogStart   = map->fogStart;
        mout->fogEnd     = map->fogEnd;
        mout->fogDensity = map->fogDensity;
        memcpy(mout->fogColor, map->fogColor, sizeof(mout->fogColor));
        return true; }

    case DD_DEF_TEXT:
        if(id && id[0])
        {
            // Read backwards to allow patching.
            for(i = defs.count.text.num - 1; i >= 0; i--)
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
            for(idx = defs.count.values.num - 1; idx >= 0; idx--)
            {
                if(!stricmp(defs.values[idx].id, id))
                    break;
            }
        }
        if(out) *(char**) out = (idx >= 0? defs.values[idx].text : 0);
        return idx; }

    case DD_DEF_VALUE_BY_INDEX: {
        int idx = *((long*) id);
        if(idx >= 0 && idx < defs.count.values.num)
        {
            if(out) *(char**) out = defs.values[idx].text;
            return true;
        }
        if(out) *(char**) out = 0;
        return false; }

    case DD_DEF_FINALE: { // Find InFine script by ID.
        finalescript_t* fin = (finalescript_t*) out;
        for(i = defs.count.finales.num - 1; i >= 0; i--)
        {
            if(stricmp(defs.finales[i].id, id)) continue;

            if(fin)
            {
                fin->before = defs.finales[i].before;
                fin->after  = defs.finales[i].after;
                fin->script = defs.finales[i].script;
            }
            return true;
        }
        return false; }

    case DD_DEF_FINALE_BEFORE: {
        finalescript_t* fin = (finalescript_t*) out;
        Uri* uri = Uri_NewWithPath2(id, FC_NONE);
        for(i = defs.count.finales.num - 1; i >= 0; i--)
        {
            if(!defs.finales[i].before || !Uri_Equality(defs.finales[i].before, uri)) continue;

            if(fin)
            {
                fin->before = defs.finales[i].before;
                fin->after  = defs.finales[i].after;
                fin->script = defs.finales[i].script;
            }
            Uri_Delete(uri);
            return true;
        }
        Uri_Delete(uri);
        return false; }

    case DD_DEF_FINALE_AFTER: {
        finalescript_t* fin = (finalescript_t*) out;
        Uri* uri = Uri_NewWithPath2(id, FC_NONE);
        for(i = defs.count.finales.num - 1; i >= 0; i--)
        {
            if(!defs.finales[i].after || !Uri_Equality(defs.finales[i].after, uri)) continue;

            if(fin)
            {
                fin->before = defs.finales[i].before;
                fin->after  = defs.finales[i].after;
                fin->script = defs.finales[i].script;
            }
            Uri_Delete(uri);
            return true;
        }
        Uri_Delete(uri);
        return false; }

    case DD_DEF_LINE_TYPE: {
        int typeId = strtol(id, (char **)NULL, 10);
        for(i = defs.count.lineTypes.num - 1; i >= 0; i--)
        {
            if(defs.lineTypes[i].id != typeId) continue;
            if(out) Def_CopyLineType((linetype_t*)out, &defs.lineTypes[i]);
            return true;
        }
        return false;
      }
    case DD_DEF_SECTOR_TYPE: {
        int typeId = strtol(id, (char **)NULL, 10);
        for(i = defs.count.sectorTypes.num - 1; i >= 0; i--)
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

    switch(type)
    {
    case DD_DEF_TEXT:
        if(index < 0 || index >= defs.count.text.num)
            Con_Error("Def_Set: Text index %i is invalid.\n", index);

        defs.text[index].text = (char*) M_Realloc(defs.text[index].text, strlen((char*)ptr) + 1);
        strcpy(defs.text[index].text, (char const*) ptr);
        break;

    case DD_DEF_STATE: {
        ded_state_t* stateDef;

        if(index < 0 || index >= defs.count.states.num)
            Con_Error("Def_Set: State index %i is invalid.\n", index);

        stateDef = &defs.states[index];
        switch(value)
        {
        case DD_SPRITE: {
            int sprite = *(int*) ptr;

            if(sprite < 0 || sprite >= defs.count.sprites.num)
            {
                Con_Message("Warning: Def_Set: Unknown sprite index %i.\n", sprite);
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
        if(index < 0 || index >= countSounds.num)
            Con_Error("Warning: Def_Set: Sound index %i is invalid.\n", index);

        switch(value)
        {
        case DD_LUMP:
            S_StopSound(index, 0);
            strcpy(sounds[index].lumpName, (char const*) ptr);
            if(strlen(sounds[index].lumpName))
            {
                sounds[index].lumpNum = App_FileSystem()->lumpNumForName(sounds[index].lumpName);
                if(sounds[index].lumpNum < 0)
                {
                    Con_Message("Warning: Def_Set: Unknown sound lump name \"%s\", sound (#%i) will be inaudible.\n",
                                sounds[index].lumpName, index);
                }
            }
            else
            {
                sounds[index].lumpNum = 0;
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
            musdef = defs.music + i;
        }
        else if(index >= 0 && index < defs.count.music.num)
        {
            musdef = defs.music + index;
        }
        else
            Con_Error("Def_Set: Music index %i is invalid.\n", index);

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
            return musdef - defs.music;
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
    for(i = 0; i < defs.count.mobjs.num; ++i)
    {
        StringArray_Append(array, defs.mobjs[i].id);
    }
    return array;
}

StringArray* Def_ListStateIDs(void)
{
    StringArray* array = StringArray_New();
    int i;
    for(i = 0; i < defs.count.states.num; ++i)
    {
        StringArray_Append(array, defs.states[i].id);
    }
    return array;
}

/**
 * Prints a list of all the registered mobjs to the console.
 * @todo Does this belong here?
 */
D_CMD(ListMobjs)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    if(defs.count.mobjs.num <= 0)
    {
        Con_Message("There are currently no mobjtypes defined/loaded.\n");
        return true;
    }

    Con_Printf("Registered Mobjs (ID | Name):\n");
    for(int i = 0; i < defs.count.mobjs.num; ++i)
    {
        if(defs.mobjs[i].name[0])
            Con_Printf(" %s | %s\n", defs.mobjs[i].id, defs.mobjs[i].name);
        else
            Con_Printf(" %s | (Unnamed)\n", defs.mobjs[i].id);
    }

    return true;
}
