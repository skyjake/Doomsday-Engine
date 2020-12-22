/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * info.h: Sprite, state, mobjtype and text identifiers.
 */

#ifndef __INFO_CONSTANTS_H__
#define __INFO_CONSTANTS_H__

// Sprites.
typedef enum {
    SPR_MAN1,                       // 000
    SPR_ACLO,                       // 001
    SPR_TLGL,                       // 002
    SPR_FBL1,                       // 003
    SPR_XPL1,                       // 004
    SPR_ARRW,                       // 005
    SPR_DART,                       // 006
    SPR_RIPP,                       // 007
    SPR_CFCF,                       // 008
    SPR_BLAD,                       // 009
    SPR_SHRD,                       // 010
    SPR_FFSM,                       // 011
    SPR_FFLG,                       // 012
    SPR_PTN1,                       // 013
    SPR_PTN2,                       // 014
    SPR_SOAR,                       // 015
    SPR_INVU,                       // 016
    SPR_SUMN,                       // 017
    SPR_TSPK,                       // 018
    SPR_TELO,                       // 019
    SPR_TRNG,                       // 020
    SPR_ROCK,                       // 021
    SPR_FOGS,                       // 022
    SPR_FOGM,                       // 023
    SPR_FOGL,                       // 024
    SPR_SGSA,                       // 025
    SPR_SGSB,                       // 026
    SPR_PORK,                       // 027
    SPR_EGGM,                       // 028
    SPR_FHFX,                       // 029
    SPR_SPHL,                       // 030
    SPR_STWN,                       // 031
    SPR_GMPD,                       // 032
    SPR_ASKU,                       // 033
    SPR_ABGM,                       // 034
    SPR_AGMR,                       // 035
    SPR_AGMG,                       // 036
    SPR_AGG2,                       // 037
    SPR_AGMB,                       // 038
    SPR_AGB2,                       // 039
    SPR_ABK1,                       // 040
    SPR_ABK2,                       // 041
    SPR_ASK2,                       // 042
    SPR_AFWP,                       // 043
    SPR_ACWP,                       // 044
    SPR_AMWP,                       // 045
    SPR_AGER,                       // 046
    SPR_AGR2,                       // 047
    SPR_AGR3,                       // 048
    SPR_AGR4,                       // 049
    SPR_TRCH,                       // 050
    SPR_PSBG,                       // 051
    SPR_ATLP,                       // 052
    SPR_THRW,                       // 053
    SPR_SPED,                       // 054
    SPR_BMAN,                       // 055
    SPR_BRAC,                       // 056
    SPR_BLST,                       // 057
    SPR_HRAD,                       // 058
    SPR_SPSH,                       // 059
    SPR_LVAS,                       // 060
    SPR_SLDG,                       // 061
    SPR_STTW,                       // 062
    SPR_RCK1,                       // 063
    SPR_RCK2,                       // 064
    SPR_RCK3,                       // 065
    SPR_RCK4,                       // 066
    SPR_CDLR,                       // 067
    SPR_TRE1,                       // 068
    SPR_TRDT,                       // 069
    SPR_TRE2,                       // 070
    SPR_TRE3,                       // 071
    SPR_STM1,                       // 072
    SPR_STM2,                       // 073
    SPR_STM3,                       // 074
    SPR_STM4,                       // 075
    SPR_MSH1,                       // 076
    SPR_MSH2,                       // 077
    SPR_MSH3,                       // 078
    SPR_MSH4,                       // 079
    SPR_MSH5,                       // 080
    SPR_MSH6,                       // 081
    SPR_MSH7,                       // 082
    SPR_MSH8,                       // 083
    SPR_SGMP,                       // 084
    SPR_SGM1,                       // 085
    SPR_SGM2,                       // 086
    SPR_SGM3,                       // 087
    SPR_SLC1,                       // 088
    SPR_SLC2,                       // 089
    SPR_SLC3,                       // 090
    SPR_MSS1,                       // 091
    SPR_MSS2,                       // 092
    SPR_SWMV,                       // 093
    SPR_CPS1,                       // 094
    SPR_CPS2,                       // 095
    SPR_TMS1,                       // 096
    SPR_TMS2,                       // 097
    SPR_TMS3,                       // 098
    SPR_TMS4,                       // 099
    SPR_TMS5,                       // 100
    SPR_TMS6,                       // 101
    SPR_TMS7,                       // 102
    SPR_CPS3,                       // 103
    SPR_STT2,                       // 104
    SPR_STT3,                       // 105
    SPR_STT4,                       // 106
    SPR_STT5,                       // 107
    SPR_GAR1,                       // 108
    SPR_GAR2,                       // 109
    SPR_GAR3,                       // 110
    SPR_GAR4,                       // 111
    SPR_GAR5,                       // 112
    SPR_GAR6,                       // 113
    SPR_GAR7,                       // 114
    SPR_GAR8,                       // 115
    SPR_GAR9,                       // 116
    SPR_BNR1,                       // 117
    SPR_TRE4,                       // 118
    SPR_TRE5,                       // 119
    SPR_TRE6,                       // 120
    SPR_TRE7,                       // 121
    SPR_LOGG,                       // 122
    SPR_ICT1,                       // 123
    SPR_ICT2,                       // 124
    SPR_ICT3,                       // 125
    SPR_ICT4,                       // 126
    SPR_ICM1,                       // 127
    SPR_ICM2,                       // 128
    SPR_ICM3,                       // 129
    SPR_ICM4,                       // 130
    SPR_RKBL,                       // 131
    SPR_RKBS,                       // 132
    SPR_RKBK,                       // 133
    SPR_RBL1,                       // 134
    SPR_RBL2,                       // 135
    SPR_RBL3,                       // 136
    SPR_VASE,                       // 137
    SPR_POT1,                       // 138
    SPR_POT2,                       // 139
    SPR_POT3,                       // 140
    SPR_PBIT,                       // 141
    SPR_CPS4,                       // 142
    SPR_CPS5,                       // 143
    SPR_CPS6,                       // 144
    SPR_CPB1,                       // 145
    SPR_CPB2,                       // 146
    SPR_CPB3,                       // 147
    SPR_CPB4,                       // 148
    SPR_BDRP,                       // 149
    SPR_BDSH,                       // 150
    SPR_BDPL,                       // 151
    SPR_CNDL,                       // 152
    SPR_LEF1,                       // 153
    SPR_LEF3,                       // 154
    SPR_LEF2,                       // 155
    SPR_TWTR,                       // 156
    SPR_WLTR,                       // 157
    SPR_BARL,                       // 158
    SPR_SHB1,                       // 159
    SPR_SHB2,                       // 160
    SPR_BCKT,                       // 161
    SPR_SHRM,                       // 162
    SPR_FBUL,                       // 163
    SPR_FSKL,                       // 164
    SPR_BRTR,                       // 165
    SPR_SUIT,                       // 166
    SPR_BBLL,                       // 167
    SPR_CAND,                       // 168
    SPR_IRON,                       // 169
    SPR_XMAS,                       // 170
    SPR_CDRN,                       // 171
    SPR_CHNS,                       // 172
    SPR_TST1,                       // 173
    SPR_TST2,                       // 174
    SPR_TST3,                       // 175
    SPR_TST4,                       // 176
    SPR_TST5,                       // 177
    SPR_TST6,                       // 178
    SPR_TST7,                       // 179
    SPR_TST8,                       // 180
    SPR_TST9,                       // 181
    SPR_TST0,                       // 182
    SPR_TELE,                       // 183
    SPR_TSMK,                       // 184
    SPR_FPCH,                       // 185
    SPR_WFAX,                       // 186
    SPR_FAXE,                       // 187
    SPR_WFHM,                       // 188
    SPR_FHMR,                       // 189
    SPR_FSRD,                       // 190
    SPR_FSFX,                       // 191
    SPR_CMCE,                       // 192
    SPR_WCSS,                       // 193
    SPR_CSSF,                       // 194
    SPR_WCFM,                       // 195
    SPR_CFLM,                       // 196
    SPR_CFFX,                       // 197
    SPR_CHLY,                       // 198
    SPR_SPIR,                       // 199
    SPR_MWND,                       // 200
    SPR_WMLG,                       // 201
    SPR_MLNG,                       // 202
    SPR_MLFX,                       // 203
    SPR_MLF2,                       // 204
    SPR_MSTF,                       // 205
    SPR_MSP1,                       // 206
    SPR_MSP2,                       // 207
    SPR_WFR1,                       // 208
    SPR_WFR2,                       // 209
    SPR_WFR3,                       // 210
    SPR_WCH1,                       // 211
    SPR_WCH2,                       // 212
    SPR_WCH3,                       // 213
    SPR_WMS1,                       // 214
    SPR_WMS2,                       // 215
    SPR_WMS3,                       // 216
    SPR_WPIG,                       // 217
    SPR_WMCS,                       // 218
    SPR_CONE,                       // 219
    SPR_SHEX,                       // 220
    SPR_BLOD,                       // 221
    SPR_GIBS,                       // 222
    SPR_PLAY,                       // 223
    SPR_FDTH,                       // 224
    SPR_BSKL,                       // 225
    SPR_ICEC,                       // 226
    SPR_CLER,                       // 227
    SPR_MAGE,                       // 228
    SPR_PIGY,                       // 229
    SPR_CENT,                       // 230
    SPR_CTXD,                       // 231
    SPR_CTFX,                       // 232
    SPR_CTDP,                       // 233
    SPR_DEMN,                       // 234
    SPR_DEMA,                       // 235
    SPR_DEMB,                       // 236
    SPR_DEMC,                       // 237
    SPR_DEMD,                       // 238
    SPR_DEME,                       // 239
    SPR_DMFX,                       // 240
    SPR_DEM2,                       // 241
    SPR_DMBA,                       // 242
    SPR_DMBB,                       // 243
    SPR_DMBC,                       // 244
    SPR_DMBD,                       // 245
    SPR_DMBE,                       // 246
    SPR_D2FX,                       // 247
    SPR_WRTH,                       // 248
    SPR_WRT2,                       // 249
    SPR_WRBL,                       // 250
    SPR_MNTR,                       // 251
    SPR_FX12,                       // 252
    SPR_FX13,                       // 253
    SPR_MNSM,                       // 254
    SPR_SSPT,                       // 255
    SPR_SSDV,                       // 256
    SPR_SSXD,                       // 257
    SPR_SSFX,                       // 258
    SPR_BISH,                       // 259
    SPR_BPFX,                       // 260
    SPR_DRAG,                       // 261
    SPR_DRFX,                       // 262
    SPR_ARM1,                       // 263
    SPR_ARM2,                       // 264
    SPR_ARM3,                       // 265
    SPR_ARM4,                       // 266
    SPR_MAN2,                       // 267
    SPR_MAN3,                       // 268
    SPR_KEY1,                       // 269
    SPR_KEY2,                       // 270
    SPR_KEY3,                       // 271
    SPR_KEY4,                       // 272
    SPR_KEY5,                       // 273
    SPR_KEY6,                       // 274
    SPR_KEY7,                       // 275
    SPR_KEY8,                       // 276
    SPR_KEY9,                       // 277
    SPR_KEYA,                       // 278
    SPR_KEYB,                       // 279
    SPR_ETTN,                       // 280
    SPR_ETTB,                       // 281
    SPR_FDMN,                       // 282
    SPR_FDMB,                       // 283
    SPR_ICEY,                       // 284
    SPR_ICPR,                       // 285
    SPR_ICWS,                       // 286
    SPR_SORC,                       // 287
    SPR_SBMP,                       // 288
    SPR_SBS4,                       // 289
    SPR_SBMB,                       // 290
    SPR_SBS3,                       // 291
    SPR_SBMG,                       // 292
    SPR_SBS1,                       // 293
    SPR_SBS2,                       // 294
    SPR_SBFX,                       // 295
    SPR_RADE,                       // 296
    SPR_WATR,                       // 297
    SPR_KORX,                       // 298
    SPR_ABAT,                       // 299
    NUMSPRITES                       // 300
} spritetype_e;

