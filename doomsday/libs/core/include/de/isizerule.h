/** @file isizerule.h  Interface for objects providing size rules.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_ISIZERULE_H
#define LIBCORE_ISIZERULE_H

#include "de/rule.h"

namespace de {

/**
 * Interface for objects providing size rules.
 * @ingroup widgets
 */
class DE_PUBLIC ISizeRule
{
public:
    virtual ~ISizeRule() = default;

    virtual const Rule &width() const = 0;
    virtual const Rule &height() const = 0;
};

/**
 * Pair of rules that implements the ISizeRule interface.
 * @ingroup widgets
 */
struct DE_PUBLIC RulePair
    : public std::pair<const Rule *, const Rule *>
    , public ISizeRule {
    RulePair(const Rule &a, const Rule &b)
        : std::pair<const Rule *, const Rule *>(&a, &b)
    {}
    // Implements ISizeRule.
    const Rule &width() const override
    {
        DE_ASSERT(first);
        return *first;
    }
    const Rule &height() const override
    {
        DE_ASSERT(second);
        return *second;
    }
};

} // namespace de

#endif // LIBCORE_ISIZERULE_H
