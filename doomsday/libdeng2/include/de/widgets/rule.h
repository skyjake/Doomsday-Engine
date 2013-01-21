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
 *
 * @ingroup widgets
 */
class Rule : public QObject
{
    Q_OBJECT

public:
    explicit Rule(QObject *parent = 0);
    explicit Rule(float initialValue, QObject *parent = 0);
    ~Rule();

    float value() const;

    /**
     * Updates the rule with a valid value. Derived classes must call
     * setValue() in their implementation of this method, because it sets the
     * new valid value for the rule.
     *
     * This is called automatically when needed.
     */
    virtual void update();

    /**
     * Replaces this rule with @a newRule. The dependent rules are updated
     * accordingly. Afterwards, this rule has no more dependencies.
     */
    void replace(Rule *newRule);

protected:
    /**
     * Links rules together. This rule will depend on @a dependency.
     */
    void dependsOn(Rule const *dependency);

    void addDependent(Rule *rule);
    void removeDependent(Rule *rule);
    void setValue(float value);

    float cachedValue() const;

    /**
     * Takes ownership of a rule.
     *
     * @param child  Rule whose ownership should be claimed.
     */
    void claim(Rule *child);

    /**
     * Called to notify that the dependency @a oldRule has been replaced with
     * @a newRule.
     */
    virtual void dependencyReplaced(Rule const *oldRule, Rule const *newRule);

public slots:
    void invalidate();

protected slots:
    void ruleDestroyed(QObject *rule);

signals:
    void valueInvalidated();

private:
    QSet<Rule *> _dependentRules;

    /// Current value of the rule.
    float _value;

    /// The value is valid.
    bool _isValid;
};

} // namespace de

#endif // LIBDENG2_RULE_H
