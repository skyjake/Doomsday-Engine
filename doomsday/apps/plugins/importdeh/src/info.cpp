/** @file info.cpp  Information look up tables.
 * @ingroup dehread
 *
 * @author Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2013-2014 Daniel Swanson <danij@dengine.net>
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

#include "importdeh.h"
#include "info.h"

static FinaleBackgroundMapping const finaleBGMappings[] = {
    { "FLOOR4_8",   "BGFLATE1" }, ///< DOOM end of episode 1
    { "SFLR6_1",    "BGFLATE2" }, ///< DOOM end of episode 2
    { "MFLR8_4",    "BGFLATE3" }, ///< DOOM end of episode 3
    { "MFLR8_3",    "BGFLATE4" }, ///< DOOM end of episode 4
    { "SLIME16",    "BGFLAT06" }, ///< DOOM2 before MAP06
    { "RROCK14",    "BGFLAT11" }, ///< DOOM2 before MAP11
    { "RROCK07",    "BGFLAT20" }, ///< DOOM2 before MAP20
    { "RROCK17",    "BGFLAT30" }, ///< DOOM2 before MAP30
    { "RROCK13",    "BGFLAT15" }, ///< DOOM2 from MAP15 to MAP31
    { "RROCK19",    "BGFLAT31" }, ///< DOOM2 from MAP31 to MAP32
    { "BOSSBACK",   "BGCASTCALL" }, ///< End of game cast call
    { "",           ""}
};

int findFinaleBackgroundMappingForText(QString const &text, FinaleBackgroundMapping const **mapping)
{
    if(!text.isEmpty())
    for(int i = 0; !finaleBGMappings[i].text.isEmpty(); ++i)
    {
        if(!finaleBGMappings[i].text.compare(text, Qt::CaseInsensitive))
        {
            if(mapping) *mapping = &finaleBGMappings[i];
            return i;
        }
    }
    return -1; // Not found.
}

static FlagMapping mobjtypeFlagMappings[] = {
// Group #0:
    {  0, 0, "SPECIAL"      },
    {  1, 0, "SOLID"        },
    {  2, 0, "SHOOTABLE"    },
    {  3, 0, "NOSECTOR"     },
    {  4, 0, "NOBLOCKMAP"   },
    {  5, 0, "AMBUSH"       },
    {  6, 0, "JUSTHIT"      },
    {  7, 0, "JUSTATTACKED" },
    {  8, 0, "SPAWNCEILING" },
    {  9, 0, "NOGRAVITY"    },
    { 10, 0, "DROPOFF"      },
    { 11, 0, "PICKUP"       },
    { 12, 0, "NOCLIP"       },
    { 14, 0, "FLOAT"        },
    { 15, 0, "TELEPORT"     },
    { 16, 0, "MISSILE"      },
    { 17, 0, "DROPPED"      },
    { 18, 0, "SHADOW"       },
    { 19, 0, "NOBLOOD"      },
    { 20, 0, "CORPSE"       },
    { 21, 0, "INFLOAT"      },
    { 22, 0, "COUNTKILL"    },
    { 23, 0, "COUNTITEM"    },
    { 24, 0, "SKULLFLY"     },
    { 25, 0, "NOTDMATCH"    },
    { 26, 0, "TRANSLATION1" },
    { 26, 0, "TRANSLATION"  }, ///< BOOM compatibility.
    { 27, 0, "TRANSLATION2" },
    { 27, 0, "UNUSED1"      }, ///< BOOM compatibility.
    { 28, 0, "STEALTH"      },
    { 28, 0, "UNUSED2"      }, ///< BOOM compatibility.
    { 29, 0, "TRANSLUC25"   },
    { 29, 0, "UNUSED3"      }, ///< BOOM compatibility.
    { 30, 0, "TRANSLUC50"   },
    { (29 << 8) | 30, 0, "TRANSLUC75" },
    { 30, 0, "UNUSED4"      }, ///< BOOM compatibility.
    { 30, 0, "TRANSLUCENT"  }, ///< BOOM compatibility?
    { 31, 0, "RESERVED"     },

// Group #1:
    {  0, 1, "LOGRAV"       },
    {  1, 1, "WINDTHRUST"   },
    {  2, 1, "FLOORBOUNCE"  },
    {  3, 1, "BLASTED"      },
    {  4, 1, "FLY"          },
    {  5, 1, "FLOORCLIP"    },
    {  6, 1, "SPAWNFLOAT"   },
    {  7, 1, "NOTELEPORT"   },
    {  8, 1, "RIP"          },
    {  9, 1, "PUSHABLE"     },
    { 10, 1, "CANSLIDE"     }, ///< Avoid conflict with SLIDE from BOOM.
    { 11, 1, "ONMOBJ"       },
    { 12, 1, "PASSMOBJ"     },
    { 13, 1, "CANNOTPUSH"   },
    { 14, 1, "DROPPED"      },
    { 15, 1, "BOSS"         },
    { 16, 1, "FIREDAMAGE"   },
    { 17, 1, "NODMGTHRUST"  },
    { 18, 1, "TELESTOMP"    },
    { 19, 1, "FLOATBOB"     },
    { 20, 1, "DONTDRAW"     },
    { 21, 1, "IMPACT"       },
    { 22, 1, "PUSHWALL"     },
    { 23, 1, "MCROSS"       },
    { 24, 1, "PCROSS"       },
    { 25, 1, "CANTLEAVEFLOORPIC" },
    { 26, 1, "NONSHOOTABLE" },
    { 27, 1, "INVULNERABLE" },
    { 28, 1, "DORMANT"      },
    { 29, 1, "ICEDAMAGE"    },
    { 30, 1, "SEEKERMISSILE" },
    { 31, 1, "REFLECTIVE"   },

    { 0, -1, "" } // terminator
};

int findMobjTypeFlagMappingByDehLabel(QString const &name, FlagMapping const **mapping)
{
    /// @todo Optimize - replace linear search.
    if(!name.isEmpty())
    for(int i = 0; !mobjtypeFlagMappings[i].dehLabel.isEmpty(); ++i)
    {
        if(!mobjtypeFlagMappings[i].dehLabel.compare(name, Qt::CaseInsensitive))
        {
            if(mapping) *mapping = &mobjtypeFlagMappings[i];
            return i;
        }
    }
    return -1; // Not found.
}

static QString const MusicMap[] = {
    "e1m1",
    "e1m2",
    "e1m3",
    "e1m4",
    "e1m5",
    "e1m6",
    "e1m7",
    "e1m8",
    "e1m9",
    "e2m1",
    "e2m2",
    "e2m3",
    "e2m4",
    "e2m5",
    "e2m6",
    "e2m7",
    "e2m8",
    "e2m9",
    "e3m1",
    "e3m2",
    "e3m3",
    "e3m4",
    "e3m5",
    "e3m6",
    "e3m7",
    "e3m8",
    "e3m9",
    "inter",
    "intro",
    "bunny",
    "victor",
    "introa",
    "runnin",
    "stalks",
    "countd",
    "betwee",
    "doom",
    "the_da",
    "shawn",
    "ddtblu",
    "in_cit",
    "dead",
    "stlks2",
    "theda2",
    "doom2",
    "ddtbl2",
    "runni2",
    "dead2",
    "stlks3",
    "romero",
    "shawn2",
    "messag",
    "count2",
    "ddtbl3",
    "ampie",
    "theda3",
    "adrian",
    "messg2",
    "romer2",
    "tense",
    "shawn3",
    "openin",
    "evil",
    "ultima",
    "read_m",
    "dm2ttl",
    "dm2int",
    "" // Terminate.
};

int findMusicLumpNameInMap(QString const &name)
{
    /// @todo Optimize - replace linear search.
    if(!name.isEmpty())
    for(int i = 0; !MusicMap[i].isEmpty(); ++i)
    {
        if(!MusicMap[i].compare(name, Qt::CaseInsensitive))
            return i;
    }
    return -1; // Not found.
}

#if 0
static QString const SpriteMap[] = {
    "TROO",
    "SHTG",
    "PUNG",
    "PISG",
    "PISF",
    "SHTF",
    "SHT2",
    "CHGG",
    "CHGF",
    "MISG",
    "MISF",
    "SAWG",
    "PLSG",
    "PLSF",
    "BFGG",
    "BFGF",
    "BLUD",
    "PUFF",
    "BAL1",
    "BAL2",
    "PLSS",
    "PLSE",
    "MISL",
    "BFS1",
    "BFE1",
    "BFE2",
    "TFOG",
    "IFOG",
    "PLAY",
    "POSS",
    "SPOS",
    "VILE",
    "FIRE",
    "FATB",
    "FBXP",
    "SKEL",
    "MANF",
    "FATT",
    "CPOS",
    "SARG",
    "HEAD",
    "BAL7",
    "BOSS",
    "BOS2",
    "SKUL",
    "SPID",
    "BSPI",
    "APLS",
    "APBX",
    "CYBR",
    "PAIN",
    "SSWV",
    "KEEN",
    "BBRN",
    "BOSF",
    "ARM1",
    "ARM2",
    "BAR1",
    "BEXP",
    "FCAN",
    "BON1",
    "BON2",
    "BKEY",
    "RKEY",
    "YKEY",
    "BSKU",
    "RSKU",
    "YSKU",
    "STIM",
    "MEDI",
    "SOUL",
    "PINV",
    "PSTR",
    "PINS",
    "MEGA",
    "SUIT",
    "PMAP",
    "PVIS",
    "CLIP",
    "AMMO",
    "ROCK",
    "BROK",
    "CELL",
    "CELP",
    "SHEL",
    "SBOX",
    "BPAK",
    "BFUG",
    "MGUN",
    "CSAW",
    "LAUN",
    "PLAS",
    "SHOT",
    "SGN2",
    "COLU",
    "SMT2",
    "GOR1",
    "POL2",
    "POL5",
    "POL4",
    "POL3",
    "POL1",
    "POL6",
    "GOR2",
    "GOR3",
    "GOR4",
    "GOR5",
    "SMIT",
    "COL1",
    "COL2",
    "COL3",
    "COL4",
    "CAND",
    "CBRA",
    "COL6",
    "TRE1",
    "TRE2",
    "ELEC",
    "CEYE",
    "FSKU",
    "COL5",
    "TBLU",
    "TGRN",
    "TRED",
    "SMBT",
    "SMGT",
    "SMRT",
    "HDB1",
    "HDB2",
    "HDB3",
    "HDB4",
    "HDB5",
    "HDB6",
    "POB1",
    "POB2",
    "BRS1",
    "TLMP",
    "TLP2",
    "" // Terminate.
};

int findSpriteNameInMap(QString const &name)
{
    /// @todo Optimize - replace linear search.
    if(!name.isEmpty())
    for(int i = 0; !SpriteMap[i].isEmpty(); ++i)
    {
        if(!SpriteMap[i].compare(name, Qt::CaseInsensitive))
            return i;
    }
    return -1; // Not found.
}
#endif

static QString const SoundMap[] = {
    "None",
    "pistol",
    "shotgn",
    "sgcock",
    "dshtgn",
    "dbopn",
    "dbcls",
    "dbload",
    "plasma",
    "bfg",
    "sawup",
    "sawidl",
    "sawful",
    "sawhit",
    "rlaunc",
    "rxplod",
    "firsht",
    "firxpl",
    "pstart",
    "pstop",
    "doropn",
    "dorcls",
    "stnmov",
    "swtchn",
    "swtchx",
    "plpain",
    "dmpain",
    "popain",
    "vipain",
    "mnpain",
    "pepain",
    "slop",
    "itemup",
    "wpnup",
    "oof",
    "telept",
    "posit1",
    "posit2",
    "posit3",
    "bgsit1",
    "bgsit2",
    "sgtsit",
    "cacsit",
    "brssit",
    "cybsit",
    "spisit",
    "bspsit",
    "kntsit",
    "vilsit",
    "mansit",
    "pesit",
    "sklatk",
    "sgtatk",
    "skepch",
    "vilatk",
    "claw",
    "skeswg",
    "pldeth",
    "pdiehi",
    "podth1",
    "podth2",
    "podth3",
    "bgdth1",
    "bgdth2",
    "sgtdth",
    "cacdth",
    "skldth",
    "brsdth",
    "cybdth",
    "spidth",
    "bspdth",
    "vildth",
    "kntdth",
    "pedth",
    "skedth",
    "posact",
    "bgact",
    "dmact",
    "bspact",
    "bspwlk",
    "vilact",
    "noway",
    "barexp",
    "punch",
    "hoof",
    "metal",
    "chgun",
    "tink",
    "bdopn",
    "bdcls",
    "itmbk",
    "flame",
    "flamst",
    "getpow",
    "bospit",
    "boscub",
    "bossit",
    "bospn",
    "bosdth",
    "manatk",
    "mandth",
    "sssit",
    "ssdth",
    "keenpn",
    "keendt",
    "skeact",
    "skesit",
    "skeatk",
    "radio",
    NULL
};

int findSoundLumpNameInMap(QString const &name)
{
    /// @todo Optimize - replace linear search.
    if(!name.isEmpty())
    for(int i = 0; !SoundMap[i].isEmpty(); ++i)
    {
        if(!SoundMap[i].compare(name, Qt::CaseInsensitive))
            return i;
    }
    return -1; // Not found.
}

static SoundMapping soundMappings[] = {
    { "Alert",    SDN_SEE,        "See" },
    { "Attack",   SDN_ATTACK,     "Attack" },
    { "Pain",     SDN_PAIN,       "Pain" },
    { "Death",    SDN_DEATH,      "Death" },
    { "Action",   SDN_ACTIVE,     "Active" },
    { "",         soundname_t(-1), "" }
};

int findSoundMappingByDehLabel(QString const &dehLabel, SoundMapping const **mapping)
{
    if(!dehLabel.isEmpty())
    for(int i = 0; !soundMappings[i].dehLabel.isEmpty(); ++i)
    {
        if(!soundMappings[i].dehLabel.compare(dehLabel, Qt::CaseInsensitive))
        {
            if(mapping) *mapping = &soundMappings[i];
            return i;
        }
    }
    return -1; // Not found.
}

static StateMapping stateMappings[] = {
    { "Initial",      SN_SPAWN,         "Spawn"   },
    { "First moving", SN_SEE,           "See"     },
    { "Injury",       SN_PAIN,          "Pain"    },
    { "Close attack", SN_MELEE,         "Melee"   },
    { "Far attack",   SN_MISSILE,       "Missile" },
    { "Death",        SN_DEATH,         "Death"   },
    { "Exploding",    SN_XDEATH,        "XDeath"  },
    { "Respawn",      SN_RAISE,         "Raise"   },
    { "",             statename_t(-1),  ""        }
};

int findStateMappingByDehLabel(QString const &dehLabel, StateMapping const **mapping)
{
    if(!dehLabel.isEmpty())
    for(int i = 0; !stateMappings[i].dehLabel.isEmpty(); ++i)
    {
        if(!stateMappings[i].dehLabel.compare(dehLabel, Qt::CaseInsensitive))
        {
            if(mapping) *mapping = &stateMappings[i];
            return i;
        }
    }
    return -1; // Not found.
}

static WeaponStateMapping weaponStateMappings[] = {
    { "Deselect",   WSN_UP,                 "Up"     },
    { "Select",     WSN_DOWN,               "Down"   },
    { "Bobbing",    WSN_READY,              "Ready"  },
    { "Shooting",   WSN_ATTACK,             "Atk"    },
    { "Firing",     WSN_FLASH,              "Flash"  },
    { "",           weaponstatename_t(-1),  ""       }
};

int findWeaponStateMappingByDehLabel(QString const &dehLabel, WeaponStateMapping const **mapping)
{
    if(!dehLabel.isEmpty())
    for(int i = 0; !weaponStateMappings[i].dehLabel.isEmpty(); ++i)
    {
        if(!weaponStateMappings[i].dehLabel.compare(dehLabel, Qt::CaseInsensitive))
        {
            if(mapping) *mapping = &weaponStateMappings[i];
            return i;
        }
    }
    return -1; // Not found.
}

static TextMapping const TextMap[] = {
    /**
     * Disallowed replacements:
     * Mainly UI texts and potentially dangerous format strings.
     */
    { "",                         "DOOM System Startup v%i.%i" },
    { "",                         "The Ultimate DOOM Startup v%i.%i" },
    { "",                         "DOOM 2: Hell on Earth v%i.%i" },
    { ""/*"D_DEVSTR"*/,           "Development mode ON.\n" },
    { ""/*"D_CDROM"*/,            "CD-ROM Version: default.cfg from c:\\doomdata" },
    { ""/*"LOADNET"*/,            "you can't do load while in a net game!\n\npress a key." },
    { ""/*"SAVEDEAD"*/,           "you can't save if you aren't playing!\n\npress a key." },
    { ""/*"QSPROMPT"*/,           "quicksave over your game named\n\n'%s'?\n\npress y or n." },
    { ""/*"QLOADNET"*/,           "you can't quickload during a netgame!\n\npress a key." },
    { ""/*"QSAVESPOT"*/,          "you haven't picked a quicksave slot yet!\n\npress a key." },
    { ""/*"QLPROMPT"*/,           "do you want to quickload the game named\n\n'%s'?\n\npress y or n." },
    { ""/*"NEWGAME"*/,            "you can't start a new game\nwhile in a network game.\n\npress a key." },
    { ""/*"NIGHTMARE"*/,          "are you sure? this skill level\nisn't even remotely fair.\n\npress y or n." },
    { ""/*"SWSTRING"*/,           "this is the shareware version of doom.\n\nyou need to order the entire trilogy.\n\npress a key." },
    { ""/*"MSGOFF"*/,             "Messages OFF" },
    { ""/*"MSGON"*/,              "Messages ON" },
    { ""/*"NETEND"*/,             "you can't end a netgame!\n\npress a key." },
    { ""/*"ENDGAME"*/,            "are you sure you want to end the game?\n\npress y or n." },
    { ""/*"DOSY"*/,               "%s\n\n(press y to quit to dos.)" },
    { ""/*"DETAILHI"*/,           "High detail" },
    { ""/*"DETAILLO"*/,           "Low detail" },
    { ""/*"AMSTR_FOLLOWON"*/,     "Follow Mode ON" },
    { ""/*"AMSTR_FOLLOWOFF"*/,    "Follow Mode OFF" },
    { ""/*"AMSTR_GRIDON"*/,       "Grid ON" },
    { ""/*"AMSTR_GRIDOFF"*/,      "Grid OFF" },
    { ""/*"AMSTR_MARKEDSPOT"*/,   "Marked Spot" },
    { ""/*"AMSTR_MARKSCLEARED"*/, "All Marks Cleared" },
    { ""/*"EMPTYSTRING"*/,        "empty slot" },
    { ""/*"GGSAVED"*/,            "game saved." },
    { "",                         "===========================================================================\nATTENTION:  This version of DOOM has been modified.  If you would like to\nget a copy of the original game, call 1-800-IDGAMES or see the readme file.\n        You will not receive technical support for modified games.\n                      press enter to continue\n===========================================================================" },
    { "",                         "===========================================================================\n             This version is NOT SHAREWARE, do not distribute!\n         Please report software piracy to the SPA: 1-800-388-PIR8\n===========================================================================" },
    { "",                         "===========================================================================\n                            Do not distribute!\n         Please report software piracy to the SPA: 1-800-388-PIR8\n===========================================================================" },
    { "",                         "I_AllocLow: DOS alloc of %i failed, %i free" },
    { "",                         "DPMI memory: 0x%x" },
    { "",                         ", 0x%x allocated for zone" },
    { "",                         "Insufficient memory!  You need to have at least 3.7 megabytes of total" },
    { "",                         "free memory available for DOOM to execute.  Reconfigure your CONFIG.SYS" },
    { "",                         "or AUTOEXEC.BAT to load fewer device drivers or TSR's.  We recommend" },
    { "",                         "creating a custom boot menu item in your CONFIG.SYS for optimum DOOMing." },
    { "",                         "Please consult your DOS manual (\"Making more memory available\") for" },
    { "",                         "information on how to free up more memory for DOOM." },
    { "",                         "DOOM aborted." },
    { "",                         "malloc() in I_InitNetwork() failed" },
    { "",                         "I_NetCmd when not in netgame" },
    { "",                         "I_StartupTimer()" },
    { "",                         "Can't register 35 Hz timer w/ DMX library" },
    { "",                         "Dude.  The ENSONIQ ain't responding." },
    { "",                         "CODEC p=0x%x, d=%d" },
    { "",                         "CODEC.  The CODEC ain't responding." },
    { "",                         "Dude.  The GUS ain't responding." },
    { "",                         "SB isn't responding at p=0x%x, i=%d, d=%d" },
    { "",                         "SB_Detect returned p=0x%x,i=%d,d=%d" },
    { "",                         "Dude.  The Adlib isn't responding." },
    { "",                         "The MPU-401 isn't reponding @ p=0x%x." },
    { "",                         "I_StartupSound: Hope you hear a pop." },
    { "",                         "  Music device #%d & dmxCode=%d" },
    { "",                         "  Sfx device #%d & dmxCode=%d" },
    { "",                         "  calling DMX_Init" },
    { "",                         "  DMX_Init() returned %d" },
    { "",                         "CyberMan: Wrong mouse driver - no SWIFT support (AX=%04x)." },
    { "",                         "CyberMan: no SWIFT device connected." },
    { "",                         "CyberMan: SWIFT device is not a CyberMan! (type=%d)" },
    { "",                         "CyberMan: CyberMan %d.%02d connected." },
    { "",                         "Austin Virtual Gaming: Levels will end after 20 minutes" },
    { "",                         "V_Init: allocate screens." },
    { "",                         "M_LoadDefaults: Load system defaults." },
    { "",                         "Z_Init: Init zone memory allocation daemon. " },
    { "",                         "W_Init: Init WADfiles." },
    { "",                         "You cannot -file with the shareware version. Register!" },
    { "",                         "This is not the registered version." },
    { "",                         "registered version." },
    { "",                         "shareware version." },
    { "",                         "commercial version." },
    { "",                         "M_Init: Init miscellaneous info." },
    { "",                         "R_Init: Init DOOM refresh daemon -" },
    { "",                         "P_Init: Init Playloop state." },
    { "",                         "I_Init: Setting up machine state." },
    { "",                         "D_CheckNetGame: Checking network game status." },
    { "",                         "S_Init: Setting up sound." },
    { "",                         "HU_Init: Setting up heads up display." },
    { "",                         "ST_Init: Init status bar." },
    { "",                         "P_Init: Checking cmd-line parameters..." },
    { "",                         "doom1.wad" },
    { "",                         "doom2f.wad" },
    { "",                         "doom2.wad" },
    { "",                         "doom.wad" },
    { "",                         "French version" },
    { "",                         "Game mode indeterminate." },
    { "",                         "Doomcom buffer invalid!" },
    { "",                         "c:\\doomdata\\doomsav%c.dsg" },
    { "",                         "c:\\doomdata\\doomsav%d.dsg" },
    { "",                         "doomsav%c.dsg" },
    { "",                         "doomsav%d.dsg" },
    { "",                         "Savegame buffer overrun" },
    { "",                         "DOOM00.pcx" },
    { "",                         "c:/localid/doom1.wad" },
    { "",                         "f:/doom/data_se/data_se/texture1.lmp" },
    { "",                         "f:/doom/data_se/data_se/pnames.lmp" },
    { "",                         "c:/localid/default.cfg" },
    { "",                         "c:/localid/doom.wad" },
    { "",                         "f:/doom/data_se/data_se/texture1.lmp" },
    { "",                         "f:/doom/data_se/data_se/texture2.lmp" },
    { "",                         "f:/doom/data_se/data_se/pnames.lmp" },
    { "",                         "c:/localid/doom2.wad" },
    { "",                         "f:/doom/data_se/cdata/texture1.lmp" },
    { "",                         "f:/doom/data_se/cdata/pnames.lmp" },
    { "",                         "c:\\doomdata" },
    { "",                         "c:/doomdata/default.cfg" },
    { "",                         "~f:/doom/data_se/cdata/map0%i.wad" },
    { "",                         "~f:/doom/data_se/cdata/map%i.wad" },
    { "",                         "~f:/doom/data_se/E%cM%c.wad" },
    { "",                         "e:/doom/data/texture1.lmp" },
    { "",                         "e:/doom/data/pnames.lmp" },
    { "",                         "e:/doom/data/texture2.lmp" },
    { "",                         "e:/doom/cdata/texture1.lmp" },
    { "",                         "e:/doom/cdata/pnames.lmp" },
    { "",                         "~e:/doom/cdata/map0%i.wad" },
    { "",                         "~e:/doom/cdata/map%i.wad" },
    { "",                         "~e:/doom/E%cM%c.wad" },
    { "",                         "_" },
    { "",                         "timed %i gametics in %i realtics" },
    { "",                         "Z_CT at g_game.c:%i" },
    { "",                         "Demo %s recorded" },
    { "",                         "Demo is from a different game version!" },
    { "",                         "version %i" },
    { "",                         "Bad savegame" },
    { "",                         "consistency failure (%i should be %i)" },
    { "",                         "External statistics registered." },
    { "",                         "ExpandTics: strange value %i at maketic %i" },
    { "",                         "Tried to transmit to another node" },
    { "",                         "send (%i + %i, R %i) [%i]" },
    { "",                         "%i" },
    { "",                         "bad packet length %i" },
    { "",                         "bad packet checksum" },
    { "",                         "setup packet" },
    { "",                         "get %i = (%i + %i, R %i)[%i]" },
    { "",                         "Killed by network driver" },
    { "",                         "retransmit from %i" },
    { "",                         "out of order packet (%i + %i)" },
    { "",                         "missed tics from %i (%i - %i)" },
    { "",                         "NetUpdate: netbuffer->numtics > BACKUPTICS" },
    { "",                         "Network game synchronization aborted." },
    { "",                         "listening for network start info..." },
    { "",                         "Different DOOM versions cannot play a net game!" },
    { "",                         "sending network start info..." },
    { "",                         "startskill %i  deathmatch: %i  startmap: %i  startepisode: %i" },
    { "",                         "player %i of %i (%i nodes)" },
    { "",                         "=======real: %i  avail: %i  game: %i" },
    { "",                         "TryRunTics: lowtic < gametic" },
    { "",                         "gametic>lowtic" },
    { "",                         "Couldn't read file %s" },

    // Supported replacements:
    { "E1TEXT", "Once you beat the big badasses and\nclean out the moon base you're supposed\nto win, aren't you? Aren't you? Where's\nyour fat reward and ticket home? What\nthe hell is this? It's not supposed to\nend this way!\n\nIt stinks like rotten meat, but looks\nlike the lost Deimos base.  Looks like\nyou're stuck on The Shores of Hell.\nThe only way out is through.\n\nTo continue the DOOM experience, play\nThe Shores of Hell and its amazing\nsequel, Inferno!\n" },
    { "E2TEXT", "You've done it! The hideous cyber-\ndemon lord that ruled the lost Deimos\nmoon base has been slain and you\nare triumphant! But ... where are\nyou? You clamber to the edge of the\nmoon and look down to see the awful\ntruth.\n\nDeimos floats above Hell itself!\nYou've never heard of anyone escaping\nfrom Hell, but you'll make the bastards\nsorry they ever heard of you! Quickly,\nyou rappel down to  the surface of\nHell.\n\nNow, it's on to the final chapter of\nDOOM! -- Inferno." },
    { "E3TEXT", "The loathsome spiderdemon that\nmasterminded the invasion of the moon\nbases and caused so much death has had\nits ass kicked for all time.\n\nA hidden doorway opens and you enter.\nYou've proven too tough for Hell to\ncontain, and now Hell at last plays\nfair -- for you emerge from the door\nto see the green fields of Earth!\nHome at last.\n\nYou wonder what's been happening on\nEarth while you were battling evil\nunleashed. It's good that no Hell-\nspawn could have come through that\ndoor with you ..." },
    { "E4TEXT", "the spider mastermind must have sent forth\nits legions of hellspawn before your\nfinal confrontation with that terrible\nbeast from hell.  but you stepped forward\nand brought forth eternal damnation and\nsuffering upon the horde as a true hero\nwould in the face of something so evil.\n\nbesides, someone was gonna pay for what\nhappened to daisy, your pet rabbit.\n\nbut now, you see spread before you more\npotential pain and gibbitude as a nation\nof demons run amok among our cities.\n\nnext stop, hell on earth!" },
    { "C1TEXT", "YOU HAVE ENTERED DEEPLY INTO THE INFESTED\nSTARPORT. BUT SOMETHING IS WRONG. THE\nMONSTERS HAVE BROUGHT THEIR OWN REALITY\nWITH THEM, AND THE STARPORT'S TECHNOLOGY\nIS BEING SUBVERTED BY THEIR PRESENCE.\n\nAHEAD, YOU SEE AN OUTPOST OF HELL, A\nFORTIFIED ZONE. IF YOU CAN GET PAST IT,\nYOU CAN PENETRATE INTO THE HAUNTED HEART\nOF THE STARBASE AND FIND THE CONTROLLING\nSWITCH WHICH HOLDS EARTH'S POPULATION\nHOSTAGE." },
    { "C2TEXT", "YOU HAVE WON! YOUR VICTORY HAS ENABLED\nHUMANKIND TO EVACUATE EARTH AND ESCAPE\nTHE NIGHTMARE.  NOW YOU ARE THE ONLY\nHUMAN LEFT ON THE FACE OF THE PLANET.\nCANNIBAL MUTATIONS, CARNIVOROUS ALIENS,\nAND EVIL SPIRITS ARE YOUR ONLY NEIGHBORS.\nYOU SIT BACK AND WAIT FOR DEATH, CONTENT\nTHAT YOU HAVE SAVED YOUR SPECIES.\n\nBUT THEN, EARTH CONTROL BEAMS DOWN A\nMESSAGE FROM SPACE: \"SENSORS HAVE LOCATED\nTHE SOURCE OF THE ALIEN INVASION. IF YOU\nGO THERE, YOU MAY BE ABLE TO BLOCK THEIR\nENTRY.  THE ALIEN BASE IS IN THE HEART OF\nYOUR OWN HOME CITY, NOT FAR FROM THE\nSTARPORT.\" SLOWLY AND PAINFULLY YOU GET\nUP AND RETURN TO THE FRAY." },
    { "C3TEXT", "YOU ARE AT THE CORRUPT HEART OF THE CITY,\nSURROUNDED BY THE CORPSES OF YOUR ENEMIES.\nYOU SEE NO WAY TO DESTROY THE CREATURES'\nENTRYWAY ON THIS SIDE, SO YOU CLENCH YOUR\nTEETH AND PLUNGE THROUGH IT.\n\nTHERE MUST BE A WAY TO CLOSE IT ON THE\nOTHER SIDE. WHAT DO YOU CARE IF YOU'VE\nGOT TO GO THROUGH HELL TO GET TO IT?" },
    { "C4TEXT", "THE HORRENDOUS VISAGE OF THE BIGGEST\nDEMON YOU'VE EVER SEEN CRUMBLES BEFORE\nYOU, AFTER YOU PUMP YOUR ROCKETS INTO\nHIS EXPOSED BRAIN. THE MONSTER SHRIVELS\nUP AND DIES, ITS THRASHING LIMBS\nDEVASTATING UNTOLD MILES OF HELL'S\nSURFACE.\n\nYOU'VE DONE IT. THE INVASION IS OVER.\nEARTH IS SAVED. HELL IS A WRECK. YOU\nWONDER WHERE BAD FOLKS WILL GO WHEN THEY\nDIE, NOW. WIPING THE SWEAT FROM YOUR\nFOREHEAD YOU BEGIN THE LONG TREK BACK\nHOME. REBUILDING EARTH OUGHT TO BE A\nLOT MORE FUN THAN RUINING IT WAS." },
    { "C5TEXT", "CONGRATULATIONS, YOU'VE FOUND THE SECRET\nLEVEL! LOOKS LIKE IT'S BEEN BUILT BY\nHUMANS, RATHER THAN DEMONS. YOU WONDER\nWHO THE INMATES OF THIS CORNER OF HELL\nWILL BE." },
    { "C6TEXT", "CONGRATULATIONS, YOU'VE FOUND THE\nSUPER SECRET LEVEL!  YOU'D BETTER\nBLAZE THROUGH THIS ONE!" },
    { "P1TEXT", "You gloat over the steaming carcass of the\nGuardian.  With its death, you've wrested\nthe Accelerator from the stinking claws\nof Hell.  You relax and glance around the\nroom.  Damn!  There was supposed to be at\nleast one working prototype, but you can't\nsee it. The demons must have taken it.\n\nYou must find the prototype, or all your\nstruggles will have been wasted. Keep\nmoving, keep fighting, keep killing.\nOh yes, keep living, too." },
    { "P2TEXT", "Even the deadly Arch-Vile labyrinth could\nnot stop you, and you've gotten to the\nprototype Accelerator which is soon\nefficiently and permanently deactivated.\n\nYou're good at that kind of thing." },
    { "P3TEXT", "You've bashed and battered your way into\nthe heart of the devil-hive.  Time for a\nSearch-and-Destroy mission, aimed at the\nGatekeeper, whose foul offspring is\ncascading to Earth.  Yeah, he's bad. But\nyou know who's worse!\n\nGrinning evilly, you check your gear, and\nget ready to give the bastard a little Hell\nof your own making!" },
    { "P4TEXT", "The Gatekeeper's evil face is splattered\nall over the place.  As its tattered corpse\ncollapses, an inverted Gate forms and\nsucks down the shards of the last\nprototype Accelerator, not to mention the\nfew remaining demons.  You're done. Hell\nhas gone back to pounding bad dead folks \ninstead of good live ones.  Remember to\ntell your grandkids to put a rocket\nlauncher in your coffin. If you go to Hell\nwhen you die, you'll need it for some\nfinal cleaning-up ..." },
    { "P5TEXT", "You've found the second-hardest level we\ngot. Hope you have a saved game a level or\ntwo previous.  If not, be prepared to die\naplenty. For master marines only." },
    { "P6TEXT", "Betcha wondered just what WAS the hardest\nlevel we had ready for ya?  Now you know.\nNo one gets out alive." },
    { "T1TEXT", "You've fought your way out of the infested\nexperimental labs.   It seems that UAC has\nonce again gulped it down.  With their\nhigh turnover, it must be hard for poor\nold UAC to buy corporate health insurance\nnowadays..\n\nAhead lies the military complex, now\nswarming with diseased horrors hot to get\ntheir teeth into you. With luck, the\ncomplex still has some warlike ordnance\nlaying around." },
    { "T2TEXT", "You hear the grinding of heavy machinery\nahead.  You sure hope they're not stamping\nout new hellspawn, but you're ready to\nream out a whole herd if you have to.\nThey might be planning a blood feast, but\nyou feel about as mean as two thousand\nmaniacs packed into one mad killer.\n\nYou don't plan to go down easy." },
    { "T3TEXT", "The vista opening ahead looks real damn\nfamiliar. Smells familiar, too -- like\nfried excrement. You didn't like this\nplace before, and you sure as hell ain't\nplanning to like it now. The more you\nbrood on it, the madder you get.\nHefting your gun, an evil grin trickles\nonto your face. Time to take some names." },
    { "T4TEXT", "Suddenly, all is silent, from one horizon\nto the other. The agonizing echo of Hell\nfades away, the nightmare sky turns to\nblue, the heaps of monster corpses start \nto evaporate along with the evil stench \nthat filled the air. Jeeze, maybe you've\ndone it. Have you really won?\n\nSomething rumbles in the distance.\nA blue light begins to glow inside the\nruined skull of the demon-spitter." },
    { "T5TEXT", "What now? Looks totally different. Kind\nof like King Tut's condo. Well,\nwhatever's here can't be any worse\nthan usual. Can it?  Or maybe it's best\nto let sleeping gods lie.." },
    { "T6TEXT", "Time for a vacation. You've burst the\nbowels of hell and by golly you're ready\nfor a break. You mutter to yourself,\nMaybe someone else can kick Hell's ass\nnext time around. Ahead lies a quiet town,\nwith peaceful flowing water, quaint\nbuildings, and presumably no Hellspawn.\n\nAs you step off the transport, you hear\nthe stomp of a cyberdemon's iron shoe." },
    { "CC_ZOMBIE", "ZOMBIEMAN" },
    { "CC_SHOTGUN", "SHOTGUN GUY" },
    { "CC_HEAVY", "HEAVY WEAPON DUDE" },
    { "CC_IMP", "IMP" },
    { "CC_DEMON", "DEMON" },
    { "CC_LOST", "LOST SOUL" },
    { "CC_CACO", "CACODEMON" },
    { "CC_HELL", "HELL KNIGHT" },
    { "CC_BARON", "BARON OF HELL" },
    { "CC_ARACH", "ARACHNOTRON" },
    { "CC_PAIN", "PAIN ELEMENTAL" },
    { "CC_REVEN", "REVENANT" },
    { "CC_MANCU", "MANCUBUS" },
    { "CC_ARCH", "ARCH-VILE" },
    { "CC_SPIDER", "THE SPIDER MASTERMIND" },
    { "CC_CYBER", "THE CYBERDEMON" },
    { "CC_HERO", "OUR HERO" },
    { "HUSTR_CHATMACRO0", "No" },
    { "HUSTR_CHATMACRO1", "I'm ready to kick butt!" },
    { "HUSTR_CHATMACRO2", "I'm OK." },
    { "HUSTR_CHATMACRO3", "I'm not looking too good!" },
    { "HUSTR_CHATMACRO4", "Help!" },
    { "HUSTR_CHATMACRO5", "You suck!" },
    { "HUSTR_CHATMACRO6", "Next time, scumbag..." },
    { "HUSTR_CHATMACRO7", "Come here!" },
    { "HUSTR_CHATMACRO8", "I'll take care of it." },
    { "HUSTR_CHATMACRO9", "Yes" },
    { "PD_BLUEO", "You need a blue key to activate this object" },
    { "PD_REDO", "You need a red key to activate this object" },
    { "PD_YELLOWO", "You need a yellow key to activate this object" },
    { "PD_BLUEK", "You need a blue key to open this door" },
    { "PD_REDK", "You need a yellow key to open this door" },
    { "PD_YELLOWK", "You need a red key to open this door" },
    { "GOTARMOR", "Picked up the armor." },
    { "GOTMEGA", "Picked up the MegaArmor!" },
    { "GOTHTHBONUS", "Picked up a health bonus." },
    { "GOTARMBONUS", "Picked up an armor bonus." },
    { "GOTSUPER", "Supercharge!" },
    { "GOTMSPHERE", "MegaSphere!" },
    { "GOTBLUECARD", "Picked up a blue keycard." },
    { "GOTYELWCARD", "Picked up a yellow keycard." },
    { "GOTREDCARD", "Picked up a red keycard." },
    { "GOTBLUESKUL", "Picked up a blue skull key." },
    { "GOTYELWSKUL", "Picked up a yellow skull key." },
    { "GOTREDSKULL", "Picked up a red skull key." },
    { "GOTSTIM", "Picked up a stimpack." },
    { "GOTMEDINEED", "Picked up a medikit that you REALLY need!" },
    { "GOTMEDIKIT", "Picked up a medikit." },
    { "GOTINVUL", "Invulnerability!" },
    { "GOTBERSERK", "Berserk!" },
    { "GOTINVIS", "Partial Invisibility" },
    { "GOTSUIT", "Radiation Shielding Suit" },
    { "GOTMAP", "Computer Area Map" },
    { "GOTVISOR", "Light Amplification Visor" },
    { "GOTCLIP", "Picked up a clip." },
    { "GOTCLIPBOX", "Picked up a box of bullets." },
    { "GOTROCKET", "Picked up a rocket." },
    { "GOTROCKBOX", "Picked up a box of rockets." },
    { "GOTCELL", "Picked up an energy cell." },
    { "GOTCELLBOX", "Picked up an energy cell pack." },
    { "GOTSHELLS", "Picked up 4 shotgun shells." },
    { "GOTSHELLBOX", "Picked up a box of shotgun shells." },
    { "GOTBACKPACK", "Picked up a backpack full of ammo!" },
    { "GOTBFG9000", "You got the BFG9000!  Oh, yes." },
    { "GOTCHAINGUN", "You got the chaingun!" },
    { "GOTCHAINSAW", "A chainsaw!  Find some meat!" },
    { "GOTLAUNCHER", "You got the rocket launcher!" },
    { "GOTPLASMA", "You got the plasma gun!" },
    { "GOTSHOTGUN", "You got the shotgun!" },
    { "GOTSHOTGUN2", "You got the super shotgun!" },
    { "STSTR_DQDON", "Degreelessness Mode On" },
    { "STSTR_DQDOFF", "Degreelessness Mode Off" },
    { "STSTR_FAADDED", "Ammo (no keys) Added" },
    { "STSTR_KFAADDED", "Very Happy Ammo Added" },
    { "STSTR_MUS", "Music Change" },
    { "STSTR_NOMUS", "IMPOSSIBLE SELECTION" },
    { "STSTR_NCON", "No Clipping Mode ON" },
    { "STSTR_NCOFF", "No Clipping Mode OFF" },
    { "STSTR_BEHOLDX", "Power-up Toggled" },
    { "STSTR_BEHOLD", "inVuln, Str, Inviso, Rad, Allmap, or Lite-amp" },
    { "STSTR_CHOPPERS", "... doesn't suck - GM" },
    { "STSTR_CLEV", "Changing Level..." },
    { "HUSTR_PLRGREEN", "Green: " },
    { "HUSTR_PLRINDIGO", "Indigo: " },
    { "HUSTR_PLRBROWN", "Brown: " },
    { "HUSTR_PLRRED", "Red: " },
    { "HUSTR_MSGU", "[Message unsent]" },
    { "HUSTR_TALKTOSELF1", "You mumble to yourself" },
    { "HUSTR_TALKTOSELF2", "Who's there?" },
    { "HUSTR_TALKTOSELF3", "You scare yourself" },
    { "HUSTR_TALKTOSELF4", "You start to rave" },
    { "HUSTR_TALKTOSELF5", "You've lost it..." },
    { "HUSTR_E1M1", "E1M1: Hangar" },
    { "HUSTR_E1M2", "E1M2: Nuclear Plant" },
    { "HUSTR_E1M3", "E1M3: Toxin Refinery" },
    { "HUSTR_E1M4", "E1M4: Command Control" },
    { "HUSTR_E1M5", "E1M5: Phobos Lab" },
    { "HUSTR_E1M6", "E1M6: Central Processing" },
    { "HUSTR_E1M7", "E1M7: Computer Station" },
    { "HUSTR_E1M8", "E1M8: Phobos Anomaly" },
    { "HUSTR_E1M9", "E1M9: Military Base" },
    { "HUSTR_E2M1", "E2M1: Deimos Anomaly" },
    { "HUSTR_E2M2", "E2M2: Containment Area" },
    { "HUSTR_E2M3", "E2M3: Refinery" },
    { "HUSTR_E2M4", "E2M4: Deimos Lab" },
    { "HUSTR_E2M5", "E2M5: Command Center" },
    { "HUSTR_E2M6", "E2M6: Halls of the Damned" },
    { "HUSTR_E2M7", "E2M7: Spawning Vats" },
    { "HUSTR_E2M8", "E2M8: Tower of Babel" },
    { "HUSTR_E2M9", "E2M9: Fortress of Mystery" },
    { "HUSTR_E3M1", "E3M1: Hell Keep" },
    { "HUSTR_E3M2", "E3M2: Slough of Despair" },
    { "HUSTR_E3M3", "E3M3: Pandemonium" },
    { "HUSTR_E3M4", "E3M4: House of Pain" },
    { "HUSTR_E3M5", "E3M5: Unholy Cathedral" },
    { "HUSTR_E3M6", "E3M6: Mt. Erebus" },
    { "HUSTR_E3M7", "E3M7: Limbo" },
    { "HUSTR_E3M8", "E3M8: Dis" },
    { "HUSTR_E3M9", "E3M9: Warrens" },
    { "HUSTR_E4M1", "E4M1: Hell Beneath" },
    { "HUSTR_E4M2", "E4M2: Perfect Hatred" },
    { "HUSTR_E4M3", "E4M3: Sever The Wicked" },
    { "HUSTR_E4M4", "E4M4: Unruly Evil" },
    { "HUSTR_E4M5", "E4M5: They Will Repent" },
    { "HUSTR_E4M6", "E4M6: Against Thee Wickedly" },
    { "HUSTR_E4M7", "E4M7: And Hell Followed" },
    { "HUSTR_E4M8", "E4M8: Unto The Cruel" },
    { "HUSTR_E4M9", "E4M9: Fear" },
    { "HUSTR_1", "level 1: entryway" },
    { "HUSTR_2", "level 2: underhalls" },
    { "HUSTR_3", "level 3: the gantlet" },
    { "HUSTR_4", "level 4: the focus" },
    { "HUSTR_5", "level 5: the waste tunnels" },
    { "HUSTR_6", "level 6: the crusher" },
    { "HUSTR_7", "level 7: dead simple" },
    { "HUSTR_8", "level 8: tricks and traps" },
    { "HUSTR_9", "level 9: the pit" },
    { "HUSTR_10", "level 10: refueling base" },
    { "HUSTR_11", "level 11: 'o' of destruction!" },
    { "HUSTR_12", "level 12: the factory" },
    { "HUSTR_13", "level 13: downtown" },
    { "HUSTR_14", "level 14: the inmost dens" },
    { "HUSTR_15", "level 15: industrial zone" },
    { "HUSTR_16", "level 16: suburbs" },
    { "HUSTR_17", "level 17: tenements" },
    { "HUSTR_18", "level 18: the courtyard" },
    { "HUSTR_19", "level 19: the citadel" },
    { "HUSTR_20", "level 20: gotcha!" },
    { "HUSTR_21", "level 21: nirvana" },
    { "HUSTR_22", "level 22: the catacombs" },
    { "HUSTR_23", "level 23: barrels o' fun" },
    { "HUSTR_24", "level 24: the chasm" },
    { "HUSTR_25", "level 25: bloodfalls" },
    { "HUSTR_26", "level 26: the abandoned mines" },
    { "HUSTR_27", "level 27: monster condo" },
    { "HUSTR_28", "level 28: the spirit world" },
    { "HUSTR_29", "level 29: the living end" },
    { "HUSTR_30", "level 30: icon of sin" },
    { "HUSTR_31", "level 31: wolfenstein" },
    { "HUSTR_32", "level 32: grosse" },
    { "PHUSTR_1", "level 1: congo" },
    { "PHUSTR_2", "level 2: well of souls" },
    { "PHUSTR_3", "level 3: aztec" },
    { "PHUSTR_4", "level 4: caged" },
    { "PHUSTR_5", "level 5: ghost town" },
    { "PHUSTR_6", "level 6: baron's lair" },
    { "PHUSTR_7", "level 7: caughtyard" },
    { "PHUSTR_8", "level 8: realm" },
    { "PHUSTR_9", "level 9: abattoire" },
    { "PHUSTR_10", "level 10: onslaught" },
    { "PHUSTR_11", "level 11: hunted" },
    { "PHUSTR_12", "level 12: speed" },
    { "PHUSTR_13", "level 13: the crypt" },
    { "PHUSTR_14", "level 14: genesis" },
    { "PHUSTR_15", "level 15: the twilight" },
    { "PHUSTR_16", "level 16: the omen" },
    { "PHUSTR_17", "level 17: compound" },
    { "PHUSTR_18", "level 18: neurosphere" },
    { "PHUSTR_19", "level 19: nme" },
    { "PHUSTR_20", "level 20: the death domain" },
    { "PHUSTR_21", "level 21: slayer" },
    { "PHUSTR_22", "level 22: impossible mission" },
    { "PHUSTR_23", "level 23: tombstone" },
    { "PHUSTR_24", "level 24: the final frontier" },
    { "PHUSTR_25", "level 25: the temple of darkness" },
    { "PHUSTR_26", "level 26: bunker" },
    { "PHUSTR_27", "level 27: anti-christ" },
    { "PHUSTR_28", "level 28: the sewers" },
    { "PHUSTR_29", "level 29: odyssey of noises" },
    { "PHUSTR_30", "level 30: the gateway of hell" },
    { "PHUSTR_31", "level 31: cyberden" },
    { "PHUSTR_32", "level 32: go 2 it" },
    { "THUSTR_1", "Level 1: System Control" },
    { "THUSTR_2", "Level 2: Human BBQ" },
    { "THUSTR_3", "Level 3: Power Control" },
    { "THUSTR_4", "Level 4: Wormhole" },
    { "THUSTR_5", "Level 5: Hanger" },
    { "THUSTR_6", "Level 6: Open Season" },
    { "THUSTR_7", "Level 7: Prison" },
    { "THUSTR_8", "Level 8: Metal" },
    { "THUSTR_9", "Level 9: Stronghold" },
    { "THUSTR_10", "Level 10: Redemption" },
    { "THUSTR_11", "Level 11: Storage Facility" },
    { "THUSTR_12", "Level 12: Crater" },
    { "THUSTR_13", "Level 13: Nukage Processing" },
    { "THUSTR_14", "Level 14: Steel Works" },
    { "THUSTR_15", "Level 15: Dead Zone" },
    { "THUSTR_16", "Level 16: Deepest Reaches" },
    { "THUSTR_17", "Level 17: Processing Area" },
    { "THUSTR_18", "Level 18: Mill" },
    { "THUSTR_19", "Level 19: Shipping/Respawning" },
    { "THUSTR_20", "Level 20: Central Processing" },
    { "THUSTR_21", "Level 21: Administration Center" },
    { "THUSTR_22", "Level 22: Habitat" },
    { "THUSTR_23", "Level 23: Lunar Mining Project" },
    { "THUSTR_24", "Level 24: Quarry" },
    { "THUSTR_25", "Level 25: Baron's Den" },
    { "THUSTR_26", "Level 26: Ballistyx" },
    { "THUSTR_27", "Level 27: Mount Pain" },
    { "THUSTR_28", "Level 28: Heck" },
    { "THUSTR_29", "Level 29: River Styx" },
    { "THUSTR_30", "Level 30: Last Call" },
    { "THUSTR_31", "Level 31: Pharaoh" },
    { "THUSTR_32", "Level 32: Caribbean" },
    { "",          "" } // Terminate.
};

