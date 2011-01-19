/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DERIVEDRULE_H
#define DERIVEDRULE_H

#include "constantrule.h"

/**
 * The value of a derived rule is defined by some other rule.
 */
class DerivedRule : public ConstantRule
{
    Q_OBJECT

public:
    explicit DerivedRule(const Rule* source, QObject *parent = 0);

protected:
    void update();
    void dependencyReplaced(const Rule* oldRule, const Rule* newRule);

private:
    const Rule* _source;
};

#endif // DERIVEDRULE_H
