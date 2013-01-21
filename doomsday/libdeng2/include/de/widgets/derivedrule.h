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

#ifndef LIBDENG2_DERIVEDRULE_H
#define LIBDENG2_DERIVEDRULE_H

#include "../ConstantRule"

namespace de {

/**
 * The value of a derived rule is dependent on some other rule. When the
 * source rule is invalidated, the derived rule is likewise invalidated.
 */
class DerivedRule : public ConstantRule
{
    Q_OBJECT

public:
    explicit DerivedRule(Rule const *source, QObject *parent = 0);

protected:
    void update();
    void dependencyReplaced(Rule const *oldRule, Rule const *newRule);

private:
    Rule const *_source;
};

} // namespace de

#endif // LIBDENG2_DERIVEDRULE_H
