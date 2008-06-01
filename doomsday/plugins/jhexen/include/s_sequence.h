/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 *
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
    int            *sequencePtr;
    int             sequence;
    mobj_t         *mobj;
    int             currentSoundID;
    int             delayTics;
    int             volume;
    int             stopSound;
    struct seqnode_s *prev;
    struct seqnode_s *next;
} seqnode_t;

extern int ActiveSequences;
extern seqnode_t *SequenceListHead;

void            SN_InitSequenceScript(void);
void            SN_StartSequence(mobj_t *mobj, int sequence);
void            SN_StartSequenceInSec(sector_t *sector, int seqBase);
void            SN_StartSequenceName(mobj_t *mobj, char *name);
void            SN_StopSequence(mobj_t *mobj);
void            SN_StopSequenceInSec(sector_t *sector);
void            SN_UpdateActiveSequences(void);
void            SN_StopAllSequences(void);
int             SN_GetSequenceOffset(int sequence, int *sequencePtr);
void            SN_ChangeNodeData(int nodeNum, int seqOffset, int delayTics,
                                  int volume, int currentSoundID);
#endif
