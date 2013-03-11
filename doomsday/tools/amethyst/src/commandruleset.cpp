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

#include "commandruleset.h"

CommandRuleSet::CommandRuleSet()
{
    _root.setRoot();
}

void CommandRuleSet::clear()
{
    _root.destroy();
}

CommandRule *CommandRuleSet::add(CommandRule *r)
{
    _root.addBefore(r);
    return r;
}

CommandRule *CommandRuleSet::remove(CommandRule *r)
{
    r->remove();
    return r;
}

CommandRule *CommandRuleSet::find(const String& byName)
{
    for(CommandRule *r = _root.next(); !r->isRoot(); r = r->next())
        if(r->name() == byName) return r;
    return &_defaultRule;
}

void CommandRuleSet::newRule(const String& name, const GemClass &gc, int flags,
                              const String& args)
{
    add(new CommandRule(name, gc, flags, args));
}

/**
 * Fill the ruleset with the standard Amethyst commands.
 */
void CommandRuleSet::initStandardRules()
{
    // Remove any existing rules.
    clear();

    // By default all commands are independent and breaking.
    // Independent commands have a Gem of their own, and breaking ones
    // require a Break gem in front.

    newRule("output",   GemClass(), CRF_TIDY, "b");
    //newRule("input",    GemClass(), CRF_TIDY, "b");
    newRule("include",  GemClass(), CRF_TIDY, "t");
    newRule("require",  GemClass(), CRF_TIDY, "t");
    newRule("format",   GemClass(), CRF_TIDY, "b");
    newRule("length",   GemClass(), CRF_TIDY, "b");
    newRule("apply",    GemClass(), 0, "bs");
    newRule("set",      GemClass(), 0, "bs");
    newRule("macro",    GemClass(), CRF_TIDY, "bs");
    newRule("arg",      GemClass(), 0, "b");
    newRule("rarg",     GemClass(), 0, "b");
    newRule("ifdef",    GemClass(), CRF_TIDY, "bs");
    newRule("ifndef",   GemClass(), CRF_TIDY, "bs");

    newRule("pre",      GemClass(GSF_PREFORMATTED), CRF_INDEPENDENT | CRF_LINE_BREAKING | CRF_POST_LINE_BREAKING, "t");

    // The basic style commands are naturally not breaking.
    newRule("em",       GemClass(GSF_EMPHASIZE), CRF_INDEPENDENT);
    newRule("def",      GemClass(GSF_DEFINITION), CRF_INDEPENDENT);
    newRule("kbd",      GemClass(GSF_KEYBOARD), CRF_INDEPENDENT);
    newRule("var",      GemClass(GSF_VARIABLE), CRF_INDEPENDENT);
    newRule("file",     GemClass(GSF_FILE), CRF_INDEPENDENT);
    newRule("opt",      GemClass(GSF_OPTION), CRF_INDEPENDENT);
    newRule("cmd",      GemClass(GSF_COMMAND), CRF_INDEPENDENT);
    newRule("acro",     GemClass(GSF_ACRONYM), CRF_INDEPENDENT);
    newRule("strong",   GemClass(GSF_STRONG), CRF_INDEPENDENT);
    newRule("header",   GemClass(GSF_HEADER), CRF_INDEPENDENT);
    newRule("url",      GemClass(GSF_URL), CRF_INDEPENDENT);
    newRule("email",    GemClass(GSF_EMAIL), CRF_INDEPENDENT);
    newRule("caption",  GemClass(GSF_CAPTION), CRF_INDEPENDENT);
    newRule("tag",      GemClass(GSF_TAG), CRF_INDEPENDENT);
    newRule("single",   GemClass(GSF_SINGLE), CRF_INDEPENDENT);
    newRule("double",   GemClass(GSF_DOUBLE), CRF_INDEPENDENT);
    newRule("large",    GemClass(GSF_LARGE), CRF_INDEPENDENT);
    newRule("huge",     GemClass(GSF_HUGE), CRF_INDEPENDENT);
    newRule("small",    GemClass(GSF_SMALL), CRF_INDEPENDENT);
    newRule("tiny",     GemClass(GSF_TINY), CRF_INDEPENDENT);
    
    // Pure style commands only affect other gems.
    newRule("enum",     GemClass(GSF_ENUMERATE), 0);

    newRule("br",       GemClass(), CRF_LINE_BREAKING);
    newRule("left",     GemClass(0, GemClass::FlushLeft), CRF_INDEPENDENT | CRF_LINE_BREAKING);
    newRule("right",    GemClass(0, GemClass::FlushRight), CRF_INDEPENDENT | CRF_LINE_BREAKING);
    newRule("center",   GemClass(0, GemClass::FlushCenter), CRF_INDEPENDENT | CRF_LINE_BREAKING | CRF_POST_LINE_BREAKING);

    newRule("ind",      GemClass(GemClass::Indent), CRF_INDEPENDENT | CRF_LINE_BREAKING | CRF_POST_LINE_BREAKING);
    newRule("code",     GemClass(GemClass::Indent, GSF_CODE), CRF_INDEPENDENT | CRF_LINE_BREAKING | CRF_POST_LINE_BREAKING);
    newRule("samp",     GemClass(GemClass::Indent, GSF_SAMPLE), CRF_INDEPENDENT | CRF_LINE_BREAKING| CRF_POST_LINE_BREAKING);
    newRule("cite",     GemClass(GemClass::Indent, GSF_CITE), CRF_INDEPENDENT | CRF_LINE_BREAKING| CRF_POST_LINE_BREAKING);

    newRule("part",     GemClass(GemClass::PartTitle), CRF_INDEPENDENT | CRF_BREAKING | CRF_POST_BREAKING);
    newRule("chapter",  GemClass(GemClass::ChapterTitle), CRF_INDEPENDENT | CRF_BREAKING | CRF_POST_BREAKING);
    newRule("section",  GemClass(GemClass::SectionTitle), CRF_INDEPENDENT | CRF_BREAKING | CRF_POST_BREAKING);
    newRule("subsec",   GemClass(GemClass::SubSectionTitle), CRF_INDEPENDENT | CRF_BREAKING | CRF_POST_BREAKING);
    newRule("sub2sec",  GemClass(GemClass::Sub2SectionTitle), CRF_INDEPENDENT | CRF_BREAKING | CRF_POST_BREAKING);
    newRule("sub3sec",  GemClass(GemClass::Sub3SectionTitle), CRF_INDEPENDENT | CRF_BREAKING | CRF_POST_BREAKING);
    newRule("sub4sec",  GemClass(GemClass::Sub4SectionTitle), CRF_INDEPENDENT | CRF_BREAKING | CRF_POST_BREAKING);

    newRule("break",    GemClass(), CRF_BREAKING);
    newRule("table",    GemClass(GemClass::Table), CRF_INDEPENDENT | CRF_LINE_BREAKING);
    newRule("tab",      GemClass(), 0);
    newRule("row",      GemClass(), 0);
    newRule("span",     GemClass(), 0);
    newRule("list",     GemClass(GemClass::List), CRF_INDEPENDENT | CRF_LINE_BREAKING);
    newRule("deflist",  GemClass(GemClass::DefinitionList), CRF_INDEPENDENT | CRF_LINE_BREAKING);
    newRule("item",     GemClass(), 0);

    newRule("contents", GemClass(GemClass::Contents), CRF_INDEPENDENT | CRF_LINE_BREAKING, "b");
}
