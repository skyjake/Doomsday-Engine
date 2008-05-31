/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * sndidx.h: Sfx and music identifiers.
 *
 * Generated with DED Manager 1.0, a long time ago :)
 */

#ifndef __AUDIO_CONSTANTS_H__
#define __AUDIO_CONSTANTS_H__

// Sounds.
typedef enum {
    SFX_NONE,                      // 000
    SFX_PISTOL,                    // 001
    SFX_SHOTGN,                    // 002
    SFX_SGCOCK,                    // 003
    SFX_DSHTGN,                    // 004
    SFX_DBOPN,                     // 005
    SFX_DBCLS,                     // 006
    SFX_DBLOAD,                    // 007
    SFX_PLASMA,                    // 008
    SFX_BFG,                       // 009
    SFX_SAWUP,                     // 010
    SFX_SAWIDL,                    // 011
    SFX_SAWFUL,                    // 012
    SFX_SAWHIT,                    // 013
    SFX_RLAUNC,                    // 014
    SFX_RXPLOD,                    // 015
    SFX_FIRSHT,                    // 016
    SFX_FIRXPL,                    // 017
    SFX_PSTART,                    // 018
    SFX_PSTOP,                     // 019
    SFX_DOROPN,                    // 020
    SFX_DORCLS,                    // 021
    SFX_STNMOV,                    // 022
    SFX_SWTCHN,                    // 023
    SFX_SWTCHX,                    // 024
    SFX_PLPAIN,                    // 025
    SFX_DMPAIN,                    // 026
    SFX_POPAIN,                    // 027
    SFX_VIPAIN,                    // 028
    SFX_MNPAIN,                    // 029
    SFX_PEPAIN,                    // 030
    SFX_SLOP,                      // 031
    SFX_ITEMUP,                    // 032
    SFX_WPNUP,                     // 033
    SFX_OOF,                       // 034
    SFX_TELEPT,                    // 035
    SFX_POSIT1,                    // 036
    SFX_POSIT2,                    // 037
    SFX_POSIT3,                    // 038
    SFX_BGSIT1,                    // 039
    SFX_BGSIT2,                    // 040
    SFX_SGTSIT,                    // 041
    SFX_CACSIT,                    // 042
    SFX_BRSSIT,                    // 043
    SFX_CYBSIT,                    // 044
    SFX_SPISIT,                    // 045
    SFX_BSPSIT,                    // 046
    SFX_KNTSIT,                    // 047
    SFX_VILSIT,                    // 048
    SFX_MANSIT,                    // 049
    SFX_PESIT,                     // 050
    SFX_SKLATK,                    // 051
    SFX_SGTATK,                    // 052
    SFX_SKEPCH,                    // 053
    SFX_VILATK,                    // 054
    SFX_CLAW,                      // 055
    SFX_SKESWG,                    // 056
    SFX_PLDETH,                    // 057
    SFX_PDIEHI,                    // 058
    SFX_PODTH1,                    // 059
    SFX_PODTH2,                    // 060
    SFX_PODTH3,                    // 061
    SFX_BGDTH1,                    // 062
    SFX_BGDTH2,                    // 063
    SFX_SGTDTH,                    // 064
    SFX_CACDTH,                    // 065
    SFX_SKLDTH,                    // 066
    SFX_BRSDTH,                    // 067
    SFX_CYBDTH,                    // 068
    SFX_SPIDTH,                    // 069
    SFX_BSPDTH,                    // 070
    SFX_VILDTH,                    // 071
    SFX_KNTDTH,                    // 072
    SFX_PEDTH,                     // 073
    SFX_SKEDTH,                    // 074
    SFX_POSACT,                    // 075
    SFX_BGACT,                     // 076
    SFX_DMACT,                     // 077
    SFX_BSPACT,                    // 078
    SFX_BSPWLK,                    // 079
    SFX_VILACT,                    // 080
    SFX_NOWAY,                     // 081
    SFX_BAREXP,                    // 082
    SFX_PUNCH,                     // 083
    SFX_HOOF,                      // 084
    SFX_METAL,                     // 085
    SFX_CHGUN,                     // 086
    SFX_TINK,                      // 087
    SFX_BDOPN,                     // 088
    SFX_BDCLS,                     // 089
    SFX_ITMBK,                     // 090
    SFX_FLAME,                     // 091
    SFX_FLAMST,                    // 092
    SFX_GETPOW,                    // 093
    SFX_BOSPIT,                    // 094
    SFX_BOSCUB,                    // 095
    SFX_BOSSIT,                    // 096
    SFX_BOSPN,                     // 097
    SFX_BOSDTH,                    // 098
    SFX_MANATK,                    // 099
    SFX_MANDTH,                    // 100
    SFX_SSSIT,                     // 101
    SFX_SSDTH,                     // 102
    SFX_KEENPN,                    // 103
    SFX_KEENDT,                    // 104
    SFX_SKEACT,                    // 105
    SFX_SKESIT,                    // 106
    SFX_SKEATK,                    // 107
    SFX_RADIO,                     // 108
    SFX_WSPLASH,                   // 109
    SFX_NSPLASH,                   // 110
    SFX_BLURB,                     // 111
    NUMSFX                         // 112
} sfxenum_t;

