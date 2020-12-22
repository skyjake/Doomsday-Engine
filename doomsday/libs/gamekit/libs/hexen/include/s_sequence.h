/** @file s_sequence.h  Sound sequence scripts.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBHEXEN_AUDIO_SOUNDSEQUENCE_H
#define LIBHEXEN_AUDIO_SOUNDSEQUENCE_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "jhexen.h"

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Attempt to parse the script on the identified @a path as "sound sequence script" data.
 */
void SndSeqParser(const Str *path);

int SN_ActiveSequenceCount(void);
void SN_StartSequence(mobj_t *mobj, int sequence);
void SN_StartSequenceInSec(Sector *sector, int seqBase);
void SN_StartSequenceName(mobj_t *mobj, const char *name);
void SN_StopSequence(mobj_t *mobj);
void SN_StopSequenceInSec(Sector *sector);
void SN_UpdateActiveSequences(void);
void SN_StopAllSequences(void);
int SN_GetSequenceOffset(int sequence, int *sequencePtr);

/**
 * @param nodeNum  @c 0= the first node.
 */
void SN_ChangeNodeData(int nodeNum, int seqOffset, int delayTics, int volume, int currentSoundID);

void SN_WriteSequences(Writer1 *writer);
void SN_ReadSequences(Reader1 *reader, int mapVersion);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHEXEN_AUDIO_SOUNDSEQUENCE_H
