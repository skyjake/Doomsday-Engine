/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * d_englsh.h: Printed strings for translation (native is English).
 */

#ifndef __D_ENGLSH_H__
#define __D_ENGLSH_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#define GET_TXT(x)          ((*_api_InternalData.text)? (*_api_InternalData.text)[x].text : "")

#define D_DEVSTR            GET_TXT(TXT_D_DEVSTR)
#define D_CDROM             GET_TXT(TXT_D_CDROM)
#define PRESSKEY            GET_TXT(TXT_PRESSKEY)
#define PRESSYN             GET_TXT(TXT_PRESSYN)
#define QUITMSG             GET_TXT(TXT_QUITMSG)
#define LOADNET             GET_TXT(TXT_LOADNET)
#define QLOADNET            GET_TXT(TXT_QLOADNET)
#define QSAVESPOT           GET_TXT(TXT_QSAVESPOT)
#define SAVEDEAD            GET_TXT(TXT_SAVEDEAD)
#define SAVEOUTMAP          GET_TXT(TXT_SAVEOUTMAP)
#define SAVENET             GET_TXT(TXT_SAVENET)
#define QSPROMPT            GET_TXT(TXT_QSPROMPT)
#define QLPROMPT            GET_TXT(TXT_QLPROMPT)
#define NEWGAME             GET_TXT(TXT_NEWGAME)
#define NIGHTMARE           GET_TXT(TXT_NIGHTMARE)
#define SWSTRING            GET_TXT(TXT_SWSTRING)
#define MSGOFF              GET_TXT(TXT_MSGOFF)
#define MSGON               GET_TXT(TXT_MSGON)
#define NETEND              GET_TXT(TXT_NETEND)
#define ENDGAME             GET_TXT(TXT_ENDGAME)
#define ENDNOGAME           GET_TXT(TXT_ENDNOGAME)

#define SUICIDEOUTMAP       GET_TXT(TXT_SUICIDEOUTMAP)
#define SUICIDEASK          GET_TXT(TXT_SUICIDEASK)

#define DOSY                GET_TXT(TXT_DOSY)
#define DETAILHI            GET_TXT(TXT_DETAILHI)
#define DETAILLO            GET_TXT(TXT_DETAILLO)

#define GAMMALVL0           GET_TXT(TXT_GAMMALVL0)
#define GAMMALVL1           GET_TXT(TXT_GAMMALVL1)
#define GAMMALVL2           GET_TXT(TXT_GAMMALVL2)
#define GAMMALVL3           GET_TXT(TXT_GAMMALVL3)
#define GAMMALVL4           GET_TXT(TXT_GAMMALVL4)

#define EMPTYSTRING         GET_TXT(TXT_EMPTYSTRING)

#define GOTARMOR            GET_TXT(TXT_GOTARMOR)
#define GOTMEGA             GET_TXT(TXT_GOTMEGA)
#define GOTHTHBONUS         GET_TXT(TXT_GOTHTHBONUS)
#define GOTARMBONUS         GET_TXT(TXT_GOTARMBONUS)
#define GOTSTIM             GET_TXT(TXT_GOTSTIM)
#define GOTMEDINEED         GET_TXT(TXT_GOTMEDINEED)
#define GOTMEDIKIT          GET_TXT(TXT_GOTMEDIKIT)
#define GOTSUPER            GET_TXT(TXT_GOTSUPER)
#define GOTBLUECARD         GET_TXT(TXT_GOTBLUECARD)
#define GOTYELWCARD         GET_TXT(TXT_GOTYELWCARD)
#define GOTREDCARD          GET_TXT(TXT_GOTREDCARD)
#define GOTBLUESKUL         GET_TXT(TXT_GOTBLUESKUL)
#define GOTYELWSKUL         GET_TXT(TXT_GOTYELWSKUL)
#define GOTREDSKULL         GET_TXT(TXT_GOTREDSKULL)
#define GOTINVUL            GET_TXT(TXT_GOTINVUL)
#define GOTBERSERK          GET_TXT(TXT_GOTBERSERK)
#define GOTINVIS            GET_TXT(TXT_GOTINVIS)
#define GOTSUIT             GET_TXT(TXT_GOTSUIT)
#define GOTMAP              GET_TXT(TXT_GOTMAP)
#define GOTVISOR            GET_TXT(TXT_GOTVISOR)
#define GOTMSPHERE          GET_TXT(TXT_GOTMSPHERE)
#define GOTCLIP             GET_TXT(TXT_GOTCLIP)
#define GOTCLIPBOX          GET_TXT(TXT_GOTCLIPBOX)
#define GOTROCKET           GET_TXT(TXT_GOTROCKET)
#define GOTROCKBOX          GET_TXT(TXT_GOTROCKBOX)
#define GOTCELL             GET_TXT(TXT_GOTCELL)
#define GOTCELLBOX          GET_TXT(TXT_GOTCELLBOX)
#define GOTSHELLS           GET_TXT(TXT_GOTSHELLS)
#define GOTSHELLBOX         GET_TXT(TXT_GOTSHELLBOX)
#define GOTBACKPACK         GET_TXT(TXT_GOTBACKPACK)
#define GOTBFG9000          GET_TXT(TXT_GOTBFG9000)
#define GOTCHAINGUN         GET_TXT(TXT_GOTCHAINGUN)
#define GOTCHAINSAW         GET_TXT(TXT_GOTCHAINSAW)
#define GOTLAUNCHER         GET_TXT(TXT_GOTLAUNCHER)
#define GOTPLASMA           GET_TXT(TXT_GOTPLASMA)
#define GOTSHOTGUN          GET_TXT(TXT_GOTSHOTGUN)
#define GOTSHOTGUN2         GET_TXT(TXT_GOTSHOTGUN2)