// Music.
typedef enum {
    MUS_NONE,                      // 000
    MUS_E1M1,                      // 001
    MUS_E1M2,                      // 002
    MUS_E1M3,                      // 003
    MUS_E1M4,                      // 004
    MUS_E1M5,                      // 005
    MUS_E1M6,                      // 006
    MUS_E1M7,                      // 007
    MUS_E1M8,                      // 008
    MUS_E1M9,                      // 009
    MUS_E2M1,                      // 010
    MUS_E2M2,                      // 011
    MUS_E2M3,                      // 012
    MUS_E2M4,                      // 013
    MUS_E2M5,                      // 014
    MUS_E2M6,                      // 015
    MUS_E2M7,                      // 016
    MUS_E2M8,                      // 017
    MUS_E2M9,                      // 018
    MUS_E3M1,                      // 019
    MUS_E3M2,                      // 020
    MUS_E3M3,                      // 021
    MUS_E3M4,                      // 022
    MUS_E3M5,                      // 023
    MUS_E3M6,                      // 024
    MUS_E3M7,                      // 025
    MUS_E3M8,                      // 026
    MUS_E3M9,                      // 027
    MUS_INTER,                     // 028
    MUS_INTRO,                     // 029
    MUS_BUNNY,                     // 030
    MUS_VICTOR,                    // 031
    MUS_INTROA,                    // 032
    MUS_RUNNIN,                    // 033
    MUS_STALKS,                    // 034
    MUS_COUNTD,                    // 035
    MUS_BETWEE,                    // 036
    MUS_DOOM,                      // 037
    MUS_THE_DA,                    // 038
    MUS_SHAWN,                     // 039
    MUS_DDTBLU,                    // 040
    MUS_IN_CIT,                    // 041
    MUS_DEAD,                      // 042
    MUS_STLKS2,                    // 043
    MUS_THEDA2,                    // 044
    MUS_DOOM2,                     // 045
    MUS_DDTBL2,                    // 046
    MUS_RUNNI2,                    // 047
    MUS_DEAD2,                     // 048
    MUS_STLKS3,                    // 049
    MUS_ROMERO,                    // 050
    MUS_SHAWN2,                    // 051
    MUS_MESSAG,                    // 052
    MUS_COUNT2,                    // 053
    MUS_DDTBL3,                    // 054
    MUS_AMPIE,                     // 055
    MUS_THEDA3,                    // 056
    MUS_ADRIAN,                    // 057
    MUS_MESSG2,                    // 058
    MUS_ROMER2,                    // 059
    MUS_TENSE,                     // 060
    MUS_SHAWN3,                    // 061
    MUS_OPENIN,                    // 062
    MUS_EVIL,                      // 063
    MUS_ULTIMA,                    // 064
    MUS_READ_M,                    // 065
    MUS_DM2TTL,                    // 066
    MUS_DM2INT,                    // 067
    NUMMUSIC                       // 068
} musicenum_t;

#endif
