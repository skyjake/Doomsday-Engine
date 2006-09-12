/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
    SPR_TROO,
    SPR_SHTG,
    SPR_PUNG,
    SPR_PISG,
    SPR_PISF,
    SPR_SHTF,
    SPR_SHT2,
    SPR_CHGG,
    SPR_CHGF,
    SPR_MISG,
    SPR_MISF,
    SPR_SAWG,
    SPR_PLSG,
    SPR_PLSF,
    SPR_PLSD,
    SPR_BFGG,
    SPR_BFGF,
    SPR_BLUD,
    SPR_PUFF,
    SPR_BAL1,
    SPR_BAL2,
    SPR_PLSS,
    SPR_PLSE,
    SPR_MISL,
    SPR_BFS1,
    SPR_BFE1,
    SPR_BFE2,
    SPR_TFOG,
    SPR_IFOG,
    SPR_PLAY,
    SPR_POSS,
    SPR_SPOS,
    SPR_VILE,
    SPR_FIRE,
    SPR_FATB,
    SPR_FBXP,
    SPR_SKEL,
    SPR_MANF,
    SPR_FATT,
    SPR_CPOS,
    SPR_SARG,
    SPR_HEAD,
    SPR_BAL7,
    SPR_BOSS,
    SPR_BOS2,
    SPR_SKUL,
    SPR_SPID,
    SPR_BSPI,
    SPR_APLS,
    SPR_APBX,
    SPR_CYBR,
    SPR_PAIN,
    SPR_SSWV,
    SPR_KEEN,
    SPR_BBRN,
    SPR_BOSF,
    SPR_ARM1,
    SPR_ARM2,
    SPR_BAR1,
    SPR_BEXP,
    SPR_FCAN,
    SPR_BON1,
    SPR_BON2,
    SPR_BON3,
    SPR_BKEY,
    SPR_RKEY,
    SPR_YKEY,
    SPR_BSKU,
    SPR_RSKU,
    SPR_YSKU,
    SPR_STIM,
    SPR_MEDI,
    SPR_SOUL,
    SPR_PINV,
    SPR_PSTR,
    SPR_PINS,
    SPR_MEGA,
    SPR_SUIT,
    SPR_PMAP,
    SPR_RMAP,
    SPR_DETH,
    SPR_SEEA,
    SPR_PVIS,
    SPR_CLIP,
    SPR_AMMO,
    SPR_ROCK,
    SPR_BROK,
    SPR_CELL,
    SPR_CELP,
    SPR_SHEL,
    SPR_SBOX,
    SPR_BPAK,
    SPR_BFUG,
    SPR_MGUN,
    SPR_CSAW,
    SPR_LAUN,
    SPR_PLAS,
    SPR_SHOT,
    SPR_SGN2,
    SPR_LGUN,
    SPR_POW1,
    SPR_POW2,
    SPR_POW3,
    SPR_POW4,
    SPR_POW5,
    SPR_STDT,
    SPR_STHT,
    SPR_COLU,
    SPR_SMT2,
    SPR_GOR1,
    SPR_POL2,
    SPR_POL5,
    SPR_POL4,
    SPR_POL3,
    SPR_POL1,
    SPR_POL6,
    SPR_GOR2,
    SPR_GOR3,
    SPR_GOR4,
    SPR_GOR5,
    SPR_SMIT,
    SPR_COL1,
    SPR_COL2,
    SPR_COL3,
    SPR_COL4,
    SPR_CAND,
    SPR_CBRA,
    SPR_COL6,
    SPR_TRE1,
    SPR_TRE2,
    SPR_ELEC,
    SPR_CEYE,
    SPR_FSKU,
    SPR_COL5,
    SPR_TBLU,
    SPR_TGRN,
    SPR_TRED,
    SPR_SMBT,
    SPR_SMGT,
    SPR_SMRT,
    SPR_HDB1,
    SPR_HDB2,
    SPR_HDB3,
    SPR_HDB4,
    SPR_HDB5,
    SPR_HDB6,
    SPR_POB1,
    SPR_POB2,
    SPR_BRS1,
    SPR_TLMP,
    SPR_TLP2,
    SPR_NTRO,
    SPR_DART,
    SPR_SAR2,
    SPR_HED2,
    SPR_BRNR,
    SPR_NMBL,
    SPR_SMOK,
    SPR_UNKF,
    SPR_HTUF,
    SPR_LDUF,
    SPR_LAZR,
    SPR_LPUF,
    SPR_MOTH,
    SPR_MBAL,
    SPR_MPUF,
    SPR_BLNK,
    SPR_POSC,
    SPR_BSGI,
    SPR_GREN,
    SPR_ACID,
    SPR_ACIP,
    SPR_STLK,
    SPR_HFOG,
    NUMSPRITES
} spritetype_e;

