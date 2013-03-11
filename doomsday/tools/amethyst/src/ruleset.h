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

#ifndef __AMETHYST_RULESET_H__
#define __AMETHYST_RULESET_H__

#include "command.h"
#include "rule.h"
#include "lengthrule.h"
#include "formatrule.h"
#include "outputcontext.h"
#include "utils.h"

class Processor;

/**
 * Set of rules to define how to translate gems to a stream of output
 * characters.
 */
class RuleSet
{
public:
    RuleSet();
        
    void clear();
    Rule *add(Rule *r);
    Rule *remove(Rule *r);

    // Formatting rules.
    String apply(Gem *gem);
    String preApply(Gem *gem);
    String postApply(Gem *gem);
    String anchorPrependApply(Gem* gem);
    String anchorAppendApply(Gem* gem);
    String applyAs(FilterApplyMode mode, String input, Gem *gem);

    // Length rules.
    Length measure(Gem *gem);

    void generateRule(Command *command);
    void removeMatching(GemTest &terms, Rule::RuleType type = Rule::GENERIC);

private:
    Rule _root;
};

#endif
