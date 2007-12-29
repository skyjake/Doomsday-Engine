/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * def_main.c: Definitions Subsystem
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_platform.h"
#include "de_refresh.h"
#include "de_console.h"
#include "de_audio.h"
#include "de_misc.h"

#include <string.h>
#include <ctype.h>

// XGClass.h is actually a part of the engine.
#include "../../../plugins/common/include/xgclass.h"

// MACROS ------------------------------------------------------------------

#define LOOPi(n)    for(i = 0; i < (n); ++i)
#define LOOPk(n)    for(k = 0; k < (n); ++k)

// Legacy frame flags.
#define FF_FULLBRIGHT       0x8000 // flag in mobj->frame
#define FF_FRAMEMASK        0x7fff

// TYPES -------------------------------------------------------------------

typedef struct {
    char   *name;               // Name of the routine.
    void    (C_DECL * func) (); // Pointer to the function.
} actionlink_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    Def_ReadProcessDED(const char *filename);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern filename_t defsFileName;
extern filename_t topDefsFileName;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ded_t   defs;                   // The main definitions database.
sprname_t *sprnames;            // Sprite name list.
state_t *states;                // State list.
mobjinfo_t *mobjinfo;           // Map object info database.
sfxinfo_t *sounds;              // Sound effect list.

ddtext_t *texts;                // Text list.
detailtex_t *details;           // Detail texture assignments.
mobjinfo_t **stateowners;       // A pointer for each state.
ded_count_t count_sprnames;
ded_count_t count_states;
ded_count_t count_mobjinfo;
ded_count_t count_sounds;

ded_count_t count_texts;
ded_count_t count_details;
ded_count_t count_stateowners;

boolean first_ded;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean defsInited = false;
static char *dedFiles[MAX_READ];
static mobjinfo_t *gettingFor;

xgclass_t nullXgClassLinks; // Used when none defined.
xgclass_t *xgClassLinks;

// CODE --------------------------------------------------------------------

/**
 * Retrieves the XG Class list from the Game.
 * XGFunc links are provided by the Game, who owns the actual
 * XG classes and their functions.
 */
static int GetXGClasses(void)
{
    xgClassLinks = (xgclass_t *) gx.GetVariable(DD_XGFUNC_LINK);
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
    int     c;

    // Sprite name list.
    sprnames = 0;
    mobjinfo = 0;
    states = 0;
    sounds = 0;
    texts = 0;
    details = 0;
    stateowners = 0;
    DED_ZCount(&count_sprnames);
    DED_ZCount(&count_mobjinfo);
    DED_ZCount(&count_states);
    DED_ZCount(&count_sounds);
    DED_ZCount(&count_texts);
    DED_ZCount(&count_details);
    DED_ZCount(&count_stateowners);

    // Retrieve the XG Class links from the game .dll
    GetXGClasses();

    DED_Init(&defs);

    // The engine defs.
    dedFiles[0] = defsFileName;

    // Add the default ded. It will be overwritten by -defs.
    dedFiles[c = 1] = topDefsFileName;

    // See which .ded files are specified on the command line.
    if(ArgCheck("-defs"))
    {
        while(c < MAX_READ)
        {
            char   *arg = ArgNext();

            if(!arg || arg[0] == '-')
                break;
            // Add it to the list.
            dedFiles[c++] = arg;
        }
    }

    // How about additional .ded files?
    if(ArgCheckWith("-def", 1))
    {
        // Find the next empty place.
        for(c = 0; dedFiles[c] && c < MAX_READ; ++c);
            while(c < MAX_READ)
            {
                char   *arg = ArgNext();

                if(!arg || arg[0] == '-')
                    break;
                // Add it to the list.
                dedFiles[c++] = arg;
            }
    }
}

/**
 * Destroy databases.
 */
void Def_Destroy(void)
{
    // To make sure...
    DED_Destroy(&defs);
    DED_Init(&defs);

    // Destroy the databases.
    DED_DelArray((void **) &sprnames, &count_sprnames);
    DED_DelArray((void **) &states, &count_states);
    DED_DelArray((void **) &mobjinfo, &count_mobjinfo);
    DED_DelArray((void **) &sounds, &count_sounds);
    DED_DelArray((void **) &texts, &count_texts);
    DED_DelArray((void **) &details, &count_details);
    DED_DelArray((void **) &stateowners, &count_stateowners);

    defsInited = false;
}

/**
 * Guesses the location of the Defs Auto directory based on main DED
 * file.
 */
void Def_GetAutoPath(char *path)
{
    char *lastSlash;

    strcpy(path, topDefsFileName);
    lastSlash = strrchr(path, DIR_SEP_CHAR);
    if(!lastSlash)
    {
        strcpy(path, ""); // Failure!
        return;
    }

    strcpy(lastSlash + 1, "auto" DIR_SEP_STR);
}

/**
 * Returns the number of the given sprite, or -1 if it doesn't exist.
 */
int Def_GetSpriteNum(char *name)
{
    int     i;

    if(!name || !name[0])
        return -1;

    for(i = 0; i < count_sprnames.num; ++i)
        if(!stricmp(sprnames[i].name, name))
            return i;
    return -1;
}

int Def_GetMobjNum(char *id)
{
    int     i;

    if(!id || !id[0])
        return -1;

    for(i = 0; i < defs.count.mobjs.num; ++i)
        if(!strcmp(defs.mobjs[i].id, id))
            return i;
    return -1;

}

int Def_GetMobjNumForName(char *name)
{
    int i;

    if(!name || !name[0])
        return -1;

    for(i = defs.count.mobjs.num -1; i >= 0; --i)
        if(!stricmp(defs.mobjs[i].name, name))
            return i;

    return -1;
}

