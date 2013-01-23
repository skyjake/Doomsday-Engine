/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_RULE_H
#define LIBDENG2_RULE_H

#include <QObject>
#include <QSet>
#include "../libdeng2.h"
#include "../Counted"

namespace de {

/**
 * Rules are used together to evaluate formulas dependent on other rules.
 *
 * - Rules are scalar.
 * - Every rule knows its current value: querying it is a O(1) operation.
 * - Every rule knows where its value comes from / how it's generated.
 * - When the value changes, all dependent rules are notified and marked as invalid.
 * - When a rule is invalid, its current value will be updated (i.e., validated).
 * - Rules can be replaced dynamically with other rules, see Rule::replace().
 * - Reference counting is used for lifetime management.
 *
 * @ingroup widgets
 */
class Rule : public QObject, public Counted
{
    Q_OBJECT

public:
    explicit Rule(float initialValue = 0);

    /**
     * Determines the rule's current value. If it has been marked invalid,
     * the value is updated first (see update()).
     */
    float value() const;

#if 0
    /**
     * Transfers this rule's dependencies to @a newRule. The dependent rules
     * are updated accordingly. Afterwards, this rule has no more dependencies.
     */
    void transferDependencies(Rule *toRule);
#endif

    /**
     * Updates the rule with a valid value. Derived classes must call
     * setValue() in their implementation of this method, because it sets the
     * new valid value for the rule.
     *
     * This is called automatically when needed.
     */
    virtual void update();

public slots:
    /**
     * Marks the rule invalid, causing all dependent rules to be invalid, too.
     */
    void invalidate();

protected:
    ~Rule(); // Counted

    /**
     * Links rules together. This rule will depend on @a dependency; if @a
     * dependency becomes invalid, this rule will likewise become invalid.
     * @a dependency will hold a reference to this rule.
     */
    void dependsOn(Rule const *dependency);

    /**
     * Unlinks rules. This rule will no longer depend on @a dependency.
     * @a dependency will release its reference to this rule.
     */
    void independentOf(Rule const *dependency);

    /**
     * Adds a dependent rule.
     *
     * @param rule  Rule that depends on this rule. A reference to @a rule
     *              is retained.
     */
    void addDependent(Rule *rule);

    /**
     * Removes a dependent rule.
     *
     * @param rule  Rule that depends on this rule. A reference to @a rule
     *              is released.
     */
    void removeDependent(Rule *rule);

    /**
     * Sets the current value of the rule and marks it valid.
     *
     * @param value  New valid value.
     */
    void setValue(float value);

    float cachedValue() const;

#if 0
    /**
     * Called to notify that the dependency @a oldRule has been replaced with
     * @a newRule.
     */
    virtual void dependencyReplaced(Rule const *oldRule, Rule const *newRule);
#endif

signals:
    void valueInvalidated();

#if 0
protected slots:
    void ruleDestroyed(QObject *rule);
#endif

private:
    typedef QSet<Rule *> Dependents;
    Dependents _dependentRules; // ref'd

    /// Current value of the rule.
    float _value;

    /// The value is valid.
    bool _isValid;
};

} // namespace de

#endif // LIBDENG2_RULE_H
