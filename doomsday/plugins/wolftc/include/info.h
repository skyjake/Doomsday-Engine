/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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
 * Sprite, state, mobjtype and text identifiers
 * Generated with DED Manager 1.1
 */

#ifndef __INFO_CONSTANTS_H__
#define __INFO_CONSTANTS_H__

// Sprites.
typedef enum {
    SPR_TROO,                      // 000
    SPR_SHTG,                      // 001
    SPR_PUNG,                      // 002
    SPR_PISG,                      // 003
    SPR_PISF,                      // 004
    SPR_SHTF,                      // 005
    SPR_SHT2,                      // 006
    SPR_PLSG,                      // 012
    SPR_PLSF,                      // 013
    SPR_BFGG,                      // 014
    SPR_BFGF,                      // 015
    SPR_BAL2,                      // 019
    SPR_PLSS,                      // 020
    SPR_PLSE,                      // 021
    SPR_MISL,                      // 022
    SPR_BFS1,                      // 023
    SPR_BFE1,                      // 024
    SPR_BFE2,                      // 025
    SPR_POSS,                      // 029
    SPR_SPOS,                      // 030
    SPR_VILE,                      // 031
    SPR_FIRE,                      // 032
    SPR_SKEL,                      // 035
    SPR_FATT,                      // 037
    SPR_SARG,                      // 039
    SPR_HEAD,                      // 040
    SPR_BAL7,                      // 041
    SPR_BOSS,                      // 042
    SPR_BOS2,                      // 043
    SPR_SKUL,                      // 044
    SPR_SPID,                      // 045
    SPR_BSPI,                      // 046
    SPR_APLS,                      // 047
    SPR_APBX,                      // 048
    SPR_CYBR,                      // 049
    SPR_PAIN,                      // 050
    SPR_KEEN,                      // 052
    SPR_BBRN,                      // 053
    SPR_BOSF,                      // 054
    SPR_ARM1,                      // 055
    SPR_ARM2,                      // 056
    SPR_BAR1,                      // 057
    SPR_BEXP,                      // 058
    SPR_FCAN,                      // 059
    SPR_BON1,                      // 060
    SPR_BON2,                      // 061
    SPR_BKEY,                      // 062
    SPR_RKEY,                      // 063
    SPR_YKEY,                      // 064
    SPR_BSKU,                      // 065
    SPR_RSKU,                      // 066
    SPR_YSKU,                      // 067
    SPR_STIM,                      // 068
    SPR_MEDI,                      // 069
    SPR_SOUL,                      // 070
    SPR_PINV,                      // 071
    SPR_PSTR,                      // 072
    SPR_PINS,                      // 073
    SPR_MEGA,                      // 074
    SPR_PMAP,                      // 076
    SPR_PVIS,                      // 077
    SPR_CLIP,                      // 078
    SPR_AMMO,                      // 079
    SPR_ROCK,                      // 080
    SPR_BROK,                      // 081
    SPR_CELL,                      // 082
    SPR_CELP,                      // 083
    SPR_SHEL,                      // 084
    SPR_SBOX,                      // 085
    SPR_BFUG,                      // 087
    SPR_MGUN,                      // 088
    SPR_LAUN,                      // 090
    SPR_PLAS,                      // 091
    SPR_SHOT,                      // 092
    SPR_SGN2,                      // 093
    SPR_COLU,                      // 094
    SPR_SMT2,                      // 095
    SPR_GOR1,                      // 096
    SPR_POL2,                      // 097
    SPR_POL5,                      // 098
    SPR_POL4,                      // 099
    SPR_POL3,                      // 100
    SPR_POL1,                      // 101
    SPR_POL6,                      // 102
    SPR_GOR2,                      // 103
    SPR_GOR3,                      // 104
    SPR_GOR4,                      // 105
    SPR_GOR5,                      // 106
    SPR_SMIT,                      // 107
    SPR_COL1,                      // 108
    SPR_COL2,                      // 109
    SPR_COL3,                      // 110
    SPR_COL4,                      // 111
    SPR_CAND,                      // 112
    SPR_CBRA,                      // 113
    SPR_COL6,                      // 114
    SPR_TRE1,                      // 115
    SPR_TRE2,                      // 116
    SPR_ELEC,                      // 117
    SPR_CEYE,                      // 118
    SPR_FSKU,                      // 119
    SPR_COL5,                      // 120
    SPR_TBLU,                      // 121
    SPR_TGRN,                      // 122
    SPR_TRED,                      // 123
    SPR_SMBT,                      // 124
    SPR_SMGT,                      // 125
    SPR_SMRT,                      // 126
    SPR_HDB1,                      // 127
    SPR_HDB2,                      // 128
    SPR_HDB3,                      // 129
    SPR_HDB4,                      // 130
    SPR_HDB5,                      // 131
    SPR_HDB6,                      // 132
    SPR_POB1,                      // 133
    SPR_POB2,                      // 134
    SPR_BRS1,                      // 135
    SPR_TLMP,                      // 136
    SPR_TLP2,                      // 137
    SPR_WPLY,
    SPR_OPLY,
    SPR_HCLP,                      // 138
    SPR_HFLM,                      // 138
    SPR_HRCK,                      // 138
    SPR_HORB,                      // 138
    SPR_HHTH,                      // 138
    SPR_HAR1,                      // 138
    SPR_HAR2,                      // 138
    SPR_HWGK,                      // 138
    SPR_HWSK,                      // 138
    SPR_HSGK,                      // 138
    SPR_HSSK,                      // 138
    SPR_MGNP,                      // 138
    SPR_FLTP,                      // 138
    SPR_CGNP,                      // 138
    SPR_RLAP,                      // 138
    SPR_CWP1,                      // 138
    SPR_CWP2,                      // 138
    SPR_LFLP,                      // 138
    SPR_LCGP,                      // 138
    SPR_LRLP,                      // 138
    SPR_KNIF,
    SPR_PIST,
    SPR_MACH,
    SPR_FLAM,
    SPR_CHAI,
    SPR_RLAU,
    SPR_HAND,
    SPR_KNF2,
    SPR_PIS2,
    SPR_MAC2,
    SPR_FLA2,
    SPR_CHA2,
    SPR_RLA2,
    SPR_KNF3,
    SPR_PIS3,
    SPR_MAC3,
    SPR_KNF4,
    SPR_RIFL,
    SPR_MAC4,
    SPR_CHA3,
    SPR_REVL,
    SPR_PIS4,
    SPR_KNF5,
    SPR_PIS5,
    SPR_MAC5,
    SPR_FLA3,
    SPR_RLA3,
    SPR_KNF6,
    SPR_PIS6,
    SPR_MAC6,
    SPR_FLA4,
    SPR_CHA4,
    SPR_RLA4,
    SPR_KNF7,
    SPR_PIS7,
    SPR_MAC7,
    SPR_CHA5,
    SPR_TSYR,
    SPR_GARD,                      // 168
    SPR_SSGD,                      // 169
    SPR_DOGG,                      // 170
    SPR_MUTA,                      // 171
    SPR_OFFI,                      // 172
    SPR_HANS,                      // 173
    SPR_SCHA,                      // 174
    SPR_HFAK,                      // 175
    SPR_MECH,                      // 176
    SPR_HITL,                      // 177
    SPR_PACA,                      // 178
    SPR_PACB,                      // 179
    SPR_PACC,                      // 180
    SPR_PACD,                      // 181
    SPR_OTTO,                      // 182
    SPR_GRET,                      // 183
    SPR_FATF,                      // 184
    SPR_TRAN,                      // 185
    SPR_WILL,                      // 186
    SPR_UBER,                      // 187
    SPR_ABOS,                      // 188
    SPR_GBOS,                      // 189
    SPR_ANGE,                      // 190
    SPR_AFIR,
    SPR_HANB,                      // 191
    SPR_HANC,                      // 192
    SPR_MUT2,                      // 193
    SPR_MUT3,                      // 194
    SPR_OFF2,                      // 195
    SPR_RATT,                      // 196
    SPR_FLGD,                      // 197
    SPR_ELGD,                      // 198
    SPR_DOGF,                      // 199
    SPR_DINR,                      // 200
    SPR_FAID,                      // 201
    SPR_MEGH,                      // 202
    SPR_WCLP,                      // 203
    SPR_WABX,                      // 204
    SPR_WRKT,                      // 205
    SPR_WRBX,                      // 206
    SPR_WFUL,                      // 207
    SPR_WFUB,                      // 208
    SPR_WSKY,                      // 209
    SPR_WGKY,                      // 210
    SPR_CKEY,                      // 211
    SPR_TRS1,                      // 212
    SPR_TRS2,                      // 213
    SPR_TRS3,                      // 214
    SPR_PSPE,                      // 215
    SPR_BRM1,                      // 216
    SPR_ARDW,                      // 217
    SPR_RDLU,                      // 218
    SPR_CHND,                      // 219
    SPR_WTA1,
    SPR_PTAB,                      // 220
    SPR_KNIG,                      // 221
    SPR_BRW1,                      // 222
    SPR_FWEL,                      // 223
    SPR_EWEL,                      // 224
    SPR_GYL2,                      // 225
    SPR_BASK,                      // 226
    SPR_CNDU,                      // 227
    SPR_RDLL,                      // 228
    SPR_BNE1,                      // 229
    SPR_BNE2,                      // 230
    SPR_BNE3,                      // 231
    SPR_FERN,                      // 232
    SPR_COLW,                      // 233
    SPR_GYL1,                      // 234
    SPR_CNDL,                      // 235
    SPR_COLB,                      // 236
    SPR_GYUN,                      // 237
    SPR_CAGE,                      // 238
    SPR_SPEA,                      // 239
    SPR_VASE,                      // 240
    SPR_FLAG,                      // 241
    SPR_BED1,                      // 242
    SPR_BOIL,                      // 243
    SPR_PLAN,                      // 244
    SPR_PAN1,                      // 245
    SPR_PAN2,                      // 246
    SPR_CGT1,                      // 247
    SPR_SHAN,                      // 248
    SPR_SINK,                      // 249
    SPR_BNE4,                      // 250
    SPR_PUDW,                      // 251
    SPR_FSKE,                      // 252
    SPR_GIBH,                      // 253
    SPR_TARD,                      // 254
    SPR_LORY,                      // 255
    SPR_LRLH,                      // 256
    SPR_BCOL,                      // 257
    SPR_SKST,                      // 258
    SPR_ASAT,                      // 259
    SPR_CGSK,                      // 260
    SPR_URLH,                      // 261
    SPR_HWEL,                      // 262
    SPR_SKPL,                      // 263
    SPR_CGBL,                      // 264
    SPR_BRM2,                      // 265
    SPR_BRW2,                      // 266
    SPR_BUSH,                      // 267
    SPR_BWEL,                      // 268
    SPR_BWE2,                      // 269
    SPR_BWE3,                      // 270
    SPR_BWE4,                      // 271
    SPR_CAG2,                      // 272
    SPR_CAG3,                      // 272
    SPR_CAG4,                      // 272
    SPR_CGB2,                      // 273
    SPR_CGB3,                      // 274
    SPR_CGS2,                      // 275
    SPR_CGS3,                      // 276
    SPR_CGS4,                      // 277
    SPR_CGS5,                      // 278
    SPR_CGS6,                      // 279
    SPR_CGT2,                      // 280
    SPR_CGT3,                      // 281
    SPR_CHAN,                      // 282
    SPR_CHBL,                      // 283
    SPR_CHB2,                      // 284
    SPR_CHHK,                      // 285
    SPR_CHH2,                      // 286
    SPR_CHH3,                      // 287
    SPR_CHH4,                      // 288
    SPR_CHH5,                      // 289
    SPR_CHH6,                      // 290
    SPR_FER2,                      // 291
    SPR_FER3,                      // 292
    SPR_FER4,                      // 293
    SPR_FSK2,                      // 294
    SPR_KNI2,                      // 295
    SPR_KNI3,                      // 296
    SPR_LIGH,                      // 297
    SPR_PAN3,                      // 298
    SPR_PAN4,                      // 299
    SPR_PLA2,                      // 304
    SPR_PLA3,                      // 305
    SPR_PTA2,                      // 306
    SPR_PTA3,                      // 307
    SPR_PTA4,                      // 308
    SPR_PTA5,                      // 309
    SPR_ROCO,                      // 308
    SPR_ROC2,                      // 309
    SPR_SHA2,                      // 310
    SPR_SHA3,                      // 310
    SPR_SHA4,                      // 310
    SPR_SHA5,                      // 310
    SPR_SHA6,                      // 310
    SPR_SIN2,                      // 311
    SPR_SKP2,                      // 312
    SPR_SKP3,                      // 313
    SPR_SKP4,                      // 314
    SPR_SKP5,                      // 315
    SPR_SKP6,                      // 316
    SPR_SPE2,                      // 317
    SPR_SPE3,                      // 318
    SPR_SPE4,                      // 319
    SPR_SPE5,                      // 320
    SPR_SPE6,                      // 321
    SPR_TREA,                      // 322
    SPR_TREB,                      // 322
    SPR_TREC,                      // 322
    SPR_TREE,                      // 322
    SPR_TREF,                      // 323
    SPR_TREG,                      // 324
    SPR_VAS2,                      // 325
    SPR_VAS3,                      // 326
    SPR_WTA2,                      // 327
    SPR_WTA3,                      // 328
    SPR_WTA4,                      // 329
    SPR_WTA5,                      // 330
    SPR_WTA6,                      // 331
    SPR_WTA7,                      // 332
    SPR_WTA8,                      // 333
    SPR_WTA9,                      // 334
    SPR_WTAA,                      // 335
    SPR_WTAB,                      // 336
    SPR_WTAC,                      // 337
    SPR_WTAD,                      // 338
    SPR_WTAE,                      // 339
    SPR_WTAF,                      // 340
    SPR_WTAG,                      // 341
    SPR_WTCR,                      // 342
    SPR_WTC2,                      // 343
    SPR_WTC3,                      // 344
    SPR_WTC4,                      // 345
    SPR_WLTR,                      // 346
    SPR_WPLU,                      // 347
    SPR_LGRD,                      // 348
    SPR_LSSG,                      // 349
    SPR_LBAT,                      // 350
    SPR_LDOG,                      // 351
    SPR_LOFI,                      // 352
    SPR_LWIL,                      // 353
    SPR_LPRO,                      // 354
    SPR_LAXE,                      // 355
    SPR_LROB,                      // 356
    SPR_LGBS,                      // 357
    SPR_LGRM,                      // 358
    SPR_LDEV,                      // 359
    SPR_DFIR,
    SPR_LGM2,                      // 360
    SPR_LSPR,                      // 361
    SPR_LELG,                      // 363
    SPR_LELC,                      // 364
    SPR_LFLG,                      // 365
    SPR_LTRD,                      // 366
    SPR_LLUT,                      // 367
    SPR_LASA,                      // 368
    SPR_LEBJ,                      // 369
    SPR_LDE2,                      // 370
    SPR_SSKY,                      // 210
    SPR_SGKY,                      // 209
    SPR_LGDL,                      // 371
    SPR_LGDU,                      // 372
    SPR_LRDL,                      // 373
    SPR_LRDU,                      // 374
    SPR_LBLB,                      // 375
    SPR_LBLU,                      // 376
    SPR_LCHN,                      // 377
    SPR_LCDL,                      // 378
    SPR_LCDU,                      // 379
    SPR_LPTB,                      // 380
    SPR_LST1,                      // 381
    SPR_LSTN,                      // 382
    SPR_LPIP,                      // 383
    SPR_LELE,                      // 384
    SPR_LBRN,                      // 385
    SPR_LGRN,                      // 386
    SPR_LYEL,                      // 387
    SPR_LWEL,                      // 388
    SPR_LEWL,                      // 389
    SPR_LPTR,                      // 390
    SPR_LFER,                      // 391
    SPR_LVAS,                      // 392
    SPR_LASK,                      // 393
    SPR_LDST,                      // 394
    SPR_LHSK,                      // 395
    SPR_LECG,                      // 396
    SPR_LBCG,                      // 397
    SPR_LSCG,                      // 398
    SPR_LBLC,                      // 399
    SPR_LCON,                      // 400
    SPR_LBJS,                      // 401
    SPR_LBUB,                      // 402
    SPR_LBNE,                      // 403
    SPR_LSKE,                      // 404
    SPR_LSKU,                      // 405
    SPR_LBBP,                      // 406
    SPR_LBWP,                      // 407
    SPR_LWPL,                      // 408
    SPR_LBPU,                      // 409
    SPR_LSPL,                      // 410
    SPR_LRAT,                      // 411
    SPR_LBC2,                      // 412
    SPR_LBC3,                      // 413
    SPR_LBC4,                      // 414
    SPR_LBC5,                      // 415
    SPR_LBL2,                      // 416
    SPR_LBP2,                      // 417
    SPR_LBR2,                      // 418
    SPR_LBR3,                      // 419
    SPR_LCO2,                      // 420
    SPR_LCO3,                      // 420
    SPR_LEC2,                      // 421
    SPR_LGR2,                      // 422
    SPR_LHS2,                      // 423
    SPR_LHS3,                      // 424
    SPR_LHS4,                      // 425
    SPR_LHS5,                      // 426
    SPR_LHS6,                      // 427
    SPR_LPI2,                      // 428
    SPR_LPI3,                      // 429
    SPR_LPI4,                      // 430
    SPR_LPT2,                      // 431
    SPR_LPT3,                      // 432
    SPR_LPT4,                      // 433
    SPR_LPT5,                      // 434
    SPR_LSCR,                      // 435
    SPR_LSC2,                      // 436
    SPR_LST2,                      // 437
    SPR_LST3,                      // 438
    SPR_LST4,                      // 439
    SPR_LST5,                      // 440
    SPR_LST6,                      // 441
    SPR_LST7,                      // 442
    SPR_LST8,                      // 443
    SPR_LST9,                      // 444
    SPR_LSTA,                      // 445
    SPR_LSTB,                      // 446
    SPR_LSTC,                      // 447
    SPR_LSTD,                      // 448
    SPR_LSTE,                      // 449
    SPR_LSTF,                      // 450
    SPR_LSTG,                      // 451
    SPR_LSI1,                      // 452
    SPR_LSI2,                      // 453
    SPR_LSI3,                      // 454
    SPR_LSI4,                      // 455
    SPR_LSI5,                      // 456
    SPR_LSI6,                      // 457
    SPR_ATRS,                      // 458
    SPR_ACHN,                      // 459
    SPR_AVAS,                      // 460
    SPR_ACLM,                      // 461
    SPR_CBAT,                      // 462
    SPR_CBKD,                      // 462
    SPR_CBDM,                      // 462
    SPR_CEYB,                      // 463
    SPR_CMAG,                      // 464
    SPR_CNEM,                      // 465
    SPR_CORC,                      // 462
    SPR_CRDM,                      // 466
    SPR_CSKE,                      // 467
    SPR_CSK2,                      // 468
    SPR_CTRO,                      // 462
    SPR_CWTR,                      // 462
    SPR_CZOM,                      // 469
    SPR_CZO2,                      // 470
    SPR_HVIA,                      // 471
    SPR_ORBS,                      // 472
    SPR_ORBL,                      // 473
    SPR_CSR1,
    SPR_CSR2,
    SPR_CSR3,
    SPR_CSR4,
    SPR_CSR5,
    SPR_CSR6,
    SPR_CSR7,
    SPR_CSR8,
    SPR_CKYB,                      // 474
    SPR_CKYG,                      // 474
    SPR_CKYR,                      // 475
    SPR_CKYY,                      // 476
    SPR_CCHE,                      // 477
    SPR_CCHW,                      // 477
    SPR_CGRA,                      // 478
    SPR_CGR2,
    SPR_CGR3,
    SPR_CWEL,                      // 479
    SPR_CCLG,                      // 479
    SPR_CCUG,                      // 479
    SPR_OSSG,                      // 480
    SPR_OBAT,                      // 480
    SPR_ORAV,                      // 481
    SPR_OBIO,                      // 482
    SPR_OMDC,                      // 483
    SPR_OMUL,                      // 484
    SPR_OSPI,                      // 485
    SPR_OWAR,                      // 486
    SPR_OCHM,                      // 487
    SPR_OMUC,                      // 488
    SPR_OBRM,                      // 489
    SPR_OBR2,                      // 490
    SPR_OMAC,                      // 491
    SPR_OROC,                      // 492
    SPR_OSTC,                      // 493
    SPR_OSTF,                      // 494
    SPR_OTOR,                      // 495
    SPR_OTOU,                      // 496
    SPR_OGTA,                      // 497
    SPR_OGT2,                      // 498
    SPR_OTWE,                      // 499
    SPR_OMUD,                      // 500
    SPR_OBRE,                      // 501
    SPR_OFOX,                      // 502
    SPR_OIMP,                      // 503
    SPR_OMEH,                      // 504
    SPR_OMGD,                      // 505
    SPR_OMTA,                      // 506
    SPR_ONEM,                      // 507
    SPR_OTKO,                      // 508
    SPR_OTRE,                      // 509
    SPR_FHAD,                      // 508
    SPR_LVHG,                      // 509
    SPR_MINI,                      // 510
    SPR_OCON,                      // 511
    SPR_ODES,                      // 512
    SPR_OELE,                      // 513
    SPR_OGT3,                      // 514
    SPR_OKNI,                      // 515
    SPR_OCOL,                      // 516
    SPR_OCOB,                      // 517
    SPR_OSPL,                      // 518
    SPR_OSPR,                      // 519
    SPR_OTOL,                      // 520
    SPR_OCAN,                      // 521
    SPR_OCAU,                      // 522
    SPR_OHOS,                      // 523
    SPR_ICDL,                      // 526
    SPR_ICDU,                      // 527
    SPR_IBGL,                      // 524
    SPR_IBG2,                      // 524
    SPR_IBGU,                      // 525
    SPR_IKNI,                      // 529
    SPR_ITB1,                      // 530
    SPR_ITB2,                      // 531
    SPR_IFAN,                      // 528
    SPR_IKN2,                      // 529
    SPR_IKN3,                      // 529
    SPR_ITB3,                      // 530
    SPR_ITB4,                      // 531
    SPR_ITB5,
    SPR_SSPT,                      // 532
    SPR_SSDV,                      // 533
    SPR_SSXD,                      // 533
    SPR_SARM,                      // 534
    SPR_SPOP,                      // 535
    SPR_STAN,                      // 536
    SPR_SPHA,                      // 537
    SPR_SHGD,
    SPR_SCAN,                      // 538
    SPR_SCAU,                      // 539
    SPR_SCHN,                      // 540
    SPR_SCHU,                      // 541
    SPR_SHA7,                      // 542
    SPR_SBUC,                      // 543
    SPR_SLOG,                      // 544
    SPR_SMUS,                      // 545
    SPR_STAB,                      // 546
    SPR_STA2,                      // 547
    SPR_STRS,                      // 548
    SPR_SBAB,                      // 549
    SPR_SBA2,                      // 550
    SPR_SBA3,                      // 551
    SPR_SCLE,                      // 552
    SPR_SSTU,                      // 553
    SPR_SSPL,                      // 554
    SPR_SSP2,                      // 554
    SPR_SSP3,                      // 554
    SPR_SSP4,                      // 554
    SPR_SSP5,                      // 554
    SPR_SSP6,                      // 554
    SPR_SSKU,                      // 554
    SPR_SSK2,                      // 555
    SPR_SLO2,
    SPR_SMU2,
    SPR_STR2,
    SPR_SST2,
    SPR_UGBL,
    SPR_URBL,
    SPR_UPBL,
    SPR_UBBL,
    SPR_UBAR,
    SPR_UC1U,
    SPR_UC2U,
    SPR_UCHR,
    SPR_UCL1,
    SPR_UCL2,
    SPR_UCON,
    SPR_UIC2,
    SPR_UICC,
    SPR_UICF,
    SPR_UIF2,
    SPR_ULMP,
    SPR_ULPU,
    SPR_UPIL,
    SPR_USIN,
    SPR_USNM,
    SPR_UTL1,
    SPR_UTL2,
    SPR_UTR1,
    SPR_UTR2,
    SPR_USP1,
    SPR_USP2,
    SPR_WMIS,                      // 556
    SPR_LMIS,                      // 556
    SPR_CFIR,                      // 557
    SPR_CFMS,                      // 557
    SPR_SYRN,                      // 558
    SPR_HFAM,                      // 559
    SPR_AGM1,                      // 560
    SPR_ROBM,
    SPR_DVM1,                      // 561
    SPR_CAM1,                      // 562
    SPR_NEMM,
    SPR_BBLM,                      // 564
    SPR_MOM2,                      // 566
    SPR_BRGM,                      // 567
    SPR_DRKM,
    SPR_SSFX,                      // 565
    SPR_SCDM,
    SPR_CMIS,
    SPR_GBMS,
    SPR_RBMS,
    SPR_UPPM,
    SPR_UPMM,
    SPR_UPCM,
    SPR_MACG,                      // 568
    SPR_MACD,                      // 569
    SPR_MACS,                      // 570
    SPR_MACM,                      // 571
    SPR_MACO,                      // 572
    SPR_MACC,
    SPR_MACR,
    SPR_MACF,
    SPR_MACE,                      // 573
    SPR_MCLG,                      // 574
    SPR_MCLS,                      // 575
    SPR_MCLD,
    SPR_MCLT,
    SPR_MCLO,                      // 576
    SPR_MCLE,                      // 575
    SPR_MCLC,                      // 576
    SPR_MCOS,                      // 577
    SPR_MCOM,
    SPR_MCOO,                      // 577
    SPR_MCSZ,
    SPR_BLNK,                      // 578
    SPR_GIBM,                      // 580
    SPR_PRIS,                      // 581
    SPR_PSWA,                      // 582
    SPR_PBJH,                      // 583
    SPR_PYBJ,                      // 582
    SPR_DRIP,                      // 583
    SPR_VINE,                      // 583
    SPR_WEBB,                      // 584
    SPR_TFOG,                      // 026
    SPR_IFOG,                      // 027
SPR_CHGG,
SPR_CHGF,
SPR_MISG,
SPR_MISF,
SPR_SAWG,
SPR_BLUD,
SPR_PUFF,
SPR_BAL1,
SPR_PLAY,
SPR_FATB,
SPR_FBXP,
SPR_MANF,
SPR_CPOS,
SPR_SSWV,
SPR_SUIT,
SPR_BPAK,
SPR_CSAW,
    NUMSPRITES          // 585
} spritetype_e;