int textMappingForBlob(QString const &origText, TextMapping const **mapping)
{
    /// @todo Optimize - replace linear search and hash the text blobs.
    if(!origText.isEmpty())
    for(int i = 0; !TextMap[i].text.isEmpty(); ++i)
    {
        if(!TextMap[i].text.compare(origText, Qt::CaseInsensitive))
        {
            if(mapping) *mapping = &TextMap[i];
            return i;
        }
    }
    return -1; // Not found.
}

// A conversion array to convert from the 448 code pointers to the 966 States
// that exist in the original game. From the DeHackEd source.
static short codepConv[448] = { 1, 2, 3, 4, 6, 9, 10, 11, 12, 14,
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

int stateIndexForActionOffset(int offset)
{
    if(offset < 0 || offset >= 448) return -1;
    return codepConv[offset];
}

static ValueMapping const valueMappings[] = {
    { "Initial Health",     "Player|Health" },
    { "Initial Bullets",    "Player|Init Ammo|Clip" },
    { "Max Health",         "Player|Health Limit" },
    { "Max Armor",          "Player|Blue Armor", },
    { "Green Armor Class",  "Player|Green Armor Class" },
    { "Blue Armor Class",   "Player|Blue Armor Class" },
    { "Max Soulsphere",     "SoulSphere|Give|Health Limit" },
    { "Soulsphere Health",  "SoulSphere|Give|Health" },
    { "Megasphere Health",  "MegaSphere|Give|Health" },
    { "God Mode Health",    "Player|God Health" },
    { "IDFA Armor",         "Player|IDFA Armor" },
    { "IDFA Armor Class",   "Player|IDFA Armor Class" },
    { "IDKFA Armor",        "Player|IDKFA Armor" },
    { "IDKFA Armor Class",  "Player|IDKFA Armor Class" },
    { "BFG Cells/Shot",     "Weapon Info|6|Per shot" },
    { "Monsters Infight",   "AI|Infight" },
    { "", "" }
};

int findValueMappingForDehLabel(QString const &dehLabel, ValueMapping const **mapping)
{
    /// @todo Optimize - replace linear search.
    if(!dehLabel.isEmpty())
    for(int i = 0; !valueMappings[i].dehLabel.isEmpty(); ++i)
    {
        if(!valueMappings[i].dehLabel.compare(dehLabel, Qt::CaseInsensitive))
        {
            if(mapping) *mapping = &valueMappings[i];
            return i;
        }
    }
    return -1; // Not found.
}

static byte origMobjHeights[] = {
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
static int origMobjHeightCount = sizeof(origMobjHeights)/sizeof(origMobjHeights[0]);

int originalHeightForMobjType(int type)
{
    if(type < 0 || type >= origMobjHeightCount) return -1;
    return origMobjHeights[type];
}

#if 0
/**
 * Here follows the remainder of the text strings in DOOM2.EXE which can
 * be changed by patching with DeHackED 3.0
 *
 * @todo There are a few interesting strings amongst this lot which I am
 * pretty sure have been used in some of the older mods such as Aliens TC.
 */
"CODEC: Testing I/O address: %04x\n"
"CODEC: Starting the \"busy\" test.\n"
"CODEC: The \"busy\" test failed.\n"
"CODEC: Passed the \"busy\" test.\n"
"CODEC: Starting the version test.\n"
"CODEC: Failed the version inspection.\n"
"CODEC: Passed the version inspection.\n"
"CODEC: Starting the misc. write test.\n"
"CODEC: Failed the misc. write test.\n"
"CODEC: Passed all I/O port inspections.\n"
"="
"SNDSCAPE"
"\\SNDSCAPE.INI"
"r"
"Port"
"DMA"
"IRQ"
"WavePort"
" out of GUS RAM:"
"%d%c"
"GF1PATCH110"
"110"
",%d"
"Ultrasound @ Port:%03Xh, IRQ:%d,  DMA:%d\n"
"Sizing GUS RAM:"
"BANK%d:OK "
"BANK%d:N/A "
"\n"
"ULTRADIR"
"\\midi\\"
"\r\n"
"Loading GUS Patches\n"
".pat"
"%s"
" - FAILED\n"
" - OK\n"
"ULTRASND"
"NO MVSOUND.SYS\n"
"PAS IRQ: %d  DMA:%d\n"
"Pro Audio Spectrum is NOT version 1.01.\n"
"ECHO Personal Sound System Enabled.\n"
"BLASTER"
"%x"
"Pro Audio Spectrum 3D JAZZ\n"
"\nDSP Version: %x.%02x\n"
"        IRQ: %d\n"
"        DMA: %d\n"
"GENMIDI.OP2"
"MixPg: "
"WAVE"
"fmt "
"data"
"MThd"
"MTrk"
"MUS"
"DMXTRACE"
"DMXOPTION"
"-opl3"
"-phase"
"Bad I_UpdateBox (%i, %i, %i, %i)"
"PLAYPAL"
"0x%x\n"
"-nomouse"
"Mouse: not present\n"
"Mouse: detected\n"
"-nojoy"
"joystick not found\n"
"joystick found\n"
"CENTER the joystick and press button 1:"
"\nPush the joystick to the UPPER LEFT corner and press button 1:"
"\nPush the joystick to the LOWER RIGHT corner and press button 1:"
"\n"
"INT_SetTimer0: %i is a bad value"
"novideo"
"-control"
"Using external control API\n"
"I_StartupDPMI\n"
"I_StartupMouse\n"
"I_StartupJoystick\n"
"I_StartupKeyboard\n"
"I_StartupSound\n"
"ENDOOM"
"-cdrom"
"STCDROM"
"STDISK"
"-net"
"d%c%s"
"-nosound"
"-nosfx"
"-nomusic"
"ENSONIQ\n"
"GUS\n"
"GUS1\n"
"GUS2\n"
"dmxgusc"
"dmxgus"
"cfg p=0x%x, i=%d, d=%d\n"
"Adlib\n"
"genmidi"
"Midi\n"
"cfg p=0x%x\n"
"SLIME16"
"RROCK14"
"RROCK07"
"RROCK17"
"RROCK13"
"RROCK19"
"FLOOR4_8"
"SFLR6_1"
"MFLR8_4"
"MFLR8_3"
"BOSSBACK"
"PFUB2"
"PFUB1"
"END0"
"END%i"
"CREDIT"
"VICTORY2"
"ENDPIC"
"map01"
"PLAYPAL"
"M_PAUSE"
"-debugfile"
"debug%i.txt"
"debug output to: %s\n"
"w"
"TITLEPIC"
"demo1"
"CREDIT"
"demo2"
"demo3"
"demo4"
"default.cfg"
"-shdev"
"-regdev"
"-comdev"
"rb"
"\nNo such response file!"
"Found response file %s!\n"
"%d command-line args:\n"
"%s\n"
"-nomonsters"
"-respawn"
"-fast"
"-devparm"
"-altdeath"
"-deathmatch"
"-cdrom"
"-turbo"
"turbo scale: %i%%\n"
"-wart"
"Warping to Episode %s, Map %s.\n"
"-file"
"-playdemo"
"-timedemo"
"%s.lmp"
"Playing demo %s.lmp.\n"
"-skill"
"-episode"
"-timer"
"Levels will end after %d minute"
"s"
".\n"
"-avg"
"-warp"
"-statcopy"
"-record"
"-loadgame"
"Player 1 left the game"
"%s is turbo!"
"NET GAME"
"Only %i deathmatch spots, 4 required"
"map31"
"-cdrom"
"SKY3"
"SKY1"
"SKY2"
"SKY4"
".lmp"
"-maxdemo"
"-nodraw"
"-noblit"
"-cdrom"
"M_LOADG"
"M_LSLEFT"
"M_LSCNTR"
"M_LSRGHT"
"M_SAVEG"
"HELP2"
"HELP1"
"HELP"
"M_SVOL"
"M_DOOM"
"M_NEWG"
"M_SKILL"
"M_EPISOD"
"M_OPTTTL"
"M_THERML"
"M_THERMM"
"M_THERMR"
"M_THERMO"
"M_CELL1"
"M_CELL2"
"PLAYPAL"
"mouse_sensitivity"
"sfx_volume"
"music_volume"
"show_messages"
"key_right"
"key_left"
"key_up"
"key_down"
"key_strafeleft"
"key_straferight"
"key_fire"
"key_use"
"key_strafe"
"key_speed"
"use_mouse"
"mouseb_fire"
"mouseb_strafe"
"mouseb_forward"
"use_joystick"
"joyb_fire"
"joyb_strafe"
"joyb_use"
"joyb_speed"
"screenblocks"
"detaillevel"
"snd_channels"
"snd_musicdevice"
"snd_sfxdevice"
"snd_sbport"
"snd_sbirq"
"snd_sbdma"
"snd_mport"
"usegamma"
"chatmacro0"
"chatmacro1"
"chatmacro2"
"chatmacro3"
"chatmacro4"
"chatmacro5"
"chatmacro6"
"chatmacro7"
"chatmacro8"
"chatmacro9"
"w"
"%s\t\t%i\n"
"%s\t\t\"%s\"\n"
"-config"
"\tdefault file: %s\n"
"r"
"%79s %[^\n]\n"
"%x"
"%i"
"DOOM00.pcx"
"M_ScreenShot: Couldn't create a PCX"
"PLAYPAL"
"screen shot"
"AMMNUM%d"
"Z_CT at am_map.c:%i"
"%s %d"
"fuck %d \r"
"Weird actor->movedir!"
"P_NewChaseDir: called with no target"
"P_GiveAmmo: bad type %i"
"P_SpecialThing: Unknown gettable thing"
"PTR_SlideTraverse: not a line?"
"P_AddActivePlat: no more plats!"
"P_RemoveActivePlat: can't find plat!"
"P_GroupLines: miscounted"
"map0%i"
"map%i"
"P_CrossSubsector: ss %i with numss = %i"
"P_InitPicAnims: bad cycle from %s to %s"
"P_PlayerInSpecialSector: unknown special %i"
"texture2"
"-avg"
"-timer"
"P_StartButton: no button slots left!"
"P_SpawnMapThing: Unknown type %i at (%i, %i)"
"Unknown tclass %i in savegame"
"P_UnarchiveSpecials:Unknown tclass %i in savegame"
"R_Subsector: ss %i with numss = %i"
"Z_CT at r_data.c:%i"
"R_GenerateLookup: column without a patch (%s)\n"
"R_GenerateLookup: texture %i is >64k"
"PNAMES"
"TEXTURE1"
"TEXTURE2"
"S_START"
"S_END"
"["
" "
"         ]"
""
""
"."
"R_InitTextures: bad texture directory"
"R_InitTextures: Missing patch in texture %s"
"F_START"
"F_END"
"COLORMAP"
"R_FlatNumForName: %s not found"
"R_TextureNumForName: %s not found"
"R_DrawFuzzColumn: %i to %i at %i"
"R_DrawColumn: %i to %i at %i"
"brdr_t"
"brdr_b"
"brdr_l"
"brdr_r"
"brdr_tl"
"brdr_tr"
"brdr_bl"
"brdr_br"
"."
"F_SKY1"
"R_MapPlane: %i, %i at %i"
"R_FindPlane: no more visplanes"
"R_DrawPlanes: drawsegs overflow (%i)"
"R_DrawPlanes: visplane overflow (%i)"
"R_DrawPlanes: opening overflow (%i)"
"Z_CT at r_plane.c:%i"
"Bad R_RenderWallRange: %i to %i"
"R_InstallSpriteLump: Bad frame characters in lump %i"
"R_InitSprites: Sprite %s frame %c has multip rot=0 lump"
"R_InitSprites: Sprite %s frame %c has rotations and a rot=0 lump"
"R_InitSprites: Sprite %s frame %c has rotations and a rot=0 lump"
"R_InitSprites: Sprite %s : %c : %c has two lumps mapped to it"
"R_InitSprites: No patches found for %s frame %c"
"R_InitSprites: Sprite %s frame %c is missing rotations"
"R_DrawSpriteRange: bad texturecolumn"
"R_ProjectSprite: invalid sprite number %i "
"R_ProjectSprite: invalid sprite frame %i : %i "
"R_ProjectSprite: invalid sprite number %i "
"R_ProjectSprite: invalid sprite frame %i : %i "
"Filename base of %s >8 chars"
"\tcouldn't open %s\n"
"\tadding %s\n"
"wad"
"IWAD"
"PWAD"
"Wad file %s doesn't have IWAD or PWAD id\n"
"Couldn't realloc lumpinfo"
"W_Reload: couldn't open %s"
"W_InitFiles: no files found"
"Couldn't allocate lumpcache"
"W_GetLumpNumForName: %s not found!"
"W_LumpLength: %i >= numlumps"
"W_ReadLump: %i >= numlumps"
"W_ReadLump: couldn't open %s"
"W_ReadLump: only read %i of %i on lump %i"
"W_CacheLump: %i >= numlumps"
"Z_CT at w_wad.c:%i"
"w"
"waddump.txt"
"%s "
"    %c"
"\n"
"Bad V_CopyRect"
"Bad V_DrawPatch"
"Bad V_DrawPatchDirect"
"Bad V_DrawBlock"
"Z_Free: freed a pointer without ZONEID"
"Z_Malloc: failed on allocation of %i bytes"
"Z_Malloc: an owner is required for purgable blocks"
"zone size: %i  location: %p\n"
"tag range: %i to %i\n"
"block:%p    size:%7i    user:%p    tag:%3i\n"
"ERROR: block size does not touch the next block\n"
"ERROR: next block doesn't have proper back link\n"
"ERROR: two consecutive free blocks\n"
"block:%p    size:%7i    user:%p    tag:%3i\n"
"ERROR: block size does not touch the next block\n"
"ERROR: next block doesn't have proper back link\n"
"ERROR: two consecutive free blocks\n"
"Z_CheckHeap: block size does not touch the next block\n"
"Z_CheckHeap: next block doesn't have proper back link\n"
"Z_CheckHeap: two consecutive free blocks\n"
"Z_ChangeTag: freed a pointer without ZONEID"
"Z_ChangeTag: an owner is required for purgable blocks"
"ang=0x%x;x,y=(0x%x,0x%x)"
"STTNUM%d"
"STYSNUM%d"
"STTPRCNT"
"STKEYS%d"
"STARMS"
"STGNUM%d"
"STFB%d"
"STBAR"
"STFST%d%d"
"STFTR%d0"
"STFTL%d0"
"STFOUCH%d"
"STFEVL%d"
"STFKILL%d"
"STFGOD0"
"STFDEAD0"
"PLAYPAL"
"Z_CT at st_stuff.c:%i"
"STTMINUS"
"drawNum: n->y - ST_Y < 0"
"updateMultIcon: y - ST_Y < 0"
"updateBinIcon: y - ST_Y < 0"
"NEWLEVEL"
"STCFN%.3d"
"Could not place patch on level %d"
"INTERPIC"
"WIMAP%d"
"CWILV%2.2d"
"WILV%d%d"
"WIURH0"
"WIURH1"
"WISPLAT"
"WIA%d%.2d%.2d"
"WIMINUS"
"WINUM%d"
"WIPCNT"
"WIF"
"WIENTER"
"WIOSTK"
"WIOSTS"
"WISCRT2"
"WIOBJ"
"WIOSTI"
"WIFRGS"
"WICOLON"
"WITIME"
"WISUCKS"
"WIPAR"
"WIKILRS"
"WIVCTMS"
"WIMSTT"
"STFST01"
"STFDEAD0"
"STPB%d"
"WIBP%d"
"Z_CT at wi_stuff.c:%i"
"wi_stuff.c"
"wbs->epsd"
"%s=%d in %s:%d"
"wbs->last"
"wbs->next"
"wbs->pnum"
"Attempt to set music volume at %d"
"Z_CT at s_sound.c:%i"
"Bad music number %d"
"d_%s"
"Attempt to set sfx volume at %d"
"Bad sfx #: %d"
"Bad list in dll_AddEndNode"
"Bad list in dll_AddStartNode"
"Bad list in dll_DelNode"
"Empty list in dll_DelNode"
"Floating-point support not loaded\r\n"
"TZ"
#endif
