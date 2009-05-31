/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1998-2003 Randy Heit <rheit@iastate.edu> (Zdoom)
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
 *
 * In Zdoom, from which this code is based, it was originally under the 3 clause
 * BSD license as described here -> http://www.opensource.org/licenses/bsd-license.php
 */


/**
 * dehmain.c: Dehacked Reader Plugin for Doomsday
 *
 * Much of this has been taken from or is based on ZDoom's DEH reader.
 * Unsupported Dehacked features have been commented out.
 *
 * Put the Doomsday Include directory in your path.
 * This DLL also serves as an example of a Doomsday plugin.
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>

// This plugin accesses the internal definition arrays. This dependency should
// be removed entirely, either by making the plugin modify the definitions
// via an API or by integrating the plugin into the engine.
#include "../../../engine/portable/include/def_data.h"

#define __INTERNAL_MAP_DATA_ACCESS__
#include <doomsday.h>

// MACROS ------------------------------------------------------------------

#define OFF_STATE   0x01000000
#define OFF_SOUND   0x02000000
#define OFF_FLOAT   0x04000000
#define OFF_SPRITE  0x08000000
#define OFF_FIXED   0x10000000
#define OFF_MASK    0x00ffffff

#define myoffsetof(type,identifier,fl) (((size_t)&((type*)0)->identifier)|fl)

#define LPrintf     Con_Message

//#define LINESIZE    2048
#define NUMSPRITES  138
#define NUMSTATES   968

#define CHECKKEY(a,b)       if (!stricmp (Line1, (a))) (b) = atoi(Line2);

// TYPES -------------------------------------------------------------------

typedef struct Key {
    char   *name;
    ptrdiff_t offset;
} Key_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

#ifdef UNIX
void    InitPlugin(void) __attribute__ ((constructor));
#endif

int     PatchThing(int);
int     PatchSound(int);
int     PatchFrame(int);
int     PatchSprite(int);
int     PatchAmmo(int);
int     PatchWeapon(int);
int     PatchPointer(int);
int     PatchCheats(int);
int     PatchMisc(int);
int     PatchText(int);
int     PatchStrings(int);
int     PatchPars(int);
int     PatchCodePtrs(int);
int     DoInclude(int);
void    ApplyDEH(char *patch, int length);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ded_t  *ded;

boolean verbose;
char   *PatchFile, *PatchPt;
char   *Line1, *Line2;
int     dversion, pversion;
boolean including, includenotext;

char   *SoundMap[] = {
    "None",                     // 000
    "pistol",                   // 001
    "shotgn",                   // 002
    "sgcock",                   // 003
    "dshtgn",                   // 004
    "dbopn",                    // 005
    "dbcls",                    // 006
    "dbload",                   // 007
    "plasma",                   // 008
    "bfg",                      // 009
    "sawup",                    // 010
    "sawidl",                   // 011
    "sawful",                   // 012
    "sawhit",                   // 013
    "rlaunc",                   // 014
    "rxplod",                   // 015
    "firsht",                   // 016
    "firxpl",                   // 017
    "pstart",                   // 018
    "pstop",                    // 019
    "doropn",                   // 020
    "dorcls",                   // 021
    "stnmov",                   // 022
    "swtchn",                   // 023
    "swtchx",                   // 024
    "plpain",                   // 025
    "dmpain",                   // 026
    "popain",                   // 027
    "vipain",                   // 028
    "mnpain",                   // 029
    "pepain",                   // 030
    "slop",                     // 031
    "itemup",                   // 032
    "wpnup",                    // 033
    "oof",                      // 034
    "telept",                   // 035
    "posit1",                   // 036
    "posit2",                   // 037
    "posit3",                   // 038
    "bgsit1",                   // 039
    "bgsit2",                   // 040
    "sgtsit",                   // 041
    "cacsit",                   // 042
    "brssit",                   // 043
    "cybsit",                   // 044
    "spisit",                   // 045
    "bspsit",                   // 046
    "kntsit",                   // 047
    "vilsit",                   // 048
    "mansit",                   // 049
    "pesit",                    // 050
    "sklatk",                   // 051
    "sgtatk",                   // 052
    "skepch",                   // 053
    "vilatk",                   // 054
    "claw",                     // 055
    "skeswg",                   // 056
    "pldeth",                   // 057
    "pdiehi",                   // 058
    "podth1",                   // 059
    "podth2",                   // 060
    "podth3",                   // 061
    "bgdth1",                   // 062
    "bgdth2",                   // 063
    "sgtdth",                   // 064
    "cacdth",                   // 065
    "skldth",                   // 066
    "brsdth",                   // 067
    "cybdth",                   // 068
    "spidth",                   // 069
    "bspdth",                   // 070
    "vildth",                   // 071
    "kntdth",                   // 072
    "pedth",                    // 073
    "skedth",                   // 074
    "posact",                   // 075
    "bgact",                    // 076
    "dmact",                    // 077
    "bspact",                   // 078
    "bspwlk",                   // 079
    "vilact",                   // 080
    "noway",                    // 081
    "barexp",                   // 082
    "punch",                    // 083
    "hoof",                     // 084
    "metal",                    // 085
    "chgun",                    // 086
    "tink",                     // 087
    "bdopn",                    // 088
    "bdcls",                    // 089
    "itmbk",                    // 090
    "flame",                    // 091
    "flamst",                   // 092
    "getpow",                   // 093
    "bospit",                   // 094
    "boscub",                   // 095
    "bossit",                   // 096
    "bospn",                    // 097
    "bosdth",                   // 098
    "manatk",                   // 099
    "mandth",                   // 100
    "sssit",                    // 101
    "ssdth",                    // 102
    "keenpn",                   // 103
    "keendt",                   // 104
    "skeact",                   // 105
    "skesit",                   // 106
    "skeatk",                   // 107
    "radio"                     // 108
};

boolean BackedUpData = false;

// This is the original data before it gets replaced by a patch.
char    OrgSprNames[NUMSPRITES][5];
char    OrgActionPtrs[NUMSTATES][40];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char *unknown_str = "Unknown key %s encountered in %s %d.\n";

// From DeHackEd source.
static int toff[] = { 129044, 129044, 129044, 129284, 129380 };

// A conversion array to convert from the 448 code pointers to the 966
// Frames that exist.
// Again taken from the DeHackEd source.
static short codepconv[448] = { 1, 2, 3, 4, 6, 9, 10, 11, 12, 14,
    16, 17, 18, 19, 20, 22, 29, 30, 31, 32,
    33, 34, 36, 38, 39, 41, 43, 44, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
    59, 60, 61, 62, 63, 65, 66, 67, 68, 69,
    70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
    119, 127, 157, 159, 160, 166, 167, 174, 175, 176,
    177, 178, 179, 180, 181, 182, 183, 184, 185, 188,
    190, 191, 195, 196, 207, 208, 209, 210, 211, 212,
    213, 214, 215, 216, 217, 218, 221, 223, 224, 228,
    229, 241, 242, 243, 244, 245, 246, 247, 248, 249,
    250, 251, 252, 253, 254, 255, 256, 257, 258, 259,
    260, 261, 262, 263, 264, 270, 272, 273, 281, 282,
    283, 284, 285, 286, 287, 288, 289, 290, 291, 292,
    293, 294, 295, 296, 297, 298, 299, 300, 301, 302,
    303, 304, 305, 306, 307, 308, 309, 310, 316, 317,
    321, 322, 323, 324, 325, 326, 327, 328, 329, 330,
    331, 332, 333, 334, 335, 336, 337, 338, 339, 340,
    341, 342, 344, 347, 348, 362, 363, 364, 365, 366,
    367, 368, 369, 370, 371, 372, 373, 374, 375, 376,
    377, 378, 379, 380, 381, 382, 383, 384, 385, 387,
    389, 390, 397, 406, 407, 408, 409, 410, 411, 412,
    413, 414, 415, 416, 417, 418, 419, 421, 423, 424,
    430, 431, 442, 443, 444, 445, 446, 447, 448, 449,
    450, 451, 452, 453, 454, 456, 458, 460, 463, 465,
    475, 476, 477, 478, 479, 480, 481, 482, 483, 484,
    485, 486, 487, 489, 491, 493, 502, 503, 504, 505,
    506, 508, 511, 514, 527, 528, 529, 530, 531, 532,
    533, 534, 535, 536, 537, 538, 539, 541, 543, 545,
    548, 556, 557, 558, 559, 560, 561, 562, 563, 564,
    565, 566, 567, 568, 570, 572, 574, 585, 586, 587,
    588, 589, 590, 594, 596, 598, 601, 602, 603, 604,
    605, 606, 607, 608, 609, 610, 611, 612, 613, 614,
    615, 616, 617, 618, 620, 621, 622, 631, 632, 633,
    635, 636, 637, 638, 639, 640, 641, 642, 643, 644,
    645, 646, 647, 648, 650, 652, 653, 654, 659, 674,
    675, 676, 677, 678, 679, 680, 681, 682, 683, 684,
    685, 686, 687, 688, 689, 690, 692, 696, 700, 701,
    702, 703, 704, 705, 706, 707, 708, 709, 710, 711,
    713, 715, 718, 726, 727, 728, 729, 730, 731, 732,
    733, 734, 735, 736, 737, 738, 739, 740, 741, 743,
    745, 746, 750, 751, 766, 774, 777, 779, 780, 783,
    784, 785, 786, 787, 788, 789, 790, 791, 792, 793,
    794, 795, 796, 797, 798, 801, 809, 811
};

static byte OrgHeights[] = {
    56, 56, 56, 56, 16, 56, 8, 16, 64, 8, 56, 56,
    56, 56, 56, 64, 8, 64, 56, 100, 64, 110, 56, 56,
    72, 16, 32, 32, 32, 16, 42, 8, 8, 8,
    8, 8, 8, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 68, 84, 84,
    68, 52, 84, 68, 52, 52, 68, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 88, 88, 64, 64, 64, 64,
    16, 16, 16
};

static const struct {
    char   *name;
    int     (*func) (int);
} Modes[] =
{
    // These appear in .deh and .bex files
    {
    "Thing", PatchThing},
    {
    "Sound", PatchSound},
    {
    "Frame", PatchFrame},
    {
    "Sprite", PatchSprite},
    {
    "Ammo", PatchAmmo},
    {
    "Weapon", PatchWeapon},
    {
    "Pointer", PatchPointer},
    {
    "Cheat", PatchCheats},
    {
    "Misc", PatchMisc},
    {
    "Text", PatchText},
        // These appear in .bex files
    {
    "include", DoInclude},
    {
    "[STRINGS]", PatchStrings},
    {
    "[PARS]", PatchPars},
    {
    "[CODEPTR]", PatchCodePtrs},
    {
NULL,},};

// CODE --------------------------------------------------------------------

void BackupData(void)
{
    int     i;

    if(BackedUpData)
        return;

    for(i = 0; i < NUMSPRITES && i < ded->count.sprites.num; i++)
        strcpy(OrgSprNames[i], ded->sprites[i].id);

    for(i = 0; i < NUMSTATES && i < ded->count.states.num; i++)
        strcpy(OrgActionPtrs[i], ded->states[i].action);
}

char    com_token[8192];
boolean com_eof;

/**
 * Parse a token out of a string.
 * From ZDoom cmdlib.cpp
 */
