/** @file sn_sonix.cpp  Sound sequence scripts.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <cstdio>
#include <cstring>

#include "dmu_lib.h"
#include "g_common.h"
#include "g_defs.h"
#include "hexlex.h"
#include "p_saveio.h"
#include "polyobjs.h"

#define SS_MAX_SCRIPTS          64
#define SS_TEMPBUFFER_SIZE      1024

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

struct seqnode_t
{
    int *sequencePtr;
    int sequence;
    mobj_t *mobj;
    int currentSoundID;
    int delayTics;
    int volume;
    int stopSound;

    seqnode_t *prev;
    seqnode_t *next;
};

static struct sstranslation_s {
    char name[32];
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

static int activeSequenceCount;
static seqnode_t *sequences;

static int *sequenceData[SS_MAX_SCRIPTS];

static void initSequenceData()
{
    std::memset(sequenceData, 0, sizeof(*sequenceData) * SS_MAX_SCRIPTS);
    activeSequenceCount = 0;
}

static int nextUnusedSequence()
{
    for(int i = 0; i < SS_MAX_SCRIPTS; ++i)
    {
        if(!sequenceData[i])
        {
            return i;
        }
    }
    return -1;
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

void SndSeqParser(const Str *path)
{
    initSequenceData();

    AutoStr *script = M_ReadFileIntoString(path, 0);

    if(!script || Str_IsEmpty(script))
        return;

    App_Log(DE2_RES_VERBOSE, "Parsing \"%s\"...", F_PrettyPath(Str_Text(path)));

    HexLex lexer(script, path);

    int seqNumber = -1;
    int seqCommmandIndex = -1;

    int *tempDataStart = 0, *tempDataPtr = 0;
    while(lexer.readToken())
    {
        if(Str_At(lexer.token(), 0) == ':')
        {
            if(seqCommmandIndex != -1)
            {
                // Found an unexpected token.
                Con_Error("SndSeqParser: Unexpected token '%s' in \"%s\" on line #%i",
                          lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
            }

            tempDataStart = (int *) Z_Calloc(SS_TEMPBUFFER_SIZE, PU_GAMESTATIC, NULL);
            tempDataPtr = tempDataStart;

            seqNumber = nextUnusedSequence();
            if(seqNumber < 0)
            {
                Con_Error("SndSeqParser: Number of SS Scripts >= SS_MAX_SCRIPTS");
            }

            for(int i = 0; i < SEQ_NUMSEQ; ++i)
            {
                if(!strcasecmp(SequenceTranslate[i].name, Str_Text(lexer.token()) + 1))
                {
                    SequenceTranslate[i].scriptNum = seqNumber;
                    seqCommmandIndex = i;
                    break;
                }
            }
            continue; // parse the next command
        }

        if(seqCommmandIndex == -1)
        {
            continue;
        }

        if(!Str_CompareIgnoreCase(lexer.token(), "end"))
        {
            *tempDataPtr++ = SS_CMD_END;
            int dataSize = (tempDataPtr - tempDataStart) * sizeof(int);
            sequenceData[seqNumber] = (int *) Z_Malloc(dataSize, PU_GAMESTATIC, NULL);
            std::memcpy(sequenceData[seqNumber], tempDataStart, dataSize);
            Z_Free(tempDataStart);
            seqCommmandIndex = -1;
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "playrepeat"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_PLAYREPEAT;
            *tempDataPtr++ = Defs().getSoundNumForName(Str_Text(lexer.readString()));
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "playtime"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_PLAY;
            *tempDataPtr++ = Defs().getSoundNumForName(Str_Text(lexer.readString()));
            *tempDataPtr++ = SS_CMD_DELAY;
            *tempDataPtr++ = (int)lexer.readNumber();
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "playuntildone"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_PLAY;
            *tempDataPtr++ = Defs().getSoundNumForName(Str_Text(lexer.readString()));
            *tempDataPtr++ = SS_CMD_WAITUNTILDONE;
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "play"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_PLAY;
            *tempDataPtr++ = Defs().getSoundNumForName(Str_Text(lexer.readString()));
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "delayrand"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_DELAYRAND;
            *tempDataPtr++ = (int)lexer.readNumber();
            *tempDataPtr++ = (int)lexer.readNumber();
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "delay"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_DELAY;
            *tempDataPtr++ = (int)lexer.readNumber();
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "volume"))
        {
            verifySequencePtr(tempDataStart, tempDataPtr);

            *tempDataPtr++ = SS_CMD_VOLUME;
            *tempDataPtr++ = (int)lexer.readNumber();
            continue;
        }
        if(!Str_CompareIgnoreCase(lexer.token(), "stopsound"))
        {
            SequenceTranslate[seqCommmandIndex].stopSound = Defs().getSoundNumForName(Str_Text(lexer.readString()));
            *tempDataPtr++ = SS_CMD_STOPSOUND;
            continue;
        }

        // Found an unexpected token.
        Con_Error("SndSeqParser: Unexpected token '%s' in \"%s\" on line #%i",
                  lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
    }
}

int SN_ActiveSequenceCount()
{
    return activeSequenceCount;
}

void SN_StartSequence(mobj_t *mobj, int sequence)
{
    if(!mobj) return;

    SN_StopSequence(mobj); // Stop any previous sequence

    seqnode_t *node = (seqnode_t *) Z_Calloc(sizeof(*node), PU_GAMESTATIC, NULL);

    node->sequencePtr = sequenceData[SequenceTranslate[sequence].scriptNum];
    node->sequence    = sequence;
    node->mobj        = mobj;
    node->delayTics   = 0;
    node->stopSound   = SequenceTranslate[sequence].stopSound;
    node->volume      = 127; // Start at max volume

    if(!sequences)
    {
        sequences = node;
        node->next = node->prev = 0;
    }
    else
    {
        sequences->prev = node;
        node->next = sequences;
        node->prev = 0;
        sequences = node;
    }
    activeSequenceCount++;
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

void SN_StartSequenceName(mobj_t *mobj, const char *name)
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
    for(node = sequences; node; node = next)
    {
        next = node->next;

        if(node->mobj == mobj)
        {
            S_StopSound(0, mobj);
            if(node->stopSound)
            {
                S_StartSoundAtVolume(node->stopSound, mobj, node->volume / 127.0f);
            }

            if(sequences == node)
            {
                sequences = node->next;
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
            activeSequenceCount--;
        }
    }
}

void SN_UpdateActiveSequences()
{
    if(!activeSequenceCount || paused)
    {
        // No sequences currently playing/game is paused
        return;
    }

    for(seqnode_t *node = sequences; node; node = node->next)
    {
        if(node->delayTics)
        {
            node->delayTics--;
            continue;
        }

        // If ID is zero, S_IsPlaying returns true if any sound is playing.
        bool sndPlaying = (node->currentSoundID ? CPP_BOOL(S_IsPlaying(node->currentSoundID, node->mobj)) : false);

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
            // Wait until something else stops the sequence.
            break;

        case SS_CMD_END:
            SN_StopSequence(node->mobj);
            break;

        default: break;
        }
    }
}

void SN_StopAllSequences()
{
    seqnode_t *node;
    seqnode_t *next = 0;

    for(node = sequences; node; node = next)
    {
        next = node->next;
        node->stopSound = 0; // Do not play stop sounds.
        SN_StopSequence(node->mobj);
    }
}

int SN_GetSequenceOffset(int sequence, int *sequencePtr)
{
    return (sequencePtr - sequenceData[SequenceTranslate[sequence].scriptNum]);
}

void SN_ChangeNodeData(int nodeNum, int seqOffset, int delayTics, int volume,
    int currentSoundID)
{
    int n = 0;
    seqnode_t *node = sequences;
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

void SN_WriteSequences(Writer1 *writer)
{
    Writer_WriteInt32(writer, activeSequenceCount);
    for(seqnode_t *node = sequences; node; node = node->next)
    {
        Writer_WriteByte(writer, 1); // Write a version byte.

        Writer_WriteInt32(writer, node->sequence);
        Writer_WriteInt32(writer, node->delayTics);
        Writer_WriteInt32(writer, node->volume);
        Writer_WriteInt32(writer, SN_GetSequenceOffset(node->sequence, node->sequencePtr));
        Writer_WriteInt32(writer, node->currentSoundID);

        int i = 0;
        if(node->mobj)
        {
            for(; i < numpolyobjs; ++i)
            {
                if(node->mobj == (mobj_t *) Polyobj_ById(i))
                {
                    break;
                }
            }
        }

        int difference;
        if(i == numpolyobjs)
        {
            // The sound's emitter is the sector, not the polyobj itself.
            difference = P_ToIndex(Sector_AtPoint_FixedPrecision(node->mobj->origin));
            Writer_WriteInt32(writer, 0); // 0 -- sector sound origin.
        }
        else
        {
            Writer_WriteInt32(writer, 1); // 1 -- polyobj sound origin
            difference = i;
        }

        Writer_WriteInt32(writer, difference);
    }
}

void SN_ReadSequences(Reader1 *reader, int mapVersion)
{
    // Reload and restart all sound sequences
    int numSequences = Reader_ReadInt32(reader);

    for(int i = 0; i < numSequences; ++i)
    {
        /*int ver =*/ (mapVersion >= 3)? Reader_ReadByte(reader) : 0;

        int sequence    = Reader_ReadInt32(reader);
        int delayTics   = Reader_ReadInt32(reader);
        int volume      = Reader_ReadInt32(reader);
        int seqOffset   = Reader_ReadInt32(reader);

        int soundID     = Reader_ReadInt32(reader);
        int polySnd     = Reader_ReadInt32(reader);
        int secNum      = Reader_ReadInt32(reader);

        mobj_t *sndMobj = 0;
        if(!polySnd)
        {
            sndMobj = (mobj_t*)P_GetPtr(DMU_SECTOR, secNum, DMU_EMITTER);
        }
        else
        {
            Polyobj *po = Polyobj_ById(secNum);
            if(po) sndMobj = (mobj_t*) po;
        }

        SN_StartSequence(sndMobj, sequence);
        SN_ChangeNodeData(i, seqOffset, delayTics, volume, soundID);
    }
}
