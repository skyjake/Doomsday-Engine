/*
 * Copyright (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AMETHYST_SCHEDULE_H__
#define __AMETHYST_SCHEDULE_H__

#include "outputcontext.h"
#include "contextrelation.h"
#include "outputstate.h"

#include <QTextStream>

class Processor;

/**
 * The Schedule represents the visual appearance of the document.
 */
class Schedule
{
public:
    enum ListType { Preceding, Following };

    Schedule(Processor& proc);

    void clear();
    void dumpContexts();

    OutputContext *newContext(OutputContext *initializer = 0);
    void link(OutputContext *source, OutputContext *target);
    ContextRelation *list(ListType type, OutputContext *ctx);
    void render(QTextStream &os, bool flagStructuredOutput = true);
    bool advance(OutputState &state);

protected:
    Processor& _proc;
    OutputContext _contextRoot;
    ContextRelation _relationRoot;
};

#endif
