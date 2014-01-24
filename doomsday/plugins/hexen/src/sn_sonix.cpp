/** @file sn_sonix.cpp  Sound sequence scripts.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "jhexen.h"
#include "s_sequence.h"

#include "g_common.h"
#include "hexlex.h"
#include <string.h>

#define SS_MAX_SCRIPTS          64
#define SS_TEMPBUFFER_SIZE      1024
#define SS_SEQUENCE_NAME_LENGTH 32

typedef enum sscmds_e {
    SS_CMD_NONE,
    SS_CMD_PLAY,
    SS_CMD_WAITUNTILDONE, // used by PLAYUNTILDONE
    SS_CMD_PLAYTIME,
    SS_CMD_PLAYREPEAT,
    SS_CMD_DELAY,
    SS_CMD_DELAYRAND,
    SS_CMD_VOLUME,
    SS_CMD_STOPSOUND,
    SS_CMD_END
} sscmds_t;

int ActiveSequences;
seqnode_t *SequenceListHead;

static struct sstranslation_s {
    char name[SS_SEQUENCE_NAME_LENGTH];
    int scriptNum;
    int stopSound;
} SequenceTranslate[SEQ_NUMSEQ] = {
    { "Platform", 0, 0 },
    { "Platform", 0, 0 },
    { "PlatformMetal", 0, 0 },
    { "Platform", 0, 0 },
    { "Silence", 0, 0 },
    { "Lava", 0, 0 },
    { "Water", 0, 0 },
    { "Ice", 0, 0 },
    { "Earth", 0, 0 },
    { "PlatformMetal2", 0, 0 },
    { "DoorNormal", 0, 0 },
    { "DoorHeavy", 0, 0 },
    { "DoorMetal", 0, 0 },
    { "DoorCreak", 0, 0 },
    { "Silence", 0, 0 },
    { "Lava", 0, 0 },
    { "Water", 0, 0 },
    { "Ice", 0, 0 },
    { "Earth", 0, 0 },
    { "DoorMetal2", 0, 0 },
    { "Wind", 0, 0 }
};

static int *SequenceData[SS_MAX_SCRIPTS];

static void initSequenceData()
{
    for(int i = 0; i < SS_MAX_SCRIPTS; ++i)
    {
        SequenceData[i] = 0;
    }
    ActiveSequences = 0;
}

/**
 * Verifies the integrity of the temporary ptr, and ensures that the ptr
 * isn't exceeding the size of the temporary buffer
 */
static void verifySequencePtr(int *base, int *ptr)
{
    if(ptr - base > SS_TEMPBUFFER_SIZE)
    {
        Con_Error("VerifySequencePtr:  tempPtr >= %d", SS_TEMPBUFFER_SIZE);
    }
}