char   *COM_Parse(char *data)
{
    int     c;
    int     len;

    len = 0;
    com_token[0] = 0;

    if(!data)
        return NULL;

    // skip whitespace
  skipwhite:
    while((c = *data) <= ' ')
    {
        if(c == 0)
        {
            com_eof = true;
            return NULL;        // end of file;
        }
        data++;
    }

    // skip // comments
    if(c == '/' && data[1] == '/')
    {
        while(*data && *data != '\n')
            data++;
        goto skipwhite;
    }

    // handle quoted strings specially
    if(c == '\"')
    {
        data++;

        while((c = *data++) != '\"')
        {
            com_token[len] = c;
            len++;
        }

        com_token[len] = 0;
        return data;
    }

    // parse single characters
    if(c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' ||
       /*[RH] */ c == '=')
    {
        com_token[len] = c;
        len++;
        com_token[len] = 0;
        return data + 1;
    }

    // parse a regular word
    do
    {
        com_token[len] = c;
        data++;
        len++;
        c = *data;
        if(c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' ||
           c == ':' || c == '=')
            break;
    } while(c > 32);

    com_token[len] = 0;
    return data;
}

boolean IsNum(char *str)
{
    char   *end;

    strtol(str, &end, 0);
    if(*end && !isspace(*end))
        return false;
    return true;
}

static boolean HandleKey(const struct Key *keys, void *structure,
                         const char *key, int value)
{
    int     off;
    void   *ptr;

    while(keys->name && stricmp(keys->name, key))
        keys++;

    if(keys->name)
    {
        //*((int *)(((byte *)structure) + keys->offset)) = value;
        off = keys->offset & OFF_MASK;
        ptr = (void *) (((byte *) structure) + off);
        // Apply value.
        if(keys->offset & OFF_STATE)
            strcpy((char *) ptr, ded->states[value].id);
        else if(keys->offset & OFF_SPRITE)
            strcpy((char *) ptr, ded->sprites[value].id);
        else if(keys->offset & OFF_SOUND)
            strcpy((char *) ptr, ded->sounds[value].id);
        else if(keys->offset & OFF_FLOAT)
        {
            if(value < 0x2000)
                *(float *) ptr = (float) value; // / (float) 0x10000;
            else
                *(float *) ptr = value / (float) 0x10000;
        }
        else if(keys->offset & OFF_FIXED)
            *(float *) ptr = value / (float) 0x10000;
        else
            *(int *) ptr = value;
        return false;
    }

    return true;
}

static boolean ReadChars(char **stuff, int size, boolean skipJunk)
{
    char   *str = *stuff;

    if(!size)
    {
        *str = 0;
        return true;
    }

    do
    {
        // Ignore carriage returns
        if(*PatchPt != '\r')
            *str++ = *PatchPt;
        else
            size++;

        PatchPt++;
    } while(--size);

    *str = 0;

    if(skipJunk)
    {
        // Skip anything else on the line.
        while(*PatchPt != '\n' && *PatchPt != 0)
            PatchPt++;
    }

    return true;
}

