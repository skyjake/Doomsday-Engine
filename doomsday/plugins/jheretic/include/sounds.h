/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/**
 * sounds.h:
 */

#ifndef __AUDIO_CONSTANTS_H__
#define __AUDIO_CONSTANTS_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

// Sounds.
typedef enum {
	SFX_NONE,					   // 000
	SFX_GLDHIT,					   // 001
	SFX_GNTFUL,					   // 002
	SFX_GNTHIT,					   // 003
	SFX_GNTPOW,					   // 004
	SFX_GNTACT,					   // 005
	SFX_GNTUSE,					   // 006
	SFX_PHOSHT,					   // 007
	SFX_PHOHIT,					   // 008
	SFX_PHOPOW,					   // 009
	SFX_LOBSHT,					   // 010
	SFX_LOBHIT,					   // 011
	SFX_LOBPOW,					   // 012
	SFX_HRNSHT,					   // 013
	SFX_HRNHIT,					   // 014
	SFX_HRNPOW,					   // 015
	SFX_RAMPHIT,				   // 016
	SFX_RAMRAIN,				   // 017
	SFX_BOWSHT,					   // 018
	SFX_STFHIT,					   // 019
	SFX_STFPOW,					   // 020
	SFX_STFCRK,					   // 021
	SFX_IMPSIT,					   // 022
	SFX_IMPAT1,					   // 023
	SFX_IMPAT2,					   // 024
	SFX_IMPDTH,					   // 025
	SFX_IMPACT,					   // 026
	SFX_IMPPAI,					   // 027
	SFX_MUMSIT,					   // 028
	SFX_MUMAT1,					   // 029
	SFX_MUMAT2,					   // 030
	SFX_MUMDTH,					   // 031
	SFX_MUMACT,					   // 032
	SFX_MUMPAI,					   // 033
	SFX_MUMHED,					   // 034
	SFX_BSTSIT,					   // 035
	SFX_BSTATK,					   // 036
	SFX_BSTDTH,					   // 037
	SFX_BSTACT,					   // 038
	SFX_BSTPAI,					   // 039
	SFX_CLKSIT,					   // 040
	SFX_CLKATK,					   // 041
	SFX_CLKDTH,					   // 042
	SFX_CLKACT,					   // 043
	SFX_CLKPAI,					   // 044
	SFX_SNKSIT,					   // 045
	SFX_SNKATK,					   // 046
	SFX_SNKDTH,					   // 047
	SFX_SNKACT,					   // 048
	SFX_SNKPAI,					   // 049
	SFX_KGTSIT,					   // 050
	SFX_KGTATK,					   // 051
	SFX_KGTAT2,					   // 052
	SFX_KGTDTH,					   // 053
	SFX_KGTACT,					   // 054
	SFX_KGTPAI,					   // 055
	SFX_WIZSIT,					   // 056
	SFX_WIZATK,					   // 057
	SFX_WIZDTH,					   // 058
	SFX_WIZACT,					   // 059
	SFX_WIZPAI,					   // 060
	SFX_MINSIT,					   // 061
	SFX_MINAT1,					   // 062
	SFX_MINAT2,					   // 063
	SFX_MINAT3,					   // 064
	SFX_MINDTH,					   // 065
	SFX_MINACT,					   // 066
	SFX_MINPAI,					   // 067
	SFX_HEDSIT,					   // 068
	SFX_HEDAT1,					   // 069
	SFX_HEDAT2,					   // 070
	SFX_HEDAT3,					   // 071
	SFX_HEDDTH,					   // 072
	SFX_HEDACT,					   // 073
	SFX_HEDPAI,					   // 074
	SFX_SORZAP,					   // 075
	SFX_SORRISE,				   // 076
	SFX_SORSIT,					   // 077
	SFX_SORATK,					   // 078
	SFX_SORACT,					   // 079
	SFX_SORPAI,					   // 080
	SFX_SORDSPH,				   // 081
	SFX_SORDEXP,				   // 082
	SFX_SORDBON,				   // 083
	SFX_SBTSIT,					   // 084
	SFX_SBTATK,					   // 085
	SFX_SBTDTH,					   // 086
	SFX_SBTACT,					   // 087
	SFX_SBTPAI,					   // 088
	SFX_PLROOF,					   // 089
	SFX_PLRPAI,					   // 090
	SFX_PLRDTH,					   // 091
	SFX_GIBDTH,					   // 092
	SFX_PLRWDTH,				   // 093
	SFX_PLRCDTH,				   // 094
	SFX_ITEMUP,					   // 095
	SFX_WPNUP,					   // 096
	SFX_TELEPT,					   // 097
	SFX_DOROPN,					   // 098
	SFX_DORCLS,					   // 099
	SFX_DORMOV,					   // 100
	SFX_ARTIUP,					   // 101
	SFX_SWITCH,					   // 102
	SFX_PSTART,					   // 103
	SFX_PSTOP,					   // 104
	SFX_STNMOV,					   // 105
	SFX_CHICPAI,				   // 106
	SFX_CHICATK,				   // 107
	SFX_CHICDTH,				   // 108
	SFX_CHICACT,				   // 109
	SFX_CHICPK1,				   // 110
	SFX_CHICPK2,				   // 111
	SFX_CHICPK3,				   // 112
	SFX_KEYUP,					   // 113
	SFX_RIPSLOP,				   // 114
	SFX_NEWPOD,					   // 115
	SFX_PODEXP,					   // 116
	SFX_BOUNCE,					   // 117
	SFX_VOLSHT,					   // 118
	SFX_VOLHIT,					   // 119
	SFX_BURN,					   // 120
	SFX_SPLASH,					   // 121
	SFX_GLOOP,					   // 122
	SFX_RESPAWN,				   // 123
	SFX_BLSSHT,					   // 124
	SFX_BLSHIT,					   // 125
	SFX_CHAT,					   // 126
	SFX_ARTIUSE,				   // 127
	SFX_GFRAG,					   // 128
	SFX_WATERFL,				   // 129
	SFX_WIND,					   // 130
	SFX_AMB1,					   // 131
	SFX_AMB2,					   // 132
	SFX_AMB3,					   // 133
	SFX_AMB4,					   // 134
	SFX_AMB5,					   // 135
	SFX_AMB6,					   // 136
	SFX_AMB7,					   // 137
	SFX_AMB8,					   // 138
	SFX_AMB9,					   // 139
	SFX_AMB10,					   // 140
	SFX_AMB11,					   // 141
	NUMSFX						   // 142
} sfxenum_t;