#define PD_BLUEO            GET_TXT(TXT_PD_BLUEO)
#define PD_REDO             GET_TXT(TXT_PD_REDO)
#define PD_YELLOWO          GET_TXT(TXT_PD_YELLOWO)
#define PD_BLUEK            GET_TXT(TXT_PD_BLUEK)
#define PD_REDK             GET_TXT(TXT_PD_REDK)
#define PD_YELLOWK          GET_TXT(TXT_PD_YELLOWK)

#define TXT_GAMESAVED       GET_TXT(TXT_GGSAVED)

#define HUSTR_MSGU          GET_TXT(TXT_HUSTR_MSGU)
#define HUSTR_E1M1          GET_TXT(TXT_HUSTR_E1M1)
#define HUSTR_E1M2          GET_TXT(TXT_HUSTR_E1M2)
#define HUSTR_E1M3          GET_TXT(TXT_HUSTR_E1M3)
#define HUSTR_E1M4          GET_TXT(TXT_HUSTR_E1M4)
#define HUSTR_E1M5          GET_TXT(TXT_HUSTR_E1M5)
#define HUSTR_E1M6          GET_TXT(TXT_HUSTR_E1M6)
#define HUSTR_E1M7          GET_TXT(TXT_HUSTR_E1M7)
#define HUSTR_E1M8          GET_TXT(TXT_HUSTR_E1M8)
#define HUSTR_E1M9          GET_TXT(TXT_HUSTR_E1M9)
#define HUSTR_E2M1          GET_TXT(TXT_HUSTR_E2M1)
#define HUSTR_E2M2          GET_TXT(TXT_HUSTR_E2M2)
#define HUSTR_E2M3          GET_TXT(TXT_HUSTR_E2M3)
#define HUSTR_E2M4          GET_TXT(TXT_HUSTR_E2M4)
#define HUSTR_E2M5          GET_TXT(TXT_HUSTR_E2M5)
#define HUSTR_E2M6          GET_TXT(TXT_HUSTR_E2M6)
#define HUSTR_E2M7          GET_TXT(TXT_HUSTR_E2M7)
#define HUSTR_E2M8          GET_TXT(TXT_HUSTR_E2M8)
#define HUSTR_E2M9          GET_TXT(TXT_HUSTR_E2M9)
#define HUSTR_E3M1          GET_TXT(TXT_HUSTR_E3M1)
#define HUSTR_E3M2          GET_TXT(TXT_HUSTR_E3M2)
#define HUSTR_E3M3          GET_TXT(TXT_HUSTR_E3M3)
#define HUSTR_E3M4          GET_TXT(TXT_HUSTR_E3M4)
#define HUSTR_E3M5          GET_TXT(TXT_HUSTR_E3M5)
#define HUSTR_E3M6          GET_TXT(TXT_HUSTR_E3M6)
#define HUSTR_E3M7          GET_TXT(TXT_HUSTR_E3M7)
#define HUSTR_E3M8          GET_TXT(TXT_HUSTR_E3M8)
#define HUSTR_E3M9          GET_TXT(TXT_HUSTR_E3M9)
#define HUSTR_E4M1          GET_TXT(TXT_HUSTR_E4M1)
#define HUSTR_E4M2          GET_TXT(TXT_HUSTR_E4M2)
#define HUSTR_E4M3          GET_TXT(TXT_HUSTR_E4M3)
#define HUSTR_E4M4          GET_TXT(TXT_HUSTR_E4M4)
#define HUSTR_E4M5          GET_TXT(TXT_HUSTR_E4M5)
#define HUSTR_E4M6          GET_TXT(TXT_HUSTR_E4M6)
#define HUSTR_E4M7          GET_TXT(TXT_HUSTR_E4M7)
#define HUSTR_E4M8          GET_TXT(TXT_HUSTR_E4M8)
#define HUSTR_E4M9          GET_TXT(TXT_HUSTR_E4M9)
#define HUSTR_1             GET_TXT(TXT_HUSTR_1)
#define HUSTR_2             GET_TXT(TXT_HUSTR_2)
#define HUSTR_3             GET_TXT(TXT_HUSTR_3)
#define HUSTR_4             GET_TXT(TXT_HUSTR_4)
#define HUSTR_5             GET_TXT(TXT_HUSTR_5)
#define HUSTR_6             GET_TXT(TXT_HUSTR_6)
#define HUSTR_7             GET_TXT(TXT_HUSTR_7)
#define HUSTR_8             GET_TXT(TXT_HUSTR_8)
#define HUSTR_9             GET_TXT(TXT_HUSTR_9)
#define HUSTR_10            GET_TXT(TXT_HUSTR_10)
#define HUSTR_11            GET_TXT(TXT_HUSTR_11)
#define HUSTR_12            GET_TXT(TXT_HUSTR_12)
#define HUSTR_13            GET_TXT(TXT_HUSTR_13)
#define HUSTR_14            GET_TXT(TXT_HUSTR_14)
#define HUSTR_15            GET_TXT(TXT_HUSTR_15)
#define HUSTR_16            GET_TXT(TXT_HUSTR_16)
#define HUSTR_17            GET_TXT(TXT_HUSTR_17)
#define HUSTR_18            GET_TXT(TXT_HUSTR_18)
#define HUSTR_19            GET_TXT(TXT_HUSTR_19)
#define HUSTR_20            GET_TXT(TXT_HUSTR_20)
#define HUSTR_21            GET_TXT(TXT_HUSTR_21)
#define HUSTR_22            GET_TXT(TXT_HUSTR_22)
#define HUSTR_23            GET_TXT(TXT_HUSTR_23)
#define HUSTR_24            GET_TXT(TXT_HUSTR_24)
#define HUSTR_25            GET_TXT(TXT_HUSTR_25)
#define HUSTR_26            GET_TXT(TXT_HUSTR_26)
#define HUSTR_27            GET_TXT(TXT_HUSTR_27)
#define HUSTR_28            GET_TXT(TXT_HUSTR_28)
#define HUSTR_29            GET_TXT(TXT_HUSTR_29)
#define HUSTR_30            GET_TXT(TXT_HUSTR_30)
#define HUSTR_31            GET_TXT(TXT_HUSTR_31)
#define HUSTR_32            GET_TXT(TXT_HUSTR_32)