// States.
typedef enum {
    S_NULL,                        // 000
    S_LIGHTDONE,                   // 001
    S_PUNCH,                       // 002
    S_PUNCHDOWN,                   // 003
    S_PUNCHUP,                     // 004
    S_PUNCH1,                      // 005
    S_PUNCH2,                      // 006
    S_PUNCH3,                      // 007
    S_PUNCH4,                      // 008
    S_PUNCH5,                      // 009
    S_PISTOL,                      // 010
    S_PISTOLDOWN,                  // 011
    S_PISTOLUP,                    // 012
    S_PISTOL1,                     // 013
    S_PISTOL2,                     // 014
    S_PISTOL3,                     // 015
    S_PISTOL4,                     // 016
    S_PISTOLFLASH,                 // 017
    S_SGUN,                        // 018
    S_SGUNDOWN,                    // 019
    S_SGUNUP,                      // 020
    S_SGUN1,                       // 021
    S_SGUN2,                       // 022
    S_SGUN3,                       // 023
    S_SGUN4,                       // 024
    S_SGUN5,                       // 025
    S_SGUN6,                       // 026
    S_SGUN7,                       // 027
    S_SGUN8,                       // 028
    S_SGUN9,                       // 029
    S_SGUNFLASH1,                  // 030
    S_SGUNFLASH2,                  // 031
    S_DSGUN,                       // 032
    S_DSGUNDOWN,                   // 033
    S_DSGUNUP,                     // 034
    S_DSGUN1,                      // 035
    S_DSGUN2,                      // 036
    S_DSGUN3,                      // 037
    S_DSGUN4,                      // 038
    S_DSGUN5,                      // 039
    S_DSGUN6,                      // 040
    S_DSGUN7,                      // 041
    S_DSGUN8,                      // 042
    S_DSGUN9,                      // 043
    S_DSGUN10,                     // 044
    S_DSNR1,                       // 045
    S_DSNR2,                       // 046
    S_DSGUNFLASH1,                 // 047
    S_DSGUNFLASH2,                 // 048
    S_CHAIN,                       // 049
    S_CHAINDOWN,                   // 050
    S_CHAINUP,                     // 051
    S_CHAIN1,                      // 052
    S_CHAIN2,                      // 053
    S_CHAIN3,                      // 054
    S_CHAINFLASH1,                 // 055
    S_CHAINFLASH2,                 // 056
    S_MISSILE,                     // 057
    S_MISSILEDOWN,                 // 058
    S_MISSILEUP,                   // 059
    S_MISSILE1,                    // 060
    S_MISSILE2,                    // 061
    S_MISSILE3,                    // 062
    S_MISSILEFLASH1,               // 063
    S_MISSILEFLASH2,               // 064
    S_MISSILEFLASH3,               // 065
    S_MISSILEFLASH4,               // 066
    S_PLASMA,                      // 074
    S_PLASMADOWN,                  // 075
    S_PLASMAUP,                    // 076
    S_PLASMA1,                     // 077
    S_PLASMA2,                     // 078
    S_PLASMAFLASH1,                // 079
    S_PLASMAFLASH2,                // 080
    S_BFG,                         // 081
    S_BFGDOWN,                     // 082
    S_BFGUP,                       // 083
    S_BFG1,                        // 084
    S_BFG2,                        // 085
    S_BFG3,                        // 086
    S_BFG4,                        // 087
    S_BFGFLASH1,                   // 088
    S_BFGFLASH2,                   // 089
    S_TBALL1,                      // 097
    S_TBALL2,                      // 098
    S_TBALLX1,                     // 099
    S_TBALLX2,                     // 100
    S_TBALLX3,                     // 101
    S_RBALL1,                      // 102
    S_RBALL2,                      // 103
    S_RBALLX1,                     // 104
    S_RBALLX2,                     // 105
    S_RBALLX3,                     // 106
    S_PLASBALL,                    // 107
    S_PLASBALL2,                   // 108
    S_PLASEXP,                     // 109
    S_PLASEXP2,                    // 110
    S_PLASEXP3,                    // 111
    S_PLASEXP4,                    // 112
    S_PLASEXP5,                    // 113
    S_ROCKET,                      // 114
    S_BFGSHOT,                     // 115
    S_BFGSHOT2,                    // 116
    S_BFGLAND,                     // 117
    S_BFGLAND2,                    // 118
    S_BFGLAND3,                    // 119
    S_BFGLAND4,                    // 120
    S_BFGLAND5,                    // 121
    S_BFGLAND6,                    // 122
    S_BFGEXP,                      // 123
    S_BFGEXP2,                     // 124
    S_BFGEXP3,                     // 125
    S_BFGEXP4,                     // 126
    S_EXPLODE1,                    // 127
    S_EXPLODE2,                    // 128
    S_EXPLODE3,                    // 129
    S_PLAY_XDIE1,                  // 165
    S_PLAY_XDIE2,                  // 166
    S_PLAY_XDIE3,                  // 167
    S_PLAY_XDIE4,                  // 168
    S_PLAY_XDIE5,                  // 169
    S_PLAY_XDIE6,                  // 170
    S_PLAY_XDIE7,                  // 171
    S_PLAY_XDIE8,                  // 172
    S_PLAY_XDIE9,                  // 173
    S_POSS_STND,                   // 174
    S_POSS_STND2,                  // 175
    S_POSS_RUN1,                   // 176
    S_POSS_RUN2,                   // 177
    S_POSS_RUN3,                   // 178
    S_POSS_RUN4,                   // 179
    S_POSS_RUN5,                   // 180
    S_POSS_RUN6,                   // 181
    S_POSS_RUN7,                   // 182
    S_POSS_RUN8,                   // 183
    S_POSS_ATK1,                   // 184
    S_POSS_ATK2,                   // 185
    S_POSS_ATK3,                   // 186
    S_POSS_PAIN,                   // 187
    S_POSS_PAIN2,                  // 188
    S_POSS_DIE1,                   // 189
    S_POSS_DIE2,                   // 190
    S_POSS_DIE3,                   // 191
    S_POSS_DIE4,                   // 192
    S_POSS_DIE5,                   // 193
    S_POSS_XDIE1,                  // 194
    S_POSS_XDIE2,                  // 195
    S_POSS_XDIE3,                  // 196
    S_POSS_XDIE4,                  // 197
    S_POSS_XDIE5,                  // 198
    S_POSS_XDIE6,                  // 199
    S_POSS_XDIE7,                  // 200
    S_POSS_XDIE8,                  // 201
    S_POSS_XDIE9,                  // 202
    S_POSS_RAISE1,                 // 203
    S_POSS_RAISE2,                 // 204
    S_POSS_RAISE3,                 // 205
    S_POSS_RAISE4,                 // 206
    S_SPOS_STND,                   // 207
    S_SPOS_STND2,                  // 208
    S_SPOS_RUN1,                   // 209
    S_SPOS_RUN2,                   // 210
    S_SPOS_RUN3,                   // 211
    S_SPOS_RUN4,                   // 212
    S_SPOS_RUN5,                   // 213
    S_SPOS_RUN6,                   // 214
    S_SPOS_RUN7,                   // 215
    S_SPOS_RUN8,                   // 216
    S_SPOS_ATK1,                   // 217
    S_SPOS_ATK2,                   // 218
    S_SPOS_ATK3,                   // 219
    S_SPOS_PAIN,                   // 220
    S_SPOS_PAIN2,                  // 221
    S_SPOS_DIE1,                   // 222
    S_SPOS_DIE2,                   // 223
    S_SPOS_DIE3,                   // 224
    S_SPOS_DIE4,                   // 225
    S_SPOS_DIE5,                   // 226
    S_SPOS_XDIE1,                  // 227
    S_SPOS_XDIE2,                  // 228
    S_SPOS_XDIE3,                  // 229
    S_SPOS_XDIE4,                  // 230
    S_SPOS_XDIE5,                  // 231
    S_SPOS_XDIE6,                  // 232
    S_SPOS_XDIE7,                  // 233
    S_SPOS_XDIE8,                  // 234
    S_SPOS_XDIE9,                  // 235
    S_SPOS_RAISE1,                 // 236
    S_SPOS_RAISE2,                 // 237
    S_SPOS_RAISE3,                 // 238
    S_SPOS_RAISE4,                 // 239
    S_SPOS_RAISE5,                 // 240
    S_VILE_STND,                   // 241
    S_VILE_STND2,                  // 242
    S_VILE_RUN1,                   // 243
    S_VILE_RUN2,                   // 244
    S_VILE_RUN3,                   // 245
    S_VILE_RUN4,                   // 246
    S_VILE_RUN5,                   // 247
    S_VILE_RUN6,                   // 248
    S_VILE_RUN7,                   // 249
    S_VILE_RUN8,                   // 250
    S_VILE_RUN9,                   // 251
    S_VILE_RUN10,                  // 252
    S_VILE_RUN11,                  // 253
    S_VILE_RUN12,                  // 254
    S_VILE_ATK1,                   // 255
    S_VILE_ATK2,                   // 256
    S_VILE_ATK3,                   // 257
    S_VILE_ATK4,                   // 258
    S_VILE_ATK5,                   // 259
    S_VILE_ATK6,                   // 260
    S_VILE_ATK7,                   // 261
    S_VILE_ATK8,                   // 262
    S_VILE_ATK9,                   // 263
    S_VILE_ATK10,                  // 264
    S_VILE_ATK11,                  // 265
    S_VILE_HEAL1,                  // 266
    S_VILE_HEAL2,                  // 267
    S_VILE_HEAL3,                  // 268
    S_VILE_PAIN,                   // 269
    S_VILE_PAIN2,                  // 270
    S_VILE_DIE1,                   // 271
    S_VILE_DIE2,                   // 272
    S_VILE_DIE3,                   // 273
    S_VILE_DIE4,                   // 274
    S_VILE_DIE5,                   // 275
    S_VILE_DIE6,                   // 276
    S_VILE_DIE7,                   // 277
    S_VILE_DIE8,                   // 278
    S_VILE_DIE9,                   // 279
    S_VILE_DIE10,                  // 280
    S_FIRE1,                       // 281
    S_FIRE2,                       // 282
    S_FIRE3,                       // 283
    S_FIRE4,                       // 284
    S_FIRE5,                       // 285
    S_FIRE6,                       // 286
    S_FIRE7,                       // 287
    S_FIRE8,                       // 288
    S_FIRE9,                       // 289
    S_FIRE10,                      // 290
    S_FIRE11,                      // 291
    S_FIRE12,                      // 292
    S_FIRE13,                      // 293
    S_FIRE14,                      // 294
    S_FIRE15,                      // 295
    S_FIRE16,                      // 296
    S_FIRE17,                      // 297
    S_FIRE18,                      // 298
    S_FIRE19,                      // 299
    S_FIRE20,                      // 300
    S_FIRE21,                      // 301
    S_FIRE22,                      // 302
    S_FIRE23,                      // 303
    S_FIRE24,                      // 304
    S_FIRE25,                      // 305
    S_FIRE26,                      // 306
    S_FIRE27,                      // 307
    S_FIRE28,                      // 308
    S_FIRE29,                      // 309
    S_FIRE30,                      // 310
    S_SMOKE1,                      // 311
    S_SMOKE2,                      // 312
    S_SMOKE3,                      // 313
    S_SMOKE4,                      // 314
    S_SMOKE5,                      // 315
    S_TRACER,                      // 316
    S_TRACER2,                     // 317
    S_TRACEEXP1,                   // 318
    S_TRACEEXP2,                   // 319
    S_TRACEEXP3,                   // 320
    S_SKEL_STND,                   // 321
    S_SKEL_STND2,                  // 322
    S_SKEL_RUN1,                   // 323
    S_SKEL_RUN2,                   // 324
    S_SKEL_RUN3,                   // 325
    S_SKEL_RUN4,                   // 326
    S_SKEL_RUN5,                   // 327
    S_SKEL_RUN6,                   // 328
    S_SKEL_RUN7,                   // 329
    S_SKEL_RUN8,                   // 330
    S_SKEL_RUN9,                   // 331
    S_SKEL_RUN10,                  // 332
    S_SKEL_RUN11,                  // 333
    S_SKEL_RUN12,                  // 334
    S_SKEL_FIST1,                  // 335
    S_SKEL_FIST2,                  // 336
    S_SKEL_FIST3,                  // 337
    S_SKEL_FIST4,                  // 338
    S_SKEL_MISS1,                  // 339
    S_SKEL_MISS2,                  // 340
    S_SKEL_MISS3,                  // 341
    S_SKEL_MISS4,                  // 342
    S_SKEL_PAIN,                   // 343
    S_SKEL_PAIN2,                  // 344
    S_SKEL_DIE1,                   // 345
    S_SKEL_DIE2,                   // 346
    S_SKEL_DIE3,                   // 347
    S_SKEL_DIE4,                   // 348
    S_SKEL_DIE5,                   // 349
    S_SKEL_DIE6,                   // 350
    S_SKEL_RAISE1,                 // 351
    S_SKEL_RAISE2,                 // 352
    S_SKEL_RAISE3,                 // 353
    S_SKEL_RAISE4,                 // 354
    S_SKEL_RAISE5,                 // 355
    S_SKEL_RAISE6,                 // 356
    S_FATSHOT1,                    // 357
    S_FATSHOT2,                    // 358
    S_FATSHOTX1,                   // 359
    S_FATSHOTX2,                   // 360
    S_FATSHOTX3,                   // 361
    S_FATT_STND,                   // 362
    S_FATT_STND2,                  // 363
    S_FATT_RUN1,                   // 364
    S_FATT_RUN2,                   // 365
    S_FATT_RUN3,                   // 366
    S_FATT_RUN4,                   // 367
    S_FATT_RUN5,                   // 368
    S_FATT_RUN6,                   // 369
    S_FATT_RUN7,                   // 370
    S_FATT_RUN8,                   // 371
    S_FATT_RUN9,                   // 372
    S_FATT_RUN10,                  // 373
    S_FATT_RUN11,                  // 374
    S_FATT_RUN12,                  // 375
    S_FATT_ATK1,                   // 376
    S_FATT_ATK2,                   // 377
    S_FATT_ATK3,                   // 378
    S_FATT_ATK4,                   // 379
    S_FATT_ATK5,                   // 380
    S_FATT_ATK6,                   // 381
    S_FATT_ATK7,                   // 382
    S_FATT_ATK8,                   // 383
    S_FATT_ATK9,                   // 384
    S_FATT_ATK10,                  // 385
    S_FATT_PAIN,                   // 386
    S_FATT_PAIN2,                  // 387
    S_FATT_DIE1,                   // 388
    S_FATT_DIE2,                   // 389
    S_FATT_DIE3,                   // 390
    S_FATT_DIE4,                   // 391
    S_FATT_DIE5,                   // 392
    S_FATT_DIE6,                   // 393
    S_FATT_DIE7,                   // 394
    S_FATT_DIE8,                   // 395
    S_FATT_DIE9,                   // 396
    S_FATT_DIE10,                  // 397
    S_FATT_RAISE1,                 // 398
    S_FATT_RAISE2,                 // 399
    S_FATT_RAISE3,                 // 400
    S_FATT_RAISE4,                 // 401
    S_FATT_RAISE5,                 // 402
    S_FATT_RAISE6,                 // 403
    S_FATT_RAISE7,                 // 404
    S_FATT_RAISE8,                 // 405
    S_CPOS_STND,                   // 406
    S_CPOS_STND2,                  // 407
    S_CPOS_RUN1,                   // 408
    S_CPOS_RUN2,                   // 409
    S_CPOS_RUN3,                   // 410
    S_CPOS_RUN4,                   // 411
    S_CPOS_RUN5,                   // 412
    S_CPOS_RUN6,                   // 413
    S_CPOS_RUN7,                   // 414
    S_CPOS_RUN8,                   // 415
    S_CPOS_ATK1,                   // 416
    S_CPOS_ATK2,                   // 417
    S_CPOS_ATK3,                   // 418
    S_CPOS_ATK4,                   // 419
    S_CPOS_PAIN,                   // 420
    S_CPOS_PAIN2,                  // 421
    S_CPOS_DIE1,                   // 422
    S_CPOS_DIE2,                   // 423
    S_CPOS_DIE3,                   // 424
    S_CPOS_DIE4,                   // 425
    S_CPOS_DIE5,                   // 426
    S_CPOS_DIE6,                   // 427
    S_CPOS_DIE7,                   // 428
    S_CPOS_XDIE1,                  // 429
    S_CPOS_XDIE2,                  // 430
    S_CPOS_XDIE3,                  // 431
    S_CPOS_XDIE4,                  // 432
    S_CPOS_XDIE5,                  // 433
    S_CPOS_XDIE6,                  // 434
    S_CPOS_RAISE1,                 // 435
    S_CPOS_RAISE2,                 // 436
    S_CPOS_RAISE3,                 // 437
    S_CPOS_RAISE4,                 // 438
    S_CPOS_RAISE5,                 // 439
    S_CPOS_RAISE6,                 // 440
    S_CPOS_RAISE7,                 // 441
    S_TROO_STND,                   // 442
    S_TROO_STND2,                  // 443
    S_TROO_RUN1,                   // 444
    S_TROO_RUN2,                   // 445
    S_TROO_RUN3,                   // 446
    S_TROO_RUN4,                   // 447
    S_TROO_RUN5,                   // 448
    S_TROO_RUN6,                   // 449
    S_TROO_RUN7,                   // 450
    S_TROO_RUN8,                   // 451
    S_TROO_ATK1,                   // 452
    S_TROO_ATK2,                   // 453
    S_TROO_ATK3,                   // 454
    S_TROO_PAIN,                   // 455
    S_TROO_PAIN2,                  // 456
    S_TROO_DIE1,                   // 457
    S_TROO_DIE2,                   // 458
    S_TROO_DIE3,                   // 459
    S_TROO_DIE4,                   // 460
    S_TROO_DIE5,                   // 461
    S_TROO_XDIE1,                  // 462
    S_TROO_XDIE2,                  // 463
    S_TROO_XDIE3,                  // 464
    S_TROO_XDIE4,                  // 465
    S_TROO_XDIE5,                  // 466
    S_TROO_XDIE6,                  // 467
    S_TROO_XDIE7,                  // 468
    S_TROO_XDIE8,                  // 469
    S_TROO_RAISE1,                 // 470
    S_TROO_RAISE2,                 // 471
    S_TROO_RAISE3,                 // 472
    S_TROO_RAISE4,                 // 473
    S_TROO_RAISE5,                 // 474
    S_SARG_STND,                   // 475
    S_SARG_STND2,                  // 476
    S_SARG_RUN1,                   // 477
    S_SARG_RUN2,                   // 478
    S_SARG_RUN3,                   // 479
    S_SARG_RUN4,                   // 480
    S_SARG_RUN5,                   // 481
    S_SARG_RUN6,                   // 482
    S_SARG_RUN7,                   // 483
    S_SARG_RUN8,                   // 484
    S_SARG_ATK1,                   // 485
    S_SARG_ATK2,                   // 486
    S_SARG_ATK3,                   // 487
    S_SARG_PAIN,                   // 488
    S_SARG_PAIN2,                  // 489
    S_SARG_DIE1,                   // 490
    S_SARG_DIE2,                   // 491
    S_SARG_DIE3,                   // 492
    S_SARG_DIE4,                   // 493
    S_SARG_DIE5,                   // 494
    S_SARG_DIE6,                   // 495
    S_SARG_RAISE1,                 // 496
    S_SARG_RAISE2,                 // 497
    S_SARG_RAISE3,                 // 498
    S_SARG_RAISE4,                 // 499
    S_SARG_RAISE5,                 // 500
    S_SARG_RAISE6,                 // 501
    S_HEAD_STND,                   // 502
    S_HEAD_RUN1,                   // 503
    S_HEAD_ATK1,                   // 504
    S_HEAD_ATK2,                   // 505
    S_HEAD_ATK3,                   // 506
    S_HEAD_PAIN,                   // 507
    S_HEAD_PAIN2,                  // 508
    S_HEAD_PAIN3,                  // 509
    S_HEAD_DIE1,                   // 510
    S_HEAD_DIE2,                   // 511
    S_HEAD_DIE3,                   // 512
    S_HEAD_DIE4,                   // 513
    S_HEAD_DIE5,                   // 514
    S_HEAD_DIE6,                   // 515
    S_HEAD_RAISE1,                 // 516
    S_HEAD_RAISE2,                 // 517
    S_HEAD_RAISE3,                 // 518
    S_HEAD_RAISE4,                 // 519
    S_HEAD_RAISE5,                 // 520
    S_HEAD_RAISE6,                 // 521
    S_BRBALL1,                     // 522
    S_BRBALL2,                     // 523
    S_BRBALLX1,                    // 524
    S_BRBALLX2,                    // 525
    S_BRBALLX3,                    // 526
    S_BOSS_STND,                   // 527
    S_BOSS_STND2,                  // 528
    S_BOSS_RUN1,                   // 529
    S_BOSS_RUN2,                   // 530
    S_BOSS_RUN3,                   // 531
    S_BOSS_RUN4,                   // 532
    S_BOSS_RUN5,                   // 533
    S_BOSS_RUN6,                   // 534
    S_BOSS_RUN7,                   // 535
    S_BOSS_RUN8,                   // 536
    S_BOSS_ATK1,                   // 537
    S_BOSS_ATK2,                   // 538
    S_BOSS_ATK3,                   // 539
    S_BOSS_PAIN,                   // 540
    S_BOSS_PAIN2,                  // 541
    S_BOSS_DIE1,                   // 542
    S_BOSS_DIE2,                   // 543
    S_BOSS_DIE3,                   // 544
    S_BOSS_DIE4,                   // 545
    S_BOSS_DIE5,                   // 546
    S_BOSS_DIE6,                   // 547
    S_BOSS_DIE7,                   // 548
    S_BOSS_RAISE1,                 // 549
    S_BOSS_RAISE2,                 // 550
    S_BOSS_RAISE3,                 // 551
    S_BOSS_RAISE4,                 // 552
    S_BOSS_RAISE5,                 // 553
    S_BOSS_RAISE6,                 // 554
    S_BOSS_RAISE7,                 // 555
    S_BOS2_STND,                   // 556
    S_BOS2_STND2,                  // 557
    S_BOS2_RUN1,                   // 558
    S_BOS2_RUN2,                   // 559
    S_BOS2_RUN3,                   // 560
    S_BOS2_RUN4,                   // 561
    S_BOS2_RUN5,                   // 562
    S_BOS2_RUN6,                   // 563
    S_BOS2_RUN7,                   // 564
    S_BOS2_RUN8,                   // 565
    S_BOS2_ATK1,                   // 566
    S_BOS2_ATK2,                   // 567
    S_BOS2_ATK3,                   // 568
    S_BOS2_PAIN,                   // 569
    S_BOS2_PAIN2,                  // 570
    S_BOS2_DIE1,                   // 571
    S_BOS2_DIE2,                   // 572
    S_BOS2_DIE3,                   // 573
    S_BOS2_DIE4,                   // 574
    S_BOS2_DIE5,                   // 575
    S_BOS2_DIE6,                   // 576
    S_BOS2_DIE7,                   // 577
    S_BOS2_RAISE1,                 // 578
    S_BOS2_RAISE2,                 // 579
    S_BOS2_RAISE3,                 // 580
    S_BOS2_RAISE4,                 // 581
    S_BOS2_RAISE5,                 // 582
    S_BOS2_RAISE6,                 // 583
    S_BOS2_RAISE7,                 // 584
    S_SKULL_STND,                  // 585
    S_SKULL_STND2,                 // 586
    S_SKULL_RUN1,                  // 587
    S_SKULL_RUN2,                  // 588
    S_SKULL_ATK1,                  // 589
    S_SKULL_ATK2,                  // 590
    S_SKULL_ATK3,                  // 591
    S_SKULL_ATK4,                  // 592
    S_SKULL_PAIN,                  // 593
    S_SKULL_PAIN2,                 // 594
    S_SKULL_DIE1,                  // 595
    S_SKULL_DIE2,                  // 596
    S_SKULL_DIE3,                  // 597
    S_SKULL_DIE4,                  // 598
    S_SKULL_DIE5,                  // 599
    S_SKULL_DIE6,                  // 600
    S_SPID_STND,                   // 601
    S_SPID_STND2,                  // 602
    S_SPID_RUN1,                   // 603
    S_SPID_RUN2,                   // 604
    S_SPID_RUN3,                   // 605
    S_SPID_RUN4,                   // 606
    S_SPID_RUN5,                   // 607
    S_SPID_RUN6,                   // 608
    S_SPID_RUN7,                   // 609
    S_SPID_RUN8,                   // 610
    S_SPID_RUN9,                   // 611
    S_SPID_RUN10,                  // 612
    S_SPID_RUN11,                  // 613
    S_SPID_RUN12,                  // 614
    S_SPID_ATK1,                   // 615
    S_SPID_ATK2,                   // 616
    S_SPID_ATK3,                   // 617
    S_SPID_ATK4,                   // 618
    S_SPID_PAIN,                   // 619
    S_SPID_PAIN2,                  // 620
    S_SPID_DIE1,                   // 621
    S_SPID_DIE2,                   // 622
    S_SPID_DIE3,                   // 623
    S_SPID_DIE4,                   // 624
    S_SPID_DIE5,                   // 625
    S_SPID_DIE6,                   // 626
    S_SPID_DIE7,                   // 627
    S_SPID_DIE8,                   // 628
    S_SPID_DIE9,                   // 629
    S_SPID_DIE10,                  // 630
    S_SPID_DIE11,                  // 631
    S_BSPI_STND,                   // 632
    S_BSPI_STND2,                  // 633
    S_BSPI_SIGHT,                  // 634
    S_BSPI_RUN1,                   // 635
    S_BSPI_RUN2,                   // 636
    S_BSPI_RUN3,                   // 637
    S_BSPI_RUN4,                   // 638
    S_BSPI_RUN5,                   // 639
    S_BSPI_RUN6,                   // 640
    S_BSPI_RUN7,                   // 641
    S_BSPI_RUN8,                   // 642
    S_BSPI_RUN9,                   // 643
    S_BSPI_RUN10,                  // 644
    S_BSPI_RUN11,                  // 645
    S_BSPI_RUN12,                  // 646
    S_BSPI_ATK1,                   // 647
    S_BSPI_ATK2,                   // 648
    S_BSPI_ATK3,                   // 649
    S_BSPI_ATK4,                   // 650
    S_BSPI_PAIN,                   // 651
    S_BSPI_PAIN2,                  // 652
    S_BSPI_DIE1,                   // 653
    S_BSPI_DIE2,                   // 654
    S_BSPI_DIE3,                   // 655
    S_BSPI_DIE4,                   // 656
    S_BSPI_DIE5,                   // 657
    S_BSPI_DIE6,                   // 658
    S_BSPI_DIE7,                   // 659
    S_BSPI_RAISE1,                 // 660
    S_BSPI_RAISE2,                 // 661
    S_BSPI_RAISE3,                 // 662
    S_BSPI_RAISE4,                 // 663
    S_BSPI_RAISE5,                 // 664
    S_BSPI_RAISE6,                 // 665
    S_BSPI_RAISE7,                 // 666
    S_ARACH_PLAZ,                  // 667
    S_ARACH_PLAZ2,                 // 668
    S_ARACH_PLEX,                  // 669
    S_ARACH_PLEX2,                 // 670
    S_ARACH_PLEX3,                 // 671
    S_ARACH_PLEX4,                 // 672
    S_ARACH_PLEX5,                 // 673
    S_CYBER_STND,                  // 674
    S_CYBER_STND2,                 // 675
    S_CYBER_RUN1,                  // 676
    S_CYBER_RUN2,                  // 677
    S_CYBER_RUN3,                  // 678
    S_CYBER_RUN4,                  // 679
    S_CYBER_RUN5,                  // 680
    S_CYBER_RUN6,                  // 681
    S_CYBER_RUN7,                  // 682
    S_CYBER_RUN8,                  // 683
    S_CYBER_ATK1,                  // 684
    S_CYBER_ATK2,                  // 685
    S_CYBER_ATK3,                  // 686
    S_CYBER_ATK4,                  // 687
    S_CYBER_ATK5,                  // 688
    S_CYBER_ATK6,                  // 689
    S_CYBER_PAIN,                  // 690
    S_CYBER_DIE1,                  // 691
    S_CYBER_DIE2,                  // 692
    S_CYBER_DIE3,                  // 693
    S_CYBER_DIE4,                  // 694
    S_CYBER_DIE5,                  // 695
    S_CYBER_DIE6,                  // 696
    S_CYBER_DIE7,                  // 697
    S_CYBER_DIE8,                  // 698
    S_CYBER_DIE9,                  // 699
    S_CYBER_DIE10,                 // 700
    S_PAIN_STND,                   // 701
    S_PAIN_RUN1,                   // 702
    S_PAIN_RUN2,                   // 703
    S_PAIN_RUN3,                   // 704
    S_PAIN_RUN4,                   // 705
    S_PAIN_RUN5,                   // 706
    S_PAIN_RUN6,                   // 707
    S_PAIN_ATK1,                   // 708
    S_PAIN_ATK2,                   // 709
    S_PAIN_ATK3,                   // 710
    S_PAIN_ATK4,                   // 711
    S_PAIN_PAIN,                   // 712
    S_PAIN_PAIN2,                  // 713
    S_PAIN_DIE1,                   // 714
    S_PAIN_DIE2,                   // 715
    S_PAIN_DIE3,                   // 716
    S_PAIN_DIE4,                   // 717
    S_PAIN_DIE5,                   // 718
    S_PAIN_DIE6,                   // 719
    S_PAIN_RAISE1,                 // 720
    S_PAIN_RAISE2,                 // 721
    S_PAIN_RAISE3,                 // 722
    S_PAIN_RAISE4,                 // 723
    S_PAIN_RAISE5,                 // 724
    S_PAIN_RAISE6,                 // 725
    S_SSWV_STND,                   // 726
    S_SSWV_STND2,                  // 727
    S_SSWV_RUN1,                   // 728
    S_SSWV_RUN2,                   // 729
    S_SSWV_RUN3,                   // 730
    S_SSWV_RUN4,                   // 731
    S_SSWV_RUN5,                   // 732
    S_SSWV_RUN6,                   // 733
    S_SSWV_RUN7,                   // 734
    S_SSWV_RUN8,                   // 735
    S_SSWV_ATK1,                   // 736
    S_SSWV_ATK2,                   // 737
    S_SSWV_ATK3,                   // 738
    S_SSWV_ATK4,                   // 739
    S_SSWV_ATK5,                   // 740
    S_SSWV_ATK6,                   // 741
    S_SSWV_PAIN,                   // 742
    S_SSWV_PAIN2,                  // 743
    S_SSWV_DIE1,                   // 744
    S_SSWV_DIE2,                   // 745
    S_SSWV_DIE3,                   // 746
    S_SSWV_DIE4,                   // 747
    S_SSWV_DIE5,                   // 748
    S_SSWV_XDIE1,                  // 749
    S_SSWV_XDIE2,                  // 750
    S_SSWV_XDIE3,                  // 751
    S_SSWV_XDIE4,                  // 752
    S_SSWV_XDIE5,                  // 753
    S_SSWV_XDIE6,                  // 754
    S_SSWV_XDIE7,                  // 755
    S_SSWV_XDIE8,                  // 756
    S_SSWV_XDIE9,                  // 757
    S_SSWV_RAISE1,                 // 758
    S_SSWV_RAISE2,                 // 759
    S_SSWV_RAISE3,                 // 760
    S_SSWV_RAISE4,                 // 761
    S_SSWV_RAISE5,                 // 762
    S_KEENSTND,                    // 763
    S_COMMKEEN,                    // 764
    S_COMMKEEN2,                   // 765
    S_COMMKEEN3,                   // 766
    S_COMMKEEN4,                   // 767
    S_COMMKEEN5,                   // 768
    S_COMMKEEN6,                   // 769
    S_COMMKEEN7,                   // 770
    S_COMMKEEN8,                   // 771
    S_COMMKEEN9,                   // 772
    S_COMMKEEN10,                  // 773
    S_COMMKEEN11,                  // 774
    S_COMMKEEN12,                  // 775
    S_KEENPAIN,                    // 776
    S_KEENPAIN2,                   // 777
    S_BRAIN,                       // 778
    S_BRAIN_PAIN,                  // 779
    S_BRAIN_DIE1,                  // 780
    S_BRAIN_DIE2,                  // 781
    S_BRAIN_DIE3,                  // 782
    S_BRAIN_DIE4,                  // 783
    S_BRAINEYE,                    // 784
    S_BRAINEYESEE,                 // 785
    S_BRAINEYE1,                   // 786
    S_SPAWN1,                      // 787
    S_SPAWN2,                      // 788
    S_SPAWN3,                      // 789
    S_SPAWN4,                      // 790
    S_SPAWNFIRE1,                  // 791
    S_SPAWNFIRE2,                  // 792
    S_SPAWNFIRE3,                  // 793
    S_SPAWNFIRE4,                  // 794
    S_SPAWNFIRE5,                  // 795
    S_SPAWNFIRE6,                  // 796
    S_SPAWNFIRE7,                  // 797
    S_SPAWNFIRE8,                  // 798
    S_BRAINEXPLODE1,               // 799
    S_BRAINEXPLODE2,               // 800
    S_BRAINEXPLODE3,               // 801
    S_ARM1,                        // 802
    S_ARM1A,                       // 803
    S_ARM2,                        // 804
    S_ARM2A,                       // 805
    S_BAR1,                        // 806
    S_BAR2,                        // 807
    S_BEXP,                        // 808
    S_BEXP2,                       // 809
    S_BEXP3,                       // 810
    S_BEXP4,                       // 811
    S_BEXP5,                       // 812
    S_BBAR1,                       // 813
    S_BBAR2,                       // 814
    S_BBAR3,                       // 815
    S_BON1,                        // 816
    S_BON1A,                       // 817
    S_BON1B,                       // 818
    S_BON1C,                       // 819
    S_BON1D,                       // 820
    S_BON1E,                       // 821
    S_BON2,                        // 822
    S_BON2A,                       // 823
    S_BON2B,                       // 824
    S_BON2C,                       // 825
    S_BON2D,                       // 826
    S_BON2E,                       // 827
    S_BKEY,                        // 828
    S_BKEY2,                       // 829
    S_RKEY,                        // 830
    S_RKEY2,                       // 831
    S_YKEY,                        // 832
    S_YKEY2,                       // 833
    S_BSKULL,                      // 834
    S_BSKULL2,                     // 835
    S_RSKULL,                      // 836
    S_RSKULL2,                     // 837
    S_YSKULL,                      // 838
    S_YSKULL2,                     // 839
    S_STIM,                        // 840
    S_MEDI,                        // 841
    S_SOUL,                        // 842
    S_SOUL2,                       // 843
    S_SOUL3,                       // 844
    S_SOUL4,                       // 845
    S_SOUL5,                       // 846
    S_SOUL6,                       // 847
    S_PINV,                        // 848
    S_PINV2,                       // 849
    S_PINV3,                       // 850
    S_PINV4,                       // 851
    S_PSTR,                        // 852
    S_PINS,                        // 853
    S_PINS2,                       // 854
    S_PINS3,                       // 855
    S_PINS4,                       // 856
    S_MEGA,                        // 857
    S_MEGA2,                       // 858
    S_MEGA3,                       // 859
    S_MEGA4,                       // 860
    S_PMAP,                        // 862
    S_PMAP2,                       // 863
    S_PMAP3,                       // 864
    S_PMAP4,                       // 865
    S_PMAP5,                       // 866
    S_PMAP6,                       // 867
    S_PVIS,                        // 868
    S_PVIS2,                       // 869
    S_CLIP,                        // 870
    S_AMMO,                        // 871
    S_ROCK,                        // 872
    S_BROK,                        // 873
    S_CELL,                        // 874
    S_CELP,                        // 875
    S_SHEL,                        // 876
    S_SBOX,                        // 877
    S_BFUG,                        // 879
    S_MGUN,                        // 880
    S_LAUN,                        // 882
    S_PLAS,                        // 883
    S_SHOT,                        // 884
    S_SHOT2,                       // 885
    S_COLU,                        // 886
    S_STALAG,                      // 887
    S_BLOODYTWITCH,                // 888
    S_BLOODYTWITCH2,               // 889
    S_BLOODYTWITCH3,               // 890
    S_BLOODYTWITCH4,               // 891
    S_DEADTORSO,                   // 892
    S_DEADBOTTOM,                  // 893
    S_HEADSONSTICK,                // 894
    S_GIBS,                        // 895
    S_HEADONASTICK,                // 896
    S_HEADCANDLES,                 // 897
    S_HEADCANDLES2,                // 898
    S_DEADSTICK,                   // 899
    S_LIVESTICK,                   // 900
    S_LIVESTICK2,                  // 901
    S_MEAT2,                       // 902
    S_MEAT3,                       // 903
    S_MEAT4,                       // 904
    S_MEAT5,                       // 905
    S_STALAGTITE,                  // 906
    S_TALLGRNCOL,                  // 907
    S_SHRTGRNCOL,                  // 908
    S_TALLREDCOL,                  // 909
    S_SHRTREDCOL,                  // 910
    S_CANDLESTIK,                  // 911
    S_CANDELABRA,                  // 912
    S_SKULLCOL,                    // 913
    S_TORCHTREE,                   // 914
    S_BIGTREE,                     // 915
    S_TECHPILLAR,                  // 916
    S_EVILEYE,                     // 917
    S_EVILEYE2,                    // 918
    S_EVILEYE3,                    // 919
    S_EVILEYE4,                    // 920
    S_FLOATSKULL,                  // 921
    S_FLOATSKULL2,                 // 922
    S_FLOATSKULL3,                 // 923
    S_HEARTCOL,                    // 924
    S_HEARTCOL2,                   // 925
    S_BLUETORCH,                   // 926
    S_BLUETORCH2,                  // 927
    S_BLUETORCH3,                  // 928
    S_BLUETORCH4,                  // 929
    S_GREENTORCH,                  // 930
    S_GREENTORCH2,                 // 931
    S_GREENTORCH3,                 // 932
    S_GREENTORCH4,                 // 933
    S_REDTORCH,                    // 934
    S_REDTORCH2,                   // 935
    S_REDTORCH3,                   // 936
    S_REDTORCH4,                   // 937
    S_BTORCHSHRT,                  // 938
    S_BTORCHSHRT2,                 // 939
    S_BTORCHSHRT3,                 // 940
    S_BTORCHSHRT4,                 // 941
    S_GTORCHSHRT,                  // 942
    S_GTORCHSHRT2,                 // 943
    S_GTORCHSHRT3,                 // 944
    S_GTORCHSHRT4,                 // 945
    S_RTORCHSHRT,                  // 946
    S_RTORCHSHRT2,                 // 947
    S_RTORCHSHRT3,                 // 948
    S_RTORCHSHRT4,                 // 949
    S_HANGNOGUTS,                  // 950
    S_HANGBNOBRAIN,                // 951
    S_HANGTLOOKDN,                 // 952
    S_HANGTSKULL,                  // 953
    S_HANGTLOOKUP,                 // 954
    S_HANGTNOBRAIN,                // 955
    S_COLONGIBS,                   // 956
    S_SMALLPOOL,                   // 957
    S_BRAINSTEM,                   // 958
    S_TECHLAMP,                    // 959
    S_TECHLAMP2,                   // 960
    S_TECHLAMP3,                   // 961
    S_TECHLAMP4,                   // 962
    S_TECH2LAMP,                   // 963
    S_TECH2LAMP2,                  // 964
    S_TECH2LAMP3,                  // 965
    S_TECH2LAMP4,                  // 966
    S_SMALL_WHITE_LIGHT,           // 967
    S_TEMPSOUNDORIGIN1,            // 968
    S_EXPLODE0,                // 969
    S_ROCKETPUFF1,                 // 970
    S_ROCKETPUFF2,                 // 971
    S_ROCKETPUFF3,                 // 972
    S_ROCKETPUFF4,                 // 973
  S_PLAY,
  S_PLAY_RUN1,
  S_PLAY_RUN2,
  S_PLAY_RUN3,
  S_PLAY_RUN4,
  S_PLAY_ATK1,
  S_PLAY_ATK2,
  S_PLAY_PAIN,
  S_PLAY_PAIN2,
  S_PLAY_DIE1,
  S_PLAY_DIE2,
  S_PLAY_DIE3,
  S_PLAY_DIE4,
  S_PLAY_DIE5,
  S_PLAY_DIE6,
  S_PLAY_DIE7,
S_CSAW,
S_WMACHINEGUNP,
S_WFLAMETHROWERP,
S_WCHAINHUNP,
S_WROCKETLAUNCHERP,
S_WHAND1P,
S_WHAND1P2,
S_WHAND1P3,
S_WHAND2P,
S_WHAND2P2,
S_WHAND2P3,
S_WHAND2P4,
S_WHAND2P5,
S_WHAND2P6,
S_WHAND2P7,
S_LMACHINEGUNP,
S_LFLAMETHROWERP,
S_LCHAINHUNP,
S_LROCKETLAUNCHERP,
S_OMACHINEGUNP,
S_OREVOLVERP,
S_OCHAINHUNP,
S_OROCKETLAUNCHERP,
S_O2MACHINEGUNP,
S_SCHAINHUNP,
S_UMACHINEGUNP,
S_UCHAINHUNP,
S_SNFLAMETHROWERP,
S_SNROCKETLAUNCHERP,
S_SAW,
S_SAWB,
S_SAWDOWN,
S_SAWUP,
S_SAW1,
S_SAW2,
S_SAW3,
S_WOLFKNIFE,                   // 974
S_WOLFKNIFEDOWN,               // 975
S_WOLFKNIFEUP,                 // 976
S_WOLFKNIFE1,                  // 977
S_WOLFKNIFE2,                  // 978
S_WOLFKNIFE3,                  // 979
S_WOLFKNIFE4,                  // 980
S_WOLFKNIFE5,                  // 981
S_WOLFPISTOL,                  // 982
S_WOLFPISTOLDOWN,              // 983
S_WOLFPISTOLUP,                // 984
S_WOLFPISTOL1,                 // 985
S_WOLFPISTOL2,                 // 986
S_WOLFPISTOL3,                 // 987
S_WOLFPISTOL4,                 // 988
S_WOLFPISTOL5,
S_WOLFMACHINEGUN,              // 989
S_WOLFMACHINEGUNDOWN,              // 990
S_WOLFMACHINEGUNUP,            // 991
S_WOLFMACHINEGUN1,             // 992
S_WOLFMACHINEGUN2,             // 993
S_WOLFMACHINEGUN3,             // 994
S_WOLFMACHINEGUN4,             // 995
S_WOLFMACHINEGUN5,             // 996
S_WOLFFLAMETHROWER,            // 997
S_WOLFFLAMETHROWERDOWN,            // 998
S_WOLFFLAMETHROWERUP,              // 999
S_WOLFFLAMETHROWER1,               // 1000
S_WOLFFLAMETHROWER2,               // 1001
S_WOLFFLAMETHROWER3,               // 1002
S_WOLFFLAMETHROWER4,               // 1003
S_WOLFCHAIN,                   // 1004
S_WOLFCHAINDOWN,               // 1005
S_WOLFCHAINUP,                 // 1006
S_WOLFCHAIN1,                  // 1007
S_WOLFCHAIN2,                  // 1008
S_WOLFCHAIN3,                  // 1009
S_WOLFCHAIN4,                  // 1010
S_WOLFCHAIN5,                  // 1011
S_WOLFMISSILE,                 // 1012
S_WOLFMISSILEDOWN,             // 1013
S_WOLFMISSILEUP,               // 1014
S_WOLFMISSILE1,                // 1015
S_WOLFMISSILE2,                // 1016
S_WOLFMISSILE3,                // 1017
S_WOLFMISSILE4,                // 1015
S_WOLFMISSILE5,                // 1016
S_WOLFMISSILE6,                // 1017
S_CHANDA,                  // 1022
S_CHANDADOWN,                  // 1023
S_CHANDAUP,                // 1024
S_CHANDA1,                 // 1025
S_CHANDA2,                 // 1026
S_CHANDA3,                 // 1027
S_CHANDB,                  // 1028
S_CHANDBDOWN,                  // 1029
S_CHANDBUP,                // 1030
S_CHANDB1,                 // 1031
S_CHANDB2,                 // 1032
S_CHANDB3,                 // 1033
S_CHANDB4,                 // 1031
S_CHANDB5,                 // 1032
S_CHANDB6,                 // 1033
S_CHANDB7,                 // 1033
S_SODMPKNIFE,                  // 1034
S_SODMPKNIFEDOWN,              // 1035
S_SODMPKNIFEUP,                // 1036
S_SODMPKNIFE1,                 // 1037
S_SODMPKNIFE2,                 // 1038
S_SODMPKNIFE3,                 // 1039
S_SODMPKNIFE4,                 // 1040
S_SODMPKNIFE5,                 // 1041
S_SODMPPISTOL,                 // 1042
S_SODMPPISTOLDOWN,             // 1043
S_SODMPPISTOLUP,               // 1044
S_SODMPPISTOL1,                // 1045
S_SODMPPISTOL2,                // 1046
S_SODMPPISTOL3,                // 1047
S_SODMPPISTOL4,                // 1048
S_SODMPPISTOL5,
S_SODMPMGUN,                   // 1049
S_SODMPMGUNDOWN,               // 1050
S_SODMPMGUNUP,                 // 1051
S_SODMPMGUN1,                  // 1052
S_SODMPMGUN2,                  // 1053
S_SODMPMGUN3,                  // 1054
S_SODMPMGUN4,                  // 1055
S_SODMPMGUN5,                  // 1056
S_SODMPFLAMETHROWER,
S_SODMPFLAMETHROWERDOWN,
S_SODMPFLAMETHROWERUP,
S_SODMPFLAMETHROWER1,
S_SODMPFLAMETHROWER2,
S_SODMPFLAMETHROWER3,
S_SODMPFLAMETHROWER4,
S_SODMPCHAIN,                  // 1057
S_SODMPCHAINDOWN,              // 1058
S_SODMPCHAINUP,                // 1059
S_SODMPCHAIN1,                 // 1060
S_SODMPCHAIN2,                 // 1061
S_SODMPCHAIN3,                 // 1062
S_SODMPCHAIN4,                 // 1063
S_SODMPCHAIN5,                 // 1064
S_SODMPMISSILE,                // 1065
S_SODMPMISSILEDOWN,            // 1066
S_SODMPMISSILEUP,              // 1067
S_SODMPMISSILE1,               // 1068
S_SODMPMISSILE2,               // 1069
S_SODMPMISSILE3,               // 1070
S_SODMPMISSILE4,               // 1068
S_SODMPMISSILE5,               // 1069
S_SODMPMISSILE6,               // 1070
S_ALPHAKNIFE,
S_ALPHAKNIFEDOWN,
S_ALPHAKNIFEUP,
S_ALPHAKNIFE1,
S_ALPHAKNIFE2,
S_ALPHAKNIFE3,
S_ALPHAKNIFE4,
S_ALPHAKNIFE5,
S_ALPHAPISTOL,
S_ALPHAPISTOLDOWN,
S_ALPHAPISTOLUP,
S_ALPHAPISTOL1,
S_ALPHAPISTOL2,
S_ALPHAPISTOL3,
S_ALPHAPISTOL4,
S_ALPHAPISTOL5,
S_ALPHAMGUN,
S_ALPHAMGUNDOWN,
S_ALPHAMGUNUP,
S_ALPHAMGUN1,
S_ALPHAMGUN2,
S_ALPHAMGUN3,
S_ALPHAMGUN4,
S_ALPHAMGUN5,
S_ALPHACHAIN,
S_ALPHACHAINDOWN,
S_ALPHACHAINUP,
S_ALPHACHAIN1,
S_ALPHACHAIN2,
S_ALPHACHAIN3,
S_ALPHACHAIN4,
S_ALPHACHAIN5,
S_OMSKNIFE,                // 1075
S_OMSKNIFEDOWN,                // 1076
S_OMSKNIFEUP,                  // 1077
S_OMSKNIFE1,                   // 1078
S_OMSKNIFE2,                   // 1079
S_OMSKNIFE3,                   // 1080
S_OMSKNIFE4,                   // 1081
S_OMSKNIFE5,                   // 1082
S_OMSRIFLE,                // 1083
S_OMSRIFLEDOWN,                // 1084
S_OMSRIFLEUP,                  // 1085
S_OMSRIFLE1,                   // 1086
S_OMSRIFLE2,                   // 1087
S_OMSRIFLE3,                   // 1088
S_OMSRIFLE4,                   // 1089
S_OMSMGUN,                 // 1090
S_OMSMGUNDOWN,                 // 1091
S_OMSMGUNUP,                   // 1092
S_OMSMGUN1,                // 1093
S_OMSMGUN2,                // 1094
S_OMSMGUN3,                // 1095
S_OMSMGUN4,                // 1096
S_OMSMGUN5,                // 1097
S_OMSCHAIN,                // 1100
S_OMSCHAINDOWN,                // 1101
S_OMSCHAINUP,                  // 1102
S_OMSCHAIN1,                   // 1103
S_OMSCHAIN2,                   // 1104
S_OMSCHAIN3,                   // 1105
S_OMSCHAIN4,                   // 1106
S_OMSREV,                  // 1107
S_OMSREVDOWN,                  // 1108
S_OMSREVUP,                // 1109
S_OMSREV1,                 // 1110
S_OMSREV2,                 // 1111
S_OMSREV3,                 // 1112
S_OMSREV4,                 // 1113
S_OMSREV5,                 // 1114
S_OMSREV6,                 // 1115
S_OMSREV7,                 // 1116
S_OMS2MGUN,
S_OMS2MGUNDOWN,
S_OMS2MGUNUP,
S_OMS2MGUN1,
S_OMS2MGUN2,
S_OMS2MGUN3,
S_OMS2MGUN4,
S_OMS2MGUN5,
S_OMS2MGUN6,
S_OMS2MGUN7,
S_SAELPISTOL,
S_SAELPISTOLDOWN,
S_SAELPISTOLUP,
S_SAELPISTOL1,
S_SAELPISTOL2,
S_SAELPISTOL3,
S_SAELPISTOL4,
S_SAELPISTOL5,
S_SAELMGUN,
S_SAELMGUNDOWN,
S_SAELMGUNUP,
S_SAELMGUN1,
S_SAELMGUN2,
S_SAELMGUN3,
S_SAELMGUN4,
S_SAELMGUN5,
S_SAELCHAIN,
S_SAELCHAINDOWN,
S_SAELCHAINUP,
S_SAELCHAIN1,
S_SAELCHAIN2,
S_SAELCHAIN3,
S_SAELCHAIN4,
S_SAELCHAIN5,
  S_URANUSKNIFE,
  S_URANUSKNIFEDOWN,
  S_URANUSKNIFEUP,
  S_URANUSKNIFE1,
  S_URANUSKNIFE2,
  S_URANUSKNIFE3,
  S_URANUSKNIFE4,
  S_URANUSKNIFE5,
  S_URANUSPISTOL,
  S_URANUSPISTOLDOWN,
  S_URANUSPISTOLUP,
  S_URANUSPISTOL1,
  S_URANUSPISTOL2,
  S_URANUSPISTOL3,
  S_URANUSPISTOL4,
  S_URANUSPISTOL5,
  S_URANUSMACHINEGUN,
  S_URANUSMACHINEGUNDOWN,
  S_URANUSMACHINEGUNUP,
  S_URANUSMACHINEGUN1,
  S_URANUSMACHINEGUN2,
  S_URANUSMACHINEGUN3,
  S_URANUSMACHINEGUN4,
  S_URANUSMACHINEGUN5,
  S_URANUSCHAIN,
  S_URANUSCHAINDOWN,
  S_URANUSCHAINUP,
  S_URANUSCHAIN1,
  S_URANUSCHAIN2,
  S_URANUSCHAIN3,
  S_URANUSCHAIN4,
  S_URANUSCHAIN5,
S_SNESKNIFE,                   // 1117
S_SNESKNIFEDOWN,               // 1118
S_SNESKNIFEUP,                 // 1119
S_SNESKNIFE1,                  // 1120
S_SNESKNIFE2,                  // 1121
S_SNESKNIFE3,                  // 1122
S_SNESKNIFE4,                  // 1123
S_SNESKNIFE5,                  // 1124
S_SNESPISTOL,                  // 1125
S_SNESPISTOLDOWN,              // 1126
S_SNESPISTOLUP,                // 1127
S_SNESPISTOL1,                 // 1128
S_SNESPISTOL2,                 // 1129
S_SNESPISTOL3,                 // 1130
S_SNESPISTOL4,                 // 1131
S_SNESPISTOL5,                 // 1132
S_SNESMGUN,                // 1133
S_SNESMGUNDOWN,                // 1134
S_SNESMGUNUP,                  // 1135
S_SNESMGUN1,                   // 1136
S_SNESMGUN2,                   // 1137
S_SNESMGUN3,                   // 1138
S_SNESMGUN4,                   // 1139
S_SNESMGUN5,                   // 1140
S_SNESFLAME,                   // 1141
S_SNESFLAMEDOWN,               // 1142
S_SNESFLAMEUP,                 // 1143
S_SNESFLAME1,                  // 1144
S_SNESFLAME2,                  // 1145
S_SNESFLAME3,                  // 1146
S_SNESFLAME4,                  // 1147
S_SNESCHAIN,                   // 1148
S_SNESCHAINDOWN,               // 1149
S_SNESCHAINUP,                 // 1150
S_SNESCHAIN1,                  // 1151
S_SNESCHAIN2,                  // 1152
S_SNESCHAIN3,                  // 1153
S_SNESCHAIN4,                  // 1154
S_SNESMISSILE,                 // 1155
S_SNESMISSILEDOWN,             // 1156
S_SNESMISSILEUP,               // 1157
S_SNESMISSILE1,                // 1158
S_SNESMISSILE2,                // 1159
S_SNESMISSILE3,                // 1160
S_SNESMISSILE4,                // 1161
S_JAGKNIFE,
S_JAGKNIFEDOWN,
S_JAGKNIFEUP,
S_JAGKNIFE1,
S_JAGKNIFE2,
S_JAGKNIFE3,
S_JAGKNIFE4,
S_JAGKNIFE5,
S_JAGPISTOL,
S_JAGPISTOLDOWN,
S_JAGPISTOLUP,
S_JAGPISTOL1,
S_JAGPISTOL2,
S_JAGPISTOL3,
S_JAGPISTOL4,
S_JAGPISTOL5,
S_JAGMGUN,
S_JAGMGUNDOWN,
S_JAGMGUNUP,
S_JAGMGUN1,
S_JAGMGUN2,
S_JAGMGUN3,
S_JAGMGUN4,
S_JAGMGUN5,
S_JAGFLAME,
S_JAGFLAME2,
S_JAGFLAMEDOWN,
S_JAGFLAMEUP,
S_JAGFLAME3,
S_JAGFLAME4,
S_JAGFLAME5,
S_JAGFLAME6,
S_JAGCHAIN,
S_JAGCHAINDOWN,
S_JAGCHAINUP,
S_JAGCHAIN1,
S_JAGCHAIN2,
S_JAGCHAIN3,
S_JAGCHAIN4,
S_JAGMISSILE,
S_JAGMISSILEDOWN,
S_JAGMISSILEUP,
S_JAGMISSILE1,
S_JAGMISSILE2,
S_JAGMISSILE3,
  S_3D0PISTOL,
  S_3D0PISTOLDOWN,
  S_3D0PISTOLUP,
  S_3D0PISTOL1,
  S_3D0PISTOL2,
  S_3D0PISTOL3,
  S_3D0PISTOL4,
  S_3D0PISTOL5,
  S_3D0MGUN,
  S_3D0MGUNDOWN,
  S_3D0MGUNUP,
  S_3D0MGUN1,
  S_3D0MGUN2,
  S_3D0MGUN3,
  S_3D0MGUN4,
  S_3D0MGUN5,
  S_3D0FLAME,
  S_3D0FLAMEDOWN,
  S_3D0FLAMEUP,
  S_3D0FLAME1,
  S_3D0FLAME2,
  S_3D0FLAME3,
  S_3D0FLAME4,
  S_3D0CHAIN,
  S_3D0CHAINDOWN,
  S_3D0CHAINUP,
  S_3D0CHAIN1,
  S_3D0CHAIN2,
  S_3D0CHAIN3,
  S_3D0CHAIN4,
  S_MULTIPISTOL,
  S_MULTIPISTOLDOWN,
  S_MULTIPISTOLUP,
  S_MULTIPISTOL1,
  S_MULTIPISTOL2,
  S_MULTIPISTOL3,
  S_MULTIPISTOL4,
  S_MULTIPISTOL5,
  S_MULTICHAIN,
  S_MULTICHAINDOWN,
  S_MULTICHAINUP,
  S_MULTICHAIN1,
  S_MULTICHAIN2,
  S_MULTICHAIN3,
  S_MULTICHAIN4,
  S_MULTICHAIN5,
  S_MULTISYRINGE,
  S_MULTISYRINGEREADY2,
  S_MULTISYRINGEDOWN,
  S_MULTISYRINGEUP,
  S_MULTISYRINGE1,
  S_MULTISYRINGE2,
  S_MULTISYRINGE3,
  S_MULTIMISSILE,
  S_MULTIMISSILEDOWN,
  S_MULTIMISSILEUP,
  S_MULTIMISSILE1,
  S_MULTIMISSILE2,
  S_MULTIMISSILE3,
  S_MULTIMISSILE4,
  S_MULTIMISSILE5,
  S_MULTIMISSILE6,
  S_MULTIRIFLE,
  S_MULTIRIFLEDOWN,
  S_MULTIRIFLEUP,
  S_MULTIRIFLE1,
  S_MULTIRIFLE2,
  S_MULTIRIFLE3,
  S_MULTIRIFLE4,
S_GUARD_STND,                  // 1162
S_GUARD_RUN1,                  // 1163
S_GUARD_RUN2,                  // 1164
S_GUARD_RUN3,                  // 1165
S_GUARD_RUN4,                  // 1166
S_GUARD_RUN5,                  // 1167
S_GUARD_RUN6,                  // 1168
S_GUARD_RUN7,                  // 1169
S_GUARD_RUN8,                  // 1170
S_GUARD_ATK1,                  // 1171
S_GUARD_ATK2,                  // 1172
S_GUARD_ATK3,                  // 1173
S_GUARD_ATK4,                  // 1174
S_GUARD_PAIN1,                 // 1175
S_GUARD_PAIN2,                 // 1176
S_GUARD_DIE1,                  // 1177
S_GUARD_DIE2,                  // 1178
S_GUARD_DIE3,                  // 1179
S_GUARD_DIE4,                  // 1180
S_GUARD_DIE5,                  // 1181
S_GUARD_XDIE1,                 // 1177
S_GUARD_XDIE2,                 // 1178
S_GUARD_XDIE3,                 // 1179
S_GUARD_XDIE4,                 // 1180
S_GUARD_XDIE5,                 // 1181
S_GUARD_XDIE6,                 // 1177
S_GUARD_XDIE7,                 // 1178
S_GUARD_XDIE8,                 // 1179
S_GUARD_XDIE9,                 // 1180
S_SSGUARD_STND,                // 1182
S_SSGUARD_RUN1,                // 1183
S_SSGUARD_RUN2,                // 1184
S_SSGUARD_RUN3,                // 1185
S_SSGUARD_RUN4,                // 1186
S_SSGUARD_RUN5,                // 1187
S_SSGUARD_RUN6,                // 1188
S_SSGUARD_RUN7,                // 1189
S_SSGUARD_RUN8,                // 1190
S_SSGUARD_ATK1,                // 1191
S_SSGUARD_ATK2,                // 1192
S_SSGUARD_ATK3,                // 1193
S_SSGUARD_ATK4,                // 1194
S_SSGUARD_ATK5,
S_SSGUARD_ATK6,
S_SSGUARD_ATK7,
S_SSGUARD_ATK8,
S_SSGUARD_ATK9,
S_SSGUARD_ATK10,
S_SSGUARD_PAIN1,               // 1195
S_SSGUARD_PAIN2,               // 1196
S_SSGUARD_DIE1,                // 1197
S_SSGUARD_DIE2,                // 1198
S_SSGUARD_DIE3,                // 1199
S_SSGUARD_DIE4,                // 1200
S_SSGUARD_DIE5,                // 1201
S_SSGUARD_XDIE1,               // 1177
S_SSGUARD_XDIE2,               // 1178
S_SSGUARD_XDIE3,               // 1179
S_SSGUARD_XDIE4,               // 1180
S_SSGUARD_XDIE5,               // 1181
S_SSGUARD_XDIE6,               // 1177
S_SSGUARD_XDIE7,               // 1178
S_SSGUARD_XDIE8,               // 1179
S_SSGUARD_XDIE9,               // 1180
S_DOG_STND1,                   // 1202
S_DOG_STND2,                   // 1203
S_DOG_STND3,                   // 1204
S_DOG_STND4,                   // 1205
S_DOG_RUN1,                // 1206
S_DOG_RUN2,                // 1207
S_DOG_RUN3,                // 1208
S_DOG_RUN4,                // 1209
S_DOG_RUN5,                // 1210
S_DOG_RUN6,                // 1211
S_DOG_RUN7,                // 1212
S_DOG_RUN8,                // 1213
S_DOG_ATK1,                // 1214
S_DOG_ATK2,                // 1215
S_DOG_ATK3,                // 1216
S_DOG_DIE1,                // 1219
S_DOG_DIE2,                // 1220
S_DOG_DIE3,                // 1221
S_DOG_DIE4,                // 1222
S_DOG_XDIE1,                   // 1219
S_DOG_XDIE2,                   // 1220
S_DOG_XDIE3,                   // 1221
S_DOG_XDIE4,                   // 1222
S_MUTANT_STND,                 // 1223
S_MUTANT_RUN1,                 // 1224
S_MUTANT_RUN2,                 // 1225
S_MUTANT_RUN3,                 // 1226
S_MUTANT_RUN4,                 // 1227
S_MUTANT_RUN5,                 // 1228
S_MUTANT_RUN6,                 // 1229
S_MUTANT_RUN7,                 // 1230
S_MUTANT_RUN8,                 // 1231
S_MUTANT_ATK1,                 // 1232
S_MUTANT_ATK2,                 // 1233
S_MUTANT_ATK3,                 // 1234
S_MUTANT_ATK4,                 // 1235
S_MUTANT_ATK5,                 // 1236
S_MUTANT_ATK6,                 // 1237
S_MUTANT_ATK7,                 // 1238
S_MUTANT_ATK8,                 // 1239
S_MUTANT_MATK1,                // 1240
S_MUTANT_MATK2,                // 1241
S_MUTANT_MATK3,                // 1242
S_MUTANT_MATK4,                // 1243
S_MUTANT_PAIN1,                // 1244
S_MUTANT_PAIN2,                // 1245
S_MUTANT_DIE1,                 // 1246
S_MUTANT_DIE2,                 // 1247
S_MUTANT_DIE3,                 // 1248
S_MUTANT_DIE4,                 // 1249
S_MUTANT_DIE5,                 // 1250
S_MUTANT_DIE6,                 // 1251
S_MUTANT_XDIE1,                // 1246
S_MUTANT_XDIE2,                // 1247
S_MUTANT_XDIE3,                // 1248
S_MUTANT_XDIE4,                // 1249
S_MUTANT_XDIE5,                // 1250
S_MUTANT_XDIE6,                // 1251
S_MUTANT_XDIE7,                // 1250
S_MUTANT_XDIE8,                // 1251
S_OFFICER_STND,                // 1252
S_OFFICER_RUN1,                // 1253
S_OFFICER_RUN2,                // 1254
S_OFFICER_RUN3,                // 1255
S_OFFICER_RUN4,                // 1256
S_OFFICER_RUN5,                // 1257
S_OFFICER_RUN6,                // 1258
S_OFFICER_RUN7,                // 1259
S_OFFICER_RUN8,                // 1260
S_OFFICER_ATK1,                // 1261
S_OFFICER_ATK2,                // 1262
S_OFFICER_ATK3,                // 1263
S_OFFICER_ATK4,                // 1264
S_OFFICER_PAIN1,               // 1265
S_OFFICER_PAIN2,               // 1266
S_OFFICER_DIE1,                // 1267
S_OFFICER_DIE2,                // 1268
S_OFFICER_DIE3,                // 1269
S_OFFICER_DIE4,                // 1270
S_OFFICER_DIE5,                // 1271
S_OFFICER_XDIE1,               // 1267
S_OFFICER_XDIE2,               // 1268
S_OFFICER_XDIE3,               // 1269
S_OFFICER_XDIE4,               // 1270
S_OFFICER_XDIE5,               // 1271
S_OFFICER_XDIE6,               // 1268
S_OFFICER_XDIE7,               // 1269
S_OFFICER_XDIE8,               // 1270
S_OFFICER_XDIE9,               // 1271
S_HANS_STND,                   // 1272
S_HANS_RUN1,                   // 1273
S_HANS_RUN2,                   // 1274
S_HANS_RUN3,                   // 1275
S_HANS_RUN4,                   // 1276
S_HANS_RUN5,                   // 1277
S_HANS_RUN6,                   // 1278
S_HANS_RUN7,                   // 1279
S_HANS_RUN8,                   // 1280
S_HANS_ATK1,                   // 1281
S_HANS_ATK2,                   // 1282
S_HANS_ATK3,                   // 1283
S_HANS_ATK4,                   // 1284
S_HANS_ATK5,                   // 1285
S_HANS_ATK6,                   // 1286
S_HANS_DIE1,                   // 1291
S_HANS_DIE2,                   // 1292
S_HANS_DIE3,                   // 1293
S_HANS_DIE4,                   // 1294
S_HANS_DIE5,                   // 1295
S_SCHABBS_STND,                // 1296
S_SCHABBS_RUN1,                // 1297
S_SCHABBS_RUN2,                // 1298
S_SCHABBS_RUN3,                // 1299
S_SCHABBS_RUN4,                // 1300
S_SCHABBS_RUN5,                // 1301
S_SCHABBS_RUN6,                // 1302
S_SCHABBS_RUN7,                // 1303
S_SCHABBS_RUN8,                // 1304
S_SCHABBS_ATK1,                // 1305
S_SCHABBS_ATK2,                // 1306
S_SCHABBS_DIE1,                // 1307
S_SCHABBS_DIE2,                // 1308
S_SCHABBS_DIE3,                // 1309
S_SCHABBS_DIE4,                // 1310
S_SCHABBS_DIE5,                // 1311
S_SCHABBS_DIE6,                // 1312
S_FAKEHITLER_STND,             // 1313
S_FAKEHITLER_RUN1,             // 1314
S_FAKEHITLER_RUN2,             // 1315
S_FAKEHITLER_RUN3,             // 1316
S_FAKEHITLER_RUN4,             // 1317
S_FAKEHITLER_RUN5,             // 1318
S_FAKEHITLER_RUN6,             // 1319
S_FAKEHITLER_RUN7,             // 1320
S_FAKEHITLER_RUN8,             // 1321
S_FAKEHITLER_ATK1,             // 1322
S_FAKEHITLER_ATK2,             // 1323
S_FAKEHITLER_ATK3,             // 1324
S_FAKEHITLER_ATK4,             // 1325
S_FAKEHITLER_ATK5,             // 1326
S_FAKEHITLER_ATK6,             // 1327
S_FAKEHITLER_DIE1,             // 1328
S_FAKEHITLER_DIE2,             // 1329
S_FAKEHITLER_DIE3,             // 1330
S_FAKEHITLER_DIE4,             // 1331
S_FAKEHITLER_DIE5,             // 1332
S_FAKEHITLER_DIE6,             // 1333
S_MECH_STND,                   // 1334
S_MECH_RUN1,                   // 1335
S_MECH_RUN2,                   // 1336
S_MECH_RUN3,                   // 1337
S_MECH_RUN4,                   // 1338
S_MECH_RUN5,                   // 1339
S_MECH_RUN6,                   // 1340
S_MECH_RUN7,                   // 1341
S_MECH_RUN8,                   // 1342
S_MECH_ATK1,                   // 1343
S_MECH_ATK2,                   // 1344
S_MECH_ATK3,                   // 1345
S_MECH_ATK4,                   // 1346
S_MECH_ATK5,                   // 1347
S_MECH_ATK6,                   // 1348
S_MECH_DIE1,                   // 1349
S_MECH_DIE2,                   // 1350
S_MECH_DIE3,                   // 1351
S_MECH_DIE4,                   // 1352
S_HITLER_STND,                 // 1353
S_HITLER_RUN1,                 // 1354
S_HITLER_RUN2,                 // 1355
S_HITLER_RUN3,                 // 1356
S_HITLER_RUN4,                 // 1357
S_HITLER_RUN5,                 // 1358
S_HITLER_RUN6,                 // 1359
S_HITLER_RUN7,                 // 1360
S_HITLER_RUN8,                 // 1361
S_HITLER_ATK1,                 // 1362
S_HITLER_ATK2,                 // 1363
S_HITLER_ATK3,                 // 1364
S_HITLER_ATK4,                 // 1365
S_HITLER_ATK5,                 // 1366
S_HITLER_ATK6,                 // 1367
S_HITLER_DIE1,                 // 1368
S_HITLER_DIE2,                 // 1369
S_HITLER_DIE3,                 // 1370
S_HITLER_DIE4,                 // 1371
S_HITLER_DIE5,                 // 1372
S_HITLER_DIE6,                 // 1373
S_HITLER_DIE7,                 // 1374
S_HITLER_DIE8,                 // 1375
S_HITLER_DIE9,                 // 1376
S_HITLER_DIE10,                // 1377
S_PACMANPINK_STND1,            // 1378
S_PACMANPINK_STND2,            // 1379
S_PACMANPINK_RUN1,             // 1380
S_PACMANPINK_RUN2,             // 1381
S_PACMANPINK_ATK1,             // 1382
S_PACMANPINK_ATK2,             // 1383
S_PACMANPINK_ATK3,             // 1384
S_PACMANPINK_ATK4,             // 1385
S_PACMANPINK_MATK1,            // 1386
S_PACMANPINK_MATK2,            // 1387
S_PACMANPINKBLUR1,             // 1388
S_PACMANPINKBLUR2,             // 1389
S_PACMANRED_STND1,             // 1390
S_PACMANRED_STND2,             // 1391
S_PACMANRED_RUN1,              // 1392
S_PACMANRED_RUN2,              // 1393
S_PACMANRED_ATK1,              // 1394
S_PACMANRED_ATK2,              // 1395
S_PACMANRED_ATK3,              // 1396
S_PACMANRED_ATK4,              // 1397
S_PACMANRED_MATK1,             // 1398
S_PACMANRED_MATK2,             // 1399
S_PACMANREDBLUR1,              // 1400
S_PACMANREDBLUR2,              // 1401
S_PACMANORANGE_STND1,              // 1402
S_PACMANORANGE_STND2,              // 1403
S_PACMANORANGE_RUN1,               // 1404
S_PACMANORANGE_RUN2,               // 1405
S_PACMANORANGEBLUR1,               // 1406
S_PACMANORANGEBLUR2,               // 1407
S_PACMANBLUE_STND1,            // 1408
S_PACMANBLUE_STND2,            // 1409
S_PACMANBLUE_RUN1,             // 1410
S_PACMANBLUE_RUN2,             // 1411
S_PACMANBLUE_MATK1,            // 1412
S_PACMANBLUE_MATK2,            // 1413
S_PACMANBLUEBLUR1,             // 1414
S_PACMANBLUEBLUR2,             // 1415
S_OTTO_STND,                   // 1416
S_OTTO_RUN1,                   // 1417
S_OTTO_RUN2,                   // 1418
S_OTTO_RUN3,                   // 1419
S_OTTO_RUN4,                   // 1420
S_OTTO_RUN5,                   // 1421
S_OTTO_RUN6,                   // 1422
S_OTTO_RUN7,                   // 1423
S_OTTO_RUN8,                   // 1424
S_OTTO_ATK1,                   // 1425
S_OTTO_ATK2,                   // 1426
S_OTTO_ATK3,                   // 1427
S_OTTO_DIE1,                   // 1428
S_OTTO_DIE2,                   // 1429
S_OTTO_DIE3,                   // 1430
S_OTTO_DIE4,                   // 1431
S_OTTO_DIE5,                   // 1432
S_OTTO_DIE6,                   // 1433
S_GRETTEL_STND,                // 1434
S_GRETTEL_RUN1,                // 1435
S_GRETTEL_RUN2,                // 1436
S_GRETTEL_RUN3,                // 1437
S_GRETTEL_RUN4,                // 1438
S_GRETTEL_RUN5,                // 1439
S_GRETTEL_RUN6,                // 1440
S_GRETTEL_RUN7,                // 1441
S_GRETTEL_RUN8,                // 1442
S_GRETTEL_ATK1,                // 1443
S_GRETTEL_ATK2,                // 1444
S_GRETTEL_ATK3,                // 1445
S_GRETTEL_ATK4,                // 1446
S_GRETTEL_ATK5,                // 1447
S_GRETTEL_ATK6,                // 1448
S_GRETTEL_DIE1,                // 1449
S_GRETTEL_DIE2,                // 1450
S_GRETTEL_DIE3,                // 1451
S_GRETTEL_DIE4,                // 1452
S_GRETTEL_DIE5,                // 1453
S_FATFACE_STND,                // 1454
S_FATFACE_RUN1,                // 1455
S_FATFACE_RUN2,                // 1456
S_FATFACE_RUN3,                // 1457
S_FATFACE_RUN4,                // 1458
S_FATFACE_RUN5,                // 1459
S_FATFACE_RUN6,                // 1460
S_FATFACE_RUN7,                // 1461
S_FATFACE_RUN8,                // 1462
S_FATFACE_ATK,                 // 1463
S_FATFACE_ATK1_1,              // 1464
S_FATFACE_ATK1_2,              // 1465
S_FATFACE_ATK1_3,              // 1466
S_FATFACE_ATK1_4,              // 1467
S_FATFACE_ATK1_5,              // 1468
S_FATFACE_ATK2_1,              // 1469
S_FATFACE_ATK2_2,              // 1470
S_FATFACE_ATK2_3,              // 1471
S_FATFACE_ATK2_4,              // 1472
S_FATFACE_ATK2_5,              // 1473
S_FATFACE_ATK2_6,              // 1474
S_FATFACE_DIE1,                // 1475
S_FATFACE_DIE2,                // 1476
S_FATFACE_DIE3,                // 1477
S_FATFACE_DIE4,                // 1478
S_FATFACE_DIE5,                // 1479
S_FATFACE_DIE6,                // 1480
S_TRANS_STND,                  // 1481
S_TRANS_RUN1,                  // 1482
S_TRANS_RUN2,                  // 1483
S_TRANS_RUN3,                  // 1484
S_TRANS_RUN4,                  // 1485
S_TRANS_RUN5,                  // 1486
S_TRANS_RUN6,                  // 1487
S_TRANS_RUN7,                  // 1488

S_TRANS_RUN8,                  // 1489
S_TRANS_ATK1,                  // 1490
S_TRANS_ATK2,                  // 1491
S_TRANS_ATK3,                  // 1492
S_TRANS_ATK4,                  // 1493
S_TRANS_ATK5,                  // 1494
S_TRANS_ATK6,                  // 1495
S_TRANS_DIE1,                  // 1496
S_TRANS_DIE2,                  // 1497
S_TRANS_DIE3,                  // 1498
S_TRANS_DIE4,                  // 1499
S_WILL_STND,                   // 1500
S_WILL_RUN1,                   // 1501
S_WILL_RUN2,                   // 1502
S_WILL_RUN3,                   // 1503
S_WILL_RUN4,                   // 1504
S_WILL_RUN5,                   // 1505
S_WILL_RUN6,                   // 1506
S_WILL_RUN7,                   // 1507
S_WILL_RUN8,                   // 1508
S_WILL_ATK,                // 1509
S_WILL_ATK1_1,                 // 1510
S_WILL_ATK1_2,                 // 1511
S_WILL_ATK1_3,                 // 1512
S_WILL_ATK2_1,                 // 1513
S_WILL_ATK2_2,                 // 1514
S_WILL_ATK2_3,                 // 1515
S_WILL_ATK2_4,                 // 1516
S_WILL_ATK2_5,                 // 1517
S_WILL_ATK2_6,                 // 1518
S_WILL_DIE1,                   // 1519
S_WILL_DIE2,                   // 1520
S_WILL_DIE3,                   // 1521
S_WILL_DIE4,                   // 1522
S_UBER_STND,                   // 1523
S_UBER_RUN1,                   // 1524
S_UBER_RUN2,                   // 1525
S_UBER_RUN3,                   // 1526
S_UBER_RUN4,                   // 1527
S_UBER_RUN5,                   // 1528
S_UBER_RUN6,                   // 1529
S_UBER_RUN7,                   // 1530
S_UBER_RUN8,                   // 1531
S_UBER_ATK1,                   // 1532
S_UBER_ATK2,                   // 1533
S_UBER_ATK3,                   // 1534
S_UBER_ATK4,                   // 1535
S_UBER_ATK5,                   // 1536
S_UBER_ATK6,                   // 1536
S_UBER_DIE1,                   // 1537
S_UBER_DIE2,                   // 1538
S_UBER_DIE3,                   // 1539
S_UBER_DIE4,                   // 1540
S_UBER_DIE5,                   // 1541
S_ABOS_STND,                   // 1542
S_ABOS_RUN1,                   // 1543
S_ABOS_RUN2,                   // 1544
S_ABOS_RUN3,                   // 1545
S_ABOS_RUN4,                   // 1546
S_ABOS_RUN5,                   // 1547
S_ABOS_RUN6,                   // 1548
S_ABOS_RUN7,                   // 1549
S_ABOS_RUN8,                   // 1550
S_ABOS_ATK,                // 1509
S_ABOS_ATK1_1,                 // 1510
S_ABOS_ATK1_2,                 // 1511
S_ABOS_ATK1_3,                 // 1512
S_ABOS_ATK2_1,                 // 1513
S_ABOS_ATK2_2,                 // 1514
S_ABOS_ATK2_3,                 // 1515
S_ABOS_ATK2_4,                 // 1516
S_ABOS_ATK2_5,                 // 1517
S_ABOS_ATK2_6,                 // 1518
S_ABOS_ATK2_7,
S_ABOS_ATK2_8,
S_ABOS_ATK2_9,
S_ABOS_ATK2_10,
S_ABOS_DIE1,                   // 1564
S_ABOS_DIE2,                   // 1564
S_ABOS_DIE3,                   // 1565
S_ABOS_DIE4,                   // 1566
S_ABOS_DIE5,                   // 1567
S_ABOS_DIE6,                   // 1569
S_ABOS_DIE7,                   // 1569
S_GBOS_STND1,                  // 1570
S_GBOS_STND2,                  // 1571
S_GBOS_STND3,                  // 1572
S_GBOS_STND4,                  // 1573
S_GBOS_RUN1,                   // 1574
S_GBOS_RUN2,                   // 1575
S_GBOS_RUN3,                   // 1576
S_GBOS_RUN4,                   // 1577
S_GBOS_ATK1,                   // 1578
S_GBOS_ATK2,                   // 1579
S_GBOS_ATK3,                   // 1578
S_GBOS_ATK4,                   // 1579
S_GBOS_DIE1,                   // 1582
S_GBOS_DIE2,                   // 1583
S_GBOS_DIE3,                   // 1584
S_GBOS_DIE4,                   // 1585
S_GBOS_DIE5,                   // 1586
S_ANG_STND1,                   // 1587
S_ANG_STND2,                   // 1588
S_ANG_RUN1,                // 1589
S_ANG_RUN2,                // 1590
S_ANG_RUN3,                // 1591
S_ANG_RUN4,                // 1592
S_ANG_RUN5,                // 1593
S_ANG_RUN6,                // 1594
S_ANG_RUN7,                // 1595
S_ANG_RUN8,                // 1596
S_ANG_ATK,                 // 1597
S_ANG_ATK1_1,                  // 1598
S_ANG_ATK1_2,                  // 1599
S_ANG_ATK1_3,                  // 1600
S_ANG_ATK1_4,                  // 1601
S_ANG_ATK1_5,                  // 1602
S_ANG_ATK1_6,                  // 1603
S_ANG_ATK2_1,                  // 1604
S_ANG_ATK2_2,                  // 1605
S_ANG_ATK2_3,                  // 1606
S_ANG_ATK2_4,                  // 1607
S_ANG_ATK2_5,                  // 1608
S_ANG_ATK2_6,                  // 1609
S_ANG_ATK2_7,                  // 1610
S_ANG_ATK3_1,                  // 1611
S_ANG_ATK3_2,                  // 1612
S_ANG_ATK3_3,                  // 1613
S_ANG_DIE1,                // 1614
S_ANG_DIE2,                // 1615
S_ANG_DIE3,                // 1616
S_ANG_DIE4,                // 1617
S_ANG_DIE5,                // 1618
S_ANG_DIE6,                // 1619
S_ANG_DIE7,                // 1620
S_ANG_DIE8,                // 1621
S_ANG_DIE9,                // 1622
S_ANG_DIE10,                   // 1623
S_ANGELFIRE1,                      // 281
S_ANGELFIRE2,                      // 282
S_ANGELFIRE3,                      // 283
S_ANGELFIRE4,                      // 284
S_ANGELFIRE5,                      // 285
S_ANGELFIRE6,                      // 286
S_ANGELFIRE7,                      // 287
S_ANGELFIRE8,                      // 288
S_ANGELFIRE9,                      // 289
S_ANGELFIRE10,                     // 290
S_ANGELFIRE11,                     // 291
S_ANGELFIRE12,                     // 292
S_ANGELFIRE13,                     // 293
S_ANGELFIRE14,                     // 294
S_ANGELFIRE15,                     // 295
S_ANGELFIRE16,                     // 296
S_ANGELFIRE17,                     // 297
S_ANGELFIRE18,                     // 298
S_ANGELFIRE19,                     // 299
S_ANGELFIRE20,                     // 300
S_ANGELFIRE21,                     // 301
S_ANGELFIRE22,                     // 302
S_ANGELFIRE23,                     // 303
S_ANGELFIRE24,                     // 304
S_ANGELFIRE25,                     // 305
S_ANGELFIRE26,                     // 306
S_ANGELFIRE27,                     // 307
S_ANGELFIRE28,                     // 308
S_ANGELFIRE29,                     // 309
S_ANGELFIRE30,                     // 310
S_RGBOS_LOOK,                  // 1624
S_RGBOS_RAISE1,                // 1625
S_RGBOS_RAISE2,                // 1626
S_RGBOS_RAISE3,                // 1627
S_RGBOS_RAISE4,                // 1628
S_RGBOS_ATK1,                  // 1628
S_RGBOS_ATK2,                  // 1628
S_RGBOS_ATK3,                  // 1628
S_RGBOS_ATK4,                  // 1628
S_HANB_STND,                   // 1629
S_HANB_RUN1,                   // 1630
S_HANB_RUN2,                   // 1631
S_HANB_RUN3,                   // 1632
S_HANB_RUN4,                   // 1633
S_HANB_RUN5,                   // 1634
S_HANB_RUN6,                   // 1635
S_HANB_RUN7,                   // 1636
S_HANB_RUN8,                   // 1637
S_HANB_ATK1,                   // 1638
S_HANB_ATK2,                   // 1639
S_HANB_ATK3,                   // 1640
S_HANB_ATK4,                   // 1641
S_HANB_ATK5,                   // 1642
S_HANB_ATK6,                   // 1643
S_HANB_DIE1,                   // 1648
S_HANB_DIE2,                   // 1649
S_HANB_DIE3,                   // 1650
S_HANB_DIE4,                   // 1651
S_HANB_DIE5,                   // 1652
S_HANC_STND,                   // 1653
S_HANC_RUN1,                   // 1654
S_HANC_RUN2,                   // 1655
S_HANC_RUN3,                   // 1656
S_HANC_RUN4,                   // 1657
S_HANC_RUN5,                   // 1658
S_HANC_RUN6,                   // 1659
S_HANC_RUN7,                   // 1660
S_HANC_RUN8,                   // 1661
S_HANC_ATK1,                   // 1662
S_HANC_ATK2,                   // 1663
S_HANC_ATK3,                   // 1664
S_HANC_ATK4,                   // 1665
S_HANC_ATK5,                   // 1666
S_HANC_ATK6,                   // 1667
S_HANC_DIE1,                   // 1672
S_HANC_DIE2,                   // 1673
S_HANC_DIE3,                   // 1674
S_HANC_DIE4,                   // 1675
S_HANC_DIE5,                   // 1676
S_HANS2_RUN1,                  // 1678
S_HANS2_RUN2,                  // 1679
S_HANS2_RUN3,                  // 1680
S_HANS2_RUN4,                  // 1681
S_HANS2_RUN5,                  // 1682
S_HANS2_RUN6,                  // 1683
S_HANS2_RUN7,                  // 1684
S_HANS2_RUN8,                  // 1685
S_HANS2_DIE1,                  // 1696
S_HANS2_DIE2,                  // 1697
S_HANS2_DIE3,                  // 1698
S_HANS2_DIE4,                  // 1699
S_HANS2_DIE5,                  // 1700
S_MUTANT2_STND,                // 1701
S_MUTANT2_RUN1,                // 1702
S_MUTANT2_RUN2,                // 1703
S_MUTANT2_RUN3,                // 1704
S_MUTANT2_RUN4,                // 1705
S_MUTANT2_RUN5,                // 1706
S_MUTANT2_RUN6,                // 1707
S_MUTANT2_RUN7,                // 1708
S_MUTANT2_RUN8,                // 1709
S_MUTANT2_ATK1,                // 1710
S_MUTANT2_ATK2,                // 1711
S_MUTANT2_ATK3,                // 1712
S_MUTANT2_ATK4,                // 1713
S_MUTANT2_PAIN1,               // 1714
S_MUTANT2_PAIN2,               // 1715
S_MUTANT2_DIE1,                // 1716
S_MUTANT2_DIE2,                // 1717
S_MUTANT2_DIE3,                // 1718
S_MUTANT2_DIE4,                // 1719
S_MUTANT2_DIE5,                // 1720
S_MUTANT2_DIE6,                // 1721
S_MUTANT2_XDIE1,               // 1716
S_MUTANT2_XDIE2,               // 1717
S_MUTANT2_XDIE3,               // 1718
S_MUTANT2_XDIE4,               // 1719
S_MUTANT2_XDIE5,               // 1720
S_MUTANT2_XDIE6,               // 1721
S_MUTANT2_XDIE7,               // 1720
S_MUTANT2_XDIE8,               // 1721
S_MUTANT3_STND,                // 1722
S_MUTANT3_RUN1,                // 1723
S_MUTANT3_RUN2,                // 1724
S_MUTANT3_RUN3,                // 1725
S_MUTANT3_RUN4,                // 1726
S_MUTANT3_RUN5,                // 1727
S_MUTANT3_RUN6,                // 1728
S_MUTANT3_RUN7,                // 1729
S_MUTANT3_RUN8,                // 1730
S_MUTANT3_ATK1,                // 1731
S_MUTANT3_ATK2,                // 1732
S_MUTANT3_ATK3,                // 1733
S_MUTANT3_ATK4,                // 1734
S_MUTANT3_ATK5,                // 1734
S_MUTANT3_MATK1,               // 1735
S_MUTANT3_MATK2,               // 1736
S_MUTANT3_MATK3,               // 1737
S_MUTANT3_MATK4,               // 1738
S_MUTANT3_PAIN1,               // 1739
S_MUTANT3_PAIN2,               // 1740
S_MUTANT3_DIE1,                // 1741
S_MUTANT3_DIE2,                // 1742
S_MUTANT3_DIE3,                // 1743
S_MUTANT3_DIE4,                // 1744
S_MUTANT3_DIE5,                // 1745
S_MUTANT3_DIE6,                // 1746
S_MUTANT3_XDIE1,               // 1716
S_MUTANT3_XDIE2,               // 1717
S_MUTANT3_XDIE3,               // 1718
S_MUTANT3_XDIE4,               // 1719
S_MUTANT3_XDIE5,               // 1720
S_MUTANT3_XDIE6,               // 1721
S_MUTANT3_XDIE7,               // 1720
S_MUTANT3_XDIE8,               // 1721
S_OFFICER2_STND,               // 1747
S_OFFICER2_RUN1,               // 1748
S_OFFICER2_RUN2,               // 1749
S_OFFICER2_RUN3,               // 1750
S_OFFICER2_RUN4,               // 1751
S_OFFICER2_RUN5,               // 1752
S_OFFICER2_RUN6,               // 1753
S_OFFICER2_RUN7,               // 1754
S_OFFICER2_RUN8,               // 1755
S_OFFICER2_ATK1,               // 1756
S_OFFICER2_ATK2,               // 1757
S_OFFICER2_ATK3,               // 1758
S_OFFICER2_ATK4,               // 1759
S_OFFICER2_PAIN1,              // 1760
S_OFFICER2_PAIN2,              // 1761
S_OFFICER2_DIE1,               // 1762
S_OFFICER2_DIE2,               // 1763
S_OFFICER2_DIE3,               // 1764
S_OFFICER2_DIE4,               // 1765
S_OFFICER2_DIE5,               // 1766
S_OFFICER2_DIE6,               // 1767
S_OFFICER2_xDIE1,              // 1762
S_OFFICER2_xDIE2,              // 1763
S_OFFICER2_xDIE3,              // 1764
S_OFFICER2_xDIE4,              // 1765
S_OFFICER2_xDIE5,              // 1766
S_OFFICER2_xDIE6,              // 1767
S_OFFICER2_xDIE7,              // 1765
S_OFFICER2_xDIE8,              // 1766
S_OFFICER2_xDIE9,              // 1767
S_OFFICER2SK_DIE1,             // 1769
S_OFFICER2SK_XDIE1,            // 1769
S_OFFICER2GK_DIE1,             // 1768
S_OFFICER2GK_XDIE1,            // 1768
S_RAT_STND,                // 1770
S_RAT_RUN1,                // 1771
S_RAT_RUN2,                // 1772
S_RAT_RUN3,                // 1773
S_RAT_RUN4,                // 1774
S_RAT_ATK1,                // 1775
S_RAT_ATK2,                // 1776
S_RAT_PAIN1,                   // 1777
S_RAT_PAIN2,                   // 1778
S_RAT_DIE1,                // 1779
S_RAT_DIE2,                // 1780
S_RAT_DIE3,                // 1781
S_RAT_DIE4,                // 1782
S_RAT_DIE5,                // 1783
S_RAT_DIE6,                // 1784
S_FLAMEGUARD_STND1,            // 1785
S_FLAMEGUARD_STND2,            // 1785
S_FLAMEGUARD_STND3,            // 1785
S_FLAMEGUARD_STND4,            // 1785
S_FLAMEGUARD_RUN1,             // 1786
S_FLAMEGUARD_RUN2,             // 1787
S_FLAMEGUARD_RUN3,             // 1788
S_FLAMEGUARD_RUN4,             // 1789
S_FLAMEGUARD_RUN5,             // 1790
S_FLAMEGUARD_RUN6,             // 1791
S_FLAMEGUARD_RUN7,             // 1792
S_FLAMEGUARD_RUN8,             // 1793
S_FLAMEGUARD_ATK1,             // 1794
S_FLAMEGUARD_ATK2,             // 1795
S_FLAMEGUARD_ATK3,             // 1796
S_FLAMEGUARD_ATK4,             // 1797
S_FLAMEGUARD_ATK5,             // 1798
S_FLAMEGUARD_ATK6,             // 1799
S_FLAMEGUARD_ATK7,             // 1800
S_FLAMEGUARD_ATK8,             // 1801
S_FLAMEGUARD_ATK9,             // 1802
S_FLAMEGUARD_PAIN1,            // 1803
S_FLAMEGUARD_PAIN2,            // 1804
S_FLAMEGUARD_DIE1,             // 1805
S_FLAMEGUARD_DIE2,             // 1806
S_FLAMEGUARD_DIE3,             // 1807
S_FLAMEGUARD_DIE4,             // 1808
S_FLAMEGUARD_DIE5,             // 1809
S_FLAMEGUARD_XDIE1,            // 1805
S_FLAMEGUARD_XDIE2,            // 1806
S_FLAMEGUARD_XDIE3,            // 1807
S_FLAMEGUARD_XDIE4,            // 1808
S_FLAMEGUARD_XDIE5,            // 1809
S_FLAMEGUARD_XDIE6,            // 1805
S_FLAMEGUARD_XDIE7,            // 1806
S_FLAMEGUARD_XDIE8,            // 1807
S_FLAMEGUARD_XDIE9,            // 1808
S_ELITEGUARD_STND,             // 1810
S_ELITEGUARD_RUN1,             // 1811
S_ELITEGUARD_RUN2,             // 1812
S_ELITEGUARD_RUN3,             // 1813
S_ELITEGUARD_RUN4,             // 1814
S_ELITEGUARD_RUN5,             // 1815
S_ELITEGUARD_RUN6,             // 1816
S_ELITEGUARD_RUN7,             // 1817
S_ELITEGUARD_RUN8,             // 1818
S_ELITEGUARD_ATK1,             // 1819
S_ELITEGUARD_ATK2,             // 1820
S_ELITEGUARD_ATK3,             // 1724
S_ELITEGUARD_ATK4,             // 1725
S_ELITEGUARD_PAIN1,            // 1731
S_ELITEGUARD_PAIN2,            // 1732
S_ELITEGUARD_DIE1,             // 1733
S_ELITEGUARD_DIE2,             // 1734
S_ELITEGUARD_DIE3,             // 1735
S_ELITEGUARD_DIE4,             // 1736
S_ELITEGUARD_DIE5,             // 1737
S_ELITEGUARD_XDIE1,            // 1733
S_ELITEGUARD_XDIE2,            // 1734
S_ELITEGUARD_XDIE3,            // 1735
S_ELITEGUARD_XDIE4,            // 1736
S_ELITEGUARD_XDIE5,            // 1737
S_ELITEGUARD_XDIE6,            // 1733
S_ELITEGUARD_XDIE7,            // 1734
S_ELITEGUARD_XDIE8,            // 1735
S_ELITEGUARD_XDIE9,            // 1736
S_SPEAR1,                  // 1738
S_SPEAR2,                  // 1739
S_TCROSS,                  // 1740
S_TCUP,                    // 1741
S_TCHEST,                  // 1742
S_TCROWN,                  // 1743
S_DINER,                   // 1744
S_FIRSTAIDKIT,                 // 1745
S_DOGFOOD,                 // 1746
S_MEGAHEALTH,                  // 1747
S_AMMOCLIP,                // 1748
S_AMMOBOX,                 // 1749
S_SILVERKEY,                   // 1750
S_GOLDKEY,                 // 1751
S_ROCKETAMMO,                  // 1752
S_ROCKETBOXAMMO,               // 1753
S_FLAMETHROWERAMMOS,               // 1754
S_FLAMETHROWERAMMOL,               // 1755
S_CANDELABRALIT,               // 1756
S_CANDELABRAUNLIT,             // 1757
S_CHANDELIERLIT,               // 1758
S_CHANDELIERUNLIT,             // 1759
S_GREYLIGHTYEL,                // 1760
S_GREYLIGHTWHT,                // 1761
S_GREYLIGHTUNLIT,              // 1762
S_REDLIGHTLIT,                 // 1763
S_REDLIGHTUNLIT,               // 1764
S_BARRELGREEN,                 // 1765
S_BARRELWOOD,                  // 1766
S_BED,                     // 1767
S_BOILER,                  // 1768
S_CAGEBLOODY,                  // 1769
S_CAGEEMPTY,                   // 1770
S_CAGESITTINGSKEL,             // 1771
S_CAGESKULL,                   // 1772
S_COLUMNBEIGE,                 // 1773
S_COLUMNWHITE,                 // 1774
S_FLAG,                    // 1775
S_KNIGHT,                  // 1776
S_LORRY,                   // 1777
S_PLANTFERNB,                  // 1778
S_POTPLANT,                // 1779
S_SKELETONHANG,                // 1780
S_SINK,                    // 1781
S_SPEARRACK,                   // 1782
S_STAKEOFSKULLS,               // 1783
S_STAKESKULL,                  // 1784
S_STATUEANGELI,                // 1785
S_TABLEPOLE,                   // 1786
S_TABLEWOOD1,                  // 1787
S_VASEBLUE,                // 1788
S_WHITEWELLFULL,               // 1789
S_WHITEWELLEMPTY,              // 1790
S_HITERWELL,                   // 1791
S_ARDWOLF1,                // 1792
S_ARDWOLF2,                // 1793
S_BASKET,                  // 1794
S_BONES1,                  // 1795
S_BONES2,                  // 1796
S_BONES3,                  // 1797
S_BONESANDBLOOD,               // 1798
S_BONESPILE,                   // 1799
S_DEADGUARD,                   // 1800
S_PANS1,                   // 1801
S_PANS2,                   // 1802
S_POOLBLOOD,                   // 1803
S_POOLWATER,                   // 1804
S_SKELETON,                // 1805
S_BARRELGREENWATER,            // 1806
S_BARRELWOODWATER,             // 1807
S_CAGESITINGSKELLEGLESS,           // 1808
S_CAGESITINGSKELONELEG,            // 1809
S_CAGEEMPTY2,                  // 1810
S_CAGEEMPTY3,                  // 1810
S_CAGEEMPTY4,                  // 1810
S_CAGEBLOODY2,                 // 1811
S_CAGEBLOODY3,                 // 1812
S_CAGEBLOODYDRIPPING1,             // 1813
S_CAGEBLOODYDRIPPING2,             // 1814
S_CAGEBLOODYDRIPPING3,             // 1815
S_CAGEBLOODYDRIPPING12,            // 1816
S_CAGEBLOODYDRIPPING22,            // 1817
S_CAGEBLOODYDRIPPING32,            // 1818
S_CAGESKULL2,                  // 1819
S_CAGESKULL3,                  // 1820
S_CAGESKULL4,                  // 1821
S_CAGESKULLDRIPPING1,              // 1822
S_CAGESKULLDRIPPING2,              // 1823
S_CAGESKULLDRIPPING3,              // 1824
S_CAGESKULLDRIPPING12,             // 1825
S_CAGESKULLDRIPPING22,             // 1826
S_CAGESKULLDRIPPING32,             // 1827
S_KNIGHT2,                 // 1830
S_KNIGHT3,                 // 1831
S_PANS3,                   // 1832
S_PANS4,                   // 1833
S_HANGINGSKELETON2,            // 1834
S_HANGINGSKELETON3,            // 1835
S_HANGINGSKELETON4,            // 1836
S_HANGINGSKELETON5,            // 1837
S_HANGINGSKELETON6,            // 1838
S_POTPLANT2,                   // 1839
S_POTPLANTPOT,                 // 1840
S_FERNPOT,                 // 1841
S_FERN2,                   // 1842
S_FERNPOT2,                // 1843
S_STAKEOFSKULLS2,              // 1844
S_STAKEOFSKULLS3,              // 1845
S_STAKEOFSKULLS4,              // 1846
S_STAKEOFSKULLS5,              // 1847
S_STAKEOFSKULLS6,              // 1848
S_SINK2,                   // 1849
S_SKELETONONELEG,              // 1850
S_SPEARRACK2,                  // 1851
S_SPEARRACK3,                  // 1852
S_SPEARRACK4,                  // 1853
S_SPEARRACK5,                  // 1854
S_SPEARRACK6,                  // 1855
S_STATUEANGEL1,                // 1856
S_STATUEANGEL2,                // 1857
S_STATUEANGEL3,                // 1858
S_POLETABLECHAIRS1,            // 1859
S_POLETABLECHAIRS2,            // 1860
S_POLETABLECHAIRS3,            // 1861
S_POLETABLECHAIRS4,            // 1862
S_WOODTABLE2,                  // 1863
S_WOODTABLE3,                  // 1864
S_WOODTABLE4,                  // 1865
S_WOODTABLE5,                  // 1866
S_WOODTABLE6,                  // 1867
S_WOODTABLE7,                  // 1868
S_WOODTABLE8,                  // 1869
S_WOODTABLE9,                  // 1870
S_WOODTABLE10,                 // 1871
S_WOODTABLE11,                 // 1872
S_WOODTABLE12,                 // 1873
S_WOODTABLE13,                 // 1874
S_WOODTABLE14,                 // 1875
S_WOODTABLE15,                 // 1876
S_WOODTABLE16,                 // 1877
S_VASEBLUE2,                   // 1878
S_VASEBLUE3,                   // 1879
S_DEADSSGUARD,                 // 1880
S_DEADDOG,                 // 1881
S_BUSH1,                   // 1882
S_TREE1,                   // 1883
S_TREE2,                   // 1884
S_TREE3,                   // 1885
S_TREE4,                   // 1883
S_TREE5,                   // 1884
S_TREE6,                   // 1885
S_OUTSIDEROCK,                 // 1883
S_OUTSIDEROCK2,                // 1884
S_HCHAIN1,                 // 1886
S_CHAINBALL1,                  // 1887
S_CHAINBALL2,                  // 1888
S_CHAINHOOK1,                  // 1889
S_CHAINHOOK2,                  // 1890
S_CHAINHOOK3,                  // 1891
S_CHAINHOOK4,                  // 1892
S_CHAINHOOKBLOODY,             // 1893
S_CHAINBLOODY2,                // 1894
S_LIGHTNING1,                  // 1895
S_LIGHTNING2,                  // 1896
S_LIGHTNING3,                  // 1897
S_LIGHTNING4,                  // 1898
S_WALLTORCH1,                  // 1906
S_WALLTORCH2,                  // 1907
S_WALLTORCH3,                  // 1908
S_WALLTORCH4,                  // 1909
S_WALLTORCH5,                  // 1910
S_WALLTORCH6,                  // 1911
S_WALLTORCH7,                  // 1912
S_WALLTORCH8,                  // 1913
S_WALLTORCHU,                  // 1914
S_WATERPLUME1,                 // 1915
S_WATERPLUME2,                 // 1916
S_WATERPLUME3,                 // 1917
S_WATERPLUME4,                 // 1918
S_BLOODWELL,                   // 1919
S_BLOODWELL2,                  // 1920
S_BLOODWELL3,                  // 1921
S_BLOODWELL4,                  // 1922
S_SYRINGE1,                // 1903
S_SYRINGE2,                // 1904
S_SYRINGE3,                // 1905
S_WOODTABLECHAIR1,             // 1899
S_WOODTABLECHAIR2,             // 1900
S_WOODTABLECHAIR3,             // 1901
S_WOODTABLECHAIR4,             // 1902
S_CAGESKULLFLOORGIBS,              // 1828
S_CAGESKULLFLOORGIBS2,             // 1829
S_ACEVENTURA,                  // 1923
S_BATMAN,                  // 1924
S_DOOMGUY,                 // 1925
S_DUKENUKEM,                   // 1926
S_JOKER,                   // 1927
S_KEENS,                   // 1928
S_RAMBO,                   // 1929
S_RIDDLER,                 // 1930
S_SPIDERMAN,                   // 1931
S_SUPERERMAN,                  // 1932
S_TWOFACE,                 // 1933
S_ZORRO,                   // 1934
  S_LGUARD_STND,               // 1935
  S_LGUARD_RUN1,               // 1936
  S_LGUARD_RUN2,               // 1937
  S_LGUARD_RUN3,               // 1938
  S_LGUARD_RUN4,               // 1939
  S_LGUARD_RUN5,               // 1940
  S_LGUARD_RUN6,               // 1941
  S_LGUARD_RUN7,               // 1942
  S_LGUARD_RUN8,               // 1943
  S_LGUARD_ATK1,               // 1944
  S_LGUARD_ATK2,               // 1945
  S_LGUARD_ATK3,               // 1946
  S_LGUARD_ATK4,               // 1947
  S_LGUARD_PAIN1,              // 1948
  S_LGUARD_PAIN2,              // 1949
  S_LGUARD_DIE1,               // 1950
  S_LGUARD_DIE2,               // 1951
  S_LGUARD_DIE3,               // 1952
  S_LGUARD_DIE4,               // 1953
  S_LGUARD_DIE5,               // 1954
  S_LGUARD_XDIE1,              // 1950
  S_LGUARD_XDIE2,              // 1951
  S_LGUARD_XDIE3,              // 1952
  S_LGUARD_XDIE4,              // 1953
  S_LGUARD_XDIE5,              // 1954
  S_LGUARD_XDIE6,              // 1950
  S_LGUARD_XDIE7,              // 1951
  S_LGUARD_XDIE8,              // 1952
  S_LGUARD_XDIE9,              // 1953
  S_LSS_STND,                  // 1955
  S_LSS_RUN1,                  // 1956
  S_LSS_RUN2,                  // 1957
  S_LSS_RUN3,                  // 1958
  S_LSS_RUN4,                  // 1959
  S_LSS_RUN5,                  // 1960
  S_LSS_RUN6,                  // 1961
  S_LSS_RUN7,                  // 1962
  S_LSS_RUN8,                  // 1963
  S_LSS_ATK1,                  // 1964
  S_LSS_ATK2,                  // 1965
  S_LSS_ATK3,                  // 1966
  S_LSS_ATK4,                  // 1967
  S_LSS_ATK5,
  S_LSS_ATK6,
  S_LSS_ATK7,
  S_LSS_ATK8,
  S_LSS_ATK9,
  S_LSS_ATK10,
  S_LSS_PAIN1,                 // 1968
  S_LSS_PAIN2,                 // 1969
  S_LSS_DIE1,                  // 1970
  S_LSS_DIE2,                  // 1971
  S_LSS_DIE3,                  // 1972
  S_LSS_DIE4,                  // 1973
  S_LSS_DIE5,                  // 1974
  S_LSS_XDIE1,                 // 1970
  S_LSS_XDIE2,                 // 1971
  S_LSS_XDIE3,                 // 1972
  S_LSS_XDIE4,                 // 1973
  S_LSS_XDIE5,                 // 1974
  S_LSS_XDIE6,                 // 1970
  S_LSS_XDIE7,                 // 1971
  S_LSS_XDIE8,                 // 1972
  S_LSS_XDIE9,                 // 1973
  S_LDOG_STND1,                // 1975
  S_LDOG_STND2,                // 1976
  S_LDOG_STND3,                // 1975
  S_LDOG_STND4,                // 1976
  S_LDOG_RUN1,                 // 1977
  S_LDOG_RUN2,                 // 1978
  S_LDOG_RUN3,                 // 1979
  S_LDOG_RUN4,                 // 1980
  S_LDOG_RUN5,                 // 1981
  S_LDOG_RUN6,                 // 1982
  S_LDOG_RUN7,                 // 1983
  S_LDOG_RUN8,                 // 1984
  S_LDOG_ATK1,                 // 1985
  S_LDOG_ATK2,                 // 1986
  S_LDOG_ATK3,                 // 1987
  S_LDOG_DIE1,                 // 1990
  S_LDOG_DIE2,                 // 1991
  S_LDOG_DIE3,                 // 1992
  S_LDOG_DIE4,                 // 1993
  S_LDOG_XDIE1,                // 1990
  S_LDOG_XDIE2,                // 1991
  S_LDOG_XDIE3,                // 1992
  S_LDOG_XDIE4,                // 1993
  S_LBAT_STND,                 // 1994
  S_LBAT_RUN1,                 // 1995
  S_LBAT_RUN2,                 // 1996
  S_LBAT_RUN3,                 // 1997
  S_LBAT_RUN4,                 // 1998
  S_LBAT_RUN5,                 // 1999
  S_LBAT_RUN6,                 // 2000
  S_LBAT_RUN7,                 // 2001
  S_LBAT_RUN8,                 // 2002
  S_LBAT_ATK1,                 // 2003
  S_LBAT_ATK2,                 // 2004
  S_LBAT_ATK3,                 // 2005
  S_LBAT_ATK4,                 // 2006
  S_LBAT_ATK5,                 // 2007
  S_LBAT_ATK6,                 // 2008
  S_LBAT_ATK7,                 // 2009
  S_LBAT_ATK8,                 // 2010
  S_LBAT_PAIN1,                // 2011
  S_LBAT_PAIN2,                // 2012
  S_LBAT_DIE1,                 // 2013
  S_LBAT_DIE2,                 // 2014
  S_LBAT_DIE3,                 // 2015
  S_LBAT_DIE4,                 // 2016
  S_LBAT_DIE5,                 // 2017
  S_LBAT_DIE6,                 // 2018
  S_LBAT_DIE7,                 // 2019
  S_LBAT_XDIE1,                // 2013
  S_LBAT_XDIE2,                // 2014
  S_LBAT_XDIE3,                // 2015
  S_LBAT_XDIE4,                // 2016
  S_LBAT_XDIE5,                // 2017
  S_LBAT_XDIE6,                // 2018
  S_LOFFI_STND,                // 2020
  S_LOFFI_RUN1,                // 2021
  S_LOFFI_RUN2,                // 2022
  S_LOFFI_RUN3,                // 2023
  S_LOFFI_RUN4,                // 2024
  S_LOFFI_RUN5,                // 2025
  S_LOFFI_RUN6,                // 2026
  S_LOFFI_RUN7,                // 2027
  S_LOFFI_RUN8,                // 2028
  S_LOFFI_ATK1,                // 2029
  S_LOFFI_ATK2,                // 2030
  S_LOFFI_ATK3,                // 2031
  S_LOFFI_ATK4,                // 2032
  S_LOFFI_PAIN1,               // 2033
  S_LOFFI_PAIN2,               // 2034
  S_LOFFI_DIE1,                // 2035
  S_LOFFI_DIE2,                // 2036
  S_LOFFI_DIE3,                // 2037
  S_LOFFI_DIE4,                // 2038
  S_LOFFI_DIE5,                // 2039
  S_LOFFI_DIE6,                // 2040
  S_LOFFI_XDIE1,               // 2035
  S_LOFFI_XDIE2,               // 2036
  S_LOFFI_XDIE3,               // 2037
  S_LOFFI_XDIE4,               // 2038
  S_LOFFI_XDIE5,               // 2039
  S_LOFFI_XDIE6,               // 2040
  S_LOFFI_XDIE7,               // 2038
  S_LOFFI_XDIE8,               // 2039
  S_LOFFI_XDIE9,               // 2040
  S_WILLY_STND,                // 2041
  S_WILLY_RUN1,                // 2042
  S_WILLY_RUN2,                // 2043
  S_WILLY_RUN3,                // 2044
  S_WILLY_RUN4,                // 2045
  S_WILLY_RUN5,                // 2046
  S_WILLY_RUN6,                // 2047
  S_WILLY_RUN7,                // 2048
  S_WILLY_RUN8,                // 2049
  S_WILLY_ATK1,                // 2050
  S_WILLY_ATK2,                // 2051
  S_WILLY_ATK3,                // 2052
  S_WILLY_ATK4,                // 2053
  S_WILLY_ATK5,                // 2054
  S_WILLY_ATK6,                // 2055
  S_WILLY_DIE1,                // 2056
  S_WILLY_DIE2,                // 2057
  S_WILLY_DIE3,                // 2058
  S_WILLY_DIE4,                // 2059
  S_WILLY_DIE5,                // 2060
  S_QUARK_STND,                // 2061
  S_QUARK_RUN1,                // 2062
  S_QUARK_RUN2,                // 2063
  S_QUARK_RUN3,                // 2064
  S_QUARK_RUN4,                // 2065
  S_QUARK_RUN5,                // 2066
  S_QUARK_RUN6,                // 2067
  S_QUARK_RUN7,                // 2068

  S_QUARK_RUN8,                // 2069
  S_QUARK_ATK,                 // 2070
  S_QUARK_ATK1_1,              // 2071
  S_QUARK_ATK1_2,              // 2072
  S_QUARK_ATK1_3,              // 2073
  S_QUARK_ATK2_1,              // 2074
  S_QUARK_ATK2_2,              // 2075
  S_QUARK_ATK2_3,              // 2076
  S_QUARK_ATK2_4,              // 2077
  S_QUARK_ATK2_5,              // 2078
  S_QUARK_DIE1,                // 2079
  S_QUARK_DIE2,                // 2080
  S_QUARK_DIE3,                // 2081
  S_QUARK_DIE4,                // 2082
  S_QUARK_DIE5,                // 2083
  S_AXE_STND,                  // 2084
  S_AXE_RUN1,                  // 2085
  S_AXE_RUN2,                  // 2086
  S_AXE_RUN3,                  // 2087
  S_AXE_RUN4,                  // 2088
  S_AXE_RUN5,                  // 2089
  S_AXE_RUN6,                  // 2090
  S_AXE_RUN7,                  // 2091
  S_AXE_RUN8,                  // 2092
  S_AXE_ATK1,                  // 2093
  S_AXE_ATK2,                  // 2094
  S_AXE_ATK3,                  // 2095
  S_AXE_ATK4,                  // 2096
  S_AXE_ATK5,                  // 2097
  S_AXE_DIE1,                  // 2098
  S_AXE_DIE2,                  // 2099
  S_AXE_DIE3,                  // 2100
  S_AXE_DIE4,                  // 2101
  S_AXE_DIE5,                  // 2102
  S_ROBOT_STND,                // 2103
  S_ROBOT_RUN1,                // 2104
  S_ROBOT_RUN2,                // 2105
  S_ROBOT_RUN3,                // 2106
  S_ROBOT_RUN4,                // 2107
  S_ROBOT_RUN5,                // 2108
  S_ROBOT_RUN6,                // 2109
  S_ROBOT_RUN7,                // 2110
  S_ROBOT_RUN8,                // 2111
  S_ROBOT_ATK,                 // 2212
  S_ROBOT_ATK1_1,              // 2212
  S_ROBOT_ATK1_2,              // 2213
  S_ROBOT_ATK1_3,              // 2214
  S_ROBOT_ATK1_4,              // 2215
  S_ROBOT_ATK1_5,              // 2216
  S_ROBOT_ATK1_6,              // 2212
  S_ROBOT_ATK1_7,              // 2213
  S_ROBOT_ATK1_8,              // 2214
  S_ROBOT_ATK1_9,              // 2215
  S_ROBOT_ATK1_10,             // 2216
  S_ROBOT_ATK1_11,             // 2212
  S_ROBOT_ATK1_12,             // 2213
  S_ROBOT_ATK1_13,             // 2214
  S_ROBOT_ATK1_14,             // 2215
  S_ROBOT_ATK1_15,             // 2216
  S_ROBOT_ATK1_16,             // 2214
  S_ROBOT_ATK1_17,             // 2215
  S_ROBOT_ATK1_18,             // 2216
  S_ROBOT_ATK2_1,              // 2212
  S_ROBOT_ATK2_2,              // 2213
  S_ROBOT_ATK2_3,              // 2214
  S_ROBOT_ATK2_4,              // 2215
  S_ROBOT_ATK2_5,              // 2216
  S_ROBOT_ATK2_6,              // 2216
  S_ROBOT_ATK2_7,              // 2214
  S_ROBOT_ATK2_8,              // 2215
  S_ROBOT_ATK2_9,              // 2216
  S_ROBOT_ATK2_10,             // 2216
  S_ROBOT_DIE1,                // 2217
  S_ROBOT_DIE2,                // 2218
  S_ROBOT_DIE3,                // 2219
  S_ROBOT_DIE4,                // 2220
  S_ROBOT_DIE5,                // 2221
  S_ROBOT_DIE6,                // 2222
  S_ROBOT_DIE7,                // 2223
  S_ROBOT_DIE8,                // 2224
  S_ROBOT_DIE9,                // 2225
  S_ROBOT_DIE10,               // 2226
  S_ROBOT_DIE11,               // 2227
  S_LGBS_STND1,                // 2228
  S_LGBS_STND2,                // 2229
  S_LGBS_STND3,                // 2230
  S_LGBS_STND4,                // 2231
  S_LGBS_RUN1,                 // 2232
  S_LGBS_RUN2,                 // 2233
  S_LGBS_RUN3,                 // 2234
  S_LGBS_RUN4,                 // 2235
  S_LGBS_ATK1,                 // 2236
  S_LGBS_ATK2,                 // 2237
  S_LGBS_ATK3,                 // 2236
  S_LGBS_ATK4,                 // 2237
  S_LGBS_DIE1,                 // 2240
  S_LGBS_DIE2,                 // 2241
  S_LGBS_DIE3,                 // 2242
  S_LGBS_DIE4,                 // 2243
  S_LGBS_DIE5,                 // 2244
  S_LGRM_STND1,                // 2245
  S_LGRM_STND2,                // 2246
  S_LGRM_STND3,                // 2247
  S_LGRM_STND4,                // 2248
  S_LGRM_RUN1,                 // 2249
  S_LGRM_RUN2,                 // 2250
  S_LGRM_RUN3,                 // 2251
  S_LGRM_RUN4,                 // 2252
  S_LGRM_ATK1,                 // 2253
  S_LGRM_ATK2,                 // 2254
  S_LGRM_ATK3,                 // 2253
  S_LGRM_ATK4,                 // 2254
  S_LGRM_DIE1,                 // 2255
  S_LGRM_DIE2,                 // 2256
  S_LGRM_DIE3,                 // 2257
  S_LGRM_DIE4,                 // 2258
  S_DEVIL_STND1,               // 2259
  S_DEVIL_STND2,               // 2260
  S_DEVIL_RUN1,                // 2261
  S_DEVIL_RUN2,                // 2262
  S_DEVIL_RUN3,                // 2263
  S_DEVIL_RUN4,                // 2264
  S_DEVIL_RUN5,                // 2265
  S_DEVIL_RUN6,                // 2266
  S_DEVIL_RUN7,                // 2267
  S_DEVIL_RUN8,                // 2268
  S_DEVIL_ATK,                 // 2269
  S_DEVIL_ATK1_1,              // 2270
  S_DEVIL_ATK1_2,              // 2271
  S_DEVIL_ATK1_3,              // 2272
  S_DEVIL_ATK1_4,              // 2273
  S_DEVIL_ATK1_5,              // 2273
  S_DEVIL_ATK1_6,              // 2274
  S_DEVIL_ATK2_1,              // 2275
  S_DEVIL_ATK2_2,              // 2276
  S_DEVIL_ATK2_3,              // 2277
  S_DEVIL_ATK2_4,              // 2278
  S_DEVIL_ATK2_5,              // 2279
  S_DEVIL_ATK2_6,              // 2280
  S_DEVIL_ATK2_7,              // 2281
  S_DEVIL_ATK3_1,              // 2282
  S_DEVIL_ATK3_2,              // 2283
  S_DEVIL_ATK3_3,              // 2284
  S_DEVIL_DIE1,                // 2285
  S_DEVIL_DIE2,                // 2286
  S_DEVIL_DIE3,                // 2287
  S_DEVIL_DIE4,                // 2288
  S_DEVIL_DIE5,                // 2289
  S_DEVIL_DIE6,                // 2290
  S_DEVIL_DIE7,                // 2291
  S_DEVIL_DIE8,                // 2292
  S_DEVIL_DIE9,                // 2293
S_DEVILFIRE1,                      // 281
S_DEVILFIRE2,                      // 282
S_DEVILFIRE3,                      // 283
S_DEVILFIRE4,                      // 284
S_DEVILFIRE5,                      // 285
S_DEVILFIRE6,                      // 286
S_DEVILFIRE7,                      // 287
S_DEVILFIRE8,                      // 288
S_DEVILFIRE9,                      // 289
S_DEVILFIRE10,                     // 290
S_DEVILFIRE11,                     // 291
S_DEVILFIRE12,                     // 292
S_DEVILFIRE13,                     // 293
S_DEVILFIRE14,                     // 294
S_DEVILFIRE15,                     // 295
S_DEVILFIRE16,                     // 296
S_DEVILFIRE17,                     // 297
S_DEVILFIRE18,                     // 298
S_DEVILFIRE19,                     // 299
S_DEVILFIRE20,                     // 300
S_DEVILFIRE21,                     // 301
S_DEVILFIRE22,                     // 302
S_DEVILFIRE23,                     // 303
S_DEVILFIRE24,                     // 304
S_DEVILFIRE25,                     // 305
S_DEVILFIRE26,                     // 306
S_DEVILFIRE27,                     // 307
S_DEVILFIRE28,                     // 308
S_DEVILFIRE29,                     // 309
S_DEVILFIRE30,                     // 310
  S_LRGBS_LOOK,                // 2294
  S_LRGBS_RAISE1,              // 2295
  S_LRGBS_RAISE2,              // 2296
  S_LRGBS_RAISE3,              // 2297
  S_LRGBS_RAISE4,              // 2298
  S_LRGBS_ATK1,                // 2236
  S_LRGBS_ATK2,                // 2237
  S_LRGBS_ATK3,                // 2236
  S_LRGBS_ATK4,                // 2237
  S_LRGRM_STND1,               // 2299
  S_LRGRM_STND2,               // 2300
  S_LRGRM_STND3,               // 2301
  S_LRGRM_STND4,               // 2302
  S_LRGRM_RUN1,                // 2303
  S_LRGRM_RUN2,                // 2304
  S_LRGRM_RUN3,                // 2305
  S_LRGRM_RUN4,                // 2306
  S_LRGRM_ATK1,                // 2307
  S_LRGRM_ATK2,                // 2308
  S_LRGRM_ATK3,                // 2307
  S_LRGRM_ATK4,                // 2308
  S_LRGRM_DIE1,                // 2309
  S_LRGRM_DIE2,                // 2310
  S_LRGRM_DIE3,                // 2311
  S_LRGRM_DIE4,                // 2312
  S_LRGRM_DIE5,                // 2313
  S_LRGRM_DIE6,                // 2314
  S_SPIRIT_STND1,              // 2315
  S_SPIRIT_STND2,              // 2316
  S_SPIRIT_RUN1,               // 2317
  S_SPIRIT_RUN2,               // 2318
  S_SPIRIT_RUN3,               // 2319
  S_SPIRIT_RUN4,               // 2320
  S_SPIRIT_RUN5,               // 2321
  S_SPIRIT_RUN6,               // 2322
  S_SPIRIT_RUN7,               // 2323
  S_SPIRIT_RUN8,               // 2324
  S_SPIRIT_ATK1,               // 2325
  S_SPIRIT_ATK2,               // 2326
  S_SPIRIT_DIE1,               // 2327
  S_SPIRIT_DIE2,               // 2328
  S_SPIRIT_DIE3,               // 2329
  S_SPIRIT_DIE4,               // 2330
  S_SPIRIT_DIE5,               // 2331
  S_SPIRIT_DIE6,               // 2332
  S_LMBAT_ATK1,
  S_LMBAT_ATK2,
  S_LMBAT_ATK3,
  S_LMBAT_ATK4,
  S_LMBAT_ATK5,
  S_LMBAT_DIE1,
  S_LMBAT_XDIE1,
  S_LFLAMEGUARD_STND,              // 2360
  S_LFLAMEGUARD_RUN1,              // 2361
  S_LFLAMEGUARD_RUN2,              // 2362
  S_LFLAMEGUARD_RUN3,              // 2363
  S_LFLAMEGUARD_RUN4,              // 2364
  S_LFLAMEGUARD_RUN5,              // 2365
  S_LFLAMEGUARD_RUN6,              // 2366
  S_LFLAMEGUARD_RUN7,              // 2367
  S_LFLAMEGUARD_RUN8,              // 2368
  S_LFLAMEGUARD_ATK1,              // 2369
  S_LFLAMEGUARD_ATK2,              // 2370
  S_LFLAMEGUARD_ATK3,              // 2371
  S_LFLAMEGUARD_ATK4,              // 2372
  S_LFLAMEGUARD_ATK5,              // 2373
  S_LFLAMEGUARD_ATK6,              // 2374
  S_LFLAMEGUARD_ATK7,              // 2375
  S_LFLAMEGUARD_ATK8,              // 2376
  S_LFLAMEGUARD_ATK9,              // 2377
  S_LFLAMEGUARD_PAIN1,             // 2378
  S_LFLAMEGUARD_PAIN2,             // 2379
  S_LFLAMEGUARD_DIE1,              // 2380
  S_LFLAMEGUARD_DIE2,              // 2381
  S_LFLAMEGUARD_DIE3,              // 2382
  S_LFLAMEGUARD_DIE4,              // 2383
  S_LFLAMEGUARD_DIE5,              // 2384
  S_LFLAMEGUARD_XDIE1,             // 2380
  S_LFLAMEGUARD_XDIE2,             // 2381
  S_LFLAMEGUARD_XDIE3,             // 2382
  S_LFLAMEGUARD_XDIE4,             // 2383
  S_LFLAMEGUARD_XDIE5,             // 2384
  S_LFLAMEGUARD_XDIE6,             // 2381
  S_LFLAMEGUARD_XDIE7,             // 2382
  S_LFLAMEGUARD_XDIE8,             // 2383
  S_LFLAMEGUARD_XDIE9,             // 2384
  S_LELITEGUARD_STND,              // 2385
  S_LELITEGUARD_RUN1,              // 2386
  S_LELITEGUARD_RUN2,              // 2387
  S_LELITEGUARD_RUN3,              // 2388
  S_LELITEGUARD_RUN4,              // 2389
  S_LELITEGUARD_RUN5,              // 2390
  S_LELITEGUARD_RUN6,              // 2391
  S_LELITEGUARD_RUN7,              // 2392
  S_LELITEGUARD_RUN8,              // 2393
  S_LELITEGUARD_ATK1,              // 2394
  S_LELITEGUARD_ATK2,              // 2395
  S_LELITEGUARD_ATK3,              // 2396
  S_LELITEGUARD_ATK4,              // 2397
  S_LELITEGUARD_PAIN1,             // 2398
  S_LELITEGUARD_PAIN2,             // 2399
  S_LELITEGUARD_DIE1,              // 2400
  S_LELITEGUARD_DIE2,              // 2401
  S_LELITEGUARD_DIE3,              // 2402
  S_LELITEGUARD_DIE4,              // 2403
  S_LELITEGUARD_DIE5,              // 2404
  S_LELITEGUARDCOM_STND,           // 2405
  S_LELITEGUARDCOM_RUN1,           // 2406
  S_LELITEGUARDCOM_RUN2,           // 2407
  S_LELITEGUARDCOM_RUN3,           // 2408
  S_LELITEGUARDCOM_RUN4,           // 2409
  S_LELITEGUARDCOM_RUN5,           // 2410
  S_LELITEGUARDCOM_RUN6,           // 2411
  S_LELITEGUARDCOM_RUN7,           // 2412
  S_LELITEGUARDCOM_RUN8,           // 2413
  S_LELITEGUARDCOM_ATK1,           // 2414
  S_LELITEGUARDCOM_ATK2,           // 2415
  S_LELITEGUARDCOM_ATK3,           // 2416
  S_LELITEGUARDCOM_ATK4,           // 2417
  S_LELITEGUARDCOM_ATK5,           // 2418
  S_LELITEGUARDCOM_ATK6,           // 2419
  S_LELITEGUARDCOM_ATK7,           // 2420
  S_LELITEGUARDCOM_ATK8,           // 2421
  S_LELITEGUARDCOM_ATK9,           // 2422
  S_LELITEGUARDCOM_PAIN1,          // 2423
  S_LELITEGUARDCOM_PAIN2,          // 2424
  S_LELITEGUARDCOM_DIE1,           // 2425
  S_LELITEGUARDCOM_DIE2,           // 2426
  S_LELITEGUARDCOM_DIE3,           // 2427
  S_LELITEGUARDCOM_DIE4,           // 2428
  S_LELITEGUARDCOM_DIE5,           // 2429
  S_LELITEGUARDCOM_XDIE1,          // 2425
  S_LELITEGUARDCOM_XDIE2,          // 2426
  S_LELITEGUARDCOM_XDIE3,          // 2427
  S_LELITEGUARDCOM_XDIE4,          // 2428
  S_LELITEGUARDCOM_XDIE5,          // 2429
  S_LELITEGUARDCOM_XDIE6,          // 2425
  S_LELITEGUARDCOM_XDIE7,          // 2426
  S_LELITEGUARDCOM_XDIE8,          // 2427
  S_LELITEGUARDCOM_XDIE9,          // 2428
  S_SECROBOT_STND,             // 2430
  S_SECROBOT_RUN1,             // 2431
  S_SECROBOT_RUN2,             // 2432
  S_SECROBOT_RUN3,             // 2433
  S_SECROBOT_RUN4,             // 2434
  S_SECROBOT_RUN5,             // 2435
  S_SECROBOT_RUN6,             // 2436
  S_SECROBOT_RUN7,             // 2437
  S_SECROBOT_RUN8,             // 2438
  S_SECROBOT_ATK1,             // 2439
  S_SECROBOT_ATK2,             // 2440
  S_SECROBOT_ATK3,             // 2441
  S_SECROBOT_ATK4,             // 2442
  S_SECROBOT_ATK5,             // 2443
  S_SECROBOT_ATK6,             // 2444
  S_SECROBOT_ATK7,             // 2445
  S_SECROBOT_DIE1,             // 2446
  S_SECROBOT_DIE2,             // 2447
  S_SECROBOT_DIE3,             // 2448
  S_SECROBOT_DIE4,             // 2449
  S_SECROBOT_DIE5,             // 2450
  S_LUTH_STND,                 // 2451
  S_LUTH_RUN1,                 // 2452
  S_LUTH_RUN2,                 // 2453
  S_LUTH_RUN3,                 // 2454
  S_LUTH_RUN4,                 // 2455
  S_LUTH_RUN5,                 // 2456
  S_LUTH_RUN6,                 // 2457
  S_LUTH_RUN7,                 // 2458
  S_LUTH_RUN8,                 // 2459
  S_LUTH_ATK1,                 // 2460
  S_LUTH_ATK2,                 // 2461
  S_LUTH_ATK3,                 // 2462
  S_LUTH_ATK4,                 // 2463
  S_LUTH_ATK5,                 // 2464
  S_LUTH_ATK6,                 // 2465
  S_LUTH_DIE1,                 // 2466
  S_LUTH_DIE2,                 // 2467
  S_LUTH_DIE3,                 // 2468
  S_LUTH_DIE4,                 // 2469
  S_LUTH_DIE5,                 // 2470
  S_ASSASIN_STND,              // 2471
  S_ASSASIN_RUN1,              // 2472
  S_ASSASIN_RUN2,              // 2473
  S_ASSASIN_RUN3,              // 2474
  S_ASSASIN_RUN4,              // 2475
  S_ASSASIN_RUN5,              // 2476
  S_ASSASIN_RUN6,              // 2477
  S_ASSASIN_RUN7,              // 2478
  S_ASSASIN_RUN8,              // 2479
  S_ASSASIN_ATK1,              // 2480
  S_ASSASIN_ATK2,              // 2481
  S_ASSASIN_ATK3,                  // 2482
  S_ASSASIN_ATK4,              // 2483
  S_ASSASIN_ATK5,              // 2484
  S_ASSASIN_ATK6,              // 2485
  S_ASSASIN_DIE1,              // 2486
  S_ASSASIN_DIE2,              // 2487
  S_ASSASIN_DIE3,              // 2488
  S_ASSASIN_DIE4,              // 2489
  S_ASSASIN_DIE5,              // 2490
  S_EVILBJ_STND,               // 2491
  S_EVILBJ_RUN1,               // 2492
  S_EVILBJ_RUN2,               // 2493
  S_EVILBJ_RUN3,               // 2494
  S_EVILBJ_RUN4,               // 2495
  S_EVILBJ_RUN5,               // 2496
  S_EVILBJ_RUN6,               // 2497
  S_EVILBJ_RUN7,               // 2498
  S_EVILBJ_RUN8,               // 2499
  S_EVILBJ_ATK1,               // 2500
  S_EVILBJ_ATK2,               // 2501
  S_EVILBJ_ATK3,               // 2502
  S_EVILBJ_ATK4,               // 2503
  S_EVILBJ_PAIN1,              // 2504
  S_EVILBJ_PAIN2,              // 2505
  S_EVILBJ_DIE1,               // 2506
  S_EVILBJ_DIE2,               // 2507
  S_EVILBJ_DIE3,               // 2508
  S_EVILBJ_DIE4,               // 2509
  S_EVILBJ_DIE5,               // 2510
  S_DEVIL2_STND1,              // 2511
  S_DEVIL2_STND2,              // 2512
  S_DEVIL2_RUN1,               // 2513
  S_DEVIL2_RUN2,               // 2514
  S_DEVIL2_RUN3,               // 2515
  S_DEVIL2_RUN4,               // 2516
  S_DEVIL2_RUN5,               // 2517
  S_DEVIL2_RUN6,               // 2518
  S_DEVIL2_RUN7,               // 2519
  S_DEVIL2_RUN8,               // 2520
  S_DEVIL2_ATK,                // 2521
  S_DEVIL2_ATK1_1,             // 2522
  S_DEVIL2_ATK1_2,             // 2523
  S_DEVIL2_ATK1_3,             // 2524
  S_DEVIL2_ATK1_4,             // 2525
  S_DEVIL2_ATK1_5,             // 2526
  S_DEVIL2_ATK1_6,             // 2527
  S_DEVIL2_ATK2_1,             // 2528
  S_DEVIL2_ATK2_2,             // 2529
  S_DEVIL2_ATK2_3,             // 2530
  S_DEVIL2_ATK2_4,             // 2531
  S_DEVIL2_ATK2_5,             // 2532
  S_DEVIL2_ATK2_6,             // 2533
  S_DEVIL2_ATK2_7,             // 2534
  S_DEVIL2_ATK3_1,             // 2535
  S_DEVIL2_ATK3_2,             // 2536
  S_DEVIL2_ATK3_3,             // 2537
  S_DEVIL2_DIE1,               // 2538
  S_DEVIL2_DIE2,               // 2539
  S_DEVIL2_DIE3,               // 2540
  S_DEVIL2_DIE4,               // 2541
  S_DEVIL2_DIE5,               // 2542
  S_DEVIL2_DIE6,               // 2543
  S_DEVIL2_DIE7,               // 2544
  S_DEVIL2_DIE8,               // 2545
  S_DEVIL2_DIE9,               // 2546
  S_SECROBOTS_DIE1,            // 2547
  S_LSPEAR1,                   // 2548
  S_LSPEAR2,                   // 2549
  S_LTRADIO,                   // 2550
  S_LTPLUTONIUM,               // 2551
  S_LTTIMER,                   // 2552
  S_LTBOMB,                // 2553
  S_LDINER,                // 2554
  S_LMEDI,                 // 2555
  S_LDOGFOOD,                  // 2556
  S_LMEGA,                 // 2557
  S_LCLIP,                 // 2558
  S_LAMMO,                 // 2559
  S_LSILVERKEY,                // 2560
  S_LGOLDKEY,                  // 2561
  S_LROCKETAMMO,
  S_LROCKETBOXAMMO,
  S_LFLAMETHROWERAMMOS,
  S_LFLAMETHROWERAMMOL,
  S_LGREENLIGHTLIT,            // 2562
  S_LGREENLIGHTUNLIT,              // 2563
  S_LREDLIGHTLIT,              // 2564
  S_LREDLIGHTUNLIT,            // 2565
  S_LBULBLIT,                  // 2566
  S_LBULBUNLIT,                // 2567
  S_LCHANDELIERLIT,            // 2568
  S_LCHANDELIERUNLIT,              // 2569
  S_LCANDELABRALIT,            // 2570
  S_LCANDELABRAUNLIT,              // 2571
  S_LTABLESILVER,              // 2572
  S_LTABLESTEEL,               // 2573
  S_LSTONECOLUMN,              // 2574
  S_LPIPECOULMN,               // 2575
  S_LELECTROCOLUMN1,               // 2576
  S_LELECTROCOLUMN2,               // 2577
  S_LHORIZBROWNBARREL,             // 2578
  S_LHORIZGREENBARREL,             // 2579
  S_LHORIZYELLOWBARREL,            // 2580
  S_LWELLFULL,                 // 2581
  S_LWELLEMPTY,                // 2582
  S_LPLAMTREE,                 // 2583
  S_LFERN,                 // 2584
  S_LBLUEVASE,                 // 2585
  S_LARMOUREDSKELETON,             // 2586
  S_LDEVILSTATUE,              // 2587
  S_LHANGINGSKELETON,              // 2588
  S_LEMPTYCAGE,                // 2589
  S_LBROKENCAGE,               // 2590
  S_LSKELETONCAGE,             // 2591
  S_LBLOODYCAGE,               // 2592
  S_LCONICAL1,                 // 2593
  S_LCONICAL2,                 // 2594
  S_LCONICAL3,                 // 2595
  S_LBJSIGN,                   // 2596
  S_LBUBBLES,                  // 2597
  S_LBONES,                // 2598
  S_LSKELETON,                 // 2599
  S_LPILESKULLS,               // 2600
  S_LBLOODBONES,               // 2601
  S_LBONESWATER,               // 2602
  S_LWATERPOOL,                // 2603
  S_LBLOODPOOL,                // 2604
  S_LSLIMEPOOL,                // 2605
  S_LDEADRAT,                  // 2606
  S_LDEADGUARD,                // 2607
  S_LTABLESTEEL2,              // 2608
  S_LTABLESTEEL3,              // 2609
  S_LTABLESTEEL4,              // 2610
  S_LTABLESTEEL5,              // 2611
  S_LTABLESTEEL6,              // 2612
  S_LTABLESTEEL7,              // 2613
  S_LTABLESTEEL8,              // 2614
  S_LTABLESTEEL9,              // 2615
  S_LTABLESTEEL10,             // 2616
  S_LTABLESTEEL11,             // 2617
  S_LTABLESTEEL12,             // 2618
  S_LTABLESTEEL13,             // 2619
  S_LTABLESTEEL14,             // 2620
  S_LTABLESTEEL15,             // 2621
  S_LTABLESTEEL16,             // 2622
  S_LPIPECOULMN2,              // 2623
  S_LPIPECOULMN3,              // 2624
  S_LPIPECOULMN4,              // 2625
  S_LHORIZBROWNBARREL2,            // 2626
  S_LHORIZBROWNBARRELWIDE,         // 2627
  S_LHORIZGREENBARRELWIDE,         // 2628
  S_LPLAMTREE2,                // 2629
  S_LPLAMTREE3,                // 2630
  S_LPLAMTREE4,                // 2631
  S_LPLAMTREE5,                // 2632
  S_LFERNPOT,                  // 2633
  S_LHANGINGSKELETON2,             // 2634
  S_LHANGINGSKELETON3,             // 2635
  S_LHANGINGSKELETON4,             // 2636
  S_LHANGINGSKELETON5,             // 2637
  S_LHANGINGSKELETON6,             // 2638
  S_LEMPTYCAGE2,               // 2639
  S_LBROKENCAGEBLOOD,              // 2640
  S_LBROKENCAGEBLOOD2,             // 2641
  S_LBROKENCAGEBLOOD3,             // 2642

  S_LBROKENCAGETOP,            // 2643
  S_LSKELETONCAGEDRIPPING1,        // 2644
  S_LSKELETONCAGEDRIPPING2,        // 2645
  S_LSKELETONCAGEDRIPPING3,        // 2646
  S_LBLOODYCAGEDRIPPING1,          // 2647
  S_LBLOODYCAGEDRIPPING2,          // 2648
  S_LBLOODYCAGEDRIPPING3,          // 2649
  S_LCONICAL21,                // 2651
  S_LCONICAL22,                // 2652
  S_LCONICAL23,                // 2653
  S_LCONICAL31,                // 2651
  S_LCONICAL32,                // 2652
  S_LCONICAL33,                // 2653
  S_LTABLESTEELCHAIR,              // 2654
  S_LTABLESTEELCHAIR2,             // 2655
  S_LSTALACTITEICEMEDIUM,          // 2656
  S_LSTALACTITEICESMALL,           // 2657
  S_LSTALACTITEICETINY,            // 2658
  S_LSTALAGMITEICEMEDIUM,          // 2659
  S_LSTALAGMITEICESMALL,           // 2660
  S_LSTALAGMITEICETINY,            // 2661
  S_LBLOODYCAGEFLOORGIBS,          // 2650
  S_AGUARD_DIE1,                   // 2662
  S_AGUARD_XDIE1,                  // 2662
  S_ACLIP,                 // 2663
  S_ACROSS,                // 2664
  S_ACUP,                  // 2665
  S_ATREASUREPILE,             // 2666
  S_ACROWN,                // 2667
  S_ALPHACHANDELIERU,              // 2668
  S_ALPHACHANDELIERL,              // 2669
  S_ALPHAVASE,                 // 2670
  S_ALPHACOULMN,               // 2671
  S_CBAT_STND1,
  S_CBAT_STND2,
  S_CBAT_STND3,
  S_CBAT_STND4,
  S_CBAT_RUN1,
  S_CBAT_RUN2,
  S_CBAT_RUN3,
  S_CBAT_RUN4,
  S_CBAT_ATK1,
  S_CBAT_ATK2,
  S_CBAT_ATK3,
  S_CBAT_ATK4,
  S_CBAT_DIE1,
  S_CBAT_DIE2,
  S_CBAT_DIE3,
  S_CBLACKDEMON_STND1,
  S_CBLACKDEMON_STND2,
  S_CBLACKDEMON_RUN1,
  S_CBLACKDEMON_RUN2,
  S_CBLACKDEMON_ATK1,
  S_CBLACKDEMON_ATK2,
  S_CBLACKDEMON_ATK3,
  S_CBLACKDEMON_DIE1,
  S_CBLACKDEMON_DIE2,
  S_CBLACKDEMON_DIE3,
  S_CBLACKDEMON_INVISIBLE1,
  S_CBLACKDEMON_INVISIBLE2,
  S_CBLACKDEMON_INVISIBLE3,
  S_CBLACKDEMON_INVISIBLE4,
  S_CBLACKDEMON_APPEAR1,
  S_CBLACKDEMON_APPEAR2,
  S_CBLACKDEMON_APPEAR3,
  S_CBLACKDEMONI_STND,
  S_CBDM_STND,                 // 2672
  S_CBDM_RUN1,                 // 2673
  S_CBDM_RUN2,                 // 2674
  S_CBDM_RUN3,                 // 2675
  S_CBDM_RUN4,                 // 2676
  S_CBDM_ATK1,                 // 2677
  S_CBDM_ATK2,                 // 2678
  S_CBDM_ATK3,                 // 2679
  S_CBDM_DIE1,                 // 2680
  S_CBDM_DIE2,                 // 2681
  S_CBDM_DIE3,                 // 2682
  S_CBDM_DIE4,                 // 2683
  S_CEYB_STND1,                // 2684
  S_CEYB_STND2,                // 2685
  S_CEYB_STND3,                // 2686
  S_CEYB_STND4,                // 2687
  S_CEYB_RUN1,                 // 2688
  S_CEYB_RUN2,                 // 2689
  S_CEYB_RUN3,                 // 2690
  S_CEYB_RUN4,                 // 2691
  S_CEYB_ATK1,                 // 2692
  S_CEYB_ATK2,                 // 2693
  S_CEYB_ATK3,                 // 2694
  S_CEYB_ATK4,                 // 2693
  S_CEYB_ATK5,                 // 2694
  S_CEYB_DIE1,                     // 2695
  S_CEYB_DIE2,                 // 2696
  S_CEYB_DIE3,                 // 2697
  S_CEYB_DIE4,                 // 2698
  S_CMAG_STND1,                // 2699
  S_CMAG_STND2,                // 2700
  S_CMAG_RUN1,                 // 2701
  S_CMAG_RUN2,                 // 2702
  S_CMAG_ATK1,                 // 2703
  S_CMAG_ATK2,                 // 2704
  S_CMAG_DIE1,                 // 2705
  S_CMAG_DIE2,                 // 2706
  S_CMAG_DIE3,                 // 2707
  S_CORC_STND,
  S_CORC_RUN1,
  S_CORC_RUN2,
  S_CORC_RUN3,
  S_CORC_RUN4,
  S_CORC_ATK1,
  S_CORC_ATK2,
  S_CORC_DIE1,
  S_CORC_DIE2,
  S_CORC_DIE3,
  S_CORC_DIE4,
  S_CRDM_STND,                 // 2708
  S_CRDM_RUN1,                 // 2709
  S_CRDM_RUN2,                 // 2710
  S_CRDM_RUN3,                 // 2711
  S_CRDM_RUN4,                 // 2712
  S_CRDM_ATK1,                 // 2713
  S_CRDM_ATK2,                 // 2714
  S_CRDM_ATK3,                 // 2715
  S_CRDM_DIE1,                 // 2716
  S_CRDM_DIE2,                 // 2717
  S_CRDM_DIE3,                 // 2718
  S_CRDM_DIE4,                 // 2719
  S_CSKE_STND,                 // 2720
  S_CSKE_RUN1,                 // 2721
  S_CSKE_RUN2,                 // 2722
  S_CSKE_RUN3,                 // 2723
  S_CSKE_RUN4,                 // 2724
  S_CSKE_ATK1,                 // 2725
  S_CSKE_ATK2,                 // 2726
  S_CSKE_ATK3,                 // 2727
  S_CSKE_DIE1,                 // 2728
  S_CSKE_DIE2,                 // 2729
  S_CSKE_DIE3,                 // 2730
  S_CSKE_DIE4,                 // 2728
  S_CSKE_DIE5,                 // 2729
  S_CSKE_DIE6,                 // 2730
  S_CSKE_STND2,
  S_CSKE_RAISE1,
  S_CSKE_RAISE2,
  S_CSKE_RAISE3,
  S_CSKE_RAISE4,
  S_CSKER_LOOK,
  S_CSKER_RAISE1,
  S_CSKER_RAISE2,
  S_CSKE2_STND,                // 2731
  S_CSKE2_RUN1,                // 2732
  S_CSKE2_RUN2,                // 2733
  S_CSKE2_RUN3,                // 2734
  S_CSKE2_RUN4,                // 2735
  S_CSKE2_ATK1,                // 2736
  S_CSKE2_ATK2,                // 2737
  S_CSKE2_ATK3,                // 2738
  S_CSKE2_DIE1,                // 2739
  S_CSKE2_DIE2,                // 2740
  S_CSKE2_DIE3,                // 2741
  S_CSKE2_DIE4,
  S_CSKE2_DIE5,
  S_CSKE2_DIE6,
  S_CSKE2_STND2,
  S_CSKE2_RAISE1,
  S_CSKE2_RAISE2,
  S_CSKE2_RAISE3,
  S_CSKE2_RAISE4,
  S_CSKE2R_LOOK,
  S_CSKE2R_RAISE1,
  S_CSKE2R_RAISE2,
  S_CTROLL_STND,
  S_CTROLL_RUN1,
  S_CTROLL_RUN2,
  S_CTROLL_RUN3,
  S_CTROLL_RUN4,
  S_CTROLL_ATK1,
  S_CTROLL_ATK2,
  S_CTROLL_ATK3,
  S_CTROLL_DIE1,
  S_CTROLL_DIE2,
  S_CTROLL_DIE3,
  S_CTROLL_DIE4,
  S_CTROLLWATER_STND,
  S_CTROLLWATER_SWIM1,
  S_CTROLLWATER_SWIM2,
  S_CTROLLWATER_RUN1,
  S_CTROLLWATER_RUN2,
  S_CTROLLWATER_RUN3,
  S_CTROLLWATER_RUN4,
  S_CTROLLWATER_ATK1,
  S_CTROLLWATER_ATK2,
  S_CTROLLWATER_ATK3,
  S_CTROLLWATER_DIE1,
  S_CTROLLWATER_DIE2,
  S_CTROLLWATER_DIE3,
  S_CTROLLWATER_DIE4,
  S_CTROLLWATER_RAISE1,
  S_CTROLLWATER_RAISE2,
  S_CTROLLWATER_RAISE3,
  S_CTROLLWATER_RAISE4,
  S_CTROLLWATER_RAISE5,
  S_CTROLLWATER_DIVE1,
  S_CTROLLWATER_DIVE2,
  S_CTROLLWATER_DIVE3,
  S_CTROLLWATER_DIVE4,
  S_CTROLLWATERI_STND,
  S_CZOM_STND,                 // 2742
  S_CZOM_STND2,                // 2743
  S_CZOM_RAISE1,               // 2744
  S_CZOM_RAISE2,               // 2745
  S_CZOM_RAISE3,               // 2746
  S_CZOM_RAISE4,               // 2747
  S_CZOM_RUN1,                 // 2748
  S_CZOM_RUN2,                 // 2749
  S_CZOM_RUN3,                 // 2750
  S_CZOM_ATK1,                 // 2751
  S_CZOM_ATK2,                 // 2752
  S_CZOM_DIE1,                 // 2753
  S_CZOM_DIE2,                 // 2754
  S_CZOM_DIE3,                 // 2755
  S_CZOM2_STND,                // 2756
  S_CZOM2_STND2,               // 2757
  S_CZOM2_RAISE1,              // 2758
  S_CZOM2_RAISE2,              // 2759
  S_CZOM2_RAISE3,              // 2760
  S_CZOM2_RAISE4,              // 2761
  S_CZOM2_RUN1,                // 2762
  S_CZOM2_RUN2,                // 2763
  S_CZOM2_RUN3,                // 2764
  S_CZOM2_ATK1,                // 2765
  S_CZOM2_ATK2,                // 2766
  S_CZOM2_DIE1,                // 2767
  S_CZOM2_DIE2,                // 2768
  S_CZOM2_DIE3,                // 2769
  S_CNEM_SEE1,                 // 2770
  S_CNEM_SEE2,                 // 2771
  S_CNEM_SEE3,                 // 2772
  S_CNEM_SEE4,                 // 2773
  S_CNEM_SEE5,                 // 2774
  S_CNEM_SEE6,                 // 2775
  S_CNEM_SEE7,                 // 2776
  S_CNEM_SEE8,                 // 2777
  S_CNEM_SEE9,                 // 2778
  S_CNEM_STND1,                // 2779
  S_CNEM_STND2,                // 2780
  S_CNEM_RUN1,                 // 2781
  S_CNEM_RUN2,                 // 2782
  S_CNEM_ATK,                  // 2783
  S_CNEM_ATK1_1,               // 2784
  S_CNEM_ATK1_2,               // 2785
  S_CNEM_ATK1_3,               // 2786
  S_CNEM_ATK2_1,               // 2787
  S_CNEM_ATK2_2,               // 2788
  S_CNEM_ATK2_3,               // 2789
  S_CNEM_ATK2_4,               // 2790
  S_CNEM_DIE1,                 // 2791
  S_CNEM_DIE2,                 // 2792
  S_CNEM_DIE3,                 // 2793
  S_CNEM_DIE4,                 // 2794
  S_CNEM_DIE5,                 // 2795
  S_CNEM_DIE6,                 // 2796
  S_CNEM_DIE7,                 // 2797
  S_CSNEM_ATK1,
  S_CSNEM_ATK2,
  S_CSNEM_ATK3,
  S_CSNEM_ATK4,
  S_CSNEM_ATK5,
  S_CSNEM_ATK6,
  S_CSNEM_ATK7,
  S_CSNEM_ATK8,
  S_CSNEM_ATK9,
  S_CSNEM_ATK10,
  S_CSNEM_ATK11,
  S_CSNEM_ATK12,
  S_CSNEM_DIE1,
  S_CSNEM_DIE2,
  S_CSNEM_DIE3,
  S_CSNEM_DIE4,
  S_CSNEM_LOOK,
  S_CSNEM_RAISE1,
  S_CSNEM_RAISE2,
  S_CSNEM_RAISE3,
  S_CHVIAL,                // 2798
  S_CBLUEKEY,                  // 2800
  S_CGREENKEY,                 // 2800
  S_CREDKEY,                   // 2801
  S_CYELLOWKEY,                // 2802
  S_CATACOMBFIREORBSMALL1,         // 2803
  S_CATACOMBFIREORBSMALL2,         // 2804
  S_CATACOMBFIREORBSMALL3,         // 2805
  S_CATACOMBFIREORBSMALL4,
  S_CATACOMBFIREORBLARGE1,         // 2806
  S_CATACOMBFIREORBLARGE2,         // 2807
  S_CATACOMBFIREORBLARGE3,         // 2808
  S_CATACOMBFIREORBLARGE4,
  S_CSCROLL1,
  S_CSCROLL2,
  S_CSCROLL3,
  S_CSCROLL4,
  S_CSCROLL5,
  S_CSCROLL6,
  S_CSCROLL7,
  S_CSCROLL8,
  S_CCHEST,                // 2809
  S_CCHESTWATER1,
  S_CCHESTWATER2,
  S_CGRAVE1,                   // 2810
  S_CGRAVE2,                   // 2811
  S_CGRAVE3,                   // 2812
  S_CWELL,                 // 2813
  S_CCHESTSMALL,               // 2814
  S_CCHESTSMALL2,              // 2815
  S_CCHESTSMALL3,              // 2816
  S_CCHESTMED,                 // 2818
  S_CCHESTMED2,                // 2819
  S_CCHESTMED3,                // 2820
  S_CCHESTLARGE,               // 2822
  S_CCHESTLARGE2,              // 2823
  S_CCHESTLARGE3,              // 2824
  S_CCHESTSMALLWATER1,
  S_CCHESTSMALLWATER2,
  S_CCHESTSMALLWATER3,
  S_CCHESTSMALLWATER4,
  S_CCHESTSMALLWATER5,
  S_CCHESTMEDWATER1,
  S_CCHESTMEDWATER2,
  S_CCHESTMEDWATER3,
  S_CCHESTMEDWATER4,
  S_CCHESTMEDWATER5,
  S_CCHESTLARGEWATER1,
  S_CCHESTLARGEWATER2,
  S_CCHESTLARGEWATER3,
  S_CCHESTLARGEWATER4,
  S_CCHESTLARGEWATER5,
  S_CATACEILINGLIGHTLIT1,
  S_CATACEILINGLIGHTLIT2,
  S_CATACEILINGLIGHTUNLIT,
  S_CBLOODMIST1,               // 2826
  S_CBLOODMIST21,              // 2827
  S_CBLOODMIST22,              // 2828
  S_CBLOODMIST23,              // 2829
  S_CPORTALLIGHT1,
  S_CPORTALLIGHT2,
  S_CPORTALLIGHT3,
  S_CPORTALLIGHT4,
  S_OMUTANTDEAD_STND,              // 2830
  S_OMUTANTDEAD_SEE,               // 2831
  S_OMSSS_STND,                // 2832
  S_OMSSS_RUN1,                // 2833
  S_OMSSS_RUN2,                // 2834
  S_OMSSS_RUN3,                // 2835
  S_OMSSS_RUN4,                // 2836
  S_OMSSS_RUN5,                // 2837
  S_OMSSS_RUN6,                // 2838
  S_OMSSS_RUN7,                // 2839
  S_OMSSS_RUN8,                // 2840
  S_OMSSS_ATK1,                // 2841
  S_OMSSS_ATK2,                // 2842
  S_OMSSS_ATK3,                // 2843
  S_OMSSS_ATK4,                // 2844
  S_OMSSS_ATK5,
  S_OMSSS_ATK6,
  S_OMSSS_ATK7,
  S_OMSSS_ATK8,
  S_OMSSS_ATK9,
  S_OMSSS_ATK10,
  S_OMSSS_PAIN1,               // 2845
  S_OMSSS_PAIN2,               // 2846
  S_OMSSS_DIE1,                // 2847
  S_OMSSS_DIE2,                // 2848
  S_OMSSS_DIE3,                // 2849
  S_OMSSS_DIE4,                // 2850
  S_OMSSS_DIE5,                // 2851
  S_OMSSS_XDIE1,               // 2847
  S_OMSSS_XDIE2,               // 2848
  S_OMSSS_XDIE3,               // 2849
  S_OMSSS_XDIE4,               // 2850
  S_OMSSS_XDIE5,               // 2851
  S_OMSSS_XDIE6,               // 2847
  S_OMSSS_XDIE7,               // 2848
  S_OMSSS_XDIE8,               // 2849
  S_OMSSS_XDIE9,               // 2850
  S_OMUTANT_DIE1,              // 2852
  S_OMUTANT_XDIE1,             // 2852
  S_OBAT_STND,                 // 2853
  S_OBAT_DIE1,                 // 2853
  S_OBAT_XDIE1,               // 2853
  S_RAVEN_STND,                // 2854
  S_RAVEN_STND2,               // 2855
  S_RAVEN_RUN1,                // 2856
  S_RAVEN_RUN2,                // 2857
  S_RAVEN_RUN3,                // 2858
  S_RAVEN_RUN4,                // 2859
  S_RAVEN_RUN5,                // 2860
  S_RAVEN_RUN6,                // 2861
  S_RAVEN_RUN7,                // 2862
  S_RAVEN_RUN8,                // 2863
  S_RAVEN_ATK1,                // 2864
  S_RAVEN_ATK2,                // 2865
  S_RAVEN_ATK3,                // 2866
  S_RAVEN_ATK4,                // 2867
  S_RAVEN_ATK5,                // 2868
  S_RAVEN_ATK6,                // 2869
  S_RAVEN_DIE1,                // 2874
  S_RAVEN_DIE2,                // 2875
  S_RAVEN_DIE3,                // 2876
  S_RAVEN_DIE4,                // 2877
  S_MADDOC_STND,               // 2878
  S_MADDOC_RUN1,               // 2879
  S_MADDOC_RUN2,               // 2880
  S_MADDOC_RUN3,               // 2881
  S_MADDOC_RUN4,               // 2882
  S_MADDOC_RUN5,               // 2883
  S_MADDOC_RUN6,               // 2884
  S_MADDOC_RUN7,               // 2885
  S_MADDOC_RUN8,               // 2886
  S_MADDOC_ATK1,               // 2887
  S_MADDOC_ATK2,               // 2888
  S_MADDOC_ATK3,               // 2889
  S_MADDOC_MATK,               // 2890
  S_MADDOC_DIE1,               // 2891
  S_MADDOC_DIE2,               // 2892
  S_MADDOC_DIE3,               // 2893
  S_MADDOC_DIE4,               // 2894
  S_BIOBLAST_STND,             // 2895
  S_BIOBLAST_RUN1,             // 2896
  S_BIOBLAST_RUN2,             // 2897
  S_BIOBLAST_RUN3,             // 2898
  S_BIOBLAST_RUN4,             // 2899
  S_BIOBLAST_RUN5,             // 2900
  S_BIOBLAST_RUN6,             // 2901
  S_BIOBLAST_RUN7,             // 2902
  S_BIOBLAST_RUN8,             // 2903
  S_BIOBLAST_ATK1,             // 2904
  S_BIOBLAST_ATK2,             // 2905
  S_BIOBLAST_ATK3,             // 2905
  S_BIOBLAST_DIE1,             // 2908
  S_BIOBLAST_DIE2,             // 2909
  S_BIOBLAST_DIE3,             // 2910
  S_BIOBLAST_DIE4,             // 2911
  S_BIOBLAST_DIE5,             // 2912
  S_BRAINDROID_STND,               // 2913
  S_BRAINDROID_RUN1,               // 2914
  S_BRAINDROID_RUN2,               // 2915
  S_BRAINDROID_RUN3,               // 2916
  S_BRAINGUY_RUN4,             // 2917
  S_BRAINDROID_RUN5,               // 2918
  S_BRAINDROID_RUN6,               // 2919
  S_BRAINDROID_RUN7,               // 2920
  S_BRAINDROID_RUN8,               // 2921
  S_BRAINDROID_ATK1,               // 2922
  S_BRAINDROID_ATK2,               // 2924
  S_BRAINDROID_ATK3,               // 2925
  S_BRAINDROID_ATK4,               // 2926
  S_BRAINDROID_ATK5,               // 2927
  S_BRAINDROID_ATK6,               // 2928
  S_BRAINDROID_DIE1,               // 2929
  S_BRAINDROID_DIE2,               // 2930
  S_BRAINDROID_DIE3,               // 2931
  S_BRAINDROID_DIE4,               // 2932
  S_BRAINDROID_DIE5,               // 2933
  S_BRAINDROID_DIE6,               // 2934
  S_CYBORGWAR_STND,            // 2935
  S_CYBORGWAR_RUN1,            // 2936
  S_CYBORGWAR_RUN2,            // 2937
  S_CYBORGWAR_RUN3,            // 2938
  S_CYBORGWAR_RUN4,            // 2939
  S_CYBORGWAR_RUN5,            // 2940
  S_CYBORGWAR_RUN6,            // 2941
  S_CYBORGWAR_RUN7,            // 2942
  S_CYBORGWAR_RUN8,            // 2943
  S_CYBORGWAR_ATK1,            // 2944
  S_CYBORGWAR_ATK2,            // 2945
  S_CYBORGWAR_ATK3,            // 2946
  S_CYBORGWAR_DIE1,            // 2947
  S_CYBORGWAR_DIE2,            // 2948
  S_CYBORGWAR_DIE3,            // 2949
  S_CYBORGWAR_DIE4,            // 2950
  S_MUTANTCOMMANDO_STND,           // 2951
  S_MUTANTCOMMANDO_RUN1,           // 2952
  S_MUTANTCOMMANDO_RUN2,           // 2953
  S_MUTANTCOMMANDO_RUN3,           // 2954
  S_MUTANTCOMMANDO_RUN4,           // 2955
  S_MUTANTCOMMANDO_RUN5,           // 2956
  S_MUTANTCOMMANDO_RUN6,           // 2957
  S_MUTANTCOMMANDO_RUN7,           // 2958
  S_MUTANTCOMMANDO_RUN8,           // 2939
  S_MUTANTCOMMANDO_ATK1,           // 2960
  S_MUTANTCOMMANDO_ATK2,           // 2961
  S_MUTANTCOMMANDO_ATK3,           // 2962
  S_MUTANTCOMMANDO_ATK4,           // 2963
  S_MUTANTCOMMANDO_ATK5,           // 2964
  S_MUTANTCOMMANDO_ATK6,           // 2965
  S_MUTANTCOMMANDO_ATK7,           // 2966
  S_MUTANTCOMMANDO_ATK8,           // 2967
  S_MUTANTCOMMANDO_ATK9,           // 2968
  S_MUTANTCOMMANDO_ATK10,          // 2969
  S_MUTANTCOMMANDO_DIE1,           // 2970
  S_MUTANTCOMMANDO_DIE2,           // 2971
  S_MUTANTCOMMANDO_DIE3,           // 2972
  S_MUTANTCOMMANDO_DIE4,           // 2973
  S_MULLER_STND,               // 2974
  S_MULLER_RUN1,               // 2975
  S_MULLER_RUN2,               // 2976
  S_MULLER_RUN3,               // 2977
  S_MULLER_RUN4,               // 2978
  S_MULLER_RUN5,               // 2979
  S_MULLER_RUN6,               // 2980
  S_MULLER_RUN7,               // 2981
  S_MULLER_RUN8,               // 2982
  S_MULLER_ATK,                // 2983
  S_MULLER_ATK1_1,             // 2984
  S_MULLER_ATK1_2,             // 2985
  S_MULLER_ATK1_3,             // 2986
  S_MULLER_ATK1_4,             // 2987
  S_MULLER_ATK1_5,             // 2988
  S_MULLER_ATK2_1,             // 2989
  S_MULLER_ATK2_2,             // 2990
  S_MULLER_ATK2_3,             // 2991
  S_MULLER_ATK2_4,                 // 2992
  S_MULLER_ATK2_5,             // 2993
  S_MULLER_ATK2_6,             // 2994
  S_MULLER_DIE1,               // 2995
  S_MULLER_DIE2,               // 2996
  S_MULLER_DIE3,               // 2997
  S_MULLER_DIE4,               // 2998
  S_MULLER_DIE5,               // 2999
  S_CHIMERA1_STND,             // 3000
  S_CHIMERA1_RUN1,             // 3001
  S_CHIMERA1_RUN2,             // 3002
  S_CHIMERA1_RUN3,             // 3003
  S_CHIMERA1_RUN4,             // 3004
  S_CHIMERA1_RUN5,             // 3005
  S_CHIMERA1_RUN6,             // 3006
  S_CHIMERA1_RUN7,             // 3007
  S_CHIMERA1_RUN8,             // 3008
  S_CHIMERA1_PAIN,             // 3009
  S_CHIMERA1_ATK1,             // 3010
  S_CHIMERA1_ATK2,             // 3011
  S_CHIMERA1_ATK3,             // 3012
  S_CHIMERA1_DIE1,             // 3013
  S_OMS2MGUARD_STND,               // 3015
  S_OMS2MGUARD_RUN1,               // 3016
  S_OMS2MGUARD_RUN2,               // 3017
  S_OMS2MGUARD_RUN3,               // 3018
  S_OMS2MGUARD_RUN4,               // 3019
  S_OMS2MGUARD_RUN5,               // 3020
  S_OMS2MGUARD_RUN6,               // 3021
  S_OMS2MGUARD_RUN7,               // 3022
  S_OMS2MGUARD_RUN8,               // 3023
  S_OMS2MGUARD_ATK1,               // 3024
  S_OMS2MGUARD_ATK2,               // 3025
  S_OMS2MGUARD_ATK3,               // 3026
  S_OMS2MGUARD_ATK4,               // 3027
  S_OMS2MGUARD_ATK5,
  S_OMS2MGUARD_ATK6,
  S_OMS2MGUARD_ATK7,
  S_OMS2MGUARD_ATK8,
  S_OMS2MGUARD_ATK9,
  S_OMS2MGUARD_ATK10,
  S_OMS2MGUARD_PAIN1,              // 3028
  S_OMS2MGUARD_PAIN2,              // 3029
  S_OMS2MGUARD_DIE1,               // 3030
  S_OMS2MGUARD_DIE2,               // 3031
  S_OMS2MGUARD_DIE3,               // 3032
  S_OMS2MGUARD_DIE4,               // 3033
  S_OMS2MGUARD_DIE5,               // 3034
  S_OMS2MGUARD_XDIE1,              // 3030
  S_OMS2MGUARD_XDIE2,              // 3031
  S_OMS2MGUARD_XDIE3,              // 3032
  S_OMS2MGUARD_XDIE4,              // 3033
  S_OMS2MGUARD_XDIE5,              // 3034
  S_OMS2MGUARD_XDIE6,              // 3030
  S_OMS2MGUARD_XDIE7,              // 3031
  S_OMS2MGUARD_XDIE8,              // 3032
  S_OMS2MGUARD_XDIE9,              // 3033
  S_DIRTDEVIL_STND,            // 3035
  S_DIRTDEVIL_RUN1,            // 3036
  S_DIRTDEVIL_ATK1,            // 3037
  S_DIRTDEVIL_ATK2,            // 3038
  S_DIRTDEVIL_ATK3,            // 3039
  S_DIRTDEVIL_ATK4,            // 3040
  S_DIRTDEVIL_DIE1,            // 3041
  S_DIRTDEVIL_DIE2,            // 3042
  S_DIRTDEVIL_DIE3,            // 3043
  S_DIRTDEVIL_DIE4,            // 3044
  S_DIRTDEVIL_XDIE1,               // 3041
  S_DIRTDEVIL_XDIE2,               // 3042
  S_DIRTDEVIL_XDIE3,               // 3043
  S_DIRTDEVIL_XDIE4,               // 3044
  S_DIRTDEVIL_XDIE5,               // 3044
  S_DIRTDEVILTRAIL1,
  S_DIRTDEVILTRAIL2,
  S_DIRTDEVILTRAIL3,
  S_DIRTDEVILTRAIL4,
  S_DIRTDEVILTRAIL5,
  S_DIRTDEVILTRAIL6,
  S_OSSAGENT_STND,             // 3045
  S_OSSAGENT_RUN1,             // 3046
  S_OSSAGENT_RUN2,             // 3047
  S_OSSAGENT_RUN3,             // 3048
  S_OSSAGENT_RUN4,             // 3049
  S_OSSAGENT_RUN5,             // 3050
  S_OSSAGENT_RUN6,             // 3051
  S_OSSAGENT_RUN7,             // 3052
  S_OSSAGENT_RUN8,             // 3053
  S_OSSAGENT_ATK1,             // 3054
  S_OSSAGENT_ATK2,             // 3055
  S_OSSAGENT_ATK3,             // 3056
  S_OSSAGENT_ATK4,             // 3057
  S_OSSAGENT_PAIN1,            // 3058
  S_OSSAGENT_PAIN2,            // 3059
  S_OSSAGENT_DIE1,             // 3060
  S_OSSAGENT_DIE2,             // 3061
  S_OSSAGENT_DIE3,             // 3062
  S_OSSAGENT_DIE4,             // 3063
  S_OSSAGENT_DIE5,             // 3064
  S_OSSAGENT_DIE6,             // 3065
  S_OSSAGENT_XDIE1,            // 3060
  S_OSSAGENT_XDIE2,            // 3061
  S_OSSAGENT_XDIE3,            // 3062
  S_OSSAGENT_XDIE4,            // 3063
  S_OSSAGENT_XDIE5,            // 3064
  S_OSSAGENT_XDIE6,            // 3065
  S_OSSAGENT_XDIE7,            // 3060
  S_OSSAGENT_XDIE8,            // 3061
  S_OSSAGENT_XDIE9,            // 3062
  S_SILVERFOX_STND,            // 3066
  S_SILVERFOX_SEE,             // 3067
  S_SILVERFOX_SEE2,            // 3068
  S_SILVERFOX_RUN1,            // 3069
  S_SILVERFOX_RUN2,            // 3070
  S_SILVERFOX_RUN3,            // 3071
  S_SILVERFOX_RUN4,            // 3072
  S_SILVERFOX_RUN5,            // 3073
  S_SILVERFOX_RUN6,            // 3074
  S_SILVERFOX_RUN7,            // 3075
  S_SILVERFOX_RUN8,            // 3076
  S_SILVERFOX_ATK1,            // 3077
  S_SILVERFOX_ATK2,            // 3078
  S_SILVERFOX_ATK3,            // 3079
  S_SILVERFOX_ATK4,            // 3080
  S_SILVERFOX_ATK5,            // 3081
  S_SILVERFOX_ATK6,            // 3082
  S_SILVERFOX_ATK7,            // 3083
  S_SILVERFOX_ATK8,            // 3084
  S_SILVERFOX_DIE1,            // 3085
  S_SILVERFOX_DIE2,            // 3086
  S_SILVERFOX_DIE3,            // 3087
  S_SILVERFOX_DIE4,            // 3088
  S_SILVERFOX_DIE5,            // 3089
  S_SILVERFOX_DIE6,            // 3090
  S_SILVERFOX_DIE7,            // 3091
  S_SILVERFOX_DIE8,            // 3092
  S_BALROG_STND,               // 3093
  S_BALROG_RUN1,               // 3094
  S_BALROG_RUN2,               // 3095
  S_BALROG_RUN3,               // 3096
  S_BALROG_RUN4,               // 3097
  S_BALROG_RUN5,               // 3098
  S_BALROG_RUN6,               // 3099
  S_BALROG_RUN7,               // 3100
  S_BALROG_RUN8,               // 3101
  S_BALROG_ATK1,               // 3102
  S_BALROG_ATK2,               // 3103
  S_BALROG_ATK3,               // 3104
  S_BALROG_ATK4,               // 3105
  S_BALROG_ATK5,               // 3106
  S_BALROG_DIE1,               // 3107
  S_BALROG_DIE2,               // 3108
  S_BALROG_DIE3,               // 3109
  S_BALROG_DIE4,               // 3110
  S_BALROG_DIE5,               // 3111
  S_MECHSENTINEL_STND,             // 3112
  S_MECHSENTINEL_RUN1,             // 3113
  S_MECHSENTINEL_RUN2,             // 3114
  S_MECHSENTINEL_RUN3,             // 3115
  S_MECHSENTINEL_RUN4,             // 3116
  S_MECHSENTINEL_RUN5,             // 3117
  S_MECHSENTINEL_RUN6,             // 3118
  S_MECHSENTINEL_RUN7,             // 3119
  S_MECHSENTINEL_RUN8,             // 3120
  S_MECHSENTINEL_ATK1,             // 3221
  S_MECHSENTINEL_ATK2,             // 3122
  S_MECHSENTINEL_ATK3,             // 3123
  S_MECHSENTINEL_ATK4,             // 3124
  S_MECHSENTINEL_PAIN1,            // 3125
  S_MECHSENTINEL_PAIN2,            // 3126
  S_MECHSENTINEL_DIE1,             // 3127
  S_MECHSENTINEL_DIE2,             // 3128
  S_MECHSENTINEL_DIE3,             // 3129
  S_MECHSENTINEL_DIE4,             // 3130
  S_MECHSENTINEL_DIE5,             // 3131
  S_OMSNEMESIS_STND,               // 3132
  S_OMSNEMESIS_RUN1,               // 3133
  S_OMSNEMESIS_RUN2,               // 3134
  S_OMSNEMESIS_RUN3,               // 3135
  S_OMSNEMESIS_RUN4,               // 3136
  S_OMSNEMESIS_RUN5,               // 3137
  S_OMSNEMESIS_RUN6,               // 3138
  S_OMSNEMESIS_RUN7,               // 3139
  S_OMSNEMESIS_RUN8,               // 3140
  S_OMSNEMESIS_ATK1,               // 3141
  S_OMSNEMESIS_ATK2,               // 3142
  S_OMSNEMESIS_ATK3,               // 3143
  S_OMSNEMESIS_DIE1,               // 3144
  S_OMSNEMESIS_DIE2,               // 3145
  S_OMSNEMESIS_DIE3,               // 3146
  S_OMSNEMESIS_DIE4,               // 3147
  S_MUTANTX_STND,              // 3148
  S_MUTANTX_RUN1,              // 3149
  S_MUTANTX_RUN2,              // 3150
  S_MUTANTX_RUN3,              // 3151
  S_MUTANTX_RUN4,              // 3152
  S_MUTANTX_RUN5,              // 3153
  S_MUTANTX_RUN6,              // 3154
  S_MUTANTX_RUN7,              // 3155
  S_MUTANTX_RUN8,              // 3156
  S_MUTANTX_ATK1,              // 3157
  S_MUTANTX_ATK2,              // 3158
  S_MUTANTX_ATK3,              // 3159
  S_MUTANTX_ATK4,              // 3160
  S_MUTANTX_DIE1,              // 3161
  S_MUTANTX_DIE2,              // 3162
  S_MUTANTX_DIE3,              // 3163
  S_MUTANTX_DIE4,              // 3164
  S_OMEGAMUTANT_STND,              // 3165
  S_OMEGAMUTANT_RUN1,              // 3166
  S_OMEGAMUTANT_RUN2,              // 3167
  S_OMEGAMUTANT_RUN3,              // 3168
  S_OMEGAMUTANT_RUN4,              // 3169
  S_OMEGAMUTANT_RUN5,              // 3170
  S_OMEGAMUTANT_RUN6,              // 3171
  S_OMEGAMUTANT_RUN7,              // 3172
  S_OMEGAMUTANT_RUN8,              // 3173
  S_OMEGAMUTANT_ATK1,              // 3174
  S_OMEGAMUTANT_ATK2,              // 3175
  S_OMEGAMUTANT_ATK3,              // 3176
  S_OMEGAMUTANT_ATK4,              // 3177
  S_OMEGAMUTANT_ATK5,              // 3178
  S_OMEGAMUTANT_ATK6,              // 3179
  S_OMEGAMUTANT_ATK7,              // 3180
  S_OMEGAMUTANT_ATK8,              // 3181
  S_OMEGAMUTANT_ATK9,              // 3182
  S_OMEGAMUTANT_ATK10,             // 3183
  S_OMEGAMUTANT_ATK11,             // 3184
  S_OMEGAMUTANT_ATK12,             // 3185
  S_OMEGAMUTANT_ATK13,             // 3186
  S_OMEGAMUTANT_PAIN,              // 3187
  S_OMEGAMUTANT_DIE1,              // 3188
  S_OMEGAMUTANT_DIE2,              // 3189
  S_OMEGAMUTANT_DIE3,              // 3190
  S_OMEGAMUTANT_DIE4,              // 3191
  S_OMEGAMUTANT_DIE5,              // 3192
  S_BRAINDROID2_DIE1,              // 3193
  S_BRAINDROID2_DIE2,              // 3194
  S_BRAINDROID2_DIE3,              // 3195
  S_CYBORGWAR2_DIE1,               // 3196
  S_MUTANTCOMMANDO2_DIE1,          // 3197
  S_MUTANTCOMMANDO2_DIE2,          // 3198
  S_MUTANTCOMMANDO2_DIE3,          // 3199
  S_CHIMERA2_STND,             // 3200
  S_CHIMERA2_RUN1,             // 3201
  S_CHIMERA2_RUN2,             // 3202
  S_CHIMERA2_RUN3,             // 3203
  S_CHIMERA2_RUN4,             // 3204
  S_CHIMERA2_RUN5,             // 3205
  S_CHIMERA2_RUN6,                         // 3206
  S_CHIMERA2_RUN7,             // 3207
  S_CHIMERA2_RUN8,             // 3208
  S_CHIMERA2_RUN9,             // 3209
  S_CHIMERA2_RUN10,            // 3210
  S_CHIMERA2_PAIN,             // 3211
  S_CHIMERA2_ATK,              // 3212
  S_CHIMERA2_ATK1_1,               // 3213
  S_CHIMERA2_ATK1_2,               // 3214
  S_CHIMERA2_ATK2_1,               // 3215
  S_CHIMERA2_ATK2_2,               // 3216
  S_CHIMERA2_DIE1,             // 3217
  S_CHIMERA2_DIE2,             // 3218
  S_CHIMERA2_DIE3,             // 3219
  S_CHIMERA2_DIE4,             // 3220
  S_CHIMERA2_DIE5,             // 3221
  S_CHIMERA2_DIE6,             // 3222
  S_CHIMERA2_DIE7,             // 3220
  S_CHIMERA2_DIE8,             // 3221
  S_CHIMERA2_DIE9,             // 3222
  S_CHIMERATENTACLE1,              // 3223
  S_CHIMERATENTACLE2,              // 3224
  S_CHIMERATENTACLE3,              // 3225
  S_CHIMERATENTACLE4,              // 3226
  S_CHIMERATENTACLE5,              // 3227
  S_CHIMERATENTACLE6,              // 3228
  S_TENTACLE1,                 // 3229
  S_TENTACLE2,                 // 3230
  S_TENTACLE3,                 // 3231
  S_TENTACLE4,                 // 3232
  S_TENTACLE5,                 // 3233
  S_TENTACLE6,                 // 3234
  S_MUTANT2DEAD_STND,              // 3235
  S_MUTANT2DEAD_SEE,               // 3236
  S_MUTANT3DEAD_STND,              // 3237
  S_MUTANT3DEAD_SEE,               // 3238
  S_MECHSENTINEL2_DIE1,            // 3239
  S_OMSNEMESIS2_DIE1,              // 3240
  S_OMSNEMESIS2_DIE2,              // 3241
  S_OMSNEMESIS2_DIE3,              // 3242
  S_MUTANTX2_DIE1,             // 3243
  S_MUTANTX2_DIE2,             // 3244
  S_OMEGAMUTANT2_STND,             // 3245
  S_OMEGAMUTANT2_RUN1,             // 3246
  S_OMEGAMUTANT2_RUN2,             // 3247
  S_OMEGAMUTANT2_RUN3,             // 3248
  S_OMEGAMUTANT2_RUN4,             // 3249
  S_OMEGAMUTANT2_RUN5,             // 3250
  S_OMEGAMUTANT2_RUN6,             // 3251
  S_OMEGAMUTANT2_RUN7,             // 3252
  S_OMEGAMUTANT2_RUN8,             // 3253
  S_OMEGAMUTANT2_ATK1,             // 3254
  S_OMEGAMUTANT2_ATK2,             // 3255
  S_OMEGAMUTANT2_ATK3,             // 3256
  S_OMEGAMUTANT2_ATK4,             // 3257
  S_OMEGAMUTANT2_ATK5,             // 3258
  S_OMEGAMUTANT2_ATK6,             // 3259
  S_OMEGAMUTANT2_ATK7,             // 3260
  S_OMEGAMUTANT2_ATK8,             // 3261
  S_OMEGAMUTANT2_ATK9,             // 3262
  S_OMEGAMUTANT2_ATK10,            // 3263
  S_OMEGAMUTANT2_ATK11,            // 3264
  S_OMEGAMUTANT2_ATK12,            // 3265
  S_OMEGAMUTANT2_ATK13,            // 3266
  S_OMEGAMUTANT2_PAIN,             // 3267
  S_OMEGAMUTANT2_DIE1,             // 3268
  S_OMEGAMUTANT2_DIE2,             // 3269
  S_OMEGAMUTANT2_DIE3,             // 3270
  S_OFIREDRAKE_STND,
  S_OFIREDRAKE_STND2,
  S_OFIREDRAKE_RAISE1,
  S_OFIREDRAKE_RAISE2,
  S_OFIREDRAKE_RAISE3,
  S_OFIREDRAKE_RAISE4,
  S_OFIREDRAKE_RUN1,
  S_OFIREDRAKE_RUN2,
  S_OFIREDRAKE_RUN3,
  S_OFIREDRAKE_RUN4,
  S_OFIREDRAKE_RUN5,
  S_OFIREDRAKE_RUN6,
  S_OFIREDRAKE_RUN7,
  S_OFIREDRAKE_RUN8,
  S_OFIREDRAKE_ATK1,
  S_OFIREDRAKE_ATK2,
  S_OFIREDRAKE_ATK3,
  S_OFIREDRAKE_ATK4,
  S_OFIREDRAKE_ATK5,
  S_OFIREDRAKE_ATK6,
  S_OFIREDRAKE_ATK7,
  S_OFIREDRAKE_ATK8,
  S_OFIREDRAKE_ATK9,
  S_OFIREDRAKE_ATK10,
  S_OFIREDRAKE_ATK11,
  S_OFIREDRAKE_ATK12,
  S_OFIREDRAKE_ATK13,
  S_OFIREDRAKE_ATK14,
  S_OFIREDRAKE_ATK15,
  S_OFIREDRAKE_ATK16,
  S_OFIREDRAKE_ATK17,
  S_OFIREDRAKE_ATK18,
  S_OFIREDRAKE_DIE1,
  S_OFIREDRAKE_DIE2,
  S_OFIREDRAKE_DIE3,
  S_OFIREDRAKE_DIE4,
  S_OFIREDRAKEHEAD_DIE1,
  S_OFIREDRAKEHEAD_DIE2,
  S_OFIREDRAKEHEAD_DIE3,
  S_OFIREDRAKEHEAD_DIE4,
  S_OFIREDRAKEHEAD_DIE5,
  S_OFIREDRAKEHEAD_DIE6,
  S_OFIREDRAKEHEAD_DIE7,
  S_OFIREDRAKEHEAD_DIE8,
  S_OMINIFIRSTSIDKIT,              // 3271
  S_OMSMEGA,                   // 3272
  S_OCLIP,                 // 3273
  S_OTREASUREPILE,             // 3274
  S_OTORCH1,                   // 3275
  S_OTORCH2,                   // 3276
  S_OTORCH3,                   // 3277
  S_OTORCH4,                   // 3278
  S_OTORCHU,                   // 3279
  S_OBARRELGREY,               // 3280
  S_OBARRELGREYBURNING1,           // 3281
  S_OBARRELGREYBURNING2,           // 3282
  S_OMACHINE,                  // 3283
  S_OROCK,                 // 3284
  S_OSTALAGMITEC,              // 3285
  S_OSTALAGMITEF,              // 3286
  S_OTABLEBLOODY,              // 3287
  S_OTABLEMUTANT,              // 3288
  S_OTENTACLE_WELL1,               // 3289
  S_OTENTACLE_WELL2,               // 3290
  S_OTENTACLE_WELL3,               // 3291
  S_OTENTACLE_WELL4,               // 3292
  S_OTENTACLE_WELL5,               // 3293
  S_OTENTACLE_WELL6,               // 3294
  S_OCONICALFLASK,             // 3295
  S_ODESK,                 // 3296
  S_OKNIGHT,                   // 3297
  S_OCOLUMNWHITE,              // 3298
  S_OSPEARS1,                  // 3299
  S_OSPEARS2,                  // 3300
  S_OSPEARS3,                  // 3301
  S_OSPEARS4,                  // 3302
  S_OSPEARS5,                  // 3303
  S_OSPEARS6,                  // 3304
  S_OSPEARS7,                  // 3305
  S_OSPEARS8,                  // 3306
  S_OTABLEEMPTY,               // 3307
  S_OTOLIET,                   // 3308
  S_ODEADMUTANT,               // 3309
  S_ODEADMULLER,               // 3310
  S_ODEADSSGUARD,              // 3311
  S_OCOLUMNWHITEBROKEN,            // 3312
  S_OELETROPILLAROFF,              // 3313
  S_OSLIMEPOOL,                // 3314
  S_ODEADMGUNGUARD,            // 3315
  S_ODEADOSSAGENT,             // 3316
  S_OCANDELABRA1,              // 3317
  S_OCANDELABRA2,              // 3317
  S_OCANDELABRA3,              // 3317
  S_OCANDELABRA4,              // 3317
  S_OCANDELABRAU,              // 3318
  S_OHOST,                 // 3319
  S_IGUARD_DIE1,               // 3320
  S_IGUARD_XDIE1,              // 3320
  S_IMUTANT_DIE1,              // 3321
  S_IMUTANT_XDIE1,             // 3321
  S_IOFFICER_DIE1,             // 3322
  S_IOFFICER_XDIE1,            // 3322
  S_IOFFICER2_DIE1,            // 3323
  S_IOFFICER2_XDIE1,               // 3323
  S_UBER2_RUN1,                // 3325
  S_UBER2_RUN2,                // 3326
  S_UBER2_RUN3,                // 3327
  S_UBER2_RUN4,                // 3328
  S_UBER2_RUN5,                // 3329
  S_UBER2_RUN6,                // 3330
  S_UBER2_RUN7,                // 3331
  S_UBER2_RUN8,                // 3332
  S_UBER2_DIE1,                // 3338
  S_UBER3_DIE1,
  S_UBER3_DIE2,
  S_ANG2_STND1,                // 3339
  S_ANG2_STND2,                // 3340
  S_ANG2_RUN1,                 // 3341
  S_ANG2_RUN2,                 // 3342
  S_ANG2_RUN3,                 // 3343
  S_ANG2_RUN4,                 // 3344
  S_ANG2_RUN5,                 // 3345
  S_ANG2_RUN6,                 // 3346
  S_ANG2_RUN7,                 // 3347
  S_ANG2_RUN8,                 // 3348
  S_ANG2_ATK,                  // 3349
  S_ANG2_ATK1_1,                   // 3350
  S_ANG2_ATK1_2,                   // 3351
  S_ANG2_ATK1_3,                   // 3352
  S_ANG2_ATK1_4,                   // 3353
  S_ANG2_ATK1_5,                   // 3354
  S_ANG2_ATK1_6,                   // 3355
  S_ANG2_ATK2_1,                   // 3356
  S_ANG2_ATK2_2,                   // 3357
  S_ANG2_ATK2_3,                   // 3358
  S_ANG2_ATK2_4,                   // 3359
  S_ANG2_ATK2_5,                   // 3360
  S_ANG2_ATK2_6,                   // 3361
  S_ANG2_ATK2_7,                   // 3362
  S_ANG2_ATK3_1,                   // 3363
  S_ANG2_ATK3_2,                   // 3364
  S_ANG2_ATK3_3,                   // 3365
  S_ANG2_DIE1,                 // 3366
  S_ANG2_DIE2,                 // 3367
  S_ANG2_DIE3,                 // 3368
  S_ANG2_DIE4,                 // 3369
  S_ANG2_DIE5,                 // 3370
  S_ANG2_DIE6,                 // 3371
  S_ANG2_DIE7,                 // 3372
  S_ANG2_DIE8,                 // 3373
  S_ANG2_DIE9,                 // 3374
  S_ANG2_DIE10,                // 3375
  S_IFLAMEGUARD_DIE1,                  // 3375
  S_IFLAMEGUARD_XDIE1,                 // 3375
  S_ISTCROSS,                  // 3376
  S_ISTCUP,                // 3377
  S_ISTCHEST,                  // 3378
  S_ISTCROWN,                  // 3379
  S_ISTFIRSTAIDKIT,                // 3380
  S_ISTMEGA,                   // 3381
  S_ISTCLIP,                   // 3382
  S_ISTAMMO,                   // 3383
  S_ISTCANDELABRALIT,              // 3384
  S_ISTCANDELABRAUNLIT,            // 3385
  S_ISTCEILINGLIGHTBLUELITYEL,         // 3386
  S_ISTCEILINGLIGHTBLUELITWHT,         // 3386
  S_ISTCEILINGLIGHTBLUEUNLIT,          // 3387
  S_ISTKNIGHT,                 // 3388
  S_ISTTABLEWOOD,              // 3389
  S_ISTTABLE2,                 // 3390
  S_ISTCEILINGFAN,             // 3391
  S_ISTKNIGHT2,                // 3388
  S_ISTKNIGHT3,                // 3388
  S_ISTTABLEWOOD2,             // 3389
  S_ISTTABLEWOOD3,                 // 3390
  S_ISTTABLE5,                 // 3391
  S_ISTCEILINGFANON1,              // 3391
  S_ISTCEILINGFANON2,              // 3391
  S_ISTCEILINGFANON3,              // 3391
  S_ISTCEILINGFANON4,              // 3391
  S_ISTCEILINGFANON5,              // 3391
  S_ISTCEILINGFANON6,              // 3391
  S_DOOMGUY_STND,              // 3392
  S_DOOMGUY_RUN1,              // 3393
  S_DOOMGUY_RUN2,              // 3394
  S_DOOMGUY_RUN3,              // 3395
  S_DOOMGUY_RUN4,              // 3396
  S_DOOMGUY_ATK1,              // 3397
  S_DOOMGUY_ATK2,              // 3398
  S_DOOMGUY_PAIN1,             // 3399
  S_DOOMGUY_PAIN2,             // 3400
  S_DOOMGUY_DIE1,              // 3401
  S_DOOMGUY_DIE2,              // 3402
  S_DOOMGUY_DIE3,              // 3403
  S_DOOMGUY_DIE4,              // 3404
  S_DOOMGUY_DIE5,              // 3405
  S_DOOMGUY_DIE6,              // 3406
  S_DOOMGUY_DIE7,              // 3407
  S_DOOMGUY_XDIE1,             // 3401
  S_DOOMGUY_XDIE2,             // 3402
  S_DOOMGUY_XDIE3,             // 3403
  S_DOOMGUY_XDIE4,             // 3404
  S_DOOMGUY_XDIE5,             // 3405
  S_DOOMGUY_XDIE6,             // 3406
  S_DOOMGUY_XDIE7,             // 3407
  S_DOOMGUY_XDIE8,             // 3406
  S_DOOMGUY_XDIE9,             // 3407
  S_CHAINGUNZOMBIE_STND,
  S_CHAINGUNZOMBIE_STND2,
  S_CHAINGUNZOMBIE_RUN1,
  S_CHAINGUNZOMBIE_RUN2,
  S_CHAINGUNZOMBIE_RUN3,
  S_CHAINGUNZOMBIE_RUN4,
  S_CHAINGUNZOMBIE_RUN5,
  S_CHAINGUNZOMBIE_RUN6,
  S_CHAINGUNZOMBIE_RUN7,
  S_CHAINGUNZOMBIE_RUN8,
  S_CHAINGUNZOMBIE_ATK1,
  S_CHAINGUNZOMBIE_ATK2,
  S_CHAINGUNZOMBIE_ATK3,
  S_CHAINGUNZOMBIE_ATK4,
  S_CHAINGUNZOMBIE_PAIN1,
  S_CHAINGUNZOMBIE_PAIN2,
  S_CHAINGUNZOMBIE_DIE1,
  S_CHAINGUNZOMBIE_DIE2,
  S_CHAINGUNZOMBIE_DIE3,
  S_CHAINGUNZOMBIE_DIE4,
  S_CHAINGUNZOMBIE_DIE5,
  S_CHAINGUNZOMBIE_DIE6,
  S_CHAINGUNZOMBIE_DIE7,
  S_CHAINGUNZOMBIE_XDIE1,
  S_CHAINGUNZOMBIE_XDIE2,
  S_CHAINGUNZOMBIE_XDIE3,
  S_CHAINGUNZOMBIE_XDIE4,
  S_CHAINGUNZOMBIE_XDIE5,
  S_CHAINGUNZOMBIE_XDIE6,
  S_SERPENT_LOOK,              // 3413
  S_SERPENT_START,             // 3414
  S_SERPENT_SWIM1,             // 3415
  S_SERPENT_SWIM2,             // 3416
  S_SERPENT_SWIM3,             // 3417
  S_SERPENT_ATK1,              // 3418
  S_SERPENT_ATK2,              // 3419
  S_SERPENT_ATK3,              // 3420
  S_SERPENT_ATK4,              // 3421
  S_SERPENT_ATK5,              // 3422
  S_SERPENT_ATK6,              // 3423
  S_SERPENT_MATK1,             // 3424
  S_SERPENT_MATK2,             // 3425
  S_SERPENT_MATK3,             // 3426
  S_SERPENT_MATK4,             // 3427
  S_SERPENT_MATK5,             // 3428
  S_SERPENT_MATK6,             // 3429
  S_SERPENT_MATK7,             // 3430
  S_SERPENT_DIVE1,             // 3431
  S_SERPENT_DIVE2,             // 3432
  S_SERPENT_DIVE3,             // 3433
  S_SERPENT_DIVE4,             // 3434
  S_SERPENT_DIVE5,             // 3435
  S_SERPENT_DIVE6,             // 3436
  S_SERPENT_DIVE7,             // 3437
  S_SERPENT_DIVE8,             // 3438
  S_SERPENT_DIVE9,             // 3439
  S_SERPENT_DIVE10,            // 3440
  S_SERPENT_DIE1,              // 3441
  S_SERPENT_DIE2,              // 3442
  S_SERPENT_DIE3,              // 3443
  S_SERPENT_DIE4,              // 3444
  S_SERPENT_DIE5,              // 3445
  S_SERPENT_DIE6,              // 3446
  S_SERPENT_DIE7,              // 3447
  S_SERPENT_DIE8,              // 3448
  S_SERPENT_DIE9,              // 3449
  S_SERPENT_DIE10,             // 3450
  S_SERPENT_DIE11,             // 3451
  S_SERPENT_DIE12,             // 3452
  S_SERPENT_XDIE1,             // 3441
  S_SERPENT_XDIE2,             // 3442
  S_SERPENT_XDIE3,             // 3443
  S_SERPENT_XDIE4,             // 3444
  S_SERPENT_XDIE5,             // 3445
  S_SERPENT_XDIE6,             // 3446
  S_SERPENT_XDIE7,             // 3447
  S_SERPENT_XDIE8,             // 3448
  S_ARMOURSUIT_STND,               // 3453
  S_ARMOURSUIT_RUN1,               // 3454
  S_ARMOURSUIT_RUN2,               // 3455
  S_ARMOURSUIT_RUN3,               // 3456
  S_ARMOURSUIT_RUN4,               // 3457
  S_ARMOURSUIT_RUN5,               // 3458
  S_ARMOURSUIT_RUN6,               // 3459
  S_ARMOURSUIT_RUN7,               // 3460
  S_ARMOURSUIT_RUN8,               // 3461
  S_ARMOURSUIT_ATK1,               // 3462
  S_ARMOURSUIT_ATK2,               // 3463
  S_ARMOURSUIT_ATK3,               // 3464
  S_ARMOURSUIT_ATK4,               // 3465
  S_ARMOURSUIT_ATK5,               // 3466
  S_ARMOURSUIT_ATK6,               // 3467
  S_ARMOURSUIT_DIE1,               // 3472
  S_ARMOURSUIT_DIE2,               // 3473
  S_ARMOURSUIT_DIE3,               // 3474
  S_ARMOURSUIT_DIE4,               // 3475
  S_POOPDECK_STND,             // 3476
  S_POOPDECK_RUN1,             // 3477
  S_POOPDECK_RUN2,             // 3478
  S_POOPDECK_RUN3,             // 3479
  S_POOPDECK_RUN4,             // 3480
  S_POOPDECK_RUN5,             // 3481
  S_POOPDECK_RUN6,             // 3482
  S_POOPDECK_RUN7,             // 3483
  S_POOPDECK_RUN8,             // 3484
  S_POOPDECK_ATK,              // 3485
  S_POOPDECK_ATK1_1,               // 3486
  S_POOPDECK_ATK1_2,               // 3487
  S_POOPDECK_ATK1_3,               // 3488
  S_POOPDECK_ATK1_4,               // 3489
  S_POOPDECK_ATK1_5,               // 3490
  S_POOPDECK_ATK2_1,               // 3491
  S_POOPDECK_ATK2_2,               // 3492
  S_POOPDECK_ATK2_3,               // 3493
  S_POOPDECK_ATK2_4,               // 3494
  S_POOPDECK_ATK2_5,               // 3495
  S_POOPDECK_ATK2_6,               // 3496
  S_POOPDECK_DIE1,             // 3497
  S_POOPDECK_DIE2,             // 3498
  S_POOPDECK_DIE3,             // 3499
  S_POOPDECK_DIE4,             // 3500
  S_TANKMAN_STND,              // 3501
  S_TANKMAN_RUN1,              // 3502
  S_TANKMAN_RUN2,              // 3503
  S_TANKMAN_RUN3,              // 3504
  S_TANKMAN_RUN4,              // 3505
  S_TANKMAN_RUN5,              // 3506
  S_TANKMAN_RUN6,              // 3507
  S_TANKMAN_RUN7,              // 3508
  S_TANKMAN_RUN8,              // 3509
  S_TANKMAN_ATK1,              // 3510
  S_TANKMAN_ATK2,              // 3511
  S_TANKMAN_ATK3,              // 3512
  S_TANKMAN_ATK4,              // 3513
  S_TANKMAN_ATK5,              // 3514
  S_TANKMAN_ATK6,              // 3515
  S_TANKMAN_DIE1,              // 3516
  S_TANKMAN_DIE2,              // 3517
  S_TANKMAN_DIE3,              // 3518
  S_TANKMAN_DIE4,              // 3519
  S_TANKMAN_DIE5,              // 3520
  S_TANKMAN_DIE6,              // 3519
  S_TANKMAN_DIE7,              // 3520
  S_PHANTOM_STND1,             // 3521
  S_PHANTOM_STND2,             // 3522
  S_PHANTOM_STND3,             // 3523
  S_PHANTOM_STND4,             // 3524
  S_PHANTOM_RUN1,              // 3525
  S_PHANTOM_RUN2,              // 3526
  S_PHANTOM_RUN3,              // 3527
  S_PHANTOM_RUN4,              // 3528
  S_PHANTOM_ATK1,              // 3529
  S_PHANTOM_ATK2,              // 3530
  S_PHANTOM_ATK3,              // 3531
  S_PHANTOM_ATK4,              // 3532
  S_PHANTOM_ATK5,              // 3533
  S_PHANTOM_ATK6,              // 3534
  S_PHANTOM_MATK1,             // 3535
  S_PHANTOM_MATK2,             // 3536
  S_PHANTOM_MATK3,             // 3535
  S_PHANTOM_MATK4,             // 3536
  S_SCHABBSD_ATK,              // 3537
  S_SCHABBSD_ATK1_1,               // 3538
  S_SCHABBSD_ATK1_2,               // 3539
  S_SCHABBSD_ATK1_3,               // 3540
  S_SCHABBSD_ATK1_4,               // 3541
  S_SCHABBSD_ATK2_1,               // 3542
  S_SCHABBSD_ATK2_2,               // 3543
  S_SCHABBSD_ATK2_3,               // 3544
  S_SCHABBSD_ATK2_4,               // 3545
  S_SCHABBSD_DIE1,             // 3548
  S_SCHABBSD_DIE2,             // 3549
  S_SCHABBSD_DIE3,             // 3550
  S_SCHABBSD_DIE4,             // 3551
  S_SCHABBSD_DIE5,             // 3552
  S_SCHABBSD_DIE6,             // 3553
  S_SCHABBSD_DIE7,             // 3554
  S_SCHABBSD_DIE8,             // 3555
  S_SCHABBSD_DIE9,             // 3556
  S_SCHABBSD_DIE10,            // 3557
  S_SCHABBSD_DIE11,            // 3558
  S_SCHABBSD_DIE12,            // 3559
  S_SCHABBSD_DIE13,            // 3560
  S_SCHABBSD_DIE14,            // 3561
  S_SCHABBSD_DIE15,            // 3562
  S_SCHABBSD_DIE16,            // 3563
  S_SCHABBSD_DIE17,            // 3564
  S_SCHABBSD_DIE18,            // 3565
  S_SCHABBSD_DIE19,            // 3566
  S_SCHABBSD_DIE20,            // 3567
  S_SCHABBSD_DIE21,            // 3568
  S_SCHABBSD_DIE22,            // 3569
  S_SCHABBSD_DIE23,            // 3570
  S_SCHABBSD_DIE24,            // 3571
  S_SCHABBSD_DIE25,            // 3572
  S_SCHABBSD_DIE26,            // 3573
  S_SCHABBSD_DIE27,            // 3574
  S_SCHABBSD_DIE28,            // 3575
  S_SCHABBSD_DIE29,            // 3576
  S_CHAINGUNZOMBIESK_DIE1,         // 3578
  S_CHAINGUNZOMBIESK_XDIE1,        // 3578
  S_CHAINGUNZOMBIEGK_DIE1,         // 3577
  S_CHAINGUNZOMBIEGK_XDIE1,        // 3577
  S_ARMOURSUIT2_RUN1,              // 3580
  S_ARMOURSUIT2_RUN2,              // 3581
  S_ARMOURSUIT2_RUN3,              // 3582
  S_ARMOURSUIT2_RUN4,              // 3583
  S_ARMOURSUIT2_RUN5,              // 3584
  S_ARMOURSUIT2_RUN6,              // 3585
  S_ARMOURSUIT2_RUN7,              // 3586
  S_ARMOURSUIT2_RUN8,              // 3587
  S_ARMOURSUIT2_DIE1,              // 3598
  S_ARMOURSUIT2_DIE2,              // 3599
  S_ARMOURSUIT2_DIE3,              // 3600
  S_ARMOURSUIT2_DIE4,              // 3601
  S_ARMOURSUIT3_DIE1,              // 3602
  S_ARMOURSUIT3_DIE2,              // 3603
  S_HELLGUARD_STND,
  S_HELLGUARD_RUN1,
  S_HELLGUARD_RUN2,
  S_HELLGUARD_RUN3,
  S_HELLGUARD_RUN4,
  S_HELLGUARD_RUN5,
  S_HELLGUARD_RUN6,
  S_HELLGUARD_RUN7,
  S_HELLGUARD_RUN8,
  S_HELLGUARD_ATK1,
  S_HELLGUARD_ATK2,
  S_HELLGUARD_ATK3,
  S_HELLGUARD_ATK4,
  S_HELLGUARD_ATK5,
  S_HELLGUARD_PAIN1,
  S_HELLGUARD_PAIN2,
  S_HELLGUARD_DIE1,
  S_HELLGUARD_DIE2,
  S_HELLGUARD_DIE3,
  S_HELLGUARD_DIE4,
  S_HELLGUARD_DIE5,
  S_HELLGUARD_DIE6,
  S_HELLGUARD_DIE7,
  S_SCANDLELIT1,               // 3604
  S_SCANDLELIT2,               // 3605
  S_SCANDLELIT3,               // 3606
  S_SCANDLEUNLIT,              // 3607
  S_SCHANDELIERLIT1,               // 3608
  S_SCHANDELIERLIT2,               // 3608
  S_SCHANDELIERLIT3,               // 3608
  S_SCHANDELIERUNLIT,              // 3609
  S_SHANGINGBODY,              // 3610
  S_SHANGINGBUCKET,            // 3611
  S_SLOG,                  // 3612
  S_SMUSHROOM,                 // 3613
  S_STABLE1,                   // 3614
  S_STABLE2,                   // 3615
  S_STREESWAMP,                // 3616
  S_SBONESANDBLOOD1,               // 3617
  S_SBONESANDBLOOD2,               // 3618
  S_SBONESANDBLOOD3,               // 3619
  S_SCLEAVER,                  // 3620
  S_SSTUMP,                // 3621
  S_SSTAKEOFSKULLS,
  S_SSTAKEOFSKULLS2,
  S_SSTAKEOFSKULLS4,
  S_SSTAKEOFSKULLS3,
  S_SSTAKEOFSKULLS5,
  S_SSTAKEOFSKULLS6,
  S_SSKULL1,                   // 3622
  S_SSKULL2,
  S_SLOG2,
  S_SMUSHROOM2,
  S_STREESWAMP2,
  S_SSTUMP2,
  S_GREENBLOB_STND,
  S_GREENBLOB_RUN1,
  S_GREENBLOB_RUN2,
  S_GREENBLOB_RUN3,
  S_GREENBLOB_RUN4,
  S_GREENBLOB_RUN5,
  S_GREENBLOB_RUN6,
  S_GREENBLOB_RUN7,
  S_GREENBLOB_RUN8,
  S_GREENBLOB_ATK1,
  S_GREENBLOB_ATK2,
  S_GREENBLOB_ATK3,
  S_GREENBLOB_ATK4,
  S_GREENBLOB_PAIN1,
  S_GREENBLOB_PAIN2,
  S_GREENBLOB_DIE1,
  S_GREENBLOB_DIE2,
  S_GREENBLOB_DIE3,
  S_GREENBLOB_DIE4,
  S_GREENBLOB_DIE5,
  S_REDBLOB_STND,
  S_REDBLOB_RUN1,
  S_REDBLOB_RUN2,
  S_REDBLOB_RUN3,
  S_REDBLOB_RUN4,
  S_REDBLOB_RUN5,
  S_REDBLOB_RUN6,
  S_REDBLOB_RUN7,
  S_REDBLOB_RUN8,
  S_REDBLOB_ATK1,
  S_REDBLOB_ATK2,
  S_REDBLOB_ATK3,
  S_REDBLOB_ATK4,
  S_REDBLOB_ATK5,
  S_REDBLOB_ATK6,
  S_REDBLOB_ATK7,
  S_REDBLOB_ATK8,
  S_REDBLOB_ATK9,
  S_REDBLOB_ATK10,
  S_REDBLOB_PAIN1,
  S_REDBLOB_PAIN2,
  S_REDBLOB_DIE1,
  S_REDBLOB_DIE2,
  S_REDBLOB_DIE3,
  S_REDBLOB_DIE4,
  S_REDBLOB_DIE5,
  S_PURPLEBLOB_STND1,
  S_PURPLEBLOB_RUN1,
  S_PURPLEBLOB_RUN2,
  S_PURPLEBLOB_ATK1,
  S_PURPLEBLOB_ATK2,
  S_PURPLEBLOB_ATK3,
  S_PURPLEBLOB_DIE1,
  S_PURPLEBLOB_DIE2,
  S_PURPLEBLOB_DIE3,
  S_PURPLEBLOB_DIE4,
  S_MOTHERBLOB_STND,
  S_MOTHERBLOB_RUN1,
  S_MOTHERBLOB_RUN2,
  S_MOTHERBLOB_RUN3,
  S_MOTHERBLOB_RUN4,
  S_MOTHERBLOB_RUN5,
  S_MOTHERBLOB_RUN6,
  S_MOTHERBLOB_RUN7,
  S_MOTHERBLOB_RUN8,
  S_MOTHERBLOB_ATK1,
  S_MOTHERBLOB_ATK2,
  S_MOTHERBLOB_ATK3,
  S_MOTHERBLOB_ATK4,
  S_MOTHERBLOB_ATK5,
  S_MOTHERBLOB_ATK6,
  S_MOTHERBLOB_DIE1,
  S_MOTHERBLOB_DIE2,
  S_MOTHERBLOB_DIE3,
  S_MOTHERBLOB_DIE4,
  S_UAMMOCLIP,
  S_T3DGLASSES,
  S_TCAP,
  S_TTSHIRT,
  S_TLUNCHBOX,
  S_UCANS,
  S_UFIRSTAIDKIT,
  S_UCAN,
  S_UMEGAHEALTH,
  S_USILVERKEYCARD,
  S_UGOLDKEYCARD,
  S_UTECHLAMP,
  S_UTECHLAMPUNLIT,
  S_UCEILINGLIGHT,
  S_UCEILINGLIGHTUNLIT,
  S_UCEILINGLIGHT2,
  S_UCEILINGLIGHTUNLIT2,
  S_UBARRELGREEN,
  S_UCHAIR,
  S_UCONSOLE,
  S_USTALACTITEICE,
  S_USTALACTITEICE2,
  S_USTALAGMITEICE,
  S_USTALAGMITEICE2,
  S_USLIMECOLUMN,
  S_USINK,
  S_USNOWMAN,
  S_UTOLIET,
  S_UTOLIET2,
  S_UTREE,
  S_UTREE2,
  S_USLIMEPOOL,
  S_USLIMEPOOL2,
  S_WROCKET,                   // 3624
  S_EXPLOSION1,
  S_EXPLOSION2,
  S_EXPLOSION3,
  S_EXPLOSION4,
  S_LROCKET,
  S_CROCKET,
  S_CEXPLOSION1,
  S_CEXPLOSION2,
  S_CONSOLEFLAMETHROWERMISSILE1,
  S_CONSOLEFLAMETHROWERMISSILEX1,
  S_CONSOLEFLAMETHROWERMISSILEX2,
  S_CONSOLEFLAMETHROWERMISSILEX3,
  S_CATAPMISSILE11,            // 3625
  S_CATAPMISSILE12,            // 3626
  S_CATAPMISSILEEXP1,              // 3627
  S_CATAPMISSILEEXP2,              // 3628
  S_CATAPMISSILEEXP3,              // 3629
  S_CATAPMISSILE21,            // 3630
  S_CATAPMISSILE22,            // 3631
  S_UPISTOLMISSILE1,
  S_UPISTOLMISSILEX1,
  S_UPISTOLMISSILEX2,
  S_UPISTOLMISSILEBLUR1,
  S_UMACHINEGUNMISSILE1,
  S_UMACHINEGUNMISSILEX1,
  S_UMACHINEGUNMISSILEX2,
  S_UMACHINEGUNMISSILEBLUR1,
  S_UCHAINGUNMISSILE1,
  S_UCHAINGUNMISSILEX1,
  S_UCHAINGUNMISSILEX2,
  S_UCHAINGUNMISSILEBLUR1,
  S_SCHABBSPROJECTILE1,            // 3632
  S_SCHABBSPROJECTILE2,            // 3633
  S_SCHABBSPROJECTILE3,            // 3634
  S_SCHABBSPROJECTILE4,            // 3635
  S_SCHABBSPROJECTILEX1,           // 3636
  S_SCHABBSPROJECTILEX2,           // 3637
  S_SCHABBSPROJECTILEX3,           // 3638
  S_FAKEHITLERPROJECTILE1,         // 3639
  S_FAKEHITLERPROJECTILE2,         // 3640
  S_FAKEHITLERPROJECTILEX1,        // 3641
  S_FAKEHITLERPROJECTILEX2,
  S_FAKEHITLERPROJECTILEX3,
  S_DEATHKNIGHTMSSILE1,
  S_DEATHKNIGHTMSSILE2,
  S_DEATHKNIGHTMSSILEX1,
  S_DEATHKNIGHTMSSILEX2,
  S_DEATHKNIGHTMSSILEX3,
  S_ANGSHOT1_1,                // 3642
  S_ANGSHOT1_2,                // 3643
  S_ANGSHOT1_3,                // 3644
  S_ANGSHOT1_4,                // 3645
  S_ANGSHOT1_1X,               // 3646
  S_ANGSHOT1_2X,               // 3647
  S_ANGSHOT1_3X,               // 3648
  S_ANGMISSILEBLUR1,
  S_ANGMISSILEBLUR2,
  S_ANGMISSILEBLUR3,
  S_ANGMISSILEBLUR4,
  S_ROBOTPROJECTILE1,
  S_ROBOTPROJECTILEX1,
  S_ROBOTPROJECTILEX2,
  S_ROBOTPROJECTILEX3,
  S_ROBOTMISSILEPUFF1,
  S_ROBOTMISSILEPUFF2,
  S_ROBOTMISSILEPUFF3,
  S_ROBOTMISSILEPUFF4,
  S_ROBOTMISSILESMOKE1,
  S_ROBOTMISSILESMOKE2,
  S_ROBOTMISSILESMOKE3,
  S_ROBOTMISSILESMOKE4,
  S_ROBOTMISSILESMOKE5,
  S_DEVSHOT1_1,                // 3653
  S_DEVSHOT1_2,                // 3654
  S_DEVSHOT1_3,                // 3655
  S_DEVSHOT1_4,                // 3656
  S_DEVSHOT1_1X,               // 3657
  S_DEVSHOT2_1,                // 3658
  S_DEVSHOT2_2,                // 3659
  S_DEVSHOT2_3,                // 3660
  S_DEVSHOT2_4,                // 3661
  S_CATASHOT1_1,               // 3662
  S_CATASHOT1_2,               // 3663
  S_CATASHOT1_1X,              // 3664
  S_CATASHOT2_1,               // 3665
  S_CATASHOT2_2,               // 3666
  S_NEMESISMISSILE1,
  S_NEMESISMISSILE2,
  S_NEMESISMISSILEX1,
  S_NEMESISMISSILEX2,
  S_NEMESISMISSILEX3,
  S_BIOBLASTERPROJECTILE1,         // 3667
  S_BIOBLASTERPROJECTILE2,         // 3668
  S_BIOBLASTERPROJECTILE3,         // 3668
  S_BIOBLASTERPROJECTILEX1,        // 3669
  S_BIOBLASTERPROJECTILEX2,        // 3670
  S_BIOBLASTERPROJECTILEX3,        // 3671
  S_MADDOCPROJECTILE1,             // 3672
  S_MADDOCPROJECTILE2,             // 3673
  S_MADDOCPROJECTILE3,             // 3674
  S_MADDOCPROJECTILE4,             // 3675
  S_MADDOCPROJECTILEX1,            // 3676
  S_MADDOCPROJECTILEX2,            // 3677
  S_MADDOCPROJECTILEX3,            // 3678
  S_MONATTAK1_1,               // 3679
  S_MONATTAK1_1X,              // 3680
  S_MONATTAK1_2X,              // 3681
  S_MONATTAK1_3X,              // 3682
  S_MONATTAK1_4X,              // 3683
  S_MONATTAK1_5X,              // 3684
  S_MONATTAK2_1,               // 3685
  S_MONATTAK2_2,               // 3686
  S_MONATTAK2_3,               // 3687
  S_MONATTAK2_4,               // 3688
  S_MONATTAK2_5,               // 3689
  S_MONATTAK2_6,               // 3690
  S_MONATTAK2_7,               // 3691
  S_MONATTAK2_8,               // 3692
  S_MONATTAK2_9,               // 3693
  S_MONATTAK2_1X,              // 3694
  S_MONATTAK2_2X,              // 3695
  S_MONATTAK2_3X,              // 3696
  S_MONATTAK2_4X,              // 3697
  S_MONATTAK2_5X,              // 3698
  S_MONATTAK2_6X,              // 3699
  S_BALROGPRO1,                // 3700
  S_BALROGPRO2,                // 3701
  S_BALROGPROX1,
  S_SERPENT_FX1,               // 3702
  S_SERPENT_FX2,               // 3703
  S_SERPENT_FX3,               // 3704
  S_SERPENT_FX4,               // 3705
  S_SERPENT_FX_X1,             // 3706
  S_SERPENT_FX_X2,             // 3707
  S_SERPENT_FX_X3,             // 3708
  S_SERPENT_FX_X4,             // 3709
  S_SERPENT_FX_X5,             // 3710
  S_SERPENT_FX_X6,             // 3711
  S_SCHABBSDEMONPROJECTILE1,
  S_SCHABBSDEMONPROJECTILE2,
  S_GREENBLOBMISSILE1,
  S_GREENBLOBMISSILEX1,
  S_GREENBLOBMISSILEX2,
  S_REDBLOBMISSILE1,
  S_REDBLOBMISSILEX1,
  S_REDBLOBMISSILEX2,
  S_LIGHTNINGA,                // 3712
  S_LIGHTNINGAB,               // 3713
  S_LIGHTNINGAC,               // 3714
  S_LORRYA,                // 3715
  S_WATERDA,                   // 3716
  S_WATERDAB,                  // 3717
  S_WATERDAC,                  // 3718
  S_WOLFHOWLA,                 // 3719
  S_WOLFHOWLAB,                // 3720
  S_WOLFHOWLAC,                // 3721
  S_CRICKETA,                  // 3722
  S_CRICKETAB,                 // 3723
  S_CRICKETAC,                 // 3724
  S_BIRDS1A,                   // 3725
  S_BIRDS1AB,                  // 3726
  S_BIRDS1AC,                  // 3727
  S_BIRDS2A,                   // 3728
  S_BIRDS2AB,                  // 3729
  S_BIRDS2AC,                  // 3730
  S_WINDA,                 // 3731
  S_COMPHUMA1,                 // 3732
  S_COMPHUMA2,                 // 3733
  S_DOORCLOSEA1,               // 3734
  S_DOORCLOSEA2,               // 3735
  S_ROCKSA,                // 3736
  S_ROCKSAB,                   // 3737
  S_ROCKSAC,                   // 3738
  S_FROGSA,                // 3739
  S_FROGSAB,                   // 3740
  S_FROGSAC,                   // 3741
  S_SCREECHA,                  // 3742
  S_SCREECHAB,                 // 3743
  S_SCREECHAC,                 // 3744
  S_WATERA,                // 3741
  S_WATERA2,                   // 3741
  S_RAINA,                 // 3741
  S_ELECTRICITYA,
  S_OWLA,
  S_PUFF1,
  S_PUFF2,
  S_PUFF3,
  S_PUFF4,
  S_BLOOD1,
  S_BLOOD2,
  S_BLOOD3,
    S_TFOG1,                       // 133
    S_TFOG2,                       // 133
    S_TFOG3,                       // 134
    S_TFOG4,                       // 135
    S_TFOG5,                       // 136
    S_TFOG6,                       // 137
    S_TFOG7,                       // 138
    S_TFOG8,                       // 139
    S_TFOG9,                       // 140
    S_TFOG10,                      // 141
    S_TFOG11,                      // 141
    S_IFOG1,                       // 131
    S_IFOG2,                       // 133
    S_IFOG3,                       // 134
    S_IFOG4,                       // 135
    S_IFOG5,                       // 136
    S_IFOG6,                       // 137
    S_IFOG7,                       // 138
    S_IFOG8,                       // 139
    S_IFOG9,                       // 140
    S_IFOG10,                      // 141
    S_IFOG11,
  S_GIBSR1,
  S_GIBSR2,
  S_GIBSP1,                // 3745
  S_GIBSP2,                // 3746
  S_GIBSM1,                // 3747
  S_GIBSM2,                // 3748
  S_PACMANSPAWNERLOOK,             // 3758
  S_PACMANSPAWNERSEE,              // 3759
  S_PACMANSPAWNER1,            // 3760
  S_PACMANSPAWNER2,            // 3760
  S_PACMANSWASTIKA1,               // 3761
  S_PACMANSWASTIKA2,               // 3762
  S_PACMANBJHEAD1,             // 3763
  S_PACMANBJHEAD2,             // 3764
  S_PACMANBJHEAD3,             // 3763
  S_PACMANBJHEAD4,             // 3764
  S_RADIATIONSUIT,
  S_BACKPACK,
  S_RAINSPAWNER,
  S_RAINDROP1,
  S_RAINDROP2,
  S_RAINDROP3,
  S_RAINDROP4,
  S_SMOKEP1,
  S_DIE1,
    NUMSTATES               // 3765
} statenum_t;

