/*
 * The Doomsday Engine Project
 *
 * Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBCORE_RULE_H
#define LIBCORE_RULE_H

#include "de/libcore.h"
#include "de/counted.h"
#include "de/observers.h"
#include "de/pointerset.h"
#include "de/string.h"

namespace de {

// Declared outside Rule because Rule itself implements the interface.
DE_DECLARE_AUDIENCE(RuleInvalidation, void ruleInvalidated())

/**
 * Rules are used together to evaluate formulas dependent on other rules.
 *
 * - Rules are scalar.
 * - Every rule knows its current value: querying it is a O(1) operation.
 * - Every rule knows where its value comes from / how it's generated.
 * - When the value changes, all dependent rules are notified and marked as invalid.
 * - When a rule is invalid, its current value will be updated (i.e., validated).
 * - Reference counting is used for lifetime management.
 *
 * @ingroup widgets
 */
class DE_PUBLIC Rule : public Counted, public DE_AUDIENCE_INTERFACE(RuleInvalidation)
{
public:
    DE_AUDIENCE_VAR(RuleInvalidation)

    /// Semantic identifiers (e.g., for RuleRectangle).
    enum Semantic {
        Left,
        Top,
        Right,
        Bottom,
        Width,
        Height,
        AnchorX,
        AnchorY,
        MAX_SEMANTICS
    };

    enum { Valid = 0x1, BaseFlagsShift = 4 };

public:
    Rule()
        : _flags(0)
        , _value(0)
    {}

    explicit Rule(float initialValue)
        : _flags(Valid)
        , _value(initialValue)
    {}

    /**
     * Determines the rule's current value. If it has been marked invalid,
     * the value is updated first (see update()).
     */
    float value() const;

    /**
     * Determines the rule's current value (as integer). Otherwise same as value().
     */
    inline int valuei() const { return de::floor(value()); }

    /**
     * Marks the rule invalid, causing all dependent rules to be invalid, too.
     */
    virtual void invalidate();

    /**
     * Updates the rule with a valid value. Derived classes must call
     * setValue() in their implementation of this method, because it sets the
     * new valid value for the rule.
     *
     * This is called automatically when needed.
     */
    virtual void update()
    {
        // This is a fixed value, so don't do anything.
        _flags |= Valid;
    }

    /**
     * Determines if the rule's value is currently valid. A rule becomes
     * invalid if any of its dependencies are invalidated, or invalidate() is
     * called directly on the rule.
     */
    inline bool isValid() const
    {
        return (_flags & Valid) != 0;
    }

    /**
     * Links rules together. This rule will depend on @a dependency; if @a
     * dependency becomes invalid, this rule will likewise become invalid.
     * @a dependency will hold a reference to this rule.
     */
    void dependsOn(const Rule &dependency);

    void dependsOn(const Rule *dependencyOrNull);

    /**
     * Unlinks rules. This rule will no longer depend on @a dependency.
     * @a dependency will release its reference to this rule.
     */
    void independentOf(const Rule &dependency);

    void independentOf(const Rule *dependencyOrNull);

    virtual String description() const = 0;

public:
    /**
     * Clears the flag that determines whether there are any invalid rules.
     * This could, for example, be called after drawing a frame.
     */
    static void markRulesValid();

    /**
     * Determines whether there are invalid rules. If there are invalid rules,
     * it could for example mean that the user interface needs to be redrawn.
     *
     * @return @c true, a rule has been invalidated since the last call to
     * markAllRulesValid().
     */
    static bool invalidRulesExist();

protected:
    ~Rule() override // not public due to being Counted
    {
        DE_ASSERT(_dependencies.isEmpty());
    }

    /**
     * Sets the current value of the rule and marks it valid.
     *
     * @param value  New valid value.
     */
    inline void setValue(float value)
    {
        _value = value;
        _flags |= Valid;
    }

    inline float cachedValue() const
    {
        return _value;
    }

    // Implements IRuleInvalidationObserver.
    void ruleInvalidated() override;

protected:
    int _flags; // Derived rules use this, too.

private:
    PointerSetT<Rule> _dependencies; // ref'd
    float             _value;        // Current value of the rule.

    static bool _invalidRulesExist;
};

} // namespace de

#endif // LIBCORE_RULE_H