#define PHUSTR_1            GET_TXT(TXT_PHUSTR_1)
#define PHUSTR_2            GET_TXT(TXT_PHUSTR_2)
#define PHUSTR_3            GET_TXT(TXT_PHUSTR_3)
#define PHUSTR_4            GET_TXT(TXT_PHUSTR_4)
#define PHUSTR_5            GET_TXT(TXT_PHUSTR_5)
#define PHUSTR_6            GET_TXT(TXT_PHUSTR_6)
#define PHUSTR_7            GET_TXT(TXT_PHUSTR_7)
#define PHUSTR_8            GET_TXT(TXT_PHUSTR_8)
#define PHUSTR_9            GET_TXT(TXT_PHUSTR_9)
#define PHUSTR_10           GET_TXT(TXT_PHUSTR_10)
#define PHUSTR_11           GET_TXT(TXT_PHUSTR_11)
#define PHUSTR_12           GET_TXT(TXT_PHUSTR_12)
#define PHUSTR_13           GET_TXT(TXT_PHUSTR_13)
#define PHUSTR_14           GET_TXT(TXT_PHUSTR_14)
#define PHUSTR_15           GET_TXT(TXT_PHUSTR_15)
#define PHUSTR_16           GET_TXT(TXT_PHUSTR_16)
#define PHUSTR_17           GET_TXT(TXT_PHUSTR_17)
#define PHUSTR_18           GET_TXT(TXT_PHUSTR_18)
#define PHUSTR_19           GET_TXT(TXT_PHUSTR_19)
#define PHUSTR_20           GET_TXT(TXT_PHUSTR_20)
#define PHUSTR_21           GET_TXT(TXT_PHUSTR_21)
#define PHUSTR_22           GET_TXT(TXT_PHUSTR_22)
#define PHUSTR_23           GET_TXT(TXT_PHUSTR_23)
#define PHUSTR_24           GET_TXT(TXT_PHUSTR_24)
#define PHUSTR_25           GET_TXT(TXT_PHUSTR_25)
#define PHUSTR_26           GET_TXT(TXT_PHUSTR_26)
#define PHUSTR_27           GET_TXT(TXT_PHUSTR_27)
#define PHUSTR_28           GET_TXT(TXT_PHUSTR_28)
#define PHUSTR_29           GET_TXT(TXT_PHUSTR_29)
#define PHUSTR_30           GET_TXT(TXT_PHUSTR_30)
#define PHUSTR_31           GET_TXT(TXT_PHUSTR_31)
#define PHUSTR_32           GET_TXT(TXT_PHUSTR_32)