int Def_GetStateNum(char *id)
{
    int     i;

    if(!id || !id[0])
        return -1;

    for(i = 0; i < defs.count.states.num; ++i)
        if(!strcmp(defs.states[i].id, id))
            return i;
    return -1;
}

int Def_GetModelNum(const char *id)
{
    int     i;

    if(!id[0])
        return -1;

    for(i = 0; i < defs.count.models.num; ++i)
        if(!strcmp(defs.models[i].id, id))
            return i;
    return -1;
}

int Def_GetSoundNum(char *id)
{
    int     i;

    if(!id || !id[0])
        return -1;

    for(i = 0; i < defs.count.sounds.num; ++i)
    {
        if(!strcmp(defs.sounds[i].id, id))
            return i;
    }
    return -1;
}

/**
 * Looks up a sound using the Name key. If the name is not found, returns
 * the NULL sound index (zero).
 */
int Def_GetSoundNumForName(char *name)
{
    int     i;

    if(!name || !name[0])
        return -1;

    for(i = 0; i < defs.count.sounds.num; ++i)
        if(!stricmp(defs.sounds[i].name, name))
            return i;
    return 0;
}

int Def_GetMusicNum(char *id)
{
    int     i;

    if(!id || !id[0])
        return -1;

    for(i = 0; i < defs.count.music.num; ++i)
        if(!strcmp(defs.music[i].id, id))
            return i;
    return -1;
}

/*// A simple action function that will be executed.
   void A_ExecuteCommand(mobj_t *mobj)
   {

   } */

acfnptr_t Def_GetActionPtr(char *name)
{
    // Action links are provided by the Game, who owns the actual
    // action functions.
    actionlink_t *link = (actionlink_t *) gx.GetVariable(DD_ACTION_LINK);

    if(!name || !name[0])
        return 0;

    if(!link)
    {
        Con_Error("GetActionPtr: Game DLL doesn't have an action "
                  "function link table.\n");
    }
    for(; link->name; link++)
        if(!strcmp(name, link->name))
            return link->func;

    // The engine provides a couple of simple action functions.
    /*if(!strcmp(name, "A_ExecuteCommand"))
       return A_ExecuteCommand;
       if(!strcmp(name, "A_ExecuteCommandPSpr"))
       return A_ExecuteCommandPSpr; */

    return 0;
}

ded_mapinfo_t *Def_GetMapInfo(const char *mapID)
{
    int     i;

    if(!mapID || !mapID[0])
        return 0;

    for(i = defs.count.mapinfo.num - 1; i >= 0; i--)
        if(!stricmp(defs.mapinfo[i].id, mapID))
            return defs.mapinfo + i;
    return 0;
}

ded_decor_t *Def_GetDecoration(int number, boolean is_texture, boolean has_ext)
{
    ded_decor_t *def;
    int     i;

    for(i = defs.count.decorations.num - 1, def = defs.decorations + i;
        i >= 0; i--, def--)
    {
        if(!def->is_texture == !is_texture && number == def->surface_index)
        {
            // Is this suitable?
            if(R_IsAllowedDecoration(def, number, has_ext))
                return def;
        }
    }
    return 0;
}

/**
 * Currently reflections cannot specify any conditions when it is
 * appropriate to use them: if a high-resolution texture or a custom
 * patch overrides the texture that the reflection was meant to be
 * used with, nothing is done to react to the situation.
 */
ded_reflection_t *Def_GetReflection(int number, boolean is_texture)
{
    ded_reflection_t *ref;
    int     i;

    for(i = defs.count.reflections.num - 1, ref = defs.reflections + i;
        i >= 0; --i, --ref)
    {
        if(!ref->is_texture == !is_texture && number == ref->surfaceIndex)
        {
            // It would be great to have a unified system that would
            // determine whether effects such as decorations and
            // reflections are allowed for a certain resource...
            return ref;
        }
    }
    return 0;
}

ded_xgclass_t *Def_GetXGClass(char *name)
{
    ded_xgclass_t *def;
    int i;

    if(!name || !name[0])
        return 0;

    for(i = defs.count.xgclasses.num - 1, def = defs.xgclasses + i;
        i >= 0; i--, def--)
    {
        if(!(stricmp(name, def->id)))
            return def;
    }
    return 0;
}

ded_lumpformat_t *Def_GetMapLumpFormat(const char *name)
{
    ded_lumpformat_t *def;
    int         i;

    if(!name || !name[0])
        return 0;

    for(i = defs.count.lumpformats.num - 1, def = defs.lumpformats + i;
        i >= 0; i--, def--)
    {
        if(!(stricmp(name, def->id)))
            return def;
    }
    return 0;
}

int Def_GetFlagValue(char *flag)
{
    int     i;

    for(i = defs.count.flags.num - 1; i >= 0; i--)
        if(!strcmp(defs.flags[i].id, flag))
            return defs.flags[i].value;
    Con_Message("Def_GetFlagValue: Undefined flag '%s'.\n", flag);
    return 0;
}

/**
 * Attempts to retrieve a flag by its prefix and value.
 * Returns a ptr to the text string of the first flag it
 * finds that matches the criteria, else NULL.
 */
char *Def_GetFlagTextByPrefixVal(char *prefix, int val)
{
    int     i;

    for(i = defs.count.flags.num - 1; i >= 0; i--)
        if(strncmp(defs.flags[i].id, prefix, sizeof(prefix)) == 0 &&
            defs.flags[i].value == val)
            return defs.flags[i].text;

    return NULL;
}