void ReplaceSpecialChars(char *str)
{
    char   *p = str, c;
    int     i;

    while((c = *p++))
    {
        if(c != '\\')
        {
            *str++ = c;
        }
        else
        {
            switch (*p)
            {
            case 'n':
            case 'N':
                *str++ = '\n';
                break;
            case 't':
            case 'T':
                *str++ = '\t';
                break;
            case 'r':
            case 'R':
                *str++ = '\r';
                break;
            case 'x':
            case 'X':
                c = 0;
                p++;
                for(i = 0; i < 2; i++)
                {
                    c <<= 4;
                    if(*p >= '0' && *p <= '9')
                        c += *p - '0';
                    else if(*p >= 'a' && *p <= 'f')
                        c += 10 + *p - 'a';
                    else if(*p >= 'A' && *p <= 'F')
                        c += 10 + *p - 'A';
                    else
                        break;
                    p++;
                }
                *str++ = c;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                c = 0;
                for(i = 0; i < 3; i++)
                {
                    c <<= 3;
                    if(*p >= '0' && *p <= '7')
                        c += *p - '0';
                    else
                        break;
                    p++;
                }
                *str++ = c;
                break;
            default:
                *str++ = *p;
                break;
            }
            p++;
        }
    }
    *str = 0;
}

char *skipwhite(char *str)
{
    if(str)
        while(*str && isspace(*str))
            str++;
    return str;
}

void stripwhite(char *str)
{
    char   *end = str + strlen(str) - 1;

    while(end >= str && isspace(*end))
        end--;

    end[1] = '\0';
}

char *igets(void)
{
    char   *line;

    if(*PatchPt == '\0')
        return NULL;

    line = PatchPt;

    while(*PatchPt != '\n' && *PatchPt != '\0')
        PatchPt++;

    if(*PatchPt == '\n')
        *PatchPt++ = 0;

    return line;
}

int GetLine(void)
{
    char   *line, *line2;

    do
    {
        while((line = igets()))
            if(line[0] != '#')  // Skip comment lines
                break;

        if(!line)
            return 0;

        Line1 = skipwhite(line);
    } while(Line1 && *Line1 == 0);  // Loop until we get a line with
    // more than just whitespace.
    line = strchr(Line1, '=');

    if(line)
    {                           // We have an '=' in the input line
        line2 = line;
        while(--line2 >= Line1)
            if(*line2 > ' ')
                break;

        if(line2 < Line1)
            return 0;           // Nothing before '='

        *(line2 + 1) = 0;

        line++;
        while(*line && *line <= ' ')
            line++;

        if(*line == 0)
            return 0;           // Nothing after '='

        Line2 = line;

        return 1;
    }
    else
    {                           // No '=' in input line
        line = Line1 + 1;
        while(*line > ' ')
            line++;             // Get beyond first word

        *line++ = 0;
        while(*line && *line <= ' ')
            line++;             // Skip white space

        //.bex files don't have this restriction
        //if (*line == 0)
        //  return 0;           // No second word

        Line2 = line;

        return 2;
    }
}

int HandleMode(const char *mode, int num)
{
    int     i = 0;

    while(Modes[i].name && stricmp(Modes[i].name, mode))
        i++;

    if(Modes[i].name)
        return Modes[i].func(num);

    // Handle unknown or unimplemented data
    LPrintf("Unknown chunk %s encountered. Skipping.\n", mode);
    do
        i = GetLine();
    while(i == 1);

    return i;
}