#define THUSTR_1            GET_TXT(TXT_THUSTR_1)
#define THUSTR_2            GET_TXT(TXT_THUSTR_2)
#define THUSTR_3            GET_TXT(TXT_THUSTR_3)
#define THUSTR_4            GET_TXT(TXT_THUSTR_4)
#define THUSTR_5            GET_TXT(TXT_THUSTR_5)
#define THUSTR_6            GET_TXT(TXT_THUSTR_6)
#define THUSTR_7            GET_TXT(TXT_THUSTR_7)
#define THUSTR_8            GET_TXT(TXT_THUSTR_8)
#define THUSTR_9            GET_TXT(TXT_THUSTR_9)
#define THUSTR_10           GET_TXT(TXT_THUSTR_10)
#define THUSTR_11           GET_TXT(TXT_THUSTR_11)
#define THUSTR_12           GET_TXT(TXT_THUSTR_12)
#define THUSTR_13           GET_TXT(TXT_THUSTR_13)
#define THUSTR_14           GET_TXT(TXT_THUSTR_14)
#define THUSTR_15           GET_TXT(TXT_THUSTR_15)
#define THUSTR_16           GET_TXT(TXT_THUSTR_16)
#define THUSTR_17           GET_TXT(TXT_THUSTR_17)
#define THUSTR_18           GET_TXT(TXT_THUSTR_18)
#define THUSTR_19           GET_TXT(TXT_THUSTR_19)
#define THUSTR_20           GET_TXT(TXT_THUSTR_20)
#define THUSTR_21           GET_TXT(TXT_THUSTR_21)
#define THUSTR_22           GET_TXT(TXT_THUSTR_22)
#define THUSTR_23           GET_TXT(TXT_THUSTR_23)
#define THUSTR_24           GET_TXT(TXT_THUSTR_24)
#define THUSTR_25           GET_TXT(TXT_THUSTR_25)
#define THUSTR_26           GET_TXT(TXT_THUSTR_26)
#define THUSTR_27           GET_TXT(TXT_THUSTR_27)
#define THUSTR_28           GET_TXT(TXT_THUSTR_28)
#define THUSTR_29           GET_TXT(TXT_THUSTR_29)
#define THUSTR_30           GET_TXT(TXT_THUSTR_30)
#define THUSTR_31           GET_TXT(TXT_THUSTR_31)
#define THUSTR_32           GET_TXT(TXT_THUSTR_32)