// States.
typedef enum {
    S_NULL,                           // 000
    S_FREETARGMOBJ,                   // 001
    S_MAPSPOT,                       // 002
    S_FIREBALL1_1,                   // 003
    S_FIREBALL1_2,                   // 004
    S_FIREBALL1_X1,                   // 005
    S_FIREBALL1_X2,                   // 006
    S_FIREBALL1_X3,                   // 007
    S_FIREBALL1_X4,                   // 008
    S_FIREBALL1_X5,                   // 009
    S_FIREBALL1_X6,                   // 010
    S_ARROW_1,                       // 011
    S_ARROW_X1,                       // 012
    S_DART_1,                       // 013
    S_DART_X1,                       // 014
    S_POISONDART_1,                   // 015
    S_POISONDART_X1,               // 016
    S_RIPPERBALL_1,                   // 017
    S_RIPPERBALL_2,                   // 018
    S_RIPPERBALL_3,                   // 019
    S_RIPPERBALL_X1,               // 020
    S_RIPPERBALL_X2,               // 021
    S_RIPPERBALL_X3,               // 022
    S_RIPPERBALL_X4,               // 023
    S_RIPPERBALL_X5,               // 024
    S_RIPPERBALL_X6,               // 025
    S_RIPPERBALL_X7,               // 026
    S_RIPPERBALL_X8,               // 027
    S_RIPPERBALL_X9,               // 028
    S_RIPPERBALL_X10,               // 029
    S_PRJ_BLADE1,                   // 030
    S_PRJ_BLADE_X1,                   // 031
    S_ICESHARD1,                   // 032
    S_ICESHARD2,                   // 033
    S_ICESHARD3,                   // 034
    S_FLAME_TSMALL1,               // 035
    S_FLAME_TSMALL2,               // 036
    S_FLAME_TSMALL3,               // 037
    S_FLAME_TSMALL4,               // 038
    S_FLAME_TSMALL5,               // 039
    S_FLAME_TSMALL6,               // 040
    S_FLAME_TLARGE1,               // 041
    S_FLAME_TLARGE2,               // 042
    S_FLAME_TLARGE3,               // 043
    S_FLAME_TLARGE4,               // 044
    S_FLAME_TLARGE5,               // 045
    S_FLAME_TLARGE6,               // 046
    S_FLAME_TLARGE7,               // 047
    S_FLAME_TLARGE8,               // 048
    S_FLAME_TLARGE9,               // 049
    S_FLAME_TLARGE10,               // 050
    S_FLAME_TLARGE11,               // 051
    S_FLAME_TLARGE12,               // 052
    S_FLAME_TLARGE13,               // 053
    S_FLAME_TLARGE14,               // 054
    S_FLAME_TLARGE15,               // 055
    S_FLAME_TLARGE16,               // 056
    S_FLAME_SDORM1,                   // 057
    S_FLAME_SDORM2,                   // 058
    S_FLAME_SDORM3,                   // 059
    S_FLAME_SMALL1,                   // 060
    S_FLAME_SMALL2,                   // 061
    S_FLAME_SMALL3,                   // 062
    S_FLAME_SMALL4,                   // 063
    S_FLAME_SMALL5,                   // 064
    S_FLAME_SMALL6,                   // 065
    S_FLAME_SMALL7,                   // 066
    S_FLAME_LDORM1,                   // 067
    S_FLAME_LDORM2,                   // 068
    S_FLAME_LDORM3,                   // 069
    S_FLAME_LDORM4,                   // 070
    S_FLAME_LDORM5,                   // 071
    S_FLAME_LARGE1,                   // 072
    S_FLAME_LARGE2,                   // 073
    S_FLAME_LARGE3,                   // 074
    S_FLAME_LARGE4,                   // 075
    S_FLAME_LARGE5,                   // 076
    S_FLAME_LARGE6,                   // 077
    S_FLAME_LARGE7,                   // 078
    S_FLAME_LARGE8,                   // 079
    S_FLAME_LARGE9,                   // 080
    S_FLAME_LARGE10,               // 081
    S_FLAME_LARGE11,               // 082
    S_FLAME_LARGE12,               // 083
    S_FLAME_LARGE13,               // 084
    S_FLAME_LARGE14,               // 085
    S_FLAME_LARGE15,               // 086
    S_FLAME_LARGE16,               // 087
    S_FLAME_LARGE17,               // 088
    S_FLAME_LARGE18,               // 089
    S_ITEM_PTN1_1,                   // 090
    S_ITEM_PTN1_2,                   // 091
    S_ITEM_PTN1_3,                   // 092
    S_HIDESPECIAL1,                   // 093
    S_HIDESPECIAL2,                   // 094
    S_HIDESPECIAL3,                   // 095
    S_HIDESPECIAL4,                   // 096
    S_HIDESPECIAL5,                   // 097
    S_HIDESPECIAL6,                   // 098
    S_HIDESPECIAL7,                   // 099
    S_HIDESPECIAL8,                   // 100
    S_HIDESPECIAL9,                   // 101
    S_HIDESPECIAL10,               // 102
    S_HIDESPECIAL11,               // 103
    S_DORMANTARTI1_1,               // 104
    S_DORMANTARTI1_2,               // 105
    S_DORMANTARTI1_3,               // 106
    S_DORMANTARTI1_4,               // 107
    S_DORMANTARTI1_5,               // 108
    S_DORMANTARTI1_6,               // 109
    S_DORMANTARTI1_7,               // 110
    S_DORMANTARTI1_8,               // 111
    S_DORMANTARTI1_9,               // 112
    S_DORMANTARTI1_10,               // 113
    S_DORMANTARTI1_11,               // 114
    S_DORMANTARTI1_12,               // 115
    S_DORMANTARTI1_13,               // 116
    S_DORMANTARTI1_14,               // 117
    S_DORMANTARTI1_15,               // 118
    S_DORMANTARTI1_16,               // 119
    S_DORMANTARTI1_17,               // 120
    S_DORMANTARTI1_18,               // 121
    S_DORMANTARTI1_19,               // 122
    S_DORMANTARTI1_20,               // 123
    S_DORMANTARTI1_21,               // 124
    S_DORMANTARTI2_1,               // 125
    S_DORMANTARTI2_2,               // 126
    S_DORMANTARTI2_3,               // 127
    S_DORMANTARTI2_4,               // 128
    S_DORMANTARTI2_5,               // 129
    S_DORMANTARTI2_6,               // 130
    S_DORMANTARTI2_7,               // 131
    S_DORMANTARTI2_8,               // 132
    S_DORMANTARTI2_9,               // 133
    S_DORMANTARTI2_10,               // 134
    S_DORMANTARTI2_11,               // 135
    S_DORMANTARTI2_12,               // 136
    S_DORMANTARTI2_13,               // 137
    S_DORMANTARTI2_14,               // 138
    S_DORMANTARTI2_15,               // 139
    S_DORMANTARTI2_16,               // 140
    S_DORMANTARTI2_17,               // 141
    S_DORMANTARTI2_18,               // 142
    S_DORMANTARTI2_19,               // 143
    S_DORMANTARTI2_20,               // 144
    S_DORMANTARTI2_21,               // 145
    S_DORMANTARTI3_1,               // 146
    S_DORMANTARTI3_2,               // 147
    S_DORMANTARTI3_3,               // 148
    S_DORMANTARTI3_4,               // 149
    S_DORMANTARTI3_5,               // 150
    S_DORMANTARTI3_6,               // 151
    S_DORMANTARTI3_7,               // 152
    S_DORMANTARTI3_8,               // 153
    S_DORMANTARTI3_9,               // 154
    S_DORMANTARTI3_10,               // 155
    S_DORMANTARTI3_11,               // 156
    S_DORMANTARTI3_12,               // 157
    S_DORMANTARTI3_13,               // 158
    S_DORMANTARTI3_14,               // 159
    S_DORMANTARTI3_15,               // 160
    S_DORMANTARTI3_16,               // 161
    S_DORMANTARTI3_17,               // 162
    S_DORMANTARTI3_18,               // 163
    S_DORMANTARTI3_19,               // 164
    S_DORMANTARTI3_20,               // 165
    S_DORMANTARTI3_21,               // 166
    S_DEADARTI1,                   // 167
    S_DEADARTI2,                   // 168
    S_DEADARTI3,                   // 169
    S_DEADARTI4,                   // 170
    S_DEADARTI5,                   // 171
    S_DEADARTI6,                   // 172
    S_DEADARTI7,                   // 173
    S_DEADARTI8,                   // 174
    S_DEADARTI9,                   // 175
    S_DEADARTI10,                   // 176
    S_ARTI_PTN2_1,                   // 177
    S_ARTI_PTN2_2,                   // 178
    S_ARTI_PTN2_3,                   // 179
    S_ARTI_SOAR1,                   // 180
    S_ARTI_SOAR2,                   // 181
    S_ARTI_SOAR3,                   // 182
    S_ARTI_SOAR4,                   // 183
    S_ARTI_INVU1,                   // 184
    S_ARTI_INVU2,                   // 185
    S_ARTI_INVU3,                   // 186
    S_ARTI_INVU4,                   // 187
    S_ARTI_SUMMON,                   // 188
    S_SUMMON_FX1_1,                   // 189
    S_SUMMON_FX2_1,                   // 190
    S_SUMMON_FX2_2,                   // 191
    S_SUMMON_FX2_3,                   // 192
    S_THRUSTINIT2_1,               // 193
    S_THRUSTINIT2_2,               // 194
    S_BTHRUSTINIT2_1,               // 195
    S_BTHRUSTINIT2_2,               // 196
    S_THRUSTINIT1_1,               // 197
    S_THRUSTINIT1_2,               // 198
    S_BTHRUSTINIT1_1,               // 199
    S_BTHRUSTINIT1_2,               // 200
    S_THRUSTRAISE1,                   // 201
    S_THRUSTRAISE2,                   // 202
    S_THRUSTRAISE3,                   // 203
    S_THRUSTRAISE4,                   // 204
    S_BTHRUSTRAISE1,               // 205
    S_BTHRUSTRAISE2,               // 206
    S_BTHRUSTRAISE3,               // 207
    S_BTHRUSTRAISE4,               // 208
    S_THRUSTIMPALE,                   // 209
    S_BTHRUSTIMPALE,               // 210
    S_THRUSTRAISE,                   // 211
    S_BTHRUSTRAISE,                   // 212
    S_THRUSTBLOCK,                   // 213
    S_BTHRUSTBLOCK,                   // 214
    S_THRUSTLOWER,                   // 215
    S_BTHRUSTLOWER,                   // 216
    S_THRUSTSTAY,                   // 217
    S_BTHRUSTSTAY,                   // 218
    S_ARTI_TELOTHER1,               // 219
    S_ARTI_TELOTHER2,               // 220
    S_ARTI_TELOTHER3,               // 221
    S_ARTI_TELOTHER4,               // 222
    S_TELO_FX1,                       // 223
    S_TELO_FX2,                       // 224
    S_TELO_FX3,                       // 225
    S_TELO_FX4,                       // 226
    S_TELO_FX5,                       // 227
    S_TELO_FX6,                       // 228
    S_TELO_FX7,                       // 229
    S_TELO_FX8,                       // 230
    S_TELO_FX9,                       // 231
    S_TELO_FX2_1,                   // 232
    S_TELO_FX2_2,                   // 233
    S_TELO_FX2_3,                   // 234
    S_TELO_FX2_4,                   // 235
    S_TELO_FX2_5,                   // 236
    S_TELO_FX2_6,                   // 237
    S_TELO_FX3_1,                   // 238
    S_TELO_FX3_2,                   // 239
    S_TELO_FX3_3,                   // 240
    S_TELO_FX3_4,                   // 241
    S_TELO_FX3_5,                   // 242
    S_TELO_FX3_6,                   // 243
    S_TELO_FX4_1,                   // 244
    S_TELO_FX4_2,                   // 245
    S_TELO_FX4_3,                   // 246
    S_TELO_FX4_4,                   // 247
    S_TELO_FX4_5,                   // 248
    S_TELO_FX4_6,                   // 249
    S_TELO_FX5_1,                   // 250
    S_TELO_FX5_2,                   // 251
    S_TELO_FX5_3,                   // 252
    S_TELO_FX5_4,                   // 253
    S_TELO_FX5_5,                   // 254
    S_TELO_FX5_6,                   // 255
    S_DIRT1_1,                       // 256
    S_DIRT1_D,                       // 257
    S_DIRT2_1,                       // 258
    S_DIRT2_D,                       // 259
    S_DIRT3_1,                       // 260
    S_DIRT3_D,                       // 261
    S_DIRT4_1,                       // 262
    S_DIRT4_D,                       // 263
    S_DIRT5_1,                       // 264
    S_DIRT5_D,                       // 265
    S_DIRT6_1,                       // 266
    S_DIRT6_D,                       // 267
    S_DIRTCLUMP1,                   // 268
    S_ROCK1_1,                       // 269
    S_ROCK1_D,                       // 270
    S_ROCK2_1,                       // 271
    S_ROCK2_D,                       // 272
    S_ROCK3_1,                       // 273
    S_ROCK3_D,                       // 274
    S_SPAWNFOG1,                   // 275
    S_FOGPATCHS1,                   // 276
    S_FOGPATCHS2,                   // 277
    S_FOGPATCHS3,                   // 278
    S_FOGPATCHS4,                   // 279
    S_FOGPATCHS5,                   // 280
    S_FOGPATCHS0,                   // 281
    S_FOGPATCHM1,                   // 282
    S_FOGPATCHM2,                   // 283
    S_FOGPATCHM3,                   // 284
    S_FOGPATCHM4,                   // 285
    S_FOGPATCHM5,                   // 286
    S_FOGPATCHM0,                   // 287
    S_FOGPATCHMA,                   // 288
    S_FOGPATCHMB,                   // 289
    S_FOGPATCHMC,                   // 290
    S_FOGPATCHMD,                   // 291
    S_FOGPATCHL1,                   // 292
    S_FOGPATCHL2,                   // 293
    S_FOGPATCHL3,                   // 294
    S_FOGPATCHL4,                   // 295
    S_FOGPATCHL5,                   // 296
    S_FOGPATCHL0,                   // 297
    S_FOGPATCHLA,                   // 298
    S_FOGPATCHLB,                   // 299
    S_FOGPATCHLC,                   // 300
    S_FOGPATCHLD,                   // 301
    S_QUAKE_ACTIVE1,               // 302
    S_QUAKE_ACTIVE2,               // 303
    S_QUAKE_ACTIVE3,               // 304
    S_QUAKE_ACTIVE4,               // 305
    S_QUAKE_ACTIVE5,               // 306
    S_QUAKE_ACTIVE6,               // 307
    S_QUAKE_ACTIVE7,               // 308
    S_QUAKE_ACTIVE8,               // 309
    S_QUAKE_ACTIVE9,               // 310
    S_QUAKE_ACTIVE0,               // 311
    S_QUAKE_ACTIVEA,               // 312
    S_QUAKE_ACTIVEB,               // 313
    S_QUAKE_ACTIVEC,               // 314
    S_QUAKE_ACTIVED,               // 315
    S_QUAKE_ACTIVEE,               // 316
    S_QUAKE_ACTIVEF,               // 317
    S_QUAKE_ACTIVEG,               // 318
    S_QUAKE_ACTIVEH,               // 319
    S_QUAKE_ACTIVEI,               // 320
    S_QUAKE_ACTIVEJ,               // 321
    S_QUAKE_ACTIVEK,               // 322
    S_QUAKE_ACTIVEL,               // 323
    S_QUAKE_ACTIVEM,               // 324
    S_QUAKE_ACTIVEN,               // 325
    S_QUAKE_ACTIVEO,               // 326
    S_QUAKE_ACTIVEP,               // 327
    S_QUAKE_ACTIVEQ,               // 328
    S_QUAKE_ACTIVER,               // 329
    S_QUAKE_ACTIVES,               // 330
    S_QUAKE_ACTIVET,               // 331
    S_QUAKE_ACTIVEU,               // 332
    S_QUAKE_ACTIVEV,               // 333
    S_QUAKE_ACTIVEW,               // 334
    S_QUAKE_ACTIVEX,               // 335
    S_QUAKE_ACTIVEY,               // 336
    S_QUAKE_ACTIVEZ,               // 337
    S_QUAKE_ACT1,                   // 338
    S_QUAKE_ACT2,                   // 339
    S_QUAKE_ACT3,                   // 340
    S_QUAKE_ACT4,                   // 341
    S_QUAKE_ACT5,                   // 342
    S_QUAKE_ACT6,                   // 343
    S_QUAKE_ACT7,                   // 344
    S_QUAKE_ACT8,                   // 345
    S_QUAKE_ACT9,                   // 346
    S_QUAKE_ACT0,                   // 347
    S_SGSHARD1_1,                   // 348
    S_SGSHARD1_2,                   // 349
    S_SGSHARD1_3,                   // 350
    S_SGSHARD1_4,                   // 351
    S_SGSHARD1_5,                   // 352
    S_SGSHARD1_D,                   // 353
    S_SGSHARD2_1,                   // 354
    S_SGSHARD2_2,                   // 355
    S_SGSHARD2_3,                   // 356
    S_SGSHARD2_4,                   // 357
    S_SGSHARD2_5,                   // 358
    S_SGSHARD2_D,                   // 359
    S_SGSHARD3_1,                   // 360
    S_SGSHARD3_2,                   // 361
    S_SGSHARD3_3,                   // 362
    S_SGSHARD3_4,                   // 363
    S_SGSHARD3_5,                   // 364
    S_SGSHARD3_D,                   // 365
    S_SGSHARD4_1,                   // 366
    S_SGSHARD4_2,                   // 367
    S_SGSHARD4_3,                   // 368
    S_SGSHARD4_4,                   // 369
    S_SGSHARD4_5,                   // 370
    S_SGSHARD4_D,                   // 371
    S_SGSHARD5_1,                   // 372
    S_SGSHARD5_2,                   // 373
    S_SGSHARD5_3,                   // 374
    S_SGSHARD5_4,                   // 375
    S_SGSHARD5_5,                   // 376
    S_SGSHARD5_D,                   // 377
    S_SGSHARD6_1,                   // 378
    S_SGSHARD6_D,                   // 379
    S_SGSHARD7_1,                   // 380
    S_SGSHARD7_D,                   // 381
    S_SGSHARD8_1,                   // 382
    S_SGSHARD8_D,                   // 383
    S_SGSHARD9_1,                   // 384
    S_SGSHARD9_D,                   // 385
    S_SGSHARD0_1,                   // 386
    S_SGSHARD0_D,                   // 387
    S_ARTI_EGGC1,                   // 388
    S_ARTI_EGGC2,                   // 389
    S_ARTI_EGGC3,                   // 390
    S_ARTI_EGGC4,                   // 391
    S_ARTI_EGGC5,                   // 392
    S_ARTI_EGGC6,                   // 393
    S_ARTI_EGGC7,                   // 394
    S_ARTI_EGGC8,                   // 395
    S_EGGFX1,                       // 396
    S_EGGFX2,                       // 397
    S_EGGFX3,                       // 398
    S_EGGFX4,                       // 399
    S_EGGFX5,                       // 400
    S_EGGFXI1_1,                   // 401
    S_EGGFXI1_2,                   // 402
    S_EGGFXI1_3,                   // 403
    S_EGGFXI1_4,                   // 404
    S_ARTI_SPHL1,                   // 405
    S_ZWINGEDSTATUENOSKULL,           // 406
    S_ZWINGEDSTATUENOSKULL2,       // 407
    S_ZGEMPEDESTAL1,               // 408
    S_ZGEMPEDESTAL2,               // 409
    S_ARTIPUZZSKULL,               // 410
    S_ARTIPUZZGEMBIG,               // 411
    S_ARTIPUZZGEMRED,               // 412
    S_ARTIPUZZGEMGREEN1,           // 413
    S_ARTIPUZZGEMGREEN2,           // 414
    S_ARTIPUZZGEMBLUE1,               // 415
    S_ARTIPUZZGEMBLUE2,               // 416
    S_ARTIPUZZBOOK1,               // 417
    S_ARTIPUZZBOOK2,               // 418
    S_ARTIPUZZSKULL2,               // 419
    S_ARTIPUZZFWEAPON,               // 420
    S_ARTIPUZZCWEAPON,               // 421
    S_ARTIPUZZMWEAPON,               // 422
    S_ARTIPUZZGEAR_1,               // 423
    S_ARTIPUZZGEAR_2,               // 424
    S_ARTIPUZZGEAR_3,               // 425
    S_ARTIPUZZGEAR_4,               // 426
    S_ARTIPUZZGEAR_5,               // 427
    S_ARTIPUZZGEAR_6,               // 428
    S_ARTIPUZZGEAR_7,               // 429
    S_ARTIPUZZGEAR_8,               // 430
    S_ARTIPUZZGEAR2_1,               // 431
    S_ARTIPUZZGEAR2_2,               // 432
    S_ARTIPUZZGEAR2_3,               // 433
    S_ARTIPUZZGEAR2_4,               // 434
    S_ARTIPUZZGEAR2_5,               // 435
    S_ARTIPUZZGEAR2_6,               // 436
    S_ARTIPUZZGEAR2_7,               // 437
    S_ARTIPUZZGEAR2_8,               // 438
    S_ARTIPUZZGEAR3_1,               // 439
    S_ARTIPUZZGEAR3_2,               // 440
    S_ARTIPUZZGEAR3_3,               // 441
    S_ARTIPUZZGEAR3_4,               // 442
    S_ARTIPUZZGEAR3_5,               // 443
    S_ARTIPUZZGEAR3_6,               // 444
    S_ARTIPUZZGEAR3_7,               // 445
    S_ARTIPUZZGEAR3_8,               // 446
    S_ARTIPUZZGEAR4_1,               // 447
    S_ARTIPUZZGEAR4_2,               // 448
    S_ARTIPUZZGEAR4_3,               // 449
    S_ARTIPUZZGEAR4_4,               // 450
    S_ARTIPUZZGEAR4_5,               // 451
    S_ARTIPUZZGEAR4_6,               // 452
    S_ARTIPUZZGEAR4_7,               // 453
    S_ARTIPUZZGEAR4_8,               // 454
    S_ARTI_TRCH1,                   // 455
    S_ARTI_TRCH2,                   // 456
    S_ARTI_TRCH3,                   // 457
    S_FIREBOMB1,                   // 458
    S_FIREBOMB2,                   // 459
    S_FIREBOMB3,                   // 460
    S_FIREBOMB4,                   // 461
    S_FIREBOMB5,                   // 462
    S_FIREBOMB6,                   // 463
    S_FIREBOMB7,                   // 464
    S_FIREBOMB8,                   // 465
    S_FIREBOMB9,                   // 466
    S_FIREBOMB10,                   // 467
    S_FIREBOMB11,                   // 468
    S_ARTI_ATLP1,                   // 469
    S_ARTI_ATLP2,                   // 470
    S_ARTI_ATLP3,                   // 471
    S_ARTI_ATLP4,                   // 472
    S_ARTI_PSBG1,                   // 473
    S_POISONBAG1,                   // 474
    S_POISONBAG2,                   // 475
    S_POISONBAG3,                   // 476
    S_POISONBAG4,                   // 477
    S_POISONCLOUD1,                   // 478
    S_POISONCLOUD2,                   // 479
    S_POISONCLOUD3,                   // 480
    S_POISONCLOUD4,                   // 481
    S_POISONCLOUD5,                   // 482
    S_POISONCLOUD6,                   // 483
    S_POISONCLOUD7,                   // 484
    S_POISONCLOUD8,                   // 485
    S_POISONCLOUD9,                   // 486
    S_POISONCLOUD10,               // 487
    S_POISONCLOUD11,               // 488
    S_POISONCLOUD12,               // 489
    S_POISONCLOUD13,               // 490
    S_POISONCLOUD14,               // 491
    S_POISONCLOUD15,               // 492
    S_POISONCLOUD16,               // 493
    S_POISONCLOUD17,               // 494
    S_POISONCLOUD18,               // 495
    S_POISONCLOUD_X1,               // 496
    S_POISONCLOUD_X2,               // 497
    S_POISONCLOUD_X3,               // 498
    S_POISONCLOUD_X4,               // 499
    S_THROWINGBOMB1,               // 500
    S_THROWINGBOMB2,               // 501
    S_THROWINGBOMB3,               // 502
    S_THROWINGBOMB4,               // 503
    S_THROWINGBOMB5,               // 504
    S_THROWINGBOMB6,               // 505
    S_THROWINGBOMB7,               // 506
    S_THROWINGBOMB8,               // 507
    S_THROWINGBOMB9,               // 508
    S_THROWINGBOMB10,               // 509
    S_THROWINGBOMB11,               // 510
    S_THROWINGBOMB12,               // 511
    S_THROWINGBOMB_X1,               // 512
    S_THROWINGBOMB_X2,               // 513
    S_THROWINGBOMB_X3,               // 514
    S_THROWINGBOMB_X4,               // 515
    S_THROWINGBOMB_X5,               // 516
    S_THROWINGBOMB_X6,               // 517
    S_THROWINGBOMB_X7,               // 518
    S_THROWINGBOMB_X8,               // 519
    S_ARTI_BOOTS1,                   // 520
    S_ARTI_BOOTS2,                   // 521
    S_ARTI_BOOTS3,                   // 522
    S_ARTI_BOOTS4,                   // 523
    S_ARTI_BOOTS5,                   // 524
    S_ARTI_BOOTS6,                   // 525
    S_ARTI_BOOTS7,                   // 526
    S_ARTI_BOOTS8,                   // 527
    S_ARTI_MANA,                   // 528
    S_ARTI_ARMOR1,                   // 529
    S_ARTI_ARMOR2,                   // 530
    S_ARTI_ARMOR3,                   // 531
    S_ARTI_ARMOR4,                   // 532
    S_ARTI_ARMOR5,                   // 533
    S_ARTI_ARMOR6,                   // 534
    S_ARTI_ARMOR7,                   // 535
    S_ARTI_ARMOR8,                   // 536
    S_ARTI_BLAST1,                   // 537
    S_ARTI_BLAST2,                   // 538
    S_ARTI_BLAST3,                   // 539
    S_ARTI_BLAST4,                   // 540
    S_ARTI_BLAST5,                   // 541
    S_ARTI_BLAST6,                   // 542
    S_ARTI_BLAST7,                   // 543
    S_ARTI_BLAST8,                   // 544
    S_ARTI_HEALRAD1,               // 545
    S_ARTI_HEALRAD2,               // 546
    S_ARTI_HEALRAD3,               // 547
    S_ARTI_HEALRAD4,               // 548
    S_ARTI_HEALRAD5,               // 549
    S_ARTI_HEALRAD6,               // 550
    S_ARTI_HEALRAD7,               // 551
    S_ARTI_HEALRAD8,               // 552
    S_ARTI_HEALRAD9,               // 553
    S_ARTI_HEALRAD0,               // 554
    S_ARTI_HEALRADA,               // 555
    S_ARTI_HEALRADB,               // 556
    S_ARTI_HEALRADC,               // 557
    S_ARTI_HEALRADD,               // 558
    S_ARTI_HEALRADE,               // 559
    S_ARTI_HEALRADF,               // 560
    S_SPLASH1,                       // 561
    S_SPLASH2,                       // 562
    S_SPLASH3,                       // 563
    S_SPLASH4,                       // 564
    S_SPLASHX,                       // 565
    S_SPLASHBASE1,                   // 566
    S_SPLASHBASE2,                   // 567
    S_SPLASHBASE3,                   // 568
    S_SPLASHBASE4,                   // 569
    S_SPLASHBASE5,                   // 570
    S_SPLASHBASE6,                   // 571
    S_SPLASHBASE7,                   // 572
    S_LAVASPLASH1,                   // 573
    S_LAVASPLASH2,                   // 574
    S_LAVASPLASH3,                   // 575
    S_LAVASPLASH4,                   // 576
    S_LAVASPLASH5,                   // 577
    S_LAVASPLASH6,                   // 578
    S_LAVASMOKE1,                   // 579
    S_LAVASMOKE2,                   // 580
    S_LAVASMOKE3,                   // 581
    S_LAVASMOKE4,                   // 582
    S_LAVASMOKE5,                   // 583
    S_SLUDGECHUNK1,                   // 584
    S_SLUDGECHUNK2,                   // 585
    S_SLUDGECHUNK3,                   // 586
    S_SLUDGECHUNK4,                   // 587
    S_SLUDGECHUNKX,                   // 588
    S_SLUDGESPLASH1,               // 589
    S_SLUDGESPLASH2,               // 590
    S_SLUDGESPLASH3,               // 591
    S_SLUDGESPLASH4,               // 592
    S_ZWINGEDSTATUE1,               // 593
    S_ZROCK1_1,                       // 594
    S_ZROCK2_1,                       // 595
    S_ZROCK3_1,                       // 596
    S_ZROCK4_1,                       // 597
    S_ZCHANDELIER1,                   // 598
    S_ZCHANDELIER2,                   // 599
    S_ZCHANDELIER3,                   // 600
    S_ZCHANDELIER_U,               // 601
    S_ZTREEDEAD1,                   // 602
    S_ZTREE,                       // 603
    S_ZTREEDESTRUCTIBLE1,           // 604
    S_ZTREEDES_D1,                   // 605
    S_ZTREEDES_D2,                   // 606
    S_ZTREEDES_D3,                   // 607
    S_ZTREEDES_D4,                   // 608
    S_ZTREEDES_D5,                   // 609
    S_ZTREEDES_D6,                   // 610
    S_ZTREEDES_X1,                   // 611
    S_ZTREEDES_X2,                   // 612
    S_ZTREEDES_X3,                   // 613
    S_ZTREEDES_X4,                   // 614
    S_ZTREEDES_X5,                   // 615
    S_ZTREEDES_X6,                   // 616
    S_ZTREEDES_X7,                   // 617
    S_ZTREEDES_X8,                   // 618
    S_ZTREEDES_X9,                   // 619
    S_ZTREEDES_X10,                   // 620
    S_ZTREESWAMP182_1,               // 621
    S_ZTREESWAMP172_1,               // 622
    S_ZSTUMPBURNED1,               // 623
    S_ZSTUMPBARE1,                   // 624
    S_ZSTUMPSWAMP1_1,               // 625
    S_ZSTUMPSWAMP2_1,               // 626
    S_ZSHROOMLARGE1_1,               // 627
    S_ZSHROOMLARGE2_1,               // 628
    S_ZSHROOMLARGE3_1,               // 629
    S_ZSHROOMSMALL1_1,               // 630
    S_ZSHROOMSMALL2_1,               // 631
    S_ZSHROOMSMALL3_1,               // 632
    S_ZSHROOMSMALL4_1,               // 633
    S_ZSHROOMSMALL5_1,               // 634
    S_ZSTALAGMITEPILLAR1,           // 635
    S_ZSTALAGMITELARGE1,           // 636
    S_ZSTALAGMITEMEDIUM1,           // 637
    S_ZSTALAGMITESMALL1,           // 638
    S_ZSTALACTITELARGE1,           // 639
    S_ZSTALACTITEMEDIUM1,           // 640
    S_ZSTALACTITESMALL1,           // 641
    S_ZMOSSCEILING1_1,               // 642
    S_ZMOSSCEILING2_1,               // 643
    S_ZSWAMPVINE1,                   // 644
    S_ZCORPSEKABOB1,               // 645
    S_ZCORPSESLEEPING1,               // 646
    S_ZTOMBSTONERIP1,               // 647
    S_ZTOMBSTONESHANE1,               // 648
    S_ZTOMBSTONEBIGCROSS1,           // 649
    S_ZTOMBSTONEBRIANR1,           // 650
    S_ZTOMBSTONECROSSCIRCLE1,       // 651
    S_ZTOMBSTONESMALLCROSS1,       // 652
    S_ZTOMBSTONEBRIANP1,           // 653
    S_CORPSEHANGING_1,               // 654
    S_ZSTATUEGARGOYLEGREENTALL_1,  // 655
    S_ZSTATUEGARGOYLEBLUETALL_1,   // 656
    S_ZSTATUEGARGOYLEGREENSHORT_1, // 657
    S_ZSTATUEGARGOYLEBLUESHORT_1,  // 658
    S_ZSTATUEGARGOYLESTRIPETALL_1, // 659
    S_ZSTATUEGARGOYLEDARKREDTALL_1,    // 660
    S_ZSTATUEGARGOYLEREDTALL_1,       // 661
    S_ZSTATUEGARGOYLETANTALL_1,       // 662
    S_ZSTATUEGARGOYLERUSTTALL_1,   // 663
    S_ZSTATUEGARGOYLEDARKREDSHORT_1,    // 664
    S_ZSTATUEGARGOYLEREDSHORT_1,   // 665
    S_ZSTATUEGARGOYLETANSHORT_1,   // 666
    S_ZSTATUEGARGOYLERUSTSHORT_1,  // 667
    S_ZBANNERTATTERED_1,           // 668
    S_ZTREELARGE1,                   // 669
    S_ZTREELARGE2,                   // 670
    S_ZTREEGNARLED1,               // 671
    S_ZTREEGNARLED2,               // 672
    S_ZLOG,                           // 673
    S_ZSTALACTITEICELARGE,           // 674
    S_ZSTALACTITEICEMEDIUM,           // 675
    S_ZSTALACTITEICESMALL,           // 676
    S_ZSTALACTITEICETINY,           // 677
    S_ZSTALAGMITEICELARGE,           // 678
    S_ZSTALAGMITEICEMEDIUM,           // 679
    S_ZSTALAGMITEICESMALL,           // 680
    S_ZSTALAGMITEICETINY,           // 681
    S_ZROCKBROWN1,                   // 682
    S_ZROCKBROWN2,                   // 683
    S_ZROCKBLACK,                   // 684
    S_ZRUBBLE1,                       // 685
    S_ZRUBBLE2,                       // 686
    S_ZRUBBLE3,                       // 687
    S_ZVASEPILLAR,                   // 688
    S_ZPOTTERY1,                   // 689
    S_ZPOTTERY2,                   // 690
    S_ZPOTTERY3,                   // 691
    S_ZPOTTERY_EXPLODE,               // 692
    S_POTTERYBIT_1,                   // 693
    S_POTTERYBIT_2,                   // 694
    S_POTTERYBIT_3,                   // 695
    S_POTTERYBIT_4,                   // 696
    S_POTTERYBIT_5,                   // 697
    S_POTTERYBIT_EX0,               // 698
    S_POTTERYBIT_EX1,               // 699
    S_POTTERYBIT_EX1_2,               // 700
    S_POTTERYBIT_EX2,               // 701
    S_POTTERYBIT_EX2_2,               // 702
    S_POTTERYBIT_EX3,               // 703
    S_POTTERYBIT_EX3_2,               // 704
    S_POTTERYBIT_EX4,               // 705
    S_POTTERYBIT_EX4_2,               // 706
    S_POTTERYBIT_EX5,               // 707
    S_POTTERYBIT_EX5_2,               // 708
    S_ZCORPSELYNCHED1,               // 709
    S_ZCORPSELYNCHED2,               // 710
    S_ZCORPSESITTING,               // 711
    S_ZCORPSESITTING_X,               // 712
    S_CORPSEBIT_1,                   // 713
    S_CORPSEBIT_2,                   // 714
    S_CORPSEBIT_3,                   // 715
    S_CORPSEBIT_4,                   // 716
    S_CORPSEBLOODDRIP,               // 717
    S_CORPSEBLOODDRIP_X1,           // 718
    S_CORPSEBLOODDRIP_X2,           // 719
    S_CORPSEBLOODDRIP_X3,           // 720
    S_CORPSEBLOODDRIP_X4,           // 721
    S_BLOODPOOL,                   // 722
    S_ZCANDLE1,                       // 723
    S_ZCANDLE2,                       // 724
    S_ZCANDLE3,                       // 725
    S_ZLEAFSPAWNER,                   // 726
    S_LEAF1_1,                       // 727
    S_LEAF1_2,                       // 728
    S_LEAF1_3,                       // 729
    S_LEAF1_4,                       // 730
    S_LEAF1_5,                       // 731
    S_LEAF1_6,                       // 732
    S_LEAF1_7,                       // 733
    S_LEAF1_8,                       // 734
    S_LEAF1_9,                       // 735
    S_LEAF1_10,                       // 736
    S_LEAF1_11,                       // 737
    S_LEAF1_12,                       // 738
    S_LEAF1_13,                       // 739
    S_LEAF1_14,                       // 740
    S_LEAF1_15,                       // 741
    S_LEAF1_16,                       // 742
    S_LEAF1_17,                       // 743
    S_LEAF1_18,                       // 744
    S_LEAF_X1,                       // 745
    S_LEAF2_1,                       // 746
    S_LEAF2_2,                       // 747
    S_LEAF2_3,                       // 748
    S_LEAF2_4,                       // 749
    S_LEAF2_5,                       // 750
    S_LEAF2_6,                       // 751
    S_LEAF2_7,                       // 752
    S_LEAF2_8,                       // 753
    S_LEAF2_9,                       // 754
    S_LEAF2_10,                       // 755
    S_LEAF2_11,                       // 756
    S_LEAF2_12,                       // 757
    S_LEAF2_13,                       // 758
    S_LEAF2_14,                       // 759
    S_LEAF2_15,                       // 760
    S_LEAF2_16,                       // 761
    S_LEAF2_17,                       // 762
    S_LEAF2_18,                       // 763
    S_ZTWINEDTORCH_1,               // 764
    S_ZTWINEDTORCH_2,               // 765
    S_ZTWINEDTORCH_3,               // 766
    S_ZTWINEDTORCH_4,               // 767
    S_ZTWINEDTORCH_5,               // 768
    S_ZTWINEDTORCH_6,               // 769
    S_ZTWINEDTORCH_7,               // 770
    S_ZTWINEDTORCH_8,               // 771
    S_ZTWINEDTORCH_UNLIT,           // 772
    S_BRIDGE1,                       // 773
    S_BRIDGE2,                       // 774
    S_BRIDGE3,                       // 775
    S_FREE_BRIDGE1,                   // 776
    S_FREE_BRIDGE2,                   // 777
    S_BBALL1,                       // 778
    S_BBALL2,                       // 779
    S_ZWALLTORCH1,                   // 780
    S_ZWALLTORCH2,                   // 781
    S_ZWALLTORCH3,                   // 782
    S_ZWALLTORCH4,                   // 783
    S_ZWALLTORCH5,                   // 784
    S_ZWALLTORCH6,                   // 785
    S_ZWALLTORCH7,                   // 786
    S_ZWALLTORCH8,                   // 787
    S_ZWALLTORCH_U,                   // 788
    S_ZBARREL1,                       // 789
    S_ZSHRUB1,                       // 790
    S_ZSHRUB1_DIE,                   // 791
    S_ZSHRUB1_X1,                   // 792
    S_ZSHRUB1_X2,                   // 793
    S_ZSHRUB1_X3,                   // 794
    S_ZSHRUB2,                       // 795
    S_ZSHRUB2_DIE,                   // 796
    S_ZSHRUB2_X1,                   // 797
    S_ZSHRUB2_X2,                   // 798
    S_ZSHRUB2_X3,                   // 799
    S_ZSHRUB2_X4,                   // 800
    S_ZBUCKET1,                       // 801
    S_ZPOISONSHROOM1,               // 802
    S_ZPOISONSHROOM_P1,               // 803
    S_ZPOISONSHROOM_P2,               // 804
    S_ZPOISONSHROOM_X1,               // 805
    S_ZPOISONSHROOM_X2,               // 806
    S_ZPOISONSHROOM_X3,               // 807
    S_ZPOISONSHROOM_X4,               // 808
    S_ZFIREBULL1,                   // 809
    S_ZFIREBULL2,                   // 810
    S_ZFIREBULL3,                   // 811
    S_ZFIREBULL4,                   // 812
    S_ZFIREBULL5,                   // 813
    S_ZFIREBULL6,                   // 814
    S_ZFIREBULL7,                   // 815
    S_ZFIREBULL_DEATH,               // 816
    S_ZFIREBULL_DEATH2,               // 817
    S_ZFIREBULL_U,                   // 818
    S_ZFIREBULL_BIRTH,               // 819
    S_ZFIREBULL_BIRTH2,               // 820
    S_ZFIRETHING1,                   // 821
    S_ZFIRETHING2,                   // 822
    S_ZFIRETHING3,                   // 823
    S_ZFIRETHING4,                   // 824
    S_ZFIRETHING5,                   // 825
    S_ZFIRETHING6,                   // 826
    S_ZFIRETHING7,                   // 827
    S_ZFIRETHING8,                   // 828
    S_ZFIRETHING9,                   // 829
    S_ZBRASSTORCH1,                   // 830
    S_ZBRASSTORCH2,                   // 831
    S_ZBRASSTORCH3,                   // 832
    S_ZBRASSTORCH4,                   // 833
    S_ZBRASSTORCH5,                   // 834
    S_ZBRASSTORCH6,                   // 835
    S_ZBRASSTORCH7,                   // 836
    S_ZBRASSTORCH8,                   // 837
    S_ZBRASSTORCH9,                   // 838
    S_ZBRASSTORCH10,               // 839
    S_ZBRASSTORCH11,               // 840
    S_ZBRASSTORCH12,               // 841
    S_ZBRASSTORCH13,               // 842
    S_ZSUITOFARMOR,                   // 843
    S_ZSUITOFARMOR_X1,               // 844
    S_ZARMORCHUNK1,                   // 845
    S_ZARMORCHUNK2,                   // 846
    S_ZARMORCHUNK3,                   // 847
    S_ZARMORCHUNK4,                   // 848
    S_ZARMORCHUNK5,                   // 849
    S_ZARMORCHUNK6,                   // 850
    S_ZARMORCHUNK7,                   // 851
    S_ZARMORCHUNK8,                   // 852
    S_ZARMORCHUNK9,                   // 853
    S_ZARMORCHUNK10,               // 854
    S_ZBELL,                       // 855
    S_ZBELL_X1,                       // 856
    S_ZBELL_X2,                       // 857
    S_ZBELL_X3,                       // 858
    S_ZBELL_X4,                       // 859
    S_ZBELL_X5,                       // 860
    S_ZBELL_X6,                       // 861
    S_ZBELL_X7,                       // 862
    S_ZBELL_X8,                       // 863
    S_ZBELL_X9,                       // 864
    S_ZBELL_X10,                   // 865
    S_ZBELL_X11,                   // 866
    S_ZBELL_X12,                   // 867
    S_ZBELL_X13,                   // 868
    S_ZBELL_X14,                   // 869
    S_ZBELL_X15,                   // 870
    S_ZBELL_X16,                   // 871
    S_ZBELL_X17,                   // 872
    S_ZBELL_X18,                   // 873
    S_ZBELL_X19,                   // 874
    S_ZBELL_X20,                   // 875
    S_ZBELL_X21,                   // 876
    S_ZBELL_X22,                   // 877
    S_ZBELL_X23,                   // 878
    S_ZBELL_X24,                   // 879
    S_ZBELL_X25,                   // 880
    S_ZBELL_X26,                   // 881
    S_ZBELL_X27,                   // 882
    S_ZBELL_X28,                   // 883
    S_ZBELL_X29,                   // 884
    S_ZBELL_X30,                   // 885
    S_ZBELL_X31,                   // 886
    S_ZBELL_X32,                   // 887
    S_ZBELL_X33,                   // 888
    S_ZBELL_X34,                   // 889
    S_ZBELL_X35,                   // 890
    S_ZBELL_X36,                   // 891
    S_ZBELL_X37,                   // 892
    S_ZBELL_X38,                   // 893
    S_ZBELL_X39,                   // 894
    S_ZBELL_X40,                   // 895
    S_ZBELL_X41,                   // 896
    S_ZBELL_X42,                   // 897
    S_ZBELL_X43,                   // 898
    S_ZBELL_X44,                   // 899
    S_ZBELL_X45,                   // 900
    S_ZBELL_X46,                   // 901
    S_ZBELL_X47,                   // 902
    S_ZBLUE_CANDLE1,               // 903
    S_ZBLUE_CANDLE2,               // 904
    S_ZBLUE_CANDLE3,               // 905
    S_ZBLUE_CANDLE4,               // 906
    S_ZBLUE_CANDLE5,               // 907
    S_ZIRON_MAIDEN,                   // 908
    S_ZXMAS_TREE,                   // 909
    S_ZXMAS_TREE_DIE,               // 910
    S_ZXMAS_TREE_X1,               // 911
    S_ZXMAS_TREE_X2,               // 912
    S_ZXMAS_TREE_X3,               // 913
    S_ZXMAS_TREE_X4,               // 914
    S_ZXMAS_TREE_X5,               // 915
    S_ZXMAS_TREE_X6,               // 916
    S_ZXMAS_TREE_X7,               // 917
    S_ZXMAS_TREE_X8,               // 918
    S_ZXMAS_TREE_X9,               // 919
    S_ZXMAS_TREE_X10,               // 920
    S_ZCAULDRON1,                   // 921
    S_ZCAULDRON2,                   // 922
    S_ZCAULDRON3,                   // 923
    S_ZCAULDRON4,                   // 924
    S_ZCAULDRON5,                   // 925
    S_ZCAULDRON6,                   // 926
    S_ZCAULDRON7,                   // 927
    S_ZCAULDRON_U,                   // 928
    S_ZCHAINBIT32,                   // 929
    S_ZCHAINBIT64,                   // 930
    S_ZCHAINEND_HEART,               // 931
    S_ZCHAINEND_HOOK1,               // 932
    S_ZCHAINEND_HOOK2,               // 933
    S_ZCHAINEND_SPIKE,               // 934
    S_ZCHAINEND_SKULL,               // 935
    S_TABLE_SHIT1,                   // 936
    S_TABLE_SHIT2,                   // 937
    S_TABLE_SHIT3,                   // 938
    S_TABLE_SHIT4,                   // 939
    S_TABLE_SHIT5,                   // 940
    S_TABLE_SHIT6,                   // 941
    S_TABLE_SHIT7,                   // 942
    S_TABLE_SHIT8,                   // 943
    S_TABLE_SHIT9,                   // 944
    S_TABLE_SHIT10,                   // 945
    S_TFOG1,                       // 946
    S_TFOG2,                       // 947
    S_TFOG3,                       // 948
    S_TFOG4,                       // 949
    S_TFOG5,                       // 950
    S_TFOG6,                       // 951
    S_TFOG7,                       // 952
    S_TFOG8,                       // 953
    S_TFOG9,                       // 954
    S_TFOG10,                       // 955
    S_TFOG11,                       // 956
    S_TFOG12,                       // 957
    S_TFOG13,                       // 958
    S_TELESMOKE1,                   // 959
    S_TELESMOKE2,                   // 960
    S_TELESMOKE3,                   // 961
    S_TELESMOKE4,                   // 962
    S_TELESMOKE5,                   // 963
    S_TELESMOKE6,                   // 964
    S_TELESMOKE7,                   // 965
    S_TELESMOKE8,                   // 966
    S_TELESMOKE9,                   // 967
    S_TELESMOKE10,                   // 968
    S_TELESMOKE11,                   // 969
    S_TELESMOKE12,                   // 970
    S_TELESMOKE13,                   // 971
    S_TELESMOKE14,                   // 972
    S_TELESMOKE15,                   // 973
    S_TELESMOKE16,                   // 974
    S_TELESMOKE17,                   // 975
    S_TELESMOKE18,                   // 976
    S_TELESMOKE19,                   // 977
    S_TELESMOKE20,                   // 978
    S_TELESMOKE21,                   // 979
    S_TELESMOKE22,                   // 980
    S_TELESMOKE23,                   // 981
    S_TELESMOKE24,                   // 982
    S_TELESMOKE25,                   // 983
    S_TELESMOKE26,                   // 984
    S_LIGHTDONE,                   // 985
    S_PUNCHREADY,                   // 986
    S_PUNCHDOWN,                   // 987
    S_PUNCHUP,                       // 988
    S_PUNCHATK1_1,                   // 989
    S_PUNCHATK1_2,                   // 990
    S_PUNCHATK1_3,                   // 991
    S_PUNCHATK1_4,                   // 992
    S_PUNCHATK1_5,                   // 993
    S_PUNCHATK2_1,                   // 994
    S_PUNCHATK2_2,                   // 995
    S_PUNCHATK2_3,                   // 996
    S_PUNCHATK2_4,                   // 997
    S_PUNCHATK2_5,                   // 998
    S_PUNCHATK2_6,                   // 999
    S_PUNCHATK2_7,                   // 1000
    S_PUNCHATK2_8,                   // 1001
    S_PUNCHATK2_9,                   // 1002
    S_PUNCHPUFF1,                   // 1003
    S_PUNCHPUFF2,                   // 1004
    S_PUNCHPUFF3,                   // 1005
    S_PUNCHPUFF4,                   // 1006
    S_PUNCHPUFF5,                   // 1007
    S_AXE,                           // 1008
    S_FAXEREADY,                   // 1009
    S_FAXEDOWN,                       // 1010
    S_FAXEUP,                       // 1011
    S_FAXEATK_1,                   // 1012
    S_FAXEATK_2,                   // 1013
    S_FAXEATK_3,                   // 1014
    S_FAXEATK_4,                   // 1015
    S_FAXEATK_5,                   // 1016
    S_FAXEATK_6,                   // 1017
    S_FAXEATK_7,                   // 1018
    S_FAXEATK_8,                   // 1019
    S_FAXEATK_9,                   // 1020
    S_FAXEATK_10,                   // 1021
    S_FAXEATK_11,                   // 1022
    S_FAXEATK_12,                   // 1023
    S_FAXEATK_13,                   // 1024
    S_FAXEREADY_G,                   // 1025
    S_FAXEREADY_G1,                   // 1026
    S_FAXEREADY_G2,                   // 1027
    S_FAXEREADY_G3,                   // 1028
    S_FAXEREADY_G4,                   // 1029
    S_FAXEREADY_G5,                   // 1030
    S_FAXEDOWN_G,                   // 1031
    S_FAXEUP_G,                       // 1032
    S_FAXEATK_G1,                   // 1033
    S_FAXEATK_G2,                   // 1034
    S_FAXEATK_G3,                   // 1035
    S_FAXEATK_G4,                   // 1036
    S_FAXEATK_G5,                   // 1037
    S_FAXEATK_G6,                   // 1038
    S_FAXEATK_G7,                   // 1039
    S_FAXEATK_G8,                   // 1040
    S_FAXEATK_G9,                   // 1041
    S_FAXEATK_G10,                   // 1042
    S_FAXEATK_G11,                   // 1043
    S_FAXEATK_G12,                   // 1044
    S_FAXEATK_G13,                   // 1045
    S_AXEPUFF_GLOW1,               // 1046
    S_AXEPUFF_GLOW2,               // 1047
    S_AXEPUFF_GLOW3,               // 1048
    S_AXEPUFF_GLOW4,               // 1049
    S_AXEPUFF_GLOW5,               // 1050
    S_AXEPUFF_GLOW6,               // 1051
    S_AXEPUFF_GLOW7,               // 1052
    S_AXEBLOOD1,                   // 1053
    S_AXEBLOOD2,                   // 1054
    S_AXEBLOOD3,                   // 1055
    S_AXEBLOOD4,                   // 1056
    S_AXEBLOOD5,                   // 1057
    S_AXEBLOOD6,                   // 1058
    S_HAMM,                           // 1059
    S_FHAMMERREADY,                   // 1060
    S_FHAMMERDOWN,                   // 1061
    S_FHAMMERUP,                   // 1062
    S_FHAMMERATK_1,                   // 1063
    S_FHAMMERATK_2,                   // 1064
    S_FHAMMERATK_3,                   // 1065
    S_FHAMMERATK_4,                   // 1066
    S_FHAMMERATK_5,                   // 1067
    S_FHAMMERATK_6,                   // 1068
    S_FHAMMERATK_7,                   // 1069
    S_FHAMMERATK_8,                   // 1070
    S_FHAMMERATK_9,                   // 1071
    S_FHAMMERATK_10,               // 1072
    S_FHAMMERATK_11,               // 1073
    S_FHAMMERATK_12,               // 1074
    S_HAMMER_MISSILE_1,               // 1075
    S_HAMMER_MISSILE_2,               // 1076
    S_HAMMER_MISSILE_3,               // 1077
    S_HAMMER_MISSILE_4,               // 1078
    S_HAMMER_MISSILE_5,               // 1079
    S_HAMMER_MISSILE_6,               // 1080
    S_HAMMER_MISSILE_7,               // 1081
    S_HAMMER_MISSILE_8,               // 1082
    S_HAMMER_MISSILE_X1,           // 1083
    S_HAMMER_MISSILE_X2,           // 1084
    S_HAMMER_MISSILE_X3,           // 1085
    S_HAMMER_MISSILE_X4,           // 1086
    S_HAMMER_MISSILE_X5,           // 1087
    S_HAMMER_MISSILE_X6,           // 1088
    S_HAMMER_MISSILE_X7,           // 1089
    S_HAMMER_MISSILE_X8,           // 1090
    S_HAMMER_MISSILE_X9,           // 1091
    S_HAMMER_MISSILE_X10,           // 1092
    S_HAMMERPUFF1,                   // 1093
    S_HAMMERPUFF2,                   // 1094
    S_HAMMERPUFF3,                   // 1095
    S_HAMMERPUFF4,                   // 1096
    S_HAMMERPUFF5,                   // 1097
    S_FSWORDREADY,                   // 1098
    S_FSWORDREADY1,                   // 1099
    S_FSWORDREADY2,                   // 1100
    S_FSWORDREADY3,                   // 1101
    S_FSWORDREADY4,                   // 1102
    S_FSWORDREADY5,                   // 1103
    S_FSWORDREADY6,                   // 1104
    S_FSWORDREADY7,                   // 1105
    S_FSWORDREADY8,                   // 1106
    S_FSWORDREADY9,                   // 1107
    S_FSWORDREADY10,               // 1108
    S_FSWORDREADY11,               // 1109
    S_FSWORDDOWN,                   // 1110
    S_FSWORDUP,                       // 1111
    S_FSWORDATK_1,                   // 1112
    S_FSWORDATK_2,                   // 1113
    S_FSWORDATK_3,                   // 1114
    S_FSWORDATK_4,                   // 1115
    S_FSWORDATK_5,                   // 1116
    S_FSWORDATK_6,                   // 1117
    S_FSWORDATK_7,                   // 1118
    S_FSWORDATK_8,                   // 1119
    S_FSWORDATK_9,                   // 1120
    S_FSWORDATK_10,                   // 1121
    S_FSWORDATK_11,                   // 1122
    S_FSWORDATK_12,                   // 1123
    S_FSWORD_MISSILE1,               // 1124
    S_FSWORD_MISSILE2,               // 1125
    S_FSWORD_MISSILE3,               // 1126
    S_FSWORD_MISSILE_X1,           // 1127
    S_FSWORD_MISSILE_X2,           // 1128
    S_FSWORD_MISSILE_X3,           // 1129
    S_FSWORD_MISSILE_X4,           // 1130
    S_FSWORD_MISSILE_X5,           // 1131
    S_FSWORD_MISSILE_X6,           // 1132
    S_FSWORD_MISSILE_X7,           // 1133
    S_FSWORD_MISSILE_X8,           // 1134
    S_FSWORD_MISSILE_X9,           // 1135
    S_FSWORD_MISSILE_X10,           // 1136
    S_FSWORD_FLAME1,               // 1137
    S_FSWORD_FLAME2,               // 1138
    S_FSWORD_FLAME3,               // 1139
    S_FSWORD_FLAME4,               // 1140
    S_FSWORD_FLAME5,               // 1141
    S_FSWORD_FLAME6,               // 1142
    S_FSWORD_FLAME7,               // 1143
    S_FSWORD_FLAME8,               // 1144
    S_FSWORD_FLAME9,               // 1145
    S_FSWORD_FLAME10,               // 1146
    S_CMACEREADY,                   // 1147
    S_CMACEDOWN,                   // 1148
    S_CMACEUP,                       // 1149
    S_CMACEATK_1,                   // 1150
    S_CMACEATK_2,                   // 1151
    S_CMACEATK_3,                   // 1152
    S_CMACEATK_4,                   // 1153
    S_CMACEATK_5,                   // 1154
    S_CMACEATK_6,                   // 1155
    S_CMACEATK_7,                   // 1156
    S_CMACEATK_8,                   // 1157
    S_CMACEATK_9,                   // 1158
    S_CMACEATK_10,                   // 1159
    S_CMACEATK_11,                   // 1160
    S_CMACEATK_12,                   // 1161
    S_CMACEATK_13,                   // 1162
    S_CMACEATK_14,                   // 1163
    S_CMACEATK_15,                   // 1164
    S_CMACEATK_16,                   // 1165
    S_CMACEATK_17,                   // 1166
    S_CSTAFF,                       // 1167
    S_CSTAFFREADY,                   // 1168
    S_CSTAFFREADY1,                   // 1169
    S_CSTAFFREADY2,                   // 1170
    S_CSTAFFREADY3,                   // 1171
    S_CSTAFFREADY4,                   // 1172
    S_CSTAFFREADY5,                   // 1173
    S_CSTAFFREADY6,                   // 1174
    S_CSTAFFREADY7,                   // 1175
    S_CSTAFFREADY8,                   // 1176
    S_CSTAFFREADY9,                   // 1177
    S_CSTAFFBLINK1,                   // 1178
    S_CSTAFFBLINK2,                   // 1179
    S_CSTAFFBLINK3,                   // 1180
    S_CSTAFFBLINK4,                   // 1181
    S_CSTAFFBLINK5,                   // 1182
    S_CSTAFFBLINK6,                   // 1183
    S_CSTAFFBLINK7,                   // 1184
    S_CSTAFFBLINK8,                   // 1185
    S_CSTAFFBLINK9,                   // 1186
    S_CSTAFFBLINK10,               // 1187
    S_CSTAFFBLINK11,               // 1188
    S_CSTAFFDOWN,                   // 1189
    S_CSTAFFDOWN2,                   // 1190
    S_CSTAFFDOWN3,                   // 1191
    S_CSTAFFUP,                       // 1192
    S_CSTAFFATK_1,                   // 1193
    S_CSTAFFATK_2,                   // 1194
    S_CSTAFFATK_3,                   // 1195
    S_CSTAFFATK_4,                   // 1196
    S_CSTAFFATK_5,                   // 1197
    S_CSTAFFATK_6,                   // 1198
    S_CSTAFFATK2_1,                   // 1199
    S_CSTAFF_MISSILE1,               // 1200
    S_CSTAFF_MISSILE2,               // 1201
    S_CSTAFF_MISSILE3,               // 1202
    S_CSTAFF_MISSILE4,               // 1203
    S_CSTAFF_MISSILE_X1,           // 1204
    S_CSTAFF_MISSILE_X2,           // 1205
    S_CSTAFF_MISSILE_X3,           // 1206
    S_CSTAFF_MISSILE_X4,           // 1207
    S_CSTAFFPUFF1,                   // 1208
    S_CSTAFFPUFF2,                   // 1209
    S_CSTAFFPUFF3,                   // 1210
    S_CSTAFFPUFF4,                   // 1211
    S_CSTAFFPUFF5,                   // 1212
    S_CFLAME1,                       // 1213
    S_CFLAME2,                       // 1214
    S_CFLAME3,                       // 1215
    S_CFLAME4,                       // 1216
    S_CFLAME5,                       // 1217
    S_CFLAME6,                       // 1218
    S_CFLAME7,                       // 1219
    S_CFLAME8,                       // 1220
    S_CFLAMEREADY1,                   // 1221
    S_CFLAMEREADY2,                   // 1222
    S_CFLAMEREADY3,                   // 1223
    S_CFLAMEREADY4,                   // 1224
    S_CFLAMEREADY5,                   // 1225
    S_CFLAMEREADY6,                   // 1226
    S_CFLAMEREADY7,                   // 1227
    S_CFLAMEREADY8,                   // 1228
    S_CFLAMEREADY9,                   // 1229
    S_CFLAMEREADY10,               // 1230
    S_CFLAMEREADY11,               // 1231
    S_CFLAMEREADY12,               // 1232
    S_CFLAMEDOWN,                   // 1233
    S_CFLAMEUP,                       // 1234
    S_CFLAMEATK_1,                   // 1235
    S_CFLAMEATK_2,                   // 1236
    S_CFLAMEATK_3,                   // 1237
    S_CFLAMEATK_4,                   // 1238
    S_CFLAMEATK_5,                   // 1239
    S_CFLAMEATK_6,                   // 1240
    S_CFLAMEATK_7,                   // 1241
    S_CFLAMEATK_8,                   // 1242
    S_CFLAMEFLOOR1,                   // 1243
    S_CFLAMEFLOOR2,                   // 1244
    S_CFLAMEFLOOR3,                   // 1245
    S_FLAMEPUFF1,                   // 1246
    S_FLAMEPUFF2,                   // 1247
    S_FLAMEPUFF3,                   // 1248
    S_FLAMEPUFF4,                   // 1249
    S_FLAMEPUFF5,                   // 1250
    S_FLAMEPUFF6,                   // 1251
    S_FLAMEPUFF7,                   // 1252
    S_FLAMEPUFF8,                   // 1253
    S_FLAMEPUFF9,                   // 1254
    S_FLAMEPUFF10,                   // 1255
    S_FLAMEPUFF11,                   // 1256
    S_FLAMEPUFF12,                   // 1257
    S_FLAMEPUFF13,                   // 1258
    S_FLAMEPUFF2_1,                   // 1259
    S_FLAMEPUFF2_2,                   // 1260
    S_FLAMEPUFF2_3,                   // 1261
    S_FLAMEPUFF2_4,                   // 1262
    S_FLAMEPUFF2_5,                   // 1263
    S_FLAMEPUFF2_6,                   // 1264
    S_FLAMEPUFF2_7,                   // 1265
    S_FLAMEPUFF2_8,                   // 1266
    S_FLAMEPUFF2_9,                   // 1267
    S_FLAMEPUFF2_10,               // 1268
    S_FLAMEPUFF2_11,               // 1269
    S_FLAMEPUFF2_12,               // 1270
    S_FLAMEPUFF2_13,               // 1271
    S_FLAMEPUFF2_14,               // 1272
    S_FLAMEPUFF2_15,               // 1273
    S_FLAMEPUFF2_16,               // 1274
    S_FLAMEPUFF2_17,               // 1275
    S_FLAMEPUFF2_18,               // 1276
    S_FLAMEPUFF2_19,               // 1277
    S_FLAMEPUFF2_20,               // 1278
    S_CIRCLE_FLAME1,               // 1279
    S_CIRCLE_FLAME2,               // 1280
    S_CIRCLE_FLAME3,               // 1281
    S_CIRCLE_FLAME4,               // 1282
    S_CIRCLE_FLAME5,               // 1283
    S_CIRCLE_FLAME6,               // 1284
    S_CIRCLE_FLAME7,               // 1285
    S_CIRCLE_FLAME8,               // 1286
    S_CIRCLE_FLAME9,               // 1287
    S_CIRCLE_FLAME10,               // 1288
    S_CIRCLE_FLAME11,               // 1289
    S_CIRCLE_FLAME12,               // 1290
    S_CIRCLE_FLAME13,               // 1291
    S_CIRCLE_FLAME14,               // 1292
    S_CIRCLE_FLAME15,               // 1293
    S_CIRCLE_FLAME16,               // 1294
    S_CIRCLE_FLAME_X1,               // 1295
    S_CIRCLE_FLAME_X2,               // 1296
    S_CIRCLE_FLAME_X3,               // 1297
    S_CIRCLE_FLAME_X4,               // 1298
    S_CIRCLE_FLAME_X5,               // 1299
    S_CIRCLE_FLAME_X6,               // 1300
    S_CIRCLE_FLAME_X7,               // 1301
    S_CIRCLE_FLAME_X8,               // 1302
    S_CIRCLE_FLAME_X9,               // 1303
    S_CIRCLE_FLAME_X10,               // 1304
    S_CFLAME_MISSILE1,               // 1305
    S_CFLAME_MISSILE2,               // 1306
    S_CFLAME_MISSILE_X,               // 1307
    S_CHOLYREADY,                   // 1308
    S_CHOLYDOWN,                   // 1309
    S_CHOLYUP,                       // 1310
    S_CHOLYATK_1,                   // 1311
    S_CHOLYATK_2,                   // 1312
    S_CHOLYATK_3,                   // 1313
    S_CHOLYATK_4,                   // 1314
    S_CHOLYATK_5,                   // 1315
    S_CHOLYATK_6,                   // 1316
    S_CHOLYATK_7,                   // 1317
    S_CHOLYATK_8,                   // 1318
    S_CHOLYATK_9,                   // 1319
    S_HOLY_FX1,                       // 1320
    S_HOLY_FX2,                       // 1321
    S_HOLY_FX3,                       // 1322
    S_HOLY_FX4,                       // 1323
    S_HOLY_FX_X1,                   // 1324
    S_HOLY_FX_X2,                   // 1325
    S_HOLY_FX_X3,                   // 1326
    S_HOLY_FX_X4,                   // 1327
    S_HOLY_FX_X5,                   // 1328
    S_HOLY_FX_X6,                   // 1329
    S_HOLY_TAIL1,                   // 1330
    S_HOLY_TAIL2,                   // 1331
    S_HOLY_PUFF1,                   // 1332
    S_HOLY_PUFF2,                   // 1333
    S_HOLY_PUFF3,                   // 1334
    S_HOLY_PUFF4,                   // 1335
    S_HOLY_PUFF5,                   // 1336
    S_HOLY_MISSILE1,               // 1337
    S_HOLY_MISSILE2,               // 1338
    S_HOLY_MISSILE3,               // 1339
    S_HOLY_MISSILE4,               // 1340
    S_HOLY_MISSILE_X,               // 1341
    S_HOLY_MISSILE_P1,               // 1342
    S_HOLY_MISSILE_P2,               // 1343
    S_HOLY_MISSILE_P3,               // 1344
    S_HOLY_MISSILE_P4,               // 1345
    S_HOLY_MISSILE_P5,               // 1346
    S_MWANDREADY,                   // 1347
    S_MWANDDOWN,                   // 1348
    S_MWANDUP,                       // 1349
    S_MWANDATK_1,                   // 1350
    S_MWANDATK_2,                   // 1351
    S_MWANDATK_3,                   // 1352
    S_MWANDATK_4,                   // 1353
    S_MWANDPUFF1,                   // 1354
    S_MWANDPUFF2,                   // 1355
    S_MWANDPUFF3,                   // 1356
    S_MWANDPUFF4,                   // 1357
    S_MWANDPUFF5,                   // 1358
    S_MWANDSMOKE1,                   // 1359
    S_MWANDSMOKE2,                   // 1360
    S_MWANDSMOKE3,                   // 1361
    S_MWANDSMOKE4,                   // 1362
    S_MWAND_MISSILE1,               // 1363
    S_MWAND_MISSILE2,               // 1364
    S_MW_LIGHTNING1,               // 1365
    S_MW_LIGHTNING2,               // 1366
    S_MW_LIGHTNING3,               // 1367
    S_MW_LIGHTNING4,               // 1368
    S_MW_LIGHTNING5,               // 1369
    S_MW_LIGHTNING6,               // 1370
    S_MW_LIGHTNING7,               // 1371
    S_MW_LIGHTNING8,               // 1372
    S_MLIGHTNINGREADY,               // 1373
    S_MLIGHTNINGREADY2,               // 1374
    S_MLIGHTNINGREADY3,               // 1375
    S_MLIGHTNINGREADY4,               // 1376
    S_MLIGHTNINGREADY5,               // 1377
    S_MLIGHTNINGREADY6,               // 1378
    S_MLIGHTNINGREADY7,               // 1379
    S_MLIGHTNINGREADY8,               // 1380
    S_MLIGHTNINGREADY9,               // 1381
    S_MLIGHTNINGREADY10,           // 1382
    S_MLIGHTNINGREADY11,           // 1383
    S_MLIGHTNINGREADY12,           // 1384
    S_MLIGHTNINGREADY13,           // 1385
    S_MLIGHTNINGREADY14,           // 1386
    S_MLIGHTNINGREADY15,           // 1387
    S_MLIGHTNINGREADY16,           // 1388
    S_MLIGHTNINGREADY17,           // 1389
    S_MLIGHTNINGREADY18,           // 1390
    S_MLIGHTNINGREADY19,           // 1391
    S_MLIGHTNINGREADY20,           // 1392
    S_MLIGHTNINGREADY21,           // 1393
    S_MLIGHTNINGREADY22,           // 1394
    S_MLIGHTNINGREADY23,           // 1395
    S_MLIGHTNINGREADY24,           // 1396
    S_MLIGHTNINGDOWN,               // 1397
    S_MLIGHTNINGUP,                   // 1398
    S_MLIGHTNINGATK_1,               // 1399
    S_MLIGHTNINGATK_2,               // 1400
    S_MLIGHTNINGATK_3,               // 1401
    S_MLIGHTNINGATK_4,               // 1402
    S_MLIGHTNINGATK_5,               // 1403
    S_MLIGHTNINGATK_6,               // 1404
    S_MLIGHTNINGATK_7,               // 1405
    S_MLIGHTNINGATK_8,               // 1406
    S_MLIGHTNINGATK_9,               // 1407
    S_MLIGHTNINGATK_10,               // 1408
    S_MLIGHTNINGATK_11,               // 1409
    S_LIGHTNING_CEILING1,           // 1410
    S_LIGHTNING_CEILING2,           // 1411
    S_LIGHTNING_CEILING3,           // 1412
    S_LIGHTNING_CEILING4,           // 1413
    S_LIGHTNING_C_X1,               // 1414
    S_LIGHTNING_C_X2,               // 1415
    S_LIGHTNING_C_X3,               // 1416
    S_LIGHTNING_C_X4,               // 1417
    S_LIGHTNING_C_X5,               // 1418
    S_LIGHTNING_C_X6,               // 1419
    S_LIGHTNING_C_X7,               // 1420
    S_LIGHTNING_C_X8,               // 1421
    S_LIGHTNING_C_X9,               // 1422
    S_LIGHTNING_C_X10,               // 1423
    S_LIGHTNING_C_X11,               // 1424
    S_LIGHTNING_C_X12,               // 1425
    S_LIGHTNING_C_X13,               // 1426
    S_LIGHTNING_C_X14,               // 1427
    S_LIGHTNING_C_X15,               // 1428
    S_LIGHTNING_C_X16,               // 1429
    S_LIGHTNING_C_X17,               // 1430
    S_LIGHTNING_C_X18,               // 1431
    S_LIGHTNING_C_X19,               // 1432
    S_LIGHTNING_FLOOR1,               // 1433
    S_LIGHTNING_FLOOR2,               // 1434
    S_LIGHTNING_FLOOR3,               // 1435
    S_LIGHTNING_FLOOR4,               // 1436
    S_LIGHTNING_F_X1,               // 1437
    S_LIGHTNING_F_X2,               // 1438
    S_LIGHTNING_F_X3,               // 1439
    S_LIGHTNING_F_X4,               // 1440
    S_LIGHTNING_F_X5,               // 1441
    S_LIGHTNING_F_X6,               // 1442
    S_LIGHTNING_F_X7,               // 1443
    S_LIGHTNING_F_X8,               // 1444
    S_LIGHTNING_F_X9,               // 1445
    S_LIGHTNING_F_X10,               // 1446
    S_LIGHTNING_F_X11,               // 1447
    S_LIGHTNING_F_X12,               // 1448
    S_LIGHTNING_F_X13,               // 1449
    S_LIGHTNING_F_X14,               // 1450
    S_LIGHTNING_F_X15,               // 1451
    S_LIGHTNING_F_X16,               // 1452
    S_LIGHTNING_F_X17,               // 1453
    S_LIGHTNING_F_X18,               // 1454
    S_LIGHTNING_F_X19,               // 1455
    S_LIGHTNING_ZAP1,               // 1456
    S_LIGHTNING_ZAP2,               // 1457
    S_LIGHTNING_ZAP3,               // 1458
    S_LIGHTNING_ZAP4,               // 1459
    S_LIGHTNING_ZAP5,               // 1460
    S_LIGHTNING_ZAP_X1,               // 1461
    S_LIGHTNING_ZAP_X2,               // 1462
    S_LIGHTNING_ZAP_X3,               // 1463
    S_LIGHTNING_ZAP_X4,               // 1464
    S_LIGHTNING_ZAP_X5,               // 1465
    S_LIGHTNING_ZAP_X6,               // 1466
    S_LIGHTNING_ZAP_X7,               // 1467
    S_LIGHTNING_ZAP_X8,               // 1468
    S_MSTAFFREADY,                   // 1469
    S_MSTAFFREADY2,                   // 1470
    S_MSTAFFREADY3,                   // 1471
    S_MSTAFFREADY4,                   // 1472
    S_MSTAFFREADY5,                   // 1473
    S_MSTAFFREADY6,                   // 1474
    S_MSTAFFREADY7,                   // 1475
    S_MSTAFFREADY8,                   // 1476
    S_MSTAFFREADY9,                   // 1477
    S_MSTAFFREADY10,               // 1478
    S_MSTAFFREADY11,               // 1479
    S_MSTAFFREADY12,               // 1480
    S_MSTAFFREADY13,               // 1481
    S_MSTAFFREADY14,               // 1482
    S_MSTAFFREADY15,               // 1483
    S_MSTAFFREADY16,               // 1484
    S_MSTAFFREADY17,               // 1485
    S_MSTAFFREADY18,               // 1486
    S_MSTAFFREADY19,               // 1487
    S_MSTAFFREADY20,               // 1488
    S_MSTAFFREADY21,               // 1489
    S_MSTAFFREADY22,               // 1490
    S_MSTAFFREADY23,               // 1491
    S_MSTAFFREADY24,               // 1492
    S_MSTAFFREADY25,               // 1493
    S_MSTAFFREADY26,               // 1494
    S_MSTAFFREADY27,               // 1495
    S_MSTAFFREADY28,               // 1496
    S_MSTAFFREADY29,               // 1497
    S_MSTAFFREADY30,               // 1498
    S_MSTAFFREADY31,               // 1499
    S_MSTAFFREADY32,               // 1500
    S_MSTAFFREADY33,               // 1501
    S_MSTAFFREADY34,               // 1502
    S_MSTAFFREADY35,               // 1503
    S_MSTAFFDOWN,                   // 1504
    S_MSTAFFUP,                       // 1505
    S_MSTAFFATK_1,                   // 1506
    S_MSTAFFATK_2,                   // 1507
    S_MSTAFFATK_3,                   // 1508
    S_MSTAFFATK_4,                   // 1509
    S_MSTAFFATK_5,                   // 1510
    S_MSTAFFATK_6,                   // 1511
    S_MSTAFFATK_7,                   // 1512
    S_MSTAFF_FX1_1,                   // 1513
    S_MSTAFF_FX1_2,                   // 1514
    S_MSTAFF_FX1_3,                   // 1515
    S_MSTAFF_FX1_4,                   // 1516
    S_MSTAFF_FX1_5,                   // 1517
    S_MSTAFF_FX1_6,                   // 1518
    S_MSTAFF_FX_X1,                   // 1519
    S_MSTAFF_FX_X2,                   // 1520
    S_MSTAFF_FX_X3,                   // 1521
    S_MSTAFF_FX_X4,                   // 1522
    S_MSTAFF_FX_X5,                   // 1523
    S_MSTAFF_FX_X6,                   // 1524
    S_MSTAFF_FX_X7,                   // 1525
    S_MSTAFF_FX_X8,                   // 1526
    S_MSTAFF_FX_X9,                   // 1527
    S_MSTAFF_FX_X10,               // 1528
    S_MSTAFF_FX2_1,                   // 1529
    S_MSTAFF_FX2_2,                   // 1530
    S_MSTAFF_FX2_3,                   // 1531
    S_MSTAFF_FX2_4,                   // 1532
    S_MSTAFF_FX2_X1,               // 1533
    S_MSTAFF_FX2_X2,               // 1534
    S_MSTAFF_FX2_X3,               // 1535
    S_MSTAFF_FX2_X4,               // 1536
    S_MSTAFF_FX2_X5,               // 1537
    S_FSWORD1,                       // 1538
    S_FSWORD2,                       // 1539
    S_FSWORD3,                       // 1540
    S_CHOLY1,                       // 1541
    S_CHOLY2,                       // 1542
    S_CHOLY3,                       // 1543
    S_MSTAFF1,                       // 1544
    S_MSTAFF2,                       // 1545
    S_MSTAFF3,                       // 1546
    S_SNOUTREADY,                   // 1547
    S_SNOUTDOWN,                   // 1548
    S_SNOUTUP,                       // 1549
    S_SNOUTATK1,                   // 1550
    S_SNOUTATK2,                   // 1551
    S_COS1,                           // 1552
    S_COS2,                           // 1553
    S_COS3,                           // 1554
    S_CONEREADY,                   // 1555
    S_CONEDOWN,                       // 1556
    S_CONEUP,                       // 1557
    S_CONEATK1_1,                   // 1558
    S_CONEATK1_2,                   // 1559
    S_CONEATK1_3,                   // 1560
    S_CONEATK1_4,                   // 1561
    S_CONEATK1_5,                   // 1562
    S_CONEATK1_6,                   // 1563
    S_CONEATK1_7,                   // 1564
    S_CONEATK1_8,                   // 1565
    S_SHARDFX1_1,                   // 1566
    S_SHARDFX1_2,                   // 1567
    S_SHARDFX1_3,                   // 1568
    S_SHARDFX1_4,                   // 1569
    S_SHARDFXE1_1,                   // 1570
    S_SHARDFXE1_2,                   // 1571
    S_SHARDFXE1_3,                   // 1572
    S_SHARDFXE1_4,                   // 1573
    S_SHARDFXE1_5,                   // 1574
    S_BLOOD1,                       // 1575
    S_BLOOD2,                       // 1576
    S_BLOOD3,                       // 1577
    S_BLOODSPLATTER1,               // 1578
    S_BLOODSPLATTER2,               // 1579
    S_BLOODSPLATTER3,               // 1580
    S_BLOODSPLATTERX,               // 1581
    S_GIBS1,                       // 1582
    S_FPLAY,                       // 1583
    S_FPLAY_RUN1,                   // 1584
    S_FPLAY_RUN2,                   // 1585
    S_FPLAY_RUN3,                   // 1586
    S_FPLAY_RUN4,                   // 1587
    S_FPLAY_ATK1,                   // 1588
    S_FPLAY_ATK2,                   // 1589
    S_FPLAY_PAIN,                   // 1590
    S_FPLAY_PAIN2,                   // 1591
    S_FPLAY_DIE1,                   // 1592
    S_FPLAY_DIE2,                   // 1593
    S_FPLAY_DIE3,                   // 1594
    S_FPLAY_DIE4,                   // 1595
    S_FPLAY_DIE5,                   // 1596
    S_FPLAY_DIE6,                   // 1597
    S_FPLAY_DIE7,                   // 1598
    S_FPLAY_XDIE1,                   // 1599
    S_FPLAY_XDIE2,                   // 1600
    S_FPLAY_XDIE3,                   // 1601
    S_FPLAY_XDIE4,                   // 1602
    S_FPLAY_XDIE5,                   // 1603
    S_FPLAY_XDIE6,                   // 1604
    S_FPLAY_XDIE7,                   // 1605
    S_FPLAY_XDIE8,                   // 1606
    S_FPLAY_ICE,                   // 1607
    S_FPLAY_ICE2,                   // 1608
    S_PLAY_F_FDTH1,                   // 1609
    S_PLAY_F_FDTH2,                   // 1610
    S_PLAY_C_FDTH1,                   // 1611
    S_PLAY_C_FDTH2,                   // 1612
    S_PLAY_M_FDTH1,                   // 1613
    S_PLAY_M_FDTH2,                   // 1614
    S_PLAY_FDTH3,                   // 1615
    S_PLAY_FDTH4,                   // 1616
    S_PLAY_FDTH5,                   // 1617
    S_PLAY_FDTH6,                   // 1618
    S_PLAY_FDTH7,                   // 1619
    S_PLAY_FDTH8,                   // 1620
    S_PLAY_FDTH9,                   // 1621
    S_PLAY_FDTH10,                   // 1622
    S_PLAY_FDTH11,                   // 1623
    S_PLAY_FDTH12,                   // 1624
    S_PLAY_FDTH13,                   // 1625
    S_PLAY_FDTH14,                   // 1626
    S_PLAY_FDTH15,                   // 1627
    S_PLAY_FDTH16,                   // 1628
    S_PLAY_FDTH17,                   // 1629
    S_PLAY_FDTH18,                   // 1630
    S_PLAY_FDTH19,                   // 1631
    S_PLAY_FDTH20,                   // 1632
    S_BLOODYSKULL1,                   // 1633
    S_BLOODYSKULL2,                   // 1634
    S_BLOODYSKULL3,                   // 1635
    S_BLOODYSKULL4,                   // 1636
    S_BLOODYSKULL5,                   // 1637
    S_BLOODYSKULL6,                   // 1638
    S_BLOODYSKULL7,                   // 1639
    S_BLOODYSKULLX1,               // 1640
    S_BLOODYSKULLX2,               // 1641
    S_PLAYER_SPEED1,               // 1642
    S_PLAYER_SPEED2,               // 1643
    S_ICECHUNK1,                   // 1644
    S_ICECHUNK2,                   // 1645
    S_ICECHUNK3,                   // 1646
    S_ICECHUNK4,                   // 1647
    S_ICECHUNK_HEAD,               // 1648
    S_ICECHUNK_HEAD2,               // 1649
    S_CPLAY,                       // 1650
    S_CPLAY_RUN1,                   // 1651
    S_CPLAY_RUN2,                   // 1652
    S_CPLAY_RUN3,                   // 1653
    S_CPLAY_RUN4,                   // 1654
    S_CPLAY_ATK1,                   // 1655
    S_CPLAY_ATK2,                   // 1656
    S_CPLAY_ATK3,                   // 1657
    S_CPLAY_PAIN,                   // 1658
    S_CPLAY_PAIN2,                   // 1659
    S_CPLAY_DIE1,                   // 1660
    S_CPLAY_DIE2,                   // 1661
    S_CPLAY_DIE3,                   // 1662
    S_CPLAY_DIE4,                   // 1663
    S_CPLAY_DIE5,                   // 1664
    S_CPLAY_DIE6,                   // 1665
    S_CPLAY_DIE7,                   // 1666
    S_CPLAY_DIE8,                   // 1667
    S_CPLAY_DIE9,                   // 1668
    S_CPLAY_XDIE1,                   // 1669
    S_CPLAY_XDIE2,                   // 1670
    S_CPLAY_XDIE3,                   // 1671
    S_CPLAY_XDIE4,                   // 1672
    S_CPLAY_XDIE5,                   // 1673
    S_CPLAY_XDIE6,                   // 1674
    S_CPLAY_XDIE7,                   // 1675
    S_CPLAY_XDIE8,                   // 1676
    S_CPLAY_XDIE9,                   // 1677
    S_CPLAY_XDIE10,                   // 1678
    S_CPLAY_ICE,                   // 1679
    S_CPLAY_ICE2,                   // 1680
    S_MPLAY,                       // 1681
    S_MPLAY_RUN1,                   // 1682
    S_MPLAY_RUN2,                   // 1683
    S_MPLAY_RUN3,                   // 1684
    S_MPLAY_RUN4,                   // 1685
    S_MPLAY_ATK1,                   // 1686
    S_MPLAY_ATK2,                   // 1687
    S_MPLAY_PAIN,                   // 1688
    S_MPLAY_PAIN2,                   // 1689
    S_MPLAY_DIE1,                   // 1690
    S_MPLAY_DIE2,                   // 1691
    S_MPLAY_DIE3,                   // 1692
    S_MPLAY_DIE4,                   // 1693
    S_MPLAY_DIE5,                   // 1694
    S_MPLAY_DIE6,                   // 1695
    S_MPLAY_DIE7,                   // 1696
    S_MPLAY_XDIE1,                   // 1697
    S_MPLAY_XDIE2,                   // 1698
    S_MPLAY_XDIE3,                   // 1699
    S_MPLAY_XDIE4,                   // 1700
    S_MPLAY_XDIE5,                   // 1701
    S_MPLAY_XDIE6,                   // 1702
    S_MPLAY_XDIE7,                   // 1703
    S_MPLAY_XDIE8,                   // 1704
    S_MPLAY_XDIE9,                   // 1705
    S_MPLAY_ICE,                   // 1706
    S_MPLAY_ICE2,                   // 1707
    S_PIGPLAY,                       // 1708
    S_PIGPLAY_RUN1,                   // 1709
    S_PIGPLAY_RUN2,                   // 1710
    S_PIGPLAY_RUN3,                   // 1711
    S_PIGPLAY_RUN4,                   // 1712
    S_PIGPLAY_ATK1,                   // 1713
    S_PIGPLAY_PAIN,                   // 1714
    S_PIG_LOOK1,                   // 1715
    S_PIG_WALK1,                   // 1716
    S_PIG_WALK2,                   // 1717
    S_PIG_WALK3,                   // 1718
    S_PIG_WALK4,                   // 1719
    S_PIG_PAIN,                       // 1720
    S_PIG_ATK1,                       // 1721
    S_PIG_ATK2,                       // 1722
    S_PIG_DIE1,                       // 1723
    S_PIG_DIE2,                       // 1724
    S_PIG_DIE3,                       // 1725
    S_PIG_DIE4,                       // 1726
    S_PIG_DIE5,                       // 1727
    S_PIG_DIE6,                       // 1728
    S_PIG_DIE7,                       // 1729
    S_PIG_DIE8,                       // 1730
    S_PIG_ICE,                       // 1731
    S_PIG_ICE2,                       // 1732
    S_CENTAUR_LOOK1,               // 1733
    S_CENTAUR_LOOK2,               // 1734
    S_CENTAUR_WALK1,               // 1735
    S_CENTAUR_WALK2,               // 1736
    S_CENTAUR_WALK3,               // 1737
    S_CENTAUR_WALK4,               // 1738
    S_CENTAUR_ATK1,                   // 1739
    S_CENTAUR_ATK2,                   // 1740
    S_CENTAUR_ATK3,                   // 1741
    S_CENTAUR_MISSILE1,               // 1742
    S_CENTAUR_MISSILE2,               // 1743
    S_CENTAUR_MISSILE3,               // 1744
    S_CENTAUR_MISSILE4,               // 1745
    S_CENTAUR_PAIN1,               // 1746
    S_CENTAUR_PAIN2,               // 1747
    S_CENTAUR_PAIN3,               // 1748
    S_CENTAUR_PAIN4,               // 1749
    S_CENTAUR_PAIN5,               // 1750
    S_CENTAUR_PAIN6,               // 1751
    S_CENTAUR_DEATH1,               // 1752
    S_CENTAUR_DEATH2,               // 1753
    S_CENTAUR_DEATH3,               // 1754
    S_CENTAUR_DEATH4,               // 1755
    S_CENTAUR_DEATH5,               // 1756
    S_CENTAUR_DEATH6,               // 1757
    S_CENTAUR_DEATH7,               // 1758
    S_CENTAUR_DEATH8,               // 1759
    S_CENTAUR_DEATH9,               // 1760
    S_CENTAUR_DEATH0,               // 1761
    S_CENTAUR_DEATH_X1,               // 1762
    S_CENTAUR_DEATH_X2,               // 1763
    S_CENTAUR_DEATH_X3,               // 1764
    S_CENTAUR_DEATH_X4,               // 1765
    S_CENTAUR_DEATH_X5,               // 1766
    S_CENTAUR_DEATH_X6,               // 1767
    S_CENTAUR_DEATH_X7,               // 1768
    S_CENTAUR_DEATH_X8,               // 1769
    S_CENTAUR_DEATH_X9,               // 1770
    S_CENTAUR_DEATH_X10,           // 1771
    S_CENTAUR_DEATH_X11,           // 1772
    S_CENTAUR_ICE,                   // 1773
    S_CENTAUR_ICE2,                   // 1774
    S_CENTAUR_FX1,                   // 1775
    S_CENTAUR_FX_X1,               // 1776
    S_CENTAUR_FX_X2,               // 1777
    S_CENTAUR_FX_X3,               // 1778
    S_CENTAUR_FX_X4,               // 1779
    S_CENTAUR_FX_X5,               // 1780
    S_CENTAUR_SHIELD1,               // 1781
    S_CENTAUR_SHIELD2,               // 1782
    S_CENTAUR_SHIELD3,               // 1783
    S_CENTAUR_SHIELD4,               // 1784
    S_CENTAUR_SHIELD5,               // 1785
    S_CENTAUR_SHIELD6,               // 1786
    S_CENTAUR_SHIELD_X1,           // 1787
    S_CENTAUR_SHIELD_X2,           // 1788
    S_CENTAUR_SHIELD_X3,           // 1789
    S_CENTAUR_SHIELD_X4,           // 1790
    S_CENTAUR_SWORD1,               // 1791
    S_CENTAUR_SWORD2,               // 1792
    S_CENTAUR_SWORD3,               // 1793
    S_CENTAUR_SWORD4,               // 1794
    S_CENTAUR_SWORD5,               // 1795
    S_CENTAUR_SWORD6,               // 1796
    S_CENTAUR_SWORD7,               // 1797
    S_CENTAUR_SWORD_X1,               // 1798
    S_CENTAUR_SWORD_X2,               // 1799
    S_CENTAUR_SWORD_X3,               // 1800
    S_DEMN_LOOK1,                   // 1801
    S_DEMN_LOOK2,                   // 1802
    S_DEMN_CHASE1,                   // 1803
    S_DEMN_CHASE2,                   // 1804
    S_DEMN_CHASE3,                   // 1805
    S_DEMN_CHASE4,                   // 1806
    S_DEMN_ATK1_1,                   // 1807
    S_DEMN_ATK1_2,                   // 1808
    S_DEMN_ATK1_3,                   // 1809
    S_DEMN_ATK2_1,                   // 1810
    S_DEMN_ATK2_2,                   // 1811
    S_DEMN_ATK2_3,                   // 1812
    S_DEMN_PAIN1,                   // 1813
    S_DEMN_PAIN2,                   // 1814
    S_DEMN_DEATH1,                   // 1815
    S_DEMN_DEATH2,                   // 1816
    S_DEMN_DEATH3,                   // 1817
    S_DEMN_DEATH4,                   // 1818
    S_DEMN_DEATH5,                   // 1819
    S_DEMN_DEATH6,                   // 1820
    S_DEMN_DEATH7,                   // 1821
    S_DEMN_DEATH8,                   // 1822
    S_DEMN_DEATH9,                   // 1823
    S_DEMN_XDEATH1,                   // 1824
    S_DEMN_XDEATH2,                   // 1825
    S_DEMN_XDEATH3,                   // 1826
    S_DEMN_XDEATH4,                   // 1827
    S_DEMN_XDEATH5,                   // 1828
    S_DEMN_XDEATH6,                   // 1829
    S_DEMN_XDEATH7,                   // 1830
    S_DEMN_XDEATH8,                   // 1831
    S_DEMN_XDEATH9,                   // 1832
    S_DEMON_ICE,                   // 1833
    S_DEMON_ICE2,                   // 1834
    S_DEMONCHUNK1_1,               // 1835
    S_DEMONCHUNK1_2,               // 1836
    S_DEMONCHUNK1_3,               // 1837
    S_DEMONCHUNK1_4,               // 1838
    S_DEMONCHUNK2_1,               // 1839
    S_DEMONCHUNK2_2,               // 1840
    S_DEMONCHUNK2_3,               // 1841
    S_DEMONCHUNK2_4,               // 1842
    S_DEMONCHUNK3_1,               // 1843
    S_DEMONCHUNK3_2,               // 1844
    S_DEMONCHUNK3_3,               // 1845
    S_DEMONCHUNK3_4,               // 1846
    S_DEMONCHUNK4_1,               // 1847
    S_DEMONCHUNK4_2,               // 1848
    S_DEMONCHUNK4_3,               // 1849
    S_DEMONCHUNK4_4,               // 1850
    S_DEMONCHUNK5_1,               // 1851
    S_DEMONCHUNK5_2,               // 1852
    S_DEMONCHUNK5_3,               // 1853
    S_DEMONCHUNK5_4,               // 1854
    S_DEMONFX_MOVE1,               // 1855
    S_DEMONFX_MOVE2,               // 1856
    S_DEMONFX_MOVE3,               // 1857
    S_DEMONFX_BOOM1,               // 1858
    S_DEMONFX_BOOM2,               // 1859
    S_DEMONFX_BOOM3,               // 1860
    S_DEMONFX_BOOM4,               // 1861
    S_DEMONFX_BOOM5,               // 1862
    S_DEMN2_LOOK1,                   // 1863
    S_DEMN2_LOOK2,                   // 1864
    S_DEMN2_CHASE1,                   // 1865
    S_DEMN2_CHASE2,                   // 1866
    S_DEMN2_CHASE3,                   // 1867
    S_DEMN2_CHASE4,                   // 1868
    S_DEMN2_ATK1_1,                   // 1869
    S_DEMN2_ATK1_2,                   // 1870
    S_DEMN2_ATK1_3,                   // 1871
    S_DEMN2_ATK2_1,                   // 1872
    S_DEMN2_ATK2_2,                   // 1873
    S_DEMN2_ATK2_3,                   // 1874
    S_DEMN2_PAIN1,                   // 1875
    S_DEMN2_PAIN2,                   // 1876
    S_DEMN2_DEATH1,                   // 1877
    S_DEMN2_DEATH2,                   // 1878
    S_DEMN2_DEATH3,                   // 1879
    S_DEMN2_DEATH4,                   // 1880
    S_DEMN2_DEATH5,                   // 1881
    S_DEMN2_DEATH6,                   // 1882
    S_DEMN2_DEATH7,                   // 1883
    S_DEMN2_DEATH8,                   // 1884
    S_DEMN2_DEATH9,                   // 1885
    S_DEMN2_XDEATH1,               // 1886
    S_DEMN2_XDEATH2,               // 1887
    S_DEMN2_XDEATH3,               // 1888
    S_DEMN2_XDEATH4,               // 1889
    S_DEMN2_XDEATH5,               // 1890
    S_DEMN2_XDEATH6,               // 1891
    S_DEMN2_XDEATH7,               // 1892
    S_DEMN2_XDEATH8,               // 1893
    S_DEMN2_XDEATH9,               // 1894
    S_DEMON2CHUNK1_1,               // 1895
    S_DEMON2CHUNK1_2,               // 1896
    S_DEMON2CHUNK1_3,               // 1897
    S_DEMON2CHUNK1_4,               // 1898
    S_DEMON2CHUNK2_1,               // 1899
    S_DEMON2CHUNK2_2,               // 1900
    S_DEMON2CHUNK2_3,               // 1901
    S_DEMON2CHUNK2_4,               // 1902
    S_DEMON2CHUNK3_1,               // 1903
    S_DEMON2CHUNK3_2,               // 1904
    S_DEMON2CHUNK3_3,               // 1905
    S_DEMON2CHUNK3_4,               // 1906
    S_DEMON2CHUNK4_1,               // 1907
    S_DEMON2CHUNK4_2,               // 1908
    S_DEMON2CHUNK4_3,               // 1909
    S_DEMON2CHUNK4_4,               // 1910
    S_DEMON2CHUNK5_1,               // 1911
    S_DEMON2CHUNK5_2,               // 1912
    S_DEMON2CHUNK5_3,               // 1913
    S_DEMON2CHUNK5_4,               // 1914
    S_DEMON2FX_MOVE1,               // 1915
    S_DEMON2FX_MOVE2,               // 1916
    S_DEMON2FX_MOVE3,               // 1917
    S_DEMON2FX_MOVE4,               // 1918
    S_DEMON2FX_MOVE5,               // 1919
    S_DEMON2FX_MOVE6,               // 1920
    S_DEMON2FX_BOOM1,               // 1921
    S_DEMON2FX_BOOM2,               // 1922
    S_DEMON2FX_BOOM3,               // 1923
    S_DEMON2FX_BOOM4,               // 1924
    S_DEMON2FX_BOOM5,               // 1925
    S_DEMON2FX_BOOM6,               // 1926
    S_WRAITH_RAISE1,               // 1927
    S_WRAITH_RAISE2,               // 1928
    S_WRAITH_RAISE3,               // 1929
    S_WRAITH_RAISE4,               // 1930
    S_WRAITH_RAISE5,               // 1931
    S_WRAITH_INIT1,                   // 1932
    S_WRAITH_INIT2,                   // 1933
    S_WRAITH_LOOK1,                   // 1934
    S_WRAITH_LOOK2,                   // 1935
    S_WRAITH_CHASE1,               // 1936
    S_WRAITH_CHASE2,               // 1937
    S_WRAITH_CHASE3,               // 1938
    S_WRAITH_CHASE4,               // 1939
    S_WRAITH_ATK1_1,               // 1940
    S_WRAITH_ATK1_2,               // 1941
    S_WRAITH_ATK1_3,               // 1942
    S_WRAITH_ATK2_1,               // 1943
    S_WRAITH_ATK2_2,               // 1944
    S_WRAITH_ATK2_3,               // 1945
    S_WRAITH_PAIN1,                   // 1946
    S_WRAITH_PAIN2,                   // 1947
    S_WRAITH_DEATH1_1,               // 1948
    S_WRAITH_DEATH1_2,               // 1949
    S_WRAITH_DEATH1_3,               // 1950
    S_WRAITH_DEATH1_4,               // 1951
    S_WRAITH_DEATH1_5,               // 1952
    S_WRAITH_DEATH1_6,               // 1953
    S_WRAITH_DEATH1_7,               // 1954
    S_WRAITH_DEATH1_8,               // 1955
    S_WRAITH_DEATH1_9,               // 1956
    S_WRAITH_DEATH1_0,               // 1957
    S_WRAITH_DEATH2_1,               // 1958
    S_WRAITH_DEATH2_2,               // 1959
    S_WRAITH_DEATH2_3,               // 1960
    S_WRAITH_DEATH2_4,               // 1961
    S_WRAITH_DEATH2_5,               // 1962
    S_WRAITH_DEATH2_6,               // 1963
    S_WRAITH_DEATH2_7,               // 1964
    S_WRAITH_DEATH2_8,               // 1965
    S_WRAITH_ICE,                   // 1966
    S_WRAITH_ICE2,                   // 1967
    S_WRTHFX_MOVE1,                   // 1968
    S_WRTHFX_MOVE2,                   // 1969
    S_WRTHFX_MOVE3,                   // 1970
    S_WRTHFX_BOOM1,                   // 1971
    S_WRTHFX_BOOM2,                   // 1972
    S_WRTHFX_BOOM3,                   // 1973
    S_WRTHFX_BOOM4,                   // 1974
    S_WRTHFX_BOOM5,                   // 1975
    S_WRTHFX_BOOM6,                   // 1976
    S_WRTHFX_SIZZLE1,               // 1977
    S_WRTHFX_SIZZLE2,               // 1978
    S_WRTHFX_SIZZLE3,               // 1979
    S_WRTHFX_SIZZLE4,               // 1980
    S_WRTHFX_SIZZLE5,               // 1981
    S_WRTHFX_SIZZLE6,               // 1982
    S_WRTHFX_SIZZLE7,               // 1983
    S_WRTHFX_DROP1,                   // 1984
    S_WRTHFX_DROP2,                   // 1985
    S_WRTHFX_DROP3,                   // 1986
    S_WRTHFX_DEAD1,                   // 1987
    S_WRTHFX_ADROP1,               // 1988
    S_WRTHFX_ADROP2,               // 1989
    S_WRTHFX_ADROP3,               // 1990
    S_WRTHFX_ADROP4,               // 1991
    S_WRTHFX_ADEAD1,               // 1992
    S_WRTHFX_BDROP1,               // 1993
    S_WRTHFX_BDROP2,               // 1994
    S_WRTHFX_BDROP3,               // 1995
    S_WRTHFX_BDEAD1,               // 1996
    S_MNTR_SPAWN1,                   // 1997
    S_MNTR_SPAWN2,                   // 1998
    S_MNTR_SPAWN3,                   // 1999
    S_MNTR_LOOK1,                   // 2000
    S_MNTR_LOOK2,                   // 2001
    S_MNTR_WALK1,                   // 2002
    S_MNTR_WALK2,                   // 2003
    S_MNTR_WALK3,                   // 2004
    S_MNTR_WALK4,                   // 2005
    S_MNTR_ROAM1,                   // 2006
    S_MNTR_ROAM2,                   // 2007
    S_MNTR_ROAM3,                   // 2008
    S_MNTR_ROAM4,                   // 2009
    S_MNTR_ATK1_1,                   // 2010
    S_MNTR_ATK1_2,                   // 2011
    S_MNTR_ATK1_3,                   // 2012
    S_MNTR_ATK2_1,                   // 2013
    S_MNTR_ATK2_2,                   // 2014
    S_MNTR_ATK2_3,                   // 2015
    S_MNTR_ATK3_1,                   // 2016
    S_MNTR_ATK3_2,                   // 2017
    S_MNTR_ATK3_3,                   // 2018
    S_MNTR_ATK3_4,                   // 2019
    S_MNTR_ATK4_1,                   // 2020
    S_MNTR_PAIN1,                   // 2021
    S_MNTR_PAIN2,                   // 2022
    S_MNTR_DIE1,                   // 2023
    S_MNTR_DIE2,                   // 2024
    S_MNTR_DIE3,                   // 2025
    S_MNTR_DIE4,                   // 2026
    S_MNTR_DIE5,                   // 2027
    S_MNTR_DIE6,                   // 2028
    S_MNTR_DIE7,                   // 2029
    S_MNTR_DIE8,                   // 2030
    S_MNTR_DIE9,                   // 2031
    S_MNTRFX1_1,                   // 2032
    S_MNTRFX1_2,                   // 2033
    S_MNTRFXI1_1,                   // 2034
    S_MNTRFXI1_2,                   // 2035
    S_MNTRFXI1_3,                   // 2036
    S_MNTRFXI1_4,                   // 2037
    S_MNTRFXI1_5,                   // 2038
    S_MNTRFXI1_6,                   // 2039
    S_MNTRFX2_1,                   // 2040
    S_MNTRFXI2_1,                   // 2041
    S_MNTRFXI2_2,                   // 2042
    S_MNTRFXI2_3,                   // 2043
    S_MNTRFXI2_4,                   // 2044
    S_MNTRFXI2_5,                   // 2045
    S_MNTRFX3_1,                   // 2046
    S_MNTRFX3_2,                   // 2047
    S_MNTRFX3_3,                   // 2048
    S_MNTRFX3_4,                   // 2049
    S_MNTRFX3_5,                   // 2050
    S_MNTRFX3_6,                   // 2051
    S_MNTRFX3_7,                   // 2052
    S_MNTRFX3_8,                   // 2053
    S_MNTRFX3_9,                   // 2054
    S_MINOSMOKE1,                   // 2055
    S_MINOSMOKE2,                   // 2056
    S_MINOSMOKE3,                   // 2057
    S_MINOSMOKE4,                   // 2058
    S_MINOSMOKE5,                   // 2059
    S_MINOSMOKE6,                   // 2060
    S_MINOSMOKE7,                   // 2061
    S_MINOSMOKE8,                   // 2062
    S_MINOSMOKE9,                   // 2063
    S_MINOSMOKE0,                   // 2064
    S_MINOSMOKEA,                   // 2065
    S_MINOSMOKEB,                   // 2066
    S_MINOSMOKEC,                   // 2067
    S_MINOSMOKED,                   // 2068
    S_MINOSMOKEE,                   // 2069
    S_MINOSMOKEF,                   // 2070
    S_MINOSMOKEG,                   // 2071
    S_MINOSMOKEX1,                   // 2072
    S_MINOSMOKEX2,                   // 2073
    S_MINOSMOKEX3,                   // 2074
    S_MINOSMOKEX4,                   // 2075
    S_MINOSMOKEX5,                   // 2076
    S_MINOSMOKEX6,                   // 2077
    S_MINOSMOKEX7,                   // 2078
    S_MINOSMOKEX8,                   // 2079
    S_MINOSMOKEX9,                   // 2080
    S_MINOSMOKEX0,                   // 2081
    S_MINOSMOKEXA,                   // 2082
    S_MINOSMOKEXB,                   // 2083
    S_MINOSMOKEXC,                   // 2084
    S_MINOSMOKEXD,                   // 2085
    S_MINOSMOKEXE,                   // 2086
    S_MINOSMOKEXF,                   // 2087
    S_MINOSMOKEXG,                   // 2088
    S_MINOSMOKEXH,                   // 2089
    S_MINOSMOKEXI,                   // 2090
    S_SERPENT_LOOK1,               // 2091
    S_SERPENT_SWIM1,               // 2092
    S_SERPENT_SWIM2,               // 2093
    S_SERPENT_SWIM3,               // 2094
    S_SERPENT_HUMP1,               // 2095
    S_SERPENT_HUMP2,               // 2096
    S_SERPENT_HUMP3,               // 2097
    S_SERPENT_HUMP4,               // 2098
    S_SERPENT_HUMP5,               // 2099
    S_SERPENT_HUMP6,               // 2100
    S_SERPENT_HUMP7,               // 2101
    S_SERPENT_HUMP8,               // 2102
    S_SERPENT_HUMP9,               // 2103
    S_SERPENT_HUMP10,               // 2104
    S_SERPENT_HUMP11,               // 2105
    S_SERPENT_HUMP12,               // 2106
    S_SERPENT_HUMP13,               // 2107
    S_SERPENT_HUMP14,               // 2108
    S_SERPENT_HUMP15,               // 2109
    S_SERPENT_SURFACE1,               // 2110
    S_SERPENT_SURFACE2,               // 2111
    S_SERPENT_SURFACE3,               // 2112
    S_SERPENT_SURFACE4,               // 2113
    S_SERPENT_SURFACE5,               // 2114
    S_SERPENT_DIVE1,               // 2115
    S_SERPENT_DIVE2,               // 2116
    S_SERPENT_DIVE3,               // 2117
    S_SERPENT_DIVE4,               // 2118
    S_SERPENT_DIVE5,               // 2119
    S_SERPENT_DIVE6,               // 2120
    S_SERPENT_DIVE7,               // 2121
    S_SERPENT_DIVE8,               // 2122
    S_SERPENT_DIVE9,               // 2123
    S_SERPENT_DIVE10,               // 2124
    S_SERPENT_WALK1,               // 2125
    S_SERPENT_WALK2,               // 2126
    S_SERPENT_WALK3,               // 2127
    S_SERPENT_WALK4,               // 2128
    S_SERPENT_PAIN1,               // 2129
    S_SERPENT_PAIN2,               // 2130
    S_SERPENT_ATK1,                   // 2131
    S_SERPENT_ATK2,                   // 2132
    S_SERPENT_MELEE1,               // 2133
    S_SERPENT_MISSILE1,               // 2134
    S_SERPENT_DIE1,                   // 2135
    S_SERPENT_DIE2,                   // 2136
    S_SERPENT_DIE3,                   // 2137
    S_SERPENT_DIE4,                   // 2138
    S_SERPENT_DIE5,                   // 2139
    S_SERPENT_DIE6,                   // 2140
    S_SERPENT_DIE7,                   // 2141
    S_SERPENT_DIE8,                   // 2142
    S_SERPENT_DIE9,                   // 2143
    S_SERPENT_DIE10,               // 2144
    S_SERPENT_DIE11,               // 2145
    S_SERPENT_DIE12,               // 2146
    S_SERPENT_XDIE1,               // 2147
    S_SERPENT_XDIE2,               // 2148
    S_SERPENT_XDIE3,               // 2149
    S_SERPENT_XDIE4,               // 2150
    S_SERPENT_XDIE5,               // 2151
    S_SERPENT_XDIE6,               // 2152
    S_SERPENT_XDIE7,               // 2153
    S_SERPENT_XDIE8,               // 2154
    S_SERPENT_ICE,                   // 2155
    S_SERPENT_ICE2,                   // 2156
    S_SERPENT_FX1,                   // 2157
    S_SERPENT_FX2,                   // 2158
    S_SERPENT_FX3,                   // 2159
    S_SERPENT_FX4,                   // 2160
    S_SERPENT_FX_X1,               // 2161
    S_SERPENT_FX_X2,               // 2162
    S_SERPENT_FX_X3,               // 2163
    S_SERPENT_FX_X4,               // 2164
    S_SERPENT_FX_X5,               // 2165
    S_SERPENT_FX_X6,               // 2166
    S_SERPENT_HEAD1,               // 2167
    S_SERPENT_HEAD2,               // 2168
    S_SERPENT_HEAD3,               // 2169
    S_SERPENT_HEAD4,               // 2170
    S_SERPENT_HEAD5,               // 2171
    S_SERPENT_HEAD6,               // 2172
    S_SERPENT_HEAD7,               // 2173
    S_SERPENT_HEAD8,               // 2174
    S_SERPENT_HEAD_X1,               // 2175
    S_SERPENT_GIB1_1,               // 2176
    S_SERPENT_GIB1_2,               // 2177
    S_SERPENT_GIB1_3,               // 2178
    S_SERPENT_GIB1_4,               // 2179
    S_SERPENT_GIB1_5,               // 2180
    S_SERPENT_GIB1_6,               // 2181
    S_SERPENT_GIB1_7,               // 2182
    S_SERPENT_GIB1_8,               // 2183
    S_SERPENT_GIB1_9,               // 2184
    S_SERPENT_GIB1_10,               // 2185
    S_SERPENT_GIB1_11,               // 2186
    S_SERPENT_GIB1_12,               // 2187
    S_SERPENT_GIB2_1,               // 2188
    S_SERPENT_GIB2_2,               // 2189
    S_SERPENT_GIB2_3,               // 2190
    S_SERPENT_GIB2_4,               // 2191
    S_SERPENT_GIB2_5,               // 2192
    S_SERPENT_GIB2_6,               // 2193
    S_SERPENT_GIB2_7,               // 2194
    S_SERPENT_GIB2_8,               // 2195
    S_SERPENT_GIB2_9,               // 2196
    S_SERPENT_GIB2_10,               // 2197
    S_SERPENT_GIB2_11,               // 2198
    S_SERPENT_GIB2_12,               // 2199
    S_SERPENT_GIB3_1,               // 2200
    S_SERPENT_GIB3_2,               // 2201
    S_SERPENT_GIB3_3,               // 2202
    S_SERPENT_GIB3_4,               // 2203
    S_SERPENT_GIB3_5,               // 2204
    S_SERPENT_GIB3_6,               // 2205
    S_SERPENT_GIB3_7,               // 2206
    S_SERPENT_GIB3_8,               // 2207
    S_SERPENT_GIB3_9,               // 2208
    S_SERPENT_GIB3_10,               // 2209
    S_SERPENT_GIB3_11,               // 2210
    S_SERPENT_GIB3_12,               // 2211
    S_BISHOP_LOOK1,                   // 2212
    S_BISHOP_DECIDE,               // 2213
    S_BISHOP_BLUR1,                   // 2214
    S_BISHOP_BLUR2,                   // 2215
    S_BISHOP_WALK1,                   // 2216
    S_BISHOP_WALK2,                   // 2217
    S_BISHOP_WALK3,                   // 2218
    S_BISHOP_WALK4,                   // 2219
    S_BISHOP_WALK5,                   // 2220
    S_BISHOP_WALK6,                   // 2221
    S_BISHOP_ATK1,                   // 2222
    S_BISHOP_ATK2,                   // 2223
    S_BISHOP_ATK3,                   // 2224
    S_BISHOP_ATK4,                   // 2225
    S_BISHOP_ATK5,                   // 2226
    S_BISHOP_PAIN1,                   // 2227
    S_BISHOP_PAIN2,                   // 2228
    S_BISHOP_PAIN3,                   // 2229
    S_BISHOP_PAIN4,                   // 2230
    S_BISHOP_PAIN5,                   // 2231
    S_BISHOP_DEATH1,               // 2232
    S_BISHOP_DEATH2,               // 2233
    S_BISHOP_DEATH3,               // 2234
    S_BISHOP_DEATH4,               // 2235
    S_BISHOP_DEATH5,               // 2236
    S_BISHOP_DEATH6,               // 2237
    S_BISHOP_DEATH7,               // 2238
    S_BISHOP_DEATH8,               // 2239
    S_BISHOP_DEATH9,               // 2240
    S_BISHOP_DEATH10,               // 2241
    S_BISHOP_ICE,                   // 2242
    S_BISHOP_ICE2,                   // 2243
    S_BISHOP_PUFF1,                   // 2244
    S_BISHOP_PUFF2,                   // 2245
    S_BISHOP_PUFF3,                   // 2246
    S_BISHOP_PUFF4,                   // 2247
    S_BISHOP_PUFF5,                   // 2248
    S_BISHOP_PUFF6,                   // 2249
    S_BISHOP_PUFF7,                   // 2250
    S_BISHOPBLUR1,                   // 2251
    S_BISHOPBLUR2,                   // 2252
    S_BISHOPPAINBLUR1,               // 2253
    S_BISHFX1_1,                   // 2254
    S_BISHFX1_2,                   // 2255
    S_BISHFX1_3,                   // 2256
    S_BISHFX1_4,                   // 2257
    S_BISHFX1_5,                   // 2258
    S_BISHFXI1_1,                   // 2259
    S_BISHFXI1_2,                   // 2260
    S_BISHFXI1_3,                   // 2261
    S_BISHFXI1_4,                   // 2262
    S_BISHFXI1_5,                   // 2263
    S_BISHFXI1_6,                   // 2264
    S_DRAGON_LOOK1,                   // 2265
    S_DRAGON_INIT,                   // 2266
    S_DRAGON_INIT2,                   // 2267
    S_DRAGON_INIT3,                   // 2268
    S_DRAGON_WALK1,                   // 2269
    S_DRAGON_WALK2,                   // 2270
    S_DRAGON_WALK3,                   // 2271
    S_DRAGON_WALK4,                   // 2272
    S_DRAGON_WALK5,                   // 2273
    S_DRAGON_WALK6,                   // 2274
    S_DRAGON_WALK7,                   // 2275
    S_DRAGON_WALK8,                   // 2276
    S_DRAGON_WALK9,                   // 2277
    S_DRAGON_WALK10,               // 2278
    S_DRAGON_WALK11,               // 2279
    S_DRAGON_WALK12,               // 2280
    S_DRAGON_ATK1,                   // 2281
    S_DRAGON_PAIN1,                   // 2282
    S_DRAGON_DEATH1,               // 2283
    S_DRAGON_DEATH2,               // 2284
    S_DRAGON_DEATH3,               // 2285
    S_DRAGON_DEATH4,               // 2286
    S_DRAGON_CRASH1,               // 2287
    S_DRAGON_CRASH2,               // 2288
    S_DRAGON_CRASH3,               // 2289
    S_DRAGON_FX1_1,                   // 2290
    S_DRAGON_FX1_2,                   // 2291
    S_DRAGON_FX1_3,                   // 2292
    S_DRAGON_FX1_4,                   // 2293
    S_DRAGON_FX1_5,                   // 2294
    S_DRAGON_FX1_6,                   // 2295
    S_DRAGON_FX1_X1,               // 2296
    S_DRAGON_FX1_X2,               // 2297
    S_DRAGON_FX1_X3,               // 2298
    S_DRAGON_FX1_X4,               // 2299
    S_DRAGON_FX1_X5,               // 2300
    S_DRAGON_FX1_X6,               // 2301
    S_DRAGON_FX2_1,                   // 2302
    S_DRAGON_FX2_2,                   // 2303
    S_DRAGON_FX2_3,                   // 2304
    S_DRAGON_FX2_4,                   // 2305
    S_DRAGON_FX2_5,                   // 2306
    S_DRAGON_FX2_6,                   // 2307
    S_DRAGON_FX2_7,                   // 2308
    S_DRAGON_FX2_8,                   // 2309
    S_DRAGON_FX2_9,                   // 2310
    S_DRAGON_FX2_10,               // 2311
    S_DRAGON_FX2_11,               // 2312
    S_ARMOR_1,                       // 2313
    S_ARMOR_2,                       // 2314
    S_ARMOR_3,                       // 2315
    S_ARMOR_4,                       // 2316
    S_MANA1_1,                       // 2317
    S_MANA1_2,                       // 2318
    S_MANA1_3,                       // 2319
    S_MANA1_4,                       // 2320
    S_MANA1_5,                       // 2321
    S_MANA1_6,                       // 2322
    S_MANA1_7,                       // 2323
    S_MANA1_8,                       // 2324
    S_MANA1_9,                       // 2325
    S_MANA2_1,                       // 2326
    S_MANA2_2,                       // 2327
    S_MANA2_3,                       // 2328
    S_MANA2_4,                       // 2329
    S_MANA2_5,                       // 2330
    S_MANA2_6,                       // 2331
    S_MANA2_7,                       // 2332
    S_MANA2_8,                       // 2333
    S_MANA2_9,                       // 2334
    S_MANA2_10,                       // 2335
    S_MANA2_11,                       // 2336
    S_MANA2_12,                       // 2337
    S_MANA2_13,                       // 2338
    S_MANA2_14,                       // 2339
    S_MANA2_15,                       // 2340
    S_MANA2_16,                       // 2341
    S_MANA3_1,                       // 2342
    S_MANA3_2,                       // 2343
    S_MANA3_3,                       // 2344
    S_MANA3_4,                       // 2345
    S_MANA3_5,                       // 2346
    S_MANA3_6,                       // 2347
    S_MANA3_7,                       // 2348
    S_MANA3_8,                       // 2349
    S_MANA3_9,                       // 2350
    S_MANA3_10,                       // 2351
    S_MANA3_11,                       // 2352
    S_MANA3_12,                       // 2353
    S_MANA3_13,                       // 2354
    S_MANA3_14,                       // 2355
    S_MANA3_15,                       // 2356
    S_MANA3_16,                       // 2357
    S_KEY1,                           // 2358
    S_KEY2,                           // 2359
    S_KEY3,                           // 2360
    S_KEY4,                           // 2361
    S_KEY5,                           // 2362
    S_KEY6,                           // 2363
    S_KEY7,                           // 2364
    S_KEY8,                           // 2365
    S_KEY9,                           // 2366
    S_KEYA,                           // 2367
    S_KEYB,                           // 2368
    S_SND_WIND1,                   // 2369
    S_SND_WIND2,                   // 2370
    S_SND_WATERFALL,               // 2371
    S_ETTIN_LOOK1,                   // 2372
    S_ETTIN_LOOK2,                   // 2373
    S_ETTIN_CHASE1,                   // 2374
    S_ETTIN_CHASE2,                   // 2375
    S_ETTIN_CHASE3,                   // 2376
    S_ETTIN_CHASE4,                   // 2377
    S_ETTIN_PAIN1,                   // 2378
    S_ETTIN_ATK1_1,                   // 2379
    S_ETTIN_ATK1_2,                   // 2380
    S_ETTIN_ATK1_3,                   // 2381
    S_ETTIN_DEATH1_1,               // 2382
    S_ETTIN_DEATH1_2,               // 2383
    S_ETTIN_DEATH1_3,               // 2384
    S_ETTIN_DEATH1_4,               // 2385
    S_ETTIN_DEATH1_5,               // 2386
    S_ETTIN_DEATH1_6,               // 2387
    S_ETTIN_DEATH1_7,               // 2388
    S_ETTIN_DEATH1_8,               // 2389
    S_ETTIN_DEATH1_9,               // 2390
    S_ETTIN_DEATH2_1,               // 2391
    S_ETTIN_DEATH2_2,               // 2392
    S_ETTIN_DEATH2_3,               // 2393
    S_ETTIN_DEATH2_4,               // 2394
    S_ETTIN_DEATH2_5,               // 2395
    S_ETTIN_DEATH2_6,               // 2396
    S_ETTIN_DEATH2_7,               // 2397
    S_ETTIN_DEATH2_8,               // 2398
    S_ETTIN_DEATH2_9,               // 2399
    S_ETTIN_DEATH2_0,               // 2400
    S_ETTIN_DEATH2_A,               // 2401
    S_ETTIN_DEATH2_B,               // 2402
    S_ETTIN_ICE1,                   // 2403
    S_ETTIN_ICE2,                   // 2404
    S_ETTIN_MACE1,                   // 2405
    S_ETTIN_MACE2,                   // 2406
    S_ETTIN_MACE3,                   // 2407
    S_ETTIN_MACE4,                   // 2408
    S_ETTIN_MACE5,                   // 2409
    S_ETTIN_MACE6,                   // 2410
    S_ETTIN_MACE7,                   // 2411
    S_FIRED_SPAWN1,                   // 2412
    S_FIRED_LOOK1,                   // 2413
    S_FIRED_LOOK2,                   // 2414
    S_FIRED_LOOK3,                   // 2415
    S_FIRED_LOOK4,                   // 2416
    S_FIRED_LOOK5,                   // 2417
    S_FIRED_LOOK6,                   // 2418
    S_FIRED_LOOK7,                   // 2419
    S_FIRED_LOOK8,                   // 2420
    S_FIRED_LOOK9,                   // 2421
    S_FIRED_LOOK0,                   // 2422
    S_FIRED_LOOKA,                   // 2423
    S_FIRED_LOOKB,                   // 2424
    S_FIRED_WALK1,                   // 2425
    S_FIRED_WALK2,                   // 2426
    S_FIRED_WALK3,                   // 2427
    S_FIRED_PAIN1,                   // 2428
    S_FIRED_ATTACK1,               // 2429
    S_FIRED_ATTACK2,               // 2430
    S_FIRED_ATTACK3,               // 2431
    S_FIRED_ATTACK4,               // 2432
    S_FIRED_DEATH1,                   // 2433
    S_FIRED_DEATH2,                   // 2434
    S_FIRED_DEATH3,                   // 2435
    S_FIRED_DEATH4,                   // 2436
    S_FIRED_XDEATH1,               // 2437
    S_FIRED_XDEATH2,               // 2438
    S_FIRED_XDEATH3,               // 2439
    S_FIRED_ICE1,                   // 2440
    S_FIRED_ICE2,                   // 2441
    S_FIRED_CORPSE1,               // 2442
    S_FIRED_CORPSE2,               // 2443
    S_FIRED_CORPSE3,               // 2444
    S_FIRED_CORPSE4,               // 2445
    S_FIRED_CORPSE5,               // 2446
    S_FIRED_CORPSE6,               // 2447
    S_FIRED_RDROP1,                   // 2448
    S_FIRED_RDEAD1_1,               // 2449
    S_FIRED_RDEAD1_2,               // 2450
    S_FIRED_RDROP2,                   // 2451
    S_FIRED_RDEAD2_1,               // 2452
    S_FIRED_RDEAD2_2,               // 2453
    S_FIRED_RDROP3,                   // 2454
    S_FIRED_RDEAD3_1,               // 2455
    S_FIRED_RDEAD3_2,               // 2456
    S_FIRED_RDROP4,                   // 2457
    S_FIRED_RDEAD4_1,               // 2458
    S_FIRED_RDEAD4_2,               // 2459
    S_FIRED_RDROP5,                   // 2460
    S_FIRED_RDEAD5_1,               // 2461
    S_FIRED_RDEAD5_2,               // 2462
    S_FIRED_FX6_1,                   // 2463
    S_FIRED_FX6_2,                   // 2464
    S_FIRED_FX6_3,                   // 2465
    S_FIRED_FX6_4,                   // 2466
    S_FIRED_FX6_5,                   // 2467
    S_ICEGUY_LOOK,                   // 2468
    S_ICEGUY_DORMANT,               // 2469
    S_ICEGUY_WALK1,                   // 2470
    S_ICEGUY_WALK2,                   // 2471
    S_ICEGUY_WALK3,                   // 2472
    S_ICEGUY_WALK4,                   // 2473
    S_ICEGUY_ATK1,                   // 2474
    S_ICEGUY_ATK2,                   // 2475
    S_ICEGUY_ATK3,                   // 2476
    S_ICEGUY_ATK4,                   // 2477
    S_ICEGUY_PAIN1,                   // 2478
    S_ICEGUY_DEATH,                   // 2479
    S_ICEGUY_FX1,                   // 2480
    S_ICEGUY_FX2,                   // 2481
    S_ICEGUY_FX3,                   // 2482
    S_ICEGUY_FX_X1,                   // 2483
    S_ICEGUY_FX_X2,                   // 2484
    S_ICEGUY_FX_X3,                   // 2485
    S_ICEGUY_FX_X4,                   // 2486
    S_ICEGUY_FX_X5,                   // 2487
    S_ICEFX_PUFF1,                   // 2488
    S_ICEFX_PUFF2,                   // 2489
    S_ICEFX_PUFF3,                   // 2490
    S_ICEFX_PUFF4,                   // 2491
    S_ICEFX_PUFF5,                   // 2492
    S_ICEGUY_FX2_1,                   // 2493
    S_ICEGUY_FX2_2,                   // 2494
    S_ICEGUY_FX2_3,                   // 2495
    S_ICEGUY_BIT1,                   // 2496
    S_ICEGUY_BIT2,                   // 2497
    S_ICEGUY_WISP1_1,               // 2498
    S_ICEGUY_WISP1_2,               // 2499
    S_ICEGUY_WISP1_3,               // 2500
    S_ICEGUY_WISP1_4,               // 2501
    S_ICEGUY_WISP1_5,               // 2502
    S_ICEGUY_WISP1_6,               // 2503
    S_ICEGUY_WISP1_7,               // 2504
    S_ICEGUY_WISP1_8,               // 2505
    S_ICEGUY_WISP1_9,               // 2506
    S_ICEGUY_WISP2_1,               // 2507
    S_ICEGUY_WISP2_2,               // 2508
    S_ICEGUY_WISP2_3,               // 2509
    S_ICEGUY_WISP2_4,               // 2510
    S_ICEGUY_WISP2_5,               // 2511
    S_ICEGUY_WISP2_6,               // 2512
    S_ICEGUY_WISP2_7,               // 2513
    S_ICEGUY_WISP2_8,               // 2514
    S_ICEGUY_WISP2_9,               // 2515
    S_FIGHTER,                       // 2516
    S_FIGHTER2,                       // 2517
    S_FIGHTERLOOK,                   // 2518
    S_FIGHTER_RUN1,                   // 2519
    S_FIGHTER_RUN2,                   // 2520
    S_FIGHTER_RUN3,                   // 2521
    S_FIGHTER_RUN4,                   // 2522
    S_FIGHTER_ATK1,                   // 2523
    S_FIGHTER_ATK2,                   // 2524
    S_FIGHTER_PAIN,                   // 2525
    S_FIGHTER_PAIN2,               // 2526
    S_FIGHTER_DIE1,                   // 2527
    S_FIGHTER_DIE2,                   // 2528
    S_FIGHTER_DIE3,                   // 2529
    S_FIGHTER_DIE4,                   // 2530
    S_FIGHTER_DIE5,                   // 2531
    S_FIGHTER_DIE6,                   // 2532
    S_FIGHTER_DIE7,                   // 2533
    S_FIGHTER_XDIE1,               // 2534
    S_FIGHTER_XDIE2,               // 2535
    S_FIGHTER_XDIE3,               // 2536
    S_FIGHTER_XDIE4,               // 2537
    S_FIGHTER_XDIE5,               // 2538
    S_FIGHTER_XDIE6,               // 2539
    S_FIGHTER_XDIE7,               // 2540
    S_FIGHTER_XDIE8,               // 2541
    S_FIGHTER_ICE,                   // 2542
    S_FIGHTER_ICE2,                   // 2543
    S_CLERIC,                       // 2544
    S_CLERIC2,                       // 2545
    S_CLERICLOOK,                   // 2546
    S_CLERIC_RUN1,                   // 2547
    S_CLERIC_RUN2,                   // 2548
    S_CLERIC_RUN3,                   // 2549
    S_CLERIC_RUN4,                   // 2550
    S_CLERIC_ATK1,                   // 2551
    S_CLERIC_ATK2,                   // 2552
    S_CLERIC_ATK3,                   // 2553
    S_CLERIC_PAIN,                   // 2554
    S_CLERIC_PAIN2,                   // 2555
    S_CLERIC_DIE1,                   // 2556
    S_CLERIC_DIE2,                   // 2557
    S_CLERIC_DIE3,                   // 2558
    S_CLERIC_DIE4,                   // 2559
    S_CLERIC_DIE5,                   // 2560
    S_CLERIC_DIE6,                   // 2561
    S_CLERIC_DIE7,                   // 2562
    S_CLERIC_DIE8,                   // 2563
    S_CLERIC_DIE9,                   // 2564
    S_CLERIC_XDIE1,                   // 2565
    S_CLERIC_XDIE2,                   // 2566
    S_CLERIC_XDIE3,                   // 2567
    S_CLERIC_XDIE4,                   // 2568
    S_CLERIC_XDIE5,                   // 2569
    S_CLERIC_XDIE6,                   // 2570
    S_CLERIC_XDIE7,                   // 2571
    S_CLERIC_XDIE8,                   // 2572
    S_CLERIC_XDIE9,                   // 2573
    S_CLERIC_XDIE10,               // 2574
    S_CLERIC_ICE,                   // 2575
    S_CLERIC_ICE2,                   // 2576
    S_MAGE,                           // 2577
    S_MAGE2,                       // 2578
    S_MAGELOOK,                       // 2579
    S_MAGE_RUN1,                   // 2580
    S_MAGE_RUN2,                   // 2581
    S_MAGE_RUN3,                   // 2582
    S_MAGE_RUN4,                   // 2583
    S_MAGE_ATK1,                   // 2584
    S_MAGE_ATK2,                   // 2585
    S_MAGE_PAIN,                   // 2586
    S_MAGE_PAIN2,                   // 2587
    S_MAGE_DIE1,                   // 2588
    S_MAGE_DIE2,                   // 2589
    S_MAGE_DIE3,                   // 2590
    S_MAGE_DIE4,                   // 2591
    S_MAGE_DIE5,                   // 2592
    S_MAGE_DIE6,                   // 2593
    S_MAGE_DIE7,                   // 2594
    S_MAGE_XDIE1,                   // 2595
    S_MAGE_XDIE2,                   // 2596
    S_MAGE_XDIE3,                   // 2597
    S_MAGE_XDIE4,                   // 2598
    S_MAGE_XDIE5,                   // 2599
    S_MAGE_XDIE6,                   // 2600
    S_MAGE_XDIE7,                   // 2601
    S_MAGE_XDIE8,                   // 2602
    S_MAGE_XDIE9,                   // 2603
    S_MAGE_ICE,                       // 2604
    S_MAGE_ICE2,                   // 2605
    S_SORC_SPAWN1,                   // 2606
    S_SORC_SPAWN2,                   // 2607
    S_SORC_LOOK1,                   // 2608
    S_SORC_WALK1,                   // 2609
    S_SORC_WALK2,                   // 2610
    S_SORC_WALK3,                   // 2611
    S_SORC_WALK4,                   // 2612
    S_SORC_PAIN1,                   // 2613
    S_SORC_PAIN2,                   // 2614
    S_SORC_ATK2_1,                   // 2615
    S_SORC_ATK2_2,                   // 2616
    S_SORC_ATK2_3,                   // 2617
    S_SORC_ATTACK1,                   // 2618
    S_SORC_ATTACK2,                   // 2619
    S_SORC_ATTACK3,                   // 2620
    S_SORC_ATTACK4,                   // 2621
    S_SORC_ATTACK5,                   // 2622
    S_SORC_DIE1,                   // 2623
    S_SORC_DIE2,                   // 2624
    S_SORC_DIE3,                   // 2625
    S_SORC_DIE4,                   // 2626
    S_SORC_DIE5,                   // 2627
    S_SORC_DIE6,                   // 2628
    S_SORC_DIE7,                   // 2629
    S_SORC_DIE8,                   // 2630
    S_SORC_DIE9,                   // 2631
    S_SORC_DIE0,                   // 2632
    S_SORC_DIEA,                   // 2633
    S_SORC_DIEB,                   // 2634
    S_SORC_DIEC,                   // 2635
    S_SORC_DIED,                   // 2636
    S_SORC_DIEE,                   // 2637
    S_SORC_DIEF,                   // 2638
    S_SORC_DIEG,                   // 2639
    S_SORC_DIEH,                   // 2640
    S_SORC_DIEI,                   // 2641
    S_SORCBALL1_1,                   // 2642
    S_SORCBALL1_2,                   // 2643
    S_SORCBALL1_3,                   // 2644
    S_SORCBALL1_4,                   // 2645
    S_SORCBALL1_5,                   // 2646
    S_SORCBALL1_6,                   // 2647
    S_SORCBALL1_7,                   // 2648
    S_SORCBALL1_8,                   // 2649
    S_SORCBALL1_9,                   // 2650
    S_SORCBALL1_0,                   // 2651
    S_SORCBALL1_A,                   // 2652
    S_SORCBALL1_B,                   // 2653
    S_SORCBALL1_C,                   // 2654
    S_SORCBALL1_D,                   // 2655
    S_SORCBALL1_E,                   // 2656
    S_SORCBALL1_F,                   // 2657
    S_SORCBALL1_D1,                   // 2658
    S_SORCBALL1_D2,                   // 2659
    S_SORCBALL1_D5,                   // 2660
    S_SORCBALL1_D6,                   // 2661
    S_SORCBALL1_D7,                   // 2662
    S_SORCBALL1_D8,                   // 2663
    S_SORCBALL1_D9,                   // 2664
    S_SORCBALL2_1,                   // 2665
    S_SORCBALL2_2,                   // 2666
    S_SORCBALL2_3,                   // 2667
    S_SORCBALL2_4,                   // 2668
    S_SORCBALL2_5,                   // 2669
    S_SORCBALL2_6,                   // 2670
    S_SORCBALL2_7,                   // 2671
    S_SORCBALL2_8,                   // 2672
    S_SORCBALL2_9,                   // 2673
    S_SORCBALL2_0,                   // 2674
    S_SORCBALL2_A,                   // 2675
    S_SORCBALL2_B,                   // 2676
    S_SORCBALL2_C,                   // 2677
    S_SORCBALL2_D,                   // 2678
    S_SORCBALL2_E,                   // 2679
    S_SORCBALL2_F,                   // 2680
    S_SORCBALL2_D1,                   // 2681
    S_SORCBALL2_D2,                   // 2682
    S_SORCBALL2_D5,                   // 2683
    S_SORCBALL2_D6,                   // 2684
    S_SORCBALL2_D7,                   // 2685
    S_SORCBALL2_D8,                   // 2686
    S_SORCBALL2_D9,                   // 2687
    S_SORCBALL3_1,                   // 2688
    S_SORCBALL3_2,                   // 2689
    S_SORCBALL3_3,                   // 2690
    S_SORCBALL3_4,                   // 2691
    S_SORCBALL3_5,                   // 2692
    S_SORCBALL3_6,                   // 2693
    S_SORCBALL3_7,                   // 2694
    S_SORCBALL3_8,                   // 2695
    S_SORCBALL3_9,                   // 2696
    S_SORCBALL3_0,                   // 2697
    S_SORCBALL3_A,                   // 2698
    S_SORCBALL3_B,                   // 2699
    S_SORCBALL3_C,                   // 2700
    S_SORCBALL3_D,                   // 2701
    S_SORCBALL3_E,                   // 2702
    S_SORCBALL3_F,                   // 2703
    S_SORCBALL3_D1,                   // 2704
    S_SORCBALL3_D2,                   // 2705
    S_SORCBALL3_D5,                   // 2706
    S_SORCBALL3_D6,                   // 2707
    S_SORCBALL3_D7,                   // 2708
    S_SORCBALL3_D8,                   // 2709
    S_SORCBALL3_D9,                   // 2710
    S_SORCFX1_1,                   // 2711
    S_SORCFX1_2,                   // 2712
    S_SORCFX1_3,                   // 2713
    S_SORCFX1_4,                   // 2714
    S_SORCFX1_D1,                   // 2715
    S_SORCFX1_D2,                   // 2716
    S_SORCFX1_D3,                   // 2717
    S_SORCFX2_SPLIT1,               // 2718
    S_SORCFX2_ORBIT1,               // 2719
    S_SORCFX2_ORBIT2,               // 2720
    S_SORCFX2_ORBIT3,               // 2721
    S_SORCFX2_ORBIT4,               // 2722
    S_SORCFX2_ORBIT5,               // 2723
    S_SORCFX2_ORBIT6,               // 2724
    S_SORCFX2_ORBIT7,               // 2725
    S_SORCFX2_ORBIT8,               // 2726
    S_SORCFX2_ORBIT9,               // 2727
    S_SORCFX2_ORBIT0,               // 2728
    S_SORCFX2_ORBITA,               // 2729
    S_SORCFX2_ORBITB,               // 2730
    S_SORCFX2_ORBITC,               // 2731
    S_SORCFX2_ORBITD,               // 2732
    S_SORCFX2_ORBITE,               // 2733
    S_SORCFX2_ORBITF,               // 2734
    S_SORCFX2T1,                   // 2735
    S_SORCFX3_1,                   // 2736
    S_SORCFX3_2,                   // 2737
    S_SORCFX3_3,                   // 2738
    S_BISHMORPH1,                   // 2739
    S_BISHMORPHA,                   // 2740
    S_BISHMORPHB,                   // 2741
    S_BISHMORPHC,                   // 2742
    S_BISHMORPHD,                   // 2743
    S_BISHMORPHE,                   // 2744
    S_BISHMORPHF,                   // 2745
    S_BISHMORPHG,                   // 2746
    S_BISHMORPHH,                   // 2747
    S_BISHMORPHI,                   // 2748
    S_BISHMORPHJ,                   // 2749
    S_SORCFX3_EXP1,                   // 2750
    S_SORCFX3_EXP2,                   // 2751
    S_SORCFX3_EXP3,                   // 2752
    S_SORCFX3_EXP4,                   // 2753
    S_SORCFX3_EXP5,                   // 2754
    S_SORCFX4_1,                   // 2755
    S_SORCFX4_2,                   // 2756
    S_SORCFX4_3,                   // 2757
    S_SORCFX4_D1,                   // 2758
    S_SORCFX4_D2,                   // 2759
    S_SORCFX4_D3,                   // 2760
    S_SORCFX4_D4,                   // 2761
    S_SORCFX4_D5,                   // 2762
    S_SORCSPARK1,                   // 2763
    S_SORCSPARK2,                   // 2764
    S_SORCSPARK3,                   // 2765
    S_SORCSPARK4,                   // 2766
    S_SORCSPARK5,                   // 2767
    S_SORCSPARK6,                   // 2768
    S_SORCSPARK7,                   // 2769
    S_BLASTEFFECT1,                   // 2770
    S_BLASTEFFECT2,                   // 2771
    S_BLASTEFFECT3,                   // 2772
    S_BLASTEFFECT4,                   // 2773
    S_BLASTEFFECT5,                   // 2774
    S_BLASTEFFECT6,                   // 2775
    S_BLASTEFFECT7,                   // 2776
    S_BLASTEFFECT8,                   // 2777
    S_BLASTEFFECT9,                   // 2778
    S_WATERDRIP1,                   // 2779
    S_KORAX_LOOK1,                   // 2780
    S_KORAX_CHASE1,                   // 2781
    S_KORAX_CHASE2,                   // 2782
    S_KORAX_CHASE3,                   // 2783
    S_KORAX_CHASE4,                   // 2784
    S_KORAX_CHASE5,                   // 2785
    S_KORAX_CHASE6,                   // 2786
    S_KORAX_CHASE7,                   // 2787
    S_KORAX_CHASE8,                   // 2788
    S_KORAX_CHASE9,                   // 2789
    S_KORAX_CHASE0,                   // 2790
    S_KORAX_CHASEA,                   // 2791
    S_KORAX_CHASEB,                   // 2792
    S_KORAX_CHASEC,                   // 2793
    S_KORAX_CHASED,                   // 2794
    S_KORAX_CHASEE,                   // 2795
    S_KORAX_CHASEF,                   // 2796
    S_KORAX_PAIN1,                   // 2797
    S_KORAX_PAIN2,                   // 2798
    S_KORAX_ATTACK1,               // 2799
    S_KORAX_ATTACK2,               // 2800
    S_KORAX_MISSILE1,               // 2801
    S_KORAX_MISSILE2,               // 2802
    S_KORAX_MISSILE3,               // 2803
    S_KORAX_COMMAND1,               // 2804
    S_KORAX_COMMAND2,               // 2805
    S_KORAX_COMMAND3,               // 2806
    S_KORAX_COMMAND4,               // 2807
    S_KORAX_COMMAND5,               // 2808
    S_KORAX_DEATH1,                   // 2809
    S_KORAX_DEATH2,                   // 2810
    S_KORAX_DEATH3,                   // 2811
    S_KORAX_DEATH4,                   // 2812
    S_KORAX_DEATH5,                   // 2813
    S_KORAX_DEATH6,                   // 2814
    S_KORAX_DEATH7,                   // 2815
    S_KORAX_DEATH8,                   // 2816
    S_KORAX_DEATH9,                   // 2817
    S_KORAX_DEATH0,                   // 2818
    S_KORAX_DEATHA,                   // 2819
    S_KORAX_DEATHB,                   // 2820
    S_KORAX_DEATHC,                   // 2821
    S_KORAX_DEATHD,                   // 2822
    S_KSPIRIT_ROAM1,               // 2823
    S_KSPIRIT_ROAM2,               // 2824
    S_KSPIRIT_DEATH1,               // 2825
    S_KSPIRIT_DEATH2,               // 2826
    S_KSPIRIT_DEATH3,               // 2827
    S_KSPIRIT_DEATH4,               // 2828
    S_KSPIRIT_DEATH5,               // 2829
    S_KSPIRIT_DEATH6,               // 2830
    S_KBOLT1,                       // 2831
    S_KBOLT2,                       // 2832
    S_KBOLT3,                       // 2833
    S_KBOLT4,                       // 2834
    S_KBOLT5,                       // 2835
    S_KBOLT6,                       // 2836
    S_KBOLT7,                       // 2837
    S_SPAWNBATS1,                   // 2838
    S_SPAWNBATS2,                   // 2839
    S_SPAWNBATS3,                   // 2840
    S_SPAWNBATS_OFF,               // 2841
    S_BAT1,                           // 2842
    S_BAT2,                           // 2843
    S_BAT3,                           // 2844
    S_BAT_DEATH,                   // 2845
    S_CAMERA,                       // 2846
    S_TEMPSOUNDORIGIN1,               // 2847
    NUMSTATES                       // 2848
} statenum_t;