// Map objects.
typedef enum {
    MT_POSSESSED,                  // 001
    MT_SHOTGUY,                    // 002
    MT_VILE,                       // 003
    MT_FIRE,
    MT_UNDEAD,                     // 005
    MT_TRACER,                     // 006
    MT_SMOKE,                      // 007
    MT_FATSO,                      // 008
    MT_FATSHOT,                    // 009
    MT_CHAINGUY,                   // 010
    MT_TROOP,                      // 011
    MT_SERGEANT,                   // 012
    MT_SHADOWS,                    // 013
    MT_HEAD,                       // 014
    MT_BRUISER,                    // 015
    MT_BRUISERSHOT,                // 016
    MT_KNIGHT,                     // 017
    MT_SKULL,                      // 018
    MT_SPIDER,                     // 019
    MT_BABY,                       // 020
    MT_CYBORG,                     // 021
    MT_PAIN,                       // 022
    MT_WOLFSS,                     // 023
    MT_KEEN,                       // 024
    MT_BOSSBRAIN,                  // 025
    MT_BOSSSPIT,                   // 026
    MT_SPAWNSHOT,                  // 028
    MT_SPAWNFIRE,                  // 029
    MT_BARREL,                     // 030
    MT_TROOPSHOT,                  // 031
    MT_HEADSHOT,                   // 032
    MT_ROCKET,                     // 033
    MT_PLASMA,                     // 034
    MT_BFG,                        // 035
    MT_ARACHPLAZ,                  // 036
    MT_EXTRABFG,                   // 042
    MT_MISC0,                      // 043
    MT_MISC1,                      // 044
    MT_MISC2,                      // 045
    MT_MISC3,                      // 046
    MT_MISC4,                      // 047
    MT_MISC5,                      // 048
    MT_MISC6,                      // 049
    MT_MISC7,                      // 050
    MT_MISC8,                      // 051
    MT_MISC9,                      // 052
    MT_MISC10,                     // 053
    MT_MISC11,                     // 054
    MT_MISC12,                     // 055
    MT_INV,                        // 056
    MT_MISC13,                     // 057
    MT_INS,                        // 058
    MT_MISC15,                     // 060
    MT_MISC16,                     // 061
    MT_MEGA,                       // 062
    MT_CLIP,                       // 063
    MT_MISC17,                     // 064
    MT_MISC18,                     // 065
    MT_MISC19,                     // 066
    MT_MISC20,                     // 067
    MT_MISC21,                     // 068
    MT_MISC22,                     // 069
    MT_MISC23,                     // 070
    MT_MISC29,                     // 079
    MT_MISC30,                     // 080
    MT_MISC31,                     // 081
    MT_MISC32,                     // 082
    MT_MISC33,                     // 083
    MT_MISC34,                     // 084
    MT_MISC35,                     // 085
    MT_MISC36,                     // 086
    MT_MISC37,                     // 087
    MT_MISC38,                     // 088
    MT_MISC39,                     // 089
    MT_MISC40,                     // 090
    MT_MISC41,                     // 091
    MT_MISC42,                     // 092
    MT_MISC43,                     // 093
    MT_MISC44,                     // 094
    MT_MISC45,                     // 095
    MT_MISC46,                     // 096
    MT_MISC47,                     // 097
    MT_MISC48,                     // 098
    MT_MISC49,                     // 099
    MT_MISC50,                     // 100
    MT_MISC51,                     // 101
    MT_MISC52,                     // 102
    MT_MISC53,                     // 103
    MT_MISC54,                     // 104
    MT_MISC55,                     // 105
    MT_MISC56,                     // 106
    MT_MISC57,                     // 107
    MT_MISC58,                     // 108
    MT_MISC59,                     // 109
    MT_MISC60,                     // 110
    MT_MISC61,                     // 111
    MT_MISC62,                     // 112
    MT_MISC63,                     // 113
    MT_MISC64,                     // 114
    MT_MISC65,                     // 115
    MT_MISC66,                     // 116
    MT_MISC67,                     // 117
    MT_MISC68,                     // 118
    MT_MISC69,                     // 119
    MT_MISC70,                     // 120
    MT_MISC72,                     // 122
    MT_MISC73,                     // 123
    MT_MISC74,                     // 124
    MT_MISC75,                     // 125
    MT_MISC76,                     // 126
    MT_MISC77,                     // 127
    MT_MISC78,                     // 128
    MT_MISC79,                     // 129
    MT_MISC80,                     // 130
    MT_MISC81,                     // 131
    MT_MISC82,                     // 132
    MT_MISC83,                     // 133
    MT_MISC84,                     // 134
    MT_MISC85,                     // 135
    MT_MISC86,                     // 136
    MT_LIGHTSOURCE,                // 137
    MT_TEMPSOUNDORIGIN,            // 138
    MT_ROCKETPUFF,                 // 139
    MT_PLAYER,
    MT_GUARD,               // 140
    MT_SSGUARD,             // 141
    MT_DOG,                 // 142
    MT_MUTANT,              // 143
    MT_OFFICER,             // 144
    MT_HANSGROSSE,              // 145
    MT_SCHABBS,             // 146
    MT_FAKEHITLER,              // 147
    MT_MECHHITLER,              // 148
    MT_HITLER,              // 149
    MT_PACMANPINK,              // 150
    MT_PACMANPINKBLUR1,         // 151
    MT_PACMANPINKBLUR2,         // 152
    MT_PACMANRED,               // 153
    MT_PACMANREDBLUR1,          // 154
    MT_PACMANREDBLUR2,          // 155
    MT_PACMANORANGE,            // 157
    MT_PACMANORANGEBLUR1,           // 158
    MT_PACMANORANGEBLUR2,           // 159
    MT_PACKMANBLUE,             // 160
    MT_PACMANBLUEBLUR1,         // 161
    MT_PACMANBLUEBLUR2,         // 162
    MT_OTTOGRIFTMAKER,          // 163
    MT_GRETTELGROOSE,           // 164
    MT_FATFACE,             // 165
    MT_TRANSHANS,               // 166
    MT_WILLHELM,                // 167
    MT_UBERMUTANT,              // 168
    MT_DEATHKNIGHT,             // 169
    MT_GHOST,               // 170
    MT_ANGELOFDEATH,            // 171
    MT_ANGELFIRE,
    MT_GHOSTR,              // 172
    MT_FANSGROSSE,              // 173
    MT_PANSGROSSE,              // 174
    MT_NONBOSSHANSGROOSE,           // 175
    MT_MELEEMUTANT,             // 176
    MT_FIREBALLMUTANT,          // 177
    MT_OFFICER2,                // 178
    MT_OFFICER2GK,              // 179
    MT_OFFICER2SK,              // 180
    MT_RAT,                 // 181
    MT_FLAMEGUARD,              // 182
    MT_ELITEGUARD,              // 183
    MT_SPEAROFDESTINY,          // 184
    MT_TREASURECROSS,           // 185
    MT_TREASURECUP,             // 186
    MT_TREASURECHEST,           // 187
    MT_TREASURECROWN,           // 188
    MT_DINER,               // 189
    MT_FIRSTAIDKIT,             // 190
    MT_DOGFOOD,             // 191
    MT_MEGAHEALTH,              // 192
    MT_AMMOCLIP,                // 193
    MT_AMMOBOX,             // 194
    MT_SILVERKEY,               // 195
    MT_GOLDKEY,             // 196
    MT_ROCKETAMMO,              // 197
    MT_ROCKETBOXAMMO,           // 198
    MT_FLAMETHROWERAMMOS,           // 199
    MT_FLAMETHROWERAMMOL,           // 200
    MT_CANDELABRALIT,           // 201
    MT_CANDELABRAUNLIT,         // 202
    MT_CHANDELIERLIT,           // 203
    MT_CHANDELIERUNLIT,         // 204
    MT_GREYLIGHTYEL,            // 205
    MT_GREYLIGHTWHT,            // 206
    MT_GREYLIGHTUNLIT,          // 207
    MT_REDLIGHTLIT,             // 208
    MT_REDLIGHTUNLIT,           // 209
    MT_BARRELGREEN,             // 210
    MT_BARRELWOOD,              // 211
    MT_BED,                 // 212
    MT_BOILER,              // 213
    MT_CAGEBLOODY,              // 214
    MT_CAGEEMPTY,               // 215
    MT_CAGESITTINGSKEL,         // 216
    MT_CAGESKULL,               // 217
    MT_COLUMNBEIGE,             // 218
    MT_COLUMNWHITE,             // 219
    MT_FLAG,                // 220
    MT_KNIGHTS,             // 221
    MT_LORRY,               // 222
    MT_PLANTFERNB,              // 223
    MT_POTPLANT,                // 224
    MT_SKELETONHANG,            // 225
    MT_SINK,                // 226
    MT_SPEARRACK,               // 227
    MT_STAKEOFSKULLS,           // 228
    MT_STAKESKULL,              // 229
    MT_STATUEANGELI,            // 230
    MT_TABLEPOLE,               // 231
    MT_TABLEWOOD1,              // 232
    MT_VASEBLUE,                // 233
    MT_WHITEWELLFULL,           // 234
    MT_WHITEWELLEMPTY,          // 235
    MT_HITERWELL,               // 236
    MT_ARDWOLF,             // 237
    MT_BASKET,              // 238
    MT_BONES1,              // 239
    MT_BONES2,              // 240
    MT_BONES3,              // 241
    MT_BONESANDBLOOD,           // 242
    MT_BONESPILE,               // 243
    MT_DEADGUARD,               // 244
    MT_PANS1,               // 245
    MT_PANS2,               // 246
    MT_POOLBLOOD,               // 247
    MT_POOLWATER,               // 248
    MT_SKELETON,                // 249
    MT_BARRELGREENWATER,            // 250
    MT_BARRELWOODWATER,         // 251
    MT_CAGESITINGSKELLEGLESS,       // 252
    MT_CAGESITINGSKELONELEG,        // 253
    MT_CAGEEMPTY2,              // 254
    MT_CAGEEMPTY3,              // 254
    MT_CAGEEMPTY4,              // 254
    MT_CAGEBLOODY2,             // 255
    MT_CAGEBLOODY3,             // 256
    MT_CAGEBLOODYDRIPPING1,         // 257
    MT_CAGEBLOODYDRIPPING2,         // 258
    MT_CAGESKULL2,              // 259
    MT_CAGESKULL3,              // 260
    MT_CAGESKULL4,              // 261
    MT_CAGESKULLDRIPPING1,          // 262
    MT_CAGESKULLDRIPPING2,          // 263
    MT_KNIGHT2,             // 266
    MT_KNIGHT3,             // 267
    MT_PANS3,               // 268
    MT_PANS4,               // 269
    MT_HANGINGSKELETON2,            // 270
    MT_HANGINGSKELETON3,            // 271
    MT_HANGINGSKELETON4,            // 272
    MT_HANGINGSKELETON5,            // 273
    MT_HANGINGSKELETON6,            // 274
    MT_POTPLANT2,               // 275
    MT_POTPLANTPOT,             // 276
    MT_FERNPOT,             // 277
    MT_FERN2,               // 278
    MT_FERNPOT2,                // 279
    MT_STAKEOFSKULLS2,          // 280
    MT_STAKEOFSKULLS3,          // 281
    MT_STAKEOFSKULLS4,          // 282
    MT_STAKEOFSKULLS5,          // 283
    MT_STAKEOFSKULLS6,          // 284
    MT_SINK2,               // 285
    MT_SKELETONONELEG,          // 286
    MT_SPEARRACK2,              // 287
    MT_SPEARRACK3,              // 288
    MT_SPEARRACK4,              // 289
    MT_SPEARRACK5,              // 290
    MT_SPEARRACK6,              // 291
    MT_STATUEANGEL,             // 292
    MT_POLETABLECHAIRS,         // 293
    MT_POLETABLECHAIRS2,            // 294
    MT_POLETABLECHAIRS3,            // 295
    MT_POLETABLECHAIRS4,            // 296
    MT_WOODTABLE2,              // 297
    MT_WOODTABLE3,              // 298
    MT_WOODTABLE4,              // 299
    MT_WOODTABLE5,              // 300
    MT_WOODTABLE6,              // 301
    MT_WOODTABLE7,              // 302
    MT_WOODTABLE8,              // 303
    MT_WOODTABLE9,              // 304
    MT_WOODTABLE10,             // 305
    MT_WOODTABLE11,             // 306
    MT_WOODTABLE12,             // 307
    MT_WOODTABLE13,             // 308
    MT_WOODTABLE14,             // 309
    MT_WOODTABLE15,             // 310
    MT_WOODTABLE16,             // 311
    MT_VASEBLUE2,               // 312
    MT_VASEBLUE3,               // 313
    MT_DEADSSGUARD,             // 314
    MT_DEADDOG,             // 315
    MT_BUSH1,               // 316
    MT_TREE1,               // 317
    MT_TREE2,               // 318
    MT_TREE3,               // 319
    MT_TREE4,               // 317
    MT_TREE5,               // 318
    MT_TREE6,               // 319
    MT_OUTSIDEROCK,             // 318
    MT_OUTSIDEROCK2,            // 319
    MT_HCHAIN1,             // 320
    MT_CHAINBALL1,              // 321
    MT_CHAINBALL2,              // 322
    MT_CHAINHOOK1,              // 323
    MT_CHAINHOOK2,              // 324
    MT_CHAINHOOK3,              // 325
    MT_CHAINHOOK4,              // 326
    MT_CHAINHOOKBLOODY1,            // 327
    MT_CHAINBLOODY2,            // 328
    MT_LIGHTNING,               // 329
    MT_WALLTORCH,               // 335
    MT_WALLTORCHUNLIT,          // 336
    MT_WATERPLUME,              // 337
    MT_BLOODWELL,               // 338
    MT_BLOODWELL2,              // 339
    MT_BLOODWELL3,              // 340
    MT_BLOODWELL4,              // 341
    MT_SYRINGEG,                // 334
    MT_WOODTABLECHAIR1,         // 330
    MT_WOODTABLECHAIR2,         // 331
    MT_WOODTABLECHAIR3,         // 332
    MT_WOODTABLECHAIR4,         // 333
    MT_CAGESKULLFLOORGIBS,          // 264
    MT_CAGESKULLFLOORGIBS2,         // 265
    MT_ACEVENTURA,              // 342
    MT_BATMAN,              // 343
    MT_DOOMGUY,             // 344
    MT_DUKENUKEM,               // 345
    MT_JOKER,               // 346
    MT_KEENS,               // 347
    MT_RAMBO,               // 348
    MT_RIDDLER,             // 349
    MT_SPIDERMAN,               // 350
    MT_SUPERERMAN,              // 351
    MT_TWOFACE,             // 352
    MT_ZORRO,               // 353
    MT_LOSTGUARD,               // 354
    MT_LOSTSSGUARD,             // 355
    MT_LOSTDOG,             // 356
    MT_LOSTBAT,             // 357
    MT_LOSTOFFICER,             // 358
    MT_WILLY,               // 359
    MT_QUARK,               // 360
    MT_AXE,                 // 361
    MT_ROBOT,               // 362
    MT_LOSTGHOST,               // 363
    MT_GREENMIST,               // 364
    MT_DEVILINCARNATE,          // 365
    MT_DEVILFIRE,
    MT_LOSTGHOSTR,              // 366
    MT_GREENMISTR,              // 367
    MT_SPIRIT,              // 368
    MT_LOSTMISSILEBAT,          // 369
    MT_LOSTFLAMEGUARD,          // 370
    MT_LOSTELITEGUARD,          // 371
    MT_LOSTELITEGUARDCOM,           // 372
    MT_SECURITYROBOT,           // 373
    MT_LUTHER,              // 374
    MT_ASSASIN,             // 375
    MT_EVILBJ,              // 376
    MT_DEVILINCARNATE2,         // 377
    MT_NONBOSSSECURITYROBOT,        // 378
    MT_LOSTEPSPEAROFDESTINY,        // 379
    MT_LOSTTREASURERADIO,           // 380
    MT_LOSTTREASUREPLUTONIUM,       // 381
    MT_LOSTTREASURETIMER,           // 382
    MT_LOSTTREASUREBOMB,            // 383
    MT_LOSTDINER,               // 384
    MT_LOSTFIRSTAIDKIT,         // 385
    MT_LOSTDOGFOOD,             // 386
    MT_LOSTMEGAHEALTH,          // 387
    MT_LOSTAMMOCLIP,            // 388
    MT_LOSTAMMOBOX,             // 389
    MT_LOSTSILVERKEY,           // 390
    MT_LOSTGOLDKEY,             // 391
    MT_LOSTROCKETAMMO,
    MT_LOSTROCKETBOXAMMO,
    MT_LOSTFLAMETHROWERAMMOS,
    MT_LOSTFLAMETHROWERAMMOL,
    MT_LOSTGREENLIGHTLIT,           // 392
    MT_LOSTGREENLIGHTUNLIT,         // 393
    MT_LOSTREDLIGHTLIT,         // 394
    MT_LOSTREDLIGHTUNLIT,           // 395
    MT_LOSTBULBLIT,             // 396
    MT_LOSTBULBUNLIT,           // 397
    MT_LOSTCHANDELIERLIT,           // 398
    MT_LOSTCHANDELIERUNLIT,         // 399
    MT_LOSTCANDELABRALIT,           // 400
    MT_LOSTCANDELABRAUNLIT,         // 401
    MT_LOSTTABLESILVER,         // 402
    MT_LOSTTABLESTEEL,          // 403
    MT_LOSTSTONECOLUMN,         // 404
    MT_LOSTPIPECOULMN,          // 405
    MT_LOSTELECTROCOLUMN,           // 406
    MT_LOSTHORIZBROWNBARREL,        // 407
    MT_LOSTHORIZGREENBARREL,        // 408
    MT_LOSTHORIZYELLOWBARREL,       // 409
    MT_LOSTWELLFULL,            // 410
    MT_LOSTWELLEMPTY,           // 411
    MT_LOSTPLAMTREE,            // 412
    MT_LOSTFERN,                // 413
    MT_LOSTBLUEVASE,            // 414
    MT_LOSTARMOUREDSKELETON,        // 415
    MT_LOSTDEVILSTATUE,         // 416
    MT_LOSTHANGINGSKELETON,         // 417
    MT_LOSTEMPTYCAGE,           // 418
    MT_LOSTBROKENCAGE,          // 419
    MT_LOSTSKELETONCAGE,            // 420
    MT_LOSTBLOODYCAGE,          // 421
    MT_LOSTCONICAL,             // 422
    MT_LOSTBJSIGN,              // 423
    MT_LOSTBUBBLES,             // 424
    MT_LOSTBONES,               // 425
    MT_LOSTSKELETON,            // 426
    MT_LOSTPILEOFSKULLS,            // 427
    MT_LOSTBLOODBONES,          // 428
    MT_LOSTBONESWATER,          // 429
    MT_LOSTWATERPOOL,           // 430
    MT_LOSTBLOODPOOL,           // 431
    MT_LOSTSLIMEPOOL,           // 432
    MT_LOSTDEADRAT,             // 433
    MT_LOSTDEADGUARD,           // 434
    MT_LOSTTABLESTEEL2,         // 435
    MT_LOSTTABLESTEEL3,         // 436
    MT_LOSTTABLESTEEL4,         // 437
    MT_LOSTTABLESTEEL5,         // 438
    MT_LOSTTABLESTEEL6,         // 439
    MT_LOSTTABLESTEEL7,         // 440
    MT_LOSTTABLESTEEL8,         // 441
    MT_LOSTTABLESTEEL9,         // 442
    MT_LOSTTABLESTEEL10,            // 443
    MT_LOSTTABLESTEEL11,            // 444
    MT_LOSTTABLESTEEL12,            // 445
    MT_LOSTTABLESTEEL13,            // 446
    MT_LOSTTABLESTEEL14,            // 447
    MT_LOSTTABLESTEEL15,            // 448
    MT_LOSTTABLESTEEL16,            // 449
    MT_LOSTPIPECOULMN2,         // 450
    MT_LOSTPIPECOULMN3,         // 451
    MT_LOSTPIPECOULMN4,         // 452
    MT_LOSTHORIZBROWNBARREL2,       // 453
    MT_LOSTHORIZBROWNBARRELWIDE,        // 454
    MT_LOSTHORIZGREENBARRELWIDE,        // 455
    MT_LOSTPLAMTREE2,           // 456
    MT_LOSTPLAMTREE3,           // 457
    MT_LOSTPLAMTREE4,           // 458
    MT_LOSTPALMPOT,             // 459
    MT_LOSTFERNPOT,             // 460
    MT_LOSTHANGINGSKELETON2,        // 461
    MT_LOSTHANGINGSKELETON3,        // 462
    MT_LOSTHANGINGSKELETON4,        // 463
    MT_LOSTHANGINGSKELETON5,        // 464
    MT_LOSTHANGINGSKELETON6,        // 465
    MT_LOSTEMPTYCAGE2,          // 466
    MT_LOSTBROKENCAGEBLOOD,         // 467
    MT_LOSTBROKENCAGEBLOOD2,        // 468
    MT_LOSTBROKENCAGEBLOOD3,        // 469
    MT_LOSTBROKENCAGETOP,           // 470
    MT_LOSTSKELETONCAGEDRIPPING,        // 471
    MT_LOSTBLOODYCAGEDRIPPING,      // 472
    MT_LOSTCONICAL2,            // 474
    MT_LOSTCONICAL3,            // 474
    MT_LOSTTABLESTEELCHAIR,         // 475
    MT_LOSTTABLESTEELCHAIR2,        // 476
    MT_LOSTSTALACTITEICEMEDIUM,     // 477
    MT_LOSTLSTALACTITEICESMALL,     // 478
    MT_LOSTLSTALACTITEICETINY,      // 479
    MT_LOSTSTALAGMITEICEMEDIUM,     // 480
    MT_LOSTSTALAGMITEICESMALL,      // 481
    MT_LOSTSTALAGMITEICETINY,       // 482
    MT_LOSTBLOODYCAGEFLOORGIBS,     // 473
    MT_ALPHAGUARD,              // 483
    MT_ALPHAAMMOCLIP,           // 484
    MT_ALPHACROSS,              // 485
    MT_ALPHACUP,                // 486
    MT_ALPHATREASUREPILE,           // 487
    MT_ALPHACROWN,              // 488
    MT_ALPHACHANDELIERU,            // 489
    MT_ALPHACHANDELIERL,            // 490
    MT_APLHAVASE,               // 491
    MT_ALPHACOLUMN,             // 492
    MT_CATABAT,             // 493
    MT_CBLACKDEMON,             // 494
    MT_CBLACKDEMONINVISIBLE,        // 494
    MT_CATABLUEDEMON,           // 493
    MT_CATAEYE,             // 494
    MT_CATAMAGE,                // 495
    MT_CATAORC,             // 495
    MT_CATAREDDEMON,            // 496
    MT_CATASKELETON,            // 497
    MT_CATASKELETONH,
    MT_CATASKELETONR,
    MT_CATASKELETON2,           // 498
    MT_CATASKELETONH2,
    MT_CATASKELETONR2,
    MT_CATATROLL,               // 496
    MT_CATAWATERTROLL,          // 496
    MT_CATAWATERTROLLUNDERWATER,        // 496
    MT_CATAZOMBIE,              // 499
    MT_CATAZOMBIEH,             // 500
    MT_CATAZOMBIE2,             // 501
    MT_CATAZOMBIEH2,            // 502
    MT_CATANEMESIS,             // 503
    MT_CATASHADOWNEMESIS,           // 503
    MT_CATASHADOWNEMESISR,          // 503
    MT_CATABLUEDEMONSHADOW,
    MT_CATAHVIAL,               // 504
    MT_CATABLUEKEY,             // 505
    MT_CATAGREENKEY,            // 505
    MT_CATAREDKEY,              // 506
    MT_CATAYELLOWKEY,           // 507
    MT_CATACOMBFIREORBSMALL,        // 508
    MT_CATACOMBFIREORBLARGE,        // 509
    MT_CATASCROLL1,             // 506
    MT_CATASCROLL2,             // 506
    MT_CATASCROLL3,             // 506
    MT_CATASCROLL4,             // 506
    MT_CATASCROLL5,             // 506
    MT_CATASCROLL6,             // 506
    MT_CATASCROLL7,             // 506
    MT_CATASCROLL8,             // 506
    MT_CATACHEST,               // 510
    MT_CATACHESTWATER,          // 510
    MT_CATAGRAVE1,              // 511
    MT_CATAGRAVE2,              // 512
    MT_CATAGRAVE3,              // 513
    MT_CATAWELL,                // 514
    MT_CATACHESTSMALL,          // 515
    MT_CATACHESTMED,            // 516
    MT_CATACHESTLARGE,          // 517
    MT_CATACHESTSMALLWATER,         // 515
    MT_CATACHESTMEDWATER,           // 516
    MT_CATACHESTLARGEWATER,         // 517
    MT_CATACEILINGLIGHTLIT,         // 516
    MT_CATACEILINGLIGHTUNLIT,       // 517
    MT_CATABLOODMIST,           // 518
    MT_CATABLOODMIST2,          // 519
    MT_PORTALLIGHT,             // 519
    MT_MUTANTPLAYINGDEAD,           // 520
    MT_OMSSSGUARD,              // 521
    MT_OMSMELEEMUTANT,          // 522
    MT_OMSMUTANT,               // 523
    MT_WINGEDMENACE,            // 524
    MT_RAVEN,               // 525
    MT_MADDOCTOR,               // 526
    MT_BIOBLASTER,              // 527
    MT_BRAINDROID,              // 528
    MT_CYBORGWARRIOR,           // 529
    MT_MUTANTCOMMANDO,          // 530
    MT_MULLER,              // 531
    MT_CHIMERASTAGE1,           // 532
    MT_OMS2MGUARD,              // 533
    MT_DIRTDEVIL,               // 534
    MT_OMSDIRTDEVILTRAIL,
    MT_OSSAGENT,                // 535
    MT_SILVERFOX,               // 536
    MT_BALROG,              // 537
    MT_OMSMECHSENTINEL,         // 538
    MT_OMSNEMESIS,              // 539
    MT_MUTANTX,             // 540
    MT_OMEGAMUTANT,             // 541
    MT_NONBOSSCYBORGWARRIOR,        // 542
    MT_NONBOSSMUTANTCOMMANDO,       // 543
    MT_NONBOSSBRAINDROID,           // 544
    MT_CHIMERASTAGE2,           // 545
    MT_CHIMERATENTACLE,         // 546
    MT_TENTACLE,                // 547
    MT_OMSFIREBALLMUTANT,           // 548
    MT_MUTANT2PLAYINGDEAD,          // 549
    MT_MUTANT3PLAYINGDEAD,          // 550
    MT_NONBOSSOMSMECHSENTINEL,      // 551
    MT_NONBOSSOMSNEMESIS,           // 552
    MT_NOBOSSMUTANTX,           // 553
    MT_NONBOSSOMEGAMUTANT,          // 554
    MT_OMSFIREDRAKE,            // 554
    MT_OMSFIREDRAKEHEAD,            // 554
    MT_OMSMINIFIRSTAIDKIT,          // 555
    MT_OMSMEGAHEALTH,           // 556
    MT_OMSAMMOCLIP,             // 557
    MT_OMSTREASUREPILE,         // 558
    MT_OMSROCKETAMMO,           // 557
    MT_OMSFLAMETHROWERAMMOS,
    MT_OMSTORCH,                // 559
    MT_OMSTORCHU,               // 560
    MT_OMSBARRELGREY,           // 561
    MT_OMSBARRELGREYBURNING,        // 562
    MT_OMSMACHINE,              // 563
    MT_OMSROCK,             // 564
    MT_OMSSTALAGMITEC,          // 565
    MT_OMSSTALAGMITEF,          // 566
    MT_OMSTABLEBLOODY,          // 567
    MT_OMSTABLEMUTANT,          // 568
    MT_OMSTENTACLEWELL,         // 569
    MT_OMSCONICALFLASK,         // 570
    MT_OMSDESK,             // 571
    MT_OMSKNIGHT,               // 572
    MT_OMSCOLUMNWHITE,          // 573
    MT_OMSSPEARS,               // 574
    MT_OMSTABLEEMPTY,           // 575
    MT_OMSTOLIET,               // 576
    MT_OMSDEADMUTANT,           // 577
    MT_OMSDEADMULLER,           // 578
    MT_OMSDEADSSGUARD,          // 579
    MT_OMSBROKENPILLAR,         // 580
    MT_OMSELETROPILLAROFF,          // 581
    MT_OMSSLIMEPOOL,            // 582
    MT_OMSDEADMGUNGUARD,            // 583
    MT_OMSDEADOSSAGENT,         // 584
    MT_OMSCANDELABRA,           // 585
    MT_OMSCANDELABRAU,          // 586
    MT_OMSBJHOSTAGE,            // 587
    MT_ISTGUARD,                // 588
    MT_ISTMUTANT,               // 589
    MT_ISTOFFICER,              // 590
    MT_ISTMELEEMUTANT,          // 591
    MT_ISTFIREBALLMUTANT,           // 592
    MT_ISTOFFICER2,             // 593
    MT_NONBOSSUBER,             // 594
    MT_NONBOSSUBER2,            // 594
    MT_ANGELOFDEATH2,           // 595
    MT_ISTFLAMEGUARD,           // 593
    MT_ISTCROSS,                // 596
    MT_ISTCUP,              // 597
    MT_ISTCHEST,                // 598
    MT_ISTCROWN,                // 599
    MT_ISTFIRSTAIDKIT,          // 600
    MT_ISTMEGAHEALTH,           // 601
    MT_ISTAMMOCLIP,             // 602
    MT_ISTAMMOBOX,              // 603
    MT_ISTROCKETAMMO,
    MT_ISTROCKETBOXAMMO,
    MT_ISTFLAMETHROWERAMMOS,
    MT_ISTFLAMETHROWERAMMOL,
    MT_ISTCANDELABRALIT,            // 604
    MT_ISTCANDELABRAUNLIT,          // 605
    MT_ISTCEILINGLIGHTBLUELITYEL,       // 606
    MT_ISTCEILINGLIGHTBLUELITWHT,       // 606
    MT_ISTCEILINGLIGHTBLUEUNLIT,        // 607
    MT_ISTKNIGHT,               // 608
    MT_ISTTABLEWOOD,            // 609
    MT_ISTTABLE2,               // 610
    MT_ISTCEILINGFAN,           // 611
    MT_ISTKNIGHT2,              // 608
    MT_ISTKNIGHT3,              // 608
    MT_ISTTABLEWOOD2,           // 609
    MT_ISTTABLEWOOD3,           // 609
    MT_ISTTABLE5,               // 610
    MT_ISTCEILINGFANON,         // 611
    MT_DOOMGUYB,                // 612
    MT_CHAINGUNZOMBIE,          // 613
    MT_SAELMUTANT,
    MT_SERPENT,             // 614
    MT_UBERARMOUR,              // 615
    MT_POOPDECK,                // 616
    MT_TANKMAN,             // 617
    MT_PHANTOM,             // 618
    MT_SCHABBSDEMON,            // 619
    MT_CHAINGUNZOMBIEGK,            // 620
    MT_CHAINGUNZOMBIESK,            // 621
    MT_NONBOSSUBERARMOUR,           // 622
    MT_NONBOSSUBERARMOUR2,          // 623
    MT_SAELMELEEMUTANT,
    MT_SAELFIREBALLMUTANT,
    MT_HELLGUARD,               // 623
    MT_SCANDLELIT,              // 624
    MT_SCANDLEUNLIT,            // 625
    MT_SCHANDELIERLIT,          // 626
    MT_SCHANDELIERUNLIT,            // 627
    MT_SHANGINGBODY,            // 628
    MT_SHANGINGBUCKET,          // 629
    MT_SLOG,                // 630
    MT_SMUSHROOM,               // 631
    MT_STABLE1,             // 632
    MT_STABLE2,             // 633
    MT_STREESWAMP,              // 634
    MT_SBONESANDBLOOD1,         // 635
    MT_SBONESANDBLOOD2,         // 636
    MT_SBONESANDBLOOD3,         // 637
    MT_SCLEAVER,                // 638
    MT_SSTUMP,              // 639
    MT_SSTAKEOFSKULLS,          // 640
    MT_SSTAKEOFSKULLS2,         // 640
    MT_SSTAKEOFSKULLS3,         // 640
    MT_SSTAKEOFSKULLS4,         // 640
    MT_SSTAKEOFSKULLS5,         // 640
    MT_SSTAKEOFSKULLS6,         // 640
    MT_SSKULL1,             // 640
    MT_SSKULL2,             // 641
    MT_SLOG2,               // 640
    MT_SMUSHROOM2,              // 641
    MT_STREESWAMP2,             // 640
    MT_SSTUMP2,             // 641
    MT_GREENBLOB,               // 642
    MT_REDBLOB,             // 642
    MT_PURPLEBLOB,              // 642
    MT_MOTHERBLOB,              // 642
    MT_UAMMOCLIP,               // 642
    MT_TREASURE3DGLASSES,           // 642
    MT_TREASURECAP,             // 642
    MT_TREASURETSHIRT,              // 642
    MT_TREASURELUNCHBOX,                // 642
    MT_UCANS,               // 642
    MT_UFIRSTAIDKIT,                // 642
    MT_UCAN,                // 642
    MT_UMEGAHEALTH,             // 642
    MT_USILVERKEYCARD,              // 642
    MT_UGOLDKEYCARD,                // 642
    MT_UTECHLAMP,               // 642
    MT_UTECHLAMPUNLIT,              // 642
    MT_UCEILINGLIGHT,               // 642
    MT_UCEILINGLIGHTUNLIT,              // 642
    MT_UCEILINGLIGHT2,              // 642
    MT_UCEILINGLIGHTUNLIT2,             // 642
    MT_UBARRELGREEN,                // 642
    MT_UCHAIR,              // 642
    MT_UCONSOLE,                // 642
    MT_USTALACTITEICE,              // 642
    MT_USTALACTITEICE2,             // 642
    MT_USTALAGMITEICE,              // 642
    MT_USTALAGMITEICE2,             // 642
    MT_USLIMECOLUMN,                // 642
    MT_USINK,               // 642
    MT_USNOWMAN,                // 642
    MT_UTOLIET,             // 642
    MT_UTOLIET2,                // 642
    MT_UTREE,               // 642
    MT_UTREE2,              // 642
    MT_USLIMEPOOL,              // 642
    MT_USLIMEPOOL2,             // 642
    MT_WROCKET,             // 642
    MT_LROCKET,             // 642
    MT_CROCKET,             // 642
    MT_FLAMETHROWERMISSILE,         // 643
    MT_CONSOLEFLAMETHROWERMISSILE,      // 645
    MT_3D0FLAMETHROWERMISSILE,      // 645
    MT_CATAPMISSILE1,           // 644
    MT_CATAPMISSILE2,           // 645
    MT_CATAPMISSILE3,           // 645
    MT_UPISTOLMISSILE,          // 644
    MT_UPISTOLMISSILEBLUR,          // 645
    MT_UMACHINEGUNMISSILE,          // 645
    MT_UMACHINEGUNMISSILEBLUR,          // 644
    MT_UCHAINGUNMISSILE,            // 645
    MT_UCHAINGUNMISSILEBLUR,            // 645
    MT_MULTIPLAYERSYRINGE,
    MT_SCHABBSPROJECTILE,           // 646
    MT_FAKEHITLERPROJECTILE,        // 647
    MT_DKMISSILE,
    MT_ANGMISSILE1,             // 649
    MT_ANGMISSILEBLUR1,
    MT_ANGMISSILEBLUR2,
    MT_ANGMISSILEBLUR3,
    MT_ANGMISSILEBLUR4,
    MT_ANGMISSILE2,             // 650
    MT_FLAMEGUARDPROJECTILE,        // 651
    MT_ROBOTPROJECTILE,         // 651
    MT_ROBOTMISSILEPUFF,            // 651
    MT_ROBOTMISSILESMOKE,           // 651
    MT_DEVMISSILE1,             // 652
    MT_DEVMISSILE2,             // 653
    MT_CATAMISSILE1,            // 654
    MT_CATAMISSILE2,            // 655
    MT_NEMESISMISSILE,
    MT_BIOBLASTERPROJECTILE,        // 656
    MT_MADDOCPROJECTILE,            // 657
    MT_CHIMERAATTACK1,          // 658
    MT_CHIMERAATTACK2,          // 659
    MT_BALROGPROJECTILE,            // 660
    MT_DRAKEMISSILE,            // 660
    MT_STALKERPROJECTILE,           // 661
    MT_HELLGUARDMISSILE,
    MT_SCHABBSDEMONPROJECTILE,
    MT_GREENBLOBMISSILE,            // 644
    MT_REDBLOBMISSILE,          // 645
    MT_LIGHTNINGA,              // 662
    MT_LIGHTNINGAB,             // 663
    MT_LIGHTNINGAC,             // 664
    MT_LORRYA,              // 665
    MT_WATERDA,             // 666
    MT_WATERDAB,                // 667
    MT_WATERDAC,                // 668
    MT_WOLFHOWLA,               // 669
    MT_WOLFHOWLAB,              // 670
    MT_WOLFHOWLAC,              // 671
    MT_CRICKETA,                // 672
    MT_CRICKETAB,               // 673
    MT_CRICKETAC,               // 674
    MT_BIRDS1A,             // 675
    MT_BIRDS1AB,                // 676
    MT_BIRDS1AC,                // 677
    MT_BIRDS2A,             // 678
    MT_BIRDS2AB,                // 679
    MT_BIRD2AC,             // 680
    MT_WINDA,               // 681
    MT_COMPHUMA1,               // 682
    MT_COMPHUMA2,               // 683
    MT_DOORCLOSEA,              // 675
    MT_ROCKSA,              // 684
    MT_ROCKSAB,             // 685
    MT_ROCKSAC,             // 686
    MT_FROGSA,              // 687
    MT_FROGSAB,             // 688
    MT_FROGSAC,             // 689
    MT_SCREECHA,                // 690
    MT_SCREECHAB,               // 691
    MT_SCREECHAC,               // 692
    MT_WATERA,              // 693
    MT_WATERA2,             // 693
    MT_RAINA,               // 693
    MT_ELECTRICITYA,
    MT_OWLA,
    MT_PUFF,
    MT_BLOOD,
    MT_TELEPORTMAN,
    MT_TFOG,                // 039
    MT_IFOG,                // 040
    MT_MISC26,
    MT_SHOTGUN,
    MT_SUPERSHOTGUN,
    MT_CHAINGUN,
    MT_MISC27,
    MT_MIDC28,
    MT_MISC25,
    MT_MISC71,
    MT_GIBSPURPLE,
    MT_GIBSMECH,
    MT_PACMANSPAWNER,           // 698
    MT_PACMANSWASTIKA,          // 699
    MT_PACMANBJHEAD,            // 699
    MT_SPAWNERTARGET,
    MT_RADIATIONSUIT,
    MT_BACKPACK,
    MT_RAINSPAWNER,             // 699
    MT_RAINDROP,                // 699
    MT_ELITEGUARDS,             // 694
    MT_SMOKEP,
    NUMMOBJTYPES                   // 700
} mobjtype_t;