int Def_EvalFlags(char *ptr)
{
    int     value = 0, len;
    char    buf[64];

    while(*ptr)
    {
        ptr = M_SkipWhite(ptr);
        len = M_FindWhite(ptr) - ptr;
        strncpy(buf, ptr, len);
        buf[len] = 0;
        value |= Def_GetFlagValue(buf);
        ptr += len;
    }
    return value;
}

int Def_GetTextNumForName(char *name)
{
    int i;

    if(!name[0])
        return -1;

    for(i = defs.count.text.num -1; i >= 0; --i)
        if(!(strcmp(defs.text[i].id, name)))
            return i;

    return -1;
}

/**
 * Escape sequences are un-escaped (\n, \r, \t, \s, \_).
 */
void Def_InitTextDef(ddtext_t *txt, char *str)
{
    char   *out, *in;

    if(!str)
        str = "";               // Handle null pointers with "".
    txt->text = M_Calloc(strlen(str) + 1);
    for(out = txt->text, in = str; *in; out++, in++)
    {
        if(*in == '\\')
        {
            in++;
            if(*in == 'n')
                *out = '\n';    // Newline.
            else if(*in == 'r')
                *out = '\r';    // Carriage return.
            else if(*in == 't')
                *out = '\t';    // Tab.
            else if(*in == '_' || *in == 's')
                *out = ' ';     // Space.
            else
                *out = *in;
            continue;
        }
        *out = *in;
    }
    // Adjust buffer to fix exactly.
    txt->text = M_Realloc(txt->text, strlen(txt->text) + 1);
}

/**
 * Callback for DD_ReadProcessDED.
 */
int Def_ReadDEDFile(const char *fn, filetype_t type, void *parm)
{
    // Skip directories.
    if(type == FT_DIRECTORY)
        return true;

    if(M_CheckFileID(fn))
    {
        if(!DED_Read(&defs, fn))
        {
            // Damn.
            Con_Error("Def_ReadDEDFile: %s\n", dedReadError);
        }
        else if(verbose)
        {
            Con_Message("DED done: %s\n", M_Pretty(fn));
        }
    }
    // Continue processing files.
    return true;
}

void Def_ReadProcessDED(const char *fileName)
{
    filename_t fn, fullFn;
    directory_t dir;

    Dir_FileName(fileName, fn);

    // We want an absolute path.
    if(!Dir_IsAbsolute(fileName))
    {
        Dir_FileDir(fileName, &dir);
        sprintf(fullFn, "%s%s", dir.path, fn);
    }
    else
    {
        strcpy(fullFn, fileName);
    }

    if(strchr(fn, '*') || strchr(fn, '?'))
    {
        // Wildcard search.
        F_ForAll(fullFn, 0, Def_ReadDEDFile);
    }
    else
    {
        Def_ReadDEDFile(fullFn, FT_NORMAL, 0);
    }
}

/**
 * Prints a count with a 2-space indentation.
 */
void Def_CountMsg(int count, const char *label)
{
    if(!verbose && !count)
        return;                 // Don't print zeros if not verbose.
    Con_Message("%5i %s\n", count, label);
}

/**
 * Reads all DD_DEFNS lumps found in the lumpinfo.
 */
void Def_ReadLumpDefs(void)
{
    int     i, c;

    for(i = 0, c = 0; i < numlumps; ++i)
        if(!strnicmp(lumpinfo[i].name, "DD_DEFNS", 8))
        {
            c++;
            if(!DED_ReadLump(&defs, i))
            {
                Con_Error("DD_ReadLumpDefs: Parse error when reading "
                          "DD_DEFNS from\n  %s.\n", W_LumpSourceFile(i));
            }
        }

    if(c || verbose)
    {
        Con_Message("ReadLumpDefs: %i definition lump%s read.\n", c,
                    c != 1 ? "s" : "");
    }
}

/**
 * Uses gettingFor. Initializes the state-owners information.
 */
int Def_StateForMobj(char *state_id)
{
    int     num = Def_GetStateNum(state_id);
    int     st, count = 16;

    if(num < 0)
        num = 0;

    // State zero is the NULL state.
    if(num > 0)
    {
        stateowners[num] = gettingFor;
        // Scan forward at most 'count' states, or until we hit a state with
        // an owner, or the NULL state.
        for(st = states[num].nextstate; st > 0 && count-- && !stateowners[st];
            st = states[st].nextstate)
        {
            stateowners[st] = gettingFor;
        }
    }
    return num;
}

int Def_GetIntValue(char *val, int *returned_val)
{
    char *data;

    // First look for a DED Value
    if(Def_Get(DD_DEF_VALUE, val, &data))
    {
        *returned_val = strtol(data, 0, 0);
        return true;
    }

    // Convert the literal string
    *returned_val = strtol(val, 0, 0);
    return false;
}

/**
 * Reads the specified definition files, and creates the sprite name,
 * state, mobjinfo, sound, music, text and mapinfo databases accordingly.
 */
