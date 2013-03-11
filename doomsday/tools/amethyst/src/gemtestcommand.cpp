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

#include "gemtestcommand.h"
#include "utils.h"
#include "gem.h"

GemTestCommand::GemTestCommand() : Linkable(true)
{
    _id = GoSelf;
    _arg = 0;
    _negate = false;
}

GemTestCommand::GemTestCommand
    (GemTestID cmdId, int intArg, const String& str, bool neg, bool escalate) : Linkable()
{
    _id = cmdId;
    _text = str;
    _arg = intArg;
    _negate = neg;
    _escalate = escalate;
}

GemTestCommand::~GemTestCommand()
{}

/**
 * Called for the root of the list!
 */
bool GemTestCommand::operator == (GemTestCommand &other)
{
    if(count() != other.count()) return false;
    for(GemTestCommand *mine = next(), *yours = other.next();
        !mine->isRoot(); mine = mine->next(), yours = yours->next())
    {
        if(mine->_id != yours->_id) return false;
        if(mine->_arg != yours->_arg) return false;
        if(mine->_negate != yours->_negate) return false;
        if(mine->_text != yours->_text) return false;
    }
    return true;
}

bool GemTestCommand::execute(Gem* self, Gem* test)
{
    bool result = false;

    switch(_id)
    {
    case HasFlag:
        return (test->style() & _arg) != 0;

    case GemType:
        return test->gemType() == _arg;

    case GemFlushMode:
        return (test->gemClass().flushMode() & _arg) != 0;

    case Text:
        return (_text == test->text());

    case TextBegins:
        return test->text().startsWith(_text);

    case NthChild:
    {
        result = false;
        Gem* origin = test->parentGem();
        if(!origin) break;
        int target = qAbs(_arg);
        int i = 1; // Counts the order.
        Gem* it;
        for(it = _arg > 0? origin->firstGem()
            : origin->lastGem(); it && it != test;
            it = _arg > 0? it->nextGem() : it->prevGem(), i++)
        {
            if(i == target)
            {
                // Didn't match it == test...
                return false;
            }
        }
        if(!it) break;
        result = i == target;
        break;
    }

    case NthOrder:
    {
        result = false;
        Gem* origin = test->parentGem();
        if(!origin) break;
        int target = qAbs(_arg);
        int i = 0; // Counts the order.
        for(Gem* it = _arg > 0? origin->firstGem() : origin->finalGem();
            it && it != origin->nextGem() && it != origin;
            it = _arg > 0? it->followingGem() : it->precedingGem())
        {
            if(!it->isControl()) i++;
            if(it == test)
            {
                result = true;
                break;
            }
            if(i >= target) return false;
        }
        if(!result) break; // Not even found...
        result = i == target;
        break;
    }

    case IsTop:
        return test->parent() == NULL;

    case IsMe:
        return test == self;

    case IsMyParent:
        return test == self->parentGem();

    case IsMyAncestor:
        result = false;
        for(Gem* it = self->parentGem(); it; it = it->parentGem())
            if(it == test)
            {
                result = true;
                break;
            }
        break;

    case IsBreak:
        return test->isBreak();

    case IsLineBreak:
        return test->isLineBreak();

    case IsControl:
        return test->isControl();

    case ExclusiveFlag:
        return (test->style() & ~_arg) == 0;

    case ChildCount:
        return test->count() == _arg;

    case CellWidth:
        return test->width() == _arg;

    default:
        qFatal("GemTestCommand: Invalid test command.");
        break;
    }

    return result;
}