#define HUSTR_CHATMACRO1    GET_TXT(TXT_HUSTR_CHATMACRO1)
#define HUSTR_CHATMACRO2    GET_TXT(TXT_HUSTR_CHATMACRO2)
#define HUSTR_CHATMACRO3    GET_TXT(TXT_HUSTR_CHATMACRO3)
#define HUSTR_CHATMACRO4    GET_TXT(TXT_HUSTR_CHATMACRO4)
#define HUSTR_CHATMACRO5    GET_TXT(TXT_HUSTR_CHATMACRO5)
#define HUSTR_CHATMACRO6    GET_TXT(TXT_HUSTR_CHATMACRO6)
#define HUSTR_CHATMACRO7    GET_TXT(TXT_HUSTR_CHATMACRO7)
#define HUSTR_CHATMACRO8    GET_TXT(TXT_HUSTR_CHATMACRO8)
#define HUSTR_CHATMACRO9    GET_TXT(TXT_HUSTR_CHATMACRO9)
#define HUSTR_CHATMACRO0    GET_TXT(TXT_HUSTR_CHATMACRO0)
#define HUSTR_TALKTOSELF1   GET_TXT(TXT_HUSTR_TALKTOSELF1)
#define HUSTR_TALKTOSELF2   GET_TXT(TXT_HUSTR_TALKTOSELF2)
#define HUSTR_TALKTOSELF3   GET_TXT(TXT_HUSTR_TALKTOSELF3)
#define HUSTR_TALKTOSELF4   GET_TXT(TXT_HUSTR_TALKTOSELF4)
#define HUSTR_TALKTOSELF5   GET_TXT(TXT_HUSTR_TALKTOSELF5)
#define HUSTR_MESSAGESENT   GET_TXT(TXT_HUSTR_MESSAGESENT)

/**
 * The following should NOT be changed unless it seems just AWFULLY
 * necessary!
 */
#define HUSTR_PLRGREEN      GET_TXT(TXT_HUSTR_PLRGREEN)
#define HUSTR_PLRINDIGO     GET_TXT(TXT_HUSTR_PLRINDIGO)
#define HUSTR_PLRBROWN      GET_TXT(TXT_HUSTR_PLRBROWN)
#define HUSTR_PLRRED        GET_TXT(TXT_HUSTR_PLRRED)

//// @todo What are these doing here??
#define HUSTR_KEYGREEN      'g'
#define HUSTR_KEYINDIGO     'i'
#define HUSTR_KEYBROWN      'b'
#define HUSTR_KEYRED        'r'

#define AMSTR_FOLLOWON      GET_TXT(TXT_AMSTR_FOLLOWON)
#define AMSTR_FOLLOWOFF     GET_TXT(TXT_AMSTR_FOLLOWOFF)
#define AMSTR_ROTATEON      GET_TXT(TXT_AMSTR_ROTATEON)
#define AMSTR_ROTATEOFF     GET_TXT(TXT_AMSTR_ROTATEOFF)
#define AMSTR_GRIDON        GET_TXT(TXT_AMSTR_GRIDON)
#define AMSTR_GRIDOFF       GET_TXT(TXT_AMSTR_GRIDOFF)
#define AMSTR_MARKEDSPOT    GET_TXT(TXT_AMSTR_MARKEDSPOT)
#define AMSTR_MARKSCLEARED  GET_TXT(TXT_AMSTR_MARKSCLEARED)

// Key names:
#define KEY1                GET_TXT(TXT_KEY1)
#define KEY2                GET_TXT(TXT_KEY2)
#define KEY3                GET_TXT(TXT_KEY3)
#define KEY4                GET_TXT(TXT_KEY4)
#define KEY5                GET_TXT(TXT_KEY5)
#define KEY6                GET_TXT(TXT_KEY6)

// Weapon names:
#define WEAPON1             GET_TXT(TXT_WEAPON1)
#define WEAPON2             GET_TXT(TXT_WEAPON2)
#define WEAPON3             GET_TXT(TXT_WEAPON3)
#define WEAPON4             GET_TXT(TXT_WEAPON4)
#define WEAPON5             GET_TXT(TXT_WEAPON5)
#define WEAPON6             GET_TXT(TXT_WEAPON6)
#define WEAPON7             GET_TXT(TXT_WEAPON7)
#define WEAPON8             GET_TXT(TXT_WEAPON8)
#define WEAPON9             GET_TXT(TXT_WEAPON9)