// States.
typedef enum {
    S_NULL,
    S_LIGHTDONE,
    S_PUNCH,
    S_PUNCHDOWN,
    S_PUNCHUP,
    S_PUNCH1,
    S_PUNCH2,
    S_PUNCH3,
    S_PUNCH4,
    S_PUNCH5,
    S_PISTOL,
    S_PISTOLDOWN,
    S_PISTOLUP,
    S_PISTOL1,
    S_PISTOL2,
    S_PISTOL3,
    S_PISTOL4,
    S_PISTOLFLASH,
    S_SGUN,
    S_SGUNDOWN,
    S_SGUNUP,
    S_SGUN1,
    S_SGUN2,
    S_SGUN3,
    S_SGUN4,
    S_SGUN5,
    S_SGUN6,
    S_SGUN7,
    S_SGUN8,
    S_SGUN9,
    S_SGUNFLASH1,
    S_SGUNFLASH2,
    S_DSGUN,
    S_DSGUNDOWN,
    S_DSGUNUP,
    S_DSGUN1,
    S_DSGUN2,
    S_DSGUN3,
    S_DSGUN4,
    S_DSGUN5,
    S_DSGUN6,
    S_DSGUN7,
    S_DSGUN8,
    S_DSGUN9,
    S_DSGUN10,
    S_DSNR1,
    S_DSNR2,
    S_DSGUNFLASH1,
    S_DSGUNFLASH2,
    S_CHAIN,
    S_CHAINDOWN,
    S_CHAINUP,
    S_CHAIN1,
    S_CHAIN2,
    S_CHAIN3,
    S_CHAINFLASH1,
    S_CHAINFLASH2,
    S_MISSILE,
    S_MISSILEDOWN,
    S_MISSILEUP,
    S_MISSILE1,
    S_MISSILE2,
    S_MISSILE3,
    S_MISSILEFLASH1,
    S_MISSILEFLASH2,
    S_MISSILEFLASH3,
    S_MISSILEFLASH4,
    S_SAW,
    S_SAWB,
    S_SAWDOWN,
    S_SAWUP,
    S_SAW1,
    S_SAW2,
    S_SAW3,
    S_PLASMA,
    S_PLASMADOWN,
    S_PLASMAUP,
    S_PLASMA1,
    S_PLASMA2,
    S_PLASMASHOCK1,
    S_PLASMASHOCK2,
    S_PLASMASHOCK3,
    S_PLASMASHOCK4,
    S_PLASMAFLASH1,
    S_PLASMAFLASH2,
    S_BFG,
    S_BFGDOWN,
    S_BFGUP,
    S_BFG1,
    S_BFG2,
    S_BFG3,
    S_BFG4,
    S_BFGFLASH1,
    S_BFGFLASH2,
    S_BLOOD1,
    S_BLOOD2,
    S_BLOOD3,
    S_PUFF1,
    S_PUFF2,
    S_PUFF3,
    S_PUFF4,
    S_TBALL1,
    S_TBALL2,
    S_TBALLX1,
    S_TBALLX2,
    S_TBALLX3,
    S_RBALL1,
    S_RBALL2,
    S_RBALLX1,
    S_RBALLX2,
    S_RBALLX3,
    S_PLASBALL,
    S_PLASBALL2,
    S_PLASEXP,
    S_PLASEXP2,
    S_PLASEXP3,
    S_PLASEXP4,
    S_PLASEXP5,
    S_ROCKET,
    S_BFGSHOT,
    S_BFGSHOT2,
    S_BFGLAND,
    S_BFGLAND2,
    S_BFGLAND3,
    S_BFGLAND4,
    S_BFGLAND5,
    S_BFGLAND6,
    S_BFGEXP,
    S_BFGEXP2,
    S_BFGEXP3,
    S_BFGEXP4,
    S_EXPLODE1,
    S_EXPLODE2,
    S_EXPLODE3,
    S_TFOG,
    S_TFOG01,
    S_TFOG02,
    S_TFOG2,
    S_TFOG3,
    S_TFOG4,
    S_TFOG5,
    S_TFOG6,
    S_TFOG7,
    S_TFOG8,
    S_TFOG9,
    S_TFOG10,
    S_IFOG,
    S_IFOG01,
    S_IFOG02,
    S_IFOG2,
    S_IFOG3,
    S_IFOG4,
    S_IFOG5,
    S_HTIMEBLINK1,
    S_HTIMEBLINK2,
    S_HTIMEBLINK3,
    S_HTIMEBLINK4,
    S_LDBLINK1,
    S_LDBLINK2,
    S_LDBLINK3,
    S_LDBLINK4,
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
    S_PLAY_XDIE1,
    S_PLAY_XDIE2,
    S_PLAY_XDIE3,
    S_PLAY_XDIE4,
    S_PLAY_XDIE5,
    S_PLAY_XDIE6,
    S_PLAY_XDIE7,
    S_PLAY_XDIE8,
    S_PLAY_XDIE9,
    S_POSS_STND,
    S_POSS_STND2,
    S_POSS_RUN1,
    S_POSS_RUN2,
    S_POSS_RUN3,
    S_POSS_RUN4,
    S_POSS_RUN5,
    S_POSS_RUN6,
    S_POSS_RUN7,
    S_POSS_RUN8,
    S_POSS_ATK1,
    S_POSS_ATK2,
    S_POSS_ATK3,
    S_POSS_PAIN,
    S_POSS_PAIN2,
    S_POSS_DIE1,
    S_POSS_DIE2,
    S_POSS_DIE3,
    S_POSS_DIE4,
    S_POSS_DIE5,
    S_POSS_XDIE1,
    S_POSS_XDIE2,
    S_POSS_XDIE3,
    S_POSS_XDIE4,
    S_POSS_XDIE5,
    S_POSS_XDIE6,
    S_POSS_XDIE7,
    S_POSS_XDIE8,
    S_POSS_XDIE9,
    S_POSS_RAISE1,
    S_POSS_RAISE2,
    S_POSS_RAISE3,
    S_POSS_RAISE4,
    S_SPOS_STND,
    S_SPOS_STND2,
    S_SPOS_RUN1,
    S_SPOS_RUN2,
    S_SPOS_RUN3,
    S_SPOS_RUN4,
    S_SPOS_RUN5,
    S_SPOS_RUN6,
    S_SPOS_RUN7,
    S_SPOS_RUN8,
    S_SPOS_ATK1,
    S_SPOS_ATK2,
    S_SPOS_ATK3,
    S_SPOS_PAIN,
    S_SPOS_PAIN2,
    S_SPOS_DIE1,
    S_SPOS_DIE2,
    S_SPOS_DIE3,
    S_SPOS_DIE4,
    S_SPOS_DIE5,
    S_SPOS_XDIE1,
    S_SPOS_XDIE2,
    S_SPOS_XDIE3,
    S_SPOS_XDIE4,
    S_SPOS_XDIE5,
    S_SPOS_XDIE6,
    S_SPOS_XDIE7,
    S_SPOS_XDIE8,
    S_SPOS_XDIE9,
    S_SPOS_RAISE1,
    S_SPOS_RAISE2,
    S_SPOS_RAISE3,
    S_SPOS_RAISE4,
    S_SPOS_RAISE5,
    S_POSC_STND,
    S_POSC_STND2,
    S_POSC_RUN1,
    S_POSC_RUN2,
    S_POSC_RUN3,
    S_POSC_RUN4,
    S_POSC_RUN5,
    S_POSC_RUN6,
    S_POSC_RUN7,
    S_POSC_RUN8,
    S_POSC_ATK1,
    S_POSC_ATK2,
    S_POSC_ATK3,
    S_POSC_PAIN,
    S_POSC_PAIN2,
    S_POSC_DIE1,
    S_POSC_DIE2,
    S_POSC_DIE3,
    S_POSC_DIE4,
    S_POSC_DIE5,
    S_POSC_XDIE1,
    S_POSC_XDIE2,
    S_POSC_XDIE3,
    S_POSC_XDIE4,
    S_POSC_XDIE5,
    S_POSC_XDIE6,
    S_POSC_XDIE7,
    S_POSC_XDIE8,
    S_POSC_XDIE9,
    S_VILE_STND,
    S_VILE_STND2,
    S_VILE_RUN1,
    S_VILE_RUN2,
    S_VILE_RUN3,
    S_VILE_RUN4,
    S_VILE_RUN5,
    S_VILE_RUN6,
    S_VILE_RUN7,
    S_VILE_RUN8,
    S_VILE_RUN9,
    S_VILE_RUN10,
    S_VILE_RUN11,
    S_VILE_RUN12,
    S_VILE_ATK1,
    S_VILE_ATK2,
    S_VILE_ATK3,
    S_VILE_ATK4,
    S_VILE_ATK5,
    S_VILE_ATK6,
    S_VILE_ATK7,
    S_VILE_ATK8,
    S_VILE_ATK9,
    S_VILE_ATK10,
    S_VILE_ATK11,
    S_VILE_HEAL1,
    S_VILE_HEAL2,
    S_VILE_HEAL3,
    S_VILE_PAIN,
    S_VILE_PAIN2,
    S_VILE_DIE1,
    S_VILE_DIE2,
    S_VILE_DIE3,
    S_VILE_DIE4,
    S_VILE_DIE5,
    S_VILE_DIE6,
    S_VILE_DIE7,
    S_VILE_DIE8,
    S_VILE_DIE9,
    S_VILE_DIE10,
    S_STALK_STND,
    S_STALK_RUN1,
    S_STALK_ATK1,
    S_STALK_ATK2,
    S_STALK_ATK3,
    S_STALK_PAIN,
    S_STALK_DIE1,
    S_STALK_DIE2,
    S_STALK_DIE3,
    S_STALK_DIE4,
    S_STALK_DIE5,
    S_STALK_DIE6,
    S_STALK_DIE7,
    S_STALK_DIE8,
    S_STALK_DIE9,
    S_STALK_DIE10,
    S_STALK_HIDE,
    S_STALK_SHOW,
    S_FIRE1,
    S_FIRE2,
    S_FIRE3,
    S_FIRE4,
    S_FIRE5,
    S_FIRE6,
    S_FIRE7,
    S_FIRE8,
    S_FIRE9,
    S_FIRE10,
    S_FIRE11,
    S_FIRE12,
    S_FIRE13,
    S_FIRE14,
    S_FIRE15,
    S_FIRE16,
    S_FIRE17,
    S_FIRE18,
    S_FIRE19,
    S_FIRE20,
    S_FIRE21,
    S_FIRE22,
    S_FIRE23,
    S_FIRE24,
    S_FIRE25,
    S_FIRE26,
    S_FIRE27,
    S_FIRE28,
    S_FIRE29,
    S_FIRE30,
    S_SMOKE1,
    S_SMOKE2,
    S_SMOKE3,
    S_SMOKE4,
    S_SMOKE5,
    S_SKEL_STND,
    S_SKEL_STND2,
    S_SKEL_RUN1,
    S_SKEL_RUN2,
    S_SKEL_RUN3,
    S_SKEL_RUN4,
    S_SKEL_RUN5,
    S_SKEL_RUN6,
    S_SKEL_RUN7,
    S_SKEL_RUN8,
    S_SKEL_RUN9,
    S_SKEL_RUN10,
    S_SKEL_RUN11,
    S_SKEL_RUN12,
    S_SKEL_FIST1,
    S_SKEL_FIST2,
    S_SKEL_FIST3,
    S_SKEL_FIST4,
    S_SKEL_MISS1,
    S_SKEL_MISS2,
    S_SKEL_MISS3,
    S_SKEL_MISS4,
    S_SKEL_PAIN,
    S_SKEL_PAIN2,
    S_SKEL_DIE1,
    S_SKEL_DIE2,
    S_SKEL_DIE3,
    S_SKEL_DIE4,
    S_SKEL_DIE5,
    S_SKEL_DIE6,
    S_SKEL_RAISE1,
    S_SKEL_RAISE2,
    S_SKEL_RAISE3,
    S_SKEL_RAISE4,
    S_SKEL_RAISE5,
    S_SKEL_RAISE6,
    S_FATSHOT1,
    S_FATSHOT2,
    S_FATSHOTX1,
    S_FATSHOTX2,
    S_FATSHOTX3,
    S_FATT_STND,
    S_FATT_STND2,
    S_FATT_RUN1,
    S_FATT_RUN2,
    S_FATT_RUN3,
    S_FATT_RUN4,
    S_FATT_RUN5,
    S_FATT_RUN6,
    S_FATT_RUN7,
    S_FATT_RUN8,
    S_FATT_RUN9,
    S_FATT_RUN10,
    S_FATT_RUN11,
    S_FATT_RUN12,
    S_FATT_ATK1,
    S_FATT_ATK2,
    S_FATT_ATK3,
    S_FATT_ATK4,
    S_FATT_ATK5,
    S_FATT_ATK6,
    S_FATT_ATK7,
    S_FATT_ATK8,
    S_FATT_ATK9,
    S_FATT_ATK10,
    S_FATT_PAIN,
    S_FATT_PAIN2,
    S_FATT_DIE1,
    S_FATT_DIE2,
    S_FATT_DIE3,
    S_FATT_DIE4,
    S_FATT_DIE5,
    S_FATT_DIE6,
    S_FATT_DIE7,
    S_FATT_DIE8,
    S_FATT_DIE9,
    S_FATT_DIE10,
    S_FATT_RAISE1,
    S_FATT_RAISE2,
    S_FATT_RAISE3,
    S_FATT_RAISE4,
    S_FATT_RAISE5,
    S_FATT_RAISE6,
    S_FATT_RAISE7,
    S_FATT_RAISE8,
    S_CPOS_STND,
    S_CPOS_STND2,
    S_CPOS_RUN1,
    S_CPOS_RUN2,
    S_CPOS_RUN3,
    S_CPOS_RUN4,
    S_CPOS_RUN5,
    S_CPOS_RUN6,
    S_CPOS_RUN7,
    S_CPOS_RUN8,
    S_CPOS_ATK1,
    S_CPOS_ATK2,
    S_CPOS_ATK3,
    S_CPOS_ATK4,
    S_CPOS_PAIN,
    S_CPOS_PAIN2,
    S_CPOS_DIE1,
    S_CPOS_DIE2,
    S_CPOS_DIE3,
    S_CPOS_DIE4,
    S_CPOS_DIE5,
    S_CPOS_DIE6,
    S_CPOS_DIE7,
    S_CPOS_XDIE1,
    S_CPOS_XDIE2,
    S_CPOS_XDIE3,
    S_CPOS_XDIE4,
    S_CPOS_XDIE5,
    S_CPOS_XDIE6,
    S_CPOS_RAISE1,
    S_CPOS_RAISE2,
    S_CPOS_RAISE3,
    S_CPOS_RAISE4,
    S_CPOS_RAISE5,
    S_CPOS_RAISE6,
    S_CPOS_RAISE7,
    S_TROO_STND,
    S_TROO_STND2,
    S_TROO_RUN1,
    S_TROO_RUN2,
    S_TROO_RUN3,
    S_TROO_RUN4,
    S_TROO_RUN5,
    S_TROO_RUN6,
    S_TROO_RUN7,
    S_TROO_RUN8,
    S_TROO_ATK1,
    S_TROO_ATK2,
    S_TROO_ATK3,
    S_TROO_MEL1,
    S_TROO_MEL2,
    S_TROO_MEL3,
    S_TROO_PAIN,
    S_TROO_PAIN2,
    S_TROO_DIE1,
    S_TROO_DIE2,
    S_TROO_DIE3,
    S_TROO_DIE4,
    S_TROO_DIE5,
    S_TROO_XDIE1,
    S_TROO_XDIE2,
    S_TROO_XDIE3,
    S_TROO_XDIE4,
    S_TROO_XDIE5,
    S_TROO_XDIE6,
    S_TROO_XDIE7,
    S_TROO_XDIE8,
    S_TROO_RAISE1,
    S_TROO_RAISE2,
    S_TROO_RAISE3,
    S_TROO_RAISE4,
    S_TROO_RAISE5,
    S_NTRO_STND,
    S_NTRO_STND2,
    S_NTRO_RUN1,
    S_NTRO_RUN2,
    S_NTRO_RUN3,
    S_NTRO_RUN4,
    S_NTRO_RUN5,
    S_NTRO_RUN6,
    S_NTRO_RUN7,
    S_NTRO_RUN8,
    S_NTRO_ATK1,
    S_NTRO_ATK2,
    S_NTRO_ATK3,
    S_NTRO_ATK4,
    S_NTRO_ATK5,
    S_NTRO_ATK6,
    S_NTRO_MEL1,
    S_NTRO_MEL2,
    S_NTRO_MEL3,
    S_NTRO_PAIN,
    S_NTRO_PAIN2,
    S_NTRO_DIE1,
    S_NTRO_DIE2,
    S_NTRO_DIE3,
    S_NTRO_DIE4,
    S_NTRO_DIE5,
    S_NTRO_XDIE1,
    S_NTRO_XDIE2,
    S_NTRO_XDIE3,
    S_NTRO_XDIE4,
    S_NTRO_XDIE5,
    S_NTRO_XDIE6,
    S_NTRO_XDIE7,
    S_NTRO_XDIE8,
    S_SARG_STND,
    S_SARG_STND2,
    S_SARG_RUN1,
    S_SARG_RUN2,
    S_SARG_RUN3,
    S_SARG_RUN4,
    S_SARG_RUN5,
    S_SARG_RUN6,
    S_SARG_RUN7,
    S_SARG_RUN8,
    S_SARG_ATK1,
    S_SARG_ATK2,
    S_SARG_ATK3,
    S_SARG_PAIN,
    S_SARG_PAIN2,
    S_SARG_DIE1,
    S_SARG_DIE2,
    S_SARG_DIE3,
    S_SARG_DIE4,
    S_SARG_DIE5,
    S_SARG_DIE6,
    S_SARG_RAISE1,
    S_SARG_RAISE2,
    S_SARG_RAISE3,
    S_SARG_RAISE4,
    S_SARG_RAISE5,
    S_SARG_RAISE6,
    S_HEAD_STND,
    S_HEAD_RUN1,
    S_HEAD_RUN2,
    S_HEAD_RUN3,
    S_HEAD_RUN4,
    S_HEAD_RUN5,
    S_HEAD_RUN6,
    S_HEAD_RUN7,
    S_HEAD_RUN8,
    S_HEAD_ATK1,
    S_HEAD_ATK2,
    S_HEAD_ATK3,
    S_HEAD_PAIN,
    S_HEAD_PAIN2,
    S_HEAD_PAIN3,
    S_HEAD_DIE1,
    S_HEAD_DIE2,
    S_HEAD_DIE3,
    S_HEAD_DIE4,
    S_HEAD_DIE5,
    S_HEAD_DIE6,
    S_HEAD_RAISE1,
    S_HEAD_RAISE2,
    S_HEAD_RAISE3,
    S_HEAD_RAISE4,
    S_HEAD_RAISE5,
    S_HEAD_RAISE6,
    S_HED2_STND,
    S_HED2_RUN1,
    S_HED2_RUN2,
    S_HED2_RUN3,
    S_HED2_RUN4,
    S_HED2_RUN5,
    S_HED2_RUN6,
    S_HED2_RUN7,
    S_HED2_RUN8,
    S_HED2_ATK1,
    S_HED2_ATK2,
    S_HED2_ATK3,
    S_HED2_ATK4,
    S_HED2_ATK5,
    S_HED2_ATK6,
    S_HED2_ATK7,
    S_HED2_ATK8,
    S_HED2_ATK9,
    S_HED2_PAIN,
    S_HED2_PAIN2,
    S_HED2_PAIN3,
    S_HED2_DIE1,
    S_HED2_DIE2,
    S_HED2_DIE3,
    S_HED2_DIE4,
    S_HED2_DIE5,
    S_HED2_DIE6,
    S_BRBALL1,
    S_BRBALL2,
    S_BRBALLX1,
    S_BRBALLX2,
    S_BRBALLX3,
    S_BOSS_STND,
    S_BOSS_STND2,
    S_BOSS_RUN1,
    S_BOSS_RUN2,
    S_BOSS_RUN3,
    S_BOSS_RUN4,
    S_BOSS_RUN5,
    S_BOSS_RUN6,
    S_BOSS_RUN7,
    S_BOSS_RUN8,
    S_BOSS_ATK1,
    S_BOSS_ATK2,
    S_BOSS_ATK3,
    S_BOSS_PAIN,
    S_BOSS_PAIN2,
    S_BOSS_DIE1,
    S_BOSS_DIE2,
    S_BOSS_DIE3,
    S_BOSS_DIE4,
    S_BOSS_DIE5,
    S_BOSS_DIE6,
    S_BOSS_DIE7,
    S_BOSS_RAISE1,
    S_BOSS_RAISE2,
    S_BOSS_RAISE3,
    S_BOSS_RAISE4,
    S_BOSS_RAISE5,
    S_BOSS_RAISE6,
    S_BOSS_RAISE7,
    S_BOS2_STND,
    S_BOS2_STND2,
    S_BOS2_RUN1,
    S_BOS2_RUN2,
    S_BOS2_RUN3,
    S_BOS2_RUN4,
    S_BOS2_RUN5,
    S_BOS2_RUN6,
    S_BOS2_RUN7,
    S_BOS2_RUN8,
    S_BOS2_ATK1,
    S_BOS2_ATK2,
    S_BOS2_ATK3,
    S_BOS2_PAIN,
    S_BOS2_PAIN2,
    S_BOS2_DIE1,
    S_BOS2_DIE2,
    S_BOS2_DIE3,
    S_BOS2_DIE4,
    S_BOS2_DIE5,
    S_BOS2_DIE6,
    S_BOS2_DIE7,
    S_BOS2_RAISE1,
    S_BOS2_RAISE2,
    S_BOS2_RAISE3,
    S_BOS2_RAISE4,
    S_BOS2_RAISE5,
    S_BOS2_RAISE6,
    S_BOS2_RAISE7,
    S_SKULL_STND,
    S_SKULL_STND2,
    S_SKULL_RUN1,
    S_SKULL_RUN2,
    S_SKULL_ATK1,
    S_SKULL_ATK2,
    S_SKULL_ATK3,
    S_SKULL_ATK4,
    S_SKULL_PAIN,
    S_SKULL_PAIN2,
    S_SKULL_DIE1,
    S_SKULL_DIE2,
    S_SKULL_DIE3,
    S_SKULL_DIE4,
    S_SKULL_DIE5,
    S_SKULL_DIE6,
    S_SPID_STND,
    S_SPID_STND2,
    S_SPID_RUN1,
    S_SPID_RUN2,
    S_SPID_RUN3,
    S_SPID_RUN4,
    S_SPID_RUN5,
    S_SPID_RUN6,
    S_SPID_RUN7,
    S_SPID_RUN8,
    S_SPID_RUN9,
    S_SPID_RUN10,
    S_SPID_RUN11,
    S_SPID_RUN12,
    S_SPID_ATK1,
    S_SPID_ATK2,
    S_SPID_ATK3,
    S_SPID_ATK4,
    S_SPID_PAIN,
    S_SPID_PAIN2,
    S_SPID_DIE1,
    S_SPID_DIE2,
    S_SPID_DIE3,
    S_SPID_DIE4,
    S_SPID_DIE5,
    S_SPID_DIE6,
    S_SPID_DIE7,
    S_SPID_DIE8,
    S_SPID_DIE9,
    S_SPID_DIE10,
    S_SPID_DIE11,
    S_BSPI_STND,
    S_BSPI_STND2,
    S_BSPI_SIGHT,
    S_BSPI_RUN1,
    S_BSPI_RUN2,
    S_BSPI_RUN3,
    S_BSPI_RUN4,
    S_BSPI_RUN5,
    S_BSPI_RUN6,
    S_BSPI_RUN7,
    S_BSPI_RUN8,
    S_BSPI_RUN9,
    S_BSPI_RUN10,
    S_BSPI_RUN11,
    S_BSPI_RUN12,
    S_BSPI_ATK1,
    S_BSPI_ATK2,
    S_BSPI_ATK3,
    S_BSPI_ATK4,
    S_BSPI_ATK5,
    S_BSPI_ATK6,
    S_BSPI_ATK7,
    S_BSPI_ATK8,
    S_BSPI_ATK9,
    S_BSPI_ATK10,
    S_BSPI_ATK11,
    S_BSPI_PAIN,
    S_BSPI_PAIN2,
    S_BSPI_DIE1,
    S_BSPI_DIE2,
    S_BSPI_DIE3,
    S_BSPI_DIE4,
    S_BSPI_DIE5,
    S_BSPI_DIE6,
    S_BSPI_DIE7,
    S_BSPI_RAISE1,
    S_BSPI_RAISE2,
    S_BSPI_RAISE3,
    S_BSPI_RAISE4,
    S_BSPI_RAISE5,
    S_BSPI_RAISE6,
    S_BSPI_RAISE7,
    S_BSGI_STND,
    S_BSGI_STND2,
    S_BSGI_RUN1,
    S_BSGI_RUN2,
    S_BSGI_RUN3,
    S_BSGI_RUN4,
    S_BSGI_RUN5,
    S_BSGI_RUN6,
    S_BSGI_RUN7,
    S_BSGI_RUN8,
    S_BSGI_RUN9,
    S_BSGI_RUN10,
    S_BSGI_RUN11,
    S_BSGI_RUN12,
    S_BSGI_ATK1,
    S_BSGI_ATK2,
    S_BSGI_ATK3,
    S_BSGI_PAIN,
    S_BSGI_PAIN2,
    S_BSGI_DIE1,
    S_BSGI_DIE2,
    S_BSGI_DIE3,
    S_BSGI_DIE4,
    S_BSGI_DIE5,
    S_BSGI_DIE6,
    S_BSGI_DIE7,
    S_ARACH_PLAZ,
    S_ARACH_PLAZ2,
    S_ARACH_PLEX,
    S_ARACH_PLEX2,
    S_ARACH_PLEX3,
    S_ARACH_PLEX4,
    S_ARACH_PLEX5,
    S_CYBER_STND,
    S_CYBER_STND2,
    S_CYBER_RUN1,
    S_CYBER_RUN2,
    S_CYBER_RUN3,
    S_CYBER_RUN4,
    S_CYBER_RUN5,
    S_CYBER_RUN6,
    S_CYBER_RUN7,
    S_CYBER_RUN8,
    S_CYBER_ATK1,
    S_CYBER_ATK2,
    S_CYBER_ATK3,
    S_CYBER_ATK4,
    S_CYBER_ATK5,
    S_CYBER_ATK6,
    S_CYBER_PAIN,
    S_CYBER_DIE1,
    S_CYBER_DIE2,
    S_CYBER_DIE3,
    S_CYBER_DIE4,
    S_CYBER_DIE5,
    S_CYBER_DIE6,
    S_CYBER_DIE7,
    S_CYBER_DIE8,
    S_CYBER_DIE9,
    S_CYBER_DIE10,
    S_PAIN_STND,
    S_PAIN_RUN1,
    S_PAIN_RUN2,
    S_PAIN_RUN3,
    S_PAIN_RUN4,
    S_PAIN_RUN5,
    S_PAIN_RUN6,
    S_PAIN_ATK1,
    S_PAIN_ATK2,
    S_PAIN_ATK3,
    S_PAIN_ATK4,
    S_PAIN_PAIN,
    S_PAIN_PAIN2,
    S_PAIN_DIE1,
    S_PAIN_DIE2,
    S_PAIN_DIE3,
    S_PAIN_DIE4,
    S_PAIN_DIE5,
    S_PAIN_DIE6,
    S_PAIN_RAISE1,
    S_PAIN_RAISE2,
    S_PAIN_RAISE3,
    S_PAIN_RAISE4,
    S_PAIN_RAISE5,
    S_PAIN_RAISE6,
    S_SSWV_STND,
    S_SSWV_STND2,
    S_SSWV_RUN1,
    S_SSWV_RUN2,
    S_SSWV_RUN3,
    S_SSWV_RUN4,
    S_SSWV_RUN5,
    S_SSWV_RUN6,
    S_SSWV_RUN7,
    S_SSWV_RUN8,
    S_SSWV_ATK1,
    S_SSWV_ATK2,
    S_SSWV_ATK3,
    S_SSWV_ATK4,
    S_SSWV_ATK5,
    S_SSWV_ATK6,
    S_SSWV_PAIN,
    S_SSWV_PAIN2,
    S_SSWV_DIE1,
    S_SSWV_DIE2,
    S_SSWV_DIE3,
    S_SSWV_DIE4,
    S_SSWV_DIE5,
    S_SSWV_XDIE1,
    S_SSWV_XDIE2,
    S_SSWV_XDIE3,
    S_SSWV_XDIE4,
    S_SSWV_XDIE5,
    S_SSWV_XDIE6,
    S_SSWV_XDIE7,
    S_SSWV_XDIE8,
    S_SSWV_XDIE9,
    S_SSWV_RAISE1,
    S_SSWV_RAISE2,
    S_SSWV_RAISE3,
    S_SSWV_RAISE4,
    S_SSWV_RAISE5,
    S_KEENSTND,
    S_COMMKEEN,
    S_COMMKEEN2,
    S_COMMKEEN3,
    S_COMMKEEN4,
    S_COMMKEEN5,
    S_COMMKEEN6,
    S_COMMKEEN7,
    S_COMMKEEN8,
    S_COMMKEEN9,
    S_COMMKEEN10,
    S_COMMKEEN11,
    S_COMMKEEN12,
    S_KEENPAIN,
    S_KEENPAIN2,
    S_BRAIN,
    S_BRAIN_PAIN,
    S_BRAIN_DIE1,
    S_BRAIN_DIE2,
    S_BRAIN_DIE3,
    S_BRAIN_DIE4,
    S_BRAINEYE,
    S_BRAINEYESEE,
    S_BRAINEYE1,
    S_SPAWN1,
    S_SPAWN2,
    S_SPAWN3,
    S_SPAWN4,
    S_SPAWNFIRE1,
    S_SPAWNFIRE2,
    S_SPAWNFIRE3,
    S_SPAWNFIRE4,
    S_SPAWNFIRE5,
    S_SPAWNFIRE6,
    S_SPAWNFIRE7,
    S_SPAWNFIRE8,
    S_BRAINEXPLODE1,
    S_BRAINEXPLODE2,
    S_BRAINEXPLODE3,
    S_ARM1,
    S_ARM1A,
    S_ARM2,
    S_ARM2A,
    S_BAR1,
    S_BAR2,
    S_BEXP,
    S_BEXP2,
    S_BEXP3,
    S_BEXP4,
    S_BEXP5,
    S_BBAR1,
    S_BBAR2,
    S_BBAR3,
    S_BON1,
    S_BON1A,
    S_BON1B,
    S_BON1C,
    S_BON1D,
    S_BON1E,
    S_BON2,
    S_BON2A,
    S_BON2B,
    S_BON2C,
    S_BON2D,
    S_BON2E,
    S_BKEY,
    S_BKEY2,
    S_RKEY,
    S_RKEY2,
    S_YKEY,
    S_YKEY2,
    S_BSKULL,
    S_BSKULL2,
    S_RSKULL,
    S_RSKULL2,
    S_YSKULL,
    S_YSKULL2,
    S_STIM,
    S_MEDI,
    S_SOUL,
    S_SOUL2,
    S_SOUL3,
    S_SOUL4,
    S_SOUL5,
    S_SOUL6,
    S_PINV,
    S_PINV2,
    S_PINV3,
    S_PINV4,
    S_PSTR,
    S_PINS,
    S_PINS2,
    S_PINS3,
    S_PINS4,
    S_MEGA,
    S_MEGA2,
    S_MEGA3,
    S_MEGA4,
    S_SUIT,
    S_PMAP,
    S_PMAP2,
    S_PMAP3,
    S_PMAP4,
    S_PMAP5,
    S_PMAP6,
    S_PVIS,
    S_PVIS2,
    S_CLIP,
    S_AMMO,
    S_ROCK,
    S_BROK,
    S_CELL,
    S_CELP,
    S_SHEL,
    S_SBOX,
    S_BPAK,
    S_BFUG,
    S_MGUN,
    S_CSAW,
    S_LAUN,
    S_PLAS,
    S_SHOT,
    S_SHOT2,
    S_COLU,
    S_STALAG,
    S_BLOODYTWITCH,
    S_BLOODYTWITCH2,
    S_BLOODYTWITCH3,
    S_BLOODYTWITCH4,
    S_DEADTORSO,
    S_DEADBOTTOM,
    S_HEADSONSTICK,
    S_GIBS,
    S_HEADONASTICK,
    S_HEADCANDLES,
    S_HEADCANDLES2,
    S_DEADSTICK,
    S_LIVESTICK,
    S_LIVESTICK2,
    S_MEAT2,
    S_MEAT3,
    S_MEAT4,
    S_MEAT5,
    S_STALAGTITE,
    S_TALLGRNCOL,
    S_SHRTGRNCOL,
    S_TALLREDCOL,
    S_SHRTREDCOL,
    S_CANDLESTIK,
    S_CANDELABRA,
    S_SKULLCOL,
    S_TORCHTREE,
    S_BIGTREE,
    S_TECHPILLAR,
    S_EVILEYE,
    S_EVILEYE2,
    S_EVILEYE3,
    S_EVILEYE4,
    S_FLOATSKULL,
    S_FLOATSKULL2,
    S_FLOATSKULL3,
    S_HEARTCOL,
    S_HEARTCOL2,
    S_BLUETORCH,
    S_BLUETORCH2,
    S_BLUETORCH3,
    S_BLUETORCH4,
    S_GREENTORCH,
    S_GREENTORCH2,
    S_GREENTORCH3,
    S_GREENTORCH4,
    S_REDTORCH,
    S_REDTORCH2,
    S_REDTORCH3,
    S_REDTORCH4,
    S_BTORCHSHRT,
    S_BTORCHSHRT2,
    S_BTORCHSHRT3,
    S_BTORCHSHRT4,
    S_GTORCHSHRT,
    S_GTORCHSHRT2,
    S_GTORCHSHRT3,
    S_GTORCHSHRT4,
    S_RTORCHSHRT,
    S_RTORCHSHRT2,
    S_RTORCHSHRT3,
    S_RTORCHSHRT4,
    S_HANGNOGUTS,
    S_HANGBNOBRAIN,
    S_HANGTLOOKDN,
    S_HANGTSKULL,
    S_HANGTLOOKUP,
    S_HANGTNOBRAIN,
    S_COLONGIBS,
    S_SMALLPOOL,
    S_BRAINSTEM,
    S_BRNRBALL1,
    S_BRNRBALL2,
    S_BRNRBALLX1,
    S_BRNRBALLX2,
    S_BRNRBALLX3,
    S_BRNRBALLX4,
    S_BRNRBALLX5,
    S_BRNRBALLX6,
    S_NMBL1,
    S_NMBL2,
    S_NMBL3,
    S_NMBLX1,
    S_NMBLX2,
    S_NMBLX3,
    S_NMBLX4,
    S_NMBLX5,
    S_NMBLX6,
    S_RPUFF1,
    S_RPUFF2,
    S_RPUFF3,
    S_RPUFF4,
    S_RPUFF5,
    S_LGUN1,
    S_UNKFLASH1,
    S_UNKF1,
    S_UNKFDOWN,
    S_UNKFUP,
    S_UNKF2,
    S_UNKF3,
    S_LAZR1,
    S_LAZR2,
    S_LAZRDTH,
    S_LAZRDTH2,
    S_LAZRDTH3,
    S_LAZRDTH4,
    S_LAZRDUST,
    S_POW10,
    S_POW11,
    S_POW12,
    S_POW13,
    S_POW14,
    S_POW15,
    S_POW16,
    S_POW17,
    S_POW20,
    S_POW21,
    S_POW22,
    S_POW23,
    S_POW24,
    S_POW25,
    S_POW26,
    S_POW27,
    S_POW30,
    S_POW31,
    S_POW32,
    S_POW33,
    S_POW34,
    S_POW35,
    S_POW36,
    S_POW37,
    S_POW40,
    S_POW41,
    S_POW42,
    S_POW43,
    S_POW44,
    S_POW45,
    S_POW46,
    S_POW47,
    S_POW50,
    S_POW51,
    S_POW52,
    S_POW53,
    S_POW54,
    S_POW55,
    S_POW56,
    S_POW57,
    S_LAZRW1,
    S_LAZRW2,
    S_MOTH_STND,
    S_MOTH_STND2,
    S_MOTH_STND3,
    S_MOTH_STND4,
    S_MOTH_RUN1,
    S_MOTH_RUN2,
    S_MOTH_RUN3,
    S_MOTH_RUN4,
    S_MOTH_RUN5,
    S_MOTH_RUN6,
    S_MOTH_RUN7,
    S_MOTH_RUN8,
    S_MOTH_PAIN,
    S_MOTH_PAIN2,
    S_MOTH_DTH,
    S_MOTH_DTH2,
    S_MOTH_DTH3,
    S_MOTH_DTH4,
    S_MOTH_DTH5,
    S_MOTH_DTH6,
    S_MOTH_DTH7,
    S_MOTH_ATK1,
    S_MOTH_ATK2,
    S_MOTH_ATK3,
    S_MOTH_ATK4,
    S_MOTH_ATK5,
    S_SAR2_STND,
    S_SAR2_STND2,
    S_SAR2_RUN1,
    S_SAR2_RUN2,
    S_SAR2_RUN3,
    S_SAR2_RUN4,
    S_SAR2_RUN5,
    S_SAR2_RUN6,
    S_SAR2_RUN7,
    S_SAR2_RUN8,
    S_SAR2_ATK1,
    S_SAR2_ATK2,
    S_SAR2_ATK3,
    S_SAR2_PAIN,
    S_SAR2_PAIN2,
    S_SAR2_DIE1,
    S_SAR2_DIE2,
    S_SAR2_DIE3,
    S_SAR2_DIE4,
    S_SAR2_DIE5,
    S_SAR2_DIE6,
    S_RTRACER1,
    S_RTRACER2,
    S_RTRACER3,
    S_MPUFF1,
    S_MPUFF2,
    S_MPUFF3,
    S_MPUFF4,
    S_MPUFF5,
    S_EMARP_STND,
    S_EMARP_STND2,
    S_EMARP_RUN1,
    S_EMARP_RUN2,
    S_EMARP_RUN3,
    S_EMARP_RUN4,
    S_EMARP_RUN5,
    S_EMARP_RUN6,
    S_EMARP_RUN7,
    S_EMARP_RUN8,
    S_EMARP_ATK1,
    S_EMARP_ATK2,
    S_EMARP_ATK3,
    S_EMARP_ATK4,
    S_EMARP_ATK5,
    S_EMARP_PAIN,
    S_EMARP_PAIN2,
    S_BLANK,
    S_FLOATDEVICE,
    S_TECHLAMP,
    S_TECHLAMP2,
    S_TECHLAMP3,
    S_TECHLAMP4,
    S_TECH2LAMP,
    S_TECH2LAMP2,
    S_TECH2LAMP3,
    S_TECH2LAMP4,
    S_TRACER,
    S_TRACER2,
    S_TRACER3,
    S_TRACEEXP1,
    S_TRACEEXP2,
    S_TRACEEXP3,
    S_TRACEEXP4,
    S_TRACEEXP5,
    S_TRACEEXP6,
    S_SMALL_WHITE_LIGHT,

    S_ACID_RUN1,
    S_ACID_RUN2,
    S_ACID_RUN3,
    S_ACID_RUN4,
    S_ACID_RUN5,
    S_ACID_RUN7,
    S_ACID_RUN8,
    S_TEMPSOUNDORIGIN1,
    NUMSTATES
} statenum_t;

