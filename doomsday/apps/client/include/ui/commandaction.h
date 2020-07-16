/** @file commandaction.h  Action that executes a console command.
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

#ifndef CLIENT_COMMANDACTION_H
#define CLIENT_COMMANDACTION_H

#include "dd_share.h"
#include <de/action.h>
#include <de/string.h>

/**
 * Action that executes a console command.
 *
 * @ingroup ui
 */
class CommandAction : public de::Action
{
public:
    CommandAction(const de::String &cmd, int commandSource = CMDS_DDAY);

    de::String command() const { return _command; }

    void trigger();

private:
    de::String _command;
    int _source;
};

#endif // CLIENT_COMMANDACTION_H
