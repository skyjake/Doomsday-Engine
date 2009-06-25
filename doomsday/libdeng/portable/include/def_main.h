/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * def_main.h: Definitions Subsystem.
 */

#ifndef __DOOMSDAY_DEFINITIONS_MAIN_H__
#define __DOOMSDAY_DEFINITIONS_MAIN_H__

#include "def_data.h"

typedef struct sfxinfo_s {
    void*           data; // Pointer to sound data.
    int             lumpNum;
    char            lumpName[9]; // Actual lump name of the sound (full name).
    char            id[32]; // Identifier name (from the def).
    char            name[32]; // Long name.
    struct sfxinfo_s* link; // Link to another sound.
    int             linkPitch;
    int             linkVolume;
    int             priority;
    int             channels; // Max. channels for the sound to occupy.
    int             usefulness; // Used to determine when to cache out.
    int             flags;
    int             group;
    char            external[256]; // Path to external file.
} sfxinfo_t;

extern ded_t defs; // The main definitions database.
extern sprname_t* sprNames; // Sprite name list.
extern state_t* states; // State list.
extern ded_light_t** stateLights;
extern ded_ptcgen_t** statePtcGens;
extern mobjinfo_t* mobjInfo; // Map object info database.
extern sfxinfo_t* sounds; // Sound effect list.
extern ddtext_t* texts; // Text list.
extern mobjinfo_t** stateOwners;
extern ded_count_t countSprNames;
extern ded_count_t countStates;

void            Def_Init(void);
void            Def_PostInit(void);

// Destroy databases.
void            Def_Destroy(void);

void            Def_GetAutoPath(char* path, size_t len);

// Reads the specified definition file, and creates the sprite name,
// state, mobjinfo, sound, music and text databases accordingly.
void            Def_Read(void);

void            Def_ReadProcessDED(const char* fileName);

int             Def_GetMobjNum(const char* id);
int             Def_GetMobjNumForName(const char* name);
int             Def_GetStateNum(const char* id);
int             Def_GetSpriteNum(const char* name);
int             Def_GetModelNum(const char* id);
int             Def_GetMusicNum(const char* id);
int             Def_GetSoundNum(const char* id);
int             Def_EvalFlags(char* ptr);
ded_mapinfo_t*  Def_GetMapInfo(const char* mapID);
ded_sky_t*      Def_GetSky(const char* id);
ded_material_t* Def_GetMaterial(const char* name, material_namespace_t group);
ded_light_t*    Def_GetLightDef(int spr, int frame);
ded_decor_t*    Def_GetDecoration(material_t* mat, boolean hasExt);
ded_reflection_t* Def_GetReflection(material_t* mat, boolean hasExt);
ded_detailtexture_t* Def_GetDetailTex(material_t* mat, boolean hasExt);
ded_ptcgen_t*   Def_GetGenerator(material_t* mat, boolean hasExt);
ded_ptcgen_t*   Def_GetDamageGenerator(int mobjType);
int             Def_Get(int type, const char* id, void* out);
boolean         Def_SameStateSequence(state_t* snew, state_t* sold);

D_CMD(ListMobjs);
#endif