// Map objects.
typedef enum {
    MT_PLAYER,
    MT_POSSESSED,
    MT_SHOTGUY,
    MT_VILE,
    MT_FIRE,
    MT_NIGHTMARECACO,
    MT_TRACER,
    MT_SMOKE,
    MT_FATSO,
    MT_FATSHOT,
    MT_CHAINGUY,
    MT_TROOP,
    MT_SERGEANT,
    MT_SHADOWS,
    MT_HEAD,
    MT_BRUISER,
    MT_BRUISERSHOT,
    MT_KNIGHT,
    MT_SKULL,
    MT_SPIDER,
    MT_BABY,
    MT_CYBORG,
    MT_PAIN,
    MT_WOLFSS,
    MT_KEEN,
    MT_BOSSBRAIN,
    MT_BOSSSPIT,
    MT_BOSSTARGET,
    MT_SPAWNSHOT,
    MT_SPAWNFIRE,
    MT_BARREL,
    MT_TROOPSHOT,
    MT_HEADSHOT,
    MT_ROCKET,
    MT_CYBERROCKET,
    MT_PLASMA,
    MT_BFG,
    MT_ARACHPLAZ,
    MT_PUFF,
    MT_BLOOD,
    MT_TFOG,
    MT_IFOG,
    MT_HFOG,
    MT_TELEPORTMAN,
    MT_FLOATER,
    MT_EXTRABFG,
    MT_MISC0,
    MT_MISC1,
    MT_MISC2,
    MT_MISC3,
    MT_MISC4,
    MT_MISC5,
    MT_MISC6,
    MT_MISC7,
    MT_MISC8,
    MT_MISC9,
    MT_MISC10,
    MT_MISC11,
    MT_MISC12,
    MT_INV,
    MT_MISC13,
    MT_INS,
    MT_MISC14,
    MT_MISC15,
    MT_MISC16,
    MT_MEGA,
    MT_CLIP,
    MT_MISC17,
    MT_MISC18,
    MT_MISC19,
    MT_MISC20,
    MT_MISC21,
    MT_MISC22,
    MT_MISC23,
    MT_MISC24,
    MT_MISC25,
    MT_CHAINGUN,
    MT_MISC26,
    MT_MISC27,
    MT_MISC28,
    MT_SHOTGUN,
    MT_SUPERSHOTGUN,
    MT_MISC29,
    MT_MISC30,
    MT_MISC31,
    MT_MISC32,
    MT_MISC33,
    MT_MISC34,
    MT_MISC35,
    MT_MISC36,
    MT_MISC37,
    MT_MISC38,
    MT_MISC39,
    MT_MISC40,
    MT_MISC41,
    MT_MISC42,
    MT_MISC43,
    MT_MISC44,
    MT_MISC45,
    MT_MISC46,
    MT_MISC47,
    MT_MISC48,
    MT_MISC49,
    MT_MISC50,
    MT_MISC51,
    MT_MISC52,
    MT_MISC53,
    MT_MISC54,
    MT_MISC55,
    MT_MISC56,
    MT_MISC57,
    MT_MISC58,
    MT_MISC59,
    MT_MISC60,
    MT_MISC61,
    MT_MISC62,
    MT_MISC63,
    MT_MISC64,
    MT_MISC65,
    MT_MISC66,
    MT_MISC67,
    MT_MISC68,
    MT_MISC69,
    MT_MISC70,
    MT_MISC71,
    MT_MISC72,
    MT_MISC73,
    MT_MISC74,
    MT_MISC75,
    MT_MISC76,
    MT_MISC77,
    MT_MISC78,
    MT_MISC79,
    MT_MISC80,
    MT_MISC81,
    MT_MISC82,
    MT_MISC83,
    MT_MISC84,
    MT_MISC85,
    MT_MISC86,
    MT_BRUISERSHOTRED,
    MT_NTROSHOT,
    MT_ROCKETPUFF,
    MT_LASERGUN,
    MT_LASERSHOT,
    MT_LASERDUST,
    MT_LPOWERUP1,
    MT_LPOWERUP2,
    MT_LPOWERUP3,
    MT_LPOWERUP4,
    MT_LPOWERUP5,
    MT_LASERSHOTWEAK,
    MT_BITCH,
    MT_FIREEND,
    MT_BITCHBALL,
    MT_MOTHERPUFF,
    MT_DART,
    MT_RDART,
    MT_EMARINEP,

    //LIST OF SPAWN THINGS - SAMUEL
    MT_TELEPORTSHOT,
    MT_TELEPORTCHAIN,
    MT_TELEPORTSSHOT,
    MT_TELEPORTROCKET,
    MT_TELEPORTPLASMA,
    MT_TELEPORTBFG,
    MT_TELEPORTMEDKIT,
    MT_TELEPORTSTIM,
    MT_TELEPORTARMOR1,
    MT_TELEPORTARMOR2,
    MT_TELEPORTLASER,
    MT_TELEPORTLKEY1,
    MT_TELEPORTLKEY2,
    MT_TELEPORTLKEY3,
    MT_TELEPORTMEGA,
    MT_TELEPORTSOUL,
    MT_TELEPORTBLUR,
    MT_TELEPORTINVUL,
    MT_TELEPORTBERSERK,
    MT_TELEPORTPOTION,
    MT_TELEPORTHELMET,
    MT_TELEPORTMAP,
    MT_TELEPORTLIGHT,
    MT_TELEPORTSUIT,
    MT_TELEPORTSHELL,
    MT_TELEPORTSBOX,
    MT_TELEPORTCLIP,
    MT_TELEPORTBULLETS,
    MT_TELEPORTRROCKET,
    MT_TELEPORTRBOX,
    MT_TELEPORTCELL,
    MT_TELEPORTCBOX,
    MT_TELEPORTBACKPACK,
    MT_TELEPORTPOSS,
    MT_TELEPORTSPOS,
    MT_TELEPORTTROOP,
    MT_TELEPORTNTROP,
    MT_TELEPORTSARG,
    MT_TELEPORTSARG2,
    MT_TELEPORTNSARG,
    MT_TELEPORTHEAD,
    MT_TELEPORTHEAD2,
    MT_TELEPORTLOSTSOUL,
    MT_TELEPORTPAIN,
    MT_TELEPORTFATSO,
    MT_TELEPORTBABY,
    MT_TELEPORTCYBORG,
    MT_TELEPORTBITCH,
    MT_TELEPORTKNIGHT,
    MT_TELEPORTBARON,
    MT_TELEPORTRKEY,
    MT_TELEPORTRKEY2,
    MT_TELEPORTBKEY,
    MT_TELEPORTBKEY2,
    MT_TELEPORTYKEY,
    MT_TELEPORTYKEY2,
    MT_DNIGHTMARE,
    MT_NTROOP,
    MT_KABOOM,
    MT_CHAINGUNGUY,
    MT_STALKER,
    MT_NIGHTCRAWLER,
    MT_GRENADE,
    MT_ACID,
    MT_ACIDMISSILE,
    MT_TELEPORTCHAINGUY,
    MT_TELEPORTCRAWLER,
    MT_TELEPORTACID,
    MT_SUPERBONUS,
    MT_RADAR,
    MT_DOOMSDAY,
    MT_UNSEER,
    MT_LIGHTSOURCE,

    MT_TEMPSOUNDORIGIN,
    NUMMOBJTYPES
} mobjtype_t;