void SndSeqParser(Str const *path)
{
    initSequenceData();

    AutoStr *script = M_ReadFileIntoString(path, 0);

    if(!script || Str_IsEmpty(script))
        return;

    App_Log(DE2_RES_VERBOSE, "Parsing \"%s\"...", F_PrettyPath(Str_Text(path)));

    HexLex lexer;
    lexer.parse(script, path);

    int i = SS_MAX_SCRIPTS;

    int inSequence = -1;
    int *tempDataStart = 0, *tempDataPtr = 0;
    while(lexer.readToken())
    {
        if(Str_At(lexer.token(), 0) == ':')
        {
            if(inSequence != -1)
            {
                // Found an unexpected token.
                Con_Error("SndSeqParser: Unexpected token '%s' in \"%s\" on line #%i",
                          lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
            }

            tempDataStart = (int *) Z_Calloc(SS_TEMPBUFFER_SIZE, PU_GAMESTATIC, NULL);
            tempDataPtr = tempDataStart;
            for(i = 0; i < SS_MAX_SCRIPTS; ++i)
            {
                if(SequenceData[i] == NULL)
                {
                    break;
                }
            }

            if(i == SS_MAX_SCRIPTS)
            {
                Con_Error("SndSeqParser: Number of SS Scripts >= SS_MAX_SCRIPTS");
            }

            for(int k = 0; k < SEQ_NUMSEQ; ++k)
            {
                if(!strcasecmp(SequenceTranslate[k].name, Str_Text(lexer.token()) + 1))
                {
                    SequenceTranslate[k].scriptNum = i;
                    inSequence = k;
                    break;
                }
            }
            continue; // parse the next command
        }

        if(inSequence == -1)
        {
            continue;
        }

        if(!Str_CompareIgnoreCase(lexer.token(), "end"))
        {
            *tempDataPtr++ = SS_CMD_END;
            int dataSize = (tempDataPtr - tempDataStart) * sizeof(int);
            SequenceData[i] = (int *) Z_Malloc(dataSize, PU_GAMESTATIC, NULL);
            memcpy(SequenceData[i], tempDataStart, dataSize);
            Z_Free(tempDataStart);
            inSequence = -1;
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "playrepeat"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_PLAYREPEAT;
            *tempDataPtr++ = lexer.readSoundIndex();
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "playtime"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_PLAY;
            *tempDataPtr++ = lexer.readSoundIndex();
            *tempDataPtr++ = SS_CMD_DELAY;
            *tempDataPtr++ = lexer.readNumber();
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "playuntildone"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_PLAY;
            *tempDataPtr++ = lexer.readSoundIndex();
            *tempDataPtr++ = SS_CMD_WAITUNTILDONE;
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "play"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_PLAY;
            *tempDataPtr++ = lexer.readSoundIndex();
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "delayrand"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_DELAYRAND;
            *tempDataPtr++ = lexer.readNumber();
            *tempDataPtr++ = lexer.readNumber();
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "delay"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_DELAY;
            *tempDataPtr++ = lexer.readNumber();
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "volume"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_VOLUME;
            *tempDataPtr++ = lexer.readNumber();
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "stopsound"))
        {
            SequenceTranslate[inSequence].stopSound = lexer.readSoundIndex();
            *tempDataPtr++ = SS_CMD_STOPSOUND;
            continue;
        }

        // Found an unexpected token.
        Con_Error("SndSeqParser: Unexpected token '%s' in \"%s\" on line #%i",
                  lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
    }
}

void SN_StartSequence(mobj_t *mobj, int sequence)
{
    if(!mobj) return;

    SN_StopSequence(mobj); // Stop any previous sequence

    seqnode_t *node = (seqnode_t *) Z_Calloc(sizeof(*node), PU_GAMESTATIC, NULL);

    node->sequencePtr = SequenceData[SequenceTranslate[sequence].scriptNum];
    node->sequence    = sequence;
    node->mobj        = mobj;
    node->delayTics   = 0;
    node->stopSound   = SequenceTranslate[sequence].stopSound;
    node->volume      = 127; // Start at max volume

    if(!SequenceListHead)
    {
        SequenceListHead = node;
        node->next = node->prev = 0;
    }
    else
    {
        SequenceListHead->prev = node;
        node->next = SequenceListHead;
        node->prev = 0;
        SequenceListHead = node;
    }
    ActiveSequences++;
}

void SN_StartSequenceInSec(Sector *sector, int seqBase)
{
    if(!sector) return;

    SN_StartSequence((mobj_t *)P_GetPtrp(sector, DMU_EMITTER),
                     seqBase + P_ToXSector(sector)->seqType);
}

void SN_StopSequenceInSec(Sector *sector)
{
    if(!sector) return;

    SN_StopSequence((mobj_t *)P_GetPtrp(sector, DMU_EMITTER));
}

void SN_StartSequenceName(mobj_t *mobj, char const *name)
{
    if(!mobj) return;

    for(int i = 0; i < SEQ_NUMSEQ; ++i)
    {
        if(!strcmp(name, SequenceTranslate[i].name))
        {
            SN_StartSequence(mobj, i);
            return;
        }
    }
}

void SN_StopSequence(mobj_t *mobj)
{
    if(!mobj) return;

    seqnode_t *node;
    seqnode_t *next = 0;
    for(node = SequenceListHead; node; node = next)
    {
        next = node->next;

        if(node->mobj == mobj)
        {
            S_StopSound(0, mobj);
            if(node->stopSound)
            {
                S_StartSoundAtVolume(node->stopSound, mobj, node->volume / 127.0f);
            }

            if(SequenceListHead == node)
            {
                SequenceListHead = node->next;
            }

            if(node->prev)
            {
                node->prev->next = node->next;
            }

            if(node->next)
            {
                node->next->prev = node->prev;
            }

            Z_Free(node);
            ActiveSequences--;
        }
    }
}

void SN_UpdateActiveSequences()
{
    if(!ActiveSequences || paused)
    {
        // No sequences currently playing/game is paused
        return;
    }

    for(seqnode_t *node = SequenceListHead; node; node = node->next)
    {
        if(node->delayTics)
        {
            node->delayTics--;
            continue;
        }

        // If ID is zero, S_IsPlaying returns true if any sound is playing.
        dd_bool sndPlaying = (node->currentSoundID ? S_IsPlaying(node->currentSoundID, node->mobj) : false);

        switch(*node->sequencePtr)
        {
        case SS_CMD_PLAY:
            if(!sndPlaying)
            {
                node->currentSoundID = *(node->sequencePtr + 1);

                App_Log(DE2_DEV_AUDIO_VERBOSE, "SS_CMD_PLAY: StartSound %s: %p",
                        SequenceTranslate[node->sequence].name, node->mobj);

                S_StartSoundAtVolume(node->currentSoundID, node->mobj,
                                     node->volume / 127.0f);
            }
            node->sequencePtr += 2;
            break;

        case SS_CMD_WAITUNTILDONE:
            if(!sndPlaying)
            {
                node->sequencePtr++;
                node->currentSoundID = 0;
            }
            break;

        case SS_CMD_PLAYREPEAT:
            if(!sndPlaying)
            {
                App_Log(DE2_DEV_AUDIO_VERBOSE,
                        "SS_CMD_PLAYREPEAT: StartSound id=%i, %s: %p",
                        node->currentSoundID, SequenceTranslate[node->sequence].name, node->mobj);

                node->currentSoundID = *(node->sequencePtr + 1);

                S_StartSoundAtVolume(node->currentSoundID | DDSF_REPEAT,
                                     node->mobj, node->volume / 127.0f);
            }
            break;

        case SS_CMD_DELAY:
            node->delayTics = *(node->sequencePtr + 1);
            node->sequencePtr += 2;
            node->currentSoundID = 0;
            break;

        case SS_CMD_DELAYRAND:
            node->delayTics =
                *(node->sequencePtr + 1) +
                M_Random() % (*(node->sequencePtr + 2) -
                              *(node->sequencePtr + 1));
            node->sequencePtr += 2;
            node->currentSoundID = 0;
            break;

        case SS_CMD_VOLUME:
            node->volume = (127 * (*(node->sequencePtr + 1))) / 100;
            node->sequencePtr += 2;
            break;

        case SS_CMD_STOPSOUND:
            // Wait until something else stops the sequence
            break;

        case SS_CMD_END:
            SN_StopSequence(node->mobj);
            break;

        default:
            break;
        }
    }
}

void SN_StopAllSequences()
{
    seqnode_t *node;
    seqnode_t *next = 0;

    for(node = SequenceListHead; node; node = next)
    {
        next = node->next;
        node->stopSound = 0;    // don't play any stop sounds
        SN_StopSequence(node->mobj);
    }
}

int SN_GetSequenceOffset(int sequence, int *sequencePtr)
{
    return (sequencePtr - SequenceData[SequenceTranslate[sequence].scriptNum]);
}

void SN_ChangeNodeData(int nodeNum, int seqOffset, int delayTics, int volume,
    int currentSoundID)
{
    int n = 0;
    seqnode_t *node = SequenceListHead;
    while(node && n < nodeNum)
    {
        node = node->next;
        n++;
    }
    if(!node) return;

    node->delayTics       = delayTics;
    node->volume          = volume;
    node->sequencePtr    += seqOffset;
    node->currentSoundID  = currentSoundID;
}
