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

#ifndef __AMETHYST_GEM_TEST_COMMAND_H__
#define __AMETHYST_GEM_TEST_COMMAND_H__

#include "string.h"
#include "linkable.h"

class Gem;

enum GemTestID
{
    InvalidGemTest, BeginTry, CheckIfPassed,
    GoSelf, GoParent, GoNext, GoPrev, GoFirst, GoLast,
    GoFollowing, GoPreceding, GoFinal, IsTop, IsMe, IsMyParent,
    IsMyAncestor, IsBreak, IsLineBreak, IsControl, GemType, GemFlushMode,
    HasFlag, ExclusiveFlag, NthChild, NthOrder, ChildCount,
    Text, TextBegins, CellWidth, NumGemTests
};

class GemTestCommand : public Linkable
{
public:
    GemTestCommand();
    GemTestCommand(GemTestID cmdId, int intArg = 0, const String& str = "", bool negate = false, bool escalate = false);
    ~GemTestCommand();

    GemTestCommand *next() { return (GemTestCommand*) Linkable::next(); }
    GemTestCommand *prev() { return (GemTestCommand*) Linkable::prev(); }

    GemTestID id() const { return _id; }
    int intArg() const { return _arg; }
    bool execute(Gem* self, Gem* test);
    bool operator == (GemTestCommand &other);
    bool negated() const { return _negate; }
    bool escalating() const { return _escalate; }

private:
    GemTestID _id;  /// Type of the command.
    int _arg;       /// Integer argument.
    String _text;   /// Text argument.
    bool _negate;
    bool _escalate;
};

#endif