// Text.
typedef enum {
    TXT_D_DEVSTR,
    TXT_D_CDROM,
    TXT_PRESSKEY,
    TXT_PRESSYN,
    TXT_QUITMSG,
    TXT_LOADNET,
    TXT_QLOADNET,
    TXT_QSAVESPOT,
    TXT_SAVEDEAD,
    TXT_QSPROMPT,
    TXT_QLPROMPT,
    TXT_NEWGAME,
    TXT_NIGHTMARE,
    TXT_SWSTRING,
    TXT_MSGOFF,
    TXT_MSGON,
    TXT_NETEND,
    TXT_ENDGAME,
    TXT_DOSY,
    TXT_DETAILHI,
    TXT_DETAILLO,
    TXT_GAMMALVL0,
    TXT_GAMMALVL1,
    TXT_GAMMALVL2,
    TXT_GAMMALVL3,
    TXT_GAMMALVL4,
    TXT_EMPTYSTRING,
    TXT_GOTARMOR,
    TXT_GOTMEGA,
    TXT_GOTHTHBONUS,
    TXT_GOTARMBONUS,
    TXT_GOTSTIM,
    TXT_GOTMEDINEED,
    TXT_GOTMEDIKIT,
    TXT_GOTSUPER,
    TXT_GOTBLUECARD,
    TXT_GOTYELWCARD,
    TXT_GOTREDCARD,
    TXT_GOTBLUESKUL,
    TXT_GOTYELWSKUL,
    TXT_GOTREDSKULL,
    TXT_GOTINVUL,
    TXT_GOTBERSERK,
    TXT_GOTINVIS,
    TXT_GOTSUIT,
    TXT_GOTMAP,
    TXT_GOTVISOR,
    TXT_GOTMSPHERE,
    TXT_GOTCLIP,
    TXT_GOTCLIPBOX,
    TXT_GOTROCKET,
    TXT_GOTROCKBOX,
    TXT_GOTCELL,
    TXT_GOTCELLBOX,
    TXT_GOTSHELLS,
    TXT_GOTSHELLBOX,
    TXT_GOTBACKPACK,
    TXT_GOTBFG9000,
    TXT_GOTCHAINGUN,
    TXT_GOTCHAINSAW,
    TXT_GOTLAUNCHER,
    TXT_GOTPLASMA,
    TXT_GOTSHOTGUN,
    TXT_GOTSHOTGUN2,
    TXT_GOTUNMAKER,
    TXT_GOTPOWERUP1,
    TXT_NGOTPOWERUP1,
    TXT_GOTPOWERUP2,
    TXT_NGOTPOWERUP2,
    TXT_GOTPOWERUP3,
    TXT_NGOTPOWERUP3,
    TXT_PD_OPNPOWERUP,
    TXT_PD_BLUEO,
    TXT_PD_REDO,
    TXT_PD_YELLOWO,
    TXT_PD_BLUEK,
    TXT_PD_REDK,
    TXT_PD_YELLOWK,
    TXT_GGSAVED,
    TXT_HUSTR_MSGU,
    TXT_HUSTR_E1M01,
    TXT_HUSTR_E1M02,
    TXT_HUSTR_E1M03,
    TXT_HUSTR_E1M04,
    TXT_HUSTR_E1M05,
    TXT_HUSTR_E1M06,
    TXT_HUSTR_E1M07,
    TXT_HUSTR_E1M08,
    TXT_HUSTR_E1M09,
    TXT_HUSTR_E1M10,
    TXT_HUSTR_E1M11,
    TXT_HUSTR_E1M12,
    TXT_HUSTR_E1M13,
    TXT_HUSTR_E1M14,
    TXT_HUSTR_E1M15,
    TXT_HUSTR_E1M16,
    TXT_HUSTR_E1M17,
    TXT_HUSTR_E1M18,
    TXT_HUSTR_E1M19,
    TXT_HUSTR_E1M20,
    TXT_HUSTR_E1M21,
    TXT_HUSTR_E1M22,
    TXT_HUSTR_E1M23,
    TXT_HUSTR_E1M24,
    TXT_HUSTR_E1M25,
    TXT_HUSTR_E1M26,
    TXT_HUSTR_E1M27,
    TXT_HUSTR_E1M28,
    TXT_HUSTR_E1M29,
    TXT_HUSTR_E1M30,
    TXT_HUSTR_E1M31,
    TXT_HUSTR_E1M32,
    TXT_HUSTR_E1M33,
    TXT_HUSTR_E1M34,
    TXT_HUSTR_E1M35,
    TXT_HUSTR_E1M36,
    TXT_HUSTR_E1M37,
    TXT_HUSTR_E1M38,
    TXT_HUSTR_E1M39,
    TXT_HUSTR_E2M01,
    TXT_HUSTR_E2M02,
    TXT_HUSTR_E2M03,
    TXT_HUSTR_E2M04,
    TXT_HUSTR_E2M05,
    TXT_HUSTR_E2M06,
    TXT_HUSTR_E2M07,
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
    TXT_HUSTR_TALKTOSELF1,
    TXT_HUSTR_TALKTOSELF2,
    TXT_HUSTR_TALKTOSELF3,
    TXT_HUSTR_TALKTOSELF4,
    TXT_HUSTR_TALKTOSELF5,
    TXT_HUSTR_MESSAGESENT,
    TXT_HUSTR_PLRGREEN,
    TXT_HUSTR_PLRINDIGO,
    TXT_HUSTR_PLRBROWN,
    TXT_HUSTR_PLRRED,
    TXT_AMSTR_FOLLOWON,
    TXT_AMSTR_FOLLOWOFF,
    TXT_AMSTR_GRIDON,
    TXT_AMSTR_GRIDOFF,
    TXT_AMSTR_MARKEDSPOT,
    TXT_AMSTR_MARKSCLEARED,
    TXT_STSTR_MUS,
    TXT_STSTR_NOMUS,
    TXT_STSTR_DQDON,
    TXT_STSTR_DQDOFF,
    TXT_STSTR_KFAADDED,
    TXT_STSTR_FAADDED,
    TXT_STSTR_NCON,
    TXT_STSTR_NCOFF,
    TXT_STSTR_BEHOLD,
    TXT_STSTR_BEHOLDX,
    TXT_STSTR_CHOPPERS,
    TXT_STSTR_CLEV,
    TXT_C1TEXT,
    TXT_C2TEXT,
    TXT_C3TEXT,
    TXT_C4TEXT,
    TXT_C5TEXT,
    TXT_C6TEXT,
    TXT_C7TEXT,
    TXT_C8TEXT,
    TXT_C9TEXT,
    TXT_OUCAST1TEXT,
    TXT_OUCAST2TEXT,
    TXT_CC_ZOMBIE,
    TXT_CC_SHOTGUN,
    TXT_CC_HEAVY,
    TXT_CC_IMP,
    TXT_CC_DEMON,
    TXT_CC_LOST,
    TXT_CC_CACO,
    TXT_CC_HELL,
    TXT_CC_BARON,
    TXT_CC_ARACH,
    TXT_CC_PAIN,
    TXT_CC_REVEN,
    TXT_CC_MANCU,
    TXT_CC_ARCH,
    TXT_CC_SPIDER,
    TXT_CC_CYBER,
    TXT_CC_NTROOP,
    TXT_CC_NDEMON,
    TXT_CC_NHEAD,
    TXT_CC_BITCH,
    TXT_CC_HERO,
    TXT_QUITMESSAGE1,
    TXT_QUITMESSAGE2,
    TXT_QUITMESSAGE3,
    TXT_QUITMESSAGE4,
    TXT_QUITMESSAGE5,
    TXT_QUITMESSAGE6,
    TXT_QUITMESSAGE7,
    TXT_QUITMESSAGE8,
    TXT_QUITMESSAGE9,
    TXT_QUITMESSAGE10,
    TXT_QUITMESSAGE11,
    TXT_QUITMESSAGE12,
    TXT_QUITMESSAGE13,
    TXT_QUITMESSAGE14,
    TXT_QUITMESSAGE15,
    TXT_QUITMESSAGE16,
    TXT_QUITMESSAGE17,
    TXT_QUITMESSAGE18,
    TXT_QUITMESSAGE19,
    TXT_QUITMESSAGE20,
    TXT_QUITMESSAGE21,
    TXT_QUITMESSAGE22,
    TXT_JOINNET,
    TXT_SAVENET,
    TXT_CLNETLOAD,
    TXT_LOADMISSING,
    TXT_RENDER_GLOWFLATS,
    TXT_RENDER_GLOWTEXTURES,
    TXT_FINALEFLAT_C2,
    TXT_FINALEFLAT_C1,
    TXT_FINALEFLAT_C3,
    TXT_FINALEFLAT_C4,
    TXT_FINALEFLAT_C5,
    TXT_FINALEFLAT_C6,
    TXT_ASK_EPISODE,
    TXT_EPISODE1,
    TXT_EPISODE2,
    TXT_KILLMSG_SUICIDE,
    TXT_KILLMSG_WEAPON0,
    TXT_KILLMSG_PISTOL,
    TXT_KILLMSG_SHOTGUN,
    TXT_KILLMSG_CHAINGUN,
    TXT_KILLMSG_MISSILE,
    TXT_KILLMSG_PLASMA,
    TXT_KILLMSG_BFG,
    TXT_KILLMSG_CHAINSAW,
    TXT_KILLMSG_SUPERSHOTGUN,
    TXT_KILLMSG_UNMAKER,
    TXT_KILLMSG_STOMP,
    TXT_AMSTR_ROTATEON,
    TXT_AMSTR_ROTATEOFF,
    TXT_WEAPON0,
    TXT_WEAPON1,
    TXT_WEAPON2,
    TXT_WEAPON3,
    TXT_WEAPON4,
    TXT_WEAPON5,
    TXT_WEAPON6,
    TXT_WEAPON7,
    TXT_WEAPON8,
    TXT_WEAPON9,
    TXT_SKILL1,
    TXT_SKILL2,
    TXT_SKILL3,
    TXT_SKILL4,
    TXT_SKILL5,
    TXT_JUMPWON,
    TXT_NOCHEAT,
    NUMTEXT
} textenum_t;

#endif
