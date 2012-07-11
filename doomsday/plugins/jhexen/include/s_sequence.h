/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * s_sequence.h: Sound sequences.
 */

#ifndef __S_SEQUENCE_H__
#define __S_SEQUENCE_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

enum {
    SEQ_PLATFORM,
    SEQ_PLATFORM_HEAVY, // same script as a normal platform.
    SEQ_PLATFORM_METAL,
    SEQ_PLATFORM_CREAK, // same script as a normal platform.
    SEQ_PLATFORM_SILENCE,
    SEQ_PLATFORM_LAVA,
    SEQ_PLATFORM_WATER,
    SEQ_PLATFORM_ICE,
    SEQ_PLATFORM_EARTH,
    SEQ_PLATFORM_METAL2,
    SEQ_DOOR_STONE,
    SEQ_DOOR_HEAVY,
    SEQ_DOOR_METAL,
    SEQ_DOOR_CREAK,
    SEQ_DOOR_SILENCE,
    SEQ_DOOR_LAVA,
    SEQ_DOOR_WATER,
    SEQ_DOOR_ICE,
    SEQ_DOOR_EARTH,
    SEQ_DOOR_METAL2,
    SEQ_ESOUND_WIND,
    SEQ_NUMSEQ
};

typedef enum {
    SEQTYPE_STONE,
    SEQTYPE_HEAVY,
    SEQTYPE_METAL,
    SEQTYPE_CREAK,
    SEQTYPE_SILENCE,
    SEQTYPE_LAVA,
    SEQTYPE_WATER,
    SEQTYPE_ICE,
    SEQTYPE_EARTH,
    SEQTYPE_METAL2,
    SEQTYPE_NUMSEQ
} seqtype_t;

typedef struct seqnode_s {
    int*            sequencePtr;
    int             sequence;
    mobj_t*         mobj;
    int             currentSoundID;
    int             delayTics;
    int             volume;
    int             stopSound;
    struct seqnode_s* prev;
    struct seqnode_s* next;
} seqnode_t;

extern int ActiveSequences;
extern seqnode_t* SequenceListHead;

void            SN_InitSequenceScript(void);
void            SN_StartSequence(mobj_t* mobj, int sequence);
void            SN_StartSequenceInSec(Sector* sector, int seqBase);
void            SN_StartSequenceName(mobj_t* mobj, const char* name);
void            SN_StopSequence(mobj_t* mobj);
void            SN_StopSequenceInSec(Sector* sector);
void            SN_UpdateActiveSequences(void);
void            SN_StopAllSequences(void);
int             SN_GetSequenceOffset(int sequence, int* sequencePtr);
void            SN_ChangeNodeData(int nodeNum, int seqOffset, int delayTics,
                                  int volume, int currentSoundID);
#endif
