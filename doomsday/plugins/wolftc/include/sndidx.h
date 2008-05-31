/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
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

/*
 * Sfx and music identifiers
 * Generated with DED Manager 1.0
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
    SFX_MENUBC,                    // 112
    SFX_MENUMV,                    // 113
    SFX_MENUSL,                    // 114
    SFX_INTCNT,                    // 115
    SFX_INTCMP,                    // 116
    SFX_INTYEA,                    // 116
    SFX_HUDMS1,                    // 116
    SFX_HUDMS2,                    // 116
    SFX_SCRNRS,                    // 116
    SFX_WLFDRO,                    // 113
    SFX_WLFDRC,                    // 114
    SFX_WLFPWL,
    SFX_SMPDRO,                    // 115
    SFX_SMPDRC,                    // 116
    SFX_SMPPWL,
    SFX_DORLCK,
    SFX_PLTSTR,
    SFX_PLTMOV,
    SFX_PLTSTP,
    SFX_BLOCKD,
    SFX_SWITCH,
    SFX_WFESWI,                    // 267
    SFX_MPESWI,                    // 268
    SFX_SECRET,
    SFX_TELE,
    SFX_NMRRSP,
    SFX_METHIT,                    // 269
    SFX_PLYPAI,
    SFX_PLYDTH,
    SFX_GIBBED,
    SFX_KNFATK,                    // 117
    SFX_CKNFAT,                    // 117
    SFX_WPISTO,                    // 118
    SFX_LPISTO,                    // 119
    SFX_UPISTO,
    SFX_CPISTO,
    SFX_3PISTO,
    SFX_WMACHI,                    // 120
    SFX_LMACHI,                    // 121
    SFX_OMACHI,                    // 122
    SFX_UMACHI,                    // 122
    SFX_3MACHI,
    SFX_WCHGUN,                    // 123
    SFX_LCHGUN,
    SFX_UCHGUN,
    SFX_3CHGUN,                    // 123
    SFX_ORIFLE,                    // 124
    SFX_OREVOL,                    // 125
    SFX_WROCKE,                    // 126
    SFX_WEXPLO,                    // 126
    SFX_CRCSHT,
    SFX_CRCEXP,
    SFX_FLASHT,                    // 264
    SFX_FLAXPL,                    // 126
    SFX_CFLSHT,                    // 126
    SFX_CFLEXP,                    // 126
    SFX_3FLSHT,                    // 126
    SFX_CMFIRE,                    // 126
    SFX_WMLOAD,                    // 127
    SFX_HELTH1,                    // 128
    SFX_HELTH2,                    // 129
    SFX_HELTH3,                    // 129
    SFX_GETMEG,                    // 130
    SFX_AMMOCL,                    // 131
    SFX_AMMOBX,                    // 132
    SFX_KEYUP,                     // 133
    SFX_CKEYUP,                    // 134
    SFX_GETMAC,                    // 135
    SFX_GETGAT,                    // 136
    SFX_SPELUP,                    // 137
    SFX_LGETGT,                    // 136
    SFX_TREAS1,                    // 138
    SFX_TREAS2,                    // 139
    SFX_TREAS3,                    // 140
    SFX_ATREAS,                    // 141
    SFX_BGYFIR,                    // 142
    SFX_BOSFIR,                    // 142
    SFX_SSGFIR,                    // 143
    SFX_BGYFBS,
    SFX_BGYFBE,
    SFX_CREFBS,
    SFX_CREFBE,
    SFX_LSSFIR,                    // 144
    SFX_SZOFIR,                    // 144
    SFX_BGYPAI,                    // 146
    SFX_METWLK,                    // 147
    SFX_TRMOVS,                    // 148
    SFX_GRDSIT,                    // 149
    SFX_GDDTH1,                    // 150
    SFX_GDDTH2,                    // 151
    SFX_GDDTH3,                    // 152
    SFX_GDDTH4,                    // 153
    SFX_GDDTH5,                    // 154
    SFX_GDDTH6,                    // 155
    SFX_GDDTH7,                    // 156
    SFX_GDDTH8,                    // 156
    SFX_SSGSIT,                    // 157
    SFX_SSGDTH,                    // 158
    SFX_DOGSIT,                    // 159
    SFX_DOGATK,                    // 160
    SFX_DOGDTH,                    // 162
    SFX_MUTDTH,                    // 163
    SFX_OFISIT,                    // 165
    SFX_OFIDTH,                    // 166
    SFX_HANSIT,                    // 169
    SFX_HANDTH,                    // 170
    SFX_SCHSIT,                    // 171
    SFX_SCHDTH,                    // 172
    SFX_SYRTHW,                    // 145
    SFX_SYREXP,
    SFX_HGTSIT,                    // 173
    SFX_HGTDTH,                    // 174
    SFX_HGBLUR,                    // 175
    SFX_MHTSIT,                    // 176
    SFX_MHTDTH,                    // 177
    SFX_HITSIT,                    // 178
    SFX_HITDTH,                    // 179
    SFX_HITSLP,
    SFX_OTTSIT,                    // 180
    SFX_OTTDTH,                    // 181
    SFX_GRESIT,                    // 182
    SFX_GREDTH,                    // 183
    SFX_GENSIT,                    // 184
    SFX_GENDTH,                    // 185
    SFX_PACMAN,                    // 186
    SFX_TRASIT,                    // 187
    SFX_TRADTH,                    // 188
    SFX_WILSIT,                    // 189
    SFX_WILDTH,                    // 190
    SFX_UBESIT,                    // 191
    SFX_UBEDTH,                    // 191
    SFX_KGHSIT,                    // 192
    SFX_KGHDTH,                    // 193
    SFX_GHOST,                     // 194
    SFX_ANGSIT,                    // 195
    SFX_ANGDTH,                    // 196
    SFX_ANGSHT,                    // 265
    SFX_ANGXPL,                    // 266
    SFX_CMSIT,                     // 167
    SFX_CMDTH,                     // 168
    SFX_RATSIT,                    // 197
    SFX_RATATK,                    // 198
    SFX_FGDSIT,                    // 199
    SFX_FGDDTH,                    // 200
    SFX_ELGSIT,                    // 201
    SFX_ELGDTH,                    // 202
    SFX_LGURDS,                    // 203
    SFX_LGRDD1,                    // 204
    SFX_LGRDD2,                    // 205
    SFX_LGRDD3,                    // 206
    SFX_LGRDD4,                    // 207
    SFX_LGRDD5,                    // 208
    SFX_LGRDD6,                    // 209
    SFX_LGRDD7,                    // 210
    SFX_LGRDD8,                    // 211
    SFX_LSSGDS,                    // 212
    SFX_LSSGDD,                    // 213
    SFX_LBATD,                     // 214
    SFX_LDOGS,                     // 215
    SFX_LDOGD,                     // 216
    SFX_LDOGA,                     // 218
    SFX_LOFFIS,                    // 219
    SFX_LOFFID,                    // 220
    SFX_LWILYS,                    // 221
    SFX_LWILYD,                    // 222
    SFX_LPROFS,                    // 223
    SFX_LPROFD,                    // 224
    SFX_LAXED,                     // 225
    SFX_LRBOTS,                    // 226
    SFX_LRBOTD,                    // 227
    SFX_LRBOTM,                    // 145
    SFX_LRBOTX,                    // 145
    SFX_LDEVLS,                    // 228
    SFX_LDEVLD,                    // 229
    SFX_LSPRD,                     // 230
    SFX_LELGST,                    // 232
    SFX_LELGDH,                    // 233
    SFX_CZOMBS,                    // 290
    SFX_CZOMBD,                    // 290
    SFX_CZOMBA,                    // 290
    SFX_CZOMBH,                    // 290
    SFX_CSKELS,                    // 290
    SFX_CSKELD,                    // 292
    SFX_CSKELA,                    // 291
    SFX_CBATS,                     // 291
    SFX_CMAGES,                    // 293
    SFX_CMAGED,                    // 294
    SFX_CTROLS,                    // 293
    SFX_CTROLA,                    // 294
    SFX_CTROLH,
    SFX_CTRLSR,
    SFX_CTRLSL,
    SFX_CDEMNS,                    // 293
    SFX_CDEMNA,                    // 294
    SFX_CEYED,                     // 289
    SFX_CEYESH,                    // 294
    SFX_CEYEST,                    // 294
    SFX_CSNEMR,
    SFX_CCHSTO,                    // 289
    SFX_OBIOBS,                    // 234
    SFX_OBIOBD,                    // 235
    SFX_OBISHT,                    // 236
    SFX_ORVSIT,                    // 234
    SFX_ORVDTH,                    // 235
    SFX_OMDOCS,                    // 236
    SFX_OMDOCD,                    // 237
    SFX_OMUSIT,                    // 238
    SFX_OMUDTH,                    // 239
    SFX_OMOSIT,                    // 240
    SFX_OMODTH,                    // 240
    SFX_OMOPAI,                    // 241
    SFX_OMOACT,                    // 242
    SFX_OMGRDS,                    // 243
    SFX_OMGRDD,                    // 244
    SFX_ODIRTA,                    // 245
    SFX_OAGENS,                    // 246
    SFX_OAGEND,                    // 247
    SFX_OSFOXS,                    // 248
    SFX_OSFOXD,                    // 249
    SFX_OBALRD,                    // 250
    SFX_ONEMES,                    // 251
    SFX_ONEMED,                    // 252
    SFX_OMTANS,                    // 253
    SFX_OMTAND,                    // 254
    SFX_SDMGYS,                    // 255
    SFX_SCZOMS,                    // 255
    SFX_SCZOMD,                    // 255
    SFX_SMUNTD,
    SFX_SSPSIT,                    // 256
    SFX_SSPATK,                    // 257
    SFX_SSPSUR,                    // 258
    SFX_SSPDTH,                    // 259
    SFX_SPTFIR,
    SFX_SPTXPL,                    // 266
    SFX_SARMRS,                    // 260
    SFX_STANKS,                    // 263
    SFX_STANKD,                    // 264
    SFX_SPHANT,                    // 262
    SFX_SDMSCS,                    // 265
    SFX_SDMSCD,                    // 266
    SFX_HLGRDS,
    SFX_HLGRDD,
    SFX_HLGRDP,
    SFX_UGRBLS,                    // 255
    SFX_UGRBD1,                    // 255
    SFX_UGRBD2,                    // 255
    SFX_UGRBLA,                    // 255
    SFX_URDBLS,                    // 255
    SFX_URDBLD,                    // 255
    SFX_URDBLA,                    // 255
    SFX_UPRBLS,                    // 255
    SFX_UPRBLD,                    // 255
    SFX_UMTBLS,                    // 255
    SFX_UMTBLD,                    // 255
    SFX_ALIGH,                     // 271
    SFX_ALIGH2,                    // 272
    SFX_ALORRY,                    // 273
    SFX_ADROP,                     // 274
    SFX_ADROP2,                    // 275
    SFX_AWIND,                     // 276
    SFX_AWOLF,                     // 277
    SFX_ACRICK,                    // 278
    SFX_ABIRD1,                    // 279
    SFX_ABIRD2,                    // 280
    SFX_ACOMP1,                    // 281
    SFX_ACOMP2,                    // 282
    SFX_ADOORC,                    // 283
    SFX_AROCK,                     // 284
    SFX_AROCK2,                    // 285
    SFX_AFROGS,                    // 286
    SFX_ASCREE,                    // 287
    SFX_AWATER,                    // 288
    SFX_AWATR2,
    SFX_ARAIN,
    SFX_AOWL,                      // 289
    NUMSFX                         // 295
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
    MUS_MAP01,                     // 033
    MUS_MAP02,                     // 034
    MUS_MAP03,                     // 035
    MUS_MAP04,                     // 036
    MUS_MAP05,                     // 037
    MUS_MAP06,                     // 038
    MUS_MAP07,                     // 039
    MUS_MAP08,                     // 040
    MUS_MAP09,                     // 041
    MUS_MAP10,                     // 042
    MUS_MAP11,                     // 043
    MUS_MAP12,                     // 044
    MUS_MAP13,                     // 045
    MUS_MAP14,                     // 046
    MUS_MAP15,                     // 047
    MUS_MAP16,                     // 048
    MUS_MAP17,                     // 049
    MUS_MAP18,                     // 050
    MUS_MAP19,                     // 051
    MUS_MAP20,                     // 052
    MUS_MAP21,                     // 053
    MUS_MAP22,                     // 054
    MUS_MAP23,                     // 055
    MUS_MAP24,                     // 056
    MUS_MAP25,                     // 057
    MUS_MAP26,                     // 058
    MUS_MAP27,                     // 059
    MUS_MAP28,                     // 060
    MUS_MAP29,                     // 061
    MUS_MAP30,                     // 062
    MUS_MAP31,                     // 063
    MUS_MAP32,                     // 064
    MUS_MAP33,                     // 065
    MUS_MAP34,                     // 066
    MUS_MAP35,                     // 067
    MUS_MAP36,                     // 068
    MUS_MAP37,                     // 069
    MUS_MAP38,                     // 070
    MUS_MAP39,                     // 071
    MUS_MAP40,                     // 072
    MUS_MAP41,                     // 073
    MUS_MAP42,                     // 074
    MUS_MAP43,                     // 075
    MUS_MAP44,                     // 076
    MUS_MAP45,                     // 077
    MUS_MAP46,                     // 078
    MUS_MAP47,                     // 079
    MUS_MAP48,                     // 080
    MUS_MAP49,                     // 081
    MUS_MAP50,                     // 082
    MUS_MAP51,                     // 083
    MUS_MAP52,                     // 084
    MUS_MAP53,                     // 085
    MUS_MAP54,                     // 086
    MUS_MAP55,                     // 087
    MUS_MAP56,                     // 088
    MUS_MAP57,                     // 089
    MUS_MAP58,                     // 090
    MUS_MAP59,                     // 091
    MUS_MAP60,                     // 092
    MUS_MAP61,                     // 093
    MUS_MAP62,                     // 094
    MUS_MAP63,                     // 095
    MUS_MAP64,                     // 096
    MUS_MAP65,                     // 097
    MUS_MAP66,                     // 098
    MUS_MAP67,                     // 099
    MUS_MAP68,                     // 100
    MUS_MAP69,                     // 101
    MUS_MAP70,                     // 102
    MUS_MAP71,                     // 103
    MUS_MAP72,                     // 104
    MUS_MAP73,                     // 105
    MUS_MAP74,                     // 106
    MUS_MAP75,                     // 107
    MUS_MAP76,                     // 108
    MUS_MAP77,                     // 109
    MUS_MAP78,                     // 110
    MUS_MAP79,                     // 111
    MUS_MAP80,                     // 112
    MUS_MAP81,                     // 113
    MUS_MAP82,                     // 114
    MUS_MAP83,                     // 115
    MUS_MAP84,                     // 116
    MUS_MAP85,                     // 117
    MUS_MAP86,                     // 116
    MUS_MAP87,                     // 117
    MUS_MAP88,                     // 117
    MUS_MAP89,                     // 112
    MUS_MAP90,                     // 113
    MUS_MAP91,                     // 114
    MUS_MAP92,                     // 115
    MUS_MAP93,                     // 116
    MUS_MAP94,                     // 117
    MUS_MAP95,                     // 116
    MUS_MAP96,                     // 117
    MUS_MAP97,                     // 117
    MUS_MAP98,                     // 117
    MUS_MAP99,                     // 117
    MUS_WLFTTL,                    // 118
    MUS_WLFINT,                    // 119
    MUS_WLFSPL,                    // 120
    MUS_WLFEND,                    // 121
    MUS_EP3END,                    // 118
    MUS_SODI,                      // 119
    MUS_SODEND,                    // 120
    MUS_SODM3I,
    MUS_CATEND,                    // 121
    MUS_CONSINT,                       // 121
    MUS_BOSS1,                     // 118
    MUS_BOSS2,                     // 119
    MUS_BOSS3,                     // 120
    MUS_BOSS4,                     // 121
    NUMMUSIC
} musicenum_t;

#endif
