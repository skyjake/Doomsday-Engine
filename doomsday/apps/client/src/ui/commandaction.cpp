/** @file commandaction.cpp  Action that executes a console command.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_platform.h"
#include "ui/commandaction.h"
#include <doomsday/console/exec.h>

using namespace de;

CommandAction::CommandAction(const String &cmd, int commandSource)
    : _command(cmd), _source(commandSource)
{}

void CommandAction::trigger()
{
    Action::trigger();

    Con_Execute(_source, _command, false /*silent*/, false /*net*/);
}
