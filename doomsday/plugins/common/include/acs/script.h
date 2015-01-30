/** @file script.h  Action Code Script (ACS), script model.
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

#ifndef LIBCOMMON_ACS_SCRIPT_H
#define LIBCOMMON_ACS_SCRIPT_H

#include "common.h"
#include <array>
#include <de/reader.h>
#include <de/writer.h>
#include <de/String>

namespace acs {

class System;

/**
 * Models the high-level state of an Action Code Script (ACS).
 *
 * @ingroup playsim
 */
class Script
{
public:
    /**
     * Stores information about an ACS byte/p-code entry point.
     */
    struct EntryPoint
    {
        int const *pcodePtr     = nullptr;
        bool startWhenMapBegins = false;
        int scriptNumber        = 0;
        int scriptArgCount      = 0;
    };

    /**
     * Script arguments.
     */
    class Args : public std::array<de::dbyte, 4>
    {
    public:
        Args();
        Args(de::dbyte const *cArr, de::dint length);
    };

    /**
     * Logical script states.
     */
    enum State {
        Inactive,
        Running,
        Suspended,

        WaitingForSector,
        WaitingForPolyobj,
        WaitingForScript,

        Terminating
    };
    static de::String stateAsText(State state);

public:
    Script();
    Script(EntryPoint const &ep);

    /**
     * Composes the human-friendly, styled, textual name of the object.
     */
    de::String describe() const;

    /**
     * Composes a human-friendly, styled, textual description of the script.
     */
    de::String description() const;

    /**
     * Start/resume script interpretation if inactive/suspended.
     *
     * If currently suspended the script is instructed to resume (deferred).
     *
     * Otherwise instantiate a new script Interpreter and add it to the list of
     * thinkers for the @em current map.
     *
     * @param args       Script argument values.
     * @param activator  Map object activator, if any (can be @c nullptr).
     * @param line       Line activator, if any (can be @c nullptr).
     * @param side       Line side number.
     * @param delayTics  Number of tics to wait before interpretation begins.
     *                   (Can be used to delay processing during map startup.)
     *
     * @return  @c true if started/resumed.
     */
    bool start(Args const &args, mobj_t *activator = nullptr,
               Line *line = nullptr, int side = 0, int delayTics = 0);

    /**
     * Instruct the script to self-suspend if running (deferred).
     *
     * @return  @c true if marked for suspension.
     */
    bool suspend();

    /**
     * Instruct the script to self-terminate if running (deferred).
     *
     * @return  @c true if marked for termination.
     */
    bool terminate();

    /**
     * Returns the current logical state of the script (FYI).
     */
    State state() const;

    bool isRunning()   const;
    bool isSuspended() const;
    bool isWaiting()   const;

    void waitForPolyobj(int tag);
    void waitForScript (int number);
    void waitForSector (int tag);

    void polyobjFinished(int tag);
    void sectorFinished (int tag);

    /**
     * Returns the entry point info for the script.
     */
    EntryPoint const &entryPoint() const;

    /**
     * Reconfigure the entry point info for the script.
     */
    void applyEntryPoint(EntryPoint const &epToCopy);

    void read(Reader *reader);
    void write(Writer *writer) const;

public:  /// @todo make private:

    /**
     * Resume @em this script if it is waiting on @a other (which has just terminated).
     *
     * @param other  Script to be considered.
     */
    void resumeIfWaitingForScript(Script const &other);

    void setState(State newState);

private:
    DENG2_PRIVATE(d)
};

} // namespace acs

#endif  // LIBCOMMON_ACS_SCRIPT_H