// Music.
typedef enum {
	MUS_E1M1,					   // 000
	MUS_E1M2,					   // 001
	MUS_E1M3,					   // 002
	MUS_E1M4,					   // 003
	MUS_E1M5,					   // 004
	MUS_E1M6,					   // 005
	MUS_E1M7,					   // 006
	MUS_E1M8,					   // 007
	MUS_E1M9,					   // 008
	MUS_E2M1,					   // 009
	MUS_E2M2,					   // 010
	MUS_E2M3,					   // 011
	MUS_E2M4,					   // 012
	MUS_E2M5,					   // 013
	MUS_E2M6,					   // 014
	MUS_E2M7,					   // 015
	MUS_E2M8,					   // 016
	MUS_E2M9,					   // 017
	MUS_E3M1,					   // 018
	MUS_E3M2,					   // 019
	MUS_E3M3,					   // 020
	MUS_E3M4,					   // 021
	MUS_E3M5,					   // 022
	MUS_E3M6,					   // 023
	MUS_E3M7,					   // 024
	MUS_E3M8,					   // 025
	MUS_E3M9,					   // 026
	MUS_E4M1,					   // 027
	MUS_E4M2,					   // 028
	MUS_E4M3,					   // 029
	MUS_E4M4,					   // 030
	MUS_E4M5,					   // 031
	MUS_E4M6,					   // 032
	MUS_E4M7,					   // 033
	MUS_E4M8,					   // 034
	MUS_E4M9,					   // 035
	MUS_E5M1,					   // 036
	MUS_E5M2,					   // 037
	MUS_E5M3,					   // 038
	MUS_E5M4,					   // 039
	MUS_E5M5,					   // 040
	MUS_E5M6,					   // 041
	MUS_E5M7,					   // 042
	MUS_E5M8,					   // 043
	MUS_E5M9,					   // 044
	MUS_E6M1,					   // 045
	MUS_E6M2,					   // 046
	MUS_E6M3,					   // 047
	MUS_TITL,					   // 048
	MUS_INTR,					   // 049
	MUS_CPTD,					   // 050
	NUMMUSIC					   // 051
} musicenum_t;

#endif
