/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *\author Copyright © 1993-1996 by id Software, Inc.
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

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <assert.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "g_eventsequence.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct eventsequence_s {
    char* sequence;
    int (*callback) (const int* args, int);
    size_t length, pos;
    int args[2];
    int currentArg;
} eventsequence_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;
static eventsequence_t* eventSequences;
static int numEventSequences;

// CODE --------------------------------------------------------------------

/// @return              Non-zero iff the sequence was completed.
static int checkSequence(eventsequence_t* es, char key, boolean* eat)
{
    if(es->sequence[es->pos] == 0)
    {
        es->args[es->currentArg++] = key;
        es->pos++;
        *eat = true;
    }
    else if(key == es->sequence[es->pos])
    {
        es->pos++;
        // Not eating partial matches.
        *eat = false;
    }
    else
    {
        es->pos = 0;
        es->currentArg = 0;
    }

    if(es->sequence[es->pos] == 1)
    {
        es->pos++;
    }

    if(es->pos >= es->length)
    {
        es->pos = 0;
        es->currentArg = 0;
        return true;
    }

    return false;
}

void G_InitEventSequences(void)
{
    if(inited) return;
    eventSequences = 0;
    numEventSequences = 0;
    inited = true;
}

void G_ShutdownEventSequences(void)
{
    if(!inited) return;
    if(eventSequences)
        Z_Free(eventSequences);
    eventSequences = 0;
    numEventSequences = 0;
    inited = false;
}

int G_EventSequenceResponder(event_t* ev)
{
    assert(inited && ev);
    {
    boolean eat = false;
    int i;

    if(ev->type != EV_KEY || ev->state != EVS_DOWN)
        return false;

    for(i = 0; i < numEventSequences; ++i)
    {
        eventsequence_t* es = &eventSequences[i];

        if(checkSequence(es, ev->data1, &eat))
        {
            es->callback(es->args, CONSOLEPLAYER);
            return true;
        }
    }

    return eat;
    }
}

void G_AddEventSequence(const char* sequence, size_t sequenceLength, int (*callback) (const int*, int))
{
    assert(inited && sequence && sequenceLength > 0 && callback);
    {
    eventsequence_t* es;

    eventSequences = Z_Realloc(eventSequences, sizeof(eventsequence_t) * ++numEventSequences, PU_GAMESTATIC);
    es = &eventSequences[numEventSequences-1];

    es->sequence = Z_Malloc(sizeof(*es->sequence) * sequenceLength, PU_GAMESTATIC, 0);
    memcpy(es->sequence, sequence, sequenceLength);
    es->length = sequenceLength;
    es->callback = callback;
    es->pos = 0;
    es->currentArg = 0;
    memset(es->args, 0, sizeof(es->args));
    }
}
