/* DE1: $Id: sndidx.h 3305 2006-06-11 17:00:36Z skyjake $
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * Sfx and music identifiers
 * Generated with DED Manager 1.0
 */

#ifndef __AUDIO_CONSTANTS_H__
#define __AUDIO_CONSTANTS_H__

// Sounds.
typedef enum {
    sfx_None,                      // 000
    sfx_pistol,                    // 001
    sfx_shotgn,                    // 002
    sfx_sgcock,                    // 003
    sfx_dshtgn,                    // 004
    sfx_dbopn,                     // 005
    sfx_dbcls,                     // 006
    sfx_dbload,                    // 007
    sfx_plasma,                    // 008
    sfx_bfg,                       // 009
    sfx_sawup,                     // 010
    sfx_sawidl,                    // 011
    sfx_sawful,                    // 012
    sfx_sawhit,                    // 013
    sfx_rlaunc,                    // 014
    sfx_rxplod,                    // 015
    sfx_firsht,                    // 016
    sfx_firxpl,                    // 017
    sfx_pstart,                    // 018
    sfx_pstop,                     // 019
    sfx_doropn,                    // 020
    sfx_dorcls,                    // 021
    sfx_stnmov,                    // 022
    sfx_swtchn,                    // 023
    sfx_swtchx,                    // 024
    sfx_plpain,                    // 025
    sfx_dmpain,                    // 026
    sfx_popain,                    // 027
    sfx_vipain,                    // 028
    sfx_mnpain,                    // 029
    sfx_pepain,                    // 030
    sfx_slop,                      // 031
    sfx_itemup,                    // 032
    sfx_wpnup,                     // 033
    sfx_oof,                       // 034
    sfx_telept,                    // 035
    sfx_posit1,                    // 036
    sfx_posit2,                    // 037
    sfx_posit3,                    // 038
    sfx_bgsit1,                    // 039
    sfx_bgsit2,                    // 040
    sfx_sgtsit,                    // 041
    sfx_cacsit,                    // 042
    sfx_brssit,                    // 043
    sfx_cybsit,                    // 044
    sfx_spisit,                    // 045
    sfx_bspsit,                    // 046
    sfx_kntsit,                    // 047
    sfx_vilsit,                    // 048
    sfx_mansit,                    // 049
    sfx_pesit,                     // 050
    sfx_sklatk,                    // 051
    sfx_sgtatk,                    // 052
    sfx_skepch,                    // 053
    sfx_vilatk,                    // 054
    sfx_claw,                      // 055
    sfx_skeswg,                    // 056
    sfx_pldeth,                    // 057
    sfx_pdiehi,                    // 058
    sfx_podth1,                    // 059
    sfx_podth2,                    // 060
    sfx_podth3,                    // 061
    sfx_bgdth1,                    // 062
    sfx_bgdth2,                    // 063
    sfx_sgtdth,                    // 064
    sfx_cacdth,                    // 065
    sfx_skldth,                    // 066
    sfx_brsdth,                    // 067
    sfx_cybdth,                    // 068
    sfx_spidth,                    // 069
    sfx_bspdth,                    // 070
    sfx_vildth,                    // 071
    sfx_kntdth,                    // 072
    sfx_pedth,                     // 073
    sfx_skedth,                    // 074
    sfx_posact,                    // 075
    sfx_bgact,                     // 076
    sfx_dmact,                     // 077
    sfx_bspact,                    // 078
    sfx_bspwlk,                    // 079
    sfx_vilact,                    // 080
    sfx_noway,                     // 081
    sfx_barexp,                    // 082
    sfx_punch,                     // 083
    sfx_hoof,                      // 084
    sfx_metal,                     // 085
    sfx_chgun,                     // 086
    sfx_tink,                      // 087
    sfx_bdopn,                     // 088
    sfx_bdcls,                     // 089
    sfx_itmbk,                     // 090
    sfx_flame,                     // 091
    sfx_flamst,                    // 092
    sfx_getpow,                    // 093
    sfx_bospit,                    // 094
    sfx_boscub,                    // 095
    sfx_bossit,                    // 096
    sfx_bospn,                     // 097
    sfx_bosdth,                    // 098
    sfx_manatk,                    // 099
    sfx_mandth,                    // 100
    sfx_sssit,                     // 101
    sfx_ssdth,                     // 102
    sfx_keenpn,                    // 103
    sfx_keendt,                    // 104
    sfx_skeact,                    // 105
    sfx_skesit,                    // 106
    sfx_skeatk,                    // 107
    sfx_radio,                     // 108
    sfx_wsplash,                   // 109
    sfx_nsplash,                   // 110
    sfx_blurb,                     // 111
    sfx_menubc,                    // 112
    sfx_menumv,                    // 113
    sfx_menusl,                    // 114
    sfx_intcnt,                    // 115
    sfx_intcmp,                    // 116
    sfx_intyea,                    // 116
    sfx_hudms1,                    // 116
    sfx_hudms2,                    // 116
    sfx_scrnrs,                    // 116
    sfx_wlfdro,                    // 113
    sfx_wlfdrc,                    // 114
    sfx_wlfpwl,
    sfx_smpdro,                    // 115
    sfx_smpdrc,                    // 116
    sfx_smppwl,
    sfx_dorlck,
    sfx_pltstr,
    sfx_pltmov,
    sfx_pltstp,
    sfx_blockd,
    sfx_switch,
    sfx_wfeswi,                    // 267
    sfx_mpeswi,                    // 268
    sfx_secret,
    sfx_tele,
    sfx_nmrrsp,
    sfx_methit,                    // 269
    sfx_plypai,
    sfx_plydth,
    sfx_gibbed,
    sfx_knfatk,                    // 117
    sfx_cknfat,                    // 117
    sfx_wpisto,                    // 118
    sfx_lpisto,                    // 119
    sfx_upisto,
    sfx_cpisto,
    sfx_3pisto,
    sfx_wmachi,                    // 120
    sfx_lmachi,                    // 121
    sfx_omachi,                    // 122
    sfx_umachi,                    // 122
    sfx_3machi,
    sfx_wchgun,                    // 123
    sfx_lchgun,
    sfx_uchgun,
    sfx_3chgun,                    // 123
    sfx_orifle,                    // 124
    sfx_orevol,                    // 125
    sfx_wrocke,                    // 126
    sfx_wexplo,                    // 126
    sfx_crcsht,
    sfx_crcexp,
    sfx_flasht,                    // 264
    sfx_flaxpl,                    // 126
    sfx_cflsht,                    // 126
    sfx_cflexp,                    // 126
    sfx_3flsht,                    // 126
    sfx_cmfire,                    // 126
    sfx_wmload,                    // 127
    sfx_helth1,                    // 128
    sfx_helth2,                    // 129
    sfx_helth3,                    // 129
    sfx_getmeg,                    // 130
    sfx_ammocl,                    // 131
    sfx_ammobx,                    // 132
    sfx_keyup,                     // 133
    sfx_ckeyup,                    // 134
    sfx_getmac,                    // 135
    sfx_getgat,                    // 136
    sfx_spelup,                    // 137
    sfx_lgetgt,                    // 136
    sfx_treas1,                    // 138
    sfx_treas2,                    // 139
    sfx_treas3,                    // 140
    sfx_atreas,                    // 141
    sfx_bgyfir,                    // 142
    sfx_bosfir,                    // 142
    sfx_ssgfir,                    // 143
    sfx_bgyfbs,
    sfx_bgyfbe,
    sfx_crefbs,
    sfx_crefbe,
    sfx_lssfir,                    // 144
    sfx_szofir,                    // 144
    sfx_bgypai,                    // 146
    sfx_metwlk,                    // 147
    sfx_trmovs,                    // 148
    sfx_grdsit,                    // 149
    sfx_gddth1,                    // 150
    sfx_gddth2,                    // 151
    sfx_gddth3,                    // 152
    sfx_gddth4,                    // 153
    sfx_gddth5,                    // 154
    sfx_gddth6,                    // 155
    sfx_gddth7,                    // 156
    sfx_gddth8,                    // 156
    sfx_ssgsit,                    // 157
    sfx_ssgdth,                    // 158
    sfx_dogsit,                    // 159
    sfx_dogatk,                    // 160
    sfx_dogdth,                    // 162
    sfx_mutdth,                    // 163
    sfx_ofisit,                    // 165
    sfx_ofidth,                    // 166
    sfx_hansit,                    // 169
    sfx_handth,                    // 170
    sfx_schsit,                    // 171
    sfx_schdth,                    // 172
    sfx_syrthw,                    // 145
    sfx_syrexp,
    sfx_hgtsit,                    // 173
    sfx_hgtdth,                    // 174
    sfx_hgblur,                    // 175
    sfx_mhtsit,                    // 176
    sfx_mhtdth,                    // 177
    sfx_hitsit,                    // 178
    sfx_hitdth,                    // 179
    sfx_hitslp,
    sfx_ottsit,                    // 180
    sfx_ottdth,                    // 181
    sfx_gresit,                    // 182
    sfx_gredth,                    // 183
    sfx_gensit,                    // 184
    sfx_gendth,                    // 185
    sfx_pacman,                    // 186
    sfx_trasit,                    // 187
    sfx_tradth,                    // 188
    sfx_wilsit,                    // 189
    sfx_wildth,                    // 190
    sfx_ubesit,                    // 191
    sfx_ubedth,                    // 191
    sfx_kghsit,                    // 192
    sfx_kghdth,                    // 193
    sfx_ghost,                     // 194
    sfx_angsit,                    // 195
    sfx_angdth,                    // 196
    sfx_angsht,                    // 265
    sfx_angxpl,                    // 266
    sfx_cmsit,                     // 167
    sfx_cmdth,                     // 168
    sfx_ratsit,                    // 197
    sfx_ratatk,                    // 198
    sfx_fgdsit,                    // 199
    sfx_fgddth,                    // 200
    sfx_elgsit,                    // 201
    sfx_elgdth,                    // 202
    sfx_lgurds,                    // 203
    sfx_lgrdd1,                    // 204
    sfx_lgrdd2,                    // 205
    sfx_lgrdd3,                    // 206
    sfx_lgrdd4,                    // 207
    sfx_lgrdd5,                    // 208
    sfx_lgrdd6,                    // 209
    sfx_lgrdd7,                    // 210
    sfx_lgrdd8,                    // 211
    sfx_lssgds,                    // 212
    sfx_lssgdd,                    // 213
    sfx_lbatd,                     // 214
    sfx_ldogs,                     // 215
    sfx_ldogd,                     // 216
    sfx_ldoga,                     // 218
    sfx_loffis,                    // 219
    sfx_loffid,                    // 220
    sfx_lwilys,                    // 221
    sfx_lwilyd,                    // 222
    sfx_lprofs,                    // 223
    sfx_lprofd,                    // 224
    sfx_laxed,                     // 225
    sfx_lrbots,                    // 226
    sfx_lrbotd,                    // 227
    sfx_lrbotm,                    // 145
    sfx_lrbotx,                    // 145
    sfx_ldevls,                    // 228
    sfx_ldevld,                    // 229
    sfx_lsprd,                     // 230
    sfx_lelgst,                    // 232
    sfx_lelgdh,                    // 233
    sfx_czombs,                    // 290
    sfx_czombd,                    // 290
    sfx_czomba,                    // 290
    sfx_czombh,                    // 290
    sfx_cskels,                    // 290
    sfx_cskeld,                    // 292
    sfx_cskela,                    // 291
    sfx_cbats,                     // 291
    sfx_cmages,                    // 293
    sfx_cmaged,                    // 294
    sfx_ctrols,                    // 293
    sfx_ctrola,                    // 294
    sfx_ctrolh,
    sfx_ctrlsr,
    sfx_ctrlsl,
    sfx_cdemns,                    // 293
    sfx_cdemna,                    // 294
    sfx_ceyed,                     // 289
    sfx_ceyesh,                    // 294
    sfx_ceyest,                    // 294
    sfx_csnemr,
    sfx_cchsto,                    // 289
    sfx_obiobs,                    // 234
    sfx_obiobd,                    // 235
    sfx_obisht,                    // 236
    sfx_orvsit,                    // 234
    sfx_orvdth,                    // 235
    sfx_omdocs,                    // 236
    sfx_omdocd,                    // 237
    sfx_omusit,                    // 238
    sfx_omudth,                    // 239
    sfx_omosit,                    // 240
    sfx_omodth,                    // 240
    sfx_omopai,                    // 241
    sfx_omoact,                    // 242
    sfx_omgrds,                    // 243
    sfx_omgrdd,                    // 244
    sfx_odirta,                    // 245
    sfx_oagens,                    // 246
    sfx_oagend,                    // 247
    sfx_osfoxs,                    // 248
    sfx_osfoxd,                    // 249
    sfx_obalrd,                    // 250
    sfx_onemes,                    // 251
    sfx_onemed,                    // 252
    sfx_omtans,                    // 253
    sfx_omtand,                    // 254
    sfx_sdmgys,                    // 255
    sfx_sczoms,                    // 255
    sfx_sczomd,                    // 255
    sfx_smuntd,
    sfx_sspsit,                    // 256
    sfx_sspatk,                    // 257
    sfx_sspsur,                    // 258
    sfx_sspdth,                    // 259
    sfx_sptfir,
    sfx_sptxpl,                    // 266
    sfx_sarmrs,                    // 260
    sfx_stanks,                    // 263
    sfx_stankd,                    // 264
    sfx_sphant,                    // 262
    sfx_sdmscs,                    // 265
    sfx_sdmscd,                    // 266
    sfx_hlgrds,
    sfx_hlgrdd,
    sfx_hlgrdp,
    sfx_ugrbls,                    // 255
    sfx_ugrbd1,                    // 255
    sfx_ugrbd2,                    // 255
    sfx_ugrbla,                    // 255
    sfx_urdbls,                    // 255
    sfx_urdbld,                    // 255
    sfx_urdbla,                    // 255
    sfx_uprbls,                    // 255
    sfx_uprbld,                    // 255
    sfx_umtbls,                    // 255
    sfx_umtbld,                    // 255
    sfx_aligh,                     // 271
    sfx_aligh2,                    // 272
    sfx_alorry,                    // 273
    sfx_adrop,                     // 274
    sfx_adrop2,                    // 275
    sfx_awind,                     // 276
    sfx_awolf,                     // 277
    sfx_acrick,                    // 278
    sfx_abird1,                    // 279
    sfx_abird2,                    // 280
    sfx_acomp1,                    // 281
    sfx_acomp2,                    // 282
    sfx_adoorc,                    // 283
    sfx_arock,                     // 284
    sfx_arock2,                    // 285
    sfx_afrogs,                    // 286
    sfx_ascree,                    // 287
    sfx_awater,                    // 288
    sfx_awatr2,
    sfx_arain,
    sfx_aowl,                      // 289
    NUMSFX                         // 295
} sfxenum_t;

