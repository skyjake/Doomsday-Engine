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

#ifndef __AMETHYST_CALL_STACK_H__
#define __AMETHYST_CALL_STACK_H__

#include "list.h"
#include "shard.h"

class CallStack
{
public:
    CallStack();

    bool isEmpty() { return _stack.isListEmpty(); }
    Shard *get();
    void push(Shard *caller);
    Shard *pop();

private:
    List<Shard> _stack;
};

#endif
