/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

#ifndef __P_SAVEG__
#define __P_SAVEG__

enum {
    sc_normal,
    sc_ploff,                   // plane offset
    sc_xg1
} sectorclass_e;

typedef enum lineclass_e {
    lc_normal,
    lc_xg1
} lineclass_t;

typedef enum {
    tc_null = -1,
    tc_end,
    tc_mobj,
    tc_xgmover,
    tc_ceiling,
    tc_door,
    tc_floor,
    tc_plat,
    tc_flash,
    tc_strobe,
#if __JDOOM__
    tc_glow,
    tc_flicker,
# if __DOOM64TC__
    tc_blink,
# endif
#else
    tc_glow,
#endif
    NUMTHINKERCLASSES
} thinkerclass_t;

void            SV_Init(void);
void            SV_SaveGameFile(int slot, char *str);
int             SV_SaveGame(char *filename, char *description);
int             SV_GetSaveDescription(char *filename, char *str);
int             SV_LoadGame(char *filename);

// Write a client savegame file.
void            SV_SaveClient(unsigned int gameid);
void            SV_ClientSaveGameFile(unsigned int game_id, char *str);
void            SV_LoadClient(unsigned int gameid);

unsigned short  SV_ThingArchiveNum(mobj_t *mo);
mobj_t         *SV_GetArchiveThing(int num);

void            SV_Write(void *data, int len);
void            SV_WriteByte(byte val);
void            SV_WriteShort(short val);
void            SV_WriteLong(long val);
void            SV_WriteFloat(float val);
void            SV_Read(void *data, int len);
byte            SV_ReadByte();
short           SV_ReadShort();
long            SV_ReadLong();
float           SV_ReadFloat();

// Misc save/load routines.
void            SV_UpdateReadMobjFlags(mobj_t *mo, int ver);
#endif