// Map objects.
typedef enum {
    MT_NONE = -1,
    MT_FIRST = 0,
    MT_MAPSPOT = MT_FIRST,          // 000
    MT_MAPSPOTGRAVITY,               // 001
    MT_FIREBALL1,                   // 002
    MT_ARROW,                       // 003
    MT_DART,                       // 004
    MT_POISONDART,                   // 005
    MT_RIPPERBALL,                   // 006
    MT_PROJECTILE_BLADE,           // 007
    MT_ICESHARD,                   // 008
    MT_FLAME_SMALL_TEMP,           // 009
    MT_FLAME_LARGE_TEMP,           // 010
    MT_FLAME_SMALL,                   // 011
    MT_FLAME_LARGE,                   // 012
    MT_HEALINGBOTTLE,               // 013
    MT_HEALTHFLASK,                   // 014
    MT_ARTIFLY,                       // 015
    MT_ARTIINVULNERABILITY,           // 016
    MT_SUMMONMAULATOR,               // 017
    MT_SUMMON_FX,                   // 018
    MT_THRUSTFLOOR_UP,               // 019
    MT_THRUSTFLOOR_DOWN,           // 020
    MT_TELEPORTOTHER,               // 021
    MT_TELOTHER_FX1,               // 022
    MT_TELOTHER_FX2,               // 023
    MT_TELOTHER_FX3,               // 024
    MT_TELOTHER_FX4,               // 025
    MT_TELOTHER_FX5,               // 026
    MT_DIRT1,                       // 027
    MT_DIRT2,                       // 028
    MT_DIRT3,                       // 029
    MT_DIRT4,                       // 030
    MT_DIRT5,                       // 031
    MT_DIRT6,                       // 032
    MT_DIRTCLUMP,                   // 033
    MT_ROCK1,                       // 034
    MT_ROCK2,                       // 035
    MT_ROCK3,                       // 036
    MT_FOGSPAWNER,                   // 037
    MT_FOGPATCHS,                   // 038
    MT_FOGPATCHM,                   // 039
    MT_FOGPATCHL,                   // 040
    MT_QUAKE_FOCUS,                   // 041
    MT_SGSHARD1,                   // 042
    MT_SGSHARD2,                   // 043
    MT_SGSHARD3,                   // 044
    MT_SGSHARD4,                   // 045
    MT_SGSHARD5,                   // 046
    MT_SGSHARD6,                   // 047
    MT_SGSHARD7,                   // 048
    MT_SGSHARD8,                   // 049
    MT_SGSHARD9,                   // 050
    MT_SGSHARD0,                   // 051
    MT_ARTIEGG,                       // 052
    MT_EGGFX,                       // 053
    MT_ARTISUPERHEAL,               // 054
    MT_ZWINGEDSTATUENOSKULL,       // 055
    MT_ZGEMPEDESTAL,               // 056
    MT_ARTIPUZZSKULL,               // 057
    MT_ARTIPUZZGEMBIG,               // 058
    MT_ARTIPUZZGEMRED,               // 059
    MT_ARTIPUZZGEMGREEN1,           // 060
    MT_ARTIPUZZGEMGREEN2,           // 061
    MT_ARTIPUZZGEMBLUE1,           // 062
    MT_ARTIPUZZGEMBLUE2,           // 063
    MT_ARTIPUZZBOOK1,               // 064
    MT_ARTIPUZZBOOK2,               // 065
    MT_ARTIPUZZSKULL2,               // 066
    MT_ARTIPUZZFWEAPON,               // 067
    MT_ARTIPUZZCWEAPON,               // 068
    MT_ARTIPUZZMWEAPON,               // 069
    MT_ARTIPUZZGEAR,               // 070
    MT_ARTIPUZZGEAR2,               // 071
    MT_ARTIPUZZGEAR3,               // 072
    MT_ARTIPUZZGEAR4,               // 073
    MT_ARTITORCH,                   // 074
    MT_FIREBOMB,                   // 075
    MT_ARTITELEPORT,               // 076
    MT_ARTIPOISONBAG,               // 077
    MT_POISONBAG,                   // 078
    MT_POISONCLOUD,                   // 079
    MT_THROWINGBOMB,               // 080
    MT_SPEEDBOOTS,                   // 081
    MT_BOOSTMANA,                   // 082
    MT_BOOSTARMOR,                   // 083
    MT_BLASTRADIUS,                   // 084
    MT_HEALRADIUS,                   // 085
    MT_SPLASH,                       // 086
    MT_SPLASHBASE,                   // 087
    MT_LAVASPLASH,                   // 088
    MT_LAVASMOKE,                   // 089
    MT_SLUDGECHUNK,                   // 090
    MT_SLUDGESPLASH,               // 091
    MT_MISC0,                       // 092
    MT_MISC1,                       // 093
    MT_MISC2,                       // 094
    MT_MISC3,                       // 095
    MT_MISC4,                       // 096
    MT_MISC5,                       // 097
    MT_MISC6,                       // 098
    MT_MISC7,                       // 099
    MT_MISC8,                       // 100
    MT_TREEDESTRUCTIBLE,           // 101
    MT_MISC9,                       // 102
    MT_MISC10,                       // 103
    MT_MISC11,                       // 104
    MT_MISC12,                       // 105
    MT_MISC13,                       // 106
    MT_MISC14,                       // 107
    MT_MISC15,                       // 108
    MT_MISC16,                       // 109
    MT_MISC17,                       // 110
    MT_MISC18,                       // 111
    MT_MISC19,                       // 112
    MT_MISC20,                       // 113
    MT_MISC21,                       // 114
    MT_MISC22,                       // 115
    MT_MISC23,                       // 116
    MT_MISC24,                       // 117
    MT_MISC25,                       // 118
    MT_MISC26,                       // 119
    MT_MISC27,                       // 120
    MT_MISC28,                       // 121
    MT_MISC29,                       // 122
    MT_MISC30,                       // 123
    MT_MISC31,                       // 124
    MT_MISC32,                       // 125
    MT_MISC33,                       // 126
    MT_MISC34,                       // 127
    MT_MISC35,                       // 128
    MT_MISC36,                       // 129
    MT_MISC37,                       // 130
    MT_MISC38,                       // 131
    MT_MISC39,                       // 132
    MT_MISC40,                       // 133
    MT_MISC41,                       // 134
    MT_MISC42,                       // 135
    MT_MISC43,                       // 136
    MT_MISC44,                       // 137
    MT_MISC45,                       // 138
    MT_MISC46,                       // 139
    MT_MISC47,                       // 140
    MT_MISC48,                       // 141
    MT_MISC49,                       // 142
    MT_MISC50,                       // 143
    MT_MISC51,                       // 144
    MT_MISC52,                       // 145
    MT_MISC53,                       // 146
    MT_MISC54,                       // 147
    MT_MISC55,                       // 148
    MT_MISC56,                       // 149
    MT_MISC57,                       // 150
    MT_MISC58,                       // 151
    MT_MISC59,                       // 152
    MT_MISC60,                       // 153
    MT_MISC61,                       // 154
    MT_MISC62,                       // 155
    MT_MISC63,                       // 156
    MT_MISC64,                       // 157
    MT_MISC65,                       // 158
    MT_MISC66,                       // 159
    MT_MISC67,                       // 160
    MT_MISC68,                       // 161
    MT_MISC69,                       // 162
    MT_MISC70,                       // 163
    MT_MISC71,                       // 164
    MT_MISC72,                       // 165
    MT_MISC73,                       // 166
    MT_MISC74,                       // 167
    MT_MISC75,                       // 168
    MT_MISC76,                       // 169
    MT_POTTERY1,                   // 170
    MT_POTTERY2,                   // 171
    MT_POTTERY3,                   // 172
    MT_POTTERYBIT1,                   // 173
    MT_MISC77,                       // 174
    MT_ZLYNCHED_NOHEART,           // 175
    MT_MISC78,                       // 176
    MT_CORPSEBIT,                   // 177
    MT_CORPSEBLOODDRIP,               // 178
    MT_BLOODPOOL,                   // 179
    MT_MISC79,                       // 180
    MT_MISC80,                       // 181
    MT_LEAF1,                       // 182
    MT_LEAF2,                       // 183
    MT_ZTWINEDTORCH,               // 184
    MT_ZTWINEDTORCH_UNLIT,           // 185
    MT_BRIDGE,                       // 186
    MT_BRIDGEBALL,                   // 187
    MT_ZWALLTORCH,                   // 188
    MT_ZWALLTORCH_UNLIT,           // 189
    MT_ZBARREL,                       // 190
    MT_ZSHRUB1,                       // 191
    MT_ZSHRUB2,                       // 192
    MT_ZBUCKET,                       // 193
    MT_ZPOISONSHROOM,               // 194
    MT_ZFIREBULL,                   // 195
    MT_ZFIREBULL_UNLIT,               // 196
    MT_FIRETHING,                   // 197
    MT_BRASSTORCH,                   // 198
    MT_ZSUITOFARMOR,               // 199
    MT_ZARMORCHUNK,                   // 200
    MT_ZBELL,                       // 201
    MT_ZBLUE_CANDLE,               // 202
    MT_ZIRON_MAIDEN,               // 203
    MT_ZXMAS_TREE,                   // 204
    MT_ZCAULDRON,                   // 205
    MT_ZCAULDRON_UNLIT,               // 206
    MT_ZCHAINBIT32,                   // 207
    MT_ZCHAINBIT64,                   // 208
    MT_ZCHAINEND_HEART,               // 209
    MT_ZCHAINEND_HOOK1,               // 210
    MT_ZCHAINEND_HOOK2,               // 211
    MT_ZCHAINEND_SPIKE,               // 212
    MT_ZCHAINEND_SKULL,               // 213
    MT_TABLE_SHIT1,                   // 214
    MT_TABLE_SHIT2,                   // 215
    MT_TABLE_SHIT3,                   // 216
    MT_TABLE_SHIT4,                   // 217
    MT_TABLE_SHIT5,                   // 218
    MT_TABLE_SHIT6,                   // 219
    MT_TABLE_SHIT7,                   // 220
    MT_TABLE_SHIT8,                   // 221
    MT_TABLE_SHIT9,                   // 222
    MT_TABLE_SHIT10,               // 223
    MT_TFOG,                       // 224
    MT_MISC81,                       // 225
    MT_TELEPORTMAN,                   // 226
    MT_PUNCHPUFF,                   // 227
    MT_FW_AXE,                       // 228
    MT_AXEPUFF,                       // 229
    MT_AXEPUFF_GLOW,               // 230
    MT_AXEBLOOD,                   // 231
    MT_FW_HAMMER,                   // 232
    MT_HAMMER_MISSILE,               // 233
    MT_HAMMERPUFF,                   // 234
    MT_FSWORD_MISSILE,               // 235
    MT_FSWORD_FLAME,               // 236
    MT_CW_SERPSTAFF,               // 237
    MT_CSTAFF_MISSILE,               // 238
    MT_CSTAFFPUFF,                   // 239
    MT_CW_FLAME,                   // 240
    MT_CFLAMEFLOOR,                   // 241
    MT_FLAMEPUFF,                   // 242
    MT_FLAMEPUFF2,                   // 243
    MT_CIRCLEFLAME,                   // 244
    MT_CFLAME_MISSILE,               // 245
    MT_HOLY_FX,                       // 246
    MT_HOLY_TAIL,                   // 247
    MT_HOLY_PUFF,                   // 248
    MT_HOLY_MISSILE,               // 249
    MT_HOLY_MISSILE_PUFF,           // 250
    MT_MWANDPUFF,                   // 251
    MT_MWANDSMOKE,                   // 252
    MT_MWAND_MISSILE,               // 253
    MT_MW_LIGHTNING,               // 254
    MT_LIGHTNING_CEILING,           // 255
    MT_LIGHTNING_FLOOR,               // 256
    MT_LIGHTNING_ZAP,               // 257
    MT_MSTAFF_FX,                   // 258
    MT_MSTAFF_FX2,                   // 259
    MT_FW_SWORD1,                   // 260
    MT_FW_SWORD2,                   // 261
    MT_FW_SWORD3,                   // 262
    MT_CW_HOLY1,                   // 263
    MT_CW_HOLY2,                   // 264
    MT_CW_HOLY3,                   // 265
    MT_MW_STAFF1,                   // 266
    MT_MW_STAFF2,                   // 267
    MT_MW_STAFF3,                   // 268
    MT_SNOUTPUFF,                   // 269
    MT_MW_CONE,                       // 270
    MT_SHARDFX1,                   // 271
    MT_BLOOD,                       // 272
    MT_BLOODSPLATTER,               // 273
    MT_GIBS,                       // 274
    MT_PLAYER_FIGHTER,               // 275
    MT_BLOODYSKULL,                   // 276
    MT_PLAYER_SPEED,               // 277
    MT_ICECHUNK,                   // 278
    MT_PLAYER_CLERIC,               // 279
    MT_PLAYER_MAGE,                   // 280
    MT_PIGPLAYER,                   // 281
    MT_PIG,                           // 282
    MT_CENTAUR,                       // 283
    MT_CENTAURLEADER,               // 284
    MT_CENTAUR_FX,                   // 285
    MT_CENTAUR_SHIELD,               // 286
    MT_CENTAUR_SWORD,               // 287
    MT_DEMON,                       // 288
    MT_DEMONCHUNK1,                   // 289
    MT_DEMONCHUNK2,                   // 290
    MT_DEMONCHUNK3,                   // 291
    MT_DEMONCHUNK4,                   // 292
    MT_DEMONCHUNK5,                   // 293
    MT_DEMONFX1,                   // 294
    MT_DEMON2,                       // 295
    MT_DEMON2CHUNK1,               // 296
    MT_DEMON2CHUNK2,               // 297
    MT_DEMON2CHUNK3,               // 298
    MT_DEMON2CHUNK4,               // 299
    MT_DEMON2CHUNK5,               // 300
    MT_DEMON2FX1,                   // 301
    MT_WRAITHB,                       // 302
    MT_WRAITH,                       // 303
    MT_WRAITHFX1,                   // 304
    MT_WRAITHFX2,                   // 305
    MT_WRAITHFX3,                   // 306
    MT_WRAITHFX4,                   // 307
    MT_WRAITHFX5,                   // 308
    MT_MINOTAUR,                   // 309
    MT_MNTRFX1,                       // 310
    MT_MNTRFX2,                       // 311
    MT_MNTRFX3,                       // 312
    MT_MNTRSMOKE,                   // 313
    MT_MNTRSMOKEEXIT,               // 314
    MT_SERPENT,                       // 315
    MT_SERPENTLEADER,               // 316
    MT_SERPENTFX,                   // 317
    MT_SERPENT_HEAD,               // 318
    MT_SERPENT_GIB1,               // 319
    MT_SERPENT_GIB2,               // 320
    MT_SERPENT_GIB3,               // 321
    MT_BISHOP,                       // 322
    MT_BISHOP_PUFF,                   // 323
    MT_BISHOPBLUR,                   // 324
    MT_BISHOPPAINBLUR,               // 325
    MT_BISH_FX,                       // 326
    MT_DRAGON,                       // 327
    MT_DRAGON_FX,                   // 328
    MT_DRAGON_FX2,                   // 329
    MT_ARMOR_1,                       // 330
    MT_ARMOR_2,                       // 331
    MT_ARMOR_3,                       // 332
    MT_ARMOR_4,                       // 333
    MT_MANA1,                       // 334
    MT_MANA2,                       // 335
    MT_MANA3,                       // 336
    MT_KEY1,                       // 337
    MT_KEY2,                       // 338
    MT_KEY3,                       // 339
    MT_KEY4,                       // 340
    MT_KEY5,                       // 341
    MT_KEY6,                       // 342
    MT_KEY7,                       // 343
    MT_KEY8,                       // 344
    MT_KEY9,                       // 345
    MT_KEYA,                       // 346
    MT_KEYB,                       // 347
    MT_SOUNDWIND,                   // 348
    MT_SOUNDWATERFALL,               // 349
    MT_ETTIN,                       // 350
    MT_ETTIN_MACE,                   // 351
    MT_FIREDEMON,                   // 352
    MT_FIREDEMON_SPLOTCH1,           // 353
    MT_FIREDEMON_SPLOTCH2,           // 354
    MT_FIREDEMON_FX1,               // 355
    MT_FIREDEMON_FX2,               // 356
    MT_FIREDEMON_FX3,               // 357
    MT_FIREDEMON_FX4,               // 358
    MT_FIREDEMON_FX5,               // 359
    MT_FIREDEMON_FX6,               // 360
    MT_ICEGUY,                       // 361
    MT_ICEGUY_FX,                   // 362
    MT_ICEFX_PUFF,                   // 363
    MT_ICEGUY_FX2,                   // 364
    MT_ICEGUY_BIT,                   // 365
    MT_ICEGUY_WISP1,               // 366
    MT_ICEGUY_WISP2,               // 367
    MT_FIGHTER_BOSS,               // 368
    MT_CLERIC_BOSS,                   // 369
    MT_MAGE_BOSS,                   // 370
    MT_SORCBOSS,                   // 371
    MT_SORCBALL1,                   // 372
    MT_SORCBALL2,                   // 373
    MT_SORCBALL3,                   // 374
    MT_SORCFX1,                       // 375
    MT_SORCFX2,                       // 376
    MT_SORCFX2_T1,                   // 377
    MT_SORCFX3,                       // 378
    MT_SORCFX3_EXPLOSION,           // 379
    MT_SORCFX4,                       // 380
    MT_SORCSPARK1,                   // 381
    MT_BLASTEFFECT,                   // 382
    MT_WATER_DRIP,                   // 383
    MT_KORAX,                       // 384
    MT_KORAX_SPIRIT1,               // 385
    MT_KORAX_SPIRIT2,               // 386
    MT_KORAX_SPIRIT3,               // 387
    MT_KORAX_SPIRIT4,               // 388
    MT_KORAX_SPIRIT5,               // 389
    MT_KORAX_SPIRIT6,               // 390
    MT_DEMON_MASH,                   // 391
    MT_DEMON2_MASH,                   // 392
    MT_ETTIN_MASH,                   // 393
    MT_CENTAUR_MASH,               // 394
    MT_KORAX_BOLT,                   // 395
    MT_BAT_SPAWNER,                   // 396
    MT_BAT,                           // 397
    MT_CAMERA,                       // 398
    MT_TEMPSOUNDORIGIN,               // 399
    NUMMOBJTYPES                   // 400
} mobjtype_t;