// Music.
typedef enum {
    mus_None,                      // 000
    mus_e1m1,                      // 001
    mus_e1m2,                      // 002
    mus_e1m3,                      // 003
    mus_e1m4,                      // 004
    mus_e1m5,                      // 005
    mus_e1m6,                      // 006
    mus_e1m7,                      // 007
    mus_e1m8,                      // 008
    mus_e1m9,                      // 009
    mus_e2m1,                      // 010
    mus_e2m2,                      // 011
    mus_e2m3,                      // 012
    mus_e2m4,                      // 013
    mus_e2m5,                      // 014
    mus_e2m6,                      // 015
    mus_e2m7,                      // 016
    mus_e2m8,                      // 017
    mus_e2m9,                      // 018
    mus_e3m1,                      // 019
    mus_e3m2,                      // 020
    mus_e3m3,                      // 021
    mus_e3m4,                      // 022
    mus_e3m5,                      // 023
    mus_e3m6,                      // 024
    mus_e3m7,                      // 025
    mus_e3m8,                      // 026
    mus_e3m9,                      // 027
    mus_inter,                     // 028
    mus_intro,                     // 029
    mus_bunny,                     // 030
    mus_victor,                    // 031
    mus_introa,                    // 032
    mus_map01,                     // 033
    mus_map02,                     // 034
    mus_map03,                     // 035
    mus_map04,                     // 036
    mus_map05,                     // 037
    mus_map06,                     // 038
    mus_map07,                     // 039
    mus_map08,                     // 040
    mus_map09,                     // 041
    mus_map10,                     // 042
    mus_map11,                     // 043
    mus_map12,                     // 044
    mus_map13,                     // 045
    mus_map14,                     // 046
    mus_map15,                     // 047
    mus_map16,                     // 048
    mus_map17,                     // 049
    mus_map18,                     // 050
    mus_map19,                     // 051
    mus_map20,                     // 052
    mus_map21,                     // 053
    mus_map22,                     // 054
    mus_map23,                     // 055
    mus_map24,                     // 056
    mus_map25,                     // 057
    mus_map26,                     // 058
    mus_map27,                     // 059
    mus_map28,                     // 060
    mus_map29,                     // 061
    mus_map30,                     // 062
    mus_map31,                     // 063
    mus_map32,                     // 064
    mus_map33,                     // 065
    mus_map34,                     // 066
    mus_map35,                     // 067
    mus_map36,                     // 068
    mus_map37,                     // 069
    mus_map38,                     // 070
    mus_map39,                     // 071
    mus_map40,                     // 072
    mus_map41,                     // 073
    mus_map42,                     // 074
    mus_map43,                     // 075
    mus_map44,                     // 076
    mus_map45,                     // 077
    mus_map46,                     // 078
    mus_map47,                     // 079
    mus_map48,                     // 080
    mus_map49,                     // 081
    mus_map50,                     // 082
    mus_map51,                     // 083
    mus_map52,                     // 084
    mus_map53,                     // 085
    mus_map54,                     // 086
    mus_map55,                     // 087
    mus_map56,                     // 088
    mus_map57,                     // 089
    mus_map58,                     // 090
    mus_map59,                     // 091
    mus_map60,                     // 092
    mus_map61,                     // 093
    mus_map62,                     // 094
    mus_map63,                     // 095
    mus_map64,                     // 096
    mus_map65,                     // 097
    mus_map66,                     // 098
    mus_map67,                     // 099
    mus_map68,                     // 100
    mus_map69,                     // 101
    mus_map70,                     // 102
    mus_map71,                     // 103
    mus_map72,                     // 104
    mus_map73,                     // 105
    mus_map74,                     // 106
    mus_map75,                     // 107
    mus_map76,                     // 108
    mus_map77,                     // 109
    mus_map78,                     // 110
    mus_map79,                     // 111
    mus_map80,                     // 112
    mus_map81,                     // 113
    mus_map82,                     // 114
    mus_map83,                     // 115
    mus_map84,                     // 116
    mus_map85,                     // 117
    mus_map86,                     // 116
    mus_map87,                     // 117
    mus_map88,                     // 117
    mus_map89,                     // 112
    mus_map90,                     // 113
    mus_map91,                     // 114
    mus_map92,                     // 115
    mus_map93,                     // 116
    mus_map94,                     // 117
    mus_map95,                     // 116
    mus_map96,                     // 117
    mus_map97,                     // 117
    mus_map98,                     // 117
    mus_map99,                     // 117
    mus_wlfttl,                    // 118
    mus_wlfint,                    // 119
    mus_wlfspl,                    // 120
    mus_wlfend,                    // 121
    mus_ep3end,                    // 118
    mus_sodi,                      // 119
    mus_sodend,                    // 120
    mus_sodm3i,
    mus_catend,                    // 121
    mus_consint,                       // 121
    mus_boss1,                     // 118
    mus_boss2,                     // 119
    mus_boss3,                     // 120
    mus_boss4,                     // 121
    NUMMUSIC
} musicenum_t;

#endif
