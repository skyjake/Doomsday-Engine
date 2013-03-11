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

#ifndef __AMETHYST_RULE_H__
#define __AMETHYST_RULE_H__

#include "linkable.h"
#include "block.h"
#include "gemtest.h"

/**
 * The base class for rules in a rule set.
 * @see RuleSet
 */
class Rule : public Linkable
{
public:
    enum RuleType { GENERIC, FORMAT, LENGTH };

public:
    Rule();
    virtual ~Rule() {}

    GemTest &terms() { return _terms; }
    Rule *next() { return (Rule*) Linkable::next(); }
    Rule *prev() { return (Rule*) Linkable::prev(); }
    RuleType type() { return _type; }
    bool matches(Gem *gem);
    
    friend class RuleSet;

protected:
    RuleType _type;
    GemTest _terms; // At least one must be fully met.
};

#endif