// Text.
typedef enum {
    TXT_PRESSKEY,
    TXT_PRESSYN,
    TXT_TXT_PAUSED,
    TXT_QUITMSG,
    TXT_LOADNET,
    TXT_QLOADNET,
    TXT_QSAVESPOT,
    TXT_SAVEDEAD,
    TXT_QSPROMPT,
    TXT_QLPROMPT,
    TXT_NEWGAME,
    TXT_MSGOFF,
    TXT_MSGON,
    TXT_NETEND,
    TXT_ENDGAME,
    TXT_DOSY,
    TXT_TXT_GAMMA_LEVEL_OFF,
    TXT_TXT_GAMMA_LEVEL_1,
    TXT_TXT_GAMMA_LEVEL_2,
    TXT_TXT_GAMMA_LEVEL_3,
    TXT_TXT_GAMMA_LEVEL_4,
    TXT_EMPTYSTRING,
    TXT_TXT_MANA_1,
    TXT_TXT_MANA_2,
    TXT_TXT_MANA_BOTH,
    TXT_TXT_KEY_STEEL,
    TXT_TXT_KEY_CAVE,
    TXT_TXT_KEY_AXE,
    TXT_TXT_KEY_FIRE,
    TXT_TXT_KEY_EMERALD,
    TXT_TXT_KEY_DUNGEON,
    TXT_TXT_KEY_SILVER,
    TXT_TXT_KEY_RUSTED,
    TXT_TXT_KEY_HORN,
    TXT_TXT_KEY_SWAMP,
    TXT_TXT_KEY_CASTLE,
    TXT_TXT_INV_INVULNERABILITY,
    TXT_TXT_INV_HEALTH,
    TXT_TXT_INV_SUPERHEALTH,
    TXT_TXT_INV_SUMMON,
    TXT_TXT_INV_TORCH,
    TXT_TXT_INV_EGG,
    TXT_TXT_INV_FLY,
    TXT_TXT_INV_TELEPORT,
    TXT_TXT_INV_POISONBAG,
    TXT_TXT_INV_TELEPORTOTHER,
    TXT_TXT_INV_SPEED,
    TXT_TXT_INV_BOOSTMANA,
    TXT_TXT_INV_BOOSTARMOR,
    TXT_TXT_INV_BLASTRADIUS,
    TXT_TXT_INV_HEALINGRADIUS,
    TXT_TXT_INV_PUZZSKULL,
    TXT_TXT_INV_PUZZGEMBIG,
    TXT_TXT_INV_PUZZGEMRED,
    TXT_TXT_INV_PUZZGEMGREEN1,
    TXT_TXT_INV_PUZZGEMGREEN2,
    TXT_TXT_INV_PUZZGEMBLUE1,
    TXT_TXT_INV_PUZZGEMBLUE2,
    TXT_TXT_INV_PUZZBOOK1,
    TXT_TXT_INV_PUZZBOOK2,
    TXT_TXT_INV_PUZZSKULL2,
    TXT_TXT_INV_PUZZFWEAPON,
    TXT_TXT_INV_PUZZCWEAPON,
    TXT_TXT_INV_PUZZMWEAPON,
    TXT_TXT_INV_PUZZGEAR1,
    TXT_TXT_INV_PUZZGEAR2,
    TXT_TXT_INV_PUZZGEAR3,
    TXT_TXT_INV_PUZZGEAR4,
    TXT_TXT_USEPUZZLEFAILED,
    TXT_TXT_ITEMHEALTH,
    TXT_TXT_ITEMBAGOFHOLDING,
    TXT_TXT_ITEMSHIELD1,
    TXT_TXT_ITEMSHIELD2,
    TXT_TXT_ITEMSUPERMAP,
    TXT_TXT_ARMOR1,
    TXT_TXT_ARMOR2,
    TXT_TXT_ARMOR3,
    TXT_TXT_ARMOR4,
    TXT_TXT_WEAPON_F2,
    TXT_TXT_WEAPON_F3,
    TXT_TXT_WEAPON_F4,
    TXT_TXT_WEAPON_C2,
    TXT_TXT_WEAPON_C3,
    TXT_TXT_WEAPON_C4,
    TXT_TXT_WEAPON_M2,
    TXT_TXT_WEAPON_M3,
    TXT_TXT_WEAPON_M4,
    TXT_TXT_QUIETUS_PIECE,
    TXT_TXT_WRAITHVERGE_PIECE,
    TXT_TXT_BLOODSCOURGE_PIECE,
    TXT_TXT_CHEATGODON,
    TXT_TXT_CHEATGODOFF,
    TXT_TXT_CHEATNOCLIPON,
    TXT_TXT_CHEATNOCLIPOFF,
    TXT_TXT_CHEATWEAPONS,
    TXT_TXT_CHEATHEALTH,
    TXT_TXT_CHEATKEYS,
    TXT_TXT_CHEATSOUNDON,
    TXT_TXT_CHEATSOUNDOFF,
    TXT_TXT_CHEATTICKERON,
    TXT_TXT_CHEATTICKEROFF,
    TXT_TXT_CHEATINVITEMS3,
    TXT_TXT_CHEATITEMSFAIL,
    TXT_TXT_CHEATWARP,
    TXT_TXT_CHEATSCREENSHOT,
    TXT_TXT_CHEATIDDQD,
    TXT_TXT_CHEATIDKFA,
    TXT_TXT_CHEATBADINPUT,
    TXT_TXT_CHEATNOMAP,
    TXT_TXT_GAMESAVED,
    TXT_HUSTR_CHATMACRO0,
    TXT_HUSTR_CHATMACRO1,
    TXT_HUSTR_CHATMACRO2,
    TXT_HUSTR_CHATMACRO3,
    TXT_HUSTR_CHATMACRO4,
    TXT_HUSTR_CHATMACRO5,
    TXT_HUSTR_CHATMACRO6,
    TXT_HUSTR_CHATMACRO7,
    TXT_HUSTR_CHATMACRO8,
    TXT_HUSTR_CHATMACRO9,
    TXT_AMSTR_FOLLOWON,
    TXT_AMSTR_FOLLOWOFF,
    TXT_LOADMISSING,
    TXT_AMSTR_ROTATEON,
    TXT_AMSTR_ROTATEOFF,
    TXT_AMSTR_GRIDON,
    TXT_AMSTR_GRIDOFF,
    TXT_AMSTR_MARKEDSPOT,
    TXT_AMSTR_MARKSCLEARED,
    TXT_SAVEOUTMAP,
    TXT_ENDNOGAME,
    TXT_SUICIDEOUTMAP,
    TXT_SUICIDEASK,
    TXT_PICKGAMETYPE,
    TXT_SINGLEPLAYER,
    TXT_MULTIPLAYER,
    TXT_NOTDESIGNEDFOR,
    TXT_GAMESETUP,
    TXT_PLAYERSETUP,
    TXT_DISCONNECT,
    TXT_RANDOMPLAYERCLASS,
    TXT_PLAYERCLASS1,
    TXT_PLAYERCLASS2,
    TXT_PLAYERCLASS3,
    TXT_PLAYERCLASS4,
    TXT_SKILLF1,
    TXT_SKILLF2,
    TXT_SKILLF3,
    TXT_SKILLF4,
    TXT_SKILLF5,
    TXT_SKILLC1,
    TXT_SKILLC2,
    TXT_SKILLC3,
    TXT_SKILLC4,
    TXT_SKILLC5,
    TXT_SKILLM1,
    TXT_SKILLM2,
    TXT_SKILLM3,
    TXT_SKILLM4,
    TXT_SKILLM5,
    TXT_DELETESAVEGAME_CONFIRM,
    TXT_REBORNLOAD_CONFIRM,
    NUMTEXT
} textenum_t;

