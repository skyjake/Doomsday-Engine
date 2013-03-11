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

#include "ruleset.h"

RuleSet::RuleSet()
{
    _root.setRoot();
}

Rule *RuleSet::add(Rule *r)
{
    _root.addBefore(r);
    return r;
}

Rule *RuleSet::remove(Rule *r)
{
    r->remove();
    return r;
}

void RuleSet::clear()
{
    _root.destroy();
}

String RuleSet::apply(Gem *gem)
{
    return applyAs(ApplyNormal, gem->text(), gem);
}

String RuleSet::preApply(Gem *gem)
{
    return applyAs(ApplyPre, gem->text(), gem);
}

String RuleSet::postApply(Gem *gem)
{
    return applyAs(ApplyPost, gem->text(), gem);
}

String RuleSet::anchorPrependApply(Gem* gem)
{
    return applyAs(ApplyAnchorPrepend, gem->text(), gem);
}

String RuleSet::anchorAppendApply(Gem* gem)
{
    return applyAs(ApplyAnchorAppend, gem->text(), gem);
}

/**
 * Apply the gem to all matching rules, in the given mode.
 */
String RuleSet::applyAs(FilterApplyMode mode, String input, Gem *gem)
{
    String output;
    bool firstMatch = true;
    FormatRule *rule;

    for(Rule *it = _root.next(); !it->isRoot(); it = it->next())
        if(it->type() == Rule::FORMAT)
        {
            rule = (FormatRule*) it;
            if((mode == ApplyNormal
                || (mode == ApplyPre && rule->hasPre())
                || (mode == ApplyPost && rule->hasPost())
                || (mode == ApplyAnchorPrepend && rule->hasAnchorPrepend())
                || (mode == ApplyAnchorAppend && rule->hasAnchorAppend()))
                && rule->matches(gem))
            {
                output = ((FormatRule*)it)->apply(mode, firstMatch? input : output, gem);
                firstMatch = false;
            }
        }

    // Finally, apply a class specific filter.
    if(mode == ApplyNormal && gem->gemClass().hasFilter())
    {
        output = applyFilter(output, gem->gemClass().filter(), mode, gem);
    }
    return output;
}

/**
 * The length rules are browsed backwards, so later rules override earlier
 * ones. The first matching set length is returned.
 */
Length RuleSet::measure(Gem *gem)
{
    // Start with the gem's own lenghs.
    Length len(gem->gemClass().length());

    // Did the gem specify all of the lengths?
    if(len.allSet()) return len;
    
    for(Rule *it = _root.prev(); !it->isRoot(); it = it->prev())
        if(it->type() == Rule::LENGTH)
        {
            Length &other = ((LengthRule*)it)->length();
            if(len.canLearnFrom(other)
                && it->matches(gem))
            {
                // This'll copy all the set lengths not already set.
                len += other;

                // Are we done?
                if(len.allSet()) return len;
            }
        }

    // Anything not set at this point is zero.
    len.defaults();
    return len;
}

/**
 * An exception will be thrown if there is an error.
 */
void RuleSet::generateRule(Command *command)
{
    Rule *rule;
    Shard *it;
    GemTest *terms;

    // Create the rule object.
    if(command->isName("format"))
    {
        rule = new FormatRule(((Block*)command->last()->first())->collect());
    }
    else 
    {
        rule = new LengthRule;
    }
    terms = &rule->terms();

    // Compile the terms (command -> shards -> blocks -> tokens).
    for(it = command->first();
        it && it != command->last(); it = it->next())
    {
        if(!it->first()) continue;
        terms->addBefore(new GemTest((Token*)it->first()->first()));
    }

    if(command->isName("format"))
    {               
        // There must not be format rules with matching terms.
        removeMatching(*terms, Rule::FORMAT);
        add(rule);
    }
    else 
    {
        add(rule);
        Length *len = &((LengthRule*)rule)->length();
        // Set the lengths that were given.
        // Get last argument -> block -> first token.
        len->init((Token*)command->last()->first()->first());
        // Was this all for naught?
        if(len->isClear()) delete remove(rule);
    }
}

/**
 * Removes all rules with an identical set of terms.
 */
void RuleSet::removeMatching(GemTest &terms, Rule::RuleType type)
{
    Rule *next;
    for(Rule *it = _root.next(); !it->isRoot(); it = next)
    {
        next = it->next();
        if(it->type() == type && it->_terms == terms)
            delete remove(it);
    }
}
