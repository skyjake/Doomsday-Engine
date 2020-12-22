/** @file g_eventsequence.cpp
 * Input (keyboard) event sequences. @ingroup libcommon
 *
 * @author Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

/// Base class for all sequence complete handlers.
class ISequenceCompleteHandler
{
public:
    virtual ~ISequenceCompleteHandler() = default;
    virtual void invoke(int player, EventSequenceArg* args, int numArgs) = 0;
};

/// Sequence complete Callback handler.
class SequenceCompleteHandler : public ISequenceCompleteHandler
{
public:
    SequenceCompleteHandler(eventsequencehandler_t _callback) : callback(_callback) {}
    virtual void invoke(int player, EventSequenceArg* args, int numArgs)
    {
        callback(player, args, numArgs);
    }
private:
    eventsequencehandler_t callback;
};

/// Sequence complete Command handler.
class SequenceCompleteCommandHandler : public ISequenceCompleteHandler
{
public:
    SequenceCompleteCommandHandler(const char* _commandTemplate)
    {
        Str_Set(Str_InitStd(&commandTemplate), _commandTemplate);
    }
    virtual ~SequenceCompleteCommandHandler()
    {
        Str_Free(&commandTemplate);
    }
    virtual void invoke(int player, EventSequenceArg* args, int numArgs)
    {
        if(!strchr(Str_Text(&commandTemplate), '%'))
        {
            DD_Execute(true/*silent*/, Str_Text(&commandTemplate));
            return;
        }

        // Compose the command from the template, inserting values for named arguments.
        /// @todo This logic should be extracted and made into an Str method.
        AutoStr* cmd = AutoStr_NewStd();
        // Reserve an estimate of the composed command length.
        Str_Reserve(cmd, Str_Length(&commandTemplate) + numArgs + 1);

        int len = Str_Length(&commandTemplate);
        const char* start = Str_Text(&commandTemplate);
        const char* ch = start, *substart = start;
        while(ch + 1 < start + len)
        {
            if(ch[0] == '%' && ch[1] && ch[1] != '%')
            {
                Str_PartAppend(cmd, substart, 0, ch - substart);

                if(ch[1] == 'p')
                {
                    Str_AppendChar(cmd, '0' + player);
                }
                else
                {
                    int arg = ch[1] - '0' - 1;
                    DE_ASSERT(arg >= 0 && arg < 9);
                    Str_AppendChar(cmd, char(args[arg]));
                }
                ch += 2;
                substart = ch;
            }
            else
            {
                ch++;
            }
        }
        // Add anything remaining.
        Str_Append(cmd, substart);

        DD_Execute(true/*silent*/, Str_Text(cmd));
    }
private:
    ::Str commandTemplate;
};

class EventSequence
{
public:
    EventSequence(const char* _sequence, ISequenceCompleteHandler& _handler)
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
                        App_Log(DE2_DEV_WARNING, "EventSequence: Sequence %s truncated due to bad suffix %c",
                                _sequence, ch[1]);
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
        delete &handler;
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
        DE_ASSERT(ev && ev->type == EV_KEY && ev->state == EVS_DOWN);
        const int key = ev->data1;

        if(Str_At(&sequence, pos  ) == '%' && pos+1 < Str_Length(&sequence) &&
           Str_At(&sequence, pos+1) >= '0' && Str_At(&sequence, pos+1) <= '9')
        {
            const int arg = Str_At(&sequence, pos+1) - '0' - 1;
            DE_ASSERT(arg >= 0 && arg < numArgs);
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
        handler.invoke(player, args, numArgs);
        rewind();

        return true;
    }

private:
    ::Str sequence;
    ISequenceCompleteHandler& handler;
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
    DE_ASSERT(inited);

    if(!inited) return false;

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

void G_AddEventSequence(const char* sequence, eventsequencehandler_t callback)
{
    if(!inited) Con_Error("G_AddEventSequence: Subsystem not presently initialized.");
    if(!sequence || !sequence[0] || !callback) Con_Error("G_AddEventSequence: Invalid argument(s).");

    SequenceCompleteHandler* handler = new SequenceCompleteHandler(callback);
    sequences.push_back(new EventSequence(sequence, *handler));
}

void G_AddEventSequenceCommand(const char* sequence, const char* commandTemplate)
{
    if(!inited) Con_Error("G_AddEventSequenceCommand: Subsystem not presently initialized.");
    if(!sequence || !sequence[0] || !commandTemplate || !commandTemplate[0]) Con_Error("G_AddEventSequenceCommand: Invalid argument(s).");

    SequenceCompleteCommandHandler* handler = new SequenceCompleteCommandHandler(commandTemplate);
    sequences.push_back(new EventSequence(sequence, *handler));
}
