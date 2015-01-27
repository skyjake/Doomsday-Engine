/** @file interpreter.h  Action Code Script (ACS) interpreter.
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

#ifndef LIBCOMMON_ACS_INTERPRETER_H
#define LIBCOMMON_ACS_INTERPRETER_H

#if __cplusplus
#  include "acs/script.h"
#  include "mapstatereader.h"
#  include "mapstatewriter.h"
#endif

#define ACS_INTERPRETER_MAX_SCRIPT_ARGS     10
#define ACS_INTERPRETER_SCRIPT_STACK_DEPTH  32

#ifdef __cplusplus

namespace acs {

class System;

/**
 * Action Code Script (ACS) interpreter (thinker).
 *
 * @ingroup playsim
 */
struct Interpreter
{
    thinker_t thinker;
    struct mobj_s *activator;
    Line *line;
    int side;
    void *_script;
    int delayCount;
    struct Stack { // Local value stack.
        int values[ACS_INTERPRETER_SCRIPT_STACK_DEPTH];
        int height;

        void push(int value);
        int pop();
        int top() const;
        void drop();
    } locals;
    int args[ACS_INTERPRETER_MAX_SCRIPT_ARGS];
    int const *pcodePtr;

    acs::System &scriptSys() const;

    /**
     * Returns the Script data for the thinker.
     */
    acs::Script &script() const;

    void think();

    /**
     * Deserialize the thinker from the currently open save file.
     */
    int read(MapStateReader *msr);

    /**
     * Serialize the thinker to the currently open save file.
     */
    void write(MapStateWriter *msw) const;

    /**
     * @param script      Logical ACS script-state instance.
     * @param scriptArgs  Args passed to the script.
     * @param delayCount  Delay in tics to wait before interpretation begins.
     */
    static thinker_s *newThinker(acs::Script &script, acs::Script::Args const &scriptArgs,
                                 struct mobj_s *activator = nullptr, Line *line = nullptr,
                                 int side = 0, int delayCount = 0);
};

}  // namespace acs
#endif  // __cplusplus

// C wrapper API ---------------------------------------------------------------

// Opaque type for interpreter instances.
struct acs_Interpreter;

#ifdef __cplusplus
extern "C" {
#endif

void acs_Interpreter_Think(acs_Interpreter *interp);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LIBCOMMON_ACS_INTERPRETER_H