int PatchThing(int thingy)
{
    size_t          thingNum = (size_t) thingy;

    static const struct Key keys[] = {
        {"ID #", myoffsetof(ded_mobj_t, doomedNum, 0)},
        {"Hit points", myoffsetof(ded_mobj_t, spawnHealth, 0)},
        {"Reaction time", myoffsetof(ded_mobj_t, reactionTime, 0)},
        {"Pain chance", myoffsetof(ded_mobj_t, painChance, 0)},
        {"Speed", myoffsetof(ded_mobj_t, speed, OFF_FLOAT)},
        {"Width", myoffsetof(ded_mobj_t, radius, OFF_FIXED)},
        {"Height", myoffsetof(ded_mobj_t, height, OFF_FIXED)},
        {"Mass", myoffsetof(ded_mobj_t, mass, 0)},
        {"Missile damage", myoffsetof(ded_mobj_t, damage, 0)},
        //{ "Translucency",     myoffsetof(ded_mobj_t,translucency,0) },
        {"Alert sound", myoffsetof(ded_mobj_t, seeSound, OFF_SOUND)},
        {"Attack sound", myoffsetof(ded_mobj_t, attackSound, OFF_SOUND)},
        {"Pain sound", myoffsetof(ded_mobj_t, painSound, OFF_SOUND)},
        {"Death sound", myoffsetof(ded_mobj_t, deathSound, OFF_SOUND)},
        {"Action sound", myoffsetof(ded_mobj_t, activeSound, OFF_SOUND)},
        {NULL,}
    };
    static const struct {
        const char*     label;
        size_t          labelLen;
        statename_t     name;
    } stateNames[] = {
        { "Initial", 7, SN_SPAWN },
        { "First moving", 12, SN_SEE },
        { "Injury", 6, SN_PAIN },
        { "Close attack", 12, SN_MELEE },
        { "Far attack", 10, SN_MISSILE },
        { "Death", 5, SN_DEATH },
        { "Exploding", 9, SN_XDEATH },
        { "Respawn", 7, SN_RAISE },
        { NULL, 0 }
    };
    // Flags can be specified by name (a .bex extension):
    static const struct {
        short           bit;
        short           whichflags;
        const char*     name;
    } bitnames[] =
    {
        {0, 0, "SPECIAL"},
        {1, 0, "SOLID"},
        {2, 0, "SHOOTABLE"},
        {3, 0, "NOSECTOR"},
        {4, 0, "NOBLOCKMAP"},
        {5, 0, "AMBUSH"},
        {6, 0, "JUSTHIT"},
        {7, 0, "JUSTATTACKED"},
        {8, 0, "SPAWNCEILING"},
        {9, 0, "NOGRAVITY"},
        {10, 0, "DROPOFF"},
        {11, 0, "PICKUP"},
        {12, 0, "NOCLIP"},
        {14, 0, "FLOAT"},
        {15, 0, "TELEPORT"},
        {16, 0, "MISSILE"},
        {17, 0, "DROPPED"},
        {18, 0, "SHADOW"},
        {19, 0, "NOBLOOD"},
        {20, 0, "CORPSE"},
        {21, 0, "INFLOAT"},
        {22, 0, "COUNTKILL"},
        {23, 0, "COUNTITEM"},
        {24, 0, "SKULLFLY"},
        {25, 0, "NOTDMATCH"},
        {26, 0, "TRANSLATION1"},
        {26, 0, "TRANSLATION"},  // BOOM compatibility
        {27, 0, "TRANSLATION2"},
        {27, 0, "UNUSED1"},      // BOOM compatibility
        {28, 0, "STEALTH"},
        {28, 0, "UNUSED2"},      // BOOM compatibility
        {29, 0, "TRANSLUC25"},
        {29, 0, "UNUSED3"},      // BOOM compatibility
        {30, 0, "TRANSLUC50"},
        {(29 << 8) | 30, 0, "TRANSLUC75"},
        {30, 0, "UNUSED4"},      // BOOM compatibility
        {30, 0, "TRANSLUCENT"},  // BOOM compatibility?
        {31, 0, "RESERVED"},
        // Names for flags2
        {0, 1, "LOGRAV"},
        {1, 1, "WINDTHRUST"},
        {2, 1, "FLOORBOUNCE"},
        {3, 1, "BLASTED"},
        {4, 1, "FLY"},
        {5, 1, "FLOORCLIP"},
        {6, 1, "SPAWNFLOAT"},
        {7, 1, "NOTELEPORT"},
        {8, 1, "RIP"},
        {9, 1, "PUSHABLE"},
        {10, 1, "CANSLIDE"},     // Avoid conflict with SLIDE from BOOM
        {11, 1, "ONMOBJ"},
        {12, 1, "PASSMOBJ"},
        {13, 1, "CANNOTPUSH"},
        {14, 1, "DROPPED"},
        {15, 1, "BOSS"},
        {16, 1, "FIREDAMAGE"},
        {17, 1, "NODMGTHRUST"},
        {18, 1, "TELESTOMP"},
        {19, 1, "FLOATBOB"},
        {20, 1, "DONTDRAW"},
        {21, 1, "IMPACT"},
        {22, 1, "PUSHWALL"},
        {23, 1, "MCROSS"},
        {24, 1, "PCROSS"},
        {25, 1, "CANTLEAVEFLOORPIC"},
        {26, 1, "NONSHOOTABLE"},
        {27, 1, "INVULNERABLE"},
        {28, 1, "DORMANT"},
        {29, 1, "ICEDAMAGE"},
        {30, 1, "SEEKERMISSILE"},
        {31, 1, "REFLECTIVE"}
    };
    int             result;
    ded_mobj_t*     info, dummy;
    boolean         hadHeight = false;
    boolean         checkHeight = false;

    thingNum--;
    if(thingNum < (unsigned) ded->count.mobjs.num)
    {
        info = ded->mobjs + thingNum;
        if(verbose)
            LPrintf("Thing %lu\n", (unsigned long) thingNum);
    }
    else
    {
        info = &dummy;
        LPrintf("Thing %lu out of range. Create more Thing defs!\n",
                (unsigned long) (thingNum + 1));
    }

    while((result = GetLine()) == 1)
    {
        int             value = atoi(Line2);
        size_t          len = strlen(Line1);
        size_t          sndmap;

        sndmap = value;
        if(sndmap >= sizeof(SoundMap))
            sndmap = 0;

        if(HandleKey(keys, info, Line1, value))
        {
            if(!stricmp(Line1 + len - 6, " frame"))
            {
                uint                i;

                for(i = 0; stateNames[i].label; ++i)
                {
                    if(!strnicmp(stateNames[i].label, Line1,
                                 stateNames[i].labelLen))
                    {
                        strcpy(info->states[stateNames[i].name],
                               ded->states[value].id);
                        break;
                    }
                }
            }
            else if(!stricmp(Line1, "Bits"))
            {
                int             value = 0, value2 = 0;
                boolean         vchanged = false, v2changed = false;
                char           *strval;

                for(strval = Line2; (strval = strtok(strval, ",+| \t\f\r"));
                    strval = NULL)
                {
                    size_t  iy;

                    if(IsNum(strval))
                    {
                        // Force the top 4 bits to 0 so that the user is forced
                        // to use the mnemonics to change them.
                        value |= (atoi(strval) & 0x0fffffff);
                        vchanged = true;
                    }
                    else
                    {
                        for(iy = 0;
                            iy < sizeof(bitnames) / sizeof(bitnames[0]); iy++)
                        {
                            if(!stricmp(strval, bitnames[iy].name))
                            {
                                if(bitnames[iy].whichflags)
                                {
                                    v2changed = true;
                                    if(bitnames[iy].bit & 0xff00)
                                        value2 |= 1 << (bitnames[iy].bit >> 8);
                                    value2 |= 1 << (bitnames[iy].bit & 0xff);
                                }
                                else
                                {
                                    vchanged = true;
                                    if(bitnames[iy].bit & 0xff00)
                                        value |= 1 << (bitnames[iy].bit >> 8);
                                    value |= 1 << (bitnames[iy].bit & 0xff);
                                }

                                break;
                            }
                        }

                        if(iy >= sizeof(bitnames) / sizeof(bitnames[0]))
                            LPrintf("Unknown bit mnemonic %s\n", strval);
                    }
                }

                if(vchanged)
                {
                    info->flags[0] = value;

                    if(value & 0x100) // Spawnceiling?
                        checkHeight = true;

                    // Bit flags are no longer used to specify translucency.
                    // This is just a temporary hack.
                    /*if (info->flags & 0x60000000)
                       info->translucency = (info->flags & 0x60000000) >> 15; */
                }

                if(v2changed)
                    info->flags[1] = value2;

                if(verbose)
                    LPrintf("Bits: %d,%d (0x%08x,0x%08x)\n", value, value2,
                            value, value2);
            }
            else
                LPrintf(unknown_str, Line1, "Thing", thingNum);
        }
        else if(!stricmp(Line1, "Height"))
        {
            hadHeight = true;
        }
    }

    if(checkHeight && !hadHeight && thingNum < sizeof(OrgHeights))
    {
        info->height = OrgHeights[thingNum];
    }

    return result;
}

int PatchSound(int soundNum)
{
/*    static struct Key keys[] = {
        {"Offset", 0}, //pointer to a name string, changed in text
        {"Zero/One", 0},  //info->singularity
        {"Value", myoffsetof(ded_sound_t, priority, 0)}, //info->priority
        {"Zero 1", myoffsetof(ded_sound_t, link, 0)}, //info->link
        {"Zero 2", myoffsetof(ded_sound_t, link_pitch, 0)}, //info->pitch
        {"Zero 3", myoffsetof(ded_sound_t, link_volume, 0)}, //info->volume
        {"Zero 4", 0}, //info->data
        {"Neg. One 1", 0}, //info->usefulness
        {"Neg. One 2", myoffsetof(ded_sound_t, id, OFF_SOUND)}, //info->lumpnum
        {NULL,}
    };
    ded_sound_t *info, dummy;*/
    int     result;

    LPrintf("Sound %d (not supported)\n", soundNum);
    /*
    if (soundNum >= 0 && soundNum < ded->count.sounds.num)
    {
        info = ded->sounds + soundNum
        if(verbose)
            LPrintf("Sound %d\n", soundNum);
    }
    else
    {
        info = &dummy;
        LPrintf ("Sound %d out of range.\n", soundNum);
    }
     */
    while((result = GetLine()) == 1)
    {
        /*if(HandleKey(keys, info, Line1, atoi(Line2)))
            LPrintf(unknown_str, Line1, "Sound", soundNum);*/
    }
    /*
       if (offset) {
       // Calculate offset from start of sound names
       offset -= toff[dversion] + 21076;

       if (offset <= 64)            // pistol .. bfg
       offset >>= 3;
       else if (offset <= 260)      // sawup .. oof
       offset = (offset + 4) >> 3;
       else                     // telept .. skeatk
       offset = (offset + 8) >> 3;

       if (offset >= 0 && offset < NUMSFX) {
       S_sfx[soundNum].name = OrgSfxNames[offset + 1];
       } else {
       Printf ("Sound name %d out of range.\n", offset + 1);
       }
       }
     */
    return result;
}