// Text.
typedef enum {
    TXT_D_DEVSTR,                  // 000
    TXT_D_CDROM,                   // 001
    TXT_PRESSKEY,                  // 002
    TXT_PRESSYN,                   // 003
    TXT_QUITMSG,                   // 004
    TXT_LOADNET,                   // 005
    TXT_QLOADNET,                  // 006
    TXT_QSAVESPOT,                 // 007
    TXT_SAVEDEAD,                  // 008
    TXT_QSPROMPT,                  // 009
    TXT_QLPROMPT,                  // 010
    TXT_NEWGAME,                   // 011
    TXT_NIGHTMARE,                 // 012
    TXT_SWSTRING,                  // 013
    TXT_MSGOFF,                    // 014
    TXT_MSGON,                     // 015
    TXT_NETEND,                    // 016
    TXT_ENDGAME,                   // 017
    TXT_DOSY,                      // 018
    TXT_DETAILHI,                  // 019
    TXT_DETAILLO,                  // 020
    TXT_GAMMALVL0,                 // 021
    TXT_GAMMALVL1,                 // 022
    TXT_GAMMALVL2,                 // 023
    TXT_GAMMALVL3,                 // 024
    TXT_GAMMALVL4,                 // 025
    TXT_EMPTYSTRING,               // 026
    TXT_GOTARMOR,                  // 027
    TXT_GOTMEGA,                   // 028
    TXT_GOTHTHBONUS,               // 029
    TXT_GOTARMBONUS,               // 030
    TXT_GOTSTIM,                   // 031
    TXT_GOTMEDINEED,               // 032
    TXT_GOTMEDIKIT,                // 033
    TXT_GOTSUPER,                  // 034
    TXT_GOTBLUECARD,               // 035
    TXT_GOTYELWCARD,               // 036
    TXT_GOTREDCARD,                // 037
    TXT_GOTBLUESKUL,               // 038
    TXT_GOTYELWSKUL,               // 039
    TXT_GOTREDSKULL,               // 040
    TXT_GOTINVUL,                  // 041
    TXT_GOTBERSERK,                // 042
    TXT_GOTINVIS,                  // 043
    TXT_GOTSUIT,                   // 044
    TXT_GOTMAP,                    // 045
    TXT_GOTVISOR,                  // 046
    TXT_GOTMSPHERE,                // 047
    TXT_GOTCLIP,                   // 048
    TXT_GOTCLIPBOX,                // 049
    TXT_GOTROCKET,                 // 050
    TXT_GOTROCKBOX,                // 051
    TXT_GOTCELL,                   // 052
    TXT_GOTCELLBOX,                // 053
    TXT_GOTSHELLS,                 // 054
    TXT_GOTSHELLBOX,               // 055
    TXT_GOTBACKPACK,               // 056
    TXT_GOTBFG9000,                // 057
    TXT_GOTCHAINGUN,               // 058
    TXT_GOTCHAINSAW,               // 059
    TXT_GOTLAUNCHER,               // 060
    TXT_GOTPLASMA,                 // 061
    TXT_GOTSHOTGUN,                // 062
    TXT_GOTSHOTGUN2,               // 063
    TXT_PD_BLUEO,                  // 064
    TXT_PD_REDO,                   // 065
    TXT_PD_YELLOWO,                // 066
    TXT_PD_BLUEK,                  // 067
    TXT_PD_REDK,                   // 068
    TXT_PD_YELLOWK,                // 069
    TXT_GGSAVED,                   // 070
    TXT_HUSTR_MSGU,                // 071
    TXT_HUSTR_E1M1,                // 072
    TXT_HUSTR_E1M2,                // 073
    TXT_HUSTR_E1M3,                // 074
    TXT_HUSTR_E1M4,                // 075
    TXT_HUSTR_E1M5,                // 076
    TXT_HUSTR_E1M6,                // 077
    TXT_HUSTR_E1M7,                // 078
    TXT_HUSTR_E1M8,                // 079
    TXT_HUSTR_E1M9,                // 080
    TXT_HUSTR_E2M1,                // 081
    TXT_HUSTR_E2M2,                // 082
    TXT_HUSTR_E2M3,                // 083
    TXT_HUSTR_E2M4,                // 084
    TXT_HUSTR_E2M5,                // 085
    TXT_HUSTR_E2M6,                // 086
    TXT_HUSTR_E2M7,                // 087
    TXT_HUSTR_E2M8,                // 088
    TXT_HUSTR_E2M9,                // 089
    TXT_HUSTR_E3M1,                // 090
    TXT_HUSTR_E3M2,                // 091
    TXT_HUSTR_E3M3,                // 092
    TXT_HUSTR_E3M4,                // 093
    TXT_HUSTR_E3M5,                // 094
    TXT_HUSTR_E3M6,                // 095
    TXT_HUSTR_E3M7,                // 096
    TXT_HUSTR_E3M8,                // 097
    TXT_HUSTR_E3M9,                // 098
    TXT_HUSTR_E4M1,                // 099
    TXT_HUSTR_E4M2,                // 100
    TXT_HUSTR_E4M3,                // 101
    TXT_HUSTR_E4M4,                // 102
    TXT_HUSTR_E4M5,                // 103
    TXT_HUSTR_E4M6,                // 104
    TXT_HUSTR_E4M7,                // 105
    TXT_HUSTR_E4M8,                // 106
    TXT_HUSTR_E4M9,                // 107
    TXT_HUSTR_1,                   // 108
    TXT_HUSTR_2,                   // 109
    TXT_HUSTR_3,                   // 110
    TXT_HUSTR_4,                   // 111
    TXT_HUSTR_5,                   // 112
    TXT_HUSTR_6,                   // 113
    TXT_HUSTR_7,                   // 114
    TXT_HUSTR_8,                   // 115
    TXT_HUSTR_9,                   // 116
    TXT_HUSTR_10,                  // 117
    TXT_HUSTR_11,                  // 118
    TXT_HUSTR_12,                  // 119
    TXT_HUSTR_13,                  // 120
    TXT_HUSTR_14,                  // 121
    TXT_HUSTR_15,                  // 122
    TXT_HUSTR_16,                  // 123
    TXT_HUSTR_17,                  // 124
    TXT_HUSTR_18,                  // 125
    TXT_HUSTR_19,                  // 126
    TXT_HUSTR_20,                  // 127
    TXT_HUSTR_21,                  // 128
    TXT_HUSTR_22,                  // 129
    TXT_HUSTR_23,                  // 130
    TXT_HUSTR_24,                  // 131
    TXT_HUSTR_25,                  // 132
    TXT_HUSTR_26,                  // 133
    TXT_HUSTR_27,                  // 134
    TXT_HUSTR_28,                  // 135
    TXT_HUSTR_29,                  // 136
    TXT_HUSTR_30,                  // 137
    TXT_HUSTR_31,                  // 138
    TXT_HUSTR_32,                  // 139
    TXT_PHUSTR_1,                  // 140
    TXT_PHUSTR_2,                  // 141
    TXT_PHUSTR_3,                  // 142
    TXT_PHUSTR_4,                  // 143
    TXT_PHUSTR_5,                  // 144
    TXT_PHUSTR_6,                  // 145
    TXT_PHUSTR_7,                  // 146
    TXT_PHUSTR_8,                  // 147
    TXT_PHUSTR_9,                  // 148
    TXT_PHUSTR_10,                 // 149
    TXT_PHUSTR_11,                 // 150
    TXT_PHUSTR_12,                 // 151
    TXT_PHUSTR_13,                 // 152
    TXT_PHUSTR_14,                 // 153
    TXT_PHUSTR_15,                 // 154
    TXT_PHUSTR_16,                 // 155
    TXT_PHUSTR_17,                 // 156
    TXT_PHUSTR_18,                 // 157
    TXT_PHUSTR_19,                 // 158
    TXT_PHUSTR_20,                 // 159
    TXT_PHUSTR_21,                 // 160
    TXT_PHUSTR_22,                 // 161
    TXT_PHUSTR_23,                 // 162
    TXT_PHUSTR_24,                 // 163
    TXT_PHUSTR_25,                 // 164
    TXT_PHUSTR_26,                 // 165
    TXT_PHUSTR_27,                 // 166
    TXT_PHUSTR_28,                 // 167
    TXT_PHUSTR_29,                 // 168
    TXT_PHUSTR_30,                 // 169
    TXT_PHUSTR_31,                 // 170
    TXT_PHUSTR_32,                 // 171
    TXT_THUSTR_1,                  // 172
    TXT_THUSTR_2,                  // 173
    TXT_THUSTR_3,                  // 174
    TXT_THUSTR_4,                  // 175
    TXT_THUSTR_5,                  // 176
    TXT_THUSTR_6,                  // 177
    TXT_THUSTR_7,                  // 178
    TXT_THUSTR_8,                  // 179
    TXT_THUSTR_9,                  // 180
    TXT_THUSTR_10,                 // 181
    TXT_THUSTR_11,                 // 182
    TXT_THUSTR_12,                 // 183
    TXT_THUSTR_13,                 // 184
    TXT_THUSTR_14,                 // 185
    TXT_THUSTR_15,                 // 186
    TXT_THUSTR_16,                 // 187
    TXT_THUSTR_17,                 // 188
    TXT_THUSTR_18,                 // 189
    TXT_THUSTR_19,                 // 190
    TXT_THUSTR_20,                 // 191
    TXT_THUSTR_21,                 // 192
    TXT_THUSTR_22,                 // 193
    TXT_THUSTR_23,                 // 194
    TXT_THUSTR_24,                 // 195
    TXT_THUSTR_25,                 // 196
    TXT_THUSTR_26,                 // 197
    TXT_THUSTR_27,                 // 198
    TXT_THUSTR_28,                 // 199
    TXT_THUSTR_29,                 // 200
    TXT_THUSTR_30,                 // 201
    TXT_THUSTR_31,                 // 202
    TXT_THUSTR_32,                 // 203
    TXT_HUSTR_CHATMACRO0,          // 204
    TXT_HUSTR_CHATMACRO1,          // 205
    TXT_HUSTR_CHATMACRO2,          // 206
    TXT_HUSTR_CHATMACRO3,          // 207
    TXT_HUSTR_CHATMACRO4,          // 208
    TXT_HUSTR_CHATMACRO5,          // 209
    TXT_HUSTR_CHATMACRO6,          // 210
    TXT_HUSTR_CHATMACRO7,          // 211
    TXT_HUSTR_CHATMACRO8,          // 212
    TXT_HUSTR_CHATMACRO9,          // 213
    TXT_HUSTR_TALKTOSELF1,         // 214
    TXT_HUSTR_TALKTOSELF2,         // 215
    TXT_HUSTR_TALKTOSELF3,         // 216
    TXT_HUSTR_TALKTOSELF4,         // 217
    TXT_HUSTR_TALKTOSELF5,         // 218
    TXT_HUSTR_MESSAGESENT,         // 219
    TXT_HUSTR_PLRGREEN,            // 220
    TXT_HUSTR_PLRINDIGO,           // 221
    TXT_HUSTR_PLRBROWN,            // 222
    TXT_HUSTR_PLRRED,              // 223
    TXT_AMSTR_FOLLOWON,            // 224
    TXT_AMSTR_FOLLOWOFF,           // 225
    TXT_AMSTR_GRIDON,              // 226
    TXT_AMSTR_GRIDOFF,             // 227
    TXT_AMSTR_MARKEDSPOT,          // 228
    TXT_AMSTR_MARKSCLEARED,        // 229
    TXT_STSTR_MUS,                 // 230
    TXT_STSTR_NOMUS,               // 231
    TXT_STSTR_DQDON,               // 232
    TXT_STSTR_DQDOFF,              // 233
    TXT_STSTR_KFAADDED,            // 234
    TXT_STSTR_FAADDED,             // 235
    TXT_STSTR_NCON,                // 236
    TXT_STSTR_NCOFF,               // 237
    TXT_STSTR_BEHOLD,              // 238
    TXT_STSTR_BEHOLDX,             // 239
    TXT_STSTR_CHOPPERS,            // 240
    TXT_STSTR_CLEV,                // 241
    TXT_E1TEXT,                    // 242
    TXT_E2TEXT,                    // 243
    TXT_E3TEXT,                    // 244
    TXT_E4TEXT,                    // 245
    TXT_C1TEXT,                    // 246
    TXT_C2TEXT,                    // 247
    TXT_C3TEXT,                    // 248
    TXT_C4TEXT,                    // 249
    TXT_C5TEXT,                    // 250
    TXT_C6TEXT,                    // 251
    TXT_P1TEXT,                    // 252
    TXT_P2TEXT,                    // 253
    TXT_P3TEXT,                    // 254
    TXT_P4TEXT,                    // 255
    TXT_P5TEXT,                    // 256
    TXT_P6TEXT,                    // 257
    TXT_T1TEXT,                    // 258
    TXT_T2TEXT,                    // 259
    TXT_T3TEXT,                    // 260
    TXT_T4TEXT,                    // 261
    TXT_T5TEXT,                    // 262
    TXT_T6TEXT,                    // 263
    TXT_CC_ZOMBIE,                 // 264
    TXT_CC_SHOTGUN,                // 265
    TXT_CC_HEAVY,                  // 266
    TXT_CC_IMP,                    // 267
    TXT_CC_DEMON,                  // 268
    TXT_CC_LOST,                   // 269
    TXT_CC_CACO,                   // 270
    TXT_CC_HELL,                   // 271
    TXT_CC_BARON,                  // 272
    TXT_CC_ARACH,                  // 273
    TXT_CC_PAIN,                   // 274
    TXT_CC_REVEN,                  // 275
    TXT_CC_MANCU,                  // 276
    TXT_CC_ARCH,                   // 277
    TXT_CC_SPIDER,                 // 278
    TXT_CC_CYBER,                  // 279
    TXT_CC_HERO,                   // 280
    TXT_QUITMESSAGE1,              // 281
    TXT_QUITMESSAGE2,              // 282
    TXT_QUITMESSAGE3,              // 283
    TXT_QUITMESSAGE4,              // 284
    TXT_QUITMESSAGE5,              // 285
    TXT_QUITMESSAGE6,              // 286
    TXT_QUITMESSAGE7,              // 287
    TXT_QUITMESSAGE8,              // 288
    TXT_QUITMESSAGE9,              // 289
    TXT_QUITMESSAGE10,             // 290
    TXT_QUITMESSAGE11,             // 291
    TXT_QUITMESSAGE12,             // 292
    TXT_QUITMESSAGE13,             // 293
    TXT_QUITMESSAGE14,             // 294
    TXT_QUITMESSAGE15,             // 295
    TXT_QUITMESSAGE16,             // 296
    TXT_QUITMESSAGE17,             // 297
    TXT_QUITMESSAGE18,             // 298
    TXT_QUITMESSAGE19,             // 299
    TXT_QUITMESSAGE20,             // 300
    TXT_QUITMESSAGE21,             // 301
    TXT_QUITMESSAGE22,             // 302
    TXT_JOINNET,                   // 303
    TXT_SAVENET,                   // 304
    TXT_CLNETLOAD,                 // 305
    TXT_LOADMISSING,               // 306
    TXT_FINALEFLAT_E1,             // 309
    TXT_FINALEFLAT_E2,             // 310
    TXT_FINALEFLAT_E3,             // 311
    TXT_FINALEFLAT_E4,             // 312
    TXT_FINALEFLAT_C2,             // 313
    TXT_FINALEFLAT_C1,             // 314
    TXT_FINALEFLAT_C3,             // 315
    TXT_FINALEFLAT_C4,             // 316
    TXT_FINALEFLAT_C5,             // 317
    TXT_FINALEFLAT_C6,             // 318
    TXT_ASK_EPISODE,               // 319
    TXT_EPISODE1,                  // 320
    TXT_EPISODE2,                  // 321
    TXT_EPISODE3,                  // 322
    TXT_EPISODE4,                  // 323
    TXT_KILLMSG_SUICIDE,           // 324
    TXT_KILLMSG_WEAPON0,           // 325
    TXT_KILLMSG_PISTOL,            // 326
    TXT_KILLMSG_SHOTGUN,           // 327
    TXT_KILLMSG_CHAINGUN,          // 328
    TXT_KILLMSG_MISSILE,           // 329
    TXT_KILLMSG_PLASMA,            // 330
    TXT_KILLMSG_BFG,               // 331
    TXT_KILLMSG_CHAINSAW,          // 332
    TXT_KILLMSG_SUPERSHOTGUN,      // 333
    TXT_KILLMSG_STOMP,
    TXT_AMSTR_ROTATEON,
    TXT_AMSTR_ROTATEOFF,
    TXT_WEAPON0,
    TXT_WEAPON1,
    TXT_WEAPON2,
    TXT_WEAPON3,                    // 340
    TXT_WEAPON4,
    TXT_WEAPON5,
    TXT_WEAPON6,
    TXT_WEAPON7,
    TXT_WEAPON8,
    TXT_SKILL1,
    TXT_SKILL2,
    TXT_SKILL3,
    TXT_SKILL4,
    TXT_SKILL5,                     // 350
    NUMTEXT
} textenum_t;

#endif
