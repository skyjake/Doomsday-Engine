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

#ifndef __AMETHYST_COMMAND_RULESET_H__
#define __AMETHYST_COMMAND_RULESET_H__

#include "commandrule.h"

class CommandRuleSet
{
public:
    CommandRuleSet();

    void clear();
    CommandRule *add(CommandRule *rule);
    CommandRule *remove(CommandRule *rule);
    CommandRule *find(const String& byName);

    void initStandardRules();
    void newRule(const String& name, const GemClass &gc, int flags = CRF_DEFAULT, const String& args = "");

protected:
    CommandRule _root;
    CommandRule _defaultRule;
};

#endif