int PatchFrame(int frameNum)
{
    static struct Key keys[] = {
        {"Sprite number", myoffsetof(ded_state_t, sprite, OFF_SPRITE)},
        {"Sprite subnumber", myoffsetof(ded_state_t, frame, 0)},
        {"Duration", myoffsetof(ded_state_t, tics, 0)},
        {"Next frame", myoffsetof(ded_state_t, nextState, OFF_STATE)},
        {"Unknown 1", 0 /*myoffsetof(ded_state_t,misc[0],0) */ },
        {"Unknown 2", 0 /*myoffsetof(ded_state_t,misc[1],0) */ },
        {NULL,}
    };
    int     result;
    ded_state_t *info, dummy;

    // C doesn't allow non-constant initializers.
    keys[4].offset = myoffsetof(ded_state_t, misc[0], 0);
    keys[5].offset = myoffsetof(ded_state_t, misc[1], 0);

    if(frameNum >= 0 && frameNum < ded->count.states.num)
    {
        info = ded->states + frameNum;
        if(verbose)
            LPrintf("Frame %d\n", frameNum);
    }
    else
    {
        info = &dummy;
        LPrintf("Frame %d out of range (Create more State defs!)\n", frameNum);
    }

    while((result = GetLine()) == 1)
        if(HandleKey(keys, info, Line1, atoi(Line2)))
            LPrintf(unknown_str, Line1, "Frame", frameNum);

    return result;
}

int PatchSprite(int sprNum)
{
    int     result;
    int     offset = 0;

    if(sprNum >= 0 && sprNum < NUMSPRITES)
    {
        if(verbose)
            LPrintf("Sprite %d\n", sprNum);
    }
    else
    {
        LPrintf("Sprite %d out of range. Create more Sprite defs!\n", sprNum);
        sprNum = -1;
    }

    while((result = GetLine()) == 1)
    {
        if(!stricmp("Offset", Line1))
            offset = atoi(Line2);
        else
            LPrintf(unknown_str, Line1, "Sprite", sprNum);
    }

    if(offset > 0 && sprNum != -1)
    {
        // Calculate offset from beginning of sprite names.
        offset = (offset - toff[dversion] - 22044) / 8;

        if(offset >= 0 && offset < ded->count.sprites.num)
        {
            //sprNames[sprNum] = OrgSprNames[offset];
            strcpy(ded->sprites[sprNum].id, OrgSprNames[offset]);
        }
        else
        {
            LPrintf("Sprite name %d out of range.\n", offset);
        }
    }

    return result;
}

/**
 * CRT's realloc can't access other modules' memory, so we must ask
 * Doomsday to reallocate memory for us.
 */
void *DD_Realloc(void *ptr, int newsize)
{
    ded_count_t cnt;

    cnt.max = 0;
    cnt.num = newsize;
    DED_NewEntries(&ptr, &cnt, 1, 0);
    return ptr;
}

void SetValueStr(const char *path, const char *id, char *str)
{
    int     i;
    char    realid[300];

    //CString realid = CString(path) + "|" + id;

    sprintf(realid, "%s|%s", path, id);

    for(i = 0; i < ded->count.values.num; i++)
        if(!stricmp(ded->values[i].id, realid))
        {
            ded->values[i].text =
                (char *) DD_Realloc(ded->values[i].text, strlen(str) + 1);
            strcpy(ded->values[i].text, str);
            return;
        }

    // Not found, create a new Value.
    i = DED_AddValue(ded, realid);
    // Must allocate memory using DD_Realloc.
    ded->values[i].text = 0;
    strcpy(ded->values[i].text =
           DD_Realloc(ded->values[i].text, strlen(str) + 1), str);
}

void SetValueInt(const char *path, const char *id, int val)
{
    char    buf[80];

    sprintf(buf, "%i", val);
    SetValueStr(path, id, buf);
}

int PatchAmmo(int ammoNum)
{
    int     result, max, per;
    char   *ammostr[4] = { "Clip", "Shell", "Cell", "Misl" };
    char   *theAmmo = NULL;

    //  CString str;

    if(ammoNum >= 0 && ammoNum < 4)
    {
        if(verbose)
            LPrintf("Ammo %d.\n", ammoNum);
        theAmmo = ammostr[ammoNum];
    }
    else
        LPrintf("Ammo %d out of range.\n", ammoNum);

    /*  int *max;
       int *per;
       int dummy;

       if (ammoNum >= 0 && ammoNum < NUM_AMMO_TYPES) {
       DPrintf ("Ammo %d.\n", ammoNum);
       max = &maxammo[ammoNum];
       per = &clipammo[ammoNum];
       } else {
       Printf (PRINT_HIGH, "Ammo %d out of range.\n", ammoNum);
       max = per = &dummy;
       }
     */
    while((result = GetLine()) == 1)
    {
        max = per = -1;
        CHECKKEY("Max ammo", max)
        else
        CHECKKEY("Per ammo", per)
        else
        LPrintf(unknown_str, Line1, "Ammo", ammoNum);
        if(!theAmmo)
            continue;

        if(max != -1)
            SetValueInt("Player|Max ammo", theAmmo, max);
        if(per != -1)
            SetValueInt("Player|Clip ammo", theAmmo, per);
    }

    return result;
}

int PatchNothing()
{
    int     result;

    while((result = GetLine()) == 1)
    {
    }

    return result;
}

int PatchWeapon(int weapNum)
{
    //  LPrintf("Weapon patches not supported.\n");
    //  return PatchNothing();

    char   *ammotypes[] =
        { "clip", "shell", "cell", "misl", "-", "noammo", 0 };
    char    buf[80];
    int     val;
    int     result;

    if(weapNum >= 0)
    {
        if(verbose)
            LPrintf("Weapon %d\n", weapNum);
        sprintf(buf, "Weapon Info|%d", weapNum);
    }
    else
    {
        LPrintf("Weapon %d out of range.\n", weapNum);
        return PatchNothing();
    }

    /*  static const struct Key keys[] = {
       { "Ammo type",           myoffsetof(weaponinfo_t,ammo) },
       { "Deselect frame",      myoffsetof(weaponinfo_t,upstate) },
       { "Select frame",        myoffsetof(weaponinfo_t,downstate) },
       { "Bobbing frame",       myoffsetof(weaponinfo_t,readystate) },
       { "Shooting frame",      myoffsetof(weaponinfo_t,atkstate) },
       { "Firing frame",        myoffsetof(weaponinfo_t,flashstate) },
       { NULL, }
       };

       weaponinfo_t *info, dummy;

       if (weapNum >= 0 && weapNum < NUM_WEAPON_TYPES) {
       info = &weaponinfo[weapNum];
       DPrintf ("Weapon %d\n", weapNum);
       } else {
       info = &dummy;
       Printf (PRINT_HIGH, "Weapon %d out of range.\n", weapNum);
       }
     */
    while((result = GetLine()) == 1)
    {
        /*
           if (HandleKey (keys, info, Line1, atoi (Line2)))
           Printf (PRINT_HIGH, unknown_str, Line1, "Weapon", weapNum); */

        //#define CHECKKEY2(a,b)        if (!stricmp (Line1, (a))) { (b) = atoi(Line2);

        val = atoi(Line2);

        if(!stricmp(Line1, "Ammo type"))
            SetValueStr(buf, "Type", ammotypes[val]);
        else if(!stricmp(Line1, "Deselect frame"))
            SetValueStr(buf, "Up", ded->states[val].id);
        else if(!stricmp(Line1, "Select frame"))
            SetValueStr(buf, "Down", ded->states[val].id);
        else if(!stricmp(Line1, "Bobbing frame"))
            SetValueStr(buf, "Ready", ded->states[val].id);
        else if(!stricmp(Line1, "Shooting frame"))
            SetValueStr(buf, "Atk", ded->states[val].id);
        else if(!stricmp(Line1, "Firing frame"))
            SetValueStr(buf, "Flash", ded->states[val].id);
        else
            LPrintf(unknown_str, Line1, "Weapon", weapNum);
    }
    return result;
}

