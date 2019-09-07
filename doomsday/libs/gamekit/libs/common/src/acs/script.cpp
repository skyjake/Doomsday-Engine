/** @file script.cpp  Action Code Script (ACS), script model.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

Script::Args::Args(const dbyte *cArr, dint length)
{
    length = cArr? de::min(dint( size() ), length) : 0;

    dint i = 0;
    for(; i < length; ++i)         (*this)[i] = cArr[i];
    for(; i < dint( size() ); ++i) (*this)[i] = 0;
}

// ----------------------------------------------------------------------------

DE_PIMPL_NOREF(Script)
{
    const Module::EntryPoint *entryPoint = nullptr;
    State state   = Inactive;
    int waitValue = 0;

    void wait(State waitState, int value)
    {
        DE_ASSERT((waitState == WaitingForSector)  ||
                     (waitState == WaitingForPolyobj) ||
                     (waitState == WaitingForScript));

        state     = waitState;
        waitValue = value;
    }

    void resumeIfWaitingForScript(const Script &other)
    {
        if(state != WaitingForScript) return;
        if(waitValue != other.entryPoint().scriptNumber) return;

        state = Running;
    }
};

Script::Script() : d(new Impl)
{}

Script::Script(const Module::EntryPoint &ep) : d(new Impl)
{
    setEntryPoint(ep);
}

String Script::describe() const
{
    const Module::EntryPoint &ep = entryPoint();
    return "ACScript #" DE2_ESC(b) + String::asText(ep.scriptNumber)
         + DE2_ESC(l) " Args: " DE2_ESC(.) DE2_ESC(i) + String::asText(ep.scriptArgCount)
         + DE2_ESC(l) " Open: " DE2_ESC(.) DE2_ESC(i) + DE_BOOL_YESNO(ep.startWhenMapBegins);
}

String Script::description() const
{
    return DE2_ESC(l) "State: " DE2_ESC(.) DE2_ESC(i) + stateAsText(d->state) + DE2_ESC(.)
         + (isWaiting()? DE2_ESC(l) " Wait-for: " DE2_ESC(.) DE2_ESC(i) + String::asText(d->waitValue) : "");
}

bool Script::start(const Args &args, mobj_t *activator, Line *line, int side, int delayCount)
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
    return d->state == Running;
}

bool Script::isSuspended() const
{
    return d->state == Suspended;
}

bool Script::isWaiting() const
{
    switch(d->state)
    {
    case WaitingForScript:
    case WaitingForSector:
    case WaitingForPolyobj:
        return true;

    default:
        return false;
    }
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

const Module::EntryPoint &Script::entryPoint() const
{
    DE_ASSERT(d->entryPoint);
    return *d->entryPoint;
}

void Script::setEntryPoint(const Module::EntryPoint &entryPoint)
{
    d->entryPoint = &entryPoint;
}

void Script::write(writer_s *writer) const
{
    DE_ASSERT(writer);
    Writer_WriteInt16(writer, d->state);
    Writer_WriteInt16(writer, d->waitValue);
}

void Script::read(reader_s *reader)
{
    DE_ASSERT(reader);
    d->state     = State( Reader_ReadInt16(reader) );
    d->waitValue = Reader_ReadInt16(reader);
}

void Script::resumeIfWaitingForScript(const Script &other)
{
    if(&other == this) return;
    d->resumeIfWaitingForScript(other);
}

void Script::setState(Script::State newState)
{
    d->state = newState;
}

String Script::stateAsText(State state) // static
{
    static const char *texts[] = {
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