#define STSTR_MUS           GET_TXT(TXT_STSTR_MUS)
#define STSTR_NOMUS         GET_TXT(TXT_STSTR_NOMUS)
#define STSTR_DQDON         GET_TXT(TXT_STSTR_DQDON)
#define STSTR_DQDOFF        GET_TXT(TXT_STSTR_DQDOFF)
#define STSTR_KFAADDED      GET_TXT(TXT_STSTR_KFAADDED)
#define STSTR_FAADDED       GET_TXT(TXT_STSTR_FAADDED)
#define STSTR_NCON          GET_TXT(TXT_STSTR_NCON)
#define STSTR_NCOFF         GET_TXT(TXT_STSTR_NCOFF)
#define STSTR_BEHOLD        GET_TXT(TXT_STSTR_BEHOLD)
#define STSTR_BEHOLDX       GET_TXT(TXT_STSTR_BEHOLDX)
#define STSTR_CHOPPERS      GET_TXT(TXT_STSTR_CHOPPERS)
#define STSTR_CLEV          GET_TXT(TXT_STSTR_CLEV)

#define E1TEXT              GET_TXT(TXT_E1TEXT)
#define E2TEXT              GET_TXT(TXT_E2TEXT)
#define E3TEXT              GET_TXT(TXT_E3TEXT)
#define E4TEXT              GET_TXT(TXT_E4TEXT)

// After level 6, put this:
#define C1TEXT              GET_TXT(TXT_C1TEXT)

// After level 11, put this:
#define C2TEXT              GET_TXT(TXT_C2TEXT)

// After level 20, put this:
#define C3TEXT              GET_TXT(TXT_C3TEXT)

// After level 29, put this:
#define C4TEXT              GET_TXT(TXT_C4TEXT)

// Before level 31, put this:
#define C5TEXT              GET_TXT(TXT_C5TEXT)

// Before level 32, put this:
#define C6TEXT              GET_TXT(TXT_C6TEXT)

// After map 06:
#define P1TEXT              GET_TXT(TXT_P1TEXT)

// After map 11:
#define P2TEXT              GET_TXT(TXT_P2TEXT)

// After map 20:
#define P3TEXT              GET_TXT(TXT_P3TEXT)

// After map 30:
#define P4TEXT              GET_TXT(TXT_P4TEXT)

// Before map 31:
#define P5TEXT              GET_TXT(TXT_P5TEXT)

// Before map 32:
#define P6TEXT              GET_TXT(TXT_P6TEXT)
#define T1TEXT              GET_TXT(TXT_T1TEXT)
#define T2TEXT              GET_TXT(TXT_T2TEXT)
#define T3TEXT              GET_TXT(TXT_T3TEXT)
#define T4TEXT              GET_TXT(TXT_T4TEXT)
#define T5TEXT              GET_TXT(TXT_T5TEXT)
#define T6TEXT              GET_TXT(TXT_T6TEXT)

#define CC_ZOMBIE           GET_TXT(TXT_CC_ZOMBIE)
#define CC_SHOTGUN          GET_TXT(TXT_CC_SHOTGUN)
#define CC_HEAVY            GET_TXT(TXT_CC_HEAVY)
#define CC_IMP              GET_TXT(TXT_CC_IMP)
#define CC_DEMON            GET_TXT(TXT_CC_DEMON)
#define CC_LOST             GET_TXT(TXT_CC_LOST)
#define CC_CACO             GET_TXT(TXT_CC_CACO)
#define CC_HELL             GET_TXT(TXT_CC_HELL)
#define CC_BARON            GET_TXT(TXT_CC_BARON)
#define CC_ARACH            GET_TXT(TXT_CC_ARACH)
#define CC_PAIN             GET_TXT(TXT_CC_PAIN)
#define CC_REVEN            GET_TXT(TXT_CC_REVEN)
#define CC_MANCU            GET_TXT(TXT_CC_MANCU)
#define CC_ARCH             GET_TXT(TXT_CC_ARCH)
#define CC_SPIDER           GET_TXT(TXT_CC_SPIDER)
#define CC_CYBER            GET_TXT(TXT_CC_CYBER)
#define CC_HERO             GET_TXT(TXT_CC_HERO)

#define DELETESAVEGAME_CONFIRM GET_TXT(TXT_DELETESAVEGAME_CONFIRM)
#define REBORNLOAD_CONFIRM  GET_TXT(TXT_REBORNLOAD_CONFIRM)

#endif