int PatchPointer(int ptrNum)
{
    int     result;

    if(ptrNum >= 0 && ptrNum < 448)
    {
        if(verbose)
            LPrintf("Pointer %d\n", ptrNum);
    }
    else
    {
        LPrintf("Pointer %d out of range.\n", ptrNum);
        ptrNum = -1;
    }

    while((result = GetLine()) == 1)
    {
        if((ptrNum != -1) && (!stricmp(Line1, "Codep Frame")))
        {
            //states[codepconv[ptrNum]].action = OrgActionPtrs[atoi (Line2)];
            strcpy(ded->states[codepconv[ptrNum]].action,
                   OrgActionPtrs[atoi(Line2)]);
        }
        else
            LPrintf(unknown_str, Line1, "Pointer", ptrNum);
    }
    return result;
}

int PatchCheats(int dummy)
{
    /*  static const struct {
       char *name;
       byte *cheatseq;
       BOOL needsval;
       } keys[] = {
       { "Change music",        cheat_mus_seq,               true },
       { "Chainsaw",            cheat_choppers_seq,          false },
       { "God mode",            cheat_god_seq,               false },
       { "Ammo & Keys",     cheat_ammo_seq,              false },
       { "Ammo",                cheat_ammonokey_seq,         false },
       { "No Clipping 1",       cheat_noclip_seq,            false },
       { "No Clipping 2",       cheat_commercial_noclip_seq, false },
       { "Invincibility",       cheat_powerup_seq[0],        false },
       { "Berserk",         cheat_powerup_seq[1],        false },
       { "Invisibility",        cheat_powerup_seq[2],        false },
       { "Radiation Suit",      cheat_powerup_seq[3],        false },
       { "Auto-map",            cheat_powerup_seq[4],        false },
       { "Lite-Amp Goggles",    cheat_powerup_seq[5],        false },
       { "BEHOLD menu",     cheat_powerup_seq[6],        false },
       { "Level Warp",          cheat_clev_seq,              true },
       { "Player Position", cheat_mypos_seq,             false },
       { "Map cheat",           cheat_amap_seq,              false },
       { NULL, }
       };
       int result;

       DPrintf ("Cheats\n");

       while ((result = GetLine ()) == 1) {
       int i = 0;
       while (keys[i].name && stricmp (keys[i].name, Line1))
       i++;

       if (!keys[i].name)
       Printf (PRINT_HIGH, "Unknown cheat %s.\n", Line2);
       else
       ChangeCheat (Line2, keys[i].cheatseq, keys[i].needsval);
       }
       return result; */

    LPrintf("Cheat patches are not supported!\n");
    return PatchNothing();
}

int PatchMisc(int dummy)
{
    /*  static const struct Key keys[] = {
       { "Initial Health",          myoffsetof(struct DehInfo,StartHealth) },
       { "Initial Bullets",     myoffsetof(struct DehInfo,StartBullets) },
       { "Max Health",              myoffsetof(struct DehInfo,MaxHealth) },
       { "Max Armor",               myoffsetof(struct DehInfo,MaxArmor) },
       { "Green Armor Class",       myoffsetof(struct DehInfo,GreenAC) },
       { "Blue Armor Class",        myoffsetof(struct DehInfo,BlueAC) },
       { "Max Soulsphere",          myoffsetof(struct DehInfo,MaxSoulsphere) },
       { "Soulsphere Health",       myoffsetof(struct DehInfo,SoulsphereHealth) },
       { "Megasphere Health",       myoffsetof(struct DehInfo,MegasphereHealth) },
       { "God Mode Health",     myoffsetof(struct DehInfo,GodHealth) },
       { "IDFA Armor",              myoffsetof(struct DehInfo,FAArmor) },
       { "IDFA Armor Class",        myoffsetof(struct DehInfo,FAAC) },
       { "IDKFA Armor",         myoffsetof(struct DehInfo,KFAArmor) },
       { "IDKFA Armor Class",       myoffsetof(struct DehInfo,KFAAC) },
       { "BFG Cells/Shot",          myoffsetof(struct DehInfo,BFGCells) },
       { "Monsters Infight",        myoffsetof(struct DehInfo,Infight) },
       { NULL, }
       };
       gitem_t *item;
     */
    int     result;
    int     val;

    if(verbose)
        LPrintf("Misc\n");

    while((result = GetLine()) == 1)
    {
        val = atoi(Line2);

        if(!stricmp(Line1, "Initial Health"))
            SetValueInt("Player|Health", "Type", val);

        else if(!stricmp(Line1, "Initial Bullets"))
            SetValueInt("Player|Init ammo", "Clip", val);

        else if(!stricmp(Line1, "Max Health"))
            SetValueInt("Player|Health Limit", "Type", val);

        else if(!stricmp(Line1, "Max Armor"))
            SetValueInt("Player|Blue Armor", "Type", val);

        else if(!stricmp(Line1, "Green Armor Class"))
            SetValueInt("Player|Green Armor Class", "Type", val);

        else if(!stricmp(Line1, "Blue Armor Class"))
            SetValueInt("Player|Blue Armor Class", "Type", val);

        else if(!stricmp(Line1, "Max Soulsphere"))
            SetValueInt("SoulSphere|Give", "Health Limit", val);

        else if(!stricmp(Line1, "Soulsphere Health"))
            SetValueInt("SoulSphere|Give", "Health", val);

        else if(!stricmp(Line1, "Megasphere Health"))
            SetValueInt("MegaSphere|Give", "Health", val);

        else if(!stricmp(Line1, "God Mode Health"))
            SetValueInt("Player|God Health", "Type", val);

        else if(!stricmp(Line1, "IDFA Armor"))
            SetValueInt("Player|IDFA Armor", "Type", val);

        else if(!stricmp(Line1, "IDFA Armor Class"))
            SetValueInt("Player|IDFA Armor Class", "Type", val);

        else if(!stricmp(Line1, "IDKFA Armor"))
            SetValueInt("Player|IDKFA Armor", "Type", val);

        else if(!stricmp(Line1, "IDKFA Armor Class"))
            SetValueInt("Player|IDKFA Armor Class", "Type", val);

        else if(!stricmp(Line1, "BFG Cells/Shot"))
            SetValueInt("Weapon Info|6", "Per shot", val);

        else if(!stricmp(Line1, "Monsters Infight"))
            SetValueInt("AI", "Infight", val);
        else
            LPrintf("Unknown miscellaneous info %s = %s.\n", Line1, Line2);
    }

    return result;
}

