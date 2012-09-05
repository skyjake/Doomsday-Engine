/**
 * @file g_eventsequence.c
 * Input (keyboard) event sequences. @ingroup libcommon
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 1999 Activision
 * @author Copyright &copy; 1993-1996 by id Software, Inc.
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

#include <cstring>
#include <vector>

#include "common.h"
#include "g_eventsequence.h"

class EventSequence
{
public:
    EventSequence(const char* _sequence, eventsequencehandler_t _handler)
        : sequence(), handler(_handler), pos(0), numArgs(0), args(0)
    {
        int len = strlen(_sequence);

        if(strchr(_sequence, '%'))
        {
            // Count and validate arguments defined within the sequence.
            const char* ch = _sequence;
            while(ch + 1 < _sequence + len)
            {
                if(ch[0] == '%' && ch[1] && ch[1] != '%')
                {
                    int arg = ch[1] - '0';
                    if(arg < 1 || arg > 9)
                    {
                        Con_Message("Warning: EventSequence: Bad suffix %c in sequence %s, sequence truncated.", *ch, _sequence);
                        len = ch - _sequence;
                        break;
                    }
                    numArgs += 1;
                    ch += 2;
                }
                else
                {
                    ch++;
                }
            }
        }

        Str_PartAppend(Str_Init(&sequence), _sequence, 0, len);
        if(numArgs)
        {
            args = new EventSequenceArg[numArgs];
            for(int i = 0; i < numArgs; ++i)
            {
                args[i] = 0;
            }
        }
    }

    ~EventSequence()
    {
        Str_Free(&sequence);
        if(args) delete[] args;
    }

    /**
     * Rewind the sequence and forget any current progress (and arguments).
     * @return
     */
    EventSequence& rewind()
    {
        pos = 0;
        return *this;
    }

    /// @return  @c true= @a the sequence was completed.
    bool complete(event_t* ev, int player, bool* eat)
    {
        DENG_ASSERT(ev && ev->type == EV_KEY && ev->state == EVS_DOWN);
        const int key = ev->data1;

        if(Str_At(&sequence, pos  ) == '%' && pos+1 < Str_Length(&sequence) &&
           Str_At(&sequence, pos+1) >= '0' && Str_At(&sequence, pos+1) <= '9')
        {
            const int arg = Str_At(&sequence, pos+1) - '0' - 1;
            DENG_ASSERT(arg >= 0 && arg < numArgs);
            EventSequenceArg& currentArg = args[arg];

            currentArg = key;
            pos += 2;

            if(eat) *eat = true;
        }
        else if(key == Str_At(&sequence, pos))
        {
            pos++;
            // Not eating partial matches.
            if(eat) *eat = false;
        }
        else
        {
            // Rewind the sequence.
            rewind();
        }

        if(pos < Str_Length(&sequence)) return false;

        // Sequence completed.
        handler(player, args, numArgs);
        rewind();

        return true;
    }

private:
    Str sequence;
    eventsequencehandler_t handler;
    int pos;
    int numArgs;
    EventSequenceArg* args;
};

typedef std::vector<EventSequence*> EventSequences;

static bool inited = false;
static EventSequences sequences;

static void clearSequences()
{
    for(EventSequences::iterator i = sequences.begin(); i != sequences.end(); ++i)
    {
        delete *i;
    }
    sequences.clear();
}

void G_InitEventSequences(void)
{
    // Allow re-init.
    if(inited)
    {
        clearSequences();
    }
    inited = true;
}

void G_ShutdownEventSequences(void)
{
    if(!inited) return;
    clearSequences();
    inited = false;
}

int G_EventSequenceResponder(event_t* ev)
{
    if(!inited) Con_Error("G_EventSequenceResponder: Subsystem not presently initialized.");

    // We are only interested in key down events.
    if(!ev || ev->type != EV_KEY || ev->state != EVS_DOWN) return false;

    // Try each sequence in turn.
    const int player = CONSOLEPLAYER; /// @todo Should be defined by the event...
    bool eventWasEaten = false;
    for(EventSequences::iterator i = sequences.begin(); i != sequences.end(); ++i)
    {
        EventSequence* seq = *i;
        if(seq->complete(ev, player, &eventWasEaten)) return true;
    }

    return eventWasEaten;
}

void G_AddEventSequence(const char* sequence, eventsequencehandler_t handler)
{
    if(!inited) Con_Error("G_AddEventSequence: Subsystem not presently initialized.");
    if(!sequence || !sequence[0] || !handler) Con_Error("G_AddEventSequence: Invalid argument(s).");

    sequences.push_back(new EventSequence(sequence, handler));
}