void Def_Read(void)
{
    int     i, k;

    if(defsInited)
    {
        // We've already initialized the definitions once.
        // Get rid of everything.
        R_ClearModelPath();
        Def_Destroy();
    }

    first_ded = true;

    // Clear all existing definitions.
    DED_Destroy(&defs);
    DED_Init(&defs);

    // Reset file IDs so previously seen files can be processed again.
    M_ResetFileIDs();

    for(i = 0; dedFiles[i]; ++i)
    {
        Con_Message("Reading definition file: %s\n", M_Pretty(dedFiles[i]));
        Def_ReadProcessDED(dedFiles[i]);
    }

    // Read definitions from WAD files.
    Def_ReadLumpDefs();

    // Any definition hooks?
    Plug_DoHook(HOOK_DEFS, 0, &defs);

    // Check that enough defs were found.
    if(!defs.count.states.num || !defs.count.mobjs.num)
        Con_Error("DD_ReadDefs: No state or mobj definitions found!\n");

    // Sprite names.
    DED_NewEntries((void **) &sprnames, &count_sprnames, sizeof(*sprnames),
                   defs.count.sprites.num);
    for(i = 0; i < count_sprnames.num; ++i)
        strcpy(sprnames[i].name, defs.sprites[i].id);
    Def_CountMsg(count_sprnames.num, "sprite names");

    // States.
    DED_NewEntries((void **) &states, &count_states, sizeof(*states),
                   defs.count.states.num);
    for(i = 0; i < count_states.num; ++i)
    {
        ded_state_t *dst = defs.states + i;

        // Make sure duplicate IDs overwrite the earliest.
        int     stateNum = Def_GetStateNum(dst->id);
        ded_state_t *dstNew = defs.states + stateNum;
        state_t *st = states + stateNum;

        st->sprite = Def_GetSpriteNum(dst->sprite.id);
        st->flags = dst->flags;
        st->frame = dst->frame;

        // check for the old or'd in fullbright flag.
        if(st->frame & FF_FULLBRIGHT)
        {
            st->frame &= FF_FRAMEMASK;
            st->flags |= STF_FULLBRIGHT;
        }

        st->tics = dst->tics;
        st->action = Def_GetActionPtr(dst->action);
        st->nextstate = Def_GetStateNum(dst->nextstate);
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
    Def_CountMsg(count_states.num, "states");

    DED_NewEntries((void **) &stateowners, &count_stateowners,
                   sizeof(mobjinfo_t *), defs.count.states.num);

    // Mobj info.
    DED_NewEntries((void **) &mobjinfo, &count_mobjinfo, sizeof(*mobjinfo),
                   defs.count.mobjs.num);
    for(i = 0; i < count_mobjinfo.num; ++i)
    {
        ded_mobj_t *dmo = defs.mobjs + i;

        // Make sure duplicate defs overwrite the earliest.
        mobjinfo_t *mo = mobjinfo + Def_GetMobjNum(dmo->id);

        gettingFor = mo;
        mo->doomednum = dmo->doomednum;
        mo->spawnstate = Def_StateForMobj(dmo->spawnstate);
        mo->seestate = Def_StateForMobj(dmo->seestate);
        mo->painstate = Def_StateForMobj(dmo->painstate);
        mo->meleestate = Def_StateForMobj(dmo->meleestate);
        mo->missilestate = Def_StateForMobj(dmo->missilestate);
        mo->crashstate = Def_StateForMobj(dmo->crashstate);
        mo->deathstate = Def_StateForMobj(dmo->deathstate);
        mo->xdeathstate = Def_StateForMobj(dmo->xdeathstate);
        mo->raisestate = Def_StateForMobj(dmo->raisestate);
        mo->spawnhealth = dmo->spawnhealth;
        mo->seesound = Def_GetSoundNum(dmo->seesound);
        mo->reactiontime = dmo->reactiontime;
        mo->attacksound = Def_GetSoundNum(dmo->attacksound);
        mo->painchance = dmo->painchance;
        mo->painsound = Def_GetSoundNum(dmo->painsound);
        mo->deathsound = Def_GetSoundNum(dmo->deathsound);
        mo->speed = dmo->speed;
        mo->radius = dmo->radius;
        mo->height = dmo->height;
        mo->mass = dmo->mass;
        mo->damage = dmo->damage;
        mo->activesound = Def_GetSoundNum(dmo->activesound);
        mo->flags = dmo->flags[0];
        mo->flags2 = dmo->flags[1];
        mo->flags3 = dmo->flags[2];
        for(k = 0; k < NUM_MOBJ_MISC; ++k)
            mo->misc[k] = dmo->misc[k];
    }
    Def_CountMsg(count_mobjinfo.num, "things");
    Def_CountMsg(defs.count.models.num, "models");

    // Dynamic lights. Update the sprite numbers.
    for(i = 0; i < defs.count.lights.num; ++i)
    {
        k = Def_GetStateNum(defs.lights[i].state);
        if(k < 0)
        {
            // It's probably a bias light definition, then?
            if(!defs.lights[i].level[0])
            {
                Con_Message("Def_Read: Lights: Undefined state '%s'.\n",
                            defs.lights[i].state);
            }
            continue;
        }
        states[k].light = defs.lights + i;
    }
    Def_CountMsg(defs.count.lights.num, "lights");

    // Sound effects.
    DED_NewEntries((void **) &sounds, &count_sounds, sizeof(*sounds),
                   defs.count.sounds.num);
    for(i = 0; i < count_sounds.num; ++i)
    {
        ded_sound_t *snd = defs.sounds + i;

        // Make sure duplicate defs overwrite the earliest.
        sfxinfo_t *si = sounds + Def_GetSoundNum(snd->id);

        strcpy(si->id, snd->id);
        strcpy(si->lumpname, snd->lumpname);
        si->lumpnum = W_CheckNumForName(snd->lumpname);
        strcpy(si->name, snd->name);
        k = Def_GetSoundNum(snd->link);
        si->link = (k >= 0 ? sounds + k : NULL);
        si->link_pitch = snd->link_pitch;
        si->link_volume = snd->link_volume;
        si->priority = snd->priority;
        si->channels = snd->channels;
        si->flags = snd->flags;
        si->group = snd->group;
        strcpy(si->external, snd->ext.path);
    }
    Def_CountMsg(count_sounds.num, "sound effects");

    // Music.
    for(i = 0; i < defs.count.music.num; ++i)
    {
        ded_music_t *mus = defs.music + i;

        // Make sure duplicate defs overwrite the earliest.
        ded_music_t *earliest = defs.music + Def_GetMusicNum(mus->id);

        if(earliest == mus)
            continue;

        strcpy(earliest->lumpname, mus->lumpname);
        strcpy(earliest->path.path, mus->path.path);
        earliest->cdtrack = mus->cdtrack;
    }
    Def_CountMsg(defs.count.music.num, "songs");

    // Text.
    DED_NewEntries((void **) &texts, &count_texts, sizeof(*texts),
                   defs.count.text.num);

    for(i = 0; i < count_texts.num; ++i)
        Def_InitTextDef(texts + i, defs.text[i].text);

    // Handle duplicate strings.
    for(i = 0; i < count_texts.num; ++i)
    {
        if(!texts[i].text)
            continue;

        for(k = i + 1; k < count_texts.num; ++k)
            if(!strcmp(defs.text[i].id, defs.text[k].id) && texts[k].text)
            {
                // Update the earlier string.
                texts[i].text =
                    M_Realloc(texts[i].text, strlen(texts[k].text) + 1);
                strcpy(texts[i].text, texts[k].text);

                // Free the later string, it isn't used (>NUMTEXT).
                M_Free(texts[k].text);
                texts[k].text = 0;
            }
    }
    Def_CountMsg(count_texts.num, "text strings");

    // Particle generators.
    for(i = 0; i < defs.count.ptcgens.num; ++i)
    {
        ded_ptcgen_t *pg = defs.ptcgens + i;
        int     st = Def_GetStateNum(pg->state);

        if(pg->surface[0])
            pg->surfaceIndex =
                R_CheckMaterialNumForName(pg->surface, MAT_FLAT);
        else
            pg->surfaceIndex = -1;

        pg->type_num = Def_GetMobjNum(pg->type);
        pg->type2_num = Def_GetMobjNum(pg->type2);
        pg->damage_num = Def_GetMobjNum(pg->damage);

        // Figure out embedded sound ID numbers.
        for(k = 0; k < pg->stage_count.num; ++k)
        {
            if(pg->stages[k].sound.name[0])
            {
                pg->stages[k].sound.id =
                    Def_GetSoundNum(pg->stages[k].sound.name);
            }
            if(pg->stages[k].hit_sound.name[0])
            {
                pg->stages[k].hit_sound.id =
                    Def_GetSoundNum(pg->stages[k].hit_sound.name);
            }
        }

        if(st <= 0)
            continue;           // Not state triggered, then...

        // Link the definition to the state.
        if(pg->flags & PGF_STATE_CHAIN)
        {
            // Add to the chain.
            pg->state_next = states[st].ptrigger;
            states[st].ptrigger = pg;
        }
        else
        {
            // Make sure the previously built list is unlinked.
            while(states[st].ptrigger)
            {
                ded_ptcgen_t *temp =
                    ((ded_ptcgen_t *) states[st].ptrigger)->state_next;

                ((ded_ptcgen_t *) states[st].ptrigger)->state_next = NULL;
                states[st].ptrigger = temp;
            }
            states[st].ptrigger = pg;
            pg->state_next = NULL;
        }
    }
    Def_CountMsg(defs.count.ptcgens.num, "particle generators");

    // Detail textures. Initialize later...
    Def_CountMsg(defs.count.details.num, "detail textures");

    // Texture animation groups.
    Def_CountMsg(defs.count.groups.num, "animation groups");

    // Surface decorations.
    /*for(i = 0; i < defs.count.decorations.num; i++)
       {
       ded_decor_t *decor = defs.decorations + i;
       decor->flags = Def_EvalFlags(decor->flags_str);
       } */
    Def_CountMsg(defs.count.decorations.num, "surface decorations");

    // Surface reflections.
    Def_CountMsg(defs.count.reflections.num, "surface reflections");

    // Other data:
    Def_CountMsg(defs.count.mapinfo.num, "map infos");
    Def_CountMsg(defs.count.finales.num, "finales");
    Def_CountMsg(defs.count.lumpformats.num, "lump formats");

    //Def_CountMsg(defs.count.xgclasses.num, "xg classes");
    Def_CountMsg(defs.count.lines.num, "line types");
    Def_CountMsg(defs.count.sectors.num, "sector types");

    // Init the base model search path (prepend).
    Dir_FixSlashes(defs.model_path);
    R_AddModelPath(defs.model_path, false);
    // Model search path specified on the command line?
    if(ArgCheckWith("-modeldir", 1))
    {
        // Prepend to the search list; takes precedence.
        R_AddModelPath(ArgNext(), false);
    }

    defsInited = true;
}

/**
 * Initialize definitions that must be initialized when engine init is
 * complete (called from R_Init).
 */
void Def_PostInit(void)
{
    int     i, k;
    ded_ptcgen_t *gen;
    char    name[40];
    modeldef_t *modef;
    ded_ptcstage_t *st;

    // Particle generators: model setup.
    for(i = 0, gen = defs.ptcgens; i < defs.count.ptcgens.num; ++i, gen++)
    {
        for(k = 0, st = gen->stages; k < gen->stage_count.num; ++k, st++)
        {
            if(st->type < PTC_MODEL || st->type >= PTC_MODEL + MAX_PTC_MODELS)
                continue;

            sprintf(name, "Particle%02i", st->type - PTC_MODEL);
            if(!(modef = R_CheckIDModelFor(name)) || modef->sub[0].model <= 0)
            {
                st->model = -1;
                continue;
            }

            st->model = modef - modefs;
            st->frame =
                R_ModelFrameNumForName(modef->sub[0].model, st->frame_name);
            if(st->end_frame_name[0])
            {
                st->end_frame =
                    R_ModelFrameNumForName(modef->sub[0].model,
                                           st->end_frame_name);
            }
            else
            {
                st->end_frame = -1;
            }
        }
    }

    // Detail textures.
    DED_DelArray((void **) &details, &count_details);
    DED_NewEntries((void **) &details, &count_details, sizeof(*details),
                   defs.count.details.num);
    for(i = 0; i < defs.count.details.num; ++i)
    {
        details[i].wall_texture =
            R_CheckMaterialNumForName(defs.details[i].wall, MAT_TEXTURE);
        details[i].flat_texture =
            R_CheckMaterialNumForName(defs.details[i].flat, MAT_FLAT);
        details[i].detail_lump =
            W_CheckNumForName(defs.details[i].detail_lump.path);
        details[i].gltex = 0;   // Not loaded.
    }

    // Surface decorations.
    for(i = 0; i < defs.count.decorations.num; ++i)
    {
        ded_decor_t *decor = defs.decorations + i;

        decor->surface_index =
            R_CheckMaterialNumForName(decor->surface,
                                      (decor->is_texture? MAT_TEXTURE : MAT_FLAT));
    }

    // Surface reflections.
    for(i = 0; i < defs.count.reflections.num; ++i)
    {
        ded_reflection_t *ref = defs.reflections + i;

        ref->surfaceIndex =
            R_CheckMaterialNumForName(ref->surface, (ref->is_texture? MAT_TEXTURE : MAT_FLAT));

        // Initialize the pointers to handle textures.
        ref->shiny_tex = ref->mask_tex = 0;
        ref->use_shiny = ref->use_mask = NULL;

        if(ref->shiny_map.path[0])
        {
            ref->use_shiny = ref;

            // Find the earliest instance of this texture.
            for(k = 0; k < i; ++k)
            {
                if(!stricmp(ref->shiny_map.path,
                            defs.reflections[k].shiny_map.path))
                {
                    ref->use_shiny = &defs.reflections[k];
                    break;
                }
            }
        }

        if(ref->mask_map.path[0])
        {
            ref->use_mask = ref;

            // Find the earliest instance of this texture.
            for(k = 0; k < i; ++k)
            {
                if(!stricmp(ref->mask_map.path,
                            defs.reflections[k].mask_map.path))
                {
                    ref->use_mask = &defs.reflections[k];
                    break;
                }
            }
        }
    }

    // Lump formats.
    for(i = 0; i < defs.count.lumpformats.num; ++i)
    {
        // \todo Initialize formats
    }

    // Animation groups.
    R_DestroyAnimGroups();
    for(i = 0; i < defs.count.groups.num; ++i)
    {
        R_InitAnimGroup(defs.groups + i);
    }
}

void Def_SetLightMap(ded_lightmap_t * map, const char *id,
                     unsigned int texture)
{
    if(stricmp(map->id, id))
        return;                 // Not the same lightmap?

    map->tex = texture;
}

void Def_LightMapLoaded(const char *id, unsigned int texture)
{
    int     i, k;
    ded_decor_t *decor;

    // Load lightmaps.
    for(i = 0; i < defs.count.lights.num; ++i)
    {
        Def_SetLightMap(&defs.lights[i].up, id, texture);
        Def_SetLightMap(&defs.lights[i].down, id, texture);
        Def_SetLightMap(&defs.lights[i].sides, id, texture);
    }

    for(i = 0, decor = defs.decorations; i < defs.count.decorations.num;
        ++i, decor++)
    {
        for(k = 0; k < DED_DECOR_NUM_LIGHTS; ++k)
        {
            if(!R_IsValidLightDecoration(&decor->lights[k]))
                break;

            Def_SetLightMap(&decor->lights[k].up, id, texture);
            Def_SetLightMap(&decor->lights[k].down, id, texture);
            Def_SetLightMap(&decor->lights[k].sides, id, texture);
        }
    }
}

void Def_SetFlareMap(ded_flaremap_t * map, const char *id,
                     unsigned int texture, boolean disabled,
                     boolean custom)
{
    if(stricmp(map->id, id))
        return;                 // Not the same lightmap?

    map->tex = texture;
    map->disabled = disabled;
    map->custom = custom;
}

void Def_FlareMapLoaded(const char *id, unsigned int texture,
                        boolean disabled, boolean custom)
{
    int     i, k;
    ded_decor_t *decor;

    for(i = 0; i < defs.count.lights.num; ++i)
    {
        Def_SetFlareMap(&defs.lights[i].flare, id, texture, disabled,
                        custom);
    }

    for(i = 0, decor = defs.decorations; i < defs.count.decorations.num;
        ++i, decor++)
    {
        for(k = 0; k < DED_DECOR_NUM_LIGHTS; ++k)
        {
            if(!R_IsValidLightDecoration(&decor->lights[k]))
                break;

            Def_SetFlareMap(&decor->lights[k].flare, id, texture,
                            disabled, custom);
        }
    }
}

/**
 * Can we reach 'snew' if we start searching from 'sold'?
 * Take a maximum of 16 steps.
 */
boolean Def_SameStateSequence(state_t * snew, state_t * sold)
{
    int     it, target = snew - states, start = sold - states;
    int     count = 0;

    if(!snew || !sold)
        return false;

    if(snew == sold)
        return true;            // Trivial.

    for(it = sold->nextstate; it >= 0 && it != start && count < 16;
        it = states[it].nextstate, ++count)
    {
        if(it == target)
            return true;

        if(it == states[it].nextstate)
            break;
    }
    return false;
}

#if 0
/**
 * @return          @c true, if the mobj (in mobjinfo) has the given state.
 */
boolean DD_HasMobjState(int mobj_num, int state_num)
{
    mobjinfo_t *mo = mobjinfo + mobj_num;
    state_t *target = states + state_num;
    int     i, statelist[9] = {
        mo->spawnstate,
        mo->seestate,
        mo->painstate,
        mo->meleestate,
        mo->missilestate,
        mo->crashstate,
        mo->deathstate,
        mo->xdeathstate,
        mo->raisestate
    };

    for(i = 0; i < 8; ++i)
        if(statelist[i] > 0 &&
           DD_SameStateSequence(target, states + statelist[i]))
            return true;
    // Not here...
    return false;
}
#endif

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
void Def_CopyLineType(linetype_t * l, ded_linetype_t * def)
{
    int     i, k, a;

    int temp;

    l->id = def->id;
    l->flags = def->flags[0];
    l->flags2 = def->flags[1];
    l->flags3 = def->flags[2];
    l->line_class = def->line_class;
    l->act_type = def->act_type;
    l->act_count = def->act_count;
    l->act_time = def->act_time;
    l->act_tag = def->act_tag;

    for(i = 0; i < 10; ++i)
    {
        if(i == 9)
            l->aparm[i] = Def_GetMobjNum(def->aparm9);
        else
            l->aparm[i] = def->aparm[i];
    }

    l->ticker_start = def->ticker_start;
    l->ticker_end = def->ticker_end;
    l->ticker_interval = def->ticker_interval;
    l->act_sound = Friendly(Def_GetSoundNum(def->act_sound));
    l->deact_sound = Friendly(Def_GetSoundNum(def->deact_sound));
    l->ev_chain = def->ev_chain;
    l->act_chain = def->act_chain;
    l->deact_chain = def->deact_chain;
    l->act_linetype = def->act_linetype;
    l->deact_linetype = def->deact_linetype;
    l->wallsection = def->wallsection;
    l->act_tex = Friendly(R_CheckMaterialNumForName(def->act_tex, MAT_TEXTURE));
    l->deact_tex = Friendly(R_CheckMaterialNumForName(def->deact_tex, MAT_TEXTURE));
    l->act_msg = def->act_msg;
    l->deact_msg = def->deact_msg;
    l->texmove_angle = def->texmove_angle;
    l->texmove_speed = def->texmove_speed;
    memcpy(l->iparm, def->iparm, sizeof(int) * 20);
    memcpy(l->fparm, def->fparm, sizeof(int) * 20);
    LOOPi(5) l->sparm[i] = def->sparm[i];

    // Some of the parameters might be strings depending on the line class.
    // Find the right mapping table.
    for(k = 0; k < 20; ++k)
    {
        temp = 0;
        a = xgClassLinks[l->line_class].iparm[k].map;
        if(a < 0)
            continue;

        if(a & MAP_SND)
        {
            l->iparm[k] = Friendly(Def_GetSoundNum(def->iparm_str[k]));
        }
        else if(a & MAP_TEX)
        {
            if(!stricmp(def->iparm_str[k], "-1"))
                l->iparm[k] = -1;
            else
                l->iparm[k] =
                    Friendly(R_CheckMaterialNumForName(def->iparm_str[k], MAT_FLAT));
        }
        else if(a & MAP_FLAT)
        {
            if(def->iparm_str[k][0])
                l->iparm[k] =
                    Friendly(R_CheckMaterialNumForName(def->iparm_str[k], MAT_FLAT));
        }
        else if(a & MAP_MUS)
        {
            temp = Friendly(Def_GetMusicNum(def->iparm_str[k]));

            if(temp == 0)
            {
                temp = Def_EvalFlags(def->iparm_str[k]);
                if(temp)
                    l->iparm[k] = temp;
            } else
                l->iparm[k] = Friendly(Def_GetMusicNum(def->iparm_str[k]));
        }
        else
        {
            temp = Def_EvalFlags(def->iparm_str[k]);

            if(temp)
                l->iparm[k] = temp;
        }
    }
}

/*
 * Converts a DED sector type to the internal format.
 */
void Def_CopySectorType(sectortype_t * s, ded_sectortype_t * def)
{
    int     i, k;

    s->id = def->id;
    s->flags = def->flags;
    s->act_tag = def->act_tag;
    LOOPi(5)
    {
        s->chain[i] = def->chain[i];
        s->chain_flags[i] = def->chain_flags[i];
        s->start[i] = def->start[i];
        s->end[i] = def->end[i];
        LOOPk(2) s->interval[i][k] = def->interval[i][k];
        s->count[i] = def->count[i];
    }
    s->ambient_sound = Friendly(Def_GetSoundNum(def->ambient_sound));
    LOOPi(2)
    {
        s->sound_interval[i] = def->sound_interval[i];
        s->texmove_angle[i] = def->texmove_angle[i];
        s->texmove_speed[i] = def->texmove_speed[i];
    }
    s->wind_angle = def->wind_angle;
    s->wind_speed = def->wind_speed;
    s->vertical_wind = def->vertical_wind;
    s->gravity = def->gravity;
    s->friction = def->friction;
    s->lightfunc = def->lightfunc;
    LOOPi(2) s->light_interval[i] = def->light_interval[i];
    LOOPi(3)
    {
        s->colfunc[i] = def->colfunc[i];
        LOOPk(2) s->col_interval[i][k] = def->col_interval[i][k];
    }
    s->floorfunc = def->floorfunc;
    s->floormul = def->floormul;
    s->flooroff = def->flooroff;
    LOOPi(2) s->floor_interval[i] = def->floor_interval[i];
    s->ceilfunc = def->ceilfunc;
    s->ceilmul = def->ceilmul;
    s->ceiloff = def->ceiloff;
    LOOPi(2) s->ceil_interval[i] = def->ceil_interval[i];
}

/**
 * @return          @c true, if the definition was found.
 */
int Def_Get(int type, char *id, void *out)
{
    int             i;
    ded_mapinfo_t  *map;
    ddmapinfo_t    *mout;
    finalescript_t *fin;

    switch(type)
    {
    case DD_DEF_MOBJ:
        return Def_GetMobjNum(id);

    case DD_DEF_MOBJ_BY_NAME:
        return Def_GetMobjNumForName(id);

    case DD_DEF_STATE:
        return Def_GetStateNum(id);

    case DD_DEF_SPRITE:
        return Def_GetSpriteNum(id);

    case DD_DEF_SOUND:
        return Def_GetSoundNum(id);

    case DD_DEF_SOUND_BY_NAME:
        return Def_GetSoundNumForName(id);

    case DD_DEF_SOUND_LUMPNAME:
        i = *((long*) id);
        if(i < 0 || i >= count_sounds.num)
            return false;
        strcpy(out, sounds[i].lumpname);
        break;

    case DD_DEF_MUSIC:
        return Def_GetMusicNum(id);

    case DD_DEF_MAP_INFO:
        map = Def_GetMapInfo(id);
        if(!map)
            return false;

        mout = (ddmapinfo_t *) out;
        mout->name = map->name;
        mout->author = map->author;
        mout->music = Def_GetMusicNum(map->music);
        mout->flags = map->flags;
        mout->ambient = map->ambient;
        mout->gravity = map->gravity;
        mout->partime = map->partime;
        break;

    case DD_DEF_TEXT:
        for(i = 0; i < defs.count.text.num; ++i)
            if(!stricmp(defs.text[i].id, id))
            {
                if(out)
                    *(char **) out = defs.text[i].text;
                return i;
            }
        return -1;

    case DD_DEF_VALUE:
        // Read backwards to allow patching.
        for(i = defs.count.values.num - 1; i >= 0; i--)
            if(!stricmp(defs.values[i].id, id))
            {
                if(out)
                    *(char **) out = defs.values[i].text;
                return true;
            }
        return false;

    case DD_DEF_FINALE:
        // Find InFine script by ID.
        for(i = defs.count.finales.num - 1; i >= 0; i--)
            if(!stricmp(defs.finales[i].id, id))
            {
                // This has a matching ID. Return a pointer to the script.
                *(void **) out = defs.finales[i].script;
                return true;
            }
        return false;

    case DD_DEF_FINALE_BEFORE:
        fin = (finalescript_t *) out;
        for(i = defs.count.finales.num - 1; i >= 0; i--)
            if(!stricmp(defs.finales[i].before, id))
            {
                fin->before = defs.finales[i].before;
                fin->after = defs.finales[i].after;
                fin->script = defs.finales[i].script;
                return true;
            }
        return false;

    case DD_DEF_FINALE_AFTER:
        fin = (finalescript_t *) out;
        for(i = defs.count.finales.num - 1; i >= 0; i--)
            if(!stricmp(defs.finales[i].after, id))
            {

                fin->before = defs.finales[i].before;
                fin->after = defs.finales[i].after;
                fin->script = defs.finales[i].script;
                return true;
            }
        return false;

    case DD_DEF_LINE_TYPE:
        for(i = defs.count.lines.num - 1; i >= 0; i--)
            if(defs.lines[i].id == strtol(id, (char **)NULL, 10))
            {
                if(out)
                    Def_CopyLineType(out, &defs.lines[i]);
                return true;
            }
        return false;

    case DD_DEF_SECTOR_TYPE:
        for(i = defs.count.sectors.num - 1; i >= 0; i--)
            if(defs.sectors[i].id == strtol(id, (char **)NULL, 10))
            {
                if(out)
                    Def_CopySectorType(out, &defs.sectors[i]);
                return true;
            }
        return false;

    default:
        return false;
    }
    return true;
}

/**
 * This is supposed to be the main interface for outside parties to
 * modify definitions (unless they want to do it manually with dedfile.h).
 */
int Def_Set(int type, int index, int value, void *ptr)
{
    ded_music_t *musdef = 0;
    int     i;

    switch (type)
    {
    case DD_DEF_SOUND:
        if(index < 0 || index >= count_sounds.num)
            Con_Error("Def_Set: Sound index %i is invalid.\n", index);

        switch(value)
        {
        case DD_LUMP:
            S_StopSound(index, 0);
            strcpy(sounds[index].lumpname, ptr);
            sounds[index].lumpnum = W_CheckNumForName(sounds[index].lumpname);
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
                strcpy(musdef->id, ptr);
            break;

        case DD_LUMP:
            if(ptr)
                strcpy(musdef->lumpname, ptr);
            break;

        case DD_CD_TRACK:
            musdef->cdtrack = *(int *) ptr;
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

/**
 * \fixme Prints a list of all the registered mobjs to the console.
 * Does this belong here?
 */
D_CMD(ListMobjs)
{
    int i;

    Con_Printf("Registered Mobjs (ID | Name):\n");

    for(i = 0; i < defs.count.mobjs.num; ++i)
    {
        if(defs.mobjs[i].name[0])
            Con_Printf(" %s | %s\n", defs.mobjs[i].id, defs.mobjs[i].name);
        else
            Con_Printf(" %s | (Unknown)\n",defs.mobjs[i].id);
    }

    return true;
}