int PatchPars(int dummy)
{
    char   *space, mapname[8], *moredata;
    ded_mapinfo_t *info;
    int     result, par, i;

    if(verbose)
        LPrintf("[Pars]\n");

    while((result = GetLine()))
    {
        // Argh! .bex doesn't follow the same rules as .deh
        if(result == 1)
        {
            LPrintf("Unknown key in [PARS] section: %s\n", Line1);
            continue;
        }
        if(stricmp("par", Line1))
            return result;

        space = strchr(Line2, ' ');

        if(!space)
        {
            LPrintf("Need data after par.\n");
            continue;
        }

        *space++ = '\0';

        while(*space && isspace(*space))
            space++;

        moredata = strchr(space, ' ');

        if(moredata)
        {
            // At least 3 items on this line, must be E?M? format
            sprintf(mapname, "E%cM%c", *Line2, *space);
            par = atoi(moredata + 1);
        }
        else
        {
            // Only 2 items, must be MAP?? format
            sprintf(mapname, "MAP%02d", atoi(Line2) % 100);
            par = atoi(space);
        }

        info = NULL;
        /*if (!(info = FindLevelInfo (mapname)) ) {
           Printf (PRINT_HIGH, "No map %s\n", mapname);
           continue;
           } */
        for(i = 0; i < ded->count.mapInfo.num; i++)
            if(!stricmp(ded->mapInfo[i].id, mapname))
            {
                info = ded->mapInfo + i;
                break;
            }

        if(info)
            info->parTime = (float) par;

        LPrintf("Par for %s changed to %d\n", mapname, par);
    }
    return result;
}

int PatchCodePtrs(int dummy)
{
    LPrintf("[CodePtr] patches not supported\n");
    return PatchNothing();

    /*  int result;

       LPrintf ("[CodePtr]\n");

       while ((result = GetLine()) == 1)
       {
       if (!strnicmp ("Frame", Line1, 5) && isspace(Line1[5]))
       {
       int frame = atoi (Line1 + 5);

       if (frame < 0 || frame >= NUMSTATES)
       {
       Printf (PRINT_HIGH, "Frame %d out of range\n", frame);
       }
       else
       {
       int i = 0;
       char *data;

       COM_Parse (Line2);

       if ((com_token[0] == 'A' || com_token[0] == 'a') && com_token[1] == '_')
       data = com_token + 2;
       else
       data = com_token;

       while (CodePtrs[i].name && stricmp (CodePtrs[i].name, data))
       i++;

       if (CodePtrs[i].name) {
       states[frame].action.acp1 = CodePtrs[i].func.acp1;
       DPrintf ("Frame %d set to %s\n", frame, CodePtrs[i].name);
       } else {
       states[frame].action.acp1 = NULL;
       DPrintf ("Unknown code pointer: %s\n", com_token);
       }
       }
       }
       }
       return result; */
}

void ReplaceInString(char *str, char *occurance, char *replacewith)
{
    char   *buf = calloc(strlen(str) * 2, 1);
    char   *out = buf, *in = str;
    int     oclen = strlen(occurance), replen = strlen(replacewith);

    for(; *in; in++)
    {
        if(!strncmp(in, occurance, oclen))
        {
            strcat(out, replacewith);
            out += replen;
            in += oclen - 1;
        }
        else
            *out++ = *in;
    }
    strcpy(str, buf);
    free(buf);
}

int PatchText(int oldSize)
{
    char    oldString[4096];
    int     newSize;
    char   *oldStr;
    char   *newStr;
    char   *temp;
    boolean good;
    int     result;
    int     i;
    char    buf[30];

    temp = COM_Parse(Line2);    // Skip old size, since we already have it
    if(!COM_Parse(temp))
    {
        LPrintf("Text chunk is missing size of new string.\n");
        return 2;
    }
    newSize = atoi(com_token);

    oldStr = malloc(oldSize + 1);
    newStr = malloc(newSize + 1);

    if(!oldStr || !newStr)
    {
        LPrintf("Out of memory.\n");
        goto donewithtext;
    }

    good = ReadChars(&oldStr, oldSize, false);
    good += ReadChars(&newStr, newSize, true);

    if(!good)
    {
        free(newStr);
        free(oldStr);
        LPrintf("Unexpected end-of-file.\n");
        return 0;
    }

    if(includenotext)
    {
        LPrintf("Skipping text chunk in included patch.\n");
        goto donewithtext;
    }

    if(verbose)
    {
        LPrintf("Searching for text:\n%s\n", oldStr);
        LPrintf("<< TO BE REPLACED WITH:\n%s\n>>\n", newStr);
    }
    good = false;

    // Search through sprite names
    for(i = 0; i < ded->count.sprites.num; i++)
    {
        if(!strcmp(ded->sprites[i].id, oldStr))
        {
            strcpy(ded->sprites[i].id, newStr);
            good = true;
            // See above.
        }
    }

    if(good)
        goto donewithtext;

#if 0
    // Search through music names.
    // This is something of an even bigger hack
    // since I changed the way music is handled.
    if(oldSize < 7)
    {                           // Music names are never >6 chars (sure they are -jk)
        if((temp = new char[oldSize + 3]))
        {
            level_info_t *info = LevelInfos;

            sprintf(temp, "d_%s", oldStr);

            while(info->level_name)
            {
                if(!strnicmp(info->music, temp, 8))
                {
                    good = true;
                    strncpy(info->music, temp, 8);
                }
                info++;
            }

            delete[]temp;
        }
    }

    if(good)
        goto donewithtext;
#endif

    // Search music names.
    if(oldSize <= 6)
    {
        sprintf(buf, "d_%s", oldStr);
        for(i = 0; i < ded->count.music.num; i++)
        {
            if(!stricmp(ded->music[i].lumpName, buf))
            {
                good = true;
                sprintf(ded->music[i].lumpName, "D_%s", newStr);
                strupr(ded->music[i].lumpName); // looks nicer...
            }
        }
    }
    if(good)
        goto donewithtext;

    // Seach map names
    for(i = 0; i < ded->count.mapInfo.num; i++)
        if(!stricmp(ded->mapInfo[i].name, oldStr))
        {
            strcpy(ded->mapInfo[i].name, newStr);
            good = true;
        }
    if(good)
        goto donewithtext;

    strcpy(oldString, oldStr);
    ReplaceInString(oldString, "\n", "\\n");
    // Search through most other texts
    for(i = 0; i < ded->count.text.num; i++)
    {
        //if(!stricmp(Strings[i].builtin, oldStr))
        if(!stricmp(ded->text[i].text, oldString))
        {
            //CString s = newStr;
            char    s[4096];
            int     ts;

            strcpy(s, newStr);
            //s.Replace("\n", "\\n");
            ReplaceInString(s, "\n", "\\n");
            ts = strlen(s);

            ded->text[i].text =
                (char *) DD_Realloc(ded->text[i].text, strlen(s) + 1);
            strcpy(ded->text[i].text, s);
            good = true;
            break;
        }
    }

    if(verbose && !good)
        LPrintf("   (Unmatched)\n");

  donewithtext:
    if(newStr)
        free(newStr);
    if(oldStr)
        free(oldStr);

    // Fetch next identifier for main loop
    while((result = GetLine()) == 1);

    return result;
}

