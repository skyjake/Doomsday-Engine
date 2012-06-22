/**\file p_savedef.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_SAVEGAME_DEFS_H
#define LIBCOMMON_SAVEGAME_DEFS_H

#if __JDOOM__
# define MY_SAVE_MAGIC         0x1DEAD666
# define MY_CLIENT_SAVE_MAGIC  0x2DEAD666
# define MY_SAVE_VERSION       10
# define SAVESTRINGSIZE        24
# define CONSISTENCY           0x2c
# define SAVEGAMENAME          "DoomSav"
# define CLIENTSAVEGAMENAME    "DoomCl"
# define SAVEGAMEEXTENSION     "dsg"
# define SAVEGAME_DEFAULT_DIR  "savegame"

# define AUTO_SLOT             9

#elif __JDOOM64__
# define MY_SAVE_MAGIC         0x1D6420F4
# define MY_CLIENT_SAVE_MAGIC  0x2D6420F4
# define MY_SAVE_VERSION       10
# define SAVESTRINGSIZE        24
# define CONSISTENCY           0x2c
# define SAVEGAMENAME          "D64Sav"
# define CLIENTSAVEGAMENAME    "D64Cl"
# define SAVEGAMEEXTENSION     "6sg"
# define SAVEGAME_DEFAULT_DIR  "savegame"

# define AUTO_SLOT             9

#elif __JHERETIC__
# define MY_SAVE_MAGIC         0x7D9A12C5
# define MY_CLIENT_SAVE_MAGIC  0x1062AF43
# define MY_SAVE_VERSION       10
# define SAVESTRINGSIZE        24
# define CONSISTENCY           0x9d
# define SAVEGAMENAME          "HticSav"
# define CLIENTSAVEGAMENAME    "HticCl"
# define SAVEGAMEEXTENSION     "hsg"
# define SAVEGAME_DEFAULT_DIR  "savegame"

# define AUTO_SLOT             9

#elif __JHEXEN__
# define MY_SAVE_MAGIC         0x1B17CC00
# define MY_CLIENT_SAVE_MAGIC  0x2B17CC00
# define MY_SAVE_VERSION       10
# define SAVESTRINGSIZE        24
# define SAVEGAMENAME          "hex"
# define CLIENTSAVEGAMENAME    "hexencl"
# define SAVEGAMEEXTENSION     "hxs"
# define SAVEGAME_DEFAULT_DIR  "hexndata"

# define MOBJ_XX_PLAYER        -2
# define BASE_SLOT             6
# define AUTO_SLOT             7

typedef union saveptr_u {
    byte        *b;
    short       *w;
    int         *l;
    float       *f;
} saveptr_t;

typedef struct targetplraddress_s {
    void     **address;
    struct targetplraddress_s *next;
} targetplraddress_t;

#endif

#if !__JHEXEN__
#define PRE_VER5_END_SPECIALS   7
#endif

#endif
