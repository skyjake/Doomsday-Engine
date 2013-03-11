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

#ifndef __AMETHYST_GEM_TEST_H__
#define __AMETHYST_GEM_TEST_H__

#include "string.h"
#include "linkable.h"
#include "token.h"
#include "gem.h"
#include "gemtestcommand.h"

class GemTest : public Linkable
{
public:
    GemTest();
    GemTest(Token *initWith);

    GemTest *next() { return (GemTest*) Linkable::next(); }
    GemTest *prev() { return (GemTest*) Linkable::prev(); }

    bool operator == (GemTest &other);
    void newTest(GemTestID id, int arg = 0, const String& text = "");
    void init(Token *first);
    bool test(Gem *gem);

private:
    GemTestCommand _commands;
    bool _failBit, _escalateBit, _addedBit;
};

#endif
