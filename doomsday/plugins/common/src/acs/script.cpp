/** @file script.cpp  Action Code Script (ACS), script model.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#include "acs/script.h"
#include "acs/interpreter.h"

using namespace de;

namespace acs {

Script::Args::Args()
{
    fill(0);
}

Script::Args::Args(dbyte const *cArr, dint length)
{
    length = cArr? de::min(dint( size() ), length) : 0;

    dint i = 0;
    for(; i < length; ++i)         (*this)[i] = cArr[i];
    for(; i < dint( size() ); ++i) (*this)[i] = 0;
}

// ----------------------------------------------------------------------------

DENG2_PIMPL_NOREF(Script)
{
    std::unique_ptr<EntryPoint> entrypoint;
    State state   = Inactive;
    int waitValue = 0;

    void wait(State waitState, int value)
    {
        DENG2_ASSERT((waitState == WaitingForSector)  ||
                     (waitState == WaitingForPolyobj) ||
                     (waitState == WaitingForScript));

        state     = waitState;
        waitValue = value;
    }

    void resumeIfWaitingForScript(Script const &other)
    {
        if(state != WaitingForScript) return;
        if(waitValue != other.entryPoint().scriptNumber) return;

        state = Running;
    }
};

Script::Script() : d(new Instance)
{}

Script::Script(EntryPoint const &ep) : d(new Instance)
{
    applyEntryPoint(ep);
}

String Script::describe() const
{
    EntryPoint const &ep = entryPoint();
    return "ACScript #" DE2_ESC(b) + String::number(ep.scriptNumber)
         + DE2_ESC(l) " Args: " DE2_ESC(.) DE2_ESC(i) + String::number(ep.scriptArgCount)
         + DE2_ESC(l) " Open: " DE2_ESC(.) DE2_ESC(i) + DENG2_BOOL_YESNO(ep.startWhenMapBegins);
}

String Script::description() const
{
    return DE2_ESC(l) "State: "     DE2_ESC(.) DE2_ESC(i) + stateAsText(d->state) + DE2_ESC(.)
         + DE2_ESC(l) " Wait-for: " DE2_ESC(.) DE2_ESC(i) + d->waitValue;
}

bool Script::start(Args const &args, mobj_t *activator, Line *line, int side, int delayCount)
{
    // Resume a suspended script?
    if(isSuspended())
    {
        d->state = Running;
        return true;
    }
    if(d->state == Inactive)
    {
        Interpreter::newThinker(*this, args, activator, line, side, delayCount);
        d->state = Running;
        return true;
    }
    return false;
}

bool Script::suspend()
{
    // Some states disallow suspension.
    if(d->state != Inactive && d->state != Suspended && d->state != Terminating)
    {
        d->state = Suspended;
        return true;
    }
    return false;
}

bool Script::terminate()
{
    // Some states disallow termination.
    if(d->state != Inactive && d->state != Terminating)
    {
        d->state = Terminating;
        return true;
    }
    return false;
}

Script::State Script::state() const
{
    return d->state;
}

bool Script::isRunning() const
{
    return state() == Running;
}

bool Script::isSuspended() const
{
    return state() == Suspended;
}

bool Script::isWaiting() const
{
    switch(state())
    {
    case WaitingForSector:
    case WaitingForPolyobj:
    case WaitingForScript:
        return true;
    }
    return false;
}

void Script::waitForPolyobj(int tag)
{
    d->wait(WaitingForPolyobj, tag);
}

void Script::waitForScript(int number)
{
    d->wait(WaitingForScript, number);
}

void Script::waitForSector(int tag)
{
    d->wait(WaitingForSector, tag);
}

void Script::polyobjFinished(int tag)
{
    if(d->state == WaitingForPolyobj && d->waitValue == tag)
    {
        d->state = Running;
    }
}

void Script::sectorFinished(int tag)
{
    if(d->state == WaitingForSector && d->waitValue == tag)
    {
        d->state = Running;
    }
}

Script::EntryPoint const &Script::entryPoint() const
{
    DENG2_ASSERT(d->entrypoint.get());
    return *d->entrypoint;
}

void Script::applyEntryPoint(EntryPoint const &epToCopy)
{
    d->entrypoint.reset(new EntryPoint(epToCopy));
}

void Script::write(writer_s *writer) const
{
    DENG2_ASSERT(writer);
    Writer_WriteInt16(writer, d->state);
    Writer_WriteInt16(writer, d->waitValue);
}

void Script::read(reader_s *reader)
{
    DENG2_ASSERT(reader);
    d->state     = State( Reader_ReadInt16(reader) );
    d->waitValue = Reader_ReadInt16(reader);
}

void Script::resumeIfWaitingForScript(Script const &other)
{
    DENG2_ASSERT(&other != this);
    d->resumeIfWaitingForScript(other);
}

void Script::setState(Script::State newState)
{
    d->state = newState;
}

String Script::stateAsText(State state) // static
{
    static char const *texts[] = {
        "Inactive",
        "Running",
        "Suspended",
        "Waiting for tag",
        "Waiting for polyobj",
        "Waiting for script",
        "Terminating"
    };
    if(state >= Inactive && state <= Terminating)
    {
        return texts[int(state)];
    }
    return "(invalid-acscript-state)";
}

} // namespace acs