// Sounds.
typedef enum {
    SFX_NONE,
    SFX_PLAYER_FIGHTER_NORMAL_DEATH,
    SFX_PLAYER_FIGHTER_CRAZY_DEATH,
    SFX_PLAYER_FIGHTER_EXTREME1_DEATH,
    SFX_PLAYER_FIGHTER_EXTREME2_DEATH,
    SFX_PLAYER_FIGHTER_EXTREME3_DEATH,
    SFX_PLAYER_FIGHTER_BURN_DEATH,
    SFX_PLAYER_CLERIC_NORMAL_DEATH,
    SFX_PLAYER_CLERIC_CRAZY_DEATH,
    SFX_PLAYER_CLERIC_EXTREME1_DEATH,
    SFX_PLAYER_CLERIC_EXTREME2_DEATH,
    SFX_PLAYER_CLERIC_EXTREME3_DEATH,
    SFX_PLAYER_CLERIC_BURN_DEATH,
    SFX_PLAYER_MAGE_NORMAL_DEATH,
    SFX_PLAYER_MAGE_CRAZY_DEATH,
    SFX_PLAYER_MAGE_EXTREME1_DEATH,
    SFX_PLAYER_MAGE_EXTREME2_DEATH,
    SFX_PLAYER_MAGE_EXTREME3_DEATH,
    SFX_PLAYER_MAGE_BURN_DEATH,
    SFX_PLAYER_FIGHTER_PAIN,
    SFX_PLAYER_CLERIC_PAIN,
    SFX_PLAYER_MAGE_PAIN,
    SFX_PLAYER_FIGHTER_GRUNT,
    SFX_PLAYER_CLERIC_GRUNT,
    SFX_PLAYER_MAGE_GRUNT,
    SFX_PLAYER_LAND,
    SFX_PLAYER_POISONCOUGH,
    SFX_PLAYER_FIGHTER_FALLING_SCREAM,
    SFX_PLAYER_CLERIC_FALLING_SCREAM,
    SFX_PLAYER_MAGE_FALLING_SCREAM,
    SFX_PLAYER_FALLING_SPLAT,
    SFX_PLAYER_FIGHTER_FAILED_USE,
    SFX_PLAYER_CLERIC_FAILED_USE,
    SFX_PLAYER_MAGE_FAILED_USE,
    SFX_PLATFORM_START,
    SFX_PLATFORM_STARTMETAL,
    SFX_PLATFORM_STOP,
    SFX_STONE_MOVE,
    SFX_METAL_MOVE,
    SFX_DOOR_OPEN,
    SFX_DOOR_LOCKED,
    SFX_DOOR_METAL_OPEN,
    SFX_DOOR_METAL_CLOSE,
    SFX_DOOR_LIGHT_CLOSE,
    SFX_DOOR_HEAVY_CLOSE,
    SFX_DOOR_CREAK,
    SFX_PICKUP_WEAPON,
    SFX_PICKUP_ITEM,
    SFX_PICKUP_KEY,
    SFX_PICKUP_PUZZ,
    SFX_PICKUP_PIECE,
    SFX_WEAPON_BUILD,
    SFX_INVENTORY_USE,
    SFX_INVITEM_BLAST,
    SFX_TELEPORT,
    SFX_THUNDER_CRASH,
    SFX_FIGHTER_PUNCH_MISS,
    SFX_FIGHTER_PUNCH_HITTHING,
    SFX_FIGHTER_PUNCH_HITWALL,
    SFX_FIGHTER_GRUNT,
    SFX_FIGHTER_AXE_HITTHING,
    SFX_FIGHTER_HAMMER_MISS,
    SFX_FIGHTER_HAMMER_HITTHING,
    SFX_FIGHTER_HAMMER_HITWALL,
    SFX_FIGHTER_HAMMER_CONTINUOUS,
    SFX_FIGHTER_HAMMER_EXPLODE,
    SFX_FIGHTER_SWORD_FIRE,
    SFX_FIGHTER_SWORD_EXPLODE,
    SFX_CLERIC_CSTAFF_FIRE,
    SFX_CLERIC_CSTAFF_EXPLODE,
    SFX_CLERIC_CSTAFF_HITTHING,
    SFX_CLERIC_FLAME_FIRE,
    SFX_CLERIC_FLAME_EXPLODE,
    SFX_CLERIC_FLAME_CIRCLE,
    SFX_MAGE_WAND_FIRE,
    SFX_MAGE_LIGHTNING_FIRE,
    SFX_MAGE_LIGHTNING_ZAP,
    SFX_MAGE_LIGHTNING_CONTINUOUS,
    SFX_MAGE_LIGHTNING_READY,
    SFX_MAGE_SHARDS_FIRE,
    SFX_MAGE_SHARDS_EXPLODE,
    SFX_MAGE_STAFF_FIRE,
    SFX_MAGE_STAFF_EXPLODE,
    SFX_SWITCH1,
    SFX_SWITCH2,
    SFX_SERPENT_SIGHT,
    SFX_SERPENT_ACTIVE,
    SFX_SERPENT_PAIN,
    SFX_SERPENT_ATTACK,
    SFX_SERPENT_MELEEHIT,
    SFX_SERPENT_DEATH,
    SFX_SERPENT_BIRTH,
    SFX_SERPENTFX_CONTINUOUS,
    SFX_SERPENTFX_HIT,
    SFX_POTTERY_EXPLODE,
    SFX_DRIP,
    SFX_CENTAUR_SIGHT,
    SFX_CENTAUR_ACTIVE,
    SFX_CENTAUR_PAIN,
    SFX_CENTAUR_ATTACK,
    SFX_CENTAUR_DEATH,
    SFX_CENTAURLEADER_ATTACK,
    SFX_CENTAUR_MISSILE_EXPLODE,
    SFX_WIND,
    SFX_BISHOP_SIGHT,
    SFX_BISHOP_ACTIVE,
    SFX_BISHOP_PAIN,
    SFX_BISHOP_ATTACK,
    SFX_BISHOP_DEATH,
    SFX_BISHOP_MISSILE_EXPLODE,
    SFX_BISHOP_BLUR,
    SFX_DEMON_SIGHT,
    SFX_DEMON_ACTIVE,
    SFX_DEMON_PAIN,
    SFX_DEMON_ATTACK,
    SFX_DEMON_MISSILE_FIRE,
    SFX_DEMON_MISSILE_EXPLODE,
    SFX_DEMON_DEATH,
    SFX_WRAITH_SIGHT,
    SFX_WRAITH_ACTIVE,
    SFX_WRAITH_PAIN,
    SFX_WRAITH_ATTACK,
    SFX_WRAITH_MISSILE_FIRE,
    SFX_WRAITH_MISSILE_EXPLODE,
    SFX_WRAITH_DEATH,
    SFX_PIG_ACTIVE1,
    SFX_PIG_ACTIVE2,
    SFX_PIG_PAIN,
    SFX_PIG_ATTACK,
    SFX_PIG_DEATH,
    SFX_MAULATOR_SIGHT,
    SFX_MAULATOR_ACTIVE,
    SFX_MAULATOR_PAIN,
    SFX_MAULATOR_HAMMER_SWING,
    SFX_MAULATOR_HAMMER_HIT,
    SFX_MAULATOR_MISSILE_HIT,
    SFX_MAULATOR_DEATH,
    SFX_FREEZE_DEATH,
    SFX_FREEZE_SHATTER,
    SFX_ETTIN_SIGHT,
    SFX_ETTIN_ACTIVE,
    SFX_ETTIN_PAIN,
    SFX_ETTIN_ATTACK,
    SFX_ETTIN_DEATH,
    SFX_FIRED_SPAWN,
    SFX_FIRED_ACTIVE,
    SFX_FIRED_PAIN,
    SFX_FIRED_ATTACK,
    SFX_FIRED_MISSILE_HIT,
    SFX_FIRED_DEATH,
    SFX_ICEGUY_SIGHT,
    SFX_ICEGUY_ACTIVE,
    SFX_ICEGUY_ATTACK,
    SFX_ICEGUY_FX_EXPLODE,
    SFX_SORCERER_SIGHT,
    SFX_SORCERER_ACTIVE,
    SFX_SORCERER_PAIN,
    SFX_SORCERER_SPELLCAST,
    SFX_SORCERER_BALLWOOSH,
    SFX_SORCERER_DEATHSCREAM,
    SFX_SORCERER_BISHOPSPAWN,
    SFX_SORCERER_BALLPOP,
    SFX_SORCERER_BALLBOUNCE,
    SFX_SORCERER_BALLEXPLODE,
    SFX_SORCERER_BIGBALLEXPLODE,
    SFX_SORCERER_HEADSCREAM,
    SFX_DRAGON_SIGHT,
    SFX_DRAGON_ACTIVE,
    SFX_DRAGON_WINGFLAP,
    SFX_DRAGON_ATTACK,
    SFX_DRAGON_PAIN,
    SFX_DRAGON_DEATH,
    SFX_DRAGON_FIREBALL_EXPLODE,
    SFX_KORAX_SIGHT,
    SFX_KORAX_ACTIVE,
    SFX_KORAX_PAIN,
    SFX_KORAX_ATTACK,
    SFX_KORAX_COMMAND,
    SFX_KORAX_DEATH,
    SFX_KORAX_STEP,
    SFX_THRUSTSPIKE_RAISE,
    SFX_THRUSTSPIKE_LOWER,
    SFX_STAINEDGLASS_SHATTER,
    SFX_FLECHETTE_BOUNCE,
    SFX_FLECHETTE_EXPLODE,
    SFX_LAVA_MOVE,
    SFX_WATER_MOVE,
    SFX_ICE_STARTMOVE,
    SFX_EARTH_STARTMOVE,
    SFX_WATER_SPLASH,
    SFX_LAVA_SIZZLE,
    SFX_SLUDGE_GLOOP,
    SFX_CHOLY_FIRE,
    SFX_SPIRIT_ACTIVE,
    SFX_SPIRIT_ATTACK,
    SFX_SPIRIT_DIE,
    SFX_VALVE_TURN,
    SFX_ROPE_PULL,
    SFX_FLY_BUZZ,
    SFX_IGNITE,
    SFX_PUZZLE_SUCCESS,
    SFX_PUZZLE_FAIL_FIGHTER,
    SFX_PUZZLE_FAIL_CLERIC,
    SFX_PUZZLE_FAIL_MAGE,
    SFX_EARTHQUAKE,
    SFX_BELLRING,
    SFX_TREE_BREAK,
    SFX_TREE_EXPLODE,
    SFX_SUITOFARMOR_BREAK,
    SFX_POISONSHROOM_PAIN,
    SFX_POISONSHROOM_DEATH,
    SFX_AMBIENT1,
    SFX_AMBIENT2,
    SFX_AMBIENT3,
    SFX_AMBIENT4,
    SFX_AMBIENT5,
    SFX_AMBIENT6,
    SFX_AMBIENT7,
    SFX_AMBIENT8,
    SFX_AMBIENT9,
    SFX_AMBIENT10,
    SFX_AMBIENT11,
    SFX_AMBIENT12,
    SFX_AMBIENT13,
    SFX_AMBIENT14,
    SFX_AMBIENT15,
    SFX_STARTUP_TICK,
    SFX_SWITCH_OTHERLEVEL,
    SFX_RESPAWN,
    SFX_KORAX_VOICE_1,
    SFX_KORAX_VOICE_2,
    SFX_KORAX_VOICE_3,
    SFX_KORAX_VOICE_4,
    SFX_KORAX_VOICE_5,
    SFX_KORAX_VOICE_6,
    SFX_KORAX_VOICE_7,
    SFX_KORAX_VOICE_8,
    SFX_KORAX_VOICE_9,
    SFX_BAT_SCREAM,
    SFX_CHAT,
    SFX_MENU_MOVE,
    SFX_CLOCK_TICK,
    SFX_FIREBALL,
    SFX_PUPPYBEAT,
    SFX_MYSTICINCANT,
    NUMSFX
} sfxenum_t;

// Music.
typedef enum {
    NUMMUSIC
} musicenum_t;

#endif