int PatchStrings(int dummy)
{
    LPrintf("[Strings] patches not supported\n");
    return PatchNothing();

    /*  static size_t maxstrlen = 128;
       static char *holdstring;
       int result;

       DPrintf ("[Strings]\n");

       if (!holdstring)
       holdstring = Malloc (maxstrlen);

       while ((result = GetLine()) == 1) {
       int i;

       *holdstring = '\0';
       do {
       while (maxstrlen < strlen (holdstring) + strlen (Line2)) {
       maxstrlen += 128;
       holdstring = (char *)Realloc (holdstring, maxstrlen);
       }
       strcat (holdstring, skipwhite (Line2));
       stripwhite (holdstring);
       if (holdstring[strlen(holdstring)-1] == '\\') {
       holdstring[strlen(holdstring)-1] = '\0';
       Line2 = igets ();
       } else
       Line2 = NULL;
       } while (Line2 && *Line2);

       for (i = 0; i < NUMSTRINGS; i++)
       if (!stricmp (Strings[i].name, Line1))
       break;

       if (i == NUMSTRINGS) {
       Printf (PRINT_HIGH, "Unknown string: %s\n", Line1);
       } else {
       ReplaceSpecialChars (holdstring);
       ReplaceString (&Strings[i].string, copystring (holdstring));
       Strings[i].type = str_patched;
       Printf (PRINT_HIGH, "%s set to:\n%s\n", Line1, holdstring);
       }
       }
       return result;
     */
}

int DoInclude(int dummy)
{
    char   *data;
    int     savedversion, savepversion;
    char   *savepatchfile, *savepatchpt;
    int     len;
    FILE   *file;
    char   *patch;

    if(including)
    {
        LPrintf("Sorry, can't nest includes\n");
        goto endinclude;
    }

    data = COM_Parse(Line2);
    if(!stricmp(com_token, "notext"))
    {
        includenotext = true;
        data = COM_Parse(data);
    }

    if(!com_token[0])
    {
        includenotext = false;
        LPrintf("Include directive is missing filename\n");
        goto endinclude;
    }

    if(verbose)
        LPrintf("Including %s\n", com_token);
    savepatchfile = PatchFile;
    savepatchpt = PatchPt;
    savedversion = dversion;
    savepversion = pversion;
    including = true;

    file = fopen(com_token, "rt");
    if(!file)
    {
        LPrintf("Can't include %s, it can't be found.\n", com_token);
        goto endinclude;
    }
    fseek(file, 0, SEEK_END);
    len = ftell(file);
    patch = calloc(len + 1, 1);
    rewind(file);
    fread(patch, len, 1, file);
    patch[len] = 0;
    fclose(file);
    ApplyDEH(patch, len);
    free(patch);

    if(verbose)
        LPrintf("Done with include\n");
    PatchFile = savepatchfile;
    PatchPt = savepatchpt;
    dversion = savedversion;
    pversion = savepversion;

  endinclude:
    including = false;
    includenotext = false;
    return GetLine();
}

void ApplyDEH(char *patch, int length)
{
    int     cont;

    BackupData();
    PatchFile = patch;
    dversion = pversion = -1;

    cont = 0;
    if(!strncmp(PatchFile, "Patch File for DeHackEd v", 25))
    {
        PatchPt = strchr(PatchFile, '\n');
        while((cont = GetLine()) == 1)
        {
            CHECKKEY("Doom version", dversion)
            else
        CHECKKEY("Patch format", pversion)}
        if(!cont || dversion == -1 || pversion == -1)
        {
            Con_Message("This is not a DeHackEd patch file!");
            return;
        }
    }
    else
    {
        LPrintf("Patch does not have DeHackEd signature. Assuming .bex\n");
        dversion = 19;
        pversion = 6;
        PatchPt = PatchFile;
        while((cont = GetLine()) == 1);
    }

    if(pversion != 6)
    {
        LPrintf
            ("DeHackEd patch version is %d.\nUnexpected results may occur.\n",
             pversion);
    }

    if(dversion == 16)
        dversion = 0;
    else if(dversion == 17)
        dversion = 2;
    else if(dversion == 19)
        dversion = 3;
    else if(dversion == 20)
        dversion = 1;
    else if(dversion == 21)
        dversion = 4;
    else
    {
        LPrintf
            ("Patch created with unknown DOOM version.\nAssuming version 1.9.\n");
        dversion = 3;
    }

    do
    {
        if(cont == 1)
        {
            LPrintf("Key %s encountered out of context\n", Line1);
            cont = 0;
        }
        else if(cont == 2)
            cont = HandleMode(Line1, atoi(Line2));
    } while(cont);

}

/**
 * Reads and applies the given lump as a DEH patch.
 */
void ReadDehackedLump(int lumpnum)
{
    size_t      len;
    byte       *lump;

    Con_Message("Applying Dehacked: lump %i...\n", lumpnum);
    len = W_LumpLength(lumpnum);
    lump = calloc(len + 1, 1);
    memcpy(lump, W_CacheLumpNum(lumpnum, PU_CACHE), len);
    ApplyDEH((char*)lump, len);
    free(lump);
}

/**
 * Reads and applies the given Dehacked patch file.
 */
void ReadDehacked(char *filename)
{
    FILE   *file;
    char   *deh;
    int     len;

    Con_Message("Applying Dehacked: %s...\n", filename);

    // Read in the patch.
    if((file = fopen(filename, "rt")) == NULL)
        return;                 // Shouldn't happen.
    // How long is it?
    fseek(file, 0, SEEK_END);
    len = ftell(file) + 1;
    // Allocate enough memory and read it.
    deh = calloc(len + 1, 1);
    rewind(file);
    fread(deh, len, 1, file);
    fclose(file);
    // Process it!
    ApplyDEH(deh, len);
    free(deh);
}

/**
 * This will be called after the engine has loaded all definitions but
 * before the data they contain has been initialized.
 */
int DefsHook(int hook_type, int parm, void *data)
{
    int                 i;

    verbose = ArgExists("-verbose");
    ded = (ded_t *) data;

    // Check for a DEHACKED lumps.
    for(i = DD_GetInteger(DD_NUMLUMPS) - 1; i >= 0; i--)
        if(!strnicmp(W_LumpName(i), "DEHACKED", 8))
        {
            ReadDehackedLump(i);
            // We'll only continue this if the -alldehs option is given.
            if(!ArgCheck("-alldehs"))
                break;
        }

    // How about the -deh option?
    if(ArgCheckWith("-deh", 1))
    {
        char                temp[256];
        const char*         fn;

        // Aha! At least one DEH specified. Let's read all of 'em.
        while((fn = ArgNext()) != NULL && fn[0] != '-')
        {
            M_TranslatePath(fn, temp);
            if(!M_FileExists(temp))
                continue;

            ReadDehacked(temp);
        }
    }
    return true;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_DEFS, DefsHook);
}

#ifdef WIN32
/**
 * Windows calls this when the DLL is loaded.
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        // Register our hooks.
        DP_Initialize();
        break;
    }
    return TRUE;
}
#endif
